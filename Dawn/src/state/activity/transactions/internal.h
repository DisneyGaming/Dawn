#pragma once

#include <cstddef>

#include "../definition.h"

namespace dawn::state::activity::transactions {

/** @return Matching slot, or the fixed-table absent value when the session is not there. */
inline std::size_t find_session(const ActivityState& state, std::uint64_t sessionId) noexcept {
    for (std::size_t index = 0; index < state.sessions.size(); ++index) {
        const SessionRecord& record = state.sessions[index];
        if (record.occupied && record.sessionId == sessionId) {
            return index;
        }
    }
    return kInvalidSessionSlot;
}

/** @return Exact matching activity slot, or the fixed-table absent value. */
inline std::size_t find_session(const ActivityState& state, ActivityInstanceKey key) noexcept {
    if (!static_cast<bool>(key)) {
        return kInvalidSessionSlot;
    }
    for (std::size_t index = 0; index < state.sessions.size(); ++index) {
        const SessionRecord& record = state.sessions[index];
        if (record.occupied && record.sessionId == key.sessionId
            && record.lifecycle.incarnation == key.incarnation) {
            return index;
        }
    }
    return kInvalidSessionSlot;
}

/** @return Exact activity key owned by a committed record, or an absent key. */
inline ActivityInstanceKey instance_key(const SessionRecord& record) noexcept {
    if (!record.occupied || record.sessionId == kAbsentSessionId
        || !static_cast<bool>(record.lifecycle.incarnation)) {
        return {};
    }
    return {record.sessionId, record.lifecycle.incarnation};
}

/** @return Exact host-region key owned by a committed record, or an absent key. */
inline HostRegionKey host_region_key(const SessionRecord& record) noexcept {
    const ActivityInstanceKey activity = instance_key(record);
    if (!static_cast<bool>(activity) || !static_cast<bool>(record.lifecycle.hostRegion)) {
        return {};
    }
    return {activity, record.lifecycle.hostRegion};
}

/** @return Lifecycle fields for a newly committed host activity record. */
inline constexpr ActivityLifecycle fresh_lifecycle() noexcept {
    return {
        ActivityIncarnation{kFirstGeneration},
        HostRegionGeneration{kFirstGeneration},
        false,
    };
}

/**
 * Picks the first empty record. Live records are never implicit eviction candidates.
 * @param state Activity State, guarded by the root State lock.
 * @return Target slot, or the fixed-table absent value when the table is full.
 */
inline std::size_t select_target(const ActivityState& state) noexcept {
    for (std::size_t index = 0; index < state.sessions.size(); ++index) {
        if (!state.sessions[index].occupied) {
            return index;
        }
    }

    return kInvalidSessionSlot;
}

/**
 * Picks an exact replacement slot when one is named, otherwise the first empty record.
 * A stale or foreign replacement never falls back to an unrelated empty slot.
 */
inline std::size_t select_target(const ActivityState& state,
                                 ActivityInstanceKey replaces) noexcept {
    return static_cast<bool>(replaces) ? find_session(state, replaces) : select_target(state);
}

/** Retires one exact record in caller-owned locked State with an idempotent typed result. */
inline RetireResult retire_exact(ActivityState& state, ActivityInstanceKey key) noexcept {
    if (key.sessionId == kAbsentSessionId) {
        return RetireResult::alreadyRetired;
    }
    if (!static_cast<bool>(key.incarnation)) {
        return find_session(state, key.sessionId) == kInvalidSessionSlot
                   ? RetireResult::alreadyRetired
                   : RetireResult::staleIncarnation;
    }
    const std::size_t slot = find_session(state, key);
    if (slot == kInvalidSessionSlot) {
        return find_session(state, key.sessionId) == kInvalidSessionSlot
                   ? RetireResult::alreadyRetired
                   : RetireResult::staleIncarnation;
    }
    state.sessions[slot] = {};
    if (state.stateRevision != kMaximumRevision) {
        ++state.stateRevision;
    }
    return RetireResult::retired;
}

/** Activity-session class the published soid carries between the account half and the index. */
inline constexpr std::uint64_t kSessionClass = 0x00200000ULL;

/**
 * Builds the soid the Client keys its own activity-session record by.
 * A bare counter matches no record, and sim event 20 then drops the roster with no line. The
 * built form is `0x9EAA300100200001`.
 * @param soidBase Account half, or zero while no account is loaded.
 * @param counter Allocator index for this record.
 * @return Published session soid.
 */
[[nodiscard]] inline std::uint64_t compose_session_soid(std::uint64_t soidBase,
                                                        std::uint64_t counter) noexcept {
    if (counter == kAbsentSessionId || counter > kMaximumSessionId) {
        return kAbsentSessionId;
    }
    return soidBase != 0 ? soidBase | kSessionClass | counter : counter;
}

/**
 * Tests whether the activity allocator can commit one more id.
 * @param state Activity allocator to check.
 * @return True when one more id can commit without wrapping a counter.
 */
inline bool allocation_available(const ActivityState& state) noexcept {
    return !state.allocatorExhausted && state.nextSessionId != kAbsentSessionId
           && state.nextSessionId <= kMaximumSessionId
           && state.stateRevision != kMaximumRevision
           && state.allocatorRevision != kMaximumRevision;
}

/**
 * Advances the allocator without wrapping the id or its revision.
 * @param state Activity State held under the root State write lock.
 */
inline void advance_allocator(ActivityState& state) noexcept {
    if (state.nextSessionId == kMaximumSessionId) {
        // The highest id stays committed, but there is no next one.
        state.allocatorExhausted = true;
    } else {
        ++state.nextSessionId;
    }
    ++state.allocatorRevision;
    if (state.allocatorRevision == kMaximumRevision) {
        state.allocatorExhausted = true;
    }
}

} // namespace dawn::state::activity::transactions
