#pragma once
#include "strike_bond_fire_trace_tests.h"
#include "../src/client/hooks/bootflow/strike_bond_boss_shield.h"
namespace strike_bond_shield_fixture {
namespace shield=sunrise::client::hooks::bootflow::strike_bond_boss_shield;
struct Fixture : strike_bond_fire_fixture::Fixture {
    static constexpr std::uintptr_t model=bundle+0x1000;
    static constexpr std::uint32_t modelSelf=0xE025;
    Fixture() {
        request.frame.region=136;put<std::uint32_t>(entity+12,5);
        zero(model,0x1C8);handles[modelSelf]=model;
        put<std::uint32_t>(model,0x80F4599F);put<std::uint32_t>(model+4,0x808072BD);put<std::uint64_t>(model+8,0x790);
        put(model+0x24,modelSelf);put<std::uint32_t>(model+0x2C,5);
        put<std::uint32_t>(model+0x1A0,0x80F4599F);put<std::uint32_t>(model+0x1A4,0x80807315);
        put<std::uint64_t>(model+0x1A8,0x978);put<std::int64_t>(model+0x1B0,-0x1B0);
        put<std::uint32_t>(model+0xC0,0x7DFD604B);put<std::uint32_t>(model+0x1C0,0x7FFD42F4);
        for(const auto pass:{0,1,3,7,9,12}) put<std::uint16_t>(model+0xC4+2*pass,1);
        put<std::uint64_t>(metadata+0x68,3);put<std::int32_t>(metadata+0x12C,0x1000);
        put<std::int32_t>(metadata+0x144,static_cast<std::int32_t>(health-bundle));
        put(image+shield::kDrawRva,shield::kDrawPrefix);
    }
    bool accepted() {shield::Binding out{};return shield::sample(*this,image,body,request,out);}
};
}
inline bool strike_bond_shield_contracts() {
    using namespace strike_bond_shield_fixture;
#define GARDEN_SHIELD_CHECK(expression) do {if(!(expression)){std::fprintf(stderr,"FAIL Garden shield line %d: %s\n",__LINE__,#expression);return false;}} while(false)
    Fixture good;shield::Binding bound{};
    GARDEN_SHIELD_CHECK(shield::sample(good,Fixture::image,Fixture::body,good.request,bound));
    GARDEN_SHIELD_CHECK(shield::boundaries(good,Fixture::image));
    GARDEN_SHIELD_CHECK(bound.model==Fixture::model && bound.health==Fixture::health && bound.modelSelf==Fixture::modelSelf);
    const auto before=good.bytes;
    auto hide=shield::command(bound,false),show=shield::command(bound,true);
    GARDEN_SHIELD_CHECK(hide.object==Fixture::model+0x1A0 && hide.renderer==0x7DFD604B && hide.pass==7 && hide.count==0);
    GARDEN_SHIELD_CHECK(show.object==hide.object && show.renderer==hide.renderer && show.pass==7 && show.count==1);
    GARDEN_SHIELD_CHECK(good.bytes==before);
    {auto nativeHidden=bound;nativeHidden.passFlags=0;GARDEN_SHIELD_CHECK(shield::command(nativeHidden,true).count==0);}
    GARDEN_SHIELD_CHECK(shield::enabled_count(0x1E1)==5 && shield::enabled_count(0x201)==0 && shield::enabled_count(0x3E1)==4);
    for(const auto address:{Fixture::model,Fixture::health}) for(const auto offset:{0U,4U,8U,0x24U,0x2CU}) {
        auto bad=good;bad.bytes[address+offset]^=std::byte{1};GARDEN_SHIELD_CHECK(!bad.accepted());
    }
    for(const auto offset:{0x1A0U,0x1A4U,0x1A8U,0x1B0U}) {
        auto bad=good;bad.bytes[Fixture::model+offset]^=std::byte{1};GARDEN_SHIELD_CHECK(!bad.accepted());
    }
    for(const auto offset:{0xC0U,0x1C0U}) {
        auto bad=good;bad.put<std::uint32_t>(Fixture::model+offset,UINT32_MAX);GARDEN_SHIELD_CHECK(!bad.accepted());
    }
    for(const auto handle:{Fixture::modelSelf,15U}) {auto bad=good;bad.handles[handle]+=0x10;GARDEN_SHIELD_CHECK(!bad.accepted());}
    for(const auto offset:{0xC0U,0xD2U,0x1B7U,0x1C0U}) {auto bad=good;bad.bytes.erase(Fixture::model+offset);GARDEN_SHIELD_CHECK(!bad.accepted());}
    {auto bad=good;bad.put<std::uint32_t>(Fixture::entity+12,5^0x2000U);GARDEN_SHIELD_CHECK(!bad.accepted());}
    {auto bad=good;bad.put<std::uint32_t>(Fixture::entity+4,1);GARDEN_SHIELD_CHECK(!bad.accepted());}
    {auto bad=good;++bad.request.owner.value;GARDEN_SHIELD_CHECK(!bad.accepted());}
    {auto bad=good;bad.request.frame.region=8;GARDEN_SHIELD_CHECK(!bad.accepted());}
    {auto bad=good;bad.request.frame.enabled=false;GARDEN_SHIELD_CHECK(!bad.accepted());}
    {auto bad=good;bad.request.frame.bossStage=3;GARDEN_SHIELD_CHECK(!bad.accepted());}
    {auto dead=good;dead.request.frame.bossDead=true;GARDEN_SHIELD_CHECK(!dead.accepted());}
    for(const auto rva:{shield::kDrawRva,std::uintptr_t{0xCD6C20}}) {
        auto bad=good;bad.bytes[Fixture::image+rva]^=std::byte{1};GARDEN_SHIELD_CHECK(!shield::boundaries(bad,Fixture::image));
    }
    // A rebuilt renderer remains eligible but must invalidate the cached key.
    {auto rebuilt=good;rebuilt.put<std::uint32_t>(Fixture::model+0xC0,0x7DFD804B);shield::Binding next{};
        GARDEN_SHIELD_CHECK(shield::sample(rebuilt,Fixture::image,Fixture::body,rebuilt.request,next) && next!=bound);}
    {auto bad=good;bad.bytes.erase(Fixture::health+0x24);auto untouched=bound;
        GARDEN_SHIELD_CHECK(!shield::sample(bad,Fixture::image,Fixture::body,bad.request,untouched) && untouched==bound);}
#undef GARDEN_SHIELD_CHECK
    return true;
}
