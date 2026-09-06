#include "omega_progression.h"
#include "omega_mission_runtime.h"
#include "omega_mission_presentation.h"

#include <array>
#include <cstdio>
#include <mutex>

#include "../../../core/logging/log.h"

namespace sunrise::state::activity::omega {
namespace {
std::mutex mutex;
Progression tracker;
std::uint64_t loggedRun = UINT64_MAX;
Progress logged;
}

void note_dialogue_dispatch(std::uint64_t run, std::int32_t row, std::uint64_t now) noexcept {
    const std::lock_guard lock(mutex);
    tracker.dispatched(run, row, now);
    if (run == loggedRun) logged = tracker.current();
}

void note_lair_cinematic_completed(std::uint64_t run, std::uint64_t completedAt) noexcept {
    const std::lock_guard lock(mutex);
    tracker.cinematic_completed(run, completedAt);
}

Progress presentation_progress(std::uint64_t run) noexcept {
    const std::lock_guard lock(mutex);
    return run == loggedRun ? logged : Progress{};
}

void note_native_area(std::uint64_t run, std::uint32_t bubble, std::uint32_t destination) noexcept {
    const std::lock_guard lock(mutex);
    tracker.loaded(run, bubble, destination);
    loggedRun = run;
    logged = tracker.current();
}

Progress update_progress(std::uint64_t run, std::uint64_t now, bool gate,
                          bool arrived, bool present, Point position) noexcept {
    const std::lock_guard lock(mutex);
    auto progress = tracker.update(run, now, gate, arrived, present, position);
    const auto mission=mission_presentation::observe(mission::runtime::snapshot(run),now);
    progress.lairDialogueRequestedMask |= mission.requested;
    progress.missionDialogueTimeouts=mission.timeouts;
    if(progress.lairDialoguePendingRow==255) progress.lairDialoguePendingRow=mission.pending;
    if(mission.objective) progress.lairObjectiveEvent=mission.objective;
    if(mission.cinematic) progress.lairDialoguePendingRow=255;
    if (run != loggedRun || progress.route != logged.route
        || progress.vistaDialogue != logged.vistaDialogue
        || progress.exitDialogue != logged.exitDialogue
        || progress.bossDoorReached != logged.bossDoorReached
        || progress.lairDialogueRequestedMask != logged.lairDialogueRequestedMask
        || progress.lairDialoguePendingRow != logged.lairDialoguePendingRow
        || progress.missionDialogueTimeouts != logged.missionDialogueTimeouts
        || progress.lairObjectiveEvent != logged.lairObjectiveEvent) {
        constexpr const char* names[]{"opening", "forest_entrance", "forest_exit", "crown_entrance", "reveal"};
        const auto target = waypoint(progress.route);
        std::array<char, 384> line{};
        const int length = std::snprintf(line.data(), line.size(),
            "ev=omega_progression run=%llu route=%s vista=%u exit=%u waypoint=0x%08X/47/%u "
            "position=%.2f,%.2f,%.2f present=%u boss_door=%u lair_cues=%016llX pending_row=%u objective=%08X dispatch_timeouts=%u",
            static_cast<unsigned long long>(run), names[static_cast<unsigned>(progress.route)],
            progress.vistaDialogue ? 1U : 0U, progress.exitDialogue ? 1U : 0U,
            target.registry, static_cast<unsigned>(target.index),
            static_cast<double>(position.x), static_cast<double>(position.y),
            static_cast<double>(position.z), present ? 1U : 0U, progress.bossDoorReached ? 1U : 0U,
            static_cast<unsigned long long>(progress.lairDialogueRequestedMask), static_cast<unsigned>(progress.lairDialoguePendingRow),
            progress.lairObjectiveEvent,progress.missionDialogueTimeouts);
        if (length > 0 && static_cast<std::size_t>(length) < line.size())
            core::log::write(core::log::Channel::state, core::log::Level::info,
                            {line.data(), static_cast<std::size_t>(length)});
        loggedRun = run;
        logged = progress;
    }
    return progress;
}
} // namespace sunrise::state::activity::omega
