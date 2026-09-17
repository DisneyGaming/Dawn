#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <span>
#include <thread>
#include <vector>

#include "client/hooks/bootflow/opening_authority/activity_authority_receive_owner.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

namespace owner =
    dawn::client::hooks::bootflow::opening_authority::activity_authority_receive_owner;
namespace hooking = dawn::client::hooking;

namespace {

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            std::cerr << "check failed: " #condition " at line " << __LINE__ << '\n';              \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)

struct VirtualBytes final {
    explicit VirtualBytes(std::size_t bytes)
        : size(bytes), data(static_cast<std::byte*>(VirtualAlloc(
                           nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE))) {
        CHECK(data != nullptr);
    }

    ~VirtualBytes() {
        if (data != nullptr) {
            CHECK(VirtualFree(data, 0U, MEM_RELEASE) != FALSE);
        }
    }

    VirtualBytes(const VirtualBytes&) = delete;
    VirtualBytes& operator=(const VirtualBytes&) = delete;

    std::size_t size{};
    std::byte* data{};
};

struct ImageFixture final {
    ImageFixture() : mapped(0x08A5EA00U), packed(122'984'224U) {
        std::memcpy(mapped.data + owner::kTargetRva,
                    owner::kExpectedMappedPrefix.data(),
                    owner::kPrefixBytes);
        std::memcpy(packed.data + owner::kPackedRawOffset,
                    owner::kExpectedPackedPrefix.data(),
                    owner::kPrefixBytes);
    }

    [[nodiscard]] owner::ImageView view() const noexcept {
        owner::ImageView output{};
        output.mapped = {mapped.data, mapped.size};
        output.packed = {packed.data, packed.size};
        output.packedSha256 = owner::kPinnedPackedImageSha256;
        output.packedFileSize = packed.size;
        output.machine = 0x8664U;
        output.sectionCount = 11U;
        output.timestamp = 0x5F43138BU;
        output.imageSize = 0x08A5EA00U;
        output.entryRva = 0x0187CDD8U;
        output.checksum = 0x0755867CU;
        output.codeViewGuid = owner::kPinnedCodeViewGuid;
        output.codeViewAge = 1U;
        return output;
    }

    VirtualBytes mapped;
    VirtualBytes packed;
};

enum class CallbackEvent : std::uint8_t {
    heap_attached,
    heap_quiescing,
    heap_detached,
    heap_pre,
    scene_pre,
    native,
    heap_post,
    scene_post,
};

struct FanoutState final {
    std::array<std::atomic<std::uint32_t>, 8U> eventCounts{};
    std::array<CallbackEvent, 64U> order{};
    std::atomic<std::size_t> orderSize{};
    std::atomic<std::uint64_t> heapPre{};
    std::atomic<std::uint64_t> heapPost{};
    std::atomic<std::uint64_t> scenePre{};
    std::atomic<std::uint64_t> scenePost{};
    std::atomic<std::uint64_t> stalePosts{};
    std::atomic<std::uint64_t> invalidWrappers{};
    std::atomic<std::uint64_t> lastCallId{};
    std::atomic<std::uintptr_t> lastTable{};
    std::atomic<std::uintptr_t> lastWrapper{};
    std::atomic<std::uintptr_t> lastStream{};
    std::atomic<std::uint64_t> lastResult{};
    std::atomic<std::uint32_t> attached{};
    std::atomic<std::uint32_t> quiescing{};
    std::atomic<std::uint32_t> detached{};
    std::atomic<std::uint32_t> deferred{};
    std::atomic_bool callbackDuringOriginal{};
    std::atomic_bool* insideOriginal{};
};

struct OriginalState final {
    std::atomic<std::uint64_t> calls{};
    std::atomic<std::uintptr_t> lastTable{};
    std::atomic<std::uintptr_t> lastStream{};
    std::atomic_bool inside{};
    std::atomic_bool block{};
    std::atomic_bool release{};
    std::atomic_bool zeroEpoch{};
    std::atomic_bool nestOnce{};
    std::atomic<std::uint64_t>* epoch{};
    std::uint64_t result{0xFEDCBA9876543201ULL};
};

std::atomic<OriginalState*> g_originalState{};
thread_local std::uint32_t g_originalDepth{};
thread_local std::array<std::uint64_t, 16U> g_sceneCallStack{};
thread_local std::size_t g_sceneCallDepth{};

[[nodiscard]] void* pointer(std::uintptr_t value) noexcept {
    return reinterpret_cast<void*>(value);
}

void record_event(FanoutState& state, CallbackEvent event) noexcept {
    if (state.insideOriginal != nullptr && state.insideOriginal->load(std::memory_order_acquire)) {
        state.callbackDuringOriginal.store(true, std::memory_order_release);
    }
    state.eventCounts[static_cast<std::size_t>(event)].fetch_add(1U, std::memory_order_relaxed);
    const std::size_t index = state.orderSize.fetch_add(1U, std::memory_order_relaxed);
    if (index < state.order.size()) {
        state.order[index] = event;
    }
}

void heap_enter(void* context,
                owner::heap_cohort::ReceiveToken& token,
                void* sensorTable,
                void* bitStream) noexcept {
    auto& state = *static_cast<FanoutState*>(context);
    token.active = true;
    token.sensorTable = reinterpret_cast<std::uintptr_t>(sensorTable);
    token.stream = reinterpret_cast<std::uintptr_t>(bitStream);
    state.heapPre.fetch_add(1U, std::memory_order_relaxed);
    record_event(state, CallbackEvent::heap_pre);
}

void heap_exit(void* context,
               owner::heap_cohort::ReceiveToken& token,
               std::uint64_t nativeResult) noexcept {
    auto& state = *static_cast<FanoutState*>(context);
    CHECK(token.active);
    token.active = false;
    token.nativeResult = nativeResult;
    state.lastResult.store(nativeResult, std::memory_order_relaxed);
    state.heapPost.fetch_add(1U, std::memory_order_relaxed);
    record_event(state, CallbackEvent::heap_post);
}

void heap_attached(void* context) noexcept {
    auto& state = *static_cast<FanoutState*>(context);
    state.attached.fetch_add(1U, std::memory_order_relaxed);
    record_event(state, CallbackEvent::heap_attached);
}

void heap_quiescing(void* context) noexcept {
    auto& state = *static_cast<FanoutState*>(context);
    state.quiescing.fetch_add(1U, std::memory_order_relaxed);
    record_event(state, CallbackEvent::heap_quiescing);
}

void heap_detached(void* context, bool removed) noexcept {
    auto& state = *static_cast<FanoutState*>(context);
    if (removed) {
        state.detached.fetch_add(1U, std::memory_order_relaxed);
        record_event(state, CallbackEvent::heap_detached);
    } else {
        state.deferred.fetch_add(1U, std::memory_order_relaxed);
    }
}

void scene_pre(void* context, const owner::BoundaryObservation& observation) noexcept {
    auto& state = *static_cast<FanoutState*>(context);
    CHECK(observation.phase == owner::BoundaryPhase::pre);
    CHECK(observation.ownerGeneration != 0U);
    CHECK(observation.aggregateEpoch != 0U);
    CHECK(observation.callId != 0U);
    CHECK(observation.entryGenerationCurrent);
    CHECK(!observation.exitGenerationCurrent);
    CHECK(!observation.nativeResultValid);
    CHECK(g_sceneCallDepth < g_sceneCallStack.size());
    g_sceneCallStack[g_sceneCallDepth++] = observation.callId;
    state.lastCallId.store(observation.callId, std::memory_order_relaxed);
    state.lastTable.store(observation.sensorTable, std::memory_order_relaxed);
    state.lastWrapper.store(observation.activityWrapper, std::memory_order_relaxed);
    state.lastStream.store(observation.bitStream, std::memory_order_relaxed);
    state.invalidWrappers.fetch_add(observation.wrapperValid ? 0U : 1U, std::memory_order_relaxed);
    state.scenePre.fetch_add(1U, std::memory_order_relaxed);
    record_event(state, CallbackEvent::scene_pre);
}

void scene_post(void* context, const owner::BoundaryObservation& observation) noexcept {
    auto& state = *static_cast<FanoutState*>(context);
    CHECK(observation.phase == owner::BoundaryPhase::post);
    CHECK(observation.nativeResultValid);
    CHECK(g_sceneCallDepth != 0U);
    CHECK(g_sceneCallStack[g_sceneCallDepth - 1U] == observation.callId);
    --g_sceneCallDepth;
    state.lastResult.store(observation.nativeResult, std::memory_order_relaxed);
    state.stalePosts.fetch_add(observation.exitGenerationCurrent ? 0U : 1U,
                               std::memory_order_relaxed);
    state.scenePost.fetch_add(1U, std::memory_order_relaxed);
    record_event(state, CallbackEvent::scene_post);
}

std::uint64_t __fastcall native_original(void* sensorTable, void* bitStream) noexcept {
    OriginalState* const state = g_originalState.load(std::memory_order_acquire);
    CHECK(state != nullptr);
    state->calls.fetch_add(1U, std::memory_order_relaxed);
    state->lastTable.store(reinterpret_cast<std::uintptr_t>(sensorTable),
                           std::memory_order_relaxed);
    state->lastStream.store(reinterpret_cast<std::uintptr_t>(bitStream), std::memory_order_relaxed);
    state->inside.store(true, std::memory_order_release);

    if (state->nestOnce.load(std::memory_order_acquire) && g_originalDepth == 0U) {
        ++g_originalDepth;
        CHECK(owner::testing::invoke(sensorTable, bitStream) == state->result);
        --g_originalDepth;
    }
    if (state->zeroEpoch.load(std::memory_order_acquire) && state->epoch != nullptr) {
        state->epoch->store(0U, std::memory_order_release);
    }
    while (state->block.load(std::memory_order_acquire)
           && !state->release.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    state->inside.store(false, std::memory_order_release);
    return state->result;
}

struct PreparedOwner final {
    std::atomic<std::uint64_t> epoch{};
    FanoutState fanoutState{};
    OriginalState originalState{};
    owner::FanoutV1 fanout{};
    owner::ValidatedTarget target{};
    std::uint64_t generation{17U};
    std::uint64_t expectedEpoch{29U};

    explicit PreparedOwner(const owner::ValidatedTarget& validated) : target(validated) {
        fanoutState.insideOriginal = &originalState.inside;
        originalState.epoch = &epoch;
        fanout.publicationGeneration = generation;
        fanout.heapContext = &fanoutState;
        fanout.heapEnter = &heap_enter;
        fanout.heapExit = &heap_exit;
        fanout.heapOwnerAttached = &heap_attached;
        fanout.heapOwnerQuiescing = &heap_quiescing;
        fanout.heapOwnerDetached = &heap_detached;
        fanout.sceneContext = &fanoutState;
        fanout.scenePre = &scene_pre;
        fanout.scenePost = &scene_post;
        g_originalState.store(&originalState, std::memory_order_release);
    }

    [[nodiscard]] owner::Publication publication() noexcept {
        return {target, &fanout, &epoch, generation, expectedEpoch};
    }

    void prepare_and_publish() noexcept {
        CHECK(owner::prepare_publication(publication()) == owner::PrepareResult::prepared);
        std::array<hooking::detour::Spec, 2U> specs{};
        std::size_t used = 0U;
        CHECK(owner::append_specs(specs, used));
        CHECK(used == owner::kParticipantSpecCount);
        CHECK(specs[0].target == target.address);
        CHECK(specs[0].replacement == owner::replacement_entry());
        owner::publish_original(&native_original);
        CHECK(owner::accept());
    }

    void open() noexcept {
        epoch.store(expectedEpoch, std::memory_order_release);
    }

    void remove() noexcept {
        epoch.store(0U, std::memory_order_release);
        owner::quiesce();
        CHECK(owner::idle());
        CHECK(owner::clear_after_removed());
        CHECK(!owner::has_ownership());
        g_originalState.store(nullptr, std::memory_order_release);
    }
};

void test_contract_and_validation(ImageFixture& image, owner::ValidatedTarget& target) {
    CHECK(owner::kParticipantSpecCount == 1U);
    CHECK(owner::kTargetRva == 0x4D7470U);
    CHECK(owner::kPackedRawOffset == 0x4D6A70U);
    CHECK(owner::kExpectedMappedPrefix.size() == 16U);
    CHECK(owner::kExpectedPackedPrefix.size() == 16U);
    CHECK(owner::validate(image.view(), target) == owner::ValidationResult::valid);
    CHECK(target.valid);
    CHECK(target.address == image.mapped.data + owner::kTargetRva);
    CHECK(target.observedMapped == owner::kExpectedMappedPrefix);
    CHECK(target.observedPacked == owner::kExpectedPackedPrefix);

    owner::ImageView view = image.view();
    view.packedSha256[31] ^= std::byte{1U};
    owner::ValidatedTarget rejected{target};
    CHECK(owner::validate(view, rejected) == owner::ValidationResult::packed_hash_mismatch);
    CHECK(!rejected.valid && rejected.address == nullptr);

    image.mapped.data[owner::kTargetRva + 15U] ^= std::byte{1U};
    CHECK(owner::validate(image.view(), rejected)
          == owner::ValidationResult::mapped_prefix_mismatch);
    CHECK(!rejected.valid && rejected.observedMapped == decltype(rejected.observedMapped){});
    image.mapped.data[owner::kTargetRva + 15U] ^= std::byte{1U};

    image.packed.data[owner::kPackedRawOffset] ^= std::byte{1U};
    CHECK(owner::validate(image.view(), rejected)
          == owner::ValidationResult::packed_prefix_mismatch);
    CHECK(!rejected.valid && rejected.observedPacked == decltype(rejected.observedPacked){});
    image.packed.data[owner::kPackedRawOffset] ^= std::byte{1U};

    view = image.view();
    view.codeViewAge = 2U;
    CHECK(owner::validate(view, rejected) == owner::ValidationResult::codeview_mismatch);
    view = image.view();
    view.timestamp ^= 1U;
    CHECK(owner::validate(view, rejected) == owner::ValidationResult::pe_mismatch);
    view = image.view();
    view.mapped = view.mapped.first(owner::kTargetRva + owner::kPrefixBytes - 1U);
    CHECK(owner::validate(view, rejected) == owner::ValidationResult::target_bounds);
}

void test_prepare_rejections(const owner::ValidatedTarget& target) {
    std::atomic<std::uint64_t> epoch{};
    FanoutState state{};
    owner::FanoutV1 fanout{};
    fanout.publicationGeneration = 7U;
    fanout.heapContext = &state;
    fanout.heapEnter = &heap_enter;
    fanout.heapExit = &heap_exit;
    fanout.heapOwnerAttached = &heap_attached;
    fanout.heapOwnerQuiescing = &heap_quiescing;
    fanout.heapOwnerDetached = &heap_detached;
    fanout.sceneContext = &state;
    fanout.scenePre = &scene_pre;
    fanout.scenePost = &scene_post;

    owner::Publication publication{target, &fanout, &epoch, 8U, 9U};
    CHECK(owner::prepare_publication(publication) == owner::PrepareResult::invalid_fanout);
    fanout.publicationGeneration = 8U;
    fanout.scenePost = nullptr;
    CHECK(owner::prepare_publication(publication) == owner::PrepareResult::invalid_fanout);
    fanout.scenePost = &scene_post;
    epoch.store(1U, std::memory_order_release);
    CHECK(owner::prepare_publication(publication)
          == owner::PrepareResult::aggregate_already_admitting);
    epoch.store(0U, std::memory_order_release);
    CHECK(owner::prepare_publication(publication) == owner::PrepareResult::prepared);
    CHECK(owner::has_ownership());
    CHECK(owner::cancel_before_attach());
    CHECK(!owner::has_ownership());
}

void test_exact_forward_and_order(const owner::ValidatedTarget& target) {
    PreparedOwner fixture{target};
    fixture.prepare_and_publish();

    void* const table = pointer(0x1028U);
    void* const stream = pointer(0xABCDEF00U);
    CHECK(owner::testing::invoke(table, stream) == fixture.originalState.result);
    CHECK(fixture.originalState.calls.load() == 1U);
    CHECK(fixture.fanoutState.heapPre.load() == 0U);
    CHECK(fixture.fanoutState.scenePre.load() == 0U);

    fixture.open();
    CHECK(owner::testing::invoke(table, stream) == 0xFEDCBA9876543201ULL);
    CHECK(fixture.originalState.calls.load() == 2U);
    CHECK(fixture.originalState.lastTable.load() == reinterpret_cast<std::uintptr_t>(table));
    CHECK(fixture.originalState.lastStream.load() == reinterpret_cast<std::uintptr_t>(stream));
    CHECK(fixture.fanoutState.lastTable.load() == reinterpret_cast<std::uintptr_t>(table));
    CHECK(fixture.fanoutState.lastWrapper.load() == 0x1000U);
    CHECK(fixture.fanoutState.lastStream.load() == reinterpret_cast<std::uintptr_t>(stream));
    CHECK(fixture.fanoutState.lastResult.load() == 0xFEDCBA9876543201ULL);
    CHECK(!fixture.fanoutState.callbackDuringOriginal.load());

    CHECK(fixture.fanoutState.orderSize.load() == 5U);
    CHECK(fixture.fanoutState.order[0] == CallbackEvent::heap_attached);
    CHECK(fixture.fanoutState.order[1] == CallbackEvent::heap_pre);
    CHECK(fixture.fanoutState.order[2] == CallbackEvent::scene_pre);
    CHECK(fixture.fanoutState.order[3] == CallbackEvent::heap_post);
    CHECK(fixture.fanoutState.order[4] == CallbackEvent::scene_post);

    const owner::Counters counts = owner::counters();
    CHECK(counts.entered == 2U && counts.forwarded == 2U);
    CHECK(counts.admitted == 1U && counts.pairedPost == 1U);
    CHECK(counts.staleAtExit == 0U);
    fixture.remove();
    CHECK(fixture.fanoutState.attached.load() == 1U);
    CHECK(fixture.fanoutState.quiescing.load() == 1U);
    CHECK(fixture.fanoutState.detached.load() == 1U);
}

void test_wrapper_underflow_and_exit_revalidation(const owner::ValidatedTarget& target) {
    PreparedOwner fixture{target};
    fixture.prepare_and_publish();
    fixture.open();
    fixture.originalState.zeroEpoch.store(true, std::memory_order_release);
    CHECK(owner::testing::invoke(pointer(0x20U), pointer(0x40U)) == fixture.originalState.result);
    CHECK(fixture.fanoutState.invalidWrappers.load() == 1U);
    CHECK(fixture.fanoutState.lastWrapper.load() == 0U);
    CHECK(fixture.fanoutState.heapPre.load() == 1U);
    CHECK(fixture.fanoutState.heapPost.load() == 1U);
    CHECK(fixture.fanoutState.scenePre.load() == 1U);
    CHECK(fixture.fanoutState.scenePost.load() == 1U);
    CHECK(fixture.fanoutState.stalePosts.load() == 1U);
    const owner::Counters counts = owner::counters();
    CHECK(counts.invalidWrapper == 1U && counts.staleAtExit == 1U);
    fixture.remove();
}

void test_commit_window(const owner::ValidatedTarget& target) {
    PreparedOwner fixture{target};
    CHECK(owner::prepare_publication(fixture.publication()) == owner::PrepareResult::prepared);
    std::array<hooking::detour::Spec, 1U> specs{};
    std::size_t used = 0U;
    CHECK(owner::append_specs(specs, used));

    std::atomic<std::uint64_t> result{};
    std::thread caller([&]() {
        result.store(owner::testing::invoke(pointer(0x2028U), pointer(0x99U)),
                     std::memory_order_release);
    });
    while (owner::counters().entered == 0U) {
        std::this_thread::yield();
    }
    CHECK(fixture.originalState.calls.load() == 0U);
    owner::publish_original(&native_original);
    caller.join();
    CHECK(result.load(std::memory_order_acquire) == fixture.originalState.result);
    CHECK(fixture.originalState.calls.load() == 1U);
    CHECK(fixture.fanoutState.heapPre.load() == 0U);
    owner::quiesce();
    CHECK(owner::clear_after_removed());
    g_originalState.store(nullptr, std::memory_order_release);
}

void test_full_call_gate_retention(const owner::ValidatedTarget& target) {
    PreparedOwner fixture{target};
    fixture.prepare_and_publish();
    fixture.open();
    fixture.originalState.block.store(true, std::memory_order_release);

    std::thread caller([&]() {
        CHECK(owner::testing::invoke(pointer(0x3028U), pointer(0x55U))
              == fixture.originalState.result);
    });
    while (!fixture.originalState.inside.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    fixture.epoch.store(0U, std::memory_order_release);
    owner::quiesce();
    CHECK(!owner::accepting());
    CHECK(!owner::idle());
    CHECK(!owner::clear_after_removed());
    owner::retain_after_failed_remove();
    CHECK(fixture.fanoutState.deferred.load() == 1U);
    CHECK(owner::has_ownership());
    fixture.originalState.release.store(true, std::memory_order_release);
    caller.join();
    CHECK(owner::idle());
    CHECK(fixture.fanoutState.heapPre.load() == fixture.fanoutState.heapPost.load());
    CHECK(fixture.fanoutState.scenePre.load() == fixture.fanoutState.scenePost.load());
    CHECK(fixture.fanoutState.stalePosts.load() == 1U);
    CHECK(owner::clear_after_removed());
    g_originalState.store(nullptr, std::memory_order_release);
}

void test_nested_pairing(const owner::ValidatedTarget& target) {
    PreparedOwner fixture{target};
    fixture.prepare_and_publish();
    fixture.open();
    fixture.originalState.nestOnce.store(true, std::memory_order_release);
    CHECK(owner::testing::invoke(pointer(0x4028U), pointer(0x66U)) == fixture.originalState.result);
    CHECK(fixture.originalState.calls.load() == 2U);
    CHECK(fixture.fanoutState.heapPre.load() == 2U);
    CHECK(fixture.fanoutState.heapPost.load() == 2U);
    CHECK(fixture.fanoutState.scenePre.load() == 2U);
    CHECK(fixture.fanoutState.scenePost.load() == 2U);
    CHECK(g_sceneCallDepth == 0U);
    fixture.remove();
}

void test_concurrency(const owner::ValidatedTarget& target) {
    PreparedOwner fixture{target};
    fixture.prepare_and_publish();
    fixture.open();

    constexpr std::size_t kThreadCount = 8U;
    constexpr std::size_t kCallsPerThread = 2'000U;
    std::vector<std::thread> threads;
    threads.reserve(kThreadCount);
    for (std::size_t thread = 0U; thread < kThreadCount; ++thread) {
        threads.emplace_back([&, thread]() {
            for (std::size_t call = 0U; call < kCallsPerThread; ++call) {
                void* const table = pointer(0x5028U + thread * 0x100U);
                void* const stream = pointer(0x1000U + call);
                CHECK(owner::testing::invoke(table, stream) == fixture.originalState.result);
            }
            CHECK(g_sceneCallDepth == 0U);
        });
    }
    for (std::thread& thread : threads) {
        thread.join();
    }
    constexpr std::uint64_t expected = kThreadCount * kCallsPerThread;
    CHECK(fixture.originalState.calls.load() == expected);
    CHECK(fixture.fanoutState.heapPre.load() == expected);
    CHECK(fixture.fanoutState.heapPost.load() == expected);
    CHECK(fixture.fanoutState.scenePre.load() == expected);
    CHECK(fixture.fanoutState.scenePost.load() == expected);
    const owner::Counters counts = owner::counters();
    CHECK(counts.entered == expected && counts.forwarded == expected);
    CHECK(counts.admitted == expected && counts.pairedPost == expected);
    fixture.remove();
}

} // namespace

int main() {
    ImageFixture image{};
    owner::ValidatedTarget target{};
    test_contract_and_validation(image, target);
    test_prepare_rejections(target);
    test_exact_forward_and_order(target);
    test_wrapper_underflow_and_exit_revalidation(target);
    test_commit_window(target);
    test_full_call_gate_retention(target);
    test_nested_pairing(target);
    test_concurrency(target);
    std::cout << "all activity authority receive-owner checks passed\n";
    return 0;
}
