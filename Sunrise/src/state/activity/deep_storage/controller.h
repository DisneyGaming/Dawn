#pragma once
#include "frame.h"
#include "../coo/native_activity_clock.h"
#include "../coo/stall_diagnostics.h"
namespace sunrise::state::activity::deep_storage {
bool contains(const Volume&,Point) noexcept;
class Controller final : private coo::Services,private coo::MissionPorts<Frame> {
public:
    void reset() noexcept;
    bool select(const coo::script::Views&,std::uint64_t) noexcept;
    void position(std::uint64_t,Point) noexcept;
    bool prepared(coo::Generation,coo::Asset) noexcept;
    bool object(const coo::ObjectReceipt&) noexcept;
    bool device(coo::Generation,coo::Asset,std::int16_t,float) noexcept;
    bool admitted(const EnemyReceipt&) noexcept;
    bool died(const EnemyReceipt&) noexcept;
    bool readiness(const EnemyReceipt& r,coo::EnemyReadiness v) noexcept { return population_.observe(r,v); }
    template<class Visit> void pending_enemies(Visit v) const noexcept { population_.pending(v); }
    template<class Visit> void living_enemies(Visit v) const noexcept { population_.living(v); }
    bool lens(const LensReceipt&,bool) noexcept;
    LensRequest lens_request() const noexcept {return {owner(),lens_,frame_.native[asset_index(kLens)].generation,frame_.enabled && frame_.native[asset_index(kLens)].active,frame_.lensExposed,frame_.lensDestroyed};}
    bool bind_plate(const PlateReceipt&) noexcept;
    bool plate(const PlateReceipt&,std::uint32_t,float,bool) noexcept;
    bool contested(const PlateReceipt&,bool) noexcept;
    bool bind_scan(const ScanReceipt&) noexcept;
    bool scan(const ScanReceipt&,bool,bool) noexcept;
    bool submitted(std::uint64_t,std::uint32_t,std::uint8_t,std::uint32_t,std::uint64_t) noexcept;
    Frame update(std::uint64_t,std::uint64_t,bool) noexcept;
    const Frame& frame() const noexcept { return frame_; }
    coo::Generation owner() const noexcept { return lifecycle_.owner(); }
    PlateRequest plate_request(std::size_t i) const noexcept { return i<3?PlateRequest{owner(),plates_[i],frame_.plates[i],frame_.enabled && (frame_.plates[i].armed || (i>0 && frame_.native[asset_index(kPlates[i].source)].active)) && (!frame_.finished || frame_.plates[i].charged)}:PlateRequest{}; }
    ScanRequest scan_request(std::size_t i) const noexcept { return i<2?ScanRequest{owner(),scans_[i],frame_.enabled && frame_.scanArmed[i] && !frame_.finished,frame_.scanStarted[i],frame_.scanComplete[i]}:ScanRequest{}; }
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
    bool entered(coo::Asset,bool=false) const noexcept;
    const coo::script::Views* views_{};std::uint64_t run_{},now_{};bool started_{},arrived_{},phaseFinished_{};
    coo::LifecycleService lifecycle_{};coo::NativeActivityClock clock_{};coo::ObjectiveService objectives_{};
    coo::DialogueService<std::size(kDialogue)> dialogue_{};
    coo::ObjectService<kObjectBindings.size()> objects_{};
    coo::PopulationService<EnemyReceipt,std::size(kSpawns),16> population_{};
    LensReceipt lens_{};std::array<PlateReceipt,3> plates_{};std::array<std::uint32_t,3> charging_{};std::array<ScanReceipt,2> scans_{};
    std::bitset<std::size(kDialogue)> submitted_{};std::array<std::uint64_t,std::size(kDialogue)> voiceEnd_{};
    std::bitset<std::size(kVolumes)> seen_{},inside_{};
    coo::MissionRuntime composition_{};coo::Executor executor_{};Frame frame_{};
};
}
