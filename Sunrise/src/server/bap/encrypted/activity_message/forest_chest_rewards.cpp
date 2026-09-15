#include "forest_chest_rewards.h"
#include "forest_cache_policy.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <string_view>

#include "../../../../core/logging/log.h"
#include "../../../../state/activity/destination/activity_destination_snapshot.h"
#include "../../../../state/activity/runtime.h"
#include "../../../../state/build_data/collectibles/collectible_catalog.h"
#include "../../../../state/runtime/runtime.h"
#include "../../../runtime/activity/native_activity_runtime.h"
#include "../../../web_service/forest_loot_pickups.h"
#include "../queuez/queuez_outcome_staging.h"
#include "../queuez/queuez_state_validation.h"

namespace sunrise::server::bap::encrypted::forest_chest_rewards {
namespace {

namespace runtime_activity = ::sunrise::server::runtime::activity;
namespace native_activity = runtime_activity::native_activity;
namespace placement_wire = runtime_activity::placement::wire;
namespace timed_round = runtime_activity::timed_round;

/**
 * One owed end-of-run chest payout. The activity key, the round population boot and the account
 * SOID are the whole dedupe identity: the chest pays once per run, and the trigger can fire more
 * than once while the player stands in the volume.
 */
struct Claim final {
    state::activity::ActivityInstanceKey activity{};
    std::uint64_t accountSoid{};
    std::uint64_t boot{};
    std::uint64_t branches{};
    std::int32_t coins{};
    std::int32_t remaining{};
    std::uint64_t retryAt{};
    bool occupied{};
    bool delivered{};
    std::uint32_t rewardHash{kChocolateStrangeCoinHash};
};

/** BAP's route lock also serializes deferred account-subscriber polling. */
std::array<Claim, 32> g_claims{};

/**
 * Milliseconds between two attempts at the same owed unit.
 * [owner] Mirrors the festival pickup ring's backoff so one stuck subscriber cannot spin the
 * deferred push loop. Not an authored value.
 */
constexpr std::uint64_t kRetryBackoffMilliseconds = 1000U;

void report(const char* stage, const Claim& claim) noexcept {
    std::array<char, core::log::kLineCapacity> line{};
    const int count = std::snprintf(
        line.data(),
        line.size(),
        "ev=forest_chest stage=%s owner=%016llX incarnation=%llu boot=%016llX branches=%llu "
        "coins=%d remaining=%d item=0x%08X",
        stage,
        static_cast<unsigned long long>(claim.activity.sessionId),
        static_cast<unsigned long long>(claim.activity.incarnation.value),
        static_cast<unsigned long long>(claim.boot),
        static_cast<unsigned long long>(claim.branches),
        claim.coins,
        claim.remaining,
        claim.rewardHash);
    if (count > 0 && static_cast<std::size_t>(count) < line.size()) {
        core::log::write(core::log::Channel::server,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(count)});
    }
}

/**
 * One line for a Haunted Forest type-31 receipt on a slot that is not pt_reward_chest. The reward
 * room authors pt_reward_space (102) and pt_reward_chest_alt (105) beside ours, so the first live
 * run must be able to read which authored volume the client actually reported. Never pays.
 */
void report_foreign(state::activity::ActivityInstanceKey owner,
                    const state::activity::coo::native_player_trigger::Receipt& receipt) noexcept {
    std::array<char, core::log::kLineCapacity> line{};
    const int count = std::snprintf(
        line.data(),
        line.size(),
        "ev=forest_chest stage=observed reason=other_trigger owner=%016llX incarnation=%llu "
        "registry=0x%08X slot=%d object=0x%08X",
        static_cast<unsigned long long>(owner.sessionId),
        static_cast<unsigned long long>(owner.incarnation.value),
        receipt.registry,
        static_cast<int>(receipt.slot),
        receipt.object);
    if (count > 0 && static_cast<std::size_t>(count) < line.size()) {
        core::log::write(core::log::Channel::server,
                         core::log::Level::debug,
                         {line.data(), static_cast<std::size_t>(count)});
    }
}

/** One line for every receipt that was ours by slot but did not qualify to pay. */
void report_skip(const char* reason,
                 state::activity::ActivityInstanceKey owner,
                 const state::activity::coo::native_player_trigger::Receipt& receipt) noexcept {
    std::array<char, core::log::kLineCapacity> line{};
    const int count = std::snprintf(
        line.data(),
        line.size(),
        "ev=forest_chest stage=skipped reason=%s owner=%016llX incarnation=%llu registry=0x%08X "
        "slot=%d object=0x%08X",
        reason,
        static_cast<unsigned long long>(owner.sessionId),
        static_cast<unsigned long long>(owner.incarnation.value),
        receipt.registry,
        static_cast<int>(receipt.slot),
        receipt.object);
    if (count > 0 && static_cast<std::size_t>(count) < line.size()) {
        core::log::write(core::log::Channel::server,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(count)});
    }
}

[[nodiscard]] bool forest_destination(state::activity::ActivityInstanceKey activity) noexcept {
    constexpr std::string_view forest = web_service::forest_loot::kForestPackageName;
    state::activity::destination::DestinationSelection destination{};
    if (!state::activity::destination::snapshot(activity, destination)
        || destination.packageNameLength != forest.size()) {
        return false;
    }
    return std::equal(forest.begin(), forest.end(), destination.packageName.begin());
}

/**
 * True only while the authored chest placement is live. The Forest host sets the reward placements
 * at Operation::retireEncounter in the rewards phase, so a placed chest is itself a phase receipt.
 */
[[nodiscard]] bool chest_placed(state::activity::ActivityInstanceKey owner, std::uint32_t& generation) noexcept {
    const runtime_activity::NativeActivityDefinition* definition = nullptr;
    placement_wire::Batch batch{};
    if (!native_activity::snapshot_placements(owner, definition, batch) || definition == nullptr) {
        return false;
    }
    const auto* request=placement_wire::find(
        batch, kForestRegistryKey, kPlacementSlotType, kRewardChestPlacementSlot);
    if (!request || !request->active || !request->generation) return false;
    generation=request->generation;
    return true;
}

} // namespace

void receive(const Session& session,
             state::activity::ActivityInstanceKey owner,
             const state::activity::coo::native_player_trigger::Receipt& receipt) noexcept {
    // Every other authored type-31 trigger in every activity arrives here. Only the Forest
    // registry is ours; another activity's trigger leaves no trace at all.
    if (receipt.registry != kForestRegistryKey) {
        return;
    }
    if (receipt.slot != kRewardChestTriggerSlot
        || receipt.object == 0U) {
        report_foreign(owner, receipt);
        return;
    }

    // The family-4 subscriber is deliberately NOT required here, exactly as festival_pickups does
    // it: the chest trigger is a one-shot edge, so a claim qualified while the account family is
    // between subscribe and repush must still be owed. consume() refuses to pay until a live
    // subscriber whose root SOID matches the claim asks for it.
    if (!session.authenticated || !lifecycle::activity_binding_is_current(session)) {
        report_skip("session", owner, receipt);
        return;
    }
    if (!static_cast<bool>(owner) || !state::activity::contains(owner)
        || session.activity.instance != owner) {
        report_skip("not_live", owner, receipt);
        return;
    }

    const state::AccountState account = state::account_snapshot();
    if (account.primarySoid == 0U) {
        report_skip("account", owner, receipt);
        return;
    }

    if (!forest_destination(owner)) {
        report_skip("not_forest", owner, receipt);
        return;
    }
    std::uint32_t chestGeneration{};
    if (!chest_placed(owner,chestGeneration)) {
        report_skip("chest_unplaced", owner, receipt);
        return;
    }

    auto phase = timed_round::Phase::entry;
    std::uint64_t completedRounds = 0U;
    std::uint64_t boot = 0U;
    if (!native_activity::round_progress(owner, phase, completedRounds, boot)) {
        report_skip("no_rounds", owner, receipt);
        return;
    }
    if (phase != timed_round::Phase::rewards && phase != timed_round::Phase::complete) {
        report_skip("not_rewards", owner, receipt);
        return;
    }

    const auto reward=payout(completedRounds);
    const auto coins=reward.coins;

    if (!kGrantOnReach) {
        // One bundle per currency. The native adapter holds these until this exact chest's
        // lid reaches its open pose; quantities remain authorized by the host pickup ledger.
        static state::activity::ActivityInstanceKey lastOwner{};
        static std::uint64_t lastBoot{};
        const bool repeat = lastOwner == owner && lastBoot == boot;
        if (repeat) return;
        if (!repeat) {
            lastOwner = owner;
            lastBoot = boot;
            if (coins>0) {
                server::web_service::forest_loot::queue_chest_drops(
                    1, kRewardChestX, kRewardChestY, kRewardChestZ,
                    server::web_service::forest_loot::kChocolateStrangeCoinHash,coins,owner,chestGeneration);
                server::web_service::forest_loot::queue_chest_drops(
                    1,kRewardChestX,kRewardChestY,kRewardChestZ,
                    server::web_service::forest_loot::kCandyDefinitionHash,reward.candy,owner,chestGeneration);
            }
            // Provisional 10% bonus mask roll. Currency is always in world drops.
            const auto seed=boot^owner.sessionId^account.primarySoid;
            if ((seed^(seed>>32))%10==0) {
                for(auto& claim:g_claims) {
                    if(claim.occupied && (!claim.delivered || state::activity::contains(claim.activity)))continue;
                    claim={owner,account.primarySoid,boot,completedRounds,0,1,0,true,false};
                    // Mix the run identity independently of the engram entry.
                    claim.rewardHash=forest_cache_policy::kMasks[(seed>>8)%forest_cache_policy::kMasks.size()];
                    report("bonus_queued",claim);break;
                }
            }
        }
        std::array<char, core::log::kLineCapacity> line{};
        const int count = std::snprintf(
            line.data(), line.size(),
            "ev=forest_chest stage=%s reason=grant_disabled owner=%016llX branches=%llu coins=%d",
            repeat ? "duplicate" : "drops_queued",
            static_cast<unsigned long long>(owner.sessionId),
            static_cast<unsigned long long>(completedRounds), coins);
        if (count > 0) {
            core::log::write(core::log::Channel::server, core::log::Level::info,
                             {line.data(), (std::min)(static_cast<std::size_t>(count), line.size() - 1U)});
        }
        return;
    }

    Claim* available = nullptr;
    for (Claim& claim : g_claims) {
        if (claim.occupied && claim.activity == owner && claim.boot == boot
            && claim.accountSoid == account.primarySoid) {
            report("duplicate", claim);
            return;
        }
        // A delivered claim is scoped to one activity incarnation. An undelivered one stays owed
        // even after the activity is gone, so a transient encoding failure cannot lose the payout.
        if (!claim.occupied || (claim.delivered && !state::activity::contains(claim.activity))) {
            if (available == nullptr) {
                available = &claim;
            }
        }
    }
    if (available == nullptr) {
        core::log::write(core::log::Channel::server,
                         core::log::Level::warn,
                         "ev=forest_chest stage=queue result=full");
        return;
    }

    *available = Claim{owner, account.primarySoid, boot, completedRounds, coins, coins, 0U, true, false};
    report("queued", *available);
}

bool consume(Session& session,
             Scratch& scratch,
             std::span<std::byte> response,
             std::size_t& written,
             bool& touchesScratch) noexcept {
    if (!session.authenticated || !session.queuez.family4Active
        || session.queuez.family4RootSoid == 0U) {
        return false;
    }

    const std::uint64_t now = GetTickCount64();
    for (Claim& claim : g_claims) {
        if (!claim.occupied || claim.delivered || claim.remaining <= 0 || now < claim.retryAt
            || claim.accountSoid != session.queuez.family4RootSoid) {
            continue;
        }

        claim.retryAt = now + kRetryBackoffMilliseconds;
        ServiceOutcome outcome{};
        auto& transaction = outcome.transaction.emplace<ProfileItemAcquisitionTransaction>();
        if (!state::prepare_profile_item_acquisition(
                state::build_data::collectibles::kNoCollectibleIndex,
                claim.rewardHash,
                transaction.pending)
            || !queuez::stage_profile_item_acquisition(
                session.queuez,
                transaction.pending.accountSoid,
                transaction.pending.acquiredInstanceSoid,
                transaction.pending.actionSource,
                transaction.pending.appended,
                transaction.update)) {
            report("retry_prepare", claim);
            continue;
        }

        touchesScratch = true;
        auto nonce = session.sendNonce;
        std::size_t size = 0U;
        queuez::StagedPublication publication{};
        if (!queuez::stage_service_outcome(scratch,
                                           session.queuez,
                                           outcome,
                                           state::bap().sessionKey,
                                           nonce,
                                           scratch.framed,
                                           size,
                                           publication)
            || !publication.hasState || size == 0U || size > response.size()
            || !state::commit_profile_item_acquisition(transaction.pending)) {
            report("retry_publish", claim);
            continue;
        }

        std::copy_n(scratch.framed.begin(), size, response.begin());
        written = size;
        session.sendNonce = nonce;
        session.queuez = publication.after;
        session.accountMutationPublished = true;
        --claim.remaining;
        claim.delivered = claim.remaining == 0;
        claim.retryAt = 0U;
        report(claim.delivered ? "granted" : "granted_partial", claim);
        return true;
    }
    return false;
}

} // namespace sunrise::server::bap::encrypted::forest_chest_rewards
