#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

namespace sunrise::state::activity::omega_crown_respawn {

// Authored global registry 80FEB3DC. The native E4C0A0 predicate requires both
// hard_wipe_globals byte0 and lifetime record+C matching the current scenario
// ordinal. Native 429BA0 -> 4C8FB0 converts packed slice 112 to ordinal 14.
inline constexpr std::uint32_t kRegistry = 0x4786C0E0U;
inline constexpr std::uint32_t kLairPackedSlice = 112;
inline constexpr std::uint32_t kLairScenarioOrdinal = 14;
static_assert((kLairPackedSlice >> 3) == kLairScenarioOrdinal);
inline constexpr std::size_t kDirectorBits = 359;
inline constexpr std::uint64_t kNativeYearTicks = 0x134F00C00000ULL;

// keep preserves unrelated/legacy authority; disable is an explicit native
// level-off publication, not omission of the director body.
enum class Restriction : std::uint8_t { keep, enable, disable };

[[nodiscard]] constexpr Restriction intent(Restriction explicitIntent,
                                           bool legacyRestricted) noexcept {
    return explicitIntent == Restriction::keep && legacyRestricted
        ? Restriction::enable : explicitIntent;
}
[[nodiscard]] constexpr bool publishes(bool validatedOmega, Restriction desired) noexcept {
    return validatedOmega && (desired == Restriction::enable || desired == Restriction::disable);
}
[[nodiscard]] constexpr bool restricted(Restriction desired) noexcept {
    return desired == Restriction::enable;
}

[[nodiscard]] constexpr bool enabled(bool validatedOmega, bool crownArrival,
                                      [[maybe_unused]] std::uint32_t advertisedRegion) noexcept {
    // Crown arrival is a run-bound typed native-volume observation. Msg22's
    // advertised region can regress to 88 while the native world remains 112;
    // it must not veto this stronger evidence. E4C0A0 filters the actual world.
    return validatedOmega && crownArrival;
}
[[nodiscard]] constexpr bool is_lifetime(std::uint32_t key, std::uint8_t type,
                                         std::uint16_t slot) noexcept {
    return key == kRegistry && type == 17 && slot == 3;
}
[[nodiscard]] constexpr bool is_director(std::uint32_t key, std::uint8_t type,
                                         std::uint16_t slot) noexcept {
    return key == kRegistry && type == 35 && slot == 1;
}

// Schema 808099BF with nested 808099C4. This sets the native restriction level;
// it neither starts a wipe countdown nor changes participation's respawn delay.
// All remaining fields match 501B20's constructed state: selector B = -1,
// inactive timer, one-year upper bound, unset timestamp and rate 1.0. The upper
// bound is the original 35F080 float conversion, not rounded integer seconds.
// Crown-arrival timing is reconstructed host policy, not a recovered script.
template<class Writer>
[[nodiscard]] bool write_director(Writer& writer, Restriction desired) noexcept {
    return publishes(true, desired)
        && writer.write(restricted(desired) ? 1U : 0U,1) && writer.write(0,1)
        && writer.write(1,2) && writer.write(0,2)
        && writer.write(0,1)
        && writer.write(0,64) && writer.write(kNativeYearTicks,64)
        && writer.write(0,64) && writer.write(0,64)
        && writer.write((std::numeric_limits<std::uint64_t>::max)(),64)
        && writer.write(0x3F800000U,32);
}

template<class Writer>
[[nodiscard]] bool write_restricted_director(Writer& writer) noexcept {
    return write_director(writer, Restriction::enable);
}

} // namespace sunrise::state::activity::omega_crown_respawn
