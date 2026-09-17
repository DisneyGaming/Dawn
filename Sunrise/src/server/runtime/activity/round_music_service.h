#pragma once

#include "music_runtime.h"
#include "timed_round_service.h"
#include <cstdint>

namespace sunrise::server::runtime::activity::round_music {

// The phase-to-selector mapping is host policy reconstructed from the supplied
// authored names. It is deliberately separate from the native music graph and
// does not claim retail frame timing.
struct Binding final {
    const music::Definition* definition{};
    std::uint32_t boots{};
    std::uint32_t boss{};
    std::uint32_t suddenDeath{};
    std::uint32_t victory{};
    std::uint32_t trap{}; // Optional authored reward-area occupancy selection.
};

[[nodiscard]] inline bool valid(const Binding& binding) noexcept {
    if (!binding.definition || !binding.boots || !binding.boss || !binding.suddenDeath
        || !binding.victory || !music::Runtime::valid(*binding.definition)) return false;
    const auto contains = [&](std::uint32_t key) noexcept {
        if (!key) return true;
        for (const auto candidate : binding.definition->candidates)
            if (candidate == key) return true;
        return false;
    };
    return contains(binding.boots) && contains(binding.boss)
        && contains(binding.suddenDeath) && contains(binding.victory) && contains(binding.trap);
}

// Owns only the optional presentation lease. It never advances a round and it
// never supplies a gameplay receipt. A failed music graph is bounded to this
// service and leaves the caller's gameplay frame usable.
class Service final {
public:
    [[nodiscard]] bool configure(const Binding* binding) noexcept {
        if (configured_) return false;
        if (binding && !valid(*binding)) return false;
        binding_ = binding;
        configured_ = true;
        return true;
    }

    [[nodiscard]] bool update(const music::Context& context, timed_round::Phase phase,
        bool expired, bool lifecycleComplete = false, bool trapOccupied = false) noexcept {
        if (!configured_ || !binding_ || failed_) return !failed_;
        if (!context.arrived || !context.admitted
            || context.bubble != binding_->definition->registry->bubble) return true;

        if (!started_) {
            if (!runtime_.begin(*binding_->definition, context)) return fail();
            started_ = true;
        }

        const auto selection = select(phase, expired, lifecycleComplete, trapOccupied);
        if (!selectionSet_ || selection != selected_) {
            if (sequence_ == UINT64_MAX || !runtime_.request(selection, sequence_ + 1, context))
                return fail();
            selected_ = selection;
            selectionSet_ = true;
            ++sequence_;
        }
        if (!runtime_.update(context)) return fail();
        return true;
    }

    [[nodiscard]] bool append(music::wire::Batch& output) const noexcept {
        if (!configured_ || !binding_ || failed_ || !started_) return true;
        return runtime_.append(output);
    }

    [[nodiscard]] bool failed() const noexcept { return failed_; }
    [[nodiscard]] bool started() const noexcept { return started_; }
    [[nodiscard]] std::uint64_t sequence() const noexcept { return sequence_; }
    [[nodiscard]] std::uint32_t selection() const noexcept { return selected_; }

    // Activity retirement destroys the optional presentation lease. Repeated
    // plates and new rounds intentionally do not call this method.
    void retire() noexcept {
        runtime_ = {};
        binding_ = nullptr;
        configured_ = started_ = failed_ = false;
        selected_ = 0;
        selectionSet_ = false;
        sequence_ = 0;
    }

private:
    [[nodiscard]] std::uint32_t select(timed_round::Phase phase, bool expired,
        bool lifecycleComplete, bool trapOccupied) const noexcept {
        if (lifecycleComplete || phase == timed_round::Phase::complete
            || phase == timed_round::Phase::faulted)
            return 0;
        if (phase == timed_round::Phase::rewards && trapOccupied && binding_->trap)
            return binding_->trap;
        if (expired && (phase == timed_round::Phase::traversal
            || phase == timed_round::Phase::toEncounter || phase == timed_round::Phase::encounter))
            return binding_->suddenDeath;
        switch (phase) {
        case timed_round::Phase::entry:
        case timed_round::Phase::traversal:
        case timed_round::Phase::toEncounter:
            return binding_->boots;
        case timed_round::Phase::encounter:
            return binding_->boss;
        case timed_round::Phase::returning:
        case timed_round::Phase::rewards:
            return binding_->victory;
        default:
            return 0;
        }
    }

    [[nodiscard]] bool fail() noexcept {
        failed_ = true;
        return false;
    }

    const Binding* binding_{};
    music::Runtime runtime_{};
    std::uint64_t sequence_{};
    std::uint32_t selected_{};
    bool configured_{}, started_{}, failed_{}, selectionSet_{};
};

} // namespace sunrise::server::runtime::activity::round_music
