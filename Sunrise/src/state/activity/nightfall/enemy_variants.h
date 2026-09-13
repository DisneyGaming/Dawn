#pragma once
#include <cstdint>
#include <span>

namespace sunrise::state::activity::nightfall {
// One installed source category whose Grandmaster selector (variant 5) replaces
// the standard selector (variant 0). This records native package identity only;
// it does not infer a champion subtype from a source or from an actor handle.
struct EnemySubstitution final {
    std::uint32_t registry{};
    std::uint16_t source{};
    std::uint32_t category{},standardEntity{},grandmasterEntity{};
};

[[nodiscard]] constexpr bool valid(const EnemySubstitution& value) noexcept {
    return value.registry && value.category && value.standardEntity && value.grandmasterEntity
        && value.standardEntity!=value.grandmasterEntity;
}

[[nodiscard]] constexpr bool substitution_source(
    std::span<const EnemySubstitution> values,std::uint32_t registry,std::uint16_t source) noexcept {
    for(const auto& value:values)
        if(value.registry==registry && value.source==source)return true;
    return false;
}

[[nodiscard]] constexpr const EnemySubstitution* substitution(
    std::span<const EnemySubstitution> values,std::uint32_t registry,
    std::uint16_t source,std::uint32_t category,std::uint32_t entity) noexcept {
    for(const auto& value:values)
        if(value.registry==registry && value.source==source
            && value.category==category && value.grandmasterEntity==entity)return &value;
    return nullptr;
}
}
