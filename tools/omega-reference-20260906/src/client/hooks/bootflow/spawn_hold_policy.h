#pragma once

#include <cstdint>

namespace sunrise::client::hooks::bootflow::spawn_hold_policy {

/** World-transition state needed by the spawn/fade decision. */
enum class Phase : std::uint8_t {
    idle,
    transitioning,
    arrived,
};

/** Immutable inputs captured by one admitted spawn-gate call. */
struct Input final {
    bool nativeAllowed{};
    Phase phase{Phase::idle};
    bool holdEnabled{};
    bool timedOut{};
    bool alreadyReleased{};
    bool loaderBusy{};
};

/** Pure spawn suppression and fade-release decision for one admitted native answer. */
struct Decision final {
    bool loaderLoading{};
    bool loading{};
    bool releaseFade{};
    bool result{};
};

/**
 * Keeps spawn suppression and fade release on opposite sides of the arrival boundary.
 * Fade release is legal only after arrival, once no configured hold remains, and only once.
 */
[[nodiscard]] constexpr Decision decide(const Input& input) noexcept {
    const bool loaderLoading =
        input.phase == Phase::arrived && !input.alreadyReleased && input.loaderBusy;
    const bool loading = input.holdEnabled && !input.timedOut && !input.alreadyReleased
                         && (input.phase == Phase::transitioning || loaderLoading);
    return Decision{
        loaderLoading,
        loading,
        input.phase == Phase::arrived && !loading && !input.alreadyReleased,
        input.nativeAllowed && !loading,
    };
}

} // namespace sunrise::client::hooks::bootflow::spawn_hold_policy
