#include "forest_loot_pickups.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string_view>

#include "../../../../core/logging/log.h"
#include "../../../../middleware/bap/activity_message/incident.h"
#include "../../../../middleware/bap/activity_message/loot_pickup.h"
#include "../../../../state/activity/destination/activity_destination_snapshot.h"
#include "../../../../state/activity/runtime.h"
#include "../../../../state/build_data/collectibles/collectible_catalog.h"
#include "../../../../state/runtime/runtime.h"
#include "../../../web_service/forest_loot_pickups.h"
#include "../queuez/queuez_outcome_staging.h"
#include "../queuez/queuez_state_validation.h"
#include "forest_chest_rewards.h"
#include "forest_cache_policy.h"
#include "../../../../state/build_data/runtime.h"
#include "../../../runtime/activity/native_activity_runtime.h"

namespace dawn::server::bap::encrypted::forest_loot_pickups {
namespace {

namespace loot = middleware::bap::activity_message::loot_pickup;
namespace incident = middleware::bap::activity_message::incident;

/** Position cells that make up one claim's dedupe identity. */
using Cell = std::array<std::int32_t, 3>;

/**
 * One owed branch-chest payout.
 *
 * The dedupe identity is the activity, the reporting character, the source hash, the bubble and
 * the quantised world position. The position has to be in it: the generator-placed branch chest
 * and the five authored reward coffers are expected to share one Forest source hash, and one run
 * can open more than one of them, so a key without the position would collapse them into a single
 * claim and pay once.
 */
struct Claim final {
    state::activity::ActivityInstanceKey activity{};
    loot::Pickup pickup{};
    Cell cell{};
    std::int32_t coins{};
    std::int32_t remaining{};
    std::uint64_t retryAt{};
    bool occupied{};
    bool delivered{};
    int cache{-1};
    forest_cache_policy::Plan rewards{};
    std::uint8_t nextReward{};
    bool decoderCharged{};
};

/** BAP's route lock also serializes deferred account-subscriber polling. */
std::array<Claim, 64> g_claims{};

/** Quantises one reported coordinate into whole kPositionQuantumMetres cells. */
[[nodiscard]] std::int32_t quantise(float metres) noexcept {
    if (!std::isfinite(metres)) {
        return 0;
    }
    const double cells =
        static_cast<double>(metres) / static_cast<double>(kPositionQuantumMetres);
    // The clamp keeps the rounded result inside std::int32_t for any finite reported coordinate.
    const double bounded = std::clamp(cells, -1.0e9, 1.0e9);
    return static_cast<std::int32_t>(std::llround(bounded));
}

[[nodiscard]] Cell cell_of(const loot::Pickup& pickup) noexcept {
    return {quantise(pickup.position[0]), quantise(pickup.position[1]),
            quantise(pickup.position[2])};
}

void report(const char* stage, const char* reason, const Claim& claim) noexcept {
    std::array<char, core::log::kLineCapacity> line{};
    const int count = std::snprintf(
        line.data(),
        line.size(),
        "ev=forest_loot stage=%s reason=%s owner=%016llX incarnation=%llu source=0x%08X bubble=%d "
        "item=0x%08X coins=%d remaining=%d layout=%u extra_index=%u extra_bit=%u extra=0x%04X "
        "x=%.3f y=%.3f z=%.3f cell=%d,%d,%d",
        stage,
        reason,
        static_cast<unsigned long long>(claim.activity.sessionId),
        static_cast<unsigned long long>(claim.activity.incarnation.value),
        claim.pickup.sourceHash,
        claim.pickup.bubble,
        forest_chest_rewards::kChocolateStrangeCoinHash,
        claim.coins,
        claim.remaining,
        static_cast<unsigned>(claim.pickup.variant),
        static_cast<unsigned>(claim.pickup.extraFieldIndex),
        static_cast<unsigned>(claim.pickup.extraFieldBitOffset),
        static_cast<unsigned>(claim.pickup.extraField),
        static_cast<double>(claim.pickup.position[0]),
        static_cast<double>(claim.pickup.position[1]),
        static_cast<double>(claim.pickup.position[2]),
        claim.cell[0],
        claim.cell[1],
        claim.cell[2]);
    if (count > 0 && static_cast<std::size_t>(count) < line.size()) {
        core::log::write(core::log::Channel::server,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(count)});
    }
}

/** One line for a Forest placed-loot report that never reached a decoded pickup. */
void report_undecoded(const char* reason,
                      state::activity::ActivityInstanceKey activity,
                      std::uint32_t payloadLength) noexcept {
    std::array<char, core::log::kLineCapacity> line{};
    const int count = std::snprintf(
        line.data(),
        line.size(),
        "ev=forest_loot stage=skipped reason=%s owner=%016llX incarnation=%llu payload=%u",
        reason,
        static_cast<unsigned long long>(activity.sessionId),
        static_cast<unsigned long long>(activity.incarnation.value),
        payloadLength);
    if (count > 0 && static_cast<std::size_t>(count) < line.size()) {
        core::log::write(core::log::Channel::server,
                         core::log::Level::debug,
                         {line.data(), static_cast<std::size_t>(count)});
    }
}

/** Mirrors festival_pickups::tower_destination against the Haunted Forest package. */
[[nodiscard]] bool forest_destination(state::activity::ActivityInstanceKey activity) noexcept {
    constexpr std::string_view forest = web_service::forest_loot::kForestPackageName;
    state::activity::destination::DestinationSelection destination{};
    if (!state::activity::destination::snapshot(activity, destination)
        || destination.packageNameLength != forest.size()) {
        return false;
    }
    return std::equal(forest.begin(), forest.end(), destination.packageName.begin());
}

[[nodiscard]] bool cache_placed(state::activity::ActivityInstanceKey owner, int cache) noexcept {
    namespace activity = server::runtime::activity;
    auto phase=activity::timed_round::Phase::entry;
    std::uint64_t branches{},boot{};
    if (!activity::native_activity::round_progress(owner,phase,branches,boot)
        || (phase!=activity::timed_round::Phase::rewards
            && phase!=activity::timed_round::Phase::complete)) return false;
    const activity::NativeActivityDefinition* definition=nullptr;
    activity::placement::wire::Batch placements{};
    if (!activity::native_activity::snapshot_placements(owner,definition,placements)
        || !definition) return false;
    const auto* placed=activity::placement::wire::find(placements,0x34D23982U,4,
        static_cast<std::uint16_t>(63+cache));
    return placed && placed->interactionMode==activity::placement::interaction::Mode::enabled;
}

// Charge and first award share one prepared after-image and one publication. A
// failed encode or full inventory cannot spend a Decoder. Subsequent awards stay
// owed in this claim and never re-roll or charge a second Decoder.
[[nodiscard]] bool consume_cache(Claim& claim,Session& session,Scratch& scratch,
    std::span<std::byte> response,std::size_t& written,bool& touchesScratch) noexcept {
    namespace data=state::build_data;
    if (claim.nextReward>=claim.rewards.count) return false;
    const auto account=state::account_snapshot();
    if (state::account::selected_character_soid(account)!=claim.pickup.characterSoid) return false;
    data::items::Definition item{},decoder{};
    data::inventory::buckets::Descriptor bucket{};
    const auto hash=claim.rewards.items[claim.nextReward];
    if (!data::find_item_definition_hash(hash,item)
        || !data::find_inventory_bucket_descriptor(item.bucketId,bucket)
        || !data::find_item_definition_hash(forest_cache_policy::kDecoder,decoder)) return false;
    const std::array<data::material_requirements::Requirement,1> price{{
        {1,decoder.definitionIndex,data::material_requirements::kUnconditionalRequirement,true,false}}};
    const std::span<const data::material_requirements::Requirement> cost=
        claim.decoderCharged ? std::span<const data::material_requirements::Requirement>{} : price;
    ServiceOutcome outcome{};
    const bool profile=bucket.arraySelector==data::inventory::buckets::ArraySelector::profile;
    if (profile) {
        auto& transaction=outcome.transaction.emplace<ProfileItemAcquisitionTransaction>();
        if (!state::prepare_profile_item_acquisition(data::collectibles::kNoCollectibleIndex,
                hash,transaction.pending,cost)
            || !queuez::stage_profile_item_acquisition(session.queuez,transaction.pending.accountSoid,
                transaction.pending.acquiredInstanceSoid,transaction.pending.actionSource,
                transaction.pending.appended,transaction.update)) return false;
    } else if (bucket.arraySelector==data::inventory::buckets::ArraySelector::character) {
        auto& transaction=outcome.transaction.emplace<ItemAcquisitionTransaction>();
        if (!state::prepare_item_acquisition(data::collectibles::kNoCollectibleIndex,
                hash,transaction.pending,cost)
            || !queuez::stage_item_acquisition(session.queuez,transaction.pending.accountSoid,
                transaction.pending.characterSoid,transaction.pending.acquiredInstanceSoid,
                transaction.pending.profileChanged,transaction.update)) return false;
    } else return false;
    touchesScratch=true;
    auto nonce=session.sendNonce;
    std::size_t size{};
    queuez::StagedPublication publication{};
    if (!queuez::stage_service_outcome(scratch,session.queuez,outcome,state::bap().sessionKey,
            nonce,scratch.framed,size,publication)
        || !publication.hasState || !size || size>response.size()) return false;
    const bool committed=profile
        ? state::commit_profile_item_acquisition(std::get<ProfileItemAcquisitionTransaction>(outcome.transaction).pending)
        : state::commit_item_acquisition(std::get<ItemAcquisitionTransaction>(outcome.transaction).pending);
    if (!committed) return false;
    std::copy_n(scratch.framed.begin(),size,response.begin());
    written=size;session.sendNonce=nonce;session.queuez=publication.after;
    session.accountMutationPublished=true;
    claim.decoderCharged=true;
    ++claim.nextReward;
    claim.remaining=claim.rewards.count-claim.nextReward;
    claim.delivered=claim.remaining==0;
    claim.retryAt=0;
    core::log::writef(core::log::Channel::server,core::log::Level::info,
        "ev=forest_cache stage=granted slot=%d item=0x%08X remaining=%d decoder_charge=%d",
        63+claim.cache,hash,claim.remaining,claim.nextReward==1?1:0);
    return true;
}

} // namespace

void receive(const Session& session,
             const middleware::bap::activity_message::Request& request) noexcept {
    const state::activity::ActivityInstanceKey activity = session.activity.instance;
    if (!session.authenticated || !lifecycle::activity_binding_is_current(session)
        || !static_cast<bool>(activity) || !state::activity::contains(activity)
        || request.accountHandle != activity.sessionId) {
        return;
    }

    incident::Incident framed{};
    if (incident::validate(request.payload, framed) != incident::Verdict::accepted
        || framed.primaryTarget != loot::kIncidentTarget || framed.extraTargetCount != 0
        || framed.hasCompressedSelector || !framed.hasPayload
        || !loot::known_payload_length(framed.payloadLength)) {
        return;
    }

    // Destination first: the Tower's own placed loot is paid by festival_pickups and must not
    // leave a second trace here, and no other activity's report is ours at all.
    if (!forest_destination(activity)) {
        return;
    }

    const state::AccountState account = state::account_snapshot();
    const std::uint64_t selectedCharacter = state::account::selected_character_soid(account);
    if (account.primarySoid == 0U || selectedCharacter == 0U
        || session.activity.characterSoid == 0U
        || selectedCharacter != session.activity.characterSoid) {
        report_undecoded("session_identity", activity, framed.payloadLength);
        return;
    }

    // The 82-byte Forest arm carries one extra 16-bit field whose position is not known, so the
    // decoder tries every insertion point and accepts only the layout whose identity fields match
    // this session. A body that decodes under none of them, or under more than one, fails closed.
    const loot::Identity identity{account.primarySoid, selectedCharacter, loot::kForestBubble};
    loot::Pickup pickup{};
    if (!loot::parse(std::span(framed.payload).first(framed.payloadLength), pickup, &identity)) {
        report_undecoded("undecoded", activity, framed.payloadLength);
        return;
    }

    // The 80-byte arm ignores the expectation, so the identity is re-checked here for both arms.
    if (pickup.accountSoid != account.primarySoid || pickup.characterSoid != selectedCharacter
        || pickup.characterSoid != session.activity.characterSoid
        || pickup.bubble != loot::kForestBubble) {
        report_undecoded("identity", activity, framed.payloadLength);
        return;
    }

    Claim probe{activity, pickup, cell_of(pickup), kBranchChestCoins, kBranchChestCoins, 0U, false,
                false};
    probe.cache=forest_cache_policy::cache_at(pickup.position);
    if (probe.cache==-2) {
        report("skipped","reward_room_not_branch_chest",probe);
        return;
    }
    if (probe.cache>=0 && !cache_placed(activity,probe.cache)) {
        report("skipped","cache_not_live",probe);
        return;
    }
    if (!kGrantOnPickup) {
        report("skipped", "grant_disabled", probe);
        return;
    }

    Claim* available = nullptr;
    std::int32_t claimsThisRun = 0;
    for (Claim& claim : g_claims) {
        if (claim.occupied && claim.activity == activity) {
            ++claimsThisRun;
            if (claim.pickup.characterSoid == pickup.characterSoid
                && ((probe.cache>=0 && claim.cache==probe.cache)
                    || (probe.cache<0 && claim.cache<0
                        && claim.pickup.sourceHash == pickup.sourceHash
                        && claim.pickup.bubble == pickup.bubble && claim.cell == probe.cell))) {
                report("duplicate", "same_source_cell", claim);
                return;
            }
        }
        // A delivered claim is scoped to one activity incarnation. An undelivered one stays owed
        // even after the activity is gone, so a transient encoding failure cannot lose the payout.
        if (!claim.occupied || (claim.delivered && !state::activity::contains(claim.activity))) {
            if (available == nullptr) {
                available = &claim;
            }
        }
    }
    if (claimsThisRun >= kBranchChestClaimsPerRun) {
        report("skipped", "run_cap", probe);
        return;
    }
    if (available == nullptr) {
        report("skipped", "ring_full", probe);
        return;
    }

    if (probe.cache>=0) {
        probe.rewards=forest_cache_policy::roll(activity.sessionId^pickup.characterSoid
            ^(static_cast<std::uint64_t>(pickup.nonce)<<32)^static_cast<unsigned>(probe.cache));
        probe.coins=0;
        probe.remaining=probe.rewards.count;
    }
    *available = probe;
    available->occupied = true;
    report("queued", "ok", *available);
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
            || claim.pickup.accountSoid != session.queuez.family4RootSoid) {
            continue;
        }

        claim.retryAt = now + kRetryBackoffMilliseconds;
        if (claim.cache>=0) {
            if (consume_cache(claim,session,scratch,response,written,touchesScratch)) return true;
            report("retry_prepare","cache_transaction",claim);
            continue;
        }
        ServiceOutcome outcome{};
        auto& transaction = outcome.transaction.emplace<ProfileItemAcquisitionTransaction>();
        if (!state::prepare_profile_item_acquisition(
                state::build_data::collectibles::kNoCollectibleIndex,
                forest_chest_rewards::kChocolateStrangeCoinHash,
                transaction.pending)
            || !queuez::stage_profile_item_acquisition(
                session.queuez,
                transaction.pending.accountSoid,
                transaction.pending.acquiredInstanceSoid,
                transaction.pending.actionSource,
                transaction.pending.appended,
                transaction.update)) {
            report("retry_prepare", "stage", claim);
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
            report("retry_publish", "publish", claim);
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
        report(claim.delivered ? "granted" : "granted_partial", "ok", claim);
        return true;
    }
    return false;
}

} // namespace dawn::server::bap::encrypted::forest_loot_pickups
