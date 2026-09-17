#pragma once

#include <atomic>
#include <cstdint>

#include "state/activity/lifecycle_generation.h"

namespace dawn::state::activity::omega {

/**
 * The mailbox deliberately assigns no retail meaning to these three observation slots. Native
 * adapters may map recovered cast identities to slots, but the pure mailbox only transports bits.
 */
enum class SceneRetirement : std::uint8_t {
    observation0 = 1U << 0U,
    observation1 = 1U << 1U,
    observation2 = 1U << 2U,
};

using SceneRetirementMask = std::uint8_t;

[[nodiscard]] constexpr SceneRetirementMask retirement_mask(SceneRetirement retirement) noexcept {
    return static_cast<SceneRetirementMask>(retirement);
}

/**
 * A bounded, generation-tagged, lock-free mailbox for native Scene-retirement observations.
 *
 * One atomic word contains a 61-bit Scene generation and three retirement bits. Zero remains the
 * absent generation. Every operation that can observe or mutate retirement bits requires the exact
 * SceneGeneration captured by the caller when the cast was created. Advancing to a newer generation
 * clears predecessor bits in the same compare/exchange, so an old retirement can never be relabeled
 * as belonging to its successor.
 */
class SceneRetirementMailbox final {
public:
    static constexpr unsigned kRetirementBitCount = 3U;
    static constexpr SceneRetirementMask kAllRetirements =
        retirement_mask(SceneRetirement::observation0)
        | retirement_mask(SceneRetirement::observation1)
        | retirement_mask(SceneRetirement::observation2);
    static constexpr std::uint64_t kMaximumMailboxGeneration =
        (std::uint64_t{1} << (64U - kRetirementBitCount)) - 1U;

    SceneRetirementMailbox() noexcept = default;
    explicit SceneRetirementMailbox(SceneGeneration initialGeneration) noexcept;

    SceneRetirementMailbox(const SceneRetirementMailbox&) = delete;
    SceneRetirementMailbox& operator=(const SceneRetirementMailbox&) = delete;
    SceneRetirementMailbox(SceneRetirementMailbox&&) = delete;
    SceneRetirementMailbox& operator=(SceneRetirementMailbox&&) = delete;

    /** Returns false for zero or for a generation that cannot fit in the packed mailbox word. */
    [[nodiscard]] static constexpr bool can_represent(SceneGeneration generation) noexcept {
        return static_cast<bool>(generation) && generation.value <= kMaximumMailboxGeneration;
    }

    /**
     * Replaces one exact current generation with a strictly newer generation and clears all bits.
     * Absent, stale, backward, reused, and unrepresentable generations fail without mutation.
     */
    [[nodiscard]] bool advance_to(SceneGeneration expectedGeneration,
                                  SceneGeneration nextGeneration) noexcept;

    /**
     * Adds one observation bit only if expectedGeneration is exact and current.
     * Returns true only when a new bit was added; duplicates and mismatches fail closed.
     */
    [[nodiscard]] bool schedule(SceneGeneration expectedGeneration,
                                SceneRetirement retirement) noexcept;

    /**
     * Clears and returns all bits only if expectedGeneration is exact and current.
     * Returns zero for an empty mailbox and for every absent, stale, or invalid generation.
     */
    [[nodiscard]] SceneRetirementMask consume(SceneGeneration expectedGeneration) noexcept;

    /** Returns bits without clearing them, but only for the exact current generation. */
    [[nodiscard]] SceneRetirementMask pending(SceneGeneration expectedGeneration) const noexcept;

    [[nodiscard]] bool is_lock_free() const noexcept {
        return state_.is_lock_free();
    }

private:
    static constexpr std::uint64_t kRetirementMask = static_cast<std::uint64_t>(kAllRetirements);

    [[nodiscard]] static constexpr std::uint64_t
    pack(SceneGeneration generation, SceneRetirementMask retirements = 0U) noexcept {
        if (!can_represent(generation)) return 0U;
        return (generation.value << kRetirementBitCount)
               | (static_cast<std::uint64_t>(retirements) & kRetirementMask);
    }

    [[nodiscard]] static constexpr SceneGeneration unpack_generation(std::uint64_t state) noexcept {
        return SceneGeneration{state >> kRetirementBitCount};
    }

    [[nodiscard]] static constexpr SceneRetirementMask
    unpack_retirements(std::uint64_t state) noexcept {
        return static_cast<SceneRetirementMask>(state & kRetirementMask);
    }

    static_assert(std::atomic<std::uint64_t>::is_always_lock_free,
                  "Scene retirement hooks require lock-free 64-bit atomics");

    std::atomic<std::uint64_t> state_{};
};

} // namespace dawn::state::activity::omega
