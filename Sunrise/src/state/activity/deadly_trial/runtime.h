#pragma once
#include "frame.h"
#include "../coo/population_service.h"
namespace sunrise::state::activity::deadly_trial {
struct Presentation { std::uint64_t run{};Frame frame{}; };
Presentation presentation() noexcept;
bool prepare(std::uint64_t,bool) noexcept;
Frame snapshot(std::uint64_t,std::uint64_t,bool) noexcept;
std::uint64_t native_run() noexcept;
bool publication_due(std::uint64_t) noexcept;
Request request() noexcept;
bool observe_admission(const EnemyReceipt&) noexcept;
bool observe_death(const EnemyReceipt&) noexcept;
void observe_readiness(const EnemyReceipt&,coo::EnemyReadiness) noexcept;
void observe_position(float,float,float) noexcept;
void observe_submission(std::uint64_t,std::uint32_t,std::int64_t,std::uint32_t,std::uint8_t,std::uint32_t) noexcept;
void observe_binding(const InteractionBinding&) noexcept;
void observe_interaction(const InteractionBinding&,std::int32_t,std::int32_t,std::int32_t,bool) noexcept;
bool observe_scene_binding(const SceneReceipt&) noexcept;
void observe_scene(const SceneReceipt&,bool) noexcept;
bool observe_scene_audio(const SceneReceipt&,float elapsed) noexcept;
}
