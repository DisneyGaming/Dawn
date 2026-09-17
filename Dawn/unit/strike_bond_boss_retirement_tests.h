#pragma once
#include "strike_bond_fire_trace_tests.h"
#include "../src/client/hooks/bootflow/strike_bond_boss_retirement.h"

namespace strike_bond_retirement_fixture {
namespace policy=dawn::client::hooks::bootflow::strike_bond_boss_retirement;
struct Fixture : strike_bond_fire_fixture::Fixture {
    static constexpr std::uintptr_t parent=0xE00000;
    static constexpr std::uint32_t parentSelf=0x6014;
    policy::Lease lease{};
    Fixture() {
        put<std::uint16_t>(actor,0);
        put<std::uint32_t>(actor+0x3C,0x80809A3BU);
        put<std::uint32_t>(actor+0x50,parentSelf);
        put<std::uint32_t>(entity+0x0C,5);
        handles[parentSelf]=parent;
        put<std::uint32_t>(parent+4,0x808082ECU);
        put<std::uint32_t>(parent+0x24,parentSelf);
        put<std::uint32_t>(parent+0x2C,5);
        put<std::uint32_t>(parent+0x1470,3);
        put<std::uint32_t>(image+0x1F9344C,42);
        put<std::uintptr_t>(image+0x2744A18,0xF00000);
        for(const auto offset:{0x30U,0x48U}) {
            put<std::uint32_t>(source+offset,request.enemy.owner);
            put<std::uint32_t>(source+offset+4,0x80809A3BU);
            put<std::int64_t>(source+offset+8,0);
        }
        auto& state=request.frame.native[policy::mission::asset_index(policy::mission::find(request.enemy.registry,1,3)->asset)];
        state.managed=state.active=state.desired=true;state.generation=request.enemy.generation;
    }
    bool capture() {
        policy::trace::Identity identity{};
        return policy::trace::sample(*this,image,body,false,request,identity)
            && policy::capture(*this,image,request,identity,lease);
    }
    void die() {
        request.frame.bossDead=true;
        auto& state=request.frame.native[policy::mission::asset_index(policy::mission::find(request.enemy.registry,1,3)->asset)];
        state.active=state.desired=false;++state.generation;
        put<std::uint16_t>(actor,0x20);
        put<std::uint32_t>(actor+0x38,UINT32_MAX);put<std::int64_t>(actor+0x40,0);
    }
    bool validates() {return policy::validate(*this,image,request,source,lease);}
};
}

inline bool strike_bond_boss_retirement_contracts() {
    using namespace strike_bond_retirement_fixture;
#define GARDEN_RETIRE_CHECK(expression) do {if(!(expression)) {std::fprintf(stderr,"FAIL Garden retirement line %d: %s\n",__LINE__,#expression);return false;}} while(false)
    Fixture alive;GARDEN_RETIRE_CHECK(alive.capture());
    GARDEN_RETIRE_CHECK(!policy::requested(alive.request,alive.lease) && !alive.validates());
    Fixture dead=alive;dead.die();
    // Native death detaches the source, but the retained entity remains owned
    // by this exact accepted actor. The callback precedes +244's update.
    GARDEN_RETIRE_CHECK(dead.validates());
    GARDEN_RETIRE_CHECK(policy::entity_state(dead,Fixture::image,dead.lease)==policy::EntityState::live);
    {auto f=dead;f.put<std::uint32_t>(Fixture::source+0x1FC,8);GARDEN_RETIRE_CHECK(f.validates());}
    {auto f=dead;f.put<std::uint32_t>(Fixture::source+0x244,8);GARDEN_RETIRE_CHECK(!f.validates());}
    {auto f=dead;f.request.frame.bossDead=false;GARDEN_RETIRE_CHECK(!f.validates());}
    {auto f=dead;f.request.frame.enabled=false;GARDEN_RETIRE_CHECK(!f.validates());}
    {auto f=dead;f.request.frame.finished=true;GARDEN_RETIRE_CHECK(!f.validates());}
    {auto f=dead;++f.request.owner.run;GARDEN_RETIRE_CHECK(!f.validates());}
    {auto f=dead;++f.request.owner.value;GARDEN_RETIRE_CHECK(!f.validates());}
    {auto f=dead;++f.request.enemy.actor;GARDEN_RETIRE_CHECK(!f.validates());}
    {auto f=dead;
        f.request.frame.native[policy::mission::asset_index(policy::mission::find(f.request.enemy.registry,1,3)->asset)].desired=true;
        GARDEN_RETIRE_CHECK(!f.validates());
    }
    {auto f=dead;f.put<std::uint16_t>(Fixture::actor,0);GARDEN_RETIRE_CHECK(!f.validates());}
    {auto f=dead;f.put<std::uint32_t>(Fixture::actor+0x38,999);GARDEN_RETIRE_CHECK(!f.validates());}
    {auto f=dead;f.put<std::uint32_t>(Fixture::actor+0x38,f.request.enemy.owner);GARDEN_RETIRE_CHECK(f.validates());}
    for(const auto address:{Fixture::actor+0x48,Fixture::actor+0x4C,Fixture::actor+0x50,
        Fixture::parent+0x24,Fixture::parent+0x2C,Fixture::parent+0x1470,Fixture::reference+0x24,
        Fixture::reference+0x60,Fixture::body+0x24,Fixture::body+0x2C}) {
        auto f=dead;std::uint32_t old{};GARDEN_RETIRE_CHECK(f.value(address,old));
        f.put<std::uint32_t>(address,old^0x2000U);GARDEN_RETIRE_CHECK(!f.validates());
    }
    for(const auto address:{Fixture::source,Fixture::source+4,Fixture::source+0x30,Fixture::source+0x48,
        Fixture::sourceBase+0x758,Fixture::parent+4,Fixture::reference+4,Fixture::body,Fixture::body+4}) {
        auto f=dead;f.put<std::uint32_t>(address,0);GARDEN_RETIRE_CHECK(!f.validates());
    }
    {auto f=dead;f.put<std::int64_t>(Fixture::source+8,0x738);GARDEN_RETIRE_CHECK(!f.validates());}
    {auto f=dead;f.put<std::uint8_t>(Fixture::sourceBase+0x75C,2);GARDEN_RETIRE_CHECK(!f.validates());}
    {auto f=dead;f.put<std::uint16_t>(Fixture::sourceBase+0x75E,4);GARDEN_RETIRE_CHECK(!f.validates());}
    {auto f=dead;f.put<std::uint32_t>(Fixture::entity+4,4);
        GARDEN_RETIRE_CHECK(policy::entity_state(f,Fixture::image,f.lease)==policy::EntityState::marked && !f.validates());
    }
    for(const auto address:{Fixture::entity+0x0C,Fixture::entity+0x4C}) {
        auto f=dead;f.put<std::uint32_t>(address,123);
        GARDEN_RETIRE_CHECK(policy::entity_state(f,Fixture::image,f.lease)==policy::EntityState::replaced && !f.validates());
    }
    {auto f=dead;f.put<std::uint32_t>(Fixture::image+0x1F9344C,43);
        GARDEN_RETIRE_CHECK(policy::entity_state(f,Fixture::image,f.lease)==policy::EntityState::otherWorld && !f.validates());
    }
    {auto f=dead;f.put<std::uintptr_t>(Fixture::image+0x2744A18,0xF10000);GARDEN_RETIRE_CHECK(!f.validates());}
    {auto f=dead;f.bytes.erase(Fixture::entity+0x0C);
        GARDEN_RETIRE_CHECK(policy::entity_state(f,Fixture::image,f.lease)==policy::EntityState::unreadable && !f.validates());
    }
    {auto f=alive;f.put<std::uint16_t>(Fixture::actor,0x20);GARDEN_RETIRE_CHECK(!f.capture());}
    {auto f=alive;f.put<std::uint32_t>(Fixture::entity+4,4);GARDEN_RETIRE_CHECK(!f.capture());}
    {auto f=alive;f.put<std::uint32_t>(Fixture::parent+0x1470,4);GARDEN_RETIRE_CHECK(!f.capture());}
#undef GARDEN_RETIRE_CHECK
    return true;
}
