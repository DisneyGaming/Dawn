#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "../../../../state/activity/coo/native_player_trigger.h"
#include "../internal.h"
#include "forest_chest_policy.h"

namespace dawn::server::bap::encrypted::forest_chest_rewards {

/** Authored Haunted Forest registry key (package "infinite_abyss", bubble 13). [authored] */
inline constexpr std::uint32_t kForestRegistryKey = 0x34D23982U;

/** Native nearby sensor; shared placement-pose service arms it while the chest is placed. */
inline constexpr std::int16_t kRewardChestTriggerSlot = 104;

/**
 * Authored pt_reward_space player trigger, registry slot 102 (descriptor 0x8155015E): the volume
 * around the chest that the client actually reported on 15 Sep 2026 (target 6685, object
 * 0x811C9DC5) three seconds after the chest sense entry appeared, while slot 104 never fired.
 * Either volume proves the player reached the placed chest. [authored]
 */
inline constexpr std::int16_t kRewardSpaceTriggerSlot = 102;

/**
 * [owner] Pay the chest as a silent account grant when the player reaches it. Off: the owner wants
 * the chest to open and drop its coins as pickups; until that mechanism ships, reaching the chest is
 * logged (stage=observed reason=grant_disabled) and pays nothing.
 */
inline constexpr bool kGrantOnReach = false;

/** Authored pf_reward_chest.o_chest world position (registry slot 55). [authored] */
inline constexpr float kRewardChestX = -368.7128F;
inline constexpr float kRewardChestY = -650.7294F;
inline constexpr float kRewardChestZ = 1190.6512F;

/** Authored pf_reward_chest.o_chest placement, registry slot 55 (class 0x80BEBFF5). [authored] */
inline constexpr std::uint16_t kRewardChestPlacementSlot = 55;

/**
 * Wire slot type of an authored placement object. [authored] placement::wire::find fails closed for
 * any other type, so a wrong value here reads as "the chest was never placed" and pays nothing.
 */
inline constexpr std::uint8_t kPlacementSlotType = 4;

/** Chocolate Strange Coin, item index 150, profile stack, max stack 9999. [authored] */
inline constexpr std::uint32_t kChocolateStrangeCoinHash = 0x4750ED6FU;

/**
 * Qualifies one decoded type-31 receipt as a Haunted Forest end-of-run chest open and queues the
 * single owed payout for this run. Silent for any trigger that is not pt_reward_chest.
 */
void receive(const Session& session,
             state::activity::ActivityInstanceKey owner,
             const state::activity::coo::native_player_trigger::Receipt& receipt) noexcept;

/** Publishes one owed Chocolate Strange Coin on an authenticated account subscriber. */
[[nodiscard]] bool consume(Session& session,
                           Scratch& scratch,
                           std::span<std::byte> response,
                           std::size_t& written,
                           bool& touchesScratch) noexcept;

} // namespace dawn::server::bap::encrypted::forest_chest_rewards
