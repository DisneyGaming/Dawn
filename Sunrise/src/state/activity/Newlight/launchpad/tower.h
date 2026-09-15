#pragma once
#include "cinematics.h"
#include "native_catalog.h"
#include "../../coo/executor.h"
#include "../../omega_ending_transit_rules.h"
#include "../../runtime.h"
#include <mutex>

namespace sunrise::state::activity::newlight::launchpad::tower {
namespace native=omega_ending_transit;
inline constexpr std::uint32_t kApproachScenario=0x80B4A0EAU,kTowerScenario=0x80B4A0F4U;
inline constexpr coo::Asset kMovie{0x32DDAD77U,0x80B4A0F6U,6,0};
enum class Phase : std::uint8_t {idle,approachRequested,approachLoading,preparing,offered,playing,stopping,towerRequested,towerLoading,towerArriving,complete,failed};
struct State {
    coo::Generation origin{};std::uint64_t run{},deadline{};Phase phase{};
    ActivityInstanceKey activity{};std::uint64_t member{};
    native::Teleport host{};std::uint32_t revision{};bool play{};
};
struct Frame {std::uint64_t run{};std::uint32_t revision{};bool enabled{},play{};};
class Sequence {
public:
    bool begin(coo::Generation owner) noexcept {
        if(!owner.valid() || (s.phase!=Phase::idle && s.phase!=Phase::complete && s.phase!=Phase::failed)) {return false;}
        if(owner==s.origin) {return false;}s={};s.origin=owner;s.run=owner.run;s.phase=Phase::approachRequested;return true;
    }
    std::int16_t wanted() const noexcept {return s.phase==Phase::approachRequested?2:s.phase==Phase::towerRequested?20:-1;}
    bool queued(std::uint64_t run,std::int16_t index,std::uint64_t now) noexcept {
        if(run!=s.run || index!=wanted() || index<0) {return false;}
        s.phase=index==2?Phase::approachLoading:Phase::towerLoading;s.deadline=now+180000;return true;
    }
    void selected(std::uint64_t run,std::int16_t index,std::uint32_t scenario,std::uint64_t now) noexcept {
        if(!run || run==s.run || !scenario) {return;}
        // The world run advances during teardown, before destination selection.
        // A final roster for the departing activity cannot reject the queued launch.
        if((s.phase==Phase::approachLoading && index==1 && scenario==kScenario)
            || (s.phase==Phase::towerLoading && index==2 && scenario==kApproachScenario)) {return;}
        if(s.phase==Phase::approachLoading && index==2 && scenario==kApproachScenario) {
            s.run=run;s.phase=Phase::preparing;s.deadline=now+90000;s.revision=1;
        } else if(s.phase==Phase::towerLoading && index==20 && scenario==kTowerScenario) {
            s.run=run;s.phase=Phase::towerArriving;s.deadline=now+180000;
        } else if(s.phase!=Phase::idle && s.phase!=Phase::complete) {s.phase=Phase::failed;s.play=false;}
    }
    bool arrived(std::uint64_t run,WorldPhase phase) noexcept {
        if(s.phase!=Phase::towerArriving || run!=s.run || phase!=WorldPhase::arrived) {return false;}
        s.phase=Phase::complete;s.deadline=0;return true;
    }
    Frame frame(std::uint64_t run) const noexcept {
        const bool active=run==s.run && s.phase>=Phase::preparing && s.phase<=Phase::towerRequested;
        return {s.run,s.revision,active,s.play};
    }
    bool incident(std::uint64_t run,const cinematics::Incident& e,std::uint64_t now) noexcept {
        if(run!=s.run || e.registry!=kMovie.registry || e.type!=6 || e.slot!=0 || e.runtime==UINT64_MAX) {return false;}
        if(e.target==5239 && s.phase==Phase::offered) {s.phase=Phase::playing;s.deadline=now+650000;return true;}
        if(e.target==3338 && s.phase==Phase::playing) {s.play=false;++s.revision;s.phase=Phase::stopping;s.deadline=now+15000;return true;}
        if(e.target!=1685 || (s.phase!=Phase::playing && s.phase!=Phase::stopping)) {return false;}
        s.play=false;++s.revision;s.phase=Phase::towerRequested;s.deadline=0;return true;
    }
    native::Authority project(ActivityInstanceKey activity,std::uint64_t run,std::uint64_t member,native::Observation o,std::uint64_t now) noexcept {
        if(!frame(run).enabled || !activity || !member || member==UINT64_MAX) {return {};}
        if(!s.activity) {
            if(s.phase!=Phase::preparing || o.local.state!=0 || (!o.hasTeleport && o.local!=native::Teleport{})) {return {};}
            s.activity=activity;s.member=member;auto token=static_cast<std::uint8_t>(o.local.token+1);if(!token) {token=1;}
            s.host={1,token,25,0x2EA8FB98U};
        }
        if(s.activity!=activity || s.member!=member) {return {};}
        if(o.hasTeleport && o.local.token==s.host.token && o.local.sliceSetIndex==s.host.sliceSetIndex && o.local.sliceSetHash==s.host.sliceSetHash) {
            if(s.host.state==1 && o.local.state==3 && o.hasRegion && o.currentRegion==25) {
                s.host.state=3;s.phase=Phase::offered;s.play=true;++s.revision;s.deadline=now+60000;
            } else if(s.host.state==3 && o.local.state==0) {s.host.state=0;}
        }
        return {s.host,true,s.host.state!=1,s.host.state==0};
    }
    void tick(std::uint64_t now) noexcept {if(s.deadline && now>=s.deadline) {s.play=false;++s.revision;s.phase=Phase::failed;s.deadline=0;}}
    const State& state() const noexcept {return s;}
private:State s{};
};
inline std::mutex mutex;
inline Sequence sequence;
inline State state() noexcept {const std::lock_guard lock(mutex);return sequence.state();}
inline bool active() noexcept {const auto s=state();return s.phase!=Phase::idle && s.phase!=Phase::complete && s.phase!=Phase::failed;}
inline bool begin(coo::Generation owner) noexcept {const std::lock_guard lock(mutex);return sequence.begin(owner);}
inline bool queued(std::uint64_t run,std::int16_t index,std::uint64_t now) noexcept {const std::lock_guard lock(mutex);return sequence.queued(run,index,now);}
inline void selected(std::uint64_t run,std::int16_t index,std::uint32_t scenario,std::uint64_t now) noexcept {const std::lock_guard lock(mutex);sequence.selected(run,index,scenario,now);}
inline bool arrived(std::uint64_t run,WorldPhase phase) noexcept {const std::lock_guard lock(mutex);return sequence.arrived(run,phase);}
inline Frame frame(std::uint64_t run) noexcept {const std::lock_guard lock(mutex);return sequence.frame(run);}
inline bool incident(std::uint64_t run,const cinematics::Incident& e,std::uint64_t now) noexcept {const std::lock_guard lock(mutex);return sequence.incident(run,e,now);}
inline void tick(std::uint64_t now) noexcept {const std::lock_guard lock(mutex);sequence.tick(now);}
inline native::Authority project(ActivityInstanceKey activity,std::uint64_t run,std::uint64_t member,bool exact,native::Observation o,std::uint64_t now) noexcept {
    const std::lock_guard lock(mutex);return exact?sequence.project(activity,run,member,o,now):native::Authority{};
}
inline bool matches(const Frame& f,std::uint32_t key,std::uint8_t type,std::uint16_t slot) noexcept {return f.enabled && key==kMovie.registry && type==6 && slot==0;}
template<class W> bool write(W& w,const Frame& f) noexcept {
    cinematics::State movie{};movie.owner={f.run,1};movie.revisions[0]=f.revision;movie.play=f.play;return cinematics::write(w,movie,0);
}
}
