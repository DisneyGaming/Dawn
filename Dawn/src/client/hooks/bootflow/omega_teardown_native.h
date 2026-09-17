#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

#include "../../content/handles/handle_resolver.h"

namespace dawn::client::hooks::bootflow::omega_teardown_native {

using Source = content::handles::Source;

template<class T>
[[nodiscard]] inline bool read(const Source& source,std::uintptr_t address,T& value) noexcept {
    return address!=0 && source.read!=nullptr
        && source.read(source.context,address,
            std::span(reinterpret_cast<std::byte*>(&value),sizeof value));
}
[[nodiscard]] inline bool add(std::uintptr_t base,std::uint64_t offset,
                              std::uintptr_t& result) noexcept {
    if(offset>(std::numeric_limits<std::uintptr_t>::max)()-base) { return false; }
    result=base+static_cast<std::uintptr_t>(offset);return true;
}
[[nodiscard]] inline bool add_signed(std::uintptr_t base,std::int64_t offset,
                                     std::uintptr_t& result) noexcept {
    if(offset>=0) { return add(base,static_cast<std::uint64_t>(offset),result); }
    const auto magnitude=0ULL-static_cast<std::uint64_t>(offset);
    if(magnitude>base) { return false; }
    result=base-static_cast<std::uintptr_t>(magnitude);return true;
}
struct Object final {
    std::uintptr_t directory{},registry{},table{},elements{},element{},address{};
    std::uint32_t tableIndex{};
    std::int32_t stride{-1};
};

/** Native 30CAB0/4D7C00/4E3B40 walk. The global points to a directory object,
 * whose first pointer is the descriptor array. Runtime datum tables and content
 * tables use the same walk; content-only table restrictions do not apply here. */
[[nodiscard]] inline Object resolve(const Source& source,std::uint32_t handle) noexcept {
    Object result{};
    if(handle==UINT32_MAX || !read(source,source.tablesSlot,result.directory)
        || !read(source,result.directory,result.registry) || result.registry==0) { return result; }
    const auto high=(handle>>13)|((handle&0x80000000U)!=0 ? 0xFFF80000U : 0U);
    result.tableIndex=static_cast<std::uint32_t>(
        ((static_cast<std::uint64_t>(high)|0x0FFC0000ULL)>>18)&(high&0xFFFFU));
    std::uintptr_t field{};
    std::int32_t mask{};
    if(!add(result.registry,static_cast<std::uint64_t>(result.tableIndex)*0x40U,result.table)
        || !add(result.table,8,field) || !read(source,field,result.elements)
        || !add(result.table,0x30,field) || !read(source,field,result.stride)
        || !add(result.table,0x34,field) || !read(source,field,mask)
        || result.elements==0 || result.stride<=0) { return result; }
    const auto delta=static_cast<std::uint64_t>(handle&0x1FFFU)*static_cast<std::uint32_t>(result.stride);
    std::uint64_t relocation{};
    if(delta>UINT32_MAX || !add(result.elements,delta,result.element)
        || !add(result.element,8,field) || !read(source,field,relocation)) { return result; }
    const auto correction=relocation&static_cast<std::uint64_t>(static_cast<std::int64_t>(mask));
    // Native subtraction accepts negative relocation deltas as well as positive ones.
    if((correction>>63)!=0) {
        if(!add(result.element,0ULL-correction,result.address)) { result.address=0; }
    } else if(correction<=result.element) { result.address=result.element-static_cast<std::uintptr_t>(correction); }
    return result;
}

/** The descriptor offset in a scene component is signed, exactly as 4E3B40.
 * This is diagnostic resolution only; it does not decide whether to destroy it. */
[[nodiscard]] inline std::uintptr_t scene_object(const Source& source,std::uintptr_t component,
                                                Object& object,std::uint32_t& handle) noexcept {
    std::uintptr_t field{},result{};std::int64_t offset{};
    if(!read(source,component,handle) || !add(component,8,field) || !read(source,field,offset)) { return 0; }
    object=resolve(source,handle);
    return object.address!=0 && add_signed(object.address,offset,result) ? result : 0;
}

/** 4D7C00 assumes the row's key is present. 4D5CB0 then changes only the two
 * neighbors. Verify those exact full-datum links before forwarding unchanged:
 * a stale salt can resolve the same slot, so the key alone is insufficient.
 * Local checks avoid an O(n squared) full-chain walk during mass teardown. */
[[nodiscard]] inline bool can_unregister(const Source& source,std::uintptr_t list,
                                         std::uint32_t datum) noexcept {
    const auto object=resolve(source,datum);
    std::uint32_t key{};std::int32_t count{};
    if(object.address==0 || !read(source,object.address,key) || !read(source,list,count)
        || count<=0 || count>=0x10000) { return false; }
    std::uintptr_t group{};
    for(std::int32_t i=0;i<count;++i) {
        std::uintptr_t row{};std::uint32_t candidate{};
        if(!add(list,4U+static_cast<std::uint64_t>(i)*16U,row) || !read(source,row,candidate)) { return false; }
        if(candidate==key) { group=row; } // Native lookup retains the last matching key.
    }
    std::uintptr_t field{};std::uint32_t head{},tail{};
    if(group==0 || !add(group,4,field) || !read(source,field,head)
        || !add(group,8,field) || !read(source,field,tail)) { return false; }
    std::uint32_t previous{},next{};
    if(!add(object.address,0x74,field) || !read(source,field,previous)
        || !add(object.address,0x78,field) || !read(source,field,next)
        || previous==datum || next==datum
        || (previous!=UINT32_MAX && previous==next)
        || (previous==UINT32_MAX)!=(head==datum)
        || (next==UINT32_MAX)!=(tail==datum)) { return false; }
    const auto neighbor_matches=[&](std::uint32_t neighbor,std::uint32_t offset) noexcept {
        if(neighbor==UINT32_MAX) { return true; }
        const auto node=resolve(source,neighbor);
        std::uint32_t nodeKey{},back{};std::uintptr_t link{};
        return node.address!=0 && read(source,node.address,nodeKey) && nodeKey==key
            && add(node.address,offset,link) && read(source,link,back) && back==datum;
    };
    return neighbor_matches(previous,0x78) && neighbor_matches(next,0x74);
}

/** 4D92A0(list,stream) resolves list+820, then calls
 * 3CCE50(resolvedOwner+11208,list+8B50). The receiver mirror is ctx+8;
 * the distinct list serialization buffers are NOT the applied roster.
 * 3CB7E0 clears this mirror on reactivation, explaining the post-hang snapshot. */
inline constexpr std::array<std::uint32_t,7> kLairKeys{
    0xF4D0E0B2U,0x95FB2E01U,0x0040BF06U,0x0040BF05U,
    0x0040BF03U,0x99BD2FEBU,0x0040BF04U};
inline constexpr std::size_t kRosterBlockBytes=0x1F8;
struct Retirement final {
    std::uintptr_t owner{}, context{}, parent{};
    std::uint64_t identity{};
    std::uint32_t handle{UINT32_MAX}, block{};
    [[nodiscard]] bool valid() const noexcept { return owner!=0; }
};
[[nodiscard]] inline bool retirement_owner(const Source& source,std::uintptr_t context,
                                           Retirement& result,std::uint32_t expectedScenario=0x80F47522U) noexcept {
    if(context<0x11208) { return false; }
    const auto owner=context-0x11208;
    std::uint32_t handle{},back{},scenario{},mirrorScenario{};
    std::uint8_t active{};std::uintptr_t parent{};std::uint64_t identity{};
    if(!read(source,context,handle) || handle==UINT32_MAX
        || !read(source,context+4,mirrorScenario) || mirrorScenario!=expectedScenario
        || !read(source,owner+0x848,back) || back!=handle
        || resolve(source,handle).address!=owner
        || !read(source,owner+0x24,scenario) || scenario!=mirrorScenario
        || !read(source,owner+0x20,active) || active!=1
        || !read(source,owner+0x10,parent) || parent==0
        || !read(source,owner+0x18,identity)) { return false; }
    result={owner,context,parent,identity,handle,0};return true;
}
[[nodiscard]] inline bool same_retirement_owner(const Source& source,const Retirement& prior,std::uint32_t expectedScenario=0x80F47522U) noexcept {
    Retirement now{};
    return prior.valid() && retirement_owner(source,prior.context,now,expectedScenario)
        && now.owner==prior.owner && now.parent==prior.parent
        && now.identity==prior.identity && now.handle==prior.handle;
}
[[nodiscard]] inline bool lair_groups(const Source& source,std::uintptr_t owner,bool present) noexcept {
    std::int32_t count{};
    if(!read(source,owner+0x28,count) || count<0 || count>128) { return false; }
    std::array<unsigned,7> found{};
    for(std::int32_t i=0;i<count;++i) {
        std::uint32_t key{};
        if(!read(source,owner+0x2C+static_cast<std::uintptr_t>(i)*16,key)) { return false; }
        for(std::size_t j=0;j<kLairKeys.size();++j) { if(key==kLairKeys[j]) { ++found[j]; } }
    }
    for(auto n:found) { if(n!=(present?1U:0U)) { return false; } }
    return true;
}
struct RosterBlock final { std::array<std::uint32_t,kRosterBlockBytes/4> words{}; };
[[nodiscard]] inline bool lair_block(const RosterBlock& block,bool present) noexcept {
    if(block.words[0]!=14 || block.words[1]!=kLairKeys.size() || block.words[0x194/4]!=7) { return false; }
    for(std::size_t i=0;i<kLairKeys.size();++i) {
        if(block.words[i+2]!=kLairKeys[i]) { return false; }
    }
    return block.words[0x188/4]==(present?0x7FU:0U)
        && block.words[0x18C/4]==0 && block.words[0x190/4]==0;
}
/** Only explicit seven-key, same-ordinal 1 -> 0 removal qualifies. Omission,
 * reordered blocks, changed generation, and partial removal cannot acknowledge. */
[[nodiscard]] inline Retirement begin_retirement(const Source& source,std::uintptr_t context,
                                                std::uintptr_t delta,std::uint32_t bubble) noexcept {
    Retirement result{};
    if(bubble!=14 || !retirement_owner(source,context,result)
        || !lair_groups(source,result.owner,true)) { return {}; }
    std::int32_t before{},after{};
    if(!read(source,context+8+0x528,before) || !read(source,delta+0x528,after)
        || before<1 || before>64 || after<1 || after>64) { return {}; }
    unsigned matches{};
    for(std::int32_t i=0;i<after;++i) {
        const auto offset=0x52CU+static_cast<std::uintptr_t>(i)*kRosterBlockBytes;
        std::uint32_t id{};
        if(!read(source,delta+offset,id)) { return {}; }
        if(id!=14) { continue; }
        if(++matches!=1 || i>=before) { return {}; }
        RosterBlock oldBlock{},newBlock{};
        if(!read(source,context+8+offset,oldBlock) || !read(source,delta+offset,newBlock)
            || !lair_block(oldBlock,true) || !lair_block(newBlock,false)
            || oldBlock.words[0x194/4]!=newBlock.words[0x194/4]) { return {}; }
        const auto* oldStates=reinterpret_cast<const std::byte*>(oldBlock.words.data())+0x198;
        const auto* newStates=reinterpret_cast<const std::byte*>(newBlock.words.data())+0x198;
        for(std::size_t j=0;j<kLairKeys.size();++j) { if(oldStates[j]!=newStates[j]) { return {}; } }
        result.block=static_cast<std::uint32_t>(i);
    }
    return matches==1 && same_retirement_owner(source,result) ? result : Retirement{};
}
/** After original 3CCE50 has returned: its mirror must contain the removal and
 * every old group descriptor must actually be absent. No native cleanup is called. */
[[nodiscard]] inline bool finish_retirement(const Source& source,const Retirement& prior,
                                            std::uint32_t bubble) noexcept {
    if(bubble!=14 || !same_retirement_owner(source,prior)
        || !lair_groups(source,prior.owner,false)) { return false; }
    std::int32_t count{};RosterBlock block{};
    return read(source,prior.context+8+0x528,count) && count>0 && count<=64
        && prior.block<static_cast<std::uint32_t>(count)
        && read(source,prior.context+8+0x52C+prior.block*kRosterBlockBytes,block)
        && lair_block(block,false) && same_retirement_owner(source,prior);
}

} // namespace dawn::client::hooks::bootflow::omega_teardown_native
