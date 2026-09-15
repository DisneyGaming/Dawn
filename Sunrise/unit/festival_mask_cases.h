#pragma once

#include "state/account/festival_mask.h"

void festival_mask_cases() {
    namespace mask = sunrise::state::account::festival_mask;
    using sunrise::state::AccountState;
    using sunrise::state::CharacterState;
    using sunrise::state::account::inventory::EquipmentSlot;

    AccountState snapshot{};
    snapshot.characterCount = 1;
    snapshot.characters[0] = CharacterState{};
    snapshot.characters[0].soid = 0x1001;
    snapshot.characters[0].equipment.slots[static_cast<std::size_t>(EquipmentSlot::helmet)] =
        sunrise::state::account::inventory::Item{0x2001, mask::kDefinitionHashes[0]};

    CHECK(mask::has_current_equipped_festival_mask(0x1001, snapshot));
    CHECK(!mask::has_current_equipped_festival_mask(0x2001, snapshot));

    auto wrongSlot = snapshot;
    wrongSlot.characters[0].equipment.slots[static_cast<std::size_t>(EquipmentSlot::helmet)] =
        std::nullopt;
    wrongSlot.characters[0].equipment.slots[static_cast<std::size_t>(EquipmentSlot::chest)] =
        sunrise::state::account::inventory::Item{0x2002, mask::kDefinitionHashes[0]};
    CHECK(!mask::has_current_equipped_festival_mask(0x1001, wrongSlot));

    auto inventoryOnly = snapshot;
    inventoryOnly.characters[0].equipment.slots[static_cast<std::size_t>(EquipmentSlot::helmet)] =
        std::nullopt;
    inventoryOnly.characters[0].inventory.count = 1;
    inventoryOnly.characters[0].inventory.values[0] =
        sunrise::state::account::inventory::Item{0x2003, mask::kDefinitionHashes[0]};
    CHECK(!mask::has_current_equipped_festival_mask(0x1001, inventoryOnly));

    auto otherHelmet = snapshot;
    otherHelmet.characters[0].equipment.slots[static_cast<std::size_t>(EquipmentSlot::helmet)]->
        definitionHash = 0x12345678U;
    CHECK(!mask::has_current_equipped_festival_mask(0x1001, otherHelmet));
}
