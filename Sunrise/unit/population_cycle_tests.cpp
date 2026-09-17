#include "server/runtime/activity/population_service.h"
#include "middleware/bap/activity_message/native/population_authority.h"
#include "middleware/encoding/bit_reader.h"
#include "middleware/encoding/bit_writer.h"
#include "state/activity/coo/native_combatant_authority.h"
#include "state/activity/native_population_events.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace rt=sunrise::server::runtime::activity::population;
namespace wire=sunrise::middleware::bap::activity_message::native::population;
namespace bits=sunrise::middleware::encoding::bits;
namespace coo=sunrise::state::activity::coo;
namespace registry=sunrise::state::activity::coo::registry;
namespace sense=sunrise::middleware::bap::activity_message::sense_update;
unsigned checks{};
#define CHECK(value) do { ++checks; if(!(value)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#value); return 1; } } while(false)

constexpr std::uint32_t kKey=0x12345678U;
constexpr registry::Slot kSlots[]{
    {0,1,1,0,0x80807EC9U,1},
    {7,66,1,0,0,2},
    {42,2,0x8080834EU,0x80807DA2U,0x80807DA1U,3}
};
const registry::Definition kDefinition{"cycle",1,kKey,2,3,7,std::span<const registry::Slot>(kSlots)};
constexpr registry::Slot kAssignedSlots[]{
    {0,1,1,0,0x80807EC9U,1},
    {7,66,1,0,0,2},
    {42,3,1,0,0x80807F0CU,3}
};
const registry::Definition kAssignedDefinition{"assigned",1,kKey,2,3,7,
    std::span<const registry::Slot>(kAssignedSlots)};

rt::Capability capability(bool cycles=true) {
    return {&kDefinition,0,7,{},true,0,cycles,42};
}
rt::Capability assigned_capability() {
    return {&kAssignedDefinition,0,7,{kKey,42,0},true,0,true};
}
rt::Command command(rt::Owner owner,std::uint64_t revision,std::uint64_t request,
                    std::uint8_t target,std::uint64_t boot=11) {
    return {owner,revision,request,kKey,0,target,boot};
}
sense::SenseObject generation_sense(std::uint32_t generation,std::uint32_t revision) {
    sense::SenseObject object{};object.registryKey=kKey;object.slotType=1;object.slotIndex=0;
    object.hasNativeSchema=true;object.nativeSchema=0x80807ECC;object.hasRootDelta=true;
    object.nativeRevision=revision;object.sourceDelta.present=1;object.sourceDelta.scalar[0]=generation;
    return object;
}

int main() {
    CHECK(rt::valid(capability()));
    std::array<registry::Slot,3> malformed{kSlots[0],kSlots[1],kSlots[2]};malformed[2].authSchema=0x80807DA2U;
    const registry::Definition badDefinition{"cycle",1,kKey,2,3,7,std::span<const registry::Slot>(malformed)};
    CHECK(!rt::valid({&badDefinition,0,7,{},true,0,true,42}));

    // Normal and explicit retirement source bodies differ only at BC.
    wire::Batch normal{};normal.count=1;normal.entries[0].source={kKey,7,7,3,{}};
    std::array<std::byte,96> normalBytes{};bits::Writer normalWriter(normalBytes);
    CHECK(sunrise::middleware::bap::activity_message::native::combatant_source::write_source(normalWriter,normal.entries[0].source));
    CHECK(normalWriter.bit_count()==641);bits::Reader normalReader(normalBytes);CHECK(normalReader.skip(603));
    std::uint64_t field{};CHECK(normalReader.read(2,field) && field==2);
    auto retiring=normal.entries[0].source;retiring.retireOwned=true;retiring.looseRequested=0;
    std::array<std::byte,96> retiredBytes{};bits::Writer retiredWriter(retiredBytes);
    CHECK(sunrise::middleware::bap::activity_message::native::combatant_source::write_source(retiredWriter,retiring));
    CHECK(retiredWriter.bit_count()==641);bits::Reader retiredReader(retiredBytes);CHECK(retiredReader.skip(603));
    CHECK(retiredReader.read(2,field) && field==1);
    retiring.looseRequested=1;bits::Writer invalidRetire(retiredBytes);
    CHECK(!sunrise::middleware::bap::activity_message::native::combatant_source::write_source(invalidRetire,retiring));
    CHECK(invalidRetire.bit_count()==0);

    // Named-member publication and the full retirement gate.
    const rt::Owner owner{99,{7}};static rt::Service service;
    const rt::Capability cap=capability();CHECK(service.begin(owner,std::span(&cap,1),11));
    CHECK(service.project(7).count==0);
    CHECK(service.request(command(owner,1,1,3),7)==rt::Result::accepted);
    auto batch=service.project(7);CHECK(batch.count==1 && batch.entries[0].source.generation==7
        && batch.entries[0].source.looseRequested==3 && !batch.entries[0].source.retireOwned
        && batch.entries[0].namedMember==42);
    CHECK(wire::find_member(batch,kKey,2,42)==&batch.entries[0]);
    CHECK(service.status(kKey,0)->phase==rt::Phase::active && service.generation(kKey,0)==7
        && service.cycle(kKey,0)==0 && !service.native_ack_needed(kKey,0));
    CHECK(service.request_retirement(owner,11,2,2,kKey,0)==rt::Result::accepted);
    batch=service.project(7);CHECK(batch.count==1 && batch.entries[0].source.generation==8
        && batch.entries[0].source.looseRequested==0 && batch.entries[0].source.retireOwned
        && service.status(kKey,0)->phase==rt::Phase::retiring && service.native_ack_needed(kKey,0));
    CHECK(service.request_retirement(owner,11,3,2,kKey,0)==rt::Result::duplicate);
    CHECK(!service.retirement_acknowledged(owner,11,kKey,0,8,false));
    auto observation=generation_sense(8,1);CHECK(service.observe(7,observation));
    CHECK(!service.retirement_acknowledged(owner,11,kKey,0,8,false));
    CHECK(service.retirement_acknowledged(owner,11,kKey,0,8,true));
    CHECK(service.status(kKey,0)->phase==rt::Phase::retired && !service.native_ack_needed(kKey,0));
    batch=service.project(7);CHECK(batch.count==1 && batch.entries[0].source.retireOwned && batch.entries[0].source.looseRequested==0);
    CHECK(service.request(command(owner,3,3,4),7)==rt::Result::notAllowed);
    CHECK(service.begin_cycle(owner,11,3,3,kKey,0,1,2)==rt::Result::accepted);
    batch=service.project(7);CHECK(batch.entries[0].source.generation==9 && batch.entries[0].source.looseRequested==2
        && !batch.entries[0].source.retireOwned && service.cycle(kKey,0)==1);
    auto oldObservation=generation_sense(8,2);CHECK(!service.observe(7,oldObservation));
    observation=generation_sense(9,3);CHECK(service.observe(7,observation));
    CHECK(service.begin_cycle(owner,11,4,3,kKey,0,1,2)==rt::Result::duplicate);
    CHECK(service.begin_cycle(owner,11,4,3,kKey,0,2,2)==rt::Result::stale);
    CHECK(service.request(command({100,{7}},4,5,4),7)==rt::Result::stale);

    // Assigned authored tasks carry the current source generation as root 13,
    // including the initial and recycled generations; unassigned callers keep zero.
    const rt::Owner assignedOwner{100,{7}};const rt::Capability assignedCap=assigned_capability();
    static rt::Service assignedService;CHECK(assignedService.begin(assignedOwner,std::span(&assignedCap,1),11));
    CHECK(assignedService.request({assignedOwner,1,1,kKey,0,2,11},7)==rt::Result::accepted);
    auto assignedBatch=assignedService.project(7);
    CHECK(assignedBatch.count==1 && assignedBatch.entries[0].source.tactical.revision==7);
    CHECK(assignedService.request_retirement(assignedOwner,11,2,2,kKey,0)==rt::Result::accepted);
    assignedBatch=assignedService.project(7);
    CHECK(assignedBatch.count==1 && assignedBatch.entries[0].source.tactical.revision==8);
    auto assignedObservation=generation_sense(8,1);
    CHECK(assignedService.observe(7,assignedObservation));
    CHECK(assignedService.retirement_acknowledged(assignedOwner,11,kKey,0,8,true));
    CHECK(assignedService.begin_cycle(assignedOwner,11,3,3,kKey,0,1,2)==rt::Result::accepted);
    assignedBatch=assignedService.project(7);
    CHECK(assignedBatch.count==1 && assignedBatch.entries[0].source.tactical.revision==9);

    // Wire roster admission sees the exact local type-2 descriptor.
    std::array<std::uint8_t,2> types{1,2},flags{2,3};std::array<std::uint16_t,2> indices{0,42};
    sunrise::middleware::bap::activity_message::sensor_auth_update::Group group{kKey,types,flags,indices};
    std::array<std::uint32_t,1> keys{kKey};
    sunrise::middleware::bap::activity_message::sensor_auth_update::BubbleSubBlock block{7,keys,{},{}};
    sunrise::middleware::bap::activity_message::sensor_auth_update::Roster roster{};roster.groupCount=1;roster.groups[0]=group;roster.bubbleSubBlocks=std::span(&block,1);
    CHECK(wire::valid(batch,roster,56));

    // Recycled two-category mission cohorts retain both authored target budgets.
    auto pairedCapability=capability();pairedCapability.categories=2;
    static rt::Service paired;const rt::Owner pairedOwner{101,{7}};
    CHECK(paired.begin(pairedOwner,std::span(&pairedCapability,1),11));
    CHECK(paired.request({pairedOwner,1,1,kKey,0,2,11,3},7)==rt::Result::accepted);
    CHECK(paired.target(0)==2 && paired.second_target(0)==3);
    CHECK(paired.request_retirement(pairedOwner,11,2,2,kKey,0)==rt::Result::accepted);
    auto pairedObservation=generation_sense(8,1);CHECK(paired.observe(7,pairedObservation));
    CHECK(paired.retirement_acknowledged(pairedOwner,11,kKey,0,8,true));
    CHECK(paired.begin_cycle(pairedOwner,11,3,3,kKey,0,1,4,5)==rt::Result::accepted);
    CHECK(paired.target(0)==4 && paired.second_target(0)==5);

    // Every authored binding fits up to the shared source capacity.
    static std::array<registry::Definition,wire::kPopulationCapacity> definitions{};
    static std::array<rt::Capability,wire::kPopulationCapacity> capabilities{};
    for(std::size_t i=0;i<definitions.size();++i) {
        definitions[i]={"many",1,static_cast<std::uint32_t>(0x2000+i),2,3,1,std::span<const registry::Slot>(kSlots,2)};
        capabilities[i]={&definitions[i],0,7,{},true,false};
    }
    static rt::Service many;CHECK(many.begin({77,{1}},capabilities,11));
    for(std::size_t i=0;i<capabilities.size();++i) {
        rt::Command c{{77,{1}},static_cast<std::uint64_t>(i+1),static_cast<std::uint64_t>(i+1),
            definitions[i].key,0,1,11};CHECK(many.request(c,1)==rt::Result::accepted);
    }
    CHECK(many.project(1).count==wire::kPopulationCapacity);
    CHECK(!many.begin({78,{1}},std::span<const rt::Capability>{},11)); // one service is already bound
    static rt::Service exhausted;
    const auto maxCapability=capability();CHECK(exhausted.begin({78,{0x7FFFFFFFU}},std::span(&maxCapability,1),11));
    CHECK(exhausted.request(command({78,{0x7FFFFFFFU}},1,1,1),7)==rt::Result::accepted);
    CHECK(exhausted.request_retirement({78,{0x7FFFFFFFU}},11,2,2,kKey,0)==rt::Result::exhausted);

    // Fixed type-1 mailbox rebind purges only the old queued lease.
    namespace events=sunrise::state::activity::native_population;
    const events::Lease oldLease{{99,{7}},{99,11,7,{kKey,2,1,0},7},7};
    auto newLease=oldLease;newLease.source.generation=8;events::Mailbox mailbox;CHECK(mailbox.bind(oldLease));
    events::Event oldEvent{oldLease,{oldLease.source,100,101},9,events::Kind::admitted};CHECK(mailbox.submit(oldEvent,mailbox.epoch()));
    CHECK(mailbox.rebind(oldLease,newLease));CHECK(!mailbox.pending(oldLease.activity));
    CHECK(!mailbox.rebind(oldLease,newLease));
    events::Event newEvent{newLease,{newLease.source,102,103},10,events::Kind::admitted};CHECK(mailbox.submit(newEvent,mailbox.epoch()));
    std::array<events::Event,2> drained{};CHECK(mailbox.drain(oldLease.activity,drained)==1 && drained[0].lease==newLease);

    // Generic 42-bit helpers are byte-identical to the qualified CoO codec.
    std::array<std::byte,8> genericBytes{},cooBytes{};bits::Writer genericWriter(genericBytes),cooWriter(cooBytes);
    CHECK(wire::write_member(genericWriter,9,false));CHECK(coo::native_combatant::write_bind(cooWriter,9));
    CHECK(genericWriter.bit_count()==cooWriter.bit_count() && genericBytes==cooBytes);
    genericBytes={};cooBytes={};bits::Writer genericRetire(genericBytes),cooRetire(cooBytes);
    CHECK(wire::write_retire_member(genericRetire,10));CHECK(coo::native_combatant::write_retire_member(cooRetire,10));
    CHECK(genericRetire.bit_count()==cooRetire.bit_count() && genericBytes==cooBytes);

    std::printf("PASS: %u checks; population cycle, named member, source retirement, bounds, and mailbox rebind\n",checks);
    return 0;
}
