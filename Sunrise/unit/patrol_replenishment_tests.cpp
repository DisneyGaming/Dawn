#include "server/runtime/activity/patrol_replenishment.h"
#include "server/runtime/activity/open_world_definitions.h"
#include "server/runtime/activity/open_world_runtime.h"
#include "server/bap/encrypted/push/activity/native_activity_publisher.h"
#include "state/activity/coo/native_population_ledger.h"
#include <cstdio>
#include <cstdlib>
namespace p=sunrise::server::runtime::activity::patrol_replenishment;
namespace a=sunrise::server::runtime::activity;
namespace c=sunrise::state::activity::coo;
namespace {
unsigned checks{};
void check(bool value,int line) {++checks;if(!value){std::printf("FAIL %d\n",line);std::exit(1);}}
#define CHECK(...) check((__VA_ARGS__),__LINE__)
void deadlines() {
    p::Credits credits;
    CHECK(credits.observe(1,100,{0,0}));
    CHECK(credits.observe(1,1000,{1,0}));
    CHECK(credits.observe(1,2000,{2,1}));
    CHECK(!credits.due(30999).total());
    CHECK(credits.due(31000).lane[0]==1 && credits.due(31000).lane[1]==0);
    CHECK(!credits.commit(31000,{{2,0}}));
    CHECK(credits.commit(31000,{{1,0}}));
    CHECK(credits.observe(1,31000,{2,1}));
    CHECK(!credits.due(31999).total());
    CHECK(credits.due(32000).lane[0]==1 && credits.due(32000).lane[1]==1);
    CHECK(credits.commit(32000,{{1,1}}));
    CHECK(!credits.commit(32000,{{1,1}}));
    CHECK(credits.observe(1,50000,{2,1}) && !credits.due(50000).total());
    CHECK(!credits.observe(1,50000,{1,1}));
    CHECK(!credits.observe(1,49999,{2,1}));
    CHECK(credits.observe(2,50001,{0,0}) && !credits.due(90000).total());
    CHECK(!credits.observe(1,90000,{0,0}));
    CHECK(!credits.observe(2,UINT64_MAX,{1,0}));
    CHECK(p::fits(62,0,{{2,0}}));CHECK(p::fits(255,0,{{1,0}}));
    CHECK(!p::fits(INT32_MAX,0,{{1,0}}));CHECK(!p::fits(0,0,{{1,0}}));
    CHECK(p::fits(INT32_MAX-1,INT32_MAX-1,{{1,1}}));
    CHECK(!p::fits(INT32_MAX-1,0,{{2,0}}));
    std::size_t result{};
    CHECK(p::casualties(c::PopulationCounts{4,1,3,2,false,false},result) && result==2);
    CHECK(p::casualties(c::PopulationCounts{4,1,0,1,false,false},result) && result==0);
    CHECK(!p::casualties(c::PopulationCounts{4,2,1,4,false,false},result));
}
struct Service final {
    a::population::Service::Observation mirror{};
    unsigned first{3},second{1};
    std::uint32_t generation(std::size_t) const {return 1;}
    unsigned target(std::size_t) const {return first;}
    unsigned second_target(std::size_t) const {return second;}
    const auto* observation(std::size_t) const {return &mirror;}
};
void categories() {
    const c::PopulationOwner owner{42,11,1,{0x123,0x456,1,0},1};
    c::NativePopulationLedger<64> ledger;CHECK(ledger.begin(owner));
    const c::PopulationActor first{owner,1,101},second{owner,2,102},survivor{owner,3,103};
    CHECK(ledger.admitted(first,0)==c::PopulationIntake::accepted);
    CHECK(ledger.admitted(second,1)==c::PopulationIntake::accepted);
    CHECK(ledger.admitted(survivor,0)==c::PopulationIntake::accepted);
    CHECK(ledger.died(first)==c::PopulationIntake::accepted);
    CHECK(ledger.actor_retired(first)==c::PopulationIntake::accepted);
    CHECK(ledger.died(second)==c::PopulationIntake::accepted);
    Service service;service.mirror.consumedKnown=true;service.mirror.consumedCount=2;
    service.mirror.consumed[0]=1;service.mirror.consumed[1]=1;
    p::Credits credits;p::Counts due;
    const auto ready=[&](std::uint64_t now,bool pending=false) {
        return p::ready(credits,now,30000,service,0,ledger,true,pending,{{3,1}},due);
    };
    CHECK(ready(1000) && !due.total());
    CHECK(ready(31000,true) && !due.total());
    CHECK(ready(31000));
    CHECK(due.lane[0]==1 && due.lane[1]==0);
    CHECK(credits.commit(31000,due));
    CHECK(ledger.actor_retired(second)==c::PopulationIntake::accepted);
    CHECK(ready(32000) && !due.total());
    CHECK(ready(61999) && !due.total());
    CHECK(ready(62000));
    CHECK(due.lane[0]==0 && due.lane[1]==1);
    service.mirror.consumed[1]=0;
    CHECK(ready(62001) && !due.total());
    service.mirror.consumed[1]=1;
    CHECK(ready(62002) && due.lane[1]==1);
    service.first=63;service.second=0;
    CHECK(ready(62003) && !due.total());
    c::NativePopulationLedger<64> unknown;CHECK(unknown.begin(owner));
    CHECK(unknown.admitted(first)==c::PopulationIntake::accepted);
    CHECK(unknown.died(first)==c::PopulationIntake::accepted);
    CHECK(unknown.actor_retired(first)==c::PopulationIntake::accepted);
    CHECK(unknown.admitted(survivor)==c::PopulationIntake::accepted);
    p::Credits unknownCredits;service.first=3;service.second=1;
    CHECK(p::ready(unknownCredits,1000,30000,service,0,unknown,true,false,{{3,1}},due));
    CHECK(p::ready(unknownCredits,40000,30000,service,0,unknown,true,false,{{3,1}},due) && !due.total());
    // A duplicated leader that loses one actor is not a vacant singleton.
    p::Credits singleton;service.first=1;service.second=0;service.mirror.consumedCount=1;
    CHECK(p::ready(singleton,1000,30000,service,0,unknown,false,false,{{1,0}},due));
    CHECK(p::ready(singleton,40000,30000,service,0,unknown,false,false,{{1,0}},due) && !due.total());
    CHECK(ledger.admitted(survivor,1)==c::PopulationIntake::conflict);
}
void mars_survivors() {
    const auto& definition=a::open_world::profiles::mars::kActivity;
    const auto ordinary=definition.populations.first(
        definition.openWorld->authored->populations.size());
    const a::population::Owner owner{42,{1}};
    a::population::Service service;CHECK(service.begin(owner,definition.populations,99));
    a::open_world::Director director;CHECK(director.begin(owner,99,*definition.openWorld,ordinary));
    std::array<c::NativePopulationLedger<64>,a::population::kSourceCapacity> ledgers{};
    std::array<std::uint8_t,a::population::kSourceCapacity> pending{};
    std::size_t index=SIZE_MAX,unvisited=SIZE_MAX;
    for(std::size_t i=0;i<ordinary.size();++i) {
        const auto& cap=ordinary[i];
        if(cap.registry->key==0xD503E412 && cap.slot==2)index=i;
        if(unvisited==SIZE_MAX && cap.registry->bubble==5)unvisited=i;
    }
    CHECK(index!=SIZE_MAX && unvisited!=SIZE_MAX);
    const auto tick=[&](std::uint64_t now,std::uint32_t bubble=1) {
        return director.update(now,bubble,true,service,std::span<const c::NativePopulationLedger<64>>(ledgers),pending);
    };
    CHECK(tick(100));CHECK(service.target(index)==6);CHECK(service.target(unvisited)==0);
    const auto& cap=ordinary[index];
    c::Asset asset{cap.registry->key,0,1,cap.slot};
    for(const auto& slot:cap.registry->slots)if(slot.index==cap.slot)asset.definition=slot.descriptorTag;
    const c::PopulationOwner source{42,99,1,asset,1};
    auto& ledger=ledgers[index];CHECK(ledger.begin(source));
    std::array<c::PopulationActor,6> actors{};
    for(std::uint32_t i=0;i<actors.size();++i) {
        actors[i]={source,i+1,i+101};CHECK(ledger.admitted(actors[i],0)==c::PopulationIntake::accepted);
    }
    for(unsigned i=0;i<3;++i) {
        CHECK(ledger.died(actors[i])==c::PopulationIntake::accepted);
        CHECK(ledger.actor_retired(actors[i])==c::PopulationIntake::accepted);
    }
    sunrise::middleware::bap::activity_message::sense_update::SenseObject observed{};
    observed.registryKey=cap.registry->key;observed.slotIndex=cap.slot;observed.slotType=1;
    observed.hasNativeSchema=true;observed.nativeSchema=0x80807ECC;observed.nativeRevision=1;
    observed.hasRootDelta=true;observed.sourceDelta.present=1;observed.sourceDelta.scalar[0]=1;
    observed.sourceDelta.consumedPresent=true;observed.sourceDelta.consumedCount=1;
    observed.sourceDelta.consumed[0]=3;
    CHECK(service.observe_retained(observed));
    // The player crossed to bubble2 before these bubble1 casualties. An already
    // started source continues its cooldown without activating bubble5.
    CHECK(tick(1000,2));CHECK(tick(30999,2));CHECK(service.target(index)==6);
    CHECK(tick(31000,2));CHECK(service.target(index)==9);CHECK(service.target(unvisited)==0);
    CHECK(service.generation(index)==1 && ledger.owner()==source);
    CHECK(ledger.counts().alive==3 && ledger.counts().admitted==6);
    CHECK(tick(62000));CHECK(service.target(index)==9 && !service.renewal(index).pending);
    CHECK(tick(62001,2));CHECK(tick(92002,1));CHECK(service.target(index)==9);
    CHECK(ledger.counts().alive==3);
    // Keep the same three survivors through more than64 lifetime replacements.
    // The native generation and authored rule never change around those actors.
    std::uint64_t now=100000;
    std::uint32_t totalDeaths=3;
    for(std::uint32_t cycle=0;cycle<100;++cycle) {
        for(std::uint32_t member=0;member<3;++member) {
            const auto id=1000+cycle*3+member;
            const c::PopulationActor replacement{source,id,id+10000,std::uint64_t(id)+1};
            CHECK(ledger.admitted(replacement,0)==c::PopulationIntake::accepted);
            CHECK(ledger.died(replacement)==c::PopulationIntake::accepted);
            CHECK(ledger.actor_retired(replacement)==c::PopulationIntake::accepted);
        }
        totalDeaths+=3;observed.nativeRevision=cycle+2;
        observed.sourceDelta.consumed[0]=static_cast<std::int32_t>(totalDeaths);
        CHECK(service.observe_retained(observed));
        const auto oldTarget=service.target(index);
        CHECK(tick(now));CHECK(tick(now+29999));CHECK(service.target(index)==oldTarget);
        CHECK(tick(now+30000));CHECK(service.target(index)==oldTarget+3);
        CHECK(service.generation(index)==1 && ledger.owner()==source && ledger.counts().alive==3);
        CHECK(tick(now+30001));CHECK(service.target(index)==oldTarget+3);
        now+=40000;
    }
    CHECK(service.target(index)==309 && ledger.counts().dead==303);
    // Finish outstanding births, then fully clear after prolonged partial
    // refilling. The wide old quota must not truncate during renewal.
    for(std::uint32_t member=0;member<3;++member) {
        const c::PopulationActor replacement{source,5000+member,15000+member,5001ULL+member};
        CHECK(ledger.admitted(replacement,0)==c::PopulationIntake::accepted);
        CHECK(ledger.died(replacement)==c::PopulationIntake::accepted);
        CHECK(ledger.actor_retired(replacement)==c::PopulationIntake::accepted);
    }
    for(unsigned member=3;member<6;++member) {
        CHECK(ledger.died(actors[member])==c::PopulationIntake::accepted);
        CHECK(ledger.actor_retired(actors[member])==c::PopulationIntake::accepted);
    }
    observed.nativeRevision=200;observed.sourceDelta.consumed[0]=309;
    CHECK(service.observe_retained(observed));CHECK(service.consumed(index));
    // All six current members are now dead and retired while the player remains
    // in bubble2. Whole-source cooling likewise uses the authored bubble1 lease.
    CHECK(tick(now,2));CHECK(tick(now+29999,2));CHECK(!service.renewal(index).pending);
    CHECK(tick(now+30000,2));CHECK(service.renewal(index).pending);
    CHECK(service.target(unvisited)==0);
    CHECK(service.renewal(index).target==309 && service.renewal(index).nextTarget==6);
    auto next=source;++next.generation;
    CHECK(ledger.renew(source,next));CHECK(service.commit_renewal(index));
    CHECK(service.target(index)==6 && service.generation(index)==2);
    CHECK(tick(now+30001));CHECK(service.target(index)==6 && !service.renewal(index).pending);
}
struct BubbleCensus final {unsigned sources{},requests{};};
[[nodiscard]] BubbleCensus census(const a::population::Service& service,std::uint32_t bubble) {
    BubbleCensus result{};const auto retained=service.project_retained();
    for(std::size_t i=0;i<retained.count;++i)if(retained.entries[i].bubble==bubble) {
        ++result.sources;result.requests+=retained.entries[i].source.looseRequested;
        result.requests+=retained.entries[i].source.secondRequested;
    }
    return result;
}
void held_region_prefetch_profile(const a::NativeActivityDefinition& definition,
    std::uint32_t publicationBubble,std::uint32_t heldBubble,BubbleCensus publicationExpected,
    BubbleCensus heldExpected,std::uint32_t dormantBubble,std::uint32_t privateBubble,
    std::uint64_t ownerId) {
    namespace publisher=sunrise::server::bap::encrypted::push::activity::native_publisher;
    CHECK(publisher::population_prefetch_bubble(-1)==UINT32_MAX);
    CHECK(publisher::population_prefetch_bubble(57)==UINT32_MAX);
    CHECK(publisher::population_prefetch_bubble(512)==UINT32_MAX);
    CHECK(publisher::population_prefetch_bubble(0)==0);
    CHECK(publisher::population_prefetch_bubble(504)==63);
    const auto ordinary=definition.populations.first(
        definition.openWorld->authored->populations.size());
    const a::population::Owner owner{ownerId,{1}};
    a::population::Service service;CHECK(service.begin(owner,definition.populations,99));
    a::open_world::Director director;CHECK(director.begin(owner,99,*definition.openWorld,ordinary));
    std::array<c::NativePopulationLedger<64>,a::population::kSourceCapacity> ledgers{};
    std::array<std::uint8_t,a::population::kSourceCapacity> pending{};
    const auto tick=[&](std::uint64_t now,std::int32_t publication,std::int32_t current) {
        const auto region=publisher::runtime_region(publication,current);
        if(region<0 || region%8)return false;
        return director.update(now,static_cast<std::uint32_t>(region/8),true,service,
            std::span<const c::NativePopulationLedger<64>>(ledgers),pending,
            publisher::population_prefetch_bubble(publication));
    };
    const auto begin_ledgers=[&](std::uint32_t bubble) {
        for(std::size_t i=0;i<ordinary.size();++i) {
            const auto& cap=ordinary[i];
            if(cap.registry->bubble!=bubble || !service.target(i) || ledgers[i].owner().valid())continue;
            c::Asset asset{cap.registry->key,0,1,cap.slot};
            for(const auto& slot:cap.registry->slots)
                if(slot.index==cap.slot && slot.type==1)asset.definition=slot.descriptorTag;
            CHECK(ledgers[i].begin({owner.sessionId,99,service.generation(i),asset,1}));
        }
    };
    // Before arrival, only the publication hint may queue patrols. It cannot
    // start the purported current bubble, queue its NPCs, or advance time.
    {
        const a::population::Owner loadingOwner{ownerId+200,{1}};
        a::population::Service loadingService;
        CHECK(loadingService.begin(loadingOwner,definition.populations,299));
        a::open_world::Director loadingDirector;
        CHECK(loadingDirector.begin(loadingOwner,299,*definition.openWorld,ordinary));
        std::array<c::NativePopulationLedger<64>,a::population::kSourceCapacity> loadingLedgers{};
        std::array<std::uint8_t,a::population::kSourceCapacity> loadingPending{};
        CHECK(loadingDirector.update(500,heldBubble,false,loadingService,
            std::span<const c::NativePopulationLedger<64>>(loadingLedgers),loadingPending,
            publicationBubble));
        CHECK(census(loadingService,publicationBubble).sources==publicationExpected.sources
            && census(loadingService,publicationBubble).requests==publicationExpected.requests);
        CHECK(census(loadingService,heldBubble).sources==0);
        const auto loadingRevision=loadingService.revision();
        CHECK(loadingDirector.update(501,heldBubble,false,loadingService,
            std::span<const c::NativePopulationLedger<64>>(loadingLedgers),loadingPending,
            publicationBubble));
        CHECK(loadingService.revision()==loadingRevision);
        // A lower first arrived clock remains valid: loading prewarm did not
        // advance the renewal timeline. Actual arrival now starts held policy.
        CHECK(loadingDirector.update(1,heldBubble,true,loadingService,
            std::span<const c::NativePopulationLedger<64>>(loadingLedgers),loadingPending,
            publicationBubble));
        CHECK(census(loadingService,heldBubble).sources==heldExpected.sources
            && census(loadingService,heldBubble).requests==heldExpected.requests);
    }
    // Publication may prefetch the adjacent bubble. The held/current cohort is
    // still authoritative, while the incoming ordinary patrol slice prewarms.
    CHECK(tick(1,static_cast<std::int32_t>(publicationBubble*8),
        static_cast<std::int32_t>(heldBubble*8)));
    CHECK(census(service,heldBubble).sources==heldExpected.sources
        && census(service,heldBubble).requests==heldExpected.requests);
    CHECK(census(service,publicationBubble).sources==publicationExpected.sources
        && census(service,publicationBubble).requests==publicationExpected.requests);
    CHECK(census(service,dormantBubble).sources==0);
    const auto prewarmRevision=service.revision();
    CHECK(tick(2,static_cast<std::int32_t>(publicationBubble*8),
        static_cast<std::int32_t>(heldBubble*8)));
    CHECK(service.revision()==prewarmRevision); // repeated hints never duplicate requests
    begin_ledgers(heldBubble);
    begin_ledgers(publicationBubble);
    unsigned publicationNpcs{};
    for(std::size_t i=0;i<ordinary.size();++i)
        if(ordinary[i].registry->bubble==publicationBubble
            && definition.openWorld->authored->populations[i].kind
                ==a::open_world::authored::PopulationKind::npc)++publicationNpcs;
    CHECK(tick(2,static_cast<std::int32_t>(publicationBubble*8),
        static_cast<std::int32_t>(publicationBubble*8)));
    // Crossing retains every prewarmed patrol lease; only non-prewarmed NPCs
    // may add a first request when the bubble becomes current.
    CHECK(service.revision()==prewarmRevision+publicationNpcs);
    CHECK(census(service,publicationBubble).sources==publicationExpected.sources+publicationNpcs
        && census(service,publicationBubble).requests==publicationExpected.requests+publicationNpcs);
    CHECK(census(service,heldBubble).sources==heldExpected.sources
        && census(service,heldBubble).requests==heldExpected.requests);
    // Private lost-sector regions have no ordinary population bindings, and
    // may not discard retained adjacent patrol authority.
    const auto before=service.project_retained();
    CHECK(tick(3,static_cast<std::int32_t>(heldBubble*8),
        static_cast<std::int32_t>(privateBubble*8)));
    CHECK(service.project_retained().count==before.count
        && census(service,privateBubble).sources==0);
    // Malformed held evidence fails closed at the caller; it must never fall
    // back to a well-formed publication and activate the wrong bubble.
    const auto malformed=static_cast<std::int32_t>(heldBubble*8+1);
    CHECK(publisher::runtime_region(static_cast<std::int32_t>(publicationBubble*8),malformed)==malformed);
    CHECK(!tick(4,static_cast<std::int32_t>(publicationBubble*8),malformed));

    // A prewarm hint cannot restart or delay a retained held source's 30-second
    // casualty clock. Settle one held patrol, then keep repeating the hint.
    std::size_t settled=ordinary.size();
    for(std::size_t i=0;i<ordinary.size();++i)
        if(ordinary[i].registry->bubble==heldBubble
            && definition.openWorld->authored->populations[i].kind
                ==a::open_world::authored::PopulationKind::patrol){settled=i;break;}
    CHECK(settled<ordinary.size());
    const auto& cap=ordinary[settled];const auto sourceOwner=ledgers[settled].owner();
    std::uint32_t actorId=1;
    for(std::uint8_t lane=0;lane<cap.categories;++lane) {
        const auto count=lane?service.second_target(settled):service.target(settled);
        for(std::uint32_t member=0;member<count;++member) {
            const c::PopulationActor actor{sourceOwner,actorId,actorId+100};++actorId;
            CHECK(ledgers[settled].admitted(actor,lane)==c::PopulationIntake::accepted);
            CHECK(ledgers[settled].died(actor)==c::PopulationIntake::accepted);
            CHECK(ledgers[settled].actor_retired(actor)==c::PopulationIntake::accepted);
        }
    }
    sunrise::middleware::bap::activity_message::sense_update::SenseObject observed{};
    observed.registryKey=cap.registry->key;observed.slotIndex=cap.slot;observed.slotType=1;
    observed.hasNativeSchema=true;observed.nativeSchema=0x80807ECC;observed.nativeRevision=1;
    observed.hasRootDelta=true;observed.sourceDelta.present=1;
    observed.sourceDelta.scalar[0]=service.generation(settled);observed.sourceDelta.consumedPresent=true;
    observed.sourceDelta.consumedCount=cap.categories;
    observed.sourceDelta.consumed[0]=service.target(settled);
    observed.sourceDelta.consumed[1]=service.second_target(settled);
    CHECK(service.observe_retained(observed));
    CHECK(tick(100,static_cast<std::int32_t>(publicationBubble*8),
        static_cast<std::int32_t>(heldBubble*8)));
    CHECK(tick(30099,static_cast<std::int32_t>(publicationBubble*8),
        static_cast<std::int32_t>(heldBubble*8)));
    CHECK(!service.renewal(settled).pending);
    CHECK(tick(30100,static_cast<std::int32_t>(publicationBubble*8),
        static_cast<std::int32_t>(heldBubble*8)));
    CHECK(service.renewal(settled).pending);

    // On the first update, before held-region evidence exists, publication is
    // the only valid route and activates that exact profile slice.
    const a::population::Owner fallbackOwner{ownerId+100,{1}};
    a::population::Service fallbackService;
    CHECK(fallbackService.begin(fallbackOwner,definition.populations,199));
    a::open_world::Director fallbackDirector;
    CHECK(fallbackDirector.begin(fallbackOwner,199,*definition.openWorld,ordinary));
    std::array<c::NativePopulationLedger<64>,a::population::kSourceCapacity> fallbackLedgers{};
    std::array<std::uint8_t,a::population::kSourceCapacity> fallbackPending{};
    const auto region=publisher::runtime_region(static_cast<std::int32_t>(heldBubble*8),-1);
    CHECK(region==static_cast<std::int32_t>(heldBubble*8));
    CHECK(fallbackDirector.update(1,heldBubble,true,fallbackService,
        std::span<const c::NativePopulationLedger<64>>(fallbackLedgers),fallbackPending));
    CHECK(census(fallbackService,heldBubble).sources==heldExpected.sources
        && census(fallbackService,heldBubble).requests==heldExpected.requests);
}
void held_region_prefetch_profiles() {
    namespace profiles=a::open_world::profiles;
    // Titan reproduces the observed Rig-held/Siren-prefetch failure exactly.
    held_region_prefetch_profile(profiles::titan::kActivity,2,7,{27,40},{31,44},5,8,700);
    held_region_prefetch_profile(profiles::mars::kActivity,7,5,{22,32},{24,44},1,2,710);
    held_region_prefetch_profile(profiles::nessus::kActivity,30,11,{36,54},{38,54},3,5,720);
    held_region_prefetch_profile(profiles::tangled_shore::kActivity,9,14,{38,63},{39,60},13,8,730);
}
// Every published generic patrol, including newly added sibling slots and the
// last slot of the full Tangled Shore profile, keeps its exact quota on renewal.
void all_destination_sources() {
    for(const auto* definition:a::open_world::profiles::kActivities) {
        if(definition==&a::open_world::profiles::dreaming_city::kActivity)continue;
        const a::population::Owner owner{500,{1}};
        const auto ordinary=definition->populations.first(
            definition->openWorld->authored->populations.size());
        a::population::Service service;CHECK(service.begin(owner,definition->populations,99));
        a::open_world::Director director;
        CHECK(director.begin(owner,99,*definition->openWorld,ordinary));
        std::array<c::NativePopulationLedger<64>,a::population::kSourceCapacity> ledgers{};
        std::array<std::uint8_t,a::population::kSourceCapacity> pending{};
        const auto tick=[&](std::uint64_t now,std::uint32_t bubble) {
            return director.update(now,bubble,true,service,std::span<const c::NativePopulationLedger<64>>(ledgers),pending);
        };
        CHECK(tick(1,63));CHECK(service.project_retained().count==0);
        for(std::uint32_t bubble=0;bubble<63;++bubble)CHECK(tick(2+bubble,bubble));
        CHECK(service.project_retained().count==ordinary.size());
        for(std::size_t index=0;index<ordinary.size();++index) {
            const auto& binding=definition->openWorld->authored->populations[index];
            if(binding.kind==a::open_world::authored::PopulationKind::npc)continue;
            const auto& cap=ordinary[index];
            c::Asset asset{cap.registry->key,0,1,cap.slot};
            for(const auto& slot:cap.registry->slots)if(slot.index==cap.slot)asset.definition=slot.descriptorTag;
            const c::PopulationOwner source{500,99,1,asset,1};
            auto& ledger=ledgers[index];CHECK(ledger.begin(source));
            std::uint32_t actorId=1;
            for(std::uint8_t lane=0;lane<cap.categories;++lane) {
                const auto count=lane?service.second_target(index):service.target(index);
                for(std::uint32_t member=0;member<count;++member) {
                    const c::PopulationActor actor{source,actorId,actorId+100};++actorId;
                    CHECK(ledger.admitted(actor,lane)==c::PopulationIntake::accepted);
                    CHECK(ledger.died(actor)==c::PopulationIntake::accepted);
                    CHECK(ledger.actor_retired(actor)==c::PopulationIntake::accepted);
                }
            }
            sunrise::middleware::bap::activity_message::sense_update::SenseObject observed{};
            observed.registryKey=cap.registry->key;observed.slotIndex=cap.slot;observed.slotType=1;
            observed.hasNativeSchema=true;observed.nativeSchema=0x80807ECC;observed.nativeRevision=1;
            observed.hasRootDelta=true;observed.sourceDelta.present=1;observed.sourceDelta.scalar[0]=1;
            observed.sourceDelta.consumedPresent=true;observed.sourceDelta.consumedCount=cap.categories;
            observed.sourceDelta.consumed[0]=service.target(index);
            observed.sourceDelta.consumed[1]=service.second_target(index);
            CHECK(service.observe_retained(observed));CHECK(service.consumed(index));
        }
        CHECK(tick(1000,63));CHECK(tick(30999,63));
        for(std::size_t i=0;i<ordinary.size();++i)CHECK(!service.renewal(i).pending);
        CHECK(tick(31000,63));
        for(std::size_t i=0;i<ordinary.size();++i) {
            const auto& binding=definition->openWorld->authored->populations[i];
            const auto& renewal=service.renewal(i);
            if(binding.kind==a::open_world::authored::PopulationKind::npc) {
                CHECK(!renewal.pending && service.target(i)==1);continue;
            }
            CHECK(renewal.pending && renewal.nextTarget==binding.requestOverride
                && renewal.nextSecondTarget==binding.secondRequestOverride);
            auto prior=ledgers[i].owner(),next=prior;++next.generation;
            CHECK(ledgers[i].renew(prior,next));CHECK(service.commit_renewal(i));
            CHECK(service.generation(i)==2 && service.target(i)==binding.requestOverride
                && service.second_target(i)==binding.secondRequestOverride);
        }
        CHECK(tick(31001,63));
        CHECK(service.project_retained().count==ordinary.size());
    }
}
}
int main(){deadlines();categories();mars_survivors();held_region_prefetch_profiles();all_destination_sources();std::printf("PASS %u patrol replenishment checks\n",checks);}
