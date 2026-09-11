#include "../src/state/activity/strike_bond/controller.h"
#include "../src/state/activity/strike_bond/authority.h"
#include "../src/state/activity/strike_bond/boss_damage.h"
#include "../src/client/activity/campaign_openings.h"
#include "../src/server/bap/encrypted/activity_message/membership/activity_membership_route.h"
#include "strike_bond_boss_damage_tests.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <limits>
#include <vector>
namespace m=sunrise::state::activity::strike_bond;
namespace coo=sunrise::state::activity::coo;
namespace sense=sunrise::middleware::bap::activity_message;
static unsigned checks{};
static void check(bool ok,const char* message) {
    ++checks;if(!ok) {std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}
}
static std::unique_ptr<coo::script::MissionDocument> parse(std::string_view text) {
    std::string error;auto d=coo::script::MissionDocument::parse_lua(text,m::kProfile,error);
    if(!d) std::fprintf(stderr,"Lua: %s\n",error.c_str());return d;
}
static std::string shipped() {
    std::ifstream in(std::filesystem::path(__FILE__).parent_path().parent_path()/"scripts"/"strike_bond.lua");
    std::ostringstream s;s<<in.rdbuf();check(in.good() || in.eof(),"read shipped mission");return s.str();
}
static m::Point inside(const m::Volume& v) {
    for(unsigned x=1;x<40;++x) for(unsigned y=1;y<40;++y) {
        m::Point p{v.min.x+(v.max.x-v.min.x)*float(x)/40.F,v.min.y+(v.max.y-v.min.y)*float(y)/40.F,(v.min.z+v.max.z)/2.F};
        if(m::contains(v,p)) return p;
    }std::abort();
}
struct Wire {
    std::size_t bits{};std::vector<std::pair<std::uint64_t,unsigned>> fields;
    std::size_t bit_count() const noexcept {return bits;}
    bool write(std::uint64_t value,unsigned width) {bits+=width;fields.push_back({value,width});return true;}
};
struct Replay {
    std::unique_ptr<coo::script::MissionDocument> document=parse(shipped());
    m::Controller c;
    std::uint64_t run{91},now{1000};
    int region{120};
    std::uint32_t nextActor{1000};
    std::array<std::vector<m::EnemyReceipt>,std::size(m::kSpawns)> actors;
    std::bitset<std::size(m::kScenes)> started,finished;
    std::bitset<std::size(m::kDialogueRows)> spoken;
    std::bitset<8> sections;
    bool checkedFakeDeath{},checkedReadiness{},checkedImmune{},checkedCannon{},checkedClosing{},checkedShields{};
    Replay() {
        check(document!=nullptr,"shipped Lua parses");
        check(m::valid_document(document->views()),"registered native profile");
        check(c.select(document->views(),run),"select mission");
        tick();c.position(run,{123.5F,255.5F,89.5F});tick();
    }
    void tick() {
        now+=100;c.update(run,now,true,region);sections.set(c.frame().section);
        check(c.diagnostics().phase!=coo::Phase::failed,"executor does not fail");
    }
    void scene(std::size_t i,bool complete) {
        sense::scene_sense::Output r{};r.delta=true;r.completed=complete;r.generationWire=0x80000000U+c.frame().scenes[i].generation;
        c.scene(run,m::kScenes[i].asset.registry,m::kScenes[i].asset.slot,r);
    }
    void service(bool submit=true) {
        const auto snapshot=c.frame();
        for(std::size_t i=0;i<std::size(m::kAssets);++i) {
            const auto a=m::kAssets[i].asset;const auto s=snapshot.native[i];
            if(!s.managed || !s.desired || a.type!=4) continue;
            if(!s.prepared) {check(c.prepared(c.owner(),a),"native object preparation");continue;}
            if(!s.acknowledged) check(c.object({{run,s.generation},a,static_cast<std::uint32_t>(30000+i),static_cast<std::uint32_t>(40000+i)}),"native object creation");
            const auto l=m::lens_index(a);
            if(l<std::size(m::kLenses) && !c.lens_request(l).lens.valid()) {
                const m::LensReceipt r{{run,s.generation},a,0x100000+i*0x1000,static_cast<std::uint32_t>(30000+i),static_cast<std::uint32_t>(40000+i),static_cast<std::uint32_t>(50000+i)};
                check(!c.lens(r,true),"a dead candidate cannot invent the live lens owner");
                check(c.lens(r,false),"bind real live lens");
                if(!c.lens_request(l).vulnerable) {
                    check(!c.lens(r,true),"shielded lens cannot be destroyed");checkedImmune=true;
                }
                auto stale=r;--stale.owner.run;
                check(!c.lens(stale,true),"stale lens generation rejected");
            }
        }
        for(std::size_t i=0;i<std::size(m::kSpawns);++i) {
            const auto& spawn=m::kSpawns[i];
            if(!c.frame().native[m::asset_index(spawn.asset)].active) continue;
            if(actors[i].empty()) for(unsigned n=0;n<spawn.count;++n) {
                m::EnemyReceipt r{run,nextActor++,nextActor+10000,c.frame().spawnGeneration,spawn.asset.slot,spawn.asset.registry};
                check(c.admitted(r),"admit requested native enemy");actors[i].push_back(r);
                check(!c.admitted(r),"duplicate birth rejected");
                auto stale=r;++stale.generation;check(!c.died(stale),"wrong-generation death rejected");
            }
            if(spawn.sceneOwned && !checkedReadiness) {
                const coo::CommandSpec ready{coo::Operation::observation,spawn.asset,1,coo::Wait::observed};
                check(c.missing(ready).missing!=coo::Missing::none,"admission alone does not satisfy ready");
                for(const auto& r:actors[i]) c.readiness(r,{true,true,false,false,r.actor+20000});
                check(c.missing(ready).missing==coo::Missing::ai,"missing AI is diagnosed independently");
                checkedReadiness=true;
            }
            for(const auto& r:actors[i]) {
                coo::EnemyReadiness v{true,true,true,true,r.actor+20000,spawn.asset.registry,spawn.objective,
                    static_cast<std::int8_t>(static_cast<int>(c.frame().taskPlusOne[i])-1)};
                c.readiness(r,v);
            }
        }
        for(std::size_t i=0;i<std::size(m::kScenes);++i) {
            if(!c.frame().scenes[i].generation || started[i]) continue;
            if(!m::body_bits(c.frame(),m::kScenes[i].asset.registry,43,m::kScenes[i].asset.slot)) continue;
            started.set(i);scene(i,false);
        }
        // The native guardian graph loops in node 3 until input 33E63A8B.
        // A cube death or a source admission cannot complete that scene.
        for(std::size_t i=0;i<std::size(m::kScenes);++i) {
            const auto& binding=m::kScenes[i];const auto& command=c.frame().scenes[i];
            if(binding.graph!=0x80F45CAAU || !command.generation) continue;
            check(command.eventCount<=1,"guardian sends at most one release event");
            if(!command.eventCount) continue;
            const auto lens=m::lens_index(binding.cast[1]);
            check(started[i] && lens<std::size(m::kLenses) && c.frame().lensDestroyed[lens],
                "guardian release requires its own destroyed cube and a started scene");
            check(command.events[0]==0x33E63A8BU,"guardian uses the native animation exit event");
            Wire wire;check(m::write_body(wire,c.frame(),binding.asset.registry,43,binding.asset.slot)
                && wire.fields.back()==std::pair<std::uint64_t,unsigned>{0x33E63A8BU,32},
                "guardian release reaches the native authority packet");
            if(!finished[i]) {finished.set(i);scene(i,true);}
        }
        const auto f=c.frame();
        if(submit && f.activeRow!=coo::kNoDialogue) {
            check(!c.submitted(run+1,m::kBank,f.activeRow,f.generations[f.activeRow],now),"stale dialogue callback rejected");
            if(f.activeRow==14) {
                check(!c.frame().finished,"offered closing dialogue does not complete mission");
                check(!c.submitted(run,m::kBank,14,f.generations[14]+1,now),"wrong closing generation rejected");
                now+=500;tick();check(!c.frame().finished,"waiting for real closing submission");
            }
            check(c.submitted(run,m::kBank,f.activeRow,f.generations[f.activeRow],now),"native voice submission accepted");
            if(f.activeRow==14) {tick();check(!c.frame().finished,"submission alone does not truncate closing clip");}
            spoken.set(f.activeRow);
        }
        if(c.frame().bossFighting && !checkedFakeDeath) {
            const auto boss=c.boss_enemy();check(boss.valid(),"Dendron has a native identity");
            sense::combatant_sense::Output actor{};actor.snapshotValid=true;actor.hasSpawnRevision=true;
            actor.spawnRevision=c.frame().spawnGeneration;actor.hasActorQuery=true;actor.actorQuery=0;
            c.combatant(run,m::kBossActor.registry,m::kBossActor.slot,actor);
            actor.detached=true;c.combatant(run,m::kBossActor.registry,m::kBossActor.slot,actor);
            sense::squad_sense::Output squad{};squad.initialized=true;squad.hasAlive=true;squad.alive=1;
            c.squad(run,m::kBossActor.registry,3,squad);squad.alive=0;squad.removal=true;c.squad(run,m::kBossActor.registry,3,squad);
            check(c.health(boss,0.F),"zero native fraction can be observed");
            check(!c.frame().bossDead && !c.frame().finished,"health zero, actor detach and empty squad are not death");
            check(!c.health(boss,std::numeric_limits<float>::quiet_NaN()),"nonfinite health rejected");
            auto wrong=boss;++wrong.actor;check(!c.health(wrong,.5F),"unowned health rejected");
            c.health(boss,1.F);checkedFakeDeath=true;
        }
        if(c.graph()->id=="tower" && !checkedCannon) {
            const auto* cannon=m::find(0x2CB86C0FU,23,222);
            check(cannon && c.frame().native[m::asset_index(cannon->asset)].active,"lower cannon enabled before higher floor cube");
            check(!c.frame().lensDestroyed[15],"lower cannon does not require tower cube death");checkedCannon=true;
        }
        if(!checkedShields) for(const auto& g:m::kGolems) {
            if(!m::body_bits(c.frame(),g.registry,26,g.tether)) continue;
            Wire shield,collection;
            check(m::write_body(shield,c.frame(),g.registry,26,g.tether) && shield.bits==186,"native linked shield bit count");
            check(m::write_body(collection,c.frame(),g.registry,34,g.collection) && collection.bits==94,"native single-golem collection bit count");
            check(collection.fields.back().first==32768U+g.source,"shield collection targets the exact Minotaur source");
            checkedShields=true;break;
        }
    }
    void satisfy(const coo::CommandSpec& s) {
        if(const auto* condition=document->views().condition(s)) {
            for(const auto& node:condition->nodes) if(node.kind==coo::script::ConditionNode::Kind::observation) {satisfy(node.native);break;}
            return;
        }
        if(s.asset==m::kRegion) {region=static_cast<int>(s.argument);tick();return;}
        if(s.asset==m::kDialogueAsset) {now+=m::kDialogueRows[s.argument].durationMs+500;return;}
        if(s.asset==m::kBossActor) {
            const auto boss=c.boss_enemy();
            if(s.argument==32 || s.argument==33) {
                const unsigned expected=s.argument==32?0U:1U;
                check(c.frame().bossStage==expected,"boss damage stages advance only after the previous guardian pair dies");
                for(unsigned n=expected*2;n<4;++n) {
                    const auto* cube=m::find(m::kBossActor.registry,4,static_cast<std::uint16_t>(180+n*8));
                    check(!c.frame().native[m::asset_index(cube->asset)].active,"future shield cubes are absent during damage phase");
                }
                now+=30000;check(c.health(boss,s.argument==32?2.F/3.F:1.F/3.F),"native boss phase health");return;
            }
            if(s.argument==30 && !c.frame().bossDead) check(c.died(boss),"authentic Dendron death");
            return;
        }
        if(s.asset.type==60) for(const auto& v:m::kVolumes) if(v.asset==s.asset) {
            region=v.bubble*8;tick();c.position(run,inside(v));return;
        }
        if(s.asset.type==1 && s.argument==2) {
            const auto n=m::spawn_index(s.asset);check(n<actors.size() && !actors[n].empty(),"required kill has admitted actor");
            for(std::size_t i=0;i<std::size(m::kScenes);++i) {
                const auto& binding=m::kScenes[i];
                if(binding.graph==0x80F45CAAU && binding.cast[0]==s.asset)
                    check(finished[i],"Minotaur cannot fight while its native scene still holds it");
            }
            if(s.asset.registry==m::kBossActor.registry && s.asset.slot>=174 && s.asset.slot<=198) {
                const auto stage=c.frame().bossStage;
                check(stage<2 && m::boss_blocked(c.frame(),m::boss_floor(c.frame())),"Dendron stays immune after cubes break until guardian deaths");
            }
            for(const auto& r:actors[n]) c.died(r);now+=15000;return;
        }
        if(s.asset.type==4 && s.argument==50) {
            if(s.asset==m::kLenses[7].source) {
                check(!c.boss_enemy().valid(),"middle cube is exposed before Dendron is requested");
                check(!c.frame().scenes[2].generation,"aborting intro scene cannot reserve the boss");
                for(unsigned n=8;n<12;++n) check(!c.frame().native[m::asset_index(m::kLenses[n].source)].active,"roof guardians are absent before encounter start");
            }
            for(const auto& tether:m::kRouteTethers) if(m::kLenses[tether.lens].source==s.asset) {
                const std::uint16_t route=tether.guardian==105?81:tether.guardian==121?97:260;
                const auto routeAsset=m::find(tether.source.registry,4,route)->asset;
                check(c.frame().native[m::asset_index(routeAsset)].acknowledged,"both room cubes exist before breaking the guardian cube");
                check(!c.frame().lensExposed[m::lens_index(routeAsset)],"transporter cube stays shielded until Minotaur death");
                check(!c.frame().native[m::asset_index(m::find(tether.source.registry,23,tether.shield)->asset)].active,"guardian cube has no surrounding shield");
                check(c.frame().native[m::asset_index(tether.source)].acknowledged,"short beam exists before cube becomes vulnerable");
            }

            const auto l=m::lens_index(s.asset);const auto q=c.lens_request(l);
            if(q.lens.valid() && q.vulnerable && !q.destroyed) {
                check(c.lens(q.lens,true),"real vulnerable cube destruction");
                for(const auto& tether:m::kRouteTethers) if(tether.lens==l) {
                    check(!c.frame().native[m::asset_index(tether.source)].active,"real cube death retires short beam immediately");
                    check(!c.frame().native[m::asset_index(m::find(tether.source.registry,23,tether.center)->asset)].active,"real cube death removes center beam immediately");
                }
                now+=10000;
            }
            return;
        }
        if(s.asset.type==43 && s.argument==2) {
            const auto n=m::scene_index(s.asset);
            if(m::kScenes[n].graph==0x80F45CAAU) return; // service requires the actual exit event
            if(started[n] && !finished[n]) {finished.set(n);scene(n,true);}
        }
    }
    void run_all(bool early=false) {
        for(unsigned pass=0;pass<1200 && !c.frame().finished;++pass) {
            tick();service();
            if(early && c.frame().bossFighting && !c.frame().bossDead) check(c.died(c.boss_enemy()),"legitimate early boss kill");
            if(c.graph()->id=="ending" && !checkedClosing) {
                const auto before=c.frame();
                check(!before.finished,"closing exchange precedes completion");
                check(!c.died(c.boss_enemy()),"duplicate boss death cannot grant another reward");
                checkedClosing=true;
            }
            const auto* g=c.graph();
            for(const auto& b:g->commands) {
                const auto state=c.step_state(b.step);
                if(state.phase!=coo::StepPhase::active || !state.commands[b.command].requested) continue;
                const auto& s=g->definition.steps[b.step].commands[b.command];
                if(coo::is_observation(s.operation)) satisfy(s);
            }
        }
        if(!c.frame().finished) {
            std::fprintf(stderr,"Stalled graph %.*s active=%08X complete=%08X\n",static_cast<int>(c.graph()->id.size()),c.graph()->id.data(),c.diagnostics().active,c.diagnostics().complete);
        }
        check(c.frame().finished && c.frame().bossDead,"full strike reaches genuine completion");
        if(!(early?sections[6]:sections.all())) std::fprintf(stderr,"sections=%s early=%d graph=%.*s\n",sections.to_string().c_str(),early,int(c.graph()->id.size()),c.graph()->id.data());
        check(early?sections[6]:sections.all(),"all required route and boss phases visited");
        for(const auto row:{0,1,3,5,7,8,9,10,11,12,13,14}) if(!early || row==14) {if(!spoken[row]) std::fprintf(stderr,"Missing dialogue row %u early=%u\n",row,early);check(spoken[row],"required strike exchange dispatched");}
        check(!spoken[2],"campaign-only dialogue excluded");
        check(checkedFakeDeath && checkedReadiness && checkedImmune && checkedCannon && checkedClosing && checkedShields,"critical regression scenarios exercised");
        check(c.frame().checkpointSliceSet==136 && c.frame().checkpointSpawnSet==0x2EA8FB98U,"Spire native respawn set retained");
        if(!early) {
            check(finished[3] && finished[4],"both Dendron shield scenes released");
            for(std::size_t i=0;i<std::size(m::kScenes);++i) {
                if(m::kScenes[i].graph==0x80F45CAAU && started[i])
                    check(finished[i],"all spawned guardians leave their dormant native scenes");
            }
            for(const auto slot:{174,182,190,198}) {
                const auto* source=m::find(m::kBossActor.registry,1,static_cast<std::uint16_t>(slot));
                check(source && c.missing({coo::Operation::observation,source->asset,2,coo::Wait::observed}).missing==coo::Missing::none,
                    "both rooftop Minotaurs must die before their boss shield phase ends");
            }
        }
        check(!c.frame().coverEnabled && !c.frame().restricted,"ending restores traversal and retires arena cover");
        for(std::uint16_t i=0;i<32;++i) {
            const auto* cover=m::find(m::kBossActor.registry,23,static_cast<std::uint16_t>(76+3*i));
            check(cover && !c.frame().native[m::asset_index(cover->asset)].active,"all cover retracts on boss death");
        }
        c.reset();check(!c.has_point() && !c.landed() && !c.boss_enemy().valid(),"reset clears prior world and boss identity");
        check(!c.died({run,1,2,1,3,m::kBossActor.registry}),"old receipt cannot affect reset run");
    }
};
static void native_regressions() {
    // Captured roof crash: source 70's cleanup list contained static point
    // (2CB86C0F,48,386), resolving to tag 80F5472F rather than a component.
    for(const auto sceneIndex:{2U,3U,4U}) {
        const auto& scene=m::kScenes[sceneIndex];const auto owned=m::participants(scene);
        check(scene.cast.back().type==48 && scene.cast.back().slot==386,"authored boss positioning point retained");
        check(owned.count==(sceneIndex==2?2U:1U),"only live boss participants enter native cleanup");
        m::Frame f{};f.enabled=true;f.spawnGeneration=1;f.region=136;
        f.native[m::asset_index(scene.asset)].managed=true;f.scenes[sceneIndex].generation=1;
        for(const auto a:owned.view()) if(a.type==4) {auto& state=f.native[m::asset_index(a)];state.active=state.acknowledged=true;}
        Wire wire;check(m::write_body(wire,f,scene.asset.registry,43,scene.asset.slot),"boss scene authority serializes");
        check(wire.fields[2]==std::pair<std::uint64_t,unsigned>{owned.count,4},"wire cleanup participant count excludes point");
        check(wire.bit_count()==m::body_bits(f,scene.asset.registry,43,scene.asset.slot),"filtered cast bit count matches packet");
        for(std::size_t i=0;i<owned.count;++i) {
            check(wire.fields[4+3*i].first==2 || wire.fields[4+3*i].first==5,"wire participant resolves an actor source or object source");
        }
    }
    m::Frame f{};f.enabled=true;f.spawnGeneration=1;f.region=136;
    Wire height;check(m::write_body(height,f,0x2CB86C0FU,32,292) && height.bits==57,"upper kill-volume command is published in Spire");
    check(height.fields==std::vector<std::pair<std::uint64_t,unsigned>>{{0x2CB86C0FU,32},{61,7},{33174,16},{1,2}},
        "height fix disables only volume 406 with the signed-enum false encoding");
    f.region=8;check(m::body_bits(f,0x2CB86C0FU,32,292)==0,"upper toggle does not affect other regions");
    f.region=136;f.enabled=false;check(m::body_bits(f,0x2CB86C0FU,32,292)==0,"disabled mission publishes no volume changes");
    m::TetherPose pose{};const m::Point cube{1572.226074F,1148.558960F,155.734802F},guardian{1569.041870F,1137.140869F,150.247101F};
    check(m::tether_pose(cube,guardian,pose),"confirmed live Spire endpoints form valid tether");
    const auto q=pose.rotation;const float length=pose.scale*10.F;
    check(std::abs(pose.position.x+length*(1.F-2.F*(q[1]*q[1]+q[2]*q[2]))-guardian.x)<.001F
        && std::abs(pose.position.y+length*2.F*(q[0]*q[1]+q[3]*q[2])-guardian.y)<.001F
        && std::abs(pose.position.z+length*2.F*(q[0]*q[2]-q[3]*q[1])-(guardian.z+3.2F))<.001F,
        "native 10m beam reaches actual shield anchor after quaternion and scale");
    check(pose.scale>1.F && pose.scale<1.5F,"short beam preserves main-line thickness");
    check(!m::tether_pose(cube,{NAN,0,0},pose) && !m::tether_pose(cube,{0,0,0},pose),"invalid or remote endpoints cannot spawn a beam");
    for(const auto& t:m::kRouteTethers) {
        check(m::kLenses[t.lens].source.slot==t.cube && m::kLenses[t.lens].source.registry==t.source.registry,"each short beam belongs to its actual guardian cube");
        check(m::find(t.source.registry,1,t.guardian) && m::route_tether(t.source)==&t,"three distinct native guardians have tether sources");
        m::Request current{{99,1},{}};current.frame.enabled=true;
        auto& source=current.frame.native[m::asset_index(t.source)];source.managed=source.active=source.desired=true;
        m::LensRequest lens{};lens.owner=current.owner;lens.index=t.lens;lens.enabled=true;
        check(m::tether_visibility(t,current,lens,current.owner)==1.F,"live cube shows its owned beam");
        lens.destroyed=true;
        check(m::tether_visibility(t,current,lens,current.owner)==0.F,"cube death explicitly hides retained render entity");
        lens.destroyed=false;source.active=source.desired=false;
        check(m::tether_visibility(t,current,lens,current.owner)==0.F,"retirement explicitly writes visibility zero");
        ++lens.owner.run;check(!m::tether_visibility(t,current,lens,current.owner),"stale owner cannot alter reused beam entity");
    }
}
static void boss_regressions() {
    m::Frame f{};f.enabled=true;f.spawnGeneration=1;
    const auto* source=m::find(m::kBossActor.registry,1,3);
    auto& state=f.native[m::asset_index(source->asset)];state.managed=state.active=true;
    Wire wire;check(m::write_body(wire,f,source->asset.registry,1,3) && wire.bits==coo::native_combatant::kSourceBits,"Dendron uses full native loose combatant authority");
    check(wire.fields[wire.fields.size()-3]==std::pair<std::uint64_t,unsigned>{1,3},"Dendron is not reserved for an intro scene");
    check(m::boss_blocked(f,1.F),"boss cannot take damage before middle cube starts fight");
    f.bossFighting=true;
    for(unsigned phase=0;phase<3;++phase) {
        f.bossStage=static_cast<std::uint8_t>(phase);
        const float floor=m::boss_floor(f);
        check(!m::boss_blocked(f,floor+.1F),"open damage bar accepts damage");
        check(m::boss_blocked(f,floor)==(phase<2),"health boundary blocks subsequent damage until guardians die");
        std::array<std::byte,0x80> packet{};const int count=2,body=7,other=9;float zero=0.F;
        std::memcpy(packet.data()+0x64,&count,4);std::memcpy(packet.data()+0x6C,&body,4);std::memcpy(packet.data()+0x78,&other,4);
        std::memcpy(packet.data()+0x70,&zero,4);
        const auto before=packet;
        const auto result=m::boss_damage::clamp(packet,body,floor);
        check(result==(phase<2?m::boss_damage::Packet::clamped:m::boss_damage::Packet::unchanged),"large damage hit cannot skip either guardian phase");
        check(m::boss_damage::get<float>(packet,0x70)==floor,"body damage stops exactly at phase floor");
        check(std::equal(packet.begin(),packet.begin()+0x70,before.begin()) && std::equal(packet.begin()+0x74,packet.end(),before.begin()+0x74),"clamp preserves unrelated hit and body-region data");
    }
}
static void catalogue() {
    auto doc=parse(shipped());check(doc && m::valid_document(doc->views()),"shipped document is authorized");
    for(const auto& graph:doc->views().graphs) {
        check(graph.definition.steps.size()<=32,"native graph step budget");
        for(const auto& step:graph.definition.steps) check(step.commands.size()<=8,"native parallel command budget");
    }
    for(const auto& g:m::kGolems) {
        check(m::find(g.registry,1,g.source) && m::find(g.registry,26,g.tether) && m::find(g.registry,34,g.collection),"golem link uses recovered assets");
        check(m::kLenses[g.lens].source.registry==g.registry,"golem lens scope matches effect");
    }
    namespace openings=sunrise::client::activity::mission_launch::openings;
    unsigned garden{};
    for(const auto& row:openings::kMissions) if(std::string_view(row.title)=="A Garden World") {
        ++garden;check(row.campaign==1,"Garden World belongs to Curse of Osiris");
        check(std::string_view(row.destination.packageName.data(),row.destination.packageNameLength)=="strike_bond","launcher selects strike package");
        check(row.destination.bubble==15 && row.destination.sliceSet==120 && row.destination.spawnSetHash==0x0232EBCEU,"native Lighthouse opening");
    }
    check(garden==1,"exactly one Garden World launcher entry");
    namespace route=sunrise::server::bap::encrypted::activity_message::membership;
    check(route::retains_held_region("strike_bond"),"Garden retains real held-region transitions");
}
int main() {
    catalogue();native_regressions();boss_regressions();check(strike_bond_boss_damage_contracts(),"Dendron native damage identity contracts");Replay normal;normal.run_all();Replay early;early.run_all(true);
    std::printf("PASS: %u Garden World route, ownership, shield, native death, dialogue and launcher checks\n",checks);
}
