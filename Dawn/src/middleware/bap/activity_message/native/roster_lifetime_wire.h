#pragma once
#include "../sensor_auth_update.h"

namespace dawn::middleware::bap::activity_message::sensor_auth_update::lifetime_wire {
// These are full encoded state bytes. Empty metadata preserves legacy encoding.
[[nodiscard]] inline bool valid_fields(std::size_t count,
    std::span<const std::uint8_t> presence, std::span<const std::uint8_t> states) noexcept {
    if ((!presence.empty() && presence.size() != count)
        || (!states.empty() && states.size() != count)) return false;
    for (auto value : presence) if (value > 1) return false;
    for (auto value : states) if (value < kStateByteBias) return false;
    return true;
}
[[nodiscard]] inline bool valid(const Roster& roster) noexcept {
    const auto count = top_level_key_count(roster);
    if (count > kDeltaMaskWords * 32
        || (roster.topLevelKeys.empty()
            && (!roster.topLevelPresence.empty() || !roster.topLevelStates.empty()))
        || !valid_fields(count, roster.topLevelPresence, roster.topLevelStates)) return false;
    for (const auto& block : roster.bubbleSubBlocks)
        if (!valid_fields(block.keys.size(), block.presence, block.states)) return false;
    return true;
}
[[nodiscard]] inline std::uint32_t mask(std::size_t count,
    std::span<const std::uint8_t> presence, std::size_t word) noexcept {
    std::uint32_t result{};
    const auto start = word * 32;
    for (std::size_t bit = 0; bit < 32 && start + bit < count; ++bit)
        if (presence.empty() || presence[start + bit]) result |= std::uint32_t{1} << bit;
    return result;
}
[[nodiscard]] inline std::uint8_t state(std::span<const std::uint8_t> states,
    std::size_t ordinal, std::uint8_t fallback) noexcept {
    return states.empty() ? static_cast<std::uint8_t>(kStateByteBias + fallback) : states[ordinal];
}
}
