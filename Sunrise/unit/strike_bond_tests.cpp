#include "../src/state/activity/strike_bond/controller.h"
#include "../src/state/activity/strike_bond/authority.h"
#include "../src/state/activity/strike_bond/boss_damage.h"
#include "../src/client/activity/campaign_openings.h"
#include "../src/server/bap/encrypted/activity_message/membership/activity_membership_route.h"
#include "strike_bond_boss_damage_tests.h"
#include "strike_bond_fire_trace_tests.h"
#include "strike_bond_intro_release_tests.h"
#include "strike_bond_target_binding_tests.h"
#include "strike_bond_carriage_tests.h"
#include "strike_bond_boss_shield_tests.h"
#include "strike_bond_boss_retirement_tests.h"
#include "../src/client/hooks/bootflow/strike_bond_boss_cycle.h"
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
    std::uint64_t at(std::size_t offset,unsigned width) const {
        std::size_t cursor{};
        for(const auto& [value,size]:fields) {if(cursor==offset && size==width) return value;cursor+=size;}
        return UINT64_MAX;
    }
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
    bool delayTethers{},rapidDamage{};
    bool checkedFakeDeath{},checkedReadiness{},checkedImmune{},checkedCannon{},checkedPastCannons{},checkedClosing{},checkedShields{};
    explicit Replay(bool delayed=false,bool rapid=false) : delayTethers(delayed),rapidDamage(rapid) {
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
            if(delayTethers && m::route_tether(a)) continue;
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
            check(started[i] && lens<std::size(m::kLenses),
                "guardians release native AI only after their owning scene starts");
            check(command.events[0]==0x33E63A8BU,"guardian uses the native animation exit event");
            Wire wire;check(m::write_body(wire,c.frame(),binding.asset.registry,43,binding.asset.slot)
                && wire.fields.back()==std::pair<std::uint64_t,unsigned>{0x33E63A8BU,32},
                "guardian release reaches the native authority packet");
            // The same guardian graph can release AI while its cube is intact.
            // Keep scene completion pending to catch accidental gameplay gates.
            if(!finished[i] && c.frame().lensDestroyed[lens]) {
                finished.set(i);scene(i,true);
            }
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
        const auto& intro=c.frame().scenes[2];
        if(intro.eventCount) {
            check(intro.eventCount==1 && intro.events[0]==0x01994745U,"Dendron receives exactly its authored intro exit event");
            check(started[2] && c.frame().lensDestroyed[7],"native boss release follows its own cube destruction");
            Wire wire;check(m::write_body(wire,c.frame(),m::kScenes[2].asset.registry,43,m::kScenes[2].asset.slot)
                && wire.fields.back()==std::pair<std::uint64_t,unsigned>{0x01994745U,32},"boss intro release is serialized to native scene");
        }
        if(c.frame().bossFighting) check(finished[2],"damage phase requires authentic native intro completion");
        if(c.boss_enemy().valid() && !c.frame().bossFighting && !checkedFakeDeath) {

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
        if(c.graph()->id=="past" && !checkedPastCannons) {
            for(std::uint16_t slot=141;slot<=144;++slot) {
                const auto* cannon=m::find(0xC95ECB1AU,23,slot);
                check(cannon && c.frame().native[m::asset_index(cannon->asset)].active,"all Past cannon effects requested on area entry");
                Wire wire;check(m::write_body(wire,c.frame(),0xC95ECB1AU,23,slot) && wire.bits==147,"each Past effect has a native device publication");
                check(wire.fields.front().first==std::bit_cast<std::uint32_t>(1.F),"Past cannon effect uses the active endpoint");
            }
            check(!c.frame().native[m::asset_index(m::find(0xC95ECB1AU,4,140)->asset)].desired,"effect activation cannot request the gated main launch source");
            checkedPastCannons=true;
        }
        if(c.graph()->id=="tower" && !checkedCannon) {
            for(std::uint16_t slot=222;slot<=225;++slot) {
                const auto* cannon=m::find(0x2CB86C0FU,23,slot);
                check(cannon && c.frame().native[m::asset_index(cannon->asset)].active,"all Spire cannon effects enabled before higher floor cube");
                Wire wire;check(m::write_body(wire,c.frame(),0x2CB86C0FU,23,slot) && wire.bits==147,"each Spire effect has a native device publication");
                check(wire.fields.front().first==std::bit_cast<std::uint32_t>(1.F),"Spire cannon effect uses the active endpoint");
            }
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
            if(s.argument==38) {
                check(c.frame().bossCycle.mode==m::BossMode::opening,"middle cube begins native opening");
                check(!c.frame().coverEnabled && m::boss_blocked(c.frame(),1.F),"opening waits before cover and damage");
                check(!c.boss_animation(boss,0,m::BossAnimation::openingAwake),"opening completion cannot precede start");
                check(c.boss_motion(boss,c.boss_platform(),0.F),"capture stationary native spawn");
                check(c.boss_animation(boss,0,m::BossAnimation::openingStarted),"native opening animation begins");
                check(c.boss_animation(boss,0,m::BossAnimation::openingAwake),"native opening completes");
                check(!m::boss_blocked(c.frame(),1.F),"opening completion permits damage");return;
            }
            if(s.argument==32 || s.argument==33) {
                const unsigned expected=s.argument==32?0U:1U;
                check(c.frame().bossStage==expected,"boss damage stages advance only after the previous guardian pair dies");
                for(unsigned n=expected*2;n<4;++n) {
                    const auto* cube=m::find(m::kBossActor.registry,4,static_cast<std::uint16_t>(180+n*8));
                    check(!c.frame().native[m::asset_index(cube->asset)].active,"future shield cubes are absent during damage phase");
                }
                now+=30000;check(c.health(boss,s.argument==32?2.F/3.F:1.F/3.F),"native boss phase health");return;
            }
            if(s.argument==34 || s.argument==35) {
                const auto cycle=static_cast<std::uint8_t>(s.argument-33);
                check(c.frame().bossCycle.mode==m::BossMode::parking,"guardian phase waits for native parking");
                check(c.boss_animation(boss,cycle,m::BossAnimation::asleep),"native folded sequence started");
                check(!c.boss_animation(boss,cycle,m::BossAnimation::parked),"no synthetic parking without native position");
                if(!c.frame().bossCycle.hasParkTarget) check(c.boss_motion(boss,c.boss_platform(),.8F),"first position selects parking destination");
                if(c.frame().bossPlatformSnap) {
                    const auto& rebase=c.frame().native[m::asset_index(m::find(m::kBossActor.registry,23,173)->asset)];
                    check(c.boss_platform_motion(boss,c.boss_platform(),{rebase.position,rebase.position,0.F,static_cast<std::int32_t>(rebase.generation)}),"native phase rebase acknowledged before travel");
                }
                check(c.boss_motion(boss,c.boss_platform(),c.frame().bossCycle.parkTarget),"observed parking position");
                check(c.boss_animation(boss,cycle,m::BossAnimation::parked),"native parked receipt opens guardian phase");return;
            }
            if(s.argument==36 || s.argument==37) {
                const auto cycle=static_cast<std::uint8_t>(s.argument-35);
                check(c.frame().bossCycle.mode==m::BossMode::waking,"both guardians lead to wake-up, not immediate DPS");
                check(!m::boss_blocked(c.frame(),cycle==1?2.F/3.F:1.F/3.F),"boss takes damage throughout wake-up");
                check(!c.boss_animation(boss,cycle,m::BossAnimation::awake),"completion cannot precede wake-up start");
                check(c.boss_animation(boss,cycle,m::BossAnimation::wakeStarted),"native wake-up and burst started");
                const auto platform=m::asset_index(m::find(m::kBossActor.registry,23,173)->asset);
                now+=5000;c.update(run,now,true,region);
                check(c.frame().native[platform].position==c.frame().bossCycle.parkTarget,"platform stays parked throughout animation");
                if(rapidDamage) {
                    check(c.health(boss,cycle==1?1.F/3.F:0.F),"wake-up damage reaches next floor");
                    check(c.frame().bossCycle.mode==m::BossMode::waking,"damage cannot cut the wake animation short");
                    check(m::boss_blocked(c.frame(),m::boss_floor(c.frame()))==(cycle==1),"next gate still clamps wake-up damage");
                }
                check(c.boss_animation(boss,cycle,m::BossAnimation::awake),"native wake completes before movement or next animation");
                check((c.frame().bossCycle.awakened & (1U<<(cycle-1)))!=0,"wake completion persists across immediate next health gate");
                if(rapidDamage) check(c.frame().bossCycle.mode==(cycle==1?m::BossMode::parking:m::BossMode::dying),"cached wake-up damage starts next gate or death without another hit");
                else {
                    check(c.frame().bossCycle.hasReturnTarget && (m::boss_at(c.frame().bossCycle.returnTarget,0.F) || m::boss_at(c.frame().bossCycle.returnTarget,.5F)),"wake-up returns to an equivalent spawn position");
                }
                return;
            }
            if(s.argument==30 && !c.frame().bossDead) {
                check(c.health(boss,0.F),"zero health requests native death sequence");
                check(!c.frame().bossDead && c.frame().bossCycle.mode==m::BossMode::dying,"zero health is dying, not a death receipt");
                check(c.frame().musicCandidate!=m::kMusicBossDead,"zero health cannot start closing music");
                check(!m::boss_blocked(c.frame(),0.F),"dying permits the animation to deliver its native kill event");
                check(c.boss_motion(boss,c.boss_platform(),.42F),"capture final live platform position");
                check(c.boss_animation(boss,2,m::BossAnimation::deathStarted),"native death animation freezes platform");
                check(c.frame().bossPlatformSnap && !c.frame().bossDead,"platform freeze does not fabricate boss death");
                check(!m::boss_blocked(c.frame(),0.F),"platform freeze cannot re-enable death immunity");
                const auto bossSource=m::find(m::kBossActor.registry,1,3)->asset;
                const auto generation=c.frame().native[m::asset_index(bossSource)].generation;
                Wire alive;check(m::write_body(alive,c.frame(),bossSource.registry,1,3),"dying source remains published");
                check(alive.at(121,32)==0x80000001U && alive.at(603,2)==2,"death presentation retains its native owner until real death");
                check(c.died(boss),"authentic Dendron death");
                const auto& retired=c.frame().native[m::asset_index(bossSource)];
                check(!retired.active && !retired.desired && retired.generation==generation+1,"real death retires one source generation");
                Wire after;check(m::write_body(after,c.frame(),bossSource.registry,1,3) && after.bits==coo::native_combatant::kSourceBits,"dead source publishes native retirement");
                check(after.at(121,32)==0x80000000U && after.at(173,31)==generation+1
                    && after.at(572,31)==generation+1 && after.at(603,2)==1,"retirement removes owned entities without spawning another boss");
                Wire member;check(m::write_body(member,c.frame(),m::kBossActor.registry,2,m::kBossActor.slot)
                    && member.bits==coo::native_combatant::kBindBits,"death explicitly retires the named combatant owner");
                check(member.at(1,31)==generation+1 && member.at(32,2)==1 && member.at(34,3)==2
                    && member.at(37,1)==0,"named teardown destroys its attached actor and disables rebinding");
                check(c.frame().musicCandidate==m::kMusicBossDead,"authentic death starts closing music");
            }
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
            for(const auto& golem:m::kGolems) if(m::kLenses[golem.lens].source==s.asset
                && golem.registry==m::kBossActor.registry && golem.source>=174 && golem.source<=198) {
                for(std::size_t si=0;si<std::size(m::kScenes);++si)
                    if(m::kScenes[si].graph==0x80F45CAAU && m::kScenes[si].cast[1]==s.asset)
                        check(started[si] && !finished[si] && c.frame().scenes[si].eventCount==1,
                            "rooftop guardian AI is released while its cube is intact and scene completion is pending");
                Wire shield;check(m::write_body(shield,c.frame(),golem.registry,26,golem.tether)
                    && shield.fields[1]==std::pair<std::uint64_t,unsigned>{0,1},
                    "early rooftop AI release preserves the cube-owned Minotaur shield");
                check(c.frame().bossStage<2 && m::boss_blocked(c.frame(),m::boss_floor(c.frame())),
                    "early guardian AI cannot release Dendron from immunity");
            }
            if(s.asset==m::kLenses[7].source) {
                check(c.frame().objective==0x82271EEAU && c.frame().presentation.marker.asset==m::kLenses[7].source,"roof sabotage objective points to the shootable middle cube");
                check(!c.frame().coverEnabled,"cover waits until the boss opening completes");
                check(c.boss_enemy().valid(),"Dendron exists before the middle cube can start combat");
                check(!c.frame().bossFighting && m::boss_blocked(c.frame(),1.F),"Dendron remains immune until the middle cube is destroyed");
                check(started[2] && !finished[2],"native boss intro is active while the middle cube is intact");
                check(c.frame().scenes[2].eventCount==0,"middle cube cannot release the intro early");
                for(unsigned n=8;n<12;++n) check(!c.frame().native[m::asset_index(m::kLenses[n].source)].active,"roof guardians are absent before encounter start");
            }
            for(const auto& tether:m::kRouteTethers) if(m::kLenses[tether.lens].source==s.asset) {
                const std::uint16_t route=tether.guardian==105?81:tether.guardian==121?97:260;
                const auto routeAsset=m::find(tether.source.registry,4,route)->asset;
                check(c.frame().native[m::asset_index(routeAsset)].acknowledged,"both room cubes exist before breaking the guardian cube");
                for(std::size_t si=0;si<std::size(m::kScenes);++si) if(m::kScenes[si].graph==0x80F45CAAU && m::kScenes[si].cast[1]==s.asset)
                    check(!finished[si] && c.frame().scenes[si].eventCount==1,
                        "route cube is damageable and beam requested while AI is released but the scene remains unfinished");
                for(const auto& g:m::kGolems) if(g.registry==tether.source.registry && g.source==tether.guardian) {
                    Wire effect;check(m::write_body(effect,c.frame(),g.registry,26,g.tether) && effect.fields[1]==std::pair<std::uint64_t,unsigned>{0,1},"route guardian shield remains enabled while its AI is released");
                }
                check(!c.frame().lensExposed[m::lens_index(routeAsset)],"transporter cube stays shielded until Minotaur death");
                check(!c.frame().native[m::asset_index(m::find(tether.source.registry,23,tether.shield)->asset)].active,"guardian cube has no surrounding shield");
                check(c.frame().native[m::asset_index(tether.source)].desired,"short beam requested before cube becomes vulnerable");
                if(delayTethers) check(!c.frame().native[m::asset_index(tether.source)].acknowledged,"cube exposes even when native beam creation never acknowledges");
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
            if(n==2) check(c.frame().lensDestroyed[7] && c.frame().scenes[n].eventCount==1 && !c.frame().bossFighting,"intro completion is awaited after cube release, before damage phase");
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
                const auto source=m::asset_index(m::find(m::kBossActor.registry,1,3)->asset);
                check(c.frame().native[source].generation==before.native[source].generation,"duplicate death cannot repeat retirement");
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
        Wire retired;check(m::write_body(retired,c.frame(),m::kBossActor.registry,1,3)
            && retired.at(121,32)==0x80000000U && retired.at(603,2)==1,"completion keeps boss source retired through summary");
        Wire retiredMember;check(m::write_body(retiredMember,c.frame(),m::kBossActor.registry,2,m::kBossActor.slot)
            && retiredMember.at(32,2)==1 && retiredMember.at(37,1)==0,"completion retains explicit named-owner teardown");
        if(!(early?sections[6]:sections.all())) std::fprintf(stderr,"sections=%s early=%d graph=%.*s\n",sections.to_string().c_str(),early,int(c.graph()->id.size()),c.graph()->id.data());
        check(early?sections[6]:sections.all(),"all required route and boss phases visited");
        for(const auto row:{0,1,3,4,5,6,7,8,9,10,11,12,13,14}) if(!early || row==14) {if(!spoken[row]) std::fprintf(stderr,"Missing dialogue row %u early=%u\n",row,early);check(spoken[row],"required strike exchange dispatched");}
        check(!spoken[2],"campaign-only dialogue excluded");
        for(unsigned row=15;row<24;++row) check(!spoken[row],"campaign scanner and Panoptes ending stay out of strike");
        check(checkedFakeDeath && checkedReadiness && checkedImmune && checkedCannon && checkedPastCannons && checkedClosing && checkedShields,"critical regression scenarios exercised");
        check(c.frame().checkpointSliceSet==136 && c.frame().checkpointSpawnSet==0x2EA8FB98U,"Spire native respawn set retained");
        if(!early) {
            check(!started[3] && !started[4],"one native animation owner; legacy boss scenes cannot compete");
            check(c.frame().bossCycle.awakened==3,"both native boss wake-ups completed");
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
        const auto resource=m::tether_resource(t);
        check(resource.entity==(t.source.registry==0xC95ECB1AU?0x80F4B0EEU:0x80F4B0CFU),"beam class is authored in the encounter region");
        m::TetherPose local{};check(m::tether_pose(cube,guardian,local,resource.length),"regional beam pose resolves");
        check(std::abs(local.scale*resource.length-length)<.001F,"regional beam length reaches the same shield anchor");
        check(m::find(t.source.registry,1,t.guardian) && m::route_tether(t.source)==&t,"three distinct native guardians have tether sources");
        for(const auto moving:{guardian,m::Point{guardian.x+4.F,guardian.y-3.F,guardian.z+1.F},
            m::Point{cube.x-8.F,cube.y,cube.z-3.2F}}) {
            m::TetherPose updated{};
            check(m::tether_pose(cube,moving,updated,resource.length),"moving guardian produces a new native beam pose");
            const auto q2=updated.rotation;const auto reach=updated.scale*resource.length;
            check(std::abs(updated.position.x+reach*(1.F-2.F*(q2[1]*q2[1]+q2[2]*q2[2]))-moving.x)<.001F
                && std::abs(updated.position.y+reach*2.F*(q2[0]*q2[1]+q2[3]*q2[2])-moving.y)<.001F
                && std::abs(updated.position.z+reach*2.F*(q2[0]*q2[2]-q2[3]*q2[1])-(moving.z+3.2F))<.001F,
                "beam still starts at cube and ends on moving guardian, including reversed direction");
        }
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
static void cover_regressions() {
    m::Frame f{};f.enabled=true;f.spawnGeneration=1;
    unsigned covers{};
    for(const auto& entry:m::kAssets) {
        const auto a=entry.asset;if(a.type!=23) continue;
        auto& state=f.native[m::asset_index(a)];state.managed=state.active=true;state.generation=7;state.position=1.F;
        Wire wire;check(m::write_body(wire,f,a.registry,23,a.slot) && wire.bits==147,"device position authority remains complete");
        const bool cover=a.registry==0x2CB86C0FU && a.slot>=76 && a.slot<=169 && (a.slot-76)%3==0;
        const bool mount=a.registry==0x2CB86C0FU && a.slot==173;
        check(wire.fields[2]==std::pair<std::uint64_t,unsigned>{cover || mount?0U:1U,1},"rooftop cover and Dendron mount interpolate; other devices preserve snap policy");
        if(cover) ++covers;
    }
    check(covers==32,"all 32 authored cover blocks receive smooth movement");
    check(!m::animated_cover({0x2CB86C0FU,0,4,76}),"cover motion policy cannot match object source types");
}
static void mount_regressions() {
    Replay r;
    for(unsigned pass=0;pass<1200 && !r.c.frame().bossFighting;++pass) {
        r.tick();r.service();
        const auto* graph=r.c.graph();
        for(const auto& binding:graph->commands) {
            const auto state=r.c.step_state(binding.step);
            if(state.phase!=coo::StepPhase::active || !state.commands[binding.command].requested) continue;
            const auto& spec=graph->definition.steps[binding.step].commands[binding.command];
            if(coo::is_observation(spec.operation) && !(spec.asset==m::kBossActor && spec.argument>=32)) r.satisfy(spec);
        }
    }
    check(r.c.frame().bossFighting && !r.c.frame().bossDead,"mount test reaches real cube-triggered combat");
    const auto* asset=m::find(m::kBossActor.registry,23,173);
    const auto index=m::asset_index(asset->asset);
    r.satisfy({coo::Operation::observation,m::kBossActor,38,coo::Wait::observed});r.tick();
    const auto generation=r.c.frame().native[index].generation;
    check(r.c.frame().native[index].position==0.F,"opening damage phase stays at spawn");
    r.c.update(r.run,r.now+120000,true,r.region);
    check(r.c.frame().native[index].generation==generation,"elapsed time cannot start an unwanted lap");
    const auto boss=r.c.boss_enemy();const auto platform=r.c.boss_platform();
    check(r.c.boss_motion(boss,platform,0.F),"observe live position before a gate");
    check(r.c.health(boss,.8501F) && !r.c.frame().bossCycle.hasFirstTarget,"less than 15% damage stays at spawn");
    check(r.c.health(boss,.85F),"15% damage requests first travel");
    check(r.c.frame().bossCycle.hasFirstTarget && r.c.frame().bossCycle.mode==m::BossMode::damage
        && r.c.frame().bossStage==0,"early first travel keeps combat active without spawning intermission guardians");
    const auto rebased=r.c.frame().native[index];
    check(rebased.position==.5F && r.c.frame().bossPlatformSnap,"endpoint travel publishes equivalent phase snap before destination");
    Wire rebaseWire;check(m::write_body(rebaseWire,r.c.frame(),asset->asset.registry,23,173)
        && rebaseWire.at(0,32)==std::bit_cast<std::uint32_t>(.5F) && rebaseWire.at(48,1)==1,"phase snap is carried by existing native type23 writer");
    r.c.update(r.run,r.now+120100,true,r.region);
    check(r.c.frame().native[index].generation==rebased.generation,"time cannot skip phase acknowledgement or churn publication");
    check(r.c.boss_platform_motion(boss,platform,{.5F,.5F,0.F,static_cast<std::int32_t>(rebased.generation)-1})
        && r.c.frame().native[index].generation==rebased.generation,"old native revision cannot acknowledge new rebase");
    check(r.c.boss_platform_motion(boss,platform,{.4F,.5F,.01F,static_cast<std::int32_t>(rebased.generation)})
        && r.c.frame().native[index].generation==rebased.generation,"target alone cannot acknowledge unsnapped current phase");
    auto staleRebase=boss;++staleRebase.generation;
    check(!r.c.boss_platform_motion(staleRebase,platform,{.5F,.5F,0.F,static_cast<std::int32_t>(rebased.generation)}),"retired boss cannot acknowledge rebase");
    auto otherPlate=platform;++otherPlate.serial;
    check(!r.c.boss_platform_motion(boss,otherPlate,{.5F,.5F,0.F,static_cast<std::int32_t>(rebased.generation)}),"recycled platform cannot acknowledge rebase");
    check(r.c.boss_platform_motion(boss,platform,{.5F,.5F,0.F,static_cast<std::int32_t>(rebased.generation)}),"exact native phase snap acknowledgement accepted");
    check(r.c.frame().native[index].position==m::boss_parking(1) && !r.c.frame().bossPlatformSnap
        && r.c.frame().native[index].generation==rebased.generation+1,"only acknowledged rebase advances to native smooth travel");
    Wire travelWire;check(m::write_body(travelWire,r.c.frame(),asset->asset.registry,23,173)
        && travelWire.at(0,32)==std::bit_cast<std::uint32_t>(m::boss_parking(1)) && travelWire.at(48,1)==0,"destination reaches native interpolation without snap");
    const auto firstRevision=r.c.frame().native[index].generation;
    check(r.c.boss_motion(boss,platform,.43F) && r.c.health(boss,.80F),"first travel samples moving boss");
    check(r.c.frame().native[index].generation==firstRevision,"damage during first travel cannot restart or retarget it");
    check(r.c.health(boss,2.F/3.F),"first health floor requests parking");
    check(r.c.frame().native[index].position==m::boss_parking(1),"health receipt immediately publishes parking destination");
    r.c.update(r.run,r.now+120200,true,r.region);
    check(r.c.frame().native[index].position==m::boss_parking(1),"park at P1 native lap parameter");
    check(!r.c.frame().bossPlatformSnap,"parking uses native interpolation");
    check(!r.c.boss_animation(boss,2,m::BossAnimation::asleep),"wrong-cycle animation rejected");
    auto stale=boss;++stale.generation;
    check(!r.c.boss_animation(stale,1,m::BossAnimation::asleep),"stale animation owner rejected");
    check(!r.c.boss_motion(stale,platform,.9F),"stale motion owner rejected");
    auto wrong=platform;++wrong.entity;
    check(!r.c.boss_motion(boss,wrong,.9F),"other platform rejected");
    check(!r.c.boss_motion(boss,platform,std::numeric_limits<float>::quiet_NaN()),"invalid motion rejected");
    check(r.c.boss_animation(boss,1,m::BossAnimation::asleep),"folded animation observed");
    check(!r.c.boss_animation(boss,1,m::BossAnimation::parked),"boss cannot park before platform arrives");
    check(r.c.boss_motion(boss,platform,m::boss_parking(1)),"observe P1 arrival");
    check(r.c.boss_animation(boss,1,m::BossAnimation::parked),"native parking complete");
    const auto parkedGeneration=r.c.frame().native[index].generation;
    r.c.update(r.run,r.now+300000,true,r.region);
    check(r.c.frame().native[index].generation==parkedGeneration,"parked boss remains fixed across arbitrary elapsed time");
    check(r.c.died(boss),"authentic unexpected death freezes current platform");
    check(r.c.frame().bossPlatformSnap && r.c.frame().native[index].position==m::boss_parking(1),"death stops in place without removing platform");
    check(r.c.frame().native[m::asset_index(m::kBossPlatform)].active,"platform stays present after boss death");
    r.c.reset();
    check(!r.c.frame().bossFighting,"reset retires mount combat lifecycle");
}
struct CycleCodeRead {
    std::array<std::byte,1024> bytes{};
    template<class T> bool value(std::uintptr_t at,T& out) const noexcept {
        if(at>bytes.size() || sizeof(T)>bytes.size()-at) return false;
        std::memcpy(&out,bytes.data()+at,sizeof(T));return true;
    }
    template<class T> void put(std::uintptr_t at,const T& value) noexcept {std::memcpy(bytes.data()+at,&value,sizeof(T));}
};
static void cycle_boundary_regressions() {
    namespace p=sunrise::client::hooks::bootflow::strike_bond_boss_cycle;
    CycleCodeRead read;constexpr std::uintptr_t entry=64,original=256,relay=384,replacement=512;
    read.put(entry,p::kPositionPrefix);
    check(p::position_boundary(read,entry,0,0,false),"untouched native position boundary accepted");
    for(unsigned i=0;i<7;++i) read.bytes[original+i]=static_cast<std::byte>(p::kPositionPrefix[i]);
    read.put(entry,std::uint8_t{0xE9});read.put(entry+1,static_cast<std::int32_t>(relay-entry-5));
    read.put(entry+5,std::uint16_t{0xCCCC});
    read.put(relay,std::uint16_t{0x25FF});read.put(relay+2,std::int32_t{-14});read.put(relay-8,replacement);
    read.put(original+7,std::uint16_t{0x25FF});read.put(original+9,std::int32_t{59});read.put(original+72,entry+7);
    check(p::position_boundary(read,entry,original,replacement,true),"captured Lighthouse detour preserves valid boss-cycle boundary");
    check(!p::position_boundary(read,entry,original,replacement,false),"unowned detour rejected");
    check(!p::position_boundary(read,entry,original,replacement+1,true),"foreign replacement rejected");
    read.put(original+72,entry+8);
    check(!p::position_boundary(read,entry,original,replacement,true),"wrong trampoline return rejected");
    read.put(original+72,entry+7);read.bytes[entry+8]=std::byte{};
    check(!p::position_boundary(read,entry,original,replacement,true),"changed native suffix rejected");
    check(m::boss_parking(1)==.3758F && m::boss_parking(2)==.6259F,"parking includes visually confirmed boss-body centering");
}
static void parking_route_regressions() {
    for(std::uint8_t cycle=1;cycle<=2;++cycle) for(unsigned sample=0;sample<=100;++sample) {
        const float current=sample/100.F,target=m::boss_nearest_parking(cycle,current);
        const float origin=m::boss_parking_origin(current,target);
        check(m::boss_position(origin) && m::boss_position(target),"parking stays within native input limits");
        check(std::abs(target-origin)<=.250001F,"parking never takes an extra orbit");
        check(origin==current || std::abs(std::abs(origin-current)-.5F)<.000001F,"rebase preserves the equivalent physical position");
        check(std::abs(target-m::boss_parking(cycle))<.000001F || std::abs(std::abs(target-m::boss_parking(cycle))-.5F)<.000001F,"both phase copies identify the same parking stop");
    }
    const auto target=m::boss_nearest_parking(1,.860230F);
    check(std::abs(target-.8758F)<.000001F && m::boss_parking_origin(.860230F,target)==.860230F,"captured 29-second failure chooses the nearby stop");
    check((target-.860230F)*60.F<1.F,"captured run needs less than one second at unchanged native speed");
}
static void cycle_command_regressions() {
    namespace p=sunrise::client::hooks::bootflow::strike_bond_boss_cycle;
    for(const auto sequence:{p::kSleep,p::kIntro,p::kDeath}) {
        const auto action=p::action(sequence);
        check(action[0x60]==std::byte{0x5D},"captured named start opcode");
        check(m::boss_damage::get<std::uint32_t>(action,0)==0xAFB11A12U
            && m::boss_damage::get<std::uint32_t>(action,4)==sequence,"captured Dendron sequence group and name");
        check(m::boss_damage::get<std::uint32_t>(action,12)==UINT32_MAX
            && m::boss_damage::get<std::uint32_t>(action,16)==UINT32_MAX,"native absent weak target");
        check(m::boss_damage::get<std::uint64_t>(action,0x68)==0,"no invented request parameters");
    }
    for(const auto gate:{p::kGate1,p::kGate2}) {
        const auto action=p::action(p::kSleep,gate);
        check(action[0x60]==std::byte{0x5E} && m::boss_damage::get<std::uint32_t>(action,8)==gate,"captured native wake-up gate");
    }
    const auto opening=p::action(p::kIntro,p::kIntroExit);
    check(opening[0x60]==std::byte{0x5E} && m::boss_damage::get<std::uint32_t>(opening,8)==0x9CD3EB24U,"opening uses the authored terminal exit gate");
    const auto start=p::action(p::kIntro,p::kIntroStart);
    check(start[0x60]==std::byte{0x5E} && m::boss_damage::get<std::uint32_t>(start,8)==0x3FEE384CU,
        "opening also sends the node-zero start input");
    check(!p::release_opening_hold(true,true,false,true,1000,8999),"intro recovery preserves presentation window");
    check(p::release_opening_hold(true,true,false,true,1000,9000),"stuck native intro gets a bounded stop");
    check(!p::release_opening_hold(true,true,true,true,1000,9000),"intro stop is once only");
    check(!p::release_opening_hold(true,true,false,false,1000,9000),"naturally finished intro needs no stop");
    check(!p::release_opening_hold(false,true,false,true,1000,9000)
        && !p::release_opening_hold(true,false,false,true,1000,9000),"unstarted or unsignaled intro cannot be released");
    check(!p::release_opening_hold(true,true,false,true,1000,999),"backwards clock cannot stop intro early");
    check(p::kBurst==0x80F45BA6U,"user-confirmed burst entity");
    m::BossRequest request{};request.owner={9,2};request.enemy={9,0x28F42027,0x1234,2,3,m::kBossActor.registry};
    request.frame.enabled=request.frame.bossFighting=true;request.frame.region=136;
    request.platform={{9,2},m::kBossPlatform,0x40FAA273,12};
    check(p::wanted(request),"cycle driver accepts current admitted rooftop owner");
    request.frame.lensDestroyed.set(7);
    namespace intro=sunrise::client::hooks::bootflow::strike_bond_intro_release;
    check(intro::wanted(request),"old held intro may release after cube destruction");
    request.frame.bossCycle.openingStarted=true;
    check(!intro::wanted(request),"old intro cleanup cannot cancel the new opening animation");
    auto wrong=request;++wrong.enemy.generation;check(!p::wanted(wrong),"cycle driver rejects stale generation");
    wrong=request;wrong.frame.finished=true;check(!p::wanted(wrong),"cycle driver stops after completion");
    wrong=request;wrong.frame.bossDead=true;check(!p::wanted(wrong),"cycle driver cannot replay death");
    wrong=request;wrong.frame.region=8;check(!p::wanted(wrong),"cycle driver cannot affect route guardians");
}
static void music_regressions() {
    m::Frame f{};f.enabled=true;f.spawnGeneration=1;
    const auto* sensor=m::find(0xEA9691FBU,11,1);
    // Find the authored sensor rather than coupling the check to the group key.
    for(const auto& a:m::kAssets) if(a.asset.type==11) sensor=&a;
    check(sensor,"authored music sensor exists");
    const auto a=sensor->asset;
    check(m::body_bits(f,a.registry,11,a.slot)==0,"unset music is not published");
    check(!m::raise_music(f,m::music_for_region(-1)),"unknown region cannot start music");
    for(const auto region:{120,80,8,136}) {
        check(m::raise_music(f,m::music_for_region(region)),"forward region raises score");
        const auto score=f.musicCandidate;
        check(!m::raise_music(f,m::music_for_region(120)) && f.musicCandidate==score,"earlier region cannot lower score");
        Wire w;check(m::write_body(w,f,a.registry,11,a.slot) && w.bits==7223,"selected music reaches native sensor");
    }
    for(const auto next:{m::kMusicBossIntro,m::kMusicBossSecond,m::kMusicBossThird,m::kMusicBossDead}) {
        check(m::raise_music(f,next),"boss score order advances");
        check(!m::raise_music(f,next),"duplicate score does not republish");
    }
    check(!m::raise_music(f,255),"invalid score rejected");
}
static void forest_endpoint_regressions() {
    const auto request=m::forest_request(12345);
    check(request.seed==12345 && request.selectAnchors,"Forest endpoint override retains layout seed");
    for(const auto& a:request.anchors)
        check(a.column>=0 && a.column<3 && a.height>=0 && a.height<3,"Forest C anchor is admitted by native three-column solver");
    const auto& exit=request.anchors[2];
    check(exit.column==1 && exit.height==1 && exit.progress==1.F,"Forest exit uses measured middle column and lower tier");
    check(request.anchors[0].column==1 && request.anchors[0].height==1
        && request.anchors[1].column==2 && request.anchors[1].height==0
        && request.anchors[3].column==1 && request.anchors[3].height==0,"Forest correction preserves other endpoints");
    Wire wire;check(coo::native_generator::write_activation(wire,request)
        && wire.bits==coo::native_generator::kMinimumBits,"Corrected endpoints reach a complete native activation packet");
    // The first record begins with seed/mode; every endpoint contributes four
    // fields. Verify the north coordinates and unique goal on the wire too.
    check(wire.fields[10]==std::pair<std::uint64_t,unsigned>{129,8}
        && wire.fields[11]==std::pair<std::uint64_t,unsigned>{129,8},"North column and height survive authority serialization");
    for(unsigned i=0;i<4;++i)
        check(wire.fields[4+4*i].first==(i==2?0x3F800000U:0U),"Only the fixed north exit is the maximum-progress goal");
}
static void boss_shield_regressions() {
    namespace shield=sunrise::client::hooks::bootflow::strike_bond_boss_shield;
    check(strike_bond_shield_contracts(),"Dendron shield draw-pass identity and native dispatch contracts");
    m::Frame f{};f.enabled=true;
    check(!shield::visible(f),"combat face effect is absent before the start cube breaks");
    f.bossFighting=true;f.bossCycle.mode=m::BossMode::opening;
    check(shield::visible(f),"cube-triggered opening enables combat face effect");
    for(std::uint8_t stage=0;stage<3;++stage) {
        f.bossStage=stage;f.bossCycle.mode=m::BossMode::damage;
        check(shield::visible(f),"purple face effect stays enabled throughout every damage bar");
        check(!m::boss_blocked(f,m::boss_floor(f)+.1F),"visible face effect does not make Dendron immune");
        check(m::boss_blocked(f,m::boss_floor(f))==(stage<2),"damage floors retain their separate immunity policy");
        if(stage<2) {
            for(const auto mode:{m::BossMode::parking,m::BossMode::dormant}) {
                f.bossCycle.mode=mode;check(shield::visible(f),"combat effect base remains available to native phase visibility owners");
            }
        }
    }
    for(std::uint8_t stage=1;stage<=2;++stage) {
        f.bossStage=stage;f.bossCycle.mode=m::BossMode::waking;
        check(shield::visible(f),"combat face effect remains enabled through both wake-up animations");
    }
    f.bossCycle.mode=m::BossMode::dying;
    check(!shield::visible(f),"death animation retires the combat face effect");
    f.bossCycle.mode=m::BossMode::damage;
    f.bossDead=true;check(!shield::visible(f),"death cannot restore combat face effect");
    f.bossCycle.mode=m::BossMode::dormant;f.bossDead=false;f.ending=true;
    check(!shield::visible(f),"ending cannot restore combat face effect");
    f.ending=false;f.finished=true;check(!shield::visible(f),"completed mission cannot restore combat face effect");
    f.finished=false;f.enabled=false;check(!shield::visible(f),"disabled mission cannot enable combat face effect");
    f.enabled=true;f.bossFighting=false;check(!shield::visible(f),"encounter reset retires combat face effect");
}
static void boss_regressions() {
    for(const auto generation:{0U,0x80000000U,UINT32_MAX}) {
        Wire invalid;check(!coo::native_combatant::write_retire_member(invalid,generation) && invalid.bits==0,"invalid member retirement does not publish partial authority");
    }
    Wire binding;check(coo::native_combatant::write_bind(binding,7) && binding.at(32,2)==0
        && binding.at(37,1)==1,"existing member binding retains its keep-alive defaults");
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
    check(strike_bond_boss_retirement_contracts(),"detached Dendron entity retirement identity contracts");
    check(strike_bond_carriage_contracts(),"Dendron animated plate attachment contracts");
    check(strike_bond_target_binding_contracts(),"Dendron primary target binding contracts");
    check(strike_bond_intro_release_contracts(),"Dendron named intro release contracts");
    check(strike_bond_fire_trace_contracts(),"Dendron firing trace ownership contracts");
    boss_shield_regressions();forest_endpoint_regressions();parking_route_regressions();cycle_boundary_regressions();cycle_command_regressions();catalogue();native_regressions();cover_regressions();mount_regressions();music_regressions();boss_regressions();check(strike_bond_boss_damage_contracts(),"Dendron native damage identity contracts");{
        auto replay=std::make_unique<Replay>();replay->run_all();
        replay=std::make_unique<Replay>();replay->run_all(true);
        replay=std::make_unique<Replay>(true);replay->run_all();
        replay=std::make_unique<Replay>(false,true);replay->run_all();
    }
    std::printf("PASS: %u Garden World route, ownership, shield, native death, dialogue and launcher checks\n",checks);
}
