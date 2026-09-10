#pragma once

#include <optional>

#include "haunted_forest_launch_profile.h"

namespace sunrise::server::runtime::activity::haunted_forest {

/** Native type17 authority+C is a scenario ordinal, separately from PAH's packed
 * arrival. A different manual arrival must not inherit this initial profile. */
[[nodiscard]] inline std::optional<std::uint32_t> initial_lifetime_scenario(
    const state::activity::destination::DestinationSelection& selection,
    const state::build_data::activities::Definition& activity,
    const state::build_data::scenarios::Definition& layout,
    std::uint32_t destinationArrival) noexcept {
    if (destinationArrival != std::uint32_t{kInitialArrival.bubble} * 8U) {
        return std::nullopt;
    }
    // Reuse the complete public/content admission without modifying the committed
    // selection or treating its already-resolved profile fields as new overrides.
    state::activity::destination::DestinationSelection identity{};
    identity.activityIndex = selection.activityIndex;
    identity.packageName = selection.packageName;
    identity.packageNameLength = selection.packageNameLength;
    if (initial_arrival::apply(kInitialArrival, activity, layout, identity)
        != initial_arrival::Result::applied) {
        return std::nullopt;
    }
    return destinationArrival >> 3U;
}

} // namespace sunrise::server::runtime::activity::haunted_forest
