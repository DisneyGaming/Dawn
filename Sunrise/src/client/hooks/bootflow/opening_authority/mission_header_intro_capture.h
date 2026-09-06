#pragma once

#include "../../../hooking/call_gate.h"

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

namespace sunrise::client::hooks::bootflow::opening_authority::mission_header_intro_capture {

inline constexpr bool kObservationOnly = true;
inline constexpr bool kOwnsNativeDetour = false;
inline constexpr bool kWritesNativeLatchState = false;
inline constexpr bool kWritesNativeQueueState = false;
inline constexpr bool kInvokesProducerSynthetically = false;
inline constexpr bool kInvokesQueueSynthetically = false;
inline constexpr bool kInvokesCuiSynthetically = false;
inline constexpr bool kProvidesDirectShowWriter = false;
inline constexpr bool kSynthesizesMissionHeader = false;
inline constexpr bool kRawEvidenceRequiresSecuredDrain = true;
inline constexpr bool kDefaultTelemetryContainsRawAddresses = false;

inline constexpr std::size_t kDigestBytes = 32U;
using Digest256 = std::array<std::byte, kDigestBytes>;
using Sha256 = Digest256;

enum class DigestAlgorithm : std::uint8_t { sha256, hmac_sha256 };

[[nodiscard]] Digest256 sha256_bytes(std::span<const std::byte> bytes) noexcept;
[[nodiscard]] bool sha256_file(std::wstring_view path,
                               std::uint64_t& fileBytes,
                               Digest256& digest) noexcept;

enum class ArtifactKind : std::uint8_t {
    pc_packed_runtime,
    pc_unpacked_provenance,
    omega_activity_package,
    ps4_eboot_provenance,
};

struct ArtifactDescriptor final {
    ArtifactKind kind{};
    std::wstring_view path{};
    std::uint64_t file_bytes{};
    Digest256 sha256{};
};

[[nodiscard]] const ArtifactDescriptor& artifact_descriptor(ArtifactKind kind) noexcept;

enum class EvidenceGrade : std::uint8_t { proven, inferred, unknown, runtime_observed };

inline constexpr EvidenceGrade kCommand5ProducerIdentityGrade = EvidenceGrade::proven;
inline constexpr EvidenceGrade kActivityTitlePairGrade = EvidenceGrade::proven;
inline constexpr EvidenceGrade kCategoryChainGrade = EvidenceGrade::proven;
inline constexpr EvidenceGrade kMissionCatalogPairGrade = EvidenceGrade::proven;
inline constexpr EvidenceGrade kCategoryZeroToMissionCuiEdgeGrade = EvidenceGrade::inferred;
inline constexpr EvidenceGrade kReplayPolicyGrade = EvidenceGrade::unknown;
inline constexpr EvidenceGrade kJoinInProgressPolicyGrade = EvidenceGrade::unknown;
inline constexpr EvidenceGrade kRuntimeDispatchDefinitionGrade = EvidenceGrade::unknown;
inline constexpr EvidenceGrade kLiveVisualTupleGrade = EvidenceGrade::unknown;
inline constexpr EvidenceGrade kPs4OmegaContentIdentityGrade = EvidenceGrade::unknown;

inline constexpr std::uint32_t kActivityIndex = 299U;
inline constexpr std::uint32_t kActivityDefinitionHash = 0x87AC2003U;
inline constexpr std::string_view kActivityInternalName = "mission_scot";
inline constexpr std::uint32_t kActivityPackageTag = 0x80F4750CU;
inline constexpr std::uint32_t kActivityPackageClass = 0x80808AAEU;
inline constexpr std::uint32_t kActivityScenarioTag = 0x80F47522U;
inline constexpr std::uint32_t kActivityRegistry = 0x82FB58B7U;

struct LocalizedPair final {
    std::uint32_t bank{};
    std::uint32_t hash{};
    friend constexpr bool operator==(LocalizedPair, LocalizedPair) noexcept = default;
};

inline constexpr LocalizedPair kOmegaTitle{0x81331697U, 0x47CAC8CFU};
inline constexpr std::uint16_t kOmegaTitleBankMapIndex = 0x303U;
inline constexpr LocalizedPair kMissionCatalogPair{0x8132F809U, 0x980BA1D8U};
inline constexpr std::uint16_t kMissionBankMapIndex = 4U;
inline constexpr std::uint16_t kMissionBankOrdinal = 0x3CU;
inline constexpr std::uint8_t kStoryActivityType = 0U;
inline constexpr std::uint8_t kStoryTypeClientCategorySource = 4U;
inline constexpr std::uint8_t kActivityPresentationCategory = 0U;
inline constexpr std::uint16_t kActivityIntroCommand = 5U;
inline constexpr std::uint16_t kNeighborActivityCommand = 8U;

enum class PresentationLane : std::uint8_t {
    arrival_or_location,
    activity_intro_command5,
    new_objective_notification,
    persistent_type68_tracker,
};

inline constexpr PresentationLane kOwnedEvidenceLane =
    PresentationLane::activity_intro_command5;
inline constexpr bool kCommand5IsType68ObjectiveBody = false;
inline constexpr bool kCommand5IsNewObjectiveNotification = false;
inline constexpr bool kCommand5IsPersistentTracker = false;
inline constexpr std::uint8_t kType68AuthorityType = 68U;
inline constexpr std::uint32_t kType68Definition = 0x80F47BD4U;
inline constexpr std::uint32_t kType68Bank = 0x80F47BD3U;
inline constexpr std::array<std::uint16_t, 3U> kType68ManagerCommandFamilies{1U, 2U, 6U};

enum class NativeSurface : std::uint8_t {
    command5_producer,
    producer_enqueue_anchor,
    queue_insert,
    activity_state_machine,
    state3_pending_ready_gate,
    producer_call_window,
    pending_clear_window,
    reset_or_direct_show,
    rearm,
    ready_setter,
    pre_presentation_channel_clear,
    hud_global_accessor,
    activity_category_resolver,
    presentation_category_mapper,
    localized_pair_resolver,
    queue_tick,
    lazy_dispatch_resolver,
    queue_reject_unresolved_window,
    queue_reject_full_window,
    queue_accept_commit_window,
    category_wrapper,
    category_getter,
    title_wrapper,
    title_getter,
    icon_theme_wrapper,
    icon_theme_getter,
    visual_tuple_wrapper,
    visual_tuple_getter,
    ready_caller,
    rearm_caller,
    reset_caller,
    direct_show_caller_0,
    direct_show_caller_1,
    count,
};

inline constexpr std::size_t kNativeSurfaceCount =
    static_cast<std::size_t>(NativeSurface::count);
inline constexpr std::size_t kNativePrefixBytes = 16U;

enum class BoundaryKind : std::uint8_t {
    function_entry,
    instruction_window,
    callsite_window,
};

enum class InstrumentationMethod : std::uint8_t {
    full_function_replacement,
    breakpoint_or_trampoline_with_register_frame,
    caller_side_join,
};

enum class NativeAbi : std::uint8_t {
    void_noargs,
    void_queue_i32_payload,
    void_queue,
    void_i32_mode,
    pointer_noargs,
    category_resolver_internal,
    presentation_category_internal,
    localized_pair_internal,
    category_getter_context_out_u8,
    title_getter_context_out_pair,
    icon_getter_context_out_u64,
    visual_getter_context_out_16,
    opaque_wrapper_entry,
    instruction_frame_only,
};

struct NativeBoundaryDescriptor final {
    std::uintptr_t rva{};
    std::span<const std::byte> prefix{};
    BoundaryKind kind{};
    InstrumentationMethod method{};
    NativeAbi abi{};
    std::uint32_t live_register_mask{};
    bool preserves_flags{};
    bool requires_unwind_protected_range{};
    bool direct_call_authorized{};
    bool inline_detour_authorized{};
};

[[nodiscard]] NativeBoundaryDescriptor native_boundary(NativeSurface surface) noexcept;
[[nodiscard]] bool native_prefix_matches(NativeSurface surface,
                                         std::span<const std::byte> observed) noexcept;

inline constexpr std::uintptr_t kPcCommand5ProducerRva = 0x131FDF0U;
inline constexpr std::uintptr_t kPcCommand5EnqueueAnchorRva = 0x131FFA8U;
inline constexpr std::uintptr_t kPcQueueInsertCallRva = 0x131FFBDU;
inline constexpr std::uintptr_t kPcQueueInsertRva = 0x131A630U;
inline constexpr std::uintptr_t kPcQueueRejectUnresolvedRva = 0x131A64AU;
inline constexpr std::uintptr_t kPcQueueRejectFullRva = 0x131A658U;
inline constexpr std::uintptr_t kPcQueueAcceptCommitRva = 0x131A7CEU;
inline constexpr std::uintptr_t kPcStateMachineRva = 0x1378EE4U;
inline constexpr std::uintptr_t kPcState3GateRva = 0x137907BU;
inline constexpr std::uintptr_t kPcProducerCallWindowRva = 0x1379080U;
inline constexpr std::uintptr_t kPcPendingClearRva = 0x13790A4U;
inline constexpr std::uintptr_t kPcResetDirectShowRva = 0x1376940U;
inline constexpr std::uintptr_t kPcRearmRva = 0x13776B0U;
inline constexpr std::uintptr_t kPcReadySetterRva = 0x1377D70U;
inline constexpr std::uintptr_t kPcQueueTickRva = 0x13A0220U;
inline constexpr std::uintptr_t kPcLazyDispatchResolverRva = 0x139A2B0U;
inline constexpr std::uintptr_t kPcCategoryCuiWrapperRva = 0x138DEF0U;
inline constexpr std::uintptr_t kPcCategoryCuiGetterRva = 0x138F270U;
inline constexpr std::uintptr_t kPcTitleCuiWrapperRva = 0x138E3D0U;
inline constexpr std::uintptr_t kPcTitleCuiGetterRva = 0x138F750U;
inline constexpr std::uintptr_t kPcIconThemeCuiWrapperRva = 0x138DF80U;
inline constexpr std::uintptr_t kPcIconThemeCuiGetterRva = 0x138F570U;
inline constexpr std::uintptr_t kPcVisualTupleCuiWrapperRva = 0x138E4F0U;
inline constexpr std::uintptr_t kPcVisualTupleCuiGetterRva = 0x1399130U;
inline constexpr std::uintptr_t kPcReadyCallerRva = 0xDC8251U;
inline constexpr std::uintptr_t kPcRearmCallerRva = 0x1407EABU;
inline constexpr std::uintptr_t kPcResetCallerRva = 0xC77FB6U;
inline constexpr std::array<std::uintptr_t, 2U> kPcDirectShowCallerRvas{0xC79EC2U,
                                                                      0xC79ED5U};

inline constexpr std::uintptr_t kPcArmLatchRva = 0x2FB6682U;
inline constexpr std::uintptr_t kPcPendingLatchRva = 0x2FB6683U;
inline constexpr std::uintptr_t kPcReadyLatchRva = 0x2FB6684U;
inline constexpr std::uintptr_t kPcHudGlobalRva = 0x2FB1600U;
inline constexpr std::size_t kPcHudQueueOffset = 0x3740U;
inline constexpr std::size_t kQueueCountOffset = 0x380U;
inline constexpr std::size_t kQueueDispatchHandleOffset = 0x388U;
inline constexpr std::size_t kQueueCurrentSequenceOffset = 0x390U;
inline constexpr std::size_t kQueueCurrentRetireOffset = 0x394U;
inline constexpr std::size_t kQueueCurrentActiveOffset = 0x3A8U;
inline constexpr std::size_t kQueueCurrentCommandOffset = 0x3ACU;
inline constexpr std::size_t kQueueCurrentPayloadOffset = 0x3B4U;
inline constexpr std::size_t kQueueNextSequenceOffset = 0x3E0U;
inline constexpr std::size_t kQueueAuthoredCommandTableOffset = 0x3E8U;
inline constexpr std::uint64_t kQueueCapacity = 16U;
inline constexpr std::size_t kQueueProjectionBytes = 0x418U;
inline constexpr std::size_t kCommand5PayloadBytes = 0x20U;
inline constexpr std::uint32_t kPinnedMappedSizeOfImage = 0x08A5EA00U;
inline constexpr std::uint64_t kPinnedPeImageBase = 0x140000000ULL;
inline constexpr std::uint16_t kPinnedPeMachine = 0x8664U;
inline constexpr std::uint16_t kPinnedPeSectionCount = 11U;
inline constexpr std::uint32_t kPinnedPeTimestamp = 0x5F43138BU;
inline constexpr std::uint32_t kPinnedPeEntryRva = 0x0187CDD8U;

enum class Ps4Surface : std::uint8_t {
    command5_producer,
    producer_enqueue_anchor,
    queue_insert,
    activity_state_machine,
    state3_pending_ready_gate,
    reset_or_direct_show,
    rearm,
    ready_setter,
    queue_tick,
    category_cui_provider,
    title_cui_provider,
    icon_theme_cui_provider,
    visual_tuple_cui_provider,
    count,
};

struct Ps4BoundaryDescriptor final {
    std::uintptr_t rva{};
    std::uintptr_t file_offset{};
    std::span<const std::byte> prefix{};
    bool generic_engine_homology_only{};
    bool omega_content_identity_proven{};
};

[[nodiscard]] Ps4BoundaryDescriptor ps4_boundary(Ps4Surface surface) noexcept;
[[nodiscard]] bool ps4_prefix_matches(Ps4Surface surface,
                                      std::span<const std::byte> observed) noexcept;

inline constexpr std::uintptr_t kPs4ArmLatchRva = 0x53E5104U;
inline constexpr std::uintptr_t kPs4PendingLatchRva = 0x53E5105U;
inline constexpr std::uintptr_t kPs4ReadyLatchRva = 0x53E5106U;
inline constexpr std::uintptr_t kPs4HudAccessorRva = 0xC1E8A0U;
inline constexpr std::uintptr_t kPs4LazyDispatchResolverRva = 0xF1E390U;
inline constexpr std::size_t kPs4ActivityManagerIndexFieldOffset = 0x8E00U;
inline constexpr std::size_t kPcActivityManagerIndexFieldOffset = 0x8E08U;

struct CuiRegistrationIdentity final {
    std::uintptr_t registration_record_rva{};
    std::uint32_t opaque_id_0{};
    std::uint32_t opaque_id_1{};
    std::uint32_t opaque_key{};
    std::uint32_t type_metadata{};
};

inline constexpr CuiRegistrationIdentity kCategoryCuiRegistration{
    0x1FF44A8U, 0x4FEBF9AEU, 0x5C2D1BA2U, 0U, 0U};
inline constexpr CuiRegistrationIdentity kTitleCuiRegistration{
    0x1FF4868U, 0x39BC9A2AU, 0xD60318F1U, 0U, 0U};
inline constexpr CuiRegistrationIdentity kIconThemeCuiRegistration{
    0x1FF5368U, 0xC7013BB7U, 0xE0AEAD32U, 0U, 0U};
inline constexpr CuiRegistrationIdentity kVisualTupleCuiRegistration{
    0x1FF53A8U, 0U, 0U, 0xCBFF00D7U, 0x82166BC0U};

enum class ReferenceAuditResult : std::uint8_t {
    exact,
    packed_runtime_mismatch,
    unpacked_runtime_mismatch,
    package_mismatch,
    ps4_mismatch,
    pc_prefix_mismatch,
    ps4_prefix_mismatch,
    pe_header_mismatch,
    io_failure,
};

/** Reads and hashes the four pinned artifacts; no caller-supplied digest participates. */
[[nodiscard]] ReferenceAuditResult audit_pinned_reference_artifacts() noexcept;

enum class AdmissionKind : std::uint8_t { invalid, live_main_module, protected_test_image };
enum class AdmissionResult : std::uint8_t {
    admitted,
    owner_entropy_unavailable,
    main_module_unavailable,
    main_module_path_mismatch,
    packed_runtime_mismatch,
    package_mismatch,
    mapped_pe_mismatch,
    mapped_base_mismatch,
    mapped_bounds_mismatch,
    page_not_committed,
    page_not_executable,
    prefix_mismatch,
    post_decryption_not_ready,
    invalid_arguments,
};

class AdmissionOwner;

class AdmissionToken final {
public:
    AdmissionToken() noexcept = default;
    [[nodiscard]] AdmissionKind kind() const noexcept { return kind_; }
    [[nodiscard]] std::uint64_t owner_id() const noexcept { return owner_id_; }
    [[nodiscard]] std::uint64_t module_generation() const noexcept {
        return module_generation_;
    }
    [[nodiscard]] std::uint64_t capture_epoch() const noexcept { return capture_epoch_; }
    [[nodiscard]] std::uintptr_t mapped_base() const noexcept { return mapped_base_; }
    [[nodiscard]] std::uint32_t mapped_size() const noexcept { return mapped_size_; }
    [[nodiscard]] const Digest256& build_digest() const noexcept { return build_digest_; }
    [[nodiscard]] const Digest256& package_digest() const noexcept { return package_digest_; }
    [[nodiscard]] const Digest256& surface_cohort_digest() const noexcept {
        return surface_cohort_digest_;
    }

private:
    friend class AdmissionOwner;
    friend class CaptureOwner;
#if defined(SUNRISE_MISSION_HEADER_CAPTURE_TESTING)
    friend class AdmissionTestAccess;
#endif
    AdmissionKind kind_{AdmissionKind::invalid};
    std::uint64_t owner_id_{};
    std::uint64_t module_generation_{};
    std::uint64_t capture_epoch_{};
    std::uintptr_t mapped_base_{};
    std::uint32_t mapped_size_{};
    Digest256 build_digest_{};
    Digest256 package_digest_{};
    Digest256 surface_cohort_digest_{};
    Digest256 seal_{};
};

struct AdmissionOutcome final {
    AdmissionResult result{AdmissionResult::invalid_arguments};
    AdmissionToken token{};
};

class AdmissionOwner final {
public:
    AdmissionOwner() noexcept;
    AdmissionOwner(const AdmissionOwner&) = delete;
    AdmissionOwner& operator=(const AdmissionOwner&) = delete;

    /** Hashes the actual main-module path and package, then reads the mapped main module. */
    [[nodiscard]] AdmissionOutcome admit_live() noexcept;
    [[nodiscard]] bool verifies(const AdmissionToken& token) const noexcept;
    void invalidate() noexcept;
    [[nodiscard]] std::uint64_t owner_id() const noexcept { return owner_id_; }

private:
    friend class CaptureOwner;
#if defined(SUNRISE_MISSION_HEADER_CAPTURE_TESTING)
    friend class AdmissionTestAccess;
#endif
    [[nodiscard]] AdmissionOutcome admit_mapped(AdmissionKind kind,
                                                const void* mappedBase,
                                                std::uint32_t mappedBytes,
                                                bool requireMainModule) noexcept;
    [[nodiscard]] Digest256 seal_token(const AdmissionToken& token) const noexcept;
    Digest256 secret_{};
    std::uint64_t owner_id_{};
    std::atomic<std::uint64_t> module_generation_{1U};
    std::atomic<std::uint64_t> next_capture_epoch_{1U};
    bool entropy_ready_{};
};

#if defined(SUNRISE_MISSION_HEADER_CAPTURE_TESTING)
class AdmissionTestAccess final {
public:
    [[nodiscard]] static AdmissionOutcome admit_protected_image(AdmissionOwner& owner,
                                                                const void* mappedBase,
                                                                std::uint32_t mappedBytes) noexcept;
};
#endif

struct OwnerGeneration final {
    std::uint64_t owner_cookie{};
    std::uint64_t generation{};
    friend constexpr bool operator==(OwnerGeneration, OwnerGeneration) noexcept = default;
};

struct GenerationSnapshot final {
    OwnerGeneration session{};
    OwnerGeneration activity{};
    OwnerGeneration world{};
    OwnerGeneration profile{};
    OwnerGeneration queue{};
    OwnerGeneration latch{};
    std::uint32_t activity_index{};
    std::uint32_t activity_definition_hash{};
    std::uint32_t activity_package_tag{};
    friend constexpr bool operator==(const GenerationSnapshot&,
                                     const GenerationSnapshot&) noexcept = default;
};

using GenerationReadFunction = bool (*)(void* context, GenerationSnapshot& output) noexcept;

struct GenerationSource final {
    GenerationReadFunction read{};
    void* context{};
};

[[nodiscard]] bool valid_exact_generations(const GenerationSnapshot& snapshot) noexcept;

struct PayloadProjection final {
    LocalizedPair title{};
    std::uint8_t presentation_category{};
    std::uint32_t icon_theme_dword{};
    std::uint32_t byte_count{};
    DigestAlgorithm digest_algorithm{DigestAlgorithm::sha256};
    Digest256 visual_tuple_digest{};
    Digest256 full_payload_digest{};
    bool exact_omega_identity{};
    friend constexpr bool operator==(const PayloadProjection&,
                                     const PayloadProjection&) noexcept = default;
};

enum class BoundedCaptureResult : std::uint8_t {
    complete,
    null_pointer,
    unreadable,
    wrong_command,
    identity_mismatch,
    invalid_arguments,
};

/** Copies exactly 32 bytes and derives every projection/digest from that one copy. */
[[nodiscard]] BoundedCaptureResult capture_command5_payload(const void* payload,
                                                            PayloadProjection& output) noexcept;

struct QueueProjection final {
    std::uint64_t queued_count{};
    std::int32_t dispatch_definition_handle{-1};
    std::int32_t current_sequence{-1};
    std::uint8_t current_retire_or_supersede{};
    std::uint8_t current_active{};
    std::uint16_t current_command{};
    std::int32_t next_sequence{};
    std::uint32_t authored_priority{};
    std::uint32_t byte_count{};
    DigestAlgorithm digest_algorithm{DigestAlgorithm::sha256};
    Digest256 exact_queue_digest{};
    PayloadProjection current_payload{};
    bool current_payload_valid{};
    friend constexpr bool operator==(const QueueProjection&, const QueueProjection&) noexcept =
        default;
};

/** Copies the exact bounded queue projection before decoding any native field. */
[[nodiscard]] BoundedCaptureResult capture_queue_projection(const void* queue,
                                                            QueueProjection& output) noexcept;

struct LatchSnapshot final {
    std::int32_t lifecycle_state{};
    std::uint8_t arm{};
    std::uint8_t pending{};
    std::uint8_t ready{};
    std::uint32_t adjacent_local_presentation_state{};
    Digest256 digest{};
    friend constexpr bool operator==(const LatchSnapshot&, const LatchSnapshot&) noexcept =
        default;
};

[[nodiscard]] LatchSnapshot capture_latch_scalars(std::int32_t lifecycle,
                                                  std::uint8_t arm,
                                                  std::uint8_t pending,
                                                  std::uint8_t ready,
                                                  std::uint32_t adjacent) noexcept;
[[nodiscard]] bool valid_latch_snapshot(const LatchSnapshot& snapshot) noexcept;

enum class ProducerAttemptOutcome : std::uint8_t {
    not_attempted,
    queued,
    producer_early_return,
    queue_rejected_definition_unresolved,
    queue_rejected_full,
    queue_outcome_unresolved,
};

struct StateMachineExpectation final {
    LatchSnapshot post{};
    ProducerAttemptOutcome attempt_outcome{ProducerAttemptOutcome::not_attempted};
    bool valid_input{};
    bool armed{};
    bool attempted{};
    bool pending_cleared_after_attempt{};
};

[[nodiscard]] StateMachineExpectation expected_state_machine_step(
    const LatchSnapshot& pre,
    ProducerAttemptOutcome observedAttemptOutcome) noexcept;
[[nodiscard]] LatchSnapshot expected_reset_post(const LatchSnapshot& pre) noexcept;
[[nodiscard]] LatchSnapshot expected_rearm_post(const LatchSnapshot& pre) noexcept;
[[nodiscard]] LatchSnapshot expected_ready_post(const LatchSnapshot& pre) noexcept;

enum class QueueDecisionMarker : std::uint8_t {
    none,
    reject_unresolved_at_131A64A,
    reject_full_at_131A658,
    accepted_commit_at_131A7CE,
};

enum class QueueInsertOutcome : std::uint8_t {
    accepted,
    rejected_definition_unresolved,
    rejected_full,
    raw_partial_no_decision_marker,
    incoherent,
};

[[nodiscard]] QueueInsertOutcome classify_queue_insert(
    const QueueProjection& pre,
    const QueueProjection& post,
    QueueDecisionMarker marker,
    std::uintptr_t insertedRecordIdentity) noexcept;

enum class CapturePhase : std::uint8_t {
    call_boundary,
    producer_entry,
    producer_stage,
    producer_payload_anchor,
    producer_return,
    latch_gate,
    latch_pending_clear,
    latch_reset,
    latch_rearm,
    latch_ready,
    queue_insert,
    lazy_dispatch_selection,
    queue_tick_promote,
    queue_tick_supersede,
    queue_tick_retire,
    cui_wrapper,
    cui_getter,
    category_localization,
    visible_first_frame,
    visible_last_frame,
};

struct ProvenanceStamp final {
    std::uint64_t admission_owner_id{};
    std::uint64_t module_generation{};
    std::uint64_t capture_epoch{};
    Digest256 build_digest{};
    Digest256 package_digest{};
    Digest256 surface_cohort_digest{};
};

struct RecordHeader final {
    ProvenanceStamp provenance{};
    GenerationSnapshot entry_generations{};
    GenerationSnapshot phase_generations{};
    std::uint64_t record_sequence{};
    std::uint64_t call_id{};
    std::uint64_t parent_call_id{};
    std::uint64_t monotonic_tick{};
    std::uint64_t monotonic_frequency{};
    std::uint64_t clock_domain_id{};
    std::uint32_t thread_id{};
    NativeSurface surface{NativeSurface::command5_producer};
    CapturePhase phase{CapturePhase::call_boundary};
    bool generations_exact_at_phase{};
};

enum class ProducerStage : std::uint8_t {
    entry,
    activity_manager,
    investment_ready,
    activity_index,
    eligibility,
    client_activity_row,
    display_row,
    category,
    title,
    payload_anchor,
    queue_insert,
    returned,
};

enum class ProducerDisposition : std::uint8_t {
    in_progress,
    early_null_activity_manager,
    early_investment_not_ready,
    early_invalid_activity_index,
    early_profile_or_activity_ineligible,
    early_missing_client_activity_row,
    early_missing_display_row,
    early_other_eligibility,
    queue_attempted,
};

struct ProducerStageFacts final {
    std::uintptr_t activity_manager_identity{};
    std::uintptr_t client_activity_row_identity{};
    std::uintptr_t display_row_identity{};
    std::uint32_t activity_index{};
    std::uint32_t definition_hash{};
    std::uint8_t activity_type{};
    std::uint8_t category_source{};
    std::uint8_t presentation_category{};
    LocalizedPair title{};
    bool investment_ready{};
    bool eligible{};
};

struct ProducerRecord final {
    RecordHeader header{};
    ProducerStage stage{ProducerStage::entry};
    ProducerDisposition disposition{ProducerDisposition::in_progress};
    ProducerStageFacts facts{};
    PayloadProjection payload{};
    bool payload_present{};
    bool trace_prefix_complete{};
    Digest256 integrity_seal{};
};

struct QueueInsertRecord final {
    RecordHeader header{};
    QueueProjection pre{};
    QueueProjection post{};
    PayloadProjection argument_payload{};
    QueueDecisionMarker marker{QueueDecisionMarker::none};
    QueueInsertOutcome outcome{QueueInsertOutcome::raw_partial_no_decision_marker};
    std::uintptr_t queue_identity{};
    std::uintptr_t inserted_record_identity{};
    Digest256 integrity_seal{};
};

struct LazyDispatchRecord final {
    RecordHeader header{};
    std::int32_t pre_handle{-1};
    std::int32_t selected_handle{-1};
    std::uint32_t selected_tag{};
    std::uint32_t selected_class{};
    std::uint32_t selected_package{};
    std::uint32_t enumerated_count{};
    Digest256 enumerated_resource_digest{};
    Digest256 authored_row_digest{};
    bool selected_from_enumeration{};
    Digest256 integrity_seal{};
};

enum class QueueTickAction : std::uint8_t { none, promoted, superseded, retired };

struct QueueTickRecord final {
    RecordHeader header{};
    QueueProjection pre{};
    QueueProjection post{};
    QueueTickAction action{QueueTickAction::none};
    std::int32_t affected_sequence{-1};
    std::uint64_t duration_ticks{};
    std::uint64_t elapsed_ticks{};
    bool exact_action_marker_observed{};
    Digest256 integrity_seal{};
};

enum class CuiValueKind : std::uint8_t { category, title, icon_theme, visual_tuple };

struct CuiRecord final {
    RecordHeader header{};
    CuiValueKind value_kind{CuiValueKind::category};
    CuiRegistrationIdentity registration{};
    std::int32_t current_sequence{-1};
    std::uint16_t current_command{};
    PayloadProjection current_payload{};
    LocalizedPair localized_value{};
    std::uint64_t scalar_value{};
    Digest256 value_digest{};
    bool wrapper_observed{};
    bool getter_observed{};
    bool command5_exact_join{};
    bool command8_neighbor_only{};
    Digest256 integrity_seal{};
};

struct LocalizationRecord final {
    RecordHeader header{};
    std::int32_t current_sequence{-1};
    std::uint16_t current_command{};
    std::uint8_t presentation_category{};
    std::uint32_t instantiated_resource_tag{};
    std::uint32_t instantiated_node_id{};
    std::uint32_t property_id{};
    std::uint16_t bank_map_index{};
    std::uint16_t bank_ordinal{};
    LocalizedPair resolved_pair{};
    Digest256 payload_digest{};
    Digest256 resolved_utf8_digest{};
    EvidenceGrade resulting_edge_grade{EvidenceGrade::inferred};
    Digest256 integrity_seal{};
};

enum class VisibleFrameEdge : std::uint8_t { first_visible, last_visible };

struct ClockCalibration final {
    std::uint64_t admission_owner_id{};
    std::uint64_t capture_epoch{};
    std::uint64_t clock_domain_id{};
    std::uint64_t qpc_tick{};
    std::uint64_t video_tick{};
    std::uint64_t video_timescale{};
    Digest256 seal{};
};

struct VisibleFrameRecord final {
    RecordHeader header{};
    VisibleFrameEdge edge{VisibleFrameEdge::first_visible};
    std::uint64_t video_frame_index{};
    std::uint64_t video_tick{};
    std::uint64_t video_timescale{};
    std::int32_t current_sequence{-1};
    Digest256 payload_digest{};
    Digest256 frame_digest{};
    bool mission_label_visible{};
    bool omega_title_visible{};
    bool synchronized_clock{};
    Digest256 integrity_seal{};
};

enum class LatchBoundaryEvent : std::uint8_t {
    state_gate,
    pending_clear,
    reset_zero,
    rearm,
    ready_setter,
    direct_show_nonzero,
};

struct LatchRecord final {
    RecordHeader header{};
    LatchBoundaryEvent event{LatchBoundaryEvent::state_gate};
    LatchSnapshot pre{};
    LatchSnapshot post{};
    ProducerAttemptOutcome attempt_outcome{ProducerAttemptOutcome::not_attempted};
    int wrapper_argument{};
    bool exact_pending_clear_marker{};
    Digest256 integrity_seal{};
};

enum class ObserverFault : std::uint8_t { none, pre_observer, post_observer };

struct CallBoundaryRecord final {
    RecordHeader header{};
    GenerationSnapshot exit_generations{};
    std::uint64_t exit_tick{};
    std::uint32_t emitted_records{};
    std::uint32_t lost_records{};
    ObserverFault observer_fault{ObserverFault::none};
    bool original_called_once{};
    bool generations_exact_at_exit{};
    bool tls_stack_overflow{};
    Digest256 integrity_seal{};
};

enum class QueuePushResult : std::uint8_t {
    enqueued,
    busy,
    full,
    sequence_exhausted,
};
enum class QueuePopResult : std::uint8_t { success, empty, busy };

struct QueueCounters final {
    std::uint64_t enqueued{};
    std::uint64_t popped{};
    std::uint64_t dropped_busy{};
    std::uint64_t dropped_full{};
    std::uint64_t dropped_sequence_exhausted{};
};

class CaptureOwner;

template <typename Record, std::size_t Capacity>
class FixedRecordQueue final {
    static_assert(Capacity > 0U);
    static_assert(std::is_trivially_copyable_v<Record>);

public:
    [[nodiscard]] QueuePopResult try_pop(Record& output) noexcept {
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

    [[nodiscard]] QueueCounters counters() const noexcept {
        return QueueCounters{enqueued_.load(std::memory_order_relaxed),
                             popped_.load(std::memory_order_relaxed),
                             dropped_busy_.load(std::memory_order_relaxed),
                             dropped_full_.load(std::memory_order_relaxed),
                             dropped_sequence_exhausted_.load(std::memory_order_relaxed)};
    }

private:
    friend class CaptureOwner;
#if defined(SUNRISE_MISSION_HEADER_CAPTURE_TESTING)
    friend class QueueTestAccess;
#endif
    [[nodiscard]] QueuePushResult push_canonical(const Record& source) noexcept {
        if (lock_.test_and_set(std::memory_order_acquire)) {
            dropped_busy_.fetch_add(1U, std::memory_order_relaxed);
            return QueuePushResult::busy;
        }
        if (count_ == Capacity) {
            dropped_full_.fetch_add(1U, std::memory_order_relaxed);
            lock_.clear(std::memory_order_release);
            return QueuePushResult::full;
        }
        Record canonical{};
        canonical = source;
        records_[tail_] = canonical;
        tail_ = (tail_ + 1U) % Capacity;
        ++count_;
        enqueued_.fetch_add(1U, std::memory_order_relaxed);
        lock_.clear(std::memory_order_release);
        return QueuePushResult::enqueued;
    }

    std::array<Record, Capacity> records_{};
    std::atomic_flag lock_ = ATOMIC_FLAG_INIT;
    std::size_t head_{};
    std::size_t tail_{};
    std::size_t count_{};
    std::atomic<std::uint64_t> enqueued_{};
    std::atomic<std::uint64_t> popped_{};
    std::atomic<std::uint64_t> dropped_busy_{};
    std::atomic<std::uint64_t> dropped_full_{};
    std::atomic<std::uint64_t> dropped_sequence_exhausted_{};
};

enum class LifecyclePhase : std::uint8_t { detached, active, quiescing, retained_failure };
enum class ProtectedDetachDisposition : std::uint8_t { removed, deferred, failed };

using ProtectedDetachFunction = ProtectedDetachDisposition (*)(void* context) noexcept;

struct LifecycleSnapshot final {
    LifecyclePhase phase{LifecyclePhase::detached};
    std::uint64_t epoch{};
    std::uint32_t full_calls{};
    std::uint32_t observation_calls{};
    bool observation_ingress_open{};
    bool state_retained{};
};

class AtomicEpochIngress final {
public:
    class Scope final {
    public:
        explicit Scope(AtomicEpochIngress& ingress) noexcept;
        ~Scope();
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;
        [[nodiscard]] bool admitted() const noexcept { return admitted_; }

    private:
        AtomicEpochIngress& ingress_;
        bool admitted_{};
    };

    void open() noexcept;
    void close() noexcept;
    [[nodiscard]] bool idle() const noexcept;
    [[nodiscard]] bool open_for_admission() const noexcept;
    [[nodiscard]] std::uint32_t active_calls() const noexcept;

private:
    static constexpr std::uint64_t kClosedBit = std::uint64_t{1U} << 63U;
    std::atomic<std::uint64_t> state_{kClosedBit};
};

struct DefaultTelemetry final {
    ProvenanceStamp provenance{};
    GenerationSnapshot entry_generations{};
    GenerationSnapshot phase_generations{};
    std::uint64_t record_sequence{};
    std::uint64_t call_id{};
    std::uint64_t parent_call_id{};
    std::uint64_t monotonic_tick{};
    std::uint64_t monotonic_frequency{};
    std::uint64_t clock_domain_id{};
    std::uint32_t thread_id{};
    CapturePhase phase{CapturePhase::call_boundary};
    Digest256 payload_or_state_digest{};
    std::int32_t native_sequence{-1};
    std::uint16_t command{};
    std::uint8_t outcome{};
    bool generations_exact{};
};

static_assert(std::is_trivially_copyable_v<DefaultTelemetry>);

[[nodiscard]] bool default_telemetry(const ProducerRecord& record,
                                     DefaultTelemetry& output) noexcept;
[[nodiscard]] bool default_telemetry(const QueueInsertRecord& record,
                                     DefaultTelemetry& output) noexcept;
[[nodiscard]] bool default_telemetry(const LazyDispatchRecord& record,
                                     DefaultTelemetry& output) noexcept;
[[nodiscard]] bool default_telemetry(const QueueTickRecord& record,
                                     DefaultTelemetry& output) noexcept;
[[nodiscard]] bool default_telemetry(const CuiRecord& record,
                                     DefaultTelemetry& output) noexcept;
[[nodiscard]] bool default_telemetry(const LocalizationRecord& record,
                                     DefaultTelemetry& output) noexcept;
[[nodiscard]] bool default_telemetry(const VisibleFrameRecord& record,
                                     DefaultTelemetry& output) noexcept;
[[nodiscard]] bool default_telemetry(const LatchRecord& record,
                                     DefaultTelemetry& output) noexcept;
[[nodiscard]] bool default_telemetry(const CallBoundaryRecord& record,
                                     DefaultTelemetry& output) noexcept;

class CaptureOwner final {
public:
    CaptureOwner(AdmissionOwner& admission,
                 AdmissionToken token,
                 GenerationSource generations) noexcept;
    CaptureOwner(const CaptureOwner&) = delete;
    CaptureOwner& operator=(const CaptureOwner&) = delete;

    [[nodiscard]] bool activate() noexcept;
    void begin_quiesce() noexcept;
    [[nodiscard]] ProtectedDetachDisposition protected_detach(ProtectedDetachFunction detach,
                                                              void* context) noexcept;
    [[nodiscard]] LifecycleSnapshot lifecycle_snapshot() const noexcept;

    [[nodiscard]] bool observe_producer_stage(ProducerStage stage,
                                              ProducerDisposition disposition,
                                              const ProducerStageFacts& facts) noexcept;
    [[nodiscard]] bool observe_producer_payload(const void* payload) noexcept;
    [[nodiscard]] bool observe_queue_decision(QueueDecisionMarker marker,
                                              std::uintptr_t insertedRecordIdentity) noexcept;
    [[nodiscard]] bool observe_queue_insert_pre(const void* queue,
                                                const void* argumentPayload) noexcept;
    [[nodiscard]] bool emit_queue_insert(const void* queue) noexcept;
    [[nodiscard]] bool emit_lazy_dispatch(std::int32_t preHandle,
                                          std::int32_t selectedHandle,
                                          std::uint32_t selectedTag,
                                          std::uint32_t selectedClass,
                                          std::uint32_t selectedPackage,
                                          std::span<const std::byte> enumeratedResources,
                                          std::span<const std::byte> authoredRow,
                                          bool selectedFromEnumeration) noexcept;
    [[nodiscard]] bool observe_queue_tick_pre(const void* queue) noexcept;
    [[nodiscard]] bool emit_queue_tick(const void* queue,
                                       QueueTickAction action,
                                       std::int32_t affectedSequence,
                                       std::uint64_t durationTicks,
                                       std::uint64_t elapsedTicks,
                                       bool exactActionMarker) noexcept;
    [[nodiscard]] bool emit_cui(CuiValueKind kind,
                                bool wrapperObserved,
                                bool getterObserved,
                                const void* queue,
                                LocalizedPair localizedValue,
                                std::uint64_t scalarValue,
                                std::span<const std::byte> valueBytes) noexcept;
    [[nodiscard]] bool emit_localization(const void* queue,
                                         std::uint32_t resourceTag,
                                         std::uint32_t nodeId,
                                         std::uint32_t propertyId,
                                         std::uint16_t bankMapIndex,
                                         std::uint16_t bankOrdinal,
                                         LocalizedPair resolvedPair,
                                         std::span<const std::byte> resolvedUtf8) noexcept;
    [[nodiscard]] bool emit_latch(LatchBoundaryEvent event,
                                  const LatchSnapshot& pre,
                                  const LatchSnapshot& post,
                                  ProducerAttemptOutcome outcome,
                                  int wrapperArgument,
                                  bool exactPendingClearMarker) noexcept;
    [[nodiscard]] ClockCalibration calibrate_video_clock(std::uint64_t videoTick,
                                                         std::uint64_t videoTimescale) noexcept;
    [[nodiscard]] bool emit_visible_frame(const ClockCalibration& calibration,
                                          VisibleFrameEdge edge,
                                          std::uint64_t videoFrameIndex,
                                          std::uint64_t videoTick,
                                          std::int32_t currentSequence,
                                          const Digest256& payloadDigest,
                                          const Digest256& frameDigest,
                                          bool missionVisible,
                                          bool omegaVisible) noexcept;

    [[nodiscard]] QueuePopResult try_pop(ProducerRecord& output) noexcept;
    [[nodiscard]] QueuePopResult try_pop(QueueInsertRecord& output) noexcept;
    [[nodiscard]] QueuePopResult try_pop(LazyDispatchRecord& output) noexcept;
    [[nodiscard]] QueuePopResult try_pop(QueueTickRecord& output) noexcept;
    [[nodiscard]] QueuePopResult try_pop(CuiRecord& output) noexcept;
    [[nodiscard]] QueuePopResult try_pop(LocalizationRecord& output) noexcept;
    [[nodiscard]] QueuePopResult try_pop(VisibleFrameRecord& output) noexcept;
    [[nodiscard]] QueuePopResult try_pop(LatchRecord& output) noexcept;
    [[nodiscard]] QueuePopResult try_pop(CallBoundaryRecord& output) noexcept;

    template <typename Original, typename PreObserver, typename PostObserver, typename... Args>
    void forward_void_original_once(NativeSurface surface,
                                    Original original,
                                    PreObserver&& preObserver,
                                    PostObserver&& postObserver,
                                    Args&&... args) noexcept {
        static_assert(std::is_void_v<std::invoke_result_t<Original, Args...>>);
        static_assert(std::is_nothrow_invocable_v<Original, Args...>);
        hooking::CallGate::Scope fullCall{full_call_gate_};
        AtomicEpochIngress::Scope admittedCall{observation_ingress_};
        const bool observing = fullCall.accepts_side_effects() && admittedCall.admitted()
                               && begin_call(surface);
        ObserverFault fault = ObserverFault::none;
        if (observing) {
            try {
                std::invoke(std::forward<PreObserver>(preObserver), *this);
            } catch (...) {
                fault = ObserverFault::pre_observer;
            }
        }
        std::invoke(original, std::forward<Args>(args)...);
        if (observing) {
            try {
                std::invoke(std::forward<PostObserver>(postObserver), *this);
            } catch (...) {
                if (fault == ObserverFault::none) {
                    fault = ObserverFault::post_observer;
                }
            }
            end_call(fault, true);
        }
    }

    template <typename Original, typename PreObserver, typename PostObserver, typename... Args>
    std::invoke_result_t<Original, Args...>
    forward_value_original_once(NativeSurface surface,
                                Original original,
                                PreObserver&& preObserver,
                                PostObserver&& postObserver,
                                Args&&... args) noexcept {
        using Result = std::invoke_result_t<Original, Args...>;
        static_assert(!std::is_void_v<Result>);
        static_assert(std::is_nothrow_invocable_v<Original, Args...>);
        hooking::CallGate::Scope fullCall{full_call_gate_};
        AtomicEpochIngress::Scope admittedCall{observation_ingress_};
        const bool observing = fullCall.accepts_side_effects() && admittedCall.admitted()
                               && begin_call(surface);
        ObserverFault fault = ObserverFault::none;
        if (observing) {
            try {
                std::invoke(std::forward<PreObserver>(preObserver), *this);
            } catch (...) {
                fault = ObserverFault::pre_observer;
            }
        }
        Result result = std::invoke(original, std::forward<Args>(args)...);
        if (observing) {
            try {
                std::invoke(std::forward<PostObserver>(postObserver), *this);
            } catch (...) {
                if (fault == ObserverFault::none) {
                    fault = ObserverFault::post_observer;
                }
            }
            end_call(fault, true);
        }
        return result;
    }

private:
#if defined(SUNRISE_MISSION_HEADER_CAPTURE_TESTING)
    friend class CaptureTestAccess;
#endif
    [[nodiscard]] bool begin_call(NativeSurface surface) noexcept;
    void end_call(ObserverFault fault, bool originalCalledOnce) noexcept;
    [[nodiscard]] RecordHeader make_header(CapturePhase phase) noexcept;
    [[nodiscard]] bool current_call_available() const noexcept;
    [[nodiscard]] Digest256 seal_bytes(std::span<const std::byte> bytes) const noexcept;
    [[nodiscard]] bool verifies_calibration(const ClockCalibration& calibration) const noexcept;

    AdmissionOwner& admission_;
    AdmissionToken token_{};
    GenerationSource generations_{};
    hooking::CallGate full_call_gate_{};
    AtomicEpochIngress observation_ingress_{};
    std::atomic<LifecyclePhase> phase_{LifecyclePhase::detached};
    std::atomic<std::uint64_t> lifecycle_epoch_{};
    std::atomic<std::uint64_t> next_call_id_{};
    std::atomic<std::uint64_t> next_record_sequence_{};
    std::atomic<std::uint64_t> next_clock_domain_id_{};
    bool allow_test_token_{};

    FixedRecordQueue<ProducerRecord, 64U> producer_records_{};
    FixedRecordQueue<QueueInsertRecord, 64U> queue_insert_records_{};
    FixedRecordQueue<LazyDispatchRecord, 32U> lazy_dispatch_records_{};
    FixedRecordQueue<QueueTickRecord, 64U> queue_tick_records_{};
    FixedRecordQueue<CuiRecord, 128U> cui_records_{};
    FixedRecordQueue<LocalizationRecord, 32U> localization_records_{};
    FixedRecordQueue<VisibleFrameRecord, 32U> visible_frame_records_{};
    FixedRecordQueue<LatchRecord, 64U> latch_records_{};
    FixedRecordQueue<CallBoundaryRecord, 128U> call_records_{};
};

#if defined(SUNRISE_MISSION_HEADER_CAPTURE_TESTING)
class CaptureTestAccess final {
public:
    static void allow_test_admission(CaptureOwner& owner) noexcept;
};

class QueueTestAccess final {
public:
    template <typename Record, std::size_t Capacity>
    static bool lock(FixedRecordQueue<Record, Capacity>& queue) noexcept {
        return !queue.lock_.test_and_set(std::memory_order_acquire);
    }
    template <typename Record, std::size_t Capacity>
    static void unlock(FixedRecordQueue<Record, Capacity>& queue) noexcept {
        queue.lock_.clear(std::memory_order_release);
    }
};
#endif

#if defined(_MSC_VER)
using ProducerOriginal = void(__fastcall*)() noexcept;
using QueueInsertOriginal =
    void(__fastcall*)(void* queue, std::int32_t command, const void* payload) noexcept;
using QueueTickOriginal = void(__fastcall*)(void* queue) noexcept;
using ResetDirectShowOriginal = void(__fastcall*)(std::int32_t mode) noexcept;
using LatchOriginal = void(__fastcall*)() noexcept;
using CategoryGetterOriginal = void(__fastcall*)(void* context, std::uint8_t* output) noexcept;
using TitleGetterOriginal = void(__fastcall*)(void* context, LocalizedPair* output) noexcept;
using IconGetterOriginal = void(__fastcall*)(void* context, std::uint64_t* output) noexcept;
using VisualGetterOriginal = void(__fastcall*)(void* context, void* output16) noexcept;
#else
using ProducerOriginal = void (*)() noexcept;
using QueueInsertOriginal = void (*)(void*, std::int32_t, const void*) noexcept;
using QueueTickOriginal = void (*)(void*) noexcept;
using ResetDirectShowOriginal = void (*)(std::int32_t) noexcept;
using LatchOriginal = void (*)() noexcept;
using CategoryGetterOriginal = void (*)(void*, std::uint8_t*) noexcept;
using TitleGetterOriginal = void (*)(void*, LocalizedPair*) noexcept;
using IconGetterOriginal = void (*)(void*, std::uint64_t*) noexcept;
using VisualGetterOriginal = void (*)(void*, void*) noexcept;
#endif

} // namespace sunrise::client::hooks::bootflow::opening_authority::mission_header_intro_capture
