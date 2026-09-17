#pragma once
#include "strike_bond_boss_damage_tests.h"
#include "../src/client/hooks/bootflow/strike_bond_fire_trace.h"
#include <functional>
#include <utility>

namespace strike_bond_fire_fixture {
namespace native=dawn::client::hooks::bootflow::strike_bond_fire_trace;
using Base=strike_bond_damage_fixture::Fixture;
struct Fixture : Base {
    static constexpr auto controller=Base::character;
    static constexpr std::uintptr_t body=0xC00000,reference=0xD00000;
    static constexpr std::uint32_t bodySelf=0x2010,referenceSelf=0x4012;
    Fixture() {
        zero(controller,0x30);
        put<std::uint32_t>(controller,0x80F66F56U);put<std::uint32_t>(controller+4,0x80806832U);
        put<std::uint32_t>(controller+0x24,14);put<std::uint32_t>(controller+0x2C,5);
        put<std::uint32_t>(source,0x80F54740U);handles[0x80F54740U]=sourceBase;
        zero(body,0x30);handles[bodySelf]=body;
        put<std::uint32_t>(body,0x80F459A3U);put<std::uint32_t>(body+4,0x80803A00U);
        put<std::uint32_t>(body+0x24,bodySelf);put<std::uint32_t>(body+0x2C,5);
        zero(reference,0x64);handles[referenceSelf]=reference;
        put<std::uint32_t>(reference+4,0x80803A38U);put<std::uint32_t>(reference+0x24,referenceSelf);
        put<std::uint32_t>(reference+0x2C,5);put<std::uint32_t>(reference+0x60,bodySelf);
        put<std::uint32_t>(entity+0x98,referenceSelf);
        put<std::uint32_t>(body+0xAC0,0x35C75F6CU);put<std::uint8_t>(body+0xAC4,1);
    }
    bool accepted(bool fromController=false) {
        native::Identity out{};
        return native::sample(*this,image,fromController?controller:body,fromController,request,out);
    }
};
// Change the synthetic process after one successful read. The qualifier must
// detect that it crossed an ownership change instead of returning mixed state.
struct ChangingRead : Fixture {
    std::uintptr_t changeAfter{};
    std::function<void(Fixture&)> change;
    bool changed{};
    bool copy(std::uintptr_t address,std::span<std::byte> output) {
        const bool ok=Base::copy(address,output);
        if(ok && !changed && address==changeAfter) {changed=true;change(*this);}
        return ok;
    }
    template<class T> bool value(std::uintptr_t address,T& out) {
        return copy(address,std::as_writable_bytes(std::span{&out,1}));
    }
    bool accepted(bool fromController=false) {
        native::Identity out{};
        return native::sample(*this,image,fromController?controller:body,fromController,request,out);
    }
};
}

inline bool strike_bond_fire_trace_contracts() {
    using namespace strike_bond_fire_fixture;
#define GARDEN_FIRE_CHECK(expression) do {if(!(expression)) {std::fprintf(stderr,"FAIL Garden fire trace line %d: %s\n",__LINE__,#expression);return false;}} while(false)
    Fixture good;native::Identity fromBody{},fromController{};
    GARDEN_FIRE_CHECK(native::sample(good,Fixture::image,Fixture::body,false,good.request,fromBody));
    GARDEN_FIRE_CHECK(native::sample(good,Fixture::image,Fixture::controller,true,good.request,fromController));
    const native::Identity expected{Fixture::body,Fixture::bodySelf,5};
    GARDEN_FIRE_CHECK(fromBody==expected && fromController==expected);
    // Observation must include the intro: combat permission is not a trace gate.
    {auto intro=good;intro.request.frame.bossFighting=false;GARDEN_FIRE_CHECK(intro.accepted());}
    {auto bad=good;bad.request.frame.enabled=false;GARDEN_FIRE_CHECK(!bad.accepted());}
    {auto bad=good;bad.request.frame.finished=true;GARDEN_FIRE_CHECK(!bad.accepted());}
    {auto bad=good;bad.request.frame.bossDead=true;GARDEN_FIRE_CHECK(!bad.accepted());}
    {auto bad=good;++bad.request.owner.value;GARDEN_FIRE_CHECK(!bad.accepted());}
    {auto bad=good;++bad.request.enemy.run;GARDEN_FIRE_CHECK(!bad.accepted());}
    {auto bad=good;++bad.request.enemy.source;GARDEN_FIRE_CHECK(!bad.accepted());}
    {auto bad=good;++bad.request.enemy.registry;GARDEN_FIRE_CHECK(!bad.accepted());}
    for(const auto offset:{0x1FCU,0x244U}) {
        auto bad=good;bad.put<std::uint32_t>(Fixture::source+offset,8);GARDEN_FIRE_CHECK(!bad.accepted());
    }
    // Salt changes preserve the low slot index and must still be rejected.
    {auto bad=good;bad.put<std::uint32_t>(Fixture::actor+0x48,3U^0x2000U);GARDEN_FIRE_CHECK(!bad.accepted());}
    {auto bad=good;bad.put<std::uint32_t>(Fixture::body+0x24,Fixture::bodySelf^0x2000U);GARDEN_FIRE_CHECK(!bad.accepted());}
    {auto bad=good;bad.put<std::uint32_t>(Fixture::reference+0x24,Fixture::referenceSelf^0x2000U);GARDEN_FIRE_CHECK(!bad.accepted());}
    {auto bad=good;bad.put<std::uint32_t>(Fixture::controller+0xC0,4);GARDEN_FIRE_CHECK(!bad.accepted(true));}
    {auto bad=good;bad.put<std::uint32_t>(Fixture::actor+0x4C,6);GARDEN_FIRE_CHECK(!bad.accepted());}
    {auto bad=good;bad.put<std::uint32_t>(Fixture::actor+0x38,11);GARDEN_FIRE_CHECK(!bad.accepted());}
    {auto bad=good;bad.handles[Fixture::bodySelf]=Fixture::body+0x100;GARDEN_FIRE_CHECK(!bad.accepted());}
    {auto bad=good;bad.handles.erase(Fixture::referenceSelf);GARDEN_FIRE_CHECK(!bad.accepted());}
    for(const auto location:{Fixture::body,Fixture::body+4,Fixture::controller,Fixture::controller+4,
                             Fixture::source,Fixture::source+4,Fixture::reference+4}) {
        auto bad=good;bad.put<std::uint32_t>(location,0);
        GARDEN_FIRE_CHECK(!bad.accepted(location==Fixture::controller || location==Fixture::controller+4));
    }
    {auto bad=good;bad.put<std::uint32_t>(Fixture::reference+0x2C,6);GARDEN_FIRE_CHECK(!bad.accepted());}
    {auto bad=good;bad.put<std::uint32_t>(Fixture::body+0x2C,6);GARDEN_FIRE_CHECK(!bad.accepted(true));}
    {auto bad=good;bad.put<std::uint32_t>(Fixture::reference+0x60,14);GARDEN_FIRE_CHECK(!bad.accepted());}
    {auto bad=good;bad.put<std::uint32_t>(Fixture::entity+4,4);GARDEN_FIRE_CHECK(!bad.accepted());}
    {auto bad=good;bad.put<std::uint32_t>(Fixture::sourceBase+0x758,0);GARDEN_FIRE_CHECK(!bad.accepted());}
    {auto bad=good;bad.put<std::uint8_t>(Fixture::sourceBase+0x75C,2);GARDEN_FIRE_CHECK(!bad.accepted());}
    {auto bad=good;bad.put<std::uint16_t>(Fixture::sourceBase+0x75E,4);GARDEN_FIRE_CHECK(!bad.accepted());}
    for(const auto offset:{std::int64_t{-1},std::int64_t{0x1000001}}) {
        auto bad=good;bad.put<std::int64_t>(Fixture::actor+0x40,offset);GARDEN_FIRE_CHECK(!bad.accepted());
        bad=good;bad.put<std::int64_t>(Fixture::source+8,offset);GARDEN_FIRE_CHECK(!bad.accepted());
    }
    for(const auto location:{Fixture::body+0x2C,Fixture::image+0x1F9D7F8,Fixture::source+0x244,
                             Fixture::sourceBase+0x758,Fixture::entity+0x98,Fixture::reference+0x60}) {
        auto bad=good;bad.bytes.erase(location);GARDEN_FIRE_CHECK(!bad.accepted());
    }
    // Mutation during traversal exercises the second ownership check, not just
    // malformed initial fixtures. Each mutation starts from a valid owner.
    const auto crossesChange=[&](std::uintptr_t after,std::function<void(Fixture&)> change,bool controller=false) {
        ChangingRead read;read.changeAfter=after;read.change=std::move(change);
        return !read.accepted(controller) && read.changed;
    };
    GARDEN_FIRE_CHECK(crossesChange(Fixture::body,[](Fixture& f){f.put<std::uint32_t>(Fixture::body+0x24,UINT32_MAX);}));
    GARDEN_FIRE_CHECK(crossesChange(Fixture::controller,[](Fixture& f){f.handles[14]=Fixture::controller+0x100;},true));
    GARDEN_FIRE_CHECK(crossesChange(Fixture::reference+0x60,[](Fixture& f){f.put<std::uint32_t>(Fixture::reference+0x60,UINT32_MAX);}));
    GARDEN_FIRE_CHECK(crossesChange(Fixture::entity+0x98,[](Fixture& f){f.put<std::uint32_t>(Fixture::entity+0x98,UINT32_MAX);}));
    GARDEN_FIRE_CHECK(crossesChange(Fixture::actor+0x4C,[](Fixture& f){f.put<std::uint32_t>(Fixture::actor+0x4C,6);}));
    GARDEN_FIRE_CHECK(crossesChange(Fixture::source+0x1FC,[](Fixture& f){f.put<std::uint32_t>(Fixture::source+0x1FC,8);}));
    GARDEN_FIRE_CHECK(crossesChange(Fixture::source+0x244,[](Fixture& f){f.handles[10]=Fixture::source+0x100;}));
    GARDEN_FIRE_CHECK(crossesChange(Fixture::source+0x244,[](Fixture& f){f.put<std::int64_t>(Fixture::actor+0x40,8);}));
    GARDEN_FIRE_CHECK(crossesChange(Fixture::source+0x244,[](Fixture& f){f.put<std::uint32_t>(Fixture::source,17);}));
    GARDEN_FIRE_CHECK(crossesChange(Fixture::source+8,[](Fixture& f){f.put<std::int64_t>(Fixture::source+8,0x738);}));
    GARDEN_FIRE_CHECK(crossesChange(Fixture::body,[](Fixture& f){f.put<std::uint32_t>(Fixture::body,0);},true));
    // A new salted character at the same address can qualify on a later call,
    // but must not compare equal to the identity captured before the callback.
    {auto next=good;constexpr auto nextSelf=Fixture::bodySelf+0x2000U;
        next.handles.erase(Fixture::bodySelf);next.handles[nextSelf]=Fixture::body;
        next.put<std::uint32_t>(Fixture::body+0x24,nextSelf);next.put<std::uint32_t>(Fixture::reference+0x60,nextSelf);
        native::Identity after{};GARDEN_FIRE_CHECK(native::sample(next,Fixture::image,Fixture::body,false,next.request,after));
        GARDEN_FIRE_CHECK(after.character==fromBody.character && after!=fromBody);
    }
    native::Suppression state{};
    GARDEN_FIRE_CHECK(native::suppression(good,fromBody,state) && state.ticks==0x35C75F6CU && state.active==1);
    {auto bad=good;bad.bytes.erase(Fixture::body+0xAC4);GARDEN_FIRE_CHECK(!native::suppression(bad,fromBody,state));}
    {auto bad=good;bad.bytes.erase(Fixture::body+0x24);native::Identity untouched=expected;
        GARDEN_FIRE_CHECK(!native::sample(bad,Fixture::image,Fixture::body,false,bad.request,untouched) && untouched==expected);
    }
#undef GARDEN_FIRE_CHECK
    return true;
}
