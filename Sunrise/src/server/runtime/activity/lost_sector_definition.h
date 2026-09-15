#pragma once

#include "population_service.h"
#include <span>

namespace sunrise::server::runtime::activity::lost_sector {

// Stages and sources are ordered slices of the package-authored Lost Sector
// capability suffix. A source is requested exactly once per sector run; native
// source composition remains responsible for the members of that squad.
struct Stage final {
    std::uint16_t first{},count{};
};

struct SourcePolicy final {
    std::uint8_t first{},second{};
    bool boss{};
};

struct Sector final {
    std::string_view name{};
    std::uint8_t bubble{};
    std::uint16_t firstStage{},stageCount{};
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
        if(sector.name.empty() || sector.bubble>=64 || !sector.stageCount
            || sector.firstStage!=priorStageEnd
            || std::size_t(sector.firstStage)+sector.stageCount>definition.stages.size())return false;
        for(std::size_t prior=0;prior<i;++prior)
            if(definition.sectors[prior].bubble==sector.bubble)return false;
        for(std::size_t stageIndex=sector.firstStage;
            stageIndex<std::size_t(sector.firstStage)+sector.stageCount;++stageIndex) {
            const auto& stage=definition.stages[stageIndex];
            if(!stage.count || std::size_t(stage.first)+stage.count>capabilities.size())return false;
            if(stageIndex && stage.first!=std::size_t(definition.stages[stageIndex-1].first)
                    +definition.stages[stageIndex-1].count)return false;
            for(std::size_t source=stage.first;source<std::size_t(stage.first)+stage.count;++source) {
                const auto& policy=definition.sources[source];
                if(!population::valid(capabilities[source]) || !policy.first
                    || unsigned(policy.first)+policy.second>63
                    || (policy.second && capabilities[source].categories!=2)
                    || (policy.boss && (policy.first!=1 || policy.second))
                    || capabilities[source].registry->bubble!=sector.bubble)return false;
            }
        }
        priorStageEnd=std::size_t(sector.firstStage)+sector.stageCount;
    }
    const auto& finalStage=definition.stages.back();
    return priorStageEnd==definition.stages.size()
        && std::size_t(finalStage.first)+finalStage.count==capabilities.size();
}

} // namespace sunrise::server::runtime::activity::lost_sector
