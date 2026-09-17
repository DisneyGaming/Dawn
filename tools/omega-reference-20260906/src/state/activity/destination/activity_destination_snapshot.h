#pragma once

#include <cstdint>

#include "../lifecycle_generation.h"
#include "definition.h"

namespace dawn::state::activity::destination {

/**
 * Copies the destination committed with one activity session.
 * @param sessionId Nonzero id returned by a committed allocation.
 * @param output Cleared first, then gets one bounded scalar selection.
 * @return True when the bounded table still holds the session.
 */
[[nodiscard]] bool snapshot(std::uint64_t sessionId, DestinationSelection& output) noexcept;

/**
 * Copies the destination only while the exact activity incarnation remains current.
 * @param key Exact activity key captured from committed State.
 * @param output Cleared first, then gets the committed destination.
 */
[[nodiscard]] bool snapshot(ActivityInstanceKey key, DestinationSelection& output) noexcept;

} // namespace dawn::state::activity::destination
