#pragma once
#include "coo_native_components.h"
#include <cmath>
namespace dawn::client::hooks::bootflow::coo_native {
struct NativeMount {
    std::uintptr_t image{};
    void controlled(std::uint32_t& out) const noexcept { reinterpret_cast<void(*)(std::uint32_t*)>(image+0x4B2260)(&out); }
    void parent(std::uintptr_t row,std::uint32_t& out) const noexcept { reinterpret_cast<void(*)(std::uintptr_t,std::uint32_t*)>(image+0x5582E0)(row,&out); }
    void position(std::uintptr_t row,std::array<float,4>& out) const noexcept { reinterpret_cast<void(*)(std::uintptr_t,std::array<float,4>*)>(image+0x558330)(row,&out); }
    template<class Read> bool valid(Read& read) const noexcept {
        constexpr std::array<std::uintptr_t,3> entries{0x4B2260,0x5582E0,0x558330};
        constexpr std::array<std::array<std::uint8_t,16>,3> prefixes{{
            {0x40,0x53,0x48,0x83,0xEC,0x20,0x48,0x8B,0xD9,0xC7,0x01,0xFF,0xFF,0xFF,0xFF,0x48},
            {0x48,0x83,0xC1,0xA0,0xC7,0x02,0xFF,0xFF,0xFF,0xFF,0xB8,0x00,0x00,0x00,0x00,0xF6},
            {0x40,0x57,0x48,0x83,0xEC,0x40,0x48,0x83,0xC1,0xA0,0xB8,0x00,0x00,0x00,0x00,0x48}}};
        for(std::size_t i=0;i<entries.size();++i) {
            std::array<std::uint8_t,16> bytes{};
            if(!read.value(image+entries[i],bytes) || bytes!=prefixes[i]) { return false; }
        }return true;
    }
};
struct MountedPlayer {
    std::uint32_t player{UINT32_MAX},vehicle{UINT32_MAX},seat{UINT32_MAX};
    std::array<float,4> position{};
};
// Position observation only needs the current controlled entity and its native
// parent, not a particular vehicle's seat definition. Keep the stricter Pike
// seat verifier below for callers that need to establish driver ownership.
template<class Read,class Native> bool controlled_player(Read& read,Native& native,MountedPlayer& result) noexcept {
    result={};MountedPlayer candidate{};native.controlled(candidate.player);
    if(candidate.player==UINT32_MAX) {return false;}
    std::uintptr_t table{};std::uint32_t stride{};
    if(!read.value(read.image+0x1F93428,table) || table<0x10000
        || !read.value(read.image+0x1F93430,stride) || stride<0xE0 || stride>0x1000
        || table>UINTPTR_MAX-8192ULL*stride) {return false;}
    const auto row=[&](std::uint32_t entity) {return table+static_cast<std::uintptr_t>(entity&0x1FFFU)*stride;};
    const auto owned=[&](std::uint32_t entity) {
        std::uint32_t self{},flags{};
        return entity!=UINT32_MAX && read.value(row(entity)+0xC,self) && self==entity
            && read.value(row(entity)+4,flags) && (flags&5U)==0;
    };
    if(!owned(candidate.player)) {return false;}
    native.parent(row(candidate.player)+0x60,candidate.vehicle);
    if(candidate.vehicle==candidate.player || (candidate.vehicle!=UINT32_MAX && !owned(candidate.vehicle))) {return false;}
    native.position(row(candidate.player)+0x60,candidate.position);
    for(float lane:candidate.position) {if(!std::isfinite(lane) || std::abs(lane)>100000.F) {return false;}}
    std::uint32_t playerAfter{UINT32_MAX},parentAfter{UINT32_MAX};
    native.controlled(playerAfter);native.parent(row(candidate.player)+0x60,parentAfter);
    if(playerAfter!=candidate.player || parentAfter!=candidate.vehicle || !owned(candidate.player)
        || (candidate.vehicle!=UINT32_MAX && !owned(candidate.vehicle))) {return false;}
    result=candidate;return true;
}
// Mounting removes the character's foot physics body. Its native world transform
// remains valid; its parent and the Pike driver seat must agree on ownership.
template<class Read,class Native> bool mounted_pike(Read& read,Native& native,MountedPlayer& result) noexcept {
    result={};MountedPlayer candidate{};native.controlled(candidate.player);
    if(candidate.player==UINT32_MAX) { return false; }
    std::uintptr_t table{};std::uint32_t stride{};
    if(!read.value(read.image+0x1F93428,table) || table<0x10000
        || !read.value(read.image+0x1F93430,stride) || stride<0xE0 || stride>0x1000
        || table>UINTPTR_MAX-8192ULL*stride) { return false; }
    const auto row=[&](std::uint32_t entity) { return table+static_cast<std::uintptr_t>(entity&0x1FFFU)*stride; };
    const auto owned=[&](std::uint32_t entity) {
        std::uint32_t self{},flags{};
        return read.value(row(entity)+0xC,self) && self==entity
            && read.value(row(entity)+4,flags) && (flags&5U)==0;
    };
    if(!owned(candidate.player)) { return false; }
    native.parent(row(candidate.player)+0x60,candidate.vehicle);
    if(candidate.vehicle==UINT32_MAX || candidate.vehicle==candidate.player || !owned(candidate.vehicle)) { return false; }
    std::uint32_t bundle{},tag{},rider{};std::int64_t definition{};std::uintptr_t seat{};
    // The observed vehicle has 366 interface rows. Enemy probes retain their
    // existing 256-row bound; the reader also enforces its total byte budget.
    if(!read.value(row(candidate.vehicle)+0x4C,bundle)
        || !component<Read,1024>(read,bundle,candidate.vehicle,0x808071BCU,seat)
        || !read.value(seat,tag) || tag!=0x80FDA97DU
        || !read.value(seat+8,definition) || definition!=0xD8
        || !read.value(seat+0x24,candidate.seat)
        || !read.value(seat+0x48,rider) || rider!=candidate.player) { return false; }
    native.position(row(candidate.player)+0x60,candidate.position);
    for(float lane:candidate.position) { if(!std::isfinite(lane) || std::abs(lane)>100000.F) { return false; } }
    std::uint32_t playerAfter{UINT32_MAX},parentAfter{UINT32_MAX},seatAfter{UINT32_MAX};
    native.controlled(playerAfter);native.parent(row(candidate.player)+0x60,parentAfter);
    if(playerAfter!=candidate.player || parentAfter!=candidate.vehicle
        || !owned(candidate.player) || !owned(candidate.vehicle)
        || !read.value(seat+0x24,seatAfter) || seatAfter!=candidate.seat
        || !read.value(seat+0x48,rider) || rider!=candidate.player) { return false; }
    result=candidate;return true;
}
}
