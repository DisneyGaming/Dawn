#pragma once

#include "forest_generator_service.h"
#include "native_activity_definition.h"
#include "native_activity_transit.h"
#include "round_music_service.h"
#include "round_sequence_service.h"
#include "round_environment_definition.h"
#include "timed_round_service.h"
#include "transit_effect_service.h"
#include "../../../state/activity/native_population_events.h"
#include "../../../state/activity/coo/mission_script.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>

namespace dawn::server::runtime::activity::round_activity {

namespace coo = state::activity::coo;
namespace native_population = state::activity::native_population;

inline constexpr std::size_t kMaximumPlatforms = 4;
inline constexpr std::size_t kMaximumEncounters = 16;
inline constexpr std::size_t kMaximumEncounterPopulations = 32;
inline constexpr std::size_t kMaximumScoreEntries = 256;
inline constexpr std::size_t kMaximumGeneratedActors = 1024;
inline constexpr std::uint32_t kDefaultDurationMilliseconds = 30000;
inline constexpr std::uint16_t kMissingIndex = (std::numeric_limits<std::uint16_t>::max)();

enum class Operation : std::uint8_t {
    prepareEntry,
    plateOccupied,
    capture,
    startTraversal,
    progressFull,
    travelEncounter,
    travelArrived,
    spawnEncounter,
    bossDead,
    retireEncounter,
    travelEntry,
    travelRewards,
    complete,
};

struct CommandBinding final {
    coo::CommandSpec command{};
    Operation operation{};
};

struct PhaseRoles final {
    std::string_view entry{"round.entry"};
    std::string_view traversal{"round.traversal"};
    std::string_view toEncounter{"round.toEncounter"};
    std::string_view encounter{"round.encounter"};
    std::string_view returning{"round.returning"};
    std::string_view rewards{"round.rewards"};
};

struct Platform final {
    std::array<std::uint16_t, 3> placementIndexes{};
    std::uint16_t occupancyIndex{kMissingIndex};
    std::uint16_t captureIndex{kMissingIndex};
    std::uint16_t deviceIndex{kMissingIndex};
    std::uint32_t transitDestinationId{};
};

struct GeneratorPalette final {
    std::uint32_t resourceTag{};
    std::uint32_t workerDefinitionTag{};
    std::uint32_t workerDefinitionOffset{};
    std::span<const native_population::PaletteDefinition> palettes{};
};

struct Encounter final {
    std::uint16_t bossPopulationIndex{kMissingIndex};
    std::span<const std::uint16_t> introPopulationIndices{};
    std::span<const std::uint16_t> midPopulationIndices{};
    bool selectStablePerRound{true};
    std::string_view hostSelectionPolicy{};
};

struct Hud final {
    std::uint32_t initialEnterAction{};
    std::uint32_t enterAction{};
    std::uint32_t startAction{};
    std::uint32_t resumeAction{};
    std::uint32_t pauseAction{};
    std::uint32_t terrorAction{};
    std::uint32_t collapseAction{};
    std::uint32_t resultsAction{};
    std::int32_t maximumBranchVariant{};
    std::int32_t collapseVariant{};
    std::span<const std::int32_t> resultsVariants{};
    std::string_view durationParameter{};
    std::uint32_t progressTarget{};
};

struct Score final {
    std::uint32_t memberPrefabTag{};
    std::uint32_t normalWeight{};
    std::uint32_t eliteWeight{};
};

struct Definition final {
    // Type zero identifies the trusted coordinator and carries no native wire body.
    coo::Asset moduleAsset{};
    PhaseRoles roles{};
    std::span<const CommandBinding> commands{};
    std::span<const Platform> platforms{};
    std::uint16_t generatorIndex{kMissingIndex};
    std::uint32_t generatorEnableAction{};
    std::uint32_t generatorDisableAction{};
    GeneratorPalette generatorPalette{};
    std::span<const Encounter> encounters{};
    Hud hud{};
    std::span<const Score> scores{};
    // The arena destination is separate from the rotating platform routes.
    std::uint32_t encounterDestinationId{};
    std::uint32_t rewardDestinationId{};
    std::span<const std::uint16_t> rewardPlacementIndices{};
    std::span<const native_activity_transit::Destination> transitDestinations{};
    coo::Asset completionTimerAsset{};
    // Zero selects normalWeight. A nonzero value is compared only after the
    // admitted member prefab has matched an exact Score row.
    std::uint32_t eliteCompletionGroup{};
    std::uint32_t defaultDurationMilliseconds{kDefaultDurationMilliseconds};
    /** Optional host-policy environment modifiers for repeated rounds. */
    const round_environment::Definition* environment{};
    /** Optional presentation-only music binding; null preserves other activities. */
    const round_music::Binding* music{};
    std::span<const round_sequence::Binding> sequences{};
    /** Reward-area sources survive encounter retirement and never count as bosses. */
    std::span<const std::uint16_t> rewardPopulationIndices{};
    std::uint16_t trapOccupancyIndex{kMissingIndex};
    const transit_effect::Definition* transitEffects{};
    /**
     * Reward placements published only while the host admits them (PersistentActivity's
     * admit_experimental_reward_placements). Empty for a definition with none. They are validated
     * exactly like rewardPlacementIndices, because an admitted run publishes them in the same
     * all-or-nothing placement frame.
     */
    std::span<const std::uint16_t> experimentalRewardPlacementIndices{};
    /** Device driven with action 1 when the rewards phase places the chest (kMissingIndex = none). */
    std::uint16_t rewardDeviceIndex{kMissingIndex};
};

/** One configured duration feeds both round expiration and the native HUD. */
template<class Document>
[[nodiscard]] bool duration_ticks(const Definition& definition,const Document& document,
    std::uint64_t& ticks) noexcept {
    std::uint64_t milliseconds=definition.defaultDurationMilliseconds;
    if(!definition.hud.durationParameter.empty()) {
        const auto* parameter=document.views().parameter(definition.hud.durationParameter);
        if(!parameter || !parameter->value)return false;
        milliseconds=parameter->value;
    }
    return activity_clock::wire::from_milliseconds(milliseconds,ticks) && ticks!=0;
}
template<class Document>
[[nodiscard]] bool bind_timer(const Definition& definition,const Document& document,
    std::span<cue_presentation::Action> directives) noexcept {
    std::uint64_t ticks{};
    if(!duration_ticks(definition,document,ticks))return false;
    if(!definition.hud.startAction)return true;
    unsigned matches{};
    for(auto& action:directives)if(action.id==definition.hud.startAction) {
        if(action.timer!=cue_presentation::TimerOperation::start)return false;
        action.durationTicks=ticks;++matches;
    }
    return matches==1;
}

[[nodiscard]] inline constexpr std::array<std::string_view, 6> phase_roles(
    const PhaseRoles& roles) noexcept {
    return {roles.entry, roles.traversal, roles.toEncounter, roles.encounter,
        roles.returning, roles.rewards};
}

[[nodiscard]] inline constexpr bool valid_index(std::uint16_t index,
    std::size_t size) noexcept {
    return index != kMissingIndex && index < size;
}

[[nodiscard]] inline constexpr bool valid_tag(std::uint32_t tag) noexcept {
    return tag != 0 && tag != (std::numeric_limits<std::uint32_t>::max)();
}

[[nodiscard]] inline constexpr bool valid_content_asset(const coo::Asset& asset) noexcept {
    // A module/content identity is not necessarily a reflected registry slot.
    // The generic type-255 module asset is therefore valid as authored data.
    return valid_tag(asset.registry) && valid_tag(asset.definition)
        && (asset.type == 0 || asset.type == 255);
}

[[nodiscard]] inline constexpr bool valid_completion_timer_asset(
    const coo::Asset& asset) noexcept {
    // The countdown is a real local type-18 roster identity, not a generic
    // module/content asset.  Its global root is admitted by the roster path,
    // so round validation must not require it in NativeActivityDefinition's
    // local registry span.
    return valid_tag(asset.registry) && valid_tag(asset.definition)
        && asset.type == 18 && asset.slot <= 32767;
}

[[nodiscard]] inline bool valid_encounter(const Encounter& encounter) noexcept {
    const auto populationCount = encounter.introPopulationIndices.size()
        + encounter.midPopulationIndices.size() + 1U;
    if (encounter.bossPopulationIndex == kMissingIndex
        || populationCount > kMaximumEncounterPopulations) return false;
    for (const auto index : encounter.introPopulationIndices) {
        if (index == kMissingIndex || index == encounter.bossPopulationIndex) return false;
    }
    for (const auto index : encounter.midPopulationIndices) {
        if (index == kMissingIndex || index == encounter.bossPopulationIndex) return false;
        for (const auto prior : encounter.introPopulationIndices)
            if (index == prior) return false;
    }
    for (std::size_t i = 0; i < encounter.introPopulationIndices.size(); ++i)
        for (std::size_t j = 0; j < i; ++j)
            if (encounter.introPopulationIndices[i] == encounter.introPopulationIndices[j]) return false;
    return true;
}

[[nodiscard]] inline constexpr bool valid_score(const Score& score) noexcept {
    return valid_tag(score.memberPrefabTag)
        && score.normalWeight <= timed_round::Service::kMaximumProgressTarget
        && score.eliteWeight <= timed_round::Service::kMaximumProgressTarget
        && (score.normalWeight || score.eliteWeight);
}

[[nodiscard]] inline bool valid(const Definition& definition) noexcept {
    if (!valid_content_asset(definition.moduleAsset) || definition.platforms.empty()
        || definition.platforms.size() > kMaximumPlatforms || definition.commands.empty()
        || definition.commands.size() > kMaximumScoreEntries
        || definition.encounters.empty() || definition.encounters.size() > kMaximumEncounters
        || !definition.encounterDestinationId
        || !definition.generatorEnableAction || !definition.generatorDisableAction
        || definition.generatorPalette.palettes.empty()
        || !valid_tag(definition.generatorPalette.resourceTag)
        || !valid_tag(definition.generatorPalette.workerDefinitionTag)
        || !definition.generatorPalette.workerDefinitionOffset
        || !definition.rewardDestinationId || definition.rewardPlacementIndices.empty()
         || !membership_transit::Service::valid(definition.transitDestinations)
         || !valid_completion_timer_asset(definition.completionTimerAsset) || !definition.hud.progressTarget
         || static_cast<std::uint64_t>(definition.hud.progressTarget)
             > static_cast<std::uint64_t>((std::numeric_limits<std::int32_t>::max)())
        || !definition.defaultDurationMilliseconds
        || definition.generatorIndex == kMissingIndex) {
        return false;
    }
    const auto roles = phase_roles(definition.roles);
    for (std::size_t i = 0; i < roles.size(); ++i) {
        if (roles[i].empty()) return false;
        for (std::size_t j = 0; j < i; ++j) if (roles[i] == roles[j]) return false;
    }
    if (definition.generatorPalette.palettes.size() > 32) return false;
    for (std::size_t i = 0; i < definition.generatorPalette.palettes.size(); ++i) {
        const auto& palette = definition.generatorPalette.palettes[i];
        if (!valid_tag(palette.resourceTag) || !palette.definitionOffset
            || !palette.actorRowsOffset || !palette.actorRowCount) return false;
        for (std::size_t j = 0; j < i; ++j)
            if (definition.generatorPalette.palettes[j] == palette) return false;
    }
    for (std::size_t i = 0; i < definition.platforms.size(); ++i) {
        const auto& platform = definition.platforms[i];
        if (!platform.transitDestinationId) return false;
        for (const auto index : platform.placementIndexes)
            if (index == kMissingIndex) return false;
        if (platform.occupancyIndex == kMissingIndex || platform.captureIndex == kMissingIndex
            || platform.deviceIndex == kMissingIndex) return false;
        for (std::size_t j = 0; j < i; ++j)
            if (definition.platforms[j].transitDestinationId == platform.transitDestinationId)
                return false;
        bool authoredDestination{};
        for (const auto& destination : definition.transitDestinations)
            authoredDestination |= destination.id == platform.transitDestinationId;
        if (!authoredDestination) return false;
    }
    for (const auto& encounter : definition.encounters)
        if (!valid_encounter(encounter)) return false;
    for (const auto index : definition.rewardPlacementIndices)
        if (index == kMissingIndex) return false;
    for (const auto index : definition.experimentalRewardPlacementIndices)
        if (index == kMissingIndex) return false;
    if (definition.scores.size() > kMaximumScoreEntries) return false;
    for (std::size_t i = 0; i < definition.scores.size(); ++i) {
        const auto& score = definition.scores[i];
        if (!valid_score(score)) return false;
        for (std::size_t j = 0; j < i; ++j)
            if (definition.scores[j].memberPrefabTag == score.memberPrefabTag) return false;
    }
    return (!definition.music || round_music::valid(*definition.music))
        && round_sequence::valid(definition.sequences);
}

template <class NativeDefinition, class Document>
[[nodiscard]] bool valid(const Definition& definition, const NativeDefinition& native,
    const Document& document) noexcept {
    if (!valid(definition) || !document.views().valid || !native.profile
        || definition.generatorIndex >= native.generators.size()
        || native.generators.size() > 4 || native.populations.size() > 128
        || native.placements.size() > 32 || native.devices.size() > world_device::wire::kCapacity
        || native.occupancyWaits.size() > 32 || native.captures.size() > 4) return false;
    const auto& views = document.views();
    if (views.missionId != native.activity || views.profileId != native.profile->id
        || !coo::script::authorized(views, *native.profile)
        || !coo::MissionRuntime::valid(views.mission)
        || !native_capture::Runtime::valid(native, document)
        || views.mission.modules.size() != 1
        || views.mission.modules[0].asset != native.persistentModule.asset
        || views.mission.modules[0].id != native.persistentModule.id
        || native.persistentModule.asset != definition.moduleAsset) return false;
    if (definition.environment) {
        if (!round_environment::valid(*definition.environment)) return false;
        for (const auto& effect : definition.environment->effects) {
            bool admitted{};
            for (const auto& registry : native.registries)
                if (&registry == effect.registry) admitted = true;
            if (!admitted) return false;
        }
    }
    if (definition.transitEffects) {
        if(!transit_effect::valid(*definition.transitEffects))return false;
        const auto& effects=definition.transitEffects->effects;
        if(effects.size()+(definition.environment?definition.environment->effects.size():0)
            >status_effect::wire::kCapacity)return false;
        for(const auto& effect:effects) {
            bool admitted{};
            for(const auto& registry:native.registries)if(&registry==effect.registry)admitted=true;
            if(!admitted || effect.registry->bubble!=native.bubble)return false;
            if(definition.environment)for(const auto& other:definition.environment->effects)
                if(effect.registry==other.registry && effect.slot==other.slot)return false;
        }
        for(const auto& destination:definition.transitDestinations) {
            unsigned matches{};
            for(const auto& route:definition.transitEffects->routes)
                if(route.destination==destination.id)++matches;
            if(matches!=1)return false;
        }
    }
    if (definition.music) {
        bool admitted{};
        for (const auto& registry : native.registries)
            if (definition.music->definition->registry == &registry) admitted = true;
        if (!admitted || definition.music->definition->registry->bubble != native.bubble
            || definition.music->definition->registry->activity != native.activity)
            return false;
    }
    for (const auto& binding : definition.sequences) {
        bool admitted{};
        for (const auto& registry : native.registries)
            if (binding.definition->registry == &registry) admitted = true;
        if (!admitted || binding.definition->registry->bubble != native.bubble
            || binding.definition->registry->activity != native.activity) return false;
    }
    for (const auto& capability : native.populations)
        if (!population::valid(capability) || capability.registry->bubble != native.bubble) return false;
    for (const auto& capability : native.placements)
        if (!capability.registry || !registry::valid(*capability.registry)
            || capability.registry->bubble != native.bubble) return false;
    for (const auto& binding : native.occupancyWaits)
        if (!occupancy_wait::valid(binding) || binding.registry->bubble != native.bubble) return false;
    const auto& generator = native.generators[definition.generatorIndex];
    if (!generator.allowCycles || generator.seedParameter.empty()) return false;
    for (const auto& capability : native.generators) {
        if (!forest_generator::valid(capability) || capability.registry->bubble != native.bubble)
            return false;
        if (!capability.seedParameter.empty() && !views.parameter(capability.seedParameter))
            return false;
    }
    for (const auto& capability : native.devices)
        if (!world_device::valid(capability) || capability.registry->bubble != native.bubble) return false;
    const auto roles = phase_roles(definition.roles);
    for (const auto role : roles) {
        const auto* graph = views.role(role);
        if (!graph || graph->domain != "nativeActivity" || !coo::Executor::valid(graph->definition))
            return false;
        for (const auto& step : graph->definition.steps) for (const auto& command : step.commands) {
            const CommandBinding* match{};
            for (const auto& binding : definition.commands) {
                if (binding.command.operation == command.operation && binding.command.asset == command.asset
                    && binding.command.argument == command.argument && binding.command.wait == command.wait) {
                    if (match) return false;
                    match = &binding;
                }
            }
            if (!match) return false;
        }
    }
    for (const auto& platform : definition.platforms) {
        for (const auto index : platform.placementIndexes)
            if (!valid_index(index, native.placements.size())) return false;
        if (!valid_index(platform.occupancyIndex, native.occupancyWaits.size())
            || !valid_index(platform.captureIndex, native.captures.size())
            || !valid_index(platform.deviceIndex, native.devices.size())) return false;
    }
    for (const auto& encounter : definition.encounters) {
        if (!valid_index(encounter.bossPopulationIndex, native.populations.size())) return false;
        for (const auto index : encounter.introPopulationIndices)
            if (!valid_index(index, native.populations.size())) return false;
        for (const auto index : encounter.midPopulationIndices)
            if (!valid_index(index, native.populations.size())) return false;
    }
    for (const auto index : definition.rewardPlacementIndices)
        if (!valid_index(index, native.placements.size())) return false;
    for (const auto index : definition.experimentalRewardPlacementIndices)
        if (!valid_index(index, native.placements.size())) return false;
    if (definition.rewardDeviceIndex != kMissingIndex
        && !valid_index(definition.rewardDeviceIndex, native.devices.size())) return false;
    if (definition.trapOccupancyIndex != kMissingIndex
        && (!definition.music || !definition.music->trap
            || !valid_index(definition.trapOccupancyIndex, native.occupancyWaits.size()))) return false;
    for (std::size_t i=0;i<definition.rewardPopulationIndices.size();++i) {
        const auto index=definition.rewardPopulationIndices[i];
        if (!valid_index(index,native.populations.size())) return false;
        for (std::size_t j=0;j<i;++j)
            if (definition.rewardPopulationIndices[j]==index) return false;
        for (const auto& encounter:definition.encounters) {
            if (encounter.bossPopulationIndex==index) return false;
            for (const auto prior:encounter.introPopulationIndices) if (prior==index) return false;
            for (const auto prior:encounter.midPopulationIndices) if (prior==index) return false;
        }
    }
    if (!definition.hud.durationParameter.empty()) {
        const auto* parameter = views.parameter(definition.hud.durationParameter);
        if (!parameter || !parameter->value) return false;
    }
    bool encounterDestination{};
    for (const auto& destination : definition.transitDestinations)
        encounterDestination |= destination.id == definition.encounterDestinationId;
    if (!encounterDestination) return false;
    return true;
}

} // namespace dawn::server::runtime::activity::round_activity
