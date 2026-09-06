#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace sunrise::state::activity::omega_enemy_lair {

inline constexpr std::uint32_t kRegistry = 0xF4D0E0B2U;
inline constexpr std::uint32_t kRegistryDefinition = 0x80F47979U;
inline constexpr std::uint32_t kPlacementDefinition = 0x80F478D1U;

/** Exact authored joins, not a wave/count recipe. XYZ belongs to the spawn-rule
 * placement volume, not an arbitrary position to write into spawned actors. */
struct Spawner final {
    std::uint16_t slot;
    std::uint16_t ruleSlot;
    std::uint32_t resource;
    std::uint32_t entity;
    std::uint64_t placementGuid;
    std::array<float,3> placement;
    std::string_view name;
    bool initialArea;
    /** A member may independently own this source's actor; zero means none. */
    std::uint16_t memberSlot;
};

inline constexpr std::array<Spawner,19> kSpawners{{
    {1,105,0x80F47919,0x80F4503F,0x26CFD73EE991F94EULL,{-1492,372,-26},"sq_anchor",true,2},
    {3,107,0x80F4791F,0x80F4500D,0x9BEB85099C9BEA10ULL,{-1492,427,-28.3403473F},"sq_front_1",true,0},
    {4,34,0x80F47922,0x80F4500D,0xFB54C52385E7F75EULL,{-1492,425,-28.1913891F},"sq_front_2",true,0},
    {5,36,0x80F47925,0x80F4500D,0x7B56E030DF5889DAULL,{-1492,415,-28.2311783F},"sq_front_3",true,0},
    {6,38,0x80F47928,0x80F4500D,0xF998284005E4BDEAULL,{-1492,380.761597F,-25.9689617F},"sq_front_4",true,0},
    {7,40,0x80F4792B,0x80F4500D,0x89E20580704739E6ULL,{-1471,412.833344F,-20.8802681F},"sq_side_1",true,0},
    {8,42,0x80F4792E,0x80F4500D,0x25730F79B52A847DULL,{-1513.16663F,412.833344F,-21.3767376F},"sq_side_2",true,0},
    {9,44,0x80F47931,0x80F45024,0x2795B45563FD456FULL,{-1469.03308F,397.062805F,-22.2020226F},"sq_sniper_left",true,0},
    {10,46,0x80F47934,0x80F45024,0x5B4B4EF236EF3D48ULL,{-1515.25757F,396.540466F,-22.1792507F},"sq_sniper_right",true,0},
    {11,48,0x80F47937,0x80F4500D,0xA6C19BC43F59F2CEULL,{-1492,377.385986F,-26},"sq_wave_b_1",true,0},
    {12,50,0x80F4793A,0x80F4500D,0x3CDBCFB476181892ULL,{-1492,375,-26},"sq_wave_b_2",true,0},
    {13,52,0x80F4793D,0x80F4500D,0xA63EE05EBDEF337AULL,{-1492,375,-26},"sq_guards_1",true,0},
    {14,54,0x80F47940,0x80F4500D,0xA91A87895F42C06BULL,{-1492,379,-26},"sq_guards_2",true,0},
    {15,70,0x80F47943,0x80F45004,0x807ADF9CFB737680ULL,{-1413.60571F,240.505402F,-27.7740936F},"sq_island_a_1",false,0},
    {16,71,0x80F47946,0x80F4507C,0xEEA634798480AB18ULL,{-1411.90247F,243.640656F,-27.8508263F},"sq_island_a_2",false,0},
    {17,72,0x80F47949,0x80F45053,0xE12A4C6D2090B866ULL,{-1577.43481F,181.595795F,-57.9205399F},"sq_island_b_1",false,0},
    {18,73,0x80F4794C,0x80F4503B,0x9A3DB0C9F996887FULL,{-1579.52686F,177.851563F,-57.9960251F},"sq_island_b_2",false,0},
    {19,74,0x80F4794F,0x80F45059,0x907D14F3DCCEE4EBULL,{-1484.42322F,81.1074371F,-50.3905258F},"sq_island_c_1",false,0},
    {20,75,0x80F47952,0x80F44FA6,0x3D88FCC403D6458AULL,{-1484.80530F,92.2418747F,-47.1841927F},"sq_island_c_2",false,0},
}};

[[nodiscard]] constexpr const Spawner* find_spawner(std::uint16_t slot) noexcept {
    for(const auto& spawner:kSpawners) { if(spawner.slot==slot) { return &spawner; } }
    return nullptr;
}

[[nodiscard]] constexpr std::uint8_t chase_island(std::uint16_t slot) noexcept {
    return slot>=15 && slot<=20 ? static_cast<std::uint8_t>((slot-15)/2+1) : 0;
}

/** Eligibility only: the encounter's native arm receipts still decide which
 * source receives requests. Catalog membership never activates a source. */
[[nodiscard]] constexpr bool supported_by_encounter(const Spawner& source) noexcept {
    return source.initialArea || chase_island(source.slot)!=0;
}

} // namespace sunrise::state::activity::omega_enemy_lair
