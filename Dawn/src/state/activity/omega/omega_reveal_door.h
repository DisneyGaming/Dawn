#pragma once
#include <array>
#include <cmath>

namespace dawn::state::activity::omega::reveal_door {
struct XY { float x, y; };
// 80F47913 +1450: tv_spawn_boss, vertices +1E40. The notch is the doorway.
inline constexpr std::array<XY, 8> spawn{{
    {-1512.511230F,467.360077F}, {-1472.080811F,467.360077F},
    {-1472.080811F,477.881348F}, {-1480.F,477.881348F},
    {-1480.F,473.512177F}, {-1505.F,473.512177F},
    {-1505.F,477.881348F}, {-1512.511230F,477.881348F}}};
// 80F47B42 +D10: tv_intro_area. Allows a sampled position beyond the thin
// spawn volume to acknowledge the doorway without requiring a timer.
inline constexpr std::array<XY, 6> intro{{
    {-1538.884644F,363.929352F}, {-1507.407104F,357.699768F},
    {-1474.701050F,356.985291F}, {-1441.122437F,365.325348F},
    {-1440.219604F,474.597504F}, {-1535.155273F,474.597504F}}};
template<std::size_t N> bool contains(const std::array<XY,N>& polygon, float x, float y) noexcept {
    bool inside = false;
    for (std::size_t i=0, j=N-1; i<N; j=i++) {
        const auto a=polygon[i], b=polygon[j];
        if ((a.y>y)!=(b.y>y) && x<(b.x-a.x)*(y-a.y)/(b.y-a.y)+a.x) inside=!inside;
    }
    return inside;
}
inline bool reached(float x, float y, float z) noexcept {
    if (!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)) return false;
    return (z>=-27.566336F && z<=30.433666F && contains(spawn,x,y))
        || (y<467.360077F && z>=-34.521420F && z<=5.478585F && contains(intro,x,y));
}
}
