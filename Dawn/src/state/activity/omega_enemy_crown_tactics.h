#pragma once

#include "omega_combatant_authority.h"
#include "omega_enemy_crown_catalog.h"

namespace dawn::state::activity::omega_enemy_crown_tactics {

/** Reconstructed host row assignment using native placement containment and
 * authored side names. Native multi-provider selection/capacities remain owned
 * by obj_all. Overlapping final/side areas do not prove the original host row
 * assignment; these are scoped policy choices, not native capacity overrides. */
[[nodiscard]] constexpr omega_combatant_authority::TacticalGroup for_source(
    std::uint32_t registry,std::uint8_t type,std::uint16_t slot) noexcept {
    const auto* catalog=omega_enemy_crown::find_registry(registry);
    if(catalog==nullptr || type!=1) { return {}; }
    std::int8_t row{-1};
    if(registry==omega_enemy_crown::kRegistry) {switch(slot) {
    case 0: case 10: case 11: case 12: case 13: case 14: case 15: row=3; break; // fa_final
    case 2: case 3: row=4; break; // fa_pit, provider93
    case 4: case 5: row=5; break; // native left provider row99/100
    case 6: case 7: row=6; break; // native right provider row101/102
    case 8: case 9: row=1; break; // fa_back_4, provider86
    default:return {};
    }} else if(registry==omega_enemy_crown::kHiveRegistry) {switch(slot) {
    case 5: row=3; break; // Ergoth: fa_final
    case 7: row=4; break; // Acolytes: pit
    case 8: row=5; break; // initial Thralls: native melee
    case 9: case 10: row=7; break; // left
    case 11: case 12: row=8; break; // right
    case 13: case 14: row=1; break; // back
    case 15: case 16: case 18: case 19: case 20: row=9; break; // native broad distribution
    case 17: row=6; break; // reinforcement Thralls: native melee
    default:return {};
    }} else if(registry==omega_enemy_crown::kVexRegistry) {switch(slot) {
    case 4: row=2; break; // Alecto: authored platform6 containing anchor
    case 12: case 13: row=1; break; // Fanatics: native melee area
    case 6: case 7: case 8: case 9: case 10: case 11:
    case 14: case 15: case 16: case 17: case 18: row=6; break; // native platform distribution
    default:return {};
    }} else {switch(slot) {
    case 0: row=2; break; // Psion front-left
    case 1: row=3; break; // Psion front-right
    case 2: case 3: row=4; break; // native broad distribution
    case 4: row=0; break; // Phalanx anchor: back
    case 6: case 7: case 8: case 9: case 10: case 11: row=1; break; // War Beasts: melee
    default:return {};
    }}
    return {registry,catalog->tacticalSlot,row};
}

} // namespace dawn::state::activity::omega_enemy_crown_tactics
