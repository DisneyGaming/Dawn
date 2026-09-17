#include "../src/client/content/scenarios/campaign_shared_groups.h"
#include "../src/server/bap/encrypted/push/activity/strike_pact_roster.h"
#include "../src/server/bap/encrypted/push/activity/strike_bond_roster.h"
#include "../src/state/build_data/cache/records/codec.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>
namespace shared=dawn::client::content::scenarios::campaign_shared;
namespace layouts=dawn::state::build_data::scenarios;
namespace records=dawn::state::build_data::cache::records;
namespace pact=dawn::server::bap::encrypted::push::activity::strike_pact_roster;
namespace bond=dawn::server::bap::encrypted::push::activity::strike_bond_roster;
namespace wire=dawn::middleware::bap::activity_message::sensor_auth_update;
static unsigned checks{};
void check(bool ok,const char* message){++checks;if(!ok){std::cerr<<"FAIL: "<<message<<'\n';std::exit(1);}}
template<class T> T read(std::ifstream& file){T result{};file.read(reinterpret_cast<char*>(&result),sizeof(result));check(file.good(),"complete captured native record");return result;}
struct Storage {
    std::array<layouts::RosterGroup,wire::kGroupCapacity> rosterGroups;
    std::array<std::array<std::uint32_t,wire::kBubbleKeyCapacity>,wire::kBubbleSubBlockCapacity> rosterSubBlockKeys;
    std::array<wire::BubbleSubBlock,wire::kBubbleSubBlockCapacity> rosterSubBlocks;
};
int main(int argc,char** argv){
    const std::filesystem::path fixture=argc>1?argv[1]:"build/coo/tree-baboon-20260912/fixtures";
    std::ifstream file(fixture/"groups.bin",std::ios::binary);check(file.good(),"captured failed-launch cache opens");
    const auto count=read<std::uint32_t>(file);check(count<=layouts::kRosterGroupCapacity,"bounded native catalog");
    std::vector<layouts::RosterGroup> groups(count);
    for(auto& group:groups) check(records::decode(read<records::RosterGroupRecord>(file),group),"decode actual cached group");
    const auto find=[&](std::size_t i,layouts::RosterGroup& g){if(i>=groups.size())return false;g=groups[i];return true;};
    for(const auto name:{"mission_pact","mission_bond","strike_pact","strike_bond"}){
        std::ifstream input(fixture/(std::string(name)+".bin"),std::ios::binary);layouts::Definition layout{};
        check(records::decode(read<records::ScenarioRecord>(input),layout),"decode real activity layout");
        auto storage=std::make_unique<Storage>();wire::Roster roster{};pact::Report pr{};bond::Report br{};
        const bool tree=std::string_view(name).ends_with("pact"),campaign=std::string_view(name).starts_with("mission");
        const auto admit=[&]{return tree?pact::admit(layout,*storage,roster,120,find,pr):bond::admit(layout,*storage,roster,120,find,br);};
        check(admit()!=campaign,"captured campaign rejects while existing strikes admit");
        if(!campaign) continue;
        check(tree?pr.lastMissing==0xCC7A090DU:br.rowExpectedTag==0x80F54563U,"reproduces the missing shared encounter causing Baboon");
        std::ifstream imports(fixture/"imports.txt");std::uint32_t scenario{},object{},key{};unsigned bubble{};std::uint64_t mask{};
        while(imports>>scenario>>object>>key>>bubble>>mask){
            if(scenario!=layout.tag) continue;
            const auto b=static_cast<std::uint8_t>(bubble);
            check(shared::candidate(scenario,object)&&shared::selected(scenario,object,key,2,b,mask),"exact imported native identity selected");
            check(!shared::selected(scenario+1,object,key,2,b,mask),"another activity cannot import this encounter");
            check(!shared::selected(scenario,object+1,key,2,b,mask),"another object cannot impersonate shared encounter");
            check(!shared::selected(scenario,object,key+1,2,b,mask),"registry identity must match");
            check(!shared::selected(scenario,object,key,0,b,mask),"shared encounter never promoted to global root");
            check(!shared::selected(scenario,object,key,2,b,0),"explicit native bubble ownership is required");
            check(!shared::selected(scenario,object,key,2,static_cast<std::uint8_t>((b+1)%64),mask),"shared encounter cannot move bubbles");
            const auto it=std::find_if(groups.begin(),groups.end(),[&](const auto& g){return g.objectTag==object&&g.registryKey==key;});
            check(it!=groups.end(),"same native object is already decoded in strike catalog");
            const auto index=static_cast<std::uint16_t>(it-groups.begin());auto& n=layout.authoredGroupCounts[b];auto& local=layout.authoredGroups[b];
            if(std::find(local.begin(),local.begin()+n,index)!=local.begin()+n) continue;
            check(n<local.size(),"complete campaign fits current per-bubble capacity");local[n++]=index;
        }
        check(admit(),"complete campaign roster admits with shared encounter objects");
        check(roster.groupCount<=wire::kGroupCapacity&&roster.playerKeyGroup==0x4786C0E0U,"native capacity and campaign player owner preserved");
        for(const auto& import:shared::kGroups){
            if(import.scenario!=layout.tag) continue;
            bool found=false;for(const auto& block:roster.bubbleSubBlocks) if(block.bubble==import.bubble)
                found|=std::find(block.keys.begin(),block.keys.end(),import.key)!=block.keys.end();
            check(found,"every imported encounter has correct bubble publication");
        }
        std::cout<<name<<": "<<roster.groupCount<<" native groups admitted\n";
    }
    std::cout<<"PASS: "<<checks<<" captured campaign failure, native import scope, roster and strike compatibility checks\n";
}
