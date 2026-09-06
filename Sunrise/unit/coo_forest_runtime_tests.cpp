#include <Windows.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include "state/activity/coo/omega_adapter.h"
#include "state/activity/runtime.h"
#include "state/activity/omega_presentation_volumes.h"
#include "core/logging/log.h"

namespace activity = sunrise::state::activity;
namespace present = activity::omega_presentation;
namespace omega = activity::coo::omega;
namespace fake {
std::atomic_uint64_t run{1};
std::atomic_bool seed{true}, quiesced{false};
std::atomic<activity::WorldPhase> phase{activity::WorldPhase::arrived};
std::atomic_uint publications{}, forestLogs{}, failures{}, receipts{};
}
// Compile the actual adapter and presentation .cpp files. Only game/encounter/
// logging dependencies are substituted; callbacks, SRW locks, mode selection,
// update ordering and reset are the production implementations.
namespace sunrise::state::activity {
std::uint64_t mission_run_generation() noexcept { return fake::run.load(); }
bool mission_seed_armed() noexcept { return fake::seed.load(); }
bool omega_authority_quiesced() noexcept { return fake::quiesced.load(); }
WorldPhase world_phase() noexcept { return fake::phase.load(); }
namespace omega_first_lair {
Authority authority(std::uint64_t,std::uint32_t,bool) noexcept { ++fake::publications; return {}; }
Status status(std::uint64_t) noexcept { return {}; }
bool observe_arrival(std::uint64_t,std::uint8_t) noexcept { return false; }
bool observe_crown_arrival(std::uint64_t) noexcept { return false; }
void reset() noexcept {}
}
namespace omega_ending {
bool request(Token,bool) noexcept { return false; }
Authority authority(std::uint64_t,std::uint64_t) noexcept { return {}; }
Handoff handoff_status(std::uint64_t) noexcept { return {}; }
void reset() noexcept {}
}
}
namespace sunrise::core::log {
void write(Channel,Level,std::string_view text) noexcept {
    if (text.starts_with("ev=coo_forest")) { ++fake::forestLogs; }
    if (text.find("failure=")!=text.npos && text.find("failure=0")==text.npos) { ++fake::failures; }
    if (text.starts_with("ev=omega_presentation stage=receipt")) { ++fake::receipts; }
}
}
unsigned checks{};
#define CHECK(value) do { ++checks; if (!(value)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#value); std::exit(1); } } while(false)
present::Point inside(const present::Volume& v) {
    const auto t=v.triangles[0]; const auto a=v.vertices[t.a],b=v.vertices[t.b],c=v.vertices[t.c];
    return {(a.x+b.x+c.x)/3,(a.y+b.y+c.y)/3,(v.minimum.z+v.maximum.z)/2};
}
omega::Frame update(int region=120,bool entrance=false,bool selected=true) {
    return omega::update({fake::run.load(),GetTickCount64(),region,entrance,selected});
}
int main() {
    CHECK(omega::select(1,true));
    // Publication sees the latched selection even if a later settings reload disagrees.
    const auto frame=update(120,false,false); CHECK(frame.presentation.activeRow==0);
    const auto count=fake::publications.load(); CHECK(count==1); CHECK(fake::forestLogs==1);
    for (unsigned i=0;i<10000;++i) {
        present::observe_scene(0x80EC0F95U,true);
        present::observe_scene(0x80F479BFU,false);
    }
    static_cast<void>(present::publication_due(GetTickCount64()));
    CHECK(fake::failures==0);
    const auto before=present::navigation(); CHECK(before.landmark==present::Landmark::lighthouse);
    present::observe_position(inside(present::kVolumes[3]));
    CHECK(present::navigation().landmark==present::Landmark::lighthouse); // Callback only queued.
    auto next=update(120,false,false); CHECK(fake::publications==count+1);
    CHECK(present::navigation().landmark==present::Landmark::forestExit);
    CHECK(next.presentation.objective==present::kObjectives[1]);
    CHECK(next.presentation.activeRow==0); // In-flight opening line retains its ownership.
    // Streaming suppresses new volumes but still accepts the offered native audio receipt.
    fake::phase=activity::WorldPhase::idle;
    present::observe_position(inside(present::kVolumes[4]));
    present::observe_submission(present::kDialogueBank,0,frame.presentation.generations[0]);
    CHECK(fake::receipts==0);
    fake::phase=activity::WorldPhase::arrived;
    next=update(); CHECK(fake::receipts==1); CHECK(next.presentation.activeRow==present::kNoDialogue);
    CHECK(present::navigation().landmark==present::Landmark::forestExit);
    present::observe_position(inside(present::kVolumes[4]));
    static_cast<void>(present::publication_due(GetTickCount64()));
    CHECK(present::navigation().landmark==present::Landmark::lair);
    next=update(); CHECK(next.presentation.objective==present::kObjectives[2]);
    // Orbit discards queued work. Same numeric run id must get a fresh incarnation.
    present::observe_position(inside(present::kVolumes[5])); present::reset();
    next=update(); CHECK(present::navigation().landmark==present::Landmark::lighthouse);
    CHECK(next.presentation.bossGeneration==0);
    fake::quiesced=true; present::observe_position(inside(present::kVolumes[3]));
    static_cast<void>(update()); fake::quiesced=false; static_cast<void>(update());
    CHECK(present::navigation().landmark==present::Landmark::lighthouse);
    // Concurrent native intake/reads and actual adapter publication exercise lock
    // order. No callback enters the adapter mutex while holding presentation's lock.
    std::atomic_bool done{};
    std::thread callbacks([&] {
        for (unsigned i=0;i<1000;++i) {
            present::observe_position(inside(present::kVolumes[2]));
            static_cast<void>(present::navigation());
            if (i%10==0) { std::this_thread::yield(); }
        }
        done=true;
    });
    for (unsigned i=0;i<1000 || !done.load();++i) {
        static_cast<void>(update()); static_cast<void>(present::publication_due(GetTickCount64()));
    }
    callbacks.join(); static_cast<void>(update()); CHECK(fake::failures==0);
    CHECK(present::navigation().landmark==present::Landmark::forestVista);
    // A different run can select legacy; its callbacks retain the synchronous contract.
    present::reset(); fake::run=2; CHECK(!omega::select(2,false));
    static_cast<void>(update(120,false,true));
    present::observe_position(inside(present::kVolumes[4]));
    CHECK(present::navigation().landmark==present::Landmark::lair);
    CHECK(update().presentation.objective==present::kObjectives[2]);
    std::printf("PASS: %u production runtime checks; native intake, streaming, reset, latched selection and concurrent publication\n",checks);
}
