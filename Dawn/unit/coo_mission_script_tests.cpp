#include "state/activity/coo/mission_script.h"
#include "state/activity/coo/script_value.h"
#include "state/activity/coo/script_lua.h"
#include <algorithm>
#include "fixtures/mission_semantics.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>
namespace c=dawn::state::activity::coo;
namespace s=c::script;
unsigned checks{};
#define CHECK(x) do { ++checks;if(!(x)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);std::exit(1); } } while(false)
constexpr c::Asset pump{91,101,0,0},display{92,102,0,0},camera{256,512,6,2},actors{257,513,2,3};
constexpr s::Capability capabilities[]{
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
constexpr c::DialogueRow rows[]{{0x400,20,0,false}};
constexpr std::uint32_t objectives[]{0x500};
constexpr s::EventCapability events[]{{"signals","power_on",99,0}};
const s::Profile profile{"lab.native.v1","labSchema",c::Schema::otherMissions,capabilities,modules,facts,{0x300,rows,{}},objectives,events,{}};
// Test-only Lua literal emitter for mutated neutral definition trees.
void encode(const s::value::Value& value,std::ostream& out) {
    using K=s::value::Value::Kind;
    switch(value.kind) {
    case K::object:out<<'{';for(std::size_t i=0;i<value.members.size();++i) { if(i)out<<',';out<<'['<<std::quoted(value.members[i].first)<<"]=";encode(value.members[i].second,out); }out<<'}';break;
    case K::array:out<<"array{";for(std::size_t i=0;i<value.items.size();++i) { if(i)out<<',';encode(value.items[i],out); }out<<'}';break;
    case K::string:out<<std::quoted(value.text);break;
    case K::number:out<<value.number;break;
    case K::boolean:out<<(value.boolean?"true":"false");break;
    }
}
s::value::Value string_value(std::string_view text) { s::value::Value result;result.kind=s::value::Value::Kind::string;result.text=text;return result; }
s::value::Value& field(s::value::Value& v,std::string_view key) { for(auto& item:v.members) { if(item.first==key)return item.second; }std::abort(); }
s::value::Value& graph(s::value::Value& v,std::string_view key) { return field(field(v,"graphs"),key); }
std::string encode(const s::value::Value& value) { std::ostringstream stream;stream<<"return ";encode(value,stream);return stream.str(); }
void reject(const s::value::Value& value) { std::string error;CHECK(!s::MissionDocument::parse_lua(encode(value),profile,error));CHECK(!error.empty()); }
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
void authoring_conditions() {
    auto caps=std::vector<s::Capability>(std::begin(capabilities),std::end(capabilities));
    caps[5].domain="*";caps[6].domain="*";
    caps.push_back({"left.entered","*",{c::Operation::observation,{400,401,0,0},1,c::Wait::observed}});
    caps.push_back({"right.entered","*",{c::Operation::observation,{400,402,0,0},2,c::Wait::observed}});
    caps.push_back({"timer.elapsed","*",{c::Operation::eventAfter,{400,403,0,0},3,c::Wait::observed},100});
    auto flexible=profile;flexible.capabilities=caps;
    const std::string declarations=R"lua(
local ready=condition('ready',any_of('left.entered',all_of('right.entered','timer.elapsed')))
local joined=condition('joined',all_of('ready','right.entered'))
)lua";
    const std::string graphs=R"lua(
local launch=graph('launch','Composition',{step('run',parallel('pump','pump_ready'))},{domain='composition'})
local first=graph('first','Authored first',{step('wait',command('ready'))})
local second=graph('second','Authored second',sequence(step('actors','population'),step('camera','scene')))
return mission{id='lab',graphs={second,launch,first},entry='launch',modules={'pump'},
observations={{fact='pump_running',receipt='pump_ready'}},conditions={ready,joined},
phases={'second','first'},observation_start='left.entered'}
)lua";
    std::string error;const auto source=declarations+graphs;
    auto document=s::MissionDocument::parse_lua(source,flexible,error);
    if(!document) { std::fprintf(stderr,"%s\n",error.c_str()); }CHECK(document);
    const auto& views=document->views();CHECK(s::authorized(views,flexible));
    auto serialized=s::lua::evaluate(source,flexible,"conditions.lua");
    auto literalConditions=s::MissionDocument::parse_lua(encode(serialized),flexible,error);CHECK(literalConditions);
    CHECK(literalConditions->views().phases[0]->id=="second");CHECK(s::authorized(literalConditions->views(),flexible));
    CHECK(literalConditions->views().conditions.size()==views.conditions.size());
    CHECK(views.phases.size()==2);CHECK(views.phases[0]->id=="second");CHECK(views.phases[1]->id=="first");
    CHECK(views.observationStart && views.observationStart->asset==caps[7].spec.asset);
    const auto ready=views.graph("first")->definition.steps[0].commands[0];
    CHECK(views.condition(ready));CHECK(views.condition(ready)->id=="ready");
    const s::ConditionView* joined{};for(const auto& value:views.conditions) { if(value.id=="joined") { joined=&value; } }CHECK(joined);
    // Exercise every truth assignment. The callback sees only registered native
    // observations, including eventAfter; it never receives the synthetic key.
    for(unsigned bits=0;bits<8;++bits) {
        unsigned queries{};
        const auto observe=[&](const c::CommandSpec& spec) {
            CHECK(spec.asset!=s::kConditionAsset);CHECK(c::is_observation(spec.operation));++queries;
            for(unsigned i=0;i<3;++i) { if(spec.asset==caps[7+i].spec.asset) { return (bits&(1U<<i))!=0; } }
            CHECK(false);return false;
        };
        const bool expected=(bits&1U) || ((bits&2U) && (bits&4U));
        CHECK(views.evaluate(ready,observe)==expected);CHECK(queries>0 && queries<=3);
        CHECK(views.evaluate(joined->spec,observe)==(expected && (bits&2U)));
        CHECK(!views.evaluate(caps[5].spec,observe));
    }
    auto forged=ready;forged.argument=999;CHECK(!views.evaluate(forged,[](const c::CommandSpec&){CHECK(false);return true;}));
    CHECK(!s::authorized(views,profile)); // Wildcard authority must be explicitly registered.
    Service services;c::Executor executor;const auto& definition=views.graph("first")->definition;
    CHECK(executor.start(definition,987));executor.update(services);CHECK(services.sent.size()==1);
    const auto published=services.sent[0];CHECK(s::valid_token(definition,executor,published));
    auto altered=published;altered.token.run++;CHECK(!s::valid_token(definition,executor,altered));
    altered=published;altered.token.incarnation++;CHECK(!s::valid_token(definition,executor,altered));
    altered=published;altered.spec.asset.definition--;CHECK(!s::valid_token(definition,executor,altered));
    altered=published;altered.spec.argument++;CHECK(!s::valid_token(definition,executor,altered));
    altered=published;altered.spec.wait=c::Wait::requested;CHECK(!s::valid_token(definition,executor,altered));
    altered=published;altered.spec.operation=c::Operation::eventAfter;CHECK(!s::valid_token(definition,executor,altered));
    altered=published;altered.schema=c::Schema::omegaArchive;CHECK(!s::valid_token(definition,executor,altered));
    executor.cancel(services);CHECK(executor.start(definition,987));CHECK(!s::valid_token(definition,executor,published));executor.cancel(services);
    const auto rejectLua=[&](std::string bad) { CHECK(!s::MissionDocument::parse_lua(bad,flexible,error));CHECK(!error.empty()); };
    const auto replace=[&](std::string old,std::string replacement) { auto result=source;const auto at=result.find(old);CHECK(at!=std::string::npos);result.replace(at,old.size(),replacement);return result; };
    rejectLua(replace("'left.entered',all_of", "'missing.observation',all_of"));
    rejectLua(replace("'left.entered',all_of", "'scene',all_of"));
    rejectLua(replace("'left.entered',all_of", "'joined',all_of")); // Named reference cycle.
    rejectLua(replace("conditions={ready,joined}", "conditions={joined}"));
    rejectLua(replace("phases={'second','first'}", "phases={'missing'}"));
    rejectLua(replace("phases={'second','first'}", "phases={'second','second'}"));
    rejectLua(replace("phases={'second','first'}", "phases={'launch'}"));
    rejectLua(replace("phases={'second','first'}", "phases={'first','second','first','second','first','second','first','second','first'}"));
    rejectLua(replace("observation_start='left.entered'", "observation_start='scene'"));
    rejectLua(replace("observation_start='left.entered'", "observation_start='ready'"));
    rejectLua(replace("any_of('left.entered',all_of('right.entered','timer.elapsed'))", "any_of()"));
    rejectLua(replace("step('run',parallel('pump','pump_ready'))", "step('run',parallel('pump','pump_ready','scene'))"));
    rejectLua(replace("command('ready')", "command('ready',{id='override',argument=1})"));
    std::string nested="'left.entered'";for(unsigned i=0;i<7;++i) { nested="all_of("+nested+")"; }
    auto deep=s::MissionDocument::parse_lua(replace("any_of('left.entered',all_of('right.entered','timer.elapsed'))",nested),flexible,error);
    CHECK(!deep);
    // A second named condition would add depth, so test the boundary independently.
    auto bounded=replace("any_of('left.entered',all_of('right.entered','timer.elapsed'))",nested);
    const auto joinedAt=bounded.find("all_of('ready','right.entered')");bounded.replace(joinedAt,std::string("all_of('ready','right.entered')").size(),"'right.entered'");
    auto boundary=s::MissionDocument::parse_lua(bounded,flexible,error);if(!boundary) { std::fprintf(stderr,"%s\n",error.c_str()); }CHECK(boundary);
    rejectLua(replace("any_of('left.entered',all_of('right.entered','timer.elapsed'))","all_of("+nested+")"));
    rejectLua("local terms={};for i=1,65 do terms[i]='left.entered' end;return condition('too_many',{any_of=terms})");
}
int main() {
    authoring_conditions();
    std::ifstream input("Dawn/unit/fixtures/mission_script_alternate.lua",std::ios::binary);CHECK(input.good());
    const std::string text((std::istreambuf_iterator<char>(input)),{});std::string error;
    auto reference=s::MissionDocument::parse_lua(text,profile,error);CHECK(reference);
    auto doc=s::MissionDocument::read("Dawn/unit/fixtures/mission_script_alternate.lua",profile,error);
    if(!doc)std::fprintf(stderr,"%s\n",error.c_str());CHECK(doc);CHECK(error.empty());
    CHECK(mission_test::semantics(doc->views())==mission_test::semantics(reference->views()));
    CHECK(doc->views().phases.empty());CHECK(!doc->views().observationStart);CHECK(doc->views().conditions.empty());
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
    const auto original=s::lua::evaluate(text,profile,"alternate.lua");auto reversed=original;
    auto& steps=field(graph(reversed,"room"),"steps").items;std::reverse(steps.begin(),steps.end());
    auto reordered=s::MissionDocument::parse_lua(encode(reversed),profile,error);CHECK(reordered);
    CHECK(executor.start(reordered->views().graph("room")->definition,72));executor.update(services);
    CHECK(executor.token("scene.ready").step!=firstScene.step);CHECK(executor.token("scene.ready").incarnation!=firstScene.incarnation);
    CHECK(!executor.enqueue({firstScene,c::Milestone::nativeReady}));CHECK(!executor.enqueue({firstPopulation,c::Milestone::completed}));
    CHECK(executor.enqueue({executor.token("scene.ready"),c::Milestone::nativeReady}));executor.update(services);
    CHECK(executor.receipt_state("scene.ready").phase==c::StepPhase::complete);executor.cancel(services);
    // Add a new native operation occurrence and receipt using authored Lua alone. No
    // compiled step count, names, ordering or graph templates exist for this profile.
    auto expanded=original;auto extra=field(graph(expanded,"room"),"steps").items[0];
    field(extra,"id").text="second_camera";field(field(extra,"commands").items[0],"id").text="another_scene";
    auto after=string_value("actors");field(extra,"after").items.push_back(after);
    field(graph(expanded,"room"),"steps").items.push_back(extra);
    field(graph(expanded,"room"),"receipts").members.emplace_back("second.scene.ready",string_value("another_scene"));
    auto added=s::MissionDocument::parse_lua(encode(expanded),profile,error);CHECK(added);CHECK(added->views().graph("room")->definition.steps.size()==3);
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
    bad=original;field(field(field(bad,"assets"),"scene"),"registry").text="0xFFFFFFFF";reject(bad);
    bad=original;field(graph(bad,"room"),"domain").text="composition";reject(bad);
    bad=original;field(field(graph(bad,"room"),"receipts"),"scene.ready").text="missing";reject(bad);
    bad=original;field(field(graph(bad,"room"),"receipts"),"scene.ready").text="population";reject(bad);
    bad=original;field(graph(bad,"room"),"receipts").members.pop_back();reject(bad);
    bad=original;field(field(field(graph(bad,"room"),"steps").items[0],"commands").items[0],"id").text="population";reject(bad);
    bad=original;field(field(graph(bad,"room"),"steps").items[0],"after").items.push_back(string_value("camera"));reject(bad);
    bad=original;field(field(graph(bad,"room"),"steps").items[0],"after").items.push_back(string_value("missing"));reject(bad);
    bad=original;field(field(graph(bad,"room"),"steps").items[0],"after").items.push_back(string_value("actors"));
    field(field(graph(bad,"room"),"steps").items[1],"after").items.push_back(string_value("camera"));reject(bad);
    bad=original;field(bad,"modules").items.push_back(field(bad,"modules").items[0]);reject(bad);
    bad=original;field(field(bad,"observations").items[0],"receipt").text="pump.running";reject(bad);
    bad=original;field(bad,"entry").text="room";reject(bad);
    bad=original;field(field(bad,"roles"),"encounter").text="missing";reject(bad);
    bad=original;auto& tooMany=field(graph(bad,"room"),"steps").items;const auto example=tooMany[0];while(tooMany.size()<33)tooMany.push_back(example);reject(bad);
    auto invalidProfile=profile;invalidProfile.schema=c::Schema::unspecified;CHECK(!s::MissionDocument::parse_lua(text,invalidProfile,error));
    auto duplicateCaps=std::vector<s::Capability>(std::begin(capabilities),std::end(capabilities));duplicateCaps.push_back(duplicateCaps[0]);invalidProfile=profile;invalidProfile.capabilities=duplicateCaps;CHECK(!s::MissionDocument::parse_lua(text,invalidProfile,error));
    for(std::size_t length=0;length+32<text.size();length+=97) { CHECK(!s::MissionDocument::parse_lua(std::string_view(text).substr(0,length),profile,error)); }
    CHECK(!s::MissionDocument::parse_lua(std::string(1048577,' '),profile,error));
    CHECK(!s::MissionDocument::parse_lua(text+"{}",profile,error));
    // Simultaneously retained documents do not share mutable global selections.
    CHECK(doc->views().graph("room")->definition.steps.size()==2);CHECK(added->views().graph("room")->definition.steps.size()==3);
    std::printf("PASS: %u checks; independent Lua profile, forward references, parallel native receipts, added authored steps, stale tokens, rejection and document isolation\n",checks);
}
