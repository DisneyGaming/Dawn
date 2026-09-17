#pragma once
#include "omega_script_views.h"
#include "native_services.h"
#include "executor.h"
#include "../omega_intro_rules.h"
#include <type_traits>

namespace dawn::state::activity::coo::reveal {
namespace native=omega_presentation;
inline constexpr Asset kCamera{native::kIntroRegistry,native::kIntroDefinition,6,native::kIntroSlot};
inline constexpr Asset kBoss{native::kBossRegistry,native::kBossEntity,0,0};
inline constexpr CommandSpec kEligibility[]{{Operation::observation,kCamera,0,Wait::observed}};
inline constexpr CommandSpec kPrepare[]{
    {Operation::mechanic,kBoss,1,Wait::requested},
    {Operation::observation,kCamera,1,Wait::observed}};
inline constexpr CommandSpec kPlay[]{
    {Operation::cinematic,kCamera,2,Wait::requested},
    {Operation::observation,kCamera,2,Wait::observed}};
inline constexpr CommandSpec kActive[]{
    {Operation::cinematic,kCamera,3,Wait::requested},
    {Operation::observation,kCamera,3,Wait::observed}};
inline constexpr CommandSpec kStop[]{{Operation::cinematic,kCamera,4,Wait::requested}};
inline constexpr Step kSteps[]{
    {"camera and dialogue eligibility",0,kEligibility},
    {"owned boss flight",1,kPrepare},
    {"native camera activation",2,kPlay},
    {"native camera completion",4,kActive},
    {"retire reveal",8,kStop}};
inline constexpr Step kRetrySteps[]{
    {"owned boss flight",0,kPrepare},
    {"native camera activation",1,kPlay},
    {"native camera completion",2,kActive},
    {"retire reveal",4,kStop}};
inline constexpr ReceiptBinding kReceipts[]{{"camera.eligible",0,0},{"boss.flight.ready",1,1},{"camera.active",2,1},{"camera.complete",3,1}};
inline constexpr Definition kDefinition{"Panoptes reveal",Schema::omegaArchive,kSteps, kReceipts};
inline constexpr ReceiptBinding kRetryReceipts[]{{"boss.flight.ready",0,1},{"camera.active",1,1},{"camera.complete",2,1}};
inline constexpr Definition kRetry{"Panoptes reveal retry",Schema::omegaArchive,kRetrySteps, kRetryReceipts};

// A value-only controller: no virtual services survive a Session copy/reset.
// Observations arrive through the presentation owner's existing ordered FIFO.
class Intro final {
public:
    void reset(std::uint64_t run,bool selected=false) noexcept {
        Driver driver(*this);executor_.cancel(driver);
        const auto retained=executor_;
        *this={};executor_=retained;run_=run;selected_=selected;
        nextRevision_=static_cast<std::uint32_t>(run*32U)|1U;legacy_.reset(run);
    }
    void request(std::uint64_t now) noexcept {
        if(!selected_) { legacy_.request(now);return; }
        if(phase_!=native::IntroPhase::dormant) { return; }
        phase_=native::IntroPhase::waiting;requestedAt_=now;now_=now;
        if(!executor_.start(script::graph("reveal", kDefinition),run_)) { finish(native::IntroPhase::failed);return; }
        update();
    }
    [[nodiscard]] native::IntroPhase phase() const noexcept { return selected_?phase_:legacy_.phase(); }
    [[nodiscard]] native::IntroCommand command() const noexcept { return selected_?command_:legacy_.command(); }
    [[nodiscard]] bool busy() const noexcept {
        return phase()==native::IntroPhase::priming || phase()==native::IntroPhase::offered || phase()==native::IntroPhase::playing;
    }
    [[nodiscard]] Diagnostics diagnostics() const noexcept { return executor_.diagnostics(); }
    [[nodiscard]] bool boss_requested() const noexcept { return bossRequested_; }
    void advance(std::uint64_t now) noexcept {
        if(!selected_) { legacy_.advance(now);return; }
        now_=now;
        if(((phase_==native::IntroPhase::waiting || phase_==native::IntroPhase::priming || phase_==native::IntroPhase::offered)
                && now-requestedAt_>=30000)
            || (phase_==native::IntroPhase::playing && now-startedAt_>=30000)) { finish(native::IntroPhase::failed); }
    }
    void observe(std::uint32_t revision,bool active,bool ready,bool mayStart,bool flight,std::uint64_t now) noexcept {
        if(!selected_) { legacy_.observe(revision,active,ready,mayStart,flight,now);return; }
        advance(now);
        // One native receipt can satisfy only the phase it was received in.
        // In particular it cannot create a boss and acknowledge its flight.
        if(phase_==native::IntroPhase::waiting && ready && mayStart) { observe_step("camera.eligible"); }
        else if(phase_==native::IntroPhase::priming && ready && mayStart && flight) { observe_step("boss.flight.ready"); }
        else if(phase_==native::IntroPhase::offered && revision==command_.revision) {
            if(active) { observe_step("camera.active"); }
            else if(now-offeredAt_>=1000) {
                if(attempts_>=3) { finish(native::IntroPhase::failed); }
                else {
                    Driver driver(*this);executor_.cancel(driver);retry_=true;
                    if(!executor_.start(script::graph("reveal_retry", kRetry),run_)) { finish(native::IntroPhase::failed);return; }
                    update();
                }
            }
        } else if(phase_==native::IntroPhase::playing && revision==command_.revision && !active) { observe_step("camera.complete"); }
    }
    void passed() noexcept {
        if(!selected_) { legacy_.passed();return; }
        if(phase_==native::IntroPhase::waiting || phase_==native::IntroPhase::priming || phase_==native::IntroPhase::offered) {
            finish(native::IntroPhase::failed);
        } else if(phase_==native::IntroPhase::dormant) { phase_=native::IntroPhase::complete; }
    }
private:
    class Driver final : public NativeServices<Driver> {
    public:
        explicit Driver(Intro& owner) noexcept : owner_(owner) {}
        [[nodiscard]] ServiceContext context() const noexcept { return {script::graph(owner_.retry_ ? "reveal_retry" : "reveal", owner_.retry_ ? kRetry : kDefinition), owner_.run_, owner_.executor_.diagnostics().incarnation}; }
        bool request(const Command& command) noexcept {
            if(command.schema!=Schema::omegaArchive || command.token.run!=owner_.run_) { return false; }
            const auto& spec=command.spec;
            if(spec.operation==Operation::observation) { return spec.asset==kCamera && spec.wait==Wait::observed; }
            if(spec.wait!=Wait::requested) { return false; }
            if(spec.argument==1 && spec.operation==Operation::mechanic && spec.asset==kBoss) {
                owner_.bossRequested_=true;owner_.phase_=native::IntroPhase::priming;return true;
            }
            if(spec.operation!=Operation::cinematic || spec.asset!=kCamera) { return false; }
            if(spec.argument==2) {
                owner_.command_={owner_.nextRevision_++,true};owner_.phase_=native::IntroPhase::offered;
                owner_.offeredAt_=owner_.now_;++owner_.attempts_;return true;
            }
            if(spec.argument==3) { owner_.phase_=native::IntroPhase::playing;owner_.startedAt_=owner_.now_;return true; }
            if(spec.argument==4) {
                owner_.command_={owner_.nextRevision_++,false};owner_.phase_=native::IntroPhase::complete;return true;
            }
            return false;
        }
        void retire(const Command&) noexcept {}
    private:
        Intro& owner_;
    };
    void update() noexcept {
        Driver driver(*this);executor_.update(driver);executor_.update(driver);
        if(executor_.diagnostics().phase==Phase::failed) { finish(native::IntroPhase::failed); }
    }
    void observe_step(std::string_view receipt) noexcept {
        static_cast<void>(executor_.enqueue({executor_.token(receipt),Milestone::observed}));update();
    }
    void finish(native::IntroPhase result) noexcept {
        Driver driver(*this);executor_.cancel(driver);
        command_={nextRevision_++,false};phase_=result;
    }
    Executor executor_{};
    native::Intro legacy_{};
    native::IntroPhase phase_{};
    native::IntroCommand command_{};
    std::uint64_t run_{},now_{},requestedAt_{},offeredAt_{},startedAt_{};
    std::uint32_t nextRevision_{1};
    std::uint8_t attempts_{};
    bool selected_{},retry_{},bossRequested_{};
};
static_assert(std::is_trivially_copyable_v<Intro>);
} // namespace dawn::state::activity::coo::reveal
