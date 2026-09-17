#include "../../runtime/storage/internal.h"
#include "../runtime.h"
#include "internal.h"

namespace dawn::state::activity {
namespace {

/** Retires one already-resolved record; exhaustion may stop mutations, never cleanup. */
[[nodiscard]] bool release_locked(ActivityState& state, std::size_t slot) noexcept {
    if (slot >= kSessionCapacity) {
        return false;
    }
    state.sessions[slot] = {};
    if (state.stateRevision != kMaximumRevision) {
        ++state.stateRevision;
    }
    return true;
}

} // namespace

/** Frees one committed activity-session record. */
bool release_session(std::uint64_t sessionId) noexcept {
    if (sessionId == kAbsentSessionId) {
        return false;
    }
    AcquireSRWLockExclusive(&runtime::storage::g_stateLock);
    ActivityState& state = runtime::storage::g_state.activity;
    const std::size_t slot = transactions::find_session(state, sessionId);
    // The revision bump retires every plan prepared against this record.
    const bool released = release_locked(state, slot);
    ReleaseSRWLockExclusive(&runtime::storage::g_stateLock);
    return released;
}

/** Frees one exact committed activity incarnation. */
bool release_session(ActivityInstanceKey key) noexcept {
    return retire_session_exact(key) == RetireResult::retired;
}

/** Retires one exact committed activity incarnation with an idempotent typed result. */
RetireResult retire_session_exact(ActivityInstanceKey key) noexcept {
    if (key.sessionId == kAbsentSessionId) {
        return RetireResult::alreadyRetired;
    }
    AcquireSRWLockExclusive(&runtime::storage::g_stateLock);
    ActivityState& state = runtime::storage::g_state.activity;
    const RetireResult result = transactions::retire_exact(state, key);
    ReleaseSRWLockExclusive(&runtime::storage::g_stateLock);
    return result;
}

} // namespace dawn::state::activity
