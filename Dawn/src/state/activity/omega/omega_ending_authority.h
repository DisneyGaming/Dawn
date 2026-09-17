#pragma once
#include "omega_ending_transit.h"

namespace dawn::state::activity::omega::ending {
inline constexpr bool slot(std::uint32_t registry,std::uint8_t type,std::uint16_t index) noexcept {
    return registry==kRegistry && type==6 && index==0;
}
template<class Writer> bool write(Writer& writer,std::uint32_t revision,bool play) noexcept {
    // Untouched 80804F08 reflection/default program: empty counts are 5 and 3
    // bits; reference -1/-1 is absent; native play revision is unsigned.
    return writer.write(UINT64_MAX,64) && writer.write(0,64)
        && writer.write(play?revision:0,32) && writer.write(play?1U:0U,1) && writer.write(0,1)
        && writer.write(0x811C9DC5,32) && writer.write(0,7) && writer.write(0x7FFF,16)
        && writer.write(2,6) && writer.write(0,5) && writer.write(0,3) && writer.write(0,32);
}
} // namespace dawn::state::activity::omega::ending
