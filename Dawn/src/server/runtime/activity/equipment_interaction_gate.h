#pragma once

#include "../../../middleware/bap/activity_message/native/placement_authority.h"
#include "../../../state/account/account_state.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>

namespace dawn::server::runtime::activity::equipment_interaction {

/** A bounded authored interaction gate for one native type-4 placement. */
struct Gate final {
    std::uint32_t registry{};
    std::uint16_t slot{};
    state::account::inventory::EquipmentSlot equipmentSlot{
        state::account::inventory::EquipmentSlot::kinetic};
    std::span<const std::uint32_t> allowedDefinitionHashes{};
    // Optional profile-material requirement. It can stand alone or accompany
    // the equipment predicate; charging belongs to the interaction transaction.
    std::uint32_t requiredProfileItem{};
    std::int32_t requiredQuantity{};
};

inline constexpr std::size_t kMaximumGateCount = 32;
inline constexpr std::size_t kMaximumAllowedDefinitionHashes = 32;

[[nodiscard]] inline bool valid(const Gate& gate) noexcept {
    if (!gate.registry || gate.registry == UINT32_MAX || gate.slot > 32767
        || static_cast<std::size_t>(gate.equipmentSlot)
            >= static_cast<std::size_t>(state::account::inventory::EquipmentSlot::count)
        || (gate.allowedDefinitionHashes.empty() && !gate.requiredProfileItem)
        || gate.requiredProfileItem==UINT32_MAX
        || (gate.requiredProfileItem ? gate.requiredQuantity<=0 : gate.requiredQuantity!=0)
        || gate.allowedDefinitionHashes.size() > kMaximumAllowedDefinitionHashes) {
        return false;
    }
    for (std::size_t i = 0; i < gate.allowedDefinitionHashes.size(); ++i) {
        const auto hash = gate.allowedDefinitionHashes[i];
        if (!hash || hash == UINT32_MAX) return false;
        for (std::size_t j = 0; j < i; ++j) {
            if (gate.allowedDefinitionHashes[j] == hash) return false;
        }
    }
    return true;
}

/** Pure check against a caller-owned account snapshot; it never reads live State. */
[[nodiscard]] inline bool equipped_definition_matches(
    const Gate& gate, std::uint64_t characterId,
    const state::AccountState& snapshot) noexcept {
    if (!valid(gate) || characterId == 0
        || snapshot.characterCount > snapshot.characters.size()) {
        return false;
    }

    const state::CharacterState* character = nullptr;
    for (std::size_t index = 0; index < snapshot.characterCount; ++index) {
        if (snapshot.characters[index].soid != characterId) continue;
        if (character != nullptr) return false;
        character = &snapshot.characters[index];
    }
    if (character == nullptr) return false;

    if (gate.requiredProfileItem) {
        if (snapshot.profileItemCount>snapshot.profileItems.size()) return false;
        bool found{};
        for (std::size_t i=0;i<snapshot.profileItemCount;++i) {
            const auto& material=snapshot.profileItems[i];
            if (material.definitionHash!=gate.requiredProfileItem) continue;
            if (found || material.quantity<gate.requiredQuantity) return false;
            found=true;
        }
        if (!found) return false;
    }
    if (gate.allowedDefinitionHashes.empty()) return true;

    const auto equipmentIndex = static_cast<std::size_t>(gate.equipmentSlot);
    const auto& item = character->equipment.slots[equipmentIndex];
    if (!item.has_value() || item->instanceSoid == 0) return false;
    return std::find(gate.allowedDefinitionHashes.begin(),
                     gate.allowedDefinitionHashes.end(),
                     item->definitionHash) != gate.allowedDefinitionHashes.end();
}

/**
 * Applies only the interaction override for gated records already in a native
 * placement batch. Source identity, generation, activation and transform are
 * copied unchanged. Invalid/ambiguous authored gates fail closed atomically.
 */
[[nodiscard]] inline bool project(std::span<const Gate> gates,
    const state::AccountState& snapshot,
    middleware::bap::activity_message::native::placement::Batch& placements) noexcept {
    namespace interaction = middleware::bap::activity_message::native::interaction;
    if (gates.size() > kMaximumGateCount
        || placements.count > placements.entries.size()) return false;

    auto result = placements;
    std::uint64_t selected{};
    if (snapshot.characterCount <= snapshot.characters.size()) {
        for (std::size_t index = 0; index < snapshot.characterCount; ++index) {
            if (snapshot.characters[index].selected) {
                selected = snapshot.characters[index].soid;
                break;
            }
        }
    }
    for (std::size_t i = 0; i < gates.size(); ++i) {
        const auto& gate = gates[i];
        if (!valid(gate)) return false;
        for (std::size_t j = 0; j < i; ++j) {
            if (gates[j].registry == gate.registry && gates[j].slot == gate.slot) return false;
        }

        std::size_t matches{};
        for (std::size_t index = 0; index < result.count; ++index) {
            auto& placement = result.entries[index];
            if (placement.registry != gate.registry || placement.slot != gate.slot) continue;
            ++matches;
            placement.interactionMode = equipped_definition_matches(gate, selected, snapshot)
                ? interaction::Mode::enabled : interaction::Mode::disabled;
        }
        if (matches > 1) return false;
    }
    placements = result;
    return true;
}

} // namespace dawn::server::runtime::activity::equipment_interaction
