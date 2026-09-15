#pragma once
#include <cstdint>

/**
 * Festival of the Lost candy drops for the Haunted Forest.
 *
 * Build 86657's client rolls dropped loot itself from authored reward sheets, but this install
 * ships no reward-sheet content (every sheet slot of the investment root is stripped, the Forest
 * activity carries no reward rows, and the loot sink whose roll entry is 0x534E90 is never even
 * dispatched here), so the native loot roller can never produce a Candy bauble. This hook restores
 * exactly the one thing the missing content would have produced: one pending-drop record for Candy
 * (item index 151, kind 3) per combatant death, written into the client's own pending-drop table
 * with the corpse registered as the tracked source, so the client's own bauble walker spawns the
 * retail blue triangle and the pickup reports opcode 601, which the server answers and pays.
 *
 * Two witnesses cooperate: the kill-record dispatcher 0x4AA1C0(sink, record) runs for every damage
 * record and hands over the damaged object's source reference and entity, which this hook remembers;
 * Dawn's typed-health death witness (observe_native_candidate) then calls observe_death() once per
 * death, and the remembered reference for that entity becomes the drop, at the corpse.
 *
 * Off by default (experiments.omega.forest_candy_drops). It never evicts, never merges and never
 * grants: when the table is full the death simply drops nothing. Every decision is logged as
 * ev=forest_candy.
 */
namespace sunrise::client::hooks::bootflow::forest_candy_drops {
/** Attaches the dispatcher detour. Only called when the experiment is enabled. */
[[nodiscard]] bool install() noexcept;
void quiesce() noexcept;
[[nodiscard]] bool uninstall() noexcept;
/** Per-frame on the camera thread: latches the game thread the kill path must be on. */
void poll() noexcept;
/** Called by the death witness with the dying actor's entity handle and character object address. */
void observe_death(std::uint32_t entity, std::uintptr_t characterAddress) noexcept;
} // namespace sunrise::client::hooks::bootflow::forest_candy_drops
