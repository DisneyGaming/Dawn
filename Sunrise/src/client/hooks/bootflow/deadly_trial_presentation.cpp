#include <Windows.h>
#include "deadly_trial_presentation.h"
#include "gateway_native_read.h"
#include "omega_directive_native.h"
#include "../../../state/activity/deadly_trial/runtime.h"
#include "../../../core/logging/log.h"
#include <cstdio>
namespace sunrise::client::hooks::bootflow::deadly_trial_presentation {
namespace {
using Content=void*(__fastcall*)(void*) noexcept;
using Retire=void(__fastcall*)(void*,std::uint32_t,std::uint32_t,std::int32_t) noexcept;
using Viewer=void(__fastcall*)(std::uint32_t*) noexcept;
using Ready=bool(__fastcall*)() noexcept;
struct Functions { Content content{};Retire retire{};Viewer viewer{};Ready ready{}; };
Functions functions() noexcept;
bool owns(std::byte* component,std::span<const std::byte> bytes) noexcept {
    gateway_native::Read memory{reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr))};
    std::uintptr_t resolved{};
    return memory.resolve(read<std::uint32_t>(bytes,0x48),resolved) && resolved==reinterpret_cast<std::uintptr_t>(component);
}
bool copy(const void* p,std::span<std::byte> b) noexcept {
    SIZE_T n{};return ReadProcessMemory(GetCurrentProcess(),p,b.data(),b.size(),&n) && n==b.size();
}
std::uint32_t identity(std::span<const std::byte> b) noexcept {
    auto value=read<std::uint32_t>(b,0x190);
    for(unsigned i=0;i<4;++i) { value=value*0x01000193U^read<std::uint8_t>(b,0x194+i); }
    return value;
}
}
void update_dialogue(std::byte* component) noexcept {
    if(!component) { return; }
    std::array<std::byte,0x188+11*32> b{};
    if(!copy(component,b) || !source(b,true) || !owns(component,b)) { return; }
    const auto state=trial::presentation();if(!state.run || !dialogue_records(b,state.frame)) { return; }
    for(std::size_t row=0;row<11;++row) {
        if(state.frame.generations[row]) { std::memcpy(component+0x188+row*32,b.data()+0x188+row*32,29); }
    }
}
void update_directive(void* instance,Build build,Register publish) noexcept {
    static thread_local bool updating{};
    if(updating || !instance || !build || !publish) { return; }
    auto* component=static_cast<std::byte*>(instance);
    std::array<std::byte,0xB10> b{};
    if(!copy(component,b) || !source(b,false) || !owns(component,b)) { return; }
    const auto state=trial::presentation();const auto& f=state.frame;
    if(!state.run || !f.enabled || !f.presentation.published) { return; }
    const auto native=functions();if(!native.content || !native.ready() || !omega_directive_native::available()) { return; }
    auto* content=native.content(component);if(!content) { return; }
    struct Guard { bool& v;Guard(bool& value):v(value) { v=true; }~Guard() { v=false; } } guard(updating);
    const auto nav=trial::navigation::goal(f.presentation.event);
    const bool active=f.presentation.active;
    const auto oldEvent=read<std::uint32_t>(b,0x190);
    const bool replace=oldEvent!=f.presentation.event || (read<std::int8_t>(b,0x198)==-1 && active);
    const bool retiring=read<std::int8_t>(b,0x198)!=-1 && (!active || replace);
    std::uint32_t viewer=UINT32_MAX;native.viewer(&viewer);if(viewer==UINT32_MAX) { return; }
    if(retiring) { native.retire(component,identity(b),4,-1); }
    put(b,0x190,f.presentation.event);put<std::uint32_t>(b,0x194,0);put<std::int8_t>(b,0x198,active?0:-1);
    put<std::uint8_t>(b,0x1F4,2);
    if(nav.target.valid()) {
        put(b,0x1F8,nav.target.asset.registry);put<std::uint8_t>(b,0x1FC,47);put<std::uint16_t>(b,0x1FE,nav.target.asset.slot);
        put(b,0x208,nav.target.locator);
    }
    const bool markerChanged=std::memcmp(component+0x1F4,b.data()+0x1F4,0x24)!=0;
    std::memcpy(component+0x190,b.data()+0x190,9);
    std::memcpy(component+0x1F4,b.data()+0x1F4,0x24);
    if(replace && active && !omega_directive_native::install_checkpoint(component,component+0x190,content)) { component[0x198]=std::byte{0xFF};return; }
    if(replace || retiring || markerChanged) {
        build(component,viewer);
        std::array<char,192> line{};const auto size=std::snprintf(line.data(),line.size(),
            "ev=deadly_trial stage=native_objective run=%llu event=%08X active=%u marker=47/%u",
            static_cast<unsigned long long>(state.run),f.presentation.event,active?1U:0U,nav.target.asset.slot);
        if(size>0 && static_cast<std::size_t>(size)<line.size()) { core::log::write(core::log::Channel::client,core::log::Level::info,{line.data(),static_cast<std::size_t>(size)}); }
    }
    if(!copy(component,b) || !route_point(b,f)) { return; }
    std::memcpy(component+0x484,b.data()+0x484,1);std::memcpy(component+0x48C,b.data()+0x48C,1);
    std::memcpy(component+0x498,b.data()+0x498,4);std::memcpy(component+0x4A0,b.data()+0x4A0,16);
    gateway_native::Ref reference{read<std::uint32_t>(b,0x48),0x80804F55U,0x480};publish(&reference);
}
namespace {
Functions functions() noexcept {
    static const Functions result=[]() noexcept {
        Functions f{};const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        std::array<std::byte,16> actual{};
        constexpr std::array contentPrefix{std::byte{0x44},std::byte{0x8B},std::byte{0x09},std::byte{0x4C},std::byte{0x8B},std::byte{0xD1},std::byte{0x48},std::byte{0x8B},std::byte{0x05},std::byte{0x33},std::byte{0x08},std::byte{0x43},std::byte{0x01},std::byte{0x41},std::byte{0x8B},std::byte{0xD1}};
        if(!copy(reinterpret_cast<void*>(image+0x1009430),actual) || actual!=contentPrefix) { return Functions{}; }
        f.content=reinterpret_cast<Content>(image+0x1009430);
        constexpr std::array retirePrefix{std::byte{0x48},std::byte{0x89},std::byte{0x5C},std::byte{0x24},std::byte{0x08},std::byte{0x48},std::byte{0x89},std::byte{0x74},std::byte{0x24},std::byte{0x10},std::byte{0x57},std::byte{0x48},std::byte{0x83},std::byte{0xEC},std::byte{0x20},std::byte{0x41}};
        if(!copy(reinterpret_cast<void*>(image+0x1009E20),actual) || actual!=retirePrefix) { return Functions{}; }
        f.retire=reinterpret_cast<Retire>(image+0x1009E20);
        constexpr std::array viewerPrefix{std::byte{0x40},std::byte{0x53},std::byte{0x48},std::byte{0x83},std::byte{0xEC},std::byte{0x20},std::byte{0x48},std::byte{0x8B},std::byte{0xD9},std::byte{0xC7},std::byte{0x01},std::byte{0xFF},std::byte{0xFF},std::byte{0xFF},std::byte{0xFF},std::byte{0xE8}};
        if(!copy(reinterpret_cast<void*>(image+0x4FFBB0),actual) || actual!=viewerPrefix) { return Functions{}; }
        f.viewer=reinterpret_cast<Viewer>(image+0x4FFBB0);
        constexpr std::array readyPrefix{std::byte{0x80},std::byte{0x3D},std::byte{0xF1},std::byte{0x9A},std::byte{0xC3},std::byte{0x01},std::byte{0x00},std::byte{0x74},std::byte{0x12},std::byte{0x48},std::byte{0x8D},std::byte{0x05},std::byte{0x5F},std::byte{0x86},std::byte{0xC3},std::byte{0x01}};
        if(!copy(reinterpret_cast<void*>(image+0x137E1D0),actual) || actual!=readyPrefix) { return Functions{}; }
        f.ready=reinterpret_cast<Ready>(image+0x137E1D0);
        return f;
    }();return result;
}
}
}
