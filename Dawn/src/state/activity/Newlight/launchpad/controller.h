#pragma once
#include "mission.h"
#include "cinematics.h"
#include "transport.h"
#include "../../../../middleware/bap/activity_message/native_sense.h"
#include "ghost.h"
#include "../../coo/object_service.h"
#include "../../coo/population_service.h"
#include "../../coo/objective_service.h"
#include "../../coo/native_activity_clock.h"
#include "../../../../middleware/bap/activity_message/object_sense.h"
#include "../../../../middleware/bap/activity_message/squad_sense.h"
#include "../../../../middleware/bap/activity_message/device_sense.h"
#include <bitset>

namespace dawn::state::activity::newlight::launchpad {
struct EnemyReceipt {
    std::uint64_t run{};
    std::uint32_t actor{UINT32_MAX},owner{UINT32_MAX},generation{};
    std::uint16_t source{};std::uint32_t registry{};
    bool valid() const noexcept {return run && generation && actor!=UINT32_MAX && owner!=UINT32_MAX && registry;}
    friend bool operator==(const EnemyReceipt&,const EnemyReceipt&)=default;
};
struct NativeState {
    std::uint32_t generation{},sourceOwner{UINT32_MAX};float position{};
    std::array<std::int32_t,3> deviceVersions{};
    std::uint64_t sequenceStartTicks{};
    std::uint8_t sequenceRevision{},deviceSeen{};
    bool managed{},desired{},prepared{},active{},acknowledged{},snap{},deviceSynchronized{},sourceCleared{};
};
struct TacticalState {std::array<std::uint8_t,24> costs{};std::uint32_t known{},revision{};std::int8_t group{-1};};
struct PickupState {coo::ObjectReceipt binding{};std::uint64_t item{},seed{};bool armed{},used{},requested{},granted{};};
struct Frame {
    bool enabled{},finished{},fault{},lightRequested{},light{},shipFound{},ketch{},ketchStarted{},flareStarted{},assault{},towerRequested{};
    std::uint8_t section{},bubble{3},activeRow{coo::kNoDialogue};
    std::uint32_t spawnGeneration{},revision{},objective{},ketchRevision{},assaultDefeated{};std::uint64_t gameplayClockTicks{};
    std::array<std::uint32_t,std::size(kDialogue)> generations{};
    std::array<NativeState,std::size(kAssets)> native{};
    std::array<TacticalState,kSpawns.size()> tactics{};
    std::array<PickupState,std::size(kPickups)> pickups{};
    coo::ObjectiveState presentation{};coo::CompletionPublication completion{};
    cinematics::State cinematic{};transport::State skiff{};ghost::State ghost{};
    EnemyReceipt ghostActor{},firstVandal{};bool firstVandalStarted{},firstVandalNear{};
    std::array<EnemyReceipt,std::size(kAmbushCues)> ambushActors{};
};
struct Request {coo::Generation owner{};Frame frame{};};
struct GrantRequest {coo::Generation owner{};coo::ObjectReceipt binding{};std::uint8_t pickup{UINT8_MAX};std::uint64_t seed{};};
constexpr std::size_t asset_index(coo::Asset a) noexcept {
    const auto i=catalog_index(a.registry,a.type,a.slot);
    return i<std::size(kAssets) && kAssets[i].asset==a?i:std::size(kAssets);
}
constexpr std::size_t pickup_index(coo::Asset a) noexcept {
    for(std::size_t i=0;i<std::size(kPickups);++i) {if(kPickups[i]==a) {return i;}}return std::size(kPickups);
}
inline constexpr auto kObjects=[] {
    std::array<coo::ObjectBinding,[] {std::size_t n{};for(const auto& a:kAssets) {if(a.asset.type==4) {++n;}}return n;}()> out{};
    std::size_t i{};for(const auto& a:kAssets) {if(a.asset.type==4) {out[i++]={a.asset,{},0.F,true};}}return out;
}();
constexpr std::size_t object_index(coo::Asset a) noexcept {
    for(std::size_t i=0;i<kObjects.size();++i) {if(kObjects[i].source==a) {return i;}}return kObjects.size();
}
struct Population {std::uint16_t source;std::uint32_t registry;std::uint8_t count;bool required;};
inline constexpr auto kPopulation=[] {
    std::array<Population,kSpawns.size()> out{};
    for(std::size_t i=0;i<out.size();++i) {const auto& s=kSpawns[i];out[i]={s.source,s.registry,s.count,s.registry==kHangar};}return out;
}();
bool contains(const Volume&,Point) noexcept;
bool crosses(const Volume&,Point,Point) noexcept;
bool near_first_vandal(Point,Point) noexcept;
class Controller final : private coo::Services {
public:
    void reset() noexcept;
    bool select(std::uint64_t,std::uint64_t now=0) noexcept;
    bool advance(std::uint64_t,std::uint64_t,bool) noexcept;
    bool fly_in_complete(coo::Generation,std::uint64_t now) noexcept;
    bool arrival(coo::Generation,std::uint8_t,std::uint64_t) noexcept;
    bool cinematic(coo::Generation,const cinematics::Incident&,std::uint64_t) noexcept;
    bool retired(coo::Generation) noexcept;
    bool region(coo::Generation,std::int32_t) noexcept;
    void position(std::uint64_t,Point) noexcept;
    bool prepared(coo::Generation,coo::Asset) noexcept;
    bool object(const coo::ObjectReceipt&) noexcept;
    bool lights(coo::Generation,const coo::ObjectReceipt&) noexcept;
    bool device(coo::Generation,coo::Asset,const middleware::bap::activity_message::device_sense::Output&) noexcept;
    void actor(coo::Generation,coo::Asset,const middleware::bap::activity_message::combatant_sense::Output&) noexcept;
    void ghost_sample(coo::Generation,const EnemyReceipt&,ghost::Sample) noexcept;
    bool passenger(coo::Generation,coo::Asset,const middleware::bap::activity_message::native_sense::Passenger&) noexcept;
    bool source(coo::Generation,coo::Asset,const middleware::bap::activity_message::squad_sense::Output&) noexcept;
    bool use(coo::Generation,coo::Asset,const middleware::bap::activity_message::object_sense::Output&) noexcept;
    bool cache_looted(coo::Generation,const coo::ObjectReceipt&) noexcept;
    GrantRequest grant_request() const noexcept;
    bool granted(const GrantRequest&,std::uint64_t item) noexcept;
    bool admitted(const EnemyReceipt&) noexcept;
    bool entrance(coo::Generation,const EnemyReceipt&) noexcept;
    bool died(const EnemyReceipt&) noexcept;
    bool readiness(const EnemyReceipt& r,coo::EnemyReadiness v) noexcept {return population_.observe(r,v);}
    template<class V> void pending_enemies(V v) const noexcept {population_.pending(v);}
    bool submitted(std::uint64_t,std::uint32_t,std::uint8_t,std::uint32_t,std::uint64_t) noexcept;
    bool tower_arrived(coo::Generation) noexcept;
    coo::Generation owner() const noexcept {return lifecycle_.owner();}
    const Frame& frame() const noexcept {return frame_;}
    const Graph& graph() const noexcept {return mission().phases[frame_.section];}
    coo::Diagnostics diagnostics() const noexcept {return executor_.diagnostics();}
    auto step_state(std::size_t i) const noexcept {return executor_.step_state(i);}
    coo::StallDetail missing(const coo::CommandSpec&) const noexcept;
private:
    bool publish(const coo::Command&) noexcept override;
    void cancel(const coo::Command&) noexcept override {}
    bool request(coo::Asset,bool,float=0.F) noexcept;
    bool observed(const coo::CommandSpec&) const noexcept;
    bool cohort(Cohort) noexcept;
    bool cleared(Cohort) const noexcept;
    void reinforce() noexcept;
    coo::MarkerTarget marker(std::uint32_t) const noexcept;
    std::uint64_t run_{},now_{};bool started_{},phaseFinished_{},hasPosition_{};
    std::array<std::uint64_t,3> reinforcementAt_{};std::uint64_t nextReinforcement_{};
    std::uint8_t enteredRegions_{};
    Point previous_{};
    coo::Executor executor_{};coo::LifecycleService lifecycle_{};coo::NativeActivityClock clock_{};
    coo::ObjectiveService objectives_{};coo::DialogueService<std::size(kDialogue)> dialogue_{};
    coo::ObjectService<kObjects.size()> objects_{};coo::PopulationService<EnemyReceipt,kSpawns.size(),16> population_{};
    std::bitset<std::size(kVolumes)> seen_{};
    std::bitset<std::size(kDialogue)> submitted_{};std::array<std::uint64_t,std::size(kDialogue)> voiceEnd_{};
    cinematics::Sequence cinematics_{};Frame frame_{};
};
}
