#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "../../internal.h"
#include "definition.h"

namespace dawn::server::bap::encrypted::activity_message {

/**
 * Routes one svc8 activity message and prepares any supported push transaction.
 * The decrypted payload is borrowed for this call only, never kept.
 * @param session Authenticated connection state used to bind type 6 and message type 52.
 * @param requestBody Whole decrypted svc8 body after the generic BAP header.
 * @param plan Cleared, then receives one deferred State transaction and optional push data.
 * @param hasTransaction Receives true only when a message stages a transaction.
 * @return True for any envelope that parses, including unhandled message types.
 */
[[nodiscard]] bool process(Session& session,
                           std::span<const std::byte> requestBody,
                           ActivityPlan& plan,
                           bool& hasTransaction) noexcept;

} // namespace dawn::server::bap::encrypted::activity_message
