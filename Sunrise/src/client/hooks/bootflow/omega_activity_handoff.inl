#include <array>
#include <atomic>
#include <cstring>
#include <cstdio>
#include "../../../state/activity/omega_ending.h"
#include "../../../state/activity/forced/activity_forced_destination.h"

namespace sunrise::client::hooks::bootflow::omega_activity_handoff {
namespace ending=state::activity::omega_ending;
template<class T> bool read(std::uintptr_t address,T& value) noexcept {
    SIZE_T copied{};
    return address!=0 && ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),
        &value,sizeof(value),&copied)!=FALSE && copied==sizeof(value);
}
template<class T> bool write(std::uintptr_t address,const T& value) noexcept {
    SIZE_T written{};
    return address!=0 && WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),
        &value,sizeof(value),&written)!=FALSE && written==sizeof(value);
}
template<class F> F resolve(std::uintptr_t base,std::uintptr_t rva,std::array<unsigned char,8> expected) noexcept {
    std::array<unsigned char,8> actual{};
    return read(base+rva,actual) && actual==expected ? reinterpret_cast<F>(base+rva) : nullptr;
}
using World=std::uintptr_t(__fastcall*)();
using Ready=bool(__fastcall*)(std::uintptr_t);
using Record=std::uintptr_t(__fastcall*)(std::uint32_t);
using Construct=void*(__fastcall*)(void*,std::uint32_t,std::int16_t);
using Valid=bool(__fastcall*)(const void*);
using Name=const char*(__fastcall*)(std::int16_t);
using Clear=void(__fastcall*)();
using Select=void(__fastcall*)(std::uint8_t,const void*);
using Commit=void(__fastcall*)(std::int32_t);
using Step=std::int32_t(__fastcall*)();
using ReportFailure=std::uint64_t(__fastcall*)(std::int32_t,std::int32_t);
/** Bootflow step `activity:in_world`, the only step the in-world exit may leave from. */
inline constexpr std::int32_t kInWorldStep=38;
/** Bootflow step `cleanup`: the world controller's only road from in-world to a new activity. */
inline constexpr std::int32_t kCleanupStep=28;
/** Bootflow step `setup:orbit_outro`, the last step that reads the transition kind. */
inline constexpr std::int32_t kOrbitOutroStep=34;
/** The reason blocker bit 30 reported on the recorded bail run (28, 309): no error dialog, and the
 * latched selection relaunched. Reason 58 (the step-38 out-of-sync exit) raises the Ostrich dialog
 * and parks the player in orbit instead. Cleanup's task mask treats 309 (and 7) as an activity
 * transition: with transition kind 1 or 2 it skips its orbit-world tasks (21, 22, 35, 53). */
inline constexpr std::int32_t kBenignExitReason=309;
/** Launch-tick countdown plus slack; past it the exit is raised even if the transition property never
 * showed the new descriptor, so a stalled launch ends in orbit rather than a black screen. */
inline constexpr std::uint64_t kExitTimeoutMs=8000;
/** Fireteam session's replicated activity-transition property (session+0x182C0): request value at
 * +0x148 (kind/reason byte +0, from +2, index +4, nonce +0x18) and the fill-once effective descriptor
 * at +0x378: kind +0, from index +4, from destination +6, from destination hash +8, to index +0xC, to
 * destination +0xE, to destination hash +0x10. Fill writes it while the kind reads -1 and classifies
 * the (from, to) records; a replicated update of the request resets it. */
inline constexpr std::uintptr_t kTransitionProperty=0x182C0;
inline constexpr std::uintptr_t kRequestValue=0x148;
inline constexpr std::uintptr_t kEffectiveValue=0x378;
/** Transition kinds: 0 spaceflight (orbit world + flight cinematic), 1 no_ship (loading screen, world
 * reload), 2 same slice set (no reload; not usable here). */
inline constexpr std::int32_t kKindUnfilled=-1;
inline constexpr std::int32_t kKindNoShip=1;
struct Effective final {
    std::int32_t kind{kKindUnfilled};
    std::int8_t fromDestination{-1},toDestination{-1};
    std::uint32_t fromHash{},toHash{};
};
inline bool read_effective(std::uintptr_t session,Effective& value) noexcept {
    const auto effective=session+kTransitionProperty+kEffectiveValue;
    return read(effective,value.kind) && read(effective+6,value.fromDestination) && read(effective+8,value.fromHash)
        && read(effective+0xE,value.toDestination) && read(effective+0x10,value.toHash);
}
/** Fill classifies the ending's launch as spaceflight: the mission and the patrol records do not pair
 * as an in-destination transition, so cleanup loads the orbit world and step 33 loads the flight
 * cinematics. Retail's return from a mission is the no_ship transition: no orbit world, the loading
 * screen from cleanup to the world transition, and step 33's batch picks the no-flight family because
 * both destination hashes name the same destination (the slice-set verdict 0xC00650 compares them).
 * Rewrite the filled descriptor to that classification: kind 1, from destination := to destination. */
inline bool assert_no_ship(std::uintptr_t session,const Effective& value) noexcept {
    const auto effective=session+kTransitionProperty+kEffectiveValue;
    return write(effective,kKindNoShip) && write(effective+6,value.toDestination) && write(effective+8,value.toHash);
}
inline bool is_no_ship(const Effective& value) noexcept {
    return value.kind==kKindNoShip && value.fromHash==value.toHash && value.fromDestination==value.toDestination;
}
inline bool fireteam_session(std::uintptr_t base,std::uintptr_t& session) noexcept {
    const auto world=resolve<World>(base,0xC03430,{0x40,0x56,0x48,0x83,0xEC,0x30,0x48,0x8B});
    if(!world) { return false; }
    const auto manager=world();
    std::int32_t primary{},sessionState{};
    if(!manager || !read(manager+0x10,primary) || primary<0 || primary>3) { return false; }
    session=manager+0x18+static_cast<std::uintptr_t>(primary)*0x1C8A0;
    return read(session+0x1AEF8,sessionState) && sessionState>=4 && sessionState<=9;
}
struct ExitState final {
    std::uint64_t run{},nonce{},queuedAt{},nextPoll{};
    std::uintptr_t session{};
    bool done{true};
};
inline ExitState& exit_state() noexcept { static ExitState s; return s; }
inline void report_exit(const char* result,const ExitState& state,std::int32_t step) noexcept {
    std::array<char,192> line{};
    const int length=std::snprintf(line.data(),line.size(),
        "ev=omega_handoff stage=native_exit result=%s run=%llu step=%d code=%d sub=%d",
        result,static_cast<unsigned long long>(state.run),step,kCleanupStep,kBenignExitReason);
    if(length>0 && static_cast<std::size_t>(length)<line.size()) {
        core::log::write(core::log::Channel::client,core::log::Level::info,{line.data(),static_cast<std::size_t>(length)});
    }
}
inline constexpr std::uint64_t kKindTimeoutMs=45000;
/** The client's own "loading cinematics are suppressed" switch (wrapper 0xC24490 over the stub
 * 0xE0DC10, which retail compiled to `return 0`). Its callers are the complete presentation of a
 * transition: the ui_stage update falls back to the common loading UI (black + spinner), step 30
 * skips the cinematic preload, step 33 skips the intro/filler batch, orbit_outro skips its
 * sequence, and step 36's tasks log `perform_prologue_intro_playback / perform_prologue_filler_exit
 * _playback / perform_cinematic_exchange was skipped due to suppressing cinematics`. The wrapper is
 * detoured (world_step.cpp) to answer true only while this window is armed: from the deliberate
 * exit until the bootflow reaches `activity:in_world` again, or the timeout. */
inline constexpr std::uint64_t kSuppressTimeoutMs=120000;
struct SuppressState final {
    std::atomic<bool> active{false};
    std::uint64_t run{},startedAt{};
    bool left{};
};
inline SuppressState& suppress_state() noexcept { static SuppressState s; return s; }
/** Read by the detour on the game thread. */
inline bool suppress_active() noexcept { return suppress_state().active.load(std::memory_order_relaxed); }
inline void report_suppress(const char* result,const SuppressState& state,std::int32_t step) noexcept {
    std::array<char,160> line{};
    const int length=std::snprintf(line.data(),line.size(),
        "ev=omega_handoff stage=cinematic_suppression result=%s run=%llu step=%d",
        result,static_cast<unsigned long long>(state.run),step);
    if(length>0 && static_cast<std::size_t>(length)<line.size()) {
        core::log::write(core::log::Channel::client,core::log::Level::info,{line.data(),static_cast<std::size_t>(length)});
    }
}
inline void arm_suppress(std::uint64_t run,std::uint64_t now,std::int32_t step) noexcept {
    auto& state=suppress_state();
    state.run=run;state.startedAt=now;state.left=false;
    state.active.store(true,std::memory_order_relaxed);
    report_suppress("armed",state,step);
}
inline void poll_suppress(std::uintptr_t base,std::uint64_t now) noexcept {
    auto& state=suppress_state();
    if(!state.active.load(std::memory_order_relaxed)) { return; }
    const auto getStep=resolve<Step>(base,0xE2D510,{0x48,0x83,0xEC,0x28,0xE8,0x07,0x83,0x00});
    if(!getStep) { state.active.store(false,std::memory_order_relaxed);report_suppress("target_guard",state,-1);return; }
    const std::int32_t step=getStep();
    // The step still reads 38 until the world controller acts on the exit; the window closes once
    // the ladder has left in_world and come back to it.
    if(step!=kInWorldStep) { state.left=true; }
    else if(state.left) {
        state.active.store(false,std::memory_order_relaxed);report_suppress("released",state,step);return;
    }
    if(now>=state.startedAt+kSuppressTimeoutMs) {
        state.active.store(false,std::memory_order_relaxed);report_suppress("timeout",state,step);
    }
}
/** Keeps the no_ship classification in place from the exit to the orbit outro: cleanup's task mask
 * reads the kind once its fireteam synchronisation clears, step 33's cinematic batch reads the
 * destination hashes, and orbit_outro reads the kind again; any replicated update of the request in
 * between refills the descriptor as spaceflight. */
struct KindState final {
    std::uint64_t run{},startedAt{};
    bool active{};
    unsigned writes{};
};
inline KindState& kind_state() noexcept { static KindState s; return s; }
inline void report_kind(const char* result,const KindState& state,std::int32_t step,const Effective& value) noexcept {
    std::array<char,224> line{};
    const int length=std::snprintf(line.data(),line.size(),
        "ev=omega_handoff stage=transition_kind result=%s run=%llu step=%d kind=%d from=%d/%08X to=%d/%08X writes=%u",
        result,static_cast<unsigned long long>(state.run),step,value.kind,static_cast<int>(value.fromDestination),
        value.fromHash,static_cast<int>(value.toDestination),value.toHash,state.writes);
    if(length>0 && static_cast<std::size_t>(length)<line.size()) {
        core::log::write(core::log::Channel::client,core::log::Level::info,{line.data(),static_cast<std::size_t>(length)});
    }
}
inline void poll_kind(std::uintptr_t base,std::uint64_t now) noexcept {
    auto& state=kind_state();
    if(!state.active) { return; }
    const auto getStep=resolve<Step>(base,0xE2D510,{0x48,0x83,0xEC,0x28,0xE8,0x07,0x83,0x00});
    if(!getStep) { state.active=false;report_kind("target_guard",state,-1,{});return; }
    const std::int32_t step=getStep();
    if(step>kOrbitOutroStep && step<kInWorldStep) { state.active=false;report_kind("done",state,step,{});return; }
    if(now>=state.startedAt+kKindTimeoutMs) { state.active=false;report_kind("timeout",state,step,{});return; }
    if(step<kCleanupStep || (step>kOrbitOutroStep && step!=kInWorldStep)) { return; }
    std::uintptr_t session{};
    Effective value{};
    if(!fireteam_session(base,session) || !read_effective(session,value)) { return; }
    if(value.kind==kKindUnfilled || is_no_ship(value)) { return; }
    if(assert_no_ship(session,value)) {
        ++state.writes;
        report_kind("reasserted",state,step,value);
    }
}
/** In-world, a resting launch moves nothing by itself: step 38 has no launch exit, the patrol
 * destination's host-request arm never reaches this host, and the pending-destination arm only
 * re-arms an already armed slot. What did move the recorded runs was the cleanup the blocker
 * raised once the launch had rewritten the fireteam's transition property: the ladder then
 * relaunched the latched selection. Raise that same cleanup deliberately, with the blocker's
 * benign reason, once the transition property carries the new descriptor and Fill has classified
 * it, rewritten as the no_ship transition cleanup expects for an in-destination activity change. */
inline void poll_exit(std::uintptr_t base,std::uint64_t now) noexcept {
    auto& state=exit_state();
    if(state.done || now<state.nextPoll) { return; }
    state.nextPoll=now+100;
    const auto getStep=resolve<Step>(base,0xE2D510,{0x48,0x83,0xEC,0x28,0xE8,0x07,0x83,0x00});
    const auto reportFailure=resolve<ReportFailure>(base,0xE2DEB0,{0x48,0x89,0x5C,0x24,0x18,0x55,0x56,0x57});
    if(!getStep || !reportFailure) { state.done=true;report_exit("target_guard",state,-1);return; }
    const auto transition=state.session+kTransitionProperty;
    std::uint8_t flags{};std::uint16_t activity{};std::uint64_t nonce{};
    Effective value{};
    const bool rested=read(transition+0x140,flags) && (flags&1U)!=0 && read(transition+kRequestValue+4,activity)
        && activity==29 && (state.nonce==0 || (read(transition+kRequestValue+0x18,nonce) && nonce==state.nonce))
        && read_effective(state.session,value) && value.kind!=kKindUnfilled;
    const bool timedOut=now>=state.queuedAt+kExitTimeoutMs;
    if(!rested && !timedOut) { return; }
    const std::int32_t step=getStep();
    if(step!=kInWorldStep) {
        if(timedOut) { state.done=true;report_exit("step_guard",state,step); }
        return;
    }
    state.done=true;
    kind_state()={state.run,now,true,0};
    if(read_effective(state.session,value) && value.kind!=kKindUnfilled && !is_no_ship(value)
        && assert_no_ship(state.session,value)) {
        ++kind_state().writes;
        report_kind("asserted",kind_state(),step,value);
    }
    arm_suppress(state.run,now,step);
    reportFailure(kCleanupStep,kBenignExitReason);
    report_exit(rested?"reported":"reported_timeout",state,step);
}
inline void report(const char* result,ending::Token token,std::uint64_t nonce=0) noexcept {
    std::array<char,224> line{};
    const int length=std::snprintf(line.data(),line.size(),
        "ev=omega_handoff stage=native_launch result=%s run=%llu origin=%u destination=mercury_freeroam activity=29 nonce=%08X-%08X",
        result,static_cast<unsigned long long>(token.run),static_cast<unsigned>(token.origin),
        static_cast<unsigned>(nonce>>32),static_cast<unsigned>(nonce&0xFFFFFFFFULL));
    if(length>0 && static_cast<std::size_t>(length)<line.size()) {
        core::log::write(core::log::Channel::client,core::log::Level::info,{line.data(),static_cast<std::size_t>(length)});
    }
}
/** The 64-bit nonce at request value +0x18 is the "fireteam nonce" the bootflow latches at activity
 * setup. In-world blocker 30 bails through orbit ("host appears to be launching a different game")
 * whenever that latched nonce is valid while the property's nonce reads 0/-1, which is exactly what
 * a launch-side selection carrying the constructor's zero nonce produces once the launch machine
 * writes it into the property. Carrying the current nonce keeps blocker 30 and the rejoin predicate
 * satisfied, so the launch proceeds as an in-place activity transition instead of an orbit round trip. */
inline bool fireteam_transition_nonce(std::uintptr_t session,std::uint64_t& nonce) noexcept {
    const auto transition=session+kTransitionProperty;
    std::uint8_t flags{};
    return read(transition+0x140,flags) && (flags&1U)!=0 && read(transition+kRequestValue+0x18,nonce)
        && nonce!=0 && nonce!=~0ULL;
}
/** Called by the existing frame owner, outside every cinematic component callback.
 * Native public wrappers own immediate/deferred execution and descriptor copying.
 * No session goal, player, fade, native activity record or actor is patched. */
inline void poll() noexcept {
    ending::update();
    const auto now=GetTickCount64();
    const auto base=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    poll_exit(base,now);
    poll_kind(base,now);
    poll_suppress(base,now);
    const auto token=ending::handoff_request();
    if(!token.valid()) { return; }
    static std::uint64_t nextPoll{},reportedRun{};
    if(now<nextPoll) { return; }
    nextPoll=now+500;
    const auto world=resolve<World>(base,0xC03430,{0x40,0x56,0x48,0x83,0xEC,0x30,0x48,0x8B});
    const auto sessionReady=resolve<Ready>(base,0x1788810,{0x83,0xB9,0x6C,0x08,0x00,0x00,0x00,0x0F});
    const auto memberReady=resolve<Ready>(base,0x178D740,{0x4C,0x8B,0xC1,0x48,0x63,0x89,0x3C,0xE9});
    const auto record=resolve<Record>(base,0xBFA030,{0x40,0x53,0x48,0x83,0xEC,0x20,0x8B,0xD9});
    const auto construct=resolve<Construct>(base,0xC061D0,{0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x74});
    const auto valid=resolve<Valid>(base,0x4D5460,{0x0F,0xB6,0x11,0xB0,0x01,0x80,0xFA,0xFF});
    const auto name=resolve<Name>(base,0xDDECA0,{0x48,0x89,0x5C,0x24,0x08,0x57,0x48,0x83});
    const auto clear=resolve<Clear>(base,0xBF95D0,{0x48,0x83,0xEC,0x38,0xE8,0xE7,0xE0,0x7F});
    const auto select=resolve<Select>(base,0xBFB1F0,{0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x74});
    const auto commit=resolve<Commit>(base,0xBF97D0,{0x89,0x4C,0x24,0x08,0x48,0x83,0xEC,0x38});
    if(!world || !sessionReady || !memberReady || !record || !construct || !valid || !name || !clear || !select || !commit) {
        if(reportedRun!=token.run) { reportedRun=token.run;report("target_guard",token); }
        return;
    }
    // Reproduce BF9FA0's read-only ownership lookup. BF9FA0 itself is already
    // detoured by the selection observer, so it cannot be resolved by prologue.
    const auto manager=world();
    std::int32_t primary{},sessionState{},member{};
    if(!manager || !read(manager+0x10,primary) || primary<0 || primary>3) { return; }
    const auto session=manager+0x18+static_cast<std::uintptr_t>(primary)*0x1C8A0;
    if(!read(session+0x1AEF8,sessionState) || sessionState<4 || sessionState>9
        || !sessionReady(session) || !memberReady(session)
        || !read(session+0xE93C,member) || member<0 || member>=12) { return; }
    const auto current=record(static_cast<std::uint32_t>(member));
    std::uint8_t launchState{};
    if(!current || !read(current+0xA33,launchState) || launchState>=3) { return; }
    std::array<char,17> package{};
    if(!read(reinterpret_cast<std::uintptr_t>(name(29)),package)
        || package!=std::array<char,17>{'m','e','r','c','u','r','y','_','f','r','e','e','r','o','a','m','\0'}) {
        if(reportedRun!=token.run) { reportedRun=token.run;report("destination_guard",token); }
        return;
    }
    alignas(16) std::array<std::byte,0x120> selection{};
    if(construct(selection.data(),0,29)!=selection.data()) { return; }
    std::int16_t source{},destination{};
    std::memcpy(&source,selection.data()+2,sizeof(source));
    std::memcpy(&destination,selection.data()+4,sizeof(destination));
    if(source!=29 || destination!=29 || selection[0]!=std::byte{} || !valid(selection.data())) { return; }
    // These two fields are part of the native launch-side descriptor, verified
    // against two recorded Mercury svc6 selections. All other fields retain
    // native constructor defaults, including its identity and optional data.
    constexpr std::array<std::uint32_t,2> landing{0xA83A9175U,0xD49C610EU};
    std::memcpy(selection.data()+0x40,landing.data(),sizeof(landing));
    // Launch-side +0x10 converts to descriptor +0x18, the nonce the launch machine writes into the
    // fireteam's transition property. Without it the in-world blocker sends the client to orbit.
    std::uint64_t nonce{};
    const bool carriesNonce=fireteam_transition_nonce(session,nonce);
    if(carriesNonce) { std::memcpy(selection.data()+0x10,&nonce,sizeof(nonce)); }
    if(!valid(selection.data()) || !ending::claim_handoff(token)) { return; }
    if(!state::activity::forced::suspend_omega_for_completed_run(token.run)) {
        ending::note_handoff_result(token,false);ending::update();report("override_changed",token);return;
    }
    if(!carriesNonce) { report("nonce_missing",token); } // Falls back to the orbit round trip.
    clear();
    select(0,selection.data());
    commit(1);
    ending::note_handoff_result(token,true);
    ending::update();
    exit_state()={token.run,carriesNonce?nonce:0,now,now,session,false};
    report("queued",token,nonce); // Native svc6 and in-world arrival remain separate receipts.
}
} // namespace sunrise::client::hooks::bootflow::omega_activity_handoff
