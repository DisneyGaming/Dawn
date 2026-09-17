#pragma once
#include "../../../middleware/bap/activity_message/sensor_auth_update.h"
#include "../../../state/build_data/scenarios/definition.h"
#include "../../../state/activity/coo/authored_registry.h"
#include <algorithm>
#include <string_view>

namespace sunrise::server::runtime::activity::registry {
namespace catalog = state::build_data::scenarios;
namespace wire = middleware::bap::activity_message::sensor_auth_update;

using state::activity::coo::registry::Slot;
using state::activity::coo::registry::Definition;
enum class Admission : std::uint8_t {
    unrelated, added, present, invalid, missingLayout, missingGroup, schemaMismatch, conflict, noCapacity
};

[[nodiscard]] inline bool valid(const Definition& definition) noexcept {
    if(definition.activity.empty() || !definition.scenario || !definition.key
        || definition.key==UINT32_MAX || definition.key==0x811C9DC5U
        || !definition.objectTag || !definition.bubbleHash || definition.bubble>63
        || definition.slots.empty() || definition.slots.size()>catalog::kRosterSlotCapacity) { return false; }
    for(std::size_t i=0;i<definition.slots.size();++i) {
        const auto& slot=definition.slots[i];
        if(slot.index>32767 || slot.type>catalog::kMaximumSlotType
            || !slot.componentClass || !slot.descriptorTag) { return false; }
        for(std::size_t j=0;j<i;++j) { if(definition.slots[j].index==slot.index) { return false; } }
    }
    return true;
}

[[nodiscard]] inline bool matches(const catalog::RosterGroup& group,const Definition& expected) noexcept {
    if(group.registryKey!=expected.key || group.objectTag!=expected.objectTag
        || group.slotCount!=expected.slots.size() || group.slotCount>group.slotTypes.size()) { return false; }
    // Descriptor extraction order may change. Slot identity and the complete set may not.
    for(const auto& slot:expected.slots) {
        std::size_t found{};
        for(std::size_t i=0;i<group.slotCount;++i) {
            if(group.slotIndices[i]!=slot.index) { continue; }
            ++found;
            if(group.slotTypes[i]!=slot.type || group.slotFlags[i]!=slot.flags()
                || group.componentClasses[i]!=slot.componentClass || group.senseSchemas[i]!=slot.senseSchema
                || group.authSchemas[i]!=slot.authSchema || group.descriptorTags[i]!=slot.descriptorTag) { return false; }
        }
        if(found!=1) { return false; }
    }
    return true;
}

[[nodiscard]] inline bool matches(const wire::Group& group,const Definition& expected) noexcept {
    if(group.key!=expected.key || group.slotTypes.size()!=expected.slots.size()
        || group.slotFlags.size()!=group.slotTypes.size() || group.slotIndices.size()!=group.slotTypes.size()) { return false; }
    for(const auto& slot:expected.slots) {
        std::size_t found{};
        for(std::size_t i=0;i<group.slotIndices.size();++i) {
            if(group.slotIndices[i]!=slot.index) { continue; }
            ++found;
            if(group.slotTypes[i]!=slot.type || group.slotFlags[i]!=slot.flags()) { return false; }
        }
        if(found!=1) { return false; }
    }
    return true;
}

// Server admission only: this publishes authored descriptors, not spawn/activation
// policy or a native-readiness receipt. Caller retains storage through serialization.
// On failure the roster and its backing storage are unchanged.
template<class Storage,class FindGroup>
[[nodiscard]] Admission admit(const catalog::Definition& layout,Storage& storage,
    wire::Roster& roster,const Definition& expected,FindGroup findGroup) noexcept {
    if(!valid(expected)) { return Admission::invalid; }
    if(layout.nameLength>layout.name.size() || layout.tag!=expected.scenario
        || std::string_view(layout.name.data(),layout.nameLength)!=expected.activity) { return Admission::unrelated; }
    if(layout.bubbleCount<=expected.bubble || layout.bubbleHashes[expected.bubble]!=expected.bubbleHash) {
        return Admission::missingLayout;
    }
    if(roster.groupCount>roster.groups.size() || roster.topLevelGroupCount>roster.groupCount
        || roster.bubbleSubBlocks.size()>storage.rosterSubBlocks.size()) {
        return Admission::invalid;
    }

    // Top-level definitions describe roots already published by the native
    // roster. They are never extracted into the activity bubble or appended
    // here; the bubble remains the arrival context for the definition.
    if(expected.topLevel) {
        if(roster.topLevelKeys.empty()
            && (!roster.topLevelPresence.empty() || !roster.topLevelStates.empty())) {
            return Admission::invalid;
        }
        if(!roster.topLevelKeys.empty()) {
            if((!roster.topLevelPresence.empty()
                    && roster.topLevelPresence.size()!=roster.topLevelKeys.size())
                || (!roster.topLevelStates.empty()
                    && roster.topLevelStates.size()!=roster.topLevelKeys.size())) {
                return Admission::invalid;
            }
            for(const auto present:roster.topLevelPresence) if(present>1) return Admission::invalid;
            for(const auto state:roster.topLevelStates) if(state<0x80U) return Admission::invalid;
        }

        std::size_t found=roster.groupCount;
        unsigned groups{};
        for(std::size_t i=0;i<roster.groupCount;++i) {
            if(roster.groups[i].key!=expected.key) continue;
            found=i;
            ++groups;
        }
        if(groups>1) return Admission::conflict;
        if(groups==0) return Admission::missingGroup;
        if(found>=roster.topLevelGroupCount) return Admission::conflict;

        for(const auto& block:roster.bubbleSubBlocks) {
            if(!block.presence.empty() && block.presence.size()!=block.keys.size()) {
                return Admission::invalid;
            }
            for(const auto key:block.keys) if(key==expected.key) return Admission::conflict;
        }

        if(!roster.topLevelKeys.empty()) {
            unsigned keys{};
            std::size_t ordinal{};
            for(std::size_t i=0;i<roster.topLevelKeys.size();++i) {
                if(roster.topLevelKeys[i]!=expected.key) continue;
                ordinal=i;
                ++keys;
            }
            if(keys!=1 || (!roster.topLevelPresence.empty()
                && roster.topLevelPresence[ordinal]!=1)) return Admission::conflict;
        }

        catalog::RosterGroup resolved{};
        if(!findGroup(expected.key,resolved)) return Admission::missingGroup;
        if(!matches(resolved,expected)) return Admission::schemaMismatch;
        if(!matches(roster.groups[found],expected)) return Admission::conflict;
        return Admission::present;
    }

    const auto blocks=roster.bubbleSubBlocks;
    std::size_t target=blocks.size(), keyCount{}, groupCount{};
    for(std::size_t b=0;b<blocks.size();++b) {
        const auto& block=blocks[b];
        if(!block.presence.empty() && block.presence.size()!=block.keys.size()) { return Admission::invalid; }
        if(block.bubble==expected.bubble) {
            if(target!=blocks.size()) { return Admission::conflict; }
            target=b;
        }
        for(std::size_t i=0;i<block.keys.size();++i) {
            if(block.keys[i]!=expected.key) { continue; }
            if(block.bubble!=expected.bubble || (!block.presence.empty() && block.presence[i]!=1)) { return Admission::conflict; }
            ++keyCount;
        }
    }
    for(std::size_t i=0;i<roster.groupCount;++i) {
        if(roster.groups[i].key!=expected.key) { continue; }
        if(!matches(roster.groups[i],expected)) { return Admission::conflict; }
        ++groupCount;
    }
    if((keyCount || groupCount) && (keyCount!=1 || groupCount!=1)) { return Admission::conflict; }

    // Validate provenance even when the wire group is already present. A cached
    // schema mismatch is not repaired by synthesizing descriptors from this table.
    catalog::RosterGroup resolved{};
    if(!findGroup(expected.key,resolved)) { return Admission::missingGroup; }
    if(!matches(resolved,expected)) { return Admission::schemaMismatch; }
    if(groupCount==1) { return Admission::present; }
    const auto keysBefore=target<blocks.size()?blocks[target].keys.size():0;
    // An existing explicit presence list owns removal state. Do not discard it
    // while appending; this admission service deliberately refuses that operation.
    if(target<blocks.size() && !blocks[target].presence.empty()) { return Admission::conflict; }
    if(roster.groupCount>=roster.groups.size() || roster.groupCount>=storage.rosterGroups.size()
        || target>=storage.rosterSubBlocks.size() || target>=storage.rosterSubBlockKeys.size()
        || keysBefore>=storage.rosterSubBlockKeys[target].size()) { return Admission::noCapacity; }
    auto& stored=storage.rosterGroups[roster.groupCount];
    stored=resolved;
    // Preserve all other sub-blocks if the input spans were externally owned.
    for(std::size_t i=0;i<blocks.size();++i) { storage.rosterSubBlocks[i]=blocks[i]; }
    auto& keys=storage.rosterSubBlockKeys[target];
    if(keysBefore && blocks[target].keys.data()!=keys.data()) {
        std::copy(blocks[target].keys.begin(),blocks[target].keys.end(),keys.begin());
    }
    keys[keysBefore]=expected.key;
    storage.rosterSubBlocks[target]={expected.bubble,std::span(keys).first(keysBefore+1)};
    roster.groups[roster.groupCount++]={stored.registryKey,
        std::span(stored.slotTypes).first(stored.slotCount),
        std::span(stored.slotFlags).first(stored.slotCount),
        std::span(stored.slotIndices).first(stored.slotCount)};
    roster.bubbleSubBlocks=std::span(storage.rosterSubBlocks).first(blocks.size()+(target==blocks.size()?1U:0U));
    return Admission::added;
}
} // namespace sunrise::server::runtime::activity::registry
