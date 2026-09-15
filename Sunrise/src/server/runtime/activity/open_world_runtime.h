#pragma once

#include "open_world_definition.h"
#include "population_service.h"
#include "placement_service.h"
#include "../../../state/activity/coo/native_population_ledger.h"
#include <array>
#include <span>
#include <utility>

namespace sunrise::server::runtime::activity::open_world {

struct Configuration final {
    std::uint32_t respawnMilliseconds{30000};
    std::uint8_t patrolRequests{1},npcRequests{1};
    bool populations{true};
};

[[nodiscard]] constexpr bool valid(const Configuration& value) noexcept {
    return value.populations && value.respawnMilliseconds>=1000
        && value.respawnMilliseconds<=3600000 && value.patrolRequests
        && value.patrolRequests<=21 && value.npcRequests==1;
}

template<class Document>
[[nodiscard]] bool configure(const Document& document,Configuration& output) noexcept {
    const auto parameter=[&](std::string_view name) noexcept {return document.views().parameter(name);};
    const auto* enabled=parameter("freeroam_population_enabled");
    const auto* respawn=parameter("freeroam_respawn_ms");
    const auto* patrol=parameter("freeroam_patrol_count");
    const auto* npc=parameter("freeroam_npc_count");
    if(!enabled || !respawn || !patrol || !npc || patrol->value>UINT8_MAX || npc->value>UINT8_MAX)return false;
    Configuration candidate{respawn->value,static_cast<std::uint8_t>(patrol->value),
        static_cast<std::uint8_t>(npc->value),enabled->value!=0};
    if(!valid(candidate))return false;
    output=candidate;return true;
}

// Starts package-authored sources only in the currently arrived bubble. Patrol
// renewal requires native consumed state plus real death and retirement receipts.
class Director final {
public:
    [[nodiscard]] bool begin(population::Owner owner,std::uint64_t boot,
        const Definition& definition,std::span<const population::Capability> capabilities,
        Configuration configuration={}) noexcept {
        if(owner_ || !owner || !boot || !valid(definition) || !valid(configuration)
            || capabilities.size()!=definition.authored->populations.size()
            || capabilities.size()>states_.size())return false;
        for(std::size_t i=0;i<capabilities.size();++i) {
            const auto& binding=definition.authored->populations[i];
            if(binding.registry>=definition.authored->registries.size()
                || capabilities[i].registry!=&definition.authored->registries[binding.registry]
                || capabilities[i].slot!=binding.source)return false;
        }
        // One loose request can admit three authored choices. Previously visited
        // zones retain their authority and adjacent zones can remain resident.
        // Bound the entire profile, not only the currently selected bubble.
        std::array<std::size_t,64> requests{};
        for(std::size_t i=0;i<capabilities.size();++i)requests[capabilities[i].registry->bubble]
            +=definition.authored->populations[i].kind==authored::PopulationKind::npc
                ?configuration.npcRequests:configuration.patrolRequests;
        std::size_t total{};for(const auto count:requests)total+=count;
        if(total*3>256 || total*9>1024)return false;
        owner_=owner;boot_=boot;definition_=&definition;capabilities_=capabilities;configuration_=configuration;
        return true;
    }

    template<class Ledger>
    [[nodiscard]] bool update(std::uint64_t now,std::uint32_t bubble,bool arrived,
        population::Service& service,std::span<const Ledger> ledgers,
        std::span<const std::uint8_t> nativePending) noexcept {
        if(!definition_ || service.owner()!=owner_ || service.boot()!=boot_
            || ledgers.size()<capabilities_.size() || nativePending.size()<capabilities_.size()
            || (lastNow_ && now<lastNow_))return false;
        lastNow_=now;if(!arrived)return true;
        if(!start_bubble(service,bubble))return false;
        return renew_patrols(now,bubble,service,ledgers,nativePending);
    }

private:
    struct State final {std::uint64_t clearAt{};bool started{},cooling{};};

    [[nodiscard]] std::uint8_t target(std::size_t capability) const noexcept {
        return definition_->authored->populations[capability].kind==authored::PopulationKind::npc
            ?configuration_.npcRequests:configuration_.patrolRequests;
    }

    [[nodiscard]] bool request(population::Service& service,std::size_t capability,
        std::uint8_t count,std::uint32_t bubble,bool renewal=false) const noexcept {
        if(capability>=capabilities_.size() || service.last_request()==UINT64_MAX)return false;
        const auto& source=capabilities_[capability];
        const population::Command command{owner_,service.revision(),service.last_request()+1,
            source.registry->key,source.slot,count,boot_};
        const auto result=renewal?service.renew(command,bubble):service.request(command,bubble);
        return result==population::Result::accepted;
    }

    [[nodiscard]] bool start_bubble(population::Service& service,std::uint32_t bubble) noexcept {
        auto staged=service;auto states=states_;
        for(std::size_t i=0;i<capabilities_.size();++i) {
            if(capabilities_[i].registry->bubble!=bubble || states[i].started)continue;
            if(staged.target(i))states[i].started=true;
            else if(!request(staged,i,target(i),bubble))return false;
            else states[i].started=true;
        }
        service=std::move(staged);states_=states;return true;
    }

    template<class Ledger>
    [[nodiscard]] bool renew_patrols(std::uint64_t now,std::uint32_t bubble,
        population::Service& service,std::span<const Ledger> ledgers,
        std::span<const std::uint8_t> nativePending) noexcept {
        auto staged=service;auto states=states_;
        for(std::size_t i=0;i<capabilities_.size();++i) {
            auto& state=states[i];
            if(!state.started || capabilities_[i].registry->bubble!=bubble
                || definition_->authored->populations[i].kind==authored::PopulationKind::npc)continue;
            if(staged.renewal(i).pending){state.cooling=false;continue;}
            const auto counts=ledgers[i].counts();
            if(counts.failed || counts.dead>counts.admitted)return false;
            const bool settled=!nativePending[i] && staged.consumed(i) && counts.admitted
                && counts.dead==counts.admitted && !counts.alive && !counts.resident;
            if(!settled){state.cooling=false;continue;}
            if(!state.cooling){state.cooling=true;state.clearAt=now;continue;}
            if(now-state.clearAt<configuration_.respawnMilliseconds)continue;
            if(!request(staged,i,target(i),bubble,true))return false;
            state.cooling=false;
        }
        service=std::move(staged);states_=states;return true;
    }

    population::Owner owner_{};std::uint64_t boot_{},lastNow_{};
    const Definition* definition_{};std::span<const population::Capability> capabilities_{};
    Configuration configuration_{};std::array<State,32> states_{};
};

[[nodiscard]] inline bool append_placements(std::span<const placement::Capability> capabilities,
    std::uint32_t bubble,placement::wire::Batch& output) noexcept {
    placement::wire::Batch additions{};
    if(output.count>output.entries.size() || !placement::project(capabilities,bubble,additions))return false;
    for(std::size_t i=0;i<additions.count;++i) {
        const auto& candidate=additions.entries[i];bool present{};
        for(std::size_t j=0;j<output.count;++j)
            if(output.entries[j].registry==candidate.registry && output.entries[j].slot==candidate.slot)present=true;
        if(present)continue;
        if(output.count==output.entries.size())return false;
        output.entries[output.count++]=candidate;
    }
    return true;
}

[[nodiscard]] inline bool append_all_placements(std::span<const placement::Capability> capabilities,
    placement::wire::Batch& output) noexcept {
    for(const auto& capability:capabilities)
        if(!capability.registry || !append_placements(capabilities,capability.registry->bubble,output))return false;
    return true;
}

} // namespace sunrise::server::runtime::activity::open_world
