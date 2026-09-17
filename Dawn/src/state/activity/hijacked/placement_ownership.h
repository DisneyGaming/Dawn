#pragma once
#include "placement_catalog.h"
#include <cstring>
namespace dawn::state::activity::hijacked {
template<class T> inline T placement_field(std::span<const std::byte> bytes,std::size_t offset) noexcept {
    T value{};if(offset<=bytes.size() && sizeof value<=bytes.size()-offset) {std::memcpy(&value,bytes.data()+offset,sizeof value);}return value;
}
inline bool placed_identity(std::span<const std::byte> bytes,const Placement& p,std::uint32_t handle) noexcept {
    return handle!=UINT32_MAX && bytes.size()>=0xA0
        && placement_field<std::uint32_t>(bytes,12)==handle
        && (placement_field<std::uint32_t>(bytes,4)&4U)==0
        && placement_field<std::uint32_t>(bytes,0x88)==p.table
        && placement_field<std::uint32_t>(bytes,0x8C)==p.record
        && placement_field<std::uint64_t>(bytes,0x90)==p.guid
        && placement_field<std::uint32_t>(bytes,0x4C)!=UINT32_MAX;
}
inline bool placement_source_identity(std::span<const std::byte> bytes,std::uint32_t handle,
    std::uint32_t definition,std::int64_t offset,std::uint32_t generation) noexcept {
    if(bytes.size()<0x200 || handle==UINT32_MAX || !generation) {return false;}
    for(const auto self:{0x30U,0x48U}) {
        if(placement_field<std::uint32_t>(bytes,self)!=handle
            || placement_field<std::uint32_t>(bytes,self+4)!=0x80809A3BU
            || placement_field<std::int64_t>(bytes,self+8)!=0) {return false;}
    }
    return placement_field<std::uint32_t>(bytes,0)==definition
        && placement_field<std::uint32_t>(bytes,4)==0x8080948FU
        && placement_field<std::int64_t>(bytes,8)==offset
        // Native4E8FB0 applies authority generation even with zero requests.
        // Sense generation is actor-result state, not placement authorization.
        && placement_field<std::uint32_t>(bytes,0x1FC)==generation;
}

}
