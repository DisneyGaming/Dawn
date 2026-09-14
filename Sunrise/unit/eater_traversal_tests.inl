// Included by eater_of_worlds_tests.cpp after namespace aliases and check().
#include "../src/client/activity/eater_player_health_binding.h"

namespace eater_health_fixture {
namespace h=sunrise::client::activity::eater_player_health::native_fallback;
namespace native=sunrise::client::hooks::bootflow::gateway_native;
inline constexpr std::uint32_t kPlayer=0x5DFAA015U,kSelf=0x4DF9E0AAU;
inline constexpr std::uintptr_t kBase=0x10000,kAllocation=0x20000,kMetadata=0x30000,
    kRows=kMetadata+0x180,kHealth=kBase+0x400,kEntityRow=0x50000;
struct Read {
    std::array<std::byte,0x51000> bytes{};
    template<class T> void put(std::uintptr_t at,T value) noexcept { std::memcpy(bytes.data()+at,&value,sizeof value); }
    template<class T> bool value(std::uintptr_t at,T& value) noexcept {
        if(at>bytes.size() || sizeof value>bytes.size()-at) return false;
        std::memcpy(&value,bytes.data()+at,sizeof value);return true;
    }
    bool resolve(std::uint32_t handle,std::uintptr_t& base,std::uintptr_t* allocation=nullptr) noexcept {
        if(handle==10) { base=kBase;if(allocation)*allocation=kAllocation;return true; }
        if(handle==11) { base=kMetadata;if(allocation)*allocation=0;return true; }
        if(handle==kSelf) { base=kHealth;if(allocation)*allocation=0;return true; }
        return false;
    }
    bool weak(native::Weak value) noexcept {
        return (value.handle==kPlayer && value.serial==101)
            || (value.handle==kSelf && value.serial==202);
    }
    bool entity_row(native::Weak entity,std::uintptr_t& row) noexcept {
        if(!weak(entity) || entity.handle!=kPlayer) return false;
        row=kEntityRow;return true;
    }
    Read() noexcept {
        put(kEntityRow+4,std::uint32_t{});put(kEntityRow+12,kPlayer);put(kEntityRow+0x4C,std::uint32_t{10});
        put(kBase,std::uint32_t{});put(kBase+4,std::uint32_t{11});
        put(kMetadata+0x68,std::uint64_t{1});put(kMetadata+0x70,std::int64_t{0x100});
        put(kRows+0x14,std::int32_t{0x400});put(kAllocation+0x18,UINT32_MAX);
        put(kHealth,native::Ref{0x815B5A40U,h::kHealthKind,0xF98});
        put(kHealth+0x24,kSelf);put(kHealth+0x2C,kPlayer);put(kHealth+0x338,std::uint8_t{});
    }
};
inline void make_weak(native::Weak& out,std::uint32_t handle) noexcept {
    out={handle==kPlayer?101U:202U,handle};
}
} // namespace eater_health_fixture

static void eater_player_health_binding_tests() {
    namespace f=eater_health_fixture;
    namespace h=f::h;
    f::Read read{};sunrise::client::activity::nightfall_player::Observation out{};
    check(h::discover(read,f::kPlayer,f::make_weak,out) && out.entity==f::kPlayer
        && out.health==f::native::Weak{202U,f::kSelf} && !out.dead,
        "bundle fallback accepts the exact salted local-player health component");
    read.put(f::kHealth+0x338,std::uint8_t{1});
    check(h::discover(read,f::kPlayer,f::make_weak,out) && out.dead,
        "bundle fallback reads only the native health death bit");
    read.put(f::kHealth+0x2C,std::uint32_t{f::kPlayer+1});
    check(!h::discover(read,f::kPlayer,f::make_weak,out),
        "bundle fallback rejects a health component owned by another entity");
    h::DiscoveryGate gate{};gate.bind(7);
    check(gate.claim(1000) && !gate.claim(1001) && !gate.claim(1249) && gate.claim(1250),
        "reflected health discovery is throttled to four attempts per second");
    gate.bind(8);check(gate.claim(1251),"new Eater run resets the health discovery throttle");
    const auto revived=h::retained_decision(true,true,false,f::kPlayer,f::kPlayer);
    check(revived.publish && !revived.dead && !revived.discover,
        "retained dead-to-alive transition publishes the clearing live edge");
    const auto replacement=h::retained_decision(true,true,false,f::kPlayer,f::kPlayer+1);
    check(replacement.publish && !replacement.dead && replacement.discover,
        "retained living lease cannot suppress discovery of a replacement player");
    const auto spectator=h::retained_decision(true,true,true,f::kPlayer,f::kPlayer+1);
    check(spectator.publish && spectator.dead && spectator.discover,
        "retained corpse stays observable while replacement discovery continues");
}

static void eater_traversal_binding_tests() {
    eater_player_health_binding_tests();
    check(m::kTraversalDoors.size()==2,"mouth door binding count stays exact");
    check(m::kMouthCrossings.size()==2
        && m::kMouthCrossings[0].volume.registry!=m::kMouthCrossings[1].volume.registry
        && m::kMouthCrossings[0].volume.slot==2 && m::kMouthCrossings[1].volume.slot==2,
        "distinct mouth-crossing owners preserve their shared authored geometry");

    check(m::kAirlock.entranceDoor==m::find(0x93BF5E9DU,23,1)->asset
        && m::kAirlock.exitDoor==m::find(0x93BF5E9DU,23,2)->asset,
        "both named airlock doors keep their exact catalog identities");
    check(m::kFinalUnderbellyDoor==m::find(0x93BF5E9DU,23,0)->asset
        && m::kFinalUnderbellyDoor==m::kEjectionTube,
        "probable final tube door keeps the remaining same-class gate identity");
    check(m::find(0x93BF5E9DU,23,0)->component==m::find(0x93BF5E9DU,23,1)->component
        && m::find(0x93BF5E9DU,23,0)->sense==m::find(0x93BF5E9DU,23,1)->sense
        && m::find(0x93BF5E9DU,23,0)->authority==m::find(0x93BF5E9DU,23,1)->authority,
        "ejection tube uses the same native position wire as the airlock doors");

    check(m::kTraversalHoops.size()==7,"all seven authored hoops are retained");
    for(std::size_t i=0;i<m::kTraversalHoops.size();++i)
        check(m::traversal_hoop_index(m::kTraversalHoops[i])==i,
            "hoop lookup follows authored suffix order rather than package slot order");
    check(m::kTraversalHoops[3].slot==9 && m::kTraversalHoops[6].slot==8,
        "hoop suffix order preserves the package's non-monotonic slots");

    check(m::kBellyTraversalSliceSet==48
        && m::kThunderingWallCheckpointSpawn==0x7DA4DB1DU
        && m::kThunderingWallCheckpointJoin.pointCount==6,
        "thundering-wall checkpoint uses its exact six-point bubble-6 spawn set");
    check(m::kThunderingWallCheckpointJoin.containingVolumes.size()==2
        && m::kThunderingWallCheckpointJoin.containingVolumes[0].slot==39
        && m::kThunderingWallCheckpointJoin.containingVolumes[1]==m::kThunderingWallCheckpoint.volume,
        "checkpoint spawn joins both safe-area 19 and the spawnpoint-update volume");
    check(m::kBarrierArenaSpawn==0x68C397B7U && m::kBarrierArenaSpawnJoin.pointCount==6
        && m::kBarrierArenaSpawnJoin.containingVolumes.size()==3,
        "arena arrival uses its exact six-point bubble-6 spawn set");
    check(m::barrier_arena_arrival_volume(m::kArenaSpawnVolumes[0])
        && m::barrier_arena_arrival_volume(m::kArenaSpawnVolumes[1])
        && !m::barrier_arena_arrival_volume(m::kArenaSpawnVolumes[2]),
        "arena boundary accepts Argos and barrier polygons without conflating the broader belly volume");

    check(m::traversal_role_volume(m::kAirlock.interiorMonitor)==&m::kAirlock.interiorVolume
        && m::traversal_role_volume(m::kBellyTraversalMonitor)==&m::kBellyTraversal.volume
        && m::traversal_role_volume(m::kFinalUnderbellyDoor)==nullptr,
        "role lookup exposes only exact observation-to-volume joins");
}
