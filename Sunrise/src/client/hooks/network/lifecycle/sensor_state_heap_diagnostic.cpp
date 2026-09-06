#include "sensor_state_heap_diagnostic.h"

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

#include "../../../../core/logging/log.h"
#include "../../../../core/settings/settings.h"
#include "../../../diagnostics/module_range.h"
#include "../../../hooking/call_gate.h"
#include "../../../hooking/detour.h"
#include "sensor_state_heap_ring.h"

#pragma comment(lib, "bcrypt.lib")

namespace sunrise::client::hooks::network::lifecycle::sensor_state_heap {
namespace {

/** Phase-0 smoke only: no allocation/free/unlink boundary is intercepted by this unit. */
constexpr std::size_t kHookCount = 4U;
constexpr std::size_t kRingCapacity = 2048U;
constexpr std::size_t kRecordCapacity = 4096U;
constexpr std::size_t kReceivedSenseOffset = 0x50U;
constexpr std::size_t kModulePathCapacity = 32768U;
constexpr std::size_t kHashReadBufferSize = 64U * 1024U;

using SensorTableInsert = std::uint32_t*(__fastcall*)(void* sensorTable,
                                                       std::uint32_t* outDatum,
                                                       const void* identity,
                                                       std::int32_t authSchema,
                                                       std::int32_t senseSchema) noexcept;
using RecordConstruct = void(__fastcall*)(void* record,
                                          std::uint16_t heapSelector,
                                          const void* identity,
                                          std::int32_t authSchema,
                                          std::int32_t senseSchema) noexcept;
using SensorTableRemove = void(__fastcall*)(void* sensorTable, std::uint32_t datum) noexcept;
using RecordDestroy = void(__fastcall*)(void* record) noexcept;

struct InsertContext final {
    std::uintptr_t peerTable{};
    std::uint32_t* outDatum{};
    RegistryToken constructed{};
};

struct RemoveContext final {
    std::uintptr_t peerTable{};
    std::uint64_t removeOrdinal{};
    std::uint32_t datum{};
};

template <typename Context>
class ScopedContext final {
public:
    ScopedContext(Context*& slot, Context& current) noexcept
        : slot_(slot), previous_(slot) {
        slot_ = &current;
    }
    ~ScopedContext() { slot_ = previous_; }
    ScopedContext(const ScopedContext&) = delete;
    ScopedContext& operator=(const ScopedContext&) = delete;

private:
    Context*& slot_;
    Context* previous_{};
};

FixedRing<kRingCapacity> g_ring;
FixedRegistry<kRecordCapacity> g_registry;
hooking::CallGate g_callGate;
std::array<hooking::detour::Handle, kHookCount> g_handles{};
std::atomic<SensorTableInsert> g_sensorTableInsertOriginal{};
std::atomic<RecordConstruct> g_recordConstructOriginal{};
std::atomic<SensorTableRemove> g_sensorTableRemoveOriginal{};
std::atomic<RecordDestroy> g_recordDestroyOriginal{};
std::atomic_bool g_installed{};
std::atomic_bool g_flushRequested{};
std::atomic<Readiness> g_readiness{Readiness::disabled};
std::atomic<std::uint64_t> g_lastDiagnosticGeneration{};
std::atomic<std::uint64_t> g_lastRemoveOrdinal{};
std::atomic<std::uint64_t> g_diagnosticClaimBusy{};
std::atomic<std::uint64_t> g_diagnosticClaimExhausted{};
std::atomic<std::uint64_t> g_removeClaimBusy{};
std::atomic<std::uint64_t> g_removeClaimExhausted{};
std::atomic<std::uint64_t> g_guardedReadFailures{};
std::atomic_bool g_imageIdentityValidated{};
thread_local InsertContext* g_insertContext{};
thread_local RemoveContext* g_removeContext{};

[[nodiscard]] bool calls_idle() noexcept { return g_callGate.idle(); }

/** One claim attempt only; contention and exhaustion are explicit cohort losses. */
[[nodiscard]] std::uint64_t try_claim_ordinal(std::atomic<std::uint64_t>& storage,
                                              std::atomic<std::uint64_t>& busy,
                                              std::atomic<std::uint64_t>& exhausted) noexcept {
    std::uint64_t current = storage.load(std::memory_order_acquire);
    if (current == (std::numeric_limits<std::uint64_t>::max)()) {
        exhausted.fetch_add(1U, std::memory_order_relaxed);
        return 0U;
    }
    if (!storage.compare_exchange_strong(current,
                                         current + 1U,
                                         std::memory_order_acq_rel,
                                         std::memory_order_acquire)) {
        busy.fetch_add(1U, std::memory_order_relaxed);
        return 0U;
    }
    return current + 1U;
}

[[nodiscard]] LONG access_exception_filter(DWORD status) noexcept {
    return status == EXCEPTION_ACCESS_VIOLATION || status == EXCEPTION_IN_PAGE_ERROR
                   || status == EXCEPTION_DATATYPE_MISALIGNMENT
               ? EXCEPTION_EXECUTE_HANDLER
               : EXCEPTION_CONTINUE_SEARCH;
}

[[nodiscard]] bool readable_without_guard(const void* source, std::size_t bytes) noexcept {
    if (source == nullptr || bytes == 0U) {
        return false;
    }
    const std::uintptr_t begin = reinterpret_cast<std::uintptr_t>(source);
    if (begin > (std::numeric_limits<std::uintptr_t>::max)() - bytes) {
        return false;
    }
    const std::uintptr_t end = begin + bytes;
    std::uintptr_t cursor = begin;
    for (unsigned region = 0U; cursor < end && region < 2U; ++region) {
        MEMORY_BASIC_INFORMATION information{};
        if (VirtualQuery(reinterpret_cast<const void*>(cursor),
                         &information,
                         sizeof information)
            != sizeof information) {
            return false;
        }
        const DWORD protection = information.Protect;
        if (information.State != MEM_COMMIT
            || (protection & (PAGE_GUARD | PAGE_NOACCESS)) != 0U) {
            return false;
        }
        const DWORD access = protection & 0xFFU;
        if (access != PAGE_READONLY && access != PAGE_READWRITE && access != PAGE_WRITECOPY
            && access != PAGE_EXECUTE_READ && access != PAGE_EXECUTE_READWRITE
            && access != PAGE_EXECUTE_WRITECOPY) {
            return false;
        }
        const std::uintptr_t regionBase =
            reinterpret_cast<std::uintptr_t>(information.BaseAddress);
        if (regionBase > (std::numeric_limits<std::uintptr_t>::max)() - information.RegionSize) {
            return false;
        }
        const std::uintptr_t regionEnd = regionBase + information.RegionSize;
        if (regionEnd <= cursor) {
            return false;
        }
        cursor = (std::min)(regionEnd, end);
    }
    return cursor == end;
}

[[nodiscard]] bool safe_copy_exact(void* destination,
                                   const void* source,
                                   std::size_t bytes) noexcept {
    if (destination == nullptr || !readable_without_guard(source, bytes)) {
        return false;
    }
#if defined(_MSC_VER)
    __try {
        std::memcpy(destination, source, bytes);
        return true;
    } __except (access_exception_filter(GetExceptionCode())) {
        return false;
    }
#else
    (void)destination;
    (void)source;
    (void)bytes;
    return false;
#endif
}

[[nodiscard]] bool capture_identity(const void* source, SensorIdentity& output) noexcept {
    if (source == nullptr) {
        return false;
    }
    const auto* const bytes = static_cast<const std::byte*>(source);
    SensorIdentity identity{};
    if (!safe_copy_exact(&identity.word, bytes, sizeof identity.word)
        || !safe_copy_exact(&identity.kind, bytes + 4U, sizeof identity.kind)
        || !safe_copy_exact(&identity.index, bytes + 6U, sizeof identity.index)) {
        return false;
    }
    output = identity;
    return true;
}

[[nodiscard]] bool capture_received_sense(std::uintptr_t record,
                                          ReceivedSenseSnapshot& output) noexcept {
    if (record == 0U
        || record > (std::numeric_limits<std::uintptr_t>::max)() - kReceivedSenseOffset) {
        return false;
    }
    const auto* const triple =
        reinterpret_cast<const std::byte*>(record + kReceivedSenseOffset);
    ReceivedSenseSnapshot captured{};
    if (!safe_copy_exact(&captured.count, triple, sizeof captured.count)
        || !safe_copy_exact(&captured.relativePayload,
                            triple + 8U,
                            sizeof captured.relativePayload)
        || !safe_copy_exact(&captured.selector, triple + 0x10U, sizeof captured.selector)) {
        return false;
    }
    output = captured;
    return true;
}

[[nodiscard]] bool cng_succeeded(NTSTATUS status) noexcept { return status >= 0; }

/** Streams the exact packed executable file through SHA-256 before any native attachment. */
[[nodiscard]] bool current_executable_sha256(HMODULE module, ImageSha256& output) noexcept {
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

template <std::size_t PrefixSize>
[[nodiscard]] std::byte* checked_target(const diagnostics::ModuleRange& image,
                                        std::uintptr_t rva,
                                        const std::array<std::byte, PrefixSize>& prefix) noexcept {
    if (image.end <= image.base || rva > image.end - image.base
        || prefix.size() > image.end - image.base - rva) {
        return nullptr;
    }
    auto* const target = reinterpret_cast<std::byte*>(image.base + rva);
    std::array<std::byte, PrefixSize> observed{};
    return safe_copy_exact(observed.data(), target, observed.size())
                   && std::equal(prefix.begin(), prefix.end(), observed.begin())
               ? target
               : nullptr;
}

/** Publishes no target until the packed file hash and every mapped prefix match. */
[[nodiscard]] bool validate_loaded_image(std::array<std::byte*, kHookCount>& targets) noexcept {
    targets = {};
    HMODULE const module = GetModuleHandleW(nullptr);
    diagnostics::ModuleRange image{};
    ImageSha256 sha256{};
    if (module == nullptr || !diagnostics::module_range(module, image)
        || !current_executable_sha256(module, sha256) || sha256 != kPinnedPackedImageSha256) {
        return false;
    }
    const std::array candidate{
        checked_target(image, kSensorTableInsertRva, kSensorTableInsertPrefix),
        checked_target(image, kRecordConstructRva, kRecordConstructPrefix),
        checked_target(image, kSensorTableRemoveRva, kSensorTableRemovePrefix),
        checked_target(image, kRecordDestroyRva, kRecordDestroyPrefix),
    };
    if (std::any_of(candidate.begin(), candidate.end(), [](const std::byte* target) {
            return target == nullptr;
        })) {
        return false;
    }
    targets = candidate;
    return true;
}

[[nodiscard]] Event make_event(RegistryToken token,
                               EventKind kind,
                               EventPhase phase) noexcept {
    Event event{};
    event.context.diagnosticGeneration = g_ring.diagnostic_generation();
    event.context.recordGeneration = token.generation;
    event.valid = validDiagnosticGeneration | validRecordGeneration;
    event.record = token.record;
    event.kind = kind;
    event.phase = phase;
    event.tick = GetTickCount64();
    LARGE_INTEGER qpc{};
    if (QueryPerformanceCounter(&qpc) != FALSE) {
        event.qpc = qpc.QuadPart;
    }
    event.threadId = GetCurrentThreadId();
    if (token.activeReuse) {
        event.flags |= flagActiveRecordReuse;
    }
    if (token.reuseAfterUncertainRetire) {
        event.flags |= flagReuseAfterUncertainRetire;
    }
    return event;
}

void apply_metadata(Event& event, const RecordMetadata& metadata) noexcept {
    event.identity = metadata.identity;
    event.sensorKey = pack_sensor_identity(metadata.identity);
    event.authSchema = metadata.authSchema;
    event.senseSchema = metadata.senseSchema;
    if (metadata.identityValid) {
        event.valid |= validConstructorIdentity;
    }
    if (metadata.peerTableValid) {
        event.context.peerTable = metadata.peerTable;
        event.valid |= validPeerTable;
    }
    if (metadata.datumValid) {
        event.context.datum = metadata.datum;
        event.valid |= validDatum;
    }
    if (metadata.baselineValid) {
        event.baseline = metadata.baseline;
        event.valid |= validBaselineSnapshot;
    }
}

void apply_remove_context(Event& event) noexcept {
    if (g_removeContext == nullptr) {
        return;
    }
    event.context.removeOrdinal = g_removeContext->removeOrdinal;
    if (g_removeContext->removeOrdinal != 0U) {
        event.valid |= validRemoveOrdinal;
    }
    if (g_removeContext->peerTable != 0U) {
        event.context.peerTable = g_removeContext->peerTable;
        event.valid |= validPeerTable;
    }
    event.context.datum = g_removeContext->datum;
    event.valid |= validDatum;
}

void compare_baseline(Event& event) noexcept {
    if ((event.valid & (validCurrentSnapshot | validBaselineSnapshot))
        != (validCurrentSnapshot | validBaselineSnapshot)) {
        return;
    }
    if (event.current.relativePayload != event.baseline.relativePayload) {
        event.flags |= flagRelativePayloadChanged;
    }
    if (event.current.selector != event.baseline.selector) {
        event.flags |= flagSelectorChanged;
    }
    if (event.current.count != event.baseline.count) {
        event.flags |= flagCountChanged;
    }
}

__declspec(noinline) void __fastcall record_construct(void* record,
                                                       std::uint16_t heapSelector,
                                                       const void* identitySource,
                                                       std::int32_t authSchema,
                                                       std::int32_t senseSchema) noexcept {
    hooking::CallGate::Scope call(g_callGate);
    const RecordConstruct original = hooking::await_original(g_recordConstructOriginal);
    RegistryToken token{};
    SensorIdentity identity{};
    bool identityValid = false;
    if (call.accepts_side_effects()) {
        identityValid = capture_identity(identitySource, identity);
        if (!identityValid) {
            g_guardedReadFailures.fetch_add(1U, std::memory_order_relaxed);
        }
        RecordMetadata metadata{};
        metadata.peerTable = g_insertContext != nullptr ? g_insertContext->peerTable : 0U;
        metadata.identity = identity;
        metadata.authSchema = static_cast<std::uint32_t>(authSchema);
        metadata.senseSchema = static_cast<std::uint32_t>(senseSchema);
        metadata.identityValid = identityValid;
        metadata.peerTableValid = metadata.peerTable != 0U;
        (void)g_registry.try_begin(
            reinterpret_cast<std::uintptr_t>(record), metadata, token);
        if (g_insertContext != nullptr) {
            g_insertContext->constructed = token;
        }
        if (token) {
            Event event = make_event(token, EventKind::construct, EventPhase::enter);
            apply_metadata(event, metadata);
            if (!identityValid) {
                event.flags |= flagGuardedReadFailed;
            }
            (void)g_ring.try_push(event);
        }
    }

    original(record, heapSelector, identitySource, authSchema, senseSchema);

    if (!call.accepts_side_effects() || !token) {
        return;
    }
    ReceivedSenseSnapshot snapshot{};
    const bool snapshotValid =
        capture_received_sense(reinterpret_cast<std::uintptr_t>(record), snapshot);
    if (!snapshotValid) {
        g_guardedReadFailures.fetch_add(1U, std::memory_order_relaxed);
    }
    (void)g_registry.try_set_baseline(token, snapshot, snapshotValid);
    RegistryView view{};
    if (g_registry.try_snapshot(token.record, view) != RegistryResult::success) {
        return;
    }
    Event event = make_event(token, EventKind::construct, EventPhase::exit);
    apply_metadata(event, view.metadata);
    if (snapshotValid) {
        event.current = snapshot;
        event.valid |= validCurrentSnapshot;
    } else {
        event.flags |= flagGuardedReadFailed;
    }
    (void)g_ring.try_push(event);
}

__declspec(noinline) std::uint32_t* __fastcall sensor_table_insert(
    void* sensorTable,
    std::uint32_t* outDatum,
    const void* identity,
    std::int32_t authSchema,
    std::int32_t senseSchema) noexcept {
    hooking::CallGate::Scope call(g_callGate);
    const SensorTableInsert original = hooking::await_original(g_sensorTableInsertOriginal);
    InsertContext context{reinterpret_cast<std::uintptr_t>(sensorTable), outDatum, {}};
    ScopedContext scope(g_insertContext, context);
    std::uint32_t* const result =
        original(sensorTable, outDatum, identity, authSchema, senseSchema);
    if (!call.accepts_side_effects() || !context.constructed) {
        return result;
    }
    std::uint32_t datum{};
    const bool datumValid = safe_copy_exact(&datum, outDatum, sizeof datum);
    if (!datumValid) {
        g_guardedReadFailures.fetch_add(1U, std::memory_order_relaxed);
    }
    if (g_registry.try_associate(context.constructed,
                                 context.peerTable,
                                 datum,
                                 datumValid)
        != RegistryResult::success) {
        return result;
    }
    RegistryView view{};
    if (g_registry.try_snapshot(context.constructed.record, view) == RegistryResult::success) {
        Event event = make_event(context.constructed, EventKind::associate, EventPhase::exit);
        apply_metadata(event, view.metadata);
        if (!datumValid) {
            event.flags |= flagGuardedReadFailed;
        }
        (void)g_ring.try_push(event);
    }
    return result;
}

__declspec(noinline) void __fastcall record_destroy(void* record) noexcept {
    hooking::CallGate::Scope call(g_callGate);
    const RecordDestroy original = hooking::await_original(g_recordDestroyOriginal);
    RegistryView view{};
    if (call.accepts_side_effects()) {
        const std::uintptr_t recordAddress = reinterpret_cast<std::uintptr_t>(record);
        if (g_registry.try_snapshot(recordAddress, view) != RegistryResult::success) {
            g_registry.mark_retirement_uncertain(recordAddress);
        }
    }
    if (view) {
        Event event = make_event(view.token, EventKind::destroy, EventPhase::enter);
        apply_metadata(event, view.metadata);
        apply_remove_context(event);
        if (capture_received_sense(event.record, event.current)) {
            event.valid |= validCurrentSnapshot;
            compare_baseline(event);
        } else {
            g_guardedReadFailures.fetch_add(1U, std::memory_order_relaxed);
            event.flags |= flagGuardedReadFailed;
        }
        (void)g_ring.try_push(event);
    }

    original(record);

    if (!call.accepts_side_effects() || !view) {
        return;
    }
    Event event = make_event(view.token, EventKind::destroy, EventPhase::exit);
    apply_metadata(event, view.metadata);
    apply_remove_context(event);
    if (capture_received_sense(event.record, event.current)) {
        event.valid |= validCurrentSnapshot;
        compare_baseline(event);
        if (event.current.count != 0U || event.current.relativePayload != 0U
            || event.current.selector != 0U) {
            event.flags |= flagPostDestroyNotZero;
        }
    } else {
        g_guardedReadFailures.fetch_add(1U, std::memory_order_relaxed);
        event.flags |= flagGuardedReadFailed;
    }
    (void)g_ring.try_push(event);
    (void)g_registry.try_retire(view.token);
}

__declspec(noinline) void __fastcall sensor_table_remove(void* sensorTable,
                                                          std::uint32_t datum) noexcept {
    hooking::CallGate::Scope call(g_callGate);
    const SensorTableRemove original = hooking::await_original(g_sensorTableRemoveOriginal);
    RemoveContext context{
        reinterpret_cast<std::uintptr_t>(sensorTable),
        call.accepts_side_effects()
            ? try_claim_ordinal(
                  g_lastRemoveOrdinal, g_removeClaimBusy, g_removeClaimExhausted)
            : 0U,
        datum,
    };
    ScopedContext scope(g_removeContext, context);
    original(sensorTable, datum);
}

[[nodiscard]] const char* kind_name(EventKind kind) noexcept {
    switch (kind) {
    case EventKind::construct:
        return "construct";
    case EventKind::associate:
        return "associate";
    case EventKind::destroy:
        return "destroy";
    default:
        return "unknown";
    }
}

[[nodiscard]] const char* phase_name(EventPhase phase) noexcept {
    return phase == EventPhase::enter ? "enter" : "exit";
}

[[nodiscard]] const char* freeze_name(FreezeReason reason) noexcept {
    switch (reason) {
    case FreezeReason::exactHeapAssert:
        return "exact_heap_assert";
    case FreezeReason::lifecycleStop:
        return "lifecycle_stop";
    default:
        return "none";
    }
}

void report_identity_summary(const char* stage) noexcept {
    std::array<char, core::log::kLineCapacity> line{};
    const int written = std::snprintf(
        line.data(),
        line.size(),
        "%s stage=%s scope=phase0_smoke conclusion=INCONCLUSIVE "
        "packed_sha256=81964380664E7FCEE3C620085A157FDEAF91FEFACF7214907820F188BBEB4CED "
        "insert_rva=4D6EB0 insert_prefix=488BC45741564883EC58488958104C8B "
        "construct_rva=9FEE40 construct_prefix=48895C241048896C2418565741564883 "
        "remove_rva=4D7C00 remove_prefix=48895C240848896C2410488974241857 "
        "destroy_rva=9FE2C0 destroy_prefix=48895C2408574883EC500F57C0C74424 "
        "identity_valid=%u prefixes=all_match",
        kSmokeTextPrefix,
        stage,
        g_imageIdentityValidated.load(std::memory_order_acquire) ? 1U : 0U);
    if (written > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::warn,
                         {line.data(),
                          (std::min)(static_cast<std::size_t>(written), line.size() - 1U)});
    }
}

void report_summary(const char* stage) noexcept {
    report_identity_summary(stage);
    const RingCounters ring = g_ring.counters();
    const RegistryCounters registry = g_registry.counters();
    const std::uint64_t guardedReads = g_guardedReadFailures.load(std::memory_order_relaxed);
    const std::uint64_t diagnosticBusy = g_diagnosticClaimBusy.load(std::memory_order_relaxed);
    const std::uint64_t diagnosticExhausted =
        g_diagnosticClaimExhausted.load(std::memory_order_relaxed);
    const std::uint64_t removeBusy = g_removeClaimBusy.load(std::memory_order_relaxed);
    const std::uint64_t removeExhausted =
        g_removeClaimExhausted.load(std::memory_order_relaxed);
    const std::uint64_t losses = ring.losses() + registry.losses() + guardedReads
                                 + diagnosticBusy + diagnosticExhausted + removeBusy
                                 + removeExhausted;
    std::array<char, core::log::kLineCapacity> ringLine{};
    const int ringWritten = std::snprintf(
        ringLine.data(),
        ringLine.size(),
        "%s stage=%s scope=phase0_smoke conclusion=INCONCLUSIVE first_corruptor=unobserved "
        "loss=%s diagnostic_gen=%llu identity=pinned_packed_sha256_81964380664E7FCE "
        "identity_valid=%u prefixes=all_or_none ring_committed=%llu ring_pending=%llu "
        "ring_high_water=%llu ring_loss=%llu frozen_reason=%s frozen_seq=%llu "
        "guarded_read_fail=%llu diagnostic_claim_busy=%llu diagnostic_claim_exhausted=%llu "
        "remove_claim_busy=%llu remove_claim_exhausted=%llu active_calls=%u",
        kSmokeTextPrefix,
        stage,
        losses == 0U ? "no" : "yes",
        static_cast<unsigned long long>(g_ring.diagnostic_generation()),
        g_imageIdentityValidated.load(std::memory_order_acquire) ? 1U : 0U,
        static_cast<unsigned long long>(ring.committed),
        static_cast<unsigned long long>(ring.pending),
        static_cast<unsigned long long>(ring.highWater),
        static_cast<unsigned long long>(ring.losses()),
        freeze_name(g_ring.frozen_reason()),
        static_cast<unsigned long long>(ring.frozenSequence),
        static_cast<unsigned long long>(guardedReads),
        static_cast<unsigned long long>(diagnosticBusy),
        static_cast<unsigned long long>(diagnosticExhausted),
        static_cast<unsigned long long>(removeBusy),
        static_cast<unsigned long long>(removeExhausted),
        g_callGate.active_calls());
    if (ringWritten > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::warn,
                         {ringLine.data(),
                          (std::min)(static_cast<std::size_t>(ringWritten),
                                    ringLine.size() - 1U)});
    }

    std::array<char, core::log::kLineCapacity> registryLine{};
    const int registryWritten = std::snprintf(
        registryLine.data(),
        registryLine.size(),
        "%s stage=%s_registry scope=phase0_smoke conclusion=INCONCLUSIVE "
        "registry_begun=%llu registry_retired=%llu registry_high_water=%llu "
        "registry_loss=%llu reg_invalid=%llu reg_begin_busy=%llu reg_update_busy=%llu "
        "reg_snapshot_busy=%llu reg_retire_busy=%llu reg_full=%llu reg_exhausted=%llu "
        "reg_stale_update=%llu reg_stale_retire=%llu reg_missing_snapshot=%llu "
        "reg_active_reuse=%llu reg_uncertain_reuse=%llu",
        kSmokeTextPrefix,
        stage,
        static_cast<unsigned long long>(registry.begun),
        static_cast<unsigned long long>(registry.retired),
        static_cast<unsigned long long>(registry.highWater),
        static_cast<unsigned long long>(registry.losses()),
        static_cast<unsigned long long>(registry.invalid),
        static_cast<unsigned long long>(registry.beginBusy),
        static_cast<unsigned long long>(registry.updateBusy),
        static_cast<unsigned long long>(registry.snapshotBusy),
        static_cast<unsigned long long>(registry.retireBusy),
        static_cast<unsigned long long>(registry.full),
        static_cast<unsigned long long>(registry.generationExhausted),
        static_cast<unsigned long long>(registry.staleUpdate),
        static_cast<unsigned long long>(registry.staleRetire),
        static_cast<unsigned long long>(registry.missingSnapshot),
        static_cast<unsigned long long>(registry.activeReuse),
        static_cast<unsigned long long>(registry.uncertainReuse));
    if (registryWritten > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::warn,
                         {registryLine.data(),
                          (std::min)(static_cast<std::size_t>(registryWritten),
                                    registryLine.size() - 1U)});
    }
}

} // namespace

bool install() noexcept {
    if (!core::settings::get().omegaExperiments.unsafeDiagnostics) {
        g_readiness.store(Readiness::disabled, std::memory_order_release);
        return true;
    }
    if (g_installed.load(std::memory_order_acquire)) {
        return g_callGate.accepting();
    }

    drain_off_hook(kRingCapacity);
    g_callGate.quiesce();
    g_readiness.store(Readiness::unavailable, std::memory_order_release);
    g_flushRequested.store(false, std::memory_order_release);
    g_imageIdentityValidated.store(false, std::memory_order_release);
    g_diagnosticClaimBusy.store(0U, std::memory_order_relaxed);
    g_diagnosticClaimExhausted.store(0U, std::memory_order_relaxed);
    g_removeClaimBusy.store(0U, std::memory_order_relaxed);
    g_removeClaimExhausted.store(0U, std::memory_order_relaxed);
    g_guardedReadFailures.store(0U, std::memory_order_relaxed);
    g_registry.reset();
    const std::uint64_t diagnosticGeneration = try_claim_ordinal(
        g_lastDiagnosticGeneration, g_diagnosticClaimBusy, g_diagnosticClaimExhausted);
    if (diagnosticGeneration == 0U || !g_ring.reset(diagnosticGeneration)) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::warn,
                         "ev=sensor_heap_smoke schema=phase0_v1 stage=install "
                         "scope=phase0_smoke conclusion=INCONCLUSIVE result=fail "
                         "reason=diagnostic_generation");
        return false;
    }

    std::array<std::byte*, kHookCount> targets{};
    if (!validate_loaded_image(targets)) {
        core::log::write(
            core::log::Channel::client,
            core::log::Level::warn,
            "ev=sensor_heap_smoke schema=phase0_v1 stage=install scope=phase0_smoke "
            "conclusion=INCONCLUSIVE result=fail reason=packed_identity_or_prefix retained=0");
        return false;
    }
    g_imageIdentityValidated.store(true, std::memory_order_release);

    const std::array specs{
        hooking::detour::Spec{targets[0], reinterpret_cast<void*>(&sensor_table_insert)},
        hooking::detour::Spec{targets[1], reinterpret_cast<void*>(&record_construct)},
        hooking::detour::Spec{targets[2], reinterpret_cast<void*>(&sensor_table_remove)},
        hooking::detour::Spec{targets[3], reinterpret_cast<void*>(&record_destroy)},
    };
    if (!hooking::detour::install(specs, g_handles)) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::warn,
                         "ev=sensor_heap_smoke schema=phase0_v1 stage=install "
                         "scope=phase0_smoke conclusion=INCONCLUSIVE result=fail "
                         "reason=attach retained=0");
        return false;
    }
    hooking::publish_original(
        g_sensorTableInsertOriginal, reinterpret_cast<SensorTableInsert>(g_handles[0].original));
    hooking::publish_original(
        g_recordConstructOriginal, reinterpret_cast<RecordConstruct>(g_handles[1].original));
    hooking::publish_original(
        g_sensorTableRemoveOriginal, reinterpret_cast<SensorTableRemove>(g_handles[2].original));
    hooking::publish_original(
        g_recordDestroyOriginal, reinterpret_cast<RecordDestroy>(g_handles[3].original));
    g_installed.store(true, std::memory_order_release);
    g_callGate.accept();
    g_readiness.store(Readiness::ready, std::memory_order_release);
    report_summary("install");
    core::log::write(
        core::log::Channel::client,
        core::log::Level::warn,
        "ev=sensor_heap_smoke schema=phase0_v1 stage=install scope=phase0_smoke "
        "conclusion=INCONCLUSIVE result=ok mode=observe_only heap_intercept=none "
        "first_corruptor=unobserved identity=packed_sha256_verified prefixes=all_match");
    return true;
}

void quiesce() noexcept {
    g_callGate.quiesce();
    if (g_installed.load(std::memory_order_acquire)) {
        g_readiness.store(Readiness::quiescing, std::memory_order_release);
    }
}

bool uninstall() noexcept {
    quiesce();
    if (!g_installed.load(std::memory_order_acquire)) {
        return true;
    }
    const std::array protectedEntries{
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&sensor_table_insert)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&record_construct)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&sensor_table_remove)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&record_destroy)},
        hooking::detour::ProtectedCodeEntry{
            reinterpret_cast<void*>(&hooking::call_gate_detail::enter)},
        hooking::detour::ProtectedCodeEntry{
            reinterpret_cast<void*>(&hooking::call_gate_detail::leave)},
    };
    const hooking::detour::UninstallResult result =
        hooking::detour::uninstall(g_handles, protectedEntries, &calls_idle);
    if (result != hooking::detour::UninstallResult::removed) {
        core::log::write(
            core::log::Channel::client,
            result == hooking::detour::UninstallResult::failed ? core::log::Level::error
                                                               : core::log::Level::warn,
            result == hooking::detour::UninstallResult::failed
                ? "ev=sensor_heap_smoke schema=phase0_v1 stage=uninstall "
                  "conclusion=INCONCLUSIVE result=fail retained=1"
                : "ev=sensor_heap_smoke schema=phase0_v1 stage=uninstall "
                  "conclusion=INCONCLUSIVE result=deferred retained=1");
        return false;
    }
    g_sensorTableInsertOriginal.store(nullptr, std::memory_order_release);
    g_recordConstructOriginal.store(nullptr, std::memory_order_release);
    g_sensorTableRemoveOriginal.store(nullptr, std::memory_order_release);
    g_recordDestroyOriginal.store(nullptr, std::memory_order_release);
    g_installed.store(false, std::memory_order_release);
    (void)g_ring.freeze(FreezeReason::lifecycleStop);
    g_readiness.store(Readiness::unavailable, std::memory_order_release);
    report_summary("uninstall");
    return true;
}

bool has_ownership() noexcept {
    if (g_installed.load(std::memory_order_acquire) || !g_callGate.idle()) {
        return true;
    }
    return std::any_of(g_handles.begin(), g_handles.end(), [](const hooking::detour::Handle& handle) {
        return handle.attached;
    });
}

Readiness readiness() noexcept { return g_readiness.load(std::memory_order_acquire); }

void drain(std::size_t maxEvents) noexcept { drain_off_hook(maxEvents); }

void drain_off_hook(std::size_t maxEvents) noexcept {
    for (std::size_t index = 0U; index < maxEvents; ++index) {
        Event event{};
        const PopResult result = g_ring.try_pop(event);
        if (result != PopResult::success) {
            return;
        }
        std::array<char, core::log::kLineCapacity> line{};
        const int written = std::snprintf(
            line.data(),
            line.size(),
            "%s scope=phase0_smoke conclusion=INCONCLUSIVE seq=%llu kind=%s phase=%s "
            "tick=%llu qpc=%lld tid=%u valid=0x%08X flags=0x%08X diagnostic_gen=%llu "
            "remove_ordinal=%llu activity_gen=0 connection_gen=0 region_gen=0 drop_gen=0 "
            "native_generation_valid=0 peer_table=%p datum=0x%08X record=%p record_gen=%u "
            "sensor_word=0x%08X sensor_kind=%u sensor_index=%u sensor_key=0x%016llX "
            "auth_schema=0x%08X sense_schema=0x%08X count=%u "
            "relative_payload=0x%016llX selector=%u baseline_count=%u "
            "baseline_relative_payload=0x%016llX baseline_selector=%u",
            kSmokeTextPrefix,
            static_cast<unsigned long long>(event.sequence),
            kind_name(event.kind),
            phase_name(event.phase),
            static_cast<unsigned long long>(event.tick),
            static_cast<long long>(event.qpc),
            event.threadId,
            event.valid,
            event.flags,
            static_cast<unsigned long long>(event.context.diagnosticGeneration),
            static_cast<unsigned long long>(event.context.removeOrdinal),
            reinterpret_cast<void*>(event.context.peerTable),
            event.context.datum,
            reinterpret_cast<void*>(event.record),
            event.context.recordGeneration,
            event.identity.word,
            static_cast<unsigned>(event.identity.kind),
            static_cast<unsigned>(event.identity.index),
            static_cast<unsigned long long>(event.sensorKey),
            event.authSchema,
            event.senseSchema,
            event.current.count,
            static_cast<unsigned long long>(event.current.relativePayload),
            static_cast<unsigned>(event.current.selector),
            event.baseline.count,
            static_cast<unsigned long long>(event.baseline.relativePayload),
            static_cast<unsigned>(event.baseline.selector));
        if (written > 0) {
            core::log::write(
                core::log::Channel::client,
                (event.flags & kSmokeWarningFlags) != 0U ? core::log::Level::warn
                                                        : core::log::Level::info,
                {line.data(),
                 (std::min)(static_cast<std::size_t>(written), line.size() - 1U)});
        }
    }
}

bool flush_requested() noexcept { return g_flushRequested.load(std::memory_order_acquire); }

bool consume_flush_request() noexcept {
    return g_flushRequested.exchange(false, std::memory_order_acq_rel);
}

void notify_assert(const char* text) noexcept {
    if (g_installed.load(std::memory_order_acquire) && exact_heap_assert(text)) {
        (void)g_ring.freeze(FreezeReason::exactHeapAssert);
        g_flushRequested.store(true, std::memory_order_release);
    }
}

static_assert(std::atomic_bool::is_always_lock_free,
              "Exact-assert flush requests must remain lock-free");

} // namespace sunrise::client::hooks::network::lifecycle::sensor_state_heap
