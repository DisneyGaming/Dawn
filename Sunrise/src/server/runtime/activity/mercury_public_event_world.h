#pragma once
#include "mercury_public_event_registries.h"
#include "world_object_runtime.h"

namespace sunrise::server::runtime::activity::mercury::public_events::world {
namespace coo=state::activity::coo;
// Package80F5E625/628 are the deferred shell sources. The separate mode0
// sources70..77 own real launch/catch physics; these must not be conflated.
inline constexpr std::array<world_object::Placement,13> kPlacements{{
    {{&kRegistries[2],27,1},0x80F58DF6,0x14D81F8D50F8E456ULL,true},
    {{&kRegistries[2],28,1},0x80F58DF6,0xFC3DF1E15C0B10B3ULL,true},
    {{&kRegistries[1],1,1},0x80F58E69,0x6D9C09013E83D948ULL,true},
    {{&kRegistries[1],70,1},0x80F58E98,0x96F3F321F063793DULL,false},
    {{&kRegistries[1],71,1},0x80F58E93,0x2266B571412026EBULL,false},
    {{&kRegistries[1],72,1},0x80F58E98,0x8CF3D5094F1F1C48ULL,false},
    {{&kRegistries[1],73,1},0x80F58E93,0xE9C1239A6AC29448ULL,false},
    {{&kRegistries[1],74,1},0x80F58E8D,0xA152498B88B74E9CULL,false},
    {{&kRegistries[1],75,1},0x80F58E88,0xB5B4B519BAD2DF63ULL,false},
    {{&kRegistries[1],76,1},0x80F58E8D,0x880DA759F8E4337DULL,false},
    {{&kRegistries[1],77,1},0x80F58E88,0x9DE4B56DB09BA152ULL,false},
    {{&kRegistries[1],78,1},0x80C1156F,0xB538EAF1E0E193DBULL,true},
    {{&kRegistries[1],79,1},0x80C1156F,0x50A920E9958CA0BFULL,true}
}};
// Native normalized device endpoints; exact visual rotation/fade acceptance
// remains a live check. Do not replace them with world transforms or velocity.
inline constexpr world_device::Action kShellActions[]{
    {1,world_device::Position|world_device::Power,{{0.0F,0,true},{1.0F,0,false},{}}},
    {2,world_device::Power,{{},{0.0F,0,false},{}}},
    {3,world_device::Position,{{1.0F,0,false},{},{}}},
    {4,world_device::Power,{{},{1.0F,0,false},{}}}
};
inline constexpr world_device::Action kEffectActions[]{
    {1,world_device::Position,{{1.0F,0,false},{},{}}},
    {2,world_device::Position,{{0.0F,0,false},{},{}}}
};
inline constexpr std::array<world_device::Capability,7> kDevices{{
    {&kRegistries[2],1,kShellActions},{&kRegistries[2],2,kShellActions},
    {&kRegistries[2],25,kEffectActions},{&kRegistries[2],26,kEffectActions},
    {&kRegistries[1],11,kEffectActions},
    {&kRegistries[1],80,kEffectActions},{&kRegistries[1],81,kEffectActions}
}};
[[nodiscard]] constexpr coo::CommandSpec place(std::size_t i,bool active=true) noexcept {
    const auto& p=kPlacements[i].capability;
    for(const auto& slot:p.registry->slots)if(slot.index==p.slot)
        return {coo::Operation::device,{p.registry->key,slot.descriptorTag,4,p.slot},active?1U:2U,coo::Wait::requested};
    return {};
}
[[nodiscard]] constexpr coo::CommandSpec device(std::size_t i,std::uint32_t action) noexcept {
    const auto& d=kDevices[i];
    for(const auto& slot:d.registry->slots)if(slot.index==d.slot)
        return {coo::Operation::device,{d.registry->key,slot.descriptorTag,23,d.slot},action,coo::Wait::requested};
    return {};
}
inline constexpr coo::CommandSpec kOpeningObjects[]{place(0),place(1),place(2),place(7),place(8),place(9),place(10)};
inline constexpr coo::CommandSpec kOpeningControls[]{device(0,1),device(1,1),device(2,1),device(3,2),device(4,1)};
inline constexpr coo::Step kOpeningSteps[]{
    {"request_world_cannon_objects_and_central_effect",0,kOpeningObjects},
    {"request_native_opening_device_state",1,kOpeningControls}};
inline constexpr coo::Definition kOpeningGraph{"crossroads_world_opening",coo::Schema::otherMissions,kOpeningSteps,{}};
inline constexpr coo::CommandSpec kOffCommands[]{place(7,false),place(8,false),place(9,false),place(10,false),device(0,2),device(1,2),device(2,2)};
inline constexpr coo::Step kOffSteps[]{{"withdraw_central_launch_physics_and_power",0,kOffCommands}};
inline constexpr coo::Definition kOffGraph{"crossroads_cannons_off",coo::Schema::otherMissions,kOffSteps,{}};
inline constexpr coo::CommandSpec kRotateCommands[]{device(0,3),device(1,3)};
inline constexpr coo::Step kRotateSteps[]{{"request_authored_shell_endpoint",0,kRotateCommands}};
inline constexpr coo::Definition kRotateGraph{"crossroads_cannons_reorient",coo::Schema::otherMissions,kRotateSteps,{}};
inline constexpr coo::CommandSpec kIslandObjects[]{place(3),place(4),place(5),place(6),place(11),place(12)};
inline constexpr coo::CommandSpec kIslandControls[]{device(0,4),device(1,4),device(3,1),device(5,1),device(6,1)};
inline constexpr coo::Step kIslandSteps[]{
    {"request_native_island_launch_routes_and_rings",0,kIslandObjects},
    {"request_island_ring_and_cannon_fields",1,kIslandControls}};
inline constexpr coo::Definition kIslandGraph{"crossroads_island_travel",coo::Schema::otherMissions,kIslandSteps,{}};
inline constexpr coo::CommandSpec kRetirePhysics[]{place(3,false),place(4,false),place(5,false),place(6,false),place(7,false),place(8,false),place(9,false),place(10,false)};
inline constexpr coo::CommandSpec kRetireControls[]{device(0,2),device(1,2),device(2,2),device(3,2),device(4,2),device(5,2),device(6,2)};
inline constexpr coo::CommandSpec kRetireVisuals[]{place(0,false),place(1,false),place(2,false),place(11,false),place(12,false)};
inline constexpr coo::Step kRetireSteps[]{
    {"withdraw_world_event_physics",0,kRetirePhysics},
    {"request_inactive_device_endpoints",1,kRetireControls},
    {"retire_deferred_world_event_visuals",2,kRetireVisuals}};
inline constexpr coo::Definition kRetireGraph{"crossroads_world_retirement",coo::Schema::otherMissions,kRetireSteps,{}};
inline constexpr std::uint32_t kOpening=1,kCannonsOff=2,kReorient=3,kIslands=4,kRetire=5;
inline constexpr world_object::Scene kScenes[]{
    {kOpening,&kOpeningGraph},{kCannonsOff,&kOffGraph},{kReorient,&kRotateGraph},{kIslands,&kIslandGraph},{kRetire,&kRetireGraph}};
inline constexpr world_object::Definition kDefinition{kPlacements,kDevices,kScenes};
// These graphs acknowledge publication only. The owning phase controller must
// choose when to request each scene from native phase/clock evidence. They do
// not claim creation, animation completion or successful player travel.
} // namespace sunrise::server::runtime::activity::mercury::public_events::world
