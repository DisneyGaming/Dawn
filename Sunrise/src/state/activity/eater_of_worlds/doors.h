#pragma once

#include <array>
#include <cstddef>

#include "catalog.h"

namespace sunrise::state::activity::eater_of_worlds {

/** Exact mouth device and its package-authored activation observation. */
struct DoorBinding final {
    coo::Asset device{};
    coo::Asset monitor{};
    coo::Asset volume{};
};

inline constexpr std::array<DoorBinding,2> kTraversalDoors{{
    {{0x5654D7FDU,0x80C43A65U,23,0},
     {0x5654D7FDU,0x8155C31BU,30,3},
     {0x5654D7FDU,0x8155C315U,60,6}},
    {{0x5654D7FDU,0x80C43A68U,23,1},
     {0x5654D7FDU,0x8155C31EU,30,4},
     {0x5654D7FDU,0x8155C315U,60,5}},
}};

[[nodiscard]] constexpr const DoorBinding* traversal_door(coo::Asset asset) noexcept {
    for(const auto& binding:kTraversalDoors) if(binding.device==asset) return &binding;
    return nullptr;
}

/**
 * Package-authored phase task and the volume with the same local role.
 *
 * These are identity joins only. They do not assert that entering the volume is
 * equivalent to receiving the native task edge. The controller may use a
 * volume as a bounded fallback only where that policy is explicit.
 */
struct TraversalPhaseBinding final {
    coo::Asset task{};
    coo::Asset volume{};
};

inline constexpr std::array<TraversalPhaseBinding,2> kMouthCrossings{{
    {{0x18C48E46U,0x8155C483U,31,0},{0x18C48E46U,0x8155C480U,60,2}},
    {{0xDA089B5BU,0x8155C48CU,31,0},{0xDA089B5BU,0x8155C489U,60,2}},
}};

inline constexpr TraversalPhaseBinding kMouthTraversal{
    {0x0941F43CU,0x8155C495U,31,0},
    {0x0941F43CU,0x8155C492U,60,3},
};

/** Exact bubble-6 airlock identities. Type-32 toggle behavior remains unbound. */
struct AirlockBinding final {
    coo::Asset entranceDoor{};
    coo::Asset exitDoor{};
    coo::Asset interiorMonitor{};
    coo::Asset exteriorMonitor{};
    coo::Asset interiorVolume{};
    coo::Asset exteriorVolume{};
    coo::Asset exteriorToggle{};
};

inline constexpr AirlockBinding kAirlock{
    {0x93BF5E9DU,0x80B49F01U,23,1},
    {0x93BF5E9DU,0x80B49F18U,23,2},
    {0x93BF5E9DU,0x8155C015U,30,18},
    {0x93BF5E9DU,0x8155C018U,30,19},
    {0x93BF5E9DU,0x8155C00FU,60,31},
    {0x93BF5E9DU,0x8155C00FU,60,36},
    {0x93BF5E9DU,0x80C42089U,32,20},
};

inline constexpr coo::Asset kEjectionTube{0x93BF5E9DU,0x80B49EF7U,23,0};
// The other two type-23 gates in this authored traversal group are the airlock
// entrance and exit. Package naming and exclusion make the remaining
// same-class gate the final-underbelly-door candidate; live acceptance must
// still prove that physical role and the authored launch response.
inline constexpr coo::Asset kFinalUnderbellyDoor=kEjectionTube;
inline constexpr coo::Asset kWindVolume{0x93BF5E9DU,0x80B49F28U,4,3};
inline constexpr coo::Asset kPiston{0x93BF5E9DU,0x80B49FC0U,23,12};
inline constexpr coo::Asset kAirlockExitObject{0x93BF5E9DU,0x80B49FD6U,4,13};
inline constexpr coo::Asset kReactorExitGrate{0x686321C8U,0x80C43CE0U,4,0};

/**
 * A type-23 gate drives a generic device owned by a separately spawned object.
 *
 * The exit pair is package-local and was confirmed in the live native graph:
 * the gate's weak target is at +1F0, while the object's unique generic device
 * is 80F3D672/80803910/+A78.  The weak target must be rebuilt for every object
 * incarnation because its full handle and address change after destruction.
 */
struct ObjectDeviceBinding final {
    std::uint32_t configuration{},kind{};std::uint64_t offset{};
    friend constexpr bool operator==(const ObjectDeviceBinding&,const ObjectDeviceBinding&)=default;
};
inline constexpr ObjectDeviceBinding kDoorObjectDevice{0x80F3D672U,0x80803910U,0xA78U};
struct GateDeviceBinding final {coo::Asset gate{};ObjectDeviceBinding device{};};
inline constexpr std::array<GateDeviceBinding,4> kGateDeviceBindings{{
    {kEjectionTube,{0x80C7069BU,0x80803910U,0xA78U}},
    {kAirlock.entranceDoor,kDoorObjectDevice},
    {kAirlock.exitDoor,kDoorObjectDevice},
    {kPiston,{0x80F3D6AAU,0x80803910U,0xA78U}},
}};
[[nodiscard]] constexpr const GateDeviceBinding* gate_device_binding(coo::Asset gate) noexcept {
    for(const auto& binding:kGateDeviceBindings) if(binding.gate==gate) return &binding;
    return nullptr;
}
struct GateObjectBinding final {coo::Asset gate{},object{};ObjectDeviceBinding device{};};
inline constexpr std::array<GateObjectBinding,1> kGateObjectBindings{{
    {kAirlock.exitDoor,kAirlockExitObject,kDoorObjectDevice},
}};
[[nodiscard]] constexpr const GateObjectBinding* gate_object_binding(coo::Asset object) noexcept {
    for(const auto& binding:kGateObjectBindings) if(binding.object==object) return &binding;
    return nullptr;
}

/** Seven package-named hoop objects, indexed by their authored hoop suffix. */
inline constexpr std::array<coo::Asset,7> kTraversalHoops{{
    {0x93BF5E9DU,0x80B49F55U,4,5},
    {0x93BF5E9DU,0x80B49F6CU,4,6},
    {0x93BF5E9DU,0x80B49F7CU,4,7},
    {0x93BF5E9DU,0x80B49FAAU,4,9},
    {0x93BF5E9DU,0x80B49FB4U,4,10},
    {0x93BF5E9DU,0x80B49FBDU,4,11},
    {0x93BF5E9DU,0x80B49F8DU,4,8},
}};
inline constexpr coo::Asset kTraversalTreasureChest{0x93BF5E9DU,0x80B49F39U,4,4};

/** Belly progression and the safe checkpoint trigger recovered beside the piston route. */
inline constexpr TraversalPhaseBinding kBellyTraversal{
    {0xE34F861DU,0x8155C306U,31,0},
    {0xE34F861DU,0x8155C303U,60,4},
};
inline constexpr TraversalPhaseBinding kThunderingWallCheckpoint{
    {0xE34F861DU,0x8155C30CU,31,2},
    {0xE34F861DU,0x8155C303U,60,3},
};
inline constexpr coo::Asset kBellyTraversalMonitor{0xE34F861DU,0x8155C309U,30,1};

/**
 * The Argos and barrier registries expose the same arena polygon. Observing
 * either volume proves natural arrival; it does not authorize either fight.
 */
inline constexpr std::array<coo::Asset,2> kBarrierArenaArrivalVolumes{{
    {0xE8D290A0U,0x8155C06CU,60,379}, // tv_quarantine_breakout
    {0x91264981U,0x8155C1D7U,60,319}, // tv_quarantine_cooking
}};

/** Exact spawn-set/volume joins recovered from all six authored points. */
struct TraversalSpawnJoin final {
    std::uint32_t spawnSet{};
    Point representative{};
    std::uint8_t pointCount{};
    std::span<const coo::Asset> containingVolumes{};
};
inline constexpr coo::Asset kThunderingWallSpawnVolumes[]{
    {0x93BF5E9DU,0x8155C00FU,60,39}, // tv_thundering_wall_safe_area_19
    {0xE34F861DU,0x8155C303U,60,3},  // tv_thundering_wall_spawnpoint_update
};
inline constexpr coo::Asset kArenaSpawnVolumes[]{
    {0xE8D290A0U,0x8155C06CU,60,379}, // tv_quarantine_breakout
    {0x91264981U,0x8155C1D7U,60,319}, // tv_quarantine_cooking
    {0xE34F861DU,0x8155C303U,60,4},   // tv_phase_belly_traversal
};
inline constexpr std::uint32_t kThunderingWallCheckpointSpawn=0x7DA4DB1DU;
inline constexpr std::uint32_t kBarrierArenaSpawn=0x68C397B7U;
inline constexpr int kBellyTraversalSliceSet=48;
inline constexpr TraversalSpawnJoin kThunderingWallCheckpointJoin{
    kThunderingWallCheckpointSpawn,{6.16500092F,-162.954346F,-929.097046F},6,kThunderingWallSpawnVolumes};
inline constexpr TraversalSpawnJoin kBarrierArenaSpawnJoin{
    kBarrierArenaSpawn,{-43.0622292F,-1303.73206F,-1909.28491F},6,kArenaSpawnVolumes};

[[nodiscard]] constexpr std::size_t traversal_hoop_index(coo::Asset asset) noexcept {
    for(std::size_t i=0;i<kTraversalHoops.size();++i) if(kTraversalHoops[i]==asset) return i;
    return kTraversalHoops.size();
}

[[nodiscard]] constexpr bool barrier_arena_arrival_volume(coo::Asset asset) noexcept {
    for(const auto& volume:kBarrierArenaArrivalVolumes) if(volume==asset) return true;
    return false;
}

/** Returns only an exact package role join; the caller chooses fallback policy. */
[[nodiscard]] constexpr const coo::Asset* traversal_role_volume(coo::Asset observation) noexcept {
    for(const auto& door:kTraversalDoors) if(door.monitor==observation) return &door.volume;
    for(const auto& crossing:kMouthCrossings) if(crossing.task==observation) return &crossing.volume;
    if(kMouthTraversal.task==observation) return &kMouthTraversal.volume;
    if(kAirlock.interiorMonitor==observation) return &kAirlock.interiorVolume;
    if(kAirlock.exteriorMonitor==observation) return &kAirlock.exteriorVolume;
    if(kBellyTraversal.task==observation || kBellyTraversalMonitor==observation)
        return &kBellyTraversal.volume;
    if(kThunderingWallCheckpoint.task==observation) return &kThunderingWallCheckpoint.volume;
    return nullptr;
}

static_assert(kTraversalDoors[0].device.registry==0x5654D7FDU
    && kTraversalDoors[0].device.slot==0 && kTraversalDoors[0].monitor.slot==3
    && kTraversalDoors[0].volume.slot==6 && kTraversalDoors[1].device.slot==1
    && kTraversalDoors[1].monitor.slot==4 && kTraversalDoors[1].volume.slot==5);
static_assert(kMouthCrossings[0].volume.registry!=kMouthCrossings[1].volume.registry
    && kMouthCrossings[0].volume.slot==kMouthCrossings[1].volume.slot);
static_assert(kAirlock.interiorMonitor.slot==18 && kAirlock.interiorVolume.slot==31
    && kAirlock.exteriorMonitor.slot==19 && kAirlock.exteriorVolume.slot==36);
static_assert(kTraversalHoops.size()==7 && kTraversalHoops[3].slot==9
    && kTraversalHoops[6].slot==8 && traversal_hoop_index(kTraversalHoops[6])==6);
static_assert(kBellyTraversal.task.type==31 && kBellyTraversalMonitor.type==30
    && kBellyTraversal.volume.type==60 && kThunderingWallCheckpoint.volume.slot==3);
static_assert(kBarrierArenaArrivalVolumes[0].slot==379
    && kBarrierArenaArrivalVolumes[1].slot==319);
static_assert(kFinalUnderbellyDoor==kEjectionTube && kFinalUnderbellyDoor.type==23
    && kFinalUnderbellyDoor.slot==0);
static_assert(kGateDeviceBindings[0].gate==kEjectionTube
    && kGateDeviceBindings[0].device.configuration==0x80C7069BU
    && kGateDeviceBindings[2].gate==kAirlock.exitDoor
    && kGateDeviceBindings[2].device==kDoorObjectDevice);
static_assert(kBellyTraversalSliceSet==48
    && kThunderingWallCheckpointJoin.spawnSet==kThunderingWallCheckpointSpawn
    && kThunderingWallCheckpointJoin.pointCount==6
    && kThunderingWallCheckpointJoin.containingVolumes.size()==2);
static_assert(kBarrierArenaSpawnJoin.spawnSet==kBarrierArenaSpawn
    && kBarrierArenaSpawnJoin.pointCount==6 && kBarrierArenaSpawnJoin.containingVolumes.size()==3);
static_assert(traversal_role_volume(kAirlock.interiorMonitor)==&kAirlock.interiorVolume
    && traversal_role_volume(kBellyTraversalMonitor)==&kBellyTraversal.volume
    && traversal_role_volume(kEjectionTube)==nullptr);

} // namespace sunrise::state::activity::eater_of_worlds
