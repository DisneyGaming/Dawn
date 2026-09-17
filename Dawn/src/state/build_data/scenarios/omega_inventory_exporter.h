#pragma once

#include <span>

#include "definition.h"

namespace dawn::state::build_data::scenarios {

/** Writes a read-only inventory of the authored Omega roster when that layout is published. */
void export_omega_inventory(std::span<const Definition> definitions,
                            std::span<const RosterGroup> groups) noexcept;

} // namespace dawn::state::build_data::scenarios
