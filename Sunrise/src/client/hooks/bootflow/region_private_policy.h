#pragma once

#include <cstdint>
#include <string_view>

#include "../../../state/activity/forced/definition.h"

namespace sunrise::client::hooks::bootflow::region_private_policy {

/**
 * Keeps the operator setting authoritative and scopes the Raid-panel solo route to its exact
 * native activity, package, and eight authored slices. Other direct launches stay public.
 */
[[nodiscard]] constexpr bool force_private(bool configured,
                                           std::int16_t directActivity,
                                           const state::activity::forced::ForcedDestination& destination,
                                           std::uint32_t sliceSet) noexcept {
    if (configured) {
        return true;
    }
    namespace profiles = state::activity::forced::profiles;
    if (directActivity != profiles::kEaterOfWorldsActivity
        || !state::activity::forced::active(destination)
        || std::string_view(destination.packageName.data(), destination.packageNameLength)
            != "raid_envy_v310") {
        return false;
    }
    for (const auto owned : profiles::kEaterOfWorldsSlices) {
        if (sliceSet == owned) {
            return true;
        }
    }
    return false;
}

} // namespace sunrise::client::hooks::bootflow::region_private_policy
