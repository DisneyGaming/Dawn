#pragma once

#include "lost_sector_runtime.h"
#include "../../../middleware/bap/activity_message/object_sense.h"

namespace dawn::server::runtime::activity::lost_sector::reward {

[[nodiscard]] constexpr bool accepted_use(const RewardTicket& ticket,std::uint32_t bubble,
    std::uint32_t registry,std::uint8_t type,std::uint16_t slot,
    const middleware::bap::activity_message::object_sense::Output& output) noexcept {
    return static_cast<bool>(ticket) && type==4 && ticket.bubble==bubble
        && ticket.registry==registry && ticket.slot==slot
        && output.alive && output.present && output.hasUse && output.used
        && output.generation>0 && static_cast<std::uint32_t>(output.generation)==ticket.generation
        && output.useRevision>=0;
}

} // namespace dawn::server::runtime::activity::lost_sector::reward
