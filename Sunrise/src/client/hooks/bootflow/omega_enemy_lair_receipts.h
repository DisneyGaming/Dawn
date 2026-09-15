#pragma once
#include <cstddef>
#include <cstdint>

namespace sunrise::client::hooks::bootflow {
/** Observes successful native A0D510 creation and typed health death at C72390.
 * Admission/death delivery uses the encounter's guaranteed state lock; optional
 * diagnostic logging cannot suppress it. Native return values remain intact.
 * Exact event, native health death bit and original encounter identity qualify
 * deaths. The event's enum and instigator do not limit accepted causes. */
[[nodiscard]] bool install_omega_enemy_lair_receipts() noexcept;
void quiesce_omega_enemy_lair_receipts() noexcept;
void poll_native_population_admissions() noexcept;
// The existing Omega source probe owns 4E4580. Share that boundary instead of
// attempting a second detour against bytes that the first owner has patched.
using NativePopulationDispatch=void(__fastcall*)(std::uint32_t*,std::uint32_t,const std::byte*) noexcept;
void dispatch_native_population_source(std::uint32_t* instance,std::uint32_t reason,
    const std::byte* authority,NativePopulationDispatch original) noexcept;
// Called only by the existing native source-retirement boundary, after its
// allocator TLS validation. Retains the population observer's unload gate.
void retire_strike_bond_boss(std::uintptr_t source,bool allocatorReady) noexcept;
[[nodiscard]] bool uninstall_omega_enemy_lair_receipts() noexcept;
} // namespace sunrise::client::hooks::bootflow
