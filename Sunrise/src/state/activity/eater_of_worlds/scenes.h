#pragma once
#include "catalog.h"
namespace sunrise::state::activity::eater_of_worlds {
// Reproduced by tools/coo/extract_eater_scenes.py. Preserve the package cast
// ordering, including the non-replicated authored performance target.
inline constexpr coo::Asset kAlphaStrikeCast[]{
    {0xE8D290A0U,0xFFFFFFFFU,48,473},
    {0xE8D290A0U,0x8155C06FU,1,3},
};
inline constexpr coo::Asset kIntroCast[]{{0xE8D290A0U,0x8155C06FU,1,3}};
struct SceneBinding {coo::Asset source;std::uint32_t selector,graph,offset;std::span<const coo::Asset> cast;};
inline constexpr SceneBinding kScenes[]{
    {{0xE8D290A0U,0x80C421A1U,43,2},0x80F444FBU,0x80F444FAU,6632,kAlphaStrikeCast},
    {{0xE8D290A0U,0x80C424FFU,43,107},0x80F44500U,0x80F444FFU,3800,kIntroCast},
};
inline constexpr const SceneBinding* scene_binding(coo::Asset a) noexcept {
    for(const auto& s:kScenes) if(s.source==a) return &s;return nullptr;
}
}
