#include "server/runtime/activity/mercury_public_event_keys.h"
#include "server/runtime/activity/public_event_clock.h"
#include "middleware/encoding/bit_reader.h"
#include <iostream>
#include <cstdlib>
#include <limits>
namespace rt=dawn::server::runtime::activity;
namespace keys=rt::public_event::keys;
namespace deferred=rt::public_event::deferred_placement;
namespace mc=rt::mercury::public_events;
unsigned checks{};
void check(bool v,const char* label){++checks;if(!v){std::cerr<<"FAIL "<<label<<'\n';std::exit(1);}}
template<class T> void put(std::span<std::byte> b,std::size_t offset,T value){std::memcpy(b.data()+offset,&value,sizeof value);}
int main(){
 check(keys::Runtime::valid(mc::kMainlandKeyDefinition),"all native source/target capabilities validate");
 keys::Mailbox box;
 auto t=mc::kMainlandKeys[0].ticket;
 for(auto* p:{&t.key,&t.sink}){p->owner={8,{4}};p->boot=11;p->definitionRevision=12;p->selectionRevision=13;p->event=14;}
 check(box.bind(t) && box.bind(t),"idempotent exact lease binding");
 auto other=t;other.key.owner.incarnation.value++;other.sink.owner=other.key.owner;
 check(!box.bind(other),"other incarnation cannot replace source before retirement");
 auto key=box.lookup(t.key.definition);check(key.epoch && key.keyEntity==UINT32_MAX,"bound is not created");
 const deferred::Source source{0x10001,1,0x40};
 check(box.created({t.key,source,1,0x1000400100000001ULL,0x10004001}),"qualified exact source creation accepted");
 check(!box.created({t.key,source,2,0x1000400100000001ULL,0x10004001}),"duplicate native creation rejected");
 check(box.created({t.sink,source,3,0x1000400200000001ULL,0x10004002}),"qualified sink creation accepted");
 key=box.carrier(t.carrierDefinition,0x10004001);
 check(!box.carry(key,0x200001,UINT32_MAX,true),"unresolved holder cannot become local player");
 check(box.carry(key,0x200001,0x300001,true),"real holder accepted");
 key=box.sink(t.interactionDefinition,0x10004002);
 std::array<std::byte,keys::kUseBytes> before{};
 put(before,0,t.interactionDefinition);put(before,4,0x80804FB2U);put(before,8,std::int64_t{0x388});
 put(before,0x24,0x200002U);put(before,0x2C,key.sinkEntity);put(before,0x2DC,std::int32_t{1});put(before,0x2E0,std::uint64_t{0x777777777ULL});
 keys::Use use{};
 check(!keys::before_use(key,before,0x400001,0x300002,use),"different requester entity cannot deposit held charge");
 check(keys::before_use(key,before,0x400001,0x300001,use),"pending native hold-E use captured");
 auto after=before;
 check(!keys::after_use(use,before,after),"prompt/pending request alone is not deposit");
 after[0x2D0]=std::byte{1};put(after,0x2D8,std::int32_t{1});
 check(keys::after_use(use,before,after),"original use consumed exact request");
 auto malformed=after;put(malformed,0x24,0x200003U);
 check(!keys::after_use(use,before,malformed),"component replacement rejects consumption");
 malformed=after;put(malformed,0x2D8,std::int32_t{2});
 check(!keys::after_use(use,before,malformed),"wrong consumed counter rejected");
 malformed=after;put(malformed,0x2E0,std::uint64_t{8});
 check(!keys::after_use(use,before,malformed),"changed requester context rejected");
 check(box.deposit(use),"qualified original use closes key exactly once");
 check(!box.deposit(use) && !box.lookup(t.key.definition).held,"duplicate cannot advance counter");
 box.release(t.key.owner);check(!box.deposit(use) && box.bind(other),"retirement rejects late callback and permits new incarnation");
 // A native sink can accept a charge other than the source listed beside it.
 // Every ordering must consume one current key and one fresh sink, separately.
 std::array<unsigned,4> sinkOrder{0,1,2,3};
 do {
  keys::Mailbox ordered;
  std::array<keys::Ticket,4> tickets{};
  for(unsigned i=0;i<4;++i) {
   auto& item=tickets[i];item=mc::kMainlandKeys[i].ticket;
   for(auto* p:{&item.key,&item.sink}){p->owner={8,{4}};p->boot=11;p->definitionRevision=12;p->selectionRevision=13;p->event=14;}
   check(ordered.bind(item),"bind each native charge and sink source");
   const auto itemEntity=0x10004010U+i,sinkEntity=0x10004020U+i;
   check(ordered.created({item.key,source,1,(std::uint64_t{itemEntity}<<32)|1,itemEntity})
    && ordered.created({item.sink,source,2,(std::uint64_t{sinkEntity}<<32)|1,sinkEntity}),"create source identities");
  }
  for(unsigned i=0;i<4;++i) {
   auto carried=ordered.lookup(tickets[i].key.definition);
   check(ordered.carry(carried,0x200010U+i,0x300001,true),"native carry selects this item");
   auto destination=ordered.sink(tickets[sinkOrder[i]].interactionDefinition,0x10004020U+sinkOrder[i]);
   carried=ordered.held(destination,0x300001);
   check(carried.keyEntity==0x10004010U+i,"held key resolved independently of destination source order");
   std::array<std::byte,keys::kUseBytes> request{};
   put(request,0,destination.ticket.interactionDefinition);put(request,4,0x80804FB2U);put(request,8,std::int64_t{0x388});
   put(request,0x24,0x200020U+sinkOrder[i]);put(request,0x2C,destination.sinkEntity);put(request,0x2DC,std::int32_t{1});
   keys::Use actual{};
   auto foreign=destination;foreign.ticket.key.owner.incarnation.value++;
   check(!keys::before_use(carried,foreign,request,0x400001,0x300001,actual),"foreign encounter sink cannot consume item");
   check(keys::before_use(carried,destination,request,0x400001,0x300001,actual),"capture actual native sink request");
   auto consumed=request;consumed[0x2D0]=std::byte{1};put(consumed,0x2D8,std::int32_t{1});
   check(keys::after_use(actual,request,consumed) && ordered.deposit(actual),"native consumption closes actual key and actual sink");
   check(!ordered.deposit(actual) && !ordered.sink(destination.ticket.interactionDefinition,destination.sinkEntity).epoch,"sink and receipt cannot be reused");
   unsigned completed{};for(const auto& item:tickets)if(ordered.lookup(item.key.definition).sinkDeposited)++completed;
   check(completed==i+1,"progress counts each consumed sink exactly once");
  }
 } while(std::next_permutation(sinkOrder.begin(),sinkOrder.end()));
 // Native deadline is independent of display/objective revisions.
 rt::public_event::Clock clock;
 rt::activity_clock::Publication c{{{8,{4}},11,1,0xABCDEF,15},{},100};
 check(clock.observe(c) && clock.start_phase(1,240000),"native event phase starts");
 dawn::middleware::bap::activity_message::native::cue::Request cue{};
 check(clock.project(cue),"deadline projects into native cue");const auto timer=cue.timer;
 c.elapsedTicks+=673200;check(clock.observe(c) && clock.start_phase(1,240000) && clock.project(cue)
  && cue.timer.anchor==timer.anchor && cue.timer.remaining==timer.remaining,"retry never extends deadline");
 auto stale=c;stale.domain.owner.incarnation.value++;check(!clock.observe(stale),"foreign world clock rejected");
 stale=c;stale.elapsedTicks--;check(!clock.observe(stale),"backwards clock rejected");
 check(clock.start_phase(2,120000) && !clock.start_phase(1,240000),"later phase may set new deadline without backwards transition");
 using namespace dawn::middleware::bap::activity_message::native;
 placement::Request p{0xC8229B2B,34,15};p.generation=1;
 check(placement::body_bits(p)==252,"existing authored wire unchanged");
 p.position=placement::Position{1,2,3};check(placement::body_bits(p)==252,"native XYZ override uses existing fixed wire fields");
 p.generation=0;check(!placement::body_bits(p),"legacy tuple cannot silently adopt XYZ semantics");
 p.generation=1;p.position->x=std::numeric_limits<float>::infinity();check(!placement::body_bits(p),"infinite world position rejected");
 p.position->x=std::numeric_limits<float>::quiet_NaN();check(!placement::body_bits(p),"NaN world position rejected");
 std::cout<<"PASS "<<checks<<" public key/clock/transform checks\n";
}
