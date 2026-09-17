#pragma once

#include "runtime.h"
#include <limits>

namespace dawn::state::runtime::detail {

/** Adds stack rewards to a private transaction image. Failure leaves that image untouched.
 * Resolve must validate a non-instanced profile definition and return its installed stack cap.
 * Caps saturate; absent stacks are appended. Positive grants get distinct observer serials.
 */
template <typename Resolve>
[[nodiscard]] bool stage_profile_rewards(AccountState& image,
                                         std::span<const ProfileExchangePayout> payouts,
                                         std::int32_t serialFloor, Resolve&& resolve) noexcept {
    if (payouts.empty() || payouts.size() > kProfileStackChangeCapacity ||
        image.profileItemCount > image.profileItems.size())
        return false;
    AccountState candidate = image;
    std::int32_t serial = serialFloor;
    for (std::size_t i = 0; i < candidate.profileItemCount; ++i) {
        serial = (std::max)(serial, candidate.profileItems[i].mutationSerial);
    }
    for (std::size_t p = 0; p < payouts.size(); ++p) {
        const auto& payout = payouts[p];
        std::int32_t maximum{};
        if (!payout.definitionHash || payout.quantity <= 0 ||
            !resolve(payout.definitionHash, maximum) || maximum <= 0)
            return false;
        for (std::size_t prior = 0; prior < p; ++prior) {
            if (payouts[prior].definitionHash == payout.definitionHash)
                return false;
        }
        std::size_t index = candidate.profileItemCount;
        for (std::size_t i = 0; i < candidate.profileItemCount; ++i) {
            const auto& item = candidate.profileItems[i];
            if (item.definitionHash != payout.definitionHash)
                continue;
            if (index != candidate.profileItemCount || item.instanceSoid != 0 || item.quantity < 0)
                return false;
            index = i;
        }
        const bool append = index == candidate.profileItemCount;
        const auto previous = append ? 0 : candidate.profileItems[index].quantity;
        const auto credited = profile_currency_credit(previous, maximum, payout.quantity);
        if (credited == 0)
            continue;
        if (serial == (std::numeric_limits<std::int32_t>::max)() ||
            (append && index == candidate.profileItems.size()))
            return false;
        candidate.profileItems[index] = {0, payout.definitionHash, previous + credited, ++serial};
        if (append)
            ++candidate.profileItemCount;
    }
    image = candidate;
    return true;
}

} // namespace dawn::state::runtime::detail
