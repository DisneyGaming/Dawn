#pragma once

#include "round_activity_definition.h"
#include "haunted_forest_environment.h"
#include "haunted_forest_generated_data.h"
#include "haunted_forest_hud.h"
#include "haunted_forest_music.h"
#include "haunted_forest_population_data.h"
#include "haunted_forest_registries.h"
#include "haunted_forest_sequences.h"

namespace sunrise::server::runtime::activity::haunted_forest::mode {

namespace round = round_activity;

// Registry admission is pointer-based for trusted capabilities. Keep one
// package-owned array alive for the complete activity lifetime, while retaining
// each source registry's immutable slot span.
inline constexpr std::array<registry::Definition,4> kOwnedRegistries{{
    kRegistries[0],kArenaRegistry[0],kAuxiliaryRegistry[0],music_data::kRegistry,
}};

inline constexpr auto kMusicDefinition = music_data::definition(&kOwnedRegistries[3]);
inline constexpr public_event::sequence::Definition kTeleportSequence{
    &kOwnedRegistries[0],&sequence_data::kTeleportGraph,89,0x81550021,0x80FF5CC1,0};
inline constexpr public_event::sequence::Definition kDefeatedSequence{
    &kOwnedRegistries[1],&sequence_data::kDefeatedGraph,129,0x8155018E,0x80B4C1BD,0};
inline constexpr std::array<round_sequence::Binding,2> kRoundSequences{{
    {&kTeleportSequence,round_sequence::phase_mask(timed_round::Phase::toEncounter)},
    {&kDefeatedSequence,static_cast<std::uint16_t>(round_sequence::phase_mask(timed_round::Phase::returning)
        |round_sequence::phase_mask(timed_round::Phase::rewards))},
}};
inline constexpr auto kRoundMusic = music_data::binding(&kMusicDefinition);

inline constexpr coo::ModuleBinding kModule{{0x81550015U,0x81550015U,0,0},1};

// [12] pf_reward_chest.o_chest, [13] pf_reward_chest.o_block, [14..18] o_coffer_1..5
// (registry slots 63,64,65,66,67). The coffer's 80BEE3B1 template includes
// the native 80804FBB interaction base; enable it through the source override.
inline constexpr auto kPlacements=[] {
    std::array<placement::Capability,19> output{};
    std::size_t cursor{};
    for (const auto& platform : kPlatforms) {
        output[cursor++]={&kOwnedRegistries[0],platform.main.slot,1};
        output[cursor++]={&kOwnedRegistries[0],platform.approach.slot,1};
        output[cursor++]={&kOwnedRegistries[0],platform.plate.slot,1};
    }
    output[cursor++]={&kOwnedRegistries[0],kStartSlots[55].index,1};
    output[cursor++]={&kOwnedRegistries[0],kStartSlots[56].index,1};
    for (std::size_t slot=63;slot<=67;++slot)
        output[cursor++]={&kOwnedRegistries[0],kStartSlots[slot].index,1};
    output[12].interactionMode=placement::interaction::Mode::enabled;
    for (std::size_t i=14;i<output.size();++i)
        output[i].interactionMode=placement::interaction::Mode::enabled;
    return output;
}();
static_assert(kPlacements.size()==19,"forest placements: 12 platform rows, chest, block, 5 coffers");
static_assert(kPlacements[18].slot==67,"the last coffer must land on registry slot 67");

inline constexpr auto kOccupancy=[] {
    std::array<occupancy_wait::Binding,5> output{};
    for (std::size_t i=0;i<kPlatforms.size();++i)
        output[i]={&kOwnedRegistries[0],kPlatforms[i].occupancy.slot,0};
    output[4]={&kOwnedRegistries[0],103,0}; // pm_pit, descriptor81550161.
    return output;
}();

inline constexpr forest_generator::wire::State kGeneratorIgnition=[] {
    forest_generator::wire::State value{};
    value.primary.overrides=forest_generator::wire::Enabled|forest_generator::wire::Anchors;
    value.primary.enabled=true;
    // Match all four authored platform approaches in the 5x5 native grid.
    // Type-37 order is +X, -X, +Y, -Y. The three alternate platform spawn
    // sets are 69310372, 69310373, 69310371; the opening is 79E3AB1F.
    // Container defaults offset the alternate approaches by a whole cell.
    // Heights are 15 m steps from the generator's authored origin.
    value.primary.anchors={{{2,1,0,true},{2,1,0,true},{2,0,0,true},{2,2,0,true}}};
    // The permanent Terror arena occupies the center of the authored 5x5
    // layout. These are coarse solver cells (9 * 15 m), not fine piece
    // coordinates. FF2F80 excludes the complete XY column at every height;
    // leaving it available generates walkways through the arena's collision.
    value.primary.blockedCount=1;
    value.primary.blockedCells[0]={2,2,0};
    return value;
}();
inline constexpr auto kGeneratorDisable=[] {
    auto value=kGeneratorIgnition;
    value.primary.overrides=forest_generator::wire::Enabled|forest_generator::wire::Anchors;
    value.primary.enabled=false;
    return value;
}();
inline constexpr std::array<forest_generator::Action,2> kGeneratorActions{{
    {1,kGeneratorIgnition},{2,kGeneratorDisable},
}};
inline constexpr std::array<forest_generator::Capability,1> kGenerators{{
    {&kOwnedRegistries[0],98,kGeneratorActions,"forest.seed",
        {{{},{},{},{"forest.entry_column","forest.entry_height"}}},true},
}};

inline constexpr std::array<world_device::Action,2> kPlateDeviceActions{{
    {1,world_device::Position,{{0.2F,0,false},{1.0F,0,false},{}}},
    {2,world_device::Position,{{0.1F,0,false},{1.0F,0,false},{}}},
}};
/** kDevices[4] = pf_reward_chest.d_block (registry slot 57): the chest plinth, byte-identical to the plate devices. */
inline constexpr std::uint16_t kRewardChestDeviceIndex=4;
inline constexpr auto kDevices=[] {
    std::array<world_device::Capability,5> output{};
    for (std::size_t i=0;i<kPlatforms.size();++i)
        output[i]={&kOwnedRegistries[0],kPlatforms[i].device.slot,kPlateDeviceActions};
    output[kRewardChestDeviceIndex]={&kOwnedRegistries[0],57,kPlateDeviceActions};
    return output;
}();

inline constexpr std::array<native_capture::Binding,4> kCaptures{{
    {2,0x80C01781U,0x815B8B3BU,0x4C8,0x248,"capture.duration_ms"},
    {5,0x80C01781U,0x815B8B3BU,0x4C8,0x248,"capture.duration_ms"},
    {8,0x80C01781U,0x815B8B3BU,0x4C8,0x248,"capture.duration_ms"},
    {11,0x80C01781U,0x815B8B3BU,0x4C8,0x248,"capture.duration_ms"},
}};

inline constexpr std::array<round::Platform,4> kRoundPlatforms{{
    {{{0,1,2}},0,0,0,1},
    {{{3,4,5}},1,1,1,2},
    {{{6,7,8}},2,2,2,3},
    {{{9,10,11}},3,3,3,4},
}};
inline constexpr std::array<std::uint16_t,2> kRewardPlacementIndices{{12,13}};
/**
 * E2: the five o_coffer_1..5 placements. Published only while the host switch
 * experiments.omega.forest_reward_coffers is on. Native placement authority validates the
 * rewards-phase frame as a whole (every requested slot must be type 4 with the roster auth flag),
 * so an unwritable coffer slot would void the authored chest placement with it. Registry slots
 * 63-67 have never been published in a live run, so they stay off until one proves them.
 */
inline constexpr std::array<std::uint16_t,5> kCofferPlacementIndices{{14,15,16,17,18}};

inline constexpr std::array<native_activity_transit::Destination,6> kTransitDestinations{{
    // Destination 1 is the original start. Its native teleport placements are
    // slots 44-46, not the side platform's capture-pad destinations 36-38.
    {1,104,0x79E3AB1FU},{2,104,0x69310371U},{3,104,0x69310372U},
    {4,104,0x69310373U},{5,104,0x8BC697B5U},{6,104,0x29E00F76U},
}};

// All five authored ho_teleporter sources use player-attached prefab80FECF37.
// Its timer component80804DAC (80FECE91, initializer+D8+4C) starts the
// SCOT teleport handoff pattern after 0.75s. Playback stays in native assets.
inline constexpr std::array<status_effect::Capability,5> kTransitEffectSources{{
    {&kOwnedRegistries[0],39},{&kOwnedRegistries[0],19},{&kOwnedRegistries[0],9},
    {&kOwnedRegistries[0],29},{&kOwnedRegistries[0],80},
}};
inline constexpr std::array<transit_effect::TargetPlacement,3> kTransitTarget1{{
    {&kOwnedRegistries[0],44,0x815500CDU,0x4C8},{&kOwnedRegistries[0],45,0x815500D0U,0x4C8},{&kOwnedRegistries[0],46,0x815500D3U,0x4C8}}};
inline constexpr std::array<transit_effect::TargetPlacement,3> kTransitTarget2{{
    {&kOwnedRegistries[0],16,0x8155007CU,0x4C8},{&kOwnedRegistries[0],17,0x8155007FU,0x4C8},{&kOwnedRegistries[0],18,0x81550082U,0x4C8}}};
inline constexpr std::array<transit_effect::TargetPlacement,3> kTransitTarget3{{
    {&kOwnedRegistries[0],6,0x81550061U,0x4C8},{&kOwnedRegistries[0],7,0x81550064U,0x4C8},{&kOwnedRegistries[0],8,0x81550067U,0x4C8}}};
inline constexpr std::array<transit_effect::TargetPlacement,3> kTransitTarget4{{
    {&kOwnedRegistries[0],26,0x81550097U,0x4C8},{&kOwnedRegistries[0],27,0x8155009AU,0x4C8},{&kOwnedRegistries[0],28,0x8155009DU,0x4C8}}};
inline constexpr std::array<transit_effect::TargetPlacement,3> kTransitTarget5{{
    {&kOwnedRegistries[0],41,0x815500C4U,0x4C8},{&kOwnedRegistries[0],42,0x815500C7U,0x4C8},{&kOwnedRegistries[0],43,0x815500CAU,0x4C8}}};
inline constexpr std::array<transit_effect::TargetPlacement,3> kTransitTarget6{{
    {&kOwnedRegistries[0],52,0x815500E5U,0x4C8},{&kOwnedRegistries[0],53,0x815500E8U,0x4C8},{&kOwnedRegistries[0],54,0x815500EBU,0x4C8}}};
inline constexpr auto kTransitLanding=[] {
    std::array<std::array<transit_effect::LandingPlacement,2>,kPlatforms.size()> output{};
    for(std::size_t i=0;i<kPlatforms.size();++i) {
        // The capture plate owns a separate, cycling generation. Only the
        // stable terrain placements prove a safe landing; capture is armed on arrival.
        const std::array assets{kPlatforms[i].main,kPlatforms[i].approach};
        for(std::size_t j=0;j<assets.size();++j)
            output[i][j]={{&kOwnedRegistries[0],assets[j].slot,assets[j].definition,0x4C8},1};
    }
    return output;
}();
inline constexpr std::array<transit_effect::Route,6> kTransitEffectRoutes{{
    {1,4,504900,kTransitTarget1,{transit_effect::ArrivalKind::monitor,&kOwnedRegistries[0],110,0},kTransitLanding[0]},
    {2,1,504900,kTransitTarget2,{transit_effect::ArrivalKind::monitor,&kOwnedRegistries[0],114,0},kTransitLanding[1]},
    {3,2,504900,kTransitTarget3,{transit_effect::ArrivalKind::monitor,&kOwnedRegistries[0],118,0},kTransitLanding[2]},
    {4,3,504900,kTransitTarget4,{transit_effect::ArrivalKind::monitor,&kOwnedRegistries[0],122,0},kTransitLanding[3]},
    {5,4,504900,kTransitTarget5,{transit_effect::ArrivalKind::monitor,&kOwnedRegistries[2],2,0}},
    {6,4,504900,kTransitTarget6,{transit_effect::ArrivalKind::playerTrigger,&kOwnedRegistries[0],102,142}},
}};
inline constexpr transit_effect::Definition kTransitEffects{kTransitEffectSources,kTransitEffectRoutes};

inline constexpr auto kIntroIndices(Faction faction) noexcept {
    switch(faction) {
    case Faction::cabal:return std::span<const std::uint16_t>(kCabalPopulationIndices.intro);
    case Faction::fallen:return std::span<const std::uint16_t>(kFallenPopulationIndices.intro);
    case Faction::hive:return std::span<const std::uint16_t>(kHivePopulationIndices.intro);
    case Faction::vex:return std::span<const std::uint16_t>(kVexPopulationIndices.intro);
    }
    return std::span<const std::uint16_t>{};
}
inline constexpr auto kMidIndices(Faction faction) noexcept {
    switch(faction) {
    case Faction::cabal:return std::span<const std::uint16_t>(kCabalPopulationIndices.mid);
    case Faction::fallen:return std::span<const std::uint16_t>(kFallenPopulationIndices.mid);
    case Faction::hive:return std::span<const std::uint16_t>(kHivePopulationIndices.mid);
    case Faction::vex:return std::span<const std::uint16_t>(kVexPopulationIndices.mid);
    }
    return std::span<const std::uint16_t>{};
}
inline constexpr auto kEncounters=[] {
    std::array<round::Encounter,kBossPopulationCount> output{};
    for(std::size_t i=0;i<output.size();++i) {
        const auto faction=kBossFactions[i];
        output[i]={static_cast<std::uint16_t>(i),kIntroIndices(faction),kMidIndices(faction),
            true,"reconstructed_host_selection"};
    }
    return output;
}();

// Native admission uses this copied array, not the arena header's original
// addresses. Tactical references are retargeted to the copied arena registry
// as well, preserving the 109 authored source capabilities exactly.
inline constexpr auto kOwnedPopulationCapabilities=[] {
    std::array<population::Capability,kPopulationCapabilities.size()+1> output{};
    for(std::size_t i=0;i<kPopulationCapabilities.size();++i) {
        output[i]=kPopulationCapabilities[i];
        output[i].registry=&kOwnedRegistries[1];
        output[i].tactical.registry=kOwnedRegistries[1].key;
    }
    // sq_darkblade uses authored sr_darkblade and obj_darkblade row0/task162.
    // Keep its native prefab/selector attributes and AI; no synthetic health edits.
    output.back()={.registry=&kOwnedRegistries[0],.slot=59,.rule=160,
        .tactical={kOwnedRegistries[0].key,61,0},.hasRule=true,
        .allowCycles=false,.namedMember=60,.categories=1};
    return output;
}();

inline constexpr std::array<std::uint16_t,1> kRewardPopulationIndices{{
    static_cast<std::uint16_t>(kPopulationCapabilities.size())}};

inline constexpr std::uint32_t score_weight(Rank rank) noexcept {
    switch(rank) {
    case Rank::minor:return 2;
    case Rank::major:return 4;
    case Rank::miniboss:return 6;
    case Rank::ambiguous:
    case Rank::nonRanked:return 0;
    }
    return 0;
}
inline constexpr std::uint32_t score_weight(std::uint32_t memberPrefabTag) noexcept {
    for(const auto& prefab:kRankedPrefabs)
        if(prefab.tag==memberPrefabTag)return score_weight(prefab.rank);
    return 0;
}
inline constexpr std::size_t kPositiveScoreCount=[] {
    std::size_t count{};
    for(const auto& prefab:kRankedPrefabs)if(score_weight(prefab.rank))++count;
    return count;
}();
inline constexpr auto kScoreStorage=[] {
    std::array<round::Score,kRankedPrefabs.size()> output{};
    std::size_t cursor{};
    for(const auto& prefab:kRankedPrefabs) {
        const auto weight=score_weight(prefab.rank);
        if(weight)output[cursor++]={prefab.tag,weight,weight};
    }
    return output;
}();
inline constexpr auto kScores=std::span<const round::Score>(kScoreStorage).first(kPositiveScoreCount);

inline constexpr coo::CommandSpec coordinatorSpec(round::Operation operation) noexcept {
    const auto observed=operation==round::Operation::plateOccupied
        || operation==round::Operation::progressFull
        || operation==round::Operation::travelArrived
        || operation==round::Operation::bossDead
        || operation==round::Operation::retireEncounter;
    const auto wait=observed?coo::Wait::observed:
        (operation==round::Operation::capture || operation==round::Operation::spawnEncounter
            ?coo::Wait::completed:coo::Wait::requested);
    return {observed?coo::Operation::observation:coo::Operation::mechanic,
        kModule.asset,static_cast<std::uint32_t>(operation)+1U,wait};
}

inline constexpr auto kRoundCommands=[] {
    std::array<round::CommandBinding,13> output{};
    for(std::size_t i=0;i<output.size();++i) {
        const auto operation=static_cast<round::Operation>(i);
        output[i]={coordinatorSpec(operation),operation};
    }
    return output;
}();

inline constexpr auto kCapabilities=[] {
    std::array<coo::script::Capability,14> output{{
        {"persistent.start","composition",{coo::Operation::mechanic,kModule.asset,1,coo::Wait::requested},0},
        {},{},{},{},{},{},{},{},{},{},{},{},{},
    }};
    constexpr std::array<std::string_view,13> names{{
        "prepareEntry","plateOccupied","capture","startTraversal","progressFull",
        "travelEncounter","travelArrived","spawnEncounter","bossDead","retireEncounter",
        "travelEntry","travelRewards","complete",
    }};
    for(std::size_t i=0;i<names.size();++i)
        output[i+1]={names[i],"nativeActivity",kRoundCommands[i].command,0};
    return output;
}();

inline constexpr auto kActions=[] {
    std::array<NativeAction,13> output{};
    for(std::size_t i=0;i<output.size();++i)
        output[i]={kRoundCommands[i].command,0,
            i==static_cast<std::size_t>(round::Operation::spawnEncounter)
                ? std::string_view{"encounter.population.count"} : std::string_view{}};
    return output;
}();

inline constexpr coo::script::ModuleCapability kModules[]{{"persistent",kModule}};
inline constexpr std::int32_t kActivityOrdinals[]{78};
inline constexpr std::array<std::int32_t,27> kResultsVariants=[] {
    std::array<std::int32_t,27> output{};
    for(std::int32_t i=0;i<=26;++i)output[static_cast<std::size_t>(i)]=i;
    return output;
}();

inline constexpr coo::script::ParameterCapability kParameters[]{
    {"host.tick_hz",1,120,30,false},
    {"capture.duration_ms",1000,30000,8000,false},
    // A nonzero authored default supplies the stable per-round selection seed.
    {"forest.seed",0,UINT32_MAX,0,false},
    {"forest.entry_column",2,2,2,false},
    {"forest.entry_height",0,2,2,false},
    {"forest.duration_ms",60000,3600000,900000,false},
    {"encounter.population.count",1,1,1,false},
};

inline constexpr auto kDirectives=haunted_forest_hud::actions();
inline constexpr coo::Asset kDirectiveSource{0x1EB557ADU,0x8155035AU,68,0};

inline constexpr round::GeneratorPalette kGeneratorPalette{
    kGeneratedResourceTag,kGeneratedWorkerDefinitionTag,kGeneratedWorkerDefinitionOffset,
    kGeneratedPalettes,
};
inline constexpr auto kEnvironmentData=environment::data_factory(&kOwnedRegistries[0]);
inline constexpr auto kEnvironment=kEnvironmentData.definition();
inline constexpr round::Hud kHud{
    haunted_forest_hud::kInitialEnter,haunted_forest_hud::kEnter,
    haunted_forest_hud::kStart,haunted_forest_hud::kResume,haunted_forest_hud::kPauseTerror,
    haunted_forest_hud::kTerror,haunted_forest_hud::kCollapse,haunted_forest_hud::kResults,
    26,1,kResultsVariants,"forest.duration_ms",100,
};
inline constexpr round::Definition kRounds{
    kModule.asset,
    {"round.entry","round.traversal","round.toEncounter","round.encounter","round.returning","round.rewards"},
    // Each branch returns to the original start and re-arms its capture plate.
    // The other authored side platforms remain available as bindings, but are
    // not a host-invented round-robin itinerary for this activity.
    kRoundCommands,std::span<const round::Platform>(kRoundPlatforms).first(1),0,1,2,kGeneratorPalette,kEncounters,kHud,kScores,
    5,6,kRewardPlacementIndices,kTransitDestinations,haunted_forest_hud::kCompletionTimerAsset,
    kNamedEliteCompletionGroup,900000,&kEnvironment,&kRoundMusic,kRoundSequences,
    kRewardPopulationIndices,4,&kTransitEffects,kCofferPlacementIndices,kRewardChestDeviceIndex,
};

inline const coo::script::Profile kProfile{
    "haunted_forest.native.v1","nativeOtherActivities",coo::Schema::otherMissions,
    kCapabilities,kModules,{}, {}, {}, {}, {}, {}, kParameters,
};

inline constexpr auto kCacheInteractionGates=[] {
    std::array<equipment_interaction::Gate,5> gates{};
    for(std::size_t i=0;i<gates.size();++i) {
        gates[i].registry=0x34D23982U;
        gates[i].slot=static_cast<std::uint16_t>(63+i);
        gates[i].requiredProfileItem=0xE31B110BU; // Cipher Decoder, installed definition12539.
        gates[i].requiredQuantity=1;
    }
    return gates;
}();

inline constexpr TriggeredPlacementPose kChestPose[]{ {0x34D23982U,55,104,0.F,1.F} };

inline const NativeActivityDefinition kActivity=[] {
    NativeActivityDefinition result{
    "infinite_abyss",L"infinite_abyss.json",13,&kProfile,
    kOwnedRegistries,kOwnedPopulationCapabilities,kPlacements,kActions,kModule,
    {},{},{},{},{},kOccupancy,kActivityOrdinals,{},"host.tick_hz",kCaptures,kGenerators,
    kDirectives,kDirectiveSource,kDevices,{},false,&kRounds,
    };
    result.equipmentInteractionGates=kCacheInteractionGates;
    result.triggeredPlacementPoses=kChestPose;
    return result;
}();

} // namespace sunrise::server::runtime::activity::haunted_forest::mode
