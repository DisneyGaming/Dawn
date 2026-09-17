#pragma once
#include "population_service.h"
#include "../../../state/activity/coo/mercury_ambient_catalog.h"

namespace dawn::server::runtime::activity::mercury::ambient {
namespace catalog=state::activity::coo::mercury::ambient;
enum class Rule : std::uint8_t { primary, fallback };

// Binding helper, not policy selection. Callers must explicitly choose the
// authored rule and an in-range native tactical row. No automatic fallback,
// source-ordinal row mapping, hotspot coexistence or retail count is inferred.
[[nodiscard]] inline bool capability(std::uint32_t key,std::uint16_t sourceSlot,Rule rule,
    std::uint8_t tacticalRow,population::Capability& output) noexcept {
    if(rule!=Rule::primary && rule!=Rule::fallback) return false;
    const auto* group=catalog::find(key);
    if(!group || tacticalRow>=group->tacticalRows) return false;
    for(const auto& source:group->sources) {
        if(source.slot!=sourceSlot) continue;
        population::Capability result{group->registry,sourceSlot,
            rule==Rule::primary?source.primaryRule:source.fallbackRule,
            {key,group->tacticalSlot,static_cast<std::int8_t>(tacticalRow)},true};
        if(!population::valid(result)) return false;
        output=result;return true;
    }
    return false;
}
} // namespace dawn::server::runtime::activity::mercury::ambient
