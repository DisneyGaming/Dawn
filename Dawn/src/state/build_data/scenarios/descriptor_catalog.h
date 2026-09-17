#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "definition.h"

namespace dawn::state::build_data::scenarios {

/** Stable wire address of one authored component slot. */
struct CueNodeId final {
    std::uint32_t registryKey{};
    std::uint16_t slotIndex{};
    std::uint8_t slotType{};
};

/** Package descriptor metadata retained for every extracted roster slot. */
struct SlotDescriptorMetadata final {
    CueNodeId node{};
    std::uint32_t objectTag{};
    std::uint32_t descriptorTag{};
    std::uint32_t descriptorOffset{};
    std::uint32_t componentClass{};
    std::uint32_t senseSchema{};
    std::uint32_t authSchema{};
    std::uint8_t flags{};
};

enum class DescriptorLookup : std::uint8_t {
    missing,
    unique,
    ambiguous,
};

/** Reads one retained descriptor by its ordinal within a roster group. */
[[nodiscard]] bool descriptor_at(const RosterGroup& group,
                                 std::size_t ordinal,
                                 SlotDescriptorMetadata& output) noexcept;

/** Finds one descriptor by the stable address carried in sense and authority packets. */
[[nodiscard]] DescriptorLookup
find_descriptor(std::span<const RosterGroup> groups,
                CueNodeId node,
                SlotDescriptorMetadata& output) noexcept;

/** Counts all descriptors in a bounded group catalog. */
[[nodiscard]] std::size_t descriptor_count(std::span<const RosterGroup> groups) noexcept;

} // namespace dawn::state::build_data::scenarios
