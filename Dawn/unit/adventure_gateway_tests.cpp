#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <filesystem>
#include <vector>
#include "core/provenance/build_provenance.h"
#include "state/build_data/cache/records/codec.h"
#include "server/runtime/activity/adventure_mercury_forest.h"
#include "server/runtime/activity/adventure_mercury_openings.h"
#include "middleware/bap/activity_message/native/adventure_player_predicates.h"
#include "middleware/encoding/bit_reader.h"
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

int main(int argc,char** argv) {
    if(argc!=3)return 2;const Fixture fixture(argv[1]);std::filesystem::create_directories(argv[2]);
    {
        namespace cue=dawn::middleware::bap::activity_message::native::cue;
        cue::Batch batch{};auto other=mercury::kOpenings[1].cue.request;
        other.variant=3;other.hasTimer=true;other.timer.anchor=17;
        batch.entries[0]=other;batch.count=1;
        const auto request=mercury::kOpenings[0].cue.request;
        check(opening::append_opening_cue(batch,request) && batch.count==2
            && batch.entries[0]==other && batch.entries[1]==request,"opening preserves another service's entire cue including timer and variant");
        check(opening::append_opening_cue(batch,request) && batch.count==2,"identical opening cue refresh is idempotent");
        auto conflict=request;conflict.variant=1;
        check(!opening::append_opening_cue(batch,conflict) && batch.count==2
            && batch.entries[1]==request,"same-slot conflict cannot replace another request");
        batch.count=batch.entries.size();const auto entries=batch.entries;
        check(!opening::append_opening_cue(batch,mercury::kOpenings[2].cue.request)
            && batch.entries==entries,"full shared cue batch cannot be overwritten");
        ++batch.count;
        check(!opening::append_opening_cue(batch,request),"malformed shared cue count rejects before access");
    }
    const auto find=[&](std::uint16_t i,layouts::RosterGroup& result){if(i>=fixture.groups.size())return false;result=fixture.groups[i];return true;};
    for(std::size_t index=0;index<mercury::kForestOverlays.size();++index) {
        const auto& b=mercury::kForestOverlays[index];auto plan=std::make_unique<overlay::Plan>();auto start=std::make_unique<overlay::Plan>();
        const auto& host=fixture.scenario(b.hostScenario);const auto& selected=fixture.scenario(b.selectedScenario);
        const auto prepared=overlay::prepare(host,selected,b,find,*plan);
        if(!prepared) {
            std::fprintf(stderr,"scenario=%08X selected_count=%u host_hash=%08X selected_hash=%08X\n",b.selectedScenario,selected.authoredGroupCounts[12],host.bubbleHashes[12],selected.bubbleHashes[12]);
            for(unsigned n=0;n<selected.authoredGroupCounts[12];++n) {
                const auto& g=fixture.groups[selected.authoredGroups[12][n]];
                std::fprintf(stderr,"group=%08X object=%08X slots=%u\n",g.registryKey,g.objectTag,g.slotCount);
                for(unsigned k=0;k<g.slotCount;++k)std::fprintf(stderr,"%u/%u class=%08X sense=%08X auth=%08X def=%08X\n",g.slotIndices[k],g.slotTypes[k],g.componentClasses[k],g.senseSchemas[k],g.authSchemas[k],g.descriptorTags[k]);
            }
        }
        check(prepared,"real Forest F root/local exact descriptors");
        check(overlay::prepare(host,selected,mercury::kOpeningOverlays[index],find,*start),"real preceding Lighthouse overlay");
        auto storage=std::make_unique<Storage>();auto roster=baseline(*storage,fixture);
        check(overlay::append(*start,*storage,roster)==overlay::Result::added,"real selected opening admitted");
        const auto rootData=roster.groups[1].slotTypes.data();const auto previousLocal=roster.groups[3].slotTypes.data();
        const auto oldKeys=roster.bubbleSubBlocks[0].keys;
        check(overlay::append_region(*plan,*storage,roster)==overlay::Result::added,"Forest local joins retained selected root");
        check(roster.groupCount==5 && roster.topLevelGroupCount==2 && roster.groups[1].slotTypes.data()==rootData,"same selected root identity and backing");
        check(roster.groups[3].slotTypes.data()==previousLocal && roster.bubbleSubBlocks[0].keys.data()==oldKeys.data(),"Lighthouse and base content retained");
        check(roster.groups[4].key==b.local.key && roster.bubbleSubBlocks[1].bubble==12 && roster.bubbleSubBlocks[1].keys[0]==b.local.key,"only exact Forest local enters scope12");
        check(overlay::append_region(*plan,*storage,roster)==overlay::Result::present && roster.groupCount==5,"regional refresh idempotent");
        check(opening::opening_lifetime_scenario(b,*plan,roster,96,96)==12U,"actual region and destination admit Forest phase12");
        check(!opening::opening_lifetime_scenario(b,*plan,roster,96,120) && !opening::opening_lifetime_scenario(b,*plan,roster,120,96),"manual or stale arrival cannot override real region");
        opening::OpeningFrame lease{};lease.binding=&mercury::kOpenings[index];
        lease.requested=lease.nativeReady=lease.dialogueSubmitted=lease.gatewayRequested=true;
        check(opening::regional_lifetime_scenario(lease,*plan,roster,96,96,120)==12U,"native-ready retained opening admits actual96 with original120 arrival");
        for(const auto arrival:{0U,96U,104U,121U,UINT32_MAX})
            check(!opening::regional_lifetime_scenario(lease,*plan,roster,96,96,arrival),"manual or foreign arrival cannot masquerade as retained admission");
        check(!opening::regional_lifetime_scenario(lease,*plan,roster,120,96,120)
            && !opening::regional_lifetime_scenario(lease,*plan,roster,96,120,120),"resolved and native reported region indices must agree");
        for(const auto reported:{-1,0,95,97,104,120,511})
            check(!opening::regional_lifetime_scenario(lease,*plan,roster,reported,reported,120),"foreign native region cannot acquire Forest phase");
        auto noReceipt=lease;noReceipt.nativeReady=false;
        check(!opening::regional_lifetime_scenario(noReceipt,*plan,roster,96,96,120),"published-only cue cannot authorize regional lease");
        noReceipt=lease;noReceipt.dialogueSubmitted=false;
        check(!opening::regional_lifetime_scenario(noReceipt,*plan,roster,96,96,120),"opening dialogue submission is part of retained lease");
        noReceipt=lease;noReceipt.gatewayRequested=false;
        check(!opening::regional_lifetime_scenario(noReceipt,*plan,roster,96,96,120),"native cue alone cannot authorize unrequested gateway");
        noReceipt=lease;noReceipt.conflictingSelection=true;
        check(!opening::regional_lifetime_scenario(noReceipt,*plan,roster,96,96,120),"changed selection cannot inherit regional admission");
        noReceipt=lease;noReceipt.binding=&mercury::kOpenings[(index+1)%3];
        check(!opening::regional_lifetime_scenario(noReceipt,*plan,roster,96,96,120),"another Adventure's root/local cannot qualify");
        check(opening::opening_lifetime_scenario(*lease.binding->overlay,*start,roster,120,120)==15U,"native return to Mercury still uses unchanged initial arrival");

        {
            // The production profile retains the authored portal registry while
            // adding the selected Forest local. Exercise native navigation
            // admission across 15 -> 12 -> 15 and the archive rejection boundary.
            wire::Snapshot navigation{};navigation.roster=roster;
            navigation.roster.groups[navigation.roster.groupCount++]=expose(fixture.group(0xF25B938B));
            std::vector<std::uint32_t> retainedKeys(roster.bubbleSubBlocks[0].keys.begin(),roster.bubbleSubBlocks[0].keys.end());
            retainedKeys.push_back(0xF25B938B);
            auto blocks=storage->rosterSubBlocks;blocks[0].keys=retainedKeys;
            navigation.roster.bubbleSubBlocks=std::span(blocks).first(roster.bubbleSubBlocks.size());
            navigation.playerKey=0x123456789ABCDEF0;navigation.hasRegion=true;navigation.lifetime=3;
            navigation.cues.entries[0]=mercury::kOpenings[index].cue.request;navigation.cues.count=1;
            check(dawn::middleware::bap::activity_message::native::cue::valid(navigation.cues,navigation.roster),
                "real retained descriptors admit exact navigation cue");
            std::array<std::byte,16384> encoded{};std::size_t written{};
            for(const auto current:{120,96,120})for(const bool archive:{false,true}) {
                navigation.region=current;navigation.lifetimeScenarioOrdinal=current==96?12:15;
                navigation.archiveOmega=archive;
                const bool complete=wire::encode_sensor_auth_update(navigation,encoded,written);
                check(archive ? !complete && !written : complete && written,
                    "native regional navigation publishes; archive retains explicit unsupported-cue rejection");
            }
            navigation.archiveOmega=false;
            --navigation.roster.groupCount;
            check(!wire::encode_sensor_auth_update(navigation,encoded,written) && !written,
                "unadmitted native waypoint target rejects whole publication");
        }

        auto malformed=roster;malformed.groups[1].slotTypes=malformed.groups[1].slotTypes.first(1);
        check(overlay::append_region(*plan,*storage,malformed)==overlay::Result::conflict && malformed.groupCount==5,"root schema cannot be silently replaced");
        malformed=roster;malformed.topLevelGroupCount=1;
        check(overlay::append_region(*plan,*storage,malformed)==overlay::Result::conflict,"global root cannot appear local");
        roster=baseline(*storage,fixture);
        check(overlay::append_region(*plan,*storage,roster)==overlay::Result::added && roster.groupCount==4,"fresh regional snapshot also admits full pair");
        check(opening::opening_lifetime_scenario(b,*plan,roster,96,96)==12U,"fresh snapshot preserves common lifetime route");
        roster=baseline(*storage,fixture);check(overlay::append(*start,*storage,roster)==overlay::Result::added,"restore retained root");
        roster.groupCount=wire::kGroupCapacity;const auto untouched=storage->rosterGroups.back().registryKey;
        check(overlay::append_region(*plan,*storage,roster)==overlay::Result::capacity && roster.groupCount==wire::kGroupCapacity && storage->rosterGroups.back().registryKey==untouched,"capacity failure changes no descriptors");
    }
    namespace predicates=dawn::middleware::bap::activity_message::native::player_predicates;
    constexpr std::uint32_t forest=0xD4C4F182,omega=0x52B968BA;
    predicates::Set set{};const std::array initial{omega};const std::array added{forest,omega};
    check(predicates::unite(set,initial) && predicates::unite(set,added) && set.count==2 && set.hashes[0]==omega && set.hashes[1]==forest,"existing permission order preserved; duplicate ignored");
    check(predicates::unite(set,added) && set.count==2,"repeat permission request idempotent");
    auto bad=set;bad.hashes[1]=omega;check(!predicates::valid(bad) && !predicates::unite(bad,initial),"duplicate existing state rejected");
    namespace placement=dawn::middleware::bap::activity_message::native::placement;
    for(const auto generation:{0U,1U}) {
        const placement::Request r{0xF25B938B,6,15,{},generation};std::array<std::byte,32> body{};bits::Writer w(body);
        check(placement::write(w,r) && w.bit_count()==placement::body_bits(r),"actual generic gateway placement wire");
        char name[64]{};std::snprintf(name,sizeof(name),"placement-generation%u.body",generation);
        std::ofstream out(std::filesystem::path(argv[2])/name,std::ios::binary);out.write(reinterpret_cast<const char*>(body.data()),body.size());check(bool(out),"placement body exported");
    }
    std::array<std::uint32_t,32> full{};for(unsigned i=0;i<32;++i)full[i]=i;
    predicates::Set capacity{};check(predicates::unite(capacity,full),"native capacity32 accepted");const auto before=capacity;
    check(!predicates::unite(capacity,added) && capacity==before,"overflow cannot erase existing permissions");
    for(const bool region:{false,true})for(const auto count:{0U,1U,2U,32U}) {
        predicates::Set permissions{};
        if(count==1)check(predicates::unite(permissions,std::span(added).first(1)),"exact authored F predicate");
        if(count==2)permissions=set;if(count==32)permissions=capacity;
        wire::Snapshot s{};s.playerKey=0x123456789ABCDEF0;s.hasRegion=region;s.region=120;
        s.playerPredicates=permissions;s.omegaPortalPlayerHash=count==2;
        std::array<std::byte,160> out{},archive{};bits::Writer writer(out),archiveWriter(archive);
        check(wire::legacy_write_auth_body(writer,s,0x4786C0E0,13,0,true),"actual full production type13 generic set");
        check(writer.bit_count()==192+32*region+32*count && writer.bit_count()==wire::legacy_auth_body_bits(s,0x4786C0E0,13,0,true),"legacy measured and actual participation body agree");
        s.archiveOmega=true;
        check(wire::write_auth_body(archiveWriter,s,0x4786C0E0,13,0,true)
            && archiveWriter.bit_count()==writer.bit_count() && archiveWriter.bit_count()==wire::auth_body_bits(s,0x4786C0E0,13,0,true)
            && archive==out,"both production body routes preserve exact participation semantics");
        char name[64]{};std::snprintf(name,sizeof(name),"predicates-region%u-count%u.body",region?1U:0U,count);
        std::ofstream file(std::filesystem::path(argv[2])/name,std::ios::binary);file.write(reinterpret_cast<const char*>(out.data()),(writer.bit_count()+7)/8);check(bool(file),"native decoder input exported");
    }
    {
        auto storage=std::make_unique<Storage>();wire::Snapshot s{};s.roster=baseline(*storage,fixture);
        s.playerKey=0x123456789ABCDEF0;s.hasRegion=true;s.region=120;s.lifetime=3;
        std::array<std::byte,16384> packet{};std::size_t written{};
        for(const bool archive:{false,true}) {
            s.archiveOmega=archive;s.playerPredicates={};s.omegaPortalPlayerHash=true;
            check(wire::encode_sensor_auth_update(s,packet,written) && written,"both complete encoders preserve legacy-only predicate path");
            s.playerPredicates.count=33;
            check(!wire::encode_sensor_auth_update(s,packet,written) && !written,"complete packet rejects invalid native predicate capacity");
            s.playerPredicates=capacity;
            check(!wire::encode_sensor_auth_update(s,packet,written) && !written,"legacy plus full distinct set cannot silently overflow");
            s.playerPredicates={};s.playerPredicates.hashes[0]=s.playerPredicates.hashes[1]=0xD4C4F182;s.playerPredicates.count=2;
            check(!wire::encode_sensor_auth_update(s,packet,written) && !written,"duplicate predicate authority rejected before packet publication");
        }
    }
    for(const auto ordinal:{0U,12U,15U}) {
        wire::Snapshot lifetime{};lifetime.lifetime=1;lifetime.lifetimeScenarioOrdinal=ordinal;
        lifetime.region=96;lifetime.hasRegion=true;lifetime.spawnSliceSet=120;
        std::array<std::byte,128> bytes{};bits::Writer writer(bytes);
        check(wire::legacy_write_auth_body(writer,lifetime,0x4786C0E0,17,3,false)
            && writer.bit_count()==wire::legacy_auth_body_bits(lifetime,0x4786C0E0,17,3,false),"regional provider emits real type17 body");
        char name[64]{};std::snprintf(name,sizeof(name),"lifetime-ordinal%u.body",ordinal);
        std::ofstream file(std::filesystem::path(argv[2])/name,std::ios::binary);
        file.write(reinterpret_cast<const char*>(bytes.data()),(writer.bit_count()+7)/8);check(bool(file),"native lifetime decoder input exported");
    }
    std::printf("PASS %u Forest regional admission and player-predicate union checks; staged production publication; live portal acceptance pending\n",checks);
}
