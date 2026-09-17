#pragma once
#include "../../coo/lifecycle_service.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace sunrise::state::activity::newlight::launchpad::cinematics {
// Native Launchpad bookends: exterior resurrection, jumpship escape, welcome.
struct Movie {std::uint32_t registry,object,definition,selector;std::uint8_t region,bubble;};
inline constexpr Movie kMovies[]{
    {0xCFEEB2CA,0x8153C2C5,0x8153C2C2,0x8FF6DD4F,25,3},
    {0x7D02E472,0x8153C279,0x8153C276,0x521A80F5,17,2},
    {0x7D3480BC,0x8153C280,0x8153C27D,0x3B77D019,18,2}};
enum class Phase : std::uint8_t { dormant,preparing,offered,playing,stopping,landing,gameplay,complete,failed };
inline constexpr std::uint64_t kOfferedTimeoutMs=60000;
struct State {
    coo::Generation owner{};Phase phase{};std::uint8_t movie{};
    std::array<std::uint32_t,3> revisions{};std::uint64_t runtime{},deadline{};
    bool play{},flyInComplete{},endingStarted{},gameplayRetired{};
    bool retiring() const noexcept {return movie>0 && phase==Phase::preparing;}
    bool masking_opening(std::uint64_t now) const noexcept {
        // Arm only at the native fly-in/arrival boundary, never at selection or
        // loading. Cover the C9 camera handoff until actual opening playback.
        return flyInComplete && owner.valid() && movie==0 && (phase==Phase::preparing || phase==Phase::offered)
            && now<deadline;
    }
    std::uint8_t route() const noexcept {
        // Initial loading must settle before a cinematic C9 transition can begin.
        if(movie==0 && phase==Phase::preparing && !flyInComplete) {return 0;}
        // Do not start C9 while 3CCE50 is still destroying the gameplay registries.
        if(retiring() && !gameplayRetired) {return 0;}
        if(phase==Phase::landing) {return 4;}
        return phase==Phase::preparing || phase==Phase::offered || phase==Phase::playing || phase==Phase::stopping?movie+1:0;
    }
    bool ending() const noexcept {return movie>0 && phase!=Phase::dormant && phase!=Phase::gameplay;}
};
class Sequence final {
public:
    void begin(coo::Generation owner,std::uint64_t now) noexcept {
        state_={};state_.owner=owner;
        if(!owner.valid()) {return;}
        state_.revisions.fill(owner.value);prepare(0,now);state_.deadline=0;
    }
    void resume_gameplay(coo::Generation owner) noexcept {
        state_={};state_.owner=owner;state_.revisions.fill(owner.value);state_.phase=Phase::gameplay;
    }
    const State& state() const noexcept {return state_;}
    bool fly_in_complete(coo::Generation owner,std::uint64_t now) noexcept {
        if(owner!=state_.owner || !owner.valid() || state_.flyInComplete || state_.movie!=0
            || (state_.phase!=Phase::preparing && state_.phase!=Phase::offered)) {return false;}
        state_.flyInComplete=true;state_.deadline=now+90000;return true;
    }
    bool finish_gameplay(coo::Generation owner,std::uint64_t now) noexcept {
        if(owner!=state_.owner || state_.phase!=Phase::gameplay) {return false;}prepare(1,now);return true;
    }
    bool retired(coo::Generation owner) noexcept {
        if(owner!=state_.owner || !owner.valid() || state_.phase!=Phase::preparing
            || !state_.retiring() || state_.gameplayRetired) {return false;}
        state_.gameplayRetired=true;return true;
    }
    bool arrival(coo::Generation owner,std::uint8_t route,std::uint64_t now) noexcept {
        if(owner!=state_.owner || !owner.valid() || route==0 || route!=state_.route()) {return false;}
        if(state_.phase==Phase::landing && route==4) {state_.phase=Phase::gameplay;state_.deadline=0;return true;}
        if(state_.phase!=Phase::preparing) {return false;}
        state_.phase=Phase::offered;state_.play=true;++state_.revisions[state_.movie];state_.deadline=now+kOfferedTimeoutMs;return true;
    }
    bool incident(coo::Generation owner,std::uint32_t target,std::uint32_t registry,std::int8_t type,
        std::int16_t slot,std::uint64_t runtime,std::uint64_t now) noexcept {
        if(owner!=state_.owner || !owner.valid() || state_.movie>=3 || type!=6 || slot!=0
            || registry!=kMovies[state_.movie].registry || runtime==UINT64_MAX) {return false;}
        if(target==5239 && state_.phase==Phase::offered) {
            state_.runtime=runtime;state_.endingStarted|=state_.movie>0;state_.phase=Phase::playing;state_.deadline=now+650000;return true;
        }
        if(target==1685 && state_.phase==Phase::offered) {fail();return true;}
        // The incident runtime value changes during a playback. Authenticate
        // the run, source and accepted-start phase; never use it as a movie ID.
        if(target==3338 && state_.phase==Phase::playing) {
            stop();state_.phase=Phase::stopping;state_.deadline=now+15000;return true;
        }
        if(target!=1685 || (state_.phase!=Phase::playing && state_.phase!=Phase::stopping)) {return false;}
        stop();state_.runtime=0;
        if(state_.movie==0) {state_.phase=Phase::landing;state_.deadline=now+90000;}
        else if(state_.movie==1) {prepare(2,now);}
        else {state_.phase=Phase::complete;state_.deadline=0;}
        return true;
    }
    void advance(std::uint64_t now) noexcept {if(state_.deadline && now>=state_.deadline) {fail();}}
private:
    void stop() noexcept {state_.play=false;++state_.revisions[state_.movie];}
    void fail() noexcept {stop();state_.phase=Phase::failed;state_.deadline=0;}
    void prepare(std::uint8_t movie,std::uint64_t now) noexcept {
        state_.movie=movie;state_.phase=Phase::preparing;state_.runtime=0;state_.play=false;state_.deadline=now+90000;
        state_.gameplayRetired=false;
    }
    State state_{};
};
constexpr int index(std::uint32_t registry,std::uint8_t type,std::uint16_t slot) noexcept {
    if(type==6 && slot==0) {for(int i=0;i<3;++i) {if(kMovies[i].registry==registry) {return i;}}}return -1;
}
template<class W> bool write(W& w,const State& state,std::size_t movie) noexcept {
    if(movie>=3 || !state.owner.valid()) {return false;}
    return w.write(UINT64_MAX,64) && w.write(0,64) && w.write(state.revisions[movie],32)
        && w.write(state.movie==movie && state.play?1U:0U,1) && w.write(0,1)
        && w.write(0x811C9DC5,32) && w.write(0,7) && w.write(0x7FFF,16)
        && w.write(2,6) && w.write(0,5) && w.write(0,3) && w.write(0,32);
}
struct Incident {std::uint32_t target{},registry{};std::uint64_t runtime{};std::int8_t type{-1};std::int16_t slot{-1};};
// Decode the normal msg19 envelope and native 808087BF tail; reject compressed
// selectors and truncated/foreign layouts. The service already validates ownership.
template<class Reader> bool decode(Reader& r,Incident& result) noexcept {
    result={};std::uint64_t target{},count{},v{};
    if(!r.read(13,target) || (target!=5239 && target!=1685 && target!=3338)
        || !r.read(5,count) || count>25 || !r.skip(count*13)
        || !r.read(1,v) || v || !r.read(1,v) || (v && !r.skip(64))
        || !r.read(9,v) || v!=61 || r.remaining_bits()<488 || !r.skip(335)) {return false;}
    std::uint64_t registry{},type{},slot{},runtime{},value{},padding{};
    if(!r.read(32,registry) || !r.read(7,type) || !r.read(16,slot) || !r.read(64,runtime)
        || !r.read(32,value) || !r.read(2,padding) || padding || type!=7 || slot!=32768) {return false;}
    result={static_cast<std::uint32_t>(target),static_cast<std::uint32_t>(registry),runtime,6,0};return true;
}
}
