#include "server/runtime/activity/round_activity_runtime.h"
#include "server/runtime/activity/persistent_activity.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <source_location>

namespace ra = sunrise::server::runtime::activity::round_activity;
namespace timed = sunrise::server::runtime::activity::timed_round;
namespace coo = sunrise::state::activity::coo;
namespace native_population = sunrise::state::activity::native_population;
namespace native_activity_transit = sunrise::server::runtime::activity::native_activity_transit;

unsigned checks{};

void expect(bool value, std::source_location where=std::source_location::current()) {
    ++checks;
    if (!value) {
        std::fprintf(stderr, "round integration check %u failed at line %u\n", checks,where.line());
        std::exit(1);
    }
}

void definition_defaults_fail_closed() {
    const ra::Definition definition{};
    expect(!ra::valid(definition));
    const auto roles = ra::phase_roles(definition.roles);
    expect(roles[0] == "round.entry" && roles[5] == "round.rewards");
}

void timed_round_receipts_keep_stale_events_out() {
    timed::Service service;
    const sunrise::state::activity::ActivityInstanceKey owner{77, {3}};
    expect(service.begin(owner, 19, {100, 10}));
    const auto entry = service.snapshot().token;
    expect(service.capture_complete(entry, 0) == timed::Result::accepted);
    const auto traversal = service.snapshot().token;
    expect(service.add_progress(traversal, 5, 101, 10) == timed::Result::accepted);
    expect(service.encounter_arrived(service.snapshot().token, 5) == timed::Result::accepted);
    const auto encounter = service.snapshot().token;
    expect(service.encounter_defeated(encounter, 6, 9001) == timed::Result::accepted);
    expect(service.returned(service.snapshot().token, 7) == timed::Result::accepted);
    expect(service.capture_complete(service.snapshot().token, 8) == timed::Result::accepted);
    expect(service.add_progress(service.snapshot().token, 9, 102, 10) == timed::Result::accepted);
    expect(service.encounter_arrived(service.snapshot().token, 9) == timed::Result::accepted);
    expect(service.encounter_defeated(service.snapshot().token, 10, 9002) == timed::Result::accepted);
    expect(service.returned(service.snapshot().token, 11) == timed::Result::accepted);
    expect(service.capture_complete(service.snapshot().token, 12) == timed::Result::accepted);
    expect(service.add_progress(service.snapshot().token, 112, 103, 10) == timed::Result::accepted);
    expect(service.snapshot().expired);
    expect(service.encounter_arrived(service.snapshot().token, 112) == timed::Result::accepted);
    expect(service.encounter_defeated(service.snapshot().token, 113, 9003) == timed::Result::accepted);
    expect(service.snapshot().phase == timed::Phase::rewards);
    expect(service.rewards_finished(service.snapshot().token, 114) == timed::Result::accepted);
    expect(service.snapshot().phase == timed::Phase::complete);
    expect(service.encounter_defeated(traversal, 115, 9004) == timed::Result::stale);
}

void native_frame_completion_is_absent_by_default() {
    sunrise::server::runtime::activity::NativeActivityFrame frame{};
    expect(!frame.completion.valid() && frame.endEpoch == 0 && !frame.restricted);
}

void persistent_population_reuse_seam() {
    namespace population = sunrise::server::runtime::activity::population;
    namespace registry = sunrise::state::activity::coo::registry;
    namespace sense = sunrise::middleware::bap::activity_message::sense_update;
    constexpr std::uint32_t key=0x12345678U;
    constexpr registry::Slot slots[]{{0,1,1,0,0x80807EC9U,1},{7,66,1,0,0,2}};
    const registry::Definition definition{"persistent-reuse",1,key,2,3,1,
        std::span<const registry::Slot>(slots)};
    const population::Capability capability{&definition,0,7,{},true,0,1,true};
    population::Service service;
    const population::Owner owner{77,{3}};
    expect(service.begin(owner,std::span(&capability,1),19));
    const population::Command initial{owner,1,1,key,0,1,19};
    expect(service.request(initial,1)==population::Result::accepted);
    expect(service.request_retirement(owner,19,2,2,key,0)==population::Result::accepted);
    expect(service.request({owner,3,3,key,0,1,19},1)==population::Result::notAllowed);
    sense::SenseObject observation{};
    observation.registryKey=key;observation.slotType=1;observation.slotIndex=0;
    observation.hasNativeSchema=true;observation.nativeSchema=0x80807ECC;
    observation.hasRootDelta=true;observation.nativeRevision=1;
    observation.sourceDelta.present=1;observation.sourceDelta.scalar[0]=4;
    expect(service.observe(1,observation)!=nullptr);
    expect(service.retirement_acknowledged(owner,19,key,0,4,true));
    expect(service.begin_cycle(owner,19,3,3,key,0,2,1)==population::Result::accepted);
    const auto batch=service.project(1);
    expect(batch.count==1 && batch.entries[0].source.generation==5
        && batch.entries[0].source.looseRequested==1
        && !batch.entries[0].source.retireOwned);
}

namespace synthetic {
constexpr coo::Asset moduleAsset{0x10000001,0x20000001,0,0};
constexpr coo::Asset prepareAsset{0x10000002,0x20000002,1,1};
constexpr coo::Asset plateAsset{0x10000003,0x20000003,30,2};
constexpr coo::Asset captureAsset{0x10000004,0x20000004,4,3};
constexpr coo::Asset startAsset{0x10000005,0x20000005,37,4};
constexpr coo::Asset progressAsset{0x10000006,0x20000006,30,5};
constexpr coo::Asset travelAsset{0x10000007,0x20000007,37,6};
constexpr coo::Asset arrivalAsset{0x10000008,0x20000008,30,7};
constexpr coo::Asset bossAsset{0x10000009,0x20000009,1,8};
constexpr coo::Asset retireAsset{0x1000000A,0x2000000A,1,9};
constexpr coo::Asset rewardsAsset{0x1000000B,0x2000000B,1,10};
constexpr coo::Asset spawnAsset{0x1000000C,0x2000000C,1,11};
constexpr coo::Asset travelEntryAsset{0x1000000D,0x2000000D,37,12};
constexpr coo::Asset completeAsset{0x1000000E,0x2000000E,0,13};

constexpr std::array<coo::script::Capability,14> capabilities{{
    {"persistent.start","composition",{coo::Operation::mechanic,moduleAsset,1,coo::Wait::requested},0},
    {"round.prepare","nativeActivity",{coo::Operation::mechanic,prepareAsset,1,coo::Wait::requested},0},
    {"round.plate","nativeActivity",{coo::Operation::observation,plateAsset,1,coo::Wait::observed},0},
    {"round.capture","nativeActivity",{coo::Operation::mechanic,captureAsset,1,coo::Wait::completed},0},
    {"round.start","nativeActivity",{coo::Operation::mechanic,startAsset,1,coo::Wait::requested},0},
    {"round.progress","nativeActivity",{coo::Operation::observation,progressAsset,1,coo::Wait::observed},0},
    {"round.travel","nativeActivity",{coo::Operation::mechanic,travelAsset,1,coo::Wait::requested},0},
    {"round.arrival","nativeActivity",{coo::Operation::observation,arrivalAsset,1,coo::Wait::observed},0},
    {"round.boss","nativeActivity",{coo::Operation::observation,bossAsset,1,coo::Wait::observed},0},
    {"round.retire","nativeActivity",{coo::Operation::observation,retireAsset,1,coo::Wait::observed},0},
    {"round.rewards","nativeActivity",{coo::Operation::mechanic,rewardsAsset,1,coo::Wait::requested},0},
    {"round.spawn","nativeActivity",{coo::Operation::mechanic,spawnAsset,1,coo::Wait::completed},0},
    {"round.travelEntry","nativeActivity",{coo::Operation::mechanic,travelEntryAsset,1,coo::Wait::requested},0},
    {"round.complete","nativeActivity",{coo::Operation::mechanic,completeAsset,1,coo::Wait::requested},0},
}};
constexpr std::array<coo::script::ModuleCapability,1> modules{{
    {"persistent",{moduleAsset,1}},
}};
const coo::script::Profile profile{
    "synthetic.profile","synthetic.schema",coo::Schema::otherMissions,
    capabilities,modules,{}, {}, {}, {}, {}, {}, {}
};

const std::string_view documentText=R"json({
  "format_version":2,
  "mission":"synthetic.round",
  "profile":"synthetic.profile",
  "authority_schema":"synthetic.schema",
  "assets":{
    "module":{"registry":"0x10000001","definition":"0x20000001","type":0,"slot":0},
    "prepare":{"registry":"0x10000002","definition":"0x20000002","type":1,"slot":1},
    "plate":{"registry":"0x10000003","definition":"0x20000003","type":30,"slot":2},
    "capture":{"registry":"0x10000004","definition":"0x20000004","type":4,"slot":3},
    "start":{"registry":"0x10000005","definition":"0x20000005","type":37,"slot":4},
    "progress":{"registry":"0x10000006","definition":"0x20000006","type":30,"slot":5},
    "travel":{"registry":"0x10000007","definition":"0x20000007","type":37,"slot":6},
    "arrival":{"registry":"0x10000008","definition":"0x20000008","type":30,"slot":7},
    "boss":{"registry":"0x10000009","definition":"0x20000009","type":1,"slot":8},
    "retire":{"registry":"0x1000000A","definition":"0x2000000A","type":1,"slot":9},
    "rewards":{"registry":"0x1000000B","definition":"0x2000000B","type":1,"slot":10}
    ,"spawn":{"registry":"0x1000000C","definition":"0x2000000C","type":1,"slot":11}
    ,"travel_entry":{"registry":"0x1000000D","definition":"0x2000000D","type":37,"slot":12}
    ,"complete":{"registry":"0x1000000E","definition":"0x2000000E","type":0,"slot":13}
  },
  "bindings":{
    "module":{"capability":"persistent.start","operation":"mechanic","asset":"module","argument":1,"wait":"requested"},
    "prepare":{"capability":"round.prepare","operation":"mechanic","asset":"prepare","argument":1,"wait":"requested"},
    "plate":{"capability":"round.plate","operation":"observation","asset":"plate","argument":1,"wait":"observed"},
    "capture":{"capability":"round.capture","operation":"mechanic","asset":"capture","argument":1,"wait":"completed"},
    "start":{"capability":"round.start","operation":"mechanic","asset":"start","argument":1,"wait":"requested"},
    "progress":{"capability":"round.progress","operation":"observation","asset":"progress","argument":1,"wait":"observed"},
    "travel":{"capability":"round.travel","operation":"mechanic","asset":"travel","argument":1,"wait":"requested"},
    "arrival":{"capability":"round.arrival","operation":"observation","asset":"arrival","argument":1,"wait":"observed"},
    "boss":{"capability":"round.boss","operation":"observation","asset":"boss","argument":1,"wait":"observed"},
    "retire":{"capability":"round.retire","operation":"observation","asset":"retire","argument":1,"wait":"observed"},
    "rewards":{"capability":"round.rewards","operation":"mechanic","asset":"rewards","argument":1,"wait":"requested"},
    "spawn":{"capability":"round.spawn","operation":"mechanic","asset":"spawn","argument":1,"wait":"completed"},
    "travel_entry":{"capability":"round.travelEntry","operation":"mechanic","asset":"travel_entry","argument":1,"wait":"requested"},
    "complete":{"capability":"round.complete","operation":"mechanic","asset":"complete","argument":1,"wait":"requested"}
  },
  "graphs":{
    "composition":{"name":"composition","domain":"composition","steps":[{"id":"start","after":[],"commands":[{"id":"module","binding":"module"}]}],"receipts":{}},
    "entry":{"name":"entry","domain":"nativeActivity","steps":[
      {"id":"prepare","after":[],"commands":[{"id":"prepare","binding":"prepare"}]},
      {"id":"plate","after":["prepare"],"commands":[{"id":"plate","binding":"plate"}]},
      {"id":"capture","after":["plate"],"commands":[{"id":"capture","binding":"capture"}]},
      {"id":"start","after":["capture"],"commands":[{"id":"start","binding":"start"}]}
    ],"receipts":{"plate":"plate","capture":"capture"}},
    "traversal":{"name":"traversal","domain":"nativeActivity","steps":[{"id":"progress","after":[],"commands":[{"id":"progress","binding":"progress"}]}],"receipts":{"progress":"progress"}},
    "to_arena":{"name":"to_arena","domain":"nativeActivity","steps":[
      {"id":"travel","after":[],"commands":[{"id":"travel","binding":"travel"}]},
      {"id":"arrival","after":["travel"],"commands":[{"id":"arrival","binding":"arrival"}]},
      {"id":"spawn","after":["arrival"],"commands":[{"id":"spawn","binding":"spawn"}]}
    ],"receipts":{"arrival":"arrival","spawn":"spawn"}},
    "encounter":{"name":"encounter","domain":"nativeActivity","steps":[{"id":"boss","after":[],"commands":[{"id":"boss","binding":"boss"}]}],"receipts":{"boss":"boss"}},
    "returning":{"name":"returning","domain":"nativeActivity","steps":[
      {"id":"retire","after":[],"commands":[{"id":"retire","binding":"retire"}]},
      {"id":"travel_entry","after":["retire"],"commands":[{"id":"travel_entry","binding":"travel_entry"}]},
      {"id":"arrival","after":["travel_entry"],"commands":[{"id":"arrival","binding":"arrival"}]}
    ],"receipts":{"retire":"retire","arrival":"arrival"}},
    "rewards":{"name":"rewards","domain":"nativeActivity","steps":[
      {"id":"retire","after":[],"commands":[{"id":"retire","binding":"retire"}]},
      {"id":"rewards","after":["retire"],"commands":[{"id":"rewards","binding":"rewards"}]},
      {"id":"arrival","after":["rewards"],"commands":[{"id":"arrival","binding":"arrival"}]},
      {"id":"complete","after":["arrival"],"commands":[{"id":"complete","binding":"complete"}]}
    ],"receipts":{"retire":"retire","arrival":"arrival"}}
  },
  "roles":{
    "round.entry":"entry","round.traversal":"traversal","round.toEncounter":"to_arena",
    "round.encounter":"encounter","round.returning":"returning","round.rewards":"rewards"
  },
  "entry":"composition",
  "modules":["persistent"],
  "observations":[],
  "presentation":{"dialogue":{"bank":"0x00000000","rows":[],"objective_cues":[],"dispatch_timeout_ms":100,"spacing_ms":0},"cue_sets":{},"action_sets":{},"binding_tables":{}}
})json";

const std::array<ra::CommandBinding,14> commandBindings{{
    {{coo::Operation::mechanic,prepareAsset,1,coo::Wait::requested},ra::Operation::prepareEntry},
    {{coo::Operation::observation,plateAsset,1,coo::Wait::observed},ra::Operation::plateOccupied},
    {{coo::Operation::mechanic,captureAsset,1,coo::Wait::completed},ra::Operation::capture},
    {{coo::Operation::mechanic,startAsset,1,coo::Wait::requested},ra::Operation::startTraversal},
    {{coo::Operation::observation,progressAsset,1,coo::Wait::observed},ra::Operation::progressFull},
    {{coo::Operation::mechanic,travelAsset,1,coo::Wait::requested},ra::Operation::travelEncounter},
    {{coo::Operation::observation,arrivalAsset,1,coo::Wait::observed},ra::Operation::travelArrived},
    {{coo::Operation::observation,bossAsset,1,coo::Wait::observed},ra::Operation::bossDead},
    {{coo::Operation::observation,retireAsset,1,coo::Wait::observed},ra::Operation::retireEncounter},
    {{coo::Operation::mechanic,rewardsAsset,1,coo::Wait::requested},ra::Operation::travelRewards},
    {{coo::Operation::mechanic,spawnAsset,1,coo::Wait::completed},ra::Operation::spawnEncounter},
    {{coo::Operation::mechanic,travelEntryAsset,1,coo::Wait::requested},ra::Operation::travelEntry},
    {{coo::Operation::mechanic,completeAsset,1,coo::Wait::requested},ra::Operation::complete},
}};
const std::array<native_population::PaletteDefinition,1> palettes{{{0x30000001,1,2,1}}};
const std::array<std::uint16_t,1> rewardPlacements{{0}};
const std::array<ra::Encounter,1> encounters{{{0,{}, {},true,"synthetic"}}};
const std::array<ra::Score,1> scores{{{0xABCDEF01,50,50}}};
const std::array<native_activity_transit::Destination,2> destinations{{{900,1,1},{901,1,2}}};
const std::array<ra::Platform,1> platforms{{{{0,1,2},0,0,0,901}}};
const ra::Definition definition{
    moduleAsset,{},commandBindings,platforms,0,1,2,{0x30000001,0x30000002,1,palettes},
    encounters,{1,2,3,4,5,6,7,8,-1,0,{}, {},100},scores,900,901,rewardPlacements,destinations,
    {0x40000001,0x40000002,18,2},0,100
};

std::unique_ptr<coo::script::MissionDocument> document() {
    std::string error;
    auto result=coo::script::MissionDocument::parse(documentText,profile,error);
    if(!result) std::fprintf(stderr,"synthetic mission parse failed: %s\n",error.c_str());
    return result;
}
}

struct FakeRoundPorts final {
    ra::Runtime* runtime{};
    std::array<coo::Command,32> commands{};
    std::array<ra::Operation,32> operations{};
    std::size_t count{};
    bool acceptTravel{true};
    bool publish(ra::Operation operation,const coo::Command& command) noexcept {
        if(count==commands.size())return false;
        commands[count]=command;operations[count++]=operation;
        if(operation==ra::Operation::prepareEntry)return runtime->select_platform(command.token,0);
        if(operation==ra::Operation::progressFull)return runtime->arm_progress(command.token);
        if(operation==ra::Operation::travelEncounter)
            return runtime->prepare_travel(command.token,0x42)
                && (!acceptTravel || runtime->mark_travel_request(true));
        if(operation==ra::Operation::travelArrived)return runtime->arm_travel_arrival(command.token);
        if(operation==ra::Operation::spawnEncounter)
            return runtime->arm_spawn(command.token)
                && runtime->mark_encounter_population_requested(0);
        if(operation==ra::Operation::bossDead)return runtime->arm_boss_dead(command.token);
        if(operation==ra::Operation::retireEncounter)return runtime->begin_retirement(command.token);
        if(operation==ra::Operation::travelEntry)
            return runtime->prepare_return_travel(command.token,901)
                && runtime->mark_travel_request(true);
        if(operation==ra::Operation::travelRewards)
            return runtime->prepare_reward_travel(command.token,901)
                && runtime->mark_travel_request(true);
        return true;
    }
    void cancel(ra::Operation,const coo::Command&) noexcept {}
    [[nodiscard]] const coo::Command* latest(ra::Operation operation) const noexcept {
        for(std::size_t i=count;i--;)if(operations[i]==operation)return &commands[i];
        return nullptr;
    }
};

void runtime_drives_first_branch_and_arena_wait() {
    auto document=synthetic::document();
    expect(static_cast<bool>(document));
    expect(ra::Runtime::valid(synthetic::definition,*document));
    ra::Runtime runtime;
    const sunrise::state::activity::ActivityInstanceKey owner{77,{3}};
    expect(runtime.begin(owner,19,synthetic::definition,*document,100));
    FakeRoundPorts ports{&runtime};

    expect(runtime.update_pending());
    expect(runtime.update(ports,0));
    expect(!runtime.update_pending()); // An untouched plate does not demand a burst.
    expect(runtime.snapshot().selectedPlatform==0);
    const auto* plate=ports.latest(ra::Operation::plateOccupied);
    expect(plate!=nullptr);
    expect(!runtime.enqueue({{owner.sessionId,999,plate->token.step,plate->token.command},coo::Milestone::observed}));
    expect(runtime.enqueue({plate->token,coo::Milestone::observed}));
    expect(runtime.update(ports,1));
    const auto* capture=ports.latest(ra::Operation::capture);
    expect(capture!=nullptr);
    expect(runtime.update(ports,2));
    expect(runtime.round_snapshot().phase==timed::Phase::entry);
    expect(ports.latest(ra::Operation::startTraversal)==nullptr);
    expect(runtime.enqueue({capture->token,coo::Milestone::nativeReady}));
    expect(runtime.update(ports,3));
    expect(runtime.round_snapshot().phase==timed::Phase::entry);
    expect(runtime.enqueue({capture->token,coo::Milestone::completed}));
    expect(runtime.update(ports,4));
    expect(runtime.round_snapshot().phase==timed::Phase::entry);
    expect(ports.latest(ra::Operation::startTraversal)!=nullptr);
    const auto sourceAsset=coo::Asset{0x50000001,0x50000002,37,4};
    const coo::PopulationOwner source{owner.sessionId,19,owner.incarnation.value,sourceAsset,1};
    const coo::PopulationActor actorOne{source,1,11};
    const coo::PopulationActor actorTwo{source,2,22};
    const coo::PopulationActor fake{source,3,33};
    const auto actorOneIdentity=(static_cast<std::uint64_t>(actorOne.actor)<<32)|actorOne.entity;
    const auto actorTwoIdentity=(static_cast<std::uint64_t>(actorTwo.actor)<<32)|actorTwo.entity;
    const auto fakeIdentity=(static_cast<std::uint64_t>(fake.actor)<<32)|fake.entity;
    // A real spawn can arrive during the final plate update. It must score
    // when killed in traversal, without making pre-traversal deaths eligible.
    expect(runtime.admit_generated(actorOne,source,0xABCDEF01,0));
    expect(runtime.generated_death(actorOneIdentity,0)==timed::Result::unsupported);
    expect(runtime.snapshot().unknownScoreEvents==0);
    expect(runtime.update(ports,5));
    expect(runtime.round_snapshot().phase==timed::Phase::traversal);
    expect(runtime.update(ports,6));
    const auto* progress=ports.latest(ra::Operation::progressFull);
    expect(progress!=nullptr);
    expect(runtime.admit_generated(actorOne,source,0xABCDEF01,0));
    expect(runtime.admit_generated(actorTwo,source,0xABCDEF01,0));
    expect(runtime.admit_generated(fake,source,0xDEADBEEF,0));
    expect(runtime.snapshot().unknownScoreEvents==1);
    expect(runtime.retire_generated(fake,source));
    expect(runtime.generated_death(fakeIdentity,10)==timed::Result::unsupported);
    expect(runtime.admit_generated(actorOne,source,0xABCDEF01,0));
    expect(runtime.generated_death(actorOneIdentity,10)==timed::Result::accepted);
    expect(runtime.generated_death(actorOneIdentity,10)==timed::Result::unsupported);
    expect(runtime.update(ports,10));
    expect(runtime.round_snapshot().progress==50 && runtime.round_snapshot().phase==timed::Phase::traversal);
    expect(runtime.update(ports,30));
    expect(!runtime.round_snapshot().expired && !runtime.restricted());
    expect(runtime.round_snapshot().phase==timed::Phase::traversal);
    expect(runtime.generated_death(actorTwoIdentity,30)==timed::Result::accepted);
    expect(runtime.retire_generated(actorOne,source));
    expect(runtime.retire_generated(actorTwo,source));
    expect(runtime.generated_source_retired(source));
    expect(runtime.round_snapshot().progress==100);
    expect(runtime.update(ports,31));
    expect(runtime.round_snapshot().phase==timed::Phase::toEncounter);
    ports.acceptTravel=false;
    expect(runtime.update(ports,32));
    const auto* travel=ports.latest(ra::Operation::travelEncounter);
    const auto* arrival=ports.latest(ra::Operation::travelArrived);
    expect(travel!=nullptr && arrival!=nullptr);
    expect(!runtime.observe_travel(true,true)); // No arrival before this trip was requested.
    expect(!runtime.snapshot().travelArrivalQualified);
    expect(runtime.mark_travel_request(true));
    ports.acceptTravel=true;
    expect(runtime.snapshot().travelRequested && runtime.snapshot().travelCohort!=0);
    expect(runtime.update(ports,33));
    expect(runtime.round_snapshot().phase==timed::Phase::toEncounter);
    expect(!runtime.observe_travel(true,false));
    expect(runtime.update(ports,34));
    expect(runtime.round_snapshot().phase==timed::Phase::toEncounter);
    expect(runtime.observe_travel(false,true));
    expect(runtime.update(ports,35));
    expect(runtime.round_snapshot().phase==timed::Phase::toEncounter);
    expect(runtime.snapshot().travelArrivalQualified);
    expect(!runtime.observe_travel(false,true));
    expect(!runtime.enqueue({{owner.sessionId,arrival->token.incarnation-1,arrival->token.step,arrival->token.command},coo::Milestone::observed}));

    const coo::PopulationOwner bossSource{owner.sessionId,19,owner.incarnation.value,
        synthetic::bossAsset,1};
    const coo::PopulationActor boss{bossSource,101,201};
    expect(runtime.update(ports,36));
    expect(runtime.population_event(0,bossSource,boss,native_population::Kind::admitted,36));
    expect(runtime.population_event(0,bossSource,boss,native_population::Kind::admitted,36));
    expect(runtime.update(ports,37));
    expect(runtime.update(ports,38));
    expect(!runtime.update_pending()); // A living boss is an external wait.
    expect(runtime.population_event(0,bossSource,boss,native_population::Kind::died,40));
    expect(runtime.update_pending());
    expect(runtime.population_event(0,bossSource,boss,native_population::Kind::died,40));
    expect(runtime.boss_dead(boss,bossSource,40)==timed::Result::duplicate);
    const coo::PopulationActor wrongBoss{bossSource,102,202};
    expect(!runtime.population_event(0,bossSource,wrongBoss,native_population::Kind::died,40));
    expect(runtime.update(ports,40));
    expect(runtime.round_snapshot().phase==timed::Phase::returning);
    expect(runtime.update_pending()); // Start the return graph without a keepalive delay.

    // A qualified retirement observation is required before return travel can
    // start; no actor death is used as a retirement acknowledgement.
    expect(runtime.update(ports,41));
    expect(!runtime.update_pending()); // Retirement must still be acknowledged.
    expect(runtime.retirement_acknowledged(true,true));
    expect(runtime.update_pending());
    expect(runtime.update(ports,42));
    expect(runtime.update_pending()); // Join the just-published travel request.
    expect(runtime.update(ports,43));
    expect(!runtime.update_pending()); // Wait for arrival; never manufacture it.
    expect(runtime.observe_travel(true,false)==false);
    expect(runtime.update(ports,44));
    expect(runtime.observe_travel(false,true));
    expect(runtime.update_pending()); // Arrival has queued a receipt after the owner update.
    expect(runtime.update(ports,45));
    expect(runtime.round_snapshot().phase==timed::Phase::entry);
    expect(runtime.round_snapshot().token.round==2);
    expect(runtime.update_pending()); // Publish round two's entry objective promptly.

    // Round two repeats entry and traversal, then deliberately expires while
    // in Terror before the real boss path enters rewards.
    expect(runtime.update(ports,45));
    expect(!runtime.update_pending()); // Back to waiting for the next plate activation.
    const auto* plateTwo=ports.latest(ra::Operation::plateOccupied);
    expect(plateTwo!=nullptr && runtime.enqueue({plateTwo->token,coo::Milestone::observed}));
    expect(runtime.update(ports,46));
    const auto* captureTwo=ports.latest(ra::Operation::capture);
    expect(captureTwo!=nullptr);
    expect(runtime.enqueue({captureTwo->token,coo::Milestone::nativeReady}));
    expect(runtime.update(ports,47));
    expect(runtime.enqueue({captureTwo->token,coo::Milestone::completed}));
    expect(runtime.update(ports,48));
    expect(runtime.update(ports,49));
    expect(runtime.round_snapshot().phase==timed::Phase::traversal);
    expect(runtime.update(ports,50));
    expect(ports.latest(ra::Operation::progressFull)!=nullptr);
    const coo::PopulationOwner sourceTwo{owner.sessionId,19,owner.incarnation.value,sourceAsset,2};
    const coo::PopulationActor actorThree{sourceTwo,4,44};
    const coo::PopulationActor actorFour{sourceTwo,5,55};
    const auto identityThree=(static_cast<std::uint64_t>(actorThree.actor)<<32)|actorThree.entity;
    const auto identityFour=(static_cast<std::uint64_t>(actorFour.actor)<<32)|actorFour.entity;
    expect(runtime.admit_generated(actorThree,sourceTwo,0xABCDEF01,0));

    // Rebinding is allowed to retain a current-source record while the old
    // source is compacted. Fill the remaining bound with old-source records,
    // leaving one live so the retirement proof is exercised at capacity.
    const auto additionalOldActors=ra::kMaximumGeneratedActors-4;
    const auto historicalActor=[&](std::size_t index) {
        return coo::PopulationActor{source,static_cast<std::uint32_t>(1000U+index),
            static_cast<std::uint32_t>(0x60000000U+index)};
    };
    for(std::size_t i=0;i<additionalOldActors;++i) {
        const auto actor=historicalActor(i);
        expect(runtime.admit_generated(actor,source,0xABCDEF01,0));
        if(i+1<additionalOldActors) expect(runtime.retire_generated(actor,source));
    }
    const auto historicalLive=historicalActor(additionalOldActors-1);
    expect(runtime.generated_death(historicalLive,source,0xABCDEF01,0,false,50)==timed::Result::unsupported);
    expect(runtime.round_snapshot().progress==0);
    expect(!runtime.release_retired_generated_source(source,sourceTwo));
    const coo::PopulationOwner foreignSource{owner.sessionId+1,19,owner.incarnation.value,
        sourceAsset,2};
    expect(!runtime.release_retired_generated_source(source,foreignSource));
    expect(!runtime.release_retired_generated_source(source,source));
    expect(runtime.retire_generated(historicalLive,source));
    expect(runtime.release_retired_generated_source(source,sourceTwo));
    expect(runtime.admit_generated(actorFour,sourceTwo,0xABCDEF01,0));

    const auto additionalCurrentActors=ra::kMaximumGeneratedActors-2;
    for(std::size_t i=0;i<additionalCurrentActors;++i) {
        const coo::PopulationActor actor{sourceTwo,static_cast<std::uint32_t>(2000U+i),
            static_cast<std::uint32_t>(0x70000000U+i)};
        expect(runtime.admit_generated(actor,sourceTwo,0xDEADBEEF,0));
    }
    const coo::PopulationActor overCapacity{sourceTwo,4000,0x71000000};
    expect(!runtime.admit_generated(overCapacity,sourceTwo,0xDEADBEEF,0));
    expect(runtime.update(ports,151));
    expect(runtime.round_snapshot().expired && runtime.restricted());
    // A confirmed defeat interrupts the pending branch-progress graph and preserves
    // the earned first branch. No boss was spawned in this second branch: retirement
    // must not wait for a nonexistent encounter before travelling to the rewards.
    {
        auto defeated=std::make_unique<ra::Runtime>(runtime);
        FakeRoundPorts rewardPorts{defeated.get()};
        expect(!defeated->observe_player_life(99,true,1000));
        expect(!defeated->observe_player_life(99,false,1001));
        expect(!defeated->observe_player_life(99,true,2000)); // teammate revive cancels
        expect(!defeated->observe_player_life(99,false,2001));
        expect(!defeated->observe_player_life(99,false,5000));
        expect(defeated->observe_player_life(99,false,5001));
        expect(defeated->update(rewardPorts,152));
        expect(defeated->round_snapshot().phase==timed::Phase::rewards);
        expect(defeated->round_snapshot().completedRounds==1);
        expect(defeated->defeat_reward_travel());
        expect(!defeated->completion().complete);
        for (unsigned clock=153;clock<160;++clock) expect(defeated->update(rewardPorts,clock));
        expect(rewardPorts.latest(ra::Operation::travelRewards)!=nullptr);
        expect(!defeated->observe_travel(true,false));
        expect(defeated->observe_travel(false,true));
        expect(defeated->reward_arrived());
        expect(!defeated->restricted());
        expect(defeated->accept_reward_placements());
        expect(defeated->request_completion(1000,160));
        expect(defeated->update(rewardPorts,161));
        expect(defeated->update(rewardPorts,162));
        expect(defeated->completion().complete);
    }
    expect(runtime.generated_death(identityThree,151)==timed::Result::accepted);
    expect(runtime.generated_death(identityFour,151)==timed::Result::accepted);
    expect(runtime.update(ports,152));
    expect(runtime.round_snapshot().phase==timed::Phase::toEncounter);
    expect(runtime.update(ports,153));
    const auto* arrivalTwo=ports.latest(ra::Operation::travelArrived);
    expect(arrivalTwo!=nullptr);
    expect(runtime.observe_travel(true,false)==false);
    expect(runtime.update(ports,154));
    expect(runtime.observe_travel(false,true));
    expect(runtime.update(ports,155));
    expect(runtime.round_snapshot().phase==timed::Phase::toEncounter);
    expect(runtime.update(ports,156));
    const coo::PopulationOwner bossSourceTwo{owner.sessionId,19,owner.incarnation.value,
        synthetic::bossAsset,2};
    const coo::PopulationActor bossTwo{bossSourceTwo,111,211};
    expect(runtime.population_event(0,bossSourceTwo,bossTwo,native_population::Kind::admitted,156));
    // An expired arena wipe ends the run without requiring or crediting this boss's death.
    {
        auto defeated=std::make_unique<ra::Runtime>(runtime);
        FakeRoundPorts rewardPorts{defeated.get()};
        expect(defeated->update(rewardPorts,157));
        expect(defeated->round_snapshot().phase==timed::Phase::encounter);
        expect(!defeated->observe_player_life(99,true,1000));
        expect(!defeated->observe_player_life(99,false,1001));
        expect(defeated->observe_player_life(99,false,4001));
        expect(defeated->update(rewardPorts,158));
        expect(defeated->round_snapshot().phase==timed::Phase::rewards);
        expect(defeated->round_snapshot().completedRounds==1 && !defeated->boss_death_qualified());
        expect(defeated->retirement_acknowledged(true,true));
        for(unsigned clock=159;clock<163;++clock)expect(defeated->update(rewardPorts,clock));
        expect(!defeated->observe_travel(true,false));
        expect(!defeated->request_completion(1000,163));
        expect(defeated->observe_travel(false,true));
        expect(!defeated->restricted());
        expect(defeated->request_completion(1000,163));
        expect(defeated->update(rewardPorts,163));
        expect(defeated->update(rewardPorts,164));
        expect(defeated->completion().complete && defeated->round_snapshot().phase==timed::Phase::complete);
    }
    // The boss can qualify before the encounter graph has consumed its wait.
    // The timed service must freeze at this proof clock until that wait commits.
    expect(runtime.population_event(0,bossSourceTwo,bossTwo,native_population::Kind::died,156));
    expect(runtime.update(ports,157));
    expect(runtime.round_snapshot().phase==timed::Phase::encounter);
    expect(runtime.update(ports,158));
    expect(runtime.round_snapshot().phase==timed::Phase::encounter);
    expect(runtime.update(ports,159));
    expect(runtime.round_snapshot().phase==timed::Phase::rewards);
    expect(runtime.update(ports,159));
    expect(runtime.retirement_acknowledged(true,true));
    expect(runtime.update(ports,160));
    expect(runtime.update(ports,161));
    expect(runtime.observe_travel(true,false)==false);
    expect(runtime.update(ports,162));
    expect(runtime.observe_travel(false,true));
    expect(runtime.reward_arrived());
    expect(runtime.accept_reward_placements());
    expect(runtime.request_completion(1000,162));
    expect(runtime.update(ports,163));
    expect(runtime.update(ports,164));
    expect(runtime.round_snapshot().phase==timed::Phase::complete);
    expect(runtime.completion().state==6 && runtime.completion().complete);
    expect(runtime.advance_lifecycle(31000));
    expect(runtime.completion().state==7);
    expect(runtime.advance_lifecycle(61000));
    expect(runtime.completion().state==8);
}

// Before expiry, deaths use the authored current-branch entry spawn.
void branch_entry_respawn_remains_available() {
    using sunrise::server::runtime::activity::traversal_respawn_suppressed;
    for (auto phase : {timed::Phase::entry,timed::Phase::traversal,timed::Phase::toEncounter,
            timed::Phase::encounter,timed::Phase::returning,timed::Phase::rewards}) {
        expect(!traversal_respawn_suppressed(true,phase,false));
        expect(!traversal_respawn_suppressed(true,phase,true));
    }
}

int main() {
    definition_defaults_fail_closed();
    branch_entry_respawn_remains_available();
    timed_round_receipts_keep_stale_events_out();
    native_frame_completion_is_absent_by_default();
    persistent_population_reuse_seam();
    runtime_drives_first_branch_and_arena_wait();
    std::printf("round activity runtime: %u checks, zero failures\n", checks);
}
