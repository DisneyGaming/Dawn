#pragma once
#include <cstdint>

namespace dawn::server::bap::encrypted::forest_chest_rewards {
// Owner-specified reward schedule; completed branches, capped after branch seven.
struct Payout final { std::int32_t coins{}, candy{}; };
[[nodiscard]] constexpr Payout payout(std::uint64_t completedBranches) noexcept {
    const auto branches=static_cast<std::int32_t>(completedBranches>7U?7U:completedBranches);
    return {branches,500*branches};
}
} // namespace dawn::server::bap::encrypted::forest_chest_rewards
