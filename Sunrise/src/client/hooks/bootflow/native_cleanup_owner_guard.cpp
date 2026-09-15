#include <Windows.h>
#include <array>
#include <cstdint>
#include <cstring>
#include "../../hooking/call_gate.h"
#include "../../hooking/detour.h"
#include "../../../core/logging/log.h"
#include "internal.h"
#include "native_hook_ownership.h"

extern "C" {
std::uintptr_t cleanup_owner_cookie{};
std::uintptr_t cleanup_owner_resolver{};
std::uintptr_t cleanup_owner_resume{};
std::uintptr_t cleanup_owner_tail{};
alignas(8) volatile LONG64 cleanup_owner_misses{};
void native_cleanup_owner_body(void* component) noexcept;
}

namespace sunrise::client::hooks::bootflow {
namespace {
hooking::CallGate gate;
hooking::detour::Handle handle;
// Two independent dumps: null resolver return -> read [RAX+10EB0] at F9C189.
// F9C190..F9C27E reassigns siblings through that owner. F9C27F begins the
// component's own cleanup, including its four resource releases and final unlink.
constexpr std::array<unsigned char,64> prefix{
    0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x55,0x57,0x41,0x54,0x41,0x56,
    0x41,0x57,0x48,0x8b,0xec,0x48,0x83,0xec,0x70,0x48,0x8b,0x05,0x18,0xd9,0x10,0x01,
    0x48,0x33,0xc4,0x48,0x89,0x45,0xf0,0x4c,0x8b,0xf9,0xe8,0x71,0x0a,0x55,0xff,0x33,
    0xff,0x48,0x8d,0x4d,0xc0,0x33,0xd2,0x8b,0xdf,0x48,0x8b,0xb0,0xb0,0x0e,0x01,0x00};
constexpr std::array<unsigned char,13> tail{
    0x41,0x8b,0x97,0xe0,0x01,0x00,0x00,0x83,0xfa,0xff,0x74,0x4d,0x8b};
bool matches(std::uintptr_t address,const void* bytes,std::size_t size) noexcept {
    __try {return std::memcmp(reinterpret_cast<const void*>(address),bytes,size)==0;}
    __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
__declspec(noinline) void __fastcall cleanup(void* component) noexcept {
    const hooking::CallGate::Scope call{gate};
    const auto before=InterlockedCompareExchange64(&cleanup_owner_misses,0,0);
    // Forward cleanup even after quiesce. No native pointer is substituted and
    // no exception is swallowed. Valid owners execute the original sibling path.
    native_cleanup_owner_body(component);
    const auto after=InterlockedCompareExchange64(&cleanup_owner_misses,0,0);
    if(after!=before && after<=64 && call.accepts_side_effects())
        core::log::write(core::log::Channel::client,core::log::Level::info,
            "ev=native_cleanup_owner_guard result=missing_owner sibling_reassignment=skipped native_cleanup=completed");
}
bool idle() noexcept {return gate.idle();}
}
bool install_native_cleanup_owner_guard() noexcept {
    if(handle.attached)return gate.accepting();
    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    if(!image || !matches(image+native_hook_ownership::kCleanupOwner[0],prefix.data(),prefix.size())
        || !matches(image+0xF9C27F,tail.data(),tail.size()))return false;
    cleanup_owner_cookie=image+0x20A9A88;
    cleanup_owner_resolver=image+0x4ECBF0;
    cleanup_owner_resume=image+0xF9C190;
    cleanup_owner_tail=image+0xF9C27F;
    if(!hooking::detour::install({reinterpret_cast<void*>(image+native_hook_ownership::kCleanupOwner[0]),
        reinterpret_cast<void*>(&cleanup)},handle))return false;
    gate.accept();
    core::log::write(core::log::Channel::client,core::log::Level::info,
        "ev=native_cleanup_owner_guard result=installed scope=F9C150_missing_owner preserve=native_cleanup_tail");
    return true;
}
void quiesce_native_cleanup_owner_guard() noexcept {gate.quiesce();}
bool uninstall_native_cleanup_owner_guard() noexcept {
    gate.quiesce();
    if(!handle.attached)return true;
    const std::array entries{
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&cleanup)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&native_cleanup_owner_body)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&hooking::call_gate_detail::enter)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&hooking::call_gate_detail::leave)}};
    if(hooking::detour::uninstall(handle,entries,&idle)!=hooking::detour::UninstallResult::removed)return false;
    cleanup_owner_cookie=cleanup_owner_resolver=cleanup_owner_resume=cleanup_owner_tail=0;
    InterlockedExchange64(&cleanup_owner_misses,0);
    return true;
}
}
