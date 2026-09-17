#pragma once
#include "omega_reveal_source.h"
#include "omega_rescue_delivery.h"
#include "../../../state/activity/omega/omega_mission_devices.h"
#include "../../../state/activity/omega/omega_transit_authority.h"

namespace dawn::client::hooks::bootflow::omega_cannon_delivery {
namespace devices = state::activity::omega::mission_devices;
namespace mission = state::activity::omega::mission;
namespace transit = state::activity::omega::transit;
struct Device {
    std::uint32_t asset{}, runtimeClass{}, metadata{}, schema{};
    std::uint16_t slot{}, reference{}, stateOffset{}, bodyBytes{};
    std::uint8_t cannon{}, kind{}; // core, FX, gate
    std::int64_t definitionOffset{};
    std::uint32_t registry{devices::kRegistry};
    std::int8_t transitIndex{-1};
};
// Exact type-23 definitions from the packaged registry descriptors.
inline constexpr std::array<std::uint32_t,43> kTransitGates{
    0,0,0,0x80F476A2,0x80F476A5,0x80F476A8,0x80F4768A,0x80F4768D,0,
    0,0,0,0x80F4777B,0x80F4777E,0x80F47781,0x80F47763,0x80F47766,0,
    0x80F4788D,0x80F47890,0x80F47893,0,0,0x80F475B2,0x80F475B8,0,0,0,0,0,0,
    0,0,0x80F4767B,0,0,0x80F47721,0,0,0x80F47851,0x80F475C1,0,0};
inline constexpr std::size_t kDeviceCount=12+transit::sources.size()+19+omega_rescue_delivery::rescue::scenes.size();
inline constexpr auto kDevices = [] {
    std::array<Device,kDeviceCount> result{};
    for(std::uint8_t i=0;i<4;++i) {
        const auto& c=devices::kCannons[i];
        result[i*3]={c.coreAsset,0x80809928,0x80809927,0x8080992F,c.core,0x160,0x180,0x170,i,0,0x4C8};
        result[i*3+1]={c.fxAsset,0x80809928,0x80809927,0x8080992F,c.fx,0x160,0x180,0x170,i,1,0x4C8};
        result[i*3+2]={c.gateAsset,0x80804F46,0x80804F45,0x80804F48,c.gate,0x188,0x1C0,0x18,i,2,0x278};
    }
    std::size_t at=12;
    for(std::size_t i=0;i<transit::sources.size();++i) {
        const auto& r=transit::sources[i];
        result[at++]={r.definition,0x80809928,0x80809927,0x8080992F,r.slot,0x160,0x180,0x170,
            r.cycle,static_cast<std::uint8_t>(r.preparation>=0?0:1),0x4C8,r.registry,static_cast<std::int8_t>(i)};
        if(r.gate) result[at++]={kTransitGates[i],0x80804F46,0x80804F45,0x80804F48,r.gate,0x188,0x1C0,0x18,
            r.cycle,2,0x278,r.registry,static_cast<std::int8_t>(i)};
    }
    for(std::size_t i=0;i<omega_rescue_delivery::rescue::scenes.size();++i) {
        const auto& r=omega_rescue_delivery::rescue::scenes[i];
        result[at++]={r.definition,0x80806266,0x80806382,0x8080626B,r.slot,0x160,0x180,0xD4,
            static_cast<std::uint8_t>(i),3,0x368,omega_rescue_delivery::rescue::kRegistry};
    }
    return result;
}();
template<class T> T field(std::span<const std::byte> b,std::size_t at) noexcept {
    T v{}; if(at<=b.size() && sizeof v<=b.size()-at) std::memcpy(&v,b.data()+at,sizeof v); return v;
}
inline bool source(std::span<const std::byte> b,const Device& d) noexcept {
    return b.size()>=0x1A0 && omega_reveal_source::matches(b,{d.asset,d.runtimeClass,d.definitionOffset})
        && field<std::uint32_t>(b,d.reference+4)==d.metadata;
}
inline bool authority(const Device& d,const mission::Snapshot& s,
                      std::span<const std::byte> object,std::span<const std::byte> b) noexcept {
    if(!s.generation || s.generation>=0x7FFFFFFFU || object.size()<0x70 || b.size()!=d.bodyBytes
        || field<std::uint32_t>(object,0)!=d.registry
        || field<std::uint8_t>(object,4)!=(d.kind==3?43:d.kind==2?23:4)
        || field<std::uint16_t>(object,6)!=d.slot || field<std::uint32_t>(object,0xC)!=d.schema
        || field<std::uint32_t>(object,0x68)!=14 || field<std::uint8_t>(object,0x18)!=0
        || field<std::uint8_t>(object,0x6E)!=1) return false;
    if(d.kind==3) return omega_rescue_delivery::scene_pending(omega_rescue_delivery::rescue::scenes[d.cannon],s,b);
    const auto* route=d.transitIndex<0?nullptr:&transit::sources[static_cast<std::size_t>(d.transitIndex)];
    if(route && s.generation>0x7FFFFFFAU) return false;
    const auto status=route?transit::status(s,*route):transit::Status{};
    const bool enabled=route?status.active:(s.cannons&(1U<<d.cannon))!=0;
    const auto generation=route?transit::generation(s,*route):s.generation+(d.kind==1 && enabled?1U:0U);
    if(d.kind!=2 && route && route->role==transit::Role::sink) {
        // Native fixed storage: array at +40, first tagged record at +50,
        // payload at +60. Deliver only this cycle's exact lock override;
        // requested-use counters and player associations are never authored here.
        if(field<std::uint32_t>(b,0x40)!=1 || field<std::uint32_t>(b,0x50)!=0x80804FB8U
            || field<std::uint8_t>(b,0x60)!=(enabled?2:1)
            || field<std::uint32_t>(b,0x64)!=0x811C9DC5U || field<std::uint8_t>(b,0x68)!=0xFF
            || field<std::uint16_t>(b,0x6A)!=0xFFFF || field<std::uint32_t>(b,0x6C)!=0
            || field<std::uint8_t>(b,0x70)!=0) return false;
    }
    if(d.kind!=2) return field<std::uint32_t>(b,0)==generation
        && field<std::uint32_t>(b,4)==0 && field<std::uint8_t>(b,8)==static_cast<std::uint8_t>(enabled)
        && field<std::uint8_t>(b,9)==0 && field<std::uint32_t>(b,0xC)==0
        && field<std::uint32_t>(b,0x10)==0x811C9DC5 && field<std::uint8_t>(b,0x14)==0xFF
        && field<std::uint16_t>(b,0x16)==0xFFFF;
    for(std::size_t i=0;i<3;++i) {
        float position=enabled?1.F:0.F;
        if(route) position=transit::position(s,*route);
        const float value=i==0?position:i==1?1.F:0.F;
        const auto serial=route?transit::revision(s,*route):0U;
        const int revision=route?(i==0?(serial?static_cast<int>(serial):-1):-1):(i==0 && enabled?1:0);
        if(field<float>(b,i*8)!=value || field<std::int16_t>(b,i*8+4)!=revision
            || field<std::uint8_t>(b,i*8+6)!=0) return false;
    }
    return true;
}
inline bool adopted(std::span<const std::byte> source,const Device& d,std::span<const std::byte> body) noexcept {
    return body.size()==d.bodyBytes && source.size()>=d.stateOffset+body.size()
        && std::memcmp(source.data()+d.stateOffset,body.data(),body.size())==0;
}
struct Reference { std::uint32_t member{UINT32_MAX}; std::int64_t offset{}; };
struct Track {
    std::uint64_t run{UINT64_MAX}, nextPoll{};
    std::uint32_t generation{};
    std::size_t cursor{};
    std::array<Reference,kDeviceCount> references{};
    std::array<unsigned,kDeviceCount> reports{};
    std::array<std::uint32_t,3> eyeSourceEpoch{};
    std::array<bool,3> eyeSourceCreated{};
};
}
