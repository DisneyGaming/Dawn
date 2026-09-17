#pragma once

#include "population_service.h"
#include "placement_service.h"
#include <span>

namespace dawn::server::runtime::activity::lost_sector {

// Stages and sources are ordered slices of the package-authored Lost Sector
// capability suffix. A source is requested exactly once per sector run; native
// source composition remains responsible for the members of that squad.
struct Stage final {
    std::uint16_t first{},count{};
};

struct SourcePolicy final {
    std::uint8_t first{},second{};
    bool boss{};
    std::array<std::uint8_t,6> additional{};
    std::uint8_t categoryCount{};
};

struct Sector final {
    std::string_view name{};
    std::uint8_t bubble{};
    std::uint16_t firstStage{},stageCount{};
    const registry::Definition* rewardRegistry{};
    std::uint16_t rewardSlot{};
};

struct Definition final {
    std::span<const Sector> sectors;
    std::span<const Stage> stages;
    std::span<const SourcePolicy> sources;
    // Index of the first Lost Sector capability in the activity-wide service.
    std::uint16_t capabilityBase{};
};

[[nodiscard]] inline bool valid(const Definition& definition,
    std::span<const population::Capability> capabilities) noexcept {
    if(definition.sectors.empty() || definition.stages.empty() || capabilities.empty()
        || definition.sources.size()!=capabilities.size()
        || std::size_t(definition.capabilityBase)+capabilities.size()>population::kSourceCapacity
        || definition.stages.front().first!=0)return false;
    std::size_t priorStageEnd{};
    for(std::size_t i=0;i<definition.sectors.size();++i) {
        const auto& sector=definition.sectors[i];
        if(sector.name.empty() || sector.bubble>=64 || !sector.stageCount || !sector.rewardRegistry
            || sector.firstStage!=priorStageEnd
            || std::size_t(sector.firstStage)+sector.stageCount>definition.stages.size())return false;
        placement::wire::Batch reward{};
        const placement::Capability chest{sector.rewardRegistry,sector.rewardSlot,1,
            placement::interaction::Mode::enabled};
        if(sector.rewardRegistry->bubble!=sector.bubble
            || !placement::project(std::span(&chest,1),sector.bubble,reward)
            || reward.count!=1)return false;
        for(std::size_t prior=0;prior<i;++prior)
            if(definition.sectors[prior].bubble==sector.bubble)return false;
        std::size_t bosses{};
        for(std::size_t stageIndex=sector.firstStage;
            stageIndex<std::size_t(sector.firstStage)+sector.stageCount;++stageIndex) {
            const auto& stage=definition.stages[stageIndex];
            if(!stage.count || std::size_t(stage.first)+stage.count>capabilities.size())return false;
            if(stageIndex && stage.first!=std::size_t(definition.stages[stageIndex-1].first)
                    +definition.stages[stageIndex-1].count)return false;
            for(std::size_t source=stage.first;source<std::size_t(stage.first)+stage.count;++source) {
                const auto& policy=definition.sources[source];
                bosses+=policy.boss?1U:0U;
                const auto categories=policy.categoryCount?policy.categoryCount:
                    static_cast<std::uint8_t>(policy.second?2U:1U);
                unsigned requested=policy.first+policy.second;
                for(const auto value:policy.additional)requested+=value;
                if(!population::valid(capabilities[source]) || !policy.first
                    || categories!=capabilities[source].categories || requested>63
                    || (policy.boss && (policy.first!=1 || requested!=1))
                    || capabilities[source].registry->bubble!=sector.bubble)return false;
                for(std::size_t category=categories;category<8;++category)
                    if(category==1?policy.second:category>=2?policy.additional[category-2]:0)return false;
            }
        }
        if(bosses!=1)return false;
        priorStageEnd=std::size_t(sector.firstStage)+sector.stageCount;
    }
    const auto& finalStage=definition.stages.back();
    return priorStageEnd==definition.stages.size()
        && std::size_t(finalStage.first)+finalStage.count==capabilities.size();
}

} // namespace dawn::server::runtime::activity::lost_sector
