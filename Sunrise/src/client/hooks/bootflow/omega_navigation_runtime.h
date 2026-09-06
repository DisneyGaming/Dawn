#pragma once

#include <Windows.h>
#include <array>
#include <atomic>
#include <cstdio>
#include <string_view>
#include "omega_navigation.h"
#include "omega_forest_recipe.h"
#include "omega_enemy_forest_receipts_runtime.h"
#include "../../hooking/call_gate.h"
#include "../../hooking/detour.h"
#include "../../player/player_position.h"
#include "../../../state/activity/forced/activity_forced_destination.h"
#include "../../../state/activity/runtime.h"
#include "../../../state/activity/omega/omega_progression.h"

namespace sunrise::client::hooks::bootflow::omega_navigation_runtime {
namespace detail {
namespace nav=omega_navigation;
using NextPoint=std::uint32_t(__fastcall*)(std::byte*, const std::byte*, void*) noexcept;
inline hooking::detour::Handle handle;
inline std::atomic<NextPoint> original;
inline hooking::CallGate gate;
inline SRWLOCK lock=SRWLOCK_INIT;
inline nav::Route route;
inline std::atomic_bool installed;
inline std::atomic_uint64_t reportRun{UINT64_MAX};
inline std::atomic_uint32_t lines;
inline constexpr std::array<unsigned char,16> kNextPrefix{0x40,0x53,0x41,0x56,0x48,0x81,0xEC,0x28,0x03,0x00,0x00,0x48,0x8B,0x05,0x26,0xE2};
inline constexpr std::array<unsigned char,16> kRegisterPrefix{0x41,0x56,0x48,0x83,0xEC,0x30,0x44,0x8B,0x01,0x4C,0x8B,0xF1,0x48,0x8B,0x49,0x08};
inline constexpr std::array<unsigned char,16> kSelfPrefix{0x0F,0xB7,0x41,0x20,0x25,0xFF,0x1F,0x00,0x00,0x0F,0xAF,0x05,0xA0,0xC4,0xAA,0x01};
inline constexpr std::array<unsigned char,16> kOwnerPrefix{0x81,0xE1,0xFF,0x1F,0x00,0x00,0x4C,0x8D,0x05,0x03,0xA5,0x2B,0x02,0x8B,0xC1,0x83};
inline bool copy(std::uintptr_t address,void* output,std::size_t bytes) noexcept {
    return forest_enemy::runtime_detail::copy_memory(address,output,bytes);
}
template<class T> inline bool get(std::uintptr_t address,T& output) noexcept { return copy(address,&output,sizeof output); }
inline bool prefix(std::uintptr_t image,std::uintptr_t rva,const std::array<unsigned char,16>& expected) noexcept {
    std::array<unsigned char,16> bytes{};
    return image && copy(image+rva,bytes.data(),bytes.size()) && bytes==expected;
}
inline bool admitted(bool arrived=true) noexcept {
    state::activity::forced::ForcedDestination forced{};
    state::activity::forced::snapshot(forced);
    const auto size=(std::min)(static_cast<std::size_t>(forced.packageNameLength),forced.packageName.size());
    return state::activity::forced::override_active()
        && std::string_view(forced.packageName.data(),size)=="mission_scot"
        && state::activity::mission_seed_armed() && !state::activity::omega_authority_quiesced()
        && (!arrived || state::activity::world_phase()==state::activity::WorldPhase::arrived);
}
template<class... Args> inline void log(std::uint64_t run,const char* format,Args... args) noexcept {
    if (reportRun.exchange(run)!=run) lines.store(0);
    if (lines.fetch_add(1)>=128) return;
    std::array<char,512> text{};
    const int count=std::snprintf(text.data(),text.size(),format,args...);
    if (count>0 && static_cast<std::size_t>(count)<text.size())
        core::log::write(core::log::Channel::client,core::log::Level::info,{text.data(),static_cast<std::size_t>(count)});
}
inline bool worker(std::uintptr_t address) noexcept {
    std::uint32_t kind{},set{};
    return address && get(address+4,kind) && get(address+0x96C,set)
        && omega_forest::matches(true,kind,set);
}
inline nav::Goal sample_route(std::uint64_t run,bool finalGate=false) noexcept {
    const auto position=player::position::snapshot();
    const auto progress=state::activity::omega::presentation_progress(run);
    AcquireSRWLockExclusive(&lock);
    (void)route.observe(run,position.present,{position.position[0],position.position[1],position.position[2]},progress.loadedBubble);
    // Existing encounter progression independently latches the fixed exit crossing.
    if (progress.route>=state::activity::omega::Route::crownEntrance) route.advance(nav::Goal::lairApproach);
    if (finalGate) route.terminal_gate();
    const auto current=route.current();
    ReleaseSRWLockExclusive(&lock);
    return current;
}
inline bool final_gate(std::uintptr_t address,std::uintptr_t node,std::uint32_t result) noexcept {
    std::array<std::byte,0x970> bytes{};
    if (!node || !copy(address,bytes.data(),bytes.size())) return false;
    const auto count=nav::read<std::int32_t>(bytes,0x924);
    const auto nodes=forest_enemy::runtime_detail::relative(address+0x858,nav::read<std::int64_t>(bytes,0x858),0x10);
    if (!nodes || count<=0 || count>4096 || node<nodes || (node-nodes)%0x38
        || (node-nodes)/0x38>=static_cast<unsigned>(count)) return false;
    std::array<std::byte,0x38> entry{};
    if (!copy(node,entry.data(),entry.size())) return false;
    const auto index=nav::read<std::int8_t>(entry,0x24);
    const auto gates=nav::read<std::int32_t>(bytes,0x92C);
    if (index<0 || gates<=0 || gates>256 || index>=gates) return false;
    const auto base=forest_enemy::runtime_detail::relative(address+0x890,nav::read<std::int64_t>(bytes,0x890),0x10);
    std::uint8_t flag{};
    return base && get(base+static_cast<unsigned>(index)*0x360+0x355,flag)
        && nav::terminal(result,nav::read<std::uint8_t>(entry,0x18),index,gates,
            nav::read<float>(entry,0x2C),nav::read<float>(bytes,0x89C),flag);
}
__declspec(noinline) inline std::uint32_t __fastcall next_point(std::byte* instance,const std::byte* node,void* output) noexcept {
    hooking::CallGate::Scope call(gate);
    const auto native=hooking::await_original(original);
    const auto result=native(instance,node,output);
    const auto address=reinterpret_cast<std::uintptr_t>(instance);
    if (!call.accepts_side_effects() || !admitted() || !worker(address)) return result;
    const auto run=state::activity::mission_run_generation();
    const bool final=final_gate(address,reinterpret_cast<std::uintptr_t>(node),result);
    const auto goal=sample_route(run,final);
    if (final) log(run,"ev=omega_navigation stage=terminal_gate run=%llu worker=%p goal=%u",run,instance,static_cast<unsigned>(goal));
    // Native caller 1007C80 removes exactly this route on result zero.
    return call.accepts_side_effects() && goal>=nav::Goal::lairApproach ? 0U : result;
}
inline bool idle() noexcept { return gate.idle(); }
struct Reference { std::uint32_t handle{UINT32_MAX},kind{0x80804F55U}; std::int64_t offset{}; };
inline bool fallback_reference(std::byte* instance,std::uintptr_t image,Reference& output) noexcept {
    if (!prefix(image,0x4E5C60,kSelfPrefix)) return false;
    using Self=std::uint32_t*(__fastcall*)(std::byte*,std::uint32_t*) noexcept;
    __try { reinterpret_cast<Self>(image+0x4E5C60)(instance,&output.handle); }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
    forest_enemy::runtime_detail::Reader read{};
    const auto datum=forest_enemy::runtime_detail::resolve(read,image,output.handle);
    const auto address=reinterpret_cast<std::uintptr_t>(instance);
    if (!datum || address<datum || address-datum>0x100000) return false;
    output.offset=static_cast<std::int64_t>(address-datum+nav::kFallback);
    return true;
}
inline bool apply_fallback(std::byte* component,const std::byte* snapshot,bool present,
                            std::uintptr_t image,const Reference& reference) noexcept {
    using Register=void(__fastcall*)(const Reference*) noexcept;
    __try {
        component[nav::kFallback+4]=snapshot[nav::kFallback+4];
        if (present) {
            component[nav::kFallback+12]=snapshot[nav::kFallback+12];
            std::memcpy(component+nav::kFallback+24,snapshot+nav::kFallback+24,4);
            std::memcpy(component+nav::kFallback+32,snapshot+nav::kFallback+32,sizeof(nav::Point));
        }
        reinterpret_cast<Register>(image+0xC763D0)(&reference);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
inline bool apply_owner(std::uintptr_t image,std::uint16_t owner) noexcept {
    using Setter=void(__fastcall*)(std::uint16_t,bool) noexcept;
    __try { reinterpret_cast<Setter>(image+0x403BD0)(owner,true); return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
} // namespace detail

/** Existing type-68 tick calls this after original completion; pointers are not retained. */
__declspec(noinline) inline void after_directive_tick(std::byte* component) noexcept {
    using namespace detail;
    hooking::CallGate::Scope call(gate);
    if (!call.accepts_side_effects() || !admitted()) return;
    std::array<std::byte,nav::kComponentBytes> snapshot{};
    if (!copy(reinterpret_cast<std::uintptr_t>(component),snapshot.data(),snapshot.size()) || !nav::component(snapshot)) return;
    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const auto run=state::activity::mission_run_generation();
    const auto goal=sample_route(run);
    if (!prefix(image,0xC763D0,kRegisterPrefix)) return;
    Reference reference{};
    if (!fallback_reference(component,image,reference) || !nav::sync(snapshot,goal)) return;
    if (!call.accepts_side_effects() || state::activity::mission_run_generation()!=run) return;
    const auto desired=nav::target(goal);
    if (!apply_fallback(component,snapshot.data(),desired.present,image,reference)) return;
    log(run,"ev=omega_navigation stage=directive run=%llu goal=%u target_bubble=%u owner=%08X offset=%llX",run,static_cast<unsigned>(goal),desired.bubble,reference.handle,reference.offset);
}

/** Exact Forest worker owner bit is separate from the replicated sensor's authority. */
__declspec(noinline) inline void repair_forest_owner(void* instance) noexcept {
    using namespace detail;
    hooking::CallGate::Scope call(gate);
    if (!call.accepts_side_effects() || !admitted(false)) return;
    const auto run=state::activity::mission_run_generation();
    const auto address=reinterpret_cast<std::uintptr_t>(instance);
    if (!worker(address)) return;
    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    std::uint16_t owner{UINT16_MAX}; std::uint32_t word{};
    if (!get(address+0x2C,owner) || owner==UINT16_MAX
        || !get(image+0x26BE0E0+((owner&0x1FFFU)>>5)*4,word)
        || ((word>>(owner&31U))&1U) || !prefix(image,0x403BD0,kOwnerPrefix)) return;
    if (!call.accepts_side_effects() || state::activity::mission_run_generation()!=run
        || !apply_owner(image,owner)) return;
    log(run,"ev=forest stage=omega_owner_authority run=%llu worker=%p owner=%04X",run,instance,static_cast<unsigned>(owner));
}

inline bool install() noexcept {
    using namespace detail;
    if (installed.load()) return gate.accepting();
    gate.quiesce();
    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    if (!prefix(image,0xFFB850,kNextPrefix) || !prefix(image,0xC763D0,kRegisterPrefix)
        || !prefix(image,0x4E5C60,kSelfPrefix) || !prefix(image,0x403BD0,kOwnerPrefix)) return false;
    const hooking::detour::Spec spec{reinterpret_cast<void*>(image+0xFFB850),reinterpret_cast<void*>(&next_point)};
    if (!hooking::detour::install(spec,handle)) return false;
    hooking::publish_original(original,reinterpret_cast<NextPoint>(handle.original));
    installed.store(true); gate.accept();
    return true;
}
inline void quiesce() noexcept { detail::gate.quiesce(); }
inline bool uninstall() noexcept {
    using namespace detail;
    quiesce();
    if (!installed.load()) return true;
    const std::array entries{
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&next_point)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&after_directive_tick)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&repair_forest_owner)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&hooking::call_gate_detail::enter)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&hooking::call_gate_detail::leave)}};
    const auto result=hooking::detour::uninstall(std::span{&handle,1},entries,&idle);
    if (result!=hooking::detour::UninstallResult::removed) return false;
    original.store(nullptr); installed.store(false);
    return true;
}
} // namespace sunrise::client::hooks::bootflow::omega_navigation_runtime
