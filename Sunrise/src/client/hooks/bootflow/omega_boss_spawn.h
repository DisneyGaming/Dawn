#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

#include "../../../state/activity/omega/omega_progression.h"
#include "omega_reveal_source.h"

namespace sunrise::client::hooks::bootflow::omega_boss_spawn {

inline constexpr std::uintptr_t kRequestRva = 0x4E2E80;
inline constexpr std::size_t kComponentBytes = 0x690;
using Request = void(*)(std::byte*, std::uint64_t, const std::int32_t*, std::int32_t*) noexcept;

// Require the full request component as well as its captured live identity.
[[nodiscard]] inline bool matches(std::span<const std::byte> component) noexcept {
    return component.size() >= kComponentBytes
        && omega_reveal_source::matches(component, omega_reveal_source::kBoss);
}

[[nodiscard]] inline bool eligible(bool active, const state::activity::omega::Progress& progress) noexcept {
    return active && progress.route >= state::activity::omega::Route::crownEntrance
        && progress.loadedBubble == 14 && progress.bossDoorReached;
}

// The authored sq_boss has one member category containing Panoptes. Native code
// selects its member and sr_boss_location_1, then constructs/queues the actor.
// This gateway needs only the verified native entry, never the retired debug hook.
[[nodiscard]] inline bool submit(bool active, const state::activity::omega::Progress& progress,
                                 std::span<std::byte> component, std::span<std::int32_t> result,
                                 Request request) noexcept {
    if (!request || !eligible(active, progress) || !matches(component) || result.size() < 9) return false;
    const std::array<std::int32_t, 10> counts{1, 1};
    request(component.data(), 2, counts.data(), result.data());
    return true;
}

} // namespace sunrise::client::hooks::bootflow::omega_boss_spawn
