#include "matchmaking_state.h"

#include <Windows.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

#include "../runtime/storage/internal.h"
#include "transactions/internal.h"

#if defined(DAWN_ACTIVITY_RETIREMENT_TESTS)
#include "matchmaking_test_support.h"
#endif

namespace dawn::state::matchmaking {

#if defined(DAWN_ACTIVITY_RETIREMENT_TESTS)
namespace {

std::atomic_size_t g_releaseFailuresRemaining{};
std::atomic_size_t g_releaseAttempts{};
std::atomic_size_t g_releaseInjectedFailures{};
std::atomic_size_t g_releaseSuccesses{};
std::atomic_size_t g_lastReleaseSlot{};
std::atomic_uint32_t g_lastReleaseGeneration{};

[[nodiscard]] bool consume_injected_release_failure() noexcept {
    std::size_t remaining = g_releaseFailuresRemaining.load(std::memory_order_relaxed);
    while (remaining != 0) {
        if (g_releaseFailuresRemaining.compare_exchange_weak(
                remaining, remaining - 1U, std::memory_order_relaxed)) {
            return true;
        }
    }
    return false;
}

} // namespace
#endif

/** Acquires the first available generation-checked logical context. */
bool acquire_context(ContextHandle& context) noexcept {
    context = {};
    AcquireSRWLockExclusive(&runtime::storage::g_stateLock);
    MatchmakingState& state = runtime::storage::g_state.matchmaking;
    if (state.allocatorRevision == kInvalidRevision) {
        ReleaseSRWLockExclusive(&runtime::storage::g_stateLock);
        return false;
    }
    for (std::size_t index = 0; index < state.contexts.size(); ++index) {
        ContextSlot& slot = state.contexts[index];
        if (slot.active || slot.generation == (std::numeric_limits<std::uint32_t>::max)()) {
            continue;
        }
        // Advance before erasure so a released handle cannot become valid again.
        std::uint32_t generation = slot.generation;
        ++generation;
        SecureZeroMemory(&slot, sizeof slot);
        slot.active = true;
        slot.generation = generation;
        slot.data.revision = kInitialContextRevision;
        slot.data.latestSlot = kInvalidVariantSlot;
        context = {index, generation};
        ReleaseSRWLockExclusive(&runtime::storage::g_stateLock);
        return true;
    }
    ReleaseSRWLockExclusive(&runtime::storage::g_stateLock);
    return false;
}

/** Releases a context and securely erases every descriptor it owns. */
bool release_context(ContextHandle context) noexcept {
#if defined(DAWN_ACTIVITY_RETIREMENT_TESTS)
    g_releaseAttempts.fetch_add(1U, std::memory_order_relaxed);
    g_lastReleaseSlot.store(context.slot, std::memory_order_relaxed);
    g_lastReleaseGeneration.store(context.generation, std::memory_order_relaxed);
    if (consume_injected_release_failure()) {
        g_releaseInjectedFailures.fetch_add(1U, std::memory_order_relaxed);
        return false;
    }
#endif
    AcquireSRWLockExclusive(&runtime::storage::g_stateLock);
    ContextSlot* slot = transactions::resolve(runtime::storage::g_state.matchmaking, context);
    if (slot == nullptr) {
        ReleaseSRWLockExclusive(&runtime::storage::g_stateLock);
        return false;
    }
    // Keep only the generation needed to reject handles from this acquisition.
    const std::uint32_t generation = slot->generation;
    SecureZeroMemory(slot, sizeof *slot);
    slot->generation = generation;
    ReleaseSRWLockExclusive(&runtime::storage::g_stateLock);
#if defined(DAWN_ACTIVITY_RETIREMENT_TESTS)
    g_releaseSuccesses.fetch_add(1U, std::memory_order_relaxed);
#endif
    return true;
}

/** Copies the latest advertisement under the State lock. */
bool latest_snapshot(ContextHandle context, LatestSnapshot& snapshot) noexcept {
    SecureZeroMemory(&snapshot, sizeof snapshot);
    AcquireSRWLockShared(&runtime::storage::g_stateLock);
    MatchmakingState& state = runtime::storage::g_state.matchmaking;
    ContextSlot* slot = transactions::resolve(state, context);
    if (slot == nullptr) {
        ReleaseSRWLockShared(&runtime::storage::g_stateLock);
        return false;
    }
    LatestSnapshot prepared{};
    // A kept variant wins once it replaces the standalone latest id.
    if (slot->data.latestSlot < kVariantCapacity) {
        const VariantRecord& latest = slot->data.variants[slot->data.latestSlot];
        if (!latest.occupied || latest.advertisementId == kAbsentAdvertisementId) {
            ReleaseSRWLockShared(&runtime::storage::g_stateLock);
            return false;
        }
        prepared.advertisementId = latest.advertisementId;
        prepared.hasDescriptor = latest.hasDescriptor;
        if (latest.hasDescriptor) {
            std::memcpy(prepared.descriptor.data(), latest.descriptor.data(), kDescriptorSize);
        }
    } else if (slot->data.latestSlot == kInvalidVariantSlot
               && slot->data.standaloneLatestId != kAbsentAdvertisementId) {
        prepared.advertisementId = slot->data.standaloneLatestId;
    } else {
        ReleaseSRWLockShared(&runtime::storage::g_stateLock);
        return false;
    }
    ReleaseSRWLockShared(&runtime::storage::g_stateLock);
    snapshot = prepared;
    // Keep the descriptor in State and the caller output, not an extra stack lifetime.
    SecureZeroMemory(&prepared, sizeof prepared);
    return true;
}

/** Securely erases a transient latest-advertisement snapshot. */
void erase_snapshot(LatestSnapshot& snapshot) noexcept {
    SecureZeroMemory(&snapshot, sizeof snapshot);
}

#if defined(DAWN_ACTIVITY_RETIREMENT_TESTS)
namespace test_support {

void fail_next_releases(std::size_t count) noexcept {
    g_releaseFailuresRemaining.store(count, std::memory_order_relaxed);
}

ReleaseSnapshot snapshot() noexcept {
    return {
        {g_lastReleaseSlot.load(std::memory_order_relaxed),
         g_lastReleaseGeneration.load(std::memory_order_relaxed)},
        g_releaseAttempts.load(std::memory_order_relaxed),
        g_releaseInjectedFailures.load(std::memory_order_relaxed),
        g_releaseSuccesses.load(std::memory_order_relaxed),
    };
}

void reset() noexcept {
    g_releaseFailuresRemaining.store(0, std::memory_order_relaxed);
    g_releaseAttempts.store(0, std::memory_order_relaxed);
    g_releaseInjectedFailures.store(0, std::memory_order_relaxed);
    g_releaseSuccesses.store(0, std::memory_order_relaxed);
    g_lastReleaseSlot.store(0, std::memory_order_relaxed);
    g_lastReleaseGeneration.store(0, std::memory_order_relaxed);
}

} // namespace test_support
#endif

} // namespace dawn::state::matchmaking
