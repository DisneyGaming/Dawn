#pragma once
#include <cstdint>
#include <span>
#include "objective_service.h"
namespace sunrise::state::activity::coo::native_presentation {
// Native reflected schemas 80804F77 (dialogue) and 80804F67 (directive).
// The caller must validate the mission's exact descriptor before publication.
inline constexpr std::size_t kDialogueBits=19767, kDirectiveBits=4802;
template<class Writer> bool absent(Writer& w) noexcept {
    return w.write(0x811C9DC5U,32) && w.write(0,7) && w.write(32767U,16);
}
template<class Writer> bool dialogue(Writer& w,std::span<const std::uint32_t> generations,std::uint8_t active) noexcept {
    if(generations.size()>128 || (active!=0xFF && (active>=generations.size() || generations[active]==0))) { return false; }
    for(const auto generation:generations) { if(generation>0x7FFFFFFFU) { return false; } }
    const auto start=w.bit_count();if(!absent(w)) { return false; }
    for(std::size_t row=0;row<128;++row) {
        const auto generation=row<generations.size()?generations[row]:0U;
        const bool playing=row==active;
        if(!w.write(UINT64_MAX,64) || !w.write(playing?1U:0U,1) || (playing && !w.write(1,64))
            || !absent(w) || !w.write(std::uint64_t{generation}+0x80000000ULL,32)
            || !w.write(playing?3U:1U,2)) { return false; }
    }
    return w.bit_count()-start==kDialogueBits+(active==0xFF?0U:64U);
}
template<class Writer> bool directive_record(Writer& w,bool active,std::uint32_t event,MarkerTarget marker={}) noexcept {
    if(!w.write(active?event:0x811C9DC5U,32) || !w.write(0x80000000ULL,32)
        || !w.write(active?1U:0U,2) || !w.write(0,1)) { return false; }
    for(unsigned i=0;i<5;++i) { if(!w.write(UINT64_MAX,64)) { return false; } }
    if(!w.write(0,32)) { return false; }
    for(unsigned i=0;i<4;++i) { if(!w.write(0x7FFFFFFFULL,32)) { return false; } }
    if(!w.write(1,2) || !absent(w) || !w.write(1,3)) { return false; }
    for(unsigned i=0;i<4;++i) {
        const bool selected=active && i==0 && marker.valid();
        if(selected) {
            if(!w.write(marker.asset.registry,32) || !w.write(marker.asset.type+1U,7) || !w.write(marker.asset.slot+32768U,16)) { return false; }
        } else if(!absent(w)) { return false; }
        if(!absent(w)) { return false; }
        for(unsigned j=0;j<4;++j) { if(!w.write(selected?marker.locator[j]:0U,32)) { return false; } }
        if(!w.write(0,1)) { return false; }
    }
    return true;
}
template<class Writer> bool directive(Writer& w,std::uint32_t event) noexcept {
    if(event==0 || event==UINT32_MAX) { return false; }
    const auto start=w.bit_count();
    return absent(w) && absent(w) && directive_record(w,true,event) && directive_record(w,false,event)
        && directive_record(w,false,event) && w.write(1,3) && w.bit_count()-start==kDirectiveBits;
}
}

namespace sunrise::state::activity::coo::native_presentation {
template<class Writer> bool objective(Writer& w,const ObjectiveState& state) noexcept {
    if(!state.published) { return false; }
    const auto begin=w.bit_count();
    return absent(w) && absent(w) && directive_record(w,state.active,state.event,state.marker)
        && directive_record(w,false,state.retiredEvent) && directive_record(w,false,0) && w.write(1,3)
        && w.bit_count()-begin==kDirectiveBits;
}
}
