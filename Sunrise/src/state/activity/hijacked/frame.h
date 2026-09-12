#pragma once
#include "bindings.h"
#include "mechanism_bindings.h"
#include "../coo/lifecycle_service.h"
#include "../coo/object_service.h"
#include "../coo/population_service.h"
#include <bitset>
#include "../../../server/runtime/activity/mission_capture_service.h"
namespace sunrise::state::activity::hijacked {
struct EnemyReceipt {
    std::uint64_t run{};std::uint32_t actor{UINT32_MAX},owner{UINT32_MAX},generation{};
    std::uint16_t source{};std::uint32_t registry{};
    bool valid() const noexcept { return run && generation && actor!=UINT32_MAX && owner!=UINT32_MAX && registry; }
    friend bool operator==(const EnemyReceipt&,const EnemyReceipt&)=default;
};
struct EnemyPosition {EnemyReceipt enemy{};Point point{};};
struct NativeState { std::uint32_t generation{};bool managed{},desired{},prepared{},active{},acknowledged{},retired{},suspended{};std::uint8_t survivingRequested{UINT8_MAX}; };
struct PlateReceipt {
    coo::Generation owner{};std::uintptr_t source{};std::uint8_t index{UINT8_MAX};
    std::uint32_t entity{UINT32_MAX},serial{UINT32_MAX},device{UINT32_MAX},timer{UINT32_MAX};
    bool valid() const noexcept { return owner.valid() && source>=0x10000 && index<std::size(kPlates) && entity!=UINT32_MAX && serial!=UINT32_MAX && device!=UINT32_MAX && timer!=UINT32_MAX; }
    friend bool operator==(const PlateReceipt&,const PlateReceipt&)=default;
};
struct ScanReceipt {
    coo::Generation owner{};std::uintptr_t source{};std::uint8_t index{UINT8_MAX};
    std::uint32_t entity{UINT32_MAX},serial{UINT32_MAX},controller{UINT32_MAX};
    bool valid() const noexcept { return owner.valid() && source>=0x10000 && index<std::size(kScans) && entity!=UINT32_MAX && serial!=UINT32_MAX && controller!=UINT32_MAX; }
    friend bool operator==(const ScanReceipt&,const ScanReceipt&)=default;
};
struct PlateState { bool armed{},occupied{},contested{},charged{};std::uint32_t revision{1}; };
struct Frame {
    bool enabled{},checked{},finished{},populationFault{},restricted{};std::uint8_t section{},activeRow{coo::kNoDialogue};
    std::uint32_t spawnGeneration{},revision{},objective{};std::uint64_t gameplayClockTicks{};
    std::array<std::uint32_t,std::size(kDialogue)> generations{};
    std::array<NativeState,std::size(kAssets)> native{};
    std::array<PlateState,std::size(kPlates)> plates{};std::array<bool,std::size(kScans)> scanArmed{},scanStarted{},scanComplete{};
    std::array<server::runtime::activity::mission_capture::Publication,std::size(kPlates)> plateCaptures{};
    coo::ObjectiveState presentation{};coo::CompletionPublication completion{};
    float bossHealth{1.F};std::uint8_t bossStage{};std::uint32_t bossRevision{};bool bossMoveRequested{},bossPositioned{};
};
inline constexpr std::size_t asset_index(coo::Asset a) noexcept {
    for(std::size_t i=0;i<std::size(kAssets);++i) { if(kAssets[i].asset==a) { return i; } }return std::size(kAssets);
}
inline float device_position(const Frame& frame,coo::Asset a) noexcept {
    return device_position(a,frame.native[asset_index(a)].active);
}
inline constexpr std::size_t spawn_index(coo::Asset a) noexcept {
    for(std::size_t i=0;i<std::size(kSpawns);++i) { if(kSpawns[i].registry==a.registry && kSpawns[i].source==a.slot && a.type==1) { return i; } }return std::size(kSpawns);
}
inline constexpr bool dialogue_identity(std::uint32_t definition,std::int64_t offset,std::uint32_t bank,std::uint8_t row) noexcept {
    const auto* binding=find(kDialogueAsset.registry,53,kDialogueAsset.slot);
    return binding && definition==kDialogueAsset.definition && offset==binding->offset && bank==kBank && row<std::size(kDialogue);
}
inline constexpr auto kObjectBindings=[] {
    std::array<coo::ObjectBinding,[] {std::size_t n{};for(const auto& a:kAssets) { if(a.asset.type==4) { ++n; } }return n;}()> out{};
    std::size_t i{};for(const auto& a:kAssets) { if(a.asset.type==4) {out[i++]={a.asset,{},0.F,true};} }return out;
}();
inline constexpr std::size_t object_index(coo::Asset a) noexcept {
    for(std::size_t i=0;i<kObjectBindings.size();++i) { if(kObjectBindings[i].source==a) {return i;} }return kObjectBindings.size();
}
struct Cohort { std::uint32_t registry;std::uint16_t source;std::uint8_t count;bool required; };
inline constexpr auto kCohorts=[] {
    std::array<Cohort,std::size(kSpawns)> out{};for(std::size_t i=0;i<out.size();++i) {out[i]={kSpawns[i].registry,kSpawns[i].source,kSpawns[i].count,kSpawns[i].required};}return out;
}();
// One source list owns exterior survivor scope and total ledger capacity.
inline constexpr std::array<std::uint16_t,7> kExteriorSources{1,2,3,4,5,6,7};
inline constexpr bool exterior_source(std::uint16_t slot) noexcept {
    for(const auto source:kExteriorSources) {if(source==slot) {return true;}}return false;
}
inline constexpr std::size_t kExteriorPopulation=[] {
    std::size_t count{};for(const auto slot:kExteriorSources) {
        const auto* source=spawn(0x3E9B74F3U,slot);
        if(!source || source->categories!=1 || source->requested[1]!=0 || source->count!=source->requested[0]) {return std::size_t{0};}
        count+=source->count;
    }return count;
}();
static_assert(kExteriorPopulation>0 && kExteriorPopulation<=256);
struct BossRequest {coo::Generation owner{};EnemyReceipt enemy{};std::uint8_t stage{};std::uint32_t revision{};bool requested{};};
struct Request { coo::Generation owner{};Frame frame{}; };
struct PlateRequest { coo::Generation owner{};PlateReceipt plate{};PlateState state{};bool enabled{};server::runtime::activity::mission_capture::Publication capture{}; };
struct ScanRequest { coo::Generation owner{};ScanReceipt scan{};bool enabled{},started{},complete{}; };
}
