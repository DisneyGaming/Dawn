#pragma once

#include "../../../internal.h"
#include "../../../region_lineage.h"
#include "../../activity_message/definition.h"

namespace sunrise::server::bap::encrypted::push::activity {

/** Immutable snapshot finalization outcome. */
enum class RegionSnapshotBuildResult : std::uint8_t {
    ready,
    pending,
    stale,
    failed,
};

/** Finalizes one authoritative/refresh semantic plan without mutating live delivery state. */
[[nodiscard]] RegionSnapshotBuildResult build_region_transition_snapshot(
    const Session& session,
    Scratch& scratch,
    const activity_message::ActivityPlan& plan,
    RegionTransitionSnapshot& output,
    RegionPublicationDebt& debt,
    gameplay::group::HostActivityLineageLease& advertisementLease,
    gameplay::group::HostActivityLineageLease& boundLineageLease) noexcept;

/** Finalizes one periodic plan from State inputs captured with its commit mutation. */
[[nodiscard]] RegionSnapshotBuildResult build_periodic_region_snapshot(
    const Session& session,
    Scratch& scratch,
    const state::activity::membership::PeriodicRegionRefresh& refresh,
    const state::activity::membership::PendingMutation& mutation,
    bool burst,
    bool rearmMissionDirector,
    RegionTransitionSnapshot& output,
    gameplay::group::HostActivityLineageLease& advertisementLease,
    gameplay::group::HostActivityLineageLease& boundLineageLease) noexcept;

/** Rebuilds one whole retry bundle from an exact committed debt. */
[[nodiscard]] RegionSnapshotBuildResult build_region_debt_snapshot(
    const Session& session,
    Scratch& scratch,
    const RegionPublicationDebt& debt,
    RegionTransitionSnapshot& output,
    gameplay::group::HostActivityLineageLease& advertisementLease,
    gameplay::group::HostActivityLineageLease& boundLineageLease) noexcept;

} // namespace sunrise::server::bap::encrypted::push::activity
