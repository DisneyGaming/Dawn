#include "server/runtime/activity/public_event_incoming_runtime.h"
#include "client/hooks/bootflow/public_event_participant_capture.h"
#include "middleware/encoding/bit_writer.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <vector>
namespace event=dawn::server::runtime::activity::public_event;
namespace f=event::participant_feedback;
namespace bridge=event::participant_bridge;
namespace engagement=event::engagement_feedback;
namespace eb=event::engagement_bridge;
namespace cf=dawn::server::runtime::activity::adventure::cue_feedback;
namespace cb=dawn::server::runtime::activity::adventure::native_bridge;
namespace capture=dawn::client::hooks::bootflow::public_event_participant_capture;
namespace bits=dawn::middleware::encoding::bits;
unsigned checks{};
void check(bool v,const char* m){++checks;if(!v){std::fprintf(stderr,"FAIL %u: %s\n",checks,m);std::exit(1);}}
template<class C,class T>void put(C& c,std::size_t at,T value){std::memcpy(c.data()+at,&value,sizeof(value));}
std::vector<std::byte> read(const std::filesystem::path& p){std::ifstream in(p,std::ios::binary|std::ios::ate);check(bool(in),"fixture opens");const auto n=in.tellg();check(n>0 && n<10000,"fixture bounded");std::vector<std::byte>b(static_cast<std::size_t>(n));in.seekg(0);in.read(reinterpret_cast<char*>(b.data()),n);check(bool(in),"fixture reads");return b;}
void save(const std::filesystem::path& p,std::span<const std::byte>b){std::ofstream out(p,std::ios::binary);out.write(reinterpret_cast<const char*>(b.data()),b.size());check(bool(out),"fixture exported");}
event::IncomingDefinition definition(){
    event::IncomingDefinition d{};
    auto& p=d.participant;p.owner={71,{5}};p.boot=8;p.definitionRevision=9;p.selectionRevision=6;p.event=13;
    p.definition=0x80F5E33D;p.definitionOffset=0x228;p.request={0xC8229B2B,110,15};
    p.request.entity=0x123456789ABCDEF0;p.request.identifierEncoding=f::identifiers::IdentifierEncoding::integer;
    auto& e=d.empty;e.owner=p.owner;e.boot=p.boot;e.definitionRevision=p.definitionRevision;e.selectionRevision=p.selectionRevision;e.event=p.event;
    e.definition=0x80F5E466;e.definitionOffset=0x358;e.request.registry=p.request.registry;e.request.slot=106;e.request.scope=15;e.request.generation=1;e.request.collection=engagement::wire::Collection::none;
    auto& c=d.cue;c.owner=p.owner;c.boot=p.boot;c.definitionRevision=p.definitionRevision;c.selectionRevision=p.selectionRevision;
    c.activity=29;c.definition=0x80F5E337;c.definitionOffset=0xB88;c.table=0x80F5E35B;c.stringBank=0x80F56034;c.title=0x50EFBC4D;c.detail=0x290979DD;
    c.request={p.request.registry,0x00D1C5B9,107,0,15};c.request.readiness={p.request.registry,70,106};c.request.publicEvent={p.request.registry,71,110};
    c.presentation=cf::Presentation::authoredProgress;c.progressLabel=0x6267DC7C;c.admissionScope=15;c.incoming=true;
    return d;
}
void cleanup(const event::IncomingDefinition& d){bridge::release(d.participant.owner);eb::release(d.participant.owner);cb::release(d.participant.owner);}
void apply_participant(const f::Ticket& p){auto b=bridge::lookup(p.definition);check(b.epoch && bridge::submit({b,{p,{1,0x1000},1}}),"native participant receipt submitted");}
void apply_engagement(const engagement::Ticket& e){auto b=eb::lookup(e.definition);check(b.epoch && eb::submit({b,{e,{2,0x1000},2}}),"native engagement receipt submitted");}
void apply_cue(const cf::Ticket& c){auto b=cb::lookup(c.definition);check(b.epoch && cb::submit({b,{c,{3,0x1000},3,0,cf::wire::manager_id(c.request.event)}}),"native incoming manager receipt submitted");}
void runtime_cases(){
    const auto d=definition();const auto& p=d.participant;
    event::EngagementContext c{p.owner,p.boot,p.definitionRevision,p.selectionRevision,p.event,29,15,true,true};
    for(bool participantFirst:{false,true}){
        cleanup(d);event::IncomingRuntime runtime;check(runtime.begin(d) && !runtime.begin(d),"immutable event begins once");
        auto frame=runtime.update(c);check(frame.publishParticipant && frame.publishEngagement && !frame.publishCue && !frame.readyForWorld,"publish prerequisites first, not cue");
        check(runtime.request_join(c),"authorized rally can latch early without early join");
        if(participantFirst)apply_participant(p);else apply_engagement(d.empty);
        frame=runtime.update(c);check(!frame.publishCue && !frame.joined,"one native prerequisite never exposes cue");
        if(participantFirst)apply_engagement(d.empty);else apply_participant(p);
        frame=runtime.update(c);check(frame.publishCue && !frame.readyForWorld && frame.engagement.collection==engagement::wire::Collection::none,"both exact native prerequisites publish incoming");
        const auto oldEmpty=d.empty;const auto consumed=eb::lookup(oldEmpty.definition);check(!consumed.epoch,"empty receipt consumed once");
        apply_cue(d.cue);frame=runtime.update(c);check(frame.readyForWorld && !frame.joined && !frame.joinApplied,"world readiness independent of joining");
        const auto joined=runtime.engagement_ticket();check(joined.request.generation==2 && joined.request.collection==engagement::wire::Collection::activePlayers,"join advances only engagement generation");
        check(!eb::submit({{oldEmpty,1},{oldEmpty,{2,0x1000},4}}),"stale empty-generation receipt cannot satisfy joined");
        apply_engagement(joined);frame=runtime.update(c);check(frame.joinApplied && !frame.joined,"native joined apply waits for participant sense");
        event::engagement_sense::Output sense{1,2,1,true};check(runtime.observe(joined,15,event::engagement_sense::kSchema,sense),"exact current joined sense accepted");
        frame=runtime.update(c);check(frame.joined && frame.cue==d.cue.request,"native ready joins same incoming directive without reinsertion");
        check(!runtime.observe(oldEmpty,15,event::engagement_sense::kSchema,sense),"old engagement ticket rejected");
        auto away=c;away.bubble=14;check(!runtime.update(away).publishCue,"wrong admitted bubble suppresses progression");
        check(runtime.update(c).joined,"same event resumes when eligible");
        auto stale=c;stale.selectionRevision++;check(runtime.update(stale).stale && !runtime.update(c).readyForWorld,"changed lifetime remains stale");
    }
    cleanup(d);event::IncomingRuntime unjoined;check(unjoined.begin(d),"fresh unjoined event");static_cast<void>(unjoined.update(c));apply_participant(p);apply_engagement(d.empty);static_cast<void>(unjoined.update(c));apply_cue(d.cue);
    check(unjoined.update(c).readyForWorld && unjoined.engagement_ticket()==d.empty,"world can start without rally or native combat join");
    for(unsigned i=0;i<6;++i){auto bad=d;if(i==0)bad.participant.request.entity=0;if(i==1)bad.cue.incoming=false;if(i==2)bad.empty.request.generation=0;if(i==3)bad.empty.request.collection=engagement::wire::Collection::activePlayers;if(i==4)bad.cue.request.publicEvent={};if(i==5)bad.empty.event++;event::IncomingRuntime rejected;check(!rejected.begin(bad),"invalid handoff definition fails closed");}
    cleanup(d);
}
void mailbox_cases(){
    const auto d=definition();eb::Mailbox box;auto joined=d.empty;joined.request.collection=engagement::wire::Collection::activePlayers;joined.request.generation++;
    check(box.bind(d.empty) && !box.join(d.empty,joined),"unapplied empty epoch cannot join");const auto b=box.lookup(d.empty.definition);
    check(box.submit({b,{d.empty,{2,0x1000},4}}) && !box.join(d.empty,joined),"undrained receipt blocks renewal");std::array<eb::Event,1> output{};check(box.drain(d.empty,output)==1,"empty receipt drained");
    for(unsigned i=0;i<5;++i){auto bad=joined;if(i==0)bad.owner.incarnation.value++;if(i==1)bad.event++;if(i==2)bad.request.generation++;if(i==3)bad.request.registry++;if(i==4)bad.selectionRevision++;check(!box.join(d.empty,bad),"foreign or skipped join epoch rejected");}
    check(box.join(d.empty,joined) && box.lookup(joined.definition).epoch!=b.epoch,"consumed empty epoch advances exactly once");
    check(!box.join(d.empty,joined) && !box.submit({b,{d.empty,{2,0x1000},4}}),"duplicate or obsolete renewal rejected");
    const f::LocalIdentity local{d.participant.request.entity,d.participant.request.identifierEncoding};
    bridge::publish_local_identity(local,100);const auto sample=bridge::local_identity(100);
    check(sample.sequence && sample.identity==local,"client identity cache exposes typed sample");
    check(!bridge::local_identity(99).sequence && !bridge::local_identity(2101).sequence,"future or stale native identity sample rejected");
    bridge::invalidate_local_identity();check(!bridge::local_identity(100).sequence,"load/reset invalidates identity cache");
    bridge::publish_local_identity({},101);check(!bridge::local_identity(101).sequence,"unknown identity never cached");
}
void capture_cases(const std::filesystem::path& path){
    const auto t=definition().participant;const f::LocalIdentity local{t.request.entity,t.request.identifierEncoding};
    for(unsigned iteration:{1U,2U}){
        const auto stem="apply-"+std::to_string(iteration);auto before=read(path/(stem+".before")),after=read(path/(stem+".after")),incoming=read(path/(stem+".incoming"));
        const auto authored=read(path/"participant-native-definition.bin");auto def=authored;
        for(auto* b:{&before,&after}){put(*b,0,t.definition);put(*b,4,0x80804F4AU);put(*b,8,t.definitionOffset);put(*b,0x20,7U);put(*b,0x170,3U);}
        auto component=before;std::array<std::byte,0x70> authority{};put(authority,0,t.request.registry);authority[4]=std::byte{71};put(authority,6,t.request.slot);put(authority,0xC,0x80804F57U);put(authority,0x68,t.nativeScope);
        std::array<std::byte,16> packet{};put(packet,8,std::uintptr_t{0x80000});std::uintptr_t deny{};capture::Identity self{{1,0x1000},7};
        const auto reader=[&](std::uintptr_t address,std::span<std::byte> output){if(address==deny)return false;const auto region=[&](std::uintptr_t base,std::span<const std::byte> data){if(address<base || address-base>data.size() || output.size()>data.size()-(address-base))return false;std::copy_n(data.begin()+address-base,output.size(),output.begin());return true;};return region(0x30000,component)||region(0x50000,def)||region(0x60000,authority)||region(0x70000,packet)||region(0x80000,incoming);};
        const auto resolver=[&](std::uint32_t handle,std::int64_t offset,std::uintptr_t& output){if(handle==t.definition && offset==t.definitionOffset){output=0x50000;return true;}if(handle==3 && !offset){output=0x60000;return true;}return false;};
        const auto identity=[&](std::uintptr_t address,capture::Identity& output){if(address!=0x30000)return false;output=self;return true;};
        bridge::Mailbox box;check(box.bind(t),"native participant ticket bound");const auto binding=box.lookup(t.definition);capture::Context context{};
        const auto begin=[&](){return capture::begin(binding,0x30000,0x70000,reader,resolver,identity,context);};
        check(begin()==capture::Result::accepted,"exact original native definition and global scope qualify");context.local=local;const auto saved=context;
        for(auto address:{0x30000U,0x50000U,0x60000U,0x70000U,0x80000U}){deny=address;check(begin()!=capture::Result::accepted,"inaccessible prerequisite rejects");deny=0;}
        def[0x38]=std::byte{15};check(begin()==capture::Result::definition,"bubble admission cannot replace authored global scope");def=authored;
        context=saved;component=after;f::Observation observation{};auto observedLocal=local;
        const auto finish=[&](){return capture::finish(context,0x30000,11,reader,resolver,identity,observedLocal,observation);};
        check(finish(),"unchanged BF5AA0 full result and local match accepted");
        observedLocal.entity++;check(!finish(),"changed native local player rejects");observedLocal=local;
        observedLocal.encoding=f::identifiers::IdentifierEncoding::byte_array;check(!finish(),"changed native wire format rejects");observedLocal=local;
        component[0x199]=std::byte{};check(!finish(),"no original local match rejects");component=after;
        self.source.member++;check(!finish(),"source reuse rejects");self.source.member--;
        authority[0x20]^=std::byte{1};check(!finish(),"authority lifecycle changes reject");authority[0x20]^=std::byte{1};
        incoming[0]^=std::byte{1};check(!finish(),"incoming packet changes reject");incoming[0]^=std::byte{1};
        component[0x180]^=std::byte{1};check(!finish(),"partial body copy rejects");component=after;
        for(auto address:{0x30000U,0x30170U,0x50000U,0x60000U,0x70000U,0x80000U}){deny=address;check(!finish(),"unreadable post apply rejects");deny=0;}
        check(finish() && box.submit({binding,observation}) && !box.submit({binding,observation}),"qualified callback delivered once");std::array<bridge::Event,1> output{};check(box.drain(t,output)==1,"owner drains exact participant receipt");box.release(t.owner);check(!box.submit({binding,observation}),"retired owner rejects late receipt");
    }
}
int main(int argc,char** argv){
    if(argc!=2)return 2;const std::filesystem::path path(argv[1]);const auto d=definition();
    std::array<std::byte,4> bytes{};bits::Writer writer(bytes);check(engagement::wire::write(writer,d.empty.request) && writer.bit_count()==32,"explicit empty type70 codec");save(path/"engagement-none.body",bytes);save(path/"engagement-none.expected",engagement::wire::decoded(d.empty.request));
    runtime_cases();mailbox_cases();
    if(std::filesystem::exists(path/"participant-native-definition.bin"))capture_cases(path);
    if(std::filesystem::exists(path/"entry-0-0.native"))for(unsigned row=0;row<8;++row)for(unsigned active=0;active<2;++active){auto t=d.cue;const auto entry=read(path/("entry-"+std::to_string(row)+"-"+std::to_string(active)+".native"));t.detail=cf::field<std::uint32_t>(entry,0x5C);t.progressLabel=cf::field<std::uint32_t>(entry,0x64);t.progressFormat=static_cast<cf::ProgressFormat>(cf::field<std::uint8_t>(entry,0x74));t.incoming=!active;check(cf::valid(t) && cf::presentation_matches(t,entry,active),"all package-authored counter/no-counter native presentations accepted");auto bad=t;bad.progressFormat=t.progressFormat==cf::ProgressFormat::none?cf::ProgressFormat::percent:cf::ProgressFormat::none;check(!cf::valid(bad),"absent-label format contract cannot be mixed");}
    std::printf("PASS %u incoming/participant/epoch/original receipt checks\n",checks);
}
