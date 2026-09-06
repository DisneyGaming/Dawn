#pragma once

#include <cstdint>

namespace sunrise::state::activity::omega::portal_entry {
inline constexpr std::uint32_t kRegistry = 0xBA5F26EFU;
inline constexpr std::uint32_t kDefinition = 0x80F47B52U;
inline constexpr std::uint32_t kEntity = 0x80F4AE39U;
inline constexpr std::uint32_t kContactEffect = 0x80C6600CU;
/** Required by controller 8156EEC5's non-inverted 80804D83 predicate. Text name unknown. */
inline constexpr std::uint32_t kRequiredPlayerHash = 0x52B968BAU;

[[nodiscard]] constexpr bool slot(std::uint32_t registry, std::uint8_t type,
                                 std::uint16_t index) noexcept {
    return registry == kRegistry && type == 4U && index == 0U;
}

} // namespace sunrise::state::activity::omega::portal_entry
