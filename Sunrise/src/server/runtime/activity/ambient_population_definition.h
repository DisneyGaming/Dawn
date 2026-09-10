#pragma once
#include "ambient_population_activation.h"
#include "ambient_population_registry.h"
#include <string_view>

namespace sunrise::server::runtime::activity::ambient_population {
// Trusted profile binding; an immutable activity document supplies the target.
// The native source/rule/tactical capability is selected by the profile, and
// cannot be replaced by arbitrary definition-file identities.
struct InitialBinding final {
    std::uint16_t capability{},monitorSlot{};
    std::string_view countParameter;
    std::int32_t authorityToken{};
    bool development{};
    const named_points::Dependency* namedDependency{};
};

[[nodiscard]] inline bool same_registry(const registry::Definition& a,const registry::Definition& b) noexcept {
    if(a.activity!=b.activity || a.scenario!=b.scenario || a.key!=b.key || a.objectTag!=b.objectTag
        || a.bubbleHash!=b.bubbleHash || a.bubble!=b.bubble || a.slots.size()!=b.slots.size()) return false;
    for(const auto& left:a.slots) {
        bool found{};
        for(const auto& right:b.slots) if(left.index==right.index) {
            found=left.type==right.type && left.componentClass==right.componentClass
                && left.senseSchema==right.senseSchema && left.authSchema==right.authSchema
                && left.descriptorTag==right.descriptorTag;
            break;
        }
        if(!found) return false;
    }
    return true;
}

// Structural template avoids a NativeActivityDefinition include cycle. Count
// values are resolved once from the retained document, before publication.
template<class Definition,class Document>
[[nodiscard]] bool configure_initial(const Definition& definition,const Document& document,
    std::span<const InitialBinding> bindings,std::array<InitialPolicy,32>& output,std::size_t& count) noexcept {
    output={};count=0;
    if(bindings.size()>output.size() || definition.populations.size()>32) return false;
    if constexpr(requires {definition.optionalRegistries;}) {
        RegistryBatch selected{};
        if(!optional_registries(definition,document,selected))return false;
    }
    const auto* graph=document.views().role("persistent");
    if(!graph) return false;
    std::array<InitialPolicy,32> candidate{};std::size_t used{};
    for(std::size_t i=0;i<bindings.size();++i) {
        const auto& binding=bindings[i];
        if(binding.capability>=definition.populations.size() || binding.countParameter.empty()) return false;
        const auto& source=definition.populations[binding.capability];
        const auto* target=document.views().parameter(binding.countParameter);
        if(!source.registry || !target || target->value>63 || source.registry->bubble!=definition.bubble) return false;
        bool registered{};
        for(const auto& admitted:definition.registries)
            if(same_registry(*source.registry,admitted)) registered=true;
        if(!registered && !optional_binding(definition,source.registry,binding.countParameter))return false;
        if(binding.namedDependency) {
            if(!named_points::valid(*binding.namedDependency))return false;
            bool ownerRegistered{};
            for(const auto& admitted:definition.registries)
                if(same_registry(*binding.namedDependency->registry,admitted))ownerRegistered=true;
            if(!ownerRegistered && !optional_binding(definition,binding.namedDependency->registry,binding.countParameter))return false;
        }
        // Zero is an explicit disabled profile parameter. Still validate every
        // native binding so a later nonzero document cannot expose a bad route.
        const InitialPolicy policy{&source,binding.monitorSlot,static_cast<std::uint8_t>(target->value?target->value:1),
            binding.authorityToken,binding.development,binding.namedDependency};
        if(!valid(policy)) return false;
        for(std::size_t j=0;j<i;++j) {
            const auto& prior=definition.populations[bindings[j].capability];
            if(prior.registry->key==source.registry->key && prior.slot==source.slot) return false;
        }
        // One source cannot be owned by both an immediate graph action and an
        // occupancy gate. Otherwise the immediate action bypasses eligibility.
        for(const auto& step:graph->definition.steps) for(const auto& command:step.commands)
            if(command.asset.type==1 && command.asset.registry==source.registry->key && command.asset.slot==source.slot) return false;
        if(target->value) candidate[used++]=policy;
    }
    output=candidate;count=used;return true;
}
} // namespace sunrise::server::runtime::activity::ambient_population
