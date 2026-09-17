#include "native_activation_observer.h"

#include <Windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <span>

#include "../../../core/logging/log.h"
#include "../../diagnostics/module_range.h"
#include "../../hooking/call_gate.h"
#include "../../hooking/detour.h"
#include "native_activation_batch.h"
#include "native_activation_contract.h"
#include "native_activation_event_queue.h"
#include "native_activation_flow.h"
#include "native_activation_global_drop_fanout.h"
#include "native_activation_validation.h"

#pragma comment(lib, "bcrypt.lib")

namespace dawn::client::hooks::activity_lifecycle {
namespace {

constexpr std::size_t kModulePathCapacity = 32768U;
constexpr std::size_t kHashReadBufferSize = 64U * 1024U;
constexpr std::size_t kWrapperActiveOffset = 0x20U;
constexpr std::size_t kWrapperFullHandleOffset = 0x24U;

enum class HookSlot : std::size_t {
    activate,
    close,
    reinstantiate,
    cleanup,
    globalDrop,
};

using NativeActivityActivate = std::uint8_t(__fastcall*)(void* wrapper,
                                                          void* diagnosticCallback,
                                                          std::uint32_t nativeHandle,
                                                          const std::uint64_t* identity,
                                                          std::uint8_t validateHost,
                                                          std::int32_t mode);
using NativeActivityClose = void(__fastcall*)(void* wrapper);
using NativeActivityGlobalDrop = void(__fastcall*)();

SRWLOCK g_lifecycleLock{SRWLOCK_INIT};
NativeActivationBatchState g_batch{};
std::array<hooking::CallGate, contract::kHookCount> g_callGates{};
std::array<hooking::detour::Handle, contract::kHookCount> g_handles{};
std::atomic<NativeActivityActivate> g_activateOriginal{};
std::atomic<NativeActivityClose> g_closeOriginal{};
std::atomic<NativeActivityClose> g_reinstantiateOriginal{};
std::atomic<NativeActivityClose> g_cleanupOriginal{};
std::atomic<NativeActivityGlobalDrop> g_globalDropOriginal{};
state::activity::ModuleGeneration g_ownedModule{};

[[nodiscard]] constexpr std::size_t index(HookSlot slot) noexcept {
    return static_cast<std::size_t>(slot);
}

static_assert(index(HookSlot::globalDrop) + 1U == contract::kHookCount);
static_assert(kNativeActivationSlotCount
              <= static_cast<std::size_t>((std::numeric_limits<std::uint32_t>::max)()));

[[nodiscard]] bool cng_succeeded(NTSTATUS status) noexcept {
    return status >= 0;
}

[[nodiscard]] bool current_packed_executable_sha256(HMODULE module,
                                                    ImageSha256& output) noexcept {
    output = {};
    std::array<wchar_t, kModulePathCapacity> path{};
    const DWORD copied =
        GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
    if (copied == 0U || static_cast<std::size_t>(copied) >= path.size()) {
        return false;
    }

    const HANDLE file = CreateFileW(path.data(),
                                    GENERIC_READ,
                                    FILE_SHARE_READ | FILE_SHARE_DELETE,
                                    nullptr,
                                    OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                                    nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    BCRYPT_ALG_HANDLE algorithm{};
    BCRYPT_HASH_HANDLE hash{};
    bool complete = cng_succeeded(
        BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0));
    if (complete) {
        complete = cng_succeeded(BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0));
    }
    std::array<std::byte, kHashReadBufferSize> buffer{};
    while (complete) {
        DWORD transferred{};
        if (ReadFile(file,
                     buffer.data(),
                     static_cast<DWORD>(buffer.size()),
                     &transferred,
                     nullptr)
            == FALSE) {
            complete = false;
            break;
        }
        if (transferred == 0U) {
            break;
        }
        complete = cng_succeeded(BCryptHashData(hash,
                                                reinterpret_cast<PUCHAR>(buffer.data()),
                                                transferred,
                                                0));
    }
    if (complete) {
        complete = cng_succeeded(
            BCryptFinishHash(hash,
                             reinterpret_cast<PUCHAR>(output.data()),
                             static_cast<ULONG>(output.size()),
                             0));
    }
    if (hash != nullptr) {
        (void)BCryptDestroyHash(hash);
    }
    if (algorithm != nullptr) {
        (void)BCryptCloseAlgorithmProvider(algorithm, 0);
    }
    complete = CloseHandle(file) != FALSE && complete;
    if (!complete) {
        output = {};
    }
    return complete;
}

template <class Value>
[[nodiscard]] bool guarded_copy(Value& output, const void* source) noexcept {
    if (source == nullptr) {
        return false;
    }
#if defined(_MSC_VER)
    __try {
        std::memcpy(&output, source, sizeof output);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#else
    (void)output;
    (void)source;
    return false;
#endif
}

template <class Value>
[[nodiscard]] bool guarded_wrapper_field(void* wrapper,
                                         std::size_t offset,
                                         Value& output) noexcept {
    const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(wrapper);
    if (base == 0U || offset > (std::numeric_limits<std::uintptr_t>::max)() - base) {
        return false;
    }
    return guarded_copy(output, reinterpret_cast<const void*>(base + offset));
}

[[nodiscard]] bool native_wrapper_active(void* wrapper) noexcept {
    std::uint8_t active{};
    return guarded_wrapper_field(wrapper, kWrapperActiveOffset, active) && active != 0U;
}

struct NativeCloseEntry final {
    std::uint32_t fullHandle{kInvalidNativeActivityHandle};
    bool active{};
    bool readable{};
};

[[nodiscard]] NativeCloseEntry capture_native_close_entry(void* wrapper) noexcept {
    NativeCloseEntry entry{};
    std::uint8_t active{};
    if (!guarded_wrapper_field(wrapper, kWrapperActiveOffset, active)) {
        return entry;
    }
    entry.readable = true;
    entry.active = active != 0U;
    if (entry.active
        && !guarded_wrapper_field(wrapper, kWrapperFullHandleOffset, entry.fullHandle)) {
        entry.readable = false;
    }
    return entry;
}

__declspec(noinline) std::uint8_t __fastcall activity_activate(
    void* wrapper,
    void* diagnosticCallback,
    std::uint32_t nativeHandle,
    const std::uint64_t* identity,
    std::uint8_t validateHost,
    std::int32_t mode) noexcept {
    hooking::CallGate::Scope call(g_callGates[index(HookSlot::activate)]);
    const NativeActivityActivate original = hooking::await_original(g_activateOriginal);

    if (!call.accepts_side_effects()) {
        return original(wrapper,
                        diagnosticCallback,
                        nativeHandle,
                        identity,
                        validateHost,
                        mode);
    }

    NativeActivationAttempt attempt{};
    attempt.wrapper = reinterpret_cast<std::uintptr_t>(wrapper);
    attempt.fullHandle = nativeHandle;
    attempt.mode = mode;
    attempt.validateHost = validateHost;
    attempt.identityValid = guarded_copy(attempt.identity, identity);

    const ActivationFlowResult result = observe_activation(
        detail::native_activation_registry(),
        attempt,
        [&]() noexcept {
            return original(wrapper,
                            diagnosticCallback,
                            nativeHandle,
                            identity,
                            validateHost,
                            mode);
        },
        [&]() noexcept { return native_wrapper_active(wrapper); },
        [&]() noexcept { return call.accepts_side_effects(); });
    if (result.postWorkAdmitted) {
        (void)detail::enqueue_native_activation_event(
            detail::make_native_activation_event(attempt, result));
    }
    return result.nativeResult;
}

void run_close(void* wrapper,
               NativeActivityClose original,
               NativeActivationClosePath path,
               const hooking::CallGate::Scope& call) noexcept {
    if (!call.accepts_side_effects()) {
        original(wrapper);
        return;
    }

    const NativeCloseEntry entry = capture_native_close_entry(wrapper);
    const CloseFlowResult result = observe_close(
        detail::native_activation_registry(),
        reinterpret_cast<std::uintptr_t>(wrapper),
        entry.fullHandle,
        entry.readable && entry.active,
        [&]() noexcept { original(wrapper); },
        [&]() noexcept { return call.accepts_side_effects(); });
    if (result.postWorkAdmitted && entry.readable && entry.active) {
        (void)detail::enqueue_native_activation_event(
            detail::make_native_activation_close_event(
                path,
                reinterpret_cast<std::uintptr_t>(wrapper),
                entry.fullHandle,
                result));
    }
}

__declspec(noinline) void __fastcall activity_close(void* wrapper) noexcept {
    hooking::CallGate::Scope call(g_callGates[index(HookSlot::close)]);
    const NativeActivityClose original = hooking::await_original(g_closeOriginal);
    run_close(wrapper, original, NativeActivationClosePath::ordinary, call);
}

__declspec(noinline) void __fastcall activity_reinstantiate(void* wrapper) noexcept {
    hooking::CallGate::Scope call(g_callGates[index(HookSlot::reinstantiate)]);
    const NativeActivityClose original = hooking::await_original(g_reinstantiateOriginal);
    run_close(wrapper, original, NativeActivationClosePath::reinstantiate, call);
}

__declspec(noinline) void __fastcall activity_cleanup(void* wrapper) noexcept {
    hooking::CallGate::Scope call(g_callGates[index(HookSlot::cleanup)]);
    const NativeActivityClose original = hooking::await_original(g_cleanupOriginal);
    run_close(wrapper, original, NativeActivationClosePath::cleanup, call);
}

__declspec(noinline) void __fastcall activity_global_drop() noexcept {
    hooking::CallGate::Scope call(g_callGates[index(HookSlot::globalDrop)]);
    const NativeActivityGlobalDrop original = hooking::await_original(g_globalDropOriginal);
    if (!call.accepts_side_effects()) {
        original();
        return;
    }

    NativeActivationGlobalDropCohort cohort{};
    NativeActivationGlobalDropFanout::Dispatch dispatch{};
    (void)observe_global_drop(
        detail::native_activation_registry(),
        [&]() noexcept { original(); },
        [&]() noexcept { return call.accepts_side_effects(); },
        [&](const GlobalDropBeginResult& capture) noexcept {
            cohort = NativeActivationGlobalDropCohort{
                capture.token.module.value,
                capture.token.epoch,
                static_cast<std::uint32_t>(capture.token.captured),
                static_cast<bool>(capture.token),
            };
            dispatch = detail::native_activation_global_drop_fanout().acquire();
            detail::notify_native_activation_global_drop_pre(dispatch, cohort);
        },
        [&](const GlobalDropBeginResult&) noexcept {
            detail::notify_native_activation_global_drop_post(dispatch, cohort);
            dispatch.finish();
        });
}

[[nodiscard]] bool hook_calls_idle() noexcept {
    return std::all_of(g_callGates.begin(), g_callGates.end(), [](const hooking::CallGate& gate) {
        return gate.idle();
    });
}

[[nodiscard]] bool detach_idle() noexcept {
    return hook_calls_idle() && detail::native_activation_global_drop_fanout().idle()
           && !core::log::writers_active();
}

void quiesce_gates() noexcept {
    for (hooking::CallGate& gate : g_callGates) {
        gate.quiesce();
    }
}

void accept_gates() noexcept {
    for (hooking::CallGate& gate : g_callGates) {
        gate.accept();
    }
}

[[nodiscard]] bool any_handle_attached() noexcept {
    return std::any_of(g_handles.begin(), g_handles.end(), [](const hooking::detour::Handle& handle) {
        return handle.attached;
    });
}

[[nodiscard]] bool all_handles_attached() noexcept {
    return std::all_of(g_handles.begin(), g_handles.end(), [](const hooking::detour::Handle& handle) {
        return handle.attached;
    });
}

[[nodiscard]] const char* validation_name(NativeActivationValidationResult result) noexcept {
    switch (result) {
    case NativeActivationValidationResult::valid:
        return "ok";
    case NativeActivationValidationResult::invalidArguments:
        return "image";
    case NativeActivationValidationResult::imageHashMismatch:
        return "image_hash";
    case NativeActivationValidationResult::targetOutOfRange:
        return "target_bounds";
    case NativeActivationValidationResult::prefixMismatch:
        return "prefix";
    default:
        return "unknown";
    }
}

} // namespace

bool install() noexcept {
    NativeActivationValidationResult validation =
        NativeActivationValidationResult::invalidArguments;
    const char* failure = nullptr;
    bool retainedFailure = false;

    AcquireSRWLockExclusive(&g_lifecycleLock);
    if (g_batch.snapshot().phase == NativeActivationBatchPhase::running
        && all_handles_attached()) {
        ReleaseSRWLockExclusive(&g_lifecycleLock);
        return true;
    }
    quiesce_gates();
    detail::native_activation_global_drop_fanout().quiesce();
    if (g_batch.snapshot().phase != NativeActivationBatchPhase::detached
        || any_handle_attached() || !hook_calls_idle()
        || g_activateOriginal.load(std::memory_order_acquire) != nullptr
        || g_closeOriginal.load(std::memory_order_acquire) != nullptr
        || g_reinstantiateOriginal.load(std::memory_order_acquire) != nullptr
        || g_cleanupOriginal.load(std::memory_order_acquire) != nullptr
        || g_globalDropOriginal.load(std::memory_order_acquire) != nullptr) {
        failure = "ownership";
    }

    std::array<std::uintptr_t, contract::kHookCount> targetAddresses{};
    if (failure == nullptr) {
        const HMODULE module = GetModuleHandleW(nullptr);
        diagnostics::ModuleRange range{};
        ImageSha256 packedOnDiskSha256{};
        if (!diagnostics::module_range(module, range) || range.end <= range.base
            || !current_packed_executable_sha256(module, packedOnDiskSha256)) {
            failure = "image";
        } else {
            const NativeActivationImageView image{
                reinterpret_cast<const std::byte*>(range.base),
                static_cast<std::size_t>(range.end - range.base),
                packedOnDiskSha256,
            };
            const std::array contracts{
                NativeActivationTargetContract{contract::kActivityActivateRva,
                                               contract::kActivityActivatePrefix},
                NativeActivationTargetContract{contract::kActivityCloseRva,
                                               contract::kActivityClosePrefix},
                NativeActivationTargetContract{contract::kActivityReinstantiateRva,
                                               contract::kActivityReinstantiatePrefix},
                NativeActivationTargetContract{contract::kActivityCleanupRva,
                                               contract::kActivityCleanupPrefix},
                NativeActivationTargetContract{contract::kActivityGlobalDropRva,
                                               contract::kActivityGlobalDropPrefix},
            };
            validation = validate_native_activation_image(
                image,
                contract::kPinnedInstalledPackedImageSha256,
                contracts,
                targetAddresses);
            if (validation != NativeActivationValidationResult::valid) {
                failure = validation_name(validation);
            }
        }
    }

    BeginModuleResult moduleResult{};
    if (failure == nullptr) {
        moduleResult = detail::native_activation_registry().begin_module();
        if (moduleResult.status != BeginModuleStatus::started) {
            failure = "module_generation";
        } else {
            g_ownedModule = moduleResult.generation;
        }
    }

    if (failure == nullptr) {
        if (!g_batch.begin_install()) {
            (void)detail::native_activation_registry().finish_module(g_ownedModule);
            g_ownedModule = {};
            failure = "ownership";
        }
    }

    if (failure == nullptr) {
        const std::array specs{
            hooking::detour::Spec{reinterpret_cast<void*>(targetAddresses[index(HookSlot::activate)]),
                                  reinterpret_cast<void*>(&activity_activate)},
            hooking::detour::Spec{reinterpret_cast<void*>(targetAddresses[index(HookSlot::close)]),
                                  reinterpret_cast<void*>(&activity_close)},
            hooking::detour::Spec{
                reinterpret_cast<void*>(targetAddresses[index(HookSlot::reinstantiate)]),
                reinterpret_cast<void*>(&activity_reinstantiate)},
            hooking::detour::Spec{reinterpret_cast<void*>(targetAddresses[index(HookSlot::cleanup)]),
                                  reinterpret_cast<void*>(&activity_cleanup)},
            hooking::detour::Spec{
                reinterpret_cast<void*>(targetAddresses[index(HookSlot::globalDrop)]),
                reinterpret_cast<void*>(&activity_global_drop)},
        };
        if (!hooking::detour::install(specs, g_handles)) {
            (void)g_batch.rollback_install();
            (void)detail::native_activation_registry().finish_module(g_ownedModule);
            g_ownedModule = {};
            failure = "attach";
        }
    }

    if (failure == nullptr) {
        const std::array<std::uintptr_t, contract::kHookCount> originals{
            reinterpret_cast<std::uintptr_t>(g_handles[index(HookSlot::activate)].original),
            reinterpret_cast<std::uintptr_t>(g_handles[index(HookSlot::close)].original),
            reinterpret_cast<std::uintptr_t>(g_handles[index(HookSlot::reinstantiate)].original),
            reinterpret_cast<std::uintptr_t>(g_handles[index(HookSlot::cleanup)].original),
            reinterpret_cast<std::uintptr_t>(g_handles[index(HookSlot::globalDrop)].original),
        };
        hooking::publish_original(
            g_activateOriginal,
            reinterpret_cast<NativeActivityActivate>(g_handles[index(HookSlot::activate)].original));
        hooking::publish_original(
            g_closeOriginal,
            reinterpret_cast<NativeActivityClose>(g_handles[index(HookSlot::close)].original));
        hooking::publish_original(
            g_reinstantiateOriginal,
            reinterpret_cast<NativeActivityClose>(
                g_handles[index(HookSlot::reinstantiate)].original));
        hooking::publish_original(
            g_cleanupOriginal,
            reinterpret_cast<NativeActivityClose>(g_handles[index(HookSlot::cleanup)].original));
        hooking::publish_original(
            g_globalDropOriginal,
            reinterpret_cast<NativeActivityGlobalDrop>(
                g_handles[index(HookSlot::globalDrop)].original));
        if (!g_batch.complete_install(originals)) {
            // Detours guarantees a complete non-null output set after a successful batch commit.
            // Published trampolines keep native forwarding live while teardown retains the batch.
            (void)g_batch.retain_install_failure(originals);
            (void)detail::native_activation_registry().quiesce_module(g_ownedModule);
            failure = "publication";
            retainedFailure = true;
        }
    }

    if (failure == nullptr) {
        detail::native_activation_global_drop_fanout().accept();
        accept_gates();
    }
    ReleaseSRWLockExclusive(&g_lifecycleLock);

    if (failure != nullptr) {
        std::array<char, 256U> line{};
        const int written = std::snprintf(
            line.data(),
            line.size(),
            "ev=native_activation v=1 stage=install result=fail reason=%s retained=%u",
            failure,
            retainedFailure ? 1U : 0U);
        if (written > 0) {
            core::log::write(
                core::log::Channel::client,
                core::log::Level::warn,
                {line.data(),
                 (std::min)(static_cast<std::size_t>(written), line.size() - 1U)});
        }
        return false;
    }

    core::log::write(
        core::log::Channel::client,
        core::log::Level::info,
        "ev=native_activation v=1 stage=install result=ok mode=observe_only hooks=5 "
        "targets=3CDB80,3CDB20,3CDDC0,3CA680,3C8EB0 correlation=unknown "
        "native_mutation=none");
    return true;
}

void quiesce_before_consumers() noexcept {
    bool changed = false;
    AcquireSRWLockExclusive(&g_lifecycleLock);
    detail::native_activation_global_drop_fanout().quiesce();
    if (g_batch.snapshot().phase == NativeActivationBatchPhase::running) {
        quiesce_gates();
        (void)detail::native_activation_registry().quiesce_module(g_ownedModule);
        changed = g_batch.quiesce();
    }
    ReleaseSRWLockExclusive(&g_lifecycleLock);
    if (changed) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         "ev=native_activation v=1 stage=quiesce result=ok retained=1");
    }
}

ObserverUninstallResult uninstall_after_consumers() noexcept {
    ObserverUninstallResult output = ObserverUninstallResult::failed;
    const char* outcome = "fail";
    AcquireSRWLockExclusive(&g_lifecycleLock);
    if (g_batch.snapshot().phase == NativeActivationBatchPhase::detached
        && !any_handle_attached()) {
        if (detail::native_activation_global_drop_fanout().clear_after_removed()) {
            output = ObserverUninstallResult::removed;
            outcome = "removed";
        } else {
            output = ObserverUninstallResult::failed;
            outcome = "fanout_active";
        }
    } else if (g_batch.snapshot().phase != NativeActivationBatchPhase::quiescing
               || !all_handles_attached()) {
        output = ObserverUninstallResult::failed;
        outcome = "wrong_phase";
    } else {
        const std::array protectedEntries{
            hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&activity_activate)},
            hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&activity_close)},
            hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&activity_reinstantiate)},
            hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&activity_cleanup)},
            hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&activity_global_drop)},
            hooking::detour::ProtectedCodeEntry{
                reinterpret_cast<void*>(&hooking::call_gate_detail::enter)},
            hooking::detour::ProtectedCodeEntry{
                reinterpret_cast<void*>(&hooking::call_gate_detail::leave)},
        };
        const hooking::detour::UninstallResult result =
            hooking::detour::uninstall(g_handles, protectedEntries, &detach_idle);
        if (result == hooking::detour::UninstallResult::removed) {
            (void)g_batch.record_detach(NativeActivationDetachDisposition::removed);
            g_activateOriginal.store(nullptr, std::memory_order_release);
            g_closeOriginal.store(nullptr, std::memory_order_release);
            g_reinstantiateOriginal.store(nullptr, std::memory_order_release);
            g_cleanupOriginal.store(nullptr, std::memory_order_release);
            g_globalDropOriginal.store(nullptr, std::memory_order_release);
            (void)detail::native_activation_registry().finish_module(g_ownedModule);
            g_ownedModule = {};
            if (detail::native_activation_global_drop_fanout().clear_after_removed()) {
                output = ObserverUninstallResult::removed;
                outcome = "removed";
            } else {
                output = ObserverUninstallResult::failed;
                outcome = "fanout_active";
            }
        } else if (result == hooking::detour::UninstallResult::protectedCodeActive) {
            (void)g_batch.record_detach(NativeActivationDetachDisposition::deferred);
            output = ObserverUninstallResult::deferred;
            outcome = "deferred";
        } else {
            (void)g_batch.record_detach(NativeActivationDetachDisposition::failed);
            output = ObserverUninstallResult::failed;
            outcome = "fail";
        }
    }
    ReleaseSRWLockExclusive(&g_lifecycleLock);

    std::array<char, 192U> line{};
    const int written = std::snprintf(line.data(),
                                      line.size(),
                                      "ev=native_activation v=1 stage=uninstall result=%s retained=%u",
                                      outcome,
                                      output == ObserverUninstallResult::removed ? 0U : 1U);
    if (written > 0) {
        core::log::write(
            core::log::Channel::client,
            output == ObserverUninstallResult::removed ? core::log::Level::info
                                                       : core::log::Level::warn,
            {line.data(), (std::min)(static_cast<std::size_t>(written), line.size() - 1U)});
    }
    return output;
}

bool has_ownership() noexcept {
    AcquireSRWLockShared(&g_lifecycleLock);
    const bool owned =
        g_batch.snapshot().phase != NativeActivationBatchPhase::detached
        || any_handle_attached() || !hook_calls_idle()
                       || detail::native_activation_global_drop_fanout().has_registration()
                       || g_activateOriginal.load(std::memory_order_acquire) != nullptr
                       || g_closeOriginal.load(std::memory_order_acquire) != nullptr
                       || g_reinstantiateOriginal.load(std::memory_order_acquire) != nullptr
                       || g_cleanupOriginal.load(std::memory_order_acquire) != nullptr
                       || g_globalDropOriginal.load(std::memory_order_acquire) != nullptr;
    ReleaseSRWLockShared(&g_lifecycleLock);
    return owned;
}

NativeActivationSnapshot observe(std::uintptr_t wrapper, std::uint32_t fullHandle) noexcept {
    return detail::native_activation_registry().snapshot(wrapper, fullHandle);
}

bool is_current(NativeActivationToken token) noexcept {
    return detail::native_activation_registry().is_current(token);
}

} // namespace dawn::client::hooks::activity_lifecycle
