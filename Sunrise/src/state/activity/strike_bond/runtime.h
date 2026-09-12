#pragma once
#include "controller.h"
namespace sunrise::state::activity::strike_bond {
bool prepare(std::uint64_t,bool) noexcept;
Frame snapshot(std::uint64_t,std::uint64_t,bool,int) noexcept;
Request request() noexcept;
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
bool observe_boss_motion(const EnemyReceipt&,const coo::ObjectReceipt&,float) noexcept;
bool observe_boss_animation(const EnemyReceipt&,std::uint8_t,BossAnimation) noexcept;
void observe_readiness(const EnemyReceipt&,coo::EnemyReadiness) noexcept;
void observe_costs(std::uint32_t,std::uint16_t,const coo::TaskCosts&) noexcept;
void observe_player_trigger(std::uint64_t,std::uint32_t,std::uint16_t) noexcept;
void observe_generator(std::uint64_t,std::uint32_t,std::uint16_t,std::uint32_t,std::uint32_t) noexcept;
void observe_scene(std::uint64_t,std::uint32_t,std::uint16_t,const middleware::bap::activity_message::scene_sense::Output&) noexcept;
void observe_squad(std::uint64_t,std::uint32_t,std::uint16_t,const middleware::bap::activity_message::squad_sense::Output&) noexcept;
void observe_combatant(std::uint64_t,std::uint32_t,std::uint16_t,const middleware::bap::activity_message::combatant_sense::Output&) noexcept;
void observe_submission(std::uint64_t,std::uint32_t,std::int64_t,std::uint32_t,std::uint8_t,std::uint32_t) noexcept;
}
