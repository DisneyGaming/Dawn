#pragma once
#include "authored_registry.h"
#include <array>

namespace sunrise::state::activity::coo::mercury::public_event_rally {
// Complete package-derived rally roster. Activation remains a separate service.
inline constexpr std::array<registry::Slot,13> kSlots{{
    {0,4,0x80809927,0x8080992E,0x8080992F,0x80F5BF33},
    {1,23,0x80804F45,0x80804F47,0x80804F48,0x80F5BF36},
    {2,70,0x808094EE,0x808094F0,0x808094F1,0x80F5BF13},
    {3,68,0x80804F53,UINT32_MAX,0x80804F67,0x80F5BF07},
    {4,53,0x80804F4B,UINT32_MAX,0x80804F77,0x80F5BF0A},
    {5,11,0x80804E8E,UINT32_MAX,0x80804F58,0x80F5BF26},
    {6,71,0x80804F49,0x80804F56,0x80804F57,0x80F5BF0D},
    {7,32,0x80809556,UINT32_MAX,0x8080955A,0x80F5BF3C},
    {8,32,0x80809556,UINT32_MAX,0x8080955A,0x80F5BF3F},
    {9,32,0x80809556,UINT32_MAX,0x8080955A,0x80F5BF42},
    {10,32,0x80809556,UINT32_MAX,0x8080955A,0x80F5BF45},
    {11,32,0x80809556,UINT32_MAX,0x8080955A,0x80F5BF48},
    {12,32,0x80809556,UINT32_MAX,0x8080955A,0x80F5BF4B},
}};
inline constexpr registry::Definition kRegistry{"mercury_freeroam",0x80F4696A,0x85C38F77,0x80F5BF4E,0xA83A9175,15,kSlots};
}
