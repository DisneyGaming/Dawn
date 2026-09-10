#pragma once
#include "../../../middleware/bap/activity_message/native/forest_generator_authority.h"
#include "registry_admission.h"
#include "../../../state/activity/lifecycle_generation.h"
#include <span>
#include <string_view>

namespace sunrise::server::runtime::activity::forest_generator {
namespace wire=middleware::bap::activity_message::native::forest_generator;
using Owner=state::activity::ActivityInstanceKey;
// Authored registry identity and trusted typed requests. Files may select these
// actions; they do not supply reflection tags, native addresses or room assets.
struct Action final {std::uint32_t id{};wire::State state{};};
struct AnchorParameters final {std::string_view column{},height{};};
struct AnchorValues final {std::uint32_t column{},height{};};
using AnchorConfiguration=std::array<AnchorValues,4>;
struct Capability final {
    const registry::Definition* registry{};
    std::uint16_t slot{};
    std::span<const Action> actions{};
    // Empty preserves the authored seed. A named parameter selects a fixed
    // seed, or zero for a host-derived seed retained for this incarnation.
    std::string_view seedParameter{};
    // Optional unsigned coordinate parameters. The complete anchor block must
    // already be supplied by every trusted action; unbound fields stay intact.
    std::array<AnchorParameters,4> anchorParameters{};
};
[[nodiscard]] inline bool valid(const Capability& capability) noexcept {
    if(!capability.registry || !registry::valid(*capability.registry)
        || capability.actions.empty() || capability.actions.size()>4)return false;
    unsigned matches{};
    for(const auto& slot:capability.registry->slots)if(slot.index==capability.slot) {
        if(slot.type!=37 || slot.componentClass!=wire::kRuntimeClass
            || slot.senseSchema!=wire::kSenseSchema || slot.authSchema!=wire::kSchema)return false;
        ++matches;
    }
    if(matches!=1)return false;
    for(std::size_t i=0;i<capability.actions.size();++i) {
        if(!capability.actions[i].id || !wire::valid(capability.actions[i].state))return false;
        for(const auto& names:capability.anchorParameters)
            if((!names.column.empty() || !names.height.empty())
                && !(capability.actions[i].state.primary.overrides&wire::Anchors))return false;
        for(std::size_t j=0;j<i;++j)if(capability.actions[j].id==capability.actions[i].id)return false;
    }
    return true;
}
struct Command final {
    Owner owner{};std::uint64_t boot{},expectedRevision{},request{};
    std::uint32_t registry{},action{};std::uint16_t slot{};
};
enum class Result : std::uint8_t {accepted,unchanged,stale,duplicate,unsupported,exhausted};
// A retained server request. There is deliberately no nativeReady/completed
// result here: only real worker observations may supply either milestone.
class Service final {
public:
    [[nodiscard]] bool begin(Owner owner,std::uint64_t boot,std::span<const Capability> capabilities,
        std::span<const std::uint32_t> seeds={},std::span<const AnchorConfiguration> anchors={}) noexcept {
        if(owner_ || !owner || !boot || capabilities.size()>bindings_.size())return false;
        if(!seeds.empty() && seeds.size()!=capabilities.size())return false;
        if(!anchors.empty() && anchors.size()!=capabilities.size())return false;
        for(std::size_t i=0;i<capabilities.size();++i) {
            if(!valid(capabilities[i]))return false;
            if(!capabilities[i].seedParameter.empty() && seeds.empty())return false;
            for(std::size_t j=0;j<capabilities[i].anchorParameters.size();++j) {
                const auto& names=capabilities[i].anchorParameters[j];
                if(!names.column.empty() && (anchors.empty() || anchors[i][j].column>127))return false;
                if(!names.height.empty() && (anchors.empty() || anchors[i][j].height>127))return false;
            }
            for(std::size_t j=0;j<i;++j)
                if(capabilities[j].registry->key==capabilities[i].registry->key
                    && capabilities[j].slot==capabilities[i].slot)return false;
        }
        for(std::size_t i=0;i<capabilities.size();++i) {
            const auto& source=capabilities[i];auto& target=bindings_[i];
            target.request.registry=source.registry->key;target.request.slot=source.slot;
            target.request.bubble=source.registry->bubble;target.count=source.actions.size();
            for(std::size_t j=0;j<source.actions.size();++j) {
                target.actions[j]=source.actions[j];
                if(!source.seedParameter.empty()) {
                    auto& recipe=target.actions[j].state.primary;
                    recipe.seed=seeds[i]?seeds[i]:derive_seed(owner,boot,source.registry->key,source.slot);
                    recipe.overrides|=wire::Seed;
                }
                for(std::size_t k=0;k<source.anchorParameters.size();++k) {
                    const auto& names=source.anchorParameters[k];
                    auto& anchor=target.actions[j].state.primary.anchors[k];
                    if(!names.column.empty())anchor.a=static_cast<std::int8_t>(anchors[i][k].column);
                    if(!names.height.empty())anchor.b=static_cast<std::int8_t>(anchors[i][k].height);
                }
            }
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
            const bool unchanged=binding.published && binding.request.state==action->state;
            binding.request.state=action->state;binding.published=true;
            lastRequest_=command.request;++revision_;
            return unchanged?Result::unchanged:Result::accepted;
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
private:
    // Reconstructed host policy. This is called once at admission, never at
    // projection time: refreshes and action changes cannot regenerate a run.
    [[nodiscard]] static std::uint32_t derive_seed(Owner owner,std::uint64_t boot,
        std::uint32_t registry,std::uint16_t slot) noexcept {
        auto value=owner.sessionId^(owner.incarnation.value*0x9E3779B97F4A7C15ULL)
            ^boot^(std::uint64_t{registry}<<16)^slot;
        value=(value^(value>>30))*0xBF58476D1CE4E5B9ULL;
        value=(value^(value>>27))*0x94D049BB133111EBULL;
        const auto result=static_cast<std::uint32_t>(value^(value>>31));
        return result?result:1U;
    }
    struct Binding final {wire::Request request{};std::array<Action,4> actions{};std::size_t count{};bool published{};};
    std::array<Binding,4> bindings_{};std::size_t count_{};Owner owner_{};
    std::uint64_t boot_{},revision_{},lastRequest_{};
};
}
