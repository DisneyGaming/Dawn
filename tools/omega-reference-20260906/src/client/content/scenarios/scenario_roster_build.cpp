#include <array>
#include <cstdio>

#include "../../../core/logging/log.h"
#include "../../../middleware/content/packages/tables/roster_intersection.h"
#include "../../../middleware/content/packages/tables/scenario_reader.h"
#include "internal.h"

namespace sunrise::client::content::scenarios {
namespace {

namespace tables = middleware::content::packages::tables;

/** The three registry descriptors, walked in this order to match the reference walk. */
constexpr std::array<std::size_t, 3> kRegistryDescriptors = {
    tables::kRegistryFirstDescriptor,
    tables::kRegistrySecondDescriptor,
    tables::kRegistryThirdDescriptor,
};

/** Destinations between progress lines. */
constexpr std::size_t kRosterProgressInterval = 64;

/**
 * Reports how far the walk has reached.
 * @param walked Destinations walked so far.
 * @param total Destinations to walk.
 * @param groups Roster groups found so far.
 */
void report_progress(std::size_t walked, std::size_t total, std::size_t groups) noexcept {
    std::array<char, core::log::kLineCapacity> line{};
    const int written = std::snprintf(line.data(),
                                      line.size(),
                                      "ev=build_data stage=roster walked=%zu of=%zu groups=%zu",
                                      walked,
                                      total,
                                      groups);
    if (written > 0) {
        core::log::write(core::log::Channel::state,
                         core::log::Level::debug,
                         {line.data(), static_cast<std::size_t>(written)});
    }
}

/** The two slot types a usable roster must carry between all of its groups. */
constexpr std::uint8_t kSlotTypeParticipation = 13;
constexpr std::uint8_t kSlotTypeLifetime = 17;

/**
 * Records one candidate group, or updates the one already recorded for its key.
 * @param walk Accumulator for one destination.
 * @param storage Working storage holding the group table.
 * @param group Roster group index.
 * @param primary True when the object was reached through the destination's own registry array.
 */
void note_candidate(Walk& walk,
                    const RosterStorage& storage,
                    std::uint16_t group,
                    bool primary) noexcept {
    for (std::size_t index = 0; index < walk.candidateCount; ++index) {
        if (walk.candidates[index].group == group) {
            walk.candidates[index].primaryRegistry =
                walk.candidates[index].primaryRegistry || primary;
            return;
        }
        if (walk.candidates[index].key == storage.groups[group].registryKey) {
            walk.ordinaryKeyConflict = true;
            return;
        }
    }
    if (walk.candidateCount == walk.candidates.size()) {
        walk.ordinaryOverflowed = true;
        return;
    }
    const layouts::RosterGroup& row = storage.groups[group];
    walk.measuredOmegaMask = static_cast<std::uint8_t>(
        walk.measuredOmegaMask | measured_omega_key_bit(row.registryKey));
    Candidate& candidate = walk.candidates[walk.candidateCount++];
    candidate.group = group;
    candidate.key = row.registryKey;
    candidate.primaryRegistry = primary;
    for (std::size_t slot = 0; slot < row.slotCount; ++slot) {
        candidate.bindsPlayer =
            candidate.bindsPlayer || row.slotTypes[slot] == kSlotTypeParticipation;
        candidate.reportsLifetime =
            candidate.reportsLifetime || row.slotTypes[slot] == kSlotTypeLifetime;
    }
}

void note_authored(const RosterStorage& storage,
                   AuthoredObservation& observation,
                   const ResolvedObject& resolved) noexcept {
    if (resolved.authoredRejected) {
        observation.overflowed = true;
    }
    if (resolved.group == kNotARosterGroup) {
        return;
    }
    if (resolved.scenarioRoot) {
        if (observation.root == kNotARosterGroup) {
            observation.root = resolved.group;
        } else if (observation.root != resolved.group) {
            observation.overflowed = true;
            observation.keyConflict = true;
        }
    }
    if (!resolved.selectedLocal) {
        return;
    }
    if (observation.root != kNotARosterGroup && observation.root != resolved.group
        && storage.groups[observation.root].registryKey
               == storage.groups[resolved.group].registryKey) {
        observation.overflowed = true;
        observation.keyConflict = true;
        return;
    }
    for (std::size_t index = 0; index < observation.localCount; ++index) {
        if (observation.locals[index] == resolved.group) {
            return;
        }
        if (storage.groups[observation.locals[index]].registryKey
            == storage.groups[resolved.group].registryKey) {
            observation.overflowed = true;
            observation.keyConflict = true;
            return;
        }
    }
    if (observation.localCount == observation.locals.size()) {
        observation.overflowed = true;
        return;
    }
    observation.locals[observation.localCount++] = resolved.group;
}

/** Unions authored objects that explicitly claim the same slice, rejecting layout ambiguity. */
void merge_authored(const RosterStorage& storage,
                    AuthoredSlice& slice,
                    const AuthoredObservation& observation) noexcept {
    if (!slice.observed) {
        slice.root = observation.root;
        slice.locals = observation.locals;
        slice.localCount = observation.localCount;
        slice.observed = true;
        slice.overflowed = observation.overflowed;
        slice.keyConflict = observation.keyConflict;
        return;
    }
    if (observation.root != kNotARosterGroup) {
        if (slice.root == kNotARosterGroup) {
            slice.root = observation.root;
        } else if (slice.root != observation.root) {
            slice.keyConflict = true;
            slice.overflowed = true;
        }
    }
    for (std::size_t current = 0; current < observation.localCount; ++current) {
        bool present = false;
        for (std::size_t prior = 0; prior < slice.localCount; ++prior) {
            if (slice.locals[prior] == observation.locals[current]) {
                present = true;
                break;
            }
            if (storage.groups[slice.locals[prior]].registryKey
                == storage.groups[observation.locals[current]].registryKey) {
                slice.overflowed = true;
                slice.keyConflict = true;
                present = true;
                break;
            }
        }
        if (present) {
            continue;
        }
        if (slice.localCount == slice.locals.size()) {
            slice.overflowed = true;
            continue;
        }
        slice.locals[slice.localCount++] = observation.locals[current];
    }
    slice.overflowed = slice.overflowed || observation.overflowed;
    slice.keyConflict = slice.keyConflict || observation.keyConflict;
}

/**
 * Walks one slice-set state to every placed object its registry names.
 * @param source Package directory and borrowed block keys.
 * @param scratch Lock-owned block storage.
 * @param storage Working storage for this pass.
 * @param walk Accumulator for one destination.
 * @param sliceSetIndex Slice-set index the entry reported.
 * @return True when the registry walked without running out of fixed storage.
 */
[[nodiscard]] bool walk_registry(const reader::Source& source,
                                 reader::Scratch& scratch,
                                 RosterStorage& storage,
                                 Walk& walk,
                                 std::uint32_t sliceSetIndex,
                                 std::uint8_t slice,
                                 std::uint32_t scenarioHash,
                                 std::uint16_t scenarioPackage,
                                 AuthoredObservation& authored) noexcept {
    for (std::size_t descriptor = 0; descriptor < kRegistryDescriptors.size(); ++descriptor) {
        tables::Array objects{};
        if (!tables::registry_objects(
                storage.registry, kRegistryDescriptors[descriptor], objects)) {
            // A registry declaring only some of its three arrays is ordinary.
            continue;
        }
        for (std::uint64_t index = 0; index < objects.count; ++index) {
            std::uint32_t objectTag = 0;
            if (!tables::registry_object_at(storage.registry, objects, index, objectTag)) {
                return false;
            }
            const ResolveContext context{scenarioHash,
                                         scenarioPackage,
                                         slice,
                                         static_cast<std::uint8_t>(descriptor)};
            ResolvedObject resolved{};
            if (!resolve_object(source, scratch, storage, objectTag, context, resolved)) {
                return false;
            }
            note_authored(storage, authored, resolved);
            if (resolved.group == kNotARosterGroup || !resolved.ordinary) {
                continue;
            }
            note_candidate(walk, storage, resolved.group, descriptor == 0);
            (void)tables::observe_roster_key(walk.intersection,
                                             sliceSetIndex,
                                             storage.groups[resolved.group].registryKey);
        }
    }
    return true;
}

/**
 * Walks one destination's scenario to every slice-set state it declares.
 * @param source Package directory and borrowed block keys.
 * @param scratch Lock-owned block storage.
 * @param storage Working storage for this pass.
 * @param walk Accumulator for this destination.
 * @return True when the scenario read and fixed storage held.
 */
[[nodiscard]] bool walk_destination(const reader::Source& source,
                                    reader::Scratch& scratch,
                                    RosterStorage& storage,
                                    Walk& walk,
                                    std::uint32_t scenarioTag,
                                    std::uint32_t scenarioHash) noexcept {
    tables::Array bubbles{};
    if (!tables::scenario_bubbles(storage.scenario, bubbles)) {
        return false;
    }
    for (std::uint64_t bubbleIndex = 0; bubbleIndex < bubbles.count; ++bubbleIndex) {
        tables::Bubble bubble{};
        if (!tables::bubble_at(storage.scenario, bubbles, bubbleIndex, bubble)) {
            return false;
        }
        for (std::uint64_t stateIndex = 0; stateIndex < bubble.stateCount; ++stateIndex) {
            tables::SliceState state{};
            if (!tables::slice_state_at(storage.scenario, bubble, stateIndex, state)) {
                return false;
            }
            tables::SliceEntry entry{};
            ++storage.reads;
            if (!reader::read_tag(source, scratch, state.entryTag, storage.entry)
                || !tables::slice_entry(storage.entry, entry)) {
                // The destination still transitions into that slice set, and no key can be proved
                // present in it, so nothing of this destination stays safe.
                tables::observe_unresolved_slice_set(walk.intersection);
                walk.authoredUnresolved = true;
                continue;
            }
            if (entry.index >= tables::kSliceSetCapacity) {
                (void)tables::observe_slice_set(
                    walk.intersection,
                    static_cast<std::uint32_t>(tables::kSliceSetCapacity
                                               * tables::kSliceSetIndexFactor));
                walk.authoredUnresolved = true;
                return true;
            }
            const std::uint32_t sliceSetIndex = entry.index * tables::kSliceSetIndexFactor;
            if (!tables::observe_slice_set(walk.intersection, sliceSetIndex)) {
                return true;
            }
            const auto slice = static_cast<std::uint8_t>(entry.index);
            AuthoredObservation authored{};
            ++storage.reads;
            if (!reader::read_tag(source, scratch, entry.registryTag, storage.registry)) {
                tables::observe_unresolved_slice_set(walk.intersection);
                merge_authored(storage, walk.authored[slice], authored);
                continue;
            }
            if (!walk_registry(source,
                               scratch,
                               storage,
                               walk,
                               sliceSetIndex,
                               slice,
                               scenarioHash,
                               tables::package_of(scenarioTag),
                               authored)) {
                return false;
            }
            merge_authored(storage, walk.authored[slice], authored);
        }
    }
    return true;
}

} // namespace

/**
 * Builds the roster half of the domain for every destination row.
 * @param source Package directory and borrowed block keys.
 * @param scratch Lock-owned block storage.
 * @param storage Working storage for this pass.
 * @param rows Destination rows whose tag is already set, updated in place.
 * @return True when every row was walked.
 */
bool build_rosters(const reader::Source& source,
                   reader::Scratch& scratch,
                   RosterStorage& storage,
                   std::span<layouts::Definition> rows) noexcept {
    storage.reads = 0;
    while (storage.cursor < rows.size() && storage.reads < kRosterReadBudget) {
        // The walk is thousands of tag reads, so it reports progress rather than going quiet.
        if (storage.cursor % kRosterProgressInterval == 0) {
            report_progress(storage.cursor, rows.size(), storage.groupCount);
        }
        layouts::Definition& row = rows[storage.cursor];
        ++storage.cursor;
        row.rosterGroupCount = 0;
        row.rosterGroups = {};
        row.bubbleGroupCount = 0;
        row.bubbleGroups = {};
        row.bubbleGroupMasks = {};
        row.authoredGroupCounts = {};
        row.authoredGroups = {};
        ++storage.reads;
        if (!reader::read_tag(source, scratch, row.tag, storage.scenario)) {
            continue;
        }
        std::uint32_t scenarioHash = 0;
        (void)tables::scenario_name_hash(storage.scenario, scenarioHash);
        Walk walk{};
        if (!walk_destination(source, scratch, storage, walk, row.tag, scenarioHash)) {
            continue;
        }
        publish_groups(walk, storage, row);
    }
    return storage.cursor >= rows.size();
}

} // namespace sunrise::client::content::scenarios
