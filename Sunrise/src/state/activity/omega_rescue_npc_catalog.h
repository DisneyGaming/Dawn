#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

// Authored 80F47B1D Scene controllers and cast providers. Generated from the
// package-verified omega_npc_evidence.json; selector-less/debug Scenes excluded.
namespace sunrise::state::activity::omega_rescue_npc {

inline constexpr std::uint32_t kRegistry=0x99BD2FEBU;
inline constexpr std::uint32_t kRegistryTag=0x80F47B1DU;

struct Binding final { std::uint8_t type; std::uint16_t slot; };
struct Scene final {
    std::uint16_t slot;
    std::uint32_t definition,selector;
    std::string_view name;
    std::span<const Binding> bindings;
};
struct Source final { std::uint16_t slot; std::uint32_t definition; };

inline constexpr std::array<Binding,20> kBindings0{{
    {1,0},
    {1,5},
    {1,6},
    {1,7},
    {1,8},
    {48,134},
    {48,133},
    {4,14},
    {4,15},
    {4,16},
    {4,17},
    {4,19},
    {4,20},
    {4,21},
    {4,22},
    {4,23},
    {4,24},
    {4,18},
    {48,135},
    {48,132},
}};

inline constexpr std::array<Binding,3> kBindings1{{
    {48,133},
    {1,0},
    {48,134},
}};

inline constexpr std::array<Binding,3> kBindings2{{
    {48,140},
    {1,38},
    {48,131},
}};

inline constexpr std::array<Binding,28> kBindings3{{
    {1,38},
    {1,40},
    {1,41},
    {1,42},
    {1,43},
    {1,44},
    {1,45},
    {48,141},
    {4,49},
    {4,50},
    {4,51},
    {4,52},
    {4,53},
    {4,54},
    {4,55},
    {4,56},
    {4,57},
    {4,59},
    {4,58},
    {4,60},
    {48,142},
    {4,61},
    {4,62},
    {4,63},
    {4,64},
    {48,130},
    {1,39},
    {4,65},
}};

inline constexpr std::array<Binding,4> kBindings4{{
    {48,127},
    {4,70},
    {4,71},
    {4,72},
}};

inline constexpr std::array<Binding,12> kBindings5{{
    {1,77},
    {48,147},
    {1,78},
    {1,79},
    {48,148},
    {48,125},
    {1,80},
    {48,126},
    {4,73},
    {4,74},
    {4,75},
    {4,76},
}};

inline constexpr std::array<Binding,2> kBindings6{{
    {48,128},
    {4,69},
}};

inline constexpr std::array<Binding,2> kBindings7{{
    {48,120},
    {4,96},
}};

inline constexpr std::array<Binding,12> kBindings8{{
    {1,85},
    {48,149},
    {1,86},
    {1,87},
    {48,150},
    {48,151},
    {1,88},
    {48,119},
    {4,97},
    {4,98},
    {4,99},
    {4,100},
}};

inline constexpr std::array<Binding,4> kBindings9{{
    {48,121},
    {4,101},
    {4,102},
    {4,103},
}};

inline constexpr std::array<Binding,7> kBindings10{{
    {48,122},
    {4,104},
    {4,105},
    {4,106},
    {1,93},
    {1,94},
    {1,95},
}};

inline constexpr std::array<Scene,11> kScenes{{
    {9,0x80F479BFU,0x80EC0DD0U,"scene_osiris_intro",kBindings0},
    {27,0x80F479F5U,0x80EC0DD5U,"scene_osiris_stop_boss_attack",kBindings1},
    {33,0x80F47A08U,0x80EC0DD5U,"scene_osiris_stop_boss_attack_2nd_location",kBindings2},
    {46,0x80F47A37U,0x80EC0DCBU,"scene_osiris_damages_boss",kBindings3},
    {66,0x80F47A73U,0x80EC0DB6U,"scene_echo_by_dunk_pickup_2nd_phase",kBindings4},
    {67,0x80F47A76U,0x80EC0DBDU,"scene_echo_by_second_mancannon_2nd_phase",kBindings5},
    {68,0x80F47A79U,0x80EC0DB9U,"scene_echo_by_first_mancannon_2nd_phase",kBindings6},
    {81,0x80F47AA4U,0x80EC0DB9U,"scene_echo_by_first_mancannon",kBindings7},
    {82,0x80F47AA7U,0x80EC0DBDU,"scene_echo_by_second_mancannon",kBindings8},
    {83,0x80F47AAAU,0x80EC0DB2U,"scene_echo_by_dunk_pickup",kBindings9},
    {84,0x80F47AADU,0x80EC0DC5U,"scene_echo_by_exit_mancannon",kBindings10},
}};

inline constexpr std::array<Source,24> kSources{{
    {0,0x80F4799DU}, // squad_osiris
    {5,0x80F479B0U}, // squad_vex_combatant_osiris_kills_1
    {6,0x80F479B4U}, // squad_vex_combatant_osiris_kills_2
    {7,0x80F479B8U}, // squad_vex_combatant_osiris_kills_3
    {8,0x80F479BCU}, // squad_vex_combatant_osiris_kills_4
    {38,0x80F47A18U}, // squad_osiris_hurt_boss
    {39,0x80F47A1CU}, // squad_osiris_hurt_boss_2
    {40,0x80F47A20U}, // squad_echo_1
    {41,0x80F47A24U}, // squad_echo_2
    {42,0x80F47A28U}, // squad_echo_3
    {43,0x80F47A2CU}, // squad_echo_4
    {44,0x80F47A30U}, // squad_echo_5
    {45,0x80F47A34U}, // squad_echo_6
    {77,0x80F47A95U}, // squad_echo_by_second_mancannon_2nd_phase
    {78,0x80F47A99U}, // squad_echo_by_second_mancannon_2_2nd_phase
    {79,0x80F47A9DU}, // squad_echo_by_second_mancannon_3_2nd_phase
    {80,0x80F47AA1U}, // squad_echo_by_second_mancannon_4_2nd_phase
    {85,0x80F47AB1U}, // squad_echo_by_second_mancannon
    {86,0x80F47AB5U}, // squad_echo_by_second_mancannon_2
    {87,0x80F47AB9U}, // squad_echo_by_second_mancannon_3
    {88,0x80F47ABDU}, // squad_echo_by_second_mancannon_4
    {93,0x80F47ACEU}, // squad_exit_mancannon_echo_1
    {94,0x80F47AD2U}, // squad_exit_mancannon_echo_2
    {95,0x80F47AD5U}, // squad_exit_mancannon_echo_3
}};

[[nodiscard]] constexpr const Scene* find_scene(std::uint16_t slot) noexcept {
    for(const auto& scene:kScenes) { if(scene.slot==slot) { return &scene; } }
    return nullptr;
}
[[nodiscard]] constexpr const Source* find_source(std::uint16_t slot) noexcept {
    for(const auto& source:kSources) { if(source.slot==slot) { return &source; } }
    return nullptr;
}
[[nodiscard]] constexpr std::size_t source_count(const Scene& scene) noexcept {
    std::size_t count{};
    for(const auto& binding:scene.bindings) { count+=binding.type==1?1U:0U; }
    return count;
}

} // namespace sunrise::state::activity::omega_rescue_npc
