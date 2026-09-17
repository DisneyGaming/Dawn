#pragma once
#include "frame.h"

namespace dawn::state::activity::beyond_infinity::forest {
// 80F44B2B / 808030ED: if_race_vex_active and if_race_fallen_active.
inline constexpr std::uint32_t kVex=0x67AF9045U,kFallen=0x89567586U;
inline constexpr std::uint32_t kHashClass=0x80800070U,kActive=0x050C5D2EU,kInactive=0x811C9DC5U;
inline constexpr std::size_t kSwitchBits=97;
inline constexpr bool selected(const Frame& f) noexcept { return f.enabled && f.spawnGeneration; }
inline constexpr std::uint32_t value(std::uint8_t pass,std::uint32_t key) noexcept {
    return ((key==kVex && pass!=2) || (key==kFallen && pass==2))?kActive:kInactive;
}
template<class Writer> bool write(Writer& writer,std::uint8_t pass) noexcept {
    // Write both keys: the native store updates entries and retains omitted keys.
    for(const auto key:{kVex,kFallen}) {
        if(!writer.write(key,32) || !writer.write(1,1) || !writer.write(kHashClass,32)
            || !writer.write(value(pass,key),32)) { return false; }
    }
    return true;
}
}
