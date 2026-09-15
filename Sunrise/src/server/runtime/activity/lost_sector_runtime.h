#pragma once

#include "lost_sector_definition.h"
#include <array>
#include <utility>

namespace sunrise::server::runtime::activity::lost_sector {

enum class Phase : std::uint8_t { dormant,active,cleared,resetReady };

struct Diagnostics final {
    Phase phase{Phase::dormant};
    std::uint16_t sector{UINT16_MAX},stage{UINT16_MAX};
    std::uint32_t runs{};
};

// The director deliberately has no patrol-style casualty timer. It advances a
// package-pinned stage only after the caller proves every source lease in that
// stage is quiescent. A completed sector rearms only after an observed boundary
// exit and re-entry; leaving an uncleared sector never duplicates its residents.
class Director final {
public:
    [[nodiscard]] bool begin(population::Owner owner,std::uint64_t boot,
        const Definition& definition,std::span<const population::Capability> capabilities) noexcept {
        if(owner_ || !owner || !boot || !valid(definition,capabilities)
            || definition.sectors.size()>states_.size())return false;
        owner_=owner;boot_=boot;definition_=&definition;capabilities_=capabilities;return true;
    }

    template<class Quiescent>
    [[nodiscard]] bool update(std::uint32_t bubble,bool arrived,population::Service& service,
        Quiescent&& quiescent,std::uint32_t populationPrefetchBubble=UINT32_MAX) noexcept {
        if(!definition_ || service.owner()!=owner_ || service.boot()!=boot_)return false;
        auto stagedService=service;auto stagedStates=states_;
        const bool prefetchEdge=populationPrefetchBubble<64 && populationPrefetchBubble!=lastPrefetchBubble_;
        for(std::size_t i=0;i<definition_->sectors.size();++i) {
            const auto& sector=definition_->sectors[i];auto& state=stagedStates[i];
            const bool inside=arrived && bubble==sector.bubble;
            if(!inside) {
                // arrived=false is a visibility/loading pause, not proof that
                // the player crossed the sector boundary.
                const bool exited=arrived && state.phase==Phase::cleared;
                if(exited)state.phase=Phase::resetReady;
                // A qualified incoming region may prewarm stage zero while the
                // held/current region remains authoritative. It is never an
                // arrival or a clear/reset signal. Refuse a same-tick exit so
                // a stale outgoing hint cannot immediately rearm a clear run.
                if(!exited && prefetchEdge && populationPrefetchBubble==sector.bubble
                    && (state.phase==Phase::dormant || state.phase==Phase::resetReady)) {
                    const bool renewal=state.phase==Phase::resetReady;
                    state.stage=0;
                    if(!activate(stagedService,sector,state.stage,renewal))return false;
                    state.phase=Phase::active;++state.runs;
                }
                continue;
            }
            if(state.phase==Phase::dormant || state.phase==Phase::resetReady) {
                const bool renewal=state.phase==Phase::resetReady;
                state.stage=0;
                if(!activate(stagedService,sector,state.stage,renewal))return false;
                state.phase=Phase::active;
                ++state.runs;
            }
            if(state.phase!=Phase::active)continue;
            const auto& stage=definition_->stages[sector.firstStage+state.stage];
            bool sourceReady=true;
            for(std::size_t source=stage.first;source<std::size_t(stage.first)+stage.count;++source)
                sourceReady&=!stagedService.renewal(definition_->capabilityBase+source).pending
                    && stagedService.consumed(definition_->capabilityBase+source);
            // Old-generation quiescent receipts must not clear a freshly
            // renewed stage. commit_renewal() clears the service observation;
            // the caller then requires new-generation admission/consumption.
            if(!sourceReady)continue;
            if(!quiescent(i,state.stage,stage))continue;
            if(std::size_t(state.stage)+1<sector.stageCount) {
                ++state.stage;
                if(!activate(stagedService,sector,state.stage,state.runs>1))return false;
            } else state.phase=Phase::cleared;
        }
        service=std::move(stagedService);states_=stagedStates;
        lastPrefetchBubble_=populationPrefetchBubble<64?populationPrefetchBubble:UINT32_MAX;
        return true;
    }

    [[nodiscard]] Diagnostics diagnostics(std::size_t sector) const noexcept {
        if(!definition_ || sector>=definition_->sectors.size())return {};
        const auto& state=states_[sector];
        return {state.phase,static_cast<std::uint16_t>(sector),state.stage,state.runs};
    }

private:
    struct State final {Phase phase{Phase::dormant};std::uint16_t stage{};std::uint32_t runs{};};

    [[nodiscard]] bool activate(population::Service& service,const Sector& sector,
        std::uint16_t relativeStage,bool renewal) const noexcept {
        const auto& stage=definition_->stages[sector.firstStage+relativeStage];
        for(std::size_t i=stage.first;i<std::size_t(stage.first)+stage.count;++i) {
            if(service.last_request()==UINT64_MAX)return false;
            const auto& capability=capabilities_[i];
            const auto& policy=definition_->sources[i];
            const population::Command command{owner_,service.revision(),service.last_request()+1,
                capability.registry->key,capability.slot,policy.first,boot_,policy.second};
            const auto result=renewal?service.renew(command,sector.bubble)
                :service.request(command,sector.bubble);
            if(result!=population::Result::accepted)return false;
        }
        return true;
    }

    population::Owner owner_{};std::uint64_t boot_{};
    const Definition* definition_{};
    std::span<const population::Capability> capabilities_{};
    std::array<State,16> states_{};
    std::uint32_t lastPrefetchBubble_{UINT32_MAX};
};

} // namespace sunrise::server::runtime::activity::lost_sector
