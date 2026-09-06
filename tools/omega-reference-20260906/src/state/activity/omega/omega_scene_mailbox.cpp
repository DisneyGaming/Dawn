#include "state/activity/omega/omega_scene_mailbox.h"

namespace sunrise::state::activity::omega {

SceneRetirementMailbox::SceneRetirementMailbox(SceneGeneration initialGeneration) noexcept
    : state_(pack(initialGeneration)) {}

bool SceneRetirementMailbox::advance_to(SceneGeneration expectedGeneration,
                                        SceneGeneration nextGeneration) noexcept {
    if (!can_represent(expectedGeneration) || !can_represent(nextGeneration)
        || nextGeneration.value <= expectedGeneration.value) {
        return false;
    }

    std::uint64_t observed = state_.load(std::memory_order_acquire);
    for (;;) {
        if (unpack_generation(observed) != expectedGeneration) return false;

        const std::uint64_t desired = pack(nextGeneration);
        if (state_.compare_exchange_weak(
                observed, desired, std::memory_order_acq_rel, std::memory_order_acquire)) {
            return true;
        }
    }
}

bool SceneRetirementMailbox::schedule(SceneGeneration expectedGeneration,
                                      SceneRetirement retirement) noexcept {
    if (!can_represent(expectedGeneration)) return false;

    const SceneRetirementMask bit = retirement_mask(retirement);
    if (bit == 0U || (bit & kAllRetirements) != bit || (bit & (bit - 1U)) != 0U) return false;

    std::uint64_t observed = state_.load(std::memory_order_acquire);
    for (;;) {
        if (unpack_generation(observed) != expectedGeneration) return false;
        if ((unpack_retirements(observed) & bit) != 0U) return false;

        const std::uint64_t desired = observed | bit;
        if (state_.compare_exchange_weak(
                observed, desired, std::memory_order_acq_rel, std::memory_order_acquire)) {
            return true;
        }
    }
}

SceneRetirementMask SceneRetirementMailbox::consume(SceneGeneration expectedGeneration) noexcept {
    if (!can_represent(expectedGeneration)) return 0U;

    std::uint64_t observed = state_.load(std::memory_order_acquire);
    for (;;) {
        if (unpack_generation(observed) != expectedGeneration) return 0U;

        const SceneRetirementMask retirements = unpack_retirements(observed);
        if (retirements == 0U) return 0U;

        const std::uint64_t desired = pack(expectedGeneration);
        if (state_.compare_exchange_weak(
                observed, desired, std::memory_order_acq_rel, std::memory_order_acquire)) {
            return retirements;
        }
    }
}

SceneRetirementMask
SceneRetirementMailbox::pending(SceneGeneration expectedGeneration) const noexcept {
    if (!can_represent(expectedGeneration)) return 0U;

    const std::uint64_t observed = state_.load(std::memory_order_acquire);
    if (unpack_generation(observed) != expectedGeneration) return 0U;
    return unpack_retirements(observed);
}

} // namespace sunrise::state::activity::omega
