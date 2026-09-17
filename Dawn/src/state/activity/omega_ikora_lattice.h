#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace dawn::state::activity::omega_ikora_lattice {

inline constexpr std::uint32_t kRegistry = 0xD00142CFU;
inline constexpr std::uint8_t kGateType = 23;
inline constexpr std::uint16_t kGateIndex = 16;
inline constexpr std::uint8_t kSceneType = 43;
inline constexpr std::uint16_t kSceneIndex = 1;
/** Raw biased generation on the wire; the decoded signed value is 0x00EC0F96. */
inline constexpr std::uint32_t kSceneGenerationWire = 0x80EC0F96U;
inline constexpr std::uint32_t kReleaseEvent = 0x792AAA50U;
inline constexpr std::size_t kMaximumEvents = 32;
inline constexpr std::size_t kMaximumBodyBits = 1130;
inline constexpr std::size_t kMaximumChunks = (kMaximumBodyBits + 63) / 64;

struct Scope final {
    std::uint32_t registry{};
    std::uint8_t type{};
    std::uint16_t index{};
};

[[nodiscard]] constexpr bool is_gate(Scope scope) noexcept {
    return scope.registry == kRegistry && scope.type == kGateType && scope.index == kGateIndex;
}

struct Scene final {
    std::uint32_t generationWire{};
    std::uint32_t sourceRevision{};
    std::uint32_t revision{};
    std::array<std::uint32_t, kMaximumEvents> events{};
    std::uint8_t eventCount{};
    std::uint8_t stateWire{};
    bool hasDelta{};
    bool flag{};
    bool hasSourceRevision{};
};

namespace detail {
/** Captures are 64-bit MSB-first chunks, with the final short chunk right-aligned. */
class ChunkReader final {
public:
    constexpr ChunkReader(std::span<const std::uint64_t> chunks, std::size_t bits) noexcept
        : chunks_(chunks), bits_(bits) {}

    [[nodiscard]] constexpr bool read(unsigned width, std::uint32_t& value) noexcept {
        if (width > 32 || cursor_ > bits_ || width > bits_ - cursor_) { return false; }
        value = 0;
        for (unsigned i = 0; i < width; ++i, ++cursor_) {
            const auto chunk = cursor_ / 64;
            const auto remaining = bits_ - chunk * 64;
            const auto chunkWidth = remaining < 64 ? remaining : 64;
            const auto shift = chunkWidth - 1 - cursor_ % 64;
            value = (value << 1) | static_cast<std::uint32_t>((chunks_[chunk] >> shift) & 1U);
        }
        return true;
    }

    [[nodiscard]] constexpr bool finished() const noexcept { return cursor_ == bits_; }

private:
    std::span<const std::uint64_t> chunks_;
    std::size_t bits_{};
    std::size_t cursor_{};
};
} // namespace detail

/**
 * Extract the exact Scene 8080626A delta plus its outer raw revision. DADD10 exports
 * only the selector's output-event suffix (indices 5 and 6 in graph 80EC0F95).
 * This function validates shape and scope; transport/run authentication belongs to the caller.
 */
[[nodiscard]] constexpr bool parse_scene_body(Scope scope,
                                             std::span<const std::uint64_t> chunks,
                                             std::size_t bodyBits, Scene& output) noexcept {
    output = {};
    if (scope.registry != kRegistry || scope.type != kSceneType || scope.index != kSceneIndex
        || bodyBits < 33 || bodyBits > kMaximumBodyBits
        || chunks.size() != (bodyBits + 63) / 64) { return false; }
    const auto partial = bodyBits % 64;
    if (partial != 0 && (chunks.back() >> partial) != 0) { return false; }

    detail::ChunkReader reader(chunks, bodyBits);
    Scene parsed{};
    std::uint32_t value{};
    if (!reader.read(1, value)) { return false; }
    parsed.hasDelta = value != 0;
    if (parsed.hasDelta) {
        if (!reader.read(32, parsed.generationWire) || !reader.read(1, value)) { return false; }
        parsed.flag = value != 0;
        if (!reader.read(1, value)) { return false; }
        parsed.hasSourceRevision = value != 0;
        if (parsed.hasSourceRevision && !reader.read(31, parsed.sourceRevision)) { return false; }
        if (!reader.read(2, value)) { return false; }
        parsed.stateWire = static_cast<std::uint8_t>(value);
        if (!reader.read(6, value) || value > kMaximumEvents) { return false; }
        parsed.eventCount = static_cast<std::uint8_t>(value);
        for (std::size_t i = 0; i < parsed.eventCount; ++i) {
            if (!reader.read(32, parsed.events[i])) { return false; }
        }
    }
    if (!reader.read(32, parsed.revision) || !reader.finished()) { return false; }
    output = parsed;
    return true;
}

/** Adapter for the existing SenseObject capture; no dependency on mutable parser/runtime code. */
template<class CapturedObject>
[[nodiscard]] constexpr bool extract_scene(const CapturedObject& object, Scene& output) noexcept {
    output = {};
    if (object.bodyBits < 33 || object.bodyBits > kMaximumBodyBits) { return false; }
    std::array<std::uint64_t, kMaximumChunks> chunks{};
    chunks[0] = object.bodyFirst;
    chunks[1] = object.bodySecond;
    chunks[2] = object.bodyThird;
    const auto count = (static_cast<std::size_t>(object.bodyBits) + 63) / 64;
    if (count > 3 && count - 3 > object.bodyTail.size()) { return false; }
    for (std::size_t i = 3; i < count; ++i) { chunks[i] = object.bodyTail[i - 3]; }
    Scene parsed{};
    if (!parse_scene_body({object.registryKey, object.slotType, object.slotIndex},
                          std::span(chunks).first(count), object.bodyBits, parsed)
        || parsed.revision != object.revision || parsed.hasDelta != object.hasDelta) { return false; }
    output = parsed;
    return true;
}

struct Channel final {
    float value{};
    std::int32_t revision{-1};
    bool snap{};
};

struct Plan final {
    bool active{};
    Channel position{};
    Channel power{1.0F, -1, false};
    Channel lock{0.0F, -1, false};
};

enum class Observation : std::uint8_t { ignored, accepted, released };

/**
 * Reconstructed host edge, not a recovered retail host script: publish the authored locked
 * presentation, then release it on graph 80EC0F95's native 792AAA50 output. No wall clock,
 * position guess, native setter, alternate interior gate, or actor ownership change is involved.
 * Owned by one authenticated activity observation lifetime; reset alongside that owner.
 */
class State final {
public:
    /** Trusted mission lifetime only. Repeated setup in the same run preserves an early release. */
    constexpr void begin(std::uint64_t run) noexcept {
        if (run_ != run) { reset(); run_ = run; }
    }

    constexpr void reset() noexcept { run_ = 0; lastRevision_ = 0; released_ = false; }

    /** `admitted` means exact destination, ready roster, and authenticated opening trigger. */
    [[nodiscard]] constexpr Observation observe(std::uint64_t run, bool admitted,
                                                const Scene& scene) noexcept {
        if (!admitted || run == 0 || run != run_ || !scene.hasDelta
            || scene.generationWire != kSceneGenerationWire
            || scene.revision == 0 || scene.revision <= lastRevision_
            || scene.eventCount > scene.events.size()) { return Observation::ignored; }
        lastRevision_ = scene.revision;
        for (std::size_t i = 0; i < scene.eventCount; ++i) {
            if (!released_ && scene.events[i] == kReleaseEvent) {
                released_ = true;
                return Observation::released;
            }
        }
        return Observation::accepted;
    }

    /** Admission controls publication only: withholding a packet never clears the event latch. */
    [[nodiscard]] constexpr Plan plan(std::uint64_t run, bool admitted) const noexcept {
        Plan result{};
        if (admitted && run != 0 && run == run_) {
            result.active = true;
            result.position = released_ ? Channel{0.0F, 2, false} : Channel{1.0F, 1, true};
        }
        return result;
    }

    [[nodiscard]] constexpr bool released() const noexcept { return released_; }
    [[nodiscard]] constexpr std::uint32_t last_revision() const noexcept { return lastRevision_; }
    [[nodiscard]] constexpr std::uint64_t run() const noexcept { return run_; }

private:
    std::uint64_t run_{};
    std::uint32_t lastRevision_{};
    bool released_{};
};

} // namespace dawn::state::activity::omega_ikora_lattice
