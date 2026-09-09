#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace sunrise::client::hooks::bootflow::beyond_future_cast {
inline constexpr std::uint32_t kActor=0x80EC1113U,kParameter=0xFE68952BU;
struct Identity {
    std::uint32_t graph{},kind{};std::uint64_t definition{};
    std::uint32_t parameterGraph{},parameterKind{};std::uint64_t parameterDefinition{};
    std::uint32_t self{},weakHandle{},parent{},expectedParent{};
    std::uint32_t actorSerial{},actorHandle{};
    std::uint8_t mainState{},holdState{},owned{},associated{};
};
inline bool eligible(const Identity& v) noexcept {
    return v.graph==0x80EC0872U && v.kind==0x808084E9U && v.definition==0x2648
        && v.parameterGraph==v.graph && v.parameterKind==0x808084E1U && v.parameterDefinition==0x2950
        && v.self!=UINT32_MAX && v.self==v.weakHandle && v.parent!=UINT32_MAX && v.parent==v.expectedParent
        && v.mainState==1 && v.holdState==2 && v.actorSerial==UINT32_MAX && v.actorHandle==UINT32_MAX
        && v.owned==0 && v.associated==0;
}
struct FirstObservation {
    std::uint64_t run{};std::uint32_t generation{},serial{UINT32_MAX},handle{UINT32_MAX};
    bool take(std::uint64_t r,std::uint32_t g,std::uint32_t s,std::uint32_t h) noexcept {
        if(!r || !g || s==UINT32_MAX || h==UINT32_MAX) { return false; }
        if(run==r && generation==g && serial==s && handle==h) { return false; }
        run=r;generation=g;serial=s;handle=h;return true;
    }
};
// Exact stack-backed request used by5902C0: +28 is a relative buffer pointer,
// +20 is capacity, +30 is consumed bytes. Native4AAF90 aligns within this buffer.
struct alignas(16) Request {
    std::array<std::byte,0x840> bytes{};
    Request() noexcept { put<std::uint32_t>(0x18,UINT32_MAX);put<std::uint64_t>(0x20,0x800);put<std::int64_t>(0x28,0x10); }
    template<class T> void put(std::size_t o,T v) noexcept { std::memcpy(bytes.data()+o,&v,sizeof v); }
    template<class T> T get(std::size_t o) const noexcept { T v{};std::memcpy(&v,bytes.data()+o,sizeof v);return v; }
    std::byte* payload() noexcept {
        const auto offset=get<std::int64_t>(0);
        if(offset<0x38 || offset>static_cast<std::int64_t>(bytes.size()-0x90) || (offset&15)!=0) { return nullptr; }
        return bytes.data()+offset;
    }
    bool contains(const std::byte* p,std::size_t n) const noexcept {
        const auto first=reinterpret_cast<std::uintptr_t>(bytes.data()),at=reinterpret_cast<std::uintptr_t>(p);
        return at>=first+0x38 && n<=bytes.size() && at-first<=bytes.size()-n;
    }
};
// Backend signatures and argument order mirror5903BF..590552. The native binder
// installs the weak and association; owned marks native58E630 cleanup responsibility.
template<class Api> bool create(Api& api,std::uintptr_t child,std::uint32_t self) noexcept {
    Request request;
    if(!api.initialize(request.bytes.data(),kActor)) { return false; }
    auto* payload=request.payload();
    if(!payload || request.get<std::uint32_t>(static_cast<std::size_t>(payload-request.bytes.data()))!=kActor) { return false; }
    auto* extra=api.enrich(request.bytes.data());
    if(extra && !request.contains(extra,0x13)) { return false; }
    payload[0x68]|=std::byte{0x69};
    api.owner(payload,self);
    alignas(16) std::array<std::byte,32> pose{};api.pose(child,pose.data());api.place(payload,pose.data());
    if(extra) { extra[0x12]=std::byte{1}; }
    const float radius=500.F;std::memcpy(payload+0x5C,&radius,4);
    std::uint32_t actor=UINT32_MAX;api.factory(&actor,payload);
    if(actor==UINT32_MAX) { return false; }
    if(!api.bind(child,kParameter,actor)) { api.destroy(actor);return false; }
    if(!api.adopt(child,actor)) { api.destroy(actor);return false; }
    api.visibility(child,actor);return true;
}
}
