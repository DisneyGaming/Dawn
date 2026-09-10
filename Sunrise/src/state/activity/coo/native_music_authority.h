#pragma once
#include "native_presentation_authority.h"
namespace sunrise::state::activity::coo::native_music {
// 80804F58. The selected bit is an authored candidate ordinal, never a sound-event hash.
template<class Writer> bool select(Writer& w,std::uint8_t candidate) noexcept {
    if(candidate>=128) { return false; }
    for(unsigned word=0;word<4;++word) {
        if(!w.write(static_cast<unsigned>(candidate)/32U==word?std::uint32_t{1}<<(candidate%32):0U,32)) { return false; }
    }
    for(unsigned i=0;i<129;++i) { if(!native_presentation::absent(w)) { return false; } }
    return true;
}
}
