#pragma once
#include "native_catalog.h"
#include "../../coo/native_device_authority.h"

namespace sunrise::state::activity::newlight::launchpad::lighting {
inline constexpr auto kSource=asset(kBreach,4,1);
inline constexpr std::size_t kAuthorityBits=573;
// Type-4 dynamic state 80805063 is consumed by DF6510 on the native object
// update. Keep power/lock and snap revisions absent; only request position.
// Calling DF6BD0 from camera polling can stall the world job's event queue.
template<class Writer> bool write(Writer& w,std::uint32_t generation) noexcept {
    if(!generation || generation>=0x7FFFFFFFU) {return false;}
    const auto begin=w.bit_count();
    return coo::native_device::object_header(w,generation,true,1)
        && w.write(1,1) && w.write(0x80805063U,32)
        && w.write(0x7FFFFFFFU,32) && w.write(0x7FFFFFFFU,32) && w.write(0x3F800000U,32)
        && w.write(0x7FFFFFFFU,32) && w.write(0x7FFFFFFFU,32) && w.write(0,32)
        && w.write(generation^0x80000000U,32) && w.write(0x7FFFFFFFU,32) && w.write(0x3F800000U,32)
        && w.bit_count()-begin==kAuthorityBits;
}
}
