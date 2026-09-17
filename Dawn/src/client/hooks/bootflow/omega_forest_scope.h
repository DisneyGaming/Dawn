#pragma once

#include <cstdint>
#include <string_view>

namespace dawn::client::hooks::bootflow::omega_forest {

/** Legacy Omega interventions require a committed current activity, never a configured default. */
[[nodiscard]] constexpr bool legacy_mutation_allowed(bool worldActive, bool joined,
                                                     std::string_view package) noexcept {
    return worldActive && joined && package == "mission_scot";
}

/** Preserve the complete native verdict outside Omega's existing forest-only fallback. */
[[nodiscard]] constexpr std::uint64_t legacy_seed_verdict(bool scoped,
                                                         std::uint64_t nativeVerdict,
                                                         std::uint32_t pendingBubbles) noexcept {
    constexpr std::uint32_t forestBubbles = 0x7F00U;
    return scoped && static_cast<std::uint8_t>(nativeVerdict) == 0U
           && pendingBubbles != 0U && (pendingBubbles & ~forestBubbles) == 0U
               ? 1U : nativeVerdict;
}

} // namespace dawn::client::hooks::bootflow::omega_forest
