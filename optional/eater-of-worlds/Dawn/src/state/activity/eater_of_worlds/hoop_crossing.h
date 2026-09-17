#pragma once
#include "catalog.h"
#include <array>
#include <cstddef>

namespace dawn::state::activity::eater_of_worlds {
struct HoopCylinder final {
    Point center{};
    float radius{},halfDepth{};
};

// Live native bodies use identity rotation. Each hkpCylinderShape runs from
// local z=0 through z=5 with radius 5, so its midplane is origin.z+2.5.
// The array is indexed by the authored hoop number, independent of source slot.
inline constexpr std::array<HoopCylinder,7> kHoopCylinders{{
    {{-81.F,-1102.F,-1493.75F},5.F,2.5F},
    {{-60.F,-1084.F,-1482.5F},5.F,2.5F},
    {{-61.F,-1079.5F,-1612.F},5.F,2.5F},
    {{-85.25F,-1094.25F,-1612.F},5.F,2.5F},
    {{-80.5F,-1108.F,-1753.F},5.F,2.5F},
    {{-65.25F,-1098.75F,-1753.75F},5.F,2.5F},
    {{-54.F,-1110.F,-1612.F},5.F,2.5F},
}};

// A valid passage crosses the cylinder's aperture midplane in either
// direction. Merely touching its curved side or remaining in its slab does not
// count. Sample age, player identity and maximum displacement are caller gates.
[[nodiscard]] constexpr bool crosses_hoop(const HoopCylinder& hoop,
                                           Point before,Point after) noexcept {
    const auto first=before.z-hoop.center.z;
    const auto second=after.z-hoop.center.z;
    if(!((first<0.F && second>=0.F) || (first>0.F && second<=0.F))) return false;
    const auto dz=after.z-before.z;
    if(dz==0.F) return false;
    const auto t=(hoop.center.z-before.z)/dz;
    if(t<0.F || t>1.F) return false;
    const auto x=before.x+(after.x-before.x)*t-hoop.center.x;
    const auto y=before.y+(after.y-before.y)*t-hoop.center.y;
    return x*x+y*y<=hoop.radius*hoop.radius;
}

[[nodiscard]] constexpr std::size_t crossed_hoop(Point before,Point after) noexcept {
    for(std::size_t i=0;i<kHoopCylinders.size();++i)
        if(crosses_hoop(kHoopCylinders[i],before,after)) return i;
    return kHoopCylinders.size();
}
[[nodiscard]] constexpr bool crossed_any_hoop(Point before,Point after) noexcept {
    return crossed_hoop(before,after)!=kHoopCylinders.size();
}
}
