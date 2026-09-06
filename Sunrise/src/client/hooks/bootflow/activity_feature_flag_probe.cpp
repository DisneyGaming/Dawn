#include <Windows.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <intrin.h>
#include <string_view>

#include "../../../core/logging/log.h"
#include "../../../state/activity/forced/activity_forced_destination.h"
#include "../../hooking/detour.h"
#include "internal.h"
#include "legacy_owner_sentinel.h"

namespace sunrise::client::hooks::bootflow {
namespace {

// The activity feature-flag getter FUN_7ff6184267d0: `movzx eax,[rip+DAT_7ff61a6ed5c6]; ret`.
// It returns a single global byte read by 14+ sites; when zero the solo activity-script init
// FUN_7ff6197a74f0 (which enables identities -> identity_enable) is skipped entirely. Offline the
// flag is zero, so the solo authored-enable path never runs.
constexpr std::uintptr_t kFlagGetterRva = 0x3B67D0U;
constexpr std::array<std::byte, 8> kFlagGetterPrefix{
    std::byte{0x0F}, std::byte{0xB6}, std::byte{0x05}, std::byte{0xEF},
    std::byte{0x6D}, std::byte{0x2C}, std::byte{0x02}, std::byte{0xC3}};

// The solo activity-init that reads the flag and, when it is set, enables identities. Any getter
// call returning into this function's body is the gate we want to open.
constexpr std::uintptr_t kSoloInitBeginRva = 0x17A74F0U;
constexpr std::uintptr_t kSoloInitEndRva = 0x17A7748U;

// Unsafe diagnostics is observe-only. Keep the former scoped poke permanently quarantined even if
// a future, separately named experiment reuses this recorder.
constexpr bool kPokeSoloInit = false;
static_assert(!kPokeSoloInit, "diagnostic probes must preserve native feature-flag results");

constexpr std::uint32_t kMaxCallerLogs = 48U;
constexpr std::uint64_t kHeartbeatMs = 4000U;

using FlagGetter = std::uint8_t(__fastcall*)() noexcept;

hooking::detour::Handle g_handle{};
std::atomic<FlagGetter> g_original{nullptr};
std::atomic_uint32_t g_callerLogs{};
std::atomic<std::uint64_t> g_lastValueSig{~0ULL};
std::atomic<std::uint64_t> g_lastBeat{};
std::atomic_bool g_installed{};

/** @return True while the Homecoming/towerfall override is active. */
[[nodiscard]] bool towerfall_forced(std::string_view& package) noexcept {
    namespace forced = state::activity::forced;
    if (!forced::override_active()) {
        return false;
    }
    forced::ForcedDestination fd{};
    forced::snapshot(fd);
    const std::size_t len =
        fd.packageNameLength <= fd.packageName.size() ? fd.packageNameLength : fd.packageName.size();
    package = std::string_view(fd.packageName.data(), len);
    return package.find("towerfall") != std::string_view::npos
           || package.find("cine_110_twr") != std::string_view::npos;
}

void log_line(const char* data, int written) noexcept {
    if (written > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {data, static_cast<std::size_t>(written)});
    }
}

__declspec(noinline) std::uint8_t __fastcall flag_getter() noexcept {
    const FlagGetter original = g_original.load(std::memory_order_acquire);
    const std::uint8_t real = original != nullptr ? original() : 0U;

    const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    const auto image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const std::uintptr_t callerRva = image != 0 && caller >= image ? caller - image : 0U;
    const bool fromSoloInit = callerRva >= kSoloInitBeginRva && callerRva < kSoloInitEndRva;

    std::string_view package{};
    const bool towerfall = towerfall_forced(package);

    // Recorder: log whenever the real flag value changes, plus a heartbeat, plus the first N
    // distinct-ish callers, so one run shows if the flag is ever set offline and who reads it.
    const std::uint64_t now = GetTickCount64();
    const std::uint64_t sig = (static_cast<std::uint64_t>(real) << 1) | (fromSoloInit ? 1U : 0U);
    const bool changed = sig != g_lastValueSig.exchange(sig, std::memory_order_relaxed);
    const bool beat = now - g_lastBeat.load(std::memory_order_relaxed) >= kHeartbeatMs;
    const bool logCaller =
        (fromSoloInit || towerfall) && g_callerLogs.fetch_add(1, std::memory_order_relaxed) < kMaxCallerLogs;
    if (changed || beat || logCaller) {
        if (beat) {
            g_lastBeat.store(now, std::memory_order_relaxed);
        }
        std::array<char, core::log::kLineCapacity> line{};
        const int written = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_feature_flag value=%u caller_rva=0x%llX solo_init=%u "
            "towerfall=%u forced=%.*s",
            static_cast<unsigned>(real),
            static_cast<unsigned long long>(callerRva),
            fromSoloInit ? 1U : 0U,
            towerfall ? 1U : 0U,
            static_cast<int>(package.size()),
            package.data());
        log_line(line.data(), written);
    }

    return real;
}

/** Resolves and validates the flag getter without trusting on-disk code. */
[[nodiscard]] std::byte* flag_getter_target() noexcept {
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    if (image == nullptr) {
        return nullptr;
    }
    std::byte* const target = image + kFlagGetterRva;
    for (std::size_t index = 0; index < kFlagGetterPrefix.size(); ++index) {
        if (target[index] != kFlagGetterPrefix[index]) {
            return nullptr;
        }
    }
    return target;
}

[[nodiscard]] std::array<legacy_owner_sentinel::HookOwnership, 1>
activity_feature_flag_hook_ownership() noexcept {
    // LEGACY_OWNER_SENTINEL_BEGIN(activity_feature_flag, 1)
    return {{{g_handle.attached,
              g_original.load(std::memory_order_acquire) != nullptr}}};
    // LEGACY_OWNER_SENTINEL_END(activity_feature_flag)
}

} // namespace

bool activity_feature_flag_probe_attached() noexcept {
    return legacy_owner_sentinel::any_handle_attached(
        activity_feature_flag_hook_ownership());
}

bool activity_feature_flag_probe_has_ownership() noexcept {
    // LEGACY_OWNER_CLAIMS_BEGIN(activity_feature_flag, 1)
    const std::array claims{
        g_installed.load(std::memory_order_acquire),
    };
    // LEGACY_OWNER_CLAIMS_END(activity_feature_flag)
    return legacy_owner_sentinel::has_ownership(
        activity_feature_flag_hook_ownership(), claims);
}

bool install_activity_feature_flag_probe() noexcept {
    if (g_installed.load(std::memory_order_acquire)) {
        return true;
    }
    std::byte* const target = flag_getter_target();
    if (target == nullptr
        || !hooking::detour::install({target, reinterpret_cast<void*>(&flag_getter)}, g_handle)) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::warn,
                         "ev=bootflow stage=activity_feature_flag result=install_fail");
        return false;
    }
    g_original.store(reinterpret_cast<FlagGetter>(g_handle.original), std::memory_order_release);
    g_installed.store(true, std::memory_order_release);
    core::log::write(core::log::Channel::client,
                     core::log::Level::info,
                     "ev=bootflow stage=activity_feature_flag result=install_ok mode=observe_only");
    return true;
}

void uninstall_activity_feature_flag_probe() noexcept {
    if (!g_installed.load(std::memory_order_acquire)) {
        return;
    }
    if (g_handle.attached && !hooking::detour::uninstall(g_handle)) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::error,
                         "ev=bootflow stage=activity_feature_flag_uninstall result=failed retained=1");
        return;
    }
    g_original.store(nullptr, std::memory_order_release);
    g_installed.store(false, std::memory_order_release);
}

} // namespace sunrise::client::hooks::bootflow
