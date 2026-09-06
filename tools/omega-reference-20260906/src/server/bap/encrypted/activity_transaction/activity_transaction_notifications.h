#pragma once

#include <array>
#include <cstddef>
#include <span>

#include "../activity_message/definition.h"
#include "../internal.h"
#include "../push/activity/activity_region_snapshot.h"

namespace sunrise::server::bap::encrypted::activity_transaction {

/** Three-way whole-bundle outcome; deferred is only exact advertisement provisioning. */
enum class NotificationStageResult : std::uint8_t {
    complete,
    deferred,
    failed,
};

#if defined(SUNRISE_REGION_PUBLICATION_TESTING)
/** Deterministic production-coordinator boundary failures used only by the linked test binary. */
enum class RegionStageFailurePoint : std::uint8_t {
    none,
    afterGlobalState,
    afterMembership,
    afterRoster,
};

void set_region_stage_failure_for_testing(RegionStageFailurePoint point) noexcept;
#endif

/** Region after-images and row pin retained across State commit and caller copy. */
struct NotificationStaging final {
    RegionTransitionSnapshot snapshot{};
    RegionPublicationDebt debt{};
    gameplay::group::HostActivityLineageLease advertisementLease{};
    gameplay::group::HostActivityLineageLease boundLineageLease{};
    NotificationStageResult result{NotificationStageResult::failed};
    bool regionBundle{};
};

/**
 * Stages the notifications one activity transaction requests.
 * @param session Connection-owned roster counters, advanced only by a staged roster.
 * @param scratch Lock-owned transform buffers.
 * @param activity Prepared activity transaction and delivery selection.
 * @param key Active AES-GCM session key.
 * @param nonce Local send nonce advanced only by complete staged notifications.
 * @param response Lock-owned complete-frame staging storage.
 * @param written Existing staged byte count, updated only by complete notifications.
 * @return True when every requested notification is staged.
 */
[[nodiscard]] NotificationStageResult stage_notifications(
    Session& session,
    Scratch& scratch,
    const activity_message::ActivityPlan& activity,
    std::span<const std::byte, state::kAesKeySize> key,
    std::array<std::byte, state::kBapNonceSize>& nonce,
    std::span<std::byte> response,
    std::size_t& written,
    NotificationStaging& staging) noexcept;

/** Stages one periodic refresh from its single copied State plan, with no live mutation. */
[[nodiscard]] NotificationStageResult stage_periodic_notifications(
    Session& session,
    Scratch& scratch,
    const state::activity::membership::PeriodicRegionRefresh& refresh,
    const state::activity::membership::PendingMutation& mutation,
    bool burst,
    bool rearmMissionDirector,
    std::span<const std::byte, state::kAesKeySize> key,
    std::array<std::byte, state::kBapNonceSize>& nonce,
    std::span<std::byte> response,
    std::size_t& written,
    NotificationStaging& staging) noexcept;

/** Releases the row pin and discards an uncommitted staged roster after any later failure. */
void discard_notification_staging(Session& session, NotificationStaging& staging) noexcept;

} // namespace sunrise::server::bap::encrypted::activity_transaction
