#pragma once
#include "frame.h"
#include "scan_playback.h"
#include "../coo/native_activity_clock.h"
#include "../coo/event_timeline.h"
#include "../coo/stall_diagnostics.h"
namespace sunrise::state::activity::hijacked {
bool contains(const Volume&,Point) noexcept;
class Controller final : private coo::Services,private coo::MissionPorts<Frame> {
public:
    void reset() noexcept;
    bool select(const coo::script::Views&,std::uint64_t) noexcept;
    void position(std::uint64_t,Point,std::uint64_t eventTime=0) noexcept;
    bool prepared(coo::Generation,coo::Asset) noexcept;
    bool object(const coo::ObjectReceipt&) noexcept;
    bool device(coo::Generation,coo::Asset,std::int16_t,float) noexcept;
    bool admitted(const EnemyReceipt&) noexcept;
    bool died(const EnemyReceipt&) noexcept;
    bool suspend_exterior(coo::Generation,std::uint16_t,std::span<const EnemyReceipt>) noexcept;
    bool resume_exterior(coo::Generation,std::uint16_t) noexcept;
    bool readiness(const EnemyReceipt& r,coo::EnemyReadiness v) noexcept { return enemy_active(r) && population_.observe(r,v); }
    template<class Visit> void pending_enemies(Visit v) const noexcept { population_.pending([&](const EnemyReceipt& r) {if(enemy_active(r)) {v(r);}}); }
    template<class Visit> void living_enemies(Visit v) const noexcept { population_.living([&](const EnemyReceipt& r) {if(enemy_active(r)) {v(r);}}); }
    // Diagnostic-only view: retain authentic receipts after logical retirement.
    template<class Visit> void retirement_enemies(Visit v) const noexcept {
        population_.living([&](const EnemyReceipt& r) {
            const auto* a=find(r.registry,1,r.source);
            if(a && frame_.native[asset_index(a->asset)].retired) {v(r);}
        });
    }
    BossRequest boss_request() const noexcept;
    bool boss_position(const EnemyReceipt&,std::uint8_t,std::uint32_t) noexcept;
    bool health(const EnemyReceipt&,float) noexcept;
    bool health_event(const EnemyReceipt&,float,std::uint64_t) noexcept;
    bool bind_plate(const PlateReceipt&) noexcept;
    bool plate_pose(const PlateReceipt&,server::runtime::activity::mission_device_pose::Sample) noexcept;
    bool plate(const PlateReceipt&,std::uint32_t,float,bool) noexcept;
    bool contested(const PlateReceipt&,bool) noexcept;
    bool contested_positions(const PlateReceipt&,std::span<const EnemyPosition>,bool complete) noexcept;
    bool bind_scan(const ScanReceipt&) noexcept;
    bool scan(const ScanReceipt&,bool,bool) noexcept;
    bool scan_playback(const ScanReceipt&,ScanPlayback,bool participant) noexcept;
    bool submitted(std::uint64_t,std::uint32_t,std::uint8_t,std::uint32_t,std::uint64_t) noexcept;
    Frame update(std::uint64_t,std::uint64_t,bool) noexcept;
    const Frame& frame() const noexcept { return frame_; }
    bool cleanup_entered() const noexcept { return cleanupEntered_; }
    coo::Generation owner() const noexcept { return lifecycle_.owner(); }
    PlateRequest plate_request(std::size_t i) const noexcept { return i<std::size(kPlates)?PlateRequest{owner(),plates_[i],frame_.plates[i],frame_.enabled && (frame_.plates[i].armed || frame_.native[asset_index(kPlates[i].source)].active) && (!frame_.finished || frame_.plates[i].charged),frame_.plateCaptures[i]}:PlateRequest{}; }
    ScanRequest scan_request(std::size_t i) const noexcept { return i<std::size(kScans)?ScanRequest{owner(),scans_[i],frame_.enabled && frame_.scanArmed[i] && !frame_.finished,frame_.scanStarted[i],frame_.scanComplete[i]}:ScanRequest{}; }
    const coo::script::GraphView* graph() const noexcept { return views_ && frame_.section<views_->phases.size()?views_->phases[frame_.section]:nullptr; }
    coo::Diagnostics diagnostics() const noexcept { return executor_.diagnostics(); }
    auto step_state(std::size_t i) const noexcept { return executor_.step_state(i); }
    const auto& seen() const noexcept { return seen_; }
    coo::StallDetail missing(const coo::CommandSpec&) const noexcept;
private:
    bool publish(const coo::Command&) noexcept override;
    void cancel(const coo::Command&) noexcept override {}
    void update_module(std::uint32_t,const coo::MissionInput&,Frame&) noexcept override;
    std::uint32_t observations(std::uint64_t,const Frame& f) noexcept override { return f.checked?1U:0U; }
    bool observed(const coo::CommandSpec&) const noexcept;
    bool request(coo::Asset,bool) noexcept;
    bool enemy_active(const EnemyReceipt&) const noexcept;
    bool entered(coo::Asset,bool=false) const noexcept;
    const coo::script::Views* views_{};std::uint64_t run_{},now_{};bool cleanupEntered_{},started_{},arrived_{},phaseFinished_{};
    coo::LifecycleService lifecycle_{};coo::NativeActivityClock clock_{};coo::ObjectiveService objectives_{};
    coo::DialogueService<std::size(kDialogue)> dialogue_{};
    coo::ObjectService<kObjectBindings.size()> objects_{};
    coo::PopulationService<EnemyReceipt,std::size(kSpawns),16> population_{};
    std::array<EnemyReceipt,kExteriorPopulation> suspendedEnemies_{};
    std::array<PlateReceipt,std::size(kPlates)> plates_{};std::array<std::uint32_t,std::size(kPlates)> charging_{};std::array<ScanReceipt,std::size(kScans)> scans_{};
    std::bitset<std::size(kDialogue)> submitted_{};std::array<std::uint64_t,std::size(kDialogue)> voiceEnd_{};
    std::bitset<std::size(kVolumes)> seen_{},inside_{};
    coo::EventTimeline<coo::Generation,std::size(kVolumes)> arrivals_{};
    coo::MissionRuntime composition_{};coo::Executor executor_{};Frame frame_{};
};
}
