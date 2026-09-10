#pragma once
#include "adventure_authored_overlay.h"
#include "adventure_opening_runtime.h"
#include <optional>

namespace sunrise::server::runtime::activity::adventure {
// The opening shares one world publication with other authored cue services.
// Preserve their complete requests and reject a conflicting owner of this slot.
[[nodiscard]] inline bool append_opening_cue(cue_feedback::wire::Batch& batch,
    const cue_feedback::wire::Request& request) noexcept {
    if(batch.count>batch.entries.size() || !cue_feedback::wire::valid(request))return false;
    for(std::size_t i=0;i<batch.count;++i)
        if(batch.entries[i].registry==request.registry && batch.entries[i].slot==request.slot)
            return batch.entries[i]==request;
    if(batch.count==batch.entries.size())return false;
    batch.entries[batch.count++]=request;return true;
}
// Shared descriptor proof; callers separately establish initial arrival or an
// observed regional handoff under an already accepted opening lease.
[[nodiscard]] inline std::optional<std::uint32_t> admitted_lifetime_scenario(
    const authored_overlay::Binding& binding,const authored_overlay::Plan& plan,
    const registry::wire::Roster& roster) noexcept {
    if(!plan.prepared || plan.bubble!=binding.bubble || binding.bubble>=64
        || !authored_overlay::valid(binding.root) || !authored_overlay::valid(binding.local)
        || !authored_overlay::matches(plan.groups[0],binding.root)
        || !authored_overlay::matches(plan.groups[1],binding.local)
        || roster.groupCount>roster.groups.size() || roster.topLevelGroupCount>roster.groupCount)return {};
    unsigned roots{},locals{},localKeys{},lifetimes{};
    for(std::size_t i=0;i<roster.groupCount;++i) {
        const auto& group=roster.groups[i];
        if(group.key==binding.root.key) {
            if(i>=roster.topLevelGroupCount || !authored_overlay::same(group,plan.groups[0]))return {};
            ++roots;
        }
        if(group.key==binding.local.key) {
            if(i<roster.topLevelGroupCount || !authored_overlay::same(group,plan.groups[1]))return {};
            ++locals;
        }
        // This is the existing shared lifetime codec's exact authored route,
        // not a request to synthesize or relocate a phase component.
        if(group.key==0x4786C0E0) {
            if(i>=roster.topLevelGroupCount || group.slotTypes.size()!=group.slotFlags.size()
                || group.slotTypes.size()!=group.slotIndices.size())return {};
            for(std::size_t s=0;s<group.slotTypes.size();++s)
                if(group.slotTypes[s]==17 && group.slotIndices[s]==3 && (group.slotFlags[s]&2))++lifetimes;
        }
    }
    for(const auto& block:roster.bubbleSubBlocks) {
        if(!block.presence.empty() && block.presence.size()!=block.keys.size())return {};
        for(std::size_t i=0;i<block.keys.size();++i) {
            if(block.keys[i]==binding.root.key)return {};
            if(block.keys[i]!=binding.local.key)continue;
            if(block.bubble!=binding.bubble || (!block.presence.empty() && block.presence[i]!=1))return {};
            ++localKeys;
        }
    }
    if(roots!=1 || locals!=1 || localKeys!=1 || lifetimes!=1)return {};
    return binding.bubble;
}
// The native directive provider reads type17 authority+C as a scenario ordinal.
// Initial admission still requires both current region and actual destination.
[[nodiscard]] inline std::optional<std::uint32_t> opening_lifetime_scenario(
    const authored_overlay::Binding& binding,const authored_overlay::Plan& plan,
    const registry::wire::Roster& roster,std::int32_t currentRegion,
    std::uint32_t destinationArrival) noexcept {
    if(binding.bubble>=64 || currentRegion!=static_cast<std::int32_t>(binding.bubble)*8
        || destinationArrival!=static_cast<std::uint32_t>(binding.bubble)*8)return {};
    return admitted_lifetime_scenario(binding,plan,roster);
}
// A native portal changes membership region while the same host retains its
// launch arrival and spawn contract. Never substitute currentRegion as a fake
// destinationArrival to pass the initial-arrival contract above. D6's separate
// hash is a native route/arrival selector (E2B120), not the layout bubble hash.
// The accepted lease and exact admitted scenario root/local qualify the region.
[[nodiscard]] inline std::optional<std::uint32_t> regional_lifetime_scenario(
    const OpeningFrame& opening,const authored_overlay::Plan& plan,
    const registry::wire::Roster& roster,std::int32_t currentRegion,
    std::int32_t reportedRegion,std::uint32_t destinationArrival) noexcept {
    if(!opening.binding || !opening.binding->overlay || !opening.binding->gateway
        || !opening.requested || !opening.nativeReady || !opening.dialogueSubmitted || !opening.gatewayRequested || opening.conflictingSelection)return {};
    const auto& initial=*opening.binding->overlay;const auto& gate=*opening.binding->gateway;
    if(!gateway::valid(gate,initial) || !gate.region
        || currentRegion!=reportedRegion || currentRegion!=static_cast<std::int32_t>(gate.region->bubble)*8
        || destinationArrival!=static_cast<std::uint32_t>(initial.bubble)*8)return {};
    return admitted_lifetime_scenario(*gate.region,plan,roster);
}
}
