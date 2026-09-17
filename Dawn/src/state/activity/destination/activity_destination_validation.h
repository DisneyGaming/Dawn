#pragma once

#include "definition.h"

namespace dawn::state::activity::destination {

/** Bounds one destination selection. @return True when the name length fits the fixed array. */
[[nodiscard]] bool valid(const DestinationSelection& selection) noexcept;

} // namespace dawn::state::activity::destination
