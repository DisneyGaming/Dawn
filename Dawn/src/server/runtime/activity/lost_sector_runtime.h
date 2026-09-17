#pragma once

#include "lost_sector_definition.h"
#include <array>
#include <utility>

namespace dawn::server::runtime::activity::lost_sector {

enum class Phase : std::uint8_t { dormant,active,cleared,resetReady };

struct Diagnostics final {
    Phase phase{Phase::dormant};
    std::uint16_t sector{UINT16_MAX},stage{UINT16_MAX};
    std::uint32_t runs{};
};

struct RewardTicket final {
    population::Owner owner{};
    std::uint64_t boot{};
    std::uint16_t sector{UINT16_MAX};
    std::uint32_t registry{},generation{};
    std::uint16_t slot{};
    std::uint8_t bubble{};
    [[nodiscard]] constexpr explicit operator bool() const noexcept {
        return static_cast<bool>(owner) && boot && sector!=UINT16_MAX && registry && generation;
    }
};

// A stage source is defeated when its complete admitted generation has been
// consumed and every admitted actor is dead. Dead native actors can retain a
// corpse object for an unbounded period; that object is not a combatant and
// must not hold the next room closed. The caller separately proves that the
// source lease has no queued or provisional births before advancing.
[[nodiscard]] constexpr bool source_defeated(bool nativePending,bool consumed,
    bool sourceValid,bool failed,std::size_t admitted,std::size_t dead,
    std::size_t alive) noexcept {
    return !nativePending && consumed && sourceValid && !failed && admitted
        && dead==admitted && !alive;
}

// The director deliberately has no patrol-style casualty timer. Entry requests
// the complete package-pinned sector population in one transaction; native
// spawn points and streaming decide when each room becomes resident. Only the
// singleton boss source gates completion. A completed sector rearms only after
// an observed boundary exit and re-entry; an uncleared sector never duplicates
// its residents.
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
        auto stagedService=service;auto stagedStates=states_;auto stagedGenerations=runGenerations_;
        const bool prefetchEdge=populationPrefetchBubble<64 && populationPrefetchBubble!=lastPrefetchBubble_;
        for(std::size_t i=0;i<definition_->sectors.size();++i) {
            const auto& sector=definition_->sectors[i];auto& state=stagedStates[i];
            const bool inside=arrived && bubble==sector.bubble;
            if(!inside) {
                // arrived=false is a visibility/loading pause, not proof that
                // the player crossed the sector boundary.
                const bool exited=arrived && state.phase==Phase::cleared;
                if(exited)state.phase=Phase::resetReady;
                // A qualified incoming region may prewarm the complete sector while the
                // held/current region remains authoritative. It is never an
                // arrival or a clear/reset signal. Refuse a same-tick exit so
                // a stale outgoing hint cannot immediately rearm a clear run.
                if(!exited && prefetchEdge && populationPrefetchBubble==sector.bubble
                    && (state.phase==Phase::dormant || state.phase==Phase::resetReady)) {
                    const bool renewal=state.phase==Phase::resetReady;
                    state.stage=0;
                    prepare_run_generations(stagedService,sector,renewal,stagedGenerations);
                    state.expectedGeneration=stagedGenerations[boss_stage(sector).first];
                    if(!activate_sector(stagedService,sector,renewal,stagedGenerations))return false;
                    state.phase=Phase::active;++state.runs;
                }
                continue;
            }
            if(state.phase==Phase::dormant || state.phase==Phase::resetReady) {
                const bool renewal=state.phase==Phase::resetReady;
                state.stage=0;
                prepare_run_generations(stagedService,sector,renewal,stagedGenerations);
                state.expectedGeneration=stagedGenerations[boss_stage(sector).first];
                if(!activate_sector(stagedService,sector,renewal,stagedGenerations))return false;
                state.phase=Phase::active;
                ++state.runs;
            }
            if(state.phase!=Phase::active)continue;
            if(state.runs>1 && !activate_sector(stagedService,sector,true,stagedGenerations))return false;
            const auto stage=boss_stage(sector);
            bool sourceReady=true;
            for(std::size_t source=stage.first;source<std::size_t(stage.first)+stage.count;++source)
                sourceReady&=!stagedService.renewal(definition_->capabilityBase+source).pending
                    && stagedService.generation(definition_->capabilityBase+source)==state.expectedGeneration
                    && stagedService.consumed(definition_->capabilityBase+source);
            // Old-generation quiescent receipts must not clear a freshly
            // renewed stage. commit_renewal() clears the service observation;
            // the caller then requires new-generation admission/consumption.
            if(!sourceReady)continue;
            if(!quiescent(i,state.stage,stage))continue;
            state.phase=Phase::cleared;
        }
        service=std::move(stagedService);states_=stagedStates;runGenerations_=stagedGenerations;
        lastPrefetchBubble_=populationPrefetchBubble<64?populationPrefetchBubble:UINT32_MAX;
        return true;
    }

    [[nodiscard]] Diagnostics diagnostics(std::size_t sector) const noexcept {
        if(!definition_ || sector>=definition_->sectors.size())return {};
        const auto& state=states_[sector];
        return {state.phase,static_cast<std::uint16_t>(sector),state.stage,state.runs};
    }
    [[nodiscard]] std::uint32_t expected_generation(std::size_t sectorIndex,
        std::uint16_t sourceSlot) const noexcept {
        if(!definition_ || sectorIndex>=definition_->sectors.size())return 0;
        const auto& sector=definition_->sectors[sectorIndex];
        for(std::size_t stageIndex=sector.firstStage;
            stageIndex<std::size_t(sector.firstStage)+sector.stageCount;++stageIndex) {
            const auto& stage=definition_->stages[stageIndex];
            for(std::size_t source=stage.first;source<std::size_t(stage.first)+stage.count;++source)
                if(capabilities_[source].slot==sourceSlot)return runGenerations_[source];
        }
        return 0;
    }

    // The exact package-authored chest becomes interactive only for the
    // cleared run. Its generation is the sector run, so a retained interaction
    // receipt from a prior clear cannot authorize a later reward.
    [[nodiscard]] bool append_rewards(std::uint32_t bubble,bool arrived,
        placement::wire::Batch& output) const noexcept {
        if(!definition_ || output.count>output.entries.size())return false;
        if(!arrived)return true;
        for(std::size_t i=0;i<definition_->sectors.size();++i) {
            const auto& sector=definition_->sectors[i];const auto& state=states_[i];
            if(sector.bubble!=bubble || state.phase!=Phase::cleared || !state.runs)continue;
            placement::wire::Batch projected{};
            const placement::Capability chest{sector.rewardRegistry,sector.rewardSlot,state.runs,
                placement::interaction::Mode::enabled};
            if(!placement::project(std::span(&chest,1),bubble,projected) || projected.count!=1)return false;
            const auto& request=projected.entries[0];
            for(std::size_t existing=0;existing<output.count;++existing) {
                const auto& old=output.entries[existing];
                if(old.registry==request.registry && old.slot==request.slot)
                    return old.bubble==request.bubble && old.generation==request.generation
                        && old.interactionMode==request.interactionMode;
            }
            if(output.count==output.entries.size())return false;
            output.entries[output.count++]=request;
        }
        return true;
    }

    [[nodiscard]] RewardTicket reward_ticket(std::uint32_t registryKey,
        std::uint16_t slot,std::uint32_t generation) const noexcept {
        if(!definition_ || !registryKey || !generation)return {};
        for(std::size_t i=0;i<definition_->sectors.size();++i) {
            const auto& sector=definition_->sectors[i];const auto& state=states_[i];
            if(state.phase==Phase::cleared && state.runs==generation
                && sector.rewardRegistry && sector.rewardRegistry->key==registryKey
                && sector.rewardSlot==slot)
                return {owner_,boot_,static_cast<std::uint16_t>(i),registryKey,generation,slot,sector.bubble};
        }
        return {};
    }

private:
    struct State final {
        Phase phase{Phase::dormant};
        std::uint16_t stage{};
        std::uint32_t runs{},expectedGeneration{};
    };

    [[nodiscard]] Stage boss_stage(const Sector& sector) const noexcept {
        for(std::size_t stageIndex=sector.firstStage;
            stageIndex<std::size_t(sector.firstStage)+sector.stageCount;++stageIndex) {
            const auto& stage=definition_->stages[stageIndex];
            for(std::size_t source=stage.first;source<std::size_t(stage.first)+stage.count;++source)
                if(definition_->sources[source].boss)
                    return {static_cast<std::uint16_t>(source),1};
        }
        return {};
    }

    void prepare_run_generations(const population::Service& service,const Sector& sector,
        bool renewal,std::array<std::uint32_t,population::kSourceCapacity>& generations) const noexcept {
        for(std::size_t stageIndex=sector.firstStage;
            stageIndex<std::size_t(sector.firstStage)+sector.stageCount;++stageIndex) {
            const auto& stage=definition_->stages[stageIndex];
            for(std::size_t source=stage.first;source<std::size_t(stage.first)+stage.count;++source) {
                const auto capability=definition_->capabilityBase+source;
                generations[source]=service.generation(capability)
                    +(renewal && service.consumed(capability)?1U:0U);
            }
        }
    }

    [[nodiscard]] bool activate_sector(population::Service& service,const Sector& sector,
        bool renewal,const std::array<std::uint32_t,population::kSourceCapacity>& generations) const noexcept {
        for(std::size_t stage=0;stage<sector.stageCount;++stage)
            if(!activate(service,sector,static_cast<std::uint16_t>(stage),renewal,generations))return false;
        return true;
    }

    [[nodiscard]] bool activate(population::Service& service,const Sector& sector,
        std::uint16_t relativeStage,bool renewal,
        const std::array<std::uint32_t,population::kSourceCapacity>& generations) const noexcept {
        const auto& stage=definition_->stages[sector.firstStage+relativeStage];
        for(std::size_t i=stage.first;i<std::size_t(stage.first)+stage.count;++i) {
            const auto capabilityIndex=definition_->capabilityBase+i;
            if(renewal) {
                const auto generation=service.generation(capabilityIndex);
                const auto expectedGeneration=generations[i];
                if(generation==expectedGeneration)continue;
                if(generation+1U!=expectedGeneration)return false;
                if(service.renewal(capabilityIndex).pending)continue;
            }
            if(service.last_request()==UINT64_MAX)return false;
            const auto& capability=capabilities_[i];
            const auto& policy=definition_->sources[i];
            population::Command command{owner_,service.revision(),service.last_request()+1,
                capability.registry->key,capability.slot,policy.first,boot_,policy.second};
            command.additionalRequested=policy.additional;
            command.categoryCount=policy.categoryCount?policy.categoryCount:capability.categories;
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
    std::array<std::uint32_t,population::kSourceCapacity> runGenerations_{};
    std::uint32_t lastPrefetchBubble_{UINT32_MAX};
};

} // namespace dawn::server::runtime::activity::lost_sector
