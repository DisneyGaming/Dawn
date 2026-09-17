#pragma once

#if defined(DAWN_ACTIVITY_RETIREMENT_TESTS)

#include <array>
#include <cstddef>
#include <cstdint>

#include "group_host_sessions.h"

namespace dawn::server::gameplay::group::test_support {

/** Real group-table boundaries at which a deterministic test may pause. */
enum class Point : std::uint8_t {
    sourcePrecheckComplete,
    allocationCommittedBeforeStore,
};

using Hook = void (*)(Point point,
                      state::activity::ActivityInstanceKey source,
                      state::activity::ActivityInstanceKey allocated) noexcept;

/** Exact owner/fence accounting copied under the real group lock. */
struct Snapshot final {
    std::array<state::activity::ActivityInstanceKey, kHostRecordRetirementCapacity> uniqueOwners{};
    std::size_t uniqueOwnerCount{};
    std::size_t occupiedRows{};
    std::size_t currentOwners{};
    std::size_t unfilledRows{};
    std::size_t pinnedRows{};
    std::size_t pendingOwners{};
    std::size_t fenceCount{};
};

void set_hook(Hook hook) noexcept;
[[nodiscard]] Snapshot snapshot() noexcept;

/** Corruption/capacity fixture only; lifecycle operations still use the real table/coordinator. */
[[nodiscard]] bool seed_current(std::uint64_t groupSessionId,
                                state::activity::ActivityInstanceKey host,
                                state::activity::ActivityInstanceKey source,
                                std::uint32_t pins = 0) noexcept;

/** Corruption/capacity fixture only; pending drain remains the production implementation. */
[[nodiscard]] bool seed_pending(state::activity::ActivityInstanceKey host,
                                state::activity::ActivityInstanceKey source) noexcept;

/** Clears only test table storage. Tests must prove all real owners absent first. */
void reset_storage() noexcept;

} // namespace dawn::server::gameplay::group::test_support

#endif
