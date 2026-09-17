#include <Windows.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include "state/activity/omega_ending.h"
#include "state/activity/omega_presentation.h"
#include "state/activity/omega_first_lair_runtime.h"
#include "state/activity/runtime.h"
#include "core/logging/log.h"

namespace activity=dawn::state::activity;
namespace ending=activity::omega_ending;
namespace fight=activity::omega_first_lair;
namespace present=activity::omega_presentation;
namespace transit=activity::omega_ending_transit;
namespace fake {
std::atomic_uint64_t run{42};
std::atomic_bool seed{true},quiesced{false},dialogue{false};
std::atomic<activity::WorldPhase> phase{activity::WorldPhase::arrived};
std::atomic_uint claims{},starts{},finishes{},notes{},completedGraphs{},failures{};
fight::Status encounter{};
constexpr activity::ActivityInstanceKey key{10,{1}};
}
namespace dawn::state::activity {
std::uint64_t mission_run_generation() noexcept { return fake::run; }
bool mission_seed_armed() noexcept { return fake::seed; }
bool omega_authority_quiesced() noexcept { return fake::quiesced; }
WorldPhase world_phase() noexcept { return fake::phase; }
bool contains(ActivityInstanceKey key) noexcept { return key==fake::key; }
namespace omega_presentation {
bool ending_dialogue_finished(std::uint64_t,std::uint64_t) noexcept { return fake::dialogue; }
Navigation navigation() noexcept { Navigation result{};result.enabled=true;result.run=fake::run;result.landmark=Landmark::lighthouse;return result; }
void note_encounter(std::uint64_t,Encounter,std::uint8_t) noexcept { ++fake::notes; }
}
namespace omega_first_lair {
Status status(std::uint64_t) noexcept { return fake::encounter; }
bool claim_action(const Boss& boss,Action action) noexcept {
    if(boss!=fake::encounter.boss || action!=fake::encounter.action) { return false; }
    ++fake::claims;fake::encounter.action=Action::none;fake::encounter.phase=Phase::mechanicRequested;return true;
}
bool observe_ending(const CrownToken& token,bool finished) noexcept {
    if(token!=fake::encounter.token) { return false; }
    if(finished) { ++fake::finishes;fake::encounter.crownStage=CrownStage::finished; }
    else { ++fake::starts;fake::encounter.phase=Phase::mechanicPlaying; }
    return true;
}
}
} // namespace dawn::state::activity
namespace dawn::core::log {
void write(Channel,Level,std::string_view line) noexcept {
    if(line.starts_with("ev=coo_ending") && line.find("phase=2 active=00000000 complete=0000007F")!=line.npos) { ++fake::completedGraphs; }
    if(line.find("failure=")!=line.npos && line.find("failure=0")==line.npos) { ++fake::failures; }
}
}
static unsigned checks{};
#define CHECK(value) do { ++checks;if(!(value)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#value);std::exit(1); } } while(false)
const ending::Token kOwner{42,20,1,7};
ending::Authority authority() { return ending::authority(fake::run,GetTickCount64()); }
void bind() {
    ending::reset();fake::run=42;fake::quiesced=false;fake::dialogue=false;
    fake::encounter={{42,1,2,3,7,1,4,20},fight::Action::finishEncounter,fight::Phase::mechanicReady,true,false,4,3,2,fight::CrownStage::ending,{}};
    fake::encounter.token={fake::encounter.boss,3};
}
ending::TransitInput transit_input() { return {fake::key,42,99,true,{}}; }
void prepare(bool selected) {
    bind();auto stale=kOwner;++stale.actor;CHECK(!ending::request(stale,selected));
    CHECK(ending::request(kOwner,selected));CHECK(ending::request(kOwner,!selected));
    CHECK(fake::encounter.phase==fight::Phase::mechanicRequested);
    CHECK(!authority().retireRoster);CHECK(!ending::observe_retirement(kOwner));
    fake::dialogue=true;static_cast<void>(authority());CHECK(ending::retirement_request(42)==kOwner);
    CHECK(ending::observe_retirement(kOwner));
    if(selected) { CHECK(ending::retirement_request(42)==kOwner); }
    CHECK(authority().bookendState);CHECK(!ending::retirement_request(42).valid());
    auto input=transit_input();auto command=ending::project_transit(input);CHECK(command.publish);CHECK(!command.arrived);
    CHECK(command.host.sliceSetIndex==ending::kSlice);CHECK(command.host.sliceSetHash==ending::kArrivalSpawnSet);
    auto foreign=input;foreign.memberKey=100;CHECK(!ending::project_transit(foreign).publish);
    input.native={command.host,ending::kSlice,true,true};input.native.local.state=3;
    CHECK(ending::project_transit(input).arrived);
    ending::observe(kOwner,0,false,true);ending::update();CHECK(authority().play);
}
int main() {
    prepare(true);const auto initialClaims=fake::claims.load();CHECK(initialClaims==1);
    auto movie=authority();ending::observe(kOwner,movie.revision,true,true);
    CHECK(fake::starts==0);ending::update();CHECK(fake::starts==1);CHECK(fake::notes==1);
    CHECK(ending::request_skip(42));CHECK(!ending::request_skip(42));
    ending::observe(kOwner,movie.revision,false,true);ending::update();movie=authority();
    CHECK(!movie.play && !movie.complete);CHECK(fake::finishes==0);
    ending::observe(kOwner,movie.revision,false,false);ending::update();CHECK(!authority().complete);
    ending::observe(kOwner,movie.revision,false,true);CHECK(fake::finishes==0);
    ending::update();CHECK(fake::finishes==1);CHECK(authority().complete);
    CHECK(ending::handoff_request()==kOwner);CHECK(ending::claim_handoff(kOwner));CHECK(!ending::claim_handoff(kOwner));
    // Commit may change the activity/run before bookkeeping consumes its result.
    fake::quiesced=true;fake::run=43;
    ending::note_handoff_result(kOwner,true);CHECK(ending::handoff_status(42)==ending::Handoff::claimed);
    ending::update();CHECK(ending::handoff_status(42)==ending::Handoff::queued);CHECK(fake::completedGraphs==1);
    CHECK(!ending::handoff_request().valid());ending::update();CHECK(fake::completedGraphs==1);
    // Fast active/inactive receipts must forward start then finish exactly once.
    prepare(true);movie=authority();const auto starts=fake::starts.load(),finishes=fake::finishes.load();
    ending::observe(kOwner,movie.revision,true,true);ending::observe(kOwner,movie.revision,false,true);
    ending::update();CHECK(fake::starts==starts+1);CHECK(fake::finishes==finishes+1);CHECK(authority().complete);
    // Native observers/readers never acquire the adapter lock or invoke services.
    std::thread callbacks([] {
        for(unsigned i=0;i<1000;++i) { ending::observe({41,20,1,7},0,false,true);static_cast<void>(ending::retirement_request(42)); }
    });
    for(unsigned i=0;i<1000;++i) { ending::update(); }
    callbacks.join();CHECK(fake::failures==0);
    // A run initially selected as legacy retains synchronous playback callbacks.
    prepare(false);movie=authority();const auto legacyStarts=fake::starts.load();
    ending::observe(kOwner,movie.revision,true,true);CHECK(fake::starts==legacyStarts+1);
    ending::reset();fake::encounter.enabled=false;CHECK(ending::preview_available());
    CHECK(ending::request_preview());CHECK(authority().bookendState);CHECK(!ending::handoff_request().valid());
    ending::reset();CHECK(!ending::handoff_request().valid());CHECK(fake::failures==0);
    std::printf("PASS: %u production ending checks; native transit, deferred callbacks, start/finish forwarding, legacy/preview, post-commit handoff and concurrent intake\n",checks);
}
