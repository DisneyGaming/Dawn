#pragma once

#include <cstdint>
#include <utility>

#include "native_activation_registry.h"

namespace dawn::client::hooks::activity_lifecycle {

/** Result of one activation call observed around its native original. */
struct ActivationFlowResult final {
    PublishResult publication{};
    std::uint8_t nativeResult{};
    bool postconditionObserved{};
    bool postWorkAdmitted{};
};

/** Result of one close/reopen/cleanup call observed around its native original. */
struct CloseFlowResult final {
    CaptureResult capture{};
    RetireStatus retirement{RetireStatus::invalidToken};
    bool postWorkAdmitted{};
};

/** Result of the parameterless global wrapper-drop owner around its native original. */
struct GlobalDropFlowResult final {
    GlobalDropBeginResult capture{};
    GlobalDropFinishResult retirement{};
    bool postWorkAdmitted{};
};

/**
 * Calls an activation original first and publishes only after both proved success predicates.
 *
 * The caller copies `attempt` before entering this helper. ActiveReader performs the guarded
 * post-original read of wrapper+0x20. PostAdmission suppresses all post-work after hook quiesce.
 */
template <class Original, class ActiveReader, class PostAdmission>
[[nodiscard]] ActivationFlowResult observe_activation(
    NativeActivationRegistry& registry,
    const NativeActivationAttempt& attempt,
    Original&& original,
    ActiveReader&& activeReader,
    PostAdmission&& postAdmission) noexcept {
    ActivationFlowResult result{};
    result.nativeResult = static_cast<std::uint8_t>(std::forward<Original>(original)());
    result.postWorkAdmitted = static_cast<bool>(std::forward<PostAdmission>(postAdmission)());
    if (!result.postWorkAdmitted || result.nativeResult == 0U) {
        return result;
    }

    result.postconditionObserved = static_cast<bool>(std::forward<ActiveReader>(activeReader)());
    if (result.postconditionObserved) {
        result.publication = registry.publish(attempt);
    }
    return result;
}

/**
 * Captures/quiesces an exact predecessor, calls the native original with no registry lock held,
 * then retires only that captured token. A nested activation may replace the slot in between.
 */
template <class Original, class PostAdmission>
[[nodiscard]] CloseFlowResult observe_close(NativeActivationRegistry& registry,
                                            std::uintptr_t wrapper,
                                            std::uint32_t fullHandle,
                                            bool nativeActive,
                                            Original&& original,
                                            PostAdmission&& postAdmission) noexcept {
    CloseFlowResult result{};
    result.capture = registry.capture_predecessor(wrapper, fullHandle, nativeActive);

    std::forward<Original>(original)();

    result.postWorkAdmitted = static_cast<bool>(std::forward<PostAdmission>(postAdmission)());
    if (result.postWorkAdmitted && result.capture.token) {
        result.retirement = registry.retire(result.capture.token);
    }
    return result;
}

/**
 * Quiesces a bounded exact predecessor cohort before the native global sweep, calls the original
 * without registry synchronization held, then compare-retires only that cohort. The module stays
 * active so later successful activations may publish fresh generations. PreObserver runs after
 * capture unlocks; PostObserver runs after native returns and before activation compare-retirement.
 */
template <class Original, class PostAdmission, class PreObserver, class PostObserver>
[[nodiscard]] GlobalDropFlowResult observe_global_drop(
    NativeActivationRegistry& registry,
    Original&& original,
    PostAdmission&& postAdmission,
    PreObserver&& preObserver,
    PostObserver&& postObserver) noexcept {
    GlobalDropFlowResult result{};
    result.capture = registry.begin_global_drop();

    std::forward<PreObserver>(preObserver)(result.capture);

    std::forward<Original>(original)();

    std::forward<PostObserver>(postObserver)(result.capture);

    result.postWorkAdmitted = static_cast<bool>(std::forward<PostAdmission>(postAdmission)());
    if (result.postWorkAdmitted && result.capture.token) {
        result.retirement = registry.finish_global_drop(result.capture.token);
    }
    return result;
}

template <class Original, class PostAdmission>
[[nodiscard]] GlobalDropFlowResult observe_global_drop(
    NativeActivationRegistry& registry,
    Original&& original,
    PostAdmission&& postAdmission) noexcept {
    return observe_global_drop(
        registry,
        std::forward<Original>(original),
        std::forward<PostAdmission>(postAdmission),
        [](const GlobalDropBeginResult&) noexcept {},
        [](const GlobalDropBeginResult&) noexcept {});
}

} // namespace dawn::client::hooks::activity_lifecycle
