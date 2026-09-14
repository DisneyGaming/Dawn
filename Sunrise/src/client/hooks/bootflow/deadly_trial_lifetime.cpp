#include "deadly_trial_lifetime.h"
#include "deadly_trial_presentation_binding.h"
#include "gateway_native_read.h"
#include "../../hooking/call_gate.h"
#include "../../hooking/detour.h"
#include "../../../state/activity/deadly_trial/runtime.h"
#include "../../../state/activity/hijacked/runtime.h"
#include "../../../core/logging/log.h"
#include <atomic>
#include <cstdio>

namespace sunrise::client::hooks::bootflow::deadly_trial_lifetime {
namespace {
using Decode=bool(__fastcall*)(void*,void*) noexcept;
hooking::CallGate gate;
hooking::detour::Handle hook{};
std::atomic<Decode> original{};
std::uintptr_t image{};
struct Native {
    std::uintptr_t context() const noexcept {
        const auto cell=reinterpret_cast<std::uintptr_t(__fastcall*)()>(image+0x4EA560)();
        std::uintptr_t value{};gateway_native::Read read{image};
        return cell && read.value(cell,value)?value:0;
    }
    bool lookup(const Identity& identity,Ref& ref) const noexcept {
        return reinterpret_cast<bool(__fastcall*)(const Identity*,std::uint32_t,Ref*)>(image+0x4EA140)(&identity,0,&ref);
    }
    bool allocated(std::uintptr_t pool,std::uint16_t index) const noexcept {
        return reinterpret_cast<bool(__fastcall*)(std::uintptr_t,std::uint16_t)>(image+0x34E830)(pool,index);
    }
    bool assign(std::uintptr_t address,std::uint32_t before,std::uint32_t after) const noexcept {
        return static_cast<std::uint32_t>(InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(address),
            static_cast<LONG>(after),static_cast<LONG>(before)))==before;
    }
};
void reconnect(std::uintptr_t roster) noexcept {
    const auto hijackedRun=state::activity::hijacked::native_run();
    const auto run=hijackedRun?hijackedRun:state::activity::deadly_trial::native_run();
    if(!run) { return; }
    const auto expectedScenario=hijackedRun?state::activity::hijacked::kScenario:0x80B2E043U;
    gateway_native::Read read{image};Native native;
    if(repair_presentation(read,native,roster,expectedScenario)==Result::repaired) {
        std::array<char,192> line{};
        const auto n=std::snprintf(line.data(),line.size(),
            "ev=%s stage=presentation_rebound run=%llu boundary=4D7380 authority=native_packet",
            hijackedRun?"hijacked":"deadly_trial",static_cast<unsigned long long>(run));
        if(n>0 && static_cast<std::size_t>(n)<line.size()) {
            core::log::write(core::log::Channel::client,core::log::Level::info,{line.data(),static_cast<std::size_t>(n)});
        }
    }
    if(repair(read,native,roster,expectedScenario)==Result::repaired) {
        std::array<char,192> line{};
        const auto n=std::snprintf(line.data(),line.size(),"ev=%s stage=lifetime_rebound run=%llu boundary=4D7380 authority=native_packet",
            hijackedRun?"hijacked":"deadly_trial",static_cast<unsigned long long>(run));
        if(n>0 && static_cast<std::size_t>(n)<line.size()) {
            core::log::write(core::log::Channel::client,core::log::Level::info,{line.data(),static_cast<std::size_t>(n)});
        }
    }
}
__declspec(noinline) bool __fastcall decode(void* roster,void* stream) noexcept {
    hooking::CallGate::Scope scope(gate);
    const auto fn=hooking::await_original(original);
    // This boundary follows the native phase-one roster update and its ready
    // gate; it does not run from an arbitrary timer or during global teardown.
    if(scope.accepts_side_effects()) { reconnect(reinterpret_cast<std::uintptr_t>(roster)); }
    return fn(roster,stream);
}
bool idle() noexcept { return gate.idle(); }
struct Entry { std::uintptr_t rva;std::array<unsigned char,12> prefix; };
constexpr Entry entries[]{
    {0x4D7380,{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48}},
    {0x4EA560,{0x0f,0xb7,0x0d,0x81,0x7a,0xaa,0x01,0x66,0x85,0xc9,0x0f,0x85}},
    {0x4EA140,{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48}},
    {0x34E830,{0xb8,0xff,0xff,0x00,0x00,0x66,0x3b,0xd0,0x74,0x23,0x48,0x8b}}
};
}
bool install() noexcept {
    if(original.load(std::memory_order_acquire)) { return gate.accepting(); }
    gate.quiesce();image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));gateway_native::Read read{image};
    for(const auto& entry:entries) {
        std::array<unsigned char,12> bytes{};
        if(!read.value(image+entry.rva,bytes) || bytes!=entry.prefix) { return false; }
    }
    if(!hooking::detour::install({reinterpret_cast<void*>(image+0x4D7380),reinterpret_cast<void*>(&decode)},hook)) { return false; }
    hooking::publish_original(original,reinterpret_cast<Decode>(hook.original));gate.accept();
    core::log::write(core::log::Channel::client,core::log::Level::info,"ev=coo_lifetime stage=binding_install result=ok missions=deadly_trial,hijacked");return true;
}
void quiesce() noexcept { gate.quiesce(); }
bool uninstall() noexcept {
    gate.quiesce();if(!original.load(std::memory_order_acquire)) { return true; }
    const std::array entriesToProtect{
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&decode)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&reconnect)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&hooking::call_gate_detail::enter)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&hooking::call_gate_detail::leave)}};
    if(hooking::detour::uninstall(hook,entriesToProtect,&idle)!=hooking::detour::UninstallResult::removed) { return false; }
    original.store(nullptr,std::memory_order_release);image=0;return true;
}
}
