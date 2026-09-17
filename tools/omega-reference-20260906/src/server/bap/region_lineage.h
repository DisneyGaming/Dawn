#pragma once

#include <cstdint>

#include "../../middleware/bap/activity_message/replicate_membership.h"
#include "../../middleware/bap/activity_message/sensor_auth_update.h"
#include "../../state/activity/membership/activity_membership_query.h"
#include "../gameplay/gameplay_advertisement.h"

namespace dawn::server::bap {

/** Why one binding may use an exact creator-root activity as its reported-region source. */
enum class RegionLineageKind : std::uint8_t {
    none,
    ownedActivity,
    groupDerivedBorrow,
};

/** Explicit delivery-scoped bound-target -> creator-root relationship. */
struct RegionLineage final {
    state::activity::ActivityInstanceKey bound{};
    state::activity::ActivityInstanceKey source{};
    RegionLineageKind kind{RegionLineageKind::none};

    [[nodiscard]] explicit constexpr operator bool() const noexcept {
        return static_cast<bool>(bound) && static_cast<bool>(source)
               && kind != RegionLineageKind::none;
    }

    friend constexpr bool operator==(RegionLineage, RegionLineage) noexcept = default;
};

/** Required notification components carried by one all-or-nothing region bundle. */
enum class RegionNotification : std::uint8_t {
    none = 0,
    globalState = 1U << 0U,
    membership = 1U << 1U,
    roster = 1U << 2U,
};

using NotificationMask = std::uint8_t;

[[nodiscard]] constexpr NotificationMask notification_mask(RegionNotification value) noexcept {
    return static_cast<NotificationMask>(value);
}

[[nodiscard]] constexpr bool requires_notification(NotificationMask mask,
                                                   RegionNotification value) noexcept {
    return (mask & notification_mask(value)) != 0;
}

/** Roster/Omega delivery values read before immutable snapshot construction. */
struct RosterDeliveryBefore final {
    std::uint32_t groups{};
    std::uint8_t sends{};
    std::uint8_t state{};
    std::uint8_t omegaOpeningStage{};
    std::uint16_t directorSends{};
    bool missionDirectorActive{};
};

/** Roster/Omega delivery values published once after caller copy. */
using RosterDeliveryAfter = RosterDeliveryBefore;

/** Immutable exact semantic/wire after-image for one complete region transaction. */
struct RegionTransitionSnapshot final {
    state::activity::BindingKey binding{};
    state::activity::ActivityInstanceKey activity{};
    state::activity::ActivityInstanceKey regionSource{};
    state::activity::HostRegionKey expectedHostRegion{};
    state::activity::HostRegionKey nextHostRegion{};
    state::activity::HostRegionKey sourceHostRegion{};
    std::int32_t regionIndex{-1};
    std::uint16_t destinationArrival{};

    state::activity::destination::DestinationSelection destination{};
    state::activity::membership::MembershipState membershipAfter{};
    middleware::bap::activity_message::replicate_membership::MembershipSnapshot membershipWire{};
    gameplay::AdvertisementSnapshot advertisement{};
    middleware::bap::activity_message::sensor_auth_update::Snapshot rosterWire{};
    state::activity::bubble_authority::Grant grantCandidate{};

    RosterDeliveryBefore before{};
    RosterDeliveryAfter after{};
    state::activity::PublicationGeneration rosterPublication{};
    NotificationMask required{};
    bool burst{};
    /** True only for a committed client-authoritative creator region report. */
    bool publishesHud{};
};

/** Value-owned retry after-image installed atomically with a deferred State commit. */
struct RegionPublicationDebt final {
    state::activity::BindingKey binding{};
    state::activity::ActivityInstanceKey activity{};
    state::activity::ActivityInstanceKey regionSource{};
    state::activity::HostRegionKey committedHostRegion{};
    state::activity::membership::MembershipState membershipAfter{};
    state::activity::destination::DestinationSelection destination{};
    std::int32_t regionIndex{-1};
    NotificationMask required{};
    bool present{};
    /** Preserves whether deferred delivery owes the authoritative HUD event. */
    bool publishesHud{};
};

/** BAP-owned explicit diagnostic anchor chosen only by a committed creator report. */
struct HudRegionAnchor final {
    state::activity::BindingKey ownerBinding{};
    state::activity::ActivityInstanceKey activity{};
    state::activity::HostRegionKey hostRegion{};
};

} // namespace dawn::server::bap
