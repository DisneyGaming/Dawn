#pragma once
#include "profile.h"
#include "reactor.h"
#include "doors.h"
#include "reactor_combat.h"
#include "arrival.h"
#include "../coo/lifecycle_service.h"
#include "../coo/objective_service.h"
#include "../coo/object_service.h"
#include <bitset>
namespace dawn::state::activity::eater_of_worlds {
struct EnemyReceipt {
    std::uint64_t run{};std::uint32_t actor{UINT32_MAX},owner{UINT32_MAX},generation{};
    std::uint16_t source{};std::uint32_t registry{};
    bool valid() const noexcept {return run && generation && registry && actor!=UINT32_MAX && owner!=UINT32_MAX;}
    friend bool operator==(const EnemyReceipt&,const EnemyReceipt&)=default;
};
struct NativeState {
    std::uint32_t generation{};float position{};
    bool managed{},desired{},prepared{},active{},acknowledged{},healthKnown{},destroyed{},carried{},dropped{},used{},retiring{};
    std::uint32_t carrier{UINT32_MAX};
    std::uint16_t usedCranium{UINT16_MAX};
    friend bool operator==(const NativeState&,const NativeState&)=default;
};
struct GratePoseReceipt final {
    coo::ObjectReceipt object{};
    std::uint32_t device{UINT32_MAX},deviceSerial{UINT32_MAX};
    std::int32_t revision{-1};float position{};
    friend bool operator==(const GratePoseReceipt&,const GratePoseReceipt&)=default;
};
struct GrateState final {
    // Revision zero first binds the authored closed pose. Every module-113
    // request advances it, including a retry after a same-room death.
    std::uint32_t revision{};bool open{},poseAcknowledged{};
    coo::ObjectReceipt object{};
    std::uint32_t device{UINT32_MAX},deviceSerial{UINT32_MAX};
    friend bool operator==(const GrateState&,const GrateState&)=default;
};
struct Frame {
    bool enabled{},checked{},finished{},restricted{},populationFault{},bossDead{},combatRetry{},routeComplete{};
    std::uint8_t section{},activeRow{coo::kNoDialogue};
    std::uint32_t spawnGeneration{},revision{},objective{},checkpointSpawnSet{};
    int checkpointSliceSet{-1},region{-1};
    std::uint64_t gameplayClockTicks{},endEpoch{};
    std::array<std::uint32_t,std::size(kDialogueRows)> generations{};
    std::array<NativeState,std::size(kAssets)> native{};
    std::array<std::uint8_t,std::size(kSpawns)> taskPlusOne{};
    ReactorState reactor{};
    GrateState grate{};
    ArrivalState arrival{};
    coo::ObjectiveState presentation{};coo::CompletionPublication completion{};
    // An unbound native mechanic stops at its real receipt boundary. This is
    // surfaced by diagnostics; it is never converted into a timed completion.
    std::uint32_t waitingMechanic{};
};
inline constexpr auto kObjectBindings=[] {
    std::array<coo::ObjectBinding,[] {std::size_t n{};for(const auto& a:kAssets) if(a.asset.type==4) ++n;return n;}()> out{};
    std::size_t i{};for(const auto& a:kAssets) if(a.asset.type==4) out[i++]={a.asset,{},0.F,true};return out;
}();
inline constexpr std::size_t object_index(coo::Asset a) noexcept {
    for(std::size_t i=0;i<kObjectBindings.size();++i) if(kObjectBindings[i].source==a) return i;return kObjectBindings.size();
}
// Narrow immutable views used by native receipt adapters. These projections
// avoid copying the complete mission Frame at high-frequency hook boundaries.
struct ObjectRequest {
    coo::Generation owner{};
    NativeState desired{};
    bool enabled{};
    std::uint32_t poseRevision{};
    bool raised{},poseAcknowledged{};
    friend bool operator==(const ObjectRequest&,const ObjectRequest&)=default;
};
inline ObjectRequest object_request(const Frame& frame,coo::Generation owner,
                                    std::size_t assetIndex) noexcept {
    ObjectRequest out{};out.owner=owner;
    if(assetIndex>=std::size(kAssets)) return out;
    out.desired=frame.native[assetIndex];
    const auto platform=platform_index(kAssets[assetIndex].asset);
    if(platform<std::size(kReactorPlatforms)) {
        out.poseRevision=frame.reactor.poseRevision[platform];
        out.raised=frame.reactor.raised[platform];
        out.poseAcknowledged=frame.reactor.poseAcknowledged[platform];
    }
    out.enabled=owner.valid() && frame.enabled && !frame.finished;
    return out;
}
struct GateRequest final {
    coo::Generation owner{};coo::Asset gate{};
    std::int16_t revision{-1};float position{};
    bool enabled{},acknowledged{};
    friend bool operator==(const GateRequest&,const GateRequest&)=default;
};
inline GateRequest gate_request(const Frame& frame,coo::Generation owner,coo::Asset gate) noexcept {
    GateRequest out{};out.owner=owner;
    if(!gate_device_binding(gate)) return out;
    const auto index=asset_index(gate);if(index>=std::size(kAssets)) return out;
    const auto& state=frame.native[index];out.gate=gate;
    out.enabled=owner.valid() && frame.enabled && !frame.finished && state.managed
        && state.desired && state.prepared && state.active && state.generation;
    out.revision=state.generation<=INT16_MAX?static_cast<std::int16_t>(state.generation):std::int16_t{-1};
    out.position=state.position;out.acknowledged=state.acknowledged;return out;
}
struct GrateRequest final {
    ObjectRequest object{};std::uint32_t revision{};bool open{},poseAcknowledged{};
    coo::ObjectReceipt acknowledgedObject{};
    std::uint32_t acknowledgedDevice{UINT32_MAX},acknowledgedDeviceSerial{UINT32_MAX};
    friend bool operator==(const GrateRequest&,const GrateRequest&)=default;
};
inline GrateRequest grate_request(const Frame& frame,coo::Generation owner) noexcept {
    const auto index=asset_index(kReactorExitGrate);GrateRequest out{};
    if(index>=std::size(kAssets)) return out;
    out.object=object_request(frame,owner,index);out.revision=frame.grate.revision;
    out.open=frame.grate.open;out.poseAcknowledged=frame.grate.poseAcknowledged;
    out.acknowledgedObject=frame.grate.object;out.acknowledgedDevice=frame.grate.device;
    out.acknowledgedDeviceSerial=frame.grate.deviceSerial;return out;
}
struct ContactRequest {
    ObjectRequest object{};
    std::size_t assetIndex{std::size(kAssets)},platformIndex{std::size(kReactorPlatforms)};
    std::uint8_t path{},next{};
    std::uint32_t attempt{},player{UINT32_MAX};
    bool ready{};
    friend bool operator==(const ContactRequest&,const ContactRequest&)=default;
};
inline ContactRequest contact_request(const Frame& frame,coo::Generation owner) noexcept {
    ContactRequest out{};
    const auto& reactor=frame.reactor;
    out.path=reactor.path;out.next=reactor.next;out.attempt=reactor.attempt;out.player=reactor.player;
    if(!owner.valid() || !out.path || out.path>kPathLengths.size()
        || out.next>=kPathLengths[out.path-1]) return out;
    for(std::size_t i=0;i<std::size(kReactorPlatforms);++i) {
        if(kReactorPlatforms[i].path+1==out.path && kReactorPlatforms[i].index==out.next) {
            out.platformIndex=i;break;
        }
    }
    if(out.platformIndex>=std::size(kReactorPlatforms)) return out;
    out.assetIndex=asset_index(kReactorPlatforms[out.platformIndex].source);
    if(out.assetIndex>=std::size(kAssets)) return out;
    out.object=object_request(frame,owner,out.assetIndex);
    const auto& desired=out.object.desired;
    out.ready=out.object.enabled && desired.managed && desired.desired && desired.prepared
        && desired.active && desired.acknowledged && out.object.raised
        && out.object.poseAcknowledged && out.player!=UINT32_MAX;
    return out;
}
struct SourceRequest {
    coo::Generation owner{};coo::Asset asset{};std::uint32_t generation{};
    bool enabled{},loaded{},managed{},retiring{},active{},desired{};
    friend bool operator==(const SourceRequest&,const SourceRequest&)=default;
};
inline SourceRequest source_request(const Frame& frame,coo::Generation owner,
                                    std::uint32_t registry,std::uint16_t slot) noexcept {
    SourceRequest out{};out.owner=owner;
    const auto spawn=spawn_index(registry,slot);if(spawn>=std::size(kSpawns)) return out;
    out.asset=kSpawns[spawn].asset;const auto asset=asset_index(out.asset);
    if(asset>=std::size(kAssets)) return {};
    const auto& state=frame.native[asset];out.generation=state.generation;
    out.managed=state.managed;out.retiring=state.retiring;out.active=state.active;out.desired=state.desired;
    out.enabled=owner.valid() && frame.enabled && !frame.finished;
    if(frame.region>=0 && frame.region<64 && frame.region%8==0)
        for(const auto& group:kGroups) if(group.key==registry) {
            out.loaded=(group.bubblesMask&(1U<<(frame.region/8)))!=0;break;
        }
    return out;
}
struct Cohort {std::uint32_t registry;std::uint16_t source;std::uint8_t count;bool required;};
inline constexpr auto kCohorts=[] {
    std::array<Cohort,std::size(kSpawns)> out{};
    for(std::size_t i=0;i<out.size();++i) out[i]={kSpawns[i].asset.registry,kSpawns[i].asset.slot,expected_enemy_count(kSpawns[i]),true};return out;
}();
struct Request {coo::Generation owner{};Frame frame{};};
inline constexpr coo::Asset kBossSource{0xE8D290A0U,0x8155C06FU,1,3};
inline constexpr coo::Asset kBossActor{0xE8D290A0U,0x80C421ABU,2,4};
struct ArrivalRequest {
    coo::Generation owner{};
    std::uint32_t player{UINT32_MAX},attempt{};
    bool enabled{};
    friend bool operator==(const ArrivalRequest&,const ArrivalRequest&)=default;
};
inline ArrivalRequest arrival_request(const Frame& f,coo::Generation owner) noexcept {
    return {owner,f.reactor.player,f.arrival.attempt,owner.valid() && f.enabled && !f.finished
        && f.section==3 && f.region==48 && f.arrival.launched && !f.arrival.landed};
}
inline constexpr bool reactor_platform(coo::Asset a) noexcept {
    return a.registry==0x686321C8U && a.type==4 && a.slot>=38 && a.slot<=93;
}
}
