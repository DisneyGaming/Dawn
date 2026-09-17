#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "../../../../middleware/bap/activity_message/activity_message_request_parser.h"
#include "../internal.h"

namespace sunrise::server::bap::encrypted::forest_loot_pickups {

/**
 * Coins paid for one opened Haunted Forest branch bonus chest.
 * User requirement: exactly one Chocolate Strange Coin and no other loot.
 * Final-room caches have a separate Decoder transaction and never use this amount.
 */
inline constexpr std::int32_t kBranchChestCoins = 1;

/**
 * Most branch-chest claims one run may queue.
 * Storage bound, not a retail chest limit. Keep the full claim ring available so
 * the five fixed caches do not consume a small branch-chest allowance.
 */
inline constexpr std::int32_t kBranchChestClaimsPerRun = 64;

/**
 * [owner] Pay the branch chest on the reported pickup.
 * On, because the identity validation is the safety: the report must decode into this exact
 * account SOID, the selected character, the connection's own live activity binding and the Forest
 * destination before anything is queued. The source hash is deliberately NOT part of the gate -
 * no value for the branch chest has been observed yet, and gating on a guessed one would pay
 * nothing while looking as if it worked.
 */
inline constexpr bool kGrantOnPickup = true;

/**
 * Metres per dedupe position quantum.
 * [owner] Not authored. The branch chest and the five coffers are expected to share one source
 * hash, so the reported world position is part of the claim identity; half a metre is far below
 * the spacing of two placed containers and far above any float noise in one reported position.
 */
inline constexpr float kPositionQuantumMetres = 0.5F;

/**
 * Milliseconds between two attempts at the same owed unit.
 * [owner] Mirrors the festival pickup ring's backoff so one stuck subscriber cannot spin the
 * deferred push loop. Not an authored value.
 */
inline constexpr std::uint64_t kRetryBackoffMilliseconds = 1000U;

/**
 * Qualifies one placed-loot incident reported from inside the Haunted Forest and queues its owed
 * coins. Called only after the activity route has checked this connection's exact live binding.
 */
void receive(const Session& session,
             const middleware::bap::activity_message::Request& request) noexcept;

/** Publishes one owed Chocolate Strange Coin on an authenticated account subscriber. */
[[nodiscard]] bool consume(Session& session,
                           Scratch& scratch,
                           std::span<std::byte> response,
                           std::size_t& written,
                           bool& touchesScratch) noexcept;

} // namespace sunrise::server::bap::encrypted::forest_loot_pickups
