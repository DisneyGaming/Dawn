#pragma once

#include "definition.h"

namespace dawn::state::unlocks {

/** Durable ownership scopes exposed to verified progression writers. */
enum class Scope : std::uint8_t { account, profile, character, characterObject };

/**
 * Publishes the immutable unlock policy for this process.
 * @param table Complete authored policy.
 */
void publish(const Table& table) noexcept;

/** Publishes fully scoped durable unlock state. */
void publish(const ScopedTable& table) noexcept;

/** @return The active unlock policy, or an empty policy when none was published. */
[[nodiscard]] ScopedTable snapshot() noexcept;

/** Copies one exact character's banks. */
[[nodiscard]] bool character_snapshot(std::uint64_t characterSoid,
                                      CharacterTable& output) noexcept;

/** Persists and publishes one validated flag mutation. */
[[nodiscard]] bool set_flag(Scope scope,
                            std::uint64_t ownerSoid,
                            std::uint32_t slot,
                            std::uint8_t value) noexcept;

/** Persists and publishes one validated objective mutation. */
[[nodiscard]] bool set_objective(Scope scope,
                                 std::uint64_t ownerSoid,
                                 std::uint32_t slot,
                                 std::int32_t value) noexcept;

/** Persists and publishes one validated progression-lane mutation. */
[[nodiscard]] bool set_progression(Scope scope,
                                   std::uint64_t ownerSoid,
                                   std::uint32_t definitionIndex,
                                   std::uint8_t lane,
                                   std::int32_t value) noexcept;

/** Rebinds cached account and character owners after their durable SOIDs migrate. */
[[nodiscard]] bool rebind_account(const AccountState& before,
                                  const AccountState& after) noexcept;

/** Restores the empty unlock policy. */
void clear() noexcept;

} // namespace dawn::state::unlocks
