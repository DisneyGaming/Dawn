#pragma once

#include "omega_combatant_authority.h"
#include "omega_enemy_chase_catalog.h"

namespace dawn::state::activity::omega_enemy_lair_tactics {

/** Reconstructed host assignments using the authored first-Lair firing areas.
 * Six sources have unambiguous area matches. The five overlapping rear sources
 * share row1's broad provider51, spanning fa_left/right/back/back_1, without
 * inventing a narrower role or capacity subdivision. The original host
 * source-to-row script is not recovered; these assignments are host policy.
 * obj_all is F4D0E0B2/3/0. Its native scheduler chooses each row's provider.
 * No tactical-group reset counter or actor/nav transform is overridden. */
[[nodiscard]] constexpr omega_combatant_authority::TacticalGroup for_source(
    std::uint32_t registry,std::uint8_t type,std::uint16_t slot) noexcept {
    if(registry!=0xF4D0E0B2U || type!=1) { return {}; }
    // obj_islands owns the later firing areas. A/B are unique spatial matches;
    // C's overlapping area1/2 assignment follows the authored source names.
    if(const auto* chase=omega_enemy_chase::find(slot)) {
        return {registry,omega_enemy_chase::kTacticalSlot,chase->tacticalRow};
    }
    std::int8_t row{-1};
    switch(slot) {
    case 1: case 11: case 12: case 13: case 14: row=1; break; // broad rear: anchor, waveB, guards
    case 3: case 4: row=6; break; // fa_mid_2: sq_front_1/2
    case 7: row=9; break; // fa_left_1: sq_side_1
    case 8: row=10; break; // fa_right_1: sq_side_2
    case 9: row=4; break; // fa_left_2: sq_sniper_left
    case 10: row=5; break; // fa_right_2: sq_sniper_right
    default: return {};
    }
    return {registry,0,row};
}

} // namespace dawn::state::activity::omega_enemy_lair_tactics
