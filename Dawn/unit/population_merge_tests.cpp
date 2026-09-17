#include "server/runtime/activity/population_service.h"
#include "state/activity/native_population_events.h"
#include <cstdio>

namespace p=dawn::server::runtime::activity::population;
namespace r=dawn::server::runtime::activity::registry;
namespace s=dawn::middleware::bap::activity_message::sense_update;
unsigned checks{};
#define CHECK(value) do { ++checks; if(!(value)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#value); return 1; } } while(false)

constexpr r::Slot slots[]{
    {0,1,1,0,0x80807EC9U,1}, {7,66,1,0,0,2},
    {42,2,0x8080834EU,0x80807DA2U,0x80807DA1U,3}
};
const r::Definition definition{"merge",1,0x12345678U,2,3,7,std::span<const r::Slot>(slots)};

s::SenseObject report(std::uint32_t generation,std::int32_t first,std::int32_t second) {
    s::SenseObject value{};value.registryKey=definition.key;value.slotType=1;
    value.hasNativeSchema=true;value.nativeSchema=0x80807ECC;value.hasRootDelta=true;
    value.nativeRevision=1;value.sourceDelta.present=1;value.sourceDelta.scalar[0]=generation;
    value.sourceDelta.consumedPresent=true;value.sourceDelta.consumedCount=2;
    value.sourceDelta.consumed[0]=first;value.sourceDelta.consumed[1]=second;return value;
}

int main() {
    const p::Owner owner{99,{7}};
    const p::Capability patrol{&definition,0,7,{},true,0,false,42,2};
    p::Service service;CHECK(service.begin(owner,std::span(&patrol,1),11));
    p::Command command{owner,1,1,definition.key,0,3,11,2};
    CHECK(service.request(command,7)==p::Result::accepted);
    command.expectedRevision=service.revision();
    auto changed=command;changed.secondRequested=3;
    CHECK(service.request(changed,7)==p::Result::stale); // A changed second count is not a historical duplicate.
    CHECK(service.request(command,7)==p::Result::duplicate);
    for(unsigned i=0;i<100;++i) {
        command.expectedRevision=service.revision();command.request=service.last_request()+1;
        CHECK(service.top_up(command,7)==p::Result::accepted);
    }
    CHECK(service.target(0)==303 && service.second_target(0)==202);
    CHECK(service.status(definition.key,0)->requested==303);
    CHECK(service.source_request(0)==101 && service.generation(0)==7);
    const auto projected=service.project_retained();
    CHECK(projected.count==1 && projected.entries[0].namedMember==42);
    CHECK(projected.entries[0].source.looseRequested==303 && projected.entries[0].source.secondRequested==202);
    CHECK(p::codec::valid(projected.entries[0].source));
    auto sense=report(7,303,201);
    CHECK(!service.observe(8,sense));
    CHECK(service.observe_retained(sense) && !service.consumed(0));
    ++sense.nativeRevision;sense.sourceDelta.consumed[1]=202;
    CHECK(service.observe_retained(sense) && service.consumed(0));
    CHECK(service.observation(0) && !service.observation(1));
    command.expectedRevision=service.revision();command.request=service.last_request()+1;
    CHECK(service.renew(command,7)==p::Result::accepted);
    CHECK(service.renewal(0).target==303 && service.renewal(0).secondTarget==202);
    CHECK(service.commit_renewal(0));
    CHECK(service.generation(0)==8 && service.target(0)==3 && service.second_target(0)==2);
    CHECK(service.source_request(0)==command.request && !service.observation(0));
    CHECK(!service.observe_retained(sense));

    // Cycle ownership remains explicit even on an authored two-category source.
    auto cycle=patrol;cycle.allowCycles=true;
    p::Service cycling;CHECK(cycling.begin(owner,std::span(&cycle,1),11));
    command={owner,1,1,definition.key,0,3,11,2};
    CHECK(cycling.request(command,7)==p::Result::accepted);
    command.expectedRevision=2;command.request=2;
    CHECK(cycling.replenish(command,7)==p::Result::notAllowed);
    CHECK(cycling.renew(command,7)==p::Result::exhausted);
    CHECK(cycling.request_retirement(owner,11,2,2,definition.key,0)==p::Result::accepted);
    const auto retired=cycling.project_retained().entries[0];
    CHECK(retired.namedMember==42 && retired.source.retireOwned && retired.source.hasSecondCategory);
    CHECK(!retired.source.looseRequested && !retired.source.secondRequested && p::codec::valid(retired.source));
    CHECK(!cycling.retirement_acknowledged(owner,11,definition.key,0,8,true));
    sense=report(8,0,0);CHECK(cycling.observe_retained(sense));
    CHECK(cycling.retirement_acknowledged(owner,11,definition.key,0,8,true));
    CHECK(cycling.begin_cycle(owner,11,3,3,definition.key,0,1,4)==p::Result::accepted);
    CHECK(cycling.generation(0)==9 && cycling.target(0)==4 && cycling.second_target(0)==0);
    CHECK(cycling.source_request(0)==3 && cycling.cycle(definition.key,0)==1);

    // Vendor completion must identify the exact native birth after handle reuse.
    namespace events=dawn::state::activity::native_population;
    static events::Mailbox mailbox;
    const events::Lease lease{owner,{99,11,7,{definition.key,2,1,0},7},7};
    CHECK(mailbox.bind(lease));
    const auto firstBirth=mailbox.begin_creation();CHECK(firstBirth.nonce);
    events::Event first{lease,{lease.source,100,101,firstBirth.nonce},9,events::Kind::admitted};
    events::Receipt firstReceipt{};
    CHECK(mailbox.stage(firstBirth,first,firstReceipt)==events::StageResult::staged);
    CHECK(mailbox.complete_external(firstReceipt,first));
    const auto nextBirth=mailbox.begin_creation();CHECK(nextBirth.nonce!=firstBirth.nonce);
    auto next=first;next.actor.birthNonce=nextBirth.nonce;
    events::Receipt nextReceipt{};
    CHECK(mailbox.stage(nextBirth,next,nextReceipt)==events::StageResult::staged);
    CHECK(firstReceipt==nextReceipt); // Same source binding, different actor lifetime.
    CHECK(!mailbox.complete_external(firstReceipt,first));
    CHECK(mailbox.provisional(nextReceipt,next));
    CHECK(mailbox.complete_external(nextReceipt,next));
    CHECK(!mailbox.pending(owner)); // External completion queues no mission event.
    std::printf("PASS: %u checks; merged population categories, renewal, named member and cycles\n",checks);
    std::printf("Footprint: Service=%zu bytes; Batch=%zu bytes\n",sizeof(p::Service),sizeof(p::wire::Batch));
    return 0;
}
