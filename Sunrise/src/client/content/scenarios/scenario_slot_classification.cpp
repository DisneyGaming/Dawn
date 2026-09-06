/**
 * Turns the descriptors one placed object declares into the slots activity message 5 publishes.
 * Package traversal owns discovery; this file owns the validated wire-facing classification.
 */

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>

#include "../../../core/logging/log.h"
#include "../../hooks/retail_log/retail_log_enqueue_observer.h"
#include "internal.h"

namespace sunrise::client::content::scenarios {
namespace {

namespace tables = middleware::content::packages::tables;

/** Logs representative schemas once while retaining the current tree's diagnostic coverage. */
void report_schema(const tables::SlotDescriptor& descriptor) noexcept {
    const bool diagnosticType = descriptor.slotType == 13 || descriptor.slotType == 16
                                || descriptor.slotType == 17 || descriptor.slotType == 18
                                || descriptor.slotType == 19 || descriptor.slotType == 35
                                || descriptor.slotType == 41;
    if (!diagnosticType || descriptor.slotType > layouts::kMaximumSlotType) {
        return;
    }
    static std::array<std::atomic_bool, layouts::kMaximumSlotType + 1> reported{};
    bool expected = false;
    if (!reported[descriptor.slotType].compare_exchange_strong(expected, true)) {
        return;
    }
    client::hooks::retail_log::register_schema_marker(descriptor.componentClass);
    client::hooks::retail_log::register_schema_marker(descriptor.senseSchema);
    client::hooks::retail_log::register_schema_marker(descriptor.authSchema);
    std::array<char, core::log::kLineCapacity> line{};
    const int written = std::snprintf(line.data(),
                                      line.size(),
                                      "ev=scenario stage=slot_schema type=%u component=0x%08X "
                                      "sense=0x%08X auth=0x%08X",
                                      static_cast<unsigned>(descriptor.slotType),
                                      descriptor.componentClass,
                                      descriptor.senseSchema,
                                      descriptor.authSchema);
    if (written > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(written)});
    }
}

} // namespace

/** Records one descriptor as a slot of the object being resolved. */
void record_slot(RosterStorage& storage, const tables::SlotDescriptor& descriptor) noexcept {
    if (descriptor.slotType == 0 || descriptor.slotType > layouts::kMaximumSlotType
        || descriptor.slotIndex >= layouts::kRosterSlotCapacity) {
        storage.slotsOverflowed = true;
        return;
    }
    report_schema(descriptor);
    for (std::size_t slot = 0; slot < storage.slotCount; ++slot) {
        if (storage.slots[slot].index == descriptor.slotIndex) {
            if (storage.slots[slot].type != descriptor.slotType
                || storage.slots[slot].flags
                       != slot_flags(descriptor.authSchema, descriptor.senseSchema)
                || storage.slots[slot].descriptorTag != descriptor.sourceTag
                || storage.slots[slot].descriptorOffset != descriptor.sourceOffset
                || storage.slots[slot].componentClass != descriptor.componentClass
                || storage.slots[slot].senseSchema != descriptor.senseSchema
                || storage.slots[slot].authSchema != descriptor.authSchema) {
                storage.slotsOverflowed = true;
            }
            return;
        }
    }
    if (storage.slotCount == storage.slots.size()) {
        storage.slotsOverflowed = true;
        return;
    }
    storage.slots[storage.slotCount] = {descriptor.slotIndex,
                                        static_cast<std::uint8_t>(descriptor.slotType),
                                        slot_flags(descriptor.authSchema, descriptor.senseSchema),
                                        descriptor.sourceTag,
                                        descriptor.sourceOffset,
                                        descriptor.componentClass,
                                        descriptor.senseSchema,
                                        descriptor.authSchema};
    ++storage.slotCount;
}

/** Fills one candidate group from the descriptors the walk found, in slot-index order. */
bool fill_slots(RosterStorage& storage,
                std::span<const std::byte> objectBlob,
                const tables::Array& declaredSlots,
                layouts::RosterGroup& group,
                bool /*allowPartial*/) noexcept {
    // Descriptor-backed layouts omit host-only slots for both ordinary and authored groups. The
    // real descriptor index below proves each retained slot against the object's declaration.
    const bool countOk = storage.slotCount <= declaredSlots.count;
    if (storage.slotsOverflowed || storage.slotCount == 0 || !countOk) {
        return false;
    }
    const auto last = storage.slots.begin() + static_cast<std::ptrdiff_t>(storage.slotCount);
    std::sort(storage.slots.begin(), last, [](const SlotRecord& first, const SlotRecord& second) {
        return first.index < second.index;
    });
    for (std::size_t slot = 0; slot < storage.slotCount; ++slot) {
        tables::Slot declared{};
        if (!tables::object_slot_at(
                objectBlob, declaredSlots, storage.slots[slot].index, declared)
            || declared.type != storage.slots[slot].type) {
            return false;
        }
        group.slotTypes[slot] = storage.slots[slot].type;
        group.slotFlags[slot] = storage.slots[slot].flags;
        group.slotIndices[slot] = storage.slots[slot].index;
        group.descriptorTags[slot] = storage.slots[slot].descriptorTag;
        group.descriptorOffsets[slot] = storage.slots[slot].descriptorOffset;
        group.componentClasses[slot] = storage.slots[slot].componentClass;
        group.senseSchemas[slot] = storage.slots[slot].senseSchema;
        group.authSchemas[slot] = storage.slots[slot].authSchema;
    }
    group.slotCount = static_cast<std::uint16_t>(storage.slotCount);
    return true;
}

} // namespace sunrise::client::content::scenarios
