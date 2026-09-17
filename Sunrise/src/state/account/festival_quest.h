#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "account_state.h"

namespace sunrise::state::account::festival_quest {

/** Eva's alternate Rewards display group repeats masks and bags from the Werewolf catalogue. */
inline constexpr std::int16_t kAlternateRewardsFlag = 950;

struct EvaInteraction { bool intro{}; bool wearingMasks{}; bool finalStage{}; };

inline constexpr std::uint16_t kMaskReceiptOpcode = 904;
inline constexpr std::int32_t kMaskReceiptVendor = 4;
inline constexpr std::int32_t kMaskReceiptCategory = 79;
inline constexpr std::int32_t kMaskReceiptFirstSale = 365;
inline constexpr std::int32_t kMaskReceiptLastSale = 382;

/** One authored six-row group of Eva's quest-mask category; every row of a group sells one mask. */
struct MaskReceiptGroup {
    std::int32_t firstSale;
    std::int32_t lastSale;
    std::uint32_t definitionHash;
};

/** Read from Eva's vendor blob: Cowl 9541 on 365..370, Helm 9991 on 371..376, Hood 10492 on 377..382. */
inline constexpr std::array<MaskReceiptGroup, 3> kMaskReceiptGroups{{
    {365, 370, 0x8C32CA56U},
    {371, 376, 0x0E41BC1AU},
    {377, 382, 0x83EF679BU},
}};

inline constexpr std::uint32_t kMaskedPurpose = 2174315371U;
inline constexpr std::array<std::uint32_t, 5> kSteps{
    0xC30052CCU, 0x9DB85110U, 0x9DB85113U, 0x9DB85112U, 0x9DB85115U};

/** Pure predicate for the one vendor receipt that advances Wearing Masks. */
[[nodiscard]] constexpr bool is_mask_receipt(std::uint16_t opcode,
                                              std::int32_t vendorIndex,
                                              std::int32_t categoryIndex,
                                              std::int32_t saleIndex,
                                              std::uint32_t definitionHash) noexcept {
    if (opcode != kMaskReceiptOpcode || vendorIndex != kMaskReceiptVendor
        || categoryIndex != kMaskReceiptCategory) {
        return false;
    }
    // A row is a receipt only for the one mask its authored group sells; a mask hash on another
    // group's row is build-data drift and must not advance the quest.
    for (const MaskReceiptGroup& group : kMaskReceiptGroups) {
        if (saleIndex >= group.firstSale && saleIndex <= group.lastSale) {
            return definitionHash == group.definitionHash;
        }
    }
    return false;
}

[[nodiscard]] inline EvaInteraction available(const CharacterState& character,
                                               bool eventActive) noexcept {
    EvaInteraction result{};
    if (!eventActive || character.inventory.count > character.inventory.values.size()) return result;
    std::size_t highest = 0;
    bool found = false;
    for (std::size_t i = 0; i < character.inventory.count; ++i) {
        const auto hash = character.inventory.values[i].definitionHash;
        for (std::size_t step = 0; step < kSteps.size(); ++step) {
            if (hash == kSteps[step] && (!found || step > highest)) { highest = step; found = true; }
        }
    }
    if (!found) result.intro = true;
    else if (highest == 0) result.wearingMasks = true;
    else if (highest == 4) result.finalStage = true;
    return result;
}

} // namespace sunrise::state::account::festival_quest
