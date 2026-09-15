#pragma once

#include "native_activity_definition.h"
#include "native_npc_animation_service.h"
#include "placement_service.h"
#include "population_service.h"
#include "../../../state/activity/coo/city_tower_social_d2_registries.h"
#include "../../../state/account/festival_mask.h"

namespace sunrise::server::runtime::activity::city_tower_social_d2 {

namespace coo = state::activity::coo;
namespace trusted = state::activity::coo::city_tower_social_d2;

inline constexpr auto kRegistries = trusted::kRegistries;
inline constexpr coo::ModuleBinding kPersistentModule{{0x80B4A0F4, 0x80B4A0F4, 0, 0}, 1};

inline constexpr coo::Asset kEvaAsset{
    0x7C6DE64F, 0x80B4AF52, 1, 0,
};
inline constexpr coo::Asset kForestLaunchAsset{
    0x7C6DE64F, 0x80B4AF55, 4, 1,
};
inline constexpr coo::Asset kEvaIdleAsset{
    0x7C6DE64F, 0x80B4AF5B, 42, 3,
};

inline constexpr coo::script::Capability kScriptCapabilities[]{
    {"persistent.start", "composition",
        {coo::Operation::mechanic, kPersistentModule.asset, 1, coo::Wait::requested}},
    {"eva", "nativeActivity",
        {coo::Operation::population, kEvaAsset, 1, coo::Wait::requested}},
    {"eva.idle", "nativeActivity",
        {coo::Operation::mechanic, kEvaIdleAsset, 1, coo::Wait::requested}},
    {"forest_launch", "nativeActivity",
        {coo::Operation::device, kForestLaunchAsset, 1, coo::Wait::requested}},
};

inline constexpr coo::script::ModuleCapability kModules[]{
    {"persistent", kPersistentModule},
};

inline constexpr coo::script::ParameterCapability kParameters[]{
    {"eva_count", 1, 1, 1, false},
};

inline constexpr std::array<population::Capability, 1> kPopulations{{
    // Tower Eva has no authored local type-66 network rule or tactical group.
    {&kRegistries[0], 0, 0, {}, false},
}};

inline constexpr std::array<placement::Capability, 1> kPlacements{{
    // Generation 1 is the stable one-shot object activation. The authored
    // placement remains native; no actor transform is supplied here.
    {&kRegistries[0], 1, 1, placement::interaction::Mode::unchanged},
}};

inline constexpr std::array<equipment_interaction::Gate, 1> kEquipmentInteractionGates{{
    {kForestLaunchAsset.registry, kForestLaunchAsset.slot,
        state::account::inventory::EquipmentSlot::helmet,
        state::account::festival_mask::kDefinitionHashes},
}};

inline constexpr std::array<npc_animation::Action, 1> kEvaAnimationActions{{
    {1, 0xCAEB4FC0, 0x811C9DC5, true},
}};

inline constexpr std::array<npc_animation::Capability, 1> kEvaAnimationCapabilities{{
    {&kRegistries[0], 3, kEvaAnimationActions},
}};

inline constexpr NativeAction kActions[]{
    {{coo::Operation::population, kEvaAsset, 1, coo::Wait::requested}, 0, "eva_count"},
    {{coo::Operation::mechanic, kEvaIdleAsset, 1, coo::Wait::requested}, 0, {}},
    {{coo::Operation::device, kForestLaunchAsset, 1, coo::Wait::requested}, 0, {}},
};

inline const coo::script::Profile kProfile{
    "city_tower_social_d2.native.v1", "nativeOtherActivities", coo::Schema::otherMissions,
    kScriptCapabilities, kModules, {}, {}, {}, {}, {}, {}, kParameters,
};

inline const NativeActivityDefinition kActivity{
    "city_tower_social_d2", L"city_tower_social_d2.json", 6, &kProfile,
    kRegistries, kPopulations, kPlacements, kActions, kPersistentModule,
    kEvaAnimationCapabilities, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
    {}, true, {}, kEquipmentInteractionGates, true,
};

} // namespace sunrise::server::runtime::activity::city_tower_social_d2
