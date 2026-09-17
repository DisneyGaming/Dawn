#pragma once
#include "public_event_deferred_placement_capture.h"
namespace dawn::client::hooks::bootflow::public_event_point_interface_capture {
namespace f=server::runtime::activity::public_event::deferred_placement;
// Native9EBC00 copies the runtime offset from the loaded compiled interface
// row+4. Package bytes leave that fixup zero; it is not a zero-offset contract.
// Match the complete authored row and reject ambiguous or unreadable metadata.
template<class Reader,class Resolve>
[[nodiscard]] bool loaded_offset(const f::Ticket& t,Reader&& read,Resolve&& resolve,
    std::int64_t& offset) noexcept {
    offset=-1;std::uintptr_t definition{};std::array<std::byte,0x68> header{};
    if(!resolve(t.entityDefinition,0,definition) || definition<0x10000
        || definition>UINTPTR_MAX-0x2000000 || !read(definition,std::span(header)))return false;
    const auto count=f::field<std::uint64_t>(header,0x58);
    const auto relative=f::field<std::int64_t>(header,0x60);
    if(!count || count>4096 || relative<0 || relative>0x1D0000)return false;
    const auto array=definition+0x60+static_cast<std::uintptr_t>(relative);
    std::array<std::byte,20> arrayHeader{};
    if(!read(array-4,std::span(arrayHeader)) || f::field<std::uint32_t>(arrayHeader,0)!=0x80809FBD
        || f::field<std::uint64_t>(arrayHeader,4)!=count
        || f::field<std::uint32_t>(arrayHeader,12)!=0x80809C22)return false;
    std::size_t matches{};
    for(std::size_t i=0;i<count;++i){
        std::array<std::byte,40> row{};
        if(!read(array+16+i*row.size(),std::span(row)))return false;
        if(f::field<std::uint32_t>(row,16)!=t.pointComponent
            || f::field<std::uint32_t>(row,20)!=0x80809C50
            || f::field<std::int64_t>(row,24)!=t.pointInterfaceOffset
            || f::field<std::uint32_t>(row,32)!=0x80FEB394)continue;
        if(++matches!=1 || f::field<std::uint32_t>(row,12)!=0x80806730)return false;
        offset=f::field<std::int32_t>(row,4);
        if(offset<0 || offset>=0x2000000)return false;
    }
    return matches==1;
}
// Output of original4E25D0 is80807F35's static interface plus a current runtime
// group weak reference. It is not a point-job success or population receipt.
template<class Reader,class Resolve,class Weak>
[[nodiscard]] bool qualify(const f::Observation& creation,std::span<const std::byte> entity,
    std::span<const std::byte> output,Reader&& read,Resolve&& resolve,Weak&& weak,std::uint64_t& groupWeak) noexcept {
    groupWeak=0;const auto& t=creation.ticket;
    if(!f::valid(t) || !f::tag(t.pointComponent) || t.pointInterfaceOffset<=0 || t.pointInterfaceOffset>=0x2000000
        || !creation.sequence || entity.size()!=0x98 || output.size()!=48
        || f::field<std::uint32_t>(entity,0xC)!=creation.child || (f::field<std::uint32_t>(entity,4)&4U)
        || f::field<std::uint64_t>(entity,0x90)!=t.pointGuid
        || f::field<std::uint32_t>(output,0)!=t.pointComponent || f::field<std::uint32_t>(output,4)!=0x80809C50
        || f::field<std::int64_t>(output,8)!=t.pointInterfaceOffset || f::field<std::uint32_t>(output,16)!=0x80FEB394
        || f::field<std::uint32_t>(output,40)!=0x80806730)return false;
    std::int64_t runtimeOffset{};
    if(!loaded_offset(t,read,resolve,runtimeOffset) || f::field<std::int64_t>(output,32)!=runtimeOffset)return false;
    const auto pair=f::field<std::uint64_t>(output,24);std::uint32_t handle=UINT32_MAX;
    if(!weak(pair,handle) || handle==UINT32_MAX || handle!=f::field<std::uint32_t>(output,28))return false;
    auto current=f::field<std::uint32_t>(entity,0x4C);std::array<std::uint32_t,64> visited{};
    for(std::size_t i=0;i<visited.size() && current!=UINT32_MAX;++i){
        for(std::size_t j=0;j<i;++j)if(visited[j]==current)return false;
        visited[i]=current;std::uintptr_t address{};std::array<std::byte,0x28> group{};
        if(!resolve(current,0,address) || !read(address,std::span(group)))return false;
        if(current==handle){
            if((f::field<std::uint32_t>(group,0)&2U) || f::field<std::uint32_t>(group,4)!=t.entityDefinition)return false;
            groupWeak=pair;return true;
        }
        current=f::field<std::uint32_t>(group,0x18);
    }
    return false;
}
}
