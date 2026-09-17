#pragma once
#include "frame.h"
#include "../coo/plate_presentation.h"
namespace dawn::state::activity::hijacked::plate_presentation {
// Native80F56977: red waiting(.2), charge(.1). Completed altar keeps its .1 pose.
inline float position(const PlateState& s) noexcept {return s.charged?.1F:!s.armed || !s.occupied || s.contested?.2F:.1F;}
template<class Read,class Current,class Apply>
bool reconcile(const PlateRequest& request,Read& read,std::uintptr_t device,Current current,Apply apply) noexcept {
    return coo::plate_presentation::reconcile(request,read,device,request.capture.presentationPosition,current,apply);
}
}
