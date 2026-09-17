#include <Windows.h>

#include "../../../runtime/storage/internal.h"
#include "../../transactions/internal.h"
#include "../runtime.h"

namespace dawn::state::activity::bubble_authority {

/** Picks the bubble owed by one copied authority after-image. */
bool select_grant(const AuthorityState& authority,
                  std::int32_t sliceSetIndex,
                  Grant& grant) noexcept {
    grant = {};
    if (sliceSetIndex < 0 || sliceSetIndex > kMaximumGrantSliceSetIndex) {
        return false;
    }
    const auto bubble = static_cast<std::uint8_t>(sliceSetIndex >> kSliceSetToBubbleShift);
    if (bubble >= kFallbackBubble || authority.grantTokens[bubble] != 0) {
        return false;
    }
    grant.bubble = bubble;
    grant.token = kInitialGrantToken;
    return true;
}

/** Picks the bubble to hand this session, if one is owed. */
bool select_grant(ActivityInstanceKey key,
                  std::int32_t sliceSetIndex,
                  Grant& grant) noexcept {
    grant = {};
    if (!static_cast<bool>(key) || sliceSetIndex < 0
        || sliceSetIndex > kMaximumGrantSliceSetIndex) {
        return false;
    }
    bool owed = false;
    AcquireSRWLockShared(&runtime::storage::g_stateLock);
    const ActivityState& state = runtime::storage::g_state.activity;
    const std::size_t target = activity::transactions::find_session(state, key);
    if (target != kInvalidSessionSlot) {
        owed = select_grant(state.sessions[target].bubbleAuthority, sliceSetIndex, grant);
    }
    ReleaseSRWLockShared(&runtime::storage::g_stateLock);
    return owed;
}

/** Records a bubble as granted so it is not granted twice. */
void record_grant(ActivityInstanceKey key, const Grant& grant) noexcept {
    if (!static_cast<bool>(key) || grant.bubble >= kAuthoritySlotCount || grant.token == 0) {
        return;
    }
    AcquireSRWLockExclusive(&runtime::storage::g_stateLock);
    ActivityState& state = runtime::storage::g_state.activity;
    const std::size_t target = activity::transactions::find_session(state, key);
    if (target != kInvalidSessionSlot) {
        state.sessions[target].bubbleAuthority.grantTokens[grant.bubble] = grant.token;
    }
    ReleaseSRWLockExclusive(&runtime::storage::g_stateLock);
}

/** Drops every grant recorded for one session, so the next roster push grants again. */
void clear_grants(ActivityInstanceKey key) noexcept {
    if (!static_cast<bool>(key)) {
        return;
    }
    AcquireSRWLockExclusive(&runtime::storage::g_stateLock);
    ActivityState& state = runtime::storage::g_state.activity;
    const std::size_t target = activity::transactions::find_session(state, key);
    if (target != kInvalidSessionSlot) {
        state.sessions[target].bubbleAuthority = {};
    }
    ReleaseSRWLockExclusive(&runtime::storage::g_stateLock);
}

} // namespace dawn::state::activity::bubble_authority
