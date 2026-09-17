#pragma once
#include "strike_bond_fire_trace.h"
#include "gateway_native_read.h"
namespace dawn::client::hooks::bootflow::strike_bond_target_binding {
namespace trace=strike_bond_fire_trace;
namespace mission=state::activity::strike_bond;
struct Binding {
    std::uintptr_t weapon{},controller{},mainState{};
    std::uint32_t controllerSelf{UINT32_MAX};
    gateway_native::Weak target{};
    trace::Identity character{};
    friend bool operator==(const Binding&,const Binding&)=default;
};
inline bool wanted(const mission::BossRequest& r) noexcept {
    return trace::admitted(r) && r.frame.bossFighting && r.frame.lensDestroyed[7] && r.frame.bossStage<=2;
}
inline bool exact_request(const std::array<std::byte,128>& b) noexcept {
    std::array<std::byte,128> expected{};
    expected[0x60]=std::byte{0x24};
    for(std::size_t i=0x70;i<0x78;++i) expected[i]=std::byte{0xFF};
    expected[0x78]=std::byte{0x80};expected[0x79]=std::byte{1};expected[0x7A]=std::byte{1};
    return b==expected;
}
// Native C613E0 receives the weapon substate, not the NPC controller. Resolve
// that owner before accepting the exact authored no-target request.
template<class Read> bool sample(Read& read,std::uintptr_t image,std::uintptr_t weapon,
    std::uintptr_t requestAddress,const mission::BossRequest& request,Binding& out) noexcept {
    if(!wanted(request)) return false;
    Binding b;b.weapon=weapon;std::array<std::byte,128> action{},again{};
    std::uint32_t mainHandle{},value{};std::uintptr_t table{},resolved{};
    if(!read.copy(requestAddress,action) || !exact_request(action)
        || !read.value(weapon+0x1D8,b.controllerSelf) || b.controllerSelf==UINT32_MAX
        || !read.resolve(b.controllerSelf,b.controller)
        || !trace::sample(read,image,b.controller,true,request,b.character)
        || !read.value(b.controller+0x24,value) || value!=b.controllerSelf
        || !read.value(b.controller+0x5C0,mainHandle) || !read.resolve(mainHandle,b.mainState)
        || b.mainState+0x8F0!=weapon || !read.value(b.mainState+0x30,value) || value!=b.controllerSelf
        || !read.value(b.mainState,value) || value!=0x80F56184U) return false;
    std::uint32_t stride{};
    if(!read.value(image+0x1F9D7F8,table) || !read.value(image+0x1F9D800,stride)) return false;
    const auto actor=table+static_cast<std::uintptr_t>(request.enemy.actor&0x1FFFU)*stride;
    if(!read.value(actor+0x30,value) || value!=0x80F66F52U) return false;
    // Slot zero is the native primary target populated by action54. Do not
    // invent an entity or preserve a target through death, retirement or reuse.
    std::uintptr_t targetRow{};
    if(!read.value(b.mainState+0x1680,b.target) || b.target.handle==b.character.entity
        || !read.entity_row(b.target,targetRow) || !read.value(targetRow+0xC,value) || value!=b.target.handle) return false;
    gateway_native::Weak targetAgain{};
    if(!read.copy(requestAddress,again) || action!=again
        || !read.value(weapon+0x1D8,value) || value!=b.controllerSelf
        || !read.resolve(b.controllerSelf,resolved) || resolved!=b.controller
        || !read.value(b.controller+0x5C0,value) || value!=mainHandle
        || !read.resolve(mainHandle,resolved) || resolved!=b.mainState
        || !read.value(actor+0x30,value) || value!=0x80F66F52U
        || !read.value(b.mainState+0x30,value) || value!=b.controllerSelf
        || !read.value(b.mainState,value) || value!=0x80F56184U
        || !read.value(b.mainState+0x1680,targetAgain) || targetAgain!=b.target || !read.entity_row(targetAgain,resolved) || resolved!=targetRow
        || !read.value(targetRow+0xC,value) || value!=b.target.handle) return false;
    out=b;return true;
}
// A7B850 takes the parameter environment at context+8 and program at +10.
// Scope FFFF in exact_request prevents AD3AE0 from rebinding that environment.
template<class Read> bool decoder_context(Read& read,std::uintptr_t context) noexcept {
    std::uintptr_t config{},program{},actual{};std::uint32_t value{};std::uint8_t byte{};
    return read.resolve(0x80F66F52U,config) && read.resolve(0x80F56183U,program)
        && read.value(context+8,actual) && actual==config && read.value(context+0x10,actual) && actual==program
        && read.value(config+0x6C8,value) && value==0x64F350F0U
        && read.value(config+0x6CC,value) && value==5
        && read.value(config+0x1204,value) && value==UINT32_MAX
        && read.value(program+0x3B6C0+384,byte) && byte==16
        && read.value(program+0x309F0+384*4,value) && value==0x50U
        && read.value(program+0x2ED70,value) && value==0x64F350F0U;
}
// Native C729F0 applies only changed actions. Combat admission alone does not
// invalidate the cache, so a pre-combat no-target action needs one native replay.
struct Replay {
    Binding binding{};
    std::uintptr_t ai{},action{},cacheRow{};
    std::uint32_t aiSelf{UINT32_MAX};
    std::array<std::byte,32> descriptor{};
    std::array<std::byte,24> cached{};
    friend bool operator==(const Replay&,const Replay&)=default;
};
inline bool exact_cached_request(const std::array<std::byte,24>& bytes) noexcept {
    constexpr std::array<std::uint8_t,24> expected{
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x80,0x01,0x24,0x01,
        0,0,0,0,0,0,0xFF,0xFF,0,0,0,0};
    return std::memcmp(bytes.data(),expected.data(),expected.size())==0;
}
template<class Read> bool replay_sample(Read& read,std::uintptr_t image,std::uintptr_t weapon,
    const mission::BossRequest& request,Replay& out,unsigned& reason) noexcept {
    reason=1;if(!wanted(request)) return false;
    Replay r;std::uint16_t requested{},offset{};std::uint32_t self{},mainHandle{},value{},count{},used{},footer{},shift{};
    std::uintptr_t controller{},main{},table{},actor{},resolved{};std::uint32_t stride{};
    reason=2;
    if(!read.value(weapon+0xB2,requested) || requested!=0xFF02
        || !read.value(weapon+0x1D8,self) || !read.resolve(self,controller)
        || !read.value(controller+0x5C0,mainHandle) || !read.resolve(mainHandle,main) || main+0x8F0!=weapon) return false;
    trace::Identity identity{};reason=3;
    if(!trace::sample(read,image,controller,true,request,identity)
        || !read.copy(main+0xC0,r.descriptor)) return false;
    r.aiSelf=trace::field<std::uint32_t>(r.descriptor,0);
    if(r.aiSelf==UINT32_MAX || trace::field<std::uint32_t>(r.descriptor,4)!=request.enemy.actor
        || trace::field<std::uint64_t>(r.descriptor,8)!=UINT64_MAX
        || trace::field<std::uint64_t>(r.descriptor,16)!=0 || trace::field<std::uint64_t>(r.descriptor,24)!=0
        || !read.resolve(r.aiSelf,r.ai)) return false;
    std::array<std::byte,0x30> aiHeader{};reason=4;
    if(!read.copy(r.ai,aiHeader) || trace::field<std::uint32_t>(aiHeader,0)!=0x80F66F55U
        || trace::field<std::uint32_t>(aiHeader,4)!=0x808082ECU
        || trace::field<std::uint64_t>(aiHeader,8)!=0x2ED8
        || trace::field<std::uint32_t>(aiHeader,0x24)!=r.aiSelf
        || trace::field<std::uint32_t>(aiHeader,0x2C)!=identity.entity
        || !read.value(image+0x1F9D7F8,table) || !read.value(image+0x1F9D800,stride)
        || stride<0xA154 || stride>0x100000) return false;
    actor=table+static_cast<std::uintptr_t>(request.enemy.actor&0x1FFFU)*stride;
    if(!read.value(actor+0x50,value) || value!=r.aiSelf) return false;
    reason=5;
    // Cache class 29 is opcode24. The relative row must fit its actual storage;
    // a retiring row or a different expression is not replayed.
    if(!read.value(main+0x770,count) || count==0 || count>71
        || !read.value(main+0x774,used) || used<24 || used>0x600
        || !read.value(main+0x778,footer) || footer!=4
        || !read.value(main+0x77C,shift) || shift!=1
        || !read.value(main+0xE0+29*2,offset) || (offset&1U) || offset>used-24) return false;
    r.cacheRow=main+0x170+offset;
    if(!read.copy(r.cacheRow,r.cached) || !exact_cached_request(r.cached)) return false;
    reason=6;std::uint32_t actions{};
    if(!read.value(r.ai+0x110,actions) || actions==0 || actions>32) return false;
    for(std::uint32_t i=0;i<actions;++i) {
        const auto address=r.ai+0x120+static_cast<std::uintptr_t>(i)*128;
        std::uint8_t opcode{};if(!read.value(address+0x60,opcode)) return false;
        if(opcode!=0x24) continue;
        std::array<std::byte,128> action{};
        if(r.action || !read.copy(address,action) || !exact_request(action)) return false;
        r.action=address;
    }
    if(!r.action || !sample(read,image,weapon,r.action,request,r.binding)) return false;
    reason=7;std::array<std::byte,32> descriptorAgain{};std::array<std::byte,24> cachedAgain{};
    std::array<std::byte,0x30> aiAgain{};std::uint16_t offsetAgain{};
    if(!read.resolve(r.aiSelf,resolved) || resolved!=r.ai || !read.copy(r.ai,aiAgain) || aiAgain!=aiHeader
        || !read.value(actor+0x50,value) || value!=r.aiSelf
        || !read.copy(main+0xC0,descriptorAgain) || descriptorAgain!=r.descriptor
        || !read.value(main+0x770,value) || value!=count || !read.value(main+0x774,value) || value!=used
        || !read.value(main+0x778,value) || value!=footer || !read.value(main+0x77C,value) || value!=shift
        || !read.value(main+0xE0+29*2,offsetAgain) || offsetAgain!=offset
        || !read.copy(r.cacheRow,cachedAgain) || cachedAgain!=r.cached
        || !read.value(r.ai+0x110,value) || value!=actions
        || !read.value(weapon+0xB2,requested) || requested!=0xFF02) return false;
    reason=0;out=r;return true;
}
} // namespace dawn::client::hooks::bootflow::strike_bond_target_binding
