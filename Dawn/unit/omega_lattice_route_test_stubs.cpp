#include "server/bap/encrypted/activity_message/activity_message_route.h"
#include "middleware/bap/activity_message/activity_join_request_parser.h"
#include "middleware/bap/activity_message/activity_message_request_parser.h"
#include "middleware/bap/activity_message/entity_slots.h"
#include "middleware/bap/activity_message/entity_authority.h"
#include "middleware/bap/activity_message/incident.h"
#include "middleware/bap/activity_message/activity_entity_slot_request_parser.h"
#include "core/settings/settings.h"
#include "state/activity/runtime.h"
#include "server/bap/encrypted/activity_message/membership/activity_membership_route.h"
#include "server/bap/encrypted/activity_message/patch_epoch/activity_patch_epoch_route.h"
#include <cstdlib>

// Unused message families in the included production translation unit still require symbols
// with this MSVC linker. Abort if one becomes reachable; these do not emulate host behavior.
namespace dawn::state::activity {
bool contains(ActivityInstanceKey) noexcept { std::abort(); }
namespace entity_slots {
bool prepare_join(std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t,
                  PendingMutation&) noexcept { std::abort(); }
bool prepare_grant(ActivityInstanceKey, std::uint64_t, PendingMutation&) noexcept { std::abort(); }
bool prepare_release(ActivityInstanceKey, const std::array<std::byte,1024>&,
                     PendingMutation&) noexcept { std::abort(); }
}
}
namespace dawn::core::settings::server::gameplay {
std::uint16_t effective_reserve(const Settings&) noexcept { std::abort(); }
}
namespace dawn::middleware::bap::activity_message {
bool parse_request(std::span<const std::byte>, Request&) noexcept { std::abort(); }
namespace join_request {
bool parse_join_request(std::span<const std::byte>, JoinRequest&) noexcept { std::abort(); }
}
namespace entity_slots {
bool decode_entity_slots(std::span<const std::byte>, std::array<std::byte,1024>&) noexcept { std::abort(); }
}
namespace entity_authority {
bool parse_abandon(std::span<const std::byte>, Release&) noexcept { std::abort(); }
bool parse_abdicate(std::span<const std::byte>, Release&) noexcept { std::abort(); }
bool parse_request_purge(std::span<const std::byte>, std::int32_t&) noexcept { std::abort(); }
bool parse_query_answer(std::uint32_t, std::span<const std::byte>, QueryAnswer&) noexcept { std::abort(); }
}
namespace incident {
const char* verdict_name(Verdict) noexcept { std::abort(); }
Verdict validate(std::span<const std::byte>, Incident&) noexcept { std::abort(); }
}
namespace entity_slot_request {
bool parse_entity_slot_request(std::span<const std::byte>, std::int32_t&) noexcept { std::abort(); }
}
}
namespace dawn::server::bap::encrypted::activity_message {
namespace membership {
bool prepare_identity(state::activity::ActivityInstanceKey,
    const middleware::bap::activity_message::Request&, ActivityPlan&) noexcept { std::abort(); }
bool prepare_authoritative(state::activity::ActivityInstanceKey,
    const middleware::bap::activity_message::Request&, ActivityPlan&) noexcept { std::abort(); }
bool prepare_refresh(state::activity::ActivityInstanceKey,
    const middleware::bap::activity_message::Request&, ActivityPlan&) noexcept { std::abort(); }
bool prepare_acknowledgement(state::activity::ActivityInstanceKey,
    const middleware::bap::activity_message::Request&, ActivityPlan&) noexcept { std::abort(); }
}
namespace patch_epoch {
bool prepare(state::activity::ActivityInstanceKey,
    const middleware::bap::activity_message::Request&, ActivityPlan&) noexcept { std::abort(); }
}
}

