#pragma once
#include "public_event_service.h"
#include "placement_service.h"
#include "public_event_placement_feedback.h"

namespace dawn::server::runtime::activity::public_event {
// Reusable native placement publication adapter for an explicitly selected
// event rally object. Its optional interaction command follows the qualified
// placement receipt; event sensors, enemy requests and rewards remain separate.
class PlacementAdapter final : public public_event::Ports {
public:
    [[nodiscard]] bool begin(Lease lease, placement::Capability capability,
        placement_feedback::Definition feedback={}) noexcept {
        placement::wire::Batch projected{};
        if (lease_.valid() || !lease.valid() || !capability.registry
            || !placement::project({&capability,1},capability.registry->bubble,projected) || projected.count!=1) return false;
        if((feedback.nativeDefinitionOffset!=0 || feedback.authoredVisualCount!=0 || feedback.enableInteractionAfterPlacement)
            && (feedback.nativeDefinitionOffset<=0 || !feedback.authoredVisualCount || feedback.authoredVisualCount>64)) return false;
        lease_=lease; capability_=capability; feedback_=feedback;
        for (const auto& slot:capability.registry->slots) if (slot.index==capability.slot)
            asset_={capability.registry->key,slot.descriptorTag,slot.type,slot.index};
        return true;
    }
    [[nodiscard]] bool publish(const Lease& lease, Stage stage, const coo::Command& command) noexcept override {
        if (!lease_.valid() || lease!=lease_ || cancelled_ || stage!=Stage::rally
            || command.token.run!=lease.event || !command.token.incarnation
            || command.schema!=coo::Schema::otherMissions || command.spec.operation!=coo::Operation::device
            || command.spec.asset!=asset_ || command.spec.argument!=1 || command.spec.wait!=coo::Wait::nativeReady) return false;
        if (published_) return command.token==token_;
        token_=command.token; published_=true; return true;
    }
    void cancel(const Lease& lease, Stage stage, const coo::Command& command) noexcept override {
        if (lease==lease_ && stage==Stage::rally && published_ && command.token==token_ && command.spec.asset==asset_)
            cancelled_=true;
        // Retain already-published authority. Removing a record or source is
        // not a proven native retirement/deactivation contract.
    }
    [[nodiscard]] bool append(std::uint32_t bubble, placement::wire::Batch& batch) const noexcept {
        if (batch.count>batch.entries.size()) return false;
        if (!published_ || bubble!=capability_.registry->bubble) return true;
        placement::wire::Batch request{};
        if (!placement::project({&capability_,1},bubble,request) || request.count!=1 || batch.count==batch.entries.size()) return false;
        for (std::size_t i=0; i<batch.count; ++i)
            if (batch.entries[i].registry==asset_.registry && batch.entries[i].slot==asset_.slot) return false;
        if(feedback_.enableInteractionAfterPlacement && lastObservation_)
            request.entries[0].interactionMode=middleware::bap::activity_message::native::interaction::Mode::enabled;
        batch.entries[batch.count++]=request.entries[0]; return true;
    }
    [[nodiscard]] bool published() const noexcept { return published_; }
    [[nodiscard]] bool cancellation_pending() const noexcept { return cancelled_; }
    [[nodiscard]] bool interaction_published() const noexcept {
        return published_ && feedback_.enableInteractionAfterPlacement && lastObservation_;
    }
    // Capture an immutable ticket before entering the native callback. The
    // activity bridge owns the qualified native authority endpoint and access.
    [[nodiscard]] bool ticket(Lease lease,std::uint32_t bubble,std::uint32_t authorityOwner,
        placement_feedback::Ticket& output) const noexcept {
        if(!published_ || cancelled_ || lease!=lease_ || bubble!=capability_.registry->bubble
            || !feedback_.authoredVisualCount) return false;
        output={lease_,asset_,token_,bubble,authorityOwner,feedback_};return true;
    }
    [[nodiscard]] Result observe(const placement_feedback::Capture& capture,
        std::uint32_t currentAuthorityOwner,Service& service) noexcept {
        placement_feedback::Ticket expected{};
        if(!ticket(capture.ticket.lease,capture.ticket.bubble,currentAuthorityOwner,expected)) return Result::stale;
        placement_feedback::Observation observation{};
        if(!placement_feedback::qualify(expected,capture,observation)) return Result::invalid;
        if(observation.sequence<=lastObservation_) return Result::duplicate;
        const auto result=service.observe(observation.receipt);
        if(result==Result::accepted) lastObservation_=observation.sequence;
        return result;
    }
    // Only the value-only native bridge may supply already-qualified evidence.
    [[nodiscard]] Result observe_qualified(const placement_feedback::Ticket& ticket,
        const placement_feedback::Observation& observation,Service& service) noexcept {
        placement_feedback::Ticket expected{};
        if(!this->ticket(ticket.lease,ticket.bubble,ticket.authorityOwner,expected)) return Result::stale;
        if(!placement_feedback::same(ticket,expected) || observation.receipt.lease!=lease_
            || observation.receipt.asset!=asset_ || observation.receipt.stage!=Stage::rally
            || observation.receipt.event.token!=token_ || observation.receipt.event.milestone!=coo::Milestone::nativeReady
            || observation.authorityOwner!=ticket.authorityOwner || !observation.sequence
            || observation.entity==UINT32_MAX || observation.source.member==UINT32_MAX
            || observation.selector>=feedback_.authoredVisualCount) return Result::invalid;
        if(observation.sequence<=lastObservation_) return Result::duplicate;
        const auto result=service.observe(observation.receipt);
        if(result==Result::accepted) lastObservation_=observation.sequence;
        return result;
    }
private:
    Lease lease_{};
    placement::Capability capability_{};
    coo::Asset asset_{};
    coo::Token token_{};
    placement_feedback::Definition feedback_{};
    std::uint64_t lastObservation_{};
    bool published_{},cancelled_{};
};
} // namespace dawn::server::runtime::activity::public_event
