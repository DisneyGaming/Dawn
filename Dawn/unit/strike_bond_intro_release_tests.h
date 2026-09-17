#pragma once
#include "strike_bond_fire_trace_tests.h"
#include "../src/client/hooks/bootflow/strike_bond_intro_release.h"

namespace strike_bond_intro_fixture {
namespace release=dawn::client::hooks::bootflow::strike_bond_intro_release;
using Base=strike_bond_fire_fixture::Fixture;
using Motion=std::array<std::byte,release::kMotionBytes>;
template<class T> void put(Motion& b,std::size_t offset,T value) {std::memcpy(b.data()+offset,&value,sizeof value);}
inline Motion owned(const release::Binding& owner,std::uint32_t animation) {
    Motion b{};
    const auto u32=[&](std::size_t o,std::uint32_t v){put(b,o,v);};
    const auto u64=[&](std::size_t o,std::uint64_t v){put(b,o,v);};
    u32(0,0x815B5A49);u32(4,0x808069EE);u64(8,0xC10);u32(0x24,owner.motionSelf);u32(0x2C,owner.character.entity);
    u32(0x30,0x815B5A49);u32(0x34,0x808069F6);u64(0x38,0xD18);
    u64(0x50,3);u64(0x60,3);u64(0x70,0x230);u32(0x80,1);put<std::uint16_t>(b,0x88,1);put<std::uint16_t>(b,0x8A,1);
    u64(0x58,0x268);u64(0x68,0x278);u64(0x78,0x298);u32(0x2D0,0x00390000);put<std::uint16_t>(b,0x2F2,0x150);
    constexpr std::size_t raw=0x320,s=raw+0x28;
    u32(raw,0x80F459AA);u32(raw+4,0x80806872);u64(raw+8,0x210);u32(raw+0x10,0x80BFDE65);
    u32(raw+0x18,owner.selectorSelf);u32(raw+0x1C,0x8080686B);u32(s,owner.character.entity);u32(s+4,owner.controllerSelf);
    u32(s+0xC,1);u32(s+0x10,0x80F459AD);u32(s+0x14,animation);u32(s+0xA8,1);u32(s+0xB4,1);
    return b;
}
struct Fixture : Base {
    static constexpr std::uintptr_t selector=bundle+0x1000,motion=bundle+0x2000,animation=bundle+0x3000;
    static constexpr std::uintptr_t main=0xE00000,names=0xF00000,interfaceBase=0x1100000;
    static constexpr std::uint32_t selectorSelf=0x6020,motionSelf=0x8021,animationSelf=0xA022,mainSelf=0xC023;
    release::Binding binding{{body,bodySelf,5},controller,selector,motion,14,selectorSelf,motionSelf};
    Fixture() {
        request.frame.lensDestroyed[7]=true;
        put<std::uint64_t>(controller+8,0x6E8);
        const auto header=[&](std::uintptr_t a,std::uint32_t tag,std::uint32_t kind,std::uint64_t offset,std::uint32_t self){
            zero(a,0x30);put(a,tag);put(a+4,kind);put(a+8,offset);put(a+0x24,self);put<std::uint32_t>(a+0x2C,5);handles[self]=a;
        };
        header(selector,0x80F459AA,0x8080686C,0x1C8,selectorSelf);
        header(animation,0x80F66F51,0x808036CF,0x1FF0,animationSelf);
        handles[motionSelf]=motion;auto data=owned(binding,animationSelf);
        for(std::size_t i=0;i<data.size();++i) bytes[motion+i]=data[i];
        put<std::uint64_t>(metadata+0x68,3);
        put<std::int32_t>(metadata+0x114,0x100);put<std::int32_t>(metadata+0x12C,0x1000);put<std::int32_t>(metadata+0x144,0x2000);
        put<std::uint32_t>(selector+0x30,14);put<std::uint64_t>(selector+0x40,3);put<std::uint64_t>(selector+0x48,0x28);
        put<std::uint32_t>(selector+0xC0,0x80F459AA);put<std::uint32_t>(selector+0xC4,0x8080300D);put<std::uint32_t>(selector+0xF4,1);
        put<std::uint64_t>(controller+0x580,1);put<std::uint64_t>(controller+0x588,0x78);
        put<std::uint32_t>(controller+0x640,0x80BFDE65);put(controller+0x648,selectorSelf);
        put<std::uint32_t>(controller+0x64C,0x8080686B);put<std::uint64_t>(controller+0x650,0);
        handles[mainSelf]=main;put(controller+0x5C0,mainSelf);put<std::uint32_t>(main+0x30,14);put<std::uint32_t>(main+4,0x80F459CD);
        handles[0x80F459CD]=names;put<std::uint64_t>(names+0x20,1);put<std::uint64_t>(names+0x28,0x18);
        constexpr auto group=names+0x50;put(group+0xC,release::kGroup);put<std::uint64_t>(group+0x10,3);put<std::uint64_t>(group+0x18,0x20);
        put(group+0x4C,release::kSequence);
        handles[0x80BFDE65]=interfaceBase;put<std::uint64_t>(interfaceBase+0x18,0x18);put<std::uintptr_t>(interfaceBase+0x60,image+0x10D35D0);
        constexpr std::array<std::uint8_t,16> prefix{0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x7C,0x24,0x10,0x55,0x48,0x8B,0xEC,0x48,0x83};
        for(std::size_t i=0;i<prefix.size();++i) put(image+0xC693F0+i,prefix[i]);
    }
    bool accepted(unsigned* failure=nullptr) {release::Binding out{};unsigned reason{};const bool ok=release::sample(*this,image,body,request,out,reason);if(failure)*failure=reason;return ok;}
};
}

inline bool strike_bond_intro_release_contracts() {
    using namespace strike_bond_intro_fixture;
#define GARDEN_INTRO_CHECK(expression) do {if(!(expression)){std::fprintf(stderr,"FAIL Garden intro release line %d: %s\n",__LINE__,#expression);return false;}} while(false)
    Fixture good;unsigned reason=99;release::Binding bound{};
    GARDEN_INTRO_CHECK(release::sample(good,Fixture::image,Fixture::body,good.request,bound,reason) && reason==0 && bound==good.binding);
    GARDEN_INTRO_CHECK(release::wanted(good.request));
    {auto bad=good.request;bad.frame.bossFighting=false;GARDEN_INTRO_CHECK(!release::wanted(bad));}
    {auto bad=good.request;bad.frame.lensDestroyed[7]=false;GARDEN_INTRO_CHECK(!release::wanted(bad));}
    {auto bad=good.request;bad.frame.bossStage=1;GARDEN_INTRO_CHECK(!release::wanted(bad));}
    {auto bad=good.request;bad.frame.finished=true;GARDEN_INTRO_CHECK(!release::wanted(bad));}
    {auto bad=good.request;bad.frame.bossDead=true;GARDEN_INTRO_CHECK(!release::wanted(bad));}
    {auto bad=good.request;bad.frame.enabled=false;GARDEN_INTRO_CHECK(!release::wanted(bad));}
    {auto bad=good.request;++bad.owner.value;GARDEN_INTRO_CHECK(!release::wanted(bad));}
    // Independent byte oracle verifies endian order, opcode placement, and zero fill.
    std::array<std::byte,128> expected{};
    constexpr std::array<std::uint8_t,8> hashes{0x12,0x1A,0xB1,0xAF,0x93,0x3F,0xA0,0x31};
    for(std::size_t i=0;i<hashes.size();++i) expected[i]=std::byte(hashes[i]);expected[0x60]=std::byte{0x5D};
    GARDEN_INTRO_CHECK(release::stop_request()==expected);
    auto loop=owned(good.binding,Fixture::animationSelf);GARDEN_INTRO_CHECK(release::owned_loop(loop,good.binding,Fixture::animationSelf));
    // Reject changed resources, layout, allocation ownership, opcode, and the
    // selected/active machine; no offset is inferred from another allocation.
    for(const auto offset:{0U,4U,8U,0x24U,0x2CU,0x30U,0x34U,0x38U,0x50U,0x58U,0x60U,0x68U,0x70U,0x78U,
                           0x80U,0x84U,0x88U,0x8AU,0x8CU,0x2D0U,0x2D2U,0x2F0U,0x2F2U,0x2F4U,
                           0x320U,0x324U,0x328U,0x330U,0x338U,0x33CU,0x340U,0x348U,0x34CU,0x350U,0x354U,
                           0x358U,0x35CU,0x3ECU,0x3F0U,0x3F8U,0x3FCU}) {
        auto bad=loop;bad[offset]^=std::byte{1};GARDEN_INTRO_CHECK(!release::owned_loop(bad,good.binding,Fixture::animationSelf));
    }
    {auto other=good.binding;other.motionSelf^=0x2000;GARDEN_INTRO_CHECK(!release::owned_loop(loop,other,Fixture::animationSelf));}
    {auto other=good.binding;++other.character.entity;GARDEN_INTRO_CHECK(!release::owned_loop(loop,other,Fixture::animationSelf));}
    {auto other=good.binding;other.selectorSelf^=0x2000;GARDEN_INTRO_CHECK(!release::owned_loop(loop,other,Fixture::animationSelf));}
    GARDEN_INTRO_CHECK(!release::owned_loop(loop,good.binding,Fixture::animationSelf^0x2000));
    // Playback advances legitimately without changing the ownership proof.
    {auto next=loop;put<float>(next,0x384,9.F);GARDEN_INTRO_CHECK(release::owned_loop(next,good.binding,Fixture::animationSelf));}
    for(const auto address:{Fixture::controller,Fixture::selector,Fixture::motion,Fixture::animation}) {
        for(const auto offset:{0U,4U,8U,0x24U,0x2CU}) {auto bad=good;bad.bytes[address+offset]^=std::byte{1};GARDEN_INTRO_CHECK(!bad.accepted());}
    }
    for(const auto address:{Fixture::controller+0xC0,Fixture::controller+0x580,Fixture::controller+0x588,Fixture::controller+0x640,Fixture::controller+0x648,Fixture::controller+0x64C,Fixture::controller+0x650,Fixture::selector+0x30,Fixture::selector+0x40,Fixture::selector+0x48,
                           Fixture::selector+0xC0,Fixture::selector+0xC4,Fixture::selector+0xF4,Fixture::main+4,Fixture::main+0x30,
                           Fixture::names+0x20,Fixture::names+0x5C,Fixture::names+0x60,Fixture::names+0x9C,
                           Fixture::interfaceBase+0x60,Fixture::image+0xC693F0}) {
        auto bad=good;bad.bytes[address]^=std::byte{1};GARDEN_INTRO_CHECK(!bad.accepted());
    }
    for(const auto handle:{Fixture::selectorSelf,Fixture::motionSelf,Fixture::animationSelf,Fixture::mainSelf,0x80F459CDU,0x80BFDE65U}) {
        auto bad=good;bad.handles[handle]+=0x10;GARDEN_INTRO_CHECK(!bad.accepted());
    }
    for(const auto address:{Fixture::selector+0xF4,Fixture::motion+0x400,Fixture::animation+0x24,Fixture::names+0x9C,
                           Fixture::interfaceBase+0x60,Fixture::image+0xC693F0+15}) {
        auto bad=good;bad.bytes.erase(address);GARDEN_INTRO_CHECK(!bad.accepted());
    }
    {auto bad=good;bad.request.frame.bossStage=1;GARDEN_INTRO_CHECK(!bad.accepted(&reason) && reason==1);}
    {auto bad=good;bad.bytes[Fixture::body]^=std::byte{1};GARDEN_INTRO_CHECK(!bad.accepted(&reason) && reason==2);}
    {auto bad=good;bad.bytes.erase(Fixture::selector+0x24);release::Binding untouched=good.binding;reason=0;
        GARDEN_INTRO_CHECK(!release::sample(bad,Fixture::image,Fixture::body,bad.request,untouched,reason) && reason!=0 && untouched==good.binding);}
#undef GARDEN_INTRO_CHECK
    return true;
}