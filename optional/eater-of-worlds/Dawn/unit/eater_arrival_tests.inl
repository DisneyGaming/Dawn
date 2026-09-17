#include "../src/state/activity/eater_of_worlds/hoop_crossing.h"

static void hoop_crossing_math() {
    constexpr auto hoop=m::kHoopCylinders[0];
    check(m::crosses_hoop(hoop,
        {hoop.center.x,hoop.center.y,hoop.center.z-3.F},
        {hoop.center.x,hoop.center.y,hoop.center.z+3.F}),
        "forward segment through the authored aperture crosses its midplane");
    check(m::crosses_hoop(hoop,
        {hoop.center.x,hoop.center.y,hoop.center.z+3.F},
        {hoop.center.x,hoop.center.y,hoop.center.z-3.F}),
        "reverse segment through the authored aperture also counts");
    check(m::crosses_hoop(hoop,
        {hoop.center.x+hoop.radius,hoop.center.y,hoop.center.z-3.F},
        {hoop.center.x+hoop.radius,hoop.center.y,hoop.center.z+3.F}),
        "segment on the aperture's lateral edge remains inside the cylinder");
    check(!m::crosses_hoop(hoop,
        {hoop.center.x+hoop.radius+.01F,hoop.center.y,hoop.center.z-3.F},
        {hoop.center.x+hoop.radius+.01F,hoop.center.y,hoop.center.z+3.F}),
        "segment outside the aperture radius misses deterministically");
    check(!m::crosses_hoop(hoop,
        {hoop.center.x,hoop.center.y,hoop.center.z-2.F},
        {hoop.center.x,hoop.center.y,hoop.center.z-1.F}),
        "movement within one half of the slab cannot count as passage");
    check(m::crossed_hoop(
        {m::kHoopCylinders[6].center.x,m::kHoopCylinders[6].center.y,m::kHoopCylinders[6].center.z-3.F},
        {m::kHoopCylinders[6].center.x,m::kHoopCylinders[6].center.y,m::kHoopCylinders[6].center.z+3.F})==6,
        "seven-cylinder lookup returns the exact authored hoop index");
}
