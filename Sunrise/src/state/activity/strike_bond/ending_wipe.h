#pragma once
#include "ending_flow.h"
#include <cmath>
#include <cstring>
#include <span>
namespace sunrise::state::activity::strike_bond::ending_wipe {
template<class T> T field(std::span<const std::byte> bytes,std::size_t at) noexcept {
    T value{};if(at<=bytes.size() && sizeof(T)<=bytes.size()-at)std::memcpy(&value,bytes.data()+at,sizeof(T));return value;
}
// Authored graph80F45178 record6: entry2 (flight, bank row5) -> node1
// (wipe, row9/clip80F1FD8D) -> node0 (non-looping idle, row1/clip80F4517C).
// F4E660 commits +B0/+B4 only after native clip loading succeeds.
inline EndingAnimationPhase phase(std::span<const std::byte> bytes,EndingActor actor,std::uint32_t biped,char nativeResult) noexcept {
    if(bytes.size()<0xB8 || !actor.valid() || biped==UINT32_MAX || nativeResult!=1
        || field<std::uint32_t>(bytes,0)!=actor.entity || field<std::uint32_t>(bytes,4)!=actor.controller
        || field<std::uint32_t>(bytes,8)!=0 || field<std::uint32_t>(bytes,0xC)!=7
        || field<std::uint32_t>(bytes,0x10)!=0x80F45178U || field<std::uint32_t>(bytes,0x14)!=biped
        || field<std::uint32_t>(bytes,0x18)!=actor.entity || field<std::uint32_t>(bytes,0x1C)!=biped
        || bytes[0x21]==std::byte{} || field<std::uint32_t>(bytes,0xB4)!=6
        || field<std::uint32_t>(bytes,0xA8)!=6)return EndingAnimationPhase::unavailable;
    const auto node=field<std::uint32_t>(bytes,0xB0),row=field<std::uint32_t>(bytes,0x30);
    const auto duration=field<float>(bytes,0x38),elapsed=field<float>(bytes,0x3C);
    if(node>2 || field<std::uint32_t>(bytes,0xA4)!=node || !std::isfinite(duration) || !std::isfinite(elapsed)
        || elapsed<0.F || duration<=0.F || elapsed>duration+0.001F)return EndingAnimationPhase::unavailable;
    const auto flags=std::to_integer<unsigned>(bytes[0x20])&1U;
    if(node==2 && row==5 && flags==0 && std::abs(duration-8.700000762939453F)<0.00001F)return EndingAnimationPhase::flight;
    if(node==1 && row==9 && flags==0 && std::abs(duration-15.133334159851074F)<0.00001F)return EndingAnimationPhase::wipe;
    if(node==0 && row==1 && flags==0 && std::abs(duration-10.F)<0.00001F)return EndingAnimationPhase::finished;
    return EndingAnimationPhase::unavailable;
}
}
