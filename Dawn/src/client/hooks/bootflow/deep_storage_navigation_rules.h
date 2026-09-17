#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <span>
#include "../../../state/activity/deep_storage/mechanism_bindings.h"

namespace dawn::client::hooks::bootflow::deep_storage_navigation {
namespace mission=state::activity::deep_storage;
template<class T> inline T read(std::span<const std::byte> b,std::size_t offset) noexcept {
    T value{};std::memcpy(&value,b.data()+offset,sizeof value);return value;
}
template<class T> inline void put(std::span<std::byte> b,std::size_t offset,T value) noexcept {
    std::memcpy(b.data()+offset,&value,sizeof value);
}
inline bool source(std::span<const std::byte> b) noexcept {
    return b.size()>=0x58 && read<std::uint32_t>(b,0)==0x80B565DFU
        && read<std::uint32_t>(b,4)==0x80804F54U && read<std::int64_t>(b,8)==0xB88
        && read<std::uint32_t>(b,0x48)!=UINT32_MAX && read<std::uint32_t>(b,0x4C)==0x80804F53U
        && read<std::int64_t>(b,0x50)==0;
}
struct Result {bool handled{};std::uint16_t publish{};};
// This package-local ActivityPoint was verified through native 5012E0 and C73820.
// Kind 3 renders the small fixed waypoint. Build it before native registration:
// the cross-context native branch otherwise clears its source on every rebuild.
inline Result build(std::span<std::byte> b,std::uint32_t context,
                    const state::activity::coo::ObjectiveState& objective) noexcept {
    if(b.size()<0xB10 || !source(b) || context!=4 || !objective.active || !objective.published
        || objective.event!=0xB035525AU || objective.marker!=mission::marker(objective.event)) {return {};}
    const auto index=read<std::uint32_t>(b,0x478);if(index>=3) {return {};}
    const auto row=0x190+index*0xF8;
    if(read<std::uint32_t>(b,row)!=objective.event || read<std::int8_t>(b,row+8)!=0
        || read<std::uint32_t>(b,row+0x68)!=0x4324A238U || read<std::uint32_t>(b,row+0x6C)!=0x0004002FU
        || read<std::array<std::uint32_t,4>>(b,row+0x78)!=objective.marker.locator) {return {};}
    for(unsigned i=0;i<3;++i) {
        if(i!=index && read<std::int8_t>(b,0x198+i*0xF8)==0) {return {};}
    }
    Result result{true,0};
    constexpr std::array<float,4> position{1284.5F,527.F,-28.733692169189453F,1.F};
    for(unsigned i=0;i<13;++i) {
        const auto point=0x480+i*0x80;bool changed{};
        if(i==index*4) {
            changed=read<std::uint8_t>(b,point+4)!=3 || read<std::uint8_t>(b,point+12)!=2
                || read<std::uint32_t>(b,point+24)!=4 || read<std::array<float,4>>(b,point+32)!=position
                || read<std::uint32_t>(b,point+48)!=0x4324A238U || read<std::uint32_t>(b,point+52)!=0x0004002FU;
            put<std::uint8_t>(b,point+4,3);put<std::uint8_t>(b,point+12,2);put<std::uint32_t>(b,point+24,4);
            put(b,point+32,position);put<std::uint32_t>(b,point+48,0x4324A238U);put<std::uint32_t>(b,point+52,0x0004002FU);
        } else if(read<std::uint8_t>(b,point+4)!=0) {put<std::uint8_t>(b,point+4,0);changed=true;}
        if(changed) {result.publish|=static_cast<std::uint16_t>(1U<<i);}
    }
    return result;
}
}
