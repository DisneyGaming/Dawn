#define SUNRISE_SENSOR_HEAP_TEST 1

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string_view>
#include <thread>
#include <type_traits>

#include "client/hooks/network/lifecycle/sensor_state_heap_ring.h"

namespace {

using namespace sunrise::client::hooks::network::lifecycle::sensor_state_heap;

template <typename Value>
concept HasNetworkGeneration = requires(Value value) { value.networkGeneration; };
template <typename Value>
concept HasActivityGeneration = requires(Value value) { value.activityGeneration; };
template <typename Value>
concept HasConnectionGeneration = requires(Value value) { value.connectionGeneration; };
template <typename Value>
concept HasRegionGeneration = requires(Value value) { value.regionGeneration; };
template <typename Value>
concept HasDropGeneration = requires(Value value) { value.dropGeneration; };

static_assert(std::is_trivially_copyable_v<Event>);
static_assert(std::is_standard_layout_v<Event>);
static_assert(!HasNetworkGeneration<EventContext>);
static_assert(!HasActivityGeneration<EventContext>);
static_assert(!HasConnectionGeneration<EventContext>);
static_assert(!HasRegionGeneration<EventContext>);
static_assert(!HasDropGeneration<EventContext>);

static_assert(validDiagnosticGeneration == 0x01U);
static_assert(validRemoveOrdinal == 0x02U);
static_assert(validRecordGeneration == 0x04U);
static_assert(validPeerTable == 0x08U);
static_assert(validDatum == 0x10U);
static_assert(validConstructorIdentity == 0x20U);
static_assert(validCurrentSnapshot == 0x40U);
static_assert(validBaselineSnapshot == 0x80U);
static_assert(flagActiveRecordReuse == 0x01U);
static_assert(flagReuseAfterUncertainRetire == 0x02U);
static_assert(flagRelativePayloadChanged == 0x04U);
static_assert(flagSelectorChanged == 0x08U);
static_assert(flagCountChanged == 0x10U);
static_assert(flagPostDestroyNotZero == 0x20U);
static_assert(flagGuardedReadFailed == 0x40U);
static_assert((kSmokeWarningFlags & flagCountChanged) != 0U);

int g_failureCount = 0;

void check(bool condition, const char* expression, int line) {
    if (condition) {
        return;
    }
    std::cerr << __FILE__ << ':' << line << ": check failed: " << expression << '\n';
    ++g_failureCount;
}

#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

[[nodiscard]] Event event(std::uint64_t diagnosticGeneration,
                          std::uint32_t recordGeneration,
                          std::uintptr_t record) noexcept {
    Event value{};
    value.context.diagnosticGeneration = diagnosticGeneration;
    value.context.recordGeneration = recordGeneration;
    value.valid = validDiagnosticGeneration | validRecordGeneration;
    value.record = record;
    value.kind = EventKind::construct;
    value.phase = EventPhase::exit;
    return value;
}

[[nodiscard]] RecordMetadata metadata(std::uint32_t identity = 7U) noexcept {
    RecordMetadata result{};
    result.identity = SensorIdentity{identity, 3U, 11U};
    result.authSchema = 0x80800001U;
    result.senseSchema = 0x80800002U;
    result.identityValid = true;
    return result;
}

void schema_is_distinct_and_public_bits_are_exact() {
    CHECK(std::string_view{kSmokeEventName} == "sensor_heap_smoke");
    CHECK(std::string_view{kSmokeSchemaName} == "phase0_v1");
    CHECK(std::string_view{kSmokeTextPrefix}
          == "ev=sensor_heap_smoke schema=phase0_v1");
    CHECK(std::string_view{kSmokeEventName} != "sensor_heap");
    CHECK((kSmokeWarningFlags & flagCountChanged) != 0U);
    CHECK(kSensorTableInsertRva == 0x4D6EB0U);
    CHECK(kRecordConstructRva == 0x9FEE40U);
    CHECK(kSensorTableRemoveRva == 0x4D7C00U);
    CHECK(kRecordDestroyRva == 0x9FE2C0U);
    CHECK(kSensorTableInsertPrefix.size() == 16U);
    CHECK(kRecordConstructPrefix.size() == 16U);
    CHECK(kSensorTableRemovePrefix.size() == 16U);
    CHECK(kRecordDestroyPrefix.size() == 16U);
    CHECK(kPinnedPackedImageSha256.front() == std::byte{0x81U});
    CHECK(kPinnedPackedImageSha256.back() == std::byte{0xEDU});
}

void semantic_sensor_identity_ignores_source_padding() {
    constexpr SensorIdentity identity{0x44332211U, 0x55U, 0x8877U};
    constexpr std::uint64_t packed = pack_sensor_identity(identity);
    static_assert(packed == 0x8877005544332211ULL);
    CHECK(((packed >> 40U) & 0xFFU) == 0U);
    CHECK((packed & 0xFFFFFFFFULL) == identity.word);
    CHECK(((packed >> 32U) & 0xFFU) == identity.kind);
    CHECK(((packed >> 48U) & 0xFFFFU) == identity.index);

    CHECK(exact_heap_assert(kExactHeapAssertText));
    CHECK(!exact_heap_assert(nullptr));
    CHECK(!exact_heap_assert("index heap double-free? previous does not point back"));
    CHECK(!exact_heap_assert("index heap double-free? previous does not point back.!"));
}

void image_identity_and_prefixes_are_all_or_none() {
    std::array<std::byte, 32U> mapped{};
    mapped[4] = std::byte{0x10U};
    mapped[5] = std::byte{0x20U};
    mapped[16] = std::byte{0x30U};
    const std::array firstPrefix{std::byte{0x10U}, std::byte{0x20U}};
    const std::array secondPrefix{std::byte{0x30U}};
    const std::array contracts{
        TargetContract{4U, firstPrefix},
        TargetContract{16U, secondPrefix},
    };
    const ImageSha256 identity = kPinnedPackedImageSha256;
    std::array<std::uintptr_t, 2U> outputs{99U, 100U};
    const ImageView image{mapped, identity};

    CHECK(validate_image(image, identity, contracts, outputs) == ImageValidationResult::valid);
    CHECK(outputs[0] == reinterpret_cast<std::uintptr_t>(mapped.data() + 4U));
    CHECK(outputs[1] == reinterpret_cast<std::uintptr_t>(mapped.data() + 16U));

    ImageSha256 wrongIdentity = identity;
    wrongIdentity[0] ^= std::byte{1U};
    outputs = {99U, 100U};
    CHECK(validate_image(image, wrongIdentity, contracts, outputs)
          == ImageValidationResult::imageIdentityMismatch);
    CHECK((outputs == std::array<std::uintptr_t, 2U>{}));

    mapped[16] ^= std::byte{1U};
    outputs = {99U, 100U};
    CHECK(validate_image(image, identity, contracts, outputs)
          == ImageValidationResult::prefixMismatch);
    CHECK((outputs == std::array<std::uintptr_t, 2U>{}));

    const std::array outOfRange{TargetContract{31U, firstPrefix}};
    std::array<std::uintptr_t, 1U> oneOutput{99U};
    CHECK(validate_image(image, identity, outOfRange, oneOutput)
          == ImageValidationResult::targetOutOfRange);
    CHECK(oneOutput[0] == 0U);
}

void ring_rejects_invalid_stale_busy_and_exhausted_inputs() {
    FixedRing<4U> ring;
    CHECK(!ring.reset(0U));
    CHECK(ring.reset(7U));

    Event invalid = event(7U, 1U, 0x1000U);
    invalid.context.recordGeneration = 0U;
    CHECK(ring.try_push(invalid) == PushResult::invalid);
    CHECK(ring.try_push(event(8U, 1U, 0x1000U))
          == PushResult::staleDiagnosticGeneration);

    CHECK(ring.testing_lock());
    CHECK(ring.try_push(event(7U, 1U, 0x1000U)) == PushResult::busy);
    Event output{};
    CHECK(ring.try_pop(output) == PopResult::busy);
    ring.testing_unlock();

    ring.testing_set_next_sequence((std::numeric_limits<std::uint64_t>::max)());
    CHECK(ring.try_push(event(7U, 1U, 0x1000U)) == PushResult::sequenceExhausted);
    const RingCounters counters = ring.counters();
    CHECK(counters.invalid == 1U);
    CHECK(counters.staleDiagnosticGeneration == 1U);
    CHECK(counters.droppedBusy == 1U);
    CHECK(counters.droppedSequenceExhausted == 1U);
    CHECK(counters.losses() == 4U);
}

void ring_wraps_without_overwrite_and_tracks_freeze_boundary() {
    FixedRing<3U> ring;
    CHECK(ring.reset(3U));
    CHECK(ring.try_push(event(3U, 1U, 0x1000U)) == PushResult::committed);
    CHECK(ring.try_push(event(3U, 2U, 0x2000U)) == PushResult::committed);
    Event output{};
    CHECK(ring.try_pop(output) == PopResult::success);
    CHECK(output.sequence == 1U);
    CHECK(ring.try_push(event(3U, 3U, 0x3000U)) == PushResult::committed);
    CHECK(ring.try_push(event(3U, 4U, 0x4000U)) == PushResult::committed);
    CHECK(ring.try_push(event(3U, 5U, 0x5000U)) == PushResult::full);
    CHECK(ring.freeze(FreezeReason::exactHeapAssert));
    CHECK(!ring.freeze(FreezeReason::lifecycleStop));
    CHECK(ring.try_push(event(3U, 6U, 0x6000U)) == PushResult::frozen);

    CHECK(ring.try_pop(output) == PopResult::success);
    CHECK(output.sequence == 2U && output.record == 0x2000U);
    CHECK(ring.try_pop(output) == PopResult::success);
    CHECK(output.sequence == 3U && output.record == 0x3000U);
    CHECK(ring.try_pop(output) == PopResult::success);
    CHECK(output.sequence == 4U && output.record == 0x4000U);
    CHECK(ring.try_pop(output) == PopResult::empty);

    const RingCounters counters = ring.counters();
    CHECK(counters.committed == 4U);
    CHECK(counters.pending == 0U);
    CHECK(counters.highWater == 3U);
    CHECK(counters.lastCommittedSequence == 4U);
    CHECK(counters.frozenSequence == 4U);
    CHECK(counters.droppedFull == 1U);
    CHECK(counters.droppedFrozen == 1U);

    CHECK(ring.reset(4U));
    CHECK(ring.counters().committed == 0U);
    CHECK(ring.counters().losses() == 0U);
    CHECK(ring.frozen_reason() == FreezeReason::none);
}

void registry_retains_colliding_inactive_keys_and_reports_full() {
    FixedRegistry<2U> registry;
    registry.reset();
    RegistryToken first{};
    RegistryToken second{};
    RegistryToken third{};
    CHECK(registry.try_begin(0x10U, metadata(1U), first) == RegistryResult::success);
    CHECK(registry.try_begin(0x30U, metadata(2U), second) == RegistryResult::success);
    CHECK(registry.try_begin(0x50U, metadata(3U), third) == RegistryResult::full);
    CHECK(!third);
    CHECK(registry.try_retire(first) == RegistryResult::success);

    RegistryToken reused{};
    CHECK(registry.try_begin(0x10U, metadata(4U), reused) == RegistryResult::success);
    CHECK(reused.generation == 2U);
    CHECK(!reused.activeReuse);
    CHECK(!reused.reuseAfterUncertainRetire);
    const RegistryCounters counters = registry.counters();
    CHECK(counters.highWater == 2U);
    CHECK(counters.full == 1U);
    CHECK(counters.retired == 1U);
}

void registry_distinguishes_certain_and_uncertain_active_reuse() {
    FixedRegistry<4U> registry;
    registry.reset();
    RegistryToken first{};
    RegistryToken certainReuse{};
    CHECK(registry.try_begin(0x100U, metadata(), first) == RegistryResult::success);
    CHECK(registry.try_begin(0x100U, metadata(), certainReuse) == RegistryResult::success);
    CHECK(certainReuse.activeReuse);
    CHECK(!certainReuse.reuseAfterUncertainRetire);

    CHECK(registry.testing_lock(0x100U));
    CHECK(registry.try_retire(certainReuse) == RegistryResult::busy);
    registry.testing_unlock(0x100U);
    RegistryToken uncertainReuse{};
    CHECK(registry.try_begin(0x100U, metadata(), uncertainReuse) == RegistryResult::success);
    CHECK(!uncertainReuse.activeReuse);
    CHECK(uncertainReuse.reuseAfterUncertainRetire);
    RegistryToken stillUncertain{};
    CHECK(registry.try_begin(0x100U, metadata(), stillUncertain) == RegistryResult::success);
    CHECK(!stillUncertain.activeReuse);
    CHECK(stillUncertain.reuseAfterUncertainRetire);
    const RegistryCounters counters = registry.counters();
    CHECK(counters.activeReuse == 1U);
    CHECK(counters.retireBusy == 1U);
    CHECK(counters.uncertainReuse == 2U);
    CHECK(counters.losses() == 3U);
}

void registry_accounts_busy_stale_missing_and_exhaustion() {
    FixedRegistry<4U> registry;
    registry.reset();
    RegistryToken invalid{};
    CHECK(registry.try_begin(0U, metadata(), invalid) == RegistryResult::invalid);
    RegistryToken token{};
    CHECK(registry.try_begin(0x200U, metadata(), token) == RegistryResult::success);
    CHECK(registry.testing_lock(0x200U));
    CHECK(registry.try_set_baseline(token, {}, true) == RegistryResult::busy);
    RegistryView view{};
    CHECK(registry.try_snapshot(0x200U, view) == RegistryResult::busy);
    registry.testing_unlock(0x200U);

    RegistryToken stale = token;
    ++stale.generation;
    CHECK(registry.try_associate(stale, 0x300U, 4U, true) == RegistryResult::stale);
    CHECK(registry.try_retire(stale) == RegistryResult::stale);
    CHECK(registry.try_snapshot(0x9990U, view) == RegistryResult::missing);

    registry.testing_set_generation(
        0x200U, (std::numeric_limits<std::uint32_t>::max)());
    RegistryToken exhausted{};
    CHECK(registry.try_begin(0x200U, metadata(), exhausted)
          == RegistryResult::generationExhausted);
    const RegistryCounters counters = registry.counters();
    CHECK(counters.invalid == 1U);
    CHECK(counters.updateBusy == 1U);
    CHECK(counters.snapshotBusy == 1U);
    CHECK(counters.staleUpdate == 1U);
    CHECK(counters.staleRetire == 1U);
    CHECK(counters.missingSnapshot == 1U);
    CHECK(counters.generationExhausted == 1U);
    CHECK(counters.losses() == 7U);
}

void failed_destroy_snapshot_cannot_fabricate_certain_reuse() {
    FixedRegistry<4U> registry;
    registry.reset();
    RegistryToken token{};
    CHECK(registry.try_begin(0x280U, metadata(), token) == RegistryResult::success);
    CHECK(registry.testing_lock(0x280U));
    RegistryView view{};
    CHECK(registry.try_snapshot(0x280U, view) == RegistryResult::busy);
    registry.mark_retirement_uncertain(0x280U);
    registry.testing_unlock(0x280U);

    RegistryToken reused{};
    CHECK(registry.try_begin(0x280U, metadata(), reused) == RegistryResult::success);
    CHECK(!reused.activeReuse);
    CHECK(reused.reuseAfterUncertainRetire);
    CHECK(registry.counters().snapshotBusy == 1U);
    CHECK(registry.counters().uncertainReuse == 1U);
}

void unrelated_registry_slots_progress_concurrently_without_global_contention() {
    constexpr std::size_t kThreadCount = 4U;
    constexpr std::size_t kCycles = 1000U;
    FixedRegistry<8U> registry;
    registry.reset();
    std::array<std::thread, kThreadCount> threads{};
    std::atomic_bool start{false};
    std::atomic_uint64_t failures{};

    for (std::size_t thread = 0U; thread < threads.size(); ++thread) {
        threads[thread] = std::thread([&, thread] {
            const std::uintptr_t record = 0x1000U + thread * 0x10U;
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            for (std::size_t cycle = 0U; cycle < kCycles; ++cycle) {
                RegistryToken token{};
                RegistryView view{};
                if (registry.try_begin(record, metadata(static_cast<std::uint32_t>(thread)), token)
                        != RegistryResult::success
                    || registry.try_set_baseline(token, {7U, 9U, 11U}, true)
                           != RegistryResult::success
                    || registry.try_snapshot(record, view) != RegistryResult::success
                    || registry.try_retire(token) != RegistryResult::success) {
                    failures.fetch_add(1U, std::memory_order_relaxed);
                }
            }
        });
    }
    start.store(true, std::memory_order_release);
    for (std::thread& thread : threads) {
        thread.join();
    }
    const RegistryCounters counters = registry.counters();
    CHECK(failures.load(std::memory_order_relaxed) == 0U);
    CHECK(counters.begun == kThreadCount * kCycles);
    CHECK(counters.retired == kThreadCount * kCycles);
    CHECK(counters.highWater == kThreadCount);
    CHECK(counters.losses() == 0U);
}

void racing_ring_producers_are_fully_accounted_without_exact_fill_assumption() {
    constexpr std::uint64_t kProducerCount = 4U;
    constexpr std::uint64_t kAttempts = 10'000U;
    FixedRing<64U> ring;
    CHECK(ring.reset(9U));
    std::atomic_bool start{false};
    std::array<std::thread, kProducerCount> producers{};
    for (std::uint64_t producer = 0U; producer < producers.size(); ++producer) {
        producers[producer] = std::thread([&, producer] {
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            for (std::uint64_t attempt = 0U; attempt < kAttempts; ++attempt) {
                (void)ring.try_push(event(9U,
                                          static_cast<std::uint32_t>(producer + 1U),
                                          static_cast<std::uintptr_t>(
                                              1U + producer * kAttempts + attempt)));
            }
        });
    }
    start.store(true, std::memory_order_release);
    for (std::thread& producer : producers) {
        producer.join();
    }
    const RingCounters counters = ring.counters();
    CHECK(counters.committed <= 64U);
    CHECK(counters.committed + counters.losses() == kProducerCount * kAttempts);
}

} // namespace

int main() {
    schema_is_distinct_and_public_bits_are_exact();
    semantic_sensor_identity_ignores_source_padding();
    image_identity_and_prefixes_are_all_or_none();
    ring_rejects_invalid_stale_busy_and_exhausted_inputs();
    ring_wraps_without_overwrite_and_tracks_freeze_boundary();
    registry_retains_colliding_inactive_keys_and_reports_full();
    registry_distinguishes_certain_and_uncertain_active_reuse();
    registry_accounts_busy_stale_missing_and_exhaustion();
    failed_destroy_snapshot_cannot_fabricate_certain_reuse();
    unrelated_registry_slots_progress_concurrently_without_global_contention();
    racing_ring_producers_are_fully_accounted_without_exact_fill_assumption();

    if (g_failureCount != 0) {
        std::cerr << g_failureCount << " sensor heap smoke check(s) failed\n";
        return 1;
    }
    std::cout << "all sensor heap smoke checks passed\n";
    return 0;
}
