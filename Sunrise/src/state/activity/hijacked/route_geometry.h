#pragma once
#include "catalog.h"
#include <cmath>
namespace sunrise::state::activity::hijacked {
// User-selected cleanup point: screenshot + read-only player position in
// PID65288 (111.1,232.5,-80.7), scenario bubble37. This is authored geometry,
// not a recovered native trigger. Keep the earlier tunnel-mouth cue separate.
inline constexpr Point kExteriorCleanupMin{104.F,230.F,-86.F};
inline constexpr Point kExteriorCleanupMax{118.F,240.F,-74.F};
inline bool exterior_cleanup_contains(Point p) noexcept {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z)
        && p.x>=kExteriorCleanupMin.x && p.x<=kExteriorCleanupMax.x
        && p.y>=kExteriorCleanupMin.y && p.y<=kExteriorCleanupMax.y
        && p.z>=kExteriorCleanupMin.z && p.z<=kExteriorCleanupMax.z;
}
}
