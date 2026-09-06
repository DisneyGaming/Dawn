#include "sensor_state_heap_full_cohort.h"
#include "../../activity_lifecycle/native_activation_global_drop_fanout.h"

#include <Windows.h>
#include <bcrypt.h>
#include <intrin.h>

#ifdef interface
#undef interface
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>
#include <span>
#include <string_view>

#include "../../../../core/filesystem/path.h"
#include "../../../../core/logging/log.h"
#include "../../../../core/settings/settings.h"
#include "../../../diagnostics/module_range.h"
#include "../../../hooking/call_gate.h"
#include "../../../hooking/detour.h"
#include "../../assert_handler/assert_handler_lifecycle.h"
#include "../../retail_log/retail_log_lifecycle.h"

#pragma comment(lib, "bcrypt.lib")

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace sunrise::client::hooks::network::lifecycle::sensor_state_heap_full_cohort {
namespace {

constexpr std::size_t kModulePathCapacity = 32768U;
constexpr std::size_t kHashReadBytes = 64U * 1024U;
constexpr std::size_t kDropTableCapacity = 64U;
constexpr std::uint32_t kCoreAttachMask = (1U << kCoreHookCount) - 1U;
constexpr std::uint32_t kGlobalDropAttachBit = 1U << kCoreHookCount;
constexpr std::uint32_t kReceiveAttachBit = 1U << (kCoreHookCount + 1U);
constexpr std::uint32_t kFullAttachMask =
    kCoreAttachMask | kGlobalDropAttachBit | kReceiveAttachBit;
constexpr std::uint64_t kReceiveCookie = 0x314C485345435256ULL;
constexpr std::array<std::size_t, 3U> kMemberOffsets{0x38U, 0x20U, 0x50U};
constexpr std::size_t kDecodeObserverCapacity = 4U;

using SensorTableInsert = std::uint32_t*(__fastcall*)(void*,
                                                       std::uint32_t*,
                                                       const void*,
                                                       std::int32_t,
                                                       std::int32_t) noexcept;
using RecordConstruct = void(__fastcall*)(void*,
                                          std::uint16_t,
                                          const void*,
                                          std::int32_t,
                                          std::int32_t) noexcept;
using AllocateMember = void(__fastcall*)(void*,
                                         std::uint16_t,
                                         std::int32_t,
                                         const char*) noexcept;
using HeapResolver = void*(__fastcall*)(std::uint16_t) noexcept;
using ReceiveResolve = void*(__fastcall*)(void*) noexcept;
using InitializeState = void(__fastcall*)(std::uint32_t, void*) noexcept;
using DecodeState = std::uint64_t(__fastcall*)(void*,
                                               void*,
                                               std::int32_t*,
                                               std::uint32_t,
                                               std::uint32_t) noexcept;
using SensorTableRemove = void(__fastcall*)(void*, std::uint32_t) noexcept;
using RecordDestroy = void(__fastcall*)(void*) noexcept;
using RelativeFree = void(__fastcall*)(void*, std::uint64_t) noexcept;
using IndexUnlink = void(__fastcall*)(void*, std::uint64_t) noexcept;
using NetworkReset = void(__fastcall*)() noexcept;

enum class HookSlot : std::size_t {
    insert,
    construct,
    allocate,
    resolve,
    initialize,
    decode,
    remove,
    destroy,
    free,
    unlink,
    reset,
};

[[nodiscard]] constexpr std::size_t index(HookSlot slot) noexcept {
    return static_cast<std::size_t>(slot);
}

struct InsertContext final {
    InsertContext* previous{};
    std::uintptr_t sensorTable{};
    std::uintptr_t peerView{};
    std::uint32_t* outDatum{};
    SensorIdentity identity{};
    std::uint32_t authSchema{};
    std::uint32_t senseSchema{};
    RecordToken constructed{};
};

struct ConstructorContext final {
    ConstructorContext* previous{};
    RecordToken token{};
    std::array<MemberBaseline, 3U> members{};
    std::uint8_t seenMask{};
    std::uint32_t operationOrdinal{};
};

struct RemoveContext final {
    RemoveContext* previous{};
    std::uintptr_t sensorTable{};
    std::uintptr_t peerView{};
    std::uint32_t datum{};
    RecordToken token{};
};

struct DestroyContext final {
    DestroyContext* previous{};
    RecordToken token{};
    RecordState state{};
    std::array<MemberBaseline, 3U> currentMembers{};
    std::uint8_t currentMask{};
    std::uint8_t nextOrdinal{1U};
};

struct ResetContext final {
    ResetContext* previous{};
    std::uint64_t dropGeneration{};
    bool valid{};
};

struct WriterAttribution final {
    RecordToken token{};
    RecordState state{};
    Member member{Member::none};
    std::uintptr_t resolverDestination{};
    std::uint32_t operationOrdinal{};
    bool valid{};
};

struct DecodeObserverSlot final {
    /** 0=free, 1=active, 2=retiring/reserved. */
    std::atomic<std::uint32_t> state{};
    std::atomic<std::uint32_t> generation{};
    std::atomic<std::uint32_t> inFlight{};
    std::atomic<DecodeObserver> observer{};
    std::atomic<void*> context{};
};

SRWLOCK g_lifecycleLock{SRWLOCK_INIT};
hooking::CallGate g_callGate;
std::array<hooking::detour::Handle, kCoreHookCount> g_handles{};
std::atomic<SensorTableInsert> g_insertOriginal{};
std::atomic<RecordConstruct> g_constructOriginal{};
std::atomic<AllocateMember> g_allocateOriginal{};
std::atomic<ReceiveResolve> g_resolveOriginal{};
std::atomic<InitializeState> g_initializeOriginal{};
std::atomic<DecodeState> g_decodeOriginal{};
std::atomic<SensorTableRemove> g_removeOriginal{};
std::atomic<RecordDestroy> g_destroyOriginal{};
std::atomic<RelativeFree> g_freeOriginal{};
std::atomic<IndexUnlink> g_unlinkOriginal{};
std::atomic<NetworkReset> g_resetOriginal{};
std::atomic<HeapResolver> g_heapResolver{};
std::atomic<Readiness> g_readiness{Readiness::disabled};
std::atomic_bool g_receiveOwnerAttached{};
std::atomic_bool g_receiveOwnerQuiescing{};
std::atomic_bool g_globalDropOwnerAttached{};
std::atomic_bool g_globalDropOwnerQuiescing{};
activity_lifecycle::NativeActivationGlobalDropObserverHandle g_globalDropObserverHandle{};
std::atomic<std::uint32_t> g_receiveInflight{};
std::atomic_bool g_partialMarker{};
std::atomic_bool g_flushRequested{};
std::atomic<std::uint64_t> g_runClock{};
std::array<DecodeObserverSlot, kDecodeObserverCapacity> g_decodeObservers{};

HANDLE g_ledgerFile{INVALID_HANDLE_VALUE};
HANDLE g_ledgerMapping{};
void* g_ledgerView{};
Shl1Header* g_header{};
EventV1* g_events{};
EventLedger g_eventLedger{};
RecordRegistry<kRecordCapacity> g_records{};
AllocationIndex<kAllocationKeyCapacity> g_allocations{};
std::array<wchar_t, kModulePathCapacity> g_ledgerPath{};

thread_local InsertContext* g_insertContext{};
thread_local ConstructorContext* g_constructorContext{};
thread_local ReceiveToken* g_receiveContext{};
thread_local RemoveContext* g_removeContext{};
thread_local DestroyContext* g_destroyContext{};
thread_local ResetContext* g_resetContext{};

[[nodiscard]] bool calls_idle() noexcept {
    if (!g_callGate.idle() || g_receiveInflight.load(std::memory_order_acquire) != 0U) {
        return false;
    }
    return std::all_of(g_decodeObservers.begin(), g_decodeObservers.end(), [](const auto& slot) {
        return slot.inFlight.load(std::memory_order_acquire) == 0U;
    });
}

class FullCallScope final {
public:
    FullCallScope() noexcept
        : call_(g_callGate), header_(g_header) {
        if (header_ != nullptr) {
            header_->inFlight.fetch_add(1U, std::memory_order_acq_rel);
        }
    }
    ~FullCallScope() noexcept {
        if (header_ != nullptr) {
            header_->inFlight.fetch_sub(1U, std::memory_order_release);
        }
    }
    [[nodiscard]] bool observes() const noexcept {
        if (!call_.accepts_side_effects()) {
            return false;
        }
        if (g_readiness.load(std::memory_order_acquire) != Readiness::ready) {
            account(header_, FailureCounter::preReadyCalls);
            return false;
        }
        return true;
    }

private:
    hooking::CallGate::Scope call_;
    Shl1Header* header_{};
};

[[nodiscard]] std::uint64_t qpc_now() noexcept {
    LARGE_INTEGER value{};
    return QueryPerformanceCounter(&value) != FALSE
               ? static_cast<std::uint64_t>(value.QuadPart)
               : 0U;
}

[[nodiscard]] bool add_address(std::uintptr_t base,
                               std::uint64_t offset,
                               std::uintptr_t& output) noexcept {
    if (!checked_add(base, offset, output)) {
        account(g_header, FailureCounter::checkedArithmeticFailure);
        return false;
    }
    return true;
}

[[nodiscard]] LONG access_filter(DWORD status) noexcept {
    switch (status) {
    case EXCEPTION_ACCESS_VIOLATION:
        account(g_header, FailureCounter::guardAccessViolation);
        return EXCEPTION_EXECUTE_HANDLER;
    case EXCEPTION_IN_PAGE_ERROR:
        account(g_header, FailureCounter::guardInPageError);
        return EXCEPTION_EXECUTE_HANDLER;
    case EXCEPTION_DATATYPE_MISALIGNMENT:
        account(g_header, FailureCounter::guardMisalignment);
        return EXCEPTION_EXECUTE_HANDLER;
    default:
        return EXCEPTION_CONTINUE_SEARCH;
    }
}

[[nodiscard]] bool readable_range(std::uintptr_t source, std::size_t bytes) noexcept {
    if (source == 0U || bytes == 0U
        || source > (std::numeric_limits<std::uintptr_t>::max)() - bytes) {
        account(g_header, FailureCounter::checkedArithmeticFailure);
        return false;
    }
    const std::uintptr_t end = source + bytes;
    std::uintptr_t cursor = source;
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION information{};
        if (VirtualQuery(reinterpret_cast<const void*>(cursor),
                         &information,
                         sizeof information)
            != sizeof information) {
            account(g_header, FailureCounter::guardVirtualQueryReject);
            return false;
        }
        const DWORD protection = information.Protect;
        if (information.State != MEM_COMMIT
            || (protection & (PAGE_GUARD | PAGE_NOACCESS)) != 0U) {
            account(g_header, FailureCounter::guardPageReject);
            return false;
        }
        const DWORD access = protection & 0xFFU;
        if (access != PAGE_READONLY && access != PAGE_READWRITE && access != PAGE_WRITECOPY
            && access != PAGE_EXECUTE_READ && access != PAGE_EXECUTE_READWRITE
            && access != PAGE_EXECUTE_WRITECOPY) {
            account(g_header, FailureCounter::guardPageReject);
            return false;
        }
        const std::uintptr_t regionBase =
            reinterpret_cast<std::uintptr_t>(information.BaseAddress);
        if (regionBase > (std::numeric_limits<std::uintptr_t>::max)() - information.RegionSize) {
            account(g_header, FailureCounter::checkedArithmeticFailure);
            return false;
        }
        const std::uintptr_t regionEnd = regionBase + information.RegionSize;
        if (regionEnd <= cursor) {
            account(g_header, FailureCounter::guardVirtualQueryReject);
            return false;
        }
        cursor = (std::min)(regionEnd, end);
    }
    return true;
}

[[nodiscard]] bool guarded_copy(std::uintptr_t source,
                                void* destination,
                                std::size_t bytes) noexcept {
    if (destination == nullptr || !readable_range(source, bytes)) {
        return false;
    }
    __try {
        std::memcpy(destination, reinterpret_cast<const void*>(source), bytes);
        return true;
    } __except (access_filter(GetExceptionCode())) {
        return false;
    }
}

[[nodiscard]] bool guarded_hash(std::uintptr_t source,
                                std::size_t bytes,
                                std::uint64_t& output) noexcept {
    output = 0U;
    if (bytes > kPayloadHashCeiling) {
        account(g_header, FailureCounter::payloadHashCeiling);
        return false;
    }
    if (bytes == 0U) {
        output = 14695981039346656037ULL;
        return true;
    }
    if (!readable_range(source, bytes)) {
        return false;
    }
    std::array<std::byte, 4096U> buffer{};
    std::uint64_t hash = 14695981039346656037ULL;
    std::size_t consumed = 0U;
    __try {
        while (consumed < bytes) {
            const std::size_t count = (std::min)(buffer.size(), bytes - consumed);
            std::memcpy(buffer.data(),
                        reinterpret_cast<const void*>(source + consumed),
                        count);
            for (std::size_t index = 0U; index < count; ++index) {
                hash ^= std::to_integer<std::uint8_t>(buffer[index]);
                hash *= 1099511628211ULL;
            }
            consumed += count;
        }
    } __except (access_filter(GetExceptionCode())) {
        return false;
    }
    output = hash;
    return true;
}

struct WindowsReader final {
    [[nodiscard]] bool copy(std::uintptr_t source,
                            void* destination,
                            std::size_t bytes) noexcept {
        return guarded_copy(source, destination, bytes);
    }
    [[nodiscard]] bool hash(std::uintptr_t source,
                            std::size_t bytes,
                            std::uint64_t& output) noexcept {
        return guarded_hash(source, bytes, output);
    }
};

[[nodiscard]] bool capture_identity(const void* source, SensorIdentity& output) noexcept {
    output = {};
    const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(source);
    std::uintptr_t kindAddress{};
    std::uintptr_t indexAddress{};
    if (address == 0U || !add_address(address, 4U, kindAddress)
        || !add_address(address, 6U, indexAddress)
        || !guarded_copy(address, &output.word, sizeof output.word)
        || !guarded_copy(kindAddress, &output.kind, sizeof output.kind)
        || !guarded_copy(indexAddress, &output.index, sizeof output.index)) {
        output = {};
        return false;
    }
    output.valid = true;
    return true;
}

[[nodiscard]] std::size_t member_index(Member member) noexcept {
    switch (member) {
    case Member::receivedAuth: return 0U;
    case Member::extractedSense: return 1U;
    case Member::receivedSense: return 2U;
    default: return 3U;
    }
}

[[nodiscard]] std::uint8_t destruction_ordinal(Member member) noexcept {
    switch (member) {
    case Member::receivedSense: return 1U;
    case Member::extractedSense: return 2U;
    case Member::receivedAuth: return 3U;
    default: return 0U;
    }
}

[[nodiscard]] Member classify_member(std::uintptr_t record, std::uintptr_t triple) noexcept {
    for (std::size_t index = 0U; index < kMemberOffsets.size(); ++index) {
        std::uintptr_t expected{};
        if (add_address(record, kMemberOffsets[index], expected) && expected == triple) {
            return static_cast<Member>(index + 1U);
        }
    }
    return Member::none;
}

[[nodiscard]] bool capture_member_triple(std::uintptr_t record,
                                         Member member,
                                         MemberBaseline& output) noexcept {
    output = {};
    const std::size_t slot = member_index(member);
    if (slot >= kMemberOffsets.size()) {
        return false;
    }
    std::uintptr_t triple{};
    std::uintptr_t relativeAddress{};
    std::uintptr_t selectorAddress{};
    if (!add_address(record, kMemberOffsets[slot], triple)
        || !add_address(triple, 8U, relativeAddress)
        || !add_address(triple, 0x10U, selectorAddress)
        || !guarded_copy(triple, &output.count, sizeof output.count)
        || !guarded_copy(relativeAddress,
                         &output.relativePayload,
                         sizeof output.relativePayload)
        || !guarded_copy(selectorAddress, &output.selector, sizeof output.selector)) {
        return false;
    }
    output.valid = true;
    return true;
}

[[nodiscard]] bool cng_ok(NTSTATUS status) noexcept { return status >= 0; }

[[nodiscard]] bool current_executable_identity(HMODULE module,
                                               ImageSha256& sha256,
                                               std::uint64_t& fileSize) noexcept {
    sha256 = {};
    fileSize = 0U;
    std::array<wchar_t, kModulePathCapacity> path{};
    const DWORD copied = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
    if (copied == 0U || copied >= path.size()) {
        account(g_header, FailureCounter::packedOpenFailure);
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
        account(g_header, FailureCounter::packedOpenFailure);
        return false;
    }
    LARGE_INTEGER length{};
    bool complete = GetFileSizeEx(file, &length) != FALSE && length.QuadPart >= 0;
    BCRYPT_ALG_HANDLE algorithm{};
    BCRYPT_HASH_HANDLE hash{};
    if (complete) {
        fileSize = static_cast<std::uint64_t>(length.QuadPart);
        complete = cng_ok(
            BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0));
    }
    if (complete) {
        complete = cng_ok(BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0));
    }
    std::array<std::byte, kHashReadBytes> buffer{};
    while (complete) {
        DWORD transferred{};
        if (ReadFile(file,
                     buffer.data(),
                     static_cast<DWORD>(buffer.size()),
                     &transferred,
                     nullptr)
            == FALSE) {
            account(g_header, FailureCounter::packedReadFailure);
            complete = false;
            break;
        }
        if (transferred == 0U) {
            break;
        }
        complete = cng_ok(BCryptHashData(hash,
                                         reinterpret_cast<PUCHAR>(buffer.data()),
                                         transferred,
                                         0));
    }
    if (complete) {
        complete = cng_ok(BCryptFinishHash(hash,
                                           reinterpret_cast<PUCHAR>(sha256.data()),
                                           static_cast<ULONG>(sha256.size()),
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
        sha256 = {};
        fileSize = 0U;
    }
    return complete;
}

[[nodiscard]] bool mapped_pe_identity(std::span<const std::byte> image,
                                      ImageView& output) noexcept {
    if (image.size() < sizeof(IMAGE_DOS_HEADER)) {
        return false;
    }
    IMAGE_DOS_HEADER dos{};
    std::memcpy(&dos, image.data(), sizeof dos);
    if (dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < 0
        || static_cast<std::size_t>(dos.e_lfanew) > image.size()
        || sizeof(IMAGE_NT_HEADERS64) > image.size() - static_cast<std::size_t>(dos.e_lfanew)) {
        return false;
    }
    IMAGE_NT_HEADERS64 nt{};
    std::memcpy(&nt, image.data() + dos.e_lfanew, sizeof nt);
    if (nt.Signature != IMAGE_NT_SIGNATURE
        || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        return false;
    }
    output.machine = nt.FileHeader.Machine;
    output.sectionCount = nt.FileHeader.NumberOfSections;
    output.timestamp = nt.FileHeader.TimeDateStamp;
    output.imageSize = nt.OptionalHeader.SizeOfImage;
    output.entryRva = nt.OptionalHeader.AddressOfEntryPoint;
    output.checksum = nt.OptionalHeader.CheckSum;
    const IMAGE_DATA_DIRECTORY debugDirectory =
        nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
    if (debugDirectory.VirtualAddress == 0U
        || debugDirectory.Size < sizeof(IMAGE_DEBUG_DIRECTORY)
        || debugDirectory.VirtualAddress > image.size()
        || debugDirectory.Size > image.size() - debugDirectory.VirtualAddress) {
        return false;
    }
    const std::size_t debugCount = debugDirectory.Size / sizeof(IMAGE_DEBUG_DIRECTORY);
    for (std::size_t index = 0U; index < debugCount; ++index) {
        IMAGE_DEBUG_DIRECTORY entry{};
        std::memcpy(&entry,
                    image.data() + debugDirectory.VirtualAddress
                        + index * sizeof(IMAGE_DEBUG_DIRECTORY),
                    sizeof entry);
        if (entry.Type != IMAGE_DEBUG_TYPE_CODEVIEW || entry.AddressOfRawData == 0U
            || entry.SizeOfData < 24U || entry.AddressOfRawData > image.size()
            || entry.SizeOfData > image.size() - entry.AddressOfRawData) {
            continue;
        }
        const std::byte* const codeView = image.data() + entry.AddressOfRawData;
        constexpr std::array<std::byte, 4U> rsds{
            std::byte{'R'}, std::byte{'S'}, std::byte{'D'}, std::byte{'S'}};
        if (std::memcmp(codeView, rsds.data(), rsds.size()) != 0) {
            continue;
        }
        std::memcpy(output.codeViewGuid.data(), codeView + 4U, output.codeViewGuid.size());
        std::memcpy(&output.codeViewAge, codeView + 20U, sizeof output.codeViewAge);
        return true;
    }
    return false;
}

[[nodiscard]] const char* validation_name(ImageValidationResult result) noexcept {
    switch (result) {
    case ImageValidationResult::valid: return "ok";
    case ImageValidationResult::hashMismatch: return "packed_hash";
    case ImageValidationResult::fileSizeMismatch: return "packed_size";
    case ImageValidationResult::peMismatch: return "pe";
    case ImageValidationResult::codeViewMismatch: return "codeview";
    case ImageValidationResult::targetBounds: return "target_bounds";
    case ImageValidationResult::prefixMismatch: return "prefix";
    default: return "unknown";
    }
}

[[nodiscard]] bool create_directory_owned(const wchar_t* path) noexcept {
    return CreateDirectoryW(path, nullptr) != FALSE || GetLastError() == ERROR_ALREADY_EXISTS;
}

[[nodiscard]] bool create_ledger(const ImageView& image,
                                 std::span<const std::array<std::byte, 17U>> observed) noexcept {
    core::path::Buffer path{};
    if (!core::path::artifact_directory(reinterpret_cast<void*>(&__ImageBase), path)
        || !core::path::append(path, L"\\diagnostics")
        || !create_directory_owned(path.chars.data())
        || !core::path::append(path, L"\\sensor-heap")
        || !create_directory_owned(path.chars.data())) {
        return false;
    }
    FILETIME now{};
    GetSystemTimeAsFileTime(&now);
    const std::uint64_t fileTime =
        (static_cast<std::uint64_t>(now.dwHighDateTime) << 32U) | now.dwLowDateTime;
    const std::uint64_t runId = g_runClock.fetch_add(1U, std::memory_order_relaxed) + 1U;
    std::array<wchar_t, 160U> suffix{};
    const int written = swprintf_s(suffix.data(),
                                   suffix.size(),
                                   L"\\shl1-%016llX-p%lu-r%llu.bin",
                                   static_cast<unsigned long long>(fileTime),
                                   static_cast<unsigned long>(GetCurrentProcessId()),
                                   static_cast<unsigned long long>(runId));
    if (written <= 0 || !core::path::append(path, suffix.data())) {
        return false;
    }
    std::wmemcpy(g_ledgerPath.data(), path.chars.data(), path.length + 1U);
    g_ledgerFile = CreateFileW(path.chars.data(),
                               GENERIC_READ | GENERIC_WRITE,
                               FILE_SHARE_READ,
                               nullptr,
                               CREATE_NEW,
                               FILE_ATTRIBUTE_NORMAL,
                               nullptr);
    if (g_ledgerFile == INVALID_HANDLE_VALUE) {
        return false;
    }
    LARGE_INTEGER size{};
    size.QuadPart = static_cast<LONGLONG>(kFileBytes);
    if (SetFilePointerEx(g_ledgerFile, size, nullptr, FILE_BEGIN) == FALSE
        || SetEndOfFile(g_ledgerFile) == FALSE) {
        account(g_header, FailureCounter::ledgerSizeFailure);
        return false;
    }
    g_ledgerMapping = CreateFileMappingW(g_ledgerFile,
                                         nullptr,
                                         PAGE_READWRITE,
                                         static_cast<DWORD>(kFileBytes >> 32U),
                                         static_cast<DWORD>(kFileBytes),
                                         nullptr);
    if (g_ledgerMapping == nullptr) {
        return false;
    }
    g_ledgerView = MapViewOfFile(g_ledgerMapping,
                                 FILE_MAP_READ | FILE_MAP_WRITE,
                                 0U,
                                 0U,
                                 static_cast<SIZE_T>(kFileBytes));
    if (g_ledgerView == nullptr) {
        return false;
    }
    std::memset(g_ledgerView, 0, static_cast<std::size_t>(kFileBytes));
    g_header = ::new (g_ledgerView) Shl1Header{};
    g_events = reinterpret_cast<EventV1*>(static_cast<std::byte*>(g_ledgerView) + kHeaderBytes);
    for (std::size_t index = 0U; index < kEventCapacity; ++index) {
        ::new (g_events + index) EventV1{};
    }
    g_header->magic = {'S', 'H', 'L', '1'};
    g_header->schemaVersion = kSchemaVersion;
    g_header->headerBytes = static_cast<std::uint16_t>(kHeaderBytes);
    g_header->eventBytes = static_cast<std::uint16_t>(kEventBytes);
    g_header->pointerWidth = 8U;
    g_header->endianness = 1U;
    g_header->capacity = static_cast<std::uint32_t>(kEventCapacity);
    g_header->normalCapacity = static_cast<std::uint32_t>(kNormalEventCapacity);
    g_header->criticalCapacity = static_cast<std::uint32_t>(kCriticalEventCapacity);
    g_header->processId = GetCurrentProcessId();
    g_header->siteCount = static_cast<std::uint32_t>(kSiteCount);
    g_header->runId = runId;
    g_header->loadedImageBase = reinterpret_cast<std::uintptr_t>(image.mapped.data());
    g_header->sizeOfImage = image.imageSize;
    g_header->peTimestamp = image.timestamp;
    g_header->peChecksum = image.checksum;
    g_header->entryRva = image.entryRva;
    g_header->packedFileSize = image.packedFileSize;
    g_header->packedSha256 = image.packedSha256;
    g_header->unpackedSha256Provenance = kPinnedUnpackedImageSha256;
    g_header->codeViewGuid = image.codeViewGuid;
    g_header->codeViewAge = image.codeViewAge;
    LARGE_INTEGER frequency{};
    if (QueryPerformanceFrequency(&frequency) != FALSE) {
        g_header->qpcFrequency = static_cast<std::uint64_t>(frequency.QuadPart);
    }
    g_header->counterCount = static_cast<std::uint32_t>(kFailureCounterCount);
    g_header->siteManifestOffset = 1024U;
    g_header->eventRegionOffset = 4096U;
    g_header->verdict.store(static_cast<std::uint32_t>(Verdict::inconclusive),
                            std::memory_order_relaxed);
    for (std::size_t siteIndex = 0U; siteIndex < kSiteManifest.size(); ++siteIndex) {
        const SiteContract& source = kSiteManifest[siteIndex];
        SiteManifestRecord& destination = g_header->siteManifest[siteIndex];
        destination.rva = source.rva;
        destination.packedRawOffset = source.packedRawOffset;
        destination.prefixLength = source.prefixLength;
        destination.action = static_cast<std::uint8_t>(source.action);
        destination.expectedMapped = source.expectedMapped;
        destination.observedMapped = observed[siteIndex];
        destination.packedRaw = source.packedRaw;
        (void)strncpy_s(destination.role.data(),
                        destination.role.size(),
                        source.role,
                        _TRUNCATE);
    }
    g_header->validationMask.store((1U << kSiteCount) - 1U, std::memory_order_release);
    g_eventLedger.bind(g_header, g_events);
    g_records.bind(g_header);
    g_records.reset();
    g_allocations.bind(g_header);
    g_allocations.reset();
    return true;
}

void close_ledger() noexcept {
    g_eventLedger.bind(nullptr, nullptr, 0U, 0U);
    g_records.bind(nullptr);
    g_allocations.bind(nullptr);
    g_header = nullptr;
    g_events = nullptr;
    if (g_ledgerView != nullptr) {
        (void)UnmapViewOfFile(g_ledgerView);
        g_ledgerView = nullptr;
    }
    if (g_ledgerMapping != nullptr) {
        (void)CloseHandle(g_ledgerMapping);
        g_ledgerMapping = nullptr;
    }
    if (g_ledgerFile != INVALID_HANDLE_VALUE) {
        (void)CloseHandle(g_ledgerFile);
        g_ledgerFile = INVALID_HANDLE_VALUE;
    }
    g_ledgerPath = {};
}

[[nodiscard]] bool durable_flush(bool headerOnlySecondPass) noexcept {
    if (g_ledgerView == nullptr || g_ledgerFile == INVALID_HANDLE_VALUE || g_header == nullptr) {
        return false;
    }
    const SIZE_T bytes = headerOnlySecondPass ? kHeaderBytes : static_cast<SIZE_T>(kFileBytes);
    if (FlushViewOfFile(g_ledgerView, bytes) == FALSE) {
        account(g_header, FailureCounter::flushViewFailure);
        g_header->flushState.store(static_cast<std::uint32_t>(FlushState::viewFailed),
                                   std::memory_order_release);
        return false;
    }
    if (FlushFileBuffers(g_ledgerFile) == FALSE) {
        account(g_header, FailureCounter::flushFileFailure);
        g_header->flushState.store(static_cast<std::uint32_t>(FlushState::fileFailed),
                                   std::memory_order_release);
        return false;
    }
    return true;
}

void report_install_failure(const char* reason) noexcept {
    std::array<char, 320U> line{};
    const int written = std::snprintf(
        line.data(),
        line.size(),
        "ev=sensor_heap v=1 stage=install result=fail reason=%s retained=%u native_mutation=none",
        reason,
        has_ownership() ? 1U : 0U);
    if (written > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::warn,
                         {line.data(),
                          (std::min)(static_cast<std::size_t>(written), line.size() - 1U)});
    }
}

void prepare_event(EventV1& event,
                   EventKind kind,
                   EventPhase phase,
                   Member member,
                   EventForm form) noexcept {
    event.qpc = qpc_now();
    event.tick = GetTickCount64();
    event.threadId = GetCurrentThreadId();
    event.kind = static_cast<std::uint8_t>(kind);
    event.phase = static_cast<std::uint8_t>(phase);
    event.member = static_cast<std::uint8_t>(member);
    event.form = static_cast<std::uint8_t>(form);
    if (g_header != nullptr) {
        event.dropGeneration = g_header->dropGeneration.load(std::memory_order_acquire);
        if (event.dropGeneration != 0U) {
            event.valid |= validDropGeneration;
        }
        event.timeoutOrdinal = g_header->timeoutOrdinal.load(std::memory_order_acquire);
        event.resumeOrdinal = g_header->resumeOrdinal.load(std::memory_order_acquire);
        if (g_header->assertSeen.load(std::memory_order_acquire) != 0U) {
            event.flags |= flagPostFirstHeapAssert;
            account(g_header, FailureCounter::postAssertContamination);
        }
    }
}

void apply_record(EventV1& event, const RecordState& state) noexcept {
    event.record = state.record;
    event.recordGeneration = state.generation;
    event.peerTable = state.sensorTable;
    event.sensorKey = state.sensorKey;
    event.datum = state.datum;
    event.authSchema = state.authSchema;
    event.senseSchema = state.senseSchema;
    event.activityGeneration = state.activityGeneration;
    event.connectionGeneration = state.connectionGeneration;
    event.regionGeneration = state.regionGeneration;
    if (state.record != 0U) {
        event.valid |= validRecord;
    }
    if (state.generation != 0U) {
        event.valid |= validRecordGeneration;
    }
    if (state.tableValid) {
        event.valid |= validPeerTable;
        if (state.sensorTable >= 0x28U) {
            event.valid |= validPeerViewDerived;
        }
    }
    if (state.datumValid) {
        event.valid |= validDatum;
    }
    if (state.identity.valid) {
        event.valid |= validSensorIdentity;
    }
    if (state.committed) {
        event.valid |= validRecordMetadata;
    }
    if (state.activityGeneration != 0U) {
        event.valid |= validActivityGeneration;
    }
    if (state.connectionGeneration != 0U) {
        event.valid |= validConnectionGeneration;
    }
    if (state.regionGeneration != 0U) {
        event.valid |= validRegionGeneration;
    }
}

void push_heap_event(EventKind kind,
                     EventPhase phase,
                     Member member,
                     const RecordState* state,
                     HeapCapture capture,
                     std::uint32_t operationOrdinal = 0U,
                     std::uint32_t callerRva = 0U,
                     std::uint32_t extraValid = 0U,
                     std::uint32_t extraFlags = 0U,
                     std::uint32_t freeOrdinal = 0U) noexcept {
    EventClaim claim = g_eventLedger.claim_normal();
    if (!claim) {
        return;
    }
    EventV1& event = *claim.first;
    prepare_event(event, kind, phase, member, EventForm::heapSnapshot);
    if (state != nullptr) {
        apply_record(event, *state);
    }
    event.operationOrdinal = operationOrdinal;
    event.callerRva = callerRva;
    event.valid |= capture.valid | extraValid;
    event.flags |= capture.flags | extraFlags;
    event.payload.heapSnapshot = capture.payload;
    event.payload.heapSnapshot.freeOrdinal = freeOrdinal;
    g_eventLedger.commit(event, claim.firstSequence);
}

void active_reuse_observer(void*, const RecordState& previous) noexcept {
    HeapCapture empty{};
    push_heap_event(EventKind::construct,
                    EventPhase::pre,
                    Member::none,
                    &previous,
                    empty,
                    0U,
                    0U,
                    0U,
                    flagActiveRecordReuse);
}

[[nodiscard]] HeapCapture capture_current_member(const RecordState& state,
                                                  Member member,
                                                  MemberBaseline* currentOutput = nullptr) noexcept {
    MemberBaseline current{};
    HeapCapture capture{};
    if (!capture_member_triple(state.record, member, current)) {
        capture.flags |= flagGuardedReadFault;
        return capture;
    }
    const HeapResolver resolver = g_heapResolver.load(std::memory_order_acquire);
    if (resolver == nullptr) {
        capture.flags |= flagGuardedReadFault;
        return capture;
    }
    current.heap = reinterpret_cast<std::uintptr_t>(resolver(current.selector));
    WindowsReader reader{};
    capture = capture_heap(reader,
                           current.heap,
                           current.relativePayload,
                           current.count,
                           current.selector);
    capture.valid |= validMemberTriple;
    const std::size_t slot = member_index(member);
    if (slot < state.members.size() && state.members[slot].valid) {
        const MemberBaseline& baseline = state.members[slot];
        if (current.selector != baseline.selector) {
            capture.flags |= flagSelectorChanged;
        }
        if (current.relativePayload != baseline.relativePayload) {
            capture.flags |= flagRelativePayloadChanged;
        }
        std::uintptr_t baselineAbsolute{};
        std::uintptr_t currentAbsolute{};
        if (checked_add(baseline.heapBase, baseline.relativePayload, baselineAbsolute)
            && checked_add(capture.payload.heapBase,
                           current.relativePayload,
                           currentAbsolute)
            && baselineAbsolute != currentAbsolute) {
            capture.flags |= flagAbsoluteDestinationChanged;
        }
        if (current.count != baseline.count) {
            capture.flags |= flagCountOrExtentMismatch;
        }
    }
    if (reader.hash(state.record, 0x7CU, capture.payload.recordHash)) {
        capture.valid |= validRecordHash;
    } else {
        capture.flags |= flagGuardedReadFault;
    }
    current.heapBase = static_cast<std::uintptr_t>(capture.payload.heapBase);
    current.relativeHeader = capture.payload.relativeHeader;
    current.headerWidth = capture.payload.headerWidth;
    if (currentOutput != nullptr) {
        *currentOutput = current;
    }
    return capture;
}

[[nodiscard]] HeapCapture capture_passed_allocation(const RecordState& state,
                                                     Member member,
                                                     std::uintptr_t heap,
                                                     std::uint64_t relativePayload,
                                                     const MemberBaseline& current) noexcept {
    WindowsReader reader{};
    HeapCapture capture = capture_heap(
        reader, heap, relativePayload, current.count, current.selector);
    capture.valid |= validMemberTriple;
    const std::size_t slot = member_index(member);
    if (slot < state.members.size() && state.members[slot].valid) {
        const MemberBaseline& baseline = state.members[slot];
        if (current.selector != baseline.selector) {
            capture.flags |= flagSelectorChanged;
        }
        if (relativePayload != baseline.relativePayload) {
            capture.flags |= flagRelativePayloadChanged;
        }
        std::uintptr_t baselineAbsolute{};
        std::uintptr_t currentAbsolute{};
        if (checked_add(baseline.heapBase, baseline.relativePayload, baselineAbsolute)
            && checked_add(capture.payload.heapBase, relativePayload, currentAbsolute)
            && baselineAbsolute != currentAbsolute) {
            capture.flags |= flagAbsoluteDestinationChanged;
        }
        if (current.count != baseline.count) {
            capture.flags |= flagCountOrExtentMismatch;
        }
    }
    if (reader.hash(state.record, 0x7CU, capture.payload.recordHash)) {
        capture.valid |= validRecordHash;
    } else {
        capture.flags |= flagGuardedReadFault;
    }
    return capture;
}

[[nodiscard]] WriterAttribution attribute_writer(void* destination) noexcept {
    WriterAttribution output{};
    const std::uintptr_t writerDestination = reinterpret_cast<std::uintptr_t>(destination);
    if (g_receiveContext != nullptr && g_receiveContext->active
        && g_receiveContext->admitted && g_receiveContext->record != 0U
        && g_receiveContext->destination == writerDestination) {
        RecordToken token{};
        if (g_records.read(g_receiveContext->record, output.state, &token)
                == RegistryResult::success
            && token.generation == g_receiveContext->recordGeneration) {
            output.token = token;
            output.member = Member::receivedSense;
            output.resolverDestination = g_receiveContext->destination;
            output.operationOrdinal = ++g_receiveContext->operationOrdinal;
            output.valid = true;
            return output;
        }
    }
    if (g_constructorContext == nullptr || !g_constructorContext->token) {
        return output;
    }
    if (g_records.read(g_constructorContext->token.record, output.state, &output.token)
        != RegistryResult::success) {
        return output;
    }
    for (std::size_t slot = 0U; slot < g_constructorContext->members.size(); ++slot) {
        const MemberBaseline& member = g_constructorContext->members[slot];
        std::uintptr_t absolute{};
        if (member.valid && checked_add(member.heapBase, member.relativePayload, absolute)
            && absolute == writerDestination) {
            output.member = static_cast<Member>(slot + 1U);
            output.resolverDestination = absolute;
            output.operationOrdinal = ++g_constructorContext->operationOrdinal;
            output.valid = true;
            return output;
        }
    }
    return output;
}

[[nodiscard]] std::uint32_t emit_writer_pair(const WriterAttribution& attribution,
                                             EventKind kind,
                                             EventPhase phase,
                                             void* stream,
                                             void* destination,
                                             std::int32_t* schemaKey,
                                             std::uint32_t schema,
                                             std::uint32_t nativeFlags,
                                             std::uint32_t bitsBefore,
                                             std::uint32_t bitsAfter,
                                             std::uint64_t nativeResult) noexcept {
    if (!attribution.valid) {
        return 0U;
    }
    HeapCapture capture = capture_current_member(attribution.state, attribution.member);
    EventClaim claim = g_eventLedger.claim_pair();
    if (!claim || claim.second == nullptr) {
        return capture.flags | flagCapacityLoss;
    }
    EventV1& meta = *claim.first;
    EventV1& links = *claim.second;
    prepare_event(meta, kind, phase, attribution.member, EventForm::writerMeta);
    prepare_event(links, kind, phase, attribution.member, EventForm::writerLinks);
    apply_record(meta, attribution.state);
    apply_record(links, attribution.state);
    meta.operationOrdinal = attribution.operationOrdinal;
    links.operationOrdinal = attribution.operationOrdinal;
    meta.valid |= capture.valid | validWriterDestination | validPairedEvent;
    links.valid |= capture.valid | validWriterDestination | validPairedEvent;
    meta.flags |= capture.flags;
    links.flags |= capture.flags;
    const std::uintptr_t destinationAddress = reinterpret_cast<std::uintptr_t>(destination);
    if (destinationAddress != attribution.resolverDestination) {
        meta.flags |= flagWriterDestinationMismatch;
        links.flags |= flagWriterDestinationMismatch;
    }
    if (schema != attribution.state.senseSchema) {
        meta.flags |= flagWriterSchemaMismatch;
        links.flags |= flagWriterSchemaMismatch;
    }
    WriterMetaPayload& payload = meta.payload.writerMeta;
    payload.stream = reinterpret_cast<std::uintptr_t>(stream);
    payload.destination = destinationAddress;
    payload.resolverDestination = attribution.resolverDestination;
    payload.schemaKeyPointer = reinterpret_cast<std::uintptr_t>(schemaKey);
    payload.nativeResult = nativeResult;
    payload.relativePayload = capture.payload.relativePayload;
    payload.heap = capture.payload.heap;
    payload.heapBase = capture.payload.heapBase;
    payload.relativeHeader = capture.payload.relativeHeader;
    payload.payloadHash = capture.payload.payloadHash;
    payload.recordHash = capture.payload.recordHash;
    payload.schema = schema;
    payload.count = capture.payload.count;
    payload.bitsBefore = bitsBefore;
    payload.bitsAfter = bitsAfter;
    payload.nativeFlags = nativeFlags;
    payload.selector = capture.payload.selector;
    payload.headerWidth = capture.payload.headerWidth;
    payload.invariant = capture.payload.invariant;
    payload.companionSequence = claim.secondSequence;
    if (stream != nullptr) {
        meta.valid |= validStream;
        links.valid |= validStream;
    }
    if (bitsBefore != 0U || bitsAfter != 0U) {
        meta.valid |= validStreamBits;
        links.valid |= validStreamBits;
    }
    if (schemaKey != nullptr
        && guarded_copy(reinterpret_cast<std::uintptr_t>(schemaKey),
                        &payload.schemaKeyValue,
                        sizeof payload.schemaKeyValue)) {
        meta.valid |= validSchemaKey;
        links.valid |= validSchemaKey;
    }
    if (phase == EventPhase::post) {
        meta.valid |= validNativeResult;
        links.valid |= validNativeResult;
    }
    links.payload.writerLinks = capture.payload;
    links.payload.writerLinks.schema = schema;
    links.payload.writerLinks.bitsBefore = bitsBefore;
    links.payload.writerLinks.bitsAfter = bitsAfter;
    links.payload.writerLinks.nativeFlags = nativeFlags;
    g_eventLedger.commit(links, claim.secondSequence);
    g_eventLedger.commit(meta, claim.firstSequence);
    return capture.flags;
}

[[nodiscard]] bool capture_stream_bits(void* stream, std::uint32_t& output) noexcept {
    output = 0U;
    std::uintptr_t address{};
    return stream != nullptr
           && add_address(reinterpret_cast<std::uintptr_t>(stream), 0x24U, address)
           && guarded_copy(address, &output, sizeof output);
}

void notify_decode_observers(const DecodeObservation& observation) noexcept {
    for (DecodeObserverSlot& slot : g_decodeObservers) {
        slot.inFlight.fetch_add(1U, std::memory_order_acq_rel);
        if (slot.state.load(std::memory_order_acquire) == 1U) {
            const DecodeObserver observer = slot.observer.load(std::memory_order_acquire);
            void* const context = slot.context.load(std::memory_order_acquire);
            if (observer != nullptr) {
                observer(context, observation);
            }
        }
        slot.inFlight.fetch_sub(1U, std::memory_order_release);
    }
}

[[nodiscard]] std::uint32_t direct_caller_rva(void* rawReturn,
                                              bool& direct,
                                              std::uint32_t& rawRva) noexcept {
    direct = false;
    rawRva = 0U;
    if (g_header == nullptr) {
        return 0U;
    }
    const std::uintptr_t base = static_cast<std::uintptr_t>(g_header->loadedImageBase);
    const std::uintptr_t returned = reinterpret_cast<std::uintptr_t>(rawReturn);
    if (returned < base || returned - base > (std::numeric_limits<std::uint32_t>::max)()) {
        return 0U;
    }
    rawRva = static_cast<std::uint32_t>(returned - base);
    if (returned < base + 5U) {
        return rawRva;
    }
    std::array<std::byte, 5U> call{};
    if (!guarded_copy(returned - 5U, call.data(), call.size())
        || call[0] != std::byte{0xE8U}) {
        return rawRva;
    }
    std::int32_t displacement{};
    std::memcpy(&displacement, call.data() + 1U, sizeof displacement);
    const std::intptr_t target = static_cast<std::intptr_t>(returned)
                                 + static_cast<std::intptr_t>(displacement);
    if (target != static_cast<std::intptr_t>(base + 0x324D90U)) {
        return rawRva;
    }
    direct = true;
    return rawRva - 5U;
}

[[nodiscard]] bool destroy_drift_binding(AllocationKey key,
                                         AllocationBinding& binding,
                                         RecordState& state,
                                         RecordToken& token,
                                         MemberBaseline& current) noexcept {
    binding = {};
    state = {};
    token = {};
    current = {};
    DestroyContext* const destroy = g_destroyContext;
    if (destroy == nullptr || !destroy->token) {
        return false;
    }
    for (Member member : {Member::receivedAuth,
                          Member::extractedSense,
                          Member::receivedSense}) {
        const std::size_t slot = member_index(member);
        if (slot >= destroy->currentMembers.size()
            || (destroy->currentMask & (1U << slot)) == 0U) {
            continue;
        }
        const MemberBaseline& candidate = destroy->currentMembers[slot];
        const std::uint64_t relative = key.kind == AllocationKeyKind::payload
                                           ? candidate.relativePayload
                                           : candidate.relativeHeader;
        if (candidate.heap != key.heap || relative != key.relative) {
            continue;
        }
        if (g_records.read_slot(destroy->token.slot,
                                destroy->token.generation,
                                state,
                                &token)
            != RegistryResult::success) {
            return false;
        }
        current = candidate;
        binding = AllocationBinding{token.slot, token.generation, member, false};
        return true;
    }
    return false;
}

__declspec(noinline) std::uint32_t* __fastcall sensor_table_insert(
    void* sensorTable,
    std::uint32_t* outDatum,
    const void* identity,
    std::int32_t authSchema,
    std::int32_t senseSchema) noexcept {
    FullCallScope call{};
    const SensorTableInsert original = hooking::await_original(g_insertOriginal);
    if (!call.observes()) {
        return original(sensorTable, outDatum, identity, authSchema, senseSchema);
    }
    InsertContext context{};
    context.sensorTable = reinterpret_cast<std::uintptr_t>(sensorTable);
    context.outDatum = outDatum;
    context.authSchema = static_cast<std::uint32_t>(authSchema);
    context.senseSchema = static_cast<std::uint32_t>(senseSchema);
    if (context.sensorTable >= 0x28U) {
        context.peerView = context.sensorTable - 0x28U;
    } else {
        account(g_header, FailureCounter::checkedArithmeticFailure);
    }
    if (!capture_identity(identity, context.identity)) {
        account(g_header, FailureCounter::guardVirtualQueryReject);
    }
    bool active = false;
    TlsScope scope{g_insertContext, context, g_header, active};
    std::uint32_t* const result =
        original(sensorTable, outDatum, identity, authSchema, senseSchema);
    if (context.constructed) {
        std::uint32_t datum{};
        const bool datumValid = outDatum != nullptr
                                && guarded_copy(reinterpret_cast<std::uintptr_t>(outDatum),
                                                &datum,
                                                sizeof datum);
        (void)g_records.update(
            context.constructed,
            [&](RecordState& state) noexcept {
                state.sensorTable = context.sensorTable;
                state.peerView = context.peerView;
                state.tableValid = context.sensorTable != 0U;
                state.datum = datum;
                state.datumValid = datumValid;
            },
            FailureCounter::staleRecordAssociate);
        RecordState state{};
        if (g_records.read(context.constructed.record, state, nullptr)
            == RegistryResult::success) {
            HeapCapture empty{};
            push_heap_event(EventKind::construct,
                            EventPhase::exit,
                            Member::none,
                            &state,
                            empty);
        }
    } else {
        account(g_header, FailureCounter::tlsMissingConstructor);
    }
    return result;
}

__declspec(noinline) void __fastcall record_construct(void* record,
                                                       std::uint16_t selector,
                                                       const void* identity,
                                                       std::int32_t authSchema,
                                                       std::int32_t senseSchema) noexcept {
    FullCallScope call{};
    const RecordConstruct original = hooking::await_original(g_constructOriginal);
    if (!call.observes()) {
        original(record, selector, identity, authSchema, senseSchema);
        return;
    }
    RecordState seed{};
    seed.record = reinterpret_cast<std::uintptr_t>(record);
    seed.authSchema = static_cast<std::uint32_t>(authSchema);
    seed.senseSchema = static_cast<std::uint32_t>(senseSchema);
    if (g_insertContext != nullptr) {
        seed.sensorTable = g_insertContext->sensorTable;
        seed.peerView = g_insertContext->peerView;
        seed.tableValid = seed.sensorTable != 0U;
        seed.identity = g_insertContext->identity;
    } else {
        account(g_header, FailureCounter::tlsMissingInsert);
        (void)capture_identity(identity, seed.identity);
    }
    if (seed.identity.valid) {
        seed.sensorKey = pack_sensor_identity(seed.identity.word,
                                              seed.identity.kind,
                                              seed.identity.index);
    }
    account(g_header, FailureCounter::lifecycleSnapshotUnavailable);
    RecordToken token{};
    const RegistryResult begun = g_records.begin(seed.record,
                                                  seed,
                                                  token,
                                                  &active_reuse_observer,
                                                  nullptr);
    ConstructorContext context{};
    context.token = token;
    if (g_insertContext != nullptr) {
        g_insertContext->constructed = token;
    }
    bool active = false;
    TlsScope scope{g_constructorContext, context, g_header, active};
    if (begun == RegistryResult::success) {
        RecordState state{};
        (void)g_records.read(token.record, state, nullptr);
        HeapCapture empty{};
        empty.payload.selector = selector;
        push_heap_event(EventKind::construct,
                        EventPhase::pre,
                        Member::none,
                        &state,
                        empty);
    }
    original(record, selector, identity, authSchema, senseSchema);
    if (begun != RegistryResult::success) {
        return;
    }
    if (context.seenMask != 0x07U) {
        account(g_header, FailureCounter::staleRecordCommit);
    }
    (void)g_records.commit(token);
    RecordState state{};
    if (g_records.read(token.record, state, nullptr) == RegistryResult::success) {
        HeapCapture empty{};
        push_heap_event(EventKind::construct,
                        EventPhase::post,
                        Member::none,
                        &state,
                        empty);
    }
}

__declspec(noinline) void __fastcall allocate_member(void* triple,
                                                      std::uint16_t selector,
                                                      std::int32_t decodedBytes,
                                                      const char* diagnosticName) noexcept {
    FullCallScope call{};
    const AllocateMember original = hooking::await_original(g_allocateOriginal);
    original(triple, selector, decodedBytes, diagnosticName);
    if (!call.observes()) {
        return;
    }
    if (g_constructorContext == nullptr || !g_constructorContext->token) {
        account(g_header, FailureCounter::tlsMissingConstructor);
        return;
    }
    const Member member = classify_member(g_constructorContext->token.record,
                                          reinterpret_cast<std::uintptr_t>(triple));
    const std::size_t slot = member_index(member);
    if (slot >= 3U) {
        return;
    }
    MemberBaseline baseline{};
    if (!capture_member_triple(g_constructorContext->token.record, member, baseline)) {
        account(g_header, FailureCounter::guardVirtualQueryReject);
        return;
    }
    const HeapResolver resolver = g_heapResolver.load(std::memory_order_acquire);
    if (resolver == nullptr) {
        account(g_header, FailureCounter::mappedTargetReadFailure);
        return;
    }
    baseline.heap = reinterpret_cast<std::uintptr_t>(resolver(baseline.selector));
    WindowsReader reader{};
    HeapCapture capture = capture_heap(reader,
                                       baseline.heap,
                                       baseline.relativePayload,
                                       baseline.count,
                                       baseline.selector);
    baseline.heapBase = static_cast<std::uintptr_t>(capture.payload.heapBase);
    baseline.relativeHeader = capture.payload.relativeHeader;
    baseline.headerWidth = capture.payload.headerWidth;
    baseline.valid = (capture.valid & (validHeap | validHeapBase | validHeader))
                     == (validHeap | validHeapBase | validHeader);
    g_constructorContext->members[slot] = baseline;
    g_constructorContext->seenMask |= static_cast<std::uint8_t>(1U << slot);
    (void)g_records.update(
        g_constructorContext->token,
        [&](RecordState& state) noexcept { state.members[slot] = baseline; },
        FailureCounter::staleRecordCommit);
    if (baseline.valid) {
        const AllocationBinding binding{g_constructorContext->token.slot,
                                        g_constructorContext->token.generation,
                                        member,
                                        false};
        (void)g_allocations.publish(
            AllocationKey{baseline.heap,
                          baseline.relativePayload,
                          AllocationKeyKind::payload},
            binding);
        (void)g_allocations.publish(
            AllocationKey{baseline.heap,
                          baseline.relativeHeader,
                          AllocationKeyKind::header},
            binding);
    }
    RecordState state{};
    if (g_records.read(g_constructorContext->token.record, state, nullptr)
        == RegistryResult::success) {
        push_heap_event(EventKind::allocation,
                        EventPhase::post,
                        member,
                        &state,
                        capture);
    }
}

__declspec(noinline) void* __fastcall receive_resolve(void* record) noexcept {
    FullCallScope call{};
    const ReceiveResolve original = hooking::await_original(g_resolveOriginal);
    void* const result = original(record);
    if (!call.observes()) {
        return result;
    }
    if (g_receiveContext == nullptr || !g_receiveContext->active) {
        account(g_header, FailureCounter::tlsMissingReceive);
        return result;
    }
    RecordState state{};
    RecordToken token{};
    if (g_records.read(reinterpret_cast<std::uintptr_t>(record), state, &token)
        != RegistryResult::success) {
        account(g_header, FailureCounter::recordReadRace);
        return result;
    }
    g_receiveContext->record = token.record;
    g_receiveContext->recordGeneration = token.generation;
    g_receiveContext->destination = reinterpret_cast<std::uintptr_t>(result);
    HeapCapture capture = capture_current_member(state, Member::receivedSense);
    std::uintptr_t expected{};
    if (checked_add(capture.payload.heapBase, capture.payload.relativePayload, expected)
        && expected != reinterpret_cast<std::uintptr_t>(result)) {
        capture.flags |= flagAbsoluteDestinationChanged;
    }
    push_heap_event(EventKind::resolve,
                    EventPhase::post,
                    Member::receivedSense,
                    &state,
                    capture,
                    0U,
                    0U,
                    validReceiveResolver);
    return result;
}

__declspec(noinline) void __fastcall initialize_state(std::uint32_t schema,
                                                       void* destination) noexcept {
    FullCallScope call{};
    const InitializeState original = hooking::await_original(g_initializeOriginal);
    const WriterAttribution attribution = call.observes() ? attribute_writer(destination)
                                                          : WriterAttribution{};
    if (attribution.valid) {
        const std::uint32_t preFlags = emit_writer_pair(attribution,
                                                        EventKind::initialize,
                                                        EventPhase::pre,
                                                        nullptr,
                                                        destination,
                                                        nullptr,
                                                        schema,
                                                        0U,
                                                        0U,
                                                        0U,
                                                        0U);
        original(schema, destination);
        const std::uint32_t postFlags = emit_writer_pair(attribution,
                                                         EventKind::initialize,
                                                         EventPhase::post,
                                                         nullptr,
                                                         destination,
                                                         nullptr,
                                                         schema,
                                                         0U,
                                                         0U,
                                                         0U,
                                                         0U);
        const FreezeReason reason = classify_failure(preFlags, postFlags);
        if (reason != FreezeReason::none) {
            (void)g_eventLedger.freeze(reason, qpc_now());
            g_flushRequested.store(true, std::memory_order_release);
        }
        return;
    }
    original(schema, destination);
}

__declspec(noinline) std::uint64_t __fastcall decode_state(void* stream,
                                                           void* destination,
                                                           std::int32_t* schemaKey,
                                                           std::uint32_t schema,
                                                           std::uint32_t nativeFlags) noexcept {
    FullCallScope call{};
    const DecodeState original = hooking::await_original(g_decodeOriginal);
    const WriterAttribution attribution = call.observes() ? attribute_writer(destination)
                                                          : WriterAttribution{};
    std::uint32_t bitsBefore{};
    std::uint32_t preFlags{};
    if (attribution.valid) {
        (void)capture_stream_bits(stream, bitsBefore);
        preFlags = emit_writer_pair(attribution,
                                    EventKind::decode,
                                    EventPhase::pre,
                                    stream,
                                    destination,
                                    schemaKey,
                                    schema,
                                    nativeFlags,
                                    bitsBefore,
                                    bitsBefore,
                                    0U);
    }
    if (call.observes()) {
        notify_decode_observers(DecodeObservation{stream,
                                                   destination,
                                                   schemaKey,
                                                   attribution.state.record,
                                                   0U,
                                                   schema,
                                                   nativeFlags,
                                                   bitsBefore,
                                                   bitsBefore,
                                                   attribution.state.generation,
                                                   EventPhase::pre,
                                                   attribution.member,
                                                   attribution.valid});
    }
    const std::uint64_t result = original(stream,
                                          destination,
                                          schemaKey,
                                          schema,
                                          nativeFlags);
    if (attribution.valid) {
        std::uint32_t bitsAfter{};
        (void)capture_stream_bits(stream, bitsAfter);
        const std::uint32_t postFlags = emit_writer_pair(attribution,
                                                         EventKind::decode,
                                                         EventPhase::post,
                                                         stream,
                                                         destination,
                                                         schemaKey,
                                                         schema,
                                                         nativeFlags,
                                                         bitsBefore,
                                                         bitsAfter,
                                                         result);
        const FreezeReason reason = classify_failure(preFlags, postFlags);
        if (reason != FreezeReason::none) {
            (void)g_eventLedger.freeze(reason, qpc_now());
            g_flushRequested.store(true, std::memory_order_release);
        }
        notify_decode_observers(DecodeObservation{stream,
                                                   destination,
                                                   schemaKey,
                                                   attribution.state.record,
                                                   result,
                                                   schema,
                                                   nativeFlags,
                                                   bitsBefore,
                                                   bitsAfter,
                                                   attribution.state.generation,
                                                   EventPhase::post,
                                                   attribution.member,
                                                   true});
    } else if (call.observes()) {
        std::uint32_t bitsAfter{};
        (void)capture_stream_bits(stream, bitsAfter);
        notify_decode_observers(DecodeObservation{stream,
                                                   destination,
                                                   schemaKey,
                                                   0U,
                                                   result,
                                                   schema,
                                                   nativeFlags,
                                                   bitsBefore,
                                                   bitsAfter,
                                                   0U,
                                                   EventPhase::post,
                                                   Member::none,
                                                   false});
    }
    return result;
}

__declspec(noinline) void __fastcall sensor_table_remove(void* sensorTable,
                                                         std::uint32_t datum) noexcept {
    FullCallScope call{};
    const SensorTableRemove original = hooking::await_original(g_removeOriginal);
    if (!call.observes()) {
        original(sensorTable, datum);
        return;
    }
    RemoveContext context{};
    context.sensorTable = reinterpret_cast<std::uintptr_t>(sensorTable);
    context.peerView = context.sensorTable >= 0x28U ? context.sensorTable - 0x28U : 0U;
    context.datum = datum;
    bool active = false;
    TlsScope scope{g_removeContext, context, g_header, active};
    original(sensorTable, datum);
    if (context.token) {
        RecordState state{};
        if (g_records.read(context.token.record, state, nullptr) == RegistryResult::success) {
            if (!state.tableValid || state.sensorTable != context.sensorTable
                || !state.datumValid || state.datum != datum) {
                account(g_header, FailureCounter::staleRecordRetire);
            }
            (void)g_records.retire(context.token);
            HeapCapture empty{};
            push_heap_event(EventKind::remove,
                            EventPhase::exit,
                            Member::none,
                            &state,
                            empty);
        }
    } else {
        account(g_header, FailureCounter::missingDestroySnapshot);
    }
}

__declspec(noinline) void __fastcall record_destroy(void* record) noexcept {
    FullCallScope call{};
    const RecordDestroy original = hooking::await_original(g_destroyOriginal);
    if (!call.observes()) {
        original(record);
        return;
    }
    DestroyContext context{};
    if (g_records.read(reinterpret_cast<std::uintptr_t>(record), context.state, &context.token)
        != RegistryResult::success) {
        account(g_header, FailureCounter::missingDestroySnapshot);
        original(record);
        return;
    }
    if (g_removeContext != nullptr) {
        g_removeContext->token = context.token;
    } else {
        account(g_header, FailureCounter::tlsMissingDestroy);
    }
    for (Member member : {Member::receivedSense,
                          Member::extractedSense,
                          Member::receivedAuth}) {
        HeapCapture capture = capture_current_member(context.state, member);
        push_heap_event(EventKind::destroy,
                        EventPhase::pre,
                        member,
                        &context.state,
                        capture,
                        destruction_ordinal(member),
                        0U,
                        validDestroyTls);
    }
    bool active = false;
    TlsScope scope{g_destroyContext, context, g_header, active};
    original(record);
    for (Member member : {Member::receivedSense,
                          Member::extractedSense,
                          Member::receivedAuth}) {
        MemberBaseline current{};
        const std::size_t slot = member_index(member);
        HeapCapture capture{};
        if (!capture_member_triple(context.state.record, member, current)) {
            capture.flags |= flagGuardedReadFault;
        } else {
            capture.payload.count = current.count;
            capture.payload.relativePayload = current.relativePayload;
            capture.payload.selector = current.selector;
            capture.valid |= validMemberTriple;
            const MemberBaseline& baseline = context.state.members[slot];
            if (current.count != 0U || current.relativePayload != 0U) {
                account(g_header, FailureCounter::postDestroyCountRelativeFailure);
                capture.flags |= flagCountOrExtentMismatch;
            }
            if (baseline.valid && current.selector != baseline.selector) {
                account(g_header, FailureCounter::postDestroySelectorFailure);
                capture.flags |= flagSelectorChanged;
            }
            if (baseline.valid) {
                (void)g_allocations.mark_retired(
                    AllocationKey{baseline.heap,
                                  baseline.relativePayload,
                                  AllocationKeyKind::payload},
                    context.token.generation);
                (void)g_allocations.mark_retired(
                    AllocationKey{baseline.heap,
                                  baseline.relativeHeader,
                                  AllocationKeyKind::header},
                    context.token.generation);
            }
        }
        push_heap_event(EventKind::destroy,
                        EventPhase::post,
                        member,
                        &context.state,
                        capture,
                        destruction_ordinal(member),
                        0U,
                        validDestroyTls);
    }
}

__declspec(noinline) void __fastcall relative_free(void* heap,
                                                   std::uint64_t relativePayload) noexcept {
    FullCallScope call{};
    const RelativeFree original = hooking::await_original(g_freeOriginal);
    if (!call.observes()) {
        original(heap, relativePayload);
        return;
    }
    const AllocationKey key{reinterpret_cast<std::uintptr_t>(heap),
                            relativePayload,
                            AllocationKeyKind::payload};
    AllocationBinding binding{};
    const RegistryResult found = g_allocations.lookup(key, binding);
    if (found != RegistryResult::success) {
        if (g_destroyContext != nullptr) {
            account(g_header, FailureCounter::trackedUnmatchedFree);
        }
        original(heap, relativePayload);
        return;
    }
    RecordState state{};
    RecordToken token{};
    if (g_records.read_slot(binding.recordSlot,
                            binding.recordGeneration,
                            state,
                            &token)
        != RegistryResult::success) {
        account(g_header, FailureCounter::allocationIndexStaleBinding);
        original(heap, relativePayload);
        return;
    }
    std::uint32_t freeOrdinal{};
    const RegistryResult ticket = g_records.next_free_ordinal(token,
                                                               binding.member,
                                                               freeOrdinal);
    HeapCapture capture = capture_current_member(state, binding.member);
    capture.flags |= flagTargetAllocationTracked;
    bool direct = false;
    std::uint32_t rawRva{};
    const std::uint32_t callerRva = direct_caller_rva(_ReturnAddress(), direct, rawRva);
    if (freeOrdinal > 1U) {
        capture.flags |= flagDuplicateFree;
    }
    if (g_destroyContext != nullptr) {
        const std::uint8_t ordinal = destruction_ordinal(binding.member);
        if (ordinal != g_destroyContext->nextOrdinal) {
            account(g_header, FailureCounter::destructorMemberOrderFailure);
        }
        g_destroyContext->nextOrdinal = static_cast<std::uint8_t>(ordinal + 1U);
    }
    push_heap_event(EventKind::free,
                    EventPhase::pre,
                    binding.member,
                    &state,
                    capture,
                    freeOrdinal,
                    callerRva,
                    (direct ? validDirectCallsite : 0U)
                        | (ticket == RegistryResult::success ? validFreeOrdinal : 0U)
                        | (g_destroyContext != nullptr ? validDestroyTls : 0U),
                    0U,
                    freeOrdinal);
    if (freeOrdinal > 1U) {
        (void)g_eventLedger.freeze(FreezeReason::duplicateFree, qpc_now());
        g_flushRequested.store(true, std::memory_order_release);
    }
    original(heap, relativePayload);
}

__declspec(noinline) void __fastcall index_unlink(void* heap,
                                                  std::uint64_t relativeHeader) noexcept {
    FullCallScope call{};
    const IndexUnlink original = hooking::await_original(g_unlinkOriginal);
    if (!call.observes()) {
        original(heap, relativeHeader);
        return;
    }
    const AllocationKey key{reinterpret_cast<std::uintptr_t>(heap),
                            relativeHeader,
                            AllocationKeyKind::header};
    AllocationBinding binding{};
    if (g_allocations.lookup(key, binding) != RegistryResult::success) {
        if (g_destroyContext != nullptr) {
            account(g_header, FailureCounter::trackedUnmatchedUnlink);
        }
        original(heap, relativeHeader);
        return;
    }
    RecordState state{};
    RecordToken token{};
    if (g_records.read_slot(binding.recordSlot,
                            binding.recordGeneration,
                            state,
                            &token)
        != RegistryResult::success) {
        account(g_header, FailureCounter::allocationIndexStaleBinding);
        original(heap, relativeHeader);
        return;
    }
    const std::size_t slot = member_index(binding.member);
    const MemberBaseline baseline = state.members[slot];
    WindowsReader reader{};
    HeapCapture capture = capture_heap(reader,
                                       reinterpret_cast<std::uintptr_t>(heap),
                                       baseline.relativePayload,
                                       baseline.count,
                                       baseline.selector);
    capture.flags |= flagTargetAllocationTracked;
    push_heap_event(EventKind::unlink,
                    EventPhase::pre,
                    binding.member,
                    &state,
                    capture,
                    0U,
                    0U,
                    g_destroyContext != nullptr ? validDestroyTls : 0U);
    if ((capture.flags & flagGuardedReadFault) != 0U) {
        (void)g_eventLedger.freeze(FreezeReason::trackedReadFailure, qpc_now());
        g_flushRequested.store(true, std::memory_order_release);
    } else if ((capture.flags & flagPreviousPredicateBad) != 0U) {
        (void)g_eventLedger.freeze(FreezeReason::badPreviousPredicate, qpc_now());
        g_flushRequested.store(true, std::memory_order_release);
    } else if ((capture.flags & flagNextPredicateBad) != 0U) {
        (void)g_eventLedger.freeze(FreezeReason::badNextPredicate, qpc_now());
        g_flushRequested.store(true, std::memory_order_release);
    }
    original(heap, relativeHeader);
}

struct TableSummary final {
    std::uintptr_t table{};
    std::uint32_t active{};
};

struct GlobalDropFrame final {
    std::array<TableSummary, kDropTableCapacity> before{};
    std::size_t beforeCount{};
};

thread_local std::array<GlobalDropFrame, 4U> g_globalDropFrames{};
thread_local std::size_t g_globalDropDepth{};

[[nodiscard]] std::size_t capture_table_summaries(
    std::array<TableSummary, kDropTableCapacity>& output) noexcept {
    std::size_t count = 0U;
    g_records.visit_active([&](const RecordState& state) noexcept {
        if (!state.tableValid) {
            return;
        }
        for (std::size_t index = 0U; index < count; ++index) {
            if (output[index].table == state.sensorTable) {
                ++output[index].active;
                return;
            }
        }
        if (count == output.size()) {
            account(g_header, FailureCounter::dropTableSummaryOverflow);
            return;
        }
        output[count++] = TableSummary{state.sensorTable, 1U};
    });
    return count;
}

void emit_drop_summaries(EventPhase phase,
                         std::span<const TableSummary> before,
                         std::span<const TableSummary> after) noexcept {
    for (std::size_t index = 0U; index < before.size(); ++index) {
        EventClaim claim = g_eventLedger.claim_normal();
        if (!claim) {
            return;
        }
        EventV1& event = *claim.first;
        prepare_event(event, EventKind::drop, phase, Member::none, EventForm::dropSummary);
        event.peerTable = before[index].table;
        event.valid |= validPeerTable | validPeerViewDerived;
        DropSummaryPayload& payload = event.payload.dropSummary;
        payload.peerView = before[index].table >= 0x28U ? before[index].table - 0x28U : 0U;
        payload.activeBefore = before[index].active;
        for (const TableSummary& candidate : after) {
            if (candidate.table == before[index].table) {
                payload.activeAfter = candidate.active;
                break;
            }
        }
        payload.tableCount = static_cast<std::uint32_t>(before.size());
        payload.distinctTables = static_cast<std::uint32_t>(before.size());
        payload.recordHighWater = g_records.high_water();
        payload.attachMask = g_header != nullptr
                                 ? g_header->attachMask.load(std::memory_order_acquire)
                                 : 0U;
        g_eventLedger.commit(event, claim.firstSequence);
    }
}

__declspec(noinline) void heap_global_drop_pre(
    void*,
    const activity_lifecycle::NativeActivationGlobalDropCohort& cohort) noexcept {
    FullCallScope call{};
    if (!call.observes()) {
        return;
    }
    if (g_globalDropDepth == g_globalDropFrames.size()) {
        account(g_header, FailureCounter::tlsMisnest);
        return;
    }
    if (!cohort.tokenValid) {
        account(g_header, FailureCounter::lifecycleSnapshotUnavailable);
    }
    GlobalDropFrame& frame = g_globalDropFrames[g_globalDropDepth++];
    frame = {};
    frame.beforeCount = capture_table_summaries(frame.before);
    emit_drop_summaries(EventPhase::pre,
                        {frame.before.data(), frame.beforeCount},
                        {});
}

__declspec(noinline) void heap_global_drop_post(
    void*,
    const activity_lifecycle::NativeActivationGlobalDropCohort& cohort) noexcept {
    FullCallScope call{};
    if (!call.observes()) {
        return;
    }
    if (g_globalDropDepth == 0U) {
        account(g_header, FailureCounter::tlsMisnest);
        return;
    }
    if (!cohort.tokenValid) {
        account(g_header, FailureCounter::lifecycleSnapshotUnavailable);
    }
    GlobalDropFrame& frame = g_globalDropFrames[--g_globalDropDepth];
    std::array<TableSummary, kDropTableCapacity> after{};
    std::size_t afterCount = capture_table_summaries(after);
    emit_drop_summaries(EventPhase::post,
                        {frame.before.data(), frame.beforeCount},
                        {after.data(), afterCount});
    frame = {};
}

const activity_lifecycle::NativeActivationGlobalDropObserverTable g_globalDropCallbackTable{
    activity_lifecycle::kNativeActivationGlobalDropObserverAbiVersion,
    nullptr,
    &heap_global_drop_pre,
    &heap_global_drop_post,
};

__declspec(noinline) void __fastcall network_reset() noexcept {
    FullCallScope call{};
    const NetworkReset original = hooking::await_original(g_resetOriginal);
    if (!call.observes()) {
        original();
        return;
    }
    ResetContext context{};
    if (g_header != nullptr) {
        std::uint64_t value = g_header->dropGeneration.load(std::memory_order_acquire);
        if (value != (std::numeric_limits<std::uint64_t>::max)()
            && g_header->dropGeneration.compare_exchange_strong(
                value, value + 1U, std::memory_order_acq_rel, std::memory_order_acquire)) {
            context.dropGeneration = value + 1U;
            context.valid = true;
        } else {
            account(g_header, FailureCounter::tlsMissingDrop);
        }
    }
    bool active = false;
    TlsScope scope{g_resetContext, context, g_header, active};
    HeapCapture empty{};
    push_heap_event(EventKind::reset, EventPhase::enter, Member::none, nullptr, empty);
    original();
    push_heap_event(EventKind::reset, EventPhase::exit, Member::none, nullptr, empty);
}

[[nodiscard]] bool all_handles_attached() noexcept {
    return std::all_of(g_handles.begin(), g_handles.end(), [](const auto& handle) {
        return handle.attached && handle.original != nullptr;
    });
}

[[nodiscard]] bool any_handle_attached() noexcept {
    return std::any_of(g_handles.begin(), g_handles.end(), [](const auto& handle) {
        return handle.attached;
    });
}

void clear_originals() noexcept {
    g_insertOriginal.store(nullptr, std::memory_order_release);
    g_constructOriginal.store(nullptr, std::memory_order_release);
    g_allocateOriginal.store(nullptr, std::memory_order_release);
    g_resolveOriginal.store(nullptr, std::memory_order_release);
    g_initializeOriginal.store(nullptr, std::memory_order_release);
    g_decodeOriginal.store(nullptr, std::memory_order_release);
    g_removeOriginal.store(nullptr, std::memory_order_release);
    g_destroyOriginal.store(nullptr, std::memory_order_release);
    g_freeOriginal.store(nullptr, std::memory_order_release);
    g_unlinkOriginal.store(nullptr, std::memory_order_release);
    g_resetOriginal.store(nullptr, std::memory_order_release);
    g_heapResolver.store(nullptr, std::memory_order_release);
}

[[nodiscard]] bool detach_idle() noexcept { return calls_idle(); }

[[nodiscard]] bool decode_observers_retained() noexcept {
    return std::any_of(g_decodeObservers.begin(), g_decodeObservers.end(), [](const auto& slot) {
        return slot.state.load(std::memory_order_acquire) != 0U
               || slot.inFlight.load(std::memory_order_acquire) != 0U;
    });
}

void refresh_readiness() noexcept {
    if (g_header == nullptr || !all_handles_attached()) {
        return;
    }
    const bool ready = g_receiveOwnerAttached.load(std::memory_order_acquire)
                       && g_globalDropOwnerAttached.load(std::memory_order_acquire)
                       && g_header->attachMask.load(std::memory_order_acquire) == kFullAttachMask;
    const Readiness next = ready ? Readiness::ready : Readiness::coreAttached;
    g_header->readinessState.store(static_cast<std::uint32_t>(next),
                                   std::memory_order_release);
    g_readiness.store(next, std::memory_order_release);
}

[[nodiscard]] const char* kind_name(std::uint8_t value) noexcept {
    switch (static_cast<EventKind>(value)) {
    case EventKind::construct: return "construct";
    case EventKind::allocation: return "allocation";
    case EventKind::resolve: return "resolve";
    case EventKind::initialize: return "initialize";
    case EventKind::decode: return "decode";
    case EventKind::remove: return "remove";
    case EventKind::destroy: return "destroy";
    case EventKind::free: return "free";
    case EventKind::unlink: return "unlink";
    case EventKind::drop: return "drop";
    case EventKind::reset: return "reset";
    case EventKind::assertion: return "assertion";
    case EventKind::control: return "control";
    default: return "unknown";
    }
}

[[nodiscard]] const char* phase_name(std::uint8_t value) noexcept {
    switch (static_cast<EventPhase>(value)) {
    case EventPhase::enter: return "enter";
    case EventPhase::pre: return "pre";
    case EventPhase::post: return "post";
    case EventPhase::exit: return "exit";
    default: return "unknown";
    }
}

[[nodiscard]] const char* member_name(std::uint8_t value) noexcept {
    switch (static_cast<Member>(value)) {
    case Member::receivedAuth: return "received_auth";
    case Member::extractedSense: return "extracted_sense";
    case Member::receivedSense: return "received_sense";
    default: return "none";
    }
}

[[nodiscard]] const char* form_name(std::uint8_t value) noexcept {
    switch (static_cast<EventForm>(value)) {
    case EventForm::heapSnapshot: return "heap";
    case EventForm::writerMeta: return "meta";
    case EventForm::writerLinks: return "links";
    case EventForm::dropSummary: return "drop";
    case EventForm::control: return "control";
    default: return "unknown";
    }
}

[[nodiscard]] bool bounded_contains(const char* text,
                                    std::size_t length,
                                    std::string_view needle) noexcept {
    if (text == nullptr || needle.empty() || needle.size() > length) {
        return false;
    }
    for (std::size_t offset = 0U; offset <= length - needle.size(); ++offset) {
        if (std::memcmp(text + offset, needle.data(), needle.size()) == 0) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool claim_u32(std::atomic<std::uint32_t>& storage) noexcept {
    std::uint32_t value = storage.load(std::memory_order_acquire);
    return value != (std::numeric_limits<std::uint32_t>::max)()
           && storage.compare_exchange_strong(
               value, value + 1U, std::memory_order_acq_rel, std::memory_order_acquire);
}

void project_event(const EventV1& event, std::uint64_t sequence) noexcept {
    const EventForm form = static_cast<EventForm>(event.form);
    HeapSnapshotPayload heap{};
    std::uint64_t stream{};
    std::uint64_t nativeResult{};
    std::uint32_t schema{};
    std::uint32_t bitsBefore{};
    std::uint32_t bitsAfter{};
    if (form == EventForm::writerMeta) {
        const WriterMetaPayload& meta = event.payload.writerMeta;
        stream = meta.stream;
        nativeResult = meta.nativeResult;
        schema = meta.schema;
        bitsBefore = meta.bitsBefore;
        bitsAfter = meta.bitsAfter;
        heap.relativePayload = meta.relativePayload;
        heap.heap = meta.heap;
        heap.heapBase = meta.heapBase;
        heap.relativeHeader = meta.relativeHeader;
        heap.payloadHash = meta.payloadHash;
        heap.recordHash = meta.recordHash;
        heap.count = meta.count;
        heap.selector = meta.selector;
        heap.headerWidth = meta.headerWidth;
        heap.invariant = meta.invariant;
    } else if (form == EventForm::heapSnapshot || form == EventForm::writerLinks) {
        heap = event.payload.heapSnapshot;
        schema = heap.schema;
        bitsBefore = heap.bitsBefore;
        bitsAfter = heap.bitsAfter;
    }
    std::uintptr_t peerView{};
    if ((event.valid & validPeerViewDerived) != 0U && event.peerTable >= 0x28U) {
        peerView = static_cast<std::uintptr_t>(event.peerTable - 0x28U);
    }
    std::uintptr_t absolutePayload{};
    std::uintptr_t header{};
    if ((event.valid & (validHeapBase | validPayloadRange))
            == (validHeapBase | validPayloadRange)) {
        (void)checked_add(static_cast<std::uintptr_t>(heap.heapBase),
                          heap.relativePayload,
                          absolutePayload);
    }
    if ((event.valid & (validHeapBase | validHeader)) == (validHeapBase | validHeader)) {
        (void)checked_add(static_cast<std::uintptr_t>(heap.heapBase),
                          heap.relativeHeader,
                          header);
    }
    std::array<char, 2048U> line{};
    const int written = std::snprintf(
        line.data(),
        line.size(),
        "ev=sensor_heap v=1 seq=%llu kind=%s phase=%s form=%s member=%s tick=%llu "
        "qpc=%llu tid=%u caller_rva=%08X valid=%08X flags=%08X activity_gen=%llu "
        "connection_gen=%llu region_gen=%llu drop_gen=%llu timeout_ordinal=%u "
        "resume_ordinal=%u peer_table=%llX peer_view=%llX datum=%u record=%llX "
        "record_gen=%u sensor_key=%016llX owner=%u auth_schema=%u sense_schema=%u "
        "schema=%u stream=%llX bits_before=%u bits_after=%u native_result=%llX count=%u "
        "relative_payload=%llX selector=%u heap=%llX heap_base=%llX absolute_payload=%llX "
        "header_width=%u relative_header=%llX header=%llX size_flags=%08X next=%llX "
        "previous=%llX previous_next=%llX next_previous=%llX head=%llX tail=%llX "
        "invariant=%u payload_hash=%016llX record_hash=%016llX free_ordinal=%u",
        static_cast<unsigned long long>(sequence),
        kind_name(event.kind),
        phase_name(event.phase),
        form_name(event.form),
        member_name(event.member),
        static_cast<unsigned long long>(event.tick),
        static_cast<unsigned long long>(event.qpc),
        event.threadId,
        event.callerRva,
        event.valid,
        event.flags,
        static_cast<unsigned long long>(event.activityGeneration),
        static_cast<unsigned long long>(event.connectionGeneration),
        static_cast<unsigned long long>(event.regionGeneration),
        static_cast<unsigned long long>(event.dropGeneration),
        event.timeoutOrdinal,
        event.resumeOrdinal,
        static_cast<unsigned long long>(event.peerTable),
        static_cast<unsigned long long>(peerView),
        event.datum,
        static_cast<unsigned long long>(event.record),
        event.recordGeneration,
        static_cast<unsigned long long>(event.sensorKey),
        0U,
        event.authSchema,
        event.senseSchema,
        schema,
        static_cast<unsigned long long>(stream),
        bitsBefore,
        bitsAfter,
        static_cast<unsigned long long>(nativeResult),
        heap.count,
        static_cast<unsigned long long>(heap.relativePayload),
        static_cast<unsigned>(heap.selector),
        static_cast<unsigned long long>(heap.heap),
        static_cast<unsigned long long>(heap.heapBase),
        static_cast<unsigned long long>(absolutePayload),
        static_cast<unsigned>(heap.headerWidth),
        static_cast<unsigned long long>(heap.relativeHeader),
        static_cast<unsigned long long>(header),
        heap.sizeFlags,
        static_cast<unsigned long long>(heap.next),
        static_cast<unsigned long long>(heap.previous),
        static_cast<unsigned long long>(heap.previousNext),
        static_cast<unsigned long long>(heap.nextPrevious),
        static_cast<unsigned long long>(heap.head),
        static_cast<unsigned long long>(heap.tail),
        static_cast<unsigned>(heap.invariant),
        static_cast<unsigned long long>(heap.payloadHash),
        static_cast<unsigned long long>(heap.recordHash),
        heap.freeOrdinal);
    if (written > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(),
                          (std::min)(static_cast<std::size_t>(written), line.size() - 1U)});
    }
}

} // namespace

bool install() noexcept {
    if (!core::settings::get().omegaExperiments.unsafeDiagnostics) {
        g_readiness.store(Readiness::disabled, std::memory_order_release);
        return true;
    }
    if (!retail_log::is_installed() || !assert_handler::is_installed()) {
        g_readiness.store(Readiness::unavailable, std::memory_order_release);
        report_install_failure("dependency");
        return false;
    }

    AcquireSRWLockExclusive(&g_lifecycleLock);
    if (all_handles_attached() && g_header != nullptr) {
        const bool ready = g_readiness.load(std::memory_order_acquire) == Readiness::ready;
        ReleaseSRWLockExclusive(&g_lifecycleLock);
        return ready;
    }
    if (any_handle_attached() || g_ledgerView != nullptr || g_header != nullptr) {
        ReleaseSRWLockExclusive(&g_lifecycleLock);
        report_install_failure("retained_ownership");
        return false;
    }

    const HMODULE module = GetModuleHandleW(nullptr);
    diagnostics::ModuleRange range{};
    ImageView image{};
    bool admission = module != nullptr && diagnostics::module_range(module, range)
                     && range.end > range.base;
    if (admission) {
        image.mapped = {reinterpret_cast<const std::byte*>(range.base),
                        static_cast<std::size_t>(range.end - range.base)};
        admission = current_executable_identity(module,
                                                image.packedSha256,
                                                image.packedFileSize)
                    && mapped_pe_identity(image.mapped, image);
    }
    std::array<std::uintptr_t, kCoreHookCount> targets{};
    std::uintptr_t resolver{};
    std::array<std::array<std::byte, 17U>, kSiteCount> observed{};
    ImageValidationResult validation = ImageValidationResult::peMismatch;
    if (admission) {
        validation = validate_image(image, targets, resolver, observed);
        admission = validation == ImageValidationResult::valid;
    }
    if (!admission) {
        g_readiness.store(Readiness::unavailable, std::memory_order_release);
        ReleaseSRWLockExclusive(&g_lifecycleLock);
        report_install_failure(validation_name(validation));
        return false;
    }
    if (!create_ledger(image, observed)) {
        close_ledger();
        g_readiness.store(Readiness::unavailable, std::memory_order_release);
        ReleaseSRWLockExclusive(&g_lifecycleLock);
        report_install_failure("ledger");
        return false;
    }
    const std::array specs{
        hooking::detour::Spec{reinterpret_cast<void*>(targets[index(HookSlot::insert)]),
                              reinterpret_cast<void*>(&sensor_table_insert)},
        hooking::detour::Spec{reinterpret_cast<void*>(targets[index(HookSlot::construct)]),
                              reinterpret_cast<void*>(&record_construct)},
        hooking::detour::Spec{reinterpret_cast<void*>(targets[index(HookSlot::allocate)]),
                              reinterpret_cast<void*>(&allocate_member)},
        hooking::detour::Spec{reinterpret_cast<void*>(targets[index(HookSlot::resolve)]),
                              reinterpret_cast<void*>(&receive_resolve)},
        hooking::detour::Spec{reinterpret_cast<void*>(targets[index(HookSlot::initialize)]),
                              reinterpret_cast<void*>(&initialize_state)},
        hooking::detour::Spec{reinterpret_cast<void*>(targets[index(HookSlot::decode)]),
                              reinterpret_cast<void*>(&decode_state)},
        hooking::detour::Spec{reinterpret_cast<void*>(targets[index(HookSlot::remove)]),
                              reinterpret_cast<void*>(&sensor_table_remove)},
        hooking::detour::Spec{reinterpret_cast<void*>(targets[index(HookSlot::destroy)]),
                              reinterpret_cast<void*>(&record_destroy)},
        hooking::detour::Spec{reinterpret_cast<void*>(targets[index(HookSlot::free)]),
                              reinterpret_cast<void*>(&relative_free)},
        hooking::detour::Spec{reinterpret_cast<void*>(targets[index(HookSlot::unlink)]),
                              reinterpret_cast<void*>(&index_unlink)},
        hooking::detour::Spec{reinterpret_cast<void*>(targets[index(HookSlot::reset)]),
                              reinterpret_cast<void*>(&network_reset)},
    };
    if (!hooking::detour::install(specs, g_handles)) {
        account(g_header, FailureCounter::attachFailure);
        close_ledger();
        g_handles = {};
        g_readiness.store(Readiness::unavailable, std::memory_order_release);
        ReleaseSRWLockExclusive(&g_lifecycleLock);
        report_install_failure("attach");
        return false;
    }
    hooking::publish_original(
        g_insertOriginal,
        reinterpret_cast<SensorTableInsert>(g_handles[index(HookSlot::insert)].original));
    hooking::publish_original(
        g_constructOriginal,
        reinterpret_cast<RecordConstruct>(g_handles[index(HookSlot::construct)].original));
    hooking::publish_original(
        g_allocateOriginal,
        reinterpret_cast<AllocateMember>(g_handles[index(HookSlot::allocate)].original));
    hooking::publish_original(
        g_resolveOriginal,
        reinterpret_cast<ReceiveResolve>(g_handles[index(HookSlot::resolve)].original));
    hooking::publish_original(
        g_initializeOriginal,
        reinterpret_cast<InitializeState>(g_handles[index(HookSlot::initialize)].original));
    hooking::publish_original(
        g_decodeOriginal,
        reinterpret_cast<DecodeState>(g_handles[index(HookSlot::decode)].original));
    hooking::publish_original(
        g_removeOriginal,
        reinterpret_cast<SensorTableRemove>(g_handles[index(HookSlot::remove)].original));
    hooking::publish_original(
        g_destroyOriginal,
        reinterpret_cast<RecordDestroy>(g_handles[index(HookSlot::destroy)].original));
    hooking::publish_original(
        g_freeOriginal,
        reinterpret_cast<RelativeFree>(g_handles[index(HookSlot::free)].original));
    hooking::publish_original(
        g_unlinkOriginal,
        reinterpret_cast<IndexUnlink>(g_handles[index(HookSlot::unlink)].original));
    hooking::publish_original(
        g_resetOriginal,
        reinterpret_cast<NetworkReset>(g_handles[index(HookSlot::reset)].original));
    g_heapResolver.store(reinterpret_cast<HeapResolver>(resolver), std::memory_order_release);
    g_header->attachMask.store(kCoreAttachMask, std::memory_order_release);
    g_header->readinessState.store(static_cast<std::uint32_t>(Readiness::coreAttached),
                                   std::memory_order_release);
    g_readiness.store(Readiness::coreAttached, std::memory_order_release);
    g_callGate.accept();
    ReleaseSRWLockExclusive(&g_lifecycleLock);

    core::log::write(core::log::Channel::client,
                     core::log::Level::info,
                     "ev=sensor_heap v=1 stage=install result=core_attached hooks=11 "
                     "global_drop_owner=pending receive_owner=pending native_mutation=none");
    return true;
}

void quiesce() noexcept {
    g_callGate.quiesce();
    const Readiness current = g_readiness.load(std::memory_order_acquire);
    if (current != Readiness::disabled && current != Readiness::unavailable) {
        g_readiness.store(Readiness::quiescing, std::memory_order_release);
        if (g_header != nullptr) {
            g_header->readinessState.store(static_cast<std::uint32_t>(Readiness::quiescing),
                                           std::memory_order_release);
        }
    }
}

bool uninstall() noexcept {
    AcquireSRWLockExclusive(&g_lifecycleLock);
    if (g_receiveOwnerAttached.load(std::memory_order_acquire)
        || g_globalDropOwnerAttached.load(std::memory_order_acquire)
        || static_cast<bool>(g_globalDropObserverHandle) || decode_observers_retained()) {
        if (g_header != nullptr) {
            account(g_header, FailureCounter::detachDeferred);
            g_header->detachResult.store(static_cast<std::uint32_t>(DetachResult::deferred),
                                         std::memory_order_release);
        }
        ReleaseSRWLockExclusive(&g_lifecycleLock);
        return false;
    }
    g_callGate.quiesce();
    g_readiness.store(Readiness::quiescing, std::memory_order_release);
    if (any_handle_attached()) {
        const std::array protectedEntries{
            hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&sensor_table_insert)},
            hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&record_construct)},
            hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&allocate_member)},
            hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&receive_resolve)},
            hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&initialize_state)},
            hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&decode_state)},
            hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&sensor_table_remove)},
            hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&record_destroy)},
            hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&relative_free)},
            hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&index_unlink)},
            hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&network_reset)},
            hooking::detour::ProtectedCodeEntry{
                reinterpret_cast<void*>(&hooking::call_gate_detail::enter)},
            hooking::detour::ProtectedCodeEntry{
                reinterpret_cast<void*>(&hooking::call_gate_detail::leave)},
        };
        const hooking::detour::UninstallResult result =
            hooking::detour::uninstall(g_handles, protectedEntries, &detach_idle);
        if (result != hooking::detour::UninstallResult::removed) {
            if (g_header != nullptr) {
                account(g_header,
                        result == hooking::detour::UninstallResult::protectedCodeActive
                            ? FailureCounter::detachDeferred
                            : FailureCounter::detachFailure);
                g_header->detachResult.store(
                    static_cast<std::uint32_t>(
                        result == hooking::detour::UninstallResult::protectedCodeActive
                            ? DetachResult::deferred
                            : DetachResult::failed),
                    std::memory_order_release);
            }
            ReleaseSRWLockExclusive(&g_lifecycleLock);
            return false;
        }
        clear_originals();
        if (g_header != nullptr) {
            g_header->attachMask.store(0U, std::memory_order_release);
            g_header->detachResult.store(static_cast<std::uint32_t>(DetachResult::removed),
                                         std::memory_order_release);
        }
    }
    if (!calls_idle()) {
        account(g_header, FailureCounter::nonzeroInflightAtFinalize);
        ReleaseSRWLockExclusive(&g_lifecycleLock);
        return false;
    }
    (void)g_eventLedger.freeze(FreezeReason::lifecycleStop, qpc_now());
    if (g_header != nullptr) {
        g_header->flushState.store(static_cast<std::uint32_t>(FlushState::requested),
                                   std::memory_order_release);
    }
    const bool flushed = durable_flush(false) && durable_flush(true);
    if (!flushed) {
        ReleaseSRWLockExclusive(&g_lifecycleLock);
        return false;
    }
    if (g_header != nullptr) {
        g_header->flushState.store(static_cast<std::uint32_t>(FlushState::durable),
                                   std::memory_order_release);
        (void)durable_flush(true);
    }
    std::array<char, 512U> summary{};
    const int written = std::snprintf(
        summary.data(),
        summary.size(),
        "ev=sensor_heap v=1 stage=uninstall result=removed events=%llu dropped=%llu "
        "records=%u allocation_keys=%u verdict=%u",
        static_cast<unsigned long long>(
            g_header != nullptr
                ? g_header->committedCount.load(std::memory_order_acquire)
                : 0U),
        static_cast<unsigned long long>(
            g_header != nullptr ? g_header->droppedCount.load(std::memory_order_acquire) : 0U),
        g_records.high_water(),
        g_allocations.high_water(),
        g_header != nullptr ? g_header->verdict.load(std::memory_order_acquire) : 0U);
    close_ledger();
    g_records.reset();
    g_allocations.reset();
    g_receiveOwnerQuiescing.store(false, std::memory_order_release);
    g_globalDropOwnerQuiescing.store(false, std::memory_order_release);
    g_readiness.store(Readiness::disabled, std::memory_order_release);
    ReleaseSRWLockExclusive(&g_lifecycleLock);
    if (written > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {summary.data(),
                          (std::min)(static_cast<std::size_t>(written), summary.size() - 1U)});
    }
    return true;
}

bool has_ownership() noexcept {
    return any_handle_attached() || g_ledgerView != nullptr || g_header != nullptr
           || g_heapResolver.load(std::memory_order_acquire) != nullptr
           || g_receiveOwnerAttached.load(std::memory_order_acquire)
           || g_globalDropOwnerAttached.load(std::memory_order_acquire)
           || static_cast<bool>(g_globalDropObserverHandle)
           || decode_observers_retained()
           || g_receiveInflight.load(std::memory_order_acquire) != 0U;
}

Readiness readiness() noexcept { return g_readiness.load(std::memory_order_acquire); }

bool register_decode_observer(DecodeObserver observer,
                              void* context,
                              DecodeObserverHandle& output) noexcept {
    output = {};
    if (observer == nullptr || g_readiness.load(std::memory_order_acquire) != Readiness::ready) {
        return false;
    }
    for (std::size_t slotIndex = 0U; slotIndex < g_decodeObservers.size(); ++slotIndex) {
        DecodeObserverSlot& slot = g_decodeObservers[slotIndex];
        std::uint32_t free = 0U;
        if (!slot.state.compare_exchange_strong(
                free, 2U, std::memory_order_acq_rel, std::memory_order_acquire)) {
            continue;
        }
        std::uint32_t generation = slot.generation.load(std::memory_order_acquire);
        if (generation == (std::numeric_limits<std::uint32_t>::max)()) {
            slot.state.store(0U, std::memory_order_release);
            return false;
        }
        ++generation;
        slot.generation.store(generation, std::memory_order_release);
        slot.context.store(context, std::memory_order_release);
        slot.observer.store(observer, std::memory_order_release);
        slot.state.store(1U, std::memory_order_release);
        output = DecodeObserverHandle{static_cast<std::uint32_t>(slotIndex), generation};
        return true;
    }
    return false;
}

bool unregister_decode_observer(DecodeObserverHandle handle) noexcept {
    if (!handle || handle.slot >= g_decodeObservers.size()) {
        return false;
    }
    DecodeObserverSlot& slot = g_decodeObservers[handle.slot];
    if (slot.generation.load(std::memory_order_acquire) != handle.generation) {
        return false;
    }
    std::uint32_t state = slot.state.load(std::memory_order_acquire);
    if (state == 1U
        && !slot.state.compare_exchange_strong(
            state, 2U, std::memory_order_acq_rel, std::memory_order_acquire)) {
        return false;
    }
    if (slot.state.load(std::memory_order_acquire) != 2U
        || slot.inFlight.load(std::memory_order_acquire) != 0U) {
        return false;
    }
    slot.observer.store(nullptr, std::memory_order_release);
    slot.context.store(nullptr, std::memory_order_release);
    slot.state.store(0U, std::memory_order_release);
    return true;
}

void receive_owner_attached() noexcept {
    if (g_header == nullptr || !all_handles_attached()) {
        account(g_header, FailureCounter::receiveOwnerReadinessFailure);
        return;
    }
    g_receiveOwnerAttached.store(true, std::memory_order_release);
    g_receiveOwnerQuiescing.store(false, std::memory_order_release);
    g_header->attachMask.fetch_or(kReceiveAttachBit, std::memory_order_acq_rel);
    refresh_readiness();
}

void receive_owner_quiescing() noexcept {
    g_receiveOwnerQuiescing.store(true, std::memory_order_release);
}

void receive_owner_detached(bool removed) noexcept {
    if (!removed) {
        account(g_header, FailureCounter::detachDeferred);
        return;
    }
    g_receiveOwnerAttached.store(false, std::memory_order_release);
    g_receiveOwnerQuiescing.store(false, std::memory_order_release);
    if (g_header != nullptr) {
        g_header->attachMask.fetch_and(~kReceiveAttachBit, std::memory_order_acq_rel);
        g_header->readinessState.store(static_cast<std::uint32_t>(Readiness::coreAttached),
                                       std::memory_order_release);
    }
    if (g_readiness.load(std::memory_order_acquire) == Readiness::ready) {
        refresh_readiness();
    }
}

void global_drop_owner_attached() noexcept {
    if (g_header == nullptr || !all_handles_attached()
        || !activity_lifecycle::register_global_drop_observer(
            &g_globalDropCallbackTable, g_globalDropObserverHandle)) {
        account(g_header, FailureCounter::receiveOwnerReadinessFailure);
        return;
    }
    g_globalDropOwnerAttached.store(true, std::memory_order_release);
    g_globalDropOwnerQuiescing.store(false, std::memory_order_release);
    g_header->attachMask.fetch_or(kGlobalDropAttachBit, std::memory_order_acq_rel);
    refresh_readiness();
}

void global_drop_owner_quiescing() noexcept {
    g_globalDropOwnerQuiescing.store(true, std::memory_order_release);
}

void global_drop_owner_detached(bool removed) noexcept {
    if (!removed) {
        account(g_header, FailureCounter::detachDeferred);
        return;
    }
    // Confirmed provider removal is the retirement authority for this publication. The native
    // owner clears its fanout only after every admitted pre/original/post lease has left; a
    // generation-specific unregister here would therefore fail against the already-free slot and
    // incorrectly retain heap readiness forever.
    g_globalDropObserverHandle = {};
    g_globalDropOwnerAttached.store(false, std::memory_order_release);
    g_globalDropOwnerQuiescing.store(false, std::memory_order_release);
    if (g_header != nullptr) {
        g_header->attachMask.fetch_and(~kGlobalDropAttachBit, std::memory_order_acq_rel);
        refresh_readiness();
    }
}

void receive_enter(ReceiveToken& token, void* sensorTable, void* bitStream) noexcept {
    ReceiveToken* const previous = g_receiveContext;
    token = {};
    token.previous = previous;
    token.sensorTable = reinterpret_cast<std::uintptr_t>(sensorTable);
    token.peerView = token.sensorTable >= 0x28U ? token.sensorTable - 0x28U : 0U;
    token.stream = reinterpret_cast<std::uintptr_t>(bitStream);
    token.cookie = kReceiveCookie ^ reinterpret_cast<std::uintptr_t>(&token);
    token.active = true;
    g_receiveContext = &token;
    g_receiveInflight.fetch_add(1U, std::memory_order_acq_rel);
    Shl1Header* const header = g_header;
    if (header != nullptr) {
        header->inFlight.fetch_add(1U, std::memory_order_acq_rel);
    }
    token.admitted = header != nullptr
                     && g_readiness.load(std::memory_order_acquire) == Readiness::ready
                     && g_receiveOwnerAttached.load(std::memory_order_acquire)
                     && !g_receiveOwnerQuiescing.load(std::memory_order_acquire)
                     && g_callGate.accepting();
    if (!token.admitted) {
        if (header != nullptr && g_readiness.load(std::memory_order_acquire) != Readiness::quiescing) {
            account(header, FailureCounter::preReadyCalls);
        }
        return;
    }
    (void)capture_stream_bits(bitStream, token.bitsBefore);
}

void receive_exit(ReceiveToken& token, std::uint64_t nativeResult) noexcept {
    Shl1Header* const header = g_header;
    if (!token.active || token.cookie != (kReceiveCookie ^ reinterpret_cast<std::uintptr_t>(&token))
        || g_receiveContext != &token) {
        account(header, FailureCounter::tlsMisnest);
    }
    token.nativeResult = nativeResult;
    if (token.admitted && token.record != 0U) {
        RecordState state{};
        RecordToken recordToken{};
        if (g_records.read(token.record, state, &recordToken) == RegistryResult::success
            && recordToken.generation == token.recordGeneration) {
            HeapCapture capture = capture_current_member(state, Member::receivedSense);
            capture.payload.bitsBefore = token.bitsBefore;
            (void)capture_stream_bits(reinterpret_cast<void*>(token.stream),
                                      capture.payload.bitsAfter);
            capture.payload.nativeFlags = static_cast<std::uint32_t>(nativeResult);
            push_heap_event(EventKind::resolve,
                            EventPhase::exit,
                            Member::receivedSense,
                            &state,
                            capture,
                            token.operationOrdinal,
                            0U,
                            validReceiveResolver | validNativeResult);
        }
    }
    g_receiveContext = static_cast<ReceiveToken*>(token.previous);
    token.active = false;
    token.admitted = false;
    g_receiveInflight.fetch_sub(1U, std::memory_order_release);
    if (header != nullptr) {
        header->inFlight.fetch_sub(1U, std::memory_order_release);
    }
}

void notify_assert(const char* text) noexcept {
    if (!exact_heap_assert(text) || g_header == nullptr) {
        return;
    }
    g_header->assertSeen.store(1U, std::memory_order_release);
    EventClaim claim = g_eventLedger.claim_critical();
    if (claim) {
        EventV1& event = *claim.first;
        prepare_event(event,
                      EventKind::assertion,
                      EventPhase::enter,
                      Member::none,
                      EventForm::control);
        event.valid |= validCriticalPersistence;
        ControlPayload& payload = event.payload.control;
        payload.freezeReason = static_cast<std::uint32_t>(FreezeReason::exactHeapAssert);
        payload.assertQpc = event.qpc;
        payload.assertThreadId = event.threadId;
        payload.normalClaimHighWater =
            g_header->normalClaim.load(std::memory_order_acquire);
        payload.criticalClaimHighWater =
            g_header->criticalClaim.load(std::memory_order_acquire);
        g_eventLedger.commit(event, claim.firstSequence);
    }
    (void)g_eventLedger.freeze(FreezeReason::exactHeapAssert, qpc_now());
    const bool first = durable_flush(false);
    const bool second = durable_flush(true);
    if (first && second) {
        g_header->flushState.store(static_cast<std::uint32_t>(FlushState::durable),
                                   std::memory_order_release);
        (void)durable_flush(true);
    }
    g_flushRequested.store(!(first && second), std::memory_order_release);
}

void observe_retail_line(const char* text, std::size_t length) noexcept {
    if (g_header == nullptr || text == nullptr) {
        return;
    }
    constexpr std::string_view timeout = "failed to connect due to timeout";
    constexpr std::string_view partial =
        "application_state: Suspend state changed from [active] to [partial]";
    constexpr std::string_view resume =
        "application_state: Suspend state changed from [partial] to [active]";
    if (bounded_contains(text, length, timeout)) {
        if (!claim_u32(g_header->timeoutOrdinal)) {
            account(g_header, FailureCounter::tlsMissingDrop);
        }
    }
    if (bounded_contains(text, length, partial)) {
        g_partialMarker.store(true, std::memory_order_release);
    }
    if (bounded_contains(text, length, resume)) {
        if (g_partialMarker.exchange(false, std::memory_order_acq_rel)
            && !claim_u32(g_header->resumeOrdinal)) {
            account(g_header, FailureCounter::tlsMissingDrop);
        }
    }
}

void drain(std::size_t maximumEvents) noexcept {
    if (g_header == nullptr || g_events == nullptr || maximumEvents == 0U) {
        return;
    }
    std::uint32_t cursor = g_header->textDrainIndex.load(std::memory_order_acquire);
    std::size_t drained = 0U;
    while (cursor < kEventCapacity && drained < maximumEvents) {
        const EventV1& event = g_events[cursor];
        const std::uint64_t sequence = event.commitSequence.load(std::memory_order_acquire);
        if (sequence == 0U) {
            break;
        }
        project_event(event, sequence);
        ++cursor;
        ++drained;
    }
    g_header->textDrainIndex.store(cursor, std::memory_order_release);
}

void service_flush() noexcept {
    if (!g_flushRequested.exchange(false, std::memory_order_acq_rel) || g_header == nullptr) {
        return;
    }
    const bool first = durable_flush(false);
    const bool second = durable_flush(true);
    if (first && second) {
        g_header->flushState.store(static_cast<std::uint32_t>(FlushState::durable),
                                   std::memory_order_release);
        (void)durable_flush(true);
    } else {
        g_flushRequested.store(true, std::memory_order_release);
    }
}

const SiteContract& receive_owner_contract() noexcept { return kSiteManifest[10U]; }

} // namespace sunrise::client::hooks::network::lifecycle::sensor_state_heap_full_cohort
