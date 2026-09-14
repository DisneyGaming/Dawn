#pragma once
#include "frame.h"
#include "health_bindings.h"
#include "carry_bindings.h"
#include "station_bindings.h"
#include "../coo/native_activity_clock.h"
#include "../coo/population_service.h"
#include "../coo/population_readiness_request.h"
#include "../coo/mission_runtime.h"
#include "../coo/task_costs.h"
#include "../../../middleware/bap/activity_message/combatant_sense.h"
#include "../../../middleware/bap/activity_message/scene_sense.h"
#include "../../../middleware/bap/activity_message/squad_sense.h"
namespace sunrise::state::activity::eater_of_worlds {
bool contains(const Volume&,Point) noexcept;
class Controller final : private coo::Services,private coo::MissionPorts<Frame> {
public:
    void reset() noexcept;
    bool select(const coo::script::Views&,std::uint64_t) noexcept;
    Frame update(std::uint64_t,std::uint64_t,bool,int) noexcept;
    void position(std::uint64_t,Point) noexcept;
    bool trigger(std::uint64_t,std::uint32_t,std::uint16_t) noexcept;
    void monitor(std::uint32_t,std::uint16_t,bool,std::int32_t,std::int32_t) noexcept;
    bool prepared(coo::Generation,coo::Asset) noexcept;
    bool object(const coo::ObjectReceipt&) noexcept;
    bool open_exit_grate() noexcept;
    bool grate_pose(const GratePoseReceipt&) noexcept;
    bool exit_grate_opened() const noexcept {return frame_.grate.open && frame_.grate.poseAcknowledged;}
    bool platform_pose(const PlatformPoseReceipt&) noexcept;
    bool platform_contact(const PlatformContact&) noexcept;
    void player(std::uint64_t,std::uint32_t) noexcept;
    bool player_health(std::uint64_t,std::uint32_t,bool) noexcept;
    bool arrival_motion(const ArrivalMotion&) noexcept;
    bool health(const HealthReceipt&,bool dead) noexcept;
    bool cranium(const CraniumReceipt&,bool held,std::uint32_t player) noexcept;
    bool station(const StationReceipt&,std::uint32_t player) noexcept;
    bool device(coo::Generation,coo::Asset,std::int16_t,float) noexcept;
    bool admitted(const EnemyReceipt&) noexcept;
    bool died(const EnemyReceipt&) noexcept;
    bool source_retired(std::uint64_t,std::uint32_t,std::uint16_t,std::uint32_t) noexcept;
    bool readiness(const EnemyReceipt& r,coo::EnemyReadiness v) noexcept {return population_.observe(r,v);}
    void capacity(std::uint64_t run,coo::PopulationCapacity v) noexcept {if(run==run_) population_.capacity(v);}
    coo::ReadinessRequest<EnemyReceipt> readiness_request(std::uint64_t now) noexcept;
    template<class Visit> void pending_enemies(Visit v) const noexcept {population_.pending(v);}
    template<class Visit> void living_enemies(Visit v) const noexcept {population_.living(v);}
    void costed(std::uint32_t,std::uint16_t,const coo::TaskCosts&) noexcept;
    void combatant(std::uint64_t,std::uint32_t,std::uint16_t,const middleware::bap::activity_message::combatant_sense::Output&) noexcept;
    void scene(std::uint64_t,std::uint32_t,std::uint16_t,const middleware::bap::activity_message::scene_sense::Output&) noexcept;
    void squad(std::uint64_t,std::uint32_t,std::uint16_t,const middleware::bap::activity_message::squad_sense::Output&) noexcept;
    bool submitted(std::uint64_t,std::uint32_t,std::int64_t,std::uint32_t,std::uint8_t,std::uint32_t) noexcept;
    bool due(std::uint64_t now) const noexcept {return views_ && (!frame_.finished || dialogue_.due(now));}
    const Frame& frame() const noexcept {return frame_;}
    coo::Generation generation() const noexcept {return lifecycle_.owner();}
    coo::Generation owner() const noexcept {return generation();}
    const coo::script::GraphView* graph() const noexcept {return views_ && frame_.section<views_->phases.size()?views_->phases[frame_.section]:nullptr;}
    coo::Diagnostics diagnostics() const noexcept {return executor_.diagnostics();}
private:
    bool publish(const coo::Command&) noexcept override;
    void cancel(const coo::Command&) noexcept override {}
    void update_module(std::uint32_t,const coo::MissionInput&,Frame&) noexcept override;
    std::uint32_t observations(std::uint64_t,const Frame& f) noexcept override {return f.checked?1U:0U;}
    bool request(coo::Asset,bool) noexcept;
    bool observed(const coo::CommandSpec&) const noexcept;
    bool entered(coo::Asset) const noexcept;
    bool current_group(std::uint32_t) const noexcept;
    bool platform_owner(const coo::ObjectReceipt&) const noexcept;
    bool path_goal(std::uint8_t) noexcept;
    void crossing_population(bool) noexcept;
    void retry_combat() noexcept;
    void retry_traversal() noexcept;
    bool stage_argos_intro() noexcept;
    void pose_argos_shell() noexcept;
    bool retiring_holdout() const noexcept;
    const coo::script::Views* views_{};
    std::uint64_t run_{},now_{};bool started_{};
    coo::LifecycleService lifecycle_{};coo::NativeActivityClock clock_{};
    coo::ObjectiveService objectives_{};coo::ObjectService<kObjectBindings.size()> objects_{};
    coo::PopulationService<EnemyReceipt,std::size(kSpawns),16> population_{};
    coo::ReadinessSchedule readinessSchedule_{};
    std::array<coo::TaskCosts,std::size(kSpawns)> costs_{};
    std::bitset<std::size(kSpawns)> resumeSources_{};
    std::array<HealthReceipt,std::size(kAssets)> healthOwners_{};
    std::array<CraniumReceipt,std::size(kAssets)> carryOwners_{};
    std::array<StationReceipt,std::size(kAssets)> stationOwners_{};
    coo::DialogueService<std::size(kDialogueRows)> dialogue_{};
    std::bitset<std::size(kDialogueRows)> submitted_{};
    std::array<std::uint64_t,std::size(kDialogueRows)> voiceEnd_{};
    std::bitset<std::size(kVolumes)> seen_{};
    std::bitset<std::size(kAssets)> triggered_{};
    std::bitset<8> regions_{};
    coo::MissionRuntime composition_{};coo::Executor executor_{};Frame frame_{};
};
}
