#include "client/hooks/bootflow/opening_authority/objective_banner_variant_capture.h"

#include <algorithm>
#include <cstring>
#include <initializer_list>

namespace sunrise::client::hooks::bootflow::opening_authority::
    objective_banner_variant_capture {
namespace {

constexpr Sha256 sha256(std::initializer_list<unsigned int> values) noexcept {
    Sha256 result{};
    std::size_t index{};
    for (const unsigned int value : values) {
        if (index < result.size()) {
            result[index++] = std::byte{static_cast<unsigned char>(value)};
        }
    }
    return result;
}

inline constexpr ArtifactDescriptor kPackedRuntime{
    ArtifactKind::pc_packed_runtime,
    L"D:\\Destiny3\\destiny2.exe",
    {122'984'224U,
     sha256({0x81, 0x96, 0x43, 0x80, 0x66, 0x4E, 0x7F, 0xCE,
             0xE3, 0xC6, 0x20, 0x08, 0x5A, 0x15, 0x7F, 0xDE,
             0xAF, 0x91, 0xFE, 0xFA, 0xCF, 0x72, 0x14, 0x90,
             0x78, 0x20, 0xF1, 0x88, 0xBB, 0xEB, 0x4C, 0xED})}};

inline constexpr ArtifactDescriptor kUnpackedReference{
    ArtifactKind::pc_unpacked_reference,
    L"D:\\Sunrise-work\\ghidra\\destiny2_unpacked.exe",
    {145'091'072U,
     sha256({0x87, 0x13, 0xD1, 0x5E, 0x3D, 0x05, 0xB2, 0x6F,
             0x9E, 0x25, 0x9E, 0x02, 0xB0, 0xF2, 0x9B, 0xC1,
             0xE0, 0x00, 0xE4, 0xB0, 0xC6, 0x2C, 0xA2, 0xCC,
             0x87, 0xC3, 0x85, 0x97, 0x18, 0x6C, 0xC3, 0xBD})}};

inline constexpr ArtifactDescriptor kOmegaPackage{
    ArtifactKind::omega_activity_package,
    L"D:\\Destiny3\\packages\\w64_mercury_destination_activities_03a3_5.pkg",
    {407'552U,
     sha256({0x99, 0x77, 0xBA, 0xAA, 0xE8, 0x9B, 0xF0, 0x7A,
             0x81, 0x59, 0x02, 0xEC, 0x81, 0xEE, 0xCD, 0x28,
             0x9A, 0x7B, 0x1B, 0xA8, 0x1E, 0x7E, 0x32, 0x8C,
             0xD0, 0xF6, 0x7D, 0x1C, 0x25, 0x07, 0x5F, 0x17})}};

inline constexpr ArtifactDescriptor kPs4Reference{
    ArtifactKind::ps4_eboot_reference,
    L"D:\\eboot-d2-0159.bin",
    {37'824'851U,
     sha256({0x52, 0x75, 0x86, 0xF1, 0x37, 0x66, 0xFA, 0xFC,
             0xC9, 0x15, 0x70, 0x59, 0x83, 0x1C, 0xEC, 0xC1,
             0xF8, 0x2A, 0xBF, 0xB5, 0x39, 0x9B, 0xB1, 0x14,
             0x04, 0x9B, 0xB9, 0xE1, 0x13, 0x39, 0xAA, 0x78})}};

using Prefix = std::array<std::byte, kNativePrefixBytes>;

constexpr Prefix prefix(std::initializer_list<unsigned int> values) noexcept {
    Prefix result{};
    std::size_t index{};
    for (const unsigned int value : values) {
        if (index < result.size()) {
            result[index++] = std::byte{static_cast<unsigned char>(value)};
        }
    }
    return result;
}

inline constexpr Prefix kClassAccessorPrefix = prefix(
    {0x48, 0x83, 0xEC, 0x38, 0x48, 0x8D, 0x05, 0x6D,
     0x8D, 0xC0, 0x01, 0xB9, 0x40, 0x00, 0x00, 0x00});
inline constexpr Prefix kRegistryInitializerPrefix = prefix(
    {0x0F, 0xB7, 0x44, 0x24, 0x28, 0x66, 0x89, 0x41,
     0x18, 0x48, 0x8B, 0xC1, 0x48, 0x89, 0x11, 0x4C});
inline constexpr Prefix kHudAccessorPrefix = prefix(
    {0x48, 0x8D, 0x05, 0x09, 0xD7, 0xC5, 0x01, 0xC3,
     0x1C, 0xBA, 0xD6, 0x95, 0xF6, 0x7F, 0x00, 0x00});
inline constexpr Prefix kCommand5ProducerPrefix = prefix(
    {0x48, 0x89, 0x5C, 0x24, 0x20, 0x55, 0x48, 0x8B,
     0xEC, 0x48, 0x83, 0xEC, 0x70, 0x33, 0xC9, 0xE8});
inline constexpr Prefix kCommand5InsertPrefix = prefix(
    {0x48, 0x8B, 0xC4, 0x57, 0x48, 0x83, 0xEC, 0x70,
     0x48, 0x8B, 0xF9, 0x4C, 0x63, 0xCA, 0x8B, 0x89});
inline constexpr Prefix kType68ApplyPrefix = prefix(
    {0x40, 0x53, 0x57, 0x41, 0x57, 0x48, 0x83, 0xEC,
     0x40, 0x44, 0x8B, 0x02, 0x4C, 0x8B, 0xF9, 0x4C});
inline constexpr Prefix kFormatterPrefix = prefix(
    {0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x6C,
     0x24, 0x18, 0x48, 0x89, 0x74, 0x24, 0x20, 0x57});
inline constexpr Prefix kMaterializerPrefix = prefix(
    {0x48, 0x89, 0x5C, 0x24, 0x10, 0x4C, 0x89, 0x4C,
     0x24, 0x20, 0x55, 0x56, 0x57, 0x41, 0x54, 0x41});
inline constexpr Prefix kPayloadBuilderPrefix = prefix(
    {0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x74,
     0x24, 0x18, 0x48, 0x89, 0x7C, 0x24, 0x20, 0x55});
inline constexpr Prefix kObjectiveInsertPrefix = prefix(
    {0x48, 0x8B, 0xC4, 0x57, 0x48, 0x83, 0xEC, 0x70,
     0x48, 0x8B, 0xF9, 0x4C, 0x63, 0xCA, 0x8B, 0x89});
inline constexpr Prefix kQueueTickPrefix = prefix(
    {0x40, 0x57, 0x48, 0x83, 0xEC, 0x40, 0x83, 0xB9,
     0x88, 0x03, 0x00, 0x00, 0xFF, 0x48, 0x8B, 0xF9});
inline constexpr Prefix kQueueErasePrefix = prefix(
    {0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74,
     0x24, 0x10, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x4C});
inline constexpr Prefix kQueueResetPrefix = prefix(
    {0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83,
     0xEC, 0x20, 0x33, 0xFF, 0xC7, 0x81, 0x90, 0x03});
inline constexpr Prefix kQueueTeardownPrefix = prefix(
    {0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83,
     0xEC, 0x20, 0x48, 0x8B, 0xD9, 0xE8, 0x9E, 0xEA});
inline constexpr Prefix kDispatchLoaderPrefix = prefix(
    {0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B,
     0xD9, 0x48, 0x8D, 0x4C, 0x24, 0x40, 0xE8, 0xDD});
inline constexpr Prefix kTerminalPrefix = prefix(
    {0x40, 0x53, 0x55, 0x48, 0x83, 0xEC, 0x28, 0x48,
     0x8B, 0xD9, 0x48, 0x89, 0x74, 0x24, 0x50, 0x48});

struct BoundaryRow final {
    std::uintptr_t rva{};
    const Prefix* prefix_bytes{};
    BoundaryKind kind{BoundaryKind::opaque_runtime_join};
    NativeAbi abi{NativeAbi::no_argument_or_internal};
};

inline constexpr std::array<BoundaryRow, kNativeSurfaceCount> kBoundaries{{
    {0x7DCA0U, &kClassAccessorPrefix, BoundaryKind::function_entry,
     NativeAbi::descriptor_accessor},
    {0x1323DD0U, &kRegistryInitializerPrefix, BoundaryKind::function_entry,
     NativeAbi::registry_initializer},
    {0x1353EF0U, &kHudAccessorPrefix, BoundaryKind::function_entry,
     NativeAbi::no_argument_or_internal},
    {0x131FDF0U, &kCommand5ProducerPrefix, BoundaryKind::function_entry,
     NativeAbi::no_argument_or_internal},
    {0x131A630U, &kCommand5InsertPrefix, BoundaryKind::function_entry,
     NativeAbi::queue_command_payload},
    {0x1009C00U, &kType68ApplyPrefix, BoundaryKind::function_entry,
     NativeAbi::component_authority_handle},
    {0x1009ED0U, nullptr, BoundaryKind::function_entry,
     NativeAbi::component_record_row_event},
    {0x1008B40U, &kFormatterPrefix, BoundaryKind::function_entry,
     NativeAbi::component_record_row_event},
    {0x137BD50U, nullptr, BoundaryKind::function_entry,
     NativeAbi::component_record_row_event},
    {0x137E6F0U, &kMaterializerPrefix, BoundaryKind::function_entry,
     NativeAbi::event_activity_entry_changed},
    {0x1382710U, &kPayloadBuilderPrefix, BoundaryKind::function_entry,
     NativeAbi::manager_entry},
    {0x137A170U, &kObjectiveInsertPrefix, BoundaryKind::function_entry,
     NativeAbi::queue_command_payload},
    {0x1382C30U, nullptr, BoundaryKind::function_entry, NativeAbi::manager_entry},
    {0x13A0220U, &kQueueTickPrefix, BoundaryKind::function_entry,
     NativeAbi::no_argument_or_internal},
    {0x131E0B0U, &kQueueErasePrefix, BoundaryKind::function_entry,
     NativeAbi::queue_record_or_index},
    {0x13991B0U, &kQueueResetPrefix, BoundaryKind::function_entry,
     NativeAbi::no_argument_or_internal},
    {0x139A1C0U, &kQueueTeardownPrefix, BoundaryKind::function_entry,
     NativeAbi::no_argument_or_internal},
    {0x139A2B0U, &kDispatchLoaderPrefix, BoundaryKind::function_entry,
     NativeAbi::no_argument_or_internal},
    {0x137DC10U, &kTerminalPrefix, BoundaryKind::function_entry,
     NativeAbi::manager_entry},
    {0x10091F0U, nullptr, BoundaryKind::function_entry,
     NativeAbi::component_record_row_event},
    {0U, nullptr, BoundaryKind::opaque_runtime_join, NativeAbi::cui_runtime_node},
}};

inline constexpr ResourceDescriptor kAggregateResource{
    ResourceRole::aggregate,
    kLayoutAggregate,
    kLayoutResourceClass,
    2'904U,
    sha256({0x1D, 0xD1, 0x62, 0xB3, 0x8C, 0x50, 0x71, 0x55,
            0x6A, 0xCB, 0x41, 0xDA, 0xA2, 0x1C, 0x3A, 0xB4,
            0x54, 0xC8, 0x18, 0x09, 0xD0, 0xC5, 0x52, 0xB2,
            0xC7, 0xF3, 0x69, 0xE0, 0x9B, 0x95, 0x18, 0xF2}),
    0U,
    623U,
    0U,
    0U};

inline constexpr ResourceDescriptor kMissionResource{
    ResourceRole::mission_activity_intro,
    0x80BC7233U,
    kLayoutResourceClass,
    45'964U,
    sha256({0x8C, 0xED, 0xE5, 0xE3, 0x0A, 0xD0, 0x18, 0xE2,
            0xC8, 0x12, 0x94, 0xC9, 0x0D, 0x70, 0xFE, 0xDB,
            0xCB, 0x99, 0x13, 0x4B, 0x1E, 0x8C, 0xC7, 0xA4,
            0x16, 0xC4, 0xC0, 0x20, 0xDC, 0x9D, 0xEE, 0xC2}),
    0U,
    126U,
    0x98BD8A0DU,
    0x41B8U};

inline constexpr ResourceDescriptor kBranchAResource{
    ResourceRole::objective_branch_a,
    kObjectiveBranchATag,
    kLayoutResourceClass,
    77'232U,
    sha256({0xD1, 0x3B, 0xAB, 0xF5, 0x50, 0x72, 0x4C, 0x44,
            0x4F, 0xCB, 0xF3, 0x3A, 0x90, 0x27, 0x04, 0xC8,
            0x46, 0xD2, 0x54, 0x9B, 0x20, 0xD2, 0xBE, 0x95,
            0x54, 0x03, 0x97, 0x66, 0x2B, 0x4D, 0x31, 0x01}),
    126U,
    190U,
    kObjectiveBranchAHash,
    0x12194U};

inline constexpr ResourceDescriptor kBranchBResource{
    ResourceRole::objective_branch_b,
    kObjectiveBranchBTag,
    kLayoutResourceClass,
    127'536U,
    sha256({0x3B, 0x37, 0x16, 0x5C, 0xFF, 0x7F, 0x85, 0xE3,
            0xF3, 0xA1, 0x55, 0x2F, 0xEF, 0x93, 0x93, 0x30,
            0x4D, 0x53, 0x2F, 0x5D, 0x47, 0x16, 0x1E, 0x1A,
            0x0F, 0x94, 0xFE, 0xB4, 0x25, 0xCA, 0x88, 0xB5}),
    325U,
    182U,
    kObjectiveBranchBHash,
    0x1E524U};

inline constexpr ResourceDescriptor kControlResource{
    ResourceRole::control_accessibility_excluded,
    kControlAccessibilityTag,
    0x80804832U,
    1'136U,
    sha256({0xFB, 0x70, 0xB4, 0x89, 0x6B, 0xC4, 0x82, 0x32,
            0x11, 0x08, 0x6F, 0x18, 0xBC, 0x51, 0xA9, 0x97,
            0xD4, 0x1E, 0x34, 0x3E, 0x0B, 0x14, 0x21, 0xC0,
            0xD2, 0xF5, 0xE2, 0x20, 0xA0, 0xFD, 0x16, 0x46}),
    0U,
    0U,
    kControlAccessibilityNewObjectiveHash,
    0x328U};

constexpr bool objective_command(std::int32_t command) noexcept {
    return command == 1 || command == 2 || command == 6;
}

constexpr bool known_banner_command(std::int32_t command) noexcept {
    return command >= 0 && command <= 15;
}

template <typename Value>
void hash_value(std::uint64_t& hash, const Value& value) noexcept {
    static_assert(std::is_trivially_copyable_v<Value>);
    const auto bytes = std::as_bytes(std::span{&value, 1U});
    for (const std::byte byte : bytes) {
        hash ^= static_cast<std::uint8_t>(byte);
        hash *= 1'099'511'628'211ULL;
    }
}

void hash_pair(std::uint64_t& hash, LocalizedPair pair) noexcept {
    hash_value(hash, pair.bank);
    hash_value(hash, pair.hash);
}

std::uint32_t read_u32(const std::array<std::byte, kObjectivePayloadBytes>& bytes,
                       std::size_t offset) noexcept {
    std::uint32_t result{};
    std::memcpy(&result, bytes.data() + offset, sizeof result);
    return result;
}

std::int32_t read_i32(const std::array<std::byte, kObjectivePayloadBytes>& bytes,
                      std::size_t offset) noexcept {
    std::int32_t result{};
    std::memcpy(&result, bytes.data() + offset, sizeof result);
    return result;
}

LocalizedPair read_pair(const std::array<std::byte, kObjectivePayloadBytes>& bytes,
                        std::size_t offset) noexcept {
    return {read_u32(bytes, offset), read_u32(bytes, offset + 4U)};
}

bool priority_row_valid(const PriorityCompatibilityRow& row,
                        std::uint32_t command) noexcept {
    return row.observed && row.command == command && row.exact_row_hash != 0U;
}

bool dispatch_valid(const DispatchSelection& dispatch) noexcept {
    if (dispatch.candidate_count == 0U || dispatch.candidate_count > dispatch.candidates.size()
        || dispatch.selected_tag == 0U || dispatch.selected_class == 0U
        || dispatch.selected_package_handle < 0 || dispatch.exact_enumeration_hash == 0U) {
        return false;
    }
    bool found{};
    for (std::size_t index = 0U; index < dispatch.candidate_count; ++index) {
        const DispatchCandidate& candidate = dispatch.candidates[index];
        if (candidate.tag == dispatch.selected_tag
            && candidate.tag_class == dispatch.selected_class
            && candidate.package_handle == dispatch.selected_package_handle
            && candidate.accepted) {
            found = true;
        }
    }
    return found;
}

bool same_queue(const QueueSnapshot& left, const QueueSnapshot& right) noexcept {
    return left == right;
}

bool valid_interval(const PresentationInterval& interval) noexcept {
    if (!interval.visible || interval.first_tick == 0U || interval.last_tick < interval.first_tick) {
        return false;
    }
    switch (interval.lane) {
    case PresentationLane::mission_header_shared_banner:
        return interval.command == static_cast<std::int32_t>(kMissionHeaderCommand)
               && interval.queue_sequence >= 0;
    case PresentationLane::objective_shared_banner:
        return objective_command(interval.command) && interval.queue_sequence >= 0;
    case PresentationLane::ghost_dialogue_independent:
    case PresentationLane::persistent_objective_display:
        return interval.command == -1 && interval.queue_sequence == -1;
    }
    return false;
}

bool overlaps(const PresentationInterval& left, const PresentationInterval& right) noexcept {
    return left.first_tick <= right.last_tick && right.first_tick <= left.last_tick;
}

CapturePhase queue_phase(QueueEventKind event) noexcept {
    switch (event) {
    case QueueEventKind::command5_insert:
        return CapturePhase::command5_insert;
    case QueueEventKind::objective_insert:
        return CapturePhase::objective_insert;
    case QueueEventKind::promote:
        return CapturePhase::queue_promote;
    case QueueEventKind::supersede:
        return CapturePhase::queue_supersede;
    case QueueEventKind::retire:
        return CapturePhase::queue_retire;
    case QueueEventKind::erase:
        return CapturePhase::queue_erase;
    case QueueEventKind::reset:
        return CapturePhase::queue_reset;
    case QueueEventKind::teardown:
        return CapturePhase::queue_teardown;
    case QueueEventKind::dispatch_load:
        return CapturePhase::dispatch_load;
    }
    return CapturePhase::command5_insert;
}

CapturePhase manager_phase(ManagerEventKind event) noexcept {
    switch (event) {
    case ManagerEventKind::add:
        return CapturePhase::manager_add;
    case ManagerEventKind::terminal_predicate:
        return CapturePhase::manager_terminal;
    case ManagerEventKind::remove:
    case ManagerEventKind::replacement_old_status:
        return CapturePhase::manager_remove;
    case ManagerEventKind::component_teardown:
        return CapturePhase::component_teardown;
    }
    return CapturePhase::manager_add;
}

} // namespace

const ArtifactDescriptor& artifact_descriptor(ArtifactKind kind) noexcept {
    switch (kind) {
    case ArtifactKind::pc_packed_runtime:
        return kPackedRuntime;
    case ArtifactKind::pc_unpacked_reference:
        return kUnpackedReference;
    case ArtifactKind::omega_activity_package:
        return kOmegaPackage;
    case ArtifactKind::ps4_eboot_reference:
        return kPs4Reference;
    }
    return kPackedRuntime;
}

bool matches_artifact(ArtifactKind kind, const ArtifactIdentity& identity) noexcept {
    return artifact_descriptor(kind).identity == identity;
}

std::uint64_t artifact_fingerprint(ArtifactKind kind) noexcept {
    const ArtifactDescriptor& descriptor = artifact_descriptor(kind);
    std::uint64_t hash = 14'695'981'039'346'656'037ULL;
    hash_value(hash, descriptor.identity.file_bytes);
    for (const std::byte byte : descriptor.identity.sha256) {
        hash_value(hash, byte);
    }
    return hash;
}

NativeBoundaryDescriptor native_boundary(NativeSurface surface) noexcept {
    const std::size_t index = static_cast<std::size_t>(surface);
    if (index >= kBoundaries.size()) {
        return {};
    }
    const BoundaryRow& row = kBoundaries[index];
    const std::span<const std::byte> prefixBytes =
        row.prefix_bytes == nullptr ? std::span<const std::byte>{}
                                    : std::span<const std::byte>{*row.prefix_bytes};
    return {row.rva, prefixBytes, row.kind, row.abi, false, false};
}

bool native_prefix_matches(NativeSurface surface,
                           std::span<const std::byte> observed) noexcept {
    const NativeBoundaryDescriptor descriptor = native_boundary(surface);
    return !descriptor.prefix.empty() && observed.size() >= descriptor.prefix.size()
           && std::equal(descriptor.prefix.begin(), descriptor.prefix.end(), observed.begin());
}

RuntimeAdmissionResult
validate_runtime_admission(const RuntimeAdmissionEvidence& evidence) noexcept {
    if (!matches_artifact(ArtifactKind::pc_packed_runtime, evidence.packed_runtime)) {
        return RuntimeAdmissionResult::packed_identity_mismatch;
    }
    if (evidence.mapped_image_base == 0U) {
        return RuntimeAdmissionResult::null_image_base;
    }
    for (std::size_t index = 0U; index < kBoundaries.size(); ++index) {
        const NativeBoundaryDescriptor descriptor =
            native_boundary(static_cast<NativeSurface>(index));
        if (descriptor.rva == 0U || descriptor.prefix.empty()) {
            continue;
        }
        if (descriptor.rva > evidence.mapped_image_bytes
            || descriptor.prefix.size() > evidence.mapped_image_bytes - descriptor.rva) {
            return RuntimeAdmissionResult::target_out_of_range;
        }
        const MappedPrefixObservation& observed = evidence.prefixes[index];
        if (observed.byte_count < descriptor.prefix.size()
            || !native_prefix_matches(static_cast<NativeSurface>(index), observed.bytes)) {
            return RuntimeAdmissionResult::prefix_missing_or_mismatch;
        }
    }
    return RuntimeAdmissionResult::admitted;
}

const ResourceDescriptor& resource_descriptor(ResourceRole role) noexcept {
    switch (role) {
    case ResourceRole::aggregate:
        return kAggregateResource;
    case ResourceRole::mission_activity_intro:
        return kMissionResource;
    case ResourceRole::objective_branch_a:
        return kBranchAResource;
    case ResourceRole::objective_branch_b:
        return kBranchBResource;
    case ResourceRole::control_accessibility_excluded:
        return kControlResource;
    }
    return kAggregateResource;
}

std::uint64_t resource_fingerprint(ResourceRole role) noexcept {
    const ResourceDescriptor& descriptor = resource_descriptor(role);
    std::uint64_t hash = 14'695'981'039'346'656'037ULL;
    hash_value(hash, descriptor.tag);
    hash_value(hash, descriptor.tag_class);
    hash_value(hash, descriptor.file_bytes);
    for (const std::byte byte : descriptor.sha256) {
        hash_value(hash, byte);
    }
    hash_value(hash, descriptor.node_base);
    hash_value(hash, descriptor.node_count);
    hash_value(hash, descriptor.new_objective_hash);
    hash_value(hash, descriptor.new_objective_offset);
    return hash;
}

std::uint64_t scalar_hash(std::span<const std::byte> bytes) noexcept {
    std::uint64_t hash = 14'695'981'039'346'656'037ULL;
    for (const std::byte byte : bytes) {
        hash ^= static_cast<std::uint8_t>(byte);
        hash *= 1'099'511'628'211ULL;
    }
    return hash;
}

std::uint64_t text_hash(std::string_view text) noexcept {
    return scalar_hash(std::as_bytes(std::span{text.data(), text.size()}));
}

bool fully_correlated(const CaptureContext& context) noexcept {
    if (context.presence_mask != kCompleteContextMask
        || context.artifact != ArtifactKind::pc_packed_runtime || !context.build_verified
        || !matches_artifact(context.artifact, context.build_identity)
        || context.runtime_admission_generation == 0U || context.capture_epoch == 0U
        || context.session_id == 0U || context.thread_id == 0U || context.call_id == 0U
        || context.monotonic_tick == 0U || context.return_rva == 0U
        || context.activity_index != kOmegaActivityIndex
        || context.activity_definition != kOmegaActivityDefinition) {
        return false;
    }
    const GenerationStamp& generation = context.generations;
    return generation.activity != 0U && generation.connection != 0U
           && generation.roster != 0U && generation.component != 0U
           && generation.manager != 0U && generation.hud_global != 0U
           && generation.cui_instance != 0U && generation.authority_publication != 0U
           && generation.queue != 0U;
}

bool phase_surface_compatible(CapturePhase phase, NativeSurface surface) noexcept {
    switch (phase) {
    case CapturePhase::command5_insert:
        return surface == NativeSurface::command5_insert;
    case CapturePhase::type68_apply:
        return surface == NativeSurface::type68_apply;
    case CapturePhase::type68_format:
        return surface == NativeSurface::type68_formatter;
    case CapturePhase::objective_materialize:
        return surface == NativeSurface::objective_model_materializer;
    case CapturePhase::objective_payload_build:
        return surface == NativeSurface::objective_payload_builder;
    case CapturePhase::objective_insert:
        return surface == NativeSurface::objective_insert;
    case CapturePhase::dispatch_load:
        return surface == NativeSurface::dispatch_loader;
    case CapturePhase::queue_promote:
    case CapturePhase::queue_supersede:
    case CapturePhase::queue_retire:
        return surface == NativeSurface::queue_tick;
    case CapturePhase::queue_erase:
        return surface == NativeSurface::queue_erase;
    case CapturePhase::queue_reset:
        return surface == NativeSurface::queue_reset;
    case CapturePhase::queue_teardown:
        return surface == NativeSurface::queue_teardown;
    case CapturePhase::manager_add:
        return surface == NativeSurface::objective_manager_add;
    case CapturePhase::manager_terminal:
        return surface == NativeSurface::manager_terminal_predicate;
    case CapturePhase::manager_remove:
        return surface == NativeSurface::objective_manager_tick;
    case CapturePhase::component_teardown:
        return surface == NativeSurface::component_teardown;
    case CapturePhase::instantiated_resource:
    case CapturePhase::provider_node_binding:
    case CapturePhase::presentation_timeline:
        return surface == NativeSurface::instantiated_cui_node;
    }
    return false;
}

bool valid_capture_header(const CaptureHeader& header) noexcept {
    return fully_correlated(header.context) && phase_surface_compatible(header.phase, header.surface)
           && header.pre_generations == header.context.generations
           && header.post_generations == header.context.generations
           && header.pre_monotonic_tick != 0U
           && header.post_monotonic_tick >= header.pre_monotonic_tick
           && header.participant_identity != 0U;
}

std::uint64_t queue_snapshot_hash(const QueueSnapshot& snapshot) noexcept {
    std::uint64_t hash = 14'695'981'039'346'656'037ULL;
    hash_value(hash, snapshot.queue_identity);
    hash_value(hash, snapshot.queued_count);
    hash_value(hash, snapshot.dispatch_definition_handle);
    hash_value(hash, snapshot.current_sequence);
    hash_value(hash, snapshot.current_retire_or_supersede);
    for (const std::uint32_t value : snapshot.dispatch_metadata) {
        hash_value(hash, value);
    }
    hash_value(hash, snapshot.current_validity_state);
    hash_value(hash, snapshot.current_command);
    hash_value(hash, snapshot.current_payload_hash);
    hash_value(hash, snapshot.next_sequence);
    hash_value(hash, snapshot.priority_compatibility_table_hash);
    return hash;
}

QueueSnapshot seal_queue_snapshot(QueueSnapshot snapshot) noexcept {
    snapshot.exact_state_hash = queue_snapshot_hash(snapshot);
    return snapshot;
}

bool valid_queue_snapshot(const QueueSnapshot& snapshot) noexcept {
    if (snapshot.queue_identity == 0U || snapshot.queued_count > kNativeBannerQueueCapacity
        || snapshot.dispatch_definition_handle < -1 || snapshot.current_sequence < -1
        || snapshot.current_command < -1 || snapshot.next_sequence < 0
        || snapshot.exact_state_hash == 0U
        || snapshot.exact_state_hash != queue_snapshot_hash(snapshot)) {
        return false;
    }
    if (snapshot.current_sequence < 0) {
        if (snapshot.current_command != -1 || snapshot.current_validity_state != 0U
            || snapshot.current_payload_hash != 0U) {
            return false;
        }
    } else if (!known_banner_command(snapshot.current_command)
               || snapshot.current_validity_state == 0U || snapshot.current_payload_hash == 0U
               || snapshot.next_sequence < snapshot.current_sequence) {
        return false;
    }
    if (snapshot.dispatch_definition_handle >= 0
        && snapshot.priority_compatibility_table_hash == 0U) {
        return false;
    }
    return true;
}

std::uint64_t command5_payload_hash(const Command5Payload& payload) noexcept {
    return scalar_hash(payload.raw);
}

Command5Payload seal_command5_payload(Command5Payload payload) noexcept {
    std::memcpy(payload.raw.data() + 0x00U, &payload.title.bank, sizeof payload.title.bank);
    std::memcpy(payload.raw.data() + 0x04U, &payload.title.hash, sizeof payload.title.hash);
    payload.raw[0x08U] = std::byte{payload.category};
    std::memcpy(payload.raw.data() + 0x0CU, &payload.icon_theme, sizeof payload.icon_theme);
    std::copy(payload.visual_tuple.begin(),
              payload.visual_tuple.end(),
              payload.raw.begin() + 0x10U);
    payload.exact_payload_hash = command5_payload_hash(payload);
    payload.exact_payload_observed = true;
    return payload;
}

Command5Payload project_command5_payload(
    const std::array<std::byte, 0x20U>& raw) noexcept {
    Command5Payload payload{};
    payload.raw = raw;
    std::memcpy(&payload.title.bank, raw.data() + 0x00U, sizeof payload.title.bank);
    std::memcpy(&payload.title.hash, raw.data() + 0x04U, sizeof payload.title.hash);
    payload.category = static_cast<std::uint8_t>(raw[0x08U]);
    std::memcpy(&payload.icon_theme, raw.data() + 0x0CU, sizeof payload.icon_theme);
    std::copy(raw.begin() + 0x10U, raw.end(), payload.visual_tuple.begin());
    payload.exact_payload_hash = scalar_hash(raw);
    payload.exact_payload_observed = true;
    return payload;
}

bool valid_command5_payload(const Command5Payload& payload) noexcept {
    const Command5Payload projected = project_command5_payload(payload.raw);
    return payload.exact_payload_observed && payload.title == kOmegaTitle
           && payload.category == kMissionPresentationCategory
           && payload.exact_payload_hash != 0U
           && payload.exact_payload_hash == command5_payload_hash(payload)
           && projected.title == payload.title && projected.category == payload.category
           && projected.icon_theme == payload.icon_theme
           && projected.visual_tuple == payload.visual_tuple;
}

bool valid_type68_authority(const Type68AuthorityObservation& observation) noexcept {
    return observation.registry == kActivityRegistry && observation.type == kType68
           && observation.index == kType68Index
           && observation.component_class == kType68ComponentClass
           && observation.authority_schema == kType68AuthoritySchema
           && observation.definition == kType68Definition
           && observation.content_bank == kType68ContentBank
           && observation.body_bits == kType68WireBits
           && observation.decoded_bytes == kType68DecodedBytes
           && observation.decoded_body_hash != 0U && observation.event == kOpeningEvent
           && observation.selector >= 0 && observation.selector <= 2
           && observation.decoded_lifecycle >= -1 && observation.canonical_round_trip;
}

std::uint32_t type68_hud_identity(std::uint32_t event, std::int32_t variant) noexcept {
    std::uint32_t hash = event;
    const std::uint32_t raw = static_cast<std::uint32_t>(variant);
    for (std::uint32_t shift = 0U; shift < 32U; shift += 8U) {
        const std::uint8_t byte = static_cast<std::uint8_t>(raw >> shift);
        hash = (hash * 0x01000193U) ^ byte;
    }
    return hash;
}

bool valid_formatter_observation(const FormatterObservation& observation) noexcept {
    if (observation.event != kOpeningEvent
        || observation.authored_pairs[0] != kOpeningPair1
        || observation.authored_pairs[1] != kOpeningPair2
        || observation.manager_event_pairs != observation.authored_pairs
        || observation.hud_identity
               != type68_hud_identity(observation.event, observation.record_variant)
        || observation.exact_manager_event_hash == 0U) {
        return false;
    }
    return true;
}

bool valid_objective_model(const FormatterObservation& formatter,
                           const ObjectiveModelObservation& model) noexcept {
    if (!valid_formatter_observation(formatter) || !model.manager_ready
        || model.activity_index != kOmegaActivityIndex || model.mode != formatter.mode
        || model.resolved_activity_title != kOmegaTitle || model.exact_model_hash == 0U
        || model.entry_secondary != formatter.authored_pairs[1]
        || model.first_subentry_pairs[1] != formatter.authored_pairs[2]
        || model.first_subentry_pairs[2] != formatter.authored_pairs[3]) {
        return false;
    }
    if (model.mode == 1U) {
        return model.entry_header == formatter.authored_pairs[0]
               && model.first_subentry_pairs[0] == formatter.authored_pairs[1];
    }
    return model.entry_header == kOmegaTitle
           && model.first_subentry_pairs[0] == formatter.authored_pairs[0];
}

ObjectivePayload project_objective_payload(
    std::uint32_t command,
    const std::array<std::byte, kObjectivePayloadBytes>& raw) noexcept {
    ObjectivePayload result{};
    result.command = command;
    result.raw = raw;
    result.pair_at_04 = read_pair(raw, 0x04U);
    result.pair_at_0c = read_pair(raw, 0x0CU);
    result.pair_at_14 = read_pair(raw, 0x14U);
    result.pair_at_1c = read_pair(raw, 0x1CU);
    result.category_state = static_cast<std::uint8_t>(raw[0x24U]);
    result.command6_value = read_i32(raw, 0x28U);
    result.bit1_field = static_cast<std::uint8_t>(raw[0x2CU]);
    result.bit3_field = static_cast<std::uint8_t>(raw[0x2DU]);
    result.exact_payload_hash = scalar_hash(raw);
    result.exact_payload_observed = true;
    return result;
}

bool valid_objective_payload(const ObjectivePayload& payload) noexcept {
    if (!objective_command(static_cast<std::int32_t>(payload.command))
        || !payload.exact_payload_observed || payload.exact_payload_hash == 0U
        || payload.exact_payload_hash != scalar_hash(payload.raw)) {
        return false;
    }
    const ObjectivePayload projected = project_objective_payload(payload.command, payload.raw);
    if (projected.pair_at_04 != payload.pair_at_04
        || projected.pair_at_0c != payload.pair_at_0c
        || projected.pair_at_14 != payload.pair_at_14
        || projected.pair_at_1c != payload.pair_at_1c
        || projected.category_state != payload.category_state
        || projected.command6_value != payload.command6_value
        || projected.bit1_field != payload.bit1_field
        || projected.bit3_field != payload.bit3_field) {
        return false;
    }
    if (payload.command == 1U) {
        return payload.command6_value == -1 && payload.bit1_field == 0U;
    }
    if (payload.command == 2U) {
        return payload.command6_value == -1;
    }
    return payload.bit1_field == 0U;
}

bool payload_matches_model(const ObjectivePayload& payload,
                           const ObjectiveModelObservation& model) noexcept {
    return valid_objective_payload(payload) && model.manager_ready
           && payload.pair_at_04 == model.entry_secondary
           && payload.pair_at_0c == model.first_subentry_pairs[1]
           && payload.pair_at_14 == model.entry_header
           && payload.pair_at_1c == model.first_subentry_pairs[2];
}

bool valid_queue_evidence(const QueueEvidence& evidence) noexcept {
    if (!valid_queue_snapshot(evidence.pre) || !valid_queue_snapshot(evidence.post)
        || evidence.pre.queue_identity != evidence.post.queue_identity) {
        return false;
    }
    switch (evidence.event) {
    case QueueEventKind::command5_insert:
    case QueueEventKind::objective_insert: {
        const bool commandValid = evidence.event == QueueEventKind::command5_insert
                                      ? evidence.command == kMissionHeaderCommand
                                            && valid_command5_payload(evidence.command5_payload)
                                      : objective_command(static_cast<std::int32_t>(evidence.command))
                                            && valid_objective_payload(evidence.objective_payload)
                                            && evidence.objective_payload.command
                                                   == evidence.command;
        if (!commandValid) {
            return false;
        }
        if (evidence.disposition == QueueDisposition::accepted) {
            return priority_row_valid(evidence.priority_row, evidence.command)
                   && evidence.pre.queued_count < kNativeBannerQueueCapacity
                   && evidence.post.queued_count == evidence.pre.queued_count + 1U;
        }
        if (!same_queue(evidence.pre, evidence.post)) {
            return false;
        }
        if (evidence.disposition == QueueDisposition::rejected_definition_unresolved) {
            return evidence.pre.dispatch_definition_handle == -1;
        }
        if (evidence.disposition == QueueDisposition::rejected_full) {
            return evidence.pre.queued_count == kNativeBannerQueueCapacity;
        }
        return evidence.disposition == QueueDisposition::rejected_authored_policy
               && priority_row_valid(evidence.priority_row, evidence.command);
    }
    case QueueEventKind::promote:
        return evidence.disposition == QueueDisposition::observed
               && known_banner_command(static_cast<std::int32_t>(evidence.command))
               && evidence.pre.queued_count > 0U
               && evidence.post.queued_count + 1U == evidence.pre.queued_count
               && evidence.post.current_sequence >= 0
               && evidence.post.current_command == static_cast<std::int32_t>(evidence.command)
               && priority_row_valid(evidence.priority_row, evidence.command);
    case QueueEventKind::supersede:
        return evidence.disposition == QueueDisposition::observed
               && evidence.pre.current_sequence >= 0
               && (evidence.post.current_retire_or_supersede != 0U
                   || evidence.post.current_sequence != evidence.pre.current_sequence);
    case QueueEventKind::retire:
        return evidence.disposition == QueueDisposition::observed
               && evidence.pre.current_sequence >= 0
               && evidence.post.current_sequence != evidence.pre.current_sequence;
    case QueueEventKind::erase:
        return evidence.disposition == QueueDisposition::observed
               && evidence.erased_record_hash != 0U
               && evidence.erased_index < evidence.pre.queued_count
               && evidence.post.queued_count + 1U == evidence.pre.queued_count;
    case QueueEventKind::reset:
        return evidence.disposition == QueueDisposition::observed
               && evidence.post.queued_count == 0U && evidence.post.current_sequence == -1
               && evidence.post.current_command == -1
               && evidence.post.current_retire_or_supersede == 0U
               && evidence.post.next_sequence == 0
               && evidence.post.dispatch_definition_handle
                      == evidence.pre.dispatch_definition_handle;
    case QueueEventKind::teardown:
        return evidence.disposition == QueueDisposition::observed
               && evidence.post.queued_count == 0U && evidence.post.current_sequence == -1
               && evidence.post.current_command == -1
               && evidence.post.next_sequence == 0
               && evidence.post.dispatch_definition_handle == -1;
    case QueueEventKind::dispatch_load:
        return evidence.disposition == QueueDisposition::observed
               && evidence.pre.dispatch_definition_handle == -1
               && evidence.post.dispatch_definition_handle >= 0
               && evidence.post.dispatch_definition_handle
                      == evidence.dispatch.selected_package_handle
               && dispatch_valid(evidence.dispatch);
    }
    return false;
}

std::uint64_t manager_snapshot_hash(const ManagerEntrySnapshot& snapshot) noexcept {
    std::uint64_t hash = 14'695'981'039'346'656'037ULL;
    hash_value(hash, snapshot.manager_identity);
    hash_value(hash, snapshot.entry_count);
    hash_value(hash, snapshot.entry_identity_hash);
    hash_value(hash, snapshot.decoded_lifecycle);
    hash_value(hash, snapshot.external_status);
    hash_value(hash, snapshot.internal_status);
    hash_value(hash, snapshot.flags);
    hash_value(hash, snapshot.new_mode);
    return hash;
}

ManagerEntrySnapshot seal_manager_snapshot(ManagerEntrySnapshot snapshot) noexcept {
    snapshot.exact_state_hash = manager_snapshot_hash(snapshot);
    return snapshot;
}

bool valid_manager_snapshot(const ManagerEntrySnapshot& snapshot) noexcept {
    if (snapshot.manager_identity == 0U || snapshot.entry_count > kObjectiveManagerCapacity
        || snapshot.exact_state_hash == 0U
        || snapshot.exact_state_hash != manager_snapshot_hash(snapshot)) {
        return false;
    }
    const LifecycleMapping mapping = map_type68_lifecycle(snapshot.decoded_lifecycle);
    return mapping.external == snapshot.external_status
           && mapping.internal == snapshot.internal_status;
}

bool valid_manager_evidence(const ManagerEvidence& evidence) noexcept {
    if (!valid_manager_snapshot(evidence.pre) || !valid_manager_snapshot(evidence.post)
        || evidence.pre.manager_identity != evidence.post.manager_identity) {
        return false;
    }
    switch (evidence.event) {
    case ManagerEventKind::add:
        return evidence.pre.entry_count < kObjectiveManagerCapacity
               && evidence.post.entry_count == evidence.pre.entry_count + 1U
               && evidence.post.entry_identity_hash != 0U
               && map_type68_lifecycle(evidence.post.decoded_lifecycle).semantic
                      == LifecycleSemantic::active_installable;
    case ManagerEventKind::terminal_predicate:
        return evidence.pre == evidence.post;
    case ManagerEventKind::remove:
        return evidence.terminal_predicate_result && evidence.pre.entry_count > 0U
               && evidence.post.entry_count + 1U == evidence.pre.entry_count;
    case ManagerEventKind::replacement_old_status:
        return evidence.old_status_sent_before_new_install
               && evidence.pre.entry_identity_hash != evidence.post.entry_identity_hash
               && evidence.pre.external_status == 4;
    case ManagerEventKind::component_teardown:
        return evidence.post.external_status == 0 && evidence.post.internal_status == -1
               && evidence.post.decoded_lifecycle == -1;
    }
    return false;
}

std::uint64_t resource_node_hash(const ResourceNodeSnapshot& snapshot) noexcept {
    std::uint64_t hash = 14'695'981'039'346'656'037ULL;
    hash_value(hash, snapshot.cui_instance_identity);
    hash_value(hash, snapshot.resource_tag);
    hash_value(hash, snapshot.resource_class);
    hash_value(hash, snapshot.program_tag);
    hash_value(hash, snapshot.node_index);
    hash_value(hash, snapshot.property_id);
    hash_value(hash, snapshot.visible_constant_hash);
    hash_value(hash, snapshot.queue_sequence);
    hash_value(hash, snapshot.queue_command);
    hash_value(hash, snapshot.active);
    hash_value(hash, snapshot.visible);
    hash_value(hash, snapshot.exact_resource_hash);
    return hash;
}

ResourceNodeSnapshot seal_resource_node(ResourceNodeSnapshot snapshot) noexcept {
    snapshot.exact_node_hash = resource_node_hash(snapshot);
    return snapshot;
}

bool valid_resource_node(const ResourceNodeSnapshot& snapshot) noexcept {
    return snapshot.cui_instance_identity != 0U && snapshot.resource_tag != 0U
           && snapshot.resource_class != 0U && snapshot.program_tag != 0U
           && snapshot.property_id != 0U && snapshot.queue_sequence >= 0
           && known_banner_command(snapshot.queue_command) && snapshot.exact_resource_hash != 0U
           && snapshot.exact_node_hash != 0U
           && snapshot.exact_node_hash == resource_node_hash(snapshot);
}

bool valid_visible_variant(const ResourceNodeEvidence& evidence) noexcept {
    if (!valid_resource_node(evidence.pre) || !valid_resource_node(evidence.post)
        || evidence.pre.cui_instance_identity != evidence.post.cui_instance_identity
        || evidence.pre.resource_tag != evidence.post.resource_tag
        || evidence.pre.resource_class != evidence.post.resource_class
        || evidence.pre.exact_resource_hash != evidence.post.exact_resource_hash
        || evidence.pre.queue_sequence != evidence.post.queue_sequence
        || evidence.pre.queue_command != evidence.post.queue_command
        || !evidence.post.active || !evidence.post.visible
        || !objective_command(evidence.post.queue_command)) {
        return false;
    }
    const ObjectiveResourceBranch classified = classify_objective_resource(
        evidence.post.resource_tag, evidence.post.visible_constant_hash);
    const ResourceRole role = classified == ObjectiveResourceBranch::branch_a
                                  ? ResourceRole::objective_branch_a
                                  : ResourceRole::objective_branch_b;
    return (classified == ObjectiveResourceBranch::branch_a
            || classified == ObjectiveResourceBranch::branch_b)
           && evidence.observed_branch == classified
           && evidence.post.resource_class == kLayoutResourceClass
           && evidence.post.exact_resource_hash == resource_fingerprint(role);
}

LocalizedPair payload_pair(const ObjectivePayload& payload, PayloadPairSlot slot) noexcept {
    switch (slot) {
    case PayloadPairSlot::at_04:
        return payload.pair_at_04;
    case PayloadPairSlot::at_0c:
        return payload.pair_at_0c;
    case PayloadPairSlot::at_14:
        return payload.pair_at_14;
    case PayloadPairSlot::at_1c:
        return payload.pair_at_1c;
    }
    return {};
}

bool valid_node_binding(const NodeBindingEvidence& binding,
                        const ObjectivePayload& payload) noexcept {
    const ProviderPairDescriptor descriptor = provider_descriptor(binding.slot);
    const ObjectiveResourceBranch branch = classify_objective_resource(
        binding.node.resource_tag, binding.node.visible_constant_hash);
    const ResourceRole role = branch == ObjectiveResourceBranch::branch_a
                                  ? ResourceRole::objective_branch_a
                                  : ResourceRole::objective_branch_b;
    return valid_objective_payload(payload) && valid_resource_node(binding.node)
           && binding.node.active && binding.node.visible && objective_command(binding.node.queue_command)
           && (branch == ObjectiveResourceBranch::branch_a
               || branch == ObjectiveResourceBranch::branch_b)
           && binding.node.exact_resource_hash == resource_fingerprint(role)
           && binding.wrapper_id == descriptor.wrapper_id
           && binding.getter_id == descriptor.getter_id
           && binding.returned_pair == payload_pair(payload, binding.slot)
           && binding.resolved_pair == binding.returned_pair
           && binding.node.queue_command == static_cast<std::int32_t>(payload.command)
           && binding.resolved_text_hash != 0U;
}

bool valid_mission_header_node(const MissionHeaderNodeEvidence& evidence) noexcept {
    if (!valid_resource_node(evidence.node) || !evidence.node.active || !evidence.node.visible
        || evidence.node.queue_command != static_cast<std::int32_t>(kMissionHeaderCommand)
        || evidence.node.resource_tag != kMissionResource.tag
        || evidence.node.resource_class != kMissionResource.tag_class
        || evidence.node.exact_resource_hash
               != resource_fingerprint(ResourceRole::mission_activity_intro)
        || evidence.resolved_text_hash == 0U) {
        return false;
    }
    if (evidence.semantic == NodeSemantic::mission_title) {
        return evidence.resolved_pair == kOmegaTitle
               && evidence.node.visible_constant_hash == evidence.resolved_pair.hash;
    }
    if (evidence.semantic == NodeSemantic::mission_category) {
        return evidence.resolved_pair == kMissionCatalogPair
               && evidence.node.visible_constant_hash == evidence.resolved_pair.hash;
    }
    return false;
}

VariantClosureResult close_dynamic_variant(const VariantClosureEvidence& evidence,
                                           const ObjectivePayload& payload) noexcept {
    const ObjectiveResourceBranch rawBranch = classify_objective_resource(
        evidence.visible_resource.post.resource_tag,
        evidence.visible_resource.post.visible_constant_hash);
    if (rawBranch == ObjectiveResourceBranch::excluded_control_accessibility) {
        return VariantClosureResult::excluded_control_resource;
    }
    if (!valid_visible_variant(evidence.visible_resource)) {
        return VariantClosureResult::incomplete_resource;
    }
    if (!valid_objective_payload(payload) || evidence.binding_count > evidence.bindings.size()) {
        return VariantClosureResult::invalid;
    }
    bool title{};
    bool body{};
    for (std::size_t index = 0U; index < evidence.binding_count; ++index) {
        const NodeBindingEvidence& binding = evidence.bindings[index];
        if (!valid_node_binding(binding, payload)
            || binding.node.resource_tag != evidence.visible_resource.post.resource_tag
            || binding.node.cui_instance_identity
                   != evidence.visible_resource.post.cui_instance_identity) {
            return VariantClosureResult::invalid;
        }
        title = title || binding.semantic == NodeSemantic::transient_title;
        body = body || binding.semantic == NodeSemantic::transient_body;
    }
    if (!title || !body) {
        return VariantClosureResult::incomplete_title_or_body;
    }
    return evidence.visible_resource.observed_branch == ObjectiveResourceBranch::branch_a
               ? VariantClosureResult::closed_branch_a
               : VariantClosureResult::closed_branch_b;
}

TimelineResult assess_retail_timeline(const PresentationInterval& missionHeader,
                                      const PresentationInterval& objective,
                                      const PresentationInterval& ghost,
                                      const PresentationInterval& persistentTracker) noexcept {
    if (!valid_interval(missionHeader) || !valid_interval(objective) || !valid_interval(ghost)
        || !valid_interval(persistentTracker)
        || missionHeader.lane != PresentationLane::mission_header_shared_banner
        || objective.lane != PresentationLane::objective_shared_banner
        || ghost.lane != PresentationLane::ghost_dialogue_independent
        || persistentTracker.lane != PresentationLane::persistent_objective_display) {
        return TimelineResult::invalid;
    }
    if (overlaps(missionHeader, objective)
        || missionHeader.queue_sequence == objective.queue_sequence) {
        return TimelineResult::shared_banner_overlap_invalid;
    }
    return overlaps(ghost, objective) || overlaps(ghost, persistentTracker)
               ? TimelineResult::serialized_with_ghost_overlap
               : TimelineResult::serialized_without_measured_ghost_overlap;
}

bool valid_capture_record(const CaptureRecord& record) noexcept {
    if (!valid_capture_header(record.header)) {
        return false;
    }
    switch (record.kind) {
    case CaptureKind::type68_authority:
        return record.header.phase == CapturePhase::type68_apply
               && valid_type68_authority(record.authority);
    case CaptureKind::formatter:
        return record.header.phase == CapturePhase::type68_format
               && valid_formatter_observation(record.formatter);
    case CaptureKind::objective_model:
        return record.header.phase == CapturePhase::objective_materialize
               && valid_objective_model(record.formatter, record.model);
    case CaptureKind::queue:
        return record.header.phase == queue_phase(record.queue.event)
               && valid_queue_evidence(record.queue);
    case CaptureKind::manager:
        return record.header.phase == manager_phase(record.manager.event)
               && valid_manager_evidence(record.manager);
    case CaptureKind::resource_node:
        return record.header.phase == CapturePhase::instantiated_resource
               && valid_visible_variant(record.resource);
    case CaptureKind::node_binding:
        return record.header.phase == CapturePhase::provider_node_binding
               && valid_node_binding(record.binding, record.binding_payload);
    case CaptureKind::mission_header_node:
        return record.header.phase == CapturePhase::provider_node_binding
               && valid_mission_header_node(record.mission_node);
    case CaptureKind::timeline:
        return record.header.phase == CapturePhase::presentation_timeline
               && assess_retail_timeline(record.mission_header_interval,
                                         record.objective_interval,
                                         record.ghost_interval,
                                         record.tracker_interval)
                      != TimelineResult::invalid
               && assess_retail_timeline(record.mission_header_interval,
                                         record.objective_interval,
                                         record.ghost_interval,
                                         record.tracker_interval)
                      != TimelineResult::shared_banner_overlap_invalid;
    }
    return false;
}

bool default_telemetry(const CaptureRecord& record, ScalarHashTelemetry& output) noexcept {
    if (!valid_capture_record(record)) {
        return false;
    }
    ScalarHashTelemetry result{};
    result.sequence = record.sequence;
    result.call_id = record.header.context.call_id;
    result.monotonic_tick = record.header.context.monotonic_tick;
    result.capture_epoch = record.header.context.capture_epoch;
    result.phase = record.header.phase;
    result.kind = record.kind;
    switch (record.kind) {
    case CaptureKind::type68_authority:
        result.payload_or_resource_hash = record.authority.decoded_body_hash;
        result.outcome = static_cast<std::uint8_t>(record.authority.decoded_lifecycle + 1);
        break;
    case CaptureKind::formatter:
        result.payload_or_resource_hash = record.formatter.exact_manager_event_hash;
        result.command = record.formatter.hud_identity;
        result.outcome = record.formatter.mode;
        break;
    case CaptureKind::objective_model:
        result.payload_or_resource_hash = record.model.exact_model_hash;
        result.outcome = record.model.mode;
        break;
    case CaptureKind::queue:
        result.pre_hash = record.queue.pre.exact_state_hash;
        result.post_hash = record.queue.post.exact_state_hash;
        result.command = record.queue.command;
        result.payload_or_resource_hash = record.queue.event == QueueEventKind::command5_insert
                                              ? record.queue.command5_payload.exact_payload_hash
                                              : record.queue.objective_payload.exact_payload_hash;
        result.outcome = static_cast<std::uint8_t>(record.queue.disposition);
        break;
    case CaptureKind::manager:
        result.pre_hash = record.manager.pre.exact_state_hash;
        result.post_hash = record.manager.post.exact_state_hash;
        result.outcome = static_cast<std::uint8_t>(record.manager.event);
        break;
    case CaptureKind::resource_node:
        result.pre_hash = record.resource.pre.exact_node_hash;
        result.post_hash = record.resource.post.exact_node_hash;
        result.payload_or_resource_hash = record.resource.post.exact_resource_hash;
        result.command = static_cast<std::uint32_t>(record.resource.post.queue_command);
        result.outcome = static_cast<std::uint8_t>(record.resource.observed_branch);
        break;
    case CaptureKind::node_binding:
        result.post_hash = record.binding.node.exact_node_hash;
        result.payload_or_resource_hash = record.binding.resolved_text_hash;
        result.command = static_cast<std::uint32_t>(record.binding.node.queue_command);
        result.outcome = static_cast<std::uint8_t>(record.binding.semantic);
        break;
    case CaptureKind::mission_header_node:
        result.post_hash = record.mission_node.node.exact_node_hash;
        result.payload_or_resource_hash = record.mission_node.resolved_text_hash;
        result.command = kMissionHeaderCommand;
        result.outcome = static_cast<std::uint8_t>(record.mission_node.semantic);
        break;
    case CaptureKind::timeline:
        result.pre_hash = record.mission_header_interval.first_tick;
        result.post_hash = record.objective_interval.last_tick;
        result.payload_or_resource_hash = record.ghost_interval.first_tick;
        result.outcome = static_cast<std::uint8_t>(assess_retail_timeline(
            record.mission_header_interval,
            record.objective_interval,
            record.ghost_interval,
            record.tracker_interval));
        break;
    }
    output = result;
    return true;
}

ParticipantContract participant_contract(ParticipantKind kind) noexcept {
    NativeSurface surface = NativeSurface::instantiated_cui_node;
    bool pre = true;
    bool post = true;
    switch (kind) {
    case ParticipantKind::command5_insert:
        surface = NativeSurface::command5_insert;
        break;
    case ParticipantKind::type68_apply:
        surface = NativeSurface::type68_apply;
        break;
    case ParticipantKind::type68_formatter:
        surface = NativeSurface::type68_formatter;
        break;
    case ParticipantKind::objective_materializer:
        surface = NativeSurface::objective_model_materializer;
        break;
    case ParticipantKind::objective_payload_builder:
        surface = NativeSurface::objective_payload_builder;
        break;
    case ParticipantKind::objective_insert:
        surface = NativeSurface::objective_insert;
        break;
    case ParticipantKind::queue_tick:
        surface = NativeSurface::queue_tick;
        break;
    case ParticipantKind::queue_erase:
        surface = NativeSurface::queue_erase;
        break;
    case ParticipantKind::queue_reset:
        surface = NativeSurface::queue_reset;
        break;
    case ParticipantKind::queue_teardown:
        surface = NativeSurface::queue_teardown;
        break;
    case ParticipantKind::dispatch_loader:
        surface = NativeSurface::dispatch_loader;
        break;
    case ParticipantKind::manager_terminal:
        surface = NativeSurface::manager_terminal_predicate;
        break;
    case ParticipantKind::cui_node_observer:
        surface = NativeSurface::instantiated_cui_node;
        pre = false;
        break;
    }
    return {kind, surface, true, 1U, pre, post, false, false};
}

OriginalOnceParticipant::CallScope::CallScope(OriginalOnceParticipant& owner) noexcept
    : owner_(owner) {
    if (owner_.phase_.load(std::memory_order_acquire) == ParticipantPhase::detached) {
        return;
    }
    owner_.calls_in_flight_.fetch_add(1U, std::memory_order_acq_rel);
    owns_call_ = true;
    if (owner_.phase_.load(std::memory_order_acquire) == ParticipantPhase::active
        && observation_owner_ != &owner_) {
        previous_observation_owner_ = observation_owner_;
        observation_owner_ = &owner_;
        observes_ = true;
    }
}

OriginalOnceParticipant::CallScope::~CallScope() {
    if (observes_) {
        observation_owner_ = previous_observation_owner_;
    }
    if (owns_call_) {
        owner_.calls_in_flight_.fetch_sub(1U, std::memory_order_acq_rel);
    }
}

bool OriginalOnceParticipant::activate(std::uint64_t admissionGeneration,
                                       std::uint64_t captureEpoch) noexcept {
    if (admissionGeneration == 0U || captureEpoch == 0U) {
        return false;
    }
    ParticipantPhase expected = ParticipantPhase::detached;
    if (!phase_.compare_exchange_strong(expected,
                                        ParticipantPhase::activating,
                                        std::memory_order_acq_rel,
                                        std::memory_order_acquire)) {
        return false;
    }
    admission_generation_.store(admissionGeneration, std::memory_order_release);
    capture_epoch_.store(captureEpoch, std::memory_order_release);
    phase_.store(ParticipantPhase::active, std::memory_order_release);
    return true;
}

bool OriginalOnceParticipant::begin_quiesce() noexcept {
    ParticipantPhase expected = ParticipantPhase::active;
    return phase_.compare_exchange_strong(expected,
                                          ParticipantPhase::quiescing,
                                          std::memory_order_acq_rel,
                                          std::memory_order_acquire);
}

bool OriginalOnceParticipant::confirm_removed(bool aggregateDetachConfirmed) noexcept {
    if (!aggregateDetachConfirmed
        || calls_in_flight_.load(std::memory_order_acquire) != 0U) {
        return false;
    }
    ParticipantPhase expected = ParticipantPhase::quiescing;
    return phase_.compare_exchange_strong(expected,
                                          ParticipantPhase::removed_pending_reset,
                                          std::memory_order_acq_rel,
                                          std::memory_order_acquire);
}

bool OriginalOnceParticipant::finalize_reset(
    bool persistenceAndQueueResetComplete) noexcept {
    if (!persistenceAndQueueResetComplete
        || calls_in_flight_.load(std::memory_order_acquire) != 0U) {
        return false;
    }
    ParticipantPhase expected = ParticipantPhase::removed_pending_reset;
    if (!phase_.compare_exchange_strong(expected,
                                        ParticipantPhase::detached,
                                        std::memory_order_acq_rel,
                                        std::memory_order_acquire)) {
        return false;
    }
    admission_generation_.store(0U, std::memory_order_release);
    capture_epoch_.store(0U, std::memory_order_release);
    return true;
}

ParticipantSnapshot OriginalOnceParticipant::snapshot() const noexcept {
    return {phase_.load(std::memory_order_acquire),
            kind_,
            admission_generation_.load(std::memory_order_acquire),
            capture_epoch_.load(std::memory_order_acquire),
            calls_in_flight_.load(std::memory_order_acquire)};
}

} // namespace sunrise::client::hooks::bootflow::opening_authority::
  // objective_banner_variant_capture
