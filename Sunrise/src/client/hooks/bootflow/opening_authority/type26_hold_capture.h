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

namespace sunrise::client::hooks::bootflow::opening_authority::type26_hold {

/** Disabled source-only contracts. Runtime detour ownership is intentionally absent. */
inline constexpr bool kOwnsNativeDetour = false;
inline constexpr bool kObservationOnly = true;
inline constexpr bool kHookPathPerformsIo = false;
inline constexpr bool kOffHookAdmissionPerformsIo = true;
inline constexpr bool kProvidesAuthorityEncoder = false;
inline constexpr bool kProvidesAuthorityWriter = false;
inline constexpr bool kProvidesGuessedBooleanPredicate = false;
inline constexpr bool kDefaultTelemetryContainsRawBodies = false;
inline constexpr bool kDefaultTelemetryContainsAbsoluteAddresses = false;

enum class ActiveAuthorityDisposition : std::uint8_t { unknown_do_not_encode };
inline constexpr ActiveAuthorityDisposition kActiveAuthorityDisposition =
    ActiveAuthorityDisposition::unknown_do_not_encode;

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
inline constexpr std::uintptr_t kPinnedUnpackedProvenanceImageBase =
    0x7FF68E9A0000ULL;
inline constexpr Sha256 kPinnedUnpackedProvenanceSha256{
    std::byte{0x87}, std::byte{0x13}, std::byte{0xD1}, std::byte{0x5E},
    std::byte{0x3D}, std::byte{0x05}, std::byte{0xB2}, std::byte{0x6F},
    std::byte{0x9E}, std::byte{0x25}, std::byte{0x9E}, std::byte{0x02},
    std::byte{0xB0}, std::byte{0xF2}, std::byte{0x9B}, std::byte{0xC1},
    std::byte{0xE0}, std::byte{0x00}, std::byte{0xE4}, std::byte{0xB0},
    std::byte{0xC6}, std::byte{0x2C}, std::byte{0xA2}, std::byte{0xCC},
    std::byte{0x87}, std::byte{0xC3}, std::byte{0x85}, std::byte{0x97},
    std::byte{0x18}, std::byte{0x6C}, std::byte{0xC3}, std::byte{0xBD}};
inline constexpr std::uint64_t kPackedBuildTelemetryId = 0x81964380664E7FCEULL;

struct PackedRuntimeIdentity final {
    std::uint64_t file_bytes{};
    Sha256 sha256{};
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
    authority_apply,
    reconcile,
    enumerate_materialize,
    subscriber_terminal,
    clear_terminal,
    attach_materialize,
    entity_effect_builder,
    sense_export,
    count,
};
inline constexpr std::size_t kNativeSurfaceCount =
    static_cast<std::size_t>(NativeSurface::count);
enum class NativeAbi : std::uint8_t {
    instance_packet,
    instance_only,
    instance_object_datum,
    entity_effect_builder,
    instance_packet_output,
};
struct NativeBoundaryDescriptor final {
    std::uintptr_t mapped_rva{};
    std::span<const std::byte> mapped_prefix{};
    NativeAbi abi{};
};
[[nodiscard]] NativeBoundaryDescriptor native_boundary(NativeSurface surface) noexcept;
[[nodiscard]] bool native_prefix_matches(NativeSurface surface,
                                         std::span<const std::byte> observed) noexcept;
[[nodiscard]] bool measure_pinned_packed_runtime(PackedRuntimeIdentity& output) noexcept;

enum class LiveRuntimeValidation : std::uint8_t {
    valid,
    invalid_arguments,
    wrong_process_main_module,
    module_path_mismatch,
    packed_file_identity_mismatch,
    invalid_pe_image,
    mapped_range_mismatch,
    target_out_of_range,
    target_not_executable,
    mapped_prefix_mismatch,
};
struct LiveRuntimeAddressGroup final {
    std::array<std::uintptr_t, kNativeSurfaceCount> addresses{};
    std::uint64_t module_generation{};
    std::uint32_t mapped_size_of_image{};
};
/** Actual packed-file measurement and mapped-main-module validation; call off-hook only. */
[[nodiscard]] LiveRuntimeValidation validate_live_runtime_group(
    void* liveModuleBase,
    std::uint64_t moduleGeneration,
    LiveRuntimeAddressGroup& output) noexcept;

inline constexpr std::uint32_t kActivityRegistry = 0xD00142CFU;
inline constexpr std::uint32_t kOpeningBubble = 15U;
inline constexpr std::uint32_t kHoldType = 26U;
inline constexpr std::uint32_t kHoldComponent = 0x8080953FU;
inline constexpr std::uint32_t kHoldAuthoritySchema = 0x8080954BU;
inline constexpr std::uint32_t kHoldSenseSchema = 0x8080954AU;
inline constexpr std::uint32_t kExternalResourceClass = 0x80809C0FU;
inline constexpr std::size_t kAuthorityNativeBytes = 0x70U;
inline constexpr std::size_t kSenseNativeBytes = 0x10U;
inline constexpr std::size_t kSenseWireBits = 97U;
inline constexpr std::size_t kSenseWireBytes = 13U;
inline constexpr std::size_t kMaximumAuthorityWireBytes = 512U;
inline constexpr std::size_t kAuthorityCacheOffset = 0x180U;
inline constexpr std::size_t kSenseCacheOffset = 0x1F0U;
inline constexpr std::size_t kDirtyByteOffset = 0x208U;

struct HoldDefinitionIdentity final {
    std::uint32_t registry{};
    std::uint32_t bubble{};
    std::uint32_t type{};
    std::uint32_t index{};
    std::uint32_t definition{};
    std::uint32_t placed_entity{};
    std::uint32_t wrapper{};
    std::uint32_t component{};
    std::uint32_t authority_schema{};
    std::uint32_t sense_schema{};
    std::uint32_t external_resource{};
    std::uint32_t external_resource_class{};
    std::uint32_t external_resource_bytes{};
    friend constexpr bool operator==(HoldDefinitionIdentity,
                                     HoldDefinitionIdentity) noexcept = default;
};
inline constexpr HoldDefinitionIdentity kWeaponDownIdentity{
    kActivityRegistry, kOpeningBubble, kHoldType, 5U, 0x80F47B7FU, 0x80F47B80U,
    0x80F47B81U, kHoldComponent, kHoldAuthoritySchema, kHoldSenseSchema, 0x80BEBF7FU,
    kExternalResourceClass, 0x8F4U};
inline constexpr HoldDefinitionIdentity kNoCombatAbilitiesIdentity{
    kActivityRegistry, kOpeningBubble, kHoldType, 6U, 0x80F47B82U, 0x80F47B83U,
    0x80F47B84U, kHoldComponent, kHoldAuthoritySchema, kHoldSenseSchema, 0x80BEAD83U,
    kExternalResourceClass, 0xBD4U};
[[nodiscard]] constexpr bool
admitted_hold_definition(const HoldDefinitionIdentity& identity) noexcept {
    return identity == kWeaponDownIdentity || identity == kNoCombatAbilitiesIdentity;
}

/** Resolver input wrapper. data is opaque and is never dereferenced as a decoded body. */
struct PacketReference final {
    std::uint32_t key{};
    std::uint32_t pad{};
    const void* data{};
};
#pragma pack(push, 1)
struct ObjectReference final {
    std::uint32_t registry{};
    std::int8_t type{};
    std::byte padding_05{};
    std::int16_t index{};
    friend constexpr bool operator==(ObjectReference, ObjectReference) noexcept = default;
};
struct AuthorityLayout final {
    std::uint8_t opaque_boolean_0{};
    std::uint8_t suppress_linked_enumeration{};
    std::array<std::byte, 2U> padding_02{};
    std::int32_t clear_generation{};
    std::int32_t subscriber_argument{};
    std::int32_t subscriber_generation{};
    std::int32_t sense_echo{};
    ObjectReference linked_selector{};
    std::array<std::byte, 4U> padding_1c{};
    std::array<std::byte, 0x50U> runtime_nested_type34{};
};
struct SenseLayout final {
    std::int32_t clear_generation{};
    std::int32_t subscriber_generation{};
    std::int32_t authority_echo{};
    std::uint8_t linked_content_item_present{};
    std::array<std::byte, 3U> padding_0d{};
};
#pragma pack(pop)
using AuthorityBody = std::array<std::byte, kAuthorityNativeBytes>;
using SenseBody = std::array<std::byte, kSenseNativeBytes>;
struct AuthorityFields final {
    std::uint8_t opaque_boolean_0{};
    std::uint8_t suppress_linked_enumeration{};
    std::int32_t clear_generation{};
    std::int32_t subscriber_argument{};
    std::int32_t subscriber_generation{};
    std::int32_t sense_echo{};
    ObjectReference linked_selector{};
};
struct SenseFields final {
    std::int32_t clear_generation{};
    std::int32_t subscriber_generation{};
    std::int32_t authority_echo{};
    std::uint8_t linked_content_item_present{};
};
[[nodiscard]] bool authority_fields(const AuthorityBody& body,
                                    AuthorityFields& output) noexcept;
[[nodiscard]] bool sense_fields(const SenseBody& body, SenseFields& output) noexcept;

#if defined(_MSC_VER)
using Apply = void(__fastcall*)(void*, const PacketReference*);
using Unary = void(__fastcall*)(void*);
using ExportSense = void(__fastcall*)(void*, PacketReference*);
using HoldAttach = std::uintptr_t(__fastcall*)(void*, std::uint32_t);
using EntityEffectBuilder = std::uint16_t*(__fastcall*)(
    void*, std::uint16_t*, const void*, std::int32_t);
using VirtualSubscriber = void(__fastcall*)(
    void*, std::uint32_t, const std::uint32_t*, std::int32_t);
#else
using Apply = void (*)(void*, const PacketReference*);
using Unary = void (*)(void*);
using ExportSense = void (*)(void*, PacketReference*);
using HoldAttach = std::uintptr_t (*)(void*, std::uint32_t);
using EntityEffectBuilder = std::uint16_t* (*)(void*, std::uint16_t*, const void*, std::int32_t);
using VirtualSubscriber = void (*)(void*, std::uint32_t, const std::uint32_t*, std::int32_t);
#endif

enum class ActivationState : std::uint8_t { absent, current, quiescing, stale };
struct ActivationContext final {
    Sha256 packed_runtime_sha256{};
    std::uint64_t module_generation{};
    std::uint64_t activation_generation{};
    std::uint64_t source_session{};
    std::uint64_t roster_epoch{};
    std::uint64_t roster_publication_sequence{};
    std::uintptr_t native_activity_wrapper{};
    std::uint32_t full_activity_handle{};
    std::uint32_t registry{};
    std::uint32_t bubble{};
    std::uint32_t local_player_datum{};
    std::uint32_t local_player_bit{};
    ActivationState state{ActivationState::absent};
    friend constexpr bool operator==(const ActivationContext&,
                                     const ActivationContext&) noexcept = default;
};
[[nodiscard]] bool exact_current_activation(const ActivationContext& context) noexcept;
[[nodiscard]] bool exact_activation_continuity(const ActivationContext& entry,
                                               const ActivationContext& exit) noexcept;
struct CaptureMetadata final {
    std::uint64_t capture_epoch{};
    std::uint64_t monotonic_tick{};
    std::uint64_t call_id{};
    std::uint32_t producer_thread_id{};
    std::uintptr_t caller_rva{};
};

enum class WireBitOrder : std::uint8_t { most_significant_bit_first };
struct WireSourceEvidence final {
    std::span<const std::byte> bytes{};
    std::size_t bit_count{};
    std::uint64_t tap_generation{};
    WireBitOrder bit_order{WireBitOrder::most_significant_bit_first};
    bool complete_without_underflow{};
    bool complete_without_trailing_bits{};
    bool dynamic_type34_complete{};
};
struct AuthorityWireSnapshot final {
    std::array<std::byte, kMaximumAuthorityWireBytes> bytes{};
    std::uint16_t byte_count{};
    std::uint16_t bit_count{};
    std::uint64_t tap_generation{};
    WireBitOrder bit_order{WireBitOrder::most_significant_bit_first};
    bool complete_without_underflow{};
    bool complete_without_trailing_bits{};
    bool dynamic_type34_complete{};
};
struct SenseWireSnapshot final {
    std::array<std::byte, kSenseWireBytes> bytes{};
    std::uint8_t bit_count{};
    std::uint64_t tap_generation{};
    WireBitOrder bit_order{WireBitOrder::most_significant_bit_first};
    bool complete_without_underflow{};
    bool complete_without_trailing_bits{};
};
enum class WireSnapshotResult : std::uint8_t {
    complete,
    invalid_length,
    unsupported_bit_order,
    nonzero_unused_bits,
    unreadable,
};
[[nodiscard]] WireSnapshotResult make_authority_wire_snapshot(
    const WireSourceEvidence& source,
    AuthorityWireSnapshot& output) noexcept;
[[nodiscard]] WireSnapshotResult make_sense_wire_snapshot(
    const WireSourceEvidence& source,
    SenseWireSnapshot& output) noexcept;

enum class CaptureBuildResult : std::uint8_t {
    ready,
    complete,
    null_pointer,
    unreadable,
    wrong_definition,
    wrong_resource_gate,
    wrong_schema,
    wrong_activation,
    invalid_wire,
    invalid_observation,
    consumed,
    activation_changed,
};

struct ApplyCaptureRecord final {
    ActivationContext entry_context{};
    ActivationContext exit_context{};
    HoldDefinitionIdentity identity{};
    CaptureMetadata metadata{};
    std::uint64_t sequence{};
    std::uint64_t authority_publication_sequence{};
    std::uintptr_t instance_identity{};
    std::uint32_t packet_schema{};
    std::uint32_t packet_pad{};
    AuthorityWireSnapshot inbound_wire{};
    AuthorityBody incoming_decoded_after_resolver{};
    AuthorityBody cache_before{};
    AuthorityBody cache_after{};
    std::uint8_t dirty_before{};
    std::uint8_t dirty_after{};
    bool packet_payload_was_nonnull{};
    bool equal_before{};
};
class PendingApplyCapture final {
public:
    PendingApplyCapture() noexcept = default;
    PendingApplyCapture(const PendingApplyCapture&) = delete;
    PendingApplyCapture& operator=(const PendingApplyCapture&) = delete;
private:
    friend CaptureBuildResult prepare_apply_capture(PendingApplyCapture&,
                                                    const ActivationContext&,
                                                    const HoldDefinitionIdentity&,
                                                    CaptureMetadata,
                                                    std::uint64_t,
                                                    const void*,
                                                    const PacketReference*,
                                                    const AuthorityWireSnapshot&) noexcept;
    friend CaptureBuildResult finish_apply_capture(PendingApplyCapture&,
                                                   const ActivationContext&,
                                                   const void*,
                                                   ApplyCaptureRecord&) noexcept;
    ApplyCaptureRecord record_{};
    bool ready_{};
};
[[nodiscard]] CaptureBuildResult prepare_apply_capture(
    PendingApplyCapture& pending,
    const ActivationContext& entryContext,
    const HoldDefinitionIdentity& identity,
    CaptureMetadata metadata,
    std::uint64_t authorityPublicationSequence,
    const void* instance,
    const PacketReference* unresolvedPacket,
    const AuthorityWireSnapshot& owningWire) noexcept;
/** Derives decoded incoming authority only from post-original component+0x180. */
[[nodiscard]] CaptureBuildResult finish_apply_capture(
    PendingApplyCapture& pending,
    const ActivationContext& exitContext,
    const void* instance,
    ApplyCaptureRecord& output) noexcept;

enum class ReconcileBranch : std::uint8_t {
    none = 0U,
    enumerate_materialize = 1U << 0U,
    attach_materialize = 1U << 1U,
    subscriber = 1U << 2U,
    clear_retire = 1U << 3U,
};
[[nodiscard]] constexpr ReconcileBranch operator|(ReconcileBranch left,
                                                   ReconcileBranch right) noexcept {
    return static_cast<ReconcileBranch>(static_cast<std::uint8_t>(left)
                                        | static_cast<std::uint8_t>(right));
}
[[nodiscard]] constexpr bool branch_contains(ReconcileBranch mask,
                                             ReconcileBranch value) noexcept {
    return (static_cast<std::uint8_t>(mask) & static_cast<std::uint8_t>(value))
           == static_cast<std::uint8_t>(value);
}
struct SubscriberObservation final {
    std::uintptr_t service_identity{};
    std::uintptr_t service_vtable{};
    std::uintptr_t slot_1a8_target{};
    std::uint32_t object_scalar{};
    std::uint32_t definition_scalar{};
    std::int32_t authority_scalar{};
    bool invoked{};
};
inline constexpr std::size_t kMaximumCapturedObjectDatums = 16U;
struct ReconcileCaptureInput final {
    ReconcileBranch branches{ReconcileBranch::none};
    ObjectReference selected_linked_reference{};
    std::array<std::uint32_t, kMaximumCapturedObjectDatums> live_object_datums{};
    std::uint8_t live_object_count{};
    SubscriberObservation subscriber{};
};
struct ReconcileCaptureRecord final {
    ActivationContext entry_context{};
    ActivationContext exit_context{};
    HoldDefinitionIdentity identity{};
    CaptureMetadata metadata{};
    std::uint64_t sequence{};
    std::uintptr_t instance_identity{};
    AuthorityBody authority_before{};
    AuthorityBody authority_after{};
    SenseBody sense_before{};
    SenseBody sense_after{};
    ReconcileCaptureInput observation{};
};
class PendingReconcileCapture final {
public:
    PendingReconcileCapture() noexcept = default;
    PendingReconcileCapture(const PendingReconcileCapture&) = delete;
    PendingReconcileCapture& operator=(const PendingReconcileCapture&) = delete;
private:
    friend CaptureBuildResult prepare_reconcile_capture(PendingReconcileCapture&,
                                                        const ActivationContext&,
                                                        const HoldDefinitionIdentity&,
                                                        CaptureMetadata,
                                                        const void*) noexcept;
    friend CaptureBuildResult finish_reconcile_capture(PendingReconcileCapture&,
                                                       const ActivationContext&,
                                                       const void*,
                                                       const ReconcileCaptureInput&,
                                                       ReconcileCaptureRecord&) noexcept;
    ReconcileCaptureRecord record_{};
    bool ready_{};
};
[[nodiscard]] CaptureBuildResult prepare_reconcile_capture(
    PendingReconcileCapture& pending,
    const ActivationContext& entryContext,
    const HoldDefinitionIdentity& identity,
    CaptureMetadata metadata,
    const void* instance) noexcept;
[[nodiscard]] CaptureBuildResult finish_reconcile_capture(
    PendingReconcileCapture& pending,
    const ActivationContext& exitContext,
    const void* instance,
    const ReconcileCaptureInput& observation,
    ReconcileCaptureRecord& output) noexcept;

struct SenseCaptureRecord final {
    ActivationContext entry_context{};
    ActivationContext exit_context{};
    HoldDefinitionIdentity identity{};
    CaptureMetadata metadata{};
    std::uint64_t sequence{};
    std::uint64_t report_sequence{};
    std::uint64_t host_receipt_sequence{};
    std::uintptr_t instance_identity{};
    std::uint32_t packet_schema{};
    SenseBody sense{};
    SenseBody sense_cache{};
    SenseWireSnapshot outbound_wire{};
    bool packet_points_to_sense_cache{};
};
class PendingSenseCapture final {
public:
    PendingSenseCapture() noexcept = default;
    PendingSenseCapture(const PendingSenseCapture&) = delete;
    PendingSenseCapture& operator=(const PendingSenseCapture&) = delete;
private:
    friend CaptureBuildResult prepare_sense_capture(PendingSenseCapture&,
                                                    const ActivationContext&,
                                                    const HoldDefinitionIdentity&,
                                                    CaptureMetadata,
                                                    std::uint64_t,
                                                    std::uint64_t,
                                                    const void*) noexcept;
    friend CaptureBuildResult finish_sense_capture(PendingSenseCapture&,
                                                   const ActivationContext&,
                                                   const void*,
                                                   const PacketReference*,
                                                   const SenseWireSnapshot&,
                                                   SenseCaptureRecord&) noexcept;
    SenseCaptureRecord record_{};
    bool ready_{};
};
[[nodiscard]] CaptureBuildResult prepare_sense_capture(
    PendingSenseCapture& pending,
    const ActivationContext& entryContext,
    const HoldDefinitionIdentity& identity,
    CaptureMetadata metadata,
    std::uint64_t reportSequence,
    std::uint64_t hostReceiptSequence,
    const void* instance) noexcept;
[[nodiscard]] CaptureBuildResult finish_sense_capture(
    PendingSenseCapture& pending,
    const ActivationContext& exitContext,
    const void* instance,
    const PacketReference* producedPacket,
    const SenseWireSnapshot& owningWire,
    SenseCaptureRecord& output) noexcept;

[[nodiscard]] bool valid_raw_record(const ApplyCaptureRecord& record) noexcept;
[[nodiscard]] bool valid_raw_record(const ReconcileCaptureRecord& record) noexcept;
[[nodiscard]] bool valid_raw_record(const SenseCaptureRecord& record) noexcept;

enum class RingPushResult : std::uint8_t {
    enqueued,
    rejected,
    wrong_producer,
    full,
    sequence_exhausted,
};
enum class RingPopResult : std::uint8_t { success, empty };
struct RingCounters final {
    std::uint64_t accepted{};
    std::uint64_t rejected{};
    std::uint64_t wrong_producer{};
    std::uint64_t dropped_full{};
    std::uint64_t dropped_sequence_exhausted{};
    [[nodiscard]] constexpr std::uint64_t losses() const noexcept {
        return rejected + wrong_producer + dropped_full + dropped_sequence_exhausted;
    }
};
/** Fixed SPSC ring owned by exactly one hook producer thread and one drain consumer. */
template <typename Record, std::size_t Capacity>
class FixedProducerRing final {
    static_assert(Capacity != 0U);
public:
    explicit FixedProducerRing(std::uint64_t producerToken) noexcept
        : producer_token_(producerToken) {}
    FixedProducerRing(const FixedProducerRing&) = delete;
    FixedProducerRing& operator=(const FixedProducerRing&) = delete;
    [[nodiscard]] RingPushResult try_push(const Record& source,
                                          std::uint64_t producerToken) noexcept {
        if (producerToken == 0U || producerToken != producer_token_) {
            wrong_producer_.fetch_add(1U, std::memory_order_relaxed);
            return RingPushResult::wrong_producer;
        }
        const std::uint64_t tail = tail_.load(std::memory_order_relaxed);
        const std::uint64_t head = head_.load(std::memory_order_acquire);
        if (tail - head >= Capacity) {
            dropped_full_.fetch_add(1U, std::memory_order_relaxed);
            return RingPushResult::full;
        }
        if (next_sequence_ == (std::numeric_limits<std::uint64_t>::max)()) {
            dropped_sequence_exhausted_.fetch_add(1U, std::memory_order_relaxed);
            return RingPushResult::sequence_exhausted;
        }
        Record& slot = records_[static_cast<std::size_t>(tail % Capacity)];
        slot = source;
        if (!valid_raw_record(slot)) {
            slot = {};
            rejected_.fetch_add(1U, std::memory_order_relaxed);
            return RingPushResult::rejected;
        }
        slot.sequence = next_sequence_++;
        tail_.store(tail + 1U, std::memory_order_release);
        accepted_.fetch_add(1U, std::memory_order_relaxed);
        return RingPushResult::enqueued;
    }
    [[nodiscard]] RingPopResult try_pop(Record& output) noexcept {
        const std::uint64_t head = head_.load(std::memory_order_relaxed);
        const std::uint64_t tail = tail_.load(std::memory_order_acquire);
        if (head == tail) {
            return RingPopResult::empty;
        }
        Record& slot = records_[static_cast<std::size_t>(head % Capacity)];
        output = slot;
        slot = {};
        head_.store(head + 1U, std::memory_order_release);
        return RingPopResult::success;
    }
    [[nodiscard]] bool empty() const noexcept {
        return head_.load(std::memory_order_acquire)
               == tail_.load(std::memory_order_acquire);
    }
    [[nodiscard]] RingCounters counters() const noexcept {
        return RingCounters{accepted_.load(std::memory_order_relaxed),
                            rejected_.load(std::memory_order_relaxed),
                            wrong_producer_.load(std::memory_order_relaxed),
                            dropped_full_.load(std::memory_order_relaxed),
                            dropped_sequence_exhausted_.load(std::memory_order_relaxed)};
    }
#if defined(SUNRISE_TYPE26_HOLD_CAPTURE_TEST)
    void testing_set_next_sequence(std::uint64_t sequence) noexcept {
        next_sequence_ = sequence;
    }
#endif
private:
    std::array<Record, Capacity> records_{};
    const std::uint64_t producer_token_{};
    std::uint64_t next_sequence_{1U};
    std::atomic<std::uint64_t> head_{};
    std::atomic<std::uint64_t> tail_{};
    std::atomic<std::uint64_t> accepted_{};
    std::atomic<std::uint64_t> rejected_{};
    std::atomic<std::uint64_t> wrong_producer_{};
    std::atomic<std::uint64_t> dropped_full_{};
    std::atomic<std::uint64_t> dropped_sequence_exhausted_{};
};
inline constexpr std::size_t kApplyRingCapacity = 8U;
inline constexpr std::size_t kReconcileRingCapacity = 32U;
inline constexpr std::size_t kSenseRingCapacity = 32U;
using ApplyCaptureRing = FixedProducerRing<ApplyCaptureRecord, kApplyRingCapacity>;
using ReconcileCaptureRing =
    FixedProducerRing<ReconcileCaptureRecord, kReconcileRingCapacity>;
using SenseCaptureRing = FixedProducerRing<SenseCaptureRecord, kSenseRingCapacity>;

struct KeyedDigest128 final {
    std::uint64_t high{};
    std::uint64_t low{};
    friend constexpr bool operator==(KeyedDigest128, KeyedDigest128) noexcept = default;
};
struct TelemetryProjectionContext final {
    std::uint64_t privacy_key_epoch{};
    std::uint64_t source_session_pseudonym{};
    std::uint64_t local_player_pseudonym{};
    std::uint32_t producer_thread_ordinal{};
};
struct TelemetryProvenance final {
    std::uint64_t build_id{};
    std::uint64_t module_generation{};
    std::uint64_t activation_generation{};
    std::uint64_t source_session_pseudonym{};
    std::uint64_t privacy_key_epoch{};
    std::uint64_t roster_epoch{};
    std::uint64_t roster_publication_sequence{};
    std::uint64_t local_player_pseudonym{};
    std::uint32_t registry{};
    std::uint32_t bubble{};
    std::uint32_t definition{};
    std::uint32_t slot{};
};
struct TelemetryEvent final {
    std::uint64_t capture_epoch{};
    std::uint64_t monotonic_tick{};
    std::uint64_t call_id{};
    std::uint32_t producer_thread_ordinal{};
};
struct ApplyTelemetryDigests final {
    KeyedDigest128 incoming{};
    KeyedDigest128 cache_before{};
    KeyedDigest128 cache_after{};
    KeyedDigest128 inbound_wire{};
};
struct ReconcileTelemetryDigests final {
    KeyedDigest128 authority_before{};
    KeyedDigest128 authority_after{};
    KeyedDigest128 sense_before{};
    KeyedDigest128 sense_after{};
    KeyedDigest128 live_objects{};
};
struct SenseTelemetryDigests final {
    KeyedDigest128 sense{};
    KeyedDigest128 outbound_wire{};
};
struct ApplyTelemetry final {
    TelemetryProvenance provenance{};
    TelemetryEvent event{};
    ApplyTelemetryDigests digests{};
    std::uint64_t authority_publication_sequence{};
    std::uint16_t inbound_wire_bits{};
    std::uint8_t dirty_before{};
    std::uint8_t dirty_after{};
    bool equal_before{};
};
struct ReconcileTelemetry final {
    TelemetryProvenance provenance{};
    TelemetryEvent event{};
    ReconcileTelemetryDigests digests{};
    std::uint32_t verified_subscriber_target_rva{};
    std::uint8_t live_object_count{};
    ReconcileBranch branches{ReconcileBranch::none};
    bool subscriber_invoked{};
    bool subscriber_target_normalized_to_verified_module{};
};
struct SenseTelemetry final {
    TelemetryProvenance provenance{};
    TelemetryEvent event{};
    SenseTelemetryDigests digests{};
    std::uint64_t report_sequence{};
    std::uint64_t host_receipt_sequence{};
    std::uint8_t outbound_wire_bits{};
    std::uint8_t linked_content_item_present{};
};
[[nodiscard]] bool default_telemetry(const ApplyCaptureRecord& record,
                                     const TelemetryProjectionContext& projection,
                                     const ApplyTelemetryDigests& digests,
                                     ApplyTelemetry& output) noexcept;
[[nodiscard]] bool default_telemetry(const ReconcileCaptureRecord& record,
                                     const TelemetryProjectionContext& projection,
                                     const ReconcileTelemetryDigests& digests,
                                     std::uint32_t verifiedSubscriberTargetRva,
                                     bool targetNormalized,
                                     ReconcileTelemetry& output) noexcept;
[[nodiscard]] bool default_telemetry(const SenseCaptureRecord& record,
                                     const TelemetryProjectionContext& projection,
                                     const SenseTelemetryDigests& digests,
                                     SenseTelemetry& output) noexcept;

class FullCallGate final {
public:
    class Scope final {
    public:
        explicit Scope(FullCallGate& gate) noexcept;
        ~Scope() noexcept;
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;
        [[nodiscard]] bool entry_observation_eligible() const noexcept;
        [[nodiscard]] bool post_observation_eligible() const noexcept;
        [[nodiscard]] std::uint64_t entry_epoch() const noexcept { return entry_epoch_; }
    private:
        FullCallGate& gate_;
        std::uint64_t entry_epoch_{};
        bool entry_eligible_{};
    };
    [[nodiscard]] bool begin_activation() noexcept;
    void quiesce() noexcept;
    [[nodiscard]] bool accepting() const noexcept;
    [[nodiscard]] bool idle() const noexcept;
    [[nodiscard]] std::uint32_t active_calls() const noexcept;
    [[nodiscard]] std::uint64_t epoch() const noexcept;
    [[nodiscard]] bool epoch_exhausted() const noexcept;
#if defined(SUNRISE_TYPE26_HOLD_CAPTURE_TEST)
    void testing_set_epoch(std::uint64_t epoch) noexcept;
#endif
private:
    friend class Scope;
    [[nodiscard]] bool advance_epoch() noexcept;
    std::atomic_bool accepting_{};
    std::atomic_bool epoch_exhausted_{};
    std::atomic<std::uint64_t> epoch_{};
    std::atomic<std::uint32_t> active_calls_{};
};

enum class OriginalPublishResult : std::uint8_t {
    published,
    invalid,
    already_published,
};
template <typename Function>
class OriginalSlot final {
    static_assert(std::is_pointer_v<Function>);
public:
    [[nodiscard]] OriginalPublishResult publish(std::uint64_t installGeneration,
                                                Function original) noexcept {
        if (installGeneration == 0U || original == nullptr) {
            return OriginalPublishResult::invalid;
        }
        std::uint8_t expected = 0U;
        if (!state_.compare_exchange_strong(
                expected, 1U, std::memory_order_acq_rel, std::memory_order_acquire)) {
            return OriginalPublishResult::already_published;
        }
        generation_.store(installGeneration, std::memory_order_relaxed);
        original_.store(original, std::memory_order_relaxed);
        state_.store(2U, std::memory_order_release);
        state_.notify_all();
        return OriginalPublishResult::published;
    }
    [[nodiscard]] Function await_original() noexcept {
        std::uint8_t state = state_.load(std::memory_order_acquire);
        while (state != 2U) {
            state_.wait(state, std::memory_order_acquire);
            state = state_.load(std::memory_order_acquire);
        }
        return original_.load(std::memory_order_acquire);
    }
    [[nodiscard]] bool matches_generation(std::uint64_t installGeneration) const noexcept {
        return state_.load(std::memory_order_acquire) == 2U
               && generation_.load(std::memory_order_acquire) == installGeneration;
    }
    [[nodiscard]] bool clear_after_confirmed_removal(std::uint64_t installGeneration) noexcept {
        if (state_.load(std::memory_order_acquire) != 2U
            || generation_.load(std::memory_order_acquire) != installGeneration) {
            return false;
        }
        original_.store(nullptr, std::memory_order_relaxed);
        generation_.store(0U, std::memory_order_relaxed);
        state_.store(0U, std::memory_order_release);
        return true;
    }
private:
    std::atomic<Function> original_{};
    std::atomic<std::uint64_t> generation_{};
    std::atomic<std::uint8_t> state_{};
};

template <typename Function, typename Before, typename After, typename... Arguments>
void forward_void_original_once(FullCallGate& gate,
                                OriginalSlot<Function>& originalSlot,
                                std::uint64_t installGeneration,
                                Before&& before,
                                After&& after,
                                Arguments... arguments) noexcept {
    static_assert(std::is_void_v<std::invoke_result_t<Function, Arguments...>>);
    static_assert(std::is_nothrow_invocable_v<Before>);
    static_assert(std::is_nothrow_invocable_v<After>);
    FullCallGate::Scope call{gate};
    const Function original = originalSlot.await_original();
    const bool generationMatches = originalSlot.matches_generation(installGeneration);
    if (generationMatches && call.entry_observation_eligible()) {
        std::invoke(std::forward<Before>(before));
    }
    std::invoke(original, arguments...);
    if (generationMatches && call.post_observation_eligible()) {
        std::invoke(std::forward<After>(after));
    }
}
template <typename Function, typename Before, typename After, typename... Arguments>
[[nodiscard]] std::invoke_result_t<Function, Arguments...> forward_value_original_once(
    FullCallGate& gate,
    OriginalSlot<Function>& originalSlot,
    std::uint64_t installGeneration,
    Before&& before,
    After&& after,
    Arguments... arguments) noexcept {
    using Result = std::invoke_result_t<Function, Arguments...>;
    static_assert(!std::is_void_v<Result>);
    static_assert(std::is_default_constructible_v<Result>);
    FullCallGate::Scope call{gate};
    const Function original = originalSlot.await_original();
    const bool generationMatches = originalSlot.matches_generation(installGeneration);
    if (generationMatches && call.entry_observation_eligible()) {
        std::invoke(std::forward<Before>(before));
    }
    Result result{};
    result = std::invoke(original, arguments...);
    if (generationMatches && call.post_observation_eligible()) {
        std::invoke(std::forward<After>(after));
    }
    return result;
}

enum class HookGroupPhase : std::uint8_t { detached, installing, running, quiescing };
enum class ProtectedDetachDisposition : std::uint8_t { removed, deferred, failed };
enum class ProtectedDetachResult : std::uint8_t {
    removed,
    protected_code_active,
    adapter_deferred,
    adapter_failed,
    wrong_phase,
};
struct HookGroupSnapshot final {
    std::array<std::uintptr_t, kNativeSurfaceCount> originals{};
    std::uint64_t install_generation{};
    HookGroupPhase phase{HookGroupPhase::detached};
};
class HookGroupState final {
public:
    [[nodiscard]] bool begin_install(std::uint64_t installGeneration) noexcept;
    [[nodiscard]] bool complete_install(
        std::span<const std::uintptr_t, kNativeSurfaceCount> originals) noexcept;
    [[nodiscard]] bool rollback_install() noexcept;
    [[nodiscard]] bool retain_install_failure(
        std::span<const std::uintptr_t, kNativeSurfaceCount> originals) noexcept;
    [[nodiscard]] bool quiesce() noexcept;
    [[nodiscard]] ProtectedDetachResult record_protected_detach(
        std::span<FullCallGate* const, kNativeSurfaceCount> gates,
        ProtectedDetachDisposition disposition) noexcept;
    [[nodiscard]] HookGroupSnapshot snapshot() const noexcept;
private:
    std::array<std::uintptr_t, kNativeSurfaceCount> originals_{};
    std::uint64_t install_generation_{};
    HookGroupPhase phase_{HookGroupPhase::detached};
};

/** Offline claim-shape validators only; no claim is derived from runtime records here. */
enum class ActiveBodyClaimResult : std::uint8_t { rejected, accepted_observation_only };
struct ActiveBodyClaim final {
    bool externally_proven_decode_encode_round_trip_bit_exact{};
    bool differs_from_neutral_candidate_in_consumed_field{};
    bool matching_reconcile_branch{};
    bool later_matching_sense{};
    bool repeated_without_crash{};
    bool no_extra_packet{};
    bool no_extra_runtime_object{};
};
[[nodiscard]] ActiveBodyClaimResult validate_active_body_claim(
    const ApplyCaptureRecord& record,
    const ActiveBodyClaim& claim) noexcept;
enum class ReadEdgeProvenance : std::uint8_t {
    none,
    native_instruction,
    mission_vm_instruction,
};
enum class HoldReadSource : std::uint8_t {
    none,
    authority_state,
    acknowledged_sense_generation,
};
struct ExplicitSceneReadEdgeEvidence final {
    ActivationContext context{};
    HoldDefinitionIdentity identity{};
    std::uint64_t hold_publication_generation{};
    std::uint64_t scene_publication_generation{};
    std::uintptr_t native_or_vm_instruction_identity{};
    ReadEdgeProvenance provenance{ReadEdgeProvenance::none};
    HoldReadSource source{HoldReadSource::none};
    bool consumes_hold_state_or_generation{};
    bool targets_scene_publication_decision{};
    bool suppresses_scene_until_satisfied{};
};
[[nodiscard]] bool proven_explicit_native_or_vm_read_edge(
    const ExplicitSceneReadEdgeEvidence& evidence) noexcept;
enum class RequiredTrial : std::uint16_t {
    cold_load_1 = 1U << 0U,
    cold_load_2 = 1U << 1U,
    cold_load_3 = 1U << 2U,
    late_join_in_progress = 1U << 3U,
    reentry_volume_60_28 = 1U << 4U,
    reentry_volume_60_26 = 1U << 5U,
    reentry_volume_60_29 = 1U << 6U,
    reverse_traversal = 1U << 7U,
    death_reset_before_scene = 1U << 8U,
    activation_teardown_restart = 1U << 9U,
};
inline constexpr std::uint16_t kAllRequiredTrials =
    static_cast<std::uint16_t>(RequiredTrial::cold_load_1)
    | static_cast<std::uint16_t>(RequiredTrial::cold_load_2)
    | static_cast<std::uint16_t>(RequiredTrial::cold_load_3)
    | static_cast<std::uint16_t>(RequiredTrial::late_join_in_progress)
    | static_cast<std::uint16_t>(RequiredTrial::reentry_volume_60_28)
    | static_cast<std::uint16_t>(RequiredTrial::reentry_volume_60_26)
    | static_cast<std::uint16_t>(RequiredTrial::reentry_volume_60_29)
    | static_cast<std::uint16_t>(RequiredTrial::reverse_traversal)
    | static_cast<std::uint16_t>(RequiredTrial::death_reset_before_scene)
    | static_cast<std::uint16_t>(RequiredTrial::activation_teardown_restart);
enum class HoldRoleClassification : std::uint8_t {
    inconclusive,
    unused_in_captured_opening,
    parallel_restriction_lane,
    ordered_presentation_predecessor,
    strict_scene_gate,
};
struct RoleClassificationClaim final {
    ActivationContext context{};
    HoldDefinitionIdentity identity{};
    std::uint64_t hold_publication_generation{};
    std::uint64_t scene_publication_generation{};
    std::uint16_t weapon_down_completed_trials{};
    std::uint16_t no_combat_abilities_completed_trials{};
    bool scene_proceeded{};
    bool schema_complete_non_neutral_authority_apply{};
    bool materialize_or_subscriber_transition{};
    bool matching_sense_host_acknowledgement{};
    bool correlated_player_facing_restriction{};
    bool repeated_hold_before_scene_same_publication_generation{};
    bool host_publication_dag_explicitly_orders_hold_before_scene{};
    ExplicitSceneReadEdgeEvidence scene_read_edge{};
};
[[nodiscard]] HoldRoleClassification validate_role_classification_claim(
    const RoleClassificationClaim& claim) noexcept;

static_assert(kPinnedPackedRuntimeSha256 != kPinnedUnpackedProvenanceSha256);
static_assert(sizeof(void*) == 8U, "The recovered PC ABI is x64-only");
static_assert(sizeof(PacketReference) == 0x10U);
static_assert(offsetof(PacketReference, data) == 0x08U);
static_assert(sizeof(ObjectReference) == 0x08U);
static_assert(offsetof(ObjectReference, type) == 0x04U);
static_assert(offsetof(ObjectReference, index) == 0x06U);
static_assert(sizeof(AuthorityLayout) == kAuthorityNativeBytes);
static_assert(offsetof(AuthorityLayout, runtime_nested_type34) == 0x20U);
static_assert(sizeof(SenseLayout) == kSenseNativeBytes);
static_assert(offsetof(SenseLayout, linked_content_item_present) == 0x0CU);
static_assert(std::is_trivially_copyable_v<ApplyCaptureRecord>);
static_assert(std::is_trivially_copyable_v<ReconcileCaptureRecord>);
static_assert(std::is_trivially_copyable_v<SenseCaptureRecord>);
static_assert(std::is_trivially_copyable_v<ApplyTelemetry>);
static_assert(std::is_trivially_copyable_v<ReconcileTelemetry>);
static_assert(std::is_trivially_copyable_v<SenseTelemetry>);
static_assert(std::atomic<std::uint64_t>::is_always_lock_free);

} // namespace sunrise::client::hooks::bootflow::opening_authority::type26_hold
