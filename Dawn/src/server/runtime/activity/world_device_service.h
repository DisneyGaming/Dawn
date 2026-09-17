#pragma once
#include "../../../middleware/bap/activity_message/native/world_device_authority.h"
#include "registry_admission.h"
#include "../../../state/activity/lifecycle_generation.h"
#include <span>
#include <limits>

namespace dawn::server::runtime::activity::world_device {
namespace wire=middleware::bap::activity_message::native::world_device;
using Owner=state::activity::ActivityInstanceKey;
enum ChannelMask : std::uint8_t {Position=1,Power=2,Lock=4};
// Trusted actions select channels and values; revisions belong to the service.
// No raw pointers, reflection tags, or wire revisions come from graph parameters.
struct Action final {std::uint32_t id{};std::uint8_t channels{};wire::State state{};};
struct Capability final {const registry::Definition* registry{};std::uint16_t slot{};std::span<const Action> actions{};};
inline constexpr std::size_t kMaxActions=8;
[[nodiscard]] inline bool valid(const Capability& capability) noexcept {
    if(!capability.registry || !registry::valid(*capability.registry)
        || capability.actions.empty() || capability.actions.size()>kMaxActions)return false;
    unsigned matches{};
    for(const auto& slot:capability.registry->slots)if(slot.index==capability.slot) {
        if(slot.type!=23 || slot.componentClass!=wire::kRuntimeClass
            || slot.senseSchema!=wire::kSenseSchema || slot.authSchema!=wire::kSchema)return false;
        ++matches;
    }
    if(matches!=1)return false;
    for(std::size_t i=0;i<capability.actions.size();++i) {
        const auto& action=capability.actions[i];
        if(!action.id || !action.channels || action.channels>7 || !wire::valid(action.state)
            || action.state.position.revision || action.state.power.revision || action.state.lock.revision)return false;
        for(std::size_t j=0;j<i;++j)if(capability.actions[j].id==action.id)return false;
    }
    return true;
}
struct Command final {Owner owner{};std::uint64_t boot{},expectedRevision{},request{};
    std::uint32_t registry{},action{};std::uint16_t slot{};};
enum class Result : std::uint8_t {accepted,unchanged,stale,duplicate,unsupported,exhausted};
class Service final {
public:
    [[nodiscard]] bool begin(Owner owner,std::uint64_t boot,std::span<const Capability> capabilities) noexcept {
        if(owner_ || !owner || !boot || capabilities.size()>bindings_.size())return false;
        for(std::size_t i=0;i<capabilities.size();++i) {
            if(!valid(capabilities[i]))return false;
            for(std::size_t j=0;j<i;++j)
                if(capabilities[j].registry->key==capabilities[i].registry->key
                    && capabilities[j].slot==capabilities[i].slot)return false;
        }
        for(std::size_t i=0;i<capabilities.size();++i) {
            const auto& source=capabilities[i];auto& target=bindings_[i];
            target.request.registry=source.registry->key;target.request.slot=source.slot;
            target.request.bubble=source.registry->bubble;target.count=source.actions.size();
            for(std::size_t j=0;j<source.actions.size();++j)target.actions[j]=source.actions[j];
        }
        count_=capabilities.size();owner_=owner;boot_=boot;revision_=1;return true;
    }
    [[nodiscard]] Result request(const Command& command,std::uint32_t bubble) noexcept {
        if(!owner_ || owner_!=command.owner || boot_!=command.boot || command.expectedRevision!=revision_)return Result::stale;
        if(!command.request || command.request<=lastRequest_)return Result::duplicate;
        for(std::size_t i=0;i<count_;++i) {
            auto& binding=bindings_[i];
            if(binding.request.registry!=command.registry || binding.request.slot!=command.slot)continue;
            if(binding.request.bubble!=bubble)return Result::stale;
            const Action* action{};
            for(std::size_t j=0;j<binding.count;++j)if(binding.actions[j].id==command.action)action=&binding.actions[j];
            if(!action)return Result::unsupported;
            if(revision_==UINT64_MAX)return Result::exhausted;
            auto next=binding.request.state;
            bool changed{};
            const auto update=[&](std::uint8_t mask,wire::Channel& target,const wire::Channel& desired) {
                if(!(action->channels&mask))return true;
                if(target.revision>0 && target.value==desired.value && target.snap==desired.snap)return true;
                if(target.revision==std::numeric_limits<std::int16_t>::max())return false;
                target.value=desired.value;target.snap=desired.snap;++target.revision;changed=true;return true;
            };
            if(!update(Position,next.position,action->state.position)
                || !update(Power,next.power,action->state.power)
                || !update(Lock,next.lock,action->state.lock))return Result::exhausted;
            binding.request.state=next;binding.published=true;
            lastRequest_=command.request;++revision_;
            return changed?Result::accepted:Result::unchanged;
        }
        return Result::unsupported;
    }
    [[nodiscard]] wire::Batch project(std::uint32_t bubble) const noexcept {
        wire::Batch output;
        for(std::size_t i=0;i<count_;++i)
            if(bindings_[i].published && bindings_[i].request.bubble==bubble)
                output.entries[output.count++]=bindings_[i].request;
        return output;
    }
    [[nodiscard]] Owner owner() const noexcept {return owner_;}
    [[nodiscard]] std::uint64_t boot() const noexcept {return boot_;}
    [[nodiscard]] std::uint64_t revision() const noexcept {return revision_;}
    [[nodiscard]] std::uint64_t last_request() const noexcept {return lastRequest_;}
    // Explicit retirement ensures no request crosses owner/boot lifetimes.
    void release() noexcept {*this={};}
private:
    struct Binding final {wire::Request request{};std::array<Action,kMaxActions> actions{};std::size_t count{};bool published{};};
    std::array<Binding,wire::kCapacity> bindings_{};std::size_t count_{};Owner owner_{};
    std::uint64_t boot_{},revision_{},lastRequest_{};
};
} // namespace dawn::server::runtime::activity::world_device
