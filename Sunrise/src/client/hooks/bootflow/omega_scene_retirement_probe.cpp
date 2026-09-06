#include <Windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "../../../core/logging/log.h"
#include "../../../core/settings/settings.h"
#include "../../hooking/call_gate.h"
#include "../../hooking/detour.h"
#include "internal.h"

namespace sunrise::client::hooks::bootflow {
namespace {

/** Pinned-client terminal scene-transition routine. The hook never changes its arguments/result. */
constexpr std::uintptr_t kSceneTransitionRetireRva = 0x58B9A0U;
constexpr std::array<std::byte, 24> kSceneTransitionRetirePrefix{
    std::byte{0x40}, std::byte{0x57}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x40}, std::byte{0x48}, std::byte{0x8B},
    std::byte{0xF9}, std::byte{0x80}, std::byte{0xFA}, std::byte{0xFF},
    std::byte{0x0F}, std::byte{0x84}, std::byte{0x2F}, std::byte{0x01},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x48}, std::byte{0x89},
    std::byte{0x5C}, std::byte{0x24}, std::byte{0x50}, std::byte{0x33}};

using SceneTransitionRetire = void(__fastcall*)(std::uint32_t* scene,
                                                 std::uint32_t transition,
                                                 char transitionFlag,
                                                 char alreadyProcessed,
                                                 char forceRetire) noexcept;

std::atomic_bool g_installed{};
std::atomic<SceneTransitionRetire> g_original{};
hooking::detour::Handle g_handle{};
hooking::CallGate g_callGate{};

[[nodiscard]] bool calls_idle() noexcept {
    return g_callGate.idle();
}

[[nodiscard]] bool prefix_matches(const std::byte* target) noexcept {
    return target != nullptr
           && std::equal(kSceneTransitionRetirePrefix.begin(),
                         kSceneTransitionRetirePrefix.end(),
                         target);
}

void scene_transition_retire_body(std::uint32_t* scene,
                                  std::uint32_t transition,
                                  char transitionFlag,
                                  char alreadyProcessed,
                                  char forceRetire,
                                  const hooking::CallGate::Scope& call) noexcept {
    const SceneTransitionRetire original = hooking::await_original(g_original);

    // Read the authored identity before the native routine is allowed to retire its storage.
    const std::uint32_t sceneHandle =
        call.accepts_side_effects() && scene != nullptr ? scene[0] : 0xFFFFFFFFU;
    original(scene, transition, transitionFlag, alreadyProcessed, forceRetire);

    if (!call.accepts_side_effects() || (transition & 0xFFU) != 0xFFU) {
        return;
    }
    if (!core::settings::get().omegaExperiments.unsafeDiagnostics) {
        return;
    }
    const std::uint64_t tickMs = GetTickCount64();
    std::array<char, 256> line{};
    const int written = std::snprintf(
        line.data(),
        line.size(),
        "ev=activity stage=omega_scene_retirement t=%llu result=observed scene=0x%08X "
        "transition=0x%08X authority=none mutation=observe_only",
        static_cast<unsigned long long>(tickMs),
        sceneHandle,
        transition);
    if (written > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(written)});
    }
}

__declspec(noinline) void __fastcall scene_transition_retire(std::uint32_t* scene,
                                                             std::uint32_t transition,
                                                             char transitionFlag,
                                                             char alreadyProcessed,
                                                             char forceRetire) noexcept {
    hooking::CallGate::Scope call(g_callGate);
    scene_transition_retire_body(scene,
                                 transition,
                                 transitionFlag,
                                 alreadyProcessed,
                                 forceRetire,
                                 call);
}

} // namespace

bool install_omega_scene_retirement_probe() noexcept {
    if (g_installed.load(std::memory_order_acquire)) {
        return g_callGate.accepting();
    }
    g_callGate.quiesce();
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    std::byte* const target = image == nullptr ? nullptr : image + kSceneTransitionRetireRva;
    if (!prefix_matches(target)) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::warn,
                         "ev=activity stage=omega_scene_retirement_install result=prefix_mismatch");
        return false;
    }
    const hooking::detour::Spec spec{target,
                                     reinterpret_cast<void*>(&scene_transition_retire)};
    if (!hooking::detour::install(spec, g_handle)) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::warn,
                         "ev=activity stage=omega_scene_retirement_install result=attach_fail");
        return false;
    }
    hooking::publish_original(
        g_original, reinterpret_cast<SceneTransitionRetire>(g_handle.original));
    g_installed.store(true, std::memory_order_release);
    g_callGate.accept();
    core::log::write(
        core::log::Channel::client,
        core::log::Level::info,
        "ev=activity stage=omega_scene_retirement_install result=ok target=+58B9A0 "
        "filter=authored_cast_terminal transition_order=native_then_defer mutation=observe_only");
    return true;
}

void quiesce_omega_scene_retirement_probe() noexcept {
    g_callGate.quiesce();
}

bool uninstall_omega_scene_retirement_probe() noexcept {
    quiesce_omega_scene_retirement_probe();
    if (!g_installed.load(std::memory_order_acquire)) {
        return true;
    }

    const std::array protectedEntries{
        hooking::detour::ProtectedCodeEntry{
            reinterpret_cast<void*>(&scene_transition_retire)},
        hooking::detour::ProtectedCodeEntry{
            reinterpret_cast<void*>(&hooking::call_gate_detail::enter)},
        hooking::detour::ProtectedCodeEntry{
            reinterpret_cast<void*>(&hooking::call_gate_detail::leave)},
    };
    const hooking::detour::UninstallResult result =
        hooking::detour::uninstall(g_handle, protectedEntries, &calls_idle);
    if (result != hooking::detour::UninstallResult::removed) {
        core::log::write(
            core::log::Channel::client,
            result == hooking::detour::UninstallResult::failed ? core::log::Level::error
                                                               : core::log::Level::warn,
            result == hooking::detour::UninstallResult::failed
                ? "ev=activity stage=omega_scene_retirement_uninstall result=failed retained=1"
                : "ev=activity stage=omega_scene_retirement_uninstall result=deferred retained=1");
        return false;
    }

    g_original.store(nullptr, std::memory_order_release);
    g_installed.store(false, std::memory_order_release);
    core::log::write(core::log::Channel::client,
                     core::log::Level::info,
                     "ev=activity stage=omega_scene_retirement_uninstall result=ok retained=0");
    return true;
}

} // namespace sunrise::client::hooks::bootflow
