#pragma once
#include <cmath>
#include "strike_bond_fire_trace.h"
namespace dawn::client::hooks::bootflow::strike_bond_carriage {
namespace mission=state::activity::strike_bond;
inline constexpr std::uint32_t kSocket=0x9F6DB313U;
inline constexpr std::uint32_t kBone=1;
inline bool wanted(const mission::BossRequest& r) noexcept {
    if(!strike_bond_fire_trace::admitted(r) || r.frame.ending || r.frame.region!=136
        || !r.platform.valid() || r.platform.source!=mission::kBossPlatform
        || r.platform.owner.run!=r.owner.run) return false;
    const auto& s=r.frame.native[mission::asset_index(mission::kBossPlatform)];
    return s.managed && s.desired && s.prepared && s.active && s.acknowledged
        && r.platform.owner.value==s.generation;
}
template<class T,std::size_t N> T at(const std::array<std::byte,N>& b,std::size_t offset) noexcept {
    return strike_bond_fire_trace::field<T>(b,offset);
}
// Captured authored marker 80F45991, row 0. Its second word names bone 1,
// while its hash is the lower plate socket. Do not use the entity root (bone 0).
inline bool socket(const std::array<std::byte,64>& marker) noexcept {
    return at<std::uint32_t>(marker,0)==0 && at<std::uint32_t>(marker,4)==kBone
        && at<std::uint32_t>(marker,0x30)==kSocket;
}
inline bool pose(const std::array<std::byte,80>& p,std::uint32_t provider) noexcept {
    if(at<std::uint32_t>(p,0x30)!=provider || provider==UINT32_MAX
        || at<std::uint32_t>(p,0x34)!=0x80809663U || at<std::int64_t>(p,0x38)!=0x30
        || at<std::uint64_t>(p,0x40)!=0) return false;
    for(std::size_t o=0x10;o<0x30;o+=4) if(!std::isfinite(at<float>(p,o))) return false;
    float norm{};for(std::size_t o=0x10;o<0x20;o+=4) {const auto v=at<float>(p,o);norm+=v*v;}
    return std::abs(norm-1.F)<.01F && std::abs(at<float>(p,0x2C)-1.F)<.01F;
}
inline bool parent_interface(const std::array<std::byte,40>& p,std::uint32_t body) noexcept {
    return body!=UINT32_MAX && at<std::uint32_t>(p,0)==0x80F4596FU
        && at<std::uint32_t>(p,4)==0x808089FDU && at<std::int64_t>(p,8)==0x2B0
        && at<std::uint32_t>(p,0x10)==0x80C70D35U && at<std::uint32_t>(p,0x18)==body
        && at<std::uint32_t>(p,0x1C)==0x80808545U && at<std::int64_t>(p,0x20)==0;
}
}
