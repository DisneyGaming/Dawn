#pragma once

#include "../../client/network/consumer.h"
#include "../../state/activity/definition.h"
#include "region_lineage.h"

namespace dawn::server::bap {

/** One owner-validated, exact destination/region tuple for the diagnostic HUD. */
struct HudRegionSnapshot final {
    HudRegionAnchor lineage{};
    state::activity::destination::DestinationSelection destination{};
    std::int32_t reportedRegion{-1};
};

/** Copies one coherent current creator-owned HUD region view, or fails closed. */
[[nodiscard]] bool snapshot_hud_region(HudRegionSnapshot& output) noexcept;

/** Applies one connection-scoped BAP lifecycle event. */
[[nodiscard]] bool consume(const client::network::BapRequest& request,
                           client::network::BapResponse& response) noexcept;

/**
 * Serializes borrower invalidation and exact retirement of one group-owned record.
 * Group code calls this only after transferring the exact owner out of its table and dropping
 * its lock, or from the fenced source-cascade coordinator before that transfer is committed.
 */
[[nodiscard]] state::activity::RetireResult retire_group_owned_activity(
    state::activity::ActivityInstanceKey activity) noexcept;

/** Best-effort nonblocking retry used after a group lineage pin is released. */
void retry_pending_activity_retirements() noexcept;

/** Wipes every connection-owned nonce and transform buffer. */
void shutdown() noexcept;

} // namespace dawn::server::bap
