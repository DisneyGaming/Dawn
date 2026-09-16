#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace sunrise::client::hooks::bootflow::vendor_network {
enum class Presence { unknown, present, removed };
template<class Weak> struct Root {
    Weak entity{},member{},network{};
    std::uint32_t facet{UINT32_MAX};
    std::uint16_t index{};
    bool attached{},networked{};
};
template<class Read,class Weak>
bool attach(Read& read,std::uint32_t entity,Root<Weak>& out) noexcept {
    Root<Weak> next;std::uintptr_t row{},member{},allocation{};std::uint32_t handle{},owner{};
    if(!read.make_weak(entity,next.entity) || !read.entity_row(next.entity,row)
        || !read.value(row+0x4C,handle) || !read.make_weak(handle,next.member)
        || !read.resolve(handle,member,&allocation) || !read.value(allocation+0x10,owner)
        || owner!=entity || !read.weak(next.entity) || !read.weak(next.member)) return false;
    next.attached=true;out=next;return true;
}
template<class Read>
bool network_row(Read& read,std::uint32_t handle,std::uintptr_t& row) noexcept {
    std::uintptr_t table{};std::uint32_t stride{};
    if(handle==UINT32_MAX || !read.value(read.image+0x2037D48,table)
        || !read.value(read.image+0x2037D50,stride) || table<0x10000 || stride<12 || stride>0x1000) return false;
    row=table+static_cast<std::uintptr_t>(handle&0x1FFFU)*stride;return true;
}
// Associate the existing admitted root with its native network facet, including
// the allocator serial and full facet handle. Never match NPC names/positions.
template<class Read,class Weak>
bool capture(Read& read,Root<Weak>& root) noexcept {
    if(!root.attached || root.networked) return root.networked;
    if(!read.weak(root.entity) || !read.weak(root.member)) return false;
    std::array<std::byte,128> bitmap{};
    if(!read.copy(read.image+0x30B0340,bitmap)) return false;
    for(std::size_t i=0;i<1024;++i) {
        if(!(std::to_integer<unsigned>(bitmap[i/8])&(1U<<(i%8)))) continue;
        // Each occupied facet has its own bounded read budget.
        Read entry{read.image};const auto address=read.image+0x30B0440+i*0x70;
        std::int8_t type{},owner{};std::uint32_t net{},facet{},object{},self{},member{},entity{};
        std::uintptr_t row{},native{},allocation{};Weak network{},linked{};
        if(!entry.value(address,type) || !entry.value(address+1,owner) || type!=0 || owner!=-1
            || !entry.value(address+4,net) || !entry.value(address+8,facet) || facet==UINT32_MAX
            || !network_row(entry,net,row) || !entry.value(row,self) || self!=facet
            || !entry.value(row+4,object) || !entry.make_weak(net,network)
            || !entry.resolve(object,native) || !entry.value(native+0xC0,self) || self!=object
            || !entry.value(native+0xC8,member) || member!=root.member.handle
            || !entry.make_weak(member,linked) || linked!=root.member
            || !entry.resolve(member,native,&allocation) || !entry.value(allocation+0x10,entity)
            || entity!=root.entity.handle || !entry.weak(root.entity) || !entry.weak(network)) continue;
        // Recheck the facet after walking the entity, so a recycled slot cannot
        // attach a different object's lifetime to this vendor.
        std::uint32_t again{},bound{};
        if(!entry.value(address+8,again) || again!=facet || !entry.value(row+4,bound) || bound!=object) continue;
        root.network=network;root.facet=facet;root.index=static_cast<std::uint16_t>(i);root.networked=true;return true;
    }
    return false;
}
template<class Read,class Weak>
bool retired(Read& read,const Root<Weak>& root) noexcept {
    if(!root.attached) return false;
    Weak current{};std::uintptr_t table{};std::uint32_t stride{},self{},member{},flags{};
    if(!read.make_weak(root.entity.handle,current)) return false;
    if(current!=root.entity) return true;
    // Native area teardown may mark the slot removed before allocator reuse.
    // entity_row intentionally rejects removed slots; inspect the native table
    // directly here, with the original allocator/self/member identities intact.
    if(!read.value(read.image+0x1F93428,table) || table<0x10000
        || !read.value(read.image+0x1F93430,stride) || stride<0x50 || stride>0x100000) return false;
    const auto row=table+static_cast<std::uintptr_t>(root.entity.handle&0x1FFFU)*stride;
    return read.value(row+0x0C,self) && self==root.entity.handle
        && read.value(row+0x4C,member) && member==root.member.handle && read.value(row+4,flags) && (flags&4U)
        && read.weak(root.entity);
}
template<class Read,class Weak>
Presence presence(Read& read,const Root<Weak>& root) noexcept {
    if(!root.attached) return Presence::unknown;
    Weak entity{};
    if(!read.make_weak(root.entity.handle,entity)) return Presence::unknown;
    if(entity==root.entity && !retired(read,root)) return Presence::present;
    // Actor/entity pool removal does not remove the streamed network replica.
    // If capture was missed, retain the latch rather than manufacture absence.
    if(!root.networked) return Presence::unknown;
    Weak network{};std::byte bit{};std::uint32_t handle{},facet{},netFacet{};std::uintptr_t row{};
    if(!read.make_weak(root.network.handle,network)) return Presence::unknown;
    if(network!=root.network) return Presence::removed;
    const auto address=read.image+0x30B0440+static_cast<std::uintptr_t>(root.index)*0x70;
    if(!read.copy(read.image+0x30B0340+root.index/8,{&bit,1})
        || !read.value(address+4,handle) || !read.value(address+8,facet)
        || !network_row(read,root.network.handle,row) || !read.value(row,netFacet)) return Presence::unknown;
    const bool live=(std::to_integer<unsigned>(bit)&(1U<<(root.index%8)))
        && handle==root.network.handle && facet==root.facet && netFacet==root.facet;
    if(live) return Presence::present;
    // A partially updated manager row is not proof of deletion. The native
    // network table's empty handle (or allocator reuse above) closes the facet.
    return netFacet==UINT32_MAX?Presence::removed:Presence::unknown;
}
}
