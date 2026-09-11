#pragma once
#include "strike_bond_intro_release.h"
namespace sunrise::client::hooks::bootflow::strike_bond_boss_cycle {
namespace mission=state::activity::strike_bond;
namespace trace=strike_bond_fire_trace;
inline constexpr std::uint32_t kGroup=0xAFB11A12U,kSleep=0x8FB6C339U,kDeath=0xFC3C74B9U;
inline constexpr std::uint32_t kGate1=0x1B8AB5A9U,kGate2=0x71E9ABF9U,kBurst=0x80F45BA6U;
inline std::array<std::byte,128> action(std::uint32_t sequence,std::uint32_t gate=0) noexcept {
    std::array<std::byte,128> out{};const std::uint32_t absent=UINT32_MAX;
    std::memcpy(out.data(),&kGroup,4);std::memcpy(out.data()+4,&sequence,4);std::memcpy(out.data()+8,&gate,4);
    std::memcpy(out.data()+12,&absent,4);std::memcpy(out.data()+16,&absent,4);
    out[0x60]=static_cast<std::byte>(gate?0x5E:0x5D);return out;
}
struct Binding {
    trace::Identity character{};
    std::uintptr_t controller{},selector{},animator{},health{},device{};
    std::uint32_t controllerSelf{},selectorSelf{},animatorSelf{},healthSelf{},deviceSelf{};
    friend bool operator==(const Binding&,const Binding&)=default;
};
inline bool wanted(const mission::BossRequest& r) noexcept {
    return trace::admitted(r) && r.frame.bossFighting && !r.frame.ending && r.frame.region==136
        && r.platform.valid() && r.platform.source==mission::kBossPlatform && r.platform.owner.run==r.owner.run;
}
template<class Read> bool sample(Read& read,std::uintptr_t image,std::uintptr_t character,const mission::BossRequest& r,Binding& out) noexcept {
    if(!wanted(r)) return false;
    Binding b{};std::uintptr_t rows{},row{},plate{};std::uint32_t stride{},bundle{},value{},flags{};
    if(!trace::sample(read,image,character,false,r,b.character)
        || !read.value(image+0x1F93428,rows) || !read.value(image+0x1F93430,stride) || stride<0xE0 || stride>0x1000) return false;
    row=rows+static_cast<std::uintptr_t>(b.character.entity&0x1FFFU)*stride;
    plate=rows+static_cast<std::uintptr_t>(r.platform.entity&0x1FFFU)*stride;
    if(!read.value(row+0x3C,value) || value!=r.platform.entity || !read.value(row+0x38,value) || value!=1
        || !read.value(row+0x4C,bundle)) return false;
    if(!coo_native::component<Read,1024>(read,bundle,b.character.entity,0x80806832U,b.controller)
        || !coo_native::component<Read,1024>(read,bundle,b.character.entity,0x8080686CU,b.selector)
        || !coo_native::component<Read,1024>(read,bundle,b.character.entity,0x808036CFU,b.animator)
        || !coo_native::component<Read,1024>(read,bundle,b.character.entity,0x80804B8AU,b.health)) return false;
    if(!strike_bond_intro_release::component_header(read,b.controller,0x80F66F56U,0x80806832U,0x6E8,b.character.entity,b.controllerSelf)
        || !strike_bond_intro_release::component_header(read,b.selector,0x80F459AAU,0x8080686CU,0x1C8,b.character.entity,b.selectorSelf)
        || !strike_bond_intro_release::component_header(read,b.animator,0x80F66F51U,0x808036CFU,0x1FF0,b.character.entity,b.animatorSelf)
        || !strike_bond_intro_release::component_header(read,b.health,0x815B5A47U,0x80804B8AU,0xF28,b.character.entity,b.healthSelf)
        || !read.value(b.controller+0xC0,value) || value!=r.enemy.actor
        || !read.value(b.selector+0x30,value) || value!=b.controllerSelf
        || !read.value(b.controller+0x2E8,value) || value!=b.healthSelf
        || !read.value(b.controller+0x2EC,value) || value!=0x80804BEEU) return false;
    std::uint64_t offset{},count{},relative{};
    if(!read.value(b.controller+0x2F0,offset) || offset
        || !read.value(b.selector+0x40,count) || count!=3 || !read.value(b.selector+0x48,relative) || relative!=0x28) return false;
    for(unsigned i=0;i<3;++i) {
        const auto a=b.selector+0x80+i*64;
        if(!read.value(a,value) || value!=0x80F459AAU || !read.value(a+4,value) || value!=0x8080300DU
            || !read.value(a+0x34,flags) || flags>1) return false;
    }
    // The named group must resolve to this selector, not merely another component on the same NPC.
    if(!read.value(b.controller+0x580,count) || count!=1 || !read.value(b.controller+0x588,relative) || relative>0x100000) return false;
    const auto groupBinding=b.controller+0x588+relative+0x40;
    if(!read.value(groupBinding,value) || value!=0x80BFDE65U || !read.value(groupBinding+8,value) || value!=b.selectorSelf
        || !read.value(groupBinding+12,value) || value!=0x8080686BU || !read.value(groupBinding+16,offset) || offset) return false;
    std::uintptr_t state{},names{};
    if(!read.value(b.controller+0x5C0,value) || !read.resolve(value,state)
        || !read.value(state+0x30,value) || value!=b.controllerSelf || !read.value(state+4,value) || value!=0x80F459CDU
        || !read.resolve(value,names) || !read.value(names+0x20,count) || count!=1
        || !read.value(names+0x28,relative) || relative>0x100000) return false;
    const auto group=names+0x38+relative;
    if(!read.value(group+12,value) || value!=kGroup || !read.value(group+16,count) || count!=3
        || !read.value(group+24,relative) || relative>0x100000
        || !read.value(group+40+relative,value) || value!=kSleep
        || !read.value(group+48+relative,value) || value!=kDeath) return false;
    const auto& mount=r.frame.native[mission::asset_index(mission::kBossPlatform)];
    if(!mount.active || !mount.acknowledged || r.platform.owner.value!=mount.generation
        || !read.entity_row({r.platform.serial,r.platform.entity},plate)
        || !read.value(plate+12,value) || value!=r.platform.entity || !read.value(plate+4,flags) || (flags&5U)
        || !read.value(plate+0x4C,bundle)
        || !coo_native::component<Read,1024>(read,bundle,r.platform.entity,0x80803910U,b.device)
        || !strike_bond_intro_release::component_header(read,b.device,0x80F45988U,0x80803910U,0xA78,r.platform.entity,b.deviceSelf)) return false;
    out=b;return true;
}
}
