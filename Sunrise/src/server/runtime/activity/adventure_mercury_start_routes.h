#pragma once
#include "adventure_start_plan.h"
#include "adventure_mercury_capabilities.h"
namespace sunrise::server::runtime::activity::adventure::mercury {
inline constexpr auto kStartRoutes=[] {
    std::array<adventure_start::Route,authored::kBeacons.size()> routes{};
    for(std::size_t i=0;i<routes.size();++i) {
        routes[i]={0x80F4696A,"mercury_freeroam",0x2749BAAE,
            authored::kBeacons[i].slot,15,static_cast<std::int16_t>(authored::kBeacons[i].publicOrdinal)};
    }
    return routes;
}();
}
