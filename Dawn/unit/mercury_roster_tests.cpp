#include "server/runtime/activity/mercury_definition.h"
#include "server/runtime/activity/mercury_freeroam_runtime.h"
#include "server/runtime/activity/adventure_authored_overlay.h"
#include "server/bap/encrypted/push/activity/native_roster_lifetime_projection.h"
#include "state/build_data/cache/records/codec.h"
#include "state/build_data/scenarios/roster_fallback.h"
#include "client/network/consumer.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>
namespace runtime=dawn::server::runtime::activity;
namespace mercury=runtime::mercury;
namespace layouts=dawn::state::build_data::scenarios;
namespace records=dawn::state::build_data::cache::records;
namespace wire=dawn::middleware::bap::activity_message::sensor_auth_update;
namespace lifetime=dawn::server::bap::encrypted::push::activity::roster_lifetime;
static unsigned checks{};
void check(bool ok,const char* message){++checks;if(!ok){std::cerr<<"FAIL: "<<message<<'\n';std::exit(1);}}
template<class T>T read(std::ifstream& file){T result{};file.read(reinterpret_cast<char*>(&result),sizeof(result));check(file.good(),"complete cached record");return result;}
struct Storage {
    std::array<layouts::RosterGroup,wire::kGroupCapacity> rosterGroups{};
    std::array<std::array<std::uint32_t,wire::kGroupCapacity>,wire::kBubbleSubBlockCapacity> rosterSubBlockKeys{};
    std::array<wire::BubbleSubBlock,wire::kBubbleSubBlockCapacity> rosterSubBlocks{};
};
wire::Group expose(const layouts::RosterGroup& group){return {group.registryKey,std::span(group.slotTypes).first(group.slotCount),std::span(group.slotFlags).first(group.slotCount),std::span(group.slotIndices).first(group.slotCount)};}
int main(int argc,char** argv){
    const std::filesystem::path fixture=argc>1?argv[1]:"build/mercury-validation/fixtures";
    std::ifstream file(fixture/"groups.bin",std::ios::binary);check(file.good(),"installed cache fixture opens");
    const auto count=read<std::uint32_t>(file);check(count<=layouts::kRosterGroupCapacity,"bounded catalog");
    std::vector<layouts::RosterGroup> groups(count);
    for(auto& group:groups)check(records::decode(read<records::RosterGroupRecord>(file),group),"decode installed group");
    const auto layoutFor=[&](std::string_view name){std::ifstream input(fixture/(std::string(name)+".bin"),std::ios::binary);layouts::Definition result{};check(records::decode(read<records::ScenarioRecord>(input),result),"decode installed scenario");return result;};
    auto layout=layoutFor("mercury_freeroam");
    const auto find=[&](std::size_t i,layouts::RosterGroup& g){if(i>=groups.size())return false;g=groups[i];return true;};
    const auto byKey=[&](std::uint32_t key,layouts::RosterGroup& g){for(const auto& row:groups)if(row.registryKey==key){g=row;return true;}return false;};
    if(!layout.rosterGroupCount)check(layouts::amend_participation_fallback(layout,find),"production free-roam participation fallback");
    for(bool publicEvent:{false,true})for(int adventure=-1;adventure<3;++adventure){
        auto storage=std::make_unique<Storage>();auto snapshot=std::make_unique<wire::Snapshot>();
        auto& roster=snapshot->roster;
        const auto addBase=[&](std::size_t index,bool top,std::uint64_t mask){
            for(std::size_t i=0;i<roster.groupCount;++i)if(roster.groups[i].key==groups[index].registryKey)return;
            check(roster.groupCount<roster.groups.size(),"base roster capacity");
            auto& g=storage->rosterGroups[roster.groupCount];check(find(index,g),"base group found");
            roster.groups[roster.groupCount++]=expose(g);
            if(top){++roster.topLevelGroupCount;for(std::size_t i=0;i<g.slotCount;++i)if(g.slotTypes[i]==13)roster.playerKeyGroup=g.registryKey;}
            else for(std::uint32_t b=0;b<64;++b)if(mask&(1ULL<<b)){
                auto used=roster.bubbleSubBlocks.size();std::size_t target=used;
                for(std::size_t i=0;i<used;++i)if(storage->rosterSubBlocks[i].bubble==b)target=i;
                auto& block=storage->rosterSubBlocks[target];const auto n=block.keys.size();
                storage->rosterSubBlockKeys[target][n]=g.registryKey;
                block={b,std::span(storage->rosterSubBlockKeys[target]).first(n+1)};
                roster.bubbleSubBlocks=std::span(storage->rosterSubBlocks).first(used+(target==used));
            }
        };
        for(std::size_t i=0;i<layout.rosterGroupCount;++i)addBase(layout.rosterGroups[i],true,0);
        const auto authored=layout.authoredGroupCounts[15];
        if(authored)addBase(layout.authoredGroups[15][0],true,0);
        for(std::size_t i=0;i<layout.bubbleGroupCount;++i)addBase(layout.bubbleGroups[i],false,layout.bubbleGroupMasks[i]);
        for(std::size_t i=1;i<authored;++i)addBase(layout.authoredGroups[15][i],false,1ULL<<15);
        check(roster.playerKeyGroup!=0,"real player group retained");
        const auto base=roster.groupCount;
        const auto admit=[&](const auto& definition){
            const auto result=runtime::registry::admit(layout,*storage,roster,definition,byKey);
            if(result!=runtime::registry::Admission::added && result!=runtime::registry::Admission::present)
                std::cerr<<"registry "<<std::hex<<definition.key<<std::dec<<" result "<<unsigned(result)<<" groups "<<roster.groupCount<<'\n';
            check(result==runtime::registry::Admission::added || result==runtime::registry::Admission::present,"native registry admission");
        };
        for(const auto& registry:mercury::kRegistries)admit(registry);
        if(publicEvent)for(const auto& binding:mercury::kOptionalRegistries)admit(*binding.registry);
        if(adventure>=0){
            const auto& binding=runtime::adventure::mercury::kOpeningOverlays[adventure];
            auto plan=std::make_unique<runtime::authored_overlay::Plan>();
            check(runtime::authored_overlay::prepare(layout,layoutFor(binding.selectedPackage),binding,find,*plan),"installed adventure opening plan");
            check(runtime::authored_overlay::append(*plan,*storage,roster)==runtime::authored_overlay::Result::added,"adventure fits populated Mercury");
            const auto& forest=runtime::adventure::mercury::kForestOverlays[adventure];
            check(runtime::authored_overlay::prepare(layout,layoutFor(forest.selectedPackage),forest,find,*plan),"installed forest overlay plan");
            check(runtime::authored_overlay::append_region(*plan,*storage,roster)==runtime::authored_overlay::Result::added,"forest overlay preserves populated Mercury and opening");
        }
        runtime::population::Service population;
        check(population.begin({0x1234,{1}},mercury::kPopulations,0x9876),"all population capabilities admitted");
        unsigned requested{};
        for(const auto& patrol:mercury::freeroam::kPatrols){const auto& cap=mercury::kPopulations[patrol.capability];
            const auto target=static_cast<std::uint8_t>(patrol.largeArea?4:3); requested+=target;
            check(population.request({population.owner(),population.revision(),population.last_request()+1,cap.registry->key,cap.slot,target,population.boot()},15)==runtime::population::Result::accepted,"dense patrol source accepted");}
        check(requested==56,"eight ordinary3 and eight large4 native squad requests");
        snapshot->populations=population.project(15);snapshot->hasRegion=true;snapshot->region=120;snapshot->lifetime=6;snapshot->stateSequence=3;
        auto lifetimes=std::make_unique<lifetime::State>();
        const lifetime::Identity identity{0x1234,1,0x5678,0x9876,0x80F4696A};
        check(lifetime::prepare({},identity,15,0x83,true,roster,*lifetimes)==lifetime::Result::ready,"production roster lifetime adoption");
        std::array<wire::BubbleSubBlock,wire::kBubbleSubBlockCapacity> retainedBlocks{};
        check(lifetime::project(*lifetimes,roster,retainedBlocks),"production retained roster projection");
        auto packet=std::make_unique<std::array<std::byte,dawn::client::network::kBapFrameCapacity>>();std::size_t written{};
        check(wire::encode_sensor_auth_update(*snapshot,*packet,written),"complete populated roster encodes");
        check(written>0 && written<packet->size(),"complete roster fits production frame");
        snapshot->region=96;
        check(wire::encode_sensor_auth_update(*snapshot,*packet,written),"retained patrols encode during forest traversal");
        std::cout<<"base="<<base<<" public_event="<<publicEvent<<" adventure="<<adventure<<" groups="<<roster.groupCount<<" sources="<<snapshot->populations.count<<" bytes="<<written<<'\n';
    }
    std::cout<<"PASS: "<<checks<<" installed Mercury roster, public event, adventure and serialization checks\n";
}
