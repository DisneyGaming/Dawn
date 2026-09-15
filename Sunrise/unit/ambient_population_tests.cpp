#include "../src/server/runtime/activity/ambient_population_activation.h"
#include "../src/server/runtime/activity/ambient_population_definition.h"
#include "../src/server/runtime/activity/mercury_ambient_populations.h"
#include "../src/server/runtime/activity/mercury_ambient_probe.h"
#include "../src/server/runtime/activity/persistent_activity.h"
#include "../src/server/runtime/activity/mercury_definition.h"
#include "../src/state/activity/native_population_events.h"
#include "fixtures/omega_native_sense_captures.h"
#include "mission_parameter_fixture.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <vector>

namespace runtime=sunrise::server::runtime::activity;
namespace ambient=runtime::ambient_population;
namespace mercury=runtime::mercury::ambient;
namespace catalog=mercury::catalog;
namespace population=runtime::population;
using Object=ambient::sense::SenseObject;
static unsigned checks{};
#define CHECK(expression) do { ++checks; if(!(expression)) { std::printf("FAILED line %d: %s\n",__LINE__,#expression); std::exit(1); } } while(false)

Object report(std::uint32_t key,std::uint16_t slot,std::uint32_t revision,
    std::int32_t count,bool all=false,std::int32_t token=0) {
    const auto countCode=static_cast<std::uint32_t>(static_cast<std::int64_t>(count)+2147483648LL);
    const auto tokenCode=static_cast<std::uint32_t>(static_cast<std::int64_t>(token)+2147483648LL);
    Object result{};result.registryKey=key;result.slotIndex=slot;result.slotType=30;
    result.hasNativeSchema=true;result.nativeSchema=0x80809531;result.nativeRevision=revision;
    result.hasRootDelta=true;result.bodyBits=99;
    result.bodyFirst=(std::uint64_t{1}<<63)|(std::uint64_t(count!=0)<<62)|(std::uint64_t(all)<<61)
        |(std::uint64_t(countCode)<<29)|(tokenCode>>3);
    result.bodySecond=(std::uint64_t(tokenCode&7U)<<32)|revision;
    return result;
}
Object unchanged(std::uint32_t key,std::uint16_t slot,std::uint32_t revision) {
    auto result=report(key,slot,revision,0);result.hasRootDelta=false;result.bodyBits=33;
    result.bodyFirst=revision;result.bodySecond=0;return result;
}

void decode_cases() {
    ambient::MonitorDelta decoded{};
    const auto measured=report(0xD00142CF,0,7,1,true);
    CHECK(measured.bodyFirst==0xF000000030000000ULL); // independently captured Omega entered payload
    CHECK(measured.bodySecond==7);
    CHECK(ambient::decode_monitor(measured,decoded));
    CHECK(decoded.any && decoded.all && decoded.selected==1 && decoded.authorityToken==0 && decoded.revision==7);
    for(int n=0;n<=32;++n) for(auto token:{INT32_MIN,-1,0,1,INT32_MAX}) {
        CHECK(ambient::decode_monitor(report(1,2,0x87654321,n,n==0,token),decoded));
        CHECK(decoded.selected==n && decoded.any==(n!=0) && decoded.authorityToken==token);
    }
    CHECK(ambient::decode_monitor(report(1,2,3,0,true),decoded));
    CHECK(decoded.all && !decoded.any); // empty selection is allowed and never occupied
    CHECK(!ambient::decode_monitor(report(1,2,3,33),decoded));
    CHECK(!ambient::decode_monitor(report(1,2,3,-1),decoded));
    auto invalid=report(1,2,3,0);invalid.bodyFirst|=std::uint64_t{1}<<62;
    CHECK(!ambient::decode_monitor(invalid,decoded));
    invalid=measured;invalid.hasNativeSchema=false;CHECK(!ambient::decode_monitor(invalid,decoded));
    invalid=measured;invalid.nativeSchema=0x80807ECC;CHECK(!ambient::decode_monitor(invalid,decoded));
    invalid=measured;invalid.slotType=1;CHECK(!ambient::decode_monitor(invalid,decoded));
    invalid=measured;invalid.inferredBodyWidth=true;CHECK(!ambient::decode_monitor(invalid,decoded));
    invalid=measured;invalid.bodyBits=100;CHECK(!ambient::decode_monitor(invalid,decoded));
    invalid=measured;invalid.nativeRevision=8;CHECK(!ambient::decode_monitor(invalid,decoded));
    invalid=measured;invalid.bodySecond|=std::uint64_t{1}<<35;CHECK(!ambient::decode_monitor(invalid,decoded));
    invalid=measured;invalid.bodyFirst&=~(std::uint64_t{1}<<63);CHECK(!ambient::decode_monitor(invalid,decoded));
    CHECK(ambient::decode_monitor(unchanged(1,2,UINT32_MAX),decoded));
    CHECK(!decoded.root && decoded.revision==UINT32_MAX);
    invalid=unchanged(1,2,3);invalid.bodyFirst|=std::uint64_t{1}<<32;CHECK(!ambient::decode_monitor(invalid,decoded));
    ambient::Monitor monitor;
    CHECK(monitor.observe(unchanged(1,2,1),0)==ambient::MonitorIntake::unchanged);
    CHECK(!monitor.known() && !monitor.occupied());
    CHECK(monitor.observe(report(1,2,2,0,true),0)==ambient::MonitorIntake::accepted);
    CHECK(monitor.known() && !monitor.occupied());
    CHECK(monitor.observe(report(1,2,3,1),0)==ambient::MonitorIntake::accepted);
    CHECK(monitor.occupied());
    CHECK(monitor.observe(unchanged(1,2,4),0)==ambient::MonitorIntake::unchanged);
    CHECK(monitor.occupied());
    CHECK(monitor.observe(report(1,2,3,0),0)==ambient::MonitorIntake::stale);
    CHECK(monitor.observe(report(1,2,4,0),0)==ambient::MonitorIntake::stale);
    CHECK(monitor.observe(report(1,2,5,0,false,1),0)==ambient::MonitorIntake::unrelated);
    CHECK(monitor.occupied() && monitor.revision()==4);
    CHECK(monitor.observe(report(1,2,5,0),0)==ambient::MonitorIntake::accepted);
    CHECK(!monitor.occupied());
    monitor.reset();CHECK(!monitor.known());
    CHECK(monitor.observe(report(1,2,UINT32_MAX,1),0)==ambient::MonitorIntake::accepted);
    CHECK(monitor.observe(report(1,2,0,0),0)==ambient::MonitorIntake::accepted);
    CHECK(!monitor.occupied() && monitor.revision()==0);
    CHECK(monitor.observe(report(1,2,0x80000000,1),0)==ambient::MonitorIntake::stale);
}

void captured_wire_cases() {
    std::size_t monitors{},occupied{};
    for(const auto& capture:sunrise::unit::fixtures::kOmegaNativeSenseCaptures) {
        std::vector<std::byte> bytes;
        const auto nibble=[](char c) {return c>='A'?unsigned(c-'A'+10):unsigned(c-'0');};
        for(std::size_t i=0;i<capture.hex.size();i+=2)
            bytes.push_back(static_cast<std::byte>((nibble(capture.hex[i])<<4)|nibble(capture.hex[i+1])));
        ambient::sense::SenseUpdate update{};std::size_t consumed{};
        CHECK(ambient::sense::parse_sense_update(bytes,update,consumed));
        for(std::size_t i=0;i<update.objectCount;++i) {
            const auto& object=update.objects[i];
            if(object.slotType!=30) continue;
            ambient::MonitorDelta decoded{};
            if(!ambient::decode_monitor(object,decoded))
                std::printf("capture %u monitor %08X:%u bits=%u native=%u root=%u schema=%08X rev=%u first=%016llX second=%016llX inferred=%u\n",
                    capture.packet,object.registryKey,object.slotIndex,object.bodyBits,object.hasNativeSchema,object.hasRootDelta,
                    object.nativeSchema,object.nativeRevision,object.bodyFirst,object.bodySecond,object.inferredBodyWidth);
            CHECK(ambient::decode_monitor(object,decoded));
            ++monitors;occupied+=decoded.root && decoded.any;
        }
    }
    CHECK(monitors>0 && occupied>0);
    std::printf("native capture validation: %zu monitor reports, %zu occupied\n",monitors,occupied);
}

void catalog_cases() {
    CHECK(catalog::kGroups.size()==37 && catalog::kRegistries.size()==37);
    std::size_t sources{},descriptors{},hotspots{};
    for(const auto& group:catalog::kGroups) {
        CHECK(runtime::registry::valid(*group.registry));
        CHECK(group.registry->bubble==15 && group.registry->scenario==0x80F4696A);
        CHECK(group.tacticalRows>0 && group.tacticalRows<=24);
        CHECK(catalog::find(group.registry->key)==&group);
        descriptors+=group.registry->slots.size();hotspots+=group.hotspot;
        for(const auto& source:group.sources) {
            ++sources;
            for(auto rule:{mercury::Rule::primary,mercury::Rule::fallback}) {
                population::Capability capability{};
                for(std::uint8_t row=0;row<group.tacticalRows;++row) {
                    CHECK(mercury::capability(group.registry->key,source.slot,rule,row,capability));
                    CHECK(capability.rule==(rule==mercury::Rule::primary?source.primaryRule:source.fallbackRule));
                    CHECK(capability.tactical.slot==group.tacticalSlot && capability.tactical.row==row);
                    CHECK(ambient::valid({&capability,group.monitorSlot,1,0}));
                }
                CHECK(!mercury::capability(group.registry->key,source.slot,rule,group.tacticalRows,capability));
            }
        }
    }
    CHECK(sources==72 && descriptors==257 && hotspots==15);
    population::Capability output{};
    CHECK(!mercury::capability(0,0,mercury::Rule::primary,0,output));
    CHECK(!mercury::capability(0x74337EDD,100,mercury::Rule::primary,0,output));
    CHECK(!mercury::capability(0x74337EDD,0,static_cast<mercury::Rule>(2),0,output));
}

void activation_cases() {
    std::array<population::Capability,2> capabilities{};
    CHECK(mercury::capability(0x74337EDD,0,mercury::Rule::fallback,0,capabilities[0]));
    CHECK(mercury::capability(0x74337EDD,1,mercury::Rule::fallback,1,capabilities[1]));
    const std::array<ambient::InitialPolicy,2> policies{{{&capabilities[0],4,1,0},{&capabilities[1],4,1,0}}};
    const population::Owner owner{0x1234,{1}};
    population::Service service;
    ambient::InitialActivation gate;
    CHECK(service.begin(owner,capabilities,55));
    CHECK(!gate.begin(owner,0,policies));CHECK(gate.begin(owner,55,policies));
    CHECK(!gate.begin(owner,55,policies));
    CHECK(gate.state(0)==ambient::InitialState::awaitingMonitor);
    CHECK(gate.state(2)==ambient::InitialState::unavailable);
    CHECK(gate.activate(0,service,15)==population::Result::invalid);
    CHECK(service.project(15).count==0);
    CHECK(gate.observe(owner,54,15,report(0x74337EDD,4,1,1))==0);
    CHECK(gate.observe({0x1234,{2}},55,15,report(0x74337EDD,4,1,1))==0);
    CHECK(gate.observe(owner,55,14,report(0x74337EDD,4,1,1))==0);
    CHECK(gate.observe(owner,55,15,report(0x74337EDD,3,1,1))==0);
    CHECK(gate.observe(owner,55,15,report(0x74337EDD,4,1,0,true))==2);
    CHECK(gate.state(0)==ambient::InitialState::unoccupied);
    CHECK(gate.observe(owner,55,15,report(0x74337EDD,4,2,1))==2);
    CHECK(gate.state(0)==ambient::InitialState::eligible);
    CHECK(gate.diagnostics(0).monitorKnown && gate.diagnostics(0).selectedPlayers==1);
    CHECK(gate.diagnostics(0).nativeRevision==2);
    CHECK(gate.activate(0,service,14)==population::Result::stale);
    CHECK(gate.state(0)==ambient::InitialState::eligible);
    CHECK(gate.activate(0,service,15)==population::Result::accepted);
    CHECK(gate.activate(1,service,15)==population::Result::accepted);
    CHECK(service.project(15).count==2 && service.revision()==3 && service.last_request()==2);
    for(int n=0;n<100;++n) CHECK(gate.activate(0,service,15)==population::Result::unchanged);
    CHECK(service.revision()==3 && service.last_request()==2);
    CHECK(gate.observe(owner,55,15,report(0x74337EDD,4,3,0))==2);
    CHECK(gate.state(0)==ambient::InitialState::published);
    gate.reset_observations();
    CHECK(gate.state(0)==ambient::InitialState::published);
    CHECK(!gate.diagnostics(0).monitorKnown);
    CHECK(gate.diagnostics(2).state==ambient::InitialState::unavailable);
    CHECK(gate.observe(owner,55,15,report(0x74337EDD,4,0,1))==2);
    CHECK(gate.activate(0,service,15)==population::Result::unchanged);
    CHECK(service.revision()==3); // no replacement on occupancy, revision reset or revisit
    ambient::InitialActivation duplicate;
    const std::array<ambient::InitialPolicy,2> duplicatePolicies{{policies[0],policies[0]}};
    CHECK(!duplicate.begin(owner,55,duplicatePolicies));
    auto invalid=policies[0];invalid.initialRequests=0;CHECK(!ambient::valid(invalid));
    invalid.initialRequests=64;CHECK(!ambient::valid(invalid));
    invalid.initialRequests=1;invalid.monitorSlot=3;CHECK(!ambient::valid(invalid));
    // The same code supports another profile/activity identity without Mercury
    // names or source choices in the generic service. This is an offline fixture.
    auto secondRegistry=*capabilities[0].registry;secondRegistry.activity="second.fixture";
    secondRegistry.scenario=0x80000011;secondRegistry.key=0x12345678;secondRegistry.bubble=7;
    auto secondCapability=capabilities[0];secondCapability.registry=&secondRegistry;
    secondCapability.tactical.registry=secondRegistry.key;
    const std::array<ambient::InitialPolicy,1> secondPolicies{{{&secondCapability,4,1,9}}};
    population::Service secondService;ambient::InitialActivation<1> secondGate;
    CHECK(secondService.begin({0x4321,{2}},{&secondCapability,1},56));
    CHECK(secondGate.begin(secondService.owner(),56,secondPolicies));
    CHECK(secondGate.observe(secondService.owner(),56,7,report(secondRegistry.key,4,10,1,false,0))==0);
    CHECK(secondGate.observe(secondService.owner(),56,7,report(secondRegistry.key,4,10,1,false,9))==1);
    CHECK(secondGate.activate(0,secondService,7)==population::Result::accepted);
    CHECK(secondService.project(7).count==1);
    CHECK(secondGate.activate(0,service,15)==population::Result::stale);
    CHECK(secondGate.size()==1 && secondGate.policy(0)->initialRequests==1);
    CHECK(secondGate.policy(1)==nullptr);
    // Moving a prepared gate must not retain a policy span into staging storage.
    auto moved=std::move(secondGate);
    CHECK(moved.state(0)==ambient::InitialState::published);
    CHECK(moved.policy(0)->source==&secondCapability && moved.policy(0)->authorityToken==9);
}

void definition_cases() {
    struct Asset {std::uint32_t registry{};std::uint8_t type{};std::uint16_t slot{};};
    struct Command {Asset asset{};};
    struct Step {std::span<const Command> commands;};
    struct GraphDefinition {std::span<const Step> steps;};
    struct Graph {GraphDefinition definition;};
    struct Parameter {std::uint32_t value;};
    struct Views {
        Graph graph;Parameter target;
        const Graph* role(std::string_view name) const {return name=="persistent"?&graph:nullptr;}
        const Parameter* parameter(std::string_view name) const {return name=="probe"?&target:nullptr;}
    };
    struct Document {Views data;const Views& views() const {return data;}};
    struct Definition {
        std::span<const population::Capability> populations;
        std::span<const runtime::registry::Definition> registries;
        std::uint8_t bubble;
    };
    population::Capability capability{};
    CHECK(mercury::capability(0xEB1E8934,0,mercury::Rule::fallback,1,capability));
    auto registry=*capability.registry;
    Definition definition{{&capability,1},{&registry,1},15};
    Document document{{{{}}, {1}}};
    std::array<ambient::InitialBinding,1> bindings{{{0,3,"probe",0}}};
    std::array<ambient::InitialPolicy,32> output{};std::size_t count{};
    CHECK(ambient::configure_initial(definition,document,bindings,output,count));
    CHECK(count==1 && output[0].source==&capability && output[0].initialRequests==1);
    document.data.target.value=0;
    CHECK(ambient::configure_initial(definition,document,bindings,output,count));
    CHECK(count==0 && output[0].source==nullptr);
    document.data.target.value=64;
    CHECK(!ambient::configure_initial(definition,document,bindings,output,count));
    CHECK(count==0 && output[0].source==nullptr);
    document.data.target.value=1;
    bindings[0].capability=1;CHECK(!ambient::configure_initial(definition,document,bindings,output,count));
    bindings[0].capability=0;bindings[0].monitorSlot=2;
    CHECK(!ambient::configure_initial(definition,document,bindings,output,count));
    bindings[0].monitorSlot=3;bindings[0].countParameter="missing";
    CHECK(!ambient::configure_initial(definition,document,bindings,output,count));
    bindings[0].countParameter="probe";
    ++registry.objectTag;CHECK(!ambient::configure_initial(definition,document,bindings,output,count));
    --registry.objectTag;
    std::array<Command,1> commands{{{{0xEB1E8934,1,0}}}};
    std::array<Step,1> steps{{{commands}}};
    document.data.graph.definition.steps=steps;
    CHECK(!ambient::configure_initial(definition,document,bindings,output,count));
    commands[0].asset.slot=1;CHECK(ambient::configure_initial(definition,document,bindings,output,count));
    commands[0].asset.slot=0;commands[0].asset.type=4;
    CHECK(ambient::configure_initial(definition,document,bindings,output,count));
    std::array<ambient::InitialBinding,2> duplicate{{bindings[0],bindings[0]}};
    CHECK(!ambient::configure_initial(definition,document,duplicate,output,count));
    CHECK(count==0 && output[0].source==nullptr);
    definition.bubble=14;CHECK(!ambient::configure_initial(definition,document,bindings,output,count));
    definition.bubble=15;bindings[0].development=true;
    CHECK(ambient::configure_initial(definition,document,bindings,output,count));
    CHECK(count==1 && output[0].development);
    namespace probe=mercury::probe;
    CHECK(population::valid(probe::kPopulation));
    CHECK(probe::kPopulation.registry->key==0xEB1E8934 && probe::kPopulation.slot==0);
    CHECK(probe::kPopulation.rule==6 && probe::kPopulation.tactical.row==1);
    CHECK(probe::binding(3).capability==3 && probe::binding(3).monitorSlot==3 && probe::binding(3).development);
    CHECK(probe::kSourceDefinition==0x80F5B77F && probe::kTemplateEntity==0x80C0D08A && probe::kFiringAreaSet==12);
}

void persistent_integration_cases() {
    namespace coo=sunrise::state::activity::coo;
    namespace events=sunrise::state::activity::native_population;
    std::ifstream file("Sunrise/scripts/mercury_freeroam.json",std::ios::binary);CHECK(file.good());
    std::string text((std::istreambuf_iterator<char>(file)),{}),error;
    CHECK(mission_parameter_fixture::numeric(text,"ambient_vex_probe_count",0));
    CHECK(mission_parameter_fixture::numeric(text,"public_event_rally_probe",0));
    std::shared_ptr<const coo::script::MissionDocument> disabled=coo::script::MissionDocument::parse(text,runtime::mercury::kProfile,error);
    CHECK(disabled && runtime::PersistentActivity::valid(runtime::mercury::kActivity,*disabled));
    runtime::PersistentActivity normal;CHECK(normal.begin({81,{1}},runtime::mercury::kActivity,disabled,17));
    CHECK(normal.ambient().size()==0);
    runtime::NativeActivityFrame frame{};
    for(int i=0;i<4;++i) frame=normal.update(15,true);
    CHECK(frame.populations.count==2); // shipped definition keeps the probe disabled
    CHECK(mission_parameter_fixture::numeric(text,"ambient_vex_probe_count",1));
    std::shared_ptr<const coo::script::MissionDocument> enabled=coo::script::MissionDocument::parse(text,runtime::mercury::kProfile,error);
    CHECK(enabled && runtime::PersistentActivity::valid(runtime::mercury::kActivity,*enabled));
    runtime::PersistentActivity activity;const population::Owner owner{82,{2}};
    CHECK(activity.begin(owner,runtime::mercury::kActivity,enabled,18));
    CHECK(activity.ambient().size()==1 && activity.ambient().policy(0)->development);
    for(int i=0;i<4;++i) frame=activity.update(15,true);
    CHECK(frame.populations.count==2 && activity.ambient().state(0)==ambient::InitialState::awaitingMonitor);
    CHECK(activity.ambient().observe(owner,18,15,report(0xEB1E8934,3,1,0))==1);
    CHECK(activity.update(15,true).populations.count==2);
    CHECK(activity.ambient().observe(owner,18,15,report(0xEB1E8934,3,2,1))==1);
    // Leaving the region or loading retains already published owner-scoped
    // authority, while the eligible probe must wait for arrival in its bubble.
    const auto retainedRevision=activity.population().revision();
    const auto retainedRequest=activity.population().last_request();
    const auto checkRetained=[&](const runtime::NativeActivityFrame& retained) {
        CHECK(retained.populations.count==frame.populations.count);
        CHECK(retained.placements.count==frame.placements.count);
        for(std::size_t i=0;i<retained.populations.count;++i) {
            const auto& actual=retained.populations.entries[i];
            const auto& expected=frame.populations.entries[i];
            CHECK(actual.slot==expected.slot && actual.bubble==expected.bubble);
            CHECK(actual.source.registry==expected.source.registry);
            CHECK(actual.source.generation==expected.source.generation);
            CHECK(actual.source.looseRequested==expected.source.looseRequested);
        }
        CHECK(activity.population().revision()==retainedRevision);
        CHECK(activity.population().last_request()==retainedRequest);
        CHECK(activity.ambient().state(0)==ambient::InitialState::eligible);
    };
    checkRetained(activity.update(14,true));
    checkRetained(activity.update(15,false));
    frame=activity.update(15,true);
    CHECK(frame.populations.count==3 && frame.placements.count==9);
    const auto& request=frame.populations.entries[2];
    CHECK(request.source.registry==0xEB1E8934 && request.slot==0 && request.source.looseRequested==1);
    CHECK(request.source.ruleSlot==6 && request.source.tactical.slot==1 && request.source.tactical.row==1);
    CHECK(activity.ambient().state(0)==ambient::InitialState::published);
    const auto revision=activity.population().revision();
    const coo::PopulationOwner source{owner.sessionId,18,owner.incarnation.value,{0xEB1E8934,0x80F5B77F,1,0},request.source.generation};
    const events::Lease lease{owner,source,15};
    auto mailbox=std::make_unique<events::Mailbox>();coo::NativePopulationLedger<16> ledger;
    CHECK(mailbox->bind(lease) && ledger.begin(source));
    const coo::PopulationActor actor{source,0x123456,0x234567};
    CHECK(mailbox->submit({lease,actor,0x345678,events::Kind::admitted},mailbox->epoch()));
    CHECK(mailbox->submit({lease,actor,0x345678,events::Kind::died},mailbox->epoch()));
    std::array<events::Event,4> received{};CHECK(mailbox->drain(owner,received)==2);
    CHECK(ledger.admitted(received[0].actor)==coo::PopulationIntake::accepted);
    CHECK(ledger.died(received[1].actor)==coo::PopulationIntake::accepted);
    CHECK(ledger.counts().dead==1 && ledger.counts().alive==0 && ledger.counts().resident==1);
    for(int i=0;i<10;++i) CHECK(activity.update(15,true).populations.count==3);
    CHECK(activity.population().revision()==revision);
    CHECK(activity.ambient().observe(owner,18,15,report(0xEB1E8934,3,3,0))==1);
    CHECK(activity.update(15,true).populations.count==3);
    CHECK(activity.ambient().observe(owner,18,15,report(0xEB1E8934,3,4,1))==1);
    CHECK(activity.update(15,true).populations.count==3 && activity.population().revision()==revision);
    CHECK(mailbox->submit({lease,actor,0x345678,events::Kind::retired},mailbox->epoch()));
    CHECK(mailbox->drain(owner,received)==1);
    CHECK(ledger.actor_retired(received[0].actor)==coo::PopulationIntake::accepted);
    CHECK(ledger.counts().resident==0 && !ledger.counts().sourceRetired);
    CHECK(activity.update(15,true).populations.count==3 && activity.population().revision()==revision);
    mailbox->release(owner);CHECK(!mailbox->submit({lease,actor,0x345678,events::Kind::died},mailbox->epoch()));
    runtime::PersistentActivity reentry;const population::Owner newOwner{82,{3}};
    CHECK(reentry.begin(newOwner,runtime::mercury::kActivity,enabled,18));
    CHECK(reentry.ambient().state(0)==ambient::InitialState::awaitingMonitor);
    CHECK(reentry.ambient().observe(owner,18,15,report(0xEB1E8934,3,5,1))==0);
    CHECK(reentry.ambient().observe(newOwner,18,15,report(0xEB1E8934,3,1,1))==1);
    for(int i=0;i<4;++i) frame=reentry.update(15,true);
    CHECK(frame.populations.count==3 && frame.populations.entries[2].source.generation==3);
}

void cabal_persistent_cases() {
    namespace coo=sunrise::state::activity::coo;
    namespace points=ambient::named_points;
    namespace probe=runtime::mercury::ambient::cabal_probe;
    std::ifstream file("Sunrise/scripts/mercury_freeroam.json",std::ios::binary);CHECK(file.good());
    std::string text((std::istreambuf_iterator<char>(file)),{}),error;
    CHECK(mission_parameter_fixture::numeric(text,"ambient_vex_probe_count",0));
    CHECK(mission_parameter_fixture::numeric(text,"public_event_rally_probe",0));
    CHECK(mission_parameter_fixture::numeric(text,"ambient_cabal_primary_probe_count",0));
    auto disabled=coo::script::MissionDocument::parse(text,runtime::mercury::kProfile,error);CHECK(disabled);
    ambient::RegistryBatch registries{};
    CHECK(ambient::optional_registries(runtime::mercury::kActivity,*disabled,registries));CHECK(registries.count==0);
    runtime::PersistentActivity normal;const population::Owner disabledOwner{300,{1}};
    CHECK(normal.begin(disabledOwner,runtime::mercury::kActivity,std::move(disabled),100));
    // The shipped profile now owns source0 as an ordinary fallback escort.
    // It does not need the diagnostic primary-rule named-point owner.
    CHECK(normal.request_population({disabledOwner,1,1,0x2571C34D,0,1,100},15)==population::Result::accepted);
    const auto ordinary=normal.population().project_retained();CHECK(ordinary.count==1);
    CHECK(ordinary.entries[0].source.ruleSlot==9 && ordinary.entries[0].source.tactical.row==1);
    CHECK(!points::lookup(probe::kNamedDependency.list).binding.epoch);
    // Keep the primary-rule probe as an isolated fixture, never as a second
    // activation path for the production fallback source.
    auto probePopulations=runtime::mercury::kPopulations;
    probePopulations[22]=probe::kPopulation;
    probePopulations[22].registry=runtime::mercury::kPopulations[22].registry;
    const std::array<ambient::InitialBinding,2> probeBindings{{
        runtime::mercury::ambient::probe::binding(7),probe::binding(22)}};
    auto probeDefinition=runtime::mercury::kActivity;
    probeDefinition.populations=probePopulations;probeDefinition.ambientInitial=probeBindings;
    CHECK(mission_parameter_fixture::numeric(text,"ambient_cabal_primary_probe_count",1));
    auto enabled=coo::script::MissionDocument::parse(text,runtime::mercury::kProfile,error);
    CHECK(enabled && runtime::PersistentActivity::valid(probeDefinition,*enabled));
    CHECK(ambient::optional_registries(probeDefinition,*enabled,registries));CHECK(registries.count==1);
    runtime::PersistentActivity activity;const population::Owner owner{301,{2}};
    CHECK(activity.begin(owner,probeDefinition,std::move(enabled),101));
    CHECK(activity.ambient().size()==1 && activity.ambient().policy(0)->source->registry->key==0x2571C34D);
    runtime::NativeActivityFrame frame{};
    for(int i=0;i<4;++i)frame=activity.update(15,true);
    CHECK(frame.populations.count==2);
    CHECK(activity.request_population({owner,activity.population().revision(),10,0x2571C34D,0,1,101},15)==population::Result::unsupported);
    CHECK(activity.request_population({owner,activity.population().revision(),10,0x4A3E4900,0,1,101},15)==population::Result::unsupported);
    CHECK(activity.ambient().observe(owner,101,15,report(0x2571C34D,4,1,1))==1);
    CHECK(activity.ambient().state(0)==ambient::InitialState::awaitingDependency);
    CHECK(activity.update(15,true).populations.count==2);
    const auto binding=points::lookup(probe::kNamedDependency.list).binding;
    for(std::size_t i=0;i<probe::kPoints.size();++i) {
        CHECK(points::constructed(binding,probe::kPoints[i],static_cast<std::uint32_t>(400+i)));
        if(i<2)CHECK(activity.update(15,true).populations.count==2);
    }
    frame=activity.update(15,true);CHECK(frame.populations.count==3);
    const auto& source=frame.populations.entries[2];
    CHECK(source.source.registry==0x2571C34D && source.slot==0 && source.source.ruleSlot==8);
    CHECK(source.source.looseRequested==1 && source.source.tactical.slot==2 && source.source.tactical.row==0);
    const auto revision=activity.population().revision();
    CHECK(points::removing(binding));
    for(std::uint32_t i=2;i<12;++i) {
        CHECK(activity.ambient().observe(owner,101,15,report(0x2571C34D,4,i,static_cast<int>(i&1)))==1);
        CHECK(activity.update(15,true).populations.count==3 && activity.population().revision()==revision);
    }
    points::release(owner);CHECK(!points::lookup(probe::kNamedDependency.list).binding.epoch);
}

void freeroam_profile_cases() {
    namespace coo=sunrise::state::activity::coo;
    std::ifstream file("Sunrise/scripts/mercury_freeroam.json",std::ios::binary);CHECK(file.good());
    std::string text((std::istreambuf_iterator<char>(file)),{}),error;
    const auto document=coo::script::MissionDocument::parse(text,runtime::mercury::kProfile,error);
    CHECK(document && runtime::PersistentActivity::valid(runtime::mercury::kActivity,*document));
    CHECK(runtime::mercury::kActivity.registries.size()==20);
    CHECK(runtime::mercury::kActivity.populations.size()==32);
    CHECK(runtime::mercury::kActivity.ambientInitial.size()==1);
    for(const auto& capability:runtime::mercury::kActivity.populations) {
        CHECK(population::valid(capability));bool registered{};
        for(const auto& registry:runtime::mercury::kActivity.registries)
            if(capability.registry==&registry)registered=true;
        for(const auto& binding:runtime::mercury::kActivity.optionalRegistries)
            if(capability.registry==binding.registry)registered=true;
        CHECK(registered);
    }
    CHECK(text.find("\"freeroam_respawn_ms\": 30000")!=std::string::npos);
    CHECK(text.find("\"freeroam_normal_patrol_count\": 1")!=std::string::npos);
    CHECK(text.find("\"freeroam_large_patrol_count\": 1")!=std::string::npos);
    CHECK(text.find("\"faction_war_wave_4_count\": 5")!=std::string::npos);
}

int main() {
    decode_cases();captured_wire_cases();catalog_cases();activation_cases();definition_cases();
    persistent_integration_cases();cabal_persistent_cases();freeroam_profile_cases();
    std::printf("ambient_population_tests: %u checks passed\n",checks);
}
