#pragma once
#include "gateway_native_read.h"
#include "coo_native_components.h"
#include "../../../state/activity/runtime.h"
#include "../../../server/runtime/activity/native_activity_runtime.h"

namespace dawn::client::hooks::bootflow::native_round_player_probe {
// Ported from 1AU-UnEx's native player-life observation, on the existing world-thread poll.
// No health writes, spawn hook, or fabricated death receipt.
inline void poll() noexcept {
    namespace runtime=server::runtime::activity::native_activity;
    static state::activity::ActivityInstanceKey previous{};
    static std::uint32_t cached{UINT32_MAX};static std::uint64_t next{};
    const auto now=GetTickCount64();if(now<next)return;next=now+100;
    const auto owner=state::activity::newest_joined_activity();
    if(owner!=previous) {previous=owner;cached=UINT32_MAX;}
    auto phase=server::runtime::activity::timed_round::Phase::entry;
    std::uint64_t branches{},boot{};
    if(!runtime::round_progress(owner,phase,branches,boot)) {cached=UINT32_MAX;return;}
    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    gateway_native::Read read{image};
    constexpr std::array<std::uint8_t,16> prefix{0x40,0x53,0x48,0x83,0xEC,0x20,0x48,0x8B,0xD9,0xC7,0x01,0xFF,0xFF,0xFF,0xFF,0x48};
    std::array<std::uint8_t,16> actual{};
    if(!read.value(image+0x4B2260,actual) || actual!=prefix)return;
    std::uint32_t controlled{UINT32_MAX};
    reinterpret_cast<void(*)(std::uint32_t*)>(image+0x4B2260)(&controlled);
    auto sample=[&](std::uint32_t entity,bool& alive) noexcept {
        if(entity==UINT32_MAX)return false;
        gateway_native::Read native{image};
        std::uintptr_t table{},health{};std::uint32_t stride{},self{},flags{},bundle{},again{};
        std::uint8_t dead{};
        if(!native.value(image+0x1F93428,table) || table<0x10000
            || !native.value(image+0x1F93430,stride) || stride<0x50 || stride>0x1000)return false;
        const auto row=table+static_cast<std::uintptr_t>(entity&0x1FFFU)*stride;
        if(!native.value(row+0xC,self) || self!=entity || !native.value(row+4,flags) || (flags&4U)
            || !native.value(row+0x4C,bundle)
            || !coo_native::component<gateway_native::Read,1024>(native,bundle,entity,0x80804B8AU,health)
            || !native.value(health+0x338,dead) || !native.value(row+0x4C,again) || again!=bundle
            || !native.value(row+0xC,self) || self!=entity)return false;
        alive=(dead&1U)==0;return true;
    };
    bool alive{};
    if(sample(controlled,alive))cached=controlled;
    else if(!sample(cached,alive))return;
    if(state::activity::newest_joined_activity()==owner)
        runtime::observe_player_life(owner,boot,cached,alive);
}
}
