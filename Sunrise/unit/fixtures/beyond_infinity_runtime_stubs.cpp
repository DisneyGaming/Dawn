// Link the real mission runtime into its isolated unit executable. No game
// process, world state, or production log sink is touched by these fixtures.
#include "../../src/state/activity/runtime.h"
#include "../../src/core/logging/log.h"
namespace sunrise::state::activity {
std::uint64_t mission_run_generation() noexcept { return 0; }
bool mission_seed_armed() noexcept { return false; }
WorldPhase world_phase() noexcept { return WorldPhase::arrived; }
}
namespace sunrise::core::log {
void write(Channel,Level,std::string_view) noexcept {}
}
