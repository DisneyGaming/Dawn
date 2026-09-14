#pragma once
#include "../../state/activity/nightfall/rules.h"
#include "../hooks/bootflow/gateway_native_read.h"
#include "../../core/logging/log.h"
#include <cstdio>

namespace sunrise::client::activity::nightfall_player {
namespace native = hooks::bootflow::gateway_native;
struct Observation { std::uint32_t entity{UINT32_MAX}; bool dead{}; native::Weak health{}; };
struct Cursor {
    std::uint64_t run{}, session{};
    Observation previous{};
    bool liveQualified{};
    void bind(std::uint64_t nextRun,std::uint64_t nextSession) noexcept {
        if(run==nextRun && session==nextSession) return;
        run=nextRun;session=nextSession;previous={};liveQualified=false;
    }
    [[nodiscard]] bool accepts(bool dead) noexcept {
        if(!dead) liveQualified=true;
        return liveQualified;
    }
};
inline constexpr std::uint64_t kMaximumComponentOffset=0x400000;
struct HealthView {
    std::uintptr_t address{};
    std::uint32_t self{UINT32_MAX};
    bool dead{};
};
[[nodiscard]] inline bool component_address(std::uintptr_t group,std::int64_t offset,
                                             std::uintptr_t& address) noexcept {
    if(group<0x10000 || offset < -static_cast<std::int64_t>(kMaximumComponentOffset)
        || offset > static_cast<std::int64_t>(kMaximumComponentOffset)) return false;
    if(offset>=0) {
        const auto amount=static_cast<std::uint64_t>(offset);
        if(amount>UINTPTR_MAX-group) return false;
        address=group+static_cast<std::uintptr_t>(amount);
    } else {
        const auto amount=static_cast<std::uint64_t>(-offset);
        if(amount>group) return false;
        address=group-static_cast<std::uintptr_t>(amount);
    }
    return address>=0x10000 && address<=UINTPTR_MAX-0x339;
}
// Original 557470 returns a group handle plus a signed component offset. The
// component's own self handle is the lease retained across a spectator switch.
template<class Read>
[[nodiscard]] bool health_from_reference(Read& read,native::Ref reference,
                                         std::uint32_t player,HealthView& out) noexcept {
    out={};
    if(reference.handle==UINT32_MAX || reference.kind!=0x80804BEEU
        || player==UINT32_MAX) return false;
    std::uintptr_t group{},health{};
    if(!read.resolve(reference.handle,group)
        || !component_address(group,reference.offset,health)) return false;
    native::Ref definition{};std::uint32_t self{},owner{};std::uint8_t flags{};
    std::uintptr_t resolvedSelf{};
    if(!read.value(health,definition) || definition.kind!=0x80804B8AU
        || !read.value(health+0x24,self) || self==UINT32_MAX
        || !read.resolve(self,resolvedSelf) || resolvedSelf!=health
        || !read.value(health+0x2C,owner) || owner!=player
        || !read.value(health+0x338,flags)) return false;
    std::uintptr_t finalGroup{},finalHealth{},finalSelf{};native::Ref finalDefinition{};
    std::uint32_t finalSelfHandle{},finalOwner{};std::uint8_t finalFlags{};
    if(!read.resolve(reference.handle,finalGroup) || finalGroup!=group
        || !component_address(finalGroup,reference.offset,finalHealth) || finalHealth!=health
        || !read.value(finalHealth,finalDefinition) || finalDefinition.handle!=definition.handle
        || finalDefinition.kind!=definition.kind || finalDefinition.offset!=definition.offset
        || !read.value(finalHealth+0x24,finalSelfHandle) || finalSelfHandle!=self
        || !read.resolve(finalSelfHandle,finalSelf) || finalSelf!=health
        || !read.value(finalHealth+0x2C,finalOwner) || finalOwner!=owner
        || !read.value(finalHealth+0x338,finalFlags) || finalFlags!=flags) return false;
    out={health,self,(flags&1U)!=0};return true;
}
// Use the same native entity/interface lookup as the Ghost binding. The health
// flag is shared with the qualified enemy death observer; no actor count,
// position disappearance or streaming retirement is interpreted as death.
inline bool capture(std::uintptr_t image, Observation& out) noexcept {
    native::Read read{image};
    constexpr std::array<unsigned char,16> playerPrefix{0x40,0x53,0x48,0x83,0xEC,0x20,0x48,0x8B,0xD9,0xC7,0x01,0xFF,0xFF,0xFF,0xFF,0x48};
    constexpr std::array<unsigned char,16> queryPrefix{0x48,0x89,0x5C,0x24,0x10,0x57,0x48,0x81,0xEC,0x60,0x0C,0x00,0x00,0x48,0x8B,0x05};
    constexpr std::array<unsigned char,16> weakPrefix{0x4C,0x8B,0xD1,0x83,0xFA,0xFF,0x74,0x5B,0x4C,0x8B,0x0D,0xD1,0x7F,0x0E,0x02,0x44};
    std::array<unsigned char,16> first{}, second{}, third{};
    if (!read.value(image+0x4B2260, first) || first!=playerPrefix
        || !read.value(image+0x557470, second) || second!=queryPrefix
        || !read.value(image+0x351C90,third) || third!=weakPrefix) return false;
    const auto local=reinterpret_cast<void(__fastcall*)(std::uint32_t*)>(image+0x4B2260);
    const auto query=reinterpret_cast<bool(__fastcall*)(std::uintptr_t,std::uint32_t,void*,std::uint32_t)>(image+0x557470);
    std::uint32_t player{UINT32_MAX}, again{UINT32_MAX}, finalPlayer{UINT32_MAX};
    std::uint32_t stride{}, actual{}, flags{}, self{}, owner{};
    std::uintptr_t table{}, health{};
    __try {
        local(&player);
        if (player==UINT32_MAX || !read.value(image+0x1F93428,table) || !read.value(image+0x1F93430,stride)
            || stride<0x50 || stride>0x100000) return false;
        const auto row=table+static_cast<std::uintptr_t>(player&0x1FFFU)*stride;
        if (!read.value(row+12,actual) || actual!=player || !read.value(row+4,flags) || (flags&4U)) return false;
        alignas(16) std::array<std::byte,0x40> result{};
        if (!query(row,0x80804BEEU,result.data(),0)) return false;
        const auto reference=native::at<native::Ref>(result.data()+0x18);
        HealthView view{};
        if (!health_from_reference(read,reference,player,view)) return false;
        health=view.address;self=view.self;
        if (!read.value(health+0x2C,owner) || owner!=player
            || !read.value(row+12,actual) || actual!=player
            || !read.value(row+4,flags) || (flags&4U)) return false;
        local(&again);
        if (again!=player) return false;
        native::Weak weak{};
        reinterpret_cast<void(__fastcall*)(native::Weak*,std::uint32_t)>(image+0x351C90)(&weak,self);
        HealthView finalView{};
        if (weak.handle!=self || !read.weak(weak)
            || !health_from_reference(read,reference,player,finalView)
            || finalView.address!=view.address || finalView.self!=view.self
            || !read.value(row+12,actual) || actual!=player
            || !read.value(row+4,flags) || (flags&4U)) return false;
        local(&finalPlayer);
        if(finalPlayer!=player || !read.weak(weak)) return false;
        out={player,finalView.dead,weak}; return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
inline bool retained(std::uintptr_t image, const Observation& previous, bool& dead) noexcept {
    native::Read read{image}; std::uintptr_t health{}; native::Ref definition{};
    std::uint32_t self{},owner{}; std::uint8_t flags{};
    if (!read.weak(previous.health) || !read.resolve(previous.health.handle,health)
        || !read.value(health,definition) || definition.kind!=0x80804B8AU
        || !read.value(health+0x24,self) || self!=previous.health.handle
        || !read.value(health+0x2C,owner) || owner!=previous.entity
        || !read.value(health+0x338,flags) || !read.weak(previous.health)) return false;
    dead=(flags&1U)!=0; return true;
}
inline void poll(std::uint64_t session,std::uint64_t run) noexcept {
    if (!state::activity::nightfall::active()) return;
    // Game-frame owner only. Keep a salted health lease when native control
    // switches to the spectator Ghost; retirement itself still proves nothing.
    static Cursor cursor{};
    cursor.bind(run,session);
    const auto before=state::activity::nightfall::progress();
    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const auto now=GetTickCount64();
    bool dead{};
    if (cursor.liveQualified && retained(image,cursor.previous,dead) && dead)
        state::activity::nightfall::observe_local_player(session,run,true,now);
    Observation observed{};
    if (capture(image,observed)) {
        cursor.previous=observed;
        if(cursor.accepts(observed.dead))
            state::activity::nightfall::observe_local_player(session,run,observed.dead,now);
    }
    const auto after=state::activity::nightfall::progress();
    if (before.playerObserved!=after.playerObserved || before.deaths!=after.deaths) {
        std::array<char,240> line{};
        const auto n=std::snprintf(line.data(),line.size(),"ev=nightfall_player run=%llu entity=%08X observed=%u deaths=%u revives=%u outcome=%u evidence=native_health",
            static_cast<unsigned long long>(run),cursor.previous.entity,static_cast<unsigned>(after.playerObserved),after.deaths,
            static_cast<unsigned>(after.revives),static_cast<unsigned>(after.outcome));
        if(n>0 && static_cast<std::size_t>(n)<line.size())
            core::log::write(core::log::Channel::client,core::log::Level::info,{line.data(),static_cast<std::size_t>(n)});
    }
}
}
