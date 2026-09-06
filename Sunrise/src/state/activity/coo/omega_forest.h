#pragma once
#include "omega_script_views.h"
#include "native_services.h"
#include "presentation_services.h"
#include "../omega_presentation_rules.h"

namespace sunrise::state::activity::coo::omega::forest {
namespace presentation = omega_presentation;
// Type-60 volume identities come from omega_presentation_volumes.h. Zero
// definition means the registry/type/slot is the authored identity, not a tag guess.
inline constexpr std::array<Asset, 4> kVolumes{{
    {0xBA5F26EFU, 0, 60, 5}, {0x2763EC97U, 0, 60, 4},
    {0x2763EC97U, 0, 60, 5}, {0xA3928C71U, 0, 60, 2}
}};
// Type 68 slot 0 publishes an event selector; type 53 slot 2 publishes bank rows.
// The selector is the leaf identity here. The bank is 80F1FD07, retained by the
// existing native dispatcher; compound/scene-owned rows remain native-owned.
inline constexpr Asset kForestObjective{0x82FB58B7U, 0x1EBF4621U, 68, 0};
inline constexpr Asset kLairObjective{0x82FB58B7U, 0x3517D4D5U, 68, 0};
inline constexpr std::array<Asset, 3> kLines{{
    {0x82FB58B7U, 0xAE2495ACU, 53, 2}, {0x82FB58B7U, 0x0E9C80BEU, 53, 2},
    {0x82FB58B7U, 0xE878194AU, 53, 2}
}};
inline constexpr std::array<TraversalBinding, 4> kTraversal{{
    {kVolumes[0], 1}, {kVolumes[1], 2}, {kVolumes[2], 3}, {kVolumes[3], 4}
}};
inline constexpr std::array<ObjectiveBinding, 2> kObjectives{{
    {kForestObjective, 0x1EBF4621U}, {kLairObjective, 0x3517D4D5U}
}};
inline constexpr std::array<DialogueBinding, 3> kDialogue{{
    {kLines[0], 6, 1500}, {kLines[1], 7, 0}, {kLines[2], 9, 0}
}};
inline constexpr PresentationBindings kBindings{Schema::omegaArchive, kTraversal, kObjectives, kDialogue};
inline constexpr std::array<CommandSpec, 4> kObserve{{
    {Operation::observation, kVolumes[0], 1, Wait::observed},
    {Operation::observation, kVolumes[1], 2, Wait::observed},
    {Operation::observation, kVolumes[2], 3, Wait::observed},
    {Operation::observation, kVolumes[3], 4, Wait::observed}
}};
// Argument is the route stage that owns this request. A later direct arrival
// retires earlier stages without narrating volumes the player never entered.
inline constexpr std::array<CommandSpec, 3> kTunnel{{
    {Operation::traversal, kVolumes[0], 1, Wait::requested},
    {Operation::objective, kForestObjective, 1, Wait::requested},
    {Operation::dialogue, kLines[0], 1, Wait::requested}
}};
inline constexpr std::array<CommandSpec, 3> kVista{{
    {Operation::traversal, kVolumes[1], 2, Wait::requested},
    {Operation::objective, kForestObjective, 2, Wait::requested},
    {Operation::dialogue, kLines[1], 2, Wait::requested}
}};
inline constexpr std::array<CommandSpec, 3> kExit{{
    {Operation::traversal, kVolumes[2], 3, Wait::requested},
    {Operation::objective, kForestObjective, 3, Wait::requested},
    {Operation::dialogue, kLines[2], 3, Wait::requested}
}};
inline constexpr std::array<CommandSpec, 2> kLair{{
    {Operation::traversal, kVolumes[3], 4, Wait::requested},
    {Operation::objective, kLairObjective, 4, Wait::requested}
}};
inline constexpr std::array<Step, 8> kSteps{{
    {"tunnel reached or passed", 0, {&kObserve[0], 1}}, {"tunnel presentation", 1U, kTunnel},
    {"vista reached or passed", 0, {&kObserve[1], 1}}, {"vista presentation", 4U, kVista},
    {"Forest exit reached or passed", 0, {&kObserve[2], 1}}, {"Forest exit presentation", 16U, kExit},
    {"Lair reached or passed", 0, {&kObserve[3], 1}}, {"Lair handoff", 64U, kLair}
}};
inline constexpr ReceiptBinding kReceipts[]{{"tunnel.reached",0,0},{"vista.reached",2,0},{"forest.exit.reached",4,0},{"lair.reached",6,0}};
inline constexpr Definition kDefinition{"Omega Forest to Lair", Schema::omegaArchive, kSteps, kReceipts};

class Sequence final {
public:
    void start(std::uint64_t run, presentation::Run& target, std::uint64_t now) noexcept {
        stop(target, now);
        rank_ = 0; skipped_ = 0;
        if (executor_.start(script::graph("forest", kDefinition), run)) { Service service(*this, target, now); executor_.update(service); }
    }
    void stop(presentation::Run& target, std::uint64_t now) noexcept {
        Service service(*this, target, now); executor_.cancel(service);
    }
    void enter(presentation::Landmark landmark, presentation::Run& target, std::uint64_t now) noexcept {
        const auto rank = static_cast<std::uint8_t>(landmark);
        if (rank > 5 || failed()) { return; }
        if (rank == 0) { target.enter(landmark, now); return; }
        if (rank <= rank_) { return; }
        rank_ = rank;
        Service service(*this, target, now);
        constexpr std::string_view receipts[]{"tunnel.reached","vista.reached","forest.exit.reached","lair.reached"};
        for (std::size_t i = 0; i < kVolumes.size() && i < rank; ++i) {
            if (executor_.receipt_state(receipts[i]).phase != StepPhase::active) { continue; }
            if (i+1 < rank) { skipped_ |= 1U << i; }
            static_cast<void>(executor_.enqueue({executor_.token(receipts[i]), Milestone::observed}));
        }
        executor_.update(service); // Join observations and request their operations.
        executor_.update(service); // Join Wait::requested, without inventing native receipts.
        if (rank == 5 && !failed()) { target.enter(landmark, now); }
    }
    [[nodiscard]] Diagnostics diagnostics() const noexcept { return executor_.diagnostics(); }
    [[nodiscard]] bool failed() const noexcept { return diagnostics().phase == Phase::failed; }
    [[nodiscard]] std::uint32_t skipped() const noexcept { return skipped_; }
private:
    class Service final : public NativeServices<Service> {
    public:
        Service(Sequence& owner, presentation::Run& target, std::uint64_t now) noexcept
            : owner_(owner), shared_(script::presentation("forest", kBindings), target, now) {}
        [[nodiscard]] ServiceContext context() const noexcept { return {script::graph("forest", kDefinition), owner_.diagnostics().run, owner_.diagnostics().incarnation}; }
        bool request(const Command& command) noexcept {
            if (command.schema != Schema::omegaArchive || command.token.run != owner_.diagnostics().run) { return false; }
            if (command.spec.operation == Operation::observation) {
                return command.spec.wait == Wait::observed && resolve(kVolumes, command.spec.asset) != nullptr;
            }
            if (!shared_.valid(command) || command.spec.argument < 1 || command.spec.argument > 4) { return false; }
            return command.spec.argument < owner_.rank_ ||
                (command.spec.argument == owner_.rank_ && shared_.publish(command));
        }
        // These are one-shot requests to the run-owned scheduler, not native leases.
        // Resetting the presentation run clears unpublished work; native audio owns submitted clips.
        void retire(const Command&) noexcept {}
    private:
        Sequence& owner_;
        PresentationServices<presentation::Run> shared_;
    };
    Executor executor_;
    std::uint8_t rank_{};
    std::uint32_t skipped_{};
};
} // namespace sunrise::state::activity::coo::omega::forest
