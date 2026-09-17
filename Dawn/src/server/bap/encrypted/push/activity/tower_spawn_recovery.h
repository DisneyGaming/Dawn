#pragma once
#include "native_roster_lifetime.h"
#include "../../../../../middleware/bap/activity_message/sensor_auth_update.h"
#include "../../../../../state/build_data/scenarios/definition.h"
#include "../../../../../state/activity/tower_spawn_recovery.h"

namespace dawn::server::bap::encrypted::push::activity::tower_spawn_recovery {
namespace request=state::activity::tower_spawn_recovery;
// A retained global remains present in phase one after leaving its authored
// slice. Slice reload reconstructs it, so phase two must still initialize it.
// Otherwise native 4D6530 blocks ALL pending authority, including player spawn.
// The observed Tower blocker is 8A4E2843 (68/0, 11/1, 53/2) on return to slice 0.
template<class Storage,class FindGroup>
inline bool retain_global_bodies(Storage& storage,
    middleware::bap::activity_message::sensor_auth_update::Roster& roster,
    const roster_lifetime::State& prior,state::activity::ActivityInstanceKey activity,
    std::uint32_t scenario,FindGroup findGroup) noexcept {
    if(scenario!=request::kScenario || !prior.identity.owner) {return true;}
    if(prior.identity.scenario!=scenario || prior.identity.owner!=activity.sessionId
        || prior.identity.incarnation!=activity.incarnation.value
        || roster_lifetime::validate(prior)!=roster_lifetime::Result::ready
        || roster.topLevelGroupCount>roster.groupCount || roster.groupCount>roster.groups.size()) {return false;}
    for(std::size_t i=0;i<prior.top.count;++i) {
        if(!prior.top.presence[i]) {continue;}
        const auto key=prior.top.keys[i];std::size_t at{};
        for(;at<roster.groupCount && roster.groups[at].key!=key;++at) {}
        if(at<roster.topLevelGroupCount) {continue;}
        if(at<roster.groupCount || roster.groupCount==roster.groups.size()) {return false;}
        std::size_t backing{};
        for(;backing<storage.rosterGroups.size();++backing) {
            bool used{};
            for(std::size_t g=0;g<roster.groupCount;++g)
                used|=roster.groups[g].slotTypes.data()==storage.rosterGroups[backing].slotTypes.data();
            if(!used) {break;}
        }
        if(backing==storage.rosterGroups.size()) {return false;}
        auto& group=storage.rosterGroups[backing];
        if(!findGroup(key,group) || group.registryKey!=key || !group.slotCount
            || group.slotCount>group.slotTypes.size()) {return false;}
        for(std::size_t g=roster.groupCount;g>roster.topLevelGroupCount;--g)
            roster.groups[g]=roster.groups[g-1];
        roster.groups[roster.topLevelGroupCount++]={key,
            std::span(group.slotTypes).first(group.slotCount),
            std::span(group.slotFlags).first(group.slotCount),
            std::span(group.slotIndices).first(group.slotCount)};
        ++roster.groupCount;
    }
    return true;
}

// This candidate is committed/rolled back together with the enclosing roster
// packet. A failed send cannot consume the request. Only the Tower spawn root
// changes generation; retained vendors and their native actors stay untouched.
inline bool project(roster_lifetime::State& life,state::activity::ActivityInstanceKey activity,
    std::uint64_t run,const request::Request& recovery) noexcept {
    if(!recovery.revision || recovery.activity!=activity || recovery.run!=run
        || recovery.lifetime==UINT32_MAX || life.identity.scenario!=request::kScenario
        || life.identity.owner!=activity.sessionId || life.identity.incarnation!=activity.incarnation.value
        || life.reinitializationRevision>=recovery.revision) {return false;}
    if(roster_lifetime::validate(life)!=roster_lifetime::Result::ready) {return false;}
    for(std::size_t i=0;i<life.top.count;++i) {
        if(life.top.keys[i]!=request::kRoot || !life.top.presence[i]) {continue;}
        life.top.states[i]=static_cast<std::uint8_t>(0x80U+((life.top.states[i]+1U)&0x7FU));
        life.reinitializationRevision=recovery.revision;return true;
    }
    return false;
}
}
