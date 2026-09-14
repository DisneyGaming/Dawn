#pragma once
#include "catalog.h"
#include "reinforcement_roster.h"
#include <array>
#include <span>

namespace sunrise::state::activity::eater_of_worlds {
// All twelve authored crossing sources load at reactor activation and remain
// owned through the crossings. Native templates/placements select their actors.
// The holdout is a separate ten-actor solo selection, not the original host wave script.
inline constexpr std::uint32_t kReactorRegistry=0x686321C8U;
inline constexpr std::array<std::uint16_t,1> kPathOneEnemies{23};
inline constexpr std::array<std::uint16_t,3> kPathTwoEnemies{24,27,28};
inline constexpr std::array<std::uint16_t,3> kPathThreeEnemies{25,29,30};
inline constexpr std::array<std::uint16_t,5> kPathFourEnemies{31,32,34,35,36};
inline constexpr std::span<const std::uint16_t> path_enemies(std::uint8_t path) noexcept {
    switch(path) {
    case 1:return kPathOneEnemies;
    case 2:return kPathTwoEnemies;
    case 3:return kPathThreeEnemies;
    case 4:return kPathFourEnemies;
    default:return {};
    }
}
inline constexpr bool holdout_source(coo::Asset source) noexcept {
    return source.registry==kReactorRegistry && source.type==1
        && (source.slot==1 || source.slot==2 || (source.slot>=4 && source.slot<=21));
}
inline constexpr bool path_source(coo::Asset source) noexcept {
    if(source.registry!=kReactorRegistry || source.type!=1) return false;
    for(std::uint8_t path=1;path<=4;++path)
        for(const auto slot:path_enemies(path)) if(slot==source.slot) return true;
    return false;
}
// These are the three authored finale squads. Their left, center and right
// placement rules belong to the destination platform instead of an earlier
// fodder island. The bounded passenger roster produces ten Loyalist actors.
inline constexpr std::array<std::uint16_t,3> kSoloHoldoutSources{18,19,20};
inline constexpr bool solo_holdout_source(coo::Asset source) noexcept {
    if(source.registry!=kReactorRegistry || source.type!=1) return false;
    for(const auto slot:kSoloHoldoutSources) if(slot==source.slot) return true;
    return false;
}
// The source definitions do not carry rule references. The installed registry
// instead exposes matching named placement rules beside these source families.
// Keep the reconstructed join explicit and bounded to reactor combat.
inline constexpr std::uint16_t reactor_spawn_rule(coo::Asset source) noexcept {
    if(source.registry!=kReactorRegistry || source.type!=1) return UINT16_MAX;
    if(source.slot==4 || source.slot==5) return 291;
    if(source.slot==10 || source.slot==11) return 301;
    switch(source.slot) {
    case 18:return 311;
    case 19:return 293;
    case 20:return 309;
    case 23:return 295;
    case 24:return 297;
    case 25:return 299;
    case 27:case 28:return 321;
    case 29:case 30:return 323;
    case 31:case 32:return 325;
    case 34:return 184;
    case 35:return 186;
    case 36:return 188;
    default:return UINT16_MAX;
    }
}
// Catalog counts describe authored category cardinality. Reactor orchestration
// deliberately requests a bounded population per category: two for crossing
// combat and the mixed roster from the single-category finale sources.
inline constexpr std::uint8_t reactor_category_request(coo::Asset source) noexcept {
    if(path_source(source)) return 2;
    if(solo_holdout_source(source)) return reinforcements::count(source.slot);
    return 1;
}
inline constexpr std::uint8_t expected_enemy_count(const Spawn& spawn) noexcept {
    return static_cast<std::uint8_t>(spawn.categories*reactor_category_request(spawn.asset));
}
static_assert([] {
    std::array<bool,37> seen{};std::size_t sources{},actors{};
    for(std::uint8_t path=1;path<=4;++path) for(const auto slot:path_enemies(path)) {
        const auto index=spawn_index(kReactorRegistry,slot);
        if(slot>=seen.size() || seen[slot] || index==std::size(kSpawns)) return false;
        seen[slot]=true;++sources;actors+=expected_enemy_count(kSpawns[index]);
    }
    return sources==12 && actors==30;
}());
static_assert([] {
    std::size_t actors{};
    for(const auto slot:kSoloHoldoutSources) {
        const auto index=spawn_index(kReactorRegistry,slot);
        if(index==std::size(kSpawns)) return false;
        if(reactor_spawn_rule(kSpawns[index].asset)==UINT16_MAX) return false;
        actors+=expected_enemy_count(kSpawns[index]);
    }
    return actors==10;
}());
}
