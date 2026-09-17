#pragma once
#include "strike_bond_fire_trace_tests.h"
#include "../src/client/hooks/bootflow/strike_bond_target_binding.h"

namespace strike_bond_target_fixture {
namespace binding=dawn::client::hooks::bootflow::strike_bond_target_binding;
using Base=strike_bond_fire_fixture::Fixture;
using Packet=std::array<std::byte,128>;
inline Packet exact() {
    Packet p{};
    p[0x60]=std::byte{0x24};
    for(std::size_t i=0x70;i<0x78;++i) p[i]=std::byte{0xFF};
    p[0x78]=std::byte{0x80};p[0x79]=std::byte{1};p[0x7A]=std::byte{1};
    return p;
}
struct Fixture : Base {
    static constexpr std::uintptr_t main=0xE00000,weapon=main+0x8F0,action=0xF00000;
    static constexpr std::uintptr_t config=0x1100000,program=0x1200000,decoder=0x1300000;
    static constexpr std::uint32_t mainSelf=0x6020,targetHandle=0x2081,targetSerial=0x05059C9B;
    static constexpr std::uintptr_t targetRow=entities+(targetHandle&0x1FFFU)*0x100;
    using Weak=dawn::client::hooks::bootflow::gateway_native::Weak;
    std::map<std::uint32_t,std::uint32_t> serials{{targetHandle,targetSerial}};
    Fixture() {
        request.frame.lensDestroyed[7]=true;
        handles[mainSelf]=main;handles[0x80F66F52U]=config;handles[0x80F56183U]=program;
        put<std::uint32_t>(weapon+0x1D8,14);put(controller+0x5C0,mainSelf);
        put<std::uint32_t>(main+0x30,14);put<std::uint32_t>(main,0x80F56184U);
        put<std::uint32_t>(actor+0x30,0x80F66F52U);
        put(main+0x1680,Weak{targetSerial,targetHandle});put<std::uint32_t>(targetRow+4,0);put(targetRow+0xC,targetHandle);
        const auto packet=exact();for(std::size_t i=0;i<packet.size();++i) bytes[action+i]=packet[i];
        put(decoder+8,config);put(decoder+0x10,program);
        put<std::uint32_t>(config+0x6C8,0x64F350F0U);put<std::uint32_t>(config+0x6CC,5);
        put<std::uint32_t>(config+0x1204,UINT32_MAX);
        put<std::uint8_t>(program+0x3B6C0+384,16);put<std::uint32_t>(program+0x309F0+384*4,0x50);
        put<std::uint32_t>(program+0x2ED70,0x64F350F0U);
    }
    bool weak(Weak w) {const auto it=serials.find(w.handle);return it!=serials.end() && it->second==w.serial;}
    bool entity_row(Weak w,std::uintptr_t& row) {
        std::uintptr_t entityTable{};std::uint32_t stride{},flags{};
        if(!weak(w) || !value(image+0x1F93428,entityTable) || !value(image+0x1F93430,stride)) return false;
        row=entityTable+static_cast<std::uintptr_t>(w.handle&0x1FFFU)*stride;
        return value(row+4,flags) && (flags&4U)==0;
    }
    bool accepted() {binding::Binding out{};return binding::sample(*this,image,weapon,action,request,out);}
};
struct ChangingRead : Fixture {
    std::uintptr_t changeAfter{};std::function<void(Fixture&)> change;bool changed{};
    bool copy(std::uintptr_t address,std::span<std::byte> output) {
        const bool ok=Fixture::copy(address,output);
        if(ok && !changed && address==changeAfter) {changed=true;change(*this);}
        return ok;
    }
    template<class T> bool value(std::uintptr_t address,T& out) {
        return copy(address,std::as_writable_bytes(std::span{&out,1}));
    }
    bool accepted() {binding::Binding out{};return binding::sample(*this,image,weapon,action,request,out);}
};

inline std::array<std::byte,24> cached_oracle() {
    // Independent little-endian oracle for the captured compact command.
    std::array<std::byte,24> b{};
    for(std::size_t i=0;i<8;++i) b[i]=std::byte{0xFF};
    b[8]=std::byte{0x80};b[9]=std::byte{1};b[10]=std::byte{0x24};b[11]=std::byte{1};
    b[18]=std::byte{0xFF};b[19]=std::byte{0xFF};return b;
}
struct ReplayFixture : Fixture {
    static constexpr std::uint32_t actorStride=0xA200,aiSelf=0x8042;
    static constexpr std::uintptr_t replayActor=table+3*actorStride,ai=0x1400000;
    static constexpr std::uintptr_t pending=ai+0x1A0,cache=main+0x1E8,head=main+0xE0+29*2;
    ReplayFixture() {
        // Move the actor row into a realistic pool stride without copying the
        // unrelated rows or changing the owning source and character fixture.
        for(std::size_t i=0;i<0x100;++i) {
            const auto it=bytes.find(actor+i);if(it!=bytes.end()) bytes[replayActor+i]=it->second;
        }
        put<std::uint32_t>(image+0x1F9D800,actorStride);put(replayActor+0x50,aiSelf);
        handles[aiSelf]=ai;zero(ai,0x30);
        put<std::uint32_t>(ai,0x80F66F55U);put<std::uint32_t>(ai+4,0x808082ECU);
        put<std::uint64_t>(ai+8,0x2ED8);put(ai+0x24,aiSelf);put<std::uint32_t>(ai+0x2C,5);
        zero(main+0xC0,32);put(main+0xC0,aiSelf);put<std::uint32_t>(main+0xC4,3);
        put<std::uint64_t>(main+0xC8,UINT64_MAX);put<std::uint16_t>(weapon+0xB2,0xFF02);
        put<std::uint32_t>(main+0x770,16);put<std::uint32_t>(main+0x774,0x1A0);
        put<std::uint32_t>(main+0x778,4);put<std::uint32_t>(main+0x77C,1);put<std::uint16_t>(head,0x78);
        const auto compact=cached_oracle();for(std::size_t i=0;i<compact.size();++i) bytes[cache+i]=compact[i];
        put<std::uint32_t>(ai+0x110,3);zero(ai+0x120,3*128);
        put<std::uint8_t>(ai+0x180,0x54);put<std::uint8_t>(ai+0x280,0x42);
        const auto packet=exact();for(std::size_t i=0;i<packet.size();++i) bytes[pending+i]=packet[i];
    }
    bool replay(unsigned* failure=nullptr) {
        binding::Replay out{};unsigned why{};const bool ok=binding::replay_sample(*this,image,weapon,request,out,why);
        if(failure)*failure=why;return ok;
    }
};
struct ChangingReplay : ReplayFixture {
    std::uintptr_t changeAfter{};std::function<void(ReplayFixture&)> change;bool changed{};
    bool copy(std::uintptr_t address,std::span<std::byte> output) {
        const bool ok=ReplayFixture::copy(address,output);
        if(ok && !changed && address==changeAfter){changed=true;change(*this);}return ok;
    }
    template<class T> bool value(std::uintptr_t address,T& out) {return copy(address,std::as_writable_bytes(std::span{&out,1}));}
    bool replay() {binding::Replay out{};unsigned why{};return binding::replay_sample(*this,image,weapon,request,out,why);}
};

}

inline bool strike_bond_target_binding_contracts() {
    using namespace strike_bond_target_fixture;
#define GARDEN_TARGET_CHECK(expression) do {if(!(expression)){std::fprintf(stderr,"FAIL Garden target binding line %d: %s\n",__LINE__,#expression);return false;}} while(false)
    Base base;base.request.frame.lensDestroyed[7]=true;
    GARDEN_TARGET_CHECK(binding::wanted(base.request));
    for(std::uint8_t stage=0;stage<=2;++stage) {
        auto r=base.request;r.frame.bossStage=stage;GARDEN_TARGET_CHECK(binding::wanted(r));
    }
    {auto r=base.request;r.frame.bossStage=3;GARDEN_TARGET_CHECK(!binding::wanted(r));}
    {auto r=base.request;r.frame.bossFighting=false;GARDEN_TARGET_CHECK(!binding::wanted(r));}
    {auto r=base.request;r.frame.lensDestroyed[7]=false;GARDEN_TARGET_CHECK(!binding::wanted(r));}
    {auto r=base.request;r.frame.enabled=false;GARDEN_TARGET_CHECK(!binding::wanted(r));}
    {auto r=base.request;r.frame.finished=true;GARDEN_TARGET_CHECK(!binding::wanted(r));}
    {auto r=base.request;r.frame.bossDead=true;GARDEN_TARGET_CHECK(!binding::wanted(r));}
    {auto r=base.request;++r.owner.value;GARDEN_TARGET_CHECK(!binding::wanted(r));}
    {auto r=base.request;++r.enemy.run;GARDEN_TARGET_CHECK(!binding::wanted(r));}
    {auto r=base.request;++r.enemy.source;GARDEN_TARGET_CHECK(!binding::wanted(r));}
    {auto r=base.request;++r.enemy.registry;GARDEN_TARGET_CHECK(!binding::wanted(r));}
    // The oracle comes from the captured request. Every byte is significant:
    // a scoped expression or another request must keep native decoding intact.
    const auto packet=exact();GARDEN_TARGET_CHECK(binding::exact_request(packet));
    for(std::size_t i=0;i<packet.size();++i) {
        auto wrong=packet;wrong[i]^=std::byte{1};
        GARDEN_TARGET_CHECK(!binding::exact_request(wrong));
    }

    Fixture good;binding::Binding actual{};
    GARDEN_TARGET_CHECK(binding::sample(good,Fixture::image,Fixture::weapon,Fixture::action,good.request,actual));
    const binding::Binding expected{Fixture::weapon,Fixture::controller,Fixture::main,14,
        {Fixture::targetSerial,Fixture::targetHandle},{Fixture::body,Fixture::bodySelf,5}};
    GARDEN_TARGET_CHECK(actual==expected);
    for(const auto address:{Fixture::weapon+0x1D8,Fixture::controller+0x24,Fixture::controller+0xC0,
            Fixture::controller+0x5C0,Fixture::main,Fixture::main+0x30,Fixture::actor+0x30,
            Fixture::actor+0x48,Fixture::actor+0x4C,Fixture::actor+0x38,Fixture::source+0x244,
            Fixture::targetRow+0xC}) {
        auto bad=good;bad.bytes[address]^=std::byte{1};GARDEN_TARGET_CHECK(!bad.accepted());
    }
    {auto bad=good;bad.request.frame.bossFighting=false;GARDEN_TARGET_CHECK(!bad.accepted());}
    {auto bad=good;++bad.request.owner.value;GARDEN_TARGET_CHECK(!bad.accepted());}
    {auto bad=good;bad.put<std::uint16_t>(Fixture::action+0x74,0);GARDEN_TARGET_CHECK(!bad.accepted());}
    {auto bad=good;bad.put<std::uint32_t>(Fixture::targetRow+4,4);GARDEN_TARGET_CHECK(!bad.accepted());}
    {auto bad=good;++bad.serials[Fixture::targetHandle];GARDEN_TARGET_CHECK(!bad.accepted());}
    {auto bad=good;bad.serials.erase(Fixture::targetHandle);GARDEN_TARGET_CHECK(!bad.accepted());}
    {auto bad=good;bad.put(Fixture::main+0x1680,Fixture::Weak{Fixture::targetSerial,5});bad.serials[5]=Fixture::targetSerial;GARDEN_TARGET_CHECK(!bad.accepted());}
    {auto bad=good;bad.put(Fixture::main+0x1680,Fixture::Weak{Fixture::targetSerial,Fixture::targetHandle^0x2000U});GARDEN_TARGET_CHECK(!bad.accepted());}
    for(const auto handle:{14U,Fixture::mainSelf,Fixture::bodySelf,Fixture::referenceSelf}) {
        auto bad=good;bad.handles[handle]+=0x100;GARDEN_TARGET_CHECK(!bad.accepted());
    }
    for(const auto address:{Fixture::action+0x7F,Fixture::weapon+0x1D8,Fixture::controller+0x5C0,
            Fixture::main+0x1680,Fixture::targetRow+4,Fixture::targetRow+0xC,Fixture::source+0x244}) {
        auto bad=good;bad.bytes.erase(address);GARDEN_TARGET_CHECK(!bad.accepted());
    }
    const auto crossed=[&](std::uintptr_t address,std::function<void(Fixture&)> change) {
        ChangingRead f;f.changeAfter=address;f.change=std::move(change);return !f.accepted() && f.changed;
    };
    GARDEN_TARGET_CHECK(crossed(Fixture::action,[](Fixture& f){f.put<std::uint16_t>(Fixture::action+0x74,0);}));
    GARDEN_TARGET_CHECK(crossed(Fixture::targetRow+0xC,[](Fixture& f){++f.serials[Fixture::targetHandle];}));
    GARDEN_TARGET_CHECK(crossed(Fixture::targetRow+0xC,[](Fixture& f){f.handles[Fixture::mainSelf]+=0x100;}));
    GARDEN_TARGET_CHECK(crossed(Fixture::targetRow+0xC,[](Fixture& f){f.handles[14]+=0x100;}));
    GARDEN_TARGET_CHECK(crossed(Fixture::targetRow+0xC,[](Fixture& f){f.put<std::uint32_t>(Fixture::weapon+0x1D8,15);}));
    GARDEN_TARGET_CHECK(crossed(Fixture::targetRow+0xC,[](Fixture& f){f.put(Fixture::main+0x1680,Fixture::Weak{Fixture::targetSerial+1,Fixture::targetHandle});}));
    GARDEN_TARGET_CHECK(crossed(Fixture::targetRow+0xC,[](Fixture& f){f.put<std::uint32_t>(Fixture::targetRow+4,4);}));
    GARDEN_TARGET_CHECK(crossed(Fixture::targetRow+0xC,[](Fixture& f){f.put<std::uint32_t>(Fixture::targetRow+0xC,Fixture::targetHandle^0x2000U);}));
    for(const auto address:{Fixture::actor+0x30,Fixture::main+0x30,Fixture::main}) {
        GARDEN_TARGET_CHECK(crossed(Fixture::targetRow+0xC,[address](Fixture& f){f.bytes[address]^=std::byte{1};}));
    }
    // A valid replacement in the same target slot must change binding identity.
    {auto next=good;++next.serials[Fixture::targetHandle];
        next.put(Fixture::main+0x1680,Fixture::Weak{Fixture::targetSerial+1,Fixture::targetHandle});
        binding::Binding after{};GARDEN_TARGET_CHECK(binding::sample(next,Fixture::image,Fixture::weapon,Fixture::action,next.request,after));
        GARDEN_TARGET_CHECK(after!=actual);
    }
    {auto bad=good;bad.bytes.erase(Fixture::action);auto untouched=expected;
        GARDEN_TARGET_CHECK(!binding::sample(bad,Fixture::image,Fixture::weapon,Fixture::action,bad.request,untouched) && untouched==expected);
    }
    GARDEN_TARGET_CHECK(binding::decoder_context(good,Fixture::decoder));
    for(const auto address:{Fixture::decoder+8,Fixture::decoder+0x10,Fixture::config+0x6C8,
            Fixture::config+0x6CC,Fixture::config+0x1204,Fixture::program+0x3B6C0+384,
            Fixture::program+0x309F0+384*4,Fixture::program+0x2ED70}) {
        auto bad=good;bad.bytes[address]^=std::byte{1};GARDEN_TARGET_CHECK(!binding::decoder_context(bad,Fixture::decoder));
        bad=good;bad.bytes.erase(address);GARDEN_TARGET_CHECK(!binding::decoder_context(bad,Fixture::decoder));
    }
    for(const auto handle:{0x80F66F52U,0x80F56183U}) {
        auto bad=good;bad.handles[handle]+=0x100;GARDEN_TARGET_CHECK(!binding::decoder_context(bad,Fixture::decoder));
    }


    const auto compact=cached_oracle();GARDEN_TARGET_CHECK(binding::exact_cached_request(compact));
    for(std::size_t i=0;i<compact.size();++i) {
        auto wrong=compact;wrong[i]^=std::byte{1};GARDEN_TARGET_CHECK(!binding::exact_cached_request(wrong));
    }
    ReplayFixture replay;binding::Replay replayed{};unsigned replayReason=99;
    GARDEN_TARGET_CHECK(binding::replay_sample(replay,Fixture::image,Fixture::weapon,replay.request,replayed,replayReason));
    GARDEN_TARGET_CHECK(replayReason==0 && replayed.ai==ReplayFixture::ai && replayed.aiSelf==ReplayFixture::aiSelf
        && replayed.action==ReplayFixture::pending && replayed.cacheRow==ReplayFixture::cache
        && replayed.cached==compact && replayed.binding==expected);
    {auto bad=replay;bad.request.frame.bossFighting=false;GARDEN_TARGET_CHECK(!bad.replay());}
    {auto bad=replay;bad.request.frame.lensDestroyed[7]=false;GARDEN_TARGET_CHECK(!bad.replay());}
    for(const auto mode:{std::uint16_t{0x0001},std::uint16_t{0xFF00},std::uint16_t{0x0101}}) {
        auto bad=replay;bad.put(ReplayFixture::weapon+0xB2,mode);GARDEN_TARGET_CHECK(!bad.replay());
    }
    for(const auto offset:{std::uint16_t{0xFFFF},std::uint16_t{0x79},std::uint16_t{0x18A},std::uint16_t{0x600}}) {
        auto bad=replay;bad.put(ReplayFixture::head,offset);GARDEN_TARGET_CHECK(!bad.replay());
    }
    for(const auto used:{0U,23U,0x601U}) {auto bad=replay;bad.put(ReplayFixture::main+0x774,used);GARDEN_TARGET_CHECK(!bad.replay());}
    for(const auto count:{0U,72U}) {auto bad=replay;bad.put(ReplayFixture::main+0x770,count);GARDEN_TARGET_CHECK(!bad.replay());}
    for(const auto count:{0U,33U}) {auto bad=replay;bad.put(ReplayFixture::ai+0x110,count);GARDEN_TARGET_CHECK(!bad.replay());}
    for(const auto address:{ReplayFixture::main+0xC0,ReplayFixture::main+0xC4,ReplayFixture::main+0xC8,
            ReplayFixture::main+0xD0,ReplayFixture::main+0xD8,ReplayFixture::main+0x778,ReplayFixture::main+0x77C,
            ReplayFixture::ai,ReplayFixture::ai+4,ReplayFixture::ai+8,ReplayFixture::ai+0x24,ReplayFixture::ai+0x2C,
            ReplayFixture::replayActor+0x50,ReplayFixture::cache+8,ReplayFixture::cache+10,ReplayFixture::cache+20,
            ReplayFixture::pending+0x74}) {
        auto bad=replay;bad.bytes[address]^=std::byte{1};GARDEN_TARGET_CHECK(!bad.replay());
    }
    {auto bad=replay;bad.put<std::uint32_t>(Fixture::image+0x1F9D800,0xA153);GARDEN_TARGET_CHECK(!bad.replay());}
    {auto bad=replay;bad.handles[ReplayFixture::aiSelf]+=0x100;GARDEN_TARGET_CHECK(!bad.replay());}
    {auto bad=replay;bad.handles.erase(ReplayFixture::aiSelf);GARDEN_TARGET_CHECK(!bad.replay());}
    {auto bad=replay;bad.put<std::uint8_t>(ReplayFixture::pending+0x60,0x23);GARDEN_TARGET_CHECK(!bad.replay());}
    {auto bad=replay;const auto duplicate=exact();for(std::size_t i=0;i<duplicate.size();++i) bad.bytes[ReplayFixture::ai+0x120+i]=duplicate[i];GARDEN_TARGET_CHECK(!bad.replay());}
    for(const auto address:{ReplayFixture::main+0xC0,ReplayFixture::ai+0x24,ReplayFixture::head,
            ReplayFixture::cache+23,ReplayFixture::ai+0x110,ReplayFixture::pending+0x60,
            ReplayFixture::pending+0x7F,ReplayFixture::ai+0x280}) {
        auto bad=replay;bad.bytes.erase(address);GARDEN_TARGET_CHECK(!bad.replay());
    }
    const auto replayCrossed=[&](std::uintptr_t address,std::function<void(ReplayFixture&)> change) {
        ChangingReplay f;f.changeAfter=address;f.change=std::move(change);return !f.replay() && f.changed;
    };
    GARDEN_TARGET_CHECK(replayCrossed(ReplayFixture::cache,[](ReplayFixture& f){f.put<std::uint32_t>(ReplayFixture::main+0xC4,4);}));
    GARDEN_TARGET_CHECK(replayCrossed(ReplayFixture::cache,[](ReplayFixture& f){f.handles[ReplayFixture::aiSelf]+=0x100;}));
    GARDEN_TARGET_CHECK(replayCrossed(ReplayFixture::pending,[](ReplayFixture& f){f.bytes[ReplayFixture::cache+20]^=std::byte{1};}));
    GARDEN_TARGET_CHECK(replayCrossed(ReplayFixture::pending,[](ReplayFixture& f){f.put<std::uint16_t>(ReplayFixture::head,0x90);}));
    GARDEN_TARGET_CHECK(replayCrossed(ReplayFixture::pending,[](ReplayFixture& f){f.put<std::uint32_t>(ReplayFixture::main+0x774,0x1B0);}));
    GARDEN_TARGET_CHECK(replayCrossed(ReplayFixture::pending,[](ReplayFixture& f){f.put<std::uint32_t>(ReplayFixture::ai+0x110,2);}));
    GARDEN_TARGET_CHECK(replayCrossed(ReplayFixture::pending,[](ReplayFixture& f){f.put<std::uint16_t>(ReplayFixture::weapon+0xB2,1);}));
    {auto bad=replay;bad.bytes.erase(ReplayFixture::cache);auto untouched=replayed;unsigned why{};
        GARDEN_TARGET_CHECK(!binding::replay_sample(bad,Fixture::image,Fixture::weapon,bad.request,untouched,why) && untouched==replayed && why!=0);
    }

#undef GARDEN_TARGET_CHECK
    return true;
}
