#pragma once
#include "controller.h"
#include "../../../../client/hooks/bootflow/coo_enemy_readiness.h"
#include <cmath>
#include <cstring>

namespace dawn::state::activity::newlight::launchpad::ghost {
namespace native=client::hooks::bootflow::coo_native;
template<class T> T field(std::span<const std::byte> b,std::size_t offset) noexcept {
    T value{};if(offset<=b.size() && sizeof value<=b.size()-offset) {std::memcpy(&value,b.data()+offset,sizeof value);}return value;
}
struct Components {std::uint32_t entity{},character{},selector{},biped{};};
// Read the two-slot authored motion arena, not requested queue/cursor state.
// Native action39 embeds F4E660's loaded animation graph at action+28.
inline Sample decode(std::span<const std::byte> b,const Components& owner) noexcept {
    if(b.size()!=0x4C0 || field<std::uint64_t>(b,0x50)!=2 || field<std::uint64_t>(b,0x58)!=0x268
        || field<std::uint64_t>(b,0x60)!=2 || field<std::uint64_t>(b,0x68)!=0x278
        || field<std::uint64_t>(b,0x70)!=0x1B0 || field<std::uint64_t>(b,0x78)!=0x288) {return {};}
    // Native segmented allocator: +80 is an occupied-slot BITSET. The
    // slot's low16 selects a six-byte segment record, not a byte offset.
    const auto occupied=field<std::uint32_t>(b,0x80),special=field<std::uint32_t>(b,0x84);
    const auto count=static_cast<unsigned>(std::popcount(occupied));
    if((occupied&~3U) || (special&~occupied) || field<std::uint16_t>(b,0x88)!=count
        || field<std::uint16_t>(b,0x8A)!=count || field<std::uint16_t>(b,0x8C)!=std::popcount(special)) {return {};}
    std::array<std::size_t,2> offsets{},sizes{};unsigned segments{};
    for(unsigned i=0;i<2;++i) {
        if(!(occupied&(1U<<i))) {continue;}
        const auto segment=field<std::uint16_t>(b,0x2D0+4U*i);
        if(segment>=count || (segments&(1U<<segment))) {return {};}
        segments|=1U<<segment;const auto record=0x2F0+6U*segment;
        offsets[i]=field<std::uint16_t>(b,record);sizes[i]=field<std::uint16_t>(b,record+2);
        if(field<std::uint16_t>(b,record+4)!=i || !sizes[i] || offsets[i]>=0x1B0 || sizes[i]>0x1B0-offsets[i]) {return {};}
        if(i && sizes[0] && offsets[0]<offsets[1]+sizes[1] && offsets[1]<offsets[0]+sizes[0]) {return {};}
    }
    Sample result{};unsigned matches{};
    for(unsigned i=0;i<2;++i) {
        if(!sizes[i] || field<std::uint8_t>(b,0x2D2+4U*i)!=0x39) {continue;}
        if(sizes[i]!=0x150) {return {};}
        const auto raw=b.subspan(0x310+offsets[i],0x28+0xB8);
        if(field<std::uint32_t>(raw,0)!=0x80FE20C3U || field<std::uint32_t>(raw,4)!=0x80806872U
            || field<std::uint64_t>(raw,8)!=0x210 || field<std::uint32_t>(raw,0x10)!=0x80BFDE65U
            || field<std::uint32_t>(raw,0x18)!=owner.selector || field<std::uint32_t>(raw,0x1C)!=0x8080686BU
            || field<std::uint64_t>(raw,0x20)!=0) {continue;}
        const auto graph=raw.subspan(0x28);const auto node=field<std::uint32_t>(graph,0xB0);
        if(field<std::uint32_t>(graph,0)!=owner.entity || field<std::uint32_t>(graph,4)!=owner.character
            || field<std::uint32_t>(graph,8)!=0 || field<std::uint32_t>(graph,0xC)!=0
            || field<std::uint32_t>(graph,0x10)!=0x80C0E38DU || field<std::uint32_t>(graph,0x14)!=owner.biped
            || field<std::uint32_t>(graph,0x18)!=owner.entity || field<std::uint32_t>(graph,0x1C)!=owner.biped
            || graph[0x21]==std::byte{} || field<std::uint32_t>(graph,0xB4)!=0 || node>=std::size(kRows)
            || field<std::uint32_t>(graph,0xA8)!=0 || field<std::uint32_t>(graph,0x30)!=kRows[node]) {continue;}
        const auto elapsed=field<float>(graph,0x3C),duration=field<float>(graph,0x38);
        if(!std::isfinite(elapsed) || elapsed<0.F || !std::isfinite(duration)
            || std::abs(duration-kSeconds[node])>0.00001F || elapsed>duration+0.001F) {continue;}
        result={static_cast<std::uint8_t>(node),node==6 && (std::to_integer<unsigned>(graph[0x20])&3U)==0
            && field<std::uint32_t>(graph,0xA4)==node && elapsed>=duration};++matches;
    }
    return matches==1?result:Sample{};
}
template<class Read> bool header(Read& read,std::uintptr_t address,std::uint32_t tag,std::uint32_t kind,
    std::uint64_t offset,std::uint32_t entity,std::uint32_t& self) noexcept {
    std::array<std::byte,0x30> b{};std::uintptr_t resolved{};
    if(!read.copy(address,b) || field<std::uint32_t>(b,0)!=tag || field<std::uint32_t>(b,4)!=kind
        || field<std::uint64_t>(b,8)!=offset || field<std::uint32_t>(b,0x2C)!=entity) {return false;}
    self=field<std::uint32_t>(b,0x24);return self!=UINT32_MAX && read.resolve(self,resolved) && resolved==address;
}
// Runs read-only in the existing frame poll. All pointers are local to this
// sample; full handles, source generation and backlinks are checked again.
template<class Read> Sample sample(Read& read,std::uintptr_t image,const EnemyReceipt& actor,unsigned& reason) noexcept {
    reason=1;if(!actor.valid() || actor.registry!=kBreach || actor.source!=27) {return {};}
    reason=2;const auto ready=native::enemy(read,image,actor);if(!ready.created || !ready.ai) {return {};}
    std::uintptr_t actors{},entities{},row{},character{},selector{},motion{},biped{};
    std::uint32_t stride{},entity{},bundle{};Components owner{};reason=3;
    if(!read.value(image+0x1F9D7F8,actors) || !read.value(image+0x1F9D800,stride) || stride<0x70 || stride>0x100000
        || !read.value(actors+static_cast<std::uintptr_t>(actor.actor&0x1FFFU)*stride+0x4C,entity)
        || entity==UINT32_MAX || !read.value(image+0x1F93428,entities)
        || !read.value(image+0x1F93430,stride) || stride<0x9C || stride>0x100000) {return {};}
    row=entities+static_cast<std::uintptr_t>(entity&0x1FFFU)*stride;
    if(!read.value(row+0x4C,bundle)
        || !native::component<Read,1024>(read,bundle,entity,0x80806832U,character)
        || !native::component<Read,1024>(read,bundle,entity,0x8080686CU,selector)
        || !native::component<Read,1024>(read,bundle,entity,0x808069EEU,motion)
        || !native::component<Read,1024>(read,bundle,entity,0x808036CFU,biped)) {return {};}
    owner.entity=entity;std::uint32_t motionSelf{},value{};reason=4;
    if(!header(read,character,0x815798E2U,0x80806832U,0x6E8,entity,owner.character)
        || !header(read,selector,0x80FE20C3U,0x8080686CU,0x1C8,entity,owner.selector)
        || !header(read,motion,0x8162C421U,0x808069EEU,0xAF8,entity,motionSelf)
        || !header(read,biped,0x815798DFU,0x808036CFU,0x3B40,entity,owner.biped)
        || !read.value(character+0xC0,value) || value!=actor.actor
        || !read.value(selector+0x30,value) || value!=owner.character
        || !read.value(biped+0xD0,value) || value!=0x81579902U) {return {};}
    reason=5;std::array<std::byte,0x4C0> bytes{};if(!read.copy(motion,bytes)) {return {};}
    const auto result=decode(bytes,owner);if(result.node==UINT8_MAX) {return {};}
    reason=6;std::uintptr_t resolved{};
    // Reject an observation spanning retirement/rebinding or a graph transition.
    std::array<std::byte,0x4C0> after{};const auto current=native::enemy(read,image,actor);
    if(!current.created || !current.ai || !read.resolve(motionSelf,resolved) || resolved!=motion
        || !read.value(character+0xC0,value) || value!=actor.actor
        || !read.value(character+0x2C,value) || value!=entity
        || !read.value(selector+0x30,value) || value!=owner.character
        || !read.copy(motion,after) || decode(after,owner).node!=result.node) {return {};}
    reason=0;return result;
}
}
