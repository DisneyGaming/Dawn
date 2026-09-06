#pragma once

#include <cstdint>

namespace sunrise::client::hooks::bootflow::omega_enemy_native_reference {

/** Native handle tables store a masked relative correction, not an absolute
 * unsigned distance. Original 30CAB0 subtracts it modulo 64 bits; negative
 * corrections legitimately resolve an object above its table entry. */
[[nodiscard]] constexpr std::uint64_t corrected_base(std::uint64_t element,
    std::uint64_t correction,std::int32_t mask) noexcept {
    return element-(correction&static_cast<std::uint64_t>(static_cast<std::int64_t>(mask)));
}

} // namespace sunrise::client::hooks::bootflow::omega_enemy_native_reference
