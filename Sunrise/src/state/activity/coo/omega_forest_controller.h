#pragma once
#include "receipt_queue.h"
#include "omega_forest.h"
#include "../omega_presentation_volumes.h"
#include <type_traits>

namespace sunrise::state::activity::coo::omega::forest {
enum class Kind : std::uint8_t { position, submission, scene, intro, encounter, entrance };
struct Owner final {
    std::uint64_t run{}, incarnation{};
    friend constexpr bool operator==(const Owner&, const Owner&) = default;
};
struct Receipt final {
    Owner owner;
    std::uint64_t now{};
    Kind kind{};
    presentation::Point position{};
    std::uint32_t identity{}, generation{};
    std::uint8_t row{}, cycle{};
    bool active{}, ready{}, flightReady{};
};
struct Before final {
    std::uint32_t revision;
    presentation::IntroPhase introPhase;
    presentation::IntroCommand intro;
};

// Serialized by the existing presentation lock. Hooks copy observations here;
// only drain() enters the executor or publishes shared presentation requests.
// All asynchronous presentation receipts share the FIFO so an arena/intro edge
// cannot overtake a Forest volume or an earlier native dialogue submission.
class Controller final {
public:
    static constexpr std::size_t kCapacity = 128;
    [[nodiscard]] presentation::Run& state() noexcept { return state_; }
    [[nodiscard]] const presentation::Run& state() const noexcept { return state_; }
    [[nodiscard]] bool selected() const noexcept { return selected_; }
    [[nodiscard]] Owner owner() const noexcept { return {state_.generation(), sequence_.diagnostics().incarnation}; }
    [[nodiscard]] std::uint64_t update_time(std::uint64_t now) const noexcept { return (std::max)(now, lastApplied_); }
    [[nodiscard]] std::size_t pending() const noexcept { return queue_.size(); }
    [[nodiscard]] bool failed() const noexcept { return queue_.overflowed() || sequence_.failed(); }
    [[nodiscard]] std::uint32_t skipped() const noexcept { return sequence_.skipped(); }
    [[nodiscard]] Diagnostics diagnostics() const noexcept {
        auto result = sequence_.diagnostics();
        result.queued = queue_.size();
        if (queue_.overflowed()) { result.phase = Phase::failed; result.failure = Failure::queueOverflow; }
        return result;
    }
    void start(std::uint64_t run, std::uint64_t now, presentation::Landmark arrival, bool selected) noexcept {
        reset();
        selected_ = selected;
        if (selected_) {
            state_.initialize(run,true);
            sequence_.start(run, state_, now);
            sequence_.enter(arrival, state_, now);
        } else { state_.start(run, now, arrival); }
    }
    void reset() noexcept {
        sequence_.stop(state_, 0);
        state_ = {};
        queue_.reset();
        selected_ = false; lastApplied_ = 0;
    }
    // An owner stamped at intake cannot survive a reset, even if run is reused.
    [[nodiscard]] bool enqueue(const Receipt& receipt) noexcept {
        if (!selected_ || failed() || state_.generation() == 0 || receipt.owner != owner()) { return false; }
        // The Scene apply hook reports every component, including inactive and
        // unrelated scenes. These can never affect this presentation controller;
        // discard them before they consume bounded queue space.
        if (receipt.kind == Kind::scene) {
            bool authored{};
            for (const auto& binding : presentation::kRescuePresentation) { authored |= receipt.identity == binding.definition; }
            if (!receipt.active || !authored) { return true; }
        }
        if (receipt.kind == Kind::submission) {
            const auto& p = state_.presentation();
            // Reject unsolicited/early acknowledgements at intake, before any
            // future update can offer that row with the same run generation.
            if (receipt.identity != presentation::kDialogueBank || receipt.row >= presentation::kDialogueRows
                || receipt.row != p.activeRow || p.generations[receipt.row] != receipt.generation) { return false; }
        }
        if (receipt.kind == Kind::position) {
            const auto signature = position_signature(receipt.position);
            if (signature == 0) { return true; }
            if (const auto* previous = queue_.back()) {
                const auto& last = *previous;
                // Same semantic position, with no intervening receipt: retain
                // the earliest timestamp so dialogue spacing cannot drift.
                if (last.kind == Kind::position && position_signature(last.position) == signature) { return true; }
            }
        }
        auto copied = receipt;
        copied.flightReady = state_.boss_flight_ready();
        return queue_.push(copied);
    }
    // Legacy mode remains synchronous. Production admission occurs before this
    // boundary; tests use the same intake and drain with explicit owner tokens.
    void observe(Receipt receipt) noexcept {
        receipt.owner = owner();
        if (selected_) { static_cast<void>(enqueue(receipt)); }
        else { apply(receipt); }
    }
    template<class OnApplied>
    void drain(OnApplied onApplied) noexcept {
        if (!selected_) { return; }
        if (failed()) {
            sequence_.stop(state_, 0); queue_.discard();
            return;
        }
        Receipt receipt{};
        while (queue_.pop(receipt)) {
            if (receipt.owner != owner()) { continue; }
            const Before before{state_.revision(), state_.intro_phase(), state_.presentation().intro};
            apply(receipt);
            onApplied(receipt, before);
            if (failed()) { queue_.discard(); return; }
        }
    }
    void drain() noexcept { drain([](const Receipt&, Before) noexcept {}); }
    void entrance(std::uint64_t now) noexcept {
        // Called at the owner update, after draining earlier hook receipts.
        enter(presentation::Landmark::tunnel, now);
    }
private:
    static std::uint32_t position_signature(presentation::Point point) noexcept {
        if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)) { return 0; }
        std::uint32_t mask{};
        for (std::size_t i = 0; i < presentation::kVolumes.size(); ++i) {
            if (presentation::contains(presentation::kVolumes[i], point)) { mask |= 1U << i; }
        }
        if (presentation::contains(presentation::kIntroVolume, point)) { mask |= 1U << 6; }
        for (unsigned i = 0; i < static_cast<unsigned>(presentation::NavigationGoal::complete); ++i) {
            presentation::Point target{};
            if (!presentation::navigation_point(static_cast<presentation::NavigationGoal>(i), target)) { continue; }
            const float x=point.x-target.x, y=point.y-target.y, z=point.z-target.z;
            if (x*x+y*y+z*z <= 49.0F) { mask |= 1U << (i+7); }
        }
        return mask;
    }
    void enter(presentation::Landmark landmark, std::uint64_t now) noexcept {
        if (failed()) { return; }
        if (selected_) { sequence_.enter(landmark, state_, now); }
        else { state_.enter(landmark, now); }
    }
    void apply(const Receipt& receipt) noexcept {
        lastApplied_ = (std::max)(lastApplied_, receipt.now);
        switch (receipt.kind) {
        case Kind::position:
            for (const auto& volume : presentation::kVolumes) {
                if (presentation::contains(volume, receipt.position)) { enter(volume.landmark, receipt.now); }
            }
            if (presentation::contains(presentation::kIntroVolume, receipt.position)) { state_.request_intro(receipt.now); }
            state_.observe_navigation(receipt.position);
            break;
        case Kind::submission: state_.submitted(receipt.identity, receipt.row, receipt.generation, receipt.now); break;
        case Kind::scene: state_.scene(receipt.identity, receipt.active, receipt.now); break;
        case Kind::intro:
            state_.observe_intro(receipt.generation, receipt.active, receipt.ready, receipt.now,
                                 selected_ ? receipt.flightReady : state_.boss_flight_ready()); break;
        case Kind::encounter:
            state_.encounter(static_cast<presentation::Encounter>(receipt.identity), receipt.cycle, receipt.now); break;
        case Kind::entrance: enter(presentation::Landmark::tunnel, receipt.now); break;
        }
    }
    presentation::Run state_;
    Sequence sequence_;
    ReceiptQueue<Receipt, kCapacity> queue_{};
    bool selected_{};
    std::uint64_t lastApplied_{};
};
static_assert(std::is_trivially_copyable_v<Controller> && std::is_standard_layout_v<Controller>);
} // namespace sunrise::state::activity::coo::omega::forest
