#pragma once

#include "../../../state/activity/coo/open_world_catalog.h"
#include "../../../middleware/bap/activity_message/native/population_authority.h"

namespace sunrise::server::runtime::activity::open_world {

namespace authored=state::activity::coo::open_world;

/** Shared runtime marker for package-derived destination free-roam profiles. */
struct Definition final {
    const authored::Destination* authored{};
};

[[nodiscard]] constexpr bool valid(const Definition& definition) noexcept {
    const auto* value=definition.authored;
    return value && !value->activity.empty() && value->scenario && value->primaryBubble<64
        && !value->registries.empty() && !value->populations.empty()
        && value->populations.size()
            <=middleware::bap::activity_message::native::population::kSourceCapacity
        && value->placements.size()<=32;
}

} // namespace sunrise::server::runtime::activity::open_world
