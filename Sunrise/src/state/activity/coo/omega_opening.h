#pragma once
#include "omega_script_views.h"
#include "native_services.h"

#include "executor.h"
#include <type_traits>
#include "../omega_ikora_lattice.h"

namespace sunrise::state::activity::coo::omega::opening {
namespace lattice = omega_ikora_lattice;

// Authored graph and GUID-bound device definition, not registry catalog offsets.
inline constexpr Asset kScene{0xD00142CF, 0x80EC0F95, 43, 1};
inline constexpr Asset kGate{0xD00142CF, 0x80F47BA0, 23, 16};
inline constexpr std::uint32_t kApproachEvent = 0xC7ECAA77;
inline constexpr std::array kBootstrap{
    CommandSpec{Operation::observation, {}, 0, Wait::observed},
    CommandSpec{Operation::device, kGate, 0, Wait::requested}};
inline constexpr std::array kApproach{
    CommandSpec{Operation::observation, {0xD00142CF, 0, 30, 20}, 0, Wait::observed}};
inline constexpr std::array kRequest{
    CommandSpec{Operation::scene, kScene, kApproachEvent, Wait::nativeReady}};
inline constexpr std::array kOutput{
    CommandSpec{Operation::observation, kScene, lattice::kReleaseEvent, Wait::observed}};
inline constexpr std::array kRelease{
    CommandSpec{Operation::device, kGate, 1, Wait::requested}};
inline constexpr std::array kEntrance{
    CommandSpec{Operation::observation, {0xD00142CF, 0, 30, 24}, 0, Wait::observed}};
inline constexpr std::array kSteps{
    Step{"roster admitted", 0, kBootstrap},
    Step{"Ikora approach", 1U << 0, kApproach},
    Step{"Ikora scene ready", 1U << 1, kRequest},
    Step{"Ikora portal output", 1U << 2, kOutput},
    Step{"lattice release requested", 1U << 3, kRelease},
    // Direct Forest entry does not require replaying the Lighthouse scene.
    Step{"Forest entrance observed", 1U << 0, kEntrance}};
inline constexpr ReceiptBinding kReceipts[]{{"roster.admitted",0,0},{"ikora.approached",1,0},{"ikora.scene.ready",2,0},{"ikora.portal.released",3,0},{"forest.entered",5,0}};
inline constexpr Definition kDefinition{"Omega opening", Schema::omegaArchive, kSteps, kReceipts};

enum class Kind : std::uint8_t { reset, roster, approach, scene, entrance };
// Copied only after transport/epoch, exact monitor and full scene-body validation.
// Store the source binding and packet ordinal at intake, never at consumption.
struct Receipt final {
    std::uint64_t binding{}, packet{};
    Kind kind{};
    std::uint32_t generation{}, revision{};
    bool delta{}, release{};
};
[[nodiscard]] inline Receipt scene_receipt(std::uint64_t binding, std::uint64_t packet,
                                           const lattice::Scene& scene) noexcept {
    Receipt result{binding, packet, Kind::scene, scene.generationWire, scene.revision, scene.hasDelta};
    for (std::size_t i = 0; i < scene.eventCount && i < scene.events.size(); ++i) {
        result.release |= scene.events[i] == lattice::kReleaseEvent;
    }
    return result;
}
struct Authority final {
    bool roster{}, requested{}, released{}, entrance{}, failed{};
};
struct Changes final {
    bool reset{}, ready{}, approached{}, released{}, entered{}, failed{};
    std::uint64_t packet{};
    std::uint32_t sceneRevision{};
};

// Owned and serialized by the authenticated BAP binding. Value copies carry no
// borrowed callback pointers. Replacing the binding/patch epoch discards its
// entire queue. A roster-reset receipt retires leases on the normal update.
class Run final {
    // The BAP owner securely wipes its value storage and then reassigns defaults.
    // A vtable pointer in that storage is not restored by assignment. Keep the
    // polymorphic service bridge on the update stack, never inside the session.
    class CallServices final : public NativeServices<CallServices> {
    public:
        explicit CallServices(Run& run) noexcept : run_(run) {}
        [[nodiscard]] ServiceContext context() const noexcept { return {script::graph("opening", kDefinition), run_.binding_, run_.executor_.diagnostics().incarnation}; }
        bool request(const Command& command) noexcept { return run_.publish(command); }
        void retire(const Command& command) noexcept { run_.cancel(command); }
    private:
        Run& run_;
    };
public:
    static constexpr std::size_t kCapacity = 128;
    [[nodiscard]] bool enqueue(Receipt receipt) noexcept {
        if (receipt.binding == 0 || receipt.packet == 0 || overflow_) { return false; }
        if (count_ == queue_.size()) { overflow_ = true; return false; }
        queue_[count_++] = receipt;
        return true; // No mission state changes at intake.
    }
    [[nodiscard]] Changes update(std::uint64_t binding) noexcept {
        CallServices services(*this);
        Changes changes;
        if (binding == 0) { return changes; }
        if (binding_ != 0 && binding_ != binding) {
            // Foreign pending receipts cannot be relabelled with a new owner.
            executor_.cancel(services); state_ = {}; validator_.reset();
            lastPacket_ = 0;
        }
        binding_ = binding;
        if (overflow_) {
            changes.failed = !state_.failed;
            executor_.cancel(services); state_ = {}; state_.failed = true;
            failure_ = Failure::queueOverflow;
            count_ = 0; return changes;
        }
        if (state_.failed) { count_ = 0; return changes; }
        for (std::size_t i = 0; i < count_; ++i) {
            const auto receipt = queue_[i];
            if (receipt.binding != binding_ || receipt.packet < lastPacket_) { continue; }
            lastPacket_ = receipt.packet;
            changes.packet = receipt.packet;
            if (receipt.kind == Kind::reset) {
                executor_.cancel(services); state_ = {}; validator_.reset();
                changes.reset = true;
                // Do not advertise changes belonging to the retired graph.
                changes.ready = changes.approached = changes.released = changes.entered = false;
                continue;
            }
            if (receipt.kind == Kind::roster && !state_.roster) {
                if (!executor_.start(script::graph("opening", kDefinition), binding_)) { state_.failed = true; break; }
                validator_.begin(binding_);
                executor_.update(services);
                observe("roster.admitted", services);
                state_.roster = true; changes.ready = true;
            } else if (receipt.kind == Kind::approach && state_.roster && !state_.requested) {
                observe("ikora.approached", services); changes.approached = state_.requested;
            } else if (receipt.kind == Kind::scene && state_.roster && state_.requested) {
                lattice::Scene scene{};
                scene.generationWire = receipt.generation; scene.revision = receipt.revision;
                scene.hasDelta = receipt.delta; scene.eventCount = receipt.release ? 1 : 0;
                scene.events[0] = lattice::kReleaseEvent;
                const auto result = validator_.observe(binding_, true, scene);
                if (result != lattice::Observation::ignored) {
                    if (executor_.receipt_state("ikora.scene.ready").phase == StepPhase::active) {
                        static_cast<void>(executor_.enqueue({executor_.token("ikora.scene.ready"), Milestone::nativeReady}));
                        executor_.update(services);
                    }
                    if (result == lattice::Observation::released) {
                        observe("ikora.portal.released", services);
                        changes.released = state_.released;
                        changes.sceneRevision = scene.revision;
                    }
                }
            } else if (receipt.kind == Kind::entrance && state_.roster && !state_.entrance) {
                observe("forest.entered", services);
                state_.entrance = executor_.receipt_state("forest.entered").phase == StepPhase::complete;
                changes.entered = state_.entrance;
            }
            if (executor_.diagnostics().phase == Phase::failed) { state_.failed = true; break; }
        }
        count_ = 0;
        // Requested device leases may have just been activated by a join.
        executor_.update(services);
        if (state_.failed || executor_.diagnostics().phase == Phase::failed) {
            failure_ = executor_.diagnostics().failure;
            executor_.cancel(services); state_ = {}; state_.failed = true; changes.failed = true;
        }
        return changes;
    }
    [[nodiscard]] Authority authority() const noexcept { return state_; }
    [[nodiscard]] Diagnostics diagnostics() const noexcept {
        auto result = executor_.diagnostics();
        if (state_.failed) { result.phase = Phase::failed; result.failure = failure_; }
        return result;
    }
    [[nodiscard]] std::size_t queued() const noexcept { return count_; }

private:
    void observe(std::string_view receipt, Services& services) noexcept {
        if (executor_.receipt_state(receipt).phase != StepPhase::active) { return; }
        static_cast<void>(executor_.enqueue({executor_.token(receipt), Milestone::observed}));
        executor_.update(services);
    }
    bool publish(const Command& command) noexcept {
        if (command.token.run != binding_ || command.schema != Schema::omegaArchive) { return false; }
        const auto& spec = command.spec;
        if (spec.operation == Operation::observation) { return true; }
        if (spec.operation == Operation::scene && spec.asset == kScene && spec.argument == kApproachEvent) {
            state_.requested = true; return true;
        }
        if (spec.operation == Operation::device && spec.asset == kGate && spec.argument <= 1) {
            state_.released = spec.argument == 1; return true;
        }
        return false;
    }
    void cancel(const Command& command) noexcept {
        if (command.spec.operation == Operation::scene) { state_.requested = false; }
        if (command.spec.operation == Operation::device) { state_.released = false; }
    }
    Executor executor_;
    lattice::State validator_;
    std::array<Receipt, kCapacity> queue_{};
    Authority state_{};
    std::uint64_t binding_{}, lastPacket_{};
    std::size_t count_{};
    bool overflow_{};
    Failure failure_{};
};
static_assert(std::is_trivially_copyable_v<Run> && std::is_standard_layout_v<Run>,
    "Opening state must remain safe for the BAP session's secure-wipe/value-copy lifecycle.");
} // namespace sunrise::state::activity::coo::omega::opening
