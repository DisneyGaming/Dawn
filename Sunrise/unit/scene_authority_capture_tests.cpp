#include <Windows.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <span>
#include <thread>
#include <type_traits>

#include "client/hooks/bootflow/opening_authority/scene_authority_capture.h"
#include "middleware/bap/activity_message/sensor_auth_update.h"

namespace {

using namespace sunrise::client::hooks::bootflow::opening_authority::scene_capture;

static_assert(kObservationOnly);
static_assert(!kOwnsNativeDetour);
static_assert(!kProvidesAuthorityWriter);
static_assert(!kProvidesSenseWriter);
static_assert(!kProvidesPublication);
static_assert(!kProvidesSceneCompletion);
static_assert(!kMutatesLegacyHandoffState);
static_assert(!sunrise::middleware::bap::activity_message::sensor_auth_update::
                  kOmegaSceneAuthorityBodyReady);
static_assert(kSceneDefinition != kType31DefinitionBa6);
static_assert(kSceneDefinition != kType31DefinitionBa9);
static_assert(kSceneAuthoritySchema != kSchedulerSelectorNotAuthority);
static_assert(std::is_trivially_copyable_v<AuthorityDecodeCapture>);
static_assert(std::is_trivially_copyable_v<SenseCapture>);
static_assert(std::is_trivially_copyable_v<ScalarHashObservation>);
static_assert(sizeof(ScalarHashObservation) < sizeof(AuthorityDecodeCapture));
static_assert(sizeof(ScalarHashObservation) < sizeof(SenseCapture));

int g_failureCount{};

void check(bool condition, const char* expression, int line) {
    if (condition) {
        return;
    }
    std::cerr << __FILE__ << ':' << line << ": check failed: " << expression << '\n';
    ++g_failureCount;
}

#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

template <typename Value>
void write_value(std::byte* destination, Value value) noexcept {
    std::memcpy(destination, &value, sizeof value);
}

[[nodiscard]] bool normalized_bit(std::span<const std::byte> body,
                                  std::size_t bit) noexcept {
    return (std::to_integer<std::uint8_t>(body[bit / 8U])
            & static_cast<std::uint8_t>(0x80U >> (bit % 8U)))
           != 0U;
}

void set_normalized_bit(std::span<std::byte> body, std::size_t bit, bool value) noexcept {
    const std::byte mask{static_cast<std::uint8_t>(0x80U >> (bit % 8U))};
    if (value) {
        body[bit / 8U] |= mask;
    } else {
        body[bit / 8U] &= ~mask;
    }
}

void write_msb_u32(std::span<std::byte> body,
                   std::size_t startBit,
                   std::uint32_t value) noexcept {
    for (std::size_t index = 0U; index < 32U; ++index) {
        const bool set = ((value >> (31U - index)) & 1U) != 0U;
        set_normalized_bit(body, startBit + index, set);
    }
}

[[nodiscard]] BuildEvidence exact_build() noexcept {
    BuildEvidence build{};
    build.unpacked_pc_sha256 = kPinnedUnpackedPcSha256;
    build.image_identity_valid = true;
    build.receive_outer_prefix_valid = true;
    build.authority_decode_prefix_valid = true;
    build.scene_apply_prefix_valid = true;
    build.component_start_prefix_valid = true;
    build.sense_outer_prefix_valid = true;
    build.sense_delta_prefix_valid = true;
    return build;
}

[[nodiscard]] ExactCaptureContext exact_context(std::uint64_t generation = 10U) noexcept {
    ExactCaptureContext context{};
    context.capture_epoch = generation + 1U;
    context.activation = {generation + 2U, generation + 3U};
    context.activity = {generation + 4U, generation + 5U, generation + 6U};
    context.session = {generation + 4U, generation + 7U, generation + 8U};
    context.run_token = generation + 9U;
    context.correlation_token = generation + 10U;
    context.authority_generation = generation + 11U;
    return context;
}

[[nodiscard]] SensorRecordLineage exact_sensor() noexcept {
    return {0x100000U, 91U, kExactSceneIdentity, 0x200000U};
}

[[nodiscard]] OwnerCallProvenance owner_provenance(std::uintptr_t entryRva,
                                                   std::uintptr_t callerRva,
                                                   std::uint64_t callId,
                                                   std::uint64_t parentCallId,
                                                   std::uint32_t threadId,
                                                   std::uint32_t depth,
                                                   std::uint64_t generation = 33U) noexcept {
    return {0xA000U + entryRva,
            generation,
            generation,
            callId,
            parentCallId,
            threadId,
            depth,
            entryRva,
            callerRva};
}

struct AuthorityFixture final {
    AuthorityDecoded decoded{};
    AuthorityRaw normalized{};
    std::array<std::byte, 256U> backing{};
    AuthorityDecodeFanoutRecord input{};
    std::size_t bits{};

    AuthorityFixture(std::uint32_t entryCount,
                     std::uint32_t wordCount,
                     std::uint32_t startBit = 5U) noexcept {
        write_value(decoded.data() + 0x08U, entryCount);
        write_value(decoded.data() + 0x4CU, 0xA1B2C3D4U);
        write_value(decoded.data() + 0x50U, wordCount);
        bits = 74U + 55U * entryCount + 32U * wordCount;
        for (std::size_t index = 0U; index < bits; ++index) {
            const bool set = ((index * 17U + 3U) % 11U) < 5U;
            set_normalized_bit(normalized, index, set);
            const std::size_t target = startBit + index;
            if (set) {
                backing[target / 8U] |=
                    std::byte{static_cast<std::uint8_t>(0x80U >> (target % 8U))};
            }
        }
        backing[(startBit - 1U) / 8U] |=
            std::byte{static_cast<std::uint8_t>(0x80U >> ((startBit - 1U) % 8U))};
        const std::size_t adjacent = startBit + bits;
        if (adjacent < backing.size() * 8U) {
            backing[adjacent / 8U] |=
                std::byte{static_cast<std::uint8_t>(0x80U >> (adjacent % 8U))};
        }

        input.build = exact_build();
        input.context = exact_context();
        input.sensor = exact_sensor();
        input.outer_owner = owner_provenance(kReceiveOuterRva, 0x123456U, 44U, 0U, 55U, 0U);
        input.decode_owner =
            owner_provenance(kAuthorityDecodeRva, kReceiveOuterRva + 0x120U, 45U, 44U, 55U, 1U);
        input.actual_schema_argument = kSceneAuthoritySchema;
        input.actual_mode_argument = 1U;
        input.mode_argument_captured = true;
        input.destination_address = input.sensor.expected_received_authority_destination;
        input.reader = {{backing.data(), backing.size()},
                        reinterpret_cast<std::uintptr_t>(backing.data()),
                        backing.size(),
                        startBit,
                        static_cast<std::uint32_t>(startBit + bits),
                        0U,
                        0U,
                        CopyOutcome::complete,
                        true};
        input.decoder_returned_true = true;
        input.inout_bytes_before = static_cast<std::int32_t>(kAuthorityDecodedBytes);
        input.inout_bytes_after = static_cast<std::int32_t>(kAuthorityDecodedBytes);
        input.decoded = decoded;
        input.decoded_copy = CopyOutcome::complete;
        input.source_copy_timing = CopyTiming::after_original_before_owner_exit;
    }
};

[[nodiscard]] SceneApplyFanoutRecord exact_apply(
    const AuthorityDecodeCapture& decode) noexcept {
    SceneApplyFanoutRecord apply{};
    apply.build = decode.build;
    apply.context = decode.context;
    apply.component = {0x310000U,
                       0x320000U,
                       0x300000U,
                       0x330000U,
                       101U,
                       kSceneDefinition,
                       0x80F47B74U,
                       0x310U,
                       decode.context.activation};
    apply.apply_owner = owner_provenance(kSceneApplyRva, 0x400000U, 103U, 0U, 104U, 0U, 102U);
    apply.component_owner =
        owner_provenance(kComponentStartRva, 0x410000U, 105U, 0U, 104U, 0U, 106U);
    write_value(apply.state_key.data(), kSceneAuthoritySchema);
    write_value(apply.state_key.data() + 0x04U, 0x11223344U);
    write_value(apply.state_key.data() + 0x08U,
                static_cast<std::uint64_t>(decode.destination_address));
    apply.decoded_source_address = decode.destination_address;
    apply.decoded_source = decode.decoded;
    apply.decoded_source_copy = CopyOutcome::complete;
    apply.decoded_source_copy_timing = CopyTiming::before_original;
    for (std::size_t index = 0U; index < kComponentCommittedPrefixBytes; ++index) {
        apply.component_180_19f_before[index] =
            std::byte{static_cast<std::uint8_t>(0x20U + index)};
        apply.component_180_19f_after[index] =
            std::byte{static_cast<std::uint8_t>(0x70U + index)};
    }
    apply.component_before_copy = CopyOutcome::complete;
    apply.component_after_copy = CopyOutcome::complete;
    apply.component_before_copy_timing = CopyTiming::before_original;
    apply.component_after_copy_timing = CopyTiming::after_original_before_owner_exit;
    apply.derived_scalar_before = 0x01020304U;
    apply.derived_scalar_after = 0xA1B2C3D4U;
    apply.derived_active_before = 0U;
    apply.derived_active_after = decode.shape.entry_count != 0U ? 1U : 0U;
    apply.original_call_audit = {1U, 1U, 1U};
    return apply;
}

[[nodiscard]] NativeOracleObservation exact_oracle(
    const AuthorityEvidenceRecord& record) noexcept {
    NativeOracleObservation oracle{};
    oracle.measurement_succeeded = true;
    oracle.rounded_bytes = static_cast<std::int32_t>((record.decode.body_bits + 7U) / 8U);
    oracle.measurement_private_storage_only = true;
    oracle.first.bytes = record.decode.raw;
    oracle.first.cursor_bits = record.decode.body_bits;
    oracle.first.writer_status_good = true;
    oracle.first.private_storage_only = true;
    oracle.second = oracle.first;
    return oracle;
}

[[nodiscard]] SenseRaw make_sense_body(std::size_t bodyBits,
                                       std::uint32_t revision) noexcept {
    SenseRaw body{};
    const std::size_t deltaBits = bodyBits - kSenseRevisionBits;
    for (std::size_t index = 0U; index < deltaBits; ++index) {
        set_normalized_bit(body, index, ((index * 7U + 1U) % 9U) < 4U);
    }
    write_msb_u32(body, deltaBits, revision);
    return body;
}

struct SenseFixture final {
    SenseRaw normalized{};
    std::array<std::byte, 640U> backing{};
    SenseFanoutRecord input{};

    SenseFixture(std::span<const std::byte> body,
                 std::size_t bodyBits,
                 std::size_t deltaBits,
                 std::uint32_t acceptedRevision,
                 std::size_t bodyStart = 4U,
                 std::size_t completedBits = 128U) noexcept {
        const std::size_t bodyBytes = (bodyBits + 7U) / 8U;
        std::copy(body.begin(), body.begin() + static_cast<std::ptrdiff_t>(bodyBytes),
                  normalized.begin());
        const std::size_t bodyEnd = bodyStart + bodyBits;
        for (std::size_t logical = 0U; logical < completedBits; ++logical) {
            bool set{};
            if (logical >= bodyStart && logical < bodyEnd) {
                set = normalized_bit(normalized, logical - bodyStart);
            }
            if (set) {
                backing[logical / 8U] |=
                    std::byte{static_cast<std::uint8_t>(0x80U >> (logical % 8U))};
            }
        }
        const std::size_t pendingBits = bodyEnd - completedBits;
        std::uint64_t pendingWord{};
        for (std::size_t index = 0U; index < pendingBits; ++index) {
            const std::size_t logical = completedBits + index;
            const bool set = logical >= bodyStart && logical < bodyEnd
                                 ? normalized_bit(normalized, logical - bodyStart)
                                 : false;
            pendingWord = (pendingWord << 1U) | (set ? 1U : 0U);
        }

        input.build = exact_build();
        input.context = exact_context(70U);
        input.sensor = exact_sensor();
        input.outer_owner = owner_provenance(kSenseOuterRva, 0x420000U, 72U, 0U, 73U, 0U, 71U);
        input.delta_owner =
            owner_provenance(kSenseDeltaRva, kSenseOuterRva + 0x80U, 74U, 72U, 73U, 1U, 75U);
        input.actual_delta_schema_argument = kSceneSenseSchema;
        input.current_source_address = 0x430000U;
        input.prior_source_address = 0x440000U;
        input.body_start = bodyStart;
        input.delta_end = bodyStart + deltaBits;
        input.body_end = bodyEnd;
        input.accepted_revision = acceptedRevision;
        input.writer_at_outer_return = {{backing.data(), backing.size()},
                                        reinterpret_cast<std::uintptr_t>(backing.data()),
                                        backing.size(),
                                        completedBits,
                                        pendingWord,
                                        static_cast<std::uint32_t>(pendingBits),
                                        0U,
                                        CopyOutcome::complete};
    }
};

void exact_gates_keep_scene_and_type31_separate() {
    CHECK(exact_scene_identity(kExactSceneIdentity));
    SceneIdentity drift = kExactSceneIdentity;
    drift.type = 31U;
    CHECK(!exact_scene_identity(drift));
    drift = kExactSceneIdentity;
    drift.index = 18U;
    CHECK(!exact_scene_identity(drift));
    drift = kExactSceneIdentity;
    drift.authority_schema = kSceneSenseSchema;
    CHECK(!exact_scene_identity(drift));
    drift = kExactSceneIdentity;
    drift.authority_schema = kSchedulerSelectorNotAuthority;
    CHECK(!exact_scene_identity(drift));
    CHECK(is_type31_definition(kType31DefinitionBa6));
    CHECK(is_type31_definition(kType31DefinitionBa9));
    CHECK(!is_type31_definition(kSceneDefinition));
    CHECK(definition_role(kSceneDefinition) == DefinitionRole::scene_type43);
    CHECK(definition_role(kType31DefinitionBa6) == DefinitionRole::type31);
    CHECK(definition_role(kType31DefinitionBa9) == DefinitionRole::type31);
    CHECK(definition_role(kSchedulerSelectorNotAuthority) == DefinitionRole::unknown);

    const ExactCaptureContext context = exact_context();
    CHECK(valid_exact_context(context));
    ExactCaptureContext partial = context;
    partial.activation.activation_generation = 0U;
    CHECK(!valid_exact_context(partial));
    partial = context;
    partial.session.session_id++;
    CHECK(!valid_exact_context(partial));
    partial = context;
    partial.correlation_token = 0U;
    CHECK(!valid_exact_context(partial));
}

void full_consumed_interval_minimum_maximum_and_alignment() {
    AuthorityFixture minimum(0U, 0U, 3U);
    AuthorityDecodeCapture minimumCapture{};
    CHECK(capture_authority_decode(minimum.input, minimumCapture)
          == AuthorityCaptureResult::complete);
    CHECK(minimumCapture.body_bits == kAuthorityMinimumBits);
    CHECK(minimumCapture.start_bit == 3U);
    CHECK(minimumCapture.end_bit == 77U);
    CHECK(significant_bits_equal(minimumCapture.raw, minimum.normalized, minimum.bits));
    CHECK(unused_low_bits_zero(minimumCapture.raw, minimum.bits));
    CHECK(minimumCapture.shape.entry_count == 0U);
    CHECK(minimumCapture.shape.word_count == 0U);

    AuthorityFixture maximum(8U, 32U, 5U);
    AuthorityDecodeCapture maximumCapture{};
    CHECK(capture_authority_decode(maximum.input, maximumCapture)
          == AuthorityCaptureResult::complete);
    CHECK(maximumCapture.body_bits == kAuthorityMaximumBits);
    CHECK(maximumCapture.shape.entry_count == 8U);
    CHECK(maximumCapture.shape.word_count == 32U);
    CHECK(significant_bits_equal(maximumCapture.raw, maximum.normalized, maximum.bits));
    const AuthorityRaw beforeAdjacentMutation = maximumCapture.raw;
    const std::size_t adjacent = maximum.input.reader.end_bit;
    maximum.backing[adjacent / 8U] ^=
        std::byte{static_cast<std::uint8_t>(0x80U >> (adjacent % 8U))};
    AuthorityDecodeCapture afterAdjacentMutation{};
    CHECK(capture_authority_decode(maximum.input, afterAdjacentMutation)
          == AuthorityCaptureResult::complete);
    CHECK(afterAdjacentMutation.raw == beforeAdjacentMutation);

    AuthorityRaw differentAfter64 = minimumCapture.raw;
    set_normalized_bit(differentAfter64, 70U, !normalized_bit(differentAfter64, 70U));
    CHECK(!significant_bits_equal(minimumCapture.raw, differentAfter64, minimum.bits));
    CHECK(significant_bits_equal(minimumCapture.raw, differentAfter64, 64U));
}

void authority_partial_fault_bounds_and_shape_fail_closed() {
    AuthorityFixture fixture(2U, 3U);
    AuthorityDecodeCapture output{};

    fixture.input.reader.backing_copy = CopyOutcome::partial;
    CHECK(capture_authority_decode(fixture.input, output)
          == AuthorityCaptureResult::source_partial);
    fixture.input.decoded_copy = CopyOutcome::fault;
    CHECK(capture_authority_decode(fixture.input, output)
          == AuthorityCaptureResult::source_fault);
    fixture.input.reader.backing_copy = CopyOutcome::complete;
    CHECK(capture_authority_decode(fixture.input, output)
          == AuthorityCaptureResult::source_fault);
    fixture.input.decoded_copy = CopyOutcome::complete;
    fixture.input.reader.error_after = 1U;
    CHECK(capture_authority_decode(fixture.input, output)
          == AuthorityCaptureResult::reader_error);
    fixture.input.reader.error_after = 0U;
    fixture.input.reader.error_before = 1U;
    CHECK(capture_authority_decode(fixture.input, output)
          == AuthorityCaptureResult::reader_error);
    fixture.input.reader.error_before = 0U;
    fixture.input.decoder_returned_true = false;
    CHECK(capture_authority_decode(fixture.input, output)
          == AuthorityCaptureResult::native_failure);
    fixture.input.decoder_returned_true = true;
    fixture.input.inout_bytes_before = 0xD3;
    CHECK(capture_authority_decode(fixture.input, output) == AuthorityCaptureResult::wrong_size);
    fixture.input.inout_bytes_before = 0xD4;
    fixture.input.reader.capacity_bytes = 1U;
    CHECK(capture_authority_decode(fixture.input, output)
          == AuthorityCaptureResult::reader_bounds);
    fixture.input.reader.capacity_bytes = fixture.backing.size();
    fixture.input.reader.end_bit = fixture.input.reader.start_bit - 1U;
    CHECK(capture_authority_decode(fixture.input, output)
          == AuthorityCaptureResult::reader_bounds);

    AuthorityFixture illegalN(8U, 0U);
    write_value(illegalN.input.decoded.data() + 0x08U, 9U);
    CHECK(capture_authority_decode(illegalN.input, output)
          == AuthorityCaptureResult::illegal_shape);
    AuthorityFixture illegalM(0U, 32U);
    write_value(illegalM.input.decoded.data() + 0x50U, 33U);
    CHECK(capture_authority_decode(illegalM.input, output)
          == AuthorityCaptureResult::illegal_shape);
    AuthorityFixture wrongLength(1U, 1U);
    --wrongLength.input.reader.end_bit;
    CHECK(capture_authority_decode(wrongLength.input, output)
          == AuthorityCaptureResult::length_mismatch);
    AuthorityFixture stale(0U, 0U);
    stale.input.sensor.record_generation = 0U;
    CHECK(capture_authority_decode(stale.input, output)
          == AuthorityCaptureResult::stale_sensor);
    AuthorityFixture wrongDefinition(0U, 0U);
    wrongDefinition.input.sensor.identity.type = 31U;
    CHECK(capture_authority_decode(wrongDefinition.input, output)
          == AuthorityCaptureResult::wrong_sensor);
    AuthorityFixture includesOuterHeader(0U, 0U);
    includesOuterHeader.input.reader.nested_reflected_body_interval = false;
    CHECK(capture_authority_decode(includesOuterHeader.input, output)
          == AuthorityCaptureResult::owner_pair_mismatch);
    AuthorityFixture wrongBuild(0U, 0U);
    wrongBuild.input.build.authority_decode_prefix_valid = false;
    CHECK(capture_authority_decode(wrongBuild.input, output)
          == AuthorityCaptureResult::wrong_build);
    AuthorityFixture wrongNativeSchema(0U, 0U);
    wrongNativeSchema.input.actual_schema_argument = kSceneSenseSchema;
    CHECK(capture_authority_decode(wrongNativeSchema.input, output)
          == AuthorityCaptureResult::wrong_sensor);
    wrongNativeSchema.input.actual_schema_argument = kSceneAuthoritySchema;
    wrongNativeSchema.input.mode_argument_captured = false;
    CHECK(capture_authority_decode(wrongNativeSchema.input, output)
          == AuthorityCaptureResult::wrong_sensor);
    AuthorityFixture staleOwner(0U, 0U);
    staleOwner.input.decode_owner.generation_at_exit++;
    CHECK(capture_authority_decode(staleOwner.input, output)
          == AuthorityCaptureResult::owner_pair_mismatch);
    AuthorityFixture badCaller(0U, 0U);
    badCaller.input.decode_owner.native_caller_rva =
        kReceiveOuterRva + kReceiveOuterRecoveredBytes;
    CHECK(capture_authority_decode(badCaller.input, output)
          == AuthorityCaptureResult::owner_pair_mismatch);
}

void full_component_snapshots_and_apply_semantics() {
    AuthorityFixture fixture(2U, 1U);
    AuthorityDecodeCapture decode{};
    CHECK(capture_authority_decode(fixture.input, decode) == AuthorityCaptureResult::complete);
    SceneApplyFanoutRecord apply = exact_apply(decode);
    AuthorityEvidenceRecord evidence{};
    CHECK(correlate_scene_apply(decode, apply, evidence) == ApplyCorrelationResult::complete);
    CHECK(evidence.component.definition == kSceneDefinition);
    CHECK(evidence.component_180_19f_before == apply.component_180_19f_before);
    CHECK(evidence.component_180_19f_after == apply.component_180_19f_after);
    CHECK(evidence.derived_scalar_after == 0xA1B2C3D4U);
    CHECK(evidence.derived_active_after == 1U);

    SceneApplyFanoutRecord rejected = apply;
    rejected.component.definition = kType31DefinitionBa6;
    CHECK(correlate_scene_apply(decode, rejected, evidence)
          == ApplyCorrelationResult::wrong_definition);
    rejected = apply;
    rejected.component.definition = kType31DefinitionBa9;
    CHECK(correlate_scene_apply(decode, rejected, evidence)
          == ApplyCorrelationResult::wrong_definition);
    rejected = apply;
    write_value(rejected.state_key.data(), kSchedulerSelectorNotAuthority);
    CHECK(correlate_scene_apply(decode, rejected, evidence)
          == ApplyCorrelationResult::wrong_state_key);
    rejected = apply;
    rejected.component.activation.activation_generation++;
    CHECK(correlate_scene_apply(decode, rejected, evidence)
          == ApplyCorrelationResult::stale_component);
    rejected = apply;
    rejected.component_after_copy = CopyOutcome::partial;
    CHECK(correlate_scene_apply(decode, rejected, evidence)
          == ApplyCorrelationResult::component_partial);
    rejected = apply;
    rejected.component_before_copy = CopyOutcome::partial;
    rejected.component_after_copy = CopyOutcome::fault;
    CHECK(correlate_scene_apply(decode, rejected, evidence)
          == ApplyCorrelationResult::component_fault);
    rejected = apply;
    rejected.derived_scalar_after++;
    CHECK(correlate_scene_apply(decode, rejected, evidence)
          == ApplyCorrelationResult::apply_semantic_mismatch);
    rejected = apply;
    rejected.original_call_audit.original_calls = 0U;
    CHECK(correlate_scene_apply(decode, rejected, evidence)
          == ApplyCorrelationResult::original_count_mismatch);
}

void oracle_is_private_exact_and_never_publishes() {
    AuthorityFixture fixture(1U, 2U);
    AuthorityDecodeCapture decode{};
    CHECK(capture_authority_decode(fixture.input, decode) == AuthorityCaptureResult::complete);
    AuthorityEvidenceRecord evidence{};
    CHECK(correlate_scene_apply(decode, exact_apply(decode), evidence)
          == ApplyCorrelationResult::complete);
    NativeOracleObservation oracle = exact_oracle(evidence);
    CHECK(validate_native_oracle(evidence, oracle) == OracleResult::accepted_observation);

    NativeOracleObservation rejected = oracle;
    rejected.measurement_succeeded = false;
    CHECK(validate_native_oracle(evidence, rejected) == OracleResult::measurement_failure);
    rejected = oracle;
    rejected.measurement_private_storage_only = false;
    CHECK(validate_native_oracle(evidence, rejected) == OracleResult::measurement_not_private);
    rejected = oracle;
    rejected.rounded_bytes++;
    CHECK(validate_native_oracle(evidence, rejected) == OracleResult::rounded_size_mismatch);
    rejected = oracle;
    rejected.first.cursor_bits--;
    CHECK(validate_native_oracle(evidence, rejected) == OracleResult::cursor_mismatch);
    rejected = oracle;
    rejected.first.writer_status_good = false;
    CHECK(validate_native_oracle(evidence, rejected) == OracleResult::writer_failure);
    rejected = oracle;
    rejected.first.private_storage_only = false;
    CHECK(validate_native_oracle(evidence, rejected) == OracleResult::writer_not_private);
    rejected = oracle;
    set_normalized_bit(rejected.first.bytes, evidence.decode.body_bits, true);
    set_normalized_bit(rejected.second.bytes, evidence.decode.body_bits, true);
    CHECK(validate_native_oracle(evidence, rejected) == OracleResult::padding_failure);
    rejected = oracle;
    const std::size_t lastBit = evidence.decode.body_bits;
    set_normalized_bit(rejected.first.bytes, lastBit - 1U,
                       !normalized_bit(rejected.first.bytes, lastBit - 1U));
    CHECK(validate_native_oracle(evidence, rejected) == OracleResult::nondeterministic);
    rejected = oracle;
    set_normalized_bit(rejected.first.bytes, 80U,
                       !normalized_bit(rejected.first.bytes, 80U));
    rejected.second = rejected.first;
    CHECK(validate_native_oracle(evidence, rejected)
          == OracleResult::significant_bit_mismatch);
    rejected = oracle;
    rejected.publication_or_packet_call_observed = true;
    CHECK(validate_native_oracle(evidence, rejected) == OracleResult::publication_attempted);
    CHECK(!kProvidesPublication);
    CHECK(!sunrise::middleware::bap::activity_message::sensor_auth_update::
              kOmegaSceneAuthorityBodyReady);
}

void frozen_evidence_one_field_mutations_fail_closed() {
    AuthorityFixture fixture(1U, 2U);
    AuthorityDecodeCapture decode{};
    CHECK(capture_authority_decode(fixture.input, decode) == AuthorityCaptureResult::complete);
    AuthorityEvidenceRecord evidence{};
    CHECK(correlate_scene_apply(decode, exact_apply(decode), evidence)
          == ApplyCorrelationResult::complete);
    const NativeOracleObservation oracle = exact_oracle(evidence);
    CHECK(validate_frozen_authority_evidence(evidence) == FrozenAuthorityValidation::valid);

    const auto rejects = [&](AuthorityEvidenceRecord mutated) {
        CHECK(validate_frozen_authority_evidence(mutated) != FrozenAuthorityValidation::valid);
        CHECK(validate_native_oracle(mutated, oracle) == OracleResult::incomplete_record);
    };

    AuthorityEvidenceRecord mutated = evidence;
    mutated.capture_format_version++;
    rejects(mutated);
    mutated = evidence;
    mutated.decode.build.image_identity_valid = false;
    rejects(mutated);
    mutated = evidence;
    mutated.decode.context.session.record_revision = 0U;
    rejects(mutated);
    mutated = evidence;
    mutated.decode.sensor.identity.type = 31U;
    rejects(mutated);
    mutated = evidence;
    mutated.decode.sensor.record_generation = 0U;
    rejects(mutated);
    mutated = evidence;
    mutated.decode.decode_owner.generation_at_exit++;
    rejects(mutated);
    mutated = evidence;
    mutated.decode.actual_schema_argument = kSceneSenseSchema;
    rejects(mutated);
    mutated = evidence;
    mutated.decode.destination_address++;
    rejects(mutated);
    mutated = evidence;
    mutated.decode.reader_buffer_address = 0U;
    rejects(mutated);
    mutated = evidence;
    mutated.decode.error_after = 1U;
    rejects(mutated);
    mutated = evidence;
    mutated.decode.decoder_returned_true = false;
    rejects(mutated);
    mutated = evidence;
    mutated.decode.inout_bytes_after = 0xD3;
    rejects(mutated);
    mutated = evidence;
    mutated.decode.raw_hash++;
    rejects(mutated);
    mutated = evidence;
    mutated.decode.decoded_hash++;
    rejects(mutated);
    mutated = evidence;
    mutated.decode.shape.entry_count++;
    rejects(mutated);
    mutated = evidence;
    mutated.decode.decoded[0x30U] ^= std::byte{1U};
    rejects(mutated);
    mutated = evidence;
    set_normalized_bit(mutated.decode.raw, mutated.decode.body_bits, true);
    rejects(mutated);
    mutated = evidence;
    write_value(mutated.state_key.data(), kSchedulerSelectorNotAuthority);
    rejects(mutated);
    mutated = evidence;
    mutated.apply_source_address++;
    rejects(mutated);
    mutated = evidence;
    mutated.apply_source[0x30U] ^= std::byte{1U};
    rejects(mutated);
    mutated = evidence;
    mutated.apply_source_copy = CopyOutcome::partial;
    rejects(mutated);
    mutated = evidence;
    mutated.component.definition = kType31DefinitionBa6;
    rejects(mutated);
    mutated = evidence;
    mutated.component.activation.activation_generation++;
    rejects(mutated);
    mutated = evidence;
    mutated.component_after_copy = CopyOutcome::fault;
    rejects(mutated);
    mutated = evidence;
    mutated.apply_owner.generation_at_exit++;
    rejects(mutated);
    mutated = evidence;
    mutated.original_call_audit.original_calls = 2U;
    rejects(mutated);
    mutated = evidence;
    mutated.derived_scalar_after++;
    rejects(mutated);
    mutated = evidence;
    mutated.derived_active_after ^= 1U;
    rejects(mutated);
}

void sense_cursor_delta_and_revisions_are_exact() {
    SenseFixture frozen(kFrozenSenseFixture140, 140U, 108U, 9U);
    SenseCapture capture{};
    CHECK(capture_scene_sense(frozen.input, capture) == SenseCaptureResult::complete);
    CHECK(capture.body_bits == 140U);
    CHECK(capture.delta_bits == 108U);
    CHECK(capture.next_revision == 10U);
    CHECK(matches_frozen_140_fixture(capture));
    CHECK(significant_bits_equal(capture.body, kFrozenSenseFixture140, 140U));
    SceneSenseServiceSplit serviceSplit{};
    CHECK(split_scene_sense_service_observation(kExactSceneIdentity,
                                                capture.body,
                                                capture.body_bits,
                                                serviceSplit));
    CHECK(serviceSplit.delta_bits == 108U);
    CHECK(serviceSplit.next_revision == 10U);
    CHECK(serviceSplit.retained_fixture_width);
    CHECK(serviceSplit.exact_140_fixture);
    SceneIdentity wrongServiceIdentity = kExactSceneIdentity;
    wrongServiceIdentity.type = 31U;
    CHECK(!split_scene_sense_service_observation(wrongServiceIdentity,
                                                 capture.body,
                                                 capture.body_bits,
                                                 serviceSplit));

    for (const auto [bits, revision] :
         std::array<std::pair<std::size_t, std::uint32_t>, 3U>{
             std::pair<std::size_t, std::uint32_t>{75U, 1U},
             {108U, 8U},
             {140U, 10U}}) {
        const SenseRaw body = make_sense_body(bits, revision);
        const std::size_t completed = bits + 4U > 64U ? bits + 4U - 32U : 8U;
        SenseFixture retained(body, bits, bits - 32U, revision - 1U, 4U, completed);
        SenseCapture retainedCapture{};
        CHECK(capture_scene_sense(retained.input, retainedCapture)
              == SenseCaptureResult::complete);
        CHECK(retainedCapture.delta_bits == bits - 32U);
        CHECK(retainedCapture.next_revision == revision);
    }

    const SenseRaw wrappedBody = make_sense_body(75U, 0U);
    SenseFixture wrappedRevision(wrappedBody,
                                 75U,
                                 43U,
                                 (std::numeric_limits<std::uint32_t>::max)(),
                                 4U,
                                 47U);
    SenseCapture wrappedCapture{};
    CHECK(capture_scene_sense(wrappedRevision.input, wrappedCapture)
          == SenseCaptureResult::complete);
    CHECK(wrappedCapture.next_revision == 0U);

    SenseFanoutRecord rejected = frozen.input;
    rejected.accepted_revision = 8U;
    CHECK(capture_scene_sense(rejected, capture) == SenseCaptureResult::revision_mismatch);
    rejected = frozen.input;
    rejected.delta_end++;
    CHECK(capture_scene_sense(rejected, capture)
          == SenseCaptureResult::missing_revision_trailer);
    rejected = frozen.input;
    rejected.delta_owner.producer_thread_id++;
    CHECK(capture_scene_sense(rejected, capture) == SenseCaptureResult::wrong_thread);
    rejected = frozen.input;
    rejected.delta_owner.parent_call_id++;
    CHECK(capture_scene_sense(rejected, capture)
          == SenseCaptureResult::owner_pair_mismatch);
    rejected = frozen.input;
    rejected.writer_at_outer_return.backing_copy = CopyOutcome::partial;
    CHECK(capture_scene_sense(rejected, capture) == SenseCaptureResult::source_partial);
    rejected = frozen.input;
    rejected.writer_at_outer_return.error = 1U;
    CHECK(capture_scene_sense(rejected, capture) == SenseCaptureResult::writer_error);
    rejected = frozen.input;
    rejected.sensor.identity.sense_schema = kSceneAuthoritySchema;
    CHECK(capture_scene_sense(rejected, capture) == SenseCaptureResult::wrong_sensor);
    rejected = frozen.input;
    rejected.actual_delta_schema_argument = kSceneAuthoritySchema;
    CHECK(capture_scene_sense(rejected, capture) == SenseCaptureResult::wrong_sensor);
    rejected = frozen.input;
    rejected.current_source_address = 0U;
    CHECK(capture_scene_sense(rejected, capture) == SenseCaptureResult::wrong_sensor);
    rejected = frozen.input;
    rejected.writer_at_outer_return.capacity_bytes = 18U;
    CHECK(capture_scene_sense(rejected, capture) == SenseCaptureResult::complete);
    rejected.writer_at_outer_return.capacity_bytes = 17U;
    CHECK(capture_scene_sense(rejected, capture) == SenseCaptureResult::cursor_mismatch);
    rejected = frozen.input;
    rejected.writer_at_outer_return.buffer_address = 0U;
    CHECK(capture_scene_sense(rejected, capture) == SenseCaptureResult::cursor_mismatch);
    rejected = frozen.input;
    rejected.body_end = rejected.body_start + kSenseCaptureStorageBits + 1U;
    rejected.delta_end = rejected.body_end - kSenseRevisionBits;
    rejected.writer_at_outer_return.completed_bits = rejected.body_end;
    rejected.writer_at_outer_return.pending_bits = 0U;
    CHECK(capture_scene_sense(rejected, capture) == SenseCaptureResult::storage_exhausted);

    const SenseRaw pending64Body = make_sense_body(75U, 3U);
    SenseFixture pending64(pending64Body, 75U, 43U, 2U, 4U, 15U);
    pending64.input.writer_at_outer_return.capacity_bytes = 10U;
    CHECK(pending64.input.writer_at_outer_return.pending_bits == 64U);
    CHECK(capture_scene_sense(pending64.input, capture) == SenseCaptureResult::complete);

    WriterBitView overflowWriter = frozen.input.writer_at_outer_return;
    overflowWriter.completed_bits = (std::numeric_limits<std::size_t>::max)() - 31U;
    overflowWriter.pending_bits = 64U;
    SenseRaw ignored{};
    CHECK(normalize_writer_interval(overflowWriter, 0U, 1U, ignored)
          == IntervalResult::capacity_overflow);
}

void legacy_prefix_and_retirement_are_observation_only() {
    struct RouteState final {
        bool handoff_armed{};
        bool scene_completed{};
        std::uint8_t opening_stage{};
    };
    const RouteState routeBefore{false, false, 3U};
    RouteState routeAfter = routeBefore;
    SenseFixture current(kFrozenSenseFixture140, 140U, 108U, 9U);
    SenseCapture currentCapture{};
    CHECK(capture_scene_sense(current.input, currentCapture) == SenseCaptureResult::complete);
    const LegacyPrefixObservation exact =
        observe_legacy_64_of_140(currentCapture.body, currentCapture.body_bits);
    CHECK(exact.width_is_140);
    CHECK(exact.first64_matches);
    CHECK(exact.full_fixture_matches);
    CHECK(exact.known_stateful_consumer_mask == kLegacyStatefulConsumerMask);
    CHECK(!exact.authorizes_authority);
    CHECK(!exact.authorizes_completion);
    CHECK(!exact.mutates_state);
    CHECK(routeAfter.handoff_armed == routeBefore.handoff_armed);
    CHECK(routeAfter.scene_completed == routeBefore.scene_completed);
    CHECK(routeAfter.opening_stage == routeBefore.opening_stage);

    SenseRaw historical = currentCapture.body;
    write_msb_u32(historical, 108U, 8U);
    const LegacyPrefixObservation old = observe_legacy_64_of_140(historical, 140U);
    CHECK(old.first64_matches);
    CHECK(!old.full_fixture_matches);
    CHECK(significant_bits_equal(historical, currentCapture.body, 108U));
    CHECK(significant_bits_equal(historical, currentCapture.body, 128U));
    CHECK(!significant_bits_equal(historical, currentCapture.body, 140U));

    CHECK(!retirement_is_scene_completion(RetirementObservation::flat_mailbox));
    CHECK(!retirement_is_scene_completion(RetirementObservation::generation_tagged_mailbox));
    CHECK(!retirement_is_scene_completion(RetirementObservation::local_wrapper_58b9a0));
}

void full_evidence_ring_owns_immutable_raw_records() {
    AuthorityFixture fixture(2U, 2U);
    AuthorityDecodeCapture decode{};
    CHECK(capture_authority_decode(fixture.input, decode) == AuthorityCaptureResult::complete);
    const AuthorityDecodeCapture expectedDecode = decode;

    FullEvidenceRing decodeRing;
    CHECK(decodeRing.try_commit(decode) == FullEvidenceCommitResult::committed);
    decode.raw.fill(std::byte{0xFFU});
    decode.decoded.fill(std::byte{0xFFU});

    FullEvidenceRecord popped{};
    CHECK(decodeRing.try_pop(popped) == FullEvidenceReadResult::success);
    CHECK(popped.kind == FullEvidenceKind::authority_decode);
    CHECK(popped.authority_decode.raw == expectedDecode.raw);
    CHECK(popped.authority_decode.decoded == expectedDecode.decoded);
    CHECK(validate_frozen_authority_decode(popped.authority_decode)
          == FrozenAuthorityValidation::valid);
    ScalarHashObservation decodeScalar{};
    CHECK(telemetry(popped, decodeScalar));
    CHECK(decodeScalar.kind == ObservationKind::authority_decode);
    decode = expectedDecode;

    AuthorityEvidenceRecord evidence{};
    CHECK(correlate_scene_apply(decode, exact_apply(decode), evidence)
          == ApplyCorrelationResult::complete);
    const AuthorityEvidenceRecord expected = evidence;

    FullEvidenceRing ring;
    CHECK(ring.try_commit(evidence) == FullEvidenceCommitResult::committed);
    evidence.decode.raw.fill(std::byte{0xFFU});
    evidence.decode.decoded.fill(std::byte{0xFFU});
    evidence.apply_source.fill(std::byte{0xFFU});
    evidence.component_180_19f_after.fill(std::byte{0xFFU});

    CHECK(ring.try_pop(popped) == FullEvidenceReadResult::success);
    CHECK(popped.capture_format_version == kCaptureFormatVersion);
    CHECK(popped.committed_sequence == 1U);
    CHECK(popped.kind == FullEvidenceKind::authority);
    CHECK(popped.authority.decode.raw == expected.decode.raw);
    CHECK(popped.authority.decode.decoded == expected.decode.decoded);
    CHECK(popped.authority.apply_source == expected.apply_source);
    CHECK(popped.authority.component_180_19f_after
          == expected.component_180_19f_after);
    CHECK(validate_frozen_authority_evidence(popped.authority)
          == FrozenAuthorityValidation::valid);

    AuthorityEvidenceRecord invalid = expected;
    invalid.decode.raw_hash++;
    CHECK(ring.try_commit(invalid) == FullEvidenceCommitResult::rejected);
    ring.account_source_outcome(CopyOutcome::partial);
    ring.account_source_outcome(CopyOutcome::fault);
    CHECK(ring.counters().rejected == 1U);
    CHECK(ring.counters().source_partial == 1U);
    CHECK(ring.counters().source_fault == 1U);

    FullEvidenceRing full;
    for (std::size_t index = 0U; index < kFullEvidenceRingCapacity; ++index) {
        AuthorityEvidenceRecord distinct = expected;
        distinct.decode.context.correlation_token += index;
        CHECK(full.try_commit(distinct) == FullEvidenceCommitResult::committed);
    }
    CHECK(full.try_commit(expected) == FullEvidenceCommitResult::full);
    CHECK(full.testing_lock());
    CHECK(full.try_commit(expected) == FullEvidenceCommitResult::busy);
    full.testing_unlock();

    FullEvidenceRing exhausted;
    exhausted.testing_set_next_sequence((std::numeric_limits<std::uint64_t>::max)());
    CHECK(exhausted.try_commit(expected) == FullEvidenceCommitResult::sequence_exhausted);

    SenseFixture senseFixture(kFrozenSenseFixture140, 140U, 108U, 9U);
    SenseCapture sense{};
    CHECK(capture_scene_sense(senseFixture.input, sense) == SenseCaptureResult::complete);
    FullEvidenceRing senseRing;
    CHECK(senseRing.try_commit(sense) == FullEvidenceCommitResult::committed);
    sense.body.fill(std::byte{0U});
    CHECK(senseRing.try_pop(popped) == FullEvidenceReadResult::success);
    CHECK(popped.kind == FullEvidenceKind::sense);
    CHECK(matches_frozen_140_fixture(popped.sense));
    SenseCapture invalidSense = popped.sense;
    invalidSense.writer_capacity_bytes = 1U;
    CHECK(senseRing.try_commit(invalidSense) == FullEvidenceCommitResult::rejected);
    invalidSense = popped.sense;
    invalidSense.body_hash++;
    CHECK(senseRing.try_commit(invalidSense) == FullEvidenceCommitResult::rejected);
    ScalarHashObservation scalar{};
    CHECK(telemetry(popped, scalar));
    CHECK(scalar.committed_record_sequence == popped.committed_sequence);
    CHECK(scalar.kind == ObservationKind::sense);
}

void full_evidence_ring_and_scalar_queue_survive_mpsc_contention() {
    AuthorityFixture fixture(1U, 1U);
    AuthorityDecodeCapture decode{};
    CHECK(capture_authority_decode(fixture.input, decode) == AuthorityCaptureResult::complete);
    AuthorityEvidenceRecord base{};
    CHECK(correlate_scene_apply(decode, exact_apply(decode), base)
          == ApplyCorrelationResult::complete);

    constexpr std::size_t kProducerCount = 4U;
    constexpr std::size_t kPerProducer = 24U;
    constexpr std::size_t kTotal = kProducerCount * kPerProducer;
    FullEvidenceRing ring;
    std::atomic<std::size_t> producersDone{};
    std::atomic<std::size_t> consumed{};
    std::atomic<bool> failed{};
    std::array<std::thread, kProducerCount> producers{};
    for (std::size_t producer = 0U; producer < kProducerCount; ++producer) {
        producers[producer] = std::thread([&, producer]() {
            for (std::size_t index = 0U; index < kPerProducer; ++index) {
                AuthorityEvidenceRecord candidate = base;
                candidate.decode.context.correlation_token =
                    1000U + producer * kPerProducer + index;
                for (;;) {
                    const FullEvidenceCommitResult result = ring.try_commit(candidate);
                    if (result == FullEvidenceCommitResult::committed) {
                        break;
                    }
                    if (result == FullEvidenceCommitResult::rejected
                        || result == FullEvidenceCommitResult::sequence_exhausted) {
                        failed.store(true, std::memory_order_release);
                        break;
                    }
                    std::this_thread::yield();
                }
            }
            producersDone.fetch_add(1U, std::memory_order_release);
        });
    }
    std::thread consumer([&]() {
        std::uint64_t lastSequence{};
        while (consumed.load(std::memory_order_acquire) < kTotal) {
            FullEvidenceRecord record{};
            if (ring.try_pop(record) == FullEvidenceReadResult::success) {
                if (record.committed_sequence <= lastSequence
                    || record.kind != FullEvidenceKind::authority
                    || validate_frozen_authority_evidence(record.authority)
                           != FrozenAuthorityValidation::valid) {
                    failed.store(true, std::memory_order_release);
                }
                lastSequence = record.committed_sequence;
                consumed.fetch_add(1U, std::memory_order_release);
            } else {
                if (producersDone.load(std::memory_order_acquire) == kProducerCount
                    && ring.counters().committed == consumed.load(std::memory_order_acquire)) {
                    break;
                }
                std::this_thread::yield();
            }
        }
    });
    for (std::thread& producer : producers) {
        producer.join();
    }
    consumer.join();
    CHECK(!failed.load(std::memory_order_acquire));
    CHECK(consumed.load(std::memory_order_acquire) == kTotal);
    CHECK(ring.counters().committed == kTotal);

    FullEvidenceRing seedRing;
    CHECK(seedRing.try_commit(base) == FullEvidenceCommitResult::committed);
    FullEvidenceRecord seeded{};
    CHECK(seedRing.try_pop(seeded) == FullEvidenceReadResult::success);
    ScalarHashObservation scalarBase{};
    CHECK(telemetry(seeded, scalarBase));

    ObservationQueue scalarQueue;
    producersDone.store(0U, std::memory_order_release);
    consumed.store(0U, std::memory_order_release);
    failed.store(false, std::memory_order_release);
    for (std::size_t producer = 0U; producer < kProducerCount; ++producer) {
        producers[producer] = std::thread([&, producer]() {
            for (std::size_t index = 0U; index < kPerProducer; ++index) {
                ScalarHashObservation item = scalarBase;
                item.committed_record_sequence =
                    1U + producer * kPerProducer + index;
                item.context.correlation_token =
                    2000U + producer * kPerProducer + index;
                item.body_hash += producer * kPerProducer + index;
                for (;;) {
                    const QueuePushResult result = scalarQueue.try_push(item);
                    if (result == QueuePushResult::enqueued) {
                        break;
                    }
                    if (result == QueuePushResult::rejected
                        || result == QueuePushResult::sequence_exhausted
                        || result == QueuePushResult::duplicate) {
                        failed.store(true, std::memory_order_release);
                        break;
                    }
                    std::this_thread::yield();
                }
            }
            producersDone.fetch_add(1U, std::memory_order_release);
        });
    }
    consumer = std::thread([&]() {
        while (consumed.load(std::memory_order_acquire) < kTotal) {
            ScalarHashObservation item{};
            if (scalarQueue.try_pop(item) == QueuePopResult::success) {
                if (item.sequence == 0U || item.committed_record_sequence == 0U) {
                    failed.store(true, std::memory_order_release);
                }
                consumed.fetch_add(1U, std::memory_order_release);
            } else {
                std::this_thread::yield();
            }
        }
    });
    for (std::thread& producer : producers) {
        producer.join();
    }
    consumer.join();
    CHECK(!failed.load(std::memory_order_acquire));
    CHECK(consumed.load(std::memory_order_acquire) == kTotal);
    CHECK(scalarQueue.counters().accepted == kTotal);
}

void seh_live_copy_boundary_handles_source_reuse_and_guard_faults() {
    AuthorityFixture fixture(2U, 2U);
    LiveAuthorityDecodeFanoutRecord live{};
    live.metadata = fixture.input;
    live.metadata.destination_address =
        reinterpret_cast<std::uintptr_t>(fixture.decoded.data());
    live.metadata.sensor.expected_received_authority_destination =
        live.metadata.destination_address;
    live.live_reader_buffer = fixture.backing.data();
    live.live_reader_readable_bytes = fixture.backing.size();
    live.live_decoded_source = fixture.decoded.data();
    live.live_decoded_readable_bytes = fixture.decoded.size();
    AuthorityDecodeCapture captured{};
    CHECK(capture_live_authority_decode(live, captured) == AuthorityCaptureResult::complete);
    const AuthorityDecodeCapture validCaptured = captured;
    const AuthorityRaw expectedRaw = captured.raw;
    const AuthorityDecoded expectedDecoded = captured.decoded;
    fixture.backing.fill(std::byte{0xFFU});
    fixture.decoded.fill(std::byte{0xFFU});
    CHECK(captured.raw == expectedRaw);
    CHECK(captured.decoded == expectedDecoded);
    fixture.decoded = expectedDecoded;

    SceneApplyFanoutRecord applyMetadata = exact_apply(validCaptured);
    std::array<std::byte, 0x300U> liveComponent{};
    StateKey16 liveStateKey = applyMetadata.state_key;
    write_value(liveStateKey.data() + 0x08U,
                static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(fixture.decoded.data())));
    applyMetadata.component.component_address =
        reinterpret_cast<std::uintptr_t>(liveComponent.data());
    applyMetadata.decoded_source_address =
        reinterpret_cast<std::uintptr_t>(fixture.decoded.data());
    LiveSceneApplyEntry liveApply{};
    liveApply.metadata = applyMetadata;
    liveApply.live_state_key = liveStateKey.data();
    liveApply.live_state_key_readable_bytes = liveStateKey.size();
    liveApply.live_decoded_source = fixture.decoded.data();
    liveApply.live_decoded_readable_bytes = fixture.decoded.size();
    liveApply.live_component = liveComponent.data();
    liveApply.live_component_readable_bytes = liveComponent.size();
    PendingLiveSceneApply pending{};
    CHECK(prepare_live_scene_apply(validCaptured, liveApply, pending)
          == ApplyCorrelationResult::complete);
    fixture.decoded.fill(std::byte{0xEEU});
    write_value(liveComponent.data() + kComponentDerivedScalarOffset, 0xA1B2C3D4U);
    liveComponent[kComponentDerivedActiveOffset] = std::byte{1U};
    AuthorityEvidenceRecord liveEvidence{};
    CHECK(finish_live_scene_apply(validCaptured,
                                  pending,
                                  liveComponent.data(),
                                  liveComponent.size(),
                                  {1U, 1U, 1U},
                                  liveEvidence) == ApplyCorrelationResult::complete);
    CHECK(validate_frozen_authority_evidence(liveEvidence)
          == FrozenAuthorityValidation::valid);

    live.live_reader_readable_bytes = 1U;
    CHECK(capture_live_authority_decode(live, captured)
          == AuthorityCaptureResult::source_partial);
    live.live_reader_readable_bytes = fixture.backing.size();

    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    auto* pages = static_cast<std::byte*>(
        VirtualAlloc(nullptr,
                     2U * info.dwPageSize,
                     MEM_COMMIT | MEM_RESERVE,
                     PAGE_READWRITE));
    CHECK(pages != nullptr);
    if (pages != nullptr) {
        DWORD oldProtection{};
        CHECK(VirtualProtect(pages + info.dwPageSize,
                             info.dwPageSize,
                             PAGE_NOACCESS,
                             &oldProtection) != FALSE);
        live.live_reader_buffer = pages + info.dwPageSize;
        live.metadata.reader.buffer_address =
            reinterpret_cast<std::uintptr_t>(pages + info.dwPageSize);
        live.live_reader_readable_bytes = fixture.backing.size();
        live.live_decoded_source = expectedDecoded.data();
        live.live_decoded_readable_bytes = expectedDecoded.size();
        CHECK(capture_live_authority_decode(live, captured)
              == AuthorityCaptureResult::source_fault);
        live.live_reader_buffer = expectedRaw.data();
        live.metadata.reader.buffer_address =
            reinterpret_cast<std::uintptr_t>(expectedRaw.data());
        live.live_reader_readable_bytes = expectedRaw.size();
        live.live_decoded_source = pages + info.dwPageSize;
        live.metadata.destination_address =
            reinterpret_cast<std::uintptr_t>(pages + info.dwPageSize);
        live.metadata.sensor.expected_received_authority_destination =
            live.metadata.destination_address;
        live.live_decoded_readable_bytes = kAuthorityDecodedBytes;
        CHECK(capture_live_authority_decode(live, captured)
              == AuthorityCaptureResult::source_fault);

        liveApply.live_state_key = pages + info.dwPageSize;
        liveApply.live_state_key_readable_bytes = kStateKeyBytes;
        liveApply.live_decoded_source = expectedDecoded.data();
        liveApply.metadata.decoded_source_address =
            reinterpret_cast<std::uintptr_t>(expectedDecoded.data());
        liveApply.live_component = liveComponent.data();
        liveApply.metadata.component.component_address =
            reinterpret_cast<std::uintptr_t>(liveComponent.data());
        CHECK(prepare_live_scene_apply(validCaptured, liveApply, pending)
              == ApplyCorrelationResult::source_fault);
        liveApply.live_state_key = liveStateKey.data();
        liveApply.live_component = pages + info.dwPageSize;
        liveApply.metadata.component.component_address =
            reinterpret_cast<std::uintptr_t>(pages + info.dwPageSize);
        CHECK(prepare_live_scene_apply(validCaptured, liveApply, pending)
              == ApplyCorrelationResult::component_fault);

        SenseFixture senseFixture(kFrozenSenseFixture140, 140U, 108U, 9U);
        LiveSenseFanoutRecord liveSense{};
        liveSense.metadata = senseFixture.input;
        liveSense.live_writer_buffer = senseFixture.backing.data();
        liveSense.live_writer_readable_bytes = senseFixture.backing.size();
        SenseCapture sense{};
        CHECK(capture_live_scene_sense(liveSense, sense) == SenseCaptureResult::complete);
        const SenseRaw expectedSense = sense.body;
        senseFixture.backing.fill(std::byte{0xFFU});
        CHECK(sense.body == expectedSense);
        liveSense.live_writer_buffer = pages + info.dwPageSize;
        liveSense.metadata.writer_at_outer_return.buffer_address =
            reinterpret_cast<std::uintptr_t>(pages + info.dwPageSize);
        liveSense.live_writer_readable_bytes = senseFixture.backing.size();
        CHECK(capture_live_scene_sense(liveSense, sense)
              == SenseCaptureResult::source_fault);
        CHECK(VirtualFree(pages, 0U, MEM_RELEASE) != FALSE);
    }
}

struct OwnerCallbackState final {
    std::atomic<std::uint32_t> before{};
    std::atomic<std::uint32_t> original{};
    std::atomic<std::uint32_t> after{};
    std::atomic<bool> block_original{};
    std::atomic<bool> original_entered{};
    std::atomic<bool> release_original{};
};

void owner_before(void* context) noexcept {
    static_cast<OwnerCallbackState*>(context)->before.fetch_add(1U,
                                                               std::memory_order_release);
}

void owner_original(void* context) noexcept {
    auto& state = *static_cast<OwnerCallbackState*>(context);
    state.original.fetch_add(1U, std::memory_order_release);
    state.original_entered.store(true, std::memory_order_release);
    while (state.block_original.load(std::memory_order_acquire)
           && !state.release_original.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
}

void owner_after(void* context) noexcept {
    static_cast<OwnerCallbackState*>(context)->after.fetch_add(1U,
                                                              std::memory_order_release);
}

void sole_owner_tls_original_once_and_detach_retention_contracts() {
    SoleOwnerClaim claim;
    CHECK(claim.try_claim(11U));
    CHECK(!claim.try_claim(12U));
    CHECK(claim.owner_id() == 11U);
    CHECK(!claim.release(12U));
    CHECK(claim.release(11U));

    OwnerTlsStack tls;
    const OwnerCallProvenance outer =
        owner_provenance(kReceiveOuterRva, 0x123456U, 100U, 0U, 77U, 0U, 5U);
    const OwnerCallProvenance nested = owner_provenance(kAuthorityDecodeRva,
                                                       kReceiveOuterRva + 0x20U,
                                                       101U,
                                                       100U,
                                                       77U,
                                                       1U,
                                                       6U);
    CHECK(tls.push(outer) == TlsStackResult::success);
    CHECK(tls.top(77U) != nullptr && tls.top(77U)->call_id == 100U);
    CHECK(tls.top(78U) == nullptr);
    CHECK(tls.push(nested) == TlsStackResult::success);
    OwnerCallProvenance wrongParent = nested;
    wrongParent.call_id++;
    wrongParent.parent_call_id = 999U;
    wrongParent.tls_depth = 2U;
    CHECK(tls.push(wrongParent) == TlsStackResult::wrong_parent);
    CHECK(tls.pop(101U, 78U) == TlsStackResult::wrong_thread);
    CHECK(tls.pop(100U, 77U) == TlsStackResult::wrong_frame);
    CHECK(tls.pop(101U, 77U) == TlsStackResult::success);
    CHECK(tls.pop(100U, 77U) == TlsStackResult::success);
    CHECK(tls.depth() == 0U);

    FanoutOwnerParticipant participant;
    CHECK(participant.configure_attached(0x1111U, 0x2222U, 1U, 1U));
    OwnerCallbackState callbacks{};
    OriginalCallAudit audit{};
    invoke_owner_original_once(participant,
                               &callbacks,
                               owner_original,
                               owner_before,
                               owner_after,
                               audit);
    CHECK(original_called_exactly_once(audit));
    CHECK(callbacks.before.load(std::memory_order_acquire) == 1U);
    CHECK(callbacks.original.load(std::memory_order_acquire) == 1U);
    CHECK(callbacks.after.load(std::memory_order_acquire) == 1U);

    participant.begin_quiesce();
    invoke_owner_original_once(participant,
                               &callbacks,
                               owner_original,
                               owner_before,
                               owner_after,
                               audit);
    CHECK(original_called_exactly_once(audit));
    CHECK(callbacks.before.load(std::memory_order_acquire) == 1U);
    CHECK(callbacks.original.load(std::memory_order_acquire) == 2U);
    CHECK(callbacks.after.load(std::memory_order_acquire) == 1U);
    CHECK(participant.try_finish_detach(false) == OwnerDetachResult::failed_retained);
    OwnerRetainedState retained = participant.retained_state();
    CHECK(retained.hook_handle == 0x1111U);
    CHECK(retained.trampoline == 0x2222U);
    CHECK(retained.owner_generation == 1U);
    CHECK(retained.evidence_generation == 1U);
    CHECK(retained.attached);
    CHECK(participant.try_finish_detach(true) == OwnerDetachResult::detached);
    CHECK(!participant.configure_attached(0x3333U, 0x4444U, 1U, 2U));
    CHECK(participant.configure_attached(0x3333U, 0x4444U, 2U, 2U));

    OwnerCallbackState concurrentCallbacks{};
    concurrentCallbacks.block_original.store(true, std::memory_order_release);
    OriginalCallAudit concurrentAudit{};
    std::thread callThread([&]() {
        invoke_owner_original_once(participant,
                                   &concurrentCallbacks,
                                   owner_original,
                                   owner_before,
                                   owner_after,
                                   concurrentAudit);
    });
    while (!concurrentCallbacks.original_entered.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    participant.begin_quiesce();
    CHECK(participant.try_finish_detach(true) == OwnerDetachResult::deferred_inflight);
    retained = participant.retained_state();
    CHECK(retained.in_flight == 1U);
    CHECK(retained.hook_handle == 0x3333U && retained.trampoline == 0x4444U);
    concurrentCallbacks.release_original.store(true, std::memory_order_release);
    callThread.join();
    CHECK(original_called_exactly_once(concurrentAudit));
    CHECK(participant.try_finish_detach(true) == OwnerDetachResult::detached);
    retained = participant.retained_state();
    CHECK(!retained.attached);
    CHECK(retained.hook_handle == 0U && retained.trampoline == 0U);
}

void scalar_hash_queue_is_fixed_nonblocking_and_exact() {
    AuthorityFixture fixture(1U, 1U);
    AuthorityDecodeCapture decode{};
    CHECK(capture_authority_decode(fixture.input, decode) == AuthorityCaptureResult::complete);
    AuthorityEvidenceRecord evidence{};
    CHECK(correlate_scene_apply(decode, exact_apply(decode), evidence)
          == ApplyCorrelationResult::complete);
    FullEvidenceRing evidenceRing;
    CHECK(evidenceRing.try_commit(evidence) == FullEvidenceCommitResult::committed);
    FullEvidenceRecord committed{};
    CHECK(evidenceRing.try_pop(committed) == FullEvidenceReadResult::success);
    ScalarHashObservation base{};
    CHECK(telemetry(committed, base));
    CHECK(base.body_hash == decode.raw_hash);
    CHECK(base.decoded_hash == decode.decoded_hash);
    CHECK(base.component_generation == evidence.component.component_generation);

    ObservationQueue queue;
    CHECK(queue.try_push(base) == QueuePushResult::enqueued);
    CHECK(queue.try_push(base) == QueuePushResult::duplicate);
    ScalarHashObservation output{};
    CHECK(queue.try_pop(output) == QueuePopResult::success);
    CHECK(output.sequence == 1U);
    CHECK(output.body_hash == base.body_hash);

    ScalarHashObservation stale = base;
    stale.context.activation.activation_generation = 0U;
    CHECK(queue.try_push(stale) == QueuePushResult::rejected);
    stale = base;
    stale.sensor_record_generation = 0U;
    CHECK(queue.try_push(stale) == QueuePushResult::rejected);
    stale = base;
    stale.status = ObservationStatus::rejected;
    CHECK(queue.try_push(stale) == QueuePushResult::rejected);
    stale = base;
    stale.component_generation = 0U;
    CHECK(queue.try_push(stale) == QueuePushResult::rejected);
    stale = base;
    stale.actual_schema_argument = kSceneSenseSchema;
    CHECK(queue.try_push(stale) == QueuePushResult::rejected);
    stale = base;
    stale.capture_format_version++;
    CHECK(queue.try_push(stale) == QueuePushResult::rejected);
    ScalarHashObservation uncommitted = telemetry(evidence);
    CHECK(uncommitted.committed_record_sequence == 0U);
    CHECK(queue.try_push(uncommitted) == QueuePushResult::rejected);

    ObservationQueue full;
    for (std::size_t index = 0U; index < kObservationQueueCapacity; ++index) {
        ScalarHashObservation distinct = base;
        distinct.committed_record_sequence += index + 1U;
        distinct.context.correlation_token += index + 1U;
        distinct.body_hash += index + 1U;
        CHECK(full.try_push(distinct) == QueuePushResult::enqueued);
    }
    ScalarHashObservation overflow = base;
    overflow.committed_record_sequence += 1000U;
    overflow.context.correlation_token += 1000U;
    overflow.body_hash += 1000U;
    CHECK(full.try_push(overflow) == QueuePushResult::full);

    ObservationQueue busy;
    CHECK(busy.testing_lock());
    CHECK(busy.try_push(base) == QueuePushResult::busy);
    busy.testing_unlock();

    ObservationQueue exhausted;
    exhausted.testing_set_next_sequence((std::numeric_limits<std::uint64_t>::max)());
    CHECK(exhausted.try_push(base) == QueuePushResult::sequence_exhausted);
    const QueueCounters counters = queue.counters();
    CHECK(counters.accepted == 1U);
    CHECK(counters.duplicates == 1U);
    CHECK(counters.rejected == 7U);
    CHECK(full.counters().dropped_full == 1U);
    CHECK(busy.counters().dropped_busy == 1U);
    CHECK(exhausted.counters().dropped_sequence_exhausted == 1U);
}

} // namespace

int main() {
    exact_gates_keep_scene_and_type31_separate();
    full_consumed_interval_minimum_maximum_and_alignment();
    authority_partial_fault_bounds_and_shape_fail_closed();
    full_component_snapshots_and_apply_semantics();
    oracle_is_private_exact_and_never_publishes();
    frozen_evidence_one_field_mutations_fail_closed();
    sense_cursor_delta_and_revisions_are_exact();
    legacy_prefix_and_retirement_are_observation_only();
    full_evidence_ring_owns_immutable_raw_records();
    full_evidence_ring_and_scalar_queue_survive_mpsc_contention();
    seh_live_copy_boundary_handles_source_reuse_and_guard_faults();
    sole_owner_tls_original_once_and_detach_retention_contracts();
    scalar_hash_queue_is_fixed_nonblocking_and_exact();

    if (g_failureCount != 0) {
        std::cerr << g_failureCount << " Scene capture check(s) failed\n";
        return 1;
    }
    std::cout << "all Scene authority/sense capture checks passed\n";
    return 0;
}
