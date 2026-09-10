#pragma once
#include "mercury_registries.h"
#include "placement_service.h"
#include "adventure_mercury_capabilities.h"
namespace sunrise::server::runtime::activity::mercury {
// 80F5E5CD / F25B938B: vendor destination and its three effects (0..3),
// centre entry (4), side entry beside landing (5). Package placements retain
// their own routes: side (42.26,282.45,126.78), vendor (-1.98,250,313.77).
// Adventure portals 6..9 have separate activity availability and remain unarmed.
inline constexpr std::uint8_t kAdventurePlacementStart=6;
inline constexpr std::array<placement::Capability,9> kPlacements{{
    {&kRegistries[2],0},{&kRegistries[2],1},{&kRegistries[2],2},
    {&kRegistries[2],3},{&kRegistries[2],4},{&kRegistries[2],5},
    adventure::mercury::kPlacements[0],
    adventure::mercury::kPlacements[1],
    adventure::mercury::kPlacements[2],
}};
}
