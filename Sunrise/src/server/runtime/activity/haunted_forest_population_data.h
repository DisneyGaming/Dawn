#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "haunted_forest_round_bindings.h"
#include "population_service.h"

namespace sunrise::server::runtime::activity::haunted_forest::mode {
namespace population = sunrise::server::runtime::activity::population;
namespace codec = sunrise::middleware::bap::activity_message::native::combatant_source;

inline constexpr std::size_t kBossPopulationCount = 13;
inline constexpr std::size_t kAddPopulationCount = 96;
inline constexpr std::size_t kPopulationCount = kBossPopulationCount + kAddPopulationCount;
inline constexpr std::uint16_t kPopulationIndexOffset =
    static_cast<std::uint16_t>(kBossPopulationCount);
inline constexpr std::uint16_t kTacticalObjectiveSlot = 122;
inline constexpr std::uint16_t kBossRuleSlot = 156;
inline constexpr std::uint16_t kAddRuleSlot = 157;
inline constexpr std::uint16_t kFactionPopulationWidth = 24;
inline constexpr std::uint16_t kFactionPhaseWidth = 12;

// This registry pointer is deliberately the authored arena registry already
// defined by the round bindings; population data does not create a second one.
inline constexpr const registry::Definition* kPopulationRegistry = &kArenaRegistry[0];
inline constexpr codec::TacticalGroup kBossTactical{
    kArenaRegistry[0].key,kTacticalObjectiveSlot,0};
inline constexpr codec::TacticalGroup kAddTactical{
    kArenaRegistry[0].key,kTacticalObjectiveSlot,2};

/** Reconstructed host admission policy for the authored phase groups. */
struct HostSelectionPolicy final {
    bool requestAllIntro{};
    bool requestAllMidOnceCleared{};
};
// These are host policy assignments over authored tasks. They do not invent
// placements or tasks, and the original scheduler's selection is unproven.
inline constexpr HostSelectionPolicy kReconstructedHostSelectionPolicy{true,true};

using PopulationIndexList = std::array<std::uint16_t,kFactionPhaseWidth>;
struct FactionPopulationIndices final {
    PopulationIndexList intro{};
    PopulationIndexList mid{};
};

/** Returns the source-slot base for one authored faction block. */
[[nodiscard]] constexpr std::uint16_t factionSourceBase(Faction faction) noexcept {
    switch(faction) {
    case Faction::cabal: return 0;
    case Faction::hive: return kFactionPopulationWidth;
    case Faction::vex: return 2*kFactionPopulationWidth;
    case Faction::fallen: return 3*kFactionPopulationWidth;
    }
    return 0;
}

/** Exposes the twelve intro and twelve mid population indices for a faction. */
[[nodiscard]] constexpr FactionPopulationIndices factionPopulationIndices(
    Faction faction) noexcept {
    FactionPopulationIndices output{};
    const auto first=static_cast<std::uint16_t>(kPopulationIndexOffset+factionSourceBase(faction));
    for(std::size_t i=0;i<kFactionPhaseWidth;++i) {
        output.intro[i]=static_cast<std::uint16_t>(first+i);
        output.mid[i]=static_cast<std::uint16_t>(first+kFactionPhaseWidth+i);
    }
    return output;
}

// The six-entry intro/intro_a and mid/mid_a pairs remain in authored order;
// each exposed phase list intentionally contains both six-entry pairs.
inline constexpr auto kCabalPopulationIndices=factionPopulationIndices(Faction::cabal);
inline constexpr auto kHivePopulationIndices=factionPopulationIndices(Faction::hive);
inline constexpr auto kVexPopulationIndices=factionPopulationIndices(Faction::vex);
inline constexpr auto kFallenPopulationIndices=factionPopulationIndices(Faction::fallen);

/** Returns the faction assigned to one of the thirteen authored bosses. */
[[nodiscard]] constexpr Faction bossFaction(std::size_t bossIndex) noexcept {
    return kBosses[bossIndex].faction;
}
inline constexpr auto kBossFactions=[] {
    std::array<Faction,kBossPopulationCount> output{};
    for(std::size_t i=0;i<output.size();++i) output[i]=bossFaction(i);
    return output;
}();

inline constexpr std::array<population::Capability,kPopulationCount> kPopulationCapabilities=[] {
    std::array<population::Capability,kPopulationCount> output{};
    std::size_t cursor{};
    for(const auto& boss:kBosses) {
        output[cursor++]=population::Capability{kPopulationRegistry,boss.squad.slot,kBossRuleSlot,
            kBossTactical,true,0,1,true,boss.combatant.slot};
    }
    const auto append=[&](const auto& sources) constexpr {
        for(const auto& source:sources) {
            output[cursor++]=population::Capability{kPopulationRegistry,source.source.slot,kAddRuleSlot,
                kAddTactical,true,0,1,true,population::kNoNamedMember};
        }
    };
    append(kCabalAddSources);
    append(kHiveAddSources);
    append(kVexAddSources);
    append(kFallenAddSources);
    return output;
}();

constexpr bool validPopulationCapabilities() noexcept {
    if(kPopulationRegistry!=&kArenaRegistry[0] || kPopulationCapabilities.size()!=109) return false;
    for(std::size_t i=0;i<kBossPopulationCount;++i) {
        const auto& capability=kPopulationCapabilities[i];
        const auto& boss=kBosses[i];
        if(capability.registry!=&kArenaRegistry[0] || capability.slot!=boss.squad.slot
            || capability.rule!=kBossRuleSlot || capability.tactical.registry!=kArenaRegistry[0].key
            || capability.tactical.slot!=kTacticalObjectiveSlot || capability.tactical.row!=0
            || !capability.hasRule || !capability.allowCycles
            || capability.namedMember!=boss.combatant.slot) return false;
    }
    for(std::size_t i=kBossPopulationCount;i<kPopulationCapabilities.size();++i) {
        const auto& capability=kPopulationCapabilities[i];
        if(capability.registry!=&kArenaRegistry[0]
            || capability.slot!=i-kBossPopulationCount || capability.rule!=kAddRuleSlot
            || capability.tactical.registry!=kArenaRegistry[0].key
            || capability.tactical.slot!=kTacticalObjectiveSlot || capability.tactical.row!=2
            || !capability.hasRule || !capability.allowCycles
            || capability.namedMember!=population::kNoNamedMember) return false;
    }
    return true;
}
static_assert(kPopulationCapabilities.size()==109);
static_assert(validPopulationCapabilities());

} // namespace sunrise::server::runtime::activity::haunted_forest::mode
