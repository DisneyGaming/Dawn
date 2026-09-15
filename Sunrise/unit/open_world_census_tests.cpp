#include "../src/server/runtime/activity/open_world_census.h"
#include "../src/core/settings/settings.h"
#include "../src/core/filesystem/path.h"
#include "../src/state/activity/open_world_member_observations.h"
#include <cstdlib>
#include <chrono>
#include <iostream>
#include <string_view>
#include <thread>

namespace sunrise::core::settings {Settings g_censusSettings{};const Settings& get() noexcept {return g_censusSettings;}}
namespace sunrise::core::path {
bool artifact_directory(void*,Buffer&) noexcept{return false;}
bool append(Buffer&,std::wstring_view) noexcept{return false;}
}
namespace census=sunrise::server::runtime::activity::open_world_census;
void require(bool condition,const char* message){if(!condition){std::cerr<<message<<'\n';std::exit(1);}}
int main(int argc,char**){
    census::Record record{};record.event=census::Event::sourceObservation;record.destination="titan\"test";
    record.scenario=0x80B98653;record.bubble=11;record.arrived=true;record.registry=0x1EE02F73;
    record.sourceSlot=0;record.ruleSlot=7;record.generation=4;record.requestSequence=19;
    record.sourceBubble=11;record.hasRule=true;
    record.requestedFirst=1;record.requestedSecond=0;record.consumedKnown=true;
    record.consumedFirst=1;record.consumedSecond=0;record.tacticalProvider=0x1EE02F73;
    record.tacticalSlot=3;record.tacticalRow=2;record.tacticalRevision=4;
    record.hasCounts=true;record.counts={4,3,1,3,false,false};record.reason="native_source_mirror";
    std::array<char,2048> output{};std::size_t length{};
    require(census::format(record,2,17,output,length),"format failed");
    const std::string_view text(output.data(),length);
    require(text.ends_with("}\n"),"not JSONL");
    require(text.find("\"destination\":\"titan\\\"test\"")!=text.npos,"string not escaped");
    require(text.find("\"consumed_first\":1")!=text.npos&&text.find("\"consumed_second\":0")!=text.npos,"category lanes missing");
    require(text.find("\"retired\":1")!=text.npos,"derived retired count missing");
    require(text.find("\"source_bubble\":11")!=text.npos&&text.find("\"has_rule\":true")!=text.npos,"source identity fields missing");
    require(text.find("\"rule_guid\":null")!=text.npos&&text.find("\"metadata_evidence\":\"unresolved\"")!=text.npos,"unknown metadata guessed");
    require(text.find("owner")==text.npos&&text.find("boot")==text.npos&&text.find("account")==text.npos,"private field leaked");
    if(argc>1)std::cout<<text;
    record.member.known=true;record.member.evidence="exact_native_member_offset";
    record.member.category=1;record.member.categoryKey=0x12345678;record.member.entity=0x80BF8A8B;
    record.member.variant=2;record.member.choice=3;record.member.weight=8;record.member.kind=0x80801234;
    record.member.offset=432;
    require(census::format(record,3,18,output,length),"selected member format failed");
    const std::string_view selected(output.data(),length);
    require(selected.find("\"category\":1")!=selected.npos&&selected.find("\"template_entity\":\"0x80bf8a8b\"")!=selected.npos,"exact member choice not serialized");
    require(selected.find("\"entity\":null")!=selected.npos,"live handle and template identity conflated");
    require(selected.find("\"choice_weight\":8")!=selected.npos&&selected.find("\"member_offset\":432")!=selected.npos,"choice evidence missing");
    require(selected.find("\"type\":null")!=selected.npos&&selected.find("\"rank\":null")!=selected.npos,"unproved species or rank inferred");
    require(selected.find("\"position\":null")!=selected.npos&&selected.find("\"rule_guid\":null")!=selected.npos,"unproved placement inferred");
    record.member={};
    record.destination=std::string_view(output.data(),output.size());
    require(!census::format(record,3,18,output,length),"oversized record accepted");
    record.destination="titan";
    require(!census::enabled(),"census must be default off");
    require(census::initialize(),"disabled lifecycle failed");
    require(!sunrise::state::activity::open_world_members::enabled(),"disabled census enabled client capture");
    sunrise::core::settings::g_censusSettings.omegaExperiments.unsafeDiagnostics=true;
    require(!census::enabled(),"broad unsafe diagnostics enabled census");
    sunrise::core::settings::g_censusSettings.omegaExperiments.openWorldCensus=true;
    require(census::enabled(),"dedicated census option ignored");
    census::testing::BoundedQueue<unsigned,2> queue;unsigned value{};
    require(queue.push(1)&&queue.push(2)&&!queue.push(3),"bounded queue accepted overflow");
    require(queue.pop(value)&&value==1&&queue.pop(value)&&value==2&&!queue.pop(value),"bounded queue order failed");
    record.counts={1,0,0,2,false,false};
    require(census::format(record,4,19,output,length),"malformed counts format failed");
    const std::string_view malformed(output.data(),length);
    require(malformed.find("\"retired\":null")!=malformed.npos&&malformed.find("\"failed\":true")!=malformed.npos,"malformed counts underflowed");
    require(census::initialize(),"diagnostic worker did not start");
    require(sunrise::state::activity::open_world_members::enabled(),"census did not enable member sidecar");
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    census::emit(record);census::shutdown();
    require(!sunrise::state::activity::open_world_members::enabled(),"shutdown left member capture active");
    require(census::stats().ioFailures>0,"I/O failure was not isolated and counted");
    return 0;
}
