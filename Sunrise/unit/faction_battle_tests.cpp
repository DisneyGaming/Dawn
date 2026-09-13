#include "server/runtime/activity/faction_battle_service.h"
#include "server/runtime/activity/mercury_faction_battle_evidence.h"
#include "server/runtime/activity/mercury_freeroam_runtime.h"
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
struct FakeLedger final {
    c::PopulationCounts value{};
    [[nodiscard]] c::PopulationCounts counts() const noexcept {return value;}
};
static void consumed(p::Service& service,std::size_t capability,std::uint32_t revision) {
    namespace sense=sunrise::middleware::bap::activity_message::sense_update;
    namespace m=sunrise::server::runtime::activity::mercury;
    const auto projected=service.project(15);const auto& cap=m::kPopulations[capability];
    const p::wire::Request* request{};
    for(std::size_t i=0;i<projected.count;++i)
        if(projected.entries[i].source.registry==cap.registry->key && projected.entries[i].slot==cap.slot)
            request=&projected.entries[i];
    CHECK(request!=nullptr);if(!request)return;
    sense::SenseObject value{};value.registryKey=cap.registry->key;value.slotIndex=cap.slot;
    value.slotType=1;value.hasNativeSchema=true;value.nativeSchema=0x80807ECC;
    value.hasRootDelta=true;value.nativeRevision=revision;value.sourceDelta.present=1;
    value.sourceDelta.scalar[0]=request->source.generation;
    value.sourceDelta.consumedPresent=true;value.sourceDelta.consumedCount=1;
    value.sourceDelta.consumed[0]=request->source.looseRequested;
    CHECK(service.observe(15,value)!=nullptr);CHECK(service.consumed(capability));
}
static void mercury_schedule_and_patrols() {
    namespace m=sunrise::server::runtime::activity::mercury;
    namespace fr=m::freeroam;
    for(unsigned minute=0;minute<60;++minute)
        CHECK(fr::join_window(static_cast<std::uint8_t>(minute))==(minute%15<3));
    CHECK(!fr::join_window(60));
    CHECK(m::kPopulations.size()==23);CHECK(fr::kPatrols.size()==16);
    for(const auto& capability:m::kPopulations)CHECK(p::valid(capability));
    CHECK(fr::valid({}) && !fr::valid({0,8000,{2,3,4,5},3,4,true,true}));
    CHECK(!fr::valid({30000,8000,{2,3,4,6},3,4,true,true}));
    CHECK(!fr::valid({30000,8000,{2,3,4,5},22,4,true,true}));
    CHECK(!fr::valid({30000,8000,{2,3,4,5},3,17,true,true}));
    CHECK(fr::valid({30000,8000,{2,3,4,5},5,4,true,true}));
    CHECK(!fr::valid({30000,8000,{2,3,4,5},6,4,true,true}));
    CHECK(fr::valid({30000,8000,{2,3,4,5},21,16,false,true}));
    std::size_t largeAreas{};
    for(const auto& patrol:fr::kPatrols) {
        largeAreas+=patrol.largeArea?1U:0U;
        const auto& capability=m::kPopulations[patrol.capability];
        const auto* group=sunrise::state::activity::coo::mercury::ambient::find(capability.registry->key);
        CHECK(group && !group->sources.empty());
        if(group->sources.size()==1) {
            CHECK(capability.slot==group->sources[0].slot);
            CHECK(capability.rule==group->sources[0].fallbackRule);
        } else {
            CHECK(capability.slot==group->sources[1].slot);
            CHECK(capability.rule==group->sources[1].fallbackRule);
        }
    }
    CHECK(largeAreas==8);

    p::Service service;const p::Owner owner{900,{7}};
    CHECK(service.begin(owner,m::kPopulations,0x1234));
    std::array<FakeLedger,m::kPopulations.size()> ledgers{};
    std::array<std::uint8_t,m::kPopulations.size()> nativePending{};
    fr::Director director;CHECK(director.begin(owner,0x1234,3));
    CHECK(director.update(100,15,true,service,std::span<const FakeLedger>(ledgers),nativePending));
    CHECK(service.project(15).count==fr::kPatrols.size());
    CHECK(director.diagnostics().war==fr::WarPhase::unavailable);
    for(const auto& patrol:fr::kPatrols)
        CHECK(service.target(patrol.capability)==(patrol.largeArea?4:3));
    std::uint32_t revision=10;
    for(const auto& patrol:fr::kPatrols) {
        const auto admitted=service.target(patrol.capability);
        ledgers[patrol.capability].value={admitted,0,admitted,0,false,false};
        consumed(service,patrol.capability,revision++);
    }
    CHECK(director.update(200,15,true,service,std::span<const FakeLedger>(ledgers),nativePending));
    CHECK(director.update(30199,15,true,service,std::span<const FakeLedger>(ledgers),nativePending));
    CHECK(service.target(fr::kPatrols[0].capability)==4);
    CHECK(director.update(30200,15,true,service,std::span<const FakeLedger>(ledgers),nativePending));
    for(const auto& patrol:fr::kPatrols)CHECK(service.renewal(patrol.capability).pending);
    // A provisional birth that arrives while the bridge reports busy cancels
    // the ticket. Its real death/retirement starts a fresh 30-second timer.
    const auto lateCapability=fr::kPatrols[0].capability;
    ledgers[lateCapability].value={5,1,4,1,false,false};service.cancel_renewal(lateCapability);
    CHECK(director.update(30201,15,true,service,std::span<const FakeLedger>(ledgers),nativePending));
    CHECK(!service.renewal(lateCapability).pending);
    ledgers[lateCapability].value={5,0,5,0,false,false};
    CHECK(director.update(30202,15,true,service,std::span<const FakeLedger>(ledgers),nativePending));
    CHECK(director.update(60201,15,true,service,std::span<const FakeLedger>(ledgers),nativePending));
    CHECK(!service.renewal(lateCapability).pending);
    CHECK(director.update(60202,15,true,service,std::span<const FakeLedger>(ledgers),nativePending));
    CHECK(service.renewal(lateCapability).pending);
    // A new source generation accepts its own mirror and rejects the old one.
    for(const auto& patrol:fr::kPatrols) {
        const auto renewal=service.renewal(patrol.capability);
        CHECK(renewal.nextTarget==(patrol.largeArea?4:3));
        CHECK(service.commit_renewal(patrol.capability));
        CHECK(service.target(patrol.capability)==(patrol.largeArea?4:3));
    }
    std::uint64_t cycleAt=60203;
    for(unsigned cycle=0;cycle<2;++cycle) {
        for(const auto& patrol:fr::kPatrols) {
            const auto admitted=service.target(patrol.capability);
            ledgers[patrol.capability].value={admitted,0,admitted,0,false,false};
            consumed(service,patrol.capability,revision++);
        }
        CHECK(director.update(cycleAt,15,true,service,std::span<const FakeLedger>(ledgers),nativePending));
        CHECK(director.update(cycleAt+30000,15,true,service,std::span<const FakeLedger>(ledgers),nativePending));
        for(const auto& patrol:fr::kPatrols) {
            CHECK(service.renewal(patrol.capability).nextTarget==(patrol.largeArea?4:3));
            CHECK(service.commit_renewal(patrol.capability));
        }
        cycleAt+=30001;
    }
}
static void mercury_escalation() {
    namespace m=sunrise::server::runtime::activity::mercury;
    namespace fr=m::freeroam;
    p::Service service;const p::Owner owner{901,{8}};
    CHECK(service.begin(owner,m::kPopulations,0x5678));
    std::array<FakeLedger,m::kPopulations.size()> ledgers{};
    std::array<std::uint8_t,m::kPopulations.size()> nativePending{};
    fr::Director director;CHECK(director.begin(owner,0x5678,45));
    std::uint32_t incident{};CHECK(director.take_announcement(incident));CHECK(incident==fr::kAnnouncementIncident);
    CHECK(!director.take_announcement(incident));CHECK(!director.announcement_observed(0x8753E5BA));
    CHECK(director.update(1000,15,true,service,std::span<const FakeLedger>(ledgers),nativePending));
    CHECK(director.diagnostics().war==fr::WarPhase::active && director.diagnostics().wave==0);
    CHECK(service.project(15).count==fr::kPatrols.size());
    for(const auto capability:fr::kWaves[0].capabilities)CHECK(service.target(capability)==6);
    std::uint32_t revision=100;
    for(std::size_t wave=0;wave<fr::kWarWaveCount;++wave) {
        for(const auto capability:fr::kWaves[wave].capabilities) {
            // One native request may admit an authored group of several actors.
            const auto admitted=service.target(capability);
            ledgers[capability].value={admitted,0,admitted,0,false,false};
            consumed(service,capability,revision++);
        }
        const auto clearAt=1002+wave*10000;
        nativePending[fr::kWaves[wave].capabilities[0]]=1;
        CHECK(director.update(clearAt-1,15,true,service,std::span<const FakeLedger>(ledgers),nativePending));
        CHECK(director.diagnostics().war==fr::WarPhase::active && !director.wave_clear_pending());
        nativePending[fr::kWaves[wave].capabilities[0]]=0;
        CHECK(director.update(clearAt,15,true,service,std::span<const FakeLedger>(ledgers),nativePending));
        CHECK(director.diagnostics().war==fr::WarPhase::active && director.wave_clear_pending());
        CHECK(director.commit_wave_clear());CHECK(!director.commit_wave_clear());
        CHECK(director.diagnostics().war==fr::WarPhase::intermission);
        std::size_t futureRenewal=SIZE_MAX;
        if(wave==0) {
            futureRenewal=fr::kWaves[wave+1].capabilities[0];
            ledgers[futureRenewal].value={4,0,4,0,false,false};
            consumed(service,futureRenewal,revision++);
            const auto& capability=m::kPopulations[futureRenewal];
            const p::Command command{owner,service.revision(),service.last_request()+1,
                capability.registry->key,capability.slot,4,0x5678};
            CHECK(service.renew(command,15)==p::Result::accepted);
        }
        CHECK(director.update(clearAt+7999,15,true,service,std::span<const FakeLedger>(ledgers),nativePending));
        CHECK(director.diagnostics().war==fr::WarPhase::intermission);
        CHECK(director.update(clearAt+8000,15,true,service,std::span<const FakeLedger>(ledgers),nativePending));
        if(futureRenewal!=SIZE_MAX) {
            CHECK(director.diagnostics().war==fr::WarPhase::intermission);
            CHECK(service.commit_renewal(futureRenewal));ledgers[futureRenewal].value={};
            CHECK(director.update(clearAt+8001,15,true,service,std::span<const FakeLedger>(ledgers),nativePending));
        }
        CHECK(director.diagnostics().war==(wave+1==fr::kWarWaveCount?fr::WarPhase::complete:fr::WarPhase::active));
        if(wave+1<fr::kWarWaveCount)for(const auto capability:fr::kWaves[wave+1].capabilities)
            CHECK(service.target(capability)==4+fr::Configuration{}.waveRequests[wave+1]);
    }
    CHECK(director.announcement_observed(fr::kAnnouncementIncident));
    CHECK(!director.announcement_observed(fr::kAnnouncementIncident));
}
int main() {
    validation();progression();failures();source_conflicts();resident_budget();
    mercury_schedule_and_patrols();mercury_escalation();
    std::printf("Faction battle: %u checks passed\n",checks);
}
