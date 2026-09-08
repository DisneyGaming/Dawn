#pragma once
#include "profile.h"
#include "frame.h"
#include "../coo/population_service.h"
#include "../coo/mission_runtime.h"
#include <bitset>
#include <cmath>
namespace sunrise::state::activity::deadly_trial {
bool contains(const Volume&,Point) noexcept;
class Controller final : private coo::Services,private coo::MissionPorts<Frame> {
public:
    void reset() noexcept;
    bool select(const coo::script::Views&,std::uint64_t) noexcept;
    void position(std::uint64_t,Point) noexcept;
    bool mounted(const PikeMount&) noexcept;
    bool submitted(std::uint64_t,std::uint32_t,std::uint8_t,std::uint32_t,std::uint64_t) noexcept;
    bool admitted(const EnemyReceipt&) noexcept;
    bool died(const EnemyReceipt&) noexcept;
    bool readiness(const EnemyReceipt& r,coo::EnemyReadiness v) noexcept { return population_.observe(r,v); }
    template<class Visit> void pending_enemies(Visit v) const noexcept { population_.pending(v); }
    bool bind(const InteractionBinding&) noexcept;
    bool interact(const InteractionBinding&,std::int32_t requested,std::int32_t before,std::int32_t after,bool active) noexcept;
    bool bind_scene(const SceneReceipt&) noexcept;
    bool scene(const SceneReceipt&,bool completed) noexcept;
    bool scene_audio(const SceneReceipt&,float elapsed,std::uint64_t now) noexcept;
    Request request() const noexcept { return {{run_,frame_.spawnGeneration+1U},frame_.sceneGeneration,frame_.enabled && frame_.reviveEnabled && !frame_.finished,binding_,frame_.interacted}; }
    Frame update(std::uint64_t,std::uint64_t,bool) noexcept;
    const Frame& frame() const noexcept { return frame_; }
    coo::Diagnostics diagnostics() const noexcept { return executor_.diagnostics(); }
    coo::StallDetail missing(const coo::CommandSpec&) const noexcept;
    const coo::script::GraphView* graph() const noexcept { return views_ && phase_<views_->phases.size()?views_->phases[phase_]:nullptr; }
    auto step_state(std::size_t i) const noexcept { return executor_.step_state(i); }
    const auto& seen() const noexcept { return seen_; }
private:
    bool publish(const coo::Command&) noexcept override;
    void cancel(const coo::Command&) noexcept override {}
    void update_module(std::uint32_t,const coo::MissionInput&,Frame&) noexcept override;
    std::uint32_t observations(std::uint64_t,const Frame& f) noexcept override { return f.checked?1U:0U; }
    void pump() noexcept;
    coo::StallDetail raw_missing(const coo::CommandSpec&) const noexcept;
    bool entered(coo::Asset) const noexcept;
    bool cleared(std::uint32_t) const noexcept;
    bool ready(std::uint32_t) const noexcept;
    void enable(std::uint32_t) noexcept;
    const coo::script::Views* views_{};std::uint64_t run_{},now_{};bool started_{},observationsStarted_{},pikeMounted_{};std::size_t phase_{};
    coo::LifecycleService lifecycle_{};coo::ObjectiveService objectives_{};
    coo::MissionRuntime composition_{};coo::Executor executor_{};
    coo::PopulationService<EnemyReceipt,kSpawns.size(),2> population_{};
    coo::DialogueService<11> dialogue_{};std::bitset<11> submitted_{};
    std::array<std::uint64_t,11> voiceEnds_{};
    std::bitset<std::size(kVolumes)> seen_{};
    InteractionBinding binding_{};SceneReceipt scene_{};Frame frame_{};
    bool sceneAudioStarted_{};std::uint64_t sceneAudioEnds_{};
};
}
