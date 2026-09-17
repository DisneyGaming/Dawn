#pragma once
#include "omega_script_views.h"
#include "receipt_queue.h"
#include "native_services.h"
#include "omega_ending_definition.h"
#include <algorithm>
#include <type_traits>

namespace dawn::state::activity::coo::ending {
enum class Kind : std::uint8_t { retirement,arrival,camera,skip,handoff };
struct Receipt final {
    native::Token owner{};
    std::uint64_t incarnation{},now{};
    Kind kind{};
    std::uint32_t revision{};
    bool active{},ready{},arrivedAtIntake{};
};

// The runtime serializes this value under the existing ending lock. Hooks
// enqueue copied facts; publication and the game-frame owner drain them.
// Acknowledgement/claim APIs keep their immediate admission contracts.
class Controller final {
public:
    static constexpr std::size_t kCapacity=128;
    void reset() noexcept {
        Driver driver(*this);executor_.cancel(driver);
        const auto retained=executor_;*this={};executor_=retained;
    }
    [[nodiscard]] bool request(native::Token token,std::uint64_t now,bool selected=false) noexcept {
        if(!token.valid() || token.origin!=native::Origin::encounter) { return false; }
        if(phase()!=native::Phase::dormant) { return this->token()==token; }
        selected_=selected;
        if(!selected_) { return legacy_.request(token,now); }
        token_=token;phase_=native::Phase::dialogue;requestedAt_=now;now_=now;
        nextRevision_=static_cast<std::uint32_t>((token.run*32U)^(token.epoch*2U))|1U;
        if(!executor_.start(script::graph("ending", kDefinition),token.run)) { fail();return false; }
        drive();return !authority().failed;
    }
    // Preview remains the accepted explicitly requested development path.
    [[nodiscard]] bool request_preview(std::uint64_t run,std::uint64_t now) noexcept {
        if(phase()!=native::Phase::dormant) { return false; }
        selected_=false;return legacy_.request_preview(run,now);
    }
    [[nodiscard]] bool selected() const noexcept { return selected_; }
    [[nodiscard]] native::Phase phase() const noexcept { return selected_?phase_:legacy_.phase(); }
    [[nodiscard]] native::Token token() const noexcept { return selected_?token_:legacy_.token(); }
    [[nodiscard]] native::Handoff handoff() const noexcept { return selected_?handoff_:legacy_.handoff(); }
    [[nodiscard]] native::Authority authority() const noexcept {
        if(!selected_) { return legacy_.authority(); }
        return {token_,revision_,stateRequested_,play_,started_,phase_==native::Phase::complete,
            phase_==native::Phase::failed,arrived_,retireRequested_};
    }
    [[nodiscard]] Diagnostics diagnostics() const noexcept {
        auto result=executor_.diagnostics();result.queued=receipts_.size();
        if(phase_==native::Phase::failed) {
            result.phase=coo::Phase::failed;
            if(result.failure==Failure::none) { result.failure=receipts_.overflowed()?Failure::queueOverflow:Failure::native; }
        }
        return result;
    }
    [[nodiscard]] std::string_view waiting() const noexcept {
        const auto& definition=script::graph(retry_?"ending_retry":"ending", retry_?kRetry:kDefinition);
        const auto active=diagnostics().active;
        for(std::size_t i=0;i<definition.steps.size();++i) { if((active&(1U<<i))!=0) { return definition.steps[i].name; } }
        return diagnostics().phase==coo::Phase::complete?"complete":"none";
    }
    [[nodiscard]] native::Token retirement_request() const noexcept {
        return selected_?(phase_==native::Phase::retiring?token_:native::Token{}):legacy_.retirement_request();
    }
    [[nodiscard]] bool observe_retirement(native::Token token,std::uint64_t now) noexcept {
        if(!selected_) { return legacy_.observe_retirement(token,now); }
        if(!matches(token) || !retireRequested_ || phase_==native::Phase::failed) { return false; }
        if(retired_ || retirementPending_) { return true; }
        if(phase_!=native::Phase::retiring || !enqueue({token,incarnation(),now,Kind::retirement})) { return false; }
        retirementPending_=true;return true;
    }
    [[nodiscard]] bool observe_arrival(native::Token token,std::int32_t region,std::uint64_t now=0) noexcept {
        if(!selected_) { return legacy_.observe_arrival(token,region); }
        if(!matches(token) || !stateRequested_ || region!=native::kSlice || phase_==native::Phase::failed) { return false; }
        if(arrived_ || arrivalPending_) { return true; }
        if(!enqueue({token,incarnation(),now,Kind::arrival})) { return false; }
        arrivalPending_=true;return true;
    }
    void observe(native::Token token,std::uint32_t revision,bool active,bool ready,std::uint64_t now) noexcept {
        if(!selected_) { legacy_.observe(token,revision,active,ready,now);return; }
        if(!matches(token) || (phase_!=native::Phase::preparing && phase_!=native::Phase::offered && phase_!=native::Phase::playing)) { return; }
        if(phase_!=native::Phase::preparing && revision!=revision_) { return; }
        static_cast<void>(enqueue({token,incarnation(),now,Kind::camera,revision,active,ready,arrived_||arrivalPending_}));
    }
    [[nodiscard]] bool skip(std::uint64_t now=0) noexcept {
        if(!selected_) { return legacy_.skip(); }
        if(phase_!=native::Phase::playing || !play_ || skipPending_) { return false; }
        if(!enqueue({token_,incarnation(),now,Kind::skip})) { return false; }
        skipPending_=true;return true;
    }
    [[nodiscard]] native::Token handoff_request() const noexcept {
        if(!selected_) { return legacy_.handoff_request(); }
        return phase_==native::Phase::complete && handoff_==native::Handoff::pending?token_:native::Token{};
    }
    [[nodiscard]] bool claim_handoff(native::Token token) noexcept {
        if(!selected_) { return legacy_.claim_handoff(token); }
        if(!token.valid() || token!=handoff_request()) { return false; }
        handoff_=native::Handoff::claimed;return true;
    }
    [[nodiscard]] bool note_handoff_result(native::Token token,bool queued,std::uint64_t now=0) noexcept {
        if(!selected_) { return legacy_.note_handoff_result(token,queued); }
        if(!matches(token) || handoff_!=native::Handoff::claimed || handoffPending_) { return false; }
        if(!enqueue({token,incarnation(),now,Kind::handoff,0,queued})) { return false; }
        handoffPending_=true;return true;
    }
    void advance(std::uint64_t now,bool finalDialogueFinished) noexcept {
        if(!selected_) { legacy_.advance(now,finalDialogueFinished);return; }
        if(receipts_.overflowed()) { fail();return; }
        Receipt receipt{};
        while(phase_!=native::Phase::failed && receipts_.pop(receipt)) {
            if(!matches(receipt.owner) || receipt.incarnation!=incarnation()) { continue; }
            now_=(std::max)(now_,receipt.now);apply(receipt);
        }
        now_=(std::max)(now_,now);
        if(phase_==native::Phase::dialogue && finalDialogueFinished) { observed("dialogue.complete"); }
        if(((phase_==native::Phase::dialogue || phase_==native::Phase::retiring || phase_==native::Phase::preparing || phase_==native::Phase::offered)
                && now_>=requestedAt_ && now_-requestedAt_>=120000U)
            || (phase_==native::Phase::playing && now_>=startedAt_ && now_-startedAt_>=300000U)) { fail(); }
    }
    // A queued launch result is allowed to finish bookkeeping after native
    // activity selection has already made the old Omega run non-current.
    void finish_handoff(std::uint64_t now) noexcept {
        if(selected_ && phase_==native::Phase::complete && handoff_==native::Handoff::claimed) { advance(now,false); }
    }
private:
    class Driver final : public NativeServices<Driver> {
    public:
        explicit Driver(Controller& owner) noexcept : owner_(owner) {}
        [[nodiscard]] ServiceContext context() const noexcept { return {script::graph(owner_.retry_ ? "ending_retry" : "ending", owner_.retry_ ? kRetry : kDefinition), owner_.token_.run, owner_.executor_.diagnostics().incarnation}; }
        bool request(const Command& command) noexcept {
            if(command.schema!=Schema::omegaArchive || command.token.run!=owner_.token_.run) { return false; }
            const auto& spec=command.spec;
            if(spec.operation==Operation::observation) {
                return spec.wait==Wait::observed && (spec.asset==kRetirement || spec.asset==kBookend || spec.asset==kHandoff);
            }
            if(spec.wait!=Wait::requested) { return false; }
            switch(static_cast<Request>(spec.argument)) {
            case Request::retire:
                if(spec.operation!=Operation::mechanic || spec.asset!=kRetirement) { return false; }
                owner_.phase_=native::Phase::retiring;owner_.retireRequested_=true;owner_.requestedAt_=owner_.now_;return true;
            case Request::arrive:
                if(spec.operation!=Operation::traversal || spec.asset!=kArrival) { return false; }
                owner_.stateRequested_=true;owner_.phase_=native::Phase::preparing;owner_.requestedAt_=owner_.now_;return true;
            case Request::play:
                if(spec.operation!=Operation::cinematic || spec.asset!=kBookend) { return false; }
                owner_.revision_=owner_.nextRevision_++;owner_.play_=true;owner_.phase_=native::Phase::offered;
                owner_.offeredAt_=owner_.now_;++owner_.attempts_;return true;
            case Request::active:
                if(spec.operation!=Operation::cinematic || spec.asset!=kBookend) { return false; }
                owner_.phase_=native::Phase::playing;owner_.started_=true;owner_.startedAt_=owner_.now_;return true;
            case Request::finish:
                if(spec.operation!=Operation::cinematic || spec.asset!=kBookend) { return false; }
                owner_.phase_=native::Phase::complete;owner_.play_=false;owner_.revision_=owner_.nextRevision_++;return true;
            case Request::handoff:
                if(spec.operation!=Operation::traversal || spec.asset!=kHandoff) { return false; }
                owner_.handoff_=native::Handoff::pending;return true;
            }
            return false;
        }
        void retire(const Command&) noexcept {} // Native retirement is itself an acknowledged operation.
    private:
        Controller& owner_;
    };
    [[nodiscard]] bool matches(native::Token token) const noexcept { return token.valid() && token==token_; }
    [[nodiscard]] std::uint64_t incarnation() const noexcept { return executor_.diagnostics().incarnation; }
    [[nodiscard]] bool enqueue(const Receipt& receipt) noexcept {
        return receipts_.push(receipt, [](const Receipt& last, const Receipt& next) noexcept {
            return next.kind==Kind::camera && last.kind==Kind::camera && last.owner==next.owner
                && last.incarnation==next.incarnation && last.revision==next.revision
                && last.active==next.active && last.ready==next.ready && last.arrivedAtIntake==next.arrivedAtIntake;
        });
    }
    void drive() noexcept {
        Driver driver(*this);
        for(unsigned i=0;i<8;++i) {
            const auto before=executor_.diagnostics();executor_.update(driver);const auto after=executor_.diagnostics();
            if(after.phase==coo::Phase::failed) { fail();return; }
            if(before.active==after.active && before.complete==after.complete) { return; }
        }
    }
    void observed(std::string_view receipt) noexcept {
        static_cast<void>(executor_.enqueue({executor_.token(receipt),Milestone::observed}));drive();
    }
    void fail() noexcept {
        if(phase_==native::Phase::failed) { return; }
        Driver driver(*this);executor_.cancel(driver);
        phase_=native::Phase::failed;play_=false;revision_=nextRevision_++;receipts_.discard();
    }
    void apply(const Receipt& receipt) noexcept {
        switch(receipt.kind) {
        case Kind::retirement:
            if(phase_==native::Phase::retiring) { retired_=true;observed("lair.retired"); }break;
        case Kind::arrival:if(stateRequested_) { arrived_=true; }break;
        case Kind::skip:
            skipPending_=false;
            if(phase_==native::Phase::playing && play_) { play_=false;revision_=nextRevision_++; }break;
        case Kind::handoff:
            if(handoff_!=native::Handoff::claimed) { break; }
            handoff_=receipt.active?native::Handoff::queued:native::Handoff::failed;
            if(receipt.active) { observed("handoff.queued"); }
            else {
                Driver driver(*this);
                static_cast<void>(executor_.enqueue({executor_.token("handoff.queued"),Milestone::failed}));executor_.update(driver);
            }
            break;
        case Kind::camera:
            if(phase_==native::Phase::preparing && arrived_ && receipt.arrivedAtIntake && receipt.ready && !receipt.active) {
                observed("camera.eligible");
            } else if(phase_==native::Phase::offered && receipt.revision==revision_) {
                if(receipt.active && receipt.ready) { observed("camera.active"); }
                else if(!receipt.active && now_>=offeredAt_ && now_-offeredAt_>=1000U) {
                    if(attempts_>=3U) { fail(); }
                    else {
                        Driver driver(*this);executor_.cancel(driver);retry_=true;phase_=native::Phase::preparing;
                        if(!executor_.start(script::graph("ending_retry", kRetry),token_.run)) { fail();return; }drive();
                    }
                }
            } else if(phase_==native::Phase::playing && receipt.revision==revision_ && !receipt.active && receipt.ready) {
                observed("camera.complete");
            }
            break;
        }
    }
    Executor executor_{};
    native::Ending legacy_{};
    native::Token token_{};
    native::Phase phase_{};
    native::Handoff handoff_{};
    ReceiptQueue<Receipt,kCapacity> receipts_{};
    std::uint64_t now_{},requestedAt_{},offeredAt_{},startedAt_{};
    std::uint32_t revision_{},nextRevision_{1};
    std::uint8_t attempts_{};
    bool selected_{},retry_{},stateRequested_{},play_{},started_{},arrived_{},retireRequested_{},retired_{};
    bool retirementPending_{},arrivalPending_{},skipPending_{},handoffPending_{};
};
static_assert(std::is_trivially_copyable_v<Controller>);
} // namespace dawn::state::activity::coo::ending
