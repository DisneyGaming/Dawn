#pragma once
#include "festival_quest.h"
#include "festival_mask.h"
#include <array>
#include <utility>

namespace sunrise::state::account::festival_projection {
// Share the native twenty-row override bank with vendor and New Light state.
template<class Object> bool project(const CharacterState& character, bool active, Object& object) noexcept {
    const auto eva=festival_quest::available(character,active);
    const std::array<std::pair<std::int16_t,bool>,4> flags{{
        {std::int16_t{20826},eva.intro},{std::int16_t{20829},eva.wearingMasks},{std::int16_t{20831},eva.finalStage},
        {festival_mask::kEquippedRequirementFlag,festival_mask::equipped(character)}}};
    for(const auto& [slot,enabled]:flags) {
        std::size_t i{};
        for(;i<object.unlockFlagCount;++i) if(object.unlockFlags[i].slot==slot) break;
        if(i>=object.unlockFlags.size()) return false;
        if(i==object.unlockFlagCount) ++object.unlockFlagCount;
        object.unlockFlags[i]={slot,static_cast<std::uint8_t>(enabled?2:0),0};
    }
    return true;
}
}
