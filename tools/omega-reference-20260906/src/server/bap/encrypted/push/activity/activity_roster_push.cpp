#include "activity_roster_push.h"

#include <Windows.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <string_view>

#include "../../../../../core/logging/log.h"
#include "../../../../../middleware/bap/activity_message/sensor_auth_update.h"
#include "../../../../../middleware/secure_channel/runtime.h"
#include "../../../../../state/activity/bubble_authority/runtime.h"
#include "../../diagnostics/omega_trace.h"
#include "activity_notification_frame.h"
#include "internal.h"

namespace sunrise::server::bap::encrypted::push::activity {
namespace {

namespace message = middleware::bap::activity_message::sensor_auth_update;

/** No bubble was granted with this body. */
constexpr std::int32_t kNoGrant = -1;
/** The destination name a refusal reports. The selection field is 40 bytes wide. */
constexpr std::size_t kDestinationCapacity = 40;

} // namespace

/** Appends one `sensor_auth_update` svc9 notification carrying the destination's roster. */
bool append_roster_notification(Session& session,
                                Scratch& scratch,
                                std::span<const std::byte, state::kAesKeySize> key,
                                std::array<std::byte, state::kBapNonceSize>& nonce,
                                std::span<std::byte> response,
                                std::size_t& written,
                                bool burst) noexcept {
    if (written > response.size() || !lifecycle::activity_binding_is_current(session)
        || !state::activity::contains(session.activity.instance)
        || session.activity.rosterStaged.staged) {
        return false;
    }
    state::activity::PublicationGeneration publication{};
    if (!lifecycle::stage_roster_publication_generation(session.activity, publication)) {
        return false;
    }
    const std::uint32_t initialRosterGroups = session.activity.rosterGroups;
    const std::uint8_t initialRosterSends = session.activity.rosterSends;
    const std::uint8_t initialRosterState = session.activity.rosterState;
    const std::uint8_t initialOmegaOpeningStage = session.activity.omegaOpeningStage;
    const std::uint16_t initialDirectorSends = session.activity.directorSends;
    const bool initialMissionDirectorActive = session.activity.missionDirectorActive;
    message::Snapshot snapshot{};
    std::array<char, kDestinationCapacity> destination{};
    std::size_t destinationLength = 0;
    RosterOutcome outcome = RosterOutcome::noEpoch;
    if (session.activityPatchEpochSeen) {
        outcome = build_roster_snapshot(
            session, scratch, snapshot, destination, destinationLength, burst);
    }
    const std::string_view name(destination.data(), destinationLength);
    if (outcome != RosterOutcome::published) {
        session.activity.rosterGroups = initialRosterGroups;
        session.activity.rosterSends = initialRosterSends;
        session.activity.rosterState = initialRosterState;
        session.activity.omegaOpeningStage = initialOmegaOpeningStage;
        session.activity.directorSends = initialDirectorSends;
        session.activity.missionDirectorActive = initialMissionDirectorActive;
        report_roster_push(session, snapshot, name, 0, kNoGrant, outcome);
        return false;
    }

    // The grant is picked here and committed only once the frame reaches the caller, so a
    // discarded body leaves the bubble ungranted and the next push retries it.
    state::activity::bubble_authority::Grant grant{};
    // The grant follows the player, not the destination. The client names the region it is in
    // and that moves as it walks between bubbles, so granting the arrival bubble again would leave
    // the bubble the player actually entered without authority.
    // The wire field is unsigned. A value past the signed range turns negative and the selector
    // rejects it, the same answer as its own upper bound.
    if (state::activity::bubble_authority::select_grant(
            session.activity.instance, static_cast<std::int32_t>(snapshot.region), grant)) {
        snapshot.hasGrant = true;
        snapshot.grant.bubble = grant.bubble;
        snapshot.grant.token = grant.token;
    }

    const std::size_t initialWritten = written;
    auto initialNonce = nonce;
    std::size_t messageSize = 0;
    bool encoded = message::encode_sensor_auth_update(snapshot, scratch.responseBody, messageSize)
                   && append_notification_frame(scratch,
                                                session.activity.instance.sessionId,
                                                message::kMessageType,
                                                std::span(scratch.responseBody).first(messageSize),
                                                key,
                                                nonce,
                                                response,
                                                written);
    if (encoded) {
        middleware::secure_channel::advance_nonce(nonce);
        // Staged, not published. The grant and the counters are one-way and this body may still be
        // discarded, so they are held here and settled by `commit_staged_roster` or
        // `discard_staged_roster`.
        session.activity.rosterStaged.binding = session.activity.key;
        session.activity.rosterStaged.activity = session.activity.instance;
        session.activity.rosterStaged.publication = publication;
        session.activity.rosterStaged.grant = grant;
        session.activity.rosterStaged.priorGroups = initialRosterGroups;
        session.activity.rosterStaged.priorSends = initialRosterSends;
        session.activity.rosterStaged.priorState = initialRosterState;
        session.activity.rosterStaged.priorOmegaOpeningStage = initialOmegaOpeningStage;
        session.activity.rosterStaged.priorDirectorSends = initialDirectorSends;
        session.activity.rosterStaged.priorMissionDirectorActive =
            initialMissionDirectorActive;
        session.activity.rosterStaged.omegaOpeningStage =
            session.activity.omegaOpeningStage != initialOmegaOpeningStage
                ? session.activity.omegaOpeningStage
                : message::kOmegaOpeningStageNone;
        session.activity.rosterStaged.omegaOpeningScriptState =
            static_cast<std::uint8_t>(snapshot.activityScriptState);
        session.activity.rosterStaged.hasGrant = snapshot.hasGrant;
        if (name == "mission_scot") {
            session.activity.rosterStaged.omegaTracePublicationId =
                diagnostics::omega_trace::record_auth_staged(
                    session.activity.instance.sessionId,
                    snapshot,
                    std::span(scratch.responseBody).first(messageSize));
        }
        session.activity.rosterStaged.staged = true;
        session.activity.rosterPublicationClock = publication;
    }
    report_roster_push(session,
                       snapshot,
                       name,
                       encoded ? messageSize : 0,
                       encoded && snapshot.hasGrant ? snapshot.grant.bubble : kNoGrant,
                       encoded ? RosterOutcome::published : RosterOutcome::encodeFailed);
    SecureZeroMemory(scratch.responseBody.data(), messageSize);
    if (!encoded) {
        if (written > initialWritten) {
            SecureZeroMemory(response.data() + initialWritten, written - initialWritten);
        }
        written = initialWritten;
        nonce = initialNonce;
        session.activity.rosterGroups = initialRosterGroups;
        session.activity.rosterSends = initialRosterSends;
        session.activity.rosterState = initialRosterState;
        session.activity.omegaOpeningStage = initialOmegaOpeningStage;
        session.activity.directorSends = initialDirectorSends;
        session.activity.missionDirectorActive = initialMissionDirectorActive;
    }
    SecureZeroMemory(&initialNonce, sizeof initialNonce);
    return encoded;
}

/** Appends one immutable coordinator-owned roster snapshot without advancing live delivery. */
bool append_roster_notification(
    Session& session,
    Scratch& scratch,
    const RegionTransitionSnapshot& transition,
    std::span<const std::byte, state::kAesKeySize> key,
    std::array<std::byte, state::kBapNonceSize>& nonce,
    std::span<std::byte> response,
    std::size_t& written) noexcept {
    if (written > response.size() || !lifecycle::activity_binding_is_current(session)
        || session.activity.key != transition.binding
        || session.activity.instance != transition.activity
        || !static_cast<bool>(transition.rosterPublication)
        || session.activity.rosterStaged.staged) {
        return false;
    }
    const std::size_t initialWritten = written;
    const auto initialNonce = nonce;
    std::size_t messageSize = 0;
    const bool encoded = message::encode_sensor_auth_update(
                             transition.rosterWire,
                             scratch.responseBody,
                             messageSize)
                         && append_notification_frame(
                             scratch,
                             transition.activity.sessionId,
                             message::kMessageType,
                             std::span(scratch.responseBody).first(messageSize),
                             key,
                             nonce,
                             response,
                             written);
    if (encoded) {
        middleware::secure_channel::advance_nonce(nonce);
        RosterPublication staged{};
        staged.binding = transition.binding;
        staged.activity = transition.activity;
        staged.sourceHostRegion = transition.sourceHostRegion;
        staged.regionIndex = transition.regionIndex;
        staged.publication = transition.rosterPublication;
        staged.grant = transition.grantCandidate;
        staged.priorGroups = transition.before.groups;
        staged.priorSends = transition.before.sends;
        staged.priorState = transition.before.state;
        staged.priorOmegaOpeningStage = transition.before.omegaOpeningStage;
        staged.priorDirectorSends = transition.before.directorSends;
        staged.priorMissionDirectorActive = transition.before.missionDirectorActive;
        staged.afterGroups = transition.after.groups;
        staged.afterSends = transition.after.sends;
        staged.afterState = transition.after.state;
        staged.afterOmegaOpeningStage = transition.after.omegaOpeningStage;
        staged.afterDirectorSends = transition.after.directorSends;
        staged.afterMissionDirectorActive = transition.after.missionDirectorActive;
        staged.omegaOpeningStage =
            transition.after.omegaOpeningStage != transition.before.omegaOpeningStage
                ? transition.after.omegaOpeningStage
                : message::kOmegaOpeningStageNone;
        staged.omegaOpeningScriptState =
            static_cast<std::uint8_t>(transition.rosterWire.activityScriptState);
        staged.hasGrant = transition.rosterWire.hasGrant;
        staged.hasAfter = true;
        staged.staged = true;
        session.activity.rosterStaged = staged;
    } else {
        if (written > initialWritten) {
            SecureZeroMemory(response.data() + initialWritten, written - initialWritten);
        }
        written = initialWritten;
        nonce = initialNonce;
    }
    SecureZeroMemory(scratch.responseBody.data(), messageSize);
    return encoded;
}

/** Settles a staged roster body that reached the caller. */
void commit_staged_roster(Session& session) noexcept {
    if (!session.activity.rosterStaged.staged) {
        return;
    }
    if (!lifecycle::staged_roster_is_current(session.activity)) {
        session.activity.rosterStaged = {};
        return;
    }
    if (session.activity.rosterStaged.hasGrant) {
        state::activity::bubble_authority::record_grant(session.activity.rosterStaged.activity,
                                                        session.activity.rosterStaged.grant);
    }
    const std::uint8_t omegaScriptState =
        session.activity.rosterStaged.omegaOpeningScriptState;
    const std::uint8_t omegaStage =
        session.activity.rosterStaged.omegaOpeningStage;
    diagnostics::omega_trace::record_auth_delivery(
        session.activity.rosterStaged.activity.sessionId,
        session.activity.rosterStaged.omegaTracePublicationId,
        omegaScriptState,
        true);
    if (omegaStage == message::kOmegaOpeningStageReady) {
        session.activity.sensorObservation.omegaOpeningReadyAuthorityPublished = true;
        // The Scene handoff can arrive before this prerequisite frame settles. If it already did,
        // wake state 2 now that the delivered order is guaranteed.
        if (session.activity.sensorObservation.omegaSceneHandoffArmed) {
            session.activity.keepaliveDueTick = 0;
        }
    } else if (omegaStage == message::kOmegaOpeningStageTriggered) {
        session.activity.sensorObservation.omegaOpeningAuthorityPublished = true;
    } else if (omegaStage == message::kOmegaForestStageTransition) {
        session.activity.sensorObservation.omegaForestEntranceAuthorityPublished = true;
    }
    if (omegaStage >= message::kOmegaOpeningStageBaseline
        && omegaStage <= message::kOmegaForestStageSettled) {
        std::array<char, 256> transitionLine{};
        const int transitionWritten = std::snprintf(
            transitionLine.data(),
            transitionLine.size(),
            "ev=activity stage=omega_opening_authority result=published opening_stage=%u "
            "script_state=%u scene_selector=0x80EC0F96 direct_spawner=0 "
            "portal_components=%s",
            static_cast<unsigned>(omegaStage),
            static_cast<unsigned>(omegaScriptState),
            (omegaStage == message::kOmegaOpeningStageTriggered
             || omegaStage == message::kOmegaOpeningStagePortal)
                ? "BA5F26EF/4/0,23/1;D00142CF/4/2-4,23/16,70/17,30/20,24"
                : "none");
        if (transitionWritten > 0) {
            core::log::write(core::log::Channel::server,
                             core::log::Level::info,
                             {transitionLine.data(),
                              static_cast<std::size_t>(transitionWritten)});
        }
    }
    if (session.activity.rosterStaged.hasAfter) {
        session.activity.rosterGroups = session.activity.rosterStaged.afterGroups;
        session.activity.rosterSends = session.activity.rosterStaged.afterSends;
        session.activity.rosterState = session.activity.rosterStaged.afterState;
        session.activity.omegaOpeningStage =
            session.activity.rosterStaged.afterOmegaOpeningStage;
        session.activity.directorSends = session.activity.rosterStaged.afterDirectorSends;
        session.activity.missionDirectorActive =
            session.activity.rosterStaged.afterMissionDirectorActive;
        session.activity.rosterPublicationClock =
            session.activity.rosterStaged.publication;
    }
    session.activity.rosterStaged = {};
}

/** Puts back what a staged roster body advanced, now that the body has been discarded. */
void discard_staged_roster(Session& session) noexcept {
    if (!session.activity.rosterStaged.staged) {
        return;
    }
    if (!lifecycle::staged_roster_is_current(session.activity)) {
        session.activity.rosterStaged = {};
        return;
    }
    if (session.activity.rosterStaged.hasAfter) {
        diagnostics::omega_trace::record_auth_delivery(
            session.activity.rosterStaged.activity.sessionId,
            session.activity.rosterStaged.omegaTracePublicationId,
            session.activity.rosterStaged.omegaOpeningScriptState,
            false);
        session.activity.rosterStaged = {};
        return;
    }
    // The client never saw this body, so its state byte must not be spent. The next push has to
    // move the byte again or the client does not rebuild its roster objects.
    session.activity.rosterGroups = session.activity.rosterStaged.priorGroups;
    session.activity.rosterSends = session.activity.rosterStaged.priorSends;
    session.activity.rosterState = session.activity.rosterStaged.priorState;
    session.activity.omegaOpeningStage =
        session.activity.rosterStaged.priorOmegaOpeningStage;
    session.activity.directorSends = session.activity.rosterStaged.priorDirectorSends;
    session.activity.missionDirectorActive =
        session.activity.rosterStaged.priorMissionDirectorActive;
    diagnostics::omega_trace::record_auth_delivery(
        session.activity.rosterStaged.activity.sessionId,
        session.activity.rosterStaged.omegaTracePublicationId,
        session.activity.rosterStaged.omegaOpeningScriptState,
        false);
    session.activity.rosterStaged = {};
}

} // namespace sunrise::server::bap::encrypted::push::activity
