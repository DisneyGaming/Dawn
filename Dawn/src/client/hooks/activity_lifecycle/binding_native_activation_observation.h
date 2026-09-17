#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>
#include <utility>

#include "../../../state/activity/lifecycle_generation.h"
#include "native_activation_global_drop_fanout.h"
#include "native_activation_registry.h"
#include "native_activation_validation.h"

namespace dawn::client::hooks::activity_lifecycle::binding_native_observation {

inline constexpr std::size_t kService7TaggedBytes = 0x89U;
inline constexpr std::size_t kService7BodyBytes = 0x88U;
inline constexpr std::size_t kService7SessionBytes = 8U;
inline constexpr std::uintptr_t kActivityClientResponseOffset = 0x15E8U;
inline constexpr std::size_t kHostObservationSlotCount = 64U;
inline constexpr std::size_t kNativeObservationSlotCount = 128U;
inline constexpr std::size_t kHostObservationQueueCapacity = 256U;
inline constexpr std::size_t kNativeObservationQueueCapacity = 256U;

using Service7TaggedResponse = std::array<std::byte, kService7TaggedBytes>;
using Service7Body = std::array<std::byte, kService7BodyBytes>;
using SessionOctets = std::array<std::byte, kService7SessionBytes>;

namespace contract {

inline constexpr std::uintptr_t kResponseHandleInternalRva = 0xE02280U;
inline constexpr std::uintptr_t kService7ThunkRva = 0xB536A0U;
inline constexpr std::uintptr_t kService7OwnerRva = 0xB536B0U;
inline constexpr std::uintptr_t kGenericDispatcherRva = 0x4F7DF0U;
inline constexpr std::uintptr_t kKind10CallbackRva = 0x4F2EE0U;
inline constexpr std::uintptr_t kValidateStoreRva = 0x4F6880U;
inline constexpr std::uintptr_t kBodyAccessorRva = 0x4F5B70U;
inline constexpr std::uintptr_t kLaterStateStepRva = 0xC0EA50U;
inline constexpr std::uintptr_t kLaterSessionLoadRva = 0xC0F68DU;
inline constexpr std::uintptr_t kPublishSecondQwordRva = 0x17AB860U;
inline constexpr std::uintptr_t kPublishFirstQwordRva = 0x17ABA30U;
inline constexpr std::uintptr_t kColdAllocationRva = 0xBFE450U;

inline constexpr std::array<std::byte, 12U> kResponseHandleInternalPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x18}, std::byte{0x55}, std::byte{0x56}, std::byte{0x57},
    std::byte{0x41}, std::byte{0x54}, std::byte{0x41}, std::byte{0x55},
};
inline constexpr std::array<std::byte, 9U> kService7ThunkPrefix{
    std::byte{0x48}, std::byte{0x8B}, std::byte{0x49}, std::byte{0x08},
    std::byte{0xE9}, std::byte{0x07}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00},
};
inline constexpr std::array<std::byte, 16U> kService7OwnerPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x18}, std::byte{0x57}, std::byte{0x48}, std::byte{0x81},
    std::byte{0xEC}, std::byte{0xD0}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x48}, std::byte{0x8B}, std::byte{0x05},
};
inline constexpr std::array<std::byte, 22U> kGenericDispatcherPrefix{
    std::byte{0x4C}, std::byte{0x8B}, std::byte{0xDC}, std::byte{0x55},
    std::byte{0x41}, std::byte{0x55}, std::byte{0x41}, std::byte{0x57},
    std::byte{0x49}, std::byte{0x8D}, std::byte{0xAB}, std::byte{0x48},
    std::byte{0xFD}, std::byte{0xFF}, std::byte{0xFF}, std::byte{0x48},
    std::byte{0x81}, std::byte{0xEC}, std::byte{0xA0}, std::byte{0x03},
    std::byte{0x00}, std::byte{0x00},
};
inline constexpr std::array<std::byte, 20U> kKind10CallbackPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x08}, std::byte{0x57}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x20}, std::byte{0x48}, std::byte{0x8B},
    std::byte{0x01}, std::byte{0x48}, std::byte{0x8B}, std::byte{0xDA},
    std::byte{0x48}, std::byte{0x8B}, std::byte{0xF9},
};
inline constexpr std::array<std::byte, 24U> kValidateStorePrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x18}, std::byte{0x55}, std::byte{0x56}, std::byte{0x57},
    std::byte{0x41}, std::byte{0x54}, std::byte{0x41}, std::byte{0x55},
    std::byte{0x41}, std::byte{0x56}, std::byte{0x41}, std::byte{0x57},
    std::byte{0x48}, std::byte{0x8D}, std::byte{0xAC}, std::byte{0x24},
    std::byte{0x20}, std::byte{0xFA}, std::byte{0xFF}, std::byte{0xFF},
};
inline constexpr std::array<std::byte, 8U> kBodyAccessorPrefix{
    std::byte{0x48}, std::byte{0x8D}, std::byte{0x81}, std::byte{0xE8},
    std::byte{0x15}, std::byte{0x00}, std::byte{0x00}, std::byte{0xC3},
};
inline constexpr std::array<std::byte, 24U> kLaterStateStepPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x18}, std::byte{0x55}, std::byte{0x56}, std::byte{0x57},
    std::byte{0x41}, std::byte{0x54}, std::byte{0x41}, std::byte{0x55},
    std::byte{0x41}, std::byte{0x56}, std::byte{0x41}, std::byte{0x57},
    std::byte{0x48}, std::byte{0x8D}, std::byte{0xAC}, std::byte{0x24},
    std::byte{0xF0}, std::byte{0xF3}, std::byte{0xFF}, std::byte{0xFF},
};
inline constexpr std::array<std::byte, 18U> kPublishSecondQwordPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x54}, std::byte{0x24},
    std::byte{0x10}, std::byte{0x53}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x20}, std::byte{0x4C}, std::byte{0x8B},
    std::byte{0x41}, std::byte{0x18}, std::byte{0x48}, std::byte{0x8B},
    std::byte{0xD9},
};
inline constexpr std::array<std::byte, 17U> kPublishFirstQwordPrefix{
    std::byte{0x40}, std::byte{0x53}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x20}, std::byte{0x4C}, std::byte{0x8B},
    std::byte{0x41}, std::byte{0x18}, std::byte{0x48}, std::byte{0x8B},
    std::byte{0xD9},
};
inline constexpr std::array<std::byte, 24U> kColdAllocationPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x74}, std::byte{0x24},
    std::byte{0x20}, std::byte{0x41}, std::byte{0x54}, std::byte{0x41},
    std::byte{0x56}, std::byte{0x41}, std::byte{0x57}, std::byte{0x48},
    std::byte{0x83}, std::byte{0xEC}, std::byte{0x40}, std::byte{0x4D},
    std::byte{0x8B}, std::byte{0xF9}, std::byte{0x48}, std::byte{0x63},
    std::byte{0xF2}, std::byte{0x4D}, std::byte{0x8B}, std::byte{0xF0},
};

} // namespace contract

enum class ObservationTarget : std::uint8_t {
    responseHandleInternal,
    service7Thunk,
    service7Owner,
    genericDispatcher,
    kind10Callback,
    validateStore,
    bodyAccessor,
    laterStateStep,
    publishSecondQword,
    publishFirstQword,
    coldAllocation,
    nativeActivate,
    nativeClose,
    nativeReinstantiate,
    nativeCleanup,
    nativeGlobalDrop,
    count,
};

inline constexpr std::size_t kObservationTargetCount =
    static_cast<std::size_t>(ObservationTarget::count);

class ObservationAdmission final {
public:
    [[nodiscard]] explicit constexpr operator bool() const noexcept { return admitted_; }
    [[nodiscard]] constexpr std::uintptr_t address(ObservationTarget target) const noexcept {
        return addresses_[static_cast<std::size_t>(target)];
    }

private:
    friend NativeActivationValidationResult validate_observation_image(
        const NativeActivationImageView&, ObservationAdmission&) noexcept;

    std::array<std::uintptr_t, kObservationTargetCount> addresses_{};
    bool admitted_{};
};

/**
 * Requires the exact packed-file digest and every complete prefix in the mapped image. The output
 * remains empty on any mismatch. This performs no hook installation or runtime publication.
 */
[[nodiscard]] NativeActivationValidationResult validate_observation_image(
    const NativeActivationImageView& image,
    ObservationAdmission& output) noexcept;

/** Canonical host integer represented by the service-7 big-endian octet string. */
[[nodiscard]] constexpr std::uint64_t canonical_session_id(SessionOctets octets) noexcept {
    std::uint64_t value = 0U;
    for (const std::byte octet : octets) {
        value = (value << 8U) | std::to_integer<std::uint8_t>(octet);
    }
    return value;
}

/** Qword produced when x86-64 loads the same eight wire octets without a byte swap. */
[[nodiscard]] constexpr std::uint64_t raw_little_endian_session_qword(
    SessionOctets octets) noexcept {
    std::uint64_t value = 0U;
    for (std::size_t index = 0U; index < octets.size(); ++index) {
        value |= static_cast<std::uint64_t>(std::to_integer<std::uint8_t>(octets[index]))
                 << (index * 8U);
    }
    return value;
}

[[nodiscard]] constexpr SessionOctets session_octets_from_canonical(
    std::uint64_t canonical) noexcept {
    SessionOctets octets{};
    for (std::size_t index = 0U; index < octets.size(); ++index) {
        const std::size_t shift = (octets.size() - 1U - index) * 8U;
        octets[index] = std::byte{static_cast<std::uint8_t>(canonical >> shift)};
    }
    return octets;
}

struct PrivacyKey final {
    std::uint64_t first{};
    std::uint64_t second{};

    [[nodiscard]] explicit constexpr operator bool() const noexcept {
        return first != 0U || second != 0U;
    }
};

/** Keyed scalar projection. Zero is reserved for an absent value. */
[[nodiscard]] std::uint64_t privacy_hash(PrivacyKey key,
                                         std::uint64_t domain,
                                         std::span<const std::byte> bytes) noexcept;

using GuardedCopy = bool (*)(void* context,
                             std::uintptr_t source,
                             std::span<std::byte> destination) noexcept;

/** Windows SEH guarded read. Other build targets fail closed instead of dereferencing. */
[[nodiscard]] bool guarded_process_copy(void* context,
                                        std::uintptr_t source,
                                        std::span<std::byte> destination) noexcept;

struct HostAuthorityEvidence final {
    state::activity::BindingKey binding{};
    state::activity::ActivityInstanceKey activity{};
    state::activity::PublicationGeneration rosterPublication{};
    state::activity::HostRegionKey sourceHostRegion{};
    Service7TaggedResponse committedResponse{};
};

struct CapturePoint final {
    std::uint64_t threadId{};
    std::uint64_t callId{};
    std::uint64_t parentCallId{};
    std::uint64_t queueId{};
    std::uintptr_t returnRva{};

    [[nodiscard]] explicit constexpr operator bool() const noexcept {
        return threadId != 0U && callId != 0U;
    }
};

struct HostObservationTicket final {
    std::uint64_t generation{};
    std::uint16_t slot{};

    [[nodiscard]] explicit constexpr operator bool() const noexcept {
        return generation != 0U;
    }
    friend constexpr bool operator==(HostObservationTicket,
                                     HostObservationTicket) noexcept = default;
};

struct NativeObservationTicket final {
    std::uint64_t generation{};
    std::uint16_t slot{};

    [[nodiscard]] explicit constexpr operator bool() const noexcept {
        return generation != 0U;
    }
    friend constexpr bool operator==(NativeObservationTicket,
                                     NativeObservationTicket) noexcept = default;
};

enum class HostStage : std::uint8_t {
    none,
    authorityIssued,
    responseE02280,
    thunkB536A0,
    ownerB536B0,
    dispatcher4F7DF0,
    callback4F2EE0,
    validateStore4F6880,
    activityClient15E8,
    queueTransfer,
    laterLoadC0F68D,
    publish17AB860,
    unknown17AB860ToBFE450,
    invalidated,
};

enum class HostInvalidationReason : std::uint8_t {
    none,
    bindingClose,
    activityRetire,
    hostRegionReplace,
    queueLoss,
    captureConflict,
};

enum class NativeEventKind : std::uint8_t {
    none,
    allocationBFE450,
    activationCurrent,
    registryRevalidated,
    closeInvalidated,
    reinstantiateInvalidated,
    cleanupInvalidated,
    globalDropInvalidated,
    moduleInvalidated,
};

enum class CorrelationDisposition : std::uint8_t {
    unknown,
};

enum class ObservationStatus : std::uint8_t {
    accepted,
    notAdmitted,
    invalidArgument,
    invalidAuthority,
    invalidTicket,
    staleStage,
    guardedReadFailed,
    bodyMismatch,
    executionMismatch,
    storageMismatch,
    registryStale,
    capacity,
    busy,
    queueLoss,
    generationExhausted,
};

struct HostKeyProjection final {
    std::uint32_t connectionId{};
    std::uint64_t connectionGeneration{};
    std::uint64_t authenticationGeneration{};
    std::uint64_t bindingGeneration{};
    std::uint64_t activityIncarnation{};
    std::uint64_t hostRegionGeneration{};
    std::uint64_t rosterPublicationGeneration{};
    std::uint64_t canonicalSessionHash{};
    std::uint64_t rawSessionQwordHash{};
};

struct HostObservationEvent final {
    std::uint64_t sequence{};
    HostKeyProjection owner{};
    std::uint64_t observationGeneration{};
    std::uint64_t bodyHash{};
    std::uint64_t pointerHash{};
    std::uint64_t activityClientHash{};
    std::uint64_t threadHash{};
    std::uint64_t queueHash{};
    std::uint64_t callId{};
    std::uint64_t parentCallId{};
    std::uintptr_t siteRva{};
    std::uintptr_t returnRva{};
    std::uint32_t capturedBytes{};
    HostStage stage{HostStage::none};
    HostInvalidationReason invalidation{HostInvalidationReason::none};
    CorrelationDisposition correlation{CorrelationDisposition::unknown};
    bool guardedCopy{};
    bool threadBoundary{};
    bool queueBoundary{};
};

struct NativeObservationEvent final {
    std::uint64_t sequence{};
    std::uint64_t observationGeneration{};
    std::uint64_t moduleGeneration{};
    std::uint64_t activationGeneration{};
    std::uint64_t wrapperHash{};
    std::uint64_t identityHash{};
    std::uint64_t ownerHash{};
    std::uint64_t pairedHostHash{};
    std::uint64_t threadHash{};
    std::uint64_t queueHash{};
    std::uint64_t callId{};
    std::uint64_t parentCallId{};
    std::uint64_t globalDropEpoch{};
    std::uintptr_t siteRva{};
    std::uintptr_t returnRva{};
    std::uint32_t fullHandle{kInvalidNativeActivityHandle};
    std::int32_t mode{};
    NativeEventKind kind{NativeEventKind::none};
    CorrelationDisposition correlation{CorrelationDisposition::unknown};
    bool registryCurrent{};
};

static_assert(std::is_trivially_copyable_v<HostObservationEvent>);
static_assert(std::is_trivially_copyable_v<NativeObservationEvent>);

enum class QueuePushResult : std::uint8_t {
    enqueued,
    busy,
    full,
    sequenceExhausted,
};

enum class QueuePopResult : std::uint8_t {
    success,
    empty,
    busy,
};

struct ObservationQueueCounters final {
    std::uint64_t enqueued{};
    std::uint64_t drained{};
    std::uint64_t droppedBusy{};
    std::uint64_t droppedFull{};
    std::uint64_t droppedSequenceExhausted{};
    std::uint64_t drainBusy{};
    std::uint64_t pending{};
    std::uint64_t highWater{};
    std::uint64_t lastSequence{};

    [[nodiscard]] constexpr std::uint64_t losses() const noexcept {
        return droppedBusy + droppedFull + droppedSequenceExhausted;
    }
};

/** Fixed-capacity, allocation-free MPSC/single-drainer queue with non-waiting loss. */
template <class Event, std::size_t Capacity>
class ObservationQueue final {
    static_assert(Capacity != 0U);
    static_assert(std::is_trivially_copyable_v<Event>);

public:
    [[nodiscard]] QueuePushResult try_push(const Event& source) noexcept {
        Lock guard{*this};
        if (!guard) {
            droppedBusy_.fetch_add(1U, std::memory_order_relaxed);
            return QueuePushResult::busy;
        }
        if (count_ == records_.size()) {
            droppedFull_.fetch_add(1U, std::memory_order_relaxed);
            return QueuePushResult::full;
        }
        if (nextSequence_ == (std::numeric_limits<std::uint64_t>::max)()) {
            droppedSequenceExhausted_.fetch_add(1U, std::memory_order_relaxed);
            return QueuePushResult::sequenceExhausted;
        }
        Event event = source;
        event.sequence = nextSequence_++;
        records_[(head_ + count_) % records_.size()] = event;
        ++count_;
        enqueued_.fetch_add(1U, std::memory_order_relaxed);
        pending_.store(count_, std::memory_order_relaxed);
        const std::uint64_t highWater = highWater_.load(std::memory_order_relaxed);
        if (count_ > highWater) {
            highWater_.store(count_, std::memory_order_relaxed);
        }
        lastSequence_.store(event.sequence, std::memory_order_release);
        return QueuePushResult::enqueued;
    }

    [[nodiscard]] QueuePopResult try_pop(Event& output) noexcept {
        Lock guard{*this};
        if (!guard) {
            drainBusy_.fetch_add(1U, std::memory_order_relaxed);
            return QueuePopResult::busy;
        }
        if (count_ == 0U) {
            return QueuePopResult::empty;
        }
        output = records_[head_];
        head_ = (head_ + 1U) % records_.size();
        --count_;
        drained_.fetch_add(1U, std::memory_order_relaxed);
        pending_.store(count_, std::memory_order_relaxed);
        return QueuePopResult::success;
    }

    [[nodiscard]] ObservationQueueCounters counters() const noexcept {
        return {
            enqueued_.load(std::memory_order_relaxed),
            drained_.load(std::memory_order_relaxed),
            droppedBusy_.load(std::memory_order_relaxed),
            droppedFull_.load(std::memory_order_relaxed),
            droppedSequenceExhausted_.load(std::memory_order_relaxed),
            drainBusy_.load(std::memory_order_relaxed),
            pending_.load(std::memory_order_relaxed),
            highWater_.load(std::memory_order_relaxed),
            lastSequence_.load(std::memory_order_acquire),
        };
    }

private:
    class Lock final {
    public:
        explicit Lock(ObservationQueue& owner) noexcept : owner_(&owner) {
            if (owner_->lock_.test_and_set(std::memory_order_acquire)) {
                owner_ = nullptr;
            }
        }
        ~Lock() noexcept {
            if (owner_ != nullptr) {
                owner_->lock_.clear(std::memory_order_release);
            }
        }
        Lock(const Lock&) = delete;
        Lock& operator=(const Lock&) = delete;
        [[nodiscard]] explicit operator bool() const noexcept { return owner_ != nullptr; }

    private:
        ObservationQueue* owner_{};
    };

    std::atomic_flag lock_ = ATOMIC_FLAG_INIT;
    std::array<Event, Capacity> records_{};
    std::size_t head_{};
    std::size_t count_{};
    std::uint64_t nextSequence_{1U};
    std::atomic<std::uint64_t> enqueued_{};
    std::atomic<std::uint64_t> drained_{};
    std::atomic<std::uint64_t> droppedBusy_{};
    std::atomic<std::uint64_t> droppedFull_{};
    std::atomic<std::uint64_t> droppedSequenceExhausted_{};
    std::atomic<std::uint64_t> drainBusy_{};
    std::atomic<std::uint64_t> pending_{};
    std::atomic<std::uint64_t> highWater_{};
    std::atomic<std::uint64_t> lastSequence_{};
};

enum class OriginalOncePhase : std::uint8_t {
    empty,
    prepared,
    originalRunning,
    originalReturned,
    completed,
};

/** Pure call-scope model; it owns no detour and retains no original function pointer. */
class OriginalOnceLifecycle final {
public:
    [[nodiscard]] bool prepare(std::uint64_t callId) noexcept;
    [[nodiscard]] bool enter_original() noexcept;
    [[nodiscard]] bool leave_original() noexcept;
    [[nodiscard]] bool complete() noexcept;
    [[nodiscard]] OriginalOncePhase phase() const noexcept { return phase_; }
    [[nodiscard]] std::uint64_t call_id() const noexcept { return callId_; }
    [[nodiscard]] std::uint8_t original_calls() const noexcept { return originalCalls_; }

private:
    std::uint64_t callId_{};
    OriginalOncePhase phase_{OriginalOncePhase::empty};
    std::uint8_t originalCalls_{};
};

template <class Result, class Original>
[[nodiscard]] bool invoke_original_once(OriginalOnceLifecycle& lifecycle,
                                        Original&& original,
                                        Result& output) noexcept {
    static_assert(std::is_nothrow_invocable_r_v<Result, Original>);
    if (!lifecycle.enter_original()) {
        return false;
    }
    output = std::forward<Original>(original)();
    return lifecycle.leave_original();
}

template <class Original>
[[nodiscard]] bool invoke_void_original_once(OriginalOnceLifecycle& lifecycle,
                                             Original&& original) noexcept {
    static_assert(std::is_nothrow_invocable_v<Original>);
    if (!lifecycle.enter_original()) {
        return false;
    }
    std::forward<Original>(original)();
    return lifecycle.leave_original();
}

struct IssueHostResult final {
    ObservationStatus status{ObservationStatus::invalidAuthority};
    HostObservationTicket ticket{};
};

struct ObserveNativeResult final {
    ObservationStatus status{ObservationStatus::registryStale};
    NativeObservationTicket ticket{};
};

struct NativeAllocationObservation final {
    std::uintptr_t owner{};
    std::uintptr_t wrapper{};
    std::uintptr_t pairedHost{};
    std::uint32_t fullHandle{kInvalidNativeActivityHandle};
    CapturePoint point{};
};

using HostEventDrain = void (*)(void* context, const HostObservationEvent& event) noexcept;
using NativeEventDrain = void (*)(void* context, const NativeObservationEvent& event) noexcept;

/**
 * Inert live-evidence recorder. It installs no hooks, owns no writer, and exposes no bridge
 * resolution API. Host and native records remain separate even when their privacy hashes match.
 */
class Recorder final {
public:
    Recorder() noexcept = default;
    Recorder(const Recorder&) = delete;
    Recorder& operator=(const Recorder&) = delete;

    [[nodiscard]] bool begin(const ObservationAdmission& admission, PrivacyKey privacyKey) noexcept;

    [[nodiscard]] IssueHostResult issue_host(const HostAuthorityEvidence& evidence) noexcept;

    [[nodiscard]] ObservationStatus observe_tagged(
        HostObservationTicket ticket,
        HostStage stage,
        std::uintptr_t source,
        std::size_t sourceBytes,
        GuardedCopy reader,
        void* readerContext,
        CapturePoint point) noexcept;

    [[nodiscard]] ObservationStatus observe_body(
        HostObservationTicket ticket,
        HostStage stage,
        std::uintptr_t source,
        std::size_t sourceBytes,
        GuardedCopy reader,
        void* readerContext,
        CapturePoint point) noexcept;

    [[nodiscard]] ObservationStatus observe_activity_client_store(
        HostObservationTicket ticket,
        std::uintptr_t activityClient,
        GuardedCopy reader,
        void* readerContext,
        CapturePoint point) noexcept;

    /** Optional exact queue-copy edge. A thread change at the later consumer requires this. */
    [[nodiscard]] ObservationStatus observe_queue_transfer(
        HostObservationTicket ticket,
        std::uint64_t queueId,
        std::uint64_t childCallId,
        std::uintptr_t copiedBody,
        GuardedCopy reader,
        void* readerContext,
        CapturePoint producer) noexcept;

    [[nodiscard]] ObservationStatus observe_later_load(
        HostObservationTicket ticket,
        std::uintptr_t activityClient,
        std::uintptr_t accessorResult,
        GuardedCopy reader,
        void* readerContext,
        CapturePoint point) noexcept;

    /** Emits both +17AB860 and the mandatory UNKNOWN +17AB860 -> +BFE450 terminal record. */
    [[nodiscard]] ObservationStatus observe_publish_17ab860(
        HostObservationTicket ticket,
        std::uint64_t rawSessionQword,
        CapturePoint point) noexcept;

    [[nodiscard]] std::size_t invalidate_binding(
        state::activity::BindingKey binding) noexcept;
    [[nodiscard]] std::size_t invalidate_activity(
        state::activity::ActivityInstanceKey activity) noexcept;
    [[nodiscard]] std::size_t invalidate_host_region(
        state::activity::HostRegionKey hostRegion) noexcept;

    /** Independent N0 lane. No host ticket is accepted by this method. */
    [[nodiscard]] ObservationStatus observe_native_allocation(
        const NativeAllocationObservation& observation) noexcept;

    /** Independent N1 lane, admitted only for an exact currently active registry token. */
    [[nodiscard]] ObserveNativeResult observe_native_current(
        NativeActivationToken token,
        const NativeActivationRegistry& registry,
        CapturePoint point) noexcept;

    [[nodiscard]] ObservationStatus revalidate_native(
        NativeObservationTicket ticket,
        const NativeActivationRegistry& registry,
        CapturePoint point) noexcept;

    [[nodiscard]] std::size_t invalidate_native_close(
        NativeActivationToken token,
        NativeEventKind closeKind,
        CapturePoint point) noexcept;

    /** Marks the observer's exact pre-existing module cohort; later observations are not marked. */
    [[nodiscard]] ObservationStatus begin_global_drop(
        const NativeActivationGlobalDropCohort& cohort) noexcept;

    /** Off-hook registry revalidation invalidates only the marked predecessor observations. */
    [[nodiscard]] std::size_t finish_global_drop(
        const NativeActivationGlobalDropCohort& cohort,
        const NativeActivationRegistry& registry,
        CapturePoint point) noexcept;

    [[nodiscard]] std::size_t invalidate_native_module(
        state::activity::ModuleGeneration module,
        CapturePoint point) noexcept;

    [[nodiscard]] std::size_t drain_host(HostEventDrain drain,
                                         void* context,
                                         std::size_t maximumEvents) noexcept;
    [[nodiscard]] std::size_t drain_native(NativeEventDrain drain,
                                           void* context,
                                           std::size_t maximumEvents) noexcept;
    [[nodiscard]] ObservationQueueCounters host_queue_counters() const noexcept;
    [[nodiscard]] ObservationQueueCounters native_queue_counters() const noexcept;

private:
    struct HostSlot final {
        HostAuthorityEvidence evidence{};
        HostKeyProjection projection{};
        HostObservationTicket ticket{};
        Service7Body body{};
        std::uintptr_t activityClient{};
        std::uintptr_t storageAddress{};
        std::uint64_t synchronousThread{};
        std::uint64_t synchronousCall{};
        std::uint64_t transferQueue{};
        std::uint64_t transferChildCall{};
        HostStage stage{HostStage::none};
        bool bodyCaptured{};
        bool transferObserved{};
        bool live{};
    };

    struct NativeSlot final {
        NativeActivationToken token{};
        NativeObservationTicket ticket{};
        std::uint64_t identity{};
        std::uint64_t globalDropEpoch{};
        std::int32_t mode{};
        bool identityValid{};
        bool live{};
    };

    class Lock final {
    public:
        explicit Lock(Recorder& owner) noexcept : owner_(&owner) {
            if (owner_->lock_.test_and_set(std::memory_order_acquire)) {
                owner_ = nullptr;
            }
        }
        ~Lock() noexcept {
            if (owner_ != nullptr) {
                owner_->lock_.clear(std::memory_order_release);
            }
        }
        Lock(const Lock&) = delete;
        Lock& operator=(const Lock&) = delete;
        [[nodiscard]] explicit operator bool() const noexcept { return owner_ != nullptr; }

    private:
        Recorder* owner_{};
    };

    [[nodiscard]] HostSlot* exact_host_slot(HostObservationTicket ticket) noexcept;
    [[nodiscard]] NativeSlot* exact_native_slot(NativeObservationTicket ticket) noexcept;
    [[nodiscard]] HostObservationEvent make_host_event(const HostSlot& slot,
                                                       HostStage stage,
                                                       std::uintptr_t siteRva,
                                                       std::uintptr_t pointer,
                                                       std::uint32_t bytes,
                                                       CapturePoint point) const noexcept;
    [[nodiscard]] NativeObservationEvent make_native_event(
        const NativeSlot& slot,
        NativeEventKind kind,
        CapturePoint point,
        bool registryCurrent) const noexcept;
    [[nodiscard]] bool enqueue_host_or_invalidate(HostSlot& slot,
                                                  HostObservationEvent event) noexcept;
    [[nodiscard]] bool enqueue_native(NativeObservationEvent event) noexcept;
    [[nodiscard]] std::size_t invalidate_hosts_matching(
        HostInvalidationReason reason,
        state::activity::BindingKey binding,
        state::activity::ActivityInstanceKey activity,
        state::activity::HostRegionKey hostRegion) noexcept;

    ObservationAdmission admission_{};
    PrivacyKey privacyKey_{};
    std::atomic_flag lock_ = ATOMIC_FLAG_INIT;
    std::array<HostSlot, kHostObservationSlotCount> hostSlots_{};
    std::array<NativeSlot, kNativeObservationSlotCount> nativeSlots_{};
    ObservationQueue<HostObservationEvent, kHostObservationQueueCapacity> hostQueue_{};
    ObservationQueue<NativeObservationEvent, kNativeObservationQueueCapacity> nativeQueue_{};
    std::uint64_t nextHostGeneration_{1U};
    std::uint64_t nextNativeGeneration_{1U};
    bool active_{};
};

} // namespace dawn::client::hooks::activity_lifecycle::binding_native_observation
