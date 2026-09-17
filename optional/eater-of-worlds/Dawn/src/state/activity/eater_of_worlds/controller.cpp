#include "controller.h"
#include "doors.h"
#include "reactor_combat.h"
#include "hoop_crossing.h"
#include <cmath>
namespace dawn::state::activity::eater_of_worlds {
namespace {
bool current_player_dead(const ReactorState& reactor) noexcept {
    return reactor.player!=UINT32_MAX && reactor.aliveKnown
        && reactor.healthPlayer==reactor.player && reactor.dead;
}
}
bool valid_document(const coo::script::Views& v) noexcept {
    return v.valid && v.missionId=="eater_of_worlds" && v.profileId==kProfile.id
        && v.phases.size()==5 && v.mission.modules.size()==1
        && v.mission.modules[0].asset==kModule && v.mission.modules[0].id==1
        && coo::script::authorized(v,kProfile);
}
bool contains(const Volume& v,Point p) noexcept {
    if(!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z) || v.vertices.size()<3
        || p.x<v.min.x || p.x>v.max.x || p.y<v.min.y || p.y>v.max.y || p.z<v.min.z || p.z>v.max.z) return false;
    bool inside{};
    for(std::size_t i=0,j=v.vertices.size()-1;i<v.vertices.size();j=i++) {
        const auto a=v.vertices[j],b=v.vertices[i];const double dx=double(b.x)-a.x,dy=double(b.y)-a.y,px=double(p.x)-a.x,py=double(p.y)-a.y;
        if(dx==0 && dy==0) continue;
        if(std::abs(dx*py-dy*px)<0.00001 && px*dx+py*dy>=0 && px*dx+py*dy<=dx*dx+dy*dy) return true;
        if((a.y>p.y)!=(b.y>p.y) && p.x<dx*py/dy+a.x) inside=!inside;
    }return inside;
}
void Controller::reset() noexcept {
    executor_.cancel(*this);composition_.reset();lifecycle_.reset();views_=nullptr;run_=now_=0;started_=false;
    clock_.reset();objects_={};population_={};readinessSchedule_.reset();costs_={};resumeSources_.reset();dialogue_={};objectives_={};
    submitted_.reset();voiceEnd_={};seen_.reset();triggered_.reset();regions_.reset();healthOwners_={};carryOwners_={};stationOwners_={};frame_={};
}
bool Controller::select(const coo::script::Views& v,std::uint64_t run) noexcept {
    if(!run || !valid_document(v)) {reset();return false;}
    if(run==run_) return views_==&v;
    reset();if(!lifecycle_.begin(run,128) || !objects_.begin(owner(),kObjectBindings)) return false;
    views_=&v;run_=run;frame_.spawnGeneration=owner().value;return true;
}
bool Controller::current_group(std::uint32_t key) const noexcept {
    if(frame_.region<0 || frame_.region>=64 || frame_.region%8) return false;
    for(const auto& g:kGroups) if(g.key==key) return (g.bubblesMask&(1U<<(frame_.region/8)))!=0;
    return false;
}
void Controller::position(std::uint64_t run,Point p) noexcept {
    if(!views_ || run!=run_ || !frame_.enabled) return;
    for(std::size_t i=0;i<std::size(kVolumes);++i) {
        const auto& v=kVolumes[i];if(!current_group(v.asset.registry) || !contains(v,p)) continue;
        if(v.asset.registry==0x686321C8U && v.asset.slot>=235 && v.asset.slot<=238)
            static_cast<void>(path_goal(static_cast<std::uint8_t>(v.asset.slot-234)));
        if(frame_.section==3 && frame_.region==48) {
            const bool checkpoint=v.asset.registry==kThunderingWallCheckpoint.volume.registry
                && v.asset.slot==kThunderingWallCheckpoint.volume.slot;
            bool arena{};
            for(const auto& target:kBarrierArenaArrivalVolumes)
                arena|=v.asset.registry==target.registry && v.asset.slot==target.slot;
            if(arena && !frame_.arrival.intro) static_cast<void>(stage_argos_intro());
            if(checkpoint && frame_.checkpointSpawnSet!=kBarrierArenaSpawn) {
                const auto spawn=kThunderingWallCheckpointSpawn;
                if(frame_.checkpointSliceSet!=48 || frame_.checkpointSpawnSet!=spawn) {
                    frame_.checkpointSliceSet=48;frame_.checkpointSpawnSet=spawn;++frame_.revision;
                }
            }
        }
        if(seen_[i]) continue;
        seen_.set(i);++frame_.revision;
    }
}
bool Controller::entered(coo::Asset a) const noexcept {
    for(std::size_t i=0;i<std::size(kVolumes);++i) if(kVolumes[i].asset.registry==a.registry && kVolumes[i].asset.slot==a.slot) return seen_[i];
    return false;
}
bool Controller::trigger(std::uint64_t run,std::uint32_t key,std::uint16_t slot) noexcept {
    const auto* a=find(key,31,slot);
    if(run!=run_ || !frame_.enabled || !a || !current_group(key)) return false;
    const auto i=asset_index(a->asset);if(triggered_[i]) return false;
    triggered_.set(i);++frame_.revision;return true;
}
void Controller::monitor(std::uint32_t key,std::uint16_t slot,bool any,std::int32_t count,std::int32_t value) noexcept {
    // Type30 echoes our lifecycle generation; old presence deltas cannot satisfy a new attempt.
    const auto* a=find(key,30,slot);
    if(!frame_.enabled || !a || !current_group(key) || !any || count!=1 || value!=static_cast<std::int32_t>(frame_.spawnGeneration)) return;
    const auto i=asset_index(a->asset);if(!triggered_[i]) {triggered_.set(i);++frame_.revision;}
    // Goal completion uses a current authenticated body sample in the exact
    // authored goal volume. A queued type-30 delta has no contact timestamp and
    // can outlive a path retry even when its activity generation still matches.
}
bool Controller::request(coo::Asset a,bool active) noexcept {
    const auto i=asset_index(a);if(i==std::size(kAssets)) return false;
    if(a.type==1) {
        const auto n=spawn_index(a.registry,a.slot);if(n==std::size(kSpawns)) return false;
        auto& s=frame_.native[i];
        if(s.retiring) {resumeSources_[n]=active;return true;}
        if(s.managed && s.active==active) return true;
        if(!s.managed && !active) return true;
        const auto generation=s.managed?s.generation+1U:frame_.spawnGeneration;
        if(!generation || generation>32766 || !lifecycle_.reserve_through(owner(),generation)) return false;
        s.managed=s.prepared=true;s.generation=generation;s.desired=s.active=active;
        s.retiring=!active;s.acknowledged=false;resumeSources_.reset(n);
        costs_[n]={};frame_.taskPlusOne[n]=0;population_.reset_source(n);
        if(active) {
            population_.enable(n);const auto& row=kSpawns[n];
            population_.policy(n,{true,row.sceneOwned?coo::EnemyIntent::idleReveal:coo::EnemyIntent::combat,
                row.objective==65535?0U:a.registry,row.objective==65535?std::uint16_t{}:row.objective,-1});
        }
        ++frame_.revision;return true;
    }
    auto& s=frame_.native[i];if(s.managed && s.desired==active) return true;
    if(!s.managed) {s.managed=true;s.generation=frame_.spawnGeneration;s.prepared=a.type!=4;}
    else if(a.type!=4 && a.type!=1) {
        if(s.generation>=32766 || !lifecycle_.reserve_through(owner(),s.generation+1U)) return false;
        ++s.generation;
    }
    s.desired=active;s.position=active?1.F:0.F;s.acknowledged=false;
    if(a.type==4) {
        const auto o=object_index(a);if(o==kObjectBindings.size()) return false;
        if(!active) {objects_.retire(o);s.carried=s.dropped=false;s.carrier=UINT32_MAX;}
        const auto state=objects_.state(o);if(active && state.phase==coo::ObjectPhase::retired) return false;
        s.generation=state.generation;s.prepared=state.phase>=coo::ObjectPhase::create;s.active=active && state.create;
    }else s.active=active;
    ++frame_.revision;return true;
}
bool Controller::source_retired(std::uint64_t run,std::uint32_t key,std::uint16_t slot,std::uint32_t generation) noexcept {
    const auto n=spawn_index(key,slot);
    if(run!=run_ || !frame_.enabled || frame_.finished || n==std::size(kSpawns) || !current_group(key)) return false;
    auto& state=frame_.native[asset_index(kSpawns[n].asset)];
    if(!state.managed || !state.retiring || state.active || state.desired || generation!=state.generation) return false;
    state.retiring=false;state.acknowledged=true;++frame_.revision;
    if(resumeSources_[n]) {
        resumeSources_.reset(n);
        if(!request(kSpawns[n].asset,true)) {frame_.populationFault=true;return false;}
    }
    return true;
}
void Controller::crossing_population(bool active) noexcept {
    for(const auto& spawn:kSpawns) if(path_source(spawn.asset))
        if(!request(spawn.asset,active)) frame_.populationFault=true;
}
bool Controller::retiring_holdout() const noexcept {
    for(const auto& spawn:kSpawns)
        if(holdout_source(spawn.asset) && frame_.native[asset_index(spawn.asset)].retiring) return true;
    return false;
}
bool Controller::stage_argos_intro() noexcept {
    if(frame_.arrival.intro) return true;
    bool ok=request(kBossSource,true);
    const auto apply=[&](std::uint32_t key,std::uint8_t type,std::uint16_t slot,bool active) {
        const auto* asset=find(key,type,slot);return asset && request(asset->asset,active);
    };
    for(std::uint16_t slot=0;slot<6;++slot) ok=apply(kBarrierRegistry,4,slot,true) && ok;
    ok=apply(kArgosRegistry,4,104,true) && ok;
    // These are the platform controls, not the six debris-shell controls.
    // Do not request their object sources during the arrival introduction.
    for(std::uint16_t slot=58;slot<=80;++slot) ok=apply(kArgosRegistry,23,slot,false) && ok;
    if(!ok) frame_.populationFault=true;
    frame_.arrival.intro=ok;
    ++frame_.revision;return ok;
}
void Controller::pose_argos_shell() noexcept {
    if(!frame_.arrival.intro || frame_.arrival.shellPosed) return;
    for(std::uint16_t slot=0;slot<6;++slot) {
        const auto* asset=find(kBarrierRegistry,4,slot);
        if(!asset || !frame_.native[asset_index(asset->asset)].acknowledged) return;
    }
    // Apply geometry controls only after their object consumers exist. A gate
    // revision consumed before its object binds cannot establish the shell.
    for(std::uint16_t slot=6;slot<12;++slot) {
        const auto* asset=find(kBarrierRegistry,23,slot);
        if(!asset || !request(asset->asset,true)) {frame_.populationFault=true;return;}
    }
    frame_.arrival.shellPosed=true;++frame_.revision;
}
void Controller::retry_traversal() noexcept {
    if(frame_.section!=3) return;
    frame_.arrival.retry();
    // Suction is created only after both forward gates have opened. Retain
    // those openings and reassert their native revision after a death/rebind.
    // The entrance stays accessible to the revived solo player as well.
    const coo::Asset gates[]{kAirlock.entranceDoor,kAirlock.exitDoor,kEjectionTube};
    for(const auto gate:gates) {
        auto& state=frame_.native[asset_index(gate)];
        if(!state.managed || !state.desired) continue;
        if(state.generation>=32766 || !lifecycle_.reserve_through(owner(),state.generation+1)) {
            frame_.populationFault=true;continue;
        }
        ++state.generation;state.acknowledged=false;
    }
    if(frame_.grate.open) static_cast<void>(open_exit_grate());
    frame_.restricted=false;++frame_.revision;
}
bool Controller::arrival_motion(const ArrivalMotion& motion) noexcept {
    auto& arrival=frame_.arrival;
    if(!arrival_request(frame_,owner()).enabled || motion.run!=run_
        || motion.player==UINT32_MAX || motion.player!=frame_.reactor.player
        || motion.attempt!=arrival.attempt || current_player_dead(frame_.reactor)
        || !finite(motion.position) || !motion.observedAt || motion.observedAt<=arrival.sampledAt
        || motion.observedAt+500<now_ || (motion.observedAt>now_ && motion.observedAt-now_>1000)) return false;
    const auto oldHoop=arrival.hoop,oldLanded=arrival.landed;
    const auto delta=motion.observedAt-arrival.sampledAt;
    const auto dx=motion.position.x-arrival.previous.x,dy=motion.position.y-arrival.previous.y,
        dz=motion.position.z-arrival.previous.z;
    const bool continuous=arrival.sampled && delta<=250 && dx*dx+dy*dy+dz*dz<=80.F*80.F;
    if(continuous && !arrival.hoop) arrival.hoop=crossed_any_hoop(arrival.previous,motion.position);
    bool arena{};
    for(const auto& volume:kVolumes) if(barrier_arena_arrival_volume(volume.asset) && contains(volume,motion.position)) {arena=true;break;}
    // User-selected solo policy: one authenticated hoop crossing followed by
    // the authored Argos arrival trigger. This does not claim floor support.
    if(arrival.hoop && arena) {
        arrival.landed=true;frame_.checkpointSliceSet=48;frame_.checkpointSpawnSet=kBarrierArenaSpawn;
    }
    arrival.previous=motion.position;arrival.sampledAt=motion.observedAt;arrival.sampled=true;
    const bool changed=arrival.hoop!=oldHoop || arrival.landed!=oldLanded;
    if(changed) ++frame_.revision;
    return changed;
}
void Controller::retry_combat() noexcept {
    if(frame_.section==2 && !frame_.combatRetry) {
        executor_.cancel(*this);started_=false;frame_.combatRetry=true;frame_.restricted=false;
        for(const auto& spawn:kSpawns) if(holdout_source(spawn.asset))
            if(!request(spawn.asset,false)) frame_.populationFault=true;
        ++frame_.revision;
    }
}
bool Controller::prepared(coo::Generation gen,coo::Asset a) noexcept {
    const auto i=asset_index(a),o=object_index(a);
    if(!frame_.enabled || gen!=owner() || i==std::size(kAssets) || o==kObjectBindings.size()) return false;
    auto& s=frame_.native[i];if(!s.managed || !s.desired || s.prepared || !objects_.prepared(gen,o)) return false;
    const auto state=objects_.state(o);s.generation=state.generation;s.prepared=true;s.active=state.create;++frame_.revision;return true;
}
bool Controller::object(const coo::ObjectReceipt& receipt) noexcept {
    const auto i=asset_index(receipt.source),o=object_index(receipt.source);
    if(!frame_.enabled || i==std::size(kAssets) || o==kObjectBindings.size() || !frame_.native[i].active
        || !objects_.observe(o,receipt,true,0.F,1)) return false;
    frame_.native[i].acknowledged=true;++frame_.revision;return true;
}
bool Controller::open_exit_grate() noexcept {
    const auto index=asset_index(kReactorExitGrate);
    if(!frame_.enabled || frame_.finished || index>=std::size(kAssets)
        || !current_group(kReactorExitGrate.registry)) return false;
    const auto& source=frame_.native[index];
    if(!source.managed || !source.desired || !source.prepared || !source.active
        || !source.acknowledged || frame_.grate.revision==INT32_MAX) return false;
    ++frame_.grate.revision;frame_.grate.open=true;frame_.grate.poseAcknowledged=false;
    frame_.grate.object={};frame_.grate.device=frame_.grate.deviceSerial=UINT32_MAX;
    ++frame_.revision;return true;
}
bool Controller::grate_pose(const GratePoseReceipt& receipt) noexcept {
    const auto index=asset_index(kReactorExitGrate),objectIndex=object_index(kReactorExitGrate);
    const auto created=objects_.owner(objectIndex);
    if(!frame_.enabled || frame_.finished || index>=std::size(kAssets)
        || objectIndex>=kObjectBindings.size() || !created.valid()
        || receipt.object.source!=kReactorExitGrate || receipt.object.owner.run!=run_
        || receipt.object.owner.value!=frame_.native[index].generation
        || receipt.object.entity==UINT32_MAX || receipt.object.serial==UINT32_MAX
        || receipt.object.owner!=created.owner || receipt.object.entity!=created.entity
        || receipt.object.serial!=created.serial
        || receipt.device==UINT32_MAX || receipt.deviceSerial==UINT32_MAX
        || receipt.revision<0 || static_cast<std::uint32_t>(receipt.revision)!=frame_.grate.revision
        || !std::isfinite(receipt.position) || receipt.position!=(frame_.grate.open?1.F:0.F)
        || !current_group(kReactorExitGrate.registry)) return false;
    const auto& source=frame_.native[index];
    if(!source.managed || !source.desired || !source.prepared || !source.active
        || !source.acknowledged) return false;
    const bool same=frame_.grate.object==receipt.object && frame_.grate.device==receipt.device
        && frame_.grate.deviceSerial==receipt.deviceSerial;
    if(frame_.grate.poseAcknowledged && same) return false;
    frame_.grate.object=receipt.object;frame_.grate.device=receipt.device;
    frame_.grate.deviceSerial=receipt.deviceSerial;frame_.grate.poseAcknowledged=true;
    ++frame_.revision;return true;
}
bool Controller::platform_owner(const coo::ObjectReceipt& receipt) const noexcept {
    const auto i=asset_index(receipt.source),o=object_index(receipt.source);
    if(!frame_.enabled || frame_.finished || receipt.owner.run!=run_
        || platform_index(receipt.source)>=56 || i==std::size(kAssets) || o==kObjectBindings.size()
        || !current_group(receipt.source.registry)) return false;
    const auto& state=frame_.native[i];const auto created=objects_.owner(o);
    return state.active && state.desired && state.acknowledged && created.valid()
        && created.owner==receipt.owner && created.entity==receipt.entity && created.serial==receipt.serial;
}
bool Controller::platform_pose(const PlatformPoseReceipt& receipt) noexcept {
    if(!platform_owner(receipt.object)) return false;
    const auto i=platform_index(receipt.object.source);auto& reactor=frame_.reactor;
    if(!reactor.poseRevision[i] || receipt.revision!=reactor.poseRevision[i]
        || receipt.position!=(reactor.raised[i]?1.F:0.F) || reactor.poseAcknowledged[i]) return false;
    reactor.poseAcknowledged.set(i);++frame_.revision;return true;
}
void Controller::player(std::uint64_t run,std::uint32_t identity) noexcept {
    if(run!=run_ || !frame_.enabled || identity==UINT32_MAX || identity==frame_.reactor.player) return;
    const bool replaced=frame_.reactor.player!=UINT32_MAX;
    static_cast<void>(frame_.reactor.identity(identity));
    if(replaced) {retry_combat();retry_traversal();}
    ++frame_.revision;
}
bool Controller::player_health(std::uint64_t run,std::uint32_t identity,bool dead) noexcept {
    if(run!=run_ || !frame_.enabled || frame_.finished || identity==UINT32_MAX) return false;
    auto& reactor=frame_.reactor;
    if(!dead) {
        const bool changed=!reactor.aliveKnown || reactor.dead || reactor.healthPlayer!=identity;
        reactor.aliveKnown=true;reactor.dead=false;reactor.healthPlayer=identity;
        if(frame_.section==1 && reactor.path) frame_.restricted=true;
        if(changed) ++frame_.revision;
        return changed;
    }
    if(!reactor.aliveKnown || reactor.healthPlayer!=identity || reactor.dead) return false;
    reactor.dead=true;
    static_cast<void>(reactor.reset_attempt());
    retry_combat();
    retry_traversal();
    if(frame_.section==1) frame_.restricted=false;
    ++frame_.revision;return true;
}
bool Controller::platform_contact(const PlatformContact& receipt) noexcept {
    auto& reactor=frame_.reactor;
    // Solo policy: authenticated current-player occupancy supplies the positive
    // progression evidence. An unavailable health lookup is not a veto, and no
    // synthetic health observation is published. Confirmed death still wins for
    // this same player; an old actor's health cannot block its replacement.
    if(receipt.player==UINT32_MAX || receipt.player!=reactor.player
        || current_player_dead(reactor)
        || !platform_owner(receipt.object)
        || !receipt.observedAt || receipt.observedAt+500<now_
        || (receipt.observedAt>now_ && receipt.observedAt-now_>1000)) return false;
    if(!receipt.supported) {reactor.release(receipt.observedAt);return false;}
    const auto* policy=views_->parameter("platform_hold_ms");
    if(!policy || !reactor.contact(platform_index(receipt.object.source),receipt.observedAt,
        static_cast<std::uint32_t>(policy->value))) return false;
    ++frame_.revision;return true;
}
bool Controller::path_goal(std::uint8_t path) noexcept {
    auto& reactor=frame_.reactor;
    // The authenticated goal position follows the required ordered platform
    // receipts. It must not acquire a second dependency on unavailable health.
    if(reactor.player==UINT32_MAX
        || current_player_dead(reactor)
        || !reactor.goal(path)) return false;
    frame_.checkpointSliceSet=56;frame_.checkpointSpawnSet=kReactorGoalSpawns[path-1];
    // Solo presentation policy: light each completed intermediate checkpoint.
    // Device names/order identify the lights; no original host timing is claimed.
    if(path<4) {
        const auto* light=find(0x686321C8U,23,static_cast<std::uint16_t>(100+path));
        if(light) static_cast<void>(request(light->asset,true));
    }
    frame_.waitingMechanic=0;++frame_.revision;return true;
}
bool Controller::health(const HealthReceipt& receipt,bool dead) noexcept {
    const auto i=asset_index(receipt.source),o=object_index(receipt.source);
    if(!frame_.enabled || frame_.finished || receipt.generation.run!=run_
        || !receipt.sourcePointer || receipt.health==UINT32_MAX
        || i==std::size(kAssets) || o==kObjectBindings.size()
        || health_index(receipt.source)==std::size(kHealthBindings)
        || !current_group(receipt.source.registry)) return false;
    auto& state=frame_.native[i];const auto created=objects_.owner(o);
    if(!state.active || !state.desired || !state.acknowledged || !created.valid()
        || created.owner!=receipt.generation || created.entity!=receipt.entity
        || created.serial!=receipt.serial) return false;
    auto& prior=healthOwners_[i];
    if(prior.sourcePointer && (prior.sourcePointer!=receipt.sourcePointer
        || prior.health!=receipt.health || prior.generation!=receipt.generation
        || prior.entity!=receipt.entity || prior.serial!=receipt.serial)) return false;
    prior=receipt;
    // The native adapter proves the health ABI. The controller additionally needs
    // an observed live instance before its dead bit can count as destruction.
    if(state.destroyed || (dead && !state.healthKnown) || (!dead && state.healthKnown)) return false;
    state.healthKnown=true;state.destroyed=dead;
    if(dead) {state.carried=state.dropped=false;state.carrier=UINT32_MAX;}
    ++frame_.revision;return true;
}
bool Controller::cranium(const CraniumReceipt& receipt,bool held,std::uint32_t player) noexcept {
    const auto i=asset_index(receipt.source),o=object_index(receipt.source);
    if(!frame_.enabled || frame_.finished || receipt.generation.run!=run_
        || !receipt.sourcePointer || receipt.component==UINT32_MAX || player==UINT32_MAX
        || i==std::size(kAssets) || o==kObjectBindings.size()
        || cranium_index(receipt.source)==std::size(kCraniumBindings)
        || !current_group(receipt.source.registry)) return false;
    auto& state=frame_.native[i];const auto created=objects_.owner(o);
    if(!state.active || !state.desired || !state.acknowledged || state.destroyed || !created.valid()
        || created.owner!=receipt.generation || created.entity!=receipt.entity
        || created.serial!=receipt.serial) return false;
    auto& prior=carryOwners_[i];
    if(prior.sourcePointer && (prior.sourcePointer!=receipt.sourcePointer
        || prior.component!=receipt.component || prior.generation!=receipt.generation
        || prior.entity!=receipt.entity || prior.serial!=receipt.serial)) return false;
    // Only a native, authenticated pickup establishes a holder. A source which
    // starts on the ground has not been dropped by this attempt's player.
    if(held) {
        if(state.carried) return false;
        prior=receipt;state.carrier=player;state.carried=true;state.dropped=false;
    }else {
        if(!state.carried || state.carrier!=player) return false;
        state.carried=false;state.dropped=true;state.carrier=UINT32_MAX;
    }
    ++frame_.revision;return true;
}
bool Controller::device(coo::Generation gen,coo::Asset a,std::int16_t revision,float value) noexcept {
    const auto i=asset_index(a);
    if(!frame_.enabled || frame_.finished || i==std::size(kAssets) || a.type!=23
        || gen.run!=run_ || !current_group(a.registry) || !std::isfinite(value)) return false;
    auto& s=frame_.native[i];
    if(!s.managed || !s.desired || !s.prepared || !s.active || s.acknowledged
        || gen.value!=s.generation || revision!=static_cast<std::int16_t>(s.generation)
        || value!=s.position) return false;
    s.acknowledged=true;++frame_.revision;return true;
}
bool Controller::station(const StationReceipt& receipt,std::uint32_t player) noexcept {
    const auto i=asset_index(receipt.source),o=object_index(receipt.source);
    if(!frame_.enabled || frame_.finished || receipt.generation.run!=run_
        || !receipt.sourcePointer || receipt.component==UINT32_MAX || player==UINT32_MAX
        || i==std::size(kAssets) || o==kObjectBindings.size()
        || station_index(receipt.source)==std::size(kStationBindings)
        || !current_group(receipt.source.registry) || receipt.consumedBefore<0
        || receipt.requested<=receipt.consumedBefore || receipt.consumedAfter!=receipt.requested) return false;
    auto& state=frame_.native[i];const auto created=objects_.owner(o);
    if(!state.active || !state.desired || !state.acknowledged || !created.valid()
        || created.owner!=receipt.generation || created.entity!=receipt.entity
        || created.serial!=receipt.serial) return false;
    auto& prior=stationOwners_[i];
    if(prior.sourcePointer && (prior.sourcePointer!=receipt.sourcePointer
        || prior.component!=receipt.component || prior.generation!=receipt.generation
        || prior.entity!=receipt.entity || prior.serial!=receipt.serial
        || receipt.requested<=prior.requested)) return false;
    std::size_t cranium=std::size(kAssets);
    for(const auto& binding:kCraniumBindings) {
        if(binding.source.registry!=receipt.source.registry) continue;
        const auto index=asset_index(binding.source);const auto& carried=frame_.native[index];
        if(!carried.active || !carried.desired || !carried.acknowledged || carried.destroyed
            || !carried.carried || carried.carrier!=player) continue;
        if(cranium!=std::size(kAssets)) return false;
        cranium=index;
    }
    if(cranium==std::size(kAssets)) return false;
    const auto& carriedOwner=carryOwners_[cranium];const auto& heldBefore=receipt.cranium;
    if(heldBefore.generation!=carriedOwner.generation || heldBefore.source!=carriedOwner.source
        || heldBefore.sourcePointer!=carriedOwner.sourcePointer || heldBefore.component!=carriedOwner.component
        || heldBefore.entity!=carriedOwner.entity || heldBefore.serial!=carriedOwner.serial) return false;
    // This edge proves a station interaction while holding this exact cranium.
    // Native Carry remains the sole owner of drop state; no charge or consumption
    // is inferred from the station's consumed request counter.
    prior=receipt;state.used=true;state.usedCranium=static_cast<std::uint16_t>(cranium);
    ++frame_.revision;return true;
}
bool Controller::admitted(const EnemyReceipt& r) noexcept {
    if(!frame_.enabled || frame_.finished) return false;
    const auto index=spawn_index(r.registry,r.source);if(index==std::size(kSpawns)) return false;
    const auto& source=frame_.native[asset_index(kSpawns[index].asset)];
    if(!source.active || source.retiring || !current_group(r.registry)) return false;
    const auto result=population_.admit(kCohorts,r,run_,source.generation);
    if(result==coo::Admission::overflow) frame_.populationFault=true;
    return result==coo::Admission::accepted;
}
bool Controller::died(const EnemyReceipt& r) noexcept {
    const auto index=spawn_index(r.registry,r.source);
    if(!frame_.enabled || frame_.finished || index==std::size(kSpawns) || !current_group(r.registry)) return false;
    const auto& source=frame_.native[asset_index(kSpawns[index].asset)];
    if(!source.active || source.retiring || !population_.died(r,run_,source.generation)) return false;
    if(r.registry==kBossSource.registry && r.source==kBossSource.slot) frame_.bossDead=true;
    ++frame_.revision;return true;
}
coo::ReadinessRequest<EnemyReceipt> Controller::readiness_request(std::uint64_t now) noexcept {
    return readinessSchedule_.request<EnemyReceipt,std::size(kSpawns)*16>(frame_.enabled?run_:0,now,[this](auto visit){population_.pending(visit);});
}
void Controller::costed(std::uint32_t key,std::uint16_t slot,const coo::TaskCosts& report) noexcept {
    const auto i=spawn_index(key,slot);if(!frame_.enabled || i==std::size(kSpawns) || !population_.enabled(i) || !current_group(key)) return;
    const auto generation=frame_.native[asset_index(kSpawns[i].asset)].generation;
    if(report.hasRevision && report.revision!=generation) return;
    costs_[i].merge(report);const auto& s=kSpawns[i];
    const auto selected=costs_[i].select(static_cast<std::int8_t>(int(frame_.taskPlusOne[i])-1),frame_.native[asset_index(s.asset)].generation,[&](auto row){return row<s.taskCount;});
    if(selected<0 || frame_.taskPlusOne[i]==selected+1) return;
    frame_.taskPlusOne[i]=static_cast<std::uint8_t>(selected+1);
    population_.policy(i,{true,s.sceneOwned?coo::EnemyIntent::idleReveal:coo::EnemyIntent::combat,key,s.objective,selected});++frame_.revision;
}
void Controller::combatant(std::uint64_t run,std::uint32_t key,std::uint16_t slot,const middleware::bap::activity_message::combatant_sense::Output& report) noexcept {
    // Detachment is not a death. The native actor ledger is the only death producer.
    if(run!=run_ || !frame_.enabled || !report.hasSpawnRevision || report.spawnRevision!=frame_.spawnGeneration) return;
    const auto* a=find(key,2,slot);if(a) frame_.native[asset_index(a->asset)].acknowledged=report.snapshotValid;
}
void Controller::scene(std::uint64_t run,std::uint32_t key,std::uint16_t slot,const middleware::bap::activity_message::scene_sense::Output& report) noexcept {
    const auto* a=find(key,43,slot);if(run!=run_ || !frame_.enabled || !a || !report.delta) return;
    auto& s=frame_.native[asset_index(a->asset)];
    if(!s.managed || report.generationWire!=0x80000000U+s.generation) return;
    if(report.completed && !s.acknowledged) {s.acknowledged=true;++frame_.revision;}
}
void Controller::squad(std::uint64_t run,std::uint32_t key,std::uint16_t slot,const middleware::bap::activity_message::squad_sense::Output& report) noexcept {
    // Cost updates are routed separately. A source reporting zero alive before
    // creation cannot clear its required population.
    static_cast<void>(run);static_cast<void>(key);static_cast<void>(slot);static_cast<void>(report);
}
bool Controller::submitted(std::uint64_t run,std::uint32_t tag,std::int64_t offset,std::uint32_t bank,std::uint8_t row,std::uint32_t generation) noexcept {
    if(!views_ || run!=run_ || tag!=0x8155C312U || offset!=0x1408 || !frame_.enabled
        || !dialogue_.submitted(views_->dialogue,bank,row,generation,now_,frame_,frame_.revision)) return false;
    submitted_.set(row);voiceEnd_[row]=dialogue_.voice_until();return true;
}
bool Controller::publish(const coo::Command& command) noexcept {
    if(!views_ || !graph() || !coo::script::valid_token(graph()->definition,executor_,command)) return false;
    const auto& s=command.spec;
    switch(s.operation) {
    case coo::Operation::population:case coo::Operation::device:return request(s.asset,s.argument!=0);
    case coo::Operation::objective:dialogue_.objective(views_->dialogue,s.argument,frame_,frame_.revision);objectives_.set(s.argument);break;
    case coo::Operation::dialogue:dialogue_.enqueue(views_->dialogue,static_cast<std::uint8_t>(s.argument),now_,0,frame_.section,frame_.revision);break;
    case coo::Operation::mechanic:
        if(s.asset!=kModule) return false;
        if(s.argument==2 || s.argument==3) {frame_.restricted=s.argument==3;break;}
        if(s.argument>=101 && s.argument<=104) {
            if(!frame_.reactor.begin(static_cast<std::uint8_t>(s.argument-100),frame_.spawnGeneration)) return false;
            if(s.argument==101) crossing_population(true);
            if(s.argument==101 && !frame_.checkpointSpawnSet) {
                frame_.checkpointSliceSet=56;frame_.checkpointSpawnSet=kReactorApproachSpawn;
            }
            frame_.waitingMechanic=s.argument;break;
        }
        if(s.argument==110 || s.argument==111) {frame_.waitingMechanic=s.argument;break;}
        if(s.argument==113) return open_exit_grate();
        if(s.argument==115) {frame_.arrival.launched=true;return stage_argos_intro();}
        return false;
    case coo::Operation::complete:
        if(!frame_.bossDead || !submitted_[0] || now_<voiceEnd_[0] || !lifecycle_.complete_timed(owner(),now_,30000)) return false;
        frame_.finished=frame_.checked=true;frame_.restricted=false;frame_.endEpoch=frame_.gameplayClockTicks;objectives_.clear();break;
    case coo::Operation::observation:break;
    default:return false;
    }
    ++frame_.revision;return true;
}
bool Controller::observed(const coo::CommandSpec& s) const noexcept {
    if(views_ && views_->condition(s)) return views_->evaluate(s,[this](const auto& child){return observed(child);});
    if(s.asset==kRegion) return s.argument<64 && s.argument%8==0 && regions_[s.argument/8];
    if(s.asset==kModule) {
        if(s.argument>=101 && s.argument<=104) return frame_.reactor.completed[s.argument-101];
        if(s.argument==114) return frame_.grate.open && frame_.grate.poseAcknowledged;
        if(s.argument==116) return frame_.arrival.landed;
        return s.argument==111 && frame_.bossDead;
    }
    if(s.asset.registry==kRoot && s.asset.type==53) return s.argument<std::size(kDialogueRows) && submitted_[s.argument] && now_>=voiceEnd_[s.argument];
    if(s.asset.type==60) return entered(s.asset);
    if(s.asset.type==1) {
        const auto i=spawn_index(s.asset.registry,s.asset.slot);if(i==std::size(kSpawns)) return false;
        const auto& spawn=kSpawns[i];const auto count=expected_enemy_count(spawn);const bool cleared=population_.cleared(i,count);
        // Row -1 requests cost evaluation; it is not an assigned combat task.
        // Actual deaths may arrive first and remain valid without an AI wait.
        const bool task=spawn.sceneOwned || spawn.objective==65535 || frame_.taskPlusOne[i]!=0;
        return s.argument==1?population_.ready(i,count) && (task || cleared):cleared;
    }
    const auto i=asset_index(s.asset);if(i==std::size(kAssets)) return false;
    if(s.asset.type==30 || s.asset.type==31) {
        for(const auto& door:kTraversalDoors) if(s.asset==door.monitor)
            return triggered_[i] || entered(door.volume);
        for(const auto& crossing:kMouthCrossings) if(s.asset==crossing.task)
            return triggered_[i] || entered(crossing.volume);
        if(s.asset==kAirlock.interiorMonitor) return triggered_[i] || entered(kAirlock.interiorVolume);
        if(s.asset==kAirlock.exteriorMonitor) return triggered_[i] || entered(kAirlock.exteriorVolume);
        if(s.asset==kBellyTraversal.task || s.asset==kBellyTraversalMonitor)
            return triggered_[i] || entered(kBellyTraversal.volume);
        if(s.asset==kThunderingWallCheckpoint.task)
            return triggered_[i] || entered(kThunderingWallCheckpoint.volume);
        return triggered_[i];
    }
    if(s.asset.type==4) {
        if(s.argument==2) return frame_.native[i].destroyed;
        if(s.argument==3) return frame_.native[i].carried;
        if(s.argument==4) return frame_.native[i].dropped;
        if(s.argument==5) return frame_.native[i].used;
    }
    return frame_.native[i].acknowledged;
}
void Controller::update_module(std::uint32_t id,const coo::MissionInput& input,Frame& output) noexcept {
    if(id!=1 || !views_ || input.run!=run_) return;
    now_=input.now;frame_.region=input.region;
    if(input.region>=0 && input.region<64 && input.region%8==0) regions_.set(input.region/8);
    frame_.gameplayClockTicks=clock_.sample(now_);frame_.enabled=!frame_.populationFault;
    if(!frame_.enabled) {output=frame_;return;}
    pose_argos_shell();
    if(frame_.finished) {
        if(lifecycle_.advance(now_)) ++frame_.revision;
        frame_.completion=lifecycle_.publication();output=frame_;return;
    }
    if(frame_.combatRetry) {
        frame_.restricted=false;
        if(current_player_dead(frame_.reactor) || retiring_holdout()) {output=frame_;return;}
        frame_.combatRetry=false;++frame_.revision;
    }
    if(!started_ && graph()) started_=executor_.start(graph()->definition,run_);
    if(!started_) {frame_.enabled=false;output=frame_;return;}
    dialogue_.advance(views_->dialogue,frame_.spawnGeneration-1U,now_,false,frame_,frame_.revision);
    executor_.update(*this);
    for(const auto& b:graph()->commands) {
        const auto state=executor_.step_state(b.step);
        if(state.phase!=coo::StepPhase::active || !state.commands[b.command].requested) continue;
        const auto& s=graph()->definition.steps[b.step].commands[b.command];
        if(coo::is_observation(s.operation) && observed(s)) static_cast<void>(executor_.enqueue({executor_.token(b.step,b.command),coo::Milestone::observed}));
    }
    executor_.update(*this);
    if(executor_.diagnostics().phase==coo::Phase::complete && static_cast<std::size_t>(frame_.section)+1<views_->phases.size()
        && !(frame_.section==1 && current_player_dead(frame_.reactor))) {
        if(frame_.section==2) {
            crossing_population(false);
            for(const auto& spawn:kSpawns) if(holdout_source(spawn.asset))
                if(!request(spawn.asset,false)) frame_.populationFault=true;
            // An earlier visit cannot complete the new traversal phase.
            for(std::size_t v=0;v<std::size(kVolumes);++v) {
                const auto key=kVolumes[v].asset.registry;
                if(key!=kReactorRegistry && key!=0x5654D7FDU && key!=0xA9E6185FU) seen_.reset(v);
            }
            for(std::size_t a=0;a<std::size(kAssets);++a) {
                const auto key=kAssets[a].asset.registry;
                if(key!=kReactorRegistry && key!=0x5654D7FDU && key!=0xA9E6185FU) triggered_.reset(a);
            }
        }
        if(frame_.section==3) frame_.routeComplete=true;
        ++frame_.section;executor_.cancel(*this);started_=executor_.start(graph()->definition,run_);
        if(started_) executor_.update(*this);++frame_.revision;
    }
    dialogue_.advance(views_->dialogue,frame_.spawnGeneration-1U,now_,false,frame_,frame_.revision);
    frame_.enabled=!frame_.populationFault && executor_.diagnostics().phase!=coo::Phase::failed;
    if(frame_.section==1 && current_player_dead(frame_.reactor)) frame_.restricted=false;
    frame_.presentation=objectives_.state();frame_.completion=lifecycle_.publication();output=frame_;
}
Frame Controller::update(std::uint64_t run,std::uint64_t now,bool ready,int region) noexcept {
    if(!views_ || run!=run_ || !ready) return {};
    return composition_.update(views_->mission,{run,now,region,false,true},*this);
}
}
