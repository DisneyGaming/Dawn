#include "service_outcome_commit.h"

#include "../../../../core/logging/log.h"
#include "../../../../state/activity/bubble_authority/runtime.h"
#include "../../../../state/activity/runtime.h"
#include "../../../../state/matchmaking/matchmaking_state.h"
#include "../../../../state/runtime/runtime.h"
#include "../internal.h"

namespace dawn::server::bap::encrypted::transactions {

/** Captures and validates any exact activity binding a successful commit will publish. */
bool prepare_publication(const ServiceOutcome& outcome, Publication& publication) noexcept {
    publication = {};
    if (const auto* allocation = transaction_if<state::activity::PendingAllocation>(outcome)) {
        if (!allocation->prepared || !static_cast<bool>(allocation->instanceKey)
            || allocation->instanceKey.sessionId != allocation->sessionId) {
            return false;
        }
        publication.activity = allocation->instanceKey;
        publication.atomicReplacement = allocation->replaces;
        publication.hasActivityBinding = true;
        publication.activityBindingCreatedByBap = true;
        return true;
    }
    const auto* plan = transaction_if<activity_message::ActivityPlan>(outcome);
    if (plan == nullptr) {
        return true;
    }
    if (!static_cast<bool>(plan->instanceKey)
        || plan->instanceKey.sessionId != plan->sessionId) {
        return false;
    }
    if (plan->mutationDomain == activity_message::MutationDomain::entitySlots
        && plan->instanceKey != plan->entitySlotMutation.instanceKey) {
        return false;
    }
    if (plan->mutationDomain == activity_message::MutationDomain::membership
        && plan->instanceKey != plan->membershipMutation.instanceKey) {
        return false;
    }
    if (plan->delivery == activity_message::Delivery::joinNotifications) {
        if (plan->mutationDomain != activity_message::MutationDomain::entitySlots) {
            return false;
        }
        publication.activity = plan->instanceKey;
        publication.hasActivityBinding = true;
        publication.activityBindingFromJoin = true;
    }
    return true;
}

/**
 * Commits at most one delayed State transaction.
 * @param outcome Checked service result whose pending transaction is used up.
 * @param publication Gets connection fields to publish after the output copy.
 * @return True when there is no transaction, or the one transaction commits.
 */
bool commit(ServiceOutcome& outcome, Publication& publication) noexcept {
    if (auto* allocation = transaction_if<state::activity::PendingAllocation>(outcome)) {
        if (!publication.hasActivityBinding || publication.activity != allocation->instanceKey
            || publication.activityBindingFromJoin
            || !publication.activityBindingCreatedByBap
            || !publication.hasRegionLineage
            || publication.regionLineage
                   != RegionLineage{allocation->instanceKey,
                                    allocation->instanceKey,
                                    RegionLineageKind::ownedActivity}
            || publication.atomicReplacement != allocation->replaces
            || !state::activity::commit(*allocation)) {
            return false;
        }
        publication.activityAllocationCommitted = true;
        return true;
    }
    if (auto* plan = transaction_if<activity_message::ActivityPlan>(outcome)) {
        if (plan->mutationDomain == activity_message::MutationDomain::entitySlots) {
            if (!state::activity::entity_slots::commit(plan->entitySlotMutation)) {
                return false;
            }
            // The keepalive only finds a link that is bound to a session. A link that allocated
            // its own session carries the same id, so this rebinds it to itself.
            if (plan->delivery == activity_message::Delivery::joinNotifications
                && (!publication.hasActivityBinding
                    || !publication.activityBindingFromJoin
                    || publication.activity != plan->instanceKey
                    || !publication.hasRegionLineage
                    || publication.regionLineage.bound != plan->instanceKey)) {
                return false;
            }
            if (plan->delivery == activity_message::Delivery::joinNotifications) {
                // The join resets the client's roster container and the grant mirror with it.
                // A kept grant leaves that container ungranted, which refuses its placed objects.
                state::activity::bubble_authority::clear_grants(plan->instanceKey);
            }
            return true;
        }
        if (plan->mutationDomain == activity_message::MutationDomain::membership) {
            return state::activity::membership::commit(plan->membershipMutation);
        }
        // The retained patch epoch is connection state, so it commits nothing here.
        return plan->mutationDomain == activity_message::MutationDomain::patchEpoch;
    }
    if (auto* mutation = transaction_if<state::matchmaking::PendingMutation>(outcome)) {
        return state::matchmaking::commit(*mutation);
    }
    if (auto* transaction = transaction_if<EquipmentSwapTransaction>(outcome)) {
        const bool committed = state::commit_equipment_swap(transaction->pending);
        core::log::write(core::log::Channel::server,
                         committed ? core::log::Level::debug : core::log::Level::warn,
                         committed ? "ev=equip stage=transaction_commit result=ok"
                                   : "ev=equip stage=transaction_commit result=fail");
        return committed;
    }
    if (auto* transaction = transaction_if<ItemAcquisitionTransaction>(outcome)) {
        const bool committed = state::commit_item_acquisition(transaction->pending);
        core::log::write(core::log::Channel::server,
                         committed ? core::log::Level::debug : core::log::Level::warn,
                         committed ? "ev=acquire stage=transaction_commit result=ok"
                                   : "ev=acquire stage=transaction_commit result=fail");
        return committed;
    }
    if (auto* transaction = transaction_if<SocketPlugTransaction>(outcome)) {
        const bool committed = state::commit_socket_plug(transaction->pending);
        core::log::write(core::log::Channel::server,
                         committed ? core::log::Level::debug : core::log::Level::warn,
                         committed ? "ev=socket_plug stage=transaction_commit result=ok"
                                   : "ev=socket_plug stage=transaction_commit result=fail");
        return committed;
    }
    if (auto* transaction = transaction_if<ItemStateTransaction>(outcome)) {
        const bool committed = state::commit_item_state(transaction->pending);
        core::log::write(core::log::Channel::server,
                         committed ? core::log::Level::debug : core::log::Level::warn,
                         committed ? "ev=item_state stage=transaction_commit result=ok"
                                   : "ev=item_state stage=transaction_commit result=fail");
        return committed;
    }
    if (auto* transaction = transaction_if<ProfileItemAcquisitionTransaction>(outcome)) {
        const bool committed = state::commit_profile_item_acquisition(transaction->pending);
        core::log::write(core::log::Channel::server,
                         committed ? core::log::Level::debug : core::log::Level::warn,
                         committed ? "ev=profile_acquire stage=transaction_commit result=ok"
                                   : "ev=profile_acquire stage=transaction_commit result=fail");
        return committed;
    }
    if (auto* transaction = transaction_if<ItemDismantleTransaction>(outcome)) {
        const bool committed = state::commit_item_dismantle(transaction->pending);
        core::log::write(core::log::Channel::server,
                         committed ? core::log::Level::debug : core::log::Level::warn,
                         committed ? "ev=dismantle stage=transaction_commit result=ok"
                                   : "ev=dismantle stage=transaction_commit result=fail");
        return committed;
    }
    return true;
}

} // namespace dawn::server::bap::encrypted::transactions
