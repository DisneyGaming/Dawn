#include "activity_transaction_notifications.h"

#include <Windows.h>

#include <algorithm>

#include "../push/activity/activity_global_state_push.h"
#include "../push/activity/activity_membership_push.h"
#include "../push/activity/activity_message_push.h"
#include "../push/activity/activity_roster_push.h"

namespace dawn::server::bap::encrypted::activity_transaction {
namespace {

#if defined(DAWN_REGION_PUBLICATION_TESTING)
RegionStageFailurePoint g_regionStageFailure = RegionStageFailurePoint::none;

[[nodiscard]] bool injected_after(RegionStageFailurePoint point) noexcept {
    return g_regionStageFailure == point;
}
#endif

/** Wipes and rewinds every frame appended after one bundle boundary. */
void rollback_bundle(Session& session,
                     const ActivityBindingState& bindingBefore,
                     std::array<std::byte, state::kBapNonceSize>& nonce,
                     const std::array<std::byte, state::kBapNonceSize>& nonceBefore,
                     std::span<std::byte> response,
                     std::size_t writtenBefore,
                     std::size_t& written,
                     NotificationStaging& staging) noexcept {
    if (written > writtenBefore && writtenBefore <= response.size()) {
        const std::size_t appended = (std::min)(written - writtenBefore,
                                                response.size() - writtenBefore);
        SecureZeroMemory(response.data() + writtenBefore, appended);
    }
    written = writtenBefore;
    nonce = nonceBefore;
    session.activity = bindingBefore;
    gameplay::group::release_host_activity_lineage(staging.advertisementLease);
    gameplay::group::release_host_activity_lineage(staging.boundLineageLease);
    staging = {};
    staging.result = NotificationStageResult::failed;
}

/** Encodes every required bit of one immutable region snapshot in protocol order. */
[[nodiscard]] bool append_region_bundle(
    Session& session,
    Scratch& scratch,
    const RegionTransitionSnapshot& snapshot,
    std::span<const std::byte, state::kAesKeySize> key,
    std::array<std::byte, state::kBapNonceSize>& nonce,
    std::span<std::byte> response,
    std::size_t& written) noexcept {
    if (requires_notification(snapshot.required, RegionNotification::globalState)
        && !push::activity::append_global_state_notification(
            scratch, snapshot.activity, key, nonce, response, written)) {
        return false;
    }
#if defined(DAWN_REGION_PUBLICATION_TESTING)
    if (requires_notification(snapshot.required, RegionNotification::globalState)
        && injected_after(RegionStageFailurePoint::afterGlobalState)) {
        return false;
    }
#endif
    if (requires_notification(snapshot.required, RegionNotification::membership)
        && !push::activity::append_membership_notification(
            scratch, snapshot, key, nonce, response, written)) {
        return false;
    }
#if defined(DAWN_REGION_PUBLICATION_TESTING)
    if (requires_notification(snapshot.required, RegionNotification::membership)
        && injected_after(RegionStageFailurePoint::afterMembership)) {
        return false;
    }
#endif
    if (requires_notification(snapshot.required, RegionNotification::roster)
        && !push::activity::append_roster_notification(
            session, scratch, snapshot, key, nonce, response, written)) {
        return false;
    }
#if defined(DAWN_REGION_PUBLICATION_TESTING)
    if (requires_notification(snapshot.required, RegionNotification::roster)
        && injected_after(RegionStageFailurePoint::afterRoster)) {
        return false;
    }
#endif
    return true;
}

} // namespace

#if defined(DAWN_REGION_PUBLICATION_TESTING)
/** Selects one deterministic boundary failure in the real coordinator. */
void set_region_stage_failure_for_testing(RegionStageFailurePoint point) noexcept {
    g_regionStageFailure = point;
}
#endif

/** Stages exactly one complete bundle, one exact debt, or nothing. */
NotificationStageResult stage_notifications(
    Session& session,
    Scratch& scratch,
    const activity_message::ActivityPlan& activity,
    std::span<const std::byte, state::kAesKeySize> key,
    std::array<std::byte, state::kBapNonceSize>& nonce,
    std::span<std::byte> response,
    std::size_t& written,
    NotificationStaging& staging) noexcept {
    staging = {};
    const std::size_t writtenBefore = written;
    const auto nonceBefore = nonce;
    const ActivityBindingState bindingBefore = session.activity;

    bool complete = false;
    if (activity.delivery == activity_message::Delivery::joinNotifications) {
        complete = push::activity::append_join_notifications(
            scratch, activity, key, nonce, response, written);
    } else if (activity.delivery == activity_message::Delivery::entitySlotNotification) {
        complete = push::activity::append_entity_slot_notification(scratch,
                                                                   activity.sessionId,
                                                                   activity.entitySlotMutation.mask,
                                                                   key,
                                                                   nonce,
                                                                   response,
                                                                   written);
    } else if (activity.delivery == activity_message::Delivery::membershipNotification) {
        complete = push::activity::append_membership_notification(
            scratch, activity, key, nonce, response, written);
    } else if (activity.delivery == activity_message::Delivery::refreshNotifications
               || activity.delivery
                      == activity_message::Delivery::authoritativeNotifications) {
        staging.regionBundle = true;
        const push::activity::RegionSnapshotBuildResult built =
            push::activity::build_region_transition_snapshot(session,
                                                             scratch,
                                                             activity,
                                                             staging.snapshot,
                                                             staging.debt,
                                                             staging.advertisementLease,
                                                             staging.boundLineageLease);
        if (built == push::activity::RegionSnapshotBuildResult::pending) {
            staging.result = NotificationStageResult::deferred;
            return staging.result;
        }
        complete = built == push::activity::RegionSnapshotBuildResult::ready
                   && append_region_bundle(session,
                                           scratch,
                                           staging.snapshot,
                                           key,
                                           nonce,
                                           response,
                                           written);
    } else {
        complete = activity.delivery == activity_message::Delivery::none;
    }

    if (!complete) {
        rollback_bundle(session,
                        bindingBefore,
                        nonce,
                        nonceBefore,
                        response,
                        writtenBefore,
                        written,
                        staging);
        return NotificationStageResult::failed;
    }
    staging.result = NotificationStageResult::complete;
    return staging.result;
}

/** Stages one periodic immutable region bundle without changing State or live delivery. */
NotificationStageResult stage_periodic_notifications(
    Session& session,
    Scratch& scratch,
    const state::activity::membership::PeriodicRegionRefresh& refresh,
    const state::activity::membership::PendingMutation& mutation,
    bool burst,
    bool rearmMissionDirector,
    std::span<const std::byte, state::kAesKeySize> key,
    std::array<std::byte, state::kBapNonceSize>& nonce,
    std::span<std::byte> response,
    std::size_t& written,
    NotificationStaging& staging) noexcept {
    staging = {};
    const std::size_t writtenBefore = written;
    const auto nonceBefore = nonce;
    const ActivityBindingState bindingBefore = session.activity;
    staging.regionBundle = true;
    const push::activity::RegionSnapshotBuildResult built =
        push::activity::build_periodic_region_snapshot(session,
                                                       scratch,
                                                       refresh,
                                                       mutation,
                                                       burst,
                                                       rearmMissionDirector,
                                                       staging.snapshot,
                                                       staging.advertisementLease,
                                                       staging.boundLineageLease);
    if (built == push::activity::RegionSnapshotBuildResult::pending) {
        gameplay::group::release_host_activity_lineage(staging.advertisementLease);
        gameplay::group::release_host_activity_lineage(staging.boundLineageLease);
        staging = {};
        staging.result = NotificationStageResult::deferred;
        return staging.result;
    }
    const bool complete = built == push::activity::RegionSnapshotBuildResult::ready
                          && append_region_bundle(session,
                                                  scratch,
                                                  staging.snapshot,
                                                  key,
                                                  nonce,
                                                  response,
                                                  written);
    if (!complete) {
        rollback_bundle(session,
                        bindingBefore,
                        nonce,
                        nonceBefore,
                        response,
                        writtenBefore,
                        written,
                        staging);
        return NotificationStageResult::failed;
    }
    staging.result = NotificationStageResult::complete;
    return staging.result;
}

/** Discards any staged delivery candidate and releases its exact advertisement pin. */
void discard_notification_staging(Session& session, NotificationStaging& staging) noexcept {
    push::activity::discard_staged_roster(session);
    gameplay::group::release_host_activity_lineage(staging.advertisementLease);
    gameplay::group::release_host_activity_lineage(staging.boundLineageLease);
    staging = {};
}

} // namespace dawn::server::bap::encrypted::activity_transaction
