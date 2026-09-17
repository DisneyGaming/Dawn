#pragma once

#if defined(DAWN_ACTIVITY_RETIREMENT_TESTS)

#include <array>
#include <cstddef>
#include <cstdint>

#include "internal.h"

namespace dawn::server::bap::test_support {

/** Real coordinator boundaries used by the deterministic integration schedules. */
enum class Point : std::uint8_t {
    sourceBeginComplete,
    sourceStateRetired,
    eventLockAcquired,
};

using Hook = void (*)(Point point,
                      state::activity::ActivityInstanceKey activity,
                      state::activity::RetireResult result,
                      bool completed) noexcept;

using SessionAction = bool (*)(Session& session) noexcept;

/** Exact BAP owner/pending/quarantine accounting copied under the real coordinator lock. */
struct Snapshot final {
    std::array<state::activity::ActivityInstanceKey, state::activity::kSessionCapacity>
        pending{};
    std::size_t pendingCount{};
    std::size_t ownerLeaseCount{};
    std::size_t borrowedBindingCount{};
    std::size_t quarantinedCount{};
};

void set_hook(Hook hook) noexcept;
[[nodiscard]] Snapshot snapshot() noexcept;
[[nodiscard]] bool snapshot_session(std::uint32_t id, Session& output) noexcept;

/** Configures an already-open real session with one current binding fixture. */
[[nodiscard]] bool configure_session(
    std::uint32_t id,
    state::activity::ActivityInstanceKey activity,
    bool ownsActivity,
    state::matchmaking::ContextHandle context = {}) noexcept;

/** Runs an action under the real serialized BAP boundary. */
[[nodiscard]] bool with_session_locked(std::uint32_t id, SessionAction action) noexcept;

/** Invokes the production authentication teardown helper under the BAP lock. */
[[nodiscard]] bool reset_authentication(std::uint32_t id) noexcept;

/** Clears only test coordinator storage. Tests must prove all real owners absent first. */
void reset_storage() noexcept;

} // namespace dawn::server::bap::test_support

#endif
