#include <Windows.h>

#include "../runtime/storage/internal.h"
#include "runtime.h"
#include "transactions/internal.h"

namespace dawn::state::activity {

/** Tests whether a nonzero activity-session id is still in the bounded table. */
bool contains(std::uint64_t sessionId) noexcept {
    if (sessionId == kAbsentSessionId) {
        return false;
    }
    AcquireSRWLockShared(&runtime::storage::g_stateLock);
    const ActivityState& state = runtime::storage::g_state.activity;
    bool found = false;
    for (const SessionRecord& record : state.sessions) {
        if (record.occupied && record.sessionId == sessionId) {
            found = true;
            break;
        }
    }
    ReleaseSRWLockShared(&runtime::storage::g_stateLock);
    return found;
}

/** Tests whether one exact activity incarnation is still in the bounded table. */
bool contains(ActivityInstanceKey key) noexcept {
    if (!static_cast<bool>(key)) {
        return false;
    }
    AcquireSRWLockShared(&runtime::storage::g_stateLock);
    const bool found = transactions::find_session(runtime::storage::g_state.activity, key)
                       != kInvalidSessionSlot;
    ReleaseSRWLockShared(&runtime::storage::g_stateLock);
    return found;
}

/** Captures the exact incarnation currently owned by one session id. */
bool snapshot_instance_key(std::uint64_t sessionId, ActivityInstanceKey& output) noexcept {
    output = {};
    if (sessionId == kAbsentSessionId) {
        return false;
    }
    AcquireSRWLockShared(&runtime::storage::g_stateLock);
    const ActivityState& state = runtime::storage::g_state.activity;
    const std::size_t slot = transactions::find_session(state, sessionId);
    const ActivityInstanceKey selected =
        slot < kSessionCapacity ? transactions::instance_key(state.sessions[slot])
                                : ActivityInstanceKey{};
    ReleaseSRWLockShared(&runtime::storage::g_stateLock);
    if (!static_cast<bool>(selected)) {
        return false;
    }
    output = selected;
    return true;
}

/** Captures the current host-region generation of one exact activity incarnation. */
bool snapshot_host_region_key(ActivityInstanceKey activity, HostRegionKey& output) noexcept {
    output = {};
    if (!static_cast<bool>(activity)) {
        return false;
    }
    AcquireSRWLockShared(&runtime::storage::g_stateLock);
    const ActivityState& state = runtime::storage::g_state.activity;
    const std::size_t slot = transactions::find_session(state, activity);
    const HostRegionKey selected =
        slot < kSessionCapacity ? transactions::host_region_key(state.sessions[slot])
                                : HostRegionKey{};
    ReleaseSRWLockShared(&runtime::storage::g_stateLock);
    if (!static_cast<bool>(selected)) {
        return false;
    }
    output = selected;
    return true;
}

/** Tests whether a committed activity-session id has finished a join. */
bool is_joined(std::uint64_t sessionId) noexcept {
    if (sessionId == kAbsentSessionId) {
        return false;
    }
    AcquireSRWLockShared(&runtime::storage::g_stateLock);
    const ActivityState& state = runtime::storage::g_state.activity;
    bool joined = false;
    for (const SessionRecord& record : state.sessions) {
        if (record.occupied && record.sessionId == sessionId) {
            joined = record.joined && record.joinedRevision != kInvalidRevision;
            break;
        }
    }
    ReleaseSRWLockShared(&runtime::storage::g_stateLock);
    return joined;
}

/** Returns the most recently created joined activity session. */
std::uint64_t newest_joined_session() noexcept {
    AcquireSRWLockShared(&runtime::storage::g_stateLock);
    const ActivityState& state = runtime::storage::g_state.activity;
    std::uint64_t selectedId = kAbsentSessionId;
    std::uint64_t selectedRevision = kInvalidRevision;
    for (const SessionRecord& record : state.sessions) {
        if (record.occupied && record.joined && record.joinedRevision != kInvalidRevision
            && record.createdRevision >= selectedRevision) {
            selectedId = record.sessionId;
            selectedRevision = record.createdRevision;
        }
    }
    ReleaseSRWLockShared(&runtime::storage::g_stateLock);
    return selectedId;
}

/** Returns the exact most recently created joined activity lifetime. */
ActivityInstanceKey newest_joined_activity() noexcept {
    AcquireSRWLockShared(&runtime::storage::g_stateLock);
    const ActivityState& state = runtime::storage::g_state.activity;
    ActivityInstanceKey selected{};
    std::uint64_t selectedRevision = kInvalidRevision;
    for (const SessionRecord& record : state.sessions) {
        if (record.occupied && record.joined && record.joinedRevision != kInvalidRevision
            && record.createdRevision >= selectedRevision) {
            selected = transactions::instance_key(record);
            selectedRevision = record.createdRevision;
        }
    }
    ReleaseSRWLockShared(&runtime::storage::g_stateLock);
    return selected;
}

} // namespace dawn::state::activity
