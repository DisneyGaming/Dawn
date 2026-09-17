#pragma once
#include <cstring>
#include <span>
#include "../../../state/activity/omega/omega_lair_authority.h"
#include "omega_reveal_source.h"

namespace dawn::client::hooks::bootflow::omega_lair_delivery {
namespace authority = state::activity::omega::lair_authority;
inline constexpr std::size_t kBodyBytes = 0xC4, kComponentBytes = 0x690;
template<class T> T field(std::span<const std::byte> bytes, std::size_t offset) noexcept {
    T value{};
    if (offset <= bytes.size() && sizeof value <= bytes.size() - offset)
        std::memcpy(&value, bytes.data() + offset, sizeof value);
    return value;
}
inline const authority::Source* source(std::span<const std::byte> component) noexcept {
    if (component.size() < kComponentBytes) return nullptr;
    for (const auto& candidate : authority::kSources)
        if (omega_reveal_source::matches(component, {candidate.definition, 0x8080948FU, 0x728})
            && field<std::uint32_t>(component, 0x5D8) == authority::kRegistry
            && field<std::uint8_t>(component, 0x5DC) == 1
            && field<std::uint16_t>(component, 0x5DE) == candidate.slot)
            return &candidate;
    return nullptr;
}
// Admit only the current first-wave request decoded for this exact native binding.
inline bool pending(const authority::Source& source, std::uint32_t generation,
                    bool leftStarted, std::span<const std::byte> object,
                    std::span<const std::byte> body) noexcept {
    return leftStarted && generation && generation <= 0x7FFFFFFFU
        && object.size() >= 0x70 && body.size() == kBodyBytes
        && field<std::uint32_t>(object, 0) == authority::kRegistry
        && field<std::uint8_t>(object, 4) == 1
        && field<std::uint16_t>(object, 6) == source.slot
        && field<std::uint32_t>(object, 0xC) == 0x80807EC9U
        && field<std::uint32_t>(object, 0x68) == 14
        && field<std::uint8_t>(object, 0x18) == 0
        && field<std::uint8_t>(object, 0x6E) == 1
        && field<std::uint32_t>(body, 0) == authority::kRegistry
        && field<std::uint8_t>(body, 4) == 3
        && field<std::uint16_t>(body, 6) == 0
        && field<std::uint32_t>(body, 0x2C) == 1
        && field<std::uint32_t>(body, 0x30) == 2
        && field<std::uint32_t>(body, 0x7C) == generation
        && field<std::uint32_t>(body, 0xA0) == authority::kRegistry
        && field<std::uint8_t>(body, 0xA4) == 66
        && field<std::uint16_t>(body, 0xA6) == source.rule
        && field<std::uint32_t>(body, 0xB4) == source.tacticalRow
        && field<std::uint8_t>(body, 0xBC) == 0
        && field<std::uint8_t>(body, 0xBD) == 0;
}
inline bool adopted(std::span<const std::byte> component,
                    std::span<const std::byte> body) noexcept {
    return component.size() >= kComponentBytes && body.size() == kBodyBytes
        && std::memcmp(component.data() + 0x180, body.data(), kBodyBytes) == 0;
}
} // namespace dawn::client::hooks::bootflow::omega_lair_delivery
