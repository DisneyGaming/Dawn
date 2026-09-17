#pragma once
#include "registry_admission.h"
#include "../../../state/activity/lifecycle_generation.h"
#include <array>
#include "../../../state/activity/coo/task_costs.h"
#include <charconv>
#include <cstdint>
#include <span>
#include <string_view>
#include "../../../middleware/bap/activity_message/sense_update.h"

namespace sunrise::server::runtime::activity::population {
namespace wire=middleware::bap::activity_message::native::population;
namespace codec=middleware::bap::activity_message::native::combatant_source;
using Owner=state::activity::ActivityInstanceKey;

inline constexpr std::uint32_t kMaximumGeneration=0x7FFFFFFFU;
inline constexpr std::size_t kBindingCapacity=wire::kPopulationCapacity;
inline constexpr std::size_t kSourceCapacity=wire::kSourceCapacity;
inline constexpr std::uint16_t kNoNamedMember=wire::kNoNamedMember;

struct Capability final {
    const registry::Definition* registry{};
    std::uint16_t slot{},rule{};
    codec::TacticalGroup tactical{};
    bool hasRule{true};
    std::uint32_t taskMask{};
    bool allowCycles{};
    std::uint16_t namedMember{kNoNamedMember};
    // Authored category-array width; categories may remain dormant.
    std::uint8_t categories{1};
    // Ordinary recurring patrol policy. Adventure/Lost Sector replay uses
    // allowCycles but must not inherit patrol casualty-rehydration semantics.
    bool recurringRehydration{};
};

[[nodiscard]] inline bool valid(const Capability& capability) noexcept {
    if(!capability.registry || !registry::valid(*capability.registry)) return false;
    unsigned source{},rule{},tactical{},member{};
    for(const auto& slot:capability.registry->slots) {
        if(slot.index==capability.slot && slot.type==1 && slot.authSchema==0x80807EC9) ++source;
        if(slot.index==capability.rule && slot.type==66) ++rule;
        if(slot.index==capability.tactical.slot && slot.type==3 && slot.authSchema==0x80807F0C) ++tactical;
        if(slot.index==capability.namedMember && slot.type==2
            && slot.componentClass==0x8080834EU && slot.senseSchema==0x80807DA2U
            && slot.authSchema==0x80807DA1U) ++member;
    }
    codec::Source request{capability.registry->key,1,capability.rule,1,capability.tactical};
    request.hasSpawnRule=capability.hasRule;
    request.hasSecondCategory=capability.categories>1;
    request.categoryCount=capability.categories;
    if(capability.taskMask && (capability.tactical.row<0 || capability.tactical.row>=24 || (capability.taskMask&0xFF000000U)
        || !(capability.taskMask&(1U<<capability.tactical.row))))return false;
    const bool named=capability.namedMember!=kNoNamedMember;
    return capability.categories>=1 && capability.categories<=codec::kMaximumCategories
        && source==1 && (!capability.hasRule || rule==1)
        && (capability.tactical.row<0 || (capability.tactical.registry==capability.registry->key && tactical==1))
        && (!named || member==1) && codec::valid(request);
}

struct Command final {
    Owner owner{};
    std::uint64_t expectedRevision{},request{};
    std::uint32_t registry{};
    std::uint16_t slot{};
    std::uint8_t requested{};
    std::uint64_t boot{};
    std::uint8_t secondRequested{};
    std::array<std::uint8_t,6> additionalRequested{};
    std::uint8_t categoryCount{};
};
enum class Phase : std::uint8_t { inactive, active, retiring, retired };
using PopulationPhase=Phase;
enum class Result : std::uint8_t {
    accepted, invalid, stale, duplicate, unsupported, decrease, unchanged, exhausted,
    notAllowed, notAcknowledged
};
struct Status final {
    std::uint32_t generation{};
    std::uint32_t cycle{};
    std::uint32_t requested{};
    Phase phase{Phase::inactive};
    bool published{};
    bool nativeAckNeeded{};
    bool retirementAcknowledged{};
};

// Explicit local development format. Absolute cumulative targets are idempotent;
// no reset, delete, teleport, arbitrary pointer or fabricated receipt operation.
// v2 BOOT_HEX OWNER_HEX INCARNATION REVISION REQUEST REGISTRY_HEX SLOT TARGET
[[nodiscard]] inline bool parse(std::string_view text,Command& output) noexcept {
    std::array<std::string_view,9> tokens{};std::size_t used{};
    while(!text.empty()) {
        const auto begin=text.find_first_not_of(" \t\r\n");
        if(begin==std::string_view::npos) break;
        text.remove_prefix(begin);
        const auto end=text.find_first_of(" \t\r\n");
        if(used==tokens.size()) return false;
        tokens[used++]=text.substr(0,end);
        if(end==std::string_view::npos) break;
        text.remove_prefix(end);
    }
    if(used!=9 || tokens[0]!="v2") return false;
    std::uint64_t boot{};
    const auto parsedBoot=std::from_chars(tokens[1].data(),tokens[1].data()+tokens[1].size(),boot,16);
    if(parsedBoot.ec!=std::errc{} || parsedBoot.ptr!=tokens[1].data()+tokens[1].size() || !boot) return false;
    std::array<std::uint64_t,7> values{};
    for(std::size_t i=0;i<values.size();++i) {
        const auto token=tokens[i+2];
        const auto result=std::from_chars(token.data(),token.data()+token.size(),values[i],(i==0 || i==4)?16:10);
        if(result.ec!=std::errc{} || result.ptr!=token.data()+token.size()) return false;
    }
    if(!values[0] || !values[1] || !values[2] || !values[3] || values[4]>UINT32_MAX
        || values[5]>32767 || values[6]==0 || values[6]>63) return false;
    output={{values[0],{values[1]}},values[2],values[3],static_cast<std::uint32_t>(values[4]),
        static_cast<std::uint16_t>(values[5]),static_cast<std::uint8_t>(values[6]),boot};
    return true;
}

class Service final {
    struct Targets final {
        std::uint32_t first{},second{};
        std::array<std::uint32_t,6> additional{};
        friend constexpr bool operator==(const Targets&,const Targets&)=default;
    };
public:
    struct Observation {
        std::array<std::uint32_t,6> scalar{};
        std::array<std::int32_t,8> consumed{};
        std::uint32_t revision{};
        std::uint8_t known{},consumedCount{};
        bool seen{},consumedKnown{};
    };

    [[nodiscard]] bool begin(Owner owner,std::span<const Capability> definitions,std::uint64_t boot=1) noexcept {
        if(owner_ || !owner || !boot || owner.incarnation.value==0 || owner.incarnation.value>kMaximumGeneration
            || definitions.size()>kBindingCapacity) return false;
        for(std::size_t i=0;i<definitions.size();++i) {
            if(!valid(definitions[i])) return false;
            for(std::size_t j=0;j<i;++j)
                if(definitions[i].registry->key==definitions[j].registry->key && definitions[i].slot==definitions[j].slot) return false;
        }
        owner_=owner;capabilities_=definitions;revision_=1;lastRequest_=0;boot_=boot;
        targets_={};occupancyTargets_={};requests_={};renewals_={};statuses_={};observations_={};observedGeneration_={};history_={};historyUsed_=0;
        for(std::size_t i=0;i<definitions.size();++i) {
            statuses_[i].generation=static_cast<std::uint32_t>(owner.incarnation.value);
            reset_task(i);
        }
        return true;
    }

    // Internal recurring-source renewal. The native consumed mirror proves the
    // old cumulative quota was drained. The caller must separately prove all
    // actors died and produced retirement receipts before publishing this.
    [[nodiscard]] Result renew(const Command& command,std::uint32_t bubble) noexcept {
        if(!owner_ || command.boot!=boot_ || command.owner!=owner_ || command.expectedRevision!=revision_)return Result::stale;
        if(!command.request || command.request<=lastRequest_)return Result::duplicate;
        if(!valid_targets(command,{}))return Result::invalid;
        for(std::size_t i=0;i<capabilities_.size();++i) {
            const auto& capability=capabilities_[i];
            if(command.registry!=capability.registry->key || command.slot!=capability.slot)continue;
            if(!valid_targets(command,capability.categories))return Result::invalid;
            if(bubble!=capability.registry->bubble)return Result::stale;
            if((capability.allowCycles && !capability.recurringRehydration)
                || statuses_[i].phase!=Phase::active || !targets_[i].first || !consumed(i) || renewals_[i].pending
                || statuses_[i].generation>=0x7FFFFFFFU || revision_==UINT64_MAX)return Result::exhausted;
            renewals_[i]={command.request,statuses_[i].generation,targets_[i].first,command.requested,true,
                targets_[i].second,command.secondRequested,capability.categories>1,
                targets_[i].additional,command.additionalRequested,capability.categories};lastRequest_=command.request;
            return Result::accepted;
        }
        return Result::unsupported;
    }
    [[nodiscard]] Result request(const Command& command,std::uint32_t bubble) noexcept {
        const auto index=find(command.registry,command.slot);
        if(index==kMissing) return Result::unsupported;
        if(!matches_session(command.owner,command.boot,command.expectedRevision)) return Result::stale;
        if(has_history(HistoryKind::normal,command,bubble,0)) return Result::duplicate;
        if(command.request==0 || command.request<=lastRequest_) return Result::stale;
        if(!valid_targets(command,capabilities_[index].categories)) return Result::invalid;
        if(bubble!=capabilities_[index].registry->bubble) return Result::stale;
        auto& state=statuses_[index];
        if(renewals_[index].pending) return Result::exhausted;
        if(state.phase==Phase::retiring || state.phase==Phase::retired) return Result::notAllowed;
        const Targets requested=targets(command);
        for(std::size_t category=0;category<capabilities_[index].categories;++category)
            if(target(requested,category)<target(targets_[index],category))return Result::decrease;
        if(requested==targets_[index]) return Result::unchanged;
        if(revision_==UINT64_MAX) return Result::exhausted;
        if(!state.published) {
            state.published=true;state.phase=Phase::active;reset_mirror(index);
        }
        targets_[index]=requested;occupancyTargets_[index]=requested;
        state.requested=command.requested;requests_[index]=command.request;
        lastRequest_=command.request;++revision_;record(HistoryKind::normal,command,bubble,0);
        return Result::accepted;
    }

    // Internal casualty replacement. These are bounded increments to lifetime
    // quotas; ordinary request() remains an absolute target capped at 63.
    [[nodiscard]] Result replenish(const Command& command,std::uint32_t bubble) noexcept {
        if(!matches_session(command.owner,command.boot,command.expectedRevision))return Result::stale;
        if(!command.request || command.request<=lastRequest_)return Result::duplicate;
        if(!valid_targets(command,{},false))return Result::invalid;
        const auto index=find(command.registry,command.slot);
        if(index==kMissing)return Result::unsupported;
        const auto& capability=capabilities_[index];
        auto& state=statuses_[index];
        if(bubble!=capability.registry->bubble || !state.published)return Result::stale;
        if((capability.allowCycles && !capability.recurringRehydration)
            || state.phase!=Phase::active)return Result::notAllowed;
        if(!targets_[index].first)return Result::stale;
        if(!valid_targets(command,capability.categories,false))return Result::invalid;
        if(renewals_[index].pending || revision_==UINT64_MAX
            )return Result::exhausted;
        for(std::size_t category=0;category<capability.categories;++category) {
            const auto add=command_target(command,category);
            if(add && !target(targets_[index],category))return Result::invalid;
            if(target(targets_[index],category)>std::uint32_t(INT32_MAX)-add)return Result::exhausted;
        }
        add_targets(targets_[index],command,capability.categories);
        state.requested=targets_[index].first;requests_[index]=command.request;
        lastRequest_=command.request;++revision_;return Result::accepted;
    }
    [[nodiscard]] Result top_up(const Command& command,std::uint32_t bubble) noexcept {
        return replenish(command,bubble);
    }
    // Internal streaming recovery. The caller must authenticate a native
    // sourceRecreated receipt and supply only retired, not-dead actors by exact
    // member category. This preserves the generation and real death totals.
    [[nodiscard]] Result rehydrate(const Command& command,std::uint32_t bubble) noexcept {
        if(!matches_session(command.owner,command.boot,command.expectedRevision))return Result::stale;
        if(!command.request || command.request<=lastRequest_)return Result::duplicate;
        if(!valid_targets(command,{},false))return Result::invalid;
        const auto index=find(command.registry,command.slot);
        if(index==kMissing)return Result::unsupported;
        const auto& capability=capabilities_[index];auto& state=statuses_[index];
        if(bubble!=capability.registry->bubble || !state.published)return Result::stale;
        if(state.phase!=Phase::active || !targets_[index].first)return Result::notAllowed;
        if(!valid_targets(command,capability.categories,false))return Result::invalid;
        if(renewals_[index].pending || revision_==UINT64_MAX)return Result::exhausted;
        for(std::size_t category=0;category<capability.categories;++category) {
            const auto add=command_target(command,category);
            if(add && !target(targets_[index],category))return Result::invalid;
            if(target(targets_[index],category)>std::uint32_t(INT32_MAX)-add)return Result::exhausted;
        }
        add_targets(targets_[index],command,capability.categories);
        state.requested=targets_[index].first;requests_[index]=command.request;
        lastRequest_=command.request;++revision_;return Result::accepted;
    }

    [[nodiscard]] Result request_retirement(Owner owner,std::uint64_t boot,std::uint64_t expectedRevision,
        std::uint64_t request,std::uint32_t registry,std::uint16_t slot) noexcept {
        Command command{owner,expectedRevision,request,registry,slot,0,boot};
        const auto index=find(registry,slot);
        if(index==kMissing) return Result::unsupported;
        if(has_history(HistoryKind::retirement,command,0,0)) return Result::duplicate;
        if(!matches_session(owner,boot,expectedRevision)) return Result::stale;
        if(!request || request<=lastRequest_) return Result::stale;
        if(revision_==UINT64_MAX) return Result::exhausted;
        const auto& capability=capabilities_[index];
        auto& state=statuses_[index];
        if(!capability.allowCycles || !state.published || state.phase!=Phase::active) return Result::notAllowed;
        if(state.generation==kMaximumGeneration) return Result::exhausted;
        ++state.generation;state.phase=Phase::retiring;state.nativeAckNeeded=true;
        state.retirementAcknowledged=false;targets_[index]={};occupancyTargets_[index]={};state.requested=0;
        requests_[index]=request;reset_mirror(index);
        lastRequest_=request;++revision_;record(HistoryKind::retirement,command,0,0);
        return Result::accepted;
    }

    [[nodiscard]] Result begin_cycle(Owner owner,std::uint64_t boot,std::uint64_t expectedRevision,
        std::uint64_t request,std::uint32_t registry,std::uint16_t slot,std::uint32_t newCycle,
        std::uint8_t requestedTarget,std::uint8_t secondTarget=0) noexcept {
        Command command{owner,expectedRevision,request,registry,slot,requestedTarget,boot,secondTarget};
        return begin_cycle(command,newCycle);
    }
    [[nodiscard]] Result begin_cycle(const Command& command,std::uint32_t newCycle) noexcept {
        const auto owner=command.owner;const auto boot=command.boot;
        const auto expectedRevision=command.expectedRevision;const auto request=command.request;
        const auto registry=command.registry;const auto slot=command.slot;
        const auto index=find(registry,slot);
        if(index==kMissing) return Result::unsupported;
        if(has_history(HistoryKind::cycle,command,0,newCycle)) return Result::duplicate;
        if(!matches_session(owner,boot,expectedRevision)) return Result::stale;
        if(!request || !valid_targets(command,capabilities_[index].categories)) return Result::invalid;
        if(request<=lastRequest_) return Result::stale;
        if(revision_==UINT64_MAX) return Result::exhausted;
        const auto& capability=capabilities_[index];
        auto& state=statuses_[index];
        const bool initial=!state.published && state.phase==Phase::inactive;
        if(!capability.allowCycles || (!initial && !(state.phase==Phase::retired && state.retirementAcknowledged))) return Result::notAllowed;
        if(newCycle<=state.cycle) return Result::decrease;
        if(!initial && state.generation==kMaximumGeneration) return Result::exhausted;
        if(!initial) ++state.generation;
        state.cycle=newCycle;state.phase=Phase::active;state.published=true;
        state.nativeAckNeeded=false;state.retirementAcknowledged=false;targets_[index]=targets(command);
        occupancyTargets_[index]=targets_[index];
        requests_[index]=request;
        state.requested=command.requested;reset_mirror(index);
        lastRequest_=request;++revision_;record(HistoryKind::cycle,command,0,newCycle);
        return Result::accepted;
    }

    /** The bool is proof from the qualified native ledger caller. */
    [[nodiscard]] bool retirement_acknowledged(Owner owner,std::uint64_t boot,std::uint32_t registry,
        std::uint16_t slot,std::uint32_t generation,bool allPriorActorsRetired) noexcept {
        const auto index=find(registry,slot);
        if(index==kMissing || !matches_owner(owner,boot)) return false;
        auto& state=statuses_[index];
        if(state.phase==Phase::retired && state.retirementAcknowledged && state.generation==generation) return allPriorActorsRetired;
        if(!allPriorActorsRetired || state.phase!=Phase::retiring || state.generation!=generation
            || !state.nativeAckNeeded || !observedGeneration_[index]) return false;
        state.phase=Phase::retired;state.nativeAckNeeded=false;state.retirementAcknowledged=true;
        return true;
    }

    [[nodiscard]] const Observation* observe(std::uint32_t bubble,
        const middleware::bap::activity_message::sense_update::SenseObject& object) noexcept {
        return observe_impl(bubble,false,object);
    }
    // Caller qualifies the retained open-world lifecycle and transport epoch.
    // Exact source identity and generation remain mandatory in observe_impl.
    [[nodiscard]] const Observation* observe_retained(
        const middleware::bap::activity_message::sense_update::SenseObject& object) noexcept {
        return observe_impl(0,true,object);
    }
private:
    [[nodiscard]] const Observation* observe_impl(std::uint32_t bubble,bool retained,
        const middleware::bap::activity_message::sense_update::SenseObject& object) noexcept {
        if(!owner_ || !object.hasNativeSchema || object.nativeSchema!=0x80807ECC || object.slotType!=1) return nullptr;
        const auto index=find(object.registryKey,object.slotIndex);
        if(index==kMissing || !targetsPublished(index)
            || (!retained && capabilities_[index].registry->bubble!=bubble)) return nullptr;
        auto& mirror=observations_[index];
        if(mirror.seen && (object.nativeRevision==mirror.revision
            || object.nativeRevision-mirror.revision>=0x80000000U)) return nullptr;
        const auto& delta=object.sourceDelta;
        if(object.hasRootDelta && (delta.present&1U)) {
            if(delta.scalar[0]!=statuses_[index].generation) return nullptr;
        } else if(!(mirror.known&1U)) return nullptr;
        if(delta.consumedCount>delta.consumed.size() || (delta.present&0xC0U)) return nullptr;
        mirror.seen=true;mirror.revision=object.nativeRevision;
        if(object.hasRootDelta) {
            for(unsigned j=0;j<delta.scalar.size();++j)
                if(delta.present&(1U<<j)) mirror.scalar[j]=delta.scalar[j];
            mirror.known|=delta.present;
            if(delta.consumedPresent) {
                mirror.consumed=delta.consumed;mirror.consumedCount=delta.consumedCount;mirror.consumedKnown=true;
            }
        }
        if((mirror.known&1U) && mirror.scalar[0]==statuses_[index].generation) observedGeneration_[index]=true;
            const auto& capability=capabilities_[index];
        if(statuses_[index].phase==Phase::active && capabilities_[index].taskMask && object.hasSquadOutput && !renewals_[index].pending) {
                const auto& squad=object.squadOutput;
                // A mismatched evaluator revision invalidates the sparse-cost
                // lease: later omitted revisions cannot be assumed current until
                // the client explicitly confirms this assignment again.
                if(squad.hasRevision && squad.revision!=tasks_[index].revision)costs_[index]={};
                else {
                    const state::activity::coo::TaskCosts report{squad.cost,squad.costMask,
                        squad.revision,squad.hasRevision,squad.initialized};
                    costs_[index].merge(report);
                    const auto selected=costs_[index].select(tasks_[index].row,tasks_[index].revision,
                        [&capability](std::uint8_t row){return (capability.taskMask&(1U<<row))!=0;});
                    if(selected>=0 && selected!=tasks_[index].row && revision_!=UINT64_MAX) {
                        tasks_[index].row=selected;++revision_;
                    }
                }
            }
        return &mirror;
    }

public:
    [[nodiscard]] wire::Batch project(std::uint32_t bubble) const noexcept { return project_impl(bubble,false); }
    [[nodiscard]] wire::Batch project_retained() const noexcept { return project_impl(0,true); }
private:
    [[nodiscard]] wire::Batch project_impl(std::uint32_t bubble,bool retained) const noexcept {
        wire::Batch batch{};
        for(std::size_t i=0;i<capabilities_.size();++i) {
            const auto& capability=capabilities_[i];const auto& state=statuses_[i];
            if(!targetsPublished(i) || (!retained && capability.registry->bubble!=bubble) || batch.count==batch.entries.size()) continue;
            auto& row=batch.entries[batch.count++];
            row.bubble=capability.registry->bubble;row.slot=capability.slot;row.namedMember=capability.namedMember;
            row.source={capability.registry->key,state.generation,capability.rule,targets_[i].first,tasks_[i]};
            row.source.secondRequested=targets_[i].second;
            row.source.additionalRequested=targets_[i].additional;
            row.source.categoryCount=capability.categories;
            row.source.hasSecondCategory=capability.categories>1;
            if (capability.tactical.row >= 0) {
                row.source.tactical.revision = state.generation;
            }
            row.source.hasSpawnRule=capability.hasRule;
            row.source.retireOwned=state.phase==Phase::retiring || state.phase==Phase::retired;
        }
        return batch;
    }

public:
    [[nodiscard]] Owner owner() const noexcept { return owner_; }
    [[nodiscard]] std::uint64_t boot() const noexcept { return boot_; }
    [[nodiscard]] std::uint64_t revision() const noexcept { return revision_; }
    [[nodiscard]] std::uint64_t last_request() const noexcept { return lastRequest_; }
    [[nodiscard]] const Status* status(std::uint32_t registry,std::uint16_t slot) const noexcept {
        const auto index=find(registry,slot);return index==kMissing?nullptr:&statuses_[index];
    }
    [[nodiscard]] const Status* binding_status(std::uint32_t registry,std::uint16_t slot) const noexcept {
        return status(registry,slot);
    }
    [[nodiscard]] std::uint32_t generation(std::uint32_t registry,std::uint16_t slot) const noexcept {
        const auto* value=status(registry,slot);return value?value->generation:0;
    }
    [[nodiscard]] std::uint32_t cycle(std::uint32_t registry,std::uint16_t slot) const noexcept {
        const auto* value=status(registry,slot);return value?value->cycle:0;
    }
    [[nodiscard]] Phase phase(std::uint32_t registry,std::uint16_t slot) const noexcept {
        const auto* value=status(registry,slot);return value?value->phase:Phase::inactive;
    }
    [[nodiscard]] bool native_ack_needed(std::uint32_t registry,std::uint16_t slot) const noexcept {
        const auto* value=status(registry,slot);return value && value->nativeAckNeeded;
    }

    [[nodiscard]] std::uint64_t source_request(std::size_t index) const noexcept {
        return index<capabilities_.size()?requests_[index]:0;
    }
    [[nodiscard]] std::size_t capability_index(std::uint32_t registry,std::uint16_t slot) const noexcept {
        const auto index=find(registry,slot);return index==kMissing?kSourceCapacity:index;
    }
    [[nodiscard]] std::uint32_t generation(std::size_t index) const noexcept {
        return index<capabilities_.size()?statuses_[index].generation:0;
    }
    [[nodiscard]] const Observation* observation(std::size_t index) const noexcept {
        return index<capabilities_.size() && observations_[index].seen?&observations_[index]:nullptr;
    }
    [[nodiscard]] std::uint32_t target(std::size_t index) const noexcept {
        return index<capabilities_.size()?targets_[index].first:0;
    }
    [[nodiscard]] std::uint32_t second_target(std::size_t index) const noexcept {
        return index<capabilities_.size()?targets_[index].second:0;
    }
    [[nodiscard]] std::uint32_t target(std::size_t index,std::size_t category) const noexcept {
        return index<capabilities_.size()?target(targets_[index],category):0;
    }
    [[nodiscard]] std::uint32_t occupancy_target(std::size_t index,std::size_t category) const noexcept {
        return index<capabilities_.size()?target(occupancyTargets_[index],category):0;
    }
    [[nodiscard]] codec::TacticalGroup tactical(std::size_t index) const noexcept {
        return index<capabilities_.size()?tasks_[index]:codec::TacticalGroup{};
    }
    [[nodiscard]] bool consumed(std::size_t index) const noexcept {
        if(index>=capabilities_.size() || !targets_[index].first)return false;
        const auto& mirror=observations_[index];
        if(!mirror.seen || !mirror.consumedKnown
            || mirror.consumedCount!=capabilities_[index].categories)return false;
        for(std::size_t category=0;category<capabilities_[index].categories;++category)
            if(mirror.consumed[category]<static_cast<std::int32_t>(target(targets_[index],category)))return false;
        return true;
    }
    struct Renewal final {
        std::uint64_t request{};std::uint32_t generation{};
        std::uint32_t target{};std::uint8_t nextTarget{};bool pending{};
        std::uint32_t secondTarget{};std::uint8_t nextSecondTarget{};bool hasSecondCategory{};
        std::array<std::uint32_t,6> additionalTarget{};
        std::array<std::uint8_t,6> nextAdditionalTarget{};
        std::uint8_t categoryCount{};
    };
    [[nodiscard]] Renewal renewal(std::size_t index) const noexcept {
        return index<capabilities_.size()?renewals_[index]:Renewal{};
    }
    // The runtime commits only after the receipt bridge proves the old lease
    // quiescent and accepts the next generation. Until then project() retains
    // the old source identity.
    [[nodiscard]] bool commit_renewal(std::size_t index) noexcept {
        if(index>=capabilities_.size())return false;const auto ticket=renewals_[index];
        if(!ticket.pending || !ticket.request || ticket.generation!=statuses_[index].generation
            || ticket.target!=targets_[index].first || ticket.secondTarget!=targets_[index].second
            || ticket.additionalTarget!=targets_[index].additional || !ticket.nextTarget
            || ticket.hasSecondCategory!=(capabilities_[index].categories>1)
            || ticket.categoryCount!=capabilities_[index].categories
            || statuses_[index].generation>=0x7FFFFFFFU || revision_==UINT64_MAX)return false;
        ++statuses_[index].generation;targets_[index]={};targets_[index].first=ticket.nextTarget;
        targets_[index].second=ticket.nextSecondTarget;
        for(std::size_t i=0;i<ticket.nextAdditionalTarget.size();++i)
            targets_[index].additional[i]=ticket.nextAdditionalTarget[i];
        occupancyTargets_[index]=targets_[index];
        requests_[index]=ticket.request;
        statuses_[index].requested=ticket.nextTarget;reset_mirror(index);
        ++revision_;renewals_[index]={};return true;
    }
    void cancel_renewal(std::size_t index) noexcept {if(index<capabilities_.size())renewals_[index]={};}
private:
    [[nodiscard]] static constexpr std::uint32_t target(const Targets& value,std::size_t category) noexcept {
        return category==0?value.first:category==1?value.second:
            category<8?value.additional[category-2]:0U;
    }
    [[nodiscard]] static constexpr std::uint8_t command_target(const Command& value,std::size_t category) noexcept {
        return category==0?value.requested:category==1?value.secondRequested:
            category<8?value.additionalRequested[category-2]:0U;
    }
    [[nodiscard]] static constexpr Targets targets(const Command& value) noexcept {
        Targets result{};result.first=value.requested;result.second=value.secondRequested;
        for(std::size_t i=0;i<value.additionalRequested.size();++i)
            result.additional[i]=value.additionalRequested[i];
        return result;
    }
    [[nodiscard]] static constexpr bool valid_targets(const Command& value,std::uint8_t categories,
        bool requireFirst=true) noexcept {
        const auto count=categories?categories:(value.categoryCount?value.categoryCount:
            value.secondRequested?2U:1U);
        if(!count || count>8 || (value.categoryCount && value.categoryCount!=count)
            || (requireFirst && !value.requested))return false;
        unsigned total{};bool any{};
        for(std::size_t i=0;i<8;++i) {
            const auto current=command_target(value,i);
            if(i>=count && current)return false;
            any=any || current!=0;total+=current;if(total>63)return false;
        }
        return any;
    }
    static constexpr void add_targets(Targets& into,const Command& value,std::uint8_t categories) noexcept {
        into.first+=value.requested;if(categories>1)into.second+=value.secondRequested;
        for(std::size_t i=2;i<categories;++i)into.additional[i-2]+=value.additionalRequested[i-2];
    }
    static constexpr std::size_t kMissing=kBindingCapacity;
    static constexpr std::size_t kHistoryCapacity=256;
    enum class HistoryKind : std::uint8_t { normal,retirement,cycle };
    struct History final { HistoryKind kind{};Command command{};std::uint32_t cycle{};std::uint32_t bubble{}; };
    [[nodiscard]] std::size_t find(std::uint32_t registry,std::uint16_t slot) const noexcept {
        for(std::size_t i=0;i<capabilities_.size();++i)
            if(capabilities_[i].registry->key==registry && capabilities_[i].slot==slot) return i;
        return kMissing;
    }
    [[nodiscard]] bool matches_session(Owner owner,std::uint64_t boot,std::uint64_t expected) const noexcept {
        return owner_ && owner==owner_ && boot==boot_ && expected==revision_;
    }
    [[nodiscard]] bool matches_owner(Owner owner,std::uint64_t boot) const noexcept {
        return owner_ && owner==owner_ && boot==boot_;
    }
    [[nodiscard]] bool targetsPublished(std::size_t index) const noexcept {
        return index<capabilities_.size() && statuses_[index].published;
    }
    [[nodiscard]] bool has_history(HistoryKind kind,const Command& command,std::uint32_t bubble,std::uint32_t cycleValue) const noexcept {
        for(std::size_t i=0;i<historyUsed_;++i) {
            const auto& prior=history_[i];
            if(prior.kind==kind && prior.command.owner==command.owner && prior.command.boot==command.boot
                && prior.command.request==command.request
                && prior.command.registry==command.registry && prior.command.slot==command.slot
                && prior.command.requested==command.requested && prior.command.secondRequested==command.secondRequested
                && prior.command.additionalRequested==command.additionalRequested
                && prior.command.categoryCount==command.categoryCount
                && prior.bubble==bubble && prior.cycle==cycleValue) return true;
        }
        return false;
    }
    void record(HistoryKind kind,const Command& command,std::uint32_t bubble,std::uint32_t cycleValue) noexcept {
        if(historyUsed_<history_.size()) history_[historyUsed_++]={kind,command,cycleValue,bubble};
        else {
            for(std::size_t i=1;i<history_.size();++i) history_[i-1]=history_[i];
            history_.back()={kind,command,cycleValue,bubble};
        }
    }
    void reset_task(std::size_t index) noexcept {
        costs_[index]={};tasks_[index]=capabilities_[index].tactical;
        if(tasks_[index].row>=0 && (capabilities_[index].allowCycles || capabilities_[index].taskMask))
            tasks_[index].revision=statuses_[index].generation;
    }
    void reset_mirror(std::size_t index) noexcept { observations_[index]={};observedGeneration_[index]=false;reset_task(index); }

    Owner owner_{};
    std::span<const Capability> capabilities_{};
    std::array<Targets,kBindingCapacity> targets_{},occupancyTargets_{};
    std::array<std::uint64_t,kBindingCapacity> requests_{};
    std::array<Status,kBindingCapacity> statuses_{};
    std::array<Renewal,kBindingCapacity> renewals_{};
    std::array<codec::TacticalGroup,kBindingCapacity> tasks_{};
    std::array<state::activity::coo::TaskCosts,kBindingCapacity> costs_{};
    std::array<Observation,kBindingCapacity> observations_{};
    std::array<bool,kBindingCapacity> observedGeneration_{};
    std::array<History,kHistoryCapacity> history_{};
    std::size_t historyUsed_{};
    std::uint64_t revision_{},lastRequest_{},boot_{};
};
} // namespace sunrise::server::runtime::activity::population
