#pragma once
#include "native_activity_definition.h"
#include "haunted_forest_registries.h"
#include "haunted_forest_hud.h"

namespace sunrise::server::runtime::activity::haunted_forest::mode {
// Package-owned entry pieces and native timed-capture controller. Fresh plate
// occupancy arms the capture; the graph waits for the original controller's
// completion edge. Duration/frequency are explicit reconstructed host policy.
inline constexpr coo::ModuleBinding kModule{{0x81550015,0x81550015,0,0},1};
inline constexpr std::array<placement::Capability,6> kPlacements{{
    // All six authored sources are deferred: publish positive generation1,
    // retain package transforms, and keep it stable across ordinary refreshes.
    {&kRegistries[0],30,1},{&kRegistries[0],31,1},{&kRegistries[0],32,1},
    {&kRegistries[0],10,1},{&kRegistries[0],11,1},{&kRegistries[0],12,1},
}};
inline constexpr std::array<occupancy_wait::Binding,1> kOccupancy{{{&kRegistries[0],109,0}}};
inline constexpr forest_generator::wire::State kGeneratorIgnition=[] {
    forest_generator::wire::State value{};
    value.primary.overrides=forest_generator::wire::Enabled|forest_generator::wire::Anchors;
    value.primary.enabled=true;
    // HF south entrance is group 3. Native port and fixed walkway collision
    // meet at column 2 / height 2 (15 units per level); height 0 is 30 units low.
    // The wire override replaces all four groups, so retain the other three.
    value.primary.anchors={{{3,2,0,true},{1,0,0,true},{1,1,0,true},{2,2,0,true}}};
    return value;
}();
inline constexpr std::array<forest_generator::Action,1> kGeneratorActions{{{1,kGeneratorIgnition}}};
inline constexpr std::array<forest_generator::Capability,1> kGenerators{{
    {&kRegistries[0],98,kGeneratorActions,"forest.seed",{{{},{},{},{"forest.entry_column","forest.entry_height"}}}},
}};
// Exact 80C7063B position bands recovered through the package renderer/material:
// .2 red, .1 pale capture. Keep native interpolation (snap=false); the service
// owns positive revisions and leaves unrelated power/lock channels untouched.
inline constexpr std::array<world_device::Action,2> kPlateDeviceActions{{
    {1,world_device::Position,{{0.2F,0,false},{1.0F,0,false},{}}},
    {2,world_device::Position,{{0.1F,0,false},{1.0F,0,false},{}}},
}};
inline constexpr std::array<world_device::Capability,1> kDevices{{
    {&kRegistries[0],33,kPlateDeviceActions},
}};
inline const auto kDirectives=haunted_forest_hud::actions();
inline constexpr coo::Asset kDirectiveSource{0x1EB557AD,0x8155035A,68,0};
inline constexpr coo::script::Capability kCapabilities[]{
    {"persistent.start","composition",{coo::Operation::mechanic,kModule.asset,1,coo::Wait::requested}},
    {"entry.main","nativeActivity",{coo::Operation::device,{0x34D23982,0x815500A3,4,30},1,coo::Wait::requested}},
    {"entry.approach","nativeActivity",{coo::Operation::device,{0x34D23982,0x815500A6,4,31},1,coo::Wait::requested}},
    {"entry.plate","nativeActivity",{coo::Operation::device,{0x34D23982,0x815500A9,4,32},1,coo::Wait::requested}},
    {"entry.occupied","nativeActivity",{coo::Operation::observation,{0x34D23982,0x81550170,30,109},1,coo::Wait::observed}},
    {"next.main","nativeActivity",{coo::Operation::device,{0x34D23982,0x8155006D,4,10},1,coo::Wait::requested}},
    {"next.approach","nativeActivity",{coo::Operation::device,{0x34D23982,0x81550070,4,11},1,coo::Wait::requested}},
    {"next.plate","nativeActivity",{coo::Operation::device,{0x34D23982,0x81550073,4,12},1,coo::Wait::requested}},
    {"entry.capture","nativeActivity",{coo::Operation::mechanic,{0x34D23982,0x815500A9,4,32},1,coo::Wait::completed}},
    {"forest.start","nativeActivity",{coo::Operation::mechanic,{0x34D23982,0x8155015B,37,98},1,coo::Wait::requested}},
    {"hud.enter","nativeActivity",{coo::Operation::objective,kDirectiveSource,1,coo::Wait::requested}},
    {"hud.branch_one","nativeActivity",{coo::Operation::objective,kDirectiveSource,2,coo::Wait::requested}},
    {"entry.red","nativeActivity",{coo::Operation::device,{0x34D23982,0x815500AC,23,33},1,coo::Wait::requested}},
    {"entry.capturing","nativeActivity",{coo::Operation::device,{0x34D23982,0x815500AC,23,33},2,coo::Wait::requested}},
};
inline constexpr NativeAction kActions[]{
    {kCapabilities[1].spec,0,{}},{kCapabilities[2].spec,1,{}},{kCapabilities[3].spec,2,{}},
    {kCapabilities[4].spec,0,{}},
    {kCapabilities[5].spec,3,{}},{kCapabilities[6].spec,4,{}},{kCapabilities[7].spec,5,{}},
    {kCapabilities[8].spec,0,{}},
    {kCapabilities[9].spec,0,{}},
    {kCapabilities[10].spec,0,{}},{kCapabilities[11].spec,1,"forest.duration_ms"},
    {kCapabilities[12].spec,0,{}},{kCapabilities[13].spec,0,{}},
};
inline constexpr coo::script::ModuleCapability kModules[]{{"persistent",kModule}};
inline constexpr coo::script::ParameterCapability kParameters[]{
    {"host.tick_hz",1,120,30,false},
    {"capture.duration_ms",1000,30000,8000,false},
    {"forest.seed",0,UINT32_MAX,0,false},
    {"forest.entry_column",2,2,2,false},
    {"forest.entry_height",0,2,2,false},
    {"forest.duration_ms",60000,3600000,900000,false},
};
inline constexpr std::array<native_capture::Binding,1> kCaptures{{
    {2,0x80C01781,0x815B8B3B,0x4C8,0x248,"capture.duration_ms"},
}};
inline const coo::script::Profile kProfile{"haunted_forest.native.v1","nativeOtherActivities",
    coo::Schema::otherMissions,kCapabilities,kModules,{}, {}, {}, {}, {},{},kParameters};
inline constexpr std::int32_t kActivityOrdinals[]{78};
inline const NativeActivityDefinition kActivity{"infinite_abyss",L"infinite_abyss.json",13,
    &kProfile,kRegistries,{},kPlacements,kActions,kModule,{}, {}, {}, {}, {},kOccupancy,kActivityOrdinals,
    {},"host.tick_hz",kCaptures,kGenerators,kDirectives,kDirectiveSource,kDevices};
}
