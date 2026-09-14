#pragma once
#include "completion_reward.h"
#include "../strike_variants.h"
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <shared_mutex>

namespace sunrise::state::activity::nightfall {
struct Options {
    bool lockedEquipment{};
    bool limitedRevives{};
    bool extinguish{};
    bool disableInfiniteAmmo{};
    std::uint8_t startingRevives{4};
    std::uint16_t reviveMinutes{45};
    std::uint8_t powerDelta{};
    friend bool operator==(const Options&, const Options&) = default;
};
enum class Outcome : std::uint8_t { none, running, completed, failed };
struct Progress {
    std::uint64_t session{}, run{}, startedAt{}, finishedAt{};
    std::uint32_t kills{}, champions{}, deaths{}, score{};
    std::uint8_t revives{};
    Outcome outcome{};
    bool playerObserved{}, playerDead{}, revivesExpired{};
};
[[nodiscard]] constexpr Options defaults(strikes::Difficulty difficulty) noexcept {
    Options result{};
    if (difficulty == strikes::Difficulty::master) result.lockedEquipment = true;
    if (difficulty == strikes::Difficulty::grandmaster) {
        result.lockedEquipment = result.limitedRevives = result.extinguish = result.disableInfiniteAmmo = true;
        result.powerDelta = 30;
    }
    return result;
}
[[nodiscard]] constexpr Options sanitize(strikes::Difficulty difficulty, Options value) noexcept {
    if (difficulty == strikes::Difficulty::standard) return {};
    value.startingRevives = (std::min)(value.startingRevives, std::uint8_t{4});
    value.reviveMinutes = std::clamp(value.reviveMinutes, std::uint16_t{5}, std::uint16_t{45});
    if (difficulty == strikes::Difficulty::grandmaster) {
        value.lockedEquipment = value.limitedRevives = value.extinguish = value.disableInfiniteAmmo = true;
        value.powerDelta = std::clamp(value.powerDelta, std::uint8_t{30}, std::uint8_t{50});
    } else value.powerDelta = 0;
    return value;
}
namespace detail {
inline std::shared_mutex mutex;
inline Options selected{};
inline std::int16_t activity{-1};
inline std::uint64_t session{};
inline bool armed{};
inline Progress progress{};
// Fast readers on weapon hooks never wait for an account transaction.
inline std::atomic_uint32_t flags{};
inline std::atomic_uint64_t configuration{}, powerRevision{};
inline unsigned lastPowerCap{};
inline void publish() noexcept {
    const auto* variant = strikes::find(activity);
    const bool gm = variant && variant->difficulty == strikes::Difficulty::grandmaster;
    const auto& o = selected;
    const std::uint64_t packed = armed
        ? (o.lockedEquipment ? 1ULL : 0ULL) | (o.limitedRevives ? 2ULL : 0ULL)
            | (o.extinguish ? 4ULL : 0ULL) | (o.disableInfiniteAmmo ? 8ULL : 0ULL) | 16ULL
            | (std::uint64_t{o.startingRevives} << 8) | (std::uint64_t{o.reviveMinutes} << 16)
            | (std::uint64_t{o.powerDelta} << 32) | (static_cast<std::uint64_t>(activity + 1) << 40)
        : 0;
    const unsigned cap = armed && gm ? 1100U - selected.powerDelta : 0U;
    const bool powerChanged = cap != lastPowerCap;
    // A changing cap makes the revision odd before any reader-visible configuration changes.
    // The matching increment below publishes an even revision only after both atomics are ready.
    if (powerChanged) powerRevision.fetch_add(1, std::memory_order_acq_rel);
    configuration.store(packed, std::memory_order_release);
    flags.store(armed ? 1U | (selected.lockedEquipment ? 2U : 0U)
        | (selected.disableInfiniteAmmo ? 4U : 0U) | (gm ? 8U : 0U) : 0U, std::memory_order_release);
    if (powerChanged) {
        lastPowerCap = cap;
        powerRevision.fetch_add(1, std::memory_order_release);
    }
}
}
[[nodiscard]] inline bool active() noexcept { return (detail::flags.load(std::memory_order_acquire) & 1U) != 0; }
[[nodiscard]] inline bool equipment_locked() noexcept { return (detail::flags.load(std::memory_order_acquire) & 2U) != 0; }
[[nodiscard]] inline bool infinite_ammo_blocked() noexcept { return (detail::flags.load(std::memory_order_acquire) & 4U) != 0; }
[[nodiscard]] inline bool movement_blocked() noexcept { return (detail::flags.load(std::memory_order_acquire) & 8U) != 0; }
struct Selection { std::int16_t activity{-1}; Options options{}; };
[[nodiscard]] inline Selection selection() noexcept {
    const auto bits = detail::configuration.load(std::memory_order_acquire);
    if ((bits & 16U) == 0) return {};
    return {static_cast<std::int16_t>((bits >> 40) - 1),
        {(bits & 1U)!=0,(bits & 2U)!=0,(bits & 4U)!=0,(bits & 8U)!=0,
         static_cast<std::uint8_t>(bits >> 8),static_cast<std::uint16_t>(bits >> 16),static_cast<std::uint8_t>(bits >> 32)}};
}
[[nodiscard]] inline Options options() noexcept { return selection().options; }
[[nodiscard]] inline std::int16_t activity_index() noexcept { return selection().activity; }
[[nodiscard]] inline std::uint64_t power_revision() noexcept { return detail::powerRevision.load(std::memory_order_acquire); }
[[nodiscard]] inline Progress progress() noexcept {
    const std::shared_lock guard(detail::mutex); return detail::progress;
}
// Called only after the strike controller has accepted an exact native death
// receipt. Rejected and duplicate receipts never reach this boundary.
inline void enemy_defeated(std::uint64_t run, bool champion = false, bool boss = false) noexcept {
    const std::unique_lock guard(detail::mutex);
    auto& p = detail::progress;
    if (!detail::armed || p.run != run || p.outcome != Outcome::running) return;
    ++p.kills;
    p.score += boss ? 5000U : champion ? 1000U : 100U;
    if (champion) {
        ++p.champions;
        if (!p.revivesExpired && p.revives < 99) ++p.revives;
    }
}
inline void complete(std::uint64_t run, std::uint64_t now) noexcept {
    const std::unique_lock guard(detail::mutex);
    auto& p = detail::progress;
    if (!detail::armed || p.run != run || p.outcome == Outcome::failed) return;
    const auto* variant = strikes::find(detail::activity);
    if (p.outcome == Outcome::completed) {
        if (variant) (void)rewards::offer(p.session, p.run, variant->difficulty);
        return;
    }
    if (p.outcome != Outcome::running) return;
    p.outcome = Outcome::completed; p.finishedAt = now;
    if (variant) (void)rewards::offer(p.session, p.run, variant->difficulty);
}
/**
 * Accepts a production completion only after its reward debt and mission completion are durable.
 * This overload keeps the test-only, memory mailbox path above independent of SQLite.
 */
[[nodiscard]] inline bool complete(std::uint64_t run,
                                   std::uint64_t now,
                                   std::uint64_t accountSoid,
                                   std::uint64_t characterSoid,
                                   std::uint32_t missionHash,
                                   std::int64_t updatedUtc) noexcept {
    const std::unique_lock guard(detail::mutex);
    auto& p = detail::progress;
    if (!detail::armed || p.run != run || p.outcome == Outcome::failed) return false;
    const auto* variant = strikes::find(detail::activity);
    if (!variant || variant->difficulty == strikes::Difficulty::standard) return false;
    if (p.outcome != Outcome::running && p.outcome != Outcome::completed) return false;
    if (!rewards::offer(accountSoid, characterSoid, p.session, p.run, missionHash,
                        variant->difficulty, updatedUtc)) return false;
    if (p.outcome == Outcome::running) {
        p.outcome = Outcome::completed;
        p.finishedAt = now;
    }
    return true;
}
// The current host publishes exactly one participant per activity membership.
// A qualified death of that participant is therefore a complete fireteam wipe.
// Missing/retired components are never submitted as death observations.
inline void observe_local_player(std::uint64_t session, std::uint64_t run, bool dead, std::uint64_t now) noexcept {
    const std::unique_lock guard(detail::mutex);
    auto& p = detail::progress;
    if (!detail::armed || !session || p.session != session || p.run != run || p.outcome != Outcome::running) return;
    if (!p.playerObserved) {
        if (!dead) { p.playerObserved = true; p.startedAt = now; }
        return;
    }
    const auto& o = detail::selected;
    if (o.limitedRevives && now >= p.startedAt && now - p.startedAt >= std::uint64_t{o.reviveMinutes} * 60000) {
        p.revives = 0; p.revivesExpired = true;
    }
    if (p.playerDead == dead) return;
    p.playerDead = dead;
    if (!dead) return;
    ++p.deaths;
    p.score = p.score > 500U ? p.score - 500U : 0U;
    if (o.extinguish || (o.limitedRevives && p.revives == 0)) {
        p.outcome = Outcome::failed; p.finishedAt = now;
    } else if (o.limitedRevives) --p.revives;
}
[[nodiscard]] inline bool failed(std::uint64_t run) noexcept {
    const std::shared_lock guard(detail::mutex);
    return detail::armed && detail::progress.run == run && detail::progress.outcome == Outcome::failed;
}
// This lock spans the account commit. Launch waits for an in-progress equipment
// transaction; a prepared transaction cannot bypass a subsequently armed lock.
class EquipmentMutation final {
public:
    EquipmentMutation() noexcept : guard_(detail::mutex), allowed_(!detail::armed || !detail::selected.lockedEquipment) {}
    [[nodiscard]] bool allowed() const noexcept { return allowed_; }
private:
    std::shared_lock<std::shared_mutex> guard_;
    bool allowed_;
};
inline void leave() noexcept {
    const std::unique_lock guard(detail::mutex);
    detail::armed = false; detail::activity = -1; detail::session = 0; detail::selected = {}; detail::publish();
}
inline void arm(std::int16_t activity, Options selected) noexcept {
    const std::unique_lock guard(detail::mutex);
    const auto* variant = strikes::find(activity);
    detail::armed = variant && variant->difficulty != strikes::Difficulty::standard;
    detail::activity = detail::armed ? activity : -1; detail::session = 0;
    detail::selected = detail::armed ? sanitize(variant->difficulty, selected) : Options{};
    detail::progress = {};
    detail::publish();
}
inline void enter(std::uint64_t session, std::int16_t activity, std::uint64_t run = 0) noexcept {
    if (!session) return;
    const std::unique_lock guard(detail::mutex);
    const auto* variant = strikes::find(activity);
    if (!variant || variant->difficulty == strikes::Difficulty::standard) {
        detail::armed = false; detail::activity = -1; detail::session = 0; detail::selected = {}; detail::publish(); return;
    }
    const bool newActivity = !detail::armed || detail::activity != activity
        || (detail::session && (run ? detail::progress.run != run : detail::session != session));
    if (newActivity) {
        detail::selected = defaults(variant->difficulty);
    }
    if (newActivity || !detail::progress.session || detail::progress.run != run) {
        detail::progress = {}; detail::progress.session = session; detail::progress.run = run;
        detail::progress.revives = detail::selected.startingRevives; detail::progress.outcome = Outcome::running;
    } else detail::progress.session = session;
    detail::armed = true; detail::activity = activity; detail::session = session; detail::publish();
}
}
