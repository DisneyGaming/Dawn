#pragma once
#include "native_capture_bridge.h"
#include "placement_service.h"
#include "../../../state/activity/coo/mission_script.h"

namespace sunrise::server::runtime::activity::native_capture {
namespace coo=state::activity::coo;
namespace wire=middleware::bap::activity_message::native::capture_controller;
struct Binding final {
    std::uint16_t placement{};
    std::uint32_t entityDefinition{},controllerDefinition{};
    std::int64_t sourceDefinitionOffset{},controllerDefinitionOffset{};
    std::string_view durationParameter{};
};
class Runtime final {
public:
    template<class Definition,class Document> [[nodiscard]] static bool valid(const Definition& d,const Document& doc) noexcept {
        if(d.captures.size()>4 || (!d.captures.empty() && d.clockFrequencyParameter.empty()))return false;
        for(std::size_t i=0;i<d.captures.size();++i) {
            const auto& b=d.captures[i];const auto* duration=doc.views().parameter(b.durationParameter);
            if(b.placement>=d.placements.size() || !b.entityDefinition || !b.controllerDefinition
                || b.sourceDefinitionOffset<=0 || b.controllerDefinitionOffset<=0 || !duration
                || !duration->value || duration->value>600000 || !d.placements[b.placement].generation)return false;
            for(std::size_t j=0;j<i;++j)if(d.captures[j].placement==b.placement)return false;
        }
        return true;
    }
    template<class Definition,class Document> [[nodiscard]] bool begin(const Definition& d,const Document& doc,
        capture_feedback::RunIdentity runIdentity=capture_feedback::RunIdentity::activityIncarnation) noexcept {
        if(!valid(d,doc) || !capture_feedback::valid(runIdentity))return false;
        bindings_=d.captures;placements_=d.placements;
        for(std::size_t i=0;i<bindings_.size();++i) {
            const auto* duration=doc.views().parameter(bindings_[i].durationParameter);
            if(!activity_clock::wire::from_milliseconds(duration->value,durations_[i]))return false;
        }
        runIdentity_=runIdentity;return true;
    }
    void clock(activity_clock::Publication value) noexcept {clock_=value;}
    [[nodiscard]] bool start(std::size_t index,coo::Token token) noexcept {
        if(index>=bindings_.size() || !clock_ || states_[index].requested
            || !capture_feedback::token_matches(clock_.domain,token,runIdentity_))return false;
        const auto& b=bindings_[index];const auto& p=placements_[b.placement];
        const registry::Slot* descriptor{};
        for(const auto& slot:p.registry->slots)if(slot.index==p.slot)descriptor=&slot;
        if(!descriptor)return false;
        auto& state=states_[index];
        state.ticket={clock_.domain,token,{p.registry->key,descriptor->descriptorTag,4,p.slot},p.generation,
            b.entityDefinition,b.controllerDefinition,b.sourceDefinitionOffset,b.controllerDefinitionOffset,
            clock_.configuration,{true,{true,0,durations_[index],0,durations_[index],clock_.elapsedTicks,1.0F},false},0,runIdentity_};
        if(!capture_bridge::bind(state.ticket))return false;
        state.firstStartGeneration=state.ticket.generation;state.requested=true;return true;
    }
    [[nodiscard]] bool rearm(std::size_t index,coo::Token newToken,
        std::uint32_t newSourceGeneration) noexcept {
        if(index>=bindings_.size() || !validClock(clock_)
            || !capture_feedback::token_matches(clock_.domain,newToken,runIdentity_))return false;
        auto& state=states_[index];
        if(!state.requested || !state.completed || !state.ticket.generation
            || newSourceGeneration<=state.ticket.generation
            || newToken==state.ticket.token)return false;
        const auto& b=bindings_[index];const auto& p=placements_[b.placement];
        const registry::Slot* descriptor{};
        for(const auto& slot:p.registry->slots)if(slot.index==p.slot)descriptor=&slot;
        if(!descriptor)return false;
        const auto requested=wire::State{true,
            {true,0,durations_[index],0,durations_[index],clock_.elapsedTicks,1.0F},false};
        capture_feedback::Ticket fresh{clock_.domain,newToken,state.ticket.source,
            newSourceGeneration,state.ticket.entityDefinition,state.ticket.controllerDefinition,
            state.ticket.sourceDefinitionOffset,state.ticket.controllerDefinitionOffset,
            clock_.configuration,requested,0,runIdentity_};
        if(!capture_bridge::rebind(state.ticket,fresh))return false;
        state.ticket=fresh;state.last={};state.requested=true;state.ready=false;state.completed=false;
        return true;
    }
    template<class Submit> [[nodiscard]] bool poll(Submit submit) noexcept {
        if(!clock_)return true;
        std::array<capture_bridge::Event,8> capturedEvents{};bool overflow{};
        const auto count=capture_bridge::drain(clock_.domain.owner,capturedEvents,overflow);if(overflow)return false;
        for(std::size_t e=0;e<count;++e)for(std::size_t i=0;i<bindings_.size();++i) {
            auto& state=states_[i];const auto& event=capturedEvents[e];
            if(!state.requested || !capture_feedback::same(state.ticket,event.observation.ticket))continue;
            if(event.ready && !submit(coo::Event{state.ticket.token,coo::Milestone::nativeReady}))return false;
            if(event.completed && !submit(coo::Event{state.ticket.token,coo::Milestone::completed}))return false;
            state.ready|=event.ready;state.completed|=event.completed;state.last=event.observation;
        }
        return true;
    }
    [[nodiscard]] bool append(placement::wire::Batch& batch) const noexcept {
        if(batch.count>batch.entries.size())return false;
        for(std::size_t i=0;i<bindings_.size();++i) {
            const auto& p=placements_[bindings_[i].placement];
            for(std::size_t n=0;n<batch.count;++n) {
                auto& target=batch.entries[n];if(target.registry!=p.registry->key || target.slot!=p.slot)continue;
                if(target.capture)return false;
                if(states_[i].requested)target.generation=states_[i].ticket.generation;
                target.capture=states_[i].requested?states_[i].ticket.requested:
                    wire::State{true,{false,0,durations_[i],0,durations_[i],UINT64_MAX,0},false};
            }
        }
        return true;
    }
    struct State {capture_feedback::Ticket ticket{};capture_feedback::Observation last{};
        bool requested{},ready{},completed{};std::uint32_t firstStartGeneration{};};
    [[nodiscard]] const State& state(std::size_t i) const noexcept {return states_[i];}
    [[nodiscard]] std::size_t size() const noexcept {return bindings_.size();}
private:
    [[nodiscard]] static bool validClock(const activity_clock::Publication& value) noexcept {
        return static_cast<bool>(value) && value.elapsedTicks!=UINT64_MAX
            && activity_clock::wire::valid(value.configuration)
            && value.configuration.timing>0.0F;
    }
    std::span<const Binding> bindings_{};std::span<const placement::Capability> placements_{};
    std::array<std::uint64_t,4> durations_{};std::array<State,4> states_{};activity_clock::Publication clock_{};
    capture_feedback::RunIdentity runIdentity_{capture_feedback::RunIdentity::activityIncarnation};
};
}
