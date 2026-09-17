#pragma once
#include "frame.h"
#include "../coo/plate_presentation.h"
namespace dawn::state::activity::deep_storage::plate_presentation {
// 80F56977: device_position input0 at 1AC0, timer_value input1 at 1AE8.
// Final plates use the red .2 branch while waiting, .1 while charging, and
// remove their rod with 0 once charged. The entry plate keeps its completed .1.
// Preloaded final plates remain red until armed, even if occupied.
// Keep the native timer complete; visual removal is not an occupancy reset.
inline float position(const PlateState& state,std::uint8_t index) noexcept {
    if(index) {return state.charged?0.F:!state.armed || state.contested || !state.occupied?.2F:.1F;}
    return state.charged?.1F:state.contested?.2F:state.occupied?.1F:0.F;
}
template<class Read,class Current,class Apply>
bool reconcile(const PlateRequest& request,Read& read,std::uintptr_t device,Current current,Apply apply) noexcept {
    if(!request.state.armed && request.plate.index==0) {return false;}
    return coo::plate_presentation::reconcile(request,read,device,request.capture.presentationPosition,current,apply);
}
}
