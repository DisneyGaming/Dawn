#include <array>

#include "../../../core/settings/settings.h"
#include "../../../middleware/content/packages/tables/roster_intersection.h"
#include "../../../middleware/content/packages/tables/scenario_reader.h"
#include "../../../middleware/content/packages/tables/slot_descriptor_reader.h"
#include "../../../state/build_data/scenarios/omega_schema_catalog.h"
#include "internal.h"

namespace dawn::client::content::scenarios {
namespace {

namespace tables = middleware::content::packages::tables;

/**
 * Exact mission_scot opening-object keys from the marker-working roster. These tags are unique to
 * Omega, so admitting them through the ordinary intersection cannot affect another destination.
 */
constexpr std::array<std::uint32_t, 4> kForcedAuthoredKeys = {
    0x82FB58B7U, 0xD00142CFU, 0xBA5F26EFU, 0xF7A6CE7FU};

[[nodiscard]] bool forced_authored_key(std::uint32_t key) noexcept {
    if (!core::settings::get().client.rosterForceAuthored) {
        return false;
    }
    for (const std::uint32_t forced : kForcedAuthoredKeys) {
        if (forced == key) {
            return true;
        }
    }
    return false;
}

constexpr std::size_t kChainDepthLimit = 8;
/** Leaves the old measured ordinary catalog available even if authored discovery grows. */
constexpr std::size_t kOrdinaryGroupReserve = 128;
static_assert(kOrdinaryGroupReserve < layouts::kRosterGroupCapacity);

bool collect_slot(void* context, const tables::SlotDescriptor& descriptor) noexcept {
    record_slot(*static_cast<RosterStorage*>(context), descriptor);
    return true;
}

[[nodiscard]] bool follow_handle(const reader::Source& source,
                                 reader::Scratch& scratch,
                                 RosterStorage& storage,
                                 std::uint32_t handle,
                                 std::uint32_t registryKey) noexcept {
    std::uint32_t tag = handle;
    for (std::size_t depth = 0; depth < kChainDepthLimit; ++depth) {
        std::uint32_t classId = 0;
        ++storage.reads;
        if (!reader::read_tag(source, scratch, tag, storage.chain, classId)) {
            // ObjectBubble handles also name host-only objects. Those tags are not guaranteed to
            // materialize in the client package reader, and the working traversal treated an
            // unread handle as a non-descriptor rather than rejecting its whole roster group.
            return true;
        }
        if (classId == tables::kPlacedObjectClass) {
            return tables::visit_slot_descriptors(
                storage.chain, tag, registryKey, &collect_slot, &storage);
        }
        if (classId != tables::kSlotIndirectClass && classId != tables::kSlotRedirectClass) {
            // A handle may terminate in a host-only/non-descriptor object. That is a successful
            // walk with no client slot, not evidence that a package read failed.
            return true;
        }
        std::uint32_t next = 0;
        if (!tables::next_descriptor_tag(storage.chain, classId, next)) {
            return true;
        }
        tag = next;
    }
    // A chain that does not reach a client descriptor contributes no client slot. Other handles
    // on the same object can still provide the complete descriptor-backed layout.
    return true;
}

[[nodiscard]] bool collect_descriptors(const reader::Source& source,
                                       reader::Scratch& scratch,
                                       RosterStorage& storage,
                                       std::span<const std::byte> objectBlob,
                                       std::uint32_t registryKey) noexcept {
    tables::Array bubbles{};
    if (!tables::object_bubbles(objectBlob, bubbles)) {
        return true;
    }
    for (std::uint64_t index = 0; index < bubbles.count; ++index) {
        tables::ObjectBubble bubble{};
        if (!tables::object_bubble_at(objectBlob, bubbles, index, bubble)) {
            return false;
        }
        for (std::uint64_t slot = 0; slot < bubble.handleCount; ++slot) {
            std::uint32_t handle = 0;
            if (!tables::object_placed_handle_at(objectBlob, bubble, slot, handle)) {
                return false;
            }
            if (!follow_handle(source, scratch, storage, handle, registryKey)) {
                return false;
            }
        }
    }
    return true;
}

/** @return Explicit non-global ObjectBubble indices declared by one object. */
[[nodiscard]] bool explicit_slice_mask(std::span<const std::byte> objectBlob,
                                       std::uint64_t& mask) noexcept {
    mask = 0;
    tables::Array bubbles{};
    if (!tables::object_bubbles(objectBlob, bubbles)) {
        // ObjectBubble is optional. Ordinary roster objects commonly omit the field entirely;
        // absence means the object declares no explicit slice, not that its package read failed.
        return true;
    }
    for (std::uint64_t index = 0; index < bubbles.count; ++index) {
        tables::ObjectBubble bubble{};
        if (!tables::object_bubble_at(objectBlob, bubbles, index, bubble)) {
            return false;
        }
        if (bubble.bubbleIndex >= 0
            && static_cast<std::uint64_t>(bubble.bubbleIndex) < layouts::kBubbleCapacity) {
            mask |= std::uint64_t{1} << static_cast<std::uint32_t>(bubble.bubbleIndex);
        }
    }
    return true;
}

void classify(const ObjectMemo& memo,
              std::uint32_t objectTag,
              const ResolveContext& context,
              bool& scenarioRoot,
              bool& selectedLocal) noexcept {
    // The measured Omega groups predate the authored-overlay model and must retain their proven
    // destination intersection/mask. Do not publish a second authored-overlay copy of them.
    if (forced_authored_key(memo.registryKey)) {
        scenarioRoot = false;
        selectedLocal = false;
        return;
    }
    scenarioRoot = context.registry == 0 && context.scenarioHash != 0
                   && memo.registryKey == context.scenarioHash;
    selectedLocal =
        context.registry == 2
        && tables::package_of(objectTag) == context.scenarioPackage
        && (memo.explicitSliceMask & (std::uint64_t{1} << context.slice)) != 0;
}

[[nodiscard]] bool potentially_authored(const ResolveContext& context,
                                        std::uint32_t objectTag) noexcept {
    return context.registry == 0
           || (context.registry == 2
               && tables::package_of(objectTag) == context.scenarioPackage);
}

void reject_authored_read(RosterStorage& storage,
                          const ResolveContext& context,
                          std::uint32_t objectTag,
                          ResolvedObject& output) noexcept {
    if (potentially_authored(context, objectTag)) {
        output.authoredRejected = true;
        ++storage.authoredReadFailures;
    }
}

[[nodiscard]] std::size_t memo_slot(const RosterStorage& storage, std::uint32_t tag) noexcept {
    std::size_t probe = tag % kObjectMemoCapacity;
    for (std::size_t step = 0; step < kObjectMemoCapacity; ++step) {
        if (storage.memo[probe].tag == 0 || storage.memo[probe].tag == tag) {
            return probe;
        }
        probe = (probe + 1) % kObjectMemoCapacity;
    }
    return kObjectMemoCapacity;
}

} // namespace

std::uint8_t measured_omega_key_bit(std::uint32_t key) noexcept {
    if (!core::settings::get().client.rosterForceAuthored) {
        return 0;
    }
    for (std::size_t index = 0; index < kForcedAuthoredKeys.size(); ++index) {
        if (kForcedAuthoredKeys[index] == key) {
            return static_cast<std::uint8_t>(1U << index);
        }
    }
    return 0;
}

bool resolve_object(const reader::Source& source,
                    reader::Scratch& scratch,
                    RosterStorage& storage,
                    std::uint32_t objectTag,
                    const ResolveContext& context,
                    ResolvedObject& output) noexcept {
    output = {};
    const std::size_t slot = memo_slot(storage, objectTag);
    if (slot == kObjectMemoCapacity) {
        return false;
    }
    ObjectMemo& memo = storage.memo[slot];
    bool objectLoaded = false;
    if (memo.tag != objectTag) {
        ObjectMemo inspected{};
        inspected.tag = objectTag;
        inspected.group = kNotARosterGroup;
        ++storage.reads;
        if (!reader::read_tag(source, scratch, objectTag, storage.object)) {
            reject_authored_read(storage, context, objectTag, output);
            return true;
        }
        objectLoaded = true;
        if (!tables::object_key(storage.object, inspected.registryKey)
            || inspected.registryKey == 0) {
            reject_authored_read(storage, context, objectTag, output);
            return true;
        }
        inspected.carriesRosterSlot = tables::carries_roster_slot(storage.object)
                                       || forced_authored_key(inspected.registryKey);
        if (!explicit_slice_mask(storage.object, inspected.explicitSliceMask)) {
            reject_authored_read(storage, context, objectTag, output);
            return true;
        }
        memo = inspected;
    }

    bool scenarioRoot = false;
    bool selectedLocal = false;
    classify(memo, objectTag, context, scenarioRoot, selectedLocal);
    if (memo.group != kNotARosterGroup) {
        output.group = memo.group;
        output.ordinary = memo.carriesRosterSlot && memo.completeLayout;
        output.scenarioRoot = scenarioRoot;
        output.selectedLocal = selectedLocal;
        return true;
    }
    if (!memo.carriesRosterSlot && !scenarioRoot && !selectedLocal) {
        return true;
    }
    if (!objectLoaded) {
        ++storage.reads;
        if (!reader::read_tag(source, scratch, objectTag, storage.object)) {
            if (scenarioRoot || selectedLocal) {
                output.authoredRejected = true;
                ++storage.authoredReadFailures;
            }
            return true;
        }
    }

    tables::Array declared{};
    if (!tables::object_slots(storage.object, declared) || declared.count == 0) {
        return true;
    }
    if (declared.count > layouts::kRosterSlotCapacity) {
        ++storage.unresolvedGroups;
        if (scenarioRoot || selectedLocal) {
            output.authoredRejected = true;
        }
        return true;
    }
    layouts::RosterGroup candidate{};
    candidate.registryKey = memo.registryKey;
    storage.slotCount = 0;
    storage.slotsOverflowed = false;
    const bool descriptorsComplete =
        collect_descriptors(source, scratch, storage, storage.object, candidate.registryKey);
    const bool allowPartial =
        scenarioRoot || selectedLocal || forced_authored_key(memo.registryKey);
    if (!descriptorsComplete) {
        ++storage.unresolvedGroups;
        if (allowPartial) {
            output.authoredRejected = true;
            ++storage.authoredReadFailures;
        }
        return true;
    }
    if (!fill_slots(storage, storage.object, declared, candidate, allowPartial)) {
        ++storage.unresolvedGroups;
        if (allowPartial && (storage.slotsOverflowed || storage.slotCount != 0)) {
            output.authoredRejected = true;
        }
        return true;
    }
    // Descriptor-backed slots intentionally omit host-only declarations. Requiring the two counts
    // to match rejects every installed ordinary group even when all client descriptors resolved.
    const bool completeLayout = true;
    candidate.objectTag = objectTag;
    for (std::size_t index = 0; index < storage.slotCount; ++index) {
        const SlotRecord& slotRecord = storage.slots[index];
        state::build_data::scenarios::record_omega_schema({candidate.registryKey,
                                                            candidate.objectTag,
                                                            slotRecord.componentClass,
                                                            slotRecord.senseSchema,
                                                            slotRecord.authSchema,
                                                            slotRecord.index,
                                                            slotRecord.type});
    }
    for (std::size_t index = 0; index < storage.groupCount; ++index) {
        if (same_group_layout(storage.groups[index], candidate)) {
            memo.group = static_cast<std::uint16_t>(index);
            memo.completeLayout = completeLayout;
            output.group = memo.group;
            output.ordinary = memo.carriesRosterSlot && completeLayout;
            output.scenarioRoot = scenarioRoot;
            output.selectedLocal = selectedLocal;
            return true;
        }
    }
    const bool ordinaryLayout = memo.carriesRosterSlot && completeLayout;
    const std::size_t authoredLimit = layouts::kRosterGroupCapacity - kOrdinaryGroupReserve;
    if (storage.groupCount == layouts::kRosterGroupCapacity
        || (!ordinaryLayout && storage.groupCount >= authoredLimit)) {
        if (scenarioRoot || selectedLocal) {
            output.authoredRejected = true;
            ++storage.catalogOverflows;
        }
        return true;
    }
    storage.groups[storage.groupCount] = candidate;
    memo.group = static_cast<std::uint16_t>(storage.groupCount);
    memo.completeLayout = completeLayout;
    output.group = memo.group;
    output.ordinary = ordinaryLayout;
    output.scenarioRoot = scenarioRoot;
    output.selectedLocal = selectedLocal;
    ++storage.groupCount;
    return true;
}

} // namespace dawn::client::content::scenarios
