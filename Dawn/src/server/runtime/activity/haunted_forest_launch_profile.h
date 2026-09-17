#pragma once

#include "trusted_initial_arrival_profile.h"
#include "../../../state/build_data/runtime.h"

namespace dawn::server::runtime::activity::haunted_forest {

/** #78 test profile: physical mapping proved in installed content; initial spawn is
 * corroborated by the supplied different-build record. See HAUNTED-FOREST-LAUNCH.md. */
inline constexpr initial_arrival::Profile kInitialArrival{
    78, 0x56B7B6A5, 0xBCD46D46, 0, 16, "infinite_abyss", 0x81550015,
    "infinite_forest_live", 20, 13, 0x47EA4CE9, 30, 1, 0x79E3AB1F};

[[nodiscard]] inline initial_arrival::Result
apply_initial_arrival(state::activity::destination::DestinationSelection& selection) noexcept {
    using initial_arrival::Result;
    if (selection.activityIndex != kInitialArrival.activityIndex
        || initial_arrival::package_name(selection) != kInitialArrival.package) {
        return Result::not_applicable;
    }
    const auto activities = state::build_data::activities::entries();
    constexpr auto index = static_cast<std::size_t>(kInitialArrival.activityIndex);
    state::build_data::scenarios::Definition layout{};
    if (index >= activities.size()
        || !state::build_data::find_scenario_layout(kInitialArrival.package, layout)) {
        return Result::content_mismatch;
    }
    return initial_arrival::apply(kInitialArrival, activities[index], layout, selection);
}

} // namespace dawn::server::runtime::activity::haunted_forest
