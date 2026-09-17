#pragma once

#include <cstdint>

#include "../definition.h"

namespace dawn::state::activity::bubble_authority {

/**
 * Picks the bubble to hand this session, if one is owed.
 * A bubble is granted once. The token is a change against the client's own mirror, so re-sending
 * the same token for a bubble already granted does nothing, rather than being an error.
 * @param activity Exact joined activity.
 * @param sliceSetIndex Slice set the client is in, or the destination's own.
 * @param grant Gets the bubble and its token.
 * @return True when a bubble is owed.
 */
[[nodiscard]] bool
select_grant(ActivityInstanceKey key, std::int32_t sliceSetIndex, Grant& grant) noexcept;

/** Picks a grant from a copied authority after-image without reading or changing State. */
[[nodiscard]] bool select_grant(const AuthorityState& authority,
                                std::int32_t sliceSetIndex,
                                Grant& grant) noexcept;

/**
 * Records a bubble as granted so it is not granted twice.
 * @param activity Exact joined activity.
 * @param grant Bubble and token that went out.
 */
void record_grant(ActivityInstanceKey key, const Grant& grant) noexcept;

/**
 * Drops every grant recorded for one session, so the next roster push grants again.
 * A join resets the client's roster container. Keeping the old grant set would leave the new
 * container ungranted.
 * @param activity Exact joined activity.
 */
void clear_grants(ActivityInstanceKey key) noexcept;

} // namespace dawn::state::activity::bubble_authority
