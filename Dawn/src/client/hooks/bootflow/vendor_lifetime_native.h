#pragma once
#include "vendor_network_presence.h"

namespace dawn::client::hooks::bootflow::vendor_lifetime_native {
// Native 170FC90 recursively destroys a local retained facet. Its manager and
// salted child chain must be qualified before calling it, after area teardown.
template<class Weak> struct Part {
    Weak network{};std::uint32_t facet{UINT32_MAX},parent{UINT32_MAX},first{UINT32_MAX},next{UINT32_MAX};
    std::uint16_t index{};
    friend bool operator==(const Part&,const Part&)=default;
};
template<class Weak> struct Retirement {
    std::uintptr_t context{},manager{};std::array<Part<Weak>,16> parts{};std::size_t count{};
    friend bool operator==(const Retirement&,const Retirement&)=default;
};
enum class Stage { entity,network,manager,facet,children,ready };
inline const char* name(Stage stage) noexcept {
    constexpr const char* names[]{"entity","network","manager","facet","children","ready"};
    return names[static_cast<unsigned>(stage)];
}
template<class Read,class Weak>
bool part(Read& read,std::uintptr_t manager,std::uint16_t index,Part<Weak>& out) noexcept {
    if(index>=1024) return false;
    std::byte allocated{},owned{};std::int16_t mapped{-1};std::int8_t type{},owner{};
    std::uint32_t net{},facet{};std::uintptr_t row{};Part<Weak> p;p.index=index;
    const auto address=read.image+0x30B0440+static_cast<std::uintptr_t>(index)*0x70;
    if(!read.value(read.image+0x30B0340+index/8,allocated) || !(std::to_integer<unsigned>(allocated)&(1U<<(index%8)))
        || !read.value(manager+0xC920+index/8,owned) || !(std::to_integer<unsigned>(owned)&(1U<<(index%8)))
        || !read.value(address,type) || type!=0 || !read.value(address+1,owner) || (owner!=-1 && owner!=-2)
        || !read.value(address+4,net) || !read.make_weak(net,p.network)
        || !read.value(address+8,p.facet) || p.facet==UINT32_MAX
        || !read.value(manager+0x114+static_cast<std::uintptr_t>(p.facet&0x1FFFU)*6,mapped) || mapped!=index
        || !vendor_network::network_row(read,net,row) || !read.value(row,facet) || facet!=p.facet
        || !read.value(address+0xC,p.parent) || !read.value(address+0x48,p.first) || !read.value(address+0x44,p.next)
        || !read.weak(p.network)) return false;
    out=p;return true;
}
template<class Read,class Weak>
bool sample(Read& read,const vendor_network::Root<Weak>& root,std::uintptr_t context,
    Retirement<Weak>& out,Stage& stage) noexcept {
    stage=Stage::entity;
    if(!vendor_network::retired(read,root)) return false;
    stage=Stage::network;
    if(!root.networked || root.index>=1024 || !read.weak(root.network)) return false;
    stage=Stage::manager;
    if(context<0x10000 || context>UINTPTR_MAX-0x60000) return false;
    Retirement<Weak> r;r.context=context;unsigned matches{};
    for(unsigned group=0;group<3;++group) {
        const auto manager=context+0x206C8+static_cast<std::uintptr_t>(group)*0x11E08+0x270;
        std::byte owned{};std::uint32_t peers{};std::array<std::uintptr_t,31> views{};
        if(!read.value(manager+0xC920+root.index/8,owned)
            || !(std::to_integer<unsigned>(owned)&(1U<<(root.index%8)))) continue;
        // This cleanup only owns local facets. A real remote view must retain
        // its native replication lifecycle instead of having its actor removed.
        if(!read.value(manager+0x110,peers) || peers || !read.value(manager+0x18,views)) return false;
        for(const auto view:views) if(view) return false;
        r.manager=manager;++matches;
    }
    if(matches!=1) return false;
    stage=Stage::facet;
    if(!part(read,r.manager,root.index,r.parts[0]) || r.parts[0].network!=root.network
        || r.parts[0].facet!=root.facet || r.parts[0].parent!=UINT32_MAX) return false;
    r.count=1;stage=Stage::children;
    for(std::size_t i=0;i<r.count;++i) {
        auto child=r.parts[i].first;
        while(child!=UINT32_MAX) {
            if(r.count==r.parts.size()) return false;
            for(std::size_t j=0;j<r.count;++j) if(r.parts[j].facet==child) return false;
            std::int16_t index{-1};auto& p=r.parts[r.count];
            if(!read.value(r.manager+0x114+static_cast<std::uintptr_t>(child&0x1FFFU)*6,index)
                || index<0 || index>=1024 || !part(read,r.manager,static_cast<std::uint16_t>(index),p)
                || p.facet!=child || p.parent!=r.parts[i].facet) return false;
            ++r.count;child=p.next;
        }
    }
    // Cross-check the independent parent links. A missing or newly attached
    // child must not be silently swept by the recursive native destructor.
    std::array<std::uint8_t,128> owned{};
    if(!read.value(r.manager+0xC920,owned)) return false;
    for(std::uint16_t index=0;index<1024;++index) {
        if(!(owned[index/8]&(1U<<(index%8)))) continue;
        bool included{};for(std::size_t i=0;i<r.count;++i) included|=r.parts[i].index==index;
        if(included) continue;
        std::uint32_t parent{};
        if(!read.value(read.image+0x30B0440+static_cast<std::uintptr_t>(index)*0x70+0xC,parent)) return false;
        for(std::size_t i=0;i<r.count;++i) if(parent==r.parts[i].facet) return false;
    }
    for(std::size_t i=0;i<r.count;++i) {
        Part<Weak> again;
        if(!part(read,r.manager,r.parts[i].index,again) || again!=r.parts[i]) return false;
    }
    if(!vendor_network::retired(read,root)) return false;
    out=r;stage=Stage::ready;return true;
}
template<class Read,class Weak> bool gone(Read& read,const Retirement<Weak>& r) noexcept {
    if(!r.count || r.count>r.parts.size()) return false;
    for(std::size_t i=0;i<r.count;++i) {
        const auto& p=r.parts[i];Weak now{};std::uintptr_t row{};std::uint32_t facet{};
        if(!read.make_weak(p.network.handle,now)) return false;
        if(now!=p.network) continue;
        if(!vendor_network::network_row(read,p.network.handle,row) || !read.value(row,facet) || facet!=UINT32_MAX) return false;
    }
    return true;
}
}
