#include "state/activity/coo/omega_script.h"
#include "state/activity/coo/omega_opening.h"
#include "state/activity/coo/omega_forest.h"
#include "state/activity/coo/omega_definition.h"
#include "state/activity/coo/omega_ending_controller.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <algorithm>
using namespace sunrise::state::activity::coo;
namespace sc=sunrise::state::activity::coo::script;
unsigned checks{};
#define CHECK(x) do { ++checks;if(!(x)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);std::exit(1); } } while(false)
std::string altered(std::string text,std::string_view from,std::string_view to) {
    const auto pos=text.find(from);if(pos==std::string::npos) { std::fprintf(stderr,"Missing replacement: %.*s\n",static_cast<int>(from.size()),from.data()); }CHECK(pos!=std::string::npos);text.replace(pos,from.size(),to);return text;
}
void reject(const std::string& text) {
    std::string error;CHECK(!sc::Document::parse_lua(text,error));CHECK(!error.empty());CHECK(sc::current()==nullptr);
}
std::string mutated(const std::string& source,std::string_view program) {
    auto result=altered(source,"return mission{","local authored = mission{");
    result+='\n';result+=program;result+="\nreturn authored\n";return result;
}
constexpr std::string_view kRenameAndReorder=R"lua(
local function reverse(items)
    for i=1,#items//2 do items[i],items[#items+1-i]=items[#items+1-i],items[i] end
end
local renamed={}
for id,graph in pairs(authored.graphs) do
    renamed["renamed_"..id]=graph
    for _,step in ipairs(graph.steps) do
        step.id="renamed_"..step.id
        for i,dependency in ipairs(step.after) do step.after[i]="renamed_"..dependency end
        local waits=false
        for _,command in ipairs(step.commands) do
            command.id="renamed_"..command.id
            waits=waits or authored.bindings[command.binding].wait~="requested"
        end
        if waits then reverse(step.commands) end
    end
    for name,command in pairs(graph.receipts) do graph.receipts[name]="renamed_"..command end
    reverse(graph.steps)
end
for role,id in pairs(authored.roles) do authored.roles[role]="renamed_"..id end
for i,id in ipairs(authored.phases) do authored.phases[i]="renamed_"..id end
 authored.entry="renamed_"..authored.entry
 authored.graphs=renamed
)lua";
int main(int argc,char** argv) {
    if(argc==3 && std::string_view(argv[1])=="--validate") {
        std::string error;const auto doc=sc::Document::read(argv[2],error);
        if(!doc) { std::fprintf(stderr,"INVALID: %s\n",error.c_str());return 1; }
        std::printf("VALID: Omega Lua, native profile verified, FNV1a64 %016llX\n",static_cast<unsigned long long>(doc->fingerprint()));return 0;
    }
    if(argc==2 && std::string_view(argv[1])=="--invalid-admission") {
        const sc::Views invalid{};CHECK(sc::publish(invalid));
        Executor executor;CHECK(!executor.start(sc::graph("opening", omega::opening::kDefinition),1));CHECK(executor.diagnostics().failure==Failure::definition);
        sunrise::state::activity::omega_presentation::Run presentation;presentation.initialize(1,true);
        omega::forest::Sequence forest;forest.start(1,presentation,0);CHECK(forest.failed());
        struct Ports:MissionPorts<unsigned> {
            unsigned calls{};
            void update_module(std::uint32_t,const MissionInput&,unsigned&) noexcept override { ++calls; }
            std::uint32_t observations(std::uint64_t,const unsigned&) noexcept override { return 0; }
        } ports;
        MissionRuntime runtime;static_cast<void>(runtime.update(sc::mission(omega::kMission),{1,0,0,false,true},ports));
        CHECK(runtime.selected());CHECK(ports.calls==0);CHECK(runtime.diagnostics().failure==Failure::definition);
        std::printf("PASS: invalid script blocks executor admission without invoking native publishers\n");return 0;
    }
    std::ifstream file("Sunrise/scripts/omega.lua",std::ios::binary);CHECK(file.good());
    std::string text((std::istreambuf_iterator<char>(file)),{});text.erase(std::remove(text.begin(),text.end(),'\r'),text.end());std::string error;
    auto document=sc::Document::parse_lua(text,error);CHECK(document);CHECK(error.empty());
    const auto& views=document->views();CHECK(views.valid);CHECK(views.graphs.size()==14);CHECK(sc::current()==nullptr);
    for(const auto& graph:views.graphs) {
        const Definition* native{};
        if(graph.id=="mission")native=&omega::kMission.sequence;
        else if(graph.id=="opening")native=&omega::opening::kDefinition;
        else if(graph.id=="forest")native=&omega::forest::kDefinition;
        else if(graph.id=="reveal")native=&reveal::kDefinition;
        else if(graph.id=="reveal_retry")native=&reveal::kRetry;
        else if(graph.id=="ending")native=&ending::kDefinition;
        else if(graph.id=="ending_retry")native=&ending::kRetry;
        else for(std::size_t n=0;n<combat::kSections.size();++n) { if(graph.id==combat::kSectionRoles[n])native=combat::kSections[n]; }
        CHECK(native);const auto& a=graph.definition;const auto& b=*native;
        CHECK(&a!=&b);CHECK(Executor::valid(a));CHECK(a.name==b.name);CHECK(a.schema==b.schema);CHECK(a.steps.size()==b.steps.size());
        for(std::size_t i=0;i<a.steps.size();++i) {
            CHECK(a.steps[i].name==b.steps[i].name);CHECK(a.steps[i].dependencies==b.steps[i].dependencies);CHECK(a.steps[i].commands.size()==b.steps[i].commands.size());
            for(std::size_t j=0;j<a.steps[i].commands.size();++j) {
                const auto& x=a.steps[i].commands[j];const auto& y=b.steps[i].commands[j];
                CHECK(x.asset==y.asset);CHECK(x.operation==y.operation);CHECK(x.wait==y.wait);CHECK(x.argument==y.argument);
            }
        }
    }
    namespace p=sunrise::state::activity::omega_presentation;
    CHECK(views.dialogue.bank==p::kDialogueBank);CHECK(views.dialogue.rows.size()==p::kDialogueRows);
    for(std::size_t i=0;i<p::kDialogueRows;++i) {
        const auto a=views.dialogue.rows[i],b=p::kDialogue[i];
        CHECK(a.selector==b.selector);CHECK(a.durationMs==b.durationMs);CHECK(a.delayMs==b.delayMs);CHECK(a.sceneOwned==b.sceneOwned);
    }
    auto compareActions=[](auto a,auto b) { CHECK(a.size()==b.size());for(std::size_t i=0;i<a.size();++i) { CHECK(a[i].operation==b[i].operation);CHECK(a[i].value==b[i].value);CHECK(a[i].delayMs==b[i].delayMs); } };
    auto compareCues=[&](auto a,auto b) { CHECK(a.size()==b.size());for(std::size_t i=0;i<a.size();++i) { CHECK(a[i].event==b[i].event);CHECK(a[i].cycles==b[i].cycles);compareActions(a[i].actions,b[i].actions); } };
    compareCues(views.cues("landmarks"),std::span(p::cues::kLandmark));compareCues(views.cues("encounters"),std::span(p::cues::kEncounter));
    compareActions(views.actions("reveal_complete"),std::span(p::cues::kRevealComplete));compareActions(views.actions("first_rescue_followup"),std::span(p::cues::kFirstRescueFollowup));
    reject("");reject(text.substr(0,text.size()/2));reject(text+"{}");reject(std::string(1048577,' '));
    reject("while true do end");reject("return os.execute('forbidden')");
    for(const auto program:{
        "authored.format_version=99", "authored.unknown=0", "authored.format_version=true",
        "authored.authority_schema='otherMissions'", "authored.mission='not_omega'",
        "authored.assets['opening/Ikora scene ready/0'].registry='0xD00142CE'",
        "authored.bindings['mission/mission services/0'].argument=99",
        "authored.graphs.mission.steps[2].after={'opening and Ikora'}",
        "authored.graphs.mission.steps[2].after={'mission services','mission services'}",
        "authored.graphs.mission.steps[2].after={'missing'}",
        "authored.graphs.opening.steps[5].after={'roster admitted'}",
        "authored.graphs.opening.receipts['ikora.approached']=nil",
        "authored.presentation.cue_sets.landmarks[1].cycles={1}",
        "authored.presentation.dialogue.rows[2].duration_ms=300001",
        "authored.presentation.dialogue.rows[2].native_delay_ms=1001",
        "authored.presentation.dialogue.dispatch_timeout_ms=0",
        "authored.presentation.dialogue.spacing_ms=0.5",
        "authored.presentation.dialogue.spacing_ms=4294967296",
        "authored.graphs.reveal.domain='opening'",
        "authored.modules[1],authored.modules[2]=authored.modules[2],authored.modules[1]"
    }) { reject(mutated(text,program)); }
    reject(altered(text,"mission/mission services/0","missing/binding"));
    CHECK(!sc::Document::read("Sunrise/scripts/does-not-exist.lua",error));
    // Rename every graph, step and command in Lua; preserve native capability
    // and receipt names while reversing independent steps and command slots.
    auto reordered=sc::Document::parse_lua(mutated(text,kRenameAndReorder),error);
    if(!reordered) { std::fprintf(stderr,"reordered: %s\n",error.c_str()); }CHECK(reordered);
    CHECK(reordered->views().role("opening")->id=="renamed_opening");
    CHECK(reordered->views().role("opening")->definition.steps[1].name=="renamed_Forest entrance observed");
    // A valid observation still cannot replace another native receipt's owner.
    auto wrong=std::string(kRenameAndReorder)+R"lua(
local receipts=authored.graphs.renamed_opening.receipts
receipts['ikora.approached']=receipts['forest.entered']
)lua";
    reject(mutated(text,wrong));
    // Timing edits feed both the landmark and graph-driven presentation paths.
    auto edits=std::string(kRenameAndReorder)+R"lua(
authored.presentation.dialogue.rows[7].duration_ms=9000
authored.presentation.dialogue.spacing_ms=500
authored.graphs.renamed_mission.name='Omega authored in Lua'
for _,cue in ipairs(authored.presentation.cue_sets.landmarks) do
    for _,action in ipairs(cue.actions) do
        if action.operation=='dialogue' and action.value==6 then action.delay_ms=2250 end
    end
end
authored.presentation.binding_tables.forest.dialogue[1].delay_ms=2250
)lua";
    auto edited=sc::Document::parse_lua(mutated(text,edits),error);
    if(!edited) { std::fprintf(stderr,"edited: %s\n",error.c_str()); }CHECK(edited);
    CHECK(edited->views().dialogue.rows[6].durationMs==9000);CHECK(edited->views().dialogue.spacingMs==500);
    CHECK(edited->views().table("forest")->dialogue[0].delayMs==2250);CHECK(edited->views().mission.sequence.name=="Omega authored in Lua");
    CHECK(edited->activate());CHECK(!document->activate());
    CHECK(sc::graph("mission", omega::kMission.sequence).name=="Omega authored in Lua");
    p::Run run;run.initialize(42,true);run.enter(p::Landmark::tunnel,1000);run.advance(3249);CHECK(run.presentation().activeRow==p::kNoDialogue);
    run.advance(3250);CHECK(run.presentation().activeRow==6);
    p::Run legacy;legacy.initialize(43,false);legacy.enter(p::Landmark::tunnel,1000);legacy.advance(2500);CHECK(legacy.presentation().activeRow==6);
    omega::forest::Sequence forest;p::Run forestRun;forestRun.initialize(44,true);forest.start(44,forestRun,1000);forest.enter(p::Landmark::tunnel,forestRun,1000);
    forestRun.advance(3249);CHECK(forestRun.presentation().activeRow==p::kNoDialogue);forestRun.advance(3250);CHECK(forestRun.presentation().activeRow==6);
    forest.stop(forestRun,4000);
    // Direct Forest entry moved from native step 5 to compiled step 1.
    omega::opening::Run openingRun;CHECK(openingRun.enqueue({55,1,omega::opening::Kind::roster}));
    static_cast<void>(openingRun.update(55));CHECK(openingRun.enqueue({55,2,omega::opening::Kind::entrance}));
    static_cast<void>(openingRun.update(55));CHECK(openingRun.authority().entrance);CHECK(!openingRun.authority().failed);
    // Reveal's readiness command changed command index, including retry.
    reveal::Intro intro;intro.reset(56,true);intro.request(100);
    intro.observe(0,false,true,true,false,101);intro.observe(0,false,true,true,true,102);
    CHECK(intro.phase()==p::IntroPhase::offered);intro.observe(intro.command().revision,false,true,true,true,1200);
    CHECK(intro.phase()==p::IntroPhase::priming);intro.observe(0,false,true,true,true,1201);
    intro.observe(intro.command().revision,true,true,true,true,1202);CHECK(intro.phase()==p::IntroPhase::playing);
    intro.observe(intro.command().revision,false,true,true,true,1203);CHECK(intro.phase()==p::IntroPhase::complete);
    namespace nativeEnding=sunrise::state::activity::omega_ending;
    ending::Controller movie;const nativeEnding::Token owner{57,20,0x77F4200CU,1};
    CHECK(movie.request(owner,100,true));movie.advance(101,true);CHECK(movie.observe_retirement(owner,102));movie.advance(102,false);
    CHECK(movie.observe_arrival(owner,nativeEnding::kSlice,103));movie.advance(103,false);
    movie.observe(owner,0,false,true,104);movie.advance(104,false);CHECK(movie.phase()==nativeEnding::Phase::offered);
    movie.observe(owner,movie.authority().revision,false,true,1200);movie.advance(1200,false);CHECK(movie.phase()==nativeEnding::Phase::preparing);
    movie.observe(owner,0,false,true,1201);movie.advance(1201,false);
    movie.observe(owner,movie.authority().revision,true,true,1202);movie.advance(1202,false);CHECK(movie.phase()==nativeEnding::Phase::playing);
    movie.observe(owner,movie.authority().revision,false,true,1203);movie.advance(1203,false);CHECK(movie.authority().complete);
    CHECK(movie.claim_handoff(owner));CHECK(movie.note_handoff_result(owner,true,1204));movie.finish_handoff(1204);
    CHECK(movie.handoff()==nativeEnding::Handoff::queued);CHECK(movie.diagnostics().phase==Phase::complete);

    std::printf("PASS: %u checks; Lua/native parity, malformed input rejection, named ordering, immutable publication and live presentation edits\n",checks);
}
