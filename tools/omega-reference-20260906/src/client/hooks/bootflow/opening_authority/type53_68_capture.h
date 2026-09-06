#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>

#include "client/hooks/activity_lifecycle/native_activation_registry.h"

namespace sunrise::client::hooks::bootflow::opening_authority {

/** Pure observation support. This module owns no hooks, lifecycle, I/O, settings, or writers. */
inline constexpr bool kOwnsNativeDetour = false;
inline constexpr bool kPerformsIo = false;
inline constexpr bool kDefaultTelemetryContainsRawBodies = false;

inline constexpr std::size_t kSha256Bytes = 32U;
using Sha256 = std::array<std::byte, kSha256Bytes>;

inline constexpr wchar_t kPinnedPackedDiskPath[] = L"D:\\Destiny3\\destiny2.exe";
inline constexpr std::uint64_t kPinnedPackedDiskBytes = 122'984'224U;
inline constexpr Sha256 kPinnedPackedDiskSha256{
    std::byte{0x81}, std::byte{0x96}, std::byte{0x43}, std::byte{0x80},
    std::byte{0x66}, std::byte{0x4E}, std::byte{0x7F}, std::byte{0xCE},
    std::byte{0xE3}, std::byte{0xC6}, std::byte{0x20}, std::byte{0x08},
    std::byte{0x5A}, std::byte{0x15}, std::byte{0x7F}, std::byte{0xDE},
    std::byte{0xAF}, std::byte{0x91}, std::byte{0xFE}, std::byte{0xFA},
    std::byte{0xCF}, std::byte{0x72}, std::byte{0x14}, std::byte{0x90},
    std::byte{0x78}, std::byte{0x20}, std::byte{0xF1}, std::byte{0x88},
    std::byte{0xBB}, std::byte{0xEB}, std::byte{0x4C}, std::byte{0xED}};

inline constexpr wchar_t kPinnedUnpackedReferencePath[] =
    L"D:\\Sunrise-work\\ghidra\\destiny2_unpacked.exe";
inline constexpr std::uint64_t kPinnedUnpackedReferenceBytes = 145'091'072U;
inline constexpr std::uintptr_t kPinnedUnpackedReferenceImageBase = 0x7FF68E9A0000ULL;
inline constexpr Sha256 kPinnedUnpackedReferenceSha256{
    std::byte{0x87}, std::byte{0x13}, std::byte{0xD1}, std::byte{0x5E},
    std::byte{0x3D}, std::byte{0x05}, std::byte{0xB2}, std::byte{0x6F},
    std::byte{0x9E}, std::byte{0x25}, std::byte{0x9E}, std::byte{0x02},
    std::byte{0xB0}, std::byte{0xF2}, std::byte{0x9B}, std::byte{0xC1},
    std::byte{0xE0}, std::byte{0x00}, std::byte{0xE4}, std::byte{0xB0},
    std::byte{0xC6}, std::byte{0x2C}, std::byte{0xA2}, std::byte{0xCC},
    std::byte{0x87}, std::byte{0xC3}, std::byte{0x85}, std::byte{0x97},
    std::byte{0x18}, std::byte{0x6C}, std::byte{0xC3}, std::byte{0xBD}};

/** Exact mapped PE OptionalHeader.SizeOfImage for the pinned PC build. */
inline constexpr std::size_t kPinnedMappedSizeOfImage = 0x08A5EA00U;
inline constexpr std::uint16_t kPinnedPeMachine = 0x8664U;
inline constexpr std::uint16_t kPinnedPeSections = 0x000BU;
inline constexpr std::uint32_t kPinnedPeTimestamp = 0x5F43138BU;
inline constexpr std::uint16_t kPinnedPeOptionalHeaderBytes = 0x00F0U;
inline constexpr std::uint16_t kPinnedPeCharacteristics = 0x0022U;
inline constexpr std::uint32_t kPinnedPeEntryPointRva = 0x0187CDD8U;
inline constexpr std::uint32_t kPinnedPeSectionAlignment = 0x1000U;
inline constexpr std::uintptr_t kPinnedPackedPreferredImageBase = 0x140000000ULL;
inline constexpr std::uint32_t kPinnedPackedPeFileAlignment = 0x200U;
inline constexpr std::uint32_t kPinnedPeHeadersBytes = 0x600U;
inline constexpr std::uint32_t kPinnedPackedPeChecksum = 0x0755867CU;
inline constexpr std::uint16_t kPinnedPeSubsystem = 0x0002U;
inline constexpr std::uint16_t kPinnedPackedPeDllCharacteristics = 0x8160U;
inline constexpr std::array<std::byte, 16U> kPinnedCodeViewGuid{
    std::byte{0xDF}, std::byte{0xFB}, std::byte{0xDC}, std::byte{0x0D},
    std::byte{0x68}, std::byte{0xEB}, std::byte{0x48}, std::byte{0x41},
    std::byte{0x8B}, std::byte{0xFB}, std::byte{0x7C}, std::byte{0x76},
    std::byte{0x18}, std::byte{0xFF}, std::byte{0xAB}, std::byte{0x03}};
inline constexpr std::uint32_t kPinnedCodeViewAge = 1U;

struct PackedMappedPeIdentity final {
    std::uintptr_t preferred_image_base{};
    std::uint32_t file_alignment{};
    std::uint32_t checksum{};
    std::uint16_t dll_characteristics{};
    std::array<std::byte, 16U> code_view_guid{};
    std::uint32_t code_view_age{};
    friend constexpr bool operator==(PackedMappedPeIdentity,
                                     PackedMappedPeIdentity) noexcept = default;
};

struct UnpackedReferencePeIdentity final {
    std::uintptr_t rebuilt_image_base{};
    std::uint32_t file_alignment{};
    std::uint32_t checksum{};
    std::uint16_t dll_characteristics{};
};

inline constexpr PackedMappedPeIdentity kPinnedPackedMappedPeIdentity{
    kPinnedPackedPreferredImageBase,
    kPinnedPackedPeFileAlignment,
    kPinnedPackedPeChecksum,
    kPinnedPackedPeDllCharacteristics,
    kPinnedCodeViewGuid,
    kPinnedCodeViewAge};
inline constexpr UnpackedReferencePeIdentity kPinnedUnpackedReferencePeIdentity{
    kPinnedUnpackedReferenceImageBase, 0x1000U, 0U, 0x8120U};

struct PackedDiskArtifactIdentity final {
    std::uint64_t file_bytes{};
    Sha256 sha256{};
};

struct UnpackedReferenceArtifactIdentity final {
    std::uint64_t file_bytes{};
    Sha256 sha256{};
};

[[nodiscard]] constexpr bool
matches_pinned_packed_disk(const PackedDiskArtifactIdentity& identity) noexcept {
    return identity.file_bytes == kPinnedPackedDiskBytes
           && identity.sha256 == kPinnedPackedDiskSha256;
}

[[nodiscard]] constexpr bool matches_pinned_unpacked_reference(
    const UnpackedReferenceArtifactIdentity& identity) noexcept {
    return identity.file_bytes == kPinnedUnpackedReferenceBytes
           && identity.sha256 == kPinnedUnpackedReferenceSha256;
}

enum class NativeSurface : std::uint8_t {
    schema_resolver,
    type53_apply,
    type53_tick,
    type53_selected_row_dispatch,
    type53_terminal_wrapper,
    type53_terminal_core,
    type53_presentation_start,
    type68_create_tail,
    type68_lifecycle_a,
    type68_lifecycle_b_update,
    type68_apply,
    type68_content_resolver,
    type68_same_identity_reconcile,
    type68_install,
    manager_ready,
    manager_get,
    manager_add,
    manager_status_update,
    manager_entry_materialize,
    manager_terminal_predicate,
    manager_materialize_walk,
};

enum class NativeAbi : std::uint8_t {
    packet_schema_resolver,
    instance_packet,
    instance_only,
    component_index,
    terminal_selector,
    create_instance_bool,
    component_content_new_old,
    component_record_content,
    no_args_bool,
    no_args_pointer,
    manager_event,
    manager_hash_status_aux,
    entry_only,
    content_resolver,
};

struct NativeBoundaryDescriptor final {
    std::uintptr_t rva{};
    std::span<const std::byte> prefix{};
    NativeAbi abi{};
    std::size_t exact_active_bytes{};
    bool inline_detour_14_safe{};
};

[[nodiscard]] NativeBoundaryDescriptor native_boundary(NativeSurface surface) noexcept;
[[nodiscard]] bool native_prefix_matches(NativeSurface surface,
                                         std::span<const std::byte> observed) noexcept;

inline constexpr std::uintptr_t kDialogueClassRecordRva = 0x3260958U;
inline constexpr std::uintptr_t kDialogueHandlerTableRva = 0x1CE3E40U;
inline constexpr std::uintptr_t kDirectiveClassRecordRva = 0x3260940U;
inline constexpr std::uintptr_t kDirectiveHandlerTableRva = 0x1CE3E00U;
inline constexpr std::uint64_t kAuthorityDispatchKey = 0x000000012B668F34ULL;
inline constexpr std::uint64_t kTickDispatchKey = 0x000000003088896AULL;
inline constexpr std::uint64_t kCreateDispatchKey = 0x0000000267774671ULL;

enum class CaptureWindow : std::uint8_t;

struct RuntimeImageView final {
    std::span<const std::byte> mapped_image{};
    const void* main_module_base{};
    PackedDiskArtifactIdentity packed_disk{};
    bool post_decryption_ready{};
    /** Supplied by the runtime owner after committed/executable-page validation. */
    bool selected_pages_committed_executable{};
};

struct RuntimeCohortEvidence final {
    PackedDiskArtifactIdentity packed_disk{};
    PackedMappedPeIdentity mapped_pe{};
    std::uintptr_t mapped_image_base{};
    std::size_t mapped_size_of_image{};
    std::uint64_t mapped_prefix_cohort_id{};
    std::uint64_t required_surface_mask{};
    std::uint64_t required_window_mask{};
    std::uint32_t required_surface_count{};
    std::uint32_t required_window_count{};
    bool pe_identity_valid{};
    bool post_decryption_ready{};
    bool selected_pages_committed_executable{};
    bool class_tables_valid{};
    bool all_prefixes_valid{};
};

enum class RuntimeCohortResult : std::uint8_t {
    valid,
    invalid_arguments,
    packed_disk_identity_mismatch,
    pe_identity_mismatch,
    not_post_decryption_ready,
    page_contract_failed,
    target_out_of_range,
    duplicate_surface,
    incomplete_manifest,
    prefix_mismatch,
    class_table_mismatch,
};

/** Validates the exact PE, both class tables, and every selected surface as one cohort. */
[[nodiscard]] RuntimeCohortResult validate_runtime_cohort(
    const RuntimeImageView& image,
    std::span<const NativeSurface> selectedSurfaces,
    std::span<const CaptureWindow> selectedWindows,
    std::span<std::uintptr_t> outputAddresses,
    std::span<std::uintptr_t> outputWindowAddresses,
    RuntimeCohortEvidence& outputEvidence) noexcept;

enum class CapturePhase : std::uint8_t {
    apply_entry,
    resolver_return,
    content_return,
    pre_call,
    post_call,
    post_commit,
    post_refresh,
    generation_consumed,
    presentation_start,
    presentation_end_unrecovered,
};

enum class CaptureWindow : std::uint8_t {
    type53_apply_entry,
    type53_resolver_return,
    type53_post_commit,
    type53_terminal_pre,
    type53_terminal_post,
    type53_generation_consumed,
    type53_terminal_wrapper,
    type53_terminal_core,
    type53_presentation_start,
    type53_presentation_end_unrecovered,
    type68_apply_entry,
    type68_resolver_return,
    type68_content_return,
    type68_reconcile_pre,
    type68_reconcile_post,
    type68_remove_pre,
    type68_remove_post,
    type68_install_pre,
    type68_install_post,
    type68_post_commit,
    type68_post_refresh,
    type68_manager_ready,
    type68_manager_add_pre,
    type68_manager_add_post,
    type68_alternate_builder_return,
    type68_alternate_add_pre,
    type68_alternate_add_post,
    type68_manager_materialize,
    type68_manager_materialize_post,
    type68_manager_terminal,
    type68_manager_terminal_post,
};

enum class CaptureProbeKind : std::uint8_t {
    function_entry,
    resolver_return_address_filter,
    callsite_pre,
    callsite_post_fallthrough,
    interior_instruction,
    unrecovered,
};

enum class CapturePlacement : std::uint8_t {
    reviewed_function_entry,
    reviewed_return_address_filter,
    owner_probe_not_recovered,
    unrecovered,
};

struct CaptureWindowDescriptor final {
    std::uintptr_t rva{};
    std::span<const std::byte> instruction_bytes{};
    std::size_t instruction_length{};
    CapturePhase phase{};
    CaptureProbeKind probe_kind{CaptureProbeKind::unrecovered};
    CapturePlacement placement{CapturePlacement::unrecovered};
    bool instruction_window_proven{};
    /** The RE window is evidence, not proof that an inline detour is safe there. */
    bool inline_probe_placement_proven{};
};

[[nodiscard]] CaptureWindowDescriptor capture_window(CaptureWindow window) noexcept;

inline constexpr std::size_t kRequiredNativeSurfaceCount = 21U;
inline constexpr std::size_t kRequiredCaptureWindowCount = 30U;
[[nodiscard]] std::span<const NativeSurface> required_native_surfaces() noexcept;
[[nodiscard]] std::span<const CaptureWindow> required_capture_windows() noexcept;

struct CaptureMetadata final {
    RuntimeCohortEvidence build{};
    std::uint64_t capture_epoch{};
    std::uint64_t monotonic_tick{};
    std::uint64_t call_id{};
    std::uint32_t producer_thread_id{};
    std::uintptr_t caller_rva{};
    CaptureWindow window{};
    CapturePhase phase{};
    std::uintptr_t native_rva{};
};

[[nodiscard]] bool valid_capture_metadata(const CaptureMetadata& metadata,
                                          bool allowUnrecoveredWindow = false) noexcept;

inline constexpr std::uint32_t kGlobalRegistry = 0x82FB58B7U;
inline constexpr std::uint32_t kDialogueType = 53U;
inline constexpr std::uint32_t kDialogueIndex = 2U;
inline constexpr std::uint32_t kDialogueComponentClass = 0x80804F4BU;
inline constexpr std::uint32_t kDialogueAuthoritySchema = 0x80804F77U;
inline constexpr std::uint32_t kDialogueDefinition = 0x80F47BDAU;
inline constexpr std::uint32_t kDialogueBank = 0x80F1FD07U;
inline constexpr std::uint32_t kDirectiveType = 68U;
inline constexpr std::uint32_t kDirectiveIndex = 0U;
inline constexpr std::uint32_t kDirectiveComponentClass = 0x80804F53U;
inline constexpr std::uint32_t kDirectiveAuthoritySchema = 0x80804F67U;
inline constexpr std::uint32_t kDirectiveDefinition = 0x80F47BD4U;
inline constexpr std::uint32_t kDirectiveBank = 0x80F47BD3U;

struct StaticConsumerProvenance final {
    std::uint32_t registry{};
    std::uint32_t type{};
    std::uint32_t index{};
    std::uint32_t component_class{};
    std::uint32_t schema{};
    std::uint32_t definition{};
    std::uint32_t bank{};
    bool statically_pinned{};
    friend constexpr bool operator==(StaticConsumerProvenance,
                                     StaticConsumerProvenance) noexcept = default;
};

inline constexpr StaticConsumerProvenance kDialogueProvenance{kGlobalRegistry,
                                                               kDialogueType,
                                                               kDialogueIndex,
                                                               kDialogueComponentClass,
                                                               kDialogueAuthoritySchema,
                                                               kDialogueDefinition,
                                                               kDialogueBank,
                                                               true};
inline constexpr StaticConsumerProvenance kDirectiveProvenance{kGlobalRegistry,
                                                                kDirectiveType,
                                                                kDirectiveIndex,
                                                                kDirectiveComponentClass,
                                                                kDirectiveAuthoritySchema,
                                                                kDirectiveDefinition,
                                                                kDirectiveBank,
                                                                true};

inline constexpr std::size_t kPacketWrapperExtent = 0x10U;
inline constexpr std::size_t kPacketWrapperMeaningfulBytes = 0x0CU;
inline constexpr std::size_t kPacketResolverInputOffset = 0x08U;
inline constexpr std::size_t kPcAuthorityCacheOffset = 0x180U;
inline constexpr std::size_t kDialogueBodyBytes = 0x1008U;
inline constexpr std::size_t kDialogueRecordCount = 128U;
inline constexpr std::size_t kDialogueRecordOffset = 0x08U;
inline constexpr std::size_t kDialogueRecordStride = 0x20U;
inline constexpr std::size_t kDialogueProcessedGenerationOffset = 0x1188U;
inline constexpr std::size_t kDirectiveBodyBytes = 0x300U;
inline constexpr std::size_t kDirectiveEntryCount = 3U;
inline constexpr std::size_t kDirectiveEntryOffset = 0x10U;
inline constexpr std::size_t kDirectiveEntryStride = 0xF8U;
inline constexpr std::size_t kDirectiveSelectorOffset = 0x2F8U;
inline constexpr std::size_t kManagerMaximumEntries = 16U;
inline constexpr std::size_t kManagerEntryStride = 0x148U;
inline constexpr std::size_t kManagerCountOffset = 0x1480U;
inline constexpr std::uint32_t kManagerEntryKind = 2U;

/** Physical apply wrapper. +8 is opaque resolver input, never a decoded-body pointer. */
struct PacketReference16 final {
    std::uint32_t schema{};
    std::array<std::byte, 4U> ignored_gap_04{};
    std::uintptr_t resolver_input{};
};

#pragma pack(push, 1)
/** Normalized entry evidence. The unused physical wrapper gap is never copied. */
struct EntryWrapperFields12 final {
    std::uint32_t schema{};
    std::uintptr_t resolver_input{};
    friend constexpr bool operator==(EntryWrapperFields12, EntryWrapperFields12) noexcept = default;
};
#pragma pack(pop)

#if defined(_MSC_VER)
using DialogueAuthorityApply =
    void(__fastcall*)(void* instance, const PacketReference16* packet) noexcept;
using DirectiveAuthorityApply =
    void(__fastcall*)(void* instance, const PacketReference16* packet) noexcept;
#else
using DialogueAuthorityApply = void (*)(void*, const PacketReference16*) noexcept;
using DirectiveAuthorityApply = void (*)(void*, const PacketReference16*) noexcept;
#endif

#pragma pack(push, 1)
struct StructuredReference8 final {
    std::array<std::byte, 8U> raw{};
    friend constexpr bool operator==(StructuredReference8, StructuredReference8) noexcept = default;
};

struct DialogueRecordLayout final {
    std::uint64_t value{};
    std::uint64_t optional_value_storage{};
    StructuredReference8 reference{};
    std::uint32_t generation{};
    std::uint32_t mode{};
};

struct DialogueDecodedLayout final {
    StructuredReference8 root{};
    std::array<DialogueRecordLayout, kDialogueRecordCount> records{};
};

struct DirectiveScalarBlockLayout final {
    std::uint8_t boolean{};
    std::array<std::byte, 7U> padding_01{};
    std::array<std::uint64_t, 5U> values{};
    std::uint32_t raw_u32{};
    std::array<std::byte, 4U> padding_34{};
};

struct DirectiveTargetLayout final {
    StructuredReference8 reference_a{};
    StructuredReference8 reference_b{};
    std::array<std::uint32_t, 4U> values{};
    std::uint8_t boolean{};
    std::array<std::byte, 3U> padding_21{};
};

struct DirectiveEntryLayout final {
    std::uint32_t event_key{};
    std::int32_t discriminator{};
    std::int8_t lifecycle{};
    std::array<std::byte, 7U> padding_09{};
    DirectiveScalarBlockLayout scalar{};
    std::array<std::int32_t, 4U> biased_values{};
    std::uint8_t auxiliary_enum_a{};
    std::array<std::byte, 3U> padding_59{};
    StructuredReference8 reference{};
    std::uint8_t auxiliary_enum_b{};
    std::array<std::byte, 3U> padding_65{};
    std::array<DirectiveTargetLayout, 4U> targets{};
};

struct DirectiveDecodedLayout final {
    StructuredReference8 root_a{};
    StructuredReference8 root_b{};
    std::array<DirectiveEntryLayout, kDirectiveEntryCount> entries{};
    std::int32_t selector{};
    std::array<std::byte, 4U> padding_2fc{};
};
#pragma pack(pop)

using DialogueBody = std::array<std::byte, kDialogueBodyBytes>;
using DirectiveBody = std::array<std::byte, kDirectiveBodyBytes>;

enum class DirectiveLifecycle : std::int8_t {
    absent = -1,
    installable = 0,
    terminal_numeric = 1,
};

[[nodiscard]] constexpr bool directive_selector_valid(std::int32_t selector) noexcept {
    return selector >= -1 && selector < static_cast<std::int32_t>(kDirectiveEntryCount);
}

[[nodiscard]] constexpr bool directive_lifecycle_valid(std::int8_t lifecycle) noexcept {
    return lifecycle >= static_cast<std::int8_t>(DirectiveLifecycle::absent)
           && lifecycle <= static_cast<std::int8_t>(DirectiveLifecycle::terminal_numeric);
}

struct DialogueRecordFields final {
    std::uint64_t value{};
    std::uint64_t optional_value_storage{};
    StructuredReference8 reference{};
    std::uint32_t generation{};
    std::uint32_t mode{};
};

struct DirectiveEntryFields final {
    std::uint32_t event_key{};
    std::int32_t discriminator{};
    std::int8_t lifecycle{};
};

[[nodiscard]] bool dialogue_record_fields(const DialogueBody& body,
                                          std::size_t index,
                                          DialogueRecordFields& output) noexcept;
[[nodiscard]] bool directive_entry_fields(const DirectiveBody& body,
                                          std::size_t index,
                                          DirectiveEntryFields& output) noexcept;
[[nodiscard]] bool canonical_dialogue_body(const DialogueBody& body) noexcept;
[[nodiscard]] bool canonical_directive_body(const DirectiveBody& body) noexcept;
/** Bounded FNV-1a fingerprint for secured local RE correlation; not an integrity digest. */
[[nodiscard]] std::uint64_t bounded_body_fingerprint(std::span<const std::byte> body) noexcept;

enum class ContextPresence : std::uint32_t {
    activation = 1U << 0U,
    native_identity = 1U << 1U,
    activity = 1U << 2U,
    roster_generation = 1U << 3U,
    authority_publication = 1U << 4U,
    run_token = 1U << 5U,
    correlation_token = 1U << 6U,
};

inline constexpr std::uint32_t kKnownContextPresenceMask =
    static_cast<std::uint32_t>(ContextPresence::activation)
    | static_cast<std::uint32_t>(ContextPresence::native_identity)
    | static_cast<std::uint32_t>(ContextPresence::activity)
    | static_cast<std::uint32_t>(ContextPresence::roster_generation)
    | static_cast<std::uint32_t>(ContextPresence::authority_publication)
    | static_cast<std::uint32_t>(ContextPresence::run_token)
    | static_cast<std::uint32_t>(ContextPresence::correlation_token);

[[nodiscard]] constexpr bool has_context_field(std::uint32_t mask,
                                               ContextPresence field) noexcept {
    return (mask & static_cast<std::uint32_t>(field)) != 0U;
}

struct CaptureContext final {
    std::uint32_t presence_mask{};
    activity_lifecycle::NativeActivationToken activation{};
    std::uint64_t native_identity{};
    state::activity::ActivityInstanceKey activity{};
    state::activity::RosterGraphGeneration roster_generation{};
    state::activity::PublicationGeneration authority_publication{};
    std::uint64_t run_token{};
    std::uint64_t correlation_token{};
    activity_lifecycle::NativeActivationState activation_state{
        activity_lifecycle::NativeActivationState::empty};
    friend constexpr bool operator==(CaptureContext, CaptureContext) noexcept = default;
};

using RegistryIsCurrentCallback = bool (*)(
    const void*, activity_lifecycle::NativeActivationToken) noexcept;

/**
 * Owner-supplied exact-registry revalidation. The callback must perform the authoritative
 * NativeActivationRegistry::is_current(token) check; copied token equality is not freshness.
 */
struct OwnerCurrentValidator final {
    const void* registry{};
    RegistryIsCurrentCallback is_current{};
    [[nodiscard]] explicit constexpr operator bool() const noexcept {
        return registry != nullptr && is_current != nullptr;
    }
};

[[nodiscard]] bool registry_token_current(const CaptureContext& context,
                                          OwnerCurrentValidator validator) noexcept;

/** Structural completeness only. It does not prove that the registry token is still current. */
[[nodiscard]] bool exact_owner_at_entry(const CaptureContext& context) noexcept;
[[nodiscard]] bool fully_correlated(const CaptureContext& context) noexcept;
/** Copied-domain comparison only. Callers must also revalidate through OwnerCurrentValidator. */
[[nodiscard]] bool same_owner_current_at_exit(const CaptureContext& entry,
                                              const CaptureContext& exit) noexcept;

enum class ApplyConsumerLane : std::uint8_t { type53, type68 };

/** One owner-derived frame. No generation is synthesized by this capture module. */
struct ApplyOwnerFrame final {
    CaptureContext context{};
    std::uint64_t capture_epoch{};
    std::uint64_t call_id{};
    std::uint32_t producer_thread_id{};
    std::uintptr_t instance_identity{};
    ApplyConsumerLane lane{};
    bool registry_current_at_entry{};
    friend constexpr bool operator==(ApplyOwnerFrame, ApplyOwnerFrame) noexcept = default;
};

enum class ApplyFrameResult : std::uint8_t {
    success,
    full,
    empty,
    mismatch,
};

/**
 * Fixed source-only TLS seam. A future owner declares this stack thread_local, pushes at the
 * admitted apply entry, routes resolver returns only through the exact reviewed return RVA, and
 * pops only the exact matching original-return frame. This class installs no probe or detour.
 */
template <std::size_t Capacity>
class FixedApplyOwnerStack final {
    static_assert(Capacity != 0U);

public:
    [[nodiscard]] ApplyFrameResult try_push(const ApplyOwnerFrame& frame) noexcept {
        if (!valid_frame(frame)) {
            return ApplyFrameResult::mismatch;
        }
        if (depth_ == Capacity) {
            return ApplyFrameResult::full;
        }
        frames_[depth_++] = frame;
        return ApplyFrameResult::success;
    }

    [[nodiscard]] const ApplyOwnerFrame* top() const noexcept {
        return depth_ == 0U ? nullptr : &frames_[depth_ - 1U];
    }

    [[nodiscard]] ApplyFrameResult try_pop_exact(std::uint64_t captureEpoch,
                                                 std::uint64_t callId,
                                                 std::uint32_t producerThreadId,
                                                 std::uintptr_t instanceIdentity,
                                                 ApplyConsumerLane lane,
                                                 ApplyOwnerFrame& output) noexcept {
        if (depth_ == 0U) {
            return ApplyFrameResult::empty;
        }
        const ApplyOwnerFrame& candidate = frames_[depth_ - 1U];
        if (candidate.capture_epoch != captureEpoch || candidate.call_id != callId
            || candidate.producer_thread_id != producerThreadId
            || candidate.instance_identity != instanceIdentity || candidate.lane != lane) {
            return ApplyFrameResult::mismatch;
        }
        output = candidate;
        frames_[depth_ - 1U] = {};
        --depth_;
        return ApplyFrameResult::success;
    }

    [[nodiscard]] constexpr std::size_t depth() const noexcept { return depth_; }

private:
    [[nodiscard]] static bool valid_frame(const ApplyOwnerFrame& frame) noexcept {
        return frame.capture_epoch != 0U && frame.call_id != 0U
               && frame.producer_thread_id != 0U && frame.instance_identity != 0U
               && exact_owner_at_entry(frame.context) && frame.registry_current_at_entry;
    }

    std::array<ApplyOwnerFrame, Capacity> frames_{};
    std::size_t depth_{};
};

inline constexpr std::uintptr_t kType53ResolverReturnRva = 0x1009B8DU;
inline constexpr std::uintptr_t kType68ResolverReturnRva = 0x1009C35U;

[[nodiscard]] bool owner_frame_accepts_resolver_return(const ApplyOwnerFrame& frame,
                                                       std::uintptr_t returnRva,
                                                       std::uint64_t captureEpoch,
                                                       std::uint64_t callId,
                                                       std::uint32_t producerThreadId) noexcept;

struct SourceOnlyOwnerIntegrationContract final {
    std::size_t packet_extent{};
    std::size_t meaningful_packet_bytes{};
    std::size_t resolver_input_offset{};
    std::uintptr_t type53_entry_rva{};
    std::uintptr_t type53_resolver_return_rva{};
    std::uintptr_t type53_commit_rva{};
    std::uintptr_t type68_entry_rva{};
    std::uintptr_t type68_resolver_return_rva{};
    std::uintptr_t type68_commit_rva{};
    bool tls_stack_required{};
    bool exact_registry_revalidation_required{};
    bool wrapper_return_may_claim_commit{};
    bool installs_live_probe{};
};

[[nodiscard]] constexpr SourceOnlyOwnerIntegrationContract source_only_owner_contract() noexcept {
    return SourceOnlyOwnerIntegrationContract{kPacketWrapperExtent,
                                               kPacketWrapperMeaningfulBytes,
                                               kPacketResolverInputOffset,
                                               0x1009B60U,
                                               kType53ResolverReturnRva,
                                               0x1009BF9U,
                                               0x1009C00U,
                                               kType68ResolverReturnRva,
                                               0x1009DF3U,
                                               true,
                                               true,
                                               false,
                                               false};
}


struct ApplyValidity final {
    bool entry_wrapper_valid{};
    bool native_resolver_observed{};
    bool native_resolver_succeeded{};
    bool decoded_copy_valid{};
    bool decoded_canonical{};
    bool cache_before_valid{};
    bool cache_after_valid{};
    bool cache_after_canonical{};
    bool exact_owner_at_entry{};
    bool same_owner_current_at_resolver{};
    bool same_owner_current_at_exit{};
};

struct ApplyPhaseTelemetry final {
    CaptureMetadata entry{};
    CaptureMetadata resolver{};
    CaptureMetadata commit{};
};

struct DialogueApplyRecord final {
    CaptureContext entry_context{};
    CaptureContext resolver_context{};
    CaptureContext exit_context{};
    ApplyPhaseTelemetry telemetry{};
    std::uint64_t sequence{};
    std::uintptr_t instance_identity{};
    StaticConsumerProvenance static_consumer{};
    EntryWrapperFields12 entry_wrapper{};
    DialogueBody resolver_body_pre{};
    DialogueBody post_commit_cache{};
    std::uint64_t cache_before_fingerprint{};
    std::uint64_t resolver_body_fingerprint{};
    std::uint64_t post_commit_cache_fingerprint{};
    ApplyValidity valid{};
};

struct DirectiveApplyRecord final {
    CaptureContext entry_context{};
    CaptureContext resolver_context{};
    CaptureContext exit_context{};
    ApplyPhaseTelemetry telemetry{};
    std::uint64_t sequence{};
    std::uintptr_t instance_identity{};
    StaticConsumerProvenance static_consumer{};
    EntryWrapperFields12 entry_wrapper{};
    DirectiveBody resolver_body_pre{};
    DirectiveBody post_commit_cache{};
    std::uint64_t cache_before_fingerprint{};
    std::uint64_t resolver_body_fingerprint{};
    std::uint64_t post_commit_cache_fingerprint{};
    ApplyValidity valid{};
};

[[nodiscard]] bool valid_raw_record(const DialogueApplyRecord& record) noexcept;
[[nodiscard]] bool valid_raw_record(const DirectiveApplyRecord& record) noexcept;

enum class CaptureBuildResult : std::uint8_t {
    entry_ready,
    resolver_captured,
    resolver_failed,
    complete,
    partial,
    null_pointer,
    unreadable,
    wrong_definition,
    wrong_schema,
    invalid_metadata,
    phase_mismatch,
    instance_mismatch,
    consumed,
};

class PendingDialogueCapture final {
public:
    PendingDialogueCapture() noexcept = default;
    PendingDialogueCapture(const PendingDialogueCapture&) = delete;
    PendingDialogueCapture& operator=(const PendingDialogueCapture&) = delete;

private:
    friend CaptureBuildResult capture_entry_wrapper(PendingDialogueCapture&,
                                                    const CaptureContext&,
                                                    OwnerCurrentValidator,
                                                    CaptureMetadata,
                                                    const void*,
                                                    const PacketReference16*) noexcept;
    friend CaptureBuildResult capture_resolver_result(PendingDialogueCapture&,
                                                      const CaptureContext&,
                                                      OwnerCurrentValidator,
                                                      CaptureMetadata,
                                                      const void*) noexcept;
    friend CaptureBuildResult capture_post_commit(PendingDialogueCapture&,
                                                  const CaptureContext&,
                                                  OwnerCurrentValidator,
                                                  CaptureMetadata,
                                                  const void*,
                                                  DialogueApplyRecord&) noexcept;
    CaptureContext entry_context_{};
    CaptureContext resolver_context_{};
    CaptureMetadata entry_metadata_{};
    CaptureMetadata resolver_metadata_{};
    std::uintptr_t instance_identity_{};
    EntryWrapperFields12 entry_wrapper_{};
    DialogueBody resolver_body_{};
    std::uint64_t cache_before_fingerprint_{};
    bool cache_before_valid_{};
    bool resolver_observed_{};
    bool resolver_succeeded_{};
    bool decoded_copy_valid_{};
    bool decoded_canonical_{};
    bool entry_registry_current_{};
    bool resolver_registry_current_{};
    bool ready_{};
};

class PendingDirectiveCapture final {
public:
    PendingDirectiveCapture() noexcept = default;
    PendingDirectiveCapture(const PendingDirectiveCapture&) = delete;
    PendingDirectiveCapture& operator=(const PendingDirectiveCapture&) = delete;

private:
    friend CaptureBuildResult capture_entry_wrapper(PendingDirectiveCapture&,
                                                    const CaptureContext&,
                                                    OwnerCurrentValidator,
                                                    CaptureMetadata,
                                                    const void*,
                                                    const PacketReference16*) noexcept;
    friend CaptureBuildResult capture_resolver_result(PendingDirectiveCapture&,
                                                      const CaptureContext&,
                                                      OwnerCurrentValidator,
                                                      CaptureMetadata,
                                                      const void*) noexcept;
    friend CaptureBuildResult capture_post_commit(PendingDirectiveCapture&,
                                                  const CaptureContext&,
                                                  OwnerCurrentValidator,
                                                  CaptureMetadata,
                                                  const void*,
                                                  DirectiveApplyRecord&) noexcept;
    CaptureContext entry_context_{};
    CaptureContext resolver_context_{};
    CaptureMetadata entry_metadata_{};
    CaptureMetadata resolver_metadata_{};
    std::uintptr_t instance_identity_{};
    EntryWrapperFields12 entry_wrapper_{};
    DirectiveBody resolver_body_{};
    std::uint64_t cache_before_fingerprint_{};
    bool cache_before_valid_{};
    bool resolver_observed_{};
    bool resolver_succeeded_{};
    bool decoded_copy_valid_{};
    bool decoded_canonical_{};
    bool entry_registry_current_{};
    bool resolver_registry_current_{};
    bool ready_{};
};

/** Entry copies schema and opaque resolver input only. It never dereferences wrapper+8. */
[[nodiscard]] CaptureBuildResult capture_entry_wrapper(PendingDialogueCapture& pending,
                                                       const CaptureContext& context,
                                                       OwnerCurrentValidator ownerValidator,
                                                       CaptureMetadata metadata,
                                                       const void* instance,
                                                       const PacketReference16* packet) noexcept;
[[nodiscard]] CaptureBuildResult capture_entry_wrapper(PendingDirectiveCapture& pending,
                                                       const CaptureContext& context,
                                                       OwnerCurrentValidator ownerValidator,
                                                       CaptureMetadata metadata,
                                                       const void* instance,
                                                       const PacketReference16* packet) noexcept;

/** Copies the actual decoded RAX only at the exact +1009B8D/+1009C35 resolver-return window. */
[[nodiscard]] CaptureBuildResult capture_resolver_result(PendingDialogueCapture& pending,
                                                         const CaptureContext& resolverContext,
                                                         OwnerCurrentValidator ownerValidator,
                                                         CaptureMetadata metadata,
                                                         const void* decodedRax) noexcept;
[[nodiscard]] CaptureBuildResult capture_resolver_result(PendingDirectiveCapture& pending,
                                                         const CaptureContext& resolverContext,
                                                         OwnerCurrentValidator ownerValidator,
                                                         CaptureMetadata metadata,
                                                         const void* decodedRax) noexcept;

/** Captures the exact post-commit cache and revalidates a copied exit owner snapshot. */
[[nodiscard]] CaptureBuildResult capture_post_commit(PendingDialogueCapture& pending,
                                                     const CaptureContext& exitContext,
                                                     OwnerCurrentValidator ownerValidator,
                                                     CaptureMetadata metadata,
                                                     const void* instance,
                                                     DialogueApplyRecord& output) noexcept;
[[nodiscard]] CaptureBuildResult capture_post_commit(PendingDirectiveCapture& pending,
                                                     const CaptureContext& exitContext,
                                                     OwnerCurrentValidator ownerValidator,
                                                     CaptureMetadata metadata,
                                                     const void* instance,
                                                     DirectiveApplyRecord& output) noexcept;

struct SemanticDedupeKey final {
    CaptureContext owner{};
    std::uint64_t capture_epoch{};
    StaticConsumerProvenance static_consumer{};
    std::uint64_t authority_body_fingerprint{};
    bool valid{};
    friend constexpr bool operator==(SemanticDedupeKey, SemanticDedupeKey) noexcept = default;
};

[[nodiscard]] SemanticDedupeKey semantic_dedupe_key(const DialogueApplyRecord& record) noexcept;
[[nodiscard]] SemanticDedupeKey semantic_dedupe_key(const DirectiveApplyRecord& record) noexcept;

enum class QueuePushResult : std::uint8_t {
    enqueued,
    semantic_duplicate,
    rejected,
    full,
    busy,
    sequence_exhausted,
};
enum class QueuePopResult : std::uint8_t { success, empty, busy };

struct QueueCounters final {
    std::uint64_t accepted{};
    std::uint64_t semantic_duplicates{};
    std::uint64_t rejected{};
    std::uint64_t dropped_full{};
    std::uint64_t dropped_busy{};
    std::uint64_t dropped_sequence_exhausted{};
    std::uint64_t pending{};
    [[nodiscard]] constexpr std::uint64_t losses() const noexcept {
        return rejected + dropped_full + dropped_busy + dropped_sequence_exhausted;
    }
};

template <typename Record, std::size_t Capacity>
class FixedCaptureQueue final {
    static_assert(Capacity != 0U);

public:
    FixedCaptureQueue() noexcept = default;
    FixedCaptureQueue(const FixedCaptureQueue&) = delete;
    FixedCaptureQueue& operator=(const FixedCaptureQueue&) = delete;

    [[nodiscard]] QueuePushResult try_push(const Record& record) noexcept {
        if (!valid_raw_record(record)) {
            rejected_.fetch_add(1U, std::memory_order_relaxed);
            return QueuePushResult::rejected;
        }
        const SemanticDedupeKey incoming = semantic_dedupe_key(record);
        QueueLock lock{*this};
        if (!lock) {
            dropped_busy_.fetch_add(1U, std::memory_order_relaxed);
            return QueuePushResult::busy;
        }
        if (incoming.valid && last_dedupe_.valid && incoming == last_dedupe_) {
            semantic_duplicates_.fetch_add(1U, std::memory_order_relaxed);
            return QueuePushResult::semantic_duplicate;
        }
        if (count_ == records_.size()) {
            dropped_full_.fetch_add(1U, std::memory_order_relaxed);
            return QueuePushResult::full;
        }
        if (next_sequence_ == (std::numeric_limits<std::uint64_t>::max)()) {
            dropped_sequence_exhausted_.fetch_add(1U, std::memory_order_relaxed);
            return QueuePushResult::sequence_exhausted;
        }
        const std::size_t tail = (head_ + count_) % records_.size();
        records_[tail] = record;
        records_[tail].sequence = next_sequence_++;
        if (incoming.valid) {
            last_dedupe_ = incoming;
        }
        ++count_;
        accepted_.fetch_add(1U, std::memory_order_relaxed);
        pending_.store(count_, std::memory_order_relaxed);
        return QueuePushResult::enqueued;
    }

    [[nodiscard]] QueuePopResult try_pop(Record& output) noexcept {
        QueueLock lock{*this};
        if (!lock) {
            return QueuePopResult::busy;
        }
        if (count_ == 0U) {
            return QueuePopResult::empty;
        }
        output = records_[head_];
        head_ = (head_ + 1U) % records_.size();
        --count_;
        pending_.store(count_, std::memory_order_relaxed);
        return QueuePopResult::success;
    }

    [[nodiscard]] QueueCounters counters() const noexcept {
        return QueueCounters{accepted_.load(std::memory_order_relaxed),
                             semantic_duplicates_.load(std::memory_order_relaxed),
                             rejected_.load(std::memory_order_relaxed),
                             dropped_full_.load(std::memory_order_relaxed),
                             dropped_busy_.load(std::memory_order_relaxed),
                             dropped_sequence_exhausted_.load(std::memory_order_relaxed),
                             pending_.load(std::memory_order_relaxed)};
    }

#if defined(SUNRISE_OPENING_AUTHORITY_CAPTURE_TEST)
    [[nodiscard]] bool testing_lock() noexcept { return try_lock(); }
    void testing_unlock() noexcept { unlock(); }
    void testing_set_next_sequence(std::uint64_t sequence) noexcept {
        next_sequence_ = sequence;
    }
#endif

private:
    class QueueLock final {
    public:
        explicit QueueLock(FixedCaptureQueue& queue) noexcept
            : queue_(queue), locked_(queue_.try_lock()) {}
        ~QueueLock() {
            if (locked_) {
                queue_.unlock();
            }
        }
        QueueLock(const QueueLock&) = delete;
        QueueLock& operator=(const QueueLock&) = delete;
        [[nodiscard]] explicit operator bool() const noexcept { return locked_; }

    private:
        FixedCaptureQueue& queue_;
        bool locked_{};
    };

    [[nodiscard]] bool try_lock() noexcept {
        return !lock_.test_and_set(std::memory_order_acquire);
    }
    void unlock() noexcept { lock_.clear(std::memory_order_release); }

    std::array<Record, Capacity> records_{};
    SemanticDedupeKey last_dedupe_{};
    std::size_t head_{};
    std::size_t count_{};
    std::uint64_t next_sequence_{1U};
    std::atomic_flag lock_ = ATOMIC_FLAG_INIT;
    std::atomic<std::uint64_t> accepted_{};
    std::atomic<std::uint64_t> semantic_duplicates_{};
    std::atomic<std::uint64_t> rejected_{};
    std::atomic<std::uint64_t> dropped_full_{};
    std::atomic<std::uint64_t> dropped_busy_{};
    std::atomic<std::uint64_t> dropped_sequence_exhausted_{};
    std::atomic<std::uint64_t> pending_{};
};

inline constexpr std::size_t kDialogueQueueCapacity = 8U;
inline constexpr std::size_t kDirectiveQueueCapacity = 32U;
using DialogueCaptureQueue = FixedCaptureQueue<DialogueApplyRecord, kDialogueQueueCapacity>;
using DirectiveCaptureQueue = FixedCaptureQueue<DirectiveApplyRecord, kDirectiveQueueCapacity>;

/** Explicit Type-53 terminal/consumed-generation evidence; consumed never means completed. */
struct Type53TerminalEvent final {
    CaptureMetadata metadata{};
    CaptureContext context{};
    StaticConsumerProvenance static_consumer{};
    std::uint32_t record_index{};
    DialogueRecordFields record{};
    std::uint32_t selector{};
    std::uint32_t bank_handle{};
    std::uint32_t processed_generation_before{};
    std::uint32_t processed_generation_after{};
    std::uint64_t terminal_handle{};
    bool terminal_handle_valid{};
    bool generation_store_observed{};
};

enum class PresentationStage : std::uint8_t { start, end_or_teardown_unrecovered };

struct Type53PresentationEvent final {
    CaptureMetadata metadata{};
    CaptureContext context{};
    std::uint64_t terminal_call_id{};
    std::uint64_t terminal_handle{};
    std::uint64_t presentation_handle{};
    PresentationStage stage{PresentationStage::start};
    bool explicit_handle_correlation{};
};

[[nodiscard]] bool valid_type53_terminal_event(const Type53TerminalEvent& event) noexcept;
[[nodiscard]] bool valid_type53_presentation_event(const Type53PresentationEvent& event) noexcept;

/** Copied identity fields from +1009C40; the content pointer itself is never retained. */
struct Type68ContentEvent final {
    CaptureMetadata metadata{};
    CaptureContext context{};
    std::uint32_t content_definition{};
    std::uint32_t content_bank{};
    std::uint64_t content_identity_fingerprint{};
    bool content_resolved{};
};

enum class Type68TransitionKind : std::uint8_t {
    reconcile,
    remove,
    install,
    status,
};

struct Type68TransitionEvent final {
    CaptureMetadata metadata{};
    CaptureContext context{};
    Type68TransitionKind kind{};
    std::uint32_t record_index{};
    std::uint32_t event_key{};
    std::int8_t lifecycle{};
    std::int32_t external_status{};
    std::int32_t auxiliary{};
    bool inside_exact_apply_call{};
    bool outcome_observed{};
};

struct ManagerEntrySnapshot final {
    std::uint32_t kind{};
    std::uint32_t hash{};
    std::int32_t status{};
    std::int32_t auxiliary{};
    friend constexpr bool operator==(ManagerEntrySnapshot, ManagerEntrySnapshot) noexcept = default;
};

struct ManagerSnapshot final {
    std::array<ManagerEntrySnapshot, kManagerMaximumEntries> entries{};
    std::uint32_t count{};
    bool readiness_true{};
    bool aligned_identity{};
    bool exact_owner_current{};
    bool valid{};
    friend constexpr bool operator==(ManagerSnapshot, ManagerSnapshot) noexcept = default;
};

struct Type68ManagerDeltaEvent final {
    CaptureMetadata metadata{};
    CaptureContext context{};
    ManagerSnapshot before{};
    ManagerSnapshot after{};
    std::uint32_t event_hash{};
    bool inside_exact_apply_call{};
    bool add_outcome_observed{};
    bool materialize_outcome_observed{};
    bool terminal_predicate_observed{};
    bool terminal_predicate_result{};
};

[[nodiscard]] bool valid_type68_content_event(const Type68ContentEvent& event) noexcept;
[[nodiscard]] bool valid_type68_transition_event(const Type68TransitionEvent& event) noexcept;
[[nodiscard]] bool valid_manager_snapshot(const ManagerSnapshot& snapshot) noexcept;
[[nodiscard]] bool valid_type68_manager_delta_event(
    const Type68ManagerDeltaEvent& event) noexcept;

/**
 * Default context projection. It retains typed lifetime domains and presence bits, but never the
 * native wrapper pointer or complete native handle from the secured raw record.
 */
struct DefaultContextFields final {
    std::uint32_t presence_mask{};
    state::activity::NativeActivationKey activation_key{};
    std::uint64_t native_identity{};
    state::activity::ActivityInstanceKey activity{};
    state::activity::RosterGraphGeneration roster_generation{};
    state::activity::PublicationGeneration authority_publication{};
    std::uint64_t run_token{};
    std::uint64_t correlation_token{};
    activity_lifecycle::NativeActivationState activation_state{
        activity_lifecycle::NativeActivationState::empty};
    bool complete_activation_token_present{};
};

/** Default telemetry is an explicit scalar whitelist and carries full build/phase provenance. */
struct DialogueDefaultLogFields final {
    ApplyPhaseTelemetry telemetry{};
    DefaultContextFields entry_context{};
    StaticConsumerProvenance static_consumer{};
    std::uint64_t sequence{};
    std::uint64_t resolver_body_fingerprint{};
    std::uint64_t post_commit_cache_fingerprint{};
    std::uint32_t record_zero_generation{};
    std::uint32_t record_zero_mode{};
    ApplyValidity valid{};
};

struct DirectiveDefaultLogFields final {
    ApplyPhaseTelemetry telemetry{};
    DefaultContextFields entry_context{};
    StaticConsumerProvenance static_consumer{};
    std::uint64_t sequence{};
    std::uint64_t resolver_body_fingerprint{};
    std::uint64_t post_commit_cache_fingerprint{};
    std::array<DirectiveEntryFields, kDirectiveEntryCount> entries{};
    std::int32_t selector{};
    ApplyValidity valid{};
};

[[nodiscard]] bool default_log_fields(const DialogueApplyRecord& record,
                                      DialogueDefaultLogFields& output) noexcept;
[[nodiscard]] bool default_log_fields(const DirectiveApplyRecord& record,
                                      DirectiveDefaultLogFields& output) noexcept;

static_assert(sizeof(void*) == 8U, "The recovered PC ABI is x64-only");
static_assert(sizeof(PacketReference16) == kPacketWrapperExtent);
static_assert(offsetof(PacketReference16, resolver_input) == kPacketResolverInputOffset);
static_assert(sizeof(EntryWrapperFields12) == kPacketWrapperMeaningfulBytes);
static_assert(offsetof(EntryWrapperFields12, resolver_input) == sizeof(std::uint32_t));
static_assert(sizeof(DialogueRecordLayout) == kDialogueRecordStride);
static_assert(offsetof(DialogueRecordLayout, reference) == 0x10U);
static_assert(offsetof(DialogueRecordLayout, generation) == 0x18U);
static_assert(offsetof(DialogueRecordLayout, mode) == 0x1CU);
static_assert(sizeof(DialogueDecodedLayout) == kDialogueBodyBytes);
static_assert(offsetof(DialogueDecodedLayout, records) == kDialogueRecordOffset);
static_assert(sizeof(DirectiveScalarBlockLayout) == 0x38U);
static_assert(sizeof(DirectiveTargetLayout) == 0x24U);
static_assert(offsetof(DirectiveTargetLayout, boolean) == 0x20U);
static_assert(sizeof(DirectiveEntryLayout) == kDirectiveEntryStride);
static_assert(offsetof(DirectiveEntryLayout, scalar) == 0x10U);
static_assert(offsetof(DirectiveEntryLayout, biased_values) == 0x48U);
static_assert(offsetof(DirectiveEntryLayout, auxiliary_enum_a) == 0x58U);
static_assert(offsetof(DirectiveEntryLayout, reference) == 0x5CU);
static_assert(offsetof(DirectiveEntryLayout, auxiliary_enum_b) == 0x64U);
static_assert(offsetof(DirectiveEntryLayout, targets) == 0x68U);
static_assert(sizeof(DirectiveDecodedLayout) == kDirectiveBodyBytes);
static_assert(offsetof(DirectiveDecodedLayout, entries) == kDirectiveEntryOffset);
static_assert(offsetof(DirectiveDecodedLayout, selector) == kDirectiveSelectorOffset);
static_assert(std::is_trivially_copyable_v<DialogueApplyRecord>);
static_assert(std::is_trivially_copyable_v<DirectiveApplyRecord>);
static_assert(std::is_trivially_copyable_v<Type53TerminalEvent>);
static_assert(std::is_trivially_copyable_v<Type68ManagerDeltaEvent>);
static_assert(std::atomic<std::uint64_t>::is_always_lock_free,
              "Authority capture requires lock-free counters");

} // namespace sunrise::client::hooks::bootflow::opening_authority
