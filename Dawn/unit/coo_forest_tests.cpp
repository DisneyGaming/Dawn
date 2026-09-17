#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <random>
#include <type_traits>
#include "state/activity/coo/omega_forest_controller.h"
#include "client/hooks/bootflow/omega_forest_recipe.h"
#include "client/hooks/bootflow/omega_navigation_rules.h"
#include "state/activity/coo/omega_projection.h"
#include "middleware/encoding/bit_writer.h"
#include "fixtures/coo_forest_legacy_presentation.h"

namespace coo = dawn::state::activity::coo;
namespace forest = coo::omega::forest;
namespace present = dawn::state::activity::omega_presentation;
namespace old = dawn::state::activity::frozen_presentation;
namespace wire = dawn::middleware::bap::activity_message::sensor_auth_update;
namespace encoding = dawn::middleware::encoding;
unsigned checks{}, bodies{};
#define CHECK(value) do { ++checks; if (!(value)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#value); std::exit(1); } } while(false)

present::Point inside(const present::Volume& volume) {
    const auto triangle = volume.triangles[0];
    const auto a=volume.vertices[triangle.a], b=volume.vertices[triangle.b], c=volume.vertices[triangle.c];
    return {(a.x+b.x+c.x)/3,(a.y+b.y+c.y)/3,(volume.minimum.z+volume.maximum.z)/2};
}
struct Replay {
    old::Run reference;
    forest::Controller executor, legacy;
    void start(std::uint64_t run, std::uint64_t now, unsigned arrival) {
        reference.start(run,now,static_cast<old::Landmark>(arrival));
        executor.start(run,now,static_cast<present::Landmark>(arrival),true);
        legacy.start(run,now,static_cast<present::Landmark>(arrival),false);
        same();
    }
    void receipt(forest::Receipt receipt) {
        switch (receipt.kind) {
        case forest::Kind::position:
            for (const auto& v:present::kVolumes) {
                if (present::contains(v,receipt.position)) { reference.enter(static_cast<old::Landmark>(v.landmark),receipt.now); }
            }
            if (present::contains(present::kIntroVolume,receipt.position)) { reference.request_intro(receipt.now); }
            reference.observe_navigation({receipt.position.x,receipt.position.y,receipt.position.z}); break;
        case forest::Kind::submission: reference.submitted(receipt.identity,receipt.row,receipt.generation,receipt.now); break;
        case forest::Kind::scene: reference.scene(receipt.identity,receipt.active,receipt.now); break;
        case forest::Kind::intro: reference.observe_intro(receipt.generation,receipt.active,receipt.ready,receipt.now); break;
        case forest::Kind::encounter: reference.encounter(static_cast<old::Encounter>(receipt.identity),receipt.cycle,receipt.now); break;
        case forest::Kind::entrance: reference.enter(old::Landmark::tunnel,receipt.now); break;
        }
        executor.observe(receipt); legacy.observe(receipt);
    }
    void position(present::Point p,std::uint64_t now) { receipt({{},now,forest::Kind::position,p}); }
    void submit(std::uint64_t now) {
        const auto& p=reference.presentation();
        if (p.activeRow==present::kNoDialogue) { return; }
        forest::Receipt r{{},now,forest::Kind::submission};
        r.identity=present::kDialogueBank; r.row=p.activeRow; r.generation=p.generations[p.activeRow];
        receipt(r);
    }
    void update(std::uint64_t now,bool wireCheck=false) {
        executor.drain(); same();
        reference.advance(now); executor.state().advance(now); legacy.state().advance(now);
        same();
        if (wireCheck) { same_wire(); }
    }
    template<class Target> void compare(const Target& target) {
        const auto& a=reference.presentation(); const auto& b=target.presentation();
        CHECK(a.generations==b.generations); CHECK(a.objective==b.objective); CHECK(a.activeRow==b.activeRow);
        CHECK(a.intro.revision==b.intro.revision && a.intro.play==b.intro.play); CHECK(a.bossGeneration==b.bossGeneration);
        CHECK(reference.generation()==target.generation()); CHECK(reference.revision()==target.revision());
        CHECK(reference.timed_out()==target.timed_out()); CHECK(reference.cycle()==target.cycle());
        CHECK(static_cast<unsigned>(reference.landmark())==static_cast<unsigned>(target.landmark()));
        CHECK(static_cast<unsigned>(reference.navigation_goal())==static_cast<unsigned>(target.navigation_goal()));
        CHECK(reference.forest_complete()==target.forest_complete()); CHECK(reference.intro_phase()==target.intro_phase());
        CHECK(reference.boss_ready()==target.boss_ready());
        CHECK(reference.ending_dialogue_finished(UINT64_MAX)==target.ending_dialogue_finished(UINT64_MAX));
    }
    void same() { compare(executor.state()); compare(legacy.state()); CHECK(!executor.failed()); }
    void same_wire() {
        wire::Snapshot a{},b{};
        a.archiveOmega=b.archiveOmega=true;
        a.omegaSceneAuthority=b.omegaSceneAuthority=true;
        a.omegaDialogueArm=b.omegaDialogueArm=true;
        const auto& p=reference.presentation();
        a.omegaDialogueGenerations=p.generations; a.omegaActiveDialogueRow=p.activeRow; a.omegaObjectiveEvent=p.objective;
        a.omegaIntroRevision=p.intro.revision; a.omegaIntroPlay=p.intro.play; a.omegaBossGeneration=p.bossGeneration;
        coo::omega::Frame frame; frame.presentation=executor.state().presentation(); coo::omega::project(frame,b);
        for (auto type:std::array<std::uint8_t,2>{53,68}) {
            const std::uint16_t slot=type==53?2:0;
            std::array<std::byte,4096> x{},y{};
            encoding::bits::Writer wx(x),wy(y);
            CHECK(wire::write_auth_body(wx,a,0x82FB58B7U,type,slot,false));
            CHECK(wire::write_auth_body(wy,b,0x82FB58B7U,type,slot,false));
            CHECK(wx.bit_count()==wy.bit_count());
            std::size_t nx{},ny{}; CHECK(wx.finish(nx) && wy.finish(ny) && nx==ny);
            CHECK(x==y); ++bodies;
        }
    }
};

void accepted_timeline() {
    // Publication times and native submission receipts are from the accepted
    // adapter log (run 1). Volume times below are inferred from its changed
    // publications: this is a reconstructed replay, not captured position data.
    Replay r; r.start(1,77187,0); r.update(77187,true); CHECK(r.reference.presentation().activeRow==0);
    r.submit(77281); r.update(77453,true);
    r.receipt({{},102093,forest::Kind::entrance}); r.update(102093,true);
    r.update(103640,true); CHECK(r.reference.presentation().activeRow==6);
    r.submit(103656); r.update(103890,true);
    r.position(inside(present::kVolumes[2]),126390); r.update(126390,true);
    CHECK(r.reference.presentation().activeRow==7); r.submit(126421); r.update(126640,true);
    // Time alone cannot claim a Forest exit, change its objective or spawn a boss.
    r.update(320484,true); CHECK(!r.executor.state().forest_complete()); CHECK(!r.executor.state().presentation().bossGeneration);
    r.position(inside(present::kVolumes[3]),322703); r.update(322703,true);
    CHECK(r.reference.presentation().activeRow==9); r.submit(322734); r.update(322968,true);
    r.position(inside(present::kVolumes[4]),335468); r.update(335468,true);
    CHECK(r.executor.diagnostics().phase==coo::Phase::complete); CHECK(r.executor.diagnostics().complete==0xFF);
    CHECK(r.executor.skipped()==0); CHECK(r.executor.state().presentation().objective==0x3517D4D5);
    // Crossing the Lair volume requests the reveal only at its separate native footprint.
    CHECK(r.executor.state().intro_phase()==present::IntroPhase::dormant);
    r.position(inside(present::kIntroVolume),337062); r.update(337062,true);
    CHECK(r.executor.state().intro_phase()==present::IntroPhase::waiting);
}

void fast_crossings_and_arrivals() {
    for (unsigned arrival=0;arrival<6;++arrival) {
        for (unsigned next=0;next<6;++next) {
            Replay r; r.start(77,1000,arrival); r.update(1000,true);
            r.position(inside(present::kVolumes[next]),1100);
            r.position(inside(present::kIntroVolume),1101);
            r.submit(1102); r.update(1200,true); r.update(20000,true);
            for (unsigned back=0;back<6;++back) { r.position(inside(present::kVolumes[back]),20001+back); }
            r.update(20100,true);
        }
    }
    // An already offered clip is kept through several crossings. Unoffered
    // tunnel/vista lines are discarded; exit dialogue uses the first exit time.
    Replay r; r.start(2,1000,0); r.update(1000);
    r.position(inside(present::kVolumes[1]),1010); r.position(inside(present::kVolumes[2]),1020);
    r.position(inside(present::kVolumes[3]),1030); r.submit(1040); r.update(1100,true);
    r.update(7734,true); CHECK(r.reference.presentation().activeRow==present::kNoDialogue);
    r.update(7735,true); CHECK(r.reference.presentation().activeRow==9);
    // Remaining boss presentation has the same event ordering after handoff.
    r.position(inside(present::kVolumes[5]),20000); r.update(20000);
    for (std::uint8_t cycle=1;cycle<=3;++cycle) {
        for (unsigned event=0;event<10;++event) {
            if (cycle < 3 && event >= 8) { continue; }
            forest::Receipt receipt{{},30000ULL+cycle*300000ULL+event*20000ULL,forest::Kind::encounter};
            receipt.identity=event; receipt.cycle=cycle; r.receipt(receipt); r.update(receipt.now,true);
            r.submit(receipt.now+10); r.update(receipt.now+15000,true);
        }
    }
}

void intro_handoff_ordering() {
    Replay r; r.start(5,1000,4);
    r.position(inside(present::kIntroVolume),1100);
    forest::Receipt intro{{},1200,forest::Kind::intro}; intro.ready=true;
    r.receipt(intro); r.update(1250);
    CHECK(r.executor.state().intro_phase()==present::IntroPhase::priming);
    const auto generation=r.reference.presentation().bossGeneration;
    CHECK(generation!=0);
    CHECK(r.reference.observe_boss(5,present::kBossEntity,123,generation));
    CHECK(r.executor.state().observe_boss(5,present::kBossEntity,123,generation));
    CHECK(r.legacy.state().observe_boss(5,present::kBossEntity,123,generation));
    CHECK(r.reference.claim_boss_intro_action(5)); CHECK(r.executor.state().claim_boss_intro_action(5));
    CHECK(r.legacy.state().claim_boss_intro_action(5));
    // This camera receipt predates the synchronous flight receipt. Deferred
    // processing must not retroactively release its camera command.
    intro.now=1300; r.receipt(intro);
    CHECK(r.reference.observe_boss_flight(5,generation));
    CHECK(r.executor.state().observe_boss_flight(5,generation));
    CHECK(r.legacy.state().observe_boss_flight(5,generation));
    r.update(1350); CHECK(r.executor.state().intro_phase()==present::IntroPhase::priming);
    intro.now=1400; r.receipt(intro); r.update(1450,true);
    CHECK(r.executor.state().intro_phase()==present::IntroPhase::offered);
    intro.generation=r.reference.presentation().intro.revision; intro.now=1500; intro.active=true;
    r.receipt(intro); r.update(1550,true); CHECK(r.executor.state().intro_phase()==present::IntroPhase::playing);
    auto stale=intro; ++stale.generation; stale.active=false; stale.now=8000; r.receipt(stale); r.update(8100);
    CHECK(r.executor.state().intro_phase()==present::IntroPhase::playing);
    intro.active=false; intro.now=8400; r.receipt(intro); r.update(8649,true);
    CHECK(r.executor.state().presentation().activeRow==present::kNoDialogue);
    r.update(8650,true); CHECK(r.executor.state().presentation().activeRow==12);
    r.submit(8651); r.position(inside(present::kVolumes[5]),8652); r.update(9000,true);
    CHECK(r.executor.state().landmark()==present::Landmark::arena);
}

void journal_contracts() {
    forest::Controller c; c.start(8,1000,present::Landmark::lighthouse,true);
    const auto oldOwner=c.owner(); const auto before=c.state().revision();
    forest::Receipt pos{oldOwner,1001,forest::Kind::position,inside(present::kVolumes[3])};
    CHECK(c.enqueue(pos)); CHECK(c.state().revision()==before); CHECK(!c.state().forest_complete());
    for (unsigned i=0;i<10000;++i) { CHECK(c.enqueue(pos)); }
    CHECK(c.pending()==1); c.drain(); CHECK(c.state().forest_complete());
    c.start(8,2000,present::Landmark::lighthouse,true); CHECK(c.owner().incarnation>oldOwner.incarnation);
    CHECK(!c.enqueue(pos)); CHECK(c.pending()==0); CHECK(!c.state().forest_complete());
    pos.owner=c.owner(); ++pos.owner.run; CHECK(!c.enqueue(pos));
    forest::Receipt early{c.owner(),2001,forest::Kind::submission};
    early.identity=present::kDialogueBank; early.row=0; early.generation=9;
    CHECK(!c.enqueue(early)); c.state().advance(2002); CHECK(c.state().presentation().activeRow==0);
    c.drain(); CHECK(c.state().presentation().activeRow==0); CHECK(c.enqueue(early)); c.drain();
    CHECK(c.state().presentation().activeRow==present::kNoDialogue);
    const auto landmark=c.state().landmark();
    for (unsigned i=0;i<10000;++i) {
        forest::Receipt inactive{c.owner(),2500,forest::Kind::scene};
        inactive.identity=0x80F479BFU; CHECK(c.enqueue(inactive));
        inactive.active=true; inactive.identity=0x80EC0F95U; CHECK(c.enqueue(inactive));
    }
    CHECK(c.pending()==0 && !c.failed());
    for (std::size_t i=0;i<forest::Controller::kCapacity;++i) {
        forest::Receipt event{c.owner(),3000+i,forest::Kind::intro}; CHECK(c.enqueue(event));
    }
    CHECK(!c.enqueue({c.owner(),4000,forest::Kind::entrance})); CHECK(c.failed()); c.drain();
    CHECK(c.pending()==0); c.entrance(5000); CHECK(c.state().landmark()==landmark);
    CHECK(c.diagnostics().failure==coo::Failure::queueOverflow);
    c.reset(); c.start(9,6000,present::Landmark::tunnel,true); CHECK(!c.failed());
    CHECK(c.state().presentation().objective==present::kObjectives[1]);
    c.observe({{},6100,forest::Kind::position,inside(present::kIntroVolume)}); c.drain();
    CHECK(c.update_time(6000)==6100); c.state().advance(c.update_time(6000));
    CHECK(c.state().intro_phase()==present::IntroPhase::waiting);
}

void randomized_fifo() {
    std::mt19937 rng(0xF04E57);
    for (unsigned run=1;run<=120;++run) {
        Replay r; std::uint64_t now=1000; r.start(run,now,run%6);
        for (unsigned batch=0;batch<160;++batch) {
            for (unsigned n=0;n<1+rng()%12;++n) {
                now+=rng()%25;
                switch (rng()%7) {
                case 0: case 1: r.position(inside(present::kVolumes[rng()%6]),now); break;
                case 2: r.position(inside(present::kIntroVolume),now); break;
                case 3: {
                    present::Point point{};
                    static_cast<void>(present::navigation_point(static_cast<present::NavigationGoal>(rng()%7),point));
                    r.position(point,now); break;
                }
                case 4: r.submit(now); break;
                case 5: {
                    forest::Receipt event{{},now,forest::Kind::scene};
                    constexpr std::array scenes{0x80F479BFU,0x80F479F5U,0x80F47A08U,0U};
                    event.identity=scenes[rng()%4]; event.active=(rng()%2)!=0; r.receipt(event); break;
                }
                case 6: r.position({NAN,0,0},now); break;
                }
            }
            r.update(now,batch%8==0); now+=rng()%1000;
        }
    }
}

void bindings() {
    CHECK(coo::Executor::valid(forest::kDefinition));
    for (std::size_t i=0;i<forest::kVolumes.size();++i) {
        CHECK(forest::kVolumes[i].registry==present::kVolumes[i+1].registry);
        CHECK(forest::kVolumes[i].type==60 && forest::kVolumes[i].slot==present::kVolumes[i+1].slot);
    }
    for (const auto& line:forest::kDialogue) { CHECK(line.asset.definition==present::kDialogue[line.row].selector); }
    present::Run target; target.initialize(1);
    coo::PresentationServices services(forest::kBindings,target,1000);
    coo::Command command{{1,1,0,0},coo::Schema::otherMissions,forest::kExit[0]};
    CHECK(!services.publish(command)); CHECK(target.revision()==0);
    command.schema=coo::Schema::omegaArchive; ++command.spec.asset.registry;
    CHECK(!services.publish(command)); CHECK(target.revision()==0);
    command.spec=forest::kExit[0]; command.spec.wait=coo::Wait::nativeReady;
    CHECK(!services.publish(command));
    constexpr std::array duplicate{forest::kTraversal[0],forest::kTraversal[0]};
    const coo::PresentationBindings ambiguous{coo::Schema::omegaArchive,duplicate,forest::kObjectives,forest::kDialogue};
    coo::PresentationServices rejected(ambiguous,target,1000); command.spec=forest::kTunnel[0];
    CHECK(!rejected.publish(command)); CHECK(target.revision()==0);
}

template<class T,std::size_t N> void put(std::array<std::byte,N>& bytes,std::size_t offset,T value) {
    CHECK(offset+sizeof value<=bytes.size()); std::memcpy(bytes.data()+offset,&value,sizeof value);
}
void native_forest_contracts() {
    namespace recipe=dawn::client::hooks::bootflow::omega_forest;
    namespace nav=dawn::client::hooks::bootflow::omega_navigation;
    std::array<std::byte,recipe::kWorkerPrefixSize> worker{};
    worker.fill(std::byte{0xA5});
    put(worker,4,recipe::kWorkerDefinitionClass); put(worker,0x96C,recipe::kForestD);
    CHECK(recipe::matches(worker,"mission_scot")); CHECK(!recipe::matches(worker,"other"));
    const auto before=worker;
    recipe::RunSeed seed; const auto first=seed.select(1,0); CHECK(first!=0);
    CHECK(seed.select(1,42)==first); CHECK(seed.select(2,0)!=first);
    recipe::prepare_worker(worker,12345);
    CHECK(recipe::read<std::uint32_t>(worker,0x94C)==12345);
    CHECK(recipe::read<float>(worker,0x984)==0 && recipe::read<float>(worker,0x98C)==1);
    CHECK(recipe::read<std::uint8_t>(worker,0x988)==1);
    for (std::size_t i=0;i<worker.size();++i) {
        const bool owned=(i>=0x948 && i<0x950) || (i>=0x970 && i<=0x990);
        if (!owned) { CHECK(before[i]==worker[i]); }
    }
    float a=2,b=3; recipe::solver_inputs(false,false,a,b); CHECK(a==2 && b==3);
    recipe::solver_inputs(true,true,a,b); CHECK(a==2 && b==3);
    recipe::solver_inputs(true,false,a,b); CHECK(a==0 && b==0);
    std::array<std::byte,0x38> node{}; std::array<std::byte,nav::kGatewaySize> gate{};
    put(worker,0x89A,std::int8_t{3}); put(worker,0x89C,1.0F); put(worker,0x92C,std::int32_t{3});
    put(node,0x18,std::uint8_t{3}); put(node,0x24,std::int8_t{2}); put(node,0x2C,1.0F);
    std::uint8_t index{};
    CHECK(nav::terminal_gateway_index(worker,node,0,index) && index==2);
    CHECK(!nav::gateway_open(gate)); gate[0x355]=std::byte{1}; CHECK(nav::gateway_open(gate));
    CHECK(!nav::terminal_gateway_index(worker,node,1,index));
    put(node,0x2C,0.0F); CHECK(!nav::terminal_gateway_index(worker,node,0,index));
    put(node,0x2C,1.0F); put(node,0x24,std::int8_t{-1}); CHECK(!nav::terminal_gateway_index(worker,node,0,index));
    put(node,0x24,std::int8_t{3}); CHECK(!nav::terminal_gateway_index(worker,node,0,index));
    put(node,0x24,std::int8_t{2}); put(worker,0x89C,0.0F); CHECK(!nav::terminal_gateway_index(worker,node,0,index));
    put(worker,0x858,std::int64_t{0x1000}); put(worker,0x924,std::int32_t{4});
    constexpr std::uintptr_t base=0x100000, nodes=base+0x1868;
    CHECK(nav::owns_node(worker,base,nodes+0x38*3)); CHECK(!nav::owns_node(worker,base,nodes+0x38*4));
    CHECK(!nav::owns_node(worker,base,nodes+1)); CHECK(!nav::owns_node(worker,base,nodes-0x38));
    CHECK(nav::forward_goal(present::NavigationGoal::forestGates,false)==present::NavigationGoal::forestGates);
    CHECK(nav::forward_goal(present::NavigationGoal::forestGates,true)==present::NavigationGoal::lairApproach);
    CHECK(nav::destination_bubble(present::NavigationGoal::lairApproach)==14);
}

int main() {
    native_forest_contracts(); bindings(); accepted_timeline(); fast_crossings_and_arrivals(); intro_handoff_ordering(); journal_contracts(); randomized_fifo();
    std::printf("PASS: %u checks; %u wire bodies; frozen presentation parity, Forest/Lair traversal, FIFO and reset safety\n",checks,bodies);
}
