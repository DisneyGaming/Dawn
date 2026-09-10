#pragma once
#include <array>
#include <cstring>
#include <span>
#include "../../../state/activity/hijacked/frame.h"
namespace sunrise::client::hooks::bootflow::hijacked_presentation {
namespace mission=state::activity::hijacked;
template<class T> inline T read(std::span<const std::byte> b,std::size_t o) noexcept { T v{};if(o<=b.size() && sizeof v<=b.size()-o) { std::memcpy(&v,b.data()+o,sizeof v); }return v; }
template<class T> inline void put(std::span<std::byte> b,std::size_t o,T v) noexcept { std::memcpy(b.data()+o,&v,sizeof v); }
inline bool source(std::span<const std::byte> b,bool dialogue) noexcept {
    return b.size()>=0x58 && read<std::uint32_t>(b,0)==(dialogue?0x80B4241FU:0x80B4241CU)
        && read<std::uint32_t>(b,4)==(dialogue?0x80804F4CU:0x80804F54U)
        && read<std::int64_t>(b,8)==(dialogue?0x1408:0xB88)
        && read<std::uint32_t>(b,0x48)!=UINT32_MAX
        && read<std::uint32_t>(b,0x4C)==(dialogue?0x80804F4BU:0x80804F53U)
        && read<std::int64_t>(b,0x50)==0;
}
// Same requested data as the native wire body. The processed-generation array is untouched.
inline bool dialogue_records(std::span<std::byte> b,const mission::Frame& f) noexcept {
    if(!f.enabled || !f.spawnGeneration || f.activeRow>=f.generations.size()
        || b.size()<0x188+f.generations.size()*32 || !source(b,true)) { return false; }
    const auto row=f.activeRow;const auto generation=f.generations[row];
    if(!generation) { return false; }
    const auto o=0x188+row*32;
    if(read<std::uint32_t>(b,o+24)==generation && read<std::uint8_t>(b,o+28)==2
        && read<std::uint64_t>(b,o+8)==1) { return false; }
    put(b,o,UINT64_MAX);put<std::uint64_t>(b,o+8,1);
    put(b,o+16,UINT64_C(0xFFFF00FF811C9DC5));put(b,o+24,generation);
    put<std::uint8_t>(b,o+28,2);return true;
}
using Build=void(__fastcall*)(void*,std::uint32_t) noexcept;
void update_directive(void*,Build) noexcept;
void update_dialogue(std::byte*) noexcept;
}
