#pragma once

#include "status_effect_service.h"
#include "placement_service.h"
#include "../../../middleware/bap/activity_message/sense_update.h"
#include "../../../state/activity/coo/native_player_trigger.h"

namespace sunrise::server::runtime::activity::transit_effect {
namespace player_trigger=state::activity::coo::native_player_trigger;

struct TargetPlacement final {
    const registry::Definition* registry{};
    std::uint16_t slot{};
    std::uint32_t definitionTag{};
    std::int64_t definitionOffset{};
};
struct TargetRequest final {
    std::uint32_t registry{};
    std::uint16_t slot{};
    std::uint32_t definitionTag{};
    std::int64_t definitionOffset{};
    std::uint32_t generation{};
    bool active{};
};
struct LandingPlacement final {
    TargetPlacement placement{};
    std::uint32_t generation{1};
};
enum class ArrivalKind : std::uint8_t { none, monitor, playerTrigger };
struct ArrivalBinding final {
    ArrivalKind kind{ArrivalKind::none};
    const registry::Definition* registry{};
    std::uint16_t slot{};
    std::uint16_t volume{};
};

/** Read-only state of the transit effect service for transition logging outside this header. */
struct Diagnostics final {
    std::uint64_t cohort{};
    std::uint32_t destination{};
    std::int32_t expected{};
    std::uint16_t effect{};
    std::uint8_t arrivalKind{};
    /** Last refusal: 1 no definition/cohort, 2 stale cohort or unbegun effects, 3 route missing,
     *  6 pulse preconditions, 7 pulse request not projected, 10+n status_effect::Result n,
     *  20 schema, 21 width/root, 22 body bits, 23 body shape, 24 revision mismatch. */
    std::uint8_t lastFailure{};
    std::uint8_t targetsPending{};
    bool prepared{},targetReady{},triggerArmed{},requested{},applied{},arrivalCandidate{},arrivalQualified{};
};

struct Route final {
    std::uint32_t destination{};
    std::uint16_t effect{};
    // Authored prefab handoff interval, in the native scenario clock domain.
    std::uint64_t handoffTicks{};
    std::span<const TargetPlacement> targets{};
    ArrivalBinding arrival{};
    // Owned by the activity placement service; transit only waits for their
    // exact committed generation. Never retire a floor with teleport markers.
    std::span<const LandingPlacement> landing{};
};
struct Definition final {
    std::span<const status_effect::Capability> effects{};
    std::span<const Route> routes{};
};
[[nodiscard]] inline bool valid(const Definition& definition) noexcept {
    if(definition.effects.empty() || definition.routes.empty()
        || definition.effects.size()>status_effect::kBindingCapacity
        || definition.routes.size()>status_effect::kBindingCapacity)return false;
    for(const auto& effect:definition.effects) {
        if(!status_effect::valid(effect))return false;
        for(const auto& slot:effect.registry->slots)
            if(slot.index==effect.slot && slot.senseSchema!=0x8080954A)return false;
    }
    for(std::size_t i=0;i<definition.routes.size();++i) {
        const auto& route=definition.routes[i];
        if(!route.destination || route.effect>=definition.effects.size()
            || !route.handoffTicks || route.handoffTicks==UINT64_MAX)return false;
        if(route.targets.size()>18 || route.landing.size()>3)return false;
        for(const auto& target:route.targets) {
            if(!target.registry || !registry::valid(*target.registry))return false;
            unsigned matches{};
            for(const auto& slot:target.registry->slots)
                if(slot.index==target.slot && slot.type==4 && slot.componentClass==0x80809927
                    && slot.senseSchema==0x8080992E && slot.authSchema==0x8080992F
                    && slot.descriptorTag==target.definitionTag)++matches;
            if(matches!=1 || target.definitionOffset<=0 || target.definitionOffset>0x1000000)return false;
        }
        for(const auto& support:route.landing) {
            const auto& target=support.placement;
            if(!support.generation || !target.registry || !registry::valid(*target.registry)
                || target.definitionOffset<=0 || target.definitionOffset>0x1000000)return false;
            unsigned matches{};
            for(const auto& slot:target.registry->slots)
                if(slot.index==target.slot && slot.type==4 && slot.componentClass==0x80809927
                    && slot.senseSchema==0x8080992E && slot.authSchema==0x8080992F
                    && slot.descriptorTag==target.definitionTag)++matches;
            if(matches!=1)return false;
            for(const auto& marker:route.targets)
                if(marker.registry->key==target.registry->key && marker.slot==target.slot)return false;
        }
        if(route.arrival.kind!=ArrivalKind::none) {
            if(!route.arrival.registry || route.arrival.slot>32767)return false;
            unsigned matches{};
            for(const auto& slot:route.arrival.registry->slots)
                if(slot.index==route.arrival.slot
                    && ((route.arrival.kind==ArrivalKind::monitor && slot.type==30
                         && slot.senseSchema==0x80809531 && slot.authSchema==0x80809532)
                        || (route.arrival.kind==ArrivalKind::playerTrigger && slot.type==31
                            && slot.authSchema==0x80809524)))++matches;
            if(matches!=1)return false;
            if(route.arrival.kind==ArrivalKind::playerTrigger) {
                if(route.arrival.registry->key==0x811C9DC5U || !route.arrival.volume)return false;
            } else if(route.arrival.kind!=ArrivalKind::monitor)return false;
        }
        for(std::size_t j=0;j<i;++j)
            if(definition.routes[j].destination==route.destination)return false;
    }
    return true;
}

/** Native player effect precedes membership travel; it never manufactures arrival. */
class Service final {
public:
    [[nodiscard]] bool begin(status_effect::Owner owner,std::uint64_t boot,
        const Definition& definition) noexcept {
        if(!valid(definition) || !effects_.begin(owner,boot,definition.effects))return false;
        definition_=&definition;return true;
    }
    [[nodiscard]] bool prepare(std::uint64_t cohort,std::uint32_t destination) noexcept {
        if(!definition_ || !cohort){lastFailure_=1;return false;}
        if(cohort==cohort_)return route_ && route_->destination==destination;
        if(cohort<cohort_ || effects_.last_request()==UINT64_MAX){lastFailure_=2;return false;}
        const Route* selected{};
        for(const auto& route:definition_->routes)if(route.destination==destination)selected=&route;
        if(!selected){lastFailure_=3;return false;}
        for(std::size_t i=0;i<targetCount_;++i) {
            auto& target=targets_[i];
            if(target.active) {
                if(target.generation==UINT32_MAX)return false;
                target.active=false;target.ready=false;++target.generation;
            }
        }
        route_=selected;cohort_=cohort;appliedAt_=UINT64_MAX;requested_=false;
        landingReady_={};
        targetReady_=selected->targets.empty() && selected->landing.empty();arrivalCandidate_=false;arrivalQualified_=false;triggerArmed_=false;
        for(const auto& authored:selected->targets) {
            TargetState* target{};
            for(std::size_t i=0;i<targetCount_;++i)
                if(targets_[i].placement.registry==authored.registry
                    && targets_[i].placement.slot==authored.slot)target=&targets_[i];
            if(!target) {
                if(targetCount_==targets_.size())return false;
                target=&targets_[targetCount_++];target->placement=authored;target->generation=0;
            }
            if(target->generation==UINT32_MAX)return false;
            target->active=true;target->ready=false;++target->generation;
        }
        return true;
    }
    [[nodiscard]] bool pulse(std::uint64_t cohort) noexcept {
        if(requested_)return false;
        if(!route_ || cohort!=cohort_ || !targetReady_){lastFailure_=6;return false;}
        const auto result=effects_.pulse({effects_.owner(),effects_.boot(),effects_.revision(),
            effects_.last_request()+1,route_->effect,true});
        if(result!=status_effect::Result::accepted){
            lastFailure_=static_cast<std::uint8_t>(10U+static_cast<unsigned>(result));return false;
        }
        requested_=true;
        const auto& capability=definition_->effects[route_->effect];
        const auto batch=effects_.project(capability.registry->bubble);
        const auto* request=status_effect::wire::find(batch,capability.registry->key,26,capability.slot);
        if(!request){lastFailure_=7;return false;}
        lastFailure_=0;expected_=request->selectionRevision;return true;
    }
    /** Compatibility entry point for effect-only consumers. Target routes use prepare/pulse. */
    [[nodiscard]] bool request(std::uint64_t cohort,std::uint32_t destination) noexcept {
        if(route_ && cohort==cohort_ && route_->destination==destination && requested_)return true;
        if(!prepare(cohort,destination))return false;
        return route_->targets.empty() ? pulse(cohort) : true;
    }
    [[nodiscard]] bool native_targets() const noexcept {return route_ && !route_->targets.empty();}
    [[nodiscard]] bool requested() const noexcept {return requested_;}
    [[nodiscard]] bool trigger_armed() const noexcept {return triggerArmed_;}
    [[nodiscard]] bool trigger_required() const noexcept {
        return route_ && route_->arrival.kind==ArrivalKind::playerTrigger;
    }
    [[nodiscard]] bool target_ready(std::uint64_t cohort) const noexcept {
        return route_ && cohort==cohort_ && targetReady_;
    }
    /** Copies only unresolved target generations; the caller owns the bounded output storage. */
    [[nodiscard]] std::size_t pending_targets(std::span<TargetRequest> output) const noexcept {
        if(!route_)return 0;
        std::size_t pending{};
        for(std::size_t i=0;i<targetCount_;++i)pending+=targets_[i].ready?0U:1U;
        for(std::size_t i=0;i<route_->landing.size();++i)pending+=landingReady_[i]?0U:1U;
        if(pending>output.size())return 0;
        std::size_t count{};
        for(std::size_t i=0;i<targetCount_;++i) {
            const auto& target=targets_[i];
            if(target.ready)continue;
            if(count==output.size())return 0;
            output[count++]={target.placement.registry->key,target.placement.slot,
                target.placement.definitionTag,target.placement.definitionOffset,
                target.generation,target.active};
        }
        for(std::size_t i=0;i<route_->landing.size();++i) {
            if(landingReady_[i])continue;
            const auto& support=route_->landing[i];const auto& target=support.placement;
            output[count++]={target.registry->key,target.slot,target.definitionTag,
                target.definitionOffset,support.generation,true};
        }
        return count;
    }
    [[nodiscard]] std::uint32_t target_generation(std::uint32_t registry,std::uint16_t slot) const noexcept {
        for(std::size_t i=0;i<targetCount_;++i)
            if(targets_[i].placement.registry->key==registry && targets_[i].placement.slot==slot)
                return targets_[i].generation;
        return 0;
    }
    [[nodiscard]] bool observe_target(std::uint32_t registry,std::uint16_t slot,
        std::uint32_t generation,bool active) noexcept {
        for(std::size_t i=0;i<targetCount_;++i) {
            auto& target=targets_[i];
            if(target.placement.registry->key!=registry || target.placement.slot!=slot
                || target.generation!=generation || target.active!=active)continue;
            target.ready=true;
            targetReady_=true;
            for(std::size_t j=0;j<targetCount_;++j)
                if(!targets_[j].ready)targetReady_=false;
            for(std::size_t j=0;j<route_->landing.size();++j)
                if(!landingReady_[j])targetReady_=false;
            return true;
        }
        return false;
    }
    [[nodiscard]] bool observe_target(const TargetRequest& request) noexcept {
        if(!route_)return false;
        for(std::size_t i=0;i<route_->landing.size();++i) {
            const auto& support=route_->landing[i];const auto& target=support.placement;
            if(target.registry->key!=request.registry || target.slot!=request.slot
                || target.definitionTag!=request.definitionTag || target.definitionOffset!=request.definitionOffset
                || support.generation!=request.generation || !request.active)continue;
            landingReady_[i]=true;targetReady_=true;
            for(std::size_t j=0;j<route_->landing.size();++j)if(!landingReady_[j])targetReady_=false;
            for(std::size_t j=0;j<targetCount_;++j)if(!targets_[j].ready)targetReady_=false;
            return true;
        }
        for(std::size_t i=0;i<targetCount_;++i) {
            const auto& target=targets_[i];
            if(target.placement.registry->key!=request.registry || target.placement.slot!=request.slot
                || target.placement.definitionTag!=request.definitionTag
                || target.placement.definitionOffset!=request.definitionOffset
                || target.generation!=request.generation || target.active!=request.active)continue;
            return observe_target(request.registry,request.slot,request.generation,request.active);
        }
        return false;
    }
    [[nodiscard]] bool append_targets(placement::wire::Batch& output,std::uint32_t bubble) const noexcept {
        if(output.count>output.entries.size())return false;
        for(std::size_t i=0;i<targetCount_;++i) {
            const auto& target=targets_[i];
            if(target.placement.registry->bubble!=bubble)continue;
            if(placement::wire::find(output,target.placement.registry->key,4,target.placement.slot))return false;
            if(output.count==output.entries.size())return false;
            auto& request=output.entries[output.count++];
            request={target.placement.registry->key,target.placement.slot,static_cast<std::uint8_t>(bubble),
                placement::interaction::Mode::unchanged,target.generation,std::nullopt,target.active,std::nullopt};
        }
        return true;
    }
    [[nodiscard]] bool append_trigger(player_trigger::Batch& output) const noexcept {
        if(!route_ || route_->arrival.kind!=ArrivalKind::playerTrigger)return true;
        if(!player_trigger::append(output,route_->arrival.registry->key,route_->arrival.slot,
            route_->arrival.registry->bubble,
            targetGenerationForTrigger()))return false;
        triggerArmed_=true;return true;
    }
    [[nodiscard]] bool observe(status_effect::Owner owner,std::uint64_t boot,std::uint32_t bubble,
        const middleware::bap::activity_message::sense_update::SenseObject& object,
        std::uint64_t clock) noexcept {
        if(!route_ || owner!=effects_.owner() || boot!=effects_.boot() || clock==UINT64_MAX)return false;
        if(route_->arrival.kind==ArrivalKind::monitor && observe_monitor(bubble,object))return true;
        if(appliedAt_!=UINT64_MAX)return false;
        const auto& capability=definition_->effects[route_->effect];
        if(bubble!=capability.registry->bubble || object.registryKey!=capability.registry->key
            || object.slotIndex!=capability.slot || object.slotType!=26)return false;
        if(!object.hasNativeSchema || object.nativeSchema!=0x8080954A){lastFailure_=20;return false;}
        if(object.inferredBodyWidth || !object.hasRootDelta){lastFailure_=21;return false;}
        if(object.bodyBits!=130){lastFailure_=22;return false;}
        if(!(object.bodyFirst>>63) || object.bodyThird>3 || object.bodyFourth){lastFailure_=23;return false;}
        // MSB-first root + s32 + s32 + s32 + bool + revision32.
        // Native 9EF8A0 echoes authority+10 at sense+8 only after its apply pass.
        const auto code=static_cast<std::uint32_t>(object.bodySecond>>31);
        const auto applied=static_cast<std::int64_t>(code)-2147483648LL;
        if(applied!=expected_){lastFailure_=24;return false;}
        lastFailure_=0;appliedAt_=clock;
        if(arrivalCandidate_ && requested_)arrivalQualified_=true;
        return true;
    }
    [[nodiscard]] bool observe_player_trigger(std::uint32_t registry,std::uint16_t slot,
        std::uint32_t object) noexcept {
        if(!route_ || route_->arrival.kind!=ArrivalKind::playerTrigger || !requested_
            || arrivalQualified_ || registry!=route_->arrival.registry->key
            || slot!=route_->arrival.slot || !object)return false;
        arrivalQualified_=true;return true;
    }
    [[nodiscard]] bool arrival_qualified() const noexcept {return arrivalQualified_;}
    [[nodiscard]] bool arrival_qualified(std::uint64_t cohort,std::uint32_t destination) const noexcept {
        return route_ && cohort==cohort_ && destination==route_->destination
            && requested_ && arrivalQualified_;
    }
    [[nodiscard]] bool ready(std::uint64_t cohort,std::uint64_t clock) const noexcept {
        return route_ && cohort==cohort_ && requested_ && appliedAt_!=UINT64_MAX && clock!=UINT64_MAX
            && clock>=appliedAt_ && clock-appliedAt_>=route_->handoffTicks;
    }
    [[nodiscard]] bool append(status_effect::wire::Batch& output,std::uint32_t bubble) const noexcept {
        const auto batch=effects_.project(bubble);
        if(output.count>output.entries.size() || batch.count>output.entries.size()-output.count)return false;
        for(std::size_t i=0;i<batch.count;++i) {
            const auto& entry=batch.entries[i];
            if(status_effect::wire::find(output,entry.registry,26,entry.slot))return false;
            output.entries[output.count++]=entry;
        }
        return true;
    }
    void release_owner() noexcept {*this={};}
    [[nodiscard]] Diagnostics diagnostics() const noexcept {
        Diagnostics output{};
        output.cohort=cohort_;output.expected=expected_;output.lastFailure=lastFailure_;
        output.prepared=route_!=nullptr;
        if(route_){
            output.destination=route_->destination;output.effect=route_->effect;
            output.arrivalKind=static_cast<std::uint8_t>(route_->arrival.kind);
        }
        output.targetReady=targetReady_;output.triggerArmed=triggerArmed_;output.requested=requested_;
        output.applied=appliedAt_!=UINT64_MAX;output.arrivalCandidate=arrivalCandidate_;
        output.arrivalQualified=arrivalQualified_;
        for(std::size_t i=0;i<targetCount_;++i)if(!targets_[i].ready && output.targetsPending<255)++output.targetsPending;
        if(route_)for(std::size_t i=0;i<route_->landing.size();++i)if(!landingReady_[i])++output.targetsPending;
        return output;
    }
private:
    struct TargetState final {TargetPlacement placement{};std::uint32_t generation{};bool active{},ready{};};
    [[nodiscard]] std::uint32_t targetGenerationForTrigger() const noexcept {
        if(!route_ || route_->targets.empty())return 1;
        for(std::size_t i=0;i<targetCount_;++i)
            for(const auto& authored:route_->targets)
                if(targets_[i].placement.registry==authored.registry
                    && targets_[i].placement.slot==authored.slot)return targets_[i].generation;
        return 0;
    }
    [[nodiscard]] bool observe_monitor(std::uint32_t bubble,
        const middleware::bap::activity_message::sense_update::SenseObject& object) noexcept {
        if(!route_ || route_->arrival.kind!=ArrivalKind::monitor
            || bubble!=route_->arrival.registry->bubble
            || object.registryKey!=route_->arrival.registry->key || object.slotIndex!=route_->arrival.slot
            || object.slotType!=30 || !object.hasNativeSchema || object.nativeSchema!=0x80809531
            || object.inferredBodyWidth || !object.hasRootDelta || !object.hasMonitorOutput
            || !object.monitorOutput.any || !object.monitorOutput.all || object.monitorOutput.count<=0
            || !object.nativeRevision)return false;
        // Each authored monitor has an independent native revision stream.
        // A boss-room revision must not suppress a lower return-platform revision.
        auto& revision=arrivalRevisions_[static_cast<std::size_t>(route_-definition_->routes.data())];
        if(object.nativeRevision<=revision)return false;
        revision=object.nativeRevision;arrivalCandidate_=true;
        if(requested_)arrivalQualified_=true;
        return true;
    }
    const Definition* definition_{};
    status_effect::Service effects_{};
    const Route* route_{};
    std::uint64_t cohort_{},appliedAt_{UINT64_MAX};
    std::int32_t expected_{};
    std::array<TargetState,18> targets_{};
    std::array<bool,3> landingReady_{};
    std::size_t targetCount_{};
    mutable bool triggerArmed_{};
    bool targetReady_{},requested_{},arrivalCandidate_{},arrivalQualified_{};
    std::array<std::uint32_t,status_effect::kBindingCapacity> arrivalRevisions_{};
    std::uint8_t lastFailure_{};
};
}
