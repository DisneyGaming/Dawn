#include "server/runtime/activity/vance_animation_capability.h"
#include "server/runtime/activity/persistent_activity.h"
#include "server/runtime/activity/mercury_definition.h"
#include "middleware/encoding/bit_writer.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

namespace p=sunrise::server::runtime::activity::npc_animation;
namespace m=sunrise::server::runtime::activity::mercury;
namespace n=sunrise::middleware::bap::activity_message::native::npc_animation;
namespace bits=sunrise::middleware::encoding::bits;
namespace roster=sunrise::middleware::bap::activity_message::sensor_auth_update;
static unsigned checks{};
static void check(bool value,int line) {
    ++checks;if(!value) {std::fprintf(stderr,"NPC animation failed at %d\n",line);std::exit(1);}
}
#define CHECK(value) check((value),__LINE__)

static void vectors(const char* path) {
    std::ifstream input(path);CHECK(input.good());std::string line;unsigned count{};
    while(std::getline(input,line)) {
        if(line.empty()) continue;
        std::istringstream fields(line);n::Control control;std::size_t expectedBits{};std::string expectedHex;
        fields>>std::hex>>control.sequence>>control.completion>>std::dec>>control.counter>>expectedBits>>expectedHex;
        CHECK(!fields.fail());
        std::array<std::byte,32> packet{};bits::Writer writer(packet);
        CHECK(n::write(writer,control));CHECK(writer.bit_count()==expectedBits);
        std::size_t written{};CHECK(writer.finish(written));CHECK(written*2==expectedHex.size());
        for(std::size_t i=0;i<written;++i) {
            const auto expected=std::stoul(expectedHex.substr(i*2,2),nullptr,16);
            CHECK(std::to_integer<unsigned>(packet[i])==expected);
        }
        ++count;
    }
    CHECK(count==7);
    auto measuring=bits::Writer::measuring();
    CHECK(!n::write(measuring,{0x010B0F07,n::kEmptyHash,0}) && measuring.bit_count()==0);
    CHECK(!n::write(measuring,{n::kEmptyHash,0,1}) && measuring.bit_count()==0);
    CHECK(!n::write(measuring,{1,2,0x80000000}) && measuring.bit_count()==0);
    std::array<std::byte,12> shortBuffer{};bits::Writer shortWriter(shortBuffer);
    CHECK(!n::write(shortWriter,{1,2,1}));
}

static void service() {
    CHECK(p::valid(m::kVanceAnimationCapabilities[0]));
    p::Service service;
    CHECK(service.begin({42,{7}},99,m::kVanceAnimationCapabilities));
    CHECK(service.project(15).count==0);
    p::Command command{{42,{7}},99,1,1,0x564C6ECE,1,2,false,false};
    // Only the qualified Vance action was promoted. Unknown/unverified actions
    // retain the normal service rejection, independently of that capability.
    auto unverifiedActions=m::kVanceAnimationActions;unverifiedActions[0].retailVerified=false;
    std::array<p::Capability,1> unverified{{{&m::kRegistries[1],2,unverifiedActions}}};
    p::Service untrusted;CHECK(untrusted.begin({42,{7}},99,unverified));
    CHECK(untrusted.request(command,15)==p::Result::unverified);
    CHECK(untrusted.revision()==1 && untrusted.project(15).count==0);
    auto wrong=command;wrong.boot=98;CHECK(service.request(wrong,15)==p::Result::stale);
    wrong=command;wrong.owner.incarnation.value=8;CHECK(service.request(wrong,15)==p::Result::stale);
    wrong=command;wrong.expectedRevision=2;CHECK(service.request(wrong,15)==p::Result::stale);
    wrong=command;wrong.slot=0;CHECK(service.request(wrong,15)==p::Result::unsupported);
    wrong=command;wrong.action=2;CHECK(service.request(wrong,15)==p::Result::unsupported);
    CHECK(service.request(command,14)==p::Result::stale);
    CHECK(service.request(command,15)==p::Result::accepted);
    CHECK(!command.development);
    CHECK(service.request(command,15)==p::Result::duplicate);
    CHECK(service.project(14).count==0);
    CHECK((service.project(15).entries[0].control==n::Control{0x010B0F07,n::kEmptyHash,1}));
    for(unsigned i=2;i<=1000;++i) {
        command.request=i;command.expectedRevision=i;
        CHECK(service.request(command,15)==p::Result::accepted);
        CHECK(service.project(15).entries[0].control.counter==i);
    }
    command.request=1001;command.expectedRevision=1001;command.action=0;command.stop=true;
    CHECK(service.request(command,15)==p::Result::accepted);
    CHECK((service.project(15).entries[0].control==n::Control{n::kEmptyHash,n::kEmptyHash,1000}));
    command.request=1002;command.expectedRevision=1002;
    CHECK(service.request(command,15)==p::Result::unchanged);
    CHECK(service.revision()==1002);

    // A different destination uses precisely the same service. Capability values
    // are copied, so changing the caller's temporary policy cannot retarget it.
    auto definition=m::kRegistries[1];definition.activity="second_destination";
    definition.scenario=17;definition.key=1234;definition.bubble=4;
    auto action=m::kVanceAnimationActions;action[0]={5,0x12345678,0xFEDCBA98,true};
    std::array<p::Capability,1> caps{{{&definition,2,action}}};
    p::Service second;CHECK(second.begin({88,{2}},123,caps));
    action[0].sequence=0;definition.key=9999;
    p::Command next{{88,{2}},123,1,1,1234,5,2,false,false};
    CHECK(second.request(next,4)==p::Result::accepted);
    CHECK(second.project(4).entries[0].control.sequence==0x12345678);
    CHECK(second.request(command,4)==p::Result::stale);
    p::Service newSession;CHECK(newSession.begin({42,{7}},100,m::kVanceAnimationCapabilities));
    CHECK(newSession.request(command,15)==p::Result::stale);
    CHECK(newSession.project(15).count==0);
    auto badCaps=m::kVanceAnimationCapabilities;badCaps[0].slot=1;
    p::Service invalid;CHECK(!invalid.begin({1,{1}},1,badCaps));CHECK(!invalid.owner());
}

static void validation() {
    std::array<std::uint8_t,3> types{1,70,42},flags{3,1,2};
    std::array<std::uint16_t,3> indices{0,1,2};
    std::array<std::uint32_t,1> keys{0x564C6ECE};
    std::array<roster::BubbleSubBlock,1> blocks{{{15,keys}}};
    roster::Roster layout{};layout.groupCount=1;layout.groups[0]={keys[0],types,flags,indices};layout.bubbleSubBlocks=blocks;
    n::Batch batch{};batch.count=1;batch.entries[0]={keys[0],2,15,{0x010B0F07,n::kEmptyHash,1}};
    CHECK(n::valid(batch,layout,120));CHECK(!n::valid(batch,layout,112));
    CHECK(n::find(batch,keys[0],42,2)!=nullptr);CHECK(n::find(batch,keys[0],1,2)==nullptr);
    types[2]=1;CHECK(!n::valid(batch,layout,120));types[2]=42;
    flags[2]=3;CHECK(!n::valid(batch,layout,120));flags[2]=2;
    auto invalid=batch;invalid.entries[1]=invalid.entries[0];invalid.count=2;CHECK(!n::valid(invalid,layout,120));
    invalid=batch;invalid.count=17;CHECK(!n::valid(invalid,layout,120));CHECK(!n::find(invalid,keys[0],42,2));
    invalid=batch;invalid.entries[0].control.counter=0;CHECK(!n::valid(invalid,layout,120));
    blocks[0].bubble=14;CHECK(!n::valid(batch,layout,120));blocks[0].bubble=15;
    layout.groups[1]=layout.groups[0];layout.groupCount=2;CHECK(!n::valid(batch,layout,120));
}
static void mailbox() {
    const std::string good="npc1 63 2A 7 1 1 564C6ECE 2 1\n";
    p::Command command{};CHECK(p::parse(good,command));
    CHECK(command.boot==99 && command.owner.sessionId==42 && command.owner.incarnation.value==7);
    CHECK(command.expectedRevision==1 && command.request==1 && command.registry==0x564C6ECE);
    CHECK(command.slot==2 && command.action==1 && !command.stop && command.development);
    CHECK(p::parse(" \t npc1 63 2A 7 1 2 564C6ECE 2 0 \r\n",command));CHECK(command.stop);
    for(const auto bad:{"npc1", "npc1 0 2A 7 1 1 564C6ECE 2 1", "npc1 63 0 7 1 1 564C6ECE 2 1",
        "npc1 63 2A 0 1 1 564C6ECE 2 1", "npc1 63 2A 7 0 1 564C6ECE 2 1",
        "npc1 63 2A 7 1 0 564C6ECE 2 1", "npc1 63 2A 7 1 1 FFFFFFFF 2 1",
        "npc1 63 2A 7 1 1 564C6ECE 32768 1", "npc1 63 2A 7 1 1 564C6ECE 2 -1",
        "npc1 63 2A 7 1 1 564C6ECE 2 4294967296", "npc1 63 2A 7 1 1 564C6ECE 2 1 extra",
        "npc1 63 2A 7 1 1 564C6ECE 2 1x", "npc1 10000000000000000 2A 7 1 1 564C6ECE 2 1"}) {
        const auto before=command;
        CHECK(!p::parse(bad,command));CHECK(command.request==before.request && command.action==before.action);
    }
    namespace population=sunrise::server::runtime::activity::population;
    population::Command pop{};
    CHECK(!population::parse(good,pop));
    CHECK(!p::parse("v2 63 2A 7 1 1 564C6ECE 0 1",command));
    CHECK(population::parse("v2 63 2A 7 1 1 564C6ECE 0 1",pop));
    CHECK(pop.boot==99 && pop.slot==0 && pop.requested==1);
}

static void packets() {
    std::array<std::uint8_t,1> types{42},flags{2};std::array<std::uint16_t,1> indices{2};
    std::array<std::uint32_t,1> keys{0x564C6ECE};
    std::array<roster::BubbleSubBlock,1> blocks{{{15,keys}}};
    roster::Snapshot baseline{};baseline.lifetime=3;baseline.hasRegion=true;baseline.region=120;
    baseline.roster.groupCount=1;baseline.roster.groups[0]={keys[0],types,flags,indices};
    baseline.roster.bubbleSubBlocks=blocks;
    for(const auto archive:{false,true}) {
        baseline.archiveOmega=archive;
        std::array<std::byte,1024> packet{},before{};std::size_t baselineSize{},activeSize{};
        CHECK(roster::encode_sensor_auth_update(baseline,before,baselineSize));
        auto active=baseline;active.animations.count=1;active.animations.entries[0]={keys[0],2,15,{0x010B0F07,n::kEmptyHash,1}};
        CHECK(roster::auth_body_bits(active,keys[0],42,2,false)==101);
        auto measure=bits::Writer::measuring();CHECK(roster::write_auth_body(measure,active,keys[0],42,2,false));
        CHECK(measure.bit_count()==101);
        CHECK(roster::encode_sensor_auth_update(active,packet,activeSize));CHECK(activeSize>baselineSize);
        auto again=active;again.animations={};std::size_t restored{};
        CHECK(roster::encode_sensor_auth_update(again,packet,restored));CHECK(restored==baselineSize && packet==before);
        for(unsigned variant=0;variant<6;++variant) {
            auto bad=active;
            switch(variant) {
            case 0:bad.animations.entries[0].slot=0;break;
            case 1:bad.animations.entries[0].bubble=14;break;
            case 2:bad.animations.entries[0].control.counter=0;break;
            case 3:bad.animations.entries[1]=bad.animations.entries[0];bad.animations.count=2;break;
            case 4:bad.animations.count=17;break;
            default:bad.region=112;break;
            }
            packet.fill(std::byte{0xA5});std::size_t written=999;
            CHECK(!roster::encode_sensor_auth_update(bad,packet,written));CHECK(written==0);
            CHECK(std::all_of(packet.begin(),packet.end(),[](std::byte b){return b==std::byte{0xA5};}));
        }
    }
}

static void persistent(const char* scriptPath) {
    namespace a=sunrise::server::runtime::activity;
    namespace c=sunrise::state::activity::coo;
    std::ifstream input(scriptPath);CHECK(input.good());
    const std::string text((std::istreambuf_iterator<char>(input)),{});std::string error;
    std::shared_ptr<const c::script::MissionDocument> doc=c::script::MissionDocument::parse(text,m::kProfile,error);
    CHECK(static_cast<bool>(doc));
    auto absentVendor=text;const std::string countText="\"vance_count\": 1";
    const auto countAt=absentVendor.find(countText);CHECK(countAt!=std::string::npos);
    absentVendor[countAt+countText.size()-1]='0';
    CHECK(!c::script::MissionDocument::parse(absentVendor,m::kProfile,error));
    auto definition=m::kActivity;definition.animations=m::kVanceAnimationCapabilities;
    a::PersistentActivity activity;CHECK(activity.begin({42,{7}},definition,doc,99));
    bool paired{};
    const auto* graph=doc->views().role("persistent");CHECK(graph!=nullptr);
    bool ordered{};
    for(const auto& step:graph->definition.steps)for(std::size_t i=1;i<step.commands.size();++i) {
        const auto& before=step.commands[i-1];const auto& after=step.commands[i];
        if(before.asset.registry==0x564C6ECE && before.asset.type==1 && before.asset.slot==0
            && after.asset.registry==0x564C6ECE && after.asset.type==42 && after.asset.slot==2)ordered=true;
    }
    CHECK(ordered);
    for(int i=0;i<4;++i) {
        const auto frame=activity.update(15,true);
        for(std::size_t j=0;j<frame.populations.count;++j)
            if(frame.populations.entries[j].source.registry==0x564C6ECE) {
                CHECK(frame.populations.entries[j].source.looseRequested==1);
                CHECK(frame.animations.count==1 && frame.animations.entries[0].control.counter==1);
                CHECK(frame.animations.entries[0].control.sequence==0x010B0F07);paired=true;
            }
    }
    CHECK(paired && activity.animation().revision()==2);
    // Time and regional movement must not clear or replay the verified idle.
    a::activity_clock::Publication clock{{{42,{7}},99,1,0x80F4696A,15},{false,1000.0F/30.0F},0};
    for(const std::uint64_t milliseconds:{60001ULL,120000ULL,3600000ULL}) {
        CHECK(a::activity_clock::wire::from_milliseconds(milliseconds,clock.elapsedTicks));
        for(const auto bubble:{14U,15U,18U}) {
            const auto frame=activity.update(bubble,true,{},true,clock);
            CHECK(frame.animations.count==1 && frame.animations.entries[0].control.counter==1);
            CHECK(frame.animations.entries[0].control.sequence==0x010B0F07);
            CHECK(activity.animation().revision()==2);
        }
    }
    p::Command command{};CHECK(p::parse("npc1 63 2A 7 2 2 564C6ECE 2 1",command));
    CHECK(activity.animation().request(command,15)==p::Result::accepted);
    CHECK(activity.update(15,false).animations.count==1);CHECK(activity.update(14,true).animations.count==1);
    for(int i=0;i<100;++i) {
        const auto frame=activity.update(15,true);CHECK(frame.animations.count==1);
        CHECK(frame.animations.entries[0].control.counter==2);
    }
    CHECK(activity.animation().revision()==3);
    command.expectedRevision=3;command.request=3;command.action=0;command.stop=true;
    CHECK(activity.animation().request(command,15)==p::Result::accepted);
    for(int i=0;i<4;++i) {
        const auto stopped=activity.update(15,true);
        CHECK(stopped.animations.count==1 && stopped.animations.entries[0].control.sequence==n::kEmptyHash);
        CHECK(stopped.animations.entries[0].control.counter==2);
    }
    CHECK(activity.animation().revision()==4); // Normal graph is one-shot; no forced replay.
    a::PersistentActivity newOwner;CHECK(newOwner.begin({43,{1}},definition,doc,100));
    CHECK(newOwner.animation().request(command,15)==p::Result::stale);
    for(int i=0;i<4;++i) static_cast<void>(newOwner.update(15,true));
    CHECK(newOwner.animation().project(15).entries[0].control.counter==1);
    std::array<p::Capability,2> duplicate{m::kVanceAnimationCapabilities[0],m::kVanceAnimationCapabilities[0]};
    auto invalid=definition;invalid.animations=duplicate;
    a::PersistentActivity rollback;CHECK(!rollback.begin({50,{1}},invalid,doc,100));
    CHECK(!rollback.population().owner() && !rollback.animation().owner());
    CHECK(rollback.begin({50,{1}},definition,doc,100));

    const std::string routeJson=R"JSON({
      "format_version":2,"mission":"npc_fixture","profile":"npc.fixture.v1","authority_schema":"nativeOtherActivities",
      "assets":{"persistent_module":{"registry":"0x80F4696A","definition":"0x80F4696A","type":0,"slot":0},
                "npc":{"registry":"0x564C6ECE","definition":"0x80F5BA28","type":42,"slot":2}},
      "bindings":{"start":{"capability":"persistent.start","operation":"mechanic","asset":"persistent_module","argument":1,"wait":"requested"},
                  "idle":{"capability":"npc.idle","operation":"mechanic","asset":"npc","argument":1,"wait":"requested"}},
      "graphs":{"entry":{"name":"entry","domain":"composition","steps":[{"id":"start","after":[],"commands":[{"id":"start","binding":"start"}]}],"receipts":{}},
                "native":{"name":"native","domain":"nativeActivity","steps":[{"id":"idle","after":[],"commands":[{"id":"idle","binding":"idle"}]}],"receipts":{}}},
      "roles":{"persistent":"native"},"entry":"entry","modules":["persistent"],"observations":[],
      "presentation":{"dialogue":{"bank":"0x00000000","rows":[],"objective_cues":[],"dispatch_timeout_ms":100,"spacing_ms":0},"cue_sets":{},"action_sets":{},"binding_tables":{}}
    })JSON";
    std::array<c::script::Capability,2> scriptCaps{{m::kScriptCapabilities[0],
        {"npc.idle","nativeActivity",{c::Operation::mechanic,{0x564C6ECE,0x80F5BA28,42,2},1,c::Wait::requested}}}};
    auto profile=m::kProfile;profile.id="npc.fixture.v1";profile.capabilities=scriptCaps;profile.parameters={};
    a::NativeActivityDefinition fixture{};fixture.activity="npc_fixture";fixture.profile=&profile;
    fixture.bubble=definition.bubble;fixture.registries=definition.registries;fixture.persistentModule=definition.persistentModule;
    auto unverifiedActions=m::kVanceAnimationActions;unverifiedActions[0].retailVerified=false;
    std::array<p::Capability,1> fixtureAnimations{{{&m::kRegistries[1],2,unverifiedActions}}};fixture.animations=fixtureAnimations;
    std::array<a::NativeAction,1> routes{{{{c::Operation::mechanic,{0x564C6ECE,0x80F5BA28,42,2},1,c::Wait::requested},0,{}}}};fixture.actions=routes;
    std::shared_ptr<const c::script::MissionDocument> routeDoc=c::script::MissionDocument::parse(routeJson,profile,error);
    CHECK(static_cast<bool>(routeDoc));CHECK(!a::PersistentActivity::valid(fixture,*routeDoc));
    auto trustedActions=m::kVanceAnimationActions;trustedActions[0].retailVerified=true;
    std::array<p::Capability,1> trusted{{{&m::kRegistries[1],2,trustedActions}}};fixture.animations=trusted;
    CHECK(a::PersistentActivity::valid(fixture,*routeDoc));
    a::PersistentActivity routed;CHECK(routed.begin({99,{1}},fixture,routeDoc,101));
    a::NativeActivityFrame frame;for(int i=0;i<4;++i) frame=routed.update(15,true);
    CHECK(frame.animations.count==1 && frame.animations.entries[0].control.sequence==0x010B0F07);
    CHECK(routed.diagnostics().phase==c::Phase::complete); // Requested, not native completion.
    CHECK(m::kVanceAnimationActions[0].retailVerified);
}

int main(int argc,char** argv) {
    CHECK(argc==3);vectors(argv[1]);service();validation();mailbox();packets();persistent(argv[2]);
    std::printf("Native NPC animation: %u checks passed; verified Vance startup and generic capability isolation.\n",checks);
}
