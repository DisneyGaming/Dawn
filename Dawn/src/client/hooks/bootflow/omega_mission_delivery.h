#pragma once
#include "omega_lair_delivery.h"
#include "../../../state/activity/omega/omega_mission_authority.h"

namespace dawn::client::hooks::bootflow::omega_mission_delivery {
namespace mission=state::activity::omega::mission;
using omega_lair_delivery::field;
inline const mission::Source* source(std::span<const std::byte> component) noexcept {
    if(component.size()<omega_lair_delivery::kComponentBytes) return nullptr;
    const auto index=mission::source_index(field<std::uint32_t>(component,0x5D8),field<std::uint16_t>(component,0x5DE));
    if(index==mission::kSources.size() || field<std::uint8_t>(component,0x5DC)!=1) return nullptr;
    const auto& row=mission::kSources[index];
    return omega_reveal_source::matches(component,{row.definition,0x8080948FU,0x728})?&row:nullptr;
}
inline bool pending(const mission::Source& source,const mission::Snapshot& snapshot,
                    std::span<const std::byte> object,std::span<const std::byte> body) noexcept {
    const auto index=mission::source_index(source.registry,source.slot);
    if(index==mission::kSources.size() || &source!=&mission::kSources[index]
        || object.size()<0x70 || body.size()!=0xC4 || !snapshot.generation
        || snapshot.generation>0x7FFFFFFFU) return false;
    const auto requested=snapshot.requested[index];
    if(requested[0]+requested[1]==0) return false;
    if(field<std::uint32_t>(object,0)!=source.registry || field<std::uint8_t>(object,4)!=1
        || field<std::uint16_t>(object,6)!=source.slot || field<std::uint32_t>(object,0xC)!=0x80807EC9U
        || field<std::uint8_t>(object,0x18)!=0
        || field<std::uint32_t>(body,0)!=source.registry || field<std::uint8_t>(body,4)!=3
        || field<std::uint16_t>(body,6)!=source.tactical
        || field<std::uint32_t>(body,0x2C)!=source.categories
        || field<std::uint32_t>(body,0x7C)!=snapshot.generation
        || field<std::uint32_t>(body,0xA0)!=source.registry || field<std::uint8_t>(body,0xA4)!=66
        || field<std::uint16_t>(body,0xA6)!=source.rule || field<std::uint32_t>(body,0xB4)!=source.row
        || field<std::uint8_t>(body,0xBC)!=0 || field<std::uint8_t>(body,0xBD)!=0) return false;
    for(unsigned c=0;c<source.categories;++c)
        if(requested[c]>source.requested[c]
            || field<std::uint32_t>(body,0x30+4*c)!=(source.member?0U:requested[c])) return false;
    return true;
}
} // namespace dawn::client::hooks::bootflow::omega_mission_delivery
