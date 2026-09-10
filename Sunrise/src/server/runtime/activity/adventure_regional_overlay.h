#pragma once
#include "adventure_authored_overlay.h"

namespace sunrise::server::runtime::activity::authored_overlay {
// Region entry can inherit a selected global root from the preceding slice.
// Admit only its exact, independently prepared local group. This does not
// retire the preceding region, publish authority, or authorize movement.
template<class Storage>
[[nodiscard]] Result append_region(const Plan& plan,Storage& storage,registry::wire::Roster& roster) noexcept {
    if(!plan.prepared || plan.bubble>=64 || !valid(plan.groups[0]) || !valid(plan.groups[1])
        || plan.groups[0].registryKey==plan.groups[1].registryKey
        || roster.groupCount>roster.groups.size() || roster.topLevelGroupCount>roster.groupCount
        || roster.bubbleSubBlocks.size()>storage.rosterSubBlocks.size())return Result::invalid;
    unsigned roots{},locals{},localKeys{};std::size_t target=roster.bubbleSubBlocks.size();
    for(std::size_t i=0;i<roster.groupCount;++i) {
        const auto& group=roster.groups[i];
        if(group.key==plan.groups[0].registryKey) {
            if(i>=roster.topLevelGroupCount || !same(group,plan.groups[0]))return Result::conflict;
            ++roots;
        }
        if(group.key==plan.groups[1].registryKey) {
            if(i<roster.topLevelGroupCount || !same(group,plan.groups[1]))return Result::conflict;
            ++locals;
        }
    }
    const auto blocks=roster.bubbleSubBlocks;
    for(std::size_t b=0;b<blocks.size();++b) {
        const auto& block=blocks[b];
        if(block.bubble>=64 || (!block.presence.empty() && block.presence.size()!=block.keys.size()))return Result::invalid;
        if(block.bubble==plan.bubble) {if(target!=blocks.size())return Result::conflict;target=b;}
        for(std::size_t i=0;i<block.keys.size();++i) {
            if(block.keys[i]==plan.groups[0].registryKey)return Result::conflict;
            if(block.keys[i]!=plan.groups[1].registryKey)continue;
            if(block.bubble!=plan.bubble || (!block.presence.empty() && block.presence[i]!=1))return Result::conflict;
            ++localKeys;
        }
    }
    if(!roots)return append(plan,storage,roster);
    if(roots!=1)return Result::conflict;
    if(locals || localKeys)return locals==1 && localKeys==1?Result::present:Result::conflict;
    if(target<blocks.size() && !blocks[target].presence.empty())return Result::conflict;
    const auto before=target<blocks.size()?blocks[target].keys.size():0;
    if(roster.groupCount>=roster.groups.size() || roster.groupCount>=storage.rosterGroups.size()
        || target>=storage.rosterSubBlocks.size() || target>=storage.rosterSubBlockKeys.size()
        || before>=storage.rosterSubBlockKeys[target].size())return Result::capacity;
    auto& group=storage.rosterGroups[roster.groupCount];group=plan.groups[1];
    for(std::size_t b=0;b<blocks.size();++b)storage.rosterSubBlocks[b]=blocks[b];
    auto& keys=storage.rosterSubBlockKeys[target];
    if(before && blocks[target].keys.data()!=keys.data())std::copy(blocks[target].keys.begin(),blocks[target].keys.end(),keys.begin());
    keys[before]=group.registryKey;
    storage.rosterSubBlocks[target]={plan.bubble,std::span(keys).first(before+1)};
    roster.bubbleSubBlocks=std::span(storage.rosterSubBlocks).first(blocks.size()+(target==blocks.size()?1U:0U));
    roster.groups[roster.groupCount++]={group.registryKey,std::span(group.slotTypes).first(group.slotCount),
        std::span(group.slotFlags).first(group.slotCount),std::span(group.slotIndices).first(group.slotCount)};
    return Result::added;
}
}
