#pragma once

#include <cstdint>

namespace sunrise::state::activity::omega_portal_entry {

/** Authored 8156EEC5 predicate 80804D83; its text name/retail assignment lifecycle are unknown. */
inline constexpr std::uint32_t kRequiredPlayerHash = 0x52B968BAU;
inline constexpr std::uint32_t kCarrierRegistry = 0xBA5F26EFU;
inline constexpr std::uint8_t kCarrierType = 4;
inline constexpr std::uint16_t kCarrierIndex = 0;

/** Only lighthouse_teleport, definition80F47B52, may use this carrier authority. */
[[nodiscard]] constexpr bool is_carrier(std::uint32_t key, std::uint8_t type,
                                       std::uint16_t index) noexcept {
    return key == kCarrierRegistry && type == kCarrierType && index == kCarrierIndex;
}

} // namespace sunrise::state::activity::omega_portal_entry
