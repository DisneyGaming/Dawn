#include "server/runtime/activity/forest_generator_progress.h"
#include "server/runtime/activity/haunted_forest_registries.h"
#include "middleware/encoding/bit_reader.h"
#include "middleware/encoding/bit_writer.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>
namespace service=sunrise::server::runtime::activity::forest_generator;
namespace progress=sunrise::server::runtime::activity::generator_progress;
namespace fg=service::wire;
namespace typed=progress::typed;
namespace sense=progress::sense;
namespace hf=sunrise::server::runtime::activity::haunted_forest::mode;
namespace bits=sunrise::middleware::encoding::bits;
unsigned checks{};
#define CHECK(x) do {++checks;if(!(x)){std::printf("FAILED %d: %s\n",__LINE__,#x);std::exit(1);}}while(false)
struct Blob {std::vector<std::byte> bytes;std::size_t width{};};
template<class F> Blob encoded(F fill) {
    Blob result{std::vector<std::byte>(24000),0};bits::Writer writer(result.bytes);
    fill(writer);result.width=writer.bit_count();std::size_t size{};CHECK(writer.finish(size));
    result.bytes.resize(size);return result;
}
void append(bits::Writer& writer,const Blob& body) {
    bits::Reader reader(body.bytes);for(std::size_t i=0;i<body.width;++i){std::uint64_t value{};CHECK(reader.read(1,value));CHECK(writer.write(value,1));}
}
Blob body(const fg::State& value,std::uint32_t revision,bool root=true) {
    return encoded([&](auto& w){CHECK(w.write(root,1));if(root)CHECK(fg::write_payload(w,value));CHECK(w.write(revision,32));});
}
Blob rootless(std::uint32_t revision){return body({},revision,false);}
struct Object {std::uint16_t slot{};std::uint8_t type{};Blob body{};std::uint32_t key{};};
struct Group {std::uint32_t key{};std::vector<Object> objects;std::int32_t widthAdjust{};};
Blob frame(const std::vector<Group>& groups) {
    return encoded([&](auto& w){
        CHECK(w.write(0x1234,64));CHECK(w.write(0xABCD,64));CHECK(w.write(0,1));CHECK(w.write(0,1));
        for(const auto& group:groups) {
            const auto groupBody=encoded([&](auto& g){
                for(const auto& o:group.objects){CHECK(g.write(1,1));CHECK(g.write(o.key?o.key:group.key,32));CHECK(g.write(o.type+1,7));CHECK(g.write(32768+o.slot,16));append(g,o.body);}
                CHECK(g.write(0,1));
            });
            CHECK(w.write(1,1));CHECK(w.write(group.key,32));
            CHECK(w.write(static_cast<std::uint64_t>(static_cast<std::int64_t>(groupBody.width)+group.widthAdjust),32));append(w,groupBody);
        }
        CHECK(w.write(0,1));CHECK(w.write(0,1));
    });
}
void flip(Blob& b,std::size_t at){CHECK(at<b.width);b.bytes[at/8]^=std::byte(1U<<(7-at%8));}
void set(Blob& b,std::size_t at,std::uint64_t value,std::size_t width) {
    CHECK(at+width<=b.width);
    for(std::size_t i=0;i<width;++i){const auto mask=std::byte(1U<<(7-(at+i)%8));auto& byte=b.bytes[(at+i)/8];byte&=~mask;if((value>>(width-i-1))&1)byte|=mask;}
}
std::unique_ptr<sense::SenseUpdate> parsed(const Blob& b) {
    auto out=std::make_unique<sense::SenseUpdate>();std::size_t width{};
    CHECK(sense::parse_sense_update(b.bytes,*out,width));CHECK(width==b.width);return out;
}
void rejected(const Blob& b) {
    auto out=std::make_unique<sense::SenseUpdate>();out->objectCount=111;out->objects[0].hasGeneratorProgress=true;std::size_t width=99;
    CHECK(!sense::parse_sense_update(b.bytes,*out,width));CHECK(width==0 && out->objectCount==0 && out->groupCount==0 && !out->objects[0].hasGeneratorProgress);
}
std::vector<std::byte> load(const std::filesystem::path& p) {
    std::ifstream f(p,std::ios::binary|std::ios::ate);CHECK(f.good());const auto n=f.tellg();CHECK(n>0);
    std::vector<std::byte> out(static_cast<std::size_t>(n));f.seekg(0);f.read(reinterpret_cast<char*>(out.data()),n);CHECK(f.good());return out;
}
template<class T> T field(const std::vector<std::byte>& b,std::size_t at){T out{};CHECK(at+sizeof out<=b.size());std::memcpy(&out,b.data()+at,sizeof out);return out;}
// Existing native-verified fixtures are the payload oracle. Synthetic data only
// varies envelope framing, owner metadata and specifically rejected conditions.
void fixture_cases(const std::filesystem::path& dir) {
    for(const auto name:{"authored","ignition","disabled","varied","maximum"}) {
        const auto bytes=load(dir/(std::string(name)+".wire.bin"));const auto native=load(dir/(std::string(name)+".native-decoded.bin"));
        bits::Reader r(bytes);fg::State state;CHECK(typed::payload(r,state));
        const auto width=bytes.size()*8-r.remaining_bits();CHECK(width==fg::body_bits(state));
        CHECK(state.primary.seed==field<std::uint32_t>(native,0));CHECK(state.reportedSeed==field<std::uint32_t>(native,0x550));
        for(std::size_t i=0;i<32;++i)CHECK(state.areas[i]==field<std::uint8_t>(native,0x554+i));
        for(std::size_t i=0;i<64;++i)CHECK(state.groups[i]==field<std::uint8_t>(native,0x574+i));
        const auto nativeBody=encoded([&](auto& w){CHECK(w.write(1,1));append(w,{bytes,width});CHECK(w.write(0xFABE0001,32));});
        const auto pe=encoded([](auto& w){CHECK(w.write(1,1));CHECK(w.write(2,5));CHECK(w.write(0xFFFF1234ABCD0000,64));CHECK(w.write(0xABAB555599994444,64));CHECK(w.write(17,16));CHECK(w.write(88,32));});
        // Before/after objects and a second group must keep exact identities.
        auto packet=frame({{0x34D23982,{{7,1,rootless(41)},{98,37,nativeBody},{60,70,pe},{49,23,rootless(43)}}},
            {0x76543210,{{2,37,nativeBody},{5,30,rootless(99)}}}});
        const auto out=parsed(packet);CHECK(out->objectCount==6 && out->groupCount==2);
        CHECK(out->objects[0].nativeRevision==41 && out->objects[2].nativeSchema==0x808094F0 && out->objects[2].nativeRevision==88);
        CHECK(out->objects[3].nativeRevision==43 && out->objects[5].nativeRevision==99);
        for(const auto i:{1,4}) {const auto& o=out->objects[i];CHECK(o.bodyBits==width+33 && o.hasNativeSchema && o.hasRootDelta && o.hasGeneratorProgress);
            CHECK(o.nativeSchema==0x80805006 && o.nativeRevision==0xFABE0001 && o.revision==o.nativeRevision);
            CHECK(o.generatorProgress==typed::summarize(state) && !o.inferredBodyWidth && o.hasForestGeneratorState);
            CHECK(o.forestSeed==state.reportedSeed && o.forestActive32==o.generatorProgress.clearedAreas && o.forestActive64==o.generatorProgress.openedGroups);}
        CHECK(out->objects[4].registryKey==0x76543210 && out->objects[4].slotIndex==2 && out->objects[4].groupOrdinal==1);
        for(std::size_t cut=0;cut<packet.bytes.size();++cut) {auto truncated=packet;truncated.bytes.resize(cut);rejected(truncated);}
        auto corrupted=packet;flip(corrupted,packet.width-1);rejected(corrupted);
        if(packet.width%8){corrupted=packet;corrupted.bytes.back()|=std::byte{1};rejected(corrupted);}
        rejected(frame({{0x34D23982,{{98,37,nativeBody}},1}}));rejected(frame({{0x34D23982,{{98,37,nativeBody}},-1}}));
        rejected(frame({{0x34D23982,{{98,37,nativeBody},{23,1,rootless(3),0xBADBAD}}}}));
    }
    const auto out=parsed(frame({{0x34D23982,{{98,37,rootless(777)},{23,1,rootless(3)}}}}));
    CHECK(out->objects[0].bodyBits==33 && !out->objects[0].hasGeneratorProgress && !out->objects[0].hasRootDelta && out->objects[0].nativeRevision==777);
    // Count101 is structurally invalid even with enough trailing bytes. The
    // parser must not fall back to an inferred body for a reflected type37.
    fg::State maximum;maximum.primary.blockedCount=100;maximum.secondary.blockedCount=100;
    auto invalid=body(maximum,5);set(invalid,1+468,101,7);
    rejected(frame({{0x34D23982,{{98,37,invalid},{23,1,rootless(3)}}}}));
    invalid=body(maximum,5);set(invalid,1+475+4800+468,101,7);
    rejected(frame({{0x34D23982,{{98,37,invalid},{23,1,rootless(3)}}}}));
    // Invalid floating-point recipe values are rejected rather than hashed.
    invalid=body({},5);set(invalid,1+56,0x7FC00000,32);rejected(frame({{0x34D23982,{{98,37,invalid}}}}));
}
void legacy_cases() {
    auto legacy=encoded([](auto& w){for(unsigned i=0;i<1783;++i)CHECK(w.write(i==0 || i==990 || i==1238 || i==1246 || i==1750 || i==1782,1));});
    auto out=parsed(frame({{0x2763EC97,{{1,37,legacy},{2,1,rootless(27)}}}}));
    const auto& o=out->objects[0];CHECK(o.hasForestGeneratorState && o.hasGeneratorProgress && o.hasNativeSchema && !o.inferredBodyWidth);
    CHECK(o.nativeSchema==typed::kSchema && o.nativeRevision==o.forestRevision && o.generatorProgress.canonicalFlags);
    CHECK(o.bodyBits==1783 && o.forestRevision==1 && o.forestActive32==0x80000001 && o.forestActive64==0x8000000000000001);
    CHECK(out->objects[1].nativeRevision==27);
    auto noncanonical=legacy;flip(noncanonical,991);
    out=parsed(frame({{0x2763EC97,{{1,37,noncanonical}}}}));
    CHECK(out->objects[0].hasGeneratorProgress && !out->objects[0].generatorProgress.canonicalFlags);
    // Omega retains the same typed rootless reports as every reflected generator.
    out=parsed(frame({{0x2763EC97,{{1,37,rootless(27)}}}}));CHECK(!out->objects[0].inferredBodyWidth && out->objects[0].hasNativeSchema && out->objects[0].nativeRevision==27 && !out->objects[0].hasForestGeneratorState && !out->objects[0].hasGeneratorProgress);
    // Another slot in that registry uses the generic reflected path.
    out=parsed(frame({{0x2763EC97,{{2,37,body({},28)}}}}));CHECK(out->objects[0].hasGeneratorProgress && out->objects[0].hasForestGeneratorState);
}
sense::SenseObject report(const fg::State& value,std::uint32_t revision,bool root=true) {
    return parsed(frame({{0x34D23982,{{98,37,body(value,revision,root)}}}}))->objects[0];
}
void observer_cases() {
    constexpr service::Owner owner{0x1234,{567}};constexpr std::uint64_t boot=18;
    fg::State initial;initial.primary.enabled=true;initial.primary.seed=9001;initial.primary.overrides=fg::Seed|fg::Enabled;
    auto changed=initial;changed.primary.seed=9002;changed.primary.blockedCount=1;changed.primary.blockedCells[0]={1,2,3};
    auto disabled=initial;disabled.primary.enabled=false;
    std::array<service::Action,3> actions{{{1,initial},{2,changed},{3,disabled}}};
    const service::Capability cap{&hf::kRegistries[0],98,actions};CHECK(service::valid(cap));
    service::Service accepted;CHECK(accepted.begin(owner,boot,{&cap,1}));
    progress::Observer mirror;CHECK(!mirror.begin({},boot) && !mirror.begin(owner,0));CHECK(mirror.begin(owner,boot));CHECK(!mirror.begin(owner,boot));
    CHECK(mirror.bind(accepted,cap,13)==progress::Result::unbound);
    CHECK(accepted.request({owner,boot,1,1,0x34D23982,1,98},13)==service::Result::accepted);
    CHECK(mirror.bind(accepted,cap,12)==progress::Result::invalid);CHECK(mirror.size()==0);
    CHECK(mirror.bind(accepted,cap,13)==progress::Result::accepted);CHECK(mirror.size()==1);
    const auto* snapshot=mirror.find(0x34D23982,98);CHECK(snapshot && snapshot->definition==0x8155015B && snapshot->seed==9001 && !snapshot->observed);
    auto state=initial;state.reportedSeed=9001;state.areas[0]=1;state.areas[31]=1;state.groups[0]=1;state.groups[63]=1;
    const auto good=report(state,100);
    const auto receive=[&](const auto& object){return mirror.observe(owner,boot,13,accepted,object);};
    service::Service mercury;CHECK(mercury.begin(owner,boot,{}));progress::Observer noBinding;CHECK(noBinding.begin(owner,boot));
    CHECK(noBinding.bind(mercury,cap,13)==progress::Result::unbound);CHECK(noBinding.observe(owner,boot,13,mercury,good)==progress::Result::unbound);CHECK(noBinding.size()==0);
    CHECK(mirror.observe({owner.sessionId+1,owner.incarnation},boot,13,accepted,good)==progress::Result::stale);
    CHECK(mirror.observe({owner.sessionId,{owner.incarnation.value+1}},boot,13,accepted,good)==progress::Result::stale);
    CHECK(mirror.observe(owner,boot+1,13,accepted,good)==progress::Result::stale);
    CHECK(mirror.observe(owner,boot,12,accepted,good)==progress::Result::stale);
    for(unsigned kind=0;kind<15;++kind) {
        auto bad=good;
        if(kind==0)bad.registryKey^=1;if(kind==1)--bad.slotIndex;if(kind==2)bad.slotType=1;
        if(kind==3)bad.hasNativeSchema=false;if(kind==4)bad.nativeSchema=0x80805007;if(kind==5)bad.inferredBodyWidth=true;
        if(kind==6)bad.revision++;if(kind==7)bad.bodyBits--;if(kind==8)bad.generatorProgress.canonicalFlags=false;
        if(kind==9)bad.generatorProgress.primarySeed++;if(kind==10)bad.generatorProgress.reportedSeed++;
        if(kind==11)bad.generatorProgress.recipeHash^=1;if(kind==12)bad.generatorProgress.primaryEnabled=false;
        if(kind==13)bad.generatorProgress.primaryOverrides=fg::Seed;if(kind==14)bad.generatorProgress.primaryOverrides=fg::Enabled;
        CHECK(receive(bad)==(kind<3?progress::Result::unbound:progress::Result::invalid));CHECK(!snapshot->observed && snapshot->nativeRevision==0);
    }
    auto missingSeed=initial;CHECK(receive(report(missingSeed,101))==progress::Result::invalid);
    auto noncanonical=state;noncanonical.areas[17]=2;CHECK(receive(report(noncanonical,101))==progress::Result::invalid);
    noncanonical=state;noncanonical.groups[32]=255;CHECK(receive(report(noncanonical,101))==progress::Result::invalid);
    CHECK(receive(good)==progress::Result::accepted);CHECK(snapshot->observed && snapshot->nativeRevision==100 && snapshot->clearedAreas==0x80000001 && snapshot->openedGroups==0x8000000000000001);
    CHECK(receive(good)==progress::Result::stale);CHECK(receive(report(state,99))==progress::Result::stale);
    CHECK(receive(report(state,777,false))==progress::Result::noDelta);CHECK(snapshot->nativeRevision==100);
    state.areas={};state.groups={};state.groups[22]=1;
    CHECK(receive(report(state,101))==progress::Result::accepted);CHECK(snapshot->clearedAreas==0 && snapshot->openedGroups==(1ULL<<22));
    // Refreshing identical authority retains the native revision floor.
    CHECK(accepted.request({owner,boot,2,2,0x34D23982,1,98},13)==service::Result::unchanged);
    CHECK(mirror.bind(accepted,cap,13)==progress::Result::unchanged);CHECK(snapshot->observed && snapshot->nativeRevision==101 && snapshot->acceptedRevision==3);
    CHECK(receive(good)==progress::Result::stale);
    CHECK(accepted.request({owner,boot,3,3,0x34D23982,2,98},13)==service::Result::accepted);
    CHECK(receive(report(state,102))==progress::Result::stale);CHECK(mirror.bind(accepted,cap,13)==progress::Result::accepted);
    CHECK(!snapshot->observed && snapshot->nativeRevision==101 && snapshot->clearedAreas==0 && snapshot->openedGroups==0 && snapshot->seed==9002);
    CHECK(receive(report(state,103))==progress::Result::invalid);
    changed.reportedSeed=9002;CHECK(receive(report(changed,100))==progress::Result::stale);CHECK(receive(report(changed,102))==progress::Result::accepted);
    CHECK(accepted.request({owner,boot,4,4,0x34D23982,3,98},13)==service::Result::accepted);
    CHECK(receive(report(changed,103))==progress::Result::stale);CHECK(mirror.bind(accepted,cap,13)==progress::Result::invalid);
    CHECK(accepted.request({owner,boot,5,5,0x34D23982,2,98},13)==service::Result::accepted);CHECK(mirror.bind(accepted,cap,13)==progress::Result::unchanged);
    CHECK(receive(report(changed,UINT32_MAX))==progress::Result::accepted);CHECK(receive(report(changed,1))==progress::Result::stale);
    // A newly admitted incarnation may start a fresh native revision floor;
    // the old receiver context cannot submit into it.
    const service::Owner newOwner{owner.sessionId,{owner.incarnation.value+1}};service::Service newService;
    CHECK(newService.begin(newOwner,boot,{&cap,1}));CHECK(newService.request({newOwner,boot,1,1,0x34D23982,1,98},13)==service::Result::accepted);
    progress::Observer fresh;CHECK(fresh.begin(newOwner,boot));CHECK(fresh.bind(newService,cap,13)==progress::Result::accepted);
    CHECK(fresh.observe(owner,boot,13,newService,good)==progress::Result::stale);CHECK(fresh.observe(newOwner,boot,13,newService,report(state,1))==progress::Result::accepted);
    CHECK(mirror.bind(newService,cap,13)==progress::Result::stale);
    // Unserialized unused cells never change the recipe consistency hash.
    auto hashSame=initial;hashSame.primary.blockedCells[99]={12,13,14};CHECK(typed::recipe_hash(initial)==typed::recipe_hash(hashSame));
    hashSame.primary.blockedCount=100;CHECK(typed::recipe_hash(initial)!=typed::recipe_hash(hashSame));
}
void archived_r12(const std::filesystem::path& directory) {
    // These are full original game packets, not writer-generated fixtures.
    fg::State expected;expected.primary.seed=2387345670;expected.primary.enabled=true;
    expected.primary.overrides=fg::Seed|fg::Enabled;
    constexpr std::uint32_t areas[]{0,0x400,0x400,0x600,0x600,0x601};
    constexpr std::uint64_t groups[]{0x8421,0x8421,0x9421,0x9421,0x9431,0x9431};
    for(unsigned index=1;index<=6;++index) {
        char filename[32]{};std::snprintf(filename,sizeof filename,"sense-%02u.bin",index);
        std::ifstream file(directory/filename,std::ios::binary|std::ios::ate);CHECK(file.good());
        const auto size=file.tellg();CHECK(size==255);
        std::vector<std::byte> bytes(static_cast<std::size_t>(size));file.seekg(0);file.read(reinterpret_cast<char*>(bytes.data()),size);CHECK(file.good());
        auto output=std::make_unique<sense::SenseUpdate>();std::size_t consumed{};
        CHECK(sense::parse_sense_update(bytes,*output,consumed));CHECK(consumed==2037 && output->objectCount==1);
        const auto& object=output->objects[0];const auto& p=object.generatorProgress;
        CHECK(object.registryKey==0x34D23982 && object.slotIndex==98 && object.slotType==37);
        CHECK(object.hasGeneratorProgress && object.hasRootDelta && !object.inferredBodyWidth);
        CHECK(object.nativeSchema==0x80805006 && object.nativeRevision==index && object.bodyBits==1783);
        CHECK(p.recipeHash==typed::recipe_hash(expected) && p.primarySeed==2387345670 && p.reportedSeed==2387345670 && p.canonicalFlags);
        CHECK(p.clearedAreas==areas[index-1] && p.openedGroups==groups[index-1]);
    }
}
int main(int argc,char** argv) {
    if(argc==2 && std::strcmp(argv[1],"--self-test")==0) {
        legacy_cases();observer_cases();
        std::printf("Generator self-test: %u checks passed; original native fixture comparisons not run.\n",checks);
        return 0;
    }
    if(argc!=3)return 2;fixture_cases(argv[1]);legacy_cases();observer_cases();archived_r12(argv[2]);
    std::printf("Generator progress/parser: %u checks passed. Progress=%zu Observer=%zu SenseObject=%zu SenseUpdate=%zu bytes.\n",checks,sizeof(typed::Progress),sizeof(progress::Observer),sizeof(sense::SenseObject),sizeof(sense::SenseUpdate));
}
