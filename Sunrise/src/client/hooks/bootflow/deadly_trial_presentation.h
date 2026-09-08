#pragma once
#include <array>
#include <cstring>
#include <span>
#include "../../../state/activity/deadly_trial/frame.h"
#include "../../../state/activity/deadly_trial/navigation.h"
namespace sunrise::client::hooks::bootflow::deadly_trial_presentation {
namespace trial=state::activity::deadly_trial;
template<class T> inline T read(std::span<const std::byte> b,std::size_t o) noexcept { T v{};if(o<=b.size() && sizeof v<=b.size()-o) { std::memcpy(&v,b.data()+o,sizeof v); }return v; }
template<class T> inline void put(std::span<std::byte> b,std::size_t o,T v) noexcept { std::memcpy(b.data()+o,&v,sizeof v); }
inline bool source(std::span<const std::byte> b,bool dialogue) noexcept {
    return b.size()>=0x58 && read<std::uint32_t>(b,0)==(dialogue?0x80B2E709U:0x80B2E706U)
        && read<std::uint32_t>(b,4)==(dialogue?0x80804F4CU:0x80804F54U)
        && read<std::int64_t>(b,8)==(dialogue?0x1408:0xB88)
        && read<std::uint32_t>(b,0x48)!=UINT32_MAX
        && read<std::uint32_t>(b,0x4C)==(dialogue?0x80804F4BU:0x80804F53U)
        && read<std::int64_t>(b,0x50)==0;
}
// Same requested data as the native wire body. The processed-generation array is untouched.
inline bool dialogue_records(std::span<std::byte> b,const trial::Frame& f) noexcept {
    if(!f.enabled || !f.spawnGeneration || b.size()<0x188+11*32 || !source(b,true)) { return false; }
    bool changed{};
    for(std::size_t row=0;row<f.generations.size();++row) {
        if(!f.generations[row]) { continue; }
        const auto o=0x188+row*32;const auto active=f.activeRow==row;
        if(read<std::uint32_t>(b,o+24)==f.generations[row] && read<std::uint8_t>(b,o+28)==(active?2:0)
            && read<std::uint64_t>(b,o+8)==(active?1U:0U)) { continue; }
        put(b,o,UINT64_MAX);put<std::uint64_t>(b,o+8,active?1:0);
        put(b,o+16,UINT64_C(0xFFFF00FF811C9DC5));put(b,o+24,f.generations[row]);
        put<std::uint8_t>(b,o+28,active?2:0);changed=true;
    }return changed;
}
inline bool route_point(std::span<std::byte> b,const trial::Frame& f) noexcept {
    if(b.size()<0xB00 || !source(b,false) || !f.enabled) { return false; }
    const auto nav=trial::navigation::goal(f.presentation.event);
    const bool active=f.presentation.active && nav.target.valid();
    put<std::uint8_t>(b,0x484,active?3:0);put<std::uint8_t>(b,0x48C,2);
    if(active) { put(b,0x498,nav.bubble);put(b,0x4A0,std::array<float,4>{nav.position.x,nav.position.y,nav.position.z,1.F}); }
    return true;
}
using Build=void(__fastcall*)(void*,std::uint32_t) noexcept;
using Register=void(__fastcall*)(const void*) noexcept;
void update_directive(void*,Build,Register) noexcept;
void update_dialogue(std::byte*) noexcept;
}
