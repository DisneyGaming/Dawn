#pragma once
#include <cstddef>
#include <cstdint>
namespace dawn::state::activity::deadly_trial::skiff {
inline constexpr std::size_t kMemberBits=462;
// Native 80807F73 union: one present command, type+1, completion+1.
// The stopping entrance ends at the authored hover anchor. Plain enter_45
// leaves forward velocity and carries the Skiff beyond that anchor.
template<class Writer> bool flight(Writer& w,std::uint32_t variant,std::uint16_t point) noexcept {
    return w.write(1,1) && w.write(10,4) && w.write(1,2)
        && w.write(0x07EBF354U,32) && w.write(variant,32) && w.write(0,32)
        && w.write(0xEB7AF018U,32) && w.write(59,7) && w.write(0x8000U+point,16)
        && w.write(0,3) && w.write(128,8); // No target actor; point index zero.
}
template<class Writer> bool member(Writer& w,std::uint32_t generation) noexcept {
    if(!generation || generation>0x7FFFFFFFU) { return false; }
    const auto begin=w.bit_count();
    const bool ok=w.write(1,1) && w.write(generation,31)
        && w.write(1,2) && w.write(1,3) && w.write(1,1)
        && w.write(0,1) && w.write(0,1) // Preserve native member placement defaults.
        && w.write(1,1) && w.write(generation,31) && w.write(0,6) && w.write(3,6)
        && flight(w,0x052C60A0U,135) // dropship / enter_45_stop
        && w.write(1,1) && w.write(3,4) && w.write(1,2) && w.write(0x40C00000U,32) // Native wait: 6 seconds.
        && flight(w,0x7D0D39A9U,136) // dropship / exit_45
        && w.write(0,1);
    // The queue revision is stable for this spawn generation. Ordinary mission
    // publications and Walker death must not rewind the native command head.
    return ok && w.bit_count()-begin==kMemberBits;
}
}
