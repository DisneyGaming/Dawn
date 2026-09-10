#pragma once
#include "adventure_native_bridge.h"
#include "world_object_runtime.h"
namespace sunrise::server::runtime::activity::public_event::directive {
namespace coo=state::activity::coo;
namespace feedback=adventure::cue_feedback;
namespace bridge=adventure::native_bridge;
struct Definition final {feedback::Ticket ticket{};const coo::Definition* graph{};};
// Non-combat event stages still use the UE's native-ready objective boundary.
// Each replacement consumes its predecessor's exact receipt before a new ring
// can publish; changing a timer/progress value is never a new insertion receipt.
class Runtime final {
public:
    [[nodiscard]] static bool valid(const Definition& d) noexcept {
        auto t=d.ticket;t.owner={1,{1}};t.boot=t.definitionRevision=t.selectionRevision=1;
        if(!feedback::valid(t) || !d.graph || !coo::Executor::valid(*d.graph)
            || d.graph->steps.size()!=1 || d.graph->steps[0].dependencies
            || d.graph->steps[0].commands.size()!=1)return false;
        const auto& c=d.graph->steps[0].commands[0];
        return c.operation==coo::Operation::objective && c.wait==coo::Wait::nativeReady
            && c.asset==coo::Asset{t.request.registry,t.definition,68,t.request.slot} && c.argument==t.request.event;
    }
    [[nodiscard]] bool begin(const Definition& d,const world_object::Context& c,
        const feedback::Ticket& previous,const feedback::wire::Timer& timer) noexcept {
        if(definition_ || !valid(d) || !c.owner || !c.boot || !c.definitionRevision || !c.selectionRevision
            || !c.event || !c.arrived || !c.admitted)return false;
        auto ticket=d.ticket;ticket.owner=c.owner;ticket.boot=c.boot;ticket.definitionRevision=c.definitionRevision;
        ticket.selectionRevision=c.selectionRevision;ticket.request.ring=static_cast<std::uint8_t>((previous.request.ring+1U)%3U);
        ticket.request.hasTimer=true;ticket.request.timer=timer;
        ticket.request.publicEvent=previous.request.publicEvent;
        if(!bridge::Mailbox::can_advance(previous,ticket) || !executor_.start(*d.graph,c.event))return false;
        definition_=&d;context_=c;ticket_=ticket;previous_=previous;return true;
    }
    [[nodiscard]] bool update(const world_object::Context& c) noexcept {
        if(!definition_ || failed_ || c.owner!=context_.owner || c.boot!=context_.boot || c.definitionRevision!=context_.definitionRevision
            || c.selectionRevision!=context_.selectionRevision || c.event!=context_.event || c.activity!=context_.activity)return false;
        if(!c.arrived || !c.admitted || c.bubble!=context_.bubble)return true;
        if(requested_ && !ready_) {
            std::array<bridge::Event,1> events{};
            if(bridge::drain(ticket_,events)) {
                if(!executor_.enqueue({token_,coo::Milestone::nativeReady}))return fail();ready_=true;
            }
        }
        Driver driver(*this);executor_.update(driver);
        return executor_.diagnostics().phase!=coo::Phase::failed || fail();
    }
    [[nodiscard]] bool requested() const noexcept {return requested_;}
    [[nodiscard]] bool ready() const noexcept {return ready_;}
    [[nodiscard]] const feedback::Ticket& ticket() const noexcept {return ticket_;}
private:
    bool fail() noexcept {failed_=true;return false;}
    struct Driver final:coo::Services {
        Runtime& owner;explicit Driver(Runtime& r):owner(r){}
        bool publish(const coo::Command& c) noexcept override {
            if(owner.requested_ || !bridge::advance(owner.previous_,owner.ticket_))return false;
            owner.token_=c.token;owner.requested_=true;return true;
        }
        void cancel(const coo::Command&) noexcept override {}
    };
    const Definition* definition_{};world_object::Context context_{};feedback::Ticket ticket_{},previous_{};
    coo::Executor executor_{};coo::Token token_{};bool requested_{},ready_{},failed_{};
};
}
