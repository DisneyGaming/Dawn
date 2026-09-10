#pragma once
#include "adventure_mercury_overlays.h"
#include "adventure_regional_overlay.h"
namespace sunrise::server::runtime::activity::adventure::mercury {
// Installed host80F4696A entry80F46AD2, registry80F46C73, map26.
// Selected scenario arrays independently identify each root/local pair.
// Descriptor identities are the resolved definition tags, not wrapper references.
inline constexpr std::array<OverlaySlot,5> kAscendForestSlots{{
    {0,30,0x8080952F,0x80809531,0x80809532,0x80F46BCA},
    {1,30,0x8080952F,0x80809531,0x80809532,0x80F46BD4},
    {2,31,0x80809522,UINT32_MAX,0x80809524,0x80F46BD7},
    {6,57,0x808094D7,UINT32_MAX,UINT32_MAX,0x80F46BCD},
    {7,57,0x808094D7,UINT32_MAX,UINT32_MAX,0x80F46BDA},
}};
inline constexpr std::array<OverlaySlot,8> kHordeForestSlots{{
    {0,31,0x80809522,UINT32_MAX,0x80809524,0x80F46C22},
    {1,31,0x80809522,UINT32_MAX,0x80809524,0x80F46C18},
    {2,31,0x80809522,UINT32_MAX,0x80809524,0x80F46C1B},
    {3,31,0x80809522,UINT32_MAX,0x80809524,0x80F46C25},
    {4,30,0x8080952F,0x80809531,0x80809532,0x80F46C33},
    {5,30,0x8080952F,0x80809531,0x80809532,0x80F46C28},
    {8,57,0x808094D7,UINT32_MAX,UINT32_MAX,0x80F46C37},
    {9,57,0x808094D7,UINT32_MAX,UINT32_MAX,0x80F46C2B},
}};
inline constexpr std::array<OverlaySlot,6> kInterceptForestSlots{{
    {0,30,0x8080952F,0x80809531,0x80809532,0x80F46C5D},
    {1,30,0x8080952F,0x80809531,0x80809532,0x80F46C67},
    {2,31,0x80809522,UINT32_MAX,0x80809524,0x80F46C6A},
    {3,31,0x80809522,UINT32_MAX,0x80809524,0x80F46C6D},
    {4,57,0x808094D7,UINT32_MAX,UINT32_MAX,0x80F46C64},
    {5,57,0x808094D7,UINT32_MAX,UINT32_MAX,0x80F46C70},
}};
inline constexpr std::array<authored_overlay::Binding,3> kForestOverlays{{
    {0x80F4696A,0x80F464F6,0x47EA4CE8,"ia_ascend_future",12,
        {0x08551BCF,0x80F46D43,kAscendCueSlots},{0x5F699C75,0x80F46B1F,kAscendForestSlots}},
    {0x80F4696A,0x80F466E1,0x47EA4CE8,"ia_horde_future",12,
        {0x9104CE05,0x80F46D79,kHordeCueSlots},{0xFE73EA43,0x80F46BBC,kHordeForestSlots}},
    {0x80F4696A,0x80F467B0,0x47EA4CE8,"ia_intercept_present",12,
        {0x3DB08419,0x80F46D8D,kInterceptCueSlots},{0x11938C07,0x80F46BF6,kInterceptForestSlots}},
}};
}
