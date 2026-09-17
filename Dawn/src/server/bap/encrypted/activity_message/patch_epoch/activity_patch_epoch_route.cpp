#include "activity_patch_epoch_route.h"

#include "../../../../../middleware/bap/activity_message/activity_patch_epoch_parser.h"

namespace dawn::server::bap::encrypted::activity_message::patch_epoch {

namespace message = middleware::bap::activity_message::patch_epoch;

/** Parses type 52 and keeps its epoch for the next roster update. */
bool prepare(state::activity::ActivityInstanceKey activity,
             const middleware::bap::activity_message::Request& request,
             ActivityPlan& plan) noexcept {
    message::PatchEpoch epoch{};
    if (!static_cast<bool>(activity) || !message::parse_patch_epoch(request.payload, epoch)) {
        return false;
    }
    plan.patchEpoch = epoch;
    plan.instanceKey = activity;
    plan.sessionId = activity.sessionId;
    plan.delivery = Delivery::none;
    plan.mutationDomain = MutationDomain::patchEpoch;
    return true;
}

} // namespace dawn::server::bap::encrypted::activity_message::patch_epoch
