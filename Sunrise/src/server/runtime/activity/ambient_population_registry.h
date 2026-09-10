#pragma once
#include "registry_admission.h"
#include <array>

namespace sunrise::server::runtime::activity::ambient_population {
// Optional authored registries are admitted only when the retained UE document
// enables their named parameter. They never synthesize source or device bodies.
struct RegistryBinding final {
    const registry::Definition* registry{};
    std::string_view enableParameter;
};
struct RegistryBatch final {
    std::array<const registry::Definition*,8> entries{};
    std::size_t count{};
};
template<class Definition,class Document>
[[nodiscard]] bool optional_registries(const Definition& definition,const Document& document,
    RegistryBatch& output) noexcept {
    output={};RegistryBatch candidate{};
    if(definition.optionalRegistries.size()>candidate.entries.size())return false;
    for(const auto& binding:definition.optionalRegistries) {
        if(!binding.registry || !registry::valid(*binding.registry) || binding.enableParameter.empty()
            || binding.registry->bubble!=definition.bubble || binding.registry->activity!=definition.activity)return false;
        const auto* parameter=document.views().parameter(binding.enableParameter);
        if(!parameter || parameter->value>1)return false;
        for(const auto& base:definition.registries)if(base.key==binding.registry->key)return false;
        for(const auto& other:definition.optionalRegistries)
            if(&other!=&binding && other.registry && other.registry->key==binding.registry->key)return false;
        if(parameter->value)candidate.entries[candidate.count++]=binding.registry;
    }
    output=candidate;return true;
}
template<class Definition>
[[nodiscard]] bool optional_binding(const Definition& definition,const registry::Definition* expected,
    std::string_view parameter) noexcept {
    if constexpr(requires {definition.optionalRegistries;}) {
        for(const auto& binding:definition.optionalRegistries)
            if(binding.registry==expected && binding.enableParameter==parameter)return true;
    }
    return false;
}
}
