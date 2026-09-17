#pragma once

#include "../../state/runtime/runtime.h"
#include "festival_bag_gear.h"
#include <array>
#include <cstdint>
#include <span>

namespace sunrise::server::web_service::festival_bags {

inline constexpr std::uint32_t kEva = 0x36D32C3CU;
inline constexpr std::uint32_t kRegular = 0xC637DCD3U;
inline constexpr std::uint32_t kEpic = 0xE1F6F647U;
inline constexpr std::uint32_t kCandy = 0xF372F896U;
inline constexpr std::uint32_t kCoins = 0x4750ED6FU;
inline constexpr std::uint32_t kGlimmer = 0xBC53E66EU;
inline constexpr std::array<std::uint32_t, 6> kPlanetaryMaterials{
    0x38AD9298U, 0x78117B13U, 0xCFE5782FU, 0x4DCCE8B3U, 0x02EDE537U, 0x01DD7E7DU};
inline constexpr std::array<std::uint32_t, 3> kUpgradeMaterials{
    0x3CF2E8E2U, 0xE5B38AD2U, 0xB19439E5U}; // Shards, Cores, Modules.

[[nodiscard]] constexpr bool matches(std::uint32_t hash) noexcept {
    return hash == kRegular || hash == kEpic;
}

[[nodiscard]] constexpr std::uint64_t next(std::uint64_t& seed) noexcept {
    seed += 0x9E3779B97F4A7C15ULL;
    auto value = seed;
    value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ULL;
    value = (value ^ (value >> 27)) * 0x94D049BB133111EBULL;
    return value ^ (value >> 31);
}

struct Plan {
    std::array<state::ProfileExchangePayout, 2> costs{};
    std::array<state::ProfileExchangePayout, 2> materials{};
    std::size_t costCount{};
    std::size_t materialCount{};
};

/** Build 86657 bag prices; contents follow contemporary 2019 observations carried into 2020.
 * Retail weights are unavailable. Equal selections, Glimmer steps and a one-unit upgrade
 * reward are explicit reconstruction policy, not recovered server values. No later-event
 * Prisms, Exotics, masks or Cipher-cache rewards are added to these pools.
 */
[[nodiscard]] inline Plan roll(std::uint32_t bag, std::uint64_t& seed) noexcept {
    Plan result{};
    if (!matches(bag))
        return result;
    const bool epic = bag == kEpic;
    result.costs[result.costCount++] = {kCandy, epic ? 300 : 150};
    if (epic)
        result.costs[result.costCount++] = {kCoins, 25};
    result.materials[result.materialCount++] = {kGlimmer, 500};
    if (epic) {
        if (next(seed) & 1U) {
            result.materials[0] = {kPlanetaryMaterials[next(seed) % kPlanetaryMaterials.size()],
                                   10};
        } else {
            result.materials[0].quantity = 500 * (1 + static_cast<std::int32_t>(next(seed) % 5));
        }
        result.materials[result.materialCount++] = {
            kUpgradeMaterials[next(seed) % kUpgradeMaterials.size()], 1};
    }
    return result;
}

[[nodiscard]] inline std::span<const Gear> gear_pool(std::uint32_t bag) noexcept {
    if (bag == kRegular)
        return kRareGear;
    if (bag == kEpic)
        return kLegendaryGear;
    return {};
}

} // namespace sunrise::server::web_service::festival_bags
