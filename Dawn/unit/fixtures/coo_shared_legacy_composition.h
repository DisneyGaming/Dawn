// Frozen accepted ending candidate; original definition and adapter bodies.
#pragma once
#include "state/activity/coo/omega_adapter.h"
namespace dawn::state::activity::coo::frozen_composition {
// First migration stage: lease the accepted controllers. Request acceptance
// does not assert native readiness. Authored operations stay in the controllers.
inline constexpr std::array<CommandSpec, 3> kControllers{{
    {Operation::mechanic, {}, 0, Wait::requested},
    {Operation::mechanic, {}, 1, Wait::requested},
    {Operation::mechanic, {}, 2, Wait::requested}
}};
inline constexpr std::array<CommandSpec, 8> kMilestones{{
    {Operation::observation, {}, 0, Wait::observed},
    {Operation::observation, {}, 1, Wait::observed},
    {Operation::observation, {}, 2, Wait::observed},
    {Operation::observation, {}, 3, Wait::observed},
    {Operation::observation, {}, 4, Wait::observed},
    {Operation::observation, {}, 5, Wait::observed},
    {Operation::observation, {}, 6, Wait::observed},
    {Operation::observation, {}, 7, Wait::observed}
}};
inline constexpr std::array<Step, 9> kSteps{{
    {"controller leases", 0, kControllers},
    {"opening and Ikora", 1U, {&kMilestones[0], 1}},
    {"Forest traversal", 2U, {&kMilestones[1], 1}},
    {"Lair arrival and encounters", 4U, {&kMilestones[2], 1}},
    {"Crown cycle 1", 8U, {&kMilestones[3], 1}},
    {"Crown cycle 2", 16U, {&kMilestones[4], 1}},
    {"Crown cycle 3", 32U, {&kMilestones[5], 1}},
    {"ending cinematic", 64U, {&kMilestones[6], 1}},
    {"handoff queued", 128U, {&kMilestones[7], 1}}
}};
inline constexpr Definition kDefinition{"Omega archive adapter", Schema::omegaArchive, kSteps};
struct Input final {
    std::uint64_t run{}, now{};
    int region{};
    bool entrance{}, executor{};
};
struct Frame final {
    omega_presentation::Presentation presentation{};
    omega_first_lair::Authority encounter{};
    omega_ending::Authority ending{};
};

// Injection boundary shared by production and differential replay.
struct Controllers {
    virtual omega_presentation::Presentation presentation(const Input&) noexcept = 0;
    virtual omega_first_lair::Authority encounter(std::uint64_t, std::uint32_t, bool) noexcept = 0;
    virtual void request_ending(std::uint64_t, bool) noexcept = 0;
    virtual omega_ending::Authority ending(const Input&) noexcept = 0;
    virtual std::array<bool, 8> observations(std::uint64_t, const Frame&) noexcept = 0;
    virtual ~Controllers() = default;
};

class Adapter final : private Services {
public:
    [[nodiscard]] bool select(std::uint64_t run, bool requested) noexcept {
        if (run == 0) { return false; }
        if (selectionRun_ != run) { selectionRun_ = run; selected_ = requested; }
        return selected_;
    }
    [[nodiscard]] Frame update(const Input& input, Controllers& controllers) noexcept {
        if (input.run == 0) { return {}; }
        static_cast<void>(select(input.run, input.executor));
        if (run_ != input.run) {
            executor_.cancel(*this);
            leases_ = {}; observed_ = {};
            run_ = input.run;
            started_ = !selected_ || executor_.start(kDefinition, run_);
        }
        if (!started_) { return {}; }
        if (selected_) {
            executor_.update(*this);
            if (executor_.diagnostics().phase == Phase::failed) { return {}; }
        }
        Frame frame;
        // Preserve reference ordering and exactly one call per authority producer.
        auto selectedInput = input;
        selectedInput.executor = selected_; // Same per-run choice as opening intake, even if settings change.
        if (!selected_ || leases_[0]) { frame.presentation = controllers.presentation(selectedInput); }
        if (!selected_ || leases_[1]) {
            frame.encounter = controllers.encounter(input.run, frame.presentation.bossGeneration, selected_);
        }
        if (!selected_ || leases_[2]) {
            if (frame.encounter.endingRequested) { controllers.request_ending(input.run, selected_); }
            frame.ending = controllers.ending(selectedInput);
        }
        if (selected_ && executor_.diagnostics().phase == Phase::running) {
            const auto facts = controllers.observations(input.run, frame);
            for (std::size_t i = 0; i < facts.size(); ++i) { observed_[i] = observed_[i] || facts[i]; }
            // Retain observations through fast transitions and direct Lair launches.
            // Elapsed time alone cannot satisfy a condition.
            for (std::size_t i = 0; i < facts.size(); ++i) {
                if (!observed_[i] || executor_.step_state(i + 1).phase != StepPhase::active) { continue; }
                static_cast<void>(executor_.enqueue({executor_.token(i + 1, 0), Milestone::observed}));
                executor_.update(*this);
            }
        }
        return frame;
    }
    void reset() noexcept {
        executor_.cancel(*this);
        run_ = selectionRun_ = 0; selected_ = started_ = false; observed_ = {}; leases_ = {};
    }
    [[nodiscard]] bool selected() const noexcept { return selected_; }
    [[nodiscard]] Diagnostics diagnostics() const noexcept { return executor_.diagnostics(); }
private:
    bool publish(const Command& command) noexcept override {
        if (command.schema != Schema::omegaArchive || command.token.run != run_) { return false; }
        if (command.spec.operation == Operation::observation) { return true; }
        if (command.spec.operation != Operation::mechanic || command.spec.argument >= leases_.size()) { return false; }
        leases_[command.spec.argument] = true;
        return true;
    }
    void cancel(const Command& command) noexcept override {
        if (command.spec.operation == Operation::mechanic && command.spec.argument < leases_.size()) {
            leases_[command.spec.argument] = false;
        }
    }
    Executor executor_;
    std::uint64_t run_{}, selectionRun_{};
    std::array<bool, 3> leases_{};
    std::array<bool, 8> observed_{};
    bool selected_{}, started_{};
};


}
