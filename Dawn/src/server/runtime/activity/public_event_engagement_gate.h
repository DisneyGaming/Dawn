#pragma once
#include "public_event_engagement_feedback.h"
#include "../../../middleware/bap/activity_message/native/public_event_engagement_sense.h"

namespace dawn::server::runtime::activity::public_event {
namespace engagement_sense=middleware::bap::activity_message::native::engagement_sense;
// Retained by a single event run under the activity owner. This gate does not
// drain a mailbox, manufacture a native source, or equate participant count with
// local-player eligibility. The subsequent mode1 cue receipt proves eligibility.
class EngagementGate final {
public:
    [[nodiscard]] bool begin(const engagement_feedback::Ticket& ticket) noexcept {
        if(bound_ || !engagement_feedback::valid(ticket)
            || ticket.request.collection!=engagement_feedback::wire::Collection::activePlayers)return false;
        ticket_=ticket;bound_=true;return true;
    }
    [[nodiscard]] bool applied(const engagement_feedback::Observation& observation) noexcept {
        if(!bound_ || applied_ || observation.ticket!=ticket_ || !observation.sequence
            || observation.source.member==UINT32_MAX || observation.source.offset<0)return false;
        applied_=true;return true;
    }
    [[nodiscard]] bool observe(const engagement_feedback::Ticket& ticket,std::uint32_t schema,
        const engagement_sense::Output& sense) noexcept {
        if(!bound_ || ticket!=ticket_ || schema!=engagement_sense::kSchema || !sense.root
            || sense.generation!=ticket_.request.generation || sense.participantCount>16
            || (hasSense_ && sense.revision<=revision_))return false;
        hasSense_=true;revision_=sense.revision;participants_=sense.participantCount;return true;
    }
    [[nodiscard]] bool ready_for_cue() const noexcept {return bound_ && applied_ && hasSense_ && participants_!=0;}
    [[nodiscard]] const engagement_feedback::Ticket& ticket() const noexcept {return ticket_;}
private:
    engagement_feedback::Ticket ticket_{};
    std::uint32_t revision_{};std::uint8_t participants_{};
    bool bound_{},applied_{},hasSense_{};
};
}
