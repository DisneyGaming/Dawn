#pragma once
#include "omega_teardown_native.h"
#include "../../../state/activity/Newlight/launchpad/native_catalog.h"

namespace dawn::client::hooks::bootflow::launchpad_retirement_native {
namespace native=omega_teardown_native;
namespace mission=state::activity::newlight::launchpad;
using native::Source;
using native::Retirement;
using native::read;

// This player-only registry is shared with the public Divide session. Native
// world cleanup can retain its private sync records even though it is catalogued
// inside a bubble rather than among the three top-level groups.
inline constexpr std::uint32_t kSharedPlayers=0xEAAF16E2U;
inline constexpr bool retained_sync_group(const mission::Group& group) noexcept {
    return group.topLevel || group.key==kSharedPlayers;
}
static_assert([] {
    for(const auto& group:mission::kGroups) if(group.key==kSharedPlayers) {
        if(group.slots.size()!=16) {return false;}
        for(const auto& slot:group.slots) {if(slot.asset.type!=13) {return false;}}
        return true;
    }
    return false;
}());
inline constexpr std::size_t kRetainedGroups=4;

// 3CCE50 receives the complete decoded roster. Its applied mirror is context+8;
// owner+28 is the actual native registry list, not a server acknowledgement.
inline bool owner(const Source& source,std::uintptr_t context,Retirement& result) noexcept {
    if(context<0x11208) {return false;}
    const auto address=context-0x11208;
    std::uint32_t handle{},back{},scenario{},mirror{};std::uint8_t active{};
    std::uintptr_t parent{};std::uint64_t identity{};
    if(!read(source,context,handle) || handle==UINT32_MAX
        || !read(source,context+4,mirror) || mirror!=mission::kScenario
        || !read(source,address+0x848,back) || back!=handle
        || native::resolve(source,handle).address!=address
        || !read(source,address+0x24,scenario) || scenario!=mirror
        || !read(source,address+0x20,active) || active!=1
        || !read(source,address+0x10,parent) || !parent
        || !read(source,address+0x18,identity)) {return false;}
    result={address,context,parent,identity,handle,0};return true;
}
inline bool same_owner(const Source& source,const Retirement& prior) noexcept {
    Retirement current{};
    return prior.valid() && owner(source,prior.context,current) && current.owner==prior.owner
        && current.parent==prior.parent && current.identity==prior.identity && current.handle==prior.handle;
}
inline bool groups(const Source& source,std::uintptr_t address,bool retired) noexcept {
    std::int32_t count{};unsigned roots{};
    if(!read(source,address+0x28,count) || count<0 || count>128) {return false;}
    // The next native activation zeroes the entire sync pool, not only the
    // current bubble. Even one retained global head can become a freed link.
    if(retired) {return count==0;}
    for(std::int32_t i=0;i<count;++i) {
        std::uint32_t key{};
        if(!read(source,address+0x2C+static_cast<std::uintptr_t>(i)*16,key)) {return false;}
        if(key==mission::kRoot) {++roots;}
    }
    // The derived/public activity deliberately excludes this mission root.
    return roots==1;
}
inline bool block(const native::RosterBlock& value,unsigned bubble,bool retired) noexcept {
    if(value.words[0]!=bubble) {return false;}
    unsigned count{};std::array<std::uint32_t,3> presence{};
    for(const auto& group:mission::kGroups) if(!group.topLevel && group.bubble==bubble) {
        if(count>=96 || value.words[count+2]!=group.key) {return false;}
        ++count;
    }
    if(value.words[1]!=count || value.words[0x194/4]!=count) {return false;}
    if(retired) for(unsigned i=0;i<3;++i) {if(value.words[0x188/4+i]!=presence[i]) {return false;}}
    return true;
}
inline bool roster(const Source& source,std::uintptr_t address,bool retired) noexcept {
    std::int32_t count{};unsigned top{};
    for(const auto& group:mission::kGroups) if(group.topLevel) {
        std::uint32_t key{};
        if(!read(source,address+4+top*4,key) || key!=group.key) {return false;}
        ++top;
    }
    if(!read(source,address,count) || count!=static_cast<std::int32_t>(top)
        || !read(source,address+0x424,count) || count!=static_cast<std::int32_t>(top)) {return false;}
    if(retired) for(unsigned i=0;i<8;++i) {
        std::uint32_t mask{};
        if(!read(source,address+0x404+i*4,mask) || mask) {return false;}
    }
    if(!read(source,address+0x528,count) || count!=4) {return false;}
    for(unsigned i=0;i<4;++i) {
        native::RosterBlock value{};
        if(!read(source,address+0x52C+i*native::kRosterBlockBytes,value) || !block(value,i,retired)) {return false;}
    }
    return true;
}
inline Retirement begin(const Source& source,std::uintptr_t context,std::uintptr_t delta,
    std::uint8_t movie,unsigned* reason=nullptr) noexcept {
    Retirement result{};
    if(reason) {*reason=1;}
    if(movie<1 || movie>2 || !owner(source,context,result)) {return {};}
    if(reason) {*reason=5;}
    // A retry may arrive after 3CCE50 has removed the mission root. Its exact
    // fully retired catalog still authenticates that private owner, as it does
    // for the second movie. finish/retire_globals must still prove actual cleanup.
    if(movie==1 && !groups(source,result.owner,false) && !roster(source,context+8,true)) {return {};}
    if(reason) {*reason=2;}
    // The first movie already retired the gameplay root. For its authenticated
    // continuation, the complete applied catalog (including that root's exact
    // key and ordinal) still identifies the private owner: public rosters omit
    // it. Actual records must again be empty after native cleanup, below.
    if(!roster(source,context+8,false)) {return {};}
    if(reason) {*reason=3;}
    if(!roster(source,delta,true)) {return {};}
    if(reason) {*reason=4;}
    // Presence changes must retain the exact slot-state versions. Neither
    // omitted groups nor a replacement roster can acknowledge this cleanup.
    unsigned top{};
    for(const auto& group:mission::kGroups) if(group.topLevel) {
        std::uint8_t before{},after{};
        if(!read(source,context+8+0x428+top,before) || !read(source,delta+0x428+top,after)
            || before!=after) {return {};}
        ++top;
    }
    for(unsigned i=0;i<4;++i) {
        native::RosterBlock before{},after{};const auto offset=0x52C+i*native::kRosterBlockBytes;
        if(!read(source,context+8+offset,before) || !read(source,delta+offset,after)) {return {};}
        const auto* oldStates=reinterpret_cast<const std::byte*>(before.words.data())+0x198;
        const auto* newStates=reinterpret_cast<const std::byte*>(after.words.data())+0x198;
        for(unsigned j=0;j<before.words[1];++j) {if(oldStates[j]!=newStates[j]) {return {};}}
    }
    if(!same_owner(source,result)) {return {};}
    if(reason) {*reason=0;}
    return result;
}
inline bool finish(const Source& source,const Retirement& lease) noexcept {
    return same_owner(source,lease) && roster(source,lease.context+8,true)
        && groups(source,lease.owner,true) && same_owner(source,lease);
}
// Shared world registries can outlive this owner's presence change. Their
// private sync records are distinct from the world objects removed by 3CA800.
// Retire those records through native 4D7C00 before C9 clears their pool. Never
// let an unresolved gameplay group, foreign key or broken list qualify.
inline bool globals_only(const Source& source,const Retirement& lease) noexcept {
    if(!same_owner(source,lease) || !roster(source,lease.context+8,true)) {return false;}
    const auto list=lease.owner+0x28;std::int32_t count{};
    if(!read(source,list,count) || count<0 || count>static_cast<std::int32_t>(kRetainedGroups)) {return false;}
    std::array<std::uint32_t,kRetainedGroups> keys{};
    for(std::int32_t i=0;i<count;++i) {
        const auto row=list+4+static_cast<std::uintptr_t>(i)*16;
        std::uint32_t key{},node{},tail{};const mission::Group* group{};
        if(!read(source,row,key) || !read(source,row+4,node) || !read(source,row+8,tail)
            || node==UINT32_MAX || tail==UINT32_MAX) {return false;}
        for(const auto& candidate:mission::kGroups) if(retained_sync_group(candidate) && candidate.key==key) {group=&candidate;}
        if(!group) {return false;}
        for(std::int32_t j=0;j<i;++j) {if(keys[j]==key) {return false;}}keys[i]=key;
        std::uint32_t prior=UINT32_MAX;std::size_t nodes{};
        while(node!=UINT32_MAX) {
            const auto object=native::resolve(source,node);std::uint32_t nodeKey{},previous{},next{};
            if(++nodes>group->slots.size() || !object.address
                || !read(source,object.address,nodeKey) || nodeKey!=key
                || !read(source,object.address+0x74,previous) || previous!=prior
                || !read(source,object.address+0x78,next)
                || !native::can_unregister(source,list,node)) {return false;}
            prior=node;node=next;
        }
        if(prior!=tail) {return false;}
    }
    return same_owner(source,lease);
}
template<class Unregister> bool retire_globals(const Source& source,const Retirement& lease,Unregister unregister) noexcept {
    if(!globals_only(source,lease)) {return false;}
    // Each native removal must shrink the list or advance its head. Bound the
    // work by the authored global slot count; no repaired links or pool writes.
    std::size_t limit{};for(const auto& group:mission::kGroups) if(retained_sync_group(group)) {limit+=group.slots.size();}
    while(limit--) {
        const auto list=lease.owner+0x28;std::int32_t count{};std::uint32_t head{},after{};
        if(!same_owner(source,lease) || !read(source,list,count)) {return false;}
        if(!count) {return finish(source,lease);}
        if(!read(source,list+8,head) || !native::can_unregister(source,list,head)) {return false;}
        unregister(list,head);
        if(!read(source,list,count) || (count && (!read(source,list+8,after) || after==head))) {return false;}
    }
    return finish(source,lease);
}
}
