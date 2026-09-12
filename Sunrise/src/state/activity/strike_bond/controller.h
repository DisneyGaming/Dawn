#pragma once
#include "frame.h"
#include "../coo/native_activity_clock.h"
#include "../coo/population_service.h"
#include "../coo/mission_runtime.h"
#include "../coo/task_costs.h"
#include "../coo/scene_service.h"
#include "../coo/combatant_state.h"
#include "../../../middleware/bap/activity_message/combatant_sense.h"
#include "../../../middleware/bap/activity_message/scene_sense.h"
#include "../../../middleware/bap/activity_message/squad_sense.h"

namespace sunrise::state::activity::strike_bond {
bool contains(const Volume&,Point) noexcept;
class Controller final : private coo::Services,private coo::MissionPorts<Frame> {
public:
    void reset() noexcept;
    bool select(const coo::script::Views&,std::uint64_t) noexcept;
    Frame update(std::uint64_t,std::uint64_t,bool,int) noexcept;
    void position(std::uint64_t,Point) noexcept;
    [[nodiscard]] const std::bitset<std::size(kVolumes)>& seen() const noexcept {return seen_;}
    [[nodiscard]] bool has_point() const noexcept {return hasPoint_;}
    [[nodiscard]] Point point() const noexcept {return lastPoint_;}
    [[nodiscard]] bool landed() const noexcept {return landed_;}
    bool player_trigger(std::uint64_t,std::uint32_t,std::uint16_t) noexcept;
    bool prepared(coo::Generation,coo::Asset) noexcept;
    bool object(const coo::ObjectReceipt&) noexcept;
    bool device(coo::Generation,coo::Asset,std::int16_t,float) noexcept;
    bool admitted(const EnemyReceipt&) noexcept;
    bool died(const EnemyReceipt&) noexcept;
    bool health(const EnemyReceipt&,float) noexcept;
    bool boss_motion(const EnemyReceipt&,const coo::ObjectReceipt&,float) noexcept;
    bool boss_animation(const EnemyReceipt&,std::uint8_t,BossAnimation) noexcept;
    EnemyReceipt boss_enemy() const noexcept {return bossEnemy_;}
    coo::ObjectReceipt boss_platform() const noexcept {return objects_.owner(object_index(kBossPlatform));}
    bool readiness(const EnemyReceipt& r,coo::EnemyReadiness v) noexcept {return population_.observe(r,v);}
    template<class Visit> void pending_enemies(Visit visit) const noexcept {population_.pending(visit);}
    template<class Visit> void living_enemies(Visit visit) const noexcept {population_.living(visit);}
    bool costed(std::uint32_t,std::uint16_t,const coo::TaskCosts&,std::int8_t&,std::uint32_t&) noexcept;
    bool lens(const LensReceipt&,bool) noexcept;
    LensRequest lens_request(std::size_t) const noexcept;
    bool submitted(std::uint64_t,std::uint32_t,std::uint8_t,std::uint32_t,std::uint64_t) noexcept;
    void generator(std::uint64_t,std::uint32_t,std::uint16_t,std::uint32_t,std::uint32_t) noexcept;
    void scene(std::uint64_t,std::uint32_t,std::uint16_t,const middleware::bap::activity_message::scene_sense::Output&) noexcept;
    void combatant(std::uint64_t,std::uint32_t,std::uint16_t,const middleware::bap::activity_message::combatant_sense::Output&) noexcept;
    void squad(std::uint64_t,std::uint32_t,std::uint16_t,const middleware::bap::activity_message::squad_sense::Output&) noexcept;
    const Frame& frame() const noexcept {return frame_;}
    coo::Generation owner() const noexcept {return lifecycle_.owner();}
    const coo::script::GraphView* graph() const noexcept {return !views_?nullptr:frame_.ending?views_->role("ending"):frame_.section<views_->phases.size()?views_->phases[frame_.section]:nullptr;}
    coo::Diagnostics diagnostics() const noexcept {return executor_.diagnostics();}
    auto step_state(std::size_t i) const noexcept {return executor_.step_state(i);}
    coo::StallDetail missing(const coo::CommandSpec&) const noexcept;
private:
    bool publish(const coo::Command&) noexcept override;
    void cancel(const coo::Command&) noexcept override {}
    void update_module(std::uint32_t,const coo::MissionInput&,Frame&) noexcept override;
    std::uint32_t observations(std::uint64_t,const Frame& f) noexcept override {return f.checked?1U:0U;}
    bool request(coo::Asset,bool) noexcept;
    bool cover(bool) noexcept;
    void update_cover() noexcept;
    void update_boss_platform() noexcept;
    bool platform_position(float,bool=false) noexcept;
    bool observed(const coo::CommandSpec&) const noexcept;
    bool entered(coo::Asset) const noexcept;
    bool revise(coo::Asset) noexcept;
    struct SceneOwner {coo::Token token{};bool valid() const noexcept {return token.run && token.incarnation;}};
    const coo::script::Views* views_{};std::uint64_t run_{},now_{};bool started_{},landed_{};
    coo::LifecycleService lifecycle_{};coo::NativeActivityClock clock_{};coo::ObjectiveService objectives_{};
    coo::ObjectService<kObjectBindings.size()> objects_{};
    // Headroom only. Squads request one actor per authored category, so this is never the
    // binding constraint; deep_storage uses the same value.
    coo::PopulationService<EnemyReceipt,std::size(kSpawns),16> population_{};
    std::array<coo::TaskCosts,std::size(kSpawns)> costs_{};
    std::array<coo::DestructibleService<LensReceipt>,std::size(kLenses)> lenses_{};
    coo::SceneService<SceneCommand,SceneOwner,std::size(kScenes)> scenes_{};
    coo::CombatantState boss_{};EnemyReceipt bossEnemy_{};float bossFraction_{1.F};bool hasBossHealth_{};
    coo::DialogueService<std::size(kDialogueRows)> dialogue_{};
    std::bitset<std::size(kDialogueRows)> submitted_{};std::array<std::uint64_t,std::size(kDialogueRows)> voiceEnd_{};
    std::bitset<std::size(kVolumes)> seen_{};std::bitset<64> regions_{};
    // Diagnostics only. strike_bond had no volume telemetry at all, so a trigger that never fires
    // was indistinguishable from a player who never walked into it.
    Point lastPoint_{};bool hasPoint_{};
    std::uint64_t nextPlatform_{},nextCover_{};std::uint32_t coverSeed_{};std::uint8_t coverGroup_{UINT8_MAX};
    bool platformForward_{true};
    coo::MissionRuntime composition_{};coo::Executor executor_{};Frame frame_{};
};
}
