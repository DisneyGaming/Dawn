#pragma once

#include <array>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <span>
#include <type_traits>
#include <utility>

namespace dawn::client::hooks::bootflow::opening_authority::type31_incident {

/**
 * Restricted, source-only support for observing the pinned PC Type-31 incident path.
 *
 * The native callback path performs no file/network I/O and owns no detour or writer. Raw capture
 * bytes are restricted local RE evidence. Only DefaultProjection is privacy-reviewed for ordinary
 * diagnostics, and it contains no process address, player datum/bit, session, or owner identity.
 */
inline constexpr bool kOwnsNativeDetour = false;
inline constexpr bool kObservationOnly = true;
inline constexpr bool kPerformsCapturePathIo = false;
inline constexpr bool kPerformsAdmissionFileIo = true;
inline constexpr bool kProvidesAuthorityEncoder = false;
inline constexpr bool kProvidesAuthorityWriter = false;
inline constexpr bool kProvidesStrictGate = false;
inline constexpr bool kDynamicRetailListenerKnown = false;
inline constexpr bool kSupportsWindowsMsvcX64Capture = true;

inline constexpr std::size_t kSha256Bytes = 32U;
using Sha256 = std::array<std::byte, kSha256Bytes>;

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

inline constexpr wchar_t kPinnedUnpackedProvenancePath[] =
    L"D:\\Dawn-work\\ghidra\\destiny2_unpacked.exe";
inline constexpr std::uint64_t kPinnedUnpackedProvenanceBytes = 145'091'072U;
inline constexpr std::uintptr_t kPinnedUnpackedImageBase = 0x7FF68E9A0000ULL;
inline constexpr std::uint32_t kPinnedPeSizeOfImage = 0x08A5EA00U;
inline constexpr Sha256 kPinnedUnpackedProvenanceSha256{
    std::byte{0x87}, std::byte{0x13}, std::byte{0xD1}, std::byte{0x5E}, std::byte{0x3D},
    std::byte{0x05}, std::byte{0xB2}, std::byte{0x6F}, std::byte{0x9E}, std::byte{0x25},
    std::byte{0x9E}, std::byte{0x02}, std::byte{0xB0}, std::byte{0xF2}, std::byte{0x9B},
    std::byte{0xC1}, std::byte{0xE0}, std::byte{0x00}, std::byte{0xE4}, std::byte{0xB0},
    std::byte{0xC6}, std::byte{0x2C}, std::byte{0xA2}, std::byte{0xCC}, std::byte{0x87},
    std::byte{0xC3}, std::byte{0x85}, std::byte{0x97}, std::byte{0x18}, std::byte{0x6C},
    std::byte{0xC3}, std::byte{0xBD}};

struct PackedRuntimeIdentity final {
    std::uint64_t file_bytes{};
    Sha256 sha256{};
    friend constexpr bool operator==(PackedRuntimeIdentity,
                                     PackedRuntimeIdentity) noexcept = default;
};

struct UnpackedProvenanceIdentity final {
    std::uint64_t file_bytes{};
    Sha256 sha256{};
};

[[nodiscard]] constexpr bool
matches_pinned_packed_runtime(const PackedRuntimeIdentity& identity) noexcept {
    return identity.file_bytes == kPinnedPackedRuntimeBytes
           && identity.sha256 == kPinnedPackedRuntimeSha256;
}

[[nodiscard]] constexpr bool
matches_pinned_unpacked_provenance(const UnpackedProvenanceIdentity& identity) noexcept {
    return identity.file_bytes == kPinnedUnpackedProvenanceBytes
           && identity.sha256 == kPinnedUnpackedProvenanceSha256;
}

enum class NativeSurface : std::uint8_t {
    create_init,
    authority_apply,
    local_evaluate,
    point_terminal,
    sobject_validate_dispatch,
    recursive_materialize_submit,
    consumed_token_materialize,
    manager_submit_vslot,
    recursive_visitor,
    incident_visitor_callback,
    incident_normal_route,
    dynamic_listener_enumeration,
    count,
};

inline constexpr std::size_t kNativeSurfaceCount = static_cast<std::size_t>(NativeSurface::count);

enum class NativeAbi : std::uint8_t {
    instance_create_u8,
    instance_packet,
    instance_only,
    instance_matched_object_reference,
    sobject_id_descriptor_root,
    internal_recursive_submit,
    internal_token_materializer,
    manager_incident_vslot,
    internal_reflected_object_walk,
    internal_incident_visitor,
    internal_incident_normal_route,
    internal_dynamic_listener_enumerator,
};

struct NativeBoundaryDescriptor final {
    std::uintptr_t mapped_rva{};
    std::size_t recovered_size{};
    std::span<const std::byte> mapped_prefix{};
    NativeAbi abi{};
};

[[nodiscard]] NativeBoundaryDescriptor native_boundary(NativeSurface surface) noexcept;
[[nodiscard]] bool native_prefix_matches(NativeSurface surface,
                                         std::span<const std::byte> observed) noexcept;

enum class PrefixCohortResult : std::uint8_t {
    valid,
    invalid_arguments,
    target_out_of_range,
    prefix_mismatch,
};

/** Pure prefix component. It cannot issue a trusted runtime token. */
[[nodiscard]] PrefixCohortResult validate_mapped_prefixes_untrusted(
    std::span<const std::byte> image,
    std::array<std::uintptr_t, kNativeSurfaceCount>& outputAddresses) noexcept;

class RuntimeCohortToken final {
public:
    /** The only public construction is the invalid, zero token. */
    constexpr RuntimeCohortToken() noexcept = default;
    [[nodiscard]] constexpr bool valid() const noexcept {
        return cohort_id_ != 0U && generation_ != 0U && pe_size_of_image_ == kPinnedPeSizeOfImage
               && prefix_digest_ != Sha256{};
    }
    [[nodiscard]] constexpr std::uint64_t cohort_id() const noexcept {
        return cohort_id_;
    }
    [[nodiscard]] constexpr std::uint64_t generation() const noexcept {
        return generation_;
    }
    [[nodiscard]] constexpr std::uint32_t pe_size_of_image() const noexcept {
        return pe_size_of_image_;
    }
    [[nodiscard]] constexpr const Sha256& prefix_digest() const noexcept {
        return prefix_digest_;
    }
    friend constexpr bool operator==(RuntimeCohortToken, RuntimeCohortToken) noexcept = default;

private:
    constexpr RuntimeCohortToken(std::uint64_t cohortId,
                                 std::uint64_t generation,
                                 std::uint32_t peSizeOfImage,
                                 Sha256 prefixDigest) noexcept
        : cohort_id_(cohortId), generation_(generation), pe_size_of_image_(peSizeOfImage),
          prefix_digest_(prefixDigest) {}
    std::uint64_t cohort_id_{};
    std::uint64_t generation_{};
    std::uint32_t pe_size_of_image_{};
    Sha256 prefix_digest_{};

    friend RuntimeCohortToken
    make_runtime_cohort_token(std::uint64_t, std::uint64_t, const Sha256&) noexcept;
#if defined(DAWN_TYPE31_INCIDENT_CAPTURE_TEST)
    friend RuntimeCohortToken testing_runtime_cohort_token(std::uint64_t) noexcept;
#endif
};

enum class LiveAdmissionResult : std::uint8_t;

class ValidatedRuntimeCohort final {
public:
    [[nodiscard]] const RuntimeCohortToken& token() const noexcept {
        return token_;
    }
    [[nodiscard]] std::span<const std::uintptr_t, kNativeSurfaceCount> targets() const noexcept {
        return targets_;
    }
    [[nodiscard]] std::uintptr_t module_base() const noexcept {
        return module_base_;
    }

private:
    RuntimeCohortToken token_{};
    std::array<std::uintptr_t, kNativeSurfaceCount> targets_{};
    std::uintptr_t module_base_{};
    friend LiveAdmissionResult admit_live_runtime(ValidatedRuntimeCohort&) noexcept;
};

enum class FileMeasurementResult : std::uint8_t {
    measured,
    open_failed,
    size_failed,
    hash_failed,
};

[[nodiscard]] FileMeasurementResult
measure_pinned_packed_runtime(PackedRuntimeIdentity& output) noexcept;

enum class LiveAdmissionResult : std::uint8_t {
    admitted,
    main_module_unavailable,
    module_path_unavailable,
    packed_file_mismatch,
    invalid_pe,
    wrong_size_of_image,
    page_not_executable,
    prefix_mismatch,
    generation_exhausted,
};

/** Measures the actual main-module file and validates its live mapped PE/prefix cohort. */
[[nodiscard]] LiveAdmissionResult admit_live_runtime(ValidatedRuntimeCohort& output) noexcept;

#if defined(DAWN_TYPE31_INCIDENT_CAPTURE_TEST)
[[nodiscard]] RuntimeCohortToken testing_runtime_cohort_token(std::uint64_t generation) noexcept;
#endif

inline constexpr std::uintptr_t kType31HandlerTableRva = 0x1C13110U;
inline constexpr std::uintptr_t kTerminalEvaluatorCallsiteRva = 0xB20E13U;
inline constexpr std::uintptr_t kTerminalEvaluatorReturnRva = 0xB20E18U;
inline constexpr std::uintptr_t kManagerInstallRva = 0xD0F720U;
inline constexpr std::uintptr_t kManagerVtableRva = 0x1C1E230U;
inline constexpr std::size_t kManagerSubmitVslot = 0x08U;
inline constexpr std::uintptr_t kIncidentVisitorVtableRva = 0x1C21E68U;
inline constexpr std::uintptr_t kRootDescriptorPointerTableRva = 0x1FA6A40U;
inline constexpr std::uintptr_t kRootRegistryRecordRva = 0x27E0340U;
inline constexpr std::uintptr_t kRootReflectionRecordRva = 0x3778FF8U;
inline constexpr std::uintptr_t kDynamicSubtypeDispatcherRva = 0xD7E9E0U;
inline constexpr char kIncidentDiagnosticOperation[] = "send_incident";

inline constexpr std::uint32_t kActivityRegistry = 0xD00142CFU;
inline constexpr std::uint32_t kOpeningBubble = 15U;
inline constexpr std::uint16_t kPointType = 31U;
inline constexpr std::uint16_t kVolumeType = 60U;
inline constexpr std::uint32_t kType31Component = 0x80809522U;
inline constexpr std::uint32_t kType31AuthoritySchema = 0x80809524U;
inline constexpr bool kType31HasSenseSchema = false;
inline constexpr std::size_t kAuthorityDecodedBytes = 0x18U;
inline constexpr std::size_t kAuthorityWireBits = 129U;
inline constexpr std::uint64_t kUnsetGeneration = (std::numeric_limits<std::uint64_t>::max)();

#pragma pack(push, 1)
struct ObjectReference final {
    std::uint32_t registry{};
    std::uint16_t type{};
    std::uint16_t index{};
    friend constexpr bool operator==(ObjectReference, ObjectReference) noexcept = default;
};
#pragma pack(pop)

struct PointIdentity final {
    ObjectReference point{};
    ObjectReference volume{};
    std::uint32_t definition{};
    std::uint32_t placed_entity{};
    std::uint32_t wrapper{};
    std::uint32_t definition_bytes{};
    friend constexpr bool operator==(PointIdentity, PointIdentity) noexcept = default;
};

inline constexpr PointIdentity kVignettePoint{{kActivityRegistry, kPointType, 18U},
                                              {kActivityRegistry, kVolumeType, 28U},
                                              0x80F47BA6U,
                                              0x80F47BA7U,
                                              0x80F47BA8U,
                                              0x358U};
inline constexpr PointIdentity kOuterDialoguePoint{{kActivityRegistry, kPointType, 19U},
                                                   {kActivityRegistry, kVolumeType, 26U},
                                                   0x80F47BA9U,
                                                   0x80F47BAAU,
                                                   0x80F47BABU,
                                                   0x35EU};

[[nodiscard]] constexpr bool admitted_point(const PointIdentity& identity) noexcept {
    return identity == kVignettePoint || identity == kOuterDialoguePoint;
}
[[nodiscard]] constexpr PointIdentity point_from_definition(std::uint32_t definition) noexcept {
    return definition == kVignettePoint.definition
               ? kVignettePoint
               : (definition == kOuterDialoguePoint.definition ? kOuterDialoguePoint
                                                               : PointIdentity{});
}

inline constexpr std::size_t kConsumedGenerationOffset = 0x180U;
inline constexpr std::size_t kAuthorityActiveOffset = 0x188U;
inline constexpr std::size_t kPendingGenerationOffset = 0x190U;
inline constexpr std::size_t kAuthorityCompanionOffset = 0x198U;
inline constexpr std::size_t kAuthorityWindowBytes = 0x20U;
inline constexpr std::size_t kRuntimeVolumeMembershipOffset = 0x08U;

struct AuthorityState final {
    std::uint64_t consumed_generation{};
    std::uint64_t pending_generation{};
    std::uint64_t companion{};
    std::uint8_t active{};
    friend constexpr bool operator==(AuthorityState, AuthorityState) noexcept = default;
};

inline constexpr std::uint32_t kIncidentSObjectKind = 7U;
inline constexpr std::uint32_t kIncidentRootSchema = 0x8080879FU;
inline constexpr std::size_t kIncidentRootDecodedBytes = 0x58U;
inline constexpr std::size_t kIncidentRootRegistrationBytes = 0x4CU;
inline constexpr std::uint32_t kRootNestedContextSchema = 0x80809512U;
inline constexpr std::size_t kRootNestedContextBytes = 0x18U;
inline constexpr std::uint32_t kRootDynamicContextSchema = 0x8080880DU;
inline constexpr std::size_t kRootDynamicContextOffset = 0x18U;
inline constexpr std::size_t kRootDynamicContextBytes = 0x30U;
inline constexpr std::uint32_t kRootObjectReferenceSchema = 0x80809C42U;
inline constexpr std::size_t kRootObjectReferenceOffset = 0x48U;
inline constexpr std::size_t kRootResolvedReferenceOffset = 0x50U;

enum class Presence : std::uint8_t {
    absent,
    present,
};

struct OwnerToken final {
    Presence presence{Presence::absent};
    std::uint64_t identity{};
    std::uint64_t generation{};
    friend constexpr bool operator==(OwnerToken, OwnerToken) noexcept = default;
};

[[nodiscard]] constexpr bool valid_presence(const OwnerToken& token) noexcept {
    return token.presence == Presence::absent ? token.identity == 0U && token.generation == 0U
                                              : token.identity != 0U && token.generation != 0U;
}

struct OptionalU32 final {
    Presence presence{Presence::absent};
    std::uint32_t value{};
};
[[nodiscard]] constexpr bool valid_presence(OptionalU32 value) noexcept {
    return value.presence == Presence::present || value.value == 0U;
}

struct OwnershipSnapshot final {
    RuntimeCohortToken cohort{};
    std::uint64_t capture_epoch{};
    OwnerToken activation{};
    OwnerToken activity{};
    OwnerToken component{};
    OwnerToken authority{};
    OwnerToken runtime_volume{};
    OwnerToken membership{};
    OwnerToken incident_manager{};
    OwnerToken listener_table{};
    std::uint32_t local_player_datum{};
    std::uint8_t local_player_bit_index{};
    friend constexpr bool operator==(const OwnershipSnapshot&,
                                     const OwnershipSnapshot&) noexcept = default;
};

using OwnershipSnapshotRead = bool (*)(void* context, OwnershipSnapshot& output) noexcept;
struct OwnershipSnapshotProvider final {
    void* context{};
    OwnershipSnapshotRead read{};
};

enum class OwnerRequirement : std::uint16_t {
    none = 0U,
    activation = 1U << 0U,
    activity = 1U << 1U,
    component = 1U << 2U,
    authority = 1U << 3U,
    runtime_volume = 1U << 4U,
    membership = 1U << 5U,
    incident_manager = 1U << 6U,
    listener_table = 1U << 7U,
};

[[nodiscard]] constexpr OwnerRequirement operator|(OwnerRequirement left,
                                                   OwnerRequirement right) noexcept {
    return static_cast<OwnerRequirement>(static_cast<std::uint16_t>(left)
                                         | static_cast<std::uint16_t>(right));
}
[[nodiscard]] constexpr bool owner_required(OwnerRequirement mask,
                                            OwnerRequirement owner) noexcept {
    return (static_cast<std::uint16_t>(mask) & static_cast<std::uint16_t>(owner)) != 0U;
}

[[nodiscard]] bool valid_ownership_snapshot(const OwnershipSnapshot& snapshot,
                                            OwnerRequirement requirements) noexcept;
[[nodiscard]] bool exact_same_owners(const OwnershipSnapshot& before,
                                     const OwnershipSnapshot& after,
                                     OwnerRequirement requirements) noexcept;

struct CaptureMetadata final {
    std::uint64_t monotonic_tick{};
    std::uint32_t producer_thread_id{};
    std::uintptr_t normalized_caller_rva{};
};

enum class EvidenceSensitivity : std::uint8_t {
    restricted_local_re,
};

inline constexpr std::size_t kMaximumDynamicObjectBytes = 512U;
inline constexpr std::size_t kMaximumListenerTableBytes = 1024U;
inline constexpr std::size_t kMaximumListenerRowBytes = 512U;

struct OpaqueWindowInput final {
    const void* data{};
    std::size_t requested_bytes{};
    OptionalU32 observed_schema_or_subtype{};
};

template <std::size_t Capacity> struct BoundedOpaqueEvidence final {
    std::array<std::byte, Capacity> bytes{};
    std::uint32_t requested_bytes{};
    std::uint16_t captured_bytes{};
    OptionalU32 observed_schema_or_subtype{};
    Presence presence{Presence::absent};
    bool readable{};
    bool truncated{};

    [[nodiscard]] constexpr std::span<const std::byte> captured() const noexcept {
        return {bytes.data(), captured_bytes};
    }
};

struct PhaseFence final {
    OwnershipSnapshot before{};
    OwnershipSnapshot after{};
    bool before_valid{};
    bool after_valid{};
    bool exact{};
};

enum class TracePhase : std::uint16_t {
    none = 0U,
    terminal_entry = 1U << 0U,
    root_dispatch = 1U << 1U,
    recursive_materialize_submit = 1U << 2U,
    manager_submit = 1U << 3U,
    recursive_visitor = 1U << 4U,
    visitor_callback = 1U << 5U,
    normal_route = 1U << 6U,
    listener_entry = 1U << 7U,
    listener_exit = 1U << 8U,
    terminal_exit = 1U << 9U,
};

[[nodiscard]] constexpr TracePhase operator|(TracePhase left, TracePhase right) noexcept {
    return static_cast<TracePhase>(static_cast<std::uint16_t>(left)
                                   | static_cast<std::uint16_t>(right));
}
[[nodiscard]] constexpr bool phase_contains(TracePhase mask, TracePhase phase) noexcept {
    return (static_cast<std::uint16_t>(mask) & static_cast<std::uint16_t>(phase))
           == static_cast<std::uint16_t>(phase);
}

inline constexpr TracePhase kCompleteTracePhases =
    TracePhase::terminal_entry | TracePhase::root_dispatch
    | TracePhase::recursive_materialize_submit | TracePhase::manager_submit
    | TracePhase::recursive_visitor | TracePhase::visitor_callback | TracePhase::normal_route
    | TracePhase::listener_entry | TracePhase::listener_exit | TracePhase::terminal_exit;

struct TerminalEntryInput final {
    const void* instance{};
    const ObjectReference* matched_reference{};
    const void* runtime_volume{};
    CaptureMetadata metadata{};
};

struct RootEntryInput final {
    const std::uint32_t* sobject_id_storage{};
    std::uint32_t descriptor_type{};
    const void* root_payload{};
    OpaqueWindowInput dynamic_type35{};
    CaptureMetadata metadata{};
};

struct ListenerEntryInput final {
    OpaqueWindowInput table_window{};
    OpaqueWindowInput selected_row_window{};
    CaptureMetadata metadata{};
};

struct RootEvidence final {
    PhaseFence fence{};
    std::array<std::byte, kIncidentRootDecodedBytes> root_bytes{};
    BoundedOpaqueEvidence<kMaximumDynamicObjectBytes> dynamic_type35{};
    std::uintptr_t sobject_storage_identity{};
    std::uintptr_t root_storage_identity{};
    std::uint32_t sobject_id_value{};
    std::uint32_t descriptor_type{};
    ObjectReference object_reference{};
    std::uint32_t resolved_reference{};
    CaptureMetadata metadata{};
    bool root_readable{};
};

struct ListenerEvidence final {
    PhaseFence entry_fence{};
    PhaseFence exit_fence{};
    BoundedOpaqueEvidence<kMaximumListenerTableBytes> table_before{};
    BoundedOpaqueEvidence<kMaximumListenerTableBytes> table_after{};
    BoundedOpaqueEvidence<kMaximumListenerRowBytes> selected_row_before{};
    BoundedOpaqueEvidence<kMaximumListenerRowBytes> selected_row_after{};
    CaptureMetadata entry_metadata{};
    CaptureMetadata exit_metadata{};
    bool entry_observed{};
    bool exit_observed{};
};

struct RestrictedIncidentRecord final {
    EvidenceSensitivity sensitivity{EvidenceSensitivity::restricted_local_re};
    RuntimeCohortToken cohort{};
    std::uint64_t sequence{};
    std::uint64_t capture_epoch{};
    std::uint64_t gate_epoch{};
    std::uint64_t terminal_call_id{};
    PointIdentity point{};
    TracePhase phases{TracePhase::none};
    PhaseFence terminal_entry_fence{};
    PhaseFence terminal_exit_fence{};
    std::array<std::byte, kAuthorityWindowBytes> authority_window_before{};
    std::array<std::byte, kAuthorityWindowBytes> authority_window_after{};
    AuthorityState authority_before{};
    AuthorityState authority_after{};
    std::uint64_t membership_mask_before{};
    ObjectReference matched_reference{};
    std::uintptr_t instance_identity{};
    std::uintptr_t runtime_volume_identity{};
    CaptureMetadata terminal_entry_metadata{};
    CaptureMetadata terminal_exit_metadata{};
    RootEvidence root{};
    std::array<PhaseFence, 5U> route_fences{};
    ListenerEvidence listener{};
    bool terminal_entry_readable{};
    bool terminal_predicate_satisfied{};
    bool terminal_original_returned{};
    bool phase_order_valid{true};
};

[[nodiscard]] bool valid_restricted_record(const RestrictedIncidentRecord& record) noexcept;
[[nodiscard]] bool complete_generic_listener_trace(const RestrictedIncidentRecord& record) noexcept;
[[nodiscard]] bool one_shot_commit_observed(const RestrictedIncidentRecord& record) noexcept;

enum class RetailConsumerDisposition : std::uint8_t {
    unknown_dynamic_listener,
};

enum class CandidateConsumer : std::uint8_t {
    ghost_opening_vo,
    grounded_ikora_dwell,
    ikora_outer_dialogue,
    type26_hold,
    scene_or_lift,
};
[[nodiscard]] constexpr bool may_be_used_as_strict_gate(CandidateConsumer) noexcept {
    return false;
}

class EpochCallGate final {
public:
    class Scope final {
    public:
        explicit Scope(EpochCallGate& gate) noexcept;
        ~Scope() noexcept;
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;
        [[nodiscard]] bool admitted() const noexcept {
            return admitted_;
        }
        [[nodiscard]] std::uint64_t epoch() const noexcept {
            return epoch_;
        }

    private:
        EpochCallGate& gate_;
        std::uint64_t epoch_{};
        bool admitted_{};
        bool counted_{};
    };

    [[nodiscard]] bool open(std::uint64_t epoch) noexcept;
    [[nodiscard]] bool quiesce(std::uint64_t epoch) noexcept;
    [[nodiscard]] bool idle() const noexcept;
    [[nodiscard]] std::uint64_t epoch() const noexcept;
    [[nodiscard]] std::uint32_t active_calls() const noexcept;

private:
    static constexpr std::uint64_t kAcceptingBit = 1ULL << 63U;
    static constexpr std::uint64_t kEpochMask = 0x7FFFFFFFULL << 32U;
    static constexpr std::uint64_t kActiveMask = 0xFFFFFFFFULL;
    std::atomic<std::uint64_t> state_{};
};

template <typename Function> class OriginalSlot final {
public:
    [[nodiscard]] bool publish(Function original) noexcept {
        static_assert(std::is_pointer_v<Function>);
        if (original == nullptr) {
            return false;
        }
        Function expected{};
        return slot_.compare_exchange_strong(
            expected, original, std::memory_order_release, std::memory_order_relaxed);
    }
    [[nodiscard]] Function load() const noexcept {
        return slot_.load(std::memory_order_acquire);
    }
    void clear_after_confirmed_detach() noexcept {
        slot_.store(Function{}, std::memory_order_release);
    }

private:
    std::atomic<Function> slot_{};
};

enum class OriginalForwardResult : std::uint8_t {
    forwarded,
    missing_original,
};

/** Nonwaiting original-once helper; owners must publish before a replacement can be reached. */
template <typename Function, typename Before, typename After, typename... Arguments>
[[nodiscard]] OriginalForwardResult forward_void_original_once(EpochCallGate& gate,
                                                               OriginalSlot<Function>& slot,
                                                               Before&& before,
                                                               After&& after,
                                                               Arguments... arguments) noexcept {
    static_assert(std::is_pointer_v<Function>);
    static_assert(std::is_void_v<std::invoke_result_t<Function, Arguments...>>);
    static_assert(std::is_nothrow_invocable_v<Before, const EpochCallGate::Scope&>);
    static_assert(std::is_nothrow_invocable_v<After, const EpochCallGate::Scope&>);
    EpochCallGate::Scope scope{gate};
    const Function original = slot.load();
    if (original == nullptr) {
        return OriginalForwardResult::missing_original;
    }
    if (scope.admitted()) {
        std::invoke(std::forward<Before>(before), scope);
    }
    std::invoke(original, arguments...);
    if (scope.admitted()) {
        std::invoke(std::forward<After>(after), scope);
    }
    return OriginalForwardResult::forwarded;
}

inline constexpr std::size_t kMaximumNestedIncidentDepth = 4U;

class ThreadTraceStack final {
public:
    ThreadTraceStack() noexcept = default;
    ThreadTraceStack(const ThreadTraceStack&) = delete;
    ThreadTraceStack& operator=(const ThreadTraceStack&) = delete;
    [[nodiscard]] std::size_t depth() const noexcept {
        return depth_;
    }

private:
    struct Frame final {
        RestrictedIncidentRecord record{};
        OwnershipSnapshot terminal_owner{};
        OwnershipSnapshot listener_owner{};
        std::uint8_t next_route_phase{};
    };
    std::array<Frame, kMaximumNestedIncidentDepth> frames_{};
    std::size_t depth_{};
    friend class TraceCoordinator;
};

enum class TraceResult : std::uint8_t {
    complete,
    partial,
    not_admitted,
    stack_full,
    no_active_trace,
    invalid_input,
    unreadable,
    owner_absent,
    owner_changed,
    wrong_definition,
    wrong_reference,
    phase_order_error,
};

enum class QueuePushResult : std::uint8_t {
    enqueued,
    rejected,
    wrong_epoch,
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
    std::uint64_t rejected{};
    std::uint64_t rejected_epoch{};
    std::uint64_t dropped_full{};
    std::uint64_t dropped_busy{};
    std::uint64_t dropped_sequence_exhausted{};
    [[nodiscard]] constexpr std::uint64_t losses() const noexcept {
        return rejected + rejected_epoch + dropped_full + dropped_busy + dropped_sequence_exhausted;
    }
};

class DetachReceipt final {
public:
    [[nodiscard]] constexpr bool valid_for(std::uint64_t epoch) const noexcept {
        return detached_ && epoch_ != 0U && epoch_ == epoch;
    }

private:
    std::uint64_t epoch_{};
    bool detached_{};
    friend class HookGroupState;
};

inline constexpr std::size_t kRestrictedQueueCapacity = 16U;
class RestrictedEvidenceQueue final {
public:
    RestrictedEvidenceQueue() noexcept = default;
    RestrictedEvidenceQueue(const RestrictedEvidenceQueue&) = delete;
    RestrictedEvidenceQueue& operator=(const RestrictedEvidenceQueue&) = delete;
    [[nodiscard]] QueuePushResult try_push(const RestrictedIncidentRecord& record) noexcept;
    [[nodiscard]] QueuePopResult try_pop(RestrictedIncidentRecord& output) noexcept;
    [[nodiscard]] QueueCounters counters() const noexcept;
    [[nodiscard]] bool try_reset(const DetachReceipt& receipt) noexcept;
    [[nodiscard]] bool empty_quiesced() noexcept;

#if defined(DAWN_TYPE31_INCIDENT_CAPTURE_TEST)
    [[nodiscard]] bool testing_lock() noexcept;
    void testing_unlock() noexcept;
    void testing_set_next_sequence(std::uint64_t sequence) noexcept;
#endif

private:
    [[nodiscard]] bool try_lock() noexcept;
    void unlock() noexcept;
    std::array<RestrictedIncidentRecord, kRestrictedQueueCapacity> records_{};
    std::size_t head_{};
    std::size_t count_{};
    std::uint64_t queue_epoch_{};
    std::uint64_t next_sequence_{1U};
    std::atomic_flag lock_ = ATOMIC_FLAG_INIT;
    std::atomic<std::uint64_t> accepted_{};
    std::atomic<std::uint64_t> rejected_{};
    std::atomic<std::uint64_t> rejected_epoch_{};
    std::atomic<std::uint64_t> dropped_full_{};
    std::atomic<std::uint64_t> dropped_busy_{};
    std::atomic<std::uint64_t> dropped_sequence_exhausted_{};
};

struct TraceFinishResult final {
    TraceResult trace{TraceResult::no_active_trace};
    QueuePushResult queue{QueuePushResult::rejected};
};

class TraceCoordinator final {
public:
    TraceCoordinator() noexcept = default;
    TraceCoordinator(const TraceCoordinator&) = delete;
    TraceCoordinator& operator=(const TraceCoordinator&) = delete;

    [[nodiscard]] TraceResult begin_terminal(ThreadTraceStack& stack,
                                             const EpochCallGate::Scope& scope,
                                             const OwnershipSnapshotProvider& owners,
                                             const TerminalEntryInput& input) noexcept;
    [[nodiscard]] TraceResult observe_root_dispatch(ThreadTraceStack& stack,
                                                    const OwnershipSnapshotProvider& owners,
                                                    const RootEntryInput& input) noexcept;
    [[nodiscard]] TraceResult
    observe_recursive_materialize_submit(ThreadTraceStack& stack,
                                         const OwnershipSnapshotProvider& owners) noexcept;
    [[nodiscard]] TraceResult
    observe_manager_submit(ThreadTraceStack& stack,
                           const OwnershipSnapshotProvider& owners) noexcept;
    [[nodiscard]] TraceResult
    observe_recursive_visitor(ThreadTraceStack& stack,
                              const OwnershipSnapshotProvider& owners) noexcept;
    [[nodiscard]] TraceResult
    observe_visitor_callback(ThreadTraceStack& stack,
                             const OwnershipSnapshotProvider& owners) noexcept;
    [[nodiscard]] TraceResult
    observe_normal_route(ThreadTraceStack& stack, const OwnershipSnapshotProvider& owners) noexcept;
    [[nodiscard]] TraceResult observe_listener_entry(ThreadTraceStack& stack,
                                                     const OwnershipSnapshotProvider& owners,
                                                     const ListenerEntryInput& input) noexcept;
    [[nodiscard]] TraceResult observe_listener_exit(ThreadTraceStack& stack,
                                                    const OwnershipSnapshotProvider& owners,
                                                    const ListenerEntryInput& input) noexcept;
    [[nodiscard]] TraceFinishResult finish_terminal(ThreadTraceStack& stack,
                                                    const EpochCallGate::Scope& scope,
                                                    const OwnershipSnapshotProvider& owners,
                                                    const void* instance,
                                                    CaptureMetadata metadata,
                                                    RestrictedEvidenceQueue& queue) noexcept;

private:
    [[nodiscard]] TraceResult observe_route_phase(ThreadTraceStack& stack,
                                                  const OwnershipSnapshotProvider& owners,
                                                  std::uint8_t routeIndex,
                                                  TracePhase phase) noexcept;
    std::atomic<std::uint64_t> next_terminal_call_id_{1U};
};

enum class HookGroupPhase : std::uint8_t {
    detached,
    installing,
    running,
    quiescing,
};
enum class ProtectedDetachDisposition : std::uint8_t {
    removed,
    deferred,
    failed,
};
enum class ParticipantResult : std::uint8_t {
    recorded,
    wrong_phase,
    invalid,
};
enum class FinalDetachResult : std::uint8_t {
    detached,
    participants_remain,
    aggregate_not_idle,
    wrong_epoch,
    wrong_phase,
};

struct HookParticipant final {
    std::uintptr_t detour_handle{};
    std::uintptr_t original{};
    std::uint64_t protected_epoch{};
    bool attached{};
};
struct AggregateParticipantSnapshot final {
    std::uint64_t capture_epoch{};
    std::uint64_t producer_epoch{};
    std::uint64_t drain_epoch{};
    std::uint32_t active_producers{};
    std::uint32_t active_drains{};
    bool evidence_empty{};
    bool final_accounting_complete{};
};
struct HookGroupSnapshot final {
    RuntimeCohortToken cohort{};
    std::array<HookParticipant, kNativeSurfaceCount> participants{};
    std::uint16_t attached_mask{};
    std::uint64_t capture_epoch{};
    HookGroupPhase phase{HookGroupPhase::detached};
};

class HookGroupState final {
public:
    [[nodiscard]] bool begin_install(const RuntimeCohortToken& cohort,
                                     std::uint64_t captureEpoch) noexcept;
    [[nodiscard]] ParticipantResult record_attached(NativeSurface surface,
                                                    std::uintptr_t detourHandle,
                                                    std::uintptr_t original,
                                                    std::uint64_t protectedEpoch) noexcept;
    [[nodiscard]] bool complete_install(std::uint16_t requiredAttachedMask) noexcept;
    [[nodiscard]] bool retain_partial_install_failure() noexcept;
    [[nodiscard]] bool quiesce(std::uint64_t captureEpoch) noexcept;
    [[nodiscard]] ParticipantResult
    record_participant_detach(NativeSurface surface,
                              ProtectedDetachDisposition disposition) noexcept;
    [[nodiscard]] FinalDetachResult finalize_detach(const AggregateParticipantSnapshot& aggregate,
                                                    DetachReceipt& receipt) noexcept;
    [[nodiscard]] HookGroupSnapshot snapshot() const noexcept;

private:
    RuntimeCohortToken cohort_{};
    std::array<HookParticipant, kNativeSurfaceCount> participants_{};
    std::uint16_t attached_mask_{};
    std::uint64_t capture_epoch_{};
    HookGroupPhase phase_{HookGroupPhase::detached};
};

enum class ProjectionValidity : std::uint16_t {
    none = 0U,
    terminal_entry = 1U << 0U,
    terminal_exit = 1U << 1U,
    root = 1U << 2U,
    dynamic = 1U << 3U,
    listener_entry = 1U << 4U,
    listener_exit = 1U << 5U,
    complete_path = 1U << 6U,
    torn_candidate = 1U << 7U,
    truncated = 1U << 8U,
};

struct DefaultProjection final {
    Sha256 packed_build_sha256{};
    Sha256 mapped_prefix_digest{};
    Sha256 root_digest{};
    Sha256 dynamic_type35_digest{};
    Sha256 listener_table_before_digest{};
    Sha256 listener_table_after_digest{};
    Sha256 selected_row_digest{};
    std::uint64_t cohort_id{};
    std::uint64_t cohort_generation{};
    std::uint64_t capture_epoch{};
    std::uint64_t gate_epoch{};
    std::uint64_t queue_sequence{};
    std::uint64_t terminal_call_id{};
    std::uint32_t terminal_rva{};
    std::uint32_t listener_rva{};
    std::uint32_t root_schema{};
    std::uint16_t point_index{};
    std::uint16_t volume_index{};
    std::uint16_t phase_mask{};
    ProjectionValidity validity{ProjectionValidity::none};
    RetailConsumerDisposition consumer{RetailConsumerDisposition::unknown_dynamic_listener};
};

[[nodiscard]] bool sha256_bytes(std::span<const std::byte> bytes, Sha256& output) noexcept;
[[nodiscard]] bool project_default(const RestrictedIncidentRecord& record,
                                   DefaultProjection& output) noexcept;

static_assert(kPinnedPackedRuntimeSha256 != kPinnedUnpackedProvenanceSha256);
static_assert(sizeof(void*) == 8U, "The recovered PC Type-31 ABI is x64-only");
static_assert(sizeof(ObjectReference) == 0x08U);
static_assert(std::is_trivially_copyable_v<RestrictedIncidentRecord>);
static_assert(std::is_trivially_copyable_v<DefaultProjection>);
static_assert(std::atomic<std::uint64_t>::is_always_lock_free);

} // namespace dawn::client::hooks::bootflow::opening_authority::type31_incident
