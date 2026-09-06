#pragma once

#include <cstddef>
#include <cstdint>

#include "definition.h"

namespace sunrise::state::build_data::scenarios {

/**
 * Registry key of the global registry group (object 80FEB3DC, the launch references).
 * The client's activity table carries this registry for every activity launch, so it is the one
 * key that is provably present in every slice set of every destination.
 */
inline constexpr std::uint32_t kGlobalParticipationKey = 0x4786C0E0U;
/** Table index the global group has held in every observed extraction: it is walked first. */
inline constexpr std::uint16_t kGlobalParticipationIndexHint = 0;
/** How far from the hint the fallback searches before refusing to guess. */
inline constexpr std::uint16_t kGlobalParticipationSearchSpan = 64;
/** Slot type that binds the local player, which a publishable roster must carry. */
inline constexpr std::uint8_t kFallbackParticipationSlotType = 13;

/** @param group Candidate roster group. @return True when one slot binds a player. */
[[nodiscard]] inline bool binds_participation(const RosterGroup& group) noexcept {
    for (std::size_t slot = 0; slot < group.slotCount; ++slot) {
        if (group.slotTypes[slot] == kFallbackParticipationSlotType) {
            return true;
        }
    }
    return false;
}

/**
 * Publishes the global participation group on a destination row that extracted no top-level
 * groups. A free-roam scenario reaches more candidate groups than the fixed intersection and
 * per-destination capacities hold, so the conservative extractor publishes an empty row rather
 * than an unprovable one. An empty row must still produce a publishable roster: the roster
 * rejection otherwise vetoes the host's whole periodic bundle and the client starves into the
 * activity-host timeout. The recorded pre-intersection Mercury free-roam session pushed exactly
 * one top-level group, this one, and the client loaded and played.
 * @param layout Destination row, amended only when it has no top-level groups.
 * @param find Roster-table lookup: bool(std::size_t index, RosterGroup& out).
 * @return True when the fallback group was published onto the row.
 */
template <typename FindGroup>
[[nodiscard]] inline bool amend_participation_fallback(Definition& layout,
                                                       FindGroup&& find) noexcept {
    if (layout.rosterGroupCount != 0 || layout.rosterGroups.size() == 0) {
        return false;
    }
    RosterGroup group{};
    std::uint16_t resolved = 0;
    bool found = false;
    for (std::uint16_t delta = 0; delta <= kGlobalParticipationSearchSpan && !found; ++delta) {
        const std::uint32_t candidate =
            static_cast<std::uint32_t>(kGlobalParticipationIndexHint) + delta;
        if (find(static_cast<std::size_t>(candidate), group)
            && group.registryKey == kGlobalParticipationKey && binds_participation(group)) {
            resolved = static_cast<std::uint16_t>(candidate);
            found = true;
        }
    }
    if (!found) {
        return false;
    }
    // The roster snapshot rejects duplicate registry keys, so a bubble-local group already
    // carrying the global key vetoes the amendment rather than poisoning the row.
    for (std::size_t index = 0; index < layout.bubbleGroupCount; ++index) {
        if (layout.bubbleGroups[index] == resolved) {
            return false;
        }
        RosterGroup bubbleGroup{};
        if (find(static_cast<std::size_t>(layout.bubbleGroups[index]), bubbleGroup)
            && bubbleGroup.registryKey == kGlobalParticipationKey) {
            return false;
        }
    }
    layout.rosterGroups[0] = resolved;
    layout.rosterGroupCount = 1;
    return true;
}

} // namespace sunrise::state::build_data::scenarios
