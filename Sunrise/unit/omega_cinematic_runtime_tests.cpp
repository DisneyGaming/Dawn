// Compile the actual callback. Native registration/start and mission context are
// controlled boundaries; this does not emulate the cinematic renderer.
#include <Windows.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include "client/hooking/call_gate.h"
#include "client/hooks/bootflow/omega_boss_spawn.h"

namespace sunrise::state::activity {
std::uint64_t fixtureRun = 7;
std::uint64_t mission_run_generation() noexcept { return fixtureRun; }
namespace omega {
Progress fixtureProgress = [] { Progress p{}; p.route=Route::crownEntrance;
    p.loadedBubble=14; p.bossDoorReached=true; return p; }();
Progress presentation_progress(std::uint64_t) noexcept { return fixtureProgress; }
}
}
namespace sunrise::client::hooks::bootflow::fixture {
namespace omega=state::activity::omega;
unsigned checks{}, starts{}, originalCalls{};
std::uint64_t fixtureTime=100;
bool registered{}, startSucceeds=true, changeRunDuringLookup{};
void check(bool pass, const char* why) {
    ++checks; if (!pass) { std::fprintf(stderr,"FAIL: %s\n",why); std::exit(1); }
}
using Tick=void(__fastcall*)(std::byte*) noexcept;
void original(std::byte*) noexcept { ++originalCalls; }
std::atomic<Tick> cinematicOriginal{original};
hooking::CallGate callGate;
std::mutex mutex;
struct RunState {
    std::uint64_t run{7}, nextCinematic{}, cinematicCompletedAt{};
    bool cinematicStarted{}, cinematicFinished{}, cinematicObserved{}, cinematicMissingLogged{};
    bool queueAccepted{true}, flightSeen{true}, graphRetired{};
    unsigned cinematicAttempts{};
} runState;
void reset(std::uint64_t run) noexcept {
    if (runState.run!=run) { runState={}; runState.run=run; }
}
bool active() noexcept { return true; }
bool copy_native(const void* from, void* to, std::size_t size) noexcept {
    if (!from) return false; std::memcpy(to,from,size); return true;
}
template<class T> bool copy_value(const void* from,T& to) noexcept { return copy_native(from,&to,sizeof to); }
template<class T> T read(const std::byte* from,std::size_t offset) noexcept {
    T value{}; std::memcpy(&value,from+offset,sizeof value); return value;
}
template<class... Args> void log(const char*,Args...) noexcept {}
void observe_tick(bool,const void*,bool,bool) noexcept {}
std::uint64_t clock_now() noexcept { return fixtureTime; }
void* __fastcall find(const std::uint32_t* selector) noexcept {
    check(*selector==0xA74B2200,"exact packaged camera selector");
    if (changeRunDuringLookup) ++runState.run;
    return registered?&runState:nullptr;
}
bool __fastcall start(std::byte* component) noexcept {
    ++starts;
    if (startSucceeds) component[0x260]=std::byte{1};
    return startSucceeds;
}
template<class T> T native(std::uintptr_t rva) noexcept {
    if (rva==0xC4C1A0) return reinterpret_cast<T>(&find);
    if (rva==0x1069CC0) return reinterpret_cast<T>(&start);
    std::abort();
}
#define GetTickCount64 clock_now
#ifndef OMEGA_CINEMATIC_SOURCE
#define OMEGA_CINEMATIC_SOURCE "client/hooks/bootflow/omega_cinematic_runtime.inl"
#endif
#include OMEGA_CINEMATIC_SOURCE
#undef GetTickCount64

std::array<std::byte,0x262> component() {
    std::array<std::byte,0x262> body{};
    const auto identity=omega_reveal_source::kIntro;
    std::memcpy(body.data(),&identity.resource,4);
    std::memcpy(body.data()+4,&identity.runtimeClass,4);
    std::memcpy(body.data()+8,&identity.offset,8);
    return body;
}
void fresh() {
    runState={}; fixtureTime=100; starts=0; registered=false; startSucceeds=true;
    changeRunDuringLookup=false; callGate.accept();
}
void tests() {
    fresh(); auto body=component();
    for (unsigned i=0;i<12;++i) { cinematic_tick(body.data()); fixtureTime+=2001; }
    check(starts==0 && runState.cinematicAttempts==0,
          "late camera registration cannot exhaust attempts without a native start");
    registered=true; cinematic_tick(body.data());
    check(starts==1 && runState.cinematicAttempts==1 && runState.cinematicStarted,
          "camera starts once after long resource loading delay");
    cinematic_tick(body.data());
    check(starts==1 && runState.cinematicObserved && !runState.cinematicFinished,
          "native active receipt does not replay or complete camera");
    body[0x260]=std::byte{}; fixtureTime+=10; cinematic_tick(body.data());
    check(runState.cinematicFinished && runState.cinematicCompletedAt==fixtureTime,
          "real active-to-inactive edge completes camera");
    fresh(); body=component(); registered=true; startSucceeds=false;
    for (unsigned i=0;i<12;++i) { cinematic_tick(body.data()); fixtureTime+=2001; }
    check(starts==5 && runState.cinematicAttempts==5 && !runState.cinematicFinished,
          "actual failed native starts remain bounded and never complete");
    fresh(); body=component(); registered=true; runState.flightSeen=false;
    cinematic_tick(body.data()); check(starts==0,"no camera before native fly receipt");
    runState.flightSeen=true; runState.queueAccepted=false;
    cinematic_tick(body.data()); check(starts==0,"no camera before owned queue acceptance");
    runState.queueAccepted=true; runState.graphRetired=true;
    cinematic_tick(body.data()); check(starts==0,"retired graph cannot start camera");
    fresh(); body=component(); registered=true; changeRunDuringLookup=true;
    cinematic_tick(body.data()); check(starts==0,"owner change during registration lookup rejects start");
    fresh(); body=component(); registered=true; body[0]=std::byte{};
    cinematic_tick(body.data()); check(starts==0,"foreign component cannot start camera");
    fresh(); body=component(); registered=true; callGate.quiesce();
    const auto forwarded=originalCalls; cinematic_tick(body.data());
    check(starts==0 && originalCalls==forwarded+1,"quiesced callback forwards native tick without starting");
}
}
int main() {
    sunrise::client::hooks::bootflow::fixture::tests();
    std::printf("PASS: %u production cinematic callback checks\n",
        sunrise::client::hooks::bootflow::fixture::checks);
}
