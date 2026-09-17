#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "definition.h"

namespace dawn::state::build_data::scenarios {

/** Raw descriptor metadata retained only for the measured Omega component roster. */
struct OmegaSchema final {
    std::uint32_t registryKey{};
    std::uint32_t objectTag{};
    std::uint32_t componentClass{};
    std::uint32_t senseSchema{};
    std::uint32_t authSchema{};
    std::uint16_t slotIndex{};
    std::uint8_t slotType{};
};

/** Clears the in-memory Omega schema sidecar. */
void clear_omega_schemas() noexcept;

/** Records one descriptor found while installed packages are being walked. */
void record_omega_schema(const OmegaSchema& schema) noexcept;

/**
 * Validates package-scan metadata or loads the last validated sidecar on a build-data cache hit.
 * The sidecar is accepted only when all six current roster layouts and 57 real slots match.
 */
[[nodiscard]] bool prepare_omega_schemas(std::span<const RosterGroup> groups,
                                         std::span<const std::size_t> omegaGroupIndices) noexcept;

/** Finds raw metadata by the same stable address the activity messages carry. */
[[nodiscard]] bool find_omega_schema(std::uint32_t registryKey,
                                     std::uint8_t slotType,
                                     std::uint16_t slotIndex,
                                     OmegaSchema& schema) noexcept;

/** @return Number of raw Omega descriptors currently available. */
[[nodiscard]] std::size_t omega_schema_count() noexcept;

} // namespace dawn::state::build_data::scenarios
