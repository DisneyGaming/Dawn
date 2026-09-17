#pragma once
#include "strike_bond_fire_trace.h"
#include "coo_native_components.h"
namespace dawn::client::hooks::bootflow::strike_bond_intro_release {
namespace trace=strike_bond_fire_trace;
namespace mission=state::activity::strike_bond;
inline constexpr std::uint32_t kGroup=0xAFB11A12U,kSequence=0x31A03F93U;
inline constexpr std::size_t kMotionBytes=0x550;
struct Binding {
    trace::Identity character{};
    std::uintptr_t controller{},selector{},motion{};
    std::uint32_t controllerSelf{},selectorSelf{},motionSelf{};
    friend bool operator==(const Binding&,const Binding&)=default;
};
inline bool wanted(const mission::BossRequest& request) noexcept {
    return trace::admitted(request) && request.frame.bossFighting && request.frame.bossStage==0
        && request.frame.lensDestroyed[7] && !request.frame.bossCycle.openingStarted;
}
inline std::array<std::byte,128> stop_request() noexcept {
    std::array<std::byte,128> out{};
    std::memcpy(out.data(),&kGroup,4);std::memcpy(out.data()+4,&kSequence,4);out[0x60]=std::byte{0x5D};return out;
}
// The startup uses a named motion selector, separately from the authored roof
// scene. Native intro completion did not cancel this looping action39. Match
// that precise payload, not every animation or every opcode39 allocation.
inline bool owned_loop(const std::array<std::byte,kMotionBytes>& b,const Binding& owner,
                       std::uint32_t animation) noexcept {
    const auto u32=[&](std::size_t o){return trace::field<std::uint32_t>(b,o);};
    const auto u64=[&](std::size_t o){return trace::field<std::uint64_t>(b,o);};
    const auto u16=[&](std::size_t o){return trace::field<std::uint16_t>(b,o);};
    if(u32(0)!=0x815B5A49U || u32(4)!=0x808069EEU || u64(8)!=0xC10
        || u32(0x24)!=owner.motionSelf || u32(0x2C)!=owner.character.entity
        || u32(0x30)!=0x815B5A49U || u32(0x34)!=0x808069F6U || u64(0x38)!=0xD18
        || u64(0x50)!=3 || u64(0x60)!=3 || u64(0x70)!=0x230
        || u32(0x80)!=1 || u32(0x84)!=0 || u16(0x88)!=1 || u16(0x8A)!=1 || u16(0x8C)!=0) return false;
    // These are the authored three-slot pool's relative arrays. Reject a
    // changed layout rather than applying a cancellation to guessed storage.
    if(u64(0x58)!=0x268 || u64(0x68)!=0x278 || u64(0x78)!=0x298
        || u32(0x2D0)!=0x00390000 || u16(0x2F0)!=0 || u16(0x2F2)!=0x150 || u16(0x2F4)!=0) return false;
    constexpr std::size_t raw=0x320,s=raw+0x28;
    return u32(raw)==0x80F459AAU && u32(raw+4)==0x80806872U && u64(raw+8)==0x210
        && u32(raw+0x10)==0x80BFDE65U && u32(raw+0x18)==owner.selectorSelf && u32(raw+0x1C)==0x8080686BU
        && u64(raw+0x20)==0 && u32(s)==owner.character.entity && u32(s+4)==owner.controllerSelf
        && u32(s+8)==0 && u32(s+0xC)==1 && u32(s+0x10)==0x80F459ADU && u32(s+0x14)==animation
        && u32(s+0xA4)==0 && u32(s+0xA8)==1 && u32(s+0xB0)==0 && u32(s+0xB4)==1;
}
template<class Read> bool component_header(Read& read,std::uintptr_t address,std::uint32_t tag,std::uint32_t kind,
    std::uint64_t offset,std::uint32_t entity,std::uint32_t& self) noexcept {
    std::array<std::byte,0x30> b{};std::uintptr_t resolved{};
    if(!read.copy(address,b) || trace::field<std::uint32_t>(b,0)!=tag || trace::field<std::uint32_t>(b,4)!=kind
        || trace::field<std::uint64_t>(b,8)!=offset || trace::field<std::uint32_t>(b,0x2C)!=entity) return false;
    self=trace::field<std::uint32_t>(b,0x24);
    return self!=UINT32_MAX && read.resolve(self,resolved) && resolved==address;
}
// Called only at the existing post-character-update boundary, before entering
// native code. Read-only qualification is also repeated immediately before stop.
template<class Read> bool sample(Read& read,std::uintptr_t image,std::uintptr_t character,
    const mission::BossRequest& request,Binding& out,unsigned& reason) noexcept {
    reason=1;if(!wanted(request)) return false;
    Binding b;reason=2;
    if(!trace::sample(read,image,character,false,request,b.character)) return false;
    std::uintptr_t table{},row{};std::uint32_t stride{},bundle{};reason=3;
    if(!read.value(image+0x1F93428,table) || !read.value(image+0x1F93430,stride) || stride<0x9C || stride>0x100000) return false;
    row=table+static_cast<std::uintptr_t>(b.character.entity&0x1FFFU)*stride;
    if(!read.value(row+0x4C,bundle)
        || !coo_native::component<Read,1024>(read,bundle,b.character.entity,0x80806832U,b.controller)
        || !coo_native::component<Read,1024>(read,bundle,b.character.entity,0x8080686CU,b.selector)
        || !coo_native::component<Read,1024>(read,bundle,b.character.entity,0x808069EEU,b.motion)) return false;
    reason=4;std::uint32_t controllerActor{},selectorController{};
    if(!component_header(read,b.controller,0x80F66F56U,0x80806832U,0x6E8,b.character.entity,b.controllerSelf)
        || !component_header(read,b.selector,0x80F459AAU,0x8080686CU,0x1C8,b.character.entity,b.selectorSelf)
        || !component_header(read,b.motion,0x815B5A49U,0x808069EEU,0xC10,b.character.entity,b.motionSelf)
        || !read.value(b.controller+0xC0,controllerActor) || controllerActor!=request.enemy.actor
        || !read.value(b.selector+0x30,selectorController) || selectorController!=b.controllerSelf) return false;
    reason=5;std::uint64_t count{},relative{};std::uint32_t flags{},tag{},kind{};
    // C693F0 dispatches through this runtime group interface. Discovering a
    // same-entity selector is insufficient if group0 was rebound meanwhile.
    if(!read.value(b.controller+0x580,count) || count!=1
        || !read.value(b.controller+0x588,relative) || relative>0x100000) return false;
    const auto groupBinding=b.controller+0x588+relative+0x40;
    std::uint32_t boundSelector{};std::uint64_t boundOffset{};
    if(!read.value(groupBinding,tag) || tag!=0x80BFDE65U
        || !read.value(groupBinding+8,boundSelector) || boundSelector!=b.selectorSelf
        || !read.value(groupBinding+0xC,kind) || kind!=0x8080686BU
        || !read.value(groupBinding+0x10,boundOffset) || boundOffset!=0) return false;
    if(!read.value(b.selector+0x40,count) || count!=3 || !read.value(b.selector+0x48,relative) || relative!=0x28
        || !read.value(b.selector+0xF4,flags) || flags!=1
        || !read.value(b.selector+0xC0,tag) || tag!=0x80F459AAU
        || !read.value(b.selector+0xC4,kind) || kind!=0x8080300DU) return false;
    reason=6;std::array<std::byte,kMotionBytes> motion{};std::uint32_t animation{},animationSelf{};std::uintptr_t animationAddress{};
    if(!read.copy(b.motion,motion)) return false;
    animation=trace::field<std::uint32_t>(motion,0x35C);
    if(!read.resolve(animation,animationAddress)
        || !component_header(read,animationAddress,0x80F66F51U,0x808036CFU,0x1FF0,b.character.entity,animationSelf)
        || animationSelf!=animation || !owned_loop(motion,b,animation)) return false;
    // Verify the named request resolves to group0 / sequence1 and its native
    // stop interface. No guessed hash, raw selector write or arena free.
    reason=7;std::uint32_t stateHandle{},config{},stateController{};std::uintptr_t state{},names{};
    if(!read.value(b.controller+0x5C0,stateHandle) || !read.resolve(stateHandle,state)
        || !read.value(state+0x30,stateController) || stateController!=b.controllerSelf
        || !read.value(state+4,config) || config!=0x80F459CDU || !read.resolve(config,names)
        || !read.value(names+0x20,count) || count!=1 || !read.value(names+0x28,relative) || relative>0x100000) return false;
    const auto group=names+0x38+relative;
    std::uint32_t hash{};std::uint64_t sequenceRelative{};
    if(!read.value(group+0xC,hash) || hash!=kGroup || !read.value(group+0x10,count) || count!=3
        || !read.value(group+0x18,sequenceRelative) || sequenceRelative>0x100000
        || !read.value(group+0x28+sequenceRelative+4,hash) || hash!=kSequence) return false;
    reason=8;std::uintptr_t interfaceBase{},callback{};
    if(!read.resolve(0x80BFDE65U,interfaceBase) || !read.value(interfaceBase+0x18,relative) || relative>0x100000
        || !read.value(interfaceBase+relative+0x48,callback) || callback!=image+0x10D35D0) return false;
    constexpr std::array<std::uint8_t,16> prefix{0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x7C,0x24,0x10,0x55,0x48,0x8B,0xEC,0x48,0x83};
    std::array<std::byte,16> actual{};
    if(!read.copy(image+0xC693F0,actual) || std::memcmp(actual.data(),prefix.data(),prefix.size())) return false;
    reason=0;out=b;return true;
}
}
