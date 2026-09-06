#include <Windows.h>

#include "../../../runtime/storage/internal.h"
#include "../activity_membership_query.h"
#include "internal.h"

namespace sunrise::state::activity::membership {

/** Prepares sparse host-state changes for one joined activity session. */
bool prepare_authoritative(ActivityInstanceKey key,
                           const AuthoritativeUpdate& update,
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
        const MembershipState merged = transactions::merge(record->membership, update);
        const bool changed = !transactions::equal_authoritative(record->membership, merged);
        const bool movesRegion = transactions::moves_region(record->membership, merged);
        const bool publishes = changed || movesRegion;
        const bool revisionExhausted =
            root.activity.stateRevision == activity::kMaximumRevision
            || (record->membership.hasIdentity
                && record->membership.revision == kMaximumMembershipRevision);
        const bool regionGenerationExhausted =
            movesRegion
            && (record->lifecycle.hostRegionExhausted
                || record->lifecycle.hostRegion.value == activity::kMaximumGeneration);
        if ((publishes && revisionExhausted) || regionGenerationExhausted) {
            ready = false;
        } else {
            PreparedRegionTransition transition{};
            transition.activity = key;
            transition.expectedHostRegion = prepared.expectedHostRegion;
            transition.nextHostRegion = prepared.expectedHostRegion;
            transition.before = record->membership;
            transition.after = merged;
            transition.destination = record->destination;
            transition.grantBefore = record->bubbleAuthority;
            transition.expectedStateRevision = prepared.expectedStateRevision;
            transition.expectedRecordRevision = prepared.expectedRecordRevision;
            transition.effectiveRegion = merged.region.index;
            transition.movesRegion = movesRegion;
            transition.publishesMembership = publishes && record->membership.hasIdentity;
            if (movesRegion) {
                HostRegionGeneration next = prepared.expectedHostRegion.generation;
                bool exhausted = false;
                if (!activity::advance(next, exhausted)) {
                    ready = false;
                } else {
                    transition.nextHostRegion = {key, next};
                }
            }
            if (publishes && transition.after.hasIdentity) {
                ++transition.after.revision;
                transition.after.acknowledgedRevision = kAbsentRevision;
            }
            prepared.authoritativeInput = update;
            prepared.authoritativeGuard = update;
            prepared.kind = MutationKind::authoritative;
            prepared.changesState = changed;
            prepared.movesRegion = movesRegion;
            prepared.movesTransitionToken =
                transactions::moves_transition_token(record->membership, merged);
            prepared.hasSnapshot = transition.publishesMembership;
            if (prepared.hasSnapshot) {
                prepared.snapshot = transactions::make_snapshot(
                    transition.after,
                    transition.after.identity,
                    transition.after.revision);
            }
            prepared.regionTransition = transition;
            prepared.regionTransitionGuard = transition;
        }
    }
    ReleaseSRWLockShared(&runtime::storage::g_stateLock);
    if (!ready) {
        return false;
    }
    mutation = prepared;
    return true;
}

} // namespace sunrise::state::activity::membership
