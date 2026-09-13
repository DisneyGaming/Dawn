#pragma once
#include "../coo/campaign_scan.h"
#include "../coo/dialogue_service.h"
#include "../coo/objective_service.h"
#include "../coo/lifecycle_service.h"
#include "catalog_tasks.h"
#include "../coo/actor_program_service.h"
namespace sunrise::state::activity::strike_pact {
struct BossPublication final {
    std::uint32_t sceneGeneration{},laserRevision{1};
    std::uint8_t room{},fights{},retreated{},cleared{},arrived{},laserRooms{},laserHighRooms{},laserObjects{};
    bool prepared{},sceneFinished{},immune{},dead{},healthObserved{};
    bool minotaurSpawned{},minotaurDead{};
    float health{1.F};
};
struct BossRequest final {
    coo::Generation owner{};EnemyReceipt enemy{};
    std::uint8_t stage{};std::uint32_t revision{};bool requested{};
};
struct Frame final {
    bool campaign{};coo::CampaignScan scan{};
    bool enabled{},checked{},finished{},populationFault{},services{};
    /** Index into the mission's authored phase list: opening, forest, chase, ledge/boss. */
    std::uint8_t section{};
    std::uint32_t spawnGeneration{},objective{},revision{};
    /** One bit per global cohort in the combined source table; bit 0 is never used. */
    std::uint64_t cohorts{};
    /** Devices the mission has taken authority over, and which of those it has since released.
     * A device only carries a body once the mission owns it, so an untouched barrier keeps its
     * authored state instead of being republished every frame. */
    std::uint32_t devices{},devicesOpen{};
    bool portalActive{};
    /** Nonzero once the Forest generator has been activated; it is the layout for the whole run,
     * so streaming and reattachment republish the same seed rather than drawing a new one. */
    std::uint32_t generatorSeed{};
    bool firstForestAreaComplete{};
    /** Shared native activity-clock ticks; timer epochs use exactly this clock domain. */
    std::uint64_t activityTime{},endEpoch{};
    std::uint8_t musicCandidate{255};
    /** Frozen source-template selector copied from the exact launch activity. */
    std::uint8_t enemyVariant{};
    std::uint16_t region{120};
    /** A death respawns on the lifetime's spawn set, and the destination's own arrival set belongs
     * to the opening. Each region the mission holds names its own, so the roster override follows
     * the mission rather than the destination once one is selected. */
    std::uint32_t checkpointSliceSet{},checkpointSpawnSet{};
    /** The authored task group each source is assigned, biased by one so that a default-built
     * frame publishes the native evaluate-only selection rather than group zero. Indexed like the
     * combined source table. */
    std::array<std::uint8_t,kAllSpawns.size()> taskPlusOne{};
    std::array<std::uint32_t,32> generations{};
    std::uint8_t activeRow{coo::kNoDialogue};
    coo::ObjectiveState presentation{};
    coo::CompletionPublication completion{};
    coo::ActorPublication harvester{};
    BossPublication boss{};
};
}
