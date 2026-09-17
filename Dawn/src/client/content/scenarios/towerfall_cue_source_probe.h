#pragma once

#include <span>

#include "../../../middleware/content/packages/reader/reader.h"
#include "../../../state/build_data/scenarios/definition.h"

namespace dawn::client::content::scenarios {

/**
 * Exports exact descriptor provenance, schema-class entries, and untyped reference candidates for
 * Towerfall. This diagnostic never changes activity state or authority.
 */
[[nodiscard]] bool probe_towerfall_cue_sources(
    const middleware::content::packages::reader::Source& source,
    middleware::content::packages::reader::Scratch& scratch,
    std::span<const state::build_data::scenarios::Definition> definitions,
    std::span<const state::build_data::scenarios::RosterGroup> groups) noexcept;

} // namespace dawn::client::content::scenarios
