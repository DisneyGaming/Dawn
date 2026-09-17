#pragma once
#include "catalog.h"
#include <cmath>
namespace dawn::state::activity::beyond_infinity::transit {
inline bool authored_contact(std::uint32_t registry,std::uint16_t slot,float x,float y,float z) noexcept {
    if(!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) { return false; }
    for(const auto& volume:kVolumes) {
        if(volume.asset.registry!=registry || volume.asset.slot!=slot) { continue; }
        if(x<volume.min.x || x>volume.max.x || y<volume.min.y || y>volume.max.y
            || z<volume.min.z || z>volume.max.z || volume.vertices.size()<3) { return false; }
        bool inside=false;auto previous=volume.vertices.back();
        for(const auto& current:volume.vertices) {
            if((current.y>y)!=(previous.y>y) && x<(previous.x-current.x)*(y-current.y)/(previous.y-current.y)+current.x) { inside=!inside; }
            previous=current;
        }
        return inside;
    }
    return false;
}
inline bool near_core(float x,float y,float z,float cx,float cy,float cz) noexcept {
    const auto dx=x-cx,dy=y-cy,dz=z-cz;return dx*dx+dy*dy+dz*dz<=9.F;
}
// Exact source placement, conservative reconstructed 3m contact coverage.
// Collider dimensions and original host teleport filters remain unresolved.
inline bool route_contact(std::uint8_t route,float x,float y,float z) noexcept {
    if(!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) { return false; }
    switch(route) {
    case 1:return near_core(x,y,z,-973.218506F,1072.88257F,-68.017967F); // 80F461BE
    case 2:return near_core(x,y,z,748.928650F,709.899658F,2.931518F); // 80F461C4
    case 3:return authored_contact(0x199D7650U,8,x,y,z); // forest_future._directive_volume
    case 4:return near_core(x,y,z,262.085175F,752.081970F,312.565247F) // 80F4604C
        || near_core(x,y,z,217.964676F,749.953613F,312.080292F); // 80F4604F
    case 5:return authored_contact(0x45AFDE9BU,1,x,y,z); // escape._directive_volume
    default:return false;
    }
}
}
