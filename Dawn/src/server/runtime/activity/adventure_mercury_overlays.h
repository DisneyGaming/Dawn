#pragma once
#include "adventure_authored_overlay.h"

namespace dawn::server::runtime::activity::adventure::mercury {
using OverlaySlot=registry::Slot;
// Exact descriptor-backed slots from installed package objects. Host-only
// output declarations (including type69 event keys) are not invented as slots.
inline constexpr std::array<OverlaySlot,4> kAscendCueSlots{{
    {0,68,0x80804F53,UINT32_MAX,0x80804F67,0x80F46D67},
    {1,11,0x80804E8E,UINT32_MAX,0x80804F58,0x80F46D3D},
    {2,53,0x80804F4B,UINT32_MAX,0x80804F77,0x80F46D63},
    {3,19,0x80809525,UINT32_MAX,0x80809527,0x80F46D37},
}};
inline constexpr std::array<OverlaySlot,3> kHordeCueSlots{{
    {0,68,0x80804F53,UINT32_MAX,0x80804F67,0x80F46D7B},
    {1,11,0x80804E8E,UINT32_MAX,0x80804F58,0x80F46D73},
    {2,53,0x80804F4B,UINT32_MAX,0x80804F77,0x80F46D77},
}};
inline constexpr std::array<OverlaySlot,3> kInterceptCueSlots{{
    {0,68,0x80804F53,UINT32_MAX,0x80804F67,0x80F46D8B},
    {1,11,0x80804E8E,UINT32_MAX,0x80804F58,0x80F46D87},
    {2,53,0x80804F4B,UINT32_MAX,0x80804F77,0x80F46D8F},
}};
inline constexpr std::array<OverlaySlot,6> kAscendOpeningSlots{{
    {0,31,0x80809522,UINT32_MAX,0x80809524,0x80F46C91},
    {1,30,0x8080952F,0x80809531,0x80809532,0x80F46C94},
    {2,30,0x8080952F,0x80809531,0x80809532,0x80F46CA2},
    {3,31,0x80809522,UINT32_MAX,0x80809524,0x80F46C97},
    {4,57,0x808094D7,UINT32_MAX,UINT32_MAX,0x80F46C9E},
    {5,57,0x808094D7,UINT32_MAX,UINT32_MAX,0x80F46CA5},
}};
inline constexpr std::array<OverlaySlot,3> kHordeOpeningSlots{{
    {0,31,0x80809522,UINT32_MAX,0x80809524,0x80F46CDE},
    {1,30,0x8080952F,0x80809531,0x80809532,0x80F46CE1},
    {3,57,0x808094D7,UINT32_MAX,UINT32_MAX,0x80F46CE9},
}};
inline constexpr std::array<OverlaySlot,5> kInterceptOpeningSlots{{
    {0,30,0x8080952F,0x80809531,0x80809532,0x80F46D01},
    {1,30,0x8080952F,0x80809531,0x80809532,0x80F46D0B},
    {2,31,0x80809522,UINT32_MAX,0x80809524,0x80F46D0F},
    {3,57,0x808094D7,UINT32_MAX,UINT32_MAX,0x80F46D16},
    {10,57,0x808094D7,UINT32_MAX,UINT32_MAX,0x80F46D08},
}};
// Mercury Lighthouse entry80F46AD5 / registry80F46C89 contains these roots
// in primary registry0 and locals in registry2. Each selected scenario's
// authored Lighthouse overlay proves its corresponding pair and order.
inline constexpr std::array<authored_overlay::Binding,3> kOpeningOverlays{{
    {0x80F4696A,0x80F464F6,0xA83A9175,"ia_ascend_future",15,
        {0x08551BCF,0x80F46D43,kAscendCueSlots},{0x4600E2C0,0x80F46C36,kAscendOpeningSlots}},
    {0x80F4696A,0x80F466E1,0xA83A9175,"ia_horde_future",15,
        {0x9104CE05,0x80F46D79,kHordeCueSlots},{0x720E1336,0x80F46CAD,kHordeOpeningSlots}},
    {0x80F4696A,0x80F467B0,0xA83A9175,"ia_intercept_present",15,
        {0x3DB08419,0x80F46D8D,kInterceptCueSlots},{0x0632668A,0x80F46CCF,kInterceptOpeningSlots}},
}};
}
