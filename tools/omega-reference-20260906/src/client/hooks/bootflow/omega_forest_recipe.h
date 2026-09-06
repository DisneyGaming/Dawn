#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>

namespace sunrise::client::hooks::bootflow::omega_forest {

inline constexpr std::uint32_t kForestD = 0x80F4E6E7U;
inline constexpr std::uint32_t kWorkerDefinitionClass = 0x80804FECU;
inline constexpr std::size_t kWorkerPrefixSize = 0x9BDU;

template <typename T>
[[nodiscard]] inline T read(std::span<const std::byte> bytes, std::size_t offset) noexcept {
    T value{};
    std::memcpy(&value, bytes.data() + offset, sizeof value);
    return value;
}

/** Exact live-validated worker family; other forest segments retain native behavior. */
[[nodiscard]] inline bool matches(std::span<const std::byte> worker,
                                  std::string_view package) noexcept {
    return package == "mission_scot" && worker.size() >= kWorkerPrefixSize
           && read<std::uint32_t>(worker, 4U) == kWorkerDefinitionClass
           && read<std::uint32_t>(worker, 0x96CU) == kForestD;
}

/** Caller serializes access. Identity is the mission run, never a movable worker pointer. */
struct RunSeed {
    std::uint64_t generation{};
    std::uint32_t value{};

    [[nodiscard]] bool needs_seed(std::uint64_t run) const noexcept {
        return generation != run || value == 0U;
    }

    [[nodiscard]] std::uint32_t select(std::uint64_t run, std::uint32_t candidate) noexcept {
        if (needs_seed(run)) {
            candidate &= 0x7FFFFFFFU;
            if (candidate == 0U) { candidate = 1U; }
            if (candidate == value) { candidate = candidate == 0x7FFFFFFFU ? 1U : candidate + 1U; }
            value = candidate;
            generation = run;
        }
        return value;
    }
};

/** Retains native seed masking and fallback anchors; never writes effective state or progress. */
inline void prepare_worker(std::span<std::byte> worker, std::uint32_t seed) noexcept {
    const std::uint32_t mask = 0U;
    std::memcpy(worker.data() + 0x948U, &mask, sizeof mask);
    std::memcpy(worker.data() + 0x94CU, &seed, sizeof seed);
    struct Anchor {
        std::size_t a, b, weight, active;
        std::int8_t column, height;
        float progress;
        bool on;
    };
    constexpr std::array<Anchor, 4> anchors{{
        {0x970U, 0x971U, 0x974U, 0x978U, -1, 2, 0.0F, false},
        {0x979U, 0x97AU, 0x97CU, 0x980U, -1, 0, 0.0F, false},
        {0x981U, 0x982U, 0x984U, 0x988U,  2, 0, 0.0F, true},
        {0x989U, 0x98AU, 0x98CU, 0x990U,  1, 2, 1.0F, false},
    }};
    // FF2F80 interpolates these values and stores their maximum at worker+89C.
    // FF6830 chooses the navigation goal by equality with that maximum. Equal endpoints
    // make every island a goal candidate; the last matching area wins in allocation order.
    for (const auto& a : anchors) {
        std::memcpy(worker.data() + a.a, &a.column, sizeof a.column);
        std::memcpy(worker.data() + a.b, &a.height, sizeof a.height);
        std::memcpy(worker.data() + a.weight, &a.progress, sizeof a.progress);
        worker[a.active] = a.on ? std::byte{1} : std::byte{0};
    }
}

/** Both inputs feed several topology passes; zero both to retain only the anchor route. */
inline void solver_inputs(bool scoped, bool diagnosticOverride, float& first, float& second) noexcept {
    if (scoped && !diagnosticOverride) {
        first = 0.0F;
        second = 0.0F;
    }
}

} // namespace sunrise::client::hooks::bootflow::omega_forest
