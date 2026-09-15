#pragma once

#include "native_activity_clock.h"
#include "../../../middleware/bap/activity_message/native/round_authority.h"
#include "../../../middleware/bap/activity_message/sensor_auth_update.h"
#include "../../../state/build_data/scenarios/definition.h"
#include "../../../state/activity/coo/executor.h"

#include <cstddef>
#include <cstdint>

namespace sunrise::server::runtime::activity::native_round_projection {

namespace coo = state::activity::coo;
namespace wire = middleware::bap::activity_message::native::round_authority;
namespace message = middleware::bap::activity_message::sensor_auth_update;
namespace layouts = state::build_data::scenarios;

inline constexpr std::uint32_t kTimerAuthSchema = 0x80809919U;
inline constexpr std::uint32_t kLifetimeAuthSchema = 0x8080991AU;
inline constexpr std::uint32_t kDirectorAuthSchema = 0x808099BFU;

[[nodiscard]] inline bool matches_resolved(const message::Group& actual,
    const layouts::RosterGroup& resolved) noexcept {
    if (resolved.slotCount > resolved.slotTypes.size()
        || actual.slotTypes.size() != resolved.slotCount
        || actual.slotFlags.size() != resolved.slotCount
        || actual.slotIndices.size() != resolved.slotCount) {
        return false;
    }
    for (std::size_t i = 0; i < resolved.slotCount; ++i) {
        if (actual.slotTypes[i] != resolved.slotTypes[i]
            || actual.slotFlags[i] != resolved.slotFlags[i]
            || actual.slotIndices[i] != resolved.slotIndices[i]) {
            return false;
        }
    }
    return true;
}

/**
 * Projects the generic round authority from one admitted profile.
 *
 * The timer asset comes from the trusted round definition. Its registry must be present exactly
 * once in the admitted top-level wire roster and match the complete catalog-resolved group;
 * companions are then resolved from that same row. No profile-local registry is treated as a
 * substitute for the actual top-level global. A missing/mismatched descriptor, duplicate
 * companion, wrong bubble, or stale clock leaves the result uncontrolled.
 */
template <class NativeDefinition, class RoundDefinition>
[[nodiscard]] wire::State project(const NativeDefinition& definition,
    const RoundDefinition& roundDefinition, const message::Roster& roster,
    const layouts::RosterGroup& resolvedTimerGroup, std::uint32_t scenario,
    std::uint8_t currentBubble, const activity_clock::Publication& clock,
    std::uint64_t endEpoch,
    bool restricted, const coo::CompletionPublication& completion = {}) noexcept {
    wire::State result{};
    const auto& timerAsset = roundDefinition.completionTimerAsset;
    if (currentBubble > wire::kMaximumBubble || definition.bubble != currentBubble
        || !wire::valid_asset(timerAsset, wire::kTimerType)
        || !clock || !activity_clock::wire::valid(clock.configuration)
        || clock.configuration.timing <= 0.0F
        || clock.domain.bubble != currentBubble || !scenario
        || scenario == UINT32_MAX || clock.domain.scenario != scenario
        || roster.groupCount > roster.groups.size()
        || roster.topLevelGroupCount == 0
        || roster.topLevelGroupCount > roster.groupCount
        || roster.bubbleSubBlocks.size() > message::kBubbleSubBlockCapacity
        || resolvedTimerGroup.registryKey != timerAsset.registry) {
        return result;
    }

    const message::Group* admitted{};
    std::size_t admittedCount{};
    for (std::size_t i = 0; i < roster.groupCount; ++i) {
        const auto& candidate = roster.groups[i];
        if (candidate.key != timerAsset.registry) continue;
        if (i >= roster.topLevelGroupCount || !matches_resolved(candidate, resolvedTimerGroup))
            return result;
        admitted = &candidate;
        ++admittedCount;
    }
    if (admittedCount != 1 || !admitted) return result;

    for (std::size_t i = roster.topLevelGroupCount; i < roster.groupCount; ++i)
        if (roster.groups[i].key == timerAsset.registry) return result;
    for (const auto& block : roster.bubbleSubBlocks) {
        if (!block.presence.empty() && block.presence.size() != block.keys.size()) return result;
        for (const auto key : block.keys)
            if (key == timerAsset.registry) return result;
    }

    std::size_t timerMatches{};
    for (std::size_t i = 0; i < resolvedTimerGroup.slotCount; ++i) {
        if (resolvedTimerGroup.slotIndices[i] != timerAsset.slot) continue;
        ++timerMatches;
        if (resolvedTimerGroup.slotTypes[i] != wire::kTimerType
            || (resolvedTimerGroup.slotFlags[i] & 2U) == 0
            || resolvedTimerGroup.authSchemas[i] != kTimerAuthSchema
            || resolvedTimerGroup.descriptorTags[i] != timerAsset.definition) return result;
    }
    if (timerMatches != 1) return result;

    auto unique_asset = [&](std::uint8_t type, std::uint32_t authSchema,
                            coo::Asset& output) noexcept {
        std::size_t matches{};
        for (std::size_t i = 0; i < resolvedTimerGroup.slotCount; ++i) {
            if (resolvedTimerGroup.slotTypes[i] != type) continue;
            ++matches;
            const coo::Asset candidate{resolvedTimerGroup.registryKey,
                resolvedTimerGroup.descriptorTags[i], type,
                resolvedTimerGroup.slotIndices[i]};
            if ((resolvedTimerGroup.slotFlags[i] & 2U) == 0
                || resolvedTimerGroup.authSchemas[i] != authSchema
                || !wire::valid_asset(candidate, type)) return false;
            output = candidate;
        }
        return matches == 1;
    };

    coo::Asset lifetime{}, director{};
    if (!unique_asset(wire::kLifetimeType, kLifetimeAuthSchema, lifetime)
        || !unique_asset(wire::kDirectorType, kDirectorAuthSchema, director)) return result;

    result.controlled = true;
    result.timer = timerAsset;
    result.lifetime = lifetime;
    result.director = director;
    result.bubble = currentBubble;
    result.restricted = restricted;
    result.endEpoch = endEpoch;
    result.completion = completion;
    return result;
}

} // namespace sunrise::server::runtime::activity::native_round_projection
