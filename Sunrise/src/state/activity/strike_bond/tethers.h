#pragma once
#include "catalog.h"
#include <cmath>
namespace sunrise::state::activity::strike_bond {
struct TetherBinding {coo::Asset source;std::uint16_t guardian,cube,center,shield;std::size_t lens;};
// Unused strike anomaly sources provide independent beam lifetimes; the cubes
// keep their original native assignments and authored placement.
inline constexpr TetherBinding kRouteTethers[]{
    {{0xC95ECB1AU,0x80F544FAU,4,150},105,111,107,108,5},
    {{0xC95ECB1AU,0x80F544F7U,4,149},121,127,123,124,6},
    {{0x2CB86C0FU,0x80F549F2U,4,233},244,250,246,247,14},
};
inline constexpr const TetherBinding* route_tether(coo::Asset a) noexcept {
    for(const auto& b:kRouteTethers) if(b.source==a) return &b;return nullptr;
}
struct TetherPose {std::array<float,4> rotation{};Point position{};float scale{};};
inline bool tether_pose(Point cube,Point guardian,TetherPose& out) noexcept {
    const float dx=guardian.x-cube.x,dy=guardian.y-cube.y,dz=guardian.z+3.2F-cube.z;
    const float length=std::sqrt(dx*dx+dy*dy+dz*dz);
    if(!std::isfinite(cube.x) || !std::isfinite(cube.y) || !std::isfinite(cube.z)
        || !std::isfinite(length) || length<.25F || length>60.F) return false;
    std::array<float,4> q{0.F,-dz/length,dy/length,1.F+dx/length};
    const float norm=std::sqrt(q[1]*q[1]+q[2]*q[2]+q[3]*q[3]);
    if(norm<.00001F) q={0.F,0.F,1.F,0.F};else for(auto& v:q) v/=norm;
    // Use the native 10 m beam, not a heavily shrunken 75 m beam. The latter
    // made the short tether much thinner than the original central line.
    out={q,cube,length/10.F};return true;
}
}
