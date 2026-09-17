#include "mechanism_bindings.h"
namespace dawn::state::activity::hijacked {
float device_position(coo::Asset a,bool active) noexcept {
    // Exact source uses 80F567A6, already verified for Deep Storage: zero closes, one removes.
    if(a==coo::Asset{0x153E22CDU,0x80B4235CU,23,29}) {return active?0.F:1.F;}
    // The final lighthouse projection graph has a .45-.55 presentation branch.
    if(a==coo::Asset{0xD997395EU,0x80B429CCU,23,25}) {return active?.5F:0.F;}
    return active?1.F:0.F;
}
}
