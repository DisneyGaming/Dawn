#pragma once
#include "state/gameplay/replication_roles.h"
#include "client/hooks/bootflow/native_replication_bindings.h"
void native_replication_role_cases() {
    namespace r=sunrise::state::gameplay::replication;
    namespace b=sunrise::client::hooks::bootflow::replication_bindings;
    r::Roles roles;r::Address address{};
    address[0]=address[30]=127;address[3]=address[33]=1;address[4]=address[34]=0;
    address[5]=address[35]=121;address[40]=1;
    CHECK(!roles.publish(91,address)); // Disabled/external topology cannot publish.
    roles.begin(10);CHECK(roles.publish(91,address));CHECK(roles.lookup(91,address)==10);
    CHECK(roles.publish(91,address));CHECK(!roles.lookup(92,address)); // Same endpoint, real different peer.
    auto remote=address;remote[3]=2;
    CHECK(!roles.lookup(91,remote));CHECK(!roles.publish(91,remote)); // No identity reclassification.
    remote=address;remote[85]=6;CHECK(!roles.lookup(91,remote)); // Relay/direct mismatch.
    roles.begin(11);CHECK(!roles.lookup(91,address));CHECK(roles.publish(91,address));
    CHECK(roles.lookup(91,address)!=10); // Old native connection epoch cannot authorize exclusion.
    roles.begin(0);CHECK(!roles.lookup(91,address));CHECK(!roles.publish(91,address));
    b::Excluded bindings;
    for(const auto index:{0U,7U,30U}) {
        CHECK(bindings.remember({100,200+index,index},0,0));
        CHECK(bindings.remember({100,200+index,index},0,0)); // Exact duplicate is idempotent.
        CHECK(!bindings.remember({100,900+index,index},0,0)); // Reused slot is a different peer.
    }
    CHECK(bindings.size()==3);
    CHECK(!bindings.remove(101,0,0,0));CHECK(bindings.size()==3);
    CHECK(!bindings.remove(100,7,444,1U<<7)); // Native slot now occupied: forward its native removal.
    CHECK(bindings.size()==2);
    CHECK(bindings.remove(100,30,0,0));CHECK(bindings.remove(100,0,0,0));
    CHECK(bindings.size()==0);CHECK(!bindings.remove(100,0,0,0));
    CHECK(!bindings.remember({100,200,31},0,0));CHECK(!bindings.remember({100,200,0},300,0));
    CHECK(!bindings.remember({100,200,0},0,1));CHECK(!bindings.remember({0,200,0},0,0));
    for(unsigned i=0;i<128;++i)CHECK(bindings.remember({100+i,200+i,5},0,0));
    CHECK(!bindings.remember({999,888,5},0,0)); // Capacity exhaustion forwards native registration.
    for(unsigned i=0;i<128;++i)CHECK(bindings.remove(100+i,5,0,0));
    CHECK(bindings.size()==0);
}
