#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <type_traits>

#include "client/hooks/bootflow/opening_authority/mission_header_intro_capture.h"

namespace {

using namespace sunrise::client::hooks::bootflow::opening_authority::
    mission_header_intro_capture;

static_assert(kObservationOnly);
static_assert(!kOwnsNativeDetour);
static_assert(!kWritesNativeLatchState);
static_assert(!kWritesNativeQueueState);
static_assert(!kInvokesCommand5Producer);
static_assert(!kInvokesQueueInsert);
static_assert(!kInvokesQueueTick);
static_assert(!kInvokesCuiGetter);
static_assert(!kProvidesDirectShowWriter);
static_assert(!kSynthesizesMissionHeader);
static_assert(!kDefaultTelemetryContainsRawPayload);
static_assert(std::is_trivially_copyable_v<Command5PayloadProjection>);
static_assert(std::is_trivially_copyable_v<QueueSnapshot>);
static_assert(std::is_trivially_copyable_v<LatchSnapshot>);
static_assert(std::is_trivially_copyable_v<CaptureRecord>);

int g_failures{};

void check(bool condition, const char* expression, int line) {
    if (condition) {
        return;
    }
    std::cerr << __FILE__ << ':' << line << ": check failed: " << expression << '\n';
    ++g_failures;
}

#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

CaptureContext exact_context() noexcept {
    CaptureContext context{};
    context.artifact = ArtifactKind::pc_packed_runtime;
    context.build_fingerprint = artifact_fingerprint(ArtifactKind::pc_packed_runtime);
    context.runtime_admission_generation = 1U;
    context.capture_epoch = 2U;
    context.module_generation = 3U;
    context.session_id = 4U;
    context.activity_generation = 5U;
    context.world_generation = 6U;
    context.profile_generation = 7U;
    context.queue_generation = 8U;
    context.latch_epoch = 9U;
    context.run_token = 10U;
    context.correlation_token = 11U;
    context.mapped_image_base = 0x140000000ULL;
    context.activity_index = kActivityIndex;
    context.activity_definition_hash = kActivityDefinitionHash;
    context.activity_package_tag = kActivityPackageTag;
    context.activity_internal_name_hash = text_hash(kActivityInternalName);
    return context;
}

CaptureMetadata metadata(std::uint64_t callId = 1U) noexcept {
    return CaptureMetadata{callId, 100U, 110U, 7U, kPcState3GateRva};
}

Command5PayloadProjection exact_payload() noexcept {
    return Command5PayloadProjection{kOmegaTitle,
                                     kActivityPresentationCategory,
                                     0xA1B2C3D4U,
                                     0x1122334455667788ULL,
                                     0x8877665544332211ULL,
                                     true};
}

ProducerTrace successful_producer() noexcept {
    ProducerTrace trace{};
    trace.final_stage = ProducerStage::returned;
    trace.disposition = ProducerDisposition::queue_attempted;
    trace.activity_manager_identity = 0x1000U;
    trace.client_activity_row_identity = 0x2000U;
    trace.display_row_identity = 0x3000U;
    trace.observed_activity_index = kActivityIndex;
    trace.observed_definition_hash = kActivityDefinitionHash;
    trace.observed_activity_type = kStoryActivityType;
    trace.observed_category_source = kStoryTypeClientCategorySource;
    trace.observed_presentation_category = kActivityPresentationCategory;
    trace.observed_title = kOmegaTitle;
    trace.payload = exact_payload();
    trace.investment_ready = true;
    trace.local_profile_eligible = true;
    trace.queue_insert_observed = true;
    return trace;
}

QueueSnapshot queue_snapshot(std::uint64_t hash = 0x100U) noexcept {
    QueueSnapshot snapshot{};
    snapshot.queued_count = 2U;
    snapshot.dispatch_definition_handle = 9;
    snapshot.current_sequence = 7;
    snapshot.current_retire_or_supersede = 0U;
    snapshot.authored_field_0 = 1U;
    snapshot.authored_field_1 = 2U;
    snapshot.authored_field_2 = 3U;
    snapshot.current_active = 1U;
    snapshot.current_command = 8;
    snapshot.current_payload_hash = 0x7788U;
    snapshot.next_sequence = 8;
    snapshot.exact_queue_state_hash = hash;
    return snapshot;
}

LatchSnapshot latch(std::int32_t lifecycle,
                    std::uint8_t arm,
                    std::uint8_t pending,
                    std::uint8_t ready,
                    std::uint32_t adjacent = 77U) noexcept {
    LatchSnapshot snapshot{};
    snapshot.lifecycle_state = lifecycle;
    snapshot.arm = arm;
    snapshot.pending = pending;
    snapshot.ready = ready;
    snapshot.adjacent_local_presentation_state = adjacent;
    return seal_latch_snapshot(snapshot);
}

CaptureRecord producer_record() noexcept {
    CaptureRecord record{};
    record.context = exact_context();
    record.metadata = metadata();
    record.kind = CaptureKind::producer;
    record.producer = successful_producer();
    return record;
}

void observation_contract_and_lane_separation_are_explicit() {
    CHECK(kOwnedEvidenceLane == PresentationLane::activity_intro_command5);
    CHECK(!kCommand5IsType68ObjectiveBody);
    CHECK(!kCommand5IsNewObjectiveNotification);
    CHECK(!kCommand5IsPersistentTracker);
    CHECK(kActivityIntroCommand == 5U);
    CHECK(kType68AuthorityType == 68U);
    CHECK(kType68Definition == 0x80F47BD4U);
    CHECK(kType68Bank == 0x80F47BD3U);
    CHECK(kType68ManagerCommandFamilies[0] == 1U);
    CHECK(kType68ManagerCommandFamilies[1] == 2U);
    CHECK(kType68ManagerCommandFamilies[2] == 6U);
}

void exact_build_package_and_native_boundaries_are_pinned() {
    for (const ArtifactKind kind : {ArtifactKind::pc_packed_runtime,
                                    ArtifactKind::pc_unpacked_provenance,
                                    ArtifactKind::omega_activity_package,
                                    ArtifactKind::ps4_eboot_provenance}) {
        const ArtifactDescriptor& descriptor = artifact_descriptor(kind);
        CHECK(descriptor.kind == kind);
        CHECK(descriptor.file_bytes != 0U);
        CHECK(matches_artifact(kind, {descriptor.file_bytes, descriptor.sha256}));
        CHECK(artifact_fingerprint(kind) != 0U);
        ArtifactIdentity wrong{descriptor.file_bytes + 1U, descriptor.sha256};
        CHECK(!matches_artifact(kind, wrong));
    }

    CHECK(artifact_descriptor(ArtifactKind::pc_packed_runtime).file_bytes == 122'984'224U);
    CHECK(artifact_descriptor(ArtifactKind::pc_unpacked_provenance).file_bytes
          == 145'091'072U);
    CHECK(artifact_descriptor(ArtifactKind::omega_activity_package).file_bytes == 407'552U);
    CHECK(artifact_descriptor(ArtifactKind::ps4_eboot_provenance).file_bytes == 37'824'851U);

    CHECK(pc_boundary(PcSurface::command5_producer).rva == kPcCommand5ProducerRva);
    CHECK(pc_boundary(PcSurface::producer_enqueue_anchor).rva
          == kPcCommand5EnqueueAnchorRva);
    CHECK(pc_boundary(PcSurface::queue_insert).rva == kPcQueueInsertRva);
    CHECK(pc_boundary(PcSurface::state3_pending_ready_gate).rva == kPcState3GateRva);
    CHECK(pc_boundary(PcSurface::reset_or_direct_show).rva == kPcResetDirectShowRva);
    CHECK(pc_boundary(PcSurface::rearm).rva == kPcRearmRva);
    CHECK(pc_boundary(PcSurface::ready_setter).rva == kPcReadySetterRva);
    CHECK(pc_boundary(PcSurface::queue_tick).rva == kPcQueueTickRva);
    CHECK(kPcQueueInsertCallRva == 0x131FFBDU);
    CHECK(kPcCurrentActivityManagerAccessorRva == 0xC26430U);
    CHECK(kPcInvestmentReadyRva == 0xC91590U);
    CHECK(kPcCurrentActivityIndexAccessorRva == 0x1778C70U);
    CHECK(kPcActivityManagerIndexFieldOffset == 0x8E08U);
    CHECK(kPcPendingClearRva == 0x13790A4U);
    CHECK(kPcLazyDispatchResolverRva == 0x139A2B0U);
    CHECK(kPcCategoryCuiWrapperRva == 0x138DEF0U);
    CHECK(kPcCategoryCuiGetterRva == 0x138F270U);
    CHECK(kPcTitleCuiWrapperRva == 0x138E3D0U);
    CHECK(kPcTitleCuiGetterRva == 0x138F750U);
    CHECK(kPcIconThemeCuiWrapperRva == 0x138DF80U);
    CHECK(kPcIconThemeCuiGetterRva == 0x138F570U);
    CHECK(kPcVisualTupleCuiWrapperRva == 0x138E4F0U);
    CHECK(kPcVisualTupleCuiGetterRva == 0x1399130U);
    CHECK(kPcArmLatchRva == 0x2FB6682U);
    CHECK(kPcPendingLatchRva == 0x2FB6683U);
    CHECK(kPcReadyLatchRva == 0x2FB6684U);
    CHECK(kPcHudQueueOffset == 0x3740U);
    CHECK(kGlobalCurrentSequenceOffset == 0x3AD0U);
    CHECK(kGlobalCurrentCommandOffset == 0x3AECU);
    CHECK(kGlobalCurrentPayloadOffset == 0x3AF4U);

    for (std::size_t index = 0U; index < kPcSurfaceCount; ++index) {
        const auto surface = static_cast<PcSurface>(index);
        const PcBoundaryDescriptor descriptor = pc_boundary(surface);
        CHECK(descriptor.prefix.size() == kNativePrefixBytes);
        CHECK(pc_prefix_matches(surface, descriptor.prefix));
        CHECK(!descriptor.direct_call_authorized);
        CHECK(!descriptor.inline_detour_authorized);
        auto wrong = std::array<std::byte, kNativePrefixBytes>{};
        std::memcpy(wrong.data(), descriptor.prefix.data(), descriptor.prefix.size());
        wrong[0] ^= std::byte{1U};
        CHECK(!pc_prefix_matches(surface, wrong));
    }

    RuntimeAdmissionEvidence admission{};
    const ArtifactDescriptor& runtime = artifact_descriptor(ArtifactKind::pc_packed_runtime);
    admission.packed_runtime = {runtime.file_bytes, runtime.sha256};
    admission.mapped_image_base = 0x140000000ULL;
    admission.mapped_image_bytes = 0x08A5EA00U;
    for (std::size_t index = 0U; index < kPcSurfaceCount; ++index) {
        const auto descriptor = pc_boundary(static_cast<PcSurface>(index));
        std::memcpy(admission.mapped_prefixes[index].bytes.data(),
                    descriptor.prefix.data(),
                    descriptor.prefix.size());
        admission.mapped_prefixes[index].byte_count =
            static_cast<std::uint8_t>(descriptor.prefix.size());
    }
    CHECK(validate_runtime_admission(admission) == RuntimeAdmissionResult::admitted);
    admission.mapped_prefixes[static_cast<std::size_t>(PcSurface::queue_insert)].bytes[0]
        ^= std::byte{1U};
    CHECK(validate_runtime_admission(admission) == RuntimeAdmissionResult::prefix_mismatch);

    CHECK(ps4_boundary(Ps4Surface::command5_producer).rva == 0xFEB7D0U);
    CHECK(ps4_boundary(Ps4Surface::command5_producer).file_offset == 0xFEF7D0U);
    CHECK(ps4_boundary(Ps4Surface::queue_insert).rva == 0xFF0D70U);
    CHECK(ps4_boundary(Ps4Surface::activity_state_machine).rva == 0xFEAB70U);
    CHECK(ps4_boundary(Ps4Surface::state3_pending_ready_gate).rva == 0xFEAD62U);
    CHECK(ps4_boundary(Ps4Surface::rearm).rva == 0xFEB780U);
    CHECK(ps4_boundary(Ps4Surface::ready_setter).rva == 0xFEF0D0U);
    CHECK(kPs4ArmLatchRva == 0x53E5104U);
    CHECK(kPs4PendingLatchRva == 0x53E5105U);
    CHECK(kPs4ReadyLatchRva == 0x53E5106U);
    CHECK(kPs4HudAccessorRva == 0xC1E8A0U);
    CHECK(kPs4LazyDispatchResolverRva == 0xF1E390U);
    CHECK(kPs4ActivityManagerIndexFieldOffset == 0x8E00U);
    CHECK(kPs4HudQueueOffset == 0x3740U);
    CHECK(kPs4ActivityManagerIndexFieldOffset != kPcActivityManagerIndexFieldOffset);
    for (std::size_t index = 0U;
         index < static_cast<std::size_t>(Ps4Surface::count);
         ++index) {
        const Ps4BoundaryDescriptor descriptor =
            ps4_boundary(static_cast<Ps4Surface>(index));
        CHECK(descriptor.file_offset == descriptor.rva + 0x4000U);
        CHECK(descriptor.generic_engine_homology_only);
        CHECK(!descriptor.omega_content_identity_proven);
        CHECK(ps4_prefix_matches(static_cast<Ps4Surface>(index), descriptor.prefix));
    }
}

void exact_activity_title_category_and_evidence_grades_are_pinned() {
    CHECK(kActivityIndex == 299U);
    CHECK(kActivityDefinitionHash == 0x87AC2003U);
    CHECK(kActivityInternalName == "mission_scot");
    CHECK(text_hash(kActivityInternalName) != 0U);
    CHECK(kActivityPackageTag == 0x80F4750CU);
    CHECK(kActivityPackageClass == 0x80808AAEU);
    CHECK(kActivityScenarioTag == 0x80F47522U);
    CHECK((kOmegaTitle == LocalizedPair{0x81331697U, 0x47CAC8CFU}));
    CHECK(kOmegaTitleBankMapIndex == 0x303U);
    CHECK(kStoryActivityType == 0U);
    CHECK(kStoryTypeClientCategorySource == 4U);
    CHECK(kActivityPresentationCategory == 0U);
    CHECK((kMissionCatalogPair == LocalizedPair{0x8132F809U, 0x980BA1D8U}));
    CHECK(kMissionBankMapIndex == 4U);
    CHECK(kMissionBankOrdinal == 0x3CU);

    CHECK(kCommand5ProducerIdentityGrade == EvidenceGrade::proven);
    CHECK(kActivityTitlePairGrade == EvidenceGrade::proven);
    CHECK(kCategoryChainGrade == EvidenceGrade::proven);
    CHECK(kMissionCatalogPairGrade == EvidenceGrade::proven);
    CHECK(kCategoryZeroToMissionCuiEdgeGrade == EvidenceGrade::inferred);
    CHECK(kReplayPolicyGrade == EvidenceGrade::unknown);
    CHECK(kJoinInProgressPolicyGrade == EvidenceGrade::unknown);
    CHECK(kRuntimeDispatchDefinitionGrade == EvidenceGrade::unknown);
    CHECK(kLiveVisualTupleGrade == EvidenceGrade::unknown);
    CHECK(kPs4OmegaContentIdentityGrade == EvidenceGrade::unknown);
}

void generation_and_build_context_must_be_exact() {
    CaptureContext context = exact_context();
    CHECK(fully_correlated(context));
    context.activity_index++;
    CHECK(!fully_correlated(context));
    context = exact_context();
    context.activity_definition_hash ^= 1U;
    CHECK(!fully_correlated(context));
    context = exact_context();
    context.activity_package_tag ^= 1U;
    CHECK(!fully_correlated(context));
    context = exact_context();
    context.activity_internal_name_hash ^= 1U;
    CHECK(!fully_correlated(context));
    context = exact_context();
    context.queue_generation = 0U;
    CHECK(!fully_correlated(context));
    context = exact_context();
    context.build_fingerprint ^= 1U;
    CHECK(!fully_correlated(context));
    context = exact_context();
    context.artifact = ArtifactKind::pc_unpacked_provenance;
    CHECK(!fully_correlated(context));
}

void producer_early_returns_and_success_are_strict() {
    ProducerTrace trace{};
    trace.final_stage = ProducerStage::entry;
    trace.disposition = ProducerDisposition::early_null_activity_manager;
    CHECK(valid_producer_trace(trace));
    trace.activity_manager_identity = 1U;
    CHECK(!valid_producer_trace(trace));

    trace = {};
    trace.final_stage = ProducerStage::activity_manager;
    trace.disposition = ProducerDisposition::early_investment_not_ready;
    trace.activity_manager_identity = 1U;
    CHECK(valid_producer_trace(trace));
    trace.investment_ready = true;
    CHECK(!valid_producer_trace(trace));

    trace = {};
    trace.final_stage = ProducerStage::investment_ready;
    trace.disposition = ProducerDisposition::early_invalid_activity_index;
    trace.activity_manager_identity = 1U;
    trace.investment_ready = true;
    trace.observed_activity_index = (std::numeric_limits<std::uint32_t>::max)();
    CHECK(valid_producer_trace(trace));
    trace.observed_activity_index = kActivityIndex;
    CHECK(!valid_producer_trace(trace));

    trace = {};
    trace.final_stage = ProducerStage::activity_index;
    trace.disposition = ProducerDisposition::early_profile_or_activity_ineligible;
    trace.activity_manager_identity = 1U;
    trace.investment_ready = true;
    trace.observed_activity_index = kActivityIndex;
    trace.observed_definition_hash = kActivityDefinitionHash;
    CHECK(valid_producer_trace(trace));
    trace.local_profile_eligible = true;
    CHECK(!valid_producer_trace(trace));

    trace = {};
    trace.final_stage = ProducerStage::eligibility;
    trace.disposition = ProducerDisposition::early_missing_client_activity_row;
    trace.observed_activity_index = kActivityIndex;
    trace.observed_definition_hash = kActivityDefinitionHash;
    trace.local_profile_eligible = true;
    CHECK(valid_producer_trace(trace));
    trace.client_activity_row_identity = 1U;
    CHECK(!valid_producer_trace(trace));

    trace = {};
    trace.final_stage = ProducerStage::client_activity_row;
    trace.disposition = ProducerDisposition::early_missing_display_row;
    trace.observed_activity_index = kActivityIndex;
    trace.observed_definition_hash = kActivityDefinitionHash;
    trace.client_activity_row_identity = 1U;
    CHECK(valid_producer_trace(trace));
    trace.display_row_identity = 2U;
    CHECK(!valid_producer_trace(trace));

    trace = successful_producer();
    CHECK(valid_producer_trace(trace));
    CHECK(valid_omega_payload(trace.payload));
    trace.observed_title.hash ^= 1U;
    CHECK(!valid_producer_trace(trace));
    trace = successful_producer();
    trace.payload.visual_tuple_hash = 0U;
    CHECK(!valid_producer_trace(trace));
    trace = successful_producer();
    trace.observed_category_source = 0U;
    CHECK(!valid_producer_trace(trace));
}

void native_queue_rejections_are_captured_without_writes() {
    QueueInsertObservation observation{};
    observation.command = kActivityIntroCommand;
    observation.payload = exact_payload();
    observation.pre = queue_snapshot();
    observation.post = observation.pre;
    observation.disposition = QueueInsertDisposition::rejected_authored_policy;
    CHECK(valid_queue_insert_observation(observation));

    observation.pre.dispatch_definition_handle = -1;
    observation.post = observation.pre;
    observation.disposition = QueueInsertDisposition::rejected_definition_unresolved;
    CHECK(valid_queue_insert_observation(observation));
    observation.post.exact_queue_state_hash++;
    CHECK(!valid_queue_insert_observation(observation));

    observation.pre = queue_snapshot();
    observation.pre.queued_count = kQueueCapacity;
    observation.post = observation.pre;
    observation.disposition = QueueInsertDisposition::rejected_full;
    CHECK(valid_queue_insert_observation(observation));
    observation.pre.dispatch_definition_handle = -1;
    observation.post = observation.pre;
    CHECK(!valid_queue_insert_observation(observation));

    observation.pre = queue_snapshot(0x100U);
    observation.post = observation.pre;
    observation.post.queued_count++;
    observation.post.exact_queue_state_hash = 0x200U;
    observation.disposition = QueueInsertDisposition::accepted;
    CHECK(valid_queue_insert_observation(observation));
    observation.post.exact_queue_state_hash = observation.pre.exact_queue_state_hash;
    CHECK(!valid_queue_insert_observation(observation));
    observation = {};
    observation.command = 68U;
    observation.payload = exact_payload();
    observation.pre = queue_snapshot();
    observation.post = observation.pre;
    CHECK(!valid_queue_insert_observation(observation));
}

void one_attempt_pending_clear_and_rearm_match_native_behavior() {
    const LatchSnapshot readyAttempt = latch(3, 1U, 1U, 1U);
    for (const ProducerAttemptOutcome outcome : {
             ProducerAttemptOutcome::queued,
             ProducerAttemptOutcome::producer_early_return,
             ProducerAttemptOutcome::queue_rejected_definition_unresolved,
             ProducerAttemptOutcome::queue_rejected_full,
             ProducerAttemptOutcome::queue_rejected_authored_policy,
         }) {
        const StateMachineExpectation expected = expected_state_machine_step(readyAttempt, outcome);
        CHECK(expected.valid_input);
        CHECK(expected.attempted);
        CHECK(expected.pending_cleared_after_attempt);
        CHECK(expected.post.pending == 0U);
        CHECK(expected.post.arm == 1U);
        CHECK(expected.post.ready == 1U);
        CHECK(expected.post.adjacent_local_presentation_state == 0U);

        LatchObservation observation{};
        observation.event = LatchBoundaryEvent::state3_gate;
        observation.pre = readyAttempt;
        observation.post = expected.post;
        observation.attempt_outcome = outcome;
        observation.unconditional_pending_clear_observed = true;
        CHECK(valid_latch_observation(observation));
        observation.unconditional_pending_clear_observed = false;
        CHECK(!valid_latch_observation(observation));
    }

    const StateMachineExpectation missingOutcome =
        expected_state_machine_step(readyAttempt, ProducerAttemptOutcome::not_attempted);
    CHECK(!missingOutcome.valid_input);

    // First state-3 arm clears ready and therefore cannot attempt in the same step.
    const LatchSnapshot fresh = latch(3, 0U, 0U, 1U);
    StateMachineExpectation expected =
        expected_state_machine_step(fresh, ProducerAttemptOutcome::not_attempted);
    CHECK(expected.valid_input && expected.armed && !expected.attempted);
    CHECK(expected.post.arm == 1U);
    CHECK(expected.post.pending == 1U);
    CHECK(expected.post.ready == 0U);

    // The observed re-arm function preserves arm, sets pending, and clears ready.
    const LatchSnapshot consumed = latch(3, 1U, 0U, 1U);
    const LatchSnapshot rearmed = expected_rearm_post(consumed);
    CHECK(rearmed.arm == 1U && rearmed.pending == 1U && rearmed.ready == 0U);
    LatchObservation rearmObservation{};
    rearmObservation.event = LatchBoundaryEvent::rearm;
    rearmObservation.pre = consumed;
    rearmObservation.post = rearmed;
    CHECK(valid_latch_observation(rearmObservation));

    // A later ready edge is required before the new one-attempt epoch can consume pending.
    const LatchSnapshot ready = expected_ready_post(rearmed);
    CHECK(ready.pending == 1U && ready.ready == 1U);
    expected = expected_state_machine_step(ready, ProducerAttemptOutcome::queued);
    CHECK(expected.valid_input && expected.attempted && expected.post.pending == 0U);

    const LatchSnapshot reset = expected_reset_post(ready);
    CHECK(reset.arm == 0U && reset.pending == 0U && reset.ready == 0U);
    CHECK(classify_reset_direct_show_mode(0) == ResetDirectShowMode::reset_clears_latches);
    CHECK(classify_reset_direct_show_mode(1)
          == ResetDirectShowMode::direct_show_observation_only);

    LatchObservation direct{};
    direct.event = LatchBoundaryEvent::direct_show_nonzero;
    direct.pre = consumed;
    direct.post = consumed;
    direct.wrapper_argument = 1;
    direct.attempt_outcome = ProducerAttemptOutcome::producer_early_return;
    CHECK(valid_latch_observation(direct));

    // Observed enum states -1 and 0 do not arm.
    for (const std::int32_t lifecycle : {-1, 0}) {
        const LatchSnapshot pre = latch(lifecycle, 0U, 0U, 0U);
        expected = expected_state_machine_step(pre, ProducerAttemptOutcome::not_attempted);
        CHECK(expected.valid_input && !expected.armed && !expected.attempted);
        CHECK(expected.post.arm == 0U && expected.post.pending == 0U);
    }
}

void fixed_scalar_hash_queue_rejects_invalid_full_busy_and_exhausted() {
    CaptureRecord record = producer_record();
    CHECK(valid_capture_record(record));
    MissionHeaderCaptureQueue queue;
    CHECK(queue.try_push(record) == QueuePushResult::enqueued);
    CaptureRecord output{};
    CHECK(queue.try_pop(output) == QueuePopResult::success);
    CHECK(output.sequence == 1U);
    CHECK(output.context == record.context);
    CHECK(output.producer.payload.full_payload_hash == exact_payload().full_payload_hash);
    ScalarHashTelemetry telemetry{};
    CHECK(default_telemetry(output, telemetry));
    CHECK(telemetry.sequence == 1U);
    CHECK(telemetry.activity_index == kActivityIndex);
    CHECK(telemetry.command == kActivityIntroCommand);
    CHECK(telemetry.payload_hash == exact_payload().full_payload_hash);

    CaptureRecord invalid = record;
    invalid.context.activity_index = 1U;
    CHECK(queue.try_push(invalid) == QueuePushResult::invalid);
    CHECK(queue.counters().rejected_invalid == 1U);

    FixedCaptureQueue<1U> full;
    CHECK(full.try_push(record) == QueuePushResult::enqueued);
    CHECK(full.try_push(record) == QueuePushResult::full);
    CHECK(full.counters().dropped_full == 1U);

    FixedCaptureQueue<1U> busy;
    CHECK(busy.testing_lock());
    CHECK(busy.try_push(record) == QueuePushResult::busy);
    busy.testing_unlock();
    CHECK(busy.counters().dropped_busy == 1U);

    FixedCaptureQueue<1U> exhausted;
    exhausted.testing_set_next_sequence((std::numeric_limits<std::uint64_t>::max)());
    CHECK(exhausted.try_push(record) == QueuePushResult::sequence_exhausted);
    CHECK(exhausted.counters().dropped_sequence_exhausted == 1U);

    CaptureRecord queueRecord{};
    queueRecord.context = exact_context();
    queueRecord.metadata = metadata(2U);
    queueRecord.kind = CaptureKind::queue_insert;
    queueRecord.queue_insert.command = kActivityIntroCommand;
    queueRecord.queue_insert.payload = exact_payload();
    queueRecord.queue_insert.pre = queue_snapshot();
    queueRecord.queue_insert.post = queueRecord.queue_insert.pre;
    queueRecord.queue_insert.disposition = QueueInsertDisposition::rejected_authored_policy;
    CHECK(valid_capture_record(queueRecord));

    CaptureRecord latchRecord{};
    latchRecord.context = exact_context();
    latchRecord.metadata = metadata(3U);
    latchRecord.kind = CaptureKind::latch;
    latchRecord.latch.event = LatchBoundaryEvent::state3_gate;
    latchRecord.latch.pre = latch(3, 1U, 1U, 1U);
    latchRecord.latch.post =
        expected_state_machine_step(latchRecord.latch.pre,
                                    ProducerAttemptOutcome::queue_rejected_full)
            .post;
    latchRecord.latch.attempt_outcome = ProducerAttemptOutcome::queue_rejected_full;
    latchRecord.latch.unconditional_pending_clear_observed = true;
    CHECK(valid_capture_record(latchRecord));
}

std::atomic<int> g_original_calls{};
std::atomic<int> g_pre_calls{};
std::atomic<int> g_post_calls{};

void void_original(int value) noexcept {
    if (value == 5) {
        g_original_calls.fetch_add(1, std::memory_order_relaxed);
    }
}

int value_original(int value) noexcept {
    g_original_calls.fetch_add(1, std::memory_order_relaxed);
    return value + 1;
}

void pre_observer() noexcept { g_pre_calls.fetch_add(1, std::memory_order_relaxed); }
void post_observer() noexcept { g_post_calls.fetch_add(1, std::memory_order_relaxed); }

void original_once_lifecycle_never_suppresses_or_duplicates_native_calls() {
    g_original_calls = 0;
    g_pre_calls = 0;
    g_post_calls = 0;
    OriginalOnceLifecycle lifecycle;

    forward_void_original_once(lifecycle,
                               &void_original,
                               &pre_observer,
                               &post_observer,
                               5);
    CHECK(g_original_calls == 1);
    CHECK(g_pre_calls == 0 && g_post_calls == 0);

    CHECK(lifecycle.activate());
    CHECK(!lifecycle.activate());
    CHECK(lifecycle.snapshot().observation_epoch == 1U);
    forward_void_original_once(lifecycle,
                               &void_original,
                               &pre_observer,
                               &post_observer,
                               5);
    CHECK(g_original_calls == 2);
    CHECK(g_pre_calls == 1 && g_post_calls == 1);
    const int value = forward_value_original_once(lifecycle,
                                                  &value_original,
                                                  &pre_observer,
                                                  &post_observer,
                                                  9);
    CHECK(value == 10);
    CHECK(g_original_calls == 3);
    CHECK(g_pre_calls == 2 && g_post_calls == 2);

    {
        OriginalOnceLifecycle::CallScope inFlight{lifecycle};
        CHECK(inFlight.accepts_observation());
        CHECK(lifecycle.snapshot().calls_in_flight == 1U);
        CHECK(lifecycle.begin_quiesce());
        CHECK(!lifecycle.try_detach());
        forward_void_original_once(lifecycle,
                                   &void_original,
                                   &pre_observer,
                                   &post_observer,
                                   5);
        CHECK(g_original_calls == 4);
        CHECK(g_pre_calls == 2 && g_post_calls == 2);
    }
    CHECK(lifecycle.try_detach());
    CHECK(lifecycle.snapshot().phase == LifecyclePhase::detached);
    CHECK(lifecycle.activate());
    CHECK(lifecycle.snapshot().observation_epoch == 2U);
    forward_void_original_once(lifecycle,
                               &void_original,
                               &pre_observer,
                               &post_observer,
                               5);
    CHECK(g_original_calls == 5);
    CHECK(g_pre_calls == 3 && g_post_calls == 3);
    CHECK(lifecycle.begin_quiesce());
    CHECK(lifecycle.try_detach());
}

} // namespace

int main() {
    observation_contract_and_lane_separation_are_explicit();
    exact_build_package_and_native_boundaries_are_pinned();
    exact_activity_title_category_and_evidence_grades_are_pinned();
    generation_and_build_context_must_be_exact();
    producer_early_returns_and_success_are_strict();
    native_queue_rejections_are_captured_without_writes();
    one_attempt_pending_clear_and_rearm_match_native_behavior();
    fixed_scalar_hash_queue_rejects_invalid_full_busy_and_exhausted();
    original_once_lifecycle_never_suppresses_or_duplicates_native_calls();

    if (g_failures != 0) {
        std::cerr << g_failures << " mission-header intro capture check(s) failed\n";
        return 1;
    }
    std::cout << "all mission-header intro capture checks passed\n";
    return 0;
}
