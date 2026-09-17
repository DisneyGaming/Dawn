#pragma once
#include "../../../src/client/hooks/bootflow/beyond_infinity_future_cast.h"
#include <vector>
namespace beyond_future_cast_fixture {
namespace cast=dawn::client::hooks::bootflow::beyond_future_cast;
struct NativeOracle {
    std::vector<unsigned> calls;bool okay{true};unsigned fail{};std::byte* request{};std::uint32_t destroyed{};
    bool initialize(std::byte* p,std::uint32_t tag) {
        calls.push_back(1);request=p;
        auto get=[&](unsigned o) {std::uint64_t v{};std::memcpy(&v,p+o,8);return v;};
        okay=okay && tag==0x80EC1113U && get(0)==0 && get(0x10)==0 && get(0x20)==0x800 && get(0x28)==0x10 && get(0x30)==0;
        std::uint32_t absent{};std::memcpy(&absent,p+0x18,4);okay=okay && absent==UINT32_MAX;
        const std::int64_t relative=fail==2?0x900:0x40;std::memcpy(p,&relative,8);std::memcpy(p+0x40,&tag,4);return fail!=1;
    }
    std::byte* enrich(std::byte*) { calls.push_back(2);return fail==3?request+0x83F:request+0xD0; }
    void owner(std::byte* p,std::uint32_t self) {calls.push_back(3);okay=okay && p==request+0x40 && self==0x22F9EA0CU && p[0x68]==std::byte{0x69};}
    void pose(std::uintptr_t child,std::byte* p) {calls.push_back(4);okay=okay && child==0x12345000;for(unsigned i=0;i<32;++i)p[i]=std::byte(i);}
    void place(std::byte* p,std::byte* pose) {calls.push_back(5);okay=okay && p==request+0x40;for(unsigned i=0;i<32;++i)okay=okay && pose[i]==std::byte(i);}
    void factory(std::uint32_t* out,std::byte* p) {calls.push_back(6);float radius{};std::memcpy(&radius,p+0x5C,4);okay=okay && radius==500.F && request[0xE2]==std::byte{1} && *out==UINT32_MAX;*out=fail==4?UINT32_MAX:0x72F42014U;}
    bool bind(std::uintptr_t child,std::uint32_t name,std::uint32_t actor) {calls.push_back(7);okay=okay && child==0x12345000 && name==0xFE68952BU && actor==0x72F42014U;return fail!=5;}
    bool adopt(std::uintptr_t,std::uint32_t) {calls.push_back(8);return fail!=6;}
    void visibility(std::uintptr_t,std::uint32_t) {calls.push_back(9);}
    void destroy(std::uint32_t actor) {calls.push_back(10);destroyed=actor;}
};
template<class Check> void run(Check check) {
    cast::Identity id{0x80EC0872,0x808084E9,0x2648,0x80EC0872,0x808084E1,0x2950,
        0x22F9EA0C,0x22F9EA0C,0x123,0x123,UINT32_MAX,UINT32_MAX,1,2,0,0};
    check(cast::eligible(id),"missing main actor after authored hold handoff qualifies");
    auto bad=id;bad.graph=0x80EC0910;check(!cast::eligible(bad),"hold child cannot receive main fallback");
    bad=id;bad.parameterDefinition=0xB00;check(!cast::eligible(bad),"foreign actor parameter rejected");
    bad=id;bad.parent++;check(!cast::eligible(bad),"foreign parent rejected");
    bad=id;bad.self++;check(!cast::eligible(bad),"recycled child rejected");
    bad=id;bad.actorSerial=5;check(!cast::eligible(bad),"partially populated weak rejected");
    bad=id;bad.actorHandle=5;check(!cast::eligible(bad),"existing actor preserved");
    bad=id;bad.owned=1;check(!cast::eligible(bad),"owned actor cannot be replaced");
    bad=id;bad.associated=1;check(!cast::eligible(bad),"pending native association cannot be replaced");
    bad=id;bad.mainState=2;check(!cast::eligible(bad),"completed main cannot spawn");
    bad=id;bad.holdState=1;check(!cast::eligible(bad),"active hold cannot duplicate actor");
    cast::FirstObservation first;
    check(!first.take(0,1,4,5) && !first.take(1,1,UINT32_MAX,5),"invalid run/weak cannot reserve fallback");
    check(first.take(1,1,4,5) && !first.take(1,1,4,5),"later actor disappearance in same child cannot respawn");
    check(first.take(2,1,4,5),"fresh run has its own first observation");
    NativeOracle normal;check(cast::create(normal,0x12345000,0x22F9EA0C) && normal.okay,"native fallback ABI and owned creation sequence match disassembly");
    check(normal.calls==std::vector<unsigned>({1,2,3,4,5,6,7,8,9}) && !normal.destroyed,"native ownership is installed before visibility");
    for(unsigned fail=1;fail<=6;++fail) {
        NativeOracle api;api.fail=fail;check(!cast::create(api,0x12345000,0x22F9EA0C),"creation failure cannot report success");
        check(api.okay,"failure path retains verified ABI");
        check((fail>=5)==(api.destroyed==0x72F42014U),"failed bind/adoption destroys only newly created actor");
        for(auto call:api.calls)check(call!=9,"failed actor never receives visibility publication");
    }
}
}
