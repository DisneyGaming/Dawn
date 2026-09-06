#pragma once

#include "../../../../state/activity/lifecycle_generation.h"
#include "../../region_lineage.h"

namespace sunrise::server::bap::encrypted::transactions {

/** Connection fields published only after State commits and caller output is copied. */
struct Publication {
    state::activity::ActivityInstanceKey activity{};
    /** Exact BAP-owned predecessor atomically overwritten by an allocation, or absent. */
    state::activity::ActivityInstanceKey atomicReplacement{};
    RegionLineage regionLineage{};
    bool hasActivityBinding{};
    /** True when the binding came from a join rather than from this link's own allocation. */
    bool activityBindingFromJoin{};
    /** True only for a service-6 allocation that creates a BAP owner lease. */
    bool activityBindingCreatedByBap{};
    /** Set only after the delayed allocation has committed in State. */
    bool activityAllocationCommitted{};
    bool hasRegionLineage{};

    friend constexpr bool operator==(Publication, Publication) noexcept = default;
};

/** @return The exact fresh allocation still needing rollback, or an absent key. */
[[nodiscard]] inline state::activity::ActivityInstanceKey rollback_activity(
    const Publication& publication) noexcept {
    return publication.activityAllocationCommitted
                   && publication.activityBindingCreatedByBap
                   && !static_cast<bool>(publication.atomicReplacement)
               ? publication.activity
               : state::activity::ActivityInstanceKey{};
}

} // namespace sunrise::server::bap::encrypted::transactions
