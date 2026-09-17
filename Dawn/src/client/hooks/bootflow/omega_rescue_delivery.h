#pragma once
#include "omega_lair_delivery.h"
#include "../../../state/activity/omega/omega_rescue_authority.h"

namespace dawn::client::hooks::bootflow::omega_rescue_delivery {
namespace rescue=state::activity::omega::rescue;
namespace mission=state::activity::omega::mission;
using omega_lair_delivery::field;
inline const rescue::Source* source(std::span<const std::byte> b) noexcept {
    if(b.size()<0x690 || field<std::uint32_t>(b,0x5D8)!=rescue::kRegistry
        || field<std::uint8_t>(b,0x5DC)!=1) return nullptr;
    const auto* row=rescue::source(field<std::uint16_t>(b,0x5DE));
    return row && omega_reveal_source::matches(b,{row->definition,0x8080948FU,0x878})?row:nullptr;
}
// Native decoded 80807EC9 defaults, captured from the actual rescue registry.
// Only the request count and generation vary in the documented NPC writer.
inline std::array<std::uint32_t,49> expected(std::uint32_t generation,bool requested) noexcept {
    std::array<std::uint32_t,49> b{};
    for(auto at:{0x00,0x08,0x14,0x18,0x1C,0x20,0x24,0x28,0x84,0x88,0x90,0x98,0xA0,0xC0}) b[at/4]=0x811C9DC5U;
    for(auto at:{0x04,0x0C,0x8C,0x94,0x9C,0xA4}) b[at/4]=0xFFFF00FFU;
    b[0x2C/4]=1;b[0x30/4]=requested?1U:0U;b[0x7C/4]=generation;b[0xBC/4]=0x100;
    return b;
}
inline bool pending(const rescue::Source& row,const mission::Snapshot& s,
    std::span<const std::byte> object,std::span<const std::byte> body) noexcept {
    if(rescue::source(row.slot)!=&row || !s.generation || s.generation>0x7FFFFFFFU
        || !rescue::requested(s,row.slot) || object.size()<0x70 || body.size()!=0xC4
        || field<std::uint32_t>(object,0)!=rescue::kRegistry || field<std::uint8_t>(object,4)!=1
        || field<std::uint16_t>(object,6)!=row.slot || field<std::uint32_t>(object,0xC)!=0x80807EC9U
        || field<std::uint8_t>(object,0x18)!=0 || field<std::uint8_t>(object,0x6E)!=1
        || field<std::uint32_t>(object,0x68)!=14) return false;
    const auto want=expected(s.generation,true);
    return std::memcmp(body.data(),want.data(),body.size())==0;
}
inline bool scene_pending(const rescue::Scene& row,const mission::Snapshot& s,std::span<const std::byte> b) noexcept {
    const auto* command=rescue::command(s,row.slot);
    if(!command || !command->generation || command->generation>0x7FFFFFFFU || b.size()!=0xD4
        || command->eventCount>command->events.size() || row.count>8) return false;
    std::array<std::uint32_t,53> want{};
    want[0]=command->generation;want[1]=command->stop?1U:0U;want[2]=row.count;
    for(unsigned i=0;i<8;++i) {
        want[3+2*i]=i<row.count?rescue::kRegistry:0x811C9DC5U;
        want[4+2*i]=i<row.count?(static_cast<std::uint32_t>(row.sources[i])<<16)|1U:0xFFFF00FFU;
    }
    want[0x4C/4]=command->generation;want[0x50/4]=command->eventCount;
    for(unsigned i=0;i<32;++i) want[0x54/4+i]=i<command->eventCount?command->events[i]:0x811C9DC5U;
    return std::memcmp(b.data(),want.data(),b.size())==0;
}
}
