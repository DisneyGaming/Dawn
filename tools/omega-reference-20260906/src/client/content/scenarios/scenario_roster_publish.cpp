#include <algorithm>
#include <cstddef>

#include "../../../middleware/content/packages/tables/roster_intersection.h"
#include "internal.h"

namespace dawn::client::content::scenarios {
namespace {

namespace tables = middleware::content::packages::tables;

/** Baseline plus the four exact groups present in the marker-working Omega roster. */
constexpr std::array<std::uint32_t, 6> kMeasuredOmegaRosterKeys = {
    0x4786C0E0U, 0x29D7B029U, 0x82FB58B7U, 0xBA5F26EFU, 0xD00142CFU, 0xF7A6CE7FU};

[[nodiscard]] bool measured_omega_roster_key(std::uint32_t key) noexcept {
    return std::find(kMeasuredOmegaRosterKeys.begin(), kMeasuredOmegaRosterKeys.end(), key)
           != kMeasuredOmegaRosterKeys.end();
}

/**
 * The dynamic builder discovers three different Omega mission groups. Appending the measured four
 * exceeds the bubble-group capacity and invalidates the entire row. Once all four measured keys
 * are present, replace that dynamic mission subset with the exact marker-working six-key roster.
 * If discovery was incomplete, remove the partial forced set and retain the dynamic fallback.
 */
[[nodiscard]] bool select_measured_omega_roster(Walk& walk) noexcept {
    constexpr std::uint8_t kCompleteMask = 0x0FU;
    const bool complete = walk.measuredOmegaMask == kCompleteMask;
    std::size_t output = 0;
    for (std::size_t index = 0; index < walk.candidateCount; ++index) {
        const Candidate& candidate = walk.candidates[index];
        const bool forced = measured_omega_key_bit(candidate.key) != 0;
        const bool keep = complete ? measured_omega_roster_key(candidate.key) : !forced;
        if (keep) {
            walk.candidates[output++] = candidate;
        }
    }
    walk.candidateCount = output;
    return complete;
}

[[nodiscard]] bool publishes_first(const Candidate& left, const Candidate& right) noexcept {
    const bool leftFilled = left.bindsPlayer || left.reportsLifetime;
    const bool rightFilled = right.bindsPlayer || right.reportsLifetime;
    if (leftFilled != rightFilled) {
        return leftFilled;
    }
    if (left.primaryRegistry != right.primaryRegistry) {
        return left.primaryRegistry;
    }
    return left.key < right.key;
}

void publish_top_level(Walk& walk, layouts::Definition& row) noexcept {
    std::array<std::uint32_t, tables::kRosterKeyCapacity> safe{};
    std::size_t safeCount = 0;
    if (!tables::safe_roster_keys(walk.intersection, safe, safeCount)) {
        walk.ordinaryOverflowed = walk.ordinaryOverflowed || walk.intersection.overflowed;
        return;
    }
    if (safeCount == 0) {
        return;
    }
    std::array<Candidate, tables::kRosterKeyCapacity> kept{};
    std::size_t keptCount = 0;
    for (std::size_t index = 0; index < walk.candidateCount; ++index) {
        const Candidate& candidate = walk.candidates[index];
        const auto last = safe.begin() + static_cast<std::ptrdiff_t>(safeCount);
        if (std::find(safe.begin(), last, candidate.key) != last && keptCount < kept.size()) {
            kept[keptCount++] = candidate;
        }
    }
    std::sort(kept.begin(), kept.begin() + static_cast<std::ptrdiff_t>(keptCount), publishes_first);
    bool binds = false;
    bool reports = false;
    for (std::size_t index = 0; index < keptCount; ++index) {
        binds = binds || kept[index].bindsPlayer;
        reports = reports || kept[index].reportsLifetime;
    }
    if (!binds || !reports) {
        return;
    }
    if (keptCount > layouts::kDestinationGroupCapacity) {
        walk.ordinaryOverflowed = true;
        return;
    }
    for (std::size_t index = 0; index < keptCount; ++index) {
        row.rosterGroups[index] = kept[index].group;
    }
    row.rosterGroupCount = static_cast<std::uint8_t>(keptCount);
}

void publish_per_bubble(Walk& walk, layouts::Definition& row) noexcept {
    std::array<std::uint32_t, tables::kRosterKeyCapacity> keys{};
    std::array<std::uint64_t, tables::kRosterKeyCapacity> masks{};
    std::size_t partialCount = 0;
    if (!tables::partial_roster_keys(walk.intersection, keys, masks, partialCount)) {
        walk.ordinaryOverflowed = walk.ordinaryOverflowed || walk.intersection.overflowed;
        return;
    }
    if (partialCount == 0) {
        return;
    }
    std::array<std::uint16_t, tables::kRosterKeyCapacity> groups{};
    std::array<std::uint64_t, tables::kRosterKeyCapacity> groupMasks{};
    std::size_t keptCount = 0;
    for (std::size_t index = 0; index < walk.candidateCount; ++index) {
        const Candidate& candidate = walk.candidates[index];
        for (std::size_t partial = 0; partial < partialCount; ++partial) {
            if (keys[partial] != candidate.key) {
                continue;
            }
            groups[keptCount] = candidate.group;
            groupMasks[keptCount] = masks[partial];
            ++keptCount;
            break;
        }
    }
    if (keptCount > layouts::kDestinationBubbleGroupCapacity) {
        walk.ordinaryOverflowed = true;
        return;
    }
    for (std::size_t index = 0; index < keptCount; ++index) {
        row.bubbleGroups[index] = groups[index];
        row.bubbleGroupMasks[index] = groupMasks[index];
    }
    row.bubbleGroupCount = static_cast<std::uint8_t>(keptCount);
}

enum class OrdinaryRelation : std::uint8_t { absent, sameLayout, conflictingLayout };

[[nodiscard]] OrdinaryRelation ordinary_relation(const layouts::Definition& row,
                                                  const RosterStorage& storage,
                                                  std::uint16_t group) noexcept {
    const std::uint32_t key = storage.groups[group].registryKey;
    for (std::size_t index = 0; index < row.rosterGroupCount; ++index) {
        if (row.rosterGroups[index] == group) {
            return OrdinaryRelation::sameLayout;
        }
        if (storage.groups[row.rosterGroups[index]].registryKey == key) {
            return OrdinaryRelation::conflictingLayout;
        }
    }
    for (std::size_t index = 0; index < row.bubbleGroupCount; ++index) {
        if (row.bubbleGroups[index] == group) {
            return OrdinaryRelation::sameLayout;
        }
        if (storage.groups[row.bubbleGroups[index]].registryKey == key) {
            return OrdinaryRelation::conflictingLayout;
        }
    }
    return OrdinaryRelation::absent;
}

void publish_authored(Walk& walk,
                      RosterStorage& storage,
                      layouts::Definition& row) noexcept {
    row.authoredGroupCounts = {};
    row.authoredGroups = {};
    if (walk.authoredUnresolved) {
        return;
    }
    const std::size_t ordinaryCount =
        std::size_t{row.rosterGroupCount} + std::size_t{row.bubbleGroupCount};
    for (std::size_t ordinal = 0; ordinal < walk.authored.size(); ++ordinal) {
        AuthoredSlice& slice = walk.authored[ordinal];
        if (!slice.observed || slice.root == kNotARosterGroup) {
            continue;
        }
        if (slice.keyConflict) {
            ++storage.keyConflicts;
            continue;
        }
        std::sort(slice.locals.begin(),
                  slice.locals.begin() + static_cast<std::ptrdiff_t>(slice.localCount),
                  [&storage](std::uint16_t left, std::uint16_t right) {
                      return storage.groups[left].registryKey < storage.groups[right].registryKey;
                  });
        std::array<std::uint16_t, layouts::kDestinationAuthoredGroupCapacity> authored{};
        std::size_t authoredCount = 1;
        authored[0] = slice.root;
        for (std::size_t local = 0; local < slice.localCount; ++local) {
            if (slice.locals[local] == slice.root) {
                continue;
            }
            if (authoredCount == authored.size()) {
                slice.overflowed = true;
                break;
            }
            authored[authoredCount++] = slice.locals[local];
        }
        std::size_t additionalCount = 0;
        for (std::size_t index = 0; index < authoredCount; ++index) {
            const OrdinaryRelation relation = ordinary_relation(row, storage, authored[index]);
            if (relation == OrdinaryRelation::conflictingLayout) {
                slice.keyConflict = true;
                break;
            }
            additionalCount += relation == OrdinaryRelation::sameLayout ? 0U : 1U;
        }
        if (slice.keyConflict) {
            ++storage.keyConflicts;
            continue;
        }
        if (slice.overflowed || authoredCount > layouts::kDestinationAuthoredGroupCapacity
            || ordinaryCount + additionalCount > layouts::kDestinationWireGroupCapacity) {
            ++storage.authoredOverflows;
            continue;
        }
        row.authoredGroupCounts[ordinal] = static_cast<std::uint8_t>(authoredCount);
        row.authoredGroups[ordinal] = authored;
    }
}

} // namespace

void publish_groups(Walk& walk, RosterStorage& storage, layouts::Definition& row) noexcept {
    row.rosterGroupCount = 0;
    row.rosterGroups = {};
    row.bubbleGroupCount = 0;
    row.bubbleGroups = {};
    row.bubbleGroupMasks = {};
    row.authoredGroupCounts = {};
    row.authoredGroups = {};
    if (walk.ordinaryKeyConflict) {
        ++storage.keyConflicts;
        return;
    }
    const bool measuredOmega = select_measured_omega_roster(walk);
    publish_top_level(walk, row);
    publish_per_bubble(walk, row);
    if (walk.ordinaryOverflowed) {
        row.rosterGroupCount = 0;
        row.rosterGroups = {};
        row.bubbleGroupCount = 0;
        row.bubbleGroups = {};
        row.bubbleGroupMasks = {};
        ++storage.ordinaryOverflows;
        return;
    }
    if (!measuredOmega) {
        publish_authored(walk, storage, row);
    }
}

} // namespace dawn::client::content::scenarios
