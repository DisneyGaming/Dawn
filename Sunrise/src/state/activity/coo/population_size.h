#pragma once
#include <array>
#include <cstdint>

namespace sunrise::state::activity::coo::population_size {
// Opt-in count helper, currently used only by Gateway's three platform
// reinforcement sets. All other sources and all waves keep authored counts.
// Native category count is template metadata, not
// the number of actors requested. Keep the second category at one (often a
// specialist); the first category supplies the additional ordinary combatant.
constexpr std::array<std::uint8_t,2> reinforcement(std::uint8_t categories) noexcept {
    return {2,static_cast<std::uint8_t>(categories==2?1:0)};
}
constexpr std::uint8_t total(std::uint8_t categories) noexcept {
    const auto n=reinforcement(categories);return static_cast<std::uint8_t>(n[0]+n[1]);
}
constexpr std::uint8_t first(std::uint8_t total,std::uint8_t categories) noexcept {
    return static_cast<std::uint8_t>(total-(categories==2?1:0));
}
}
