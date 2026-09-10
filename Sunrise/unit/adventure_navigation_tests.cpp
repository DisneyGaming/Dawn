#include "../src/server/runtime/activity/adventure_cue_feedback.h"
#include "../src/server/runtime/activity/adventure_mercury_openings.h"
#include "../src/middleware/encoding/bit_writer.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <vector>
namespace cue=sunrise::middleware::bap::activity_message::native::cue;
namespace feedback=sunrise::server::runtime::activity::adventure::cue_feedback;
namespace bits=sunrise::middleware::encoding::bits;
unsigned checks{};
void check(bool ok,const char* what){++checks;if(!ok){std::fprintf(stderr,"FAIL %s\n",what);std::exit(1);}}
std::vector<std::byte> read(const std::filesystem::path& path){
    std::ifstream f(path,std::ios::binary|std::ios::ate);check(bool(f),"fixture opens");
    const auto n=f.tellg();check(n>0 && n<8192,"bounded fixture");std::vector<std::byte> b(static_cast<std::size_t>(n));
    f.seekg(0);f.read(reinterpret_cast<char*>(b.data()),n);check(bool(f),"complete fixture");return b;
}
template<class Bytes> void write(const std::filesystem::path& path,const Bytes& bytes){
    std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());check(bool(f),"export complete body");
}
struct Group {std::uint32_t key;std::span<const std::uint8_t> slotTypes,slotFlags;std::span<const std::uint16_t> slotIndices;};
struct Block {std::uint32_t bubble;std::span<const std::uint32_t> keys;std::span<const std::uint8_t> presence;};
struct Roster {std::array<Group,4> groups{};std::size_t groupCount{},topLevelGroupCount{};std::span<const Block> bubbleSubBlocks;};
int main(int argc,char** argv){
    if(argc!=3)return 2;const std::filesystem::path fixture(argv[1]),out(argv[2]);std::filesystem::create_directories(out);
    cue::Request initial{0x08551BCF,0xC9E4C596,0,0};
    const auto original=read(fixture/"navigation-811C9DC5--1--1.body");
    const auto originalDecoded=read(fixture/"navigation-811C9DC5--1--1.decoded.bin");
    std::array<std::byte,601> bytes{};bits::Writer w(bytes);
    check(cue::write(w,initial) && w.bit_count()==4802 && std::equal(bytes.begin(),bytes.end(),original.begin()),"default native body unchanged");
    check(cue::matches_fields(originalDecoded,initial),"default full original body qualifier unchanged");
    auto target=initial;target.navigation[0]={0xF25B938B,6,15};
    bytes={};w=bits::Writer(bytes);check(cue::write(w,target) && w.bit_count()==4802,"target preserves exact native body size");
    const auto nativeBody=read(fixture/"navigation-F25B938B-4-6.body"),nativeDecoded=read(fixture/"navigation-F25B938B-4-6.decoded.bin");
    check(std::equal(bytes.begin(),bytes.end(),nativeBody.begin()),"target encoder equals independent original-decoded wire");
    const auto expected=cue::decoded(target);
    check(std::equal(expected.begin(),expected.end(),nativeDecoded.begin()),"all768 native decoded bytes equal");
    check(cue::matches_fields(nativeDecoded,target) && !cue::matches_fields(originalDecoded,target)
        && !cue::matches_fields(nativeDecoded,initial),"receipt requires exact target presence");
    for(unsigned ring=0;ring<3;++ring)for(unsigned slot=0;slot<4;++slot){
        auto r=initial;r.ring=static_cast<std::uint8_t>(ring);r.navigation[slot]={0xF25B938B,6,15};
        bytes={};w=bits::Writer(bytes);check(cue::write(w,r) && w.bit_count()==4802,"bounded ring target encodes");
        const auto decoded=cue::decoded(r);const auto at=0x10+ring*0xF8+0x68+slot*0x24;
        check(feedback::field<std::uint32_t>(decoded,at)==0xF25B938B && decoded[at+4]==std::byte{4}
            && feedback::field<std::uint16_t>(decoded,at+6)==6,"only selected ring target populated");
        char stem[64]{};std::snprintf(stem,sizeof stem,"ring%u-target%u",ring,slot);
        write(out/(std::string(stem)+".body"),bytes);write(out/(std::string(stem)+".decoded.bin"),decoded);
    }
    const std::array<std::uint8_t,1> cueTypes{68},placementTypes{4},flags{3},present{1},absent{0};
    const std::array<std::uint16_t,1> cueSlots{0},placementSlots{6};
    const std::array<std::uint32_t,1> keys{0xF25B938B};
    std::array<Block,2> blocks{{{15,keys,present},{15,keys,present}}};
    Roster roster{};roster.groups[0]={0x08551BCF,cueTypes,flags,cueSlots};
    roster.groups[1]={0xF25B938B,placementTypes,flags,placementSlots};roster.groupCount=2;roster.topLevelGroupCount=1;
    roster.bubbleSubBlocks=std::span(blocks).first(1);cue::Batch batch{};batch.entries[0]=target;batch.count=1;
    check(cue::valid(batch,roster),"exact authored source scope qualifies");
    auto bad=roster;bad.groupCount=1;check(!cue::valid(batch,bad),"unadmitted target rejects");
    bad=roster;bad.groups[1].slotTypes=cueTypes;check(!cue::valid(batch,bad),"wrong target type rejects");
    bad=roster;bad.groups[1].slotIndices=cueSlots;check(!cue::valid(batch,bad),"wrong target slot rejects");
    bad=roster;bad.topLevelGroupCount=2;check(!cue::valid(batch,bad),"target scope cannot become global");
    blocks[0].bubble=12;check(!cue::valid(batch,roster),"foreign bubble rejects");blocks[0].bubble=15;
    blocks[0].presence=absent;check(!cue::valid(batch,roster),"absent native scope rejects");blocks[0].presence=present;
    bad=roster;bad.bubbleSubBlocks=blocks;check(!cue::valid(batch,bad),"duplicate native scope rejects");
    bad=roster;bad.groups[2]=bad.groups[1];bad.groupCount=3;check(!cue::valid(batch,bad),"duplicate target group rejects");
    for(const auto hash:{UINT32_MAX,cue::kAbsent}){auto b=target;b.navigation[0].registry=hash;check(!cue::valid(b),"invalid target hash rejects");}
    auto invalid=target;invalid.navigation[1]=invalid.navigation[0];check(!cue::valid(invalid),"duplicate ring targets reject");
    invalid=target;invalid.navigation[0].slot=32768;check(!cue::valid(invalid),"out-of-range target slot rejects");
    invalid=target;invalid.navigation[0].bubble=64;check(!cue::valid(invalid),"out-of-range target scope rejects");
    invalid=target;invalid.navigation[0].registry=0;check(!cue::valid(invalid),"partial absent target rejects");
    // Both references in one real authored tuple: Crossroads' engagement
    // target is separate from the Forest F placement used for navigation.
    const std::array<std::uint8_t,2> eventTypes{70,68},eventFlags{3,3};
    const std::array<std::uint16_t,2> eventSlots{106,107};
    const std::array<std::uint32_t,2> eventKeys{0xC8229B2B,0xF25B938B};
    const std::array<std::uint8_t,2> eventPresent{1,1};
    const std::array<Block,1> eventBlocks{{{15,eventKeys,eventPresent}}};
    Roster eventRoster{};eventRoster.groups[0]={0xC8229B2B,eventTypes,eventFlags,eventSlots};
    eventRoster.groups[1]={0xF25B938B,placementTypes,flags,placementSlots};
    eventRoster.groupCount=2;eventRoster.bubbleSubBlocks=eventBlocks;
    for(unsigned ring=0;ring<3;++ring) {
        cue::Request combined{0xC8229B2B,0x00D1C5B9,107,static_cast<std::uint8_t>(ring),15};
        combined.readiness={0xC8229B2B,70,106};combined.navigation[0]={0xF25B938B,6,15};
        cue::Batch both{};both.count=1;both.entries[0]=combined;
        check(cue::valid(both,eventRoster),"independent authored readiness and navigation targets both admitted");
        bytes={};w=bits::Writer(bytes);check(cue::write(w,combined) && w.bit_count()==4802,"combined refs retain native body size");
        const auto body=cue::decoded(combined);const auto at=0x10+ring*0xF8+0x68;
        check(feedback::field<std::uint32_t>(body,0)==0xC8229B2B && body[4]==std::byte{70}
            && feedback::field<std::uint16_t>(body,6)==106
            && feedback::field<std::uint32_t>(body,at)==0xF25B938B && body[at+4]==std::byte{4},
            "first top-level reference cannot alias active-ring target");
        auto foreign=both;foreign.entries[0].readiness.registry=0xF25B938B;
        check(!cue::valid(foreign,eventRoster),"existing first-reference same-registry restriction preserved");
        foreign=both;foreign.entries[0].navigation[0].registry=0xC8229B2B;
        check(!cue::valid(foreign,eventRoster),"readiness registry cannot substitute for navigation placement");
        char stem[64]{};std::snprintf(stem,sizeof stem,"combined-ring%u",ring);
        write(out/(std::string(stem)+".body"),bytes);write(out/(std::string(stem)+".decoded.bin"),body);
    }
    for(const auto& opening:sunrise::server::runtime::activity::adventure::mercury::kOpenings) {
        check(opening.cue.request.navigation[0]==cue::PlacementTarget{0xF25B938B,6,15}
            && opening.cue.request.readiness==cue::Reference{},"each Adventure binds exact F source without engagement reference");
    }
    const std::array<std::uint32_t,3> definitions{0x80F46D67,0x80F46D7B,0x80F46D8B};
    const std::array<std::uint32_t,3> registries{0x08551BCF,0x9104CE05,0x3DB08419};
    for(unsigned i=0;i<definitions.size();++i){
        char stem[64]{};std::snprintf(stem,sizeof stem,"%08X.manager-entry.bin",definitions[i]);const auto entry=read(fixture/"navigation-apply"/stem);
        feedback::Ticket ticket{};ticket.owner={0x9EAA300100200001,{1}};ticket.boot=7;ticket.definitionRevision=9;
        ticket.selectionRevision=3;ticket.activity=static_cast<std::int16_t>(1076+i);ticket.definition=definitions[i];ticket.definitionOffset=0xB88;
        ticket.table=0x80F46D25+i;ticket.stringBank=feedback::field<std::uint32_t>(entry,0x58);ticket.title=0x099C20CB;ticket.detail=0x9661E8EB;
        ticket.request=target;ticket.request.registry=registries[i];auto prior=expected;prior[0x10]^=std::byte{1};
        feedback::Capture capture{ticket,{0x1234,0x180},1,feedback::kProducerRva,expected,prior,expected,entry,0,1,true,true,true};
        feedback::Observation observation{};
        check(feedback::qualify(ticket,capture,observation)==feedback::Result::accepted,"original full native manager receipt accepts exact navigation body");
        auto wrong=expected;wrong[0x78]^=std::byte{1};capture.incoming=wrong;capture.applied=wrong;
        check(feedback::qualify(ticket,capture,observation)==feedback::Result::body,"qualified observer refuses wrong native target");
    }
    std::printf("PASS %u optional navigation codec, native manager receipt and exact roster target checks\n",checks);
}
