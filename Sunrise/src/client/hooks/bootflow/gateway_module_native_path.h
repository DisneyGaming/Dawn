#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include "gateway_module_identity.h"
namespace sunrise::client::hooks::bootflow::gateway_module_native_path {
struct Probe { std::uint32_t health{UINT32_MAX};std::uintptr_t component{};std::uint32_t resources{};bool dead{}; };
// Match the read-only resource iterator (591290/59A350) and component-self
// lookup (557529..55754A). Runtime metadata, unlike serialized definitions,
// describes the component offsets in each entity resource allocation.
template<class Read> bool find(Read& read,std::uint32_t bundle,std::uint32_t entity,Probe& out,
    native_box_identity::Binding binding=native_box_identity::kGatewayBinding) noexcept {
    std::array<std::uint32_t,64> visited{};
    while(bundle!=UINT32_MAX && out.resources<visited.size()) {
        for(std::uint32_t i=0;i<out.resources;++i) { if(visited[i]==bundle) { return false; } }
        visited[out.resources++]=bundle;
        std::uintptr_t base{},allocation{};std::uint32_t flags{},type{};
        if(!read.resolve(bundle,base,&allocation) || !read.value(base,flags)) { return false; }
        if((flags&2U)==0) {
            std::uintptr_t metadata{};std::uint64_t count{};std::int64_t relative{};
            if(!read.value(base+4,type) || !read.resolve(type,metadata)
                || !read.value(metadata+0x68,count) || count>256
                || !read.value(metadata+0x70,relative)) { return false; }
            if(count!=0) {
                if(relative==0 || relative>0x1000000 || relative < -0x1000000 || metadata>UINTPTR_MAX-0x80-0x1000000
                    || metadata+0x80<static_cast<std::uintptr_t>(relative<0?-relative:0)) { return false; }
                const auto records=relative<0?metadata+0x80-static_cast<std::uintptr_t>(-relative):metadata+0x80+static_cast<std::uintptr_t>(relative);
                for(std::uint64_t i=0;i<count;++i) {
                    std::int32_t offset{};
                    if(!read.value(records+i*24+0x14,offset) || offset<0 || offset>0x400000) { return false; }
                    if(base>UINTPTR_MAX-static_cast<std::uintptr_t>(offset)) { return false; }
                    const auto component=base+static_cast<std::uintptr_t>(offset);
                    std::array<std::byte,16> header{};
                    if(!read.copy(component,header)) { return false; }
                    std::uint32_t tag{},kind{};std::uint64_t definitionOffset{};
                    std::memcpy(&tag,header.data(),4);std::memcpy(&kind,header.data()+4,4);std::memcpy(&definitionOffset,header.data()+8,8);
                    if(tag!=binding.definition || kind!=binding.kind || definitionOffset!=binding.definitionOffset) { continue; }
                    std::array<std::byte,0x340> state{};std::uintptr_t resolved{};
                    if(!read.copy(component,state)) { return false; }
                    std::memcpy(&out.health,state.data()+0x24,4);
                    if(!native_box_identity::health(state,out.health,entity,binding)
                        || !read.resolve(out.health,resolved) || resolved!=component) { return false; }
                    out.component=component;out.dead=native_box_identity::dead(state,out.health,entity,binding);return true;
                }
            }
        }
        if(!read.value(allocation+0x18,bundle)) { return false; }
    }
    return false;
}
}
