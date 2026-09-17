#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>

namespace dawn::client::hooks::network::lifecycle::sensor_state_heap {

/** Distinct schema: this four-boundary smoke format is not the canonical SHL1 Phase-1 ledger. */
inline constexpr char kSmokeEventName[] = "sensor_heap_smoke";
inline constexpr char kSmokeSchemaName[] = "phase0_v1";
inline constexpr char kSmokeTextPrefix[] = "ev=sensor_heap_smoke schema=phase0_v1";
inline constexpr char kExactHeapAssertText[] =
    "index heap double-free? previous does not point back.";

inline constexpr std::size_t kSha256Bytes = 32U;
using ImageSha256 = std::array<std::byte, kSha256Bytes>;

/** SHA-256 of the pinned packed destiny2.exe artifact, not the unpacked RE image. */
inline constexpr ImageSha256 kPinnedPackedImageSha256{
    std::byte{0x81}, std::byte{0x96}, std::byte{0x43}, std::byte{0x80},
    std::byte{0x66}, std::byte{0x4E}, std::byte{0x7F}, std::byte{0xCE},
    std::byte{0xE3}, std::byte{0xC6}, std::byte{0x20}, std::byte{0x08},
    std::byte{0x5A}, std::byte{0x15}, std::byte{0x7F}, std::byte{0xDE},
    std::byte{0xAF}, std::byte{0x91}, std::byte{0xFE}, std::byte{0xFA},
    std::byte{0xCF}, std::byte{0x72}, std::byte{0x14}, std::byte{0x90},
    std::byte{0x78}, std::byte{0x20}, std::byte{0xF1}, std::byte{0x88},
    std::byte{0xBB}, std::byte{0xEB}, std::byte{0x4C}, std::byte{0xED},
};

inline constexpr std::uintptr_t kSensorTableInsertRva = 0x4D6EB0U;
inline constexpr std::uintptr_t kRecordConstructRva = 0x9FEE40U;
inline constexpr std::uintptr_t kSensorTableRemoveRva = 0x4D7C00U;
inline constexpr std::uintptr_t kRecordDestroyRva = 0x9FE2C0U;

inline constexpr std::array<std::byte, 16U> kSensorTableInsertPrefix{
    std::byte{0x48}, std::byte{0x8B}, std::byte{0xC4}, std::byte{0x57},
    std::byte{0x41}, std::byte{0x56}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x58}, std::byte{0x48}, std::byte{0x89},
    std::byte{0x58}, std::byte{0x10}, std::byte{0x4C}, std::byte{0x8B}};
inline constexpr std::array<std::byte, 16U> kRecordConstructPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x10}, std::byte{0x48}, std::byte{0x89}, std::byte{0x6C},
    std::byte{0x24}, std::byte{0x18}, std::byte{0x56}, std::byte{0x57},
    std::byte{0x41}, std::byte{0x56}, std::byte{0x48}, std::byte{0x83}};
inline constexpr std::array<std::byte, 16U> kSensorTableRemovePrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x08}, std::byte{0x48}, std::byte{0x89}, std::byte{0x6C},
    std::byte{0x24}, std::byte{0x10}, std::byte{0x48}, std::byte{0x89},
    std::byte{0x74}, std::byte{0x24}, std::byte{0x18}, std::byte{0x57}};
inline constexpr std::array<std::byte, 16U> kRecordDestroyPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x08}, std::byte{0x57}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x50}, std::byte{0x0F}, std::byte{0x57},
    std::byte{0xC0}, std::byte{0xC7}, std::byte{0x44}, std::byte{0x24}};

struct ImageView final {
    std::span<const std::byte> mapped{};
    ImageSha256 packedSha256{};
};

struct TargetContract final {
    std::uintptr_t rva{};
    std::span<const std::byte> prefix{};
};

enum class ImageValidationResult : std::uint8_t {
    valid,
    invalidArguments,
    imageIdentityMismatch,
    targetOutOfRange,
    prefixMismatch,
};

/** Validates the packed artifact identity and all mapped prefixes before publishing any address. */
inline ImageValidationResult validate_image(
    const ImageView& image,
    const ImageSha256& expectedSha256,
    std::span<const TargetContract> contracts,
    std::span<std::uintptr_t> outputs) noexcept {
    std::fill(outputs.begin(), outputs.end(), std::uintptr_t{});
    if (image.mapped.empty() || contracts.empty() || contracts.size() != outputs.size()) {
        return ImageValidationResult::invalidArguments;
    }
    if (image.packedSha256 != expectedSha256) {
        return ImageValidationResult::imageIdentityMismatch;
    }
    for (const TargetContract& contract : contracts) {
        if (contract.prefix.empty() || contract.rva > image.mapped.size()
            || contract.prefix.size() > image.mapped.size() - contract.rva) {
            return ImageValidationResult::targetOutOfRange;
        }
        const std::span<const std::byte> observed =
            image.mapped.subspan(contract.rva, contract.prefix.size());
        if (!std::equal(contract.prefix.begin(), contract.prefix.end(), observed.begin())) {
            return ImageValidationResult::prefixMismatch;
        }
    }
    const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(image.mapped.data());
    for (std::size_t index = 0U; index < contracts.size(); ++index) {
        if (contracts[index].rva > (std::numeric_limits<std::uintptr_t>::max)() - base) {
            std::fill(outputs.begin(), outputs.end(), std::uintptr_t{});
            return ImageValidationResult::targetOutOfRange;
        }
        outputs[index] = base + contracts[index].rva;
    }
    return ImageValidationResult::valid;
}

/** Fixed native boundaries observed by the deliberately narrow smoke diagnostic. */
enum class EventKind : std::uint8_t { construct, associate, destroy };
enum class EventPhase : std::uint8_t { enter, exit };
enum class FreezeReason : std::uint8_t { none, exactHeapAssert, lifecycleStop };

/** Public Phase-0 presence bits. Native activity/connection/region/drop generations are absent. */
enum EventValid : std::uint32_t {
    validDiagnosticGeneration = 1U << 0U,
    validRemoveOrdinal = 1U << 1U,
    validRecordGeneration = 1U << 2U,
    validPeerTable = 1U << 3U,
    validDatum = 1U << 4U,
    validConstructorIdentity = 1U << 5U,
    validCurrentSnapshot = 1U << 6U,
    validBaselineSnapshot = 1U << 7U,
};

/** Public Phase-0 flags. They intentionally do not claim the canonical SHL1 v1 meanings. */
enum EventFlag : std::uint32_t {
    flagActiveRecordReuse = 1U << 0U,
    flagReuseAfterUncertainRetire = 1U << 1U,
    flagRelativePayloadChanged = 1U << 2U,
    flagSelectorChanged = 1U << 3U,
    flagCountChanged = 1U << 4U,
    flagPostDestroyNotZero = 1U << 5U,
    flagGuardedReadFailed = 1U << 6U,
};

inline constexpr std::uint32_t kSmokeWarningFlags =
    flagActiveRecordReuse | flagReuseAfterUncertainRetire | flagRelativePayloadChanged
    | flagSelectorChanged | flagCountChanged | flagPostDestroyNotZero | flagGuardedReadFailed;

static_assert(validDiagnosticGeneration == (1U << 0U));
static_assert(validRemoveOrdinal == (1U << 1U));
static_assert(validRecordGeneration == (1U << 2U));
static_assert(validPeerTable == (1U << 3U));
static_assert(validDatum == (1U << 4U));
static_assert(validConstructorIdentity == (1U << 5U));
static_assert(validCurrentSnapshot == (1U << 6U));
static_assert(validBaselineSnapshot == (1U << 7U));
static_assert(flagActiveRecordReuse == (1U << 0U));
static_assert(flagReuseAfterUncertainRetire == (1U << 1U));
static_assert(flagRelativePayloadChanged == (1U << 2U));
static_assert(flagSelectorChanged == (1U << 3U));
static_assert(flagCountChanged == (1U << 4U));
static_assert(flagPostDestroyNotZero == (1U << 5U));
static_assert(flagGuardedReadFailed == (1U << 6U));
static_assert((kSmokeWarningFlags & flagCountChanged) != 0U);

struct SensorIdentity final {
    std::uint32_t word{};
    std::uint8_t kind{};
    std::uint16_t index{};
    friend constexpr bool operator==(SensorIdentity, SensorIdentity) noexcept = default;
};

/** Packs only u32@0, u8@4 and u16@6; source padding byte 5 is deterministically zero. */
[[nodiscard]] constexpr std::uint64_t pack_sensor_identity(SensorIdentity identity) noexcept {
    return static_cast<std::uint64_t>(identity.word)
           | static_cast<std::uint64_t>(identity.kind) << 32U
           | static_cast<std::uint64_t>(identity.index) << 48U;
}

[[nodiscard]] inline bool exact_heap_assert(const char* text) noexcept {
    if (text == nullptr) {
        return false;
    }
    for (std::size_t index = 0U; index < sizeof kExactHeapAssertText; ++index) {
        if (text[index] != kExactHeapAssertText[index]) {
            return false;
        }
    }
    return true;
}

struct ReceivedSenseSnapshot final {
    std::uint32_t count{};
    std::uint64_t relativePayload{};
    std::uint16_t selector{};
    friend constexpr bool operator==(ReceivedSenseSnapshot,
                                     ReceivedSenseSnapshot) noexcept = default;
};

struct EventContext final {
    /** Local probe-install token; it is not a native network or connection generation. */
    std::uint64_t diagnosticGeneration{};
    /** Local +4D7C00 call ordinal; it is not a native reset/drop generation. */
    std::uint64_t removeOrdinal{};
    std::uint32_t recordGeneration{};
    std::uintptr_t peerTable{};
    std::uint32_t datum{};
    friend constexpr bool operator==(EventContext, EventContext) noexcept = default;
};

struct Event final {
    EventContext context{};
    std::uint64_t sequence{};
    std::uint64_t tick{};
    std::int64_t qpc{};
    std::uint32_t threadId{};
    std::uint32_t valid{};
    std::uint32_t flags{};
    std::uintptr_t record{};
    SensorIdentity identity{};
    std::uint64_t sensorKey{};
    std::uint32_t authSchema{};
    std::uint32_t senseSchema{};
    ReceivedSenseSnapshot current{};
    ReceivedSenseSnapshot baseline{};
    EventKind kind{};
    EventPhase phase{};
};

enum class PushResult : std::uint8_t {
    committed,
    invalid,
    staleDiagnosticGeneration,
    frozen,
    full,
    busy,
    sequenceExhausted,
};
enum class PopResult : std::uint8_t { success, empty, busy };

struct RingCounters final {
    std::uint64_t committed{};
    std::uint64_t invalid{};
    std::uint64_t staleDiagnosticGeneration{};
    std::uint64_t droppedFrozen{};
    std::uint64_t droppedFull{};
    std::uint64_t droppedBusy{};
    std::uint64_t droppedSequenceExhausted{};
    std::uint64_t pending{};
    std::uint64_t highWater{};
    std::uint64_t lastCommittedSequence{};
    std::uint64_t frozenSequence{};
    [[nodiscard]] constexpr std::uint64_t losses() const noexcept {
        return invalid + staleDiagnosticGeneration + droppedFrozen + droppedFull + droppedBusy
               + droppedSequenceExhausted;
    }
};

template <std::size_t Capacity>
class FixedRing final {
    static_assert(Capacity != 0U);

public:
    FixedRing() noexcept = default;
    FixedRing(const FixedRing&) = delete;
    FixedRing& operator=(const FixedRing&) = delete;

    [[nodiscard]] bool reset(std::uint64_t diagnosticGeneration) noexcept {
        if (diagnosticGeneration == 0U) {
            return false;
        }
        RingLock lock{*this};
        if (!lock) {
            return false;
        }
        records_ = {};
        head_ = 0U;
        count_ = 0U;
        nextSequence_ = 1U;
        diagnosticGeneration_.store(diagnosticGeneration, std::memory_order_release);
        frozenReason_.store(FreezeReason::none, std::memory_order_release);
        committed_.store(0U, std::memory_order_relaxed);
        invalid_.store(0U, std::memory_order_relaxed);
        staleDiagnosticGeneration_.store(0U, std::memory_order_relaxed);
        droppedFrozen_.store(0U, std::memory_order_relaxed);
        droppedFull_.store(0U, std::memory_order_relaxed);
        droppedBusy_.store(0U, std::memory_order_relaxed);
        droppedSequenceExhausted_.store(0U, std::memory_order_relaxed);
        pending_.store(0U, std::memory_order_relaxed);
        highWater_.store(0U, std::memory_order_relaxed);
        lastCommittedSequence_.store(0U, std::memory_order_relaxed);
        frozenSequence_.store(0U, std::memory_order_relaxed);
        return true;
    }

    [[nodiscard]] PushResult try_push(const Event& event) noexcept {
        if (event.context.diagnosticGeneration == 0U || event.context.recordGeneration == 0U
            || event.record == 0U
            || (event.valid & (validDiagnosticGeneration | validRecordGeneration))
                   != (validDiagnosticGeneration | validRecordGeneration)) {
            invalid_.fetch_add(1U, std::memory_order_relaxed);
            return PushResult::invalid;
        }
        if (event.context.diagnosticGeneration
            != diagnosticGeneration_.load(std::memory_order_acquire)) {
            staleDiagnosticGeneration_.fetch_add(1U, std::memory_order_relaxed);
            return PushResult::staleDiagnosticGeneration;
        }
        if (frozenReason_.load(std::memory_order_acquire) != FreezeReason::none) {
            droppedFrozen_.fetch_add(1U, std::memory_order_relaxed);
            return PushResult::frozen;
        }
        RingLock lock{*this};
        if (!lock) {
            droppedBusy_.fetch_add(1U, std::memory_order_relaxed);
            return PushResult::busy;
        }
        if (frozenReason_.load(std::memory_order_acquire) != FreezeReason::none) {
            droppedFrozen_.fetch_add(1U, std::memory_order_relaxed);
            return PushResult::frozen;
        }
        if (count_ == records_.size()) {
            droppedFull_.fetch_add(1U, std::memory_order_relaxed);
            return PushResult::full;
        }
        if (nextSequence_ == (std::numeric_limits<std::uint64_t>::max)()) {
            droppedSequenceExhausted_.fetch_add(1U, std::memory_order_relaxed);
            return PushResult::sequenceExhausted;
        }

        Event committed = event;
        committed.sequence = nextSequence_++;
        records_[(head_ + count_) % records_.size()] = committed;
        ++count_;
        committed_.fetch_add(1U, std::memory_order_relaxed);
        pending_.store(count_, std::memory_order_relaxed);
        if (count_ > highWater_.load(std::memory_order_relaxed)) {
            highWater_.store(count_, std::memory_order_relaxed);
        }
        lastCommittedSequence_.store(committed.sequence, std::memory_order_release);
        return PushResult::committed;
    }

    [[nodiscard]] PopResult try_pop(Event& output) noexcept {
        RingLock lock{*this};
        if (!lock) {
            return PopResult::busy;
        }
        if (count_ == 0U) {
            return PopResult::empty;
        }
        const Event event = records_[head_];
        head_ = (head_ + 1U) % records_.size();
        --count_;
        pending_.store(count_, std::memory_order_relaxed);
        output = event;
        return PopResult::success;
    }

    [[nodiscard]] bool freeze(FreezeReason reason) noexcept {
        if (reason == FreezeReason::none) {
            return false;
        }
        FreezeReason expected = FreezeReason::none;
        if (!frozenReason_.compare_exchange_strong(
                expected, reason, std::memory_order_acq_rel, std::memory_order_acquire)) {
            return false;
        }
        frozenSequence_.store(lastCommittedSequence_.load(std::memory_order_acquire),
                              std::memory_order_release);
        return true;
    }

    [[nodiscard]] FreezeReason frozen_reason() const noexcept {
        return frozenReason_.load(std::memory_order_acquire);
    }
    [[nodiscard]] std::uint64_t diagnostic_generation() const noexcept {
        return diagnosticGeneration_.load(std::memory_order_acquire);
    }
    [[nodiscard]] RingCounters counters() const noexcept {
        return RingCounters{
            committed_.load(std::memory_order_relaxed),
            invalid_.load(std::memory_order_relaxed),
            staleDiagnosticGeneration_.load(std::memory_order_relaxed),
            droppedFrozen_.load(std::memory_order_relaxed),
            droppedFull_.load(std::memory_order_relaxed),
            droppedBusy_.load(std::memory_order_relaxed),
            droppedSequenceExhausted_.load(std::memory_order_relaxed),
            pending_.load(std::memory_order_relaxed),
            highWater_.load(std::memory_order_relaxed),
            lastCommittedSequence_.load(std::memory_order_relaxed),
            frozenSequence_.load(std::memory_order_relaxed),
        };
    }

#if defined(DAWN_SENSOR_HEAP_TEST)
    [[nodiscard]] bool testing_lock() noexcept { return try_lock(); }
    void testing_unlock() noexcept { unlock(); }
    void testing_set_next_sequence(std::uint64_t sequence) noexcept { nextSequence_ = sequence; }
#endif

private:
    class RingLock final {
    public:
        explicit RingLock(FixedRing& ring) noexcept
            : ring_(ring), locked_(ring_.try_lock()) {}
        ~RingLock() {
            if (locked_) {
                ring_.unlock();
            }
        }
        RingLock(const RingLock&) = delete;
        RingLock& operator=(const RingLock&) = delete;
        [[nodiscard]] explicit operator bool() const noexcept { return locked_; }

    private:
        FixedRing& ring_;
        bool locked_{};
    };

    [[nodiscard]] bool try_lock() noexcept {
        return !lock_.test_and_set(std::memory_order_acquire);
    }
    void unlock() noexcept { lock_.clear(std::memory_order_release); }

    std::array<Event, Capacity> records_{};
    std::size_t head_{};
    std::size_t count_{};
    std::uint64_t nextSequence_{1U};
    std::atomic_flag lock_ = ATOMIC_FLAG_INIT;
    std::atomic<std::uint64_t> diagnosticGeneration_{};
    std::atomic<FreezeReason> frozenReason_{FreezeReason::none};
    std::atomic<std::uint64_t> committed_{};
    std::atomic<std::uint64_t> invalid_{};
    std::atomic<std::uint64_t> staleDiagnosticGeneration_{};
    std::atomic<std::uint64_t> droppedFrozen_{};
    std::atomic<std::uint64_t> droppedFull_{};
    std::atomic<std::uint64_t> droppedBusy_{};
    std::atomic<std::uint64_t> droppedSequenceExhausted_{};
    std::atomic<std::uint64_t> pending_{};
    std::atomic<std::uint64_t> highWater_{};
    std::atomic<std::uint64_t> lastCommittedSequence_{};
    std::atomic<std::uint64_t> frozenSequence_{};
};

struct RecordMetadata final {
    std::uintptr_t peerTable{};
    SensorIdentity identity{};
    std::uint32_t authSchema{};
    std::uint32_t senseSchema{};
    std::uint32_t datum{};
    ReceivedSenseSnapshot baseline{};
    bool identityValid{};
    bool baselineValid{};
    bool peerTableValid{};
    bool datumValid{};
};

struct RegistryToken final {
    std::uintptr_t record{};
    std::uint32_t generation{};
    bool activeReuse{};
    bool reuseAfterUncertainRetire{};
    [[nodiscard]] explicit operator bool() const noexcept {
        return record > 1U && generation != 0U;
    }
};

enum class RegistryResult : std::uint8_t {
    success, invalid, busy, full, generationExhausted, stale, missing
};

struct RegistryView final {
    RegistryToken token{};
    RecordMetadata metadata{};
    [[nodiscard]] explicit operator bool() const noexcept { return static_cast<bool>(token); }
};

struct RegistryCounters final {
    std::uint64_t begun{};
    std::uint64_t retired{};
    std::uint64_t invalid{};
    std::uint64_t beginBusy{};
    std::uint64_t updateBusy{};
    std::uint64_t snapshotBusy{};
    std::uint64_t retireBusy{};
    std::uint64_t full{};
    std::uint64_t generationExhausted{};
    std::uint64_t staleUpdate{};
    std::uint64_t staleRetire{};
    std::uint64_t missingSnapshot{};
    std::uint64_t activeReuse{};
    std::uint64_t uncertainReuse{};
    std::uint64_t highWater{};
    [[nodiscard]] constexpr std::uint64_t losses() const noexcept {
        return invalid + beginBusy + updateBusy + snapshotBusy + retireBusy + full
               + generationExhausted + staleUpdate + staleRetire + missingSnapshot
               + uncertainReuse;
    }
};

/** Per-slot non-waiting registry. Keys are never erased, so inactive generations remain visible. */
template <std::size_t Capacity>
class FixedRegistry final {
    static_assert(Capacity != 0U);
    static constexpr std::uintptr_t kReservedKey = 1U;

    struct Slot final {
        std::atomic<std::uintptr_t> key{};
        std::atomic_flag writeLock = ATOMIC_FLAG_INIT;
        std::atomic_bool retirementUncertain{};
        std::uint32_t generation{};
        bool active{};
        RecordMetadata metadata{};
    };

public:
    FixedRegistry() noexcept = default;
    FixedRegistry(const FixedRegistry&) = delete;
    FixedRegistry& operator=(const FixedRegistry&) = delete;

    /** Detached/off-hook reset only. */
    void reset() noexcept {
        for (Slot& slot : slots_) {
            slot.key.store(0U, std::memory_order_relaxed);
            slot.writeLock.clear(std::memory_order_relaxed);
            slot.retirementUncertain.store(false, std::memory_order_relaxed);
            slot.generation = 0U;
            slot.active = false;
            slot.metadata = {};
        }
        begun_.store(0U, std::memory_order_relaxed);
        retired_.store(0U, std::memory_order_relaxed);
        invalid_.store(0U, std::memory_order_relaxed);
        beginBusy_.store(0U, std::memory_order_relaxed);
        updateBusy_.store(0U, std::memory_order_relaxed);
        snapshotBusy_.store(0U, std::memory_order_relaxed);
        retireBusy_.store(0U, std::memory_order_relaxed);
        full_.store(0U, std::memory_order_relaxed);
        generationExhausted_.store(0U, std::memory_order_relaxed);
        staleUpdate_.store(0U, std::memory_order_relaxed);
        staleRetire_.store(0U, std::memory_order_relaxed);
        missingSnapshot_.store(0U, std::memory_order_relaxed);
        activeReuse_.store(0U, std::memory_order_relaxed);
        uncertainReuse_.store(0U, std::memory_order_relaxed);
        keysClaimed_.store(0U, std::memory_order_relaxed);
    }

    [[nodiscard]] RegistryResult try_begin(std::uintptr_t record,
                                           const RecordMetadata& metadata,
                                           RegistryToken& output) noexcept {
        output = {};
        if (record <= kReservedKey) {
            invalid_.fetch_add(1U, std::memory_order_relaxed);
            return RegistryResult::invalid;
        }
        const std::size_t first = start(record);
        for (std::size_t offset = 0U; offset < slots_.size(); ++offset) {
            Slot& slot = slots_[(first + offset) % slots_.size()];
            const std::uintptr_t key = slot.key.load(std::memory_order_acquire);
            if (key == 0U) {
                std::uintptr_t empty = 0U;
                if (!slot.key.compare_exchange_strong(empty,
                                                      kReservedKey,
                                                      std::memory_order_acq_rel,
                                                      std::memory_order_acquire)) {
                    beginBusy_.fetch_add(1U, std::memory_order_relaxed);
                    return RegistryResult::busy;
                }
                slot.generation = 1U;
                slot.active = true;
                slot.metadata = metadata;
                slot.retirementUncertain.store(false, std::memory_order_relaxed);
                slot.key.store(record, std::memory_order_release);
                keysClaimed_.fetch_add(1U, std::memory_order_relaxed);
                begun_.fetch_add(1U, std::memory_order_relaxed);
                output = RegistryToken{record, 1U, false, false};
                return RegistryResult::success;
            }
            if (key == kReservedKey) {
                beginBusy_.fetch_add(1U, std::memory_order_relaxed);
                return RegistryResult::busy;
            }
            if (key != record) {
                continue;
            }
            SlotLock lock{slot};
            if (!lock) {
                beginBusy_.fetch_add(1U, std::memory_order_relaxed);
                return RegistryResult::busy;
            }
            if (slot.key.load(std::memory_order_acquire) != record) {
                beginBusy_.fetch_add(1U, std::memory_order_relaxed);
                return RegistryResult::busy;
            }
            if (slot.generation == (std::numeric_limits<std::uint32_t>::max)()) {
                generationExhausted_.fetch_add(1U, std::memory_order_relaxed);
                return RegistryResult::generationExhausted;
            }
            const bool activeReuse = slot.active
                                     && !slot.retirementUncertain.load(std::memory_order_acquire);
            const bool uncertainReuse = slot.active && !activeReuse;
            if (activeReuse) {
                activeReuse_.fetch_add(1U, std::memory_order_relaxed);
            }
            if (uncertainReuse) {
                uncertainReuse_.fetch_add(1U, std::memory_order_relaxed);
            }
            ++slot.generation;
            slot.active = true;
            slot.metadata = metadata;
            // Only a confirmed retirement clears uncertainty. A concurrent begin must not erase
            // evidence that an earlier retirement attempt failed to acquire this slot.
            begun_.fetch_add(1U, std::memory_order_relaxed);
            output = RegistryToken{record, slot.generation, activeReuse, uncertainReuse};
            return RegistryResult::success;
        }
        full_.fetch_add(1U, std::memory_order_relaxed);
        return RegistryResult::full;
    }

    [[nodiscard]] RegistryResult try_set_baseline(RegistryToken token,
                                                   ReceivedSenseSnapshot baseline,
                                                   bool valid) noexcept {
        return update(token, [&](RecordMetadata& metadata) noexcept {
            metadata.baseline = baseline;
            metadata.baselineValid = valid;
        });
    }

    [[nodiscard]] RegistryResult try_associate(RegistryToken token,
                                               std::uintptr_t peerTable,
                                               std::uint32_t datum,
                                               bool datumValid) noexcept {
        return update(token, [&](RecordMetadata& metadata) noexcept {
            metadata.peerTable = peerTable;
            metadata.peerTableValid = peerTable != 0U;
            metadata.datum = datum;
            metadata.datumValid = datumValid;
        });
    }

    [[nodiscard]] RegistryResult try_snapshot(std::uintptr_t record,
                                              RegistryView& output) noexcept {
        output = {};
        Slot* const slot = find(record);
        if (slot == nullptr) {
            missingSnapshot_.fetch_add(1U, std::memory_order_relaxed);
            return RegistryResult::missing;
        }
        SlotLock lock{*slot};
        if (!lock) {
            snapshotBusy_.fetch_add(1U, std::memory_order_relaxed);
            return RegistryResult::busy;
        }
        if (slot->key.load(std::memory_order_acquire) != record || !slot->active
            || slot->generation == 0U) {
            missingSnapshot_.fetch_add(1U, std::memory_order_relaxed);
            return RegistryResult::missing;
        }
        output = RegistryView{RegistryToken{record, slot->generation, false, false},
                              slot->metadata};
        return RegistryResult::success;
    }

    /** Prevents a later failed observation from being promoted to certain active-reuse evidence. */
    void mark_retirement_uncertain(std::uintptr_t record) noexcept {
        Slot* const slot = find(record);
        if (slot != nullptr) {
            slot->retirementUncertain.store(true, std::memory_order_release);
        }
    }

    [[nodiscard]] RegistryResult try_retire(RegistryToken token) noexcept {
        Slot* const slot = find(token.record);
        if (slot == nullptr) {
            staleRetire_.fetch_add(1U, std::memory_order_relaxed);
            return RegistryResult::stale;
        }
        SlotLock lock{*slot};
        if (!lock) {
            slot->retirementUncertain.store(true, std::memory_order_release);
            retireBusy_.fetch_add(1U, std::memory_order_relaxed);
            return RegistryResult::busy;
        }
        if (slot->key.load(std::memory_order_acquire) != token.record
            || slot->generation != token.generation || !slot->active) {
            staleRetire_.fetch_add(1U, std::memory_order_relaxed);
            return RegistryResult::stale;
        }
        slot->active = false;
        slot->retirementUncertain.store(false, std::memory_order_release);
        retired_.fetch_add(1U, std::memory_order_relaxed);
        return RegistryResult::success;
    }

    [[nodiscard]] RegistryCounters counters() const noexcept {
        return RegistryCounters{
            begun_.load(std::memory_order_relaxed),
            retired_.load(std::memory_order_relaxed),
            invalid_.load(std::memory_order_relaxed),
            beginBusy_.load(std::memory_order_relaxed),
            updateBusy_.load(std::memory_order_relaxed),
            snapshotBusy_.load(std::memory_order_relaxed),
            retireBusy_.load(std::memory_order_relaxed),
            full_.load(std::memory_order_relaxed),
            generationExhausted_.load(std::memory_order_relaxed),
            staleUpdate_.load(std::memory_order_relaxed),
            staleRetire_.load(std::memory_order_relaxed),
            missingSnapshot_.load(std::memory_order_relaxed),
            activeReuse_.load(std::memory_order_relaxed),
            uncertainReuse_.load(std::memory_order_relaxed),
            keysClaimed_.load(std::memory_order_relaxed),
        };
    }

#if defined(DAWN_SENSOR_HEAP_TEST)
    [[nodiscard]] bool testing_lock(std::uintptr_t record) noexcept {
        Slot* const slot = find(record);
        return slot != nullptr && !slot->writeLock.test_and_set(std::memory_order_acquire);
    }
    void testing_unlock(std::uintptr_t record) noexcept {
        Slot* const slot = find(record);
        if (slot != nullptr) {
            slot->writeLock.clear(std::memory_order_release);
        }
    }
    void testing_set_generation(std::uintptr_t record, std::uint32_t generation) noexcept {
        Slot* const slot = find(record);
        if (slot != nullptr) {
            slot->generation = generation;
        }
    }
#endif

private:
    class SlotLock final {
    public:
        explicit SlotLock(Slot& slot) noexcept
            : slot_(slot), locked_(!slot_.writeLock.test_and_set(std::memory_order_acquire)) {}
        ~SlotLock() {
            if (locked_) {
                slot_.writeLock.clear(std::memory_order_release);
            }
        }
        SlotLock(const SlotLock&) = delete;
        SlotLock& operator=(const SlotLock&) = delete;
        [[nodiscard]] explicit operator bool() const noexcept { return locked_; }

    private:
        Slot& slot_;
        bool locked_{};
    };

    [[nodiscard]] static std::size_t start(std::uintptr_t record) noexcept {
        return (record >> 4U) % Capacity;
    }
    [[nodiscard]] Slot* find(std::uintptr_t record) noexcept {
        if (record <= kReservedKey) {
            return nullptr;
        }
        const std::size_t first = start(record);
        for (std::size_t offset = 0U; offset < slots_.size(); ++offset) {
            Slot& slot = slots_[(first + offset) % slots_.size()];
            const std::uintptr_t key = slot.key.load(std::memory_order_acquire);
            if (key == record) {
                return &slot;
            }
            if (key == 0U) {
                return nullptr;
            }
        }
        return nullptr;
    }

    template <typename Update>
    [[nodiscard]] RegistryResult update(RegistryToken token, Update&& updateMetadata) noexcept {
        Slot* const slot = find(token.record);
        if (slot == nullptr) {
            staleUpdate_.fetch_add(1U, std::memory_order_relaxed);
            return RegistryResult::stale;
        }
        SlotLock lock{*slot};
        if (!lock) {
            updateBusy_.fetch_add(1U, std::memory_order_relaxed);
            return RegistryResult::busy;
        }
        if (slot->key.load(std::memory_order_acquire) != token.record
            || slot->generation != token.generation || !slot->active) {
            staleUpdate_.fetch_add(1U, std::memory_order_relaxed);
            return RegistryResult::stale;
        }
        updateMetadata(slot->metadata);
        return RegistryResult::success;
    }

    std::array<Slot, Capacity> slots_{};
    std::atomic<std::uint64_t> begun_{};
    std::atomic<std::uint64_t> retired_{};
    std::atomic<std::uint64_t> invalid_{};
    std::atomic<std::uint64_t> beginBusy_{};
    std::atomic<std::uint64_t> updateBusy_{};
    std::atomic<std::uint64_t> snapshotBusy_{};
    std::atomic<std::uint64_t> retireBusy_{};
    std::atomic<std::uint64_t> full_{};
    std::atomic<std::uint64_t> generationExhausted_{};
    std::atomic<std::uint64_t> staleUpdate_{};
    std::atomic<std::uint64_t> staleRetire_{};
    std::atomic<std::uint64_t> missingSnapshot_{};
    std::atomic<std::uint64_t> activeReuse_{};
    std::atomic<std::uint64_t> uncertainReuse_{};
    std::atomic<std::uint64_t> keysClaimed_{};
};

static_assert(std::is_trivially_copyable_v<SensorIdentity>);
static_assert(std::is_trivially_copyable_v<ReceivedSenseSnapshot>);
static_assert(std::is_trivially_copyable_v<EventContext>);
static_assert(std::is_trivially_copyable_v<Event>);
static_assert(std::atomic<std::uint64_t>::is_always_lock_free,
              "Smoke callbacks require lock-free 64-bit counters");
static_assert(std::atomic<std::uintptr_t>::is_always_lock_free,
              "Smoke registry requires lock-free native keys");

} // namespace dawn::client::hooks::network::lifecycle::sensor_state_heap
