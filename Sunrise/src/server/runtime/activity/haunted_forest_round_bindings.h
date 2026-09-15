#pragma once

#include <array>
#include <cstdint>

#include "haunted_forest_registries.h"

namespace sunrise::server::runtime::activity::haunted_forest::mode {
namespace coo = state::activity::coo;

// This is authored identity data only. The source inventory is an ignored,
// generated local artifact; raw bytes and localized game strings are not copied.
// These bindings are inert until a separately authorized consumer selects them.
inline constexpr std::array<registry::Slot,133> kArenaSlots{{
{129,5,0x80804F01,0xFFFFFFFF,0x80804F04,0x8155018E}, // seq_boss_killed
{156,66,0x808094CF,0xFFFFFFFF,0xFFFFFFFF,0x81550191}, // spawnrule_1b
{157,66,0x808094CF,0xFFFFFFFF,0xFFFFFFFF,0x81550194}, // spawnrule_1a
{0,1,0x80809A3B,0x80807ECC,0x80807EC9,0x8155019A}, // sq_cabal_intro[0]
{1,1,0x80809A3B,0x80807ECC,0x80807EC9,0x8155019D}, // sq_cabal_intro[1]
{2,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815501A0}, // sq_cabal_intro[2]
{3,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815501A3}, // sq_cabal_intro[3]
{4,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815501A6}, // sq_cabal_intro[4]
{5,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815501A9}, // sq_cabal_intro[5]
{6,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815501AC}, // sq_cabal_intro_a[0]
{7,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815501AF}, // sq_cabal_intro_a[1]
{8,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815501B2}, // sq_cabal_intro_a[2]
{9,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815501B5}, // sq_cabal_intro_a[3]
{10,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815501B8}, // sq_cabal_intro_a[4]
{11,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815501BB}, // sq_cabal_intro_a[5]
{12,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815501BE}, // sq_cabal_mid[0]
{13,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815501C1}, // sq_cabal_mid[1]
{14,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815501C4}, // sq_cabal_mid[2]
{15,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815501C7}, // sq_cabal_mid[3]
{16,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815501CA}, // sq_cabal_mid[4]
{17,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815501CD}, // sq_cabal_mid[5]
{18,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815501D0}, // sq_cabal_mid_a[0]
{19,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815501D3}, // sq_cabal_mid_a[1]
{20,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815501D6}, // sq_cabal_mid_a[2]
{21,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815501D9}, // sq_cabal_mid_a[3]
{22,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815501DC}, // sq_cabal_mid_a[4]
{23,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815501DF}, // sq_cabal_mid_a[5]
{24,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815501E2}, // sq_hive_intro[0]
{25,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815501E5}, // sq_hive_intro[1]
{26,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815501E8}, // sq_hive_intro[2]
{27,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815501EB}, // sq_hive_intro[3]
{28,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815501EE}, // sq_hive_intro[4]
{29,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815501F1}, // sq_hive_intro[5]
{30,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815501F4}, // sq_hive_intro_a[0]
{31,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815501F7}, // sq_hive_intro_a[1]
{32,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815501FA}, // sq_hive_intro_a[2]
{33,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815501FD}, // sq_hive_intro_a[3]
{34,1,0x80809A3B,0x80807ECC,0x80807EC9,0x81550200}, // sq_hive_intro_a[4]
{35,1,0x80809A3B,0x80807ECC,0x80807EC9,0x81550203}, // sq_hive_intro_a[5]
{36,1,0x80809A3B,0x80807ECC,0x80807EC9,0x81550206}, // sq_hive_mid[0]
{37,1,0x80809A3B,0x80807ECC,0x80807EC9,0x81550209}, // sq_hive_mid[1]
{38,1,0x80809A3B,0x80807ECC,0x80807EC9,0x8155020C}, // sq_hive_mid[2]
{39,1,0x80809A3B,0x80807ECC,0x80807EC9,0x8155020F}, // sq_hive_mid[3]
{40,1,0x80809A3B,0x80807ECC,0x80807EC9,0x81550212}, // sq_hive_mid[4]
{41,1,0x80809A3B,0x80807ECC,0x80807EC9,0x81550215}, // sq_hive_mid[5]
{42,1,0x80809A3B,0x80807ECC,0x80807EC9,0x81550218}, // sq_hive_mid_a[0]
{43,1,0x80809A3B,0x80807ECC,0x80807EC9,0x8155021B}, // sq_hive_mid_a[1]
{44,1,0x80809A3B,0x80807ECC,0x80807EC9,0x8155021E}, // "|6a&|{|sq_hive_mid_a[2]
{45,1,0x80809A3B,0x80807ECC,0x80807EC9,0x81550221}, // sq_hive_mid_a[3]
{46,1,0x80809A3B,0x80807ECC,0x80807EC9,0x81550224}, // sq_hive_mid_a[4]
{47,1,0x80809A3B,0x80807ECC,0x80807EC9,0x81550227}, // sq_hive_mid_a[5]
{48,1,0x80809A3B,0x80807ECC,0x80807EC9,0x8155022A}, // sq_vex_intro[0]
{49,1,0x80809A3B,0x80807ECC,0x80807EC9,0x8155022D}, // sq_vex_intro[1]
{50,1,0x80809A3B,0x80807ECC,0x80807EC9,0x81550230}, // sq_vex_intro[2]
{51,1,0x80809A3B,0x80807ECC,0x80807EC9,0x81550233}, // sq_vex_intro[3]
{52,1,0x80809A3B,0x80807ECC,0x80807EC9,0x81550236}, // sq_vex_intro[4]
{53,1,0x80809A3B,0x80807ECC,0x80807EC9,0x81550239}, // sq_vex_intro[5]
{54,1,0x80809A3B,0x80807ECC,0x80807EC9,0x8155023C}, // sq_vex_intro_a[0]
{55,1,0x80809A3B,0x80807ECC,0x80807EC9,0x8155023F}, // sq_vex_intro_a[1]
{56,1,0x80809A3B,0x80807ECC,0x80807EC9,0x81550242}, // sq_vex_intro_a[2]
{57,1,0x80809A3B,0x80807ECC,0x80807EC9,0x81550245}, // sq_vex_intro_a[3]
{58,1,0x80809A3B,0x80807ECC,0x80807EC9,0x81550248}, // sq_vex_intro_a[4]
{59,1,0x80809A3B,0x80807ECC,0x80807EC9,0x8155024B}, // sq_vex_intro_a[5]
{60,1,0x80809A3B,0x80807ECC,0x80807EC9,0x8155024E}, // sq_vex_mid[0]
{61,1,0x80809A3B,0x80807ECC,0x80807EC9,0x81550251}, // sq_vex_mid[1]
{62,1,0x80809A3B,0x80807ECC,0x80807EC9,0x81550254}, // sq_vex_mid[2]
{63,1,0x80809A3B,0x80807ECC,0x80807EC9,0x81550257}, // sq_vex_mid[3]
{64,1,0x80809A3B,0x80807ECC,0x80807EC9,0x8155025A}, // sq_vex_mid[4]
{65,1,0x80809A3B,0x80807ECC,0x80807EC9,0x8155025D}, // sq_vex_mid[5]
{66,1,0x80809A3B,0x80807ECC,0x80807EC9,0x81550260}, // sq_vex_mid_a[0]
{67,1,0x80809A3B,0x80807ECC,0x80807EC9,0x81550263}, // sq_vex_mid_a[1]
{68,1,0x80809A3B,0x80807ECC,0x80807EC9,0x81550266}, // sq_vex_mid_a[2]
{69,1,0x80809A3B,0x80807ECC,0x80807EC9,0x81550269}, // sq_vex_mid_a[3]
{70,1,0x80809A3B,0x80807ECC,0x80807EC9,0x8155026C}, // sq_vex_mid_a[4]
{71,1,0x80809A3B,0x80807ECC,0x80807EC9,0x8155026F}, // sq_vex_mid_a[5]
{72,1,0x80809A3B,0x80807ECC,0x80807EC9,0x81550272}, // sq_fallen_intro[0]
{73,1,0x80809A3B,0x80807ECC,0x80807EC9,0x81550275}, // sq_fallen_intro[1]
{74,1,0x80809A3B,0x80807ECC,0x80807EC9,0x81550278}, // sq_fallen_intro[2]
{75,1,0x80809A3B,0x80807ECC,0x80807EC9,0x8155027B}, // sq_fallen_intro[3]
{76,1,0x80809A3B,0x80807ECC,0x80807EC9,0x8155027E}, // sq_fallen_intro[4]
{77,1,0x80809A3B,0x80807ECC,0x80807EC9,0x81550281}, // sq_fallen_intro[5]
{78,1,0x80809A3B,0x80807ECC,0x80807EC9,0x81550284}, // sq_fallen_intro_a[0]
{79,1,0x80809A3B,0x80807ECC,0x80807EC9,0x81550287}, // sq_fallen_intro_a[1]
{80,1,0x80809A3B,0x80807ECC,0x80807EC9,0x8155028A}, // sq_fallen_intro_a[2]
{81,1,0x80809A3B,0x80807ECC,0x80807EC9,0x8155028D}, // sq_fallen_intro_a[3]
{82,1,0x80809A3B,0x80807ECC,0x80807EC9,0x81550290}, // sq_fallen_intro_a[4]
{83,1,0x80809A3B,0x80807ECC,0x80807EC9,0x81550293}, // sq_fallen_intro_a[5]
{84,1,0x80809A3B,0x80807ECC,0x80807EC9,0x81550296}, // sq_fallen_mid[0]
{85,1,0x80809A3B,0x80807ECC,0x80807EC9,0x81550299}, // sq_fallen_mid[1]
{86,1,0x80809A3B,0x80807ECC,0x80807EC9,0x8155029C}, // sq_fallen_mid[2]
{87,1,0x80809A3B,0x80807ECC,0x80807EC9,0x8155029F}, // sq_fallen_mid[3]
{88,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815502A2}, // sq_fallen_mid[4]
{89,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815502A5}, // sq_fallen_mid[5]
{90,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815502A8}, // sq_fallen_mid_a[0]
{91,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815502AB}, // sq_fallen_mid_a[1]
{92,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815502AE}, // sq_fallen_mid_a[2]
{93,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815502B1}, // sq_fallen_mid_a[3]
{94,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815502B4}, // sq_fallen_mid_a[4]
{95,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815502B7}, // sq_fallen_mid_a[5]
{96,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815502BA}, // sq_cabal_boss01
{97,2,0x8080834E,0x80807DA2,0x80807DA1,0x815502BD}, // sq_cabal_boss01__ai
{98,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815502C0}, // sq_cabal_boss02
{99,2,0x8080834E,0x80807DA2,0x80807DA1,0x815502C3}, // sq_cabal_boss02__ai
{100,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815502C6}, // sq_cabal_boss03
{101,2,0x8080834E,0x80807DA2,0x80807DA1,0x815502C9}, // sq_cabal_boss03__ai
{102,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815502CC}, // sq_fallen_boss01
{103,2,0x8080834E,0x80807DA2,0x80807DA1,0x815502CF}, // sq_fallen_boss01__ai
{104,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815502D2}, // sq_fallen_boss02
{105,2,0x8080834E,0x80807DA2,0x80807DA1,0x815502D5}, // sq_fallen_boss02__ai
{106,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815502D8}, // sq_fallen_boss03
{107,2,0x8080834E,0x80807DA2,0x80807DA1,0x815502DB}, // sq_fallen_boss03__ai
{108,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815502DE}, // sq_hive_boss01
{109,2,0x8080834E,0x80807DA2,0x80807DA1,0x815502E1}, // sq_hive_boss01__ai
{110,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815502E4}, // sq_hive_boss02
{111,2,0x8080834E,0x80807DA2,0x80807DA1,0x815502E7}, // sq_hive_boss02__ai
{112,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815502EA}, // sq_hive_boss03
{113,2,0x8080834E,0x80807DA2,0x80807DA1,0x815502ED}, // sq_hive_boss03__ai
{114,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815502F0}, // sq_vex_boss01
{115,2,0x8080834E,0x80807DA2,0x80807DA1,0x815502F3}, // sq_vex_boss01__ai
{116,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815502F6}, // sq_vex_boss02
{117,2,0x8080834E,0x80807DA2,0x80807DA1,0x815502F9}, // sq_vex_boss02__ai
{118,1,0x80809A3B,0x80807ECC,0x80807EC9,0x815502FC}, // sq_vex_boss03
{119,2,0x8080834E,0x80807DA2,0x80807DA1,0x815502FF}, // sq_vex_boss03__ai
{120,1,0x80809A3B,0x80807ECC,0x80807EC9,0x81550302}, // sq_vex_boss_solo
{121,2,0x8080834E,0x80807DA2,0x80807DA1,0x81550305}, // sq_vex_boss_solo__ai
{122,3,0x80808348,0x80807F04,0x80807F0C,0x81550308}, // ai_objective
{123,4,0x80809927,0x8080992E,0x8080992F,0x8155030B}, // o_chest[0]
{124,4,0x80809927,0x8080992E,0x8080992F,0x8155030E}, // o_chest[1]
{125,4,0x80809927,0x8080992E,0x8080992F,0x81550311}, // o_chest[2]
{126,4,0x80809927,0x8080992E,0x8080992F,0x81550314}, // o_chest[3]
{127,4,0x80809927,0x8080992E,0x8080992F,0x81550317}, // o_chest[4]
{128,4,0x80809927,0x8080992E,0x8080992F,0x8155031A}, // o_chest[5]
{130,70,0x808094EE,0x808094F0,0x808094F1,0x8155031D}, // m_engagement_sensor
}};
inline constexpr std::array<registry::Definition,1> kArenaRegistry{{
    {"infinite_abyss",0x81550015U,0x41D79D07U,0x81550320U,0x47EA4CE9U,13,kArenaSlots},
}};

/** Finds an authored arena slot by its semantic slot and native type. */
[[nodiscard]] constexpr coo::Asset arenaAsset(std::uint16_t slot,std::uint16_t type) noexcept {
    for(const auto& candidate:kArenaSlots) {
        if(candidate.index==slot && candidate.type==type) {
            return {kArenaRegistry[0].key,candidate.descriptorTag,candidate.type,candidate.index};
        }
    }
    return {};
}

inline constexpr std::array<registry::Slot,10> kAuxiliarySlots{{
{7,34,0x80809568,0xFFFFFFFF,0x8080956A,0x81550329}, // of_allplayers
{0,31,0x80809522,0xFFFFFFFF,0x80809524,0x81550333}, // pt_start_heroic_timer
{1,30,0x8080952F,0x80809531,0x80809532,0x81550336}, // pm_forest
{2,30,0x8080952F,0x80809531,0x80809532,0x81550339}, // pm_boss_room
{3,30,0x8080952F,0x80809531,0x80809532,0x8155033C}, // pm_dungeon
{4,31,0x80809522,0xFFFFFFFF,0x80809524,0x8155033F}, // pt_start_encounter
{5,32,0x80809556,0xFFFFFFFF,0x8080955A,0x81550342}, // t_reward_spawn
{6,32,0x80809556,0xFFFFFFFF,0x8080955A,0x81550345}, // t_top_spawn
{8,30,0x8080952F,0x80809531,0x80809532,0x81550348}, // pm_allplayers
{16,57,0x808094D7,0xFFFFFFFF,0xFFFFFFFF,0x8155034B}, // infinite_forest_g
}};
inline constexpr std::array<registry::Definition,1> kAuxiliaryRegistry{{
    {"infinite_abyss",0x81550015U,0x28BA7846U,0x8155034EU,0x47EA4CE9U,13,kAuxiliarySlots},
}};

struct PlatformBinding final {
    std::uint8_t platformNumber{};
    coo::Asset main{}, approach{}, plate{}, device{}, success{}, teleportTarget{};
    std::array<coo::Asset,3> destinations{};
    coo::Asset effect{}, occupancy{}, noTeleport{}, filter{};
};
inline constexpr std::array<PlatformBinding,4> kPlatforms{{
    {0,{kRegistries[0].key,kStartSlots[30].descriptorTag,kStartSlots[30].type,kStartSlots[30].index},{kRegistries[0].key,kStartSlots[31].descriptorTag,kStartSlots[31].type,kStartSlots[31].index},{kRegistries[0].key,kStartSlots[32].descriptorTag,kStartSlots[32].type,kStartSlots[32].index},{kRegistries[0].key,kStartSlots[33].descriptorTag,kStartSlots[33].type,kStartSlots[33].index},{kRegistries[0].key,kStartSlots[34].descriptorTag,kStartSlots[34].type,kStartSlots[34].index},{kRegistries[0].key,kStartSlots[35].descriptorTag,kStartSlots[35].type,kStartSlots[35].index},
        {{{kRegistries[0].key,kStartSlots[36].descriptorTag,kStartSlots[36].type,kStartSlots[36].index},{kRegistries[0].key,kStartSlots[37].descriptorTag,kStartSlots[37].type,kStartSlots[37].index},{kRegistries[0].key,kStartSlots[38].descriptorTag,kStartSlots[38].type,kStartSlots[38].index}}},{kRegistries[0].key,kStartSlots[39].descriptorTag,kStartSlots[39].type,kStartSlots[39].index},{kRegistries[0].key,kStartSlots[109].descriptorTag,kStartSlots[109].type,kStartSlots[109].index},{kRegistries[0].key,kStartSlots[110].descriptorTag,kStartSlots[110].type,kStartSlots[110].index},{kRegistries[0].key,kStartSlots[111].descriptorTag,kStartSlots[111].type,kStartSlots[111].index}},
    {1,{kRegistries[0].key,kStartSlots[10].descriptorTag,kStartSlots[10].type,kStartSlots[10].index},{kRegistries[0].key,kStartSlots[11].descriptorTag,kStartSlots[11].type,kStartSlots[11].index},{kRegistries[0].key,kStartSlots[12].descriptorTag,kStartSlots[12].type,kStartSlots[12].index},{kRegistries[0].key,kStartSlots[13].descriptorTag,kStartSlots[13].type,kStartSlots[13].index},{kRegistries[0].key,kStartSlots[14].descriptorTag,kStartSlots[14].type,kStartSlots[14].index},{kRegistries[0].key,kStartSlots[15].descriptorTag,kStartSlots[15].type,kStartSlots[15].index},
        {{{kRegistries[0].key,kStartSlots[16].descriptorTag,kStartSlots[16].type,kStartSlots[16].index},{kRegistries[0].key,kStartSlots[17].descriptorTag,kStartSlots[17].type,kStartSlots[17].index},{kRegistries[0].key,kStartSlots[18].descriptorTag,kStartSlots[18].type,kStartSlots[18].index}}},{kRegistries[0].key,kStartSlots[19].descriptorTag,kStartSlots[19].type,kStartSlots[19].index},{kRegistries[0].key,kStartSlots[113].descriptorTag,kStartSlots[113].type,kStartSlots[113].index},{kRegistries[0].key,kStartSlots[114].descriptorTag,kStartSlots[114].type,kStartSlots[114].index},{kRegistries[0].key,kStartSlots[115].descriptorTag,kStartSlots[115].type,kStartSlots[115].index}},
    {2,{kRegistries[0].key,kStartSlots[0].descriptorTag,kStartSlots[0].type,kStartSlots[0].index},{kRegistries[0].key,kStartSlots[1].descriptorTag,kStartSlots[1].type,kStartSlots[1].index},{kRegistries[0].key,kStartSlots[2].descriptorTag,kStartSlots[2].type,kStartSlots[2].index},{kRegistries[0].key,kStartSlots[3].descriptorTag,kStartSlots[3].type,kStartSlots[3].index},{kRegistries[0].key,kStartSlots[4].descriptorTag,kStartSlots[4].type,kStartSlots[4].index},{kRegistries[0].key,kStartSlots[5].descriptorTag,kStartSlots[5].type,kStartSlots[5].index},
        {{{kRegistries[0].key,kStartSlots[6].descriptorTag,kStartSlots[6].type,kStartSlots[6].index},{kRegistries[0].key,kStartSlots[7].descriptorTag,kStartSlots[7].type,kStartSlots[7].index},{kRegistries[0].key,kStartSlots[8].descriptorTag,kStartSlots[8].type,kStartSlots[8].index}}},{kRegistries[0].key,kStartSlots[9].descriptorTag,kStartSlots[9].type,kStartSlots[9].index},{kRegistries[0].key,kStartSlots[117].descriptorTag,kStartSlots[117].type,kStartSlots[117].index},{kRegistries[0].key,kStartSlots[118].descriptorTag,kStartSlots[118].type,kStartSlots[118].index},{kRegistries[0].key,kStartSlots[119].descriptorTag,kStartSlots[119].type,kStartSlots[119].index}},
    {3,{kRegistries[0].key,kStartSlots[20].descriptorTag,kStartSlots[20].type,kStartSlots[20].index},{kRegistries[0].key,kStartSlots[21].descriptorTag,kStartSlots[21].type,kStartSlots[21].index},{kRegistries[0].key,kStartSlots[22].descriptorTag,kStartSlots[22].type,kStartSlots[22].index},{kRegistries[0].key,kStartSlots[23].descriptorTag,kStartSlots[23].type,kStartSlots[23].index},{kRegistries[0].key,kStartSlots[24].descriptorTag,kStartSlots[24].type,kStartSlots[24].index},{kRegistries[0].key,kStartSlots[25].descriptorTag,kStartSlots[25].type,kStartSlots[25].index},
        {{{kRegistries[0].key,kStartSlots[26].descriptorTag,kStartSlots[26].type,kStartSlots[26].index},{kRegistries[0].key,kStartSlots[27].descriptorTag,kStartSlots[27].type,kStartSlots[27].index},{kRegistries[0].key,kStartSlots[28].descriptorTag,kStartSlots[28].type,kStartSlots[28].index}}},{kRegistries[0].key,kStartSlots[29].descriptorTag,kStartSlots[29].type,kStartSlots[29].index},{kRegistries[0].key,kStartSlots[121].descriptorTag,kStartSlots[121].type,kStartSlots[121].index},{kRegistries[0].key,kStartSlots[122].descriptorTag,kStartSlots[122].type,kStartSlots[122].index},{kRegistries[0].key,kStartSlots[123].descriptorTag,kStartSlots[123].type,kStartSlots[123].index}}
}};

constexpr bool validPlatformBindings() noexcept {
    constexpr std::array<std::uint16_t,4> kMainSlots{{30,10,0,20}};
    for(std::size_t i=0;i<kPlatforms.size();++i) {
        const auto& platform=kPlatforms[i];
        if(platform.platformNumber!=i || platform.destinations.size()!=3
            || platform.main.registry!=kRegistries[0].key
            || platform.main.slot!=kStartSlots[kMainSlots[i]].index) return false;
        if(platform.approach.registry!=kRegistries[0].key
            || platform.plate.registry!=kRegistries[0].key
            || platform.device.registry!=kRegistries[0].key
            || platform.success.registry!=kRegistries[0].key
            || platform.teleportTarget.registry!=kRegistries[0].key
            || platform.effect.registry!=kRegistries[0].key
            || platform.occupancy.registry!=kRegistries[0].key
            || platform.noTeleport.registry!=kRegistries[0].key
            || platform.filter.registry!=kRegistries[0].key) return false;
        for(const auto& destination:platform.destinations)
            if(destination.registry!=kRegistries[0].key) return false;
    }
    return true;
}
static_assert(validPlatformBindings());

enum class Faction : std::uint8_t { cabal, fallen, hive, vex };
struct BossBinding final {
    coo::Asset squad{}, combatant{}, spawnRule{};
    Faction faction{};
};
inline constexpr coo::Asset kBossSpawnRule=arenaAsset(156,66);
inline constexpr std::array<BossBinding,13> kBosses{{
    {arenaAsset(96,1),arenaAsset(97,2),kBossSpawnRule,Faction::cabal},
    {arenaAsset(98,1),arenaAsset(99,2),kBossSpawnRule,Faction::cabal},
    {arenaAsset(100,1),arenaAsset(101,2),kBossSpawnRule,Faction::cabal},
    {arenaAsset(102,1),arenaAsset(103,2),kBossSpawnRule,Faction::fallen},
    {arenaAsset(104,1),arenaAsset(105,2),kBossSpawnRule,Faction::fallen},
    {arenaAsset(106,1),arenaAsset(107,2),kBossSpawnRule,Faction::fallen},
    {arenaAsset(108,1),arenaAsset(109,2),kBossSpawnRule,Faction::hive},
    {arenaAsset(110,1),arenaAsset(111,2),kBossSpawnRule,Faction::hive},
    {arenaAsset(112,1),arenaAsset(113,2),kBossSpawnRule,Faction::hive},
    {arenaAsset(114,1),arenaAsset(115,2),kBossSpawnRule,Faction::vex},
    {arenaAsset(116,1),arenaAsset(117,2),kBossSpawnRule,Faction::vex},
    {arenaAsset(118,1),arenaAsset(119,2),kBossSpawnRule,Faction::vex},
    {arenaAsset(120,1),arenaAsset(121,2),kBossSpawnRule,Faction::vex}
}};

inline constexpr std::uint16_t kFirstBossSourceSlot=96;
inline constexpr std::uint16_t kBossSourceStride=2;
constexpr bool validBossBindings() noexcept {
    for(std::size_t i=0;i<kBosses.size();++i) {
        const auto& boss=kBosses[i];
        const auto sourceSlot=static_cast<std::uint16_t>(kFirstBossSourceSlot+i*kBossSourceStride);
        if(boss.squad.registry!=kArenaRegistry[0].key
            || boss.combatant.registry!=kArenaRegistry[0].key
            || boss.squad.type!=1 || boss.combatant.type!=2
            || boss.squad.slot!=sourceSlot || boss.combatant.slot!=sourceSlot+1
            || boss.combatant.slot!=boss.squad.slot+1
            || boss.spawnRule.registry!=kArenaRegistry[0].key
            || boss.spawnRule.slot!=156 || boss.spawnRule.type!=66) return false;
    }
    return true;
}
static_assert(validBossBindings());

enum class AddPhase : std::uint8_t { intro, mid };
struct AddSourceBinding final {
    coo::Asset source{}, spawnRule{};
    Faction faction{};
    AddPhase phase{};
};
inline constexpr coo::Asset kAddSpawnRule=arenaAsset(157,66);
template<std::size_t N>
constexpr bool validAddSourceRange(const std::array<AddSourceBinding,N>& sources,
    std::uint16_t firstSlot,Faction faction) noexcept {
    for(std::size_t i=0;i<sources.size();++i) {
        const auto& source=sources[i];
        const auto expectedSlot=static_cast<std::uint16_t>(firstSlot+i);
        const auto expectedPhase=i<12 ? AddPhase::intro : AddPhase::mid;
        if(source.faction!=faction || source.phase!=expectedPhase
            || source.source.registry!=kArenaRegistry[0].key || source.source.type!=1
            || source.source.slot!=expectedSlot || source.spawnRule.registry!=kArenaRegistry[0].key
            || source.spawnRule.slot!=157 || source.spawnRule.type!=66) return false;
    }
    return true;
}
// Each group contains authored source slots 0..23, 24..47, 48..71 or 72..95.
// Within each group, the first 12 entries are intro and the next 12 are mid.
// Rule157 is retained as authored data; no AI or tactical row is selected here.
inline constexpr std::array<AddSourceBinding,24> kCabalAddSources{{
    {{0x41D79D07U,kArenaSlots[3].descriptorTag,kArenaSlots[3].type,kArenaSlots[3].index},kAddSpawnRule,Faction::cabal,AddPhase::intro},
    {{0x41D79D07U,kArenaSlots[4].descriptorTag,kArenaSlots[4].type,kArenaSlots[4].index},kAddSpawnRule,Faction::cabal,AddPhase::intro},
    {{0x41D79D07U,kArenaSlots[5].descriptorTag,kArenaSlots[5].type,kArenaSlots[5].index},kAddSpawnRule,Faction::cabal,AddPhase::intro},
    {{0x41D79D07U,kArenaSlots[6].descriptorTag,kArenaSlots[6].type,kArenaSlots[6].index},kAddSpawnRule,Faction::cabal,AddPhase::intro},
    {{0x41D79D07U,kArenaSlots[7].descriptorTag,kArenaSlots[7].type,kArenaSlots[7].index},kAddSpawnRule,Faction::cabal,AddPhase::intro},
    {{0x41D79D07U,kArenaSlots[8].descriptorTag,kArenaSlots[8].type,kArenaSlots[8].index},kAddSpawnRule,Faction::cabal,AddPhase::intro},
    {{0x41D79D07U,kArenaSlots[9].descriptorTag,kArenaSlots[9].type,kArenaSlots[9].index},kAddSpawnRule,Faction::cabal,AddPhase::intro},
    {{0x41D79D07U,kArenaSlots[10].descriptorTag,kArenaSlots[10].type,kArenaSlots[10].index},kAddSpawnRule,Faction::cabal,AddPhase::intro},
    {{0x41D79D07U,kArenaSlots[11].descriptorTag,kArenaSlots[11].type,kArenaSlots[11].index},kAddSpawnRule,Faction::cabal,AddPhase::intro},
    {{0x41D79D07U,kArenaSlots[12].descriptorTag,kArenaSlots[12].type,kArenaSlots[12].index},kAddSpawnRule,Faction::cabal,AddPhase::intro},
    {{0x41D79D07U,kArenaSlots[13].descriptorTag,kArenaSlots[13].type,kArenaSlots[13].index},kAddSpawnRule,Faction::cabal,AddPhase::intro},
    {{0x41D79D07U,kArenaSlots[14].descriptorTag,kArenaSlots[14].type,kArenaSlots[14].index},kAddSpawnRule,Faction::cabal,AddPhase::intro},
    {{0x41D79D07U,kArenaSlots[15].descriptorTag,kArenaSlots[15].type,kArenaSlots[15].index},kAddSpawnRule,Faction::cabal,AddPhase::mid},
    {{0x41D79D07U,kArenaSlots[16].descriptorTag,kArenaSlots[16].type,kArenaSlots[16].index},kAddSpawnRule,Faction::cabal,AddPhase::mid},
    {{0x41D79D07U,kArenaSlots[17].descriptorTag,kArenaSlots[17].type,kArenaSlots[17].index},kAddSpawnRule,Faction::cabal,AddPhase::mid},
    {{0x41D79D07U,kArenaSlots[18].descriptorTag,kArenaSlots[18].type,kArenaSlots[18].index},kAddSpawnRule,Faction::cabal,AddPhase::mid},
    {{0x41D79D07U,kArenaSlots[19].descriptorTag,kArenaSlots[19].type,kArenaSlots[19].index},kAddSpawnRule,Faction::cabal,AddPhase::mid},
    {{0x41D79D07U,kArenaSlots[20].descriptorTag,kArenaSlots[20].type,kArenaSlots[20].index},kAddSpawnRule,Faction::cabal,AddPhase::mid},
    {{0x41D79D07U,kArenaSlots[21].descriptorTag,kArenaSlots[21].type,kArenaSlots[21].index},kAddSpawnRule,Faction::cabal,AddPhase::mid},
    {{0x41D79D07U,kArenaSlots[22].descriptorTag,kArenaSlots[22].type,kArenaSlots[22].index},kAddSpawnRule,Faction::cabal,AddPhase::mid},
    {{0x41D79D07U,kArenaSlots[23].descriptorTag,kArenaSlots[23].type,kArenaSlots[23].index},kAddSpawnRule,Faction::cabal,AddPhase::mid},
    {{0x41D79D07U,kArenaSlots[24].descriptorTag,kArenaSlots[24].type,kArenaSlots[24].index},kAddSpawnRule,Faction::cabal,AddPhase::mid},
    {{0x41D79D07U,kArenaSlots[25].descriptorTag,kArenaSlots[25].type,kArenaSlots[25].index},kAddSpawnRule,Faction::cabal,AddPhase::mid},
    {{0x41D79D07U,kArenaSlots[26].descriptorTag,kArenaSlots[26].type,kArenaSlots[26].index},kAddSpawnRule,Faction::cabal,AddPhase::mid},
}};

inline constexpr std::array<AddSourceBinding,24> kHiveAddSources{{
    {{0x41D79D07U,kArenaSlots[27].descriptorTag,kArenaSlots[27].type,kArenaSlots[27].index},kAddSpawnRule,Faction::hive,AddPhase::intro},
    {{0x41D79D07U,kArenaSlots[28].descriptorTag,kArenaSlots[28].type,kArenaSlots[28].index},kAddSpawnRule,Faction::hive,AddPhase::intro},
    {{0x41D79D07U,kArenaSlots[29].descriptorTag,kArenaSlots[29].type,kArenaSlots[29].index},kAddSpawnRule,Faction::hive,AddPhase::intro},
    {{0x41D79D07U,kArenaSlots[30].descriptorTag,kArenaSlots[30].type,kArenaSlots[30].index},kAddSpawnRule,Faction::hive,AddPhase::intro},
    {{0x41D79D07U,kArenaSlots[31].descriptorTag,kArenaSlots[31].type,kArenaSlots[31].index},kAddSpawnRule,Faction::hive,AddPhase::intro},
    {{0x41D79D07U,kArenaSlots[32].descriptorTag,kArenaSlots[32].type,kArenaSlots[32].index},kAddSpawnRule,Faction::hive,AddPhase::intro},
    {{0x41D79D07U,kArenaSlots[33].descriptorTag,kArenaSlots[33].type,kArenaSlots[33].index},kAddSpawnRule,Faction::hive,AddPhase::intro},
    {{0x41D79D07U,kArenaSlots[34].descriptorTag,kArenaSlots[34].type,kArenaSlots[34].index},kAddSpawnRule,Faction::hive,AddPhase::intro},
    {{0x41D79D07U,kArenaSlots[35].descriptorTag,kArenaSlots[35].type,kArenaSlots[35].index},kAddSpawnRule,Faction::hive,AddPhase::intro},
    {{0x41D79D07U,kArenaSlots[36].descriptorTag,kArenaSlots[36].type,kArenaSlots[36].index},kAddSpawnRule,Faction::hive,AddPhase::intro},
    {{0x41D79D07U,kArenaSlots[37].descriptorTag,kArenaSlots[37].type,kArenaSlots[37].index},kAddSpawnRule,Faction::hive,AddPhase::intro},
    {{0x41D79D07U,kArenaSlots[38].descriptorTag,kArenaSlots[38].type,kArenaSlots[38].index},kAddSpawnRule,Faction::hive,AddPhase::intro},
    {{0x41D79D07U,kArenaSlots[39].descriptorTag,kArenaSlots[39].type,kArenaSlots[39].index},kAddSpawnRule,Faction::hive,AddPhase::mid},
    {{0x41D79D07U,kArenaSlots[40].descriptorTag,kArenaSlots[40].type,kArenaSlots[40].index},kAddSpawnRule,Faction::hive,AddPhase::mid},
    {{0x41D79D07U,kArenaSlots[41].descriptorTag,kArenaSlots[41].type,kArenaSlots[41].index},kAddSpawnRule,Faction::hive,AddPhase::mid},
    {{0x41D79D07U,kArenaSlots[42].descriptorTag,kArenaSlots[42].type,kArenaSlots[42].index},kAddSpawnRule,Faction::hive,AddPhase::mid},
    {{0x41D79D07U,kArenaSlots[43].descriptorTag,kArenaSlots[43].type,kArenaSlots[43].index},kAddSpawnRule,Faction::hive,AddPhase::mid},
    {{0x41D79D07U,kArenaSlots[44].descriptorTag,kArenaSlots[44].type,kArenaSlots[44].index},kAddSpawnRule,Faction::hive,AddPhase::mid},
    {{0x41D79D07U,kArenaSlots[45].descriptorTag,kArenaSlots[45].type,kArenaSlots[45].index},kAddSpawnRule,Faction::hive,AddPhase::mid},
    {{0x41D79D07U,kArenaSlots[46].descriptorTag,kArenaSlots[46].type,kArenaSlots[46].index},kAddSpawnRule,Faction::hive,AddPhase::mid},
    {{0x41D79D07U,kArenaSlots[47].descriptorTag,kArenaSlots[47].type,kArenaSlots[47].index},kAddSpawnRule,Faction::hive,AddPhase::mid},
    {{0x41D79D07U,kArenaSlots[48].descriptorTag,kArenaSlots[48].type,kArenaSlots[48].index},kAddSpawnRule,Faction::hive,AddPhase::mid},
    {{0x41D79D07U,kArenaSlots[49].descriptorTag,kArenaSlots[49].type,kArenaSlots[49].index},kAddSpawnRule,Faction::hive,AddPhase::mid},
    {{0x41D79D07U,kArenaSlots[50].descriptorTag,kArenaSlots[50].type,kArenaSlots[50].index},kAddSpawnRule,Faction::hive,AddPhase::mid},
}};

inline constexpr std::array<AddSourceBinding,24> kVexAddSources{{
    {{0x41D79D07U,kArenaSlots[51].descriptorTag,kArenaSlots[51].type,kArenaSlots[51].index},kAddSpawnRule,Faction::vex,AddPhase::intro},
    {{0x41D79D07U,kArenaSlots[52].descriptorTag,kArenaSlots[52].type,kArenaSlots[52].index},kAddSpawnRule,Faction::vex,AddPhase::intro},
    {{0x41D79D07U,kArenaSlots[53].descriptorTag,kArenaSlots[53].type,kArenaSlots[53].index},kAddSpawnRule,Faction::vex,AddPhase::intro},
    {{0x41D79D07U,kArenaSlots[54].descriptorTag,kArenaSlots[54].type,kArenaSlots[54].index},kAddSpawnRule,Faction::vex,AddPhase::intro},
    {{0x41D79D07U,kArenaSlots[55].descriptorTag,kArenaSlots[55].type,kArenaSlots[55].index},kAddSpawnRule,Faction::vex,AddPhase::intro},
    {{0x41D79D07U,kArenaSlots[56].descriptorTag,kArenaSlots[56].type,kArenaSlots[56].index},kAddSpawnRule,Faction::vex,AddPhase::intro},
    {{0x41D79D07U,kArenaSlots[57].descriptorTag,kArenaSlots[57].type,kArenaSlots[57].index},kAddSpawnRule,Faction::vex,AddPhase::intro},
    {{0x41D79D07U,kArenaSlots[58].descriptorTag,kArenaSlots[58].type,kArenaSlots[58].index},kAddSpawnRule,Faction::vex,AddPhase::intro},
    {{0x41D79D07U,kArenaSlots[59].descriptorTag,kArenaSlots[59].type,kArenaSlots[59].index},kAddSpawnRule,Faction::vex,AddPhase::intro},
    {{0x41D79D07U,kArenaSlots[60].descriptorTag,kArenaSlots[60].type,kArenaSlots[60].index},kAddSpawnRule,Faction::vex,AddPhase::intro},
    {{0x41D79D07U,kArenaSlots[61].descriptorTag,kArenaSlots[61].type,kArenaSlots[61].index},kAddSpawnRule,Faction::vex,AddPhase::intro},
    {{0x41D79D07U,kArenaSlots[62].descriptorTag,kArenaSlots[62].type,kArenaSlots[62].index},kAddSpawnRule,Faction::vex,AddPhase::intro},
    {{0x41D79D07U,kArenaSlots[63].descriptorTag,kArenaSlots[63].type,kArenaSlots[63].index},kAddSpawnRule,Faction::vex,AddPhase::mid},
    {{0x41D79D07U,kArenaSlots[64].descriptorTag,kArenaSlots[64].type,kArenaSlots[64].index},kAddSpawnRule,Faction::vex,AddPhase::mid},
    {{0x41D79D07U,kArenaSlots[65].descriptorTag,kArenaSlots[65].type,kArenaSlots[65].index},kAddSpawnRule,Faction::vex,AddPhase::mid},
    {{0x41D79D07U,kArenaSlots[66].descriptorTag,kArenaSlots[66].type,kArenaSlots[66].index},kAddSpawnRule,Faction::vex,AddPhase::mid},
    {{0x41D79D07U,kArenaSlots[67].descriptorTag,kArenaSlots[67].type,kArenaSlots[67].index},kAddSpawnRule,Faction::vex,AddPhase::mid},
    {{0x41D79D07U,kArenaSlots[68].descriptorTag,kArenaSlots[68].type,kArenaSlots[68].index},kAddSpawnRule,Faction::vex,AddPhase::mid},
    {{0x41D79D07U,kArenaSlots[69].descriptorTag,kArenaSlots[69].type,kArenaSlots[69].index},kAddSpawnRule,Faction::vex,AddPhase::mid},
    {{0x41D79D07U,kArenaSlots[70].descriptorTag,kArenaSlots[70].type,kArenaSlots[70].index},kAddSpawnRule,Faction::vex,AddPhase::mid},
    {{0x41D79D07U,kArenaSlots[71].descriptorTag,kArenaSlots[71].type,kArenaSlots[71].index},kAddSpawnRule,Faction::vex,AddPhase::mid},
    {{0x41D79D07U,kArenaSlots[72].descriptorTag,kArenaSlots[72].type,kArenaSlots[72].index},kAddSpawnRule,Faction::vex,AddPhase::mid},
    {{0x41D79D07U,kArenaSlots[73].descriptorTag,kArenaSlots[73].type,kArenaSlots[73].index},kAddSpawnRule,Faction::vex,AddPhase::mid},
    {{0x41D79D07U,kArenaSlots[74].descriptorTag,kArenaSlots[74].type,kArenaSlots[74].index},kAddSpawnRule,Faction::vex,AddPhase::mid},
}};

inline constexpr std::array<AddSourceBinding,24> kFallenAddSources{{
    {{0x41D79D07U,kArenaSlots[75].descriptorTag,kArenaSlots[75].type,kArenaSlots[75].index},kAddSpawnRule,Faction::fallen,AddPhase::intro},
    {{0x41D79D07U,kArenaSlots[76].descriptorTag,kArenaSlots[76].type,kArenaSlots[76].index},kAddSpawnRule,Faction::fallen,AddPhase::intro},
    {{0x41D79D07U,kArenaSlots[77].descriptorTag,kArenaSlots[77].type,kArenaSlots[77].index},kAddSpawnRule,Faction::fallen,AddPhase::intro},
    {{0x41D79D07U,kArenaSlots[78].descriptorTag,kArenaSlots[78].type,kArenaSlots[78].index},kAddSpawnRule,Faction::fallen,AddPhase::intro},
    {{0x41D79D07U,kArenaSlots[79].descriptorTag,kArenaSlots[79].type,kArenaSlots[79].index},kAddSpawnRule,Faction::fallen,AddPhase::intro},
    {{0x41D79D07U,kArenaSlots[80].descriptorTag,kArenaSlots[80].type,kArenaSlots[80].index},kAddSpawnRule,Faction::fallen,AddPhase::intro},
    {{0x41D79D07U,kArenaSlots[81].descriptorTag,kArenaSlots[81].type,kArenaSlots[81].index},kAddSpawnRule,Faction::fallen,AddPhase::intro},
    {{0x41D79D07U,kArenaSlots[82].descriptorTag,kArenaSlots[82].type,kArenaSlots[82].index},kAddSpawnRule,Faction::fallen,AddPhase::intro},
    {{0x41D79D07U,kArenaSlots[83].descriptorTag,kArenaSlots[83].type,kArenaSlots[83].index},kAddSpawnRule,Faction::fallen,AddPhase::intro},
    {{0x41D79D07U,kArenaSlots[84].descriptorTag,kArenaSlots[84].type,kArenaSlots[84].index},kAddSpawnRule,Faction::fallen,AddPhase::intro},
    {{0x41D79D07U,kArenaSlots[85].descriptorTag,kArenaSlots[85].type,kArenaSlots[85].index},kAddSpawnRule,Faction::fallen,AddPhase::intro},
    {{0x41D79D07U,kArenaSlots[86].descriptorTag,kArenaSlots[86].type,kArenaSlots[86].index},kAddSpawnRule,Faction::fallen,AddPhase::intro},
    {{0x41D79D07U,kArenaSlots[87].descriptorTag,kArenaSlots[87].type,kArenaSlots[87].index},kAddSpawnRule,Faction::fallen,AddPhase::mid},
    {{0x41D79D07U,kArenaSlots[88].descriptorTag,kArenaSlots[88].type,kArenaSlots[88].index},kAddSpawnRule,Faction::fallen,AddPhase::mid},
    {{0x41D79D07U,kArenaSlots[89].descriptorTag,kArenaSlots[89].type,kArenaSlots[89].index},kAddSpawnRule,Faction::fallen,AddPhase::mid},
    {{0x41D79D07U,kArenaSlots[90].descriptorTag,kArenaSlots[90].type,kArenaSlots[90].index},kAddSpawnRule,Faction::fallen,AddPhase::mid},
    {{0x41D79D07U,kArenaSlots[91].descriptorTag,kArenaSlots[91].type,kArenaSlots[91].index},kAddSpawnRule,Faction::fallen,AddPhase::mid},
    {{0x41D79D07U,kArenaSlots[92].descriptorTag,kArenaSlots[92].type,kArenaSlots[92].index},kAddSpawnRule,Faction::fallen,AddPhase::mid},
    {{0x41D79D07U,kArenaSlots[93].descriptorTag,kArenaSlots[93].type,kArenaSlots[93].index},kAddSpawnRule,Faction::fallen,AddPhase::mid},
    {{0x41D79D07U,kArenaSlots[94].descriptorTag,kArenaSlots[94].type,kArenaSlots[94].index},kAddSpawnRule,Faction::fallen,AddPhase::mid},
    {{0x41D79D07U,kArenaSlots[95].descriptorTag,kArenaSlots[95].type,kArenaSlots[95].index},kAddSpawnRule,Faction::fallen,AddPhase::mid},
    {{0x41D79D07U,kArenaSlots[96].descriptorTag,kArenaSlots[96].type,kArenaSlots[96].index},kAddSpawnRule,Faction::fallen,AddPhase::mid},
    {{0x41D79D07U,kArenaSlots[97].descriptorTag,kArenaSlots[97].type,kArenaSlots[97].index},kAddSpawnRule,Faction::fallen,AddPhase::mid},
    {{0x41D79D07U,kArenaSlots[98].descriptorTag,kArenaSlots[98].type,kArenaSlots[98].index},kAddSpawnRule,Faction::fallen,AddPhase::mid},
}};

static_assert(validAddSourceRange(kCabalAddSources,0,Faction::cabal));
static_assert(validAddSourceRange(kHiveAddSources,24,Faction::hive));
static_assert(validAddSourceRange(kVexAddSources,48,Faction::vex));
static_assert(validAddSourceRange(kFallenAddSources,72,Faction::fallen));

struct DirectiveBounds final {
    std::uint32_t event{};
    std::uint32_t firstVariant{};
    std::uint32_t lastVariant{};
};
inline constexpr std::uint32_t kEnterDirective=0x000D87C7U;
inline constexpr std::uint32_t kBranchDirective=0x6A3CC92FU;
inline constexpr std::uint32_t kTerrorDirective=0xE0491E48U;
inline constexpr std::uint32_t kCollapseDirective=0xC972D9EDU;
inline constexpr std::uint32_t kRewardDirective=0xB300293AU;
inline constexpr std::uint32_t kTimerOnlyDirective=0x61A52A3CU;
// Variant bounds are bank metadata, not gameplay limits.
inline constexpr std::array<DirectiveBounds,6> kDirectiveBounds{{
    {kEnterDirective,0,8},
    {kBranchDirective,0,26},
    {kTerrorDirective,0,0},
    {kCollapseDirective,0,20},
    {kRewardDirective,0,26},
    {kTimerOnlyDirective,0,0},
}};
// Native tactical objective slot122 exists, but its row assignments remain
// untraced; this profile deliberately does not select AI tactical rows.
} // namespace sunrise::server::runtime::activity::haunted_forest::mode
