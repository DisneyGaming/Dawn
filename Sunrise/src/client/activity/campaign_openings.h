#pragma once

#include <array>
#include <span>
#include <string_view>
#include "../../state/activity/forced/definition.h"
#include "../../state/activity/forced/prelaunch_profile.h"
#include "../../state/build_data/activities/activity_catalog.h"

namespace sunrise::client::activity::mission_launch {
namespace openings {
namespace forced = state::activity::forced;

struct Mission {
    const char* title;
    const char* location;
    const char* description;
    unsigned campaign;
    forced::ForcedDestination destination;
};

// Measured successful Omega arrival: references/omega-native-success-20260827-155532.log.
inline constexpr auto kOmegaOpening = [] {
    forced::ForcedDestination value{};
    constexpr std::string_view name = "mission_scot";
    for (std::size_t i = 0; i < name.size(); ++i) { value.packageName[i] = name[i]; }
    value.packageNameLength = static_cast<std::uint8_t>(name.size());
    value.bubble = 15; value.sliceSet = 120; value.spawnSetHash = 0x4AB3287AU;
    value.hasBubble = value.hasSliceSet = value.hasSpawnSetHash = value.enabled = true;
    return value;
}();

inline constexpr std::array<Mission, 9> kMissions{{
    {"Homecoming", "THE LAST CITY", "Return to the Tower as the Red Legion attacks the Last City.", 0, forced::profiles::kTowerfallOpening},
    {"Gateway", "MERCURY", "Follow Ikora to Mercury and begin the search for Osiris.", 1, forced::profiles::kGatewayOpening},
    {"A Deadly Trial", "EUROPEAN DEAD ZONE", "Track a lead through the EDZ in search of a way into the Infinite Forest.", 1, forced::profiles::kDeadlyTrialOpening},
    {"Beyond Infinity", "MERCURY", "Enter the Infinite Forest and explore the Vex simulations.", 1, forced::profiles::kBeyondInfinityOpening},
    {"Deep Storage", "IO", "Search the Vex network on Io for the information you need.", 1, forced::profiles::kDeepStorageOpening},
    {"Tree of Probabilities", "MERCURY", "Follow the trail through the shifting paths of the Infinite Forest.", 1, forced::profiles::kStrikePactOpening},
    {"Hijacked", "NESSUS", "Find a Vex mind on Nessus to help locate Panoptes.", 1, forced::profiles::kHijackedOpening},
    {"A Garden World", "MERCURY", "Return to the Simulant Past and defeat Dendron, Root Mind. Strike version.", 1, forced::profiles::kStrikeBondOpening},
    {"Omega", "MERCURY", "Return to the Infinite Forest and confront Panoptes with Osiris.", 1, kOmegaOpening},
}};

struct Route {
    std::uint16_t transport{0xFFFF};
    forced::ForcedDestination destination{};
    [[nodiscard]] constexpr bool valid() const noexcept { return transport != 0xFFFF; }
};

// All curated openings use the existing Chosen donor, including missions with hidden native rows.
// Never substitute a similarly named activity if the pinned donor identity is unavailable.
[[nodiscard]] inline Route resolve(std::size_t mission,
    std::span<const state::build_data::activities::Definition> rows) noexcept {
    constexpr auto donor = forced::prelaunch::kDonorActivity;
    if (mission >= kMissions.size() || rows.size() <= donor
        || rows[donor].index != donor || rows[donor].name() != "mission_reunion") { return {}; }
    return {static_cast<std::uint16_t>(donor), kMissions[mission].destination};
}
} // namespace openings
} // namespace sunrise::client::activity::mission_launch
