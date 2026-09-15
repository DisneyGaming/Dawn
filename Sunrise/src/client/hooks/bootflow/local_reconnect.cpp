#include <Windows.h>
#include <array>
#include <atomic>
#include <cstring>
#include "local_reconnect_policy.h"
#include "native_hook_ownership.h"
#include "internal.h"
#include "../../hooking/call_gate.h"
#include "../../hooking/detour.h"
#include "../../../core/logging/log.h"
#include "../../../core/settings/settings.h"
#include "../../../middleware/gameplay/descriptor/join_descriptor.h"
#include "../../../server/gameplay/endpoint/gameplay_endpoint.h"

namespace sunrise::client::hooks::bootflow {
namespace {
namespace policy = local_reconnect;
hooking::CallGate gate;
hooking::detour::Handle detour;
std::atomic_uint reports{};
using Close = void(__fastcall*)(void*, std::int32_t, bool) noexcept;

bool read_snapshot(void* manager, std::int32_t index, policy::Snapshot& out) noexcept {
    if (!manager || index < 0 || index >= 64) return false;
    __try {
        const auto* root = static_cast<const std::byte*>(manager);
        const auto* channel = root + policy::kChannelBase + index * policy::kChannelStride;
        const auto* config = *reinterpret_cast<const std::byte* const*>(root + 0x28);
        if (!config) return false;
        std::memcpy(&out.cooldown, config + 0xC, sizeof(out.cooldown));
        std::memcpy(&out.managerState, channel + policy::kManagerState, sizeof(out.managerState));
        std::memcpy(&out.channelState, channel + policy::kChannelState, sizeof(out.channelState));
        std::memcpy(&out.closeReason, channel + policy::kCloseReason, sizeof(out.closeReason));
        std::memcpy(&out.stateTime, channel + policy::kStateTime, sizeof(out.stateTime));
        std::memcpy(&out.retry, channel + policy::kRetry, sizeof(out.retry));
        std::memcpy(out.address.data(), channel + policy::kAddress, out.address.size());
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

bool local_endpoint(policy::Address& expected) noexcept {
    const auto& settings = core::settings::get();
    const auto& gameplay = settings.server.gameplay;
    constexpr std::array<unsigned char, 4> loopback{127, 0, 0, 1};
    if (settings.client.externalServer.enabled
        || gameplay.topology != core::settings::server::gameplay::Topology::embedded
        || gameplay.bindAddress != loopback || gameplay.advertisedAddress != loopback
        || gameplay.transportAddress != loopback || !server::gameplay::endpoint::ready()) return false;
    const auto endpoint = server::gameplay::endpoint::advertised();
    if (endpoint.address != 0x7F000001 || endpoint.port == 0 || endpoint.port != gameplay.port) return false;
    middleware::gameplay::descriptor::write_net_addr(endpoint.address, endpoint.port, expected);
    return true;
}

// This runs on the existing channel-update owner, after native security reset.
// Only its waiting-state timestamp changes; the original close always runs once.
// No config, channel state, owner, packet, security key or retry flag is replaced.
__declspec(noinline) void __fastcall close_channel(void* manager, std::int32_t index,
                                                  bool resetOwners) noexcept {
    const hooking::CallGate::Scope call{gate};
    policy::Snapshot before{}, after{};
    policy::Address expected{};
    const bool eligible = call.accepts_side_effects() && local_endpoint(expected)
        && read_snapshot(manager, index, before);
    reinterpret_cast<Close>(detour.original)(manager, index, resetOwners);
    std::uint64_t shortened{};
    if (eligible && read_snapshot(manager, index, after)
        && policy::shortened_time(before, after, true, expected, shortened)) {
        auto* channel = static_cast<std::byte*>(manager) + policy::kChannelBase
            + index * policy::kChannelStride;
        std::memcpy(channel + policy::kStateTime, &shortened, sizeof(shortened));
        if (reports.fetch_add(1, std::memory_order_relaxed) < 64)
            core::log::write(core::log::Channel::client, core::log::Level::info,
                "ev=local_reconnect result=shortened reason=owners_released old_ms=10000 new_ms=250 security=native");
    }
}
bool idle() noexcept { return gate.idle(); }
bool matches(std::uintptr_t address, const unsigned char* bytes, std::size_t size) noexcept {
    __try { return std::memcmp(reinterpret_cast<const void*>(address), bytes, size) == 0; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
constexpr std::array<unsigned char, 31> prefix{
    0x48,0x89,0x5C,0x24,0x20,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,
    0x48,0x8D,0xAC,0x24,0x70,0xFD,0xFF,0xFF,0x48,0x81,0xEC,0x90,0x03,0x00,0x00};
constexpr std::array<unsigned char, 8> waitState{0x41,0xC7,0x87,0x40,0x30,0x00,0x00,0x01};
constexpr std::array<unsigned char, 7> timestamp{0x49,0x89,0x87,0x48,0x30,0x00,0x00};
constexpr std::array<unsigned char, 8> cooldownLoad{0x49,0x8B,0x4D,0x28,0x8B,0x41,0x0C,0xEB};
}
bool install_local_reconnect() noexcept {
    if (detour.attached) return gate.accepting();
    const auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    if (!base || !matches(base + native_hook_ownership::kLocalReconnect[0], prefix.data(), prefix.size())
        || !matches(base + 0x17CC9FB, waitState.data(), waitState.size())
        || !matches(base + 0x17CCA2F, timestamp.data(), timestamp.size())
        || !matches(base + 0x1802D7A, cooldownLoad.data(), cooldownLoad.size())) return false;
    if (!hooking::detour::install({reinterpret_cast<void*>(base + native_hook_ownership::kLocalReconnect[0]),
                                  reinterpret_cast<void*>(&close_channel)}, detour)) return false;
    gate.accept();
    core::log::write(core::log::Channel::client, core::log::Level::info,
        "ev=local_reconnect result=installed scope=embedded_loopback_clean_close cooldown_ms=250");
    return true;
}
void quiesce_local_reconnect() noexcept { gate.quiesce(); }
bool uninstall_local_reconnect() noexcept {
    gate.quiesce();
    if (!detour.attached) return true;
    const std::array entries{
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&close_channel)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&hooking::call_gate_detail::enter)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&hooking::call_gate_detail::leave)}};
    if (hooking::detour::uninstall(detour, entries, &idle) != hooking::detour::UninstallResult::removed) return false;
    reports.store(0);
    return true;
}
}
