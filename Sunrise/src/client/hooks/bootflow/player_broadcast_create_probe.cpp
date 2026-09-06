#include <Windows.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <intrin.h>

#include "../../../core/logging/log.h"
#include "../../hooking/detour.h"
#include "internal.h"
#include "legacy_owner_sentinel.h"

namespace sunrise::client::hooks::bootflow {
namespace {

/** Native simulation-entity allocator reached by the player_broadcast creation wrapper. */
constexpr std::uintptr_t kEntityCreateRva = 0x170F190U;
/** Builds the allocated simulation-entity record and its component storage. */
constexpr std::uintptr_t kEntityConstructorRva = 0x170B0F0U;
/** Reserves the fixed 0x70-byte entity record used by the constructor. */
constexpr std::uintptr_t kRecordAcquireRva = 0x170B2B0U;
/** Allocates the constructor's two component arrays. */
constexpr std::uintptr_t kComponentAllocateRva = 0x16C95C0U;
/** Performs the final per-record initialization before publication. */
constexpr std::uintptr_t kRecordInitializeRva = 0x171C280U;
/** Seeds or synchronizes the manager's 8192-bit native entity-id allocation pool. */
constexpr std::uintptr_t kEntityIdPoolInitializeRva = 0x171DB20U;
/** Rebuilds a manager-local registry mask and merges it into the entity-id pool. */
constexpr std::uintptr_t kEntityIdRegistryMergeRva = 0x170ACB0U;
/** Merges an externally supplied 8192-bit registry mask into the entity-id pool. */
constexpr std::uintptr_t kEntityIdSourceMergeRva = 0x17129E0U;
/** Return site of the exact player_broadcast wrapper call into kEntityCreateRva. */
constexpr std::uintptr_t kPlayerBroadcastCreateReturnRva = 0x16EE20AU;
/** The allocator scans this 8192-bit set for an available native entity id. */
constexpr std::size_t kEntityIdPoolOffset = 0xC118U;
constexpr std::size_t kEntityIdPoolBits = 0x2000U;
constexpr std::size_t kEntityIdPoolWords = kEntityIdPoolBits / 64U;
/** The synchronizer-owned source mask passed to the later native pool merge. */
constexpr std::size_t kSynchronizerEntityIdMaskOffset = 0x5FE98U;
constexpr std::size_t kComponentAllocationLimit = 4U;
constexpr std::size_t kPoolInitializeLogLimit = 6U;
constexpr std::size_t kManagerMergeStatsLimit = 8U;

constexpr std::array<std::byte, 23> kEntityCreatePrefix{
    std::byte{0x40}, std::byte{0x55}, std::byte{0x53}, std::byte{0x56}, std::byte{0x57},
    std::byte{0x41}, std::byte{0x56}, std::byte{0x48}, std::byte{0x8D}, std::byte{0xAC},
    std::byte{0x24}, std::byte{0xC0}, std::byte{0xFD}, std::byte{0xFF}, std::byte{0xFF},
    std::byte{0x48}, std::byte{0x81}, std::byte{0xEC}, std::byte{0x40}, std::byte{0x03},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x48}};
constexpr std::array<std::byte, 22> kEntityConstructorPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24}, std::byte{0x10},
    std::byte{0x48}, std::byte{0x89}, std::byte{0x6C}, std::byte{0x24}, std::byte{0x18},
    std::byte{0x56}, std::byte{0x57}, std::byte{0x41}, std::byte{0x55}, std::byte{0x41},
    std::byte{0x56}, std::byte{0x41}, std::byte{0x57}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x20}};
constexpr std::array<std::byte, 20> kRecordAcquirePrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24}, std::byte{0x08},
    std::byte{0x48}, std::byte{0x89}, std::byte{0x74}, std::byte{0x24}, std::byte{0x10},
    std::byte{0x57}, std::byte{0x48}, std::byte{0x83}, std::byte{0xEC}, std::byte{0x20},
    std::byte{0x81}, std::byte{0xE2}, std::byte{0xFF}, std::byte{0x1F}, std::byte{0x00}};
constexpr std::array<std::byte, 20> kComponentAllocatePrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24}, std::byte{0x08},
    std::byte{0x57}, std::byte{0x48}, std::byte{0x83}, std::byte{0xEC}, std::byte{0x20},
    std::byte{0x48}, std::byte{0x8B}, std::byte{0x1D}, std::byte{0x87}, std::byte{0x88},
    std::byte{0x98}, std::byte{0x01}, std::byte{0x48}, std::byte{0x63}, std::byte{0xF9}};
constexpr std::array<std::byte, 20> kRecordInitializePrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24}, std::byte{0x08},
    std::byte{0x48}, std::byte{0x89}, std::byte{0x74}, std::byte{0x24}, std::byte{0x10},
    std::byte{0x57}, std::byte{0x48}, std::byte{0x83}, std::byte{0xEC}, std::byte{0x20},
    std::byte{0x48}, std::byte{0x8B}, std::byte{0xF9}, std::byte{0x8B}, std::byte{0xF2}};
constexpr std::array<std::byte, 23> kEntityIdPoolInitializePrefix{
    std::byte{0x40}, std::byte{0x53}, std::byte{0x55}, std::byte{0x57}, std::byte{0x48},
    std::byte{0x81}, std::byte{0xEC}, std::byte{0x40}, std::byte{0x04}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x48}, std::byte{0x8B}, std::byte{0x05}, std::byte{0x56},
    std::byte{0xBF}, std::byte{0x98}, std::byte{0x00}, std::byte{0x48}, std::byte{0x33},
    std::byte{0xC4}, std::byte{0x48}, std::byte{0x89}};
constexpr std::array<std::byte, 18> kEntityIdRegistryMergePrefix{
    std::byte{0x40}, std::byte{0x55}, std::byte{0x56}, std::byte{0x48},
    std::byte{0x83}, std::byte{0xEC}, std::byte{0x48}, std::byte{0x48},
    std::byte{0x8B}, std::byte{0x41}, std::byte{0x08}, std::byte{0x48},
    std::byte{0x8B}, std::byte{0xF1}, std::byte{0x48}, std::byte{0x8B},
    std::byte{0x68}, std::byte{0x10}};
constexpr std::array<std::byte, 20> kEntityIdSourceMergePrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x08}, std::byte{0x57}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x40}, std::byte{0x48}, std::byte{0x8B},
    std::byte{0xD9}, std::byte{0x41}, std::byte{0xB9}, std::byte{0x00},
    std::byte{0x20}, std::byte{0x00}, std::byte{0x00}, std::byte{0x48}};

using EntityCreate = std::int32_t*(__fastcall*)(std::byte*,
                                                std::int32_t*,
                                                std::int32_t,
                                                std::int32_t,
                                                std::int32_t) noexcept;
using EntityConstructor = bool(__fastcall*)(std::byte*,
                                             std::int32_t,
                                             std::int32_t,
                                             std::int32_t,
                                             std::int32_t) noexcept;
using RecordAcquire = std::byte*(__fastcall*)(std::byte*, std::int32_t) noexcept;
using ComponentAllocate = void*(__fastcall*)(std::int32_t) noexcept;
using RecordInitialize = bool(__fastcall*)(std::byte*, std::int32_t) noexcept;
using EntityIdPoolInitialize = void(__fastcall*)(std::byte*) noexcept;
using EntityIdRegistryMerge = void(__fastcall*)(std::byte*) noexcept;
using EntityIdSourceMerge = void(__fastcall*)(std::byte*, const std::byte*) noexcept;

hooking::detour::Handle g_entityCreateHandle{};
hooking::detour::Handle g_entityConstructorHandle{};
hooking::detour::Handle g_recordAcquireHandle{};
hooking::detour::Handle g_componentAllocateHandle{};
hooking::detour::Handle g_recordInitializeHandle{};
hooking::detour::Handle g_entityIdPoolInitializeHandle{};
hooking::detour::Handle g_entityIdRegistryMergeHandle{};
hooking::detour::Handle g_entityIdSourceMergeHandle{};
std::atomic<EntityCreate> g_entityCreateOriginal{nullptr};
std::atomic<EntityConstructor> g_entityConstructorOriginal{nullptr};
std::atomic<RecordAcquire> g_recordAcquireOriginal{nullptr};
std::atomic<ComponentAllocate> g_componentAllocateOriginal{nullptr};
std::atomic<RecordInitialize> g_recordInitializeOriginal{nullptr};
std::atomic<EntityIdPoolInitialize> g_entityIdPoolInitializeOriginal{nullptr};
std::atomic<EntityIdRegistryMerge> g_entityIdRegistryMergeOriginal{nullptr};
std::atomic<EntityIdSourceMerge> g_entityIdSourceMergeOriginal{nullptr};
std::atomic_bool g_installAttempted{};
std::atomic_bool g_resultLogged{};
std::atomic_bool g_successSeen{};
std::atomic_uintptr_t g_successManager{};
std::atomic_bool g_postSuccessFailureLogged{};
std::atomic_uint32_t g_emptyPoolAttemptsSkipped{};
std::atomic_uint32_t g_poolInitializeCalls{};

struct ManagerMergeStats final {
    std::atomic_uintptr_t manager{};
    std::atomic_uint32_t registryCalls{};
    std::atomic_uint32_t sourceCalls{};
    std::atomic_uint32_t nonzeroSourceCalls{};
    std::atomic_uint32_t mutations{};
};

std::array<ManagerMergeStats, kManagerMergeStatsLimit> g_managerMergeStats{};

struct PoolSnapshot final {
    std::uint32_t setBits{};
    std::int32_t firstSet{-1};
    bool readable{};
};

struct ManagerMergeSnapshot final {
    std::uint32_t registryCalls{};
    std::uint32_t sourceCalls{};
    std::uint32_t nonzeroSourceCalls{};
    std::uint32_t mutations{};
};

struct CreateTrace final {
    bool active{};
    bool constructorCalled{};
    bool constructorResult{};
    bool recordAcquireCalled{};
    std::byte* record{};
    bool recordInitializeCalled{};
    bool recordInitializeResult{};
    std::int32_t allocatedHandle{-1};
    std::uint32_t componentAllocationCalls{};
    std::array<std::int32_t, kComponentAllocationLimit> componentAllocationSizes{};
    std::array<void*, kComponentAllocationLimit> componentAllocations{};
};

thread_local CreateTrace g_trace{};

struct PoolInitializeContext final {
    void* owner{};
    void* synchronizer{};
    std::uint32_t lowWater{};
    std::uint32_t highWater{};
    bool readable{};
};

/** Counts set bits in one native 8192-bit mask without changing allocator state. */
[[nodiscard]] PoolSnapshot snapshot_entity_id_mask(const std::byte* mask) noexcept {
    PoolSnapshot snapshot{};
    if (mask == nullptr) {
        return snapshot;
    }
    __try {
        const auto* const words = reinterpret_cast<const std::uint64_t*>(mask);
        for (std::size_t wordIndex = 0; wordIndex < kEntityIdPoolWords; ++wordIndex) {
            std::uint64_t word = words[wordIndex];
            if (snapshot.firstSet < 0 && word != 0U) {
                unsigned long bit = 0;
                _BitScanForward64(&bit, word);
                snapshot.firstSet = static_cast<std::int32_t>((wordIndex * 64U) + bit);
            }
            while (word != 0U) {
                word &= word - 1U;
                ++snapshot.setBits;
            }
        }
        snapshot.readable = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        snapshot = {};
    }
    return snapshot;
}

/** Counts available native entity ids without invoking or changing allocator state. */
[[nodiscard]] PoolSnapshot snapshot_entity_id_pool(const std::byte* manager) noexcept {
    return manager != nullptr ? snapshot_entity_id_mask(manager + kEntityIdPoolOffset)
                              : PoolSnapshot{};
}

[[nodiscard]] ManagerMergeStats* merge_stats_for_manager(std::byte* manager) noexcept {
    const std::uintptr_t key = reinterpret_cast<std::uintptr_t>(manager);
    if (key == 0U) {
        return nullptr;
    }
    for (auto& stats : g_managerMergeStats) {
        if (stats.manager.load(std::memory_order_acquire) == key) {
            return &stats;
        }
    }
    for (auto& stats : g_managerMergeStats) {
        std::uintptr_t expected = 0U;
        if (stats.manager.compare_exchange_strong(
                expected, key, std::memory_order_acq_rel, std::memory_order_acquire)
            || expected == key) {
            return &stats;
        }
    }
    return nullptr;
}

[[nodiscard]] ManagerMergeSnapshot snapshot_merge_stats(std::byte* manager) noexcept {
    ManagerMergeSnapshot snapshot{};
    const std::uintptr_t key = reinterpret_cast<std::uintptr_t>(manager);
    for (const auto& stats : g_managerMergeStats) {
        if (stats.manager.load(std::memory_order_acquire) == key) {
            snapshot.registryCalls = stats.registryCalls.load(std::memory_order_acquire);
            snapshot.sourceCalls = stats.sourceCalls.load(std::memory_order_acquire);
            snapshot.nonzeroSourceCalls =
                stats.nonzeroSourceCalls.load(std::memory_order_acquire);
            snapshot.mutations = stats.mutations.load(std::memory_order_acquire);
            break;
        }
    }
    return snapshot;
}

[[nodiscard]] std::uintptr_t return_address_rva(const void* returnAddress) noexcept {
    const auto* const image = reinterpret_cast<const std::byte*>(GetModuleHandleW(nullptr));
    const auto address = reinterpret_cast<std::uintptr_t>(returnAddress);
    return image != nullptr && address >= reinterpret_cast<std::uintptr_t>(image)
               ? address - reinterpret_cast<std::uintptr_t>(image)
               : 0U;
}

/** Reads the branch inputs used by the native entity-id pool initializer. */
[[nodiscard]] PoolInitializeContext
snapshot_pool_initialize_context(const std::byte* manager) noexcept {
    PoolInitializeContext snapshot{};
    if (manager == nullptr) {
        return snapshot;
    }
    __try {
        snapshot.owner = *reinterpret_cast<void* const*>(manager + 0x08U);
        if (snapshot.owner != nullptr) {
            snapshot.synchronizer = *reinterpret_cast<void* const*>(
                static_cast<const std::byte*>(snapshot.owner) + 0x10U);
        }
        snapshot.lowWater = *reinterpret_cast<const std::uint32_t*>(manager + 0xC518U);
        snapshot.highWater = *reinterpret_cast<const std::uint32_t*>(manager + 0xC51CU);
        snapshot.readable = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        snapshot = {};
    }
    return snapshot;
}

template <std::size_t Size>
[[nodiscard]] std::byte* validated_target(std::uintptr_t rva,
                                          const std::array<std::byte, Size>& prefix) noexcept {
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

/** Observes the first fixed-record acquisition nested inside player_broadcast construction. */
__declspec(noinline) std::byte* __fastcall record_acquire(std::byte* manager,
                                                          std::int32_t handle) noexcept {
    const RecordAcquire original = g_recordAcquireOriginal.load(std::memory_order_acquire);
    std::byte* const result = original != nullptr ? original(manager, handle) : nullptr;
    if (g_trace.active) {
        g_trace.recordAcquireCalled = true;
        g_trace.record = result;
    }
    return result;
}

/** Observes both component-array allocations made by the entity constructor. */
__declspec(noinline) void* __fastcall component_allocate(std::int32_t count) noexcept {
    const ComponentAllocate original =
        g_componentAllocateOriginal.load(std::memory_order_acquire);
    void* const result = original != nullptr ? original(count) : nullptr;
    if (g_trace.active) {
        const std::uint32_t call = g_trace.componentAllocationCalls++;
        if (call < g_trace.componentAllocations.size()) {
            g_trace.componentAllocationSizes[call] = count;
            g_trace.componentAllocations[call] = result;
        }
    }
    return result;
}

/** Observes the final native record-initialization result. */
__declspec(noinline) bool __fastcall record_initialize(std::byte* record,
                                                        std::int32_t mode) noexcept {
    const RecordInitialize original =
        g_recordInitializeOriginal.load(std::memory_order_acquire);
    const bool result = original != nullptr && original(record, mode);
    if (g_trace.active) {
        g_trace.recordInitializeCalled = true;
        g_trace.recordInitializeResult = result;
    }
    return result;
}

/** Observes every native entity-id pool initialization and its normal result. */
__declspec(noinline) void __fastcall entity_id_pool_initialize(std::byte* manager) noexcept {
    const EntityIdPoolInitialize original =
        g_entityIdPoolInitializeOriginal.load(std::memory_order_acquire);
    const std::uint32_t call = g_poolInitializeCalls.fetch_add(1U, std::memory_order_relaxed) + 1U;
    const auto* const image = reinterpret_cast<const std::byte*>(GetModuleHandleW(nullptr));
    const auto returnAddress = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= reinterpret_cast<std::uintptr_t>(image)
                                         ? returnAddress - reinterpret_cast<std::uintptr_t>(image)
                                         : 0U;
    const PoolSnapshot before = snapshot_entity_id_pool(manager);
    const PoolInitializeContext context = snapshot_pool_initialize_context(manager);
    if (original != nullptr) {
        original(manager);
    }
    const PoolSnapshot after = snapshot_entity_id_pool(manager);
    if (call <= kPoolInitializeLogLimit) {
        std::array<char, 512> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=entity_id_pool_initialize n=%u caller_rva=0x%llX manager=%p owner=%p synchronizer=%p context_readable=%u low=%u high=%u pool_before=%u first_before=%d pool_after=%u first_after=%d mode=observe_only",
            call,
            static_cast<unsigned long long>(callerRva),
            static_cast<void*>(manager),
            context.owner,
            context.synchronizer,
            context.readable ? 1U : 0U,
            context.lowWater,
            context.highWater,
            before.setBits,
            before.firstSet,
            after.setBits,
            after.firstSet);
        if (length > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(length)});
        }
    }
}

/** Observes the native registry-derived pool merge without changing its inputs or result. */
__declspec(noinline) void __fastcall entity_id_registry_merge(std::byte* manager) noexcept {
    const EntityIdRegistryMerge original =
        g_entityIdRegistryMergeOriginal.load(std::memory_order_acquire);
    ManagerMergeStats* const stats = merge_stats_for_manager(manager);
    if (stats != nullptr) {
        stats->registryCalls.fetch_add(1U, std::memory_order_relaxed);
    }
    const std::uintptr_t callerRva = return_address_rva(_ReturnAddress());
    const PoolSnapshot before = snapshot_entity_id_pool(manager);
    if (original != nullptr) {
        original(manager);
    }
    const PoolSnapshot after = snapshot_entity_id_pool(manager);
    if (before.readable && after.readable && before.setBits != after.setBits) {
        if (stats != nullptr) {
            stats->mutations.fetch_add(1U, std::memory_order_relaxed);
        }
        std::array<char, 384> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=entity_id_pool_merge kind=registry caller_rva=0x%llX manager=%p pool_before=%u first_before=%d pool_after=%u first_after=%d mode=observe_only",
            static_cast<unsigned long long>(callerRva),
            static_cast<void*>(manager),
            before.setBits,
            before.firstSet,
            after.setBits,
            after.firstSet);
        if (length > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(length)});
        }
    }
}

/** Observes the native source-mask pool merge without changing its inputs or result. */
__declspec(noinline) void __fastcall entity_id_source_merge(std::byte* manager,
                                                             const std::byte* source) noexcept {
    const EntityIdSourceMerge original =
        g_entityIdSourceMergeOriginal.load(std::memory_order_acquire);
    ManagerMergeStats* const stats = merge_stats_for_manager(manager);
    if (stats != nullptr) {
        stats->sourceCalls.fetch_add(1U, std::memory_order_relaxed);
    }
    const std::uintptr_t callerRva = return_address_rva(_ReturnAddress());
    const PoolSnapshot sourceSnapshot = snapshot_entity_id_mask(source);
    if (stats != nullptr && sourceSnapshot.readable && sourceSnapshot.setBits != 0U) {
        stats->nonzeroSourceCalls.fetch_add(1U, std::memory_order_relaxed);
    }
    const PoolSnapshot before = snapshot_entity_id_pool(manager);
    if (original != nullptr) {
        original(manager, source);
    }
    const PoolSnapshot after = snapshot_entity_id_pool(manager);
    if (before.readable && after.readable && before.setBits != after.setBits) {
        if (stats != nullptr) {
            stats->mutations.fetch_add(1U, std::memory_order_relaxed);
        }
        std::array<char, 448> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=entity_id_pool_merge kind=source caller_rva=0x%llX manager=%p source=%p source_readable=%u source_bits=%u source_first=%d pool_before=%u first_before=%d pool_after=%u first_after=%d mode=observe_only",
            static_cast<unsigned long long>(callerRva),
            static_cast<void*>(manager),
            static_cast<const void*>(source),
            sourceSnapshot.readable ? 1U : 0U,
            sourceSnapshot.setBits,
            sourceSnapshot.firstSet,
            before.setBits,
            before.firstSet,
            after.setBits,
            after.firstSet);
        if (length > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(length)});
        }
    }
}

/** Observes the constructor result while the exact outer player_broadcast call is active. */
__declspec(noinline) bool __fastcall entity_constructor(std::byte* manager,
                                                         std::int32_t handle,
                                                         std::int32_t entityType,
                                                         std::int32_t ownerSlot,
                                                         std::int32_t reference) noexcept {
    if (g_trace.active) {
        g_trace.constructorCalled = true;
        g_trace.allocatedHandle = handle;
    }
    const EntityConstructor original =
        g_entityConstructorOriginal.load(std::memory_order_acquire);
    const bool result = original != nullptr
                        && original(manager, handle, entityType, ownerSlot, reference);
    if (g_trace.active) {
        g_trace.constructorResult = result;
    }
    return result;
}

/** Captures the complete result of the private attempt and the first later public attempt. */
__declspec(noinline) std::int32_t* __fastcall entity_create(std::byte* manager,
                                                            std::int32_t* output,
                                                            std::int32_t entityType,
                                                            std::int32_t ownerSlot,
                                                            std::int32_t reference) noexcept {
    const EntityCreate original = g_entityCreateOriginal.load(std::memory_order_acquire);
    if (original == nullptr) {
        return output;
    }
    const auto* const image = reinterpret_cast<const std::byte*>(GetModuleHandleW(nullptr));
    const auto returnAddress = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    const bool exactPlayerBroadcast = image != nullptr
                                      && returnAddress
                                             == reinterpret_cast<std::uintptr_t>(image)
                                                    + kPlayerBroadcastCreateReturnRva
                                      && entityType == 2;
    if (!exactPlayerBroadcast || g_postSuccessFailureLogged.load(std::memory_order_acquire)) {
        return original(manager, output, entityType, ownerSlot, reference);
    }

    g_trace = {};
    g_trace.active = true;
    const PoolSnapshot observedBefore = snapshot_entity_id_pool(manager);
    const PoolInitializeContext poolContext = snapshot_pool_initialize_context(manager);
    const auto* const sourceMask =
        poolContext.synchronizer != nullptr
            ? static_cast<const std::byte*>(poolContext.synchronizer)
                  + kSynchronizerEntityIdMaskOffset
            : nullptr;
    const PoolSnapshot sourceBefore =
        sourceMask != nullptr ? snapshot_entity_id_mask(sourceMask) : PoolSnapshot{};
    PoolSnapshot before = observedBefore;
    bool premergeApplied = false;
    // The public swap asks for player_broadcast one tick before its already-populated native
    // synchronizer is merged into the new manager. Use the same native merge and exact owned mask
    // that the client calls on the following tick; never manufacture or copy allocator bits.
    if (observedBefore.readable && observedBefore.setBits == 0U && sourceBefore.readable
        && sourceBefore.setBits != 0U) {
        const EntityIdSourceMerge sourceMerge =
            g_entityIdSourceMergeOriginal.load(std::memory_order_acquire);
        if (sourceMerge != nullptr) {
            ManagerMergeStats* const stats = merge_stats_for_manager(manager);
            if (stats != nullptr) {
                stats->sourceCalls.fetch_add(1U, std::memory_order_relaxed);
                stats->nonzeroSourceCalls.fetch_add(1U, std::memory_order_relaxed);
            }
            sourceMerge(manager, sourceMask);
            before = snapshot_entity_id_pool(manager);
            premergeApplied = before.readable && before.setBits != 0U;
            if (premergeApplied && stats != nullptr) {
                stats->mutations.fetch_add(1U, std::memory_order_relaxed);
            }
            std::array<char, 448> mergeLine{};
            const int mergeLength = std::snprintf(
                mergeLine.data(),
                mergeLine.size(),
                "ev=bootflow stage=entity_id_pool_premerge result=%s manager=%p synchronizer=%p source=%p source_bits=%u source_first=%d pool_before=%u pool_after=%u first_after=%d mode=native_owned_timing_correction",
                premergeApplied ? "applied" : "unchanged",
                static_cast<void*>(manager),
                poolContext.synchronizer,
                static_cast<const void*>(sourceMask),
                sourceBefore.setBits,
                sourceBefore.firstSet,
                observedBefore.setBits,
                before.setBits,
                before.firstSet);
            if (mergeLength > 0) {
                core::log::write(core::log::Channel::client,
                                 premergeApplied ? core::log::Level::info
                                                 : core::log::Level::warn,
                                 {mergeLine.data(), static_cast<std::size_t>(mergeLength)});
            }
        }
    }
    std::int32_t* const result = original(manager, output, entityType, ownerSlot, reference);
    const PoolSnapshot after = snapshot_entity_id_pool(manager);
    g_trace.active = false;

    // The manager begins with no advertised ids and is populated later by the normal simulation
    // registry update. Preserve that timing and wait for the first failure after ids are present.
    const bool successSeen = g_successSeen.load(std::memory_order_acquire);
    if (!successSeen && before.readable && before.setBits == 0U) {
        g_emptyPoolAttemptsSkipped.fetch_add(1U, std::memory_order_relaxed);
        return result;
    }

    std::int32_t outputValue = -999;
    __try {
        outputValue = output != nullptr ? *output : -999;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        outputValue = -999;
    }
    const bool failed = outputValue == -1;
    const char* phase = "initial";
    if (!successSeen) {
        bool expected = false;
        if (!g_resultLogged.compare_exchange_strong(
                expected, true, std::memory_order_acq_rel, std::memory_order_acquire)) {
            return result;
        }
        if (!failed) {
            g_successManager.store(reinterpret_cast<std::uintptr_t>(manager),
                                   std::memory_order_release);
            g_successSeen.store(true, std::memory_order_release);
        }
    } else {
        if (reinterpret_cast<std::uintptr_t>(manager)
            == g_successManager.load(std::memory_order_acquire)) {
            return result;
        }
        bool expected = false;
        if (!g_postSuccessFailureLogged.compare_exchange_strong(
                expected, true, std::memory_order_acq_rel, std::memory_order_acquire)) {
            return result;
        }
        phase = "after_success";
    }
    const char* reason = !failed ? "accepted"
                         : before.readable && before.setBits == 0U ? "entity_id_pool_empty"
                         : !g_trace.constructorCalled ? "entity_id_allocate_failed"
                         : g_trace.constructorResult ? "outer_rejected_after_constructor"
                         : g_trace.recordAcquireCalled && g_trace.record == nullptr
                             ? "entity_record_pool_empty"
                         : g_trace.componentAllocationCalls > 0U
                                   && g_trace.componentAllocations[0] == nullptr
                             ? "component_allocation_1_failed"
                         : g_trace.componentAllocationCalls > 1U
                                   && g_trace.componentAllocations[1] == nullptr
                             ? "component_allocation_2_failed"
                         : g_trace.recordInitializeCalled && !g_trace.recordInitializeResult
                             ? "record_initialization_failed"
                             : "constructor_rejected_other";
    const ManagerMergeSnapshot mergeStats = snapshot_merge_stats(manager);
    std::array<char, 1280> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=player_broadcast_create_probe phase=%s result=%s output=%d manager=%p entity_type=%d owner_slot=%d reference=%d early_empty_skipped=%u pool_readable=%u pool_set_observed=%u pool_set_before=%u pool_first_before=%d pool_set_after=%u pool_first_after=%d synchronizer=%p source_readable=%u source_bits_before=%u source_first_before=%d premerge_applied=%u merge_registry_calls=%u merge_source_calls=%u merge_nonzero_source_calls=%u merge_mutations=%u constructor_called=%u constructor_result=%u allocated_handle=%d record_called=%u record=%p component_calls=%u component_0_size=%d component_0=%p component_1_size=%d component_1=%p initialize_called=%u initialize_result=%u mode=native_owned_timing_correction",
        phase,
        reason,
        outputValue,
        static_cast<void*>(manager),
        entityType,
        ownerSlot,
        reference,
        g_emptyPoolAttemptsSkipped.load(std::memory_order_acquire),
        before.readable ? 1U : 0U,
        observedBefore.setBits,
        before.setBits,
        before.firstSet,
        after.setBits,
        after.firstSet,
        poolContext.synchronizer,
        sourceBefore.readable ? 1U : 0U,
        sourceBefore.setBits,
        sourceBefore.firstSet,
        premergeApplied ? 1U : 0U,
        mergeStats.registryCalls,
        mergeStats.sourceCalls,
        mergeStats.nonzeroSourceCalls,
        mergeStats.mutations,
        g_trace.constructorCalled ? 1U : 0U,
        g_trace.constructorResult ? 1U : 0U,
        g_trace.allocatedHandle,
        g_trace.recordAcquireCalled ? 1U : 0U,
        static_cast<void*>(g_trace.record),
        g_trace.componentAllocationCalls,
        g_trace.componentAllocationSizes[0],
        g_trace.componentAllocations[0],
        g_trace.componentAllocationSizes[1],
        g_trace.componentAllocations[1],
        g_trace.recordInitializeCalled ? 1U : 0U,
        g_trace.recordInitializeResult ? 1U : 0U);
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         failed ? core::log::Level::warn : core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
    return result;
}

void clear_probe_handles() noexcept {
    if (g_entityCreateHandle.attached) {
        (void)hooking::detour::uninstall(g_entityCreateHandle);
    }
    if (g_entityIdSourceMergeHandle.attached) {
        (void)hooking::detour::uninstall(g_entityIdSourceMergeHandle);
    }
    if (g_entityIdRegistryMergeHandle.attached) {
        (void)hooking::detour::uninstall(g_entityIdRegistryMergeHandle);
    }
    if (g_entityIdPoolInitializeHandle.attached) {
        (void)hooking::detour::uninstall(g_entityIdPoolInitializeHandle);
    }
    if (g_entityConstructorHandle.attached) {
        (void)hooking::detour::uninstall(g_entityConstructorHandle);
    }
    if (g_recordInitializeHandle.attached) {
        (void)hooking::detour::uninstall(g_recordInitializeHandle);
    }
    if (g_componentAllocateHandle.attached) {
        (void)hooking::detour::uninstall(g_componentAllocateHandle);
    }
    if (g_recordAcquireHandle.attached) {
        (void)hooking::detour::uninstall(g_recordAcquireHandle);
    }
    g_entityCreateOriginal.store(nullptr, std::memory_order_release);
    g_entityConstructorOriginal.store(nullptr, std::memory_order_release);
    g_recordAcquireOriginal.store(nullptr, std::memory_order_release);
    g_componentAllocateOriginal.store(nullptr, std::memory_order_release);
    g_recordInitializeOriginal.store(nullptr, std::memory_order_release);
    g_entityIdPoolInitializeOriginal.store(nullptr, std::memory_order_release);
    g_entityIdRegistryMergeOriginal.store(nullptr, std::memory_order_release);
    g_entityIdSourceMergeOriginal.store(nullptr, std::memory_order_release);
}

template <typename Function>
[[nodiscard]] bool attach_target(std::byte* target,
                                 void* replacement,
                                 hooking::detour::Handle& handle,
                                 std::atomic<Function>& original) noexcept {
    const hooking::detour::Spec spec{target, replacement};
    if (!hooking::detour::install(spec, handle)) {
        return false;
    }
    original.store(reinterpret_cast<Function>(handle.original), std::memory_order_release);
    return true;
}

[[nodiscard]] std::array<legacy_owner_sentinel::HookOwnership, 8>
player_broadcast_create_hook_ownership() noexcept {
    // LEGACY_OWNER_SENTINEL_BEGIN(player_broadcast_create, 8)
    return {{{g_entityCreateHandle.attached,
              g_entityCreateOriginal.load(std::memory_order_acquire) != nullptr},
             {g_entityConstructorHandle.attached,
              g_entityConstructorOriginal.load(std::memory_order_acquire) != nullptr},
             {g_recordAcquireHandle.attached,
              g_recordAcquireOriginal.load(std::memory_order_acquire) != nullptr},
             {g_componentAllocateHandle.attached,
              g_componentAllocateOriginal.load(std::memory_order_acquire) != nullptr},
             {g_recordInitializeHandle.attached,
              g_recordInitializeOriginal.load(std::memory_order_acquire) != nullptr},
             {g_entityIdPoolInitializeHandle.attached,
              g_entityIdPoolInitializeOriginal.load(std::memory_order_acquire) != nullptr},
             {g_entityIdRegistryMergeHandle.attached,
              g_entityIdRegistryMergeOriginal.load(std::memory_order_acquire) != nullptr},
             {g_entityIdSourceMergeHandle.attached,
              g_entityIdSourceMergeOriginal.load(std::memory_order_acquire) != nullptr}}};
    // LEGACY_OWNER_SENTINEL_END(player_broadcast_create)
}

} // namespace

bool player_broadcast_create_probe_attached() noexcept {
    return legacy_owner_sentinel::any_handle_attached(
        player_broadcast_create_hook_ownership());
}

bool player_broadcast_create_probe_has_ownership() noexcept {
    // LEGACY_OWNER_CLAIMS_BEGIN(player_broadcast_create, 1)
    const std::array claims{
        g_installAttempted.load(std::memory_order_acquire),
    };
    // LEGACY_OWNER_CLAIMS_END(player_broadcast_create)
    return legacy_owner_sentinel::has_ownership(
        player_broadcast_create_hook_ownership(), claims);
}

/** Attaches after the first retail failure, when all five native functions are decrypted. */
bool install_player_broadcast_create_probe() noexcept {
    LateInstallGuard lateInstall;
    if (!lateInstall.accepted()) {
        return false;
    }
    if (g_entityCreateHandle.attached && g_entityConstructorHandle.attached
        && g_recordAcquireHandle.attached && g_componentAllocateHandle.attached
        && g_recordInitializeHandle.attached && g_entityIdPoolInitializeHandle.attached
        && g_entityIdRegistryMergeHandle.attached && g_entityIdSourceMergeHandle.attached) {
        return true;
    }
    bool expected = false;
    if (!g_installAttempted.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel, std::memory_order_acquire)) {
        return false;
    }
    std::byte* const createTarget = validated_target(kEntityCreateRva, kEntityCreatePrefix);
    std::byte* const constructorTarget =
        validated_target(kEntityConstructorRva, kEntityConstructorPrefix);
    std::byte* const recordTarget = validated_target(kRecordAcquireRva, kRecordAcquirePrefix);
    std::byte* const componentTarget =
        validated_target(kComponentAllocateRva, kComponentAllocatePrefix);
    std::byte* const initializeTarget =
        validated_target(kRecordInitializeRva, kRecordInitializePrefix);
    std::byte* const poolInitializeTarget =
        validated_target(kEntityIdPoolInitializeRva, kEntityIdPoolInitializePrefix);
    std::byte* const registryMergeTarget =
        validated_target(kEntityIdRegistryMergeRva, kEntityIdRegistryMergePrefix);
    std::byte* const sourceMergeTarget =
        validated_target(kEntityIdSourceMergeRva, kEntityIdSourceMergePrefix);
    if (createTarget == nullptr || constructorTarget == nullptr || recordTarget == nullptr
        || componentTarget == nullptr || initializeTarget == nullptr
        || poolInitializeTarget == nullptr || registryMergeTarget == nullptr
        || sourceMergeTarget == nullptr) {
        core::log::write(
            core::log::Channel::client,
            core::log::Level::warn,
            "ev=bootflow stage=player_broadcast_create_probe result=fail reason=target mode=observe_only");
        return false;
    }

    // Attach inner calls first so the outer trace can never run with a partial nested observer.
    const bool attached = attach_target(recordTarget,
                                        reinterpret_cast<void*>(&record_acquire),
                                        g_recordAcquireHandle,
                                        g_recordAcquireOriginal)
                          && attach_target(componentTarget,
                                           reinterpret_cast<void*>(&component_allocate),
                                           g_componentAllocateHandle,
                                           g_componentAllocateOriginal)
                          && attach_target(initializeTarget,
                                           reinterpret_cast<void*>(&record_initialize),
                                           g_recordInitializeHandle,
                                           g_recordInitializeOriginal)
                          && attach_target(constructorTarget,
                                           reinterpret_cast<void*>(&entity_constructor),
                                           g_entityConstructorHandle,
                                           g_entityConstructorOriginal)
                          && attach_target(poolInitializeTarget,
                                           reinterpret_cast<void*>(&entity_id_pool_initialize),
                                           g_entityIdPoolInitializeHandle,
                                           g_entityIdPoolInitializeOriginal)
                          && attach_target(registryMergeTarget,
                                           reinterpret_cast<void*>(&entity_id_registry_merge),
                                           g_entityIdRegistryMergeHandle,
                                           g_entityIdRegistryMergeOriginal)
                          && attach_target(sourceMergeTarget,
                                           reinterpret_cast<void*>(&entity_id_source_merge),
                                           g_entityIdSourceMergeHandle,
                                           g_entityIdSourceMergeOriginal)
                          && attach_target(createTarget,
                                           reinterpret_cast<void*>(&entity_create),
                                           g_entityCreateHandle,
                                           g_entityCreateOriginal);
    if (!attached) {
        clear_probe_handles();
        core::log::write(
            core::log::Channel::client,
            core::log::Level::warn,
            "ev=bootflow stage=player_broadcast_create_probe result=fail reason=attach mode=observe_only");
        return false;
    }
    g_resultLogged.store(false, std::memory_order_release);
    g_successSeen.store(false, std::memory_order_release);
    g_successManager.store(0U, std::memory_order_release);
    g_postSuccessFailureLogged.store(false, std::memory_order_release);
    g_emptyPoolAttemptsSkipped.store(0U, std::memory_order_release);
    g_poolInitializeCalls.store(0U, std::memory_order_release);
    for (auto& stats : g_managerMergeStats) {
        stats.manager.store(0U, std::memory_order_release);
        stats.registryCalls.store(0U, std::memory_order_release);
        stats.sourceCalls.store(0U, std::memory_order_release);
        stats.nonzeroSourceCalls.store(0U, std::memory_order_release);
        stats.mutations.store(0U, std::memory_order_release);
    }
    core::log::write(
        core::log::Channel::client,
        core::log::Level::info,
        "ev=bootflow stage=player_broadcast_create_probe result=armed mode=observe_only");
    return true;
}

/** Detaches the complete nested observer without changing simulation state. */
void uninstall_player_broadcast_create_probe() noexcept {
    clear_probe_handles();
    g_installAttempted.store(false, std::memory_order_release);
    g_resultLogged.store(false, std::memory_order_release);
    g_successSeen.store(false, std::memory_order_release);
    g_successManager.store(0U, std::memory_order_release);
    g_postSuccessFailureLogged.store(false, std::memory_order_release);
    g_emptyPoolAttemptsSkipped.store(0U, std::memory_order_release);
    g_poolInitializeCalls.store(0U, std::memory_order_release);
    for (auto& stats : g_managerMergeStats) {
        stats.manager.store(0U, std::memory_order_release);
        stats.registryCalls.store(0U, std::memory_order_release);
        stats.sourceCalls.store(0U, std::memory_order_release);
        stats.nonzeroSourceCalls.store(0U, std::memory_order_release);
        stats.mutations.store(0U, std::memory_order_release);
    }
    g_trace = {};
}

} // namespace sunrise::client::hooks::bootflow
