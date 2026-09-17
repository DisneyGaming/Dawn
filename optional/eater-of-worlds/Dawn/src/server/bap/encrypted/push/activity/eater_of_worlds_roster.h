#pragma once

#include <algorithm>
#include <array>
#include <span>
#include <string_view>

#include "../../../../../middleware/bap/activity_message/sensor_auth_update.h"
#include "../../../../../state/activity/eater_of_worlds/catalog.h"
#include "../../../../../state/build_data/scenarios/definition.h"

namespace dawn::server::bap::encrypted::push::activity::eater_of_worlds_roster {
namespace native = state::activity::eater_of_worlds;
namespace layouts = state::build_data::scenarios;
namespace wire = middleware::bap::activity_message::sensor_auth_update;

/** Projects an admitted packed region (bubble ordinal << 3) to lifetime authority+C.
 * Eater publishes exactly eight authored bubbles at packed regions 0,8,...,56. */
[[nodiscard]] constexpr std::optional<std::uint32_t>
lifetime_scenario(std::int32_t packedRegion) noexcept {
    if (packedRegion < 0 || packedRegion > 63 || (packedRegion & 7) != 0) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(packedRegion) >> 3;
}

inline constexpr bool top_level(const native::Group& group) noexcept {
    // These four groups belong to every one of the scenario's eight bubbles. They carry the
    // mission/player/global roots; the remaining groups retain their authored bubble ownership.
    return group.bubblesMask == 0xFFU;
}

static_assert(std::size(native::kGroups) <= wire::kGroupCapacity);

/** Client-descriptor storage recovered from the installed Eater packages.
 * Host-only geometry is omitted while every authored client slot index is preserved. */
inline constexpr auto kRosterGroups = [] {
    std::array<layouts::RosterGroup, std::size(native::kGroups)> result{};
    for (std::size_t groupIndex = 0; groupIndex < result.size(); ++groupIndex) {
        const auto& source = native::kGroups[groupIndex];
        auto& target = result[groupIndex];
        target.registryKey = source.key;
        target.objectTag = source.tag;
        for (const auto& slot : source.slots) {
            // Host-only declarations have no client descriptor and cannot appear in a roster.
            if (slot.tag == 0xFFFFFFFFU) { continue; }
            const auto slotIndex = target.slotCount++;
            target.slotTypes[slotIndex] = slot.type;
            target.slotFlags[slotIndex] = slot.flags;
            target.slotIndices[slotIndex] = slot.index;
            target.descriptorTags[slotIndex] = slot.tag;
            target.descriptorOffsets[slotIndex] = slot.offset;
            target.componentClasses[slotIndex] = slot.component;
            target.senseSchemas[slotIndex] = slot.sense;
            target.authSchemas[slotIndex] = slot.auth;
        }
    }
    return result;
}();

static_assert([] {
    for (std::size_t index = 0; index < std::size(native::kGroups); ++index) {
        const auto& source = native::kGroups[index];
        const auto& target = kRosterGroups[index];
        std::size_t clientSlots{};
        for (const auto& slot : source.slots) { clientSlots += slot.tag != 0xFFFFFFFFU ? 1U : 0U; }
        if (clientSlots > layouts::kRosterSlotCapacity || target.slotCount != clientSlots
            || target.registryKey != source.key
            || target.objectTag != source.tag) { return false; }
    }
    return true;
}());

/**
 * Builds Eater's complete roster from byte-verified package records. The scenario cache omitted
 * the encounter package groups, so admission must not depend on an all-cache lookup succeeding.
 * The immutable group storage above owns every span; caller scratch owns only sub-block key lists.
 */
template<class Storage>
[[nodiscard]] bool admit(const layouts::Definition& layout, Storage& storage,
                         wire::Roster& roster) noexcept {
    if (layout.tag != native::kScenario || layout.nameLength > layout.name.size()
        || std::string_view(layout.name.data(), layout.nameLength) != native::kPackage
        || layout.bubbleCount != 8 || std::size(native::kGroups) > roster.groups.size()) {
        return false;
    }

    wire::Roster candidate{};
    std::size_t output{};
    const auto append = [&](std::size_t catalogIndex) {
        const auto& group = kRosterGroups[catalogIndex];
        if(group.slotCount==0) { return; }
        candidate.groups[output++] = {group.registryKey,
            std::span(group.slotTypes).first(group.slotCount),
            std::span(group.slotFlags).first(group.slotCount),
            std::span(group.slotIndices).first(group.slotCount)};
    };
    for (std::size_t i = 0; i < std::size(native::kGroups); ++i) {
        if (top_level(native::kGroups[i])) { append(i); }
    }
    candidate.topLevelGroupCount = output;
    for (std::size_t i = 0; i < std::size(native::kGroups); ++i) {
        if (!top_level(native::kGroups[i])) { append(i); }
    }
    candidate.groupCount = output;

    for (std::size_t i = 0; i < candidate.topLevelGroupCount; ++i) {
        const auto types = candidate.groups[i].slotTypes;
        if (std::find(types.begin(), types.end(), std::uint8_t{13}) != types.end()) {
            if (candidate.playerKeyGroup != 0) { return false; }
            candidate.playerKeyGroup = candidate.groups[i].key;
        }
    }
    if (candidate.playerKeyGroup == 0) { return false; }

    std::size_t blockCount{};
    for (std::uint8_t bubble = 0; bubble < layout.bubbleCount; ++bubble) {
        if (blockCount >= storage.rosterSubBlockKeys.size()
            || blockCount >= storage.rosterSubBlocks.size()) { return false; }
        auto& keys = storage.rosterSubBlockKeys[blockCount];
        std::size_t keyCount{};
        for (const auto& group : native::kGroups) {
            if (!top_level(group) && kRosterGroups[&group-std::data(native::kGroups)].slotCount!=0
                && (group.bubblesMask & (1U << bubble)) != 0) {
                if (keyCount == keys.size()) { return false; }
                keys[keyCount++] = group.key;
            }
        }
        if (keyCount == 0) { continue; }
        storage.rosterSubBlocks[blockCount] = {bubble, std::span(keys).first(keyCount)};
        ++blockCount;
    }
    candidate.bubbleSubBlocks = std::span(storage.rosterSubBlocks).first(blockCount);
    roster = candidate;
    return true;
}

} // namespace dawn::server::bap::encrypted::push::activity::eater_of_worlds_roster
