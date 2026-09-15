#include "../src/server/runtime/activity/open_world_runtime.h"
#include "../src/state/activity/native_population_events.h"
#include "../src/client/hooks/bootflow/native_population_pending.h"
#include "../src/client/hooks/bootflow/native_population_streaming.h"
#include <array>
#include <cstdio>

namespace activity=sunrise::server::runtime::activity;
namespace authored=sunrise::state::activity::coo::open_world;
namespace events=sunrise::state::activity::native_population;
namespace native=sunrise::middleware::bap::activity_message::native;
namespace pending=sunrise::client::hooks::bootflow::native_population_pending;
namespace wire=sunrise::middleware::bap::activity_message::sensor_auth_update;

namespace {
unsigned checks{};
#define CHECK(value) do { ++checks; if(!(value)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#value); return 1; } } while(false)

struct Fixture final {
    static constexpr std::size_t count=activity::population::kSourceCapacity;
    static constexpr std::size_t groups=86;
    std::array<std::array<sunrise::state::activity::coo::registry::Slot,3>,groups> slots{};
    std::array<sunrise::state::activity::coo::registry::Definition,groups> registries{};
    std::array<activity::population::Capability,count> capabilities{};
    std::array<authored::PopulationBinding,count> bindings{};
    authored::Destination destination{};
    activity::open_world::Definition definition{};
    Fixture() {
        for(std::size_t group=0;group<groups;++group) {
            const auto width=(count+groups-1-group)/groups;
            for(std::size_t slot=0;slot<width;++slot)
                slots[group][slot]={static_cast<std::uint16_t>(slot),1,0x80809A3BU,0x80807ECCU,
                    0x80807EC9U,static_cast<std::uint32_t>(0x81000000U+group*3+slot)};
            registries[group]={"capacity",0x80F00001U,static_cast<std::uint32_t>(0x10000000U+group),
                static_cast<std::uint32_t>(0x82000000U+group),static_cast<std::uint32_t>(0x20000000U+group),
                0,std::span(slots[group]).first(width)};
        }
        for(std::size_t i=0;i<count;++i) {
            const auto group=i%groups;const auto slot=i/groups;
            capabilities[i]={&registries[group],static_cast<std::uint16_t>(slot),0,{},false,0,1};
            bindings[i]={static_cast<std::uint16_t>(group),static_cast<std::uint16_t>(slot),0,0,-1,
                authored::PopulationKind::patrol,false,0,1,0,1};
        }
        destination={"Capacity","capacity",0x80F00001U,0,registries,bindings,{},{}};
        definition={&destination};
    }
};

events::Lease lease(std::uint16_t slot=0) {
    const sunrise::state::activity::ActivityInstanceKey activityKey{0x1234U,{7}};
    return {activityKey,{activityKey.sessionId,9,activityKey.incarnation.value,
        {0x81000001U,0x10000001U,1,slot},1},0,true};
}
}

int main() {
    static_assert(native::population::kSourceCapacity==256);
    static_assert(activity::open_world::kRetainedRequestCapacity==384);
    static_assert(events::kCreationCapacity==1152 && events::kProvisionalCapacity==1152);
    static_assert(events::kEventCapacity==3456);
    static_assert(events::kBindingCapacity==320);
    static_assert(sunrise::client::hooks::bootflow::native_population_streaming::kSourceCapacity==320);
    static_assert(wire::kGroupCapacity==96);
    static_assert(sunrise::state::build_data::scenarios::kDestinationWireGroupCapacity==96);

    Fixture fixture;
    CHECK(activity::open_world::valid(fixture.definition));
    activity::population::Service service;
    const activity::population::Owner owner{0x5000U,{11}};
    CHECK(service.begin(owner,fixture.capabilities,99));
    std::array<activity::population::Capability,Fixture::count+1> tooMany{};
    std::copy(fixture.capabilities.begin(),fixture.capabilities.end(),tooMany.begin());
    tooMany.back()=fixture.capabilities.front();
    activity::population::Service rejected;
    CHECK(!rejected.begin(owner,tooMany,99));

    activity::open_world::Director director;
    CHECK(director.begin(owner,99,fixture.definition,fixture.capabilities));
    auto budgetBindings=fixture.bindings;
    for(std::size_t i=0;i<128;++i)budgetBindings[i].requestOverride=2;
    for(std::size_t i=128;i<Fixture::count;++i)budgetBindings[i].requestOverride=1;
    fixture.destination.populations=budgetBindings;
    activity::open_world::Director maximum;
    CHECK(maximum.begin(owner,99,fixture.definition,fixture.capabilities));
    budgetBindings.back().requestOverride=2;
    activity::open_world::Director overBudget;
    CHECK(!overBudget.begin(owner,99,fixture.definition,fixture.capabilities));

    native::population::Batch batch{};
    batch.count=batch.entries.size();
    for(std::size_t i=0;i<batch.count;++i) {
        const auto& capability=fixture.capabilities[i];
        batch.entries[i].slot=capability.slot;
        batch.entries[i].bubble=capability.registry->bubble;
        batch.entries[i].source={capability.registry->key,1,0,1,{}};
        batch.entries[i].source.hasSpawnRule=false;
    }
    CHECK(native::population::find(batch,fixture.capabilities.back().registry->key,1,
        fixture.capabilities.back().slot)==&batch.entries.back());
    CHECK(native::population::bits(batch.entries.back())==native::combatant_source::kSourceBits);
    auto invalidBatch=batch;invalidBatch.count=invalidBatch.entries.size()+1;
    CHECK(native::population::find(invalidBatch,fixture.registries.front().key,1,0)==nullptr);

    std::array<std::array<std::uint8_t,3>,Fixture::groups> types{},flags{};
    std::array<std::array<std::uint16_t,3>,Fixture::groups> indices{};
    std::array<std::uint32_t,Fixture::groups> keys{};
    wire::Snapshot snapshot{};snapshot.lifetime=3;snapshot.region=0;snapshot.hasRegion=true;
    snapshot.populations=batch;snapshot.roster.groupCount=Fixture::groups;
    for(std::size_t i=0;i<Fixture::groups;++i) {
        const auto width=fixture.registries[i].slots.size();keys[i]=fixture.registries[i].key;
        for(std::size_t slot=0;slot<width;++slot) {
            types[i][slot]=1;flags[i][slot]=2;indices[i][slot]=static_cast<std::uint16_t>(slot);
        }
        snapshot.roster.groups[i]={keys[i],std::span(types[i]).first(width),
            std::span(flags[i]).first(width),std::span(indices[i]).first(width)};
    }
    std::array<wire::BubbleSubBlock,1> blocks{{{0,keys}}};
    snapshot.roster.bubbleSubBlocks=blocks;
    std::array<std::byte,256*1024> encoded{};std::size_t written{};
    CHECK(native::population::valid(snapshot.populations,snapshot.roster,0));
    CHECK(wire::legacy_encode_sensor_auth_update(snapshot,encoded,written) && written>0);
    const auto fullWireBytes=written;
    auto tooManyGroups=snapshot;tooManyGroups.roster.groupCount=wire::kGroupCapacity+1;written=77;
    CHECK(!wire::legacy_encode_sensor_auth_update(tooManyGroups,encoded,written) && written==0);

    events::Mailbox bindings;
    for(std::size_t i=0;i<events::kBindingCapacity;++i)CHECK(bindings.bind(lease(static_cast<std::uint16_t>(i))));
    CHECK(bindings.capture(lease(static_cast<std::uint16_t>(events::kBindingCapacity-1))));
    CHECK(!bindings.bind(lease(static_cast<std::uint16_t>(events::kBindingCapacity))));

    events::Mailbox creations;CHECK(creations.bind(lease()));
    std::array<events::Creation,events::kCreationCapacity> tickets{};
    for(auto& ticket:tickets){ticket=creations.begin_creation();CHECK(ticket);}
    CHECK(!creations.begin_creation());
    for(const auto ticket:tickets)creations.cancel(ticket);

    events::Mailbox provisionals;CHECK(provisionals.bind(lease()));
    const auto receipt=provisionals.capture(lease());
    for(std::size_t i=0;i<events::kProvisionalCapacity;++i) {
        const auto creation=provisionals.begin_creation();CHECK(creation);
        events::Event event{lease(),{lease().source,static_cast<std::uint32_t>(i),
            static_cast<std::uint32_t>(i+1),creation.nonce},0x44U,events::Kind::admitted};
        events::Receipt staged{};CHECK(provisionals.stage(creation,event,staged)==events::StageResult::staged);
        CHECK(staged==receipt);
    }
    const auto overflowCreation=provisionals.begin_creation();CHECK(overflowCreation);
    events::Event overflowEvent{lease(),{lease().source,0xFFFFU,0xFFFFU,overflowCreation.nonce},
        0x44U,events::Kind::admitted};events::Receipt staged{};
    CHECK(provisionals.stage(overflowCreation,overflowEvent,staged)==events::StageResult::rejected);

    events::Mailbox queue;CHECK(queue.bind(lease()));const auto queueReceipt=queue.capture(lease());
    events::Event death{lease(),{lease().source,1,2,3},0x44U,events::Kind::died};
    for(std::size_t i=0;i<events::kEventCapacity;++i)CHECK(queue.submit(death,queueReceipt));
    CHECK(!queue.submit(death,queueReceipt));CHECK(queue.overflow());

    pending::Queue<events::kProvisionalCapacity> clientPending;
    auto admitted=death;admitted.kind=events::Kind::admitted;
    pending::Birth birth{admitted,9,queueReceipt};
    for(std::size_t i=0;i<events::kProvisionalCapacity;++i) {
        birth.event.actor.actor=static_cast<std::uint32_t>(i);
        birth.event.actor.birthNonce=static_cast<std::uint64_t>(i+1);
        CHECK(clientPending.add(birth)==pending::Intake::accepted);
    }
    birth.event.actor.actor=0xFFFFFFFEU;birth.event.actor.birthNonce=0xFFFFFFFFULL;
    CHECK(clientPending.add(birth)==pending::Intake::overflow);

    std::printf("PASS %u open-world source-capacity checks wire=%zu service=%zu director=%zu snapshot=%zu roster_group=%zu\n",
        checks,fullWireBytes,
        sizeof(activity::population::Service),sizeof(activity::open_world::Director),
        sizeof(wire::Snapshot),sizeof(sunrise::state::build_data::scenarios::RosterGroup));
}
