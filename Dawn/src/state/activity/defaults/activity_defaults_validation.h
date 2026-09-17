#pragma once

#include "definition.h"

namespace dawn::state::activity::defaults {

/**
 * Checks one coherent default destination and its small numeric fallback policy.
 * @param candidate Destination and launch scalars to check.
 * @return True when selection ranges and bubble-to-slice ordering agree.
 */
[[nodiscard]] bool valid(const DefaultDestination& candidate) noexcept;

/**
 * Checks all immutable activity defaults supplied to State startup.
 * @return True when every default inside is coherent.
 */
[[nodiscard]] bool valid(const ActivityDefaults& candidate) noexcept;

/** @return Dawn's whole local fallback, used when service 6 leaves out its selection. */
[[nodiscard]] ActivityDefaults authored() noexcept;

} // namespace dawn::state::activity::defaults
