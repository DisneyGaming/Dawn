#include "client/hooks/bootflow/opening_authority/type31_incident_listener_capture.h"

#include <algorithm>
#include <cstring>
#include <vector>

#if defined(_WIN32)
#include <Windows.h>

#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#endif

namespace dawn::client::hooks::bootflow::opening_authority::type31_incident {
namespace {

[[nodiscard]] consteval std::uint8_t hex_nibble(char value) {
    if (value >= '0' && value <= '9') {
        return static_cast<std::uint8_t>(value - '0');
    }
    if (value >= 'A' && value <= 'F') {
        return static_cast<std::uint8_t>(value - 'A' + 10);
    }
    return 0U;
}

template <std::size_t Characters>
[[nodiscard]] consteval auto hex_bytes(const char (&text)[Characters]) {
    static_assert(Characters > 1U);
    static_assert(((Characters - 1U) % 2U) == 0U);
    std::array<std::byte, (Characters - 1U) / 2U> output{};
    for (std::size_t index = 0U; index < output.size(); ++index) {
        output[index] = std::byte{static_cast<std::uint8_t>((hex_nibble(text[index * 2U]) << 4U)
                                                            | hex_nibble(text[index * 2U + 1U]))};
    }
    return output;
}

inline constexpr auto kCreateInitPrefix =
    hex_bytes("48895C2410574883EC20488BF94881C188010000E8B7F1FFFF488D4C2430E88D");
inline constexpr auto kAuthorityApplyPrefix =
    hex_bytes("40534883EC30448B02488BD94C8B4A08488D4C2420488B05FC2248018B104489");
inline constexpr auto kLocalEvaluatePrefix =
    hex_bytes("405557488BEC4883EC7880B98801000000488BF90F8425030000488B89900100");
inline constexpr auto kPointTerminalPrefix =
    hex_bytes("4889742420574881ECE0000000488B05549258014833C448898424D000000048");
inline constexpr auto kSObjectValidateDispatchPrefix =
    hex_bytes("48895C24104889742418574883EC308B01498BF08BDA488BF93DC59D1C817458");
inline constexpr auto kRecursiveMaterializeSubmitPrefix =
    hex_bytes("B8C8160000E8762A3D01482BE0488B05C4F9BF014833C448898424B01600004C");
inline constexpr auto kConsumedTokenMaterializePrefix =
    hex_bytes("48895C2408574883EC20488BD9488BFA488BCAE888730A00488BCBE8307A0000");
inline constexpr auto kManagerSubmitVslotPrefix =
    hex_bytes("40534883EC20488BDAE882B874FF488D0DF3E7E900488BD348894C2440488D4C");
inline constexpr auto kRecursiveVisitorPrefix =
    hex_bytes("405741564883EC284C8BF148895C2450488D4C24404889742420488BFAE8AEAF");
inline constexpr auto kIncidentVisitorCallbackPrefix =
    hex_bytes("48895C2418574881EC60050000488B05447332014833C4488984245005000048");
inline constexpr auto kIncidentNormalRoutePrefix =
    hex_bytes("48895C2418574883EC50488B05976F32014833C44889442440488BDA488BF9BA");
inline constexpr auto kDynamicListenerEnumerationPrefix =
    hex_bytes("4055535741554156488DAC2450FCFFFF4881ECB0040000488B050A6F32014833");

inline constexpr std::size_t kMaximumPrefixBytes = 32U;
inline constexpr OwnerRequirement kTerminalOwners =
    OwnerRequirement::activation | OwnerRequirement::activity | OwnerRequirement::component
    | OwnerRequirement::authority | OwnerRequirement::runtime_volume | OwnerRequirement::membership;
inline constexpr OwnerRequirement kManagerOwners =
    kTerminalOwners | OwnerRequirement::incident_manager;
inline constexpr OwnerRequirement kListenerOwners =
    kManagerOwners | OwnerRequirement::listener_table;

[[nodiscard]] int seh_copy_filter(unsigned int code) noexcept {
#if defined(_WIN32)
    return code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_IN_PAGE_ERROR
               ? EXCEPTION_EXECUTE_HANDLER
               : EXCEPTION_CONTINUE_SEARCH;
#else
    (void)code;
    return 0;
#endif
}

[[nodiscard]] bool
safe_copy_exact(void* destination, const void* source, std::size_t bytes) noexcept {
    if (destination == nullptr || source == nullptr || bytes == 0U) {
        return false;
    }
#if defined(_WIN32) && defined(_MSC_VER)
    __try {
        std::memcpy(destination, source, bytes);
        return true;
    } __except (seh_copy_filter(GetExceptionCode())) {
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

[[nodiscard]] bool valid_metadata(const CaptureMetadata& metadata) noexcept {
    return metadata.monotonic_tick != 0U && metadata.producer_thread_id != 0U
           && metadata.normalized_caller_rva != 0U;
}

[[nodiscard]] bool token_present(const OwnerToken& token) noexcept {
    return valid_presence(token) && token.presence == Presence::present;
}

[[nodiscard]] const OwnerToken& select_token(const OwnershipSnapshot& snapshot,
                                             OwnerRequirement requirement) noexcept {
    switch (requirement) {
    case OwnerRequirement::activation:
        return snapshot.activation;
    case OwnerRequirement::activity:
        return snapshot.activity;
    case OwnerRequirement::component:
        return snapshot.component;
    case OwnerRequirement::authority:
        return snapshot.authority;
    case OwnerRequirement::runtime_volume:
        return snapshot.runtime_volume;
    case OwnerRequirement::membership:
        return snapshot.membership;
    case OwnerRequirement::incident_manager:
        return snapshot.incident_manager;
    case OwnerRequirement::listener_table:
        return snapshot.listener_table;
    case OwnerRequirement::none:
        break;
    }
    return snapshot.activation;
}

[[nodiscard]] bool snapshots_share_trace(const OwnershipSnapshot& left,
                                         const OwnershipSnapshot& right) noexcept {
    return left.cohort == right.cohort && left.capture_epoch == right.capture_epoch
           && left.activation == right.activation && left.activity == right.activity
           && left.component == right.component && left.authority == right.authority
           && left.runtime_volume == right.runtime_volume && left.membership == right.membership
           && left.local_player_datum == right.local_player_datum
           && left.local_player_bit_index == right.local_player_bit_index;
}

[[nodiscard]] AuthorityState
decode_authority(const std::array<std::byte, kAuthorityWindowBytes>& window) noexcept {
    AuthorityState state{};
    std::memcpy(&state.consumed_generation, window.data(), sizeof state.consumed_generation);
    std::memcpy(&state.active, window.data() + 0x08U, sizeof state.active);
    std::memcpy(&state.pending_generation, window.data() + 0x10U, sizeof state.pending_generation);
    std::memcpy(&state.companion, window.data() + 0x18U, sizeof state.companion);
    return state;
}

[[nodiscard]] bool terminal_predicate(const RestrictedIncidentRecord& record) noexcept {
    const AuthorityState& state = record.authority_before;
    const bool newer = state.pending_generation != kUnsetGeneration
                       && (state.consumed_generation == kUnsetGeneration
                           || state.consumed_generation < state.pending_generation);
    const std::uint8_t bit = record.terminal_entry_fence.before.local_player_bit_index;
    const bool member = bit < 64U && (record.membership_mask_before & (1ULL << bit)) != 0U;
    return record.terminal_entry_fence.exact && state.active == 1U && newer && member
           && record.matched_reference == record.point.volume
           && record.terminal_entry_fence.before.component.identity == record.instance_identity
           && record.terminal_entry_fence.before.runtime_volume.identity
                  == record.runtime_volume_identity;
}

template <std::size_t Capacity>
[[nodiscard]] bool copy_opaque(const OpaqueWindowInput& input,
                               BoundedOpaqueEvidence<Capacity>& output) noexcept {
    output = {};
    if (!valid_presence(input.observed_schema_or_subtype)) {
        return false;
    }
    if (input.data == nullptr || input.requested_bytes == 0U) {
        return input.data == nullptr && input.requested_bytes == 0U
               && input.observed_schema_or_subtype.presence == Presence::absent;
    }
    if (input.requested_bytes > (std::numeric_limits<std::uint32_t>::max)()) {
        return false;
    }
    output.presence = Presence::present;
    output.requested_bytes = static_cast<std::uint32_t>(input.requested_bytes);
    output.captured_bytes = static_cast<std::uint16_t>((std::min)(input.requested_bytes, Capacity));
    output.truncated = input.requested_bytes > Capacity;
    output.observed_schema_or_subtype = input.observed_schema_or_subtype;
    output.readable = safe_copy_exact(output.bytes.data(), input.data, output.captured_bytes);
    return output.readable;
}

template <typename Copy>
[[nodiscard]] bool fenced_capture(const OwnershipSnapshotProvider& provider,
                                  OwnerRequirement requirements,
                                  PhaseFence& fence,
                                  Copy&& copy) noexcept {
    fence = {};
    if (provider.read == nullptr) {
        return false;
    }
    fence.before_valid = provider.read(provider.context, fence.before)
                         && valid_ownership_snapshot(fence.before, requirements);
    const bool copied = fence.before_valid && std::invoke(std::forward<Copy>(copy));
    fence.after_valid = provider.read(provider.context, fence.after)
                        && valid_ownership_snapshot(fence.after, requirements);
    fence.exact = fence.before_valid && fence.after_valid
                  && exact_same_owners(fence.before, fence.after, requirements);
    return copied;
}

template <std::size_t Capacity>
[[nodiscard]] bool
bounded_evidence_consistent(const BoundedOpaqueEvidence<Capacity>& evidence) noexcept {
    if (evidence.presence == Presence::absent) {
        return evidence.requested_bytes == 0U && evidence.captured_bytes == 0U && !evidence.readable
               && !evidence.truncated
               && evidence.observed_schema_or_subtype.presence == Presence::absent;
    }
    return evidence.requested_bytes != 0U && evidence.captured_bytes != 0U
           && evidence.captured_bytes <= evidence.bytes.size() && evidence.readable
           && valid_presence(evidence.observed_schema_or_subtype)
           && evidence.truncated == (evidence.requested_bytes > evidence.bytes.size());
}

#if defined(_WIN32)
[[nodiscard]] bool hash_stream(HANDLE file, Sha256& output) noexcept {
    BCRYPT_ALG_HANDLE algorithm{};
    BCRYPT_HASH_HANDLE hash{};
    DWORD objectBytes{};
    DWORD returned{};
    bool success =
        BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0U) >= 0
        && BCryptGetProperty(algorithm,
                             BCRYPT_OBJECT_LENGTH,
                             reinterpret_cast<PUCHAR>(&objectBytes),
                             sizeof objectBytes,
                             &returned,
                             0U)
               >= 0;
    std::vector<std::byte> object{};
    if (success) {
        object.resize(objectBytes);
        success = BCryptCreateHash(algorithm,
                                   &hash,
                                   reinterpret_cast<PUCHAR>(object.data()),
                                   objectBytes,
                                   nullptr,
                                   0U,
                                   0U)
                  >= 0;
    }
    std::array<std::byte, 64U * 1024U> buffer{};
    while (success) {
        DWORD read{};
        if (ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr)
            == FALSE) {
            success = false;
            break;
        }
        if (read == 0U) {
            break;
        }
        success = BCryptHashData(hash, reinterpret_cast<PUCHAR>(buffer.data()), read, 0U) >= 0;
    }
    if (success) {
        success = BCryptFinishHash(hash,
                                   reinterpret_cast<PUCHAR>(output.data()),
                                   static_cast<ULONG>(output.size()),
                                   0U)
                  >= 0;
    }
    if (hash != nullptr) {
        (void)BCryptDestroyHash(hash);
    }
    if (algorithm != nullptr) {
        (void)BCryptCloseAlgorithmProvider(algorithm, 0U);
    }
    if (!success) {
        output = {};
    }
    return success;
}

[[nodiscard]] FileMeasurementResult measure_file(const wchar_t* path,
                                                 PackedRuntimeIdentity& output) noexcept {
    output = {};
    const HANDLE file = CreateFileW(path,
                                    GENERIC_READ,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                    nullptr,
                                    OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                                    nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return FileMeasurementResult::open_failed;
    }
    LARGE_INTEGER size{};
    if (GetFileSizeEx(file, &size) == FALSE || size.QuadPart < 0) {
        CloseHandle(file);
        return FileMeasurementResult::size_failed;
    }
    output.file_bytes = static_cast<std::uint64_t>(size.QuadPart);
    const bool hashed = hash_stream(file, output.sha256);
    CloseHandle(file);
    return hashed ? FileMeasurementResult::measured : FileMeasurementResult::hash_failed;
}

[[nodiscard]] bool executable_page(const void* address, std::size_t bytes) noexcept {
    const auto start = reinterpret_cast<std::uintptr_t>(address);
    if (start == 0U || bytes == 0U
        || start > (std::numeric_limits<std::uintptr_t>::max)() - bytes) {
        return false;
    }
    std::uintptr_t cursor = start;
    const std::uintptr_t end = start + bytes;
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION info{};
        if (VirtualQuery(reinterpret_cast<const void*>(cursor), &info, sizeof info) != sizeof info
            || info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0U) {
            return false;
        }
        const DWORD protection = info.Protect & 0xFFU;
        const bool executable = protection == PAGE_EXECUTE || protection == PAGE_EXECUTE_READ
                                || protection == PAGE_EXECUTE_READWRITE
                                || protection == PAGE_EXECUTE_WRITECOPY;
        if (!executable) {
            return false;
        }
        const std::uintptr_t regionEnd =
            reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
        if (regionEnd <= cursor) {
            return false;
        }
        cursor = regionEnd;
    }
    return true;
}
#endif

[[nodiscard]] std::uint16_t surface_bit(NativeSurface surface) noexcept {
    const auto index = static_cast<std::uint8_t>(surface);
    return index < static_cast<std::uint8_t>(NativeSurface::count)
               ? static_cast<std::uint16_t>(1U << index)
               : 0U;
}

} // namespace

RuntimeCohortToken make_runtime_cohort_token(std::uint64_t cohortId,
                                             std::uint64_t generation,
                                             const Sha256& digest) noexcept {
    return RuntimeCohortToken{cohortId, generation, kPinnedPeSizeOfImage, digest};
}

NativeBoundaryDescriptor native_boundary(NativeSurface surface) noexcept {
    switch (surface) {
    case NativeSurface::create_init:
        return {0xB202F0U, 0x45U, kCreateInitPrefix, NativeAbi::instance_create_u8};
    case NativeSurface::authority_apply:
        return {0xB20640U, 0x4AU, kAuthorityApplyPrefix, NativeAbi::instance_packet};
    case NativeSurface::local_evaluate:
        return {0xB20B00U, 0x346U, kLocalEvaluatePrefix, NativeAbi::instance_only};
    case NativeSurface::point_terminal:
        return {
            0xB20820U, 0x1B5U, kPointTerminalPrefix, NativeAbi::instance_matched_object_reference};
    case NativeSurface::sobject_validate_dispatch:
        return {0x4CFA00U,
                0x88U,
                kSObjectValidateDispatchPrefix,
                NativeAbi::sobject_id_descriptor_root};
    case NativeSurface::recursive_materialize_submit:
        return {
            0x4AA0B0U, 0U, kRecursiveMaterializeSubmitPrefix, NativeAbi::internal_recursive_submit};
    case NativeSurface::consumed_token_materialize:
        return {
            0x4E5200U, 0U, kConsumedTokenMaterializePrefix, NativeAbi::internal_token_materializer};
    case NativeSurface::manager_submit_vslot:
        return {0xD83660U, 0U, kManagerSubmitVslotPrefix, NativeAbi::manager_incident_vslot};
    case NativeSurface::recursive_visitor:
        return {0x4AA1C0U, 0U, kRecursiveVisitorPrefix, NativeAbi::internal_reflected_object_walk};
    case NativeSurface::incident_visitor_callback:
        return {
            0xD82730U, 0U, kIncidentVisitorCallbackPrefix, NativeAbi::internal_incident_visitor};
    case NativeSurface::incident_normal_route:
        return {
            0xD82AE0U, 0U, kIncidentNormalRoutePrefix, NativeAbi::internal_incident_normal_route};
    case NativeSurface::dynamic_listener_enumeration:
        return {0xD82B60U,
                0U,
                kDynamicListenerEnumerationPrefix,
                NativeAbi::internal_dynamic_listener_enumerator};
    case NativeSurface::count:
        break;
    }
    return {};
}

bool native_prefix_matches(NativeSurface surface, std::span<const std::byte> observed) noexcept {
    const NativeBoundaryDescriptor boundary = native_boundary(surface);
    return boundary.mapped_rva != 0U && !boundary.mapped_prefix.empty()
           && observed.size() >= boundary.mapped_prefix.size()
           && std::equal(
               boundary.mapped_prefix.begin(), boundary.mapped_prefix.end(), observed.begin());
}

PrefixCohortResult validate_mapped_prefixes_untrusted(
    std::span<const std::byte> image,
    std::array<std::uintptr_t, kNativeSurfaceCount>& outputAddresses) noexcept {
    outputAddresses = {};
    if (image.empty()) {
        return PrefixCohortResult::invalid_arguments;
    }
    std::array<std::uintptr_t, kNativeSurfaceCount> candidates{};
    for (std::size_t index = 0U; index < kNativeSurfaceCount; ++index) {
        const NativeSurface surface = static_cast<NativeSurface>(index);
        const NativeBoundaryDescriptor boundary = native_boundary(surface);
        if (boundary.mapped_rva == 0U || boundary.mapped_prefix.empty()
            || boundary.mapped_prefix.size() > kMaximumPrefixBytes) {
            return PrefixCohortResult::invalid_arguments;
        }
        if (boundary.mapped_rva > image.size()
            || boundary.mapped_prefix.size() > image.size() - boundary.mapped_rva) {
            return PrefixCohortResult::target_out_of_range;
        }
        std::array<std::byte, kMaximumPrefixBytes> observed{};
        if (!safe_copy_exact(
                observed.data(), image.data() + boundary.mapped_rva, boundary.mapped_prefix.size())
            || !native_prefix_matches(surface, {observed.data(), boundary.mapped_prefix.size()})) {
            return PrefixCohortResult::prefix_mismatch;
        }
        candidates[index] = reinterpret_cast<std::uintptr_t>(image.data()) + boundary.mapped_rva;
    }
    outputAddresses = candidates;
    return PrefixCohortResult::valid;
}

FileMeasurementResult measure_pinned_packed_runtime(PackedRuntimeIdentity& output) noexcept {
#if defined(_WIN32)
    return measure_file(kPinnedPackedRuntimePath, output);
#else
    output = {};
    return FileMeasurementResult::open_failed;
#endif
}

bool sha256_bytes(std::span<const std::byte> bytes, Sha256& output) noexcept {
    output = {};
#if defined(_WIN32)
    BCRYPT_ALG_HANDLE algorithm{};
    BCRYPT_HASH_HANDLE hash{};
    DWORD objectBytes{};
    DWORD returned{};
    bool success =
        BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0U) >= 0
        && BCryptGetProperty(algorithm,
                             BCRYPT_OBJECT_LENGTH,
                             reinterpret_cast<PUCHAR>(&objectBytes),
                             sizeof objectBytes,
                             &returned,
                             0U)
               >= 0;
    std::vector<std::byte> object{};
    if (success) {
        object.resize(objectBytes);
        success = BCryptCreateHash(algorithm,
                                   &hash,
                                   reinterpret_cast<PUCHAR>(object.data()),
                                   objectBytes,
                                   nullptr,
                                   0U,
                                   0U)
                  >= 0;
    }
    if (success && !bytes.empty()) {
        success = bytes.size() <= (std::numeric_limits<ULONG>::max)()
                  && BCryptHashData(hash,
                                    reinterpret_cast<PUCHAR>(const_cast<std::byte*>(bytes.data())),
                                    static_cast<ULONG>(bytes.size()),
                                    0U)
                         >= 0;
    }
    if (success) {
        success = BCryptFinishHash(hash,
                                   reinterpret_cast<PUCHAR>(output.data()),
                                   static_cast<ULONG>(output.size()),
                                   0U)
                  >= 0;
    }
    if (hash != nullptr) {
        (void)BCryptDestroyHash(hash);
    }
    if (algorithm != nullptr) {
        (void)BCryptCloseAlgorithmProvider(algorithm, 0U);
    }
    if (!success) {
        output = {};
    }
    return success;
#else
    (void)bytes;
    return false;
#endif
}

LiveAdmissionResult admit_live_runtime(ValidatedRuntimeCohort& output) noexcept {
    output = {};
#if defined(_WIN32) && defined(_MSC_VER)
    const HMODULE module = GetModuleHandleW(nullptr);
    if (module == nullptr) {
        return LiveAdmissionResult::main_module_unavailable;
    }
    std::array<wchar_t, 32768U> path{};
    const DWORD pathBytes =
        GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
    if (pathBytes == 0U || pathBytes == static_cast<DWORD>(path.size())) {
        return LiveAdmissionResult::module_path_unavailable;
    }
    PackedRuntimeIdentity measured{};
    if (measure_file(path.data(), measured) != FileMeasurementResult::measured
        || !matches_pinned_packed_runtime(measured)) {
        return LiveAdmissionResult::packed_file_mismatch;
    }

    const auto* const base = reinterpret_cast<const std::byte*>(module);
    IMAGE_DOS_HEADER dos{};
    if (!safe_copy_exact(&dos, base, sizeof dos) || dos.e_magic != IMAGE_DOS_SIGNATURE
        || dos.e_lfanew <= 0) {
        return LiveAdmissionResult::invalid_pe;
    }
    IMAGE_NT_HEADERS64 nt{};
    if (!safe_copy_exact(&nt, base + dos.e_lfanew, sizeof nt) || nt.Signature != IMAGE_NT_SIGNATURE
        || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        return LiveAdmissionResult::invalid_pe;
    }
    if (nt.OptionalHeader.SizeOfImage != kPinnedPeSizeOfImage) {
        return LiveAdmissionResult::wrong_size_of_image;
    }
    for (std::size_t index = 0U; index < kNativeSurfaceCount; ++index) {
        const NativeBoundaryDescriptor boundary =
            native_boundary(static_cast<NativeSurface>(index));
        if (!executable_page(base + boundary.mapped_rva, boundary.mapped_prefix.size())) {
            return LiveAdmissionResult::page_not_executable;
        }
    }
    std::array<std::uintptr_t, kNativeSurfaceCount> targets{};
    if (validate_mapped_prefixes_untrusted({base, kPinnedPeSizeOfImage}, targets)
        != PrefixCohortResult::valid) {
        return LiveAdmissionResult::prefix_mismatch;
    }

    std::array<std::byte, kNativeSurfaceCount * kMaximumPrefixBytes> cohortBytes{};
    for (std::size_t index = 0U; index < kNativeSurfaceCount; ++index) {
        const NativeBoundaryDescriptor boundary =
            native_boundary(static_cast<NativeSurface>(index));
        std::memcpy(cohortBytes.data() + index * kMaximumPrefixBytes,
                    base + boundary.mapped_rva,
                    boundary.mapped_prefix.size());
    }
    Sha256 digest{};
    if (!sha256_bytes(cohortBytes, digest)) {
        return LiveAdmissionResult::prefix_mismatch;
    }
    std::uint64_t cohortId{};
    std::memcpy(&cohortId, digest.data(), sizeof cohortId);
    if (cohortId == 0U) {
        return LiveAdmissionResult::prefix_mismatch;
    }
    static std::atomic<std::uint64_t> nextGeneration{1U};
    std::uint64_t generation = nextGeneration.load(std::memory_order_acquire);
    for (;;) {
        if (generation == (std::numeric_limits<std::uint64_t>::max)()) {
            return LiveAdmissionResult::generation_exhausted;
        }
        if (nextGeneration.compare_exchange_weak(generation,
                                                 generation + 1U,
                                                 std::memory_order_acq_rel,
                                                 std::memory_order_acquire)) {
            break;
        }
    }
    output.token_ = make_runtime_cohort_token(cohortId, generation, digest);
    output.targets_ = targets;
    output.module_base_ = reinterpret_cast<std::uintptr_t>(base);
    return LiveAdmissionResult::admitted;
#else
    return LiveAdmissionResult::main_module_unavailable;
#endif
}

#if defined(DAWN_TYPE31_INCIDENT_CAPTURE_TEST)
RuntimeCohortToken testing_runtime_cohort_token(std::uint64_t generation) noexcept {
    Sha256 digest{};
    for (std::size_t index = 0U; index < digest.size(); ++index) {
        digest[index] = std::byte{static_cast<std::uint8_t>(index + 1U)};
    }
    return RuntimeCohortToken{0x1122334455667788ULL, generation, kPinnedPeSizeOfImage, digest};
}
#endif

bool valid_ownership_snapshot(const OwnershipSnapshot& snapshot,
                              OwnerRequirement requirements) noexcept {
    if (!snapshot.cohort.valid() || snapshot.capture_epoch == 0U
        || snapshot.local_player_datum == 0U || snapshot.local_player_bit_index >= 64U) {
        return false;
    }
    constexpr std::array<OwnerRequirement, 8U> owners{OwnerRequirement::activation,
                                                      OwnerRequirement::activity,
                                                      OwnerRequirement::component,
                                                      OwnerRequirement::authority,
                                                      OwnerRequirement::runtime_volume,
                                                      OwnerRequirement::membership,
                                                      OwnerRequirement::incident_manager,
                                                      OwnerRequirement::listener_table};
    for (const OwnerRequirement owner : owners) {
        const OwnerToken& token = select_token(snapshot, owner);
        if (!valid_presence(token)
            || (owner_required(requirements, owner) && !token_present(token))) {
            return false;
        }
    }
    return true;
}

bool exact_same_owners(const OwnershipSnapshot& before,
                       const OwnershipSnapshot& after,
                       OwnerRequirement requirements) noexcept {
    return valid_ownership_snapshot(before, requirements)
           && valid_ownership_snapshot(after, requirements) && before == after;
}

EpochCallGate::Scope::Scope(EpochCallGate& gate) noexcept : gate_(gate) {
    std::uint64_t observed = gate_.state_.load(std::memory_order_acquire);
    for (;;) {
        const std::uint32_t active =
            static_cast<std::uint32_t>(observed & EpochCallGate::kActiveMask);
        if (active == (std::numeric_limits<std::uint32_t>::max)()) {
            return;
        }
        const std::uint64_t desired = observed + 1U;
        if (gate_.state_.compare_exchange_weak(
                observed, desired, std::memory_order_acq_rel, std::memory_order_acquire)) {
            counted_ = true;
            admitted_ = (observed & EpochCallGate::kAcceptingBit) != 0U;
            epoch_ = (observed & EpochCallGate::kEpochMask) >> 32U;
            return;
        }
    }
}

EpochCallGate::Scope::~Scope() noexcept {
    if (counted_) {
        gate_.state_.fetch_sub(1U, std::memory_order_release);
    }
}

bool EpochCallGate::open(std::uint64_t epochValue) noexcept {
    if (epochValue == 0U || epochValue > 0x7FFFFFFFU) {
        return false;
    }
    std::uint64_t observed = state_.load(std::memory_order_acquire);
    for (;;) {
        const std::uint64_t oldEpoch = (observed & kEpochMask) >> 32U;
        if ((observed & (kAcceptingBit | kActiveMask)) != 0U || epochValue <= oldEpoch) {
            return false;
        }
        const std::uint64_t desired = kAcceptingBit | (epochValue << 32U);
        if (state_.compare_exchange_weak(
                observed, desired, std::memory_order_acq_rel, std::memory_order_acquire)) {
            return true;
        }
    }
}

bool EpochCallGate::quiesce(std::uint64_t epochValue) noexcept {
    std::uint64_t observed = state_.load(std::memory_order_acquire);
    for (;;) {
        if ((observed & kAcceptingBit) == 0U || ((observed & kEpochMask) >> 32U) != epochValue) {
            return false;
        }
        const std::uint64_t desired = observed & ~kAcceptingBit;
        if (state_.compare_exchange_weak(
                observed, desired, std::memory_order_acq_rel, std::memory_order_acquire)) {
            return true;
        }
    }
}

bool EpochCallGate::idle() const noexcept {
    return (state_.load(std::memory_order_acquire) & kActiveMask) == 0U;
}
std::uint64_t EpochCallGate::epoch() const noexcept {
    return (state_.load(std::memory_order_acquire) & kEpochMask) >> 32U;
}
std::uint32_t EpochCallGate::active_calls() const noexcept {
    return static_cast<std::uint32_t>(state_.load(std::memory_order_acquire) & kActiveMask);
}

TraceResult TraceCoordinator::begin_terminal(ThreadTraceStack& stack,
                                             const EpochCallGate::Scope& scope,
                                             const OwnershipSnapshotProvider& owners,
                                             const TerminalEntryInput& input) noexcept {
    if (!scope.admitted()) {
        return TraceResult::not_admitted;
    }
    if (stack.depth_ == stack.frames_.size()) {
        return TraceResult::stack_full;
    }
    if (input.instance == nullptr || input.matched_reference == nullptr
        || input.runtime_volume == nullptr || !valid_metadata(input.metadata)
        || input.metadata.normalized_caller_rva != kTerminalEvaluatorCallsiteRva) {
        return TraceResult::invalid_input;
    }

    ThreadTraceStack::Frame frame{};
    RestrictedIncidentRecord& record = frame.record;
    std::uint32_t liveDefinition{};
    bool copied =
        fenced_capture(owners, kTerminalOwners, record.terminal_entry_fence, [&]() noexcept {
            return safe_copy_exact(&liveDefinition, input.instance, sizeof liveDefinition)
                   && safe_copy_exact(&record.matched_reference,
                                      input.matched_reference,
                                      sizeof record.matched_reference)
                   && safe_copy_exact(record.authority_window_before.data(),
                                      offset_address(input.instance, kConsumedGenerationOffset),
                                      record.authority_window_before.size())
                   && safe_copy_exact(
                       &record.membership_mask_before,
                       offset_address(input.runtime_volume, kRuntimeVolumeMembershipOffset),
                       sizeof record.membership_mask_before);
        });
    if (!copied) {
        return TraceResult::unreadable;
    }
    record.point = point_from_definition(liveDefinition);
    if (!admitted_point(record.point)) {
        return TraceResult::wrong_definition;
    }
    if (record.matched_reference != record.point.volume) {
        return TraceResult::wrong_reference;
    }
    std::uint64_t callId = next_terminal_call_id_.load(std::memory_order_acquire);
    for (;;) {
        if (callId == (std::numeric_limits<std::uint64_t>::max)()) {
            return TraceResult::invalid_input;
        }
        if (next_terminal_call_id_.compare_exchange_weak(
                callId, callId + 1U, std::memory_order_acq_rel, std::memory_order_acquire)) {
            break;
        }
    }
    record.cohort = record.terminal_entry_fence.before.cohort;
    record.capture_epoch = record.terminal_entry_fence.before.capture_epoch;
    record.gate_epoch = scope.epoch();
    record.terminal_call_id = callId;
    record.instance_identity = reinterpret_cast<std::uintptr_t>(input.instance);
    record.runtime_volume_identity = reinterpret_cast<std::uintptr_t>(input.runtime_volume);
    record.terminal_entry_metadata = input.metadata;
    record.authority_before = decode_authority(record.authority_window_before);
    record.terminal_entry_readable = true;
    record.terminal_predicate_satisfied = terminal_predicate(record);
    record.phases = TracePhase::terminal_entry;
    frame.terminal_owner = record.terminal_entry_fence.before;
    stack.frames_[stack.depth_++] = frame;
    return record.terminal_entry_fence.exact && record.terminal_predicate_satisfied
               ? TraceResult::complete
               : TraceResult::partial;
}

TraceResult TraceCoordinator::observe_root_dispatch(ThreadTraceStack& stack,
                                                    const OwnershipSnapshotProvider& owners,
                                                    const RootEntryInput& input) noexcept {
    if (stack.depth_ == 0U) {
        return TraceResult::no_active_trace;
    }
    if (input.sobject_id_storage == nullptr || input.root_payload == nullptr
        || !valid_metadata(input.metadata)
        || input.metadata.normalized_caller_rva
               != native_boundary(NativeSurface::sobject_validate_dispatch).mapped_rva) {
        return TraceResult::invalid_input;
    }
    ThreadTraceStack::Frame& frame = stack.frames_[stack.depth_ - 1U];
    RestrictedIncidentRecord& record = frame.record;
    if (phase_contains(record.phases, TracePhase::root_dispatch)
        || !phase_contains(record.phases, TracePhase::terminal_entry)) {
        record.phase_order_valid = false;
        return TraceResult::phase_order_error;
    }
    RootEvidence& root = record.root;
    bool dynamicValid{};
    const bool copied = fenced_capture(owners, kManagerOwners, root.fence, [&]() noexcept {
        const bool rootCopied =
            safe_copy_exact(
                &root.sobject_id_value, input.sobject_id_storage, sizeof root.sobject_id_value)
            && safe_copy_exact(root.root_bytes.data(), input.root_payload, root.root_bytes.size());
        dynamicValid = copy_opaque(input.dynamic_type35, root.dynamic_type35);
        return rootCopied;
    });
    root.sobject_storage_identity = reinterpret_cast<std::uintptr_t>(input.sobject_id_storage);
    root.root_storage_identity = reinterpret_cast<std::uintptr_t>(input.root_payload);
    root.descriptor_type = input.descriptor_type;
    root.metadata = input.metadata;
    root.root_readable = copied;
    if (copied) {
        std::memcpy(&root.object_reference,
                    root.root_bytes.data() + kRootObjectReferenceOffset,
                    sizeof root.object_reference);
        std::memcpy(&root.resolved_reference,
                    root.root_bytes.data() + kRootResolvedReferenceOffset,
                    sizeof root.resolved_reference);
    }
    record.phases = record.phases | TracePhase::root_dispatch;
    const bool sameTrace =
        root.fence.before_valid && snapshots_share_trace(frame.terminal_owner, root.fence.before);
    return copied && dynamicValid && root.fence.exact && sameTrace ? TraceResult::complete
                                                                   : TraceResult::partial;
}

TraceResult TraceCoordinator::observe_route_phase(ThreadTraceStack& stack,
                                                  const OwnershipSnapshotProvider& owners,
                                                  std::uint8_t routeIndex,
                                                  TracePhase phase) noexcept {
    if (stack.depth_ == 0U) {
        return TraceResult::no_active_trace;
    }
    ThreadTraceStack::Frame& frame = stack.frames_[stack.depth_ - 1U];
    RestrictedIncidentRecord& record = frame.record;
    if (!phase_contains(record.phases, TracePhase::root_dispatch)
        || frame.next_route_phase != routeIndex || routeIndex >= record.route_fences.size()) {
        record.phase_order_valid = false;
        return TraceResult::phase_order_error;
    }
    PhaseFence& fence = record.route_fences[routeIndex];
    const bool fenced =
        fenced_capture(owners, kManagerOwners, fence, []() noexcept { return true; });
    record.phases = record.phases | phase;
    ++frame.next_route_phase;
    return fenced && fence.exact && snapshots_share_trace(frame.terminal_owner, fence.before)
               ? TraceResult::complete
               : TraceResult::partial;
}

TraceResult TraceCoordinator::observe_recursive_materialize_submit(
    ThreadTraceStack& stack, const OwnershipSnapshotProvider& owners) noexcept {
    return observe_route_phase(stack, owners, 0U, TracePhase::recursive_materialize_submit);
}
TraceResult
TraceCoordinator::observe_manager_submit(ThreadTraceStack& stack,
                                         const OwnershipSnapshotProvider& owners) noexcept {
    return observe_route_phase(stack, owners, 1U, TracePhase::manager_submit);
}
TraceResult
TraceCoordinator::observe_recursive_visitor(ThreadTraceStack& stack,
                                            const OwnershipSnapshotProvider& owners) noexcept {
    return observe_route_phase(stack, owners, 2U, TracePhase::recursive_visitor);
}
TraceResult
TraceCoordinator::observe_visitor_callback(ThreadTraceStack& stack,
                                           const OwnershipSnapshotProvider& owners) noexcept {
    return observe_route_phase(stack, owners, 3U, TracePhase::visitor_callback);
}
TraceResult
TraceCoordinator::observe_normal_route(ThreadTraceStack& stack,
                                       const OwnershipSnapshotProvider& owners) noexcept {
    return observe_route_phase(stack, owners, 4U, TracePhase::normal_route);
}

TraceResult TraceCoordinator::observe_listener_entry(ThreadTraceStack& stack,
                                                     const OwnershipSnapshotProvider& owners,
                                                     const ListenerEntryInput& input) noexcept {
    if (stack.depth_ == 0U) {
        return TraceResult::no_active_trace;
    }
    if (!valid_metadata(input.metadata)
        || input.metadata.normalized_caller_rva
               != native_boundary(NativeSurface::dynamic_listener_enumeration).mapped_rva) {
        return TraceResult::invalid_input;
    }
    ThreadTraceStack::Frame& frame = stack.frames_[stack.depth_ - 1U];
    RestrictedIncidentRecord& record = frame.record;
    if (frame.next_route_phase != record.route_fences.size()
        || phase_contains(record.phases, TracePhase::listener_entry)) {
        record.phase_order_valid = false;
        return TraceResult::phase_order_error;
    }
    ListenerEvidence& listener = record.listener;
    bool tableValid{};
    bool rowValid{};
    const bool fenced =
        fenced_capture(owners, kListenerOwners, listener.entry_fence, [&]() noexcept {
            tableValid = copy_opaque(input.table_window, listener.table_before);
            rowValid = copy_opaque(input.selected_row_window, listener.selected_row_before);
            return tableValid;
        });
    listener.entry_metadata = input.metadata;
    listener.entry_observed = true;
    record.phases = record.phases | TracePhase::listener_entry;
    frame.listener_owner = listener.entry_fence.before;
    return fenced && tableValid && rowValid && listener.entry_fence.exact
                   && snapshots_share_trace(frame.terminal_owner, listener.entry_fence.before)
               ? TraceResult::complete
               : TraceResult::partial;
}

TraceResult TraceCoordinator::observe_listener_exit(ThreadTraceStack& stack,
                                                    const OwnershipSnapshotProvider& owners,
                                                    const ListenerEntryInput& input) noexcept {
    if (stack.depth_ == 0U) {
        return TraceResult::no_active_trace;
    }
    if (!valid_metadata(input.metadata)
        || input.metadata.normalized_caller_rva
               != native_boundary(NativeSurface::dynamic_listener_enumeration).mapped_rva) {
        return TraceResult::invalid_input;
    }
    ThreadTraceStack::Frame& frame = stack.frames_[stack.depth_ - 1U];
    RestrictedIncidentRecord& record = frame.record;
    if (!phase_contains(record.phases, TracePhase::listener_entry)
        || phase_contains(record.phases, TracePhase::listener_exit)) {
        record.phase_order_valid = false;
        return TraceResult::phase_order_error;
    }
    ListenerEvidence& listener = record.listener;
    bool tableValid{};
    bool rowValid{};
    const bool fenced =
        fenced_capture(owners, kListenerOwners, listener.exit_fence, [&]() noexcept {
            tableValid = copy_opaque(input.table_window, listener.table_after);
            rowValid = copy_opaque(input.selected_row_window, listener.selected_row_after);
            return tableValid;
        });
    listener.exit_metadata = input.metadata;
    listener.exit_observed = true;
    record.phases = record.phases | TracePhase::listener_exit;
    const bool sameListener =
        listener.exit_fence.before_valid && frame.listener_owner == listener.exit_fence.before;
    return fenced && tableValid && rowValid && listener.exit_fence.exact && sameListener
               ? TraceResult::complete
               : TraceResult::partial;
}

TraceFinishResult TraceCoordinator::finish_terminal(ThreadTraceStack& stack,
                                                    const EpochCallGate::Scope& scope,
                                                    const OwnershipSnapshotProvider& owners,
                                                    const void* instance,
                                                    CaptureMetadata metadata,
                                                    RestrictedEvidenceQueue& queue) noexcept {
    TraceFinishResult result{};
    if (stack.depth_ == 0U) {
        return result;
    }
    ThreadTraceStack::Frame& liveFrame = stack.frames_[stack.depth_ - 1U];
    RestrictedIncidentRecord& liveRecord = liveFrame.record;
    if (!scope.admitted() || scope.epoch() != liveRecord.gate_epoch || instance == nullptr
        || reinterpret_cast<std::uintptr_t>(instance) != liveRecord.instance_identity
        || !valid_metadata(metadata)
        || metadata.normalized_caller_rva != kTerminalEvaluatorReturnRva) {
        liveRecord.phase_order_valid = false;
        RestrictedIncidentRecord abandoned = liveRecord;
        liveFrame = {};
        --stack.depth_;
        result.trace = TraceResult::invalid_input;
        result.queue = queue.try_push(abandoned);
        return result;
    }
    const bool copied =
        fenced_capture(owners, kManagerOwners, liveRecord.terminal_exit_fence, [&]() noexcept {
            return safe_copy_exact(liveRecord.authority_window_after.data(),
                                   offset_address(instance, kConsumedGenerationOffset),
                                   liveRecord.authority_window_after.size());
        });
    liveRecord.authority_after = decode_authority(liveRecord.authority_window_after);
    liveRecord.terminal_exit_metadata = metadata;
    liveRecord.terminal_original_returned = true;
    liveRecord.phases = liveRecord.phases | TracePhase::terminal_exit;
    const bool sameTrace =
        liveRecord.terminal_exit_fence.before_valid
        && snapshots_share_trace(liveFrame.terminal_owner, liveRecord.terminal_exit_fence.before);
    RestrictedIncidentRecord completed = liveRecord;
    liveFrame = {};
    --stack.depth_;
    result.trace = copied && completed.terminal_exit_fence.exact && sameTrace
                           && complete_generic_listener_trace(completed)
                       ? TraceResult::complete
                       : TraceResult::partial;
    result.queue = queue.try_push(completed);
    return result;
}

bool valid_restricted_record(const RestrictedIncidentRecord& record) noexcept {
    if (record.sensitivity != EvidenceSensitivity::restricted_local_re || !record.cohort.valid()
        || record.capture_epoch == 0U || record.gate_epoch == 0U || record.terminal_call_id == 0U
        || !admitted_point(record.point)
        || !phase_contains(record.phases, TracePhase::terminal_entry)
        || !record.terminal_entry_readable || record.instance_identity == 0U
        || record.runtime_volume_identity == 0U || !valid_metadata(record.terminal_entry_metadata)
        || record.terminal_entry_metadata.normalized_caller_rva != kTerminalEvaluatorCallsiteRva
        || record.matched_reference != record.point.volume
        || record.terminal_entry_fence.before.cohort != record.cohort
        || record.terminal_entry_fence.before.capture_epoch != record.capture_epoch) {
        return false;
    }
    if (phase_contains(record.phases, TracePhase::root_dispatch)) {
        if (!record.root.root_readable || record.root.sobject_storage_identity == 0U
            || record.root.root_storage_identity == 0U || !valid_metadata(record.root.metadata)
            || record.root.metadata.normalized_caller_rva
                   != native_boundary(NativeSurface::sobject_validate_dispatch).mapped_rva
            || record.root.sobject_id_value != kIncidentRootSchema
            || record.root.descriptor_type != kIncidentSObjectKind
            || !bounded_evidence_consistent(record.root.dynamic_type35)) {
            return false;
        }
    }
    if (phase_contains(record.phases, TracePhase::listener_entry)) {
        if (!record.listener.entry_observed
            || !bounded_evidence_consistent(record.listener.table_before)
            || !bounded_evidence_consistent(record.listener.selected_row_before)) {
            return false;
        }
    }
    if (phase_contains(record.phases, TracePhase::listener_exit)) {
        if (!record.listener.exit_observed
            || !bounded_evidence_consistent(record.listener.table_after)
            || !bounded_evidence_consistent(record.listener.selected_row_after)) {
            return false;
        }
    }
    return true;
}

bool complete_generic_listener_trace(const RestrictedIncidentRecord& record) noexcept {
    if (!valid_restricted_record(record) || !record.phase_order_valid
        || !phase_contains(record.phases, kCompleteTracePhases)
        || !record.terminal_predicate_satisfied || !record.terminal_original_returned
        || !record.terminal_entry_fence.exact || !record.root.fence.exact
        || !record.listener.entry_fence.exact || !record.listener.exit_fence.exact
        || !record.terminal_exit_fence.exact || record.root.dynamic_type35.truncated
        || record.listener.table_before.truncated || record.listener.table_after.truncated
        || record.listener.selected_row_before.truncated
        || record.listener.selected_row_after.truncated
        || record.root.dynamic_type35.presence != Presence::present
        || record.listener.selected_row_before.presence != Presence::present
        || record.listener.selected_row_after.presence != Presence::present) {
        return false;
    }
    const OwnershipSnapshot& terminalOwner = record.terminal_entry_fence.before;
    const OwnershipSnapshot& managerOwner = record.root.fence.before;
    const auto sameManagerTrace = [&](const OwnershipSnapshot& owner) noexcept {
        return snapshots_share_trace(terminalOwner, owner)
               && owner.incident_manager == managerOwner.incident_manager;
    };
    if (!sameManagerTrace(managerOwner) || !sameManagerTrace(record.terminal_exit_fence.before)
        || !sameManagerTrace(record.listener.entry_fence.before)
        || !sameManagerTrace(record.listener.exit_fence.before)
        || record.listener.entry_fence.before.listener_table
               != record.listener.exit_fence.before.listener_table) {
        return false;
    }
    return std::all_of(
        record.route_fences.begin(), record.route_fences.end(), [&](const PhaseFence& fence) {
            return fence.exact && sameManagerTrace(fence.before);
        });
}

bool one_shot_commit_observed(const RestrictedIncidentRecord& record) noexcept {
    if (!valid_restricted_record(record) || !record.terminal_exit_fence.exact
        || !record.terminal_original_returned
        || !snapshots_share_trace(record.terminal_entry_fence.before,
                                  record.terminal_exit_fence.before)
        || record.authority_after.consumed_generation == kUnsetGeneration) {
        return false;
    }
    return record.authority_before.consumed_generation == kUnsetGeneration
           || record.authority_after.consumed_generation
                  > record.authority_before.consumed_generation;
}

bool RestrictedEvidenceQueue::try_lock() noexcept {
    return !lock_.test_and_set(std::memory_order_acquire);
}
void RestrictedEvidenceQueue::unlock() noexcept {
    lock_.clear(std::memory_order_release);
}

QueuePushResult RestrictedEvidenceQueue::try_push(const RestrictedIncidentRecord& record) noexcept {
    if (record.sequence != 0U || !valid_restricted_record(record)) {
        rejected_.fetch_add(1U, std::memory_order_relaxed);
        return QueuePushResult::rejected;
    }
    RestrictedIncidentRecord accepted = record;
    if (!try_lock()) {
        dropped_busy_.fetch_add(1U, std::memory_order_relaxed);
        return QueuePushResult::busy;
    }
    struct Unlock final {
        RestrictedEvidenceQueue& queue;
        ~Unlock() noexcept {
            queue.unlock();
        }
    } unlock{*this};
    if (queue_epoch_ != 0U && queue_epoch_ != record.capture_epoch) {
        rejected_epoch_.fetch_add(1U, std::memory_order_relaxed);
        return QueuePushResult::wrong_epoch;
    }
    if (count_ == records_.size()) {
        dropped_full_.fetch_add(1U, std::memory_order_relaxed);
        return QueuePushResult::full;
    }
    if (next_sequence_ == (std::numeric_limits<std::uint64_t>::max)()) {
        dropped_sequence_exhausted_.fetch_add(1U, std::memory_order_relaxed);
        return QueuePushResult::sequence_exhausted;
    }
    if (queue_epoch_ == 0U) {
        queue_epoch_ = record.capture_epoch;
    }
    accepted.sequence = next_sequence_++;
    records_[(head_ + count_) % records_.size()] = accepted;
    ++count_;
    accepted_.fetch_add(1U, std::memory_order_relaxed);
    return QueuePushResult::enqueued;
}

QueuePopResult RestrictedEvidenceQueue::try_pop(RestrictedIncidentRecord& output) noexcept {
    if (!try_lock()) {
        return QueuePopResult::busy;
    }
    struct Unlock final {
        RestrictedEvidenceQueue& queue;
        ~Unlock() noexcept {
            queue.unlock();
        }
    } unlock{*this};
    if (count_ == 0U) {
        return QueuePopResult::empty;
    }
    output = records_[head_];
    records_[head_] = {};
    head_ = (head_ + 1U) % records_.size();
    --count_;
    return QueuePopResult::success;
}

QueueCounters RestrictedEvidenceQueue::counters() const noexcept {
    return QueueCounters{accepted_.load(std::memory_order_relaxed),
                         rejected_.load(std::memory_order_relaxed),
                         rejected_epoch_.load(std::memory_order_relaxed),
                         dropped_full_.load(std::memory_order_relaxed),
                         dropped_busy_.load(std::memory_order_relaxed),
                         dropped_sequence_exhausted_.load(std::memory_order_relaxed)};
}

bool RestrictedEvidenceQueue::try_reset(const DetachReceipt& receipt) noexcept {
    if (!receipt.valid_for(queue_epoch_) || !try_lock()) {
        return false;
    }
    struct Unlock final {
        RestrictedEvidenceQueue& queue;
        ~Unlock() noexcept {
            queue.unlock();
        }
    } unlock{*this};
    if (count_ != 0U) {
        return false;
    }
    records_ = {};
    head_ = 0U;
    queue_epoch_ = 0U;
    next_sequence_ = 1U;
    accepted_.store(0U, std::memory_order_relaxed);
    rejected_.store(0U, std::memory_order_relaxed);
    rejected_epoch_.store(0U, std::memory_order_relaxed);
    dropped_full_.store(0U, std::memory_order_relaxed);
    dropped_busy_.store(0U, std::memory_order_relaxed);
    dropped_sequence_exhausted_.store(0U, std::memory_order_relaxed);
    return true;
}

bool RestrictedEvidenceQueue::empty_quiesced() noexcept {
    if (!try_lock()) {
        return false;
    }
    const bool empty = count_ == 0U;
    unlock();
    return empty;
}

#if defined(DAWN_TYPE31_INCIDENT_CAPTURE_TEST)
bool RestrictedEvidenceQueue::testing_lock() noexcept {
    return try_lock();
}
void RestrictedEvidenceQueue::testing_unlock() noexcept {
    unlock();
}
void RestrictedEvidenceQueue::testing_set_next_sequence(std::uint64_t sequence) noexcept {
    next_sequence_ = sequence;
}
#endif

bool HookGroupState::begin_install(const RuntimeCohortToken& cohort,
                                   std::uint64_t captureEpoch) noexcept {
    if (phase_ != HookGroupPhase::detached || !cohort.valid() || captureEpoch == 0U) {
        return false;
    }
    cohort_ = cohort;
    capture_epoch_ = captureEpoch;
    phase_ = HookGroupPhase::installing;
    return true;
}

ParticipantResult HookGroupState::record_attached(NativeSurface surface,
                                                  std::uintptr_t detourHandle,
                                                  std::uintptr_t original,
                                                  std::uint64_t protectedEpoch) noexcept {
    const auto index = static_cast<std::size_t>(surface);
    if (phase_ != HookGroupPhase::installing) {
        return ParticipantResult::wrong_phase;
    }
    if (index >= participants_.size() || detourHandle == 0U || original == 0U
        || protectedEpoch == 0U || participants_[index].attached) {
        return ParticipantResult::invalid;
    }
    participants_[index] = HookParticipant{detourHandle, original, protectedEpoch, true};
    attached_mask_ = static_cast<std::uint16_t>(attached_mask_ | surface_bit(surface));
    return ParticipantResult::recorded;
}

bool HookGroupState::complete_install(std::uint16_t requiredAttachedMask) noexcept {
    if (phase_ != HookGroupPhase::installing || requiredAttachedMask == 0U
        || (attached_mask_ & requiredAttachedMask) != requiredAttachedMask) {
        return false;
    }
    phase_ = HookGroupPhase::running;
    return true;
}

bool HookGroupState::retain_partial_install_failure() noexcept {
    if (phase_ != HookGroupPhase::installing || attached_mask_ == 0U) {
        return false;
    }
    phase_ = HookGroupPhase::quiescing;
    return true;
}

bool HookGroupState::quiesce(std::uint64_t captureEpoch) noexcept {
    if (phase_ == HookGroupPhase::quiescing) {
        return captureEpoch == capture_epoch_;
    }
    if (phase_ != HookGroupPhase::running || captureEpoch != capture_epoch_) {
        return false;
    }
    phase_ = HookGroupPhase::quiescing;
    return true;
}

ParticipantResult
HookGroupState::record_participant_detach(NativeSurface surface,
                                          ProtectedDetachDisposition disposition) noexcept {
    const auto index = static_cast<std::size_t>(surface);
    if (phase_ != HookGroupPhase::quiescing) {
        return ParticipantResult::wrong_phase;
    }
    if (index >= participants_.size() || !participants_[index].attached) {
        return ParticipantResult::invalid;
    }
    if (disposition == ProtectedDetachDisposition::removed) {
        participants_[index] = {};
        attached_mask_ = static_cast<std::uint16_t>(attached_mask_ & ~surface_bit(surface));
    }
    return ParticipantResult::recorded;
}

FinalDetachResult HookGroupState::finalize_detach(const AggregateParticipantSnapshot& aggregate,
                                                  DetachReceipt& receipt) noexcept {
    receipt = {};
    if (phase_ != HookGroupPhase::quiescing) {
        return FinalDetachResult::wrong_phase;
    }
    if (attached_mask_ != 0U) {
        return FinalDetachResult::participants_remain;
    }
    if (aggregate.capture_epoch != capture_epoch_ || aggregate.producer_epoch != capture_epoch_
        || aggregate.drain_epoch != capture_epoch_) {
        return FinalDetachResult::wrong_epoch;
    }
    if (aggregate.active_producers != 0U || aggregate.active_drains != 0U
        || !aggregate.evidence_empty || !aggregate.final_accounting_complete) {
        return FinalDetachResult::aggregate_not_idle;
    }
    receipt.epoch_ = capture_epoch_;
    receipt.detached_ = true;
    cohort_ = {};
    participants_ = {};
    attached_mask_ = 0U;
    capture_epoch_ = 0U;
    phase_ = HookGroupPhase::detached;
    return FinalDetachResult::detached;
}

HookGroupSnapshot HookGroupState::snapshot() const noexcept {
    return HookGroupSnapshot{cohort_, participants_, attached_mask_, capture_epoch_, phase_};
}

bool project_default(const RestrictedIncidentRecord& record, DefaultProjection& output) noexcept {
    output = {};
    if (!valid_restricted_record(record)) {
        return false;
    }
    DefaultProjection projected{};
    projected.packed_build_sha256 = kPinnedPackedRuntimeSha256;
    projected.mapped_prefix_digest = record.cohort.prefix_digest();
    projected.cohort_id = record.cohort.cohort_id();
    projected.cohort_generation = record.cohort.generation();
    projected.capture_epoch = record.capture_epoch;
    projected.gate_epoch = record.gate_epoch;
    projected.queue_sequence = record.sequence;
    projected.terminal_call_id = record.terminal_call_id;
    projected.terminal_rva =
        static_cast<std::uint32_t>(native_boundary(NativeSurface::point_terminal).mapped_rva);
    projected.listener_rva = static_cast<std::uint32_t>(
        native_boundary(NativeSurface::dynamic_listener_enumeration).mapped_rva);
    projected.root_schema = kIncidentRootSchema;
    projected.point_index = record.point.point.index;
    projected.volume_index = record.point.volume.index;
    projected.phase_mask = static_cast<std::uint16_t>(record.phases);
    projected.consumer = RetailConsumerDisposition::unknown_dynamic_listener;

    std::uint16_t validity{};
    if (record.terminal_entry_fence.exact) {
        validity |= static_cast<std::uint16_t>(ProjectionValidity::terminal_entry);
    }
    if (record.terminal_exit_fence.exact) {
        validity |= static_cast<std::uint16_t>(ProjectionValidity::terminal_exit);
    }
    if (record.root.root_readable && record.root.fence.exact) {
        validity |= static_cast<std::uint16_t>(ProjectionValidity::root);
        if (!sha256_bytes(record.root.root_bytes, projected.root_digest)) {
            return false;
        }
    }
    if (record.root.dynamic_type35.readable) {
        validity |= static_cast<std::uint16_t>(ProjectionValidity::dynamic);
        if (!sha256_bytes(record.root.dynamic_type35.captured(), projected.dynamic_type35_digest)) {
            return false;
        }
    }
    if (record.listener.table_before.readable) {
        validity |= static_cast<std::uint16_t>(ProjectionValidity::listener_entry);
        if (!sha256_bytes(record.listener.table_before.captured(),
                          projected.listener_table_before_digest)) {
            return false;
        }
    }
    if (record.listener.table_after.readable) {
        validity |= static_cast<std::uint16_t>(ProjectionValidity::listener_exit);
        if (!sha256_bytes(record.listener.table_after.captured(),
                          projected.listener_table_after_digest)) {
            return false;
        }
    }
    if (record.listener.selected_row_after.readable
        && !sha256_bytes(record.listener.selected_row_after.captured(),
                         projected.selected_row_digest)) {
        return false;
    }
    if (complete_generic_listener_trace(record)) {
        validity |= static_cast<std::uint16_t>(ProjectionValidity::complete_path);
    }
    const bool managerChanged =
        phase_contains(record.phases, TracePhase::root_dispatch)
        && (std::any_of(record.route_fences.begin(),
                        record.route_fences.end(),
                        [&](const PhaseFence& fence) {
                            return fence.before.incident_manager
                                   != record.root.fence.before.incident_manager;
                        })
            || (phase_contains(record.phases, TracePhase::listener_entry)
                && record.listener.entry_fence.before.incident_manager
                       != record.root.fence.before.incident_manager)
            || (phase_contains(record.phases, TracePhase::terminal_exit)
                && record.terminal_exit_fence.before.incident_manager
                       != record.root.fence.before.incident_manager));
    const bool torn =
        !record.terminal_entry_fence.exact
        || (phase_contains(record.phases, TracePhase::root_dispatch) && !record.root.fence.exact)
        || (phase_contains(record.phases, TracePhase::listener_entry)
            && !record.listener.entry_fence.exact)
        || managerChanged
        || (phase_contains(record.phases, TracePhase::listener_exit)
            && !record.listener.exit_fence.exact)
        || (phase_contains(record.phases, TracePhase::terminal_exit)
            && (!record.terminal_exit_fence.exact
                || !snapshots_share_trace(record.terminal_entry_fence.before,
                                          record.terminal_exit_fence.before)))
        || (phase_contains(record.phases, TracePhase::root_dispatch)
            && !snapshots_share_trace(record.terminal_entry_fence.before, record.root.fence.before))
        || (phase_contains(record.phases, TracePhase::listener_exit)
            && (record.listener.entry_fence.before.incident_manager
                    != record.listener.exit_fence.before.incident_manager
                || record.listener.entry_fence.before.listener_table
                       != record.listener.exit_fence.before.listener_table));
    if (torn) {
        validity |= static_cast<std::uint16_t>(ProjectionValidity::torn_candidate);
    }
    const bool truncated =
        record.root.dynamic_type35.truncated || record.listener.table_before.truncated
        || record.listener.table_after.truncated || record.listener.selected_row_before.truncated
        || record.listener.selected_row_after.truncated;
    if (truncated) {
        validity |= static_cast<std::uint16_t>(ProjectionValidity::truncated);
    }
    projected.validity = static_cast<ProjectionValidity>(validity);
    output = projected;
    return true;
}

} // namespace dawn::client::hooks::bootflow::opening_authority::type31_incident
