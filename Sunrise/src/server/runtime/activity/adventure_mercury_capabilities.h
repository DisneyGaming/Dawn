#pragma once
#include "native_activity_definition.h"
#include "../../../state/activity/coo/adventure_mercury.h"

namespace sunrise::server::runtime::activity::adventure::mercury {
namespace authored = state::activity::coo::adventure::mercury;
// Uses the common server placement service and unchanged native 8080992F codec.
// Authored entity 80C0127D owns prompt, eligibility channels and interaction.
// No client prop creation, local input injection or launch wrapper is necessary
// to request these native placements. Native launch completion remains unverified.
inline constexpr std::array<placement::Capability,3> kPlacements{{
    {&authored::kRegistry,0},{&authored::kRegistry,1},{&authored::kRegistry,2},
}};
inline constexpr auto kScriptCapabilities=[] {
    std::array<coo::script::Capability,3> result{};
    for(std::size_t i=0;i<result.size();++i) {
        const auto& b=authored::kBeacons[i];
        result[i]={b.capability,"nativeActivity",{coo::Operation::device,
            {authored::kRegistry.key,b.descriptor,4,b.slot},1,coo::Wait::requested}};
    }
    return result;
}();
// The destination profile determines where its placement span includes this set.
[[nodiscard]] constexpr auto actions(std::uint8_t firstPlacement) noexcept {
    std::array<NativeAction,3> result{};
    for(std::size_t i=0;i<result.size();++i)
        result[i]={kScriptCapabilities[i].spec,static_cast<std::uint16_t>(firstPlacement+i),{}};
    return result;
}
} // namespace sunrise::server::runtime::activity::adventure::mercury
