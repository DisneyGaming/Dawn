#pragma once
#include "placement_service.h"
#include "world_device_service.h"
#include "../../../state/activity/coo/executor.h"

namespace dawn::server::runtime::activity::world_object {
namespace coo=state::activity::coo;
using Owner=state::activity::ActivityInstanceKey;
// The source mode is package evidence: immediate sources use auth.active;
// deferred sources additionally need an advancing native generation to retire.
struct Placement final {
    placement::Capability capability{};
    std::uint32_t entity{};
    std::uint64_t pointGuid{};
    bool deferred{};
};
struct Scene final {std::uint32_t id{};const coo::Definition* graph{};};
struct Definition final {
    std::span<const Placement> placements{};
    std::span<const world_device::Capability> devices{};
    std::span<const Scene> scenes{};
};
struct Context final {
    Owner owner{};std::uint64_t boot{},definitionRevision{},selectionRevision{},event{};
    std::int16_t activity{-1};std::uint32_t bubble{UINT32_MAX};bool arrived{},admitted{};
};
class Runtime final {
public:
    [[nodiscard]] static bool valid(const Definition& d) noexcept {
        if(d.placements.empty() || d.placements.size()>32
            || d.devices.size()>world_device::wire::kCapacity || d.scenes.empty() || d.scenes.size()>16)return false;
        for(std::size_t i=0;i<d.placements.size();++i) {
            const auto& p=d.placements[i];placement::wire::Batch scratch;
            if(!p.capability.registry || (i && p.capability.registry->bubble!=d.placements[0].capability.registry->bubble)
                || p.capability.generation!=1 || !p.entity || p.entity==UINT32_MAX
                || p.entity==0x811C9DC5 || !p.pointGuid
                || !placement::project({&p.capability,1},p.capability.registry->bubble,scratch) || scratch.count!=1)return false;
            for(std::size_t j=0;j<i;++j)if(same(p.capability,d.placements[j].capability))return false;
        }
        for(std::size_t i=0;i<d.devices.size();++i) {
            if(!world_device::valid(d.devices[i]) || d.devices[i].registry->bubble!=d.placements[0].capability.registry->bubble)return false;
            for(std::size_t j=0;j<i;++j)if(d.devices[i].registry->key==d.devices[j].registry->key && d.devices[i].slot==d.devices[j].slot)return false;
        }
        for(std::size_t i=0;i<d.scenes.size();++i) {
            const auto& s=d.scenes[i];
            if(!s.id || !s.graph || !coo::Executor::valid(*s.graph) || s.graph->schema!=coo::Schema::otherMissions)return false;
            for(std::size_t j=0;j<i;++j)if(d.scenes[j].id==s.id)return false;
            for(const auto& step:s.graph->steps)for(const auto& c:step.commands) {
                if(c.operation!=coo::Operation::device || c.wait!=coo::Wait::requested || !supported(d,c))return false;
            }
        }
        return true;
    }
    [[nodiscard]] bool begin(const Definition& d,const Context& c) noexcept {
        if(definition_ || !valid(d) || !c.owner || !c.boot || !c.definitionRevision || !c.selectionRevision
            || !c.event || c.activity<0 || c.bubble!=d.placements[0].capability.registry->bubble
            || !devices_.begin(c.owner,c.boot,d.devices))return false;
        definition_=&d;context_=c;return true;
    }
    [[nodiscard]] bool request(std::uint32_t scene,const Context& c) noexcept {
        if(!same(c) || failed_ || !eligible(c))return false;
        if(scene==scene_)return true;
        if(scene<=scene_ || (scene_ && executor_.diagnostics().phase!=coo::Phase::complete))return false;
        for(const auto& s:definition_->scenes)if(s.id==scene) {
            if(run_==UINT64_MAX)return false;
            executor_={};if(!executor_.start(*s.graph,++run_))return false;scene_=scene;return true;
        }
        return false;
    }
    [[nodiscard]] bool update(const Context& c) noexcept {
        if(!same(c) || failed_)return false;
        if(!scene_ || !eligible(c))return true;
        Driver driver(*this);executor_.update(driver);
        failed_=executor_.diagnostics().phase==coo::Phase::failed;return !failed_;
    }
    [[nodiscard]] bool append(placement::wire::Batch& placements,world_device::wire::Batch& devices) const noexcept {
        if(!definition_ || failed_ || placements.count>placements.entries.size() || devices.count>devices.entries.size())return false;
        auto p=placements;auto d=devices;
        for(std::size_t i=0;i<definition_->placements.size();++i)if(published_[i]) {
            const auto& r=placements_[i];if(p.count==p.entries.size())return false;
            for(std::size_t j=0;j<p.count;++j)if(p.entries[j].registry==r.registry && p.entries[j].slot==r.slot)return false;
            p.entries[p.count++]=r;
        }
        // Keep authority stable when the player enters another region. The
        // owning activity must retain each exact admitted registry as well.
        const auto projected=devices_.project(context_.bubble);
        for(std::size_t i=0;i<projected.count;++i) {
            const auto& r=projected.entries[i];if(d.count==d.entries.size())return false;
            for(std::size_t j=0;j<d.count;++j)if(d.entries[j].registry==r.registry && d.entries[j].slot==r.slot)return false;
            d.entries[d.count++]=r;
        }
        placements=p;devices=d;return true;
    }
    [[nodiscard]] bool requested() const noexcept {return scene_ && executor_.diagnostics().phase==coo::Phase::complete;}
    [[nodiscard]] std::uint32_t scene() const noexcept {return scene_;}
private:
    [[nodiscard]] static bool same(const placement::Capability& a,const placement::Capability& b) noexcept {
        return a.registry->key==b.registry->key && a.slot==b.slot;
    }
    [[nodiscard]] static coo::Asset asset(const registry::Definition& r,std::uint16_t index) noexcept {
        for(const auto& slot:r.slots)if(slot.index==index)return {r.key,slot.descriptorTag,slot.type,slot.index};return {};
    }
    [[nodiscard]] static bool supported(const Definition& d,const coo::CommandSpec& c) noexcept {
        for(const auto& p:d.placements)if(asset(*p.capability.registry,p.capability.slot)==c.asset)return c.argument==1 || c.argument==2;
        for(const auto& device:d.devices)if(asset(*device.registry,device.slot)==c.asset)
            for(const auto& action:device.actions)if(action.id==c.argument)return true;
        return false;
    }
    [[nodiscard]] bool same(const Context& c) const noexcept {
        return definition_ && c.owner==context_.owner && c.boot==context_.boot && c.definitionRevision==context_.definitionRevision
            && c.selectionRevision==context_.selectionRevision && c.event==context_.event && c.activity==context_.activity;
    }
    [[nodiscard]] bool eligible(const Context& c) const noexcept {
        if(!c.arrived || !c.admitted)return false;
        for(const auto& p:definition_->placements)if(p.capability.registry->bubble!=c.bubble)return false;
        for(const auto& d:definition_->devices)if(d.registry->bubble!=c.bubble)return false;
        return true;
    }
    struct Driver final:coo::Services {
        Runtime& owner;explicit Driver(Runtime& value):owner(value){}
        bool publish(const coo::Command& c) noexcept override {
            for(std::size_t i=0;i<owner.definition_->placements.size();++i) {
                const auto& p=owner.definition_->placements[i];
                if(asset(*p.capability.registry,p.capability.slot)!=c.spec.asset)continue;
                auto& r=owner.placements_[i];const bool active=c.spec.argument==1;
                if(!owner.published_[i]) {
                    r.registry=p.capability.registry->key;r.slot=p.capability.slot;r.bubble=p.capability.registry->bubble;r.generation=1;
                } else if(p.deferred && r.active!=active) {
                    if(r.generation==0x7FFFFFFFU)return false;++r.generation;
                }
                r.active=active;owner.published_[i]=true;return true;
            }
            if(owner.devices_.last_request()==UINT64_MAX)return false;
            const auto result=owner.devices_.request({owner.context_.owner,owner.context_.boot,owner.devices_.revision(),
                owner.devices_.last_request()+1,c.spec.asset.registry,c.spec.argument,c.spec.asset.slot},owner.context_.bubble);
            return result==world_device::Result::accepted || result==world_device::Result::unchanged;
        }
        void cancel(const coo::Command&) noexcept override {}
    };
    const Definition* definition_{};Context context_{};world_device::Service devices_{};coo::Executor executor_{};
    std::array<placement::wire::Request,32> placements_{};std::array<bool,32> published_{};
    std::uint64_t run_{};std::uint32_t scene_{};bool failed_{};
};
} // namespace dawn::server::runtime::activity::world_object
