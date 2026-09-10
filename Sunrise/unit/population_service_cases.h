#pragma once
#include "server/runtime/activity/mercury_populations.h"

void population_service_cases() {
    namespace p=sunrise::server::runtime::activity::population;
    namespace m=sunrise::server::runtime::activity::mercury;
    p::Service service;
    CHECK(service.begin({42,{7}},m::kPopulations));
    CHECK(!service.begin({42,{7}},m::kPopulations));
    CHECK(service.project(15).count==0);
    p::Command command{};
    CHECK(p::parse("v2 1 2A 7 1 1 74337EDD 0 1\n",command));
    p::Service restarted;CHECK(restarted.begin({42,{7}},m::kPopulations,2));
    CHECK(restarted.request(command,15)==p::Result::stale);
    CHECK(restarted.project(15).count==0);
    CHECK(!p::parse("v1 2A 7 1 1 74337EDD 0 1",command));
    CHECK(!p::parse("v2 0 2A 7 1 1 74337EDD 0 1",command));
    CHECK(!p::parse("v2 FFFFFFFFFFFFFFFFF 2A 7 1 1 74337EDD 0 1",command));
    CHECK(service.request(command,14)==p::Result::stale);
    CHECK(service.request(command,15)==p::Result::accepted);
    CHECK(service.revision()==2);
    auto batch=service.project(15);CHECK(batch.count==1);
    CHECK(batch.entries[0].source.looseRequested==1 && batch.entries[0].source.generation==7);
    CHECK(batch.entries[0].source.ruleSlot==8 && batch.entries[0].source.tactical.row==0);
    CHECK(service.project(14).count==0);
    sunrise::middleware::bap::activity_message::sense_update::SenseObject sense{};
    sense.registryKey=0x74337EDD;sense.slotType=1;sense.slotIndex=0;
    sense.hasNativeSchema=true;sense.nativeSchema=0x80807ECC;
    sense.hasRootDelta=true;sense.nativeRevision=3;
    sense.sourceDelta.present=1;sense.sourceDelta.scalar[0]=6;
    CHECK(!service.observe(15,sense)); // old generation
    sense.sourceDelta.scalar[0]=7;
    CHECK(!service.observe(14,sense)); // other bubble
    const auto* observation=service.observe(15,sense);
    CHECK(observation && observation->known==1 && !observation->consumedKnown);
    CHECK(!service.observe(15,sense)); // duplicate packet on second binding
    sense.nativeRevision=4;sense.sourceDelta={};
    sense.sourceDelta.present=8;sense.sourceDelta.scalar[3]=1;
    observation=service.observe(15,sense);
    CHECK(observation && observation->scalar[0]==7 && observation->scalar[3]==1);
    sense.nativeRevision=3;CHECK(!service.observe(15,sense)); // reordered packet
    sense.nativeRevision=5;sense.sourceDelta={};
    sense.sourceDelta.consumedPresent=true;sense.sourceDelta.consumedCount=1;
    sense.sourceDelta.consumed[0]=1;
    observation=service.observe(15,sense);
    CHECK(observation && observation->consumedKnown && observation->scalar[3]==1);
    // A consumed request never synthesizes an associated-count decrement or kill.
    CHECK(service.request(command,15)==p::Result::stale);
    command.expectedRevision=2;
    CHECK(service.request(command,15)==p::Result::duplicate);
    command.request=2;command.requested=2;
    CHECK(service.request(command,15)==p::Result::accepted);
    command.expectedRevision=3;command.request=3;command.requested=1;
    CHECK(service.request(command,15)==p::Result::decrease);
    command.slot=17;CHECK(service.request(command,15)==p::Result::unsupported);
    command.registry=0x564C6ECE;command.slot=0;
    CHECK(service.request(command,15)==p::Result::accepted);
    batch=service.project(15);CHECK(batch.count==2);
    CHECK(!batch.entries[1].source.hasSpawnRule && batch.entries[1].source.tactical.row==-1);
    for(const auto text:{"v2 1 2A 7 1 1 74337EDD 0 0","v2 1 2A 7 1 1 74337EDD 0 64",
        "v2 1 2A 7 1 1 74337EDD 0 1 extra","v2 1 2A 7 1 1 74337EDD 0",
        "v2 1 0 7 1 1 74337EDD 0 1","v2 1 2A 0 1 1 74337EDD 0 1",
        "v2 1 2A 7 -1 1 74337EDD 0 1","v2 1 2A 7 1 1 FFFFFFFFF 0 1"})
        CHECK(!p::parse(text,command));
    CHECK(service.revision()==4 && service.last_request()==3);
    p::Service invalid;
    CHECK(!invalid.begin({0,{1}},m::kPopulations));
    CHECK(!invalid.begin({42,{0x80000000ULL}},m::kPopulations));
    auto malformed=m::kPopulations;malformed[0].rule=7;
    CHECK(!invalid.begin({43,{1}},malformed));
    malformed=m::kPopulations;malformed[0].tactical.slot=3;
    CHECK(!invalid.begin({43,{1}},malformed));
    malformed=m::kPopulations;malformed[1]=malformed[0];
    CHECK(!invalid.begin({43,{1}},malformed));
}
