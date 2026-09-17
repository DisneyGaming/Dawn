#include "server/runtime/activity/persistent_activity.h"
#include "server/runtime/activity/mercury_definition.h"
#include "client/hooks/bootflow/adventure_dialogue_observer.h"
#include "middleware/bap/activity_message/sensor_auth_update.h"
#include "core/logging/log.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <vector>
namespace dawn::core::log {void write(Channel,Level,std::string_view) noexcept {}}
namespace activity=dawn::server::runtime::activity;
namespace adventure=activity::adventure;
namespace bridge=adventure::dialogue_bridge;
namespace feedback=adventure::dialogue_feedback;
namespace voice=feedback::wire;
namespace wire=dawn::middleware::bap::activity_message::sensor_auth_update;
namespace bits=dawn::middleware::encoding::bits;
namespace coo=dawn::state::activity::coo;
namespace observer=dawn::client::hooks::bootflow::adventure_dialogue_observer;
unsigned checks{};
void check(bool ok,const char* message){++checks;if(!ok){std::fprintf(stderr,"FAIL %u: %s\n",checks,message);std::exit(1);}}
std::vector<std::byte> read(const std::filesystem::path& path) {
    std::ifstream in(path,std::ios::binary|std::ios::ate);check(bool(in),"fixture opens");const auto n=in.tellg();
    check(n>0 && n<20000,"bounded complete fixture");std::vector<std::byte> result(static_cast<std::size_t>(n));
    in.seekg(0);in.read(reinterpret_cast<char*>(result.data()),n);check(bool(in),"fixture read");return result;
}
void save(const std::filesystem::path& path,std::span<const std::byte> data) {
    std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(data.data()),data.size());check(bool(out),"production body exported");
}
int main(int argc,char** argv) {
    if(argc!=4){std::fprintf(stderr,"usage: adventure_dialogue_tests retail-evidence export-directory mercury-json\n");return 2;}
    const std::filesystem::path evidence(argv[1]),output(argv[2]);std::filesystem::create_directories(output);
    voice::Request request{0x08551BCF,2,21,0};request.generations[0]=1;
    for(const auto row:{0U,20U,127U})for(const auto generation:{1U,0x7FFFFFFFU,0x80000000U,UINT32_MAX})for(const bool retired:{false,true}) {
        auto r=request;r.bankRows=128;r.activeRow=retired?voice::kNoRow:static_cast<std::uint8_t>(row);r.generations={};r.generations[row]=generation;
        char name[80]{};std::snprintf(name,sizeof(name),"row%u-gen%08X%s",row,generation,retired?"-retired":"");
        const std::string stem(name);const auto expectedWire=read(evidence/"dialogue-wire"/(stem+".body"));
        const auto original=read(evidence/"dialogue-wire"/(stem+".decoded.bin"));
        const auto mask=read(evidence/"dialogue-wire"/(stem+".written-mask.bin"));
        const auto expected=voice::decoded(r);check(original.size()==expected.size() && std::equal(expected.begin(),expected.end(),original.begin()),"full original 4104-byte decoder image");
        check(mask.size()==voice::kDecodedBytes,"full native write mask");
        for(std::size_t i=0;i<mask.size();++i)check(voice::decoder_writes(i,r)==(mask[i]==std::byte{255}),"native optional-field write coverage");
        check(voice::matches_fields(original,r),"actual applied semantic fields");
        auto padded=original;for(std::size_t i=0;i<padded.size();++i)if(!voice::state_field(i))padded[i]=std::byte{0xAC};
        check(voice::matches_fields(padded,r),"untouched padding is legal");
        std::array<std::byte,2479> encoded{};bits::Writer writer(encoded);
        check(voice::write(writer,r) && writer.bit_count()==voice::body_bits(r),"production codec width");
        const auto body=std::span(encoded).first((writer.bit_count()+7)/8);
        check(expectedWire.size()==body.size() && std::equal(body.begin(),body.end(),expectedWire.begin()),"complete wire matches independently original-decoded fixture");
        save(output/(stem+".body"),body);
        wire::Snapshot snapshot{};snapshot.dialogues.entries[0]=r;snapshot.dialogues.count=1;
        for(const bool legacy:{false,true}) {
            std::array<std::byte,2479> routed{};bits::Writer route(routed);
            const auto count=legacy?wire::legacy_auth_body_bits(snapshot,r.registry,53,2,false):wire::auth_body_bits(snapshot,r.registry,53,2,false);
            const bool ok=legacy?wire::legacy_write_auth_body(route,snapshot,r.registry,53,2,false):wire::write_auth_body(route,snapshot,r.registry,53,2,false);
            check(ok && count==writer.bit_count() && route.bit_count()==count && routed==encoded,"both actual authority body routers preserve generic wire");
        }
    }
    auto reject=[&](auto r){auto writer=bits::Writer::measuring();check(!voice::valid(r) && !voice::write(writer,r) && !writer.bit_count(),"bad request cannot publish");};
    for(const auto key:{0U,UINT32_MAX,voice::kAbsent}){auto r=request;r.registry=key;reject(r);}
    {auto r=request;r.slot=32768;reject(r);}{auto r=request;r.bankRows=0;reject(r);}{auto r=request;r.bankRows=129;reject(r);}
    {auto r=request;r.activeRow=21;reject(r);}{auto r=request;r.generations[0]=0;reject(r);}{auto r=request;r.generations[127]=2;reject(r);}
    std::array<std::uint8_t,1> types{53},flags{2};std::array<std::uint16_t,1> slots{2};
    wire::Snapshot snapshot{};snapshot.dialogues.entries[0]=request;snapshot.dialogues.count=1;
    snapshot.roster.groups[0]={request.registry,types,flags,slots};snapshot.roster.groupCount=1;snapshot.roster.topLevelGroupCount=1;
    check(voice::valid(snapshot.dialogues,snapshot.roster),"one exact authored top-level authority slot");
    snapshot.roster.topLevelGroupCount=0;check(!voice::valid(snapshot.dialogues,snapshot.roster),"local group cannot impersonate root");snapshot.roster.topLevelGroupCount=1;
    snapshot.roster.groups[1]=snapshot.roster.groups[0];snapshot.roster.groupCount=2;snapshot.roster.topLevelGroupCount=2;
    check(!voice::valid(snapshot.dialogues,snapshot.roster),"duplicate root is ambiguous");snapshot.roster.groupCount=snapshot.roster.topLevelGroupCount=1;
    flags[0]=1;check(!voice::valid(snapshot.dialogues,snapshot.roster),"sense-only cannot receive authority");flags[0]=2;
    snapshot.dialogues.entries[1]=request;snapshot.dialogues.count=2;check(!voice::valid(snapshot.dialogues,snapshot.roster),"duplicate request rejected");
    std::array<std::byte,16384> packet{};std::size_t written=999;
    check(!wire::encode_sensor_auth_update(snapshot,packet,written) && !written,"full encoder rejects malformed batch");snapshot.dialogues.count=1;snapshot.archiveOmega=true;
    check(!wire::encode_sensor_auth_update(snapshot,packet,written),"new dialogue batch cannot enter Omega archive");
    namespace placement=dawn::middleware::bap::activity_message::native::placement;
    namespace interaction=dawn::middleware::bap::activity_message::native::interaction;
    for(const auto generation:{0U,1U,0x7FFFFFFFU})for(const auto mode:{interaction::Mode::unchanged,interaction::Mode::disabled,interaction::Mode::enabled}) {
        wire::Snapshot placed{};placed.placements.entries[0]={0xF25B938B,6,15,mode,generation};placed.placements.count=1;
        std::array<std::byte,48> expected{};bits::Writer expectedWriter(expected);
        check(placement::write(expectedWriter,placed.placements.entries[0]),"existing placement authority remains valid");
        for(const bool legacy:{false,true}) {
            std::array<std::byte,48> actual{};bits::Writer routed(actual);
            const bool ok=legacy?wire::legacy_write_auth_body(routed,placed,0xF25B938B,4,6,false):wire::write_auth_body(routed,placed,0xF25B938B,4,6,false);
            check(ok && routed.bit_count()==expectedWriter.bit_count() && actual==expected,"both writers retain positive placement generation and interaction request");
        }
    }
    feedback::Ticket ticket{};ticket.owner={71,{4}};ticket.boot=2;ticket.definitionRevision=3;ticket.selectionRevision=4;ticket.activity=1076;
    ticket.authored=adventure::mercury::kOpenings[0].dialogue;ticket.request=request;check(feedback::valid(ticket),"authored dialogue ticket");
    const auto decoded=voice::decoded(request);feedback::Capture capture{ticket,{0xAABB0012,0x180},1,decoded,decoded,0,1,true,true};feedback::Observation observed{};
    check(feedback::qualify(ticket,capture,observed)==feedback::Result::accepted,"original processed-generation transition qualifies submission");
    for(std::size_t i=0;i<decoded.size();++i)if(voice::state_field(i)) {
        auto mutated=decoded;mutated[i]^=std::byte{1};auto c=capture;c.before=mutated;c.after=mutated;
        check(feedback::qualify(ticket,c,observed)==feedback::Result::body,"all semantic fields qualified including inactive optional time");
    }
    auto bad=capture;bad.processedBefore=1;check(feedback::qualify(ticket,bad,observed)==feedback::Result::unchanged,"current processed state is not new submission");
    bad=capture;bad.processedAfter=2;check(feedback::qualify(ticket,bad,observed)==feedback::Result::notSubmitted,"wrong processed generation");
    bad=capture;bad.ticket.boot++;check(feedback::qualify(ticket,bad,observed)==feedback::Result::identity,"stale boot");
    bad=capture;bad.ticket.selectionRevision++;check(feedback::qualify(ticket,bad,observed)==feedback::Result::identity,"stale selection");
    bad=capture;bad.originalForwarded=false;check(feedback::qualify(ticket,bad,observed)==feedback::Result::identity,"forwarding required");
    bad=capture;bad.identityStable=false;check(feedback::qualify(ticket,bad,observed)==feedback::Result::identity,"stable native component required");
    check(feedback::qualify(ticket,capture,observed)==feedback::Result::accepted,"restore exact receipt");
    bridge::Mailbox mailbox;check(mailbox.bind(ticket),"arm before packet");const auto binding=mailbox.lookup(ticket.authored.definition);
    check(mailbox.bind(ticket),"refresh does not reset ticket");auto foreign=ticket;foreign.owner.sessionId++;
    check(!mailbox.bind(foreign),"same native definition cannot bind different owner");
    const bridge::Event event{binding,observed};check(mailbox.submit(event) && !mailbox.submit(event),"receipt accepted exactly once");
    std::array<bridge::Event,2> drained{};check(!mailbox.drain(foreign,drained),"other owner cannot steal receipt");
    auto second=ticket;second.authored=adventure::mercury::kOpenings[1].dialogue;second.request.registry=0x9104CE05;second.request.bankRows=14;
    check(mailbox.bind(second),"same-owner different definition can coexist");const auto secondBinding=mailbox.lookup(second.authored.definition);
    auto secondObservation=observed;secondObservation.ticket=second;check(mailbox.submit({secondBinding,secondObservation}),"independent native submission");
    check(mailbox.drain(second,drained)==1 && drained[0].binding.ticket==second,"reverse exact-ticket consumption");
    check(mailbox.drain(ticket,drained)==1 && drained[0].binding.ticket==ticket,"first receipt retained");
    mailbox.release(ticket.owner);check(mailbox.bind(ticket) && !mailbox.submit(event),"retired epoch cannot submit into new owner lease");
    check(!observer::begin(nullptr).binding.epoch && !observer::finish(nullptr,{},false),"read-only observer null guards");
    std::string error;std::shared_ptr<const coo::script::MissionDocument> document=coo::script::MissionDocument::read_native_policy(argv[3],activity::mercury::kProfile,error);
    if(!document)std::fprintf(stderr,"%s\n",error.c_str());check(bool(document),"actual assembled Mercury document parses");
    check(activity::PersistentActivity::valid(activity::mercury::kActivity,*document),"actual native profile accepts objective plus conversation");
    const auto raw=read(evidence.parent_path()/"live-r2-31956"/"request-114750-target-1078.bin");
    activity::adventure_start::wire::Request selected{};check(activity::adventure_start::wire::parse(raw,selected),"actual complete native start request");
    for(const auto& opening:adventure::mercury::kOpenings) {
        selected.selection.activityIndex=selected.selection.sourceActivityIndex=opening.activity;
        const feedback::Owner owner{static_cast<std::uint64_t>(opening.activity),{31}};activity::PersistentActivity persistent;
        check(persistent.begin(owner,activity::mercury::kActivity,document,97),"owner begins once");
        check(!persistent.update(15,false,selected).opening.dialogueRequested,"load cannot submit conversation");
        check(!persistent.update(15,true,selected,false).opening.dialogueRequested,"roster warmup cannot consume conversation");
        auto frame=persistent.update(15,true,selected);const auto voiceTicket=persistent.opening().dialogue_ticket();
        check(frame.opening.requested && frame.opening.dialogueRequested && !frame.opening.dialogueSubmitted,"objective and native dialogue request publish together");
        check(frame.placements.count==9 && frame.populations.count==2,"ambient and portals preserved");
        auto armed=bridge::lookup(opening.dialogue.definition);check(armed.epoch && armed.ticket==voiceTicket,"per-owner actual bank ticket armed before authority publication");
        const auto expected=voice::decoded(voiceTicket.request);feedback::Capture native{voiceTicket,{0xBBDD0001,0x180},7,expected,expected,0,voiceTicket.request.generations[0],true,true};
        check(feedback::qualify(voiceTicket,native,observed)==feedback::Result::accepted && bridge::submit({armed,observed}),"qualified native mode2 submission reaches server");
        frame=persistent.update(15,true,selected);
        check(frame.opening.dialogueSubmitted && frame.opening.dialogue.activeRow==voice::kNoRow && frame.opening.dialogue.generations[0]==voiceTicket.request.generations[0],"submission retires optional trigger and retains history");
        check(!frame.opening.gatewayRequested,"dialogue submission cannot bypass missing objective callback");
        namespace cues=adventure::cue_feedback;
        const auto cueTicket=persistent.opening().ticket();const auto cueBinding=adventure::native_bridge::lookup(cueTicket.definition);
        std::array<char,64> cueFilename{};std::snprintf(cueFilename.data(),cueFilename.size(),"%08X.manager-entry.bin",cueTicket.definition);
        const auto cueEntry=read(evidence.parent_path()/"cue-inventory"/cueFilename.data());
        const auto cueBody=cues::wire::decoded(cueTicket.request);auto cuePrior=cueBody;cuePrior[0x10]^=std::byte{1};
        cues::Capture cueCapture{cueTicket,{0xCCDD0001,0},8,cues::kProducerRva,cueBody,cuePrior,cueBody,cueEntry,0,1,true,true,true};
        cues::Observation cueObservation{};
        check(cues::qualify(cueTicket,cueCapture,cueObservation)==cues::Result::accepted
            && adventure::native_bridge::submit({cueBinding,cueObservation}),"original manager fixture qualifies exact opening owner before gateway");
        frame=persistent.update(15,true,selected);
        check(frame.opening.gatewayRequested && frame.opening.nativeReady,"graph advances gateway only after both actual receipt contracts");
        check(persistent.opening().retains_region(12,selected),"exact selected Forest route authorizes clock continuation");
        for(const auto foreignBubble:{0U,13U,15U,63U,64U})
            check(!persistent.opening().retains_region(foreignBubble,selected),"foreign or initial bubble cannot use regional clock exception");
        auto foreignSelection=selected;++foreignSelection.revision;
        check(!persistent.opening().retains_region(12,foreignSelection),"new selection revision cannot continue old regional clock");
        activity::activity_clock::Service retainedClock;
        const activity::activity_clock::Policy clockPolicy{opening.overlay->hostScenario,15,{false,1000.0F/30.0F}};
        check(retainedClock.begin(owner,97,19,clockPolicy,100),"clock begins only at explicit initial admission");
        const auto admittedDomain=retainedClock.domain();activity::activity_clock::Publication continued{};
        check(!retainedClock.project(owner,97,12,150,continued),"ordinary clock projection retains strict bubble admission");
        check(retainedClock.project_retained(owner,97,admittedDomain,150,continued)
            && continued.domain==admittedDomain && continued.elapsedTicks>0,
            "qualified regional continuation retains exact initial domain and elapsed origin");
        auto staleDomain=admittedDomain;++staleDomain.epoch;
        check(!retainedClock.project_retained(owner,97,staleDomain,151,continued),"foreign clock epoch rejected");
        staleDomain=admittedDomain;staleDomain.bubble=12;
        check(!retainedClock.project_retained(owner,97,staleDomain,151,continued),"continuation cannot rewrite admission bubble");
        auto otherOwner=owner;++otherOwner.incarnation.value;
        check(!retainedClock.project_retained(otherOwner,97,admittedDomain,151,continued)
            && !retainedClock.project_retained(owner,98,admittedDomain,151,continued)
            && !retainedClock.project_retained(owner,97,admittedDomain,149,continued),
            "wrong owner, boot and time reversal cannot continue clock");
        auto placementBatch=frame.placements;adventure::gateway::predicates::Set names{};
        check(adventure::gateway::project(*opening.gateway,15,placementBatch,names) && placementBatch.count==11
            && names.count==1 && names.hashes[0]==0xD4C4F182,"same-owner gateway adds only source/destination and authored predicate");
        activity::placement::wire::Batch regionalPlacements{};adventure::gateway::predicates::Set regionalNames{};
        check(adventure::gateway::project(*opening.gateway,12,regionalPlacements,regionalNames)
            && regionalPlacements.count==0 && regionalNames==names,"native regional loading does not send out-of-scope placement authority");
        check(adventure::gateway::project(*opening.gateway,15,regionalPlacements,regionalNames)
            && regionalPlacements.count==2 && regionalPlacements.entries[0].generation==1,
            "return uses the retained authored generation without a synthetic retirement");
        check(placementBatch.entries[9].generation==1 && placementBatch.entries[10].generation==1
            && placementBatch.entries[9].slot==6 && placementBatch.entries[10].slot==7,"actual generic authored-placement generation and exact endpoints");
        const auto beforeCollision=placementBatch;const auto beforeNames=names;
        check(!adventure::gateway::project(*opening.gateway,15,placementBatch,names)
            && placementBatch.count==beforeCollision.count && names==beforeNames,"duplicate feature projection rejects without erasing existing requests");

        for(const auto bubble:{15U,15U,12U,15U}) {
            frame=persistent.update(bubble,true,selected);check(frame.opening.dialogueSubmitted && frame.opening.dialogue.activeRow==voice::kNoRow && !bridge::lookup(opening.dialogue.definition).epoch,"refresh and reentry cannot replay opening conversation");
        }
        check(!persistent.opening().failed() && frame.opening.binding->activity==opening.activity,"same-owner runtime remains healthy");
        const auto retainedCue=persistent.opening().ticket();
        const auto retainedVoice=persistent.opening().dialogue_ticket();
        frame=persistent.update(12,false,selected);
        check(frame.opening.gatewayRequested && persistent.opening().ticket()==retainedCue
            && persistent.opening().dialogue_ticket()==retainedVoice,
            "native regional loading retains exact owner, boot, selection and conversation generation");
        auto changed=selected;++changed.revision;
        frame=persistent.update(12,true,changed);
        check(frame.opening.conflictingSelection && persistent.opening().ticket()==retainedCue,
            "changed selection cannot replace an admitted regional lease");
        check(!persistent.opening().retains_region(12,selected),"conflicted lease cannot continue regional clock");
        frame=persistent.update(15,true,selected);
        check(frame.opening.conflictingSelection && persistent.opening().ticket()==retainedCue,
            "return to original selection cannot clear a detected lease conflict");
        bridge::release(owner);adventure::native_bridge::release(owner);
        activity::PersistentActivity next;
        const feedback::Owner nextOwner{owner.sessionId,{owner.incarnation.value+1}};
        check(next.begin(nextOwner,activity::mercury::kActivity,document,98),"new world incarnation begins independently");
        check(!next.update(12,true,selected).opening.requested,
            "new incarnation appearing in Forest cannot inherit retired Lighthouse admission");
        check(!next.opening().retains_region(12,selected),"new incarnation cannot inherit regional clock exception");
        frame=next.update(15,true,selected);
        check(frame.opening.requested && !frame.opening.nativeReady && !frame.opening.dialogueSubmitted
            && !frame.opening.gatewayRequested && next.opening().ticket().owner==nextOwner,
            "new incarnation must receive its own native cue and conversation callbacks");
        check(!bridge::submit({armed,observed}) && !adventure::native_bridge::submit({cueBinding,cueObservation}),
            "retired callbacks cannot authorize the new incarnation gateway");
        frame=next.update(15,true,selected);
        check(!frame.opening.gatewayRequested && !frame.opening.nativeReady && !frame.opening.dialogueSubmitted,
            "rejected retired receipts leave new gateway unpublished");
        bridge::release(nextOwner);adventure::native_bridge::release(nextOwner);
    }
    std::printf("PASS %u dialogue codec, full native-state qualification, mailbox and assembled opening checks; audible playback remains live acceptance\n",checks);
}
