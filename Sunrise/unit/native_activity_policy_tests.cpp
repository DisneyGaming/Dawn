#include "state/activity/coo/mission_script.h"
#include "state/activity/coo/script_json.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>
namespace c=sunrise::state::activity::coo;
namespace s=c::script;
unsigned checks{};
#define CHECK(x) do { ++checks;if(!(x)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);std::exit(1); } } while(false)
constexpr c::Asset pump{91,101,0,0},display{92,102,0,0},camera{256,512,6,2},actors{257,513,2,3};
constexpr s::Capability fixtureCapabilities[]{
    {"pump","composition",{c::Operation::mechanic,pump,17,c::Wait::requested}},
    {"display","composition",{c::Operation::mechanic,display,44,c::Wait::requested}},
    {"pump_ready","composition",{c::Operation::observation,{},6,c::Wait::observed}},
    {"display_ready","composition",{c::Operation::observation,{},12,c::Wait::observed}},
    {"confirmed","composition",{c::Operation::observation,{},9,c::Wait::observed}},
    {"scene","room",{c::Operation::scene,camera,7,c::Wait::nativeReady}},
    {"population","room",{c::Operation::population,actors,2,c::Wait::completed}}
};
constexpr s::ModuleCapability modules[]{{"pump",{pump,17}},{"display",{display,44}}};
constexpr s::FactCapability facts[]{{"pump_running",6},{"display_running",12},{"operator_confirmed",9}};
constexpr c::DialogueRow fixtureDialogueRows[]{{0x400,20,0,false}};
constexpr std::uint32_t objectives[]{0x500};
constexpr s::EventCapability fixtureEvents[]{{"signals","power_on",99,0}};
const s::Profile profile{"lab.native.v1","labSchema",c::Schema::otherMissions,fixtureCapabilities,modules,facts,{0x300,fixtureDialogueRows,{}},objectives,fixtureEvents,{}};
void encode(const s::json::Value& value,std::ostream& out) {
    using K=s::json::Value::Kind;
    switch(value.kind) {
    case K::object:out<<'{';for(std::size_t i=0;i<value.members.size();++i) { if(i)out<<',';out<<std::quoted(value.members[i].first)<<':';encode(value.members[i].second,out); }out<<'}';break;
    case K::array:out<<'[';for(std::size_t i=0;i<value.items.size();++i) { if(i)out<<',';encode(value.items[i],out); }out<<']';break;
    case K::string:out<<std::quoted(value.text);break;
    case K::number:out<<value.number;break;
    case K::boolean:out<<(value.boolean?"true":"false");break;
    }
}
s::json::Value& field(s::json::Value& v,std::string_view key) { for(auto& item:v.members) { if(item.first==key)return item.second; }std::abort(); }
s::json::Value& graph(s::json::Value& v,std::string_view key) { return field(field(v,"graphs"),key); }
std::string encode(const s::json::Value& value) { std::ostringstream stream;encode(value,stream);return stream.str(); }
void reject(const s::json::Value& value) { std::string error;CHECK(!s::MissionDocument::parse(encode(value),profile,error));CHECK(!error.empty()); }
struct Service final:c::Services {
    std::vector<c::Command> sent,cancelled;
    bool publish(const c::Command& command) noexcept override { sent.push_back(command);return command.schema==c::Schema::otherMissions; }
    void cancel(const c::Command& command) noexcept override { cancelled.push_back(command); }
};
struct Ports final:c::MissionPorts<unsigned> {
    std::vector<std::uint32_t> calls;
    std::uint32_t facts{};
    void update_module(std::uint32_t id,const c::MissionInput& input,unsigned& output) noexcept override { CHECK(input.executor);calls.push_back(id);++output; }
    std::uint32_t observations(std::uint64_t,const unsigned&) noexcept override { return facts; }
};
#include "activity_live_definition_cases.h"
#include "native_population_ledger_cases.h"
#include "native_combatant_source_cases.h"
#include "activity_registry_cases.h"
#include "population_service_cases.h"
#include "persistent_activity_cases.h"
#include "native_population_event_cases.h"
#include "native_replication_role_cases.h"
int main() {
    persistent_activity_cases();
    native_population_event_cases();
    native_replication_role_cases();
    std::ifstream input("Sunrise/unit/fixtures/native_policy_alternate.json",std::ios::binary);CHECK(input.good());
    const std::string text((std::istreambuf_iterator<char>(input)),{});std::string error;
    auto doc=s::MissionDocument::parse(text,profile,error);if(!doc)std::fprintf(stderr,"%s\n",error.c_str());CHECK(doc);CHECK(error.empty());
    auto fromFile=s::MissionDocument::read_native_policy("Sunrise/unit/fixtures/native_policy_alternate.json",profile,error);CHECK(fromFile);
    CHECK(fromFile->same_structure(*doc));
    CHECK(!s::MissionDocument::read("Sunrise/unit/fixtures/native_policy_alternate.json",profile,error));
    CHECK(error.find(".lua extension")!=std::string::npos);
    CHECK(!s::MissionDocument::read_native_policy("Sunrise/unit/fixtures/mission_script_alternate.lua",profile,error));
    CHECK(error.find(".json extension")!=std::string::npos);
    live_definition_cases(s::json::Reader(text).parse());
    native_population_cases();
    native_combatant_source_cases();
    activity_registry_cases();
    population_service_cases();
    CHECK(doc->views().missionId=="pump_station");CHECK(doc->views().profileId==profile.id);CHECK(doc->views().graphs.size()==2);
    CHECK(doc->views().role("encounter")->id=="room");CHECK(doc->views().mission.sequence.schema==c::Schema::otherMissions);
    CHECK(doc->views().mission.sequence.steps.size()==4);CHECK(doc->views().mission.sequence.steps[0].name=="publish");
    CHECK(doc->views().cues("signals")[0].event==99);CHECK(doc->views().actions("mission_success")[0].value==0x500);
    Ports ports;c::MissionRuntime runtime;ports.facts=(1U<<6)|(1U<<12)|(1U<<9);
    CHECK(runtime.update(doc->views().mission,{71,100,0,false,true},ports)==2);
    CHECK(ports.calls[0]==44 && ports.calls[1]==17);
    static_cast<void>(runtime.update(doc->views().mission,{71,101,0,false,true},ports));CHECK(runtime.diagnostics().phase==c::Phase::complete);
    runtime.reset();
    Service services;c::Executor executor;const auto& room=doc->views().graph("room")->definition;
    CHECK(executor.start(room,72));executor.update(services);CHECK(services.sent.size()==2);
    const auto firstScene=executor.token("scene.ready"),firstPopulation=executor.token("population.finished");
    CHECK(firstScene.step!=firstPopulation.step);CHECK(executor.token("missing").step==UINT8_MAX);
    CHECK(!executor.enqueue({executor.token("missing"),c::Milestone::nativeReady}));
    CHECK(executor.enqueue({firstPopulation,c::Milestone::completed}));executor.update(services);
    CHECK(!executor.receipt_state("population.finished").commands[firstPopulation.command].completed);
    CHECK(executor.enqueue({firstScene,c::Milestone::nativeReady}));
    CHECK(executor.enqueue({firstPopulation,c::Milestone::nativeReady}));CHECK(executor.enqueue({firstPopulation,c::Milestone::completed}));executor.update(services);
    CHECK(executor.diagnostics().phase==c::Phase::complete);executor.cancel(services);CHECK(services.cancelled.size()==2);
    const auto original=s::json::Reader(text).parse();auto reversed=original;
    auto& steps=field(graph(reversed,"room"),"steps").items;std::reverse(steps.begin(),steps.end());
    auto reordered=s::MissionDocument::parse(encode(reversed),profile,error);CHECK(reordered);
    CHECK(executor.start(reordered->views().graph("room")->definition,72));executor.update(services);
    CHECK(executor.token("scene.ready").step!=firstScene.step);CHECK(executor.token("scene.ready").incarnation!=firstScene.incarnation);
    CHECK(!executor.enqueue({firstScene,c::Milestone::nativeReady}));CHECK(!executor.enqueue({firstPopulation,c::Milestone::completed}));
    CHECK(executor.enqueue({executor.token("scene.ready"),c::Milestone::nativeReady}));executor.update(services);
    CHECK(executor.receipt_state("scene.ready").phase==c::StepPhase::complete);executor.cancel(services);
    // Add a new native operation occurrence and receipt using JSON alone. No
    // compiled step count, names, ordering or graph templates exist for this profile.
    auto expanded=original;auto extra=field(graph(expanded,"room"),"steps").items[0];
    field(extra,"id").text="second_camera";field(field(extra,"commands").items[0],"id").text="another_scene";
    auto after=s::json::Reader("\"actors\"").parse();field(extra,"after").items.push_back(after);
    field(graph(expanded,"room"),"steps").items.push_back(extra);
    field(graph(expanded,"room"),"receipts").members.emplace_back("second.scene.ready",s::json::Reader("\"another_scene\"").parse());
    auto added=s::MissionDocument::parse(encode(expanded),profile,error);CHECK(added);CHECK(added->views().graph("room")->definition.steps.size()==3);
    CHECK(executor.start(added->views().graph("room")->definition,73));executor.update(services);
    CHECK(executor.enqueue({executor.token("population.finished"),c::Milestone::nativeReady}));CHECK(executor.enqueue({executor.token("population.finished"),c::Milestone::completed}));executor.update(services);
    CHECK(executor.receipt_state("second.scene.ready").phase==c::StepPhase::active);CHECK(executor.token("second.scene.ready").step!=executor.token("scene.ready").step);
    CHECK(executor.enqueue({executor.token("scene.ready"),c::Milestone::nativeReady}));CHECK(executor.enqueue({executor.token("second.scene.ready"),c::Milestone::nativeReady}));executor.update(services);
    CHECK(executor.diagnostics().phase==c::Phase::complete);executor.cancel(services);
    // Invalid references, cyclic dependency, mixed domain/schema and ambiguous
    // named receipts must fail at load time, before native publication.
    auto bad=original;field(bad,"profile").text="wrong";reject(bad);
    bad=original;field(bad,"authority_schema").text="wrong";reject(bad);
    bad=original;field(field(field(bad,"bindings"),"scene"),"argument").number=999;reject(bad);
    bad=original;field(field(field(bad,"bindings"),"scene"),"capability").text="unimplemented";reject(bad);
    bad=original;field(field(field(bad,"assets"),"camera"),"registry").text="0xFFFFFFFF";reject(bad);
    bad=original;field(graph(bad,"room"),"domain").text="composition";reject(bad);
    bad=original;field(field(graph(bad,"room"),"receipts"),"scene.ready").text="missing";reject(bad);
    bad=original;field(field(graph(bad,"room"),"receipts"),"scene.ready").text="population";reject(bad);
    bad=original;field(graph(bad,"room"),"receipts").members.pop_back();reject(bad);
    bad=original;field(field(field(graph(bad,"room"),"steps").items[0],"commands").items[0],"id").text="population";reject(bad);
    bad=original;field(field(graph(bad,"room"),"steps").items[0],"after").items.push_back(s::json::Reader("\"camera\"").parse());reject(bad);
    bad=original;field(field(graph(bad,"room"),"steps").items[0],"after").items.push_back(s::json::Reader("\"missing\"").parse());reject(bad);
    bad=original;field(field(graph(bad,"room"),"steps").items[0],"after").items.push_back(s::json::Reader("\"actors\"").parse());
    field(field(graph(bad,"room"),"steps").items[1],"after").items.push_back(s::json::Reader("\"camera\"").parse());reject(bad);
    bad=original;field(bad,"modules").items.push_back(field(bad,"modules").items[0]);reject(bad);
    bad=original;field(field(bad,"observations").items[0],"receipt").text="pump.running";reject(bad);
    bad=original;field(bad,"entry").text="room";reject(bad);
    bad=original;field(field(bad,"roles"),"encounter").text="missing";reject(bad);
    bad=original;auto& tooMany=field(graph(bad,"room"),"steps").items;const auto example=tooMany[0];while(tooMany.size()<33)tooMany.push_back(example);reject(bad);
    auto invalidProfile=profile;invalidProfile.schema=c::Schema::unspecified;CHECK(!s::MissionDocument::parse(text,invalidProfile,error));
    auto duplicateCaps=std::vector<s::Capability>(std::begin(fixtureCapabilities),std::end(fixtureCapabilities));duplicateCaps.push_back(duplicateCaps[0]);invalidProfile=profile;invalidProfile.capabilities=duplicateCaps;CHECK(!s::MissionDocument::parse(text,invalidProfile,error));
    for(std::size_t length=0;length+32<text.size();length+=97) { CHECK(!s::MissionDocument::parse(std::string_view(text).substr(0,length),profile,error)); }
    CHECK(!s::MissionDocument::parse(std::string(1048577,' '),profile,error));
    CHECK(!s::MissionDocument::parse(text+"{}",profile,error));
    // Simultaneously retained documents do not share mutable global selections.
    CHECK(doc->views().graph("room")->definition.steps.size()==2);CHECK(added->views().graph("room")->definition.steps.size()==3);
    std::printf("PASS: %u checks; generic profiles, live revisions, native population retirement, source codecs, and existing graph contracts\n",checks);
}
