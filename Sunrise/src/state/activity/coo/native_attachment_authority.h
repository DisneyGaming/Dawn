#pragma once
#include <cstdint>
namespace sunrise::state::activity::coo::native_attachment {
/** Type-26 authored hop-on health layer; detachment retains native initial controls. */
template<class Writer> bool squad(Writer& w,std::uint32_t registry,std::uint16_t slot,bool attached) noexcept {
    if(attached && (!registry || registry==0x811C9DC5U || slot>0x7FFFU)) { return false; }
    if(!w.write(0,2)) { return false; }
    for(unsigned i=0;i<4;++i) { if(!w.write(0x80000000U,32)) { return false; } }
    return w.write(0x811C9DC5U,32) && w.write(0,7) && w.write(32767U,16)
        && w.write(attached?1U:0U,1)
        && (!attached || (w.write(0x80809157U,32) && w.write(registry,32)
            && w.write(2,7) && w.write(32768U+slot,16)));
}
}
