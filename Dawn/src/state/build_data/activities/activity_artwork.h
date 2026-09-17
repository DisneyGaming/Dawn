#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace dawn::state::build_data::activities {
enum class Icon : std::uint8_t {
    missing, redWar, osiris, warmind, forsaken, shadowkeep, newLight,
    strike, raid, dungeon, patrol, social, crucible, gambit, adventure,
    nightmare, arena, heroic, forge, menagerie, sundial, reckoning, patrolKill, patrolCollect, patrolSurvey, patrolScan, patrolAssassinate, quest, seasonUndying, seasonDawn, seasonWorthy, seasonArrivals, dawning, festival, solstice, crimson, revelry, ironBanner, seasonForge, seasonDrifter, seasonOpulence, count
};
inline constexpr std::size_t kIconCount = static_cast<std::size_t>(Icon::count);
struct Artwork {
    std::uint32_t tag{};
    std::uint16_t width{}, height{};
    /** Native RGBA8 pixels. No game-owned texture/resource pointers cross this boundary. */
    std::vector<std::byte> pixels{};
};
[[nodiscard]] bool publish_artwork(std::array<Artwork, kIconCount>&& rows) noexcept;
[[nodiscard]] std::span<const Artwork> artwork() noexcept;
} // namespace dawn::state::build_data::activities
