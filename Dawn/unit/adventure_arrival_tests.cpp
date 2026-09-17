#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <filesystem>
#include <vector>
#include "core/provenance/build_provenance.h"
#include "state/build_data/cache/records/codec.h"
#include "server/runtime/activity/adventure_mercury_overlays.h"
#include "server/runtime/activity/adventure_opening_publication.h"
#include "middleware/encoding/bit_writer.h"

namespace rec=dawn::state::build_data::cache::records;
namespace overlay=dawn::server::runtime::activity::authored_overlay;
namespace mercury=dawn::server::runtime::activity::adventure::mercury;
namespace layouts=dawn::state::build_data::scenarios;
namespace wire=dawn::middleware::bap::activity_message::sensor_auth_update;
namespace opening=dawn::server::runtime::activity::adventure;
namespace bits=dawn::middleware::encoding::bits;
unsigned checks{};
void check(bool value,const char* what){++checks;if(!value){std::fprintf(stderr,"FAIL: %s\n",what);std::exit(1);}}
template<class T> bool read(std::ifstream& f,T& value){return bool(f.read(reinterpret_cast<char*>(&value),sizeof value));}
template<class T> void skip(std::ifstream& f,std::uint32_t count){f.seekg(std::uint64_t(count)*sizeof(T),std::ios::cur);}
struct Fixture final {
    std::vector<layouts::Definition> scenarios;
    std::vector<layouts::RosterGroup> groups;
    explicit Fixture(const char* path){
        std::ifstream file(path,std::ios::binary);rec::Header h{};
        check(read(file,h) && h.magic==rec::kCacheMagic && h.version==rec::kCacheFormatVersion,"real current-format cache");
        skip<rec::NamedRecord>(file,h.namedCount);skip<rec::ItemRecord>(file,h.itemCount);
        skip<rec::CollectibleRecord>(file,h.collectibleCount);skip<rec::MaterialRequirementSetRecord>(file,h.materialRequirementSetCount);
        skip<rec::ItemDetailRecord>(file,h.itemDetailCount);skip<rec::SocketPlugRuleRecord>(file,h.socketPlugRuleCount);
        skip<rec::SocketPlugPoolRecord>(file,h.socketPlugPoolCount);skip<rec::SocketPlugMemberRecord>(file,h.socketPlugMemberCount);
        skip<rec::InventoryBucketRecord>(file,h.inventoryBucketCount);skip<rec::SocketEntryListRecord>(file,h.socketEntryListCount);
        skip<rec::SocketEntryTableRecord>(file,h.socketEntryTableCount);skip<rec::AbilityBucketRecord>(file,h.abilityBucketCount);
        skip<rec::ProgressionRecord>(file,h.progressionCount);
        check(h.scenarioCount<=layouts::kDefinitionCapacity && h.rosterGroupCount<=layouts::kRosterGroupCapacity,"bounded real catalog");
        scenarios.resize(h.scenarioCount);groups.resize(h.rosterGroupCount);
        for(auto& s:scenarios){rec::ScenarioRecord r{};check(read(file,r) && rec::decode(r,s),"production scenario decode");}
        for(auto& g:groups){rec::RosterGroupRecord r{};check(read(file,r) && rec::decode(r,g),"production descriptor decode");}
    }
    const layouts::Definition& scenario(std::uint32_t tag)const{
        for(const auto& s:scenarios)if(s.tag==tag)return s;check(false,"fixture scenario present");std::abort();
    }
    const layouts::RosterGroup& group(std::uint32_t key)const{
        for(const auto& g:groups)if(g.registryKey==key)return g;check(false,"fixture group present");std::abort();
    }
};
struct Storage final {
    std::array<layouts::RosterGroup,wire::kGroupCapacity> rosterGroups{};
    std::array<wire::BubbleSubBlock,64> rosterSubBlocks{};
    std::array<std::array<std::uint32_t,96>,64> rosterSubBlockKeys{};
};
wire::Group expose(const layouts::RosterGroup& g){return {g.registryKey,std::span(g.slotTypes).first(g.slotCount),
    std::span(g.slotFlags).first(g.slotCount),std::span(g.slotIndices).first(g.slotCount)};}
wire::Roster baseline(Storage& s,const Fixture& f){
    s.rosterGroups[0]=f.group(0x4786C0E0);s.rosterGroups[1]=f.group(0x2749BAAE);
    s.rosterSubBlockKeys[0][0]=0x2749BAAE;s.rosterSubBlocks[0]={15,std::span(s.rosterSubBlockKeys[0]).first(1)};
    wire::Roster r{};r.groups[0]=expose(s.rosterGroups[0]);r.groups[1]=expose(s.rosterGroups[1]);
    r.groupCount=2;r.topLevelGroupCount=1;r.playerKeyGroup=0x4786C0E0;r.bubbleSubBlocks=std::span(s.rosterSubBlocks).first(1);return r;
}
#include "state/activity/adventure_destination_transition.h"
#include "middleware/bap/activity_message/adventure_start_request.h"
#include "server/bap/encrypted/push/activity/activity_arrival.h"

int main(int argc,char**argv) {
    if(argc!=3)return 2;
    const Fixture fixture(argv[1]);
    std::ifstream file(argv[2],std::ios::binary|std::ios::ate);
    check(bool(file),"current live request exists");const auto length=file.tellg();
    check(length==90,"full live request, no prefix padding");
    std::vector<std::byte> bytes(static_cast<std::size_t>(length));file.seekg(0);
    file.read(reinterpret_cast<char*>(bytes.data()),length);check(bool(file),"read full live request");
    namespace state=dawn::state::activity;
    namespace start=dawn::middleware::bap::activity_message::adventure_start;
    namespace arrival=dawn::server::bap::encrypted::push::activity;
    start::Request request{};check(start::parse(bytes,request),"original current in-world request parses");
    const auto& incoming=request.selection;
    check(incoming.reason==1 && incoming.activityIndex==1076 && incoming.sourceActivityIndex==1076,"actual mode1 selected Adventure");
    check(incoming.hasArrivalBubbleHash && incoming.arrivalBubbleHash==0x811C9DC5
        && incoming.hasSpawnSetHash && incoming.spawnSetHash==0x811C9DC5,"actual native neutral landing values");
    const auto& binding=mercury::kOpeningOverlays[0];
    const auto& host=fixture.scenario(binding.hostScenario);const auto& selected=fixture.scenario(binding.selectedScenario);
    auto plan=std::make_unique<overlay::Plan>();
    check(overlay::prepare(host,selected,binding,[&](std::size_t index,layouts::RosterGroup& group) {
        if(index>=fixture.groups.size())return false;group=fixture.groups[index];return true;
    },*plan),"current cache exact selected root/local plan");
    auto storage=std::make_unique<Storage>();auto roster=baseline(*storage,fixture);
    check(overlay::append(*plan,*storage,roster)==overlay::Result::added,"current cache selected overlay admitted");
    state::defaults::DefaultDestination defaults{};
    static state::ActivityState world{};world.stateRevision=22;
    auto& record=world.sessions[0];record.occupied=true;record.joined=true;
    record.sessionId=0x9EAA300100200001ULL;record.lifecycle={{1},{4},false};record.recordRevision=18;
    record.memberKey=31337;record.membership.region={120,0xA83A9175};
    auto& before=record.destination;before.packageName=incoming.packageName;
    before.packageNameLength=incoming.packageNameLength;before.activityIndex=29;
    before.hasArrivalBubbleHash=true;before.arrivalBubbleHash=0xA83A9175;
    before.hasSpawnSetHash=true;before.spawnSetHash=0xD49C610E;
    auto proposed=before;proposed.reason=incoming.reason;proposed.activityIndex=incoming.activityIndex;
    proposed.previousActivityIndex=incoming.sourceActivityIndex;
    proposed.hasArrivalBubbleHash=incoming.hasArrivalBubbleHash;proposed.arrivalBubbleHash=incoming.arrivalBubbleHash;
    proposed.hasSpawnSetHash=incoming.hasSpawnSetHash;proposed.spawnSetHash=incoming.spawnSetHash;
    proposed.descriptorBits=incoming.descriptorBits;proposed.descriptorBitLength=static_cast<std::uint16_t>(incoming.descriptorBitLength);
    proposed.hasDescriptorName=true;proposed.descriptorNameBit=static_cast<std::uint16_t>(incoming.packageNameBitOffset);
    const auto resolve=[&](const auto& s){return arrival::arrival_slice_set(defaults,s,"mercury_freeroam",host);};
    check(resolve(before)==120 && resolve(proposed)==0,"reproduce installedr6 arrival regression from real wire and cache");
    check(!opening::opening_lifetime_scenario(binding,*plan,roster,120,resolve(proposed)),"strict opening guard explains actual no_groups");
    state::adventure_destination::Pending pending{};
    const state::ActivityInstanceKey owner{record.sessionId,{1}};
    check(state::adventure_destination::prepare_from(world,owner,18,proposed,pending)==state::adventure_destination::Result::prepared,"prepare same-host descriptor transaction");
    check(resolve(pending.after)==120 && before.activityIndex==29,"prepared arrival retained without early commit");
    check(state::adventure_destination::commit_to(world,pending),"commit exact same owner");
    check(record.destination.activityIndex==1076 && record.recordRevision==23,"selection advances");
    check(record.membership.region.index==120 && record.membership.region.hash==0xA83A9175
        && record.memberKey==31337 && record.lifecycle.hostRegion.value==4,"physical world and owner unchanged");
    check(record.destination.hasArrivalBubbleHash && record.destination.arrivalBubbleHash==0xA83A9175
        && record.destination.hasSpawnSetHash && record.destination.spawnSetHash==0xD49C610E,"original arrival and spawn binding retained");
    check(record.destination.descriptorBits==incoming.descriptorBits && record.destination.descriptorBitLength==incoming.descriptorBitLength,"entire native descriptor including unknown tail unchanged");
    start::Request echoed{};
    check(start::parse(std::span(record.destination.descriptorBits).first(bytes.size()),echoed)
        && echoed.selection.arrivalBubbleHash==0x811C9DC5 && echoed.selection.spawnSetHash==0x811C9DC5
        && echoed.nonce==request.nonce && echoed.revision==request.revision,"native echoed fields remain original");
    check(opening::opening_lifetime_scenario(binding,*plan,roster,120,resolve(record.destination))==15U,"real transition now admits authored opening phase15");
    for(const auto manual:{0U,96U,104U,128U}) {
        record.destination.hasSliceSetOverride=true;record.destination.sliceSetOverride=static_cast<std::uint16_t>(manual);
        check(state::adventure_destination::prepare_from(world,owner,record.recordRevision,proposed,pending)==state::adventure_destination::Result::prepared,"explicit manual policy retained");
        check(resolve(pending.after)==manual && !opening::opening_lifetime_scenario(binding,*plan,roster,120,resolve(pending.after)),"manual foreign arrival still rejects opening");
    }
    record.destination.hasSliceSetOverride=false;
    record.destination.hasArrivalBubbleOverride=true;record.destination.arrivalBubbleOverride=12;
    record.destination.hasSpawnSetOverride=true;record.destination.spawnSetOverride=0xD49C610E;
    record.membership.region={96,host.bubbleHashes[12]};
    check(state::adventure_destination::prepare_from(world,owner,record.recordRevision,proposed,pending)==state::adventure_destination::Result::prepared,"descriptor transaction after independently observed region transition");
    check(pending.after.hasArrivalBubbleOverride && pending.after.arrivalBubbleOverride==12
        && pending.after.hasSpawnSetOverride && pending.after.spawnSetOverride==0xD49C610E
        && !pending.after.hasSliceSetOverride,"manual arrival/spawn override flags and values preserved");
    check(state::adventure_destination::commit_to(world,pending),"later descriptor commit preserves real region transition");
    check(record.membership.region.index==96 && record.membership.region.hash==host.bubbleHashes[12]
        && record.lifecycle.hostRegion.value==4 && record.memberKey==31337,"no snapback of actual membership or host lineage");
    check(!opening::opening_lifetime_scenario(binding,*plan,roster,96,resolve(record.destination))
        && record.destination.descriptorBits==incoming.descriptorBits,"outside-Lighthouse opening still rejects; raw mode1 remains bit exact");
    std::printf("PASS %u actual-r6 request/cache and retained-arrival checks; before arrival0 refused, after arrival120 admitted15; native descriptor unchanged\n",checks);
}

