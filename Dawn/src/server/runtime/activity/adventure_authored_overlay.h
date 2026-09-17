#pragma once
#include "registry_admission.h"

namespace dawn::server::runtime::activity::authored_overlay {
// A selected activity can use groups already present in its persistent world's
// slice. This capability only composes authored descriptors; it does not create
// a session, move a player, grant eligibility, or request any component state.
struct GroupIdentity final {
    std::uint32_t key{},object{};
    std::span<const registry::Slot> slots;
};
struct Binding final {
    std::uint32_t hostScenario{},selectedScenario{},bubbleHash{};
    std::string_view selectedPackage;
    std::uint8_t bubble{};
    GroupIdentity root,local;
};
struct Plan final {
    std::array<registry::catalog::RosterGroup,2> groups{};
    std::uint8_t bubble{};
    bool prepared{};
};
enum class Result:std::uint8_t {added,present,invalid,conflict,capacity};

[[nodiscard]] inline bool valid_key(std::uint32_t key) noexcept {
    return key && key!=UINT32_MAX && key!=0x811C9DC5U;
}

[[nodiscard]] inline bool valid(const GroupIdentity& identity) noexcept {
    if(!valid_key(identity.key) || !identity.object || identity.slots.empty()
        || identity.slots.size()>registry::catalog::kRosterSlotCapacity)return false;
    for(std::size_t i=0;i<identity.slots.size();++i) {
        const auto& slot=identity.slots[i];
        if(slot.index>32767 || slot.type>registry::catalog::kMaximumSlotType
            || !slot.componentClass || !slot.descriptorTag)return false;
        for(std::size_t j=0;j<i;++j)if(identity.slots[j].index==slot.index)return false;
    }
    return true;
}

[[nodiscard]] inline bool valid(const registry::catalog::RosterGroup& group) noexcept {
    if(!valid_key(group.registryKey) || !group.objectTag || !group.slotCount
        || group.slotCount>group.slotTypes.size())return false;
    for(std::size_t i=0;i<group.slotCount;++i) {
        if(group.slotIndices[i]>32767 || group.slotTypes[i]>registry::catalog::kMaximumSlotType
            || group.slotFlags[i]>registry::catalog::kSlotFlagMask
            || !group.componentClasses[i] || !group.descriptorTags[i])return false;
        for(std::size_t j=0;j<i;++j)if(group.slotIndices[j]==group.slotIndices[i])return false;
    }
    return true;
}

[[nodiscard]] inline bool matches(const registry::catalog::RosterGroup& group,
                                  const GroupIdentity& identity) noexcept {
    const registry::Definition expected{{},0,identity.key,identity.object,0,0,identity.slots};
    return !identity.slots.empty() && registry::matches(group,expected);
}

// Binding provenance must establish that these same objects exist in the host
// slice. The selected scenario's authored order disambiguates its root/local
// pair; arbitrary co-resident groups are never promoted into an overlay.
template<class FindGroup>
[[nodiscard]] bool prepare(const registry::catalog::Definition& host,
    const registry::catalog::Definition& selected,const Binding& binding,
    FindGroup findGroup,Plan& output) noexcept {
    output={};
    if(!binding.hostScenario || !binding.selectedScenario || !binding.bubbleHash
        || binding.selectedPackage.empty() || binding.bubble>=64
        || !valid(binding.root) || !valid(binding.local)
        || binding.root.key==binding.local.key || binding.root.object==binding.local.object
        || host.tag!=binding.hostScenario || selected.tag!=binding.selectedScenario
        || selected.nameLength>selected.name.size()
        || std::string_view(selected.name.data(),selected.nameLength)!=binding.selectedPackage
        || host.bubbleCount<=binding.bubble || selected.bubbleCount<=binding.bubble
        || host.bubbleHashes[binding.bubble]!=binding.bubbleHash
        || selected.bubbleHashes[binding.bubble]!=binding.bubbleHash
        || selected.authoredGroupCounts[binding.bubble]!=2)return false;
    Plan candidate{};
    for(std::size_t i=0;i<2;++i)
        if(!findGroup(selected.authoredGroups[binding.bubble][i],candidate.groups[i]))return false;
    if(!matches(candidate.groups[0],binding.root) || !matches(candidate.groups[1],binding.local))return false;
    candidate.bubble=binding.bubble;candidate.prepared=true;output=std::move(candidate);return true;
}

[[nodiscard]] inline bool same(const registry::wire::Group& wire,
                              const registry::catalog::RosterGroup& group) noexcept {
    return wire.key==group.registryKey && wire.slotTypes.size()==group.slotCount
        && wire.slotFlags.size()==group.slotCount && wire.slotIndices.size()==group.slotCount
        && std::equal(wire.slotTypes.begin(),wire.slotTypes.end(),group.slotTypes.begin())
        && std::equal(wire.slotFlags.begin(),wire.slotFlags.end(),group.slotFlags.begin())
        && std::equal(wire.slotIndices.begin(),wire.slotIndices.end(),group.slotIndices.begin());
}

// All validation precedes writes. Original backing slots stay in place while
// the new root's wire reference is inserted before the bubble-local references.
// Callers retain Storage through serialization, as with registry::admit.
template<class Storage>
[[nodiscard]] Result append(const Plan& plan,Storage& storage,registry::wire::Roster& roster) noexcept {
    if(!plan.prepared || plan.bubble>=64 || roster.groupCount>roster.groups.size()
        || roster.topLevelGroupCount>roster.groupCount
        || roster.bubbleSubBlocks.size()>storage.rosterSubBlocks.size())return Result::invalid;
    const auto root=plan.groups[0].registryKey,local=plan.groups[1].registryKey;
    if(root==local || !valid(plan.groups[0]) || !valid(plan.groups[1]))return Result::invalid;
    std::size_t rootCount{},localCount{},localKeys{},target=roster.bubbleSubBlocks.size();
    for(std::size_t i=0;i<roster.groupCount;++i) {
        const auto& group=roster.groups[i];
        if(group.key==root) {
            if(i>=roster.topLevelGroupCount || !same(group,plan.groups[0]))return Result::conflict;
            ++rootCount;
        }
        if(group.key==local) {
            if(i<roster.topLevelGroupCount || !same(group,plan.groups[1]))return Result::conflict;
            ++localCount;
        }
    }
    const auto blocks=roster.bubbleSubBlocks;
    for(std::size_t b=0;b<blocks.size();++b) {
        const auto& block=blocks[b];
        if(block.bubble>63 || (!block.presence.empty() && block.presence.size()!=block.keys.size()))return Result::invalid;
        if(block.bubble==plan.bubble) {
            if(target!=blocks.size())return Result::conflict;
            target=b;
        }
        for(std::size_t i=0;i<block.keys.size();++i) {
            if(block.keys[i]==root)return Result::conflict;
            if(block.keys[i]!=local)continue;
            if(block.bubble!=plan.bubble || (!block.presence.empty() && block.presence[i]!=1))return Result::conflict;
            ++localKeys;
        }
    }
    if(rootCount || localCount || localKeys)
        return rootCount==1 && localCount==1 && localKeys==1?Result::present:Result::conflict;
    if(target<blocks.size() && !blocks[target].presence.empty())return Result::conflict;
    const auto keysBefore=target<blocks.size()?blocks[target].keys.size():0;
    const auto count=roster.groupCount,top=roster.topLevelGroupCount;
    if(count+2>roster.groups.size() || count+2>storage.rosterGroups.size()
        || target>=storage.rosterSubBlocks.size() || target>=storage.rosterSubBlockKeys.size()
        || keysBefore>=storage.rosterSubBlockKeys[target].size())return Result::capacity;
    storage.rosterGroups[count]=plan.groups[0];storage.rosterGroups[count+1]=plan.groups[1];
    const auto expose=[](const auto& group) noexcept -> registry::wire::Group {
        return {group.registryKey,std::span(group.slotTypes).first(group.slotCount),
            std::span(group.slotFlags).first(group.slotCount),std::span(group.slotIndices).first(group.slotCount)};
    };
    for(std::size_t i=count;i>top;--i)roster.groups[i]=roster.groups[i-1];
    roster.groups[top]=expose(storage.rosterGroups[count]);
    roster.groups[count+1]=expose(storage.rosterGroups[count+1]);
    for(std::size_t b=0;b<blocks.size();++b)storage.rosterSubBlocks[b]=blocks[b];
    auto& keys=storage.rosterSubBlockKeys[target];
    if(keysBefore && blocks[target].keys.data()!=keys.data())
        std::copy(blocks[target].keys.begin(),blocks[target].keys.end(),keys.begin());
    keys[keysBefore]=local;
    storage.rosterSubBlocks[target]={plan.bubble,std::span(keys).first(keysBefore+1)};
    roster.bubbleSubBlocks=std::span(storage.rosterSubBlocks).first(blocks.size()+(target==blocks.size()?1U:0U));
    roster.groupCount=count+2;roster.topLevelGroupCount=top+1;
    return Result::added;
}
}
