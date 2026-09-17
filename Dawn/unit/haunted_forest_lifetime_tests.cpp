#include "middleware/bap/activity_message/sensor_auth_update.h"
#include "middleware/encoding/bit_reader.h"
#include "middleware/content/packages/tables/scenario_reader.h"
#include "middleware/content/packages/tables/activity_table.h"
#include "middleware/content/packages/tables/internal.h"
#include "server/bap/encrypted/push/activity/activity_arrival.h"
#include "server/runtime/activity/haunted_forest_lifetime_profile.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>
namespace tables=dawn::middleware::content::packages::tables;
namespace wire=dawn::middleware::bap::activity_message::sensor_auth_update;
namespace bits=dawn::middleware::encoding::bits;
namespace hf=dawn::server::runtime::activity::haunted_forest;
namespace dest=dawn::state::activity::destination;
namespace scenario=dawn::state::build_data::scenarios;
namespace arrival=dawn::server::bap::encrypted::push::activity;
unsigned checks{};
#define CHECK(c) do {++checks;if(!(c)){std::fprintf(stderr,"FAIL line%d: %s\n",__LINE__,#c);std::exit(1);}}while(false)
std::vector<std::byte> read(const std::filesystem::path& p) {
 std::ifstream f(p,std::ios::binary|std::ios::ate);CHECK(bool(f));const auto n=f.tellg();CHECK(n>0);
 std::vector<std::byte>b(static_cast<std::size_t>(n));f.seekg(0);f.read(reinterpret_cast<char*>(b.data()),n);CHECK(bool(f));return b;
}
using Body=std::array<std::byte,65>;
Body body(const wire::Snapshot& snapshot,std::uint32_t key=0x4786C0E0,std::uint16_t slot=3) {
 Body bytes{};bits::Writer w(bytes);CHECK(wire::write_auth_body(w,snapshot,key,17,slot,false));CHECK(w.bit_count()==520);return bytes;
}
std::uint32_t ordinal(const Body& bytes) {
 bits::Reader r(bytes);std::uint64_t value{};CHECK(r.skip(72)&&r.read(32,value));return static_cast<std::uint32_t>(value)-0x80000000U;
}
int main(int argc,char**argv) {
 CHECK(argc==3);const std::filesystem::path root(argv[1]);
 const auto activityBytes=read(root/"81327CF0.bin");
 static std::array<dawn::state::build_data::activities::Definition,4095> catalog{};std::size_t count{};
 CHECK(tables::activities::decode(activityBytes,catalog,count));CHECK(count==1170);
 const auto scenarioBytes=read(root/"81550015.bin");tables::Array rows{};CHECK(tables::scenario_bubbles(scenarioBytes,rows));
 tables::Bubble row{};tables::SliceState state{};CHECK(tables::bubble_at(scenarioBytes,rows,13,row));CHECK(tables::slice_state_at(scenarioBytes,row,0,state));
 static scenario::Definition layout{};layout.tag=0x81550015;layout.bubbleCount=static_cast<std::uint8_t>(rows.count);
 layout.bubbleStates[13]=state.enabled?scenario::kBubbleEnabledByte:scenario::kBubbleDisabledByte;
 layout.bubbleHashes[13]=row.nameHash;layout.bubbleMapIndices[13]=static_cast<std::uint16_t>(state.mapBubbleIndex);layout.bubbleStateCounts[13]=static_cast<std::uint8_t>(row.stateCount);
 for(std::uint8_t i=0;i<layout.bubbleCount;++i){tables::Bubble b{};tables::SliceState st{};CHECK(tables::bubble_at(scenarioBytes,rows,i,b));CHECK(tables::slice_state_at(scenarioBytes,b,0,st));layout.bubbleStates[i]=st.enabled?scenario::kBubbleEnabledByte:scenario::kBubbleDisabledByte;layout.bubbleHashes[i]=b.nameHash;layout.bubbleMapIndices[i]=static_cast<std::uint16_t>(st.mapBubbleIndex);layout.bubbleStateCounts[i]=static_cast<std::uint8_t>(b.stateCount);}
 constexpr std::string_view stem="infinite_forest_live";std::memcpy(layout.spawnStem.data(),stem.data(),stem.size());layout.spawnStemLength=static_cast<std::uint8_t>(stem.size());
 dest::DestinationSelection selection{};selection.activityIndex=78;constexpr std::string_view name="infinite_abyss";
 std::memcpy(selection.packageName.data(),name.data(),name.size());selection.packageNameLength=static_cast<std::uint8_t>(name.size());
 selection.hasArrivalBubbleOverride=true;selection.arrivalBubbleOverride=13;selection.hasSpawnSetOverride=true;selection.spawnSetOverride=0x79E3AB1F;
 dawn::state::activity::defaults::DefaultDestination defaults{};
 const auto admitted=[&](const auto&s,const auto&a,const auto&l) {return hf::initial_lifetime_scenario(s,a,l,arrival::arrival_slice_set(defaults,s,name,l));};
 const auto originalSelection=selection;auto selected=admitted(selection,catalog[78],layout);CHECK(selected&&*selected==13);CHECK(selection.arrivalBubbleOverride==originalSelection.arrivalBubbleOverride&&selection.spawnSetOverride==originalSelection.spawnSetOverride);
 for(auto region:{0U,8U,96U,105U,112U,120U,511U,512U,UINT32_MAX})CHECK(!hf::initial_lifetime_scenario(selection,catalog[78],layout,region));
 for(auto index:{79,80,299,1076,-1}){auto s=selection;s.activityIndex=static_cast<std::int16_t>(index);CHECK(!hf::initial_lifetime_scenario(s,catalog[78],layout,104));}
 auto s=selection;s.hasSliceSetOverride=true;s.sliceSetOverride=120;CHECK(!admitted(s,catalog[78],layout));
 s.sliceSetOverride=104;CHECK(admitted(s,catalog[78],layout)==13U);s=selection;s.arrivalBubbleOverride=15;CHECK(!admitted(s,catalog[78],layout));
 s=selection;s.spawnSetOverride=0x12345678;CHECK(admitted(s,catalog[78],layout)==13U);CHECK(s.spawnSetOverride==0x12345678);
 s.packageName[0]='x';CHECK(!hf::initial_lifetime_scenario(s,catalog[78],layout,104));
 for(int mutation=0;mutation<6;++mutation){auto a=catalog[78];if(mutation==0)a.hash^=1;if(mutation==1)a.gameplaySettingsHash^=1;if(mutation==2)a.nativeType=1;if(mutation==3)a.destination=15;if(mutation==4)a.index=79;if(mutation==5)a.package[0]='x';CHECK(!hf::initial_lifetime_scenario(selection,a,layout,104));}
 for(int mutation=0;mutation<9;++mutation){auto l=layout;if(mutation==0)l.tag^=1;if(mutation==1)l.truncated=1;if(mutation==2)l.bubbleCount=19;if(mutation==3)l.bubbleStates[13]=scenario::kBubbleDisabledByte;if(mutation==4)l.bubbleHashes[13]^=1;if(mutation==5)l.bubbleMapIndices[13]=0;if(mutation==6)l.bubbleStateCounts[13]=2;if(mutation==7)l.spawnStem[0]='x';if(mutation==8)l.spawnStemLength=255;CHECK(!hf::initial_lifetime_scenario(selection,catalog[78],l,104));}
 wire::Snapshot snapshot{};snapshot.lifetime=3;snapshot.hasRegion=true;snapshot.region=104;snapshot.hasSpawnOverride=true;snapshot.spawnSliceSet=104;snapshot.spawnSetHash=0x79E3AB1F;
 const auto old=body(snapshot);CHECK(ordinal(old)==0);snapshot.lifetimeScenarioOrdinal=selected;const auto fixed=body(snapshot);CHECK(ordinal(fixed)==13);
 for(std::size_t i=0;i<old.size();++i)if(i<9||i>12)CHECK(old[i]==fixed[i]);
 CHECK(body(snapshot,0x12345678)==old);CHECK(body(snapshot,0x4786C0E0,4)==old);
 snapshot.lifetimeScenarioOrdinal=0;CHECK(body(snapshot)==old);
 for(auto lifetime:wire::kLifetimeStates){snapshot.lifetime=lifetime;snapshot.lifetimeScenarioOrdinal=13;CHECK(ordinal(body(snapshot))==13);}
 snapshot.lifetime=3;snapshot.archiveOmega=true;snapshot.lifetimeScenarioOrdinal=13;CHECK(body(snapshot)==old);
 snapshot.omegaSceneAuthority=true;snapshot.omegaCrownRestriction=dawn::state::activity::omega_crown_respawn::Restriction::enable;CHECK(ordinal(body(snapshot))==14);
 snapshot.archiveOmega=false;snapshot.omegaCrownRestriction=dawn::state::activity::omega_crown_respawn::Restriction::keep;snapshot.omegaMission.generation=1;snapshot.omegaMission.restriction=true;CHECK(ordinal(body(snapshot))==14);
 snapshot.omegaMission={};snapshot.lifetimeScenarioOrdinal=63;CHECK(ordinal(body(snapshot))==63);
 for(auto invalid:{64U,512U,UINT32_MAX}){snapshot.lifetimeScenarioOrdinal=invalid;Body bytes{};bits::Writer w(bytes);CHECK(!wire::write_auth_body(w,snapshot,0x4786C0E0,17,3,false));CHECK(w.bit_count()==0);}
 const std::filesystem::path output(argv[2]);std::filesystem::create_directories(output);
 for(const auto& item:{std::pair{"lifetime-before.body",old},std::pair{"lifetime-haunted13.body",fixed}}){std::ofstream f(output/item.first,std::ios::binary);f.write(reinterpret_cast<const char*>(item.second.data()),item.second.size());CHECK(bool(f));}
 std::printf("PASS %u exact-profile/arrival/lifetime-wire checks; native field+C=13, all other520-bit-body fields unchanged.\n",checks);
}



