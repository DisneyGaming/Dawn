#pragma once
#include "mercury_definition.h"
#include "haunted_forest_definition.h"
#include "open_world_definitions.h"
namespace sunrise::server::runtime::activity {
inline const std::array<const NativeActivityDefinition*,8> kNativeActivityProfiles{{
    &mercury::kActivity,
    open_world::profiles::kActivities[0],open_world::profiles::kActivities[1],
    open_world::profiles::kActivities[2],open_world::profiles::kActivities[3],
    open_world::profiles::kActivities[4],open_world::profiles::kActivities[5],
    &haunted_forest::mode::kActivity,
}};
[[nodiscard]] inline const NativeActivityDefinition* native_activity_profile(std::string_view activity,
    std::int32_t activityOrdinal=-1) noexcept {
    const NativeActivityDefinition* result{};
    for(const auto* definition:kNativeActivityProfiles) if(definition->activity==activity) {
        if(!definition->activityOrdinals.empty()) {
            bool selected{};
            for(const auto ordinal:definition->activityOrdinals)if(ordinal==activityOrdinal)selected=true;
            if(!selected)continue;
        }
        if(result) return nullptr;result=definition;
    }
    return result;
}
}
