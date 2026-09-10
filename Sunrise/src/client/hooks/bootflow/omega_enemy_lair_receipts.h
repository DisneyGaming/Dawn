#pragma once

namespace sunrise::client::hooks::bootflow {
/** Observes successful native A0D510 creation and typed health death at C72390.
 * Admission/death delivery uses the encounter's guaranteed state lock; optional
 * diagnostic logging cannot suppress it. Native return values remain intact.
 * Exact event, native health death bit and original encounter identity qualify
 * deaths. The event's enum and instigator do not limit accepted causes. */
[[nodiscard]] bool install_omega_enemy_lair_receipts() noexcept;
void quiesce_omega_enemy_lair_receipts() noexcept;
void poll_native_population_admissions() noexcept;
[[nodiscard]] bool uninstall_omega_enemy_lair_receipts() noexcept;
} // namespace sunrise::client::hooks::bootflow
