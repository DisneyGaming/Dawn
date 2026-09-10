#pragma once
#include "frame.h"
#include <cmath>
#include <cstring>
#include <span>
namespace sunrise::state::activity::hijacked::boss_damage {
inline constexpr float floor(std::uint8_t stage) noexcept { return stage==0?2.F/3.F:stage==1?1.F/3.F:0.F; }
inline bool blocked(const BossRequest& request,float fraction) noexcept {
    return request.owner.valid() && request.enemy.valid() && request.stage<3 && std::isfinite(fraction)
        && ((request.stage>0 && request.requested) || (request.stage<2 && fraction<=floor(request.stage)));
}
enum class Packet { invalid, unchanged, clamped };
template<class T> T get(std::span<const std::byte> bytes,std::size_t offset) noexcept {
    T value{};std::memcpy(&value,bytes.data()+offset,sizeof value);return value;
}
// B804E0 consumes 12-byte entries at packet+68: flags, global region, target
// health fraction. These are resulting fractions, not damage amounts. Preserve
// every field except an owned body target that would cross this phase's floor.
inline Packet clamp(std::span<std::byte> packet,std::int32_t bodyRegion,float minimum) noexcept {
    if(packet.size()<0x68 || !std::isfinite(minimum) || minimum<0 || minimum>1) {return Packet::invalid;}
    const auto count=get<std::int32_t>(packet,0x64);
    if(count<0 || count>32 || packet.size()<0x68+static_cast<std::size_t>(count)*12) {return Packet::invalid;}
    for(int i=0;i<count;++i) {
        const auto entry=0x68+static_cast<std::size_t>(i)*12;
        if(get<std::int32_t>(packet,entry+4)==bodyRegion && !std::isfinite(get<float>(packet,entry+8))) {return Packet::invalid;}
    }
    bool changed{};
    for(int i=0;i<count;++i) {
        const auto entry=0x68+static_cast<std::size_t>(i)*12;
        if(minimum>0 && get<std::int32_t>(packet,entry+4)==bodyRegion && get<float>(packet,entry+8)<minimum) {
            std::memcpy(packet.data()+entry+8,&minimum,sizeof minimum);changed=true;
        }
    }
    return changed?Packet::clamped:Packet::unchanged;
}
}
