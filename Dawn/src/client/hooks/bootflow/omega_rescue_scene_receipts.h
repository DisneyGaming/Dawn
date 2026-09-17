#pragma once

namespace dawn::client::hooks::bootflow {
[[nodiscard]] bool install_omega_rescue_scene_receipts() noexcept;
void quiesce_omega_rescue_scene_receipts() noexcept;
[[nodiscard]] bool uninstall_omega_rescue_scene_receipts() noexcept;
}
