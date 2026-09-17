#include "cue_graph_manifest.h"

namespace dawn::state::build_data::scenarios {
namespace {

void add_group(ManifestGroups& output, std::uint16_t index) noexcept {
    for (std::size_t ordinal = 0; ordinal < output.count; ++ordinal) {
        if (output.indices[ordinal] == index) {
            return;
        }
    }
    if (output.count == output.indices.size()) {
        output.overflowed = true;
        return;
    }
    output.indices[output.count++] = index;
}

[[nodiscard]] bool trigger_type(std::uint8_t type) noexcept {
    return type == 23U || type == 30U || type == 60U || type == 70U;
}

[[nodiscard]] bool action_type(std::uint8_t type) noexcept {
    return type == 1U || type == 11U || type == 18U || type == 35U || type == 43U
           || type == 53U || type == 68U;
}

[[nodiscard]] DescriptorLookup find_in_manifest(const ManifestGroups& manifest,
                                                std::span<const RosterGroup> groups,
                                                CueNodeId node) noexcept {
    SlotDescriptorMetadata found{};
    bool matched = false;
    for (std::size_t ordinal = 0; ordinal < manifest.count; ++ordinal) {
        const std::size_t groupIndex = manifest.indices[ordinal];
        if (groupIndex >= groups.size()) {
            continue;
        }
        const RosterGroup& group = groups[groupIndex];
        if (group.registryKey != node.registryKey) {
            continue;
        }
        for (std::size_t slot = 0; slot < group.slotCount; ++slot) {
            if (group.slotTypes[slot] != node.slotType
                || group.slotIndices[slot] != node.slotIndex) {
                continue;
            }
            SlotDescriptorMetadata candidate{};
            if (!descriptor_at(group, slot, candidate)) {
                continue;
            }
            if (matched
                && (found.objectTag != candidate.objectTag
                    || found.descriptorTag != candidate.descriptorTag
                    || found.descriptorOffset != candidate.descriptorOffset
                    || found.componentClass != candidate.componentClass
                    || found.senseSchema != candidate.senseSchema
                    || found.authSchema != candidate.authSchema)) {
                return DescriptorLookup::ambiguous;
            }
            found = candidate;
            matched = true;
        }
    }
    return matched ? DescriptorLookup::unique : DescriptorLookup::missing;
}

} // namespace

ManifestGroups manifest_groups(const Definition& definition) noexcept {
    ManifestGroups output{};
    for (std::size_t ordinal = 0; ordinal < definition.rosterGroupCount; ++ordinal) {
        add_group(output, definition.rosterGroups[ordinal]);
    }
    for (std::size_t ordinal = 0; ordinal < definition.bubbleGroupCount; ++ordinal) {
        add_group(output, definition.bubbleGroups[ordinal]);
    }
    for (std::size_t slice = 0; slice < definition.authoredGroups.size(); ++slice) {
        for (std::size_t ordinal = 0; ordinal < definition.authoredGroupCounts[slice]; ++ordinal) {
            add_group(output, definition.authoredGroups[slice][ordinal]);
        }
    }
    return output;
}

ManifestEdgeReport inspect_manifest_edges(const Definition& definition,
                                          std::span<const RosterGroup> groups) noexcept {
    ManifestEdgeReport output{};
    const ManifestGroups manifest = manifest_groups(definition);
    for (std::size_t ordinal = 0; ordinal < manifest.count; ++ordinal) {
        const std::size_t groupIndex = manifest.indices[ordinal];
        if (groupIndex >= groups.size()) {
            continue;
        }
        const RosterGroup& group = groups[groupIndex];
        std::size_t groupTriggers = 0;
        std::size_t groupActions = 0;
        output.structuralContainmentEdges += group.slotCount;
        for (std::size_t slot = 0; slot < group.slotCount; ++slot) {
            groupTriggers += trigger_type(group.slotTypes[slot]) ? 1U : 0U;
            groupActions += action_type(group.slotTypes[slot]) ? 1U : 0U;
        }
        output.triggerNodes += groupTriggers;
        output.actionNodes += groupActions;
        output.coResidentTriggerActionPairs += groupTriggers * groupActions;
    }
    // The currently decoded package records prove containment and schema identity only. They do
    // not expose a typed successor-reference field, so co-residence is deliberately not promoted
    // to a semantic edge.
    output.explicitSuccessorEdges = 0;
    output.openingSuccessorsRecoverable = false;
    return output;
}

ObservationMappingReport map_observations(
    const Definition& definition,
    std::span<const RosterGroup> groups,
    const middleware::bap::activity_message::sense_update::SenseUpdate& update) noexcept {
    ObservationMappingReport output{};
    const ManifestGroups manifest = manifest_groups(definition);
    output.observedObjects = update.objectCount;
    for (std::size_t ordinal = 0; ordinal < update.objectCount; ++ordinal) {
        const auto& object = update.objects[ordinal];
        const CueNodeId node{object.registryKey, object.slotIndex, object.slotType};
        const DescriptorLookup lookup = find_in_manifest(manifest, groups, node);
        if (lookup == DescriptorLookup::unique) {
            ++output.mappedObjects;
            continue;
        }
        if (lookup == DescriptorLookup::ambiguous) {
            ++output.ambiguousObjects;
        } else {
            ++output.missingObjects;
        }
        if (output.missingNodeCount < output.missingNodes.size()) {
            output.missingNodes[output.missingNodeCount++] = node;
        }
    }
    return output;
}

} // namespace dawn::state::build_data::scenarios
