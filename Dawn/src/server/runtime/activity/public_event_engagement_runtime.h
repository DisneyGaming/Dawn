#pragma once

#include "public_event_engagement_bridge.h"
#include "public_event_engagement_gate.h"

namespace dawn::server::runtime::activity::public_event {

// One retained event run in one world. The caller supplies these identities
// from the committed activity and exact registry admission, never from a timer.
struct EngagementContext final {
    engagement_feedback::Owner owner{};
    std::uint64_t boot{},definitionRevision{},selectionRevision{},event{};
    std::int16_t activity{-1};
    std::uint32_t bubble{UINT32_MAX};
    bool arrived{},admitted{};
};

struct EngagementFrame final {
    engagement_feedback::wire::Request authority{};
    bool publish{},applied{},readyForCue{},stale{},failed{};
};

// Binds the native observer ticket before projection. Apply and participant
// sense may arrive in either order; both must qualify before a cue is offered.
// There is no owner release, replay, source renewal or synthetic native event
// here. Actual world retirement owns bridge cleanup and destroys this instance.
class EngagementRuntime final {
public:
    [[nodiscard]] bool begin(const engagement_feedback::Ticket& ticket,
                             std::int16_t activity) noexcept {
        if (bound_ || activity < 0 || !gate_.begin(ticket)) {
            return false;
        }
        ticket_ = ticket;
        activity_ = activity;
        bound_ = true;
        return true;
    }

    [[nodiscard]] EngagementFrame update(const EngagementContext& current) noexcept {
        eligible_ = false;
        if (!bound_ || stale_ || failed_) {
            return frame();
        }
        if (current.owner != ticket_.owner || current.boot != ticket_.boot
            || current.definitionRevision != ticket_.definitionRevision
            || current.selectionRevision != ticket_.selectionRevision
            || current.event != ticket_.event || current.activity != activity_) {
            stale_ = true;
            return frame();
        }
        eligible_ = current.arrived && current.admitted && current.bubble == ticket_.request.scope;
        if (!eligible_) {
            return frame();
        }
        if (!published_) {
            if (!engagement_bridge::bind(ticket_)) {
                failed_ = true;
                return frame();
            }
            published_ = true;
        }
        std::array<engagement_bridge::Event,1> events{};
        const auto count = engagement_bridge::drain(ticket_, events);
        for (std::size_t i = 0; i < count; ++i) {
            if (events[i].binding.ticket != ticket_ || !gate_.applied(events[i].observation)) {
                failed_ = true;
                return frame();
            }
            applied_ = true;
        }
        return frame();
    }

    // The central sense decoder supplies its current full world/event ticket.
    // No mailbox drain or client player-identity guess occurs on this route.
    [[nodiscard]] bool observe(const engagement_feedback::Ticket& current,
                               std::uint32_t bubble,
                               std::uint32_t schema,
                               const engagement_sense::Output& sense) noexcept {
        return bound_ && published_ && !stale_ && !failed_
            && bubble == ticket_.request.scope
            && gate_.observe(current, schema, sense);
    }

    [[nodiscard]] EngagementFrame frame() const noexcept {
        const bool active = bound_ && published_ && eligible_ && !stale_ && !failed_;
        return {ticket_.request, active, applied_, active && gate_.ready_for_cue(), stale_, failed_};
    }

    // Publication and readiness have different lifetimes. The same retained
    // world owner republishes its already-bound authority even when progression
    // is ineligible or stopped. This accessor never binds or consumes a receipt.
    [[nodiscard]] const engagement_feedback::wire::Request* retained_authority() const noexcept {
        return bound_ && published_ ? &ticket_.request : nullptr;
    }

    [[nodiscard]] const engagement_feedback::Ticket& ticket() const noexcept { return ticket_; }

private:
    engagement_feedback::Ticket ticket_{};
    EngagementGate gate_{};
    std::int16_t activity_{-1};
    bool bound_{},published_{},eligible_{},applied_{},stale_{},failed_{};
};

} // namespace dawn::server::runtime::activity::public_event
