#pragma once

namespace dawn::server::ui::runtime {

/** @return True when the Server module owns its Core UI registry slot. */
[[nodiscard]] bool initialize() noexcept;

/** Removes the Server module from the Core UI registry. */
void shutdown() noexcept;

} // namespace dawn::server::ui::runtime
