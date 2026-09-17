#pragma once
#include "omega_presentation_rules.h"
namespace dawn::state::activity::omega_crown_transit {
using omega_presentation::Point;
using omega_presentation::Landmark;
using omega_presentation::Triangle;
// The final approach has12 vertices/10 triangles, larger than the presentation
// landmarks. Retain its whole polygon rather than truncate or use its AABB.
struct Volume final {
    Landmark unused;
    std::uint32_t registry;
    std::uint16_t slot;
    Point minimum,maximum;
    std::array<Point,12> vertices;
    std::array<Triangle,10> triangles;
    std::uint8_t triangleCount;
};
[[nodiscard]] inline bool contains(const Volume& volume,Point point) noexcept {
    if(!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)
        || point.x<volume.minimum.x || point.y<volume.minimum.y || point.z<volume.minimum.z
        || point.x>volume.maximum.x || point.y>volume.maximum.y || point.z>volume.maximum.z) { return false; }
    const auto cross=[](Point a,Point b,Point c) noexcept {
        return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);
    };
    for(std::uint8_t i=0;i<volume.triangleCount;++i) {
        const auto triangle=volume.triangles[i];
        const auto a=cross(volume.vertices[triangle.a],volume.vertices[triangle.b],point);
        const auto b=cross(volume.vertices[triangle.b],volume.vertices[triangle.c],point);
        const auto c=cross(volume.vertices[triangle.c],volume.vertices[triangle.a],point);
        if((a>=0.F && b>=0.F && c>=0.F)||(a<=0.F && b<=0.F && c<=0.F)) { return true; }
    }
    return false;
}
// Exact authored type60 polygons. They observe route area entry, not grounded
// landing, pickup or dunk. The embedded Landmark field is inert metadata.
// Order: left/right charge platforms, left/right/final native portal arrivals,
// then final area arrival (BF03 proximity31/36 ->60/60), and the separate
// final cannon approach (99BD2FEB proximity31/118 ->60/137).
inline constexpr std::array<Volume,7> kRouteVolumes{{
    {Landmark::lair,0x0040BF06U,69,
     {-1425.6903076171875F,-104.12779235839844F,-47.04325866699219F},{-1395.373779296875F,-65.38795471191406F,-32.04325866699219F},
     {{{-1395.373779296875F,-79.2198257446289F,-47.04325866699219F},{-1408.0753173828125F,-65.38795471191406F,-47.04325866699219F},{-1425.6903076171875F,-91.16847229003906F,-47.04325866699219F},{-1416.4598388671875F,-104.12779235839844F,-47.04325866699219F}}},
     {{{3,0,1},{3,1,2}}},2},
    {Landmark::lair,0x0040BF05U,80,
     {-1584.739990234375F,-114.58885192871094F,-46.42744445800781F},{-1548.6790771484375F,-66.08267211914062F,-26.427444458007812F},
     {{{-1559.382080078125F,-114.58885192871094F,-46.42744445800781F},{-1548.6790771484375F,-99.15829467773438F,-46.42744445800781F},{-1574.1256103515625F,-66.08267211914062F,-46.42744445800781F},{-1584.739990234375F,-77.93523406982422F,-46.42744445800781F}}},
     {{{3,0,1},{3,1,2}}},2},
    {Landmark::lair,0x0040BF06U,81,
     {-1503.574462890625F,-139.10992431640625F,-43.63665008544922F},{-1479.3170166015625F,-118.72134399414062F,-23.63665008544922F},
     {{{-1479.7657470703125F,-118.72134399414062F,-43.63665008544922F},{-1503.574462890625F,-118.73176574707031F,-43.63665008544922F},{-1503.574462890625F,-139.10992431640625F,-43.63665008544922F},{-1479.3170166015625F,-139.10992431640625F,-43.63665008544922F}}},
     {{{1,2,3},{3,0,1}}},2},
    {Landmark::lair,0x0040BF05U,76,
     {-1503.574462890625F,-139.10992431640625F,-43.63665008544922F},{-1479.3170166015625F,-118.72134399414062F,-23.63665008544922F},
     {{{-1479.7657470703125F,-118.72134399414062F,-43.63665008544922F},{-1503.574462890625F,-118.73176574707031F,-43.63665008544922F},{-1503.574462890625F,-139.10992431640625F,-43.63665008544922F},{-1479.3170166015625F,-139.10992431640625F,-43.63665008544922F}}},
     {{{1,2,3},{3,0,1}}},2},
    {Landmark::lair,0x0040BF03U,73,
     {-1503.574462890625F,-465.3511962890625F,-9.177861213684082F},{-1479.3170166015625F,-444.9626159667969F,10.822138786315918F},
     {{{-1479.7657470703125F,-444.9626159667969F,-9.177861213684082F},{-1503.574462890625F,-444.9730529785156F,-9.177861213684082F},{-1503.574462890625F,-465.3511962890625F,-9.177861213684082F},{-1479.3170166015625F,-465.3511962890625F,-9.177861213684082F}}},
     {{{1,2,3},{3,0,1}}},2},
    {Landmark::lair,0x0040BF03U,60,
     {-1565.8834228515625F,-352.3605651855469F,-48.077056884765625F},{-1420.7716064453125F,-201.8621826171875F,-8.07705307006836F},
     {{{-1562.9730224609375F,-352.3605651855469F,-48.077056884765625F},{-1421.31201171875F,-350.92315673828125F,-48.07705307006836F},{-1420.7716064453125F,-201.8621826171875F,-48.077056884765625F},{-1534.186767578125F,-201.8621826171875F,-48.077056884765625F},{-1534.186767578125F,-281.10784912109375F,-48.077056884765625F},{-1495.43115234375F,-281.1317443847656F,-48.077056884765625F},{-1495.43115234375F,-277.94073486328125F,-48.077056884765625F},{-1483.5457763671875F,-277.78753662109375F,-48.077056884765625F},{-1483.5457763671875F,-285.0872802734375F,-48.077056884765625F},{-1495.3477783203125F,-285.0872802734375F,-48.077056884765625F},{-1495.3477783203125F,-281.3077392578125F,-48.077056884765625F},{-1565.8834228515625F,-281.5955505371094F,-48.077056884765625F}}},
     {{{2,3,4},{4,5,6},{9,10,11},{11,0,1},{2,4,6},{2,6,7},{1,2,7},{1,7,8},{1,8,9},{11,1,9}}},10},
    {Landmark::lair,0x99BD2FEBU,137,
     {-1505.851318359375F,-142.78689575195312F,-56.999752044677734F},{-1477.388671875F,-121.57508087158203F,-41.999752044677734F},
     {{{-1478.735107421875F,-142.78689575195312F,-56.999752044677734F},{-1477.388671875F,-124.66092681884766F,-56.999752044677734F},{-1502.7596435546875F,-121.57508087158203F,-56.999752044677734F},{-1505.851318359375F,-141.68255615234375F,-56.999752044677734F}}},
     {{{1,2,3},{3,0,1}}},2}
}};

/** Original4A55A0/398010 uses a tolerant winding count, including its native
 * near-edge behavior, rather than strict triangle signs. Scope this exact flat
 * volume evaluator to the newly recovered final-cannon trigger. Existing route
 * observations retain their established conservative boundary policy. */
[[nodiscard]] inline bool contains_final_cannon(Point point) noexcept {
    const auto& volume=kRouteVolumes[6];
    constexpr float tolerance=0.01F;
    if(!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)
        || !(point.x>volume.minimum.x-tolerance && point.x<volume.maximum.x+tolerance
            && point.y>volume.minimum.y-tolerance && point.y<volume.maximum.y+tolerance
            && point.z>volume.minimum.z-tolerance && point.z<volume.maximum.z+tolerance)) { return false; }
    for(std::uint8_t i=0;i<volume.triangleCount;++i) {
        const auto triangle=volume.triangles[i];
        const std::array<Point,3> vertices{volume.vertices[triangle.a],volume.vertices[triangle.b],volume.vertices[triangle.c]};
        int winding{};auto previous=vertices.back();
        for(const auto current:vertices) {
            const auto dx=current.x-previous.x,dy=current.y-previous.y;
            const auto cross=(point.y-previous.y)*dx-(point.x-previous.x)*dy;
            const auto lengthSquared=dx*dx+dy*dy;
            const auto distanceSquared=lengthSquared<=0.00000001F?0.F:cross*cross/lengthSquared;
            const bool above=point.y-tolerance<current.y,below=previous.y<=point.y+tolerance;
            const bool left=distanceSquared<=tolerance*tolerance || cross>=0.F;
            if(above && below && left) { ++winding; }
            if(!below && !above && !left) { --winding; }
            previous=current;
        }
        if(winding!=0) { return true; }
    }
    return false;
}
} // namespace dawn::state::activity::omega_crown_transit
