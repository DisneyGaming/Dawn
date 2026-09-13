#pragma once
#include "frame.h"
#include "ending_presentation.h"
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
    bool claim_ending_animation(coo::Generation token,EndingActor actor) noexcept {
        const bool accepted=frame_.campaign && frame_.enabled && frame_.endingFlow.claim(token,actor);
        if(accepted) ++frame_.revision;return accepted;
    }
    bool ending_animation(coo::Generation token,EndingActor actor,bool active,std::uint64_t now) noexcept {
        const bool accepted=frame_.campaign && frame_.endingFlow.animation(token,actor,active,now);
        if(accepted) ++frame_.revision;return accepted;
    }
    bool ending_playback(coo::Generation token,EndingActor actor,std::uint32_t biped,EndingAnimationPhase phase) noexcept {
        const bool accepted=frame_.campaign && frame_.enabled && frame_.endingFlow.playback(token,actor,biped,phase);
        if(accepted) ++frame_.revision;return accepted;
    }
    bool ending_retirement(coo::Generation token) noexcept {
        const bool accepted=frame_.endingFlow.cleanup(token);if(accepted) ++frame_.revision;return accepted;
    }
    void ending_arrival(coo::Generation token) noexcept {
        if(token==owner() && frame_.endingFlow.retired && !frame_.endingFlow.arrived) {
            frame_.endingFlow.arrived=true;++frame_.revision;
        }
    }
    bool ending_movie(coo::Generation token,std::uint32_t self,std::uint32_t resource,std::uint32_t revision,bool active) noexcept {
        const bool accepted=frame_.endingFlow.movie(token,self,resource,revision,active);
        if(accepted) ++frame_.revision;return accepted;
    }
    coo::CampaignScanRequest scan_request() const noexcept {
        return frame_.campaign && frame_.enabled && !frame_.finished
            ?coo::CampaignScanRequest{lifecycle_.owner(),{0xC80A735BU,0x80F4748CU,65,0},frame_.spawnGeneration+1U,frame_.scan}
            :coo::CampaignScanRequest{};
    }
    bool scan_observation(coo::Generation owner,std::uint32_t handle,std::uint32_t serial,coo::ScanPlayback playback,bool participant) noexcept {
        const auto request=scan_request();
        if(!request.enabled() || request.owner!=owner || !frame_.scan.observe(request.generation,handle,serial,playback,participant)) return false;
        ++frame_.revision;return true;
    }
    bool ending_speech(const EndingSpeechReceipt&,std::uint8_t) noexcept;
    bool sagira_delay(const EndingSpeechReceipt&,bool fired) noexcept;
    bool ending_scene_cue(const EndingSpeechReceipt&,bool closing) noexcept;
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
    bool boss_platform_motion(const EnemyReceipt&,const coo::ObjectReceipt&,PlatformMotion) noexcept;
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
    bool platform_travel(float) noexcept;
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
    bool platformForward_{true};PlatformTravel platformTravel_{};
    EndingPresentation endingPresentation_{};
    coo::MissionRuntime composition_{};coo::Executor executor_{};Frame frame_{};
};
}
