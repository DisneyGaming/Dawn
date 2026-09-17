#pragma once

#include "edz_moon_lost_sector_catalog.h"
#include "moon_lost_sector_types.h"


namespace sunrise::server::runtime::activity::moon_lost_sector {
namespace moon=lost_sector::catalog::moon;
[[nodiscard]] constexpr bool current_gate_death(bool renewalPending,std::uint32_t current,
    std::uint32_t expected,bool defeated) noexcept {
    return !renewalPending && expected && current==expected && defeated;
}

// K1 boss protection is independent from the all-at-entry population policy.
// The registered Nightmare/guardian deaths only toggle the exact authored
// shield. Revelation additionally requires four package-authored crystals to
// be destroyed through a live-instance health receipt.
class Director final {
public:
    [[nodiscard]] bool begin(population::Owner owner,std::uint64_t boot,
        std::uint16_t capabilityBase) noexcept {
        if(owner_ || !owner || !boot || capabilityBase>population::kSourceCapacity-moon::kCapabilities.size())return false;
        owner_=owner;boot_=boot;base_=capabilityBase;return true;
    }
    template<class Defeated>
    [[nodiscard]] bool update(std::uint32_t bubble,bool arrived,const lost_sector::Director& sectors,
        const population::Service& service,placement::wire::Batch& placements,
        ShieldBatch& shields,
        middleware::bap::activity_message::native::world_device::Batch& devices,
        Defeated&& defeated) noexcept {
        if(!owner_ || service.owner()!=owner_ || service.boot()!=boot_)return false;
        for(std::size_t sector=0;sector<moon::kSectors.size();++sector) {
            const auto diagnostics=sectors.diagnostics(sector);
            auto& state=states_[sector];
            if(diagnostics.runs && diagnostics.runs!=state.run) {
                // Both placed objects and native world devices use this run's
                // two positive revisions. Keep the value inside int16_t too.
                if(diagnostics.runs>0x3FFFU)return false;
                state={};state.run=diagnostics.runs;state.generation=diagnostics.runs*2U-1U;
            }
            if(!state.run || (diagnostics.phase!=lost_sector::Phase::active
                && diagnostics.phase!=lost_sector::Phase::cleared))continue;
            if(sector==1) {
                constexpr std::array<std::uint16_t,3> guardians{{34,41,48}};
                for(std::size_t i=0;i<guardians.size();++i)
                    if(source_dead(sectors,service,sector,guardians[i],defeated))state.crystalActive[i]=true;
                if(state.crystals[0].dead() && state.crystals[1].dead() && state.crystals[2].dead())
                    state.crystalActive[3]=true;
                state.exposed=state.crystals[3].dead();
            } else {
                const auto nightmare=sector==0?59U:sector==2?47U:62U;
                state.exposed=source_dead(sectors,service,sector,static_cast<std::uint16_t>(nightmare),defeated);
                if(sector==0)state.exposed&=source_dead(sectors,service,sector,64,defeated);
            }
            if(arrived && bubble==moon::kSectors[sector].bubble) {
                if(!append_traversal(sector,state,placements,devices))return false;
                if(!append_shield(sector,!state.exposed,shields))return false;
                if(sector==1 && !append_crystals(state,placements))return false;
            }
        }
        return true;
    }
    [[nodiscard]] DestructibleRequest object_request(std::uint32_t definition) const noexcept {
        const auto& state=states_[1];if(!owner_ || !state.run)return {};
        for(std::size_t i=0;i<kCrystals.size();++i)if(kCrystals[i].definition==definition
            && state.crystalActive[i] && !state.crystals[i].dead())
            return {owner_,boot_,{owner_.sessionId,state.generation},kCrystals[i],state.crystals[i].owner(),
                true,true,state.crystals[i].dead()};
        return {};
    }
    [[nodiscard]] bool observe_object(const ObjectReceipt& receipt,bool dead,
        bool replaced=false) noexcept {
        auto& state=states_[1];
        if(!receipt.valid() || receipt.activity!=owner_ || receipt.boot!=boot_
            || receipt.owner!=state::activity::coo::Generation{owner_.sessionId,state.generation})return false;
        for(std::size_t i=0;i<kCrystals.size();++i)if(receipt.source==kCrystals[i]) {
            if(!state.crystalActive[i] || state.crystals[i].dead())return false;
            auto& service=state.crystals[i];
            if(!service.owner().valid()) {
                if(dead || !service.bind(receipt))return false;
                service.expose();return true;
            }
            if(!dead && replaced && service.owner()!=receipt) {
                service={};if(!service.bind(receipt))return false;
                service.expose();return true;
            }
            return dead && service.destroyed(receipt);
        }
        return false;
    }
private:
    struct State final {
        std::uint32_t run{},generation{};bool exposed{};
        std::array<bool,4> crystalActive{};
        std::array<state::activity::coo::DestructibleService<ObjectReceipt>,4> crystals{};
    };
    inline static constexpr std::array<state::activity::coo::Asset,4> kCrystals{{
        {0xADE66EB0U,0x815700D5U,4,69},{0xADE66EB0U,0x815700D8U,4,70},
        {0xADE66EB0U,0x815700DBU,4,71},{0xADE66EB0U,0x815700DEU,4,72},
    }};
    struct TraversalDevice final {std::uint8_t sector{};std::uint16_t slot{};std::uint32_t definition{};};
    struct TraversalBlocker final {std::uint8_t sector{};std::uint16_t slot{};std::uint32_t definition{};};
    // Package-pinned route/loopback devices. Position=1 is a reconstructed safe
    // open state; it is published once per run and never closed while the player
    // can still be inside the sector.
    inline static constexpr std::array<TraversalDevice,11> kTraversalDevices{{
        {0,47,0x81567997U},{0,49,0x8156799DU},{0,51,0x815679A3U},
        {0,55,0x815679AFU},{0,57,0x815679B5U},{1,16,0x81570036U},
        {2,0,0x8157125AU},{2,1,0x8157125DU},{2,2,0x81571260U},
        {3,88,0x81572B9DU},{3,89,0x81572BA0U},
    }};
    // Paired physical blockers are withdrawn at the run's second generation.
    inline static constexpr std::array<TraversalBlocker,11> kTraversalBlockers{{
        {0,48,0x8156799AU},{0,50,0x815679A0U},{0,52,0x815679A6U},
        {0,56,0x815679B2U},{0,58,0x815679B8U},{2,3,0x81571263U},
        {2,4,0x81571266U},{3,9,0x81572AB0U},{3,10,0x81572AB3U},
        {3,34,0x81572AFBU},{3,35,0x81572AFEU},
    }};
    [[nodiscard]] std::size_t source_index(std::size_t sector,std::uint16_t slot) const noexcept {
        const auto stage=moon::kStages[sector];
        for(std::size_t local=stage.first;local<std::size_t(stage.first)+stage.count;++local)
            if(moon::kCapabilities[local].slot==slot)return base_+local;
        return population::kSourceCapacity;
    }
    template<class Defeated> [[nodiscard]] bool source_dead(const lost_sector::Director& sectors,
        const population::Service& service,
        std::size_t sector,std::uint16_t slot,Defeated&& defeated) const noexcept {
        const auto index=source_index(sector,slot);
        const auto expected=sectors.expected_generation(sector,slot);
        return index<population::kSourceCapacity && current_gate_death(service.renewal(index).pending,
            service.generation(index),expected,defeated(index,expected));
    }
    [[nodiscard]] static bool append_shield(std::size_t sector,bool enabled,
        ShieldBatch& out) noexcept {
        if(out.count==out.entries.size())return false;
        const auto& registry=moon::kRegistries[sector];
        const std::array<std::uint16_t,4> effects{{46,76,46,98}};
        const std::array<std::uint16_t,4> filters{{75,95,55,113}};
        const std::array<std::uint16_t,4> bosses{{42,64,43,60}};
        out.entries[out.count++]={&registry,effects[sector],filters[sector],bosses[sector],enabled};
        return true;
    }
    [[nodiscard]] static bool append_crystals(const State& state,placement::wire::Batch& out) noexcept {
        const auto& registry=moon::kRegistries[1];
        for(std::size_t i=0;i<kCrystals.size();++i) {
            if(!state.crystalActive[i])continue;
            if(out.count==out.entries.size())return false;
            auto& request=out.entries[out.count++];request.registry=registry.key;
            request.slot=kCrystals[i].slot;request.bubble=registry.bubble;
            request.active=!state.crystals[i].dead();
            request.generation=state.generation+(request.active?0U:1U);
        }
        return true;
    }
    [[nodiscard]] static bool append_traversal(std::size_t sector,const State& state,
        placement::wire::Batch& placements,
        middleware::bap::activity_message::native::world_device::Batch& devices) noexcept {
        const auto& registry=moon::kRegistries[sector];
        for(const auto& device:kTraversalDevices)if(device.sector==sector) {
            unsigned matches{};for(const auto& slot:registry.slots)
                matches+=slot.index==device.slot && slot.type==23 && slot.descriptorTag==device.definition;
            if(matches!=1 || devices.count==devices.entries.size())return false;
            auto& request=devices.entries[devices.count++];request.registry=registry.key;
            request.slot=device.slot;request.bubble=registry.bubble;
            request.state.position={1.F,static_cast<std::int16_t>(state.generation),true};
        }
        for(const auto& blocker:kTraversalBlockers)if(blocker.sector==sector) {
            unsigned matches{};for(const auto& slot:registry.slots)
                matches+=slot.index==blocker.slot && slot.type==4 && slot.descriptorTag==blocker.definition;
            if(matches!=1 || placements.count==placements.entries.size())return false;
            for(std::size_t i=0;i<placements.count;++i)
                if(placements.entries[i].registry==registry.key
                    && placements.entries[i].slot==blocker.slot)return false;
            auto& request=placements.entries[placements.count++];request.registry=registry.key;
            request.slot=blocker.slot;request.bubble=registry.bubble;
            request.active=false;request.generation=state.generation+1U;
        }
        return true;
    }
    population::Owner owner_{};std::uint64_t boot_{};std::uint16_t base_{};
    std::array<State,4> states_{};
};
} // namespace sunrise::server::runtime::activity::moon_lost_sector
