#include "omega_lair_start.h"

#include <array>
#include <cstdio>
#include <mutex>
#include "../../../core/logging/log.h"

namespace dawn::state::activity::omega::lair_start {
namespace { std::mutex mutex; State state; }

Snapshot prepare(std::uint64_t run, std::uint32_t generation) noexcept {
    const std::lock_guard lock(mutex);
    return state.prepare(run, generation);
}
void note_left_started(std::uint64_t run, std::uint32_t generation,
                       std::uint32_t actor, std::uint32_t character,
                       std::uint32_t biped) noexcept {
    const std::lock_guard lock(mutex);
    if (!state.left_started({run, generation, actor, character, biped})) return;
    std::array<char, 320> line{};
    const int length = std::snprintf(line.data(), line.size(),
        "ev=omega_lair_start stage=left_started run=%llu generation=%u actor=%08X "
        "character=%08X biped=%08X source_slots=3..8 requested=12 trigger=native_left_animation",
        static_cast<unsigned long long>(run), generation, actor, character, biped);
    if (length > 0 && static_cast<std::size_t>(length) < line.size())
        core::log::write(core::log::Channel::state, core::log::Level::info,
                        {line.data(), static_cast<std::size_t>(length)});
}
} // namespace dawn::state::activity::omega::lair_start
