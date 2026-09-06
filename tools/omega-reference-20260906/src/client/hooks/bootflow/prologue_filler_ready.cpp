#include <Windows.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <span>
#include <string_view>

#include "../../../core/logging/log.h"
#include "../../../state/activity/forced/activity_forced_destination.h"
#include "../../hooking/call_gate.h"
#include "../../hooking/detour.h"
#include "internal.h"
#include "prologue_filler_ready_lifecycle.h"
#if defined(SUNRISE_PROLOGUE_FILLER_READY_TESTING)
#include "prologue_filler_ready_test_support.h"
#endif

namespace sunrise::client::hooks::bootflow {
namespace {

/** Runtime-decrypted local prologue-filler readiness accessor in the pinned client. */
constexpr std::uintptr_t kReadyRva = 0xC23EC0U;
constexpr std::array<std::byte, 4> kReadyPrefix{
    std::byte{0x48}, std::byte{0x83}, std::byte{0xEC}, std::byte{0x28}};
constexpr std::array<std::byte, 3> kBetweenCalls{std::byte{0x48}, std::byte{0x8B}, std::byte{0xC8}};
constexpr std::array<std::byte, 13> kReadySuffix{std::byte{0x0F},
                                                 std::byte{0xB6},
                                                 std::byte{0x40},
                                                 std::byte{0x08},
                                                 std::byte{0xD0},
                                                 std::byte{0xE8},
                                                 std::byte{0x24},
                                                 std::byte{0x01},
                                                 std::byte{0x48},
                                                 std::byte{0x83},
                                                 std::byte{0xC4},
                                                 std::byte{0x28},
                                                 std::byte{0xC3}};

/** Task update that converts the prologue completion into a world-controller event. */
constexpr std::uintptr_t kStateUpdateRva = 0xD46DD0U;
constexpr std::array<std::byte, 26> kStateUpdatePrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24}, std::byte{0x08},
    std::byte{0x48}, std::byte{0x89}, std::byte{0x7C}, std::byte{0x24}, std::byte{0x10},
    std::byte{0x55}, std::byte{0x48}, std::byte{0x8D}, std::byte{0xAC}, std::byte{0x24},
    std::byte{0x80}, std::byte{0xFD}, std::byte{0xFF}, std::byte{0xFF}, std::byte{0x48},
    std::byte{0x81}, std::byte{0xEC}, std::byte{0x80}, std::byte{0x03}, std::byte{0x00},
    std::byte{0x00}};

/** Retired host/player readiness task that follows the local prologue completion task. */
constexpr std::uintptr_t kActivityReadyRva = 0xD474D0U;
constexpr std::array<std::byte, 15> kActivityReadyPrefix{std::byte{0x40},
                                                         std::byte{0x53},
                                                         std::byte{0x57},
                                                         std::byte{0x48},
                                                         std::byte{0x83},
                                                         std::byte{0xEC},
                                                         std::byte{0x48},
                                                         std::byte{0x48},
                                                         std::byte{0x8B},
                                                         std::byte{0xD9},
                                                         std::byte{0xBF},
                                                         std::byte{0x01},
                                                         std::byte{0x00},
                                                         std::byte{0x00},
                                                         std::byte{0x00}};

using Ready = bool(__fastcall*)() noexcept;
using StateUpdate = std::int32_t(__fastcall*)(std::byte*) noexcept;
using ActivityReady = std::int32_t(__fastcall*)(std::byte*) noexcept;

constexpr std::size_t kReadyIndex = 0U;
constexpr std::size_t kStateIndex = 1U;
constexpr std::size_t kActivityReadyIndex = 2U;
constexpr std::size_t kHookCount = 3U;

std::array<hooking::detour::Handle, kHookCount> g_handles{};
std::atomic<Ready> g_readyOriginal{nullptr};
std::atomic<StateUpdate> g_stateOriginal{nullptr};
std::atomic<ActivityReady> g_activityReadyOriginal{nullptr};
std::atomic_bool g_armed{};
std::atomic_bool g_readyReported{};
std::atomic_bool g_completionReported{};
std::atomic_bool g_activityReadyReported{};
hooking::CallGate g_callGate{};
std::mutex g_ownerLock{};
prologue_filler_lifecycle::OwnerLifecycle g_lifecycle{};

/** Resolves and validates the accessor, excluding only its two near-call displacements. */
[[nodiscard]] std::byte* ready_target() noexcept {
#if defined(SUNRISE_PROLOGUE_FILLER_READY_TESTING)
    return prologue_filler_ready_test_support::resolve_target(
        prologue_filler_ready_test_support::TargetSlot::ready);
#else
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    if (image == nullptr) {
        return nullptr;
    }
    std::byte* const target = image + kReadyRva;
    for (std::size_t index = 0; index < kReadyPrefix.size(); ++index) {
        if (target[index] != kReadyPrefix[index]) {
            return nullptr;
        }
    }
    if (target[4] != std::byte{0xE8}) {
        return nullptr;
    }
    constexpr std::size_t kBetweenCallsOffset = 9;
    for (std::size_t index = 0; index < kBetweenCalls.size(); ++index) {
        if (target[kBetweenCallsOffset + index] != kBetweenCalls[index]) {
            return nullptr;
        }
    }
    constexpr std::size_t kSecondCallOffset = 12;
    if (target[kSecondCallOffset] != std::byte{0xE8}) {
        return nullptr;
    }
    constexpr std::size_t kSuffixOffset = 17;
    for (std::size_t index = 0; index < kReadySuffix.size(); ++index) {
        if (target[kSuffixOffset + index] != kReadySuffix[index]) {
            return nullptr;
        }
    }
    return target;
#endif
}

/** Resolves and validates the prologue task update in the pinned client. */
[[nodiscard]] std::byte* state_update_target() noexcept {
#if defined(SUNRISE_PROLOGUE_FILLER_READY_TESTING)
    return prologue_filler_ready_test_support::resolve_target(
        prologue_filler_ready_test_support::TargetSlot::state_update);
#else
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    if (image == nullptr) {
        return nullptr;
    }
    std::byte* const target = image + kStateUpdateRva;
    for (std::size_t index = 0; index < kStateUpdatePrefix.size(); ++index) {
        if (target[index] != kStateUpdatePrefix[index]) {
            return nullptr;
        }
    }
    return target;
#endif
}

/** Resolves and validates the retired activity-readiness task in the pinned client. */
[[nodiscard]] std::byte* activity_ready_target() noexcept {
#if defined(SUNRISE_PROLOGUE_FILLER_READY_TESTING)
    return prologue_filler_ready_test_support::resolve_target(
        prologue_filler_ready_test_support::TargetSlot::activity_ready);
#else
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    if (image == nullptr) {
        return nullptr;
    }
    std::byte* const target = image + kActivityReadyRva;
    for (std::size_t index = 0; index < kActivityReadyPrefix.size(); ++index) {
        if (target[index] != kActivityReadyPrefix[index]) {
            return nullptr;
        }
    }
    return target;
#endif
}

/** @return True while the explicit override targets either half of the Red War opening. */
[[nodiscard]] bool red_war_opening_is_forced() noexcept {
    state::activity::forced::ForcedDestination forced{};
    state::activity::forced::snapshot(forced);
    const std::string_view package(forced.packageName.data(), forced.packageNameLength);
    return state::activity::forced::override_active()
           && (package == "cine_110_twr" || package == "mission_towerfall");
}

/** Supplies the local completion bit that the retired activity authority never sets. */
__declspec(noinline) bool __fastcall ready() noexcept {
    hooking::CallGate::Scope call(g_callGate);
    const Ready original = hooking::await_original(g_readyOriginal);
    const bool result = original();
    if (call.accepts_side_effects() && g_armed.load(std::memory_order_acquire)
        && red_war_opening_is_forced()) {
        if (!g_readyReported.exchange(true, std::memory_order_relaxed)) {
            core::log::write(
                core::log::Channel::client,
                core::log::Level::info,
                "ev=bootflow stage=prologue_ready result=forced activity=red_war_opening");
        }
        if (call.accepts_side_effects()) {
            return true;
        }
    }
    return result;
}

/**
 * Publishes the task's normal successful event after the forced ready answer. With no retired
 * authority callback, the task has no authored event bit and otherwise returns -1 forever.
 */
__declspec(noinline) std::int32_t __fastcall state_update(std::byte* state) noexcept {
    hooking::CallGate::Scope call(g_callGate);
    if (call.accepts_side_effects() && red_war_opening_is_forced()) {
        observe_activity_script_manager_table();
    }
    const StateUpdate original = hooking::await_original(g_stateOriginal);
    const std::int32_t result = original(state);
    if (call.accepts_side_effects() && g_armed.load(std::memory_order_acquire)
        && red_war_opening_is_forced()) {
        if (!g_completionReported.exchange(true, std::memory_order_relaxed)) {
            std::array<char, 128> line{};
            const int length = std::snprintf(line.data(),
                                             line.size(),
                                             "ev=bootflow stage=prologue_complete result=forced "
                                             "original=%d activity=red_war_opening",
                                             result);
            if (length > 0) {
                core::log::write(core::log::Channel::client,
                                 core::log::Level::info,
                                 {line.data(), static_cast<std::size_t>(length)});
            }
        }
        if (call.accepts_side_effects()) {
            return 3;
        }
    }
    return result;
}

/** Completes only the legacy four-peer readiness wait after its real local work has run. */
__declspec(noinline) std::int32_t __fastcall activity_ready(std::byte* state) noexcept {
    hooking::CallGate::Scope call(g_callGate);
    if (call.accepts_side_effects() && red_war_opening_is_forced()) {
        observe_activity_script_manager_table();
    }
    const ActivityReady original = hooking::await_original(g_activityReadyOriginal);
    const std::int32_t result = original(state);
    if (call.accepts_side_effects() && result == 1 && g_armed.load(std::memory_order_acquire)
        && red_war_opening_is_forced()) {
        if (!g_activityReadyReported.exchange(true, std::memory_order_relaxed)) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             "ev=bootflow stage=prologue_activity_ready result=forced original=1 "
                             "activity=red_war_opening");
        }
        if (call.accepts_side_effects()) {
            return 3;
        }
    }
    return result;
}

/** @return True only when no admitted replacement owns any lifecycle-managed state. */
[[nodiscard]] bool calls_idle() noexcept {
    return g_callGate.idle();
}

/** Production operations consumed by the source-linked pure lifecycle policy. */
class PrologueOwnerOperations final {
public:
    void close_side_effects() noexcept {
        g_callGate.quiesce();
    }

    [[nodiscard]] bool has_ownership() const noexcept {
        for (const hooking::detour::Handle& handle : g_handles) {
            if (handle.attached) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] bool attach_all_or_none() noexcept {
        std::byte* const readyTarget = ready_target();
        std::byte* const stateTarget = state_update_target();
        std::byte* const activityReadyTarget = activity_ready_target();
        if (readyTarget == nullptr || stateTarget == nullptr || activityReadyTarget == nullptr) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::warn,
                             "ev=bootflow stage=prologue_filler_group result=fail reason=target");
            return false;
        }

        const std::array specs{
            hooking::detour::Spec{readyTarget, reinterpret_cast<void*>(&ready)},
            hooking::detour::Spec{stateTarget, reinterpret_cast<void*>(&state_update)},
            hooking::detour::Spec{activityReadyTarget, reinterpret_cast<void*>(&activity_ready)},
        };
        if (!hooking::detour::install(std::span<const hooking::detour::Spec>(specs),
                                      std::span<hooking::detour::Handle>(g_handles))) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::warn,
                             "ev=bootflow stage=prologue_filler_group result=fail reason=attach");
            return false;
        }
        return true;
    }

    void publish_originals() noexcept {
        hooking::publish_original(g_readyOriginal,
                                  reinterpret_cast<Ready>(g_handles[kReadyIndex].original));
        hooking::publish_original(g_stateOriginal,
                                  reinterpret_cast<StateUpdate>(g_handles[kStateIndex].original));
        hooking::publish_original(
            g_activityReadyOriginal,
            reinterpret_cast<ActivityReady>(g_handles[kActivityReadyIndex].original));
    }

    void open_side_effects() noexcept {
        g_callGate.accept();
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         "ev=bootflow stage=prologue_filler_group result=ok hooks=3");
    }

    [[nodiscard]] prologue_filler_lifecycle::RemovalResult remove() noexcept {
        const std::array protectedEntries{
            hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&ready)},
            hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&state_update)},
            hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&activity_ready)},
            hooking::detour::ProtectedCodeEntry{
                reinterpret_cast<void*>(&hooking::call_gate_detail::enter)},
            hooking::detour::ProtectedCodeEntry{
                reinterpret_cast<void*>(&hooking::call_gate_detail::leave)},
        };
        const hooking::detour::UninstallResult result = hooking::detour::uninstall(
            std::span<hooking::detour::Handle>(g_handles),
            std::span<const hooking::detour::ProtectedCodeEntry>(protectedEntries),
            &calls_idle);
        switch (result) {
        case hooking::detour::UninstallResult::removed:
            return prologue_filler_lifecycle::RemovalResult::removed;
        case hooking::detour::UninstallResult::protectedCodeActive:
            core::log::write(
                core::log::Channel::client,
                core::log::Level::warn,
                "ev=bootflow stage=prologue_filler_uninstall result=deferred retained=1");
            return prologue_filler_lifecycle::RemovalResult::protected_code_active;
        case hooking::detour::UninstallResult::failed:
        default:
            core::log::write(
                core::log::Channel::client,
                core::log::Level::error,
                "ev=bootflow stage=prologue_filler_uninstall result=failed retained=1");
            return prologue_filler_lifecycle::RemovalResult::failed;
        }
    }

    void clear_removed() noexcept {
        g_readyOriginal.store(nullptr, std::memory_order_release);
        g_stateOriginal.store(nullptr, std::memory_order_release);
        g_activityReadyOriginal.store(nullptr, std::memory_order_release);
        g_armed.store(false, std::memory_order_release);
        g_readyReported.store(false, std::memory_order_release);
        g_completionReported.store(false, std::memory_order_release);
        g_activityReadyReported.store(false, std::memory_order_release);
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         "ev=bootflow stage=prologue_filler_uninstall result=ok retained=0");
    }
};

} // namespace

/** Attaches the scoped archived-Homecoming readiness and completion forces. */
bool install_prologue_filler_ready() noexcept {
    LateInstallGuard lateInstall;
    if (!lateInstall.accepted()) {
        return false;
    }
    const std::scoped_lock lock(g_ownerLock);
    PrologueOwnerOperations operations;
    return g_lifecycle.install(operations);
}

/** Disarms the force while Destiny performs the real initial-slice transition. */
void reset_prologue_filler_ready() noexcept {
    LateInstallGuard lateInstall;
    if (!lateInstall.accepted()) {
        return;
    }
    const std::scoped_lock lock(g_ownerLock);
    if (g_lifecycle.phase() == prologue_filler_lifecycle::OwnerPhase::quiescing) {
        return;
    }
    g_armed.store(false, std::memory_order_release);
    g_completionReported.store(false, std::memory_order_release);
    g_readyReported.store(false, std::memory_order_release);
    g_activityReadyReported.store(false, std::memory_order_release);
    reset_activity_script_bootstrap();
}

/** Arms the force only after Destiny confirms that its real initial slice completed. */
void arm_prologue_filler_ready() noexcept {
    LateInstallGuard lateInstall;
    if (!lateInstall.accepted()) {
        return;
    }
    const std::scoped_lock lock(g_ownerLock);
    if (g_lifecycle.phase() != prologue_filler_lifecycle::OwnerPhase::active
        || !g_callGate.accepting() || !red_war_opening_is_forced()) {
        return;
    }
    if (!g_armed.exchange(true, std::memory_order_acq_rel)) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         "ev=bootflow stage=prologue_arm result=ok activity=red_war_opening");
        // Identity 1 is requested by the authority post-apply observer after type 18 exists.
    }
}

/** Stops Sunrise-owned prologue work while retaining native forwarding. */
void quiesce_prologue_filler_ready() noexcept {
    const std::scoped_lock lock(g_ownerLock);
    PrologueOwnerOperations operations;
    g_lifecycle.quiesce(operations);
}

/** Detaches the archived-Homecoming readiness and completion forces as one protected batch. */
bool uninstall_prologue_filler_ready_checked() noexcept {
    const std::scoped_lock lock(g_ownerLock);
    PrologueOwnerOperations operations;
    return g_lifecycle.uninstall(operations);
}

#if defined(SUNRISE_PROLOGUE_FILLER_READY_TESTING)
namespace prologue_filler_ready_test_support {

std::uint32_t active_calls() noexcept {
    return g_callGate.active_calls();
}

std::uint32_t ownership_count() noexcept {
    std::uint32_t count{};
    for (const hooking::detour::Handle& handle : g_handles) {
        count += handle.attached ? 1U : 0U;
    }
    return count;
}

std::uint32_t published_original_count() noexcept {
    std::uint32_t count{};
    count += g_readyOriginal.load(std::memory_order_acquire) != nullptr ? 1U : 0U;
    count += g_stateOriginal.load(std::memory_order_acquire) != nullptr ? 1U : 0U;
    count += g_activityReadyOriginal.load(std::memory_order_acquire) != nullptr ? 1U : 0U;
    return count;
}

bool accepting() noexcept {
    return g_callGate.accepting();
}

bool armed() noexcept {
    return g_armed.load(std::memory_order_acquire);
}

bool invoke_ready() noexcept {
    return ready();
}

std::int32_t invoke_state_update(std::byte* state) noexcept {
    return state_update(state);
}

std::int32_t invoke_activity_ready(std::byte* state) noexcept {
    return activity_ready(state);
}

} // namespace prologue_filler_ready_test_support
#endif

} // namespace sunrise::client::hooks::bootflow
