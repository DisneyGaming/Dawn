#include "server/runtime/activity/adventure_native_bridge.h"
#include "client/hooks/bootflow/adventure_cue_observer.h"
#include "server/runtime/activity/persistent_activity.h"
#include "server/runtime/activity/mercury_definition.h"
#include "middleware/encoding/bit_writer.h"
#include "core/logging/log.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <vector>

namespace sunrise::core::log {void write(Channel,Level,std::string_view) noexcept {}}
namespace bridge=sunrise::server::runtime::activity::adventure::native_bridge;
namespace f=bridge::feedback;
namespace cue=f::wire;
namespace bits=sunrise::middleware::encoding::bits;
namespace observer=sunrise::client::hooks::bootflow::adventure_cue_observer;
unsigned checks{};
void check(bool value,const char* what) {
    ++checks;if(!value){std::fprintf(stderr,"FAIL %u: %s\n",checks,what);std::exit(1);}
}
std::vector<std::byte> read(const std::filesystem::path& path) {
    std::ifstream in(path,std::ios::binary|std::ios::ate);check(bool(in),"fixture open");
    const auto n=in.tellg();check(n>0 && n<10000,"fixture size");
    std::vector<std::byte> b(static_cast<std::size_t>(n));in.seekg(0);in.read(reinterpret_cast<char*>(b.data()),n);
    check(bool(in),"fixture read");return b;
}
int main(int argc,char** argv) {
    if(argc!=4){std::fprintf(stderr,"usage: adventure_cue_tests actual-fixture-directory export-body mercury-json\n");return 2;}
    const std::filesystem::path directory(argv[1]);
    const auto originalBody=read(directory/"runner-opening.body");
    const auto originalDecoded=read(directory/"runner-opening.decoded.bin");
    const auto nativeMask=read(directory/"type68-decoder-written-mask.bin");
    const auto nativeFilled=read(directory/"runner-opening.decoded-filled.bin");
    check(nativeMask.size()==cue::kDecodedBytes,"complete native decoder write mask");
    for(std::size_t i=0;i<nativeMask.size();++i)
        check(cue::decoder_writes(i)==(nativeMask[i]==std::byte{255}),"schema fields match actual original write coverage");
    const std::array<std::uint32_t,3> definitions{0x80F46D67,0x80F46D7B,0x80F46D8B};
    f::Ticket finalTicket{};f::Observation finalObservation{};
    for(std::size_t i=0;i<definitions.size();++i) {
        std::array<char,48> filename{};std::snprintf(filename.data(),filename.size(),"%08X.definition.bin",definitions[i]);
        const auto definition=read(directory/filename.data());
        std::snprintf(filename.data(),filename.size(),"%08X.manager-entry.bin",definitions[i]);
        const auto entry=read(directory/filename.data());
        f::Ticket ticket{};ticket.owner={0x9EAA300100200001,{1}};ticket.boot=7;ticket.definitionRevision=9;
        ticket.selectionRevision=3;ticket.activity=static_cast<std::int16_t>(1076+i);ticket.definition=definitions[i];
        ticket.definitionOffset=f::field<std::int64_t>(definition,0x78);ticket.nativeScope=UINT32_MAX;
        ticket.request={f::field<std::uint32_t>(definition,0xBB8),0xC9E4C596,0,0};
        ticket.table=f::field<std::uint32_t>(definition,0xBE4);
        ticket.stringBank=f::field<std::uint32_t>(entry,0x58);ticket.title=0x099C20CB;ticket.detail=0x9661E8EB;
        check(f::valid(ticket),"actual authored ticket");
        const auto expected=cue::decoded(ticket.request);
        check(originalDecoded.size()==expected.size() && std::equal(expected.begin(),expected.end(),originalDecoded.begin()),
            "complete expected image equals original decoder");
        std::array<std::byte,601> encoded{};bits::Writer writer(encoded);
        check(cue::write(writer,ticket.request) && writer.bit_count()==cue::kBits,"independent cue codec");
        check(originalBody.size()==encoded.size() && std::equal(encoded.begin(),encoded.end(),originalBody.begin()),
            "cue wire equals previously original-decoded production body");
        if(i==2){std::ofstream out(argv[2],std::ios::binary);out.write(reinterpret_cast<const char*>(encoded.data()),encoded.size());
            check(bool(out),"new codec wire exported for original decoder");}
        auto prior=expected;prior[0x10]^=std::byte{1};
        f::Capture capture{ticket,{0x1234,0x180},1,f::kProducerRva,expected,prior,expected,entry,0,1,true,true,true};
        f::Observation observation{};
        check(f::qualify(ticket,capture,observation)==f::Result::accepted,"full native presentation entry qualifies");
        auto withPadding=capture;withPadding.incoming=nativeFilled;withPadding.applied=nativeFilled;
        check(f::qualify(ticket,withPadding,observation)==f::Result::accepted,"native untouched padding is retained, not presumed zero");
        finalTicket=ticket;finalObservation=observation;
        for(std::size_t byte=0;byte<expected.size();++byte) {
            auto changed=expected;changed[byte]^=std::byte{1};auto bad=capture;bad.incoming=changed;
            check(f::qualify(ticket,bad,observation)==f::Result::body,"reject every changed incoming byte");
            bad=capture;bad.applied=changed;
            check(f::qualify(ticket,bad,observation)==f::Result::body,"reject every changed applied byte");
        }
        for(const auto offset:{0U,4U,0x10U,0x124U,0x110U,0x70U,0x58U,0x5CU,0x20U,0x24U,0x140U,0x141U}) {
            auto altered=entry;altered[offset]^=std::byte{1};auto bad=capture;bad.entry=altered;
            check(f::qualify(ticket,bad,observation)==f::Result::managerEntry,"reject unrelated or incomplete presentation entry");
        }
        auto bad=capture;bad.originalForwarded=false;check(f::qualify(ticket,bad,observation)==f::Result::identity,"no original forwarding");
        bad=capture;bad.ticket.boot++;check(f::qualify(ticket,bad,observation)==f::Result::identity,"stale boot");
        bad=capture;bad.ticket.selectionRevision++;check(f::qualify(ticket,bad,observation)==f::Result::identity,"different selected revision");
        bad=capture;bad.managerReadyBefore=false;check(f::qualify(ticket,bad,observation)==f::Result::managerUnavailable,"body acceptance is not readiness");
        bad=capture;bad.managerReadyAfter=false;check(f::qualify(ticket,bad,observation)==f::Result::managerUnavailable,"manager became unavailable");
        bad=capture;bad.prior=expected;check(f::qualify(ticket,bad,observation)==f::Result::unchanged,"no receipt from unchanged authority");
        bad=capture;bad.managerCountAfter=2;check(f::qualify(ticket,bad,observation)==f::Result::managerEntry,"ambiguous queue growth");
        bad=capture;bad.incoming=std::span(expected).first(64);check(f::qualify(ticket,bad,observation)==f::Result::body,"prefix insufficient");
        bad=capture;bad.source.member=UINT32_MAX;check(f::qualify(ticket,bad,observation)==f::Result::identity,"absent source");
    }
    bridge::Mailbox mailbox;check(mailbox.bind(finalTicket),"arm before publication");
    auto binding=mailbox.lookup(finalTicket.definition);check(binding.epoch!=0,"find exact definition");
    check(mailbox.bind(finalTicket),"idempotent ticket");auto competing=finalTicket;competing.owner.sessionId++;
    check(!mailbox.bind(competing),"cannot confuse shared asset across owners");
    const bridge::Event event{binding,finalObservation};check(mailbox.submit(event),"accept qualified receipt");
    check(!mailbox.submit(event),"no duplicate receipt");check(!mailbox.lookup(finalTicket.definition).epoch,"no repeat observation");
    std::array<bridge::Event,2> drained{};check(mailbox.drain(competing.owner,drained)==0,"foreign owner cannot drain");
    check(mailbox.drain(finalTicket.owner,drained)==1,"owner drains once");
    mailbox.release(finalTicket.owner);check(mailbox.bind(finalTicket),"rebind after release");
    check(mailbox.lookup(finalTicket.definition).epoch!=binding.epoch,"new capture epoch");
    check(!mailbox.submit(event),"released capture rejected despite same asset");
    check(!observer::begin(nullptr,nullptr).binding.epoch,"observer null input is inert");
    check(!observer::finish(nullptr,{}),"observer missing capture is inert");
    for(unsigned ring=0;ring<3;++ring) {auto r=finalTicket.request;r.ring=static_cast<std::uint8_t>(ring);
        auto count=bits::Writer::measuring();check(cue::write(count,r)&&count.bit_count()==cue::kBits,"bounded cue ring");}
    auto invalid=finalTicket.request;invalid.ring=3;auto count=bits::Writer::measuring();
    check(!cue::write(count,invalid)&&count.bit_count()==0,"invalid ring writes nothing");
    namespace activity=sunrise::server::runtime::activity;
    namespace coo=sunrise::state::activity::coo;
    std::string error;
    std::shared_ptr<const coo::script::MissionDocument> document=coo::script::MissionDocument::read_native_policy(argv[3],activity::mercury::kProfile,error);
    if(!document)std::fprintf(stderr,"%s\n",error.c_str());
    check(bool(document),"assembled Mercury JSON parses");
    check(activity::PersistentActivity::valid(activity::mercury::kActivity,*document),"assembled native activity validates");
    const auto request=read(directory.parent_path()/"live-r2-31956"/"request-114750-target-1078.bin");
    activity::adventure_start::wire::Request selected{};
    check(activity::adventure_start::wire::parse(request,selected),"actual full90-byte committed selection");
    activity::PersistentActivity persistent;const f::Owner owner{51,{19}};
    check(persistent.begin(owner,activity::mercury::kActivity,document,51),"persistent owner begins");
    check(!persistent.update(15,false,selected).opening.requested,"no opening during world load");
    check(!persistent.update(14,true,selected).opening.requested,"wrong bubble cannot start opening");
    check(!persistent.update(15,true,selected,false).opening.requested,"roster warmup cannot consume cue");
    auto frame=persistent.update(15,true,selected);
    check(frame.opening.requested && !frame.opening.nativeReady && frame.opening.binding->activity==1078,
        "committed selection requests exact Runner opening");
    check(frame.placements.count==9 && frame.populations.count==2,"base Mercury services retained");
    const auto openingTicket=persistent.opening().ticket();
    auto current=bridge::lookup(openingTicket.definition);
    check(current.epoch && current.ticket==openingTicket,"ticket precedes native publication");
    const auto actualEntry=read(directory/"80F46D8B.manager-entry.bin");
    const auto complete=cue::decoded(openingTicket.request);auto prior=complete;prior[0x10]^=std::byte{1};
    f::Capture capture{openingTicket,{0x1234,0x180},2,f::kProducerRva,complete,prior,complete,actualEntry,0,1,true,true,true};
    f::Observation observed{};
    check(f::qualify(openingTicket,capture,observed)==f::Result::accepted,"original entry accepts owner ticket");
    check(bridge::submit({current,observed}),"copy native receipt to server mailbox");
    frame=persistent.update(15,true,selected);
    check(frame.opening.nativeReady && frame.opening.requested && frame.placements.count==9 && frame.populations.count==2,
        "only qualified receipt advances opening executor");
    check(!bridge::submit({current,observed}),"opening cannot advance twice");
    frame=persistent.update(12,true,selected);
    check(frame.opening.requested && frame.opening.binding->activity==1078,"selected root lease persists into Forest");
    auto competingSelection=selected;competingSelection.selection.activityIndex=1077;competingSelection.revision++;
    frame=persistent.update(15,true,competingSelection);
    check(frame.opening.conflictingSelection && frame.opening.binding->activity==1078,"unsupported switch cannot replace native lease");
    bridge::release(owner);
    std::printf("PASS %u Adventure cue/receipt checks; actual original-code fixtures; observer compiled\n",checks);
}
