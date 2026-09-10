// One flat source table for the whole strike. Dawn's shared population ledger is indexed by a
// single catalog, so the five section catalogs are concatenated here and their cohort numbers are
// rebased into one global sequence. Nothing is redefined: each row is the section's own row with
// only its cohort shifted, so a section catalog stays the single place an identity is written.
#pragma once
#include "catalog.h"
#include "catalog_forest.h"
#include "catalog_chase.h"
#include "catalog_ledge.h"
#include "catalog_boss.h"
namespace sunrise::state::activity::strike_pact {

// sq_prefight_skirmish[0..5] (sources 40..45) are in both section catalogs, because the proven
// build placed them from populations.lua and gated the reveal on them from boss.lua. One flat
// ledger holds one row per source, and the reveal is the join that needs them, so the boss keeps
// them and the ledge copy is dropped here rather than edited out of either catalog.
inline constexpr std::uint8_t kLedgePrefightCohort=kCohortPrefightSkirmish;

// Each section keeps its authored cohort numbering; the base shifts it clear of the ones before.
inline constexpr std::uint8_t kOpeningCohortBase=0;   // 1..3
inline constexpr std::uint8_t kForestCohortBase=3;    // 4
inline constexpr std::uint8_t kChaseCohortBase=4;     // 5..7
inline constexpr std::uint8_t kLedgeCohortBase=7;     // 8..11
inline constexpr std::uint8_t kBossCohortBase=11;     // 12..16
inline constexpr std::uint8_t kAllLastCohort=kBossCohortBase+kBossLastCohort;

inline constexpr std::size_t kAllSpawnCount=
    kSpawns.size()+kForestSpawns.size()+kChaseSpawns.size()+(kLedgeSpawns.size()-6)+kBossSpawns.size();

[[nodiscard]] consteval std::array<Spawn,kAllSpawnCount> build_all_spawns() noexcept {
    std::array<Spawn,kAllSpawnCount> out{};
    std::size_t next=0;
    const auto append=[&out,&next](std::span<const Spawn> rows,std::uint8_t base,int drop) noexcept {
        for(const auto& row:rows) {
            if(drop>=0 && row.cohort==static_cast<std::uint8_t>(drop)) { continue; }
            out[next]=row;
            out[next].cohort=static_cast<std::uint8_t>(row.cohort+base);
            ++next;
        }
    };
    append(kSpawns,kOpeningCohortBase,-1);
    append(kForestSpawns,kForestCohortBase,-1);
    append(kChaseSpawns,kChaseCohortBase,-1);
    append(kLedgeSpawns,kLedgeCohortBase,static_cast<int>(kLedgePrefightCohort));
    append(kBossSpawns,kBossCohortBase,-1);
    return out;
}
inline constexpr auto kAllSpawns=build_all_spawns();

// A repeated (registry, source) pair would make the ledger admit an actor into whichever row it
// reached first and then wait forever on the other, so it is refused at compile time.
static_assert([]{
    for(std::size_t i=0;i<kAllSpawns.size();++i) {
        for(std::size_t j=0;j<i;++j) {
            if(kAllSpawns[i].registry==kAllSpawns[j].registry
                && kAllSpawns[i].source==kAllSpawns[j].source) { return false; }
        }
        if(kAllSpawns[i].cohort==0 || kAllSpawns[i].cohort>kAllLastCohort) { return false; }
        if(kAllSpawns[i].count!=kAllSpawns[i].loose+kAllSpawns[i].second) { return false; }
    }
    return true;
}(),"the combined strike source table repeats a source, or carries an out-of-range cohort");

[[nodiscard]] constexpr const Spawn* all_spawn(std::uint32_t registry,std::uint16_t source) noexcept {
    for(const auto& row:kAllSpawns) {
        if(row.registry==registry && row.source==source) { return &row; }
    }
    return nullptr;
}
/** @return The global cohort a section's own cohort became. */
[[nodiscard]] constexpr std::uint8_t opening_cohort(std::uint8_t local) noexcept { return local+kOpeningCohortBase; }
[[nodiscard]] constexpr std::uint8_t forest_cohort(std::uint8_t local) noexcept { return local+kForestCohortBase; }
[[nodiscard]] constexpr std::uint8_t chase_cohort(std::uint8_t local) noexcept { return local+kChaseCohortBase; }
[[nodiscard]] constexpr std::uint8_t ledge_cohort(std::uint8_t local) noexcept { return local+kLedgeCohortBase; }
[[nodiscard]] constexpr std::uint8_t boss_cohort(std::uint8_t local) noexcept { return local+kBossCohortBase; }

// Every authored trigger box the mission waits on, in one table for the same reason as the
// sources: the controller marks entry from one player-position poll and answers every graph's
// observation out of one bitset.
inline constexpr std::size_t kAllVolumeCount=
    std::size(kVolumes)+kForestVolumes.size()+kChaseVolumes.size()+kLedgeVolumes.size()+kBossVolumes.size();

[[nodiscard]] consteval std::array<Volume,kAllVolumeCount> build_all_volumes() noexcept {
    std::array<Volume,kAllVolumeCount> out{};
    std::size_t next=0;
    const auto append=[&out,&next](std::span<const Volume> rows) noexcept {
        for(const auto& row:rows) { out[next++]=row; }
    };
    append(kVolumes); append(kForestVolumes); append(kChaseVolumes);
    append(kLedgeVolumes); append(kBossVolumes);
    return out;
}
inline constexpr auto kAllVolumes=build_all_volumes();
/** Trigger ownership follows its authored section, including corridor registries. */
[[nodiscard]] constexpr int volume_region(std::size_t index) noexcept {
    if(index<std::size(kVolumes)) { return kRegion; }
    index-=std::size(kVolumes);
    if(index<kForestVolumes.size()) { return kForestRegion; }
    index-=kForestVolumes.size();
    if(index<kChaseVolumes.size()) { return kChaseRegion; }
    index-=kChaseVolumes.size();
    if(index<kLedgeVolumes.size()+kBossVolumes.size()) { return kLedgeRegion; }
    return -1;
}
static_assert([]{
    for(std::size_t i=0;i<kAllVolumes.size();++i) {
        for(std::size_t j=0;j<i;++j) {
            if(kAllVolumes[i].registry==kAllVolumes[j].registry
                && kAllVolumes[i].slot==kAllVolumes[j].slot) { return false; }
        }
    }
    return true;
}(),"the combined strike volume table names the same trigger twice");

} // namespace sunrise::state::activity::strike_pact
