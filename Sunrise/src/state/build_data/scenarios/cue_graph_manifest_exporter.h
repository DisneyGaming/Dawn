#pragma once

#include <span>
#include <string_view>

#include "../../../middleware/bap/activity_message/sense_update.h"
#include "definition.h"

namespace sunrise::state::build_data::scenarios {

/** Exports the first generated cue manifest and evidence report without changing authority. */
void export_towerfall_cue_manifest(std::span<const Definition> definitions,
                                  std::span<const RosterGroup> groups) noexcept;

/** Maps a decoded observation against its generated manifest and exports the result. */
void export_cue_observation_mapping(
    std::string_view activity,
    const middleware::bap::activity_message::sense_update::SenseUpdate& update) noexcept;

} // namespace sunrise::state::build_data::scenarios
