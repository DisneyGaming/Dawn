#pragma once
#include "../../../middleware/bap/activity_message/native/forest_generator_route.h"
#include "registry_admission.h"
#include "../../../state/activity/lifecycle_generation.h"
#include <optional>
#include <span>
#include <string_view>

namespace dawn::server::runtime::activity::forest_generator {
namespace wire=middleware::bap::activity_message::native::forest_generator;
using Owner=state::activity::ActivityInstanceKey;
// Authored registry identity and trusted typed requests. Files may select these
// actions; they do not supply reflection tags, native addresses or room assets.
struct Action final {std::uint32_t id{};wire::State state{};std::optional<wire::Route> route{};};
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
    // Only the shared executor may use the separate cycle admission path.
    bool allowCycles=false;
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
        if(capability.actions[i].route && !wire::valid(*capability.actions[i].route))return false;
        for(const auto& names:capability.anchorParameters)
            if((!names.column.empty() || !names.height.empty())
                && !(capability.actions[i].state.primary.overrides&wire::Anchors)
                && !capability.actions[i].route)return false;
        if(capability.actions[i].route)
            for(std::size_t j=0;j<capability.anchorParameters.size();++j)
                if((!capability.anchorParameters[j].column.empty() || !capability.anchorParameters[j].height.empty())
                    && !wire::has_side(*capability.actions[i].route,j))return false;
        for(std::size_t j=0;j<i;++j)if(capability.actions[j].id==capability.actions[i].id)return false;
    }
    return true;
}
[[nodiscard]] inline bool valid_coordinates(const Capability& capability,
    const AnchorConfiguration& coordinates) noexcept {
    if(!valid(capability))return false;
    for(const auto& action:capability.actions) {
        for(std::size_t j=0;j<capability.anchorParameters.size();++j) {
            const auto& names=capability.anchorParameters[j];
            if(names.column.empty() && names.height.empty())continue;
            if(action.route) {
                const auto& grid=action.route->grid;
                if((!names.column.empty() && coordinates[j].column>=grid.columns)
                    || (!names.height.empty() && coordinates[j].height>=grid.heights))return false;
            } else if((!names.column.empty() && coordinates[j].column>127)
                || (!names.height.empty() && coordinates[j].height>127))return false;
        }
    }
    return true;
}
struct Command final {
    Owner owner{};std::uint64_t boot{},expectedRevision{},request{};
    std::uint32_t registry{},action{};std::uint16_t slot{};
};
struct CycleCommand final {
    Owner owner{};std::uint64_t boot{},expectedRevision{},request{};
    std::uint32_t registry{};std::uint16_t slot{};std::uint32_t action{};std::uint64_t cycle{};
};
enum class Result : std::uint8_t {accepted,unchanged,stale,duplicate,unsupported,exhausted};
// A retained server request. There is deliberately no nativeReady/completed
// result here: only real worker observations may supply either milestone.
class Service final {
    struct Binding final {
        wire::Request request{};std::array<Action,4> actions{};std::size_t count{};
        std::uint32_t baseSeed{},effectiveSeed{};std::uint64_t cycle{},cycleRequest{};
        std::uint32_t cycleAction{};bool allowCycles{},published{};
    };
public:
    [[nodiscard]] bool begin(Owner owner,std::uint64_t boot,std::span<const Capability> capabilities,
        std::span<const std::uint32_t> seeds={},std::span<const AnchorConfiguration> anchors={}) noexcept {
        if(owner_ || !owner || !boot || capabilities.size()>bindings_.size())return false;
        if(!seeds.empty() && seeds.size()!=capabilities.size())return false;
        if(!anchors.empty() && anchors.size()!=capabilities.size())return false;
        std::array<Binding,4> prepared{};
        for(std::size_t i=0;i<capabilities.size();++i) {
            const auto& source=capabilities[i];
            if(!valid(source))return false;
            if(!capabilities[i].seedParameter.empty() && seeds.empty())return false;
            bool hasCoordinates{};
            for(const auto& names:source.anchorParameters)
                hasCoordinates|=!names.column.empty() || !names.height.empty();
            if(hasCoordinates && anchors.empty())return false;
            if(!anchors.empty() && !valid_coordinates(source,anchors[i]))return false;
            for(std::size_t j=0;j<i;++j)
                if(capabilities[j].registry->key==source.registry->key
                    && capabilities[j].slot==source.slot)return false;
        }
        for(std::size_t i=0;i<capabilities.size();++i) {
            const auto& source=capabilities[i];
            auto& staged=prepared[i];
            staged.request.registry=source.registry->key;staged.request.slot=source.slot;
            staged.request.bubble=source.registry->bubble;staged.count=source.actions.size();
            staged.allowCycles=source.allowCycles;
            if(!source.seedParameter.empty()) {
                staged.baseSeed=seeds[i]?seeds[i]:derive_seed(owner,boot,source.registry->key,source.slot);
                staged.effectiveSeed=staged.baseSeed;
            }
            for(std::size_t j=0;j<source.actions.size();++j) {
                staged.actions[j]=source.actions[j];
                auto& state=staged.actions[j].state;
                if(staged.actions[j].route && !wire::resolve_route(*staged.actions[j].route,state))return false;
                if(staged.baseSeed)apply_seed(state,staged.baseSeed);
                for(std::size_t k=0;k<source.anchorParameters.size();++k) {
                    const auto& names=source.anchorParameters[k];
                    auto& anchor=state.primary.anchors[k];
                    if(!names.column.empty())anchor.a=static_cast<std::int8_t>(anchors[i][k].column);
                    if(!names.height.empty())anchor.b=static_cast<std::int8_t>(anchors[i][k].height);
                }
            }
        }
        bindings_=prepared;
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
            auto next=action->state;
            if(binding.baseSeed)apply_seed(next,binding.effectiveSeed);
            const bool unchanged=binding.published && binding.request.state==next;
            binding.request.state=next;binding.published=true;
            lastRequest_=command.request;++revision_;
            return unchanged?Result::unchanged:Result::accepted;
        }
        return Result::unsupported;
    }
    // Cycle admission only publishes a generator request. Actual worker
    // readiness and completion still arrive through the native progress observer.
    [[nodiscard]] Result begin_cycle(const CycleCommand& command,std::uint32_t bubble) noexcept {
        if(!owner_ || owner_!=command.owner || boot_!=command.boot
            || command.expectedRevision!=revision_)return Result::stale;
        if(!command.cycle || !command.request)return Result::unsupported;
        Binding* binding{};
        for(std::size_t i=0;i<count_;++i) {
            auto& candidate=bindings_[i];
            if(candidate.request.registry==command.registry && candidate.request.slot==command.slot) {
                binding=&candidate;break;
            }
        }
        if(!binding)return Result::unsupported;
        if(binding->request.bubble!=bubble)return Result::stale;
        if(binding->cycle && command.cycle==binding->cycle
            && command.request==binding->cycleRequest && command.action==binding->cycleAction)
            return Result::duplicate;
        if(binding->cycle && command.cycle<=binding->cycle)return Result::stale;
        if(!binding->allowCycles || !binding->baseSeed)return Result::unsupported;
        if(command.request<=lastRequest_)return Result::duplicate;
        const Action* action{};
        for(std::size_t i=0;i<binding->count;++i)
            if(binding->actions[i].id==command.action)action=&binding->actions[i];
        if(!action || !action->state.primary.enabled)return Result::unsupported;
        if(revision_==UINT64_MAX)return Result::exhausted;
        auto next=action->state;
        const auto seed=derive_cycle_seed(binding->baseSeed,command.cycle,binding->effectiveSeed);
        apply_seed(next,seed);
        binding->request.state=next;binding->published=true;binding->cycle=command.cycle;
        binding->cycleRequest=command.request;binding->cycleAction=command.action;
        binding->effectiveSeed=seed;lastRequest_=command.request;++revision_;
        return Result::accepted;
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
    static void apply_seed(wire::State& state,std::uint32_t seed) noexcept {
        state.primary.seed=seed;
        state.primary.overrides|=wire::Seed;
    }
    [[nodiscard]] static std::uint32_t mix_seed(std::uint64_t value) noexcept {
        value=(value^(value>>30))*0xBF58476D1CE4E5B9ULL;
        value=(value^(value>>27))*0x94D049BB133111EBULL;
        const auto result=static_cast<std::uint32_t>(value^(value>>31));
        return result?result:1U;
    }
    [[nodiscard]] static std::uint32_t derive_cycle_seed(std::uint32_t baseSeed,
        std::uint64_t cycle,std::uint32_t previous) noexcept {
        auto result=mix_seed(static_cast<std::uint64_t>(baseSeed)
            ^(cycle*0x9E3779B97F4A7C15ULL));
        // Avoid the only state that could make a cycle look like its prior
        // projection. The increment is deterministic and remains nonzero.
        while(result==previous || result==baseSeed)
            result=result==UINT32_MAX?1U:result+1U;
        return result;
    }
    // Reconstructed host policy. This is called once at admission, never at
    // projection time: refreshes and action changes cannot regenerate a run.
    [[nodiscard]] static std::uint32_t derive_seed(Owner owner,std::uint64_t boot,
        std::uint32_t registry,std::uint16_t slot) noexcept {
        auto value=owner.sessionId^(owner.incarnation.value*0x9E3779B97F4A7C15ULL)
            ^boot^(std::uint64_t{registry}<<16)^slot;
        return mix_seed(value);
    }
    std::array<Binding,4> bindings_{};std::size_t count_{};Owner owner_{};
    std::uint64_t boot_{},revision_{},lastRequest_{};
};
}
