#include "server/runtime/activity/mercury_public_event_opening.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
namespace rt=dawn::server::runtime::activity;
namespace pe=rt::public_event;
namespace mc=rt::mercury::public_events;
namespace coo=dawn::state::activity::coo;
namespace bridge=rt::adventure::native_bridge;
unsigned checks{};
void check(bool value,const char* label){++checks;if(!value){std::fprintf(stderr,"FAIL %s\n",label);std::exit(1);}}
int main(int argc,char** argv){
 const rt::population::Owner owner{123,{2}};
 rt::population::Service population;
 check(population.begin(owner,mc::kOpeningSources,456),"scoped population service");
 pe::OpeningRuntime runtime;
 check(runtime.begin(mc::kOpeningDefinition,owner,456,7,8,9,population),"opening begins with configured quota");
 auto step=[&](){return runtime.update(15,true,true,29,8);};
 auto frame=step();
 check(frame.cue.hasProgress && frame.cue.progress.current==0 && frame.cue.progress.target==9,"objective starts at zero before any deaths");
 check(population.project(15).count==0,"objective publication does not spawn a cohort");
 const auto binding=bridge::lookup(0x80F5E337);
 check(binding.epoch && bridge::submit({binding,{binding.ticket,{100,0x1000},1,0,
     bridge::feedback::wire::manager_id(binding.ticket.request.event)}}),"qualified native objective fixture");
 frame=step();const auto batch=population.project(15);
 check(batch.count==2 && batch.entries[0].source.looseRequested==9 && batch.entries[1].source.looseRequested==9,"cumulative budgets come from graph commands");
 check(frame.cue.progress.current==0 && !frame.combatReady,"requests neither count as deaths nor native actor admission");
 auto event=[&](unsigned source,unsigned actor,pe::opening_native::Kind kind){
  const coo::PopulationOwner sourceOwner{owner.sessionId,456,owner.incarnation.value,mc::kOpeningSourceCommands[source].asset,2};
  return pe::opening_native::Event{{owner,sourceOwner,15},{sourceOwner,1000+actor,2000+actor},3000+source,kind};
 };
 auto accept=[&](const pe::opening_native::Event& e){return runtime.observe_accepted(e,coo::PopulationIntake::accepted);};
 const auto born=pe::opening_native::Kind::admitted,dead=pe::opening_native::Kind::died,retired=pe::opening_native::Kind::retired;
 check(!accept(event(0,1,dead)),"death before known birth rejected");
 check(accept(event(0,1,born)) && accept(event(1,2,born)),"both native cohorts admitted");
 frame=step();check(frame.combatReady && !runtime.defeat_goal_completed(),"actor readiness is separate from encounter completion");
 auto firstDeath=event(0,1,dead);
 auto foreign=firstDeath;++foreign.lease.source.run;foreign.actor.owner=foreign.lease.source;
 check(!accept(foreign),"wrong boot death rejected");
 foreign=firstDeath;++foreign.sourceHandle;check(!accept(foreign),"wrong source handle rejected");
 check(!runtime.observe_accepted(firstDeath,coo::PopulationIntake::duplicate),"central duplicate cannot count");
 check(accept(firstDeath) && !accept(firstDeath),"one actual death counted once even if replayed");
 check(accept(event(1,2,retired)),"native retirement accepted for accounting");
 check(!accept(event(1,2,dead)),"retirement without death never becomes a kill");
 frame=step();check(frame.cue.progress.current==1 && runtime.defeated()==1,"only death changes objective numerator");
 for(unsigned actor=3;actor<=10;++actor){
  check(accept(event(actor%2,actor,born)),"later native member admitted without resubmitting source readiness");
  check(accept(event(actor%2,actor,dead)),"later native death counted");
 }
 frame=step();check(frame.cue.progress.current==9 && runtime.defeat_goal_completed(),"quota receipt is consumed in the same authority update");
 frame=step();check(runtime.defeat_goal_completed(),"UE completes only from actual death quota");
 check(accept(event(0,11,born)) && accept(event(0,11,dead)),"late source members retain their ownership");
 frame=step();check(runtime.defeated()==10 && frame.cue.progress.current==9,"UI clamps completed progress while ledger retains exact deaths");
 check(population.project(15).entries[0].source.looseRequested==9,"deaths do not reset or renew source generation");
 auto invalid=mc::kOpeningDefinition;invalid.defeatGoal=8;check(!pe::OpeningRuntime::valid(invalid),"quota must match graph observation");
 bridge::Mailbox mailbox;const auto active=runtime.ticket();auto next=mc::kGatekeeperCue;
 next.owner=active.owner;next.boot=active.boot;next.definitionRevision=active.definitionRevision;next.selectionRevision=active.selectionRevision;
 check(mailbox.bind(active) && !mailbox.advance(active,next),"unacknowledged directive cannot advance");
 const auto first=mailbox.lookup(active.definition);
 const bridge::Event receipt{first,{active,{100,0x1000},1,0,bridge::feedback::wire::manager_id(active.request.event)}};
 check(mailbox.submit(receipt) && mailbox.pending(owner),"cue receipt wakes exact owner");
 check(!mailbox.pending({owner.sessionId,{owner.incarnation.value+1}}) && !mailbox.advance(active,next),"foreign incarnation and undrained transition rejected");
 std::array<bridge::Event,1> drained{};check(mailbox.drain(active,drained)==1 && !mailbox.pending(owner),"inspection leaves cue receipt available to owner");
 auto bad=next;++bad.boot;check(!mailbox.advance(active,bad),"cross-boot cue adoption rejected");
 bad=next;++bad.selectionRevision;check(!mailbox.advance(active,bad),"cross-selection cue adoption rejected");
 bad=next;bad.request.ring=active.request.ring;check(!mailbox.advance(active,bad),"same-ring substitution rejected");
 check(mailbox.advance(active,next) && !mailbox.submit(receipt),"exact consumed transition invalidates old capture");
 check(mailbox.lookup(active.definition).epoch>first.epoch,"successor gets new epoch");
 // Optional exact native manager capture from the live numeric objective. The
 // artifact stays outside the repo; all fields are checked by production code.
 if(argc==2) {
  std::array<std::byte,0x148> entry{};std::ifstream file(argv[1],std::ios::binary);
  check(bool(file.read(reinterpret_cast<char*>(entry.data()),entry.size())),"native manager capture reads in full");
  auto prior=bridge::feedback::wire::decoded(active.request);const auto incoming=bridge::feedback::wire::decoded(next.request);
  const bridge::feedback::Capture captured{next,{100,0x1000},1,bridge::feedback::kProducerRva,
      incoming,prior,incoming,entry,1,2,true,true,true};
  bridge::feedback::Observation observed;
  check(bridge::feedback::qualify(next,captured,observed)==bridge::feedback::Result::accepted,"actual native numeric objective passes complete receipt qualification");
  next.progressFormat=bridge::feedback::ProgressFormat::percent;auto wrong=captured;wrong.ticket=next;
  check(bridge::feedback::qualify(next,wrong,observed)==bridge::feedback::Result::managerEntry,"percent contract rejects numeric display rather than silently accepting a mismatch");
 }
 bridge::release(owner);
 std::printf("PASS %u opening progress checks\n",checks);
}
