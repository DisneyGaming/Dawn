#include "server/runtime/activity/faction_battle_service.h"
#include "server/runtime/activity/mercury_faction_battle_evidence.h"
#include "server/runtime/activity/mercury_freeroam_runtime.h"
#include "state/activity/coo/open_world_member_catalog.h"
#include <algorithm>
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
static void consumed_value(p::Service& service,std::size_t capability,std::uint32_t revision,
    std::int32_t consumedCount) {
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
    value.sourceDelta.consumedPresent=true;value.sourceDelta.consumedCount=cap.categories;
    value.sourceDelta.consumed[0]=consumedCount;
    CHECK(service.observe(15,value)!=nullptr);
}
static void consumed(p::Service& service,std::size_t capability,std::uint32_t revision) {
    const auto projected=service.project(15);const auto& cap=
        sunrise::server::runtime::activity::mercury::kPopulations[capability];
    for(std::size_t i=0;i<projected.count;++i)
        if(projected.entries[i].source.registry==cap.registry->key
            && projected.entries[i].slot==cap.slot) {
            consumed_value(service,capability,revision,projected.entries[i].source.looseRequested);
            CHECK(service.consumed(capability));
            return;
        }
    CHECK(false);
}
static void mercury_schedule_and_patrols() {
    namespace m=sunrise::server::runtime::activity::mercury;
    namespace fr=m::freeroam;
    for(unsigned minute=0;minute<60;++minute)
        CHECK(fr::join_window(static_cast<std::uint8_t>(minute))==(minute%15<3));
    CHECK(!fr::join_window(60));
    CHECK(m::kPopulations.size()==32);CHECK(fr::kPatrols.size()==27);
    for(const auto& capability:m::kPopulations)CHECK(p::valid(capability));
    CHECK(fr::valid({}) && !fr::valid({0,8000,{2,3,4,5},3,4,true,true}));
    CHECK(!fr::valid({30000,8000,{2,3,4,6},3,4,true,true}));
    CHECK(!fr::valid({30000,8000,{2,3,4,5},22,4,true,true}));
    CHECK(!fr::valid({30000,8000,{2,3,4,5},3,17,true,true}));
    CHECK(fr::valid({30000,8000,{2,3,4,5},5,4,true,true}));
    CHECK(fr::valid({30000,8000,{2,3,4,5},6,4,true,true}));
    CHECK(fr::valid({30000,8000,{2,3,4,5},21,16,false,true}));
    std::size_t largeAreas{};
    for(const auto& patrol:fr::kPatrols) {
        largeAreas+=patrol.largeArea?1U:0U;
        const auto& capability=m::kPopulations[patrol.capability];
        const auto* group=sunrise::state::activity::coo::mercury::ambient::find(capability.registry->key);
        CHECK(group && !group->sources.empty());
        if(patrol.capability==0 || patrol.capability>=22) {
            CHECK(capability.slot==group->sources[0].slot);
            CHECK(capability.rule==group->sources[0].fallbackRule);
        } else if(group->sources.size()==1) {
            CHECK(capability.slot==group->sources[0].slot);
            CHECK(capability.rule==group->sources[0].fallbackRule);
        } else {
            CHECK(capability.slot==group->sources[1].slot);
            CHECK(capability.rule==group->sources[1].fallbackRule);
        }
    }
    CHECK(largeAreas==8);
    const auto harpy=fr::kPatrols[1];
    CHECK(harpy.capability==7 && harpy.placement=="pf_lighthouse_vx_center_left_b"
        && harpy.minimum==2 && harpy.baseline==3 && harpy.maximum==4);
    CHECK(m::kPopulations[harpy.capability].registry->key==0xEB1E8934U
        && m::kPopulations[harpy.capability].slot==0
        && m::kPopulations[harpy.capability].rule==6
        && m::kPopulations[harpy.capability].tactical.registry==0xEB1E8934U
        && m::kPopulations[harpy.capability].tactical.slot==1
        && m::kPopulations[harpy.capability].tactical.row==1);
    constexpr std::array<std::array<std::uint8_t,3>,27> expectedRanges{{
        {{1,1,1}},{{2,3,4}},{{1,1,1}},{{2,2,2}},{{1,1,1}},{{2,2,2}},
        {{1,1,1}},{{2,2,3}},{{2,3,4}},{{4,4,6}},{{1,1,1}},{{2,2,2}},
        {{1,1,1}},{{1,1,1}},{{1,2,3}},{{1,1,1}},{{2,2,2}},
        {{2,2,2}},{{2,2,4}},{{4,4,4}},{{3,3,3}},{{1,1,1}},
        {{1,2,2}},{{2,3,3}},{{2,3,3}},{{1,1,1}},{{2,2,3}}
    }};
    for(std::size_t i=0;i<fr::kPatrols.size();++i) {
        CHECK(fr::kPatrols[i].minimum==expectedRanges[i][0]
            && fr::kPatrols[i].baseline==expectedRanges[i][1]
            && fr::kPatrols[i].maximum==expectedRanges[i][2]);
        if(!fr::kPatrols[i].baseline)continue;
        bool varied{};
        for(std::uint32_t sequence=1;sequence<=12;++sequence) {
            const auto target=fr::patrol_target(fr::kPatrols[i],fr::Configuration{},sequence);
            CHECK(target>=fr::kPatrols[i].minimum && target<=fr::kPatrols[i].maximum);
            varied|=target!=fr::kPatrols[i].baseline;
        }
        CHECK((fr::kPatrols[i].minimum==fr::kPatrols[i].maximum) != varied);
    }
    std::size_t baselineRequests{},maximumRequests{};
    for(const auto& patrol:fr::kPatrols) {
        baselineRequests+=fr::patrol_target(patrol,fr::Configuration{});
        maximumRequests+=fr::patrol_maximum(patrol,fr::Configuration{});
    }
    CHECK(baselineRequests==53 && maximumRequests==62);
    CHECK(m::kPopulations[25].categories==2); // explicit dormant second lane
    struct DefenderPin final {
        std::size_t capability;
        std::uint32_t registry,sourceDescriptor,templateEntity;
        std::uint16_t rule,tactical;
    };
    constexpr std::array<DefenderPin,3> defenderPins{{
        {16,0xCF2196EAU,0x80F5B706U,0x80C0D0BFU,7,2},
        {18,0xBBF1BA51U,0x80F5B883U,0x80C0D0BFU,7,2},
        {19,0x9B219BF3U,0x80F5B670U,0x80C19B1FU,7,2},
    }};
    for(const auto& pin:defenderPins) {
        const auto patrol=std::find_if(fr::kPatrols.begin(),fr::kPatrols.end(),
            [&](const auto& value){return value.capability==pin.capability;});
        CHECK(patrol!=fr::kPatrols.end());
        CHECK(patrol->minimum==1 && patrol->baseline==1 && patrol->maximum==1);
        const auto& capability=m::kPopulations[pin.capability];
        CHECK(capability.registry->key==pin.registry && capability.slot==1
            && capability.rule==pin.rule && capability.tactical.registry==pin.registry
            && capability.tactical.slot==pin.tactical && capability.tactical.row==1);
        const sunrise::state::activity::open_world_members::SourceRange* range=nullptr;
        for(const auto& candidate:sunrise::state::activity::open_world_members::kSources)
            if(candidate.resource==pin.sourceDescriptor && candidate.registry==pin.registry
                && candidate.source==1) { CHECK(range==nullptr);range=&candidate; }
        CHECK(range!=nullptr && range->count==6);
        for(std::uint32_t i=0;i<range->count;++i)
            CHECK(sunrise::state::activity::open_world_members::kChoices[range->first+i].entity
                ==pin.templateEntity); // Authored template; live display name is separate.
    }

    p::Service service;const p::Owner owner{900,{7}};
    CHECK(service.begin(owner,m::kPopulations,0x1234));
    std::array<FakeLedger,m::kPopulations.size()> ledgers{};
    std::array<std::uint8_t,m::kPopulations.size()> nativePending{};
    fr::Director director;CHECK(director.begin(owner,0x1234,3));
    CHECK(director.update(100,15,true,service,std::span<const FakeLedger>(ledgers),nativePending));
    CHECK(service.project(15).count==fr::kPatrols.size());
    CHECK(director.diagnostics().war==fr::WarPhase::unavailable);
    for(const auto& patrol:fr::kPatrols)
        CHECK(service.target(patrol.capability)==fr::patrol_target(patrol,fr::Configuration{}));
    std::uint32_t revision=10;
    for(const auto& patrol:fr::kPatrols) {
        const auto admitted=service.target(patrol.capability);
        ledgers[patrol.capability].value={admitted,0,admitted,0,false,false};
        consumed(service,patrol.capability,revision++);
    }
    CHECK(director.update(200,15,true,service,std::span<const FakeLedger>(ledgers),nativePending));
    CHECK(director.update(30199,15,true,service,std::span<const FakeLedger>(ledgers),nativePending));
    CHECK(service.target(fr::kPatrols[0].capability)==1);
    CHECK(director.update(30200,15,true,service,std::span<const FakeLedger>(ledgers),nativePending));
    for(const auto& patrol:fr::kPatrols)CHECK(service.renewal(patrol.capability).pending);
    // A provisional birth that arrives while the bridge reports busy cancels
    // the ticket. Its real death/retirement starts a fresh 30-second timer.
    const auto lateCapability=fr::kPatrols[0].capability;
    ledgers[lateCapability].value={2,1,1,1,false,false};service.cancel_renewal(lateCapability);
    CHECK(director.update(30201,15,true,service,std::span<const FakeLedger>(ledgers),nativePending));
    CHECK(!service.renewal(lateCapability).pending);
    ledgers[lateCapability].value={2,0,2,0,false,false};
    CHECK(director.update(30202,15,true,service,std::span<const FakeLedger>(ledgers),nativePending));
    CHECK(director.update(60201,15,true,service,std::span<const FakeLedger>(ledgers),nativePending));
    CHECK(!service.renewal(lateCapability).pending);
    CHECK(director.update(60202,15,true,service,std::span<const FakeLedger>(ledgers),nativePending));
    CHECK(service.renewal(lateCapability).pending);
    // A new source generation accepts its own mirror and rejects the old one.
    for(const auto& patrol:fr::kPatrols) {
        const auto renewal=service.renewal(patrol.capability);
        CHECK(renewal.nextTarget==fr::patrol_target(patrol,fr::Configuration{},1));
        CHECK(service.commit_renewal(patrol.capability));
        CHECK(service.target(patrol.capability)==fr::patrol_target(patrol,fr::Configuration{},1));
    }
    std::uint64_t cycleAt=60203;
    for(std::uint32_t cycle=0;cycle<2;++cycle) {
        for(const auto& patrol:fr::kPatrols) {
            const auto admitted=service.target(patrol.capability);
            ledgers[patrol.capability].value={admitted,0,admitted,0,false,false};
            consumed(service,patrol.capability,revision++);
        }
        CHECK(director.update(cycleAt,15,true,service,std::span<const FakeLedger>(ledgers),nativePending));
        CHECK(director.update(cycleAt+30000,15,true,service,std::span<const FakeLedger>(ledgers),nativePending));
        for(const auto& patrol:fr::kPatrols) {
            CHECK(service.renewal(patrol.capability).nextTarget
                ==fr::patrol_target(patrol,fr::Configuration{},cycle+2));
            CHECK(service.commit_renewal(patrol.capability));
        }
        cycleAt+=30001;
    }

    // The reviewed Harpy range is independent of configurable ordinary density
    // and its deterministic next value survives a consumed renewal cycle.
    p::Service customService;const p::Owner customOwner{902,{9}};
    CHECK(customService.begin(customOwner,m::kPopulations,0x9ABC));
    std::array<FakeLedger,m::kPopulations.size()> customLedgers{};
    std::array<std::uint8_t,m::kPopulations.size()> customPending{};
    auto customConfiguration=fr::Configuration{};
    customConfiguration.normalPatrolRequests=5;customConfiguration.largePatrolRequests=5;
    customConfiguration.war=false;
    fr::Director customDirector;
    CHECK(customDirector.begin(customOwner,0x9ABC,3,customConfiguration));
    CHECK(customDirector.update(100,15,true,customService,
        std::span<const FakeLedger>(customLedgers),customPending));
    CHECK(customService.target(harpy.capability)==3);
    CHECK(customService.target(fr::kPatrols[2].capability)==1);
    CHECK(customService.target(fr::kPatrols[0].capability)==1);
    customLedgers[harpy.capability].value={3,0,3,0,false,false};
    consumed(customService,harpy.capability,500);
    CHECK(customDirector.update(200,15,true,customService,
        std::span<const FakeLedger>(customLedgers),customPending));
    CHECK(customDirector.update(30200,15,true,customService,
        std::span<const FakeLedger>(customLedgers),customPending));
    const auto nextHarpy=fr::patrol_target(harpy,customConfiguration,1);
    CHECK(customService.renewal(harpy.capability).pending
        && customService.renewal(harpy.capability).nextTarget==nextHarpy);
    CHECK(customService.commit_renewal(harpy.capability));
    CHECK(customService.target(harpy.capability)==nextHarpy);
}
static void mercury_cannon_tower_cohort() {
    namespace m=sunrise::server::runtime::activity::mercury;
    namespace fr=m::freeroam;
    constexpr std::size_t phalanxCapability=10,legionaryCapability=23;
    const auto* group=c::mercury::ambient::find(0x9D083869U);
    CHECK(group && !group->hotspot && group->sources.size()==2);
    CHECK(group->sources[0].slot==0 && group->sources[0].primaryRule==8
        && group->sources[0].fallbackRule==9 && group->sources[0].definition==0x80F5B593U);
    CHECK(group->sources[1].slot==1 && group->sources[1].primaryRule==8
        && group->sources[1].fallbackRule==9 && group->sources[1].definition==0x80F5B596U);
    const auto* hotspot=c::mercury::ambient::find(0xDC79C5E5U);
    CHECK(hotspot && hotspot->hotspot);
    const auto& legionary=m::kPopulations[legionaryCapability];
    const auto& phalanx=m::kPopulations[phalanxCapability];
    CHECK(legionary.registry->key==0x9D083869U && legionary.slot==0 && legionary.rule==9
        && legionary.tactical.registry==0x9D083869U && legionary.tactical.slot==2
        && legionary.tactical.row==1 && legionary.taskMask==0xFU);
    CHECK(phalanx.registry->key==0x9D083869U && phalanx.slot==1 && phalanx.rule==9
        && phalanx.tactical.registry==0x9D083869U && phalanx.tactical.slot==2
        && phalanx.tactical.row==1 && phalanx.taskMask==0xFU);
    const auto patrolFor=[](std::size_t capability) -> const fr::Patrol* {
        for(const auto& patrol:fr::kPatrols)if(patrol.capability==capability)return &patrol;
        return nullptr;
    };
    const auto* legionaryPatrol=patrolFor(legionaryCapability);
    const auto* phalanxPatrol=patrolFor(phalanxCapability);
    CHECK(legionaryPatrol && phalanxPatrol);
    for(std::uint32_t sequence=0;sequence<16;++sequence) {
        CHECK(fr::patrol_target(*legionaryPatrol,fr::Configuration{},sequence)==2);
        CHECK(fr::patrol_target(*phalanxPatrol,fr::Configuration{},sequence)==1);
    }

    p::Service service;const p::Owner owner{903,{10}};
    CHECK(service.begin(owner,m::kPopulations,0xC0110A7U));
    std::array<FakeLedger,m::kPopulations.size()> ledgers{};
    std::array<std::uint8_t,m::kPopulations.size()> nativePending{};
    auto configuration=fr::Configuration{};configuration.war=false;
    fr::Director director;CHECK(director.begin(owner,0xC0110A7U,3,configuration));
    CHECK(director.update(100,15,true,service,std::span<const FakeLedger>(ledgers),nativePending));
    CHECK(service.target(legionaryCapability)==2 && service.target(phalanxCapability)==1);
    std::size_t towerRows{},hotspotRows{};
    for(const auto& row:service.project(15).entries) {
        towerRows+=row.source.registry==0x9D083869U?1U:0U;
        hotspotRows+=row.source.registry==0xDC79C5E5U?1U:0U;
    }
    CHECK(towerRows==2 && hotspotRows==0);
    const auto legionaryGeneration=service.generation(legionaryCapability);
    const auto phalanxGeneration=service.generation(phalanxCapability);
    CHECK(director.update(101,14,true,service,std::span<const FakeLedger>(ledgers),nativePending));
    CHECK(service.target(legionaryCapability)==2 && service.target(phalanxCapability)==1);
    towerRows=0;
    for(const auto& row:service.project_retained().entries)
        towerRows+=row.source.registry==0x9D083869U?1U:0U;
    CHECK(towerRows==2);
    CHECK(service.generation(legionaryCapability)==legionaryGeneration
        && service.generation(phalanxCapability)==phalanxGeneration);
    CHECK(director.update(102,15,true,service,std::span<const FakeLedger>(ledgers),nativePending));
    CHECK(service.target(legionaryCapability)+service.target(phalanxCapability)==3);

    ledgers[legionaryCapability].value={2,0,2,0,false,false};
    ledgers[phalanxCapability].value={1,0,1,0,false,false};
    consumed(service,legionaryCapability,700);consumed(service,phalanxCapability,701);
    CHECK(director.update(200,15,true,service,std::span<const FakeLedger>(ledgers),nativePending));
    CHECK(director.update(30200,14,true,service,std::span<const FakeLedger>(ledgers),nativePending));
    CHECK(!service.renewal(legionaryCapability).pending
        && !service.renewal(phalanxCapability).pending);
    CHECK(director.update(30201,15,true,service,std::span<const FakeLedger>(ledgers),nativePending));
    CHECK(service.renewal(legionaryCapability).pending
        && service.renewal(legionaryCapability).nextTarget==2);
    CHECK(service.renewal(phalanxCapability).pending
        && service.renewal(phalanxCapability).nextTarget==1);
    CHECK(service.commit_renewal(legionaryCapability));
    CHECK(service.commit_renewal(phalanxCapability));
    CHECK(service.target(legionaryCapability)==2 && service.target(phalanxCapability)==1);
    CHECK(service.generation(legionaryCapability)==legionaryGeneration+1
        && service.generation(phalanxCapability)==phalanxGeneration+1);
}
static void mercury_partial_replenishment() {
    namespace m=sunrise::server::runtime::activity::mercury;
    namespace fr=m::freeroam;
    constexpr std::size_t multiCapability=23;
    constexpr std::size_t singletonCapability=1;
    const auto owner_for=[](const p::Owner& owner,const p::Service& service,
        std::size_t capability,std::uint64_t run) {
        const auto& selected=m::kPopulations[capability];
        c::Asset source{selected.registry->key,0,1,selected.slot};
        for(const auto& slot:selected.registry->slots)
            if(slot.index==selected.slot)source.definition=slot.descriptorTag;
        return c::PopulationOwner{owner.sessionId,run,owner.incarnation.value,source,
            service.generation(capability)};
    };

    p::Service service;const p::Owner owner{904,{11}};constexpr std::uint64_t run=0xC0110A8U;
    CHECK(service.begin(owner,m::kPopulations,run));
    std::array<c::NativePopulationLedger<64>,m::kPopulations.size()> ledgers{};
    std::array<std::uint8_t,m::kPopulations.size()> nativePending{};
    auto configuration=fr::Configuration{};configuration.war=false;
    fr::Director director;CHECK(director.begin(owner,run,3,configuration));
    CHECK(director.update(100,15,true,service,
        std::span<const c::NativePopulationLedger<64>>(ledgers),nativePending));
    CHECK(service.target(multiCapability)==2 && service.target(singletonCapability)==1);
    const auto multiGeneration=service.generation(multiCapability);
    const auto singletonGeneration=service.generation(singletonCapability);

    const auto multiOwner=owner_for(owner,service,multiCapability,run);
    CHECK(multiOwner.valid() && ledgers[multiCapability].begin(multiOwner));
    const c::PopulationActor fallen{multiOwner,0x71001,0x72001,1};
    const c::PopulationActor survivor{multiOwner,0x71002,0x72002,2};
    CHECK(ledgers[multiCapability].admitted(fallen)==c::PopulationIntake::accepted);
    CHECK(ledgers[multiCapability].admitted(survivor)==c::PopulationIntake::accepted);
    CHECK(ledgers[multiCapability].died(fallen)==c::PopulationIntake::accepted);
    CHECK(ledgers[multiCapability].actor_retired(fallen)==c::PopulationIntake::accepted);
    consumed_value(service,multiCapability,700,1);

    // Model a duplicated singleton admission. Its authenticated casualty earns
    // a credit, but the surviving singleton occupies the entire intended roster.
    const auto singletonOwner=owner_for(owner,service,singletonCapability,run);
    CHECK(singletonOwner.valid() && ledgers[singletonCapability].begin(singletonOwner));
    const c::PopulationActor oldSingleton{singletonOwner,0x73001,0x74001,3};
    const c::PopulationActor liveSingleton{singletonOwner,0x73002,0x74002,4};
    CHECK(ledgers[singletonCapability].admitted(oldSingleton)==c::PopulationIntake::accepted);
    CHECK(ledgers[singletonCapability].admitted(liveSingleton)==c::PopulationIntake::accepted);
    CHECK(ledgers[singletonCapability].died(oldSingleton)==c::PopulationIntake::accepted);
    CHECK(ledgers[singletonCapability].actor_retired(oldSingleton)==c::PopulationIntake::accepted);
    consumed_value(service,singletonCapability,701,1);

    CHECK(director.update(200,15,true,service,
        std::span<const c::NativePopulationLedger<64>>(ledgers),nativePending));
    CHECK(director.update(30199,15,true,service,
        std::span<const c::NativePopulationLedger<64>>(ledgers),nativePending));
    CHECK(service.target(multiCapability)==2 && service.target(singletonCapability)==1);
    CHECK(director.update(30200,15,true,service,
        std::span<const c::NativePopulationLedger<64>>(ledgers),nativePending));
    CHECK(service.target(multiCapability)==3 && service.target(singletonCapability)==1);
    CHECK(service.generation(multiCapability)==multiGeneration
        && service.generation(singletonCapability)==singletonGeneration);
    CHECK(!service.renewal(multiCapability).pending
        && !service.renewal(singletonCapability).pending);
    CHECK(director.update(60200,15,true,service,
        std::span<const c::NativePopulationLedger<64>>(ledgers),nativePending));
    CHECK(service.target(multiCapability)==3 && service.target(singletonCapability)==1);
    CHECK(service.generation(multiCapability)==multiGeneration
        && service.generation(singletonCapability)==singletonGeneration);
    CHECK(!service.renewal(multiCapability).pending
        && !service.renewal(singletonCapability).pending);
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
    const auto patrolFor=[](std::size_t capability) -> const fr::Patrol* {
        for(const auto& patrol:fr::kPatrols)if(patrol.capability==capability)return &patrol;
        return nullptr;
    };
    for(const auto capability:fr::kWaves[0].capabilities) {
        const auto* patrol=patrolFor(capability);CHECK(patrol!=nullptr);
        const auto baseline=fr::patrol_target(*patrol,fr::Configuration{});
        const auto singleton=patrol->minimum==1 && patrol->baseline==1 && patrol->maximum==1;
        CHECK(service.target(capability)==(singleton?1U
            :baseline+fr::Configuration{}.waveRequests[0]));
    }
    std::uint32_t revision=100;
    for(std::size_t wave=0;wave<fr::kWarWaveCount;++wave) {
        if(wave==3) {
            // The shared singleton lease cannot clear merely because its wave
            // publishes while the receipt ledger still has zero admissions.
            for(const auto capability:fr::kWaves[wave].capabilities)
                consumed(service,capability,revision++);
            CHECK(director.update(31000,15,true,service,
                std::span<const FakeLedger>(ledgers),nativePending));
            CHECK(!director.wave_clear_pending());
        }
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
        if(wave+1<fr::kWarWaveCount)for(const auto capability:fr::kWaves[wave+1].capabilities) {
            const auto* patrol=patrolFor(capability);CHECK(patrol!=nullptr);
            const auto base=futureRenewal==capability?4U
                :unsigned{fr::patrol_target(*patrol,fr::Configuration{})};
            const auto singleton=patrol->minimum==1 && patrol->baseline==1
                && patrol->maximum==1;
            const auto expected=singleton?1U:base+fr::Configuration{}.waveRequests[wave+1];
            CHECK(service.target(capability)==expected);
        }
    }
    CHECK(director.announcement_observed(fr::kAnnouncementIncident));
    CHECK(!director.announcement_observed(fr::kAnnouncementIncident));
}
int main() {
    validation();progression();failures();source_conflicts();resident_budget();
    mercury_schedule_and_patrols();mercury_cannon_tower_cohort();
    mercury_partial_replenishment();mercury_escalation();
    std::printf("Faction battle: %u checks passed\n",checks);
}
