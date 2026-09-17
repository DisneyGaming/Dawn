#pragma once
#include "public_event_key_bridge.h"
#include "public_event_deferred_placement_bridge.h"
#include "placement_service.h"
#include "world_device_service.h"
#include "world_object_runtime.h"

namespace dawn::server::runtime::activity::public_event::keys {
struct Item final {Ticket ticket{};activity::placement::Capability visual{};std::uint8_t side{};};
struct Definition final {std::span<const Item> items{};std::span<const world_device::Capability> devices{};};
class Runtime final {
public:
    [[nodiscard]] static bool valid(const Definition& d) noexcept {
        if(d.items.empty() || d.items.size()>4 || d.devices.size()!=d.items.size())return false;
        for(std::size_t i=0;i<d.items.size();++i) {
            auto t=d.items[i].ticket;
            t.key.owner=t.sink.owner={1,{1}};
            t.key.boot=t.sink.boot=t.key.definitionRevision=t.sink.definitionRevision=
                t.key.selectionRevision=t.sink.selectionRevision=t.key.event=t.sink.event=1;
            activity::placement::wire::Batch p{};
            if(!keys::valid(t) || d.items[i].side>1 || !d.items[i].visual.registry
                || !activity::placement::project({&d.items[i].visual,1},t.key.bubble,p)
                || !world_device::valid(d.devices[i]) || d.devices[i].registry!=d.items[i].visual.registry
                || d.devices[i].registry->key!=t.key.registry)return false;
            for(std::size_t j=0;j<i;++j)if(t.key.definition==d.items[j].ticket.key.definition
                || t.sink.definition==d.items[j].ticket.sink.definition)return false;
        }
        return true;
    }
    [[nodiscard]] bool begin(const Definition& d,const world_object::Context& c) noexcept {
        if(definition_ || !valid(d) || !c.owner || !c.boot || !c.definitionRevision || !c.selectionRevision
            || !c.event || !c.arrived || !c.admitted || c.bubble!=d.items[0].ticket.key.bubble
            || !devices_.begin(c.owner,c.boot,d.devices))return false;
        definition_=&d;context_=c;
        for(std::size_t i=0;i<d.items.size();++i) {
            auto& t=tickets_[i];t=d.items[i].ticket;
            for(auto* p:{&t.key,&t.sink}) {
                p->owner=c.owner;p->boot=c.boot;p->definitionRevision=c.definitionRevision;
                p->selectionRevision=c.selectionRevision;p->event=c.event;
            }
            if(!bridge::bind(t) || !deferred_bridge::bind(t.sink) || !device(i,1))return false;
        }
        return true;
    }
    // Source-specific, centrally accepted death counts trigger exactly two
    // authored mainland keys per Keeper. Retries cannot reissue a live source.
    [[nodiscard]] bool update(const world_object::Context& c,std::array<std::size_t,2> deaths) noexcept {
        if(!definition_ || c.owner!=context_.owner || c.boot!=context_.boot
            || c.definitionRevision!=context_.definitionRevision || c.selectionRevision!=context_.selectionRevision
            || c.event!=context_.event || c.activity!=context_.activity)return false;
        if(!c.arrived || !c.admitted || c.bubble!=context_.bubble)return true;
        for(std::size_t i=0;i<definition_->items.size();++i) {
            if(!requested_[i] && deaths[definition_->items[i].side]) {
                if(!deferred_bridge::bind(tickets_[i].key))return false;requested_[i]=true;
            }
            const auto state=bridge::lookup(tickets_[i].key.definition);
            if(state.epoch && state.ticket==tickets_[i] && state.sinkDeposited && !deposited_[i]) {
                if(!device(i,2))return false;deposited_[i]=true;
            }
        }
        return true;
    }
    [[nodiscard]] bool append(activity::placement::wire::Batch& placements,world_device::wire::Batch& devices) const noexcept {
        if(!definition_)return true;
        auto p=placements;auto d=devices;
        if(p.count>p.entries.size() || d.count>d.entries.size())return false;
        const auto add=[&](const activity::placement::wire::Request& request) {
            if(p.count==p.entries.size())return false;
            for(std::size_t j=0;j<p.count;++j)if(p.entries[j].registry==request.registry && p.entries[j].slot==request.slot)return false;
            p.entries[p.count++]=request;return true;
        };
        for(std::size_t i=0;i<definition_->items.size();++i) {
            const auto& t=tickets_[i];
            activity::placement::wire::Request sink{t.sink.registry,t.sink.slot,t.sink.bubble};sink.generation=1;
            const auto native=deferred_bridge::lookup(t.sink.definition);
            if(native.created && native.binding.ticket==t.sink)
                sink.interactionMode=deposited_[i]?middleware::bap::activity_message::native::interaction::Mode::disabled:middleware::bap::activity_message::native::interaction::Mode::enabled;
            if(!add(sink))return false;
            const auto& v=definition_->items[i].visual;
            activity::placement::wire::Request visual{v.registry->key,v.slot,v.registry->bubble};visual.generation=1;
            if(!add(visual))return false;
            if(requested_[i]) {
                activity::placement::wire::Request key{t.key.registry,t.key.slot,t.key.bubble};key.generation=1;
                if(!add(key))return false;
            }
        }
        const auto projected=devices_.project(context_.bubble);
        for(std::size_t i=0;i<projected.count;++i) {
            const auto& r=projected.entries[i];if(d.count==d.entries.size())return false;
            for(std::size_t j=0;j<d.count;++j)if(d.entries[j].registry==r.registry && d.entries[j].slot==r.slot)return false;
            d.entries[d.count++]=r;
        }
        placements=p;devices=d;return true;
    }
    [[nodiscard]] unsigned deposited() const noexcept {unsigned n{};for(bool d:deposited_)if(d)++n;return n;}
    [[nodiscard]] bool complete() const noexcept {return definition_ && deposited()==definition_->items.size();}
private:
    bool device(std::size_t i,std::uint32_t action) noexcept {
        const auto& d=definition_->devices[i];
        const auto r=devices_.request({context_.owner,context_.boot,devices_.revision(),devices_.last_request()+1,d.registry->key,action,d.slot},context_.bubble);
        return r==world_device::Result::accepted || r==world_device::Result::unchanged;
    }
    const Definition* definition_{};world_object::Context context_{};world_device::Service devices_{};
    std::array<Ticket,4> tickets_{};std::array<bool,4> requested_{},deposited_{};
};
}
