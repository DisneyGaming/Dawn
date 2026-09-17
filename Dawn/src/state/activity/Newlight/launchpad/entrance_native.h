#pragma once
#include "controller.h"
#include "performances.h"
#include "../../../../client/hooks/bootflow/coo_enemy_readiness.h"

namespace dawn::state::activity::newlight::launchpad::entrance {
// First reveal is the short authored scurry away into the next ambush.
// Keep its full action; the drop-only action belongs to later ceiling Vandals.
inline constexpr std::uint32_t kSequence=0x742289C7U,kBank=0x80FEFAF5U;
struct Binding {
    std::uintptr_t character{},channel{},entry{};
    std::uint32_t characterSelf{},animation{},entity{},runtime{UINT32_MAX};bool started{},playing{};
    friend bool operator==(const Binding&,const Binding&)=default;
};
inline const EnemyReceipt& actor(const Request& r,std::size_t index) noexcept {
    return index?r.frame.ambushActors[index-1]:r.frame.firstVandal;
}
inline bool wanted(const Request& r,std::size_t index=0) noexcept {
    if(index>std::size(kAmbushCues)) {return false;}
    const auto& receipt=actor(r,index);const auto slot=index?kAmbushCues[index-1].source:29;
    const auto sound=index?kAmbushCues[index-1].sound:77;
    const auto& source=r.frame.native[asset_index(asset(kBreach,1,static_cast<std::uint16_t>(slot)))];
    return r.owner.valid() && r.frame.enabled && !r.frame.finished && receipt.valid()
        && r.frame.section==2 && r.frame.cinematic.phase==cinematics::Phase::gameplay
        && receipt.run==r.owner.run && receipt.registry==kBreach && receipt.source==slot
        && !r.frame.native[asset_index(asset(kBreach,5,static_cast<std::uint16_t>(sound)))].active
        && (index || !r.frame.firstVandalStarted)
        && source.active && !source.sourceCleared && source.generation==receipt.generation && source.sourceOwner==receipt.owner;
}
// The first Vandal has no named Type-2 member. Use the same native named
// sequence channel as the Type-2 sequence atom, scoped to its admitted actor.
template<class Read> bool sample(Read& read,std::uintptr_t image,const Request& req,Binding& out,std::size_t index=0) noexcept {
    namespace cn=client::hooks::bootflow::coo_native;
    if(!wanted(req,index)) {return false;}
    const auto& actor=entrance::actor(req,index);const auto ready=cn::enemy(read,image,actor);
    if(!ready.created || !ready.ai || !ready.health) {return false;}
    const auto sequence=index?performances::entrance(asset(kBreach,2,kAmbushCues[index-1].actor)):kSequence;
    const auto definition=index?(sequence==0xFC44A797U?0x80C1C95AU:sequence==0x2AC3BDBAU?0x80C1C95CU:
        sequence==0x13B6A591U?0x80C1C958U:0x80C1B511U):0x80C1B50FU;
    Binding b{};std::uintptr_t actors{},entities{},state{},names{},resolved{};std::uint32_t stride{},bundle{},value{},bank{};
    if(!read.value(image+0x1F9D7F8,actors) || !read.value(image+0x1F9D800,stride) || stride<0x70 || stride>0x100000
        || !read.value(actors+static_cast<std::uintptr_t>(actor.actor&0x1FFFU)*stride+0x4C,b.entity)
        || !read.value(image+0x1F93428,entities) || !read.value(image+0x1F93430,stride) || stride<0x50 || stride>0x100000
        || !read.value(entities+static_cast<std::uintptr_t>(b.entity&0x1FFFU)*stride+0x4C,bundle)
        || !cn::component<Read,1024>(read,bundle,b.entity,0x80806832U,b.character)
        || !read.value(b.character+0x24,b.characterSelf) || !read.resolve(b.characterSelf,resolved) || resolved!=b.character
        || !read.value(b.character+0xC0,value) || value!=actor.actor
        || !read.value(b.character+0x5C0,b.animation) || !read.resolve(b.animation,state)
        || !read.value(state+4,bank) || (!index && bank!=kBank)
        || !read.value(state+0x30,value) || value!=b.characterSelf || !read.resolve(bank,names)) {return false;}
    std::uint64_t count{},relative{};
    if(!read.value(names+0x10,count) || !count || count>64 || !read.value(names+0x18,relative) || relative>0x100000) {return false;}
    for(std::size_t i=0;i<count;++i) {
        const auto entry=names+0x28+relative+i*24;
        if(!read.value(entry,value)) {return false;}
        if(value==sequence) {if(b.entry) {return false;}b.entry=entry;}
    }
    if(!b.entry || !read.value(b.entry+4,value) || value!=2
        || !read.value(b.entry+16,value) || value!=definition) {return false;}
    b.channel=state+0x1980;
    if(!read.value(b.channel,value) || value!=b.characterSelf || !read.value(b.channel+4,value) || value!=b.animation) {return false;}
    bool available{};unsigned matches{};
    for(unsigned i=0;i<8;++i) {
        if(!read.value(b.channel+0x20+i*0x108,value)) {return false;}
        available|=value==0x811C9DC5U;
        if(value==sequence) {
            ++matches;
            // C6F910 -> E94F70 -> 58B3D0: an allocated channel key is not
            // playback. Its .tft instance must have live actions and no stop bit.
            if(!read.value(b.channel+0x34+i*0x108,b.runtime)) {return false;}
            if(b.runtime!=UINT32_MAX) {
                std::int32_t active{},pending{};std::uint8_t flags{};
                if(!read.resolve(b.runtime,resolved) || !read.value(resolved+0xE0,active)
                    || !read.value(resolved+0xF8,pending) || !read.value(resolved+0x246,flags)) {return false;}
                b.playing=(active>0 || pending>0) && !(flags&0x10U);
            }
        }
    }
    if(matches>1 || (!available && !matches)) {return false;}b.started=matches==1;out=b;return true;
}
}
