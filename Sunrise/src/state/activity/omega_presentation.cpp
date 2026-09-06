#include <Windows.h>

#include <array>
#include <cstdio>

#include "omega_presentation.h"
#include "coo/omega_adapter.h"
#include "coo/omega_forest_controller.h"
#include "omega_first_lair_runtime.h"
#include "omega_ending.h"
#include "omega_presentation_volumes.h"
#include "omega_lair_chase_geometry.h"
#include "runtime.h"
#include "../../core/logging/log.h"

namespace sunrise::state::activity::omega_presentation {
namespace {
SRWLOCK g_lock = SRWLOCK_INIT;
namespace forest = coo::omega::forest;
forest::Controller g_controller;
Run& g_run = g_controller.state();
std::uint32_t g_forestComplete{UINT32_MAX};
coo::Phase g_forestPhase{coo::Phase::idle};
bool g_enabled{};
std::uint64_t g_lastPublication{};
std::uint32_t g_publishedRevision{};
std::uint32_t g_loggedTimeouts{};

bool admitted() noexcept {
    return g_enabled && g_run.generation() == mission_run_generation()
           && mission_seed_armed() && !omega_authority_quiesced()
           && world_phase() == WorldPhase::arrived;
}
void log_snapshot(std::uint64_t now) noexcept {
    const auto& p = g_run.presentation();
    std::array<char, 352> line{};
    const int size = std::snprintf(line.data(), line.size(),
        "ev=omega_presentation stage=publish run=%llu revision=%u objective=%08X "
        "row=%u selector=%08X cycle=%u dispatch_timeouts=%u tick=%llu intro_phase=%u intro_revision=%u intro_play=%u",
        static_cast<unsigned long long>(g_run.generation()), g_run.revision(), p.objective,
        static_cast<unsigned>(p.activeRow), p.activeRow < kDialogueRows ? kDialogue[p.activeRow].selector : 0,
        static_cast<unsigned>(g_run.cycle()), g_run.timed_out(), static_cast<unsigned long long>(now),
        static_cast<unsigned>(g_run.intro_phase()),p.intro.revision,p.intro.play?1U:0U);
    if (size > 0) {
        core::log::write(core::log::Channel::server, core::log::Level::info,
                        {line.data(), static_cast<std::size_t>(size)});
    }
}
// Called after application, never while native callbacks publish mission work.
void log_applied(const forest::Receipt& receipt, forest::Before before) noexcept {
    std::array<char, 384> line{};
    int size{};
    switch (receipt.kind) {
    case forest::Kind::position:
        if (before.introPhase != g_run.intro_phase()) {
            size = std::snprintf(line.data(),line.size(),
                "ev=omega_intro stage=volume registry=A3928C71 slot=60/4 observed_tick=%llu",
                static_cast<unsigned long long>(receipt.now));
        }
        break;
    case forest::Kind::submission:
        if (before.revision != g_run.revision()) {
            size = std::snprintf(line.data(),line.size(),
                "ev=omega_presentation stage=receipt run=%llu bank=%08X row=%u generation=%u observed_tick=%llu",
                static_cast<unsigned long long>(g_run.generation()), receipt.identity,
                static_cast<unsigned>(receipt.row), receipt.generation, static_cast<unsigned long long>(receipt.now));
        }
        break;
    case forest::Kind::scene:
        if (before.revision != g_run.revision()) {
            size = std::snprintf(line.data(),line.size(),
                "ev=omega_presentation stage=scene definition=%08X run=%llu cycle=%u observed_tick=%llu",
                receipt.identity, static_cast<unsigned long long>(g_run.generation()),
                static_cast<unsigned>(g_run.cycle()), static_cast<unsigned long long>(receipt.now));
        }
        break;
    case forest::Kind::intro:
        if (before.introPhase != g_run.intro_phase() || before.intro.revision != g_run.presentation().intro.revision) {
            size = std::snprintf(line.data(),line.size(),
                "ev=omega_intro stage=state run=%llu phase=%u observed=%u active=%u ready=%u command=%u play=%u boss_ready=%u observed_tick=%llu",
                static_cast<unsigned long long>(g_run.generation()),static_cast<unsigned>(g_run.intro_phase()),
                receipt.generation,receipt.active?1U:0U,receipt.ready?1U:0U,g_run.presentation().intro.revision,
                g_run.presentation().intro.play?1U:0U,g_run.boss_ready()?1U:0U,
                static_cast<unsigned long long>(receipt.now));
        }
        break;
    default: break;
    }
    if (size > 0 && static_cast<std::size_t>(size) < line.size()) {
        core::log::write(core::log::Channel::client,core::log::Level::info,
                        {line.data(),static_cast<std::size_t>(size)});
    }
}
void observe_locked(forest::Receipt receipt) noexcept {
    const forest::Before before{g_run.revision(),g_run.intro_phase(),g_run.presentation().intro};
    g_controller.observe(receipt);
    if (!g_controller.selected()) { log_applied(receipt,before); }
}
void drain_locked() noexcept {
    g_controller.drain(log_applied);
    if (!g_controller.selected()) { return; }
    const auto d = g_controller.diagnostics();
    if (d.complete == g_forestComplete && d.phase == g_forestPhase) { return; }
    std::array<char,384> line{};
    const int size = std::snprintf(line.data(),line.size(),
        "ev=coo_forest run=%llu incarnation=%llu phase=%u active=%08X complete=%08X skipped=%X failure=%u landmark=%u goal=%u",
        static_cast<unsigned long long>(d.run),static_cast<unsigned long long>(d.incarnation),
        static_cast<unsigned>(d.phase),d.active,d.complete,g_controller.skipped(),static_cast<unsigned>(d.failure),
        static_cast<unsigned>(g_run.landmark()),static_cast<unsigned>(g_run.navigation_goal()));
    if (size > 0 && static_cast<std::size_t>(size) < line.size()) {
        core::log::write(core::log::Channel::server,core::log::Level::info,
                        {line.data(),static_cast<std::size_t>(size)});
    }
    g_forestComplete = d.complete; g_forestPhase = d.phase;
}
/** Encounter-controller stage for the current run, read outside the presentation lock so
 * the two locks are never nested. A disabled/failed controller or another run yields none. */
struct EncounterMirror final { bool valid{}; std::uint8_t cycle{}; omega_first_lair::CrownStage stage{}; };
EncounterMirror encounter_mirror(std::uint64_t run) noexcept {
    const auto status = omega_first_lair::status(run);
    if (!status.enabled || status.failed || !status.token.valid() || status.boss.run != run) { return {}; }
    return {true, status.cycle, status.crownStage};
}
/** Caller holds g_lock. Mirrors the qualified stage and logs each resulting revision change. */
void sync_encounter_locked(const EncounterMirror& mirror, std::uint64_t now) noexcept {
    if (!mirror.valid) { return; }
    const auto before = g_run.revision();
    g_run.sync_encounter(mirror.cycle, mirror.stage, now);
    if (before == g_run.revision()) { return; }
    const auto& p = g_run.presentation();
    std::array<char, 224> line{};
    const int size = std::snprintf(line.data(), line.size(),
        "ev=omega_presentation stage=encounter run=%llu cycle=%u crown_stage=%u revision=%u objective=%08X source=controller",
        static_cast<unsigned long long>(g_run.generation()), static_cast<unsigned>(mirror.cycle),
        static_cast<unsigned>(mirror.stage), g_run.revision(), p.objective);
    if (size > 0 && static_cast<std::size_t>(size) < line.size()) {
        core::log::write(core::log::Channel::server, core::log::Level::info,
                        {line.data(), static_cast<std::size_t>(size)});
    }
}
} // namespace

Presentation snapshot(std::uint64_t run, std::uint64_t now, int region, bool entrance, bool executor) noexcept {
    const auto mirror = encounter_mirror(run);
    AcquireSRWLockExclusive(&g_lock);
    if (world_phase() != WorldPhase::arrived || !mission_seed_armed() || omega_authority_quiesced()) {
        auto result = g_run.presentation();
        result.activeRow = kNoDialogue;
        ReleaseSRWLockExclusive(&g_lock);
        return result;
    }
    if (!g_enabled || g_run.generation() != run) {
        const auto arrival = region >= 112 && region < 120 ? Landmark::lair
                           : (entrance || (region >= 64 && region <= 104)) ? Landmark::tunnel
                           : Landmark::lighthouse;
        g_controller.start(run, now, arrival, executor);
        g_forestComplete = UINT32_MAX;
        g_forestPhase = coo::Phase::idle;
        g_enabled = true;
        g_publishedRevision = 0;
        g_loggedTimeouts = 0;
    }
    drain_locked();
    now = g_controller.update_time(now);
    if (!g_controller.failed()) {
        if (entrance) { g_controller.entrance(now); }
        sync_encounter_locked(mirror, now);
        g_run.advance(now);
    }
    if (g_publishedRevision != g_run.revision() || g_loggedTimeouts != g_run.timed_out()) {
        log_snapshot(now);
        g_loggedTimeouts = g_run.timed_out();
    }
    auto result = g_run.presentation();
    if (g_controller.failed()) { result.activeRow = kNoDialogue; }
    g_publishedRevision = g_run.revision();
    g_lastPublication = now;
    ReleaseSRWLockExclusive(&g_lock);
    return result;
}

void observe_position(Point p) noexcept {
    std::uint64_t chaseRun{};
    AcquireSRWLockExclusive(&g_lock);
    if (admitted()) {
        chaseRun=mission_run_generation();
        const auto now = GetTickCount64();
        observe_locked({{}, now, forest::Kind::position, p});
    }
    ReleaseSRWLockExclusive(&g_lock);
    // The encounter lock is independent of presentation. Authored type31
    // approach volumes feed monotonic per-run arrival latches, not landmarks.
    if(chaseRun!=0) {
        for(std::size_t i=0;i<omega_lair_chase_geometry::kChaseArrivals.size();++i) {
            if(contains(omega_lair_chase_geometry::kChaseArrivals[i],p)) {
                static_cast<void>(omega_first_lair::observe_arrival(chaseRun,static_cast<std::uint8_t>(i+1)));
            }
        }
        if(contains(omega_lair_chase_geometry::kCrownArrivalVolume,p)) {
            static_cast<void>(omega_first_lair::observe_crown_arrival(chaseRun));
        }
    }
}

void observe_submission(std::uint32_t bank, std::uint8_t row, std::uint32_t generation) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    // A submitted packet can finish during a walked streaming transition. Its receipt must still
    // retire the offered row, even though no new scene/volume cue is accepted during the load.
    if (g_enabled && g_run.generation() == mission_run_generation()
        && mission_seed_armed() && !omega_authority_quiesced()) {
        forest::Receipt receipt{{}, GetTickCount64(), forest::Kind::submission};
        receipt.identity = bank; receipt.row = row; receipt.generation = generation;
        observe_locked(receipt);
    }
    ReleaseSRWLockExclusive(&g_lock);
}

void observe_scene(std::uint32_t definition, bool active) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    if (admitted()) {
        forest::Receipt receipt{{}, GetTickCount64(), forest::Kind::scene};
        receipt.identity = definition; receipt.active = active;
        observe_locked(receipt);
    }
    ReleaseSRWLockExclusive(&g_lock);
}

void note_encounter(std::uint64_t run, Encounter event, std::uint8_t cycle) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    if (admitted() && g_run.generation() == run) {
        forest::Receipt receipt{{}, GetTickCount64(), forest::Kind::encounter};
        receipt.identity = static_cast<std::uint32_t>(event); receipt.cycle = cycle;
        observe_locked(receipt);
    }
    ReleaseSRWLockExclusive(&g_lock);
}

bool observe_boss(std::uint64_t run, std::uint32_t definition, std::uint32_t object,
                  std::uint32_t nativeGeneration) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    bool accepted=false;
    if (g_enabled && g_run.generation() == mission_run_generation() && mission_seed_armed()
        && !omega_authority_quiesced()) {
        const bool before = g_run.boss_ready();
        accepted=g_run.observe_boss(run, definition, object, nativeGeneration);
        if (!before && g_run.boss_ready()) {
            std::array<char,160> line{};
            const int size = std::snprintf(line.data(), line.size(),
                "ev=omega_boss stage=actor_ready run=%llu definition=%08X object=%08X generation=%u source=native_member",
                static_cast<unsigned long long>(run), definition, object, nativeGeneration);
            if (size > 0 && static_cast<std::size_t>(size) < line.size()) {
                core::log::write(core::log::Channel::client, core::log::Level::info,
                    {line.data(),static_cast<std::size_t>(size)});
            }
        }
    }
    ReleaseSRWLockExclusive(&g_lock);
    return accepted;
}

bool claim_boss_intro_action(std::uint64_t run) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    const bool claimed = admitted() && g_run.claim_boss_intro_action(run);
    ReleaseSRWLockExclusive(&g_lock);
    return claimed;
}

bool observe_boss_flight(std::uint64_t run, std::uint32_t nativeGeneration) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    const bool accepted = admitted() && g_run.observe_boss_flight(run,nativeGeneration);
    if (accepted) {
        std::array<char,160> line{};
        const int size=std::snprintf(line.data(),line.size(),
            "ev=omega_intro stage=flight_ready run=%llu generation=%u source=native_graph",
            static_cast<unsigned long long>(run),nativeGeneration);
        if(size>0 && static_cast<std::size_t>(size)<line.size()) {
            core::log::write(core::log::Channel::client,core::log::Level::info,
                            {line.data(),static_cast<std::size_t>(size)});
        }
    }
    ReleaseSRWLockExclusive(&g_lock);
    return accepted;
}

void observe_intro(std::uint32_t revision, bool active, bool ready) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    if (admitted()) {
        forest::Receipt receipt{{}, GetTickCount64(), forest::Kind::intro};
        receipt.generation = revision; receipt.active = active; receipt.ready = ready;
        observe_locked(receipt);
    }
    ReleaseSRWLockExclusive(&g_lock);
}

Navigation navigation() noexcept {
    AcquireSRWLockShared(&g_lock);
    const bool enabled = g_enabled && g_run.generation() == mission_run_generation()
                         && mission_seed_armed() && !omega_authority_quiesced()
                         && world_phase() != WorldPhase::idle && !g_controller.failed();
    const Navigation result{g_run.generation(), g_run.landmark(), enabled,
                            enabled && g_run.forest_complete(), g_run.navigation_goal()};
    ReleaseSRWLockShared(&g_lock);
    return result;
}

bool ending_dialogue_finished(std::uint64_t run,std::uint64_t now) noexcept {
    AcquireSRWLockShared(&g_lock);
    const bool ready=g_enabled && run==g_run.generation() && run==mission_run_generation()
        && mission_seed_armed() && !omega_authority_quiesced()
        && g_run.ending_dialogue_finished(now);
    ReleaseSRWLockShared(&g_lock);
    return ready;
}

bool publication_due(std::uint64_t now) noexcept {
    // The controller's own publication_due wakes the same keepalive; mirroring here as well
    // lets a changed encounter stage request the presentation frame without a roster pass.
    const auto mirror = encounter_mirror(mission_run_generation());
    AcquireSRWLockExclusive(&g_lock);
    if (admitted()) {
        drain_locked();
        now = g_controller.update_time(now);
        if (!g_controller.failed()) { sync_encounter_locked(mirror, now); g_run.advance(now); }
    }
    const bool due = admitted() && now >= g_lastPublication+250
                     && (g_run.revision() != g_publishedRevision
                         || g_run.presentation().activeRow != kNoDialogue);
    ReleaseSRWLockExclusive(&g_lock);
    return due;
}

void reset() noexcept {
    coo::omega::reset();
    omega_ending::reset();
    omega_first_lair::reset();
    AcquireSRWLockExclusive(&g_lock);
    g_enabled = false;
    g_controller.reset();
    g_forestComplete = UINT32_MAX; g_forestPhase = coo::Phase::idle;
    g_lastPublication = 0;
    g_publishedRevision = 0;
    ReleaseSRWLockExclusive(&g_lock);
}

} // namespace sunrise::state::activity::omega_presentation
