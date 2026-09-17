#pragma once
#include "public_event_deferred_placement_feedback.h"

namespace dawn::server::runtime::activity::public_event::keys {
namespace placement=deferred_placement;
struct Ticket final {
    placement::Ticket key{},sink{};
    std::uint32_t carrierDefinition{},interactionDefinition{};
    friend bool operator==(const Ticket&,const Ticket&)=default;
};
[[nodiscard]] inline bool valid(const Ticket& t) noexcept {
    return placement::valid(t.key) && placement::valid(t.sink)
        && t.key.owner==t.sink.owner && t.key.boot==t.sink.boot
        && t.key.definitionRevision==t.sink.definitionRevision && t.key.selectionRevision==t.sink.selectionRevision
        && t.key.event==t.sink.event && t.key.registry==t.sink.registry && t.key.bubble==t.sink.bubble
        && t.key.definition!=t.sink.definition && t.key.slot!=t.sink.slot
        && placement::tag(t.carrierDefinition) && placement::tag(t.interactionDefinition);
}
struct State final {
    Ticket ticket{};std::uint64_t epoch{},carryRevision{};
    std::uint32_t keyEntity{UINT32_MAX},sinkEntity{UINT32_MAX},holder{UINT32_MAX},carrier{UINT32_MAX};
    bool held{},deposited{},sinkDeposited{};
};
struct Use final {
    State state{},sink{};
    std::uint32_t controller{UINT32_MAX},requester{UINT32_MAX},playerEntity{UINT32_MAX};
    std::int32_t requested{},consumed{};
};
[[nodiscard]] inline bool same_encounter(const State& a,const State& b) noexcept {
    const auto& x=a.ticket.key;const auto& y=b.ticket.key;
    return a.epoch && b.epoch && x.owner==y.owner && x.boot==y.boot
        && x.definitionRevision==y.definitionRevision && x.selectionRevision==y.selectionRevision
        && x.event==y.event && x.registry==y.registry && x.bubble==y.bubble;
}
// Receipts come from the unchanged native creation/carry/use callbacks. Source
// publication and disappearance cannot stand in for any of these transitions.
class Mailbox final {
public:
    [[nodiscard]] bool bind(const Ticket& t) noexcept {
        if(!valid(t))return false;
        for(std::size_t i=0;i<count_;++i) {
            if(rows_[i].ticket==t)return true;
            if(rows_[i].ticket.key.definition==t.key.definition || rows_[i].ticket.sink.definition==t.sink.definition)return false;
        }
        if(count_==rows_.size() || epoch_==UINT64_MAX)return false;
        rows_[count_++]={t,++epoch_};return true;
    }
    [[nodiscard]] State lookup(std::uint32_t definition) const noexcept {
        for(std::size_t i=0;i<count_;++i)if(rows_[i].ticket.key.definition==definition)return rows_[i];return {};
    }
    [[nodiscard]] State carrier(std::uint32_t definition,std::uint32_t entity) const noexcept {
        if(entity==UINT32_MAX)return {};
        for(std::size_t i=0;i<count_;++i)if(!rows_[i].deposited && rows_[i].keyEntity==entity
            && rows_[i].ticket.carrierDefinition==definition)return rows_[i];return {};
    }
    [[nodiscard]] State sink(std::uint32_t definition,std::uint32_t entity) const noexcept {
        if(entity==UINT32_MAX)return {};
        for(std::size_t i=0;i<count_;++i)if(!rows_[i].sinkDeposited && rows_[i].sinkEntity==entity
            && rows_[i].ticket.interactionDefinition==definition)return rows_[i];return {};
    }
    // The native sink predicate decides whether the carried item is eligible.
    // Source ordering is not a key-to-sink restriction. Capture the current
    // holder independently and count the sink consumed by the original call.
    [[nodiscard]] State held(const State& sink,std::uint32_t holder) const noexcept {
        State result{};
        if(!sink.epoch || sink.sinkDeposited || holder==UINT32_MAX)return result;
        for(std::size_t i=0;i<count_;++i) {
            const auto& r=rows_[i];
            if(!r.held || r.deposited || r.holder!=holder || !same_encounter(r,sink))continue;
            if(result.epoch)return {};
            result=r;
        }
        return result;
    }
    [[nodiscard]] bool created(const placement::Observation& o) noexcept {
        if(!o.sequence || o.child==UINT32_MAX || static_cast<std::uint32_t>(o.weakChild>>32)!=o.child)return false;
        for(std::size_t i=0;i<count_;++i) {
            auto& r=rows_[i];
            if(o.ticket!=r.ticket.key && o.ticket!=r.ticket.sink)continue;
            auto& entity=o.ticket==r.ticket.key?r.keyEntity:r.sinkEntity;
            if(entity!=UINT32_MAX)return false;
            for(std::size_t j=0;j<count_;++j)if(rows_[j].keyEntity==o.child || rows_[j].sinkEntity==o.child)return false;
            entity=o.child;return true;
        }
        return false;
    }
    [[nodiscard]] bool carry(const State& before,std::uint32_t controller,std::uint32_t holder,bool held) noexcept {
        if(!before.epoch || controller==UINT32_MAX || (held && holder==UINT32_MAX))return false;
        for(std::size_t i=0;i<count_;++i) {
            auto& r=rows_[i];if(r.epoch!=before.epoch || r.ticket!=before.ticket || r.keyEntity!=before.keyEntity)continue;
            if(r.deposited || r.keyEntity==UINT32_MAX || r.carryRevision!=before.carryRevision
                || r.carryRevision==UINT64_MAX || (r.carrier!=UINT32_MAX && r.carrier!=controller))return false;
            if(r.held==held && r.holder==holder && r.carrier==controller)return true;
            // A new native carry attachment replaces this holder's previous
            // carried charge. Old items can remain resident after native use.
            if(held)for(std::size_t j=0;j<count_;++j) {
                auto& old=rows_[j];
                if(j!=i && old.held && old.holder==holder && same_encounter(old,r)) {
                    if(old.carryRevision==UINT64_MAX)return false;
                }
            }
            if(held)for(std::size_t j=0;j<count_;++j) {
                auto& old=rows_[j];
                if(j!=i && old.held && old.holder==holder && same_encounter(old,r)) {
                    old.held=false;old.holder=UINT32_MAX;++old.carryRevision;
                }
            }
            r.carrier=controller;r.holder=holder;r.held=held;++r.carryRevision;return true;
        }
        return false;
    }
    [[nodiscard]] bool deposit(const Use& use) noexcept {
        const auto& s=use.state;
        if(!same_encounter(s,use.sink) || !s.held || s.deposited || use.sink.sinkDeposited
            || s.holder==UINT32_MAX || s.holder!=use.playerEntity
            || use.controller==UINT32_MAX || use.requester==UINT32_MAX || use.consumed<0 || use.requested<=use.consumed)return false;
        State* destination{};
        for(std::size_t i=0;i<count_;++i) {
            auto& r=rows_[i];
            if(r.epoch==use.sink.epoch && r.ticket==use.sink.ticket
                && r.sinkEntity==use.sink.sinkEntity && !r.sinkDeposited)destination=&r;
        }
        if(!destination || destination->sinkEntity==UINT32_MAX)return false;
        for(std::size_t i=0;i<count_;++i) {
            auto& r=rows_[i];if(r.epoch!=s.epoch || r.ticket!=s.ticket)continue;
            // Native use may detach the charge during the call. Its captured
            // carry revision is retained separately by the callback wrapper;
            // another holder or a different item must never complete this use.
            if(r.deposited || r.keyEntity!=s.keyEntity || r.sinkEntity!=s.sinkEntity || r.holder!=s.holder
                || r.carryRevision!=s.carryRevision || r.carrier!=s.carrier)return false;
            r.deposited=true;r.held=false;destination->sinkDeposited=true;return true;
        }
        return false;
    }
    void release(placement::Owner owner) noexcept {
        for(std::size_t i=0;i<count_;)if(rows_[i].ticket.key.owner==owner)rows_[i]=rows_[--count_];else ++i;
    }
private:std::array<State,8> rows_{};std::size_t count_{};std::uint64_t epoch_{};
};
inline constexpr std::size_t kUseBytes=0x2E8;
[[nodiscard]] inline bool before_use(const State& state,const State& sink,std::span<const std::byte> before,
    std::uint32_t requester,std::uint32_t entity,Use& out) noexcept {
    using placement::field;out={};
    if(!same_encounter(state,sink) || sink.sinkDeposited || !state.held || state.deposited || entity==UINT32_MAX || entity!=state.holder
        || requester==UINT32_MAX || before.size()!=kUseBytes
        || field<std::uint32_t>(before,0)!=sink.ticket.interactionDefinition
        || field<std::uint32_t>(before,4)!=0x80804FB2 || field<std::int64_t>(before,8)!=0x388
        || field<std::uint32_t>(before,0x2C)!=sink.sinkEntity || before[0x2C0]!=std::byte{} || before[0x2D0]!=std::byte{})return false;
    const auto controller=field<std::uint32_t>(before,0x24);
    const auto requested=field<std::int32_t>(before,0x2DC),consumed=field<std::int32_t>(before,0x2D8);
    if(controller==UINT32_MAX || consumed<0 || requested<=consumed)return false;
    out={state,sink,controller,requester,entity,requested,consumed};return true;
}
[[nodiscard]] inline bool before_use(const State& state,std::span<const std::byte> before,
    std::uint32_t requester,std::uint32_t entity,Use& out) noexcept {
    return before_use(state,state,before,requester,entity,out);
}
[[nodiscard]] inline bool after_use(const Use& use,std::span<const std::byte> before,
    std::span<const std::byte> after) noexcept {
    using placement::field;
    return use.state.epoch && before.size()==kUseBytes && after.size()==kUseBytes
        && std::equal(before.begin(),before.begin()+16,after.begin())
        && field<std::uint32_t>(after,0x24)==use.controller && field<std::uint32_t>(after,0x2C)==use.sink.sinkEntity
        && after[0x2D0]==std::byte{1} && field<std::int32_t>(after,0x2D8)==use.requested
        && field<std::int32_t>(after,0x2DC)==use.requested
        && std::equal(before.begin()+0x2E0,before.end(),after.begin()+0x2E0);
}
namespace bridge {
[[nodiscard]] bool bind(const Ticket&) noexcept;
[[nodiscard]] State lookup(std::uint32_t) noexcept;
[[nodiscard]] State carrier(std::uint32_t,std::uint32_t) noexcept;
[[nodiscard]] State sink(std::uint32_t,std::uint32_t) noexcept;
[[nodiscard]] State held(const State&,std::uint32_t) noexcept;
[[nodiscard]] bool created(const placement::Observation&) noexcept;
[[nodiscard]] bool carry(const State&,std::uint32_t,std::uint32_t,bool) noexcept;
[[nodiscard]] bool deposit(const Use&) noexcept;
void release(placement::Owner) noexcept;
}
}
