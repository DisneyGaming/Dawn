#pragma once

#if !defined(_WIN32)
#error The pinned Type-69 capture substrate supports Win64 only.
#endif

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <span>
#include <type_traits>
#include <utility>

namespace sunrise::client::hooks::bootflow::opening_authority::type69_capture {

inline constexpr bool kOwnsRuntimeDetour = false;
inline constexpr bool kWritesNativeState = false;
inline constexpr bool kSynthesizesType69Action = false;
inline constexpr bool kCallsType68ApplyDirectly = false;
inline constexpr bool kCallsType68InstallDirectly = false;
inline constexpr bool kGuessesUserInterface = false;
inline constexpr bool kDefaultTelemetryContainsRawBodies = false;

inline constexpr std::size_t kSha256Bytes = 32U;
using Sha256 = std::array<std::byte, kSha256Bytes>;

inline constexpr wchar_t kPinnedPackedRuntimePath[] = L"D:\\Destiny3\\destiny2.exe";
inline constexpr std::uint64_t kPinnedPackedRuntimeBytes = 122'984'224U;
inline constexpr Sha256 kPinnedPackedRuntimeSha256{
    std::byte{0x81}, std::byte{0x96}, std::byte{0x43}, std::byte{0x80},
    std::byte{0x66}, std::byte{0x4E}, std::byte{0x7F}, std::byte{0xCE},
    std::byte{0xE3}, std::byte{0xC6}, std::byte{0x20}, std::byte{0x08},
    std::byte{0x5A}, std::byte{0x15}, std::byte{0x7F}, std::byte{0xDE},
    std::byte{0xAF}, std::byte{0x91}, std::byte{0xFE}, std::byte{0xFA},
    std::byte{0xCF}, std::byte{0x72}, std::byte{0x14}, std::byte{0x90},
    std::byte{0x78}, std::byte{0x20}, std::byte{0xF1}, std::byte{0x88},
    std::byte{0xBB}, std::byte{0xEB}, std::byte{0x4C}, std::byte{0xED}};

inline constexpr wchar_t kPinnedUnpackedProvenancePath[] =
    L"D:\\Sunrise-work\\ghidra\\destiny2_unpacked.exe";
inline constexpr std::uint64_t kPinnedUnpackedProvenanceBytes = 145'091'072U;
inline constexpr std::uintptr_t kPinnedUnpackedPreferredImageBase = 0x7FF68E9A0000ULL;
inline constexpr Sha256 kPinnedUnpackedProvenanceSha256{
    std::byte{0x87}, std::byte{0x13}, std::byte{0xD1}, std::byte{0x5E},
    std::byte{0x3D}, std::byte{0x05}, std::byte{0xB2}, std::byte{0x6F},
    std::byte{0x9E}, std::byte{0x25}, std::byte{0x9E}, std::byte{0x02},
    std::byte{0xB0}, std::byte{0xF2}, std::byte{0x9B}, std::byte{0xC1},
    std::byte{0xE0}, std::byte{0x00}, std::byte{0xE4}, std::byte{0xB0},
    std::byte{0xC6}, std::byte{0x2C}, std::byte{0xA2}, std::byte{0xCC},
    std::byte{0x87}, std::byte{0xC3}, std::byte{0x85}, std::byte{0x97},
    std::byte{0x18}, std::byte{0x6C}, std::byte{0xC3}, std::byte{0xBD}};

inline constexpr wchar_t kPinnedOmegaPackagePath[] =
    L"D:\\Destiny3\\packages\\w64_mercury_destination_activities_03a3_5.pkg";
inline constexpr std::uint64_t kPinnedOmegaPackageBytes = 407'552U;
inline constexpr Sha256 kPinnedOmegaPackageSha256{
    std::byte{0x99}, std::byte{0x77}, std::byte{0xBA}, std::byte{0xAA},
    std::byte{0xE8}, std::byte{0x9B}, std::byte{0xF0}, std::byte{0x7A},
    std::byte{0x81}, std::byte{0x59}, std::byte{0x02}, std::byte{0xEC},
    std::byte{0x81}, std::byte{0xEE}, std::byte{0xCD}, std::byte{0x28},
    std::byte{0x9A}, std::byte{0x7B}, std::byte{0x1B}, std::byte{0xA8},
    std::byte{0x1E}, std::byte{0x7E}, std::byte{0x32}, std::byte{0x8C},
    std::byte{0xD0}, std::byte{0xF6}, std::byte{0x7D}, std::byte{0x1C},
    std::byte{0x25}, std::byte{0x07}, std::byte{0x5F}, std::byte{0x17}};

inline constexpr std::uint32_t kPinnedPcSizeOfImage = 0x08A5EA00U;
inline constexpr std::uint16_t kPinnedPcMachineAmd64 = 0x8664U;
inline constexpr std::uint16_t kPinnedPe32PlusMagic = 0x020BU;

struct ArtifactInspection final {
    std::uint64_t file_bytes{};
    Sha256 sha256{};
    std::uint32_t pe_size_of_image{};
    std::uint16_t pe_machine{};
    std::uint16_t optional_header_magic{};
};

struct FileDigestInspection final {
    std::uint64_t file_bytes{};
    Sha256 sha256{};
};

enum class ArtifactInspectionResult : std::uint8_t {
    complete,
    invalid_arguments,
    open_failed,
    read_failed,
    hash_failed,
    invalid_pe,
};

[[nodiscard]] ArtifactInspectionResult inspect_file_artifact(
    const wchar_t* path,
    ArtifactInspection& output) noexcept;
[[nodiscard]] ArtifactInspectionResult inspect_file_digest(
    const wchar_t* path,
    FileDigestInspection& output) noexcept;

enum class EvidenceState : std::uint8_t { proven, inferred_high, unknown };
enum class BindingState : std::uint8_t {
    static_expectation_only,
    materialization_unobserved,
    observed_candidate_not_joined,
};

inline constexpr EvidenceState kFullSubscriberStructure = EvidenceState::proven;
inline constexpr EvidenceState kShortFormTerminalClassification = EvidenceState::inferred_high;
inline constexpr EvidenceState kAuthoredSlotToMaterializedEndpointIndex = EvidenceState::unknown;
inline constexpr EvidenceState kConcreteSlot30SubscriberIdentity = EvidenceState::unknown;
inline constexpr EvidenceState kSlot30ToType68Publication = EvidenceState::unknown;
inline constexpr EvidenceState kOpeningType68Body = EvidenceState::unknown;
inline constexpr EvidenceState kRuntimeEndpointBindingStructure = EvidenceState::proven;
inline constexpr EvidenceState kRuntimeEndpointToAuthoredRow = EvidenceState::unknown;
inline constexpr EvidenceState kType68ReflectionSerialization = EvidenceState::proven;
inline constexpr EvidenceState kDecodedType68Builder = EvidenceState::unknown;
inline constexpr EvidenceState kExactType68OutboundCallsite = EvidenceState::unknown;
inline constexpr EvidenceState kFirstOutboundType68Body = EvidenceState::unknown;

enum class PcSurface : std::uint8_t {
    runtime_resolver_entry,
    full_dispatch_entry,
    full_category_resolver_entry,
    full_thunk,
    full_handler_entry,
    full_type69_branch,
    full_pre_subscriber_call,
    full_post_subscriber_return,
    full_subscriber_trampoline,
    short_dispatch_entry,
    short_category_resolver_entry,
    short_thunk,
    short_handler_entry,
    short_type69_branch,
    short_pre_subscriber_call,
    short_post_subscriber_return,
    short_subscriber_trampoline,
    endpoint_activation_entry,
    endpoint_binder_entry,
    endpoint_registration_call,
    endpoint_registration_return,
    endpoint_registration_trampoline,
    endpoint_lookup_entry,
    endpoint_copy_entry,
    reflected_decoder_entry,
    reflected_encoder_entry,
    large_serializer_entry,
    large_serializer_cursor,
    schema_transport_entry,
    schema_transport_encode_call,
    schema_transport_transport_call,
    decoded_pointer_getter,
    type68_apply_entry,
    type68_authored_resolution_entry,
    type68_event_row_lookup_entry,
    type68_post_cache_window,
    type68_lifecycle0_call,
    type68_lifecycle0_return,
    type68_formatter_call,
    type68_formatter_return,
    type68_manager_add_call,
    type68_manager_add_return,
    directive_formatter_entry,
    manager_add_entry,
    manager_presentation_update_entry,
    presentation_queue_entry,
    queue_promote_entry,
    cui_provider_entry,
    count,
};

inline constexpr std::size_t kPcSurfaceCount = static_cast<std::size_t>(PcSurface::count);

enum class BoundaryKind : std::uint8_t {
    function_entry,
    exact_function,
    thunk,
    instruction_window,
    callsite_window,
    return_window,
};

enum class PcAbi : std::uint8_t {
    runtime_resolver_win64,
    dispatch_win64,
    internal_unexposed,
    full_subscriber_target_win64,
    short_subscriber_target_win64,
    endpoint_binder_win64,
    endpoint_registration_target_win64,
    endpoint_lookup_win64,
    endpoint_copy_win64,
    reflected_decoder_win64,
    reflected_encoder_win64,
    schema_serializer_win64,
    schema_transport_win64,
    type68_apply_win64,
};

struct PcBoundaryDescriptor final {
    std::uintptr_t rva{};
    std::span<const std::byte> prefix{};
    PcAbi abi{PcAbi::internal_unexposed};
    BoundaryKind kind{BoundaryKind::instruction_window};
    std::size_t exact_function_bytes{};
    bool inline_detour_authorized{};
};

[[nodiscard]] PcBoundaryDescriptor pc_boundary(PcSurface surface) noexcept;
[[nodiscard]] bool pc_prefix_matches(PcSurface surface,
                                     std::span<const std::byte> observed) noexcept;

enum class LiveCohortValidationResult : std::uint8_t {
    valid,
    main_module_unavailable,
    path_resolution_failed,
    packed_path_mismatch,
    packed_size_mismatch,
    packed_hash_mismatch,
    mapped_pe_invalid,
    mapped_image_size_mismatch,
    surface_out_of_range,
    surface_page_not_executable,
    cohort_prefix_mismatch,
};

class ValidatedRuntimeCohort final {
public:
    ValidatedRuntimeCohort() noexcept = default;
    [[nodiscard]] bool valid() const noexcept { return validation_cookie_ != 0U; }
    [[nodiscard]] std::uint64_t build_id() const noexcept { return build_id_; }
    [[nodiscard]] std::uint64_t cohort_id() const noexcept { return cohort_id_; }
    [[nodiscard]] std::uintptr_t module_base() const noexcept { return module_base_; }
    [[nodiscard]] std::uint32_t size_of_image() const noexcept { return size_of_image_; }
    [[nodiscard]] std::uint64_t surface_mask() const noexcept { return surface_mask_; }

private:
    friend LiveCohortValidationResult validate_live_pc_runtime_cohort(
        ValidatedRuntimeCohort&) noexcept;
    friend class CaptureCohortOwner;
#if defined(SUNRISE_TYPE69_CAPTURE_TESTING)
    friend struct CaptureTestAccess;
#endif
    std::uint64_t validation_cookie_{};
    std::uint64_t build_id_{};
    std::uint64_t cohort_id_{};
    std::uintptr_t module_base_{};
    std::uint32_t size_of_image_{};
    std::uint64_t surface_mask_{};
};

/** Validates the actual process main module and every pinned surface as one cohort. */
[[nodiscard]] LiveCohortValidationResult validate_live_pc_runtime_cohort(
    ValidatedRuntimeCohort& output) noexcept;

#if defined(_MSC_VER)
using RuntimeResolverOriginal =
    std::uint8_t(__fastcall*)(std::uint16_t*, std::byte*, std::uint8_t) noexcept;
using DispatchOriginal = void(__fastcall*)(void*, const std::byte*, void*) noexcept;
using FullSubscriberTarget =
    void(__fastcall*)(void*, std::uint8_t, std::int32_t, const float*, std::uint32_t) noexcept;
using ShortSubscriberTarget = void(__fastcall*)(void*, std::uint32_t) noexcept;
#else
using RuntimeResolverOriginal =
    std::uint8_t (*)(std::uint16_t*, std::byte*, std::uint8_t) noexcept;
using DispatchOriginal = void (*)(void*, const std::byte*, void*) noexcept;
using FullSubscriberTarget =
    void (*)(void*, std::uint8_t, std::int32_t, const float*, std::uint32_t) noexcept;
using ShortSubscriberTarget = void (*)(void*, std::uint32_t) noexcept;
#endif

inline constexpr std::uint8_t kType69 = 0x45U;
inline constexpr std::size_t kRuntimeRecordTypeOffset = 0x0AU;
inline constexpr std::size_t kRuntimeRecordBodyOffset = 0x10U;
inline constexpr std::size_t kRuntimeRecordCaptureBytes = 0x30U;
inline constexpr std::size_t kActionTypeOffset = 0x60U;
inline constexpr std::size_t kType69PayloadBytes = 0x20U;

#pragma pack(push, 1)
struct Type69Payload final {
    std::int32_t endpoint_index{};
    std::uint32_t auxiliary{};
    std::uint8_t operation{};
    std::array<std::byte, 3U> unclaimed_09{};
    std::int32_t selectable_low{};
    std::int32_t selectable_high{};
    std::array<float, 3U> payload_xyz{};
};
#pragma pack(pop)

struct DescriptorProjection final {
    Type69Payload payload{};
    std::uint8_t type{};
};

using RuntimeRecordImage = std::array<std::byte, kRuntimeRecordCaptureBytes>;

[[nodiscard]] bool runtime_record_projection(const RuntimeRecordImage& record,
                                             Type69Payload& output,
                                             std::uint8_t& type) noexcept;
[[nodiscard]] bool descriptor_projection(const void* descriptor,
                                         DescriptorProjection& output) noexcept;
[[nodiscard]] constexpr bool suppresses_native_dispatch(const Type69Payload& payload) noexcept {
    return payload.endpoint_index == -1;
}
[[nodiscard]] constexpr bool selectable_is_failure_sentinel(
    const Type69Payload& payload) noexcept {
    return payload.selectable_low == -1 && payload.selectable_high == -1;
}
[[nodiscard]] bool resolver_failure_sentinel(const Type69Payload& payload) noexcept;

inline constexpr std::uint32_t kOmegaRegistryTag = 0x80F47BC6U;
inline constexpr std::uint32_t kOmegaRegistryClass = 0x80809462U;
inline constexpr std::uint32_t kOmegaRegistryBytes = 408U;
inline constexpr std::size_t kOmegaRegistryKeyOffset = 0x0CU;
inline constexpr std::uint32_t kOmegaRegistryKey = 0xF7A6CE7FU;
inline constexpr std::size_t kOmegaRegistrySlotCountOffset = 0x20U;
inline constexpr std::uint32_t kOmegaRegistrySlotCount = 13U;
inline constexpr std::size_t kOmegaRegistrySlotTableOffset = 0x70U;
inline constexpr std::size_t kOmegaRegistrySlotStride = 8U;

struct AuthoredSlotRowIdentity final {
    std::uint32_t registry_tag{};
    std::uint32_t registry_class{};
    std::uint32_t registry_key{};
    std::uint32_t slot{};
    std::uint32_t type{};
    std::uint32_t name_hash{};
    std::size_t row_offset{};
    friend constexpr bool operator==(AuthoredSlotRowIdentity,
                                     AuthoredSlotRowIdentity) noexcept = default;
};

inline constexpr AuthoredSlotRowIdentity kOpeningDirectiveRow{
    kOmegaRegistryTag, kOmegaRegistryClass, kOmegaRegistryKey, 5U, 69U, 0xC252E306U, 0x98U};
inline constexpr AuthoredSlotRowIdentity kNextDirectiveRow{
    kOmegaRegistryTag, kOmegaRegistryClass, kOmegaRegistryKey, 6U, 69U, 0x1EBF4621U, 0xA0U};

struct AuthoredBankIdentity final {
    std::uint32_t tag{};
    std::uint32_t class_id{};
    std::uint32_t event_key{};
    std::size_t event_key_offset{};
};

inline constexpr AuthoredBankIdentity kOpeningDirectiveBank{
    0x80F47BD3U, 0x80804F72U, 0xC252E306U, 0x30U};

struct Type68PublicationIdentity final {
    std::uint32_t registry{};
    std::uint32_t type{};
    std::uint32_t index{};
    std::uint32_t component_class{};
    std::uint32_t authority_schema{};
    std::uint32_t definition{};
    std::uint32_t bank{};
    std::uint32_t fixed_wire_bits{};
    std::uint32_t decoded_bytes{};
};

inline constexpr Type68PublicationIdentity kDownstreamType68Identity{
    0x82FB58B7U, 68U, 0U, 0x80804F53U, 0x80804F67U, 0x80F47BD4U,
    0x80F47BD3U, 4'802U, 0x300U};

inline constexpr std::size_t kActivityTargetEndpointVectorOwnerOffset = 0x588U;
inline constexpr std::size_t kEndpointRecordStride = 0x50U;
inline constexpr std::size_t kEndpointRuntimeKeyOffset = 0x30U;
inline constexpr std::size_t kEndpointRuntimeKeyBytes = 40U;
inline constexpr std::size_t kEndpointInterfaceHalfOffset = 0x40U;
inline constexpr std::size_t kEndpointTargetHalfOffset = 0x48U;
inline constexpr std::size_t kEndpointActivationCountOffset = 0x580U;
inline constexpr std::size_t kEndpointActivationTargetHandleOffset = 0x5C0U;
inline constexpr std::size_t kEndpointOwnerContainerHandleOffset = 0x00U;
inline constexpr std::size_t kEndpointOwnerClearedOffset = 0x04U;
inline constexpr std::size_t kEndpointOwnerCountOffset = 0x88U;
inline constexpr std::uint32_t kEndpointNotFound = 0xFFFFFFFFU;
inline constexpr std::size_t kEndpointRegistrationMethodSlot = 0x90U;
inline constexpr std::size_t kFullSubscriberMethodSlot = 0x30U;
inline constexpr std::size_t kShortSubscriberMethodSlot = 0x48U;

using RuntimeEndpointKey40 = std::array<std::byte, kEndpointRuntimeKeyBytes>;

inline constexpr std::uint32_t kType68AuthoritySchema = 0x80804F67U;
inline constexpr std::size_t kType68DecodedBytes = 0x300U;
inline constexpr std::uint32_t kType68SignificantBits = 4'802U;
inline constexpr std::uint32_t kType68RoundedBytes = 601U;
inline constexpr std::uint32_t kLargeSerializerCapacityBytes = 0x7D800U;
inline constexpr std::uint32_t kSchemaTransportEncodeFlag = 1U;
inline constexpr bool kReflectedEncoderReturnIsOverflowOracle = false;
inline constexpr bool kSerializerSurfaceIsCallableByCaptureModule = false;
inline constexpr bool kSchemaTransportSurfaceIsCallableByCaptureModule = false;

enum class StaticInterfacePlatform : std::uint8_t { pc_win64, ps4_x64 };

struct StaticInterfaceIdentity final {
    StaticInterfacePlatform platform{StaticInterfacePlatform::pc_win64};
    std::uint32_t type{};
    std::uint32_t schema{};
    std::uintptr_t type_getter_rva{};
    std::uintptr_t schema_getter_rva{};
    const char* decoded_rtti{};
};

inline constexpr StaticInterfaceIdentity kPs4Type68Interface{
    StaticInterfacePlatform::ps4_x64, 68U, kType68AuthoritySchema, 0x4F3DB0U,
    0x4FDE20U, "c_directive_sensor_import_interface"};
inline constexpr StaticInterfaceIdentity kPs4Type69Interface{
    StaticInterfacePlatform::ps4_x64, 69U, 0U, 0x4F3DC0U, 0U,
    "c_directive_proxy_import_interface"};

enum class CapturePlanStage : std::uint8_t {
    activation_materialization,
    concrete_subscriber,
    queue_classification,
    type68_builder_serializer,
    receive_manager_join,
};

enum class Type68TransportInput : std::uint8_t {
    unrelated_schema,
    decoded_candidate,
    preencoded_publication_candidate,
};

[[nodiscard]] constexpr Type68TransportInput classify_type68_transport(
    std::uint32_t schema,
    std::uint32_t flags) noexcept {
    if (schema != kType68AuthoritySchema) {
        return Type68TransportInput::unrelated_schema;
    }
    return (flags & kSchemaTransportEncodeFlag) == 0U
               ? Type68TransportInput::decoded_candidate
               : Type68TransportInput::preencoded_publication_candidate;
}

/** Generic codec traffic is a candidate only; it is never an endpoint-lineage join by itself. */
[[nodiscard]] constexpr bool exact_type68_serialization_shape(std::uint32_t schema,
                                                              std::uint32_t cursorBits,
                                                              std::uint32_t roundedBytes,
                                                              bool writerError) noexcept {
    return schema == kType68AuthoritySchema && cursorBits == kType68SignificantBits
           && roundedBytes == kType68RoundedBytes && !writerError;
}

/** No live materializer surface is recovered, so production cannot mint a valid token. */
class MaterializationObservationToken final {
public:
    MaterializationObservationToken() noexcept = default;
    [[nodiscard]] bool valid() const noexcept { return value_ != 0U; }
    [[nodiscard]] std::uint64_t value() const noexcept { return value_; }

private:
    std::uint64_t value_{};
};

enum class OwnerField : std::uint32_t {
    activation = 1U << 0U,
    activity = 1U << 1U,
    connection = 1U << 2U,
    roster = 1U << 3U,
    registry_materialization = 1U << 4U,
    endpoint_vector = 1U << 5U,
    authority = 1U << 6U,
    run_token = 1U << 7U,
    correlation_token = 1U << 8U,
    component = 1U << 9U,
};

inline constexpr std::uint32_t kNativeRequiredOwnerMask =
    static_cast<std::uint32_t>(OwnerField::activation)
    | static_cast<std::uint32_t>(OwnerField::activity)
    | static_cast<std::uint32_t>(OwnerField::connection)
    | static_cast<std::uint32_t>(OwnerField::roster)
    | static_cast<std::uint32_t>(OwnerField::registry_materialization)
    | static_cast<std::uint32_t>(OwnerField::endpoint_vector)
    | static_cast<std::uint32_t>(OwnerField::run_token)
    | static_cast<std::uint32_t>(OwnerField::correlation_token);

enum class ActivationSnapshotState : std::uint8_t { absent, current, quiescing, stale };

struct OwnerGenerationSet final {
    std::uint32_t presence_mask{};
    std::uint64_t module_generation{};
    std::uint64_t activation_generation{};
    std::uint64_t activity_session_id{};
    std::uint64_t activity_generation{};
    std::uint64_t connection_generation{};
    std::uint64_t roster_generation{};
    std::uint64_t registry_materialization_generation{};
    std::uint64_t endpoint_vector_generation{};
    std::uint64_t authority_generation{};
    std::uint64_t run_token{};
    std::uint64_t correlation_token{};
    std::uint64_t component_generation{};
    ActivationSnapshotState activation_state{ActivationSnapshotState::absent};
    friend constexpr bool operator==(OwnerGenerationSet, OwnerGenerationSet) noexcept = default;
};

[[nodiscard]] bool owner_fields_present(const OwnerGenerationSet& snapshot,
                                        std::uint32_t requiredMask) noexcept;

enum class NativeParticipant : std::uint8_t {
    resolver,
    full_dispatch,
    full_callsite,
    short_dispatch,
    short_callsite,
    endpoint_binder,
    endpoint_registration_callsite,
    large_serializer,
    schema_transport,
    reflected_decoder,
    type68_apply,
    count,
};
inline constexpr std::size_t kNativeParticipantCount =
    static_cast<std::size_t>(NativeParticipant::count);

enum class OwnerValidationState : std::uint8_t {
    not_published,
    exact_same_owner,
    readable_but_stale,
    provider_unstable,
};

struct CaptureTiming final {
    std::uint64_t pre_monotonic_tick{};
    std::uint32_t producer_thread_id{};
    std::uintptr_t caller_rva{};
};

struct CaptureProvenance final {
    std::uint64_t build_id{};
    std::uint64_t cohort_id{};
    std::uint64_t capture_epoch{};
    std::uint64_t owner_instance_id{};
    std::uint64_t call_id{};
    std::uint64_t parent_call_id{};
    std::uint64_t materialization_token{};
    std::uint64_t entry_owner_revision{};
    std::uint64_t exit_owner_revision{};
    std::uint64_t pre_monotonic_tick{};
    std::uint64_t post_monotonic_tick{};
    std::uintptr_t caller_rva{};
    OwnerGenerationSet owner_entry{};
    OwnerGenerationSet owner_exit{};
    std::uint32_t producer_thread_id{};
    std::uint32_t required_owner_mask{};
    std::uint32_t current_owner_mask_at_entry{};
    std::uint32_t current_owner_mask_at_exit{};
    PcSurface entry_surface{PcSurface::count};
    PcSurface return_surface{PcSurface::count};
    NativeParticipant participant{NativeParticipant::count};
    OwnerValidationState owner_validation{OwnerValidationState::not_published};
    BindingState binding{BindingState::materialization_unobserved};
    bool exact_owner_at_entry{};
    bool same_owner_at_publication{};
};

enum class CohortPhase : std::uint8_t {
    detached,
    installing,
    running,
    quiescing,
    protected_retained,
};

enum class OwnerResult : std::uint8_t {
    success,
    wrong_phase,
    invalid_cohort,
    invalid_epoch,
    invalid_participant,
    original_missing,
    originals_incomplete,
    producer_closed,
    generation_unavailable,
    call_id_exhausted,
    active_calls,
    drain_open,
    drain_active,
    drain_incomplete,
};

enum class ProtectedDetachDisposition : std::uint8_t { removed, deferred, failed };

struct OwnerSnapshot final {
    CohortPhase phase{CohortPhase::detached};
    std::uint64_t capture_epoch{};
    std::uint64_t last_detached_epoch{};
    std::uint64_t published_original_mask{};
    std::array<std::uint32_t, kNativeParticipantCount> active_calls{};
    std::uint32_t active_drains{};
    bool producer_open{};
    bool drain_open{};
    bool drain_complete{};
};

class CaptureCohortOwner;
class CaptureQueueAggregate;

class ParticipantLease final {
public:
    ParticipantLease() noexcept = default;
    ~ParticipantLease() noexcept;
    ParticipantLease(const ParticipantLease&) = delete;
    ParticipantLease& operator=(const ParticipantLease&) = delete;
    ParticipantLease(ParticipantLease&& other) noexcept;
    ParticipantLease& operator=(ParticipantLease&& other) noexcept;

    [[nodiscard]] bool active() const noexcept { return owner_ != nullptr; }
    [[nodiscard]] NativeParticipant participant() const noexcept { return participant_; }
    [[nodiscard]] const CaptureProvenance& entry_provenance() const noexcept {
        return provenance_;
    }
    [[nodiscard]] bool accepts_observation() const noexcept { return observe_; }

    template <typename Function>
    [[nodiscard]] Function original_as() const noexcept {
        static_assert(std::is_pointer_v<Function>);
        return reinterpret_cast<Function>(original_address_);
    }

private:
    friend class CaptureCohortOwner;
    template <typename, std::size_t>
    friend class BoundedCaptureQueue;
    void release() noexcept;
    [[nodiscard]] bool revalidate_for_publication(CaptureProvenance& provenance,
                                                  std::uint64_t postTick) noexcept;

    CaptureCohortOwner* owner_{};
    CaptureProvenance provenance_{};
    std::uintptr_t original_address_{};
    NativeParticipant participant_{NativeParticipant::count};
    bool observe_{};
};

class DrainLease final {
public:
    DrainLease() noexcept = default;
    ~DrainLease() noexcept;
    DrainLease(const DrainLease&) = delete;
    DrainLease& operator=(const DrainLease&) = delete;
    DrainLease(DrainLease&& other) noexcept;
    DrainLease& operator=(DrainLease&& other) noexcept;
    [[nodiscard]] bool active() const noexcept { return owner_ != nullptr; }

private:
    friend class CaptureCohortOwner;
    void release() noexcept;
    CaptureCohortOwner* owner_{};
};

class CaptureCohortOwner final {
public:
    CaptureCohortOwner() noexcept;
    CaptureCohortOwner(const CaptureCohortOwner&) = delete;
    CaptureCohortOwner& operator=(const CaptureCohortOwner&) = delete;

    [[nodiscard]] OwnerResult begin_install(const ValidatedRuntimeCohort& cohort,
                                            std::uint64_t freshEpoch) noexcept;
    [[nodiscard]] OwnerResult publish_original(NativeParticipant participant,
                                               std::uintptr_t original) noexcept;
    [[nodiscard]] OwnerResult publish_generations(const OwnerGenerationSet& snapshot) noexcept;
    [[nodiscard]] OwnerResult start_running() noexcept;
    [[nodiscard]] OwnerResult try_enter(NativeParticipant participant,
                                        CaptureTiming timing,
                                        ParticipantLease& output) noexcept;
    void quiesce() noexcept;
    void close_drain_admission() noexcept;
    [[nodiscard]] OwnerResult try_enter_drain(DrainLease& output) noexcept;
    [[nodiscard]] OwnerResult mark_drain_complete_if_empty(
        const CaptureQueueAggregate& queues) noexcept;
    [[nodiscard]] OwnerResult protected_detach(ProtectedDetachDisposition disposition) noexcept;
    [[nodiscard]] OwnerResult reopen_retained_for_detach() noexcept;
    [[nodiscard]] OwnerSnapshot snapshot() const noexcept;

private:
    friend class ParticipantLease;
    friend class DrainLease;
    [[nodiscard]] bool read_generations(OwnerGenerationSet& output,
                                        std::uint64_t& revision) const noexcept;
    [[nodiscard]] OwnerResult mark_drain_complete() noexcept;
    void leave(NativeParticipant participant) noexcept;
    void leave_drain() noexcept;

    std::atomic<CohortPhase> phase_{CohortPhase::detached};
    std::array<std::atomic<std::uintptr_t>, kNativeParticipantCount> originals_{};
    std::array<std::atomic<std::uint32_t>, kNativeParticipantCount> active_calls_{};
    std::atomic_uint32_t active_drains_{};
    std::atomic_bool producer_open_{};
    std::atomic_bool drain_open_{};
    std::atomic_bool drain_complete_{};
    std::atomic_uint64_t next_call_id_{1U};
    std::atomic_uint64_t generation_revision_{};
    std::array<std::atomic<std::uint64_t>, 14U> generation_words_{};
    std::uint64_t owner_instance_id_{};
    std::uint64_t capture_epoch_{};
    std::uint64_t last_detached_epoch_{};
    std::uint64_t build_id_{};
    std::uint64_t cohort_id_{};
    std::uint64_t required_original_mask_{};
};

enum class CaptureBuildResult : std::uint8_t {
    ready,
    complete,
    partial,
    null_pointer,
    unreadable,
    wrong_type,
    wrong_participant,
    identity_mismatch,
    no_parent_dispatch,
    arguments_do_not_match_parent,
    timing_invalid,
    consumed,
};

struct ResolverCaptureRecord final {
    CaptureProvenance provenance{};
    std::uint64_t sequence{};
    std::uintptr_t runtime_identity{};
    std::uintptr_t record_identity{};
    std::uint16_t runtime_id_pre{};
    std::uint8_t prior_result{};
    std::uint8_t original_result{};
    RuntimeRecordImage record_pre{};
    RuntimeRecordImage record_post{};
    Type69Payload payload_pre{};
    Type69Payload payload_post{};
    std::uint64_t record_pre_hash{};
    std::uint64_t record_post_hash{};
    bool pre_valid{};
    bool post_valid{};
};

enum class DispatchKind : std::uint8_t { full_apply, paired_short_form };

struct DispatchCaptureRecord final {
    CaptureProvenance provenance{};
    std::uint64_t sequence{};
    std::uintptr_t activity_runtime_identity{};
    std::uintptr_t descriptor_identity{};
    std::uintptr_t dynamic_context_identity{};
    DescriptorProjection descriptor_pre{};
    DescriptorProjection descriptor_post{};
    std::uint64_t descriptor_pre_hash{};
    std::uint64_t descriptor_post_hash{};
    DispatchKind kind{DispatchKind::full_apply};
    bool pre_valid{};
    bool post_valid{};
};

enum class ResolutionValidity : std::uint32_t {
    pair_readable = 1U << 0U,
    interface_datum = 1U << 1U,
    endpoint_object = 1U << 2U,
    interface_metadata = 1U << 3U,
    adjusted_this = 1U << 4U,
    concrete_method = 1U << 5U,
};

struct SubscriberResolutionCandidate final {
    std::uintptr_t interface_pair{};
    std::uintptr_t interface_datum{};
    std::uintptr_t endpoint_object{};
    std::uintptr_t interface_metadata{};
    std::uintptr_t adjusted_this{};
    std::uintptr_t concrete_method{};
    std::uint32_t validity_mask{};
    std::uint32_t method_slot{};
};

enum class SubscriberKind : std::uint8_t { full_slot_30, short_slot_48 };

struct CallsiteCaptureRecord final {
    CaptureProvenance provenance{};
    std::uint64_t sequence{};
    SubscriberResolutionCandidate resolution{};
    DescriptorProjection parent_descriptor{};
    std::array<float, 4U> payload4{};
    std::int32_t resolved_value{};
    std::uint32_t auxiliary{};
    std::uint8_t operation{};
    SubscriberKind kind{SubscriberKind::full_slot_30};
    bool arguments_match_parent{};
    bool return_observed{};
};

struct EndpointBinderCaptureRecord final {
    CaptureProvenance provenance{};
    std::uint64_t sequence{};
    std::uintptr_t endpoint_owner{};
    std::uint32_t requested_count{};
    std::uint32_t container_handle_pre{};
    std::uint32_t container_handle_post{};
    std::uint32_t cleared_field_pre{};
    std::uint32_t cleared_field_post{};
    std::uint32_t owner_count_pre{};
    std::uint32_t owner_count_post{};
    bool original_result{};
    bool pre_valid{};
    bool post_valid{};
};

struct EndpointRegistrationCaptureRecord final {
    CaptureProvenance provenance{};
    std::uint64_t sequence{};
    std::uintptr_t row_identity{};
    std::uintptr_t interface_pair{};
    std::uint32_t owner_handle{};
    std::uint32_t observed_ordinal{};
    RuntimeEndpointKey40 runtime_key_pre{};
    RuntimeEndpointKey40 runtime_key_post{};
    std::uint64_t runtime_key_pre_hash{};
    std::uint64_t runtime_key_post_hash{};
    SubscriberResolutionCandidate registration_candidate{};
    bool pre_valid{};
    bool post_valid{};
    bool return_observed{};
};

using Type68DecodedImage = std::array<std::byte, kType68DecodedBytes>;
using Type68RoundedWireImage = std::array<std::byte, kType68RoundedBytes>;

struct Type68SerializerCaptureRecord final {
    CaptureProvenance provenance{};
    std::uint64_t sequence{};
    std::uintptr_t decoded_identity{};
    std::uintptr_t output_identity{};
    std::uintptr_t output_bytes_identity{};
    Type68DecodedImage decoded_pre{};
    Type68DecodedImage decoded_post{};
    Type68RoundedWireImage wire_post{};
    std::uint64_t decoded_pre_hash{};
    std::uint64_t decoded_post_hash{};
    std::uint64_t rounded_wire_hash{};
    std::uint32_t schema{};
    std::uint32_t cursor_bits{};
    std::int32_t output_bytes{};
    bool original_result{};
    bool writer_error{};
    bool cursor_observed{};
    bool decoded_post_valid{};
    bool wire_post_valid{};
    bool exact_type68_shape{};
};

class PendingResolverCapture final {
public:
    PendingResolverCapture() noexcept = default;
    PendingResolverCapture(const PendingResolverCapture&) = delete;
    PendingResolverCapture& operator=(const PendingResolverCapture&) = delete;

private:
    friend CaptureBuildResult prepare_resolver_capture(PendingResolverCapture&,
                                                       const ParticipantLease&,
                                                       const std::uint16_t*,
                                                       const void*,
                                                       std::uint8_t) noexcept;
    friend CaptureBuildResult finish_resolver_capture(PendingResolverCapture&,
                                                      const ParticipantLease&,
                                                      const void*,
                                                      std::uint8_t,
                                                      std::uint64_t,
                                                      ResolverCaptureRecord&) noexcept;
    CaptureProvenance provenance_{};
    std::uintptr_t runtime_identity_{};
    std::uintptr_t record_identity_{};
    std::uint16_t runtime_id_pre_{};
    std::uint8_t prior_result_{};
    RuntimeRecordImage record_pre_{};
    Type69Payload payload_pre_{};
    bool ready_{};
};

class PendingDispatchCapture final {
public:
    PendingDispatchCapture() noexcept = default;
    ~PendingDispatchCapture() noexcept;
    PendingDispatchCapture(const PendingDispatchCapture&) = delete;
    PendingDispatchCapture& operator=(const PendingDispatchCapture&) = delete;

private:
    friend CaptureBuildResult prepare_dispatch_capture(PendingDispatchCapture&,
                                                       const ParticipantLease&,
                                                       const void*,
                                                       const void*,
                                                       const void*) noexcept;
    friend CaptureBuildResult finish_dispatch_capture(PendingDispatchCapture&,
                                                      const ParticipantLease&,
                                                      const void*,
                                                      std::uint64_t,
                                                      DispatchCaptureRecord&) noexcept;
    CaptureProvenance provenance_{};
    std::uintptr_t activity_runtime_identity_{};
    std::uintptr_t descriptor_identity_{};
    std::uintptr_t dynamic_context_identity_{};
    DescriptorProjection descriptor_pre_{};
    DispatchKind kind_{DispatchKind::full_apply};
    std::size_t stack_depth_{};
    bool stack_pushed_{};
    bool ready_{};
};

class PendingCallsiteCapture final {
public:
    PendingCallsiteCapture() noexcept = default;
    PendingCallsiteCapture(const PendingCallsiteCapture&) = delete;
    PendingCallsiteCapture& operator=(const PendingCallsiteCapture&) = delete;

private:
    friend CaptureBuildResult prepare_full_callsite_capture(PendingCallsiteCapture&,
                                                            const ParticipantLease&,
                                                            const void*,
                                                            std::uint8_t,
                                                            std::int32_t,
                                                            const float*,
                                                            std::uint32_t) noexcept;
    friend CaptureBuildResult prepare_short_callsite_capture(PendingCallsiteCapture&,
                                                             const ParticipantLease&,
                                                             const void*,
                                                             std::uint32_t) noexcept;
    friend CaptureBuildResult finish_callsite_capture(PendingCallsiteCapture&,
                                                      const ParticipantLease&,
                                                      std::uint64_t,
                                                      CallsiteCaptureRecord&) noexcept;
    CallsiteCaptureRecord record_{};
    bool ready_{};
};

class PendingEndpointBinderCapture final {
public:
    PendingEndpointBinderCapture() noexcept = default;
    ~PendingEndpointBinderCapture() noexcept;
    PendingEndpointBinderCapture(const PendingEndpointBinderCapture&) = delete;
    PendingEndpointBinderCapture& operator=(const PendingEndpointBinderCapture&) = delete;

private:
    friend CaptureBuildResult prepare_endpoint_binder_capture(
        PendingEndpointBinderCapture&, const ParticipantLease&, const void*, std::uint32_t) noexcept;
    friend CaptureBuildResult finish_endpoint_binder_capture(
        PendingEndpointBinderCapture&, const ParticipantLease&, const void*, bool, std::uint64_t,
        EndpointBinderCaptureRecord&) noexcept;
    CaptureProvenance provenance_{};
    std::uintptr_t endpoint_owner_{};
    std::uint32_t requested_count_{};
    std::uint32_t container_handle_pre_{};
    std::uint32_t cleared_field_pre_{};
    std::uint32_t owner_count_pre_{};
    std::size_t stack_depth_{};
    bool stack_pushed_{};
    bool ready_{};
};

class PendingEndpointRegistrationCapture final {
public:
    PendingEndpointRegistrationCapture() noexcept = default;
    PendingEndpointRegistrationCapture(const PendingEndpointRegistrationCapture&) = delete;
    PendingEndpointRegistrationCapture& operator=(const PendingEndpointRegistrationCapture&) = delete;

private:
    friend CaptureBuildResult prepare_endpoint_registration_capture(
        PendingEndpointRegistrationCapture&, const ParticipantLease&, const void*,
        std::uint32_t) noexcept;
    friend CaptureBuildResult finish_endpoint_registration_capture(
        PendingEndpointRegistrationCapture&, const ParticipantLease&, std::uint64_t,
        EndpointRegistrationCaptureRecord&) noexcept;
    EndpointRegistrationCaptureRecord record_{};
    bool ready_{};
};

class PendingType68SerializerCapture final {
public:
    PendingType68SerializerCapture() noexcept = default;
    PendingType68SerializerCapture(const PendingType68SerializerCapture&) = delete;
    PendingType68SerializerCapture& operator=(const PendingType68SerializerCapture&) = delete;

private:
    friend CaptureBuildResult prepare_type68_serializer_capture(
        PendingType68SerializerCapture&, const ParticipantLease&, std::uint32_t,
        const void*, const void*, const std::int32_t*) noexcept;
    friend CaptureBuildResult observe_type68_serializer_cursor(
        PendingType68SerializerCapture&, const ParticipantLease&, std::uint32_t,
        bool) noexcept;
    friend CaptureBuildResult finish_type68_serializer_capture(
        PendingType68SerializerCapture&, const ParticipantLease&, const void*, const void*,
        const std::int32_t*, bool, std::uint64_t, Type68SerializerCaptureRecord&) noexcept;
    Type68SerializerCaptureRecord record_{};
    bool ready_{};
};

[[nodiscard]] CaptureBuildResult prepare_resolver_capture(
    PendingResolverCapture& pending,
    const ParticipantLease& lease,
    const std::uint16_t* activityRuntimeId,
    const void* record,
    std::uint8_t priorResult) noexcept;
[[nodiscard]] CaptureBuildResult finish_resolver_capture(
    PendingResolverCapture& pending,
    const ParticipantLease& lease,
    const void* record,
    std::uint8_t originalResult,
    std::uint64_t postMonotonicTick,
    ResolverCaptureRecord& output) noexcept;

[[nodiscard]] CaptureBuildResult prepare_dispatch_capture(
    PendingDispatchCapture& pending,
    const ParticipantLease& lease,
    const void* activityRuntime,
    const void* descriptor,
    const void* dynamicContext) noexcept;
[[nodiscard]] CaptureBuildResult finish_dispatch_capture(
    PendingDispatchCapture& pending,
    const ParticipantLease& lease,
    const void* descriptor,
    std::uint64_t postMonotonicTick,
    DispatchCaptureRecord& output) noexcept;

[[nodiscard]] CaptureBuildResult prepare_full_callsite_capture(
    PendingCallsiteCapture& pending,
    const ParticipantLease& lease,
    const void* interfacePair,
    std::uint8_t operation,
    std::int32_t resolvedValue,
    const float* payload4,
    std::uint32_t auxiliary) noexcept;
[[nodiscard]] CaptureBuildResult prepare_short_callsite_capture(
    PendingCallsiteCapture& pending,
    const ParticipantLease& lease,
    const void* interfacePair,
    std::uint32_t auxiliary) noexcept;
[[nodiscard]] CaptureBuildResult finish_callsite_capture(
    PendingCallsiteCapture& pending,
    const ParticipantLease& lease,
    std::uint64_t postMonotonicTick,
    CallsiteCaptureRecord& output) noexcept;

[[nodiscard]] CaptureBuildResult prepare_endpoint_binder_capture(
    PendingEndpointBinderCapture& pending,
    const ParticipantLease& lease,
    const void* endpointOwner,
    std::uint32_t count) noexcept;
[[nodiscard]] CaptureBuildResult finish_endpoint_binder_capture(
    PendingEndpointBinderCapture& pending,
    const ParticipantLease& lease,
    const void* endpointOwner,
    bool originalResult,
    std::uint64_t postMonotonicTick,
    EndpointBinderCaptureRecord& output) noexcept;
[[nodiscard]] CaptureBuildResult prepare_endpoint_registration_capture(
    PendingEndpointRegistrationCapture& pending,
    const ParticipantLease& lease,
    const void* interfacePair,
    std::uint32_t ownerHandle) noexcept;
[[nodiscard]] CaptureBuildResult finish_endpoint_registration_capture(
    PendingEndpointRegistrationCapture& pending,
    const ParticipantLease& lease,
    std::uint64_t postMonotonicTick,
    EndpointRegistrationCaptureRecord& output) noexcept;
[[nodiscard]] CaptureBuildResult prepare_type68_serializer_capture(
    PendingType68SerializerCapture& pending,
    const ParticipantLease& lease,
    std::uint32_t schema,
    const void* decoded,
    const void* outputBuffer,
    const std::int32_t* outputBytes) noexcept;
/** Called only at pinned PC +0x4DBFF7 with the active serializer lease. */
[[nodiscard]] CaptureBuildResult observe_type68_serializer_cursor(
    PendingType68SerializerCapture& pending,
    const ParticipantLease& lease,
    std::uint32_t cursorBits,
    bool writerError) noexcept;
[[nodiscard]] CaptureBuildResult finish_type68_serializer_capture(
    PendingType68SerializerCapture& pending,
    const ParticipantLease& lease,
    const void* decoded,
    const void* outputBuffer,
    const std::int32_t* outputBytes,
    bool originalResult,
    std::uint64_t postMonotonicTick,
    Type68SerializerCaptureRecord& output) noexcept;

[[nodiscard]] bool valid_raw_record(const ResolverCaptureRecord& record) noexcept;
[[nodiscard]] bool valid_raw_record(const DispatchCaptureRecord& record) noexcept;
[[nodiscard]] bool valid_raw_record(const CallsiteCaptureRecord& record) noexcept;
[[nodiscard]] bool valid_raw_record(const EndpointBinderCaptureRecord& record) noexcept;
[[nodiscard]] bool valid_raw_record(const EndpointRegistrationCaptureRecord& record) noexcept;
[[nodiscard]] bool valid_raw_record(const Type68SerializerCaptureRecord& record) noexcept;

enum class QueuePushResult : std::uint8_t {
    enqueued,
    rejected,
    full,
    busy,
    closed,
    sequence_exhausted,
};
enum class QueuePopResult : std::uint8_t { success, empty, busy, closed };

struct QueueCounters final {
    std::uint64_t accepted{};
    std::uint64_t rejected{};
    std::uint64_t dropped_full{};
    std::uint64_t dropped_busy{};
    std::uint64_t dropped_closed{};
    std::uint64_t dropped_sequence_exhausted{};
    [[nodiscard]] constexpr std::uint64_t losses() const noexcept {
        return rejected + dropped_full + dropped_busy + dropped_closed
               + dropped_sequence_exhausted;
    }
};

/** Per-slot MPSC reserve/commit: no producer-wide copy lock and one record copy per publish. */
template <typename Record, std::size_t Capacity>
class BoundedCaptureQueue final {
    static_assert(Capacity >= 2U && (Capacity & (Capacity - 1U)) == 0U);

    struct Slot final {
        std::atomic_uint64_t turn{};
        Record record{};
    };

public:
    explicit BoundedCaptureQueue(std::uint64_t epoch = 1U) noexcept : epoch_(epoch) {
        for (std::size_t index = 0U; index < Capacity; ++index) {
            slots_[index].turn.store(index, std::memory_order_relaxed);
        }
    }
    BoundedCaptureQueue(const BoundedCaptureQueue&) = delete;
    BoundedCaptureQueue& operator=(const BoundedCaptureQueue&) = delete;

    [[nodiscard]] QueuePushResult try_publish(const Record& record,
                                              ParticipantLease& lease,
                                              std::uint64_t postTick) noexcept {
        if (!valid_raw_record(record)
            || record.provenance.call_id != lease.entry_provenance().call_id
            || record.provenance.owner_instance_id
                   != lease.entry_provenance().owner_instance_id) {
            rejected_.fetch_add(1U, std::memory_order_relaxed);
            return QueuePushResult::rejected;
        }
        if (!producer_open_.load(std::memory_order_acquire)) {
            dropped_closed_.fetch_add(1U, std::memory_order_relaxed);
            return QueuePushResult::closed;
        }
        std::uint64_t position = enqueue_position_.load(std::memory_order_relaxed);
        if (position >= (std::numeric_limits<std::uint64_t>::max)() - Capacity) {
            dropped_sequence_exhausted_.fetch_add(1U, std::memory_order_relaxed);
            return QueuePushResult::sequence_exhausted;
        }
        Slot& slot = slots_[static_cast<std::size_t>(position & (Capacity - 1U))];
        const std::uint64_t turn = slot.turn.load(std::memory_order_acquire);
        const auto difference = static_cast<std::int64_t>(turn - position);
        if (difference < 0) {
            dropped_full_.fetch_add(1U, std::memory_order_relaxed);
            return QueuePushResult::full;
        }
        if (difference != 0
            || !enqueue_position_.compare_exchange_strong(position,
                                                          position + 1U,
                                                          std::memory_order_acq_rel,
                                                          std::memory_order_relaxed)) {
            dropped_busy_.fetch_add(1U, std::memory_order_relaxed);
            return QueuePushResult::busy;
        }
        slot.record = record;
        slot.record.sequence = position + 1U;
        (void)lease.revalidate_for_publication(slot.record.provenance, postTick);
        slot.turn.store(position + 1U, std::memory_order_release);
        accepted_.fetch_add(1U, std::memory_order_relaxed);
        return QueuePushResult::enqueued;
    }

    [[nodiscard]] QueuePopResult try_pop(Record& output) noexcept {
        std::uint64_t position = dequeue_position_.load(std::memory_order_relaxed);
        Slot& slot = slots_[static_cast<std::size_t>(position & (Capacity - 1U))];
        const std::uint64_t turn = slot.turn.load(std::memory_order_acquire);
        const auto difference = static_cast<std::int64_t>(turn - (position + 1U));
        if (difference < 0) {
            return consumer_open_.load(std::memory_order_acquire) ? QueuePopResult::empty
                                                                  : QueuePopResult::closed;
        }
        if (difference != 0
            || !dequeue_position_.compare_exchange_strong(position,
                                                          position + 1U,
                                                          std::memory_order_acq_rel,
                                                          std::memory_order_relaxed)) {
            return QueuePopResult::busy;
        }
        output = slot.record;
        slot.turn.store(position + Capacity, std::memory_order_release);
        return QueuePopResult::success;
    }

    void close_producers() noexcept { producer_open_.store(false, std::memory_order_release); }
    void close_consumer() noexcept { consumer_open_.store(false, std::memory_order_release); }
    [[nodiscard]] bool empty() const noexcept {
        return enqueue_position_.load(std::memory_order_acquire)
               == dequeue_position_.load(std::memory_order_acquire);
    }
    [[nodiscard]] std::uint64_t epoch() const noexcept { return epoch_; }
    [[nodiscard]] bool reset(std::uint64_t freshEpoch) noexcept {
        if (producer_open_.load(std::memory_order_acquire)
            || consumer_open_.load(std::memory_order_acquire) || !empty()
            || freshEpoch == 0U || freshEpoch <= epoch_) {
            return false;
        }
        enqueue_position_.store(0U, std::memory_order_relaxed);
        dequeue_position_.store(0U, std::memory_order_relaxed);
        for (std::size_t index = 0U; index < Capacity; ++index) {
            slots_[index].record = {};
            slots_[index].turn.store(index, std::memory_order_relaxed);
        }
        epoch_ = freshEpoch;
        producer_open_.store(true, std::memory_order_release);
        consumer_open_.store(true, std::memory_order_release);
        return true;
    }
    [[nodiscard]] QueueCounters counters() const noexcept {
        return {accepted_.load(std::memory_order_relaxed),
                rejected_.load(std::memory_order_relaxed),
                dropped_full_.load(std::memory_order_relaxed),
                dropped_busy_.load(std::memory_order_relaxed),
                dropped_closed_.load(std::memory_order_relaxed),
                dropped_sequence_exhausted_.load(std::memory_order_relaxed)};
    }

#if defined(SUNRISE_TYPE69_CAPTURE_TESTING)
    void testing_set_enqueue_position(std::uint64_t value) noexcept {
        enqueue_position_.store(value, std::memory_order_relaxed);
    }
#endif

private:
    std::array<Slot, Capacity> slots_{};
    std::atomic_uint64_t enqueue_position_{};
    std::atomic_uint64_t dequeue_position_{};
    std::atomic_bool producer_open_{true};
    std::atomic_bool consumer_open_{true};
    std::atomic_uint64_t accepted_{};
    std::atomic_uint64_t rejected_{};
    std::atomic_uint64_t dropped_full_{};
    std::atomic_uint64_t dropped_busy_{};
    std::atomic_uint64_t dropped_closed_{};
    std::atomic_uint64_t dropped_sequence_exhausted_{};
    std::uint64_t epoch_{};
};

inline constexpr std::size_t kResolverQueueCapacity = 32U;
inline constexpr std::size_t kDispatchQueueCapacity = 64U;
inline constexpr std::size_t kCallsiteQueueCapacity = 64U;
inline constexpr std::size_t kEndpointBinderQueueCapacity = 16U;
inline constexpr std::size_t kEndpointRegistrationQueueCapacity = 64U;
inline constexpr std::size_t kType68SerializerQueueCapacity = 16U;
using ResolverCaptureQueue = BoundedCaptureQueue<ResolverCaptureRecord, kResolverQueueCapacity>;
using DispatchCaptureQueue = BoundedCaptureQueue<DispatchCaptureRecord, kDispatchQueueCapacity>;
using CallsiteCaptureQueue = BoundedCaptureQueue<CallsiteCaptureRecord, kCallsiteQueueCapacity>;
using EndpointBinderCaptureQueue =
    BoundedCaptureQueue<EndpointBinderCaptureRecord, kEndpointBinderQueueCapacity>;
using EndpointRegistrationCaptureQueue =
    BoundedCaptureQueue<EndpointRegistrationCaptureRecord, kEndpointRegistrationQueueCapacity>;
using Type68SerializerCaptureQueue =
    BoundedCaptureQueue<Type68SerializerCaptureRecord, kType68SerializerQueueCapacity>;

/** The lifecycle drain oracle owns the complete queue set; callers cannot omit a queue. */
class CaptureQueueAggregate final {
public:
    explicit CaptureQueueAggregate(std::uint64_t epoch = 1U) noexcept
        : resolver(epoch), dispatch(epoch), callsite(epoch), endpoint_binder(epoch),
          endpoint_registration(epoch), type68_serializer(epoch), epoch_(epoch) {}
    CaptureQueueAggregate(const CaptureQueueAggregate&) = delete;
    CaptureQueueAggregate& operator=(const CaptureQueueAggregate&) = delete;

    void close_producers() noexcept {
        resolver.close_producers();
        dispatch.close_producers();
        callsite.close_producers();
        endpoint_binder.close_producers();
        endpoint_registration.close_producers();
        type68_serializer.close_producers();
    }
    void close_consumers() noexcept {
        resolver.close_consumer();
        dispatch.close_consumer();
        callsite.close_consumer();
        endpoint_binder.close_consumer();
        endpoint_registration.close_consumer();
        type68_serializer.close_consumer();
    }
    [[nodiscard]] bool empty() const noexcept {
        return resolver.empty() && dispatch.empty() && callsite.empty()
               && endpoint_binder.empty() && endpoint_registration.empty()
               && type68_serializer.empty();
    }
    [[nodiscard]] std::uint64_t epoch() const noexcept { return epoch_; }
    [[nodiscard]] bool reset(std::uint64_t freshEpoch) noexcept {
        if (freshEpoch == 0U || freshEpoch <= epoch_) {
            return false;
        }
        if (!resolver.reset(freshEpoch) || !dispatch.reset(freshEpoch)
            || !callsite.reset(freshEpoch) || !endpoint_binder.reset(freshEpoch)
            || !endpoint_registration.reset(freshEpoch)
            || !type68_serializer.reset(freshEpoch)) {
            return false;
        }
        epoch_ = freshEpoch;
        return true;
    }

    ResolverCaptureQueue resolver;
    DispatchCaptureQueue dispatch;
    CallsiteCaptureQueue callsite;
    EndpointBinderCaptureQueue endpoint_binder;
    EndpointRegistrationCaptureQueue endpoint_registration;
    Type68SerializerCaptureQueue type68_serializer;

private:
    std::uint64_t epoch_{};
};

[[nodiscard]] std::uint64_t bounded_hash(std::span<const std::byte> bytes) noexcept;

struct TelemetryProvenance final {
    std::uint64_t build_id{};
    std::uint64_t cohort_id{};
    std::uint64_t capture_epoch{};
    std::uint64_t owner_instance_id{};
    std::uint64_t call_id{};
    std::uint64_t parent_call_id{};
    std::uint64_t materialization_token{};
    std::uint64_t queue_sequence{};
    std::uint64_t entry_owner_revision{};
    std::uint64_t exit_owner_revision{};
    std::uint64_t pre_monotonic_tick{};
    std::uint64_t post_monotonic_tick{};
    std::uint64_t duration_ticks{};
    std::uintptr_t caller_rva{};
    std::uintptr_t entry_rva{};
    std::uintptr_t return_rva{};
    OwnerGenerationSet owner_entry{};
    OwnerGenerationSet owner_exit{};
    std::uint32_t producer_thread_id{};
    std::uint32_t required_owner_mask{};
    std::uint32_t entry_presence_mask{};
    std::uint32_t exit_presence_mask{};
    std::uint32_t current_owner_mask_at_entry{};
    std::uint32_t current_owner_mask_at_exit{};
    PcSurface entry_surface{PcSurface::count};
    PcSurface return_surface{PcSurface::count};
    NativeParticipant participant{NativeParticipant::count};
    OwnerValidationState owner_validation{OwnerValidationState::not_published};
    BindingState binding{BindingState::materialization_unobserved};
    StaticInterfacePlatform platform{StaticInterfacePlatform::pc_win64};
    bool exact_owner_at_entry{};
    bool same_owner_at_publication{};
};

struct ResolverTelemetry final {
    TelemetryProvenance provenance{};
    std::uint64_t pre_hash{};
    std::uint64_t post_hash{};
    std::int32_t endpoint_index{};
    std::uint32_t auxiliary{};
    std::int32_t selectable_low{};
    std::int32_t selectable_high{};
    std::array<float, 3U> payload_xyz{};
    std::uint8_t operation{};
    std::uint8_t original_result{};
    bool post_valid{};
    bool failure_sentinel{};
};

struct DispatchTelemetry final {
    TelemetryProvenance provenance{};
    std::uint64_t pre_hash{};
    std::uint64_t post_hash{};
    std::int32_t endpoint_index{};
    std::uint32_t auxiliary{};
    std::uint8_t operation{};
    DispatchKind kind{DispatchKind::full_apply};
    bool post_valid{};
};

struct CallsiteTelemetry final {
    TelemetryProvenance provenance{};
    std::uint64_t interface_candidate_hash{};
    std::int32_t endpoint_index{};
    std::int32_t resolved_value{};
    std::uint32_t auxiliary{};
    std::uint32_t resolution_validity_mask{};
    std::uint32_t method_slot{};
    std::uint8_t operation{};
    SubscriberKind kind{SubscriberKind::full_slot_30};
    bool arguments_match_parent{};
    bool return_observed{};
};

struct Type68SerializerTelemetry final {
    TelemetryProvenance provenance{};
    std::uint64_t decoded_pre_hash{};
    std::uint64_t decoded_post_hash{};
    std::uint64_t rounded_wire_hash{};
    std::uint32_t schema{};
    std::uint32_t cursor_bits{};
    std::int32_t output_bytes{};
    bool original_result{};
    bool writer_error{};
    bool exact_type68_shape{};
};

[[nodiscard]] bool default_telemetry(const ResolverCaptureRecord& record,
                                     ResolverTelemetry& output) noexcept;
[[nodiscard]] bool default_telemetry(const DispatchCaptureRecord& record,
                                     DispatchTelemetry& output) noexcept;
[[nodiscard]] bool default_telemetry(const CallsiteCaptureRecord& record,
                                     CallsiteTelemetry& output) noexcept;
[[nodiscard]] bool default_telemetry(const Type68SerializerCaptureRecord& record,
                                     Type68SerializerTelemetry& output) noexcept;

template <typename Function, typename Before, typename After, typename... Arguments>
void forward_void_original_once(ParticipantLease& lease,
                                Before&& before,
                                After&& after,
                                Arguments... arguments) noexcept {
    static_assert(std::is_pointer_v<Function>);
    static_assert(std::is_void_v<std::invoke_result_t<Function, Arguments...>>);
    static_assert(std::is_nothrow_invocable_v<Before>);
    static_assert(std::is_nothrow_invocable_v<After>);
    const Function original = lease.original_as<Function>();
    if (lease.accepts_observation()) {
        std::invoke(std::forward<Before>(before));
    }
    std::invoke(original, arguments...);
    if (lease.accepts_observation()) {
        std::invoke(std::forward<After>(after));
    }
}

template <typename Function, typename Before, typename After, typename... Arguments>
[[nodiscard]] std::invoke_result_t<Function, Arguments...> forward_value_original_once(
    ParticipantLease& lease,
    Before&& before,
    After&& after,
    Arguments... arguments) noexcept {
    static_assert(std::is_pointer_v<Function>);
    static_assert(!std::is_void_v<std::invoke_result_t<Function, Arguments...>>);
    static_assert(std::is_nothrow_invocable_v<Before>);
    const Function original = lease.original_as<Function>();
    if (lease.accepts_observation()) {
        std::invoke(std::forward<Before>(before));
    }
    auto result = std::invoke(original, arguments...);
    static_assert(std::is_nothrow_invocable_v<After, decltype(result)>);
    if (lease.accepts_observation()) {
        std::invoke(std::forward<After>(after), result);
    }
    return result;
}

#if defined(SUNRISE_TYPE69_CAPTURE_TESTING)
struct CaptureTestAccess final {
    [[nodiscard]] static ValidatedRuntimeCohort validated_cohort() noexcept;
};
#endif

static_assert(sizeof(void*) == 8U);
static_assert(sizeof(Type69Payload) == kType69PayloadBytes);
static_assert(sizeof(Type68SerializerCaptureRecord) > 736U);
static_assert(offsetof(Type69Payload, operation) == 0x08U);
static_assert(offsetof(Type69Payload, selectable_low) == 0x0CU);
static_assert(offsetof(Type69Payload, payload_xyz) == 0x14U);
static_assert(std::atomic<std::uint64_t>::is_always_lock_free);

} // namespace sunrise::client::hooks::bootflow::opening_authority::type69_capture
