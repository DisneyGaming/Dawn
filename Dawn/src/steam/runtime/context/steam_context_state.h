#pragma once

namespace dawn::steam::context {

/** Invalidates every caller-owned Steam context table. */
void advance_generation() noexcept;

} // namespace dawn::steam::context
