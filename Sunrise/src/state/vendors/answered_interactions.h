#pragma once

#include <cstddef>
#include <cstdint>

namespace sunrise::state::vendors {

/**
 * The interactions a player has answered, per vendor, for this session.
 *
 * The client-side producer is intentionally outside this Dawn port. The state API remains
 * available for the server's commit-side bookkeeping without adding a vendor hook or activation
 * path. Append-only for the session; slots are atomic because the two sides may run on different
 * threads.
 */

/** Vendors tracked, which matches the installed vendor index. */
inline constexpr std::size_t kVendorCapacity = 512;

/** Interactions that can be held answered at once, across every vendor. */
inline constexpr std::size_t kAnsweredCapacity = 256;

/** Value of a vendor or interaction slot that names nothing. */
inline constexpr std::uint16_t kAbsentIndex = 0xFFFFU;

/**
 * Records the interaction one vendor is showing right now.
 * @param vendorIndex Vendor row of the installed index.
 * @param interactionIndex Interaction the picker has selected, or `kAbsentIndex` while it has none.
 */
void record_shown(std::uint16_t vendorIndex, std::uint16_t interactionIndex) noexcept;

/**
 * @param vendorIndex Vendor row.
 * @param interactionIndex Interaction row.
 * @return True when that interaction has been answered this session.
 */
[[nodiscard]] bool is_answered(std::uint16_t vendorIndex, std::uint16_t interactionIndex) noexcept;

/**
 * Marks the interaction one vendor is showing right now as answered.
 *
 * Called once a quest grant has committed, which is the point the shipped game appends its own
 * entry. Nothing is written for a vendor that is showing no interaction.
 *
 * @param vendorIndex Vendor whose shown interaction was answered.
 * @return True when an interaction was showing and is now answered.
 */
bool answer_shown(std::uint16_t vendorIndex) noexcept;

/** Forgets every answer and every shown interaction. */
void clear() noexcept;

} // namespace sunrise::state::vendors
