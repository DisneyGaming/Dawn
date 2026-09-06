#include "client/hooks/bootflow/opening_authority/ikora_ground_dwell_capture.h"

#include <algorithm>
#include <bit>
#include <cmath>

namespace sunrise::client::hooks::bootflow::opening_authority::ikora_ground_dwell {
namespace {

using Prefix32 = std::array<std::byte, 32U>;

constexpr Prefix32 kSceneApplyPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24}, std::byte{0x08},
    std::byte{0x57}, std::byte{0x48}, std::byte{0x83}, std::byte{0xEC}, std::byte{0x30},
    std::byte{0x48}, std::byte{0x8B}, std::byte{0xFA}, std::byte{0x48}, std::byte{0x8B},
    std::byte{0xD9}, std::byte{0xE8}, std::byte{0x8B}, std::byte{0x61}, std::byte{0x9A},
    std::byte{0xFF}, std::byte{0x84}, std::byte{0xC0}, std::byte{0x74}, std::byte{0x3A},
    std::byte{0x48}, std::byte{0x8B}, std::byte{0x05}, std::byte{0x20}, std::byte{0xC9},
    std::byte{0x52}, std::byte{0x01}};

constexpr Prefix32 kSceneReconcilePrefix{
    std::byte{0x40}, std::byte{0x57}, std::byte{0x48}, std::byte{0x83}, std::byte{0xEC},
    std::byte{0x30}, std::byte{0x48}, std::byte{0x8B}, std::byte{0xF9}, std::byte{0xE8},
    std::byte{0x32}, std::byte{0x6C}, std::byte{0x9A}, std::byte{0xFF}, std::byte{0x84},
    std::byte{0xC0}, std::byte{0x0F}, std::byte{0x84}, std::byte{0x93}, std::byte{0x01},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x48}, std::byte{0x89}, std::byte{0x6C},
    std::byte{0x24}, std::byte{0x28}, std::byte{0x48}, std::byte{0x8B}, std::byte{0xCF},
    std::byte{0x48}, std::byte{0x89}};

constexpr Prefix32 kSceneStartPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24}, std::byte{0x10},
    std::byte{0x48}, std::byte{0x89}, std::byte{0x6C}, std::byte{0x24}, std::byte{0x18},
    std::byte{0x56}, std::byte{0x57}, std::byte{0x41}, std::byte{0x56}, std::byte{0x48},
    std::byte{0x81}, std::byte{0xEC}, std::byte{0xB0}, std::byte{0x08}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x48}, std::byte{0x8B}, std::byte{0x05}, std::byte{0x4C},
    std::byte{0x68}, std::byte{0x56}, std::byte{0x01}, std::byte{0x48}, std::byte{0x33},
    std::byte{0xC4}, std::byte{0x48}};

constexpr Prefix32 kActorMaterializePrefix{
    std::byte{0x4C}, std::byte{0x8B}, std::byte{0xDC}, std::byte{0x55}, std::byte{0x56},
    std::byte{0x41}, std::byte{0x55}, std::byte{0x41}, std::byte{0x57}, std::byte{0x49},
    std::byte{0x8D}, std::byte{0xAB}, std::byte{0xE8}, std::byte{0xF7}, std::byte{0xFF},
    std::byte{0xFF}, std::byte{0x48}, std::byte{0x81}, std::byte{0xEC}, std::byte{0xF8},
    std::byte{0x08}, std::byte{0x00}, std::byte{0x00}, std::byte{0x48}, std::byte{0x8B},
    std::byte{0x05}, std::byte{0xAA}, std::byte{0x97}, std::byte{0xB1}, std::byte{0x01},
    std::byte{0x48}, std::byte{0x33}};

constexpr std::array<std::byte, 31U> kFactoryThunkPrefix{
    std::byte{0x40}, std::byte{0x53}, std::byte{0x48}, std::byte{0x83}, std::byte{0xEC},
    std::byte{0x20}, std::byte{0x41}, std::byte{0x83}, std::byte{0xC9}, std::byte{0xFF},
    std::byte{0x41}, std::byte{0x83}, std::byte{0xC8}, std::byte{0xFF}, std::byte{0x48},
    std::byte{0x8B}, std::byte{0xD9}, std::byte{0xE8}, std::byte{0x0A}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x48}, std::byte{0x8B}, std::byte{0xC3},
    std::byte{0x48}, std::byte{0x83}, std::byte{0xC4}, std::byte{0x20}, std::byte{0x5B},
    std::byte{0xC3}};

constexpr Prefix32 kFactoryPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24}, std::byte{0x18},
    std::byte{0x48}, std::byte{0x89}, std::byte{0x6C}, std::byte{0x24}, std::byte{0x20},
    std::byte{0x56}, std::byte{0x57}, std::byte{0x41}, std::byte{0x56}, std::byte{0x48},
    std::byte{0x81}, std::byte{0xEC}, std::byte{0x40}, std::byte{0x01}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x48}, std::byte{0x8B}, std::byte{0x05}, std::byte{0xBC},
    std::byte{0xC0}, std::byte{0xB3}, std::byte{0x01}, std::byte{0x48}, std::byte{0x33},
    std::byte{0xC4}, std::byte{0x48}};

constexpr Prefix32 kActorTickPrefix{
    std::byte{0x48}, std::byte{0x8B}, std::byte{0xC4}, std::byte{0x55}, std::byte{0x41},
    std::byte{0x54}, std::byte{0x41}, std::byte{0x55}, std::byte{0x41}, std::byte{0x56},
    std::byte{0x48}, std::byte{0x83}, std::byte{0xEC}, std::byte{0x78}, std::byte{0x45},
    std::byte{0x33}, std::byte{0xE4}, std::byte{0x0F}, std::byte{0x29}, std::byte{0x70},
    std::byte{0xC8}, std::byte{0x66}, std::byte{0x44}, std::byte{0x89}, std::byte{0xA1},
    std::byte{0x12}, std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x0F},
    std::byte{0x28}, std::byte{0xF3}};

constexpr Prefix32 kActorTerminalPrefix{
    std::byte{0x40}, std::byte{0x57}, std::byte{0x48}, std::byte{0x83}, std::byte{0xEC},
    std::byte{0x40}, std::byte{0x48}, std::byte{0x8B}, std::byte{0xF9}, std::byte{0x80},
    std::byte{0xFA}, std::byte{0xFF}, std::byte{0x0F}, std::byte{0x84}, std::byte{0x2F},
    std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x48}, std::byte{0x89},
    std::byte{0x5C}, std::byte{0x24}, std::byte{0x50}, std::byte{0x33}, std::byte{0xDB},
    std::byte{0x45}, std::byte{0x84}, std::byte{0xC0}, std::byte{0x75}, std::byte{0x36},
    std::byte{0x45}, std::byte{0x84}};

constexpr Prefix32 kType31ApplyPrefix{
    std::byte{0x40}, std::byte{0x53}, std::byte{0x48}, std::byte{0x83}, std::byte{0xEC},
    std::byte{0x30}, std::byte{0x44}, std::byte{0x8B}, std::byte{0x02}, std::byte{0x48},
    std::byte{0x8B}, std::byte{0xD9}, std::byte{0x4C}, std::byte{0x8B}, std::byte{0x4A},
    std::byte{0x08}, std::byte{0x48}, std::byte{0x8D}, std::byte{0x4C}, std::byte{0x24},
    std::byte{0x20}, std::byte{0x48}, std::byte{0x8B}, std::byte{0x05}, std::byte{0xFC},
    std::byte{0x22}, std::byte{0x48}, std::byte{0x01}, std::byte{0x8B}, std::byte{0x10},
    std::byte{0x44}, std::byte{0x89}};

constexpr Prefix32 kType31PredicatePrefix{
    std::byte{0x40}, std::byte{0x55}, std::byte{0x57}, std::byte{0x48}, std::byte{0x8B},
    std::byte{0xEC}, std::byte{0x48}, std::byte{0x83}, std::byte{0xEC}, std::byte{0x78},
    std::byte{0x80}, std::byte{0xB9}, std::byte{0x88}, std::byte{0x01}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x48}, std::byte{0x8B}, std::byte{0xF9},
    std::byte{0x0F}, std::byte{0x84}, std::byte{0x25}, std::byte{0x03}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x48}, std::byte{0x8B}, std::byte{0x89}, std::byte{0x90},
    std::byte{0x01}, std::byte{0x00}};

constexpr Prefix32 kType31TerminalPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x74}, std::byte{0x24}, std::byte{0x20},
    std::byte{0x57}, std::byte{0x48}, std::byte{0x81}, std::byte{0xEC}, std::byte{0xE0},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x48}, std::byte{0x8B},
    std::byte{0x05}, std::byte{0x54}, std::byte{0x92}, std::byte{0x58}, std::byte{0x01},
    std::byte{0x48}, std::byte{0x33}, std::byte{0xC4}, std::byte{0x48}, std::byte{0x89},
    std::byte{0x84}, std::byte{0x24}, std::byte{0xD0}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x48}};

constexpr Prefix32 kDynamicListenerPrefix{
    std::byte{0x40}, std::byte{0x55}, std::byte{0x53}, std::byte{0x57}, std::byte{0x41},
    std::byte{0x55}, std::byte{0x41}, std::byte{0x56}, std::byte{0x48}, std::byte{0x8D},
    std::byte{0xAC}, std::byte{0x24}, std::byte{0x50}, std::byte{0xFC}, std::byte{0xFF},
    std::byte{0xFF}, std::byte{0x48}, std::byte{0x81}, std::byte{0xEC}, std::byte{0xB0},
    std::byte{0x04}, std::byte{0x00}, std::byte{0x00}, std::byte{0x48}, std::byte{0x8B},
    std::byte{0x05}, std::byte{0x0A}, std::byte{0x6F}, std::byte{0x32}, std::byte{0x01},
    std::byte{0x48}, std::byte{0x33}};

constexpr Sha256 kSceneApplyHash{
    std::byte{0x91}, std::byte{0x33}, std::byte{0x0D}, std::byte{0x15}, std::byte{0x5F},
    std::byte{0x8F}, std::byte{0xE2}, std::byte{0xF3}, std::byte{0xEA}, std::byte{0x4D},
    std::byte{0xC5}, std::byte{0x11}, std::byte{0xD7}, std::byte{0xAA}, std::byte{0x44},
    std::byte{0x53}, std::byte{0x7F}, std::byte{0xEA}, std::byte{0x5F}, std::byte{0x4E},
    std::byte{0x58}, std::byte{0xEF}, std::byte{0x92}, std::byte{0xE8}, std::byte{0x4E},
    std::byte{0xCD}, std::byte{0xCA}, std::byte{0x64}, std::byte{0x1B}, std::byte{0x4A},
    std::byte{0x7E}, std::byte{0x39}};
constexpr Sha256 kSceneReconcileHash{
    std::byte{0x17}, std::byte{0x67}, std::byte{0x9B}, std::byte{0x6C}, std::byte{0x99},
    std::byte{0xF7}, std::byte{0x2A}, std::byte{0x81}, std::byte{0x38}, std::byte{0x51},
    std::byte{0xF3}, std::byte{0xC6}, std::byte{0x30}, std::byte{0x69}, std::byte{0x0F},
    std::byte{0xEC}, std::byte{0x0C}, std::byte{0x7D}, std::byte{0x72}, std::byte{0x5E},
    std::byte{0xD0}, std::byte{0xA1}, std::byte{0xCA}, std::byte{0xB7}, std::byte{0x95},
    std::byte{0x7E}, std::byte{0x9C}, std::byte{0x31}, std::byte{0x06}, std::byte{0x49},
    std::byte{0x1C}, std::byte{0x5B}};
constexpr Sha256 kSceneStartHash{
    std::byte{0x8C}, std::byte{0xF7}, std::byte{0x4A}, std::byte{0x90}, std::byte{0x05},
    std::byte{0x5B}, std::byte{0x26}, std::byte{0x4E}, std::byte{0x29}, std::byte{0x87},
    std::byte{0x12}, std::byte{0xF6}, std::byte{0x29}, std::byte{0x40}, std::byte{0x7D},
    std::byte{0x36}, std::byte{0x0E}, std::byte{0x66}, std::byte{0x51}, std::byte{0x72},
    std::byte{0xE5}, std::byte{0x58}, std::byte{0xDE}, std::byte{0xF6}, std::byte{0x31},
    std::byte{0x90}, std::byte{0x8A}, std::byte{0x80}, std::byte{0xE9}, std::byte{0x82},
    std::byte{0x0D}, std::byte{0x33}};
constexpr Sha256 kActorMaterializeHash{
    std::byte{0x19}, std::byte{0x4F}, std::byte{0x23}, std::byte{0xAF}, std::byte{0x75},
    std::byte{0x29}, std::byte{0x75}, std::byte{0x03}, std::byte{0xD9}, std::byte{0x54},
    std::byte{0xA2}, std::byte{0x1C}, std::byte{0xBD}, std::byte{0x62}, std::byte{0x71},
    std::byte{0x87}, std::byte{0xC2}, std::byte{0x94}, std::byte{0xC4}, std::byte{0x49},
    std::byte{0x42}, std::byte{0xF7}, std::byte{0xF3}, std::byte{0x7F}, std::byte{0x21},
    std::byte{0x1A}, std::byte{0x2E}, std::byte{0xEA}, std::byte{0x5A}, std::byte{0xBA},
    std::byte{0xE5}, std::byte{0x95}};
constexpr Sha256 kFactoryThunkHash{
    std::byte{0x74}, std::byte{0x62}, std::byte{0x90}, std::byte{0x6C}, std::byte{0xAA},
    std::byte{0xFE}, std::byte{0xBF}, std::byte{0x3B}, std::byte{0x88}, std::byte{0xA6},
    std::byte{0xC6}, std::byte{0xEA}, std::byte{0x00}, std::byte{0x16}, std::byte{0x11},
    std::byte{0x34}, std::byte{0x3F}, std::byte{0x4A}, std::byte{0xAB}, std::byte{0x4C},
    std::byte{0xB3}, std::byte{0x03}, std::byte{0x88}, std::byte{0xAA}, std::byte{0xB4},
    std::byte{0x05}, std::byte{0x27}, std::byte{0x88}, std::byte{0x00}, std::byte{0x70},
    std::byte{0x0A}, std::byte{0x48}};
constexpr Sha256 kFactoryHash{std::byte{0x61}, std::byte{0xFD}, std::byte{0x5D}, std::byte{0x85},
                              std::byte{0xE9}, std::byte{0xBB}, std::byte{0x99}, std::byte{0xD1},
                              std::byte{0xE5}, std::byte{0xC9}, std::byte{0x76}, std::byte{0xC7},
                              std::byte{0x4F}, std::byte{0x65}, std::byte{0x94}, std::byte{0x06},
                              std::byte{0xB9}, std::byte{0xCC}, std::byte{0xF6}, std::byte{0x05},
                              std::byte{0x29}, std::byte{0x8A}, std::byte{0xA8}, std::byte{0xC7},
                              std::byte{0x96}, std::byte{0xE0}, std::byte{0x7B}, std::byte{0x1C},
                              std::byte{0x19}, std::byte{0x3D}, std::byte{0x20}, std::byte{0x93}};
constexpr Sha256 kActorTickHash{std::byte{0x43}, std::byte{0x55}, std::byte{0x86}, std::byte{0xE0},
                                std::byte{0xDF}, std::byte{0x8E}, std::byte{0xD4}, std::byte{0xCC},
                                std::byte{0x00}, std::byte{0x61}, std::byte{0xA2}, std::byte{0xD5},
                                std::byte{0x2F}, std::byte{0xC1}, std::byte{0x84}, std::byte{0x8B},
                                std::byte{0xBE}, std::byte{0x81}, std::byte{0xD2}, std::byte{0x6A},
                                std::byte{0x45}, std::byte{0xD3}, std::byte{0x5D}, std::byte{0x65},
                                std::byte{0x51}, std::byte{0xF5}, std::byte{0xDD}, std::byte{0x84},
                                std::byte{0xE0}, std::byte{0xF9}, std::byte{0xFE}, std::byte{0x0E}};
constexpr Sha256 kActorTerminalHash{
    std::byte{0x81}, std::byte{0x93}, std::byte{0x40}, std::byte{0x9D}, std::byte{0xCD},
    std::byte{0xE2}, std::byte{0x5B}, std::byte{0x37}, std::byte{0xFF}, std::byte{0xDB},
    std::byte{0x3D}, std::byte{0x10}, std::byte{0x07}, std::byte{0xCE}, std::byte{0x71},
    std::byte{0x6F}, std::byte{0xE2}, std::byte{0x61}, std::byte{0x0F}, std::byte{0x5A},
    std::byte{0x50}, std::byte{0x90}, std::byte{0x31}, std::byte{0xED}, std::byte{0xEA},
    std::byte{0x81}, std::byte{0xDC}, std::byte{0xBF}, std::byte{0xC9}, std::byte{0xE7},
    std::byte{0x7E}, std::byte{0xCD}};
constexpr Sha256 kNoRecoveredHash{};

[[nodiscard]] bool
rva_in_range(std::uintptr_t rva, std::uint32_t begin, std::uint32_t end) noexcept {
    return rva >= begin && rva < end;
}

[[nodiscard]] bool nonzero_actor_storage(const ActorIdentity& actor) noexcept {
    return actor.scheduler_tag != 0U || actor.entity_definition != 0U
           || actor.packed_full_handle != 0U || actor.packed_low_index != 0U
           || actor.object_record_identity != 0U || actor.object_record_generation != 0U
           || actor.actor_component_identity != 0U || actor.actor_component_generation != 0U
           || actor.authority_generation != 0U || actor.factory_generation != 0U;
}

[[nodiscard]] bool same_actor_generation(const ActorIdentity& left,
                                         const ActorIdentity& right) noexcept {
    return left == right;
}

} // namespace

NativeTarget native_target(NativeSurface surface) noexcept {
    switch (surface) {
    case NativeSurface::scene_authority_apply:
        return {0xB41DD0U,
                0x5EU,
                kSceneApplyPrefix,
                kSceneApplyHash,
                NativeAbi::scene_component_state_key,
                true};
    case NativeSurface::scene_reconcile_commit:
        return {0xB41330U,
                0x1AFU,
                kSceneReconcilePrefix,
                kSceneReconcileHash,
                NativeAbi::scene_component_only,
                true};
    case NativeSurface::scene_root_advance_start:
        return {0xB43220U,
                0x284U,
                kSceneStartPrefix,
                kSceneStartHash,
                NativeAbi::scene_component_effective_root,
                true};
    case NativeSurface::actor_slot_scheduler:
        return {0x5902C0U,
                0x2E8U,
                kActorMaterializePrefix,
                kActorMaterializeHash,
                NativeAbi::actor_scheduler_only,
                true};
    case NativeSurface::entity_factory_thunk:
        return {0x56D990U,
                0x1FU,
                kFactoryThunkPrefix,
                kFactoryThunkHash,
                NativeAbi::factory_thunk,
                true};
    case NativeSurface::entity_factory:
        return {0x56D9B0U, 0x448U, kFactoryPrefix, kFactoryHash, NativeAbi::generic_factory, true};
    case NativeSurface::actor_local_transition_tick:
        return {0x58FFD0U,
                0x2EDU,
                kActorTickPrefix,
                kActorTickHash,
                NativeAbi::actor_scheduler_definition_context_delta,
                true};
    case NativeSurface::actor_transition_terminal:
        return {0x58B9A0U,
                0x1B0U,
                kActorTerminalPrefix,
                kActorTerminalHash,
                NativeAbi::actor_scheduler_transition_flags,
                true};
    case NativeSurface::type31_authority_apply:
        return {0xB20640U,
                0x4AU,
                kType31ApplyPrefix,
                kNoRecoveredHash,
                NativeAbi::type31_instance_state_key,
                false};
    case NativeSurface::type31_predicate:
        return {0xB20B00U,
                0x346U,
                kType31PredicatePrefix,
                kNoRecoveredHash,
                NativeAbi::type31_instance_only,
                false};
    case NativeSurface::type31_terminal:
        return {0xB20820U,
                0x1B5U,
                kType31TerminalPrefix,
                kNoRecoveredHash,
                NativeAbi::type31_instance_object_reference,
                false};
    case NativeSurface::incident_dynamic_listener_enumeration:
        return {0xD82B60U,
                0x2FCU,
                kDynamicListenerPrefix,
                kNoRecoveredHash,
                NativeAbi::internal_dynamic_listener_enumerator,
                false};
    case NativeSurface::count:
        break;
    }
    return {};
}

bool native_prefix_matches(NativeSurface surface, std::span<const std::byte> observed) noexcept {
    const NativeTarget target = native_target(surface);
    return target.mapped_rva != 0U && observed.size() == target.mapped_prefix.size()
           && std::equal(observed.begin(), observed.end(), target.mapped_prefix.begin());
}

RuntimeAdmissionResult
validate_runtime_admission(const RuntimeAdmissionEvidence& evidence) noexcept {
    if (!matches_pinned_packed_runtime(evidence.packed_runtime)) {
        return RuntimeAdmissionResult::packed_identity_mismatch;
    }
    if (evidence.mapped_image_base == 0U || (evidence.mapped_image_base & 0xFFFU) != 0U
        || evidence.mapped_image_bytes != kPinnedSizeOfImage
        || evidence.pe_timestamp != kPinnedPeTimestamp || evidence.entry_rva != kPinnedEntryRva
        || evidence.machine != kPinnedMachine || evidence.section_count != kPinnedSectionCount) {
        return RuntimeAdmissionResult::mapped_pe_mismatch;
    }
    if (evidence.executable_rva_begin == 0U
        || evidence.executable_rva_begin >= evidence.executable_rva_end
        || evidence.executable_rva_end > evidence.mapped_image_bytes) {
        return RuntimeAdmissionResult::invalid_executable_range;
    }
    for (std::size_t index = 0U; index < kNativeSurfaceCount; ++index) {
        const NativeSurface surface = static_cast<NativeSurface>(index);
        const NativeTarget target = native_target(surface);
        if (target.mapped_rva < evidence.executable_rva_begin
            || target.mapped_rva > evidence.executable_rva_end
            || target.recovered_function_bytes > evidence.executable_rva_end - target.mapped_rva) {
            return RuntimeAdmissionResult::target_out_of_range;
        }
        const MappedPrefixObservation& observed = evidence.mapped_prefixes[index];
        if (observed.byte_count != target.mapped_prefix.size()
            || !native_prefix_matches(
                surface, std::span<const std::byte>{observed.bytes.data(), observed.byte_count})) {
            return RuntimeAdmissionResult::prefix_mismatch;
        }
    }
    return RuntimeAdmissionResult::admitted;
}

bool exact_root_transform(const RootTransform& transform) noexcept {
    return std::bit_cast<std::array<std::uint32_t, 7U>>(transform)
           == std::bit_cast<std::array<std::uint32_t, 7U>>(kExactSharedRootTransform);
}

std::uint64_t transform_hash(const RootTransform& transform) noexcept {
    const auto bytes = std::bit_cast<std::array<std::byte, sizeof(RootTransform)>>(transform);
    return bounded_hash(bytes);
}

bool valid_lineage(const CaptureLineage& lineage) noexcept {
    return lineage.capture_epoch != 0U && lineage.session_id != 0U && lineage.activity_id != 0U
           && lineage.activity_activation_generation != 0U && lineage.region_id != 0U
           && lineage.region_generation != 0U && lineage.wipe_replay_generation != 0U
           && lineage.roster_record_identity != 0U && lineage.roster_record_generation != 0U
           && lineage.scene_component_identity != 0U && lineage.scene_component_generation != 0U
           && lineage.authority_generation != 0U && lineage.publication_epoch != 0U;
}

bool valid_scene_authority(const SceneAuthorityObservation& authority,
                           const CaptureLineage& lineage) noexcept {
    if (authority.scene != kSceneReference || authority.schema != kSceneAuthoritySchema
        || authority.definition != kSceneDefinition
        || authority.authority_generation != lineage.authority_generation
        || authority.publication_epoch != lineage.publication_epoch
        || authority.publication_serial == 0U || authority.body_hash == 0U
        || authority.decoded_hash == 0U || authority.entry_count > kSceneEntryMaximum
        || authority.word_count > kSceneWordMaximum || authority.authority_active > 1U
        || authority.terminal_latch > 1U || authority.live_scene_full_handle == 0U
        || authority.live_scene_handle_generation == 0U || !authority.native_decode_succeeded
        || !authority.private_encode_deterministic || !authority.significant_bits_round_trip) {
        return false;
    }
    const std::size_t expectedBits =
        kSceneAuthorityMinimumBits + 55U * authority.entry_count + 32U * authority.word_count;
    return authority.significant_bits == expectedBits
           && authority.significant_bits <= kSceneAuthorityMaximumBits
           && authority.effective_root > authority.committed_root;
}

bool valid_actor_identity(const ActorIdentity& actor) noexcept {
    return actor.scheduler_tag != 0U && actor.entity_definition == kSharedActorEntity
           && actor.packed_full_handle != 0U
           && actor.packed_low_index == packed_low_index(actor.packed_full_handle)
           && actor.object_record_identity != 0U && actor.object_record_generation != 0U
           && actor.actor_component_identity != 0U && actor.actor_component_generation != 0U
           && actor.authority_generation != 0U && actor.factory_generation != 0U;
}

bool valid_actor_observation(const ActorObservation& actor) noexcept {
    if (!valid_actor_identity(actor.identity) || actor.source == ActorSource::unknown
        || actor.created_tick == 0U || actor.created_serial == 0U || actor.descriptor_hash == 0U
        || actor.descriptor_bytes == 0U || actor.source_row_identity == 0U
        || actor.source_row_generation == 0U || !exact_root_transform(actor.root)
        || (actor.provider_full_handle == 0U) != (actor.provider_generation == 0U)) {
        return false;
    }
    const bool terminalTickPresent = actor.terminal_tick != 0U;
    const bool terminalSerialPresent = actor.terminal_serial != 0U;
    if (terminalTickPresent != terminalSerialPresent) {
        return false;
    }
    if (actor.terminal_tick != 0U
        && (actor.terminal_tick < actor.created_tick
            || actor.terminal_serial <= actor.created_serial)) {
        return false;
    }
    if (actor.source == ActorSource::scene) {
        return actor.spawner_key == kSceneSpawnerKey;
    }
    return actor.spawner_key != kSceneSpawnerKey;
}

bool valid_cast_binding(const CastBindingObservation& cast) noexcept {
    return cast.actor_cast_class == kActorCastClass && cast.actor_binding_id == kActorCastBinding
           && cast.actor_source == kSquadReference
           && cast.actor_source_definition == kSquadDefinition
           && cast.actor_source_kind == kSquadKind
           && cast.actor_entity_definition == kSharedActorEntity
           && cast.timeline_cast_class == kTimelineCastClass
           && cast.timeline_binding_id == kTimelineCastBinding
           && cast.timeline_source == kTimelineReference
           && cast.timeline_definition == kTimelineDefinition
           && cast.timeline_placement == kTimelinePlacement
           && cast.resolved_actor_source_full_handle != 0U
           && cast.resolved_actor_source_generation != 0U
           && cast.resolved_timeline_full_handle != 0U && cast.resolved_timeline_generation != 0U
           && cast.cast_generation != 0U && exact_root_transform(cast.timeline_root);
}

bool valid_creation_edge(const ActorCreationEdge& edge,
                         std::uint64_t expectedSceneStartSerial) noexcept {
    return valid_actor_observation(edge.actor) && edge.actor.source == ActorSource::scene
           && edge.scene_start_serial == expectedSceneStartSerial
           && expectedSceneStartSerial < edge.materialize_serial
           && edge.materialize_serial < edge.factory_thunk_serial
           && edge.factory_thunk_serial < edge.factory_serial
           && edge.factory_serial == edge.actor.created_serial;
}

bool point_predicate_ready(const Type31AuthorityObservation& authority) noexcept {
    return authority.authority_generation != 0U
           && authority.significant_bits == kType31AuthorityBits && authority.body_hash != 0U
           && authority.decoded_hash != 0U && authority.native_decode_succeeded
           && authority.authority_active == 1U && authority.pending_generation != kUnsetGeneration
           && (authority.consumed_generation == kUnsetGeneration
               || authority.consumed_generation < authority.pending_generation);
}

bool valid_point_listener_correlation(const PointListenerCorrelation& correlation,
                                      const CaptureLineage& lineage,
                                      std::uint32_t executableRvaBegin,
                                      std::uint32_t executableRvaEnd) noexcept {
    const AuthorityPublicationObservation& publication = correlation.resulting_publication;
    return admitted_point(correlation.point) && point_predicate_ready(correlation.authority)
           && correlation.apply_serial != 0U
           && correlation.apply_serial < correlation.predicate_serial
           && correlation.predicate_serial < correlation.terminal_serial
           && correlation.terminal_serial < correlation.listener_enumeration_serial
           && correlation.listener_enumeration_serial < publication.publication_serial
           && correlation.local_player_full_handle != 0U
           && correlation.local_player_generation != 0U
           && correlation.volume_membership_generation != 0U
           && correlation.local_player_membership_bit && correlation.incident_root_identity != 0U
           && correlation.incident_root_generation != 0U && correlation.incident_root_hash != 0U
           && correlation.listener_row_identity != 0U && correlation.listener_row_generation != 0U
           && correlation.listener_row_hash != 0U
           && rva_in_range(correlation.listener_callback_rva, executableRvaBegin, executableRvaEnd)
           && correlation.incident_manager_generation != 0U && publication.target.registry != 0U
           && publication.target.type != 0U && publication.authority_schema != 0U
           && publication.authority_generation != 0U && publication.publication_epoch != 0U
           && publication.publication_epoch <= lineage.publication_epoch
           && publication.body_hash != 0U && publication.significant_bits != 0U
           && publication.significant_bits <= 4096U;
}

bool valid_scene_start(const SceneStartObservation& scene, const CaptureLineage& lineage) noexcept {
    if (!valid_scene_authority(scene.authority, lineage) || !valid_cast_binding(scene.cast)
        || scene.reconcile_serial <= scene.authority.publication_serial
        || scene.root_advance_start_serial <= scene.reconcile_serial
        || scene.start_effective_root != scene.authority.effective_root
        || !valid_creation_edge(scene.initial_visible, scene.root_advance_start_serial)
        || !valid_creation_edge(scene.concurrent_carrier, scene.root_advance_start_serial)) {
        return false;
    }
    const ActorObservation& initial = scene.initial_visible.actor;
    const ActorObservation& carrier = scene.concurrent_carrier.actor;
    return initial.identity.scheduler_tag == kInitialVisibleSchedulerTag
           && carrier.identity.scheduler_tag == kConcurrentCarrierSchedulerTag
           && initial.identity.authority_generation == lineage.authority_generation
           && carrier.identity.authority_generation == lineage.authority_generation
           && initial.created_tick == carrier.created_tick
           && scene.initial_visible.materialize_serial
                  == scene.concurrent_carrier.materialize_serial
           && initial.identity.packed_full_handle != carrier.identity.packed_full_handle
           && initial.identity.object_record_identity != carrier.identity.object_record_identity
           && initial.root == scene.cast.timeline_root && carrier.root == scene.cast.timeline_root;
}

bool valid_countdown_observation(const ActorCountdownObservation& countdown,
                                 std::uint32_t executableRvaBegin,
                                 std::uint32_t executableRvaEnd) noexcept {
    if (!valid_actor_identity(countdown.owner) || countdown.source_row_identity == 0U
        || countdown.source_row_generation == 0U || countdown.transition_tick_serial == 0U
        || !std::isfinite(countdown.delta_seconds) || !std::isfinite(countdown.remaining_before)
        || !std::isfinite(countdown.remaining_after) || countdown.delta_seconds < 0.0F
        || countdown.remaining_before < 0.0F || countdown.remaining_after < 0.0F
        || !rva_in_range(countdown.row_callback_rva, executableRvaBegin, executableRvaEnd)
        || !countdown.owns_local_actor_countdown || countdown.claims_ground_dwell
        || countdown.used_as_authority_gate) {
        return false;
    }
    const float expected = countdown.remaining_before > countdown.delta_seconds
                               ? countdown.remaining_before - countdown.delta_seconds
                               : 0.0F;
    if (std::fabs(expected - countdown.remaining_after) > 0.00001F) {
        return false;
    }
    return countdown.row_expired == (expected == 0.0F);
}

bool valid_actor_chronology(const ActorChronologyObservation& chronology) noexcept {
    const ActorObservation& initial = chronology.initial_visible;
    const ActorObservation& carrier = chronology.concurrent_carrier;
    const ActorObservation& successor = chronology.true_successor;
    if (!valid_actor_observation(initial) || !valid_actor_observation(carrier)
        || !valid_actor_observation(successor)
        || initial.identity.scheduler_tag != kInitialVisibleSchedulerTag
        || carrier.identity.scheduler_tag != kConcurrentCarrierSchedulerTag
        || successor.identity.scheduler_tag != kTrueSuccessorSchedulerTag
        || initial.identity.entity_definition != successor.identity.entity_definition
        || initial.identity.entity_definition != carrier.identity.entity_definition
        || chronology.initial_terminal_selector != kTerminalSelector
        || !chronology.initial_and_carrier_simultaneous
        || !chronology.successor_started_after_initial_terminal
        || !chronology.carrier_remained_live_at_successor_start
        || chronology.claims_initial_to_carrier_transfer
        || chronology.claims_low_index_continuity) {
        return false;
    }
    return initial.created_tick == carrier.created_tick && initial.terminal_tick != 0U
           && successor.created_tick > initial.terminal_tick && carrier.terminal_tick != 0U
           && carrier.terminal_tick > successor.created_tick
           && initial.identity.packed_full_handle != carrier.identity.packed_full_handle
           && initial.identity.packed_full_handle != successor.identity.packed_full_handle
           && carrier.identity.packed_full_handle != successor.identity.packed_full_handle
           && initial.identity.object_record_generation
                  != successor.identity.object_record_generation;
}

bool matches_frozen_current_chronology(const ActorChronologyObservation& chronology) noexcept {
    return valid_actor_chronology(chronology)
           && chronology.initial_visible.identity.packed_full_handle == kFrozenInitialFullHandle
           && chronology.concurrent_carrier.identity.packed_full_handle == kFrozenCarrierFullHandle
           && chronology.true_successor.identity.packed_full_handle == kFrozenSuccessorFullHandle
           && chronology.initial_visible.created_tick == kFrozenInitialAndCarrierBornTick
           && chronology.concurrent_carrier.created_tick == kFrozenInitialAndCarrierBornTick
           && chronology.initial_visible.terminal_tick == kFrozenInitialTerminalTick
           && chronology.true_successor.created_tick == kFrozenSuccessorBornTick
           && chronology.concurrent_carrier.terminal_tick == kFrozenCarrierTerminalTick
           && chronology.true_successor.created_tick - chronology.initial_visible.terminal_tick
                  == kFrozenSuccessorGapMs
           && chronology.initial_visible.identity.packed_low_index == kFrozenReusedVisibleLowIndex
           && chronology.true_successor.identity.packed_low_index == kFrozenReusedVisibleLowIndex;
}

CaptureValidationResult validate_capture_record(const GroundDwellCaptureRecord& record,
                                                std::uint32_t executableRvaBegin,
                                                std::uint32_t executableRvaEnd) noexcept {
    if (record.metadata.monotonic_tick == 0U || record.metadata.correlation_token == 0U
        || record.metadata.producer_thread_id == 0U || executableRvaBegin == 0U
        || executableRvaBegin >= executableRvaEnd) {
        return CaptureValidationResult::invalid_metadata;
    }
    if (!valid_lineage(record.lineage)) {
        return CaptureValidationResult::invalid_lineage;
    }
    if (any_forbidden_claim(record.forbidden_claims)) {
        return CaptureValidationResult::forbidden_inference;
    }
    if (record.retail_grounded.classification != GroundedIdentityClassification::unknown
        || record.retail_grounded.grade != EvidenceGrade::unknown
        || record.retail_grounded.full_identity_captured
        || nonzero_actor_storage(record.retail_grounded.actor)
        || record.retail_grounded.provider_full_handle != 0U
        || record.retail_grounded.provider_generation != 0U) {
        return CaptureValidationResult::retail_identity_overclaimed;
    }
    if (!valid_point_listener_correlation(
            record.point_listener, record.lineage, executableRvaBegin, executableRvaEnd)) {
        return CaptureValidationResult::invalid_point_listener;
    }
    if (!valid_scene_authority(record.scene_start.authority, record.lineage)) {
        return CaptureValidationResult::invalid_scene_authority;
    }
    const AuthorityPublicationObservation& publication =
        record.point_listener.resulting_publication;
    const bool exactScenePublication =
        publication.target == record.scene_start.authority.scene
        && publication.authority_schema == record.scene_start.authority.schema
        && publication.authority_generation == record.scene_start.authority.authority_generation
        && publication.publication_epoch == record.scene_start.authority.publication_epoch
        && publication.publication_serial == record.scene_start.authority.publication_serial
        && publication.body_hash == record.scene_start.authority.body_hash
        && publication.significant_bits == record.scene_start.authority.significant_bits;
    const bool exactPredecessorPublication =
        !exactScenePublication
        && publication.publication_epoch < record.scene_start.authority.publication_epoch
        && publication.publication_serial < record.scene_start.authority.publication_serial;
    const bool publicationRelationValid =
        (record.point_publication_relation
             == AuthorityPublicationRelation::direct_scene_start_authority
         && exactScenePublication)
        || (record.point_publication_relation
                == AuthorityPublicationRelation::precedes_scene_start_authority
            && exactPredecessorPublication);
    if (!publicationRelationValid) {
        return CaptureValidationResult::publication_mismatch;
    }
    if (!valid_scene_start(record.scene_start, record.lineage)) {
        return CaptureValidationResult::invalid_scene_start;
    }
    if (!valid_actor_chronology(record.chronology)
        || !same_actor_generation(record.chronology.initial_visible.identity,
                                  record.scene_start.initial_visible.actor.identity)
        || !same_actor_generation(record.chronology.concurrent_carrier.identity,
                                  record.scene_start.concurrent_carrier.actor.identity)) {
        return CaptureValidationResult::invalid_chronology;
    }
    if (!valid_creation_edge(record.successor_creation,
                             record.scene_start.root_advance_start_serial)
        || !same_actor_generation(record.successor_creation.actor.identity,
                                  record.chronology.true_successor.identity)
        || record.successor_creation.actor.created_tick
               <= record.chronology.initial_visible.terminal_tick) {
        return CaptureValidationResult::invalid_successor_creation;
    }
    if (!valid_countdown_observation(record.countdown, executableRvaBegin, executableRvaEnd)
        || record.countdown.owner.authority_generation != record.lineage.authority_generation
        || (record.countdown.owner.scheduler_tag != kInitialVisibleSchedulerTag
            && record.countdown.owner.scheduler_tag != kTrueSuccessorSchedulerTag)) {
        return CaptureValidationResult::invalid_countdown;
    }
    if (!record.original_forwarded_exactly_once) {
        return CaptureValidationResult::original_not_forwarded_once;
    }
    return CaptureValidationResult::valid;
}

std::uint64_t bounded_hash(std::span<const std::byte> bytes) noexcept {
    constexpr std::uint64_t kOffset = 14695981039346656037ULL;
    constexpr std::uint64_t kPrime = 1099511628211ULL;
    std::uint64_t hash = kOffset;
    for (const std::byte value : bytes) {
        hash ^= static_cast<std::uint64_t>(std::to_integer<std::uint8_t>(value));
        hash *= kPrime;
    }
    return hash;
}

TelemetryProjection project_telemetry(const GroundDwellCaptureRecord& record) noexcept {
    TelemetryProjection projection{};
    projection.queue_sequence = record.metadata.queue_sequence;
    projection.monotonic_tick = record.metadata.monotonic_tick;
    projection.capture_epoch = record.lineage.capture_epoch;
    projection.activity_activation_generation = record.lineage.activity_activation_generation;
    projection.region_generation = record.lineage.region_generation;
    projection.wipe_replay_generation = record.lineage.wipe_replay_generation;
    projection.roster_record_generation = record.lineage.roster_record_generation;
    projection.scene_component_generation = record.lineage.scene_component_generation;
    projection.point_authority_generation = record.point_listener.authority.authority_generation;
    projection.scene_authority_generation = record.scene_start.authority.authority_generation;
    projection.point_result_publication_epoch =
        record.point_listener.resulting_publication.publication_epoch;
    projection.scene_publication_epoch = record.scene_start.authority.publication_epoch;
    projection.incident_root_hash = record.point_listener.incident_root_hash;
    projection.listener_row_hash = record.point_listener.listener_row_hash;
    projection.scene_body_hash = record.scene_start.authority.body_hash;
    projection.scene_decoded_hash = record.scene_start.authority.decoded_hash;
    projection.scene_entries_hash = record.scene_start.authority.entries_hash;
    projection.scene_words_hash = record.scene_start.authority.words_hash;
    projection.actor_cast_source_full_handle =
        record.scene_start.cast.resolved_actor_source_full_handle;
    projection.actor_cast_source_generation =
        record.scene_start.cast.resolved_actor_source_generation;
    projection.timeline_cast_full_handle = record.scene_start.cast.resolved_timeline_full_handle;
    projection.timeline_cast_generation = record.scene_start.cast.resolved_timeline_generation;
    projection.initial_full_handle = record.chronology.initial_visible.identity.packed_full_handle;
    projection.carrier_full_handle =
        record.chronology.concurrent_carrier.identity.packed_full_handle;
    projection.successor_full_handle = record.chronology.true_successor.identity.packed_full_handle;
    projection.initial_object_generation =
        record.chronology.initial_visible.identity.object_record_generation;
    projection.carrier_object_generation =
        record.chronology.concurrent_carrier.identity.object_record_generation;
    projection.successor_object_generation =
        record.chronology.true_successor.identity.object_record_generation;
    projection.initial_transform_hash = transform_hash(record.chronology.initial_visible.root);
    projection.carrier_transform_hash = transform_hash(record.chronology.concurrent_carrier.root);
    projection.successor_transform_hash = transform_hash(record.chronology.true_successor.root);
    projection.initial_descriptor_hash = record.chronology.initial_visible.descriptor_hash;
    projection.carrier_descriptor_hash = record.chronology.concurrent_carrier.descriptor_hash;
    projection.successor_descriptor_hash = record.chronology.true_successor.descriptor_hash;
    projection.listener_callback_rva = record.point_listener.listener_callback_rva;
    projection.countdown_source_row_identity = record.countdown.source_row_identity;
    projection.countdown_source_row_generation = record.countdown.source_row_generation;
    projection.committed_root = record.scene_start.authority.committed_root;
    projection.effective_root = record.scene_start.authority.effective_root;
    projection.countdown_delta_bits = std::bit_cast<std::uint32_t>(record.countdown.delta_seconds);
    projection.countdown_before_bits =
        std::bit_cast<std::uint32_t>(record.countdown.remaining_before);
    projection.countdown_after_bits =
        std::bit_cast<std::uint32_t>(record.countdown.remaining_after);
    projection.initial_scheduler_tag = record.chronology.initial_visible.identity.scheduler_tag;
    projection.carrier_scheduler_tag = record.chronology.concurrent_carrier.identity.scheduler_tag;
    projection.successor_scheduler_tag = record.chronology.true_successor.identity.scheduler_tag;
    projection.transition_ordinal = record.countdown.transition_ordinal;
    return projection;
}

QueuePushResult CaptureQueue::try_push(const GroundDwellCaptureRecord& record,
                                       std::uint32_t executableRvaBegin,
                                       std::uint32_t executableRvaEnd) noexcept {
    if (validate_capture_record(record, executableRvaBegin, executableRvaEnd)
        != CaptureValidationResult::valid) {
        invalid_.fetch_add(1U, std::memory_order_relaxed);
        return QueuePushResult::invalid;
    }
    if (claim_.test_and_set(std::memory_order_acquire)) {
        busy_.fetch_add(1U, std::memory_order_relaxed);
        return QueuePushResult::busy;
    }
    struct ClaimRelease final {
        std::atomic_flag& claim;
        ~ClaimRelease() noexcept {
            claim.clear(std::memory_order_release);
        }
    } release{claim_};
    if (count_ == records_.size()) {
        full_.fetch_add(1U, std::memory_order_relaxed);
        return QueuePushResult::full;
    }
    if (next_sequence_ == 0U || next_sequence_ == (std::numeric_limits<std::uint64_t>::max)()) {
        sequence_exhausted_.fetch_add(1U, std::memory_order_relaxed);
        return QueuePushResult::sequence_exhausted;
    }
    records_[write_index_] = record;
    records_[write_index_].metadata.queue_sequence = next_sequence_++;
    write_index_ = (write_index_ + 1U) % records_.size();
    ++count_;
    visible_count_.store(count_, std::memory_order_release);
    published_.fetch_add(1U, std::memory_order_relaxed);
    return QueuePushResult::published;
}

bool CaptureQueue::try_pop(GroundDwellCaptureRecord& output) noexcept {
    if (claim_.test_and_set(std::memory_order_acquire)) {
        return false;
    }
    struct ClaimRelease final {
        std::atomic_flag& claim;
        ~ClaimRelease() noexcept {
            claim.clear(std::memory_order_release);
        }
    } release{claim_};
    if (count_ == 0U) {
        return false;
    }
    output = records_[read_index_];
    records_[read_index_] = {};
    read_index_ = (read_index_ + 1U) % records_.size();
    --count_;
    visible_count_.store(count_, std::memory_order_release);
    return true;
}

bool CaptureQueue::try_reset() noexcept {
    if (claim_.test_and_set(std::memory_order_acquire)) {
        return false;
    }
    struct ClaimRelease final {
        std::atomic_flag& claim;
        ~ClaimRelease() noexcept {
            claim.clear(std::memory_order_release);
        }
    } release{claim_};
    if (count_ != 0U) {
        return false;
    }
    records_ = {};
    read_index_ = 0U;
    write_index_ = 0U;
    next_sequence_ = 1U;
    visible_count_.store(0U, std::memory_order_release);
    published_.store(0U, std::memory_order_relaxed);
    invalid_.store(0U, std::memory_order_relaxed);
    busy_.store(0U, std::memory_order_relaxed);
    full_.store(0U, std::memory_order_relaxed);
    sequence_exhausted_.store(0U, std::memory_order_relaxed);
    return true;
}

std::size_t CaptureQueue::size() const noexcept {
    return visible_count_.load(std::memory_order_acquire);
}

QueueCounters CaptureQueue::counters() const noexcept {
    return {published_.load(std::memory_order_relaxed),
            invalid_.load(std::memory_order_relaxed),
            busy_.load(std::memory_order_relaxed),
            full_.load(std::memory_order_relaxed),
            sequence_exhausted_.load(std::memory_order_relaxed)};
}

} // namespace sunrise::client::hooks::bootflow::opening_authority::ikora_ground_dwell
