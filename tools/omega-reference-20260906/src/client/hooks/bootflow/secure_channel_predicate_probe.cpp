#include <Windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <intrin.h>
#include <span>

#include "../../../core/filesystem/path.h"
#include "../../../core/logging/log.h"
#include "../../hooking/detour.h"
#include "internal.h"

namespace sunrise::client::hooks::bootflow {
namespace {

/** Vtable byte offsets used by the state-2 channel update at +0x1803189. */
constexpr std::size_t kNativeUpdateSlot = 0x00U / sizeof(void*);
constexpr std::size_t kSecureStatusSlot = 0x20U / sizeof(void*);
constexpr std::size_t kChannelGateSlot = 0x30U / sizeof(void*);
constexpr std::size_t kRequestSecureSlot = 0x80U / sizeof(void*);
constexpr std::size_t kStartSecuritySlot = 0x98U / sizeof(void*);
constexpr std::size_t kCommitSecuritySlot = 0xD0U / sizeof(void*);

/** Return addresses of the three calls inside the pinned client state-2 block. */
constexpr std::uintptr_t kChannelGateReturnRva = 0x180319FU;
constexpr std::uintptr_t kSecureStatusReturnRva = 0x18031B0U;
constexpr std::uintptr_t kRequestSecureReturnRva = 0x18032E9U;
constexpr std::uintptr_t kStartSecurityReturnRva = 0x1802E65U;
constexpr std::uintptr_t kCommitSecurityReturnRva = 0x1802E75U;

/** Enough repeated updates to show a changing result without flooding the run log. */
constexpr std::uint32_t kMaximumReportsPerMethod = 64U;
constexpr std::uintptr_t kCandidateExpansionRva = 0x179B270U;
constexpr std::size_t kOwnerSlotCount = 9U;

using NativeUpdate = void(__fastcall*)(void*, void*, const std::int32_t*, void*) noexcept;
using Predicate = bool(__fastcall*)(void*) noexcept;
using RequestSecure = void(__fastcall*)(void*, bool) noexcept;
using StartSecurity = void(__fastcall*)(void*, bool) noexcept;
using CommitSecurity = void(__fastcall*)(void*) noexcept;

using CandidateExpansion = std::uint64_t(__fastcall*)(std::int32_t, void*) noexcept;

SRWLOCK g_probeLock{SRWLOCK_INIT};
std::array<hooking::detour::Handle, 6> g_handles{};
std::atomic<NativeUpdate> g_nativeUpdateOriginal{nullptr};
std::atomic<Predicate> g_secureStatusOriginal{nullptr};
std::atomic<Predicate> g_channelGateOriginal{nullptr};
std::atomic<RequestSecure> g_requestSecureOriginal{nullptr};
std::atomic<StartSecurity> g_startSecurityOriginal{nullptr};
std::atomic<CommitSecurity> g_commitSecurityOriginal{nullptr};
std::atomic<void*> g_targetChannelObject{nullptr};
std::atomic_uint32_t g_nativeUpdateReports{};
std::atomic_uint32_t g_secureStatusReports{};
std::atomic_uint32_t g_channelGateReports{};
std::atomic_uint32_t g_requestSecureReports{};
std::atomic_uint32_t g_startSecurityReports{};
std::atomic_uint32_t g_commitSecurityReports{};
std::atomic_bool g_installed{};

/** Native channel-manager layout recovered from the runtime-decrypted update method. */
constexpr std::size_t kDesiredOffset = 0x18U;
constexpr std::size_t kSecondaryDesiredOffset = 0x19U;
constexpr std::size_t kRelayStateOffset = 0x1CU;
constexpr std::size_t kSelectedPathOffset = 0x48U;
constexpr std::size_t kCandidateCountOffset = 0x4AU;
constexpr std::size_t kCandidateChangedOffset = 0x4BU;
constexpr std::size_t kDirectReadyOffset = 0x4CU;
constexpr std::size_t kDirectReadyStickyOffset = 0x4DU;
constexpr std::size_t kOwnerRecordOffset = 0x68U;
constexpr std::size_t kOwnerRecordStride = 0x58U;
constexpr std::size_t kOwnerRecordCount = 18U;
constexpr std::size_t kOwnerResultOffset = 0x04U;
constexpr std::size_t kOwnerPrimaryAddressOffset = 0x08U;
constexpr std::size_t kOwnerRequestOffset = 0x20U;
constexpr std::size_t kOwnerSecondaryAddressOffset = 0x30U;
constexpr std::size_t kOwnerTransportOffset = 0x48U;
constexpr std::size_t kOwnerMaskOffset = 0x698U;
constexpr std::array<std::uint32_t, 5> kStateReportObservations{1U, 64U, 192U, 960U, 1280U};

/**
 * Records the nine identity handles supplied to the native security update and asks the same
 * read-only expansion helper used by that update how many direct candidates each handle owns.
 */
void report_owner_inputs(void* object,
                         const std::int32_t* ownerSlots,
                         std::uint32_t observation) noexcept {
    if (object == nullptr || ownerSlots == nullptr) {
        return;
    }
    std::array<std::int32_t, kOwnerSlotCount> identities{};
    std::array<std::uint8_t, kOwnerSlotCount> candidateCounts{};
    std::uint16_t slotReadMask = 0;
    std::uint16_t expansionFaultMask = 0;
    const auto image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const auto expand = image == 0U
                            ? nullptr
                            : reinterpret_cast<CandidateExpansion>(image + kCandidateExpansionRva);
    for (std::size_t index = 0; index < kOwnerSlotCount; ++index) {
        SIZE_T bytesRead = 0;
        if (ReadProcessMemory(GetCurrentProcess(),
                              ownerSlots + index,
                              &identities[index],
                              sizeof(identities[index]),
                              &bytesRead)
                == FALSE
            || bytesRead != sizeof(identities[index])) {
            identities[index] = INT32_MIN;
            continue;
        }
        slotReadMask |= static_cast<std::uint16_t>(1U << index);
        if (expand != nullptr && identities[index] >= -1 && identities[index] < 10) {
            std::array<std::byte, 256> candidates{};
            __try {
                const std::uint64_t count = expand(identities[index], candidates.data());
                candidateCounts[index] = static_cast<std::uint8_t>(std::min<std::uint64_t>(count, 255U));
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                candidateCounts[index] = 0xFFU;
                expansionFaultMask |= static_cast<std::uint16_t>(1U << index);
            }
        }
    }

    std::array<char, 768> line{};
    const int written = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=secure_channel_owner_inputs n=%u object=%p "
        "slots=%p read_mask=0x%03X expansion_fault_mask=0x%03X "
        "identities=%d,%d,%d,%d,%d,%d,%d,%d,%d "
        "expanded=%u,%u,%u,%u,%u,%u,%u,%u,%u mutation=observe_only",
        observation,
        object,
        static_cast<const void*>(ownerSlots),
        slotReadMask,
        expansionFaultMask,
        identities[0], identities[1], identities[2], identities[3], identities[4],
        identities[5], identities[6], identities[7], identities[8],
        candidateCounts[0], candidateCounts[1], candidateCounts[2], candidateCounts[3],
        candidateCounts[4], candidateCounts[5], candidateCounts[6], candidateCounts[7],
        candidateCounts[8]);
    if (written > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(written)});
    }
}

/** @return Main-image-relative return address, or zero outside the main image. */
[[nodiscard]] std::uintptr_t caller_rva(const void* returnAddress) noexcept {
    const auto image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const auto caller = reinterpret_cast<std::uintptr_t>(returnAddress);
    return image != 0 && caller >= image ? caller - image : 0U;
}

/** Writes one bounded method observation. */
void report_predicate(const char* method,
                      std::uint32_t observation,
                      void* object,
                      std::uintptr_t caller,
                      bool nativeResult,
                      bool result,
                      bool forced) noexcept {
    std::array<char, 384> line{};
    const int written = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=secure_channel_predicate method=%s n=%u object=%p "
        "caller_rva=0x%llX native=%u result=%u forced=%u mutation=%s",
        method,
        observation,
        object,
        static_cast<unsigned long long>(caller),
        nativeResult ? 1U : 0U,
        result ? 1U : 0U,
        forced ? 1U : 0U,
        forced ? "scoped_gate_override" : "observe_only");
    if (written > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(written)});
    }
}

/** Compact non-cryptographic fingerprint for one native 24-byte address candidate. */
[[nodiscard]] std::uint64_t address_fingerprint(const std::byte* address) noexcept {
    constexpr std::uint64_t kOffset = 1469598103934665603ULL;
    constexpr std::uint64_t kPrime = 1099511628211ULL;
    std::uint64_t hash = kOffset;
    for (std::size_t index = 0; index < 24U; ++index) {
        hash ^= std::to_integer<std::uint8_t>(address[index]);
        hash *= kPrime;
    }
    return hash;
}

/** Reports the per-owner records whose update result controls direct-security readiness. */
void report_channel_state(void* object, std::uint32_t observation) noexcept {
    if (object == nullptr) {
        return;
    }
    auto* const bytes = static_cast<std::byte*>(object);
    std::array<std::uint32_t, 8> stateCounts{};
    std::array<std::uint32_t, 8> resultCounts{};
    std::array<std::uint32_t, kOwnerRecordCount> states{};
    std::array<std::uint32_t, kOwnerRecordCount> results{};
    std::array<std::uint8_t, kOwnerRecordCount> requests{};
    std::array<std::uint64_t, kOwnerRecordCount> primaryHashes{};
    std::array<std::uint64_t, kOwnerRecordCount> secondaryHashes{};
    std::array<std::uintptr_t, kOwnerRecordCount> transports{};
    std::uint8_t desired = 0;
    std::uint8_t secondaryDesired = 0;
    std::uint32_t relayState = 0;
    std::uint16_t selectedPath = 0;
    std::uint8_t candidateCount = 0;
    std::uint8_t candidateChanged = 0;
    std::uint8_t directReady = 0;
    std::uint8_t directReadySticky = 0;
    std::uint16_t ownerMask = 0;
    __try {
        desired = std::to_integer<std::uint8_t>(bytes[kDesiredOffset]);
        secondaryDesired = std::to_integer<std::uint8_t>(bytes[kSecondaryDesiredOffset]);
        std::memcpy(&relayState, bytes + kRelayStateOffset, sizeof(relayState));
        std::memcpy(&selectedPath, bytes + kSelectedPathOffset, sizeof(selectedPath));
        candidateCount = std::to_integer<std::uint8_t>(bytes[kCandidateCountOffset]);
        candidateChanged = std::to_integer<std::uint8_t>(bytes[kCandidateChangedOffset]);
        directReady = std::to_integer<std::uint8_t>(bytes[kDirectReadyOffset]);
        directReadySticky = std::to_integer<std::uint8_t>(bytes[kDirectReadyStickyOffset]);
        std::memcpy(&ownerMask, bytes + kOwnerMaskOffset, sizeof(ownerMask));
        for (std::size_t index = 0; index < kOwnerRecordCount; ++index) {
            const std::byte* const record =
                bytes + kOwnerRecordOffset + (index * kOwnerRecordStride);
            std::memcpy(&states[index], record, sizeof(states[index]));
            std::memcpy(&results[index], record + kOwnerResultOffset, sizeof(results[index]));
            requests[index] = std::to_integer<std::uint8_t>(record[kOwnerRequestOffset]);
            primaryHashes[index] = address_fingerprint(record + kOwnerPrimaryAddressOffset);
            secondaryHashes[index] = address_fingerprint(record + kOwnerSecondaryAddressOffset);
            std::memcpy(&transports[index],
                        record + kOwnerTransportOffset,
                        sizeof(transports[index]));
            if (states[index] < stateCounts.size()) {
                ++stateCounts[states[index]];
            }
            if (results[index] < resultCounts.size()) {
                ++resultCounts[results[index]];
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::warn,
                         "ev=bootflow stage=secure_channel_state result=read_fault");
        return;
    }

    std::array<char, 640> line{};
    int written = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=secure_channel_state n=%u object=%p desired=%u secondary=%u "
        "relay_state=%u selected=%u candidates=%u changed=%u direct_ready=%u sticky=%u "
        "owner_mask=0x%04X states=0:%u,1:%u,2:%u,3:%u,4:%u,5:%u,6:%u,7:%u "
        "results=0:%u,1:%u,2:%u,3:%u,4:%u,5:%u,6:%u,7:%u mutation=observe_only",
        observation,
        object,
        desired,
        secondaryDesired,
        relayState,
        selectedPath,
        candidateCount,
        candidateChanged,
        directReady,
        directReadySticky,
        ownerMask,
        stateCounts[0],
        stateCounts[1],
        stateCounts[2],
        stateCounts[3],
        stateCounts[4],
        stateCounts[5],
        stateCounts[6],
        stateCounts[7],
        resultCounts[0],
        resultCounts[1],
        resultCounts[2],
        resultCounts[3],
        resultCounts[4],
        resultCounts[5],
        resultCounts[6],
        resultCounts[7]);
    if (written > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(written)});
    }
    for (std::size_t index = 0; index < kOwnerRecordCount; ++index) {
        if (states[index] == 0U && results[index] == 0U && requests[index] == 0U
            && transports[index] == 0U) {
            continue;
        }
        written = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=secure_channel_owner n=%u index=%zu state=%u result_code=%u "
            "request=%u primary_hash=0x%016llX secondary_hash=0x%016llX transport=%p "
            "mutation=observe_only",
            observation,
            index,
            states[index],
            results[index],
            requests[index],
            static_cast<unsigned long long>(primaryHashes[index]),
            static_cast<unsigned long long>(secondaryHashes[index]),
            reinterpret_cast<void*>(transports[index]));
        if (written > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(written)});
        }
    }
}

/** Observes the exact owner-identity inputs before the native updater consumes them unchanged. */
__declspec(noinline) void __fastcall native_update(void* object,
                                                   void* localAddress,
                                                   const std::int32_t* ownerSlots,
                                                   void* validationContexts) noexcept {
    const bool scoped = object == g_targetChannelObject.load(std::memory_order_acquire);
    const std::uint32_t observation = scoped
                                          ? g_nativeUpdateReports.fetch_add(
                                                1, std::memory_order_relaxed)
                                                + 1U
                                          : 0U;
    if (scoped && observation <= 8U) {
        report_owner_inputs(object, ownerSlots, observation);
    }
    const NativeUpdate original = g_nativeUpdateOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(object, localAddress, ownerSlots, validationContexts);
    }
}

/** Records the aggregate security-ready gate without changing native state. */
__declspec(noinline) bool __fastcall channel_gate(void* object) noexcept {
    const Predicate original = g_channelGateOriginal.load(std::memory_order_acquire);
    const bool result = original != nullptr && original(object);
    const std::uintptr_t caller = caller_rva(_ReturnAddress());
    const bool scoped = caller == kChannelGateReturnRva
                        && object == g_targetChannelObject.load(std::memory_order_acquire);
    const std::uint32_t observation = scoped
                                          ? g_channelGateReports.fetch_add(
                                                1, std::memory_order_relaxed)
                                                + 1U
                                          : 0U;
    if (scoped && observation <= kMaximumReportsPerMethod) {
        report_predicate("gate_30", observation, object, caller, result, result, false);
    }
    if (scoped
        && std::find(kStateReportObservations.begin(),
                     kStateReportObservations.end(),
                     observation)
               != kStateReportObservations.end()) {
        report_channel_state(object, observation);
    }
    return result;
}

/** Records whether the channel already considers itself secure. */
__declspec(noinline) bool __fastcall secure_status(void* object) noexcept {
    const Predicate original = g_secureStatusOriginal.load(std::memory_order_acquire);
    const bool result = original != nullptr && original(object);
    const std::uintptr_t caller = caller_rva(_ReturnAddress());
    const std::uint32_t observation =
        g_secureStatusReports.fetch_add(1, std::memory_order_relaxed) + 1U;
    if (caller == kSecureStatusReturnRva && observation <= kMaximumReportsPerMethod) {
        report_predicate("status_20", observation, object, caller, result, result, false);
    }
    return result;
}

/** Records the desired secure state and lets the native method apply it unchanged. */
__declspec(noinline) void __fastcall request_secure(void* object, bool desired) noexcept {
    const std::uintptr_t caller = caller_rva(_ReturnAddress());
    const std::uint32_t observation =
        g_requestSecureReports.fetch_add(1, std::memory_order_relaxed) + 1U;
    if (caller == kRequestSecureReturnRva && observation <= kMaximumReportsPerMethod) {
        report_predicate("request_80", observation, object, caller, desired, desired, false);
    }
    const RequestSecure original = g_requestSecureOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(object, desired);
    }
}

/** Records the state-one request that starts native address security. */
__declspec(noinline) void __fastcall start_security(void* object, bool desired) noexcept {
    const std::uintptr_t caller = caller_rva(_ReturnAddress());
    const std::uint32_t observation =
        g_startSecurityReports.fetch_add(1, std::memory_order_relaxed) + 1U;
    if (caller == kStartSecurityReturnRva && observation <= kMaximumReportsPerMethod) {
        report_predicate("start_98",
                         observation,
                         object,
                         caller,
                         desired,
                         desired,
                         false);
    }
    const StartSecurity original = g_startSecurityOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(object, desired);
    }
}

/** Records the state-one commit/kick method that follows the start request. */
__declspec(noinline) void __fastcall commit_security(void* object) noexcept {
    const std::uintptr_t caller = caller_rva(_ReturnAddress());
    const std::uint32_t observation =
        g_commitSecurityReports.fetch_add(1, std::memory_order_relaxed) + 1U;
    if (caller == kCommitSecurityReturnRva && observation <= kMaximumReportsPerMethod) {
        report_predicate("commit_D0", observation, object, caller, true, true, false);
    }
    const CommitSecurity original = g_commitSecurityOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(object);
    }
}

/** Saves one runtime-decrypted vtable method for offline disassembly. */
void dump_method_code(const wchar_t* phase, const void* target) noexcept {
    constexpr std::size_t kCaptureSize = 0x400U;
    if (phase == nullptr || target == nullptr) {
        return;
    }
    const auto image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const auto begin = reinterpret_cast<std::uintptr_t>(target);
    if (image == 0U || begin < image) {
        return;
    }

    std::array<std::byte, kCaptureSize> snapshot{};
    __try {
        std::memcpy(snapshot.data(), target, snapshot.size());
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return;
    }

    const HMODULE sunrise = GetModuleHandleW(L"steam_api64.dll");
    core::path::Buffer path{};
    if (sunrise == nullptr || !core::path::artifact_directory(sunrise, path)
        || !core::path::append(path, L"\\analysis")) {
        return;
    }
    if (CreateDirectoryW(path.chars.data(), nullptr) == FALSE
        && GetLastError() != ERROR_ALREADY_EXISTS) {
        return;
    }
    std::array<wchar_t, 144> filename{};
    const int filenameLength = std::swprintf(
        filename.data(),
        filename.size(),
        L"\\activity_code.secure_channel_%ls.rva_%08llX.bin",
        phase,
        static_cast<unsigned long long>(begin - image));
    if (filenameLength <= 0 || !core::path::append(path, filename.data())) {
        return;
    }
    const HANDLE file = CreateFileW(path.chars.data(),
                                    GENERIC_WRITE,
                                    FILE_SHARE_READ,
                                    nullptr,
                                    CREATE_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL,
                                    nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD written = 0;
    const bool complete = WriteFile(file,
                                    snapshot.data(),
                                    static_cast<DWORD>(snapshot.size()),
                                    &written,
                                    nullptr)
                              != FALSE
                          && written == static_cast<DWORD>(snapshot.size())
                          && FlushFileBuffers(file) != FALSE;
    (void)CloseHandle(file);

    std::array<char, 256> line{};
    const int lineLength = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=secure_channel_method_code phase=%ls rva=0x%llX bytes=%lu result=%s",
        phase,
        static_cast<unsigned long long>(begin - image),
        static_cast<unsigned long>(written),
        complete ? "ok" : "write");
    if (lineLength > 0) {
        core::log::write(core::log::Channel::client,
                         complete ? core::log::Level::info : core::log::Level::warn,
                         {line.data(), static_cast<std::size_t>(lineLength)});
    }
}

/** Reads the three vtable methods from a native channel object under SEH. */
[[nodiscard]] bool resolve_methods(void* object,
                                   void*& nativeUpdate,
                                   void*& secureStatus,
                                   void*& channelGate,
                                   void*& requestSecure,
                                   void*& startSecurity,
                                   void*& commitSecurity,
                                   void**& vtable) noexcept {
    nativeUpdate = nullptr;
    secureStatus = nullptr;
    channelGate = nullptr;
    requestSecure = nullptr;
    startSecurity = nullptr;
    commitSecurity = nullptr;
    vtable = nullptr;
    if (object == nullptr) {
        return false;
    }
    __try {
        vtable = *static_cast<void***>(object);
        if (vtable != nullptr) {
            nativeUpdate = vtable[kNativeUpdateSlot];
            secureStatus = vtable[kSecureStatusSlot];
            channelGate = vtable[kChannelGateSlot];
            requestSecure = vtable[kRequestSecureSlot];
            startSecurity = vtable[kStartSecuritySlot];
            commitSecurity = vtable[kCommitSecuritySlot];
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        vtable = nullptr;
    }
    return vtable != nullptr && nativeUpdate != nullptr && secureStatus != nullptr
           && channelGate != nullptr
           && requestSecure != nullptr && secureStatus != channelGate
           && secureStatus != requestSecure && channelGate != requestSecure
           && startSecurity != nullptr && commitSecurity != nullptr;
}

} // namespace

/** Installs behavior-preserving observers on the three state-2 virtual methods. */
bool install_secure_channel_predicate_probe(void* channelObject) noexcept {
    if (g_installed.load(std::memory_order_acquire)) {
        return true;
    }
    AcquireSRWLockExclusive(&g_probeLock);
    if (g_installed.load(std::memory_order_relaxed)) {
        ReleaseSRWLockExclusive(&g_probeLock);
        return true;
    }

    void* nativeUpdateTarget = nullptr;
    void* secureStatus = nullptr;
    void* channelGate = nullptr;
    void* requestSecureTarget = nullptr;
    void* startSecurityTarget = nullptr;
    void* commitSecurityTarget = nullptr;
    void** vtable = nullptr;
    const bool resolved = resolve_methods(channelObject,
                                          nativeUpdateTarget,
                                          secureStatus,
                                          channelGate,
                                          requestSecureTarget,
                                          startSecurityTarget,
                                          commitSecurityTarget,
                                          vtable);
    if (resolved) {
        g_targetChannelObject.store(channelObject, std::memory_order_release);
        dump_method_code(L"v00_update", nativeUpdateTarget);
        dump_method_code(L"v20_status", secureStatus);
        dump_method_code(L"v30_ready", channelGate);
        dump_method_code(L"v80_select_path", requestSecureTarget);
        dump_method_code(L"v98_start", startSecurityTarget);
        dump_method_code(L"vD0_commit", commitSecurityTarget);
    }
    const std::array<hooking::detour::Spec, 6> specs{
        hooking::detour::Spec{nativeUpdateTarget, reinterpret_cast<void*>(&native_update)},
        hooking::detour::Spec{secureStatus, reinterpret_cast<void*>(&secure_status)},
        hooking::detour::Spec{channelGate, reinterpret_cast<void*>(&channel_gate)},
        hooking::detour::Spec{requestSecureTarget, reinterpret_cast<void*>(&request_secure)},
        hooking::detour::Spec{startSecurityTarget, reinterpret_cast<void*>(&start_security)},
        hooking::detour::Spec{commitSecurityTarget, reinterpret_cast<void*>(&commit_security)}};
    const bool installed = resolved && hooking::detour::install(specs, g_handles);
    if (installed) {
        g_nativeUpdateOriginal.store(reinterpret_cast<NativeUpdate>(g_handles[0].original),
                                     std::memory_order_release);
        g_secureStatusOriginal.store(reinterpret_cast<Predicate>(g_handles[1].original),
                                     std::memory_order_release);
        g_channelGateOriginal.store(reinterpret_cast<Predicate>(g_handles[2].original),
                                    std::memory_order_release);
        g_requestSecureOriginal.store(reinterpret_cast<RequestSecure>(g_handles[3].original),
                                      std::memory_order_release);
        g_startSecurityOriginal.store(reinterpret_cast<StartSecurity>(g_handles[4].original),
                                      std::memory_order_release);
        g_commitSecurityOriginal.store(reinterpret_cast<CommitSecurity>(g_handles[5].original),
                                       std::memory_order_release);
        g_installed.store(true, std::memory_order_release);
    }
    else {
        g_targetChannelObject.store(nullptr, std::memory_order_release);
    }
    ReleaseSRWLockExclusive(&g_probeLock);

    const auto image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    std::array<char, 512> line{};
    const int written = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=secure_channel_predicate_probe result=%s object=%p vtable=%p "
        "method00_rva=0x%llX method20_rva=0x%llX method30_rva=0x%llX method80_rva=0x%llX "
        "method98_rva=0x%llX methodD0_rva=0x%llX mode=observe_only",
        installed ? "install_ok" : resolved ? "install_fail" : "resolve_fail",
        channelObject,
        static_cast<void*>(vtable),
        static_cast<unsigned long long>(image != 0
                                            && reinterpret_cast<std::uintptr_t>(nativeUpdateTarget)
                                                   >= image
                                                ? reinterpret_cast<std::uintptr_t>(nativeUpdateTarget)
                                                      - image
                                                : 0U),
        static_cast<unsigned long long>(image != 0
                                            && reinterpret_cast<std::uintptr_t>(secureStatus) >= image
                                                ? reinterpret_cast<std::uintptr_t>(secureStatus) - image
                                                : 0U),
        static_cast<unsigned long long>(image != 0
                                            && reinterpret_cast<std::uintptr_t>(channelGate) >= image
                                                ? reinterpret_cast<std::uintptr_t>(channelGate) - image
                                                : 0U),
        static_cast<unsigned long long>(image != 0
                                            && reinterpret_cast<std::uintptr_t>(requestSecureTarget)
                                                   >= image
                                                ? reinterpret_cast<std::uintptr_t>(requestSecureTarget)
                                                      - image
                                                : 0U),
        static_cast<unsigned long long>(image != 0
                                            && reinterpret_cast<std::uintptr_t>(startSecurityTarget)
                                                   >= image
                                                ? reinterpret_cast<std::uintptr_t>(startSecurityTarget)
                                                      - image
                                                : 0U),
        static_cast<unsigned long long>(image != 0
                                            && reinterpret_cast<std::uintptr_t>(commitSecurityTarget)
                                                   >= image
                                                ? reinterpret_cast<std::uintptr_t>(commitSecurityTarget)
                                                      - image
                                                : 0U));
    if (written > 0) {
        core::log::write(core::log::Channel::client,
                         installed ? core::log::Level::info : core::log::Level::warn,
                         {line.data(), static_cast<std::size_t>(written)});
    }
    return installed;
}

/** Detaches all three dynamic observers. */
void uninstall_secure_channel_predicate_probe() noexcept {
    AcquireSRWLockExclusive(&g_probeLock);
    if (g_installed.load(std::memory_order_relaxed)) {
        const std::array<hooking::detour::ProtectedCodeEntry, 6> protectedEntries{
            hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&native_update)},
            hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&secure_status)},
            hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&channel_gate)},
            hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&request_secure)},
            hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&start_security)},
            hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&commit_security)}};
        if (hooking::detour::uninstall(g_handles, protectedEntries)
            == hooking::detour::UninstallResult::removed) {
            g_installed.store(false, std::memory_order_release);
            g_nativeUpdateOriginal.store(nullptr, std::memory_order_release);
            g_secureStatusOriginal.store(nullptr, std::memory_order_release);
            g_channelGateOriginal.store(nullptr, std::memory_order_release);
            g_requestSecureOriginal.store(nullptr, std::memory_order_release);
            g_startSecurityOriginal.store(nullptr, std::memory_order_release);
            g_commitSecurityOriginal.store(nullptr, std::memory_order_release);
            g_targetChannelObject.store(nullptr, std::memory_order_release);
        }
    }
    ReleaseSRWLockExclusive(&g_probeLock);
}

} // namespace sunrise::client::hooks::bootflow
