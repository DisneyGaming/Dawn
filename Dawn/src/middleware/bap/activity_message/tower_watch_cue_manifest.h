#pragma once

#include <array>
#include <cstdint>

namespace dawn::middleware::bap::activity_message::tower_watch {

/** The package and roster records recovered for Homecoming's Tower Watch opening. */
inline constexpr char kPackage[] = "mission_towerfall";
inline constexpr std::uint32_t kMissionRuntimeRegistry = 0x4786C0E0U;
inline constexpr std::uint32_t kRootCueRegistry = 0x664128F4U;
inline constexpr std::uint32_t kTowerWatchRegistry = 0x9D8076E4U;
inline constexpr std::uint32_t kDirectiveType = 68U;
inline constexpr std::uint32_t kDirectiveIndex = 0U;
inline constexpr std::uint32_t kDialogueType = 53U;
inline constexpr std::uint32_t kDialogueIndex = 2U;
/** Wall-breach Scene controller recovered from the Tower Watch local registry. */
inline constexpr std::uint8_t kBreachSceneType = 43U;
inline constexpr std::uint16_t kBreachSceneIndex = 5U;
/** Package-authored selector referenced by the type-43/index-5 descriptor. */
inline constexpr std::uint32_t kBreachSceneSelector = 0x80B82771U;
/**
 * Active Scene anchor recovered from selector 0x80B82771's 0x80809C25 reference table.
 * This is the invariant type-2 action anchor used by the sibling Towerfall Scene definitions;
 * the varying cast/action references remain package-owned and are resolved by the native Scene.
 */
inline constexpr std::uint32_t kBreachSceneEntryRegistry = 0x57318E3BU;
inline constexpr std::uint8_t kBreachSceneEntryType = 2U;
inline constexpr std::uint16_t kBreachSceneEntryIndex = 1U;
/**
 * Safety gate for the recovered type-43 Scene authority body. The body decodes cleanly, but the
 * retail consumer currently dereferences an unresolved native runtime entry (0x314). Keep the
 * recovered descriptor available for diagnostics while preventing publication until the native
 * producer contract has been captured.
 */
inline constexpr bool kPublishBreachSceneAuthority = false;

enum class Stage : std::uint8_t {
    none = 0,
    opening = 1,
    breach = 2,
    pathUnlocked = 3,
};

/** One authored output row; cue keys come from Towerfall's 0x80804F72 table in package order. */
struct Beat final {
    Stage stage{};
    const char* name{};
    std::uint32_t directiveEvent{};
    std::uint8_t dialogueRecord{};
    std::int32_t scriptState{};
};

inline constexpr std::array<Beat, 3> kBeats{{
    {Stage::opening, "opening_objective_and_ghost", 0x4FCECAB6U, 0U, 1},
    {Stage::breach, "wall_breach_and_first_cabal", 0x432D2C95U, 1U, 2},
    {Stage::pathUnlocked, "encounter_clear_and_path_unlock", 0x432D2C96U, 2U, 3},
}};

/** Disproved startup choreography edge. Retained only so diagnostics can reject it explicitly. */
inline constexpr std::uint8_t kStartupChoreographyType = 3U;
inline constexpr std::uint16_t kStartupChoreographyIndex = 11U;
inline constexpr std::uint64_t kStartupChoreographyHash = 0x6018351690B58115ULL;

/** Movement-sensitive player monitor currently under test as Tower Watch's wall approach edge. */
inline constexpr std::uint32_t kWallApproachCandidateRegistry = 0x9027B6A1U;
inline constexpr std::uint8_t kWallApproachCandidateType = 30U;
inline constexpr std::uint16_t kWallApproachCandidateIndex = 3U;

/** Bubble-local engagement sensor used as a change edge after the first encounter starts. */
inline constexpr std::uint8_t kEncounterSenseType = 70U;
inline constexpr std::uint16_t kEncounterSenseIndex = 126U;

[[nodiscard]] constexpr const Beat* beat(Stage stage) noexcept {
    for (const Beat& candidate : kBeats) {
        if (candidate.stage == stage) {
            return &candidate;
        }
    }
    return nullptr;
}

[[nodiscard]] constexpr std::uint8_t value(Stage stage) noexcept {
    return static_cast<std::uint8_t>(stage);
}

} // namespace dawn::middleware::bap::activity_message::tower_watch
