#include "server/runtime/activity/public_event_engagement_runtime.h"
#include <cstdio>
#include <cstdlib>

namespace pe=sunrise::server::runtime::activity::public_event;
namespace feedback=pe::engagement_feedback;
namespace bridge=pe::engagement_bridge;
namespace sense=pe::engagement_sense;
unsigned checks{};
void check(bool value,const char* what){++checks;if(!value){std::fprintf(stderr,"FAIL %u: %s\n",checks,what);std::exit(1);}}
feedback::Ticket ticket(std::uint64_t owner){
    feedback::Ticket t{};t.owner={owner,{7}};t.boot=9;t.definitionRevision=3;t.selectionRevision=2;t.event=11;
    t.definition=0x80F5E466;t.definitionOffset=0x358;t.request.registry=0xC8229B2B;t.request.slot=106;
    t.request.scope=15;t.request.collection=feedback::wire::Collection::activePlayers;return t;
}
pe::EngagementContext context(const feedback::Ticket& t){return {t.owner,t.boot,t.definitionRevision,t.selectionRevision,t.event,29,15,true,true};}
void submit(const feedback::Ticket& t,std::uint64_t sequence){
    const auto binding=bridge::lookup(t.definition);check(binding.epoch!=0 && binding.ticket==t,"observer ticket retained before authority publication");
    // Modeled already-qualified observer output. Actual before/after capture
    // qualification is covered by public_event_engagement_tests and its native fixtures.
    check(bridge::submit({binding,{t,{0x1234,0x250},sequence}}),"qualified observation enters exact ticket mailbox");
}
int main(){
    const sense::Output occupied{1,0,1,true},empty{1,0,0,true};
    for(bool senseFirst:{false,true}){
        const auto t=ticket(senseFirst?101:102);auto current=context(t);pe::EngagementRuntime runtime;
        check(runtime.begin(t,29)&&!runtime.begin(t,29),"one retained event lifetime");
        check(runtime.retained_authority()==nullptr,"unpublished engagement has no retained body");
        check(!runtime.observe(t,15,sense::kSchema,occupied),"unsolicited prepublication sense rejected");
        current.arrived=false;check(!runtime.update(current).publish && !bridge::lookup(t.definition).epoch,"loading does not bind or publish");current.arrived=true;
        current.admitted=false;check(!runtime.update(current).publish,"missing exact registry admission stays pending");current.admitted=true;
        current.bubble=14;check(!runtime.update(current).publish,"wrong bubble stays pending");current.bubble=15;
        auto f=runtime.update(current);check(f.publish && !f.applied && !f.readyForCue,"publication cannot acknowledge native readiness");
        check(!runtime.observe(t,14,sense::kSchema,occupied),"foreign native bubble cannot satisfy participant receipt");
        if(senseFirst){check(runtime.observe(t,15,sense::kSchema,occupied),"native sense may precede apply receipt");check(!runtime.update(current).readyForCue,"participant count alone does not unlock cue");}
        submit(t,1);f=runtime.update(current);check(f.applied && f.readyForCue==senseFirst,"apply requires matching native participant receipt");
        if(!senseFirst){check(runtime.observe(t,15,sense::kSchema,empty),"empty native participant state accepted");check(!runtime.update(current).readyForCue,"empty membership stays pending");auto next=occupied;next.revision=2;check(runtime.observe(t,15,sense::kSchema,next),"later participant revision accepted");}
        f=runtime.update(current);check(f.readyForCue && f.publish && !f.failed,"both exact native prerequisites offer the cue");
        for(unsigned i=0;i<12;++i)check(runtime.update(current).readyForCue,"stable updates retain authority without renewal");
        current.admitted=false;check(!runtime.update(current).publish&&!runtime.frame().readyForCue,"leaving admission hides active cue offer");
        check(runtime.retained_authority() && *runtime.retained_authority()==t.request,"inactive engagement retains exact published authority for its world owner");current.admitted=true;
        check(runtime.update(current).readyForCue,"same retained run can resume admitted projection");
        auto stale=t;++stale.selectionRevision;check(!runtime.observe(stale,15,sense::kSchema,occupied),"stale sense ticket rejected");
        auto wrongGeneration=occupied;wrongGeneration.generation=1;wrongGeneration.revision=99;
        check(!runtime.observe(t,15,sense::kSchema,wrongGeneration),"wrong native generation cannot manufacture readiness");
        check(!runtime.observe(t,15,sense::kSchema,occupied),"replayed native revision rejected");
        bridge::release(t.owner);
    }
    for(unsigned field=0;field<6;++field){
        const auto t=ticket(200+field);auto current=context(t);pe::EngagementRuntime runtime;check(runtime.begin(t,29),"fresh exact ticket begins");
        check(runtime.update(current).publish,"ticket publishes before stale transition");
        if(field==0)++current.owner.incarnation.value;if(field==1)++current.boot;if(field==2)++current.definitionRevision;
        if(field==3)++current.selectionRevision;if(field==4)++current.event;if(field==5)++current.activity;
        check(runtime.update(current).stale && !runtime.frame().publish,"changed committed identity fails closed");
        check(runtime.retained_authority() && *runtime.retained_authority()==t.request,"stopped engagement preserves original owner authority without rebinding");
        current=context(t);check(!runtime.update(current).publish,"old identity cannot revive an invalidated lifetime");bridge::release(t.owner);
    }
    const auto first=ticket(301);auto second=first;second.definition=0x80F5E467;second.request.slot=105;
    pe::EngagementRuntime a,b;check(a.begin(first,29)&&b.begin(second,29),"two independently retained tickets share one activity owner");
    check(a.update(context(first)).publish&&b.update(context(second)).publish,"both definitions bind independently");
    submit(second,2);submit(first,1);check(a.observe(first,15,sense::kSchema,occupied)&&b.observe(second,15,sense::kSchema,occupied),"each sense routes to its own event ticket");
    check(a.update(context(first)).readyForCue,"first drain gets its own receipt");check(b.update(context(second)).readyForCue,"second receipt remains available under same owner");
    auto renewed=first;++renewed.event;pe::EngagementRuntime rejected;check(rejected.begin(renewed,29),"new request validates structurally");
    check(rejected.update(context(renewed)).failed&&!rejected.frame().publish,"native definition cannot rebind before actual owner retirement");
    bridge::release(first.owner);
    std::printf("PASS %u retained engagement runtime checks\n",checks);
}
