#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "../destination/definition.h"

namespace sunrise::state::activity::forced {

/** The bubble number picks one of the 64 wire slots. */
inline constexpr std::uint8_t kMaximumBubble = 63;
/** The slice-set index is a 10-bit bias-1 wire field, so this is the largest it can hold. */
inline constexpr std::uint16_t kMaximumSliceSet = 1'022;
/** The set normal arrivals use. */
inline constexpr std::uint32_t kDefaultSpawnSetHash = 0x2EA8FB98U;
/**
 * The hash that names no set, so the Client searches the loaded world itself.
 * A set belongs to the map, not the bubble. Send one the bubble lacks and nothing spawns.
 */
inline constexpr std::uint32_t kAbsentSpawnSetHash = 0x811C9DC5U;

/**
 * One operator-chosen destination that replaces what the client asked for.
 * Nothing here is saved. The process starts with no selection and the switch off.
 */
struct ForcedDestination {
    /** Destination package name, without its `:scenario_client` suffix. */
    std::array<char, destination::kPackageNameCapacity> packageName{};
    std::uint8_t packageNameLength{};
    std::uint8_t bubble{};
    std::uint16_t sliceSet{};
    /** Spawn-set name hash, used only when one was chosen. */
    std::uint32_t spawnSetHash{};
    bool hasBubble{};
    bool hasSliceSet{};
    bool hasSpawnSetHash{};
    /** True only for an authored cinematic destination whose scenario declares no world bubbles. */
    bool bubbleless{};
    /** The global switch. Off means the client's own selection stands. */
    bool enabled{};
};

/**
 * Tests whether a forced destination names enough to replace a client selection.
 * The spawn set is the one optional part. Without it the Client picks its own point.
 * @param value Candidate selection.
 * @return True when the switch is on and the destination, bubble, and slice set are all named.
 */
[[nodiscard]] constexpr bool active(const ForcedDestination& value) noexcept {
    const bool world = value.hasBubble && value.bubble <= kMaximumBubble && value.hasSliceSet
                       && value.sliceSet <= kMaximumSliceSet;
    const bool cinematic = value.bubbleless && !value.hasBubble && !value.hasSliceSet;
    return value.enabled && value.packageNameLength != 0
           && value.packageNameLength <= value.packageName.size() && (world || cinematic);
}

namespace profiles {

/** Measured Homecoming opening from mission_towerfall's installed scenario definition. */
inline constexpr char kTowerfallPackageName[] = "mission_towerfall";
/** Measured Underwatch opening used by the archived activity: bubble ordinal 9, region 72. */
inline constexpr std::uint8_t kTowerfallOpeningBubble = 9;
inline constexpr std::uint16_t kTowerfallOpeningSlice = 72;

/**
 * Builds the isolated Towerfall mission profile. The spawn remains absent deliberately: the
 * retained authored Chosen route owns the actual arrival point inside the opening bubble.
 */
[[nodiscard]] constexpr ForcedDestination towerfall_opening() noexcept {
    ForcedDestination value{};
    constexpr std::size_t packageLength = sizeof kTowerfallPackageName - 1;
    for (std::size_t index = 0; index < packageLength; ++index) {
        value.packageName[index] = kTowerfallPackageName[index];
    }
    value.packageNameLength = static_cast<std::uint8_t>(packageLength);
    value.bubble = kTowerfallOpeningBubble;
    value.sliceSet = kTowerfallOpeningSlice;
    value.hasBubble = true;
    value.hasSliceSet = true;
    value.enabled = true;
    return value;
}

inline constexpr ForcedDestination kTowerfallOpening = towerfall_opening();

} // namespace profiles

static_assert(active(profiles::kTowerfallOpening));

/**
 * Tests whether a candidate can be stored, complete or not.
 * A partial selection is kept so the interface can set one field at a time.
 * @param value Candidate selection.
 * @return True when every named field is inside its wire range.
 */
[[nodiscard]] constexpr bool storable(const ForcedDestination& value) noexcept {
    return value.packageNameLength <= value.packageName.size()
           && (!value.hasBubble || value.bubble <= kMaximumBubble)
           && (!value.hasSliceSet || value.sliceSet <= kMaximumSliceSet);
}

} // namespace sunrise::state::activity::forced
