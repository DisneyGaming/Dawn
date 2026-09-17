#pragma once

namespace dawn::client::hooks::retail_log {

/** Installs the game retail-log capture hook when its targets and settings allow it. */
[[nodiscard]] bool install() noexcept;

/** Stops new Dawn-owned retail-log work while preserving native forwarding. */
void quiesce() noexcept;

/** Removes the retail-log capture hook, retaining its owner state unless detach is confirmed. */
[[nodiscard]] bool uninstall() noexcept;

/** @return True while the retail-log capture hook is attached. */
[[nodiscard]] bool is_installed() noexcept;

} // namespace dawn::client::hooks::retail_log
