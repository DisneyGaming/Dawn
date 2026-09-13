#pragma once
#include "deadly_trial_lifetime_fixture.h"
#include "client/hooks/bootflow/deadly_trial_presentation_binding.h"

// The failed live run retained both allocated records, but their root owner
// and runtime sync associations were absent. Presentation bodies stay native.
struct TrialPresentationFixture : LifetimeFixture {
    static constexpr std::uintptr_t objective=0x4400;
    static constexpr std::uint32_t objectiveComponent=0x22F90014,objectiveSync=sync+1;
    bool resolve(std::uint32_t h,std::uintptr_t& a) {
        if(h==objectiveComponent) { a=objective;return true; }
        if(h==objectiveSync) { a=record+0x88;return true; }
        return LifetimeFixture::resolve(h,a);
    }
    bool lookup(const Identity& id,Ref& ref) {
        if(!found || id.key!=0xC9BC773AU) { return false; }
        if(ownerChanged) { set(root+0x1C,7U); }
        if(id.type==53 && id.slot==2) { ref={component,0x80804F4BU,0};return true; }
        if(id.type==68 && id.slot==0) { ref={objectiveComponent,0x80804F53U,0};return true; }
        return false;
    }
    bool allocated(std::uintptr_t p,std::uint16_t i) { return active && p==pool && i<3; }
    TrialPresentationFixture() {
        set(root+0xC,std::array<std::uint32_t,6>{3,0,0xC9BC773AU,0x80B2ED51U,UINT32_MAX,UINT32_MAX});
        set(life,Ref{0x80B2E709U,0x80804F4CU,0x1408});set(life+0x48,component);
        set(objective,Ref{0x80B2E706U,0x80804F54U,0xB88});set(objective+0x48,objectiveComponent);
        set(objective+0x170,UINT32_MAX);set(objective+0x180,0xDA95AE49U);
        set(pool+0x18,2U);
        set(record,Identity{0xC9BC773AU,53,0,2});set(record+12,0x80804F77U);
        set(record+0x88,Identity{0xC9BC773AU,68,0,0});set(record+0x88+12,0x80804F67U);
        set(record+0x88+0x80,0x0F5DB7C1U);
    }
};
