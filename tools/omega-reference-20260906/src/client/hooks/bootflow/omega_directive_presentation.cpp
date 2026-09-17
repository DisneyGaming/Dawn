#include <Windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string_view>

#include "../../../core/logging/log.h"
#include "../../../core/settings/settings.h"
#include "../../../state/activity/forced/activity_forced_destination.h"
#include "../../../state/activity/runtime.h"
#include "../../hooking/call_gate.h"
#include "../../hooking/detour.h"
#include "bootflow_hook_lifecycle.h"
#include "internal.h"

namespace dawn::client::hooks::bootflow {
namespace {

/** m_directive_sensor's class-specific initialization callback in the pinned client. */
constexpr std::uintptr_t kDirectiveComponentInitializeRva = 0x1009630U;
constexpr std::array<std::byte, 16> kDirectiveComponentInitializePrefix{
    std::byte{0x40}, std::byte{0x53}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x30}, std::byte{0x44}, std::byte{0x8B},
    std::byte{0x02}, std::byte{0x48}, std::byte{0x8B}, std::byte{0xD9},
    std::byte{0x4C}, std::byte{0x8B}, std::byte{0x4A}, std::byte{0x08}};

/** Resolves the directive list authored on one m_directive_sensor component. */
constexpr std::uintptr_t kDirectiveContentResolveRva = 0x1009430U;
constexpr std::array<std::byte, 16> kDirectiveContentResolvePrefix{
    std::byte{0x44}, std::byte{0x8B}, std::byte{0x09}, std::byte{0x4C},
    std::byte{0x8B}, std::byte{0xD1}, std::byte{0x48}, std::byte{0x8B},
    std::byte{0x05}, std::byte{0x33}, std::byte{0x08}, std::byte{0x43},
    std::byte{0x01}, std::byte{0x41}, std::byte{0x8B}, std::byte{0xD1}};

/** Looks up an authored directive record by its event/output key. */
constexpr std::uintptr_t kDirectiveContentLookupRva = 0x100A110U;
constexpr std::array<std::byte, 16> kDirectiveContentLookupPrefix{
    std::byte{0x48}, std::byte{0x83}, std::byte{0xEC}, std::byte{0x08},
    std::byte{0x4C}, std::byte{0x8B}, std::byte{0x51}, std::byte{0x08},
    std::byte{0x45}, std::byte{0x33}, std::byte{0xC9}, std::byte{0x4D},
    std::byte{0x85}, std::byte{0xD2}, std::byte{0x7E}, std::byte{0x53}};

/** Native inactive-to-present edge: builds and publishes one directive HUD entry. */
constexpr std::uintptr_t kDirectiveInstallRva = 0x1009ED0U;
constexpr std::array<std::byte, 16> kDirectiveInstallPrefix{
    std::byte{0x40}, std::byte{0x55}, std::byte{0x53}, std::byte{0x57},
    std::byte{0x48}, std::byte{0x8D}, std::byte{0x6C}, std::byte{0x24},
    std::byte{0xA0}, std::byte{0x48}, std::byte{0x81}, std::byte{0xEC},
    std::byte{0x60}, std::byte{0x01}, std::byte{0x00}, std::byte{0x00}};

/** HUD directive-manager availability predicate and object accessor. */
constexpr std::uintptr_t kDirectiveManagerReadyRva = 0x137E1D0U;
constexpr std::array<std::byte, 16> kDirectiveManagerReadyPrefix{
    std::byte{0x80}, std::byte{0x3D}, std::byte{0xF1}, std::byte{0x9A},
    std::byte{0xC3}, std::byte{0x01}, std::byte{0x00}, std::byte{0x74},
    std::byte{0x12}, std::byte{0x48}, std::byte{0x8D}, std::byte{0x05},
    std::byte{0x5F}, std::byte{0x86}, std::byte{0xC3}, std::byte{0x01}};
constexpr std::uintptr_t kDirectiveManagerGetRva = 0x137D6F0U;
constexpr std::array<std::byte, 12> kDirectiveManagerGetPrefix{
    std::byte{0x48}, std::byte{0x8D}, std::byte{0x05}, std::byte{0x48},
    std::byte{0x91}, std::byte{0xC3}, std::byte{0x01}, std::byte{0x48},
    std::byte{0x83}, std::byte{0xE0}, std::byte{0xF8}, std::byte{0xC3}};

/** Omega's first authored type-69 directive output. */
constexpr std::uint32_t kOmegaOpeningDirective = 0xC252E306U;
/** Canonical absent activity client-reference key. */
constexpr std::uint32_t kAbsentReference = 0x811C9DC5U;
/** One decoded 0x80804F6B directive record. */
constexpr std::size_t kDirectiveRecordBytes = 0xF8U;
constexpr std::size_t kDirectiveOptionalReferenceOffset = 0x38U;
constexpr std::size_t kDirectiveClientReferenceOffset = 0x5CU;
/** The HUD manager owns no more than sixteen 0x148-byte directive entries. */
constexpr std::size_t kDirectiveManagerEntryCountOffset = 0x1480U;
constexpr std::size_t kDirectiveManagerEntryStride = 0x148U;
constexpr std::size_t kDirectiveManagerMaximumEntries = 16U;
constexpr std::uint8_t kDirectiveManagerEntryKind = 2U;
/** Retry only during the opening's short post-arrival presentation window. */
constexpr std::uint32_t kMaximumAttempts = 32U;
constexpr std::uint64_t kRetryIntervalMs = 125U;
constexpr std::uint64_t kPresentationWindowMs = 10'000U;

[[nodiscard]] constexpr std::uint32_t directive_hash_step(std::uint32_t value,
                                                          std::uint8_t byte) noexcept {
    return value * 0x01000193U ^ byte;
}

[[nodiscard]] constexpr std::uint32_t opening_directive_hash() noexcept {
    std::uint32_t value = kOmegaOpeningDirective;
    for (std::size_t index = 0; index < sizeof(std::uint32_t); ++index) {
        value = directive_hash_step(value, 0U);
    }
    return value;
}

/** Native HUD identity generated from C252E306 plus its zero variant. */
constexpr std::uint32_t kOmegaOpeningDirectiveHash = opening_directive_hash();
static_assert(kOmegaOpeningDirectiveHash == 0x32678D66U);

using DirectiveComponentInitialize = std::uint64_t(__fastcall*)(std::uint32_t* component,
                                                                 const void* stateKey) noexcept;
using DirectiveContentResolve = void*(__fastcall*)(std::uint32_t* component) noexcept;
using DirectiveContentLookup = void*(__fastcall*)(void* content,
                                                   const std::uint32_t* key) noexcept;
using DirectiveInstall = void(__fastcall*)(std::uint32_t* component,
                                            std::byte* record,
                                            void* content) noexcept;
using DirectiveManagerReady = void*(__fastcall*)() noexcept;
using DirectiveManagerGet = std::byte*(__fastcall*)() noexcept;

hooking::detour::Handle g_componentInitializeHandle{};
std::atomic_bool g_installed{};
std::atomic<DirectiveComponentInitialize> g_componentInitializeOriginal{};
std::atomic<DirectiveContentResolve> g_contentResolve{};
std::atomic<DirectiveContentLookup> g_contentLookup{};
std::atomic<DirectiveInstall> g_directiveInstall{};
std::atomic<DirectiveManagerReady> g_managerReady{};
std::atomic<DirectiveManagerGet> g_managerGet{};
hooking::CallGate g_callGate{};
std::atomic<std::uintptr_t> g_component{};
std::atomic_bool g_arrived{};
std::atomic_bool g_idleReset{};
std::atomic_bool g_confirmed{};

[[nodiscard]] bool calls_idle() noexcept {
    return g_callGate.idle();
}
std::atomic_bool g_exhaustedReported{};
std::atomic_uint32_t g_attempts{};
std::atomic_uint64_t g_arrivalTick{};
std::atomic_uint64_t g_lastAttemptTick{};

template <std::size_t N>
[[nodiscard]] bool prefix_matches(const std::byte* target,
                                  const std::array<std::byte, N>& prefix) noexcept {
    return target != nullptr && std::equal(prefix.begin(), prefix.end(), target);
}

[[nodiscard]] bool omega_is_forced() noexcept {
    state::activity::forced::ForcedDestination forced{};
    state::activity::forced::snapshot(forced);
    const std::size_t length =
        (std::min)(static_cast<std::size_t>(forced.packageNameLength), forced.packageName.size());
    return state::activity::forced::override_active()
           && std::string_view(forced.packageName.data(), length) == "mission_scot";
}

void reset_attempt_state(bool clearComponent) noexcept {
    g_arrived.store(false, std::memory_order_release);
    g_confirmed.store(false, std::memory_order_release);
    g_exhaustedReported.store(false, std::memory_order_release);
    g_attempts.store(0U, std::memory_order_release);
    g_arrivalTick.store(0U, std::memory_order_release);
    g_lastAttemptTick.store(0U, std::memory_order_release);
    if (clearComponent) {
        g_component.store(0U, std::memory_order_release);
    }
}

[[nodiscard]] bool manager_contains_opening_directive(std::byte* manager) noexcept {
    if (manager == nullptr) {
        return false;
    }
    bool found = false;
    __try {
        const std::int64_t count = *reinterpret_cast<const std::int64_t*>(
            manager + kDirectiveManagerEntryCountOffset);
        if (count < 0
            || static_cast<std::uint64_t>(count) > kDirectiveManagerMaximumEntries) {
            return false;
        }
        const auto aligned = (reinterpret_cast<std::uintptr_t>(manager) + 7U) & ~std::uintptr_t{7U};
        const auto* const entries = reinterpret_cast<const std::byte*>(aligned);
        for (std::int64_t index = 0; index < count; ++index) {
            const std::byte* const entry =
                entries + static_cast<std::size_t>(index) * kDirectiveManagerEntryStride;
            const std::uint8_t kind = std::to_integer<std::uint8_t>(entry[0]);
            std::uint32_t hash = 0U;
            std::memcpy(&hash, entry + 4U, sizeof hash);
            if (kind == kDirectiveManagerEntryKind && hash == kOmegaOpeningDirectiveHash) {
                found = true;
                break;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        found = false;
    }
    return found;
}

[[nodiscard]] std::array<std::byte, kDirectiveRecordBytes> opening_record() noexcept {
    std::array<std::byte, kDirectiveRecordBytes> record{};
    const std::int32_t variant = 0;
    const std::int64_t noOptionalReference = -1;
    const std::int8_t absentType = -1;
    const std::int16_t absentIndex = -1;
    std::memcpy(record.data(), &kOmegaOpeningDirective, sizeof kOmegaOpeningDirective);
    std::memcpy(record.data() + 4U, &variant, sizeof variant);
    // Byte +8 is already zero. That exact state is the native install/new-objective edge.
    std::memcpy(record.data() + kDirectiveOptionalReferenceOffset,
                &noOptionalReference,
                sizeof noOptionalReference);
    std::memcpy(record.data() + kDirectiveClientReferenceOffset,
                &kAbsentReference,
                sizeof kAbsentReference);
    std::memcpy(record.data() + kDirectiveClientReferenceOffset + 4U,
                &absentType,
                sizeof absentType);
    std::memcpy(record.data() + kDirectiveClientReferenceOffset + 6U,
                &absentIndex,
                sizeof absentIndex);
    return record;
}

void report_attempt(std::uint32_t attempt,
                    const char* result,
                    std::uintptr_t component,
                    void* content,
                    std::byte* manager) noexcept {
    std::array<char, 384> line{};
    const int written = std::snprintf(
        line.data(),
        line.size(),
        "ev=activity stage=omega_directive_presentation attempt=%u result=%s "
        "event=0x%08X hud_hash=0x%08X component=%p content=%p manager=%p mutation=local_ui",
        attempt,
        result,
        kOmegaOpeningDirective,
        kOmegaOpeningDirectiveHash,
        reinterpret_cast<void*>(component),
        content,
        manager);
    if (written > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(),
                          static_cast<std::size_t>(written) < line.size()
                              ? static_cast<std::size_t>(written)
                              : line.size() - 1U});
    }
}

[[nodiscard]] bool fail_install(const char* reason) noexcept {
    std::array<char, 192> line{};
    const int written = std::snprintf(line.data(),
                                      line.size(),
                                      "ev=activity stage=omega_directive_presentation_install "
                                      "result=fail reason=%s",
                                      reason);
    if (written > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::warn,
                         {line.data(), static_cast<std::size_t>(written)});
    }
    return false;
}

std::uint64_t directive_component_initialize_body(
    std::uint32_t* component,
    const void* stateKey,
    const hooking::CallGate::Scope& call) noexcept {
    const DirectiveComponentInitialize original =
        hooking::await_original(g_componentInitializeOriginal);
    const std::uint64_t result = original(component, stateKey);
    if (!call.accepts_side_effects() || component == nullptr || !omega_is_forced()) {
        return result;
    }

    // This callback is class-specific but not tag-specific. Prove that the initialized component's
    // authored list actually owns C252E306 before retaining its runtime pointer. 80F47BD6 is the
    // package handle; *component is a process-local datum and therefore cannot be compared to it.
    const DirectiveContentResolve contentResolve = g_contentResolve.load(std::memory_order_acquire);
    const DirectiveContentLookup contentLookup = g_contentLookup.load(std::memory_order_acquire);
    void* content = nullptr;
    bool ownsOpeningDirective = false;
    if (contentResolve != nullptr && contentLookup != nullptr) {
        __try {
            content = contentResolve(component);
            std::uint32_t key = kOmegaOpeningDirective;
            ownsOpeningDirective = content != nullptr && contentLookup(content, &key) != nullptr;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            content = nullptr;
            ownsOpeningDirective = false;
        }
    }
    if (!ownsOpeningDirective || !call.accepts_side_effects()) {
        return result;
    }

    const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(component);
    if (!call.accepts_side_effects()) {
        return result;
    }
    const std::uintptr_t previous = g_component.exchange(address, std::memory_order_acq_rel);
    if (previous != address && call.accepts_side_effects()) {
        std::array<char, 256> line{};
        std::uint32_t datum = 0xFFFFFFFFU;
        __try {
            datum = *component;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            datum = 0xFFFFFFFFU;
        }
        const int written = std::snprintf(
            line.data(),
            line.size(),
            "ev=activity stage=omega_directive_component result=captured component=%p "
            "runtime_datum=0x%08X content=%p event=0x%08X previous=%p mutation=observe_only",
            component,
            datum,
            content,
            kOmegaOpeningDirective,
            reinterpret_cast<void*>(previous));
        if (written > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(written)});
        }
    }
    return result;
}

__declspec(noinline) std::uint64_t __fastcall directive_component_initialize(
    std::uint32_t* component,
    const void* stateKey) noexcept {
    hooking::CallGate::Scope call(g_callGate);
    return directive_component_initialize_body(component, stateKey, call);
}

} // namespace

bool install_omega_directive_presentation() noexcept {
    if (g_installed.load(std::memory_order_acquire)) {
        return g_callGate.accepting();
    }
    g_callGate.quiesce();
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    if (image == nullptr) {
        return fail_install("image");
    }

    std::byte* const initializeTarget = image + kDirectiveComponentInitializeRva;
    std::byte* const contentResolveTarget = image + kDirectiveContentResolveRva;
    std::byte* const contentLookupTarget = image + kDirectiveContentLookupRva;
    std::byte* const installTarget = image + kDirectiveInstallRva;
    std::byte* const managerReadyTarget = image + kDirectiveManagerReadyRva;
    std::byte* const managerGetTarget = image + kDirectiveManagerGetRva;
    if (!prefix_matches(initializeTarget, kDirectiveComponentInitializePrefix)) {
        return fail_install("component_prefix");
    }
    if (!prefix_matches(contentResolveTarget, kDirectiveContentResolvePrefix)) {
        return fail_install("content_resolve_prefix");
    }
    if (!prefix_matches(contentLookupTarget, kDirectiveContentLookupPrefix)) {
        return fail_install("content_lookup_prefix");
    }
    if (!prefix_matches(installTarget, kDirectiveInstallPrefix)) {
        return fail_install("directive_install_prefix");
    }
    if (!prefix_matches(managerReadyTarget, kDirectiveManagerReadyPrefix)) {
        return fail_install("manager_ready_prefix");
    }
    if (!prefix_matches(managerGetTarget, kDirectiveManagerGetPrefix)) {
        return fail_install("manager_get_prefix");
    }

    // Publish read-only helpers before the class hook can observe an initialization callback. The
    // hook uses them to reject every directive component whose authored list lacks C252E306.
    g_contentResolve.store(reinterpret_cast<DirectiveContentResolve>(contentResolveTarget),
                           std::memory_order_release);
    g_contentLookup.store(reinterpret_cast<DirectiveContentLookup>(contentLookupTarget),
                          std::memory_order_release);
    g_directiveInstall.store(reinterpret_cast<DirectiveInstall>(installTarget),
                             std::memory_order_release);
    g_managerReady.store(reinterpret_cast<DirectiveManagerReady>(managerReadyTarget),
                         std::memory_order_release);
    g_managerGet.store(reinterpret_cast<DirectiveManagerGet>(managerGetTarget),
                       std::memory_order_release);
    const hooking::detour::Spec spec{initializeTarget,
                                     reinterpret_cast<void*>(&directive_component_initialize)};
    if (!hooking::detour::install(spec, g_componentInitializeHandle)) {
        g_contentResolve.store(nullptr, std::memory_order_release);
        g_contentLookup.store(nullptr, std::memory_order_release);
        g_directiveInstall.store(nullptr, std::memory_order_release);
        g_managerReady.store(nullptr, std::memory_order_release);
        g_managerGet.store(nullptr, std::memory_order_release);
        return fail_install("attach");
    }
    hooking::publish_original(
        g_componentInitializeOriginal,
        reinterpret_cast<DirectiveComponentInitialize>(g_componentInitializeHandle.original));
    reset_attempt_state(true);
    g_idleReset.store(false, std::memory_order_release);
    g_installed.store(true, std::memory_order_release);
    g_callGate.accept();
    core::log::write(
        core::log::Channel::client,
        core::log::Level::info,
        "ev=activity stage=omega_directive_presentation_install result=ok "
        "component=+1009630 content=+1009430 lookup=+100A110 install=+1009ED0 "
        "manager=+137D6F0 event=0xC252E306 timing=post_arrival mutation=local_ui");
    return true;
}

namespace {

void sample_omega_directive_presentation_body(
    const hooking::CallGate::Scope& call) noexcept {
    if (!call.accepts_side_effects() || !core::settings::get().omegaExperiments.directiveUi
        || !g_installed.load(std::memory_order_acquire)) {
        return;
    }
    const state::activity::WorldPhase phase = state::activity::world_phase();
    if (!omega_is_forced()) {
        if (!call.accepts_side_effects()) {
            return;
        }
        g_idleReset.store(false, std::memory_order_release);
        reset_attempt_state(true);
        return;
    }
    if (phase == state::activity::WorldPhase::idle) {
        // Idle is the activity-instance boundary. Clearing here prevents a process-local component
        // pointer from surviving a mission reload. Reset only on the edge: a component constructed
        // late in the idle-to-load handoff must not be erased again on the next camera frame.
        if (!call.accepts_side_effects()) {
            return;
        }
        if (!g_idleReset.exchange(true, std::memory_order_acq_rel)) {
            reset_attempt_state(true);
        }
        return;
    }
    if (!call.accepts_side_effects()) {
        return;
    }
    g_idleReset.store(false, std::memory_order_release);

    const bool eligible = in_world()
                          && phase == state::activity::WorldPhase::arrived
                          && state::activity::mission_seed_armed();
    if (!eligible) {
        // Keep the one-shot latch across later slice transitions. mission_seed_armed is retained
        // there, and replaying C252E306 on every re-arrival would resurrect the opening objective.
        return;
    }
    if (!call.accepts_side_effects()) {
        return;
    }
    if (!g_arrived.exchange(true, std::memory_order_acq_rel)) {
        g_confirmed.store(false, std::memory_order_release);
        g_exhaustedReported.store(false, std::memory_order_release);
        g_attempts.store(0U, std::memory_order_release);
        g_arrivalTick.store(GetTickCount64(), std::memory_order_release);
        g_lastAttemptTick.store(0U, std::memory_order_release);
    }
    if (g_confirmed.load(std::memory_order_acquire)) {
        return;
    }

    const std::uint64_t now = GetTickCount64();
    const std::uint64_t arrivedAt = g_arrivalTick.load(std::memory_order_acquire);
    const std::uint32_t attempts = g_attempts.load(std::memory_order_acquire);
    if (attempts >= kMaximumAttempts
        || (arrivedAt != 0U && now - arrivedAt >= kPresentationWindowMs)) {
        if (!call.accepts_side_effects()) {
            return;
        }
        if (!g_exhaustedReported.exchange(true, std::memory_order_acq_rel)) {
            report_attempt(attempts,
                           "exhausted",
                           g_component.load(std::memory_order_acquire),
                           nullptr,
                           nullptr);
        }
        return;
    }
    const std::uint64_t lastAttempt = g_lastAttemptTick.load(std::memory_order_acquire);
    if (lastAttempt != 0U && now - lastAttempt < kRetryIntervalMs) {
        return;
    }
    if (!call.accepts_side_effects()) {
        return;
    }
    g_lastAttemptTick.store(now, std::memory_order_release);

    const std::uintptr_t componentAddress = g_component.load(std::memory_order_acquire);
    const DirectiveContentResolve contentResolve = g_contentResolve.load(std::memory_order_acquire);
    const DirectiveContentLookup contentLookup = g_contentLookup.load(std::memory_order_acquire);
    const DirectiveInstall directiveInstall = g_directiveInstall.load(std::memory_order_acquire);
    const DirectiveManagerReady managerReady = g_managerReady.load(std::memory_order_acquire);
    const DirectiveManagerGet managerGet = g_managerGet.load(std::memory_order_acquire);
    if (componentAddress == 0U || contentResolve == nullptr || contentLookup == nullptr
        || directiveInstall == nullptr || managerReady == nullptr || managerGet == nullptr) {
        return;
    }

    std::byte* manager = nullptr;
    bool managerAvailable = false;
    __try {
        managerAvailable = managerReady() != nullptr;
        if (managerAvailable) {
            manager = managerGet();
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        managerAvailable = false;
        manager = nullptr;
    }
    if (!managerAvailable || manager == nullptr) {
        return;
    }
    if (!call.accepts_side_effects()) {
        return;
    }
    if (manager_contains_opening_directive(manager)) {
        if (!call.accepts_side_effects()) {
            return;
        }
        g_confirmed.store(true, std::memory_order_release);
        report_attempt(attempts, "confirmed_existing", componentAddress, nullptr, manager);
        return;
    }

    if (!call.accepts_side_effects()) {
        return;
    }
    const std::uint32_t attempt = g_attempts.fetch_add(1U, std::memory_order_acq_rel) + 1U;
    auto record = opening_record();
    void* content = nullptr;
    bool invoked = false;
    __try {
        auto* const component = reinterpret_cast<std::uint32_t*>(componentAddress);
        content = contentResolve(component);
        std::uint32_t key = kOmegaOpeningDirective;
        if (call.accepts_side_effects() && content != nullptr
            && contentLookup(content, &key) != nullptr
            && call.accepts_side_effects()) {
            directiveInstall(component, record.data(), content);
            invoked = true;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        content = nullptr;
        invoked = false;
    }

    if (!call.accepts_side_effects()) {
        return;
    }
    if (invoked && manager_contains_opening_directive(manager)) {
        if (!call.accepts_side_effects()) {
            return;
        }
        g_confirmed.store(true, std::memory_order_release);
        report_attempt(attempt, "confirmed_inserted", componentAddress, content, manager);
        return;
    }
    if (call.accepts_side_effects() && (attempt == 1U || attempt % 8U == 0U)) {
        report_attempt(attempt,
                       invoked ? "not_confirmed" : "prerequisite_missing",
                       componentAddress,
                       content,
                       manager);
    }
}

} // namespace

__declspec(noinline) void sample_omega_directive_presentation() noexcept {
    hooking::CallGate::Scope call(g_callGate);
    sample_omega_directive_presentation_body(call);
}

void quiesce_omega_directive_presentation() noexcept {
    g_callGate.quiesce();
}

bool uninstall_omega_directive_presentation() noexcept {
    quiesce_omega_directive_presentation();
    if (!g_installed.load(std::memory_order_acquire)) {
        return true;
    }

    const std::array protectedEntries{
        hooking::detour::ProtectedCodeEntry{
            reinterpret_cast<void*>(&directive_component_initialize)},
        hooking::detour::ProtectedCodeEntry{
            reinterpret_cast<void*>(&sample_omega_directive_presentation)},
        hooking::detour::ProtectedCodeEntry{
            reinterpret_cast<void*>(&hooking::call_gate_detail::enter)},
        hooking::detour::ProtectedCodeEntry{
            reinterpret_cast<void*>(&hooking::call_gate_detail::leave)},
    };
    const hooking::detour::UninstallResult result = hooking::detour::uninstall(
        g_componentInitializeHandle, protectedEntries, &calls_idle);
    if (result != hooking::detour::UninstallResult::removed) {
        core::log::write(
            core::log::Channel::client,
            result == hooking::detour::UninstallResult::failed ? core::log::Level::error
                                                               : core::log::Level::warn,
            result == hooking::detour::UninstallResult::failed
                ? "ev=activity stage=omega_directive_presentation_uninstall result=failed retained=1"
                : "ev=activity stage=omega_directive_presentation_uninstall result=deferred retained=1");
        return false;
    }

    g_componentInitializeOriginal.store(nullptr, std::memory_order_release);
    g_contentResolve.store(nullptr, std::memory_order_release);
    g_contentLookup.store(nullptr, std::memory_order_release);
    g_directiveInstall.store(nullptr, std::memory_order_release);
    g_managerReady.store(nullptr, std::memory_order_release);
    g_managerGet.store(nullptr, std::memory_order_release);
    reset_attempt_state(true);
    g_idleReset.store(false, std::memory_order_release);
    g_installed.store(false, std::memory_order_release);
    core::log::write(
        core::log::Channel::client,
        core::log::Level::info,
        "ev=activity stage=omega_directive_presentation_uninstall result=ok retained=0");
    return true;
}

} // namespace dawn::client::hooks::bootflow
