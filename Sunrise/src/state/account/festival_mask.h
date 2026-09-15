#pragma once

#include <array>
#include <cstdint>

#include "account_state.h"

namespace sunrise::state::account::festival_mask {

/** Recovered Eva vendor identity used to document the source of the helmet allow-list. */
inline constexpr std::uint32_t kEvaVendorIndexRootTag = 0x8131931DU;
inline constexpr std::uint32_t kEvaVendorDefinitionHash = 0x36D32C3CU;
inline constexpr std::uint16_t kEvaVendorDefinitionIndex = 4;
inline constexpr std::uint32_t kEvaVendorDefinitionTag = 0x81319082U;
inline constexpr std::uint16_t kEvaFirstMaskSaleRow = 11;
inline constexpr std::uint16_t kEvaLastMaskSaleRow = 28;

/** Recovered installed metadata shared by the three accepted Festival helmets. */
inline constexpr std::uint8_t kHelmetBucketId = 3;
inline constexpr std::uint32_t kLocalizedNameBank = 192;
inline constexpr std::array<std::uint32_t, 3> kDefinitionHashes{
    0x8C32CA56U,
    0x0E41BC1AU,
    0x83EF679BU,
};

/**
 * Checks only the exact helmet equipment slot on the named character.
 * Inventory ownership, ornaments, and similarly named helmet definitions are not accepted.
 *
 * @param characterId Character SOID to find in the supplied account snapshot.
 * @param snapshot Account view already copied by the caller.
 * @return True when the character exists and its equipped helmet is one of the three masks.
 */
[[nodiscard]] inline bool has_current_equipped_festival_mask(
    std::uint64_t characterId,
    const AccountState& snapshot) noexcept {
    if (characterId == 0 || snapshot.characterCount > snapshot.characters.size()) {
        return false;
    }

    const CharacterState* character = nullptr;
    for (std::size_t index = 0; index < snapshot.characterCount; ++index) {
        if (snapshot.characters[index].soid != characterId) {
            continue;
        }
        if (character != nullptr) {
            return false;
        }
        character = &snapshot.characters[index];
    }
    if (character == nullptr) {
        return false;
    }

    const auto helmetIndex = static_cast<std::size_t>(inventory::EquipmentSlot::helmet);
    const auto& helmet = character->equipment.slots[helmetIndex];
    if (!helmet.has_value() || helmet->instanceSoid == 0) {
        return false;
    }
    for (const std::uint32_t definitionHash : kDefinitionHashes) {
        if (helmet->definitionHash == definitionHash) {
            return true;
        }
    }
    return false;
}

} // namespace sunrise::state::account::festival_mask
