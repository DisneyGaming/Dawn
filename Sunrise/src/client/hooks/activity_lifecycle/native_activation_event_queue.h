#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

#include "native_activation_flow.h"

namespace sunrise::client::hooks::activity_lifecycle {

inline constexpr std::size_t kNativeActivationEventQueueCapacity = 512U;

enum class NativeActivationEventKind : std::uint8_t {
    none,
    activation,
    close,
};

enum class NativeActivationClosePath : std::uint8_t {
    none,
    ordinary,
    reinstantiate,
    cleanup,
};

/** Fixed scalar observation copied by a native hook and interpreted only by off-hook consumers. */
struct NativeActivationEvent final {
    std::uint64_t sequence{};
    state::activity::ModuleGeneration module{};
    state::activity::ActivationGeneration activation{};
    std::uintptr_t observedWrapper{};
    std::uintptr_t publicationWrapper{};
    std::uint64_t identity{};
    std::uint32_t observedFullHandle{kInvalidNativeActivityHandle};
    std::uint32_t publicationFullHandle{kInvalidNativeActivityHandle};
    std::int32_t mode{};
    PublishStatus publishStatus{PublishStatus::moduleInactive};
    CaptureStatus captureStatus{CaptureStatus::notCurrent};
    RetireStatus retireStatus{RetireStatus::invalidToken};
    NativeActivationEventKind kind{NativeActivationEventKind::none};
    NativeActivationClosePath closePath{NativeActivationClosePath::none};
    std::uint8_t nativeResult{};
    std::uint8_t validateHost{};
    bool identityValid{};
    bool postconditionObserved{};
};

static_assert(std::is_trivially_copyable_v<NativeActivationEvent>);
// std::atomic_flag is the standard's always-lock-free atomic primitive.
static_assert(std::atomic<std::uint64_t>::is_always_lock_free);

enum class NativeActivationEventPushResult : std::uint8_t {
    enqueued,
    invalid,
    busy,
    full,
    sequenceExhausted,
};

enum class NativeActivationEventPopResult : std::uint8_t {
    success,
    empty,
    busy,
};

struct NativeActivationEventQueueCounters final {
    std::uint64_t enqueued{};
    std::uint64_t drained{};
    std::uint64_t invalid{};
    std::uint64_t droppedBusy{};
    std::uint64_t droppedFull{};
    std::uint64_t droppedSequenceExhausted{};
    std::uint64_t drainBusy{};
    std::uint64_t pending{};
    std::uint64_t highWater{};
    std::uint64_t lastSequence{};

    [[nodiscard]] constexpr std::uint64_t losses() const noexcept {
        return invalid + droppedBusy + droppedFull + droppedSequenceExhausted;
    }
};

/** Fixed-capacity MPSC/single-drainer queue with non-waiting contention loss. */
template <std::size_t Capacity>
class NativeActivationEventQueue final {
    static_assert(Capacity != 0U);

public:
    NativeActivationEventQueue() noexcept = default;
    NativeActivationEventQueue(const NativeActivationEventQueue&) = delete;
    NativeActivationEventQueue& operator=(const NativeActivationEventQueue&) = delete;

    [[nodiscard]] NativeActivationEventPushResult
    try_push(const NativeActivationEvent& source) noexcept {
        if (source.kind == NativeActivationEventKind::none) {
            invalid_.fetch_add(1U, std::memory_order_relaxed);
            return NativeActivationEventPushResult::invalid;
        }

        QueueLock lock{*this};
        if (!lock) {
            droppedBusy_.fetch_add(1U, std::memory_order_relaxed);
            return NativeActivationEventPushResult::busy;
        }
        if (count_ == records_.size()) {
            droppedFull_.fetch_add(1U, std::memory_order_relaxed);
            return NativeActivationEventPushResult::full;
        }
        if (nextSequence_ == (std::numeric_limits<std::uint64_t>::max)()) {
            droppedSequenceExhausted_.fetch_add(1U, std::memory_order_relaxed);
            return NativeActivationEventPushResult::sequenceExhausted;
        }

        NativeActivationEvent event = source;
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
        return NativeActivationEventPushResult::enqueued;
    }

    [[nodiscard]] NativeActivationEventPopResult
    try_pop(NativeActivationEvent& output) noexcept {
        QueueLock lock{*this};
        if (!lock) {
            drainBusy_.fetch_add(1U, std::memory_order_relaxed);
            return NativeActivationEventPopResult::busy;
        }
        if (count_ == 0U) {
            return NativeActivationEventPopResult::empty;
        }
        output = records_[head_];
        head_ = (head_ + 1U) % records_.size();
        --count_;
        drained_.fetch_add(1U, std::memory_order_relaxed);
        pending_.store(count_, std::memory_order_relaxed);
        return NativeActivationEventPopResult::success;
    }

    [[nodiscard]] NativeActivationEventQueueCounters counters() const noexcept {
        return NativeActivationEventQueueCounters{
            enqueued_.load(std::memory_order_relaxed),
            drained_.load(std::memory_order_relaxed),
            invalid_.load(std::memory_order_relaxed),
            droppedBusy_.load(std::memory_order_relaxed),
            droppedFull_.load(std::memory_order_relaxed),
            droppedSequenceExhausted_.load(std::memory_order_relaxed),
            drainBusy_.load(std::memory_order_relaxed),
            pending_.load(std::memory_order_relaxed),
            highWater_.load(std::memory_order_relaxed),
            lastSequence_.load(std::memory_order_acquire),
        };
    }

    /** Deterministic unit-test seam; production hook paths never call these methods. */
    [[nodiscard]] bool testing_lock() noexcept {
        return !lock_.test_and_set(std::memory_order_acquire);
    }
    void testing_unlock() noexcept { lock_.clear(std::memory_order_release); }
    void testing_set_next_sequence(std::uint64_t sequence) noexcept {
        nextSequence_ = sequence;
    }

private:
    class QueueLock final {
    public:
        explicit QueueLock(NativeActivationEventQueue& owner) noexcept
            : owner_(&owner) {
            if (owner_->lock_.test_and_set(std::memory_order_acquire)) {
                owner_ = nullptr;
            }
        }

        ~QueueLock() noexcept {
            if (owner_ != nullptr) {
                owner_->lock_.clear(std::memory_order_release);
            }
        }

        QueueLock(const QueueLock&) = delete;
        QueueLock& operator=(const QueueLock&) = delete;

        [[nodiscard]] explicit operator bool() const noexcept { return owner_ != nullptr; }

    private:
        NativeActivationEventQueue* owner_{};
    };

    std::atomic_flag lock_ = ATOMIC_FLAG_INIT;
    std::array<NativeActivationEvent, Capacity> records_{};
    std::size_t head_{};
    std::size_t count_{};
    std::uint64_t nextSequence_{1U};
    std::atomic<std::uint64_t> enqueued_{};
    std::atomic<std::uint64_t> drained_{};
    std::atomic<std::uint64_t> invalid_{};
    std::atomic<std::uint64_t> droppedBusy_{};
    std::atomic<std::uint64_t> droppedFull_{};
    std::atomic<std::uint64_t> droppedSequenceExhausted_{};
    std::atomic<std::uint64_t> drainBusy_{};
    std::atomic<std::uint64_t> pending_{};
    std::atomic<std::uint64_t> highWater_{};
    std::atomic<std::uint64_t> lastSequence_{};
};

using NativeActivationEventDrain =
    void (*)(void* context, const NativeActivationEvent& event) noexcept;

/**
 * Drains at most maximumEvents through one off-hook callback. No formatting/logging is performed
 * by this module. Callers serialize drain ownership and may project records after each pop unlocks.
 */
[[nodiscard]] std::size_t drain_native_activation_events(NativeActivationEventDrain drain,
                                                         void* context,
                                                         std::size_t maximumEvents) noexcept;

[[nodiscard]] NativeActivationEventQueueCounters native_activation_event_counters() noexcept;

namespace detail {

/** Pure scalar projections used by the hook replacements and deterministic tests. */
[[nodiscard]] NativeActivationEvent make_native_activation_event(
    const NativeActivationAttempt& attempt,
    const ActivationFlowResult& result) noexcept;

[[nodiscard]] NativeActivationEvent make_native_activation_close_event(
    NativeActivationClosePath path,
    std::uintptr_t observedWrapper,
    std::uint32_t observedFullHandle,
    const CloseFlowResult& result) noexcept;

/** Non-waiting hook-side enqueue; loss is retained in scalar counters. */
[[nodiscard]] NativeActivationEventPushResult
enqueue_native_activation_event(const NativeActivationEvent& event) noexcept;

} // namespace detail

} // namespace sunrise::client::hooks::activity_lifecycle
