#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "bubble_authority/definition.h"
#include "defaults/definition.h"
#include "destination/definition.h"
#include "entity_slots/definition.h"
#include "forced/definition.h"
#include "lifecycle_generation.h"
#include "membership/definition.h"

namespace dawn::state::activity {

/**
 * Records bound process-local session lookup with no heap storage. It must hold every live
 * session at once: the private one plus one activity host per region. A full table refuses a new
 * allocation; live ownership is never retired implicitly.
 */
inline constexpr std::size_t kSessionCapacity = 16;
/** Zero is reserved as the absent activity-session id. */
inline constexpr std::uint64_t kAbsentSessionId = 0;
/** An unjoined record keeps its member-key storage cleared. */
inline constexpr std::uint64_t kClearedMemberKey = 0;
/** Runtime activity-session ids begin at 1 because 0 means absent. */
inline constexpr std::uint64_t kFirstSessionId = 1;
/** A cleared revision cannot validate a prepared allocation. */
inline constexpr std::uint64_t kInvalidRevision = 0;
/** State revisions begin at 1 so cleared storage rejects pending allocations. */
inline constexpr std::uint64_t kInitialStateRevision = 1;
/** Allocator revisions begin at 1 so cleared storage rejects pending allocations. */
inline constexpr std::uint64_t kInitialAllocatorRevision = 1;
/** The session table size is also its invalid-slot sentinel. */
inline constexpr std::size_t kInvalidSessionSlot = kSessionCapacity;
/**
 * Activity SOIDs reserve bit 21 for the session class. The allocator may use only the lower
 * 21-bit index, otherwise OR-composition aliases an earlier published session.
 */
inline constexpr std::uint64_t kMaximumSessionId = 0x001FFFFFULL;
/** Revisions never wrap because stale transactions could otherwise become valid again. */
inline constexpr std::uint64_t kMaximumRevision = (std::numeric_limits<std::uint64_t>::max)();

/** Lifecycle identity owned by one committed host activity record. */
struct ActivityLifecycle final {
    /** Fresh allocations begin at one. Zero is reserved for an empty record. */
    ActivityIncarnation incarnation{};
    /** Region generation one represents the fresh record before its first accepted move. */
    HostRegionGeneration hostRegion{};
    /** Sticky fail-closed state after a host-region generation cannot advance. */
    bool hostRegionExhausted{};
};

/** One committed activity-session id and its lifecycle revisions. */
struct SessionRecord {
    /** Scalar destination committed in the same transaction as this session. */
    destination::DestinationSelection destination{};
    /** Slots granted to this session and not yet returned by its client. */
    entity_slots::LeaseMask heldEntitySlots{};
    /**
     * Slots reserved for server-authored entities. Always disjoint from the held mask, and
     * never granted to a client. A released client slot returns to the free set, not here.
     */
    entity_slots::LeaseMask serverEntitySlots{};
    /** Membership data is valid only after this session binds its client key. */
    membership::MembershipState membership{};
    /** Bubble grant tokens reset with the bounded activity session record. */
    bubble_authority::AuthorityState bubbleAuthority{};
    /** Exact activity and host-region lifetimes; absent on an empty record. */
    ActivityLifecycle lifecycle{};
    std::uint64_t sessionId{};
    /** Join binds this key before any later client identity update can be accepted. */
    std::uint64_t memberKey{};
    std::uint64_t createdRevision{};
    /** Last State revision that changed this record. */
    std::uint64_t recordRevision{};
    /** Last successful join revision, or the invalid revision before any join. */
    std::uint64_t joinedRevision{};
    bool occupied{};
    bool joined{};
};

/** Read-only allocation plan validated again under the State write lock. */
struct PendingAllocation {
    /** Validated destination captured before the allocation acquires its write lock. */
    destination::DestinationSelection destination{};
    /** Exact fresh activity key created by this allocation plan. */
    ActivityInstanceKey instanceKey{};
    /** Exact caller-owned record atomically replaced by this allocation, or absent. */
    ActivityInstanceKey replaces{};
    std::uint64_t sessionId{};
    /** Account half the published soid carries, captured before the State lock is taken. */
    std::uint64_t soidBase{};
    std::uint64_t expectedStateRevision{};
    std::uint64_t expectedAllocatorRevision{};
    std::uint64_t expectedNextSessionId{};
    std::size_t targetSlot{kInvalidSessionSlot};
    bool prepared{};
};

/** Exact retirement outcome; callers can distinguish idempotence from a stale lifetime. */
enum class RetireResult : std::uint8_t {
    /** The exact record was present and is now clear. */
    retired,
    /** No committed record currently carries the requested scalar session id. */
    alreadyRetired,
    /** The scalar id is live, but its incarnation does not match the requested lifetime. */
    staleIncarnation,
};

/** Process-local activity-session records and their monotonic allocator. */
struct ActivityState {
    std::array<SessionRecord, kSessionCapacity> sessions{};
    /** Immutable authored or configured fallback published during State initialization. */
    defaults::ActivityDefaults defaults{};
    /** Operator-chosen destination that replaces the client's own. Never saved. */
    forced::ForcedDestination forced{};
    std::uint64_t stateRevision{kInitialStateRevision};
    std::uint64_t nextSessionId{kFirstSessionId};
    std::uint64_t allocatorRevision{kInitialAllocatorRevision};
    bool allocatorExhausted{};
};

} // namespace dawn::state::activity
