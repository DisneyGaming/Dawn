#pragma once
#include "mercury_ambient_catalog.h"

namespace sunrise::state::activity::coo::mercury::ambient::primary_owner {
// Exact free-roam list80F46C89 indices41(owner),3(Cabal area). Capability
// extraction is separate from optional document-selected roster admission.
inline constexpr std::array<registry::Slot,8> kSlots{{
    {0,1,0x80809A3B,0x80807ECC,0x80807EC9,0x80F5B990},
    {1,1,0x80809A3B,0x80807ECC,0x80807EC9,0x80F5B994},
    {2,1,0x80809A3B,0x80807ECC,0x80807EC9,0x80F5B997},
    {3,3,0x80808348,0x80807F04,0x80807F0C,0x80F5B9B8},
    {4,4,0x80809927,0x8080992E,0x8080992F,0x80F5B9BB},
    {5,70,0x808094EE,0x808094F0,0x808094F1,0x80F5B99A},
    {10,66,0x808094CF,UINT32_MAX,UINT32_MAX,0x80F5B98A},
    {16,66,0x808094CF,UINT32_MAX,UINT32_MAX,0x80F5B98D},
}};
inline constexpr std::array<registry::Definition,2> kRegistries{{
    {"mercury_freeroam",0x80F4696A,0x4A3E4900,0x80F5B9C1,0xA83A9175,15,kSlots},
    *find(0x2571C34D)->registry,
}};
[[nodiscard]] constexpr bool required(std::uint32_t scenario,std::uint32_t object,
    std::uint32_t key,std::uint64_t scope) noexcept {
    for(const auto& definition:kRegistries)
        if(registry::required(definition,scenario,object,key,scope))return true;
    return false;
}
}
