#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "../../../middleware/bap/activity_message/sense_update.h"
#include "definition.h"
#include "descriptor_catalog.h"

namespace dawn::state::build_data::scenarios {

inline constexpr std::size_t kManifestGroupCapacity =
    kDestinationGroupCapacity + kDestinationBubbleGroupCapacity
    + kBubbleCapacity * kDestinationAuthoredGroupCapacity;
inline constexpr std::size_t kMissingObservationCapacity = 16;

/** Unique roster-group indices referenced by one generated mission manifest. */
struct ManifestGroups final {
    std::array<std::uint16_t, kManifestGroupCapacity> indices{};
    std::size_t count{};
    bool overflowed{};
};

/** Result of resolving one complete sense update against a generated mission manifest. */
struct ObservationMappingReport final {
    std::array<CueNodeId, kMissingObservationCapacity> missingNodes{};
    std::size_t observedObjects{};
    std::size_t mappedObjects{};
    std::size_t missingObjects{};
    std::size_t ambiguousObjects{};
    std::size_t missingNodeCount{};
};

/** Evidence available for compiling semantic successor edges. */
struct ManifestEdgeReport final {
    std::size_t structuralContainmentEdges{};
    std::size_t triggerNodes{};
    std::size_t actionNodes{};
    std::size_t coResidentTriggerActionPairs{};
    std::size_t explicitSuccessorEdges{};
    bool openingSuccessorsRecoverable{};
};

/** Collects every ordinary, bubble-local, and authored group named by a mission. */
[[nodiscard]] ManifestGroups manifest_groups(const Definition& definition) noexcept;

/** Counts and classifies the nodes retained by one generated manifest. */
[[nodiscard]] ManifestEdgeReport
inspect_manifest_edges(const Definition& definition,
                       std::span<const RosterGroup> groups) noexcept;

/** Resolves sense objects by registry key, slot type, and real slot index. */
[[nodiscard]] ObservationMappingReport
map_observations(const Definition& definition,
                 std::span<const RosterGroup> groups,
                 const middleware::bap::activity_message::sense_update::SenseUpdate& update) noexcept;

} // namespace dawn::state::build_data::scenarios
