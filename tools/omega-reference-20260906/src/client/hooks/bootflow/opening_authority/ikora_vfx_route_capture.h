#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>

namespace sunrise::client::hooks::bootflow::opening_authority::ikora_vfx_route {

// Deliberately inert, source-only evidence schema. It is not registered in any build graph or
// shared lifecycle owner and exposes no native-state treatment surface.
inline constexpr bool kIntegrationReady = false;
inline constexpr bool kObservationOnly = true;
inline constexpr bool kOwnsNativeDetour = false;
inline constexpr bool kProvidesVfxWriter = false;
inline constexpr bool kProvidesAttachmentRebind = false;
inline constexpr bool kProvidesCandidateSubstitution = false;
inline constexpr bool kProvidesPoseSwap = false;
inline constexpr bool kProvidesSuppression = false;
inline constexpr bool kCarrierFallbackAllowed = false;
inline constexpr bool kFrozenSuccessorRouteObserved = false;
inline constexpr bool kFrozenSuccessorEmissionObserved = false;
inline constexpr bool kCaptureBeforeEnableOraclePassed = false;
inline constexpr bool kSuccessorEmissionEnabled = false;

using Sha256 = std::array<std::byte, 32U>;

inline constexpr wchar_t kPinnedPackedRuntimePath[] = L"D:\\Destiny3\\destiny2.exe";
inline constexpr std::uint64_t kPinnedPackedRuntimeBytes = 122'984'224U;
inline constexpr Sha256 kPinnedPackedRuntimeSha256{
    std::byte{0x81}, std::byte{0x96}, std::byte{0x43}, std::byte{0x80}, std::byte{0x66},
    std::byte{0x4E}, std::byte{0x7F}, std::byte{0xCE}, std::byte{0xE3}, std::byte{0xC6},
    std::byte{0x20}, std::byte{0x08}, std::byte{0x5A}, std::byte{0x15}, std::byte{0x7F},
    std::byte{0xDE}, std::byte{0xAF}, std::byte{0x91}, std::byte{0xFE}, std::byte{0xFA},
    std::byte{0xCF}, std::byte{0x72}, std::byte{0x14}, std::byte{0x90}, std::byte{0x78},
    std::byte{0x20}, std::byte{0xF1}, std::byte{0x88}, std::byte{0xBB}, std::byte{0xEB},
    std::byte{0x4C}, std::byte{0xED}};

// Separate unpacked RE provenance; never accepted as the installed packed-file identity.
inline constexpr wchar_t kPinnedUnpackedProvenancePath[] =
    L"D:\\Sunrise-work\\ghidra\\destiny2_unpacked.exe";
inline constexpr std::uint64_t kPinnedUnpackedProvenanceBytes = 145'091'072U;
inline constexpr Sha256 kPinnedUnpackedProvenanceSha256{
    std::byte{0x87}, std::byte{0x13}, std::byte{0xD1}, std::byte{0x5E}, std::byte{0x3D},
    std::byte{0x05}, std::byte{0xB2}, std::byte{0x6F}, std::byte{0x9E}, std::byte{0x25},
    std::byte{0x9E}, std::byte{0x02}, std::byte{0xB0}, std::byte{0xF2}, std::byte{0x9B},
    std::byte{0xC1}, std::byte{0xE0}, std::byte{0x00}, std::byte{0xE4}, std::byte{0xB0},
    std::byte{0xC6}, std::byte{0x2C}, std::byte{0xA2}, std::byte{0xCC}, std::byte{0x87},
    std::byte{0xC3}, std::byte{0x85}, std::byte{0x97}, std::byte{0x18}, std::byte{0x6C},
    std::byte{0xC3}, std::byte{0xBD}};

inline constexpr std::uint16_t kPinnedMachine = 0x8664U;
inline constexpr std::uint16_t kPinnedSectionCount = 11U;
inline constexpr std::uint32_t kPinnedPeTimestamp = 0x5F43138BU;
inline constexpr std::uintptr_t kPinnedPreferredImageBase = 0x140000000ULL;
inline constexpr std::uint32_t kPinnedSizeOfImage = 0x08A5EA00U;
inline constexpr std::uint32_t kPinnedSizeOfHeaders = 0x600U;
inline constexpr std::uint32_t kPinnedEntryRva = 0x0187CDD8U;

enum class NativeSurface : std::uint8_t {
    actor_terminal_invalidation,
    transition_wrapper,
    scene_actor_scheduler,
    entity_factory,
    component_tail_dispatch,
    cache_rebuild,
    descriptor_walk,
    source_writer,
    effect_create_one,
    effect_object_initializer,
    effect_group_create,
    effect_explicit_destroy,
    effect_priority_eviction_destroy,
    effect_update_and_expiry,
    kind2_runtime_resolver,
    provider_selection,
    candidate_socket_resolver,
    pose_socket_dispatch,
    downstream_bounds_oracle,
    count,
};

inline constexpr std::size_t kNativeSurfaceCount = static_cast<std::size_t>(NativeSurface::count);

struct NativeTarget final {
    NativeSurface surface{NativeSurface::count};
    std::uint32_t rva{};
    std::span<const std::byte> prefix{};
    std::uint32_t function_bytes{};
    Sha256 function_sha256{};
};

struct DirectEdge final {
    std::uint32_t callsite_rva{};
    std::uint32_t target_rva{};
    std::uint8_t opcode{};
};

struct MappedWindow final {
    std::uint32_t rva{};
    std::span<const std::byte> bytes{};
};

enum class CapturePayload : std::uint8_t {
    none,
    source_route,
    actor_generation_invalidation,
    effect_create_out_pair,
    effect_full_handle_terminal,
    effect_inlined_expiry,
};

enum class CaptureTiming : std::uint8_t {
    none,
    pre_native_entry,
    entry_and_post_original,
    within_original_terminal_path,
};

struct CaptureBoundaryDescriptor final {
    NativeSurface surface{NativeSurface::count};
    std::uint32_t boundary_rva{};
    CapturePayload payload{CapturePayload::none};
    CaptureTiming timing{CaptureTiming::none};
    std::array<std::uint32_t, 2U> exact_callsite_rvas{};
    std::uint8_t exact_callsite_count{};
    std::array<std::uint32_t, 2U> terminal_path_rvas{};
    std::uint8_t terminal_path_count{};
};

[[nodiscard]] NativeTarget native_target(NativeSurface surface) noexcept;
[[nodiscard]] CaptureBoundaryDescriptor capture_boundary(NativeSurface surface) noexcept;
[[nodiscard]] std::span<const DirectEdge> direct_edge_manifest() noexcept;
[[nodiscard]] MappedWindow candidate_result_bank_copy_window() noexcept;

class RuntimeCohortToken final {
public:
    [[nodiscard]] constexpr bool valid() const noexcept { return cohort_id_ != 0U; }
    [[nodiscard]] constexpr std::uint64_t cohort_id() const noexcept { return cohort_id_; }
    friend constexpr bool operator==(RuntimeCohortToken, RuntimeCohortToken) noexcept = default;

private:
    std::uint64_t cohort_id_{};
    friend struct RuntimeAdmissionIssuer;
};

struct RuntimeImageView final {
    std::span<const std::byte> packed_file{};
    const std::byte* mapped_image{};
    std::size_t mapped_image_bytes{};
};

enum class RuntimeAdmissionResult : std::uint8_t {
    admitted,
    unsupported_platform,
    packed_size_mismatch,
    packed_hash_mismatch,
    packed_pe_mismatch,
    mapped_bounds_mismatch,
    mapped_pe_mismatch,
    mapped_section_mismatch,
    target_not_in_executable_section,
    page_not_committed_read_execute,
    function_prefix_mismatch,
    function_hash_mismatch,
    callsite_mismatch,
    result_copy_window_mismatch,
};

struct RuntimeAdmissionDiagnostic final {
    RuntimeAdmissionResult result{RuntimeAdmissionResult::mapped_bounds_mismatch};
    NativeSurface surface{NativeSurface::count};
    std::uint32_t rva{};
};

// Trust is derived from actual bytes/pages, not caller-asserted booleans or edge masks.
[[nodiscard]] RuntimeAdmissionDiagnostic validate_runtime_admission(
    const RuntimeImageView& image,
    RuntimeCohortToken& issued_token) noexcept;

inline constexpr std::uint32_t kPresentationMachine = 0x80FCCE87U;
inline constexpr std::uint32_t kPresentationClass = 0x80809C36U;
inline constexpr std::uint8_t kOutputSlot = 1U;
inline constexpr std::uint8_t kRuntimeSourceRow = 1U;
inline constexpr std::uint32_t kPurpleSelection = 0x98DA9A6BU;
inline constexpr std::uint32_t kSentinelF00dFeed = 0xF00DFEEDU;
inline constexpr std::uint32_t kSentinelFeedF00d = 0xFEEDF00DU;
inline constexpr std::uint32_t kProviderComponentDefinition = 0x8161FB60U;
inline constexpr std::uint32_t kProviderRuntimeKind = 0x80806A22U;
inline constexpr std::uint32_t kSharedEntityDefinition = 0x80EC0F27U;

inline constexpr std::uint32_t kPcBankTransformRelativeOffset = 0x38U;
inline constexpr std::uint32_t kPcBankValidityRelativeOffset = 0x48U;
inline constexpr std::uint32_t kPcBankRuntimeRelativeOffset = 0x58U;
inline constexpr std::uint32_t kPcBankVectorHeaderBytes = 0x10U;
inline constexpr std::uint32_t kTransformBytes = 0x20U;
inline constexpr std::uint32_t kValidityBytes = 0x02U;
inline constexpr std::uint32_t kRuntimeRowBytes = 0x18U;

inline constexpr std::uint32_t kProviderPoseDispatchOffset = 0x50U;
inline constexpr std::uint32_t kProviderPoseObjectHandleOffset = 0x58U;
inline constexpr std::uint32_t kProviderPoseObjectRelativeOffset = 0x60U;
inline constexpr std::uint32_t kProviderFilterRelativeOffset = 0x70U;
inline constexpr std::uint32_t kDefinitionTableRelativeOffset = 0x58U;
inline constexpr std::uint32_t kSelectionTableRowsOffset = 0x40U;
inline constexpr std::uint32_t kCandidateTableRowsOffset = 0x10U;
inline constexpr std::uint32_t kCandidateBindingKeyOffset = 0x04U;
inline constexpr std::uint32_t kCandidateLocalTransformOffset = 0x10U;
inline constexpr std::uint32_t kCandidateSelectionKeyOffset = 0x30U;
inline constexpr std::uint32_t kCandidateResultTransformOffset = 0x10U;
inline constexpr std::uint32_t kCandidateResultBytes = 0x50U;

inline constexpr std::uint32_t kProviderSelectionExactCallsiteRva = 0xA1F47CU;
inline constexpr std::uint32_t kActorTerminalInvalidationRva = 0x588690U;
inline constexpr std::uint32_t kActorTerminalInvalidationCallsiteRva = 0x58BB4BU;
inline constexpr std::uint32_t kSceneWrapperActorHandleOffset = 0x2CU;
inline constexpr std::uint32_t kSceneWrapperTerminalFlagsOffset = 0x246U;
inline constexpr std::uint8_t kSceneWrapperTerminalBit = 0x10U;
inline constexpr std::uint32_t kEffectCreateOneRva = 0x12065D0U;
inline constexpr std::uint32_t kEffectExplicitDestroyRva = 0x120B4F0U;
inline constexpr std::uint32_t kEffectPriorityEvictionDestroyRva = 0x120B3D0U;
inline constexpr std::uint32_t kEffectUpdateAndExpiryRva = 0x12103E0U;
inline constexpr std::uint32_t kEffectExpiryPathARva = 0x121059CU;
inline constexpr std::uint32_t kEffectExpiryPathBRva = 0x12106D0U;
inline constexpr std::uint32_t kEffectResourceTagObjectOffset = 0x3CU;

inline constexpr std::array<std::byte, 16U> kExactOutput1Descriptor{
    std::byte{0x02}, std::byte{0x01}, std::byte{0x01}, std::byte{0x00},
    std::byte{0x3C}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x6B}, std::byte{0x9A}, std::byte{0xDA}, std::byte{0x98},
    std::byte{0xFF}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};

struct EffectResourceDefinition final {
    std::uint32_t value{};
    friend constexpr bool operator==(EffectResourceDefinition,
                                     EffectResourceDefinition) noexcept = default;
};

struct EffectInstanceHandle32 final {
    std::uint32_t value{};
    friend constexpr bool operator==(EffectInstanceHandle32,
                                     EffectInstanceHandle32) noexcept = default;
};

inline constexpr std::array<EffectResourceDefinition, 4U> kEarlyEffectDefinitions{
    EffectResourceDefinition{0x80C71D8EU}, EffectResourceDefinition{0x80C71D8AU},
    EffectResourceDefinition{0x80F1FCCAU}, EffectResourceDefinition{0x80C71D70U}};

[[nodiscard]] bool early_effect_definition(EffectResourceDefinition definition) noexcept;
[[nodiscard]] constexpr std::uint32_t effect_serial_from_out_pair(std::uint64_t out_pair) noexcept {
    return static_cast<std::uint32_t>(out_pair);
}
[[nodiscard]] constexpr EffectInstanceHandle32 effect_handle_from_out_pair(
    std::uint64_t out_pair) noexcept {
    return EffectInstanceHandle32{static_cast<std::uint32_t>(out_pair >> 32U)};
}

// Permanent exclusion runs before provider/effect eligibility.
inline constexpr std::uint32_t kPlacedVisualClass = 0x80809927U;
inline constexpr std::uint32_t kStaticFanAnchor = 0x80EC0F94U;
inline constexpr std::uint32_t kStaticPortalTarget = 0x80F4AE39U;
inline constexpr std::array<std::uint32_t, 4U> kPlacedVisualDefinitions{
    0x80F47B52U, 0x80F47B76U, 0x80F47B79U, 0x80F47B7CU};
inline constexpr std::array<std::uint64_t, 4U> kPlacedVisualFullHandles{
    0x18FAA00468F48A31ULL, 0x34FAA005E55BDB69ULL,
    0x21FAA003A355AF43ULL, 0x71FAA3C889EB1FE3ULL};

struct RouteSubject final {
    std::uint32_t effect_class{};
    EffectResourceDefinition effect_resource{};
    std::uint32_t placed_anchor_or_target{};
    std::uint64_t placed_visual_full_handle{};
};

[[nodiscard]] bool permanently_excluded_static_visual(const RouteSubject& subject) noexcept;

inline constexpr std::uint32_t kInitialVisibleSchedulerTag = 0x80EC0F0EU;
inline constexpr std::uint32_t kCarrierSchedulerTag = 0x80EC0FA8U;
inline constexpr std::uint32_t kSuccessorVisibleSchedulerTag = 0x80EC0FA6U;
inline constexpr std::uint32_t kInitialVisibleFullHandle = 0x1BFAA3C4U;
inline constexpr std::uint32_t kCarrierFullHandle = 0x69FAA007U;
inline constexpr std::uint32_t kSuccessorVisibleFullHandle = 0x1CFAA3C4U;
inline constexpr std::uint16_t kReusedVisibleLowIndex = 0x03C4U;
inline constexpr std::uint64_t kObservedProbeSampleDeltaMs = 31U;

// 31 is only a coarse delta between different probe boundaries, never a provider outage.
enum class ProbeBoundary : std::uint8_t {
    after_transition_wrapper_and_schedule,
    before_successor_factory,
};
struct ProbeSamplePair final {
    std::uint64_t first_tick{};
    std::uint64_t second_tick{};
    ProbeBoundary first_boundary{ProbeBoundary::after_transition_wrapper_and_schedule};
    ProbeBoundary second_boundary{ProbeBoundary::before_successor_factory};
};
[[nodiscard]] std::uint64_t coarse_probe_sample_delta(const ProbeSamplePair& pair) noexcept;

inline constexpr std::uint32_t kPoseResourceDefinition = 0x80EC139FU;
inline constexpr std::uint64_t kPoseHeaderIdentity = 0x8080854680EC139FULL;
inline constexpr std::uint32_t kPoseObjectBytes = 0x550U;
inline constexpr std::uint32_t kLeftHandJointIndex = 21U;
inline constexpr std::uint32_t kLeftHandJointNameFnv1 = 0x67CB838CU;
inline constexpr char kLeftHandJointName[] = "b_l_hand";
inline constexpr std::uint32_t kSourceRetainedCandidateOrdinal = 7U;
inline constexpr std::uint32_t kSourceRetainedBindingKey = 0x15U;
inline constexpr std::uint32_t kSourceRetainedProviderFactoryOrdinal = 1U;
enum class EvidenceGrade : std::uint8_t { proven, source_retained, inferred, unknown, observed };
inline constexpr EvidenceGrade kFactory1OwnerGrade = EvidenceGrade::source_retained;
inline constexpr EvidenceGrade kCarrierPresentationCorrelationGrade = EvidenceGrade::inferred;
inline constexpr EvidenceGrade kCarrierExactProviderOwnerGrade = EvidenceGrade::unknown;
[[nodiscard]] std::uint32_t fnv1_name_hash(std::span<const char> name) noexcept;

class OwnerScopeToken final {
public:
    [[nodiscard]] constexpr bool valid() const noexcept {
        return cohort_id_ != 0U && host_session_ != 0U && activation_ != 0U && region_ != 0U
               && issue_serial_ != 0U;
    }
    [[nodiscard]] constexpr std::uint64_t cohort_id() const noexcept { return cohort_id_; }
    [[nodiscard]] constexpr std::uint64_t host_session() const noexcept { return host_session_; }
    [[nodiscard]] constexpr std::uint64_t activation() const noexcept { return activation_; }
    [[nodiscard]] constexpr std::uint64_t region() const noexcept { return region_; }
    [[nodiscard]] constexpr std::uint64_t issue_serial() const noexcept { return issue_serial_; }
    friend constexpr bool operator==(OwnerScopeToken, OwnerScopeToken) noexcept = default;

private:
    std::uint64_t cohort_id_{};
    std::uint64_t host_session_{};
    std::uint64_t activation_{};
    std::uint64_t region_{};
    std::uint64_t issue_serial_{};
    friend class LifecycleTokenOwner;
};

class LifecycleTokenOwner final {
public:
    LifecycleTokenOwner(RuntimeCohortToken cohort,
                        std::uint64_t host_session,
                        std::uint64_t activation,
                        std::uint64_t region) noexcept;
    LifecycleTokenOwner(const LifecycleTokenOwner&) = delete;
    LifecycleTokenOwner& operator=(const LifecycleTokenOwner&) = delete;
    [[nodiscard]] bool try_snapshot(OwnerScopeToken& output) const noexcept;
    void publish(std::uint64_t activation, std::uint64_t region) noexcept;
    [[nodiscard]] bool is_current(OwnerScopeToken token) const noexcept;

private:
    RuntimeCohortToken cohort_{};
    std::uint64_t host_session_{};
    mutable std::atomic<std::uint64_t> version_{2U};
    std::atomic<std::uint64_t> activation_{};
    std::atomic<std::uint64_t> region_{};
    mutable std::atomic<std::uint64_t> next_issue_{1U};
};

struct AddressRange final {
    std::uintptr_t begin{};
    std::uintptr_t end{};
    [[nodiscard]] bool contains(std::uintptr_t address, std::size_t bytes) const noexcept;
};

struct HandleResolution final {
    std::uint32_t full_handle{};
    std::uintptr_t record_identity{};
    std::uint64_t record_generation{};
    bool equality_rechecked_at_use{};
};

struct ActorGeneration final {
    OwnerScopeToken scope{};
    std::uint32_t scheduler_tag{};
    std::uint32_t entity_definition{};
    std::uint32_t full_handle{};
    std::uint16_t low_index{};
    std::uintptr_t object_record_identity{};
    std::uint64_t object_record_generation{};
    std::uint64_t factory_create_serial{};
    std::uint64_t terminal_serial{};
};

struct PresentationGeneration final {
    OwnerScopeToken scope{};
    std::uint64_t generation{};
    std::uint64_t bank_generation{};
    std::uint32_t machine_full_handle{};
    std::uint64_t state_update_serial{};
};

struct ProviderGeneration final {
    OwnerScopeToken scope{};
    ActorGeneration owner{};
    std::uint32_t component_definition{};
    std::uint32_t component_full_handle{};
    AddressRange component_allocation{};
    std::uint64_t component_generation{};
    HandleResolution runtime_provider_handle{};
    std::int64_t runtime_provider_relative{};
    std::uintptr_t provider_identity{};
    HandleResolution definition_handle{};
    std::int64_t definition_relative{};
    std::uintptr_t definition_target_identity{};
    std::uint64_t definition_target_generation{};
    std::int64_t definition_table_relative{};
    std::uintptr_t definition_table_identity{};
    std::uint64_t definition_table_generation{};
    AddressRange definition_table_allocation{};
    HandleResolution pose_dispatch_handle{};
    std::uintptr_t pose_dispatch_base_identity{};
    HandleResolution pose_object_handle{};
    std::int64_t pose_object_relative{};
    std::uintptr_t concrete_pose_object_identity{};
    std::int64_t filter_relative{};
    std::uintptr_t filter_context_identity{};
    std::uint32_t pose_resource_definition{};
    std::uint64_t pose_resource_generation{};
    std::uint64_t provider_generation{};
    std::array<std::byte, 0x78U> provider_bytes{};
    std::array<std::byte, 0x60U> definition_target_bytes{};
    bool provider_read_complete{};
    bool definition_target_read_complete{};
};

using Transform32 = std::array<std::byte, 0x20U>;
using RuntimeRow24 = std::array<std::byte, 0x18U>;
using CandidateRow64 = std::array<std::byte, 0x40U>;
using CandidateResult80 = std::array<std::byte, 0x50U>;

struct BankSnapshot final {
    std::uintptr_t bank_identity{};
    std::int64_t transform_relative{};
    std::uint64_t transform_count{};
    std::uintptr_t transform_base_identity{};
    AddressRange transform_allocation{};
    std::int64_t validity_relative{};
    std::uintptr_t validity_base_identity{};
    AddressRange validity_allocation{};
    std::int64_t runtime_relative{};
    std::uintptr_t runtime_base_identity{};
    std::uintptr_t runtime_rows_identity{};
    AddressRange runtime_allocation{};
    std::uintptr_t selected_output_identity{};
    std::uintptr_t selected_validity_identity{};
    std::uintptr_t selected_runtime_identity{};
    Transform32 output_before{};
    Transform32 output_after{};
    std::uint16_t validity_before{};
    std::uint16_t validity_after{};
    bool before_read_complete{};
    bool after_read_complete{};
};

struct SourceRowSnapshot final {
    std::uint32_t machine_definition{};
    std::uint32_t machine_class{};
    std::array<std::byte, 16U> descriptor{};
    RuntimeRow24 runtime{};
    std::uint8_t source_row{};
    std::uint8_t output_start{};
    std::uint8_t output_count{};
    std::uint8_t source_kind{};
    bool descriptor_read_complete{};
    bool runtime_read_complete{};
};

enum class RuntimeSelectorMode : std::uint8_t { ordinary, sentinel_f00dfeed, sentinel_feedf00d };
enum class NativeResultStatus : std::uint8_t {
    results_returned,
    zero_result,
    failed_negative,
    partial_read,
};

struct ResolverSnapshot final {
    std::uint32_t runtime_selector{};
    RuntimeSelectorMode selector_mode{RuntimeSelectorMode::ordinary};
    std::uint32_t resolver_stride{};
    std::uintptr_t selection_address{};
    std::uint32_t selection_value{};
    std::uint32_t candidate_interval_begin{};
    std::uint32_t candidate_interval_end{};
    std::uint32_t candidate_ordinal{};
    std::uintptr_t candidate_row_identity{};
    CandidateRow64 candidate_row{};
    std::uint32_t binding_key{};
    std::uint32_t candidate_selection_key{};
    Transform32 candidate_local{};
    std::uintptr_t pose_interface_dispatch_base{};
    std::uintptr_t pose_interface_object{};
    std::uint64_t pose_header_identity{};
    std::uint32_t pose_object_bytes{};
    std::uint64_t pose_resource_generation{};
    std::uint8_t socket_flags{};
    bool allow_fallback{};
    std::uint32_t provider_selection_callsite_rva{};
    std::uint64_t provider_route_ordinal_for_actor{};
    std::int32_t native_result_count{};
    std::uint32_t output_capacity{};
    NativeResultStatus result_status{NativeResultStatus::partial_read};
    CandidateResult80 candidate_result_before{};
    CandidateResult80 candidate_result_after{};
    Transform32 pose_output_before{};
    Transform32 pose_output_after{};
    bool candidate_row_read_complete{};
    bool candidate_result_before_read_complete{};
    bool candidate_result_after_read_complete{};
    bool pose_output_before_read_complete{};
    bool pose_output_after_read_complete{};
};

struct CaptureMetadata final {
    std::uint64_t capture_epoch{};
    std::uint64_t monotonic_tick{};
    std::uint64_t call_id{};
    std::uint64_t tls_nonce{};
    std::uint32_t producer_thread_id{};
    NativeSurface surface{NativeSurface::count};
    std::uint32_t callsite_rva{};
};

struct OriginalOnceReceipt final {
    std::uint64_t call_id{};
    std::uint8_t invocation_count{};
    [[nodiscard]] constexpr bool valid_for(std::uint64_t expected_call_id) const noexcept {
        return expected_call_id != 0U && call_id == expected_call_id && invocation_count == 1U;
    }
};

struct OwnershipObservation final {
    std::uint32_t observed_provider_factory_ordinal{};
    ActorGeneration concurrent_carrier{};
    bool carrier_concurrent{};
    bool carrier_presentation_correlated{};
    bool carrier_exact_provider_owner_proven{};
};

struct RouteCaptureRecord final {
    std::uint64_t sequence{};
    CaptureMetadata metadata{};
    OwnerScopeToken entry_scope{};
    OwnerScopeToken exit_scope{};
    RouteSubject subject{};
    PresentationGeneration presentation{};
    ProviderGeneration provider{};
    OwnershipObservation ownership{};
    SourceRowSnapshot source{};
    BankSnapshot bank{};
    ResolverSnapshot resolver{};
    OriginalOnceReceipt original{};
    bool provider_owner_handle_current_at_entry{};
    bool provider_owner_handle_current_at_exit{};
};

enum class RawObservationResult : std::uint8_t {
    retained,
    static_fan_excluded,
    invalid_metadata,
    invalid_scope_token,
    wrong_surface,
};
enum class RouteClosureResult : std::uint8_t {
    closed,
    raw_observation_invalid,
    stale_scope,
    retired_or_unbound_owner,
    wrong_machine_or_descriptor,
    wrong_source_row,
    incomplete_bank_read,
    bank_address_equation_mismatch,
    provider_address_equation_mismatch,
    table_address_equation_mismatch,
    dispatch_or_pose_equation_mismatch,
    sentinel_or_failed_result,
    candidate_address_equation_mismatch,
    incomplete_nested_read,
    nonfinite_transform,
    result_bank_copy_mismatch,
    original_once_not_proven,
};

[[nodiscard]] RawObservationResult validate_raw_observation(const RouteCaptureRecord& record) noexcept;
[[nodiscard]] RouteClosureResult close_route(const RouteCaptureRecord& record,
                                              OwnerScopeToken current_scope) noexcept;
[[nodiscard]] bool resolves_exact_left_hand_joint(const RouteCaptureRecord& record) noexcept;

struct ProviderFreshnessKey final {
    std::uint32_t actor_full_handle{};
    std::uint64_t actor_record_generation{};
    std::uint64_t component_generation{};
    std::uint32_t provider_full_handle{};
    std::uint64_t provider_record_generation{};
    std::uint64_t provider_generation{};
    std::uint64_t definition_target_generation{};
    std::uint64_t definition_table_generation{};
    std::uint64_t pose_dispatch_generation{};
    std::uint64_t pose_object_generation{};
    std::uint64_t pose_resource_generation{};
    std::uint64_t presentation_generation{};
    std::uint64_t bank_generation{};
    friend constexpr bool operator==(const ProviderFreshnessKey&,
                                     const ProviderFreshnessKey&) noexcept = default;
};
[[nodiscard]] ProviderFreshnessKey provider_freshness_key(const RouteCaptureRecord& record) noexcept;

enum class EffectTerminalKind : std::uint8_t {
    explicit_destroy,
    priority_eviction_destroy,
    automatic_expiry_path_a,
    automatic_expiry_path_b,
};

struct EffectCreateCapture final {
    CaptureMetadata metadata{};
    OwnerScopeToken scope{};
    EffectResourceDefinition resource{};
    std::uint64_t out_pair_before{};
    std::uint64_t out_pair_after{};
    std::uintptr_t resolved_effect_identity{};
    std::uint64_t pool_generation{};
    std::uint64_t create_serial{};
    std::uint32_t owner_actor_full_handle{};
    std::uint64_t owner_actor_record_generation{};
    std::uint64_t owner_provider_generation{};
    std::uint32_t parent_or_context_handle{};
    bool first_or_child_call{};
    bool native_success{};
    bool out_pair_before_read_complete{};
    bool out_pair_after_read_complete{};
    OriginalOnceReceipt original{};
};

struct EffectTerminalCapture final {
    CaptureMetadata metadata{};
    OwnerScopeToken scope{};
    EffectTerminalKind kind{EffectTerminalKind::explicit_destroy};
    EffectInstanceHandle32 full_handle{};
    std::uintptr_t resolved_effect_identity{};
    std::uint64_t pool_generation{};
    std::uint32_t terminal_path_rva{};
    std::uint64_t terminal_serial{};
    OriginalOnceReceipt original{};
};

struct ActorInvalidationCapture final {
    CaptureMetadata metadata{};
    OwnerScopeToken retiring_scope{};
    ActorGeneration retiring_actor{};
    std::uint32_t wrapper_scene_tag{};
    std::uint32_t wrapper_full_actor_handle{};
    std::uint8_t terminal_flags_before{};
    std::uint8_t terminal_flags_after{};
    std::uint64_t invalidated_provider_generation{};
    std::uint64_t invalidated_bank_generation{};
    bool rooted_generation_invalidated_before_original{};
    OriginalOnceReceipt original{};
};

enum class AuxiliaryValidationResult : std::uint8_t {
    valid,
    invalid_metadata,
    invalid_scope,
    wrong_boundary,
    invalid_resource_or_handle,
    invalid_out_pair,
    wrong_terminal_path,
    invalidation_not_pre_native,
    original_once_not_proven,
};
[[nodiscard]] AuxiliaryValidationResult validate_effect_create(const EffectCreateCapture& capture) noexcept;
[[nodiscard]] AuxiliaryValidationResult validate_effect_terminal(const EffectTerminalCapture& capture) noexcept;
[[nodiscard]] AuxiliaryValidationResult validate_actor_invalidation(const ActorInvalidationCapture& capture) noexcept;

struct EffectLifetime final {
    OwnerScopeToken scope{};
    EffectResourceDefinition resource{};
    EffectInstanceHandle32 full_handle{};
    std::uintptr_t resolved_effect_identity{};
    std::uint32_t serial_token{};
    std::uint64_t pool_generation{};
    std::uint64_t create_serial{};
    std::uint32_t owner_actor_full_handle{};
    std::uint64_t owner_actor_record_generation{};
    std::uint64_t owner_provider_generation{};
    std::uint64_t terminal_serial{};
    EffectTerminalKind terminal_kind{EffectTerminalKind::explicit_destroy};
    bool terminal_observed{};
};
enum class EffectLedgerResult : std::uint8_t {
    recorded,
    busy,
    full,
    invalid_capture,
    duplicate_live_handle,
    unmatched_terminal,
    already_terminal,
};
inline constexpr std::size_t kEffectLedgerCapacity = 64U;

class EffectLifetimeLedger final {
public:
    [[nodiscard]] EffectLedgerResult try_record_create(const EffectCreateCapture& capture) noexcept;
    [[nodiscard]] EffectLedgerResult try_record_terminal(const EffectTerminalCapture& capture) noexcept;
    [[nodiscard]] bool try_find(EffectInstanceHandle32 handle,
                                std::uint64_t pool_generation,
                                EffectLifetime& output) noexcept;
#if defined(SUNRISE_IKORA_VFX_ROUTE_CAPTURE_TEST)
    [[nodiscard]] bool testing_lock() noexcept { return try_lock(); }
    void testing_unlock() noexcept { unlock(); }
#endif
private:
    [[nodiscard]] bool try_lock() noexcept { return !lock_.test_and_set(std::memory_order_acquire); }
    void unlock() noexcept { lock_.clear(std::memory_order_release); }
    std::array<EffectLifetime, kEffectLedgerCapacity> entries_{};
    std::size_t count_{};
    std::atomic_flag lock_ = ATOMIC_FLAG_INIT;
};

enum class SuccessorEligibilityResult : std::uint8_t {
    eligible_capture_only_emission_not_observed,
    invalid_invalidation,
    successor_route_not_closed,
    wrong_successor_tuple,
    not_first_exact_route,
    carrier_fallback_rejected,
    stale_or_reused_generation,
    wrong_selection_candidate_or_joint,
};
[[nodiscard]] SuccessorEligibilityResult evaluate_successor_eligibility(
    const ActorInvalidationCapture& invalidation,
    const ProviderFreshnessKey& retiring_key,
    const RouteCaptureRecord& successor,
    OwnerScopeToken current_scope) noexcept;

enum class QueuePushResult : std::uint8_t {
    enqueued,
    static_fan_excluded,
    rejected,
    full,
    busy,
    sequence_exhausted,
};
enum class QueuePopResult : std::uint8_t { success, empty, busy };
struct QueueCounters final {
    std::uint64_t accepted{};
    std::uint64_t rejected{};
    std::uint64_t static_fan_excluded{};
    std::uint64_t dropped_full{};
    std::uint64_t dropped_busy{};
    std::uint64_t dropped_sequence_exhausted{};
    [[nodiscard]] constexpr std::uint64_t losses() const noexcept {
        return rejected + static_fan_excluded + dropped_full + dropped_busy
               + dropped_sequence_exhausted;
    }
};
inline constexpr std::size_t kCaptureQueueCapacity = 64U;

class CaptureQueue final {
public:
    CaptureQueue() noexcept = default;
    CaptureQueue(const CaptureQueue&) = delete;
    CaptureQueue& operator=(const CaptureQueue&) = delete;
    [[nodiscard]] QueuePushResult try_push(const RouteCaptureRecord& record) noexcept;
    [[nodiscard]] QueuePopResult try_pop(RouteCaptureRecord& output) noexcept;
    [[nodiscard]] QueueCounters counters() const noexcept;
#if defined(SUNRISE_IKORA_VFX_ROUTE_CAPTURE_TEST)
    [[nodiscard]] bool testing_lock() noexcept { return try_lock(); }
    void testing_unlock() noexcept { unlock(); }
    void testing_set_next_sequence(std::uint64_t value) noexcept { next_sequence_ = value; }
#endif
private:
    [[nodiscard]] bool try_lock() noexcept { return !lock_.test_and_set(std::memory_order_acquire); }
    void unlock() noexcept { lock_.clear(std::memory_order_release); }
    std::array<RouteCaptureRecord, kCaptureQueueCapacity> records_{};
    std::size_t head_{};
    std::size_t count_{};
    std::uint64_t next_sequence_{1U};
    std::atomic_flag lock_ = ATOMIC_FLAG_INIT;
    std::atomic<std::uint64_t> accepted_{};
    std::atomic<std::uint64_t> rejected_{};
    std::atomic<std::uint64_t> static_fan_excluded_{};
    std::atomic<std::uint64_t> dropped_full_{};
    std::atomic<std::uint64_t> dropped_busy_{};
    std::atomic<std::uint64_t> dropped_sequence_exhausted_{};
};

// FNV-1a is only a bounded local equality aid, never privacy or integrity protection.
[[nodiscard]] std::uint64_t bounded_local_hash(std::span<const std::byte> bytes) noexcept;

class OpaqueIdProjector final {
public:
    [[nodiscard]] std::uint32_t project(std::uint64_t sensitive_scalar) noexcept;
private:
    struct Entry final { std::uint64_t value{}; std::uint32_t id{}; };
    std::array<Entry, 64U> entries_{};
    std::size_t count_{};
    std::uint32_t next_id_{1U};
};

enum class TelemetryRecordStatus : std::uint8_t { raw_retained, route_closed, stale_or_partial };
// No raw pointers, packed handles, raw bytes, effect definitions, or resource tags.
struct RouteTelemetry final {
    std::uint64_t sequence{};
    std::uint64_t cohort_id{};
    std::uint64_t capture_epoch{};
    std::uint64_t monotonic_tick{};
    std::uint64_t call_id{};
    std::uint64_t activation{};
    std::uint64_t region{};
    std::uint64_t presentation_generation{};
    std::uint64_t bank_generation{};
    std::uint64_t descriptor_hash{};
    std::uint64_t runtime_hash{};
    std::uint64_t candidate_row_hash{};
    std::uint64_t candidate_result_hash{};
    std::uint64_t bank_output_hash{};
    std::uint64_t queue_loss_epoch{};
    std::uint32_t actor_opaque_id{};
    std::uint32_t provider_opaque_id{};
    std::uint32_t pose_opaque_id{};
    std::uint32_t surface_rva{};
    std::uint32_t callsite_rva{};
    std::uint32_t runtime_selector{};
    std::uint32_t selection{};
    std::uint32_t candidate_ordinal{};
    std::uint32_t binding_key{};
    std::uint8_t output_slot{};
    NativeSurface surface{NativeSurface::count};
    TelemetryRecordStatus status{TelemetryRecordStatus::raw_retained};
    NativeResultStatus native_result{NativeResultStatus::partial_read};
    bool descriptor_valid{};
    bool runtime_valid{};
    bool candidate_valid{};
    bool result_valid{};
    bool bank_valid{};
    bool owner_current_at_entry{};
    bool owner_current_at_exit{};
    bool fallback_used{};
    bool left_hand_joint_valid{};
    bool carrier_concurrent{};
    bool carrier_presentation_correlated{};
    bool carrier_exact_provider_owner_proven{};
    std::uint32_t observed_provider_factory_ordinal{};
};
[[nodiscard]] bool default_telemetry(const RouteCaptureRecord& published_record,
                                     OwnerScopeToken current_scope,
                                     std::uint64_t queue_loss_epoch,
                                     OpaqueIdProjector& projector,
                                     RouteTelemetry& output) noexcept;

static_assert(kPinnedPackedRuntimeSha256 != kPinnedUnpackedProvenanceSha256);
static_assert(kSourceRetainedBindingKey == kLeftHandJointIndex);
static_assert(!kIntegrationReady && !kCarrierFallbackAllowed);
static_assert(!kFrozenSuccessorRouteObserved && !kFrozenSuccessorEmissionObserved);
static_assert(!kCaptureBeforeEnableOraclePassed && !kSuccessorEmissionEnabled);
static_assert(std::is_trivially_copyable_v<RouteCaptureRecord>);
static_assert(std::is_trivially_copyable_v<EffectCreateCapture>);
static_assert(std::is_trivially_copyable_v<EffectTerminalCapture>);
static_assert(std::is_trivially_copyable_v<ActorInvalidationCapture>);
static_assert(std::is_trivially_copyable_v<RouteTelemetry>);
static_assert(std::atomic<std::uint64_t>::is_always_lock_free);

} // namespace sunrise::client::hooks::bootflow::opening_authority::ikora_vfx_route
