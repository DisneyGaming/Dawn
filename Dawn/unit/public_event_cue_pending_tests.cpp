#include "server/runtime/activity/public_event_cue_pending.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <vector>
namespace p=dawn::server::runtime::activity::public_event_cue_pending;
namespace f=p::feedback;namespace b=p::bridge;
unsigned checks{};
void check(bool value,const char* label) {++checks;if(!value){std::fprintf(stderr,"FAIL %u: %s\n",checks,label);std::exit(1);}}
std::vector<std::byte> read(const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary|std::ios::ate);check(bool(file),"original fixture opens");
    const auto size=file.tellg();check(size>0 && size<10000,"original fixture bounded");
    std::vector<std::byte> out(static_cast<std::size_t>(size));file.seekg(0);file.read(reinterpret_cast<char*>(out.data()),size);
    check(bool(file),"original fixture reads");return out;
}
int main(int argc,char** argv) {
    if(argc!=2){std::fprintf(stderr,"usage: public_event_cue_pending_tests original-delayed-fixtures\n");return 2;}
    const std::filesystem::path fixtures(argv[1]);
    const auto body=read(fixtures/"authority.bin"),pendingEntry=read(fixtures/"pending-entry.bin"),readyEntry=read(fixtures/"ready-entry.bin");
    f::Ticket t{};t.owner={51,{19}};t.boot=7;t.definitionRevision=1;t.selectionRevision=3;t.activity=29;
    t.definition=0x80F5E337;t.definitionOffset=0xB88;t.table=0x80F5E35B;t.stringBank=0x80F56034;
    t.title=0x50EFBC4D;t.detail=0x290979DD;t.admissionScope=15;
    t.request={0xC8229B2B,0x00D1C5B9,107,0,15};t.request.readiness={0xC8229B2B,70,106};
    t.presentation=f::Presentation::authoredProgress;t.progressLabel=0x6267DC7C;
    b::Mailbox mailbox;check(mailbox.bind(t),"current world binds immutable cue");
    p::Identity id{};id.binding=mailbox.lookup(t.definition);id.source={0x1234,0};id.component=0x100000;id.authority=0x4567;id.componentLink=0x789A;
    auto prior=body;prior[0x10]^=std::byte{1};f::Observation observation{};
    f::Capture initial{t,id.source,1,f::kProducerRva,body,prior,body,pendingEntry,0,1,true,true,true,{},0};
    f::Capture later{t,id.source,2,f::kProducerRva,body,body,body,readyEntry,1,1,true,true,true,{},0};
    check(f::qualify(t,initial,observation)==f::Result::managerEntry,"native mode0 insertion remains unaccepted");
    check(f::qualify(t,later,observation)==f::Result::unchanged,"generic unchanged guard remains intact");
    p::Pending pending;
    check(pending.remember(id,initial,1,0,1),"exact original pending insertion retained");
    check(pending.index(id)==0,"same native lease locates original manager index");
    auto notReady=later;notReady.entry=pendingEntry;
    check(!pending.observe(id,notReady,1,1,observation) && pending.active(),"unchanged pending native tick cannot acknowledge readiness");
    check(pending.observe(id,later,0,1,observation),"original native mode0 to1 with unchanged full body qualifies later");
    check(observation.ticket==t && observation.source==id.source && observation.sequence==2 && observation.managerIndex==0,
        "receipt preserves owner ticket source original insertion and later observation sequence");
    check(!pending.observe(id,later,0,1,observation),"pending receipt is consumed once");
    // A subsequent unrelated insertion may grow the manager, but cannot replace
    // the retained slot or its native serial/context identity.
    check(pending.remember(id,initial,1,0,1),"rearm isolated fixture");auto growing=later;growing.managerCountBefore=growing.managerCountAfter=2;
    check(pending.observe(id,growing,0,1,observation),"unrelated manager entry does not block exact retained insertion");
    for(unsigned which=0;which<14;++which) {
        auto changed=id;
        if(which==0)++changed.binding.epoch;if(which==1)++changed.binding.ticket.owner.sessionId;
        if(which==2)++changed.binding.ticket.owner.incarnation.value;if(which==3)++changed.binding.ticket.boot;
        if(which==4)++changed.binding.ticket.definitionRevision;if(which==5)++changed.binding.ticket.selectionRevision;
        if(which==6)++changed.binding.ticket.request.event;if(which==7)++changed.source.member;
        if(which==8)++changed.component;if(which==9)++changed.authority;if(which==10)++changed.componentLink;
        if(which==11)changed.header[0]^=std::byte{1};if(which==12)changed.definition[0]^=std::byte{1};
        if(which==13)changed.authorityObject[0]^=std::byte{1};
        check(pending.remember(id,initial,1,0,1),"arm before stale native identity");
        check(pending.index(changed)==UINT64_MAX,"foreign lease cannot locate retained entry");
        check(!pending.observe(changed,later,0,1,observation) && !pending.active(),"changed lease invalidates pending insertion");
    }
    for(unsigned which=0;which<12;++which) {
        auto bad=later;auto altered=body;
        if(which==0)bad.originalForwarded=false;if(which==1)bad.producerRva=0;if(which==2)bad.sequence=1;
        if(which==3)bad.managerReadyBefore=false;if(which==4)bad.managerReadyAfter=false;
        if(which==5)bad.managerCountAfter=0;if(which==6)bad.managerCountBefore=2;
        if(which==7)bad.entryIndex=1;
        altered[0x100]^=std::byte{1};
        if(which==8)bad.incoming=altered;if(which==9)bad.prior=altered;if(which==10)bad.applied=altered;
        if(which==11)bad.managerCountBefore=bad.managerCountAfter=17;
        check(pending.remember(id,initial,1,0,1),"arm before invalid later capture");
        check(!pending.observe(id,bad,0,1,observation),"bad original call body count or slot cannot qualify later");
    }
    for(const auto offset:{0U,2U,4U,8U,0xCU,0x10U,0x18U,0x1CU,0x20U,0x24U,0x28U,0x2BU,0x2CU,0x58U,
        0x5CU,0x60U,0x64U,0x70U,0x74U,0x78U,0x7CU,0x110U,0x124U,0x140U,0x141U}) {
        auto altered=readyEntry;altered[offset]^=offset==0x2C?std::byte{4}:std::byte{1};auto bad=later;bad.entry=altered;
        check(pending.remember(id,initial,1,0,1),"arm before replaced native entry");
        check(!pending.observe(id,bad,0,1,observation),"every exact manager identity presentation and count field is guarded");
    }
    for(const unsigned duplicates:{0U,2U}) {
        check(pending.remember(id,initial,1,0,1),"arm before manager alias");
        check(!pending.observe(id,later,0,duplicates,observation),"missing or duplicate manager identity rejects");
    }
    for(unsigned which=0;which<10;++which) {
        auto bad=initial;auto changed=id;auto altered=pendingEntry;unsigned before=0,after=1;std::uint8_t flag=1;
        if(which==0)before=1;if(which==1)after=2;if(which==2)flag=0;
        if(which==3)bad.prior=body;if(which==4)bad.managerCountBefore=1;if(which==5)bad.entry=readyEntry;
        if(which==6){altered[0x24]^=std::byte{1};bad.entry=altered;}
        if(which==7)changed.binding.epoch=0;
        if(which==8){changed.binding.ticket.request.clear=true;bad.ticket=changed.binding.ticket;}
        if(which==9){changed.binding.ticket.presentation=f::Presentation::activityObjective;bad.ticket=changed.binding.ticket;}
        check(!pending.remember(changed,bad,flag,before,after),"no preexisting ready foreign malformed or Adventure entry seeds pending");
    }
    check(pending.remember(id,initial,1,0,1),"arm before retirement");mailbox.release(t.owner);
    auto retired=id;retired.binding=mailbox.lookup(t.definition);
    check(!pending.observe(retired,later,0,1,observation),"retired mailbox cannot acknowledge native entry");
    check(mailbox.bind(t),"same world tuple may get a new explicit binding epoch");auto replaced=id;replaced.binding=mailbox.lookup(t.definition);
    check(replaced.binding.epoch!=id.binding.epoch,"binding epoch never reused");
    check(pending.remember(id,initial,1,0,1),"arm old insertion fixture");
    check(!pending.observe(replaced,later,0,1,observation),"rebound owner cannot reuse previous insertion");
    std::printf("PASS %u original delayed cue and lifetime checks\n",checks);
}
