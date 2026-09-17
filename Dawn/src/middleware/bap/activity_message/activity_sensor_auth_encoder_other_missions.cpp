#include "garden_ending_encoder.h"
#include "../../../state/activity/omega/omega_mission_authority.h"
#include "../../../state/activity/omega/omega_rescue_catalog.h"
#include <algorithm>

#include "sensor_auth_update.h"
#include "native/roster_lifetime_wire.h"
#include "../../../state/activity/omega/omega_progression.h"
#include "../../../state/activity/omega/omega_portal_entry.h"
#include "../../../state/activity/omega/omega_ikora_authority.h"

namespace dawn::middleware::bap::activity_message::sensor_auth_update {
namespace {

namespace bits = encoding::bits;
namespace ikora = state::activity::omega::ikora;

/** The type-13 slot type, which is the only one that may carry the player key. */
constexpr std::uint8_t kSlotTypeParticipation = 13;
/** Native-owned mission state omitted from Omega's deltas after its one initialization packet. */
constexpr std::uint8_t kSlotTypeActivityScript = 18;
constexpr std::uint8_t kSlotTypeMissionDirector = 35;
/** Omega's authored activity-script and mission-director registry. */
constexpr std::uint32_t kOmegaMissionRuntimeRegistry = 0x4786C0E0U;
/** Client-backed scene_ikora_opens_portal authority record. */
constexpr std::uint32_t kOmegaOpeningRegistry = 0xD00142CFU;
/** Client-backed lighthouse_teleport authority record. */
constexpr std::uint32_t kOmegaTeleportRegistry = 0xBA5F26EFU;
/** Ghost pre-roll dialogue slot (type 53) in the global sensor group; body from bodies.cpp. */
constexpr std::uint32_t kOmegaDialogueRegistry = 0x82FB58B7U;
/** Infinite Forest map-generator group (bubble 11), amended into the roster activity-wide. */
constexpr std::uint32_t kOmegaForestGeneratorRegistry = 0x2763EC97U;
constexpr std::uint8_t kOmegaDialogueSlotType = 53;
constexpr std::uint16_t kOmegaDialogueSlotIndex = 2;
constexpr std::uint8_t kOmegaPortalVisualSlotType = 4;
constexpr std::uint16_t kOmegaTeleportSlotIndex = 0;
constexpr std::uint8_t kOmegaPortalGateSlotType = 23;
constexpr std::uint16_t kOmegaPortalGateSlotIndex = 1;
constexpr std::uint16_t kOmegaPortalVisualFirstIndex = 2;
constexpr std::uint16_t kOmegaPortalVisualLastIndex = 4;
constexpr std::uint16_t kOmegaGateControllerSlotIndex = 16;
constexpr std::uint8_t kOmegaEngagementSlotType = 70;
constexpr std::uint16_t kOmegaEngagementSlotIndex = 17;
constexpr std::uint8_t kOmegaMonitorSlotType = 30;
constexpr std::uint16_t kOmegaOpeningMonitorSlotIndex = 20;
constexpr std::uint16_t kOmegaEntranceMonitorSlotIndex = 24;
/** The participation region rides a signed field, so this is the widest index it accepts. */
constexpr std::uint32_t kMaximumRegion = 0x7FFFFFFF;
[[nodiscard]] bool valid_sub_blocks(std::span<const BubbleSubBlock> subBlocks) noexcept {
    if (subBlocks.size() > kBubbleSubBlockCapacity) {
        return false;
    }
    for (std::size_t index = 0; index < subBlocks.size(); ++index) {
        const BubbleSubBlock& block = subBlocks[index];
        if (block.bubble > kMaximumSubBlockBubble || block.keys.empty()
            || block.keys.size() > kBubbleKeyCapacity) {
            return false;
        }
        for (std::size_t earlier = 0; earlier < index; ++earlier) {
            if (subBlocks[earlier].bubble == block.bubble) {
                return false;
            }
        }
    }
    return true;
}

[[nodiscard]] bool ikora_authority(const Snapshot& snapshot) noexcept {
    return snapshot.omegaSceneAuthority && snapshot.seedAuthoredSensors
        && kOmegaSceneAuthorityBodyReady;
}

[[nodiscard]] bool valid_ikora_sources(const Snapshot& snapshot) noexcept {
    if (!ikora_authority(snapshot)) return true;
    unsigned sources = 0, scenes = 0, gates = 0, groups = 0;
    for (std::size_t group = 0; group < snapshot.roster.groupCount; ++group) {
        const auto& row = snapshot.roster.groups[group];
        if (row.key != ikora::kRegistry) continue;
        ++groups;
        for (std::size_t slot = 0; slot < row.slotTypes.size(); ++slot) {
            sources += ikora::source_slot(row.key, row.slotTypes[slot], row.slotIndices[slot]);
            scenes += ikora::scene_slot(row.key, row.slotTypes[slot], row.slotIndices[slot]);
            gates += ikora::gate_slot(row.key, row.slotTypes[slot], row.slotIndices[slot]);
        }
    }
    return groups == 1 && sources == 1 && scenes == 1 && gates == 1;
}

/**
 * Checks the scalars whose out-of-range values would encode with no complaint.
 * @param snapshot Message input.
 * @return True when every scalar fits its field.
 */
[[nodiscard]] bool valid(const Snapshot& snapshot) noexcept {
    if (!lifetime_wire::valid(snapshot.roster)) return false;
    if((snapshot.activityClock && !native::activity_clock::valid(*snapshot.activityClock))
        || (!snapshot.activityClock && snapshot.activityElapsedTicks))return false;
    if(!native::placement::valid(snapshot.placements,snapshot.roster,snapshot.region)) return false;
    if(!native::lost_sector_shield::valid(snapshot.lostSectorShields,snapshot.roster,snapshot.region)) return false;
    if(!native::engagement::valid(snapshot.engagements,snapshot.roster,snapshot.region)) return false;
    if(!native::forest_generator::valid(snapshot.generators,snapshot.roster,snapshot.region)) return false;
    if(!native::world_device::valid(snapshot.devices,snapshot.roster,snapshot.region)) return false;
    if(!native::npc_animation::valid(snapshot.animations,snapshot.roster,snapshot.region)) return false;
    if(!native::cue::valid(snapshot.cues,snapshot.roster)) return false;
    if(!native::dialogue::valid(snapshot.dialogues,snapshot.roster)) return false;
    if(!native::world_sequence::valid(snapshot.sequences,snapshot.roster,snapshot.region)) return false;
    if(!native::event_participant::valid(snapshot.eventParticipants,snapshot.roster,snapshot.region)) return false;
    if(!native::music::valid(snapshot.music,snapshot.roster,snapshot.region)) return false;
    if(!native::player_predicates::compose(snapshot.playerPredicates,snapshot.omegaPortalPlayerHash,
        state::activity::omega::portal_entry::kRequiredPlayerHash))return false;
    if(!native::population::valid(snapshot.populations,snapshot.roster,snapshot.region)) return false;
    if (std::find(kLifetimeStates.begin(), kLifetimeStates.end(), snapshot.lifetime)
        == kLifetimeStates.end()) {
        return false;
    }
    if (snapshot.hasRegion && snapshot.region > kMaximumRegion) {
        return false;
    }
    if (snapshot.hasSpawnOverride
        && (snapshot.spawnSliceSet > kMaximumSpawnSliceSet || snapshot.spawnSetHash == 0
            || (snapshot.spawnSetHash == kAbsentSpawnSetHash && !snapshot.preferSpawnHistory))) {
        return false;
    }
    if(snapshot.preferSpawnHistory && (!snapshot.hasSpawnOverride
        || snapshot.spawnSetHash!=kAbsentSpawnSetHash))return false;
    if (snapshot.missionDirectorVariant > kMaximumMissionDirectorVariant) {
        return false;
    }
    if (snapshot.publishAuthoredCueTransition
        && (snapshot.authoredCueRegistry == 0U
            || snapshot.authoredDirectiveEvent == 0U
            || snapshot.authoredDialogueRecord >= 128U
            || snapshot.authoredCueStage == 0U)) {
        return false;
    }
    if (!snapshot.publishAuthoredCueTransition
        && (snapshot.authoredCueRegistry != 0U
            || snapshot.authoredDirectiveEvent != 0U
            || snapshot.authoredDialogueRecord != 0U
            || snapshot.authoredCueStage != 0U)) {
        return false;
    }
    if (snapshot.publishAuthoredSceneSelector
        && (!snapshot.publishAuthoredCueTransition
            || snapshot.authoredSceneRegistry == 0U
            || snapshot.authoredSceneType == 0U
            || snapshot.authoredSceneSelector == 0U
            || snapshot.authoredSceneEntryRegistry == 0U
            || snapshot.authoredSceneEntryType >= 127U
            || snapshot.authoredSceneEntryIndex > 0x7FFFU)) {
        return false;
    }
    if (!snapshot.publishAuthoredSceneSelector
        && (snapshot.authoredSceneRegistry != 0U
            || snapshot.authoredSceneType != 0U
            || snapshot.authoredSceneIndex != 0U
            || snapshot.authoredSceneSelector != 0U
            || snapshot.authoredSceneEntryRegistry != 0U
            || snapshot.authoredSceneEntryType != 0U
            || snapshot.authoredSceneEntryIndex != 0U)) {
        return false;
    }
    const bool publishesRuntime = snapshot.omegaOpeningStage == kOmegaOpeningStageTriggered
                                  || snapshot.omegaOpeningStage
                                         == kOmegaOpeningStageCompleted
                                  || snapshot.omegaOpeningStage
                                         == kOmegaForestStageTransition;
    if (snapshot.omegaOpeningStage > kOmegaForestStageSettled
        || snapshot.omegaOpeningStage == kOmegaOpeningStageBaseline
        || snapshot.publishOmegaOpeningTransition
               != publishesRuntime) {
        return false;
    }
    const bool sceneStage = snapshot.omegaOpeningStage == kOmegaOpeningStageScene
                            || snapshot.omegaOpeningStage == kOmegaOpeningStageReady;
    const bool portalAuthorityStage =
        snapshot.omegaOpeningStage == kOmegaOpeningStageTriggered
        || snapshot.omegaOpeningStage == kOmegaOpeningStagePortal;
    const bool portalTransitionStage =
        snapshot.omegaOpeningStage == kOmegaOpeningStageCompleted
        || snapshot.omegaOpeningStage == kOmegaForestStageTransition;
    if ((sceneStage
         && (!snapshot.omegaSceneAuthority || !kOmegaSceneAuthorityBodyReady))
        || (portalAuthorityStage && !snapshot.omegaPortalMutation)
        || (portalTransitionStage && !snapshot.omegaPortalMutation)) {
        return false;
    }
    // The grant is a change, not a value: the client compares it against a mirror that starts at
    // zero, so a token of zero grants nothing.
    if (snapshot.hasGrant
        && (snapshot.grant.bubble > kMaximumGrantBubble
            || snapshot.grant.token < kMinimumGrantToken)) {
        return false;
    }
    if (snapshot.roster.groupCount > kGroupCapacity
        || snapshot.roster.topLevelGroupCount > snapshot.roster.groupCount) {
        return false;
    }
    for (std::size_t group = 0; group < snapshot.roster.groupCount; ++group) {
        const Group& row = snapshot.roster.groups[group];
        if (row.slotTypes.size() != row.slotFlags.size()
            || row.slotTypes.size() != row.slotIndices.size() || row.slotTypes.empty()) {
            return false;
        }
        for (const std::uint16_t index : row.slotIndices) {
            if (index > kMaximumSlotIndex) {
                return false;
            }
        }
    }
    return valid_sub_blocks(snapshot.roster.bubbleSubBlocks) && valid_ikora_sources(snapshot);
}

/**
 * Writes every group's object blocks, in publish order. Every registered object must be seeded
 * before any auth state applies, because the client's gate walks the whole sync-record pool. A
 * partial message seeds nothing that applies.
 * @param writer Body writer sitting after the phase-1 delta.
 * @param snapshot Message input.
 * @return True when every block fits.
 */
/**
 * Emits the Infinite Forest generator group's object blocks (empty bodies) so its sync-pool
 * objects can SEED. The client's record processor rewinds records whose object bubble is not
 * the CURRENT bubble, so bubble-11 objects ignore the early full-roster pushes made while the
 * player stands in the Lighthouse; unless the post-opening stages keep publishing this group,
 * its objects stay pending forever, ClientRosterSync_AllRecordsInBubbleSeeded vetoes bubble
 * 11's seed commit, and the sweep that instantiates the network-replicated map-generator
 * worker never runs. A group block is self-contained, so appending one is framing-safe.
 */
[[nodiscard]] bool write_forest_generator_group(bits::Writer& writer,
                                               const Snapshot& snapshot) noexcept {
    for (std::size_t group = 0; group < snapshot.roster.groupCount; ++group) {
        const Group& row = snapshot.roster.groups[group];
        if (row.key != kOmegaForestGeneratorRegistry
            && row.key != state::activity::omega::kHandoffGroup
            && row.key != state::activity::omega::kCrownGroup
            && row.key != state::activity::omega::kBossGroup
            && row.key != state::activity::omega::kRevealGroup
            && !(snapshot.omegaEndingSelected && row.key==0x3A6CE17AU)
            && !(snapshot.omegaMission.generation
                && (state::activity::omega::mission_authority::registry(row.key)
                    || row.key==state::activity::omega::rescue::kRegistry))) {
            continue;
        }
        if (row.slotTypes.empty()) {
            continue;
        }
        bool encoded = writer.write(1, kPresenceWidth) && writer.write(row.key, kKeyWidth)
                       && writer.write(0, kKeyWidth);
        for (std::size_t slot = 0; encoded && slot < row.slotTypes.size(); ++slot) {
            // The extracted flags miss most of this group's sync-carrying slots (the client's
            // pool provably holds objects for 70/0 and 30/2 with extracted flag 0), so force the
            // auth flag on EVERY slot: an empty {reset=1, present=0} block seeds a pool object,
            // and the client rewinds blocks for slots it has no pool entry for, so over-emission
            // is harmless.
            encoded = legacy_write_object_block(writer,
                                         snapshot,
                                         row.key,
                                         row.slotTypes[slot],
                                         row.slotIndices[slot],
                                         static_cast<std::uint8_t>(row.slotFlags[slot]
                                                                   | kSlotAuthFlag),
                                         false);
        }
        if (!encoded || !writer.write(0, kPresenceWidth)) return false;
    }
    return true;
}

/** Publishes only the authored contact carrier, without touching the gate or beam trio. */
[[nodiscard]] bool write_portal_entry_group(bits::Writer& writer, const Snapshot& snapshot) noexcept {
    if (!snapshot.omegaPortalEntry) return true;
    for (std::size_t group = 0; group < snapshot.roster.groupCount; ++group) {
        const Group& row = snapshot.roster.groups[group];
        if (row.key != state::activity::omega::portal_entry::kRegistry) continue;
        for (std::size_t slot = 0; slot < row.slotTypes.size(); ++slot) {
            if (!state::activity::omega::portal_entry::slot(row.key, row.slotTypes[slot], row.slotIndices[slot])) continue;
            return writer.write(1, kPresenceWidth) && writer.write(row.key, kKeyWidth)
                && writer.write(0, kKeyWidth)
                && legacy_write_object_block(writer, snapshot, row.key, 4U, 0U,
                                      static_cast<std::uint8_t>(row.slotFlags[slot] | kSlotAuthFlag), false)
                && writer.write(0, kPresenceWidth);
        }
    }
    return false;
}

/** Flags are not sent in an object header. Native descriptors decide whether to
 * consume the sense-present tail, so selecting authority must preserve that flag. */
[[nodiscard]] bool write_ikora_object(bits::Writer& writer, const Snapshot& snapshot,
                                     const Group& row, std::uint8_t type,
                                     std::uint16_t index) noexcept {
    for (std::size_t slot = 0; slot < row.slotTypes.size(); ++slot) {
        if (row.slotTypes[slot] != type || row.slotIndices[slot] != index) continue;
        return legacy_write_object_block(writer, snapshot, row.key, type, index,
                                  static_cast<std::uint8_t>(row.slotFlags[slot] | kSlotAuthFlag), false);
    }
    return false;
}

/** Source first, then its requesting Scene, then the exact near-portal lattice. */
[[nodiscard]] bool write_ikora_group(bits::Writer& writer, const Snapshot& snapshot) noexcept {
    if (!ikora_authority(snapshot)) return true;
    for (std::size_t group = 0; group < snapshot.roster.groupCount; ++group) {
        const Group& row = snapshot.roster.groups[group];
        if (row.key != ikora::kRegistry) continue;
        return writer.write(1, kPresenceWidth) && writer.write(row.key, kKeyWidth)
            && writer.write(0, kKeyWidth)
            && write_ikora_object(writer, snapshot, row, 1, 0)
            && write_ikora_object(writer, snapshot, row, 43, 1)
            && write_ikora_object(writer, snapshot, row, 23, 16)
            && writer.write(0, kPresenceWidth);
    }
    return false;
}

/** Keep the two host-driven presentation slots alive after native script ownership transfers.
 * No type-18/35, encounters, doors or native forest authority are overwritten here. */
[[nodiscard]] bool write_omega_progression(bits::Writer& writer, const Snapshot& snapshot) noexcept {
    if (!write_ikora_group(writer, snapshot)) return false;
    if (!snapshot.omegaSceneAuthority || !snapshot.omegaDialogueArm)
        return write_portal_entry_group(writer, snapshot);
    for (std::size_t group = 0; group < snapshot.roster.groupCount; ++group) {
        const Group& row = snapshot.roster.groups[group];
        if (row.key != kOmegaDialogueRegistry) continue;
        bool encoded = writer.write(1, kPresenceWidth) && writer.write(row.key, kKeyWidth)
                       && writer.write(0, kKeyWidth);
        unsigned count = 0;
        for (std::size_t slot = 0; encoded && slot < row.slotTypes.size(); ++slot) {
            if (!((row.slotTypes[slot] == 53 && row.slotIndices[slot] == 2)
                  || (row.slotTypes[slot] == 68 && row.slotIndices[slot] == 0))) continue;
            encoded = legacy_write_object_block(writer, snapshot, row.key, row.slotTypes[slot],
                                         row.slotIndices[slot],
                                         static_cast<std::uint8_t>(row.slotFlags[slot] | kSlotAuthFlag), false);
            ++count;
        }
        return encoded && count == 2 && writer.write(0, kPresenceWidth)
            && write_portal_entry_group(writer, snapshot);
    }
    return false;
}

[[nodiscard]] bool write_phase_two(bits::Writer& writer, const Snapshot& snapshot) noexcept {
    if (snapshot.publishAuthoredCueTransition) {
        bool encoded = true;
        std::size_t selectedGroups = 0U;
        std::size_t selectedObjects = 0U;
        for (std::size_t group = 0U;
             encoded && group < snapshot.roster.groupCount;
             ++group) {
            const Group& row = snapshot.roster.groups[group];
            const bool runtimeGroup = row.key == kOmegaMissionRuntimeRegistry;
            const bool cueGroup = row.key == snapshot.authoredCueRegistry;
            const bool sceneGroup = snapshot.publishAuthoredSceneSelector
                                    && row.key == snapshot.authoredSceneRegistry;
            if (!runtimeGroup && !cueGroup && !sceneGroup) {
                continue;
            }
            bool hasTarget = false;
            for (std::size_t slot = 0U; slot < row.slotTypes.size(); ++slot) {
                const bool runtime = runtimeGroup
                                     && (row.slotTypes[slot] == kSlotTypeActivityScript
                                         || row.slotTypes[slot] == kSlotTypeMissionDirector);
                const bool cue = cueGroup
                                 && ((row.slotTypes[slot] == 68U
                                      && row.slotIndices[slot] == 0U)
                                     || (row.slotTypes[slot] == kOmegaDialogueSlotType
                                         && row.slotIndices[slot]
                                                == kOmegaDialogueSlotIndex));
                const bool scene = sceneGroup
                                   && row.slotTypes[slot] == snapshot.authoredSceneType
                                   && row.slotIndices[slot] == snapshot.authoredSceneIndex;
                hasTarget = hasTarget || runtime || cue || scene;
            }
            if (!hasTarget) {
                continue;
            }
            encoded = writer.write(1, kPresenceWidth) && writer.write(row.key, kKeyWidth)
                      && writer.write(0, kKeyWidth);
            std::size_t selectedInGroup = 0U;
            for (std::size_t slot = 0U; encoded && slot < row.slotTypes.size(); ++slot) {
                const bool runtime = runtimeGroup
                                     && (row.slotTypes[slot] == kSlotTypeActivityScript
                                         || row.slotTypes[slot] == kSlotTypeMissionDirector);
                const bool cue = cueGroup
                                 && ((row.slotTypes[slot] == 68U
                                      && row.slotIndices[slot] == 0U)
                                     || (row.slotTypes[slot] == kOmegaDialogueSlotType
                                         && row.slotIndices[slot]
                                                == kOmegaDialogueSlotIndex));
                const bool scene = sceneGroup
                                   && row.slotTypes[slot] == snapshot.authoredSceneType
                                   && row.slotIndices[slot] == snapshot.authoredSceneIndex;
                if (!runtime && !cue && !scene) {
                    continue;
                }
                encoded = legacy_write_object_block(writer,
                                             snapshot,
                                             row.key,
                                             row.slotTypes[slot],
                                             row.slotIndices[slot],
                                             static_cast<std::uint8_t>(row.slotFlags[slot]
                                                                       | kSlotAuthFlag),
                                             false);
                ++selectedInGroup;
                ++selectedObjects;
            }
            const std::size_t expectedInGroup = (runtimeGroup ? 2U : 0U)
                                                + (cueGroup ? 2U : 0U)
                                                + (sceneGroup ? 1U : 0U);
            encoded = encoded && selectedInGroup == expectedInGroup
                      && writer.write(0, kPresenceWidth);
            ++selectedGroups;
        }
        std::size_t expectedGroups = 2U;
        if (snapshot.publishAuthoredSceneSelector
            && snapshot.authoredSceneRegistry != kOmegaMissionRuntimeRegistry
            && snapshot.authoredSceneRegistry != snapshot.authoredCueRegistry) {
            ++expectedGroups;
        }
        const std::size_t expectedObjects = 4U
                                            + (snapshot.publishAuthoredSceneSelector ? 1U : 0U);
        return encoded && selectedGroups == expectedGroups
               && selectedObjects == expectedObjects;
    }
    if (snapshot.omegaOpeningStage == kOmegaOpeningStageScene
        || snapshot.omegaOpeningStage == kOmegaOpeningStageReady) {
        if (!write_ikora_group(writer, snapshot)) return false;
        const bool dialogue = kOmegaDialogueBodyReady && snapshot.omegaSceneAuthority
                              && snapshot.omegaDialogueArm;
        unsigned dialogueObjects = 0;
        for (std::size_t group = 0; dialogue && group < snapshot.roster.groupCount; ++group) {
            const auto& row = snapshot.roster.groups[group];
            if (row.key != kOmegaDialogueRegistry) continue;
            for (std::size_t slot = 0; slot < row.slotTypes.size(); ++slot) {
                if (row.slotTypes[slot] != kOmegaDialogueSlotType
                    || row.slotIndices[slot] != kOmegaDialogueSlotIndex) continue;
                if (++dialogueObjects > 1
                    || !writer.write(1, kPresenceWidth) || !writer.write(row.key, kKeyWidth)
                    || !writer.write(0, kKeyWidth)
                    || !legacy_write_object_block(writer, snapshot, row.key, kOmegaDialogueSlotType,
                                            kOmegaDialogueSlotIndex,
                                            static_cast<std::uint8_t>(row.slotFlags[slot] | kSlotAuthFlag), false)
                    || !writer.write(0, kPresenceWidth)) return false;
            }
        }
        return write_forest_generator_group(writer, snapshot)
            && write_portal_entry_group(writer, snapshot);
    }

    // Retained native source/Scene/device authority is independent of the legacy
    // script-stage experiment. Never let those stages instantiate portal beams
    // or a carrier before the accepted native lattice output.
    if (ikora_authority(snapshot)
        && snapshot.omegaOpeningStage >= kOmegaOpeningStageTriggered) {
        return write_omega_progression(writer, snapshot)
            && write_forest_generator_group(writer, snapshot);
    }
    if (snapshot.omegaOpeningStage == kOmegaOpeningStageTriggered
        || snapshot.omegaOpeningStage == kOmegaOpeningStagePortal) {
        const bool includesRuntime =
            snapshot.omegaOpeningStage == kOmegaOpeningStageTriggered;
        bool encoded = true;
        std::size_t selectedGroups = 0;
        std::size_t selectedObjects = 0;
        for (std::size_t group = 0;
             encoded && group < snapshot.roster.groupCount;
             ++group) {
            const Group& row = snapshot.roster.groups[group];
            const bool runtimeGroup =
                includesRuntime && row.key == kOmegaMissionRuntimeRegistry;
            const bool teleportGroup = row.key == kOmegaTeleportRegistry;
            const bool visualGroup = row.key == kOmegaOpeningRegistry;
            if (!runtimeGroup && !teleportGroup && !visualGroup) {
                continue;
            }
            encoded = writer.write(1, kPresenceWidth)
                      && writer.write(row.key, kKeyWidth) && writer.write(0, kKeyWidth);
            std::size_t selectedInGroup = 0;
            for (std::size_t slot = 0; encoded && slot < row.slotTypes.size(); ++slot) {
                const std::uint16_t slotIndex = row.slotIndices[slot];
                const std::uint8_t slotType = row.slotTypes[slot];
                const bool selectedRuntime =
                    runtimeGroup
                    && (slotType == kSlotTypeActivityScript
                        || slotType == kSlotTypeMissionDirector);
                const bool selectedTeleport = teleportGroup
                                              && slotType == kOmegaPortalVisualSlotType
                                              && slotIndex == kOmegaTeleportSlotIndex;
                const bool selectedGate = teleportGroup
                                          && slotType == kOmegaPortalGateSlotType
                                          && slotIndex == kOmegaPortalGateSlotIndex;
                const bool selectedVisual = visualGroup
                                            && slotType == kOmegaPortalVisualSlotType
                                            && slotIndex >= kOmegaPortalVisualFirstIndex
                                            && slotIndex <= kOmegaPortalVisualLastIndex;
                const bool selectedMissionGate =
                    visualGroup && slotType == kOmegaPortalGateSlotType
                    && slotIndex == kOmegaGateControllerSlotIndex;
                const bool selectedEngagement =
                    visualGroup && slotType == kOmegaEngagementSlotType
                    && slotIndex == kOmegaEngagementSlotIndex;
                const bool selectedMonitor =
                    visualGroup && slotType == kOmegaMonitorSlotType
                    && (slotIndex == kOmegaOpeningMonitorSlotIndex
                        || slotIndex == kOmegaEntranceMonitorSlotIndex);
                if ((row.slotFlags[slot] & (kSlotAuthFlag | kSlotSenseFlag)) == 0
                    || (!selectedRuntime && !selectedTeleport && !selectedGate
                        && !selectedVisual && !selectedMissionGate
                        && !selectedEngagement && !selectedMonitor)) {
                    continue;
                }
                encoded = legacy_write_object_block(writer,
                                             snapshot,
                                             row.key,
                                             row.slotTypes[slot],
                                             row.slotIndices[slot],
                                             row.slotFlags[slot],
                                             false);
                ++selectedInGroup;
                ++selectedObjects;
            }
            const std::size_t expectedInGroup = runtimeGroup     ? 2U
                                                : teleportGroup ? 2U
                                                                : 7U;
            encoded = encoded && selectedInGroup == expectedInGroup
                      && writer.write(0, kPresenceWidth);
            ++selectedGroups;
        }
        const std::size_t expectedGroups = includesRuntime ? 3U : 2U;
        const std::size_t expectedObjects = includesRuntime ? 11U : 9U;
        return encoded && selectedGroups == expectedGroups
               && selectedObjects == expectedObjects
               && write_forest_generator_group(writer, snapshot);
    }
    if (snapshot.omegaOpeningStage == kOmegaOpeningStageCompleted
        || snapshot.omegaOpeningStage == kOmegaForestStageTransition) {
        for (std::size_t group = 0; group < snapshot.roster.groupCount; ++group) {
            const Group& row = snapshot.roster.groups[group];
            if (row.key != kOmegaMissionRuntimeRegistry) {
                continue;
            }
            bool encoded = writer.write(1, kPresenceWidth)
                           && writer.write(row.key, kKeyWidth) && writer.write(0, kKeyWidth);
            std::size_t selected = 0;
            for (std::size_t slot = 0; encoded && slot < row.slotTypes.size(); ++slot) {
                const std::uint8_t slotType = row.slotTypes[slot];
                if ((row.slotFlags[slot] & (kSlotAuthFlag | kSlotSenseFlag)) == 0
                    || (slotType != kSlotTypeActivityScript
                        && slotType != kSlotTypeMissionDirector)) {
                    continue;
                }
                encoded = legacy_write_object_block(writer,
                                             snapshot,
                                             row.key,
                                             slotType,
                                             row.slotIndices[slot],
                                             row.slotFlags[slot],
                                             false);
                ++selected;
            }
            return encoded && selected == 2 && writer.write(0, kPresenceWidth)
                   && write_forest_generator_group(writer, snapshot);
        }
        return false;
    }
    if (snapshot.omegaOpeningStage == kOmegaOpeningStageSettled) {
        return write_omega_progression(writer, snapshot) && write_forest_generator_group(writer, snapshot);
    }
    bool encoded = true;
    bool keyPlaced = false;
    for (std::size_t group = 0; encoded && group < snapshot.roster.groupCount; ++group) {
        const Group& row = snapshot.roster.groups[group];
        // The generator group's extracted flags miss most of its sync-carrying slots (the
        // client's pool holds objects for 70/0 and 30/2 with extracted flag 0), and a slot
        // that never receives a block never SEEDS, which vetoes bubble 11's replicated-content
        // sweep. Force the auth flag on all of its slots; the client rewinds blocks for slots
        // it has no pool entry for, so over-emission is harmless.
        const bool forestGroup = row.key == kOmegaForestGeneratorRegistry;
        // The filler word after the key is read and discarded.
        encoded = writer.write(1, kPresenceWidth) && writer.write(row.key, kKeyWidth)
                  && writer.write(0, kKeyWidth);
        const bool nativeIkora = ikora_authority(snapshot) && row.key == ikora::kRegistry;
        if (nativeIkora)
            encoded = encoded && write_ikora_object(writer, snapshot, row, 1, 0);
        for (std::size_t slot = 0; encoded && slot < row.slotTypes.size(); ++slot) {
            if (nativeIkora && ikora::source_slot(row.key, row.slotTypes[slot], row.slotIndices[slot]))
                continue;
            const bool ikoraSlot = nativeIkora
                && (ikora::scene_slot(row.key, row.slotTypes[slot], row.slotIndices[slot])
                    || ikora::gate_slot(row.key, row.slotTypes[slot], row.slotIndices[slot]));
            const bool portalEntry = snapshot.omegaPortalEntry
                && state::activity::omega::portal_entry::slot(row.key, row.slotTypes[slot], row.slotIndices[slot]);
            const std::uint8_t slotFlags =
                (forestGroup || portalEntry || ikoraSlot) ? static_cast<std::uint8_t>(row.slotFlags[slot] | kSlotAuthFlag)
                            : row.slotFlags[slot];
            if ((slotFlags & (kSlotAuthFlag | kSlotSenseFlag)) == 0) {
                continue;
            }
            const std::uint8_t slotType = row.slotTypes[slot];
            if (snapshot.preserveMissionAuthorityState
                && (slotType == kSlotTypeParticipation
                    || slotType == kSlotTypeActivityScript
                    || slotType == kSlotTypeMissionDirector)) {
                continue;
            }
            const bool firstOrEvery = !keyPlaced || snapshot.keyOnEveryParticipationSlot;
            const bool carriesPlayerKey = slotType == kSlotTypeParticipation
                                          && row.key == snapshot.roster.playerKeyGroup
                                          && firstOrEvery;
            keyPlaced = keyPlaced || carriesPlayerKey;
            encoded = legacy_write_object_block(writer,
                                         snapshot,
                                         row.key,
                                         slotType,
                                         row.slotIndices[slot],
                                         slotFlags,
                                         carriesPlayerKey);
        }
        encoded = encoded && writer.write(0, kPresenceWidth);
    }
    return encoded;
}

/**
 * Writes the whole body through one writer.
 * @param writer Real or measuring writer positioned at the first bit.
 * @param snapshot Message input.
 * @return True when every field fit.
 */
[[nodiscard]] bool write_body(bits::Writer& writer, const Snapshot& snapshot) noexcept {
    if(snapshot.strike_bond.campaign && snapshot.strike_bond.endingFlow.retire) return garden_ending::write(writer,snapshot);
    // The hardwipe token is unchecked unless the client's `use_hardwipe_tokens` config is on.
    bool encoded = writer.write(0, kHardwipeWidth)
                   && writer.write(snapshot.patchEpoch.first, kEpochWidth)
                   && writer.write(snapshot.patchEpoch.second, kEpochWidth)
                   && writer.write(snapshot.hasGrant ? 1U : 0U, kPresenceWidth);
    if (encoded && snapshot.hasGrant) {
        encoded = legacy_write_bubble_block(writer, snapshot.grant);
    }
    const std::size_t latchBit = kLatchBitWithoutGrant + (snapshot.hasGrant ? kBubbleBlockBits : 0);
    // Both mission and native service clocks use the same little-endian field.
    const auto elapsedTicks=snapshot.activityClock?snapshot.activityElapsedTicks:snapshot.gameplayClockTicks;
    encoded = encoded && native::activity_clock::write_elapsed(writer, elapsedTicks) && writer.bit_count() == latchBit;
    // The enable latch is not sticky, so it goes on every message.
    encoded = encoded && writer.write(1, kPresenceWidth)
              && legacy_write_roster_delta(writer, snapshot.roster, snapshot.stateSequence)
              && writer.bit_count()
                     == latchBit + 1
                            + delta_bits(top_level_key_count(snapshot.roster),
                                         snapshot.roster.bubbleSubBlocks);
    // Once the client has acknowledged type 18, even reapplying unrelated authority objects is
    // destructive: the native publish pass copies their neutral route input over the live script
    // phase. Keep publishing the phase-1 roster delta, but leave the entire phase-2 object list
    // absent so the changed-object collector stays empty and native simulation state survives.
    if (encoded && !snapshot.phaseOneOnly) {
        if (!snapshot.preserveMissionAuthorityState
            || snapshot.omegaOpeningStage != kOmegaOpeningStageNone) {
            encoded = write_phase_two(writer, snapshot);
        } else {
            // The forest generator group is exempt from the suppression: its blocks are empty
            // ({reset=1, present=0}, no authority applied, nothing clobbered) and they are the
            // only way its bubble-11 sync objects ever SEED â€” without them
            // ClientRosterSync_AllRecordsInBubbleSeeded vetoes the bubble's seed commit and
            // the replicated map-generator worker never instantiates.
            encoded = write_omega_progression(writer, snapshot)
                      && write_forest_generator_group(writer, snapshot);
        }
    }
    // The entity-group loop end, then the trailing pair, which short-circuits to one bit.
    return encoded && writer.write(0, kPresenceWidth) && writer.write(0, kPresenceWidth);
}

} // namespace

/** Encodes one `sensor_auth_update` body. */
bool legacy_encode_sensor_auth_update(const Snapshot& snapshot,
                               std::span<std::byte> output,
                               std::size_t& written) noexcept {
    written = 0;
    if (output.empty() || !valid(snapshot)) {
        return false;
    }

    // Measure first. The writer clears and fills the caller's storage as it goes, so a body that
    // does not fit would leave a partial one behind.
    bits::Writer measure = bits::Writer::measuring();
    std::size_t required = 0;
    if (!write_body(measure, snapshot) || !measure.finish(required) || required > output.size()) {
        return false;
    }

    bits::Writer writer(output);
    std::size_t produced = 0;
    if (!write_body(writer, snapshot) || !writer.finish(produced) || produced != required) {
        return false;
    }
    written = produced;
    return true;
}

} // namespace dawn::middleware::bap::activity_message::sensor_auth_update
