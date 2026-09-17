#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "../../../state/activity/eater_of_worlds/reinforcement_roster.h"

namespace dawn::client::hooks::bootflow::eater_reinforcement_selection {

inline constexpr std::uint32_t kSelectionReferenceKind = 0x808099D8U;
inline constexpr std::uint32_t kFinaleCategoryHash = 0x19C57E65U;
inline constexpr std::uint32_t kOriginalEntity = 0x80C0FA98U;
inline constexpr std::size_t kSourceDefinitionOffset = 0x728U;

struct NativeReference final {
    std::uint32_t handle{UINT32_MAX};
    std::uint32_t kind{};
    std::int64_t offset{};
};
static_assert(sizeof(NativeReference) == 0x10U);

// Exact output of native +4E34C0. +4E2E80 copies all 0x28 bytes into its durable queue.
struct SelectedMember final {
    NativeReference reference{};
    std::uint32_t categoryIndex{UINT32_MAX};
    std::uint32_t categoryHash{};
    std::uint32_t choiceIdentity{};
    std::uint8_t choiceFlags{};
    std::array<std::byte, 3U> choiceReserved{};
    std::uint32_t sourceGeneration{};
    std::uint32_t tailReserved{};
};
static_assert(sizeof(SelectedMember) == 0x28U);
static_assert(offsetof(SelectedMember, categoryIndex) == 0x10U);
static_assert(offsetof(SelectedMember, categoryHash) == 0x14U);
static_assert(offsetof(SelectedMember, choiceIdentity) == 0x18U);
static_assert(offsetof(SelectedMember, choiceFlags) == 0x1CU);
static_assert(offsetof(SelectedMember, sourceGeneration) == 0x20U);

struct DonorSpec final {
    state::activity::eater_of_worlds::reinforcements::Kind kind{};
    NativeReference reference{};
    std::uint32_t entity{};
    std::uint32_t choiceIdentity{};
    std::uint8_t choiceFlags{};
};

// These are complete authored Eater choices, including the native selector identity. The offsets
// address the 0x808099D8 object within the loaded resource, never a process address.
[[nodiscard]] constexpr DonorSpec donor_spec(
    state::activity::eater_of_worlds::reinforcements::Kind kind) noexcept {
    using Kind = state::activity::eater_of_worlds::reinforcements::Kind;
    switch (kind) {
    case Kind::incendior:
        return {kind, {0x8155C43BU, kSelectionReferenceKind, 0x920}, 0x80BFAA20U,
                0xAAF9AB71U, 0U};
    case Kind::psion:
        return {kind, {0x8155C3FFU, kSelectionReferenceKind, 0x8B0}, 0x80C1A8E4U,
                0x36E6516AU, 0U};
    default: return {};
    }
}

[[nodiscard]] constexpr std::uint16_t source_slot(std::uint32_t sourceTag) noexcept {
    switch (sourceTag) {
    case 0x8155C42FU: return 18U;
    case 0x8155C432U: return 19U;
    case 0x8155C435U: return 20U;
    default: return UINT16_MAX;
    }
}

struct Context final {
    std::uint64_t run{};
    std::uint32_t sourceTag{UINT32_MAX};
    std::uint32_t sourceGeneration{};
    std::int32_t category{-1};
    std::uint32_t resolvedEntity{UINT32_MAX};
};

struct Donor final {
    DonorSpec spec{};
    bool resolved{};
};

enum class Result : std::uint8_t { passthrough, native, substituted };

/**
 * Applies one verified, complete Eater choice to a native selected-member value.
 * Only the relocatable entity reference and its choice metadata change. Native category,
 * generation, and opaque fields remain byte-for-byte as produced by +4E34C0.
 */
[[nodiscard]] constexpr Result apply(const Context& context,
                                     const Donor& donor,
                                     SelectedMember& selected) noexcept {
    using namespace state::activity::eater_of_worlds;
    const std::uint16_t slot = source_slot(context.sourceTag);
    if (context.run == 0U || slot == UINT16_MAX || context.sourceGeneration == 0U
        || context.sourceGeneration > 0x7FFFFFFFU
        || context.category != 0
        || context.sourceGeneration != selected.sourceGeneration
        || selected.reference.handle != context.sourceTag
        || selected.reference.kind != kSelectionReferenceKind
        || selected.categoryIndex != 0U || selected.categoryHash != kFinaleCategoryHash
        || context.resolvedEntity != kOriginalEntity) {
        return Result::passthrough;
    }

    const auto selectedKind = reinforcements::select(slot, 0U);
    if (selectedKind == reinforcements::Kind::colossus) {
        return Result::native;
    }
    const DonorSpec expected = donor_spec(selectedKind);
    if (!donor.resolved || expected.kind == reinforcements::Kind::invalid
        || donor.spec.kind != expected.kind || donor.spec.reference.handle != expected.reference.handle
        || donor.spec.reference.kind != expected.reference.kind
        || donor.spec.reference.offset != expected.reference.offset
        || donor.spec.entity != reinforcements::entity(selectedKind)
        || donor.spec.entity != expected.entity
        || donor.spec.choiceIdentity != expected.choiceIdentity
        || donor.spec.choiceFlags != expected.choiceFlags) {
        return Result::passthrough;
    }

    selected.reference = donor.spec.reference;
    selected.choiceIdentity = donor.spec.choiceIdentity;
    selected.choiceFlags = donor.spec.choiceFlags;
    return Result::substituted;
}

static_assert(source_slot(0x8155C42FU) == 18U && source_slot(0x8155C435U) == 20U);
static_assert(donor_spec(state::activity::eater_of_worlds::reinforcements::Kind::incendior)
                  .entity
              == 0x80BFAA20U);
static_assert(donor_spec(state::activity::eater_of_worlds::reinforcements::Kind::psion)
                  .reference.offset
              == 0x8B0);

} // namespace dawn::client::hooks::bootflow::eater_reinforcement_selection
