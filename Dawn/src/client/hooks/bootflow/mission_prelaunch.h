#pragma once
#include "../../../state/activity/forced/definition.h"

namespace dawn::client::hooks::bootflow {
/** Requests installation on the manager owner. True only when every required hook is ready.
 * Does not publish or activate the destination while the caller waits. */
[[nodiscard]] bool prepare_mission_prelaunch(
    const state::activity::forced::ForcedDestination& destination) noexcept;
}
