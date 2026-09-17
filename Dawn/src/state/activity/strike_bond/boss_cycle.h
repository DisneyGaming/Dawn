#pragma once
#include <cmath>
#include <cstdint>
namespace dawn::state::activity::strike_bond {
enum class BossMode : std::uint8_t { damage, parking, dormant, waking, dying, opening };
enum class BossAnimation : std::uint8_t { asleep, parked, wakeStarted, awake, deathStarted, openingStarted, openingAwake };
struct PlatformMotion {
    float position{},target{},velocity{};std::int32_t revision{-1};
};
struct PlatformTravel {
    float target{},origin{};bool requested{},rebasing{};
};
struct BossCycle {
    BossMode mode{BossMode::damage};std::uint8_t cycle{},parked{},awakened{};
    bool asleep{},wakeStarted{},deathStarted{},platformStopped{},hasPosition{};
    bool hasParkTarget{},openingStarted{},openingComplete{},hasHome{},hasReturnTarget{},hasFirstTarget{};
    float position{},parkTarget{},home{},returnTarget{},firstTarget{};
};
// P1 was visually centered at0.3758; P2 applies the same boss-body correction
// against its authored beam angle. Each point repeats after half the input range.
// The native animation covers two revolutions; its input is not a sector index.
// Native interpolation and hierarchy own the platform and its rider.
inline constexpr float boss_parking(std::uint8_t cycle) noexcept {return cycle==1?.3758F:.6259F;}
inline bool boss_position(float value) noexcept {return std::isfinite(value) && value>=0.F && value<=1.F;}
inline bool boss_at(float value,float target) noexcept {return boss_position(value) && std::abs(value-target)<.0001F;}
inline float boss_nearest_phase(float first,float position) noexcept {
    const auto other=first<.5F?first+.5F:first-.5F;
    return std::abs(other-position)<std::abs(first-position)?other:first;
}
inline float boss_nearest_parking(std::uint8_t cycle,float position) noexcept {
    return boss_nearest_phase(boss_parking(cycle),position);
}
// Near an input endpoint, the equivalent phase half a range away avoids a
// full extra turn. Both phases have the same native world pose; travel itself
// always uses normal interpolation, never a snap to the destination.
inline float boss_parking_origin(float position,float target) noexcept {
    return std::abs(position-target)>.25F?position+(position<target?.5F:-.5F):position;
}
inline bool boss_intermission(BossMode mode) noexcept {
    return mode==BossMode::parking || mode==BossMode::dormant || mode==BossMode::waking;
}
}
