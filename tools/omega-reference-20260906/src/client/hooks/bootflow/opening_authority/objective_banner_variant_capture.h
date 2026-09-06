#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

namespace sunrise::client::hooks::bootflow::opening_authority::
    objective_banner_variant_capture {

/**
 * Source-only observation support for the shared activity-intro/objective banner path.
 *
 * This module owns no native hook and exposes no UI or queue writer. Future sole hook owners may
 * copy exact pre/post observations into these fixed records after pinned-build admission. Native
 * state remains authoritative; this module never creates a Type-68 body or presentation.
 */
inline constexpr bool kObservationOnly = true;
inline constexpr bool kOwnsNativeDetour = false;
inline constexpr bool kWritesNativeState = false;
inline constexpr bool kWritesBannerQueue = false;
inline constexpr bool kWritesManagerState = false;
inline constexpr bool kSynthesizesType68Authority = false;
inline constexpr bool kInvokesUiWriter = false;
inline constexpr bool kInvokesNativeProducer = false;
inline constexpr bool kInvokesNativeQueueHelper = false;
inline constexpr bool kInvokesNativeManagerHelper = false;
inline constexpr bool kDefaultTelemetryContainsRawPayload = false;
inline constexpr bool kPresentationCompletionIsAuthority = false;
inline constexpr bool kQueueHasReplicatedAcknowledgement = false;

enum class EvidenceGrade : std::uint8_t { proven, inferred, unknown };

struct LocalizedPair final {
    std::uint32_t bank{};
    std::uint32_t hash{};
    friend constexpr bool operator==(LocalizedPair, LocalizedPair) noexcept = default;
};

inline constexpr std::uint32_t kOmegaActivityIndex = 299U;
inline constexpr std::uint32_t kOmegaActivityDefinition = 0x87AC2003U;
inline constexpr std::string_view kOmegaActivityName = "mission_scot";
inline constexpr LocalizedPair kOmegaTitle{0x81331697U, 0x47CAC8CFU};
inline constexpr LocalizedPair kMissionCatalogPair{0x8132F809U, 0x980BA1D8U};
inline constexpr LocalizedPair kOpeningPair1{0x80C71DD2U, 0x4BCAD15BU};
inline constexpr LocalizedPair kOpeningPair2{0x80C71DD2U, 0xA0071ABBU};
inline constexpr LocalizedPair kAbsentPair{0xFFFFFFFFU, 0xFFFFFFFFU};
inline constexpr std::uint8_t kMissionPresentationCategory = 0U;
inline constexpr std::uint32_t kMissionHeaderCommand = 5U;
inline constexpr std::array<std::uint32_t, 3U> kObjectiveCommands{1U, 2U, 6U};
inline constexpr EvidenceGrade kMissionCatalogCuiEdgeGrade = EvidenceGrade::inferred;
inline constexpr EvidenceGrade kOpeningModeGrade = EvidenceGrade::unknown;
inline constexpr EvidenceGrade kTransientTitleBodyBindingGrade = EvidenceGrade::unknown;

inline constexpr std::uint32_t kActivityRegistry = 0x82FB58B7U;
inline constexpr std::uint8_t kType68 = 68U;
inline constexpr std::uint8_t kType68Index = 0U;
inline constexpr std::uint32_t kType68ComponentClass = 0x80804F53U;
inline constexpr std::uint32_t kType68AuthoritySchema = 0x80804F67U;
inline constexpr std::uint32_t kType68Definition = 0x80F47BD4U;
inline constexpr std::uint32_t kType68ContentBank = 0x80F47BD3U;
inline constexpr std::uint32_t kOpeningEvent = 0xC252E306U;
inline constexpr std::uint32_t kType68WireBits = 4'802U;
inline constexpr std::uint32_t kType68DecodedBytes = 0x300U;
inline constexpr std::uint32_t kObjectiveManagerKind = 2U;
inline constexpr std::uint32_t kObjectiveManagerCapacity = 16U;
inline constexpr std::size_t kObjectiveManagerEntryStride = 0x148U;
inline constexpr std::size_t kObjectiveManagerCountOffset = 0x1480U;

inline constexpr std::string_view kBannerClassName =
    "cui::c_thud_banner_container_widget";
inline constexpr std::uint16_t kBannerProviderRecordCount = 0x40U;
inline constexpr std::size_t kBannerProviderRecordStride = 0x40U;
inline constexpr std::uint32_t kNativeBannerQueueCapacity = 16U;
inline constexpr std::size_t kNativeBannerQueueRecordStride = 0x38U;
inline constexpr std::size_t kHudGlobalQueueOffset = 0x3740U;
inline constexpr std::size_t kQueueCountOffset = 0x380U;
inline constexpr std::size_t kQueueDefinitionHandleOffset = 0x388U;
inline constexpr std::size_t kQueueCurrentSequenceOffset = 0x390U;
inline constexpr std::size_t kQueueRetireFlagOffset = 0x394U;
inline constexpr std::array<std::size_t, 3U> kQueueDispatchMetadataOffsets{
    0x398U, 0x39CU, 0x3A0U};
inline constexpr std::size_t kQueueCurrentValidityOffset = 0x3A8U;
inline constexpr std::size_t kQueueCurrentCommandOffset = 0x3ACU;
inline constexpr std::size_t kQueueCurrentPayloadOffset = 0x3B0U;
inline constexpr std::size_t kQueueNextSequenceOffset = 0x3E0U;
inline constexpr std::size_t kQueuePriorityTableOffset = 0x3E8U;

inline constexpr std::size_t kSha256Bytes = 32U;
using Sha256 = std::array<std::byte, kSha256Bytes>;

enum class ArtifactKind : std::uint8_t {
    pc_packed_runtime,
    pc_unpacked_reference,
    omega_activity_package,
    ps4_eboot_reference,
};

struct ArtifactIdentity final {
    std::uint64_t file_bytes{};
    Sha256 sha256{};
    friend constexpr bool operator==(const ArtifactIdentity&, const ArtifactIdentity&) noexcept =
        default;
};

struct ArtifactDescriptor final {
    ArtifactKind kind{};
    std::wstring_view path{};
    ArtifactIdentity identity{};
};

[[nodiscard]] const ArtifactDescriptor& artifact_descriptor(ArtifactKind kind) noexcept;
[[nodiscard]] bool matches_artifact(ArtifactKind kind,
                                    const ArtifactIdentity& identity) noexcept;
[[nodiscard]] std::uint64_t artifact_fingerprint(ArtifactKind kind) noexcept;

enum class NativeSurface : std::uint8_t {
    banner_class_accessor,
    banner_registry_initializer,
    hud_global_accessor,
    command5_producer,
    command5_insert,
    type68_apply,
    type68_install,
    type68_formatter,
    objective_manager_add,
    objective_model_materializer,
    objective_payload_builder,
    objective_insert,
    objective_manager_tick,
    queue_tick,
    queue_erase,
    queue_reset,
    queue_teardown,
    dispatch_loader,
    manager_terminal_predicate,
    component_teardown,
    instantiated_cui_node,
    count,
};

inline constexpr std::size_t kNativeSurfaceCount =
    static_cast<std::size_t>(NativeSurface::count);
inline constexpr std::size_t kNativePrefixBytes = 16U;

enum class BoundaryKind : std::uint8_t {
    function_entry,
    callsite_or_instruction_window,
    opaque_runtime_join,
};

enum class NativeAbi : std::uint8_t {
    descriptor_accessor,
    registry_initializer,
    no_argument_or_internal,
    queue_command_payload,
    component_authority_handle,
    component_record_row_event,
    event_activity_entry_changed,
    manager_entry,
    queue_record_or_index,
    cui_runtime_node,
};

struct NativeBoundaryDescriptor final {
    std::uintptr_t rva{};
    std::span<const std::byte> prefix{};
    BoundaryKind kind{BoundaryKind::opaque_runtime_join};
    NativeAbi abi{NativeAbi::no_argument_or_internal};
    bool direct_call_authorized{};
    bool native_write_authorized{};
};

[[nodiscard]] NativeBoundaryDescriptor native_boundary(NativeSurface surface) noexcept;
[[nodiscard]] bool native_prefix_matches(NativeSurface surface,
                                         std::span<const std::byte> observed) noexcept;

struct MappedPrefixObservation final {
    std::array<std::byte, kNativePrefixBytes> bytes{};
    std::uint8_t byte_count{};
};

struct RuntimeAdmissionEvidence final {
    ArtifactIdentity packed_runtime{};
    std::uintptr_t mapped_image_base{};
    std::uint32_t mapped_image_bytes{};
    std::array<MappedPrefixObservation, kNativeSurfaceCount> prefixes{};
};

enum class RuntimeAdmissionResult : std::uint8_t {
    admitted,
    packed_identity_mismatch,
    null_image_base,
    target_out_of_range,
    prefix_missing_or_mismatch,
};

[[nodiscard]] RuntimeAdmissionResult
validate_runtime_admission(const RuntimeAdmissionEvidence& evidence) noexcept;

struct BannerRegistryDescriptor final {
    std::uintptr_t class_accessor_rva{};
    std::uintptr_t class_name_rva{};
    std::uintptr_t descriptor_storage_rva{};
    std::uintptr_t secondary_registry_rva{};
    std::uintptr_t provider_table_rva{};
    std::uintptr_t metadata_rva{};
    std::uintptr_t initializer_rva{};
    std::uint16_t record_count{};
    std::uint16_t record_stride{};
};

[[nodiscard]] constexpr BannerRegistryDescriptor banner_registry_descriptor() noexcept {
    return {0x7DCA0U,
            0x1C86A18U,
            0x2FBA018U,
            0x2FB54F0U,
            0x1FF43C0U,
            0x2FB94B0U,
            0x1323DD0U,
            kBannerProviderRecordCount,
            static_cast<std::uint16_t>(kBannerProviderRecordStride)};
}

enum class ResourceRole : std::uint8_t {
    aggregate,
    mission_activity_intro,
    objective_branch_a,
    objective_branch_b,
    control_accessibility_excluded,
};

struct ResourceDescriptor final {
    ResourceRole role{};
    std::uint32_t tag{};
    std::uint32_t tag_class{};
    std::uint32_t file_bytes{};
    Sha256 sha256{};
    std::uint32_t node_base{};
    std::uint32_t node_count{};
    std::uint32_t new_objective_hash{};
    std::uint32_t new_objective_offset{};
};

[[nodiscard]] const ResourceDescriptor& resource_descriptor(ResourceRole role) noexcept;
[[nodiscard]] std::uint64_t resource_fingerprint(ResourceRole role) noexcept;

inline constexpr std::uint32_t kLayoutAggregate = 0x80BC7241U;
inline constexpr std::uint32_t kLayoutResourceClass = 0x80804825U;
inline constexpr std::uint32_t kObjectiveBranchATag = 0x80BC723BU;
inline constexpr std::uint32_t kObjectiveBranchAHash = 0xEB73F1DBU;
inline constexpr std::uint32_t kObjectiveBranchBTag = 0x80BC723DU;
inline constexpr std::uint32_t kObjectiveBranchBHash = 0x47680580U;
inline constexpr std::uint32_t kControlAccessibilityTag = 0x80BC6FFBU;
inline constexpr std::uint32_t kControlAccessibilityNewObjectiveHash = 0x963A9B85U;

enum class ObjectiveResourceBranch : std::uint8_t {
    unknown,
    branch_a,
    branch_b,
    excluded_control_accessibility,
    unrecognized,
};

inline constexpr ObjectiveResourceBranch kCompileTimeOmegaObjectiveBranch =
    ObjectiveResourceBranch::unknown;
inline constexpr EvidenceGrade kOmegaObjectiveBranchGrade = EvidenceGrade::unknown;

[[nodiscard]] constexpr ObjectiveResourceBranch classify_objective_resource(
    std::uint32_t resourceTag,
    std::uint32_t visibleHash) noexcept {
    if (resourceTag == kObjectiveBranchATag && visibleHash == kObjectiveBranchAHash) {
        return ObjectiveResourceBranch::branch_a;
    }
    if (resourceTag == kObjectiveBranchBTag && visibleHash == kObjectiveBranchBHash) {
        return ObjectiveResourceBranch::branch_b;
    }
    if (resourceTag == kControlAccessibilityTag
        || visibleHash == kControlAccessibilityNewObjectiveHash) {
        return ObjectiveResourceBranch::excluded_control_accessibility;
    }
    return ObjectiveResourceBranch::unrecognized;
}

[[nodiscard]] std::uint64_t scalar_hash(std::span<const std::byte> bytes) noexcept;
[[nodiscard]] std::uint64_t text_hash(std::string_view text) noexcept;

enum ContextPresence : std::uint64_t {
    context_build = std::uint64_t{1U} << 0U,
    context_admission_generation = std::uint64_t{1U} << 1U,
    context_capture_epoch = std::uint64_t{1U} << 2U,
    context_session = std::uint64_t{1U} << 3U,
    context_activity_generation = std::uint64_t{1U} << 4U,
    context_connection_generation = std::uint64_t{1U} << 5U,
    context_roster_generation = std::uint64_t{1U} << 6U,
    context_component_generation = std::uint64_t{1U} << 7U,
    context_manager_generation = std::uint64_t{1U} << 8U,
    context_hud_global_generation = std::uint64_t{1U} << 9U,
    context_cui_instance_generation = std::uint64_t{1U} << 10U,
    context_authority_generation = std::uint64_t{1U} << 11U,
    context_queue_generation = std::uint64_t{1U} << 12U,
    context_thread_call = std::uint64_t{1U} << 13U,
    context_monotonic_clock = std::uint64_t{1U} << 14U,
};

inline constexpr std::uint64_t kCompleteContextMask =
    context_build | context_admission_generation | context_capture_epoch | context_session
    | context_activity_generation | context_connection_generation | context_roster_generation
    | context_component_generation | context_manager_generation | context_hud_global_generation
    | context_cui_instance_generation | context_authority_generation | context_queue_generation
    | context_thread_call | context_monotonic_clock;

struct GenerationStamp final {
    std::uint64_t activity{};
    std::uint64_t connection{};
    std::uint64_t roster{};
    std::uint64_t component{};
    std::uint64_t manager{};
    std::uint64_t hud_global{};
    std::uint64_t cui_instance{};
    std::uint64_t authority_publication{};
    std::uint64_t queue{};
    friend constexpr bool operator==(const GenerationStamp&, const GenerationStamp&) noexcept =
        default;
};

struct CaptureContext final {
    std::uint64_t presence_mask{};
    ArtifactKind artifact{ArtifactKind::pc_packed_runtime};
    ArtifactIdentity build_identity{};
    bool build_verified{};
    std::uint64_t runtime_admission_generation{};
    std::uint64_t capture_epoch{};
    std::uint64_t session_id{};
    GenerationStamp generations{};
    std::uint64_t thread_id{};
    std::uint64_t call_id{};
    std::uint64_t monotonic_tick{};
    std::uintptr_t return_rva{};
    std::uint32_t activity_index{};
    std::uint32_t activity_definition{};
    friend constexpr bool operator==(const CaptureContext&, const CaptureContext&) noexcept =
        default;
};

[[nodiscard]] bool fully_correlated(const CaptureContext& context) noexcept;

enum class CapturePhase : std::uint8_t {
    command5_insert,
    type68_apply,
    type68_format,
    objective_materialize,
    objective_payload_build,
    objective_insert,
    dispatch_load,
    queue_promote,
    queue_supersede,
    queue_retire,
    queue_erase,
    queue_reset,
    queue_teardown,
    manager_add,
    manager_terminal,
    manager_remove,
    component_teardown,
    instantiated_resource,
    provider_node_binding,
    presentation_timeline,
};

struct CaptureHeader final {
    CaptureContext context{};
    CapturePhase phase{CapturePhase::command5_insert};
    NativeSurface surface{NativeSurface::command5_insert};
    GenerationStamp pre_generations{};
    GenerationStamp post_generations{};
    std::uint64_t pre_monotonic_tick{};
    std::uint64_t post_monotonic_tick{};
    std::uintptr_t participant_identity{};
};

[[nodiscard]] bool phase_surface_compatible(CapturePhase phase,
                                            NativeSurface surface) noexcept;
[[nodiscard]] bool valid_capture_header(const CaptureHeader& header) noexcept;

struct QueueSnapshot final {
    std::uintptr_t queue_identity{};
    std::uint32_t queued_count{};
    std::int32_t dispatch_definition_handle{-1};
    std::int64_t current_sequence{-1};
    std::uint32_t current_retire_or_supersede{};
    std::array<std::uint32_t, 3U> dispatch_metadata{};
    std::uint32_t current_validity_state{};
    std::int32_t current_command{-1};
    std::uint64_t current_payload_hash{};
    std::int64_t next_sequence{};
    std::uint64_t priority_compatibility_table_hash{};
    std::uint64_t exact_state_hash{};
    friend constexpr bool operator==(const QueueSnapshot&, const QueueSnapshot&) noexcept =
        default;
};

[[nodiscard]] std::uint64_t queue_snapshot_hash(const QueueSnapshot& snapshot) noexcept;
[[nodiscard]] QueueSnapshot seal_queue_snapshot(QueueSnapshot snapshot) noexcept;
[[nodiscard]] bool valid_queue_snapshot(const QueueSnapshot& snapshot) noexcept;

struct Command5Payload final {
    std::array<std::byte, 0x20U> raw{};
    LocalizedPair title{};
    std::uint8_t category{};
    std::uint32_t icon_theme{};
    std::array<std::byte, 16U> visual_tuple{};
    std::uint64_t exact_payload_hash{};
    bool exact_payload_observed{};
};

[[nodiscard]] bool valid_command5_payload(const Command5Payload& payload) noexcept;
[[nodiscard]] std::uint64_t command5_payload_hash(const Command5Payload& payload) noexcept;
[[nodiscard]] Command5Payload seal_command5_payload(Command5Payload payload) noexcept;
[[nodiscard]] Command5Payload project_command5_payload(
    const std::array<std::byte, 0x20U>& raw) noexcept;

struct Type68AuthorityObservation final {
    std::uint32_t registry{};
    std::uint8_t type{};
    std::uint8_t index{};
    std::uint32_t component_class{};
    std::uint32_t authority_schema{};
    std::uint32_t definition{};
    std::uint32_t content_bank{};
    std::uint32_t body_bits{};
    std::uint32_t decoded_bytes{};
    std::uint64_t decoded_body_hash{};
    std::uint32_t event{};
    std::int32_t selector{-1};
    std::int32_t record_variant{};
    std::int32_t decoded_lifecycle{-1};
    bool canonical_round_trip{};
};

[[nodiscard]] bool valid_type68_authority(
    const Type68AuthorityObservation& observation) noexcept;
[[nodiscard]] std::uint32_t type68_hud_identity(std::uint32_t event,
                                                std::int32_t variant) noexcept;

struct FormatterObservation final {
    std::uint32_t event{};
    std::int32_t record_variant{};
    std::array<LocalizedPair, 4U> authored_pairs{};
    std::array<LocalizedPair, 4U> manager_event_pairs{};
    std::uint8_t mode{};
    std::uint32_t hud_identity{};
    std::uint64_t exact_manager_event_hash{};
};

[[nodiscard]] bool valid_formatter_observation(const FormatterObservation& observation) noexcept;

struct ObjectiveModelObservation final {
    std::uint16_t activity_index{};
    std::uint8_t mode{};
    LocalizedPair resolved_activity_title{};
    LocalizedPair entry_header{};
    LocalizedPair entry_secondary{};
    std::array<LocalizedPair, 3U> first_subentry_pairs{};
    bool changed{};
    bool manager_ready{};
    std::uint64_t exact_model_hash{};
};

[[nodiscard]] bool valid_objective_model(const FormatterObservation& formatter,
                                         const ObjectiveModelObservation& model) noexcept;

inline constexpr std::size_t kObjectivePayloadBytes = 0x2EU;

struct ObjectivePayload final {
    std::uint32_t command{};
    std::array<std::byte, kObjectivePayloadBytes> raw{};
    LocalizedPair pair_at_04{};
    LocalizedPair pair_at_0c{};
    LocalizedPair pair_at_14{};
    LocalizedPair pair_at_1c{};
    std::uint8_t category_state{};
    std::int32_t command6_value{};
    std::uint8_t bit1_field{};
    std::uint8_t bit3_field{};
    std::uint64_t exact_payload_hash{};
    bool exact_payload_observed{};
};

[[nodiscard]] ObjectivePayload project_objective_payload(
    std::uint32_t command,
    const std::array<std::byte, kObjectivePayloadBytes>& raw) noexcept;
[[nodiscard]] bool valid_objective_payload(const ObjectivePayload& payload) noexcept;
[[nodiscard]] bool payload_matches_model(const ObjectivePayload& payload,
                                         const ObjectiveModelObservation& model) noexcept;

struct PriorityCompatibilityRow final {
    std::uint32_t command{};
    std::int32_t priority{};
    std::uint32_t duration_scalar{};
    std::uint32_t compatibility_bits{};
    std::uint64_t exact_row_hash{};
    bool observed{};
};

struct DispatchCandidate final {
    std::uint32_t tag{};
    std::uint32_t tag_class{};
    std::int32_t package_handle{-1};
    bool accepted{};
};

struct DispatchSelection final {
    std::array<DispatchCandidate, 16U> candidates{};
    std::uint8_t candidate_count{};
    std::uint32_t selected_tag{};
    std::uint32_t selected_class{};
    std::int32_t selected_package_handle{-1};
    std::uint64_t exact_enumeration_hash{};
};

enum class QueueEventKind : std::uint8_t {
    command5_insert,
    objective_insert,
    promote,
    supersede,
    retire,
    erase,
    reset,
    teardown,
    dispatch_load,
};

enum class QueueDisposition : std::uint8_t {
    observed,
    accepted,
    rejected_definition_unresolved,
    rejected_full,
    rejected_authored_policy,
};

struct QueueEvidence final {
    QueueEventKind event{QueueEventKind::command5_insert};
    QueueDisposition disposition{QueueDisposition::observed};
    std::uint32_t command{};
    QueueSnapshot pre{};
    QueueSnapshot post{};
    Command5Payload command5_payload{};
    ObjectivePayload objective_payload{};
    PriorityCompatibilityRow priority_row{};
    DispatchSelection dispatch{};
    std::uint32_t erased_index{};
    std::uint64_t erased_record_hash{};
};

[[nodiscard]] bool valid_queue_evidence(const QueueEvidence& evidence) noexcept;

enum class LifecycleSemantic : std::uint8_t {
    inactive_absent,
    active_installable,
    terminal_supported,
    replacement_removal,
};

struct LifecycleMapping final {
    std::int32_t decoded{};
    std::int32_t external{};
    std::int32_t internal{};
    LifecycleSemantic semantic{LifecycleSemantic::inactive_absent};
};

[[nodiscard]] constexpr LifecycleMapping map_type68_lifecycle(std::int32_t decoded) noexcept {
    if (decoded == -1) {
        return {-1, 0, -1, LifecycleSemantic::inactive_absent};
    }
    if (decoded == 0) {
        return {0, 3, 1, LifecycleSemantic::active_installable};
    }
    if (decoded == 1) {
        return {1, 5, 4, LifecycleSemantic::terminal_supported};
    }
    return {decoded, 4, 3, LifecycleSemantic::replacement_removal};
}

struct ManagerEntrySnapshot final {
    std::uintptr_t manager_identity{};
    std::uint32_t entry_count{};
    std::uint64_t entry_identity_hash{};
    std::int32_t decoded_lifecycle{-1};
    std::int32_t external_status{};
    std::int32_t internal_status{-1};
    std::uint32_t flags{};
    std::uint8_t new_mode{};
    std::uint64_t exact_state_hash{};
    friend constexpr bool operator==(const ManagerEntrySnapshot&,
                                     const ManagerEntrySnapshot&) noexcept = default;
};

[[nodiscard]] std::uint64_t manager_snapshot_hash(const ManagerEntrySnapshot& snapshot) noexcept;
[[nodiscard]] ManagerEntrySnapshot seal_manager_snapshot(
    ManagerEntrySnapshot snapshot) noexcept;
[[nodiscard]] bool valid_manager_snapshot(const ManagerEntrySnapshot& snapshot) noexcept;

enum class ManagerEventKind : std::uint8_t {
    add,
    terminal_predicate,
    remove,
    replacement_old_status,
    component_teardown,
};

struct ManagerEvidence final {
    ManagerEventKind event{ManagerEventKind::add};
    ManagerEntrySnapshot pre{};
    ManagerEntrySnapshot post{};
    bool terminal_predicate_result{};
    bool old_status_sent_before_new_install{};
};

[[nodiscard]] bool valid_manager_evidence(const ManagerEvidence& evidence) noexcept;

enum class NodeSemantic : std::uint8_t {
    unknown,
    mission_title,
    mission_category,
    transient_title,
    transient_body,
    persistent_header,
    persistent_body,
};

enum class PayloadPairSlot : std::uint8_t { at_04, at_0c, at_14, at_1c };

struct ProviderPairDescriptor final {
    PayloadPairSlot slot{};
    std::uint32_t wrapper_id{};
    std::uint32_t getter_id{};
    std::size_t payload_offset{};
};

[[nodiscard]] constexpr ProviderPairDescriptor provider_descriptor(PayloadPairSlot slot) noexcept {
    switch (slot) {
    case PayloadPairSlot::at_04:
        return {slot, 0x9A21F3C9U, 0x542C4F72U, 0x04U};
    case PayloadPairSlot::at_0c:
        return {slot, 0x16454C71U, 0xFE71CB34U, 0x0CU};
    case PayloadPairSlot::at_14:
        return {slot, 0x6BDAA032U, 0x99F4E371U, 0x14U};
    case PayloadPairSlot::at_1c:
        return {slot, 0x8A385542U, 0x1541CD5BU, 0x1CU};
    }
    return {};
}

struct ResourceNodeSnapshot final {
    std::uintptr_t cui_instance_identity{};
    std::uint32_t resource_tag{};
    std::uint32_t resource_class{};
    std::uint32_t program_tag{};
    std::uint32_t node_index{};
    std::uint32_t property_id{};
    std::uint32_t visible_constant_hash{};
    std::int64_t queue_sequence{-1};
    std::int32_t queue_command{-1};
    bool active{};
    bool visible{};
    std::uint64_t exact_node_hash{};
    std::uint64_t exact_resource_hash{};
    friend constexpr bool operator==(const ResourceNodeSnapshot&,
                                     const ResourceNodeSnapshot&) noexcept = default;
};

[[nodiscard]] std::uint64_t resource_node_hash(const ResourceNodeSnapshot& snapshot) noexcept;
[[nodiscard]] ResourceNodeSnapshot seal_resource_node(ResourceNodeSnapshot snapshot) noexcept;
[[nodiscard]] bool valid_resource_node(const ResourceNodeSnapshot& snapshot) noexcept;

struct ResourceNodeEvidence final {
    ResourceNodeSnapshot pre{};
    ResourceNodeSnapshot post{};
    ObjectiveResourceBranch observed_branch{ObjectiveResourceBranch::unknown};
};

[[nodiscard]] bool valid_visible_variant(const ResourceNodeEvidence& evidence) noexcept;

struct NodeBindingEvidence final {
    PayloadPairSlot slot{PayloadPairSlot::at_04};
    std::uint32_t wrapper_id{};
    std::uint32_t getter_id{};
    LocalizedPair returned_pair{};
    LocalizedPair resolved_pair{};
    std::uint64_t resolved_text_hash{};
    NodeSemantic semantic{NodeSemantic::unknown};
    ResourceNodeSnapshot node{};
};

[[nodiscard]] LocalizedPair payload_pair(const ObjectivePayload& payload,
                                         PayloadPairSlot slot) noexcept;
[[nodiscard]] bool valid_node_binding(const NodeBindingEvidence& binding,
                                      const ObjectivePayload& payload) noexcept;

struct MissionHeaderNodeEvidence final {
    NodeSemantic semantic{NodeSemantic::unknown};
    ResourceNodeSnapshot node{};
    LocalizedPair resolved_pair{};
    std::uint64_t resolved_text_hash{};
};

[[nodiscard]] bool valid_mission_header_node(
    const MissionHeaderNodeEvidence& evidence) noexcept;

struct VariantClosureEvidence final {
    ResourceNodeEvidence visible_resource{};
    std::array<NodeBindingEvidence, 4U> bindings{};
    std::uint8_t binding_count{};
};

enum class VariantClosureResult : std::uint8_t {
    closed_branch_a,
    closed_branch_b,
    incomplete_resource,
    incomplete_title_or_body,
    excluded_control_resource,
    invalid,
};

[[nodiscard]] VariantClosureResult close_dynamic_variant(
    const VariantClosureEvidence& evidence,
    const ObjectivePayload& payload) noexcept;

enum class PresentationLane : std::uint8_t {
    mission_header_shared_banner,
    objective_shared_banner,
    ghost_dialogue_independent,
    persistent_objective_display,
};

struct PresentationInterval final {
    PresentationLane lane{PresentationLane::mission_header_shared_banner};
    std::uint64_t first_tick{};
    std::uint64_t last_tick{};
    std::int64_t queue_sequence{-1};
    std::int32_t command{-1};
    bool visible{};
};

enum class TimelineResult : std::uint8_t {
    serialized_with_ghost_overlap,
    serialized_without_measured_ghost_overlap,
    shared_banner_overlap_invalid,
    invalid,
};

[[nodiscard]] TimelineResult assess_retail_timeline(
    const PresentationInterval& missionHeader,
    const PresentationInterval& objective,
    const PresentationInterval& ghost,
    const PresentationInterval& persistentTracker) noexcept;

enum class CaptureKind : std::uint8_t {
    type68_authority,
    formatter,
    objective_model,
    queue,
    manager,
    resource_node,
    node_binding,
    mission_header_node,
    timeline,
};

struct CaptureRecord final {
    CaptureHeader header{};
    std::uint64_t sequence{};
    CaptureKind kind{CaptureKind::type68_authority};
    Type68AuthorityObservation authority{};
    FormatterObservation formatter{};
    ObjectiveModelObservation model{};
    QueueEvidence queue{};
    ManagerEvidence manager{};
    ResourceNodeEvidence resource{};
    NodeBindingEvidence binding{};
    MissionHeaderNodeEvidence mission_node{};
    ObjectivePayload binding_payload{};
    VariantClosureEvidence closure{};
    PresentationInterval mission_header_interval{};
    PresentationInterval objective_interval{};
    PresentationInterval ghost_interval{};
    PresentationInterval tracker_interval{};
};

[[nodiscard]] bool valid_capture_record(const CaptureRecord& record) noexcept;

struct ScalarHashTelemetry final {
    std::uint64_t sequence{};
    std::uint64_t call_id{};
    std::uint64_t monotonic_tick{};
    std::uint64_t pre_hash{};
    std::uint64_t post_hash{};
    std::uint64_t payload_or_resource_hash{};
    std::uint64_t capture_epoch{};
    std::uint32_t command{};
    CapturePhase phase{};
    CaptureKind kind{};
    std::uint8_t outcome{};
};

[[nodiscard]] bool default_telemetry(const CaptureRecord& record,
                                     ScalarHashTelemetry& output) noexcept;

enum class QueuePushResult : std::uint8_t {
    enqueued,
    invalid,
    busy,
    full,
    sequence_exhausted,
};
enum class QueuePopResult : std::uint8_t { success, empty, busy };
enum class QueueResetResult : std::uint8_t {
    reset,
    not_confirmed_detached,
    busy,
    not_empty,
};

struct QueueCounters final {
    std::uint64_t enqueued{};
    std::uint64_t popped{};
    std::uint64_t rejected_invalid{};
    std::uint64_t dropped_busy{};
    std::uint64_t dropped_full{};
    std::uint64_t dropped_sequence_exhausted{};
};

template <std::size_t Capacity>
class FixedCaptureQueue final {
    static_assert(Capacity > 0U);

public:
    [[nodiscard]] QueuePushResult try_push(const CaptureRecord& source) noexcept {
        if (!valid_capture_record(source)) {
            rejected_invalid_.fetch_add(1U, std::memory_order_relaxed);
            return QueuePushResult::invalid;
        }
        if (lock_.test_and_set(std::memory_order_acquire)) {
            dropped_busy_.fetch_add(1U, std::memory_order_relaxed);
            return QueuePushResult::busy;
        }
        if (count_ == Capacity) {
            dropped_full_.fetch_add(1U, std::memory_order_relaxed);
            lock_.clear(std::memory_order_release);
            return QueuePushResult::full;
        }
        if (next_sequence_ == (std::numeric_limits<std::uint64_t>::max)()) {
            dropped_sequence_exhausted_.fetch_add(1U, std::memory_order_relaxed);
            lock_.clear(std::memory_order_release);
            return QueuePushResult::sequence_exhausted;
        }
        CaptureRecord copy = source;
        copy.sequence = ++next_sequence_;
        records_[tail_] = copy;
        tail_ = (tail_ + 1U) % Capacity;
        ++count_;
        enqueued_.fetch_add(1U, std::memory_order_relaxed);
        lock_.clear(std::memory_order_release);
        return QueuePushResult::enqueued;
    }

    [[nodiscard]] QueuePopResult try_pop(CaptureRecord& output) noexcept {
        if (lock_.test_and_set(std::memory_order_acquire)) {
            return QueuePopResult::busy;
        }
        if (count_ == 0U) {
            lock_.clear(std::memory_order_release);
            return QueuePopResult::empty;
        }
        output = records_[head_];
        head_ = (head_ + 1U) % Capacity;
        --count_;
        popped_.fetch_add(1U, std::memory_order_relaxed);
        lock_.clear(std::memory_order_release);
        return QueuePopResult::success;
    }

    [[nodiscard]] QueueResetResult try_reset(bool confirmedDetached) noexcept {
        if (!confirmedDetached) {
            return QueueResetResult::not_confirmed_detached;
        }
        if (lock_.test_and_set(std::memory_order_acquire)) {
            return QueueResetResult::busy;
        }
        if (count_ != 0U) {
            lock_.clear(std::memory_order_release);
            return QueueResetResult::not_empty;
        }
        head_ = 0U;
        tail_ = 0U;
        next_sequence_ = 0U;
        records_.fill({});
        enqueued_.store(0U, std::memory_order_relaxed);
        popped_.store(0U, std::memory_order_relaxed);
        rejected_invalid_.store(0U, std::memory_order_relaxed);
        dropped_busy_.store(0U, std::memory_order_relaxed);
        dropped_full_.store(0U, std::memory_order_relaxed);
        dropped_sequence_exhausted_.store(0U, std::memory_order_relaxed);
        lock_.clear(std::memory_order_release);
        return QueueResetResult::reset;
    }

    [[nodiscard]] QueueCounters counters() const noexcept {
        return {enqueued_.load(std::memory_order_relaxed),
                popped_.load(std::memory_order_relaxed),
                rejected_invalid_.load(std::memory_order_relaxed),
                dropped_busy_.load(std::memory_order_relaxed),
                dropped_full_.load(std::memory_order_relaxed),
                dropped_sequence_exhausted_.load(std::memory_order_relaxed)};
    }

    /** Unit-only contention/exhaustion seams; production capture never holds this lock. */
    [[nodiscard]] bool testing_lock() noexcept {
        return !lock_.test_and_set(std::memory_order_acquire);
    }
    void testing_unlock() noexcept { lock_.clear(std::memory_order_release); }
    void testing_set_next_sequence(std::uint64_t sequence) noexcept {
        next_sequence_ = sequence;
    }

private:
    std::array<CaptureRecord, Capacity> records_{};
    std::atomic_flag lock_ = ATOMIC_FLAG_INIT;
    std::size_t head_{};
    std::size_t tail_{};
    std::size_t count_{};
    std::uint64_t next_sequence_{};
    std::atomic<std::uint64_t> enqueued_{};
    std::atomic<std::uint64_t> popped_{};
    std::atomic<std::uint64_t> rejected_invalid_{};
    std::atomic<std::uint64_t> dropped_busy_{};
    std::atomic<std::uint64_t> dropped_full_{};
    std::atomic<std::uint64_t> dropped_sequence_exhausted_{};
};

using ObjectiveBannerCaptureQueue = FixedCaptureQueue<64U>;

enum class ParticipantKind : std::uint8_t {
    command5_insert,
    type68_apply,
    type68_formatter,
    objective_materializer,
    objective_payload_builder,
    objective_insert,
    queue_tick,
    queue_erase,
    queue_reset,
    queue_teardown,
    dispatch_loader,
    manager_terminal,
    cui_node_observer,
};

struct ParticipantContract final {
    ParticipantKind kind{};
    NativeSurface surface{};
    bool original_required{};
    std::uint8_t required_original_calls{};
    bool captures_pre{};
    bool captures_post{};
    bool may_write_native_state{};
    bool may_call_native_directly{};
};

[[nodiscard]] ParticipantContract participant_contract(ParticipantKind kind) noexcept;

enum class ParticipantPhase : std::uint8_t {
    detached,
    activating,
    active,
    quiescing,
    removed_pending_reset,
};

struct ParticipantSnapshot final {
    ParticipantPhase phase{ParticipantPhase::detached};
    ParticipantKind kind{ParticipantKind::command5_insert};
    std::uint64_t admission_generation{};
    std::uint64_t capture_epoch{};
    std::uint32_t calls_in_flight{};
};

class OriginalOnceParticipant final {
public:
    explicit OriginalOnceParticipant(ParticipantKind kind) noexcept : kind_(kind) {}

    class CallScope final {
    public:
        explicit CallScope(OriginalOnceParticipant& owner) noexcept;
        ~CallScope();
        CallScope(const CallScope&) = delete;
        CallScope& operator=(const CallScope&) = delete;
        [[nodiscard]] bool accepts_observation() const noexcept { return observes_; }

    private:
        OriginalOnceParticipant& owner_;
        const OriginalOnceParticipant* previous_observation_owner_{};
        bool owns_call_{};
        bool observes_{};
    };

    [[nodiscard]] bool activate(std::uint64_t admissionGeneration,
                                std::uint64_t captureEpoch) noexcept;
    [[nodiscard]] bool begin_quiesce() noexcept;
    [[nodiscard]] bool confirm_removed(bool aggregateDetachConfirmed) noexcept;
    [[nodiscard]] bool finalize_reset(bool persistenceAndQueueResetComplete) noexcept;
    [[nodiscard]] ParticipantSnapshot snapshot() const noexcept;

private:
    friend class CallScope;
    ParticipantKind kind_{};
    std::atomic<ParticipantPhase> phase_{ParticipantPhase::detached};
    std::atomic<std::uint64_t> admission_generation_{};
    std::atomic<std::uint64_t> capture_epoch_{};
    std::atomic<std::uint32_t> calls_in_flight_{};
    inline static thread_local const OriginalOnceParticipant* observation_owner_{};
};

template <typename Original, typename PreObserver, typename PostObserver, typename... Args>
void forward_void_original_once(OriginalOnceParticipant& participant,
                                Original&& original,
                                PreObserver&& preObserver,
                                PostObserver&& postObserver,
                                Args&&... args) noexcept {
    static_assert(std::is_nothrow_invocable_r_v<void, Original, Args...>);
    static_assert(std::is_nothrow_invocable_r_v<void, PreObserver>);
    static_assert(std::is_nothrow_invocable_r_v<void, PostObserver>);
    OriginalOnceParticipant::CallScope scope{participant};
    if (scope.accepts_observation()) {
        std::invoke(std::forward<PreObserver>(preObserver));
    }
    std::invoke(std::forward<Original>(original), std::forward<Args>(args)...);
    if (scope.accepts_observation()) {
        std::invoke(std::forward<PostObserver>(postObserver));
    }
}

template <typename Original, typename PreObserver, typename PostObserver, typename... Args>
std::invoke_result_t<Original, Args...>
forward_value_original_once(OriginalOnceParticipant& participant,
                            Original&& original,
                            PreObserver&& preObserver,
                            PostObserver&& postObserver,
                            Args&&... args) noexcept {
    using Result = std::invoke_result_t<Original, Args...>;
    static_assert(!std::is_void_v<Result>);
    static_assert(std::is_nothrow_invocable_v<Original, Args...>);
    static_assert(std::is_nothrow_invocable_r_v<void, PreObserver>);
    static_assert(std::is_nothrow_invocable_r_v<void, PostObserver>);
    OriginalOnceParticipant::CallScope scope{participant};
    if (scope.accepts_observation()) {
        std::invoke(std::forward<PreObserver>(preObserver));
    }
    Result result = std::invoke(std::forward<Original>(original), std::forward<Args>(args)...);
    if (scope.accepts_observation()) {
        std::invoke(std::forward<PostObserver>(postObserver));
    }
    return result;
}

} // namespace sunrise::client::hooks::bootflow::opening_authority::
  // objective_banner_variant_capture
