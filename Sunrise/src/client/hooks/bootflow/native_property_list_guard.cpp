#include <Windows.h>
#include <array>
#include <atomic>
#include <cstring>
#include <cstdio>
#include "native_property_list_guard.h"
#include "native_hook_ownership.h"
#include "internal.h"
#include "../../hooking/call_gate.h"
#include "../../hooking/detour.h"
#include "../../../core/logging/log.h"

namespace sunrise::client::hooks::bootflow {
namespace {
namespace policy=native_property_list;
hooking::CallGate gate;
hooking::detour::Handle detour;
using Update=void(__fastcall*)(void*,const policy::Group*) noexcept;
Update update{};
std::atomic_uint logs{};
bool copy(void*,std::uintptr_t address,std::span<std::byte> output) noexcept {
    if(address<0x10000 || address>UINTPTR_MAX-output.size())return false;
    __try {std::memcpy(output.data(),reinterpret_cast<const void*>(address),output.size());return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
void report(const char* result,std::uintptr_t list,std::uint32_t key) noexcept {
    if(logs.fetch_add(1,std::memory_order_relaxed)>=64)return;
    std::array<char,256> line{};
    const auto n=std::snprintf(line.data(),line.size(),
        "ev=native_property_list result=%s list=%p key=%08X actor_mutation=none",
        result,reinterpret_cast<void*>(list),key);
    if(n>0 && static_cast<std::size_t>(n)<line.size())
        core::log::write(core::log::Channel::client,core::log::Level::info,{line.data(),static_cast<std::size_t>(n)});
}
// Called on the native list-update owner, just like original 4D86B0. No lock is
// held over native code. No destructor, spawn, roster or mission state is skipped.
__declspec(noinline) void __fastcall update_list(void* object) noexcept {
    const hooking::CallGate::Scope call{gate};
    const auto list=reinterpret_cast<std::uintptr_t>(object);
    const policy::Source source{0,nullptr,&copy};
    policy::run(source,list,call.accepts_side_effects(),
        [&](std::uintptr_t address) noexcept {update(object,reinterpret_cast<const policy::Group*>(address));},
        [&](std::int32_t i,std::int32_t count) noexcept {
            // Same descriptor compaction as native 4D7D23..4D7D55, without its
            // unlink/free: all this group's slots are already gone. Clear no data
            // outside the active descriptor prefix. Other groups retain order.
            const auto address=list+4+static_cast<std::uintptr_t>(i)*sizeof(policy::Group);
            std::memmove(reinterpret_cast<void*>(address),reinterpret_cast<void*>(address+sizeof(policy::Group)),
                static_cast<std::size_t>(count-i-1)*sizeof(policy::Group));
            *reinterpret_cast<std::int32_t*>(list)=count-1;
            return true;
        },[&](const char* result,std::uint32_t key) noexcept {report(result,list,key);});
}
bool idle() noexcept {return gate.idle();}
constexpr std::array<unsigned char,21> prefix{
    0x40,0x56,0x48,0x83,0xEC,0x20,0x8B,0x01,0x48,0x8B,0xF1,0x85,0xC0,0x7E,0x30,
    0x48,0x89,0x5C,0x24,0x30,0x48};
}
bool install_native_property_list_guard() noexcept {
    if(detour.attached)return gate.accepting();
    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    std::array<std::byte,prefix.size()> actual{};
    if(!copy(nullptr,image+native_hook_ownership::kPropertyList[0],actual)
        || std::memcmp(actual.data(),prefix.data(),prefix.size())!=0)return false;
    update=reinterpret_cast<Update>(image+0x4D8700);
    if(!hooking::detour::install({reinterpret_cast<void*>(image+native_hook_ownership::kPropertyList[0]),
        reinterpret_cast<void*>(&update_list)},detour))return false;
    gate.accept();
    core::log::write(core::log::Channel::client,core::log::Level::info,
        "ev=native_property_list result=installed scope=4D86B0_pool_verified_retired_groups");
    return true;
}
void quiesce_native_property_list_guard() noexcept {gate.quiesce();}
bool uninstall_native_property_list_guard() noexcept {
    gate.quiesce();if(!detour.attached)return true;
    const std::array entries{
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&update_list)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&hooking::call_gate_detail::enter)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&hooking::call_gate_detail::leave)}};
    if(hooking::detour::uninstall(detour,entries,&idle)!=hooking::detour::UninstallResult::removed)return false;
    update=nullptr;logs.store(0);return true;
}
}
