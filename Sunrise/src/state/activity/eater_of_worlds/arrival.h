#pragma once
#include "catalog.h"
#include <cmath>
namespace sunrise::state::activity::eater_of_worlds {
struct ArrivalState {
    std::uint32_t attempt{1};
    bool intro{},shellPosed{},launched{},hoop{},landed{},sampled{};
    Point previous{};
    std::uint64_t sampledAt{};
    void retry() noexcept {
        if(++attempt==0) ++attempt;
        hoop=landed=sampled=false;sampledAt=0;previous={};
    }
};
struct ArrivalMotion {
    std::uint64_t run{},observedAt{};
    std::uint32_t player{UINT32_MAX},attempt{};
    Point position{};
};
inline bool finite(Point p) noexcept {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}
// The six shell sections and the 23 boss platforms have separate native
// controls. Intro creation never requests a boss platform entity.
inline constexpr std::uint32_t kBarrierRegistry=0x91264981U,kArgosRegistry=0xE8D290A0U;
}
