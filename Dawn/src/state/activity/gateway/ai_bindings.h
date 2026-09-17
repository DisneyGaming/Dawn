// Generated from package tactical groups and placement policy by generate_gateway_ai.py.
#pragma once
#include "traversal_catalog.h"
#include "../coo/native_combatant_authority.h"
namespace dawn::state::activity::gateway {
struct TacticalJoin { std::uint32_t registry; std::uint16_t source,group; std::int8_t row; };
inline constexpr TacticalJoin kTacticalJoins[]{
    {0x4B946B28U,45,17,1}, // gate_goto_outskirts_east_support_a_squad
    {0x4B946B28U,46,17,1}, // gate_goto_outskirts_east_support_b_squad
    {0x4B946B28U,47,17,5}, // gate_goto_outskirts_east_support_c_squad
    {0x4B946B28U,48,17,5}, // gate_goto_outskirts_east_support_d_squad
    {0x4B946B28U,49,16,4}, // gate_goto_lighthouse_center_anchor_a_squad
    {0x4B946B28U,51,16,4}, // gate_goto_lighthouse_center_support_a_squad
    {0x4B946B28U,52,16,2}, // gate_goto_lighthouse_center_support_b_squad
    {0x4B946B28U,53,16,6}, // gate_goto_lighthouse_center_east_support_a_squad
    {0x4B946B28U,54,16,6}, // gate_goto_lighthouse_center_east_support_b_squad
    {0x4B946B28U,55,16,9}, // gate_goto_lighthouse_center_east_support_c_squad
    {0x4B946B28U,56,16,9}, // gate_goto_lighthouse_center_east_support_d_squad
    {0x4B946B28U,57,16,14}, // gate_goto_lighthouse_center_west_support_a_squad
    {0x4B946B28U,58,16,14}, // gate_goto_lighthouse_center_west_support_b_squad
    {0x4B946B28U,59,16,9}, // gate_goto_lighthouse_center_west_support_c_squad
    {0x4B946B28U,60,16,9}, // gate_goto_lighthouse_center_west_support_d_squad
    {0x4B946B28U,61,17,8}, // gate_goto_outskirts_west_support_a_squad
    {0x4B946B28U,62,17,8}, // gate_goto_outskirts_west_support_b_squad
    {0x4B946B28U,63,17,11}, // gate_goto_outskirts_west_support_c_squad
    {0x4B946B28U,64,17,13}, // gate_goto_outskirts_west_support_d_squad
    {0x4B946B28U,65,22,0}, // tower_return_lighthouse_center_anchor_a_squad
    {0x4B946B28U,67,22,0}, // tower_return_lighthouse_center_support_a_squad
    {0x4B946B28U,68,22,0}, // tower_return_lighthouse_center_support_b_squad
    {0x4B946B28U,69,22,0}, // tower_return_lighthouse_center_melee_a_squad
    {0x4B946B28U,70,22,0}, // tower_return_lighthouse_center_melee_b_squad
    {0x4B946B28U,71,22,0}, // tower_return_lighthouse_center_melee_c_squad
    {0x4B946B28U,72,22,0}, // tower_return_lighthouse_center_melee_d_squad
    {0x4B946B28U,73,22,6}, // tower_return_lighthouse_center_east_support_a_squad
    {0x4B946B28U,74,22,8}, // tower_return_lighthouse_center_east_support_b_squad
    {0x4B946B28U,75,22,10}, // tower_return_lighthouse_center_west_support_a_squad
    {0x4B946B28U,76,22,12}, // tower_return_lighthouse_center_west_support_b_squad
    {0x4B946B28U,77,21,0}, // tower_return_outskirts_east_support_a_squad
    {0x4B946B28U,78,21,1}, // tower_return_outskirts_east_support_b_squad
    {0x4B946B28U,79,21,3}, // tower_return_outskirts_east_support_c_squad
    {0x4B946B28U,80,21,3}, // tower_return_outskirts_east_support_d_squad
    {0x4B946B28U,81,21,6}, // tower_return_outskirts_east_ranged_a_squad
    {0x4B946B28U,82,21,6}, // tower_return_outskirts_east_ranged_b_squad
    {0x4B946B28U,83,21,10}, // tower_return_outskirts_west_support_a_squad
    {0x4B946B28U,84,21,11}, // tower_return_outskirts_west_support_b_squad
    {0x4B946B28U,85,21,12}, // tower_return_outskirts_west_support_c_squad
    {0x4B946B28U,86,21,12}, // tower_return_outskirts_west_support_d_squad
    {0x4B946B28U,87,21,16}, // tower_return_outskirts_west_ranged_a_squad
    {0x4B946B28U,88,21,16}, // tower_return_outskirts_west_ranged_b_squad
    {0x4B946B28U,89,18,3}, // tower_finale_wave_1_support_a_squad
    {0x4B946B28U,90,18,3}, // tower_finale_wave_1_support_b_squad
    {0x4B946B28U,91,18,3}, // tower_finale_wave_1_support_c_squad
    {0x4B946B28U,92,18,0}, // tower_finale_wave_1_support_d_squad
    {0x4B946B28U,93,19,1}, // tower_finale_wave_2_melee_a_squad
    {0x4B946B28U,94,19,0}, // tower_finale_wave_2_melee_b_squad
    {0x4B946B28U,95,19,1}, // tower_finale_wave_2_melee_c_squad
    {0x4B946B28U,96,19,1}, // tower_finale_wave_2_ranged_a_squad
    {0x4B946B28U,97,19,1}, // tower_finale_wave_2_ranged_b_squad
    {0x4B946B28U,98,19,0}, // tower_finale_wave_2_vignette_a_squad
    {0x4B946B28U,99,20,8}, // tower_finale_wave_3_anchor_a_squad
    {0x4B946B28U,101,20,4}, // tower_finale_wave_3_support_a_squad
    {0x4B946B28U,102,20,8}, // tower_finale_wave_3_support_b_squad
    {0x4B946B28U,103,20,1}, // tower_finale_wave_3_support_c_squad
    {0x4B946B28U,104,20,1}, // tower_finale_wave_3_support_d_squad
    {0x4B946B28U,105,20,1}, // tower_finale_wave_3_support_f_squad
    {0x4B946B28U,106,20,1}, // tower_finale_wave_3_support_g_squad
    {0x85742F3EU,206,5,0}, // leadup_end_anchor_a_squad
    {0x85742F3EU,208,5,10}, // leadup_end_support_a_squad
    {0x85742F3EU,209,5,0}, // leadup_end_support_b_squad
    {0x85742F3EU,210,5,8}, // leadup_end_ranged_a_squad
    {0x85742F3EU,212,5,10}, // leadup_end_ranged_b_squad
    {0x85742F3EU,214,6,0}, // leadup_recess_anchor_a_squad
    {0x85742F3EU,216,6,4}, // leadup_recess_support_a_a_squad
    {0x85742F3EU,217,6,4}, // leadup_recess_support_a_b_squad
    {0x85742F3EU,218,6,1}, // leadup_recess_support_a_c_squad
    {0x85742F3EU,219,6,2}, // leadup_recess_support_b_a_squad
    {0x85742F3EU,220,6,1}, // leadup_recess_support_b_b_squad
    {0x85742F3EU,221,6,2}, // leadup_recess_support_b_c_squad
    {0x85742F3EU,222,6,0}, // leadup_recess_support_c_squad
    {0x85742F3EU,223,7,0}, // leadup_shelf_melee_a_a_squad
    {0x85742F3EU,224,7,0}, // leadup_shelf_melee_a_b_squad
    {0x85742F3EU,225,7,0}, // leadup_shelf_melee_a_c_squad
    {0x85742F3EU,226,7,0}, // leadup_shelf_melee_b_a_squad
    {0x85742F3EU,227,7,0}, // leadup_shelf_melee_b_b_squad
    {0x85742F3EU,228,7,2}, // leadup_shelf_melee_b_c_squad
    {0x85742F3EU,229,7,1}, // leadup_shelf_melee_c_squad
    {0x85742F3EU,230,7,1}, // leadup_shelf_melee_d_squad
    {0x85742F3EU,231,7,0}, // leadup_shelf_support_a_squad
    {0x85742F3EU,232,7,1}, // leadup_shelf_support_b_squad
};
[[nodiscard]] constexpr coo::native_combatant::TacticalGroup tactical_group(const Spawn& source) noexcept {
    if(source.registry==kTraversalRegistry && source.cohort==0) { return {source.registry,static_cast<std::uint16_t>(24+((source.source-16)/18)*18),0}; }
    for(const auto& join:kTacticalJoins) { if(join.registry==source.registry && join.source==source.source) { return {join.registry,join.group,join.row}; } }
    return {};
}
static_assert([] { for(const auto& source:kSpawns) { if(tactical_group(source).row<0) { return false; } } return true; }());
}
