#pragma once

#include <array>
#include <cmath>
#include <cstdint>

namespace dawn::server::bap::encrypted::forest_cache_policy {

// Installed Cipher Decoder definition and its Encrypted Cache preview.
inline constexpr std::uint32_t kDecoder = 0xE31B110BU;
inline constexpr std::uint32_t kLegendaryEngram = 0x03FBACBBU;
inline constexpr std::array<std::uint32_t,20> kMasks{{
    0xC0149F44U,0xC0149F45U,0xC0149F46U,0xC0149F47U,0xC0149F40U,
    0xC0149F41U,0xC0149F42U,0xC0149F43U,0xC0149F4CU,0xC0149F4DU,
    0xC662FA25U,0xC662FA24U,0x591A1862U,0x591A1863U,0x591A1860U,
    0x591A1861U,0x591A1866U,0x591A1867U,0x47A1BEE7U,0x47A1BEE6U}};

// Registry 34D23982, native slots 63..67; the order is authored, not numerical naming.
inline constexpr std::array<std::array<float,3>,5> kPositions{{
    {{-368.7128F,-646.7294F,1190.6512F}},
    {{-370.7128F,-647.2294F,1190.6512F}},
    {{-366.7128F,-647.2294F,1190.6512F}},
    {{-372.7128F,-648.7294F,1190.6512F}},
    {{-364.7128F,-648.7294F,1190.6512F}}}};

// -1: branch content; -2: reward-room point that is not one unambiguous cache.
[[nodiscard]] inline int cache_at(const std::array<float,3>& position) noexcept {
    for (float value:position) if (!std::isfinite(value)) return -2;
    int found=-1;
    for (std::size_t i=0;i<kPositions.size();++i) {
        float distance{};
        for (std::size_t axis=0;axis<3;++axis) {
            const auto delta=position[axis]-kPositions[i][axis];distance+=delta*delta;
        }
        if (distance>0.75F*0.75F) continue;
        if (found!=-1) return -2;
        found=static_cast<int>(i);
    }
    if (found!=-1) return found;
    if (std::abs(position[0]+368.7128F)<10.F
        && std::abs(position[1]+650.7294F)<10.F
        && std::abs(position[2]-1190.6512F)<5.F) return -2;
    return -1;
}

struct Plan {std::array<std::uint32_t,3> items{};std::uint8_t count{};};
[[nodiscard]] inline Plan roll(std::uint64_t seed) noexcept {
    // Host balancing, not a recovered retail probability: one or two engrams,
    // with a separate 20% chance of a mask from the installed cache preview.
    seed+=0x9E3779B97F4A7C15ULL;
    seed=(seed^(seed>>30))*0xBF58476D1CE4E5B9ULL;
    seed=(seed^(seed>>27))*0x94D049BB133111EBULL;
    seed^=seed>>31;
    Plan result{};
    result.items[result.count++]=kLegendaryEngram;
    if (seed&1U) result.items[result.count++]=kLegendaryEngram;
    if ((seed>>1)%5==0) result.items[result.count++]=kMasks[(seed>>8)%kMasks.size()];
    return result;
}
} // namespace dawn::server::bap::encrypted::forest_cache_policy
