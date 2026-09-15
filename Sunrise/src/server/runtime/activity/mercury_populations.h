#pragma once
#include "population_service.h"
#include "mercury_registries.h"
#include "mercury_ambient_probe.h"
#include "mercury_ambient_cabal_probe.h"
#include "mercury_public_event_opening.h"

namespace sunrise::server::runtime::activity::mercury {
// Source/rule identities are package-derived. Reviewed ordinary siblings use
// their authored fallback points, matching the working surface populations.
// Initial tactical rows and squad concurrency remain reconstructed host policy.
// The former source0 primary-rule probes are now ordinary fallback siblings;
// standalone primary-probe definitions/tests remain separate from this profile.
// The persistent Mercury director requests the selected patrol and war sources.
inline constexpr auto kPopulations=[] {
    std::array<population::Capability,32> result{{
    {&kRegistries[0],0,9,{0x74337EDD,2,1},true},
    {&kRegistries[0],1,9,{0x74337EDD,2,1},true},
    {&kRegistries[1],0,0,{},false},
    public_events::kOpeningSources[0],public_events::kOpeningSources[1],
    public_events::kGatekeeperSources[0],public_events::kGatekeeperSources[1],
    // Persistent free-roam placements. Source/rule/tactical identities are
    // package-authored; initial request and respawn cadence are host policy.
    // Fallback rule bindings avoid the primary-rule named-point dependency.
    // EB1E8934 has one source, so it keeps slot0 and uses its fallback rule.
    {&kRegistries[4],0,6,{0xEB1E8934,1,1},true},
    {&kRegistries[6],1,9,{0xB3CBA385,2,1},true},
    {&kRegistries[7],1,9,{0x2571C34D,2,1},true},
    {&kRegistries[8],1,9,{0x9D083869,2,1},true},
    {&kRegistries[9],1,9,{0x9692BB5E,2,1},true},
    {&kRegistries[10],1,9,{0x1ED6087A,2,1},true},
    {&kRegistries[11],1,7,{0xFB7F2889,2,1},true},
    {&kRegistries[12],1,8,{0x1780D86F,2,1},true},
    // Native hotspot placements supply opposing Cabal/Vex war fronts.
    {&kRegistries[13],1,7,{0x90EFDE28,2,1},true},
    {&kRegistries[14],1,7,{0xCF2196EA,2,1},true},
    {&kRegistries[15],1,9,{0x8C756CC3,2,1},true},
    {&kRegistries[16],1,7,{0xBBF1BA51,2,1},true},
    {&kRegistries[17],1,7,{0x9B219BF3,2,1},true},
    {&kRegistries[18],1,9,{0xCCF03E8D,3,1},true},
    {&kRegistries[19],1,9,{0x0EFE61CB,2,1},true},
    // Ordinary Legionary escort for the already enabled forest Centurion.
    {&kRegistries[7],0,9,{0x2571C34D,2,1},true},
    // Reviewed ordinary cannon-tower cohort sibling. Keep the exact source0
    // identity while using its package-authored fallback rule and objective.
    {&kRegistries[8],0,9,{0x9D083869,2,1},true},
    // Complete selected native families without enabling another registry.
    {&kRegistries[9],0,9,{0x9692BB5E,2,1},true},
    {&kRegistries[10],0,9,{0x1ED6087A,2,1},true,0,2},
    {&kRegistries[11],0,7,{0xFB7F2889,2,1},true},
    {&kRegistries[12],0,8,{0x1780D86F,2,1},true},
    {&kRegistries[14],0,7,{0xCF2196EA,2,1},true},
    {&kRegistries[16],0,7,{0xBBF1BA51,2,1},true},
    {&kRegistries[17],0,7,{0x9B219BF3,2,1},true},
    {&kRegistries[19],0,9,{0x0EFE61CB,2,1},true},
    }};
    // Patrols may reselect the owning encounter's authored task rows using
    // native costs. Vendors, public-event cohorts and diagnostics stay fixed.
    result[1].taskMask=(1U<<4)-1U;
    for(std::size_t i=7;i<=21;++i) {
        const auto* group=state::activity::coo::mercury::ambient::find(result[i].registry->key);
        result[i].taskMask=(1U<<group->tacticalRows)-1U;
    }
    result[23].taskMask=result[10].taskMask;
    result[0].taskMask=result[1].taskMask;
    result[22].taskMask=result[9].taskMask;
    for(std::size_t i=24;i<result.size();++i) {
        const auto* group=state::activity::coo::mercury::ambient::find(result[i].registry->key);
        result[i].taskMask=(1U<<group->tacticalRows)-1U;
    }
    return result;
}();
} // namespace sunrise::server::runtime::activity::mercury
