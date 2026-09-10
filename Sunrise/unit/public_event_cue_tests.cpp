#include "server/runtime/activity/adventure_native_bridge.h"
#include "middleware/bap/activity_message/sensor_auth_update.h"
#include "middleware/encoding/bit_writer.h"
#include "server/runtime/activity/mercury_public_event_opening.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <vector>

namespace bridge=sunrise::server::runtime::activity::adventure::native_bridge;
namespace f=bridge::feedback;
namespace cue=f::wire;
namespace wire=sunrise::middleware::bap::activity_message::sensor_auth_update;
namespace bits=sunrise::middleware::encoding::bits;
unsigned checks{};
void check(bool value,const char* what){++checks;if(!value){std::fprintf(stderr,"FAIL %u: %s\n",checks,what);std::exit(1);}}
std::vector<std::byte> read(const std::filesystem::path& path){
    std::ifstream file(path,std::ios::binary|std::ios::ate);check(bool(file),"fixture opens");
    const auto n=file.tellg();check(n>0 && n<10000,"bounded fixture");
    std::vector<std::byte> data(static_cast<std::size_t>(n));file.seekg(0);
    file.read(reinterpret_cast<char*>(data.data()),n);check(bool(file),"fixture reads");return data;
}
std::array<std::byte,601> encode(cue::Request request){
    std::array<std::byte,601> body{};bits::Writer writer(body);
    check(cue::write(writer,request) && writer.bit_count()==cue::kBits,"production cue codec");return body;
}
int main(int argc,char** argv){
    if(argc!=4){std::fprintf(stderr,"usage: public_event_cue_tests adventure-fixtures crossroads-original-fixtures export-directory\n");return 2;}
    const std::filesystem::path fixtures(argv[1]),nativeFixtures(argv[2]),output(argv[3]);std::filesystem::create_directories(output);
    const auto definition=read(fixtures/"80F46D67.definition.bin"),entry=read(fixtures/"80F46D67.manager-entry.bin");
    const auto original=read(fixtures/"runner-opening.body");
    f::Ticket ticket{};ticket.owner={51,{19}};ticket.boot=7;ticket.definitionRevision=9;ticket.selectionRevision=3;
    ticket.activity=1076;ticket.definition=0x80F46D67;ticket.definitionOffset=f::field<std::int64_t>(definition,0x78);
    ticket.request={f::field<std::uint32_t>(definition,0xBB8),0xC9E4C596,0,0};
    ticket.table=f::field<std::uint32_t>(definition,0xBE4);ticket.stringBank=f::field<std::uint32_t>(entry,0x58);
    ticket.title=0x099C20CB;ticket.detail=0x9661E8EB;
    check(f::valid(ticket) && ticket.request.scope==UINT32_MAX,"existing aggregate defaults to top-level scope");
    const auto globalBody=encode(ticket.request);
    check(original.size()==globalBody.size() && std::equal(original.begin(),original.end(),globalBody.begin()),
        "existing Adventure bytes match original-code fixture exactly");
    const std::array<std::uint8_t,1> types{68},flags{2};const std::array<std::uint16_t,1> slots{0};
    wire::Roster roster{};roster.groupCount=roster.topLevelGroupCount=1;
    roster.groups[0]={ticket.request.registry,types,flags,slots};cue::Batch batch{};batch.count=1;batch.entries[0]=ticket.request;
    check(cue::valid(batch,roster),"existing top-level admission");
    const std::array<std::uint32_t,2> keys{ticket.request.registry,ticket.request.registry};
    std::array<wire::BubbleSubBlock,2> blocks{{{15,std::span(keys).first(1)},{15,std::span(keys).first(1)}}};
    roster.bubbleSubBlocks=std::span(blocks).first(1);
    check(!cue::valid(batch,roster),"global request cannot also be bubble admitted");
    roster.bubbleSubBlocks={};roster.topLevelGroupCount=0;
    check(!cue::valid(batch,roster),"global request cannot use local descriptor");
    batch.entries[0].scope=15;
    check(!cue::valid(batch,roster),"local request requires bubble admission");
    roster.bubbleSubBlocks=std::span(blocks).first(1);
    check(cue::valid(batch,roster),"exact local descriptor plus authored bubble admission");
    check(encode(batch.entries[0])==globalBody,"scope changes admission only, not one wire bit");
    roster.topLevelGroupCount=1;check(!cue::valid(batch,roster),"local request rejects top-level descriptor");roster.topLevelGroupCount=0;
    blocks[0].bubble=14;check(!cue::valid(batch,roster),"wrong native bubble rejected");blocks[0].bubble=15;
    roster.bubbleSubBlocks=blocks;check(!cue::valid(batch,roster),"duplicate bubble admission rejected");
    blocks[1].bubble=14;check(!cue::valid(batch,roster),"matching and foreign admission rejected");
    roster.bubbleSubBlocks=std::span(blocks).first(1);blocks[0].keys=keys;
    check(!cue::valid(batch,roster),"duplicate key in same bubble rejected");blocks[0].keys=std::span(keys).first(1);
    std::array<std::uint8_t,2> presence{0,1};blocks[0].presence=std::span(presence).first(1);
    check(!cue::valid(batch,roster),"removed registry is not admitted");presence[0]=1;
    check(cue::valid(batch,roster),"explicit present registry admitted");blocks[0].presence=presence;
    check(!cue::valid(batch,roster),"malformed presence mask rejected");blocks[0].presence={};
    auto duplicate=roster;duplicate.groups[1]=duplicate.groups[0];duplicate.groupCount=2;
    check(!cue::valid(batch,duplicate),"duplicate native descriptor group rejected");
    auto duplicateBatch=batch;duplicateBatch.count=2;duplicateBatch.entries[1]=duplicateBatch.entries[0];
    check(!cue::valid(duplicateBatch,roster),"duplicate requested slot rejected");
    for(const auto invalid:{64U,120U,0xFFFFFFFEU}){auto request=batch.entries[0];request.scope=invalid;
        auto writer=bits::Writer::measuring();check(!cue::valid(request) && !cue::write(writer,request) && writer.bit_count()==0,"invalid scope emits nothing");}
    auto expected=cue::decoded(ticket.request),prior=expected;prior[0x10]^=std::byte{1};
    f::Capture capture{ticket,{0x1234,0x180},1,f::kProducerRva,expected,prior,expected,entry,0,1,true,true,true};
    f::Observation observation{};check(f::qualify(ticket,capture,observation)==f::Result::accepted,"existing Adventure native entry still qualifies");
    auto scoped=ticket;scoped.request.scope=scoped.nativeScope=scoped.admissionScope=15;capture.ticket=scoped;
    check(f::qualify(scoped,capture,observation)==f::Result::accepted,"scope identity contract accepts exact ticket");
    // The preceding test reuses the Adventure entry to isolate scope identity.
    // It does not claim a Crossroads native manager insertion; the original-code proof is separate.
    for(const auto scope:{UINT32_MAX,14U,16U,64U}){auto bad=scoped;bad.nativeScope=scope;
        capture.ticket=bad;
        check(f::qualify(scoped,capture,observation)==f::Result::identity,"wrong scope receipt rejected");}
    auto badAdmission=scoped;badAdmission.admissionScope=14;
    check(!f::valid(badAdmission),"ticket registry admission scope must match request scope");
    capture.ticket=scoped;capture.ticket.request.scope=capture.ticket.nativeScope=14;
    check(f::qualify(scoped,capture,observation)==f::Result::identity,"internally valid foreign-scope ticket rejected");
    for(unsigned changed=0;changed<5;++changed){capture.ticket=scoped;
        if(changed==0)++capture.ticket.boot;if(changed==1)++capture.ticket.definitionRevision;
        if(changed==2)++capture.ticket.selectionRevision;if(changed==3)++capture.ticket.owner.sessionId;
        if(changed==4)++capture.ticket.activity;
        check(f::qualify(scoped,capture,observation)==f::Result::identity,"stale ownership or selected activity rejected");}
    const cue::Request crossroads{0xC8229B2B,0x00D1C5B9,107,0,15};
    namespace activity=sunrise::server::runtime::activity;
    namespace mercury=activity::mercury::public_events;
    namespace native=sunrise::state::activity::native_population;
    namespace coo=sunrise::state::activity::coo;
    const auto nativeEntry=read(nativeFixtures/"80F5E337.manager-entry.bin");
    const auto nativeDecoded=read(nativeFixtures/"crossroads-opening.decoded.bin");
    auto crossTicket=mercury::kOpeningCue;crossTicket.owner=ticket.owner;crossTicket.boot=ticket.boot;
    crossTicket.definitionRevision=9;crossTicket.selectionRevision=3;
    check(f::valid(crossTicket),"exact Crossroads authored presentation ticket");
    const auto crossBody=cue::decoded(crossTicket.request);
    check(std::equal(nativeDecoded.begin(),nativeDecoded.end(),crossBody.begin(),crossBody.end()),"Crossroads full decoded image equals original native result");
    auto crossPrior=crossBody;crossPrior[0x10]^=std::byte{1};
    f::Capture nativeCapture{crossTicket,{0x1234,0x180},1,f::kProducerRva,crossBody,crossPrior,crossBody,nativeEntry,0,1,true,true,true};
    check(f::qualify(crossTicket,nativeCapture,observation)==f::Result::managerEntry,"original Crossroads unready insertion cannot acknowledge presentation readiness");
    // Isolate expected READY-layout rejection checks with modeled manager state.
    // The actual original-code fixture above is unready; no native target or
    // participant membership has been recovered/claimed by this unit test.
    auto modeledReadyEntry=nativeEntry;modeledReadyEntry[0x70]=std::byte{1};nativeCapture.entry=modeledReadyEntry;
    check(f::qualify(crossTicket,nativeCapture,observation)==f::Result::accepted,"modeled ready layout exercises exact qualifier fields");
    for(const auto offset:{0U,4U,0x10U,0x18U,0x1CU,0x20U,0x24U,0x28U,0x58U,0x5CU,0x60U,0x64U,0x70U,0x74U,0x110U,0x124U,0x140U,0x141U,0x2CU}){
        auto altered=modeledReadyEntry;altered[offset]^=offset==0x2C?std::byte{4}:std::byte{1};auto invalidCapture=nativeCapture;invalidCapture.entry=altered;
        check(f::qualify(crossTicket,invalidCapture,observation)==f::Result::managerEntry,"each authored Crossroads presentation field is exact");}
    auto noManager=nativeCapture;noManager.managerReadyBefore=false;
    check(f::qualify(crossTicket,noManager,observation)==f::Result::managerUnavailable,"Crossroads cache adoption alone cannot acknowledge UI");
    auto incorrectStyle=crossTicket;incorrectStyle.presentation=f::Presentation::activityObjective;incorrectStyle.progressLabel=cue::kAbsent;
    auto incorrectCapture=nativeCapture;incorrectCapture.ticket=incorrectStyle;
    check(f::qualify(incorrectStyle,incorrectCapture,observation)==f::Result::managerEntry,"Crossroads cannot pass the default Adventure layout");
    check(activity::public_event::OpeningRuntime::valid(mercury::kOpeningDefinition),"authored cue plus source44/46 rules174/239 graph validates");
    activity::population::Service population;check(population.begin(ticket.owner,mercury::kOpeningSources,ticket.boot),"real native population service retains Crossroads capabilities");
    activity::public_event::OpeningRuntime opening;
    check(opening.begin(mercury::kOpeningDefinition,ticket.owner,ticket.boot,9,3,41,population),"independent event run begins with world owner");
    check(!opening.update(15,false,true,29,3).requested && population.project(15).count==0,"world loading emits no startup or population");
    check(!opening.update(15,true,false,29,3).requested,"missing exact roster admission cannot start");
    check(!opening.update(14,true,true,29,3).requested,"wrong region cannot start");
    auto frame=opening.update(15,true,true,29,3);
    check(frame.requested && !frame.nativeReady && population.project(15).count==0,"opening cue precedes all source requests");
    auto openingBinding=bridge::lookup(opening.ticket().definition);
    check(openingBinding.epoch && openingBinding.ticket==crossTicket,"immutable native ticket bound before publication");
    check(f::qualify(crossTicket,nativeCapture,observation)==f::Result::accepted && bridge::submit({openingBinding,observation}),"modeled qualified receipt tests synchronized production bridge and UE gate");
    frame=opening.update(15,true,true,29,3);
    check(frame.nativeReady && frame.combatRequested && !frame.combatReady,"native cue receipt unlocks finite authored requests");
    const auto sources=population.project(15);check(sources.count==2,"two authored sources projected");
    for(std::size_t i=0;i<sources.count;++i){
        check(sources.entries[i].slot==mercury::kOpeningSources[i].slot && sources.entries[i].source.ruleSlot==mercury::kOpeningSources[i].rule,
            "source slot and exact native rule retained");
    }
    std::array<coo::NativePopulationLedger<16>,2> ledgers;
    std::array<native::Event,2> nativeEvents{};
    for(std::size_t i=0;i<2;++i){
        const coo::PopulationOwner owner{ticket.owner.sessionId,ticket.boot,ticket.owner.incarnation.value,mercury::kOpeningSourceCommands[i].asset,19};
        check(ledgers[i].begin(owner),"central ledger begins exact native source generation");
        nativeEvents[i]={{ticket.owner,owner,15},{owner,static_cast<std::uint32_t>(0x7001+i),static_cast<std::uint32_t>(0xA001+i)},static_cast<std::uint32_t>(0x6001+i),native::Kind::admitted};
        auto falseDeath=nativeEvents[i];falseDeath.kind=native::Kind::died;
        check(!opening.observe_accepted(falseDeath,ledgers[i].died(falseDeath.actor)),"unadmitted native death does not advance opening");
        auto staleEvent=nativeEvents[i];++staleEvent.lease.source.generation;staleEvent.actor.owner=staleEvent.lease.source;
        check(!opening.observe_accepted(staleEvent,coo::PopulationIntake::accepted),"stale source generation cannot advance opening");
    }
    check(opening.observe_accepted(nativeEvents[0],ledgers[0].admitted(nativeEvents[0].actor)),"first exact native source admission accepted");
    frame=opening.update(15,true,true,29,3);check(!frame.combatReady,"one side does not acknowledge both sources");
    check(!opening.observe_accepted(nativeEvents[0],ledgers[0].admitted(nativeEvents[0].actor)),"duplicate actor receipt cannot advance twice");
    check(opening.observe_accepted(nativeEvents[1],ledgers[1].admitted(nativeEvents[1].actor)),"second exact source admission accepted");
    frame=opening.update(15,true,true,29,3);check(frame.combatReady && !frame.failed,"UE opening advances only after both actual admissions");
    auto realDeath=nativeEvents[0];realDeath.kind=native::Kind::died;
    check(!opening.observe_accepted(realDeath,ledgers[0].died(realDeath.actor)),"one real death does not complete the unimplemented percentage objective");
    const auto revision=population.revision();for(unsigned i=0;i<12;++i)frame=opening.update(15,true,true,29,3);
    check(population.revision()==revision && population.project(15).count==2,"no repeated request or invented source renewal");
    check(opening.update(15,true,true,1078,4).conflictingSelection,"committed selection change cannot replace retained event lifetime");
    bridge::release(ticket.owner);
    bridge::Mailbox mailbox;auto other=ticket;other.definition=0x80F5E337;other.request=crossroads;other.admissionScope=15;
    check(mailbox.bind(ticket) && mailbox.bind(other),"two definitions share one world owner");
    const auto event=[&](const f::Ticket& t,std::uint64_t sequence){const auto binding=mailbox.lookup(t.definition);
        return bridge::Event{binding,{t,{0x1234,0x180},sequence,0,cue::manager_id(t.request.event)}};};
    const auto adventureEvent=event(ticket,1),publicEvent=event(other,2);
    check(mailbox.submit(publicEvent) && mailbox.submit(adventureEvent),"interleaved feature receipts queued");
    std::array<bridge::Event,4> drained{};auto stale=ticket;++stale.selectionRevision;
    check(mailbox.drain(stale,drained)==0 && mailbox.drain(other,std::span<bridge::Event>{})==0,"stale ticket or empty output consumes nothing");
    check(mailbox.drain(ticket,drained)==1 && drained[0].binding.ticket==ticket,"Adventure drains only its receipt despite public receipt first");
    check(mailbox.drain(ticket,drained)==0,"same ticket cannot redrain");
    check(mailbox.drain(other,drained)==1 && drained[0].binding.ticket==other,"public receipt remains intact after Adventure drain");
    mailbox.release(ticket.owner);check(mailbox.bind(ticket) && mailbox.bind(other),"world retirement permits new binding epochs");
    check(!mailbox.submit(adventureEvent) && !mailbox.submit(publicEvent),"retired epochs cannot submit to reused owner");
    check(mailbox.submit(event(ticket,3)) && mailbox.submit(event(other,4)),"reverse feature order accepted");
    check(mailbox.drain(other,drained)==1 && drained[0].observation.sequence==4,"public consumer does not consume earlier Adventure receipt");
    check(mailbox.drain(ticket,drained)==1 && drained[0].observation.sequence==3,"earlier Adventure receipt retains its identity and sequence");
    mailbox.release(ticket.owner);check(mailbox.bind(ticket) && mailbox.bind(other),"third world lease");
    check(mailbox.submit(event(ticket,5)) && mailbox.submit(event(other,6)),"both receipts pending at world retirement");
    auto foreign=ticket.owner;++foreign.sessionId;mailbox.release(foreign);
    check(mailbox.drain(foreign,drained)==0,"foreign owner cannot clear or consume queues");
    mailbox.release(ticket.owner);
    check(mailbox.drain(ticket,drained)==0 && mailbox.drain(other,drained)==0,"actual world retirement clears both feature receipts");
    check(!mailbox.lookup(ticket.definition).epoch && !mailbox.lookup(other.definition).epoch,"actual retirement clears both bindings");
    const auto body=encode(crossroads);const auto decoded=cue::decoded(crossroads);
    for(const auto& name:{"crossroads-opening.body","crossroads-opening.expected.bin"}){
        std::ofstream file(output/name,std::ios::binary);
        const auto bytes=std::string_view(name).ends_with(".body")?std::span<const std::byte>(body):std::span<const std::byte>(decoded);
        file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());check(bool(file),"Crossroads proof fixture exported");}
    std::printf("PASS %u scoped native cue and existing Adventure regression checks\n",checks);
}
