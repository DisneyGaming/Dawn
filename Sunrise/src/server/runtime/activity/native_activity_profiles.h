#pragma once
#include "mercury_definition.h"
#include "haunted_forest_definition.h"
namespace sunrise::server::runtime::activity {
inline const std::array<const NativeActivityDefinition*,2> kNativeActivityProfiles{{&mercury::kActivity,&haunted_forest::mode::kActivity}};
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
