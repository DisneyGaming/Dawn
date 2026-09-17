#pragma once
#include <cstdint>

namespace dawn::client::hooks::bootflow::forest_candy_drops::drop_slot {

/** BE2EF0 retires a freed record's bauble, then clears the handle pair at BE31D9.
 * A zero-filled initial slot has never owned a bauble. A recycled slot must wait
 * for that native teardown rather than discarding the old handle on publication. */
[[nodiscard]] constexpr bool reusable(bool zeroHead, std::uint16_t item,
                                       std::uint64_t handlePair) noexcept {
    if (zeroHead) { return handlePair == 0 || handlePair == UINT64_MAX; }
    return item == UINT16_MAX && handlePair == UINT64_MAX;
}

} // namespace dawn::client::hooks::bootflow::forest_candy_drops::drop_slot
