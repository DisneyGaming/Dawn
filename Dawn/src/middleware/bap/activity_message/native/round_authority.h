#pragma once

#include "../../../../state/activity/coo/executor.h"
#include <cstddef>
#include <cstdint>

namespace dawn::middleware::bap::activity_message::native::round_authority {

namespace coo = dawn::state::activity::coo;

/** Proven native activity-script, mission-director and lifetime slot types. */
inline constexpr std::uint8_t kTimerType = 18;
inline constexpr std::uint8_t kDirectorType = 35;
inline constexpr std::uint8_t kLifetimeType = 17;
/** Highest bubble and signed slot index accepted by the native object reference. */
inline constexpr std::uint8_t kMaximumBubble = 63;
inline constexpr std::uint16_t kMaximumSlot = 32767;
/** Native 80809919 countdown body width. */
inline constexpr std::size_t kBodyBits = 386;

/** Value-only native authority owned by one bounded round profile. */
struct State final {
    bool controlled{};
    coo::Asset timer{};
    coo::Asset director{};
    coo::Asset lifetime{};
    std::uint8_t bubble{};
    bool restricted{};
    std::uint64_t endEpoch{};
    coo::CompletionPublication completion{};
};

/** Checks a trusted asset binding without deriving identities from wire input. */
[[nodiscard]] constexpr bool valid_asset(const coo::Asset& asset,
    std::uint8_t expectedType) noexcept {
    return asset.registry != 0 && asset.registry != UINT32_MAX
        && asset.definition != 0 && asset.definition != UINT32_MAX
        && asset.type == expectedType && asset.slot <= kMaximumSlot;
}

/** Validates the three trusted profile bindings and its bounded bubble. */
[[nodiscard]] constexpr bool validState(const State& state) noexcept {
    return valid_asset(state.timer, kTimerType)
        && valid_asset(state.director, kDirectorType)
        && valid_asset(state.lifetime, kLifetimeType)
        && state.bubble <= kMaximumBubble;
}

/** Exact wire identity match; descriptor definitions are trusted profile data, not wire fields. */
[[nodiscard]] constexpr bool matches(const coo::Asset& asset, std::uint32_t key,
    std::uint8_t type, std::uint16_t slot) noexcept {
    return asset.registry == key && asset.type == type && asset.slot == slot;
}

/** Returns true only for the controlled profile's exact timer identity. */
[[nodiscard]] constexpr bool timer(const State& state, std::uint32_t key,
    std::uint8_t type, std::uint16_t slot) noexcept {
    return state.controlled && validState(state) && matches(state.timer, key, type, slot);
}

/** Returns true only for the controlled profile's exact director identity. */
[[nodiscard]] constexpr bool director(const State& state, std::uint32_t key,
    std::uint8_t type, std::uint16_t slot) noexcept {
    return state.controlled && validState(state) && matches(state.director, key, type, slot);
}

/** Returns true only for the controlled profile's exact lifetime identity. */
[[nodiscard]] constexpr bool lifetime(const State& state, std::uint32_t key,
    std::uint8_t type, std::uint16_t slot) noexcept {
    return state.controlled && validState(state) && matches(state.lifetime, key, type, slot);
}

} // namespace dawn::middleware::bap::activity_message::native::round_authority
