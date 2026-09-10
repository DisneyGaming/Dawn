#include "middleware/bap/activity_message/native/public_event_participant_authority.h"
#include "middleware/encoding/bit_writer.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <vector>
namespace wire=sunrise::middleware::bap::activity_message::native::event_participant;
namespace cue=sunrise::middleware::bap::activity_message::native::cue;
namespace engagement=sunrise::middleware::bap::activity_message::native::engagement;
namespace bits=sunrise::middleware::encoding::bits;
unsigned checks{};
void check(bool v,const char* m){++checks;if(!v){std::fprintf(stderr,"FAIL %u: %s\n",checks,m);std::exit(1);}}
void save(const std::filesystem::path& p,std::span<const std::byte> bytes){std::ofstream o(p,std::ios::binary);o.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());check(o.good(),"exported actual production payload");}
struct Group {std::uint32_t key;std::span<const std::uint8_t> slotTypes,slotFlags;std::span<const std::uint16_t> slotIndices;};
struct Block {std::uint32_t bubble;std::span<const std::uint32_t> keys;std::span<const std::uint8_t> presence,states;};
struct Roster {std::array<Group,2> groups{};std::size_t groupCount{},topLevelGroupCount{};std::vector<Block> bubbleSubBlocks;};
int main(int argc,char** argv){
    if(argc!=2)return 2;const std::filesystem::path root(argv[1]);std::filesystem::create_directories(root);
    wire::Request r{0xC8229B2B,110,15};r.identifierEncoding=engagement::IdentifierEncoding::integer;
    unsigned index{};
    for(auto encoding:{engagement::IdentifierEncoding::integer,engagement::IdentifierEncoding::byte_array})
        for(auto entity:{0ULL,0x123456789ABCDEF0ULL})for(bool hasRegion:{false,true}){
            auto q=r;q.entity=entity;q.identifierEncoding=encoding;
            if(hasRegion){q.region={r.registry,31,116};q.leaveDelay=7.5F;}
            std::array<std::byte,23> bytes{};bits::Writer writer(bytes);
            check(wire::write(writer,q) && writer.bit_count()==wire::kBits,"native type71 width");
            const auto expected=wire::decoded(q);check(wire::matches_fields(expected,q),"decoded full fields");
            auto padding=expected;padding[17]=std::byte{0xAD};check(wire::matches_fields(padding,q),"native padding ignored");
            padding[4]^=std::byte{1};check(!wire::matches_fields(padding,q),"different entity rejected");
            const auto stem="participant-"+std::to_string(index++);save(root/(stem+".body"),bytes);save(root/(stem+".expected"),expected);
        }
    auto bad=r;bad.identifierEncoding=engagement::IdentifierEncoding::unknown;check(!wire::valid(bad),"unqualified ID encoding rejected");
    bad=r;bad.entity=UINT64_MAX;check(!wire::valid(bad),"invalid entity sentinel rejected");
    bad=r;bad.region.registry++;check(!wire::valid(bad),"foreign region rejected");
    bad=r;bad.leaveDelay=-1;check(!wire::valid(bad),"negative delay rejected");
    cue::Request cr{r.registry,0x00D1C5B9,107,0,15};cr.readiness={r.registry,70,106};
    std::array<std::byte,601> before{},after{};bits::Writer w1(before);check(cue::write(w1,cr),"existing cue valid");
    save(root/"cue-before.body",before);save(root/"cue-before.expected",cue::decoded(cr));
    cr.publicEvent={r.registry,71,r.slot};bits::Writer w2(after);check(cue::write(w2,cr) && w2.bit_count()==4802,"second reference retains exact width");
    save(root/"cue-after.body",after);save(root/"cue-after.expected",cue::decoded(cr));
    unsigned changes{};const auto d2=cue::decoded(cr);cr.publicEvent={};const auto d1=cue::decoded(cr);
    for(std::size_t i=0;i<d1.size();++i)if(d1[i]!=d2[i]){check(i>=8 && i<16,"only native second reference fields changed");++changes;}
    check(changes>0,"native reference changed");cr.publicEvent={r.registry,70,r.slot};check(!cue::valid(cr),"wrong public-event controller type rejected");
    cr.publicEvent={r.registry,71,r.slot};
    std::array<std::uint8_t,3> types{68,70,71},flags{2,3,3};std::array<std::uint8_t,1> presence{1},states{0x80};std::array<std::uint16_t,3> slots{107,106,110};std::array<std::uint32_t,1> keys{r.registry};
    Roster roster;roster.groupCount=1;roster.groups[0]={r.registry,types,flags,slots};roster.bubbleSubBlocks.push_back({15,keys,presence,states});
    wire::Batch batch;batch.count=1;batch.entries[0]=r;cue::Batch cues;cues.count=1;cues.entries[0]=cr;
    check(wire::valid(batch,roster,120) && cue::valid(cues,roster),"authored participant and linked directive admitted");
    check(wire::valid(batch,roster,128),"retained owner admission independent of movement");
    flags[2]=1;check(!wire::valid(batch,roster,120),"sense-only participant rejected");flags[2]=2;
    check(!cue::valid(cues,roster),"cue requires actual readable participant sensor");flags[2]=3;
    presence[0]=0;check(!wire::valid(batch,roster,120) && !cue::valid(cues,roster),"withdrawn registry rejected");presence[0]=1;
    batch.entries[1]=r;batch.count=2;check(!wire::valid(batch,roster,120),"duplicate authority rejected");
    std::printf("PASS %u participant codec/reference/admission checks; native UI rendering remains acceptance\n",checks);
}
