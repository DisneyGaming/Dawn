#include "server/runtime/activity/mercury_definition.h"
#include "middleware/encoding/bit_writer.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstdlib>
namespace rt=sunrise::server::runtime::activity;namespace pe=rt::public_event;namespace mc=rt::mercury;
namespace coo=sunrise::state::activity::coo;namespace msg=sunrise::middleware::bap::activity_message;
unsigned checks{};void check(bool v,const char* s){++checks;if(!v){std::cerr<<"FAIL "<<s<<'\n';std::exit(1);}}
struct Frame{rt::placement::wire::Batch placements{};pe::opening_cue::wire::Batch cues{};pe::engagement_feedback::wire::Batch engagements{};};
std::vector<std::byte> bytes(const Frame& f){
 std::vector<std::byte> out(4096);sunrise::middleware::encoding::bits::Writer w(out);
 for(std::size_t i=0;i<f.placements.count;++i){const auto& r=f.placements.entries[i];check(w.write(r.registry,32)&&w.write(r.slot,16)&&w.write(r.bubble,32)&&rt::placement::wire::write(w,r),"retained placement encodes");}
 for(std::size_t i=0;i<f.engagements.count;++i){const auto& r=f.engagements.entries[i];check(w.write(r.registry,32)&&w.write(r.slot,16)&&w.write(r.scope,32)&&pe::engagement_feedback::wire::write(w,r),"retained engagement encodes");}
 for(std::size_t i=0;i<f.cues.count;++i){const auto& r=f.cues.entries[i];check(w.write(r.registry,32)&&w.write(r.slot,16)&&w.write(r.scope,32)&&pe::opening_cue::wire::write(w,r),"retained cue encodes");}
 out.resize((w.bit_count()+7)/8);return out;
}
struct Harness {
 // Exercise the reusable gate/engagement lifetime contract independently of
 // the presentation extension. Full incoming/key/cannon flow is covered by
 // public_event_initial_tests with actual receipt ordering.
 std::array<pe::InitialDefinition,1> initials{mc::public_events::kInitialDefinitions};
 rt::NativeActivityDefinition profile=mc::kActivity;
 rt::population::Owner owner;rt::population::Service population;pe::InitialRuntime initial;rt::adventure_start::wire::Request selected{};std::uint64_t boot=78;
 Harness(std::uint64_t id,const coo::script::MissionDocument& document,std::span<const rt::population::Capability> caps=mc::kPopulations):owner{id,{2}}{
  initials[0].intro=nullptr;initials[0].world=nullptr;initials[0].keys=nullptr;initials[0].travel=nullptr;
  initials[0].participant={};initials[0].joinedVoice=nullptr;initials[0].incoming=nullptr;initials[0].musicDefinition=nullptr;profile.publicEventInitials=initials;
  check(population.begin(owner,caps,boot),"actual population service begins");check(initial.begin(owner,boot,profile,document),"base PE prerequisite profile begins");selected.revision=7;selected.selection.activityIndex=selected.selection.sourceActivityIndex=29;
 }
 ~Harness(){pe::deferred_bridge::release(owner);pe::engagement_bridge::release(owner);rt::adventure::native_bridge::release(owner);rt::adventure::dialogue_bridge::release(owner);}
 Frame step(std::uint32_t bubble=15,bool arrived=true,bool admitted=true){Frame f{};check(initial.update(bubble,arrived,admitted,selected,population,f.placements,f.cues,f.engagements,true),"retained projection succeeds");return f;}
 void gates(){for(const auto& expected:mc::public_events::kInitialGates){const auto s=pe::deferred_bridge::lookup(expected.definition);check(s.binding.epoch && s.binding.ticket.owner==owner,"exact gate binding");check(!s.created && !s.ready && !s.creation.sequence,"fresh owner inherits no prior native receipt");const auto& t=s.binding.ticket;pe::deferred_placement::Observation c{t,{0x100011,17,0},10ULL+t.slot,(std::uint64_t{t.slot}<<32)|1,t.slot};const bool created=pe::deferred_bridge::created(s.binding,c);const bool ready=created && pe::deferred_bridge::point_ready(s.binding,c,100ULL+t.slot,(std::uint64_t{t.slot+1U}<<32)|1);if(!ready)std::cerr<<"owner="<<owner.sessionId<<" slot="<<t.slot<<" created="<<created<<" prior="<<s.created<<","<<s.ready<<" seq="<<c.sequence<<" child="<<c.child<<" weak="<<c.weakChild<<'\n';check(ready,"qualified gate/interface fixtures submitted");}}
 void engagement(){auto b=pe::engagement_bridge::lookup(0x80F5E466);check(b.epoch && b.ticket.owner==owner,"exact engagement binding");check(initial.observe(0xC8229B2B,106,15,0x808094F0,{1,1,1,true}),"qualified native participants");check(pe::engagement_bridge::submit({b,{b.ticket,{0x100011,0},200}}),"qualified native engagement apply");}
 void cue(){auto b=rt::adventure::native_bridge::lookup(0x80F5E337);check(b.epoch && b.ticket.owner==owner,"exact cue binding");check(rt::adventure::native_bridge::submit({b,{b.ticket,{0x100011,0},201,0,pe::opening_cue::wire::manager_id(b.ticket.request.event)}}),"qualified cue fixture submitted");}
};
int main(int argc,char** argv){
 check(argc==2,"usage Mercury-default-json");std::ifstream in(argv[1],std::ios::binary);check(bool(in),"real JSON opens");std::string text((std::istreambuf_iterator<char>(in)),{}),error;
 auto disabled=coo::script::MissionDocument::parse(text,mc::kProfile,error);check(bool(disabled),"default profile parses");
 const auto token=std::string("\"public_event_opening_probe\": 0");const auto at=text.find(token);check(at!=text.npos,"probe stays default off");text[at+token.size()-1]='1';
 auto doc=coo::script::MissionDocument::parse(text,mc::kProfile,error);check(bool(doc),"enabled probe parses");
 {Harness h(900,*disabled);check(bytes(h.step()).empty()&&bytes(h.step(16)).empty(),"disabled event stays empty across regions");}
 {Harness h(901,*doc);check(bytes(h.step(16)).empty()&&bytes(h.step(15,false)).empty()&&bytes(h.step(15,true,false)).empty(),"unstarted event cannot begin outside exact admission");h.selected.selection.activityIndex=1078;h.selected.revision++;check(bytes(h.step()).empty(),"selected Adventure cannot begin patrol event");}
 {Harness h(902,*doc);auto gate=h.step();check(gate.placements.count==4 && gate.engagements.count==0,"initial gates only");const auto gateBytes=bytes(gate);h.gates();const auto state=h.initial.state();
  for(unsigned b:{16U,UINT32_MAX})check(bytes(h.step(b))==gateBytes && h.initial.state()==state,"120 to128 and unresolved region retain gates without consuming readiness");
  check(bytes(h.step(15,false))==gateBytes&&bytes(h.step(15,true,false))==gateBytes,"arrival/admission suspension retains gate bytes");
  auto engaged=h.step();check(engaged.engagements.count==1&&engaged.cues.count==0,"return consumes gate receipts and requests engagement");const auto engagedBytes=bytes(engaged);h.engagement();const auto engagedState=h.initial.state();
  check(bytes(h.step(16))==engagedBytes&&h.initial.state()==engagedState,"region departure retains engagement and defers queued apply");
  auto cue=h.step();check(cue.cues.count==1&&h.population.project(15).count==0,"return publishes objective before sources");const auto cueBytes=bytes(cue);h.cue();
  check(bytes(h.step(16))==cueBytes&&h.population.project(15).count==0,"queued cue does not start sources in other region");
  auto active=h.step();check(bytes(active)==cueBytes&&h.population.project(15).count==2,"eligible return admits finite sources");const auto last=h.population.last_request();
  for(unsigned b:{16U,15U,UINT32_MAX,15U})check(bytes(h.step(b))==cueBytes&&h.population.last_request()==last,"region roundtrips retain bodies without renewing sources");
  Frame rejected{};rt::population::Service foreign;check(foreign.begin({903,{2}},mc::kPopulations,78),"foreign service fixture");check(!h.initial.update(15,true,true,h.selected,foreign,rejected.placements,rejected.cues,rejected.engagements,true)&&bytes(rejected).empty(),"old owner cannot project into another owner service");
 }
 {Harness h(904,*doc);h.step();h.gates();h.step();h.engagement();auto cue=h.step();h.cue();const auto retained=bytes(cue);h.selected.revision++;h.selected.selection.activityIndex=1078;
  check(bytes(h.step(16))==retained&&(h.initial.state()&32768)&&h.population.last_request()==0,"Adventure selection retains cue/gates/engagement in other region");
  h.selected.revision--;h.selected.selection.activityIndex=29;check(bytes(h.step())==retained&&h.population.last_request()==0,"returning patrol does not revive conflicted event");
 }
 {const std::span<const rt::population::Capability> onlyFirst=std::span(mc::public_events::kOpeningSources).first(1);Harness h(905,*doc,onlyFirst);h.step();h.gates();h.step();h.engagement();auto cue=h.step();h.cue();const auto retained=bytes(cue);
  check(bytes(h.step())==retained&&(h.initial.state()&65536)&&h.population.project(15).count==1,"native service refusal stops graph while retaining adopted bodies and earlier source");const auto last=h.population.last_request();
  check(bytes(h.step(16))==retained&&bytes(h.step())==retained&&h.population.last_request()==last,"failed event remains projected without retry or completion");
 }
 {Harness h(906,*doc);h.step();h.gates();h.step();h.engagement();auto cue=h.step();const auto retained=bytes(cue);Frame full{};full.cues.count=full.cues.entries.size();const auto count=full.cues.count;
  check(!h.initial.update(16,true,true,h.selected,h.population,full.placements,full.cues,full.engagements,true)&&full.placements.count==0&&full.engagements.count==0&&full.cues.count==count,"projection capacity failure changes no caller batch");
  check(bytes(h.step(16))==retained&&(h.initial.state()&65536),"next available frame retains already-adopted authority after projection failure");
 }
 {Harness h(907,*doc);check(bytes(h.step(16)).empty(),"fresh world owner inherits no retired projection");}
 std::cout<<"PASS "<<checks<<" public-event regional lifetime checks\n";
}
