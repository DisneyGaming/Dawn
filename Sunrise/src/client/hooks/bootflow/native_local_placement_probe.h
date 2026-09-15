#pragma once

#include "gateway_native_read.h"
#include <array>
#include <cstdint>
#include <limits>

namespace sunrise::client::hooks::bootflow::native_local_placement_probe {

/** Trusted server binding for one authored type-4 source. No transform is carried here. */
struct Request final {
    std::uint32_t registry{};
    std::uint16_t slot{};
    std::uint32_t definition{};
    std::int64_t definitionOffset{};
    std::uint32_t generation{};
    bool active{};
};

struct Identity final {
    std::uintptr_t root{};
    std::uintptr_t source{};
    std::uintptr_t component{};
    std::uintptr_t definition{};
    gateway_native::Weak child{};
    std::uint32_t generation{};
    bool active{};
    friend bool operator==(const Identity&,const Identity&)=default;
};

namespace detail {
inline constexpr std::uintptr_t kSelector=0x1F91FE8;
inline constexpr std::uintptr_t kTable=0x2109B80;
inline constexpr std::uintptr_t kStride=0x2109B90;
inline constexpr std::uintptr_t kSourceRoot=0xC20;
inline constexpr std::uintptr_t kGroupOffset=0xC;
inline constexpr std::uintptr_t kGroupStride=24;
inline constexpr std::uintptr_t kSourceStride=40;
inline constexpr std::uint32_t kComponentTag=0x80809928U;
inline constexpr std::uint32_t kDefinitionTag=0x80809927U;

[[nodiscard]] inline bool add(std::uintptr_t base,std::int64_t offset,std::uintptr_t& result) noexcept {
    if(offset>=0) {
        const auto distance=static_cast<std::uint64_t>(offset);
        if(distance>UINTPTR_MAX-base)return false;
        result=base+static_cast<std::uintptr_t>(distance);return true;
    }
    // Avoid negating INT64_MIN directly. Negative self-relative references are
    // valid native data and must be checked as signed subtraction.
    const auto distance=static_cast<std::uint64_t>(-(offset+1))+1U;
    if(distance>base)return false;
    result=base-static_cast<std::uintptr_t>(distance);return true;
}

[[nodiscard]] inline bool indexed(std::uintptr_t base,std::uint64_t index,
    std::uintptr_t stride,std::uintptr_t& result) noexcept {
    if(index && stride> (UINTPTR_MAX-base)/index)return false;
    const auto distance=index*stride;
    result=base+static_cast<std::uintptr_t>(distance);return true;
}

[[nodiscard]] inline bool source(gateway_native::Read& read,const Request& request,
    std::uintptr_t& root,std::uintptr_t& sourceRow,std::uintptr_t& component,
    std::uintptr_t& definition) noexcept {
    std::uint16_t selector{};std::int64_t relative{};std::uintptr_t stride{};
    if(!read.value(read.image+kSelector,selector) || selector==UINT16_MAX
        || !read.value(read.image+kTable,relative) || !read.value(read.image+kStride,stride)
        || stride<8 || stride>0x100000)return false;
    std::uintptr_t table{},selected{};
    if(!add(read.image+kTable,relative,table)
        || !indexed(table,selector,stride,selected) || !read.value(selected,root))return false;
    std::uintptr_t groupBase{},groupCountAddress{};
    if(!add(root,kGroupOffset,groupBase) || !add(root,8,groupCountAddress))return false;
    std::uint32_t count{};
    if(!read.value(groupCountAddress,count) || count>128)return false;
    for(std::uint32_t index=0;index<count;++index) {
        std::array<std::uint32_t,6> group{};
        std::uintptr_t groupAddress{};
        if(!indexed(groupBase,index,kGroupStride,groupAddress)
            || !read.copy(groupAddress,std::as_writable_bytes(std::span{group})))return false;
        if(group[2]!=request.registry)continue;
        if(group[0]>4096U || group[1]>4096U || request.slot>=group[0])return false;
        const auto sourceIndex=static_cast<std::uint64_t>(group[1])+request.slot;
        if(sourceIndex>std::numeric_limits<std::uint32_t>::max())return false;
        std::uintptr_t sourceBase{},row{};
        if(!add(root,kSourceRoot,sourceBase)
            || !indexed(sourceBase,sourceIndex,kSourceStride,row))return false;
        gateway_native::Ref outer{};
        if(!read.value(row,outer) || outer.handle==UINT32_MAX)return false;
        std::uintptr_t outerBase{};
        if(!read.resolve(outer.handle,outerBase) || !add(outerBase,outer.offset,component))return false;
        std::array<std::byte,16> componentHeader{};
        if(!read.copy(component,std::span(componentHeader)))return false;
        if(gateway_native::at<std::uint32_t>(componentHeader.data())!=request.definition
            || gateway_native::at<std::uint32_t>(componentHeader.data()+4)!=kComponentTag
            || gateway_native::at<std::int64_t>(componentHeader.data()+8)!=request.definitionOffset)return false;
        if(!read.resolve(request.definition,definition)
            || !add(definition,request.definitionOffset,definition))return false;
        std::array<std::byte,0x3C> definitionHeader{};
        if(!read.copy(definition,std::span(definitionHeader)))return false;
        // The definition header binds tag, class, registry, type and slot. Its +8 field is NOT the
        // source row's outer offset: read live on 2026-09-14 it is 0x70 on every Haunted Forest
        // placement while the outer offset is 0, so comparing them refused every ready target and
        // the transit effect could never pulse. The component header above already binds the
        // definition offset, and the whole chain is re-read below against recycling.
        if(gateway_native::at<std::uint32_t>(definitionHeader.data())!=request.definition
            || gateway_native::at<std::uint32_t>(definitionHeader.data()+4)!=kDefinitionTag
            || gateway_native::at<std::uint32_t>(definitionHeader.data()+0x30)!=request.registry
            || gateway_native::at<std::uint8_t>(definitionHeader.data()+0x34)!=4
            || gateway_native::at<std::uint16_t>(definitionHeader.data()+0x36)!=request.slot)return false;
        sourceRow=row;return true;
    }
    return false;
}

[[nodiscard]] inline bool state(gateway_native::Read& read,const Request& request,
    std::uintptr_t component,Identity& identity) noexcept {
    std::uint32_t generation{},child{};std::uint8_t active{};
    if(!read.value(component+0x180,generation) || !read.value(component+0x188,active)
        || generation!=request.generation || active!=(request.active?1U:0U))return false;
    gateway_native::Weak weak{};
    if(!read.value(component+0x440,weak) || !read.value(component+0x444,child))return false;
    if(request.active) {
        std::uint32_t committed{};std::uintptr_t row{};std::uint32_t entityHandle{};
        if(!read.value(component+0x2F0,committed) || committed!=generation || weak.handle==UINT32_MAX
            || child!=weak.handle || !read.entity_row(weak,row) || !read.value(row+0x0C,entityHandle)
            || entityHandle!=weak.handle)return false;
        identity.child=weak;
    } else if(child!=UINT32_MAX || weak.handle!=UINT32_MAX)return false;
    identity.component=component;identity.generation=generation;identity.active=request.active;
    return true;
}
} // namespace detail

/**
 * Read-only readiness probe for the reflected type-4 placement authority. The selected table,
 * source row, component header, definition header, generation and weak child are re-read after
 * the state check; a recycled source therefore cannot qualify the prior request.
 */
[[nodiscard]] inline bool probe(gateway_native::Read& read,const Request& request,
    Identity* output=nullptr) noexcept {
    if(!read.image || !request.registry || !request.definition
        || request.definitionOffset<=0 || request.definitionOffset>0x1000000
        || !request.generation)return false;
    Identity first{};
    if(!detail::source(read,request,first.root,first.source,first.component,first.definition)
        || !detail::state(read,request,first.component,first))return false;
    Identity second{};
    if(!detail::source(read,request,second.root,second.source,second.component,second.definition)
        || !detail::state(read,request,second.component,second)
        || first.root!=second.root || first.source!=second.source || first.component!=second.component
        || first.definition!=second.definition || first.generation!=second.generation
        || first.active!=second.active || first.child!=second.child)return false;
    if(output)*output=second;
    return true;
}
} // namespace sunrise::client::hooks::bootflow::native_local_placement_probe
