#pragma once
#include "../../../server/runtime/activity/adventure_cue_feedback.h"
#include <limits>

namespace dawn::client::hooks::bootflow::adventure_cue_native_identity {
namespace feedback=server::runtime::activity::adventure::cue_feedback;
struct Identity final {
    feedback::Source source{};
    std::uint32_t componentLink{UINT32_MAX};
    friend bool operator==(const Identity&,const Identity&)=default;
};

// Original 4E5640 obtains the component's self reference from the common pool
// row selected by component+20. The reference at component+160 is optional and
// is absent in the captured native type68 component; it is not the self source.
template<class Reader,class Resolver>
[[nodiscard]] bool capture(std::uintptr_t component,std::uintptr_t pool,std::uint32_t stride,
    Reader&& read,Resolver&& resolve,Identity& out) noexcept {
    out={};
    constexpr auto maximum=std::numeric_limits<std::uintptr_t>::max();
    if(component<0x10000 || component>maximum-0x24 || pool<0x10000 || stride<0x24 || stride>0x100000)return false;
    std::array<std::byte,4> raw{};
    if(!read(component+0x20,std::span(raw)))return false;
    const auto link=feedback::field<std::uint32_t>(raw,0);
    if(link==UINT32_MAX)return false;
    const auto slot=link&0x1FFFU;
    const auto distance=std::uint64_t{slot}*stride+0x20;
    if(distance>maximum-4 || pool>maximum-distance-4 || !read(pool+distance,std::span(raw)))return false;
    const auto member=feedback::field<std::uint32_t>(raw,0);
    if(member==UINT32_MAX || (member&0x1FFFU)!=slot)return false;
    std::uintptr_t base{};
    if(!resolve(member,0,base) || base<0x10000 || base>component || component-base>=0x2000000)return false;
    out={{member,static_cast<std::int64_t>(component-base)},link};
    return true;
}
}
