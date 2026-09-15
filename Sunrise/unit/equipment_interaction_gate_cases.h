#pragma once

#include "state/account/festival_mask.h"
#include "server/runtime/activity/equipment_interaction_gate.h"

void equipment_interaction_gate_cases() {
    namespace gate = sunrise::server::runtime::activity::equipment_interaction;
    namespace interaction = sunrise::middleware::bap::activity_message::native::interaction;
    namespace placement = sunrise::middleware::bap::activity_message::native::placement;
    using sunrise::state::AccountState;
    using sunrise::state::CharacterState;
    using sunrise::state::account::inventory::EquipmentSlot;

    constexpr std::uint32_t sourceRegistry = 0x7C6DE64FU;
    constexpr std::uint16_t sourceSlot = 1;
    const gate::Gate maskGate{
        sourceRegistry, sourceSlot, EquipmentSlot::helmet,
        sunrise::state::account::festival_mask::kDefinitionHashes};

    AccountState snapshot{};
    snapshot.characterCount = 1;
    snapshot.characters[0] = CharacterState{};
    snapshot.characters[0].soid = 0x1001;
    snapshot.characters[0].selected = true;
    snapshot.characters[0].equipment.slots[static_cast<std::size_t>(EquipmentSlot::helmet)] =
        sunrise::state::account::inventory::Item{
            0x2001, sunrise::state::account::festival_mask::kDefinitionHashes[0]};

    placement::Batch placements{};
    placements.count = 2;
    placements.entries[0] = {sourceRegistry, sourceSlot, 6, interaction::Mode::unchanged, 17};
    placements.entries[0].active = false;
    placements.entries[0].position = placement::Position{1.0F, 2.0F, 3.0F};
    placements.entries[1] = {0xDEAD0001U, 2, 6, interaction::Mode::enabled, 23};
    const auto otherBefore = placements.entries[1];

    CHECK(gate::valid(maskGate));
    CHECK(gate::project(std::span(&maskGate, 1), snapshot, placements));
    CHECK(placements.entries[0].interactionMode == interaction::Mode::enabled);
    CHECK(placements.entries[0].registry == sourceRegistry && placements.entries[0].slot == sourceSlot
        && placements.entries[0].generation == 17 && !placements.entries[0].active
        && placements.entries[0].position.has_value()
        && placements.entries[0].position->x == 1.0F
        && placements.entries[0].position->y == 2.0F
        && placements.entries[0].position->z == 3.0F);
    CHECK(placements.entries[1].registry == otherBefore.registry
        && placements.entries[1].slot == otherBefore.slot
        && placements.entries[1].bubble == otherBefore.bubble
        && placements.entries[1].generation == otherBefore.generation
        && placements.entries[1].interactionMode == otherBefore.interactionMode);

    auto held = snapshot;
    held.characters[0].equipment.slots[static_cast<std::size_t>(EquipmentSlot::helmet)].reset();
    held.characters[0].inventory.count = 1;
    held.characters[0].inventory.values[0] = {
        0x2002, sunrise::state::account::festival_mask::kDefinitionHashes[0]};
    placements.entries[0].interactionMode = interaction::Mode::unchanged;
    CHECK(gate::project(std::span(&maskGate, 1), held, placements));
    CHECK(placements.entries[0].interactionMode == interaction::Mode::disabled);

    auto wrong = snapshot;
    wrong.characters[0].equipment.slots[static_cast<std::size_t>(EquipmentSlot::helmet)]
        ->definitionHash = 0x12345678U;
    placements.entries[0].interactionMode = interaction::Mode::unchanged;
    CHECK(gate::project(std::span(&maskGate, 1), wrong, placements));
    CHECK(placements.entries[0].interactionMode == interaction::Mode::disabled);

    auto missing = snapshot;
    missing.characters[0].equipment.slots[static_cast<std::size_t>(EquipmentSlot::helmet)].reset();
    placements.entries[0].interactionMode = interaction::Mode::unchanged;
    CHECK(gate::project(std::span(&maskGate, 1), missing, placements));
    CHECK(placements.entries[0].interactionMode == interaction::Mode::disabled);
}
