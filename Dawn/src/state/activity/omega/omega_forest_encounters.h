#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace dawn::state::activity::omega::forest_encounters {
inline constexpr std::uint32_t kVexKey = 0x67AF9045U;
inline constexpr std::uint32_t kHashClass = 0x80800070U;
inline constexpr std::uint32_t kActiveValue = 0x050C5D2EU;
inline constexpr std::size_t kSwitchBits = 32 + 1 + 32 + 32;

// Selection is mission-scoped, established before generation, and independent
// of region or portal quiescence. Native authored encounters own all population.
[[nodiscard]] constexpr bool selected(std::string_view mission) noexcept {
    return mission == "mission_scot";
}
} // namespace dawn::state::activity::omega::forest_encounters
