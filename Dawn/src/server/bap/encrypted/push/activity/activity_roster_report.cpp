#include <array>
#include <algorithm>
#include <atomic>
#include <cstdio>

#include "../../../../../core/logging/log.h"
#include "internal.h"

namespace dawn::server::bap::encrypted::push::activity {
namespace {

/** Log names for each outcome, in the enum's own order. */
constexpr std::array<const char*, 5> kOutcomeNames = {
    "ok", "no_epoch", "no_layout", "no_groups", "encode"};

std::atomic_uint32_t g_openingLayoutReports{};

void report_opening_layout(const message::Roster& roster, std::string_view destination) noexcept {
    if (destination != "cine_110_twr" && destination != "mission_towerfall"
        && destination != "mission_scot") {
        return;
    }
    const std::uint32_t observation =
        g_openingLayoutReports.fetch_add(1, std::memory_order_relaxed) + 1U;
    if (observation > 8U) {
        return;
    }
    for (std::size_t groupIndex = 0; groupIndex < roster.groupCount; ++groupIndex) {
        std::array<char, core::log::kLineCapacity> line{};
        int used = std::snprintf(line.data(),
                                 line.size(),
                                 "ev=activity stage=roster_layout n=%u dest=%.*s group=%zu key=0x%X slots=",
                                 observation,
                                 static_cast<int>(destination.size()),
                                 destination.data(),
                                 groupIndex,
                                 roster.groups[groupIndex].key);
        for (const std::uint8_t slotType : roster.groups[groupIndex].slotTypes) {
            if (used <= 0 || static_cast<std::size_t>(used) >= line.size()) {
                break;
            }
            const int appended = std::snprintf(line.data() + used,
                                               line.size() - static_cast<std::size_t>(used),
                                               "%s%u",
                                               used > 0 && line[used - 1] != '=' ? "," : "",
                                               static_cast<unsigned>(slotType));
            if (appended <= 0) {
                break;
            }
            used += appended;
        }
        if (used > 0) {
            core::log::write(core::log::Channel::server,
                             core::log::Level::info,
                             {line.data(), (std::min)(static_cast<std::size_t>(used),
                                                      line.size() - 1)});
        }
    }
}

} // namespace

/** Reports one roster push, and only when its outcome is new. */
void report_roster_push(Session& session,
                        const message::Snapshot& snapshot,
                        std::string_view destination,
                        std::size_t bytes,
                        std::int32_t grant,
                        RosterOutcome outcome) noexcept {
    const message::Roster& roster = snapshot.roster;
    const auto reason = static_cast<std::uint8_t>(outcome) + 1U;
    // A published push reports every time. A refusal reports only when its reason is new.
    if (outcome != RosterOutcome::published && session.activity.rosterReason == reason) {
        return;
    }
    session.activity.rosterReason = static_cast<std::uint8_t>(reason);
    std::size_t slots = 0;
    for (std::size_t index = 0; index < roster.groupCount; ++index) {
        slots += roster.groups[index].slotTypes.size();
    }
    std::size_t bubbleKeys = 0;
    for (const message::BubbleSubBlock& block : roster.bubbleSubBlocks) {
        bubbleKeys += block.keys.size();
    }
    std::array<char, core::log::kLineCapacity> line{};
    const int written =
        std::snprintf(line.data(),
                      line.size(),
                      "ev=activity stage=roster result=%s soid=0x%llX foreign=%u dest=%.*s "
                       "groups=%zu top=%zu sub=%zu subkeys=%zu objects=%zu bytes=%zu state=%u "
                      "phase=%s opening_stage=%u authority_init=%u authority_preserve=%u seed=%u "
                      "keygroup=0x%X grant=%d "
                      "region=%u slice=%u spawn=0x%X director=%u director_active=%u "
                      "director_transition=%u script_flag=%u script_state=%d "
                      "join=0x%llX player=0x%llX",
                      kOutcomeNames[static_cast<std::size_t>(outcome)],
                      static_cast<unsigned long long>(session.activity.instance.sessionId),
                      session.activity.joinedForeignSession ? 1U : 0U,
                      static_cast<int>(destination.size()),
                       destination.data(),
                       roster.groupCount,
                       roster.topLevelGroupCount,
                       roster.bubbleSubBlocks.size(),
                       bubbleKeys,
                       slots,
                      bytes,
                       session.activity.rosterState,
                       snapshot.phaseOneOnly
                           ? "register"
                           : (snapshot.omegaOpeningStage
                                      == message::kOmegaForestStageTransition
                                  ? "omega_forest_transition"
                                  : (snapshot.omegaOpeningStage
                                             == message::kOmegaForestStageSettled
                                         ? "omega_forest_settled"
                                         : (snapshot.omegaOpeningStage
                                      == message::kOmegaOpeningStagePortal
                                  ? "omega_portal"
                                  : (snapshot.omegaOpeningStage
                                             == message::kOmegaOpeningStageCompleted
                                         ? "omega_complete"
                                         : (snapshot.omegaOpeningStage
                                                    == message::kOmegaOpeningStageSettled
                                                ? "omega_settled"
                                                : (snapshot.omegaOpeningStage
                                      == message::kOmegaOpeningStageTriggered
                                  ? "omega_trigger"
                                  : (snapshot.omegaOpeningStage
                                             == message::kOmegaOpeningStageScene
                                         ? "omega_scene"
                                         : (snapshot.omegaOpeningStage
                                                    == message::kOmegaOpeningStageReady
                                                ? "omega_ready"
                                                : (snapshot.preserveMissionAuthorityState
                                                       ? "preserve"
                                                       : "seed"))))))))),
                       static_cast<unsigned>(snapshot.omegaOpeningStage),
                       snapshot.initializeMissionAuthorityRuntime ? 1U : 0U,
                       snapshot.preserveMissionAuthorityState ? 1U : 0U,
                       snapshot.seedAuthoredSensors ? 1U : 0U,
                      roster.playerKeyGroup,
                      grant,
                      snapshot.region,
                      snapshot.spawnSliceSet,
                      snapshot.spawnSetHash,
                      static_cast<unsigned>(snapshot.missionDirectorVariant),
                      snapshot.missionDirectorActive ? 1U : 0U,
                      snapshot.missionDirectorTransition ? 1U : 0U,
                      snapshot.activityScriptFlag ? 1U : 0U,
                      snapshot.activityScriptState,
                      static_cast<unsigned long long>(session.activity.characterSoid),
                      static_cast<unsigned long long>(snapshot.playerKey));
    if (written > 0) {
        core::log::write(core::log::Channel::server,
                         outcome == RosterOutcome::published ? core::log::Level::debug
                                                             : core::log::Level::warn,
                         {line.data(), static_cast<std::size_t>(written)});
    }
    if (outcome == RosterOutcome::published) {
        report_opening_layout(roster, destination);
    }
}

} // namespace dawn::server::bap::encrypted::push::activity
