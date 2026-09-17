#pragma once

#include <cstdint>
#include "../../state/activity/definition.h"

namespace dawn::server::web_service::forest_loot {

/** Candy, item 0xF372F896 / index 151: the only authored drop this route pays today. */
inline constexpr std::uint32_t kCandyDefinitionHash = 0xF372F896U;

/**
 * Candy credited per reported bauble pickup.
 * [inferred/owner] The authored per-entry quantity lives in the combatants' reward sheets and is
 * being decoded; until it is, one unit per pickup sits inside the retail-observed band of roughly
 * 100-300 Candy per fifteen-minute run. Replace with the authored value, never with a guess.
 */
inline constexpr std::int32_t kCandyPerPickup = 1;

/** Native pickup kinds that spawn a visible bauble (record +8): reward-sheet drop 3 and kind 5. */
inline constexpr std::int32_t kRewardSheetDropKind = 3;
inline constexpr std::int32_t kBaubleDropKindAlternate = 5;

/** Haunted Forest destination package name. */
inline constexpr char kForestPackageName[] = "infinite_abyss";

/** Candy's item-definition index in this build's investment tables: the record's +0 field. [authored] */
inline constexpr std::uint16_t kCandyDefinitionIndex = 151;

/**
 * Percentage of qualifying Forest kills that drop a Candy bauble.
 * Provisional tuning for occasional drops; the original retail probability is not recovered.
 * This is not an authored reward-sheet value or an every-kill diagnostic setting.
 */
inline constexpr std::int32_t kCandyDropPercent = 20;

/**
 * True while a Forest candy drop should exist at all: the Festival is live, the selected character
 * wears a Festival mask and the joined destination is the Haunted Forest. Read by the client-side
 * drop hook on the game thread; the pickup route re-checks the same gates before paying.
 */
[[nodiscard]] bool candy_drop_armed() noexcept;

/** Chocolate Strange Coin, item 0x4750ED6F / index 150. [authored] */
inline constexpr std::uint32_t kChocolateStrangeCoinHash = 0x4750ED6FU;
inline constexpr std::uint16_t kChocolateStrangeCoinIndex = 150;

/** Pending pickups must use the same item identity as their server payout. */
[[nodiscard]] inline constexpr std::uint16_t pickup_definition_index(std::uint32_t hash) noexcept {
    if (hash == kCandyDefinitionHash) { return kCandyDefinitionIndex; }
    if (hash == kChocolateStrangeCoinHash) { return kChocolateStrangeCoinIndex; }
    return UINT16_MAX;
}


/**
 * What the server pays for a drop the client hook wrote, keyed by the tuple the client reports in
 * opcode 601 (the item is never on the wire). Unregistered tuples pay Candy.
 */
void remember_injected_drop(std::int32_t sourceTag, std::uint64_t sourceHandle, std::int32_t sequence,
                            std::uint32_t itemDefinitionHash, std::int32_t quantity = 1) noexcept;
[[nodiscard]] bool injected_drop_item(std::int32_t sourceTag, std::uint64_t sourceHandle, std::int32_t sequence,
                                      std::uint32_t& itemDefinitionHash, std::int32_t& quantity) noexcept;

/** A prepared pickup follows its inventory transaction; no claim or cleanup occurs before commit. */
struct PickupCommit final {
    std::int32_t sourceTag{};
    std::uint64_t sourceHandle{};
    std::int32_t sequence{};
    std::uint64_t characterSoid{};
    std::uint32_t itemDefinitionHash{};
    bool prepared{};
};
void finish_pickup(const PickupCommit& pickup, bool committed) noexcept;

/** A drop the server wants spawned at a world position (chest rewards); drained by the client hook. */
struct ChestDrop final {
    float x{}, y{}, z{};
    std::uint32_t itemDefinitionHash{};
    std::int32_t quantity{1};
    state::activity::ActivityInstanceKey owner{};
    std::uint32_t chestGeneration{};
};
void queue_chest_drops(std::int32_t count, float x, float y, float z, std::uint32_t itemDefinitionHash,
    std::int32_t quantity, state::activity::ActivityInstanceKey owner, std::uint32_t chestGeneration) noexcept;
[[nodiscard]] bool take_chest_drop(ChestDrop& drop) noexcept;

/** A paid pickup whose client record the hook should free so the bauble despawns. */
struct RetireRequest final {
    std::int32_t sourceTag{};
    std::uint64_t sourceHandle{};
    std::int32_t sequence{};
    std::uint64_t queuedTick{};
};
void request_retire(std::int32_t sourceTag, std::uint64_t sourceHandle, std::int32_t sequence) noexcept;
/** Hands out the oldest request at least minAgeMs old: the client must process the reply first. */
[[nodiscard]] bool take_retire(RetireRequest& request, std::uint64_t minAgeMs) noexcept;

} // namespace dawn::server::web_service::forest_loot
