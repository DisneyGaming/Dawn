#pragma once
#include "registry_admission.h"
#include "../../../state/activity/lifecycle_generation.h"
#include "../../../middleware/bap/activity_message/native/native_npc_animation_authority.h"
#include <array>
#include <span>
#include <charconv>
#include <string_view>

namespace sunrise::server::runtime::activity::npc_animation {
namespace wire=middleware::bap::activity_message::native::npc_animation;
using Owner=state::activity::ActivityInstanceKey;
struct Action final {
    std::uint32_t id{},sequence{wire::kEmptyHash},completion{wire::kEmptyHash};
    bool retailVerified{};
};
struct Capability final {
    const registry::Definition* registry{};
    std::uint16_t slot{};
    std::span<const Action> actions{};
};
[[nodiscard]] inline bool valid(const Capability& capability) noexcept {
    if(!capability.registry || !registry::valid(*capability.registry)
        || capability.actions.empty() || capability.actions.size()>8) return false;
    unsigned slots{};
    for(const auto& slot:capability.registry->slots) if(slot.index==capability.slot) {
        if(slot.type!=42 || slot.componentClass!=wire::kRuntimeClass
            || slot.senseSchema!=UINT32_MAX || slot.authSchema!=wire::kSchema) return false;
        ++slots;
    }
    if(slots!=1) return false;
    for(std::size_t i=0;i<capability.actions.size();++i) {
        const auto& action=capability.actions[i];
        if(!action.id || action.sequence==wire::kEmptyHash) return false;
        for(std::size_t j=0;j<i;++j) if(capability.actions[j].id==action.id) return false;
    }
    return true;
}
struct Command final {
    Owner owner{};
    std::uint64_t boot{},expectedRevision{},request{};
    std::uint32_t registry{},action{};
    std::uint16_t slot{};
    bool stop{},development{};
};
enum class Result : std::uint8_t { accepted,stale,duplicate,invalid,unsupported,unverified,unchanged,exhausted };

// Explicit development mailbox; action zero means stop the selected controller.
// npc1 BOOT_HEX OWNER_HEX INCARNATION REVISION REQUEST REGISTRY_HEX SLOT ACTION
// Action IDs resolve only against copied trusted capabilities, never raw hashes.
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
    if(used!=tokens.size() || tokens[0]!="npc1") return false;
    std::array<std::uint64_t,8> values{};
    for(std::size_t i=0;i<values.size();++i) {
        const auto token=tokens[i+1];
        const auto result=std::from_chars(token.data(),token.data()+token.size(),values[i],
            (i==0 || i==1 || i==5)?16:10);
        if(result.ec!=std::errc{} || result.ptr!=token.data()+token.size()) return false;
    }
    if(!values[0] || !values[1] || !values[2] || !values[3] || !values[4]
        || !values[5] || values[5]>=UINT32_MAX || values[5]==wire::kEmptyHash
        || values[6]>32767 || values[7]>UINT32_MAX) return false;
    output={{values[1],{values[2]}},values[0],values[3],values[4],
        static_cast<std::uint32_t>(values[5]),static_cast<std::uint32_t>(values[7]),
        static_cast<std::uint16_t>(values[6]),values[7]==0,true};
    return true;
}

// One owner-thread service retains native control state. Publication is a
// request, never an animation-start/completion receipt. No timers or raw actor
// access occur here. All capability/action values are copied at begin().
class Service final {
public:
    [[nodiscard]] bool begin(Owner owner,std::uint64_t boot,std::span<const Capability> definitions) noexcept {
        if(owner_ || !owner || !boot || definitions.empty() || definitions.size()>bindings_.size()) return false;
        for(std::size_t i=0;i<definitions.size();++i) {
            if(!valid(definitions[i])) return false;
            for(std::size_t j=0;j<i;++j)
                if(definitions[i].registry->key==definitions[j].registry->key
                    && definitions[i].slot==definitions[j].slot) return false;
        }
        for(std::size_t i=0;i<definitions.size();++i) {
            const auto& source=definitions[i];auto& binding=bindings_[i];
            binding.registry=source.registry->key;binding.slot=source.slot;
            binding.bubble=source.registry->bubble;binding.actionCount=source.actions.size();
            for(std::size_t j=0;j<source.actions.size();++j) binding.actions[j]=source.actions[j];
        }
        count_=definitions.size();owner_=owner;boot_=boot;revision_=1;return true;
    }
    [[nodiscard]] Result request(const Command& command,std::uint32_t bubble) noexcept {
        if(!owner_ || owner_!=command.owner || boot_!=command.boot) return Result::stale;
        if(!command.request || command.request<=lastRequest_) return Result::duplicate;
        if(command.expectedRevision!=revision_) return Result::stale;
        if(command.stop ? command.action!=0 : command.action==0) return Result::invalid;
        for(std::size_t i=0;i<count_;++i) {
            auto& binding=bindings_[i];
            if(binding.registry!=command.registry || binding.slot!=command.slot) continue;
            if(binding.bubble!=bubble) return Result::stale;
            const Action* action{};
            if(!command.stop) {
                for(std::size_t j=0;j<binding.actionCount;++j)
                    if(binding.actions[j].id==command.action) action=&binding.actions[j];
                if(!action) return Result::unsupported;
                if(!action->retailVerified && !command.development) return Result::unverified;
            } else if(!binding.published || binding.control.sequence==wire::kEmptyHash) return Result::unchanged;
            if(revision_==UINT64_MAX || (!command.stop && binding.control.counter==0x7FFFFFFFU)) return Result::exhausted;
            if(command.stop) {
                binding.control.sequence=wire::kEmptyHash;binding.control.completion=wire::kEmptyHash;
            } else {
                binding.control.sequence=action->sequence;binding.control.completion=action->completion;
                ++binding.control.counter;
            }
            binding.published=true;lastRequest_=command.request;++revision_;return Result::accepted;
        }
        return Result::unsupported;
    }
    [[nodiscard]] wire::Batch project(std::uint32_t bubble) const noexcept {
        wire::Batch batch{};
        for(std::size_t i=0;i<count_;++i) {
            const auto& binding=bindings_[i];
            if(binding.published && binding.bubble==bubble)
                batch.entries[batch.count++]={binding.registry,binding.slot,binding.bubble,binding.control};
        }
        return batch;
    }
    [[nodiscard]] Owner owner() const noexcept { return owner_; }
    [[nodiscard]] std::uint64_t boot() const noexcept { return boot_; }
    [[nodiscard]] std::uint64_t revision() const noexcept { return revision_; }
    [[nodiscard]] std::uint64_t last_request() const noexcept { return lastRequest_; }
private:
    struct Binding final {
        std::uint32_t registry{};std::uint16_t slot{};std::uint8_t bubble{};
        std::array<Action,8> actions{};std::size_t actionCount{};
        wire::Control control{};bool published{};
    };
    std::array<Binding,16> bindings_{};std::size_t count_{};
    Owner owner_{};std::uint64_t boot_{},revision_{},lastRequest_{};
};
} // namespace sunrise::server::runtime::activity::npc_animation
