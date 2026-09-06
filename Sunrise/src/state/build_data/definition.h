#pragma once

#include <cstdint>

#include "../../core/provenance/build_provenance.h"

namespace sunrise::state::build_data {

/** Destiny PE fields, authored configuration, and the Sunrise producer tie mappings to one build. */
struct BuildIdentity {
    std::uint32_t imageTimestamp{};
    std::uint32_t imageSize{};
    /** Authored item levels and the light-stat rules, so a mismatched cache is not reused. */
    std::uint64_t configuredEquipmentHash{};
    /** Canonical digest of the frozen tracked/dirty/untracked source manifest. */
    core::provenance::Sha256Digest producerSourceSha256{};
    /** Exact SHA-256 of the loaded Sunrise module's on-disk image. */
    core::provenance::Sha256Digest producerImageSha256{};

    /** @return True when every field matches. */
    [[nodiscard]] constexpr bool operator==(const BuildIdentity& other) const noexcept = default;
};

} // namespace sunrise::state::build_data
