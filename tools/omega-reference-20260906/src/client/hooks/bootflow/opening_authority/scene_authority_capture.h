#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace sunrise::client::hooks::bootflow::opening_authority::scene_capture {

/**
 * Pure Phase-2 Scene evidence support.
 *
 * This module consumes value/scalar views supplied by future sole-owner fanouts. It owns no native
 * detour, lifecycle, writer, selector, publication flag, network buffer, completion transition, or
 * retirement mailbox.
 */
inline constexpr bool kObservationOnly = true;
inline constexpr bool kOwnsNativeDetour = false;
inline constexpr bool kPerformsIo = false;
inline constexpr bool kProvidesAuthorityWriter = false;
inline constexpr bool kProvidesSenseWriter = false;
inline constexpr bool kProvidesPublication = false;
inline constexpr bool kProvidesSceneCompletion = false;
inline constexpr bool kMutatesLegacyHandoffState = false;

inline constexpr std::uint32_t kCaptureFormatVersion = 1U;
inline constexpr std::uint32_t kSceneRegistry = 0xD00142CFU;
inline constexpr std::uint8_t kSceneType = 43U;
inline constexpr std::uint16_t kSceneIndex = 1U;
inline constexpr std::uint32_t kSceneAuthoritySchema = 0x8080626BU;
inline constexpr std::uint32_t kSceneSenseSchema = 0x8080626AU;
inline constexpr std::uint32_t kSceneDefinition = 0x80F47B73U;
inline constexpr std::uint32_t kType31DefinitionBa6 = 0x80F47BA6U;
inline constexpr std::uint32_t kType31DefinitionBa9 = 0x80F47BA9U;
inline constexpr std::uint32_t kSchedulerSelectorNotAuthority = 0x80EC0F96U;

inline constexpr std::uintptr_t kReceiveOuterRva = 0x4D7470U;
inline constexpr std::size_t kReceiveOuterRecoveredBytes = 0x323U;
inline constexpr std::uintptr_t kAuthorityDecodeRva = 0x4C72E0U;
inline constexpr std::size_t kAuthorityDecodeRecoveredBytes = 0x1CDU;
inline constexpr std::uintptr_t kSceneApplyRva = 0xB41DD0U;
inline constexpr std::size_t kSceneApplyRecoveredBytes = 0x5EU;
inline constexpr std::uintptr_t kComponentStartRva = 0xB31910U;
inline constexpr std::size_t kComponentStartRecoveredBytes = 0x10U;
inline constexpr std::uintptr_t kSenseOuterRva = 0x4D8490U;
inline constexpr std::size_t kSenseOuterRecoveredBytes = 0x135U;
inline constexpr std::uintptr_t kSenseDeltaRva = 0x4C77A0U;
inline constexpr std::size_t kSenseDeltaRecoveredBytes = 0x10FU;

inline constexpr std::size_t kSha256Bytes = 32U;
using Sha256 = std::array<std::byte, kSha256Bytes>;
inline constexpr Sha256 kPinnedUnpackedPcSha256{
    std::byte{0x87}, std::byte{0x13}, std::byte{0xD1}, std::byte{0x5E},
    std::byte{0x3D}, std::byte{0x05}, std::byte{0xB2}, std::byte{0x6F},
    std::byte{0x9E}, std::byte{0x25}, std::byte{0x9E}, std::byte{0x02},
    std::byte{0xB0}, std::byte{0xF2}, std::byte{0x9B}, std::byte{0xC1},
    std::byte{0xE0}, std::byte{0x00}, std::byte{0xE4}, std::byte{0xB0},
    std::byte{0xC6}, std::byte{0x2C}, std::byte{0xA2}, std::byte{0xCC},
    std::byte{0x87}, std::byte{0xC3}, std::byte{0x85}, std::byte{0x97},
    std::byte{0x18}, std::byte{0x6C}, std::byte{0xC3}, std::byte{0xBD}};

inline constexpr std::size_t kAuthorityDecodedBytes = 0xD4U;
inline constexpr std::size_t kAuthorityMinimumBits = 74U;
inline constexpr std::size_t kAuthorityMaximumBits = 1538U;
inline constexpr std::size_t kAuthorityMaximumBytes = 193U;
inline constexpr std::size_t kAuthorityEntryMaximum = 8U;
inline constexpr std::size_t kAuthorityWordMaximum = 32U;
inline constexpr std::size_t kStateKeyBytes = 16U;
inline constexpr std::size_t kComponentCommittedPrefixOffset = 0x180U;
inline constexpr std::size_t kComponentCommittedPrefixBytes = 0x20U;
inline constexpr std::size_t kComponentDerivedScalarOffset = 0x25CU;
inline constexpr std::size_t kComponentDerivedActiveOffset = 0x260U;

/** Operational capture bound only; it makes no claim about the schema's semantic maximum. */
inline constexpr std::size_t kSenseCaptureStorageBits = 4096U;
inline constexpr std::size_t kSenseCaptureStorageBytes = kSenseCaptureStorageBits / 8U;
inline constexpr std::size_t kSenseRevisionBits = 32U;
inline constexpr std::size_t kFrozenSenseFixtureBits = 140U;
inline constexpr std::uint64_t kLegacyFirst64 = 0xC07607CB084F2555ULL;

using AuthorityRaw = std::array<std::byte, kAuthorityMaximumBytes>;
using AuthorityDecoded = std::array<std::byte, kAuthorityDecodedBytes>;
using StateKey16 = std::array<std::byte, kStateKeyBytes>;
using ComponentCommittedPrefix = std::array<std::byte, kComponentCommittedPrefixBytes>;
using SenseRaw = std::array<std::byte, kSenseCaptureStorageBytes>;

/** Normalized MSB-first form of the exact retained 140-bit body; low four pad bits are zero. */
inline constexpr std::array<std::byte, 18U> kFrozenSenseFixture140{
    std::byte{0xC0}, std::byte{0x76}, std::byte{0x07}, std::byte{0xCB},
    std::byte{0x08}, std::byte{0x4F}, std::byte{0x25}, std::byte{0x55},
    std::byte{0x4A}, std::byte{0x0A}, std::byte{0xB3}, std::byte{0x19},
    std::byte{0x0D}, std::byte{0x60}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0xA0}};

struct BuildEvidence final {
    Sha256 unpacked_pc_sha256{};
    bool image_identity_valid{};
    bool receive_outer_prefix_valid{};
    bool authority_decode_prefix_valid{};
    bool scene_apply_prefix_valid{};
    bool component_start_prefix_valid{};
    bool sense_outer_prefix_valid{};
    bool sense_delta_prefix_valid{};
    friend constexpr bool operator==(const BuildEvidence&, const BuildEvidence&) noexcept = default;
};

[[nodiscard]] bool valid_authority_build(const BuildEvidence& build) noexcept;
[[nodiscard]] bool valid_sense_build(const BuildEvidence& build) noexcept;

struct NativeActivationKey final {
    std::uint64_t module_generation{};
    std::uint64_t activation_generation{};
    friend constexpr bool operator==(NativeActivationKey, NativeActivationKey) noexcept = default;
};

struct ActivityInstanceKey final {
    std::uint64_t session_id{};
    std::uint64_t activity_id{};
    std::uint64_t activity_generation{};
    friend constexpr bool operator==(ActivityInstanceKey, ActivityInstanceKey) noexcept = default;
};

struct SessionLineage final {
    std::uint64_t session_id{};
    std::uint64_t created_revision{};
    std::uint64_t record_revision{};
    friend constexpr bool operator==(SessionLineage, SessionLineage) noexcept = default;
};

struct ExactCaptureContext final {
    std::uint64_t capture_epoch{};
    NativeActivationKey activation{};
    ActivityInstanceKey activity{};
    SessionLineage session{};
    std::uint64_t run_token{};
    std::uint64_t correlation_token{};
    std::uint64_t authority_generation{};
    friend constexpr bool operator==(const ExactCaptureContext&,
                                     const ExactCaptureContext&) noexcept = default;
};

[[nodiscard]] bool valid_exact_context(const ExactCaptureContext& context) noexcept;

struct SceneIdentity final {
    std::uint32_t registry{};
    std::uint8_t type{};
    std::uint16_t index{};
    std::uint32_t authority_schema{};
    std::uint32_t sense_schema{};
    friend constexpr bool operator==(SceneIdentity, SceneIdentity) noexcept = default;
};

inline constexpr SceneIdentity kExactSceneIdentity{kSceneRegistry,
                                                    kSceneType,
                                                    kSceneIndex,
                                                    kSceneAuthoritySchema,
                                                    kSceneSenseSchema};

[[nodiscard]] constexpr bool exact_scene_identity(const SceneIdentity& identity) noexcept {
    return identity == kExactSceneIdentity;
}

[[nodiscard]] constexpr bool is_type31_definition(std::uint32_t definition) noexcept {
    return definition == kType31DefinitionBa6 || definition == kType31DefinitionBa9;
}

enum class DefinitionRole : std::uint8_t {
    unknown,
    scene_type43,
    type31,
};

[[nodiscard]] constexpr DefinitionRole definition_role(std::uint32_t definition) noexcept {
    return definition == kSceneDefinition
               ? DefinitionRole::scene_type43
               : (is_type31_definition(definition) ? DefinitionRole::type31
                                                     : DefinitionRole::unknown);
}

struct SensorRecordLineage final {
    std::uintptr_t record_address{};
    std::uint64_t record_generation{};
    SceneIdentity identity{};
    std::uintptr_t expected_received_authority_destination{};
    friend constexpr bool operator==(const SensorRecordLineage&,
                                     const SensorRecordLineage&) noexcept = default;
};

[[nodiscard]] bool valid_authority_sensor(const SensorRecordLineage& sensor) noexcept;
[[nodiscard]] bool valid_sense_sensor(const SensorRecordLineage& sensor) noexcept;

struct SceneComponentLineage final {
    std::uintptr_t entry_address{};
    std::uintptr_t descriptor_address{};
    std::uintptr_t component_address{};
    std::uintptr_t handler_address{};
    std::uint64_t component_generation{};
    std::uint32_t definition{};
    std::uint32_t owner_object{};
    std::uint64_t encoded_component_size{};
    NativeActivationKey activation{};
    friend constexpr bool operator==(const SceneComponentLineage&,
                                     const SceneComponentLineage&) noexcept = default;
};

[[nodiscard]] bool valid_scene_component(const SceneComponentLineage& component,
                                         const ExactCaptureContext& context) noexcept;

enum class CopyOutcome : std::uint8_t {
    complete,
    partial,
    fault,
};

enum class CopyTiming : std::uint8_t {
    unknown,
    before_original,
    after_original_before_owner_exit,
};

/** Immutable scalar provenance supplied by one already-established native owner. */
struct OwnerCallProvenance final {
    std::uint64_t owner_id{};
    std::uint64_t generation_at_entry{};
    std::uint64_t generation_at_exit{};
    std::uint64_t call_id{};
    std::uint64_t parent_call_id{};
    std::uint32_t producer_thread_id{};
    std::uint32_t tls_depth{};
    std::uintptr_t owner_entry_rva{};
    std::uintptr_t native_caller_rva{};
    friend constexpr bool operator==(const OwnerCallProvenance&,
                                     const OwnerCallProvenance&) noexcept = default;
};

[[nodiscard]] bool valid_owner_provenance(const OwnerCallProvenance& provenance,
                                          std::uintptr_t expectedEntryRva,
                                          std::uintptr_t callerRangeStart,
                                          std::size_t callerRangeBytes) noexcept;

struct OriginalCallAudit final {
    std::uint32_t owner_entries{};
    std::uint32_t original_calls{};
    std::uint32_t owner_exits{};
    friend constexpr bool operator==(OriginalCallAudit, OriginalCallAudit) noexcept = default;
};

[[nodiscard]] constexpr bool original_called_exactly_once(OriginalCallAudit audit) noexcept {
    return audit.owner_entries == 1U && audit.original_calls == 1U && audit.owner_exits == 1U;
}

enum class IntervalResult : std::uint8_t {
    success,
    output_too_small,
    decreasing_cursor,
    capacity_overflow,
    backing_too_small,
};

/** Copies only [startBit,endBit), MSB-first, and clears every unused output bit. */
[[nodiscard]] IntervalResult normalize_msb_interval(std::span<const std::byte> backing,
                                                    std::size_t capacityBytes,
                                                    std::uint32_t startBit,
                                                    std::uint32_t endBit,
                                                    std::span<std::byte> output) noexcept;

[[nodiscard]] bool significant_bits_equal(std::span<const std::byte> left,
                                          std::span<const std::byte> right,
                                          std::size_t significantBits) noexcept;
[[nodiscard]] bool unused_low_bits_zero(std::span<const std::byte> bytes,
                                        std::size_t significantBits) noexcept;
[[nodiscard]] std::uint64_t bounded_hash(std::span<const std::byte> bytes,
                                         std::size_t significantBits) noexcept;

struct AuthorityShape final {
    std::uint32_t entry_count{};
    std::uint32_t word_count{};
    std::size_t significant_bits{};
};

enum class ShapeResult : std::uint8_t {
    valid,
    entry_count_overflow,
    word_count_overflow,
    arithmetic_overflow,
};

[[nodiscard]] ShapeResult authority_shape(const AuthorityDecoded& decoded,
                                          AuthorityShape& output) noexcept;

struct AuthorityReaderView final {
    std::span<const std::byte> backing{};
    std::uintptr_t buffer_address{};
    std::size_t capacity_bytes{};
    std::uint32_t start_bit{};
    std::uint32_t end_bit{};
    std::uint8_t error_before{};
    std::uint8_t error_after{};
    CopyOutcome backing_copy{CopyOutcome::fault};
    /** The sole owner asserts this interval starts after the outer object header. */
    bool nested_reflected_body_interval{};
};

/** Neutral, non-owning input passed synchronously by the future sole-owner decode fanout. */
struct AuthorityDecodeFanoutRecord final {
    BuildEvidence build{};
    ExactCaptureContext context{};
    SensorRecordLineage sensor{};
    OwnerCallProvenance outer_owner{};
    OwnerCallProvenance decode_owner{};
    std::uint32_t actual_schema_argument{};
    std::uint32_t actual_mode_argument{};
    bool mode_argument_captured{};
    std::uintptr_t destination_address{};
    AuthorityReaderView reader{};
    bool decoder_returned_true{};
    std::int32_t inout_bytes_before{};
    std::int32_t inout_bytes_after{};
    AuthorityDecoded decoded{};
    CopyOutcome decoded_copy{CopyOutcome::fault};
    CopyTiming source_copy_timing{CopyTiming::unknown};
};

struct AuthorityDecodeCapture final {
    std::uint32_t capture_format_version{};
    BuildEvidence build{};
    ExactCaptureContext context{};
    SensorRecordLineage sensor{};
    OwnerCallProvenance outer_owner{};
    OwnerCallProvenance decode_owner{};
    std::uint32_t actual_schema_argument{};
    std::uint32_t actual_mode_argument{};
    bool mode_argument_captured{};
    std::uintptr_t destination_address{};
    std::uintptr_t reader_buffer_address{};
    std::size_t reader_capacity_bytes{};
    std::uint32_t start_bit{};
    std::uint32_t end_bit{};
    std::uint8_t error_before{};
    std::uint8_t error_after{};
    bool decoder_returned_true{};
    std::int32_t inout_bytes_before{};
    std::int32_t inout_bytes_after{};
    AuthorityRaw raw{};
    std::size_t body_bits{};
    std::uint64_t raw_hash{};
    AuthorityDecoded decoded{};
    std::uint64_t decoded_hash{};
    AuthorityShape shape{};
    CopyOutcome reader_copy{CopyOutcome::fault};
    CopyOutcome decoded_copy{CopyOutcome::fault};
    CopyTiming source_copy_timing{CopyTiming::unknown};
};

enum class AuthorityCaptureResult : std::uint8_t {
    complete,
    wrong_build,
    incomplete_context,
    wrong_sensor,
    stale_sensor,
    owner_pair_mismatch,
    wrong_destination,
    wrong_size,
    native_failure,
    source_partial,
    source_fault,
    reader_error,
    reader_bounds,
    body_too_small,
    body_too_large,
    illegal_shape,
    length_mismatch,
};

[[nodiscard]] AuthorityCaptureResult capture_authority_decode(
    const AuthorityDecodeFanoutRecord& input,
    AuthorityDecodeCapture& output) noexcept;

struct LiveAuthorityDecodeFanoutRecord final {
    /** Scalar metadata; reader.backing and decoded bytes are ignored by the live-copy entry. */
    AuthorityDecodeFanoutRecord metadata{};
    const std::byte* live_reader_buffer{};
    std::size_t live_reader_readable_bytes{};
    const std::byte* live_decoded_source{};
    std::size_t live_decoded_readable_bytes{};
};

/** Sole-owner boundary: all live reads occur inside one MSVC SEH-owned copy operation. */
[[nodiscard]] AuthorityCaptureResult capture_live_authority_decode(
    const LiveAuthorityDecodeFanoutRecord& input,
    AuthorityDecodeCapture& output) noexcept;

struct SceneApplyFanoutRecord final {
    BuildEvidence build{};
    ExactCaptureContext context{};
    SceneComponentLineage component{};
    OwnerCallProvenance apply_owner{};
    OwnerCallProvenance component_owner{};
    StateKey16 state_key{};
    std::uintptr_t decoded_source_address{};
    AuthorityDecoded decoded_source{};
    CopyOutcome decoded_source_copy{CopyOutcome::fault};
    CopyTiming decoded_source_copy_timing{CopyTiming::unknown};
    ComponentCommittedPrefix component_180_19f_before{};
    ComponentCommittedPrefix component_180_19f_after{};
    CopyOutcome component_before_copy{CopyOutcome::fault};
    CopyOutcome component_after_copy{CopyOutcome::fault};
    CopyTiming component_before_copy_timing{CopyTiming::unknown};
    CopyTiming component_after_copy_timing{CopyTiming::unknown};
    std::uint32_t derived_scalar_before{};
    std::uint32_t derived_scalar_after{};
    std::uint8_t derived_active_before{};
    std::uint8_t derived_active_after{};
    OriginalCallAudit original_call_audit{};
};

struct AuthorityEvidenceRecord final {
    std::uint32_t capture_format_version{};
    AuthorityDecodeCapture decode{};
    SceneComponentLineage component{};
    OwnerCallProvenance apply_owner{};
    OwnerCallProvenance component_owner{};
    StateKey16 state_key{};
    std::uintptr_t apply_source_address{};
    AuthorityDecoded apply_source{};
    std::uint64_t apply_source_hash{};
    CopyOutcome apply_source_copy{CopyOutcome::fault};
    CopyTiming apply_source_copy_timing{CopyTiming::unknown};
    ComponentCommittedPrefix component_180_19f_before{};
    ComponentCommittedPrefix component_180_19f_after{};
    CopyOutcome component_before_copy{CopyOutcome::fault};
    CopyOutcome component_after_copy{CopyOutcome::fault};
    CopyTiming component_before_copy_timing{CopyTiming::unknown};
    CopyTiming component_after_copy_timing{CopyTiming::unknown};
    std::uint32_t derived_scalar_before{};
    std::uint32_t derived_scalar_after{};
    std::uint8_t derived_active_before{};
    std::uint8_t derived_active_after{};
    OriginalCallAudit original_call_audit{};
};

enum class ApplyCorrelationResult : std::uint8_t {
    complete,
    wrong_build,
    incomplete_context,
    stale_component,
    wrong_definition,
    source_mismatch,
    wrong_state_key,
    source_partial,
    source_fault,
    component_partial,
    component_fault,
    original_count_mismatch,
    apply_semantic_mismatch,
};

[[nodiscard]] ApplyCorrelationResult correlate_scene_apply(
    const AuthorityDecodeCapture& decode,
    const SceneApplyFanoutRecord& apply,
    AuthorityEvidenceRecord& output) noexcept;

struct LiveSceneApplyEntry final {
    /** Scalar metadata; all copied arrays/derived fields are ignored by prepare. */
    SceneApplyFanoutRecord metadata{};
    const std::byte* live_state_key{};
    std::size_t live_state_key_readable_bytes{};
    const std::byte* live_decoded_source{};
    std::size_t live_decoded_readable_bytes{};
    const std::byte* live_component{};
    std::size_t live_component_readable_bytes{};
};

struct PendingLiveSceneApply final {
    SceneApplyFanoutRecord owned{};
    bool ready{};
};

[[nodiscard]] ApplyCorrelationResult prepare_live_scene_apply(
    const AuthorityDecodeCapture& decode,
    const LiveSceneApplyEntry& input,
    PendingLiveSceneApply& pending) noexcept;

[[nodiscard]] ApplyCorrelationResult finish_live_scene_apply(
    const AuthorityDecodeCapture& decode,
    PendingLiveSceneApply& pending,
    const std::byte* liveComponentAfter,
    std::size_t liveComponentReadableBytes,
    OriginalCallAudit originalCallAudit,
    AuthorityEvidenceRecord& output) noexcept;

enum class FrozenAuthorityValidation : std::uint8_t {
    valid,
    wrong_format,
    wrong_build,
    incomplete_context,
    wrong_sensor,
    stale_sensor,
    owner_provenance_mismatch,
    wrong_native_schema,
    wrong_destination,
    reader_invalid,
    decoder_invalid,
    raw_hash_mismatch,
    decoded_hash_mismatch,
    shape_mismatch,
    padding_failure,
    wrong_state_key,
    source_mismatch,
    stale_component,
    source_copy_invalid,
    component_copy_invalid,
    original_count_mismatch,
    apply_semantic_mismatch,
};

/** Recomputes every cached field and revalidates every owned byte/scalar invariant. */
[[nodiscard]] FrozenAuthorityValidation validate_frozen_authority_decode(
    const AuthorityDecodeCapture& decode) noexcept;
[[nodiscard]] FrozenAuthorityValidation validate_frozen_authority_evidence(
    const AuthorityEvidenceRecord& record) noexcept;

struct PrivateEncodeObservation final {
    AuthorityRaw bytes{};
    std::size_t cursor_bits{};
    bool writer_status_good{};
    bool private_storage_only{};
};

struct NativeOracleObservation final {
    bool measurement_succeeded{};
    std::int32_t rounded_bytes{};
    bool measurement_private_storage_only{};
    PrivateEncodeObservation first{};
    PrivateEncodeObservation second{};
    /** Any such call is a hard failure; the module itself exposes no way to make one. */
    bool publication_or_packet_call_observed{};
};

enum class OracleResult : std::uint8_t {
    accepted_observation,
    incomplete_record,
    measurement_failure,
    measurement_not_private,
    rounded_size_mismatch,
    writer_failure,
    writer_not_private,
    cursor_mismatch,
    padding_failure,
    nondeterministic,
    significant_bit_mismatch,
    publication_attempted,
};

[[nodiscard]] OracleResult validate_native_oracle(
    const AuthorityEvidenceRecord& record,
    const NativeOracleObservation& observation) noexcept;

struct WriterBitView final {
    std::span<const std::byte> backing{};
    std::uintptr_t buffer_address{};
    std::size_t capacity_bytes{};
    std::size_t completed_bits{};
    std::uint64_t pending_word{};
    std::uint32_t pending_bits{};
    std::uint8_t error{};
    CopyOutcome backing_copy{CopyOutcome::fault};
};

[[nodiscard]] std::size_t writer_logical_cursor(const WriterBitView& writer) noexcept;
[[nodiscard]] IntervalResult normalize_writer_interval(const WriterBitView& writer,
                                                       std::size_t startBit,
                                                       std::size_t endBit,
                                                       std::span<std::byte> output) noexcept;

/** Neutral nested-pair input passed synchronously by the future +4D8490 sole-owner fanout. */
struct SenseFanoutRecord final {
    BuildEvidence build{};
    ExactCaptureContext context{};
    SensorRecordLineage sensor{};
    OwnerCallProvenance outer_owner{};
    OwnerCallProvenance delta_owner{};
    std::uint32_t actual_delta_schema_argument{};
    std::uintptr_t current_source_address{};
    std::uintptr_t prior_source_address{};
    std::size_t body_start{};
    std::size_t delta_end{};
    std::size_t body_end{};
    std::uint32_t accepted_revision{};
    WriterBitView writer_at_outer_return{};
};

struct SenseCapture final {
    std::uint32_t capture_format_version{};
    BuildEvidence build{};
    ExactCaptureContext context{};
    SensorRecordLineage sensor{};
    OwnerCallProvenance outer_owner{};
    OwnerCallProvenance delta_owner{};
    std::uint32_t actual_delta_schema_argument{};
    std::uintptr_t current_source_address{};
    std::uintptr_t prior_source_address{};
    std::uintptr_t writer_buffer_address{};
    std::size_t writer_capacity_bytes{};
    std::size_t writer_completed_bits{};
    std::uint32_t writer_pending_bits{};
    std::uint8_t writer_error{};
    std::size_t writer_logical_end{};
    std::size_t body_start{};
    std::size_t delta_end{};
    std::size_t body_end{};
    SenseRaw body{};
    std::size_t body_bits{};
    std::size_t delta_bits{};
    std::uint64_t body_hash{};
    std::uint64_t delta_hash{};
    bool delta_any{};
    std::uint32_t accepted_revision{};
    std::uint32_t next_revision{};
    CopyOutcome writer_copy{CopyOutcome::fault};
};

enum class SenseCaptureResult : std::uint8_t {
    complete,
    wrong_build,
    incomplete_context,
    wrong_sensor,
    stale_sensor,
    owner_pair_mismatch,
    wrong_thread,
    source_partial,
    source_fault,
    writer_error,
    cursor_mismatch,
    decreasing_cursor,
    storage_exhausted,
    missing_revision_trailer,
    revision_mismatch,
};

[[nodiscard]] SenseCaptureResult capture_scene_sense(const SenseFanoutRecord& input,
                                                     SenseCapture& output) noexcept;

struct LiveSenseFanoutRecord final {
    /** Scalar metadata; writer backing span is ignored by the live-copy entry. */
    SenseFanoutRecord metadata{};
    const std::byte* live_writer_buffer{};
    std::size_t live_writer_readable_bytes{};
};

[[nodiscard]] SenseCaptureResult capture_live_scene_sense(
    const LiveSenseFanoutRecord& input,
    SenseCapture& output) noexcept;
[[nodiscard]] bool matches_frozen_140_fixture(const SenseCapture& capture) noexcept;

struct SceneSenseServiceSplit final {
    std::size_t body_bits{};
    std::size_t delta_bits{};
    std::uint32_t next_revision{};
    bool retained_fixture_width{};
    bool exact_140_fixture{};
};

/** Source-only service seam; retained widths are fixtures, never schema maxima. */
[[nodiscard]] bool split_scene_sense_service_observation(
    const SceneIdentity& identity,
    std::span<const std::byte> normalizedBody,
    std::size_t bodyBits,
    SceneSenseServiceSplit& output) noexcept;

enum class LegacyStatefulConsumer : std::uint32_t {
    handoff_arm = 1U << 0U,
    keepalive_wakeup = 1U << 1U,
    roster_stage = 1U << 2U,
};

inline constexpr std::uint32_t kLegacyStatefulConsumerMask =
    static_cast<std::uint32_t>(LegacyStatefulConsumer::handoff_arm)
    | static_cast<std::uint32_t>(LegacyStatefulConsumer::keepalive_wakeup)
    | static_cast<std::uint32_t>(LegacyStatefulConsumer::roster_stage);

struct LegacyPrefixObservation final {
    std::size_t observed_bits{};
    std::uint64_t observed_first64{};
    bool width_is_140{};
    bool first64_matches{};
    bool full_fixture_matches{};
    std::uint32_t known_stateful_consumer_mask{kLegacyStatefulConsumerMask};
    bool authorizes_authority{};
    bool authorizes_completion{};
    bool mutates_state{};
};

[[nodiscard]] LegacyPrefixObservation observe_legacy_64_of_140(
    std::span<const std::byte> normalizedBody,
    std::size_t bodyBits) noexcept;

enum class RetirementObservation : std::uint8_t {
    flat_mailbox,
    generation_tagged_mailbox,
    local_wrapper_58b9a0,
};

[[nodiscard]] constexpr bool retirement_is_scene_completion(RetirementObservation) noexcept {
    return false;
}

enum class FullEvidenceKind : std::uint8_t {
    authority_decode,
    authority,
    sense,
};

struct FullEvidenceRecord final {
    std::uint32_t capture_format_version{};
    std::uint64_t committed_sequence{};
    FullEvidenceKind kind{FullEvidenceKind::authority};
    AuthorityDecodeCapture authority_decode{};
    AuthorityEvidenceRecord authority{};
    SenseCapture sense{};
};

enum class FullEvidenceCommitResult : std::uint8_t {
    committed,
    rejected,
    full,
    busy,
    sequence_exhausted,
};

enum class FullEvidenceReadResult : std::uint8_t {
    success,
    empty,
    busy,
};

struct FullEvidenceCounters final {
    std::uint64_t committed{};
    std::uint64_t rejected{};
    std::uint64_t dropped_full{};
    std::uint64_t dropped_busy{};
    std::uint64_t dropped_sequence_exhausted{};
    std::uint64_t source_partial{};
    std::uint64_t source_fault{};
};

inline constexpr std::size_t kFullEvidenceRingCapacity = 8U;

/**
 * Fixed immutable evidence transport. Every operation performs one lock acquisition attempt; a
 * stable value copy is committed before visibility and popped before scalar telemetry is derived.
 */
class FullEvidenceRing final {
  public:
    FullEvidenceRing() noexcept = default;
    FullEvidenceRing(const FullEvidenceRing&) = delete;
    FullEvidenceRing& operator=(const FullEvidenceRing&) = delete;

    [[nodiscard]] FullEvidenceCommitResult try_commit(
        const AuthorityDecodeCapture& authorityDecode) noexcept;
    [[nodiscard]] FullEvidenceCommitResult try_commit(
        const AuthorityEvidenceRecord& authority) noexcept;
    [[nodiscard]] FullEvidenceCommitResult try_commit(const SenseCapture& sense) noexcept;
    [[nodiscard]] FullEvidenceReadResult try_pop(FullEvidenceRecord& output) noexcept;
    void account_source_outcome(CopyOutcome outcome) noexcept;
    [[nodiscard]] FullEvidenceCounters counters() const noexcept;

    [[nodiscard]] bool testing_lock() noexcept;
    void testing_unlock() noexcept;
    void testing_set_next_sequence(std::uint64_t sequence) noexcept;

  private:
    [[nodiscard]] bool lock_once() noexcept;
    void unlock() noexcept;
    [[nodiscard]] FullEvidenceCommitResult commit(FullEvidenceRecord record) noexcept;

    std::array<FullEvidenceRecord, kFullEvidenceRingCapacity> records_{};
    std::atomic_flag lock_ = ATOMIC_FLAG_INIT;
    std::size_t head_{};
    std::size_t size_{};
    std::uint64_t next_sequence_{1U};
    std::atomic<std::uint64_t> committed_{};
    std::atomic<std::uint64_t> rejected_{};
    std::atomic<std::uint64_t> dropped_full_{};
    std::atomic<std::uint64_t> dropped_busy_{};
    std::atomic<std::uint64_t> dropped_sequence_exhausted_{};
    std::atomic<std::uint64_t> source_partial_{};
    std::atomic<std::uint64_t> source_fault_{};
};

inline constexpr std::size_t kOwnerTlsMaximumDepth = 8U;

enum class TlsStackResult : std::uint8_t {
    success,
    overflow,
    wrong_thread,
    wrong_parent,
    wrong_frame,
};

class OwnerTlsStack final {
  public:
    [[nodiscard]] TlsStackResult push(const OwnerCallProvenance& frame) noexcept;
    [[nodiscard]] TlsStackResult pop(std::uint64_t callId,
                                     std::uint32_t producerThreadId) noexcept;
    [[nodiscard]] const OwnerCallProvenance* top(std::uint32_t producerThreadId) const noexcept;
    [[nodiscard]] std::size_t depth() const noexcept { return depth_; }

  private:
    std::array<OwnerCallProvenance, kOwnerTlsMaximumDepth> frames_{};
    std::size_t depth_{};
    std::uint32_t thread_id_{};
};

class SoleOwnerClaim final {
  public:
    [[nodiscard]] bool try_claim(std::uint64_t ownerId) noexcept;
    [[nodiscard]] bool release(std::uint64_t ownerId) noexcept;
    [[nodiscard]] std::uint64_t owner_id() const noexcept;

  private:
    std::atomic<std::uint64_t> owner_id_{};
};

enum class OwnerDetachResult : std::uint8_t {
    detached,
    deferred_inflight,
    failed_retained,
    not_attached,
};

struct OwnerRetainedState final {
    std::uintptr_t hook_handle{};
    std::uintptr_t trampoline{};
    std::uint64_t owner_generation{};
    std::uint64_t evidence_generation{};
    std::uint32_t in_flight{};
    bool admission_open{};
    bool attached{};
};

class FanoutOwnerParticipant final {
  public:
    [[nodiscard]] bool configure_attached(std::uintptr_t hookHandle,
                                          std::uintptr_t trampoline,
                                          std::uint64_t ownerGeneration,
                                          std::uint64_t evidenceGeneration) noexcept;
    [[nodiscard]] bool try_enter() noexcept;
    void leave() noexcept;
    void begin_quiesce() noexcept;
    [[nodiscard]] OwnerDetachResult try_finish_detach(bool nativeDetachSucceeded) noexcept;
    [[nodiscard]] OwnerRetainedState retained_state() const noexcept;

  private:
    std::atomic<bool> admission_open_{};
    std::atomic<bool> attached_{};
    std::atomic<std::uint32_t> in_flight_{};
    std::atomic<std::uintptr_t> hook_handle_{};
    std::atomic<std::uintptr_t> trampoline_{};
    std::atomic<std::uint64_t> owner_generation_{};
    std::atomic<std::uint64_t> evidence_generation_{};
    std::atomic<std::uint64_t> last_owner_generation_{};
    std::atomic<std::uint64_t> last_evidence_generation_{};
};

using OwnerCallback = void (*)(void*) noexcept;

/** Source-only fake-owner seam: native original is forwarded once even when observation is shut. */
void invoke_owner_original_once(FanoutOwnerParticipant& participant,
                                void* context,
                                OwnerCallback original,
                                OwnerCallback beforeObservation,
                                OwnerCallback afterObservation,
                                OriginalCallAudit& audit) noexcept;

enum class ObservationKind : std::uint8_t {
    authority_decode,
    authority_apply,
    authority_oracle,
    sense,
    legacy_prefix,
    retirement,
};

enum class ObservationStatus : std::uint8_t {
    complete,
    partial,
    fault,
    rejected,
};

/** Fixed scalar/hash projection. Raw authority, decoded, component, and sense bytes never enter it. */
struct ScalarHashObservation final {
    std::uint32_t capture_format_version{};
    Sha256 build_fingerprint{};
    ExactCaptureContext context{};
    std::uint64_t sequence{};
    std::uint64_t committed_record_sequence{};
    std::uint64_t owner_generation{};
    std::uint64_t owner_generation_at_exit{};
    std::uint64_t apply_owner_generation{};
    std::uint64_t sensor_record_generation{};
    std::uint64_t component_generation{};
    std::uint64_t sensor_record_address_hash{};
    std::uint64_t component_address_hash{};
    std::uint64_t body_hash{};
    std::uint64_t decoded_hash{};
    std::size_t body_bits{};
    std::size_t delta_bits{};
    std::uint32_t actual_schema_argument{};
    std::uint32_t next_revision{};
    std::uint32_t producer_thread_id{};
    std::uintptr_t owner_entry_rva{};
    std::uintptr_t native_caller_rva{};
    std::uint32_t sensor_registry{};
    std::uint16_t sensor_index{};
    std::uint8_t sensor_type{};
    std::uint32_t component_definition{};
    ObservationKind kind{ObservationKind::authority_decode};
    ObservationStatus status{ObservationStatus::rejected};
    friend constexpr bool operator==(const ScalarHashObservation&,
                                     const ScalarHashObservation&) noexcept = default;
};

[[nodiscard]] ScalarHashObservation telemetry(const AuthorityDecodeCapture& capture) noexcept;
[[nodiscard]] ScalarHashObservation telemetry(const AuthorityEvidenceRecord& record) noexcept;
[[nodiscard]] ScalarHashObservation telemetry(const SenseCapture& capture) noexcept;
[[nodiscard]] bool telemetry(const FullEvidenceRecord& record,
                             ScalarHashObservation& output) noexcept;

enum class QueuePushResult : std::uint8_t {
    enqueued,
    duplicate,
    rejected,
    full,
    busy,
    sequence_exhausted,
};

enum class QueuePopResult : std::uint8_t {
    success,
    empty,
    busy,
};

struct QueueCounters final {
    std::uint64_t accepted{};
    std::uint64_t duplicates{};
    std::uint64_t rejected{};
    std::uint64_t dropped_full{};
    std::uint64_t dropped_busy{};
    std::uint64_t dropped_sequence_exhausted{};
};

inline constexpr std::size_t kObservationQueueCapacity = 32U;

class ObservationQueue final {
  public:
    ObservationQueue() noexcept = default;
    ObservationQueue(const ObservationQueue&) = delete;
    ObservationQueue& operator=(const ObservationQueue&) = delete;

    [[nodiscard]] QueuePushResult try_push(const ScalarHashObservation& observation) noexcept;
    [[nodiscard]] QueuePopResult try_pop(ScalarHashObservation& output) noexcept;
    [[nodiscard]] QueueCounters counters() const noexcept;

    /** Deterministic unit-only contention seam; production callers do not need these methods. */
    [[nodiscard]] bool testing_lock() noexcept;
    void testing_unlock() noexcept;
    void testing_set_next_sequence(std::uint64_t sequence) noexcept;

  private:
    [[nodiscard]] bool lock_once() noexcept;
    void unlock() noexcept;

    std::array<ScalarHashObservation, kObservationQueueCapacity> records_{};
    std::atomic_flag lock_ = ATOMIC_FLAG_INIT;
    std::size_t head_{};
    std::size_t size_{};
    std::uint64_t next_sequence_{1U};
    std::atomic<std::uint64_t> accepted_{};
    std::atomic<std::uint64_t> duplicates_{};
    std::atomic<std::uint64_t> rejected_{};
    std::atomic<std::uint64_t> dropped_full_{};
    std::atomic<std::uint64_t> dropped_busy_{};
    std::atomic<std::uint64_t> dropped_sequence_exhausted_{};
};

} // namespace sunrise::client::hooks::bootflow::opening_authority::scene_capture
