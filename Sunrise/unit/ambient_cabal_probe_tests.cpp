#include "../src/server/runtime/activity/mercury_ambient_cabal_probe.h"
#include "../src/client/hooks/bootflow/ambient_population_named_identity.h"
#include "../src/state/activity/coo/native_population_ledger.h"
#include <cstdio>
#include <cstdlib>

namespace rt=sunrise::server::runtime::activity;
namespace ambient=rt::ambient_population;
namespace points=ambient::named_points;
namespace probe=rt::mercury::ambient::cabal_probe;
namespace ident=sunrise::client::hooks::bootflow::ambient_named_identity;
namespace coo=sunrise::state::activity::coo;
static unsigned checks{};
#define CHECK(x) do {++checks;if(!(x)){std::printf("FAIL %d %s\n",__LINE__,#x);std::exit(1);}}while(false)
template<class T,std::size_t N> void put(std::array<std::byte,N>& bytes,std::size_t at,T value) {
    std::memcpy(bytes.data()+at,&value,sizeof(value));
}
ambient::sense::SenseObject monitor(std::uint32_t revision,bool occupied) {
    ambient::sense::SenseObject result{};result.registryKey=0x2571C34D;result.slotType=30;result.slotIndex=4;
    result.hasNativeSchema=true;result.nativeSchema=0x80809531;result.nativeRevision=revision;
    result.hasRootDelta=true;result.bodyBits=99;
    result.bodyFirst=(1ULL<<63)|(std::uint64_t{occupied}<<62)|((std::uint64_t{0x80000000U}+occupied)<<29)|0x10000000U;
    result.bodySecond=revision;return result;
}
void optional_registry_cases() {
    struct Value {std::uint64_t value{};};
    struct Doc {
        Value value{};bool present{true};
        const Doc& views() const {return *this;}
        const Value* parameter(std::string_view name) const {return present && name==probe::kCountParameter?&value:nullptr;}
    } doc;
    struct Definition {std::string_view activity;std::uint8_t bubble;
        std::span<const rt::registry::Definition> registries;
        std::span<const ambient::RegistryBinding> optionalRegistries;};
    Definition definition{"mercury_freeroam",15,{},probe::kOptionalRegistries};
    ambient::RegistryBatch selected{};
    CHECK(ambient::optional_registries(definition,doc,selected));CHECK(selected.count==0);
    doc.value.value=1;CHECK(ambient::optional_registries(definition,doc,selected));CHECK(selected.count==2);
    CHECK(selected.entries[0]->key==0x4A3E4900 && selected.entries[0]->slots.size()==8);
    CHECK(selected.entries[1]->key==0x2571C34D && selected.entries[1]->slots.size()==7);
    CHECK(probe::kPopulation.registry==selected.entries[1]);
    CHECK(probe::kPopulation.slot==0 && probe::kPopulation.rule==8);
    CHECK(probe::kPopulation.tactical.slot==2 && probe::kPopulation.tactical.row==0);
    CHECK(probe::owner::required(0x80F4696A,0x80F5B9C1,0x4A3E4900,1ULL<<15));
    CHECK(!probe::owner::required(0x80F4696A,0x80F5B9C1,0x4A3E4900,1ULL<<14));
    CHECK(!probe::owner::required(0x80F4696B,0x80F5B9C1,0x4A3E4900,1ULL<<15));
    doc.value.value=2;CHECK(!ambient::optional_registries(definition,doc,selected));CHECK(selected.count==0);
    doc.value.value=1;doc.present=false;CHECK(!ambient::optional_registries(definition,doc,selected));
    doc.present=true;auto duplicate=probe::kOptionalRegistries;duplicate[1]=duplicate[0];
    definition.optionalRegistries=duplicate;CHECK(!ambient::optional_registries(definition,doc,selected));
    definition.optionalRegistries=probe::kOptionalRegistries;definition.registries=probe::owner::kRegistries;
    CHECK(!ambient::optional_registries(definition,doc,selected));
}
void mailbox_cases() {
    const auto ticket=points::ticket({10,{2}},20,probe::kNamedDependency);points::Mailbox mailbox;
    CHECK(points::valid(probe::kNamedDependency));CHECK(mailbox.bind(ticket));CHECK(mailbox.bind(ticket));
    const auto binding=mailbox.lookup(ticket.list).binding;
    CHECK(binding.epoch && !mailbox.lookup(ticket.list).ready());
    auto wrong=ticket;wrong.owner={11,{2}};CHECK(!mailbox.bind(wrong));
    wrong=ticket;wrong.boot=21;CHECK(!mailbox.bind(wrong));
    auto stale=binding;stale.epoch++;CHECK(!mailbox.constructed(stale,probe::kPoints[0],1));
    auto bad=probe::kPoints[0];bad.guid++;CHECK(!mailbox.constructed(binding,bad,1));
    CHECK(!mailbox.constructed(binding,probe::kPoints[0],UINT32_MAX));
    for(std::size_t i=0;i<probe::kPoints.size();++i) {
        const auto& point=probe::kPoints[i];std::array<std::byte,144> placement{};
        put(placement,0,point.entityDefinition);put(placement,0x70,point.guid);
        points::Point parsed{};CHECK(ident::placement(binding,point.index,placement,parsed));CHECK(parsed==point);
        CHECK(!ident::placement(binding,point.index+1,placement,parsed));
        put(placement,0x70,point.guid+1);CHECK(!ident::placement(binding,point.index,placement,parsed));
        std::array<std::byte,0x98> resident{};
        put(resident,0x0C,std::uint32_t{42});
        put(resident,0x88,ticket.list);put(resident,0x8C,point.index);put(resident,0x90,point.guid);
        CHECK(ident::resident(binding,point,42,resident));CHECK(!ident::resident(binding,point,UINT32_MAX,resident));
        CHECK(!ident::resident(binding,point,43,resident));
        put(resident,0x88,ticket.list+1);CHECK(!ident::resident(binding,point,42,resident));
        const auto handle=static_cast<std::uint32_t>(100+i);
        CHECK(mailbox.constructed(binding,point,handle));CHECK(mailbox.constructed(binding,point,handle));
        CHECK(!mailbox.constructed(binding,point,handle+1));
        CHECK(mailbox.lookup(ticket.list).ready()==(i==2));
    }
    CHECK(mailbox.interface_resolved(binding,probe::kPoints[0].guid));
    CHECK(!mailbox.interface_resolved(binding,probe::kPoints[0].guid));
    CHECK(mailbox.removing(binding));CHECK(!mailbox.lookup(ticket.list).ready());
    CHECK(!mailbox.constructed(binding,probe::kPoints[0],100));
    const auto fresh=mailbox.lookup(ticket.list).binding;
    CHECK(fresh.epoch!=binding.epoch);CHECK(mailbox.constructed(fresh,probe::kPoints[0],200));
    mailbox.release(ticket.owner);CHECK(!mailbox.lookup(ticket.list).binding.epoch);
    CHECK(mailbox.retirement(ticket.list).handles[0]==200); // retain cleanup identity only
    CHECK(!mailbox.constructed(fresh,probe::kPoints[1],201));
    wrong=ticket;wrong.owner={10,{3}};CHECK(mailbox.bind(wrong));
    CHECK(!mailbox.lookup(ticket.list).ready());CHECK(!mailbox.constructed(fresh,probe::kPoints[0],200));
    const auto reentered=mailbox.lookup(ticket.list).binding;
    for(std::size_t i=0;i<3;++i)CHECK(mailbox.constructed(reentered,probe::kPoints[i],static_cast<std::uint32_t>(300+i)));
    CHECK(mailbox.removing(reentered));mailbox.release(wrong.owner);
    for(std::size_t i=0;i<3;++i) {
        const auto cleanup=mailbox.retirement(ticket.list);
        CHECK(!cleanup.active && !cleanup.ready() && cleanup.observed==7);
        CHECK(!mailbox.actor_retired(cleanup.binding,probe::kPoints[i],static_cast<std::uint32_t>(900+i)));
        CHECK(mailbox.actor_retired(cleanup.binding,probe::kPoints[i],static_cast<std::uint32_t>(300+i)));
        CHECK(!mailbox.constructed(cleanup.binding,probe::kPoints[i],static_cast<std::uint32_t>(300+i)));
    }
    CHECK(mailbox.retirement(ticket.list).retired==7);
}
void finite_lifecycle_cases() {
    const rt::population::Owner owner{0xABCD,{9}};constexpr std::uint64_t boot=0x123456;
    const std::array capabilities{probe::kPopulation};rt::population::Service service;
    CHECK(service.begin(owner,capabilities,boot));
    const std::array policies{ambient::InitialPolicy{&capabilities[0],4,1,0,true,&probe::kNamedDependency}};
    ambient::InitialActivation<> activation;CHECK(activation.begin(owner,boot,policies));
    CHECK(service.project(15).count==0);CHECK(activation.state(0)==ambient::InitialState::awaitingMonitor);
    CHECK(activation.observe(owner,boot,15,monitor(1,true))==1);
    CHECK(activation.state(0)==ambient::InitialState::awaitingDependency);
    CHECK(activation.activate(0,service,15)==rt::population::Result::invalid);
    const auto binding=points::lookup(probe::kNamedDependency.list).binding;
    for(std::size_t i=0;i<3;++i)CHECK(points::constructed(binding,probe::kPoints[i],static_cast<std::uint32_t>(0x700000+i)));
    CHECK(activation.state(0)==ambient::InitialState::eligible);
    CHECK(activation.activate(0,service,15)==rt::population::Result::accepted);
    auto batch=service.project(15);CHECK(batch.count==1);
    CHECK(batch.entries[0].source.registry==0x2571C34D);CHECK(batch.entries[0].source.ruleSlot==8);
    CHECK(batch.entries[0].source.looseRequested==1);
    CHECK(service.request({owner,service.revision(),service.last_request()+1,0x4A3E4900,0,1,boot},15)==rt::population::Result::unsupported);
    const coo::PopulationOwner source{owner.sessionId,boot,owner.incarnation.value,{0x2571C34D,0x80F5B4F7,1,0},9};
    coo::NativePopulationLedger<16> ledger;CHECK(ledger.begin(source));
    coo::PopulationActor actor{source,0x1342001,0x2FAA001};
    CHECK(ledger.died(actor)==coo::PopulationIntake::unknown);
    CHECK(ledger.admitted(actor)==coo::PopulationIntake::accepted);
    CHECK(ledger.died(actor)==coo::PopulationIntake::accepted);
    CHECK(ledger.counts().dead==1 && ledger.counts().resident==1 && ledger.counts().alive==0);
    CHECK(ledger.actor_retired(actor)==coo::PopulationIntake::accepted);
    CHECK(ledger.counts().resident==0 && !ledger.counts().sourceRetired);
    CHECK(points::removing(binding));
    for(std::uint32_t i=2;i<42;++i) {
        CHECK(activation.observe(owner,boot,15,monitor(i,(i&1U)!=0))==1);
        CHECK(activation.activate(0,service,15)==rt::population::Result::unchanged);
        CHECK(service.project(15).entries[0].source.looseRequested==1);
    }
    points::release(owner);
}
const std::array<points::Point,3> authored{{{0,0x80F74364,0x3175CA7E8643C2A9ULL},{5,0x80F4B3B3,0xD028E6A9CBB47BA6ULL},{6,0x80F4B3B3,0xC54292CEE0277A33ULL}}};
points::Ticket history_ticket(){points::Ticket t{{10,{2}},20,0x4A3E4900,0x80F5B9A4,19};t.count=3;std::copy(authored.begin(),authored.end(),t.points.begin());return t;}
void rebuilt_cleanup(){
 points::Mailbox m;const auto t=history_ticket();CHECK(m.bind(t));const auto first=m.lookup(t.list).binding;
 for(unsigned i=0;i<3;++i)CHECK(m.constructed(first,authored[i],100+i));CHECK(m.lookup(t.list).ready());CHECK(m.pending_retirements()==3);
 CHECK(m.removing(first));const auto next=m.lookup(t.list).binding;
 for(unsigned i=0;i<3;++i)CHECK(m.constructed(next,authored[i],200+i));CHECK(m.lookup(t.list).ready());CHECK(m.pending_retirements()==6);
 for(unsigned i=0;i<3;++i){
  const auto old=m.retirement(t.list,100+i);CHECK(old.binding==first);CHECK(!old.active && !old.ready());CHECK(old.observed==(1U<<i));
  auto wrong=old.binding;wrong.epoch+=100;CHECK(!m.actor_retired(wrong,authored[i],100+i));
  CHECK(m.actor_retired(old.binding,authored[i],100+i));CHECK(m.retirement(t.list,100+i).binding.epoch==0);
  CHECK(!m.actor_retired(old.binding,authored[i],100+i));CHECK(m.lookup(t.list).binding==next);CHECK(m.lookup(t.list).ready());
 }
 CHECK(m.pending_retirements()==3);m.release(t.owner);CHECK(!m.lookup(t.list).ready());
 auto newOwner=t;newOwner.owner.incarnation.value++;CHECK(m.bind(newOwner));const auto third=m.lookup(t.list).binding;
 for(unsigned i=0;i<3;++i)CHECK(m.constructed(third,authored[i],300+i));CHECK(m.lookup(t.list).ready());
 for(unsigned i=0;i<3;++i){const auto old=m.retirement(t.list,200+i);CHECK(old.binding==next);CHECK(m.actor_retired(old.binding,authored[i],200+i));CHECK(m.lookup(t.list).binding==third);CHECK(m.lookup(t.list).ready());}
 CHECK(m.pending_retirements()==3);
 for(unsigned i=0;i<3;++i){const auto own=m.retirement(t.list,300+i);CHECK(m.actor_retired(own.binding,authored[i],300+i));CHECK(!m.lookup(t.list).ready());}
 CHECK(m.pending_retirements()==0);CHECK(m.retirement(t.list).retired==7);
}
void history_overflow(){
 points::Mailbox m;auto t=history_ticket();t.count=1;CHECK(m.bind(t));
 for(unsigned i=0;i<256;++i){const auto b=m.lookup(t.list).binding;CHECK(m.constructed(b,authored[0],1000+i));CHECK(m.constructed(b,authored[0],1000+i));CHECK(m.pending_retirements()==i+1);CHECK(m.lookup(t.list).ready());CHECK(m.removing(b));}
 const auto b=m.lookup(t.list).binding;CHECK(!m.constructed(b,authored[0],9000));CHECK(!m.lookup(t.list).ready());CHECK(m.lookup(t.list).incomplete);CHECK(m.pending_retirements()==256);
 for(unsigned i=0;i<256;++i){const auto r=m.retirement(t.list,1000+i);CHECK(r.binding.epoch);CHECK(m.actor_retired(r.binding,authored[0],1000+i));}
 CHECK(m.pending_retirements()==0);CHECK(m.constructed(m.lookup(t.list).binding,authored[0],9000));CHECK(!m.lookup(t.list).ready());CHECK(m.lookup(t.list).incomplete);
}

int main() {rebuilt_cleanup();history_overflow();optional_registry_cases();mailbox_cases();finite_lifecycle_cases();std::printf("ambient Cabal: %u checks passed\n",checks);}
