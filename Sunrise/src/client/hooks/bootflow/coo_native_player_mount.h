#pragma once
#include "coo_native_components.h"
#include <cmath>
namespace sunrise::client::hooks::bootflow::coo_native {
struct MountedPlayer {
    std::uint32_t player{UINT32_MAX},vehicle{UINT32_MAX},seat{UINT32_MAX};
    std::array<float,4> position{};
};
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
