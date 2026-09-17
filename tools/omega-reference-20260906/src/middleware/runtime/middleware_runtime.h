#pragma once

namespace dawn::middleware {

/** Initializes protocol middleware state. */
[[nodiscard]] bool initialize() noexcept;

/** Clears protocol middleware state. */
void shutdown() noexcept;

} // namespace dawn::middleware
