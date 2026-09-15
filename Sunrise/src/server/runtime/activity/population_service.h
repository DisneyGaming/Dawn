#pragma once
#include "registry_admission.h"
#include "../../../state/activity/lifecycle_generation.h"
#include "../../../state/activity/coo/task_costs.h"
#include <charconv>
#include <string_view>
#include "../../../middleware/bap/activity_message/sense_update.h"

namespace sunrise::server::runtime::activity::population {
namespace wire=middleware::bap::activity_message::native::population;
namespace codec=middleware::bap::activity_message::native::combatant_source;
inline constexpr std::size_t kSourceCapacity=wire::kSourceCapacity;
using Owner=state::activity::ActivityInstanceKey;
struct Capability final {
    const registry::Definition* registry{};
    std::uint16_t slot{},rule{};
    codec::TacticalGroup tactical{};
    bool hasRule{true};
    // Explicit opt-in: only these package-authored rows of this objective may
    // replace the initial row using the native squad's reachable-cost reports.
    std::uint32_t taskMask{};
    // Authored native source category-array width. Category one stays dormant
    // until an explicit request supplies a nonzero second target.
    std::uint8_t categories{1};
};
[[nodiscard]] inline bool valid(const Capability& capability) noexcept {
    if(!capability.registry || !registry::valid(*capability.registry)) return false;
    unsigned source{},rule{},tactical{};
    for(const auto& slot:capability.registry->slots) {
        if(slot.index==capability.slot && slot.type==1 && slot.authSchema==0x80807EC9) ++source;
        if(slot.index==capability.rule && slot.type==66) ++rule;
        if(slot.index==capability.tactical.slot && slot.type==3 && slot.authSchema==0x80807F0C) ++tactical;
    }
    codec::Source request{capability.registry->key,1,capability.rule,1,capability.tactical};
    request.hasSecondCategory=capability.categories==2;
    request.hasSpawnRule=capability.hasRule;
    if(capability.taskMask && (capability.tactical.row<0 || capability.tactical.row>=24 || (capability.taskMask&0xFF000000U)
        || !(capability.taskMask&(1U<<capability.tactical.row))))return false;
    return (capability.categories==1 || capability.categories==2)
        && source==1 && (!capability.hasRule || rule==1)
        && (capability.tactical.row<0 || (capability.tactical.registry==capability.registry->key && tactical==1))
        && codec::valid(request);
}
struct Command final {
    Owner owner{};
    std::uint64_t expectedRevision{},request{};
    std::uint32_t registry{};
    std::uint16_t slot{};
    std::uint8_t requested{};
    std::uint64_t boot{};
    std::uint8_t secondRequested{};
};
enum class Result : std::uint8_t { accepted, invalid, stale, duplicate, unsupported, decrease, unchanged, exhausted };
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
        [[nodiscard]] constexpr std::uint64_t total() const noexcept {return std::uint64_t(first)+second;}
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
        if(owner_ || !owner || !boot || owner.incarnation.value>0x7FFFFFFFU
            || definitions.size()>targets_.size()) return false;
        for(std::size_t i=0;i<definitions.size();++i) {
            if(!valid(definitions[i])) return false;
            for(std::size_t j=0;j<i;++j)
                if(definitions[i].registry->key==definitions[j].registry->key && definitions[i].slot==definitions[j].slot) return false;
        }
        owner_=owner;capabilities_=definitions;revision_=1;boot_=boot;
        for(std::size_t i=0;i<definitions.size();++i) {
            generations_[i]=static_cast<std::uint32_t>(owner.incarnation.value);
            tasks_[i]=definitions[i].tactical;
            if(definitions[i].taskMask)tasks_[i].revision=generations_[i];
        }
        return true;
    }
    // Internal recurring-source renewal. The native consumed mirror proves the
    // old cumulative quota was drained. The caller must separately prove all
    // actors died and produced retirement receipts before publishing this.
    [[nodiscard]] Result renew(const Command& command,std::uint32_t bubble) noexcept {
        if(!owner_ || command.boot!=boot_ || command.owner!=owner_ || command.expectedRevision!=revision_)return Result::stale;
        if(!command.request || command.request<=lastRequest_)return Result::duplicate;
        if(!command.requested || unsigned(command.requested)+command.secondRequested>63)return Result::invalid;
        for(std::size_t i=0;i<capabilities_.size();++i) {
            const auto& capability=capabilities_[i];
            if(command.registry!=capability.registry->key || command.slot!=capability.slot)continue;
            if(command.secondRequested && capability.categories!=2)return Result::invalid;
            if(bubble!=capability.registry->bubble)return Result::stale;
            if(!targets_[i].first || !consumed(i) || renewals_[i].pending
                || generations_[i]>=0x7FFFFFFFU || revision_==UINT64_MAX)return Result::exhausted;
            renewals_[i]={command.request,generations_[i],targets_[i].first,command.requested,true,
                targets_[i].second,command.secondRequested,capability.categories==2};lastRequest_=command.request;
            return Result::accepted;
        }
        return Result::unsupported;
    }
    [[nodiscard]] Result request(const Command& command,std::uint32_t bubble) noexcept {
        if(!owner_ || command.boot!=boot_ || command.owner!=owner_ || command.expectedRevision!=revision_) return Result::stale;
        if(!command.request || command.request<=lastRequest_) return Result::duplicate;
        if(!command.requested || unsigned(command.requested)+command.secondRequested>63) return Result::invalid;
        for(std::size_t i=0;i<capabilities_.size();++i) {
            const auto& capability=capabilities_[i];
            if(command.registry!=capability.registry->key || command.slot!=capability.slot) continue;
            if(command.secondRequested && capability.categories!=2)return Result::invalid;
            if(bubble!=capability.registry->bubble) return Result::stale;
            if(renewals_[i].pending) return Result::exhausted;
            const Targets requested{command.requested,command.secondRequested};
            if(requested.first<targets_[i].first || requested.second<targets_[i].second) return Result::decrease;
            if(requested==targets_[i]) return Result::unchanged;
            if(revision_==UINT64_MAX) return Result::exhausted;
            targets_[i]=requested;requests_[i]=command.request;lastRequest_=command.request;++revision_;return Result::accepted;
        }
        return Result::unsupported;
    }
    // Internal casualty replacement only. Command counts here are bounded
    // increments; ordinary request()/the developer text format remain absolute
    // and capped at63. This extends lifetime quota, never the intended live cap.
    [[nodiscard]] Result replenish(const Command& command,std::uint32_t bubble) noexcept {
        if(!owner_ || command.boot!=boot_ || command.owner!=owner_ || command.expectedRevision!=revision_)return Result::stale;
        if(!command.request || command.request<=lastRequest_)return Result::duplicate;
        if(!command.requested && !command.secondRequested)return Result::invalid;
        if(unsigned(command.requested)+command.secondRequested>63)return Result::invalid;
        for(std::size_t i=0;i<capabilities_.size();++i) {
            const auto& capability=capabilities_[i];
            if(command.registry!=capability.registry->key || command.slot!=capability.slot)continue;
            if(bubble!=capability.registry->bubble || !targets_[i].first)return Result::stale;
            if(command.secondRequested && (capability.categories!=2 || !targets_[i].second))return Result::invalid;
            if(renewals_[i].pending || revision_==UINT64_MAX
                || targets_[i].first>std::uint32_t(INT32_MAX)-command.requested
                || targets_[i].second>std::uint32_t(INT32_MAX)-command.secondRequested)return Result::exhausted;
            targets_[i].first+=command.requested;targets_[i].second+=command.secondRequested;
            requests_[i]=command.request;lastRequest_=command.request;++revision_;return Result::accepted;
        }
        return Result::unsupported;
    }
    [[nodiscard]] wire::Batch project(std::uint32_t bubble) const noexcept {
        return project_selected(bubble,false);
    }
    // A retained roster continues to own started sources outside the currently
    // selected region. Omitting their bodies resets native tactical authority.
    // This republishes existing targets only; it never starts an unvisited zone.
    [[nodiscard]] wire::Batch project_retained() const noexcept {
        return project_selected(0,true);
    }
private:
    [[nodiscard]] wire::Batch project_selected(std::uint32_t bubble,bool retained) const noexcept {
        wire::Batch batch{};
        for(std::size_t i=0;i<capabilities_.size();++i) {
            const auto& capability=capabilities_[i];
            if(!targets_[i].first || (!retained && capability.registry->bubble!=bubble)) continue;
            auto& row=batch.entries[batch.count++];
            row.bubble=capability.registry->bubble;row.slot=capability.slot;
            row.source={capability.registry->key,generations_[i],
                capability.rule,targets_[i].first,tasks_[i]};
            row.source.secondRequested=targets_[i].second;
            row.source.hasSecondCategory=capability.categories==2;
            row.source.hasSpawnRule=capability.hasRule;
        }
        return batch;
    }
public:
    [[nodiscard]] Owner owner() const noexcept { return owner_; }
    [[nodiscard]] std::uint64_t boot() const noexcept { return boot_; }
    // Transport/session/epoch qualification belongs to the caller. Only a
    // generation-qualified native source delta can establish this mirror.
    [[nodiscard]] const Observation* observe(std::uint32_t bubble,
        const middleware::bap::activity_message::sense_update::SenseObject& object) noexcept {
        return observe_impl(bubble,false,object);
    }
    // Open-world sources remain authored and owned while the player crosses a
    // bubble boundary. The caller must restrict this path to retained
    // open-world population lifecycles; exact source identity and generation
    // remain mandatory below.
    [[nodiscard]] const Observation* observe_retained(
        const middleware::bap::activity_message::sense_update::SenseObject& object) noexcept {
        return observe_impl(0,true,object);
    }
private:
    [[nodiscard]] const Observation* observe_impl(std::uint32_t bubble,bool retained,
        const middleware::bap::activity_message::sense_update::SenseObject& object) noexcept {
        if(!owner_ || !object.hasNativeSchema || object.nativeSchema!=0x80807ECC
            || object.slotType!=1) return nullptr;
        for(std::size_t i=0;i<capabilities_.size();++i) {
            const auto& capability=capabilities_[i];
            if(!targets_[i].first || (!retained && capability.registry->bubble!=bubble)
                || capability.registry->key!=object.registryKey || capability.slot!=object.slotIndex) continue;
            auto& mirror=observations_[i];
            if(mirror.seen && (object.nativeRevision==mirror.revision
                || object.nativeRevision-mirror.revision>=0x80000000U)) return nullptr;
            const auto& delta=object.sourceDelta;
            if(object.hasRootDelta && (delta.present&1U)) {
                if(delta.scalar[0]!=generations_[i]) return nullptr;
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
            if(capability.taskMask && object.hasSquadOutput && !renewals_[i].pending) {
                const auto& squad=object.squadOutput;
                // A mismatched evaluator revision invalidates the sparse-cost
                // lease: later omitted revisions cannot be assumed current until
                // the client explicitly confirms this assignment again.
                if(squad.hasRevision && squad.revision!=tasks_[i].revision)costs_[i]={};
                else {
                    const state::activity::coo::TaskCosts report{squad.cost,squad.costMask,
                        squad.revision,squad.hasRevision,squad.initialized};
                    costs_[i].merge(report);
                    const auto selected=costs_[i].select(tasks_[i].row,tasks_[i].revision,
                        [&capability](std::uint8_t row){return (capability.taskMask&(1U<<row))!=0;});
                    if(selected>=0 && selected!=tasks_[i].row && revision_!=UINT64_MAX) {
                        tasks_[i].row=selected;++revision_;
                    }
                }
            }
            return &mirror;
        }
        return nullptr;
    }
public:
    [[nodiscard]] std::uint64_t revision() const noexcept { return revision_; }
    [[nodiscard]] std::uint64_t last_request() const noexcept { return lastRequest_; }
    [[nodiscard]] std::uint64_t source_request(std::size_t index) const noexcept {
        return index<capabilities_.size()?requests_[index]:0;
    }
    [[nodiscard]] std::uint32_t generation(std::size_t index) const noexcept {
        return index<capabilities_.size()?generations_[index]:0;
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
    [[nodiscard]] codec::TacticalGroup tactical(std::size_t index) const noexcept {
        return index<capabilities_.size()?tasks_[index]:codec::TacticalGroup{};
    }
    [[nodiscard]] bool consumed(std::size_t index) const noexcept {
        if(index>=capabilities_.size() || !targets_[index].first)return false;
        const auto& mirror=observations_[index];
        if(!mirror.seen || !mirror.consumedKnown
            || mirror.consumedCount!=capabilities_[index].categories)return false;
        for(std::size_t category=0;category<capabilities_[index].categories;++category)
            if(mirror.consumed[category]<static_cast<std::int32_t>(category?targets_[index].second:targets_[index].first))return false;
        return true;
    }
    struct Renewal final {
        std::uint64_t request{};std::uint32_t generation{};
        std::uint32_t target{};std::uint8_t nextTarget{};bool pending{};
        std::uint32_t secondTarget{};std::uint8_t nextSecondTarget{};bool hasSecondCategory{};
    };
    [[nodiscard]] Renewal renewal(std::size_t index) const noexcept {
        return index<capabilities_.size()?renewals_[index]:Renewal{};
    }
    // The runtime commits only after the receipt bridge proves the old lease
    // quiescent and accepts the next generation. Until then project() retains
    // the old source identity.
    [[nodiscard]] bool commit_renewal(std::size_t index) noexcept {
        if(index>=capabilities_.size())return false;const auto ticket=renewals_[index];
        if(!ticket.pending || !ticket.request || ticket.generation!=generations_[index]
            || ticket.target!=targets_[index].first || ticket.secondTarget!=targets_[index].second || !ticket.nextTarget
            || ticket.hasSecondCategory!=(capabilities_[index].categories==2)
            || generations_[index]>=0x7FFFFFFFU || revision_==UINT64_MAX)return false;
        ++generations_[index];targets_[index]={ticket.nextTarget,ticket.nextSecondTarget};requests_[index]=ticket.request;observations_[index]={};
        costs_[index]={};tasks_[index]=capabilities_[index].tactical;
        if(capabilities_[index].taskMask)tasks_[index].revision=generations_[index];
        ++revision_;renewals_[index]={};return true;
    }
    void cancel_renewal(std::size_t index) noexcept {if(index<capabilities_.size())renewals_[index]={};}
private:
    Owner owner_{};
    // Registered capabilities have static lifetime. Dynamic definitions must
    // retain their owning revision lease before using this service.
    std::span<const Capability> capabilities_{};
    std::array<Targets,kSourceCapacity> targets_{};
    std::array<std::uint64_t,kSourceCapacity> requests_{};
    std::array<std::uint32_t,kSourceCapacity> generations_{};
    std::array<Renewal,kSourceCapacity> renewals_{};
    std::array<Observation,kSourceCapacity> observations_{};
    std::array<codec::TacticalGroup,kSourceCapacity> tasks_{};
    std::array<state::activity::coo::TaskCosts,kSourceCapacity> costs_{};
    std::uint64_t revision_{},lastRequest_{},boot_{};
};
} // namespace sunrise::server::runtime::activity::population
