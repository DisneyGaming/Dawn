#pragma once
#include "../../../../../state/activity/coo/native_asset_catalog.h"
#include "../../../../../state/build_data/scenarios/definition.h"
#include "../../../../../middleware/bap/activity_message/sensor_auth_update.h"
#include "native_roster_lifetime.h"
#include "../../../../../state/activity/vendors/presentation.h"

namespace sunrise::server::bap::encrypted::push::activity::vendor_roster {
namespace welcome=state::activity::vendors::presentation;
namespace native=state::activity::coo::native_catalog;
namespace wire=middleware::bap::activity_message::sensor_auth_update;
inline void recovered(state::build_data::scenarios::RosterGroup& out,const native::Group& group) noexcept {
    out={};out.registryKey=group.key;out.objectTag=group.tag;out.slotCount=static_cast<std::uint16_t>(group.slots.size());
    for(std::size_t i=0;i<group.slots.size();++i) {
        const auto& s=group.slots[i];out.slotTypes[i]=static_cast<std::uint8_t>(s.asset.type);out.slotIndices[i]=s.asset.slot;out.slotFlags[i]=native::slot_flags(s);
        out.descriptorTags[i]=s.asset.definition;out.descriptorOffsets[i]=s.offset;out.componentClasses[i]=s.component;out.senseSchemas[i]=s.sense;out.authSchemas[i]=s.authority;
    }
}
inline void retain(roster_lifetime::State& life,std::uint32_t scenario) noexcept {
    // Join warm-up may rebuild players. A persistent vendor registry keeps its
    // native generation so it cannot create a new source beside its old NPC.
    for(std::size_t b=0;b<life.blockCount;++b) {
        auto& entries=life.blocks[b].entries;
        for(std::size_t i=0;i<entries.count;++i)
            if(welcome::owns(entries.keys[i],scenario)) {entries.states[i]=wire::kStateByteBias+1;}
    }
}
template<class Storage,class FindGroup> bool admit(Storage& storage,wire::Roster& roster,
    std::uint32_t scenario,FindGroup findGroup) noexcept {
    if(!roster.playerKeyGroup || roster.groupCount>roster.groups.size()) {return false;}
    for(const auto& wanted:welcome::groups(scenario)) {
        state::build_data::scenarios::RosterGroup resolved{};
        if(!findGroup(wanted.key,resolved)) {
            // The Tower bindings already include every sync-bearing descriptor.
            // Destination bindings select only the vendor controls; a partial
            // fallback leaves member/sensor records unseeded and cannot spawn.
            if(scenario!=welcome::kTower) {return false;}
            recovered(resolved,wanted);
        }
        if(resolved.registryKey!=wanted.key || resolved.objectTag!=wanted.tag
            || !resolved.slotCount || resolved.slotCount>resolved.slotTypes.size()) {return false;}
        for(const auto& expected:wanted.slots) {
            std::size_t count{};
            for(std::size_t i=0;i<resolved.slotCount;++i) if(resolved.slotIndices[i]==expected.asset.slot) {
                ++count;
                if(resolved.slotTypes[i]!=expected.asset.type
                    || resolved.slotFlags[i]!=native::slot_flags(expected)
                    || resolved.descriptorTags[i]!=expected.asset.definition
                    || resolved.descriptorOffsets[i]!=expected.offset
                    || resolved.componentClasses[i]!=expected.component
                    || resolved.senseSchemas[i]!=expected.sense
                    || resolved.authSchemas[i]!=expected.authority) {return false;}
            }
            if(count!=1) {return false;}
        }
        std::size_t at=roster.groupCount;
        for(std::size_t i=roster.topLevelGroupCount;i<roster.groupCount;++i)
            if(roster.groups[i].key==wanted.key) {at=i;break;}
        if(at>=roster.groups.size()) {return false;}
        // fill_roster can insert an authored root before the ordinary locals.
        // Its wire order then differs from the backing storage order.
        std::size_t backing=storage.rosterGroups.size();
        for(std::size_t i=0;i<storage.rosterGroups.size();++i) {
            const auto* data=storage.rosterGroups[i].slotTypes.data();
            if(at<roster.groupCount) {
                if(roster.groups[at].slotTypes.data()==data) {backing=i;break;}
            } else {
                bool used{};
                for(std::size_t g=0;g<roster.groupCount;++g) {used|=roster.groups[g].slotTypes.data()==data;}
                if(!used) {backing=i;break;}
            }
        }
        if(backing==storage.rosterGroups.size()) {return false;}
        auto& group=storage.rosterGroups[backing];
        if(at==roster.groupCount) {group=resolved;}
        else {
            // Destination registries may also contain unrelated world objects.
            // Merge vendor descriptors without dropping or reordering those slots.
            if(group.registryKey!=wanted.key || group.slotCount>group.slotTypes.size()) {return false;}
            for(std::size_t s=0;s<resolved.slotCount;++s) {
                std::size_t i{};for(;i<group.slotCount;++i)
                    if(group.slotIndices[i]==resolved.slotIndices[s]) {break;}
                if(i>=group.slotTypes.size()) {return false;}
                if(i<group.slotCount && (group.slotTypes[i]!=resolved.slotTypes[s]
                    || group.descriptorTags[i]!=resolved.descriptorTags[s])) {return false;}
                if(i==group.slotCount) {++group.slotCount;}
                group.slotTypes[i]=resolved.slotTypes[s];group.slotIndices[i]=resolved.slotIndices[s];
                group.slotFlags[i]=resolved.slotFlags[s];group.descriptorTags[i]=resolved.descriptorTags[s];
                group.descriptorOffsets[i]=resolved.descriptorOffsets[s];group.componentClasses[i]=resolved.componentClasses[s];
                group.senseSchemas[i]=resolved.senseSchemas[s];group.authSchemas[i]=resolved.authSchemas[s];
            }
        }
        roster.groups[at]={group.registryKey,std::span(group.slotTypes).first(group.slotCount),
            std::span(group.slotFlags).first(group.slotCount),std::span(group.slotIndices).first(group.slotCount)};
        if(at==roster.groupCount) {++roster.groupCount;}
    }
    // Keep cached globals and navigation. Add each vendor registry exactly once under
    // its authored bubble, including when the player returns to an earlier area.
    auto count=roster.bubbleSubBlocks.size();
    if(count>storage.rosterSubBlocks.size()) {return false;}
    for(const auto& wanted:welcome::groups(scenario)) {
        std::size_t at=count;
        for(std::size_t i=0;i<count;++i) if(storage.rosterSubBlocks[i].bubble==wanted.bubble) {at=i;break;}
        if(at>=storage.rosterSubBlocks.size()) {return false;}
        if(at==count) {storage.rosterSubBlocks[count++]={wanted.bubble,{}};}
        auto& block=storage.rosterSubBlocks[at];auto& keys=storage.rosterSubBlockKeys[at];
        const auto used=block.keys.size();if(used>keys.size()) {return false;}
        bool found{};for(const auto key:block.keys) if(key==wanted.key) {found=true;}
        if(!found) {
            if(used==keys.size()) {return false;}
            std::copy(block.keys.begin(),block.keys.end(),keys.begin());keys[used]=wanted.key;block.keys=std::span(keys).first(used+1);
        }
    }
    roster.bubbleSubBlocks=std::span(storage.rosterSubBlocks).first(count);return true;
}
}
