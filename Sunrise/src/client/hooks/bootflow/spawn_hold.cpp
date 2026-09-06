#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string_view>

#include "../../../core/logging/log.h"
#include "../../../core/settings/settings.h"
#include "../../../state/activity/runtime.h"
#include "../../../state/activity/forced/activity_forced_destination.h"
#include "../../hooking/call_gate.h"
#include "../../hooking/detour.h"
#include "internal.h"
#include "spawn_hold_policy.h"

namespace sunrise::client::hooks::bootflow {
namespace {

/**
 * The player spawn gate. Anchored on the load of the encrypted manager global, then run on
 * through the stack-cookie store because the wildcarded frame size leaves the head too short.
 */
constexpr std::string_view kSpawnGateSignatureText =
    "40 53 57 41 57 48 81 EC ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 84 24 ? ? ? ? 8B D9 "
    "40 B7 01";
/** Compiled pattern bytes of the signature text above. */
constexpr auto kSpawnGateSignature =
    signature<signature_length(kSpawnGateSignatureText)>(kSpawnGateSignatureText);

using SpawnGate = bool(__fastcall*)(std::int32_t) noexcept;
using SpawnDatum = void*(__fastcall*)(std::int32_t) noexcept;
using ReadWorldState = bool(__fastcall*)(std::int32_t*) noexcept;
using LocalReady = bool(__fastcall*)() noexcept;
using ActivitySlotLookup = bool(__fastcall*)(std::uint32_t, std::uint32_t, void*) noexcept;

struct ActivitySlotRecord final {
    std::int32_t datum{-1};
    std::uint32_t component{};
    std::uint64_t relativeOffset{};
};

/** RVAs recovered from the same retail build as the spawn-gate signature. */
constexpr std::ptrdiff_t kSpawnGateRva = 0xDC3360;
constexpr std::ptrdiff_t kSpawnDatumRva = 0xA55FF0;
constexpr std::ptrdiff_t kWorldStateRva = 0x4FFDA0;
constexpr std::ptrdiff_t kLocalReadyRva = 0xDC31A0;
constexpr std::ptrdiff_t kActivitySlotLookupRva = 0x4EA280;
/** World tag-loader manager and request pools, confirmed against the pinned image. */
constexpr std::ptrdiff_t kLoaderManagerRva = 0x4294D0;
constexpr std::ptrdiff_t kLoaderContextOnePoolRva = 0x1F8DD10;
constexpr std::ptrdiff_t kLoaderContextTwoPoolRva = 0x1F8DDB0;
constexpr std::ptrdiff_t kLoaderGenericTableRva = 0x2439C70;
constexpr std::ptrdiff_t kLoaderQueueHeadOffset = 0x220050;
constexpr std::ptrdiff_t kLoaderCurrentOffset = 0x220068;
constexpr std::size_t kLoaderRequestNameOffset = 0x44;
constexpr std::size_t kLoaderRequestNameCapacity = 40;
constexpr std::uint32_t kInvalidLoaderHandle = std::numeric_limits<std::uint32_t>::max();
/** First bytes of +4294D0. A mismatch disables only the loader extension. */
constexpr std::array<std::uint8_t, 7> kLoaderManagerPrefix{
    0x48U, 0x83U, 0xECU, 0x28U, 0x48U, 0x8BU, 0x05U};
/** The native spawn gate reads this byte from the participation record. */
constexpr std::ptrdiff_t kParticipationSpawnFlagOffset = 0xA;

using LoaderManager = std::uintptr_t(__fastcall*)() noexcept;

hooking::detour::Handle g_handle{};
std::atomic<SpawnGate> g_original{nullptr};
std::atomic<SpawnDatum> g_spawnDatum{nullptr};
std::atomic<ReadWorldState> g_worldState{nullptr};
std::atomic<LocalReady> g_localReady{nullptr};
std::atomic<ActivitySlotLookup> g_activitySlotLookup{nullptr};
std::atomic<LoaderManager> g_loaderManager{nullptr};
std::atomic<std::byte*> g_image{nullptr};
std::atomic_bool g_arrivalReported{};
std::atomic_bool g_loaderHoldReported{};
std::atomic_uint64_t g_holdStartedTick{};
hooking::CallGate g_callGate{};

/** @return True when no spawn replacement call owns this owner's retained state. */
[[nodiscard]] bool calls_idle() noexcept {
    return g_callGate.idle();
}

/** Roster slots that distinguish a registered authority object from a constructed runtime. */
constexpr std::array<std::uint32_t, 6> kDiagnosticSlotTypes{13, 16, 17, 18, 35, 41};

template <typename Value>
[[nodiscard]] Value safe_read(const void* address, Value fallback = {}) noexcept {
    Value value = fallback;
    __try {
        if (address != nullptr) {
            value = *static_cast<const Value*>(address);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        value = fallback;
    }
    return value;
}

[[nodiscard]] bool prefix_matches(const std::byte* target) noexcept {
    if (target == nullptr) {
        return false;
    }
    for (std::size_t index = 0; index < kLoaderManagerPrefix.size(); ++index) {
        if (safe_read<std::uint8_t>(target + index, 0U) != kLoaderManagerPrefix[index]) {
            return false;
        }
    }
    return true;
}

/** Resolves a loader datum exactly as the retail request service does. */
[[nodiscard]] std::uintptr_t loader_request_record(std::uint32_t handle) noexcept {
    std::byte* const image = g_image.load(std::memory_order_acquire);
    if (image == nullptr || handle == kInvalidLoaderHandle) {
        return 0U;
    }
    const std::uint32_t type = (handle >> 13U) & 0x3FFU;
    if (type == 0x3C7U || type == 0x3C6U) {
        const std::byte* const pool =
            image + (type == 0x3C7U ? kLoaderContextOnePoolRva : kLoaderContextTwoPoolRva);
        const std::uintptr_t base = safe_read<std::uintptr_t>(pool + 8U, 0U);
        const std::uint32_t stride = safe_read<std::uint32_t>(pool + 0x10U, 0U);
        if (base != 0U && base != 0xDEADBEEFDEADBEEFULL && stride != 0U
            && stride < 0x10000U) {
            return base + static_cast<std::uintptr_t>(handle & 0x1FFFU) * stride;
        }
        return 0U;
    }

    const std::uintptr_t table = safe_read<std::uintptr_t>(image + kLoaderGenericTableRva, 0U);
    if (table == 0U || table == 0xDEADBEEFDEADBEEFULL) {
        return 0U;
    }
    const std::uintptr_t descriptor = table + static_cast<std::uintptr_t>(type) * 0x40U;
    const std::uintptr_t base = safe_read<std::uintptr_t>(
        reinterpret_cast<const void*>(descriptor + 8U), 0U);
    const std::uint32_t stride = safe_read<std::uint32_t>(
        reinterpret_cast<const void*>(descriptor + 0x30U), 0U);
    const std::uint32_t mask = safe_read<std::uint32_t>(
        reinterpret_cast<const void*>(descriptor + 0x34U), 0U);
    if (base == 0U || base == 0xDEADBEEFDEADBEEFULL || stride == 0U
        || stride > 0x100000U) {
        return 0U;
    }
    const std::uintptr_t record =
        base + static_cast<std::uintptr_t>(handle & 0x1FFFU) * stride;
    const std::uint64_t adjust = safe_read<std::uint64_t>(
        reinterpret_cast<const void*>(record + 8U), 0xDEADBEEFDEADBEEFULL);
    if (adjust == 0xDEADBEEFDEADBEEFULL) {
        return record;
    }
    return record
           - (static_cast<std::uint64_t>(static_cast<std::int32_t>(mask)) & adjust);
}

[[nodiscard]] char lower_ascii(char value) noexcept {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) : value;
}

[[nodiscard]] bool contains_ascii(std::string_view text, std::string_view needle) noexcept {
    if (needle.empty() || needle.size() > text.size()) {
        return false;
    }
    for (std::size_t offset = 0; offset + needle.size() <= text.size(); ++offset) {
        bool match = true;
        for (std::size_t index = 0; index < needle.size(); ++index) {
            if (lower_ascii(text[offset + index]) != lower_ascii(needle[index])) {
                match = false;
                break;
            }
        }
        if (match) {
            return true;
        }
    }
    return false;
}

struct LoaderRequest final {
    std::array<char, kLoaderRequestNameCapacity + 1U> name{};
    std::uint32_t handle{kInvalidLoaderHandle};
    std::uint8_t state{0xFFU};
    std::uint8_t source{0xFFU};
    std::uint8_t context{0xFFU};
    bool valid{};
    bool worldPrecache{};
};

[[nodiscard]] LoaderRequest observe_loader_request(std::uint32_t handle) noexcept {
    LoaderRequest result{};
    result.handle = handle;
    const std::uintptr_t record = loader_request_record(handle);
    if (record == 0U) {
        return result;
    }
    result.state = safe_read<std::uint8_t>(reinterpret_cast<const void*>(record + 7U), 0xFFU);
    result.source = safe_read<std::uint8_t>(reinterpret_cast<const void*>(record + 5U), 0xFFU);
    result.context = safe_read<std::uint8_t>(reinterpret_cast<const void*>(record + 6U), 0xFFU);
    __try {
        std::memcpy(result.name.data(),
                    reinterpret_cast<const void*>(record + kLoaderRequestNameOffset),
                    kLoaderRequestNameCapacity);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        result.name = {};
    }
    result.name.back() = '\0';
    std::size_t nameLength = 0U;
    while (nameLength < kLoaderRequestNameCapacity && result.name[nameLength] != '\0') {
        const unsigned char value = static_cast<unsigned char>(result.name[nameLength]);
        if (value < 0x20U || value > 0x7EU) {
            result.name[nameLength] = '?';
        }
        ++nameLength;
    }
    const std::string_view name(result.name.data(), nameLength);
    const bool active = result.state == 0U || result.state == 1U;
    const bool namedSlice = contains_ascii(name, "slice") && contains_ascii(name, "precache");
    const bool namedActivity = contains_ascii(name, "activity") && contains_ascii(name, "load");
    result.valid = true;
    result.worldPrecache = active && (result.source == 6U || namedSlice || namedActivity);
    return result;
}

struct LoaderObservation final {
    LoaderRequest current{};
    LoaderRequest queued{};
    bool readable{};
    bool busy{};
};

/** Reads only the current request and queue head, which are the loader's own scheduling gates. */
[[nodiscard]] LoaderObservation observe_loader() noexcept {
    LoaderObservation result{};
    const LoaderManager accessor = g_loaderManager.load(std::memory_order_acquire);
    if (accessor == nullptr) {
        return result;
    }
    std::uintptr_t manager = 0U;
    __try {
        manager = accessor();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        manager = 0U;
    }
    if (manager == 0U) {
        return result;
    }
    result.readable = true;
    const std::uint32_t current = safe_read<std::uint32_t>(
        reinterpret_cast<const void*>(manager + kLoaderCurrentOffset), kInvalidLoaderHandle);
    const std::uint32_t queued = safe_read<std::uint32_t>(
        reinterpret_cast<const void*>(manager + kLoaderQueueHeadOffset), kInvalidLoaderHandle);
    result.current = observe_loader_request(current);
    if (queued != current) {
        result.queued = observe_loader_request(queued);
    }
    result.busy = result.current.worldPrecache || result.queued.worldPrecache;
    return result;
}

void report_loader_hold(const LoaderObservation& loader,
                        std::uint64_t age,
                        const char* result) noexcept {
    std::array<char, core::log::kLineCapacity> line{};
    const int written = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=spawn_hold_loader result=%s age=%llu busy=%u "
        "current=%08X current_state=%u current_source=%u current_context=%u current_name=\"%s\" "
        "queued=%08X queued_state=%u queued_source=%u queued_context=%u queued_name=\"%s\"",
        result,
        static_cast<unsigned long long>(age),
        loader.busy ? 1U : 0U,
        loader.current.handle,
        static_cast<unsigned>(loader.current.state),
        static_cast<unsigned>(loader.current.source),
        static_cast<unsigned>(loader.current.context),
        loader.current.name.data(),
        loader.queued.handle,
        static_cast<unsigned>(loader.queued.state),
        static_cast<unsigned>(loader.queued.source),
        static_cast<unsigned>(loader.queued.context),
        loader.queued.name.data());
    if (written > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), (std::min)(static_cast<std::size_t>(written),
                                                  core::log::kLineCapacity - 1U)});
    }
}

/** Reports the first record of each Homecoming roster type after the native arrival gate opens. */
void report_activity_slots(ActivitySlotLookup lookup) noexcept {
    if (lookup == nullptr) {
        return;
    }
    for (const std::uint32_t type : kDiagnosticSlotTypes) {
        ActivitySlotRecord record{};
        const bool found = lookup(type, 0, &record);
        std::array<char, 224> line{};
        const int written = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_slot_runtime type=%u index=0 result=%s datum=0x%X "
            "component=0x%X relative_offset=0x%llX",
            type,
            found ? "found" : "missing",
            static_cast<unsigned>(record.datum),
            record.component,
            static_cast<unsigned long long>(record.relativeOffset));
        if (written > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(written)});
        }
    }
}

/** Records the native gate's immediate inputs without changing the gate answer. */
void report_arrival(std::int32_t datum, bool allowed) noexcept {
    if (g_arrivalReported.exchange(true, std::memory_order_relaxed)) {
        return;
    }
    const SpawnDatum readDatum = g_spawnDatum.load(std::memory_order_acquire);
    void* const participation = readDatum != nullptr ? readDatum(datum) : nullptr;
    const unsigned spawnFlag = participation != nullptr
                                   ? static_cast<unsigned>(*reinterpret_cast<const std::uint8_t*>(
                                         static_cast<const std::byte*>(participation)
                                         + kParticipationSpawnFlagOffset))
                                   : 0xFFU;
    std::int32_t worldState = -2;
    const ReadWorldState readWorld = g_worldState.load(std::memory_order_acquire);
    const bool worldReadable = readWorld != nullptr && readWorld(&worldState);
    const LocalReady readLocal = g_localReady.load(std::memory_order_acquire);
    const bool localReady = readLocal != nullptr && readLocal();
    ActivitySlotRecord script{};
    ActivitySlotRecord director{};
    const ActivitySlotLookup lookup = g_activitySlotLookup.load(std::memory_order_acquire);
    const bool hasScript = lookup != nullptr && lookup(18, 0, &script);
    const bool hasDirector = lookup != nullptr && lookup(35, 0, &director);
    std::array<char, core::log::kLineCapacity> line{};
    const int written = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=spawn_gate result=%s datum=0x%X participation=%p "
        "flag10=%u world_ok=%u world_state=%d local_ready=%u "
        "script_ok=%u script=0x%X script_component=0x%X script_relative_offset=0x%llX "
        "director_ok=%u director=0x%X director_component=0x%X director_relative_offset=0x%llX",
        allowed ? "native_allowed" : "native_held",
        static_cast<unsigned>(datum),
        participation,
        spawnFlag,
        worldReadable ? 1U : 0U,
        worldState,
        localReady ? 1U : 0U,
        hasScript ? 1U : 0U,
        static_cast<unsigned>(script.datum),
        script.component,
        static_cast<unsigned long long>(script.relativeOffset),
        hasDirector ? 1U : 0U,
        static_cast<unsigned>(director.datum),
        director.component,
        static_cast<unsigned long long>(director.relativeOffset));
    if (written > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(written)});
    }
    report_activity_slots(lookup);
}

/**
 * Replaces Towerfall's unreachable migration-alias acknowledgement with the narrowest native
 * boundary available in this client, then arms the native launch producer. The manager-update
 * owner keeps retrying that producer until Destiny reaches native component dispatch.
 */
void attempt_towerfall_native_bootstrap(std::int32_t datum,
                                        bool allowed,
                                        state::activity::WorldPhase phase) noexcept {
    (void)datum;
    state::activity::forced::ForcedDestination destination{};
    state::activity::forced::snapshot(destination);
    const std::size_t packageLength =
        destination.packageNameLength <= destination.packageName.size()
            ? destination.packageNameLength
            : destination.packageName.size();
    if (!state::activity::forced::override_active()
        || std::string_view(destination.packageName.data(), packageLength)
               != "mission_towerfall") {
        return;
    }
    std::int32_t worldState = -2;
    const ReadWorldState readWorld = g_worldState.load(std::memory_order_acquire);
    const bool worldReadable = readWorld != nullptr && readWorld(&worldState);
    const LocalReady readLocal = g_localReady.load(std::memory_order_acquire);
    const bool localReady = readLocal != nullptr && readLocal();
    ActivitySlotRecord script{};
    ActivitySlotRecord director{};
    const ActivitySlotLookup lookup = g_activitySlotLookup.load(std::memory_order_acquire);
    const bool hasScript = lookup != nullptr && lookup(18, 0, &script);
    const bool hasDirector = lookup != nullptr && lookup(35, 0, &director);
    const spawn_hold_policy::Phase policyPhase =
        phase == state::activity::WorldPhase::arrived
            ? spawn_hold_policy::Phase::arrived
            : phase == state::activity::WorldPhase::transitioning
                  ? spawn_hold_policy::Phase::transitioning
                  : spawn_hold_policy::Phase::idle;
    const spawn_hold_policy::TowerfallReadiness evidence{
        allowed,
        policyPhase,
        prologue_filler_ready_armed(),
        worldReadable,
        worldState,
        localReady,
        hasScript,
        hasDirector,
    };
    if (!spawn_hold_policy::towerfall_ready(evidence)) {
        return;
    }

    if (state::activity::forced::mark_towerfall_native_ready()) {
        core::log::write(
            core::log::Channel::client,
            core::log::Level::info,
            "ev=gameplay stage=opening_host_ready result=latched package=mission_towerfall trigger=native_spawn_runtime");
    }
    // The retired lifecycle-event experiment called managed-session migration with an activity
    // definition payload. Towerfall now enters through the same no-argument native launch producer
    // as a normal mission; no synthetic lifecycle event is emitted here.
    arm_towerfall_executor_bootstrap();
}

/**
 * Puts the spawn after the world-transition fade is armed.
 * A release on a channel that is not up does nothing, so a spawn during the load leaves the
 * screen black. The client's own predicate reads a host field this destination never fills.
 * @param datum Borrowed player datum handle; the answer does not depend on it.
 * @return The native answer, or held while a destination load is still running.
 */
__declspec(noinline) bool __fastcall spawn_gate(std::int32_t datum) noexcept {
    hooking::CallGate::Scope call(g_callGate);
    const SpawnGate original = hooking::await_original(g_original);
    const bool allowed = original(datum);
    if (!call.accepts_side_effects()) {
        return allowed;
    }

    observe_world_step();
    if (!call.accepts_side_effects()) {
        return allowed;
    }

    const state::activity::WorldPhase phase = state::activity::world_phase();
    const bool transitioning = phase == state::activity::WorldPhase::transitioning;
    const spawn_hold_policy::Phase policyPhase =
        transitioning ? spawn_hold_policy::Phase::transitioning
        : phase == state::activity::WorldPhase::arrived ? spawn_hold_policy::Phase::arrived
                                                        : spawn_hold_policy::Phase::idle;
    const std::uint64_t now = GetTickCount64();
    if (transitioning) {
        std::uint64_t expected = 0U;
        (void)g_holdStartedTick.compare_exchange_strong(
            expected, now, std::memory_order_relaxed, std::memory_order_relaxed);
        g_arrivalReported.store(false, std::memory_order_relaxed);
        g_loaderHoldReported.store(false, std::memory_order_relaxed);
    } else if (phase == state::activity::WorldPhase::idle) {
        g_holdStartedTick.store(0U, std::memory_order_relaxed);
        g_loaderHoldReported.store(false, std::memory_order_relaxed);
    } else if (phase == state::activity::WorldPhase::arrived
               && g_holdStartedTick.load(std::memory_order_relaxed) == 0U
               && !g_arrivalReported.load(std::memory_order_relaxed)) {
        // Preserve a timeout even if this hook first observes the transition on its arrival tick.
        g_holdStartedTick.store(now, std::memory_order_relaxed);
    }

    const std::uint64_t started = g_holdStartedTick.load(std::memory_order_relaxed);
    const std::uint64_t age = started != 0U ? now - started
                                           : state::activity::world_transition_age();
    const core::settings::client::Settings& client = core::settings::get().client;
    const bool gaveUp = age >= client.spawnHoldMs;
    const bool released = g_arrivalReported.load(std::memory_order_relaxed);
    LoaderObservation loader{};
    if (phase == state::activity::WorldPhase::arrived && client.holdSpawn && !released) {
        loader = observe_loader();
    }
    const spawn_hold_policy::Decision decision = spawn_hold_policy::decide(
        spawn_hold_policy::Input{
            allowed, policyPhase, client.holdSpawn, gaveUp, released, loader.busy});

    if (call.accepts_side_effects() && decision.loaderLoading
        && !g_loaderHoldReported.exchange(true, std::memory_order_relaxed)) {
        report_loader_hold(loader, age, "held");
    }

    // Release only on arrival. The step-37 exit re-arms the fade unless one is already up, and
    // nothing polls this gate after the spawn, so an early release leaves a fade nobody clears.
    // Retail also leaves the player in its tunnel while world precache/activity requests remain.
    if (call.accepts_side_effects() && decision.releaseFade) {
        if (g_loaderHoldReported.exchange(false, std::memory_order_relaxed)) {
            report_loader_hold(loader, age, gaveUp ? "timeout" : "released");
        }
        report_arrival(datum, allowed);
        if (call.accepts_side_effects()) {
            release_world_fade();
        }
        g_holdStartedTick.store(0U, std::memory_order_relaxed);
    }
    if (call.accepts_side_effects() && decision.result
        && phase == state::activity::WorldPhase::arrived) {
        attempt_towerfall_native_bootstrap(datum, allowed, phase);
    }
    return call.accepts_side_effects() ? decision.result : allowed;
}

} // namespace

/** Attaches the spawn hold. */
bool install_spawn_hold() noexcept {
    if (g_handle.attached) {
        return g_callGate.accepting();
    }
    g_callGate.quiesce();
    std::byte* const target = scan_main_image_unique(kSpawnGateSignature, "player_spawn_gate");
    if (target == nullptr) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::warn,
                         "ev=bootflow stage=spawn_hold result=fail reason=target");
        return false;
    }
    std::byte* const image = target - kSpawnGateRva;
    std::byte* const loaderTarget = image + kLoaderManagerRva;
    g_image.store(image, std::memory_order_release);
    if (prefix_matches(loaderTarget)) {
        g_loaderManager.store(reinterpret_cast<LoaderManager>(loaderTarget),
                              std::memory_order_release);
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         "ev=bootflow stage=spawn_hold_loader install=ok");
    } else {
        g_loaderManager.store(nullptr, std::memory_order_release);
        core::log::write(core::log::Channel::client,
                         core::log::Level::warn,
                         "ev=bootflow stage=spawn_hold_loader install=disabled reason=prefix");
    }
    g_spawnDatum.store(reinterpret_cast<SpawnDatum>(
                           target + (kSpawnDatumRva - kSpawnGateRva)),
                       std::memory_order_release);
    g_worldState.store(reinterpret_cast<ReadWorldState>(
                           target + (kWorldStateRva - kSpawnGateRva)),
                       std::memory_order_release);
    g_localReady.store(reinterpret_cast<LocalReady>(
                           target + (kLocalReadyRva - kSpawnGateRva)),
                       std::memory_order_release);
    g_activitySlotLookup.store(reinterpret_cast<ActivitySlotLookup>(
                                   target + (kActivitySlotLookupRva - kSpawnGateRva)),
                               std::memory_order_release);
    const hooking::detour::Spec spec{target, reinterpret_cast<void*>(&spawn_gate)};
    if (!hooking::detour::install(spec, g_handle)) {
        g_spawnDatum.store(nullptr, std::memory_order_release);
        g_worldState.store(nullptr, std::memory_order_release);
        g_localReady.store(nullptr, std::memory_order_release);
        g_activitySlotLookup.store(nullptr, std::memory_order_release);
        g_loaderManager.store(nullptr, std::memory_order_release);
        g_image.store(nullptr, std::memory_order_release);
        core::log::write(core::log::Channel::client,
                         core::log::Level::warn,
                         "ev=bootflow stage=spawn_hold result=fail reason=attach");
        return false;
    }
    hooking::publish_original(g_original, reinterpret_cast<SpawnGate>(g_handle.original));
    g_callGate.accept();
    core::log::write(core::log::Channel::client,
                     core::log::Level::info,
                     "ev=bootflow stage=spawn_hold result=ok");
    return true;
}

/** Stops Sunrise-owned spawn work while retaining native forwarding. */
void quiesce_spawn_hold() noexcept {
    g_callGate.quiesce();
}

/** Detaches the spawn hold. */
bool uninstall_spawn_hold() noexcept {
    quiesce_spawn_hold();
    if (!g_handle.attached) {
        return true;
    }

    const std::array protectedEntries{
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&spawn_gate)},
        hooking::detour::ProtectedCodeEntry{
            reinterpret_cast<void*>(&hooking::call_gate_detail::enter)},
        hooking::detour::ProtectedCodeEntry{
            reinterpret_cast<void*>(&hooking::call_gate_detail::leave)},
    };
    const hooking::detour::UninstallResult result =
        hooking::detour::uninstall(g_handle, protectedEntries, &calls_idle);
    if (result != hooking::detour::UninstallResult::removed) {
        core::log::write(
            core::log::Channel::client,
            result == hooking::detour::UninstallResult::failed ? core::log::Level::error
                                                               : core::log::Level::warn,
            result == hooking::detour::UninstallResult::failed
                ? "ev=bootflow stage=spawn_hold_uninstall result=failed retained=1"
                : "ev=bootflow stage=spawn_hold_uninstall result=deferred retained=1");
        return false;
    }

    g_original.store(nullptr, std::memory_order_release);
    g_spawnDatum.store(nullptr, std::memory_order_release);
    g_worldState.store(nullptr, std::memory_order_release);
    g_localReady.store(nullptr, std::memory_order_release);
    g_activitySlotLookup.store(nullptr, std::memory_order_release);
    g_loaderManager.store(nullptr, std::memory_order_release);
    g_image.store(nullptr, std::memory_order_release);
    g_arrivalReported.store(false, std::memory_order_release);
    g_loaderHoldReported.store(false, std::memory_order_release);
    g_holdStartedTick.store(0U, std::memory_order_release);
    core::log::write(core::log::Channel::client,
                     core::log::Level::info,
                     "ev=bootflow stage=spawn_hold_uninstall result=ok retained=0");
    return true;
}

} // namespace sunrise::client::hooks::bootflow
