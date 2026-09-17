#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "membership_transit_service.h"

namespace dawn::server::runtime::activity::native_activity_transit {

using Owner = membership_transit::Owner;
using Destination = membership_transit::Destination;
namespace membership = state::activity::membership;

/** The bounded result for one requested cohort. */
struct Status final {
    bool bound{};
    bool requested{};
    bool arrived{};
    bool released{};
    std::size_t members{};
};

/** Binds one activity incarnation to copied authored destinations. */
[[nodiscard]] bool bind(Owner owner, std::uint64_t boot,
                        std::span<const Destination> destinations) noexcept;

/** Retires one exact owner/boot binding. */
void release(Owner owner, std::uint64_t boot) noexcept;

/** Requests one destination for every member observed before this cohort was frozen. */
[[nodiscard]] bool request_all(Owner owner, std::uint64_t boot,
                               std::uint64_t cohortId,
                               std::uint32_t destinationId) noexcept;

/** Recovers a defeated cohort at an authored spawn in its current region. */
[[nodiscard]] bool request_respawn_all(Owner owner,std::uint64_t boot,
    std::uint64_t cohortId,std::uint32_t destinationId) noexcept;
/** Projects the native spawn handshake; a teleport cannot revive a dead member. */
[[nodiscard]] bool project_respawn(Owner owner,std::uint64_t memberKey,
    membership::SpawnState local,std::int32_t actualRegion,membership::SpawnState& output) noexcept;

/** Reports the retained state of one frozen cohort. */
[[nodiscard]] Status snapshot(Owner owner, std::uint64_t boot,
                              std::uint64_t cohortId) noexcept;

/** Retains the accepted cohort destination for native spawning and respawning. */
[[nodiscard]] bool spawn_destination(Owner owner, Destination& output) noexcept;
/** Records the authored destination for respawn only; it never freezes members or requests travel. */
[[nodiscard]] bool set_respawn_destination(Owner owner,std::uint64_t boot,
                                           std::uint32_t destinationId) noexcept;

/**
 * True when this exact binding carries the authored destination id. This is the validation half of
 * set_respawn_destination with none of its side effects, so a caller that must not latch yet can
 * still keep an unknown destination id fail-closed.
 */
[[nodiscard]] bool destination_bound(Owner owner,std::uint64_t boot,
                                     std::uint32_t destinationId) noexcept;

/**
 * Drops any retained respawn destination for this exact binding. Without it the previous round's
 * destination stays authoritative for the whole of the next round, because the only other clear is
 * the whole-slot wipe in release() at activity teardown.
 */
void clear_respawn_destination(Owner owner,std::uint64_t boot) noexcept;

/** True while this owner retains a latched respawn destination. Read-only, for diagnostics. */
[[nodiscard]] bool respawn_latched(Owner owner) noexcept;
[[nodiscard]] std::size_t observed_member_count(Owner owner,std::uint64_t boot) noexcept;

/**
 * Projects and, when present, observes one raw native membership tuple. Missing receipt or region
 * data is deliberately read-only. Arrival is accepted only by the wrapped service's exact tuple
 * and region checks.
 */
[[nodiscard]] membership_transit::Projection project(
    Owner owner, std::uint64_t memberKey, bool hasTeleportReceipt,
    membership::TeleportState local, std::int32_t actualRegion) noexcept;

/** Returns whether the owner needs a membership publication at the caller's supplied time. */
[[nodiscard]] bool membership_due(Owner owner, std::uint64_t now) noexcept;

/** Records that a complete membership frame was delivered for this owner. */
void note_membership_published(Owner owner, std::uint64_t now) noexcept;

} // namespace dawn::server::runtime::activity::native_activity_transit
