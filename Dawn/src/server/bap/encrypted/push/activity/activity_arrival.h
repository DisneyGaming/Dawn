#pragma once

#include <cstdint>
#include <string_view>

#include "../../../../../state/activity/defaults/definition.h"
#include "../../../../../state/activity/destination/definition.h"
#include "../../../../../state/build_data/scenarios/definition.h"

namespace dawn::server::bap::encrypted::push::activity {

/**
 * Finds the slice-set index a destination arrives in.
 * Order: authored override, the bubble the client named, the default destination's, then its first
 * live one. A fallback never crosses destinations; a foreign index loads wrong geometry silently.
 *
 * @param defaults Authored default destination and its numeric launch policy.
 * @param selection Destination the session committed, carrying any wire arrival hash or override.
 * @param name Destination package name.
 * @param layout Extracted layout for that name, or a zero-bubble layout when there is none.
 * @return The slice-set index to publish.
 */
[[nodiscard]] std::uint16_t
arrival_slice_set(const state::activity::defaults::DefaultDestination& defaults,
                  const state::activity::destination::DestinationSelection& selection,
                  std::string_view name,
                  const state::build_data::scenarios::Definition& layout) noexcept;

/** The region one session publishes, with the arrival slice set behind it. */
struct EffectiveRegion final {
    /** Region index. The roster and the citizen advertisement both publish this value. */
    std::int32_t index{};
    /** The destination's own arrival slice set. The spawn override always names it. */
    std::uint16_t arrival{};
    /** True when the client reported the region, false when the arrival stood in. */
    bool reported{};
    /** True only when the exact activity destination remained current. */
    bool valid{};
};

/**
 * Purely resolves a region from already-copied destination and creator-source inputs.
 * Borrowed bindings pass allowArrival=false so a target arrival can never impersonate a report.
 */
[[nodiscard]] EffectiveRegion resolve_region(
    const state::activity::defaults::DefaultDestination& defaults,
    const state::activity::destination::DestinationSelection& selection,
    std::int32_t reportedRegion,
    bool allowArrival,
    std::string_view name,
    const state::build_data::scenarios::Definition& layout) noexcept;

} // namespace dawn::server::bap::encrypted::push::activity
