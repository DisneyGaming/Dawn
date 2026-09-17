#pragma once
#include <cstddef>
#include <cstdint>

namespace dawn::client::hooks::bootflow::gateway_vance_native_path {
// Live-validated 80EC0ABC selector. Resolve salted weak references on every tick;
// physical addresses are not retained across the native scene update.
struct Weak {
    std::uint32_t serial{UINT32_MAX},handle{UINT32_MAX};
    bool operator==(const Weak&) const = default;
};
struct Ref { std::uint32_t tag{},kind{};std::uint64_t offset{}; };
struct Stage { bool valid{},turned{},conversation{}; };
template<class Read> bool header(Read& read,std::uintptr_t address,Ref expected,std::uint32_t handle) noexcept {
    Ref ref{};std::uint32_t self{},owner{};
    return read.value(address,ref) && ref.tag==expected.tag && ref.kind==expected.kind && ref.offset==expected.offset
        && read.value(address+0x24,self) && self==handle && read.value(address+0x2C,owner) && owner!=UINT32_MAX;
}
template<class Read> bool node(Read& read,std::uintptr_t root,std::uint32_t handle,std::uint64_t offset,
    std::uint32_t kind,std::uint64_t definition,std::uint8_t& state) noexcept {
    Ref ref{};std::uint32_t parent{},parentKind{};std::uint64_t selfOffset{};
    return read.value(root+offset,ref) && ref.tag==0x80EC0ABCU && ref.kind==kind && ref.offset==definition
        && read.value(root+offset+0x28,parent) && parent==handle
        && read.value(root+offset+0x2C,parentKind) && parentKind==kind-1U
        && read.value(root+offset+0x30,selfOffset) && selfOffset==offset
        && read.value(root+offset+0x98,state) && state<=2;
}
template<class Read,class Resolve> Stage probe(Read& read,std::uintptr_t root,std::uint32_t handle,Resolve&& resolve) noexcept {
    if(root<0x10000 || root>UINTPTR_MAX-0x2530 || handle==UINT32_MAX
        || !header(read,root,{0x80EC0ABCU,0x80806384U,0x2548},handle)) { return {}; }
    std::uint64_t count{};std::uint8_t turn{},hold{},child{};
    if(!read.value(root+0x38,count) || count!=12
        || !node(read,root,handle,0x1240,0x80806307U,0x2D28,turn)
        || !node(read,root,handle,0x14A0,0x80806307U,0x2DE8,hold)
        || !node(read,root,handle,0x8B0,0x808062FEU,0x2998,child)) { return {}; }
    if(turn==2 && hold==1 && child==0) { return {true,true,false}; }
    if(turn!=2 || hold!=2 || child!=1) { return {true,false,false}; }
    std::int32_t stop{};Weak weak{},after{};std::uintptr_t address{};
    if(!read.value(root+0xA50,stop) || stop>0 || !read.value(root+0xA60,weak)
        || weak.handle==UINT32_MAX || !resolve(weak,address) || address<0x10000 || address>UINTPTR_MAX-0x30
        || !header(read,address,{0x80EC0AC5U,0x808084E9U,0x27E8},weak.handle)
        || !read.value(root+0xA60,after) || after!=weak) { return {}; }
    std::uint8_t afterTurn{},afterHold{},afterChild{};
    if(!read.value(root+0x12D8,afterTurn) || afterTurn!=turn
        || !read.value(root+0x1538,afterHold) || afterHold!=hold
        || !read.value(root+0x948,afterChild) || afterChild!=child) { return {}; }
    return {true,false,true};
}
} // namespace dawn::client::hooks::bootflow::gateway_vance_native_path
