#pragma once
#include "registry_admission.h"
#include "../../../middleware/bap/activity_message/native/world_sequence_authority.h"
#include "../../../state/activity/lifecycle_generation.h"
#include "../../../state/activity/coo/executor.h"

namespace sunrise::server::runtime::activity::public_event::sequence {
namespace wire=middleware::bap::activity_message::native::world_sequence;
namespace coo=state::activity::coo;
using Owner=state::activity::ActivityInstanceKey;
struct Definition final {
    const registry::Definition* registry{};
    const coo::Definition* graph{};
    std::uint16_t slot{};
    // Evidence qualification for the bounded no-parameter authority contract.
    std::uint32_t definition{},entity{},parameterRows{};
};
struct Context final {
    Owner owner{};std::uint64_t boot{},definitionRevision{},selectionRevision{},event{};
    std::uint64_t clockTicks{UINT64_MAX};
    std::int16_t activity{-1};std::uint32_t bubble{UINT32_MAX};bool arrived{},admitted{};
};
class Runtime final {
public:
    [[nodiscard]] static bool valid(const Definition& d) noexcept {
        if(!d.registry || !registry::valid(*d.registry) || !d.graph || !coo::Executor::valid(*d.graph)
            || d.graph->schema!=coo::Schema::otherMissions || d.graph->steps.size()!=1
            || d.graph->steps[0].dependencies || d.graph->steps[0].commands.size()!=1
            || d.parameterRows || !d.entity || d.entity==UINT32_MAX || d.entity==wire::kAbsent)return false;
        const auto& command=d.graph->steps[0].commands[0];
        if(command.operation!=coo::Operation::scene || command.wait!=coo::Wait::requested || command.argument!=1
            || command.asset!=coo::Asset{d.registry->key,d.definition,5,d.slot})return false;
        unsigned matches{};for(const auto& slot:d.registry->slots)if(slot.index==d.slot) {
            if(slot.type!=5 || slot.componentClass!=0x80804F01 || slot.authSchema!=wire::kSchema
                || slot.descriptorTag!=d.definition)return false;
            ++matches;
        }
        return matches==1;
    }
    [[nodiscard]] bool begin(const Definition& d,const Context& c) noexcept {
        if(definition_ || !valid(d) || !c.owner || !c.boot || !c.definitionRevision || !c.selectionRevision
            || !c.event || c.clockTicks==UINT64_MAX || c.activity<0 || c.bubble!=d.registry->bubble
            || !c.arrived || !c.admitted || !executor_.start(*d.graph,c.event))return false;
        definition_=&d;owner_=c;request_={d.registry->key,d.slot,d.registry->bubble,1,c.clockTicks,UINT64_MAX};return true;
    }
    [[nodiscard]] bool update(const Context& c) noexcept {
        if(!same(c) || failed_)return false;
        if(!c.arrived || !c.admitted || c.bubble!=definition_->registry->bubble || stopped_)return true;
        Driver driver(*this);executor_.update(driver);
        if(executor_.diagnostics().phase==coo::Phase::failed){failed_=true;return false;}
        return true;
    }
    [[nodiscard]] bool withdraw(const Context& c) noexcept {
        if(!same(c) || !published_ || c.clockTicks==UINT64_MAX || c.clockTicks<request_.startTicks)return false;
        // Native FF generation explicitly removes the existing authored entity.
        // Repeating withdrawal retains the first exact authority record.
        if(!stopped_){request_.generation=255;request_.endTicks=c.clockTicks;stopped_=true;}
        return true;
    }
    [[nodiscard]] bool append(wire::Batch& output) const noexcept {
        if(!published_)return true;
        if(output.count>=output.entries.size())return false;
        for(std::size_t i=0;i<output.count;++i)if(output.entries[i].registry==request_.registry && output.entries[i].slot==request_.slot)return false;
        output.entries[output.count++]=request_;return true;
    }
    [[nodiscard]] bool requested() const noexcept {return published_;}
    [[nodiscard]] bool stopped() const noexcept {return stopped_;}
private:
    [[nodiscard]] bool same(const Context& c) const noexcept {
        return definition_ && c.owner==owner_.owner && c.boot==owner_.boot && c.definitionRevision==owner_.definitionRevision
            && c.selectionRevision==owner_.selectionRevision && c.event==owner_.event && c.activity==owner_.activity;
    }
    struct Driver final:coo::Services {
        Runtime& owner;explicit Driver(Runtime& value):owner(value){}
        bool publish(const coo::Command&) noexcept override {
            if(owner.published_)return false;owner.published_=true;return true;
        }
        void cancel(const coo::Command&) noexcept override {}
    };
    const Definition* definition_{};Context owner_{};wire::Request request_{};coo::Executor executor_;
    bool published_{},stopped_{},failed_{};
};
} // namespace sunrise::server::runtime::activity::public_event::sequence
