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
// Nothing spawns until an explicit development request is accepted.
inline constexpr std::array<population::Capability,9> kPopulations{{
    {&kRegistries[0],0,8,{0x74337EDD,2,0},true},
    {&kRegistries[0],1,9,{0x74337EDD,2,1},true},
    {&kRegistries[1],0,0,{},false},
    ambient::probe::kPopulation,
    ambient::cabal_probe::kPopulation,
    public_events::kOpeningSources[0],public_events::kOpeningSources[1],
    public_events::kGatekeeperSources[0],public_events::kGatekeeperSources[1],
}};
} // namespace sunrise::server::runtime::activity::mercury
