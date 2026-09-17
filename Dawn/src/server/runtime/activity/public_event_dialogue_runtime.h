#pragma once
#include "adventure_dialogue_bridge.h"
#include "registry_admission.h"
#include "../../../state/activity/coo/executor.h"

namespace dawn::server::runtime::activity::public_event::dialogue {
namespace feedback=adventure::dialogue_feedback;
namespace bridge=adventure::dialogue_bridge;
namespace coo=state::activity::coo;

struct Definition final {
    const registry::Definition* registry{};
    const coo::Definition* graph{};
    std::uint16_t slot{};
    feedback::Authored conversation{};
};
struct Context final {
    feedback::Owner owner{};
    std::uint64_t boot{},definitionRevision{},selectionRevision{},event{};
    std::int16_t activity{-1};
    std::uint32_t bubble{UINT32_MAX};
    bool arrived{},admitted{};
};
// One authored conversation per event stage. The UE command requests the bank
// row; the original consumer receipt proves submission to native arbitration.
// Submission never pretends to prove that a voice line finished or was audible.
class Runtime final {
public:
    [[nodiscard]] static bool valid(const Definition& d) noexcept {
        if(!d.registry || !registry::valid(*d.registry) || !d.graph
            || !coo::Executor::valid(*d.graph) || d.graph->schema!=coo::Schema::otherMissions
            || d.graph->steps.size()!=1 || d.graph->steps[0].dependencies
            || d.graph->steps[0].commands.size()!=1 || !feedback::valid(d.conversation))return false;
        const auto& command=d.graph->steps[0].commands[0];
        if(command.operation!=coo::Operation::dialogue || command.wait!=coo::Wait::nativeReady
            || command.argument!=d.conversation.selector
            || command.asset!=coo::Asset{d.registry->key,d.conversation.definition,53,d.slot})return false;
        unsigned matches{};
        for(const auto& s:d.registry->slots)if(s.index==d.slot) {
            if(s.type!=53 || s.componentClass!=0x80804F4B || s.authSchema!=0x80804F77
                || s.descriptorTag!=d.conversation.definition)return false;
            ++matches;
        }
        return matches==1;
    }
    [[nodiscard]] bool begin(const Definition& d,const Context& c) noexcept {
        if(definition_ || !valid(d) || !c.owner || !c.boot || !c.definitionRevision
            || !c.selectionRevision || !c.event || c.activity<0 || !c.arrived || !c.admitted
            || c.bubble!=d.registry->bubble)return false;
        feedback::Ticket ticket{};
        ticket.owner=c.owner;ticket.boot=c.boot;ticket.definitionRevision=c.definitionRevision;
        ticket.selectionRevision=c.selectionRevision;ticket.activity=c.activity;ticket.authored=d.conversation;
        ticket.request.registry=d.registry->key;ticket.request.slot=d.slot;
        ticket.request.bankRows=d.conversation.bankRows;ticket.request.activeRow=d.conversation.row;
        ticket.request.generations[d.conversation.row]=static_cast<std::uint32_t>(c.event%0x7FFFFFFEULL)+1;
        ticket.request.scope=d.registry->bubble;
        if(!feedback::valid(ticket) || !executor_.start(*d.graph,c.event))return false;
        definition_=&d;ticket_=ticket;event_=c.event;return true;
    }
    [[nodiscard]] bool update(const Context& c) noexcept {
        if(!definition_ || failed_)return false;
        if(c.owner!=ticket_.owner || c.boot!=ticket_.boot || c.definitionRevision!=ticket_.definitionRevision
            || c.selectionRevision!=ticket_.selectionRevision || c.event!=event_ || c.activity!=ticket_.activity)return false;
        // Region eligibility pauses graph updates, never erases retained authority.
        if(!c.arrived || !c.admitted || c.bubble!=definition_->registry->bubble)return true;
        if(requested_ && !submitted_) {
            std::array<bridge::Event,1> events{};
            if(bridge::drain(ticket_,events)) {
                if(!executor_.enqueue({token_,coo::Milestone::nativeReady}))return fail();
                submitted_=true;
            }
        }
        Driver driver(*this);executor_.update(driver);
        return executor_.diagnostics().phase!=coo::Phase::failed || fail();
    }
    [[nodiscard]] bool advance(const Definition& d,const Context& c) noexcept {
        if(!definition_ || !submitted_ || failed_ || !valid(d)
            || c.owner!=ticket_.owner || c.boot!=ticket_.boot || c.definitionRevision!=ticket_.definitionRevision
            || c.selectionRevision!=ticket_.selectionRevision || c.event!=event_ || c.activity!=ticket_.activity
            || !c.arrived || !c.admitted || c.bubble!=d.registry->bubble
            || d.registry->key!=definition_->registry->key || d.slot!=definition_->slot)return false;
        auto next=ticket_;next.authored=d.conversation;next.request.activeRow=d.conversation.row;
        next.request.clearInactiveTimes=true;
        if(next.request.generations[d.conversation.row]>=0x7FFFFFFFU)return false;
        ++next.request.generations[d.conversation.row];
        if(!bridge::advance(ticket_,next))return false;
        Driver driver(*this);executor_.cancel(driver);
        if(!executor_.start(*d.graph,event_))return fail();
        definition_=&d;ticket_=next;token_={};requested_=false;submitted_=false;return true;
    }
    [[nodiscard]] bool append(feedback::wire::Batch& batch) const noexcept {
        if(!requested_)return true;
        if(batch.count>=batch.entries.size())return false;
        for(std::size_t i=0;i<batch.count;++i)
            if(batch.entries[i].registry==ticket_.request.registry && batch.entries[i].slot==ticket_.request.slot)return false;
        auto request=ticket_.request;
        if(submitted_)request.activeRow=feedback::wire::kNoRow;
        batch.entries[batch.count++]=request;return true;
    }
    [[nodiscard]] bool requested() const noexcept {return requested_;}
    [[nodiscard]] bool submitted() const noexcept {return submitted_;}
    [[nodiscard]] bool failed() const noexcept {return failed_;}
    [[nodiscard]] const feedback::Ticket& ticket() const noexcept {return ticket_;}
private:
    bool fail() noexcept {failed_=true;return false;}
    struct Driver final:coo::Services {
        Runtime& owner;explicit Driver(Runtime& value):owner(value){}
        bool publish(const coo::Command& command) noexcept override {
            if(owner.requested_ || !bridge::bind(owner.ticket_))return false;
            owner.token_=command.token;owner.requested_=true;return true;
        }
        void cancel(const coo::Command&) noexcept override {}
    };
    const Definition* definition_{};feedback::Ticket ticket_{};
    coo::Executor executor_;coo::Token token_{};std::uint64_t event_{};
    bool requested_{},submitted_{},failed_{};
};
} // namespace dawn::server::runtime::activity::public_event::dialogue
