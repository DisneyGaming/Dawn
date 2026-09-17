#include "server/runtime/activity/mercury_definition.h"
#include "server/runtime/activity/persistent_activity.h"
#include "middleware/encoding/bit_reader.h"
#include "mission_parameter_fixture.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstdlib>
namespace rt=dawn::server::runtime::activity;
namespace pe=rt::public_event;
namespace mc=rt::mercury;
namespace coo=dawn::state::activity::coo;
namespace msg=dawn::middleware::bap::activity_message;
namespace bits=dawn::middleware::encoding::bits;
unsigned checks{};
void check(bool v,const char* label){++checks;if(!v){std::cerr<<"FAIL "<<label<<'\n';std::exit(1);}}
std::shared_ptr<const coo::script::MissionDocument> parse(const std::string& text){std::string error;auto d=coo::script::MissionDocument::parse(text,mc::kProfile,error);if(!d)std::cerr<<error<<'\n';check(bool(d),"real Mercury document parses");return d;}
void enable(std::string& text,const char* name){check(mission_parameter_fixture::numeric(text,name,1),"fixture parameter is unique numeric value");}
void clear(rt::population::Owner owner){pe::participant_bridge::release(owner);pe::keys::bridge::release(owner);pe::deferred_bridge::release(owner);pe::engagement_bridge::release(owner);rt::adventure::native_bridge::release(owner);pe::native_bridge::release(owner);rt::adventure::dialogue_bridge::release(owner);}
std::uint64_t testMs{};
rt::NativeActivityFrame tick(rt::PersistentActivity& activity,std::uint32_t bubble,bool arrived,
 const rt::adventure_start::wire::Request& selected,bool admitted) {
 rt::activity_clock::Publication clock{{activity.population().owner(),activity.population().boot(),1,mc::kRegistries[0].scenario,15},{},0};
 check(rt::activity_clock::wire::from_milliseconds(testMs,clock.elapsedTicks),"test native clock converts");
 return activity.update(bubble,arrived,selected,admitted,clock,{0x123456789ABCDEF0,pe::participant_feedback::identifiers::IdentifierEncoding::integer});
}
int main(int argc,char** argv){
 check(argc==3,"usage Mercury-json output-directory");std::ifstream file(argv[1],std::ios::binary);check(bool(file),"source json opens");std::string text((std::istreambuf_iterator<char>(file)),{});
 check(mission_parameter_fixture::numeric(text,"public_event_rally_probe",0),"fixture disables rally independently of saved settings");
 check(mission_parameter_fixture::numeric(text,"ambient_vex_probe_count",0),"fixture disables ambient probe independently of saved settings");
 auto defaults=parse(text);check(rt::PersistentActivity::valid(mc::kActivity,*defaults),"all retained profile capabilities validate");
 for(const auto& registry:mc::public_events::kRegistries) {
  check(coo::mercury::public_events::required(registry.scenario,registry.objectTag,registry.key,1ULL<<registry.bubble),"every optional event registry is eligible for runtime extraction");
  check(!coo::mercury::public_events::required(registry.scenario,registry.objectTag,registry.key,1ULL<<14),"foreign explicit geometry scope cannot enter the catalog");
 }
 rt::ambient_population::RegistryBatch optional{};check(rt::ambient_population::optional_registries(mc::kActivity,*defaults,optional) && optional.count==0,"defaults admit no optional Crossroads/Cabal group");
 rt::PersistentActivity normal;const rt::population::Owner normalOwner{800,{1}};check(normal.begin(normalOwner,mc::kActivity,defaults,77),"default activity starts");
 rt::adventure_start::wire::Request selected{};selected.revision=7;selected.selection.activityIndex=29;selected.selection.sourceActivityIndex=29;
 auto normalFrame=normal.update(15,true,selected,true);check(normalFrame.engagements.count==0 && normalFrame.cues.count==0 && normalFrame.placements.count==9,"default native frame remains nine existing placements");
 check(normal.request_population({normalOwner,normal.population().revision(),normal.population().last_request()+1,0xC8229B2B,44,1,77},15)==rt::population::Result::unsupported,"disabled event source cannot bypass prerequisite owner");
 enable(text,"public_event_opening_probe");enable(text,"public_event_rally_probe");enable(text,"ambient_cabal_primary_probe_count");
 auto document=parse(text);check(rt::PersistentActivity::valid(mc::kActivity,*document),"combined rally/Cabal/initial profile validates");
 check(rt::ambient_population::optional_registries(mc::kActivity,*document,optional) && optional.count==3,"exact optional Cabal owner, event and geometry registries");
 std::ofstream live(std::filesystem::path(argv[2])/"mercury-initial-test.json",std::ios::binary);live<<text;live.close();check(live.good(),"reviewable enabled JSON emitted");
 const rt::population::Owner owner{801,{2}};constexpr std::uint64_t boot=78;
 rt::PersistentActivity activity;check(activity.begin(owner,mc::kActivity,document,boot),"retained combined activity starts");
 check(tick(activity,15,false,selected,true).placements.count==0,"no arrival no placement");
 check(tick(activity,14,true,selected,true).engagements.count==0,"wrong bubble cannot publish engagement");
 auto frame=tick(activity,15,true,selected,false);check((activity.public_initial().state()&30)==0,"warmup cannot start native gate requests");
 frame=tick(activity,15,true,selected,true);check(frame.placements.count==10 && frame.engagements.count==0 && frame.cues.count==0,"arrived patrol waits for rally; no event gates or objective");
 const auto rally=pe::native_bridge::lookup_definition(mc::public_events::kRallyFlag.definition);check(rally.epoch!=0,"rally source lease exists");
 const auto& r=rally.ticket;
 check(pe::native_bridge::submit({rally,{{r.lease,pe::Stage::rally,r.asset,{r.token,coo::Milestone::nativeReady}},
    {0x11223344,0x600},0x10203040,0,15,1}}),"qualified rally placement fixture submitted");
 frame=tick(activity,15,true,selected,true);check(frame.placements.count==10 && frame.cues.count==0,"native flag readiness alone does not start");
 const auto use=pe::native_bridge::lookup_use(mc::public_events::kRallyInteraction,0x10203040);
 check(pe::native_bridge::submit_use({use,0x20123456,0x30123456,0x40123456,1}),"qualified completed-use fixture submitted");
 frame=tick(activity,15,true,selected,true);
 check(frame.placements.count==10 && frame.sequences.count==0 && frame.dialogues.count==0,"rally arms intro without early native effects");
 testMs=1750;frame=tick(activity,15,true,selected,true);
 check(frame.eventParticipants.count==1 && frame.engagements.count==1 && frame.cues.count==0,"incoming participant and empty collection precede manager entry");
 const auto participant=pe::participant_bridge::lookup(0x80F5E33D);
 const auto empty=pe::engagement_bridge::lookup(0x80F5E466);
 check(participant.epoch && empty.epoch && empty.ticket.request.collection==pe::engagement_feedback::wire::Collection::none,"incoming has qualified identity and an explicit empty collection");
 check(pe::participant_bridge::submit({participant,{participant.ticket,{0x100011,0x1000},2}}),"qualified type71 apply fixture");
 frame=tick(activity,15,true,selected,true);check(frame.cues.count==0,"participant alone cannot insert incoming");
 check(pe::engagement_bridge::submit({empty,{empty.ticket,{0x100011,0x2000},3}}),"qualified empty type70 apply fixture");
 frame=tick(activity,15,true,selected,true);check(frame.cues.count==1,"both original applies admit incoming");
 const auto cue=rt::adventure::native_bridge::lookup(0x80F5E337);
 check(cue.epoch && cue.ticket.incoming && cue.ticket.request.publicEvent==msg::native::cue::Reference{0xC8229B2B,71,110},"incoming manager ticket references exact participant authority");
 check(rt::adventure::native_bridge::submit({cue,{cue.ticket,{0x100011,0x3000},4,0,msg::native::cue::manager_id(cue.ticket.request.event)}}),"qualified mode0 incoming receipt fixture");
 testMs=5250;frame=tick(activity,15,true,selected,true);
 check(frame.sequences.count==1 && frame.placements.count==10 && frame.dialogues.count==0,"intro effect precedes voice and world publication");
 testMs=9750;frame=tick(activity,15,true,selected,true);
 check(frame.placements.count==21 && frame.engagements.count==1 && frame.cues.count==1,"world start retains incoming and requests shells, central physics and four gates");
 check(frame.music.count==1 && frame.music.entries[0].registry==0xC8229B2B
  && frame.music.entries[0].slot==109 && frame.music.entries[0].active[0]==1,"world opening publishes the authored first native music candidate");
 check(frame.dialogues.count==1 && frame.dialogues.entries[0].registry==0xC8229B2B
     && frame.dialogues.entries[0].slot==108 && frame.dialogues.entries[0].activeRow==0
     && frame.dialogues.entries[0].scope==15,"completed rally submits scoped native incoming conversation");
 check(activity.placements().count==14,"read-only retained placement projection includes gates");
 check(activity.population().project(15).count==2,"gate publication requests no traveling source");
 const auto& expected=mc::public_events::kInitialGates;
 for(std::size_t i=0;i<expected.size();++i){
  const auto state=pe::deferred_bridge::lookup(expected[i].definition);check(state.binding.epoch && state.binding.ticket.owner==owner,"gate ticket installed before projection");
  const auto& t=state.binding.ticket;const auto* p=rt::placement::wire::find(frame.placements,t.registry,4,t.slot);check(p && p->generation==1 && rt::placement::wire::body_bits(*p)==252,"exact positive generation authored transform placement");
  pe::deferred_placement::Observation created{t,{0x100011,17,0x1000},10+i,((2ULL+i)<<32)|1,static_cast<std::uint32_t>(2+i)};
  check(pe::deferred_bridge::created(state.binding,created),"qualified native creation fixture accepted");
  frame=tick(activity,15,true,selected,true);check(activity.population().project(15).count==2,"creation without attached point cannot admit combat");
  check(pe::deferred_bridge::point_ready(state.binding,created,30+i,((3ULL+i)<<32)|1),"qualified attached interface fixture accepted");
  frame=tick(activity,15,true,selected,true);if(i+1<expected.size())check(activity.population().project(15).count==2,"missing one gate blocks combat");
 }
 check(frame.engagements.count==1 && frame.cues.count==1,"world retains original incoming cue during joining");
 const auto engagement=pe::engagement_bridge::lookup(0x80F5E466);check(engagement.epoch!=0,"type70 ticket precedes authority publication");
 const auto& e=engagement.ticket;check(frame.engagements.entries[0]==e.request && msg::native::engagement::body_bits(e.request)==32,"exact active-player schema command");
 check(e.request.generation==2,"join advances empty collection exactly once");
 check(activity.public_initial().observe(0xC8229B2B,106,15,0x808094F0,{1,2,1,true}),"real joined participant fixture accepted before apply");
 frame=tick(activity,15,true,selected,true);check(frame.cues.count==1 && frame.cues.entries[0].timer.remaining==cue.ticket.request.timer.remaining,"sense alone cannot reset incoming timer");
 check(!activity.public_initial().observe(0xC8229B2B,106,14,0x808094F0,{2,2,1,true}),"wrong bubble sense rejected");
 check(pe::engagement_bridge::submit({engagement,{e,{0x100011,0x1000},40}}),"qualified native type70 apply fixture accepted");
 frame=tick(activity,15,true,selected,true);check(frame.cues.count==1,"joined state retains a single objective");
 check(frame.cues.entries[0].event==0x00D1C5B9 && frame.cues.entries[0].readiness==msg::native::cue::Reference{0xC8229B2B,70,106},"authored traveling objective targets actual engagement");
 check(!rt::adventure::native_bridge::lookup(0x80F5E337).epoch,"consumed incoming receipt reused without new manager insertion");
 frame=tick(activity,15,true,selected,true);auto populations=activity.population().project(15);check(populations.count==4,"only accepted native objective requests both traveling sources");
 for(const auto& source:mc::public_events::kOpeningSources){
  const auto* p=msg::native::population::find(populations,source.registry->key,1,source.slot);
  check(p && p->source.looseRequested==9,"finite cumulative request budget per authored source");
  check(p && p->source.tactical.registry==0xC8229B2B && p->source.tactical.slot==82
      && p->source.tactical.row==4,"traveling cohort projects its native central tactical assignment");
 }
 check((activity.public_initial().state()&16384)==0,"publication is not actor readiness");
 for(std::size_t i=0;i<2;++i){const auto asset=mc::public_events::kOpeningSourceCommands[i].asset;coo::PopulationOwner source{owner.sessionId,boot,owner.incarnation.value,asset,2};
  pe::opening_native::Event event{{owner,source,15},{source,static_cast<std::uint32_t>(100+i),static_cast<std::uint32_t>(200+i)},300,pe::opening_native::Kind::admitted};
  check(!activity.public_initial().observe_accepted(event,coo::PopulationIntake::duplicate),"duplicate central intake cannot advance event");
  check(activity.public_initial().observe_accepted(event,coo::PopulationIntake::accepted),"only central-ledger accepted admission advances source");
 }
 frame=tick(activity,15,true,selected,true);check((activity.public_initial().state()&16384)!=0,"finite initial diagnostic ready after both real source admissions");
 check(frame.cues.entries[0].hasTimer && frame.cues.entries[0].timer.advancing,"opening carries a native ticking phase clock");
 const auto openingTimer=frame.cues.entries[0].timer;
 testMs+=1000;
 // Crossroads advances from actual scoped deaths, and the new directive must
 // be accepted natively before either Gatekeeper source can publish.
 check(activity.request_population({owner,activity.population().revision(),activity.population().last_request()+1,
     0xC8229B2B,48,1,boot},15)==rt::population::Result::unsupported,"manual source route cannot bypass Gatekeeper prerequisite");
 for(unsigned i=0;i<9;++i) {
  const auto asset=mc::public_events::kOpeningSourceCommands[i%2].asset;
  const coo::PopulationOwner source{owner.sessionId,boot,owner.incarnation.value,asset,2};
  pe::opening_native::Event event{{owner,source,15},{source,static_cast<std::uint32_t>(100+i),static_cast<std::uint32_t>(200+i)},300,pe::opening_native::Kind::admitted};
  if(i>=2)check(activity.public_initial().observe_accepted(event,coo::PopulationIntake::accepted),"later traveling admission");
  event.kind=pe::opening_native::Kind::died;
  check(activity.public_initial().observe_accepted(event,coo::PopulationIntake::accepted),"qualified traveling death");
  frame=tick(activity,15,true,selected,true);
  check(frame.cues.count==1,"one current directive throughout the transition");
  if(i<8)check(frame.cues.entries[0].event==0x00D1C5B9 && frame.cues.entries[0].progress.current==static_cast<int>(i+1),"per-kill progress published in the same authority turn");
 }
 check(frame.cues.entries[0].hasTimer && frame.cues.entries[0].timer.anchor==openingTimer.anchor
     && frame.cues.entries[0].timer.remaining==openingTimer.remaining,"Gatekeeper objective retains opening deadline");
 check(frame.cues.entries[0].event==0x00D1C5BA && frame.cues.entries[0].ring==1
     && frame.cues.entries[0].progress.current==0 && frame.cues.entries[0].progress.target==4,"quota publishes authored key objective at zero of four");
 check(activity.population().project(15).count==4,"new objective request alone cannot spawn Gatekeepers");
 const auto nextCue=rt::adventure::native_bridge::lookup(0x80F5E337);
 check(nextCue.epoch>cue.epoch && nextCue.ticket.request.event==0x00D1C5BA,"next cue has a new exact binding epoch");
 check(!rt::adventure::native_bridge::submit({cue,{cue.ticket,{0x100011,0x1000},42,0,msg::native::cue::manager_id(cue.ticket.request.event)}}),"old cue cannot acknowledge the new stage");
 check(rt::adventure::native_bridge::submit({nextCue,{nextCue.ticket,{0x100011,0x1000},43,1,msg::native::cue::manager_id(nextCue.ticket.request.event)}}),"qualified next native objective receipt");
 frame=tick(activity,15,true,selected,true);populations=activity.population().project(15);
 check(populations.count==6,"Gatekeeper sources publish only after new native objective receipt");
 for(std::size_t i=0;i<2;++i) {
  const auto& source=mc::public_events::kGatekeeperSources[i];
  const auto* p=msg::native::population::find(populations,source.registry->key,1,source.slot);
  check(p && p->source.looseRequested==1 && p->source.tactical.row==static_cast<int>(2+i),"native Gatekeeper source receives its side-specific tactical provider");
  const coo::PopulationOwner lease{owner.sessionId,boot,owner.incarnation.value,mc::public_events::kGatekeeperSourceCommands[i].asset,2};
  pe::opening_native::Event event{{owner,lease,15},{lease,static_cast<std::uint32_t>(500+i),static_cast<std::uint32_t>(600+i)},static_cast<std::uint32_t>(700+i),pe::opening_native::Kind::admitted};
  check(activity.public_initial().observe_accepted(event,coo::PopulationIntake::accepted),"Gatekeeper admission tracked by its own source ledger");
  event.kind=pe::opening_native::Kind::died;
  check(activity.public_initial().observe_accepted(event,coo::PopulationIntake::accepted),"Gatekeeper death tracked without pretending to deposit a key");
 }
 frame=tick(activity,15,true,selected,true);
 check((activity.public_initial().state()&2097152)!=0 && frame.cues.entries[0].progress.current==0,"Gatekeeper readiness/death does not increment gate counter");
 for(std::size_t i=0;i<4;++i) {
  const auto& item=mc::public_events::kMainlandKeyDefinition.items[i];
  auto key=pe::keys::bridge::lookup(item.ticket.key.definition);check(key.epoch!=0,"each paired key has a distinct live event lease");
  for(const auto& t:{key.ticket.key,key.ticket.sink}) {
   const auto binding=pe::deferred_bridge::lookup(t.definition).binding;
   const auto entity=static_cast<std::uint32_t>(0x2000+i*2+(t==key.ticket.sink?1:0));
   pe::deferred_placement::Observation created{t,{static_cast<std::uint32_t>(0x3000+i),static_cast<std::uint32_t>(0x1000+i),0x1000},100+i,(std::uint64_t(entity)<<32)|1,entity};
   check(binding.epoch && pe::deferred_bridge::created(binding,created) && pe::keys::bridge::created(created),"native created receipt links full key/sink entities");
  }
  key=pe::keys::bridge::lookup(item.ticket.key.definition);
  check(pe::keys::bridge::carry(key,static_cast<std::uint32_t>(0x4000+i),0x5000,true),"native held receipt links exact player to charge");
  key=pe::keys::bridge::lookup(item.ticket.key.definition);
  const pe::keys::Use deposit{key,key,static_cast<std::uint32_t>(0x6000+i),0x7000,0x5000,1,0};
  check(pe::keys::bridge::deposit(deposit) && !pe::keys::bridge::deposit(deposit),"accepted native use completes a key only once");
  frame=tick(activity,15,true,selected,true);
  if(i<3)check(frame.cues.entries[0].progress.current==static_cast<int>(i+1) && frame.cues.entries[0].event==0x00D1C5BA,"three uses cannot trigger cannon travel");
 }
 check(frame.cues.entries[0].event==0x0730D92A && frame.cues.entries[0].timer.remaining<openingTimer.remaining,"four native deposits begin travel with a distinct phase deadline");
 const auto travelCue=rt::adventure::native_bridge::lookup(0x80F5E337);
 check(travelCue.epoch && rt::adventure::native_bridge::submit({travelCue,{travelCue.ticket,{0x100011,0x1000},110,2,msg::native::cue::manager_id(travelCue.ticket.request.event)}}),"authored travel manager receipt");
 frame=tick(activity,15,true,selected,true);
 const auto* central=rt::placement::wire::find(frame.placements,0xC8229B2B,4,74);
 check(central && !central->active && !rt::placement::wire::find(frame.placements,0xC8229B2B,4,70),"central launch physics off before island launch enabled");
 const auto transitMs=testMs;
 testMs=transitMs+5000;frame=tick(activity,15,true,selected,true);frame=tick(activity,15,true,selected,true);
 const auto* shell=rt::world_device::wire::find(frame.devices,0xB1EAC5B6,23,1);
 check(shell && shell->state.position.value==1 && shell->state.power.value==0,"authored shell rotates while its field is off");
 testMs=transitMs+7000;frame=tick(activity,15,true,selected,true);frame=tick(activity,15,true,selected,true);
 check(rt::placement::wire::find(frame.placements,0xC8229B2B,4,70) && rt::placement::wire::find(frame.placements,0xC8229B2B,4,78),"native island launch routes and white ring sources retained together");
 const auto retainedPlacementCount=frame.placements.count;
 selected.revision++;frame=tick(activity,15,true,selected,true);check((activity.public_initial().state()&32768)!=0 && frame.engagements.count==1 && frame.cues.count==1 && frame.placements.count==retainedPlacementCount,"changed selection halts progression but retains adopted world authority");
 check(frame.music.count==1 && frame.music.entries[0].active[0]==1,"selection conflict retains adopted music authority without restarting it");
 clear(owner);clear(normalOwner);check(!pe::deferred_bridge::lookup(0x80F5E481).binding.epoch,"retirement clears gate lease");
 std::cout<<"PASS "<<checks<<" public initial checks\n";
}
