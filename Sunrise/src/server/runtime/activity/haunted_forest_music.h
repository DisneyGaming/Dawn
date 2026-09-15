#pragma once

#include "music_runtime.h"
#include "round_music_service.h"
#include "registry_admission.h"
#include <array>
#include <cstdint>

namespace sunrise::server::runtime::activity::haunted_forest::music_data {

namespace coo = state::activity::coo;

// Authored registry 1EB557AD/object 81550360. Keep the complete four-slot
// package set: the type-11 music descriptor is slot 1, not a new standalone
// playback override. Sense is absent on all four authored rows.
inline constexpr std::array<registry::Slot, 4> kRegistrySlots{{
    {0, 68, 0x80804F53U, UINT32_MAX, 0x80804F67U, 0x8155035AU},
    {1, 11, 0x80804E8EU, UINT32_MAX, 0x80804F58U, 0x8155035DU},
    {2, 53, 0x80804F4BU, UINT32_MAX, 0x80804F77U, 0x80F5983BU},
    {3, 35, 0x808099BDU, UINT32_MAX, 0x808099BFU, 0x8150AA3FU},
}};

inline constexpr registry::Definition kRegistry{
    "infinite_abyss", 0x81550015U, 0x1EB557ADU, 0x81550360U,
    0x47EA4CE9U, 13, kRegistrySlots, true,
};

inline constexpr std::uint32_t kGroup = 0xA91A199EU;
inline constexpr std::uint16_t kSlot = 1;
inline constexpr std::uint32_t kBoots = 0x896FF320U;
inline constexpr std::uint32_t kBoss = 0x5CFE4C22U;
inline constexpr std::uint32_t kSuddenDeath = 0xAC8DD943U;
inline constexpr std::uint32_t kVictory = 0xA1ED3A41U;
inline constexpr std::uint32_t kTrap = 0xC202C778U;

// Preserve all eight authored candidate ordinals. Unknown rows remain
// intentionally unnamed and are not selected by the round host policy.
inline constexpr std::array<std::uint32_t, 8> kCandidates{{
    kBoots, kBoss, kSuddenDeath, 0xA71967C9U, kVictory, kTrap,
    0x01806E8FU, 0x971566C3U,
}};

inline constexpr coo::Asset kAsset{
    0x1EB557ADU, 0x8155035DU, 11, kSlot,
};

inline constexpr auto kCommands = [] {
    std::array<coo::CommandSpec, kCandidates.size() + 1> output{};
    output[0] = {coo::Operation::device, kAsset, 0, coo::Wait::requested};
    for (std::size_t i = 0; i < kCandidates.size(); ++i)
        output[i + 1] = {coo::Operation::device, kAsset, kCandidates[i], coo::Wait::requested};
    return output;
}();

inline constexpr auto kSteps = [] {
    std::array<coo::Step, kCommands.size()> output{};
    output[0] = {"native_music_clear", 0, {&kCommands[0], 1}};
    for (std::size_t i = 0; i < kCandidates.size(); ++i)
        output[i + 1] = {"native_music_candidate", 0, {&kCommands[i + 1], 1}};
    return output;
}();

inline constexpr auto kGraphs = [] {
    std::array<coo::Definition, kSteps.size()> output{};
    output[0] = {"haunted_music_clear", coo::Schema::otherMissions, {&kSteps[0], 1}, {}};
    for (std::size_t i = 0; i < kCandidates.size(); ++i)
        output[i + 1] = {"haunted_music_candidate", coo::Schema::otherMissions,
            {&kSteps[i + 1], 1}, {}};
    return output;
}();

inline constexpr auto kSelections = [] {
    std::array<music::Selection, kCandidates.size() + 1> output{};
    output[0] = {0, &kGraphs[0]};
    for (std::size_t i = 0; i < kCandidates.size(); ++i)
        output[i + 1] = {kCandidates[i], &kGraphs[i + 1]};
    return output;
}();

[[nodiscard]] inline constexpr music::Definition definition(
    const registry::Definition* ownedRegistry) noexcept {
    return {ownedRegistry, kSlot, kGroup, kCandidates, kSelections};
}

[[nodiscard]] inline constexpr round_music::Binding binding(
    const music::Definition* authored) noexcept {
    return {authored, kBoots, kBoss, kSuddenDeath, kVictory, kTrap};
}

} // namespace sunrise::server::runtime::activity::haunted_forest::music_data
