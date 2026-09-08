#pragma once
#include "client/hooks/bootflow/deadly_trial_lifetime.h"
#include <cstring>
// Compact reconstruction of the live missing-owner/missing-sync case. The
// runtime, packet record and activity owner remain distinct native objects.
struct LifetimeFixture {
    using Ref=sunrise::client::hooks::bootflow::deadly_trial_lifetime::Ref;
    using Identity=sunrise::client::hooks::bootflow::deadly_trial_lifetime::Identity;
    static constexpr std::uintptr_t activity=0x2000,roster=activity+0x28,life=0x4000,root=0x5000,pool=0x6000,record=0x7000;
    static constexpr std::uint32_t owner=0x00FBE000,component=0x21F90013,sync=0x60F26000;
    std::array<std::byte,0x9000> bytes{};
    unsigned writes{};bool active{true},found{true},race{},ownerChanged{};
    template<class T> void set(std::uintptr_t a,const T& v) { std::memcpy(bytes.data()+a,&v,sizeof v); }
    template<class T> T get(std::uintptr_t a) { T v{};std::memcpy(&v,bytes.data()+a,sizeof v);return v; }
    template<class T> bool value(std::uintptr_t a,T& v) {
        if(a>bytes.size() || sizeof v>bytes.size()-a) { return false; }v=get<T>(a);return true;
    }
    bool resolve(std::uint32_t h,std::uintptr_t& a) {
        a=h==owner?activity:h==component?life:h==sync?record:0;return a!=0;
    }
    std::uintptr_t context() { return root; }
    bool lookup(const Identity& id,Ref& ref) {
        if(!found || id.key!=0x4786C0E0U || id.type!=17 || id.slot!=3) { return false; }
        if(ownerChanged) { set(root+0x1C,7U); }
        ref={component,0x80809915U,0};return true;
    }
    bool allocated(std::uintptr_t p,std::uint16_t i) { return active && p==pool && i<2; }
    bool assign(std::uintptr_t a,std::uint32_t before,std::uint32_t after) {
        if(race || get<std::uint32_t>(a)!=before) { return false; }set(a,after);++writes;return true;
    }
    LifetimeFixture() {
        set(roster+0x820,owner);set(activity+0x24,0x80B2E043U);set(root+8,1U);
        set(root+0xC,std::array<std::uint32_t,6>{21,0,0x4786C0E0U,0x80FEB3DCU,UINT32_MAX,UINT32_MAX});
        // Match the captured service object: +0x24 is zero, not a self-handle.
        set(life,Ref{0x80FE12B6U,0x80809916U,0x718});set(life+0x170,UINT32_MAX);set(life+0x180,3U);
        set(roster+0x808,pool);set(pool+8,record);set(pool+0x14,1500U);set(pool+0x18,1U);
        set(pool+0x1C,0x80U);set(pool+0x20,0x88U);set(pool+0x34,0x393U);
        set(record,Identity{0x4786C0E0U,17,0,3});set(record+12,0x8080991AU);set(record+0x80,0x0F5DB7C1U);
    }
};
