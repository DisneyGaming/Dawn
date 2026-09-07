#pragma once
#include <cstdint>
namespace sunrise::client::hooks::teleport::identity {
// The low13 bits index a pool. The remaining salt distinguishes recycled entries.
[[nodiscard]] constexpr bool current(std::uint32_t before,std::uint32_t owner,std::uint32_t after) noexcept {
    return before!=UINT32_MAX && owner==before && after==before;
}
}
