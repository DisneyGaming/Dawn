#include "completion_reward.h"

#include "../../persistence/persistence.h"

namespace sunrise::state::activity::nightfall::rewards {

bool offer(std::uint64_t accountSoid,
           std::uint64_t characterSoid,
           std::uint64_t session,
           std::uint64_t run,
           std::uint32_t missionHash,
           strikes::Difficulty difficulty,
           std::int64_t updatedUtc) noexcept {
    const std::int32_t quantity = amount(difficulty);
    if (!accountSoid || !characterSoid || !session || !run || !missionHash || quantity <= 0) {
        return false;
    }
    persistence::RewardDebt debt{};
    return persistence::offer_reward(accountSoid, characterSoid, session, run, missionHash,
                                     kGlimmerDefinitionHash, quantity, updatedUtc, debt);
}

bool claim(std::uint64_t session, std::uint64_t accountSoid, Ticket& ticket) noexcept {
    ticket = {};
    if (!session || !accountSoid) return false;
    const std::lock_guard guard(detail::mutex);
    persistence::RewardDebt debt{};
    bool found = false;
    if (!persistence::load_pending_reward(accountSoid, found, debt) || !found) return false;
    std::size_t selected = detail::entries.size();
    for (std::size_t index = 0; index < detail::entries.size(); ++index) {
        const auto& entry = detail::entries[index];
        if (entry.status == detail::Status::claimed && entry.debtId == debt.debtId) return false;
        if (entry.status == detail::Status::pending && entry.debtId == debt.debtId) {
            selected = index;
        }
    }
    std::uint64_t oldest = UINT64_MAX;
    if (selected == detail::entries.size()) {
        for (std::size_t index = 0; index < detail::entries.size(); ++index) {
            const auto& entry = detail::entries[index];
            if (entry.status == detail::Status::empty) { selected = index; break; }
            if (entry.status == detail::Status::delivered && entry.sequence < oldest) {
                selected = index; oldest = entry.sequence;
            }
        }
    }
    if (selected == detail::entries.size()) return false;
    const std::uint64_t nonce = detail::nextNonce++;
    if (!detail::nextNonce) detail::nextNonce = 1;
    detail::entries[selected] = {debt.debtId, debt.accountSoid, debt.characterSoid, session,
                                 debt.runId, nonce, detail::nextSequence++, debt.definitionHash,
                                 debt.quantity, 0, detail::Status::claimed};
    if (!detail::nextSequence) detail::nextSequence = 1;
    ticket = {debt.debtId, debt.accountSoid, debt.characterSoid, session, debt.runId, nonce,
              debt.definitionHash, debt.quantity};
    return true;
}

bool finish_durable(Ticket ticket, std::int32_t credited) noexcept {
    return ticket.debtId != 0 && persistence::finish_reward(ticket.debtId, credited)
        && finish(ticket, credited);
}

} // namespace sunrise::state::activity::nightfall::rewards
