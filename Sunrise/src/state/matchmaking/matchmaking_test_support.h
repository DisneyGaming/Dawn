#pragma once

#if defined(SUNRISE_ACTIVITY_RETIREMENT_TESTS)

#include <cstddef>

#include "definition.h"

namespace sunrise::state::matchmaking::test_support {

/** Test-only accounting for deterministic release-context failure injection. */
struct ReleaseSnapshot final {
    ContextHandle last{};
    std::size_t attempts{};
    std::size_t injectedFailures{};
    std::size_t releases{};
};

/** Makes the next count real release_context calls fail before mutating State. */
void fail_next_releases(std::size_t count) noexcept;

/** Copies the release injection counters. */
[[nodiscard]] ReleaseSnapshot snapshot() noexcept;

/** Clears only test injection state. Callers first drain all real contexts. */
void reset() noexcept;

} // namespace sunrise::state::matchmaking::test_support

#endif
