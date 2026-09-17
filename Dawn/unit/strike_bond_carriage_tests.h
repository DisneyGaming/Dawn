#pragma once
#include <limits>
#include "../src/client/hooks/bootflow/strike_bond_carriage.h"
namespace strike_bond_carriage_fixture {
namespace p=dawn::client::hooks::bootflow::strike_bond_carriage;
namespace m=dawn::state::activity::strike_bond;
template<class T,std::size_t N> void put(std::array<std::byte,N>& bytes,std::size_t offset,T value) {std::memcpy(bytes.data()+offset,&value,sizeof value);}
}
inline bool strike_bond_carriage_contracts() {
    using namespace strike_bond_carriage_fixture;
#define CARRIAGE_CHECK(x) do {if(!(x)){std::fprintf(stderr,"FAIL carriage line %d: %s\n",__LINE__,#x);return false;}} while(false)
    m::BossRequest r{};r.owner={7,1};r.enemy={7,0x7BF42018,0x1234,1,3,0x2CB86C0F};
    r.frame.enabled=true;r.frame.region=136;r.platform={{7,2},m::kBossPlatform,0x2FFAA254,11};
    const auto i=m::asset_index(m::kBossPlatform);r.frame.native[i]={2,0.F,true,true,true,true,true};
    // Attach before the middle cube breaks: motion must inherit the authored
    // initial placement, not preserve an arbitrary point halfway through a lap.
    CARRIAGE_CHECK(p::wanted(r));
    {auto x=r;x.frame.bossFighting=true;x.frame.lensDestroyed[7]=true;CARRIAGE_CHECK(p::wanted(x));}
    {auto x=r;x.frame.bossStage=1;CARRIAGE_CHECK(p::wanted(x));}
    {auto x=r;x.platform.owner.value=1;CARRIAGE_CHECK(!p::wanted(x));}
    {auto x=r;++x.platform.owner.run;CARRIAGE_CHECK(!p::wanted(x));}
    {auto x=r;x.platform.source.slot=173;CARRIAGE_CHECK(!p::wanted(x));}
    {auto x=r;x.platform.entity=UINT32_MAX;CARRIAGE_CHECK(!p::wanted(x));}
    {auto x=r;x.frame.native[i].acknowledged=false;CARRIAGE_CHECK(!p::wanted(x));}
    {auto x=r;x.frame.native[i].active=false;CARRIAGE_CHECK(!p::wanted(x));}
    {auto x=r;x.frame.native[i].desired=false;CARRIAGE_CHECK(!p::wanted(x));}
    {auto x=r;x.frame.bossDead=true;CARRIAGE_CHECK(!p::wanted(x));}
    {auto x=r;x.frame.ending=true;CARRIAGE_CHECK(!p::wanted(x));}
    {auto x=r;x.frame.region=120;CARRIAGE_CHECK(!p::wanted(x));}
    {auto x=r;++x.enemy.generation;CARRIAGE_CHECK(!p::wanted(x));}
    // Byte oracles recovered from the lower and upper native socket rows.
    std::array<std::byte,64> lower{};put<std::uint32_t>(lower,4,1);put<std::uint32_t>(lower,0x30,0x9F6DB313);
    CARRIAGE_CHECK(p::socket(lower));
    {auto x=lower;put<std::uint32_t>(x,0x30,0xA56D6FC0);CARRIAGE_CHECK(!p::socket(x));}
    {auto x=lower;put<std::uint32_t>(x,4,0);CARRIAGE_CHECK(!p::socket(x));}
    std::array<std::byte,80> pose{};put<float>(pose,0x1C,1.F);put<float>(pose,0x2C,1.F);
    put<std::uint32_t>(pose,0x30,0x7DF9E1A3);put<std::uint32_t>(pose,0x34,0x80809663);put<std::int64_t>(pose,0x38,0x30);
    CARRIAGE_CHECK(p::pose(pose,0x7DF9E1A3));CARRIAGE_CHECK(!p::pose(pose,0x7DF9E1A4));
    {auto x=pose;put<std::uint64_t>(x,0x40,1);CARRIAGE_CHECK(!p::pose(x,0x7DF9E1A3));}
    {auto x=pose;put<float>(x,0x20,std::numeric_limits<float>::quiet_NaN());CARRIAGE_CHECK(!p::pose(x,0x7DF9E1A3));}
    constexpr std::uint8_t captured[]{0x6f,0x59,0xf4,0x80,0xfd,0x89,0x80,0x80,0xb0,0x02,0,0,0,0,0,0,
        0x35,0x0d,0xc7,0x80,0,0,0,0,0xcc,0xe1,0xf9,0x76,0x45,0x85,0x80,0x80,0,0,0,0,0,0,0,0};
    std::array<std::byte,40> parent{};std::memcpy(parent.data(),captured,sizeof captured);
    CARRIAGE_CHECK(p::parent_interface(parent,0x76F9E1CC));CARRIAGE_CHECK(!p::parent_interface(parent,0x76F9E1CD));
    {auto x=parent;put<std::int64_t>(x,0x20,0x30);CARRIAGE_CHECK(!p::parent_interface(x,0x76F9E1CC));}
#undef CARRIAGE_CHECK
    return true;
}
