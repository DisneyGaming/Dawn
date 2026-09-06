#pragma once

#include <array>
#include <cstdint>

namespace sunrise::state::activity::omega_enemy_chase {

struct Group final {
    std::uint16_t source;
    std::uint8_t requested;
    std::uint8_t island; // 1=A, 2=B, 3=C; initial approach remains island0.
    std::int8_t tacticalRow; // Authored obj_islands, F4D0E0B2/type3/slot21.
};

/** Reconstructed host recipe through exact authored native sources. Retail
 * frames show A:2 Dregs+1 Vandal, B:2 Psions+1 Legionary, C:3 Acolytes and
 * multiple Thralls. C's third Thrall remains a bounded reconstruction because
 * that part of the reference is occluded. No count is derived from the six
 * template-selector alternatives. Native arm-start receipts admit each batch. */
inline constexpr std::array<Group,6> kGroups{{
    {15,2,1,0}, {16,1,1,0},
    {17,2,2,1}, {18,1,2,1},
    {19,3,3,2}, {20,3,3,3},
}};
inline constexpr std::uint16_t kTacticalSlot=21;
inline constexpr std::uint32_t kTacticalDefinition=0x80F47955U;

[[nodiscard]] constexpr const Group* find(std::uint16_t source) noexcept {
    for(const auto& group:kGroups) { if(group.source==source) { return &group; } }
    return nullptr;
}

} // namespace sunrise::state::activity::omega_enemy_chase
