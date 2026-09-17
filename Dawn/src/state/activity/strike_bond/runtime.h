#pragma once
#include "../coo/population_readiness_request.h"
#include "controller.h"
#include "../lifecycle_generation.h"
namespace dawn::state::activity::strike_bond {
[[nodiscard]] coo::ReadinessRequest<EnemyReceipt> readiness_request(std::uint64_t now) noexcept;
bool claim_ending_animation(coo::Generation,EndingActor) noexcept;
void observe_ending_animation(coo::Generation,EndingActor,bool,std::uint64_t) noexcept;
void observe_ending_retirement(coo::Generation) noexcept;
void observe_ending_movie(coo::Generation,std::uint32_t,std::uint32_t,std::uint32_t,bool) noexcept;
omega_ending_transit::Authority ending_transit(ActivityInstanceKey,std::uint64_t,std::uint64_t,bool,omega_ending_transit::Observation) noexcept;
bool ending_membership_due(std::uint64_t) noexcept;
bool prepare(std::uint64_t,bool,bool campaign=false) noexcept;
Frame snapshot(std::uint64_t,std::uint64_t,bool,int) noexcept;
Request request() noexcept;
void observe_ending_scene_cue(const EndingSpeechReceipt&,bool closing) noexcept;
void observe_ending_playback(coo::Generation,EndingActor,std::uint32_t,EndingAnimationPhase) noexcept;
void observe_ending_speech(const EndingSpeechReceipt&,std::uint8_t) noexcept;
bool sagira_delay(const EndingSpeechReceipt&,bool fired) noexcept;
std::uint64_t native_run() noexcept;
bool publication_due(std::uint64_t) noexcept;
void observe_position(float,float,float) noexcept;
void observe_prepared(coo::Generation,coo::Asset) noexcept;
void observe_object(const coo::ObjectReceipt&) noexcept;
LensRequest lens_request(std::size_t) noexcept;
void observe_lens(const LensReceipt&,bool) noexcept;
bool observe_admission(const EnemyReceipt&) noexcept;
bool observe_death(const EnemyReceipt&) noexcept;
EnemyReceipt boss_enemy() noexcept;
BossRequest boss_request() noexcept;
EnemyReceipt guardian_enemy(std::uint32_t,std::uint16_t) noexcept;
void observe_health(const EnemyReceipt&,float) noexcept;
bool observe_boss_platform_motion(const EnemyReceipt&,const coo::ObjectReceipt&,PlatformMotion) noexcept;
bool observe_boss_animation(const EnemyReceipt&,std::uint8_t,BossAnimation) noexcept;
void observe_readiness(const EnemyReceipt&,coo::EnemyReadiness) noexcept;
void observe_costs(std::uint32_t,std::uint16_t,const coo::TaskCosts&) noexcept;
void observe_player_trigger(std::uint64_t,std::uint32_t,std::uint16_t) noexcept;
void observe_generator(std::uint64_t,std::uint32_t,std::uint16_t,std::uint32_t,std::uint32_t) noexcept;
void observe_scene(std::uint64_t,std::uint32_t,std::uint16_t,const middleware::bap::activity_message::scene_sense::Output&) noexcept;
void observe_squad(std::uint64_t,std::uint32_t,std::uint16_t,const middleware::bap::activity_message::squad_sense::Output&) noexcept;
void observe_combatant(std::uint64_t,std::uint32_t,std::uint16_t,const middleware::bap::activity_message::combatant_sense::Output&) noexcept;
void observe_submission(std::uint64_t,std::uint32_t,std::int64_t,std::uint32_t,std::uint8_t,std::uint32_t) noexcept;
coo::CampaignScanRequest scan_request() noexcept;
void observe_scan(coo::Generation,std::uint32_t,std::uint32_t,coo::ScanPlayback,bool) noexcept;
}
