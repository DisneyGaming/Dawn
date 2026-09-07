#pragma once

#include "coo/native_device_authority.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace sunrise::state::activity::omega_first_mancannon {

inline constexpr std::uint32_t kRegistry = 0x95FB2E01U;
inline constexpr std::uint8_t kSlotType = 4;
inline constexpr std::uint16_t kSlot = 10;
inline constexpr std::size_t kAuthorityBits = 252;

struct Cannon {
    std::uint8_t index;
    std::uint16_t coreSlot, fxSlot, gateSlot;
    std::uint32_t coreDefinition, fxDefinition, gateDefinition, entity;
};

// Exact 95FB2E01 authored slots. Each gate's +58 GUID matches its own FX
// candidate's +70 GUID. Gate +38 is 14 in all four definitions, not a slot.
inline constexpr std::array<Cannon, 4> kCannons{{
    {0, 10, 14, 18, 0x80F47588U, 0x80F47594U, 0x80F475A0U, 0x80F44F46U},
    {1, 11, 15, 19, 0x80F4758BU, 0x80F47597U, 0x80F475A3U, 0x80F44F50U},
    {2, 12, 16, 20, 0x80F4758EU, 0x80F4759AU, 0x80F475A6U, 0x80F44F55U},
    {3, 13, 17, 21, 0x80F47591U, 0x80F4759DU, 0x80F475A9U, 0x80F44F4BU},
}};

[[nodiscard]] constexpr const Cannon* source(std::uint16_t slot) noexcept {
    for (const auto& value : kCannons) {
        if (value.coreSlot == slot || value.fxSlot == slot) { return &value; }
    }
    return nullptr;
}
[[nodiscard]] constexpr const Cannon* gate(std::uint16_t slot) noexcept {
    for (const auto& value : kCannons) {
        if (value.gateSlot == slot) { return &value; }
    }
    return nullptr;
}
[[nodiscard]] constexpr bool active(const Cannon& value, bool firstThree,
                                     bool finalCannon) noexcept {
    return value.index == 3 ? finalCannon : firstThree;
}

struct Preparation {
    std::uint32_t generation{};
    std::uint8_t index{};
};

/** Pure check of bounded post-original copies; no native pointers are retained. */
[[nodiscard]] inline bool preparation(std::span<const std::byte, 16> prefix,
    std::span<const std::byte, 0x44> state, Preparation& receipt) noexcept {
    receipt = {};
    const auto read32 = [](const std::byte* bytes) noexcept {
        std::uint32_t value{};
        std::memcpy(&value, bytes, sizeof value);
        return value;
    };
    const auto read64 = [](const std::byte* bytes) noexcept {
        std::uint64_t value{};
        std::memcpy(&value, bytes, sizeof value);
        return value;
    };
    const Cannon* cannon{};
    for (const auto& value : kCannons) {
        if (value.coreDefinition == read32(prefix.data())) { cannon = &value; break; }
    }
    // The preceding package marker is absent at runtime and is not inspected.
    if (cannon == nullptr || read32(prefix.data() + 4) != 0x80809928U
        || read64(prefix.data() + 8) != 0x4C8U) { return false; }
    if(!coo::native_device::inactive_state(state)) { return false; }
    const auto generation = read32(state.data());
    receipt = {generation, cannon->index};
    return true;
}

/** Native 8080992F authority for each chase cannon's sole candidate.
 * Enable the first three after the initial-area clear and boss relocation;
 * the fourth has its own later encounter milestone.
 * Publish active=false first, then active=true at the established milestone.
 * Native creation reads the previously applied state before copying the new one.
 * Keep generation stable across retransmission; host phase ownership is external.
 * No transform override: use each complete authored candidate placement.
 */
template<class Writer>
[[nodiscard]] bool write_authority(Writer& writer, std::uint32_t generation,
                                   bool active) noexcept {
    return coo::native_device::object(writer,generation,active);
}

} // namespace sunrise::state::activity::omega_first_mancannon
