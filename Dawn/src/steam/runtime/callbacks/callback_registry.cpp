#include "callback_registry.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string_view>

#include "../../../core/logging/log.h"

namespace dawn::steam::runtime::callbacks {
namespace {

/** In Steam's callback base, the flags follow the vtable pointer. */
constexpr std::size_t kCallbackFlagsOffset = sizeof(void*);
/** The callback id follows the pointer and the 32-bit flags field. */
constexpr std::size_t kCallbackIdOffset = sizeof(void*) + sizeof(std::uint32_t);
/** Steam's flag bit that marks an object as registered. */
constexpr std::uint8_t kRegisteredFlag = 0x01;
/** The third CCallbackBase virtual method reports the callback payload size. */
constexpr std::size_t kCallbackSizeSlot = 2;
/** Call-result observations are diagnostic and bounded independently of normal callback traffic. */
constexpr unsigned kMaximumCallResultReports = 32;

std::atomic<unsigned> g_callResultReports{0};

struct CallbackMetadata {
    int callbackId{};
    int payloadSize{-1};
    std::uintptr_t vtable{};
    std::uintptr_t runMethod{};
    std::uintptr_t sizeMethod{};
    bool readable{};
};

/** Reads the private callback ABI behind SEH so an unexpected object cannot take down the Client. */
CallbackMetadata inspect_callback(void* callback) noexcept {
    CallbackMetadata result{};
    __try {
        auto** const methods = *static_cast<void***>(callback);
        if (methods == nullptr || methods[kCallbackSizeSlot] == nullptr) {
            return result;
        }
        result.callbackId = callback_id(callback);
        result.vtable = reinterpret_cast<std::uintptr_t>(methods);
        result.runMethod = reinterpret_cast<std::uintptr_t>(methods[0]);
        result.sizeMethod = reinterpret_cast<std::uintptr_t>(methods[kCallbackSizeSlot]);
        const auto size = reinterpret_cast<int (*)(void*)>(methods[kCallbackSizeSlot]);
        result.payloadSize = size(callback);
        result.readable = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        result = {};
        result.payloadSize = -1;
    }
    return result;
}

/** Emits the call-result contract selected by the Client for an async Steam call. */
void report_call_result(void* callback, ApiCall call) noexcept {
    if (g_callResultReports.fetch_add(1, std::memory_order_relaxed)
        >= kMaximumCallResultReports) {
        return;
    }
    const CallbackMetadata metadata = inspect_callback(callback);
    const auto imageBase = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const std::uintptr_t vtableRva =
        metadata.vtable >= imageBase ? metadata.vtable - imageBase : metadata.vtable;
    HMODULE callbackModule{};
    std::array<wchar_t, MAX_PATH> callbackModulePath{};
    if (metadata.runMethod != 0
        && GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                                 | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                             reinterpret_cast<LPCWSTR>(metadata.runMethod),
                             &callbackModule)) {
        (void)GetModuleFileNameW(
            callbackModule, callbackModulePath.data(), static_cast<DWORD>(callbackModulePath.size()));
    }
    const auto callbackModuleBase = reinterpret_cast<std::uintptr_t>(callbackModule);
    const std::uintptr_t runMethodRva = metadata.runMethod >= callbackModuleBase
                                            ? metadata.runMethod - callbackModuleBase
                                            : metadata.runMethod;
    const std::uintptr_t sizeMethodRva = metadata.sizeMethod >= callbackModuleBase
                                             ? metadata.sizeMethod - callbackModuleBase
                                             : metadata.sizeMethod;
    std::array<char, 640> line{};
    const int written = std::snprintf(
        line.data(),
        line.size(),
        "ev=steam_networking stage=call_result_register call=0x%016llX callback_id=%d "
        "payload_size=%d callback=%p vtable=+0x%llX run_method=+0x%llX "
        "size_method=+0x%llX module=%ls readable=%u",
        static_cast<unsigned long long>(call),
        metadata.callbackId,
        metadata.payloadSize,
        callback,
        static_cast<unsigned long long>(vtableRva),
        static_cast<unsigned long long>(runMethodRva),
        static_cast<unsigned long long>(sizeMethodRva),
        callbackModulePath.data(),
        metadata.readable ? 1U : 0U);
    if (written > 0) {
        const std::size_t length = static_cast<std::size_t>(written) < line.size()
                                       ? static_cast<std::size_t>(written)
                                       : line.size() - 1;
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         std::string_view(line.data(), length));
    }
}

/** Stores a callback id in Steam-owned callback storage. */
void set_callback_id(void* callback, int callbackId) noexcept {
    auto* bytes = static_cast<std::byte*>(callback);
    std::memcpy(bytes + kCallbackIdOffset, &callbackId, sizeof(callbackId));
}

} // namespace

SRWLOCK g_lock{SRWLOCK_INIT};
std::array<CallbackEntry, kCallbackCapacity> g_callbacks{};
std::array<CallResultEntry, kCallResultCapacity> g_callResults{};
std::array<CallbackEvent, kEventCapacity> g_events{};
std::size_t g_eventHead{};
std::size_t g_eventCount{};

/** Updates the registered bit in Steam-owned callback storage. */
void set_registration(void* callback, bool registered) noexcept {
    if (callback == nullptr) {
        return;
    }
    auto* bytes = static_cast<std::byte*>(callback);
    std::uint8_t flags{};
    std::memcpy(&flags, bytes + kCallbackFlagsOffset, sizeof(flags));
    flags = registered ? static_cast<std::uint8_t>(flags | kRegisteredFlag)
                       : static_cast<std::uint8_t>(flags & ~kRegisteredFlag);
    std::memcpy(bytes + kCallbackFlagsOffset, &flags, sizeof(flags));
}

/** Reads a callback id from Steam-owned callback storage. */
int callback_id(void* callback) noexcept {
    int callbackId{};
    const auto* bytes = static_cast<const std::byte*>(callback);
    std::memcpy(&callbackId, bytes + kCallbackIdOffset, sizeof(callbackId));
    return callbackId;
}

} // namespace dawn::steam::runtime::callbacks

namespace dawn::steam {

/** Registers one Steam-owned callback object for a callback id. */
void register_callback(void* callback, int callbackId) noexcept {
    using namespace runtime::callbacks;
    if (callback == nullptr) {
        return;
    }
    AcquireSRWLockExclusive(&g_lock);
    CallbackEntry* freeEntry = nullptr;
    for (auto& entry : g_callbacks) {
        if (entry.callback == callback) {
            entry.callbackId = callbackId;
            set_callback_id(callback, callbackId);
            set_registration(callback, true);
            ReleaseSRWLockExclusive(&g_lock);
            return;
        }
        if (freeEntry == nullptr && entry.callback == nullptr) {
            freeEntry = &entry;
        }
    }
    if (freeEntry != nullptr) {
        *freeEntry = CallbackEntry{callback, callbackId};
        set_callback_id(callback, callbackId);
        set_registration(callback, true);
    }
    ReleaseSRWLockExclusive(&g_lock);
    if (freeEntry == nullptr) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::error,
                         "ev=callback_register result=full");
    }
}

/** @param callback Steam-owned object to remove from every registration. */
void unregister_callback(void* callback) noexcept {
    using namespace runtime::callbacks;
    if (callback == nullptr) {
        return;
    }
    AcquireSRWLockExclusive(&g_lock);
    for (auto& entry : g_callbacks) {
        if (entry.callback == callback) {
            entry = {};
        }
    }
    set_registration(callback, false);
    ReleaseSRWLockExclusive(&g_lock);
}

/** Registers one Steam-owned call-result object for a nonzero API call. */
void register_call_result(void* callback, ApiCall call) noexcept {
    using namespace runtime::callbacks;
    if (callback == nullptr || call == 0) {
        return;
    }
    report_call_result(callback, call);
    AcquireSRWLockExclusive(&g_lock);
    CallResultEntry* freeEntry = nullptr;
    for (auto& entry : g_callResults) {
        if (entry.callback == callback && entry.call == call) {
            ReleaseSRWLockExclusive(&g_lock);
            return;
        }
        if (freeEntry == nullptr && entry.callback == nullptr) {
            freeEntry = &entry;
        }
    }
    if (freeEntry != nullptr) {
        *freeEntry = CallResultEntry{callback, call};
    }
    ReleaseSRWLockExclusive(&g_lock);
    if (freeEntry == nullptr) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::error,
                         "ev=call_result_register result=full");
    }
}

/** Removes an API call-result registration. */
void unregister_call_result(void* callback, ApiCall call) noexcept {
    using namespace runtime::callbacks;
    if (callback == nullptr) {
        return;
    }
    AcquireSRWLockExclusive(&g_lock);
    for (auto& entry : g_callResults) {
        if (entry.callback == callback && (call == 0 || entry.call == call)) {
            entry = {};
        }
    }
    ReleaseSRWLockExclusive(&g_lock);
}

namespace runtime::callbacks {

/** Clears every callback, call-result and queued-event registration. */
void clear() noexcept {
    AcquireSRWLockExclusive(&g_lock);
    for (auto& entry : g_callbacks) {
        if (entry.callback != nullptr) {
            set_registration(entry.callback, false);
        }
        entry = {};
    }
    g_callResults.fill({});
    g_events.fill({});
    g_eventHead = 0;
    g_eventCount = 0;
    ReleaseSRWLockExclusive(&g_lock);
}

} // namespace runtime::callbacks
} // namespace dawn::steam
