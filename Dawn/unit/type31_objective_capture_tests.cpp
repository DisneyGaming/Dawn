#include <Windows.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <thread>
#include <type_traits>

#include "client/hooks/bootflow/type31_objective_capture.h"
#include "client/hooks/bootflow/type31_objective_capture_lifecycle.h"
#include "client/hooking/call_gate.h"

namespace {

using namespace dawn::client::hooks::bootflow::type31_capture;
using dawn::state::activity::ActivationGeneration;
using dawn::state::activity::ActivityIncarnation;
using dawn::state::activity::ActivityInstanceKey;
using dawn::state::activity::ModuleGeneration;
using dawn::state::activity::NativeActivationKey;

static_assert(std::is_trivially_copyable_v<CaptureContext>);
static_assert(std::is_trivially_copyable_v<CaptureRecord>);
static_assert(sizeof(StateKey16) == 0x10U);
static_assert(sizeof(ComponentSnapshot) == 0x20U);
static_assert(kQueueCapacity == 64U);
static_assert(kDecodedBodyBytes == 0x18U);
static_assert(std::is_trivially_copyable_v<DefaultLogFields>);

int g_failureCount = 0;

void check(bool condition, const char* expression, int line) {
    if (condition) {
        return;
    }
    std::cerr << __FILE__ << ':' << line << ": check failed: " << expression << '\n';
    ++g_failureCount;
}

#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

constexpr CaptureContext exact_context(std::uint64_t activationGeneration = 19U) noexcept {
    CaptureContext result{};
    result.presence_mask = kKnownContextPresenceMask;
    result.activation =
        NativeActivationKey{ModuleGeneration{17U}, ActivationGeneration{activationGeneration}};
    result.native_identity = 0x1122334455667788ULL;
    result.activity = ActivityInstanceKey{41U, ActivityIncarnation{7U}};
    result.session = SessionLineageSnapshot{41U, 5U, 9U};
    result.run_token = 23U;
    result.correlation_token = 29U;
    result.generation_token = 31U;
    result.activation_state = ActivationSnapshotState::current;
    return result;
}

void write32(std::byte* destination, std::uint32_t value) noexcept {
    std::memcpy(destination, &value, sizeof value);
}

void write64(std::byte* destination, std::uint64_t value) noexcept {
    std::memcpy(destination, &value, sizeof value);
}

struct Fixture final {
    std::array<std::byte, 0x1A0U> component{};
    std::array<std::byte, kStateKeyBytes> stateKey{};
    std::array<std::byte, kDecodedBodyBytes> decodedBody{};

    explicit Fixture(std::uint32_t definition = kObjectiveDefinition) noexcept {
        write32(component.data(), definition);
        write32(stateKey.data(), kAuthoritySchema);
        const std::uintptr_t decodedAddress =
            reinterpret_cast<std::uintptr_t>(decodedBody.data());
        std::memcpy(stateKey.data() + 8U, &decodedAddress, sizeof decodedAddress);
        decodedBody[0] = std::byte{1U};
        write64(decodedBody.data() + 8U, 0x1122334455667788ULL);
        write64(decodedBody.data() + 16U, 0x8877665544332211ULL);
        write64(component.data() + 0x180U, 0x100U);
        write64(component.data() + 0x188U, 0x180U);
        write64(component.data() + 0x190U, 0x200U);
        write64(component.data() + 0x198U, 0x300U);
    }
};

CaptureRecord raw_record(std::uint64_t identity,
                         const CaptureContext& captureContext = {}) noexcept {
    CaptureRecord result{};
    result.context = captureContext;
    result.capture_epoch = 1U;
    result.monotonic_tick = identity;
    result.producer_thread_id = 4U;
    result.caller_rva = 0x1234U;
    result.component_identity = static_cast<std::uintptr_t>(identity);
    result.definition = kObjectiveDefinition;
    result.schema = kAuthoritySchema;
    result.logical = logical_identity(result.definition);
    write32(result.state_key.data(), kAuthoritySchema);
    result.decoded_body_pre[0] = std::byte{1U};
    write64(result.decoded_body_pre.data() + 8U, identity);
    result.before = ComponentSnapshot{identity, identity + 1U, identity + 2U, identity + 3U};
    result.after =
        ComponentSnapshot{identity + 4U, identity + 5U, identity + 6U, identity + 7U};
    result.state_key_valid = true;
    result.decoded_body_pre_valid = true;
    result.component_before_valid = true;
    result.component_after_valid = true;
    result.activation_entry_exact = fully_correlated(captureContext);
    result.activation_current_at_exit = result.activation_entry_exact;
    return result;
}

void raw_validity_is_separate_from_correlation() {
    CaptureRecord observation = raw_record(11U);
    CHECK(valid_raw_record(observation));
    CHECK(!fully_correlated(observation.context));
    CHECK(!record_matches_context(observation, {}));

    CHECK(fully_correlated(exact_context()));
    CaptureContext partial = exact_context();
    partial.presence_mask &= ~static_cast<std::uint32_t>(ContextPresence::session);
    CHECK(!fully_correlated(partial));
    partial = exact_context();
    partial.session.session_id = 42U;
    CHECK(!fully_correlated(partial));
    partial = exact_context();
    partial.activation.generation = {};
    CHECK(!fully_correlated(partial));
    partial = exact_context();
    partial.session.record_revision = partial.session.created_revision - 1U;
    CHECK(!fully_correlated(partial));

    observation.context = exact_context();
    observation.activation_entry_exact = true;
    observation.activation_current_at_exit = true;
    CHECK(record_matches_context(observation, exact_context()));
    CHECK(record_matches_current_context(observation, exact_context()));
    CHECK(!record_matches_context(observation, exact_context(20U)));
    observation.activation_current_at_exit = false;
    CHECK(record_matches_context(observation, exact_context()));
    CHECK(!record_matches_current_context(observation, exact_context()));
}

void context_equality_covers_presence_and_every_value() {
    const CaptureContext expected = exact_context();
    CaptureContext changed = expected;
    CHECK(changed == expected);
    changed.presence_mask ^= static_cast<std::uint32_t>(ContextPresence::run_token);
    CHECK(changed != expected);
    changed = expected;
    changed.activation.module.value++;
    CHECK(changed != expected);
    changed = expected;
    changed.activity.incarnation.value++;
    CHECK(changed != expected);
    changed = expected;
    changed.session.record_revision++;
    CHECK(changed != expected);
    changed = expected;
    changed.run_token++;
    CHECK(changed != expected);
    changed = expected;
    changed.correlation_token++;
    CHECK(changed != expected);
    changed = expected;
    changed.generation_token++;
    CHECK(changed != expected);
    changed = expected;
    changed.activation_state = ActivationSnapshotState::quiescing;
    CHECK(changed != expected);
}

void partial_context_shape_is_sanitized_without_losing_raw_evidence() {
    CaptureContext sanitized{};
    CaptureContext malformed{};
    malformed.presence_mask = static_cast<std::uint32_t>(ContextPresence::activation) | 0x80000000U;
    malformed.activation = NativeActivationKey{ModuleGeneration{1U}, ActivationGeneration{2U}};
    malformed.activation_state = ActivationSnapshotState::current;
    CHECK(!sanitize_context(malformed, sanitized));
    CHECK(sanitized.presence_mask == static_cast<std::uint32_t>(ContextPresence::activation));
    CHECK(static_cast<bool>(sanitized.activation));

    malformed = exact_context();
    malformed.activation = {};
    CHECK(!sanitize_context(malformed, sanitized));
    CHECK(!has_context_field(sanitized.presence_mask, ContextPresence::activation));
    CHECK(sanitized.activation_state == ActivationSnapshotState::absent);

    malformed = exact_context();
    malformed.native_identity = 0U;
    CHECK(!sanitize_context(malformed, sanitized));
    CHECK(!has_context_field(sanitized.presence_mask, ContextPresence::native_identity));

    malformed = exact_context();
    malformed.activity = {};
    CHECK(!sanitize_context(malformed, sanitized));
    CHECK(!has_context_field(sanitized.presence_mask, ContextPresence::activity));

    malformed = exact_context();
    malformed.session.record_revision = malformed.session.created_revision - 1U;
    CHECK(!sanitize_context(malformed, sanitized));
    CHECK(!has_context_field(sanitized.presence_mask, ContextPresence::session));

    malformed = exact_context();
    malformed.session.session_id++;
    CHECK(!sanitize_context(malformed, sanitized));
    CHECK(!has_context_field(sanitized.presence_mask, ContextPresence::activity));
    CHECK(!has_context_field(sanitized.presence_mask, ContextPresence::session));

    for (const ContextPresence field : {ContextPresence::run_token,
                                        ContextPresence::correlation_token,
                                        ContextPresence::generation_token}) {
        malformed = exact_context();
        if (field == ContextPresence::run_token) {
            malformed.run_token = 0U;
        } else if (field == ContextPresence::correlation_token) {
            malformed.correlation_token = 0U;
        } else {
            malformed.generation_token = 0U;
        }
        CHECK(!sanitize_context(malformed, sanitized));
        CHECK(!has_context_field(sanitized.presence_mask, field));
    }

    Fixture fixture;
    CaptureTarget target{};
    CHECK(inspect_capture_target(fixture.component.data(), fixture.stateKey.data(), target)
          == CaptureBuildResult::ready);
    malformed = exact_context();
    malformed.native_identity = 0U;
    PendingCapture pending;
    CHECK(prepare_capture(pending,
                          malformed,
                          CaptureMetadata{1U, 2U, 3U, 4U},
                          target,
                          fixture.component.data(),
                          fixture.stateKey.data()) == CaptureBuildResult::ready);
    CaptureRecord record{};
    CHECK(finish_capture(pending, fixture.component.data(), false, record)
          == CaptureBuildResult::complete);
    CHECK(valid_raw_record(record));
    CHECK(!record.context_shape_valid);
    CHECK(!has_context_field(record.context.presence_mask, ContextPresence::native_identity));
    CaptureQueue queue;
    CHECK(queue.try_push(record) == PushResult::enqueued);
    CHECK(queue.counters().context_resolver_failures == 1U);

    // Removing only unknown presence bits can leave every known exact field populated. The
    // resolver failure must still keep that record out of exact correlation and dedupe lanes.
    malformed = exact_context();
    malformed.presence_mask |= 0x80000000U;
    PendingCapture exactPending;
    CHECK(prepare_capture(exactPending,
                          malformed,
                          CaptureMetadata{1U, 2U, 3U, 4U},
                          target,
                          fixture.component.data(),
                          fixture.stateKey.data()) == CaptureBuildResult::ready);
    CaptureRecord malformedExact{};
    CHECK(finish_capture(exactPending, fixture.component.data(), true, malformedExact)
          == CaptureBuildResult::complete);
    CHECK(fully_correlated(malformedExact.context));
    CHECK(!malformedExact.context_shape_valid);
    CHECK(!record_matches_context(malformedExact, exact_context()));
    CHECK(queue.try_push(malformedExact) == PushResult::enqueued);
    CHECK(queue.try_push(malformedExact) == PushResult::enqueued);
    CHECK(queue.counters().duplicates == 0U);
    CHECK(queue.counters().context_resolver_failures == 3U);
}

void native_prefixes_require_an_exact_pinned_value() {
    CHECK(native_target(NativeSurface::apply).rva == kApplyRva);
    CHECK(native_target(NativeSurface::predicate).rva == kPredicateRva);
    CHECK(native_target(NativeSurface::terminal).rva == kTerminalRva);
    CHECK(native_target(NativeSurface::subscriber).rva == kSubscriberRva);
    CHECK(native_target(NativeSurface::listener_enumerator).rva == kListenerEnumeratorRva);
    CHECK(native_prefix_matches(NativeSurface::apply, kApplyPrefix));
    CHECK(native_prefix_matches(NativeSurface::predicate, kPredicatePrefix));
    CHECK(native_prefix_matches(NativeSurface::terminal, kTerminalPrefix));
    CHECK(native_prefix_matches(NativeSurface::subscriber, kSubscriberPrefix));
    CHECK(native_prefix_matches(NativeSurface::listener_enumerator, kListenerEnumeratorPrefix));

    auto wrong = kApplyPrefix;
    wrong.back() ^= std::byte{0x01U};
    CHECK(!native_prefix_matches(NativeSurface::apply, wrong));
    CHECK(
        !native_prefix_matches(NativeSurface::apply, std::span<const std::byte>{wrong.data(), 8U}));
    CHECK(native_target(static_cast<NativeSurface>(0xFFU)).rva == 0U);
    CHECK(!native_prefix_matches(static_cast<NativeSurface>(0xFFU), kApplyPrefix));
}

void definition_and_schema_gates_fail_closed() {
    Fixture fixture;
    CaptureTarget target{};
    CHECK(inspect_capture_target(fixture.component.data(), fixture.stateKey.data(), target)
          == CaptureBuildResult::ready);
    CHECK(target.definition == kObjectiveDefinition);
    CHECK(target.schema == kAuthoritySchema);
    CHECK(target.logical == logical_identity(kObjectiveDefinition));

    CaptureTarget unchanged = target;
    write32(fixture.component.data(), 0x80F47BA7U);
    CHECK(inspect_capture_target(fixture.component.data(), fixture.stateKey.data(), unchanged)
          == CaptureBuildResult::unsupported_definition);
    CHECK(unchanged == target);
    write32(fixture.component.data(), kDialogueDefinition);
    write32(fixture.stateKey.data(), 0x80809525U);
    CHECK(inspect_capture_target(fixture.component.data(), fixture.stateKey.data(), unchanged)
          == CaptureBuildResult::wrong_schema);
    CHECK(unchanged == target);
    CHECK(inspect_capture_target(nullptr, fixture.stateKey.data(), unchanged)
          == CaptureBuildResult::null_pointer);
    CHECK(inspect_capture_target(fixture.component.data(), nullptr, unchanged)
          == CaptureBuildResult::null_pointer);
    CHECK(inspect_capture_target(reinterpret_cast<const void*>(1U),
                                 fixture.stateKey.data(),
                                 unchanged)
          == CaptureBuildResult::unreadable);
}

void decoded_body_exposes_only_proved_fields() {
    CaptureRecord observation = raw_record(20U);
    const std::uint64_t first = 0x1122334455667788ULL;
    const std::uint64_t second = 0x8877665544332211ULL;
    write64(observation.decoded_body_pre.data() + 8U, first);
    write64(observation.decoded_body_pre.data() + 16U, second);

    DecodedBodyFields fields{};
    CHECK(decode_body(observation, fields));
    CHECK(fields.auth_bool);
    CHECK(fields.u64_0 == first);
    CHECK(fields.u64_1 == second);

    observation.decoded_body_pre[0] = std::byte{2U};
    fields = {true, 7U, 8U};
    CHECK(!decode_body(observation, fields));
    CHECK(fields.auth_bool);
    CHECK(fields.u64_0 == 7U);
    CHECK(fields.u64_1 == 8U);
}

void normal_log_projection_contains_only_bounded_scalars_and_hashes() {
    CaptureRecord observation = raw_record(0x1122334455667788ULL, exact_context());
    DefaultLogFields fields{};
    CHECK(default_log_fields(observation, fields));
    CHECK(fields.component_token != 0U);
    CHECK(fields.context_hash != 0U);
    CHECK(fields.state_key_hash == bounded_hash(observation.state_key));
    CHECK(fields.decoded_body_pre_hash == bounded_hash(observation.decoded_body_pre));
    CHECK(fields.component_before_hash != 0U);
    CHECK(fields.component_after_hash != 0U);
    CHECK(fields.decoded_valid);

    CaptureRecord changed = observation;
    changed.state_key[8] ^= std::byte{0x01U};
    DefaultLogFields changedFields{};
    CHECK(default_log_fields(changed, changedFields));
    CHECK(changedFields.state_key_hash != fields.state_key_hash);
    CHECK(changedFields.component_token == fields.component_token);

    CHECK(kPinnedPackedImageSha256[0] == std::byte{0x81U});
    CHECK(kPinnedPackedImageSha256[31] == std::byte{0xEDU});
}

void two_phase_capture_preserves_exact_pre_body_and_full_component_windows() {
    Fixture fixture{kDialogueDefinition};
    CaptureTarget target{};
    CHECK(inspect_capture_target(fixture.component.data(), fixture.stateKey.data(), target)
          == CaptureBuildResult::ready);
    const auto expectedStateKey = fixture.stateKey;
    const auto expectedBody = fixture.decodedBody;

    PendingCapture mismatch;
    CaptureRecord unchanged = raw_record(77U);
    CHECK(prepare_capture(mismatch,
                          {},
                          CaptureMetadata{5U, 6U, 7U, 8U},
                          target,
                          fixture.component.data(),
                          fixture.stateKey.data()) == CaptureBuildResult::ready);
    CHECK(finish_capture(mismatch, fixture.component.data() + 1U, false, unchanged)
          == CaptureBuildResult::component_mismatch);
    CHECK(unchanged.component_identity == 77U);
    CHECK(finish_capture(mismatch, fixture.component.data(), false, unchanged)
          == CaptureBuildResult::consumed);

    PendingCapture pending;
    CHECK(prepare_capture(pending,
                          {},
                          CaptureMetadata{5U, 6U, 7U, 8U},
                          target,
                          fixture.component.data(),
                          fixture.stateKey.data()) == CaptureBuildResult::ready);
    write64(fixture.component.data() + 0x180U, 0x101U);
    write64(fixture.component.data() + 0x188U, 0x181U);
    write64(fixture.component.data() + 0x190U, 0x201U);
    write64(fixture.component.data() + 0x198U, 0x301U);
    fixture.decodedBody.fill(std::byte{0xFFU});

    CaptureRecord output{};
    CHECK(finish_capture(pending, fixture.component.data(), false, output)
          == CaptureBuildResult::complete);
    CHECK(valid_raw_record(output));
    CHECK(!fully_correlated(output.context));
    CHECK(output.capture_epoch == 5U);
    CHECK(output.monotonic_tick == 6U);
    CHECK(output.producer_thread_id == 7U);
    CHECK(output.caller_rva == 8U);
    CHECK(output.definition == kDialogueDefinition);
    CHECK(output.state_key == expectedStateKey);
    CHECK(output.decoded_body_pre == expectedBody);
    CHECK((output.before == ComponentSnapshot{0x100U, 0x180U, 0x200U, 0x300U}));
    CHECK((output.after == ComponentSnapshot{0x101U, 0x181U, 0x201U, 0x301U}));
    CHECK(output.state_key_valid);
    CHECK(output.decoded_body_pre_valid);
    CHECK(output.component_before_valid);
    CHECK(output.component_after_valid);
    CHECK(!output.activation_entry_exact);
    CHECK(!output.activation_current_at_exit);
}

void target_change_and_unreadable_body_do_not_change_output() {
    Fixture fixture;
    CaptureTarget target{};
    CHECK(inspect_capture_target(fixture.component.data(), fixture.stateKey.data(), target)
          == CaptureBuildResult::ready);
    PendingCapture pending;
    write32(fixture.stateKey.data(), 0x80809525U);
    CHECK(prepare_capture(pending,
                          {},
                          CaptureMetadata{1U, 2U, 3U, 4U},
                          target,
                          fixture.component.data(),
                          fixture.stateKey.data()) == CaptureBuildResult::target_changed);
    write32(fixture.stateKey.data(), kAuthoritySchema);
    const std::uintptr_t badAddress = 1U;
    std::memcpy(fixture.stateKey.data() + 8U, &badAddress, sizeof badAddress);
    CHECK(prepare_capture(pending,
                          {},
                          CaptureMetadata{1U, 2U, 3U, 4U},
                          target,
                          fixture.component.data(),
                          fixture.stateKey.data()) == CaptureBuildResult::unreadable);
}

void post_snapshot_failure_preserves_partial_raw_evidence() {
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    void* const allocation =
        VirtualAlloc(nullptr, info.dwPageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    CHECK(allocation != nullptr);
    if (allocation == nullptr) {
        return;
    }

    auto* const component = static_cast<std::byte*>(allocation);
    write32(component, kObjectiveDefinition);
    write64(component + 0x180U, 0x10U);
    write64(component + 0x188U, 0x18U);
    write64(component + 0x190U, 0x20U);
    write64(component + 0x198U, 0x28U);
    Fixture fixture;
    CaptureTarget target{};
    CHECK(inspect_capture_target(component, fixture.stateKey.data(), target)
          == CaptureBuildResult::ready);
    PendingCapture pending;
    CHECK(prepare_capture(pending,
                          {},
                          CaptureMetadata{9U, 10U, 11U, 12U},
                          target,
                          component,
                          fixture.stateKey.data()) == CaptureBuildResult::ready);

    DWORD oldProtection{};
    CHECK(VirtualProtect(allocation, info.dwPageSize, PAGE_NOACCESS, &oldProtection) != FALSE);
    CaptureRecord output{};
    CHECK(finish_capture(pending, component, false, output) == CaptureBuildResult::partial);
    CHECK(valid_raw_record(output));
    CHECK(output.decoded_body_pre_valid);
    CHECK(output.component_before_valid);
    CHECK(!output.component_after_valid);
    CHECK(output.decoded_body_pre == fixture.decodedBody);

    DWORD ignoredProtection{};
    CHECK(VirtualProtect(allocation,
                         info.dwPageSize,
                         oldProtection,
                         &ignoredProtection) != FALSE);
    CHECK(VirtualFree(allocation, 0U, MEM_RELEASE) != FALSE);
}

void absent_context_records_are_never_deduplicated() {
    CaptureQueue queue;
    const CaptureRecord observation = raw_record(1U);
    CHECK(queue.try_push(observation) == PushResult::enqueued);
    CHECK(queue.try_push(observation) == PushResult::enqueued);
    CHECK(queue.counters().duplicates == 0U);

    CaptureRecord output{};
    CHECK(queue.try_pop_raw(output) == ReadResult::success);
    CHECK(output.sequence == 1U);
    CHECK(queue.try_pop_raw(output) == ReadResult::success);
    CHECK(output.sequence == 2U);
}

void dedupe_requires_the_complete_exact_owner() {
    CaptureQueue queue;
    const CaptureRecord first = raw_record(1U, exact_context(50U));
    CHECK(queue.try_push(first) == PushResult::enqueued);
    CHECK(queue.try_push(first) == PushResult::duplicate);

    CaptureRecord second = first;
    second.context = exact_context(51U);
    CHECK(queue.try_push(second) == PushResult::enqueued);
    const QueueCounters counters = queue.counters();
    CHECK(counters.accepted == 2U);
    CHECK(counters.duplicates == 1U);
}

void raw_fifo_and_exact_consumers_have_distinct_semantics() {
    CaptureQueue queue;
    const CaptureRecord absent = raw_record(1U);
    CaptureRecord historical = raw_record(2U, exact_context(60U));
    historical.activation_current_at_exit = false;
    CHECK(queue.try_push(absent) == PushResult::enqueued);
    CHECK(queue.try_push(historical) == PushResult::enqueued);

    CaptureContext front = exact_context();
    CHECK(queue.try_front_context(front) == ReadResult::success);
    CHECK(front == CaptureContext{});
    CaptureRecord output = raw_record(99U);
    CHECK(queue.try_pop_exact({}, output) == ReadResult::invalid_context);
    CHECK(queue.try_pop_exact(exact_context(60U), output) == ReadResult::context_mismatch);
    CHECK(output.component_identity == 99U);
    CHECK(queue.try_pop_raw(output) == ReadResult::success);
    CHECK(output.component_identity == 1U);
    CHECK(output.sequence == 1U);

    CHECK(queue.try_pop_exact_current(exact_context(60U), output) == ReadResult::not_current);
    CHECK(queue.try_pop_exact(exact_context(60U), output) == ReadResult::success);
    CHECK(output.component_identity == 2U);
    CHECK(output.sequence == 2U);
    CHECK(queue.try_pop_raw(output) == ReadResult::empty);
}

void queue_counts_rejections_full_and_contention_drops() {
    CaptureQueue queue;
    CaptureRecord invalid = raw_record(1U);
    invalid.state_key_valid = false;
    CHECK(queue.try_push(invalid) == PushResult::rejected);
    queue.account_rejected();
    for (std::uint64_t index = 1U; index <= kQueueCapacity; ++index) {
        CHECK(queue.try_push(raw_record(index)) == PushResult::enqueued);
    }
    CHECK(queue.try_push(raw_record(kQueueCapacity + 1U)) == PushResult::full);
    QueueCounters counters = queue.counters();
    CHECK(counters.accepted == kQueueCapacity);
    CHECK(counters.rejected == 2U);
    CHECK(counters.dropped_full == 1U);

    constexpr std::uint64_t kProducerCount = 4U;
    constexpr std::uint64_t kAttemptsPerProducer = 10'000U;
    CaptureQueue contended;
    std::array<std::thread, kProducerCount> producers{};
    std::atomic_bool start{false};
    for (std::uint64_t producer = 0U; producer < producers.size(); ++producer) {
        producers[producer] = std::thread([&, producer] {
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            for (std::uint64_t attempt = 0U; attempt < kAttemptsPerProducer; ++attempt) {
                const std::uint64_t identity =
                    1U + producer * kAttemptsPerProducer + attempt;
                (void)contended.try_push(raw_record(identity));
            }
        });
    }
    start.store(true, std::memory_order_release);
    for (std::thread& producer : producers) {
        producer.join();
    }
    counters = contended.counters();
    const std::uint64_t accounted =
        counters.accepted + counters.duplicates + counters.rejected + counters.dropped();
    CHECK(counters.accepted == kQueueCapacity);
    CHECK(accounted == kProducerCount * kAttemptsPerProducer);
    CHECK(counters.dropped_full + counters.dropped_busy != 0U);
}

void confirmed_reset_starts_a_clean_sequence_and_counter_epoch() {
    CaptureQueue queue;
    CHECK(queue.try_push(raw_record(1U)) == PushResult::enqueued);
    queue.account_rejected();
    queue.account_projection_failure();
    CHECK(queue.counters().accepted == 1U);
    CHECK(queue.counters().rejected == 1U);
    CHECK(queue.counters().projection_failures == 1U);
    CHECK(queue.try_reset() == ResetResult::reset);
    const QueueCounters reset = queue.counters();
    CHECK(reset.accepted == 0U);
    CHECK(reset.rejected == 0U);
    CHECK(reset.projection_failures == 0U);
    CaptureRecord output{};
    CHECK(queue.try_pop_raw(output) == ReadResult::empty);
    CHECK(queue.try_push(raw_record(2U)) == PushResult::enqueued);
    CHECK(queue.try_pop_raw(output) == ReadResult::success);
    CHECK(output.sequence == 1U);
}

struct FakeOwnerOperations final {
    bool enabled_value{true};
    bool admitted{true};
    bool reset_ok{true};
    bool attach_ok{true};
    bool finalize_ok{true};
    bool handle{};
    bool original{};
    bool accepting{};
    std::uint64_t next_epoch{1U};
    std::uint64_t queue_epoch{};
    std::uint64_t queue_records{};
    std::uint32_t publish_calls{};
    std::uint32_t remove_calls{};
    OwnerRemovalResult removal{OwnerRemovalResult::removed};
    dawn::client::hooking::CallGate point_gate{};
    dawn::client::hooking::CallGate drain_gate{};

    [[nodiscard]] bool enabled() const noexcept { return enabled_value; }
    [[nodiscard]] bool admit_image() const noexcept { return admitted; }
    [[nodiscard]] std::uint64_t allocate_epoch() noexcept { return next_epoch++; }
    [[nodiscard]] bool reset_for_new_epoch(std::uint64_t epoch) noexcept {
        if (!reset_ok) {
            return false;
        }
        queue_epoch = epoch;
        queue_records = 0U;
        return true;
    }
    [[nodiscard]] bool attach() noexcept {
        handle = attach_ok;
        return attach_ok;
    }
    void publish(std::uint64_t) noexcept {
        original = true;
        accepting = true;
        point_gate.accept();
        drain_gate.accept();
        ++publish_calls;
    }
    void close_admission() noexcept {
        accepting = false;
        point_gate.quiesce();
        drain_gate.quiesce();
    }
    [[nodiscard]] OwnerRemovalResult remove() noexcept {
        ++remove_calls;
        if (!point_gate.idle() || !drain_gate.idle()) {
            return OwnerRemovalResult::protected_code_active;
        }
        if (removal == OwnerRemovalResult::removed) {
            handle = false;
        }
        return removal;
    }
    [[nodiscard]] bool finalize_removed() noexcept {
        if (!finalize_ok) {
            return false;
        }
        original = false;
        queue_epoch = 0U;
        queue_records = 0U;
        return true;
    }
};

void production_owner_policy_retains_failures_and_isolates_reinstall_epochs() {
    OwnerLifecycle lifecycle;
    FakeOwnerOperations operations;
    operations.enabled_value = false;
    CHECK(!lifecycle.install(operations));
    CHECK(lifecycle.phase() == OwnerPhase::detached);
    CHECK(operations.publish_calls == 0U);

    operations.enabled_value = true;
    operations.admitted = false;
    CHECK(!lifecycle.install(operations));
    CHECK(operations.publish_calls == 0U);

    operations.admitted = true;
    CHECK(lifecycle.install(operations));
    CHECK(lifecycle.phase() == OwnerPhase::active);
    CHECK(lifecycle.epoch() == 1U);
    CHECK(operations.handle);
    CHECK(operations.original);
    CHECK(operations.accepting);
    CHECK(operations.publish_calls == 1U);
    operations.queue_records = 7U;

    {
        dawn::client::hooking::CallGate::Scope pointCall(operations.point_gate);
        dawn::client::hooking::CallGate::Scope drainCall(operations.drain_gate);
        CHECK(pointCall.accepts_side_effects());
        CHECK(drainCall.accepts_side_effects());
        CHECK(!lifecycle.uninstall(operations));
        CHECK(!pointCall.accepts_side_effects());
        CHECK(!drainCall.accepts_side_effects());
    }
    CHECK(lifecycle.phase() == OwnerPhase::quiescing);
    CHECK(lifecycle.epoch() == 1U);
    CHECK(operations.handle);
    CHECK(operations.original);
    CHECK(!operations.accepting);
    CHECK(operations.queue_records == 7U);

    operations.removal = OwnerRemovalResult::removed;
    operations.finalize_ok = false;
    CHECK(!lifecycle.uninstall(operations));
    CHECK(lifecycle.phase() == OwnerPhase::removed_pending_reset);
    CHECK(lifecycle.epoch() == 1U);
    CHECK(!operations.handle);
    CHECK(operations.original);
    CHECK(operations.queue_records == 7U);

    operations.finalize_ok = true;
    CHECK(lifecycle.uninstall(operations));
    CHECK(lifecycle.phase() == OwnerPhase::detached);
    CHECK(lifecycle.epoch() == 0U);
    CHECK(!operations.original);
    CHECK(operations.queue_records == 0U);

    CHECK(lifecycle.install(operations));
    CHECK(lifecycle.phase() == OwnerPhase::active);
    CHECK(lifecycle.epoch() == 2U);
    CHECK(operations.queue_epoch == 2U);
    CHECK(operations.queue_records == 0U);
    CHECK(operations.publish_calls == 2U);
}

std::uint32_t g_fakeOriginalCalls{};

void fake_original(Fixture& fixture) noexcept {
    ++g_fakeOriginalCalls;
    write64(fixture.component.data() + 0x188U, 0xABCDEFU);
    fixture.decodedBody.fill(std::byte{0xEEU});
}

void fake_fanout(Fixture& fixture,
                 CaptureQueue& queue,
                 bool observe,
                 std::uint64_t epoch) noexcept {
    PendingCapture pending;
    CaptureTarget target{};
    CaptureBuildResult prepared = CaptureBuildResult::consumed;
    if (observe) {
        prepared = inspect_capture_target(fixture.component.data(), fixture.stateKey.data(), target);
        if (prepared == CaptureBuildResult::ready) {
            prepared = prepare_capture(pending,
                                       {},
                                       CaptureMetadata{epoch, 2U, 3U, 4U},
                                       target,
                                       fixture.component.data(),
                                       fixture.stateKey.data());
        }
    }

    fake_original(fixture);
    if (prepared == CaptureBuildResult::ready) {
        CaptureRecord output{};
        const CaptureBuildResult finished =
            finish_capture(pending, fixture.component.data(), false, output);
        if (finished == CaptureBuildResult::complete || finished == CaptureBuildResult::partial) {
            (void)queue.try_push(output);
        } else {
            queue.account_rejected();
        }
    } else if (observe) {
        queue.account_rejected();
    }
}

void fake_fanout_always_calls_original_once() {
    g_fakeOriginalCalls = 0U;
    CaptureQueue queue;
    Fixture success;
    const auto expectedBody = success.decodedBody;
    fake_fanout(success, queue, true, 1U);
    CHECK(g_fakeOriginalCalls == 1U);
    CaptureRecord output{};
    CHECK(queue.try_pop_raw(output) == ReadResult::success);
    CHECK(output.decoded_body_pre == expectedBody);

    Fixture rejected;
    write32(rejected.stateKey.data(), 0x80809525U);
    fake_fanout(rejected, queue, true, 1U);
    CHECK(g_fakeOriginalCalls == 2U);
    CHECK(queue.counters().rejected == 1U);

    Fixture quiesced;
    fake_fanout(quiesced, queue, false, 1U);
    CHECK(g_fakeOriginalCalls == 3U);

    CaptureQueue full;
    for (std::uint64_t index = 1U; index <= kQueueCapacity; ++index) {
        CHECK(full.try_push(raw_record(index)) == PushResult::enqueued);
    }
    Fixture dropped;
    fake_fanout(dropped, full, true, 2U);
    CHECK(g_fakeOriginalCalls == 4U);
    CHECK(full.counters().dropped_full == 1U);
}

void noncurrent_exact_activation_remains_available_to_raw_drain() {
    Fixture fixture;
    CaptureTarget target{};
    CHECK(inspect_capture_target(fixture.component.data(), fixture.stateKey.data(), target)
          == CaptureBuildResult::ready);
    PendingCapture pending;
    const CaptureContext context = exact_context(70U);
    CHECK(prepare_capture(pending,
                          context,
                          CaptureMetadata{3U, 4U, 5U, 6U},
                          target,
                          fixture.component.data(),
                          fixture.stateKey.data()) == CaptureBuildResult::ready);
    CaptureRecord record{};
    CHECK(finish_capture(pending, fixture.component.data(), false, record)
          == CaptureBuildResult::complete);
    CHECK(record.activation_entry_exact);
    CHECK(!record.activation_current_at_exit);

    CaptureQueue queue;
    CHECK(queue.try_push(record) == PushResult::enqueued);
    CaptureRecord output{};
    CHECK(queue.try_pop_exact_current(context, output) == ReadResult::not_current);
    CHECK(queue.try_pop_raw(output) == ReadResult::success);
    CHECK(output.context == context);
}

} // namespace

int main() {
    raw_validity_is_separate_from_correlation();
    context_equality_covers_presence_and_every_value();
    partial_context_shape_is_sanitized_without_losing_raw_evidence();
    native_prefixes_require_an_exact_pinned_value();
    definition_and_schema_gates_fail_closed();
    decoded_body_exposes_only_proved_fields();
    normal_log_projection_contains_only_bounded_scalars_and_hashes();
    two_phase_capture_preserves_exact_pre_body_and_full_component_windows();
    target_change_and_unreadable_body_do_not_change_output();
    post_snapshot_failure_preserves_partial_raw_evidence();
    absent_context_records_are_never_deduplicated();
    dedupe_requires_the_complete_exact_owner();
    raw_fifo_and_exact_consumers_have_distinct_semantics();
    queue_counts_rejections_full_and_contention_drops();
    confirmed_reset_starts_a_clean_sequence_and_counter_epoch();
    production_owner_policy_retains_failures_and_isolates_reinstall_epochs();
    fake_fanout_always_calls_original_once();
    noncurrent_exact_activation_remains_available_to_raw_drain();

    if (g_failureCount != 0) {
        std::cerr << g_failureCount << " Type-31 capture check(s) failed\n";
        return 1;
    }
    std::cout << "all Type-31 capture checks passed\n";
    return 0;
}
