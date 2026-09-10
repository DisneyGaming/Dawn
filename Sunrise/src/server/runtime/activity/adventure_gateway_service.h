#pragma once
#include "adventure_authored_overlay.h"
#include "placement_service.h"
#include "../../../middleware/bap/activity_message/native/adventure_player_predicates.h"
#include "../../../state/activity/coo/executor.h"

namespace sunrise::server::runtime::activity::adventure::gateway {
namespace predicates=middleware::bap::activity_message::native::player_predicates;
namespace coo=state::activity::coo;
// Authored paired controllers perform contact and movement. The host publishes
// their placement requests and the named player predicate used by the source.
struct Binding final {
    std::array<placement::Capability,2> endpoints{};
    std::uint32_t predicate{};
    const authored_overlay::Binding* region{};
};
[[nodiscard]] inline coo::Asset asset(const placement::Capability& endpoint) noexcept {
    if(endpoint.registry)for(const auto& slot:endpoint.registry->slots)
        if(slot.index==endpoint.slot && slot.type==4)
            return {endpoint.registry->key,slot.descriptorTag,4,endpoint.slot};
    return {};
}
[[nodiscard]] inline bool valid(const Binding& binding,const authored_overlay::Binding& opening) noexcept {
    const auto* region=binding.region;
    if(!region || !binding.predicate || binding.predicate==UINT32_MAX || binding.predicate==0x811C9DC5
        || region->hostScenario!=opening.hostScenario || region->selectedScenario!=opening.selectedScenario
        || region->selectedPackage!=opening.selectedPackage || region->bubble>=64 || region->bubble==opening.bubble
        || !authored_overlay::valid(region->root) || !authored_overlay::valid(region->local)
        || region->root.key!=opening.root.key || region->root.object!=opening.root.object
        || !region->bubbleHash)return false;
    placement::wire::Batch projected{};
    if(!placement::project(binding.endpoints,opening.bubble,projected) || projected.count!=2)return false;
    for(const auto& endpoint:binding.endpoints)
        if(!endpoint.generation || endpoint.registry->scenario!=opening.hostScenario
            || !asset(endpoint).definition)return false;
    return true;
}
// All additions are atomic and retain other services' full placement requests,
// including capture/controller payloads. Publication is not native readiness.
[[nodiscard]] inline bool project(const Binding& binding,std::uint32_t bubble,
    placement::wire::Batch& placements,predicates::Set& names) noexcept {
    if(placements.count>placements.entries.size() || !predicates::valid(names))return false;
    if(!binding.region || !binding.endpoints[0].registry || !binding.predicate
        || (bubble!=binding.endpoints[0].registry->bubble && bubble!=binding.region->bubble))return false;
    placement::wire::Batch requested{};
    // Native placement authority is scoped to its current admitted bubble.
    // Off-scope requests are omitted; no retirement or replacement generation
    // is synthesized. Reentry projects the same retained authored generation.
    if(!placement::project(binding.endpoints,bubble,requested))return false;
    auto result=placements;auto merged=names;
    for(std::size_t i=0;i<requested.count;++i) {
        const auto& r=requested.entries[i];
        for(std::size_t j=0;j<result.count;++j)
            if(result.entries[j].registry==r.registry && result.entries[j].slot==r.slot)return false;
        if(result.count==result.entries.size())return false;
        result.entries[result.count++]=r;
    }
    if(!predicates::unite(merged,{&binding.predicate,1}))return false;
    placements=result;names=merged;return true;
}
}
