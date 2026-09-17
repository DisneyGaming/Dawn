#include <Windows.h>

#include <cstdint>

#include "../../../runtime/storage/internal.h"
#include "../../destination/activity_destination_validation.h"
#include "../activity_membership_query.h"
#include "internal.h"

namespace dawn::state::activity::membership {

/** Prepares one exact identity for a joined activity session. */
bool prepare_identity(ActivityInstanceKey key,
                      const Identity& identity,
                      PendingMutation& mutation) noexcept {
    mutation = {};
    if (!static_cast<bool>(key)) {
        return false;
    }

    AcquireSRWLockShared(&runtime::storage::g_stateLock);
    const auto& root = runtime::storage::g_state;
    PendingMutation prepared{};
    const SessionRecord* record =
        transactions::prepare_base(root.activity, root.account.primarySoid, key, prepared);
    bool ready = record != nullptr && transactions::valid_identity(identity, record->memberKey);
    if (ready) {
        const bool changed = !record->membership.hasIdentity
                             || !transactions::equal(record->membership.identity, identity);
        if (changed
            && (root.activity.stateRevision == activity::kMaximumRevision
                || record->membership.revision == kMaximumMembershipRevision)) {
            ready = false;
        } else {
            const std::uint32_t revision =
                !changed ? record->membership.revision
                         : (record->membership.hasIdentity ? record->membership.revision + 1U
                                                           : kInitialRevision);
            prepared.snapshot = transactions::make_snapshot(record->membership, identity, revision);
            prepared.identityGuard = identity;
            prepared.kind = MutationKind::identity;
            prepared.hasSnapshot = true;
            prepared.changesState = changed;
        }
    }
    ReleaseSRWLockShared(&runtime::storage::g_stateLock);
    if (!ready) {
        return false;
    }
    mutation = prepared;
    return true;
}

/** Captures the current membership snapshot without changing stored State. */
bool prepare_refresh(ActivityInstanceKey key,
                     std::uint32_t requestedRevision,
                     std::int32_t bubbleIndex,
                     PendingMutation& mutation) noexcept {
    mutation = {};
    if (!static_cast<bool>(key)) {
        return false;
    }

    AcquireSRWLockShared(&runtime::storage::g_stateLock);
    const auto& root = runtime::storage::g_state;
    PendingMutation prepared{};
    const SessionRecord* record =
        transactions::prepare_base(root.activity, root.account.primarySoid, key, prepared);
    if (record != nullptr) {
        if (record->membership.hasIdentity) {
            prepared.snapshot = transactions::make_snapshot(
                record->membership, record->membership.identity, record->membership.revision);
            prepared.hasSnapshot = true;
        }
        prepared.requestedRevision = requestedRevision;
        prepared.bubbleIndex = bubbleIndex;
        prepared.refreshRequestGuard = transactions::refresh_guard(requestedRevision, bubbleIndex);
        prepared.kind = MutationKind::refresh;
    }
    ReleaseSRWLockShared(&runtime::storage::g_stateLock);
    if (record == nullptr) {
        return false;
    }
    mutation = prepared;
    return true;
}

/** Captures one periodic exact region refresh without publishing its optional next revision. */
bool prepare_periodic_region_refresh(ActivityInstanceKey key,
                                     std::int32_t advertisedRegion,
                                     PendingMutation& mutation,
                                     PeriodicRegionRefresh& refresh,
                                     bool forceMembershipRepublish) noexcept {
    mutation = {};
    refresh = {};
    if (!static_cast<bool>(key)) {
        return false;
    }

    PendingMutation prepared{};
    PeriodicRegionRefresh selected{};
    bool ready = false;
    AcquireSRWLockShared(&runtime::storage::g_stateLock);
    const auto& root = runtime::storage::g_state;
    const SessionRecord* record =
        transactions::prepare_base(root.activity, root.account.primarySoid, key, prepared);
    if (record != nullptr) {
        selected.inputs.bound = key;
        selected.inputs.source = key;
        selected.inputs.sourceHostRegion = prepared.expectedHostRegion;
        selected.inputs.destination = record->destination;
        selected.inputs.sourceDestination = record->destination;
        selected.inputs.grantBefore = record->bubbleAuthority;
        selected.inputs.sourceMembership = record->membership;
        selected.inputs.defaults = root.activity.defaults;
        selected.inputs.stateRevision = root.activity.stateRevision;
        selected.inputs.boundRecordRevision = record->recordRevision;
        selected.inputs.sourceRecordRevision = record->recordRevision;
        selected.regionChanged = record->membership.region.index >= 0
                                 && record->membership.region.index != advertisedRegion;
        const auto publication = transactions::periodic_publication(
            record->membership, selected.regionChanged, forceMembershipRepublish);
        selected.publishesMembership = publication.publish;
        const bool republish = publication.republish;
        ready = !republish
                || (root.activity.stateRevision != activity::kMaximumRevision
                    && record->membership.revision != kMaximumMembershipRevision);
        if (ready && republish) {
            PreparedRegionTransition transition{};
            transition.activity = key;
            transition.expectedHostRegion = prepared.expectedHostRegion;
            transition.nextHostRegion = prepared.expectedHostRegion;
            transition.before = record->membership;
            transition.after = record->membership;
            ++transition.after.revision;
            transition.after.acknowledgedRevision = kAbsentRevision;
            transition.destination = record->destination;
            transition.grantBefore = record->bubbleAuthority;
            transition.expectedStateRevision = prepared.expectedStateRevision;
            transition.expectedRecordRevision = prepared.expectedRecordRevision;
            transition.effectiveRegion = transition.after.region.index;
            transition.publishesMembership = true;
            prepared.regionTransition = transition;
            prepared.regionTransitionGuard = transition;
            prepared.snapshot = transactions::make_snapshot(
                transition.after, transition.after.identity, transition.after.revision);
            prepared.kind = MutationKind::republish;
            prepared.hasSnapshot = true;
            prepared.changesState = true;
        } else if (ready) {
            if (record->membership.hasIdentity) {
                prepared.snapshot = transactions::make_snapshot(record->membership,
                                                                record->membership.identity,
                                                                record->membership.revision);
                prepared.hasSnapshot = true;
            }
            prepared.requestedRevision = 0;
            prepared.bubbleIndex = -1;
            prepared.refreshRequestGuard = transactions::refresh_guard(0, -1);
            prepared.kind = MutationKind::refresh;
        }
    }
    ReleaseSRWLockShared(&runtime::storage::g_stateLock);
    if (!ready || !destination::valid(selected.inputs.destination)) {
        return false;
    }
    mutation = prepared;
    refresh = selected;
    return true;
}

/** Prepares an acknowledgement mark for the current membership revision. */
bool prepare_acknowledgement(ActivityInstanceKey key,
                             std::uint32_t revision,
                             PendingMutation& mutation) noexcept {
    mutation = {};
    if (!static_cast<bool>(key)) {
        return false;
    }

    AcquireSRWLockShared(&runtime::storage::g_stateLock);
    const auto& root = runtime::storage::g_state;
    PendingMutation prepared{};
    const SessionRecord* record =
        transactions::prepare_base(root.activity, root.account.primarySoid, key, prepared);
    bool ready = record != nullptr;
    if (ready) {
        const bool changed = record->membership.hasIdentity
                             && revision == record->membership.revision
                             && revision != record->membership.acknowledgedRevision;
        if (changed && root.activity.stateRevision == activity::kMaximumRevision) {
            ready = false;
        } else {
            prepared.acknowledgement = revision;
            prepared.kind = MutationKind::acknowledgement;
            prepared.changesState = changed;
        }
    }
    ReleaseSRWLockShared(&runtime::storage::g_stateLock);
    if (!ready) {
        return false;
    }
    mutation = prepared;
    return true;
}

} // namespace dawn::state::activity::membership
