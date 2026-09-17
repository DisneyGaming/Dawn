#include "eater_entity_id_startup_policy.h"

#include <Windows.h>

#include <array>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string_view>

#include "../../../core/logging/log.h"
#include "../../../state/activity/destination/activity_destination_snapshot.h"
#include "../../../state/activity/runtime.h"
#include "../../activity/mission_launch.h"
#include "../../hooking/call_gate.h"
#include "../../hooking/detour.h"
#include "internal.h"

namespace dawn::client::hooks::bootflow {
namespace {

namespace startup = eater_entity_id_startup;
namespace mission_launch = client::activity::mission_launch;

/** Native manager maintenance that requests or returns owned entity-ID lease bits. */
constexpr std::uintptr_t kMaintenanceRva = 0x171DB20U;
/** Native merge of an externally delivered 8,192-bit entity-ID lease mask. */
constexpr std::uintptr_t kSourceMergeRva = 0x17129E0U;
/** Getter for the live simulation root used to reject stale-domain manager callbacks. */
constexpr std::uintptr_t kSimulationRootRva = 0x16C0BA0U;

constexpr std::size_t kPoolOffset = 0xC118U;
constexpr std::size_t kProfileOffset = 0xC518U;
constexpr std::size_t kMaskBytes = 1'024U;
constexpr std::size_t kMaskWords = kMaskBytes / sizeof(std::uint64_t);
constexpr std::size_t kDomainOffset = 0x560E0U;
constexpr std::size_t kFirstManagerOffset = 0x206C8U + 0x270U;
constexpr std::size_t kManagerStride = 0x11E08U;
constexpr std::uint32_t kDomainCount = 3U;
constexpr std::uint32_t kReceiptLimit = 48U;

static_assert(kProfileOffset % alignof(std::uint64_t) == 0U);

constexpr std::array<std::byte, 23> kMaintenancePrefix{
    std::byte{0x40}, std::byte{0x53}, std::byte{0x55}, std::byte{0x57},
    std::byte{0x48}, std::byte{0x81}, std::byte{0xEC}, std::byte{0x40},
    std::byte{0x04}, std::byte{0x00}, std::byte{0x00}, std::byte{0x48},
    std::byte{0x8B}, std::byte{0x05}, std::byte{0x56}, std::byte{0xBF},
    std::byte{0x98}, std::byte{0x00}, std::byte{0x48}, std::byte{0x33},
    std::byte{0xC4}, std::byte{0x48}, std::byte{0x89}};
constexpr std::array<std::byte, 20> kSourceMergePrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x08}, std::byte{0x57}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x40}, std::byte{0x48}, std::byte{0x8B},
    std::byte{0xD9}, std::byte{0x41}, std::byte{0xB9}, std::byte{0x00},
    std::byte{0x20}, std::byte{0x00}, std::byte{0x00}, std::byte{0x48}};
constexpr std::array<std::byte, 18> kSimulationRootPrefix{
    std::byte{0x40}, std::byte{0x53}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x20}, std::byte{0x48}, std::byte{0x8B},
    std::byte{0x1D}, std::byte{0xFB}, std::byte{0x13}, std::byte{0x99},
    std::byte{0x01}, std::byte{0x48}, std::byte{0x85}, std::byte{0xDB},
    std::byte{0x0F}, std::byte{0x84}};

using Maintenance = void(__fastcall*)(std::byte*) noexcept;
using SourceMerge = void(__fastcall*)(std::byte*, const std::byte*) noexcept;
using SimulationRoot = std::byte*(__fastcall*)() noexcept;

std::array<hooking::detour::Handle, 2> g_handles{};
std::atomic<Maintenance> g_maintenanceOriginal{};
std::atomic<SourceMerge> g_sourceMergeOriginal{};
std::atomic<SimulationRoot> g_simulationRoot{};
hooking::CallGate g_callGate{};
std::atomic_uint32_t g_receipts{};

struct PoolSnapshot final {
    std::uint32_t setBits{};
    bool readable{};
};

struct Qualification final {
    startup::Eligibility eligibility{};
    std::uint64_t session{};
    std::uint64_t run{};
};

class NativeProfileStore final {
public:
    explicit NativeProfileStore(std::byte* manager) noexcept
        : word_(reinterpret_cast<volatile LONG64*>(manager + kProfileOffset)) {}

    [[nodiscard]] bool compare_exchange(std::uint64_t& expected,
                                        std::uint64_t desired) noexcept {
        const LONG64 prior = InterlockedCompareExchange64(
            word_, static_cast<LONG64>(desired), static_cast<LONG64>(expected));
        if (static_cast<std::uint64_t>(prior) == expected) {
            return true;
        }
        expected = static_cast<std::uint64_t>(prior);
        return false;
    }

private:
    volatile LONG64* word_{};
};

template <class Value>
[[nodiscard]] bool safe_read(const void* address, Value& value) noexcept {
    SIZE_T copied = 0;
    return address != nullptr
           && ReadProcessMemory(GetCurrentProcess(), address, &value, sizeof(value), &copied)
                  != FALSE
           && copied == sizeof(value);
}

[[nodiscard]] PoolSnapshot snapshot_mask(const std::byte* mask) noexcept {
    PoolSnapshot result{};
    if (mask == nullptr) {
        return result;
    }
    std::array<std::uint64_t, kMaskWords> words{};
    SIZE_T copied = 0;
    if (ReadProcessMemory(GetCurrentProcess(), mask, words.data(), kMaskBytes, &copied) == FALSE
        || copied != kMaskBytes) {
        return result;
    }
    for (const std::uint64_t word : words) {
        result.setBits += static_cast<std::uint32_t>(std::popcount(word));
    }
    result.readable = true;
    return result;
}

[[nodiscard]] std::string_view package_name(
    const state::activity::forced::ForcedDestination& destination) noexcept {
    return destination.packageNameLength <= destination.packageName.size()
               ? std::string_view(destination.packageName.data(), destination.packageNameLength)
               : std::string_view{};
}

[[nodiscard]] std::string_view package_name(
    const state::activity::destination::DestinationSelection& destination) noexcept {
    return destination.packageNameLength <= destination.packageName.size()
               ? std::string_view(
                     reinterpret_cast<const char*>(destination.packageName.data()),
                     destination.packageNameLength)
               : std::string_view{};
}

[[nodiscard]] bool exact_requested_route(const mission_launch::Snapshot& launch) noexcept {
    const auto& destination = launch.destination;
    const startup::Eligibility eligibility{package_name(destination),
                                             destination.bubble,
                                             destination.sliceSet,
                                             destination.spawnSetHash,
                                             launch.busy && !launch.inMission,
                                             true,
                                             true,
                                             destination.hasBubble,
                                             destination.hasSliceSet,
                                             destination.hasSpawnSetHash};
    return startup::exact_entrance(eligibility);
}

[[nodiscard]] bool exact_committed_route(
    const state::activity::destination::DestinationSelection& destination) noexcept {
    const startup::Eligibility eligibility{package_name(destination),
                                             destination.arrivalBubbleOverride,
                                             destination.sliceSetOverride,
                                             destination.spawnSetOverride,
                                             true,
                                             true,
                                             true,
                                             destination.hasArrivalBubbleOverride,
                                             destination.hasSliceSetOverride,
                                             destination.hasSpawnSetOverride};
    return startup::exact_entrance(eligibility);
}

[[nodiscard]] bool current_simulation_manager(std::byte* manager) noexcept {
    const SimulationRoot resolve = g_simulationRoot.load(std::memory_order_acquire);
    if (manager == nullptr || resolve == nullptr) {
        return false;
    }
    std::byte* const root = resolve();
    std::uint32_t domain = kDomainCount;
    return root != nullptr && safe_read(root + kDomainOffset, domain) && domain < kDomainCount
           && manager == root + kFirstManagerOffset + domain * kManagerStride;
}

[[nodiscard]] Qualification qualify(std::byte* manager) noexcept {
    Qualification result{};
    const mission_launch::Snapshot launch = mission_launch::snapshot();
    if (!exact_requested_route(launch) || !current_simulation_manager(manager)) {
        return result;
    }
    const std::uint64_t session = state::activity::newest_joined_session();
    state::activity::destination::DestinationSelection committed{};
    if (session == state::activity::kAbsentSessionId
        || !state::activity::destination::snapshot(session, committed)
        || !exact_committed_route(committed)) {
        return result;
    }
    // exact_committed_route already compared the copied destination against the exact package.
    // Retain the process-lifetime constant here: committed is a stack snapshot and its package
    // bytes cannot back the string_view returned in Qualification.
    result.eligibility = {startup::kEntrancePackage,
                          committed.arrivalBubbleOverride,
                          committed.sliceSetOverride,
                          committed.spawnSetOverride,
                          true,
                          true,
                          true,
                          committed.hasArrivalBubbleOverride,
                          committed.hasSliceSetOverride,
                          committed.hasSpawnSetOverride};
    result.session = session;
    result.run = state::activity::mission_run_generation();
    return result;
}

void report(const char* stage,
            const char* result,
            std::byte* manager,
            Qualification qualification,
            startup::CacheProfile nativeProfile,
            PoolSnapshot before,
            PoolSnapshot source,
            PoolSnapshot after,
            std::uint32_t requestDeficit,
            bool restored) noexcept {
    const std::uint32_t receipt = g_receipts.fetch_add(1U, std::memory_order_relaxed) + 1U;
    if (receipt > kReceiptLimit) {
        return;
    }
    std::array<char, core::log::kLineCapacity> line{};
    const int written = std::snprintf(
        line.data(),
        line.size(),
        "ev=eater_entity_ids stage=%s result=%s n=%u owner=%016llX run=%llu "
        "manager=%p native_low=%u native_high=%u startup_low=%u startup_high=%u "
        "target=%u available_before=%u requested=%u source_bits=%u available_after=%u "
        "merged=%u profile_restored=%u lease=native_owned",
        stage,
        result,
        receipt,
        static_cast<unsigned long long>(qualification.session),
        static_cast<unsigned long long>(qualification.run),
        static_cast<void*>(manager),
        nativeProfile.low,
        nativeProfile.high,
        startup::kEntranceStartupProfile.low,
        startup::kEntranceStartupProfile.high,
        startup::midpoint(startup::kEntranceStartupProfile),
        before.setBits,
        requestDeficit,
        source.setBits,
        after.setBits,
        before.readable && after.readable && after.setBits > before.setBits
            ? after.setBits - before.setBits
            : 0U,
        restored ? 1U : 0U);
    if (written > 0 && static_cast<std::size_t>(written) < line.size()) {
        core::log::write(core::log::Channel::client,
                         restored ? core::log::Level::info : core::log::Level::warn,
                         {line.data(), static_cast<std::size_t>(written)});
    }
}

/** Runs retail maintenance with a temporary cache policy; allocation and leases stay native. */
__declspec(noinline) void __fastcall maintenance(std::byte* manager) noexcept {
    const hooking::CallGate::Scope call{g_callGate};
    const Maintenance original = hooking::await_original(g_maintenanceOriginal);
    if (!call.accepts_side_effects()) {
        original(manager);
        return;
    }
    const Qualification qualification = qualify(manager);
    std::uint64_t profileWord = 0;
    if (!startup::exact_entrance(qualification.eligibility)
        || !safe_read(manager + kProfileOffset, profileWord)) {
        original(manager);
        return;
    }
    const startup::CacheProfile nativeProfile = startup::unpack(profileWord);
    const PoolSnapshot before = snapshot_mask(manager + kPoolOffset);
    const startup::MaintenanceDecision decision = startup::decide(
        qualification.eligibility, nativeProfile, before.readable ? before.setBits : 0U);
    if (!before.readable || !decision.overrideProfile) {
        original(manager);
        return;
    }

    NativeProfileStore store{manager};
    startup::TemporaryProfileOverride override{
        store, startup::kRetailLocalProfile, decision.profile};
    if (!override.applied()) {
        original(manager);
        report("maintenance",
               "profile_changed",
               manager,
               qualification,
               nativeProfile,
               before,
               {},
               snapshot_mask(manager + kPoolOffset),
               0U,
               true);
        return;
    }
    original(manager);
    const bool restored = override.restore();
    const PoolSnapshot after = snapshot_mask(manager + kPoolOffset);
    if (decision.requestDeficit != 0U || after.setBits != before.setBits || !restored) {
        report("maintenance",
               decision.requestDeficit != 0U ? "requested" : "maintained",
               manager,
               qualification,
               nativeProfile,
               before,
               {},
               after,
               decision.requestDeficit,
               restored);
    }
}

/** Records the exact native-owned grant mask and the number of bits the client merged. */
__declspec(noinline) void __fastcall source_merge(std::byte* manager,
                                                   const std::byte* source) noexcept {
    const hooking::CallGate::Scope call{g_callGate};
    const SourceMerge original = hooking::await_original(g_sourceMergeOriginal);
    if (!call.accepts_side_effects()) {
        original(manager, source);
        return;
    }
    const Qualification qualification = qualify(manager);
    if (!startup::exact_entrance(qualification.eligibility)) {
        original(manager, source);
        return;
    }
    const PoolSnapshot before = snapshot_mask(manager + kPoolOffset);
    const PoolSnapshot delivered = snapshot_mask(source);
    original(manager, source);
    const PoolSnapshot after = snapshot_mask(manager + kPoolOffset);
    report("grant_merge",
           delivered.readable ? "merged" : "source_unreadable",
           manager,
           qualification,
           startup::kRetailLocalProfile,
           before,
           delivered,
           after,
           0U,
           true);
}

template <std::size_t Size>
[[nodiscard]] std::byte* checked_target(std::byte* image,
                                        std::uintptr_t rva,
                                        const std::array<std::byte, Size>& expected) noexcept {
    if (image == nullptr) {
        return nullptr;
    }
    std::array<std::byte, Size> actual{};
    SIZE_T copied = 0;
    std::byte* const target = image + rva;
    return ReadProcessMemory(GetCurrentProcess(), target, actual.data(), actual.size(), &copied)
                       != FALSE
                   && copied == actual.size() && actual == expected
               ? target
               : nullptr;
}

[[nodiscard]] bool callbacks_idle() noexcept {
    return g_callGate.idle();
}

} // namespace

bool install_eater_entity_id_startup() noexcept {
    if (g_handles[0].attached || g_handles[1].attached) {
        return g_handles[0].attached && g_handles[1].attached && g_callGate.accepting();
    }
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    std::byte* const maintenanceTarget =
        checked_target(image, kMaintenanceRva, kMaintenancePrefix);
    std::byte* const sourceMergeTarget =
        checked_target(image, kSourceMergeRva, kSourceMergePrefix);
    std::byte* const rootTarget =
        checked_target(image, kSimulationRootRva, kSimulationRootPrefix);
    if (maintenanceTarget == nullptr || sourceMergeTarget == nullptr || rootTarget == nullptr) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::warn,
                         "ev=eater_entity_ids stage=install result=signature_mismatch");
        return false;
    }

    const std::array<hooking::detour::Spec, 2> specs{{
        {maintenanceTarget, reinterpret_cast<void*>(&maintenance)},
        {sourceMergeTarget, reinterpret_cast<void*>(&source_merge)},
    }};
    if (!hooking::detour::install(specs, g_handles)) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::warn,
                         "ev=eater_entity_ids stage=install result=detour_failed");
        return false;
    }
    g_simulationRoot.store(reinterpret_cast<SimulationRoot>(rootTarget),
                           std::memory_order_release);
    hooking::publish_original(
        g_maintenanceOriginal, reinterpret_cast<Maintenance>(g_handles[0].original));
    hooking::publish_original(
        g_sourceMergeOriginal, reinterpret_cast<SourceMerge>(g_handles[1].original));
    g_receipts.store(0U, std::memory_order_release);
    g_callGate.accept();
    core::log::write(core::log::Channel::client,
                     core::log::Level::info,
                     "ev=eater_entity_ids stage=install result=ok scope=entrance_startup "
                     "target=400 lease=native_owned");
    return true;
}

void quiesce_eater_entity_id_startup() noexcept {
    g_callGate.quiesce();
}

bool uninstall_eater_entity_id_startup() noexcept {
    quiesce_eater_entity_id_startup();
    if (!g_handles[0].attached && !g_handles[1].attached) {
        return true;
    }
    const std::array<hooking::detour::ProtectedCodeEntry, 4> code{{
        {reinterpret_cast<void*>(&maintenance)},
        {reinterpret_cast<void*>(&source_merge)},
        {reinterpret_cast<void*>(&hooking::call_gate_detail::enter)},
        {reinterpret_cast<void*>(&hooking::call_gate_detail::leave)},
    }};
    if (hooking::detour::uninstall(g_handles, code, callbacks_idle)
        != hooking::detour::UninstallResult::removed) {
        return false;
    }
    g_maintenanceOriginal.store(nullptr, std::memory_order_release);
    g_sourceMergeOriginal.store(nullptr, std::memory_order_release);
    g_simulationRoot.store(nullptr, std::memory_order_release);
    g_receipts.store(0U, std::memory_order_release);
    return true;
}

} // namespace dawn::client::hooks::bootflow
