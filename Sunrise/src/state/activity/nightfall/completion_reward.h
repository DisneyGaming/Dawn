#pragma once

#include "../strike_variants.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>

namespace sunrise::state::activity::nightfall::rewards {

/** Installed profile-currency definition used by Dawn's explicit completion bonus. */
inline constexpr std::uint32_t kGlimmerDefinitionHash = 0xBC53E66EU;

/** @return Dawn-authored Glimmer bonus for one completed difficulty. */
[[nodiscard]] constexpr std::int32_t amount(strikes::Difficulty difficulty) noexcept {
    switch (difficulty) {
    case strikes::Difficulty::adept: return 1000;
    case strikes::Difficulty::master: return 2500;
    case strikes::Difficulty::grandmaster: return 5000;
    default: return 0;
    }
}

/** @return True when this authenticated Family-4 root owns the current account. */
[[nodiscard]] constexpr bool account_matches(std::uint64_t accountSoid,
                                             std::uint64_t family4RootSoid) noexcept {
    return accountSoid != 0 && accountSoid == family4RootSoid;
}

/** Exact claim on one queued completion. */
struct Ticket {
    std::uint64_t session{};
    std::uint64_t run{};
    std::uint64_t nonce{};
    std::uint32_t definitionHash{};
    std::int32_t quantity{};
    explicit constexpr operator bool() const noexcept {
        return session != 0 && run != 0 && nonce != 0 && definitionHash != 0 && quantity > 0;
    }
};

namespace detail {
enum class Status : std::uint8_t { empty, pending, claimed, delivered };
struct Entry {
    std::uint64_t session{}, run{}, nonce{}, sequence{};
    std::uint32_t definitionHash{};
    std::int32_t quantity{}, credited{};
    Status status{};
};
inline constexpr std::size_t kCapacity = 16;
inline std::mutex mutex;
inline std::array<Entry, kCapacity> entries{};
inline std::uint64_t nextNonce{1}, nextSequence{1};
}

/** Clears process-local reward debt when the owning State lifetime ends. */
inline void clear() noexcept {
    const std::lock_guard guard(detail::mutex);
    detail::entries = {};
    detail::nextNonce = detail::nextSequence = 1;
}

/** Queues one immutable reward debt after an authenticated terminal accepts completion. */
inline bool offer(std::uint64_t session,
                  std::uint64_t run,
                  strikes::Difficulty difficulty) noexcept {
    const std::int32_t quantity = amount(difficulty);
    if (!session || !run || quantity <= 0) return false;
    const std::lock_guard guard(detail::mutex);
    for (const auto& entry : detail::entries)
        if (entry.status != detail::Status::empty && entry.session == session && entry.run == run)
            return true;
    std::size_t selected = detail::entries.size();
    std::uint64_t oldest = UINT64_MAX;
    for (std::size_t index = 0; index < detail::entries.size(); ++index) {
        const auto& entry = detail::entries[index];
        if (entry.status == detail::Status::empty) { selected = index; break; }
        if (entry.status == detail::Status::delivered && entry.sequence < oldest) {
            selected = index; oldest = entry.sequence;
        }
    }
    if (selected == detail::entries.size()) return false;
    detail::entries[selected] = {session, run, 0, detail::nextSequence++, kGlimmerDefinitionHash,
                                 quantity, 0, detail::Status::pending};
    if (!detail::nextSequence) detail::nextSequence = 1;
    return true;
}

/** Claims one exact pending debt for the authenticated destination session. */
inline bool claim(std::uint64_t session, Ticket& ticket) noexcept {
    ticket = {};
    if (!session) return false;
    const std::lock_guard guard(detail::mutex);
    for (auto& entry : detail::entries) {
        if (entry.status != detail::Status::pending || entry.session != session)
            continue;
        entry.nonce = detail::nextNonce++;
        if (!detail::nextNonce) detail::nextNonce = 1;
        entry.status = detail::Status::claimed;
        ticket = {entry.session, entry.run, entry.nonce, entry.definitionHash, entry.quantity};
        return true;
    }
    return false;
}

/** Makes a failed synchronous delivery attempt retryable. */
inline void release(Ticket ticket) noexcept {
    if (!ticket) return;
    const std::lock_guard guard(detail::mutex);
    for (auto& entry : detail::entries)
        if (entry.status == detail::Status::claimed && entry.session == ticket.session
            && entry.run == ticket.run && entry.nonce == ticket.nonce
            && entry.definitionHash == ticket.definitionHash && entry.quantity == ticket.quantity) {
            entry.status = detail::Status::pending; entry.nonce = 0; return;
        }
}

/** Records the terminal delivery only after the account commit and staged output both succeed. */
inline bool finish(Ticket ticket, std::int32_t credited) noexcept {
    if (!ticket || credited < 0 || credited > ticket.quantity) return false;
    const std::lock_guard guard(detail::mutex);
    for (auto& entry : detail::entries)
        if (entry.status == detail::Status::claimed && entry.session == ticket.session
            && entry.run == ticket.run && entry.nonce == ticket.nonce
            && entry.definitionHash == ticket.definitionHash && entry.quantity == ticket.quantity
            && credited <= entry.quantity) {
            entry.status = detail::Status::delivered; entry.credited = credited; return true;
        }
    return false;
}

} // namespace sunrise::state::activity::nightfall::rewards
