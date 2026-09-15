#pragma once

#include "haunted_forest_registries.h"
#include "round_environment_definition.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace sunrise::server::runtime::activity::haunted_forest::environment {

namespace mode = sunrise::server::runtime::activity::haunted_forest::mode;
namespace registry = state::activity::coo::registry;
namespace round = sunrise::server::runtime::activity::round_environment;
namespace status_effect = sunrise::server::runtime::activity::status_effect;

inline constexpr std::uint32_t kStartRegistryKey = 0x34D23982U;
inline constexpr std::uint16_t kGlassSlot = 47;
inline constexpr std::uint16_t kAttritionSlot = 48;
inline constexpr std::uint16_t kLostSlot = 49;
inline constexpr std::uint16_t kGroundedSlot = 50;
inline constexpr std::uint16_t kBlackoutSlot = 51;

inline constexpr std::array<std::uint16_t, 4> kEffectSlots{
    kGlassSlot, kAttritionSlot, kGroundedSlot, kBlackoutSlot};

inline constexpr std::uint32_t kForestPopulationSelector = 0x050C5D2EU;
inline constexpr std::uint32_t kForestVexKey = 0x67AF9045U;
inline constexpr std::uint32_t kForestCabalKey = 0x0D979BCDU;
inline constexpr std::uint32_t kForestHiveKey = 0xAD3780EEU;
inline constexpr std::uint32_t kForestFallenKey = 0x89567586U;

// These are authored/native selector values, not generated faction alternatives.
inline constexpr std::array<round::HashSwitch, 6> kForestSwitches{{
    {kForestVexKey, kForestPopulationSelector},
    {kForestCabalKey, kForestPopulationSelector},
    {kForestHiveKey, kForestPopulationSelector},
    {kForestFallenKey, kForestPopulationSelector},
    {0x045DC993U, 0xFE1640D2U},
    {0x98CBCE18U, 0xB955F8A7U},
}};

// The schedule is reconstructed host policy; the enabled bits are native-authored effects.
inline constexpr std::array<round::ModifierStage, 9> kModifierStages{{
    {1, 0, 0},
    {2, 1U << 0, 1},
    {3, 1U << 1, 2},
    {4, 1U << 2, 3},
    {5, 1U << 3, 4},
    {6, (1U << 3) | (1U << 0), 5},
    {7, (1U << 3) | (1U << 1), 6},
    {8, (1U << 3) | (1U << 2), 7},
    {9, 0x0FU, 8},
}};

inline constexpr auto kStages = kModifierStages;

constexpr bool recovered_effect_slots() noexcept {
    for (std::size_t i = 0; i < kEffectSlots.size(); ++i) {
        const auto& slot = mode::kStartSlots[kEffectSlots[i]];
        if (slot.index != kEffectSlots[i] || slot.type != 26
            || slot.componentClass != 0x8080953FU || slot.authSchema != 0x8080954BU)
            return false;
    }
    const auto& omitted = mode::kStartSlots[kLostSlot];
    return omitted.index == kLostSlot && omitted.type == 26;
}
static_assert(mode::kRegistries[0].key == kStartRegistryKey);
static_assert(recovered_effect_slots());

/**
 * Value-owned mission data. The effect registry pointers are deliberately
 * filled by data_factory from the caller's admitted NativeDefinition array.
 */
struct Data final {
    std::array<status_effect::Capability, 4> effects{};
    std::span<const round::ModifierStage> stages{
        kModifierStages.data(), kModifierStages.size()};
    std::span<const round::HashSwitch> forestSwitches{
        kForestSwitches.data(), kForestSwitches.size()};

    [[nodiscard]] constexpr round::Definition definition() const noexcept {
        return {std::span<const status_effect::Capability>(effects), stages, forestSwitches};
    }
};

/** Builds immutable Haunted inputs against one already-admitted start registry. */
[[nodiscard]] inline constexpr Data data_factory(
    const registry::Definition* ownedStartRegistry) noexcept {
    Data output{};
    for (std::size_t i = 0; i < output.effects.size(); ++i)
        output.effects[i] = {ownedStartRegistry, kEffectSlots[i]};
    return output;
}

[[nodiscard]] inline constexpr Data data_factory(
    const registry::Definition& ownedStartRegistry) noexcept {
    return data_factory(&ownedStartRegistry);
}

[[nodiscard]] inline constexpr Data make_data(
    const registry::Definition* ownedStartRegistry) noexcept {
    return data_factory(ownedStartRegistry);
}

} // namespace sunrise::server::runtime::activity::haunted_forest::environment
