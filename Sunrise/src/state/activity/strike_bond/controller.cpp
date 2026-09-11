#include "controller.h"
#include <algorithm>
#include <chrono>
#include <cmath>

namespace sunrise::state::activity::strike_bond {
bool valid_document(const coo::script::Views& v) noexcept {
    return v.valid && v.missionId=="strike_bond" && v.profileId==kProfile.id && !v.phases.empty()
        && v.mission.modules.size()==1 && v.mission.modules[0].asset==kModule && v.mission.modules[0].id==1
        && v.role("ending") && coo::script::authorized(v,kProfile);
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
    executor_.cancel(*this);composition_.reset();lifecycle_.reset();views_=nullptr;run_=now_=0;started_=landed_=false;
    clock_.reset();objects_={};population_={};costs_={};lenses_={};scenes_={};boss_={};bossEnemy_={};bossFraction_=1.F;hasBossHealth_=false;
    lastPoint_={};hasPoint_=false;nextCover_=0;coverSeed_=0;coverGroup_=UINT8_MAX;dialogue_={};objectives_={};submitted_.reset();voiceEnd_={};seen_.reset();regions_.reset();frame_={};
}
bool Controller::select(const coo::script::Views& v,std::uint64_t run) noexcept {
    if(!run || !valid_document(v)) {reset();return false;}
    if(run==run_) return views_==&v;
    reset();if(!lifecycle_.begin(run,128) || !objects_.begin(owner(),kObjectBindings)) return false;
    views_=&v;run_=run;frame_.spawnGeneration=owner().value;return true;
}
void Controller::position(std::uint64_t run,Point p) noexcept {
    if(!views_ || run!=run_ || !std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) return;
    if(frame_.region<0) return;
    // Arrival is already qualified by the composition. The first real player sample
    // permits presentation; a pending destination or origin camera cannot do so.
    lastPoint_=p;hasPoint_=true;
    if(!views_->observationStart) landed_=true;
    for(std::size_t i=0;i<std::size(kVolumes);++i) {
        const auto& v=kVolumes[i];if(v.bubble*8!=frame_.region || !contains(v,p)) continue;
        if(views_->observationStart && v.asset==views_->observationStart->asset) landed_=true;
        if(landed_) seen_.set(i);
    }
}
bool Controller::entered(coo::Asset a) const noexcept {
    for(std::size_t i=0;i<std::size(kVolumes);++i) if(kVolumes[i].asset==a) return seen_[i];
    return false;
}
bool Controller::player_trigger(std::uint64_t run,std::uint32_t key,std::uint16_t slot) noexcept {
    if(run!=run_ || !frame_.enabled) return false;
    for(const auto& trigger:kTriggers) {
        if(trigger.asset.registry!=key || trigger.asset.slot!=slot) continue;
        for(std::size_t i=0;i<std::size(kVolumes);++i) {
            if(kVolumes[i].asset!=trigger.volume || kVolumes[i].bubble*8!=frame_.region || seen_[i]) continue;
            seen_.set(i);++frame_.revision;return true;
        }
    }return false;
}
bool Controller::revise(coo::Asset a) noexcept {
    const auto i=asset_index(a);if(i==std::size(kAssets)) return false;
    auto& s=frame_.native[i];
    if(s.generation>=32766 || !lifecycle_.reserve_through(owner(),s.generation+1)) return false;
    ++s.generation;s.acknowledged=false;++frame_.revision;return true;
}
bool Controller::request(coo::Asset a,bool active) noexcept {
    const auto i=asset_index(a);if(i==std::size(kAssets)) return false;
    auto& s=frame_.native[i];if(s.managed && s.desired==active) return true;
    if(!s.managed) {s.managed=true;s.generation=frame_.spawnGeneration;s.prepared=a.type!=4;}
    else if(a.type!=1 && a.type!=4 && a.type!=43 && !revise(a)) return false;
    s.desired=active;s.acknowledged=false;s.position=active?1.F:0.F;
    if(a.type==4) {
        const auto o=object_index(a);if(o==kObjectBindings.size()) return false;
        if(!active) objects_.retire(o);
        const auto object=objects_.state(o);if(active && object.phase==coo::ObjectPhase::retired) return false;
        s.generation=object.generation;s.prepared=object.phase>=coo::ObjectPhase::create;s.active=active && object.create;
    }else s.active=active;
    if(a.type==1 && active) {
        const auto n=spawn_index(a);if(n==std::size(kSpawns)) return false;
        const auto& row=kSpawns[n];population_.enable(n);
        // Authored rooftop center area 354 is task 10. Dendron must not wait for
        // a cost report that its stationary Cyclops source never sends.
        if(a.registry==kBossActor.registry && a.slot==3) frame_.taskPlusOne[n]=11;
        population_.policy(n,{true,row.sceneOwned?coo::EnemyIntent::idleReveal:coo::EnemyIntent::combat,
            a.registry,row.objective,static_cast<std::int8_t>(static_cast<int>(frame_.taskPlusOne[n])-1)});
    }
    ++frame_.revision;return true;
}
bool Controller::prepared(coo::Generation gen,coo::Asset a) noexcept {
    const auto i=asset_index(a),o=object_index(a);
    if(!frame_.enabled || gen!=owner() || i==std::size(kAssets) || o==kObjectBindings.size()) return false;
    auto& s=frame_.native[i];if(!s.managed || !s.desired || s.prepared || !objects_.prepared(gen,o)) return false;
    const auto object=objects_.state(o);s.generation=object.generation;s.prepared=true;s.active=object.create;++frame_.revision;return true;
}
bool Controller::object(const coo::ObjectReceipt& r) noexcept {
    const auto i=asset_index(r.source),o=object_index(r.source);
    if(!frame_.enabled || i==std::size(kAssets) || o==kObjectBindings.size() || !frame_.native[i].active) return false;
    if(!objects_.observe(o,r,true,0.F,1)) return false;
    frame_.native[i].acknowledged=true;++frame_.revision;return true;
}
bool Controller::device(coo::Generation gen,coo::Asset a,std::int16_t revision,float value) noexcept {
    const auto i=asset_index(a);if(!frame_.enabled || i==std::size(kAssets) || a.type!=23 || gen.run!=run_ || !std::isfinite(value)) return false;
    auto& s=frame_.native[i];if(!s.managed || s.acknowledged || gen.value!=s.generation || revision!=static_cast<std::int16_t>(s.generation) || value!=device_position(frame_,a)) return false;
    s.acknowledged=true;++frame_.revision;return true;
}
bool Controller::admitted(const EnemyReceipt& r) noexcept {
    if(!frame_.enabled || frame_.finished) return false;
    const auto result=population_.admit(kCohorts,r,run_,frame_.spawnGeneration);
    if(result==coo::Admission::overflow) frame_.populationFault=true;
    if(result==coo::Admission::accepted && r.registry==kBossActor.registry && r.source==3) bossEnemy_=r;
    return result==coo::Admission::accepted;
}
bool Controller::died(const EnemyReceipt& r) noexcept {
    if(!frame_.enabled || frame_.finished || !population_.died(r,run_,frame_.spawnGeneration)) return false;
    if(r.registry==kBossActor.registry && r.source==3 && frame_.bossFighting) frame_.bossDead=true;
    ++frame_.revision;return true;
}
bool Controller::health(const EnemyReceipt& r,float fraction) noexcept {
    if(!frame_.enabled || frame_.finished || !r.valid() || r!=bossEnemy_
        || !std::isfinite(fraction) || fraction<0.F || fraction>1.F) return false;
    // A fraction is progress evidence only. Even zero cannot synthesize death.
    bossFraction_=fraction;hasBossHealth_=true;return true;
}
bool Controller::costed(std::uint32_t key,std::uint16_t slot,const coo::TaskCosts& report,std::int8_t& selected,std::uint32_t& known) noexcept {
    selected=-1;known=0;if(!frame_.enabled) return false;
    const auto* a=find(key,1,slot);if(!a) return false;
    const auto i=spawn_index(a->asset);if(i==std::size(kSpawns) || !population_.enabled(i)) return false;
    auto& costs=costs_[i];costs.merge(report);known=costs.mask;
    if(key==kBossActor.registry && slot==3) {selected=10;return false;}
    const auto current=static_cast<std::int8_t>(static_cast<int>(frame_.taskPlusOne[i])-1);
    selected=costs.select(current,1,[&](auto row) {return row<kSpawns[i].taskCount;});
    if(selected==current) return false;
    frame_.taskPlusOne[i]=static_cast<std::uint8_t>(selected+1);
    population_.policy(i,{true,kSpawns[i].sceneOwned?coo::EnemyIntent::idleReveal:coo::EnemyIntent::combat,
        key,kSpawns[i].objective,selected});
    ++frame_.revision;return true;
}
LensRequest Controller::lens_request(std::size_t i) const noexcept {
    if(i>=std::size(kLenses)) return {};
    const auto& state=frame_.native[asset_index(kLenses[i].source)];
    return {owner(),lenses_[i].owner(),state.generation,i,frame_.enabled && state.active,!lenses_[i].immune(),lenses_[i].dead()};
}
bool Controller::lens(const LensReceipt& r,bool dead) noexcept {
    const auto i=lens_index(r.asset);if(i==std::size(kLenses) || r.asset!=kLenses[i].source || !r.valid()) return false;
    const auto q=lens_request(i);if(!q.enabled || r.owner.run!=run_ || r.owner.value!=q.generation || q.destroyed) return false;
    if(!lenses_[i].owner().valid()) {if(dead) return false;return lenses_[i].bind(r);}
    if(!dead || !lenses_[i].destroyed(r) || !revise(kLenses[i].device)) return false;
    frame_.lensDestroyed.set(i);
    for(const auto& tether:kRouteTethers) if(tether.lens==i) {
        static_cast<void>(request(tether.source,false));
        static_cast<void>(request(find(tether.source.registry,23,tether.center)->asset,false));
    }
    ++frame_.revision;return true;
}
bool Controller::submitted(std::uint64_t run,std::uint32_t bank,std::uint8_t row,std::uint32_t generation,std::uint64_t now) noexcept {
    if(!views_ || run!=run_ || !frame_.enabled || !dialogue_.submitted(views_->dialogue,bank,row,generation,now,frame_,frame_.revision)) return false;
    submitted_.set(row);voiceEnd_[row]=dialogue_.voice_until();return true;
}
void Controller::generator(std::uint64_t run,std::uint32_t key,std::uint16_t slot,std::uint32_t seed,std::uint32_t completed) noexcept {
    if(run==run_ && key==kGenerator.registry && slot==kGenerator.slot && frame_.generatorSeed && seed==frame_.generatorSeed && completed && !frame_.forestGenerated) {
        frame_.forestGenerated=true;++frame_.revision;
    }
}
void Controller::scene(std::uint64_t run,std::uint32_t key,std::uint16_t slot,const middleware::bap::activity_message::scene_sense::Output& r) noexcept {
    if(run!=run_ || !frame_.enabled || !r.delta) return;
    const auto* a=find(key,43,slot);if(!a) return;
    const auto i=scene_index(a->asset);
    if(i==std::size(kScenes) || !scenes_.requested(i) || r.generationWire!=0x80000000U+scenes_.commands()[i].generation) return;
    scenes_.mark(i,r.completed?3:1);++frame_.revision;
}
void Controller::combatant(std::uint64_t run,std::uint32_t key,std::uint16_t slot,const middleware::bap::activity_message::combatant_sense::Output& r) noexcept {
    if(run!=run_ || key!=kBossActor.registry || slot!=kBossActor.slot || !frame_.enabled) return;
    if(!r.snapshotValid || r.detached) {boss_={};return;}
    boss_.merge(r);
    // The reflected seven-bit actor query is not a proven health fraction.
    // Health comes from the admitted actor's native health interface instead.
}
void Controller::squad(std::uint64_t,std::uint32_t,std::uint16_t,
    const middleware::bap::activity_message::squad_sense::Output&) noexcept {
    // Removal and zero live count also occur during streaming. Only the
    // authenticated C72390 health-death receipt can complete this strike.
}
bool Controller::cover(bool active) noexcept {
    // Four authored eight-block layouts. Object sources are created once; the
    // native device animates their rise/retraction without transform overrides.
    for(std::uint16_t i=0;i<32;++i) {
        const auto slot=static_cast<std::uint16_t>(76+3*i);
        const auto* device=find(kBossActor.registry,23,slot);
        const auto* avoid=find(kBossActor.registry,4,static_cast<std::uint16_t>(slot+1));
        const auto* block=find(kBossActor.registry,4,static_cast<std::uint16_t>(slot+2));
        if(!device || !avoid || !block || (active && (!request(avoid->asset,true) || !request(block->asset,true)))
            || !request(device->asset,false)) return false;
    }
    frame_.coverEnabled=active;nextCover_=now_;coverGroup_=UINT8_MAX;
    if(active && !coverSeed_) coverSeed_=frame_.generatorSeed?frame_.generatorSeed:static_cast<std::uint32_t>(run_);
    return true;
}
void Controller::update_cover() noexcept {
    if(!frame_.coverEnabled || frame_.bossDead || now_<nextCover_) return;
    // Do not raise collision before all of the native block objects exist.
    for(std::uint16_t i=0;i<32;++i) {
        const auto* block=find(kBossActor.registry,4,static_cast<std::uint16_t>(78+3*i));
        if(!block || !frame_.native[asset_index(block->asset)].acknowledged) return;
    }
    coverSeed_=coverSeed_*1664525U+1013904223U;
    const auto next=static_cast<std::uint8_t>(coverGroup_==UINT8_MAX?(coverSeed_>>16)%4:
        (coverGroup_+1+(coverSeed_>>16)%3)%4);
    for(std::uint16_t i=0;i<32;++i) {
        const auto* device=find(kBossActor.registry,23,static_cast<std::uint16_t>(76+3*i));
        if(!device || !request(device->asset,i/8==next)) {frame_.populationFault=true;return;}
    }
    coverGroup_=next;
    // Reconstruction cadence; source data provides placements and native
    // animations, but not the retail host's random-cover scheduling.
    nextCover_=now_+12000;
}
bool Controller::publish(const coo::Command& command) noexcept {
    if(!views_ || !graph() || !coo::script::valid_token(graph()->definition,executor_,command)) return false;
    const auto& s=command.spec;
    switch(s.operation) {
    case coo::Operation::population:case coo::Operation::device:return request(s.asset,s.argument!=0);
    // ObjectiveService::set OVERWRITES state_.marker with its second argument. Passing nothing, as
    // this did, wiped the waypoint on every directive change - so a marker published by the
    // standalone .marker mechanic survived only until the next objective, and the strike ran with
    // no waypoint for most of its length. gateway, strike_pact, deep_storage and deadly_trial all
    // carry the marker on the objective; look ours up the same way, from the script's marker table.
    case coo::Operation::objective: {
        dialogue_.objective(views_->dialogue,s.argument,frame_,frame_.revision);
        coo::MarkerTarget marker{};
        for(const auto& binding:views_->markers) { if(binding.event==s.argument) { marker=binding.target; } }
        objectives_.set(s.argument,marker);
    } break;
    case coo::Operation::dialogue:dialogue_.enqueue(views_->dialogue,static_cast<std::uint8_t>(s.argument),now_,0,frame_.section,frame_.revision);break;
    case coo::Operation::scene: {
        const auto i=scene_index(s.asset);if(i==std::size(kScenes)) return false;
        if(scenes_.requested(i)) return true;
        // Request live participants; static performance points stay in the authored
        // selector parameters and never enter the runtime ownership list.
        for(const auto a:kScenes[i].cast) if((a.type==1 || a.type==4) && !request(a,true)) return false;
        if(!request(s.asset,true) || !scenes_.begin(i,frame_.native[asset_index(s.asset)].generation,SceneOwner{command.token})) return false;
        break;
    }
    case coo::Operation::mechanic:
        if(s.asset==kModule && (s.argument==70 || s.argument==71)) return cover(s.argument==70);
        // 80F45CAA holds the reserved Minotaur in its dormant animation until
        // input 33E63A8B. Device/shield changes alone do not release native AI.
        if(s.asset.type==43 && s.argument==0x33E63A8BU) {
            const auto i=scene_index(s.asset);
            if(i==std::size(kScenes) || kScenes[i].graph!=0x80F45CAAU
                || kScenes[i].cast.size()!=2 || !scenes_.seen(i,1)) return false;
            const auto lens=lens_index(kScenes[i].cast[1]);
            if(lens==std::size(kLenses) || !lenses_[lens].dead()
                || scenes_.event(i,s.argument)!=coo::SceneEvent::accepted) return false;
            break;
        }
        if(s.asset.type==43 && (s.argument==0x01994745U || s.argument==0x858A9281U)) {
            const auto i=scene_index(s.asset);
            if(i==std::size(kScenes)) return false;
            if(s.argument==0x858A9281U) {
                const unsigned phase=s.asset.slot==73?0U:s.asset.slot==75?1U:2U;
                if(phase>1 || frame_.bossStage!=phase || !frame_.bossFighting) return false;
                for(unsigned n=0;n<2;++n) {
                    const auto* source=find(kBossActor.registry,1,static_cast<std::uint16_t>(174+16*phase+8*n));
                    const auto cohort=spawn_index(source->asset);
                    if(!population_.cleared(cohort,kSpawns[cohort].count)) return false;
                }
                if(scenes_.event(i,s.argument)!=coo::SceneEvent::accepted) return false;
                ++frame_.bossStage;
            } else if(scenes_.event(i,s.argument)!=coo::SceneEvent::accepted) return false;
            break;
        }
        if(s.asset==kModule && (s.argument==10 || s.argument==11)) {frame_.restricted=s.argument==11;break;}
        if(s.asset==kModule && (s.argument==60 || s.argument==61)) {
            const int region=s.argument==60?8:136;if(frame_.region!=region) return false;
            frame_.checkpointSliceSet=region;frame_.checkpointSpawnSet=s.argument==60?0x6F7119B7U:0x2EA8FB98U;break;
        }
        if(s.asset==kGenerator && s.argument==20) {
            if(!frame_.generatorSeed) {
                const auto entropy=std::chrono::high_resolution_clock::now().time_since_epoch().count();
                frame_.generatorSeed=coo::native_generator::mission_seed(static_cast<std::uint32_t>(entropy)^static_cast<std::uint32_t>(run_));
            }break;
        }
        if(s.asset==kBossActor && s.argument==31) {
            if(!lenses_[7].dead() || !bossEnemy_.valid()) return false;
            frame_.bossFighting=true;break;
        }
        if(s.asset==kObjectiveAsset && s.argument==40) {objectives_.clear_marker();break;}
        // Type 4 as well as 47: the diamond is aimed at the shootable object itself, because the
        // nav-point-to-module mapping was never recoverable. strike_pact aims one at a type-2 actor,
        // so a non-nav-point target is an established shape rather than a new one.
        if((s.asset.type==47 || s.asset.type==4) && s.argument==41) {objectives_.marker({s.asset,kAbsentLocator});break;}
        if(s.argument==50) {
            const auto i=lens_index(s.asset);if(i==std::size(kLenses) || s.asset!=kLenses[i].source || lenses_[i].dead()) return false;
            if(!request(kLenses[i].source,true) || !request(kLenses[i].device,true)) return false;
            if(!frame_.lensExposed[i]) {if(!revise(kLenses[i].device)) return false;lenses_[i].expose();frame_.lensExposed.set(i);}break;
        }return false;
    case coo::Operation::complete:
        if(!frame_.bossDead || !lifecycle_.complete_timed(owner(),now_)) return false;
        frame_.finished=true;frame_.restricted=false;frame_.endEpoch=frame_.gameplayClockTicks;objectives_.clear();break;
    case coo::Operation::observation:break;
    default:return false;
    }++frame_.revision;return true;
}
bool Controller::observed(const coo::CommandSpec& s) const noexcept {
    if(views_ && views_->condition(s)) return views_->evaluate(s,[this](const auto& child){return observed(child);});
    if(s.asset==kRegion) return s.argument/8<regions_.size() && regions_[s.argument/8];
    if(s.asset==kGenerator) return frame_.forestGenerated;
    if(s.asset==kBossActor && s.argument==30) return frame_.bossDead;
    if(s.asset==kBossActor && (s.argument==32 || s.argument==33))
        return frame_.bossFighting && hasBossHealth_ && bossEnemy_.valid()
            && bossFraction_<=(s.argument==32?2.F/3.F:1.F/3.F);
    if(s.asset==kDialogueAsset) return s.argument<std::size(kDialogueRows) && submitted_[s.argument] && now_>=voiceEnd_[s.argument];
    if(s.asset.type==1) {const auto i=spawn_index(s.asset);return i<std::size(kSpawns) && (s.argument==1?population_.ready(i,kSpawns[i].count):population_.cleared(i,kSpawns[i].count));}
    if(s.asset.type==43) {const auto i=scene_index(s.asset);return i<std::size(kScenes) && scenes_.seen(i,s.argument==1?1:2);}
    if(s.asset.type==4) {
        const auto l=lens_index(s.asset);if(s.argument==50) return l<std::size(kLenses) && lenses_[l].dead();
        const auto i=asset_index(s.asset);return i<std::size(kAssets) && frame_.native[i].acknowledged;
    }
    return entered(s.asset);
}
coo::StallDetail Controller::missing(const coo::CommandSpec& s) const noexcept {
    if(s.wait==coo::Wait::requested || observed(s)) return {};
    if(s.asset.type==1) {const auto i=spawn_index(s.asset);auto out=population_.missing(i,i<std::size(kSpawns)?kSpawns[i].count:0,s.argument==2);out.asset=s.asset;return out;}
    // Every unsatisfied type-4 wait used to report object_creation, so "the cube was never spawned"
    // and "the cube is standing there and nobody has shot it" produced an identical line. Those are
    // opposite problems and telling them apart cost a whole playthrough. Walk the real ladder.
    if(s.asset.type==4) {
        const auto i=asset_index(s.asset);
        if(i==std::size(kAssets) || !frame_.native[i].acknowledged) {
            return {coo::Missing::object,s.asset,1U,i<std::size(kAssets) && frame_.native[i].active?1U:0U};
        }
        if(s.argument!=50) { return {coo::Missing::observation,s.asset}; }
        const auto l=lens_index(s.asset);
        if(l==std::size(kLenses)) { return {coo::Missing::observation,s.asset}; }
        if(!lenses_[l].owner().valid()) { return {coo::Missing::controller,s.asset,1U,0U}; }
        if(!frame_.lensExposed[l]) { return {coo::Missing::preparation,s.asset,1U,0U}; }
        return {coo::Missing::death,s.asset,1U,lenses_[l].immune()?0U:1U};
    }
    return {coo::Missing::observation,s.asset};
}
void Controller::update_module(std::uint32_t id,const coo::MissionInput& input,Frame& output) noexcept {
    if(id!=1 || !views_ || input.run!=run_) return;
    now_=input.now;frame_.region=input.region;if(input.region>=0 && input.region/8<64) regions_.set(input.region/8);frame_.gameplayClockTicks=clock_.sample(now_);
    if(!landed_) {output=frame_;return;}
    frame_.enabled=!frame_.populationFault;
    if(!frame_.enabled) {output=frame_;return;}
    if(frame_.finished) {
        dialogue_.advance(views_->dialogue,frame_.spawnGeneration-1U,now_,false,frame_,frame_.revision);
        if(lifecycle_.advance(now_)) ++frame_.revision;
        frame_.completion=lifecycle_.publication();output=frame_;return;
    }
    if(frame_.bossDead && !frame_.ending) {
        // Death cancels pending wave/scene joins, preserving the script-authored
        // reward and closing exchange even after a legitimate early boss kill.
        executor_.cancel(*this);frame_.ending=true;started_=false;
        dialogue_.discard_before(frame_.section);++frame_.revision;
    }
    update_cover();
    if(!started_) started_=executor_.start(graph()->definition,run_);
    if(!started_) return;
    // advance()'s `run` argument becomes the generation stamped on the dialogue record
    // (generations[row] = run % 0x7FFFFFFE + 1). Passing run_ made every row publish at
    // generation 2; beyond_infinity, deadly_trial and deep_storage all pass
    // spawnGeneration-1 and gateway and strike_pact pass publicationGeneration-1. Garden was
    // the only mission not stamping a lifecycle generation here.
    dialogue_.advance(views_->dialogue,frame_.spawnGeneration-1U,now_,false,frame_,frame_.revision);
    executor_.update(*this);
    for(const auto& b:graph()->commands) {
        const auto state=executor_.step_state(b.step);if(state.phase!=coo::StepPhase::active || !state.commands[b.command].requested) continue;
        const auto& s=graph()->definition.steps[b.step].commands[b.command];
        if(coo::is_observation(s.operation) && observed(s)) static_cast<void>(executor_.enqueue({executor_.token(b.step,b.command),coo::Milestone::observed}));
    }
    executor_.update(*this);
    if(!frame_.ending && executor_.diagnostics().phase==coo::Phase::complete && static_cast<std::size_t>(frame_.section)+1<views_->phases.size()) {
        ++frame_.section;executor_.cancel(*this);started_=executor_.start(graph()->definition,run_);
        if(started_) executor_.update(*this);
        ++frame_.revision;
    }
    if(lifecycle_.advance(now_)) ++frame_.revision;
    frame_.enabled=!frame_.populationFault && executor_.diagnostics().phase!=coo::Phase::failed;
    frame_.scenes=scenes_.commands();frame_.checked=frame_.finished;frame_.presentation=objectives_.state();frame_.completion=lifecycle_.publication();output=frame_;
}
Frame Controller::update(std::uint64_t run,std::uint64_t now,bool ready,int region) noexcept {
    if(!views_ || run!=run_ || !ready) return {};
    return composition_.update(views_->mission,{run,now,region,false,true},*this);
}
}
