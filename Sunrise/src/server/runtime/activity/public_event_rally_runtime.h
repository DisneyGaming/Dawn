#pragma once
#include "public_event_placement_adapter.h"
#include "public_event_native_bridge.h"

namespace sunrise::server::runtime::activity::public_event {
struct RallyBinding final {
    const Definition* event{};
    placement::Capability placement{};
    placement_feedback::Definition feedback{};
    std::uint32_t nativeAuthorityOwner{}; // Authored bubble scope; legacy field name.
    std::string_view enabledParameter{};
};
// Optional, explicitly selected first event increment. One rally per activity;
// the service's real native retirement barrier is required before any renewal.
// The same binding/parameter contract works for another destination profile.
class RallyRuntime final {
public:
    template<class ActivityDefinition,class Document>
    [[nodiscard]] static bool valid(const ActivityDefinition& activity,const Document& document) noexcept {
        if(!activity.profile || activity.publicEventRallies.size()>1)return false;
        for(const auto& binding:activity.publicEventRallies) {
            const auto* parameter=document.views().parameter(binding.enabledParameter);
            if(!parameter || parameter->value>1)return false;
            if(!binding.event || !Service::valid(*binding.event) || binding.event->encounter
                || !binding.placement.registry || binding.nativeAuthorityOwner>=64 || binding.enabledParameter.empty()
                || binding.feedback.nativeDefinitionOffset<=0 || !binding.feedback.authoredVisualCount
                || binding.feedback.authoredVisualCount>64 || binding.event->registries.size()!=1)return false;
            const auto& registry=*binding.placement.registry;
            if(binding.nativeAuthorityOwner!=registry.bubble)return false;
            const auto& expected=binding.event->registries.front();
            // Disabled optional data does not constrain an unrelated profile
            // reusing the persistent adapter. Enabling it requires exact scope.
            if((parameter->value && registry.activity!=activity.activity) || registry.bubble!=activity.bubble || registry.key!=expected.key
                || registry.scenario!=expected.scenario || registry.objectTag!=expected.objectTag || registry.bubbleHash!=expected.bubbleHash)return false;
            bool admitted{};for(const auto& candidate:activity.registries)if(&candidate==binding.placement.registry)admitted=true;
            placement::wire::Batch projected{};
            if(!admitted || !placement::project({&binding.placement,1},activity.bubble,projected) || projected.count!=1)return false;
            const auto& graph=*binding.event->rally;
            if(graph.schema!=coo::Schema::otherMissions || graph.steps.size()!=1 || graph.steps[0].commands.size()!=1)return false;
            const auto& command=graph.steps[0].commands[0];
            if(command.operation!=coo::Operation::device || command.wait!=coo::Wait::nativeReady || command.argument!=1
                || command.asset.registry!=registry.key || command.asset.type!=4 || command.asset.slot!=binding.placement.slot)return false;
            bool descriptor{};
            for(const auto& slot:registry.slots)
                if(slot.index==binding.placement.slot)descriptor=slot.descriptorTag==command.asset.definition;
            if(!descriptor)return false;
            unsigned registered{};
            for(const auto& candidate:activity.profile->parameters)
                if(candidate.id==binding.enabledParameter && candidate.minimum==0 && candidate.maximum==1
                    && candidate.defaultValue==0 && !candidate.liveEditable)++registered;
            if(registered!=1)return false;
            // No independent persistent command may own this same native slot.
            for(const auto& placement:activity.placements)
                if(placement.registry && placement.registry->key==registry.key && placement.slot==binding.placement.slot)return false;
        }
        return true;
    }
    template<class ActivityDefinition,class Document>
    [[nodiscard]] bool begin(state::activity::ActivityInstanceKey owner,std::uint64_t boot,
        const ActivityDefinition& activity,const Document& document) noexcept {
        if(owner_ || !owner || !boot || !valid(activity,document))return false;
        owner_=owner;boot_=boot;
        if(activity.publicEventRallies.empty())return true;
        binding_=&activity.publicEventRallies.front();
        enabled_=document.views().parameter(binding_->enabledParameter)->value==1;
        return true;
    }
    [[nodiscard]] bool update(std::uint32_t bubble,placement::wire::Batch& frame) noexcept {
        if(!enabled_)return true;
        if(failed_ || !binding_ || bubble!=binding_->placement.registry->bubble)return false;
        if(!started_) {
            // This runtime is single-use and the immutable document is revision1.
            const Lease lease{owner_,boot_,1,1};
            if(!service_.begin(lease,*binding_->event) || !adapter_.begin(lease,binding_->placement,binding_->feedback)) {
                failed_=true;return false;
            }
            started_=true;
        }
        std::array<native_bridge::Event,1> observations{};bool overflow{};
        const auto count=native_bridge::drain(owner_,observations,overflow);
        if(overflow) {failed_=true;return false;}
        for(std::size_t i=0;i<count;++i) {
            if(adapter_.observe_qualified(observations[i].binding.ticket,observations[i].observation,service_)!=Result::accepted) {
                failed_=true;return false;
            }
            lastObservation_=observations[i].observation;
        }
        service_.update(adapter_);
        auto projected=frame;
        placement_feedback::Ticket ticket{};
        if(!adapter_.append(bubble,projected) || !adapter_.ticket(service_.diagnostics().lease,bubble,binding_->nativeAuthorityOwner,ticket)
            || !native_bridge::bind(ticket)) {failed_=true;return false;}
        frame=projected;return true;
    }
    [[nodiscard]] bool append(std::uint32_t bubble,placement::wire::Batch& frame) const noexcept {
        return !started_ || adapter_.append(bubble,frame);
    }
    [[nodiscard]] bool enabled() const noexcept {return enabled_;}
    [[nodiscard]] bool failed() const noexcept {return failed_;}
    [[nodiscard]] bool used() const noexcept {
        return enabled_ && started_ && !failed_ && lastObservation_.sequence
            && native_bridge::used(service_.diagnostics().lease);
    }
    [[nodiscard]] Diagnostics diagnostics() const noexcept {return service_.diagnostics();}
    [[nodiscard]] const placement_feedback::Observation& last_observation() const noexcept {return lastObservation_;}
private:
    state::activity::ActivityInstanceKey owner_{};std::uint64_t boot_{};
    const RallyBinding* binding_{};
    Service service_;PlacementAdapter adapter_;
    placement_feedback::Observation lastObservation_{};
    bool enabled_{},started_{},failed_{};
};
}
