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

/** Native witnesses required before Towerfall may replace the unreachable migration latch. */
struct TowerfallReadiness final {
    bool nativeAllowed{};
    Phase phase{Phase::idle};
    bool initialSliceComplete{};
    bool worldReadable{};
    std::int32_t worldState{-1};
    bool localReady{};
    bool scriptRuntime{};
    bool directorRuntime{};
};

/**
 * Accepts only Towerfall's fully arrived native spawn boundary. World state 3 is the in-world
 * state observed by the retail spawn gate; type 18 and type 35 prove the script and director
 * runtimes were constructed rather than merely listed in the roster.
 */
[[nodiscard]] constexpr bool towerfall_ready(const TowerfallReadiness& input) noexcept {
    return input.nativeAllowed && input.phase == Phase::arrived
           && input.initialSliceComplete && input.worldReadable && input.worldState == 3
           && input.localReady && input.scriptRuntime && input.directorRuntime;
}

/** Evidence for completing a pending arrival after the native player already spawned. */
struct FrameArrival final {
    Phase phase{Phase::idle};
    bool alreadyReleased{};
    bool controlledEntity{};
    bool worldReadable{};
    std::int32_t worldState{-1};
    bool localReady{};
    bool loaderReadable{};
    bool loaderBusy{};
};

/** A frame may finish the fade only with current player ownership and a fully loaded world. */
[[nodiscard]] constexpr bool frame_arrival_ready(const FrameArrival& input) noexcept {
    return input.phase == Phase::arrived && !input.alreadyReleased && input.controlledEntity
           && input.worldReadable && input.worldState == 3 && input.localReady
           && input.loaderReadable && !input.loaderBusy;
}

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
