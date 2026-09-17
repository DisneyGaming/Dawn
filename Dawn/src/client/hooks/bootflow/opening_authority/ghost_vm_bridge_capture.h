#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <span>
#include <type_traits>

namespace dawn::client::hooks::bootflow::opening_authority::ghost_vm_bridge_capture {

/**
 * Source-only evidence model for the Phase-2 Ghost Type-60 / mission-VM / Type-53 route.
 *
 * The module installs no hook, writes no game memory, publishes no authority, performs no I/O,
 * and contains no guessed producer. Integration code may use the descriptors and value-copy
 * helpers to build an observation-only probe after separately proving the exact artifact.
 */
inline constexpr bool kOwnsNativeDetour = false;
inline constexpr bool kOwnsAuthorityWriter = false;
inline constexpr bool kPublishesType5 = false;
inline constexpr bool kMutatesVmState = false;
inline constexpr bool kMutatesMembership = false;
inline constexpr bool kPerformsIo = false;
inline constexpr bool kRetainsNativePointers = false;
inline constexpr bool kStoresRawAuthorityBodies = false;
inline constexpr bool kRetailHostPublisherRecovered = false;
inline constexpr std::uintptr_t kRetailHostPublisherRva = 0U;
inline constexpr bool kProducerInstructionRecovered = false;
inline constexpr bool kGhostLocalEventOrdinalRecovered = false;
inline constexpr std::uint32_t kFrozenOpeningActivityScriptCallbackCount = 0U;
inline constexpr bool kType53CompletionFeedbackRecovered = false;
inline constexpr bool kDialogueSenseExportRecovered = false;
inline constexpr std::uintptr_t kMissionResultConsumerRva = 0U;
inline constexpr bool kGenerationComparisonIsEqualityOnly = true;
inline constexpr bool kSameGenerationReplaysOnSameInstance = false;
inline constexpr bool kType53ApplyClearsProcessedMirror = false;
inline constexpr bool kGenerationStoredAfterSubmitBeforeActualStart = true;
inline constexpr bool kPresentationDropRetriesSameGeneration = false;
inline constexpr bool kNewInstanceProcessedMirrorInitializerRecovered = false;
inline constexpr bool kRetailLateJoinPolicyRecovered = false;

inline constexpr std::size_t kSha256Bytes = 32U;
using Sha256 = std::array<std::byte, kSha256Bytes>;
inline constexpr const char kEvidenceDigestAlgorithm[] = "SHA-256";

enum class ArtifactKind : std::uint8_t {
    pc_unpacked_reference,
    ps4_eboot_reference,
    mercury_mission_package,
    external_retail_host_build,
};

struct ArtifactIdentity final {
    std::uint64_t file_bytes{};
    Sha256 sha256{};
    friend constexpr bool operator==(ArtifactIdentity, ArtifactIdentity) noexcept = default;
};

struct ArtifactDescriptor final {
    ArtifactKind kind{};
    const wchar_t* path{};
    std::uint64_t file_bytes{};
    Sha256 sha256{};
    std::uintptr_t preferred_image_base{};
    std::size_t code_file_offset_bias{};
};

[[nodiscard]] const ArtifactDescriptor& artifact_descriptor(ArtifactKind kind) noexcept;
[[nodiscard]] bool matches_pinned_artifact(ArtifactKind kind,
                                           const ArtifactIdentity& identity) noexcept;
[[nodiscard]] Sha256 sha256(std::span<const std::byte> bytes) noexcept;

enum class ArtifactVerificationResult : std::uint8_t {
    verified,
    unsupported_artifact,
    exact_size_mismatch,
    sha256_mismatch,
};

enum class NativeSurface : std::uint8_t;
enum class EndpointValidation : std::uint8_t;

/** Opaque proof bound to the exact immutable span that was hashed. */
class VerifiedArtifact final {
public:
    VerifiedArtifact() noexcept = default;
    [[nodiscard]] bool valid() const noexcept {
        return valid_;
    }
    [[nodiscard]] ArtifactKind kind() const noexcept {
        return kind_;
    }
    [[nodiscard]] ArtifactIdentity identity() const noexcept {
        return identity_;
    }

private:
    friend ArtifactVerificationResult
    verify_pinned_artifact(ArtifactKind, std::span<const std::byte>, VerifiedArtifact&) noexcept;
    friend EndpointValidation validate_reference_endpoint(const VerifiedArtifact&,
                                                          std::span<const std::byte>,
                                                          NativeSurface) noexcept;

    ArtifactKind kind_{ArtifactKind::external_retail_host_build};
    ArtifactIdentity identity_{};
    const std::byte* verified_data_{};
    std::size_t verified_size_{};
    bool valid_{};
};

[[nodiscard]] ArtifactVerificationResult verify_pinned_artifact(
    ArtifactKind kind, std::span<const std::byte> immutableFile, VerifiedArtifact& output) noexcept;

inline constexpr std::uintptr_t kPcPreferredImageBase = 0x7FF68E9A0000ULL;
inline constexpr std::size_t kPs4CodeFileOffsetBias = 0x4000U;

struct PackageEntryProvenance final {
    std::uint32_t tag{};
    std::uint32_t package_id{};
    std::uint32_t entry_index{};
    std::uint32_t entry_class{};
    std::uint32_t decoded_size{};
    std::uint32_t entry_record_file_offset{};
    std::uint32_t logical_block{};
    std::uint32_t logical_offset{};
    std::uint32_t start_patch{};
    std::uint32_t physical_block_file_offset{};
    std::uint32_t stored_block_size{};
    std::uint32_t flags{};
};

[[nodiscard]] PackageEntryProvenance package_entry_provenance(std::uint32_t tag) noexcept;

enum class Platform : std::uint8_t { pc_windows_x64, ps4_sysv_amd64 };

enum class BoundaryKind : std::uint8_t {
    function_entry,
    callsite_before,
    callsite_after,
    instruction_before,
    instruction_after,
    function_return_after_original,
    post_state,
    exact_short_function,
};

enum class ObservationBracket : std::uint8_t {
    entry_only,
    bracket_original,
    after_original,
    instruction_point,
};

enum class NativeAbi : std::uint8_t {
    instance_opaque_create_context_bool,
    instance_only,
    record_subject_bool,
    runtime_entry_player,
    instance_token,
    instance_token_registration_pair,
    callback_mask_context,
    callback_only,
    event_only,
    event_payload,
    no_arguments,
    manager_slot_mode_value_bool,
    manager_payload_event,
    list_context,
    node_context,
    instance_packet_reference,
    component_row,
    terminal_selector_pair,
    manager_runtime_handle,
    manager_only,
    lookup_pair_timing,
    delegate_pointer,
    observation_point,
};

enum class NativeSurface : std::uint8_t {
    type60_create_register,
    type60_start,
    type60_lifecycle_clear,
    type60_containment,
    type60_membership_set,
    type60_membership_clear,
    type60_local_registration,
    type60_authored_record_start,
    event_subscriber_register,
    event_subscriber_remove,
    event_enqueue,
    event_enqueue_payload,
    event_dispatch,
    event_dispatch_payload,
    event_drain,
    activity_script_component_register,
    activity_script_event_callback,
    vm_runner_tick,
    vm_list_sequence,
    vm_node_dispatch,
    type53_apply,
    type53_decoded_body,
    type53_post_copy,
    type53_tick,
    type53_generation_compare,
    type53_active_time_predicate,
    type53_record_reference_gate,
    type53_root_reference_gate,
    type53_root_predicate,
    type53_record_predicate,
    type53_selected_row_call,
    type53_post_terminal,
    type53_post_generation_store,
    selected_row_extractor,
    terminal_submit,
    terminal_core,
    voice_leaf_queue,
    voice_arbitrate,
    voice_actual_start,
    timed_presentation_publish,
    voice_teardown_update,
    timed_delegate_install,
    timed_delegate_setter,
    ps4_type53_apply,
    ps4_type53_decoded_body,
    ps4_type53_post_copy,
    ps4_type53_tick,
    ps4_type53_active_time_predicate,
    ps4_type53_record_reference_gate,
    ps4_type53_root_reference_gate,
    ps4_type53_root_predicate,
    ps4_type53_record_predicate,
    ps4_type53_terminal_call,
    ps4_type53_post_terminal,
    ps4_type53_generation_store,
    ps4_type53_post_generation_store,
    ps4_terminal_wrapper,
    ps4_terminal_core,
};

struct NativeBoundaryDescriptor final {
    NativeSurface surface{};
    Platform platform{};
    BoundaryKind kind{};
    NativeAbi abi{};
    std::uintptr_t rva{};
    std::size_t file_offset{};
    std::span<const std::byte> prefix{};
    Sha256 prefix_sha256{};
    ObservationBracket observation_bracket{};
    /** Non-zero only where the recovered function's exact active extent is known. */
    std::size_t exact_active_bytes{};
    /** Always false for callsites and known-short functions. No detour installer exists here. */
    bool inline_detour_14_safe{};
};

[[nodiscard]] NativeBoundaryDescriptor native_boundary(NativeSurface surface) noexcept;
[[nodiscard]] bool native_prefix_matches(NativeSurface surface,
                                         std::span<const std::byte> observed) noexcept;

enum class EndpointValidation : std::uint8_t {
    reference_anchor_valid,
    invalid_verified_artifact,
    mapping_not_bound_to_verification,
    invalid_surface,
    wrong_artifact,
    no_prefix_anchor,
    target_out_of_range,
    prefix_mismatch,
};

[[nodiscard]] EndpointValidation
validate_reference_endpoint(const VerifiedArtifact& artifact,
                            std::span<const std::byte> sameVerifiedFile,
                            NativeSurface surface) noexcept;

enum class EndpointAttachability : std::uint8_t {
    reference_anchor_only,
    structural_owner_and_call_edge_required,
};
inline constexpr bool kAnyEndpointAttachableFromPrefixOnly = false;

/** No surface is attachable from a prefix alone in this source-only module. */
[[nodiscard]] EndpointAttachability endpoint_attachability(NativeSurface surface) noexcept;

struct PcSubmitAbiContract final {
    std::int32_t r8d{-1};
    std::int32_t r9d{-1};
    std::int32_t stack_argument_5{};
    bool rcx_is_output{true};
    bool rdx_is_selector_pair{true};
};

struct Ps4TerminalAbiContract final {
    std::int32_t esi{-1};
    std::int32_t edx{-1};
    std::int32_t ecx{};
    bool rdi_is_selector_pair{true};
};

inline constexpr PcSubmitAbiContract kPcSubmitAbi{};
inline constexpr Ps4TerminalAbiContract kPs4TerminalAbi{};

// Exact source volume identity and runtime layout.
inline constexpr std::uint32_t kGhostVolumeRegistryTag = 0x80F47B5BU;
inline constexpr std::uint32_t kGhostVolumeRegistry = 0xBA5F26EFU;
inline constexpr std::uint32_t kGhostVolumeType = 60U;
inline constexpr std::uint32_t kGhostVolumeIndex = 4U;
inline constexpr std::uint32_t kGhostVolumePackedTypeIndex = 0x0004003CU;
inline constexpr std::uint32_t kGhostVolumeNameHash = 0xFDF6CB49U;
inline constexpr std::uint32_t kGhostVolumeDefinition = 0x80F47B4FU;
inline constexpr std::uint32_t kGhostVolumeWrapper = 0x80F47B51U;
inline constexpr std::uint32_t kGhostVolumeEntity = 0x80F47B50U;
inline constexpr std::uint32_t kGhostVolumeRuntimeClass = 0x808099C8U;
inline constexpr std::uint32_t kType60ClassRecordRva = 0x27E2C10U;
inline constexpr std::uint32_t kType60HandlerTableRva = 0x1C0F880U;
inline constexpr std::size_t kType60HandlerCount = 8U;
inline constexpr std::size_t kType60DefinitionCountOffset = 0x38U;
inline constexpr std::size_t kType60DefinitionDataOffset = 0x40U;
inline constexpr std::size_t kType60DefinitionRecordStride = 0x120U;
inline constexpr std::size_t kType60RegistrationTokenOffset = 0x62U;
inline constexpr std::size_t kType60RuntimeEntryCountOffset = 0x140U;
inline constexpr std::size_t kType60RuntimeEntriesOffset = 0x150U;
inline constexpr std::size_t kType60RuntimeEntryStride = 0x20U;
inline constexpr std::size_t kType60MembershipMaskOffset = 0x08U;
inline constexpr bool kType60HasAuthorityApply = false;
inline constexpr bool kType60HasSenseExport = false;
inline constexpr bool kFrozenRosterContainsType60 = false;
inline constexpr bool kColdSpawnInitialOverlapCaptureRequired = true;

struct Float3 final {
    float x{};
    float y{};
    float z{};
    friend constexpr bool operator==(Float3, Float3) noexcept = default;
};

inline constexpr Float3 kGhostVolumeOrigin{260.4328613F, 242.4115601F, 78.4487228F};
inline constexpr float kGhostVolumeExtrusion = 50.0F;
inline constexpr Float3 kGhostVolumeAabbMin{255.3617096F, 237.3612366F, 78.4487228F};
inline constexpr Float3 kGhostVolumeAabbMax{265.5040283F, 247.3700867F, 128.4487305F};

// Structural Type-54 destination. It is a content/VM symbol, not a native component.
inline constexpr std::uint32_t kType54RegistryTag = 0x80F47BC6U;
inline constexpr std::uint32_t kType54Registry = 0xF7A6CE7FU;
inline constexpr std::uint32_t kType54Type = 54U;
inline constexpr std::uint32_t kType54Index = 7U;
inline constexpr std::uint32_t kType54PackedTypeIndex = 0x00070036U;
inline constexpr std::uint32_t kGhostSelector = 0xAD60F465U;
inline constexpr bool kType54HasNativeComponent = false;
inline constexpr bool kPs4Type60CallbackFamilyRecovered = false;
inline constexpr bool kPs4ContainsOmegaMissionIdentity = false;
inline constexpr bool kPs4Type53GenericParityProven = true;

// Exact global Type-53 consumer and bank row zero.
inline constexpr std::uint32_t kGlobalRegistry = 0x82FB58B7U;
inline constexpr std::uint32_t kDialogueType = 53U;
inline constexpr std::uint32_t kDialogueIndex = 2U;
inline constexpr std::uint32_t kDialogueRecordIndex = 0U;
inline constexpr std::uint32_t kDialogueComponentClass = 0x80804F4BU;
inline constexpr std::uint32_t kDialogueAuthoritySchema = 0x80804F77U;
inline constexpr std::uint32_t kDialogueDefinition = 0x80F47BDAU;
inline constexpr std::uint32_t kDialogueBank = 0x80F1FD07U;
inline constexpr std::uint32_t kDialogueBankClass = 0x80808D54U;
inline constexpr std::uint32_t kGhostDurationBits = 0x40AE374CU;
inline constexpr float kGhostDurationSeconds = 5.444250106811523F;
inline constexpr std::uint32_t kGhostInternalId = 0x1B0C16D1U;
inline constexpr std::uint32_t kGhostLineId = 0x13E8F153U;
inline constexpr std::uint32_t kGhostPriority = 12U;
inline constexpr std::uint32_t kGhostChannel = 0x166521A5U;
inline constexpr float kGhostRootDelaySeconds = 1.0F;
inline constexpr float kGhostEligibilitySeconds = 10.0F;
inline constexpr std::uint32_t kGhostPrimaryVoice = 0x80F1FCFDU;
inline constexpr std::uint32_t kGhostPrimaryLookupBank = 0x80F1FCE8U;
inline constexpr std::uint32_t kGhostPrimaryCue = 0xB66EEB77U;
inline constexpr std::uint32_t kGhostAlternateVoice = 0xFFFFFFFFU;
inline constexpr std::uint32_t kGhostAlternateLookupBank = 0x80F1FCE9U;
inline constexpr std::uint32_t kGhostAlternateCue = 0x166D360EU;
inline constexpr std::uint32_t kGhostSpeaker = 0xEFCD14BEU;
inline constexpr std::uint32_t kGhostWwiseBankAndEvent = 0x22B41355U;
inline constexpr std::uint32_t kGhostWwiseAction = 0x245849CBU;
inline constexpr std::uint32_t kGhostWwiseSound = 0x0756D816U;
inline constexpr bool kSelectorSerializedInAuthorityRecord = false;

inline constexpr std::uint32_t kActivityMessageAuthorityType = 5U;
inline constexpr std::uint32_t kOmegaScenario = 0x80F47522U;
inline constexpr std::size_t kType53DecodedBodyBytes = 0x1008U;
inline constexpr std::size_t kType53RecordCount = 128U;
inline constexpr std::size_t kType53RecordsOffset = 0x08U;
inline constexpr std::size_t kType53RecordStride = 0x20U;
inline constexpr std::size_t kType53RecordGenerationOffset = 0x18U;
inline constexpr std::size_t kType53RecordModeOffset = 0x1CU;
inline constexpr std::size_t kPcType53CacheOffset = 0x180U;
inline constexpr std::size_t kPcType53RecordZeroOffset = 0x188U;
inline constexpr std::size_t kPcType53ProcessedGenerationOffset = 0x1188U;
inline constexpr std::size_t kPs4Type53CacheOffset = 0x178U;
inline constexpr std::size_t kPs4Type53RecordZeroOffset = 0x180U;
inline constexpr std::size_t kPs4Type53ProcessedGenerationOffset = 0x1180U;
inline constexpr std::uintptr_t kPs4GenerationStoreFirstRva = 0x004A6012U;
inline constexpr std::uintptr_t kPs4GenerationStoreLastRva = 0x004A6015U;
inline constexpr std::uintptr_t kPs4PostGenerationStoreRva = 0x004A601DU;

enum class EvidenceStrength : std::uint8_t {
    proven,
    strong_inference_live_capture_required,
    unknown_not_recovered,
    proven_negative,
};

struct BridgeChainClaim final {
    std::uint32_t source_registry{};
    std::uint32_t source_type{};
    std::uint32_t source_index{};
    std::uint32_t destination_registry{};
    std::uint32_t destination_type{};
    std::uint32_t destination_index{};
    std::uint32_t selector{};
    EvidenceStrength strength{};
    bool requires_live_capture{};
};

inline constexpr BridgeChainClaim kVolumeToType54Claim{
    kGhostVolumeRegistry,
    kGhostVolumeType,
    kGhostVolumeIndex,
    kType54Registry,
    kType54Type,
    kType54Index,
    kGhostSelector,
    EvidenceStrength::strong_inference_live_capture_required,
    true};

inline constexpr BridgeChainClaim kType54ToRecordZeroClaim{kType54Registry,
                                                           kType54Type,
                                                           kType54Index,
                                                           kGlobalRegistry,
                                                           kDialogueType,
                                                           kDialogueIndex,
                                                           kGhostSelector,
                                                           EvidenceStrength::unknown_not_recovered,
                                                           true};

inline constexpr EvidenceStrength kCombinedGhostBridgeStrength =
    EvidenceStrength::unknown_not_recovered;
inline constexpr bool kCombinedGhostBridgeRequiresLiveCapture = true;
inline constexpr bool kCombinedGhostBridgePromotable = false;

enum class ContextField : std::uint32_t {
    session = 1U << 0U,
    patch_epoch = 1U << 1U,
    destination = 1U << 2U,
    roster_generation = 1U << 3U,
    activity_instance_generation = 1U << 4U,
    thread = 1U << 5U,
    call = 1U << 6U,
    monotonic_time = 1U << 7U,
    return_address = 1U << 8U,
    build_provenance = 1U << 9U,
};

inline constexpr std::uint32_t kCompleteContextMask =
    static_cast<std::uint32_t>(ContextField::session)
    | static_cast<std::uint32_t>(ContextField::patch_epoch)
    | static_cast<std::uint32_t>(ContextField::destination)
    | static_cast<std::uint32_t>(ContextField::roster_generation)
    | static_cast<std::uint32_t>(ContextField::activity_instance_generation)
    | static_cast<std::uint32_t>(ContextField::thread)
    | static_cast<std::uint32_t>(ContextField::call)
    | static_cast<std::uint32_t>(ContextField::monotonic_time)
    | static_cast<std::uint32_t>(ContextField::return_address)
    | static_cast<std::uint32_t>(ContextField::build_provenance);

enum class CaptureOrigin : std::uint8_t {
    pc_client,
    ps4_client,
    retail_host_external_boundary,
};

enum class BuildProvenanceStatus : std::uint8_t {
    absent,
    exact_pinned_bytes_verified,
    external_digest_unverified,
};

enum class PrivacyClass : std::uint8_t {
    default_digest_and_pseudonym,
    secured_re_trace,
};

struct CaptureContext;

class BuildProvenanceSnapshot final {
public:
    BuildProvenanceSnapshot() noexcept = default;
    [[nodiscard]] ArtifactKind artifact() const noexcept {
        return artifact_;
    }
    [[nodiscard]] ArtifactIdentity identity() const noexcept {
        return identity_;
    }
    [[nodiscard]] BuildProvenanceStatus status() const noexcept {
        return status_;
    }
    friend constexpr bool operator==(BuildProvenanceSnapshot,
                                     BuildProvenanceSnapshot) noexcept = default;

private:
    friend bool bind_verified_build(CaptureContext&, const VerifiedArtifact&) noexcept;
    friend bool bind_external_build_digest(CaptureContext&, const ArtifactIdentity&) noexcept;
    ArtifactKind artifact_{ArtifactKind::external_retail_host_build};
    ArtifactIdentity identity_{};
    BuildProvenanceStatus status_{BuildProvenanceStatus::absent};
};

/** Exact per-row context required by the RE capture plan. Missing fields remain explicit. */
struct CaptureContext final {
    std::uint32_t presence_mask{};
    CaptureOrigin origin{CaptureOrigin::pc_client};
    ArtifactKind artifact{ArtifactKind::pc_unpacked_reference};
    std::uint64_t session_pseudonym{};
    std::uint64_t patch_epoch{};
    std::uint32_t destination{};
    std::uint64_t roster_generation{};
    std::uint64_t activity_instance_generation{};
    std::uint32_t thread_id{};
    std::uint64_t call_id{};
    std::uint64_t monotonic_tick{};
    std::uintptr_t return_rva{};
    BuildProvenanceSnapshot build{};
    PrivacyClass privacy{PrivacyClass::default_digest_and_pseudonym};
    friend constexpr bool operator==(CaptureContext, CaptureContext) noexcept = default;
};

[[nodiscard]] bool bind_verified_build(CaptureContext& context,
                                       const VerifiedArtifact& artifact) noexcept;
[[nodiscard]] bool bind_external_build_digest(CaptureContext& context,
                                              const ArtifactIdentity& identity) noexcept;

[[nodiscard]] constexpr bool context_has(const CaptureContext& context,
                                         ContextField field) noexcept {
    return (context.presence_mask & static_cast<std::uint32_t>(field)) != 0U;
}

/** Correlation completeness never controls whether raw scalar/hash evidence is retained. */
[[nodiscard]] bool fully_correlated(const CaptureContext& context) noexcept;

enum class CapturePhase : std::uint8_t {
    type60_register,
    type60_start,
    type60_containment,
    type60_membership_set,
    type60_membership_clear,
    subscriber_register,
    subscriber_remove,
    event_enqueue,
    event_dispatch,
    event_drain,
    activity_script_register,
    activity_script_callback,
    vm_runner,
    vm_node,
    type5_publication,
    type53_apply,
    generation_gate,
    row_zero_submit,
    actual_start,
    timed_presentation,
    deadline_drop,
    stop,
    free_record,
};

struct CaptureHeader final {
    CaptureContext context{};
    CapturePhase phase{};
    std::uint64_t sequence{};
    friend constexpr bool operator==(CaptureHeader, CaptureHeader) noexcept = default;
};

enum class OverlapObservationKind : std::uint8_t {
    unknown,
    initial_level,
    enter_edge,
    periodic_query,
    dwell,
    exit_edge,
    reentry_edge,
};

enum class Type60Field : std::uint32_t {
    exact_identity = 1U << 0U,
    instance_pseudonym = 1U << 1U,
    record_pseudonym = 1U << 2U,
    subject_pseudonym = 1U << 3U,
    player_pseudonym = 1U << 4U,
    registration_token = 1U << 5U,
    registration_pair = 1U << 6U,
    overlap_kind = 1U << 7U,
    world_latch = 1U << 8U,
    player_present_latch = 1U << 9U,
    inside_result = 1U << 10U,
    player_index = 1U << 11U,
    membership_before = 1U << 12U,
    membership_after = 1U << 13U,
};

[[nodiscard]] constexpr bool type60_has(std::uint32_t mask, Type60Field field) noexcept {
    return (mask & static_cast<std::uint32_t>(field)) != 0U;
}

/** Default telemetry carries pseudonyms and scalars, never live ASLR pointers. */
struct Type60Evidence final {
    CaptureHeader header{};
    std::uint32_t presence_mask{};
    std::uint64_t instance_pseudonym{};
    std::uint64_t record_pseudonym{};
    std::uint64_t subject_pseudonym{};
    std::uint64_t player_pseudonym{};
    std::int16_t registration_token{};
    std::array<std::uint64_t, 2U> registration_pair{};
    std::uint32_t definition{};
    std::uint32_t registry{};
    std::uint32_t type{};
    std::uint32_t index{};
    std::uint32_t name_hash{};
    std::uint32_t player_index{};
    std::uint64_t membership_before{};
    std::uint64_t membership_after{};
    OverlapObservationKind overlap_kind{OverlapObservationKind::unknown};
    bool world_latch{};
    bool player_present_latch{};
    bool inside{};
    bool original_result{};
    friend constexpr bool operator==(Type60Evidence, Type60Evidence) noexcept = default;
};

enum class DynamicBoundaryKind : std::uint8_t {
    subscriber_registration,
    subscriber_removal,
    bus_enqueue,
    bus_dispatch,
    bus_drain,
    activity_component_registration,
    activity_event_callback,
    vm_runner,
    vm_list,
    vm_node,
};

enum class DynamicField : std::uint32_t {
    callback_rva = 1U << 0U,
    subscriber_context_pseudonym = 1U << 1U,
    subscriber_mask = 1U << 2U,
    event_ordinal = 1U << 3U,
    payload_length = 1U << 4U,
    payload_sha256 = 1U << 5U,
    manager_pseudonym = 1U << 6U,
    activity_fields = 1U << 7U,
    node_rva = 1U << 8U,
    node_class = 1U << 9U,
    owner_provenance = 1U << 10U,
    vm_context_sha256 = 1U << 11U,
};

enum class OwnerProvenanceState : std::uint8_t {
    not_observed,
    absent_observed,
    present,
};

[[nodiscard]] constexpr bool dynamic_has(std::uint32_t mask, DynamicField field) noexcept {
    return (mask & static_cast<std::uint32_t>(field)) != 0U;
}

/** Dynamic identities are RVAs or pseudonyms; default payload content is SHA-256 only. */
struct DynamicBridgeEvidence final {
    CaptureHeader header{};
    std::uint32_t presence_mask{};
    DynamicBoundaryKind boundary{};
    std::uintptr_t callback_rva{};
    std::uint64_t subscriber_context_pseudonym{};
    std::uint64_t subscriber_mask{};
    std::uint32_t event_ordinal{};
    std::uint32_t payload_bytes{};
    Sha256 payload_sha256{};
    std::uint64_t manager_pseudonym{};
    std::uint32_t activity_slot{};
    std::uint32_t activity_mode{};
    std::uint32_t activity_value{};
    std::uintptr_t node_rva{};
    std::uint32_t node_class{};
    std::uint32_t owner_datum{};
    std::uint32_t owner_tag{};
    OwnerProvenanceState owner_provenance{OwnerProvenanceState::not_observed};
    Sha256 vm_context_sha256{};
    friend constexpr bool operator==(DynamicBridgeEvidence,
                                     DynamicBridgeEvidence) noexcept = default;
};

inline constexpr std::size_t kSecurePayloadPrefixBytes = 32U;
struct SecurePayloadPrefixEvidence final {
    CaptureHeader header{};
    PrivacyClass privacy{PrivacyClass::secured_re_trace};
    std::uint32_t copied_bytes{};
    std::uint32_t payload_bytes{};
    std::array<std::byte, kSecurePayloadPrefixBytes> prefix{};
    Sha256 payload_sha256{};
    friend constexpr bool operator==(SecurePayloadPrefixEvidence,
                                     SecurePayloadPrefixEvidence) noexcept = default;
};

enum class Type5Delivery : std::uint8_t {
    unknown,
    snapshot,
    delta,
    retry,
};

enum class AuthorityBodyState : std::uint8_t {
    omitted,
    neutral,
    active_record_zero,
};

struct RecordZeroScalars final {
    std::uint32_t presence_mask{};
    Sha256 root_reference_sha256{};
    std::uint64_t value{};
    std::uint64_t optional_value{};
    Sha256 record_reference_sha256{};
    std::uint32_t generation{};
    std::uint32_t mode{};
    bool optional_value_present{};
    friend constexpr bool operator==(RecordZeroScalars, RecordZeroScalars) noexcept = default;
};

enum class RecordZeroField : std::uint32_t {
    root_reference = 1U << 0U,
    value = 1U << 1U,
    optional_presence = 1U << 2U,
    optional_value = 1U << 3U,
    record_reference = 1U << 4U,
    generation = 1U << 5U,
    mode = 1U << 6U,
};

[[nodiscard]] constexpr bool record_zero_has(const RecordZeroScalars& record,
                                             RecordZeroField field) noexcept {
    return (record.presence_mask & static_cast<std::uint32_t>(field)) != 0U;
}

enum class AuthorityField : std::uint32_t {
    delivery = 1U << 0U,
    body_state = 1U << 1U,
    body_present = 1U << 2U,
    reset = 1U << 3U,
    body_bit_count = 1U << 4U,
    body_sha256 = 1U << 5U,
    record_zero = 1U << 6U,
    processed_before = 1U << 7U,
    processed_after = 1U << 8U,
    old_host_generation = 1U << 9U,
    new_host_generation = 1U << 10U,
    trigger_cause_sha256 = 1U << 11U,
};

[[nodiscard]] constexpr bool authority_has(std::uint32_t mask, AuthorityField field) noexcept {
    return (mask & static_cast<std::uint32_t>(field)) != 0U;
}

enum class GenerationDisposition : std::uint8_t {
    incomplete_capture,
    malformed_mode,
    unknown_unrecovered_mode,
    suppressed_equal_generation,
    retry_inactive,
    retry_eligibility_failure,
    consumed_without_submit,
    submitted_and_consumed,
};

struct Type5PublicationEvidence final {
    CaptureHeader header{};
    std::uint32_t presence_mask{};
    std::uint32_t activity_message_type{kActivityMessageAuthorityType};
    std::uint32_t registry{kGlobalRegistry};
    std::uint32_t type{kDialogueType};
    std::uint32_t index{kDialogueIndex};
    Type5Delivery delivery{Type5Delivery::unknown};
    AuthorityBodyState body_state{AuthorityBodyState::omitted};
    std::uint32_t body_bit_count{};
    Sha256 body_sha256{};
    RecordZeroScalars record_zero{};
    std::uint32_t old_host_generation{};
    std::uint32_t new_host_generation{};
    Sha256 trigger_cause_sha256{};
    bool body_present{};
    bool reset{};
    friend constexpr bool operator==(Type5PublicationEvidence,
                                     Type5PublicationEvidence) noexcept = default;
};

struct Type53ApplyEvidence final {
    CaptureHeader header{};
    std::uint32_t presence_mask{};
    std::uint32_t registry{kGlobalRegistry};
    std::uint32_t type{kDialogueType};
    std::uint32_t index{kDialogueIndex};
    AuthorityBodyState body_state{AuthorityBodyState::omitted};
    Sha256 body_sha256{};
    RecordZeroScalars record_zero{};
    std::uint32_t processed_generation_before{};
    std::uint32_t processed_generation_after{};
    bool body_present{};
    friend constexpr bool operator==(Type53ApplyEvidence, Type53ApplyEvidence) noexcept = default;
};

enum class ConsumerCorrelationProvenance : std::uint8_t {
    absent,
    observed_selected_row_extractor,
    observed_ps4_inlined_row_extractor,
};

enum class ConsumerCorrelationField : std::uint32_t {
    record_index = 1U << 0U,
    bank = 1U << 1U,
    selector = 1U << 2U,
    generation = 1U << 3U,
    processed_generation = 1U << 4U,
    mode = 1U << 5U,
    disposition = 1U << 6U,
};

struct Type53ConsumerCorrelation final {
    CaptureHeader header{};
    std::uint32_t presence_mask{};
    ConsumerCorrelationProvenance provenance{ConsumerCorrelationProvenance::absent};
    std::uint32_t record_index{};
    std::uint32_t bank{};
    std::uint32_t selector{};
    std::uint32_t generation{};
    std::uint32_t processed_generation{};
    std::uint32_t mode{};
    GenerationDisposition disposition{GenerationDisposition::incomplete_capture};
    friend constexpr bool operator==(Type53ConsumerCorrelation,
                                     Type53ConsumerCorrelation) noexcept = default;
};

enum class PresentationOutcome : std::uint8_t {
    submitted,
    started,
    timed_presented,
    deadline_dropped,
    stopped,
    freed,
};

inline constexpr std::size_t kVoiceSourceAOffset = 0x04U;
inline constexpr std::size_t kVoiceSourceBOffset = 0x08U;
inline constexpr std::size_t kVoiceSpeakerOffset = 0x0CU;
inline constexpr std::size_t kVoiceLaneOffset = 0x10U;
inline constexpr std::size_t kVoiceHandleOffset = 0x14U;
inline constexpr std::size_t kVoiceLookupBankOffset = 0x18U;
inline constexpr std::size_t kVoiceLookupHashOffset = 0x1CU;
inline constexpr std::size_t kVoiceOriginalStartOffset = 0x20U;
inline constexpr std::size_t kVoicePlannedStartOffset = 0x28U;
inline constexpr std::size_t kVoicePlannedEndOffset = 0x30U;
inline constexpr std::size_t kVoiceAbsoluteDeadlineOffset = 0x38U;
inline constexpr std::size_t kVoicePriorityOffset = 0x40U;
inline constexpr std::size_t kVoiceAudioHandleOffset = 0x44U;
inline constexpr std::size_t kVoiceStartedOffset = 0x48U;
inline constexpr std::size_t kVoiceTimedEnabledOffset = 0x49U;
inline constexpr std::size_t kVoiceCorrelationOffset = 0x4CU;

enum class VoiceStateField : std::uint32_t {
    runtime_handle = 1U << 0U,
    source_pair = 1U << 1U,
    speaker = 1U << 2U,
    lane = 1U << 3U,
    voice_selector = 1U << 4U,
    lookup_pair = 1U << 5U,
    original_start = 1U << 6U,
    planned_start = 1U << 7U,
    planned_end = 1U << 8U,
    absolute_deadline = 1U << 9U,
    priority = 1U << 10U,
    audio_handle = 1U << 11U,
    started = 1U << 12U,
    timed_enabled = 1U << 13U,
    correlation = 1U << 14U,
};

struct VoiceStateSnapshot final {
    std::uint32_t presence_mask{};
    std::uint32_t runtime_record_handle{};
    std::uint32_t source_a{};
    std::uint32_t source_b{};
    std::uint32_t speaker_hash{};
    std::uint32_t lane{};
    std::uint32_t voice_handle_or_selector{};
    std::uint32_t lookup_bank{};
    std::uint32_t lookup_hash{};
    std::uint64_t original_start{};
    std::uint64_t planned_start{};
    std::uint64_t planned_end{};
    std::uint64_t absolute_deadline{};
    std::uint32_t priority{};
    std::uint32_t audio_handle{};
    std::uint32_t correlation_value{};
    bool started{};
    bool timed_presentation_enabled{};
    friend constexpr bool operator==(VoiceStateSnapshot, VoiceStateSnapshot) noexcept = default;
};

struct PresentationEvidence final {
    CaptureHeader header{};
    PresentationOutcome outcome{PresentationOutcome::submitted};
    std::uint32_t selector{};
    std::uint32_t bank{};
    std::uint32_t record_index{};
    std::uint32_t generation{};
    VoiceStateSnapshot before{};
    VoiceStateSnapshot after{};
    friend constexpr bool operator==(PresentationEvidence, PresentationEvidence) noexcept = default;
};

[[nodiscard]] bool valid_evidence(const Type60Evidence& evidence) noexcept;
[[nodiscard]] bool valid_evidence(const DynamicBridgeEvidence& evidence) noexcept;
[[nodiscard]] bool valid_evidence(const SecurePayloadPrefixEvidence& evidence) noexcept;
[[nodiscard]] bool valid_evidence(const Type5PublicationEvidence& evidence) noexcept;
[[nodiscard]] bool valid_evidence(const Type53ApplyEvidence& evidence) noexcept;
[[nodiscard]] bool valid_evidence(const Type53ConsumerCorrelation& evidence) noexcept;
[[nodiscard]] bool valid_evidence(const PresentationEvidence& evidence) noexcept;

[[nodiscard]] bool same_observation(const Type60Evidence& left,
                                    const Type60Evidence& right) noexcept;
[[nodiscard]] bool same_observation(const DynamicBridgeEvidence& left,
                                    const DynamicBridgeEvidence& right) noexcept;
[[nodiscard]] bool same_observation(const SecurePayloadPrefixEvidence& left,
                                    const SecurePayloadPrefixEvidence& right) noexcept;
[[nodiscard]] bool same_observation(const Type5PublicationEvidence& left,
                                    const Type5PublicationEvidence& right) noexcept;
[[nodiscard]] bool same_observation(const Type53ApplyEvidence& left,
                                    const Type53ApplyEvidence& right) noexcept;
[[nodiscard]] bool same_observation(const Type53ConsumerCorrelation& left,
                                    const Type53ConsumerCorrelation& right) noexcept;
[[nodiscard]] bool same_observation(const PresentationEvidence& left,
                                    const PresentationEvidence& right) noexcept;

enum class QueuePushResult : std::uint8_t {
    enqueued,
    duplicate,
    rejected,
    full,
    busy,
    sequence_exhausted,
};

enum class QueuePopResult : std::uint8_t { success, empty, busy };

struct QueueCounters final {
    /** Concurrent snapshots are approximate; the final drained snapshot is authoritative. */
    std::uint64_t accepted{};
    std::uint64_t duplicates{};
    std::uint64_t rejected{};
    std::uint64_t dropped_full{};
    std::uint64_t dropped_busy{};
    std::uint64_t dropped_sequence_exhausted{};

    [[nodiscard]] constexpr std::uint64_t losses() const noexcept {
        return rejected + dropped_full + dropped_busy + dropped_sequence_exhausted;
    }
};

/** Fixed-capacity, allocation-free and nonblocking. A contended operation returns busy. */
template <typename Record, std::size_t Capacity> class FixedCaptureQueue final {
    static_assert(Capacity != 0U);
    static_assert(std::is_trivially_copyable_v<Record>);

public:
    FixedCaptureQueue() noexcept = default;
    FixedCaptureQueue(const FixedCaptureQueue&) = delete;
    FixedCaptureQueue& operator=(const FixedCaptureQueue&) = delete;

    [[nodiscard]] QueuePushResult try_push(const Record& input) noexcept {
        if (!valid_evidence(input)) {
            rejected_.fetch_add(1U, std::memory_order_relaxed);
            return QueuePushResult::rejected;
        }

        TryLock lock{*this};
        if (!lock) {
            dropped_busy_.fetch_add(1U, std::memory_order_relaxed);
            return QueuePushResult::busy;
        }
        if (has_last_ && fully_correlated(last_.header.context)
            && fully_correlated(input.header.context) && same_observation(last_, input)) {
            duplicates_.fetch_add(1U, std::memory_order_relaxed);
            return QueuePushResult::duplicate;
        }
        if (count_ == Capacity) {
            dropped_full_.fetch_add(1U, std::memory_order_relaxed);
            return QueuePushResult::full;
        }
        if (next_sequence_ == (std::numeric_limits<std::uint64_t>::max)()) {
            dropped_sequence_exhausted_.fetch_add(1U, std::memory_order_relaxed);
            return QueuePushResult::sequence_exhausted;
        }

        Record accepted = input;
        accepted.header.sequence = next_sequence_++;
        records_[(head_ + count_) % Capacity] = accepted;
        last_ = accepted;
        has_last_ = true;
        ++count_;
        accepted_.fetch_add(1U, std::memory_order_relaxed);
        return QueuePushResult::enqueued;
    }

    [[nodiscard]] QueuePopResult try_pop(Record& output) noexcept {
        TryLock lock{*this};
        if (!lock) {
            return QueuePopResult::busy;
        }
        if (count_ == 0U) {
            return QueuePopResult::empty;
        }
        output = records_[head_];
        head_ = (head_ + 1U) % Capacity;
        --count_;
        return QueuePopResult::success;
    }

    [[nodiscard]] QueueCounters counters() const noexcept {
        return QueueCounters{accepted_.load(std::memory_order_relaxed),
                             duplicates_.load(std::memory_order_relaxed),
                             rejected_.load(std::memory_order_relaxed),
                             dropped_full_.load(std::memory_order_relaxed),
                             dropped_busy_.load(std::memory_order_relaxed),
                             dropped_sequence_exhausted_.load(std::memory_order_relaxed)};
    }

#if defined(DAWN_GHOST_VM_BRIDGE_UNIT_TEST)
    // Compiled out of production headers. Tests must hold/release the same queue lock.
    [[nodiscard]] bool testing_lock() noexcept {
        return !lock_.test_and_set(std::memory_order_acquire);
    }
    void testing_unlock() noexcept {
        lock_.clear(std::memory_order_release);
    }
    void testing_set_next_sequence(std::uint64_t value) noexcept {
        TryLock lock{*this};
        if (lock) {
            next_sequence_ = value;
        }
    }
#endif

private:
    class TryLock final {
    public:
        explicit TryLock(FixedCaptureQueue& queue) noexcept
            : queue_(queue), owns_(!queue.lock_.test_and_set(std::memory_order_acquire)) {}
        ~TryLock() noexcept {
            if (owns_) {
                queue_.lock_.clear(std::memory_order_release);
            }
        }
        TryLock(const TryLock&) = delete;
        TryLock& operator=(const TryLock&) = delete;
        [[nodiscard]] explicit operator bool() const noexcept {
            return owns_;
        }

    private:
        FixedCaptureQueue& queue_;
        bool owns_{};
    };

    std::array<Record, Capacity> records_{};
    Record last_{};
    std::atomic_flag lock_ = ATOMIC_FLAG_INIT;
    std::size_t head_{};
    std::size_t count_{};
    std::uint64_t next_sequence_{1U};
    bool has_last_{};
    std::atomic<std::uint64_t> accepted_{};
    std::atomic<std::uint64_t> duplicates_{};
    std::atomic<std::uint64_t> rejected_{};
    std::atomic<std::uint64_t> dropped_full_{};
    std::atomic<std::uint64_t> dropped_busy_{};
    std::atomic<std::uint64_t> dropped_sequence_exhausted_{};
};

inline constexpr std::size_t kType60QueueCapacity = 64U;
inline constexpr std::size_t kDynamicQueueCapacity = 128U;
inline constexpr std::size_t kAuthorityQueueCapacity = 64U;
inline constexpr std::size_t kPresentationQueueCapacity = 128U;
inline constexpr bool kLiveQueueCapacityQualified = false;
inline constexpr bool kLiveQueueRateMeasurementRequired = true;
using Type60CaptureQueue = FixedCaptureQueue<Type60Evidence, kType60QueueCapacity>;
using DynamicCaptureQueue = FixedCaptureQueue<DynamicBridgeEvidence, kDynamicQueueCapacity>;
using SecurePayloadCaptureQueue =
    FixedCaptureQueue<SecurePayloadPrefixEvidence, kDynamicQueueCapacity>;
using Type5PublicationCaptureQueue =
    FixedCaptureQueue<Type5PublicationEvidence, kAuthorityQueueCapacity>;
using Type53ApplyCaptureQueue = FixedCaptureQueue<Type53ApplyEvidence, kAuthorityQueueCapacity>;
using ConsumerCorrelationCaptureQueue =
    FixedCaptureQueue<Type53ConsumerCorrelation, kAuthorityQueueCapacity>;
using PresentationCaptureQueue =
    FixedCaptureQueue<PresentationEvidence, kPresentationQueueCapacity>;

enum class CaptureCriticality : std::uint8_t { causal_transition, high_frequency_diagnostic };

[[nodiscard]] constexpr CaptureCriticality capture_criticality(CapturePhase phase) noexcept {
    switch (phase) {
    case CapturePhase::vm_runner:
    case CapturePhase::vm_node:
    case CapturePhase::event_drain:
        return CaptureCriticality::high_frequency_diagnostic;
    default:
        return CaptureCriticality::causal_transition;
    }
}

/** Fixed-window limiter; causal transitions are never sampled away. */
class CaptureRateLimiter final {
public:
    explicit CaptureRateLimiter(std::uint32_t highFrequencyLimit) noexcept
        : high_frequency_limit_(highFrequencyLimit) {}
    [[nodiscard]] bool admit(CapturePhase phase, std::uint64_t window) noexcept;
    [[nodiscard]] std::uint64_t rate_limited() const noexcept {
        return rate_limited_.load(std::memory_order_relaxed);
    }

private:
    std::uint32_t high_frequency_limit_{};
    std::atomic<std::uint64_t> window_and_count_{};
    std::atomic<std::uint64_t> rate_limited_{};
};

struct CausalWindowLossEvidence final {
    std::uint64_t session_pseudonym{};
    std::uint64_t activity_instance_generation{};
    QueueCounters counters{};
    std::uint64_t rate_limited{};
};

[[nodiscard]] constexpr bool
causal_window_loss_free(const CausalWindowLossEvidence& evidence) noexcept {
    return evidence.session_pseudonym != 0U && evidence.activity_instance_generation != 0U
           && evidence.counters.losses() == 0U && evidence.rate_limited == 0U;
}

struct GenerationInputs final {
    std::uint32_t record_generation{};
    std::uint32_t processed_generation{};
    std::uint32_t mode{};
    bool active_time_predicate{};
    bool duration_expired{};
    bool record_reference_passed{};
    bool root_reference_passed{};
    bool root_predicate_passed{};
    bool record_predicate_passed{};
    bool selected_row_call_observed{};
};

struct GenerationDecision final {
    GenerationDisposition disposition{GenerationDisposition::incomplete_capture};
    bool submit{};
    bool store_generation{};
    bool retry_later{};
};

[[nodiscard]] GenerationDecision evaluate_generation(const GenerationInputs& inputs) noexcept;

enum class ComponentContinuity : std::uint8_t { retained, replaced, unknown };
enum class MirrorContinuity : std::uint8_t {
    retained,
    replaced_observed,
    replaced_initializer_unknown,
    unknown,
};

enum class ReplayDisposition : std::uint8_t {
    no_body_in_snapshot_or_delta,
    neutralized_by_host,
    suppressed_same_generation,
    may_replay_different_generation,
    unknown_processed_mirror,
    invalid,
};

struct ReplayEvidence final {
    Type5Delivery delivery{Type5Delivery::unknown};
    AuthorityBodyState body_state{AuthorityBodyState::omitted};
    ComponentContinuity component{ComponentContinuity::unknown};
    MirrorContinuity mirror{MirrorContinuity::unknown};
    std::uint32_t record_generation{};
    std::uint32_t processed_generation{};
    std::uint32_t selector{};
    std::uint32_t previous_selector{};
    bool selector_observed{};
    bool previous_selector_observed{};
    bool processed_mirror_observed{};
};

struct ReplayAssessment final {
    ReplayDisposition disposition{ReplayDisposition::invalid};
    bool generation_equal{};
    bool selector_equal{};
    bool host_policy_required{};
};

[[nodiscard]] ReplayAssessment assess_replay(const ReplayEvidence& evidence) noexcept;

enum class LifecyclePhase : std::uint8_t { detached, active, quiescing };
inline constexpr bool kLifecycleAuthorizesNativeRemoval = false;
inline constexpr std::uint32_t kMaximumNestedObservationSurfaces = 32U;

struct LifecycleSnapshot final {
    LifecyclePhase phase{LifecyclePhase::detached};
    std::uint32_t calls_in_flight{};
};

/**
 * Testable call-ownership contract only. It neither installs nor removes hooks. A call that starts
 * active retains pre/post observation rights; quiescing blocks new observations but every helper
 * still forwards the supplied original exactly once.
 */
class OriginalOnceLifecycle final {
public:
    explicit OriginalOnceLifecycle(std::uint64_t endpointOwnerKey) noexcept
        : endpoint_owner_key_(endpointOwnerKey) {}
    class CallScope final {
    public:
        explicit CallScope(OriginalOnceLifecycle& owner) noexcept;
        ~CallScope() noexcept;
        CallScope(const CallScope&) = delete;
        CallScope& operator=(const CallScope&) = delete;
        [[nodiscard]] bool accepts_observation() const noexcept {
            return observes_;
        }

    private:
        OriginalOnceLifecycle& owner_;
        bool owns_call_{};
        bool observes_{};
        bool pushed_observation_key_{};
    };

    [[nodiscard]] bool activate() noexcept;
    [[nodiscard]] bool begin_quiesce() noexcept;
    /** Bookkeeping only. Native removal still requires the external hook library's unhook barrier.
     */
    [[nodiscard]] bool try_detach() noexcept;
    [[nodiscard]] LifecycleSnapshot snapshot() const noexcept;

private:
    friend class CallScope;
    static constexpr std::uint64_t kCountMask = 0x00000000FFFFFFFFULL;
    static constexpr std::uint64_t kPhaseShift = 32U;
    [[nodiscard]] static constexpr std::uint64_t pack(LifecyclePhase phase,
                                                      std::uint32_t count) noexcept {
        return (static_cast<std::uint64_t>(phase) << kPhaseShift) | count;
    }
    [[nodiscard]] static constexpr LifecyclePhase phase_of(std::uint64_t state) noexcept {
        return static_cast<LifecyclePhase>(state >> kPhaseShift);
    }
    [[nodiscard]] static constexpr std::uint32_t count_of(std::uint64_t state) noexcept {
        return static_cast<std::uint32_t>(state & kCountMask);
    }

    std::atomic<std::uint64_t> state_{pack(LifecyclePhase::detached, 0U)};
    std::uint64_t endpoint_owner_key_{};
    inline static thread_local std::array<std::uint64_t, kMaximumNestedObservationSurfaces>
        active_observation_keys_{};
    inline static thread_local std::uint32_t active_observation_count_{};
};

template <typename Function, typename PreObserver, typename PostObserver, typename... Arguments>
void forward_void_original_once(OriginalOnceLifecycle& lifecycle,
                                Function original,
                                PreObserver preObserver,
                                PostObserver postObserver,
                                Arguments... arguments) noexcept {
    static_assert(std::is_nothrow_invocable_r_v<void, Function, Arguments...>);
    static_assert(std::is_nothrow_invocable_r_v<void, PreObserver>);
    static_assert(std::is_nothrow_invocable_r_v<void, PostObserver>);
    OriginalOnceLifecycle::CallScope call{lifecycle};
    if (call.accepts_observation()) {
        std::invoke(preObserver);
    }
    std::invoke(original, arguments...);
    if (call.accepts_observation()) {
        std::invoke(postObserver);
    }
}

template <typename Function, typename PreObserver, typename PostObserver, typename... Arguments>
[[nodiscard]] std::invoke_result_t<Function, Arguments...>
forward_value_original_once(OriginalOnceLifecycle& lifecycle,
                            Function original,
                            PreObserver preObserver,
                            PostObserver postObserver,
                            Arguments... arguments) noexcept {
    static_assert(!std::is_void_v<std::invoke_result_t<Function, Arguments...>>);
    static_assert(std::is_nothrow_invocable_v<Function, Arguments...>);
    static_assert(std::is_nothrow_invocable_r_v<void, PreObserver>);
    static_assert(std::is_nothrow_invocable_r_v<void, PostObserver>);
    OriginalOnceLifecycle::CallScope call{lifecycle};
    if (call.accepts_observation()) {
        std::invoke(preObserver);
    }
    auto result = std::invoke(original, arguments...);
    if (call.accepts_observation()) {
        std::invoke(postObserver);
    }
    return result;
}

static_assert(std::is_trivially_copyable_v<Type60Evidence>);
static_assert(std::is_trivially_copyable_v<DynamicBridgeEvidence>);
static_assert(std::is_trivially_copyable_v<SecurePayloadPrefixEvidence>);
static_assert(std::is_trivially_copyable_v<Type5PublicationEvidence>);
static_assert(std::is_trivially_copyable_v<Type53ApplyEvidence>);
static_assert(std::is_trivially_copyable_v<Type53ConsumerCorrelation>);
static_assert(std::is_trivially_copyable_v<PresentationEvidence>);
static_assert(kVolumeToType54Claim.strength
              == EvidenceStrength::strong_inference_live_capture_required);
static_assert(kVolumeToType54Claim.requires_live_capture);
static_assert(kType54ToRecordZeroClaim.requires_live_capture);
static_assert(kCombinedGhostBridgeStrength == EvidenceStrength::unknown_not_recovered);
static_assert(!kCombinedGhostBridgePromotable);

} // namespace dawn::client::hooks::bootflow::opening_authority::ghost_vm_bridge_capture
