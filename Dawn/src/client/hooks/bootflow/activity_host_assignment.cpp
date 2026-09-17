#include <Windows.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string_view>

#include "../../../core/logging/log.h"
#include "../../../core/settings/settings.h"
#include "../../../state/activity/forced/activity_forced_destination.h"
#include "../../../state/activity/runtime.h"
#include "../../hooking/detour.h"
#include "internal.h"

namespace dawn::client::hooks::bootflow {
namespace {

/**
 * Applies decoded activity message 54 to the activity client. The complete prologue continues
 * through the destination list offset, making the match specific to this Season of Arrivals
 * build's bubble-host table copy.
 */
constexpr std::string_view kApplySignatureText =
    "48 89 5C 24 18 48 89 74 24 20 57 48 81 EC 30 01 00 00 48 8B 05 ? ? ? ? 48 33 C4 "
    "48 89 84 24 20 01 00 00 48 8B DA 48 8B F1 E8 ? ? ? ? 48 8D 86 E8 5B 06 00";
constexpr auto kApplySignature =
    signature<signature_length(kApplySignatureText)>(kApplySignatureText);

/** Consumes decoded message 4 and copies its Activity Host session id into client state. */
constexpr std::string_view kJoinResultSignatureText =
    "48 89 5C 24 18 55 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 E0 F9 FF FF "
    "48 81 EC 20 07 00 00 48 8B 05 ? ? ? ? 48 33 C4 48 89 85 10 06 00 00 "
    "48 8B F2 48 89 54 24 70 BA 04 00 00 00";
constexpr auto kJoinResultSignature =
    signature<signature_length(kJoinResultSignatureText)>(kJoinResultSignatureText);

/** Native direct-address serializer used by the message-54 schema. */
constexpr std::string_view kAddressEncodeSignatureText =
    "48 89 5C 24 18 48 89 7C 24 20 55 48 8D 6C 24 F0 48 81 EC 10 01 00 00 48 8B 05 ? ? ? ? "
    "48 33 C4 48 89 45 00 48 8B F9 C7 44 24 34 01 00 00 00";
constexpr auto kAddressEncodeSignature =
    signature<signature_length(kAddressEncodeSignatureText)>(kAddressEncodeSignatureText);

/** Native validator paired with the direct-address serializer. */
constexpr std::string_view kAddressValidSignatureText =
    "40 53 48 81 EC 20 01 00 00 48 8B 05 ? ? ? ? 48 33 C4 48 89 84 24 10 01 00 00 48 8B D9 "
    "C7 44 24 44 01 00 00 00";
constexpr auto kAddressValidSignature =
    signature<signature_length(kAddressValidSignatureText)>(kAddressValidSignatureText);

/** Message 54 decodes a 32-entry list whose native records are 0xf8 bytes each. */
constexpr std::size_t kListHeaderSize = 8;
constexpr std::size_t kRecordSize = 0xF8;
constexpr std::size_t kStateOffset = 0;
constexpr std::size_t kSliceSetOffset = 4;
constexpr std::size_t kPeerIndexOffset = 8;
constexpr std::size_t kBubbleHostIdOffset = 0xC;
constexpr std::size_t kSessionIdOffset = 0x10;
constexpr std::size_t kSessionIdCapacity = 0x80;
constexpr std::size_t kUnresponsiveOffset = 0x90;
constexpr std::size_t kExternalAddressOffset = 0x91;
constexpr std::size_t kExternalAddressSize = 0x5F;
constexpr std::size_t kReadyOffset = 0xF0;
constexpr std::size_t kTransportAddressSize = 0x14;
constexpr std::size_t kTransportEndpointIdOffset = 4;
constexpr std::size_t kTransportPortOffset = 0x10;
/** Native state-name table: 0 starting, 1 loading, 2 active, 3 shutting down. */
constexpr std::uint8_t kActiveState = 2;
constexpr std::int8_t kLocalPeerIndex = 0;
constexpr std::int32_t kInitialSliceSet = 0;
constexpr std::int32_t kEmptyHostCount = 0;
constexpr std::int32_t kLocalHostCount = 1;
constexpr unsigned kMaximumReports = 4;
/** Little-endian memory form whose bytes format as 127.0.0.1. */
constexpr std::uint32_t kLoopbackIpv4 = 0x0100007FU;
constexpr std::uint16_t kDirectEndpointId = 0;
/** The game resolves endpoint type 2 as its local bubble-host UDP socket. */
constexpr std::uint16_t kLocalBubbleHostPort = 2003;
constexpr std::string_view kActivitySessionText = "dawn-red-war-opening";
constexpr std::size_t kJoinResultStatusOffset = 0x2970;
constexpr std::size_t kJoinResultSessionTextOffset = 0x2971;
constexpr std::size_t kJoinResultKeepaliveFlagOffset = 0x2A08;
constexpr std::size_t kJoinResultHostSessionTextOffset = 0x2B24;
constexpr std::size_t kClientFlagsOffset = 0x130;
constexpr std::size_t kClientCorrelationOffset = 0x1670;
constexpr std::size_t kClientSessionIdOffset = 0x3FE0;
constexpr std::size_t kClientJoinResultOffset = 0x3FE8;
constexpr std::size_t kJoinTextCaptureSize = 48;

using ApplyHostList = std::uint8_t(__fastcall*)(void*, std::byte*) noexcept;
using ApplyJoinResult = std::uint8_t(__fastcall*)(void*, std::byte*) noexcept;
using EncodeAddress = void(__fastcall*)(const std::byte*, std::byte*) noexcept;
using ValidateAddress = std::uint8_t(__fastcall*)(const std::byte*) noexcept;

hooking::detour::Handle g_hostListHandle{};
hooking::detour::Handle g_joinResultHandle{};
std::atomic<ApplyHostList> g_original{nullptr};
std::atomic<ApplyJoinResult> g_joinResultOriginal{nullptr};
std::atomic<EncodeAddress> g_encodeAddress{nullptr};
std::atomic<ValidateAddress> g_validateAddress{nullptr};
std::atomic<unsigned> g_reported{0};
std::atomic<bool> g_sessionReported{false};

template <typename T>
[[nodiscard]] T guarded_read(const std::byte* address, T fallback = {}) noexcept {
    if (address == nullptr) {
        return fallback;
    }
    __try {
        T value{};
        std::memcpy(&value, address, sizeof value);
        return value;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return fallback;
    }
}

void capture_bounded_text(const std::byte* source,
                          std::array<char, kJoinTextCaptureSize>& output) noexcept {
    output.fill('\0');
    if (source == nullptr) {
        return;
    }
    __try {
        for (std::size_t index = 0; index + 1 < output.size(); ++index) {
            const unsigned char value = static_cast<unsigned char>(source[index]);
            if (value == 0) {
                break;
            }
            output[index] = value >= 0x20U && value <= 0x7EU ? static_cast<char>(value) : '.';
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        output.fill('\0');
    }
}

void report_join_snapshot(std::string_view phase,
                          const std::byte* activityClient,
                          const std::byte* result) noexcept {
    std::array<char, kJoinTextCaptureSize> sessionText{};
    std::array<char, kJoinTextCaptureSize> hostText{};
    std::array<char, kJoinTextCaptureSize> copiedHostText{};
    capture_bounded_text(result != nullptr ? result + kJoinResultSessionTextOffset : nullptr,
                         sessionText);
    capture_bounded_text(result != nullptr ? result + kJoinResultHostSessionTextOffset : nullptr,
                         hostText);
    capture_bounded_text(activityClient != nullptr
                             ? activityClient + kClientJoinResultOffset
                                   + kJoinResultHostSessionTextOffset
                             : nullptr,
                         copiedHostText);

    std::array<char, core::log::kLineCapacity> line{};
    const int written = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_host_join_snapshot phase=%.*s client=%p result=%p result_correlation=0x%08X client_correlation=0x%08X result_session=0x%016llX runtime_session=0x%016llX result_status=%d keepalive_flag=%u client_flags=0x%04X session_text='%s' host_text='%s' client_session=0x%016llX copied_host_text='%s'",
        static_cast<int>(phase.size()),
        phase.data(),
        static_cast<const void*>(activityClient),
        static_cast<const void*>(result),
        guarded_read<std::uint32_t>(result, 0U),
        guarded_read<std::uint32_t>(
            activityClient != nullptr ? activityClient + kClientCorrelationOffset : nullptr, 0U),
        static_cast<unsigned long long>(guarded_read<std::uint64_t>(
            result != nullptr ? result + 8 : nullptr, 0U)),
        static_cast<unsigned long long>(state::activity::newest_joined_session()),
        static_cast<int>(guarded_read<std::int8_t>(
            result != nullptr ? result + kJoinResultStatusOffset : nullptr, -1)),
        static_cast<unsigned int>(guarded_read<std::uint8_t>(
            result != nullptr ? result + kJoinResultKeepaliveFlagOffset : nullptr, 0U)),
        static_cast<unsigned int>(guarded_read<std::uint16_t>(
            activityClient != nullptr ? activityClient + kClientFlagsOffset : nullptr, 0U)),
        sessionText.data(),
        hostText.data(),
        static_cast<unsigned long long>(guarded_read<std::uint64_t>(
            activityClient != nullptr ? activityClient + kClientSessionIdOffset : nullptr, 0U)),
        copiedHostText.data());
    if (written > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(written)});
    }
}

/**
 * Publishes the same bounded Activity Host session name in both decoded join-result fields.
 * The native host-list record already carries this name; leaving the join result blank keeps the
 * peer activity layer dormant even after backend membership has been committed.
 */
[[nodiscard]] bool publish_join_session_text(std::byte* result) noexcept {
    if (result == nullptr
        || state::activity::newest_joined_session() == state::activity::kAbsentSessionId
        || kActivitySessionText.empty() || kActivitySessionText.size() >= kSessionIdCapacity) {
        return false;
    }
    __try {
        std::byte* const sessionText = result + kJoinResultSessionTextOffset;
        std::byte* const hostSessionText = result + kJoinResultHostSessionTextOffset;
        SecureZeroMemory(sessionText, kSessionIdCapacity);
        SecureZeroMemory(hostSessionText, kSessionIdCapacity);
        std::memcpy(sessionText, kActivitySessionText.data(), kActivitySessionText.size());
        std::memcpy(hostSessionText, kActivitySessionText.data(), kActivitySessionText.size());
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

/** @return True only while the operator has selected one half of the Red War opening. */
[[nodiscard]] bool red_war_opening_selected() noexcept {
    state::activity::forced::ForcedDestination forced{};
    state::activity::forced::snapshot(forced);
    const std::string_view package(forced.packageName.data(), forced.packageNameLength);
    return state::activity::forced::override_active()
           && (package == "cine_110_twr" || package == "mission_towerfall");
}

/** Writes one decoded local-host record without replacing any real server assignment. */
[[nodiscard]] bool publish_local_host(std::byte* list,
                                      std::uint64_t sessionId,
                                      std::uint16_t port) noexcept {
    const EncodeAddress encodeAddress = g_encodeAddress.load(std::memory_order_acquire);
    const ValidateAddress validateAddress = g_validateAddress.load(std::memory_order_acquire);
    if (list == nullptr || sessionId == state::activity::kAbsentSessionId || port == 0
        || encodeAddress == nullptr || validateAddress == nullptr) {
        return false;
    }
    __try {
        std::int32_t count = 0;
        std::memcpy(&count, list, sizeof count);
        if (count != kEmptyHostCount) {
            return false;
        }
        std::byte* const record = list + kListHeaderSize;
        SecureZeroMemory(record, kRecordSize);
        record[kStateOffset] = static_cast<std::byte>(kActiveState);
        std::memcpy(record + kSliceSetOffset, &kInitialSliceSet, sizeof kInitialSliceSet);
        record[kPeerIndexOffset] = static_cast<std::byte>(kLocalPeerIndex);
        const std::uint32_t hostId = static_cast<std::uint32_t>(sessionId);
        std::memcpy(record + kBubbleHostIdOffset, &hostId, sizeof hostId);
        char* const sessionText = reinterpret_cast<char*>(record + kSessionIdOffset);
        if (kActivitySessionText.size() >= kSessionIdCapacity) {
            SecureZeroMemory(record, kRecordSize);
            return false;
        }
        std::memcpy(sessionText, kActivitySessionText.data(), kActivitySessionText.size());
        record[kUnresponsiveOffset] = std::byte{0};
        SecureZeroMemory(record + kExternalAddressOffset, kExternalAddressSize);
        std::byte address[kTransportAddressSize]{};
        std::memcpy(address, &kLoopbackIpv4, sizeof kLoopbackIpv4);
        std::memcpy(address + kTransportEndpointIdOffset,
                    &kDirectEndpointId,
                    sizeof kDirectEndpointId);
        std::memcpy(address + kTransportPortOffset, &port, sizeof port);
        encodeAddress(address, record + kExternalAddressOffset);
        if (validateAddress(record + kExternalAddressOffset) == 0) {
            SecureZeroMemory(record, kRecordSize);
            return false;
        }
        record[kReadyOffset] = std::byte{1};
        std::memcpy(list, &kLocalHostCount, sizeof kLocalHostCount);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

/** Emits a bounded result line for each empty table the bridge sees. */
void report(bool published, std::uint64_t sessionId, std::uint16_t port) noexcept {
    if (g_reported.fetch_add(1, std::memory_order_relaxed) >= kMaximumReports) {
        return;
    }
    std::array<char, 176> line{};
    const int written = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_host_assignment result=%s state=active session=0x%016llX endpoint=127.0.0.1:%u",
        published ? "forced" : "skip",
        static_cast<unsigned long long>(sessionId),
        static_cast<unsigned>(port));
    if (written > 0) {
        core::log::write(core::log::Channel::client,
                         published ? core::log::Level::info : core::log::Level::warn,
                         {line.data(), static_cast<std::size_t>(written)});
    }
}

/** Supplies one local active host to the native list copy for Homecoming only. */
std::uint8_t __fastcall apply(void* activityClient, std::byte* list) noexcept {
    const ApplyHostList original = g_original.load(std::memory_order_acquire);
    if (original == nullptr) {
        return 0;
    }
    if (red_war_opening_selected()) {
        const std::uint64_t sessionId = state::activity::newest_joined_session();
        const std::uint16_t port = kLocalBubbleHostPort;
        const bool published = publish_local_host(list, sessionId, port);
        report(published, sessionId, port);
    }
    return original(activityClient, list);
}

/** Completes the accepted join result with the local Activity Host session name. */
std::uint8_t __fastcall apply_join_result(void* activityClient, std::byte* result) noexcept {
    const ApplyJoinResult original = g_joinResultOriginal.load(std::memory_order_acquire);
    if (original == nullptr) {
        return 0;
    }
    const bool openingSelected = red_war_opening_selected();
    const bool sessionPublished = openingSelected && publish_join_session_text(result);
    if (openingSelected && !g_sessionReported.exchange(true, std::memory_order_relaxed)) {
        core::log::write(core::log::Channel::client,
                         sessionPublished ? core::log::Level::info
                                          : core::log::Level::warn,
                         sessionPublished
                             ? "ev=bootflow stage=activity_host_session result=published id=dawn-red-war-opening reason=membership_ready"
                             : "ev=bootflow stage=activity_host_session result=skip reason=membership_or_result_missing");
    }
    if (openingSelected) {
        report_join_snapshot("before", static_cast<std::byte*>(activityClient), result);
    }
    const std::uint8_t applied = original(activityClient, result);
    if (openingSelected) {
        report_join_snapshot("after", static_cast<std::byte*>(activityClient), result);
    }
    notify_activity_script_client_joined(static_cast<std::byte*>(activityClient));
    return applied;
}

} // namespace

/** Attaches the scoped decoded bubble-host assignment bridge. */
bool install_activity_host_assignment() noexcept {
    if (g_hostListHandle.attached && g_joinResultHandle.attached) {
        return true;
    }
    std::byte* const addressEncoder =
        scan_main_image_unique(kAddressEncodeSignature, "activity_host_assignment_address_encode");
    std::byte* const addressValidator =
        scan_main_image_unique(kAddressValidSignature, "activity_host_assignment_address_valid");
    std::byte* const target =
        scan_main_image_unique(kApplySignature, "activity_host_assignment_apply");
    std::byte* const joinResultTarget =
        scan_main_image_unique(kJoinResultSignature, "activity_host_assignment_join_result");
    if (target == nullptr || joinResultTarget == nullptr || addressEncoder == nullptr
        || addressValidator == nullptr) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::warn,
                         "ev=bootflow stage=activity_host_assignment result=fail reason=target_join_or_address_codec");
        return false;
    }
    const std::array specs{
        hooking::detour::Spec{target, reinterpret_cast<void*>(&apply)},
        hooking::detour::Spec{joinResultTarget, reinterpret_cast<void*>(&apply_join_result)},
    };
    std::array<hooking::detour::Handle, 2> installed{};
    if (!hooking::detour::install(specs, installed)) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::warn,
                         "ev=bootflow stage=activity_host_assignment result=fail reason=attach");
        return false;
    }
    g_hostListHandle = installed[0];
    g_joinResultHandle = installed[1];
    g_encodeAddress.store(reinterpret_cast<EncodeAddress>(addressEncoder),
                          std::memory_order_release);
    g_validateAddress.store(reinterpret_cast<ValidateAddress>(addressValidator),
                            std::memory_order_release);
    g_original.store(reinterpret_cast<ApplyHostList>(g_hostListHandle.original),
                     std::memory_order_release);
    g_joinResultOriginal.store(reinterpret_cast<ApplyJoinResult>(g_joinResultHandle.original),
                               std::memory_order_release);
    core::log::write(core::log::Channel::client,
                     core::log::Level::info,
                     "ev=bootflow stage=activity_host_assignment result=ok");
    return true;
}

/** Detaches the decoded bubble-host assignment bridge. */
void uninstall_activity_host_assignment() noexcept {
    if (g_hostListHandle.attached || g_joinResultHandle.attached) {
        std::array handles{g_hostListHandle, g_joinResultHandle};
        (void)hooking::detour::uninstall(handles);
        g_hostListHandle = {};
        g_joinResultHandle = {};
    }
    g_original.store(nullptr, std::memory_order_release);
    g_joinResultOriginal.store(nullptr, std::memory_order_release);
    g_encodeAddress.store(nullptr, std::memory_order_release);
    g_validateAddress.store(nullptr, std::memory_order_release);
    g_reported.store(0, std::memory_order_release);
    g_sessionReported.store(false, std::memory_order_release);
}

} // namespace dawn::client::hooks::bootflow
