#include "../src/state/activity/hijacked/controller.h"
#include "../src/state/activity/hijacked/backtracking_identity.h"
#include "../src/state/activity/hijacked/scan_playback.h"
#include "../src/client/hooks/bootflow/hijacked_presentation.h"
#include "../src/state/activity/hijacked/plate_presentation.h"
#include "../src/state/activity/hijacked/boss_motion.h"
#include "../src/state/activity/hijacked/boss_damage.h"
#include "../src/state/activity/hijacked/route_geometry.h"
#include "hijacked_lifetime_tests.h"
#include "hijacked_boss_damage_tests.h"
#include "../src/server/bap/region_lineage.h"
#include "../src/server/bap/encrypted/activity_message/membership/activity_membership_route.h"
#include "../src/state/activity/membership/transactions/internal.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <limits>
namespace ds=sunrise::state::activity::hijacked;
namespace coo=sunrise::state::activity::coo;
static void check(bool ok,const char* what) {if(!ok) {std::fprintf(stderr,"FAIL: %s\n",what);std::exit(1);}}
static ds::Point point(const ds::Volume& v) {
    for(unsigned x=1;x<40;++x) for(unsigned y=1;y<40;++y) {
        ds::Point p{v.min.x+(v.max.x-v.min.x)*float(x)/40,v.min.y+(v.max.y-v.min.y)*float(y)/40,(v.min.z+v.max.z)/2};
        if(ds::contains(v,p)) {return p;}
    }std::abort();
}
static void enter(ds::Controller& c,std::uint64_t run,coo::Asset a) {
    for(const auto& v:ds::kVolumes) {if(v.asset==a) {c.position(run,point(v));return;}}std::abort();
}
static std::unique_ptr<coo::script::MissionDocument> parse(std::string_view text) {
    std::string error;auto doc=coo::script::MissionDocument::parse_lua(text,ds::kProfile,error);
    if(!doc) {std::fprintf(stderr,"Lua: %s\n",error.c_str());}return doc;
}
static std::string shipped() {
    std::ifstream file(std::filesystem::path(__FILE__).parent_path().parent_path()/"scripts"/"hijacked.lua");
    std::ostringstream text;text<<file.rdbuf();check(file.good() || file.eof(),"read shipped Lua");return text.str();
}
static void clock_ownership() {
    namespace bap=sunrise::server::bap;
    using sunrise::state::activity::ActivityInstanceKey;
    const ActivityInstanceKey primary{0x9EAA300100200001ULL,{1}};
    const ActivityInstanceKey auxiliary{0x9EAA300100200002ULL,{1}};
    const bap::RegionLineage owned{primary,primary,bap::RegionLineageKind::ownedActivity};
    const bap::RegionLineage borrowed{auxiliary,primary,bap::RegionLineageKind::groupDerivedBorrow};
    check(owned.owns(primary),"primary retains clock authority after a newer auxiliary activity joins");
    check(!owned.owns(auxiliary) && !borrowed.owns(auxiliary) && !borrowed.owns(primary),
        "auxiliary binding cannot publish the primary mission clock");
    check(!owned.owns({primary.sessionId,{2}}),"reused session ID cannot authorize a stale clock incarnation");
    check(!bap::RegionLineage{primary,auxiliary,bap::RegionLineageKind::ownedActivity}.owns(primary),
        "mismatched creator lineage cannot publish a clock");
    check(!bap::RegionLineage{}.owns({}),"empty lineage cannot publish a clock");
}
static void native_contracts() {
    ds::ScanPlayback scan{7,2,1,.2F,8.F};check(scan.started(7,true),"native eight-second scan starts with participant");
    check(!scan.started(8,true) && !scan.started(7,false),"stale scan and missing Ghost rejected");
    scan.active=0;scan.elapsed=3.F;check(!scan.finished(7,true),"base three seconds cannot finish overridden scan");
    scan.elapsed=8.01F;check(scan.finished(7,true) && !scan.finished(7,false),"overshoot accepted only after authentic start");
    scan.duration=std::numeric_limits<float>::quiet_NaN();check(!scan.valid(7),"nonfinite scan duration rejected");
    for(std::uint8_t stage=0;stage<3;++stage) {
        auto request=ds::boss_motion::request(stage);check(request.has_value(),"three native teleport destinations");
        check((*request)[0]==std::byte{0} && (*request)[4]==std::byte{6} && (*request)[0x60]==std::byte{0x45},"Hydra group zero sequence six native teleport opcode");
        check(ds::boss_motion::arrived(stage,ds::boss_motion::kDestinations[stage]),"exact native pose accepted");
        auto bad=ds::boss_motion::kDestinations[stage];bad[0]+=2.F;check(!ds::boss_motion::arrived(stage,bad),"requested destination is not proof of native arrival");
        bad[0]=std::numeric_limits<float>::quiet_NaN();check(!ds::boss_motion::arrived(stage,bad),"invalid world pose rejected");
    }
    check(!ds::boss_motion::request(3) && !ds::boss_motion::arrived(3,{}),"unknown movement stage rejected");
    // The final live destination was adjusted by native projection/height handling.
    // It is over six metres from the authored point, but is the actual arrival.
    namespace motion=ds::boss_motion;
    const std::array<float,4> previous{400.7519F,359.8733F,-95.9226F,1.F};
    motion::Landing landed{{472.2383F,328.2527F,-94.9722F,1.F},6,false};
    const std::array<float,3> actual{472.2383F,328.2527F,-94.9722F};
    check(!motion::arrived(2,actual) && motion::teleport_arrived(true,false,previous,landed,actual),
        "native adjusted landing unlocks despite being outside the authored-point radius");
    check(!motion::teleport_arrived(false,true,previous,landed,actual),"unissued motion cannot acknowledge arrival");
    check(!motion::teleport_arrived(true,false,landed.destination,landed,actual),"unchanged inactive selector cannot acknowledge new motion");
    check(motion::teleport_arrived(true,true,landed.destination,landed,actual),"observed activation permits a repeated destination after settling");
    check(!motion::teleport_arrived(true,false,previous,landed,motion::kDestinations[1]),"resolved endpoint without actual arrival cannot complete motion");
    auto invalid=landed;invalid.active=true;
    check(!motion::teleport_arrived(true,true,previous,invalid,actual),"active teleport stays immune even at its destination");
    invalid=landed;invalid.sequence=5;
    check(!motion::teleport_arrived(true,true,previous,invalid,actual),"other selector sequence cannot acknowledge Hydra teleport");
    invalid=landed;invalid.destination[3]=0.F;
    check(!motion::teleport_arrived(true,true,previous,invalid,actual),"invalid native destination w rejected");
    for(unsigned i=0;i<4;++i) {
        invalid=landed;invalid.destination[i]=std::numeric_limits<float>::quiet_NaN();
        check(!motion::teleport_arrived(true,true,previous,invalid,actual),"nonfinite resolved destination rejected");
        auto old=previous;old[i]=std::numeric_limits<float>::quiet_NaN();
        check(!motion::teleport_arrived(true,true,old,landed,actual),"nonfinite prior endpoint rejected");
        if(i<3) {auto pose=actual;pose[i]=std::numeric_limits<float>::quiet_NaN();
            check(!motion::teleport_arrived(true,true,previous,landed,pose),"nonfinite actual world pose rejected");}
    }
    motion::LandingEvidence evidence{previous};
    const motion::Landing oldLanding{previous,6,false};
    motion::retain_landing(evidence,oldLanding);
    check(!motion::teleport_arrived(true,evidence,oldLanding,actual),"old inactive endpoint does not acknowledge a new request");
    // Replay both captured failures: the native endpoint changes after the first
    // observation, with no observed active frame. Neither may poison arrival.
    const std::array<motion::Landing,2> provisional{{
        {{400.3701171875F,359.076965332F,-95.9198379517F,1.F},6,false},
        {{466.3000183105F,331.3999938965F,-94.5770263672F,1.F},6,false}}};
    const std::array<motion::Landing,2> corrected{{
        {{400.7518615723F,359.1269226074F,-95.9202880859F,1.F},6,false},
        {{472.2383117676F,328.2527465820F,-94.9722061157F,1.F},6,false}}};
    const std::array<std::array<float,4>,2> origins{{
        {283.8002929688F,307.6007080078F,-91.9804840088F,1.F},corrected[0].destination}};
    for(unsigned phase=0;phase<2;++phase) {
        evidence={origins[phase]};motion::retain_landing(evidence,provisional[phase]);
        motion::retain_landing(evidence,corrected[phase]);
        const auto& endpoint=corrected[phase].destination;
        std::array<float,3> pose{endpoint[0],endpoint[1],endpoint[2]};
        check(motion::teleport_arrived(true,evidence,corrected[phase],pose),"captured native endpoint correction releases retreat immunity");
        pose[0]+=14.F;
        check(motion::teleport_arrived(true,evidence,corrected[phase],pose),"generous arrival allowance accepts Hydra landing drift");
        pose[0]+=2.F;
        check(!motion::teleport_arrived(true,evidence,corrected[phase],pose),"a boss outside the destination area is still pending");
        auto transient=corrected[phase];transient.sequence=3;motion::retain_landing(evidence,transient);
        check(!motion::teleport_arrived(true,evidence,transient,pose),"different current native sequence does not release immunity");
        pose={endpoint[0],endpoint[1],endpoint[2]};motion::retain_landing(evidence,corrected[phase]);
        check(motion::teleport_arrived(true,evidence,corrected[phase],pose),"transient selector changes cannot permanently poison arrival");
        auto moving=corrected[phase];moving.active=true;motion::retain_landing(evidence,moving);
        check(!motion::teleport_arrived(true,evidence,moving,pose),"loose spatial check still waits for the teleport to stop");
    }
    std::array<std::byte,0xC0> selector{};
    selector[0x90]=std::byte{1};const std::uint32_t sequence=6;
    std::memcpy(selector.data()+0x94,&sequence,sizeof sequence);
    std::memcpy(selector.data()+0xB0,landed.destination.data(),sizeof landed.destination);
    const auto decoded=motion::landing(selector);
    check(decoded.active && decoded.sequence==6 && decoded.destination==landed.destination,"selector fields decode at native offsets");
    check(!motion::teleport_arrived(true,true,previous,motion::landing(std::span(selector).first(0xBF)),actual),"truncated selector cannot acknowledge arrival");
    ds::PlateState plate{};plate.occupied=true;check(ds::plate_presentation::position(plate)==.2F,"occupied unarmed plate remains red");
    plate.armed=true;check(ds::plate_presentation::position(plate)==.1F,"arming occupied plate starts native charge pose");
    plate.charged=true;plate.occupied=false;check(ds::plate_presentation::position(plate)==.1F,"completed plate retains presentation after leaving");
}
struct Replay {
    ds::Controller c;const coo::script::Views& views;static constexpr std::uint64_t run=710;
    std::uint64_t now{1000},endingEnd{};bool earlyDeath{},suppressInitialArrival{},heldEnding{},heldBoss{},plateChecked{},scanChecked{};
    std::array<std::vector<ds::EnemyReceipt>,std::size(ds::kSpawns)> enemies{};
    std::array<unsigned,3> moves{};std::vector<unsigned> dialogue;unsigned healthWait{},bossWait{},optionalSurvivors{};
    explicit Replay(const coo::script::Views& v,bool early,bool suppress=false):views(v),earlyDeath(early),suppressInitialArrival(suppress) {
        check(c.select(v,run),"select current mission");
        check(!c.update(run,now,false).enabled,"loading does not publish opening");
        check(c.update(run,now,true).enabled,"confirmed arrival starts without a position sample");
    }
    void serve() {
        auto frame=c.frame();
        for(std::size_t i=0;i<std::size(ds::kAssets);++i) {
            const auto a=ds::kAssets[i].asset;const auto state=c.frame().native[i];if(a.type!=4 || !state.managed || !state.desired) {continue;}
            if(!state.prepared) {check(c.prepared(c.owner(),a),"owned source preparation");continue;}
            if(!state.acknowledged) {check(c.object({{run,state.generation},a,static_cast<std::uint32_t>(100+i),static_cast<std::uint32_t>(500+i)}),"owned native object acknowledged");}
        }
        for(std::size_t i=0;i<enemies.size();++i) {
            const auto& spawn=ds::kSpawns[i];const auto a=ds::find(spawn.registry,1,spawn.source)->asset;
            if(!c.frame().native[ds::asset_index(a)].active || !enemies[i].empty()) {continue;}
            if(!c.frame().native[ds::asset_index(a)].prepared) {
                auto stale=c.owner();++stale.value;check(!c.prepared(stale,a),"stale placement completion rejected");
                const ds::EnemyReceipt premature{run,static_cast<std::uint32_t>(1000+i*16),static_cast<std::uint32_t>(3000+i),frame.spawnGeneration,spawn.source,spawn.registry};
                check(!c.admitted(premature),"enemy admission cannot precede authored placement readiness");
                check(c.prepared(c.owner(),a) && !c.prepared(c.owner(),a),"authentic placement readiness retained once");
            }
            for(unsigned n=0;n<spawn.count;++n) {
                ds::EnemyReceipt r{run,static_cast<std::uint32_t>(1000+i*16+n),static_cast<std::uint32_t>(3000+i),frame.spawnGeneration,spawn.source,spawn.registry};
                auto wrong=r;++wrong.generation;check(!c.admitted(wrong) && !c.died(r),"stale admission and death without creation rejected");
                check(c.admitted(r) && !c.admitted(r),"admission deduplicated");enemies[i].push_back(r);
                const auto t=a==ds::kBoss?ds::kBossTactics[0]:spawn.tactical;
                check(c.readiness(r,{true,true,true,true,r.actor+9000,t.registry,t.slot,t.row}),"native health AI and tactical evidence");
            }
        }
        auto boss=c.boss_request();
        if(boss.requested && !(suppressInitialArrival && boss.stage==0)) {
            heldBoss=true;check(!c.boss_position(boss.enemy,boss.stage,boss.revision+1),"stale movement revision rejected");
            if(moves[boss.stage]++>=2) {check(c.boss_position(boss.enemy,boss.stage,boss.revision),"authentic native boss destination");check(!c.boss_position(boss.enemy,boss.stage,boss.revision),"duplicate move receipt rejected");}
        }
        if(earlyDeath && c.frame().section==2 && c.frame().bossPositioned) {for(const auto& r:enemies[ds::spawn_index(ds::kBoss)]) {static_cast<void>(c.died(r));}}
        const auto plateSource=c.frame().native[ds::asset_index(ds::kPlates[0].source)];
        if(plateSource.acknowledged && !c.plate_request(0).plate.valid()) {
            ds::PlateReceipt r{{run,plateSource.generation},0x20000,0,4100,4200,4300,4400};
            check(c.bind_plate(r),"owned plate timer bound");
            {auto wrongPoseOwner=r;++wrongPoseOwner.owner.value;
             check(!c.plate_pose(wrongPoseOwner,{0.F,0.F,INT32_MAX,INT32_MAX}),"pose rejects retired source generation");
             wrongPoseOwner=r;++wrongPoseOwner.serial;
             check(!c.plate_pose(wrongPoseOwner,{0.F,0.F,INT32_MAX,INT32_MAX}),"pose rejects recycled entity identity");
             wrongPoseOwner=r;++wrongPoseOwner.device;
             check(!c.plate_pose(wrongPoseOwner,{0.F,0.F,INT32_MAX,INT32_MAX}),"pose rejects another device component");
             wrongPoseOwner=r;++wrongPoseOwner.owner.run;
             check(!c.plate_pose(wrongPoseOwner,{0.F,0.F,INT32_MAX,INT32_MAX}),"pose rejects retired mission run");
             wrongPoseOwner=r;++wrongPoseOwner.source;
             check(!c.plate_pose(wrongPoseOwner,{0.F,0.F,INT32_MAX,INT32_MAX}),"pose rejects another source address");
             wrongPoseOwner=r;++wrongPoseOwner.timer;
             check(!c.plate_pose(wrongPoseOwner,{0.F,0.F,INT32_MAX,INT32_MAX}),"pose rejects another native timer");
             check(c.plate_pose(r,{0.F,0.F,700,900}),"server accepts exact native pose receipt");}
enter(c,run,ds::kPlates[0].volume);
            check(!c.plate(r,c.frame().plates[0].revision,1.F,true),"unarmed or unstarted plate cannot complete");plateChecked=true;
        }
        const auto scanSource=c.frame().native[ds::asset_index(ds::kScans[0].source)];
        if(scanSource.acknowledged && !c.scan_request(0).scan.valid()) {
            ds::ScanReceipt r{{run,scanSource.generation},0x30000,0,5100,5200,5300};
            check(c.bind_scan(r) && !c.scan(r,false,true),"scan completion requires retained start");scanChecked=true;
        }
        frame=c.frame();
        if(frame.activeRow!=coo::kNoDialogue && (dialogue.empty() || dialogue.back()!=frame.activeRow)) {
            const auto row=frame.activeRow;check(c.submitted(run,ds::kBank,row,frame.generations[row],now),"native dialogue submission accepted");dialogue.push_back(row);
            if(row==13) {endingEnd=now+28080;}
        }
    }
    void fulfill(const coo::CommandSpec& s) {
        if(const auto* group=views.condition(s)) {
            static_cast<void>(group->evaluate([&](const coo::CommandSpec& child) {fulfill(child);return true;}));return;
        }
        if(s.asset==ds::kModule && s.argument==30) {c.position(run,{111.1F,232.5F,-80.7F});return;}
        if(s.asset==ds::kDialogueAsset) {return;}
        if(s.asset.type==60) {enter(c,run,s.asset);return;}
        if(s.asset==ds::kBoss && s.argument>=100) {
            const auto boss=c.boss_request();if(!boss.enemy.valid()) {return;}
            if(healthWait++%4!=3) {check(!c.frame().native[ds::asset_index(ds::find(0x153E22CDU,23,29)->asset)].desired || c.frame().section==2,"health wait preserves boss barrier");return;}
            auto wrong=boss.enemy;++wrong.owner;check(!c.health(wrong,.1F),"foreign boss health rejected");
            check(!c.health(boss.enemy,std::numeric_limits<float>::quiet_NaN()),"invalid native health rejected");
            static_cast<void>(c.health(boss.enemy,ds::boss_damage::floor(static_cast<std::uint8_t>(s.argument-100))));return;
        }
        if(s.asset.type==1) {
            const auto i=ds::spawn_index(s.asset);if(enemies[i].empty()) {return;}
            if(s.asset==ds::kBoss && !earlyDeath && bossWait++<3) {
                check(c.frame().native[ds::asset_index(ds::find(0x153E22CDU,23,29)->asset)].desired,"barrier stays closed until real Mind death");return;
            }
            for(const auto& r:enemies[i]) {auto wrong=r;++wrong.owner;check(!c.died(wrong),"foreign death cannot clear source");static_cast<void>(c.died(r));}return;
        }
        if(s.asset==ds::kPlates[0].source) {
            auto q=c.plate_request(0);if(!q.plate.valid()) {return;}enter(c,run,ds::kPlates[0].volume);q=c.plate_request(0);
            check(!c.plate(q.plate,q.state.revision+1,.5F,false),"stale plate revision rejected");
            static_cast<void>(c.plate(q.plate,q.state.revision,.5F,false));
            const auto interrupted=q.state.revision;c.position(run,{});enter(c,run,ds::kPlates[0].volume);q=c.plate_request(0);
            check(q.state.revision!=interrupted && !c.plate(q.plate,interrupted,1.F,true),"departure rejects the old charge completion");
            check(!c.plate(q.plate,q.state.revision,1.F,true),"reentry needs a fresh charging sample");
            static_cast<void>(c.plate(q.plate,q.state.revision,.5F,false));
            check(c.contested(q.plate,true),"native enemy contest interrupts charge");
            q=c.plate_request(0);check(!c.plate(q.plate,q.state.revision,1.F,true),"contested charge cannot complete");
            check(c.contested(q.plate,false),"native clearance releases contest");q=c.plate_request(0);
            check(!c.plate(q.plate,q.state.revision,1.F,true),"contest clearance needs a fresh charging sample");
            static_cast<void>(c.plate(q.plate,q.state.revision,.5F,false));
            check(c.plate(q.plate,q.state.revision,1.F,true),"native timer completion retained");
            c.position(run,{});check(c.frame().plates[0].charged,"charge survives departure");return;
        }
        if(s.asset==ds::kScans[0].source) {
            const auto q=c.scan_request(0);if(!q.scan.valid()) {return;}
            auto wrong=q.scan;++wrong.controller;check(!c.scan(wrong,true,false),"foreign Ghost controller rejected");
            if(s.argument==11) {static_cast<void>(c.scan(q.scan,true,false));}
            else {check(c.frame().scanStarted[0],"finish follows authentic start");static_cast<void>(c.scan(q.scan,false,true));}return;
        }
    }
    void run_all() {
        for(unsigned tick=0;tick<4000 && !c.frame().finished;++tick) {
            auto f=c.update(run,now,true);serve();const auto* graph=c.graph();
            for(const auto& b:graph->commands) {if(c.step_state(b.step).phase!=coo::StepPhase::active) {continue;}
                const auto& s=graph->definition.steps[b.step].commands[b.command];if(coo::is_observation(s.operation)) {fulfill(s);}}
            f=c.update(run,now,true);
            check(f.enabled,"replay does not fail executor");
            check(!f.native[ds::asset_index(ds::find(0xD997395EU,4,18)->asset)].desired,"spare block05 stays absent through the full mission and ending");
            if(endingEnd && now<endingEnd) {check(!f.finished,"full final exchange gates success");heldEnding=true;}
            now+=100;
        }
        if(!c.frame().finished) {std::fprintf(stderr,"stalled section=%u active=%08X\n",c.frame().section,c.diagnostics().active);}
        check(c.frame().finished && c.frame().completion.valid(),"complete shipped route");
        check(heldEnding && heldBoss && plateChecked && scanChecked,"route exercised native gates");
        const std::vector<unsigned> expected{0,4,2,5,6,9,10,11,12,13};
        // A synthetic instant chase can finish teleport2 before the independent
        // ten-second identification cue; arrival dialogue must not wait for it.
        const std::vector<unsigned> fastRetreat{0,4,2,6,5,9,10,11,12,13};
        if(dialogue!=expected && dialogue!=fastRetreat) {
            std::fprintf(stderr,"dialogue order:");for(const auto row:dialogue) {std::fprintf(stderr," %u",row);}std::fprintf(stderr,"\n");
        }
        check(dialogue==expected || dialogue==fastRetreat,"full deduplicated dialogue follows room-entry and teleport cues");
        c.living_enemies([&](const auto&) {++optionalSurvivors;});check(optionalSurvivors>0,"traversal enemies may survive mission completion");
        const auto stale=c.scan_request(0).scan;const auto old=c.owner();c.reset();check(c.select(views,run),"same run reselect allocates new owner");
        check(c.owner()!=old && !c.scan(stale,false,true),"reset rejects old native receipt");
    }
};

static bool dialogue_requested(const ds::Controller& c,unsigned row) {
    const auto* g=c.graph();
    for(const auto& b:g->commands) {
        const auto& q=g->definition.steps[b.step].commands[b.command];
        if(q.operation==coo::Operation::dialogue && q.argument==row) {
            return c.step_state(b.step).phase==coo::StepPhase::complete;
        }
    }
    return false;
}
static void opening_tunnel_bypass(const coo::script::Views& views) {
    Replay r(views,false);auto& c=r.c;
    const auto source=[](std::uint32_t registry,std::uint16_t slot) {return ds::find(registry,1,slot)->asset;};
    const auto travel=[&](std::uint32_t registry,std::uint16_t slot) {
        for(const auto& v:ds::kVolumes) {if(v.asset.registry==registry && v.asset.slot==slot) {enter(c,Replay::run,v.asset);return;}}std::abort();
    };
    const auto tick=[&] {
        for(unsigned n=0;n<20;++n) {
            static_cast<void>(c.update(Replay::run,r.now,true));r.serve();r.now+=100;
            check(c.frame().enabled,"remote Harpy fixture remains enabled");
            for(std::uint16_t slot=1;slot<=7;++slot) {
                check(!c.frame().native[ds::asset_index(source(0x3E9B74F3U,slot))].retired,
                    "all exterior sources remain available through approach and backtracking");
            }
        }
    };
    const auto desired=[&](std::uint16_t slot) {return c.frame().native[ds::asset_index(source(0x153E22CDU,slot))].desired;};
    const auto living=[&](std::uint16_t slot) {
        unsigned count{};c.living_enemies([&](const auto& receipt) {
            if(receipt.registry==0x3E9B74F3U && receipt.source==slot) {++count;}
        });return count;
    };
    tick();
    for(const auto slot:std::array<std::uint16_t,3>{1,2,6}) {
        check(c.frame().native[ds::asset_index(source(0x3E9B74F3U,slot))].desired,"outside Mists Harpies request at mission load before patrol approach");
    }
    for(const std::uint16_t slot:std::array<std::uint16_t,4>{3,4,5,7}) {
        const auto a=source(0x3E9B74F3U,slot);
        check(!c.frame().native[ds::asset_index(a)].desired && r.enemies[ds::spawn_index(a)].empty(),
            "initial Harpies do not pull Goblins or ledge Hobgoblins forward before patrol approach");
    }
    travel(0x3E9B74F3U,35);tick();
    for(const std::uint16_t slot:std::array<std::uint16_t,5>{1,2,5,6,7}) {
        check(c.frame().native[ds::asset_index(source(0x3E9B74F3U,slot))].desired && living(slot)>0,
            "patrol approach adds ledge Hobgoblins and the original upper Goblin squad to waiting Harpies");
    }
    check(living(5)==2 && living(7)==3,"upper patrol has two ledge Hobgoblins and three Goblins");
    for(const std::uint16_t slot:std::array<std::uint16_t,2>{3,4}) {
        check(!c.frame().native[ds::asset_index(source(0x3E9B74F3U,slot))].desired && living(slot)==0,
            "lower Goblin patrols wait for their original drop trigger");
    }
    travel(0xF5737F85U,13);tick();
    constexpr std::array<unsigned,7> exteriorCounts{1,1,3,3,2,3,3};unsigned exteriorTotal{};
    for(std::uint16_t slot=1;slot<=7;++slot) {
        check(c.frame().native[ds::asset_index(source(0x3E9B74F3U,slot))].desired
            && living(slot)==exteriorCounts[slot-1],"drop trigger activates both lower Goblin groups without requiring kills");
        exteriorTotal+=living(slot);
    }
    check(exteriorTotal==16 && living(5)==2,"full exterior encounter contains sixteen enemies including two ledge Hobgoblins");
    check(c.frame().objective==ds::kObjectives[0].event,"approach and lower bowl do not update the tunnel objective");
    check(!dialogue_requested(c,2),"patrol dialogue waits for tunnel objective");
    for(const auto slot:std::array<std::uint16_t,7>{1,2,3,4,5,6,7}) {
        check(living(slot)==ds::kSpawns[ds::spawn_index(source(0x3E9B74F3U,slot))].count,"exterior squads remain alive before tunnel mouth");
    }
    travel(0xF5737F85U,15);
    static_cast<void>(c.update(Replay::run,r.now,true));
    check(c.frame().objective==ds::kObjectives[1].event && dialogue_requested(c,2),"tunnel objective and patrol dialogue request in the same update");
    tick();
    check(c.frame().objective==ds::kObjectives[1].event,"tunnel mouth immediately updates objective before Mists load-zone entry");
    check(!desired(1) && !desired(2),"early objective does not pull forward cave entrance spawns");
    for(const std::uint16_t slot:std::array<std::uint16_t,7>{1,2,3,4,5,6,7}) {
        const auto& state=c.frame().native[ds::asset_index(source(0x3E9B74F3U,slot))];
        check(!state.retired && state.active && state.desired && living(slot)>0,"tunnel mouth keeps exterior enemies fightable before the Mists load zone");
    }
    // Turning back before intro136 must keep combat and authentic death receipts working.
    travel(0x3E9B74F3U,35);tick();
    const auto outside=source(0x3E9B74F3U,6);
    const auto killed=r.enemies[ds::spawn_index(outside)].front();
    check(c.died(killed),"exterior enemy can still be killed after entering and leaving the tunnel mouth");
    check(living(6)==2,"turning back before the load zone preserves the remaining exterior enemies");
    travel(0x153E22CDU,136);tick();
    check(c.frame().section==1 && desired(1) && desired(2),"Mists entry advances without clearing the exterior squads");
    for(const std::uint16_t slot:std::array<std::uint16_t,7>{1,2,3,4,5,6,7}) {
        const auto& state=c.frame().native[ds::asset_index(source(0x3E9B74F3U,slot))];
        check(state.active && state.desired && !state.retired && state.generation==c.frame().spawnGeneration,
            "Mists entry preserves exterior source intent until a confirmed streaming handoff");
    }
    check(living(6)==2 && !c.died(killed),"Mists entry preserves survivors and genuine terminal death");
    travel(0x3E9B74F3U,35);tick();
    check(living(6)==2,"ordinary backtracking does not recreate killed exterior slots");
    // A missed approach/drop sample cannot strand a player who reaches the tunnel.
    Replay skipped(views,false);
    enter(skipped.c,Replay::run,coo::Asset{0xF5737F85U,0x80B4232CU,60,5});
    for(unsigned n=0;n<20;++n) {static_cast<void>(skipped.c.update(Replay::run,skipped.now,true));skipped.serve();skipped.now+=100;}
    check(skipped.c.frame().objective==ds::kObjectives[1].event,"authored tunnel endpoint also releases objective without approach samples");
    enter(skipped.c,Replay::run,coo::Asset{0x153E22CDU,0x80B42182U,60,136});
    for(unsigned n=0;n<20;++n) {static_cast<void>(skipped.c.update(Replay::run,skipped.now,true));skipped.serve();skipped.now+=100;}
    check(skipped.c.frame().section==1,"tunnel fallback also releases opening travel dependencies without kills");
}
static void cleanup_at_user_location(const coo::script::Views& views) {
    Replay r(views,false);auto& c=r.c;
    const auto source=[](std::uint32_t registry,std::uint16_t slot) {return ds::find(registry,1,slot)->asset;};
    const auto tick=[&] {for(unsigned n=0;n<20;++n) {static_cast<void>(c.update(Replay::run,r.now,true));r.serve();r.now+=100;}};
    const ds::Point userPoint{111.1F,232.5F,-80.7F};
    check(ds::exterior_cleanup_contains(userPoint),"user screenshot position lies inside cleanup region");
    check(!ds::exterior_cleanup_contains({111.1F,228.F,-80.7F}),"earlier endpoint remains outside cleanup region");
    check(!ds::exterior_cleanup_contains({111.1F,232.5F,-40.F}),"surface ledge above tunnel cannot trigger cleanup");
    check(!ds::exterior_cleanup_contains({std::numeric_limits<float>::quiet_NaN(),232.5F,-80.7F}),"invalid pose cannot trigger cleanup");
    enter(c,Replay::run,{0xF5737F85U,0x80B4232CU,60,15});tick();
    check(!c.cleanup_entered(),"tunnel mouth does not latch later cleanup point");
    c.position(Replay::run+1,userPoint);tick();
    check(!c.cleanup_entered(),"stale run cannot latch cleanup point");
    for(const auto slot:std::array<std::uint16_t,7>{1,2,3,4,5,6,7}) {check(!c.frame().native[ds::asset_index(source(0x3E9B74F3U,slot))].retired,"enabled exterior squads stay alive before cleanup point");}
    c.position(Replay::run,userPoint);
    c.position(Replay::run,{111.1F,228.F,-80.7F});tick();
    check(c.cleanup_entered() && c.frame().section==0,"brief visit latches cleanup without advancing cave entry");
    for(const std::uint16_t slot:std::array<std::uint16_t,7>{1,2,3,4,5,6,7}) {check(!c.frame().native[ds::asset_index(source(0x3E9B74F3U,slot))].retired,"old cleanup point preserves exterior survivors for backtracking");}
    check(!c.frame().native[ds::asset_index(source(0x153E22CDU,1))].desired,"cleanup does not spawn cave entrance early");
    enter(c,Replay::run,{0x153E22CDU,0x80B42182U,60,136});tick();
    check(c.frame().section==1,"original intro observation still advances into the cave independently");
    c.reset();check(!c.cleanup_entered(),"cleanup observation clears with mission run");
}
static void native_survivor_hierarchy() {
    // Captured E64D8 root36 owns child35's separate weapon entity and child37's
    // component. Native ownership is the salted parent link, not entity equality.
    const ds::BacktrackingPart root{36,0x24U,0x1bfba024U,UINT32_MAX};
    const auto none=[](std::uint32_t,std::uint32_t&) {return false;};
    check(ds::native_ancestry(root.facet,root.facet,none)==ds::NativeAncestry::descendant,"weapon attachment is owned by exact native root");
    check(ds::native_ancestry(root.facet,UINT32_MAX,none)==ds::NativeAncestry::unrelated,"independent native root stays outside removal");
    check(ds::native_ancestry(root.facet,root.facet+0x2000,none)==ds::NativeAncestry::invalid,"reused parent slot with new salt cannot authorize removal");
    const auto nested=[&](std::uint32_t id,std::uint32_t& parent) {if(id!=17) {return false;}parent=root.facet;return true;};
    check(ds::native_ancestry(root.facet,17,nested)==ds::NativeAncestry::descendant,"nested native attachment retains root ownership");
    const auto cycle=[](std::uint32_t id,std::uint32_t& parent) {parent=id==17?18U:17U;return true;};
    check(ds::native_ancestry(root.facet,17,cycle)==ds::NativeAncestry::invalid,"cyclic malformed native hierarchy fails closed");
    check(ds::native_part_matches(root,0,-1,root.facet,root.net,root.parent)
        && ds::native_part_matches(root,0,-2,root.facet,root.net,root.parent),"local and detached lifetimes retain exact facet identity");
    check(!ds::native_part_matches(root,0,0,root.facet,root.net,root.parent),"foreign peer ownership cannot authorize removal");
    check(!ds::native_part_matches(root,2,-2,root.facet,root.net,root.parent),"player broadcast kind cannot authorize removal");
    check(!ds::native_part_matches(root,0,-2,root.facet+0x2000,root.net,root.parent),"reused facet salt cannot authorize removal");
    check(!ds::native_part_matches(root,0,-2,root.facet,root.net+0x2000,root.parent),"rebound net identity cannot authorize removal");
}
static void exterior_streaming_survivors(const coo::script::Views& views) {
    Replay r(views,false);auto& c=r.c;
    for(unsigned n=0;n<20;++n) {static_cast<void>(c.update(Replay::run,r.now,true));r.serve();r.now+=100;}
    const auto asset=ds::find(0x3E9B74F3U,1,6)->asset;
    const auto index=ds::spawn_index(asset);const auto initial=r.enemies[index];
    check(initial.size()==3,"streaming fixture has three authentic native admissions");
    check(c.died(initial[0]),"genuine pre-streaming kill is recorded");
    std::array<ds::EnemyReceipt,2> survivors{initial[1],initial[2]};
    check(!c.suspend_exterior({Replay::run+1,c.owner().value},6,survivors),"stale run cannot suspend native ownership");
    check(!c.suspend_exterior(c.owner(),6,std::span(survivors).first(1)),"partial source handoff is rejected");
    auto deadSet=survivors;deadSet[0]=initial[0];
    check(!c.suspend_exterior(c.owner(),6,deadSet),"dead slot cannot be presented as a survivor");
    check(c.suspend_exterior(c.owner(),6,survivors),"complete authenticated survivor source suspends");
    auto state=c.frame().native[ds::asset_index(asset)];
    check(state.suspended && !state.active && !state.prepared && !state.retired && state.desired
        && state.survivingRequested==2 && state.generation==2,"suspension requests zero until native source returns");
    check(!c.admitted(initial[1]) && !c.died(initial[1]),"unloaded identity cannot generate receipts");
    check(!c.prepared(c.owner(),asset),"late placement receipt cannot prepare a suspended source");
    check(!c.suspend_exterior(c.owner(),6,survivors),"same unload cannot increment a source twice");
    check(c.resume_exterior(c.owner(),6),"verified new native source can resume");
    auto replacement=initial[1];replacement.actor+=0x2000;replacement.owner+=0x2000;replacement.generation=2;
    check(!c.admitted(replacement),"new source waits for real placement readiness");
    check(c.prepared(c.owner(),asset),"return placement readiness opens survivor request");
    check(c.admitted(replacement),"native admission rebinds one living predecessor");
    check(!c.admitted(replacement),"duplicate admission cannot consume another survivor slot");
    auto second=initial[2];second.actor+=0x2000;second.owner=replacement.owner;second.generation=2;
    check(c.admitted(second),"second genuine survivor rebinds");
    auto excess=second;excess.actor+=0x4000;
    check(!c.admitted(excess),"killed slot cannot become a third actor");
    check(c.died(replacement),"reconstructed native actor remains killable");
    check(!c.died(initial[1]),"old predecessor cannot supply a new actor death");
    std::array<ds::EnemyReceipt,1> last{second};
    check(c.suspend_exterior(c.owner(),6,last),"second roundtrip preserves the remaining survivor");
    check(c.frame().native[ds::asset_index(asset)].survivingRequested==1,"second return omits both genuine kills");
    check(c.resume_exterior(c.owner(),6) && c.prepared(c.owner(),asset),"second native return prepares");
    auto third=second;third.actor+=0x2000;third.owner+=0x2000;third.generation=3;
    check(c.admitted(third) && c.died(third),"second reconstructed survivor remains killable");
    check(c.suspend_exterior(c.owner(),6,{}),"all-dead source can stream without resurrection");
    check(c.frame().native[ds::asset_index(asset)].survivingRequested==0,"all killed slots retain zero requests");
    check(c.resume_exterior(c.owner(),6) && c.prepared(c.owner(),asset),"empty source return remains valid");
    third.actor+=0x2000;third.owner+=0x2000;third.generation=4;
    check(!c.admitted(third),"zero-survivor return rejects a new enemy");
}
static void all_exterior_streaming_survivors(const coo::script::Views& views) {
    Replay r(views,false);auto& c=r.c;
    for(const auto& volume:ds::kVolumes) {
        if(volume.asset.registry==0x3E9B74F3U && volume.asset.slot==35) {enter(c,Replay::run,volume.asset);break;}
    }
    for(const auto& volume:ds::kVolumes) {
        if(volume.asset.registry==0xF5737F85U && volume.asset.slot==13) {enter(c,Replay::run,volume.asset);break;}
    }
    for(unsigned n=0;n<20;++n) {static_cast<void>(c.update(Replay::run,r.now,true));r.serve();r.now+=100;}
    constexpr std::array<std::size_t,7> requested{1,1,3,3,2,3,3};
    std::array<std::vector<ds::EnemyReceipt>,7> living{};std::size_t total{};
    const auto asset=[](std::uint16_t slot) {return ds::find(0x3E9B74F3U,1,slot)->asset;};
    for(std::uint16_t slot=1;slot<=7;++slot) {
        living[slot-1]=r.enemies[ds::spawn_index(asset(slot))];
        check(living[slot-1].size()==requested[slot-1],"every exterior source has its exact native admission count");
        total+=living[slot-1].size();
        check(c.suspend_exterior(c.owner(),slot,living[slot-1]),"all seven sources support a simultaneous survivor handoff");
    }
    check(total==16,"survivor handoff retains all sixteen exterior actors simultaneously");
    // Restore the complete encounter first, then kill one actor in each group.
    // The next trip must keep each death in its original source, including the
    // new Goblin groups and one of the two ledge Hobgoblins.
    for(unsigned trip=1;trip<=2;++trip) {
        for(std::uint16_t slot=1;slot<=7;++slot) {
            const auto a=asset(slot);const auto& state=c.frame().native[ds::asset_index(a)];
            check(state.suspended && state.survivingRequested==living[slot-1].size()
                && !state.active && !state.prepared,"every suspended source requests only its recorded survivors");
            check(!c.prepared(c.owner(),a),"late native placement cannot prepare any suspended exterior group");
            check(c.resume_exterior(c.owner(),slot) && c.prepared(c.owner(),a),"every reloaded exterior source resumes through placement readiness");
            for(auto& receipt:living[slot-1]) {
                const auto old=receipt;receipt.actor+=0x10000;receipt.owner+=0x10000;receipt.generation=trip+1;
                check(!c.died(old) && !c.admitted(old),"old native lifetime cannot supply receipts for any exterior group");
                check(c.admitted(receipt) && !c.admitted(receipt),"all returned exterior native admissions are unique");
                const auto tactical=ds::kSpawns[ds::spawn_index(a)].tactical;
                check(c.readiness(receipt,{true,true,true,true,receipt.actor+9000,tactical.registry,tactical.slot,tactical.row}),
                    "returned Goblins Harpies and Hobgoblins accept genuine native health AI and tactical readiness");
            }
            ds::EnemyReceipt excess{Replay::run,static_cast<std::uint32_t>(0x100000+trip*32+slot),
                static_cast<std::uint32_t>(0x200000+trip*32+slot),trip+1,slot,0x3E9B74F3U};
            check(!c.admitted(excess),"no exterior source can duplicate a survivor or refill a killed slot");
        }
        if(trip==1) {
            for(auto& group:living) {check(c.died(group.front()),"one genuinely returned enemy in each exterior group remains killable");group.erase(group.begin());}
            check(living[4].size()==1,"killing one ledge Hobgoblin preserves the other for the next trip");
        } else {
            for(auto& group:living) {for(const auto& receipt:group) {check(c.died(receipt),"every second-trip survivor remains killable");}group.clear();}
        }
        for(std::uint16_t slot=1;slot<=7;++slot) {
            check(c.suspend_exterior(c.owner(),slot,living[slot-1]),"every exterior group preserves its remaining slots on another Mists trip");
        }
    }
    for(std::uint16_t slot=1;slot<=7;++slot) {
        const auto a=asset(slot);check(c.frame().native[ds::asset_index(a)].survivingRequested==0,"all exterior kills remain terminal after repeated trips");
        check(c.resume_exterior(c.owner(),slot) && c.prepared(c.owner(),a),"all-dead exterior sources return without requesting actors");
        check(!c.admitted({Replay::run,static_cast<std::uint32_t>(0x300000+slot),0x400000,4,slot,0x3E9B74F3U}),
            "no restored Goblin Harpy or Hobgoblin source resurrects a killed actor");
    }
}
static void late_well_arrival(const coo::script::Views& views) {
    Replay r(views,false);auto& c=r.c;
    const auto travel=[&](std::uint32_t registry,std::uint16_t slot) {
        for(const auto& v:ds::kVolumes) {if(v.asset.registry==registry && v.asset.slot==slot) {enter(c,Replay::run,v.asset);return;}}std::abort();
    };
    const auto seen=[&](std::uint32_t registry,std::uint16_t slot) {
        for(std::size_t i=0;i<std::size(ds::kVolumes);++i) {const auto a=ds::kVolumes[i].asset;if(a.registry==registry && a.slot==slot) {return c.seen().test(i);}}std::abort();
    };
    const auto tick=[&](bool beforeSurface) {
        static_cast<void>(c.update(Replay::run,r.now,true));r.serve();const auto* g=c.graph();
        for(const auto& b:g->commands) {
            if(c.step_state(b.step).phase!=coo::StepPhase::active) {continue;}
            const auto& q=g->definition.steps[b.step].commands[b.command];
            if(!coo::is_observation(q.operation)) {continue;}
            if(beforeSurface && c.frame().section>=4) {continue;}
            // Never synthesize either branch of the missed exterior approach,
            // or any Well arrival/charge observation after the phase starts.
            if(!beforeSurface && (c.frame().section!=4 || b.step==2)) {continue;}
            r.fulfill(q);
        }
        if(!beforeSurface) {c.position(Replay::run,{});}
        static_cast<void>(c.update(Replay::run,r.now,true));r.now+=100;
        check(c.frame().enabled,"late Well arrival fixture remains enabled");
    };
    for(unsigned n=0;n<2000 && c.frame().section<4;++n) {tick(true);}
    check(c.frame().section==4 && !seen(0x3E9B74F3U,103),"fixture reaches return route without exterior approach trigger");
    travel(0xD997395EU,79);travel(0xD997395EU,89);travel(0x40A009B5U,12);c.position(Replay::run,{});
    for(unsigned n=0;n<200 && !c.frame().plates[0].armed;++n) {tick(false);}
    check(!seen(0x3E9B74F3U,103) && c.frame().section==5 && c.frame().plates[0].armed,
        "recorded Well arrivals release the plate despite a missed exterior trigger and departed landing volumes");
    check(!c.frame().plates[0].charged && !c.frame().native[ds::asset_index(ds::find(0xD997395EU,4,11)->asset)].desired,
        "retained route arrivals cannot fabricate charge or reveal the first platform");
}

static void mists_bypass_and_boss_gates(const coo::script::Views& views) {
    Replay r(views,false);auto& c=r.c;
    const auto tick=[&](bool route) {
        static_cast<void>(c.update(Replay::run,r.now,true));r.serve();const auto* g=c.graph();
        if(route) {for(const auto& b:g->commands) {
            if(c.step_state(b.step).phase==coo::StepPhase::active) {
                const auto& q=g->definition.steps[b.step].commands[b.command];
                if(coo::is_observation(q.operation)) {r.fulfill(q);}
            }
        }}
        static_cast<void>(c.update(Replay::run,r.now,true));r.now+=100;
    };
    for(unsigned i=0;i<1000 && c.frame().section<1;++i) {tick(true);}
    check(c.frame().section==1,"enter cave after surface patrol");
    const auto volume=[](std::string_view name) {
        for(const auto& cap:ds::kCapabilities) {if(cap.id==name) {return cap.spec.asset;}}
        std::abort();
    };
    enter(c,Replay::run,volume("mists.sml_arena_trigger_volume"));tick(false);tick(false);
    for(std::uint16_t slot:std::array<std::uint16_t,7>{4,5,6,7,8,9,10}) {
        const auto a=ds::find(0x153E22CDU,1,slot)->asset;
        check(c.frame().native[ds::asset_index(a)].desired,"first room requests infantry while Harpies live");
    }
    unsigned alive{};c.living_enemies([&](const auto& e) {if(e.registry==0x153E22CDU && e.source<=10) {++alive;}});
    check(alive==15,"entrance and small-room squads are alive when the player runs past");
    const auto arrival=r.now;
    const auto room=volume("mists.lrg_arena_trigger_volume");
    for(const auto& v:ds::kVolumes) {if(v.asset==room) {c.position(Replay::run,point(v),arrival);}}
    const auto identified=[&] {
        const auto* g=c.graph();if(c.frame().section!=2) {return false;}
        for(const auto& b:g->commands) {
            const auto& q=g->definition.steps[b.step].commands[b.command];
            if(q.operation==coo::Operation::dialogue && q.argument==5) {return c.step_state(b.step).phase==coo::StepPhase::complete;}
        }return false;
    };
    for(unsigned i=0;i<99;++i) {tick(false);check(!identified(),"boss identification cannot precede ten seconds after room entry");}
    check(c.frame().section==2 && c.boss_request().enemy.valid(),"boss room advances with every first-room enemy alive");
    r.now=arrival+9999;tick(false);check(!identified(),"9999ms entry buffer remains closed");
    r.now=arrival+10000;tick(false);check(identified(),"ten seconds after retained entry releases identification dialogue");
    const auto requested=[&](std::uint16_t slot) {
        return c.frame().native[ds::asset_index(ds::find(0x153E22CDU,1,slot)->asset)].desired;
    };
    const auto backWave=[&](bool expected) {
        for(const auto slot:std::array<std::uint16_t,5>{14,15,16,17,19}) {
            check(requested(slot)==expected,"second-room reinforcement requests follow boss arrival");
        }
    };
    const auto finalWave=[&](bool expected) {
        for(const auto slot:std::array<std::uint16_t,4>{23,24,25,26}) {
            check(requested(slot)==expected,"final-room reinforcement requests follow boss arrival");
        }
    };
    backWave(false);finalWave(false);
    auto q=c.boss_request();check(q.stage==0 && !q.requested,"initial boss pose settled");
    auto foreign=q.enemy;++foreign.generation;
    check(!c.health_event(foreign,ds::boss_damage::floor(0),r.now) && c.boss_request().stage==0,"foreign damage event cannot request a retreat");
    check(c.health_event(q.enemy,ds::boss_damage::floor(0),r.now),"first health gate applied event observed");
    q=c.boss_request();
    check(q.stage==1 && q.requested,"first retreat requests in the health callback without a publication update");
    const auto firstRevision=q.revision;
    check(!c.health_event(q.enemy,ds::boss_damage::floor(0),r.now) && c.boss_request().revision==firstRevision,"duplicate floor sample cannot dispatch a second retreat");
    check(q.stage==1 && q.requested && ds::boss_damage::blocked(q,c.frame().bossHealth),"retreat stays immune");
    check(!c.boss_position(q.enemy,1,q.revision+1),"stale arrival cannot unlock immunity");
    static_cast<void>(c.update(Replay::run,r.now,true));backWave(false);finalWave(false);
    check(c.boss_position(q.enemy,1,q.revision),"owned first retreat arrival");
    q=c.boss_request();check(!ds::boss_damage::blocked(q,c.frame().bossHealth),"second third unlocks only on native arrival");
    static_cast<void>(c.update(Replay::run,r.now,true));
    backWave(true);finalWave(false);
    check(c.frame().section==2,"second-room wave requests without crossing the chase trigger or killing earlier guards");
    check(c.health_event(q.enemy,ds::boss_damage::floor(1),r.now),"second health gate applied event observed");
    q=c.boss_request();
    check(q.stage==2 && q.requested,"final retreat requests in the health callback without a publication update");
    check(q.stage==2 && q.requested && ds::boss_damage::blocked(q,c.frame().bossHealth),"final retreat stays immune");
    finalWave(false);
    check(!c.boss_position(q.enemy,2,q.revision+1),"stale final arrival cannot release guards");
    static_cast<void>(c.update(Replay::run,r.now,true));finalWave(false);
    check(!dialogue_requested(c,6),"pursuit dialogue waits until second teleport finishes");
    check(c.boss_position(q.enemy,2,q.revision),"owned final arrival");
    static_cast<void>(c.update(Replay::run,r.now,true));finalWave(false);
    check(c.frame().section==2,"final guards wait for third-room entry after the second gate");
    check(dialogue_requested(c,6),"second teleport arrival immediately requests pursuit dialogue before third-room entry");
    bool pursuit{};
    // Let preceding native audio finish while the player remains outside room3.
    for(unsigned n=0;n<220 && !pursuit;++n) {
        tick(false);for(const auto row:r.dialogue) {pursuit=pursuit || row==6;}
    }
    check(pursuit && c.frame().section==2,"pursuit dialogue submits before third-room entry without guard kills");
    enter(c,Replay::run,volume("mists.hydra_trigger_volume"));
    for(unsigned n=0;n<4;++n) {tick(false);}
    finalWave(true);
    check(c.frame().section==3 && c.frame().objective==ds::kObjectives[3].event,"third-room entry updates objective with every second-room enemy alive");
    unsigned pursuitSurvivors{};c.living_enemies([&](const auto& e) {if(e.registry==0x153E22CDU && e.source>=12 && e.source<20) {++pursuitSurvivors;}});
    check(pursuitSurvivors==14,"all front and back guards survive the final-room transition");
    const auto returnRequested=[&](std::uint16_t slot) {return c.frame().native[ds::asset_index(ds::find(0x3E9B74F3U,1,slot)->asset)].desired;};
    for(std::uint16_t slot=9;slot<=17;++slot) {check(!returnRequested(slot),"surface return patrols wait for actual Mind death");}
    check(c.died(q.enemy),"actual final Mind death");
    static_cast<void>(c.update(Replay::run,r.now,true));
    for(const auto slot:std::array<std::uint16_t,3>{9,13,14}) {check(returnRequested(slot),"Mind death requests standing return patrols before using the exit portal");}
    for(const auto slot:std::array<std::uint16_t,6>{10,11,12,15,16,17}) {check(!returnRequested(slot),"Mind death cannot request return reinforcements before engagement zones");}
    check(!returnRequested(18) && !returnRequested(20) && !returnRequested(21),"early surface patrols leave dormant Fallen sources disabled");
    enter(c,Replay::run,volume("exit_route.tv_endpoint"));
    enter(c,Replay::run,volume("return_route.tv_teleport"));
    for(unsigned n=0;n<4;++n) {tick(false);}
    check(c.frame().section==4,"return to surface without approaching reinforcement zones");
    for(const auto slot:std::array<std::uint16_t,6>{10,11,12,15,16,17}) {check(!returnRequested(slot),"return portal does not release distant reinforcements");}
    enter(c,Replay::run,volume("surface.echoes_intro_fallback01_trigger_volume"));tick(false);
    check(returnRequested(11) && returnRequested(12),"entry zone requests its two reinforcement groups");
    for(const auto slot:std::array<std::uint16_t,4>{10,15,16,17}) {check(!returnRequested(slot),"entry zone does not release the separate combat-zone reinforcements");}
    enter(c,Replay::run,volume("surface.echoes_fallback01_trigger_volume"));tick(false);
    for(const auto slot:std::array<std::uint16_t,4>{10,15,16,17}) {check(returnRequested(slot),"combat-zone entry requests remaining reinforcements");}
    check(!ds::boss_damage::blocked(c.boss_request(),0.F),"final third permits native lethal damage");
}

static void incremental_platforms(const coo::script::Views& views) {
    Replay r(views,false);auto& c=r.c;
    const auto a=[](std::uint16_t type,std::uint16_t slot) {
        if(type==60) {for(const auto& v:ds::kVolumes) {if(v.asset.registry==0xD997395EU && v.asset.slot==slot) {return v.asset;}}}
        const auto* found=ds::find(0xD997395EU,type,slot);check(found!=nullptr,"fixture native asset exists");return found->asset;
    };
    const auto desired=[&](std::uint16_t type,std::uint16_t slot) {return c.frame().native[ds::asset_index(a(type,slot))].desired;};
    const auto tick=[&](bool route,bool clear=false) {
        static_cast<void>(c.update(Replay::run,r.now,true));r.serve();const auto* g=c.graph();
        for(const auto& b:g->commands) {
            if(c.step_state(b.step).phase!=coo::StepPhase::active) {continue;}
            const auto& q=g->definition.steps[b.step].commands[b.command];
            if(!coo::is_observation(q.operation)) {continue;}
            if(!route && q.asset.registry==0xD997395EU && q.asset.type==60 && q.asset.slot>=53 && q.asset.slot<=57) {continue;}
            if(c.frame().section==6 && (!clear || q.asset==ds::kScans[0].source)) {continue;}
            r.fulfill(q);
        }
        static_cast<void>(c.update(Replay::run,r.now,true));r.now+=100;check(c.frame().enabled,"platform fixture enabled");
    };
    const auto travel=[&](std::uint32_t registry,std::uint16_t slot) {
        for(const auto& v:ds::kVolumes) {if(v.asset.registry==registry && v.asset.slot==slot) {enter(c,Replay::run,v.asset);return;}}std::abort();
    };
    const auto surface=[&](std::uint16_t slot) {return c.frame().native[ds::asset_index(ds::find(0x3E9B74F3U,1,slot)->asset)].desired;};
    travel(0x3E9B74F3U,35);
    for(unsigned n=0;n<10;++n) {static_cast<void>(c.update(Replay::run,r.now,true));r.serve();r.now+=100;}
    check(surface(1) && surface(2) && surface(5) && surface(6) && surface(7) && !surface(3) && !surface(4),
        "patrol approach requests the original upper Goblins and two ledge Hobgoblins while lower Goblins wait for drop");
    travel(0xF5737F85U,13);
    for(unsigned n=0;n<10;++n) {static_cast<void>(c.update(Replay::run,r.now,true));r.serve();r.now+=100;}
    for(std::uint16_t slot=1;slot<=7;++slot) {check(surface(slot),"lower-bowl occupancy preserves all seven exterior sources");}
    for(unsigned n=0;n<2000 && c.frame().section<5;++n) {tick(true);}
    check(c.frame().section==5 && desired(4,21) && desired(23,22),"conflux visible before Well traversal");
    check(!c.frame().scanArmed[0],"visibility does not arm scan");
    enter(c,Replay::run,a(60,55));enter(c,Replay::run,a(60,56));enter(c,Replay::run,a(60,57));c.position(Replay::run,{});
    for(unsigned n=0;n<100 && !desired(4,11);++n) {tick(false);}
    for(unsigned n=0;n<10;++n) {tick(false);}
    check(c.frame().plates[0].charged && desired(4,11),"native charge exposes first platform");
    check(!desired(4,12) && !desired(4,14) && !desired(4,15) && !desired(4,18),"charge and stale visits cannot reveal later platforms");
    check(!desired(1,27) && !desired(1,28),"ground squads wait for supporting platforms");
    check(desired(1,33) && desired(1,34),"lower final guards appear at plate start before ascent");
    check(!desired(1,29) && !desired(1,30) && !desired(1,31) && !desired(1,32) && !desired(1,35),"plate charge cannot request Well Harpies before the final Hydra");
    enter(c,Replay::run,a(60,53));for(unsigned n=0;n<10;++n) {tick(false);}
    check(desired(4,12) && desired(4,13) && desired(1,27),"first landing reveals second platform cover and squad");
    check(!desired(4,14) && !desired(1,28),"second platform does not cascade through a stale visit");
    enter(c,Replay::run,a(60,55));for(unsigned n=0;n<10;++n) {tick(false);}
    check(desired(4,14) && desired(1,28) && !desired(4,15),"second landing reveals third platform and squad only");
    enter(c,Replay::run,a(60,56));for(unsigned n=0;n<10;++n) {tick(false);}
    check(desired(4,15) && desired(4,16) && desired(4,17) && desired(23,8),"third landing reveals fourth assembly and device");
    check(!desired(4,18),"spare slab stays absent before the final landing");
    enter(c,Replay::run,a(60,57));for(unsigned n=0;n<30;++n) {tick(false);}
    check(c.frame().section==6 && !desired(4,18),"final landing reaches conflux without the spare slab");
    check(desired(4,15) && desired(4,16) && desired(4,17) && desired(23,8),"conflux supporting assembly remains after final landing");
    check(!desired(4,10) && !desired(23,9),"unused sniper slab and its device never join final platform");
    check(desired(1,35) && desired(1,29) && desired(1,30) && !desired(1,31) && !desired(1,32),"two Harpy groups request alongside the final Well Hydra");
    unsigned wellHarpies{};c.living_enemies([&](const auto& e) {if(e.registry==0xD997395EU && e.source>=29 && e.source<=32) {++wellHarpies;}});
    check(wellHarpies==6,"final Well escort has six Harpies instead of twelve");
    check(desired(4,21) && desired(23,22) && !c.frame().scanArmed[0],"conflux stays visible while guards hold scan access");
    for(unsigned n=0;n<20 && !c.frame().scanArmed[0];++n) {tick(false,true);}
    check(c.frame().scanArmed[0] && !c.frame().scanStarted[0],"real guard deaths arm scan without inventing interaction");
    auto optional=r.enemies[22].front();optional.actor+=100000;
    check(!c.admitted(optional) && !c.frame().populationFault,"extra optional surface actor cannot fault mission on reentry");
    auto required=r.enemies[ds::spawn_index(ds::kBoss)].front();required.actor+=200000;
    check(!c.admitted(required) && c.frame().populationFault,"extra required encounter actor still reports population overflow");
}


static void presentation_records() {
    namespace p=sunrise::client::hooks::bootflow::hijacked_presentation;
    std::array<std::byte,0x1500> bytes{};bytes.fill(std::byte{0xA5});
    p::put<std::uint32_t>(bytes,0,0x80B4241FU);p::put<std::uint32_t>(bytes,4,0x80804F4CU);
    p::put<std::int64_t>(bytes,8,0x1408);p::put<std::uint32_t>(bytes,0x48,0x12345678);
    p::put<std::uint32_t>(bytes,0x4C,0x80804F4BU);p::put<std::int64_t>(bytes,0x50,0);
    check(p::source(bytes,true) && !p::source(bytes,false),"exact native dialogue identity accepted");
    auto wrong=bytes;p::put<std::uint32_t>(wrong,0,0x80B4241CU);
    check(!p::source(wrong,true),"foreign dialogue definition rejected");
    check(!p::source(std::span<const std::byte>(bytes).first(0x57),true),"short source rejected");
    // Dialogue delivery is tested from production wire bodies by native_mission_dialogue_tests.
    p::put<std::uint32_t>(wrong,0,0x80B4241CU);p::put<std::uint32_t>(wrong,4,0x80804F54U);
    p::put<std::int64_t>(wrong,8,0xB88);p::put<std::uint32_t>(wrong,0x4C,0x80804F53U);
    check(p::source(wrong,false),"exact directive source accepted");p::put<std::uint32_t>(wrong,0,0x80B4241FU);
    check(!p::source(wrong,false),"foreign directive definition rejected");
}

static void held_region_routing() {
    namespace route=sunrise::server::bap::encrypted::activity_message::membership;
    namespace membership=sunrise::state::activity::membership;
    namespace client=sunrise::middleware::bap::activity_message::client_authoritative_data;
    membership::MembershipState current{};
    const auto apply=[&](int held,std::uint32_t heldHash,int outgoing,std::uint32_t outgoingHash,
            std::uint8_t token,bool hasToken,int expected,std::uint32_t expectedHash) {
        client::ClientAuthoritativeData parsed{};
        parsed.hasCurrentRegion=held>=0;parsed.currentRegion={held,heldHash,held>=0};
        parsed.hasRegion=true;parsed.region={outgoing,outgoingHash,true};
        parsed.hasTransitionToken=hasToken;parsed.transitionToken=token;
        const auto update=route::make_authoritative(parsed,"adventure_rumba");
        check(update.hasCurrentRegion==parsed.hasCurrentRegion,"Hijacked retains sparse held-region receipt presence");
        check(update.currentRegion.index==held && update.currentRegion.hash==heldHash,
              "Hijacked maps exact held index and hash");
        check(update.region.index==outgoing && update.region.hash==outgoingHash,
              "Hijacked keeps outgoing leg for transition prefetch");
        current=membership::transactions::merge(current,update);
        check(current.region.index==expected && current.region.hash==expectedHash,
              "Hijacked publishes loaded region after swap and target during prefetch");
        const auto repeated=membership::transactions::merge(current,update);
        check(membership::transactions::equal(current,repeated),"duplicate Hijacked report cannot rewind publication");
    };
    // Live run1: second-leg hashes come from client_authoritative rows; loaded
    // region hashes come from native slice-set-switch names in the same log.
    apply(-1,0,104,0x1BD69720U,1,true,104,0x1BD69720U);
    apply(104,0x65771C73U,-1,0x811C9DC5U,0,false,104,0x65771C73U);
    apply(-1,0,296,0xEF471B4AU,2,true,296,0xEF471B4AU); // t117032 prefetch.
    apply(296,0x37A08717U,104,0x1BD69720U,0,false,296,0x37A08717U); // t132360.
    apply(-1,0,264,0xB42CB106U,3,true,264,0xB42CB106U); // t142125 prefetch.
    apply(264,0x849E9C59U,296,0xEF471B4AU,0,false,264,0x849E9C59U); // t160110.
    apply(296,0x37A08717U,264,0xB42CB106U,0,false,296,0x37A08717U); // t170860 return.
    apply(-1,0,264,0xB42CB106U,3,true,296,0x37A08717U); // Repeated current token.
    apply(-1,0,-1,0x811C9DC5U,0,false,296,0x37A08717U); // Unset is not a region.
    apply(-1,0,264,0xB42CB106U,4,true,264,0xB42CB106U); // t184844 genuine next transition.
    check(route::retains_held_region("mercury_freeroam") && route::retains_held_region("strike_pact"),
          "existing Mercury and strike held-region policies remain enabled");
    check(!route::retains_held_region("adventure_rumba_extra") && !route::retains_held_region("adventure_whisk"),
          "Hijacked opt-in is exact and does not change other mission routing");
}

int main() {
    held_region_routing();native_survivor_hierarchy();
    check(hijacked_boss_damage_contracts(),"Hijacked native damage contracts");
    check(hijacked_lifetime_binding(),"Hijacked native lifetime binding");
    presentation_records();clock_ownership();native_contracts();auto doc=parse(shipped());check(doc && ds::valid_document(doc->views()),"shipped Lua compiles against recovered bindings");
    mists_bypass_and_boss_gates(doc->views());opening_tunnel_bypass(doc->views());exterior_streaming_survivors(doc->views());all_exterior_streaming_survivors(doc->views());cleanup_at_user_location(doc->views());late_well_arrival(doc->views());incremental_platforms(doc->views());Replay normal(doc->views(),false);normal.run_all();Replay early(doc->views(),true);early.run_all();Replay adjustedInitial(doc->views(),false,true);adjustedInitial.run_all();
    check(adjustedInitial.moves[0]==0 && adjustedInitial.moves[1]>0 && adjustedInitial.moves[2]>0,"initial relocation proximity cannot block genuine health retreats");
    std::puts("PASS Hijacked normal and early-death route, native ownership, charge, scan, movement and ending gates");
}
