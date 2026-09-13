#pragma once
#include "population_service.h"
#include "mercury_registries.h"
#include "mercury_ambient_probe.h"
#include "mercury_ambient_cabal_probe.h"
#include "mercury_public_event_opening.h"

namespace sunrise::server::runtime::activity::mercury {
// Source/rule identities are package-derived. The pond row assignments are
// test policy: rows 0/1 of the authored four-row objective, matching source
// ordinals. Exact retail host scheduling and initial request counts remain
// unverified. Slot 0 tests the primary rule (8); slot 1 tests the package's
// fallback (9). The primary consumed a request without an actor in r1. This
// explicit A/B test is not an automatic fallback or a retail scheduling claim.
// The persistent Mercury director requests the selected patrol and war sources.
inline constexpr std::array<population::Capability,23> kPopulations{{
    {&kRegistries[0],0,8,{0x74337EDD,2,0},true},
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
    // The Vex diagnostic shares capability7 exactly. The distinct Cabal
    // primary-rule probe remains available at count zero.
    {&kRegistries[7],0,8,{0x2571C34D,2,0},true},
}};
} // namespace sunrise::server::runtime::activity::mercury
