#pragma once
#include <cmath>
#include <cstdint>
namespace sunrise::state::activity::strike_bond {
enum class BossMode : std::uint8_t { damage, parking, dormant, waking, dying };
enum class BossAnimation : std::uint8_t { asleep, parked, wakeStarted, awake, deathStarted };
struct BossCycle {
    BossMode mode{BossMode::damage};std::uint8_t cycle{},parked{},awakened{};
    bool asleep{},wakeStarted{},deathStarted{},platformStopped{},hasPosition{};
    float position{};
};
// S, P1 and P2 divide the native lap into thirds. Native interpolation and
// hierarchy still own the moving platform and its rider.
inline constexpr float boss_parking(std::uint8_t cycle) noexcept {return cycle==1?1.F/3.F:2.F/3.F;}
inline bool boss_position(float value) noexcept {return std::isfinite(value) && value>=0.F && value<=1.F;}
inline bool boss_at(float value,float target) noexcept {return boss_position(value) && std::abs(value-target)<.0001F;}
inline bool boss_intermission(BossMode mode) noexcept {
    return mode==BossMode::parking || mode==BossMode::dormant || mode==BossMode::waking;
}
}
