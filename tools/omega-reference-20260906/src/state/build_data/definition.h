#pragma once

#include <cstdint>

#include "../../core/provenance/build_provenance.h"

namespace dawn::state::build_data {

/** Destiny PE fields, authored configuration, and the Dawn producer tie mappings to one build. */
struct BuildIdentity {
    std::uint32_t imageTimestamp{};
    std::uint32_t imageSize{};
    /** Authored item levels and the light-stat rules, so a mismatched cache is not reused. */
    std::uint64_t configuredEquipmentHash{};
    /** Canonical digest of the frozen tracked/dirty/untracked source manifest. */
    core::provenance::Sha256Digest producerSourceSha256{};
    /** Exact SHA-256 of the loaded Dawn module's on-disk image. */
    core::provenance::Sha256Digest producerImageSha256{};

    /** @return True when every field matches. */
    [[nodiscard]] constexpr bool operator==(const BuildIdentity& other) const noexcept = default;
};

} // namespace dawn::state::build_data
