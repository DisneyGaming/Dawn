#pragma once
#include "bindings.h"
#include "mechanism_bindings.h"
#include "../coo/lifecycle_service.h"
#include "../coo/object_service.h"
#include "../coo/population_service.h"
#include <bitset>
namespace sunrise::state::activity::deep_storage {
struct EnemyReceipt {
    std::uint64_t run{};std::uint32_t actor{UINT32_MAX},owner{UINT32_MAX},generation{};
    std::uint16_t source{};std::uint32_t registry{};
    bool valid() const noexcept { return run && generation && actor!=UINT32_MAX && owner!=UINT32_MAX && registry; }
    friend bool operator==(const EnemyReceipt&,const EnemyReceipt&)=default;
};
inline constexpr coo::Asset kLens{0x59700FA7U,0x80B568A6U,4,79};
inline constexpr coo::Asset kLensDevice{0x59700FA7U,0x80B56904U,23,90};
struct LensReceipt {
    coo::Generation owner{};std::uintptr_t source{};
    std::uint32_t entity{UINT32_MAX},serial{UINT32_MAX},health{UINT32_MAX};
    bool valid() const noexcept {return owner.valid() && source>=0x10000 && entity!=UINT32_MAX && serial!=UINT32_MAX && health!=UINT32_MAX;}
    friend bool operator==(const LensReceipt&,const LensReceipt&)=default;
};
struct LensRequest {coo::Generation owner{};LensReceipt lens{};std::uint32_t objectGeneration{};bool enabled{},vulnerable{},destroyed{};};
struct NativeState { std::uint32_t generation{};bool managed{},desired{},prepared{},active{},acknowledged{}; };
struct PlateReceipt {
    coo::Generation owner{};std::uintptr_t source{};std::uint8_t index{UINT8_MAX};
    std::uint32_t entity{UINT32_MAX},serial{UINT32_MAX},device{UINT32_MAX},timer{UINT32_MAX};
    bool valid() const noexcept { return owner.valid() && source>=0x10000 && index<3 && entity!=UINT32_MAX && serial!=UINT32_MAX && device!=UINT32_MAX && timer!=UINT32_MAX; }
    friend bool operator==(const PlateReceipt&,const PlateReceipt&)=default;
};
struct ScanReceipt {
    coo::Generation owner{};std::uintptr_t source{};std::uint8_t index{UINT8_MAX};
    std::uint32_t entity{UINT32_MAX},serial{UINT32_MAX},controller{UINT32_MAX};
    bool valid() const noexcept { return owner.valid() && source>=0x10000 && index<2 && entity!=UINT32_MAX && serial!=UINT32_MAX && controller!=UINT32_MAX; }
    friend bool operator==(const ScanReceipt&,const ScanReceipt&)=default;
};
struct PlateState { bool armed{},occupied{},contested{},charged{};std::uint32_t revision{1}; };
struct Frame {
    bool enabled{},checked{},finished{},populationFault{},restricted{};std::uint8_t section{},activeRow{coo::kNoDialogue};
    std::uint32_t spawnGeneration{},revision{},objective{};std::uint64_t gameplayClockTicks{};
    std::array<std::uint32_t,std::size(kDialogue)> generations{};
    std::array<NativeState,std::size(kAssets)> native{};
    std::array<PlateState,3> plates{};std::array<bool,2> scanArmed{},scanStarted{},scanComplete{};
    coo::ObjectiveState presentation{};coo::CompletionPublication completion{};
    bool lensExposed{},lensDestroyed{};
};
inline constexpr std::size_t asset_index(coo::Asset a) noexcept {
    for(std::size_t i=0;i<std::size(kAssets);++i) { if(kAssets[i].asset==a) { return i; } }return std::size(kAssets);
}
// 80F48031 native lens graph: 1 shielded, .75 exposed, 0 removed.
inline float device_position(const Frame& frame,coo::Asset a) noexcept {
    const auto& state=frame.native[asset_index(a)];
    if(a==kLensDevice) {return !state.active || frame.lensDestroyed?0.F:frame.lensExposed?.75F:1.F;}
    return device_position(a,state.active);
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
    std::array<Cohort,std::size(kSpawns)> out{};for(std::size_t i=0;i<out.size();++i) {out[i]={kSpawns[i].registry,kSpawns[i].source,kSpawns[i].count,true};}return out;
}();
struct Request { coo::Generation owner{};Frame frame{}; };
struct PlateRequest { coo::Generation owner{};PlateReceipt plate{};PlateState state{};bool enabled{}; };
struct ScanRequest { coo::Generation owner{};ScanReceipt scan{};bool enabled{},started{},complete{}; };
}
