#pragma once
#include "omega_mission_state.h"
#include "omega_boss_authority.h"
#include <bit>

namespace sunrise::state::activity::omega::mission_devices {
struct Cannon {std::uint16_t core,fx,gate;std::uint32_t coreAsset,fxAsset,gateAsset;};
inline constexpr std::uint32_t kRegistry=0x95FB2E01U;
inline constexpr std::array<Cannon,4> kCannons{{
    {10,14,18,0x80F47588,0x80F47594,0x80F475A0},
    {11,15,19,0x80F4758B,0x80F47597,0x80F475A3},
    {12,16,20,0x80F4758E,0x80F4759A,0x80F475A6},
    {13,17,21,0x80F47591,0x80F4759D,0x80F475A9}}};
inline std::size_t cannon(std::uint32_t registry,std::uint8_t type,std::uint16_t slot) noexcept {
    if(registry==kRegistry) for(std::size_t i=0;i<kCannons.size();++i) {
        const auto& row=kCannons[i];
        if((type==4 && (slot==row.core || slot==row.fx)) || (type==23 && slot==row.gate)) return i;
    }
    return kCannons.size();
}
// Reflected 8080992F. Core mode0 retains the generation across preparation and
// activation. Mode1 FX must cross a generation to create its deferred candidate.
template<class Writer> bool source(Writer& writer,std::uint32_t generation,bool active,bool interaction=false) noexcept {
    if(!generation || generation>0x7FFFFFFFU) return false;
    return writer.write(0x80000000U+generation,32) && writer.write(0x80000000U,32)
        && writer.write(active?1:0,1) && writer.write(0,1) && writer.write(0x80000000U,32)
        && boss_authority::write_reference(writer)
        && writer.write(0,32) && writer.write(0,32) && writer.write(0,32)
        && writer.write(0,1) && writer.write(interaction?1:0,2)
        // 80809AEA -> one 80809AE8 dynamic record. Original 9FA4B0 reads
        // a present schema tag, then its reflected payload. F33930 applies
        // 80804FB8 mode 1=locked / 2=unlocked and invalidates prompt cache.
        // F32820 otherwise initializes an interaction without setup as locked.
        && (!interaction || (writer.write(1,1) && writer.write(0x80804FB8U,32)
            && writer.write(active?3:2,2) // signed enum with +1 wire bias
            && boss_authority::write_reference(writer)
            && writer.write(0x80000000U,32) && writer.write(0,1)));
}
template<class Writer> bool channel(Writer& writer,float position,std::uint16_t revision) noexcept {
    return writer.write(std::bit_cast<std::uint32_t>(position),32)
        && writer.write(0x8000U+revision,16) && writer.write(0,1);
}
template<class Writer> bool gate(Writer& writer,float position,std::uint16_t revision) noexcept {
    return channel(writer,position,revision) && channel(writer,1.F,0) && channel(writer,0.F,0);
}
template<class Writer> bool write(Writer& writer,const mission::Snapshot& snapshot,
                                  std::uint32_t registry,std::uint8_t type,std::uint16_t slot) noexcept {
    const auto index=cannon(registry,type,slot);
    if(index==kCannons.size() || !snapshot.generation || snapshot.generation>=0x7FFFFFFFU) return false;
    const bool active=(snapshot.cannons&(1U<<index))!=0;
    if(type==23) return gate(writer,active?1.F:0.F,active?1:0);
    const bool fx=slot==kCannons[index].fx;
    return source(writer,snapshot.generation+(fx&&active?1U:0U),active);
}
} // namespace sunrise::state::activity::omega::mission_devices
