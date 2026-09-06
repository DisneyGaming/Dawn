#include "client/hooks/bootflow/opening_authority/type26_hold_capture.h"

#include <algorithm>
#include <cstring>
#include <cwchar>

#if defined(_WIN32)
#include <Windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#endif

namespace sunrise::client::hooks::bootflow::opening_authority::type26_hold {
namespace {

inline constexpr std::array<std::byte, 25U> kAuthorityApplyPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x08}, std::byte{0x48}, std::byte{0x89}, std::byte{0x74},
    std::byte{0x24}, std::byte{0x10}, std::byte{0x57}, std::byte{0x48},
    std::byte{0x83}, std::byte{0xEC}, std::byte{0x30}, std::byte{0x44},
    std::byte{0x8B}, std::byte{0x02}, std::byte{0x48}, std::byte{0x8B},
    std::byte{0xF1}, std::byte{0x4C}, std::byte{0x8B}, std::byte{0x4A},
    std::byte{0x08}};
inline constexpr std::array<std::byte, 26U> kReconcilePrefix{
    std::byte{0x40}, std::byte{0x53}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x20}, std::byte{0x48}, std::byte{0x8B},
    std::byte{0xD9}, std::byte{0xE8}, std::byte{0x62}, std::byte{0x26},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x80}, std::byte{0xBB},
    std::byte{0x81}, std::byte{0x01}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x75}, std::byte{0x08}, std::byte{0x48},
    std::byte{0x8B}, std::byte{0xCB}};
inline constexpr std::array<std::byte, 31U> kEnumeratePrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x10}, std::byte{0x57}, std::byte{0x48}, std::byte{0x81},
    std::byte{0xEC}, std::byte{0x20}, std::byte{0x01}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x48}, std::byte{0x8B}, std::byte{0x05},
    std::byte{0x34}, std::byte{0xA1}, std::byte{0x6B}, std::byte{0x01},
    std::byte{0x48}, std::byte{0x33}, std::byte{0xC4}, std::byte{0x48},
    std::byte{0x89}, std::byte{0x84}, std::byte{0x24}, std::byte{0x10},
    std::byte{0x01}, std::byte{0x00}, std::byte{0x00}};
inline constexpr std::array<std::byte, 30U> kSubscriberPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x74}, std::byte{0x24},
    std::byte{0x20}, std::byte{0x57}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x20}, std::byte{0x44}, std::byte{0x8B},
    std::byte{0x19}, std::byte{0x48}, std::byte{0x8B}, std::byte{0xF1},
    std::byte{0x4C}, std::byte{0x8B}, std::byte{0x49}, std::byte{0x08},
    std::byte{0x41}, std::byte{0x8B}, std::byte{0xD3}, std::byte{0x48},
    std::byte{0x8B}, std::byte{0x05}, std::byte{0x92}, std::byte{0x76},
    std::byte{0xA4}, std::byte{0x01}};
inline constexpr std::array<std::byte, 24U> kClearPrefix{
    std::byte{0x48}, std::byte{0x83}, std::byte{0xEC}, std::byte{0x28},
    std::byte{0x44}, std::byte{0x8B}, std::byte{0x01}, std::byte{0x41},
    std::byte{0x8B}, std::byte{0xC0}, std::byte{0x48}, std::byte{0x8B},
    std::byte{0x49}, std::byte{0x08}, std::byte{0x41}, std::byte{0x81},
    std::byte{0xE0}, std::byte{0xFF}, std::byte{0x1F}, std::byte{0x00},
    std::byte{0x00}, std::byte{0xC1}, std::byte{0xF8}, std::byte{0x0D}};
inline constexpr std::array<std::byte, 25U> kAttachPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x18}, std::byte{0x57}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x20}, std::byte{0x8B}, std::byte{0xC2},
    std::byte{0x8B}, std::byte{0xDA}, std::byte{0x25}, std::byte{0xFF},
    std::byte{0x1F}, std::byte{0x00}, std::byte{0x00}, std::byte{0x48},
    std::byte{0x8B}, std::byte{0xF9}, std::byte{0x44}, std::byte{0x8B},
    std::byte{0xC0}};
inline constexpr std::array<std::byte, 26U> kBuilderPrefix{
    std::byte{0x40}, std::byte{0x55}, std::byte{0x53}, std::byte{0x57},
    std::byte{0x41}, std::byte{0x54}, std::byte{0x41}, std::byte{0x56},
    std::byte{0x48}, std::byte{0x8D}, std::byte{0xAC}, std::byte{0x24},
    std::byte{0x20}, std::byte{0xF0}, std::byte{0xFF}, std::byte{0xFF},
    std::byte{0xB8}, std::byte{0xE0}, std::byte{0x10}, std::byte{0x00},
    std::byte{0x00}, std::byte{0xE8}, std::byte{0x56}, std::byte{0xCF},
    std::byte{0xE8}, std::byte{0x00}};
inline constexpr std::array<std::byte, 25U> kSensePrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x10}, std::byte{0x57}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x20}, std::byte{0x44}, std::byte{0x8B},
    std::byte{0x09}, std::byte{0x48}, std::byte{0x8B}, std::byte{0xD9},
    std::byte{0x41}, std::byte{0x8B}, std::byte{0xC1}, std::byte{0x48},
    std::byte{0x8B}, std::byte{0xFA}, std::byte{0xC1}, std::byte{0xF8},
    std::byte{0x0D}};

inline constexpr std::uint8_t kKnownBranchMask = 0x0FU;
// File identity is measured off-hook; keep the sequential buffer's stack use bounded
// without introducing an allocation/failure mode.
inline constexpr std::size_t kFileReadBufferBytes = 8U * 1024U;

#if defined(_WIN32) && defined(_MSC_VER)
[[nodiscard]] int native_read_exception_filter(unsigned long exceptionCode) noexcept {
    switch (exceptionCode) {
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_IN_PAGE_ERROR:
    case EXCEPTION_GUARD_PAGE:
        return EXCEPTION_EXECUTE_HANDLER;
    default:
        return EXCEPTION_CONTINUE_SEARCH;
    }
}
#endif

[[nodiscard]] bool safe_copy_exact(void* destination,
                                   const void* source,
                                   std::size_t bytes) noexcept {
    if (destination == nullptr || source == nullptr || bytes == 0U) {
        return false;
    }
#if defined(_WIN32) && defined(_MSC_VER)
    __try {
        std::memcpy(destination, source, bytes);
        return true;
    } __except (native_read_exception_filter(GetExceptionCode())) {
        return false;
    }
#else
    (void)destination;
    (void)source;
    (void)bytes;
    return false;
#endif
}

[[nodiscard]] const void* offset_address(const void* base, std::size_t offset) noexcept {
    const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(base);
    if (address == 0U || address > (std::numeric_limits<std::uintptr_t>::max)() - offset) {
        return nullptr;
    }
    return reinterpret_cast<const void*>(address + offset);
}

template <typename Body>
[[nodiscard]] bool copy_instance_body(const void* instance,
                                      std::size_t offset,
                                      Body& output) noexcept {
    Body temporary{};
    if (!safe_copy_exact(temporary.data(), offset_address(instance, offset), temporary.size())) {
        return false;
    }
    output = temporary;
    return true;
}

[[nodiscard]] bool valid_metadata(const CaptureMetadata& metadata) noexcept {
    return metadata.capture_epoch != 0U && metadata.call_id != 0U;
}

[[nodiscard]] CaptureBuildResult validate_admission(
    const ActivationContext& context,
    const HoldDefinitionIdentity& identity) noexcept {
    if (identity.definition != kWeaponDownIdentity.definition
        && identity.definition != kNoCombatAbilitiesIdentity.definition) {
        return CaptureBuildResult::wrong_definition;
    }
    if (!admitted_hold_definition(identity)) {
        return CaptureBuildResult::wrong_resource_gate;
    }
    if (!exact_current_activation(context) || context.registry != identity.registry
        || context.bubble != identity.bubble) {
        return CaptureBuildResult::wrong_activation;
    }
    return CaptureBuildResult::complete;
}

[[nodiscard]] bool valid_authority_wire(const AuthorityWireSnapshot& wire) noexcept {
    if (wire.bit_count == 0U || wire.tap_generation == 0U
        || wire.bit_order != WireBitOrder::most_significant_bit_first) {
        return false;
    }
    const std::size_t bytes = (static_cast<std::size_t>(wire.bit_count) + 7U) / 8U;
    if (bytes != wire.byte_count || bytes > wire.bytes.size()) {
        return false;
    }
    if (std::any_of(wire.bytes.begin() + static_cast<std::ptrdiff_t>(bytes),
                    wire.bytes.end(),
                    [](std::byte value) { return value != std::byte{}; })) {
        return false;
    }
    const unsigned remainder = wire.bit_count % 8U;
    if (remainder == 0U) {
        return true;
    }
    const std::uint8_t unusedMask = static_cast<std::uint8_t>((1U << (8U - remainder)) - 1U);
    return (std::to_integer<std::uint8_t>(wire.bytes[bytes - 1U]) & unusedMask) == 0U;
}

[[nodiscard]] bool valid_sense_wire(const SenseWireSnapshot& wire) noexcept {
    return wire.bit_count == kSenseWireBits && wire.tap_generation != 0U
           && wire.bit_order == WireBitOrder::most_significant_bit_first
           && wire.complete_without_underflow && wire.complete_without_trailing_bits
           && (std::to_integer<std::uint8_t>(wire.bytes.back()) & 0x7FU) == 0U;
}

[[nodiscard]] bool valid_reconcile_observation(const ReconcileCaptureInput& input) noexcept {
    const std::uint8_t mask = static_cast<std::uint8_t>(input.branches);
    if ((mask & static_cast<std::uint8_t>(~kKnownBranchMask)) != 0U
        || input.live_object_count > input.live_object_datums.size()) {
        return false;
    }
    const bool subscriberBranch = branch_contains(input.branches, ReconcileBranch::subscriber);
    if (subscriberBranch != input.subscriber.invoked) {
        return false;
    }
    if (!input.subscriber.invoked) {
        return input.subscriber.service_identity == 0U
               && input.subscriber.service_vtable == 0U
               && input.subscriber.slot_1a8_target == 0U;
    }
    return input.subscriber.service_identity != 0U && input.subscriber.service_vtable != 0U
           && input.subscriber.slot_1a8_target != 0U;
}

[[nodiscard]] bool digest_present(KeyedDigest128 digest) noexcept {
    return digest.high != 0U || digest.low != 0U;
}

[[nodiscard]] bool valid_projection(const TelemetryProjectionContext& projection) noexcept {
    return projection.privacy_key_epoch != 0U && projection.source_session_pseudonym != 0U
           && projection.local_player_pseudonym != 0U
           && projection.producer_thread_ordinal != 0U;
}

[[nodiscard]] TelemetryProvenance make_provenance(
    const ActivationContext& context,
    const HoldDefinitionIdentity& identity,
    const TelemetryProjectionContext& projection) noexcept {
    return TelemetryProvenance{kPackedBuildTelemetryId,
                               context.module_generation,
                               context.activation_generation,
                               projection.source_session_pseudonym,
                               projection.privacy_key_epoch,
                               context.roster_epoch,
                               context.roster_publication_sequence,
                               projection.local_player_pseudonym,
                               context.registry,
                               context.bubble,
                               identity.definition,
                               identity.index};
}

[[nodiscard]] TelemetryEvent make_event(
    const CaptureMetadata& metadata,
    const TelemetryProjectionContext& projection) noexcept {
    return TelemetryEvent{metadata.capture_epoch,
                          metadata.monotonic_tick,
                          metadata.call_id,
                          projection.producer_thread_ordinal};
}

#if defined(_WIN32)
[[nodiscard]] bool cng_succeeded(NTSTATUS status) noexcept { return status >= 0; }

[[nodiscard]] bool measure_file(const wchar_t* path, PackedRuntimeIdentity& output) noexcept {
    output = {};
    HANDLE file = CreateFileW(path,
                              GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              nullptr,
                              OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                              nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    LARGE_INTEGER size{};
    bool complete = GetFileSizeEx(file, &size) != FALSE && size.QuadPart >= 0;
    BCRYPT_ALG_HANDLE algorithm{};
    BCRYPT_HASH_HANDLE hash{};
    if (complete) {
        complete = cng_succeeded(
            BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0));
    }
    if (complete) {
        complete = cng_succeeded(
            BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0));
    }
    std::array<std::byte, kFileReadBufferBytes> buffer{};
    while (complete) {
        DWORD transferred{};
        if (ReadFile(file,
                     buffer.data(),
                     static_cast<DWORD>(buffer.size()),
                     &transferred,
                     nullptr)
            == FALSE) {
            complete = false;
            break;
        }
        if (transferred == 0U) {
            break;
        }
        complete = cng_succeeded(BCryptHashData(
            hash, reinterpret_cast<PUCHAR>(buffer.data()), transferred, 0));
    }
    Sha256 sha{};
    if (complete) {
        complete = cng_succeeded(BCryptFinishHash(
            hash, reinterpret_cast<PUCHAR>(sha.data()), static_cast<ULONG>(sha.size()), 0));
    }
    if (hash != nullptr) {
        (void)BCryptDestroyHash(hash);
    }
    if (algorithm != nullptr) {
        (void)BCryptCloseAlgorithmProvider(algorithm, 0);
    }
    complete = CloseHandle(file) != FALSE && complete;
    if (!complete) {
        return false;
    }
    output = PackedRuntimeIdentity{static_cast<std::uint64_t>(size.QuadPart), sha};
    return true;
}

[[nodiscard]] bool executable_target(const void* moduleBase,
                                     const void* target,
                                     std::size_t bytes) noexcept {
    MEMORY_BASIC_INFORMATION information{};
    if (VirtualQuery(target, &information, sizeof information) != sizeof information
        || information.AllocationBase != moduleBase || information.State != MEM_COMMIT
        || (information.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0U) {
        return false;
    }
    const DWORD executableMask = PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE
                                 | PAGE_EXECUTE_WRITECOPY;
    if ((information.Protect & executableMask) == 0U) {
        return false;
    }
    const std::uintptr_t region = reinterpret_cast<std::uintptr_t>(information.BaseAddress);
    const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(target);
    return address >= region && bytes <= information.RegionSize
           && address - region <= information.RegionSize - bytes;
}
#endif

} // namespace

NativeBoundaryDescriptor native_boundary(NativeSurface surface) noexcept {
    switch (surface) {
    case NativeSurface::authority_apply:
        return {0x9F1940U, kAuthorityApplyPrefix, NativeAbi::instance_packet};
    case NativeSurface::reconcile:
        return {0x9EF8A0U, kReconcilePrefix, NativeAbi::instance_only};
    case NativeSurface::enumerate_materialize:
        return {0x9EF940U, kEnumeratePrefix, NativeAbi::instance_only};
    case NativeSurface::subscriber_terminal:
        return {0x9F25C0U, kSubscriberPrefix, NativeAbi::instance_only};
    case NativeSurface::clear_terminal:
        return {0x9F1310U, kClearPrefix, NativeAbi::instance_only};
    case NativeSurface::attach_materialize:
        return {0x9F2760U, kAttachPrefix, NativeAbi::instance_object_datum};
    case NativeSurface::entity_effect_builder:
        return {0x9EFBC0U, kBuilderPrefix, NativeAbi::entity_effect_builder};
    case NativeSurface::sense_export:
        return {0x9F0650U, kSensePrefix, NativeAbi::instance_packet_output};
    case NativeSurface::count:
        break;
    }
    return {};
}

bool native_prefix_matches(NativeSurface surface,
                           std::span<const std::byte> observed) noexcept {
    const NativeBoundaryDescriptor boundary = native_boundary(surface);
    return boundary.mapped_rva != 0U && !boundary.mapped_prefix.empty()
           && observed.size() >= boundary.mapped_prefix.size()
           && std::equal(boundary.mapped_prefix.begin(),
                         boundary.mapped_prefix.end(),
                         observed.begin());
}

bool measure_pinned_packed_runtime(PackedRuntimeIdentity& output) noexcept {
#if defined(_WIN32)
    return measure_file(kPinnedPackedRuntimePath, output);
#else
    output = {};
    return false;
#endif
}

LiveRuntimeValidation validate_live_runtime_group(void* liveModuleBase,
                                                   std::uint64_t moduleGeneration,
                                                   LiveRuntimeAddressGroup& output) noexcept {
    output = {};
#if defined(_WIN32)
    if (liveModuleBase == nullptr || moduleGeneration == 0U) {
        return LiveRuntimeValidation::invalid_arguments;
    }
    const HMODULE mainModule = GetModuleHandleW(nullptr);
    if (mainModule == nullptr || liveModuleBase != mainModule) {
        return LiveRuntimeValidation::wrong_process_main_module;
    }
    // Admission accepts one exact pinned path. A longer result is necessarily a mismatch,
    // so a path-sized buffer is sufficient and keeps this validator's stack bounded.
    std::array<wchar_t, std::size(kPinnedPackedRuntimePath)> modulePath{};
    const DWORD pathLength = GetModuleFileNameW(
        mainModule, modulePath.data(), static_cast<DWORD>(modulePath.size()));
    if (pathLength == 0U || pathLength >= modulePath.size()
        || _wcsicmp(modulePath.data(), kPinnedPackedRuntimePath) != 0) {
        return LiveRuntimeValidation::module_path_mismatch;
    }
    PackedRuntimeIdentity measured{};
    if (!measure_file(modulePath.data(), measured) || !matches_pinned_packed_runtime(measured)) {
        return LiveRuntimeValidation::packed_file_identity_mismatch;
    }

    IMAGE_DOS_HEADER dos{};
    if (!safe_copy_exact(&dos, liveModuleBase, sizeof dos) || dos.e_magic != IMAGE_DOS_SIGNATURE
        || dos.e_lfanew <= 0) {
        return LiveRuntimeValidation::invalid_pe_image;
    }
    IMAGE_NT_HEADERS64 nt{};
    if (!safe_copy_exact(&nt,
                         offset_address(liveModuleBase, static_cast<std::size_t>(dos.e_lfanew)),
                         sizeof nt)
        || nt.Signature != IMAGE_NT_SIGNATURE
        || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC
        || nt.OptionalHeader.SizeOfImage == 0U) {
        return LiveRuntimeValidation::invalid_pe_image;
    }
    const std::size_t imageBytes = nt.OptionalHeader.SizeOfImage;
    std::array<std::uintptr_t, kNativeSurfaceCount> candidates{};
    const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(liveModuleBase);
    for (std::size_t index = 0U; index < candidates.size(); ++index) {
        const NativeSurface surface = static_cast<NativeSurface>(index);
        const NativeBoundaryDescriptor boundary = native_boundary(surface);
        if (boundary.mapped_rva > imageBytes
            || boundary.mapped_prefix.size() > imageBytes - boundary.mapped_rva
            || boundary.mapped_rva > (std::numeric_limits<std::uintptr_t>::max)() - base) {
            return LiveRuntimeValidation::target_out_of_range;
        }
        const void* target = reinterpret_cast<const void*>(base + boundary.mapped_rva);
        if (!executable_target(liveModuleBase, target, boundary.mapped_prefix.size())) {
            return LiveRuntimeValidation::target_not_executable;
        }
        std::array<std::byte, 31U> observed{};
        if (!safe_copy_exact(observed.data(), target, boundary.mapped_prefix.size())
            || !native_prefix_matches(
                surface, {observed.data(), boundary.mapped_prefix.size()})) {
            return LiveRuntimeValidation::mapped_prefix_mismatch;
        }
        candidates[index] = base + boundary.mapped_rva;
    }
    output = LiveRuntimeAddressGroup{
        candidates, moduleGeneration, nt.OptionalHeader.SizeOfImage};
    return LiveRuntimeValidation::valid;
#else
    (void)liveModuleBase;
    (void)moduleGeneration;
    return LiveRuntimeValidation::invalid_arguments;
#endif
}

bool authority_fields(const AuthorityBody& body, AuthorityFields& output) noexcept {
    AuthorityLayout layout{};
    std::memcpy(&layout, body.data(), sizeof layout);
    if (layout.opaque_boolean_0 > 1U || layout.suppress_linked_enumeration > 1U) {
        return false;
    }
    output = AuthorityFields{layout.opaque_boolean_0,
                             layout.suppress_linked_enumeration,
                             layout.clear_generation,
                             layout.subscriber_argument,
                             layout.subscriber_generation,
                             layout.sense_echo,
                             layout.linked_selector};
    return true;
}

bool sense_fields(const SenseBody& body, SenseFields& output) noexcept {
    SenseLayout layout{};
    std::memcpy(&layout, body.data(), sizeof layout);
    if (layout.linked_content_item_present > 1U) {
        return false;
    }
    output = SenseFields{layout.clear_generation,
                         layout.subscriber_generation,
                         layout.authority_echo,
                         layout.linked_content_item_present};
    return true;
}

bool exact_current_activation(const ActivationContext& context) noexcept {
    return context.packed_runtime_sha256 == kPinnedPackedRuntimeSha256
           && context.module_generation != 0U && context.activation_generation != 0U
           && context.source_session != 0U && context.roster_epoch != 0U
           && context.roster_publication_sequence != 0U
           && context.native_activity_wrapper != 0U && context.registry == kActivityRegistry
           && context.bubble == kOpeningBubble && context.local_player_bit != 0U
           && std::has_single_bit(context.local_player_bit)
           && context.state == ActivationState::current;
}

bool exact_activation_continuity(const ActivationContext& entry,
                                 const ActivationContext& exit) noexcept {
    return exact_current_activation(entry) && exact_current_activation(exit) && entry == exit;
}

WireSnapshotResult make_authority_wire_snapshot(const WireSourceEvidence& source,
                                                 AuthorityWireSnapshot& output) noexcept {
    output = {};
    if (source.bit_order != WireBitOrder::most_significant_bit_first) {
        return WireSnapshotResult::unsupported_bit_order;
    }
    if (source.bit_count == 0U || source.tap_generation == 0U
        || source.bit_count > kMaximumAuthorityWireBytes * 8U) {
        return WireSnapshotResult::invalid_length;
    }
    const std::size_t bytes = (source.bit_count + 7U) / 8U;
    if (source.bytes.size() != bytes
        || source.bit_count > (std::numeric_limits<std::uint16_t>::max)()) {
        return WireSnapshotResult::invalid_length;
    }
    AuthorityWireSnapshot candidate{};
    if (!safe_copy_exact(candidate.bytes.data(), source.bytes.data(), bytes)) {
        return WireSnapshotResult::unreadable;
    }
    const unsigned remainder = static_cast<unsigned>(source.bit_count % 8U);
    if (remainder != 0U) {
        const std::uint8_t mask = static_cast<std::uint8_t>((1U << (8U - remainder)) - 1U);
        if ((std::to_integer<std::uint8_t>(candidate.bytes[bytes - 1U]) & mask) != 0U) {
            return WireSnapshotResult::nonzero_unused_bits;
        }
    }
    candidate.byte_count = static_cast<std::uint16_t>(bytes);
    candidate.bit_count = static_cast<std::uint16_t>(source.bit_count);
    candidate.tap_generation = source.tap_generation;
    candidate.bit_order = source.bit_order;
    candidate.complete_without_underflow = source.complete_without_underflow;
    candidate.complete_without_trailing_bits = source.complete_without_trailing_bits;
    candidate.dynamic_type34_complete = source.dynamic_type34_complete;
    output = candidate;
    return WireSnapshotResult::complete;
}

WireSnapshotResult make_sense_wire_snapshot(const WireSourceEvidence& source,
                                             SenseWireSnapshot& output) noexcept {
    output = {};
    if (source.bit_order != WireBitOrder::most_significant_bit_first) {
        return WireSnapshotResult::unsupported_bit_order;
    }
    if (source.bit_count != kSenseWireBits || source.bytes.size() != kSenseWireBytes
        || source.tap_generation == 0U) {
        return WireSnapshotResult::invalid_length;
    }
    SenseWireSnapshot candidate{};
    if (!safe_copy_exact(candidate.bytes.data(), source.bytes.data(), candidate.bytes.size())) {
        return WireSnapshotResult::unreadable;
    }
    if ((std::to_integer<std::uint8_t>(candidate.bytes.back()) & 0x7FU) != 0U) {
        return WireSnapshotResult::nonzero_unused_bits;
    }
    candidate.bit_count = static_cast<std::uint8_t>(source.bit_count);
    candidate.tap_generation = source.tap_generation;
    candidate.bit_order = source.bit_order;
    candidate.complete_without_underflow = source.complete_without_underflow;
    candidate.complete_without_trailing_bits = source.complete_without_trailing_bits;
    output = candidate;
    return WireSnapshotResult::complete;
}

CaptureBuildResult prepare_apply_capture(PendingApplyCapture& pending,
                                         const ActivationContext& entryContext,
                                         const HoldDefinitionIdentity& identity,
                                         CaptureMetadata metadata,
                                         std::uint64_t authorityPublicationSequence,
                                         const void* instance,
                                         const PacketReference* unresolvedPacket,
                                         const AuthorityWireSnapshot& owningWire) noexcept {
    pending.record_ = {};
    pending.ready_ = false;
    if (instance == nullptr || unresolvedPacket == nullptr) {
        return CaptureBuildResult::null_pointer;
    }
    const CaptureBuildResult admission = validate_admission(entryContext, identity);
    if (admission != CaptureBuildResult::complete) {
        return admission;
    }
    if (!valid_metadata(metadata) || authorityPublicationSequence == 0U) {
        return CaptureBuildResult::invalid_observation;
    }
    if (!valid_authority_wire(owningWire)) {
        return CaptureBuildResult::invalid_wire;
    }
    std::uint32_t liveDefinition{};
    PacketReference packetCopy{};
    ApplyCaptureRecord record{};
    if (!safe_copy_exact(&liveDefinition, instance, sizeof liveDefinition)
        || !safe_copy_exact(&packetCopy, unresolvedPacket, sizeof packetCopy)
        || !copy_instance_body(instance, kAuthorityCacheOffset, record.cache_before)
        || !safe_copy_exact(&record.dirty_before,
                            offset_address(instance, kDirtyByteOffset),
                            sizeof record.dirty_before)) {
        return CaptureBuildResult::unreadable;
    }
    if (liveDefinition != identity.definition) {
        return CaptureBuildResult::wrong_definition;
    }
    if (packetCopy.key != kHoldAuthoritySchema) {
        return CaptureBuildResult::wrong_schema;
    }
    record.entry_context = entryContext;
    record.identity = identity;
    record.metadata = metadata;
    record.authority_publication_sequence = authorityPublicationSequence;
    record.instance_identity = reinterpret_cast<std::uintptr_t>(instance);
    record.packet_schema = packetCopy.key;
    record.packet_pad = packetCopy.pad;
    record.packet_payload_was_nonnull = packetCopy.data != nullptr;
    record.inbound_wire = owningWire;
    pending.record_ = record;
    pending.ready_ = true;
    return CaptureBuildResult::ready;
}

CaptureBuildResult finish_apply_capture(PendingApplyCapture& pending,
                                        const ActivationContext& exitContext,
                                        const void* instance,
                                        ApplyCaptureRecord& output) noexcept {
    output = {};
    if (!pending.ready_) {
        return CaptureBuildResult::consumed;
    }
    ApplyCaptureRecord record = pending.record_;
    pending.record_ = {};
    pending.ready_ = false;
    if (!exact_activation_continuity(record.entry_context, exitContext)) {
        return CaptureBuildResult::activation_changed;
    }
    if (instance == nullptr
        || reinterpret_cast<std::uintptr_t>(instance) != record.instance_identity) {
        return CaptureBuildResult::invalid_observation;
    }
    if (!copy_instance_body(instance, kAuthorityCacheOffset, record.cache_after)
        || !safe_copy_exact(&record.dirty_after,
                            offset_address(instance, kDirtyByteOffset),
                            sizeof record.dirty_after)) {
        return CaptureBuildResult::unreadable;
    }
    record.exit_context = exitContext;
    record.incoming_decoded_after_resolver = record.cache_after;
    record.equal_before = record.cache_before == record.cache_after;
    output = record;
    return CaptureBuildResult::complete;
}

CaptureBuildResult prepare_reconcile_capture(PendingReconcileCapture& pending,
                                             const ActivationContext& entryContext,
                                             const HoldDefinitionIdentity& identity,
                                             CaptureMetadata metadata,
                                             const void* instance) noexcept {
    pending.record_ = {};
    pending.ready_ = false;
    if (instance == nullptr) {
        return CaptureBuildResult::null_pointer;
    }
    const CaptureBuildResult admission = validate_admission(entryContext, identity);
    if (admission != CaptureBuildResult::complete) {
        return admission;
    }
    if (!valid_metadata(metadata)) {
        return CaptureBuildResult::invalid_observation;
    }
    ReconcileCaptureRecord record{};
    std::uint32_t liveDefinition{};
    if (!safe_copy_exact(&liveDefinition, instance, sizeof liveDefinition)
        || !copy_instance_body(instance, kAuthorityCacheOffset, record.authority_before)
        || !copy_instance_body(instance, kSenseCacheOffset, record.sense_before)) {
        return CaptureBuildResult::unreadable;
    }
    if (liveDefinition != identity.definition) {
        return CaptureBuildResult::wrong_definition;
    }
    record.entry_context = entryContext;
    record.identity = identity;
    record.metadata = metadata;
    record.instance_identity = reinterpret_cast<std::uintptr_t>(instance);
    pending.record_ = record;
    pending.ready_ = true;
    return CaptureBuildResult::ready;
}

CaptureBuildResult finish_reconcile_capture(PendingReconcileCapture& pending,
                                            const ActivationContext& exitContext,
                                            const void* instance,
                                            const ReconcileCaptureInput& observation,
                                            ReconcileCaptureRecord& output) noexcept {
    output = {};
    if (!pending.ready_) {
        return CaptureBuildResult::consumed;
    }
    ReconcileCaptureRecord record = pending.record_;
    pending.record_ = {};
    pending.ready_ = false;
    if (!exact_activation_continuity(record.entry_context, exitContext)) {
        return CaptureBuildResult::activation_changed;
    }
    if (instance == nullptr
        || reinterpret_cast<std::uintptr_t>(instance) != record.instance_identity
        || !valid_reconcile_observation(observation)) {
        return CaptureBuildResult::invalid_observation;
    }
    if (!copy_instance_body(instance, kAuthorityCacheOffset, record.authority_after)
        || !copy_instance_body(instance, kSenseCacheOffset, record.sense_after)) {
        return CaptureBuildResult::unreadable;
    }
    record.exit_context = exitContext;
    record.observation = observation;
    output = record;
    return CaptureBuildResult::complete;
}

CaptureBuildResult prepare_sense_capture(PendingSenseCapture& pending,
                                         const ActivationContext& entryContext,
                                         const HoldDefinitionIdentity& identity,
                                         CaptureMetadata metadata,
                                         std::uint64_t reportSequence,
                                         std::uint64_t hostReceiptSequence,
                                         const void* instance) noexcept {
    pending.record_ = {};
    pending.ready_ = false;
    if (instance == nullptr) {
        return CaptureBuildResult::null_pointer;
    }
    const CaptureBuildResult admission = validate_admission(entryContext, identity);
    if (admission != CaptureBuildResult::complete) {
        return admission;
    }
    if (!valid_metadata(metadata) || reportSequence == 0U) {
        return CaptureBuildResult::invalid_observation;
    }
    std::uint32_t liveDefinition{};
    if (!safe_copy_exact(&liveDefinition, instance, sizeof liveDefinition)) {
        return CaptureBuildResult::unreadable;
    }
    if (liveDefinition != identity.definition) {
        return CaptureBuildResult::wrong_definition;
    }
    SenseCaptureRecord record{};
    record.entry_context = entryContext;
    record.identity = identity;
    record.metadata = metadata;
    record.report_sequence = reportSequence;
    record.host_receipt_sequence = hostReceiptSequence;
    record.instance_identity = reinterpret_cast<std::uintptr_t>(instance);
    pending.record_ = record;
    pending.ready_ = true;
    return CaptureBuildResult::ready;
}

CaptureBuildResult finish_sense_capture(PendingSenseCapture& pending,
                                        const ActivationContext& exitContext,
                                        const void* instance,
                                        const PacketReference* producedPacket,
                                        const SenseWireSnapshot& owningWire,
                                        SenseCaptureRecord& output) noexcept {
    output = {};
    if (!pending.ready_) {
        return CaptureBuildResult::consumed;
    }
    SenseCaptureRecord record = pending.record_;
    pending.record_ = {};
    pending.ready_ = false;
    if (!exact_activation_continuity(record.entry_context, exitContext)) {
        return CaptureBuildResult::activation_changed;
    }
    if (instance == nullptr || producedPacket == nullptr
        || reinterpret_cast<std::uintptr_t>(instance) != record.instance_identity) {
        return CaptureBuildResult::invalid_observation;
    }
    if (!valid_sense_wire(owningWire)) {
        return CaptureBuildResult::invalid_wire;
    }
    PacketReference packetCopy{};
    if (!safe_copy_exact(&packetCopy, producedPacket, sizeof packetCopy)) {
        return CaptureBuildResult::unreadable;
    }
    if (packetCopy.key != kHoldSenseSchema) {
        return CaptureBuildResult::wrong_schema;
    }
    if (packetCopy.data == nullptr
        || !safe_copy_exact(record.sense.data(), packetCopy.data, record.sense.size())
        || !copy_instance_body(instance, kSenseCacheOffset, record.sense_cache)) {
        return CaptureBuildResult::unreadable;
    }
    record.packet_points_to_sense_cache =
        packetCopy.data == offset_address(instance, kSenseCacheOffset)
        && record.sense == record.sense_cache;
    if (!record.packet_points_to_sense_cache) {
        return CaptureBuildResult::invalid_observation;
    }
    record.exit_context = exitContext;
    record.packet_schema = packetCopy.key;
    record.outbound_wire = owningWire;
    output = record;
    return CaptureBuildResult::complete;
}

bool valid_raw_record(const ApplyCaptureRecord& record) noexcept {
    AuthorityFields incoming{};
    AuthorityFields before{};
    AuthorityFields after{};
    return exact_activation_continuity(record.entry_context, record.exit_context)
           && admitted_hold_definition(record.identity) && valid_metadata(record.metadata)
           && record.authority_publication_sequence != 0U && record.instance_identity != 0U
           && record.packet_schema == kHoldAuthoritySchema && valid_authority_wire(record.inbound_wire)
           && authority_fields(record.incoming_decoded_after_resolver, incoming)
           && authority_fields(record.cache_before, before)
           && authority_fields(record.cache_after, after)
           && record.incoming_decoded_after_resolver == record.cache_after
           && record.equal_before == (record.cache_before == record.cache_after)
           && record.dirty_before <= 1U && record.dirty_after <= 1U;
}

bool valid_raw_record(const ReconcileCaptureRecord& record) noexcept {
    AuthorityFields before{};
    AuthorityFields after{};
    SenseFields senseBefore{};
    SenseFields senseAfter{};
    return exact_activation_continuity(record.entry_context, record.exit_context)
           && admitted_hold_definition(record.identity) && valid_metadata(record.metadata)
           && record.instance_identity != 0U && authority_fields(record.authority_before, before)
           && authority_fields(record.authority_after, after)
           && sense_fields(record.sense_before, senseBefore)
           && sense_fields(record.sense_after, senseAfter)
           && valid_reconcile_observation(record.observation);
}

bool valid_raw_record(const SenseCaptureRecord& record) noexcept {
    SenseFields fields{};
    return exact_activation_continuity(record.entry_context, record.exit_context)
           && admitted_hold_definition(record.identity) && valid_metadata(record.metadata)
           && record.report_sequence != 0U && record.instance_identity != 0U
           && record.packet_schema == kHoldSenseSchema && valid_sense_wire(record.outbound_wire)
           && record.packet_points_to_sense_cache && record.sense == record.sense_cache
           && sense_fields(record.sense, fields);
}

bool default_telemetry(const ApplyCaptureRecord& record,
                       const TelemetryProjectionContext& projection,
                       const ApplyTelemetryDigests& digests,
                       ApplyTelemetry& output) noexcept {
    output = {};
    if (!valid_raw_record(record) || !valid_projection(projection)
        || !digest_present(digests.incoming) || !digest_present(digests.cache_before)
        || !digest_present(digests.cache_after) || !digest_present(digests.inbound_wire)) {
        return false;
    }
    output = ApplyTelemetry{make_provenance(record.entry_context, record.identity, projection),
                            make_event(record.metadata, projection),
                            digests,
                            record.authority_publication_sequence,
                            record.inbound_wire.bit_count,
                            record.dirty_before,
                            record.dirty_after,
                            record.equal_before};
    return true;
}

bool default_telemetry(const ReconcileCaptureRecord& record,
                       const TelemetryProjectionContext& projection,
                       const ReconcileTelemetryDigests& digests,
                       std::uint32_t verifiedSubscriberTargetRva,
                       bool targetNormalized,
                       ReconcileTelemetry& output) noexcept {
    output = {};
    const bool allDigests = digest_present(digests.authority_before)
                            && digest_present(digests.authority_after)
                            && digest_present(digests.sense_before)
                            && digest_present(digests.sense_after)
                            && digest_present(digests.live_objects);
    const bool subscriberIdentitySafe =
        !record.observation.subscriber.invoked
        || (targetNormalized && verifiedSubscriberTargetRva != 0U);
    if (!valid_raw_record(record) || !valid_projection(projection) || !allDigests
        || !subscriberIdentitySafe) {
        return false;
    }
    output = ReconcileTelemetry{
        make_provenance(record.entry_context, record.identity, projection),
        make_event(record.metadata, projection),
        digests,
        record.observation.subscriber.invoked ? verifiedSubscriberTargetRva : 0U,
        record.observation.live_object_count,
        record.observation.branches,
        record.observation.subscriber.invoked,
        record.observation.subscriber.invoked && targetNormalized};
    return true;
}

bool default_telemetry(const SenseCaptureRecord& record,
                       const TelemetryProjectionContext& projection,
                       const SenseTelemetryDigests& digests,
                       SenseTelemetry& output) noexcept {
    output = {};
    SenseFields fields{};
    if (!valid_raw_record(record) || !valid_projection(projection)
        || !digest_present(digests.sense) || !digest_present(digests.outbound_wire)
        || !sense_fields(record.sense, fields)) {
        return false;
    }
    output = SenseTelemetry{make_provenance(record.entry_context, record.identity, projection),
                            make_event(record.metadata, projection),
                            digests,
                            record.report_sequence,
                            record.host_receipt_sequence,
                            record.outbound_wire.bit_count,
                            fields.linked_content_item_present};
    return true;
}

FullCallGate::Scope::Scope(FullCallGate& gate) noexcept : gate_(gate) {
    gate_.active_calls_.fetch_add(1U, std::memory_order_acq_rel);
    const std::uint64_t first = gate_.epoch_.load(std::memory_order_acquire);
    const bool accepting = gate_.accepting_.load(std::memory_order_acquire);
    const std::uint64_t second = gate_.epoch_.load(std::memory_order_acquire);
    entry_epoch_ = first;
    entry_eligible_ = first == second && accepting && first != 0U
                      && !gate_.epoch_exhausted_.load(std::memory_order_acquire);
}

FullCallGate::Scope::~Scope() noexcept {
    gate_.active_calls_.fetch_sub(1U, std::memory_order_release);
}
bool FullCallGate::Scope::entry_observation_eligible() const noexcept {
    return entry_eligible_;
}
bool FullCallGate::Scope::post_observation_eligible() const noexcept {
    return entry_eligible_ && gate_.accepting_.load(std::memory_order_acquire)
           && gate_.epoch_.load(std::memory_order_acquire) == entry_epoch_
           && !gate_.epoch_exhausted_.load(std::memory_order_acquire);
}
bool FullCallGate::advance_epoch() noexcept {
    std::uint64_t current = epoch_.load(std::memory_order_acquire);
    for (;;) {
        if (current == (std::numeric_limits<std::uint64_t>::max)()) {
            accepting_.store(false, std::memory_order_release);
            epoch_exhausted_.store(true, std::memory_order_release);
            return false;
        }
        if (epoch_.compare_exchange_weak(current,
                                         current + 1U,
                                         std::memory_order_acq_rel,
                                         std::memory_order_acquire)) {
            return true;
        }
    }
}
bool FullCallGate::begin_activation() noexcept {
    accepting_.store(false, std::memory_order_release);
    if (!advance_epoch()) {
        return false;
    }
    accepting_.store(true, std::memory_order_release);
    return true;
}
void FullCallGate::quiesce() noexcept {
    accepting_.store(false, std::memory_order_release);
    (void)advance_epoch();
}
bool FullCallGate::accepting() const noexcept {
    return accepting_.load(std::memory_order_acquire);
}
bool FullCallGate::idle() const noexcept {
    return active_calls_.load(std::memory_order_acquire) == 0U;
}
std::uint32_t FullCallGate::active_calls() const noexcept {
    return active_calls_.load(std::memory_order_acquire);
}
std::uint64_t FullCallGate::epoch() const noexcept {
    return epoch_.load(std::memory_order_acquire);
}
bool FullCallGate::epoch_exhausted() const noexcept {
    return epoch_exhausted_.load(std::memory_order_acquire);
}
#if defined(SUNRISE_TYPE26_HOLD_CAPTURE_TEST)
void FullCallGate::testing_set_epoch(std::uint64_t epoch) noexcept {
    accepting_.store(false, std::memory_order_release);
    epoch_.store(epoch, std::memory_order_release);
    epoch_exhausted_.store(false, std::memory_order_release);
}
#endif

bool HookGroupState::begin_install(std::uint64_t installGeneration) noexcept {
    if (phase_ != HookGroupPhase::detached || installGeneration == 0U) {
        return false;
    }
    install_generation_ = installGeneration;
    phase_ = HookGroupPhase::installing;
    return true;
}
bool HookGroupState::complete_install(
    std::span<const std::uintptr_t, kNativeSurfaceCount> originals) noexcept {
    if (phase_ != HookGroupPhase::installing
        || std::any_of(originals.begin(), originals.end(), [](std::uintptr_t value) {
               return value == 0U;
           })) {
        return false;
    }
    std::copy(originals.begin(), originals.end(), originals_.begin());
    phase_ = HookGroupPhase::running;
    return true;
}
bool HookGroupState::rollback_install() noexcept {
    if (phase_ != HookGroupPhase::installing) {
        return false;
    }
    originals_ = {};
    install_generation_ = 0U;
    phase_ = HookGroupPhase::detached;
    return true;
}
bool HookGroupState::retain_install_failure(
    std::span<const std::uintptr_t, kNativeSurfaceCount> originals) noexcept {
    if (phase_ != HookGroupPhase::installing
        || std::any_of(originals.begin(), originals.end(), [](std::uintptr_t value) {
               return value == 0U;
           })) {
        return false;
    }
    std::copy(originals.begin(), originals.end(), originals_.begin());
    phase_ = HookGroupPhase::quiescing;
    return true;
}
bool HookGroupState::quiesce() noexcept {
    if (phase_ == HookGroupPhase::quiescing) {
        return true;
    }
    if (phase_ != HookGroupPhase::running) {
        return false;
    }
    phase_ = HookGroupPhase::quiescing;
    return true;
}
ProtectedDetachResult HookGroupState::record_protected_detach(
    std::span<FullCallGate* const, kNativeSurfaceCount> gates,
    ProtectedDetachDisposition disposition) noexcept {
    if (phase_ != HookGroupPhase::quiescing) {
        return ProtectedDetachResult::wrong_phase;
    }
    if (std::any_of(gates.begin(), gates.end(), [](const FullCallGate* gate) {
            return gate == nullptr || !gate->idle();
        })) {
        return ProtectedDetachResult::protected_code_active;
    }
    if (disposition == ProtectedDetachDisposition::deferred) {
        return ProtectedDetachResult::adapter_deferred;
    }
    if (disposition == ProtectedDetachDisposition::failed) {
        return ProtectedDetachResult::adapter_failed;
    }
    originals_ = {};
    install_generation_ = 0U;
    phase_ = HookGroupPhase::detached;
    return ProtectedDetachResult::removed;
}
HookGroupSnapshot HookGroupState::snapshot() const noexcept {
    return HookGroupSnapshot{originals_, install_generation_, phase_};
}

ActiveBodyClaimResult validate_active_body_claim(const ApplyCaptureRecord& record,
                                                 const ActiveBodyClaim& claim) noexcept {
    const bool accepted = valid_raw_record(record)
                          && record.inbound_wire.complete_without_underflow
                          && record.inbound_wire.complete_without_trailing_bits
                          && record.inbound_wire.dynamic_type34_complete
                          && claim.externally_proven_decode_encode_round_trip_bit_exact
                          && claim.differs_from_neutral_candidate_in_consumed_field
                          && claim.matching_reconcile_branch && claim.later_matching_sense
                          && claim.repeated_without_crash && claim.no_extra_packet
                          && claim.no_extra_runtime_object;
    return accepted ? ActiveBodyClaimResult::accepted_observation_only
                    : ActiveBodyClaimResult::rejected;
}
bool proven_explicit_native_or_vm_read_edge(
    const ExplicitSceneReadEdgeEvidence& evidence) noexcept {
    const bool validProvenance =
        evidence.provenance == ReadEdgeProvenance::native_instruction
        || evidence.provenance == ReadEdgeProvenance::mission_vm_instruction;
    const bool validSource = evidence.source == HoldReadSource::authority_state
                             || evidence.source
                                    == HoldReadSource::acknowledged_sense_generation;
    return exact_current_activation(evidence.context)
           && admitted_hold_definition(evidence.identity)
           && evidence.hold_publication_generation != 0U
           && evidence.scene_publication_generation != 0U
           && evidence.native_or_vm_instruction_identity != 0U && validProvenance
           && validSource && evidence.consumes_hold_state_or_generation
           && evidence.targets_scene_publication_decision
           && evidence.suppresses_scene_until_satisfied;
}
HoldRoleClassification validate_role_classification_claim(
    const RoleClassificationClaim& claim) noexcept {
    if (!exact_current_activation(claim.context) || !admitted_hold_definition(claim.identity)) {
        return HoldRoleClassification::inconclusive;
    }
    const bool trialsComplete =
        (claim.weapon_down_completed_trials & kAllRequiredTrials) == kAllRequiredTrials
        && (claim.no_combat_abilities_completed_trials & kAllRequiredTrials)
               == kAllRequiredTrials;
    if (!claim.schema_complete_non_neutral_authority_apply) {
        const bool contradictory = claim.materialize_or_subscriber_transition
                                   || claim.matching_sense_host_acknowledgement
                                   || claim.correlated_player_facing_restriction
                                   || proven_explicit_native_or_vm_read_edge(
                                       claim.scene_read_edge);
        return trialsComplete && claim.scene_proceeded && !contradictory
                   ? HoldRoleClassification::unused_in_captured_opening
                   : HoldRoleClassification::inconclusive;
    }
    const bool effectAndSense = claim.materialize_or_subscriber_transition
                                && claim.matching_sense_host_acknowledgement;
    const bool coherentPublications = claim.scene_proceeded
                                      && claim.hold_publication_generation != 0U
                                      && claim.scene_publication_generation != 0U;
    const bool ordered = coherentPublications && effectAndSense
                         && claim.repeated_hold_before_scene_same_publication_generation
                         && claim.host_publication_dag_explicitly_orders_hold_before_scene;
    const bool edgeMatches = claim.scene_read_edge.context == claim.context
                             && claim.scene_read_edge.identity == claim.identity
                             && claim.scene_read_edge.hold_publication_generation
                                    == claim.hold_publication_generation
                             && claim.scene_read_edge.scene_publication_generation
                                    == claim.scene_publication_generation;
    if (ordered && edgeMatches
        && proven_explicit_native_or_vm_read_edge(claim.scene_read_edge)) {
        return HoldRoleClassification::strict_scene_gate;
    }
    if (ordered) {
        return HoldRoleClassification::ordered_presentation_predecessor;
    }
    if (effectAndSense && claim.correlated_player_facing_restriction
        && !proven_explicit_native_or_vm_read_edge(claim.scene_read_edge)) {
        return HoldRoleClassification::parallel_restriction_lane;
    }
    return HoldRoleClassification::inconclusive;
}

} // namespace sunrise::client::hooks::bootflow::opening_authority::type26_hold
