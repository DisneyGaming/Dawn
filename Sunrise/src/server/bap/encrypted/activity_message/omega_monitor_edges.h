#pragma once

#include "../../../../middleware/bap/activity_message/sense_update.h"

namespace sunrise::server::bap::encrypted::activity_message::omega_monitor_edges {
namespace sense = middleware::bap::activity_message::sense_update;

/** Measured Omega entered report; authentication/readiness and one-shot latching
 * remain the caller's responsibility. */
[[nodiscard]] inline bool entered(const sense::SenseUpdate& update,
                                  std::uint16_t slotIndex) noexcept {
    if (update.objectCount > update.objects.size()) { return false; }
    bool result = false;
    for (std::size_t index = 0; index < update.objectCount; ++index) {
        const auto& object = update.objects[index];
        if (object.registryKey != 0xD00142CFU || object.slotType != 30
            || object.slotIndex != slotIndex) { continue; }
        // A valid report can coalesce other objects/groups with this monitor.
        // Retain the exact measured 67-bit entered payload, but separate its
        // raw 32-bit revision (4D8490) from type-30 schema 80809531. The retained
        // payload is any/all occupied, selected count 1, authority token 0;
        // native B1FC00 confirms these fields independently of the revision.
        // bodySecond contains the final three payload bits then that revision.
        // If a monitor occurs twice, its last reported state wins.
        result = object.hasDelta && object.bodyBits == 99
            && object.bodyFirst == 0xF000000030000000ULL
            && object.bodySecond == object.revision && object.bodyThird == 0;
    }
    return result;
}
} // namespace sunrise::server::bap::encrypted::activity_message::omega_monitor_edges
