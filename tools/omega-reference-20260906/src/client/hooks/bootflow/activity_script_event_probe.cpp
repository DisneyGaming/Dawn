#include <Windows.h>

#include <intrin.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string_view>

#include "../../../core/logging/log.h"
#include "../../../state/activity/forced/activity_forced_destination.h"
#include "../../../state/activity/omega_presentation.h"
#include "../../hooking/detour.h"
#include "internal.h"
#include "legacy_owner_sentinel.h"

namespace sunrise::client::hooks::bootflow {
namespace {

/** Runtime-decrypted activity-script event callback in the pinned Season of Arrivals client. */
constexpr std::uintptr_t kEventRva = 0x1789B6EU;
/** Creates one of the activity-script component slots consumed by the event callback. */
constexpr std::uintptr_t kComponentRegisterRva = 0x17898A0U;
/** Stable helper called by the encrypted mission-director initializer at +0x501B20. */
constexpr std::uintptr_t kMissionDirectorRuntimeInitializeHelperRva = 0x4FF830U;
/** Stable datum resolver called by the encrypted mission-director apply callback at +0x501C10. */
constexpr std::uintptr_t kMissionDirectorRuntimeApplyResolverRva = 0x4A6340U;
/** Type-43 authoritative-state consumer used by Omega's opening scene component. */
constexpr std::uintptr_t kOmegaSceneAuthorityApplyRva = 0xB41DD0U;
/** Return addresses uniquely identifying the two encrypted mission-director callers. */
constexpr std::uintptr_t kMissionDirectorRuntimeInitializeReturnRva = 0x501B35U;
constexpr std::uintptr_t kMissionDirectorRuntimeApplyReturnRva = 0x501C3DU;
/** Omega opening-scene authoritative-state schema (scene_ikora_opens_portal). */
constexpr std::uint32_t kOmegaSceneAuthoritySchema = 0x8080626BU;
constexpr std::array<std::byte, 23> kComponentRegisterPrefix{
    std::byte{0x40}, std::byte{0x55}, std::byte{0x41}, std::byte{0x55}, std::byte{0x41},
    std::byte{0x56}, std::byte{0x41}, std::byte{0x57}, std::byte{0x48}, std::byte{0x8D},
    std::byte{0xAC}, std::byte{0x24}, std::byte{0x78}, std::byte{0xFD}, std::byte{0xFF},
    std::byte{0xFF}, std::byte{0x48}, std::byte{0x81}, std::byte{0xEC}, std::byte{0x88},
    std::byte{0x03}, std::byte{0x00}, std::byte{0x00}};
constexpr std::array<std::byte, 23> kEventPrefix{
    std::byte{0x40}, std::byte{0x53}, std::byte{0x40}, std::byte{0x55}, std::byte{0x56},
    std::byte{0x57}, std::byte{0x41}, std::byte{0x56}, std::byte{0x48}, std::byte{0x8D},
    std::byte{0xAC}, std::byte{0x24}, std::byte{0xE8}, std::byte{0xFD}, std::byte{0xFF},
    std::byte{0xFF}, std::byte{0x48}, std::byte{0x81}, std::byte{0xEC}, std::byte{0x18},
    std::byte{0x03}, std::byte{0x00}, std::byte{0x00}};
constexpr std::array<std::byte, 10> kMissionDirectorRuntimeInitializeHelperPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24}, std::byte{0x08},
    std::byte{0x57}, std::byte{0x48}, std::byte{0x83}, std::byte{0xEC}, std::byte{0x30}};
constexpr std::array<std::byte, 5> kMissionDirectorRuntimeApplyResolverPrefix{
    std::byte{0x48}, std::byte{0x8B}, std::byte{0x41}, std::byte{0x08}, std::byte{0xC3}};
constexpr std::array<std::byte, 16> kOmegaSceneAuthorityApplyPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24}, std::byte{0x08},
    std::byte{0x57}, std::byte{0x48}, std::byte{0x83}, std::byte{0xEC}, std::byte{0x30},
    std::byte{0x48}, std::byte{0x8B}, std::byte{0xFA}, std::byte{0x48}, std::byte{0x8B},
    std::byte{0xD9}};

using ActivityScriptEvent =
    void(__fastcall*)(std::byte* manager, const void* payload, std::uint32_t event) noexcept;
using ActivityScriptComponentRegister = bool(__fastcall*)(std::byte* manager,
                                                           std::int32_t slotIndex,
                                                           std::int32_t mode,
                                                           std::int32_t value) noexcept;
using MissionDirectorRuntimeInitializeHelper = void(__fastcall*)(std::byte* state) noexcept;
using MissionDirectorRuntimeApplyResolver =
    const std::byte*(__fastcall*)(const std::byte* datumKey, std::uint32_t schema) noexcept;
using OmegaSceneAuthorityApply =
    void(__fastcall*)(std::byte* component, const std::byte* datumKey) noexcept;

hooking::detour::Handle g_eventHandle{};
hooking::detour::Handle g_componentRegisterHandle{};
hooking::detour::Handle g_missionDirectorRuntimeInitializeHandle{};
hooking::detour::Handle g_missionDirectorRuntimeApplyHandle{};
hooking::detour::Handle g_omegaSceneAuthorityApplyHandle{};
std::atomic<ActivityScriptEvent> g_original{nullptr};
std::atomic<ActivityScriptComponentRegister> g_componentRegisterOriginal{nullptr};
std::atomic<MissionDirectorRuntimeInitializeHelper> g_missionDirectorRuntimeInitializeOriginal{
    nullptr};
std::atomic<MissionDirectorRuntimeApplyResolver> g_missionDirectorRuntimeApplyOriginal{nullptr};
std::atomic<OmegaSceneAuthorityApply> g_omegaSceneAuthorityApplyOriginal{nullptr};
std::atomic_uint32_t g_observed{};
std::atomic_uint32_t g_componentRegisterObserved{};
std::atomic_uint32_t g_missionDirectorRuntimeInitializeObserved{};
std::atomic_uint32_t g_missionDirectorRuntimeApplyObserved{};
std::atomic_uint32_t g_omegaSceneRuntimeResolveObserved{};
std::atomic_uint32_t g_omegaSceneAuthorityApplyObserved{};
SRWLOCK g_omegaSceneAuthorityObservationLock = SRWLOCK_INIT;
OmegaSceneAuthorityObservation g_lastOmegaSceneAuthorityObservation{};

struct MissionRuntimeSnapshot final {
    std::array<std::uint64_t, 8> words{};
    std::uint64_t hash = 1469598103934665603ULL;
    std::uint32_t nonzero = 0;
};

struct SceneAuthoritySnapshot final {
    std::array<std::uint32_t, 0xD4U / sizeof(std::uint32_t)> dwords{};
    std::uint64_t hash = 1469598103934665603ULL;
    std::uint32_t nonzero = 0;
};

/** Reads one bounded mission-state block without allowing a stale diagnostic pointer to fault. */
[[nodiscard]] bool snapshot_mission_runtime(const std::byte* source,
                                            MissionRuntimeSnapshot& snapshot) noexcept {
    if (source == nullptr) {
        return false;
    }
    __try {
        std::memcpy(snapshot.words.data(), source, sizeof snapshot.words);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        snapshot = {};
        return false;
    }
    const auto* const bytes = reinterpret_cast<const std::uint8_t*>(snapshot.words.data());
    for (std::size_t index = 0; index < sizeof snapshot.words; ++index) {
        snapshot.nonzero += bytes[index] != 0U ? 1U : 0U;
        snapshot.hash ^= bytes[index];
        snapshot.hash *= 1099511628211ULL;
    }
    return true;
}

/** Reads the complete decoded type-43 state so branch-dependent fields cannot hide past +0x40. */
[[nodiscard]] bool snapshot_scene_authority(const std::byte* source,
                                            SceneAuthoritySnapshot& snapshot) noexcept {
    if (source == nullptr) {
        return false;
    }
    __try {
        std::memcpy(snapshot.dwords.data(), source, sizeof snapshot.dwords);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        snapshot = {};
        return false;
    }
    const auto* const bytes = reinterpret_cast<const std::uint8_t*>(snapshot.dwords.data());
    for (std::size_t index = 0; index < sizeof snapshot.dwords; ++index) {
        snapshot.nonzero += bytes[index] != 0U ? 1U : 0U;
        snapshot.hash ^= bytes[index];
        snapshot.hash *= 1099511628211ULL;
    }
    return true;
}

[[nodiscard]] bool opening_forced_destination(std::string_view& package) noexcept {
    static thread_local state::activity::forced::ForcedDestination forced{};
    state::activity::forced::snapshot(forced);
    package = std::string_view(forced.packageName.data(), forced.packageNameLength);
    return state::activity::forced::override_active()
           && (package == "cine_110_twr" || package == "mission_towerfall"
               || package == "mission_scot");
}

/** Resolves and validates the component-slot registration entry point. */
[[nodiscard]] std::byte* component_register_target() noexcept {
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    if (image == nullptr) {
        return nullptr;
    }
    std::byte* const target = image + kComponentRegisterRva;
    for (std::size_t index = 0; index < kComponentRegisterPrefix.size(); ++index) {
        if (target[index] != kComponentRegisterPrefix[index]) {
            return nullptr;
        }
    }
    return target;
}

/** Resolves and validates the exact callback without depending on encrypted on-disk code. */
[[nodiscard]] std::byte* event_target() noexcept {
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    if (image == nullptr) {
        return nullptr;
    }
    std::byte* const target = image + kEventRva;
    for (std::size_t index = 0; index < kEventPrefix.size(); ++index) {
        if (target[index] != kEventPrefix[index]) {
            return nullptr;
        }
    }
    return target;
}

/** Resolves the stable helper beneath the runtime-encrypted mission-director initializer. */
[[nodiscard]] std::byte* mission_director_runtime_initialize_target() noexcept {
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    if (image == nullptr) {
        return nullptr;
    }
    std::byte* const target = image + kMissionDirectorRuntimeInitializeHelperRva;
    for (std::size_t index = 0;
         index < kMissionDirectorRuntimeInitializeHelperPrefix.size();
         ++index) {
        if (target[index] != kMissionDirectorRuntimeInitializeHelperPrefix[index]) {
            return nullptr;
        }
    }
    return target;
}

/** Resolves the stable datum resolver beneath the runtime-encrypted authoritative-state copy. */
[[nodiscard]] std::byte* mission_director_runtime_apply_target() noexcept {
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    if (image == nullptr) {
        return nullptr;
    }
    std::byte* const target = image + kMissionDirectorRuntimeApplyResolverRva;
    for (std::size_t index = 0; index < kMissionDirectorRuntimeApplyResolverPrefix.size(); ++index) {
        if (target[index] != kMissionDirectorRuntimeApplyResolverPrefix[index]) {
            return nullptr;
        }
    }
    return target;
}

/** Resolves the exact native consumer that copies type-43 state into the live scene component. */
[[nodiscard]] std::byte* omega_scene_authority_apply_target() noexcept {
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    if (image == nullptr) {
        return nullptr;
    }
    std::byte* const target = image + kOmegaSceneAuthorityApplyRva;
    for (std::size_t index = 0; index < kOmegaSceneAuthorityApplyPrefix.size(); ++index) {
        if (target[index] != kOmegaSceneAuthorityApplyPrefix[index]) {
            return nullptr;
        }
    }
    return target;
}

/**
 * Records the source fields and the two live component fields used by the type-43 consumer.
 * This observer never edits either block; it exists to distinguish a malformed wire body from a
 * correctly decoded scene state that the native component subsequently refuses to run.
 */
__declspec(noinline) void __fastcall
omega_scene_authority_apply(std::byte* component, const std::byte* datumKey) noexcept {
    std::string_view package{};
    const bool opening = opening_forced_destination(package);

    std::uint32_t datumIdentity = 0U;
    const std::byte* source = nullptr;
    std::uint32_t componentValueBefore = 0U;
    std::uint8_t componentActiveBefore = 0U;
    if (opening) {
        __try {
            if (datumKey != nullptr) {
                std::memcpy(&datumIdentity, datumKey, sizeof datumIdentity);
                source = *reinterpret_cast<const std::byte* const*>(datumKey + 8);
            }
            if (component != nullptr) {
                std::memcpy(&componentValueBefore, component + 0x25C, sizeof componentValueBefore);
                std::memcpy(&componentActiveBefore,
                            component + 0x260,
                            sizeof componentActiveBefore);
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            datumIdentity = 0U;
            source = nullptr;
            componentValueBefore = 0U;
            componentActiveBefore = 0U;
        }
    }

    SceneAuthoritySnapshot sourceSnapshot{};
    const bool sourceValid = opening && snapshot_scene_authority(source, sourceSnapshot);
    const OmegaSceneAuthorityApply original =
        g_omegaSceneAuthorityApplyOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(component, datumKey);
    }

    if (!opening) {
        return;
    }
    std::uint32_t componentValueAfter = 0U;
    std::uint8_t componentActiveAfter = 0U;
    __try {
        if (component != nullptr) {
            std::memcpy(&componentValueAfter, component + 0x25C, sizeof componentValueAfter);
            std::memcpy(&componentActiveAfter, component + 0x260, sizeof componentActiveAfter);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        componentValueAfter = 0U;
        componentActiveAfter = 0U;
    }

    const std::uint32_t observation =
        g_omegaSceneAuthorityApplyObserved.fetch_add(1, std::memory_order_relaxed) + 1U;
    const auto field = [&sourceSnapshot](std::size_t offset) noexcept {
        const std::size_t index = offset / sizeof(std::uint32_t);
        return index < sourceSnapshot.dwords.size() ? sourceSnapshot.dwords[index] : 0U;
    };
    if (package == "mission_scot" && sourceValid && component != nullptr) {
        std::uint32_t definition{};
        __try { std::memcpy(&definition, component, sizeof definition); }
        __except (EXCEPTION_EXECUTE_HANDLER) { definition = 0; }
        state::activity::omega_presentation::observe_scene(definition, field(0x08U) == 1U);
    }
    OmegaSceneAuthorityObservation authorityObservation{};
    authorityObservation.tickMs = GetTickCount64();
    authorityObservation.sourceHash = sourceSnapshot.hash;
    authorityObservation.component = reinterpret_cast<std::uintptr_t>(component);
    authorityObservation.datumKey = reinterpret_cast<std::uintptr_t>(datumKey);
    authorityObservation.source = reinterpret_cast<std::uintptr_t>(source);
    authorityObservation.sequence = observation;
    authorityObservation.datumIdentity = datumIdentity;
    authorityObservation.source00 = field(0x00U);
    authorityObservation.source04 = field(0x04U);
    authorityObservation.source08 = field(0x08U);
    authorityObservation.source4C = field(0x4CU);
    authorityObservation.source98 = field(0x98U);
    authorityObservation.componentValueBefore = componentValueBefore;
    authorityObservation.componentValueAfter = componentValueAfter;
    authorityObservation.componentActiveBefore = componentActiveBefore;
    authorityObservation.componentActiveAfter = componentActiveAfter;
    authorityObservation.sourceValid = sourceValid;
    AcquireSRWLockExclusive(&g_omegaSceneAuthorityObservationLock);
    g_lastOmegaSceneAuthorityObservation = authorityObservation;
    ReleaseSRWLockExclusive(&g_omegaSceneAuthorityObservationLock);
    if (observation > 64U) {
        return;
    }
    std::array<char, 1408> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=omega_scene_authority_apply n=%u component=%p datum_key=%p datum_identity=0x%08X source=%p source_valid=%u source_nonzero=%u source_hash=0x%llX selector=0x%08X active=0x%08X scene_value=0x%08X nested_mode=0x%08X source_00=0x%08X source_0C=0x%08X source_44=0x%08X source_48=0x%08X source_50=0x%08X source_90=0x%08X source_94=0x%08X source_9C=0x%08X source_D0=0x%08X component_value_before=0x%08X component_value_after=0x%08X component_active_before=%u component_active_after=%u forced=%.*s mutation=observe_only",
        observation,
        static_cast<void*>(component),
        static_cast<const void*>(datumKey),
        datumIdentity,
        static_cast<const void*>(source),
        sourceValid ? 1U : 0U,
        sourceSnapshot.nonzero,
        static_cast<unsigned long long>(sourceSnapshot.hash),
        field(0x04),
        field(0x08),
        field(0x4C),
        field(0x98),
        field(0x00),
        field(0x0C),
        field(0x44),
        field(0x48),
        field(0x50),
        field(0x90),
        field(0x94),
        field(0x9C),
        field(0xD0),
        componentValueBefore,
        componentValueAfter,
        static_cast<unsigned>(componentActiveBefore),
        static_cast<unsigned>(componentActiveAfter),
        static_cast<int>(package.size()),
        package.data());
    if (length > 0) {
        const std::size_t safeLength = static_cast<std::size_t>(length) < line.size()
                                           ? static_cast<std::size_t>(length)
                                           : line.size() - 1U;
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), safeLength});
    }
}

/** Records construction of the live state from the initializer's stable native helper. */
__declspec(noinline) void __fastcall
mission_director_runtime_initialize(std::byte* state) noexcept {
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    const void* const returnAddress = _ReturnAddress();
    const bool missionCall = image != nullptr
                             && returnAddress == image + kMissionDirectorRuntimeInitializeReturnRva;
    MissionRuntimeSnapshot before{};
    MissionRuntimeSnapshot after{};
    const bool beforeValid = missionCall && snapshot_mission_runtime(state, before);
    const MissionDirectorRuntimeInitializeHelper original =
        g_missionDirectorRuntimeInitializeOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(state);
    }
    if (!missionCall) {
        return;
    }
    const bool afterValid = snapshot_mission_runtime(state, after);
    const std::uint32_t observation =
        g_missionDirectorRuntimeInitializeObserved.fetch_add(1, std::memory_order_relaxed) + 1U;
    std::string_view package{};
    (void)opening_forced_destination(package);
    std::array<char, 1024> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=mission_director_runtime_initialize n=%u path=stable_helper runtime=%p state=%p before_valid=%u before_nonzero=%u before_hash=0x%llX after_valid=%u after_nonzero=%u after_hash=0x%llX after=0x%llX,0x%llX,0x%llX,0x%llX,0x%llX,0x%llX,0x%llX,0x%llX forced=%.*s mutation=observe_only",
        observation,
        state != nullptr ? static_cast<void*>(state - 0x180) : nullptr,
        static_cast<void*>(state),
        beforeValid ? 1U : 0U,
        before.nonzero,
        static_cast<unsigned long long>(before.hash),
        afterValid ? 1U : 0U,
        after.nonzero,
        static_cast<unsigned long long>(after.hash),
        static_cast<unsigned long long>(after.words[0]),
        static_cast<unsigned long long>(after.words[1]),
        static_cast<unsigned long long>(after.words[2]),
        static_cast<unsigned long long>(after.words[3]),
        static_cast<unsigned long long>(after.words[4]),
        static_cast<unsigned long long>(after.words[5]),
        static_cast<unsigned long long>(after.words[6]),
        static_cast<unsigned long long>(after.words[7]),
        static_cast<int>(package.size()),
        package.data());
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
}

/** Records the exact decoded state returned to the encrypted mission-director copy callback. */
__declspec(noinline) const std::byte* __fastcall
mission_director_runtime_apply(const std::byte* datumKey, std::uint32_t schema) noexcept {
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    const void* const returnAddress = _ReturnAddress();
    const bool missionCall = image != nullptr
                             && returnAddress == image + kMissionDirectorRuntimeApplyReturnRva;
    const MissionDirectorRuntimeApplyResolver original =
        g_missionDirectorRuntimeApplyOriginal.load(std::memory_order_acquire);
    const std::byte* const source =
        original != nullptr ? original(datumKey, schema) : nullptr;
    std::string_view package{};
    const bool opening = opening_forced_destination(package);
    if (opening && schema == kOmegaSceneAuthoritySchema) {
        const std::uint32_t sceneObservation =
            g_omegaSceneRuntimeResolveObserved.fetch_add(1, std::memory_order_relaxed) + 1U;
        if (sceneObservation <= 64U) {
            MissionRuntimeSnapshot sceneSnapshot{};
            const bool sceneSourceValid = snapshot_mission_runtime(source, sceneSnapshot);
            std::uint32_t sceneDatumIdentity = 0U;
            const std::byte* sceneKeyedSource = nullptr;
            if (datumKey != nullptr) {
                __try {
                    std::memcpy(&sceneDatumIdentity, datumKey, sizeof sceneDatumIdentity);
                    sceneKeyedSource = *reinterpret_cast<const std::byte* const*>(datumKey + 8);
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                    sceneDatumIdentity = 0U;
                    sceneKeyedSource = nullptr;
                }
            }
            const std::uintptr_t returnRva = image != nullptr
                                                 ? reinterpret_cast<std::uintptr_t>(returnAddress)
                                                       - reinterpret_cast<std::uintptr_t>(image)
                                                 : 0U;
            std::array<char, 1024> sceneLine{};
            const int sceneLength = std::snprintf(
                sceneLine.data(),
                sceneLine.size(),
                "ev=bootflow stage=omega_scene_runtime_resolve n=%u return_rva=0x%llX datum_key=%p datum_identity=0x%08X schema=0x%08X keyed_source=%p source=%p source_match=%u source_valid=%u source_nonzero=%u source_hash=0x%llX source_words=0x%llX,0x%llX,0x%llX,0x%llX,0x%llX,0x%llX,0x%llX,0x%llX forced=%.*s mutation=observe_only",
                sceneObservation,
                static_cast<unsigned long long>(returnRva),
                static_cast<const void*>(datumKey),
                sceneDatumIdentity,
                schema,
                static_cast<const void*>(sceneKeyedSource),
                static_cast<const void*>(source),
                sceneKeyedSource == source ? 1U : 0U,
                sceneSourceValid ? 1U : 0U,
                sceneSnapshot.nonzero,
                static_cast<unsigned long long>(sceneSnapshot.hash),
                static_cast<unsigned long long>(sceneSnapshot.words[0]),
                static_cast<unsigned long long>(sceneSnapshot.words[1]),
                static_cast<unsigned long long>(sceneSnapshot.words[2]),
                static_cast<unsigned long long>(sceneSnapshot.words[3]),
                static_cast<unsigned long long>(sceneSnapshot.words[4]),
                static_cast<unsigned long long>(sceneSnapshot.words[5]),
                static_cast<unsigned long long>(sceneSnapshot.words[6]),
                static_cast<unsigned long long>(sceneSnapshot.words[7]),
                static_cast<int>(package.size()),
                package.data());
            if (sceneLength > 0) {
                core::log::write(core::log::Channel::client,
                                 core::log::Level::info,
                                 {sceneLine.data(), static_cast<std::size_t>(sceneLength)});
            }
        }
    }
    if (!missionCall) {
        return source;
    }

    MissionRuntimeSnapshot sourceSnapshot{};
    const bool sourceValid = snapshot_mission_runtime(source, sourceSnapshot);
    std::uint32_t datumIdentity = 0U;
    const std::byte* keyedSource = nullptr;
    if (datumKey != nullptr) {
        __try {
            std::memcpy(&datumIdentity, datumKey, sizeof datumIdentity);
            keyedSource = *reinterpret_cast<const std::byte* const*>(datumKey + 8);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            datumIdentity = 0U;
            keyedSource = nullptr;
        }
    }
    const std::uint32_t observation =
        g_missionDirectorRuntimeApplyObserved.fetch_add(1, std::memory_order_relaxed) + 1U;
    std::array<char, 1024> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=mission_director_runtime_apply n=%u path=stable_resolver datum_key=%p datum_identity=0x%08X schema=0x%08X keyed_source=%p source=%p source_match=%u source_valid=%u source_nonzero=%u source_hash=0x%llX source_words=0x%llX,0x%llX,0x%llX,0x%llX,0x%llX,0x%llX,0x%llX,0x%llX forced=%.*s mutation=observe_only",
        observation,
        static_cast<const void*>(datumKey),
        datumIdentity,
        schema,
        static_cast<const void*>(keyedSource),
        static_cast<const void*>(source),
        keyedSource == source ? 1U : 0U,
        sourceValid ? 1U : 0U,
        sourceSnapshot.nonzero,
        static_cast<unsigned long long>(sourceSnapshot.hash),
        static_cast<unsigned long long>(sourceSnapshot.words[0]),
        static_cast<unsigned long long>(sourceSnapshot.words[1]),
        static_cast<unsigned long long>(sourceSnapshot.words[2]),
        static_cast<unsigned long long>(sourceSnapshot.words[3]),
        static_cast<unsigned long long>(sourceSnapshot.words[4]),
        static_cast<unsigned long long>(sourceSnapshot.words[5]),
        static_cast<unsigned long long>(sourceSnapshot.words[6]),
        static_cast<unsigned long long>(sourceSnapshot.words[7]),
        static_cast<int>(package.size()),
        package.data());
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
    return source;
}

/** Observes whether Homecoming ever creates the component slots that own script events. */
__declspec(noinline) bool __fastcall activity_script_component_register(std::byte* manager,
                                                                         std::int32_t slotIndex,
                                                                         std::int32_t mode,
                                                                         std::int32_t value) noexcept {
    const bool validSlot = manager != nullptr && slotIndex >= 0 && slotIndex < 1024;
    std::byte* const slot = validSlot ? manager + 0xE958 + (slotIndex * 0x38) : nullptr;
    const std::int32_t beforeKind =
        slot != nullptr ? *reinterpret_cast<const std::int32_t*>(slot) : -2;
    const std::int32_t beforeValue =
        slot != nullptr ? *reinterpret_cast<const std::int32_t*>(slot + 4) : -2;
    const ActivityScriptComponentRegister original =
        g_componentRegisterOriginal.load(std::memory_order_acquire);
    const bool result = original != nullptr && original(manager, slotIndex, mode, value);

    std::string_view package{};
    const bool opening = opening_forced_destination(package);
    const std::uint32_t observation =
        g_componentRegisterObserved.fetch_add(1, std::memory_order_relaxed) + 1U;
    if (opening || observation <= 64U) {
        const std::int32_t activity =
            manager != nullptr ? *reinterpret_cast<const std::int32_t*>(manager + 0x850) : -1;
        const std::int32_t variant =
            manager != nullptr ? *reinterpret_cast<const std::int32_t*>(manager + 0x854) : -1;
        const std::int32_t eventMode =
            manager != nullptr ? *reinterpret_cast<const std::int32_t*>(manager + 0x1AEF8) : -1;
        const std::int32_t component =
            manager != nullptr ? *reinterpret_cast<const std::int32_t*>(manager + 0x1AF00) : -1;
        const std::int32_t afterKind =
            slot != nullptr ? *reinterpret_cast<const std::int32_t*>(slot) : -2;
        const std::int32_t afterValue =
            slot != nullptr ? *reinterpret_cast<const std::int32_t*>(slot + 4) : -2;
        const void* const runtime =
            slot != nullptr ? *reinterpret_cast<void* const*>(slot + 8) : nullptr;
        const void* const callback =
            slot != nullptr ? *reinterpret_cast<void* const*>(slot + 0x20) : nullptr;
        std::array<char, 384> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_script_component_probe n=%u result=%s manager=%p slot=%d mode=%d value=%d before_kind=%d before_value=%d after_kind=%d after_value=%d runtime=%p callback=%p activity=%d variant=%d event_mode=%d component=%d forced=%.*s",
            observation,
            result ? "accepted" : "rejected",
            static_cast<void*>(manager),
            slotIndex,
            mode,
            value,
            beforeKind,
            beforeValue,
            afterKind,
            afterValue,
            runtime,
            callback,
            activity,
            variant,
            eventMode,
            component,
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

/** Observes the native event stream without changing the event, payload, manager, or result. */
__declspec(noinline) void __fastcall activity_script_event(std::byte* manager,
                                                            const void* payload,
                                                            std::uint32_t event) noexcept {
    std::string_view package{};
    const bool opening = opening_forced_destination(package);
    const std::uint32_t observation = g_observed.fetch_add(1, std::memory_order_relaxed) + 1U;
    if ((opening && observation <= 512U) || event == 0x1EU || event == 0x1FU) {
        const std::int32_t mode =
            manager != nullptr ? *reinterpret_cast<const std::int32_t*>(manager + 0x1AEF8) : -1;
        const std::int32_t activity =
            manager != nullptr ? *reinterpret_cast<const std::int32_t*>(manager + 0x850) : -1;
        const std::int32_t component =
            manager != nullptr ? *reinterpret_cast<const std::int32_t*>(manager + 0x1AF00) : -1;
        std::array<char, 256> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_script_event_probe n=%u event=0x%X manager=%p payload=%p mode=%d activity=%d component=%d forced=%.*s",
            observation,
            event,
            static_cast<void*>(manager),
            payload,
            mode,
            activity,
            component,
            static_cast<int>(package.size()),
            package.data());
        if (length > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(length)});
        }
    }
    const ActivityScriptEvent original = g_original.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(manager, payload, event);
    }
}

[[nodiscard]] std::array<legacy_owner_sentinel::HookOwnership, 5>
activity_script_event_hook_ownership() noexcept {
    // LEGACY_OWNER_SENTINEL_BEGIN(activity_script_event, 5)
    return {{{g_eventHandle.attached,
              g_original.load(std::memory_order_acquire) != nullptr},
             {g_componentRegisterHandle.attached,
              g_componentRegisterOriginal.load(std::memory_order_acquire) != nullptr},
             {g_missionDirectorRuntimeInitializeHandle.attached,
              g_missionDirectorRuntimeInitializeOriginal.load(std::memory_order_acquire)
                  != nullptr},
             {g_missionDirectorRuntimeApplyHandle.attached,
              g_missionDirectorRuntimeApplyOriginal.load(std::memory_order_acquire) != nullptr},
             {g_omegaSceneAuthorityApplyHandle.attached,
              g_omegaSceneAuthorityApplyOriginal.load(std::memory_order_acquire) != nullptr}}};
    // LEGACY_OWNER_SENTINEL_END(activity_script_event)
}

} // namespace

bool snapshot_omega_scene_authority_observation(
    OmegaSceneAuthorityObservation& observation) noexcept {
    AcquireSRWLockShared(&g_omegaSceneAuthorityObservationLock);
    observation = g_lastOmegaSceneAuthorityObservation;
    ReleaseSRWLockShared(&g_omegaSceneAuthorityObservationLock);
    return observation.sequence != 0U;
}

bool activity_script_event_probe_attached() noexcept {
    return legacy_owner_sentinel::any_handle_attached(
        activity_script_event_hook_ownership());
}

bool activity_script_event_probe_has_ownership() noexcept {
    return legacy_owner_sentinel::has_ownership(
        activity_script_event_hook_ownership());
}

/** Attaches independent script-event and mission-runtime observers. */
bool install_activity_script_event_probe() noexcept {
    LateInstallGuard lateInstall;
    if (!lateInstall.accepted()) {
        return false;
    }
    if (g_eventHandle.attached && g_componentRegisterHandle.attached
        && g_missionDirectorRuntimeInitializeHandle.attached
        && g_missionDirectorRuntimeApplyHandle.attached
        && g_omegaSceneAuthorityApplyHandle.attached) {
        return true;
    }
    std::byte* const registerTarget = component_register_target();
    std::byte* const eventTarget = event_target();
    std::byte* const missionInitializeTarget = mission_director_runtime_initialize_target();
    std::byte* const missionApplyTarget = mission_director_runtime_apply_target();
    std::byte* const sceneApplyTarget = omega_scene_authority_apply_target();
    g_observed.store(0, std::memory_order_release);
    g_componentRegisterObserved.store(0, std::memory_order_release);
    g_missionDirectorRuntimeInitializeObserved.store(0, std::memory_order_release);
    g_missionDirectorRuntimeApplyObserved.store(0, std::memory_order_release);
    g_omegaSceneRuntimeResolveObserved.store(0, std::memory_order_release);
    g_omegaSceneAuthorityApplyObserved.store(0, std::memory_order_release);
    AcquireSRWLockExclusive(&g_omegaSceneAuthorityObservationLock);
    g_lastOmegaSceneAuthorityObservation = {};
    ReleaseSRWLockExclusive(&g_omegaSceneAuthorityObservationLock);

    bool scriptAttached = g_eventHandle.attached && g_componentRegisterHandle.attached;
    if (!scriptAttached && (registerTarget == nullptr || eventTarget == nullptr)) {
        std::array<char, 192> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_script_event_probe result=fail reason=target register_target=%u event_target=%u",
            registerTarget != nullptr ? 1U : 0U,
            eventTarget != nullptr ? 1U : 0U);
        if (length > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::warn,
                             {line.data(), static_cast<std::size_t>(length)});
        }
    } else if (!scriptAttached) {
        const hooking::detour::Spec registerSpec{
            registerTarget, reinterpret_cast<void*>(&activity_script_component_register)};
        if (!hooking::detour::install(registerSpec, g_componentRegisterHandle)) {
            core::log::write(
                core::log::Channel::client,
                core::log::Level::warn,
                "ev=bootflow stage=activity_script_component_probe result=fail reason=attach");
        } else {
            g_componentRegisterOriginal.store(
                reinterpret_cast<ActivityScriptComponentRegister>(
                    g_componentRegisterHandle.original),
                std::memory_order_release);
            const hooking::detour::Spec eventSpec{
                eventTarget, reinterpret_cast<void*>(&activity_script_event)};
            if (!hooking::detour::install(eventSpec, g_eventHandle)) {
                (void)hooking::detour::uninstall(g_componentRegisterHandle);
                g_componentRegisterOriginal.store(nullptr, std::memory_order_release);
                core::log::write(
                    core::log::Channel::client,
                    core::log::Level::warn,
                    "ev=bootflow stage=activity_script_event_probe result=fail reason=attach");
            } else {
                g_original.store(reinterpret_cast<ActivityScriptEvent>(g_eventHandle.original),
                                 std::memory_order_release);
            }
        }
        scriptAttached = g_eventHandle.attached && g_componentRegisterHandle.attached;
    }
    if (scriptAttached) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         "ev=bootflow stage=activity_script_component_probe result=ok mode=observe");
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         "ev=bootflow stage=activity_script_event_probe result=ok mode=observe");
    }

    bool missionAttached = g_missionDirectorRuntimeInitializeHandle.attached
                           && g_missionDirectorRuntimeApplyHandle.attached;
    if (!missionAttached && (missionInitializeTarget == nullptr || missionApplyTarget == nullptr)) {
        std::array<char, 208> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=mission_director_runtime_probe result=fail reason=target initialize_helper=%u apply_resolver=%u",
            missionInitializeTarget != nullptr ? 1U : 0U,
            missionApplyTarget != nullptr ? 1U : 0U);
        if (length > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::warn,
                             {line.data(), static_cast<std::size_t>(length)});
        }
    } else if (!missionAttached) {
        const hooking::detour::Spec missionInitializeSpec{
            missionInitializeTarget,
            reinterpret_cast<void*>(&mission_director_runtime_initialize)};
        if (!hooking::detour::install(missionInitializeSpec,
                                      g_missionDirectorRuntimeInitializeHandle)) {
            core::log::write(
                core::log::Channel::client,
                core::log::Level::warn,
                "ev=bootflow stage=mission_director_runtime_probe result=fail reason=initialize_helper_attach");
        } else {
            g_missionDirectorRuntimeInitializeOriginal.store(
                reinterpret_cast<MissionDirectorRuntimeInitializeHelper>(
                    g_missionDirectorRuntimeInitializeHandle.original),
                std::memory_order_release);
            const hooking::detour::Spec missionApplySpec{
                missionApplyTarget, reinterpret_cast<void*>(&mission_director_runtime_apply)};
            if (!hooking::detour::install(missionApplySpec,
                                          g_missionDirectorRuntimeApplyHandle)) {
                (void)hooking::detour::uninstall(g_missionDirectorRuntimeInitializeHandle);
                g_missionDirectorRuntimeInitializeOriginal.store(nullptr,
                                                                  std::memory_order_release);
                core::log::write(
                    core::log::Channel::client,
                    core::log::Level::warn,
                    "ev=bootflow stage=mission_director_runtime_probe result=fail reason=apply_resolver_attach");
            } else {
                g_missionDirectorRuntimeApplyOriginal.store(
                    reinterpret_cast<MissionDirectorRuntimeApplyResolver>(
                        g_missionDirectorRuntimeApplyHandle.original),
                    std::memory_order_release);
            }
        }
        missionAttached = g_missionDirectorRuntimeInitializeHandle.attached
                          && g_missionDirectorRuntimeApplyHandle.attached;
    }
    if (missionAttached) {
        core::log::write(
            core::log::Channel::client,
            core::log::Level::info,
            "ev=bootflow stage=mission_director_runtime_probe result=ok mode=observe_via_stable_helpers state_bytes=64");
    }

    bool sceneAttached = g_omegaSceneAuthorityApplyHandle.attached;
    if (!sceneAttached && sceneApplyTarget == nullptr) {
        core::log::write(
            core::log::Channel::client,
            core::log::Level::warn,
            "ev=bootflow stage=omega_scene_authority_probe result=fail reason=target");
    } else if (!sceneAttached) {
        const hooking::detour::Spec sceneApplySpec{
            sceneApplyTarget, reinterpret_cast<void*>(&omega_scene_authority_apply)};
        if (!hooking::detour::install(sceneApplySpec, g_omegaSceneAuthorityApplyHandle)) {
            core::log::write(
                core::log::Channel::client,
                core::log::Level::warn,
                "ev=bootflow stage=omega_scene_authority_probe result=fail reason=attach");
        } else {
            g_omegaSceneAuthorityApplyOriginal.store(
                reinterpret_cast<OmegaSceneAuthorityApply>(
                    g_omegaSceneAuthorityApplyHandle.original),
                std::memory_order_release);
            sceneAttached = true;
        }
    }
    if (sceneAttached) {
        core::log::write(
            core::log::Channel::client,
            core::log::Level::info,
            "ev=bootflow stage=omega_scene_authority_probe result=ok mode=observe state_bytes=212 component_fields=25C,260");
    }
    return scriptAttached && missionAttached && sceneAttached;
}

/** Detaches the observer and clears its trampoline. */
void uninstall_activity_script_event_probe() noexcept {
    if (g_omegaSceneAuthorityApplyHandle.attached) {
        (void)hooking::detour::uninstall(g_omegaSceneAuthorityApplyHandle);
    }
    if (g_missionDirectorRuntimeApplyHandle.attached) {
        (void)hooking::detour::uninstall(g_missionDirectorRuntimeApplyHandle);
    }
    if (g_missionDirectorRuntimeInitializeHandle.attached) {
        (void)hooking::detour::uninstall(g_missionDirectorRuntimeInitializeHandle);
    }
    if (g_eventHandle.attached) {
        (void)hooking::detour::uninstall(g_eventHandle);
    }
    if (g_componentRegisterHandle.attached) {
        (void)hooking::detour::uninstall(g_componentRegisterHandle);
    }
    g_original.store(nullptr, std::memory_order_release);
    g_componentRegisterOriginal.store(nullptr, std::memory_order_release);
    g_missionDirectorRuntimeInitializeOriginal.store(nullptr, std::memory_order_release);
    g_missionDirectorRuntimeApplyOriginal.store(nullptr, std::memory_order_release);
    g_omegaSceneAuthorityApplyOriginal.store(nullptr, std::memory_order_release);
    g_observed.store(0, std::memory_order_release);
    g_componentRegisterObserved.store(0, std::memory_order_release);
    g_missionDirectorRuntimeInitializeObserved.store(0, std::memory_order_release);
    g_missionDirectorRuntimeApplyObserved.store(0, std::memory_order_release);
    g_omegaSceneRuntimeResolveObserved.store(0, std::memory_order_release);
    g_omegaSceneAuthorityApplyObserved.store(0, std::memory_order_release);
    AcquireSRWLockExclusive(&g_omegaSceneAuthorityObservationLock);
    g_lastOmegaSceneAuthorityObservation = {};
    ReleaseSRWLockExclusive(&g_omegaSceneAuthorityObservationLock);
}

} // namespace sunrise::client::hooks::bootflow
