#include "native_activation_event_queue.h"

namespace sunrise::client::hooks::activity_lifecycle {
namespace {

NativeActivationEventQueue<kNativeActivationEventQueueCapacity> g_events{};

} // namespace

std::size_t drain_native_activation_events(NativeActivationEventDrain drain,
                                           void* context,
                                           std::size_t maximumEvents) noexcept {
    if (drain == nullptr || maximumEvents == 0U) {
        return 0U;
    }

    std::size_t drained = 0U;
    while (drained < maximumEvents) {
        NativeActivationEvent event{};
        const NativeActivationEventPopResult result = g_events.try_pop(event);
        if (result != NativeActivationEventPopResult::success) {
            break;
        }
        drain(context, event);
        ++drained;
    }
    return drained;
}

NativeActivationEventQueueCounters native_activation_event_counters() noexcept {
    return g_events.counters();
}

namespace detail {

NativeActivationEvent make_native_activation_event(
    const NativeActivationAttempt& attempt,
    const ActivationFlowResult& result) noexcept {
    const NativeActivationSnapshot& publication = result.publication.snapshot;
    return NativeActivationEvent{
        0U,
        publication.key.module,
        publication.key.generation,
        attempt.wrapper,
        publication.wrapper,
        attempt.identity,
        attempt.fullHandle,
        publication.fullHandle,
        attempt.mode,
        result.publication.status,
        CaptureStatus::notCurrent,
        RetireStatus::invalidToken,
        NativeActivationEventKind::activation,
        NativeActivationClosePath::none,
        result.nativeResult,
        attempt.validateHost,
        attempt.identityValid,
        result.postconditionObserved,
    };
}

NativeActivationEvent make_native_activation_close_event(
    NativeActivationClosePath path,
    std::uintptr_t observedWrapper,
    std::uint32_t observedFullHandle,
    const CloseFlowResult& result) noexcept {
    const NativeActivationSnapshot& predecessor = result.capture.predecessor;
    return NativeActivationEvent{
        0U,
        predecessor.key.module,
        predecessor.key.generation,
        observedWrapper,
        predecessor.wrapper,
        predecessor.identity,
        observedFullHandle,
        predecessor.fullHandle,
        0,
        PublishStatus::moduleInactive,
        result.capture.status,
        result.retirement,
        NativeActivationEventKind::close,
        path,
        0U,
        0U,
        predecessor.identityValid,
        false,
    };
}

NativeActivationEventPushResult enqueue_native_activation_event(
    const NativeActivationEvent& event) noexcept {
    return g_events.try_push(event);
}

} // namespace detail

} // namespace sunrise::client::hooks::activity_lifecycle
