#pragma once
#include "adventure_native_bridge.h"
#include "population_service.h"
#include "../../../state/activity/native_population_events.h"

namespace sunrise::server::runtime::activity::public_event {
namespace opening_cue=adventure::cue_feedback;
namespace opening_bridge=adventure::native_bridge;
namespace opening_native=state::activity::native_population;
namespace opening_coo=state::activity::coo;
struct OpeningDefinition final {
    const registry::Definition* registry{};
    const opening_coo::Definition* graph{};
    opening_cue::Ticket cue{};
    std::span<const population::Capability> sources{};
    std::uint32_t defeatGoal{};
};
struct OpeningFrame final {
    opening_cue::wire::Request cue{};
    bool requested{},nativeReady{},combatRequested{},combatReady{},conflictingSelection{},failed{};
};
// A bounded opening encounter on the reusable UE. Its optional defeat goal is
// satisfied only by accepted native deaths from this encounter's sources.
// Source admission, percentage completion and source retirement stay separate.
class OpeningRuntime final {
public:
    [[nodiscard]] static bool valid(const OpeningDefinition& d) noexcept {
        if(!d.registry || !registry::valid(*d.registry) || !d.graph || !opening_coo::Executor::valid(*d.graph)
            || d.graph->schema!=opening_coo::Schema::otherMissions || d.graph->steps.size()!=(d.defeatGoal?3U:2U)
            || d.defeatGoal>128
            || d.sources.empty() || d.sources.size()>2 || d.graph->steps[0].commands.size()!=1
            || d.graph->steps[0].dependencies || d.graph->steps[1].dependencies!=1
            || d.graph->steps[1].commands.size()!=d.sources.size())return false;
        auto ticket=d.cue;ticket.owner={1,{1}};ticket.boot=ticket.definitionRevision=ticket.selectionRevision=1;
        if(!opening_cue::valid(ticket) || ticket.request.scope!=d.registry->bubble || ticket.request.registry!=d.registry->key)return false;
        const auto& first=d.graph->steps[0].commands[0];
        if(first.operation!=opening_coo::Operation::objective || first.wait!=opening_coo::Wait::nativeReady
            || first.argument!=ticket.request.event || first.asset!=opening_coo::Asset{d.registry->key,ticket.definition,68,ticket.request.slot})return false;
        if(d.defeatGoal){
            const auto& step=d.graph->steps[2];
            if(step.dependencies!=2 || step.commands.size()!=1)return false;
            const auto& command=step.commands[0];
            if(command.operation!=opening_coo::Operation::observation || command.asset!=first.asset
                || command.argument!=d.defeatGoal || command.wait!=opening_coo::Wait::observed)return false;
        }
        for(std::size_t step=0;step<d.graph->steps.size();++step)
            for(const auto& command:d.graph->steps[step].commands){
                unsigned matches{};for(const auto& slot:d.registry->slots)
                    if(slot.index==command.asset.slot && slot.type==command.asset.type && slot.descriptorTag==command.asset.definition)++matches;
                if(command.asset.registry!=d.registry->key || matches!=1)return false;
            }
        for(std::size_t i=0;i<d.sources.size();++i){const auto& source=d.sources[i];const auto& command=d.graph->steps[1].commands[i];
            if(!population::valid(source) || source.registry!=d.registry || command.asset.slot!=source.slot
                || command.operation!=opening_coo::Operation::population || command.wait!=opening_coo::Wait::nativeReady
                || !command.argument || command.argument>63)return false;
            for(std::size_t j=0;j<i;++j)if(d.sources[j].slot==source.slot)return false;
        }
        return true;
    }
    [[nodiscard]] bool begin(const OpeningDefinition& definition,opening_cue::Owner owner,std::uint64_t boot,
        std::uint64_t definitionRevision,std::uint64_t selectionRevision,std::uint64_t event,
        population::Service& population,const opening_cue::Ticket* previous=nullptr) noexcept {
        if(definition_ || !valid(definition) || !event || population.owner()!=owner || population.boot()!=boot)return false;
        auto ticket=definition.cue;ticket.owner=owner;ticket.boot=boot;ticket.definitionRevision=definitionRevision;ticket.selectionRevision=selectionRevision;
        if(definition.defeatGoal){ticket.request.hasProgress=true;ticket.request.progress={0,static_cast<std::int32_t>(definition.defeatGoal)};}
        if(previous && !opening_bridge::Mailbox::can_advance(*previous,ticket))return false;
        if(!opening_cue::valid(ticket) || owner.incarnation.value>0x7FFFFFFFU || !executor_.start(*definition.graph,event))return false;
        definition_=&definition;ticket_=ticket;population_=&population;if(previous)previous_=*previous;return true;
    }
    [[nodiscard]] OpeningFrame update(std::uint32_t bubble,bool arrived,bool admitted,
        std::int16_t selectedActivity,std::uint64_t selectedRevision) noexcept {
        if(!definition_)return {};
        if(selectedActivity!=ticket_.activity || selectedRevision!=ticket_.selectionRevision)conflicting_=true;
        if(!failed_ && !conflicting_ && arrived && admitted && bubble==definition_->registry->bubble){
            std::array<opening_bridge::Event,1> receipts{};
            const auto n=opening_bridge::drain(ticket_,receipts);
            for(std::size_t i=0;i<n;++i){
                if(!requested_ || receipts[i].observation.ticket!=ticket_ || receipts[i].binding.ticket!=ticket_
                    || !executor_.enqueue({cueToken_,opening_coo::Milestone::nativeReady})){failed_=true;break;}
                ready_=true;
            }
            if(!failed_){
                Driver driver(*this);executor_.update(driver);failed_=executor_.diagnostics().phase==opening_coo::Phase::failed;
                if(!failed_ && defeatRequested_ && !defeatCompleted_ && defeated()>=definition_->defeatGoal){
                    if(!executor_.enqueue({defeatToken_,opening_coo::Milestone::observed}))failed_=true;
                    else {
                        defeatCompleted_=true;
                        // Consume the real observation in this authority turn;
                        // do not wait for an unrelated periodic roster refresh.
                        executor_.update(driver);failed_=executor_.diagnostics().phase==opening_coo::Phase::failed;
                    }
                }
            }
        }
        return frame();
    }
    // Called only by the central authoritative native population intake AFTER
    // its existing ledger accepts the real event. This class never drains that
    // mailbox or derives an actor/death from publication, absence or a timer.
    [[nodiscard]] bool observe_accepted(const opening_native::Event& event,opening_coo::PopulationIntake intake) noexcept {
        if(!definition_ || failed_ || conflicting_ || intake!=opening_coo::PopulationIntake::accepted || !ready_
            || !event.actor.valid() || event.sourceHandle==UINT32_MAX || event.lease.activity!=ticket_.owner
            || event.lease.bubble!=definition_->registry->bubble || event.actor.owner!=event.lease.source)return false;
        for(std::size_t i=0;i<definition_->sources.size();++i){
            const auto& expected=definition_->graph->steps[1].commands[i].asset;
            const opening_coo::PopulationOwner owner{ticket_.owner.sessionId,ticket_.boot,ticket_.owner.incarnation.value,expected,
                static_cast<std::uint32_t>(ticket_.owner.incarnation.value)};
            if(event.lease.source!=owner)continue;
            if(!sourceRequested_[i] || (sourceHandle_[i]!=UINT32_MAX && sourceHandle_[i]!=event.sourceHandle))return false;
            sourceHandle_[i]=event.sourceHandle;
            auto& ledger=ledgers_[i];
            if(event.kind==opening_native::Kind::died)
                return ledger.died(event.actor)==opening_coo::PopulationIntake::accepted;
            if(event.kind==opening_native::Kind::retired)
                return ledger.actor_retired(event.actor)==opening_coo::PopulationIntake::accepted;
            if(event.kind!=opening_native::Kind::admitted)return false;
            const auto accepted=ledger.admitted(event.actor);
            if(accepted!=opening_coo::PopulationIntake::accepted){
                if(accepted==opening_coo::PopulationIntake::overflow || accepted==opening_coo::PopulationIntake::conflict)failed_=true;
                return false;
            }
            if(sourceAdmitted_[i])return true;
            if(!executor_.enqueue({sourceTokens_[i],opening_coo::Milestone::nativeReady})){failed_=true;return false;}
            sourceAdmitted_[i]=true;return true;
        }
        return false;
    }
    [[nodiscard]] OpeningFrame frame() const noexcept {
        bool requested=definition_!=nullptr,admitted=requested;
        if(definition_)for(std::size_t i=0;i<definition_->sources.size();++i){requested&=sourceRequested_[i];admitted&=sourceAdmitted_[i];}
        auto cue=ticket_.request;
        if(definition_ && definition_->defeatGoal){
            const auto count=defeated();
            cue.progress.current=static_cast<std::int32_t>(count<definition_->defeatGoal?count:definition_->defeatGoal);
        }
        return {cue,requested_,ready_,requested,admitted,conflicting_,failed_};
    }
    [[nodiscard]] const opening_cue::Ticket& ticket() const noexcept{return ticket_;}
    // The incoming coordinator already consumed this exact native manager
    // receipt. Transfer it to this graph without inserting the directive again.
    [[nodiscard]] bool adopt_cue_receipt(const opening_bridge::Event& receipt) noexcept {
        if(!definition_ || !requested_ || ready_ || failed_ || conflicting_
            || !receipt.binding.epoch || !receipt.observation.sequence
            || receipt.binding.ticket!=ticket_ || receipt.observation.ticket!=ticket_
            || receipt.observation.source.member==UINT32_MAX
            || !executor_.enqueue({cueToken_,opening_coo::Milestone::nativeReady}))return false;
        ready_=true;return true;
    }
    [[nodiscard]] std::size_t defeated() const noexcept {
        std::size_t count{};for(const auto& ledger:ledgers_)count+=ledger.counts().dead;return count;
    }
    [[nodiscard]] std::size_t defeated(std::size_t source) const noexcept {
        return source<ledgers_.size()?ledgers_[source].counts().dead:0;
    }
    [[nodiscard]] bool defeat_goal_completed() const noexcept {
        return defeatCompleted_ && executor_.diagnostics().phase==opening_coo::Phase::complete;
    }
private:
    struct Driver final:opening_coo::Services {
        OpeningRuntime& owner;explicit Driver(OpeningRuntime& value):owner(value){}
        bool publish(const opening_coo::Command& command) noexcept override {
            if(command.spec.operation==opening_coo::Operation::objective){
                if(owner.requested_)return false;
                const bool bound=owner.previous_.owner?opening_bridge::advance(owner.previous_,owner.ticket_):opening_bridge::bind(owner.ticket_);
                if(!bound)return false;
                owner.cueToken_=command.token;owner.requested_=true;return true;
            }
            if(command.spec.operation==opening_coo::Operation::observation){
                if(owner.defeatRequested_ || !owner.definition_->defeatGoal)return false;
                owner.defeatToken_=command.token;owner.defeatRequested_=true;return true;
            }
            for(std::size_t i=0;i<owner.definition_->sources.size();++i)
                if(command.spec.asset==owner.definition_->graph->steps[1].commands[i].asset){
                    if(owner.sourceRequested_[i] || !owner.ready_ || owner.population_->last_request()==UINT64_MAX)return false;
                    const population::Command request{owner.ticket_.owner,owner.population_->revision(),owner.population_->last_request()+1,
                        command.spec.asset.registry,command.spec.asset.slot,static_cast<std::uint8_t>(command.spec.argument),owner.ticket_.boot};
                    const opening_coo::PopulationOwner source{owner.ticket_.owner.sessionId,owner.ticket_.boot,
                        owner.ticket_.owner.incarnation.value,command.spec.asset,static_cast<std::uint32_t>(owner.ticket_.owner.incarnation.value)};
                    if(!owner.ledgers_[i].begin(source))return false;
                    if(owner.population_->request(request,owner.definition_->registry->bubble)!=population::Result::accepted)return false;
                    owner.sourceTokens_[i]=command.token;owner.sourceRequested_[i]=true;return true;
                }
            return false;
        }
        void cancel(const opening_coo::Command&) noexcept override {}
    };
    const OpeningDefinition* definition_{};population::Service* population_{};
    opening_cue::Ticket ticket_{},previous_{};opening_coo::Executor executor_;opening_coo::Token cueToken_{};
    std::array<opening_coo::Token,2> sourceTokens_{};
    std::array<bool,2> sourceRequested_{},sourceAdmitted_{};
    std::array<std::uint32_t,2> sourceHandle_{UINT32_MAX,UINT32_MAX};
    std::array<opening_coo::NativePopulationLedger<64>,2> ledgers_{};
    opening_coo::Token defeatToken_{};
    bool defeatRequested_{},defeatCompleted_{};
    bool requested_{},ready_{},conflicting_{},failed_{};
};
}
