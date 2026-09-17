#pragma once
#include "ambient_population_definition.h"
#include "ambient_population_registry.h"
#include "../../../state/activity/coo/mercury_ambient_primary_owner.h"

namespace dawn::server::runtime::activity::mercury::ambient::cabal_probe {
namespace owner=state::activity::coo::mercury::ambient::primary_owner;
inline constexpr std::string_view kCountParameter="ambient_cabal_primary_probe_count";
inline constexpr std::array<ambient_population::named_points::Point,3> kPoints{{
    {0,0x80F74364,0x3175CA7E8643C2A9ULL},
    {5,0x80F4B3B3,0xD028E6A9CBB47BA6ULL},
    {6,0x80F4B3B3,0xC54292CEE0277A33ULL},
}};
inline constexpr ambient_population::named_points::Dependency kNamedDependency{
    &owner::kRegistries[0],0x80F5B9A4,19,kPoints};
inline constexpr std::array<ambient_population::RegistryBinding,2> kOptionalRegistries{{
    {&owner::kRegistries[0],kCountParameter},{&owner::kRegistries[1],kCountParameter},
}};
// Finite diagnostic choice: source0, primary rule8, objective2 row0 (engaged).
// Package bindings are proved; count1 and this source-to-row assignment are
// explicit test policy, not recovered retail scheduling. Native AI owns combat.
inline constexpr population::Capability kPopulation{&owner::kRegistries[1],0,8,{0x2571C34D,2,0},true};
[[nodiscard]] constexpr ambient_population::InitialBinding binding(std::uint16_t capability) noexcept {
    return {capability,4,kCountParameter,0,true,&kNamedDependency};
}
}
