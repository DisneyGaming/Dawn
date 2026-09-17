#include "descriptor_catalog.h"

namespace dawn::state::build_data::scenarios {
namespace {

[[nodiscard]] bool same_node(CueNodeId left, CueNodeId right) noexcept {
    return left.registryKey == right.registryKey && left.slotType == right.slotType
           && left.slotIndex == right.slotIndex;
}

[[nodiscard]] bool same_descriptor(const SlotDescriptorMetadata& left,
                                   const SlotDescriptorMetadata& right) noexcept {
    return same_node(left.node, right.node) && left.objectTag == right.objectTag
           && left.descriptorTag == right.descriptorTag
           && left.descriptorOffset == right.descriptorOffset
           && left.componentClass == right.componentClass
           && left.senseSchema == right.senseSchema && left.authSchema == right.authSchema
           && left.flags == right.flags;
}

} // namespace

bool descriptor_at(const RosterGroup& group,
                   std::size_t ordinal,
                   SlotDescriptorMetadata& output) noexcept {
    output = {};
    if (ordinal >= group.slotCount) {
        return false;
    }
    output.node = {group.registryKey, group.slotIndices[ordinal], group.slotTypes[ordinal]};
    output.objectTag = group.objectTag;
    output.descriptorTag = group.descriptorTags[ordinal];
    output.descriptorOffset = group.descriptorOffsets[ordinal];
    output.componentClass = group.componentClasses[ordinal];
    output.senseSchema = group.senseSchemas[ordinal];
    output.authSchema = group.authSchemas[ordinal];
    output.flags = group.slotFlags[ordinal];
    return true;
}

DescriptorLookup find_descriptor(std::span<const RosterGroup> groups,
                                 CueNodeId node,
                                 SlotDescriptorMetadata& output) noexcept {
    output = {};
    bool found = false;
    for (const RosterGroup& group : groups) {
        if (group.registryKey != node.registryKey) {
            continue;
        }
        for (std::size_t ordinal = 0; ordinal < group.slotCount; ++ordinal) {
            if (group.slotTypes[ordinal] != node.slotType
                || group.slotIndices[ordinal] != node.slotIndex) {
                continue;
            }
            SlotDescriptorMetadata candidate{};
            if (!descriptor_at(group, ordinal, candidate)) {
                continue;
            }
            if (found && !same_descriptor(output, candidate)) {
                output = {};
                return DescriptorLookup::ambiguous;
            }
            output = candidate;
            found = true;
        }
    }
    return found ? DescriptorLookup::unique : DescriptorLookup::missing;
}

std::size_t descriptor_count(std::span<const RosterGroup> groups) noexcept {
    std::size_t count = 0;
    for (const RosterGroup& group : groups) {
        count += group.slotCount;
    }
    return count;
}

} // namespace dawn::state::build_data::scenarios
