#include "server/runtime/activity/public_event_engagement_feedback.h"
#include "server/runtime/activity/public_event_engagement_gate.h"
#include "middleware/encoding/bit_writer.h"
#include "middleware/encoding/bit_reader.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <vector>
namespace f=dawn::server::runtime::activity::public_event::engagement_feedback;
namespace wire=f::wire;
namespace bits=dawn::middleware::encoding::bits;
unsigned checks{};
void check(bool value,const char* what){++checks;if(!value){std::fprintf(stderr,"FAIL %u: %s\n",checks,what);std::exit(1);}}
std::vector<std::byte> read(const std::filesystem::path& path){
    std::ifstream file(path,std::ios::binary|std::ios::ate);check(bool(file),"fixture opens");
    const auto size=file.tellg();check(size>0 && size<10000,"bounded fixture");
    std::vector<std::byte> out(static_cast<std::size_t>(size));file.seekg(0);
    file.read(reinterpret_cast<char*>(out.data()),size);check(bool(file),"fixture reads");return out;
}
template<class Container,class T> void put(Container& data,std::size_t at,T value){std::memcpy(data.data()+at,&value,sizeof(value));}
#include "public_event_engagement_capture_cases.h"
int main(int argc,char** argv){
    if(argc!=2){std::fprintf(stderr,"usage: public_event_engagement_tests original-proof-directory\n");return 2;}
    const std::filesystem::path fixtures(argv[1]);
    f::Ticket t{};t.owner={71,{5}};t.boot=8;t.definitionRevision=9;t.selectionRevision=6;t.event=13;
    t.definition=0x80F5E466;t.definitionOffset=0x358;
    t.request.registry=0xC8229B2B;t.request.scope=15;t.request.slot=106;t.request.participantCount=1;
    t.request.participants[0]=0x123456789ABCDEF0;
    check(!f::valid(t),"unknown native identifier format fails closed");
    t.request.identifierEncoding=wire::IdentifierEncoding::integer;check(f::valid(t),"bounded native engagement ticket");
    for(unsigned count:{1U,16U})for(auto mode:{wire::IdentifierEncoding::integer,wire::IdentifierEncoding::byte_array}){
        auto request=t.request;request.participantCount=count;request.identifierEncoding=mode;
        for(unsigned i=1;i<count;++i)request.participants[i]=request.participants[0]+i;
        const auto stem=std::string("engagement-")+std::to_string(count)+(mode==wire::IdentifierEncoding::integer?"-integer":"-bytes");
        const auto expected=read(fixtures/(stem+".body")),decoded=read(fixtures/(stem+".decoded"));
        std::array<std::byte,132> body{};bits::Writer writer(body);
        check(wire::write(writer,request) && writer.bit_count()==wire::body_bits(request),"production body sizing");
        std::size_t used{};check(writer.finish(used),"production body finishes");
        check(used==expected.size() && std::equal(expected.begin(),expected.end(),body.begin()),"exact original-decoded vector");
        check(wire::matches_fields(decoded,request),"original native decoded fields");
        for(std::size_t shortSize=0;shortSize<used;++shortSize){bits::Writer shortWriter(std::span(body).first(shortSize));check(!wire::write(shortWriter,request),"short buffer rejected");}
    }
    auto bad=t.request;bad.participantCount=0;check(!wire::valid(bad),"missing participant rejected");
    bad=t.request;bad.participantCount=17;check(!wire::valid(bad),"participant overflow rejected");
    bad=t.request;bad.participantCount=2;bad.participants[1]=bad.participants[0];check(!wire::valid(bad),"duplicate participant rejected");
    bad=t.request;bad.participants[0]=0;check(!wire::valid(bad),"zero participant rejected");
    bad=t.request;bad.participants[0]=UINT64_MAX;check(!wire::valid(bad),"sentinel participant rejected");
    bad=t.request;bad.scope=64;check(!wire::valid(bad),"ungrantable scope rejected");
    bad=t.request;bad.generation=-1;check(!wire::valid(bad),"negative host generation rejected");
    auto active=t.request;active.participantCount=0;active.participants={};active.identifierEncoding=wire::IdentifierEncoding::unknown;
    active.collection=wire::Collection::activePlayers;check(wire::valid(active) && wire::body_bits(active)==32,"native active collection needs no participant ID encoding");
    {
        const auto expected=read(fixtures/"engagement-active.body"),decoded=read(fixtures/"engagement-active.decoded");
        std::array<std::byte,4> encoded{};bits::Writer writer(encoded);
        check(wire::write(writer,active) && std::equal(expected.begin(),expected.end(),encoded.begin()),"active collection original-decoded wire");
        check(wire::matches_fields(decoded,active),"active collection original decoded fields");
    }
    bad=active;bad.participantCount=1;check(!wire::valid(bad),"automatic route cannot inject participant IDs");
    namespace sense=dawn::middleware::bap::activity_message::native::engagement_sense;
    for(unsigned count:{0U,1U,16U}){
        std::array<std::byte,135> body{};bits::Writer writer(body);
        check(writer.write(1,1) && writer.write(count,5),"sense root and count fixture");
        for(unsigned i=0;i<count;++i)check(writer.write(0x123456789ABCDEF0ULL+i,64),"sense raw identity fixture");
        check(writer.write(32775,16) && writer.write(31,32),"sense committed generation and revision fixture");
        const auto meaningful=writer.bit_count();std::size_t used{};check(writer.finish(used),"sense fixture finishes");
        bits::Reader reader(std::span(body).first(used));sense::Output decoded{};std::size_t width{};
        check(sense::read(reader,decoded,width) && width==meaningful && decoded.root && decoded.participantCount==count
            && decoded.generation==7 && decoded.revision==31,"complete native engagement sense decode");
        for(std::size_t shortSize=0;shortSize<used;++shortSize){bits::Reader truncated(std::span(body).first(shortSize));
            check(!sense::read(truncated,decoded,width),"truncated native engagement sense rejected");}
    }
    {
        using Gate=dawn::server::runtime::activity::public_event::EngagementGate;
        auto ticket=t;ticket.request=active;Gate gate;
        check(gate.begin(ticket) && !gate.ready_for_cue(),"engagement publication alone does not advance");
        check(!gate.begin(ticket),"same event cannot rebind gate");
        const sense::Output participants{4,0,1,true};
        check(gate.observe(ticket,sense::kSchema,participants) && !gate.ready_for_cue(),"native sense waits for exact native apply receipt");
        f::Observation observation{ticket,{1,0x1000},7};
        auto stale=observation;stale.ticket.event++;check(!gate.applied(stale),"another event receipt rejected");
        check(gate.applied(observation) && gate.ready_for_cue(),"actual apply plus native participants allows cue publication");
        check(!gate.applied(observation),"duplicate apply receipt rejected");
        check(!gate.observe(ticket,sense::kSchema,participants),"repeated sense revision rejected");
        auto empty=participants;empty.revision++;empty.participantCount=0;
        check(gate.observe(ticket,sense::kSchema,empty) && !gate.ready_for_cue(),"new empty native list stops pending cue readiness");
        auto wrong=participants;wrong.revision+=2;wrong.generation++;
        check(!gate.observe(ticket,sense::kSchema,wrong),"wrong committed generation rejected");
        wrong=participants;wrong.revision+=2;wrong.root=false;check(!gate.observe(ticket,sense::kSchema,wrong),"rootless sense cannot fabricate participants");
        auto staleTicket=ticket;staleTicket.owner.incarnation.value++;check(!gate.observe(staleTicket,sense::kSchema,participants),"stale incarnation sense rejected");
        staleTicket=ticket;staleTicket.request.scope=14;check(!gate.observe(staleTicket,sense::kSchema,participants),"wrong scope sense rejected");
    }
    struct GenerationCase {std::int16_t first,second;};
    for(const auto pair:std::array<GenerationCase,8>{{{-1,0},{0,0},{0,1},{7,6},{7,7},{7,8},{32766,32767},{32767,0}}}){
        t.request.generation=pair.second;
        const auto stem=std::string("engagement-generation-")+std::to_string(pair.first)+"-"+std::to_string(pair.second);
        auto before=read(fixtures/(stem+".before")),after=read(fixtures/(stem+".after")),incoming=read(fixtures/(stem+".authority"));
        // Original fixtures contain actual callback results. Identity/storage
        // belongs to the private test world, independently of those results.
        for(auto* c:{&before,&after}){put(*c,0,t.definition);put(*c,4,0x808094EFU);put(*c,8,t.definitionOffset);put(*c,0x170,3U);}
        std::array<std::byte,0x70> authority{};put(authority,0,t.request.registry);authority[4]=std::byte{70};
        put(authority,6,t.request.slot);put(authority,0xC,0x808094F1U);put(authority,0x68,t.request.scope);
        f::Capture capture{t,{1,0x1000},1,f::kProducerRva,before,after,incoming,authority,t.request.identifierEncoding,true};
        f::Observation observation{};
        const auto expected=pair.first>pair.second?f::Result::staleGeneration:f::Result::accepted;
        check(f::qualify(t,capture,observation)==expected,"original apply generation result qualified");
        if(expected!=f::Result::accepted)continue;
        check(observation.ticket==t && observation.source==capture.source,"accepted receipt keeps exact ticket/source");
        auto wrong=capture;wrong.ticket.owner.incarnation.value++;check(f::qualify(t,wrong,observation)==f::Result::identity,"stale owner rejected");
        wrong=capture;wrong.ticket.event++;check(f::qualify(t,wrong,observation)==f::Result::identity,"different event rejected");
        wrong=capture;wrong.nativeIdentifierEncoding=wire::IdentifierEncoding::byte_array;check(f::qualify(t,wrong,observation)==f::Result::identity,"wrong native identifier format rejected");
        wrong=capture;wrong.originalForwarded=false;check(f::qualify(t,wrong,observation)==f::Result::identity,"missing original rejected");
        after[0x170]^=std::byte{1};check(f::qualify(t,capture,observation)==f::Result::identity,"recycled authority rejected");after[0x170]^=std::byte{1};
        authority[0x68]=std::byte{14};check(f::qualify(t,capture,observation)==f::Result::identity,"wrong native scope rejected");authority[0x68]=std::byte{15};
        after[0x180]^=std::byte{1};check(f::qualify(t,capture,observation)==f::Result::body,"incomplete native cache copy rejected");after[0x180]^=std::byte{1};
        after[0x254]^=std::byte{1};check(f::qualify(t,capture,observation)==f::Result::localParticipants,"invented queue clearing rejected");after[0x254]^=std::byte{1};
        // Native padding is unconstrained, but the unchanged callback must copy it.
        incoming[1]=after[0x181]=std::byte{0xA5};check(f::qualify(t,capture,observation)==f::Result::accepted,"native-owned padding accepted after full copy");
        incoming[0x50]^=std::byte{1};after[0x1D0]^=std::byte{1};check(f::qualify(t,capture,observation)==f::Result::body,"wrong player rejected despite complete copy");
    }
    capture_cases(fixtures,t);
    std::printf("PASS %u engagement codec and original native receipt checks\n",checks);return 0;
}
