#pragma once
#include "profile.h"
#include "frame.h"
#include "ending_receipts.h"
#include "traversal_catalog.h"
#include "../coo/population_service.h"
#include "service_bindings.h"
#include <bitset>
#include <cmath>
namespace sunrise::state::activity::gateway {
[[nodiscard]] inline bool contains(const Volume& volume, Point point) noexcept {
    if(!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)
        || point.x<volume.min.x || point.x>volume.max.x || point.y<volume.min.y
        || point.y>volume.max.y || point.z<volume.min.z || point.z>volume.max.z
        || volume.vertices.size()<3) { return false; }
    bool inside{};
    for(std::size_t i=0,j=volume.vertices.size()-1;i<volume.vertices.size();j=i++) {
        const auto a=volume.vertices[j], b=volume.vertices[i];
        const double dx=double(b.x)-a.x,dy=double(b.y)-a.y;
        const double px=double(point.x)-a.x,py=double(point.y)-a.y;
        if(dx==0 && dy==0) { continue; }
        const double cross=dx*py-dy*px;
        if(std::abs(cross)<0.00001 && px*dx+py*dy>=0 && px*dx+py*dy<=dx*dx+dy*dy) { return true; }
        if((a.y>point.y)!=(b.y>point.y) && point.x<dx*py/dy+a.x) { inside=!inside; }
    }
    return inside;
}
class Controller final : private coo::Services, private coo::MissionPorts<Frame> {
public:
    void reset() noexcept;
    [[nodiscard]] bool select(const coo::script::Views& views,std::uint64_t run) noexcept;
    void position(std::uint64_t run,Point point) noexcept;
    [[nodiscard]] bool submitted(std::uint64_t run,std::uint32_t bank,std::uint8_t row,
        std::uint32_t generation,std::uint64_t now) noexcept;
    [[nodiscard]] bool admitted(const EnemyReceipt& receipt) noexcept;
    bool readiness(const EnemyReceipt& receipt,coo::EnemyReadiness value) noexcept { return population_.observe(receipt,value); }
    void capacity(coo::PopulationCapacity value) noexcept { population_.capacity(value); }
    coo::PopulationCapacity capacity() const noexcept { return population_.capacity(); }
    template<class Visit> void background_diagnostics(Visit visit) const noexcept {
        for(std::size_t i=0;i<kSpawns.size();++i) {
            if(!population_.enabled(i)) { continue; }
            auto detail=population_.missing(i,kSpawns[i].count,false);
            detail.asset={kSpawns[i].registry,kSpawns[i].definition,1,kSpawns[i].source};visit(i,detail);
        }
        for(std::size_t i=0;i<3;++i) {
            const auto phase=objects_.state(i).phase;using coo::Missing;
            const auto missing=phase==coo::ObjectPhase::prepare?Missing::preparation:phase==coo::ObjectPhase::create?Missing::object
                :phase==coo::ObjectPhase::bind?Missing::controller:phase==coo::ObjectPhase::apply?Missing::device:Missing::none;
            visit(kSpawns.size()+i,coo::StallDetail{missing,kEndingObjects[i].source});
        }
        using coo::Missing;coo::StallDetail sceneWait{};
        if(frame_.sceneGeneration && !frame_.sceneStarted) { sceneWait.missing=Missing::sceneBinding; }
        else if(frame_.vanceEntered && !frame_.vanceTurned) {
            sceneWait.missing=Missing::sceneEvent;sceneWait.expected=coo::signal_bit(coo::SceneSignal::animationReady);
        }
        sceneWait.asset={0xBA0B27A0U,0x80F46DDDU,1,4};visit(kSpawns.size()+3,sceneWait);
    }
    template<class Visit> void pending_enemies(Visit visit) const noexcept { population_.pending(visit); }
    bool object(std::size_t index,const coo::ObjectReceipt& receipt,bool applied,float position,std::int16_t revision) noexcept;
    [[nodiscard]] const Frame& frame() const noexcept { return frame_; }
    coo::StallDetail missing(const coo::CommandSpec&) const noexcept;
    [[nodiscard]] const coo::script::GraphView* graph() const noexcept { return views_ && frame_.section<views_->phases.size()?views_->phases[frame_.section]:nullptr; }
    [[nodiscard]] auto step_state(std::size_t i) const noexcept { return executor_.step_state(i); }
    [[nodiscard]] bool died(const EnemyReceipt& receipt) noexcept;
    [[nodiscard]] bool prepared(std::uint64_t run,std::uint32_t generation,std::uint8_t index) noexcept;
    [[nodiscard]] bool module(const ModuleReceipt&,bool dead) noexcept;
    [[nodiscard]] bool scene(const SceneReceipt&,bool completed) noexcept;
    [[nodiscard]] bool vance(const SceneReceipt&,VanceMilestone,std::uint64_t now) noexcept;
    [[nodiscard]] EndingRequest ending_request() const noexcept { return {frame_.enabled,frame_.moduleVulnerable,run_,publicationGeneration_+1U,frame_.sceneGeneration,destructible_.owner(),frame_.moduleDestroyed}; }
    [[nodiscard]] Frame update(std::uint64_t run,std::uint64_t now,bool ready) noexcept;
    [[nodiscard]] const auto& seen() const noexcept { return seen_; }
    [[nodiscard]] coo::Diagnostics diagnostics() const noexcept { return executor_.diagnostics(); }
    [[nodiscard]] std::uint32_t timeouts() const noexcept { return dialogue_.timed_out(); }
private:
    bool publish(const coo::Command&) noexcept override;
    void cancel(const coo::Command&) noexcept override {}
    void update_module(std::uint32_t,const coo::MissionInput&,Frame&) noexcept override;
    std::uint32_t observations(std::uint64_t,const Frame& frame) noexcept override { return frame.checked?1U:0U; }
    bool entered(coo::Asset asset) const noexcept;
    bool observed(const coo::CommandSpec&) const noexcept;
    bool cleared(std::uint32_t cohort) const noexcept;
    bool ready(std::uint32_t cohort) const noexcept;
    void enable(std::uint32_t cohort) noexcept;
    const coo::script::Views* views_{};
    std::uint64_t run_{},now_{};
    std::uint32_t publicationGeneration_{}; // Retained across resets, including a reused run.
    bool started_{},landingSeen_{},vanceRequested_{},greetingRequested_{};
    std::bitset<32> deathCohorts_{};
    std::array<std::uint64_t,16> voiceEnds_{};
    coo::LifecycleService lifecycle_{};
    coo::ObjectService<3> objects_{};
    coo::DestructibleService<ModuleReceipt> destructible_{};
    coo::SceneOrchestration<SceneReceipt> scene_{};
    coo::EventTimeline<coo::Generation,16> dialogueClock_{};
    coo::ObjectiveService objectives_{};
    void project_services() noexcept;
    std::bitset<16> dialogueSubmitted_{};
    std::uint8_t prepared_{};
    coo::PopulationService<EnemyReceipt,kSpawns.size(),2> population_;
    coo::MissionRuntime composition_;
    coo::Executor executor_;
    coo::DialogueService<16> dialogue_;
    Frame frame_;
    std::bitset<std::size(kVolumes)> seen_{};
};
} // namespace sunrise::state::activity::gateway
