#pragma once
#include "fixtures/deadly_trial_presentation_fixture.h"
#include "../src/state/activity/hijacked/catalog.h"
#include <cstdio>

struct HijackedPresentationFixture : TrialPresentationFixture {
    static constexpr std::uint32_t scenario=0x80B4206A,registry=0x77852DB9,rootTag=0x80B42ADB;
    bool lookup(const Identity& id,Ref& ref) {
        if(!found || id.key!=registry)return false;
        if(ownerChanged)set(root+0x1C,7U);
        if(id.type==53 && id.slot==2){ref={component,0x80804F4B,0};return true;}
        if(id.type==68 && id.slot==0){ref={objectiveComponent,0x80804F53,0};return true;}
        return false;
    }
    HijackedPresentationFixture() {
        set(activity+0x24,scenario);
        set(root+0xC,std::array<std::uint32_t,6>{3,0,registry,rootTag,UINT32_MAX,UINT32_MAX});
        set(life,Ref{0x80B4241F,0x80804F4C,0x1408});
        set(objective,Ref{0x80B4241C,0x80804F54,0xB88});
        set(record,Identity{registry,53,0,2});
        set(record+0x88,Identity{registry,68,0,0});
    }
};

inline bool hijacked_presentation_binding() {
    namespace binding=sunrise::client::hooks::bootflow::deadly_trial_lifetime;
    using F=HijackedPresentationFixture;
    static_assert(F::scenario==sunrise::state::activity::hijacked::kScenario);
    static_assert(F::registry==sunrise::state::activity::hijacked::kRoot);
#define HIJACKED_PRESENTATION_CHECK(expression) do {if(!(expression)) {std::fprintf(stderr,"FAIL Hijacked presentation line %d: %s\n",__LINE__,#expression);return false;}} while(false)
    // Reproduce the live failure: both native components still exist with their
    // opening bodies, but their authority associations were cleared at handoff.
    F f;const auto before=f.bytes;
    HIJACKED_PRESENTATION_CHECK(binding::repair_presentation(f,f,F::roster,F::scenario)==binding::Result::repaired);
    HIJACKED_PRESENTATION_CHECK(f.writes==3 && f.get<std::uint32_t>(F::root+0x1C)==F::owner);
    HIJACKED_PRESENTATION_CHECK(f.get<std::uint32_t>(F::life+0x170)==F::sync);
    HIJACKED_PRESENTATION_CHECK(f.get<std::uint32_t>(F::objective+0x170)==F::objectiveSync);
    for(std::size_t i=0;i<f.bytes.size();++i) {
        if((i>=F::root+0x1C && i<F::root+0x20)
            || (i>=F::life+0x170 && i<F::life+0x174)
            || (i>=F::objective+0x170 && i<F::objective+0x174))continue;
        HIJACKED_PRESENTATION_CHECK(f.bytes[i]==before[i]);
    }
    const auto repaired=f.bytes;
    HIJACKED_PRESENTATION_CHECK(binding::repair_presentation(f,f,F::roster,F::scenario)==binding::Result::unchanged);
    HIJACKED_PRESENTATION_CHECK(f.writes==3 && f.bytes==repaired);
    // A later handoff can clear just one association; leave the other intact.
    f.set(F::objective+0x170,UINT32_MAX);
    HIJACKED_PRESENTATION_CHECK(binding::repair_presentation(f,f,F::roster,F::scenario)==binding::Result::repaired);
    HIJACKED_PRESENTATION_CHECK(f.get<std::uint32_t>(F::life+0x170)==F::sync);
    HIJACKED_PRESENTATION_CHECK(f.get<std::uint32_t>(F::objective+0x170)==F::objectiveSync);
    F wrongDefault;
    HIJACKED_PRESENTATION_CHECK(binding::repair_presentation(wrongDefault,wrongDefault,F::roster)==binding::Result::unavailable);
    HIJACKED_PRESENTATION_CHECK(wrongDefault.writes==0);
    for(unsigned variant=0;variant<22;++variant) {
        F bad;auto expected=F::scenario;
        switch(variant) {
        case 0:bad.set(F::activity+0x24,0x80B2E043U);break;
        case 1:expected=0x80B4206BU;bad.set(F::activity+0x24,expected);break;
        case 2:bad.set(F::root+0x1C,F::owner^0x02000000U);break;
        case 3:bad.set(F::roster+0x820,F::owner^0x02000000U);break;
        case 4:bad.set(F::life+0x170,F::sync^0x02000000U);break;
        case 5:bad.set(F::objective+0x170,F::objectiveSync^0x02000000U);break;
        case 6:bad.set(F::life,0x80B2E709U);break;
        case 7:bad.set(F::objective,0x80B2E706U);break;
        case 8:bad.set(F::objective+8,std::int64_t{0xB89});break;
        case 9:bad.set(F::life+0x48,F::objectiveComponent);break;
        case 10:bad.set(F::record+0x80,1U);break;
        case 11:bad.set(F::record+0x88+12,0x80804F77U);break;
        case 12:bad.active=false;break;
        case 13:bad.found=false;break;
        case 14:bad.race=true;break;
        case 15:bad.set(F::pool+0x18,4097U);break;
        case 16:bad.set(F::root+0xC,4U);break;
        case 17:bad.set(F::pool+0x18,3U);
            bad.set(F::record+2*0x88,binding::Identity{F::registry,53,0,2});break;
        case 18:bad.set(F::root+8,2U);
            bad.set(F::root+0xC+24,bad.get<std::array<std::uint32_t,6>>(F::root+0xC));break;
        case 19:bad.set(F::pool+0x18,1U);break;
        case 20:bad.set(F::pool+0x34,0x40000393U);break;
        case 21:bad.set(F::root+0x18,0x80B2ED51U);break;
        }
        const auto unchanged=bad.bytes;
        HIJACKED_PRESENTATION_CHECK(binding::repair_presentation(bad,bad,F::roster,expected)==binding::Result::unavailable);
        HIJACKED_PRESENTATION_CHECK(bad.writes==0 && bad.bytes==unchanged);
    }
    F changed;changed.ownerChanged=true;
    HIJACKED_PRESENTATION_CHECK(binding::repair_presentation(changed,changed,F::roster,F::scenario)==binding::Result::unavailable);
    HIJACKED_PRESENTATION_CHECK(changed.writes==0 && changed.get<std::uint32_t>(F::root+0x1C)==7U);
#undef HIJACKED_PRESENTATION_CHECK
    return true;
}
