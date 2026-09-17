// Frozen accepted ending candidate; only includes/namespace adapted.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include "state/activity/omega_intro_rules.h"
#include "state/activity/coo/omega_reveal.h"
#include "state/activity/omega_first_lair_encounter.h"

namespace dawn::state::activity::frozen_shared_presentation {
using namespace dawn::state::activity::omega_presentation;

inline constexpr std::uint32_t kDialogueBank = 0x80F1FD07U;
inline constexpr std::size_t kDialogueRows = 34;
inline constexpr std::uint8_t kNoDialogue = 0xFFU;
inline constexpr std::array<std::uint32_t, 7> kObjectives{
    0xC252E306U, 0x1EBF4621U, 0x3517D4D5U, 0x31A51CEBU,
    0xA41DE99BU, 0x85A8F583U, 0xDF97334DU};

struct Dialogue final {
    std::uint32_t selector;
    std::uint32_t durationMs;
    std::uint32_t delayMs;
    bool sceneOwned;
};
// Bank order, not playback order. Compound rows retain their native child sequencing/selection.
// Rows 1/3/4 belong to Ikora actors; row 23 also occurs in Osiris actor graphs.
inline constexpr std::array<Dialogue, kDialogueRows> kDialogue{{
    {0xAD60F465U, 5445, 1000, false}, {0x57477432U, 6083, 0, true},
    {0x730F03C7U, 1985, 0, false}, {0x47AF17F4U, 5051, 0, true},
    {0xB4C3F0B9U, 3218, 0, true}, {0x0ED8C762U, 0, 0, false},
    {0xAE2495ACU, 8093, 0, false}, {0x0E9C80BEU, 10990, 0, false},
    {0x08AE5FB8U, 0, 0, false}, {0xE878194AU, 6461, 0, false},
    {0x7BA4F101U, 0, 0, false}, {0xB2CF9D6EU, 0, 0, false},
    {0xAB0676A8U, 2019, 0, false}, {0xA558F78FU, 6222, 0, false},
    {0x94E09524U, 5761, 0, false}, {0x0294D229U, 4432, 0, false},
    {0xB8CE809FU, 6354, 0, false}, {0x9DA4C20AU, 0, 0, false},
    {0xCB7F171DU, 2301, 0, false}, {0xC0570578U, 0, 0, false},
    {0xD16ECB03U, 0, 0, false}, {0xC645267EU, 3538, 0, false},
    {0x202F7829U, 3320, 0, false}, {0xD4CADE1DU, 2486, 0, true},
    {0x1C653216U, 0, 0, false}, {0x6352D26CU, 4173, 0, false},
    {0xD453DB47U, 1712, 0, false}, {0xEB43430DU, 0, 0, false},
    {0xDC1E272EU, 0, 0, false}, {0xB88C4BB2U, 3201, 0, false},
    {0x349D2672U, 5489, 0, false}, {0x5F8E6160U, 2781, 0, false},
    {0x03622EEBU, 3529, 0, false}, {0x921B35F9U, 3852, 5000, false}
}};

enum class Landmark : std::uint8_t { lighthouse, tunnel, forestVista, forestExit, lair, arena };
enum class Encounter : std::uint8_t {
    defenses, deletion, osirisArrives, osirisHolds, arcReady, arcReminder,
    eyeVulnerable, pursuit, defeated, cinematic
};

struct Point final { float x{}, y{}, z{}; };
enum class NavigationGoal : std::uint8_t {
    ikora, portal, forestEntrance, forestGates, lairApproach, lairEntry, complete
};
/** Fixed mission handoff points from lane 13's typed package records. The generated
 * forest has no fixed target here: its own worker supplies the next unopened gate. */
[[nodiscard]] inline constexpr bool navigation_point(NavigationGoal goal, Point& point) noexcept {
    switch (goal) {
    case NavigationGoal::ikora: point={347.3339F,249.9883F,102.6479F}; return true;
    case NavigationGoal::portal: point={366.69455F,249.999283F,100.592934F}; return true;
    case NavigationGoal::forestEntrance: point={-1357.0991F,1126.9106F,-46.8116F}; return true;
    case NavigationGoal::lairApproach: point={-1491.8749F,519.2537F,-15.931F}; return true;
    case NavigationGoal::lairEntry: point={-1491.7739F,415.2429F,-26.9186F}; return true;
    default: return false;
    }
}
struct Triangle final { std::uint8_t a{}, b{}, c{}; };
struct Volume final {
    Landmark landmark;
    std::uint32_t registry;
    std::uint16_t slot;
    Point minimum, maximum;
    std::array<Point, 10> vertices;
    std::array<Triangle, 8> triangles;
    std::uint8_t triangleCount;
};

[[nodiscard]] inline bool contains(const Volume& v, Point p) noexcept {
    if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)
        || p.x < v.minimum.x || p.x > v.maximum.x
        || p.y < v.minimum.y || p.y > v.maximum.y
        || p.z < v.minimum.z || p.z > v.maximum.z) { return false; }
    const auto cross = [](Point a, Point b, Point c) noexcept {
        return (b.x-a.x)*(c.y-a.y) - (b.y-a.y)*(c.x-a.x);
    };
    for (std::uint8_t i = 0; i < v.triangleCount; ++i) {
        const auto t = v.triangles[i];
        const float a = cross(v.vertices[t.a], v.vertices[t.b], p);
        const float b = cross(v.vertices[t.b], v.vertices[t.c], p);
        const float c = cross(v.vertices[t.c], v.vertices[t.a], p);
        if ((a >= 0 && b >= 0 && c >= 0) || (a <= 0 && b <= 0 && c <= 0)) {
            return true;
        }
    }
    return false;
}

struct Presentation final {
    std::array<std::uint32_t, kDialogueRows> generations{};
    std::uint32_t objective{kObjectives[0]};
    std::uint8_t activeRow{kNoDialogue};
    IntroCommand intro{};
    std::uint32_t bossGeneration{};
};

/** Presentation and the reveal actor request. Combat, doors and encounter completion remain separate. */
class Run final {
public:
    void start(std::uint64_t generation, std::uint64_t now, Landmark arrival) noexcept {
        initialize(generation);
        enter(arrival, now);
    }
    // Start without choosing authored cues; the selected executor publishes them.
    void initialize(std::uint64_t generation,bool executorOwned=false) noexcept {
        *this = {};
        generation_ = generation;
        intro_.reset(generation,executorOwned);
    }
    [[nodiscard]] std::uint64_t generation() const noexcept { return generation_; }
    [[nodiscard]] const Presentation& presentation() const noexcept { return presentation_; }
    [[nodiscard]] std::uint32_t revision() const noexcept { return revision_; }
    [[nodiscard]] std::uint32_t timed_out() const noexcept { return timedOut_; }
    [[nodiscard]] std::uint8_t cycle() const noexcept { return cycle_; }
    /** A kill alone must not cut off Osiris's final gameplay line. Submission
     * owns its native delay and clip duration; a missing receipt never passes. */
    [[nodiscard]] bool ending_dialogue_finished(std::uint64_t now) const noexcept {
        return defeated_ && finalDialogueSubmitted_ && now >= voiceUntil_
            && presentation_.activeRow == kNoDialogue;
    }
    [[nodiscard]] Landmark landmark() const noexcept { return static_cast<Landmark>(landmark_); }
    [[nodiscard]] NavigationGoal navigation_goal() const noexcept { return navigationGoal_; }
    [[nodiscard]] IntroPhase intro_phase() const noexcept { return intro_.phase(); }
    void request_intro(std::uint64_t now) noexcept {
        if (started_ && landmark_ < static_cast<unsigned>(Landmark::arena)) {
            intro_.request(now);
        }
    }
    bool observe_boss(std::uint64_t run, std::uint32_t definition, std::uint32_t object,
                      std::uint32_t nativeGeneration) noexcept {
        if (started_ && run == generation_ && presentation_.bossGeneration != 0
            && nativeGeneration == presentation_.bossGeneration
            && definition == kBossEntity && object != UINT32_MAX) { bossReady_ = true; return true; }
        return false;
    }
    [[nodiscard]] bool boss_ready() const noexcept { return bossReady_; }
    [[nodiscard]] bool claim_boss_intro_action(std::uint64_t run) noexcept {
        if (!started_ || run != generation_ || !bossReady_ || bossIntroAction_
            || intro_.phase() != IntroPhase::priming) { return false; }
        bossIntroAction_ = true;
        return true;
    }
    /** The native caller validates the complete member/character/graph identity and
     * loaded fly node. This latch cannot be supplied by a timer or camera receipt. */
    [[nodiscard]] bool observe_boss_flight(std::uint64_t run, std::uint32_t nativeGeneration) noexcept {
        if (!started_ || run != generation_ || !bossReady_ || !bossIntroAction_ || bossFlightReady_
            || intro_.phase() != IntroPhase::priming || presentation_.bossGeneration == 0
            || nativeGeneration != presentation_.bossGeneration) { return false; }
        bossFlightReady_ = true;
        return true;
    }
    [[nodiscard]] bool boss_flight_ready() const noexcept { return bossFlightReady_; }
    void observe_intro(std::uint32_t revision, bool active, bool ready, std::uint64_t now) noexcept {
        observe_intro(revision, active, ready, now, bossFlightReady_);
    }
    // Deferred receipts retain whether the synchronous native flight claim had
    // happened at intake. A later claim cannot release an earlier camera receipt.
    void observe_intro(std::uint32_t revision, bool active, bool ready, std::uint64_t now,
                       bool flightReadyAtReceipt) noexcept {
        if (!started_) { return; }
        const auto before = intro_.command();
        const auto phaseBefore = intro_.phase();
        const bool dialogueDue = std::any_of(queue_.begin(), queue_.end(),
            [now](const auto& cue) noexcept { return cue.used && now >= cue.due; });
        intro_.observe(revision, active, ready,
            presentation_.activeRow == kNoDialogue && now >= voiceUntil_
                && (phaseBefore == IntroPhase::priming || !dialogueDue), flightReadyAtReceipt && bossFlightReady_, now);
        sync_intro(before);
        if (phaseBefore == IntroPhase::waiting && intro_.phase() == IntroPhase::priming) {
            // The authored approach, camera registration and dialogue gates are ready.
            // Prepare the native actor/graph; its loaded fly node releases the camera.
            request_boss();
        }
        if (phaseBefore == IntroPhase::playing && intro_.phase() == IntroPhase::complete) {
            // Ghost reacts after the native camera has finished (including a native skip).
            // Arrival, a rejected start, and an abort timeout are not completion receipts.
            enqueue(12,now,250);
        }
    }
    /** Reaching a fixed point consumes it for this run. Later landmarks also skip already
     * completed approach points, including direct arrivals and walks during streaming. */
    void observe_navigation(Point player) noexcept {
        if (!started_ || !std::isfinite(player.x) || !std::isfinite(player.y)
            || !std::isfinite(player.z)) { return; }
        Point target{};
        if (!navigation_point(navigationGoal_, target)) { return; }
        const float x=player.x-target.x, y=player.y-target.y, z=player.z-target.z;
        if (x*x+y*y+z*z <= 7.0F*7.0F) {
            navigationGoal_=static_cast<NavigationGoal>(static_cast<unsigned>(navigationGoal_)+1);
        }
    }
    [[nodiscard]] bool forest_complete() const noexcept {
        return started_ && landmark_ >= static_cast<std::uint8_t>(Landmark::forestExit);
    }

    // Shared traversal service: update route and discard only unpublished older cues.
    bool traverse(std::uint8_t rank) noexcept {
        if (rank>static_cast<std::uint8_t>(Landmark::arena)) { return false; }
        if (started_ && rank <= landmark_) { return false; }
        const auto landmark = static_cast<Landmark>(rank);
        started_ = true;
        landmark_ = rank;
        // Direct arena arrivals bypass the entry reveal; preserve their boss request.
        if (landmark == Landmark::arena) { request_boss(); }
        constexpr std::array goals{NavigationGoal::ikora, NavigationGoal::forestEntrance,
            NavigationGoal::forestGates, NavigationGoal::lairApproach,
            NavigationGoal::lairEntry, NavigationGoal::complete};
        navigationGoal_ = (std::max)(navigationGoal_, goals[rank]);
        // Do not narrate scenery the player has already passed. An already submitted clip ends
        // under native audio ownership; only unpublished work is discarded here.
        for (auto& cue : queue_) {
            if (cue.used && cue.landmark < rank) { cue.used = false; }
        }
        // An offered row may already be in a packet in flight. Keep it until the native receipt
        // so the next row cannot overtake it and lose its audio arbitration deadline.
        return true;
    }

    void enter(Landmark landmark, std::uint64_t now) noexcept {
        if (!traverse(static_cast<std::uint8_t>(landmark))) { return; }
        switch (landmark) {
        case Landmark::lighthouse: enqueue(0, now); break;
        case Landmark::tunnel: set_objective(kObjectives[1]); enqueue(6, now, 1500); break;
        case Landmark::forestVista: set_objective(kObjectives[1]); enqueue(7, now); break;
        case Landmark::forestExit: set_objective(kObjectives[1]); enqueue(9, now); break;
        case Landmark::lair: set_objective(kObjectives[2]); break;
        case Landmark::arena:
            intro_.passed(); presentation_.intro = intro_.command();
            set_objective(kObjectives[3]); enqueue(13, now); break;
        }
    }

    /** Inputs must describe observed mechanics. Cycle is 1..3, never inferred from elapsed time. */
    void encounter(Encounter event, std::uint8_t cycle, std::uint64_t now) noexcept {
        if (!started_ || landmark_ < static_cast<std::uint8_t>(Landmark::arena)
            || cycle < 1 || cycle > 3 || cycle < cycle_ || cinematic_
            || static_cast<unsigned>(event) > static_cast<unsigned>(Encounter::cinematic)) { return; }
        if (defeated_ && event != Encounter::cinematic) { return; }
        if (event == Encounter::arcReminder && presentation_.objective != kObjectives[5]) { return; }
        constexpr std::array<std::uint8_t,10> stage{1,2,2,3,4,4,5,6,7,8};
        if (stage[static_cast<unsigned>(event)] < mechanicStage_[cycle-1U]) { return; }
        const std::uint64_t bit = 1ULL << (static_cast<unsigned>(event) + (cycle-1U)*10U);
        if ((encounters_ & bit) != 0) { return; }
        encounters_ |= bit;
        cycle_ = cycle;
        mechanicStage_[cycle-1U] = stage[static_cast<unsigned>(event)];
        switch (event) {
        case Encounter::defenses: set_objective(kObjectives[3]); break;
        case Encounter::deletion: if (cycle == 1) { enqueue(14, now); } break;
        case Encounter::osirisArrives: enqueue(15, now); break;
        case Encounter::osirisHolds:
            if (cycle == 1) { enqueue(16, now); }
            else if (cycle == 2) { enqueue(25, now); enqueue(26, now, 5840); }
            else { enqueue(30, now); }
            break;
        case Encounter::arcReady:
            set_objective(kObjectives[5]);
            if (cycle == 1) { enqueue(18, now); }
            break;
        case Encounter::arcReminder:
            // The encounter owner decides that the charge is still available. No timed replay.
            enqueue(cycle == 3 ? 31 : 21, now);
            break;
        case Encounter::eyeVulnerable:
            set_objective(kObjectives[4]);
            if (cycle == 1) { enqueue(22, now); }
            else if (cycle == 3) { enqueue(32, now); }
            break;
        case Encounter::pursuit: set_objective(kObjectives[6]); enqueue(29, now); break;
        case Encounter::defeated: defeated_ = true; enqueue(33, now); break;
        case Encounter::cinematic:
            cinematic_ = true;
            queue_ = {};
            presentation_.activeRow = kNoDialogue;
            ++revision_;
            break;
        }
    }

    /**
     * Mirror of the encounter controller's qualified Crown stage. The controller advances
     * CrownStage only on accepted native animation/Scene/charge/health receipts, so this is
     * real encounter state, not elapsed time. It is the only tree producer for Ghost's
     * deletion panic (row 14), the Arc objective/callout (85A8F583, row 18), the eye
     * objective/callouts (A41DE99B, rows 22/32), the relocation pursuit (DF97334D, row 29)
     * and the per-cycle return to "Overcome Panoptes's defenses". Events are emitted in
     * authored stage order so a late first sample still queues every implied line once;
     * `encounter()` deduplicates and never regresses a later same-cycle stage.
     * Retail: D10 at deletion start (387 s), D14 after Osiris's hold line, D15 1.3 s after
     * the shield detonation, D18 as Panoptes leaves its throne (546 s).
     * `defeated` here is a fallback for the native death producer: CrownStage::ending needs
     * both the qualified native death and the terminal death-animation receipt.
     */
    void sync_encounter(std::uint8_t cycle, omega_first_lair::CrownStage stage,
                        std::uint64_t now) noexcept {
        using Stage = omega_first_lair::CrownStage;
        if (!started_ || cycle < 1 || cycle > 3
            || static_cast<unsigned>(stage) > static_cast<unsigned>(Stage::finished)) { return; }
        const auto reached = [stage](Stage minimum) noexcept {
            return static_cast<unsigned>(stage) >= static_cast<unsigned>(minimum);
        };
        if (reached(Stage::waves)) { encounter(Encounter::defenses, cycle, now); }
        if (reached(Stage::rescue)) { encounter(Encounter::deletion, cycle, now); }
        if (reached(Stage::route)) { encounter(Encounter::arcReady, cycle, now); }
        // Osiris announces the clear shot at the accepted dunk (breakShield scheduled), before
        // the native eye-exposure animation finishes; the objective follows the same receipt.
        if (reached(Stage::eyeOpening)) { encounter(Encounter::eyeVulnerable, cycle, now); }
        // Ghost's chase line belongs to the boss actually leaving its throne (relocateFinal),
        // not to the overlapping Cabal escape cohort that precedes it.
        if (cycle == 2 && (stage == Stage::relocation || stage == Stage::finalArrival)) {
            encounter(Encounter::pursuit, cycle, now);
        }
        if (cycle == 3 && reached(Stage::ending)) { encounter(Encounter::defeated, cycle, now); }
    }

    /** Native Scene authority identities; a seeded inactive component is not a scene start. */
    void scene(std::uint32_t definition, bool active, std::uint64_t now) noexcept {
        if (!active || landmark_ < static_cast<std::uint8_t>(Landmark::arena)) { return; }
        switch (definition) {
        case 0x80F479BFU:
            if (cycle_ > 1 || defeated_) { break; }
            encounter(Encounter::osirisArrives, 1, now);
            // The two introductory Osiris calls are 6.4 seconds apart in the retail capture.
            // This is spacing within one observed scene, never a timer that advances mechanics.
            if ((encounters_ & (1ULL << static_cast<unsigned>(Encounter::osirisHolds))) == 0) {
                encounters_ |= 1ULL << static_cast<unsigned>(Encounter::osirisHolds);
                enqueue(16, now, 6400);
            }
            break;
        case 0x80F479F5U: encounter(Encounter::osirisHolds, 2, now); break;
        case 0x80F47A08U: encounter(Encounter::osirisHolds, 3, now); break;
        default: break;
        }
    }

    void advance(std::uint64_t now) noexcept {
        const auto introBefore = intro_.command();
        intro_.advance(now);
        sync_intro(introBefore);
        if (presentation_.activeRow != kNoDialogue) {
            // No dispatch acknowledgement means the component/bank is not ready. Keep the same
            // generation for a bounded retry window; never flood the native ten-second queue.
            if (now - offeredAt_ < 15000) { return; }
            presentation_.activeRow = kNoDialogue;
            ++timedOut_;
            ++revision_;
        }
        if (now < voiceUntil_ || cinematic_ || intro_.busy()) { return; }
        Cue* next = nullptr;
        for (auto& cue : queue_) {
            if (cue.used && now >= cue.due && (next == nullptr || cue.order < next->order)) {
                next = &cue;
            }
        }
        if (next == nullptr) { return; }
        presentation_.activeRow = next->row;
        // Positive and different after an orbit/relaunch even if the native sensor survives.
        presentation_.generations[next->row] = static_cast<std::uint32_t>(generation_ % 0x7FFFFFFEULL)+1;
        offeredAt_ = now;
        next->used = false;
        ++revision_;
    }

    /** Submission acknowledgement, not proof that the audio was audible. */
    void submitted(std::uint32_t bank, std::uint8_t row, std::uint32_t generation,
                   std::uint64_t now) noexcept {
        if (bank != kDialogueBank || row >= kDialogueRows || presentation_.activeRow != row
            || presentation_.generations[row] != generation) { return; }
        voiceUntil_ = now + kDialogue[row].durationMs + kDialogue[row].delayMs + 250;
        if (row == 33 && defeated_) { finalDialogueSubmitted_ = true; }
        presentation_.activeRow = kNoDialogue;
        ++revision_;
    }

private:
    void request_boss() noexcept {
        if (presentation_.bossGeneration != 0) { return; }
        presentation_.bossGeneration = static_cast<std::uint32_t>(generation_ & 0x7FFFFFFFU);
        if (presentation_.bossGeneration == 0) { presentation_.bossGeneration = 1; }
        ++revision_;
    }
    void sync_intro(IntroCommand before) noexcept {
        presentation_.intro = intro_.command();
        if (before.revision != presentation_.intro.revision || before.play != presentation_.intro.play) {
            ++revision_;
        }
    }
    struct Cue final {
        std::uint64_t due{};
        std::uint32_t order{};
        std::uint8_t row{}, landmark{};
        bool used{};
    };
public:
    // Shared dialogue/objective service entry points. Their accepted scheduling
    // and stale-cue rules are also used by the remaining encounter controller.
    void enqueue(std::uint8_t row, std::uint64_t now, std::uint64_t delay = 0) noexcept {
        if (row >= kDialogueRows || kDialogue[row].durationMs == 0 || kDialogue[row].sceneOwned
            || (requested_ & (1ULL << row)) != 0) { return; }
        for (auto& cue : queue_) {
            if (!cue.used) {
                cue = {now+delay, ++order_, row, landmark_, true};
                requested_ |= 1ULL << row;
                ++revision_;
                return;
            }
        }
    }
    void set_objective(std::uint32_t event) noexcept {
        if (presentation_.objective != event) {
            presentation_.objective = event;
            const auto stale = [event](std::uint8_t row) noexcept {
                return ((row == 18 || row == 21 || row == 31) && event != kObjectives[5])
                    || ((row == 22 || row == 32) && event != kObjectives[4]);
            };
            for (auto& cue : queue_) { if (cue.used && stale(cue.row)) { cue.used = false; } }
            ++revision_;
        }
    }
private:
    Presentation presentation_{};
    coo::reveal::Intro intro_{};
    bool bossReady_{};
    bool bossIntroAction_{};
    bool bossFlightReady_{};
    std::array<Cue, kDialogueRows> queue_{};
    std::array<std::uint8_t,3> mechanicStage_{};
    std::uint64_t generation_{}, requested_{}, encounters_{}, offeredAt_{}, voiceUntil_{};
    std::uint32_t revision_{}, order_{}, timedOut_{};
    std::uint8_t landmark_{}, cycle_{1};
    NavigationGoal navigationGoal_{};
    bool started_{}, defeated_{}, cinematic_{}, finalDialogueSubmitted_{};
};

} // namespace dawn::state::activity::omega_presentation
