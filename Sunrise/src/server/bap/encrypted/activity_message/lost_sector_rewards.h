#pragma once

#include "../internal.h"
#include "../../../../middleware/bap/activity_message/object_sense.h"

namespace sunrise::server::bap::encrypted::lost_sector_rewards {

// Accept one native use latch for the exact boss-cleared chest generation.
// Delivery is deferred to the authenticated Family-4 connection.
void receive(const Session& session,state::activity::ActivityInstanceKey owner,
    std::uint32_t bubble,std::uint32_t registry,std::uint8_t type,std::uint16_t slot,
    const middleware::bap::activity_message::object_sense::Output& output) noexcept;

[[nodiscard]] bool consume(Session& session,Scratch& scratch,std::span<std::byte> response,
    std::size_t& written,bool& touchesScratch) noexcept;

} // namespace sunrise::server::bap::encrypted::lost_sector_rewards
