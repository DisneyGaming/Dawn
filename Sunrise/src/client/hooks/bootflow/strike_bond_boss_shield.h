#pragma once
#include "strike_bond_intro_release.h"
#include "omega_boss_health_identity.h"
#include <bit>
#include <cmath>
namespace sunrise::client::hooks::bootflow::strike_bond_boss_shield {
namespace mission=state::activity::strike_bond;
namespace trace=strike_bond_fire_trace;
// Live pass isolation: hiding all passes hid body and shield; hiding pass 7
// alone hid the shield and retained the visible, functional Dendron body.
inline constexpr std::uint8_t kShieldPass=7;
inline constexpr std::uintptr_t kDrawRva=0x1150420;
inline constexpr std::array<std::uint8_t,16> kDrawPrefix{
    0x48,0x83,0xEC,0x48,0x8B,0x41,0x20,0x83,0xF8,0xFF,0x74,0x2D,0x89,0x54,0x24,0x20};
inline bool visible(const mission::Frame& f,float fraction) noexcept {
    return f.enabled && !f.finished && !f.ending && !f.bossDead
        && f.bossCycle.mode!=mission::BossMode::dying && mission::boss_blocked(f,fraction);
}
inline bool wanted(const mission::BossRequest& r) noexcept {
    return trace::admitted(r) && r.frame.region==136 && r.frame.bossStage<=2;
}
// This is the native pass-reference reduction used by the model caller. Keep
// its counters untouched, and restore their current effective count when shown.
inline std::uint8_t enabled_count(std::uint16_t flags) noexcept {
    return static_cast<std::uint8_t>(std::popcount(static_cast<unsigned>(flags&((flags&0x1E00U)?0x1E0U:0x1E1U))));
}
struct Binding {
    trace::Identity character{};
    std::uintptr_t model{},health{};
    std::uint32_t modelSelf{},healthSelf{},renderer{},displayOwner{};
    std::uint16_t passFlags{};
    friend bool operator==(const Binding&,const Binding&)=default;
};
struct Draw {std::uintptr_t object{};std::uint32_t renderer{};std::uint8_t pass{},count{};};
inline Draw command(const Binding& b,bool show) noexcept {
    return {b.model+0x1A0,b.renderer,kShieldPass,show?enabled_count(b.passFlags):std::uint8_t{0}};
}
template<class Read> bool boundaries(Read& read,std::uintptr_t image) noexcept {
    std::array<std::uint8_t,16> draw{};std::array<std::byte,16> fraction{};
    return read.value(image+kDrawRva,draw) && draw==kDrawPrefix
        && read.value(image+0xCD6C20,fraction) && omega_boss_health::fraction_getter_prefix(fraction);
}
template<class Read> bool sample(Read& read,std::uintptr_t image,std::uintptr_t character,
    const mission::BossRequest& r,Binding& out) noexcept {
    if(!wanted(r)) return false;
    Binding b{};std::uintptr_t rows{};std::uint32_t stride{},bundle{},value{};
    if(!trace::sample(read,image,character,false,r,b.character)
        || !read.value(image+0x1F93428,rows) || !read.value(image+0x1F93430,stride) || stride<0xE0 || stride>0x1000) return false;
    const auto row=rows+static_cast<std::uintptr_t>(b.character.entity&0x1FFFU)*stride;
    if(!read.value(row+12,value) || value!=b.character.entity || !read.value(row+4,value) || (value&5U)
        || !read.value(row+0x4C,bundle)
        || !coo_native::component<Read,1024>(read,bundle,b.character.entity,0x808072BDU,b.model)
        || !coo_native::component<Read,1024>(read,bundle,b.character.entity,0x80804B8AU,b.health)
        || !strike_bond_intro_release::component_header(read,b.model,0x80F4599FU,0x808072BDU,0x790,b.character.entity,b.modelSelf)
        || !strike_bond_intro_release::component_header(read,b.health,0x815B5A47U,0x80804B8AU,0xF28,b.character.entity,b.healthSelf)) return false;
    std::array<std::byte,0x28> display{};
    if(!read.copy(b.model+0x1A0,display) || trace::field<std::uint32_t>(display,0)!=0x80F4599FU
        || trace::field<std::uint32_t>(display,4)!=0x80807315U || trace::field<std::uint64_t>(display,8)!=0x978
        || trace::field<std::int64_t>(display,0x10)!=-0x1B0
        || !read.value(b.model+0xC0,b.renderer) || b.renderer==UINT32_MAX
        || !read.value(b.model+0xC4+2*kShieldPass,b.passFlags)) return false;
    b.displayOwner=trace::field<std::uint32_t>(display,0x20);
    if(b.displayOwner==UINT32_MAX) return false;
    out=b;return true;
}
}
