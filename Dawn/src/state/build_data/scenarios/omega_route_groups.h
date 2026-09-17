#pragma once

#include "definition.h"
#include "../../activity/omega/omega_progression.h"

namespace dawn::state::build_data::scenarios {

/** Descriptor-backed routing groups recovered from the pinned installed packages.
 * The ordinary cache omits E831A367. ActivityPoints are local content, not sync slots;
 * registering their owning group only needs its player-monitor and bubble-selector records. */
[[nodiscard]] inline bool pinned_omega_route_group(std::uint32_t key, RosterGroup& group) noexcept {
    group = {};
    const bool handoff = key == activity::omega::kHandoffGroup;
    const bool crown = key == activity::omega::kCrownGroup;
    if (!handoff && !crown) return false;
    group.registryKey = key;
    group.objectTag = handoff ? 0x80F47547U : 0x80F47B4BU;
    group.slotCount = 2;
    group.slotTypes[0] = 30;
    group.slotTypes[1] = 57;
    group.slotIndices[0] = 0;
    group.slotIndices[1] = handoff ? 3 : 7;
    group.slotFlags[0] = 3; // sense + auth schemas
    group.slotFlags[1] = 0; // local selector, no authority payload
    group.descriptorTags[0] = handoff ? 0x80F47541U : 0x80F47B45U;
    group.descriptorTags[1] = handoff ? 0x80F47544U : 0x80F47B48U;
    group.descriptorOffsets[0] = 0x218;
    group.descriptorOffsets[1] = 0x1F8;
    group.componentClasses[0] = 0x8080952F;
    group.componentClasses[1] = 0x808094D7;
    group.senseSchemas[0] = 0x80809531;
    group.authSchemas[0] = 0x80809532;
    group.senseSchemas[1] = group.authSchemas[1] = UINT32_MAX;
    return true;
}

} // namespace dawn::state::build_data::scenarios
