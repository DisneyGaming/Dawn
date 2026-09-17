#include "festival_pickups.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <string_view>

#include "../../../../core/logging/log.h"
#include "../../../../middleware/bap/activity_message/incident.h"
#include "../../../../middleware/bap/activity_message/loot_pickup.h"
#include "../../../../state/activity/events/activity_event_selection.h"
#include "../../../../state/activity/destination/activity_destination_snapshot.h"
#include "../../../../state/activity/runtime.h"
#include "../../../../state/build_data/collectibles/collectible_catalog.h"
#include "../../../../state/runtime/runtime.h"
#include "../queuez/queuez_outcome_staging.h"
#include "../queuez/queuez_state_validation.h"

namespace dawn::server::bap::encrypted::festival_pickups {
namespace {

namespace loot = middleware::bap::activity_message::loot_pickup;
namespace incident = middleware::bap::activity_message::incident;

/** Immutable server binding from one authored placed source to its profile reward. */
struct RewardBinding final {
    std::uint32_t sourceHash{};
    std::int32_t bubble{};
    std::uint32_t itemDefinitionHash{};
    std::int32_t quantity{};
};

/**
 * Authored item_loot source identities from the nine Tower Festival placements. The source and
 * bubble are both required: a source hash alone is not permission to claim it from another scope.
 */
constexpr std::array<RewardBinding, 9> kRewardBindings{{
    {0xE86DC713U, 6, 4084398230U, 50},
    {0xE86DC710U, 6, 4084398230U, 50},
    {0xE86DC711U, 6, 4084398230U, 50},
    {0xE86DC716U, 6, 4084398230U, 50},
    {0xB6D6DC59U, 6, 4084398230U, 250},
    {0xE86DC717U, 1, 4084398230U, 60},
    {0xE86DC714U, 1, 4084398230U, 60},
    {0xE86DC715U, 1, 4084398230U, 60},
    {0xB6D6DC5AU, 1, 4084398230U, 250},
}};

struct Claim final {
    state::activity::ActivityInstanceKey activity{};
    loot::Pickup pickup{};
    RewardBinding reward{};
    std::int32_t remaining{};
    std::uint64_t retryAt{};
    bool occupied{};
    bool delivered{};
};

/** BAP's route lock also serializes deferred account-subscriber polling. */
std::array<Claim, 256> g_claims{};

[[nodiscard]] const RewardBinding* reward_for(const loot::Pickup& pickup) noexcept {
    const auto result = std::find_if(
        kRewardBindings.begin(), kRewardBindings.end(), [&pickup](const RewardBinding& binding) {
            return binding.sourceHash == pickup.sourceHash && binding.bubble == pickup.bubble;
        });
    return result == kRewardBindings.end() ? nullptr : &*result;
}

[[nodiscard]] bool tower_destination(state::activity::ActivityInstanceKey activity) noexcept {
    constexpr std::string_view tower = "city_tower_social_d2";
    state::activity::destination::DestinationSelection destination{};
    if (!state::activity::destination::snapshot(activity, destination)
        || destination.packageNameLength != tower.size()) {
        return false;
    }
    return std::equal(tower.begin(), tower.end(), destination.packageName.begin());
}

[[nodiscard]] std::uint32_t event_key_for_bubble(std::int32_t bubble) noexcept {
    switch (bubble) {
    case 6: return 0x7C6DE64FU;
    case 1: return 0xFC6B8707U;
    case 7: return 0xEE34BBABU;
    case 0: return 0x6D3740C6U;
    default: return 0U;
    }
}

[[nodiscard]] bool festival_visible(std::int32_t bubble) noexcept {
    const std::uint32_t key = event_key_for_bubble(bubble);
    return key != 0U && !state::activity::events::withheld(key);
}

/**
 * One line for a well-formed loot pickup reported from outside the Tower - today that is the
 * Haunted Forest, whose baubles are paid by the web-service route, not here. Logged so a Forest
 * run that does report incident 3539 can be told apart from one that reports nothing at all.
 */
void report_observed(const char* reason, const loot::Pickup& pickup) noexcept {
    std::array<char, core::log::kLineCapacity> line{};
    const int count = std::snprintf(
        line.data(),
        line.size(),
        "ev=festival_pickup stage=observed reason=%s source=0x%08X bubble=%d nonce=0x%08X "
        "layout=%u extra_index=%u extra_bit=%u extra=0x%04X x=%.3f y=%.3f z=%.3f",
        reason,
        pickup.sourceHash,
        pickup.bubble,
        pickup.nonce,
        static_cast<unsigned>(pickup.variant),
        static_cast<unsigned>(pickup.extraFieldIndex),
        static_cast<unsigned>(pickup.extraFieldBitOffset),
        static_cast<unsigned>(pickup.extraField),
        static_cast<double>(pickup.position[0]),
        static_cast<double>(pickup.position[1]),
        static_cast<double>(pickup.position[2]));
    if (count > 0 && static_cast<std::size_t>(count) < line.size()) {
        // Debug, not info: a full Forest run reports one bauble pickup per bauble and each one
        // already logs on the web-service route. settings.json runs the server channel at debug.
        core::log::write(core::log::Channel::server,
                         core::log::Level::debug,
                         {line.data(), static_cast<std::size_t>(count)});
    }
}

void report(const char* stage, const Claim& claim) noexcept {
    std::array<char, core::log::kLineCapacity> line{};
    const int count = std::snprintf(
        line.data(),
        line.size(),
        "ev=loot stage=%s source=0x%08X item=%u quantity=%d remaining=%d bubble=%d nonce=0x%08X",
        stage,
        claim.pickup.sourceHash,
        claim.reward.itemDefinitionHash,
        claim.reward.quantity,
        claim.remaining,
        claim.pickup.bubble,
        claim.pickup.nonce);
    if (count > 0 && static_cast<std::size_t>(count) < line.size()) {
        core::log::write(core::log::Channel::server,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(count)});
    }
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
    loot::Pickup pickup{};
    if (incident::validate(request.payload, framed) != incident::Verdict::accepted
        || framed.primaryTarget != loot::kIncidentTarget || framed.extraTargetCount != 0
        || framed.hasCompressedSelector || !framed.hasPayload
        || !loot::known_payload_length(framed.payloadLength)) {
        return;
    }

    // The 82-byte Haunted Forest arm carries one extra 16-bit field at an unknown position, so the
    // session identity is what picks its layout. The 80-byte Tower arm ignores the expectation and
    // decodes exactly as before, so nothing about the Tower path changes. Reading the account here
    // instead of below is what makes report_observed("not_tower") reachable for a Forest report.
    const state::AccountState account = state::account_snapshot();
    const std::uint64_t selectedCharacter = state::account::selected_character_soid(account);
    const loot::Identity identity{account.primarySoid, selectedCharacter, loot::kForestBubble};
    if (!loot::parse(std::span(framed.payload).first(framed.payloadLength), pickup, &identity)) {
        return;
    }

    // The destination check moved below the parse so a non-Tower report is still visible. It is
    // reported and dropped here: nothing outside the Tower is ever paid by this ring.
    if (!tower_destination(activity)) {
        report_observed("not_tower", pickup);
        return;
    }
    if (!festival_visible(pickup.bubble)) {
        return;
    }

    if (account.primarySoid == 0U || session.activity.characterSoid == 0U
        || pickup.accountSoid != account.primarySoid || pickup.characterSoid != selectedCharacter
        || pickup.characterSoid != session.activity.characterSoid) {
        return;
    }

    const RewardBinding* reward = reward_for(pickup);
    if (reward == nullptr) {
        return;
    }

    Claim* available = nullptr;
    for (Claim& claim : g_claims) {
        if (claim.occupied && claim.activity == activity
            && claim.pickup.characterSoid == pickup.characterSoid
            && claim.pickup.sourceHash == pickup.sourceHash
            && claim.pickup.bubble == pickup.bubble) {
            report("duplicate", claim);
            return;
        }
        // Completed claims are scoped to an activity incarnation. Pending claims remain owed even
        // if the activity goes away, so a transient subscriber/encoding failure cannot lose Candy.
        if (!claim.occupied
            || (claim.delivered && !state::activity::contains(claim.activity))) {
            if (available == nullptr) {
                available = &claim;
            }
        }
    }
    if (available == nullptr) {
        core::log::write(core::log::Channel::server,
                         core::log::Level::warn,
                         "ev=loot stage=queue result=full");
        return;
    }

    *available = Claim{activity, pickup, *reward, reward->quantity, 0U, true, false};
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
            || claim.pickup.accountSoid != session.queuez.family4RootSoid) {
            continue;
        }

        claim.retryAt = now + 1000U;
        ServiceOutcome outcome{};
        auto& transaction = outcome.transaction.emplace<ProfileItemAcquisitionTransaction>();
        if (!state::prepare_profile_item_acquisition(
                state::build_data::collectibles::kNoCollectibleIndex,
                claim.reward.itemDefinitionHash,
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

} // namespace dawn::server::bap::encrypted::festival_pickups
