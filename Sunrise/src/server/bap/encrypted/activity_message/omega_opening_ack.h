#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include "../../../../middleware/bap/activity_message/sense_update.h"
#include "../../../../state/activity/omega/omega_progression.h"
#include "../../../../state/build_data/scenarios/omega_crown_roster.h"

namespace sunrise::server::bap::encrypted::activity_message::omega_ack {
namespace service = middleware::bap::activity_message;

/** Exact measured Omega roster topology acknowledged by both recovered bootstrap forms. */
constexpr std::array<std::uint32_t, 6> kOmegaRosterKeys{{
    0x4786C0E0U,
    0x29D7B029U,
    0x82FB58B7U,
    0xBA5F26EFU,
    0xD00142CFU,
    0xF7A6CE7FU,
}};
constexpr std::uint16_t kOmegaTopLevelGroups = 3;
constexpr std::int16_t kOmegaBubble = 15;
/** The forest map-generator group is appended to the published roster (stable group set across
 *  the tunnel crossing keeps the authored objects alive); the client's acknowledgement then
 *  carries one extra entry for it, on the forest bubble. */
constexpr std::uint32_t kOmegaForestGeneratorKey = 0x2763EC97U;
constexpr std::int16_t kOmegaForestBubble = 11;
struct ExpectedSenseGroup final {
    std::uint32_t key;
    std::uint16_t firstObject;
    std::uint16_t objectCount;
};

struct ExpectedSenseObject final {
    std::uint32_t key;
    std::uint16_t slotIndex;
    std::uint8_t slotType;
};

/** Required object topology shared by the recovered 235-byte and 236-byte bootstraps. */
constexpr std::array<ExpectedSenseGroup, 2> kOmegaInitialGroups{{
    {0xBA5F26EFU, 0, 2},
    {0xD00142CFU, 2, 4},
}};
constexpr std::array<ExpectedSenseObject, 6> kOmegaInitialObjects{{
    {0xBA5F26EFU, 1, 23},
    {0xBA5F26EFU, 2, 70},
    {0xD00142CFU, 0, 1},
    {0xD00142CFU, 1, 43},
    {0xD00142CFU, 16, 23},
    {0xD00142CFU, 17, 70},
}};
constexpr std::uint32_t kOmegaFirstSenseGroupBits = 334;
/** Four object headers plus the fixed scene, mission, and monitor bodies. */
constexpr std::uint32_t kOmegaSecondSenseGroupFixedBits = 521;

[[nodiscard]] inline bool exact_omega_roster(
    const service::sense_update::SenseUpdate& update) noexcept {
    // Roster acknowledgements include stable future-area groups, even before those areas
    // load. Accept the exact legacy, forest, route, and reveal topologies.
    const bool withMission = update.rosterEntryCount == kOmegaRosterKeys.size() + 10;
    const bool withReveal = withMission || update.rosterEntryCount == kOmegaRosterKeys.size() + 5;
    const bool withRoute = withReveal || update.rosterEntryCount == kOmegaRosterKeys.size() + 3;
    const bool withGenerator = withRoute
        || update.rosterEntryCount == kOmegaRosterKeys.size() + 1;
    if (!update.hasRosterAcknowledgement
        || update.topLevelRosterCount != kOmegaTopLevelGroups
        || (update.rosterEntryCount != kOmegaRosterKeys.size() && !withGenerator)
        || update.bubbleBlockCount != (withRoute ? 3U : (withGenerator ? 2U : 1U))) {
        return false;
    }
    // The client orders bubble entries by bubble number, so the generator (bubble 11) sits
    // BEFORE the bubble-15 trio (measured: index 3 of 7). Accept it at any position while the
    // six measured entries keep their own relative order.
    std::size_t expectedIndex = 0;
    bool generatorSeen = !withGenerator;
    bool handoffSeen = !withRoute;
    bool crownSeen = !withRoute;
    bool bossSeen = !withReveal;
    bool revealSeen = !withReveal;
    std::uint8_t missionSeen=withMission?0:31;
    for (std::size_t index = 0; index < update.rosterEntryCount; ++index) {
        const service::sense_update::RosterEntry& entry = update.rosterEntries[index];
        if (!entry.active || entry.state != 0x83U) {
            return false;
        }
        if(withMission) {
            const auto& groups=state::build_data::scenarios::omega_crown_roster::groups;
            if(const auto* group=state::build_data::scenarios::omega_crown_roster::find(entry.registryKey)) {
                const auto bit=static_cast<std::uint8_t>(1U<<static_cast<unsigned>(group-groups.data()));
                if(entry.bubble!=14 || (missionSeen&bit)) return false;
                missionSeen|=bit;
                continue;
            }
        }
        if (!generatorSeen && entry.registryKey == kOmegaForestGeneratorKey
            && entry.bubble == kOmegaForestBubble) {
            generatorSeen = true;
            continue;
        }
        if (!handoffSeen && entry.registryKey == state::activity::omega::kHandoffGroup
            && entry.bubble == 11) {
            handoffSeen = true;
            continue;
        }
        if (!crownSeen && entry.registryKey == state::activity::omega::kCrownGroup
            && entry.bubble == 14) {
            crownSeen = true;
            continue;
        }
        if (!bossSeen && entry.registryKey == state::activity::omega::kBossGroup && entry.bubble == 14) {
            bossSeen = true;
            continue;
        }
        if (!revealSeen && entry.registryKey == state::activity::omega::kRevealGroup && entry.bubble == 14) {
            revealSeen = true;
            continue;
        }
        if (expectedIndex >= kOmegaRosterKeys.size()) {
            return false;
        }
        const std::int16_t expectedBubble =
            expectedIndex < kOmegaTopLevelGroups ? -1 : kOmegaBubble;
        if (entry.registryKey != kOmegaRosterKeys[expectedIndex]
            || entry.bubble != expectedBubble) {
            return false;
        }
        ++expectedIndex;
    }
    return generatorSeen && handoffSeen && crownSeen && bossSeen && revealSeen && missionSeen==31
        && expectedIndex == kOmegaRosterKeys.size();
}

[[nodiscard]] inline bool exact_body(const service::sense_update::SenseObject& object,
                              std::uint32_t bodyBits,
                              std::uint64_t first,
                              std::uint64_t second,
                              std::uint64_t third = 0) noexcept {
    return object.bodyBits == bodyBits && object.bodyFirst == first
           && object.bodySecond == second && object.bodyThird == third;
}

/** The initial type-1 object has two measured optional-field layouts with the same role. */
[[nodiscard]] inline bool omega_initial_type1_body(
    const service::sense_update::SenseObject& object) noexcept {
    return exact_body(object, 85, 0x8093180000000000ULL, 1)
           || exact_body(object, 92, 0x87F9263000000000ULL, 1);
}

[[nodiscard]] inline bool omega_initial_object_body(
    std::size_t index,
    const service::sense_update::SenseObject& object) noexcept {
    if (index == 0 || index == 4) {
        return exact_body(object,
                          167,
                          0xAFFFFFFFF3F80000ULL,
                          0x0BFFFFFFFAFFFFFFULL,
                          0x7F00000001ULL);
    }
    if (index == 1 || index == 5) {
        return exact_body(object, 54, 0x20800100000001ULL, 0);
    }
    if (index == 2) {
        return omega_initial_type1_body(object);
    }
    return index == 3 && exact_body(object, 75, 0xC000000048000000ULL, 1);
}

[[nodiscard]] inline bool exact_omega_initial_report(
    const service::sense_update::SenseUpdate& update) noexcept {
    if (!exact_omega_roster(update) || update.groupCount != kOmegaInitialGroups.size()
        || update.objectCount != kOmegaInitialObjects.size()) {
        return false;
    }
    for (std::size_t index = 0; index < kOmegaInitialGroups.size(); ++index) {
        const service::sense_update::SenseGroup& actual = update.groups[index];
        const ExpectedSenseGroup& expected = kOmegaInitialGroups[index];
        if (actual.registryKey != expected.key || actual.firstObject != expected.firstObject
            || actual.objectCount != expected.objectCount) {
            return false;
        }
    }
    if (update.groups[0].bodyBits != kOmegaFirstSenseGroupBits
        || update.groups[1].bodyBits
               != kOmegaSecondSenseGroupFixedBits + update.objects[2].bodyBits) {
        return false;
    }
    for (std::size_t index = 0; index < kOmegaInitialObjects.size(); ++index) {
        const service::sense_update::SenseObject& actual = update.objects[index];
        const ExpectedSenseObject& expected = kOmegaInitialObjects[index];
        if (actual.registryKey != expected.key || actual.slotType != expected.slotType
            || actual.slotIndex != expected.slotIndex
            || !omega_initial_object_body(index, actual)) {
            return false;
        }
    }
    return true;
}

} // namespace sunrise::server::bap::encrypted::activity_message::omega_ack
