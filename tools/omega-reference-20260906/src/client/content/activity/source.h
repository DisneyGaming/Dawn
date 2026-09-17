#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "../../../middleware/content/packages/reader/reader.h"
#include "../../../middleware/content/packages/tables/scenario_walk.h"

namespace dawn::client::content::activity {

namespace packages = dawn::middleware::content::packages;

/** One buffer per read slot. */
inline constexpr std::size_t kSlotCount =
    static_cast<std::size_t>(packages::tables::ReadSlot::count);

/** Lock-owned storage for one scenario walk. */
struct ScenarioSource {
    const packages::reader::Source* source{};
    packages::reader::Scratch scratch{};
    std::array<std::vector<std::byte>, kSlotCount> slots{};
};

/**
 * Writes the installed Homecoming activity records to the Dawn analysis directory.
 * This is temporary local research instrumentation and is not part of the public runtime.
 */
[[nodiscard]] bool dump_homecoming(const packages::reader::Source& source,
                                   packages::reader::Scratch& scratch) noexcept;

/**
 * Finds the Omega behavior-property definition required by native condition 0x75.
 * This is bounded, read-only research instrumentation over the two package families observed
 * live; it does not publish or mutate package content.
 */
[[nodiscard]] bool scan_omega_behavior_properties(const packages::reader::Source& source,
                                                  packages::reader::Scratch& scratch) noexcept;

} // namespace dawn::client::content::activity
