#pragma once

#include "../../../../middleware/bap/activity_message/activity_message_request_parser.h"
#include "../internal.h"

namespace dawn::server::bap::encrypted::festival_pickups {

/** Called only after the activity route has checked this connection's exact live binding. */
void receive(const Session& session,
             const middleware::bap::activity_message::Request& request) noexcept;

/** Publishes one owed Candy unit on an authenticated account subscriber. */
[[nodiscard]] bool consume(Session& session,
                           Scratch& scratch,
                           std::span<std::byte> response,
                           std::size_t& written,
                           bool& touchesScratch) noexcept;

} // namespace dawn::server::bap::encrypted::festival_pickups
