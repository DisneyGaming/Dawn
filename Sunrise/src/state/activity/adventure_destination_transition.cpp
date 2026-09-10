#include "adventure_destination_transition.h"
#include <Windows.h>
#include "../runtime/storage/internal.h"

namespace sunrise::state::activity::adventure_destination {
bool snapshot(ActivityInstanceKey owner,Snapshot& output) noexcept {
    output={};
    AcquireSRWLockShared(&runtime::storage::g_stateLock);
    const auto& state=runtime::storage::g_state.activity;
    const auto slot=activity::transactions::find_session(state,owner);
    if(slot<state.sessions.size() && state.sessions[slot].joined) {
        const auto& record=state.sessions[slot];
        output={owner,record.recordRevision,record.destination};
    }
    ReleaseSRWLockShared(&runtime::storage::g_stateLock);
    return static_cast<bool>(output.owner);
}
Result prepare(ActivityInstanceKey owner,std::uint64_t expectedRecordRevision,
               const destination::DestinationSelection& selection,Pending& output) noexcept {
    AcquireSRWLockShared(&runtime::storage::g_stateLock);
    const auto result=prepare_from(runtime::storage::g_state.activity,owner,expectedRecordRevision,selection,output);
    ReleaseSRWLockShared(&runtime::storage::g_stateLock);
    return result;
}
bool commit(Pending& pending) noexcept {
    AcquireSRWLockExclusive(&runtime::storage::g_stateLock);
    const bool result=commit_to(runtime::storage::g_state.activity,pending);
    ReleaseSRWLockExclusive(&runtime::storage::g_stateLock);
    return result;
}
} // namespace sunrise::state::activity::adventure_destination
