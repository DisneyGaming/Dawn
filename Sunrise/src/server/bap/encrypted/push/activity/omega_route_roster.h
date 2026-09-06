#pragma once
#include "../../../../../state/build_data/scenarios/omega_crown_roster.h"
#include "../../../../../state/activity/omega/omega_progression.h"
#include "../../../../../middleware/bap/activity_message/sensor_auth_update.h"

namespace sunrise::server::bap::encrypted::push::activity::omega_route_roster {
namespace message=middleware::bap::activity_message::sensor_auth_update;
namespace catalog=state::build_data::scenarios::omega_crown_roster;
inline constexpr std::array<std::uint32_t,7> base{
    0x4786C0E0U,0x29D7B029U,0x82FB58B7U,0xBA5F26EFU,
    0xD00142CFU,0xF7A6CE7FU,0x2763EC97U};
inline constexpr std::array<std::uint32_t,9> extra{
    state::activity::omega::kHandoffGroup,state::activity::omega::kCrownGroup,
    state::activity::omega::kBossGroup,state::activity::omega::kRevealGroup,
    0x0040BF06U,0x0040BF05U,0x0040BF04U,0x0040BF03U,0x99BD2FEBU};
static_assert(base.size()+extra.size()<=message::kGroupCapacity);
template<class Scratch,class Load>
bool append(Scratch& scratch,message::Roster& roster,Load load) noexcept {
    if(roster.groupCount!=base.size() || roster.topLevelGroupCount!=3) return false;
    for(std::size_t i=0;i<base.size();++i) if(roster.groups[i].key!=base[i]) return false;
    for(std::size_t i=0;i<extra.size();++i) {
        const auto slot=base.size()+i;
        auto& group=scratch.rosterGroups[slot];
        if(!load(extra[i],group) || group.registryKey!=extra[i]
            || !group.slotCount || group.slotCount>group.slotTypes.size()
            || (catalog::find(extra[i]) && !catalog::valid(group,extra[i]))) return false;
        // Stable group registration creates native sources. Their authorities
        // remain dormant until run-owned commands release each wave or scene.
        for(std::size_t object=0;object<group.slotCount;++object)
            group.slotFlags[object]|=message::kSlotAuthFlag;
        roster.groups[slot]={group.registryKey,
            std::span<const std::uint8_t>(group.slotTypes).first(group.slotCount),
            std::span<const std::uint8_t>(group.slotFlags).first(group.slotCount),
            std::span<const std::uint16_t>(group.slotIndices).first(group.slotCount)};
    }
    roster.groupCount=base.size()+extra.size();
    scratch.rosterSubBlockKeys[0][0]=base[6];
    scratch.rosterSubBlockKeys[0][1]=extra[0];
    scratch.rosterSubBlocks[0]={11,std::span(scratch.rosterSubBlockKeys[0]).first(2)};
    for(std::size_t i=1;i<extra.size();++i) scratch.rosterSubBlockKeys[1][i-1]=extra[i];
    scratch.rosterSubBlocks[1]={14,std::span(scratch.rosterSubBlockKeys[1]).first(extra.size()-1)};
    for(std::size_t i=0;i<3;++i) scratch.rosterSubBlockKeys[2][i]=base[3+i];
    scratch.rosterSubBlocks[2]={15,std::span(scratch.rosterSubBlockKeys[2]).first(3)};
    roster.bubbleSubBlocks=std::span(scratch.rosterSubBlocks).first(3);
    return true;
}
}
