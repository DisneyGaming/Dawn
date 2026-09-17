#pragma once
#include "state/activity/gateway/traversal_catalog.h"
#include "state/activity/deep_storage/ai_bindings.h"
#include "state/activity/hijacked/ai_bindings.h"
#include "state/activity/strike_bond/catalog.h"
#include "state/activity/strike_pact/catalog_all.h"
#include "state/activity/omega/omega_mission_catalog.h"

namespace reinforcement_count_checks {
namespace a=dawn::state::activity;
// Route placements, unique actors, and both native category shapes.
static_assert(a::gateway::spawn(a::gateway::kTraversalRegistry,222)->count==2);
static_assert(a::gateway::spawn(a::gateway::kTraversalRegistry,229)->count==3);
static_assert(a::gateway::spawn(a::gateway::kTraversalRegistry,208)->count==2);
static_assert(a::gateway::spawn(a::gateway::kTraversalRegistry,16)->count==1);
static_assert(a::gateway::spawn(a::gateway::kTraversalRegistry,216)->count==1);
static_assert(a::gateway::spawn(a::gateway::kMainlandRegistry,49)->count==1);
static_assert(a::gateway::spawn(a::gateway::kMainlandRegistry,99)->count==1);
static_assert([] {
    unsigned unchanged{};
    for(std::size_t i=0;i<a::gateway::kSpawns.size();++i) {
        const auto& s=a::gateway::kSpawns[i];
        if(s.cohort!=2 && s.cohort!=4 && s.cohort!=6) {
            if(s.count!=a::gateway::kBaseSpawns[i].count) return false;
            ++unchanged;
        }
    }
    return unchanged==121; // Exactly five sources in the three reinforcement sets change.
}());
static_assert(a::deep_storage::spawn(0x59700FA7U,1)->count==1);
static_assert(a::deep_storage::spawn(0x59700FA7U,27)->count==1);
static_assert(a::deep_storage::spawn(0x59700FA7U,35)->count==1); // Gate Hydra.
static_assert(a::deep_storage::spawn(0x59700FA7U,113)->count==2);
static_assert(a::hijacked::spawn(0x153E22CDU,2)->count==3); // Authored patrol retains three.
static_assert(a::hijacked::spawn(0x153E22CDU,21)->count==1); // Entangled Mind.
static_assert(a::hijacked::spawn(0x3E9B74F3U,11)->count==3);
static_assert(a::hijacked::spawn(0x3E9B74F3U,16)->requested==std::array<std::uint8_t,2>{1,2});
static_assert([] {
    for(const auto& s:a::strike_bond::kSpawns) {
        if(s.asset.registry!=0x2CB86C0FU) continue;
        if(s.asset.slot==3 && s.count!=1) return false;
        if(s.asset.slot==25 && s.count!=1) return false;
        if(s.asset.slot==13 && s.count!=1) return false;
        if(s.asset.slot==29 && s.count!=1) return false;
    }
    return true;
}());
static_assert(a::strike_pact::all_spawn(a::strike_pact::kChase,0)->count==1);
static_assert(a::strike_pact::all_spawn(a::strike_pact::kChase,7)->count==1);
static_assert(a::strike_pact::all_spawn(a::strike_pact::kLedge,15)->count==1);
static_assert(a::strike_pact::all_spawn(a::strike_pact::kBoss,57)->count==1);
static_assert(a::strike_pact::all_spawn(a::strike_pact::kBoss,50)->count==1);
static_assert(a::omega::mission::kWaveTotals==std::array<unsigned,15>{12,9,3,3,6,18,4,6,18,8,7,11,20,11,7});
}
