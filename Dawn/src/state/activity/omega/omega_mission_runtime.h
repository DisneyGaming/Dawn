#pragma once
#include "omega_mission_state.h"
#include <mutex>
#include <utility>

namespace dawn::state::activity::omega::mission::runtime {
// One state instance for native callbacks and authority publication. Native calls
// must run outside this lock: they can synchronously reenter another callback.
inline std::mutex mutex;
inline State state;
template<class Callback> bool receipt(Callback&& callback) noexcept {
    const std::lock_guard lock(mutex);
    return std::forward<Callback>(callback)(state);
}
inline Snapshot snapshot(std::uint64_t run) noexcept {
    const std::lock_guard lock(mutex);
    const auto& value=state.snapshot();
    return value.command.token.boss.run==run ? value : Snapshot{};
}
} // namespace dawn::state::activity::omega::mission::runtime
