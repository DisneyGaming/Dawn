#include "mechanism_bindings.h"
namespace sunrise::state::activity::deep_storage {
float device_position(coo::Asset a,bool active) noexcept {
    // 80F567A6: mode0 raises the barrier and selects collision state0F0B176F;
    // mode1 fades both outputs to0 and selects removal state1575D743.
    if(a==coo::Asset{0x59700FA7U,0x80B566FDU,23,50}) {return active?0.F:1.F;}
    // This static descent doorway is never used by Deep Storage. Native0 raises
    // its model/physics; .2 removes both (80F4BA2D / 80F26F00 / 80F26F01).
    if(a==coo::Asset{0xE6402111U,0x80B56A48U,23,0}) {return .2F;}
    if(!active) {return 0.F;}
    // 8157EB3E: .5 raises the catch outputs; both0 and1 fade them to0.
    // Use0 for the ordinary hidden state;1 also runs the terminal sequence.
    if(a==coo::Asset{0x59700FA7U,0x80B5690EU,23,93} || a==coo::Asset{0x59700FA7U,0x80B56914U,23,94}) {return .5F;}
    // PACKAGE: frame's supported active ranges are .1/.2; 1 matches no branch.
    if(a.registry==0x59700FA7U && a.type==23 && a.slot==38) {return .1F;}
    // 80F2625B action6 (.5) rises to1; action9 (1) adds 3501 and fades to0.
    if(a==coo::Asset{0x59700FA7U,0x80B5690BU,23,92}) {return .5F;}
    return 1.F;
}
}
