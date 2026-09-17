#pragma once
#include "authored_registry.h"
#include <array>

namespace dawn::state::activity::coo::adventure::mercury {
// Exact installed 80F5B993 descriptor chain, re-read 7 September 2026.
// Admission retains the COMPLETE native registry, including inactive patrol and
// forge components. Only the three separately selected capabilities below arm flags.
inline constexpr std::array<registry::Slot,16> kSlots{{
    {0,4,0x80809927,0x8080992E,0x8080992F,0x80F5B960},
    {1,4,0x80809927,0x8080992E,0x8080992F,0x80F5B956},
    {2,4,0x80809927,0x8080992E,0x8080992F,0x80F5B959},
    {3,4,0x80809927,0x8080992E,0x8080992F,0x80F5B95D},
    {4,4,0x80809927,0x8080992E,0x8080992F,0x80F5B963},
    {5,4,0x80809927,0x8080992E,0x8080992F,0x80F5B966},
    {6,4,0x80809927,0x8080992E,0x8080992F,0x80F5B969},
    {7,4,0x80809927,0x8080992E,0x8080992F,0x80F5B96C},
    {8,4,0x80809927,0x8080992E,0x8080992F,0x80F5B96F},
    {9,4,0x80809927,0x8080992E,0x8080992F,0x80F5B972},
    {10,4,0x80809927,0x8080992E,0x8080992F,0x80F5B975},
    {11,4,0x80809927,0x8080992E,0x8080992F,0x80F5B978},
    {12,23,0x80804F45,0x80804F47,0x80804F48,0x80F5B97B},
    {13,4,0x80809927,0x8080992E,0x8080992F,0x80F5B97E},
    {14,4,0x80809927,0x8080992E,0x8080992F,0x80F5B981},
    {15,70,0x808094EE,0x808094F0,0x808094F1,0x80F5B984},
}};
inline constexpr registry::Definition kRegistry{
    "mercury_freeroam",0x80F4696A,0x2749BAAE,0x80F5B993,0xA83A9175,15,kSlots};

struct Beacon final {
    std::string_view capability, authoredName, targetPackage;
    std::uint16_t slot{}, publicOrdinal{};
    std::uint32_t descriptor{}, selector{}, activityHash{};
};
// The 80804CFC override selects a row in investment-global slot 5, 80805B8F.
// Ordinals below are that row's native choices, NOT the similarly named 342/344/346.
// Neither this binding nor package existence grants eligibility.
inline constexpr std::array<Beacon,3> kBeacons{{
    {"adventure.ascend.beacon","o_ia_ascend","ia_ascend_future",0,1076,0x80F5B960,0x7B03FFBD,0x3B660B3C},
    {"adventure.horde.beacon","o_ia_horde","ia_horde_future",1,1077,0x80F5B956,0x543A678B,0x3421737A},
    {"adventure.intercept.beacon","o_ia_intercept","ia_intercept_present",2,1078,0x80F5B959,0x25630B67,0xB39DD6F2},
}};
} // namespace dawn::state::activity::coo::adventure::mercury
