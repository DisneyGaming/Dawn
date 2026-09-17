#include <Windows.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string_view>

#include "../../../core/logging/log.h"
#include "../../../state/activity/forced/activity_forced_destination.h"
#include "../../hooking/detour.h"
#include "internal.h"
#include "legacy_owner_sentinel.h"

namespace dawn::client::hooks::bootflow {
namespace {

/** Decoder callbacks recovered from the pinned Season of Arrivals client registry. */
constexpr std::uintptr_t kMembershipDecodeRva = 0x173BFC0U;
constexpr std::uintptr_t kParametersDecodeRva = 0x17A2670U;
constexpr std::uintptr_t kReliableRegistryDecodeRva = 0x16E3140U;

constexpr std::array<std::byte, 16> kMembershipDecodePrefix{
    std::byte{0x48}, std::byte{0x8B}, std::byte{0xC4}, std::byte{0x53},
    std::byte{0x41}, std::byte{0x56}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x78}, std::byte{0x48}, std::byte{0x89},
    std::byte{0x68}, std::byte{0x08}, std::byte{0x4D}, std::byte{0x8B}};
constexpr std::array<std::byte, 16> kParametersDecodePrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x08}, std::byte{0x48}, std::byte{0x89}, std::byte{0x6C},
    std::byte{0x24}, std::byte{0x18}, std::byte{0x48}, std::byte{0x89},
    std::byte{0x74}, std::byte{0x24}, std::byte{0x20}, std::byte{0x57}};
constexpr std::array<std::byte, 16> kReliableRegistryDecodePrefix{
    std::byte{0x40}, std::byte{0x53}, std::byte{0x57}, std::byte{0x41},
    std::byte{0x54}, std::byte{0x41}, std::byte{0x56}, std::byte{0x41},
    std::byte{0x57}, std::byte{0xB8}, std::byte{0xB0}, std::byte{0x39},
    std::byte{0x02}, std::byte{0x00}, std::byte{0xE8}, std::byte{0xDD}};

constexpr std::int32_t kMembershipDecodedSize = 0x7980;
constexpr std::size_t kMembershipRevisionOffset = 0xCU;
constexpr std::size_t kMembershipBaseRevisionOffset = 0x1740U;
constexpr std::size_t kMembershipMemberCountOffset = 0x1748U;
constexpr std::size_t kMembershipPlayerCountOffset = 0x174AU;
constexpr std::size_t kMembershipHashOffset = 0x7978U;

using MembershipDecode = bool(__fastcall*)(void* reader,
                                             std::int32_t decodedSize,
                                             std::byte* output) noexcept;
using ParametersDecode = bool(__fastcall*)(void* reader, std::byte* output) noexcept;
using ReliableRegistryDecode = bool(__fastcall*)(std::byte* queue,
                                                   std::int32_t* messageId,
                                                   std::int32_t* decodedSize,
                                                   std::uint32_t maximumBits,
                                                   void** decodedObject) noexcept;

hooking::detour::Handle g_membershipHandle{};
hooking::detour::Handle g_parametersHandle{};
hooking::detour::Handle g_reliableRegistryHandle{};
std::atomic<MembershipDecode> g_membershipOriginal{nullptr};
std::atomic<ParametersDecode> g_parametersOriginal{nullptr};
std::atomic<ReliableRegistryDecode> g_reliableRegistryOriginal{nullptr};
std::atomic_uint32_t g_membershipObserved{};
std::atomic_uint32_t g_parametersObserved{};
std::atomic_uint32_t g_reliableRegistryObserved{};

template <typename Value>
[[nodiscard]] Value read_value(const std::byte* source, std::size_t offset) noexcept {
    Value value{};
    if (source != nullptr) {
        std::memcpy(&value, source + offset, sizeof(value));
    }
    return value;
}

struct ReaderProgress {
    std::uint32_t field18{};
    std::uint32_t field1C{};
    std::uint32_t field20{};
    std::uint32_t field24{};
    std::uint32_t field30{};
    std::uint32_t field34{};
};

[[nodiscard]] ReaderProgress reader_progress(const void* reader) noexcept {
    ReaderProgress progress{};
    if (reader == nullptr) {
        return progress;
    }
    const auto* const bytes = static_cast<const std::byte*>(reader);
    progress.field18 = read_value<std::uint32_t>(bytes, 0x18U);
    progress.field1C = read_value<std::uint32_t>(bytes, 0x1CU);
    progress.field20 = read_value<std::uint32_t>(bytes, 0x20U);
    progress.field24 = read_value<std::uint32_t>(bytes, 0x24U);
    progress.field30 = read_value<std::uint32_t>(bytes, 0x30U);
    progress.field34 = read_value<std::uint32_t>(bytes, 0x34U);
    return progress;
}

[[nodiscard]] bool opening_forced_destination(std::string_view& package) noexcept {
    static thread_local state::activity::forced::ForcedDestination forced{};
    state::activity::forced::snapshot(forced);
    package = std::string_view(forced.packageName.data(), forced.packageNameLength);
    return state::activity::forced::override_active()
           && (package == "cine_110_twr" || package == "mission_towerfall");
}

template <std::size_t PrefixSize>
[[nodiscard]] std::byte* validated_target(std::uintptr_t rva,
                                          const std::array<std::byte, PrefixSize>& prefix) noexcept {
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    if (image == nullptr) {
        return nullptr;
    }
    std::byte* const target = image + rva;
    for (std::size_t index = 0; index < prefix.size(); ++index) {
        if (target[index] != prefix[index]) {
            return nullptr;
        }
    }
    return target;
}

/** Records whether message 30 reaches and survives its native body decoder. */
__declspec(noinline) bool __fastcall membership_decode(void* reader,
                                                        std::int32_t decodedSize,
                                                        std::byte* output) noexcept {
    const ReaderProgress before = reader_progress(reader);
    const MembershipDecode original = g_membershipOriginal.load(std::memory_order_acquire);
    const bool result = original != nullptr && original(reader, decodedSize, output);
    const ReaderProgress after = reader_progress(reader);

    std::string_view package{};
    const bool opening = opening_forced_destination(package);
    const std::uint32_t observation =
        g_membershipObserved.fetch_add(1, std::memory_order_relaxed) + 1U;
    if (opening || observation <= 64U) {
        const bool sized = output != nullptr && decodedSize >= kMembershipDecodedSize;
        const std::uint64_t host = sized ? read_value<std::uint64_t>(output, 0U) : 0U;
        const std::uint32_t revision =
            sized ? read_value<std::uint32_t>(output, kMembershipRevisionOffset) : 0U;
        const std::int32_t base =
            sized ? read_value<std::int32_t>(output, kMembershipBaseRevisionOffset) : -2;
        const std::uint16_t members =
            sized ? read_value<std::uint16_t>(output, kMembershipMemberCountOffset) : 0U;
        const std::uint16_t players =
            sized ? read_value<std::uint16_t>(output, kMembershipPlayerCountOffset) : 0U;
        const std::uint32_t hash =
            sized ? read_value<std::uint32_t>(output, kMembershipHashOffset) : 0U;
        std::array<char, 640> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=gameplay stage=membership_decode n=%u result=%s decoded_size=%d output=%p host=0x%016llX revision=%u base=%d members=%u players=%u hash=0x%08X reader_before=%u,%u,%u,%u,%u,%u reader_after=%u,%u,%u,%u,%u,%u opening=%u package=%.*s",
            observation,
            result ? "accepted" : "rejected",
            decodedSize,
            static_cast<void*>(output),
            static_cast<unsigned long long>(host),
            revision,
            base,
            static_cast<unsigned>(members),
            static_cast<unsigned>(players),
            hash,
            before.field18,
            before.field1C,
            before.field20,
            before.field24,
            before.field30,
            before.field34,
            after.field18,
            after.field1C,
            after.field20,
            after.field24,
            after.field30,
            after.field34,
            opening ? 1U : 0U,
            static_cast<int>(package.size()),
            package.data());
        if (length > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(length)});
        }
    }
    return result;
}

/** Records whether message 38 reaches and survives its native parameter decoder. */
__declspec(noinline) bool __fastcall parameters_decode(void* reader, std::byte* output) noexcept {
    const ReaderProgress before = reader_progress(reader);
    const ParametersDecode original = g_parametersOriginal.load(std::memory_order_acquire);
    const bool result = original != nullptr && original(reader, output);
    const ReaderProgress after = reader_progress(reader);

    std::string_view package{};
    const bool opening = opening_forced_destination(package);
    const std::uint32_t observation =
        g_parametersObserved.fetch_add(1, std::memory_order_relaxed) + 1U;
    if (opening || observation <= 64U) {
        const std::uint64_t session = read_value<std::uint64_t>(output, 0U);
        const std::uint8_t reset = read_value<std::uint8_t>(output, 8U);
        const std::uint64_t released = read_value<std::uint64_t>(output, 0x10U);
        const std::uint64_t carried = read_value<std::uint64_t>(output, 0x18U);
        std::array<char, 608> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=gameplay stage=parameters_decode n=%u result=%s output=%p session=0x%016llX reset=%u released=0x%016llX carried=0x%016llX reader_before=%u,%u,%u,%u,%u,%u reader_after=%u,%u,%u,%u,%u,%u opening=%u package=%.*s",
            observation,
            result ? "accepted" : "rejected",
            static_cast<void*>(output),
            static_cast<unsigned long long>(session),
            static_cast<unsigned>(reset),
            static_cast<unsigned long long>(released),
            static_cast<unsigned long long>(carried),
            before.field18,
            before.field1C,
            before.field20,
            before.field24,
            before.field30,
            before.field34,
            after.field18,
            after.field1C,
            after.field20,
            after.field24,
            after.field30,
            after.field34,
            opening ? 1U : 0U,
            static_cast<int>(package.size()),
            package.data());
        if (length > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(length)});
        }
    }
    return result;
}

/** Records the native reliable-fragment reassembly and registry-dispatch boundary. */
__declspec(noinline) bool __fastcall reliable_registry_decode(std::byte* queue,
                                                               std::int32_t* messageId,
                                                               std::int32_t* decodedSize,
                                                               std::uint32_t maximumBits,
                                                               void** decodedObject) noexcept {
    const std::int32_t idBefore = messageId != nullptr ? *messageId : -1;
    const std::int32_t sizeBefore = decodedSize != nullptr ? *decodedSize : -1;
    void* const objectBefore = decodedObject != nullptr ? *decodedObject : nullptr;
    const std::uint32_t widthBefore = read_value<std::uint32_t>(queue, 0x10U);
    const std::uint8_t errorBefore = read_value<std::uint8_t>(queue, 0x09U);
    void* const registryBefore = read_value<void*>(queue, 0x28U);
    void* const beginBefore = read_value<void*>(queue, 0x80U);
    void* const endBefore = read_value<void*>(queue, 0x88U);

    const ReliableRegistryDecode original =
        g_reliableRegistryOriginal.load(std::memory_order_acquire);
    const bool result = original != nullptr
                        && original(queue,
                                    messageId,
                                    decodedSize,
                                    maximumBits,
                                    decodedObject);

    const std::int32_t idAfter = messageId != nullptr ? *messageId : -1;
    const std::int32_t sizeAfter = decodedSize != nullptr ? *decodedSize : -1;
    void* const objectAfter = decodedObject != nullptr ? *decodedObject : nullptr;
    const std::uint32_t widthAfter = read_value<std::uint32_t>(queue, 0x10U);
    const std::uint8_t errorAfter = read_value<std::uint8_t>(queue, 0x09U);
    void* const beginAfter = read_value<void*>(queue, 0x80U);
    void* const endAfter = read_value<void*>(queue, 0x88U);

    std::string_view package{};
    const bool opening = opening_forced_destination(package);
    const std::uint32_t observation =
        g_reliableRegistryObserved.fetch_add(1, std::memory_order_relaxed) + 1U;
    const bool changed = idBefore != idAfter || sizeBefore != sizeAfter
                         || objectBefore != objectAfter || widthBefore != widthAfter
                         || errorBefore != errorAfter || beginBefore != beginAfter
                         || endBefore != endAfter;
    if (result || changed || (opening && observation <= 256U) || observation <= 32U) {
        std::array<char, 896> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=gameplay stage=reliable_registry_decode n=%u result=%s queue=%p maximum_bits=%u width_before=%u width_after=%u id_before=%d id_after=%d size_before=%d size_after=%d object_before=%p object_after=%p error_before=%u error_after=%u registry=%p begin_before=%p end_before=%p begin_after=%p end_after=%p opening=%u package=%.*s",
            observation,
            result ? "accepted" : "rejected",
            static_cast<void*>(queue),
            maximumBits,
            widthBefore,
            widthAfter,
            idBefore,
            idAfter,
            sizeBefore,
            sizeAfter,
            objectBefore,
            objectAfter,
            static_cast<unsigned>(errorBefore),
            static_cast<unsigned>(errorAfter),
            registryBefore,
            beginBefore,
            endBefore,
            beginAfter,
            endAfter,
            opening ? 1U : 0U,
            static_cast<int>(package.size()),
            package.data());
        if (length > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(length)});
        }
    }
    return result;
}

} // namespace

bool group_initial_update_decode_probe_has_ownership() noexcept {
    // LEGACY_OWNER_SENTINEL_BEGIN(group_initial_update_decode, 3)
    const std::array hooks{
        legacy_owner_sentinel::HookOwnership{
            g_membershipHandle.attached,
            g_membershipOriginal.load(std::memory_order_acquire) != nullptr},
        legacy_owner_sentinel::HookOwnership{
            g_parametersHandle.attached,
            g_parametersOriginal.load(std::memory_order_acquire) != nullptr},
        legacy_owner_sentinel::HookOwnership{
            g_reliableRegistryHandle.attached,
            g_reliableRegistryOriginal.load(std::memory_order_acquire) != nullptr},
    };
    // LEGACY_OWNER_SENTINEL_END(group_initial_update_decode)
    static_assert(hooks.size() == 3U);
    return legacy_owner_sentinel::has_ownership(hooks);
}

/** Attaches read-only observers to initial-update reassembly and body decoding. */
bool install_group_initial_update_decode_probe() noexcept {
    if (g_membershipHandle.attached && g_parametersHandle.attached
        && g_reliableRegistryHandle.attached) {
        return true;
    }
    std::byte* const membershipTarget =
        validated_target(kMembershipDecodeRva, kMembershipDecodePrefix);
    std::byte* const parametersTarget =
        validated_target(kParametersDecodeRva, kParametersDecodePrefix);
    std::byte* const reliableRegistryTarget =
        validated_target(kReliableRegistryDecodeRva, kReliableRegistryDecodePrefix);
    if (membershipTarget == nullptr || parametersTarget == nullptr
        || reliableRegistryTarget == nullptr) {
        core::log::write(
            core::log::Channel::client,
            core::log::Level::warn,
            "ev=gameplay stage=initial_update_decode_probe result=fail reason=target");
        return false;
    }

    const hooking::detour::Spec membershipSpec{
        membershipTarget, reinterpret_cast<void*>(&membership_decode)};
    if (!hooking::detour::install(membershipSpec, g_membershipHandle)) {
        core::log::write(
            core::log::Channel::client,
            core::log::Level::warn,
            "ev=gameplay stage=initial_update_decode_probe result=fail reason=membership_attach");
        return false;
    }
    g_membershipOriginal.store(reinterpret_cast<MembershipDecode>(g_membershipHandle.original),
                               std::memory_order_release);

    const hooking::detour::Spec parametersSpec{
        parametersTarget, reinterpret_cast<void*>(&parameters_decode)};
    if (!hooking::detour::install(parametersSpec, g_parametersHandle)) {
        (void)hooking::detour::uninstall(g_membershipHandle);
        g_membershipOriginal.store(nullptr, std::memory_order_release);
        core::log::write(
            core::log::Channel::client,
            core::log::Level::warn,
            "ev=gameplay stage=initial_update_decode_probe result=fail reason=parameters_attach");
        return false;
    }
    g_parametersOriginal.store(reinterpret_cast<ParametersDecode>(g_parametersHandle.original),
                               std::memory_order_release);

    const hooking::detour::Spec reliableRegistrySpec{
        reliableRegistryTarget, reinterpret_cast<void*>(&reliable_registry_decode)};
    if (!hooking::detour::install(reliableRegistrySpec, g_reliableRegistryHandle)) {
        (void)hooking::detour::uninstall(g_parametersHandle);
        (void)hooking::detour::uninstall(g_membershipHandle);
        g_parametersOriginal.store(nullptr, std::memory_order_release);
        g_membershipOriginal.store(nullptr, std::memory_order_release);
        core::log::write(
            core::log::Channel::client,
            core::log::Level::warn,
            "ev=gameplay stage=initial_update_decode_probe result=fail reason=reliable_registry_attach");
        return false;
    }
    g_reliableRegistryOriginal.store(
        reinterpret_cast<ReliableRegistryDecode>(g_reliableRegistryHandle.original),
        std::memory_order_release);
    g_membershipObserved.store(0, std::memory_order_release);
    g_parametersObserved.store(0, std::memory_order_release);
    g_reliableRegistryObserved.store(0, std::memory_order_release);
    core::log::write(
        core::log::Channel::client,
        core::log::Level::info,
        "ev=gameplay stage=initial_update_decode_probe result=ok mode=observe membership_rva=0x173BFC0 parameters_rva=0x17A2670 reliable_registry_rva=0x16E3140");
    return true;
}

/** Detaches the decoder observers and clears their trampolines. */
void uninstall_group_initial_update_decode_probe() noexcept {
    if (g_reliableRegistryHandle.attached) {
        (void)hooking::detour::uninstall(g_reliableRegistryHandle);
    }
    if (g_parametersHandle.attached) {
        (void)hooking::detour::uninstall(g_parametersHandle);
    }
    if (g_membershipHandle.attached) {
        (void)hooking::detour::uninstall(g_membershipHandle);
    }
    g_reliableRegistryOriginal.store(nullptr, std::memory_order_release);
    g_parametersOriginal.store(nullptr, std::memory_order_release);
    g_membershipOriginal.store(nullptr, std::memory_order_release);
    g_reliableRegistryObserved.store(0, std::memory_order_release);
    g_parametersObserved.store(0, std::memory_order_release);
    g_membershipObserved.store(0, std::memory_order_release);
}

} // namespace dawn::client::hooks::bootflow
