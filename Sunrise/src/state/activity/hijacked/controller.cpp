#include "controller.h"
#include "plate_presentation.h"
#include "boss_damage.h"
#include "route_geometry.h"
#include <cmath>
#include <algorithm>
namespace sunrise::state::activity::hijacked {
bool valid_document(const coo::script::Views& v) noexcept {
    return v.valid && v.missionId=="hijacked" && v.profileId==kProfile.id && !v.phases.empty()
        && v.mission.modules.size()==1 && v.mission.modules[0].asset==kModule && v.mission.modules[0].id==1
        && coo::script::authorized(v,kProfile);
}
bool contains(const Volume& v,Point p) noexcept {
    if(!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z) || v.vertices.size()<3
        || p.x<v.min.x || p.x>v.max.x || p.y<v.min.y || p.y>v.max.y || p.z<v.min.z || p.z>v.max.z) {return false;}
    bool inside{};for(std::size_t i=0,j=v.vertices.size()-1;i<v.vertices.size();j=i++) {
        const auto a=v.vertices[j],b=v.vertices[i];const double dx=double(b.x)-a.x,dy=double(b.y)-a.y,px=double(p.x)-a.x,py=double(p.y)-a.y;
        if(dx==0 && dy==0) {continue;}
        if(std::abs(dx*py-dy*px)<0.00001 && px*dx+py*dy>=0 && px*dx+py*dy<=dx*dx+dy*dy) {return true;}
        if((a.y>p.y)!=(b.y>p.y) && p.x<dx*py/dy+a.x) {inside=!inside;}
    }return inside;
}
void Controller::reset() noexcept {
    executor_.cancel(*this);composition_.reset();lifecycle_.reset();views_=nullptr;run_=now_=0;cleanupEntered_=started_=arrived_=phaseFinished_=false;
    clock_.reset();objectives_={};dialogue_={};objects_={};population_={};plates_={};charging_={};scans_={};
    suspendedEnemies_={};submitted_.reset();voiceEnd_={};seen_.reset();inside_.reset();arrivals_.reset();frame_={};
}
bool Controller::select(const coo::script::Views& v,std::uint64_t run) noexcept {
    if(!run || !valid_document(v)) {reset();return false;}if(run_==run) {return views_==&v;}
    reset();if(!lifecycle_.begin(run,128) || !objects_.begin(lifecycle_.owner(),kObjectBindings)) {return false;}
    views_=&v;run_=run;frame_.spawnGeneration=owner().value;static_cast<void>(arrivals_.bind(owner()));
    for(std::size_t i=0;i<std::size(kSpawns);++i) {const auto t=kSpawns[i].registry==kBoss.registry && kSpawns[i].source==kBoss.slot?kBossTactics[0]:kSpawns[i].tactical;population_.policy(i,{true,coo::EnemyIntent::combat,t.registry,t.slot,t.row});}
    return true;
}
bool Controller::entered(coo::Asset a,bool current) const noexcept {
    for(std::size_t i=0;i<std::size(kVolumes);++i) {if(kVolumes[i].asset==a) {return current?inside_[i]:seen_[i];}}return false;
}
void Controller::position(std::uint64_t run,Point p,std::uint64_t eventTime) noexcept {
    if(!views_ || run!=run_ || frame_.finished) {return;}inside_.reset();
    for(std::size_t i=0;i<std::size(kVolumes);++i) {if(contains(kVolumes[i],p)) {inside_.set(i);}}
    if(!arrived_) {arrived_=!views_->observationStart || entered(views_->observationStart->asset,true);}
    if(arrived_) {seen_|=inside_;cleanupEntered_|=exterior_cleanup_contains(p);}
    if(arrived_ && eventTime) {for(std::size_t i=0;i<inside_.size();++i) {if(inside_[i]) {static_cast<void>(arrivals_.mark(owner(),i,eventTime));}}}
    for(std::size_t i=0;i<std::size(kPlates);++i) {
        auto& s=frame_.plates[i];const bool occupied=entered(kPlates[i].volume,true);
        if(s.occupied==occupied) {continue;}s.occupied=occupied;
        if(!s.charged) {if(s.revision==UINT32_MAX) {frame_.populationFault=true;return;}++s.revision;charging_[i]=0;}
        ++frame_.revision;
    }
}
bool Controller::request(coo::Asset a,bool active) noexcept {
    const auto i=asset_index(a);if(i==std::size(kAssets)) {return false;}auto& s=frame_.native[i];
    if(a.type==1 && s.retired && active) {return false;}
    if(s.managed && s.desired==active) {return true;}
    if(!s.managed) {s.managed=true;s.generation=frame_.spawnGeneration;s.prepared=a.type!=4 && a.type!=1;}
    else if(a.type!=1 && a.type!=4) {if(s.generation>=32766 || !lifecycle_.reserve_through(owner(),s.generation+1)) {return false;}++s.generation;}
    s.desired=active;s.acknowledged=false;
    if(a.type==1 && !active) {
        // Native authority treats a new source generation as retirement of its
        // previous actors. Retain the zero-count publication for that generation.
        if(s.generation>=32766 || !lifecycle_.reserve_through(owner(),s.generation+1)) {return false;}
        ++s.generation;s.retired=true;
    }
    if(a.type==4) {
        const auto o=object_index(a);if(o==kObjectBindings.size()) {return false;}
        if(!active) {objects_.retire(o);}const auto state=objects_.state(o);
        // A retired source is never silently resurrected with an old lease.
        if(active && state.phase==coo::ObjectPhase::retired) {return false;}
        s.generation=state.generation;s.prepared=state.phase>=coo::ObjectPhase::create;s.active=active && state.create;
    } else {s.active=active;}
    if(a.type==1 && active) {const auto n=spawn_index(a);if(n==std::size(kSpawns)) {return false;}population_.enable(n);}
    ++frame_.revision;return true;
}
bool Controller::prepared(coo::Generation gen,coo::Asset a) noexcept {
    const auto i=asset_index(a);if(!frame_.enabled || gen!=owner() || i==std::size(kAssets)) {return false;}
    auto& s=frame_.native[i];if(!s.managed || !s.desired || s.prepared) {return false;}
    // Enemy counts stay zero until the native placement adapter has resolved all
    // authored anchor entities. This is distinct from combatant admission/readiness.
    if(a.type==1) {if(s.suspended || !s.active) {return false;}s.prepared=true;++frame_.revision;return true;}
    const auto o=object_index(a);if(o==kObjectBindings.size() || !objects_.prepared(gen,o)) {return false;}
    const auto object=objects_.state(o);s.generation=object.generation;s.prepared=true;s.active=object.create;++frame_.revision;return true;
}
bool Controller::object(const coo::ObjectReceipt& r) noexcept {
    const auto i=asset_index(r.source),o=object_index(r.source);if(!frame_.enabled || i==std::size(kAssets) || o==kObjectBindings.size() || !frame_.native[i].active) {return false;}
    if(!objects_.observe(o,r,true,0.F,1)) {return false;}frame_.native[i].acknowledged=true;++frame_.revision;return true;
}
bool Controller::device(coo::Generation gen,coo::Asset a,std::int16_t revision,float value) noexcept {
    const auto i=asset_index(a);if(!frame_.enabled || i==std::size(kAssets) || a.type!=23 || gen.run!=run_ || !std::isfinite(value)) {return false;}
    auto& s=frame_.native[i];if(!s.managed || s.acknowledged || gen.value!=s.generation || revision!=static_cast<std::int16_t>(s.generation) || value!=device_position(frame_,a)) {return false;}
    s.acknowledged=true;++frame_.revision;return true;
}
bool Controller::enemy_active(const EnemyReceipt& r) const noexcept {
    const auto* source=find(r.registry,1,r.source);if(!source) {return false;}
    const auto& state=frame_.native[asset_index(source->asset)];
    return state.managed && state.active && !state.retired && !state.suspended && r.generation==state.generation;
}
bool Controller::admitted(const EnemyReceipt& r) noexcept {
    const auto* source=find(r.registry,1,r.source);
    if(!frame_.enabled || frame_.finished || !r.valid() || r.run!=run_ || !enemy_active(r)
        || !source || !frame_.native[asset_index(source->asset)].prepared) {return false;}
    const auto& state=frame_.native[asset_index(source->asset)];
    if(state.survivingRequested!=UINT8_MAX) {
        // Only native admissions from the rearmed source can fill a suspended
        // survivor's slot. A killed or already rebound slot is never available.
        for(auto& previous:suspendedEnemies_) {
            if(previous.valid() && previous.registry==r.registry && previous.source==r.source
                && population_.rebind(previous,r)) {previous={};return true;}
        }
        return false;
    }
    const auto result=population_.admit(kCohorts,r,run_,frame_.spawnGeneration);
    if(result==coo::Admission::overflow) {frame_.populationFault=true;}return result==coo::Admission::accepted;
}
bool Controller::died(const EnemyReceipt& r) noexcept {
    return frame_.enabled && !frame_.finished && enemy_active(r) && population_.died(r,run_,r.generation);
}
bool Controller::suspend_exterior(coo::Generation run,std::uint16_t slot,std::span<const EnemyReceipt> survivors) noexcept {
    if(run!=owner() || !frame_.enabled || frame_.finished || !exterior_source(slot)) {return false;}
    const auto a=find(0x3E9B74F3U,1,slot)->asset;auto& state=frame_.native[asset_index(a)];
    const auto index=spawn_index(a);
    if(!state.managed || !state.active || !state.desired || state.retired || state.suspended
        || state.generation>=32766 || !population_.admitted(index,kSpawns[index].count)) {return false;}
    std::array<EnemyReceipt,16> living{};std::size_t count{};
    population_.living([&](const EnemyReceipt& r) {
        if(r.registry==a.registry && r.source==slot && count<living.size()) {living[count++]=r;}
    });
    if(count!=survivors.size()) {return false;}
    for(std::size_t i=0;i<count;++i) {
        bool present{};for(const auto& r:survivors) {present|=r==living[i];}
        if(!present) {return false;}
    }
    std::size_t free{};for(const auto& r:suspendedEnemies_) {free+=!r.valid();}
    if(free<count || !lifecycle_.reserve_through(owner(),state.generation+1)) {return false;}
    for(std::size_t i=0;i<count;++i) for(auto& r:suspendedEnemies_) {
        if(!r.valid()) {r=living[i];break;}
    }
    ++state.generation;state.suspended=true;state.active=false;state.prepared=false;
    state.survivingRequested=static_cast<std::uint8_t>(count);++frame_.revision;return true;
}
bool Controller::resume_exterior(coo::Generation run,std::uint16_t slot) noexcept {
    if(run!=owner() || !frame_.enabled || frame_.finished || !exterior_source(slot)) {return false;}
    auto& s=frame_.native[asset_index(find(0x3E9B74F3U,1,slot)->asset)];
    if(!s.suspended || !s.desired || s.retired || s.active || s.prepared) {return false;}
    s.suspended=false;s.active=true;++frame_.revision;return true;
}
BossRequest Controller::boss_request() const noexcept {
    BossRequest out{owner(),{},frame_.bossStage,frame_.bossRevision,false};
    if(!frame_.enabled || frame_.finished) {return out;}
    population_.living([&](const EnemyReceipt& r) {if(r.registry==kBoss.registry && r.source==kBoss.slot) {out.enemy=r;out.requested=frame_.bossMoveRequested && !frame_.bossPositioned;}});
    return out;
}
bool Controller::boss_position(const EnemyReceipt& r,std::uint8_t stage,std::uint32_t revision) noexcept {
    const auto q=boss_request();if(!q.requested || q.enemy!=r || q.stage!=stage || q.revision!=revision) {return false;}
    frame_.bossPositioned=true;frame_.bossMoveRequested=false;++frame_.revision;return true;
}
bool Controller::health(const EnemyReceipt& r,float fraction) noexcept {
    if(!frame_.enabled || frame_.finished || !std::isfinite(fraction) || fraction<0.F || fraction>1.F
        || r.run!=run_ || r.generation!=frame_.spawnGeneration || r.registry!=kBoss.registry || r.source!=kBoss.slot) {return false;}
    bool owned{};population_.living([&](const EnemyReceipt& live) {if(live==r) {owned=true;}});
    if(!owned || fraction>=frame_.bossHealth) {return false;}frame_.bossHealth=fraction;++frame_.revision;return true;
}
bool Controller::health_event(const EnemyReceipt& r,float fraction,std::uint64_t now) noexcept {
    const auto stage=frame_.bossStage;
    if(!health(r,fraction)) {return false;}
    // Evaluate the existing Lua health-gate edges at the health event, not at
    // the next authority publication. No engine call runs under this update.
    if(frame_.section==2 && stage<2 && fraction<=boss_damage::floor(stage)) {
        static_cast<void>(update(run_,(std::max)(now,now_),true));
    }
    return true;
}
bool Controller::plate_pose(const PlateReceipt& r,server::runtime::activity::mission_device_pose::Sample sample) noexcept {
    if(!r.valid() || r!=plates_[r.index] || !frame_.enabled || r.owner.run!=run_)return false;
    const auto& native=frame_.native[asset_index(kPlates[r.index].source)];
    if(!native.active || native.generation!=r.owner.value)return false;
    return server::runtime::activity::mission_device_pose::observe(frame_.plateCaptures[r.index].pose,sample);
}
bool Controller::bind_plate(const PlateReceipt& r) noexcept {
    if(!r.valid() || !frame_.enabled || r.owner.run!=run_) {return false;}
    const auto& s=frame_.native[asset_index(kPlates[r.index].source)];
    if(!s.active || s.generation!=r.owner.value || plates_[r.index].valid()) {return false;}plates_[r.index]=r;return true;
}
bool Controller::contested_positions(const PlateReceipt& r,std::span<const EnemyPosition> positions,bool complete) noexcept {
    if(!r.valid() || r!=plates_[r.index] || positions.size()>256) {return false;}
    const Volume* volume{};for(const auto& v:kVolumes) {if(v.asset==kPlates[r.index].volume) {volume=&v;break;}}
    if(!volume) {return false;}
    bool occupied{};
    population_.living([&](const EnemyReceipt& enemy) {
        const EnemyPosition* sample{};
        for(const auto& p:positions) {if(p.enemy==enemy) {sample=&p;break;}}
        if(!sample || !std::isfinite(sample->point.x) || !std::isfinite(sample->point.y) || !std::isfinite(sample->point.z)) {complete=false;return;}
        occupied|=contains(*volume,sample->point);
    });
    // Missing or newly admitted actors cannot become evidence of an empty plate.
    return (occupied || complete) && contested(r,occupied);
}
bool Controller::contested(const PlateReceipt& r,bool value) noexcept {
    if(!r.valid() || !frame_.enabled || r!=plates_[r.index]) {return false;}
    auto& s=frame_.plates[r.index];if(s.contested==value || s.charged) {return false;}
    if(s.revision==UINT32_MAX) {frame_.populationFault=true;return false;}s.contested=value;++s.revision;charging_[r.index]=0;++frame_.revision;return true;
}
bool Controller::plate(const PlateReceipt& r,std::uint32_t revision,float value,bool completed) noexcept {
    if(!r.valid() || !frame_.enabled || r!=plates_[r.index] || r.owner.run!=run_ || !std::isfinite(value) || value<0 || value>1) {return false;}
    auto& s=frame_.plates[r.index];if(!s.armed || !s.occupied || s.contested || s.charged || revision!=s.revision) {return false;}
    if(!completed && value<1.F) {charging_[r.index]=revision;return false;}
    if(!completed || value!=1.F || charging_[r.index]!=revision) {return false;}s.charged=true;
    ++frame_.revision;return true;
}
bool Controller::bind_scan(const ScanReceipt& r) noexcept {
    if(!r.valid() || !frame_.enabled || r.owner.run!=run_) {return false;}
    const auto& s=frame_.native[asset_index(kScans[r.index].source)];
    if(!s.active || s.generation!=r.owner.value || scans_[r.index].valid()) {return false;}scans_[r.index]=r;return true;
}
bool Controller::scan_playback(const ScanReceipt& r,ScanPlayback playback,bool participant) noexcept {
    if(!r.valid()) {return false;}
    if(playback.started(r.owner.value,participant)) {return scan(r,true,false);}
    if(playback.finished(r.owner.value,frame_.scanStarted[r.index])) {return scan(r,false,true);}
    return false;
}
bool Controller::scan(const ScanReceipt& r,bool started,bool completed) noexcept {
    if(!r.valid() || !frame_.enabled || r!=scans_[r.index] || r.owner.run!=run_ || !frame_.scanArmed[r.index] || frame_.scanComplete[r.index]) {return false;}
    if(started && !completed && !frame_.scanStarted[r.index]) {frame_.scanStarted[r.index]=true;++frame_.revision;return true;}
    if(!completed || !frame_.scanStarted[r.index]) {return false;}frame_.scanComplete[r.index]=true;++frame_.revision;return true;
}
bool Controller::submitted(std::uint64_t run,std::uint32_t bank,std::uint8_t row,std::uint32_t generation,std::uint64_t now) noexcept {
    if(!views_ || run!=run_ || !frame_.enabled || !dialogue_.submitted(views_->dialogue,bank,row,generation,now,frame_,frame_.revision)) {return false;}
    submitted_.set(row);voiceEnd_[row]=dialogue_.voice_until();return true;
}
bool Controller::publish(const coo::Command& c) noexcept {
    if(!views_ || !graph() || !coo::script::valid_token(graph()->definition,executor_,c)) {return false;}const auto& s=c.spec;
    switch(s.operation) {
    case coo::Operation::objective: dialogue_.objective(views_->dialogue,s.argument,frame_,frame_.revision);objectives_.set(s.argument,marker(s.argument));break;
    case coo::Operation::dialogue: dialogue_.enqueue(views_->dialogue,static_cast<std::uint8_t>(s.argument),now_,0,frame_.section,frame_.revision);break;
    case coo::Operation::population: case coo::Operation::device: return request(s.asset,s.argument!=0);
    case coo::Operation::mechanic:
        // Explicit encounter exit follows this mission's authentic final scan.
        if(s.asset==kModule && s.argument==12) {if(!frame_.scanComplete.back()) {return false;}phaseFinished_=true;break;}
        if(s.asset==kModule && (s.argument==10 || s.argument==11)) {frame_.restricted=s.argument==11;++frame_.revision;break;}
        if(s.asset==kBoss && s.argument>=20 && s.argument<=22) {
            if(frame_.bossRevision==UINT32_MAX) {return false;}frame_.bossStage=static_cast<std::uint8_t>(s.argument-20);
            ++frame_.bossRevision;frame_.bossMoveRequested=true;frame_.bossPositioned=false;++frame_.revision;return true;
        }
        for(std::size_t i=0;i<std::size(kPlates);++i) {if(s.asset==kPlates[i].source && s.argument==10) {frame_.plates[i].armed=true;++frame_.revision;return true;}}
        for(std::size_t i=0;i<std::size(kScans);++i) {if(s.asset==kScans[i].source && s.argument==10) {frame_.scanArmed[i]=true;++frame_.revision;return request(s.asset,true);}}
        return false;
    case coo::Operation::complete:
        if(!lifecycle_.complete(owner())) {return false;}frame_.finished=true;frame_.restricted=false;objectives_.clear();break;
    case coo::Operation::observation:case coo::Operation::eventAfter:break;
    default:return false;
    }return true;
}
bool Controller::observed(const coo::CommandSpec& s) const noexcept {
    if(views_ && views_->condition(s)) {return views_->evaluate(s,[this](const auto& child) {return observed(child);});}
    if(s.operation==coo::Operation::eventAfter) {
        for(std::size_t i=0;i<std::size(kVolumes);++i) {if(kVolumes[i].asset==s.asset) {return arrivals_.elapsed(i,now_,s.argument);}}return false;
    }
    if(s.asset==kModule && s.argument==30) {return cleanupEntered_;}
    if(s.asset==kBoss && s.argument>=100 && s.argument<=101) {return frame_.bossHealth<=boss_damage::floor(static_cast<std::uint8_t>(s.argument-100));}
    if(s.asset==kDialogueAsset) {return s.argument<std::size(kDialogue) && submitted_[s.argument] && now_>=voiceEnd_[s.argument];}
    if(s.asset.type==1) {const auto i=spawn_index(s.asset);return i<std::size(kSpawns) && !frame_.native[asset_index(s.asset)].retired && population_.cleared(i,kSpawns[i].count);}
    for(std::size_t i=0;i<std::size(kPlates);++i) {if(s.asset==kPlates[i].source) {return s.argument==11?frame_.plates[i].armed && frame_.plates[i].occupied:frame_.plates[i].charged;}}
    for(std::size_t i=0;i<std::size(kScans);++i) {if(s.asset==kScans[i].source) {return s.argument==11?frame_.scanStarted[i]:frame_.scanComplete[i];}}
    return entered(s.asset,s.argument==1);
}
coo::StallDetail Controller::missing(const coo::CommandSpec& s) const noexcept {
    if(s.wait==coo::Wait::requested || (coo::is_observation(s.operation) && observed(s))) {return {};}
    if(s.asset.type==1) {const auto i=spawn_index(s.asset);auto detail=population_.missing(i,i<std::size(kSpawns)?kSpawns[i].count:0,s.operation==coo::Operation::observation);detail.asset=s.asset;return detail;}
    if(s.operation==coo::Operation::dialogue) {return {coo::Missing::dialogue,s.asset,s.argument};}
    if(coo::is_observation(s.operation)) {
        for(std::size_t i=0;i<std::size(kScans);++i) {if(s.asset!=kScans[i].source) {continue;}
            if(!frame_.native[asset_index(s.asset)].acknowledged) {return {coo::Missing::object,s.asset};}
            if(!scans_[i].valid()) {return {coo::Missing::controller,s.asset};}
            return {coo::Missing::observation,s.asset,1U,
                (s.argument==11?frame_.scanStarted[i]:frame_.scanComplete[i])?1U:0U,s.argument};
        }
    }
    if(s.asset.type==4) {return {coo::Missing::object,s.asset};}return {coo::Missing::observation,s.asset};
}
void Controller::update_module(std::uint32_t id,const coo::MissionInput& input,Frame& output) noexcept {
    if(id!=1 || !views_ || input.run!=run_ || !arrived_) {return;}now_=input.now;frame_.enabled=!frame_.populationFault;
    for(std::size_t i=0;i<seen_.size();++i) {if(seen_[i]) {static_cast<void>(arrivals_.mark(owner(),i,now_));}}
    frame_.gameplayClockTicks=clock_.sample(now_);if(!frame_.enabled) {output=frame_;return;}
    if(!started_) {started_=executor_.start(graph()->definition,run_);}if(!started_) {return;}
    executor_.update(*this);
    for(const auto& b:graph()->commands) {
        const auto state=executor_.step_state(b.step);if(state.phase!=coo::StepPhase::active || !state.commands[b.command].requested) {continue;}
        const auto& s=graph()->definition.steps[b.step].commands[b.command];const auto token=executor_.token(b.step,b.command);
        if(coo::is_observation(s.operation) && observed(s)) {static_cast<void>(executor_.enqueue({token,coo::Milestone::observed}));}
        else if(s.operation==coo::Operation::dialogue && s.argument<std::size(kDialogue) && submitted_[s.argument]) {static_cast<void>(executor_.enqueue({token,coo::Milestone::nativeReady}));}
        else if(s.operation==coo::Operation::mechanic && s.asset==kBoss && s.argument==20U+frame_.bossStage && (frame_.bossPositioned || population_.cleared(spawn_index(kBoss),1))) {static_cast<void>(executor_.enqueue({token,coo::Milestone::nativeReady}));}
        else if(s.operation==coo::Operation::population) {const auto i=spawn_index(s.asset);if(i<std::size(kSpawns) && population_.ready(i,kSpawns[i].count)) {static_cast<void>(executor_.enqueue({token,coo::Milestone::nativeReady}));}}
        else if(s.operation==coo::Operation::device && s.asset.type==4 && frame_.native[asset_index(s.asset)].acknowledged) {static_cast<void>(executor_.enqueue({token,coo::Milestone::nativeReady}));}
    }
    executor_.update(*this);executor_.update(*this);
    dialogue_.advance(views_->dialogue,frame_.spawnGeneration-1U,now_,false,frame_,frame_.revision);
    if((executor_.diagnostics().phase==coo::Phase::complete || (phaseFinished_ && executor_.diagnostics().phase!=coo::Phase::failed))
        && std::size_t(frame_.section)+1<views_->phases.size()) {
        phaseFinished_=false;++frame_.section;executor_.cancel(*this);started_=executor_.start(graph()->definition,run_);if(started_) {executor_.update(*this);}++frame_.revision;
    }
    frame_.enabled=!frame_.populationFault && executor_.diagnostics().phase!=coo::Phase::failed;
    for(std::size_t i=0;i<std::size(kPlates);++i) {
        const auto& plate=frame_.plates[i];
        if(!server::runtime::activity::mission_capture::update(frame_.plateCaptures[i],plate.revision,
            frame_.native[asset_index(kPlates[i].source)].active && plate.armed && plate.occupied && !plate.contested,
            plate.charged,coo::native_activity_ticks(static_cast<std::uint64_t>(kPlates[i].chargeSeconds*1000.F)),frame_.gameplayClockTicks)) {
            frame_.populationFault=true;frame_.enabled=false;
        }
        frame_.plateCaptures[i].presentationPosition=plate_presentation::position(plate);
        if(!server::runtime::activity::mission_device_pose::desire(frame_.plateCaptures[i].pose,frame_.plateCaptures[i].presentationPosition)) {frame_.enabled=false;}
    }
    frame_.checked=frame_.finished;frame_.presentation=objectives_.state();frame_.completion=lifecycle_.publication();output=frame_;
}
Frame Controller::update(std::uint64_t run,std::uint64_t now,bool ready) noexcept {
    if(!views_ || run!=run_ || !ready) {return {};}return composition_.update(views_->mission,{run,now,0,false,true},*this);
}
}
