#include <Windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <intrin.h>
#include <string_view>

#include "../../../core/logging/log.h"
#include "../../../core/settings/settings.h"
#include "../../../state/activity/forced/activity_forced_destination.h"
#include "../../../state/activity/runtime.h"
#include "../../hooking/call_gate.h"
#include "../../hooking/detour.h"
#include "bootflow_hook_lifecycle.h"
#include "internal.h"
#include "omega_directive_native.h"

namespace sunrise::client::hooks::bootflow {
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
/** Towerfall's measured 0x80804F72 event-key table. */
constexpr std::array<std::uint32_t, 16> kTowerfallDirectiveKeys{
    0x4FCECAB6U, 0x432D2C95U, 0x432D2C96U, 0xB85090A0U,
    0xE5ED7DB0U, 0x41222C61U, 0xBB13D094U, 0x23716DE6U,
    0xD56AE99DU, 0xE68735EDU, 0x5DC9D705U, 0xF0D48F30U,
    0xD15B9A42U, 0xDA1CA185U, 0x57395492U, 0xE84A26AAU};
/** Non-absent typed hashes carried by the 17 measured 0x80804F76 values. */
constexpr std::array<std::uint32_t, 28> kTowerfallValueHashes{
    0x2B9907B5U, 0x02BE2729U, 0xA7223110U, 0x8F01E408U,
    0x7E59519BU, 0x2DB372FBU, 0xD4175BADU, 0x6E47E341U,
    0x78F7670CU, 0xD7BE59FCU, 0x197E80A1U, 0x49097A8DU,
    0xD65E8254U, 0x0B25C264U, 0xAB9A7014U, 0x123C97F4U,
    0xF75227A4U, 0xABC99255U, 0x4AFB40C9U, 0x44FBFF68U,
    0x7F082155U, 0x99CD57C9U, 0xC1739F32U, 0x9C5D1EAEU,
    0xC018E549U, 0xBBEE8CD5U, 0x5DC0DEE3U, 0xB10B97C3U};
/** Canonical absent activity client-reference key. */
constexpr std::uint32_t kAbsentReference = 0x811C9DC5U;
/** One decoded 0x80804F6B directive record. */
constexpr std::size_t kDirectiveRecordBytes = 0xF8U;
constexpr std::size_t kDirectiveLookupSnapshotBytes = 0x40U;
constexpr std::size_t kDirectiveOptionalReferenceOffset = 0x38U;
constexpr std::size_t kDirectiveClientReferenceOffset = 0x5CU;
/** The HUD manager owns no more than sixteen 0x148-byte directive entries. */
constexpr std::size_t kDirectiveManagerEntryCountOffset = 0x1480U;
constexpr std::size_t kDirectiveManagerEntryStride = 0x148U;
constexpr std::size_t kDirectiveManagerMaximumEntries = 16U;
constexpr std::uint8_t kDirectiveManagerEntryKind = 2U;
/** Bound the observe-only Towerfall trace without filtering its runtime key domain. */
constexpr std::uint32_t kTowerfallMaximumComponentInitializations = 128U;
constexpr std::uint32_t kTowerfallMaximumNativeLookups = 256U;
constexpr std::uint32_t kTowerfallMaximumNativeInstalls = 128U;
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

enum HookIndex : std::size_t {
    componentInitializeIndex,
    contentLookupIndex,
    directiveInstallIndex,
    hookCount,
};
std::array<hooking::detour::Handle, hookCount> g_handles{};
std::atomic_bool g_installed{};
std::atomic<DirectiveComponentInitialize> g_componentInitializeOriginal{};
std::atomic<DirectiveContentResolve> g_contentResolve{};
std::atomic<DirectiveContentLookup> g_contentLookup{};
std::atomic<DirectiveInstall> g_directiveInstall{};
std::atomic<DirectiveContentLookup> g_contentLookupOriginal{};
std::atomic<DirectiveInstall> g_directiveInstallOriginal{};
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
std::atomic_uint32_t g_towerfallComponentInitializations{};
std::atomic_uint32_t g_towerfallNativeLookups{};
std::atomic_uint32_t g_towerfallInstalls{};

template <std::size_t N>
[[nodiscard]] bool prefix_matches(const std::byte* target,
                                  const std::array<std::byte, N>& prefix) noexcept {
    return target != nullptr && std::equal(prefix.begin(), prefix.end(), target);
}

[[nodiscard]] bool forced_activity_is(std::string_view expected) noexcept {
    state::activity::forced::ForcedDestination forced{};
    state::activity::forced::snapshot(forced);
    const std::size_t length =
        (std::min)(static_cast<std::size_t>(forced.packageNameLength), forced.packageName.size());
    return state::activity::forced::override_active()
           && std::string_view(forced.packageName.data(), length) == expected;
}

[[nodiscard]] bool omega_is_forced() noexcept {
    return forced_activity_is("mission_scot");
}

[[nodiscard]] bool towerfall_is_forced() noexcept {
    return forced_activity_is("mission_towerfall");
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

[[nodiscard]] bool towerfall_directive_key(std::uint32_t key) noexcept {
    return std::find(kTowerfallDirectiveKeys.begin(), kTowerfallDirectiveKeys.end(), key)
           != kTowerfallDirectiveKeys.end();
}

[[nodiscard]] bool towerfall_value_hash(std::uint32_t hash) noexcept {
    return std::find(kTowerfallValueHashes.begin(), kTowerfallValueHashes.end(), hash)
           != kTowerfallValueHashes.end();
}

[[nodiscard]] std::uintptr_t caller_rva(const void* caller) noexcept {
    const auto image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const auto address = reinterpret_cast<std::uintptr_t>(caller);
    return image != 0U && address >= image ? address - image : 0U;
}

struct TowerfallRecordSummary final {
    std::array<std::uint64_t, 8> words{};
    std::uint64_t digest{};
    std::uint32_t cueHashHits{};
    std::uint32_t firstCueHash{};
    std::uint32_t firstCueOffset{0xFFFFFFFFU};
    bool readable{};
};

struct TowerfallPointerSummary final {
    std::array<std::uint64_t, 4> words{};
    std::size_t bytes{};
    bool readable{};
};

[[nodiscard]] TowerfallPointerSummary summarize_towerfall_pointer(const void* pointer,
                                                                   std::size_t extent) noexcept {
    TowerfallPointerSummary output{};
    if (pointer == nullptr || extent == 0U || extent > sizeof output.words) {
        return output;
    }
    __try {
        std::memcpy(output.words.data(), pointer, extent);
        output.bytes = extent;
        output.readable = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return {};
    }
    return output;
}

[[nodiscard]] TowerfallRecordSummary summarize_towerfall_record(const void* record,
                                                                 std::size_t extent) noexcept {
    TowerfallRecordSummary output{};
    if (record == nullptr || extent < sizeof output.words || extent > kDirectiveRecordBytes) {
        return output;
    }
    std::array<std::byte, kDirectiveRecordBytes> bytes{};
    __try {
        std::memcpy(bytes.data(), record, extent);
        std::memcpy(output.words.data(), bytes.data(), sizeof output.words);
        output.readable = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return {};
    }
    output.digest = 1469598103934665603ULL;
    for (std::size_t index = 0; index < extent; ++index) {
        const std::byte value = bytes[index];
        output.digest ^= std::to_integer<std::uint8_t>(value);
        output.digest *= 1099511628211ULL;
    }
    for (std::size_t offset = 0; offset + sizeof(std::uint32_t) <= extent;
         offset += sizeof(std::uint32_t)) {
        std::uint32_t value = 0U;
        std::memcpy(&value, bytes.data() + offset, sizeof value);
        if (!towerfall_value_hash(value)) {
            continue;
        }
        if (output.cueHashHits == 0U) {
            output.firstCueHash = value;
            output.firstCueOffset = static_cast<std::uint32_t>(offset);
        }
        ++output.cueHashHits;
    }
    return output;
}

void report_towerfall_consumer_record(const char* stage,
                                      const char* origin,
                                      std::uint32_t sequence,
                                      std::uintptr_t caller,
                                      std::uint32_t* component,
                                      void* content,
                                      std::uint32_t key,
                                      bool keyReadable,
                                      void* record,
                                      std::size_t recordBytes) noexcept {
    const TowerfallRecordSummary summary = summarize_towerfall_record(record, recordBytes);
    std::uint32_t datum = 0xFFFFFFFFU;
    if (component != nullptr) {
        __try {
            datum = *component;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            datum = 0xFFFFFFFFU;
        }
    }
    std::array<char, core::log::kLineCapacity> line{};
    const int written = std::snprintf(
        line.data(),
        line.size(),
        "ev=towerfall_cue_consumer stage=%s origin=%s n=%u caller_rva=0x%llX component=%p datum=0x%08X content=%p key=0x%08X key_readable=%u known_key=%u record=%p record_bytes=%zu readable=%u record_hash=0x%016llX cue_hash_hits=%u first_cue_hash=0x%08X first_cue_offset=%u q0=0x%016llX q1=0x%016llX q2=0x%016llX q3=0x%016llX q4=0x%016llX q5=0x%016llX q6=0x%016llX q7=0x%016llX mutation=observe_only",
        stage,
        origin,
        sequence,
        static_cast<unsigned long long>(caller),
        component,
        datum,
        content,
        key,
        keyReadable ? 1U : 0U,
        keyReadable && towerfall_directive_key(key) ? 1U : 0U,
        record,
        recordBytes,
        summary.readable ? 1U : 0U,
        static_cast<unsigned long long>(summary.digest),
        summary.cueHashHits,
        summary.firstCueHash,
        summary.firstCueOffset,
        static_cast<unsigned long long>(summary.words[0]),
        static_cast<unsigned long long>(summary.words[1]),
        static_cast<unsigned long long>(summary.words[2]),
        static_cast<unsigned long long>(summary.words[3]),
        static_cast<unsigned long long>(summary.words[4]),
        static_cast<unsigned long long>(summary.words[5]),
        static_cast<unsigned long long>(summary.words[6]),
        static_cast<unsigned long long>(summary.words[7]));
    if (written > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(),
                          static_cast<std::size_t>(written) < line.size()
                              ? static_cast<std::size_t>(written)
                              : line.size() - 1U});
    }
}

void report_towerfall_component_initialization(std::uint32_t sequence,
                                               std::uintptr_t caller,
                                               std::uint32_t* component,
                                               const void* stateKey,
                                               void* content,
                                               bool probeAttempted,
                                               bool probeException,
    std::size_t hits,
    std::uint32_t hitMask) noexcept {
    const TowerfallPointerSummary componentSummary =
        summarize_towerfall_pointer(component, 4U * sizeof(std::uint64_t));
    const TowerfallPointerSummary stateSummary =
        summarize_towerfall_pointer(stateKey, 2U * sizeof(std::uint64_t));
    std::array<char, core::log::kLineCapacity> line{};
    const int written = std::snprintf(
        line.data(),
        line.size(),
        "ev=towerfall_cue_consumer stage=component_initialize origin=native n=%u caller_rva=0x%llX component=%p state_key=%p content=%p probe_attempted=%u probe_exception=%u hit_count=%zu hit_mask=0x%04X component_readable=%u component_bytes=%zu component_q0=0x%016llX component_q1=0x%016llX component_q2=0x%016llX component_q3=0x%016llX state_readable=%u state_bytes=%zu state_q0=0x%016llX state_q1=0x%016llX world_phase=%u seed_armed=%u mutation=observe_only",
        sequence,
        static_cast<unsigned long long>(caller),
        component,
        stateKey,
        content,
        probeAttempted ? 1U : 0U,
        probeException ? 1U : 0U,
        hits,
        hitMask,
        componentSummary.readable ? 1U : 0U,
        componentSummary.bytes,
        static_cast<unsigned long long>(componentSummary.words[0]),
        static_cast<unsigned long long>(componentSummary.words[1]),
        static_cast<unsigned long long>(componentSummary.words[2]),
        static_cast<unsigned long long>(componentSummary.words[3]),
        stateSummary.readable ? 1U : 0U,
        stateSummary.bytes,
        static_cast<unsigned long long>(stateSummary.words[0]),
        static_cast<unsigned long long>(stateSummary.words[1]),
        static_cast<unsigned int>(state::activity::world_phase()),
        state::activity::mission_seed_armed() ? 1U : 0U);
    if (written > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(),
                          static_cast<std::size_t>(written) < line.size()
                              ? static_cast<std::size_t>(written)
                              : line.size() - 1U});
    }
}

void trace_towerfall_component(std::uint32_t* component,
                               const void* stateKey,
                               std::uintptr_t caller) noexcept {
    if (component == nullptr || !towerfall_is_forced()) {
        return;
    }
    const std::uint32_t sequence =
        g_towerfallComponentInitializations.fetch_add(1U, std::memory_order_relaxed) + 1U;
    if (sequence > kTowerfallMaximumComponentInitializations) {
        return;
    }
    const DirectiveContentResolve contentResolve = g_contentResolve.load(std::memory_order_acquire);
    const DirectiveContentLookup contentLookup = g_contentLookup.load(std::memory_order_acquire);
    const bool probeAttempted = contentResolve != nullptr && contentLookup != nullptr;
    void* content = nullptr;
    std::array<void*, kTowerfallDirectiveKeys.size()> records{};
    std::size_t hits = 0;
    std::uint32_t hitMask = 0U;
    bool probeException = false;
    if (probeAttempted) {
        __try {
            content = contentResolve(component);
            if (content != nullptr) {
                for (std::size_t index = 0; index < kTowerfallDirectiveKeys.size(); ++index) {
                    std::uint32_t key = kTowerfallDirectiveKeys[index];
                    records[index] = contentLookup(content, &key);
                    if (records[index] != nullptr) {
                        ++hits;
                        hitMask |= 1U << index;
                    }
                }
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            content = nullptr;
            records.fill(nullptr);
            hits = 0;
            hitMask = 0U;
            probeException = true;
        }
    }
    report_towerfall_component_initialization(sequence,
                                              caller,
                                              component,
                                              stateKey,
                                              content,
                                              probeAttempted,
                                              probeException,
                                              hits,
                                              hitMask);
    for (std::size_t index = 0; index < records.size(); ++index) {
        if (records[index] != nullptr) {
            report_towerfall_consumer_record("content_enumerate",
                                             "probe_read_only",
                                             sequence,
                                             caller,
                                             component,
                                             content,
                                             kTowerfallDirectiveKeys[index],
                                             true,
                                             records[index],
                                             kDirectiveLookupSnapshotBytes);
        }
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
    std::uintptr_t caller,
    const hooking::CallGate::Scope& call) noexcept {
    const DirectiveComponentInitialize original =
        hooking::await_original(g_componentInitializeOriginal);
    const std::uint64_t result = original(component, stateKey);
    if (!call.accepts_side_effects() || component == nullptr) {
        return result;
    }
    trace_towerfall_component(component, stateKey, caller);
    if (!omega_is_forced()) {
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
    const std::uintptr_t caller = caller_rva(_ReturnAddress());
    hooking::CallGate::Scope call(g_callGate);
    return directive_component_initialize_body(component, stateKey, caller, call);
}

void* directive_content_lookup_body(
    void* content,
    const std::uint32_t* keyPointer,
    std::uintptr_t caller,
    const hooking::CallGate::Scope& call) noexcept {
    const DirectiveContentLookup original = hooking::await_original(g_contentLookupOriginal);
    std::uint32_t key = 0U;
    bool readableKey = false;
    if (keyPointer != nullptr) {
        __try {
            key = *keyPointer;
            readableKey = true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            readableKey = false;
        }
    }
    void* const record = original(content, keyPointer);
    if (call.accepts_side_effects() && towerfall_is_forced()) {
        const std::uint32_t sequence =
            g_towerfallNativeLookups.fetch_add(1U, std::memory_order_relaxed) + 1U;
        if (sequence <= kTowerfallMaximumNativeLookups) {
            report_towerfall_consumer_record("content_lookup",
                                             "native",
                                             sequence,
                                             caller,
                                             nullptr,
                                             content,
                                             key,
                                             readableKey,
                                             record,
                                             record != nullptr ? kDirectiveLookupSnapshotBytes
                                                               : 0U);
        }
    }
    return record;
}

__declspec(noinline) void* __fastcall directive_content_lookup(
    void* content,
    const std::uint32_t* keyPointer) noexcept {
    const std::uintptr_t caller = caller_rva(_ReturnAddress());
    hooking::CallGate::Scope call(g_callGate);
    return directive_content_lookup_body(content, keyPointer, caller, call);
}

void directive_install_observer_body(
    std::uint32_t* component,
    std::byte* record,
    void* content,
    std::uintptr_t caller,
    const hooking::CallGate::Scope& call) noexcept {
    const DirectiveInstall original = hooking::await_original(g_directiveInstallOriginal);
    std::uint32_t key = 0U;
    bool readableKey = false;
    if (record != nullptr) {
        __try {
            std::memcpy(&key, record, sizeof key);
            readableKey = true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            readableKey = false;
        }
    }
    const bool trace = call.accepts_side_effects() && towerfall_is_forced();
    const std::uint32_t sequence = trace
                                       ? g_towerfallInstalls.fetch_add(
                                             1U, std::memory_order_relaxed)
                                             + 1U
                                       : 0U;
    if (trace && sequence <= kTowerfallMaximumNativeInstalls) {
        report_towerfall_consumer_record("record_install_enter",
                                         "native",
                                         sequence,
                                         caller,
                                         component,
                                         content,
                                         key,
                                         readableKey,
                                         record,
                                         record != nullptr ? kDirectiveRecordBytes : 0U);
    }
    original(component, record, content);
    if (trace && sequence <= kTowerfallMaximumNativeInstalls
        && call.accepts_side_effects()) {
        report_towerfall_consumer_record("record_install_exit",
                                         "native",
                                         sequence,
                                         caller,
                                         component,
                                         content,
                                         key,
                                         readableKey,
                                         record,
                                         record != nullptr ? kDirectiveRecordBytes : 0U);
    }
}

__declspec(noinline) void __fastcall directive_install_observer(
    std::uint32_t* component,
    std::byte* record,
    void* content) noexcept {
    const std::uintptr_t caller = caller_rva(_ReturnAddress());
    hooking::CallGate::Scope call(g_callGate);
    directive_install_observer_body(component, record, content, caller, call);
}

} // namespace

bool omega_directive_native::available() noexcept {
    return g_installed.load(std::memory_order_acquire) && g_callGate.accepting()
        && g_directiveInstallOriginal.load(std::memory_order_acquire) != nullptr;
}

bool omega_directive_native::install_checkpoint(std::byte* component, std::byte* record,
                                                void* content) noexcept {
    hooking::CallGate::Scope call(g_callGate);
    const auto original = g_directiveInstallOriginal.load(std::memory_order_acquire);
    if (!call.accepts_side_effects() || !original || !component || !record || !content) return false;
    original(reinterpret_cast<std::uint32_t*>(component), record, content);
    return true;
}

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
    const std::array<hooking::detour::Spec, hookCount> specs{
        hooking::detour::Spec{initializeTarget,
                              reinterpret_cast<void*>(&directive_component_initialize)},
        hooking::detour::Spec{contentLookupTarget,
                              reinterpret_cast<void*>(&directive_content_lookup)},
        hooking::detour::Spec{installTarget,
                              reinterpret_cast<void*>(&directive_install_observer)},
    };
    if (!hooking::detour::install(specs, g_handles)) {
        g_contentResolve.store(nullptr, std::memory_order_release);
        g_contentLookup.store(nullptr, std::memory_order_release);
        g_directiveInstall.store(nullptr, std::memory_order_release);
        g_managerReady.store(nullptr, std::memory_order_release);
        g_managerGet.store(nullptr, std::memory_order_release);
        return fail_install("attach");
    }
    hooking::publish_original(
        g_componentInitializeOriginal,
        reinterpret_cast<DirectiveComponentInitialize>(
            g_handles[componentInitializeIndex].original));
    hooking::publish_original(
        g_contentLookupOriginal,
        reinterpret_cast<DirectiveContentLookup>(g_handles[contentLookupIndex].original));
    hooking::publish_original(
        g_directiveInstallOriginal,
        reinterpret_cast<DirectiveInstall>(g_handles[directiveInstallIndex].original));
    g_contentLookup.store(g_contentLookupOriginal.load(std::memory_order_acquire),
                          std::memory_order_release);
    g_directiveInstall.store(g_directiveInstallOriginal.load(std::memory_order_acquire),
                             std::memory_order_release);
    reset_attempt_state(true);
    g_idleReset.store(false, std::memory_order_release);
    g_towerfallComponentInitializations.store(0U, std::memory_order_release);
    g_towerfallNativeLookups.store(0U, std::memory_order_release);
    g_towerfallInstalls.store(0U, std::memory_order_release);
    g_installed.store(true, std::memory_order_release);
    g_callGate.accept();
    core::log::write(
        core::log::Channel::client,
        core::log::Level::info,
        "ev=activity stage=omega_directive_presentation_install result=ok "
        "component=+1009630 content=+1009430 lookup=+100A110 install=+1009ED0 "
        "manager=+137D6F0 event=0xC252E306 towerfall_consumer=observe_only "
        "component_zero_hits=reported lookup_scope=all install_scope=all "
        "timing=post_arrival mutation=local_ui_guarded");
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
            reinterpret_cast<void*>(&directive_content_lookup)},
        hooking::detour::ProtectedCodeEntry{
            reinterpret_cast<void*>(&directive_install_observer)},
        hooking::detour::ProtectedCodeEntry{
            reinterpret_cast<void*>(&sample_omega_directive_presentation)},
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
                ? "ev=activity stage=omega_directive_presentation_uninstall result=failed retained=1"
                : "ev=activity stage=omega_directive_presentation_uninstall result=deferred retained=1");
        return false;
    }

    g_componentInitializeOriginal.store(nullptr, std::memory_order_release);
    g_contentLookupOriginal.store(nullptr, std::memory_order_release);
    g_directiveInstallOriginal.store(nullptr, std::memory_order_release);
    g_contentResolve.store(nullptr, std::memory_order_release);
    g_contentLookup.store(nullptr, std::memory_order_release);
    g_directiveInstall.store(nullptr, std::memory_order_release);
    g_managerReady.store(nullptr, std::memory_order_release);
    g_managerGet.store(nullptr, std::memory_order_release);
    reset_attempt_state(true);
    g_idleReset.store(false, std::memory_order_release);
    g_towerfallComponentInitializations.store(0U, std::memory_order_release);
    g_towerfallNativeLookups.store(0U, std::memory_order_release);
    g_towerfallInstalls.store(0U, std::memory_order_release);
    g_installed.store(false, std::memory_order_release);
    core::log::write(
        core::log::Channel::client,
        core::log::Level::info,
        "ev=activity stage=omega_directive_presentation_uninstall result=ok retained=0");
    return true;
}

} // namespace sunrise::client::hooks::bootflow
