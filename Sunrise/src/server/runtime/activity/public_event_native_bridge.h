#pragma once
#include "public_event_placement_feedback.h"
#include "public_event_rally_use.h"

namespace sunrise::server::runtime::activity::public_event::native_bridge {
namespace feedback=placement_feedback;
struct Binding final { feedback::Ticket ticket{};std::uint64_t epoch{}; };
struct Event final { Binding binding{};feedback::Observation observation{}; };
// An armed ticket precedes native publication. Captures retain its epoch before
// the original callback; releasing an activity invalidates its pending captures.
// No native pointers or spans are stored in this bounded mailbox.
class Mailbox final {
public:
    [[nodiscard]] bool bind(const feedback::Ticket& ticket) noexcept {
        if(!ticket.lease.valid() || ticket.asset.type!=4 || !ticket.asset.definition || !ticket.asset.registry
            || ticket.token.run!=ticket.lease.event || !ticket.token.incarnation || ticket.bubble>63
            || ticket.authorityOwner!=ticket.bubble || !ticket.definition.authoredVisualCount
            || ticket.definition.authoredVisualCount>64 || ticket.definition.nativeDefinitionOffset<=0
            || ((ticket.definition.interactionDefinition!=0)!=(ticket.definition.interactionOffset>0))
            || (ticket.definition.interactionDefinition && !ticket.definition.enableInteractionAfterPlacement)) return false;
        for(std::size_t i=0;i<count_;++i) {
            const auto& prior=bindings_[i].binding.ticket;
            if(feedback::same(prior,ticket)) return true;
            // Shared constant placement generations cannot disambiguate two
            // simultaneously live native instances of the same authored asset.
            if(prior.asset==ticket.asset) return false;
        }
        if(count_==bindings_.size() || nextEpoch_==UINT64_MAX) return false;
        bindings_[count_++]={{ticket,++nextEpoch_},false,UINT32_MAX,false};return true;
    }
    void release(state::activity::ActivityInstanceKey owner) noexcept {
        for(std::size_t i=0;i<count_;)
            if(bindings_[i].binding.ticket.lease.owner==owner) bindings_[i]=bindings_[--count_];else ++i;
        for(std::size_t i=0;i<queued_;)
            if(events_[i].binding.ticket.lease.owner==owner) erase(i);else ++i;
    }
    [[nodiscard]] Binding lookup(std::uint32_t definition,std::uint32_t registry,std::uint16_t slot) const noexcept {
        const Binding* found{};
        for(std::size_t i=0;i<count_;++i) {
            const auto& binding=bindings_[i].binding;
            if(binding.ticket.asset.definition!=definition || binding.ticket.asset.registry!=registry
                || binding.ticket.asset.slot!=slot || bindings_[i].submitted) continue;
            if(found) return {};found=&binding;
        }
        return found?*found:Binding{};
    }
    [[nodiscard]] Binding lookup_definition(std::uint32_t definition) const noexcept {
        const Binding* found{};
        for(std::size_t i=0;i<count_;++i) {
            if(bindings_[i].submitted || bindings_[i].binding.ticket.asset.definition!=definition) continue;
            if(found)return {};found=&bindings_[i].binding;
        }
        return found?*found:Binding{};
    }
    [[nodiscard]] bool submit(const Event& event) noexcept {
        const auto& t=event.binding.ticket;const auto& o=event.observation;
        if(!event.binding.epoch || o.receipt.lease!=t.lease || o.receipt.stage!=Stage::rally || o.receipt.asset!=t.asset
            || o.receipt.event.token!=t.token || o.receipt.event.milestone!=coo::Milestone::nativeReady
            || o.authorityOwner!=t.authorityOwner || !o.sequence || o.entity==UINT32_MAX
            || o.source.member==UINT32_MAX || o.source.offset<0 || o.selector>=t.definition.authoredVisualCount) return false;
        for(std::size_t i=0;i<count_;++i) {
            auto& row=bindings_[i];
            if(row.binding.epoch!=event.binding.epoch || !feedback::same(row.binding.ticket,t)) continue;
            if(row.submitted) return false;
            if(queued_==events_.size()) {overflow_=true;return false;}
            events_[queued_++]=event;row.submitted=true;row.entity=o.entity;return true;
        }
        return false;
    }
    [[nodiscard]] std::size_t drain(state::activity::ActivityInstanceKey owner,std::span<Event> output) noexcept {
        std::size_t used{};
        for(std::size_t i=0;i<queued_ && used<output.size();)
            if(events_[i].binding.ticket.lease.owner==owner) {output[used++]=events_[i];erase(i);}else ++i;
        return used;
    }
    [[nodiscard]] bool overflow() const noexcept {return overflow_;}
    [[nodiscard]] rally_use::Binding lookup_use(std::uint32_t definition,std::uint32_t entity) const noexcept {
        rally_use::Binding found{};
        for(std::size_t i=0;i<count_;++i) {
            const auto& row=bindings_[i];const auto& t=row.binding.ticket;
            if(!row.submitted || row.used || entity==UINT32_MAX || row.entity!=entity
                || !definition || t.definition.interactionDefinition!=definition)continue;
            if(found.epoch)return {};
            found={t,row.binding.epoch,entity};
        }
        return found;
    }
    [[nodiscard]] bool submit_use(const rally_use::Receipt& receipt) noexcept {
        if(!receipt.binding.epoch || receipt.controller==UINT32_MAX || receipt.requester==UINT32_MAX
            || receipt.playerEntity==UINT32_MAX || receipt.completed<=0)return false;
        for(std::size_t i=0;i<count_;++i) {
            auto& row=bindings_[i];
            if(row.submitted && !row.used && row.binding.epoch==receipt.binding.epoch
                && row.entity==receipt.binding.entity && row.entity!=UINT32_MAX
                && row.binding.ticket.definition.interactionDefinition
                && feedback::same(row.binding.ticket,receipt.binding.ticket)) {row.used=true;return true;}
        }
        return false;
    }
    [[nodiscard]] bool used(const Lease& lease) const noexcept {
        for(std::size_t i=0;i<count_;++i)if(bindings_[i].binding.ticket.lease==lease && bindings_[i].used)return true;
        return false;
    }
private:
    void erase(std::size_t at) noexcept {for(std::size_t i=at+1;i<queued_;++i) events_[i-1]=events_[i];--queued_;}
    struct Row {Binding binding{};bool submitted{};std::uint32_t entity{UINT32_MAX};bool used{};};
    std::array<Row,16> bindings_{};std::size_t count_{};
    std::array<Event,16> events_{};std::size_t queued_{};
    std::uint64_t nextEpoch_{};bool overflow_{};
};
[[nodiscard]] bool bind(const feedback::Ticket&) noexcept;
void release(state::activity::ActivityInstanceKey) noexcept;
[[nodiscard]] Binding lookup(std::uint32_t definition,std::uint32_t registry,std::uint16_t slot) noexcept;
[[nodiscard]] Binding lookup_definition(std::uint32_t definition) noexcept;
[[nodiscard]] bool submit(const Event&) noexcept;
[[nodiscard]] rally_use::Binding lookup_use(std::uint32_t definition,std::uint32_t entity) noexcept;
[[nodiscard]] bool submit_use(const rally_use::Receipt&) noexcept;
[[nodiscard]] bool used(const Lease&) noexcept;
[[nodiscard]] std::size_t drain(state::activity::ActivityInstanceKey,std::span<Event>,bool& overflow) noexcept;
} // namespace sunrise::server::runtime::activity::public_event::native_bridge
