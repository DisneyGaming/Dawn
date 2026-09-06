#include <Windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <string_view>

#include "../../../../../core/logging/log.h"
#include "../../../../../core/settings/settings.h"
#include "../../../../../middleware/bap/activity_message/tower_watch_cue_manifest.h"
#include "../../../../../middleware/content/packages/tables/scenario_reader.h"
#include "../../../../../state/account/account_state.h"
#include "../../../../../state/activity/defaults/activity_defaults_snapshot.h"
#include "../../../../../state/activity/bubble_authority/runtime.h"
#include "../../../../../state/activity/destination/activity_destination_snapshot.h"
#include "../../../../../state/activity/destination/activity_destination_spawn_binding.h"
#include "../../../../../state/activity/forced/activity_forced_destination.h"
#include "../../../../../state/activity/membership/activity_membership_query.h"
#include "../../../../../state/activity/runtime.h"
#include "../../../../../state/activity/omega_presentation.h"
#include "../../../../../state/activity/coo/omega_projection.h"
#include "../../../../../state/activity/coo/omega_opening_projection.h"
#include "../../../../../state/activity/omega_first_lair_runtime.h"
#include "../../../../../state/activity/omega_ending.h"
#include "../../../../../state/build_data/runtime.h"
#include "../../../../../state/runtime/runtime.h"
#include "activity_arrival.h"
#include "activity_region_snapshot.h"
#include "internal.h"
#include "omega_lair_roster.h"
#include "../../../../../middleware/bap/activity_message/tower_watch_cue_manifest.h"

namespace sunrise::server::bap::encrypted::push::activity {
namespace {

namespace layouts = state::build_data::scenarios;
namespace membership_message = middleware::bap::activity_message::replicate_membership;
namespace tower_watch = middleware::bap::activity_message::tower_watch;

/**
 * The type-17 lifetime state the roster reports.
 * Only 3, 6 and 10 are safe: spawn gate G4 indexes a jump table with no bounds check, so any other
 * value is a wild jump, not a refusal. 3 is what a live activity measured.
 */
constexpr std::uint8_t kLifetimeState = 3;
/** Sends whose state byte moves regardless. A body absorbed while the world loads needs them. */
constexpr std::uint8_t kWarmupSends = 3;
/** The state byte is stored biased into one signed byte, so the sequence stays inside this. */
constexpr std::uint8_t kStateSequenceWrap = 128;
/** Standard 32-bit FNV-1a basis and prime fold the group set into one comparable value. */
constexpr std::uint32_t kFoldBasis = 2166136261U;
constexpr std::uint32_t kFoldPrime = 16777619U;
/** Only a type-13 slot binds the player, so only a group holding one may carry the key. */
constexpr std::uint8_t kSlotTypeParticipation = 13;
constexpr std::uint8_t kSlotTypeMissionDirector = 35;
/** The join request names its character in the low half of the SOID, so compare on that half. */
constexpr std::uint64_t kIdentityLowMask = 0xFFFFFFFFULL;
constexpr std::uint16_t kNoRosterGroup = 0xFFFFU;

std::atomic_uint64_t g_lastTowerfallLayoutTrace{UINT64_MAX};
std::atomic_uint64_t g_lastTowerfallBuilderTrace{UINT64_MAX};

static_assert(message::kGroupCapacity == layouts::kDestinationWireGroupCapacity);

/**
 * Finds the full authored SOID for the character the join request named.
 * The client sends a short identity form. Publishing that form binds no object, so the full SOID
 * goes out instead.
 * @param joinCharacter Character id the join request carried, or zero when it carried none.
 * @return Authored SOID of the named character, or of the selected character when nothing matches.
 */
[[nodiscard]] std::uint64_t roster_player_key(std::uint64_t joinCharacter) noexcept {
    const state::AccountState account = state::account_snapshot();
    const std::uint64_t selected = state::account::selected_character_soid(account);
    if (joinCharacter == 0) {
        return selected;
    }
    for (std::size_t index = 0; index < account.characterCount; ++index) {
        const std::uint64_t soid = account.characters[index].soid;
        if ((soid & kIdentityLowMask) == (joinCharacter & kIdentityLowMask)) {
            return soid;
        }
    }
    return selected;
}

[[nodiscard]] bool
load_group(std::uint16_t tableIndex, Scratch& scratch, std::size_t slot) noexcept {
    return slot < scratch.rosterGroups.size()
           && state::build_data::find_roster_group(tableIndex, scratch.rosterGroups[slot]);
}

void expose_group(const layouts::RosterGroup& group, message::Group& output) noexcept {
    output.key = group.registryKey;
    output.slotTypes = std::span<const std::uint8_t>(group.slotTypes.data(), group.slotCount);
    output.slotFlags = std::span<const std::uint8_t>(group.slotFlags.data(), group.slotCount);
    output.slotIndices = std::span<const std::uint16_t>(group.slotIndices.data(), group.slotCount);
}

// The state-121 shells share these existing global keys. Only the native
// bookend has a new network descriptor; its type61 companion has none.
bool terminal_roster(Scratch& scratch,message::Roster& roster) noexcept {
    constexpr std::array<std::uint32_t,4> globals{0x4786C0E0,0x82FB58B7,0x29D7B029,0x96E0A5E5};
    std::size_t count{};bool root=false;
    for(std::size_t i=0;i<roster.groupCount;++i) {
        const auto group=roster.groups[i];
        if(std::find(globals.begin(),globals.end(),group.key)==globals.end()) continue;
        root|=group.key==0x4786C0E0;roster.groups[count++]=group;
    }
    if(!root || count>=roster.groups.size()) return false;
    static constexpr std::array<std::uint8_t,1> types{6},flags{2};
    static constexpr std::array<std::uint16_t,1> indices{0};
    roster.playerKeyGroup=0;
    for(std::size_t i=0;i<count && !roster.playerKeyGroup;++i)
        if(std::find(roster.groups[i].slotTypes.begin(),roster.groups[i].slotTypes.end(),std::uint8_t{13})!=roster.groups[i].slotTypes.end())
            roster.playerKeyGroup=roster.groups[i].key;
    if(!roster.playerKeyGroup) return false;
    roster.topLevelGroupCount=count;
    roster.groups[count++]={0x3A6CE17A,types,flags,indices};
    roster.groupCount=count;
    scratch.rosterSubBlockKeys[0][0]=0x3A6CE17A;
    scratch.rosterSubBlocks[0]={15,std::span(scratch.rosterSubBlockKeys[0]).first(1)};
    roster.bubbleSubBlocks=std::span(scratch.rosterSubBlocks).first(1);
    return true;
}

enum class OrdinaryCoverage : std::uint8_t { absent, active, inactiveBubble };

[[nodiscard]] OrdinaryCoverage ordinary_coverage(const layouts::Definition& layout,
                                                  std::uint16_t tableIndex,
                                                  std::size_t selectedSlice,
                                                  std::size_t& scratchSlot) noexcept {
    scratchSlot = 0;
    for (std::size_t index = 0; index < layout.rosterGroupCount; ++index) {
        if (layout.rosterGroups[index] == tableIndex) {
            scratchSlot = index;
            return OrdinaryCoverage::active;
        }
    }
    for (std::size_t index = 0; index < layout.bubbleGroupCount; ++index) {
        if (layout.bubbleGroups[index] == tableIndex) {
            scratchSlot = std::size_t{layout.rosterGroupCount} + index;
            return (layout.bubbleGroupMasks[index] & (std::uint64_t{1} << selectedSlice)) != 0
                       ? OrdinaryCoverage::active
                       : OrdinaryCoverage::inactiveBubble;
        }
    }
    return OrdinaryCoverage::absent;
}

[[nodiscard]] std::span<const message::BubbleSubBlock> fill_sub_blocks(
    const layouts::Definition& layout,
    Scratch& scratch,
    const message::Roster& roster,
    std::size_t ordinaryBubbleStart,
    std::size_t selectedSlice,
    std::span<const std::uint32_t> authoredLocalKeys) noexcept {
    std::size_t published = 0;
    for (std::size_t bubble = 0; bubble < scratch.rosterSubBlocks.size(); ++bubble) {
        std::size_t keyCount = 0;
        for (std::size_t index = 0; index < layout.bubbleGroupCount; ++index) {
            if ((layout.bubbleGroupMasks[index] & (std::uint64_t{1} << bubble)) == 0) {
                continue;
            }
            scratch.rosterSubBlockKeys[published][keyCount] =
                roster.groups[ordinaryBubbleStart + index].key;
            ++keyCount;
        }
        if (bubble == selectedSlice) {
            for (const std::uint32_t key : authoredLocalKeys) {
                if (keyCount == scratch.rosterSubBlockKeys[published].size()) {
                    return {};
                }
                scratch.rosterSubBlockKeys[published][keyCount++] = key;
            }
        }
        if (keyCount == 0) {
            continue;
        }
        scratch.rosterSubBlocks[published].bubble = static_cast<std::uint32_t>(bubble);
        scratch.rosterSubBlocks[published].keys = std::span<const std::uint32_t>(
            scratch.rosterSubBlockKeys[published].data(), keyCount);
        scratch.rosterSubBlocks[published].presence = {};
        ++published;
    }
    return std::span(scratch.rosterSubBlocks).first(published);
}

[[nodiscard]] bool fill_roster(const layouts::Definition& layout,
                               Scratch& scratch,
                               message::Roster& roster,
                               std::int32_t region) noexcept {
    roster = {};
    const std::size_t ordinaryCount =
        std::size_t{layout.rosterGroupCount} + std::size_t{layout.bubbleGroupCount};
    if (layout.rosterGroupCount == 0 || ordinaryCount > scratch.rosterGroups.size()
        || ordinaryCount > roster.groups.size()) {
        return false;
    }
    for (std::size_t index = 0; index < layout.rosterGroupCount; ++index) {
        if (!load_group(layout.rosterGroups[index], scratch, index)) {
            return false;
        }
    }
    for (std::size_t index = 0; index < layout.bubbleGroupCount; ++index) {
        if (!load_group(layout.bubbleGroups[index],
                        scratch,
                        std::size_t{layout.rosterGroupCount} + index)) {
            return false;
        }
    }
    for (std::size_t left = 0; left < ordinaryCount; ++left) {
        for (std::size_t right = left + 1; right < ordinaryCount; ++right) {
            if (scratch.rosterGroups[left].registryKey
                == scratch.rosterGroups[right].registryKey) {
                return false;
            }
        }
    }

    namespace tables = middleware::content::packages::tables;
    std::size_t selectedSlice = layouts::kBubbleCapacity;
    if (region >= 0 && region % static_cast<std::int32_t>(tables::kSliceSetIndexFactor) == 0) {
        const std::size_t ordinal =
            static_cast<std::size_t>(region) / tables::kSliceSetIndexFactor;
        if (ordinal < layouts::kBubbleCapacity) {
            selectedSlice = ordinal;
        }
    }
    std::uint16_t authoredRoot = kNoRosterGroup;
    std::array<std::uint16_t, layouts::kDestinationAuthoredGroupCapacity> authoredLocals{};
    std::size_t authoredLocalCount = 0;
    std::array<std::uint32_t, layouts::kDestinationAuthoredGroupCapacity> inactiveAuthoredKeys{};
    std::size_t inactiveAuthoredCount = 0;
    if (selectedSlice < layouts::kBubbleCapacity) {
        const std::size_t authoredCount = layout.authoredGroupCounts[selectedSlice];
        if (authoredCount != 0 && authoredCount <= layouts::kDestinationAuthoredGroupCapacity) {
            const std::uint16_t root = layout.authoredGroups[selectedSlice][0];
            std::size_t ordinarySlot = 0;
            const OrdinaryCoverage rootCoverage =
                ordinary_coverage(layout, root, selectedSlice, ordinarySlot);
            if (rootCoverage == OrdinaryCoverage::absent) {
                authoredRoot = root;
            } else if (rootCoverage == OrdinaryCoverage::inactiveBubble) {
                inactiveAuthoredKeys[inactiveAuthoredCount++] =
                    scratch.rosterGroups[ordinarySlot].registryKey;
            }
            for (std::size_t index = 1; index < authoredCount; ++index) {
                const std::uint16_t local = layout.authoredGroups[selectedSlice][index];
                const OrdinaryCoverage localCoverage =
                    ordinary_coverage(layout, local, selectedSlice, ordinarySlot);
                if (localCoverage == OrdinaryCoverage::absent) {
                    authoredLocals[authoredLocalCount++] = local;
                } else if (localCoverage == OrdinaryCoverage::inactiveBubble) {
                    inactiveAuthoredKeys[inactiveAuthoredCount++] =
                        scratch.rosterGroups[ordinarySlot].registryKey;
                }
            }
        }
    }
    const std::size_t authoredAdditionalCount =
        (authoredRoot != kNoRosterGroup ? 1U : 0U) + authoredLocalCount;
    bool publishAuthored = (authoredAdditionalCount != 0 || inactiveAuthoredCount != 0)
                           && ordinaryCount + authoredAdditionalCount
                                  <= layouts::kDestinationWireGroupCapacity;
    std::size_t loadedAuthored = 0;
    if (publishAuthored && authoredRoot != kNoRosterGroup) {
        publishAuthored = load_group(authoredRoot, scratch, ordinaryCount + loadedAuthored);
        loadedAuthored += publishAuthored ? 1U : 0U;
    }
    for (std::size_t index = 0; publishAuthored && index < authoredLocalCount; ++index) {
        publishAuthored =
            load_group(authoredLocals[index], scratch, ordinaryCount + loadedAuthored);
        loadedAuthored += publishAuthored ? 1U : 0U;
    }
    for (std::size_t authored = 0; publishAuthored && authored < loadedAuthored; ++authored) {
        const layouts::RosterGroup& candidate = scratch.rosterGroups[ordinaryCount + authored];
        for (std::size_t ordinary = 0; ordinary < ordinaryCount; ++ordinary) {
            if (candidate.registryKey == scratch.rosterGroups[ordinary].registryKey) {
                publishAuthored = false;
                break;
            }
        }
        for (std::size_t earlier = 0; publishAuthored && earlier < authored; ++earlier) {
            if (candidate.registryKey
                == scratch.rosterGroups[ordinaryCount + earlier].registryKey) {
                publishAuthored = false;
                break;
            }
        }
    }
    if (!publishAuthored) {
        authoredRoot = kNoRosterGroup;
        authoredLocalCount = 0;
        inactiveAuthoredCount = 0;
        loadedAuthored = 0;
    }

    std::size_t output = 0;
    for (std::size_t index = 0; index < layout.rosterGroupCount; ++index) {
        expose_group(scratch.rosterGroups[index], roster.groups[output++]);
    }
    const bool addsRoot = authoredRoot != kNoRosterGroup;
    if (addsRoot) {
        expose_group(scratch.rosterGroups[ordinaryCount], roster.groups[output++]);
    }
    const std::size_t ordinaryBubbleStart = output;
    for (std::size_t index = 0; index < layout.bubbleGroupCount; ++index) {
        expose_group(scratch.rosterGroups[layout.rosterGroupCount + index],
                     roster.groups[output++]);
    }
    std::array<std::uint32_t, layouts::kDestinationAuthoredGroupCapacity> authoredLocalKeys{};
    std::copy_n(inactiveAuthoredKeys.begin(), inactiveAuthoredCount, authoredLocalKeys.begin());
    for (std::size_t index = 0; index < authoredLocalCount; ++index) {
        const std::size_t scratchIndex = ordinaryCount + (addsRoot ? 1U : 0U) + index;
        expose_group(scratch.rosterGroups[scratchIndex], roster.groups[output]);
        authoredLocalKeys[inactiveAuthoredCount + index] = roster.groups[output].key;
        ++output;
    }
    roster.topLevelGroupCount = std::size_t{layout.rosterGroupCount} + (addsRoot ? 1U : 0U);
    roster.groupCount = output;
    roster.bubbleSubBlocks = fill_sub_blocks(
        layout,
        scratch,
        roster,
        ordinaryBubbleStart,
        selectedSlice,
        std::span(authoredLocalKeys).first(inactiveAuthoredCount + authoredLocalCount));
    for (std::size_t index = 0; index < roster.topLevelGroupCount && roster.playerKeyGroup == 0;
         ++index) {
        const message::Group& group = roster.groups[index];
        for (const std::uint8_t slotType : group.slotTypes) {
            if (slotType == kSlotTypeParticipation) {
                roster.playerKeyGroup = group.key;
                break;
            }
        }
    }
    return roster.playerKeyGroup != 0;
}

/** @param roster Published groups. @return One value that changes when the group set changes. */
[[nodiscard]] std::uint32_t fold_groups(const message::Roster& roster) noexcept {
    std::uint32_t folded = kFoldBasis;
    for (std::size_t index = 0; index < roster.groupCount; ++index) {
        folded = (folded ^ roster.groups[index].key) * kFoldPrime;
    }
    return folded;
}

/** @return True when one published group owns the mission-director slot. */
[[nodiscard]] bool carries_mission_director(const message::Roster& roster) noexcept {
    for (std::size_t group = 0; group < roster.groupCount; ++group) {
        for (const std::uint8_t slotType : roster.groups[group].slotTypes) {
            if (slotType == kSlotTypeMissionDirector) {
                return true;
            }
        }
    }
    return false;
}

/**
 * Picks the per-entry state byte and advances the connection's counters.
 * A burst send leaves the byte and the latched group set alone past the warm-up, so a group change
 * during a load is published by the next keepalive send instead.
 * @param session Connection-owned roster counters.
 * @param folded Current group set.
 * @param burst True for a send on the loading cadence.
 * @return The state byte to send.
 */
[[nodiscard]] std::uint8_t
next_state_sequence(Session& session, std::uint32_t folded, bool burst) noexcept {
    if (session.activity.rosterSends < kWarmupSends
        || (!burst && session.activity.rosterGroups != folded)) {
        session.activity.rosterState =
            static_cast<std::uint8_t>((session.activity.rosterState + 1) % kStateSequenceWrap);
        session.activity.rosterGroups = folded;
    }
    if (session.activity.rosterSends < kWarmupSends) {
        ++session.activity.rosterSends;
    }
    return session.activity.rosterState;
}

} // namespace

/** Purely resolves one copied destination/source pair. */
EffectiveRegion resolve_region(
    const state::activity::defaults::DefaultDestination& defaults,
    const state::activity::destination::DestinationSelection& selection,
    std::int32_t reportedRegion,
    bool allowArrival,
    std::string_view name,
    const layouts::Definition& layout) noexcept {
    EffectiveRegion region{};
    region.arrival = arrival_slice_set(defaults, selection, name, layout);
    region.reported = reportedRegion >= 0;
    if (region.reported) {
        region.index = reportedRegion;
        region.valid = true;
    } else if (allowArrival) {
        region.index = static_cast<std::int32_t>(region.arrival);
        region.valid = true;
    }
    return region;
}

/** Compatibility entry point: copy exact self inputs once, then use the pure builder. */
RosterOutcome build_roster_snapshot(Session& session,
                                    Scratch& scratch,
                                    message::Snapshot& snapshot,
                                    std::span<char> destination,
                                    std::size_t& destinationLength,
    bool burst) noexcept {
    state::activity::membership::RegionSnapshotInputs copied{};
    if (!region_lineage_is_current_locked(session, session.activity.lineage)
        || !state::activity::membership::snapshot_region_inputs(session.activity.lineage.bound,
                                                              session.activity.lineage.source,
                                                              {},
                                                              copied)) {
        return RosterOutcome::noLayout;
    }
    const std::string_view name(
        reinterpret_cast<const char*>(copied.destination.packageName.data()),
        copied.destination.packageNameLength);
    layouts::Definition layout{};
    if (!state::build_data::find_scenario_layout(name, layout)) {
        return RosterOutcome::noLayout;
    }
    const EffectiveRegion region = resolve_region(copied.defaults.defaultDestination,
                                                  copied.destination,
                                                  copied.sourceMembership.region.index,
                                                  session.activity.lineage.kind
                                                          == RegionLineageKind::ownedActivity
                                                      && session.activity.lineage.bound
                                                             == session.activity.lineage.source,
                                                  name,
                                                  layout);
    if (!region.valid) {
        return RosterOutcome::noGroups;
    }
    const RosterSnapshotInputs inputs{
        copied.destination,
        copied.defaults,
        copied.sourceMembership,
        copied.grantBefore,
        region.index,
        region.arrival,
    };
    return build_roster_snapshot(
        session, scratch, inputs, snapshot, destination, destinationLength, burst);
}

/** Builds from one immutable copied region plan. */
RosterOutcome build_roster_snapshot(Session& session,
                                    Scratch& scratch,
                                    const RosterSnapshotInputs& inputs,
                                    message::Snapshot& snapshot,
                                    std::span<char> destination,
                                    std::size_t& destinationLength,
                                    bool burst) noexcept {
    snapshot = {};
    destinationLength = 0;
    const auto& defaults = inputs.defaults;
    const auto& selection = inputs.destination;
    layouts::Definition layout{};
    const std::string_view name(reinterpret_cast<const char*>(selection.packageName.data()),
                                selection.packageNameLength);
    const bool omegaDestination = name == "mission_scot";
    const auto& omegaExperiments = core::settings::get().omegaExperiments;
    const bool syntheticOmega = omegaDestination;
    // Forest-D's native encounter classifier requires the selected race global
    // before loading its per-piece populations. Keep it stable across portal
    // quiescence; this does not request individual actors or override gate state.
    snapshot.omegaForestVexEncounters = syntheticOmega;
    if (!syntheticOmega && !session.activity.joinedForeignSession) {
        state::activity::omega_presentation::reset();
    }
    // Once the type-7 forest transition is issued, every Omega body goes silent so nothing
    // applies into components the slice teardown is freeing (index-heap double-free).
    const bool omegaQuiesced = state::activity::omega_authority_quiesced();
    snapshot.archiveOmega = name == "mission_scot";
    snapshot.omegaSceneAuthority = syntheticOmega && !omegaQuiesced;
    const bool cooOpening = syntheticOmega && !session.activity.joinedForeignSession
        && state::activity::coo::omega::select(state::activity::mission_run_generation(),
            omegaExperiments.cooExecutor);
    // Reconstructed Omega policy: the authenticated pm_weapondown monitor (30/20,
    // backed by authored tv_weapondown 60/29) advances Ikora's waiting orb animation.
    // This is distinct from pt_start_ikora_vignette 31/18 -> 60/28; the original
    // retail host join to C7ECAA77 is not recovered. Keep the accepted approach
    // latch for the whole run so backtracking cannot remove/reinsert the event.
    // A direct Forest/Lair launch has no accepted opening edge and leaves it absent.
    snapshot.omegaIkoraPortalRequested = snapshot.omegaSceneAuthority
        && session.activity.sensorObservation.omegaOpeningTriggered;
    // A received native scene output stays latched even when authority publication is delayed.
    // Initialization is the exact gate's closed state; snapshot construction never resets it.
    const auto lattice = session.activity.sensorObservation.omegaIkoraLattice.plan(
        session.activity.key.generation.value, snapshot.omegaSceneAuthority);
    snapshot.omegaIkoraLatticeReleased = lattice.active && lattice.position.revision == 2;
    // This hash is initial player data, independent of the later portal carrier activation.
    // The encoder preserves native participation ownership; do not resend a neutral player
    // record when the lattice opens merely to supply the previously missing predicate input.
    snapshot.omegaPortalPlayerHash = syntheticOmega && !omegaQuiesced;
    snapshot.omegaPortalEntry = snapshot.omegaSceneAuthority && snapshot.omegaIkoraLatticeReleased;
    if (cooOpening) {
        state::activity::coo::omega::opening::project(session.activity.sensorObservation.omegaOpeningExecutor, snapshot);
    }
    // Arm the one-shot Ghost line only once the client is IN WORLD (the same latch that gates
    // the authored seed). Run 6 proved the hazard: the record dispatched at t=60.7 during the
    // load screen, its 10 s eligibility deadline expired exactly at the t=70.7 fade-in, and
    // the consumed generation is never retried. Roster-ack is NOT an in-world signal (it
    // arrived 10 s early that run); mission_seed_armed() is. The scan re-runs on every apply,
    // so arming after component start is safe.
    snapshot.omegaDialogueArm =
        syntheticOmega && !omegaQuiesced && state::activity::mission_seed_armed();
    // The gate-hop moment: the entrance sense latched, the player stands in the shared tunnel.
    snapshot.omegaTunnelDialogue =
        snapshot.omegaDialogueArm
        && session.activity.sensorObservation.omegaForestEntranceTriggered;
    // The banner switches to the forest objective at the gate, like the tunnel Ghost line: a
    // walked z-leg reports region 88 client-side but the server membership never commits it
    // (advertised region stays 120), so region-gating never fired. Use the persistent entrance
    // sense instead, plus the region for a direct forest launch that skips the gate.
    snapshot.omegaForestBanner =
        snapshot.omegaDialogueArm
        && (session.activity.sensorObservation.omegaForestEntranceTriggered
            || (inputs.regionIndex >= 64 && inputs.regionIndex <= 112));
    if (snapshot.omegaDialogueArm && !session.activity.joinedForeignSession) {
        const auto mission = state::activity::coo::omega::update({
            state::activity::mission_run_generation(), GetTickCount64(), inputs.regionIndex,
            session.activity.sensorObservation.omegaForestEntranceTriggered, omegaExperiments.cooExecutor});
        state::activity::coo::omega::project(mission, snapshot);
    }
    // Activate the map generator once the player has ever entered the gate (persistent sense
    // latch), or by region for a direct forest launch. Region alone flaps during walked z-legs
    // (same failure the banner hit), and an unarmed window while the component constructs means
    // the body and the component never overlap; a record for a not-yet-created component is
    // skipped harmlessly, so arming wide is safe.
    snapshot.omegaForestGenerator =
        syntheticOmega
        && (session.activity.sensorObservation.omegaForestEntranceTriggered
            || (inputs.regionIndex >= 64 && inputs.regionIndex <= 104));
    snapshot.omegaGateAuthority = syntheticOmega && omegaExperiments.gateAuthority;
    snapshot.omegaPortalMutation = syntheticOmega && omegaExperiments.portalMutation;
    // TEMP diagnostic: which arm flag drops at the gate (dialogue/directive applies stop there).
    // Throttled to a change-only line so it cannot flood.
    {
        static std::uint32_t s_lastArmState = 0xFFFFFFFFu;
        const std::uint32_t armState =
            (snapshot.omegaDialogueArm ? 1u : 0u)
            | (snapshot.omegaSceneAuthority ? 2u : 0u)
            | (state::activity::mission_seed_armed() ? 4u : 0u)
            | (omegaQuiesced ? 8u : 0u)
            | (session.activity.sensorObservation.omegaForestEntranceTriggered ? 16u : 0u)
            | (snapshot.omegaTunnelDialogue ? 32u : 0u)
            | (snapshot.omegaForestBanner ? 64u : 0u)
            | (static_cast<std::uint32_t>(inputs.regionIndex & 0x3FF) << 16);
        if (s_lastArmState != armState) {
            s_lastArmState = armState;
            std::array<char, 200> line{};
            const int written = std::snprintf(
                line.data(), line.size(),
                "ev=omega_arm dialogue=%u scene=%u seed=%u quiesced=%u entrance=%u tunnel=%u "
                "banner=%u region=%d stage=%u",
                snapshot.omegaDialogueArm ? 1u : 0u, snapshot.omegaSceneAuthority ? 1u : 0u,
                state::activity::mission_seed_armed() ? 1u : 0u, omegaQuiesced ? 1u : 0u,
                session.activity.sensorObservation.omegaForestEntranceTriggered ? 1u : 0u,
                snapshot.omegaTunnelDialogue ? 1u : 0u, snapshot.omegaForestBanner ? 1u : 0u,
                inputs.regionIndex, static_cast<unsigned>(session.activity.omegaOpeningStage));
            if (written > 0) {
                core::log::write(core::log::Channel::server, core::log::Level::info,
                                 {line.data(), static_cast<std::size_t>(written)});
            }
        }
    }
    destinationLength = (std::min)(name.size(), destination.size());
    std::copy_n(name.begin(), destinationLength, destination.begin());
    if (!state::build_data::find_scenario_layout(name, layout)) {
        return RosterOutcome::noLayout;
    }
    // Publish the map generator group for the WHOLE activity, not per-region: a group set that
    // changes at the tunnel crossing advances the roster state sequence, which destroys and
    // recreates every authored object mid-run (killing the dialogue/directive components and the
    // objective banner). The readiness check accepts the extra acknowledgement entry, and the
    // bubble-11 mask keeps the generator's objects out of the lighthouse sub-block.
    if (name == "mission_scot") {
        state::build_data::amend_omega_forest_generator(layout);
    }
    // Free-roam scenarios overflow the fixed intersection capacities, so their extracted rows
    // hold zero groups; a zero-group row otherwise vetoes the whole periodic bundle and the
    // client starves into the activity-host timeout. Publish the global participation group
    // alone, which is part of every activity launch and safe in every slice set.
    if (layout.rosterGroupCount == 0) {
        const bool amended = state::build_data::amend_participation_fallback(layout);
        static std::atomic<std::uint32_t> s_lastFallback{0xFFFFFFFFU};
        const std::uint32_t observed = (layout.tag << 1) | (amended ? 1U : 0U);
        if (s_lastFallback.exchange(observed) != observed) {
            std::array<char, 160> line{};
            const int written = std::snprintf(
                line.data(), line.size(),
                "ev=activity stage=roster_fallback dest=%.*s tag=0x%X amended=%u",
                static_cast<int>(destinationLength), destination.data(), layout.tag,
                amended ? 1U : 0U);
            if (written > 0 && static_cast<std::size_t>(written) < line.size()) {
                core::log::write(core::log::Channel::server,
                                 amended ? core::log::Level::info : core::log::Level::warn,
                                 {line.data(), static_cast<std::size_t>(written)});
            }
        }
    }
    if (inputs.regionIndex < 0
        || !fill_roster(layout, scratch, snapshot.roster, inputs.regionIndex)) {
        return RosterOutcome::noGroups;
    }
    if (name == "mission_scot") {
        const auto admission = omega_lair::admit(layout, scratch, snapshot.roster,
            [](std::size_t index, layouts::RosterGroup& group) noexcept {
                return state::build_data::find_roster_group(index, group);
            });
        static std::atomic_int lastAdmission{-1};
        if (lastAdmission.exchange(static_cast<int>(admission)) != static_cast<int>(admission)) {
            const bool admitted = admission == omega_lair::Admission::added
                                  || admission == omega_lair::Admission::present;
            std::array<char, 160> line{};
            const int written = std::snprintf(line.data(), line.size(),
                "ev=omega_intro stage=roster admitted=%u result=%u registry=F4D0E0B2 bubble=14 groups=%zu",
                admitted ? 1U : 0U, static_cast<unsigned>(admission), snapshot.roster.groupCount);
            if (written > 0 && static_cast<std::size_t>(written) < line.size()) {
                core::log::write(core::log::Channel::server,
                    admitted ? core::log::Level::info : core::log::Level::warn,
                    {line.data(), static_cast<std::size_t>(written)});
            }
        }
    }
    if (name == "mission_scot") {
        const auto admission = omega_lair::admit(layout, scratch, snapshot.roster,
            [](std::size_t index, layouts::RosterGroup& group) noexcept {
                return state::build_data::find_roster_group(index, group);
            }, true);
        static std::atomic_int lastBossAdmission{-1};
        if (lastBossAdmission.exchange(static_cast<int>(admission)) != static_cast<int>(admission)) {
            std::array<char, 160> line{};
            const int written = std::snprintf(line.data(), line.size(),
                "ev=omega_boss stage=roster result=%u registry=95FB2E01 bubble=14 groups=%zu",
                static_cast<unsigned>(admission), snapshot.roster.groupCount);
            if (written > 0 && static_cast<std::size_t>(written) < line.size()) {
                core::log::write(core::log::Channel::server, core::log::Level::info,
                    {line.data(), static_cast<std::size_t>(written)});
            }
        }
    }
    if (name == "mission_scot") {
        const auto admission = omega_lair::admit_crown(layout, scratch, snapshot.roster,
            [](std::size_t index, layouts::RosterGroup& group) noexcept {
                return state::build_data::find_roster_group(index, group);
            });
        static std::atomic_int lastCrownAdmission{-1};
        if (lastCrownAdmission.exchange(static_cast<int>(admission)) != static_cast<int>(admission)) {
            std::array<char,160> line{};
            const int written = std::snprintf(line.data(),line.size(),
                "ev=omega_crown stage=roster result=%u registry=0040BF06 bubble=14 groups=%zu",
                static_cast<unsigned>(admission),snapshot.roster.groupCount);
            if (written>0 && static_cast<std::size_t>(written)<line.size()) {
                core::log::write(core::log::Channel::server,core::log::Level::info,
                    {line.data(),static_cast<std::size_t>(written)});
            }
        }
    }
    if(name=="mission_scot") {
        for(const auto& group:state::activity::omega_lair_full_roster::kCombatGroups) {
            const auto admission=omega_lair::admit_full(layout,scratch,snapshot.roster,
                [](std::size_t index,layouts::RosterGroup& row) noexcept {
                    return state::build_data::find_roster_group(index,row);
                },group);
            if(admission!=omega_lair::Admission::added && admission!=omega_lair::Admission::present) {
                return RosterOutcome::noGroups;
            }
        }
    }
    if(snapshot.omegaEndingRetire) {
        if(!omega_lair::terminal_roster(scratch,snapshot.roster,
            state::activity::omega_ending::kState)) {
            return RosterOutcome::noGroups;
        }
        snapshot.omegaPortalEntry=false;
        snapshot.omegaForestGenerator=false;
        snapshot.omegaOpeningStage=message::kOmegaOpeningStageNone;
        snapshot.preserveMissionAuthorityState=true;
        snapshot.omegaCrownRestriction=state::activity::omega_crown_respawn::Restriction::disable;
    }
    const state::activity::defaults::FallbackPolicy& fallback =
        defaults.defaultDestination.fallback;
    snapshot.patchEpoch = session.activityPatchEpoch;
    // The character the join named wins, resolved to its authored SOID. The client binds its
    // player by matching this value against the object registry, and the short form the join
    // carries matches nothing.
    snapshot.playerKey = roster_player_key(session.activity.characterSoid);
    // The old encoder documents this key as message 12's member record `+16` while its own code
    // sends the character SOID. That field is the membership identity, so this sends it instead.
    if (defaults.rosterKeyFromIdentity) {
        const std::uint64_t identity = inputs.sourceMembership.hasIdentity
                                           ? inputs.sourceMembership.identity.joinIdentity
                                           : 0;
        if (identity != 0) {
            snapshot.playerKey = identity;
        }
    }
    snapshot.lifetime = kLifetimeState;
    snapshot.keyOnEveryParticipationSlot = defaults.rosterKeyOnAllSlots;
    // The participation record's `+0` latches only when the region index is known.
    snapshot.region = static_cast<std::uint32_t>(inputs.regionIndex);
    snapshot.hasRegion = true;
    // The spawn override always names the destination's own arrival, never the player's position.
    snapshot.spawnSliceSet = inputs.destinationArrival;
    snapshot.spawnSetHash =
        state::activity::destination::attachable_spawn_set_hash(selection, fallback.spawnSetHash);
    snapshot.hasSpawnOverride =
        snapshot.spawnSetHash != 0 && snapshot.spawnSetHash != message::kAbsentSpawnSetHash;
    // The archived opening packages need one registration-only packet on the primary/private
    // activity before their object state arrives. Publishing phase 1 and phase 2 together there
    // makes the client authority table report type 18 as already present before the native
    // activity-script manager has constructed it, so the manager skips its own component
    // dispatch. The joined foreign/public activity has the opposite ordering requirement: its
    // simulation registry must be fully seeded before the client swaps it to PUBLIC CURRENT and
    // creates player_broadcast. A registration-only foreign packet leaves that manager's entity-id
    // pool empty until the post-swap update, after the one native creation attempt has failed.
    const bool openingDestination =
        state::activity::forced::mission_host_reestablishment_enabled();
    snapshot.phaseOneOnly = openingDestination && !session.activity.joinedForeignSession
                            && session.activity.rosterSends == 0;
    // Keep sending the complete initialization until the client's post-apply observer proves type
    // 18 was actually constructed. Only that acknowledgement can transfer ownership to the native
    // simulation: a server send can arrive before the authority manager is ready and be discarded.
    const bool authorityRuntimeInitialized =
        syntheticOmega && state::activity::mission_authority_runtime_initialized();
    snapshot.initializeMissionAuthorityRuntime =
        syntheticOmega && !authorityRuntimeInitialized;
    // Keep the existing delivery order for the opening runtime and its Scene/Ready commits.
    // Ikora's separate source authority now supplies one Scene-owned actor; each authored
    // wrapper binds that actor. The retained approach event above advances its orb prelude
    // independently of the still-unrecovered runtime handoff to state 2.
    // Do not spend Omega's opening clock behind the loading screen. In the measured retail intro,
    // the objective and Ghost preroll begin only after arrival. Publishing state 1 before the
    // client's in-world latch queued the Ikora Scene during transition, so both cast objects were
    // instantiated almost immediately when the world became available and the whole preroll was
    // skipped. WorldPhase::arrived is the exact activity:in_world observation; unlike the retained
    // mission_seed_armed latch it also pauses the opening during any later slice transition.
    const bool openingEligible =
        syntheticOmega && authorityRuntimeInitialized
        && session.activity.sensorObservation.omegaRosterReady
        && state::activity::world_phase() == state::activity::WorldPhase::arrived
        && state::activity::mission_seed_armed();
    bool forcedOmegaSeed = false;
    if (openingEligible) {
        const std::uint8_t previous = session.activity.omegaOpeningStage;
        if (previous == message::kOmegaOpeningStageNone
            && session.activity.sensorObservation.omegaOpeningTriggered) {
            // The player begins inside BA5F's authored Lighthouse/Ghost volume. Hold the Ikora
            // Scene at None until the later D001 approach edge arrives, preserving retail's
            // arrival -> Ghost VO -> approach -> vignette order.
            forcedOmegaSeed = true;
            snapshot.initializeMissionAuthorityRuntime = true;
            session.activity.omegaOpeningStage = message::kOmegaOpeningStageBaseline;
        } else if (previous == message::kOmegaOpeningStageBaseline
                   && snapshot.omegaSceneAuthority
                   && message::kOmegaSceneAuthorityBodyReady) {
            forcedOmegaSeed = true;
            snapshot.omegaOpeningStage = message::kOmegaOpeningStageScene;
            session.activity.omegaOpeningStage = message::kOmegaOpeningStageScene;
        } else if (previous == message::kOmegaOpeningStageScene) {
            forcedOmegaSeed = true;
            snapshot.omegaOpeningStage = message::kOmegaOpeningStageReady;
            session.activity.omegaOpeningStage = message::kOmegaOpeningStageReady;
        } else if (previous == message::kOmegaOpeningStageReady
                   && session.activity.sensorObservation.omegaSceneHandoffArmed
                   && !session.activity.sensorObservation.omegaOpeningAuthorityPublished) {
            // The approach edge started state 1; the exact type43/1 handoff is the next authored
            // boundary. Reusing the already-latched approach bit here advanced straight through
            // state 2 and skipped the Ikora vignette's natural dwell.
            snapshot.omegaOpeningStage = message::kOmegaOpeningStageTriggered;
            session.activity.omegaOpeningStage = message::kOmegaOpeningStageTriggered;
        } else if (previous == message::kOmegaOpeningStageTriggered
                   && session.activity.sensorObservation.omegaSceneCompleted) {
            // State 2 already armed the portal objects before the entrance edge could fire. Keep
            // them installed after cast retirement unless the client has now confirmed transport.
            snapshot.omegaOpeningStage =
                session.activity.sensorObservation.omegaPortalTransportConfirmed
                    ? message::kOmegaOpeningStageCompleted
                    : message::kOmegaOpeningStagePortal;
            session.activity.omegaOpeningStage = snapshot.omegaOpeningStage;
        } else if (previous == message::kOmegaOpeningStageSpawner) {
            // Compatibility with the retired direct-spawner experiment: never send type 1 again.
            snapshot.omegaOpeningStage = message::kOmegaOpeningStagePortal;
            session.activity.omegaOpeningStage = message::kOmegaOpeningStagePortal;
        } else if (previous == message::kOmegaOpeningStagePortal
                   && session.activity.sensorObservation.omegaSceneCompleted
                   && session.activity.sensorObservation.omegaForestEntranceTriggered
                   && !session.activity.sensorObservation.omegaForestEntranceAuthorityPublished) {
            // Type 30/index 24 is the authored entrance edge that requests the next mission state.
            // Publish state 4 now; type 22 or a region move can only confirm the transport after
            // the client has consumed the state that causes it.
            snapshot.omegaOpeningStage = message::kOmegaForestStageTransition;
            session.activity.omegaOpeningStage = message::kOmegaForestStageTransition;
        } else if (previous == message::kOmegaOpeningStagePortal
                   && session.activity.sensorObservation.omegaPortalTransportConfirmed) {
            // Compatibility for a client that reports transport before its entrance monitor edge.
            snapshot.omegaOpeningStage = message::kOmegaOpeningStageCompleted;
            session.activity.omegaOpeningStage = message::kOmegaOpeningStageCompleted;
        } else if (previous == message::kOmegaOpeningStagePortal) {
            snapshot.omegaOpeningStage = message::kOmegaOpeningStagePortal;
        } else if (previous == message::kOmegaOpeningStageCompleted) {
            snapshot.omegaOpeningStage = message::kOmegaOpeningStageSettled;
            session.activity.omegaOpeningStage = message::kOmegaOpeningStageSettled;
        } else if (previous == message::kOmegaOpeningStageSettled
                   && session.activity.sensorObservation.omegaForestEntranceTriggered
                   && !session.activity.sensorObservation.omegaForestEntranceAuthorityPublished) {
            snapshot.omegaOpeningStage = message::kOmegaForestStageTransition;
            session.activity.omegaOpeningStage = message::kOmegaForestStageTransition;
        } else if (previous == message::kOmegaForestStageTransition) {
            // State 4 is a persistent authored runtime state, not an edge pulse. Keep it installed
            // until a later observed mission event gives the host a proven successor.
            snapshot.omegaOpeningStage = message::kOmegaForestStageTransition;
        } else if (previous >= message::kOmegaForestStageSettled) {
            snapshot.omegaOpeningStage = message::kOmegaForestStageSettled;
        } else if (previous >= message::kOmegaOpeningStageSettled) {
            snapshot.omegaOpeningStage = message::kOmegaOpeningStageSettled;
        }
    }
    snapshot.publishOmegaOpeningTransition =
        snapshot.omegaOpeningStage == message::kOmegaOpeningStageTriggered
        || snapshot.omegaOpeningStage == message::kOmegaOpeningStageCompleted
        || snapshot.omegaOpeningStage == message::kOmegaForestStageTransition;
    const bool openingBaseline =
        openingEligible
        && session.activity.omegaOpeningStage == message::kOmegaOpeningStageBaseline;
    snapshot.preserveMissionAuthorityState =
        syntheticOmega && authorityRuntimeInitialized && !openingBaseline;
    snapshot.missionDirectorActive = openingBaseline || snapshot.publishOmegaOpeningTransition;
    snapshot.missionDirectorTransition =
        openingBaseline || snapshot.publishOmegaOpeningTransition;
    snapshot.activityScriptFlag = openingBaseline || snapshot.publishOmegaOpeningTransition;
    snapshot.activityScriptState =
        snapshot.omegaOpeningStage == message::kOmegaForestStageTransition
            ? 4
            : (snapshot.omegaOpeningStage == message::kOmegaOpeningStageCompleted
                   ? 3
                   : (snapshot.omegaOpeningStage == message::kOmegaOpeningStageTriggered
                           ? 2
                           : (openingBaseline ? 1 : 0)));
    // Tower Watch uses the same root-cue/runtime transport as Omega, but the progression itself
    // is a compact manifest. Publish exactly one undelivered beat at a time so startup edges that
    // arrive in one frame cannot skip the opening objective and Ghost line.
    const bool towerWatchEligible = name == tower_watch::kPackage
                                    && session.activity.sensorObservation.towerWatchRosterReady
                                    && state::activity::world_phase()
                                           == state::activity::WorldPhase::arrived
                                    && state::activity::mission_seed_armed();
    tower_watch::Stage nextTowerWatchStage = tower_watch::Stage::none;
    if (towerWatchEligible) {
        if (!session.activity.sensorObservation.towerWatchBreachSeen
            && state::activity::tower_watch_opening_dialogue_processed()
            && session.activity.sensorObservation.towerWatchPublishedStage
                   >= tower_watch::value(tower_watch::Stage::opening)) {
            session.activity.sensorObservation.towerWatchBreachSeen = true;
            std::array<char, 224> handoffLine{};
            const int handoffWritten = std::snprintf(
                handoffLine.data(),
                handoffLine.size(),
                "ev=tower_watch_executor stage=dialogue_handoff result=accepted "
                "record=0 published=%u next=breach_cabal_beat",
                static_cast<unsigned>(
                    session.activity.sensorObservation.towerWatchPublishedStage));
            if (handoffWritten > 0) {
                core::log::write(core::log::Channel::server,
                                 core::log::Level::info,
                                 {handoffLine.data(),
                                  (std::min)(static_cast<std::size_t>(handoffWritten),
                                             handoffLine.size() - 1U)});
            }
        }
        const std::uint8_t published =
            session.activity.sensorObservation.towerWatchPublishedStage;
        if (published < tower_watch::value(tower_watch::Stage::opening)) {
            nextTowerWatchStage = tower_watch::Stage::opening;
        } else if (published < tower_watch::value(tower_watch::Stage::breach)
                   && session.activity.sensorObservation.towerWatchBreachSeen) {
            nextTowerWatchStage = tower_watch::Stage::breach;
        } else if (published < tower_watch::value(tower_watch::Stage::pathUnlocked)
                   && session.activity.sensorObservation.towerWatchEncounterChanged) {
            nextTowerWatchStage = tower_watch::Stage::pathUnlocked;
        }
    }
    if (const tower_watch::Beat* const beat = tower_watch::beat(nextTowerWatchStage);
        beat != nullptr) {
        snapshot.phaseOneOnly = false;
        snapshot.publishAuthoredCueTransition = true;
        snapshot.authoredCueRegistry = tower_watch::kRootCueRegistry;
        snapshot.authoredDirectiveEvent = beat->directiveEvent;
        snapshot.authoredDialogueRecord = beat->dialogueRecord;
        snapshot.authoredCueStage = tower_watch::value(beat->stage);
        snapshot.publishAuthoredSceneSelector =
            tower_watch::kPublishBreachSceneAuthority
            && beat->stage == tower_watch::Stage::breach;
        if (snapshot.publishAuthoredSceneSelector) {
            snapshot.authoredSceneRegistry = tower_watch::kTowerWatchRegistry;
            snapshot.authoredSceneType = tower_watch::kBreachSceneType;
            snapshot.authoredSceneIndex = tower_watch::kBreachSceneIndex;
            snapshot.authoredSceneSelector = tower_watch::kBreachSceneSelector;
            snapshot.authoredSceneEntryRegistry = tower_watch::kBreachSceneEntryRegistry;
            snapshot.authoredSceneEntryType = tower_watch::kBreachSceneEntryType;
            snapshot.authoredSceneEntryIndex = tower_watch::kBreachSceneEntryIndex;
        }
        snapshot.initializeMissionAuthorityRuntime =
            beat->stage == tower_watch::Stage::opening;
        snapshot.preserveMissionAuthorityState = false;
        snapshot.missionDirectorActive = true;
        snapshot.missionDirectorTransition = true;
        snapshot.activityScriptFlag = true;
        snapshot.activityScriptState = beat->scriptState;

        std::array<char, 320> cueLine{};
        const int cueWritten = std::snprintf(
            cueLine.data(),
            cueLine.size(),
            "ev=tower_watch_executor stage=publish result=staged beat=%u name=%s "
            "root=0x%08X directive=0x%08X dialogue_record=%u script_state=%d "
            "scene=%u scene_blocked_unsafe=%u scene_slot=0x%08X/%u/%u "
            "selector=0x%08X scene_entry=0x%08X/%u/%u scene_candidate_bits=129",
            static_cast<unsigned>(snapshot.authoredCueStage),
            beat->name,
            snapshot.authoredCueRegistry,
            snapshot.authoredDirectiveEvent,
            static_cast<unsigned>(snapshot.authoredDialogueRecord),
            snapshot.activityScriptState,
            snapshot.publishAuthoredSceneSelector ? 1U : 0U,
            !tower_watch::kPublishBreachSceneAuthority
                    && beat->stage == tower_watch::Stage::breach
                ? 1U
                : 0U,
            snapshot.authoredSceneRegistry,
            static_cast<unsigned>(snapshot.authoredSceneType),
            static_cast<unsigned>(snapshot.authoredSceneIndex),
            snapshot.authoredSceneSelector,
            snapshot.authoredSceneEntryRegistry,
            static_cast<unsigned>(snapshot.authoredSceneEntryType),
            static_cast<unsigned>(snapshot.authoredSceneEntryIndex));
        if (cueWritten > 0) {
            core::log::write(core::log::Channel::server,
                             core::log::Level::info,
                             {cueLine.data(),
                              (std::min)(static_cast<std::size_t>(cueWritten),
                                         cueLine.size() - 1U)});
        }
    }
    if (name == tower_watch::kPackage
        && session.activity.sensorObservation.towerWatchPublishedStage != 0U
        && !snapshot.publishAuthoredCueTransition) {
        snapshot.preserveMissionAuthorityState = true;
    }
    const bool configuredAuthoredSeed = core::settings::get().client.seedAuthoredSensors
                                        && state::activity::mission_seed_armed();
    snapshot.seedAuthoredSensors = omegaDestination
                                       ? syntheticOmega
                                             && (forcedOmegaSeed || configuredAuthoredSeed)
                                       : configuredAuthoredSeed;
    // The state byte owns the roster object's lifetime, not its auth-body revision. Advancing it
    // for a slot-35 value change destroys slot 18 and slot 35 before the replacement auth bodies
    // can update them. Keep the object generation stable and let the changed auth body apply to
    // the existing runtime instead.
    if(snapshot.omegaEndingRetire) {
        snapshot.omegaOpeningStage=message::kOmegaOpeningStageNone;
        snapshot.publishOmegaOpeningTransition=false;
        snapshot.preserveMissionAuthorityState=true;
        snapshot.omegaPortalEntry=false;
        snapshot.omegaForestGenerator=false;
        snapshot.omegaCrownRestriction=state::activity::omega_crown_respawn::Restriction::disable;
        // Retain each prior bubble/key ordinal and publish explicit removals.
        // Native cleanup must acknowledge those removals before bookendState
        // permits the teleport. Keep the globals' generation stable throughout.
        session.activity.rosterGroups=fold_groups(snapshot.roster);
    }
    snapshot.stateSequence = next_state_sequence(session, fold_groups(snapshot.roster), burst);
    return RosterOutcome::published;
}

namespace {

/** Copies the delivery fields whose candidate mutation must remain off the live Session. */
[[nodiscard]] RosterDeliveryBefore delivery_of(const ActivityBindingState& binding) noexcept {
    return {
        binding.rosterGroups,
        binding.rosterSends,
        binding.rosterState,
        binding.omegaOpeningStage,
        binding.directorSends,
        binding.missionDirectorActive,
    };
}

/** Maps one copied membership after-image into the fixed wire schema. */
[[nodiscard]] bool make_membership_wire(
    state::activity::ActivityInstanceKey activity,bool validatedOmega,
    const state::activity::membership::MembershipState& membership,
    const gameplay::AdvertisementSnapshot& advertisement,
    membership_message::MembershipSnapshot& wire) noexcept {
    wire = {};
    if (!membership.hasIdentity || membership.revision == 0) {
        return false;
    }
    wire.identity.memberKey = membership.identity.memberKey;
    wire.identity.field1 = membership.identity.smallOpaque;
    wire.identity.field2 = membership.identity.signedOpaque;
    wire.identity.field3 = membership.identity.joinIdentity;
    wire.identity.accountSoid = membership.identity.accountSoid;
    wire.identity.field5 = membership.identity.opaqueSoid;
    wire.identity.field6 = membership.identity.secondaryOpaque;
    wire.spawn.state = membership.spawn.state;
    wire.spawn.opaqueByte = membership.spawn.opaqueByte;
    wire.spawn.opaqueValue = membership.spawn.opaqueValue;
    wire.teleport.state = membership.teleport.state;
    wire.teleport.token = membership.teleport.token;
    wire.teleport.sliceSetIndex = membership.teleport.sliceSetIndex;
    wire.teleport.sliceSetHash = membership.teleport.sliceSetHash;
    const state::activity::omega_ending_transit::Observation nativeTransit{
        {membership.teleport.state,membership.teleport.token,membership.teleport.sliceSetIndex,
            membership.teleport.sliceSetHash},membership.region.index,membership.hasTeleportReceipt,membership.region.index>=0};
    const auto terminal=state::activity::omega_ending::project_transit({activity,
        state::activity::mission_run_generation(),membership.identity.memberKey,validatedOmega,
        nativeTransit});
    if(terminal.publish) {
        wire.teleport={terminal.host.state,terminal.host.token,terminal.host.sliceSetIndex,
            terminal.host.sliceSetHash};
    }
    wire.revision = membership.revision;
    wire.epoch = state::activity::membership::kStableEpoch;
    wire.transitionToken = membership.hasTransitionToken
                               ? membership.transitionToken
                               : state::activity::membership::kInitialTransitionToken;
    wire.hasSynchronizationToken=membership.hasSynchronizationToken;
    wire.synchronizationToken=membership.synchronizationToken;
    if (advertisement.readiness == gameplay::AdvertisementReadiness::ready) {
        wire.citizen = advertisement.citizen;
        if(!membership_message::select_active_region(wire,wire.citizen.regionIndex)) { return false; }
    } else if(membership.region.index>=0
        && !membership_message::select_active_region(wire,membership.region.index)) {
        return false;
    }
    wire.hasHostSynchronizationToken=state::activity::omega_ending_transit::host_synchronization_ready(
        terminal,nativeTransit,membership.hasSynchronizationToken,membership.synchronizationToken,
        advertisement.readiness==gameplay::AdvertisementReadiness::ready && wire.citizen.present,
        wire.citizen.regionIndex);
    if(wire.hasHostSynchronizationToken) { wire.hostSynchronizationToken=terminal.host.token; }
    return true;
}

/** Completes roster/grant/delivery fields from copied State and a candidate Session value. */
[[nodiscard]] bool finalize_roster(const Session& session,
                                   Scratch& scratch,
                                   const RosterSnapshotInputs& inputs,
                                   bool rearmMissionDirector,
                                   RegionTransitionSnapshot& snapshot) noexcept {
    if (!requires_notification(snapshot.required, RegionNotification::roster)) {
        snapshot.before = delivery_of(session.activity);
        snapshot.after = snapshot.before;
        return true;
    }
    if (!session.activityPatchEpochSeen) {
        return false;
    }
    Session candidate = session;
    if (rearmMissionDirector
        && snapshot.advertisement.readiness == gameplay::AdvertisementReadiness::ready) {
        candidate.activity.missionDirectorActive = false;
    }
    std::array<char, state::activity::destination::kPackageNameCapacity> destination{};
    std::size_t destinationLength = 0;
    const RosterOutcome built = build_roster_snapshot(candidate,
                                                      scratch,
                                                      inputs,
                                                      snapshot.rosterWire,
                                                      destination,
                                                      destinationLength,
                                                      snapshot.burst);
    if (built != RosterOutcome::published) {
        // Bounded rejection diagnostic: a vetoed bundle sends the client nothing at all, so a
        // destination that can never publish would otherwise starve the load invisibly.
        static std::atomic<std::uint32_t> s_lastRejection{0};
        std::uint32_t observed = kFoldBasis;
        for (std::size_t index = 0; index < destinationLength; ++index) {
            observed = (observed ^ static_cast<std::uint8_t>(destination[index])) * kFoldPrime;
        }
        observed ^= static_cast<std::uint32_t>(built) + 1U;
        if (s_lastRejection.exchange(observed) != observed) {
            const char* reason = built == RosterOutcome::noEpoch     ? "no_epoch"
                                 : built == RosterOutcome::noLayout  ? "no_layout"
                                 : built == RosterOutcome::encodeFailed ? "encode"
                                                                        : "no_groups";
            std::array<char, 160> line{};
            const int written = std::snprintf(
                line.data(), line.size(), "ev=activity stage=roster_finalize result=%s dest=%.*s",
                reason, static_cast<int>(destinationLength), destination.data());
            if (written > 0 && static_cast<std::size_t>(written) < line.size()) {
                core::log::write(core::log::Channel::server, core::log::Level::warn,
                                 {line.data(), static_cast<std::size_t>(written)});
            }
        }
        return false;
    }
    snapshot.before = delivery_of(session.activity);
    snapshot.after = delivery_of(candidate.activity);
    if (!lifecycle::stage_roster_publication_generation(session.activity,
                                                        snapshot.rosterPublication)) {
        return false;
    }
    state::activity::bubble_authority::Grant grant{};
    if (state::activity::bubble_authority::select_grant(inputs.grantBefore,
                                                        snapshot.regionIndex,
                                                        grant)) {
        snapshot.grantCandidate = grant;
        snapshot.rosterWire.hasGrant = true;
        snapshot.rosterWire.grant.bubble = grant.bubble;
        snapshot.rosterWire.grant.token = grant.token;
    }
    return true;
}

/** Finalizes a copied semantic plan after readiness and exact lineage have been proved. */
[[nodiscard]] RegionSnapshotBuildResult finalize_snapshot(
    const Session& session,
    Scratch& scratch,
    const state::activity::membership::RegionSnapshotInputs& copied,
    const state::activity::membership::MembershipState& membershipAfter,
    state::activity::HostRegionKey expectedHost,
    state::activity::HostRegionKey nextHost,
    NotificationMask required,
    bool burst,
    bool publishesHud,
    bool rearmMissionDirector,
    RegionTransitionSnapshot& output,
    RegionPublicationDebt& debt,
    gameplay::group::HostActivityLineageLease& advertisementLease) noexcept {
    output = {};
    debt = {};
    const RegionLineage& lineage = session.activity.lineage;
    const std::string_view name(
        reinterpret_cast<const char*>(copied.destination.packageName.data()),
        copied.destination.packageNameLength);
    layouts::Definition layout{};
    const bool hasLayout = state::build_data::find_scenario_layout(name, layout);
    const bool allowArrival = lineage.kind == RegionLineageKind::ownedActivity
                              && lineage.bound == lineage.source;
    const EffectiveRegion region = resolve_region(copied.defaults.defaultDestination,
                                                  copied.destination,
                                                  membershipAfter.region.index,
                                                  allowArrival,
                                                  name,
                                                  layout);
    if (!region.valid
        || (requires_notification(required, RegionNotification::roster) && !hasLayout)) {
        return RegionSnapshotBuildResult::failed;
    }

    gameplay::AdvertisementSnapshot advertisement{};
    if (requires_notification(required, RegionNotification::membership)) {
        const gameplay::AdvertisementReadiness readiness =
            gameplay::request_advertisement_host(lineage.source, region.index);
        if (readiness == gameplay::AdvertisementReadiness::stale) {
            return RegionSnapshotBuildResult::stale;
        }
        if (readiness == gameplay::AdvertisementReadiness::pending) {
            debt.binding = session.activity.key;
            debt.activity = lineage.bound;
            debt.regionSource = lineage.source;
            debt.committedHostRegion = nextHost;
            debt.membershipAfter = membershipAfter;
            debt.destination = copied.destination;
            debt.regionIndex = region.index;
            debt.required = required;
            debt.present = true;
            debt.publishesHud = publishesHud;
            return RegionSnapshotBuildResult::pending;
        }
        advertisement.readiness = readiness;
        if (readiness == gameplay::AdvertisementReadiness::ready
            && !gameplay::acquire_advertisement_snapshot(lineage.source,
                                                         region.index,
                                                         advertisement,
                                                         advertisementLease)) {
            return RegionSnapshotBuildResult::stale;
        }
    }

    output.binding = session.activity.key;
    output.activity = lineage.bound;
    output.regionSource = lineage.source;
    output.expectedHostRegion = expectedHost;
    output.nextHostRegion = nextHost;
    output.sourceHostRegion = nextHost;
    output.regionIndex = region.index;
    output.destinationArrival = region.arrival;
    output.destination = copied.destination;
    output.membershipAfter = membershipAfter;
    output.advertisement = advertisement;
    output.required = required;
    output.burst = burst;
    output.publishesHud = publishesHud;
    if (requires_notification(required, RegionNotification::membership)
        && !make_membership_wire(lineage.bound,allowArrival && name=="mission_scot"
                && hasLayout && layout.tag==0x80F47522U,
            membershipAfter, advertisement, output.membershipWire)) {
        gameplay::group::release_host_activity_lineage(advertisementLease);
        return RegionSnapshotBuildResult::failed;
    }
    const RosterSnapshotInputs rosterInputs{
        copied.destination,
        copied.defaults,
        membershipAfter,
        copied.grantBefore,
        region.index,
        region.arrival,
    };
    if (!finalize_roster(
            session, scratch, rosterInputs, rearmMissionDirector, output)) {
        gameplay::group::release_host_activity_lineage(advertisementLease);
        return RegionSnapshotBuildResult::failed;
    }
    return RegionSnapshotBuildResult::ready;
}

} // namespace

/** Finalizes one prepared authoritative/refresh region plan. */
RegionSnapshotBuildResult build_region_transition_snapshot(
    const Session& session,
    Scratch& scratch,
    const activity_message::ActivityPlan& plan,
    RegionTransitionSnapshot& output,
    RegionPublicationDebt& debt,
    gameplay::group::HostActivityLineageLease& advertisementLease,
    gameplay::group::HostActivityLineageLease& boundLineageLease) noexcept {
    output = {};
    debt = {};
    gameplay::group::release_host_activity_lineage(advertisementLease);
    gameplay::group::release_host_activity_lineage(boundLineageLease);
    if (!lifecycle::activity_binding_is_current(session)
        || plan.instanceKey != session.activity.instance
        || plan.membershipMutation.instanceKey != plan.instanceKey) {
        return RegionSnapshotBuildResult::stale;
    }
    if (!acquire_region_lineage_locked(
            session, session.activity.lineage, boundLineageLease)) {
        return RegionSnapshotBuildResult::stale;
    }

    NotificationMask required = 0;
    if (plan.delivery == activity_message::Delivery::refreshNotifications) {
        required |= notification_mask(RegionNotification::globalState);
        required |= notification_mask(RegionNotification::roster);
        if (plan.membershipMutation.hasSnapshot) {
            required |= notification_mask(RegionNotification::membership);
        }
    } else if (plan.delivery == activity_message::Delivery::authoritativeNotifications) {
        if (plan.membershipMutation.hasSnapshot) {
            required |= notification_mask(RegionNotification::membership);
        }
        if (plan.regionMoved) {
            required |= notification_mask(RegionNotification::roster);
        }
    } else {
        gameplay::group::release_host_activity_lineage(boundLineageLease);
        return RegionSnapshotBuildResult::failed;
    }

    state::activity::membership::RegionSnapshotInputs copied{};
    state::activity::membership::MembershipState membershipAfter{};
    state::activity::HostRegionKey expectedHost{};
    state::activity::HostRegionKey nextHost{};
    if (plan.delivery == activity_message::Delivery::authoritativeNotifications) {
        const auto& transition = plan.membershipMutation.regionTransition;
        if (session.activity.lineage.bound != session.activity.lineage.source
            || transition.activity != session.activity.lineage.source
            || transition.activity != plan.instanceKey
            || transition.movesRegion != plan.regionMoved) {
            gameplay::group::release_host_activity_lineage(boundLineageLease);
            return RegionSnapshotBuildResult::stale;
        }
        copied.bound = transition.activity;
        copied.source = transition.activity;
        copied.sourceHostRegion = transition.nextHostRegion;
        copied.destination = transition.destination;
        copied.grantBefore = transition.grantBefore;
        copied.sourceMembership = transition.after;
        state::activity::defaults::snapshot(copied.defaults);
        copied.stateRevision = transition.expectedStateRevision;
        copied.boundRecordRevision = transition.expectedRecordRevision;
        copied.sourceRecordRevision = transition.expectedRecordRevision;
        membershipAfter = transition.after;
        expectedHost = transition.expectedHostRegion;
        nextHost = transition.nextHostRegion;
    } else {
        if (!state::activity::membership::snapshot_region_inputs(
                session.activity.lineage.bound,
                session.activity.lineage.source,
                {},
                copied)) {
            gameplay::group::release_host_activity_lineage(boundLineageLease);
            return RegionSnapshotBuildResult::stale;
        }
        membershipAfter = copied.sourceMembership;
        expectedHost = copied.sourceHostRegion;
        nextHost = copied.sourceHostRegion;
    }
    const RegionSnapshotBuildResult result = finalize_snapshot(
        session,
        scratch,
        copied,
        membershipAfter,
        expectedHost,
        nextHost,
        required,
        false,
        plan.delivery == activity_message::Delivery::authoritativeNotifications
            && plan.regionMoved,
        false,
        output,
        debt,
        advertisementLease);
    if (result == RegionSnapshotBuildResult::failed
        || result == RegionSnapshotBuildResult::stale) {
        gameplay::group::release_host_activity_lineage(boundLineageLease);
    }
    return result;
}

/** Finalizes one periodic bundle from the State value captured with its commit mutation. */
RegionSnapshotBuildResult build_periodic_region_snapshot(
    const Session& session,
    Scratch& scratch,
    const state::activity::membership::PeriodicRegionRefresh& refresh,
    const state::activity::membership::PendingMutation& mutation,
    bool burst,
    bool rearmMissionDirector,
    RegionTransitionSnapshot& output,
    gameplay::group::HostActivityLineageLease& advertisementLease,
    gameplay::group::HostActivityLineageLease& boundLineageLease) noexcept {
    output = {};
    gameplay::group::release_host_activity_lineage(advertisementLease);
    gameplay::group::release_host_activity_lineage(boundLineageLease);
    if (!lifecycle::activity_binding_is_current(session)
        || refresh.inputs.bound != session.activity.instance
        || refresh.inputs.source != session.activity.instance
        || mutation.instanceKey != session.activity.instance
        || (mutation.kind != state::activity::membership::MutationKind::refresh
            && mutation.kind != state::activity::membership::MutationKind::republish)
        || !acquire_region_lineage_locked(
            session, session.activity.lineage, boundLineageLease)) {
        return RegionSnapshotBuildResult::stale;
    }
    state::activity::membership::MembershipState membershipAfter =
        refresh.inputs.sourceMembership;
    if (mutation.kind == state::activity::membership::MutationKind::republish) {
        membershipAfter = mutation.regionTransition.after;
    }
    NotificationMask required = notification_mask(RegionNotification::roster);
    if (!burst) {
        required |= notification_mask(RegionNotification::globalState);
    }
    if (refresh.publishesMembership) {
        required |= notification_mask(RegionNotification::membership);
    }
    RegionPublicationDebt ignored{};
    const RegionSnapshotBuildResult result = finalize_snapshot(
        session,
        scratch,
        refresh.inputs,
        membershipAfter,
        refresh.inputs.sourceHostRegion,
        refresh.inputs.sourceHostRegion,
        required,
        burst,
        false,
        rearmMissionDirector,
        output,
        ignored,
        advertisementLease);
    if (result != RegionSnapshotBuildResult::ready
        && result != RegionSnapshotBuildResult::pending) {
        gameplay::group::release_host_activity_lineage(boundLineageLease);
    }
    return result;
}

/** Rebuilds one exact deferred debt without selecting newer/global State. */
RegionSnapshotBuildResult build_region_debt_snapshot(
    const Session& session,
    Scratch& scratch,
    const RegionPublicationDebt& debt,
    RegionTransitionSnapshot& output,
    gameplay::group::HostActivityLineageLease& advertisementLease,
    gameplay::group::HostActivityLineageLease& boundLineageLease) noexcept {
    output = {};
    gameplay::group::release_host_activity_lineage(advertisementLease);
    gameplay::group::release_host_activity_lineage(boundLineageLease);
    if (!debt.present || !lifecycle::activity_binding_is_current(session)
        || debt.binding != session.activity.key || debt.activity != session.activity.instance
        || debt.activity != session.activity.lineage.bound
        || debt.regionSource != session.activity.lineage.source
        || !acquire_region_lineage_locked(
            session, session.activity.lineage, boundLineageLease)) {
        return RegionSnapshotBuildResult::stale;
    }
    state::activity::membership::RegionSnapshotInputs copied{};
    if (!state::activity::membership::snapshot_region_inputs(debt.activity,
                                                              debt.regionSource,
                                                              debt.committedHostRegion,
                                                              copied)
        || std::memcmp(&copied.sourceMembership,
                       &debt.membershipAfter,
                       sizeof debt.membershipAfter)
               != 0
        || std::memcmp(&copied.destination, &debt.destination, sizeof debt.destination) != 0) {
        gameplay::group::release_host_activity_lineage(boundLineageLease);
        return RegionSnapshotBuildResult::stale;
    }
    RegionPublicationDebt ignored{};
    const RegionSnapshotBuildResult result = finalize_snapshot(session,
                                                               scratch,
                                                               copied,
                                                               debt.membershipAfter,
                                                               debt.committedHostRegion,
                                                               debt.committedHostRegion,
                                                               debt.required,
                                                               false,
                                                               debt.publishesHud,
                                                               false,
                                                               output,
                                                               ignored,
                                                               advertisementLease);
    if (result != RegionSnapshotBuildResult::ready
        && result != RegionSnapshotBuildResult::pending) {
        gameplay::group::release_host_activity_lineage(boundLineageLease);
    }
    return result;
}

} // namespace sunrise::server::bap::encrypted::push::activity
