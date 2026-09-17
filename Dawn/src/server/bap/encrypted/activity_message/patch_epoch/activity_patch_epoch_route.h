#pragma once

#include "../../../../../middleware/bap/activity_message/definition.h"
#include "../definition.h"

namespace dawn::server::bap::encrypted::activity_message::patch_epoch {

/**
 * Parses type 52 and keeps its epoch for the next roster update.
 * @param activity Exact authenticated joined activity capability.
 * @param request Validated zero-handle svc8 envelope.
 * @param plan Cleared, then receives the epoch echo.
 * @return True when the fixed 16-byte epoch parses.
 */
[[nodiscard]] bool prepare(state::activity::ActivityInstanceKey activity,
                           const middleware::bap::activity_message::Request& request,
                           ActivityPlan& plan) noexcept;

} // namespace dawn::server::bap::encrypted::activity_message::patch_epoch
