#pragma once
#include <cmath>
#include <cstdint>
namespace dawn::state::activity::coo::plate_presentation {
// DF6BD0 acknowledges +960. DF6D32 writes the target +37C; snap also writes
// current +370 at DF6D3F. Check both so a pending return to idle is repaired.
// Current revalidates the source generation, salted entity and both components.
// Apply is the native setter, never a write to an effect or completion latch.
template<class Request,class Read,class Current,class Apply>
bool reconcile(const Request& request,Read& read,std::uintptr_t device,float desired,Current current,Apply apply) noexcept {
    if(!request.enabled || !request.plate.valid() || !current()) {return false;}
    float actual{},target{};std::uint32_t revision{};
    if(!read.value(device+0x370,actual) || !read.value(device+0x37C,target) || !read.value(device+0x960,revision)
        || !std::isfinite(actual) || !std::isfinite(target) || !current()) {return false;}
    if(actual==desired && target==desired) {return true;}
    if(revision==UINT32_MAX-1U) {return false;}
    const auto next=revision==UINT32_MAX?0U:revision+1U;
    apply(desired,next);
    std::uint32_t accepted{};
    return current() && read.value(device+0x370,actual) && actual==desired
        && read.value(device+0x37C,target) && target==desired
        && read.value(device+0x960,accepted) && accepted==next && current();
}
}
