#pragma once

#include <cmath>
#include <cstdint>
#include "omega_reveal_door.h"
#include "omega_lair_dialogue.h"

namespace dawn::state::activity::omega {

struct Point { float x, y, z; };
enum class Route : std::uint8_t { opening, forestEntrance, forestExit, crownEntrance, reveal };

// Authored ActivityPoints from 80F47BB3, 80F4753A and 80F47B3E respectively.
inline constexpr Point kForestEntrance{-1357.0991F, 1126.9106F, -46.8116F};
inline constexpr Point kForestExit{-1491.8749F, 519.2537F, -15.9310F};
inline constexpr Point kCrownEntrance{-1491.7739F, 415.2429F, -26.9186F};
inline constexpr std::uint32_t kHandoffGroup = 0xE831A367U;
inline constexpr std::uint32_t kCrownGroup = 0xA3928C71U;
inline constexpr std::uint32_t kBossGroup = 0x95FB2E01U;
inline constexpr std::uint32_t kRevealGroup = 0xF4D0E0B2U;

/** Live type-53 identity: definition 80F47BDA+1408 owns bank 80F1FD07 at +58.
 * The old diagnostic handle-table resolver fails on this executable; do not gate cues on it. */
[[nodiscard]] constexpr bool dialogue_source(std::uint32_t definition, std::int64_t offset) noexcept {
    return definition == 0x80F47BDAU && offset == 0x1408;
}

struct Waypoint {
    std::uint32_t registry{};
    std::uint16_t index{};
    std::uint32_t bubbleName{};
    std::uint32_t pointName{};
};
[[nodiscard]] constexpr Waypoint waypoint(Route route) noexcept {
    switch (route) {
    case Route::forestEntrance: return {0xF7A6CE7FU, 10, 0xA83A9175U, 0x4E5FD117U};
    case Route::forestExit: return {kHandoffGroup, 8, 0x47EA4CEAU, 0xBF60FA2AU};
    case Route::crownEntrance:
    case Route::reveal: return {kCrownGroup, 13, 0x23345E71U, 0x83403AF7U};
    default: return {};
    }
}

struct Progress {
    Route route{};
    bool vistaDialogue{};
    bool exitDialogue{};
    bool bossDoorReached{};
    std::uint64_t lairDialogueRequestedMask{};
    std::uint8_t lairDialoguePendingRow{255};
    std::uint32_t lairObjectiveEvent{}, missionDialogueTimeouts{};
    std::uint32_t destination{0x811C9DC5U};
    std::uint32_t loadedBubble{UINT32_MAX};
};

/** Stable native spawn generation after the doorway, retained through region reloads. */
[[nodiscard]] constexpr std::uint32_t boss_generation(std::uint64_t run, const Progress& progress) noexcept {
    return progress.bossDoorReached && run != UINT64_MAX
        ? 1U + static_cast<std::uint32_t>(run % 0x7FFFFFFFULL) : 0U;
}

/** Run-local, monotonic progression; region loads do not create another mission run. */
class Progression final {
public:
    void reset_for(std::uint64_t run) noexcept {
        if (run_ != run) {
            *this = Progression{};
            run_ = run;
        }
    }

    void dispatched(std::uint64_t run, std::int32_t row, std::uint64_t now) noexcept {
        reset_for(run);
        // Observe the native dispatch, then allow the full bank duration plus a small gap.
        // First dispatch wins: duplicate native notifications cannot postpone the next cue.
        if (row == 6 && tunnelEnds_ == 0) tunnelEnds_ = now + 9100;
        if (row == 7 && vistaEnds_ == 0) vistaEnds_ = now + 11840;
        lairDialogue_.dispatched(row, now);
        sync_lair_dialogue();
    }

    void cinematic_completed(std::uint64_t run, std::uint64_t completedAt) noexcept {
        reset_for(run);
        lairDialogue_.cinematic_completed(completedAt);
    }

    void loaded(std::uint64_t run, std::uint32_t bubble, std::uint32_t destination) noexcept {
        reset_for(run);
        progress_.loadedBubble = bubble;
        if (destination != 0 && destination != 0x811C9DC5U) progress_.destination = destination;
        // A native area load retires the previous checkpoint even when a forced teleport
        // bypasses its proximity trigger. Backtracking cannot restore a retired checkpoint.
        if (progress_.route == Route::opening) return;
        if (bubble == 11 && progress_.route < Route::forestExit) progress_.route = Route::forestExit;
        if (bubble == 14 && progress_.route < Route::crownEntrance) progress_.route = Route::crownEntrance;
    }

    [[nodiscard]] Progress current() const noexcept { return progress_; }

    [[nodiscard]] Progress update(std::uint64_t run, std::uint64_t now, bool gate,
                                  bool arrived, bool present, Point position) noexcept {
        reset_for(run);
        if (!arrived) return progress_;
        lairDialogue_.update(now, progress_.loadedBubble, present, position.x, position.y, position.z);
        sync_lair_dialogue();
        if (gate && progress_.route == Route::opening) progress_.route = Route::forestEntrance;
        loaded(run, progress_.loadedBubble, progress_.destination);
        if (present && progress_.route != Route::opening) {
            if (progress_.loadedBubble == 14 && progress_.route >= Route::crownEntrance
                && reveal_door::reached(position.x, position.y, position.z))
                progress_.bossDoorReached = true;
            // Latch proximity separately from speech readiness, so walking through a checkpoint
            // while Ghost is talking does not lose its cue. No route jumps from a timer alone.
            if (progress_.route == Route::forestEntrance && within_radius(position, kForestEntrance, 40))
                progress_.route = Route::forestExit;
            if (progress_.route == Route::forestExit && within_radius(position, kForestExit, 35))
                progress_.route = Route::crownEntrance;
            if (progress_.route == Route::crownEntrance && within_radius(position, kCrownEntrance, 18))
                progress_.route = Route::reveal;
        }
        if (progress_.route >= Route::forestExit && tunnelEnds_ != 0 && now >= tunnelEnds_)
            progress_.vistaDialogue = true;
        if (progress_.route >= Route::crownEntrance && progress_.vistaDialogue
            && vistaEnds_ != 0 && now >= vistaEnds_)
            progress_.exitDialogue = true;
        return progress_;
    }

private:
    void sync_lair_dialogue() noexcept {
        progress_.lairDialogueRequestedMask = lairDialogue_.requested_mask();
        progress_.lairDialoguePendingRow = lairDialogue_.pending_row();
        progress_.lairObjectiveEvent = lairDialogue_.objective_event();
    }
    [[nodiscard]] static bool within_radius(Point a, Point b, float radius) noexcept {
        const float x = a.x - b.x, y = a.y - b.y, z = a.z - b.z;
        const float squared = x*x + y*y + z*z;
        return std::isfinite(squared) && squared <= radius*radius;
    }
    std::uint64_t run_{UINT64_MAX};
    std::uint64_t tunnelEnds_{};
    std::uint64_t vistaEnds_{};
    Progress progress_{};
    LairDialogue lairDialogue_{};
};

/** Thread-safe bridge between native dialogue dispatch and the authority publisher. */
void note_dialogue_dispatch(std::uint64_t run, std::int32_t row, std::uint64_t now) noexcept;
void note_native_area(std::uint64_t run, std::uint32_t bubble, std::uint32_t destination) noexcept;
void note_lair_cinematic_completed(std::uint64_t run, std::uint64_t completedAt) noexcept;
[[nodiscard]] Progress update_progress(std::uint64_t run, std::uint64_t now, bool gate,
                                       bool arrived, bool present, Point position) noexcept;
/** Latest host checkpoint for this exact run, or no presentation intent for an old run. */
[[nodiscard]] Progress presentation_progress(std::uint64_t run) noexcept;

} // namespace dawn::state::activity::omega
