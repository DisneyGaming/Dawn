#pragma once
#include "frame.h"
#include <cmath>
namespace sunrise::state::activity::deep_storage::plate_presentation {
// 80F56977: device_position input0 at 1AC0, timer_value input1 at 1AE8.
// Final plates use the red .2 branch while waiting, .1 while charging, and
// remove their rod with 0 once charged. The entry plate keeps its completed .1.
// Preloaded final plates remain red until armed, even if occupied.
// Keep the native timer complete; visual removal is not an occupancy reset.
inline float position(const PlateState& state,std::uint8_t index) noexcept {
    if(index) {return state.charged?0.F:!state.armed || state.contested || !state.occupied?.2F:.1F;}
    return state.charged?.1F:state.contested?.2F:state.occupied?.1F:0.F;
}
// DF6BD0 acknowledges +960. DF6D32 writes the target +37C; snap also writes
// current +370 at DF6D3F. Check both so a pending return to idle is repaired.
// Current revalidates the source generation, salted entity and both components.
// Apply is the native setter, never a write to an effect or completion latch.
template<class Read,class Current,class Apply>
bool reconcile(const PlateRequest& request,Read& read,std::uintptr_t device,Current current,Apply apply) noexcept {
    if(!request.enabled || (!request.state.armed && request.plate.index==0) || !request.plate.valid() || !current()) {return false;}
    float actual{},target{};std::uint32_t revision{};
    if(!read.value(device+0x370,actual) || !read.value(device+0x37C,target) || !read.value(device+0x960,revision)
        || !std::isfinite(actual) || !std::isfinite(target) || !current()) {return false;}
    const auto desired=position(request.state,request.plate.index);
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
