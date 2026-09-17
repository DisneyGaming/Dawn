#pragma once
#include <atomic>
#include <cstdint>
#include <mutex>

namespace dawn::state::activity::eater_of_worlds::contest {

enum class Mode : std::uint8_t { standard, contest };
// Installed public activity 536; see NATIVE-PACKAGE-MAP.md. No native identity substitution.
inline constexpr std::int16_t kActivity = 536;
inline constexpr std::uint32_t kDifficultySettings = 0x813206EEU;
inline constexpr std::int32_t kActivityPower = 750;
inline constexpr std::uint8_t kPowerDelta = 20;
inline constexpr std::int32_t kPlayerPowerCap = kActivityPower - kPowerDelta;

namespace detail {
inline std::mutex mutex;
inline std::uint64_t session{};
inline std::atomic_bool enabled{};
inline std::atomic_uint64_t powerRevision{};
// Caller owns mutex. Power readers use the revision to reject a mixed publication.
inline void publish(bool value) noexcept {
    if (enabled.load(std::memory_order_relaxed) == value) return;
    powerRevision.fetch_add(1, std::memory_order_acq_rel);
    enabled.store(value, std::memory_order_release);
    powerRevision.fetch_add(1, std::memory_order_release);
}
inline void clear() noexcept {
    session = 0;
    publish(false);
}
}

/** Pending launch power is published before the native client builds its player records. */
[[nodiscard]] inline bool enabled() noexcept { return detail::enabled.load(std::memory_order_acquire); }
[[nodiscard]] inline std::uint64_t power_revision() noexcept {
    return detail::powerRevision.load(std::memory_order_acquire);
}
inline void leave() noexcept {
    const std::lock_guard guard(detail::mutex);
    detail::clear();
}
/** Game-thread launch owner only, after native descriptor and destination validation. */
inline void arm(std::int16_t activity, Mode mode) noexcept {
    const std::lock_guard guard(detail::mutex);
    detail::session = 0;
    detail::publish(activity == kActivity && mode == Mode::contest);
}
/** Wipes in the same session retain the mode; an unrelated session cannot inherit it. */
inline void enter(std::uint64_t session, std::int16_t activity) noexcept {
    const std::lock_guard guard(detail::mutex);
    if (!enabled()) return;
    if (!session || activity != kActivity || (detail::session && detail::session != session)) {
        detail::clear();
        return;
    }
    detail::session = session;
}
} // namespace dawn::state::activity::eater_of_worlds::contest
