#pragma once
#include "fixtures/deadly_trial_lifetime_fixture.h"
#include "../src/state/activity/hijacked/catalog.h"
#include <cstdio>

inline bool hijacked_lifetime_binding() {
    namespace binding=sunrise::client::hooks::bootflow::deadly_trial_lifetime;
    constexpr auto scenario=sunrise::state::activity::hijacked::kScenario;
    static_assert(scenario==0x80B4206AU);
    using F=LifetimeFixture;
#define HIJACKED_LIFETIME_CHECK(expression) do {if(!(expression)) {std::fprintf(stderr,"FAIL Hijacked lifetime line %d: %s\n",__LINE__,#expression);return false;}} while(false)
    F f;f.set(F::activity+0x24,scenario);
    // Preserve a nonterminal native body. The real completion packet, not the
    // binding repair, must subsequently apply its terminal lifetime value.
    f.set(F::life+0x180,3U);f.set(F::life+0x184,0xA5C37E19U);
    const auto before=f.bytes;
    HIJACKED_LIFETIME_CHECK(binding::repair(f,f,F::roster,scenario)==binding::Result::repaired);
    HIJACKED_LIFETIME_CHECK(f.writes==2 && f.get<std::uint32_t>(F::root+0x1C)==F::owner);
    HIJACKED_LIFETIME_CHECK(f.get<std::uint32_t>(F::life+0x170)==F::sync);
    for(std::size_t i=0;i<f.bytes.size();++i) {
        if((i>=F::root+0x1C && i<F::root+0x20) || (i>=F::life+0x170 && i<F::life+0x174)) {continue;}
        HIJACKED_LIFETIME_CHECK(f.bytes[i]==before[i]);
    }
    HIJACKED_LIFETIME_CHECK(f.get<std::uint32_t>(F::life+0x180)==3U);
    const auto repaired=f.bytes;
    HIJACKED_LIFETIME_CHECK(binding::repair(f,f,F::roster,scenario)==binding::Result::unchanged);
    HIJACKED_LIFETIME_CHECK(f.writes==2 && f.bytes==repaired);
    // The original default remains scoped to A Deadly Trial.
    F oldDefault;
    HIJACKED_LIFETIME_CHECK(binding::repair(oldDefault,oldDefault,F::roster)==binding::Result::repaired);
    F wrongDefault;wrongDefault.set(F::activity+0x24,scenario);
    HIJACKED_LIFETIME_CHECK(binding::repair(wrongDefault,wrongDefault,F::roster)==binding::Result::unavailable);
    HIJACKED_LIFETIME_CHECK(wrongDefault.writes==0);
    for(unsigned test=0;test<8;++test) {
        F bad;bad.set(F::activity+0x24,scenario);auto expected=scenario;
        switch(test) {
        case 0:bad.set(F::activity+0x24,0x80B2E043U);break; // Foreign mission cannot use Hijacked permission.
        case 1:expected=0x80B4206BU;bad.set(F::activity+0x24,expected);break; // Unsupported matching scenario still rejected.
        case 2:bad.set(F::roster+0x820,F::owner^0x02000000U);break; // Same low index, different full owner handle.
        case 3:bad.set(F::root+0x1C,F::owner^0x02000000U);break; // Another salted group owner cannot be stolen.
        case 4:bad.set(F::record+0x80,1U);break; // Reused sync allocation cannot bind.
        case 5:bad.set(F::life+0x170,F::sync^0x02000000U);break; // Retained foreign sync cannot be replaced.
        case 6:bad.set(F::life+4,0x80809917U);break; // Wrong lifetime definition.
        case 7:bad.race=true;break; // Concurrent owner assignment wins.
        }
        const auto unchanged=bad.bytes;
        HIJACKED_LIFETIME_CHECK(binding::repair(bad,bad,F::roster,expected)==binding::Result::unavailable);
        HIJACKED_LIFETIME_CHECK(bad.writes==0 && bad.bytes==unchanged);
    }
#undef HIJACKED_LIFETIME_CHECK
    return true;
}
