#include "state/activity/coo/omega_adapter.h"
#include "state/activity/coo/receipt_queue.h"
#include "fixtures/coo_shared_legacy_presentation.h"
#include "fixtures/coo_shared_legacy_composition.h"
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace coo = dawn::state::activity::coo;
namespace omega = coo::omega;
namespace present = dawn::state::activity::omega_presentation;
namespace frozen = dawn::state::activity::frozen_shared_presentation;
namespace old = coo::frozen_composition;
unsigned checks{}, comparisons{};
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#x); std::exit(1); } } while(false)

// A deliberately different composition: two publishers, four facts, concurrent
// prerequisites, and another schema. This is a contract fixture, not Gateway.
constexpr coo::ModuleBinding modules[]{{{91,101,0,0},17},{{92,102,0,0},44}};
constexpr coo::CommandSpec boot[]{
    {coo::Operation::mechanic,modules[0].asset,17,coo::Wait::requested},
    {coo::Operation::mechanic,modules[1].asset,44,coo::Wait::requested}};
constexpr coo::CommandSpec factCommands[]{
    {coo::Operation::observation,{},0,coo::Wait::observed},
    {coo::Operation::observation,{},1,coo::Wait::observed},
    {coo::Operation::observation,{},2,coo::Wait::observed},
    {coo::Operation::observation,{},3,coo::Wait::observed}};
constexpr coo::Step moduleSteps[]{
    {"publishers",0,boot}, {"a",1,{&factCommands[0],1}}, {"b",1,{&factCommands[1],1}},
    {"joined",6,{&factCommands[2],1}}, {"destination",8,{&factCommands[3],1}}};
constexpr coo::ObservationBinding factBindings[]{{5,1,0},{9,2,0},{12,3,0},{31,4,0}};
constexpr coo::MissionDefinition alternate{{"synthetic composition",coo::Schema::otherMissions,moduleSteps},modules,factBindings};
struct AlternateFrame final { int first{}, second{}; };
struct AlternatePorts final : coo::MissionPorts<AlternateFrame> {
    std::vector<std::uint32_t> calls;
    std::uint32_t facts{};
    bool selected{};
    void update_module(std::uint32_t id,const coo::MissionInput& input,AlternateFrame& frame) noexcept override {
        selected=input.executor; calls.push_back(id);
        if(id==17) { frame.first=7; }
        if(id==44) { CHECK(frame.first==7);frame.second=frame.first+2; }
    }
    std::uint32_t observations(std::uint64_t,const AlternateFrame&) noexcept override { return facts; }
};
void composition_contracts() {
    CHECK(coo::MissionRuntime::valid(alternate));
    coo::MissionRuntime runtime;
    AlternatePorts ports;
    CHECK(runtime.select(8,true));
    CHECK(runtime.select(8,false));
    // Later facts may arrive first and remain retained until both prerequisites.
    ports.facts=(1U<<12)|(1U<<31);
    const auto first=runtime.update(alternate,{8,0,0,false,false},ports);
    CHECK(first.second==9 && ports.selected);
    CHECK(ports.calls==std::vector<std::uint32_t>({17,44}));
    CHECK(runtime.diagnostics().complete==1);
    ports.facts=1U<<5;
    static_cast<void>(runtime.update(alternate,{8,1,0,false,false},ports));
    CHECK(runtime.diagnostics().complete==3);
    ports.facts=1U<<9;
    static_cast<void>(runtime.update(alternate,{8,2,0,false,false},ports));
    CHECK(runtime.diagnostics().complete==31 && runtime.diagnostics().phase==coo::Phase::complete);
    const auto incarnation=runtime.diagnostics().incarnation;
    runtime.reset();ports.facts=0;
    static_cast<void>(runtime.update(alternate,{8,3,0,false,true},ports));
    CHECK(runtime.diagnostics().incarnation>incarnation && runtime.diagnostics().complete==1);
    const auto before=ports.calls.size();
    static_cast<void>(runtime.update(alternate,{0,4,0,false,true},ports));CHECK(ports.calls.size()==before);
    static_cast<void>(runtime.update(alternate,{9,4,0,false,false},ports));CHECK(!ports.selected);
    static_cast<void>(runtime.update(alternate,{9,5,0,false,true},ports));CHECK(!ports.selected);

    auto invalid=alternate;
    std::array duplicateModules{modules[0],modules[0]};invalid.modules=duplicateModules;
    CHECK(!coo::MissionRuntime::valid(invalid));
    invalid=alternate;
    std::array duplicateFacts{factBindings[0],factBindings[0]};invalid.observations=duplicateFacts;
    CHECK(!coo::MissionRuntime::valid(invalid));
    invalid=alternate;
    std::array invalidFacts{factBindings[0],factBindings[1],factBindings[2],factBindings[3]};
    invalidFacts[3].fact=32;invalid.observations=invalidFacts;
    CHECK(!coo::MissionRuntime::valid(invalid));
    invalidFacts[3]={31,0,0};CHECK(!coo::MissionRuntime::valid(invalid));
    const auto prior=ports.calls.size();
    static_cast<void>(runtime.update(invalid,{10,6,0,false,true},ports));
    CHECK(runtime.diagnostics().failure==coo::Failure::definition && ports.calls.size()==prior);
    runtime.reset();
    static_cast<void>(runtime.update(alternate,{11,0,0,false,true},ports));
    auto changed=alternate;
    static_cast<void>(runtime.update(changed,{11,1,0,false,true},ports));
    CHECK(runtime.diagnostics().failure==coo::Failure::definition);
}

constexpr coo::CommandSpec nativeCommands[]{
    {coo::Operation::scene,{1,11,43,2},0,coo::Wait::completed},
    {coo::Operation::population,{2,12,18,3},0,coo::Wait::completed},
    {coo::Operation::objective,{3,13,68,0},0,coo::Wait::requested},
    {coo::Operation::dialogue,{4,14,53,0},0,coo::Wait::requested},
    {coo::Operation::device,{5,15,23,0},1,coo::Wait::requested},
    {coo::Operation::cinematic,{6,16,6,0},0,coo::Wait::completed},
    {coo::Operation::traversal,{7,17,60,0},0,coo::Wait::requested},
    {coo::Operation::mechanic,{8,18,0,0},2,coo::Wait::requested}};
constexpr coo::Step nativeSteps[]{{"native requests",0,nativeCommands}};
constexpr coo::Definition nativeDefinition{"native service fixture",coo::Schema::otherMissions,nativeSteps};
struct NativeBinding final : coo::NativeServices<NativeBinding> {
    coo::Executor& executor;
    std::vector<coo::Command> requested, retired;
    explicit NativeBinding(coo::Executor& value):executor(value){}
    coo::ServiceContext context() const noexcept {
        const auto d=executor.diagnostics();return {nativeDefinition,d.run,d.incarnation};
    }
    bool request(const coo::Command& command) noexcept { requested.push_back(command);return true; }
    void retire(const coo::Command& command) noexcept { retired.push_back(command); }
};
void native_contracts() {
    coo::Executor executor;NativeBinding binding(executor);
    CHECK(executor.start(nativeDefinition,3));executor.update(binding);CHECK(binding.requested.size()==8);
    auto original=binding.requested[0];
    for(unsigned variant=0;variant<8;++variant) {
        auto foreign=original;
        switch(variant) {
        case 0:++foreign.token.run;break;case 1:++foreign.token.incarnation;break;
        case 2:++foreign.token.step;break;case 3:foreign.token.command=8;break;
        case 4:foreign.schema=coo::Schema::omegaArchive;break;
        case 5:foreign.spec.asset.slot=9;break;case 6:++foreign.spec.argument;break;
        case 7:foreign.spec.wait=coo::Wait::requested;break;
        }
        CHECK(!binding.publish(foreign));binding.cancel(foreign);CHECK(binding.retired.empty());
    }
    CHECK(executor.enqueue({executor.token(0,0),coo::Milestone::completed}));executor.update(binding);
    CHECK(!executor.step_state(0).commands[0].completed);
    for(auto index:{0U,1U,5U}) {
        CHECK(executor.enqueue({executor.token(0,index),coo::Milestone::nativeReady}));
        CHECK(executor.enqueue({executor.token(0,index),coo::Milestone::completed}));
    }
    executor.update(binding);CHECK(executor.diagnostics().phase==coo::Phase::complete);
    executor.cancel(binding);CHECK(binding.retired.size()==8);
    for(unsigned i=0;i<8;++i) { CHECK(binding.retired[i].token.command==7-i); }
    CHECK(executor.start(nativeDefinition,3));CHECK(!binding.publish(original));
}

void receipt_contracts() {
    struct Receipt final { std::uint64_t owner{},time{};unsigned kind{}; };
    coo::ReceiptQueue<Receipt,3> queue;
    const auto same=[](const Receipt& a,const Receipt& b) { return a.owner==b.owner && a.kind==b.kind; };
    CHECK(queue.push({1,10,2},same));CHECK(queue.push({1,20,2},same));CHECK(queue.size()==1);
    CHECK(queue.push({1,21,3},same));CHECK(queue.push({2,22,2},same));
    CHECK(!queue.push({2,23,4},same));CHECK(queue.overflowed());
    Receipt value;CHECK(queue.pop(value));CHECK(value.time==10);
    CHECK(!queue.push({2,24,2},same));queue.discard();CHECK(queue.overflowed() && queue.size()==0);
    queue.reset();CHECK(!queue.overflowed());
    for(unsigned i=0;i<500;++i) {
        CHECK(queue.push({4,i,i}));CHECK(queue.pop(value));CHECK(value.kind==i);
    }
}

void native_ledger_contracts() {
    struct ActorReceipt final {
        std::uint64_t run{};
        std::uint32_t actor{},sourceHandle{},generation{};
        std::uint16_t source{};
        std::uint32_t registry{};
        bool operator==(const ActorReceipt&) const = default;
        bool valid() const noexcept { return run!=0 && generation!=0 && actor!=0 && sourceHandle!=0; }
    };
    struct Cohort final { std::uint16_t source;std::uint32_t registry;std::uint8_t count;bool required; };
    const std::array<Cohort,2> cohorts{{{3,77,2,true},{7,78,1,false}}};
    coo::PopulationService<ActorReceipt,2,2> populations;
    ActorReceipt a{91,1,101,5,3,77},b{91,2,102,5,3,77},optional{91,3,103,5,7,78};
    CHECK(populations.admit(cohorts,a,91,5)==coo::Admission::ignored);
    populations.enable(0);populations.enable(1);
    CHECK(populations.admit(cohorts,a,91,5)==coo::Admission::accepted);
    CHECK(populations.admit(cohorts,a,91,5)==coo::Admission::ignored);
    CHECK(populations.admit(cohorts,b,91,6)==coo::Admission::ignored);
    CHECK(populations.admit(cohorts,b,91,5)==coo::Admission::accepted);
    auto extraActor=b;extraActor.actor=4;extraActor.sourceHandle=104;
    CHECK(populations.admit(cohorts,extraActor,91,5)==coo::Admission::overflow);
    CHECK(populations.admit(cohorts,optional,91,5)==coo::Admission::accepted);
    extraActor=optional;extraActor.actor=5;extraActor.sourceHandle=105;
    CHECK(populations.admit(cohorts,extraActor,91,5)==coo::Admission::ignored);
    auto wrongSalt=a;++wrongSalt.sourceHandle;CHECK(!populations.died(wrongSalt,91,5));
    CHECK(!populations.cleared(0,2));CHECK(populations.died(a,91,5));CHECK(!populations.died(a,91,5));
    CHECK(!populations.cleared(0,2));CHECK(populations.died(b,91,5));CHECK(populations.cleared(0,2));
    CHECK(!populations.cleared(1,1)); // Optional actor is still alive.
    populations={};CHECK(!populations.died(a,91,5));

    struct Owner final {
        std::uint64_t run{},epoch{};
        bool valid() const noexcept { return run!=0 && epoch!=0; }
        bool operator==(const Owner&) const=default;
    };
    struct Body final { std::uint32_t generation{};bool stop{};std::uint8_t eventCount{};std::array<std::uint32_t,2> events{}; };
    coo::SceneService<Body,Owner,2> scenes;
    CHECK(!scenes.begin(2,1,{1,1}));CHECK(!scenes.begin(0,0,{1,1}));
    CHECK(!scenes.begin(0,0x80000000ULL,{1,1}));CHECK(!scenes.begin(0,1,{}));
    CHECK(scenes.begin(0,7,{1,2}));CHECK(scenes.requested(0));CHECK(scenes.owner(0).epoch==2);
    CHECK(scenes.event(0,90)==coo::SceneEvent::accepted);CHECK(scenes.event(0,90)==coo::SceneEvent::accepted);
    CHECK(scenes.commands()[0].eventCount==1);CHECK(scenes.event(0,91)==coo::SceneEvent::accepted);
    CHECK(scenes.event(0,92)==coo::SceneEvent::overflow);CHECK(scenes.event(1,90)==coo::SceneEvent::ignored);
    scenes.mark(0,4);CHECK(scenes.seen(0,4));scenes.stop(0);CHECK(scenes.commands()[0].stop);
    CHECK(scenes.begin(0,8,{1,3}));CHECK(!scenes.seen(0,4));CHECK(!scenes.commands()[0].stop);
    CHECK(scenes.commands()[0].eventCount==0 && scenes.owner(0).epoch==3);
    scenes.mark(9,4);CHECK(!scenes.seen(9,4) && !scenes.owner(9).valid());
}

void alternate_dialogue_contracts() {
    constexpr coo::DialogueRow rows[]{{101,100,20,false},{102,100,0,false},{103,100,0,true}};
    constexpr coo::ObjectiveCueBinding objectiveRows[]{{1,555}};
    const coo::DialogueDefinition policy{987,rows,objectiveRows,500,10};
    struct State final { std::array<std::uint32_t,3> generations{};std::uint32_t objective{};std::uint8_t activeRow{coo::kNoDialogue}; };
    coo::DialogueService<3> service;State state;std::uint32_t revision{};
    service.enqueue(policy,0,1000,20,1,revision);service.enqueue(policy,2,1000,0,1,revision);
    CHECK(revision==1);service.advance(policy,20,1019,false,state,revision);CHECK(state.activeRow==coo::kNoDialogue);
    service.advance(policy,20,1020,false,state,revision);CHECK(state.activeRow==0 && state.generations[0]==21);
    CHECK(!service.submitted(policy,988,0,21,1030,state,revision));
    CHECK(!service.submitted(policy,987,0,20,1030,state,revision));
    service.objective(policy,555,state,revision);service.enqueue(policy,1,1020,0,1,revision);
    CHECK(service.submitted(policy,987,0,21,1030,state,revision));CHECK(service.voice_until()==1160);
    service.advance(policy,20,1159,false,state,revision);CHECK(state.activeRow==coo::kNoDialogue);
    service.advance(policy,20,1160,true,state,revision);CHECK(state.activeRow==coo::kNoDialogue);
    service.objective(policy,999,state,revision);service.advance(policy,20,1160,false,state,revision);
    CHECK(state.activeRow==coo::kNoDialogue && !service.due(1160));
    service={};state={};revision=0;service.enqueue(policy,0,2000,0,1,revision);
    service.advance(policy,21,2000,false,state,revision);CHECK(state.generations[0]==22);
    service.discard_before(2);CHECK(state.activeRow==0); // Retain an offered row in flight.
    service.advance(policy,21,2499,false,state,revision);CHECK(service.timed_out()==0);
    service.advance(policy,21,2500,false,state,revision);CHECK(service.timed_out()==1 && state.activeRow==coo::kNoDialogue);
    // Required speech survives missing native authority without a new generation
    // or an invented acknowledgement, then releases the next line exactly once.
    service={};state={};revision=0;
    service.objective(policy,555,state,revision);
    service.enqueue(policy,0,3000,0,1,revision);service.enqueue(policy,1,3000,0,1,revision);
    service.advance(policy,22,3000,false,state,revision,true);
    const auto generation=state.generations[0];
    for(const auto now:{3500ULL,4000ULL,65000ULL}) {
        service.advance(policy,22,now,false,state,revision,true);
        CHECK(state.activeRow==0 && state.generations[0]==generation);
    }
    CHECK(service.timed_out()==3 && state.generations[1]==0);
    CHECK(!service.submitted(policy,988,0,generation,65001,state,revision));
    CHECK(!service.submitted(policy,987,0,generation+1,65001,state,revision));
    CHECK(service.submitted(policy,987,0,generation,65001,state,revision));
    CHECK(!service.submitted(policy,987,0,generation,65001,state,revision));
    service.advance(policy,22,65131,false,state,revision,true);CHECK(state.activeRow==1);
    service.silence(state,revision);service.advance(policy,22,90000,false,state,revision,true);
    CHECK(state.activeRow==coo::kNoDialogue);
}

void compare_presentation(const present::Run& a,const frozen::Run& b,std::uint64_t now) {
    ++comparisons;
    const auto& x=a.presentation();const auto& y=b.presentation();
    CHECK(x.generations==y.generations);CHECK(x.objective==y.objective);CHECK(x.activeRow==y.activeRow);
    CHECK(x.intro.revision==y.intro.revision && x.intro.play==y.intro.play && x.bossGeneration==y.bossGeneration);
    CHECK(a.revision()==b.revision() && a.generation()==b.generation());CHECK(a.timed_out()==b.timed_out());
    CHECK(a.cycle()==b.cycle());CHECK(static_cast<unsigned>(a.landmark())==static_cast<unsigned>(b.landmark()));
    CHECK(static_cast<unsigned>(a.navigation_goal())==static_cast<unsigned>(b.navigation_goal()));CHECK(a.intro_phase()==b.intro_phase());
    CHECK(a.boss_ready()==b.boss_ready() && a.boss_flight_ready()==b.boss_flight_ready());
    CHECK(a.ending_dialogue_finished(now)==b.ending_dialogue_finished(now));
    CHECK(a.ending_dialogue_finished(now+15000)==b.ending_dialogue_finished(now+15000));
}
void presentation_parity() {
    for(unsigned trial=0;trial<96;++trial) {
        present::Run current;frozen::Run accepted;
        const std::uint64_t run=trial+1;
        current.initialize(run,true);accepted.initialize(run,true);
        std::uint64_t now=1000;
        auto compare=[&] { compare_presentation(current,accepted,now); };
        auto advance=[&](std::uint64_t delta) { now+=delta;current.advance(now);accepted.advance(now);compare(); };
        auto submit=[&] {
            const auto row=current.presentation().activeRow;
            if(row==present::kNoDialogue) { return; }
            const auto generation=current.presentation().generations[row];
            current.submitted(present::kDialogueBank,row,generation,now);
            accepted.submitted(present::kDialogueBank,row,generation,now);compare();
        };
        for(unsigned landmark=trial%3;landmark<6;++landmark) {
            current.enter(static_cast<present::Landmark>(landmark),now);
            accepted.enter(static_cast<frozen::Landmark>(landmark),now);compare();
            advance(2000);if(trial%2==0) { submit(); }advance(20000);
        }
        for(std::uint8_t cycle=1;cycle<=3;++cycle) {
            for(unsigned event=0;event<9;++event) {
                if(event==2) {
                    const auto scene=present::kRescuePresentation[cycle-1].definition;
                    current.scene(scene,true,now);accepted.scene(scene,true,now);compare();
                }
                current.encounter(static_cast<present::Encounter>(event),cycle,now);
                accepted.encounter(static_cast<frozen::Encounter>(event),cycle,now);compare();
                advance(1000+trial*37);if(trial%3!=1) { submit(); }advance(20000);submit();advance(20000);
                // Late and repeated events cannot regress mechanics or replay cues.
                current.encounter(present::Encounter::defenses,cycle,now);
                accepted.encounter(frozen::Encounter::defenses,cycle,now);compare();
                if(event==7 && cycle<3) { break; }
            }
        }
        current.encounter(present::Encounter::cinematic,3,now);
        accepted.encounter(frozen::Encounter::cinematic,3,now);compare();advance(20000);
        // Repeat with mixed direct requests to cover queue timing, stale cues,
        // scene-owned rows, wrong receipts, reset and native dispatch timeouts.
        current.initialize(run+1000,true);accepted.initialize(run+1000,true);
        current.enter(present::Landmark::arena,now);accepted.enter(frozen::Landmark::arena,now);
        std::uint32_t random=trial+1;
        for(unsigned i=0;i<240;++i) {
            random=random*1664525U+1013904223U;
            const auto row=static_cast<std::uint8_t>((random>>8)%36);
            switch(random%5) {
            case 0:current.enqueue(row,now,random%5000);accepted.enqueue(row,now,random%5000);break;
            case 1:{const auto objective=present::kObjectives[(random>>16)%7];current.set_objective(objective);accepted.set_objective(objective);break;}
            case 2:submit();break;
            case 3:current.submitted(99,row,0,now);accepted.submitted(99,row,0,now);break;
            case 4:current.scene(present::kRescuePresentation[(random>>16)%3].definition,true,now);
                accepted.scene(present::kRescuePresentation[(random>>16)%3].definition,true,now);break;
            }
            compare();advance(random%17000);
        }
    }
}

struct NewPorts final : omega::Controllers {
    omega::Frame value;
    std::array<bool,8> observed{};
    std::vector<unsigned> calls;
    present::Presentation presentation(const omega::Input&) noexcept override { calls.push_back(0);return value.presentation; }
    dawn::state::activity::omega_first_lair::Authority encounter(std::uint64_t,std::uint32_t gen,bool) noexcept override {
        CHECK(gen==value.presentation.bossGeneration);calls.push_back(1);return value.encounter;
    }
    void request_ending(std::uint64_t,bool) noexcept override { calls.push_back(2); }
    dawn::state::activity::omega_ending::Authority ending(const omega::Input&) noexcept override { calls.push_back(3);return value.ending; }
    std::array<bool,8> facts(std::uint64_t,const omega::Frame&) noexcept override { return observed; }
};
struct OldPorts final : old::Controllers {
    NewPorts& target;
    explicit OldPorts(NewPorts& value):target(value){}
    present::Presentation presentation(const old::Input& input) noexcept override {
        return target.presentation({input.run,input.now,input.region,input.entrance,input.executor});
    }
    dawn::state::activity::omega_first_lair::Authority encounter(std::uint64_t run,std::uint32_t gen,bool selected) noexcept override {
        return target.encounter(run,gen,selected);
    }
    void request_ending(std::uint64_t run,bool selected) noexcept override { target.request_ending(run,selected); }
    dawn::state::activity::omega_ending::Authority ending(const old::Input& input) noexcept override {
        return target.ending({input.run,input.now,input.region,input.entrance,input.executor});
    }
    std::array<bool,8> observations(std::uint64_t,const old::Frame&) noexcept override { return target.observed; }
};
void composition_parity() {
    for(bool selected:{false,true}) for(unsigned trial=0;trial<32;++trial) {
        coo::MissionRuntime current;old::Adapter accepted;
        NewPorts now,then;OldPorts oldPorts(then);
        for(unsigned i=0;i<12;++i) {
            now.value.presentation.bossGeneration=7;
            now.value.presentation.objective=present::kObjectives[i%7];
            now.value.presentation.generations.fill(i+1);
            now.value.encounter.endingRequested=i>=9;now.value.ending.complete=i>=10;
            for(unsigned fact=0;fact<8;++fact) { now.observed[fact]=i>=((fact+(trial%8))%8); }
            then.value=now.value;then.observed=now.observed;now.calls.clear();then.calls.clear();
            const auto a=current.update(coo::script::mission(omega::kMission),{42,i*1000U,120,true,selected},now);
            const auto b=accepted.update({42,i*1000U,120,true,selected},oldPorts);
            CHECK(now.calls==then.calls);
            CHECK(a.presentation.generations==b.presentation.generations && a.presentation.objective==b.presentation.objective);
            CHECK(a.presentation.bossGeneration==b.presentation.bossGeneration);
            CHECK(a.encounter.endingRequested==b.encounter.endingRequested && a.ending.complete==b.ending.complete);
            const auto x=current.diagnostics(),y=accepted.diagnostics();
            CHECK(x.phase==y.phase && x.active==y.active && x.complete==y.complete && x.incarnation==y.incarnation);
        }
        current.reset();accepted.reset();now.observed={};then.observed={};
        static_cast<void>(current.update(coo::script::mission(omega::kMission),{42,1,0,false,selected},now));
        static_cast<void>(accepted.update({42,1,0,false,selected},oldPorts));
        CHECK(current.diagnostics().complete==accepted.diagnostics().complete);
    }
}
int main() {
    composition_contracts();native_contracts();receipt_contracts();native_ledger_contracts();alternate_dialogue_contracts();presentation_parity();composition_parity();
    std::printf("PASS: %u checks; %u frozen presentation comparisons; shared composition, native services, FIFO and alternate schema contracts\n",checks,comparisons);
}
