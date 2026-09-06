#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <latch>
#include <limits>
#include <memory>
#include <thread>
#include <type_traits>
#include <vector>

#include "client/hooking/call_gate.h"
#include "client/hooks/activity_lifecycle/native_activation_batch.h"
#include "client/hooks/activity_lifecycle/native_activation_contract.h"
#include "client/hooks/activity_lifecycle/native_activation_event_queue.h"
#include "client/hooks/activity_lifecycle/native_activation_flow.h"
#include "client/hooks/activity_lifecycle/native_activation_global_drop_fanout.h"
#include "client/hooks/activity_lifecycle/native_activation_validation.h"

namespace {

using namespace sunrise::client::hooks::activity_lifecycle;
using namespace sunrise::state::activity;

static_assert(kNativeActivationSlotCount == 8192U);
static_assert(contract::kHookCount == kNativeActivationHookCount);
static_assert(contract::kActivityActivateRva == 0x3CDB80U);
static_assert(contract::kActivityCloseRva == 0x3CDB20U);
static_assert(contract::kActivityReinstantiateRva == 0x3CDDC0U);
static_assert(contract::kActivityCleanupRva == 0x3CA680U);
static_assert(contract::kActivityGlobalDropRva == 0x3C8EB0U);
static_assert(contract::kActivityActivatePrefix.size() == 24U);
static_assert(contract::kActivityClosePrefix.size() == 24U);
static_assert(contract::kActivityReinstantiatePrefix.size() == 24U);
static_assert(contract::kActivityCleanupPrefix.size() == 23U);
static_assert(contract::kActivityGlobalDropPrefix.size() == 17U);
static_assert(contract::kPinnedInstalledPackedImageSha256.size() == kSha256DigestSize);
static_assert(contract::kPinnedUnpackedReReferenceSha256.size() == kSha256DigestSize);
static_assert(contract::kPinnedInstalledPackedImageSha256
              != contract::kPinnedUnpackedReReferenceSha256);
static_assert(std::is_same_v<decltype(NativeActivationSnapshot::key), NativeActivationKey>);
static_assert(std::is_trivially_copyable_v<NativeActivationSnapshot>);
static_assert(std::is_trivially_copyable_v<NativeActivationToken>);

constexpr std::array<std::byte, 24U> kExpectedActivatePrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x10}, std::byte{0x48}, std::byte{0x89}, std::byte{0x6C},
    std::byte{0x24}, std::byte{0x18}, std::byte{0x48}, std::byte{0x89},
    std::byte{0x74}, std::byte{0x24}, std::byte{0x20}, std::byte{0x57},
    std::byte{0x41}, std::byte{0x56}, std::byte{0x41}, std::byte{0x57},
    std::byte{0x48}, std::byte{0x83}, std::byte{0xEC}, std::byte{0x20},
};
constexpr std::array<std::byte, 24U> kExpectedClosePrefix{
    std::byte{0x40}, std::byte{0x57}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x20}, std::byte{0x80}, std::byte{0x79},
    std::byte{0x20}, std::byte{0x00}, std::byte{0x48}, std::byte{0x8B},
    std::byte{0xF9}, std::byte{0x74}, std::byte{0x41}, std::byte{0x48},
    std::byte{0x83}, std::byte{0xC1}, std::byte{0x28}, std::byte{0x48},
    std::byte{0x89}, std::byte{0x5C}, std::byte{0x24}, std::byte{0x30},
};
constexpr std::array<std::byte, 24U> kExpectedReinstantiatePrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x10}, std::byte{0x48}, std::byte{0x89}, std::byte{0x6C},
    std::byte{0x24}, std::byte{0x18}, std::byte{0x48}, std::byte{0x89},
    std::byte{0x74}, std::byte{0x24}, std::byte{0x20}, std::byte{0x57},
    std::byte{0x48}, std::byte{0x83}, std::byte{0xEC}, std::byte{0x30},
    std::byte{0x48}, std::byte{0x8B}, std::byte{0x41}, std::byte{0x18},
};
constexpr std::array<std::byte, 23U> kExpectedCleanupPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x08}, std::byte{0x57}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x20}, std::byte{0x80}, std::byte{0x79},
    std::byte{0x20}, std::byte{0x00}, std::byte{0x48}, std::byte{0x8B},
    std::byte{0xD9}, std::byte{0x74}, std::byte{0x2E}, std::byte{0x48},
    std::byte{0x83}, std::byte{0xC1}, std::byte{0x28},
};
constexpr std::array<std::byte, 17U> kExpectedGlobalDropPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x08}, std::byte{0x48}, std::byte{0x89}, std::byte{0x74},
    std::byte{0x24}, std::byte{0x10}, std::byte{0x48}, std::byte{0x89},
    std::byte{0x7C}, std::byte{0x24}, std::byte{0x18}, std::byte{0x4C},
    std::byte{0x89},
};
constexpr ImageSha256 kExpectedPackedSha256{
    std::byte{0x81}, std::byte{0x96}, std::byte{0x43}, std::byte{0x80},
    std::byte{0x66}, std::byte{0x4E}, std::byte{0x7F}, std::byte{0xCE},
    std::byte{0xE3}, std::byte{0xC6}, std::byte{0x20}, std::byte{0x08},
    std::byte{0x5A}, std::byte{0x15}, std::byte{0x7F}, std::byte{0xDE},
    std::byte{0xAF}, std::byte{0x91}, std::byte{0xFE}, std::byte{0xFA},
    std::byte{0xCF}, std::byte{0x72}, std::byte{0x14}, std::byte{0x90},
    std::byte{0x78}, std::byte{0x20}, std::byte{0xF1}, std::byte{0x88},
    std::byte{0xBB}, std::byte{0xEB}, std::byte{0x4C}, std::byte{0xED},
};
constexpr ImageSha256 kExpectedUnpackedReSha256{
    std::byte{0x87}, std::byte{0x13}, std::byte{0xD1}, std::byte{0x5E},
    std::byte{0x3D}, std::byte{0x05}, std::byte{0xB2}, std::byte{0x6F},
    std::byte{0x9E}, std::byte{0x25}, std::byte{0x9E}, std::byte{0x02},
    std::byte{0xB0}, std::byte{0xF2}, std::byte{0x9B}, std::byte{0xC1},
    std::byte{0xE0}, std::byte{0x00}, std::byte{0xE4}, std::byte{0xB0},
    std::byte{0xC6}, std::byte{0x2C}, std::byte{0xA2}, std::byte{0xCC},
    std::byte{0x87}, std::byte{0xC3}, std::byte{0x85}, std::byte{0x97},
    std::byte{0x18}, std::byte{0x6C}, std::byte{0xC3}, std::byte{0xBD},
};

static_assert(contract::kActivityActivatePrefix == kExpectedActivatePrefix);
static_assert(contract::kActivityClosePrefix == kExpectedClosePrefix);
static_assert(contract::kActivityReinstantiatePrefix == kExpectedReinstantiatePrefix);
static_assert(contract::kActivityCleanupPrefix == kExpectedCleanupPrefix);
static_assert(contract::kActivityGlobalDropPrefix == kExpectedGlobalDropPrefix);
static_assert(contract::kPinnedInstalledPackedImageSha256 == kExpectedPackedSha256);
static_assert(contract::kPinnedUnpackedReReferenceSha256 == kExpectedUnpackedReSha256);

int g_failureCount = 0;
std::atomic_bool g_fakeOriginalCalled{};

struct FanoutTestContext final {
    std::atomic<std::uint32_t> preCount{};
    std::atomic<std::uint32_t> postCount{};
    std::atomic<std::uint32_t> callbackDepth{};
    NativeActivationGlobalDropCohort preCohort{};
    NativeActivationGlobalDropCohort postCohort{};
};

void fanout_test_pre(void* opaque,
                     const NativeActivationGlobalDropCohort& cohort) noexcept {
    auto& context = *static_cast<FanoutTestContext*>(opaque);
    context.callbackDepth.fetch_add(1U, std::memory_order_acq_rel);
    context.preCohort = cohort;
    context.preCount.fetch_add(1U, std::memory_order_release);
    context.callbackDepth.fetch_sub(1U, std::memory_order_release);
}

void fanout_test_post(void* opaque,
                      const NativeActivationGlobalDropCohort& cohort) noexcept {
    auto& context = *static_cast<FanoutTestContext*>(opaque);
    context.callbackDepth.fetch_add(1U, std::memory_order_acq_rel);
    context.postCohort = cohort;
    context.postCount.fetch_add(1U, std::memory_order_release);
    context.callbackDepth.fetch_sub(1U, std::memory_order_release);
}

void fake_original() noexcept {
    g_fakeOriginalCalled.store(true, std::memory_order_release);
}

void check(bool condition, const char* expression, int line) {
    if (condition) {
        return;
    }
    std::cerr << __FILE__ << ':' << line << ": check failed: " << expression << '\n';
    ++g_failureCount;
}

#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

[[nodiscard]] NativeActivationAttempt make_attempt(std::uintptr_t wrapper,
                                                   std::uint32_t fullHandle,
                                                   std::uint64_t identity = 0xABCDEFU) noexcept {
    return NativeActivationAttempt{wrapper, identity, fullHandle, 7, 1U, true};
}

[[nodiscard]] NativeActivationToken token_of(const NativeActivationSnapshot& snapshot) noexcept {
    return NativeActivationToken{snapshot.key, snapshot.wrapper, snapshot.fullHandle};
}

void activation_original_runs_before_publication_and_both_success_gates_apply() {
    auto registry = std::make_unique<NativeActivationRegistry>();
    CHECK(registry->begin_module().status == BeginModuleStatus::started);
    const NativeActivationAttempt attempt = make_attempt(0x1000U, 0x12340011U);

    bool originalCalled = false;
    const ActivationFlowResult failed = observe_activation(
        *registry,
        attempt,
        [&]() noexcept {
            originalCalled = true;
            return std::uint8_t{0};
        },
        [&]() noexcept {
            CHECK(false);
            return true;
        },
        []() noexcept { return true; });
    CHECK(originalCalled);
    CHECK(failed.nativeResult == 0U);
    CHECK(!registry->snapshot(attempt.wrapper, attempt.fullHandle));

    originalCalled = false;
    const ActivationFlowResult inactive = observe_activation(
        *registry,
        attempt,
        [&]() noexcept {
            originalCalled = true;
            return std::uint8_t{1};
        },
        [&]() noexcept {
            CHECK(originalCalled);
            return false;
        },
        []() noexcept { return true; });
    CHECK(inactive.nativeResult == 1U);
    CHECK(inactive.postWorkAdmitted);
    CHECK(!inactive.postconditionObserved);
    CHECK(!registry->snapshot(attempt.wrapper, attempt.fullHandle));

    bool activeReaderCalled = false;
    const ActivationFlowResult quiesced = observe_activation(
        *registry,
        attempt,
        []() noexcept { return std::uint8_t{1}; },
        [&]() noexcept {
            activeReaderCalled = true;
            return true;
        },
        []() noexcept { return false; });
    CHECK(quiesced.nativeResult == 1U);
    CHECK(!quiesced.postWorkAdmitted);
    CHECK(!activeReaderCalled);
    CHECK(!registry->snapshot(attempt.wrapper, attempt.fullHandle));

    originalCalled = false;
    const ActivationFlowResult success = observe_activation(
        *registry,
        attempt,
        [&]() noexcept {
            CHECK(!registry->snapshot(attempt.wrapper, attempt.fullHandle));
            originalCalled = true;
            return std::uint8_t{1};
        },
        [&]() noexcept {
            CHECK(originalCalled);
            return true;
        },
        []() noexcept { return true; });
    CHECK(success.publication.status == PublishStatus::published);
    CHECK(success.publication.snapshot.key.generation.value == kFirstGeneration);
    CHECK(registry->is_current(token_of(success.publication.snapshot)));
}

void close_quiesces_before_original_and_retires_after_original() {
    auto registry = std::make_unique<NativeActivationRegistry>();
    CHECK(registry->begin_module().status == BeginModuleStatus::started);
    const PublishResult published = registry->publish(make_attempt(0x2000U, 0x20000022U));
    CHECK(published.status == PublishStatus::published);
    const NativeActivationToken predecessor = token_of(published.snapshot);

    bool originalCalled = false;
    const CloseFlowResult closed = observe_close(
        *registry,
        predecessor.wrapper,
        predecessor.fullHandle,
        true,
        [&]() noexcept {
            originalCalled = true;
            CHECK(!registry->is_current(predecessor));
            const NativeActivationSnapshot during =
                registry->snapshot(predecessor.wrapper, predecessor.fullHandle);
            CHECK(during.state == NativeActivationState::quiescing);
        },
        []() noexcept { return true; });
    CHECK(originalCalled);
    CHECK(closed.capture.status == CaptureStatus::captured);
    CHECK(closed.retirement == RetireStatus::retired);
    CHECK(!registry->snapshot(predecessor.wrapper, predecessor.fullHandle));
    CHECK(!registry->is_current(predecessor));
}

void nested_same_wrapper_reopen_survives_outer_stale_retirement() {
    auto registry = std::make_unique<NativeActivationRegistry>();
    CHECK(registry->begin_module().status == BeginModuleStatus::started);
    const NativeActivationAttempt attempt = make_attempt(0x3000U, 0xABCD0033U, 0x11223344U);
    const PublishResult first = registry->publish(attempt);
    CHECK(first.status == PublishStatus::published);
    const NativeActivationToken predecessor = token_of(first.snapshot);

    ActivationFlowResult nested{};
    const CloseFlowResult outer = observe_close(
        *registry,
        attempt.wrapper,
        attempt.fullHandle,
        true,
        [&]() noexcept {
            CHECK(!registry->is_current(predecessor));
            nested = observe_activation(
                *registry,
                attempt,
                []() noexcept { return std::uint8_t{1}; },
                []() noexcept { return true; },
                []() noexcept { return true; });
            CHECK(nested.publication.status == PublishStatus::published);
            CHECK(registry->is_current(token_of(nested.publication.snapshot)));
        },
        []() noexcept { return true; });

    CHECK(outer.capture.status == CaptureStatus::captured);
    CHECK(outer.retirement == RetireStatus::staleToken);
    CHECK(nested.publication.snapshot.key != predecessor.key);
    CHECK(nested.publication.snapshot.key.generation.value
          == predecessor.key.generation.value + 1U);
    CHECK(registry->is_current(token_of(nested.publication.snapshot)));
    CHECK(!registry->is_current(predecessor));
}

void global_drop_quiesces_exact_cohort_and_preserves_nested_successor() {
    auto registry = std::make_unique<NativeActivationRegistry>();
    CHECK(registry->begin_module().status == BeginModuleStatus::started);
    const NativeActivationAttempt firstAttempt = make_attempt(0x3100U, 0x31000031U);
    const NativeActivationAttempt secondAttempt = make_attempt(0x3200U, 0x32000032U);
    const PublishResult first = registry->publish(firstAttempt);
    const PublishResult second = registry->publish(secondAttempt);
    CHECK(first.status == PublishStatus::published);
    CHECK(second.status == PublishStatus::published);
    const NativeActivationToken firstPredecessor = token_of(first.snapshot);
    const NativeActivationToken secondPredecessor = token_of(second.snapshot);

    NativeActivationGlobalDropFanout fanout{};
    FanoutTestContext fanoutContext{};
    const NativeActivationGlobalDropObserverTable fanoutTable{
        kNativeActivationGlobalDropObserverAbiVersion,
        &fanoutContext,
        &fanout_test_pre,
        &fanout_test_post,
    };
    NativeActivationGlobalDropObserverHandle fanoutHandle{};
    CHECK(fanout.register_observer(&fanoutTable, fanoutHandle));
    fanout.accept();
    NativeActivationGlobalDropFanout::Dispatch dispatch{};
    NativeActivationGlobalDropCohort cohort{};

    ActivationFlowResult nested{};
    const GlobalDropFlowResult drop = observe_global_drop(
        *registry,
        [&]() noexcept {
            const NativeActivationSnapshot firstDuringDrop =
                registry->snapshot(firstAttempt.wrapper, firstAttempt.fullHandle);
            const NativeActivationSnapshot secondDuringDrop =
                registry->snapshot(secondAttempt.wrapper, secondAttempt.fullHandle);
            CHECK(firstDuringDrop.state == NativeActivationState::quiescing);
            CHECK(secondDuringDrop.state == NativeActivationState::quiescing);
            CHECK(!registry->is_current(firstPredecessor));
            CHECK(!registry->is_current(secondPredecessor));

            // Models an activation nested inside the native global-drop original. Its fresh key
            // replaces only the first exact predecessor and clears that slot's captured epoch.
            nested = observe_activation(
                *registry,
                firstAttempt,
                []() noexcept { return std::uint8_t{1}; },
                []() noexcept { return true; },
                []() noexcept { return true; });
            CHECK(nested.publication.status == PublishStatus::published);
            CHECK(registry->is_current(token_of(nested.publication.snapshot)));
            CHECK(fanoutContext.preCount.load(std::memory_order_acquire) == 1U);
            CHECK(fanoutContext.postCount.load(std::memory_order_acquire) == 0U);
            CHECK(fanoutContext.callbackDepth.load(std::memory_order_acquire) == 0U);
        },
        []() noexcept { return true; },
        [&](const GlobalDropBeginResult& capture) noexcept {
            cohort = NativeActivationGlobalDropCohort{
                capture.token.module.value,
                capture.token.epoch,
                static_cast<std::uint32_t>(capture.token.captured),
                static_cast<bool>(capture.token),
            };
            dispatch = fanout.acquire();
            detail::notify_native_activation_global_drop_pre(dispatch, cohort);
        },
        [&](const GlobalDropBeginResult&) noexcept {
            detail::notify_native_activation_global_drop_post(dispatch, cohort);
            dispatch.finish();
            // The external post facet finishes before activation compare-retirement.
            CHECK(registry->snapshot(secondAttempt.wrapper, secondAttempt.fullHandle).state
                  == NativeActivationState::quiescing);
        });

    CHECK(drop.capture.status == GlobalDropBeginStatus::captured);
    CHECK(drop.capture.token);
    CHECK(drop.capture.token.captured == 2U);
    CHECK(drop.retirement.matchedModule);
    CHECK(drop.retirement.retired == 1U);
    CHECK(drop.retirement.stale == 1U);
    CHECK(fanoutContext.preCount.load(std::memory_order_acquire) == 1U);
    CHECK(fanoutContext.postCount.load(std::memory_order_acquire) == 1U);
    CHECK(fanoutContext.preCohort == cohort);
    CHECK(fanoutContext.postCohort == cohort);
    CHECK(registry->is_current(token_of(nested.publication.snapshot)));
    CHECK(!registry->is_current(firstPredecessor));
    CHECK(!registry->snapshot(secondAttempt.wrapper, secondAttempt.fullHandle));
    CHECK(registry->mutation_enabled());

    // Global drop is not module quiescence: later success can publish a fresh generation.
    const PublishResult later = registry->publish(secondAttempt);
    CHECK(later.status == PublishStatus::published);
    CHECK(later.snapshot.key.module == drop.capture.token.module);
    CHECK(later.snapshot.key.generation.value
          == nested.publication.snapshot.key.generation.value + 1U);
    fanout.quiesce();
    CHECK(fanout.unregister_observer(fanoutHandle));
}

void global_drop_uses_all_bounded_slots_and_old_module_token_is_stale() {
    auto registry = std::make_unique<NativeActivationRegistry>();
    const BeginModuleResult firstModule = registry->begin_module();
    CHECK(firstModule.status == BeginModuleStatus::started);
    for (std::uint32_t slot = 0U; slot < kNativeActivationSlotCount; ++slot) {
        CHECK(registry
                  ->publish(make_attempt(static_cast<std::uintptr_t>(slot) + 1U,
                                         slot,
                                         static_cast<std::uint64_t>(slot) + 20U))
                  .status
              == PublishStatus::published);
    }

    const GlobalDropBeginResult capture = registry->begin_global_drop();
    CHECK(capture.status == GlobalDropBeginStatus::captured);
    CHECK(capture.token.captured == kNativeActivationSlotCount);
    CHECK(registry->snapshot(1U, 0U).state == NativeActivationState::quiescing);
    CHECK(registry->snapshot(kNativeActivationSlotCount,
                             static_cast<std::uint32_t>(kNativeActivationSlotCount - 1U))
              .state
          == NativeActivationState::quiescing);
    const GlobalDropFinishResult retirement = registry->finish_global_drop(capture.token);
    CHECK(retirement.matchedModule);
    CHECK(retirement.retired == kNativeActivationSlotCount);
    CHECK(retirement.stale == 0U);
    CHECK(registry->mutation_enabled());

    CHECK(registry->finish_module(firstModule.generation));
    const BeginModuleResult secondModule = registry->begin_module();
    CHECK(secondModule.status == BeginModuleStatus::started);
    const NativeActivationAttempt successorAttempt = make_attempt(0x3300U, 0x33000033U);
    const PublishResult successor = registry->publish(successorAttempt);
    CHECK(successor.status == PublishStatus::published);
    const GlobalDropFinishResult stale = registry->finish_global_drop(capture.token);
    CHECK(!stale.matchedModule);
    CHECK(stale.retired == 0U);
    CHECK(stale.stale == 0U);
    CHECK(registry->is_current(token_of(successor.snapshot)));
}

void stale_close_and_aba_tokens_cannot_retire_a_successor() {
    auto registry = std::make_unique<NativeActivationRegistry>();
    CHECK(registry->begin_module().status == BeginModuleStatus::started);
    const NativeActivationAttempt attempt = make_attempt(0x4000U, 0x44440044U);
    const PublishResult first = registry->publish(attempt);
    const NativeActivationToken stale = token_of(first.snapshot);
    const CaptureResult captured =
        registry->capture_predecessor(attempt.wrapper, attempt.fullHandle, true);
    CHECK(captured.token == stale);

    const PublishResult successor = registry->publish(attempt);
    CHECK(successor.status == PublishStatus::published);
    const NativeActivationToken current = token_of(successor.snapshot);
    CHECK(current.key != stale.key);
    CHECK(registry->retire(stale) == RetireStatus::staleToken);
    CHECK(registry->is_current(current));
    CHECK(!registry->is_current(stale));

    const CaptureResult closeCurrent =
        registry->capture_predecessor(current.wrapper, current.fullHandle, true);
    CHECK(closeCurrent.status == CaptureStatus::captured);
    CHECK(registry->retire(closeCurrent.token) == RetireStatus::retired);
    const PublishResult reused = registry->publish(attempt);
    CHECK(reused.status == PublishStatus::published);
    CHECK(registry->is_current(token_of(reused.snapshot)));
    CHECK(!registry->is_current(stale));
    CHECK(!registry->is_current(current));
}

void active_close_mismatch_fails_closed_without_retiring_a_successor() {
    auto registry = std::make_unique<NativeActivationRegistry>();
    CHECK(registry->begin_module().status == BeginModuleStatus::started);
    constexpr std::uint32_t predecessorHandle = 0x41000041U;
    constexpr std::uint32_t collidingHandle = predecessorHandle + 0x2000U;
    const NativeActivationAttempt attempt = make_attempt(0x4100U, predecessorHandle);
    const PublishResult predecessor = registry->publish(attempt);
    CHECK(predecessor.status == PublishStatus::published);
    const NativeActivationToken predecessorToken = token_of(predecessor.snapshot);

    const CaptureResult outer = registry->capture_predecessor(
        attempt.wrapper, attempt.fullHandle, true);
    CHECK(outer.status == CaptureStatus::captured);
    const PublishResult successor = registry->publish(attempt);
    CHECK(successor.status == PublishStatus::published);
    const NativeActivationToken successorToken = token_of(successor.snapshot);

    bool originalCalled = false;
    const CloseFlowResult conflicting = observe_close(
        *registry,
        0x4200U,
        collidingHandle,
        true,
        [&]() noexcept { originalCalled = true; },
        []() noexcept { return true; });
    CHECK(originalCalled);
    CHECK(conflicting.capture.status == CaptureStatus::ownershipConflict);
    CHECK(!conflicting.capture.token);
    CHECK(conflicting.capture.predecessor.key == successorToken.key);
    CHECK(conflicting.capture.predecessor.wrapper == successorToken.wrapper);
    CHECK(conflicting.capture.predecessor.fullHandle == successorToken.fullHandle);
    CHECK(conflicting.capture.predecessor.state == NativeActivationState::quiescing);
    CHECK(conflicting.retirement == RetireStatus::invalidToken);
    CHECK(!registry->mutation_enabled());
    CHECK(!registry->is_current(successorToken));
    const NativeActivationSnapshot retained =
        registry->snapshot(successorToken.wrapper, successorToken.fullHandle);
    CHECK(retained.key == successorToken.key);
    CHECK(retained.state == NativeActivationState::quiescing);
    CHECK(registry->retire(predecessorToken) == RetireStatus::staleToken);
    CHECK(registry->snapshot(successorToken.wrapper, successorToken.fullHandle).key
          == successorToken.key);

    auto missing = std::make_unique<NativeActivationRegistry>();
    CHECK(missing->begin_module().status == BeginModuleStatus::started);
    const CaptureResult noPublication =
        missing->capture_predecessor(0x4300U, 0x43000043U, true);
    CHECK(noPublication.status == CaptureStatus::ownershipConflict);
    CHECK(!missing->mutation_enabled());

    auto invalidHandle = std::make_unique<NativeActivationRegistry>();
    CHECK(invalidHandle->begin_module().status == BeginModuleStatus::started);
    const PublishResult validPublication =
        invalidHandle->publish(make_attempt(0x4400U, 0x44000044U));
    CHECK(validPublication.status == PublishStatus::published);
    const CaptureResult sentinel = invalidHandle->capture_predecessor(
        0x4400U, kInvalidNativeActivityHandle, true);
    CHECK(sentinel.status == CaptureStatus::ownershipConflict);
    CHECK(!sentinel.token);
    CHECK(!invalidHandle->mutation_enabled());
    CHECK(invalidHandle
              ->snapshot(validPublication.snapshot.wrapper,
                         validPublication.snapshot.fullHandle)
              .state
          == NativeActivationState::quiescing);
}

void exact_wrapper_and_full_handle_are_required_beyond_the_masked_slot() {
    auto registry = std::make_unique<NativeActivationRegistry>();
    CHECK(registry->begin_module().status == BeginModuleStatus::started);
    constexpr std::uint32_t firstHandle = 0x10000055U;
    constexpr std::uint32_t collidingHandle = firstHandle + 0x2000U;
    const PublishResult first = registry->publish(make_attempt(0x5000U, firstHandle));
    CHECK(first.status == PublishStatus::published);

    CHECK(!registry->snapshot(0x5001U, firstHandle));
    CHECK(!registry->snapshot(0x5000U, collidingHandle));
    CHECK(!registry->is_current(
        NativeActivationToken{first.snapshot.key, 0x5001U, firstHandle}));
    CHECK(!registry->is_current(
        NativeActivationToken{first.snapshot.key, 0x5000U, collidingHandle}));

    const PublishResult collision = registry->publish(make_attempt(0x6000U, collidingHandle));
    CHECK(collision.status == PublishStatus::slotCollision);
    CHECK(!registry->mutation_enabled());
    CHECK(!registry->is_current(token_of(first.snapshot)));
}

void second_success_without_a_close_is_an_impossible_current_conflict() {
    auto registry = std::make_unique<NativeActivationRegistry>();
    CHECK(registry->begin_module().status == BeginModuleStatus::started);
    const NativeActivationAttempt attempt = make_attempt(0x6100U, 0x61000061U);
    const PublishResult first = registry->publish(attempt);
    CHECK(first.status == PublishStatus::published);
    const PublishResult second = registry->publish(attempt);
    CHECK(second.status == PublishStatus::currentConflict);
    CHECK(!registry->mutation_enabled());
    CHECK(!registry->is_current(token_of(first.snapshot)));
    CHECK(registry->snapshot(attempt.wrapper, attempt.fullHandle).state
          == NativeActivationState::quiescing);
}

void all_8192_slots_fill_without_eviction_and_the_next_collision_fails_closed() {
    auto registry = std::make_unique<NativeActivationRegistry>();
    CHECK(registry->begin_module().status == BeginModuleStatus::started);
    for (std::uint32_t slot = 0U; slot < kNativeActivationSlotCount; ++slot) {
        const PublishResult result = registry->publish(
            make_attempt(static_cast<std::uintptr_t>(slot) + 1U, slot, slot + 10U));
        CHECK(result.status == PublishStatus::published);
    }
    CHECK(registry->mutation_enabled());

    const PublishResult full = registry->publish(
        make_attempt(kNativeActivationSlotCount + 1U,
                     static_cast<std::uint32_t>(kNativeActivationSlotCount)));
    CHECK(full.status == PublishStatus::slotCollision);
    CHECK(!registry->mutation_enabled());
    CHECK(registry->snapshot(1U, 0U).state == NativeActivationState::quiescing);
}

void generation_exhaustion_never_wraps_and_disables_mutation() {
    {
        NativeActivationClock clock{};
        clock.module.value = kMaximumGeneration;
        auto registry = std::make_unique<NativeActivationRegistry>(clock);
        const BeginModuleResult result = registry->begin_module();
        CHECK(result.status == BeginModuleStatus::generationExhausted);
        CHECK(registry->clock().module.value == kMaximumGeneration);
        CHECK(registry->clock().moduleExhausted);
        CHECK(!registry->mutation_enabled());
    }
    {
        NativeActivationClock clock{};
        clock.activation.value = kMaximumGeneration;
        auto registry = std::make_unique<NativeActivationRegistry>(clock);
        CHECK(registry->begin_module().status == BeginModuleStatus::started);
        const PublishResult result = registry->publish(make_attempt(0x7000U, 0x77U));
        CHECK(result.status == PublishStatus::generationExhausted);
        CHECK(registry->clock().activation.value == kMaximumGeneration);
        CHECK(registry->clock().activationExhausted);
        CHECK(!registry->mutation_enabled());
    }
}

void impossible_success_arguments_disable_the_current_module() {
    auto registry = std::make_unique<NativeActivationRegistry>();
    CHECK(registry->begin_module().status == BeginModuleStatus::started);
    const PublishResult invalid = registry->publish(make_attempt(0U, 0x99U));
    CHECK(invalid.status == PublishStatus::invalidAttempt);
    CHECK(!registry->mutation_enabled());
    CHECK(registry->publish(make_attempt(0x9900U, 0x99U)).status
          == PublishStatus::mutationDisabled);
}

void module_restart_changes_owner_and_preserves_the_process_activation_clock() {
    auto registry = std::make_unique<NativeActivationRegistry>();
    const BeginModuleResult firstModule = registry->begin_module();
    CHECK(firstModule.status == BeginModuleStatus::started);
    const NativeActivationAttempt attempt = make_attempt(0x8000U, 0x88U);
    const PublishResult first = registry->publish(attempt);
    const NativeActivationToken old = token_of(first.snapshot);
    CHECK(registry->quiesce_module(firstModule.generation));
    CHECK(!registry->is_current(old));
    CHECK(registry->finish_module(firstModule.generation));

    const BeginModuleResult secondModule = registry->begin_module();
    CHECK(secondModule.status == BeginModuleStatus::started);
    CHECK(secondModule.generation.value == firstModule.generation.value + 1U);
    const PublishResult second = registry->publish(attempt);
    CHECK(second.status == PublishStatus::published);
    CHECK(second.snapshot.key.module == secondModule.generation);
    CHECK(second.snapshot.key.generation.value == old.key.generation.value + 1U);
    CHECK(!registry->is_current(old));
    CHECK(registry->is_current(token_of(second.snapshot)));
}

void packed_identity_and_mapped_prefix_validation_are_distinct_and_retryable() {
    constexpr std::size_t mappedSize =
        static_cast<std::size_t>(contract::kActivityReinstantiateRva)
        + contract::kActivityReinstantiatePrefix.size() + 0x100U;
    constexpr std::size_t packedTextRawDelta = 0xA00U;
    CHECK(contract::kActivityActivateRva - packedTextRawDelta == 0x3CD180U);

    std::vector<std::byte> mapped(mappedSize);
    std::vector<std::byte> packedRaw(mappedSize, std::byte{0xA5});
    const std::array contracts{
        NativeActivationTargetContract{contract::kActivityActivateRva,
                                       contract::kActivityActivatePrefix},
        NativeActivationTargetContract{contract::kActivityCloseRva,
                                       contract::kActivityClosePrefix},
        NativeActivationTargetContract{contract::kActivityReinstantiateRva,
                                       contract::kActivityReinstantiatePrefix},
        NativeActivationTargetContract{contract::kActivityCleanupRva,
                                       contract::kActivityCleanupPrefix},
        NativeActivationTargetContract{contract::kActivityGlobalDropRva,
                                       contract::kActivityGlobalDropPrefix},
    };

    const auto decrypt_prefixes = [&](std::vector<std::byte>& image) {
        for (const NativeActivationTargetContract& target : contracts) {
            std::copy(target.prefix.begin(),
                      target.prefix.end(),
                      image.begin() + static_cast<std::size_t>(target.rva));
        }
    };
    decrypt_prefixes(mapped);

    std::array<std::uintptr_t, contract::kHookCount> outputs{};
    outputs.fill(1U);
    NativeActivationImageView mappedView{
        mapped.data(), mapped.size(), contract::kPinnedInstalledPackedImageSha256};
    CHECK(validate_native_activation_image(mappedView,
                                           contract::kPinnedInstalledPackedImageSha256,
                                           contracts,
                                           outputs)
          == NativeActivationValidationResult::valid);
    for (std::size_t index = 0U; index < outputs.size(); ++index) {
        CHECK(outputs[index]
              == reinterpret_cast<std::uintptr_t>(mapped.data()) + contracts[index].rva);
    }

    // The unpacked RE digest cannot impersonate the installed packed-file identity.
    mappedView.installedPackedOnDiskSha256 = contract::kPinnedUnpackedReReferenceSha256;
    outputs.fill(1U);
    CHECK(validate_native_activation_image(mappedView,
                                           contract::kPinnedInstalledPackedImageSha256,
                                           contracts,
                                           outputs)
          == NativeActivationValidationResult::imageHashMismatch);
    CHECK((outputs == std::array<std::uintptr_t, contract::kHookCount>{}));

    // Swapping the expected identity in the other direction also fails exact equality.
    mappedView.installedPackedOnDiskSha256 = contract::kPinnedInstalledPackedImageSha256;
    outputs.fill(1U);
    CHECK(validate_native_activation_image(mappedView,
                                           contract::kPinnedUnpackedReReferenceSha256,
                                           contracts,
                                           outputs)
          == NativeActivationValidationResult::imageHashMismatch);
    CHECK((outputs == std::array<std::uintptr_t, contract::kHookCount>{}));

    // Reading packed raw storage as though raw offsets were mapped RVAs must not pass.
    NativeActivationImageView packedRawView{
        packedRaw.data(), packedRaw.size(), contract::kPinnedInstalledPackedImageSha256};
    outputs.fill(1U);
    CHECK(validate_native_activation_image(packedRawView,
                                           contract::kPinnedInstalledPackedImageSha256,
                                           contracts,
                                           outputs)
          == NativeActivationValidationResult::prefixMismatch);
    CHECK((outputs == std::array<std::uintptr_t, contract::kHookCount>{}));

    // An early encrypted mapped view fails without publication and succeeds on a later retry.
    std::vector<std::byte> decrypting(mappedSize, std::byte{0xCC});
    NativeActivationImageView retryView{
        decrypting.data(), decrypting.size(), contract::kPinnedInstalledPackedImageSha256};
    NativeActivationBatchState retryBatch{};
    outputs.fill(1U);
    CHECK(validate_native_activation_image(retryView,
                                           contract::kPinnedInstalledPackedImageSha256,
                                           contracts,
                                           outputs)
          == NativeActivationValidationResult::prefixMismatch);
    CHECK((outputs == std::array<std::uintptr_t, contract::kHookCount>{}));
    CHECK(retryBatch.snapshot().phase == NativeActivationBatchPhase::detached);
    decrypt_prefixes(decrypting);
    CHECK(validate_native_activation_image(retryView,
                                           contract::kPinnedInstalledPackedImageSha256,
                                           contracts,
                                           outputs)
          == NativeActivationValidationResult::valid);
    CHECK(retryBatch.snapshot().phase == NativeActivationBatchPhase::detached);
    CHECK(retryBatch.begin_install());
    CHECK(retryBatch.rollback_install());

    const std::array outOfBounds{
        NativeActivationTargetContract{static_cast<std::uintptr_t>(mappedSize - 1U),
                                       contract::kActivityActivatePrefix},
    };
    std::array<std::uintptr_t, 1U> oneOutput{1U};
    CHECK(validate_native_activation_image(mappedView,
                                           contract::kPinnedInstalledPackedImageSha256,
                                           outOfBounds,
                                           oneOutput)
          == NativeActivationValidationResult::targetOutOfRange);
    CHECK(oneOutput[0] == 0U);
}

void batch_model_enforces_all_or_none_publication_and_retained_detach() {
    NativeActivationBatchState batch{};
    CHECK(batch.snapshot().phase == NativeActivationBatchPhase::detached);

    CHECK(batch.begin_install());

    constexpr std::array<std::uintptr_t, kNativeActivationHookCount> partial{
        0x1000U, 0x2000U, 0U, 0x4000U, 0x5000U};
    CHECK(!batch.complete_install(partial));
    CHECK(batch.snapshot().phase == NativeActivationBatchPhase::installing);
    CHECK((batch.snapshot().originals
           == std::array<std::uintptr_t, kNativeActivationHookCount>{}));
    CHECK(batch.rollback_install());
    CHECK(batch.snapshot().phase == NativeActivationBatchPhase::detached);

    constexpr std::array<std::uintptr_t, kNativeActivationHookCount> complete{
        0x1000U, 0x2000U, 0x3000U, 0x4000U, 0x5000U};
    NativeActivationBatchState retainedPublicationFailure{};
    CHECK(retainedPublicationFailure.begin_install());
    CHECK(retainedPublicationFailure.retain_install_failure(complete));
    CHECK(retainedPublicationFailure.snapshot().phase
          == NativeActivationBatchPhase::quiescing);
    CHECK(retainedPublicationFailure.snapshot().originals == complete);
    CHECK(retainedPublicationFailure.record_detach(
        NativeActivationDetachDisposition::removed));
    CHECK(retainedPublicationFailure.snapshot().phase
          == NativeActivationBatchPhase::detached);

    CHECK(batch.begin_install());
    CHECK(batch.complete_install(complete));
    CHECK(batch.snapshot().phase == NativeActivationBatchPhase::running);
    CHECK(batch.snapshot().originals == complete);
    CHECK(batch.quiesce());

    CHECK(batch.record_detach(NativeActivationDetachDisposition::deferred));
    CHECK(batch.snapshot().phase == NativeActivationBatchPhase::quiescing);
    CHECK(batch.snapshot().originals == complete);
    CHECK(batch.record_detach(NativeActivationDetachDisposition::failed));
    CHECK(batch.snapshot().phase == NativeActivationBatchPhase::quiescing);
    CHECK(batch.snapshot().originals == complete);
    CHECK(batch.record_detach(NativeActivationDetachDisposition::removed));
    CHECK(batch.snapshot().phase == NativeActivationBatchPhase::detached);
    CHECK((batch.snapshot().originals
           == std::array<std::uintptr_t, kNativeActivationHookCount>{}));
}

void call_gate_covers_publication_wait_and_nested_in_flight_detach() {
    using FakeOriginal = void (*)() noexcept;
    sunrise::client::hooking::CallGate gate{};
    std::atomic<FakeOriginal> original{};
    std::latch entered{1};
    g_fakeOriginalCalled.store(false, std::memory_order_release);

    std::thread waiting([&]() noexcept {
        sunrise::client::hooking::CallGate::Scope call(gate);
        entered.count_down();
        const FakeOriginal forwarded = sunrise::client::hooking::await_original(original);
        forwarded();
    });
    entered.wait();
    CHECK(gate.active_calls() == 1U);
    CHECK(!g_fakeOriginalCalled.load(std::memory_order_acquire));
    sunrise::client::hooking::publish_original(original, &fake_original);
    waiting.join();
    CHECK(g_fakeOriginalCalled.load(std::memory_order_acquire));
    CHECK(gate.idle());
    CHECK(!gate.accepting());

    std::array<sunrise::client::hooking::CallGate, kNativeActivationHookCount> gates{};
    NativeActivationBatchState batch{};
    constexpr std::array<std::uintptr_t, kNativeActivationHookCount> originals{
        0x1000U, 0x2000U, 0x3000U, 0x4000U, 0x5000U};
    CHECK(batch.begin_install());
    CHECK(batch.complete_install(originals));
    CHECK(batch.quiesce());
    {
        sunrise::client::hooking::CallGate::Scope outer(gates[2]);
        CHECK(gates[2].active_calls() == 1U);
        {
            sunrise::client::hooking::CallGate::Scope nested(gates[0]);
            CHECK(gates[0].active_calls() == 1U);
            CHECK(batch.record_detach(NativeActivationDetachDisposition::deferred));
            CHECK(batch.snapshot().phase == NativeActivationBatchPhase::quiescing);
            CHECK(batch.snapshot().originals == originals);
        }
        CHECK(gates[0].idle());
        CHECK(!gates[2].idle());
    }
    CHECK(gates[2].idle());
    CHECK(batch.record_detach(NativeActivationDetachDisposition::removed));
    CHECK(batch.snapshot().phase == NativeActivationBatchPhase::detached);
}

void global_drop_fanout_is_generation_safe_nonblocking_and_quiescent() {
    NativeActivationGlobalDropFanout fanout{};
    FanoutTestContext firstContext{};
    const NativeActivationGlobalDropObserverTable firstTable{
        kNativeActivationGlobalDropObserverAbiVersion,
        &firstContext,
        &fanout_test_pre,
        &fanout_test_post,
    };
    NativeActivationGlobalDropObserverHandle firstHandle{};
    CHECK(fanout.register_observer(&firstTable, firstHandle));
    CHECK(firstHandle);
    CHECK(fanout.has_registration());
    CHECK(!fanout.accepting());
    CHECK(!fanout.acquire());
    fanout.accept();

    constexpr NativeActivationGlobalDropCohort cohort{7U, 11U, 2U, true};
    std::latch preFinished{1};
    std::latch continuePost{1};
    std::thread admitted([&]() noexcept {
        NativeActivationGlobalDropFanout::Dispatch dispatch = fanout.acquire();
        CHECK(dispatch);
        dispatch.notify_pre(cohort);
        CHECK(firstContext.callbackDepth.load(std::memory_order_acquire) == 0U);
        preFinished.count_down();

        // Models native forwarding: the immutable table lease remains, but no callback runs.
        continuePost.wait();
        CHECK(firstContext.callbackDepth.load(std::memory_order_acquire) == 0U);
        dispatch.notify_post(cohort);
        dispatch.finish();
    });
    preFinished.wait();

    // Retirement is nonblocking and must retain table/context across the native interval.
    CHECK(!fanout.unregister_observer(firstHandle));
    CHECK(fanout.has_registration());
    fanout.quiesce();
    CHECK(!fanout.accepting());
    CHECK(!fanout.acquire());
    CHECK(!fanout.idle());
    continuePost.count_down();
    admitted.join();

    CHECK(fanout.idle());
    CHECK(firstContext.preCount.load(std::memory_order_acquire) == 1U);
    CHECK(firstContext.postCount.load(std::memory_order_acquire) == 1U);
    CHECK(firstContext.preCohort == cohort);
    CHECK(firstContext.postCohort == cohort);
    CHECK(fanout.unregister_observer(firstHandle));
    CHECK(!fanout.has_registration());

    FanoutTestContext secondContext{};
    const NativeActivationGlobalDropObserverTable secondTable{
        kNativeActivationGlobalDropObserverAbiVersion,
        &secondContext,
        &fanout_test_pre,
        &fanout_test_post,
    };
    NativeActivationGlobalDropObserverHandle secondHandle{};
    CHECK(fanout.register_observer(&secondTable, secondHandle));
    CHECK(secondHandle.generation != firstHandle.generation);
    CHECK(!fanout.unregister_observer(firstHandle));
    fanout.accept();
    {
        NativeActivationGlobalDropFanout::Dispatch dispatch = fanout.acquire();
        CHECK(dispatch);
        dispatch.notify_pre(cohort);
        dispatch.notify_post(cohort);
    }
    CHECK(secondContext.preCount.load(std::memory_order_acquire) == 1U);
    CHECK(secondContext.postCount.load(std::memory_order_acquire) == 1U);
    CHECK(firstContext.preCount.load(std::memory_order_acquire) == 1U);
    fanout.quiesce();
    CHECK(fanout.clear_after_removed());
    CHECK(!fanout.has_registration());
    CHECK(!fanout.unregister_observer(secondHandle));

    NativeActivationGlobalDropObserverTable invalid = secondTable;
    invalid.abiVersion = kNativeActivationGlobalDropObserverAbiVersion + 1U;
    NativeActivationGlobalDropObserverHandle invalidHandle{99U};
    CHECK(!fanout.register_observer(&invalid, invalidHandle));
    CHECK(!invalidHandle);
}

void global_drop_fanout_stale_unregister_cannot_race_reserved_publication() {
    NativeActivationGlobalDropFanout fanout{};
    FanoutTestContext context{};
    const NativeActivationGlobalDropObserverTable table{
        kNativeActivationGlobalDropObserverAbiVersion,
        &context,
        &fanout_test_pre,
        &fanout_test_post,
    };

    NativeActivationGlobalDropObserverHandle stale{};
    CHECK(fanout.register_observer(&table, stale));
    CHECK(fanout.unregister_observer(stale));
    for (std::size_t iteration = 0U; iteration < 64U; ++iteration) {
        std::latch start{1};
        NativeActivationGlobalDropObserverHandle current{};
        bool registered = false;
        bool staleRemoved = true;
        std::thread publisher([&]() noexcept {
            start.wait();
            registered = fanout.register_observer(&table, current);
        });
        std::thread staleRetire([&]() noexcept {
            start.wait();
            staleRemoved = fanout.unregister_observer(stale);
        });
        start.count_down();
        publisher.join();
        staleRetire.join();
        CHECK(registered);
        CHECK(!staleRemoved);
        CHECK(current);
        CHECK(current.generation != stale.generation);
        CHECK(fanout.unregister_observer(current));
        stale = current;
    }

    NativeActivationGlobalDropObserverHandle active{};
    CHECK(fanout.register_observer(&table, active));
    NativeActivationGlobalDropObserverHandle republished{};
    std::latch start{1};
    bool firstRemoved = false;
    bool secondRemoved = false;
    bool republishSucceeded = false;
    std::thread firstRetire([&]() noexcept {
        start.wait();
        firstRemoved = fanout.unregister_observer(active);
    });
    std::thread secondRetire([&]() noexcept {
        start.wait();
        secondRemoved = fanout.unregister_observer(active);
    });
    std::thread publisher([&]() noexcept {
        start.wait();
        while (!fanout.register_observer(&table, republished)) {
            std::this_thread::yield();
        }
        republishSucceeded = true;
    });
    start.count_down();
    firstRetire.join();
    secondRetire.join();
    publisher.join();
    CHECK(firstRemoved != secondRemoved);
    CHECK(republishSucceeded);
    CHECK(republished.generation != active.generation);
    fanout.accept();
    {
        NativeActivationGlobalDropFanout::Dispatch dispatch = fanout.acquire();
        CHECK(dispatch);
        dispatch.notify_pre(NativeActivationGlobalDropCohort{1U, 2U, 3U, true});
        dispatch.notify_post(NativeActivationGlobalDropCohort{1U, 2U, 3U, true});
    }
    fanout.quiesce();
    CHECK(fanout.unregister_observer(republished));
}

struct EventDrainContext final {
    std::array<NativeActivationEvent, 8U> events{};
    std::size_t count{};
};

struct ReentrantEventDrainContext final {
    EventDrainContext collected{};
    NativeActivationEventPushResult projectionEnqueue{
        NativeActivationEventPushResult::invalid};
};

void collect_native_activation_event(void* opaque,
                                     const NativeActivationEvent& event) noexcept {
    auto& context = *static_cast<EventDrainContext*>(opaque);
    if (context.count < context.events.size()) {
        context.events[context.count++] = event;
    }
}

void discard_native_activation_event(void*, const NativeActivationEvent&) noexcept {}

void collect_and_enqueue_native_activation_event(
    void* opaque,
    const NativeActivationEvent& event) noexcept {
    auto& context = *static_cast<ReentrantEventDrainContext*>(opaque);
    collect_native_activation_event(&context.collected, event);
    NativeActivationEvent projected{};
    projected.kind = NativeActivationEventKind::activation;
    projected.identity = 0xD4U;
    context.projectionEnqueue = detail::enqueue_native_activation_event(projected);
}

void hook_event_projection_is_scalar_and_preserves_forwarding_results() {
    auto registry = std::make_unique<NativeActivationRegistry>();
    CHECK(registry->begin_module().status == BeginModuleStatus::started);
    const NativeActivationAttempt attempt = make_attempt(
        0xA100U, 0xA10000A1U, 0xAABBCCDDEEFF0011ULL);

    std::uint32_t activationForwards = 0U;
    const ActivationFlowResult activation = observe_activation(
        *registry,
        attempt,
        [&]() noexcept {
            ++activationForwards;
            return std::uint8_t{0x7FU};
        },
        []() noexcept { return true; },
        []() noexcept { return true; });
    CHECK(activationForwards == 1U);
    const NativeActivationEvent activationEvent =
        detail::make_native_activation_event(attempt, activation);
    CHECK(activationEvent.sequence == 0U);
    CHECK(activationEvent.kind == NativeActivationEventKind::activation);
    CHECK(activationEvent.closePath == NativeActivationClosePath::none);
    CHECK(activationEvent.nativeResult == 0x7FU);
    CHECK(activationEvent.postconditionObserved);
    CHECK(activationEvent.publishStatus == PublishStatus::published);
    CHECK(activationEvent.observedWrapper == attempt.wrapper);
    CHECK(activationEvent.publicationWrapper == attempt.wrapper);
    CHECK(activationEvent.observedFullHandle == attempt.fullHandle);
    CHECK(activationEvent.publicationFullHandle == attempt.fullHandle);
    CHECK(activationEvent.identity == attempt.identity);
    CHECK(activationEvent.identityValid == attempt.identityValid);
    CHECK(activationEvent.validateHost == attempt.validateHost);
    CHECK(activationEvent.mode == attempt.mode);
    CHECK(activationEvent.module == activation.publication.snapshot.key.module);
    CHECK(activationEvent.activation == activation.publication.snapshot.key.generation);

    std::uint32_t closeForwards = 0U;
    const CloseFlowResult close = observe_close(
        *registry,
        attempt.wrapper,
        attempt.fullHandle,
        true,
        [&]() noexcept { ++closeForwards; },
        []() noexcept { return true; });
    CHECK(closeForwards == 1U);
    const NativeActivationEvent closeEvent = detail::make_native_activation_close_event(
        NativeActivationClosePath::reinstantiate,
        attempt.wrapper,
        attempt.fullHandle,
        close);
    CHECK(closeEvent.sequence == 0U);
    CHECK(closeEvent.kind == NativeActivationEventKind::close);
    CHECK(closeEvent.closePath == NativeActivationClosePath::reinstantiate);
    CHECK(closeEvent.captureStatus == CaptureStatus::captured);
    CHECK(closeEvent.retireStatus == RetireStatus::retired);
    CHECK(closeEvent.observedWrapper == attempt.wrapper);
    CHECK(closeEvent.publicationWrapper == attempt.wrapper);
    CHECK(closeEvent.observedFullHandle == attempt.fullHandle);
    CHECK(closeEvent.publicationFullHandle == attempt.fullHandle);
    CHECK(closeEvent.identity == attempt.identity);
    CHECK(closeEvent.identityValid == attempt.identityValid);
    CHECK(closeEvent.module == activationEvent.module);
    CHECK(closeEvent.activation == activationEvent.activation);
}

void event_queue_is_bounded_reports_loss_and_drains_in_sequence() {
    NativeActivationEvent event{};
    event.kind = NativeActivationEventKind::activation;
    event.observedWrapper = 0xB100U;
    event.identity = 0xB1U;

    NativeActivationEventQueue<2U> queue{};
    NativeActivationEvent invalid{};
    CHECK(queue.try_push(invalid) == NativeActivationEventPushResult::invalid);
    CHECK(queue.try_push(event) == NativeActivationEventPushResult::enqueued);
    event.identity = 0xB2U;
    CHECK(queue.try_push(event) == NativeActivationEventPushResult::enqueued);
    event.identity = 0xB3U;
    CHECK(queue.try_push(event) == NativeActivationEventPushResult::full);

    NativeActivationEventQueueCounters counters = queue.counters();
    CHECK(counters.enqueued == 2U);
    CHECK(counters.invalid == 1U);
    CHECK(counters.droppedFull == 1U);
    CHECK(counters.pending == 2U);
    CHECK(counters.highWater == 2U);
    CHECK(counters.losses() == 2U);

    NativeActivationEvent output{};
    CHECK(queue.try_pop(output) == NativeActivationEventPopResult::success);
    CHECK(output.sequence == 1U);
    CHECK(output.identity == 0xB1U);
    CHECK(queue.try_pop(output) == NativeActivationEventPopResult::success);
    CHECK(output.sequence == 2U);
    CHECK(output.identity == 0xB2U);
    CHECK(queue.try_pop(output) == NativeActivationEventPopResult::empty);
    counters = queue.counters();
    CHECK(counters.drained == 2U);
    CHECK(counters.pending == 0U);
    CHECK(counters.lastSequence == 2U);

    NativeActivationEventQueue<2U> busyQueue{};
    CHECK(busyQueue.testing_lock());
    CHECK(busyQueue.try_push(event) == NativeActivationEventPushResult::busy);
    CHECK(busyQueue.try_pop(output) == NativeActivationEventPopResult::busy);
    busyQueue.testing_unlock();
    counters = busyQueue.counters();
    CHECK(counters.droppedBusy == 1U);
    CHECK(counters.drainBusy == 1U);
    CHECK(counters.losses() == 1U);

    NativeActivationEventQueue<2U> exhaustingQueue{};
    exhaustingQueue.testing_set_next_sequence(
        (std::numeric_limits<std::uint64_t>::max)() - 1U);
    CHECK(exhaustingQueue.try_push(event) == NativeActivationEventPushResult::enqueued);
    CHECK(exhaustingQueue.try_pop(output) == NativeActivationEventPopResult::success);
    CHECK(output.sequence == (std::numeric_limits<std::uint64_t>::max)() - 1U);
    CHECK(exhaustingQueue.try_push(event)
          == NativeActivationEventPushResult::sequenceExhausted);
    CHECK(exhaustingQueue.counters().droppedSequenceExhausted == 1U);
}

void event_queue_contention_is_nonwaiting_and_accounts_every_attempt() {
    constexpr std::size_t kProducerCount = 8U;
    constexpr std::size_t kAttemptsPerProducer = 128U;
    constexpr std::size_t kAttemptCount = kProducerCount * kAttemptsPerProducer;
    NativeActivationEventQueue<32U> queue{};
    std::latch start{1};
    std::atomic<std::size_t> accepted{};
    std::array<std::thread, kProducerCount> producers{};
    for (std::size_t producer = 0U; producer < producers.size(); ++producer) {
        producers[producer] = std::thread([&, producer]() noexcept {
            NativeActivationEvent event{};
            event.kind = NativeActivationEventKind::activation;
            event.observedWrapper = 0xC000U + producer;
            start.wait();
            for (std::size_t attempt = 0U; attempt < kAttemptsPerProducer; ++attempt) {
                event.identity = producer * kAttemptsPerProducer + attempt;
                if (queue.try_push(event) == NativeActivationEventPushResult::enqueued) {
                    accepted.fetch_add(1U, std::memory_order_relaxed);
                }
            }
        });
    }
    start.count_down();
    for (std::thread& producer : producers) {
        producer.join();
    }

    const NativeActivationEventQueueCounters beforeDrain = queue.counters();
    CHECK(beforeDrain.enqueued == accepted.load(std::memory_order_relaxed));
    CHECK(beforeDrain.pending == beforeDrain.enqueued);
    CHECK(beforeDrain.enqueued + beforeDrain.losses() == kAttemptCount);
    CHECK(beforeDrain.droppedBusy + beforeDrain.droppedFull != 0U);
    CHECK(beforeDrain.invalid == 0U);
    CHECK(beforeDrain.droppedSequenceExhausted == 0U);

    std::uint64_t previousSequence = 0U;
    std::size_t drained = 0U;
    for (;;) {
        NativeActivationEvent output{};
        const NativeActivationEventPopResult result = queue.try_pop(output);
        if (result == NativeActivationEventPopResult::empty) {
            break;
        }
        CHECK(result == NativeActivationEventPopResult::success);
        if (result != NativeActivationEventPopResult::success) {
            continue;
        }
        CHECK(output.sequence > previousSequence);
        previousSequence = output.sequence;
        ++drained;
    }
    CHECK(drained == accepted.load(std::memory_order_relaxed));
    CHECK(queue.counters().pending == 0U);
}

void global_event_queue_has_an_explicit_bounded_off_hook_drain() {
    (void)drain_native_activation_events(
        &discard_native_activation_event,
        nullptr,
        (std::numeric_limits<std::size_t>::max)());
    const NativeActivationEventQueueCounters before = native_activation_event_counters();

    NativeActivationEvent first{};
    first.kind = NativeActivationEventKind::activation;
    first.identity = 0xD1U;
    NativeActivationEvent second = first;
    second.kind = NativeActivationEventKind::close;
    second.closePath = NativeActivationClosePath::cleanup;
    second.identity = 0xD2U;
    NativeActivationEvent third = first;
    third.identity = 0xD3U;
    CHECK(detail::enqueue_native_activation_event(first)
          == NativeActivationEventPushResult::enqueued);
    CHECK(detail::enqueue_native_activation_event(second)
          == NativeActivationEventPushResult::enqueued);
    CHECK(detail::enqueue_native_activation_event(third)
          == NativeActivationEventPushResult::enqueued);
    CHECK(drain_native_activation_events(nullptr, nullptr, 3U) == 0U);

    ReentrantEventDrainContext reentrant{};
    CHECK(drain_native_activation_events(
              &collect_and_enqueue_native_activation_event, &reentrant, 1U)
          == 1U);
    CHECK(reentrant.collected.count == 1U);
    CHECK(reentrant.collected.events[0].identity == 0xD1U);
    CHECK(reentrant.projectionEnqueue == NativeActivationEventPushResult::enqueued);

    EventDrainContext context{};
    CHECK(drain_native_activation_events(&collect_native_activation_event, &context, 0U) == 0U);
    CHECK(drain_native_activation_events(&collect_native_activation_event, &context, 8U) == 3U);
    CHECK(context.count == 3U);
    CHECK(context.events[0].identity == 0xD2U);
    CHECK(context.events[1].identity == 0xD3U);
    CHECK(context.events[2].identity == 0xD4U);
    CHECK(reentrant.collected.events[0].sequence < context.events[0].sequence);
    CHECK(context.events[0].sequence < context.events[1].sequence);
    CHECK(context.events[1].sequence < context.events[2].sequence);

    const NativeActivationEventQueueCounters after = native_activation_event_counters();
    CHECK(after.enqueued == before.enqueued + 4U);
    CHECK(after.drained == before.drained + 4U);
    CHECK(after.pending == 0U);
}

} // namespace

int main() {
    activation_original_runs_before_publication_and_both_success_gates_apply();
    close_quiesces_before_original_and_retires_after_original();
    nested_same_wrapper_reopen_survives_outer_stale_retirement();
    global_drop_quiesces_exact_cohort_and_preserves_nested_successor();
    global_drop_uses_all_bounded_slots_and_old_module_token_is_stale();
    stale_close_and_aba_tokens_cannot_retire_a_successor();
    active_close_mismatch_fails_closed_without_retiring_a_successor();
    exact_wrapper_and_full_handle_are_required_beyond_the_masked_slot();
    second_success_without_a_close_is_an_impossible_current_conflict();
    all_8192_slots_fill_without_eviction_and_the_next_collision_fails_closed();
    generation_exhaustion_never_wraps_and_disables_mutation();
    impossible_success_arguments_disable_the_current_module();
    module_restart_changes_owner_and_preserves_the_process_activation_clock();
    packed_identity_and_mapped_prefix_validation_are_distinct_and_retryable();
    batch_model_enforces_all_or_none_publication_and_retained_detach();
    call_gate_covers_publication_wait_and_nested_in_flight_detach();
    global_drop_fanout_is_generation_safe_nonblocking_and_quiescent();
    global_drop_fanout_stale_unregister_cannot_race_reserved_publication();
    hook_event_projection_is_scalar_and_preserves_forwarding_results();
    event_queue_is_bounded_reports_loss_and_drains_in_sequence();
    event_queue_contention_is_nonwaiting_and_accounts_every_attempt();
    global_event_queue_has_an_explicit_bounded_off_hook_drain();

    if (g_failureCount != 0) {
        std::cerr << g_failureCount << " native activation lifecycle check(s) failed\n";
        return 1;
    }
    std::cout << "all native activation lifecycle checks passed\n";
    return 0;
}
