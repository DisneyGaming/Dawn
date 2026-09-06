#pragma once
#include "../../../state/activity/omega/omega_mission_state.h"
#include <cmath>
#include <span>
#include <cstring>

namespace sunrise::client::hooks::bootflow::omega_mission_motion {
namespace mission = state::activity::omega::mission;
using Point = std::array<float,4>;
inline constexpr std::array<Point,5> destinations{{
    {-1425.054931640625F,169.7905731201172F,-43.593299865722656F,1.F},
    {-1596.3707275390625F,120.1434555053711F,-62.594085693359375F,1.F},
    {-1486.6104736328125F,11.791311264038086F,-73.3829116821289F,1.F},
    {-1489.560791015625F,-165.89990234375F,-62.49308395385742F,1.F},
    {-1490.12109375F,-491.90216064453125F,-32.84910202026367F,1.F}}};
inline bool finite(const Point& p) noexcept {
    return std::all_of(p.begin(),p.end(),[](float v){return std::isfinite(v);});
}
inline bool position_matches(const Point& a,const Point& b,float tolerance) noexcept {
    if(!finite(a)||!finite(b)) return false;
    float distance{};
    for(unsigned i=0;i<3;++i) distance+=(a[i]-b[i])*(a[i]-b[i]);
    return distance<=tolerance*tolerance;
}
template<class T> inline T field(std::span<const std::byte> b,std::size_t offset) noexcept {
    T result{};
    if(offset<=b.size() && sizeof(T)<=b.size()-offset) std::memcpy(&result,b.data()+offset,sizeof(T));
    return result;
}
struct Allocation { std::uint16_t slot{},offset{},size{}; std::uint8_t opcode{},flags{}; };
struct Arena {
    std::uint32_t self{UINT32_MAX},entity{UINT32_MAX},movement{UINT32_MAX};
    std::array<Allocation,3> slots{};
    unsigned count{};
    std::size_t data{};
    bool pending{},enabled{};
};
inline bool arena(std::span<const std::byte> b,Arena& out) noexcept {
    out={};
    if(b.size()<0x2B0 || field<std::uint32_t>(b,0)!=0x815B5A43U
        || field<std::uint32_t>(b,4)!=0x808069EEU || field<std::int64_t>(b,8)!=0x1010
        || field<std::uint64_t>(b,0x50)!=3 || field<std::uint64_t>(b,0x60)!=3
        || field<std::uint64_t>(b,0x70)!=0x3F0) return false;
    const auto relative=[&](std::size_t at,std::size_t size,std::size_t& result) {
        const auto displacement=field<std::int64_t>(b,at);
        if(displacement<0 || static_cast<std::uint64_t>(displacement)>b.size()) return false;
        result=at+static_cast<std::size_t>(displacement)+16;
        return result<=b.size() && size<=b.size()-result;
    };
    std::size_t slots{},blocks{};
    if(!relative(0x58,12,slots)||!relative(0x68,18,blocks)||!relative(0x78,0x3F0,out.data)) return false;
    out.self=field<std::uint32_t>(b,0x24); out.entity=field<std::uint32_t>(b,0x2C);
    out.movement=field<std::uint32_t>(b,0x17C);
    if(out.self==UINT32_MAX || out.entity==UINT32_MAX || out.movement==UINT32_MAX
        || field<std::uint32_t>(b,0x170)!=out.entity) return false;
    for(std::uint16_t i=0;i<3;++i) {
        const auto row=slots+i*4;
        const auto offset=field<std::uint16_t>(b,row);
        const auto opcode=field<std::uint8_t>(b,row+2);
        if(opcode==0xFF) { if(offset!=UINT16_MAX) return false; continue; }
        unsigned matches{};std::uint16_t length{};
        for(unsigned block=0;block<3;++block) {
            const auto record=blocks+block*6;
            if(field<std::uint16_t>(b,record+4)==i && field<std::uint16_t>(b,record)==offset) {
                length=field<std::uint16_t>(b,record+2); ++matches;
            }
        }
        if(matches!=1 || !length || offset>0x3F0 || length>0x3F0-offset) return false;
        out.slots[out.count++]={i,offset,length,opcode,field<std::uint8_t>(b,row+3)};
    }
    if(field<std::uint32_t>(b,0x80)!=out.count) return false;
    const auto first=field<std::uint16_t>(b,0xE0),second=field<std::uint16_t>(b,0x140);
    if(first>4 || second>4) return false;
    out.pending=first || second;
    out.enabled=field<std::uint8_t>(b,0x2A2)==1 && field<std::uint8_t>(b,0x2A0)==0
        && field<std::uint32_t>(b,0x1E4)==0x80F83641U && field<std::uint32_t>(b,0x1E8)==0x80F83881U;
    return true;
}
inline bool departure_ready(std::span<const std::byte> bytes,const Arena& a) noexcept {
    if(!a.enabled || a.pending || a.count>1) return false;
    if(!a.count) return true;
    const auto& slot=a.slots[0];
    return slot.opcode==0x6C && slot.flags==0 && slot.size==0x330
        && field<std::uint32_t>(bytes,a.data+slot.offset+0x304)==a.entity;
}
struct Identity {
    mission::Token token{};
    std::uint32_t component{UINT32_MAX},movement{UINT32_MAX},selector{UINT32_MAX};
    std::uint16_t slot{UINT16_MAX};
    bool operator==(const Identity&) const = default;
};
// Physical arena offsets can change between callbacks. Identity retains the
// full component handle and logical slot, together with the command epoch.
struct Track {
    mission::Token token{};
    Identity identity{};
    Point placed{};
    std::uint32_t physics{UINT32_MAX};
    std::uint8_t stage{};
    std::uint8_t completionWaitMask{255};
    bool stopClaimed{},requestClaimed{},submitted{},cleaned{},completed{},uncertain{};
    bool observe(const Identity& owner,std::uint8_t next,const Point& target,std::uint32_t body) noexcept {
        if(!submitted || cleaned || uncertain || owner.token!=token || !finite(target)
            || owner.component==UINT32_MAX || owner.movement==UINT32_MAX || owner.selector==UINT32_MAX
            || owner.slot==UINT16_MAX || body==UINT32_MAX || next<1 || next>3) return false;
        if(stage==0) {
            if(next!=1) return false;
            identity=owner; placed=target; physics=body;
        } else if(identity!=owner || physics!=body || !position_matches(placed,target,0.001F)) return false;
        if(next!=stage && next!=stage+1) return false;
        stage=next; return true;
    }
    bool cleanup(const Identity& owner) noexcept {
        if(!submitted || uncertain || stage!=3 || cleaned || identity!=owner) return false;
        cleaned=true; return true;
    }
    bool finish(const mission::Token& current,bool retired,bool selectorIdle,const Point& actual) noexcept {
        if(current!=token || completed || uncertain || !cleaned || !retired || !selectorIdle
            || !position_matches(placed,actual,0.1F)) return false;
        completed=true; return true;
    }
};
}
