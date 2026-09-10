#pragma once
#include "mercury_ambient_populations.h"
#include "ambient_population_definition.h"

namespace sunrise::server::runtime::activity::mercury::ambient::probe {
// Finite, explicitly labeled native probe beyond the pond. Package bytes prove
// the available source/rule/task choices below, not original host scheduling.
// The document selects zero (disabled) or one cumulative native request. There
// is no replacement and no inference that one is the retail squad population.
inline constexpr auto* kGroup=catalog::find(0xEB1E8934);
static_assert(kGroup && kGroup->sources.size()==1 && kGroup->tacticalRows==2);
inline constexpr std::array<registry::Definition,1> kRegistries{{*kGroup->registry}};
inline constexpr population::Capability kPopulation{&kRegistries[0],0,6,{0xEB1E8934,1,1},true};
inline constexpr std::string_view kCountParameter="ambient_vex_probe_count";
inline constexpr std::uint32_t kSourceDefinition=0x80F5B77F;
inline constexpr std::uint32_t kTemplateEntity=0x80C0D08A;
// Row 1's sole native task points at EB1E8934/45/12, as proved by the
// installed 80F5B78E objective. Native evaluation/AI owns all behavior thereafter.
inline constexpr std::uint16_t kFiringAreaSet=12;
[[nodiscard]] constexpr ambient_population::InitialBinding binding(std::uint16_t capability) noexcept {
    return {capability,3,kCountParameter,0,true};
}
} // namespace sunrise::server::runtime::activity::mercury::ambient::probe
