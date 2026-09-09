#pragma once
#include "../../../src/client/hooks/bootflow/beyond_infinity_lens_damage.h"
#include <array>
#include <cstring>
namespace beyond_lens_damage_fixture {
namespace damage=sunrise::client::hooks::bootflow::beyond_infinity_lens_damage;
namespace identity=sunrise::client::hooks::bootflow::native_box_identity;
namespace bi=sunrise::state::activity::beyond_infinity;
struct Memory {
    static constexpr std::uintptr_t base=0x10000,health=0x11000,source=0x12000,context=0x13000;
    static constexpr std::uint32_t healthHandle=0x12340123,entityHandle=0x23450234;
    std::array<std::byte,0x3400> bytes{};
    std::uintptr_t resolvedHealth{health};bool weakLive{true};
    struct Weak { std::uint32_t serial{},entity{}; };
    template<class T> void put(std::uintptr_t address,T value) { std::memcpy(bytes.data()+address-base,&value,sizeof value); }
    bool copy(std::uintptr_t address,std::span<std::byte> out) {
        if(address<base || address-base>bytes.size() || out.size()>bytes.size()-(address-base)) { return false; }
        std::memcpy(out.data(),bytes.data()+address-base,out.size());return true;
    }
    template<class T> bool value(std::uintptr_t address,T& out) { return copy(address,std::as_writable_bytes(std::span{&out,std::size_t{1}})); }
    bool resolve(std::uint32_t handle,std::uintptr_t& out) { out=resolvedHealth;return handle==healthHandle; }
    bool weak(Weak ref) { return weakLive && ref.serial==9 && ref.entity==entityHandle; }
    Memory() {
        put(context+8,health);put(health,std::uint32_t{0x80F48026});put(health+4,std::uint32_t{0x80804B8A});put(health+8,std::uint64_t{0xB08});
        put(health+0x24,healthHandle);put(health+0x2C,entityHandle);
        put(source,std::uint32_t{0x80F462B3});put(source+4,std::uint32_t{0x80809928});put(source+8,std::uint64_t{0x4C8});
        put(source+0x180,std::uint32_t{2});put(source+0x2F0,std::uint32_t{2});put(source+0x188,std::uint8_t{1});
        put(source+0x440,std::uint32_t{9});put(source+0x444,entityHandle);
    }
};
inline void run(void (*check)(bool,const char*)) {
    Memory m;identity::Sample sample{};
    check(identity::sample(m,Memory::context,sample) && !sample.dead,"lens exact live health context");
    bi::LensRequest request{};request.owner={90,1};request.enabled=true;request.objectGeneration=2;
    request.lens={{90,2},Memory::source,Memory::entityHandle,9,Memory::healthHandle};
    const damage::Candidate candidate{request.owner,request.lens};
    check(damage::current(m,request,{},sample),"lens object generation may differ from mission lifecycle");
    for(bool native:{false,true}) {
        check(damage::blocked(request,true) && !damage::allowed(request,true,native),"protected lens blocks health and lethal branch");
        request.vulnerable=true;
        check(!damage::blocked(request,true) && damage::allowed(request,true,native),"exposed bound lens permits native death branch");
        check(damage::allowed(request,false,native)==native && !damage::blocked(request,false),"unrelated object preserves native damage result");
        auto unbound=request;unbound.lens={};
        const bool matched=damage::current(m,unbound,candidate,sample);
        check(matched && damage::blocked(unbound,matched) && !damage::allowed(unbound,matched,native),"unbound authenticated lens remains protected even if exposed");
        check(!damage::current(m,unbound,{},sample),"unbound request alone cannot identify arbitrary boxes");
        unbound.owner.run++;
        check(!damage::current(m,unbound,candidate,sample),"candidate cannot cross mission runs");
        unbound=request;unbound.lens={};unbound.owner.value++;
        check(!damage::current(m,unbound,candidate,sample),"candidate cannot cross lifecycle generations");
        unbound=request;unbound.lens={};unbound.objectGeneration++;
        check(!damage::current(m,unbound,candidate,sample),"candidate cannot cross object generations");
        auto inactive=request;inactive.enabled=false;
        check(!damage::current(m,inactive,candidate,sample) && damage::allowed(inactive,true,native)==native,"disabled mission leaves damage native");
        inactive=request;inactive.destroyed=true;
        check(!damage::current(m,inactive,candidate,sample) && damage::allowed(inactive,true,native)==native,"destroyed lens leaves damage native");
        request.vulnerable=false;
    }
    for(const auto offset:{0U,4U,8U,0x180U,0x2F0U,0x188U,0x440U,0x444U}) {
        const auto address=Memory::source+offset;std::uint8_t saved{};m.value(address,saved);m.put(address,static_cast<std::uint8_t>(saved^1U));
        check(!damage::current(m,request,candidate,sample),"changed source identity generation or weak owner rejected");m.put(address,saved);
    }
    m.put(Memory::source,std::uint32_t{0x80F46F23});
    check(!damage::current(m,request,candidate,sample),"Gateway source cannot become a Beyond Infinity lens");m.put(Memory::source,std::uint32_t{0x80F462B3});
    auto other=sample;other.entity^=0x2000U;check(!damage::current(m,request,candidate,other),"entity salt substitution rejected");
    other=sample;other.health^=0x2000U;check(!damage::current(m,request,candidate,other),"health salt substitution rejected");
    m.weakLive=false;check(!damage::current(m,request,candidate,sample),"expired entity serial rejected");m.weakLive=true;
    auto stale=request;stale.lens.owner.run++;check(!damage::current(m,stale,candidate,sample),"bound owner from another run rejected");
    stale=request;stale.lens.owner.value++;check(!damage::current(m,stale,candidate,sample),"bound owner from another object generation rejected");
    m.resolvedHealth++;check(!identity::sample(m,Memory::context,sample),"health component must resolve to exact address");m.resolvedHealth=Memory::health;
    m.put(Memory::health+0x338,std::uint8_t{1});check(identity::sample(m,Memory::context,sample) && sample.dead,"native destruction bit recognized");
    m.put(Memory::health+0x338,std::uint8_t{2});check(identity::sample(m,Memory::context,sample) && !sample.dead,"other health bits do not invent destruction");
    m.put(Memory::health,std::uint32_t{0x815B5A40});check(!identity::sample(m,Memory::context,sample),"enemy health definition excluded from box damage adapter");
}
}
