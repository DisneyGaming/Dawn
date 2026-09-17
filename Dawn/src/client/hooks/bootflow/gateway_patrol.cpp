#include "gateway_patrol.h"
#include "gateway_patrol_native.h"
#include "gateway_native_read.h"
#include "../../hooking/call_gate.h"
#include "../../hooking/detour.h"
#include "../../../state/activity/gateway/runtime.h"
#include "../../../core/logging/log.h"
#include <atomic>
#include <mutex>
#include <cstdio>

namespace dawn::client::hooks::bootflow::gateway_patrol {
namespace {
namespace g=state::activity::gateway;
using BuildGoal=bool(__fastcall*)(void*,void*,void*,void*) noexcept;
using Suppressed=bool(__fastcall*)(std::uint32_t) noexcept;
hooking::CallGate gate;
std::array<hooking::detour::Handle,3> hooks{};
std::array<std::atomic<Suppressed>,2> sensoryOriginal{};
std::atomic<BuildGoal> original{};
std::uintptr_t image{};
std::mutex mutex;
g::patrol::Loop loop;
std::array<g::EnemyReceipt,g::patrol::kActors> logged{};

bool ignores_combat(std::uint32_t handle) noexcept {
    const auto owner=g::marcher(handle);
    if(!owner.valid()) return false;
    gateway_native::Read read{image};std::uintptr_t table{};std::uint32_t stride{};
    if(!read.value(image+0x1F9D7F8,table) || !read.value(image+0x1F9D800,stride)
        || stride<0x70 || stride>0x100000) return false;
    return owns(read,image,table+std::uintptr_t(handle&0x1FFFU)*stride,owner);
}
// These are the native controlFlag predicates, not global AI switches.
// A81ED0 suppresses visual/attack evaluation; A82070 rejects the sensory
// event dispatcher A060E0 before any of its six stimulus handlers run.
// Returning true for the admitted shield formations also blocks shot reactions.
// Query-time overrides do not leave flags behind when a slot is recycled.
template<std::size_t Index>
__declspec(noinline) bool __fastcall sensory_suppressed(std::uint32_t handle) noexcept {
    hooking::CallGate::Scope scope(gate);
    const auto fn=hooking::await_original(sensoryOriginal[Index]);
    return (scope.accepts_side_effects() && ignores_combat(handle)) || fn(handle);
}

bool redirect(std::uintptr_t actor,std::uintptr_t command) noexcept {
    gateway_native::Read read{image};std::uint32_t handle{};
    if(!read.value(actor+0x48,handle)) return false;
    const auto owner=g::marcher(handle);
    if(!owns(read,image,actor,owner)) return false;
    alignas(16) std::array<float,4> actual{},goal{};
    if(!read.value(command+0x10,goal)) return false;
    reinterpret_cast<void(__fastcall*)(std::uint32_t,float*)>(image+0xA05D00)(handle,actual.data());
    // Native A8FE10 constructs the common locomotion command. Normal walking
    // takes its A8FBD0/A8F1C0/A8F960/A8FA70 branches, bypassing A08660 entirely.
    // Retain the actor, its locomotion speed and shield selection. Only the
    // destination and the native navigation cache are replaced.
    const std::lock_guard lock(mutex);
    auto next=loop;
    const auto target=next.update(owner,{actual[0],actual[1],actual[2]},
        {goal[0],goal[1],goal[2]});
    if(!target.valid) return false;
    alignas(16) const std::array<float,4> destination{target.position.x,target.position.y,target.position.z,1.F};
    const auto zone=reinterpret_cast<std::int32_t(__fastcall*)(std::uint32_t)>(image+0xA13300)(handle);
    std::array<std::uint32_t,2> location{};
    // Same unrestricted fallback used by native A0EEA0. Failed navigation
    // leaves both the original command and patrol direction untouched.
    if(zone==-1 || !reinterpret_cast<bool(__fastcall*)(std::int32_t,std::int32_t,const float*,void*,void*)>
        (image+0xB13490)(-1,zone,destination.data(),location.data(),nullptr)) return false;
    gateway_native::Read again{image};
    if(g::marcher(handle)!=owner || !owns(again,image,actor,owner)) return false;
    std::memcpy(reinterpret_cast<void*>(command+0x10),destination.data(),sizeof(destination));
    reinterpret_cast<void(__fastcall*)(void*,const void*)>(image+0xADC690)(reinterpret_cast<void*>(command),location.data());
    loop=next;
    const auto index=g::patrol::index(owner.registry,owner.source);
    if(logged[index]!=owner || target.turned) {
        logged[index]=owner;
        std::array<char,320> line{};
        const auto n=std::snprintf(line.data(),line.size(),
            "ev=gateway stage=patrol_%s run=%llu source=%u actor=%08X turns=%llu target=%.3f,%.3f,%.3f movement=native",
            target.turned?"reversed":"bound",static_cast<unsigned long long>(owner.run),owner.source,owner.actor,
            static_cast<unsigned long long>(target.turns),destination[0],destination[1],destination[2]);
        if(n>0 && static_cast<std::size_t>(n)<line.size()) core::log::write(core::log::Channel::client,
            core::log::Level::info,{line.data(),static_cast<std::size_t>(n)});
    }
    return true;
}
__declspec(noinline) bool __fastcall build_goal(void* context,void* state,void* request,void* command) noexcept {
    hooking::CallGate::Scope scope(gate);
    const auto fn=hooking::await_original(original);
    const bool changed=fn(context,state,request,command);
    if(!scope.accepts_side_effects()) return changed;
    // A8FE10 may return false without initializing its output. Continue from
    // its retained command at context+18 in that case; never read stack garbage.
    gateway_native::Read read{image};
    return publish_point(read,reinterpret_cast<std::uintptr_t>(context),command,changed,&redirect);
}
bool idle() noexcept { return gate.idle(); }
struct Entry { std::uintptr_t rva;std::array<unsigned char,12> prefix; };
// Filled from the pinned unpacked client, independently checked by the native fixture.
constexpr Entry entries[]{
    {0xA8FE10,{0x40,0x55,0x53,0x56,0x57,0x41,0x54,0x41,0x56,0x41,0x57,0x48}},
    {0xA05D00,{0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x40,0x83,0xc8}},
    {0xA13300,{0x81,0xe1,0xff,0x1f,0x00,0x00,0x0f,0xaf,0x0d,0x93,0xa5,0x58}},
    {0xB13490,{0x48,0x83,0xec,0x58,0x0f,0x28,0x05,0xd5,0xcd,0x0e,0x01,0x41}},
    {0xADC690,{0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0x02,0x48,0x8d,0x99}},
    {0xA81ED0,{0x81,0xe1,0xff,0x1f,0x00,0x00,0x0f,0xaf,0x0d,0x23,0xb9,0x51}},
    {0xA82070,{0x81,0xe1,0xff,0x1f,0x00,0x00,0x0f,0xaf,0x0d,0x83,0xb7,0x51}},
};
}
bool install() noexcept {
    if(original.load(std::memory_order_acquire)) return gate.accepting();
    gate.quiesce();image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    gateway_native::Read read{image};
    for(const auto& entry:entries) {
        std::array<unsigned char,12> bytes{};
        if(!read.value(image+entry.rva,bytes) || bytes!=entry.prefix) return false;
    }
    const std::array specs{
        hooking::detour::Spec{reinterpret_cast<void*>(image+0xA8FE10),reinterpret_cast<void*>(&build_goal)},
        hooking::detour::Spec{reinterpret_cast<void*>(image+0xA81ED0),reinterpret_cast<void*>(&sensory_suppressed<0>)},
        hooking::detour::Spec{reinterpret_cast<void*>(image+0xA82070),reinterpret_cast<void*>(&sensory_suppressed<1>)}};
    if(!hooking::detour::install(specs,hooks)) return false;
    hooking::publish_original(original,reinterpret_cast<BuildGoal>(hooks[0].original));
    for(std::size_t i=0;i<sensoryOriginal.size();++i)
        hooking::publish_original(sensoryOriginal[i],reinterpret_cast<Suppressed>(hooks[i+1].original));
    gate.accept();
    core::log::write(core::log::Channel::client,core::log::Level::info,"ev=gateway stage=patrol_install result=ok movement=native_point_loop boundary=A8FE10 combat=ignored scope=shielded_marchers");
    return true;
}
void quiesce() noexcept { gate.quiesce(); }
bool uninstall() noexcept {
    gate.quiesce();if(!original.load(std::memory_order_acquire)) return true;
    const std::array protect{
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&build_goal)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&redirect)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&ignores_combat)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&sensory_suppressed<0>)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&sensory_suppressed<1>)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&hooking::call_gate_detail::enter)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&hooking::call_gate_detail::leave)}};
    if(hooking::detour::uninstall(hooks,protect,&idle)!=hooking::detour::UninstallResult::removed) return false;
    original.store(nullptr,std::memory_order_release);
    for(auto& fn:sensoryOriginal) fn.store(nullptr,std::memory_order_release);
    const std::lock_guard lock(mutex);loop.reset();logged={};image=0;return true;
}
}
