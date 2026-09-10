#include "server/runtime/activity/faction_battle_service.h"
#include "server/runtime/activity/mercury_faction_battle_evidence.h"
#include <cstdio>
#include <cstdlib>

namespace f=sunrise::server::runtime::activity::faction_battle;
namespace p=sunrise::server::runtime::activity::population;
namespace r=sunrise::server::runtime::activity::registry;
namespace e=sunrise::state::activity::native_population;
namespace c=sunrise::state::activity::coo;
static unsigned checks;
#define CHECK(value) do {++checks;if(!(value)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#value);std::exit(1);}} while(false)

// Synthetic, independently named profile data; these are not Mercury sources.
static constexpr r::Slot slots[]{
    {0,1,0x80809A3B,0x80807ECC,0x80807EC9,0x80900101},
    {0,1,0x80809A3B,0x80807ECC,0x80807EC9,0x80900102},
    {0,1,0x80809A3B,0x80807ECC,0x80807EC9,0x80900103},
    {0,1,0x80809A3B,0x80807ECC,0x80807EC9,0x80900104}
};
static constexpr r::Definition registries[]{
    {"fixture_destination",0x80900001,0x101,0x80900201,0x1010,3,{slots+0,1}},
    {"fixture_destination",0x80900001,0x102,0x80900202,0x1010,3,{slots+1,1}},
    {"fixture_destination",0x80900001,0x103,0x80900203,0x1010,3,{slots+2,1}},
    {"fixture_destination",0x80900001,0x104,0x80900204,0x1010,3,{slots+3,1}}
};
static const std::array<p::Capability,4> capabilities{{
    {registries+0,0,0,{},false},{registries+1,0,0,{},false},
    {registries+2,0,0,{},false},{registries+3,0,0,{},false}
}};
static constexpr f::Wave waves[]{
    {{f::Cohort{0,1,2},f::Cohort{1,1,1}}},{{f::Cohort{2,1,1},f::Cohort{3,1,1}}}
};
static const f::Definition definition{0x5678,0x1234,f::kRequiredEvidence,5,capabilities,waves};
static constexpr f::Token token{{42,{7}},0x9999,1,1};

static e::Event actor(f::Service& service,std::size_t source,std::uint32_t identity,e::Kind kind=e::Kind::admitted) {
    const auto lease=service.lease(source);
    return {lease,{lease.source,identity,identity+0x10000},0,kind}; // zero is a valid native source handle
}
static void launch(f::Service& service) {
    CHECK(service.begin(token,definition));
    CHECK(!service.begin(token,definition));
    CHECK(!service.update(token,3,true));
    CHECK(service.project(3).count==0);
    CHECK(service.request_announcement(token));
    CHECK(!service.request_announcement(token));
    CHECK(!service.update(token,3,true));
    CHECK(!service.announcement_observed(token,0x1235));
    auto stale=token;++stale.boot;
    CHECK(!service.announcement_observed(stale,0x1234));
    CHECK(service.announcement_observed(token,0x1234));
    CHECK(!service.announcement_observed(token,0x1234));
    CHECK(!service.update(token,3,false));
    CHECK(!service.update(token,4,true));
    CHECK(service.update(token,3,true));
    CHECK(service.project(3).count==2 && service.project(4).count==0);
    CHECK(service.lease(0).source.run==token.boot);
    CHECK(!service.lease(2).activity);
}
static void admit_and_die(f::Service& service,std::size_t source,std::uint32_t id) {
    auto event=actor(service,source,id);
    CHECK(service.observe(token,event)==c::PopulationIntake::accepted);
    CHECK(service.observe(token,event)==c::PopulationIntake::duplicate);
    event.kind=e::Kind::died;
    CHECK(service.observe(token,event)==c::PopulationIntake::accepted);
    CHECK(service.observe(token,event)==c::PopulationIntake::duplicate);
}
static void progression() {
    f::Service service;launch(service);
    for(unsigned i=0;i<1000;++i) CHECK(!service.update(token,3,true));
    CHECK(service.diagnostics().blocked==f::Block::births);
    admit_and_die(service,0,1);
    CHECK(!service.update(token,3,true)); // request1 contains two authored actors
    admit_and_die(service,0,2);
    CHECK(!service.update(token,3,true)); // the opposing cohort is still absent
    auto opponent=actor(service,1,3);
    CHECK(service.observe(token,opponent)==c::PopulationIntake::accepted);
    CHECK(!service.update(token,3,true));
    CHECK(service.diagnostics().blocked==f::Block::deaths);
    opponent.kind=e::Kind::died;CHECK(service.observe(token,opponent)==c::PopulationIntake::accepted);
    CHECK(service.update(token,3,true));
    CHECK(service.project(3).count==4 && service.diagnostics().wave==1);
    CHECK(service.diagnostics().sides[0].admitted==0);
    CHECK(service.observe(token,opponent)==c::PopulationIntake::duplicate); // old wave cannot clear new wave
    CHECK(!service.update(token,3,true));
    admit_and_die(service,2,4);admit_and_die(service,3,5);
    CHECK(service.update(token,3,true));
    CHECK(service.diagnostics().phase==f::Phase::resolved);
    CHECK(!service.begin(token,definition));
    CHECK(service.retirement_requested(token));
    CHECK(service.diagnostics().phase==f::Phase::retiring);
    for(std::size_t i=0;i<4;++i) CHECK(service.source_retired(token,service.lease(i)));
    CHECK(service.diagnostics().phase==f::Phase::retiring); // dead corpses are still resident
    for(std::uint32_t id=1;id<=5;++id) {
        const auto source=id<=2?0U:id-2;
        auto retired=actor(service,source,id,e::Kind::retired);
        CHECK(service.observe(token,retired)==c::PopulationIntake::accepted);
    }
    CHECK(service.diagnostics().phase==f::Phase::retired);
    CHECK(service.diagnostics().blocked==f::Block::sourceRenewal);
    CHECK(service.project(3).count==4); // source authority was not reset or lowered
    auto next=token;++next.run;CHECK(!service.begin(next,definition));
}
static void failures() {
    f::Service service;launch(service);
    auto stale=token;++stale.owner.incarnation.value;
    auto event=actor(service,0,10);
    CHECK(service.observe(stale,event)==c::PopulationIntake::unrelated);
    event.lease.source.generation++;
    CHECK(service.observe(token,event)==c::PopulationIntake::unrelated);
    event=actor(service,0,10,e::Kind::died);
    CHECK(service.observe(token,event)==c::PopulationIntake::unknown);
    CHECK(service.diagnostics().phase==f::Phase::failed);
    CHECK(!service.update(token,3,true));
    CHECK(service.project(3).count==2); // retain native ownership on observation loss
    CHECK(service.retirement_requested(token));
    CHECK(service.source_retired(token,service.lease(0)));
    CHECK(service.source_retired(token,service.lease(1)));
    CHECK(service.diagnostics().phase==f::Phase::retiring); // incomplete never proves cleanup
}
static void source_conflicts() {
    f::Service service;launch(service);
    auto event=actor(service,0,10);
    CHECK(service.observe(token,event)==c::PopulationIntake::accepted);
    event.sourceHandle=1;event.kind=e::Kind::died;
    CHECK(service.observe(token,event)==c::PopulationIntake::conflict);
    CHECK(!service.update(token,3,true));
}
static void validation() {
    CHECK(f::Service::valid(definition));
    for(std::uint16_t bit=1;bit<=64;bit=static_cast<std::uint16_t>(bit<<1)) {
        auto invalid=definition;invalid.evidence=static_cast<std::uint16_t>(invalid.evidence&~bit);
        f::Service service;CHECK(!service.begin(token,invalid));CHECK(service.project(3).count==0);
    }
    auto reused=waves[0];reused.sides[1].capability=0;
    auto invalid=definition;invalid.waves={&reused,1};CHECK(!f::Service::valid(invalid));
    reused=waves[0];reused.sides[0].actors=0;CHECK(!f::Service::valid(invalid));
    reused=waves[0];reused.sides[1].requests=64;CHECK(!f::Service::valid(invalid));
    auto duplicate=capabilities;duplicate[1]=duplicate[0];invalid=definition;invalid.populations=duplicate;
    f::Service service;CHECK(!service.begin(token,invalid));CHECK(service.project(3).count==0);
    CHECK(!sunrise::server::runtime::activity::mercury::faction_battle_evidence::kActivationSupported);
    CHECK(sunrise::server::runtime::activity::mercury::faction_battle_evidence::kUnresolved.size()==8);
}
static void resident_budget() {
    f::Service service;auto bounded=definition;bounded.residentBudget=3;
    CHECK(service.begin(token,bounded));CHECK(service.request_announcement(token));
    CHECK(service.announcement_observed(token,0x1234));CHECK(service.update(token,3,true));
    admit_and_die(service,0,1);admit_and_die(service,0,2);admit_and_die(service,1,3);
    CHECK(!service.update(token,3,true));CHECK(service.diagnostics().blocked==f::Block::residentBudget);
    CHECK(service.project(3).count==2); // corpses reserve budget until native release
    auto retired=actor(service,0,1,e::Kind::retired);CHECK(service.observe(token,retired)==c::PopulationIntake::accepted);
    CHECK(!service.update(token,3,true));
    retired=actor(service,0,2,e::Kind::retired);CHECK(service.observe(token,retired)==c::PopulationIntake::accepted);
    CHECK(service.update(token,3,true));CHECK(service.project(3).count==4);
    auto late=actor(service,0,100);CHECK(service.observe(token,late)==c::PopulationIntake::accepted);
    CHECK(service.diagnostics().phase==f::Phase::failed); // extra old-wave birth invalidates accounting
}
int main() {validation();progression();failures();source_conflicts();resident_budget();std::printf("Faction battle: %u checks passed\n",checks);}
