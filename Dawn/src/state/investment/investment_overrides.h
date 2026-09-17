#pragma once

#include <cstdint>

namespace dawn::state::investment {

/**
 * Runtime edits to the family-5 override lists.
 *
 * The lists start as the authored `family5_flag_overrides` and `family5_value_overrides` from
 * settings.json and are sent whole in every ws-205 and ws-503 response. Slots are unique in each
 * list: setting a slot that is already listed replaces its value.
 */

/**
 * Sets or replaces one flag override.
 * @param slot Unlock flag slot.
 * @param value Logical flag value; 2 is active.
 * @return False when the slot was absent and the list is full.
 */
[[nodiscard]] bool set_flag_override(std::uint16_t slot, std::uint8_t value) noexcept;

/** Removes one flag override. Nothing happens when the slot has none. */
void clear_flag_override(std::uint16_t slot) noexcept;

/**
 * Sets or replaces one value override.
 * @param slot Unlock value slot.
 * @param value Signed value the client reads for the slot.
 * @return False when the slot was absent and the list is full.
 */
[[nodiscard]] bool set_value_override(std::uint16_t slot, std::int32_t value) noexcept;

/** Removes one value override. Nothing happens when the slot has none. */
void clear_value_override(std::uint16_t slot) noexcept;

/** Requests an inert client refetch notification for a future native integration. */
void request_client_refetch() noexcept;

/** @return True once per request; unused unless a native client adapter consumes it. */
[[nodiscard]] bool consume_client_refetch() noexcept;

} // namespace dawn::state::investment
