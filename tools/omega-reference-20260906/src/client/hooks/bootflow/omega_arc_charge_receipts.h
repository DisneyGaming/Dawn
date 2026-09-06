#pragma once

#include <cstdint>

namespace sunrise::client::hooks::bootflow {

/** Called after the original native type-4 authority apply. Binds the exact
 * activity source to its native created world entity; performs no game writes. */
void observe_omega_arc_charge_carrier(void* component, std::uint64_t run) noexcept;
[[nodiscard]] bool install_omega_arc_charge_receipts() noexcept;
void quiesce_omega_arc_charge_receipts() noexcept;
[[nodiscard]] bool uninstall_omega_arc_charge_receipts() noexcept;

} // namespace sunrise::client::hooks::bootflow
