#include <Windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <intrin.h>
#include <limits>
#include <span>

#include "../../../core/logging/log.h"
#include "../../hooking/detour.h"
#include "../../player/player_position.h"
#include "bootflow_hook_lifecycle.h"
#include "internal.h"
#include "legacy_owner_sentinel.h"

namespace sunrise::client::hooks::bootflow {
namespace {

// Pinned-client functions confirmed by the 2026-08-20 headless decompile. These are observation
// sites only: no condition return value, runner state, object state, or authoritative data is
// changed by this probe.
constexpr std::uintptr_t kCondition7ARva = 0x1087F70U;
constexpr std::uintptr_t kCondition75Rva = 0x100C000U;
constexpr std::uintptr_t kPropertyNameMatchRva = 0x184FD70U;
constexpr std::uintptr_t kScriptRunnerStartRva = 0x10859C0U;
constexpr std::uintptr_t kScriptRunnerTickRva = 0x10881A0U;
constexpr std::uintptr_t kVmSpawnRva = 0xDF8BD0U;
constexpr std::uintptr_t kComponentConstructRva = 0x56DE00U;
constexpr std::uintptr_t kComponentAttachRva = 0x56A020U;
constexpr std::uintptr_t kComponentRemoveRva = 0x56AF40U;

constexpr std::array<std::byte, 16> kCondition7APrefix{
    std::byte{0x48}, std::byte{0x8B}, std::byte{0xC4}, std::byte{0x48},
    std::byte{0x89}, std::byte{0x68}, std::byte{0x10}, std::byte{0x48},
    std::byte{0x89}, std::byte{0x70}, std::byte{0x18}, std::byte{0x57},
    std::byte{0x48}, std::byte{0x83}, std::byte{0xEC}, std::byte{0x70}};
constexpr std::array<std::byte, 16> kCondition75Prefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x08}, std::byte{0x55}, std::byte{0x48}, std::byte{0x8D},
    std::byte{0xAC}, std::byte{0x24}, std::byte{0x50}, std::byte{0xF4},
    std::byte{0xFF}, std::byte{0xFF}, std::byte{0x48}, std::byte{0x81}};
constexpr std::array<std::byte, 15> kPropertyNameMatchPrefix{
    std::byte{0x48}, std::byte{0x83}, std::byte{0xEC}, std::byte{0x28},
    std::byte{0x4C}, std::byte{0x8B}, std::byte{0x01}, std::byte{0x48},
    std::byte{0x8B}, std::byte{0x49}, std::byte{0x08}, std::byte{0x49},
    std::byte{0x8B}, std::byte{0x40}, std::byte{0x18}};
constexpr std::array<std::byte, 9> kScriptRunnerStartPrefix{
    std::byte{0x40}, std::byte{0x57}, std::byte{0x48}, std::byte{0x81},
    std::byte{0xEC}, std::byte{0xD0}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
constexpr std::array<std::byte, 11> kScriptRunnerTickPrefix{
    std::byte{0x40}, std::byte{0x56}, std::byte{0x41}, std::byte{0x57},
    std::byte{0x48}, std::byte{0x81}, std::byte{0xEC}, std::byte{0xA8},
    std::byte{0x01}, std::byte{0x00}, std::byte{0x00}};
constexpr std::array<std::byte, 13> kVmSpawnPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x20}, std::byte{0x56}, std::byte{0x48}, std::byte{0x81},
    std::byte{0xEC}, std::byte{0x80}, std::byte{0x08}, std::byte{0x00}, std::byte{0x00}};
constexpr std::array<std::byte, 16> kComponentConstructPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x20}, std::byte{0x55}, std::byte{0x56}, std::byte{0x57},
    std::byte{0x41}, std::byte{0x56}, std::byte{0x41}, std::byte{0x57},
    std::byte{0x48}, std::byte{0x8D}, std::byte{0x6C}, std::byte{0x24}};
constexpr std::array<std::byte, 16> kComponentAttachPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x08}, std::byte{0x55}, std::byte{0x48}, std::byte{0x8B},
    std::byte{0xEC}, std::byte{0x48}, std::byte{0x81}, std::byte{0xEC},
    std::byte{0x80}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
constexpr std::array<std::byte, 16> kComponentRemovePrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x20}, std::byte{0x55}, std::byte{0x57}, std::byte{0x41},
    std::byte{0x56}, std::byte{0x48}, std::byte{0x83}, std::byte{0xEC},
    std::byte{0x20}, std::byte{0x8B}, std::byte{0xE9}, std::byte{0x8B}};

// Object positions are stored in the datum table as protected float bits. These locations are
// from +1087F70 itself, not from the older note whose helper/table RVAs belonged to another image.
constexpr std::uintptr_t kObjectTablePointerRva = 0x1F93428U;
constexpr std::uintptr_t kObjectTableStrideRva = 0x1F93430U;
constexpr std::uintptr_t kPositionMaskOneRva = 0x1B9E420U;
constexpr std::uintptr_t kPositionMaskTwoRva = 0x1B9E430U;
constexpr std::uintptr_t kPositionKeyXyRva = 0x6260781U;
constexpr std::uintptr_t kPositionKeyZwRva = 0x584DFF2U;
constexpr std::size_t kObjectPositionOffset = 0xD0U;
constexpr std::uint32_t kDatumIndexMask = 0x1FFFU;

// +100C000 initializes the native property iterator through +591110. Repeating that read-only
// initialization in sampled calls lets the log distinguish an empty iterator from a name miss.
constexpr std::uintptr_t kPropertyIteratorInitRva = 0x591110U;
constexpr std::uintptr_t kPropertyTypePointerRva = 0x2091350U;
/** Global datum-pool directory used verbatim by the observed +C994A0 candidate predicate. */
constexpr std::uintptr_t kDatumPoolDirectoryPointerRva = 0x2439C70U;
constexpr std::size_t kPropertyIteratorSize = 0xC20U;
constexpr std::size_t kPropertyIteratorStateOffset = 0xC0CU;

constexpr std::uint64_t kCondition7ASampleMs = 500U;
constexpr std::uint64_t kCondition75SampleMs = 1000U;
constexpr std::uint32_t kInitialConditionSamples = 16U;
constexpr std::uint32_t kInitialRunnerSamples = 32U;
constexpr std::uint32_t kTickSamplePeriod = 500U;
constexpr std::uint32_t kVmSpawnSamplePeriod = 250U;
/** More than the known six Omega runners, while keeping the UI snapshot bounded. */
constexpr std::size_t kMaximumObservedAnchors = 32U;
/** A later runner-start burst belongs to a new destination and replaces the old anchor bank. */
constexpr std::uint64_t kRunnerBatchGapMs = 2'000U;
/** One Omega lookup repeats the same small candidate set every tick. Log each shape once. */
constexpr std::size_t kMaximumPropertyCandidates = 64U;
/** Keeps pre-trigger component history without capturing a call stack on every construction. */
constexpr std::size_t kMaximumComponentMutations = 32'768U;
constexpr std::size_t kMaximumMutationDump = 128U;
constexpr std::size_t kMaximumMutationDumpKeys = 64U;

using Condition7A = bool(__fastcall*)(const float* range, const std::byte* context) noexcept;
using Condition75 = bool(__fastcall*)(const std::uint32_t* nameHash,
                                      const std::byte* context) noexcept;
using PropertyNameMatch = bool(__fastcall*)(const std::uintptr_t* property,
                                             const std::uint32_t* nameHash) noexcept;
using ScriptRunnerStart = void(__fastcall*)(const std::byte* component) noexcept;
using ScriptRunnerTick = std::uint64_t(__fastcall*)(const std::byte* component) noexcept;
using VmSpawn = void(__fastcall*)(const std::byte* node, const std::byte* context) noexcept;
using ComponentConstruct = std::int32_t*(__fastcall*)(std::int32_t* result,
                                                       std::byte* descriptor) noexcept;
using ComponentAttach = void(__fastcall*)(std::uint32_t owner,
                                           std::uint32_t component) noexcept;
using ComponentRemove = void(__fastcall*)(std::uint32_t owner,
                                           std::uint32_t component) noexcept;
using PositionKey = std::uint32_t(__fastcall*)() noexcept;
using PropertyIteratorInit = void(__fastcall*)(void* iterator,
                                                std::uint32_t objectHandle,
                                                std::uint32_t propertyType,
                                                bool includeDerived) noexcept;

enum class HookSlot : std::size_t {
    condition7A,
    condition75,
    propertyNameMatch,
    runnerStart,
    runnerTick,
    vmSpawn,
    componentConstruct,
    componentAttach,
    componentRemove,
    count,
};

constexpr std::size_t kHookCount = static_cast<std::size_t>(HookSlot::count);

std::array<hooking::detour::Handle, kHookCount> g_handles{};
std::atomic<Condition7A> g_condition7AOriginal{nullptr};
std::atomic<Condition75> g_condition75Original{nullptr};
std::atomic<PropertyNameMatch> g_propertyNameMatchOriginal{nullptr};
std::atomic<ScriptRunnerStart> g_runnerStartOriginal{nullptr};
std::atomic<ScriptRunnerTick> g_runnerTickOriginal{nullptr};
std::atomic<VmSpawn> g_vmSpawnOriginal{nullptr};
std::atomic<ComponentConstruct> g_componentConstructOriginal{nullptr};
std::atomic<ComponentAttach> g_componentAttachOriginal{nullptr};
std::atomic<ComponentRemove> g_componentRemoveOriginal{nullptr};
std::atomic<std::byte*> g_image{nullptr};
std::atomic_uint32_t g_condition7ACalls{};
std::atomic_uint32_t g_condition75Calls{};
std::atomic_uint32_t g_runnerStarts{};
std::atomic_uint32_t g_runnerTicks{};
std::atomic_uint32_t g_vmSpawns{};
std::atomic_uint64_t g_condition7ALastLog{};
std::atomic_uint64_t g_condition75LastLog{};
std::atomic_uint64_t g_lastRunnerStartTick{};
std::atomic_bool g_installed{};

/** Active only while this thread is inside +100C000, filtering the otherwise shared matcher. */
thread_local std::uint32_t g_condition75Depth{};
thread_local std::uint32_t g_condition75Observation{};
thread_local std::uint32_t g_condition75Target{std::numeric_limits<std::uint32_t>::max()};
thread_local std::uint32_t g_condition75MatchCalls{};
thread_local std::uint32_t g_condition75Matches{};

struct ObservedAnchor final {
    std::array<float, 3> position{};
    float requiredMaximum{};
    std::uint32_t handle{std::numeric_limits<std::uint32_t>::max()};
    bool valid{};
};

struct ObservedPropertyCandidate final {
    std::uintptr_t owner{};
    std::uintptr_t value{};
    std::uintptr_t resolved{};
    std::uint32_t nameHash{};
    std::uint32_t candidateHash{};
    bool match{};
    bool valid{};
};

enum class ComponentMutationKind : std::uint8_t {
    construct,
    attach,
    remove,
};

struct ComponentMutation final {
    std::uint64_t sequence{};
    std::uint64_t tick{};
    std::uint32_t owner{std::numeric_limits<std::uint32_t>::max()};
    std::uint32_t component{std::numeric_limits<std::uint32_t>::max()};
    std::uintptr_t callerRva{};
    ComponentMutationKind kind{ComponentMutationKind::construct};
    std::array<std::uint64_t, 11> descriptor{};
};

struct MutationDumpKey final {
    std::uint32_t target{std::numeric_limits<std::uint32_t>::max()};
    std::uint32_t component{std::numeric_limits<std::uint32_t>::max()};
    std::uint32_t requiredHash{};
    std::uint32_t candidateHash{};
    bool valid{};
};

/** Shared only by sampled game-thread observations and the presentation-thread HUD. */
SRWLOCK g_anchorLock{SRWLOCK_INIT};
std::array<ObservedAnchor, kMaximumObservedAnchors> g_anchors{};
std::array<float, 3> g_playerOriginOffset{};
std::uint32_t g_observedPlayerHandle{std::numeric_limits<std::uint32_t>::max()};
bool g_playerOriginOffsetValid{};
SRWLOCK g_propertyCandidateLock{SRWLOCK_INIT};
std::array<ObservedPropertyCandidate, kMaximumPropertyCandidates> g_propertyCandidates{};
SRWLOCK g_componentMutationLock{SRWLOCK_INIT};
std::array<ComponentMutation, kMaximumComponentMutations> g_componentMutations{};
std::array<MutationDumpKey, kMaximumMutationDumpKeys> g_mutationDumpKeys{};
std::uint64_t g_componentMutationSequence{};

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

struct ResolvedPropertyCandidate final {
    std::uintptr_t address{};
    std::uint32_t datum{std::numeric_limits<std::uint32_t>::max()};
    std::uint32_t nameHash{};
    bool valid{};
};

/** Mirrors +C994A0's datum-pool arithmetic and reads only its compared object+0x9C field. */
[[nodiscard]] ResolvedPropertyCandidate
resolve_property_candidate(std::uintptr_t value) noexcept {
    ResolvedPropertyCandidate result{};
    std::byte* const image = g_image.load(std::memory_order_acquire);
    if (image == nullptr || value == 0U) {
        return result;
    }
    result.datum = safe_read<std::uint32_t>(reinterpret_cast<const void*>(value),
                                            std::numeric_limits<std::uint32_t>::max());
    const std::uintptr_t relative = safe_read<std::uintptr_t>(
        reinterpret_cast<const void*>(value + 8U), std::numeric_limits<std::uintptr_t>::max());
    if (result.datum == std::numeric_limits<std::uint32_t>::max()
        || relative == std::numeric_limits<std::uintptr_t>::max() || relative > 0x10000000U) {
        return result;
    }

    const std::int32_t shifted = static_cast<std::int32_t>(result.datum) >> 13U;
    const std::uint32_t poolIndex =
        ((static_cast<std::uint32_t>(shifted) | 0x0FFC0000U) >> 18U)
        & static_cast<std::uint16_t>(shifted);
    const std::byte* const directoryHolder = safe_read<const std::byte*>(
        image + kDatumPoolDirectoryPointerRva, nullptr);
    const std::byte* const directory = safe_read<const std::byte*>(directoryHolder, nullptr);
    if (directory == nullptr || poolIndex > 0xFFFFU) {
        return result;
    }
    const std::byte* const pool = directory + static_cast<std::size_t>(poolIndex) * 0x40U;
    const std::byte* const storage = safe_read<const std::byte*>(pool + 8U, nullptr);
    const std::int32_t stride = safe_read<std::int32_t>(pool + 0x30U, 0);
    const std::int32_t mask = safe_read<std::int32_t>(pool + 0x34U, 0);
    if (storage == nullptr || stride <= 0 || stride > 0x100000) {
        return result;
    }
    const std::uintptr_t slot = reinterpret_cast<std::uintptr_t>(storage)
                                + static_cast<std::size_t>(result.datum & kDatumIndexMask)
                                      * static_cast<std::size_t>(stride);
    const std::uintptr_t masked = static_cast<std::uintptr_t>(static_cast<std::intptr_t>(mask))
                                  & safe_read<std::uintptr_t>(
                                      reinterpret_cast<const void*>(slot + 8U), 0U);
    result.address = slot - masked + relative;
    result.nameHash = safe_read<std::uint32_t>(
        reinterpret_cast<const void*>(result.address + 0x9CU), 0U);
    result.valid = result.address != 0U;
    return result;
}

void write_line(core::log::Level level, const char* line, int written) noexcept {
    if (written <= 0) {
        return;
    }
    const std::size_t length =
        (std::min)(static_cast<std::size_t>(written), core::log::kLineCapacity - 1U);
    core::log::write(core::log::Channel::client, level, {line, length});
}

[[nodiscard]] std::uintptr_t image_rva(const void* address) noexcept {
    const std::byte* const image = g_image.load(std::memory_order_acquire);
    const auto* const value = static_cast<const std::byte*>(address);
    return image != nullptr && value >= image
               ? static_cast<std::uintptr_t>(value - image)
               : 0U;
}

void clear_component_mutations() noexcept {
    AcquireSRWLockExclusive(&g_componentMutationLock);
    g_componentMutations = {};
    g_mutationDumpKeys = {};
    g_componentMutationSequence = 0U;
    ReleaseSRWLockExclusive(&g_componentMutationLock);
}

void record_component_mutation(ComponentMutationKind kind,
                               std::uint32_t owner,
                               std::uint32_t component,
                               const void* caller,
                               const std::byte* descriptor = nullptr) noexcept {
    ComponentMutation event{};
    event.tick = GetTickCount64();
    event.owner = owner;
    event.component = component;
    event.callerRva = image_rva(caller);
    event.kind = kind;
    if (descriptor != nullptr) {
        for (std::size_t index = 0; index < event.descriptor.size(); ++index) {
            event.descriptor[index] = safe_read<std::uint64_t>(
                descriptor + index * sizeof(std::uint64_t), 0U);
        }
    }

    AcquireSRWLockExclusive(&g_componentMutationLock);
    event.sequence = ++g_componentMutationSequence;
    g_componentMutations[(event.sequence - 1U) % g_componentMutations.size()] = event;
    ReleaseSRWLockExclusive(&g_componentMutationLock);
}

[[nodiscard]] const char* component_mutation_name(ComponentMutationKind kind) noexcept {
    switch (kind) {
    case ComponentMutationKind::construct:
        return "construct";
    case ComponentMutationKind::attach:
        return "attach";
    case ComponentMutationKind::remove:
        return "remove";
    }
    return "unknown";
}

/** Correlates pre-trigger component construction/removal with a native condition candidate. */
void dump_component_mutations(std::uint32_t target,
                              std::uint32_t component,
                              std::uint32_t requiredHash,
                              std::uint32_t candidateHash,
                              std::uint32_t conditionObservation) noexcept {
    std::array<ComponentMutation, kMaximumMutationDump> matches{};
    std::size_t copied = 0U;
    std::size_t matched = 0U;
    std::uint64_t total = 0U;
    std::uint64_t oldest = 0U;
    bool duplicate = false;
    bool keyStored = false;

    AcquireSRWLockExclusive(&g_componentMutationLock);
    MutationDumpKey* freeKey = nullptr;
    for (MutationDumpKey& key : g_mutationDumpKeys) {
        if (key.valid && key.target == target && key.component == component
            && key.requiredHash == requiredHash && key.candidateHash == candidateHash) {
            duplicate = true;
            break;
        }
        if (!key.valid && freeKey == nullptr) {
            freeKey = &key;
        }
    }
    if (!duplicate && freeKey != nullptr) {
        *freeKey = {target, component, requiredHash, candidateHash, true};
        keyStored = true;
        total = g_componentMutationSequence;
        oldest = total > g_componentMutations.size()
                     ? total - g_componentMutations.size() + 1U
                     : (total == 0U ? 0U : 1U);
        for (std::uint64_t sequence = oldest; sequence != 0U && sequence <= total; ++sequence) {
            const ComponentMutation& event =
                g_componentMutations[(sequence - 1U) % g_componentMutations.size()];
            if (event.sequence != sequence
                || (event.owner != target && event.component != component)) {
                continue;
            }
            matches[matched % matches.size()] = event;
            ++matched;
        }
        copied = (std::min)(matched, matches.size());
    }
    ReleaseSRWLockExclusive(&g_componentMutationLock);
    if (duplicate || !keyStored) {
        return;
    }

    std::sort(matches.begin(), matches.begin() + static_cast<std::ptrdiff_t>(copied),
              [](const ComponentMutation& left, const ComponentMutation& right) {
                  return left.sequence < right.sequence;
              });
    std::array<char, core::log::kLineCapacity> line{};
    int written = std::snprintf(
        line.data(),
        line.size(),
        "ev=omega_probe stage=cond_75_component_history condition_n=%u target=%08X "
        "component=%08X required_hash=%08X candidate_hash=%08X total=%llu oldest=%llu "
        "matched=%zu copied=%zu truncated=%u",
        conditionObservation,
        target,
        component,
        requiredHash,
        candidateHash,
        static_cast<unsigned long long>(total),
        static_cast<unsigned long long>(oldest),
        matched,
        copied,
        matched > copied ? 1U : 0U);
    write_line(core::log::Level::info, line.data(), written);

    for (const ComponentMutation& event : std::span(matches).first(copied)) {
        written = std::snprintf(
            line.data(),
            line.size(),
            "ev=omega_probe stage=cond_75_component_mutation condition_n=%u seq=%llu "
            "tick=%llu kind=%s target=%08X owner=%08X component=%08X "
            "required_hash=%08X candidate_hash=%08X caller_rva=0x%llX "
            "d00=%016llX d08=%016llX d10=%016llX d18=%016llX d20=%016llX "
            "d28=%016llX d30=%016llX d38=%016llX d40=%016llX d48=%016llX d50=%016llX",
            conditionObservation,
            static_cast<unsigned long long>(event.sequence),
            static_cast<unsigned long long>(event.tick),
            component_mutation_name(event.kind),
            target,
            event.owner,
            event.component,
            requiredHash,
            candidateHash,
            static_cast<unsigned long long>(event.callerRva),
            static_cast<unsigned long long>(event.descriptor[0]),
            static_cast<unsigned long long>(event.descriptor[1]),
            static_cast<unsigned long long>(event.descriptor[2]),
            static_cast<unsigned long long>(event.descriptor[3]),
            static_cast<unsigned long long>(event.descriptor[4]),
            static_cast<unsigned long long>(event.descriptor[5]),
            static_cast<unsigned long long>(event.descriptor[6]),
            static_cast<unsigned long long>(event.descriptor[7]),
            static_cast<unsigned long long>(event.descriptor[8]),
            static_cast<unsigned long long>(event.descriptor[9]),
            static_cast<unsigned long long>(event.descriptor[10]));
        write_line(core::log::Level::info, line.data(), written);
    }
}

void clear_property_candidates() noexcept {
    AcquireSRWLockExclusive(&g_propertyCandidateLock);
    g_propertyCandidates = {};
    ReleaseSRWLockExclusive(&g_propertyCandidateLock);
}

/** Logs one native iterator candidate once, including a later change from miss to match. */
void record_property_candidate(const std::uintptr_t* property,
                               const std::uint32_t* nameHash,
                               bool match) noexcept {
    const std::uintptr_t owner = safe_read<std::uintptr_t>(property, 0U);
    const std::uintptr_t value = safe_read<std::uintptr_t>(
        property != nullptr ? property + 1U : nullptr, 0U);
    const std::uint32_t hash = safe_read<std::uint32_t>(nameHash, 0U);
    const ResolvedPropertyCandidate resolved = resolve_property_candidate(value);
    std::size_t ordinal = kMaximumPropertyCandidates;
    bool duplicate = false;
    AcquireSRWLockExclusive(&g_propertyCandidateLock);
    for (std::size_t index = 0; index < g_propertyCandidates.size(); ++index) {
        ObservedPropertyCandidate& candidate = g_propertyCandidates[index];
        if (candidate.valid && candidate.owner == owner && candidate.value == value
            && candidate.resolved == resolved.address && candidate.nameHash == hash
            && candidate.candidateHash == resolved.nameHash && candidate.match == match) {
            duplicate = true;
            break;
        }
        if (!candidate.valid && ordinal == kMaximumPropertyCandidates) {
            ordinal = index;
        }
    }
    if (!duplicate && ordinal < g_propertyCandidates.size()) {
        g_propertyCandidates[ordinal] = {
            owner, value, resolved.address, hash, resolved.nameHash, match, true};
    }
    ReleaseSRWLockExclusive(&g_propertyCandidateLock);
    if (duplicate || ordinal >= g_propertyCandidates.size()) {
        return;
    }

    const std::uint64_t owner0 = safe_read<std::uint64_t>(
        reinterpret_cast<const void*>(owner), 0U);
    const std::uint64_t owner8 = safe_read<std::uint64_t>(
        reinterpret_cast<const void*>(owner + 8U), 0U);
    const std::uintptr_t owner18 = safe_read<std::uintptr_t>(
        reinterpret_cast<const void*>(owner + 0x18U), 0U);
    const std::uintptr_t matcher = safe_read<std::uintptr_t>(
        reinterpret_cast<const void*>(owner + owner18 + 0x48U), 0U);
    const std::uint64_t value0 = safe_read<std::uint64_t>(
        reinterpret_cast<const void*>(value), 0U);
    const std::uint64_t value8 = safe_read<std::uint64_t>(
        reinterpret_cast<const void*>(value + 8U), 0U);
    const std::uint64_t candidate90 = safe_read<std::uint64_t>(
        reinterpret_cast<const void*>(resolved.address + 0x90U), 0U);
    const std::uint64_t candidate98 = safe_read<std::uint64_t>(
        reinterpret_cast<const void*>(resolved.address + 0x98U), 0U);
    const std::uint64_t candidateA0 = safe_read<std::uint64_t>(
        reinterpret_cast<const void*>(resolved.address + 0xA0U), 0U);
    const std::byte* const image = g_image.load(std::memory_order_acquire);
    const std::uintptr_t imageAddress = reinterpret_cast<std::uintptr_t>(image);
    const std::uint64_t matcherRva = imageAddress != 0U && matcher >= imageAddress
                                         ? matcher - imageAddress
                                         : 0U;
    std::array<char, core::log::kLineCapacity> line{};
    const int written = std::snprintf(
        line.data(),
        line.size(),
        "ev=omega_probe stage=cond_75_candidate condition_n=%u ordinal=%zu result=%u "
        "name_hash=%08X target=%08X owner=%p value=%p owner0=%016llX owner8=%016llX "
        "owner18=%016llX value0=%016llX value8=%016llX matcher_rva=0x%llX "
        "resolved=%p datum=%08X candidate_hash=%08X candidate90=%016llX "
        "candidate98=%016llX candidateA0=%016llX",
        g_condition75Observation,
        ordinal,
        match ? 1U : 0U,
        hash,
        g_condition75Target,
        reinterpret_cast<const void*>(owner),
        reinterpret_cast<const void*>(value),
        static_cast<unsigned long long>(owner0),
        static_cast<unsigned long long>(owner8),
        static_cast<unsigned long long>(owner18),
        static_cast<unsigned long long>(value0),
        static_cast<unsigned long long>(value8),
        static_cast<unsigned long long>(matcherRva),
        reinterpret_cast<const void*>(resolved.address),
        resolved.datum,
        resolved.nameHash,
        static_cast<unsigned long long>(candidate90),
        static_cast<unsigned long long>(candidate98),
        static_cast<unsigned long long>(candidateA0));
    write_line(core::log::Level::info, line.data(), written);
    dump_component_mutations(g_condition75Target,
                             resolved.datum,
                             hash,
                             resolved.nameHash,
                             g_condition75Observation);
}

[[nodiscard]] bool sample_due(std::uint32_t observation,
                              std::atomic_uint64_t& lastLog,
                              std::uint64_t interval) noexcept {
    if (observation <= kInitialConditionSamples) {
        return true;
    }
    const std::uint64_t now = GetTickCount64();
    std::uint64_t previous = lastLog.load(std::memory_order_relaxed);
    if (now - previous < interval) {
        return false;
    }
    return lastLog.compare_exchange_strong(
        previous, now, std::memory_order_relaxed, std::memory_order_relaxed);
}

template <std::size_t Size>
[[nodiscard]] bool prefix_matches(const std::byte* target,
                                  const std::array<std::byte, Size>& prefix) noexcept {
    if (target == nullptr) {
        return false;
    }
    for (std::size_t index = 0; index < prefix.size(); ++index) {
        if (safe_read<std::byte>(target + index, std::byte{0}) != prefix[index]) {
            return false;
        }
    }
    return true;
}

struct DecodedPosition {
    std::array<std::uint32_t, 3> raw{};
    std::array<float, 3> value{};
    bool present{};
    bool valid{};
};

[[nodiscard]] float float_from_bits(std::uint32_t bits) noexcept {
    float value = 0.0F;
    static_assert(sizeof value == sizeof bits);
    std::memcpy(&value, &bits, sizeof value);
    return value;
}

/** Reads and unprotects one object position exactly as +1087F70 does. */
[[nodiscard]] DecodedPosition decode_position(std::uint32_t handle) noexcept {
    DecodedPosition result{};
    std::byte* const image = g_image.load(std::memory_order_acquire);
    if (image == nullptr || handle == std::numeric_limits<std::uint32_t>::max()) {
        return result;
    }
    std::byte* const table = safe_read<std::byte*>(image + kObjectTablePointerRva, nullptr);
    const std::uint32_t stride = safe_read<std::uint32_t>(image + kObjectTableStrideRva, 0U);
    if (table == nullptr || stride == 0U || stride > 0x10000U) {
        return result;
    }
    result.present = true;
    const std::byte* const encoded =
        table + static_cast<std::size_t>(handle & kDatumIndexMask) * stride
        + kObjectPositionOffset;
    for (std::size_t lane = 0; lane < result.raw.size(); ++lane) {
        result.raw[lane] = safe_read<std::uint32_t>(encoded + lane * sizeof(std::uint32_t), 0U);
    }

    const auto keyXy = reinterpret_cast<PositionKey>(image + kPositionKeyXyRva);
    const auto keyZw = reinterpret_cast<PositionKey>(image + kPositionKeyZwRva);
    std::uint32_t xy = 0U;
    std::uint32_t zw = 0U;
    bool keysRead = false;
    __try {
        xy = keyXy();
        zw = keyZw();
        keysRead = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        keysRead = false;
    }
    if (!keysRead) {
        return result;
    }

    for (std::size_t lane = 0; lane < result.value.size(); ++lane) {
        const std::uint32_t first =
            safe_read<std::uint32_t>(image + kPositionMaskOneRva
                                         + lane * sizeof(std::uint32_t),
                                     0U);
        const std::uint32_t second =
            safe_read<std::uint32_t>(image + kPositionMaskTwoRva
                                         + lane * sizeof(std::uint32_t),
                                     0U);
        const std::uint32_t key = lane < 2U ? xy : zw;
        const std::uint32_t bits = ((result.raw[lane] ^ key) & first & second)
                                   | ((~second) & 0x3F800000U);
        result.value[lane] = float_from_bits(bits);
    }
    result.valid = std::isfinite(result.value[0]) && std::isfinite(result.value[1])
                   && std::isfinite(result.value[2])
                   && std::fabs(result.value[0]) < 10000000.0F
                   && std::fabs(result.value[1]) < 10000000.0F
                   && std::fabs(result.value[2]) < 10000000.0F;
    return result;
}

struct IteratorObservation {
    std::uint32_t propertyType{};
    std::int32_t state{-2};
    bool measured{};
    bool empty{};
};

void clear_anchor_bank() noexcept {
    AcquireSRWLockExclusive(&g_anchorLock);
    g_anchors = {};
    g_playerOriginOffset = {};
    g_observedPlayerHandle = std::numeric_limits<std::uint32_t>::max();
    g_playerOriginOffsetValid = false;
    ReleaseSRWLockExclusive(&g_anchorLock);
}

/** Starts a fresh bank when a new destination produces a later burst of runner starts. */
void observe_runner_batch() noexcept {
    const std::uint64_t now = GetTickCount64();
    const std::uint64_t previous = g_lastRunnerStartTick.exchange(now, std::memory_order_acq_rel);
    if (previous == 0U || now - previous > kRunnerBatchGapMs) {
        clear_anchor_bank();
        clear_property_candidates();
    }
}

/** Remembers one native condition's fixed endpoint and the player/body origin offset it observed. */
void observe_anchor(std::uint32_t playerHandle,
                    std::uint32_t anchorHandle,
                    const DecodedPosition& playerPosition,
                    const DecodedPosition& anchorPosition,
                    const player::position::Snapshot& player,
                    float requiredMaximum) noexcept {
    if (!playerPosition.valid || !anchorPosition.valid
        || anchorHandle == std::numeric_limits<std::uint32_t>::max()) {
        return;
    }
    AcquireSRWLockExclusive(&g_anchorLock);
    ObservedAnchor* destination = nullptr;
    for (ObservedAnchor& anchor : g_anchors) {
        if (anchor.valid && anchor.handle == anchorHandle) {
            destination = &anchor;
            break;
        }
        if (!anchor.valid && destination == nullptr) {
            destination = &anchor;
        }
    }
    if (destination != nullptr) {
        destination->position = anchorPosition.value;
        destination->requiredMaximum = requiredMaximum;
        destination->handle = anchorHandle;
        destination->valid = true;
    }
    if (player.present) {
        for (std::size_t lane = 0; lane < g_playerOriginOffset.size(); ++lane) {
            g_playerOriginOffset[lane] = playerPosition.value[lane] - player.position[lane];
        }
        g_observedPlayerHandle = playerHandle;
        g_playerOriginOffsetValid = true;
    }
    ReleaseSRWLockExclusive(&g_anchorLock);
}

/** Repeats the condition's read-only iterator initialization in sampled calls. */
[[nodiscard]] IteratorObservation observe_property_iterator(std::uint32_t handle) noexcept {
    IteratorObservation result{};
    std::byte* const image = g_image.load(std::memory_order_acquire);
    if (image == nullptr || handle == std::numeric_limits<std::uint32_t>::max()) {
        return result;
    }
    const std::uint32_t* const propertyType =
        safe_read<const std::uint32_t*>(image + kPropertyTypePointerRva, nullptr);
    result.propertyType = safe_read<std::uint32_t>(propertyType, 0U);
    if (propertyType == nullptr || result.propertyType == 0U) {
        return result;
    }

    alignas(16) std::array<std::byte, kPropertyIteratorSize> iterator{};
    const auto initialize =
        reinterpret_cast<PropertyIteratorInit>(image + kPropertyIteratorInitRva);
    __try {
        initialize(iterator.data(), handle, result.propertyType, true);
        result.state = safe_read<std::int32_t>(
            iterator.data() + kPropertyIteratorStateOffset, -2);
        result.measured = true;
        result.empty = result.state == -1;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        result = {};
        result.state = -2;
    }
    return result;
}

/** Shared native name matcher, recorded only while the current thread is inside condition 0x75. */
__declspec(noinline) bool __fastcall property_name_match(const std::uintptr_t* property,
                                                          const std::uint32_t* nameHash) noexcept {
    const PropertyNameMatch original =
        g_propertyNameMatchOriginal.load(std::memory_order_acquire);
    const bool nativeResult = original != nullptr && original(property, nameHash);
    if (g_condition75Depth != 0U) {
        ++g_condition75MatchCalls;
        if (nativeResult) {
            ++g_condition75Matches;
        }
        record_property_candidate(property, nameHash, nativeResult);
    }
    return nativeResult;
}

__declspec(noinline) bool __fastcall condition_7a(const float* range,
                                                   const std::byte* context) noexcept {
    const Condition7A original = g_condition7AOriginal.load(std::memory_order_acquire);
    const bool nativeResult = original != nullptr && original(range, context);
    const std::uint32_t observation =
        g_condition7ACalls.fetch_add(1U, std::memory_order_relaxed) + 1U;
    if (!sample_due(observation, g_condition7ALastLog, kCondition7ASampleMs)) {
        return nativeResult;
    }

    const float minimum = safe_read<float>(range, 0.0F);
    const float maximum = safe_read<float>(range != nullptr ? range + 1 : nullptr, 0.0F);
    const std::uint32_t target = safe_read<std::uint32_t>(
        context != nullptr ? context + 0x04U : nullptr,
        std::numeric_limits<std::uint32_t>::max());
    const std::uint32_t candidate = safe_read<std::uint32_t>(
        context != nullptr ? context + 0x0CU : nullptr,
        std::numeric_limits<std::uint32_t>::max());
    const DecodedPosition targetPosition = decode_position(target);
    const DecodedPosition candidatePosition = decode_position(candidate);
    float distanceSquared = 0.0F;
    bool decoded = targetPosition.valid && candidatePosition.valid;
    if (decoded) {
        for (std::size_t lane = 0; lane < 3U; ++lane) {
            const float delta = candidatePosition.value[lane] - targetPosition.value[lane];
            distanceSquared += delta * delta;
        }
        decoded = std::isfinite(distanceSquared);
    }
    const float distance = decoded ? std::sqrt(distanceSquared) : 0.0F;
    const bool expected = decoded && distanceSquared >= minimum * minimum
                          && distanceSquared <= maximum * maximum;
    const bool consistent = decoded && expected == nativeResult;
    const player::position::Snapshot player = player::position::snapshot();
    observe_anchor(target,
                   candidate,
                   targetPosition,
                   candidatePosition,
                   player,
                   maximum);

    std::array<char, core::log::kLineCapacity> line{};
    const int written = std::snprintf(
        line.data(),
        line.size(),
        "ev=omega_probe stage=cond_7a n=%u result=%u target=%08X candidate=%08X "
        "min=%.3f max=%.3f decoded=%u consistent=%u d=%.3f d2=%.3f "
        "target_pos=%.3f,%.3f,%.3f candidate_pos=%.3f,%.3f,%.3f "
        "target_raw=%08X,%08X,%08X candidate_raw=%08X,%08X,%08X "
        "player_present=%u player_pos=%.3f,%.3f,%.3f",
        observation,
        nativeResult ? 1U : 0U,
        target,
        candidate,
        static_cast<double>(minimum),
        static_cast<double>(maximum),
        decoded ? 1U : 0U,
        consistent ? 1U : 0U,
        static_cast<double>(distance),
        static_cast<double>(distanceSquared),
        static_cast<double>(targetPosition.value[0]),
        static_cast<double>(targetPosition.value[1]),
        static_cast<double>(targetPosition.value[2]),
        static_cast<double>(candidatePosition.value[0]),
        static_cast<double>(candidatePosition.value[1]),
        static_cast<double>(candidatePosition.value[2]),
        targetPosition.raw[0],
        targetPosition.raw[1],
        targetPosition.raw[2],
        candidatePosition.raw[0],
        candidatePosition.raw[1],
        candidatePosition.raw[2],
        player.present ? 1U : 0U,
        static_cast<double>(player.position[0]),
        static_cast<double>(player.position[1]),
        static_cast<double>(player.position[2]));
    write_line(core::log::Level::info, line.data(), written);
    return nativeResult;
}

__declspec(noinline) bool __fastcall condition_75(const std::uint32_t* nameHash,
                                                   const std::byte* context) noexcept {
    const std::uint32_t observation =
        g_condition75Calls.fetch_add(1U, std::memory_order_relaxed) + 1U;
    const std::uint32_t hash = safe_read<std::uint32_t>(nameHash, 0U);
    const std::uint32_t target = safe_read<std::uint32_t>(
        context != nullptr ? context + 0x04U : nullptr,
        std::numeric_limits<std::uint32_t>::max());

    const bool outermost = g_condition75Depth == 0U;
    const std::uint32_t previousObservation = g_condition75Observation;
    const std::uint32_t previousTarget = g_condition75Target;
    const std::uint32_t previousMatchCalls = g_condition75MatchCalls;
    const std::uint32_t previousMatches = g_condition75Matches;
    if (outermost) {
        g_condition75Observation = observation;
        g_condition75Target = target;
        g_condition75MatchCalls = 0U;
        g_condition75Matches = 0U;
    }
    ++g_condition75Depth;
    const Condition75 original = g_condition75Original.load(std::memory_order_acquire);
    const bool nativeResult = original != nullptr && original(nameHash, context);
    --g_condition75Depth;
    const std::uint32_t matcherCalls = g_condition75MatchCalls;
    const std::uint32_t matcherMatches = g_condition75Matches;
    if (outermost) {
        g_condition75Observation = previousObservation;
        g_condition75Target = previousTarget;
        g_condition75MatchCalls = previousMatchCalls;
        g_condition75Matches = previousMatches;
    }
    if (!sample_due(observation, g_condition75LastLog, kCondition75SampleMs)) {
        return nativeResult;
    }

    const IteratorObservation iterator = observe_property_iterator(target);
    std::array<char, core::log::kLineCapacity> line{};
    const int written = std::snprintf(
        line.data(),
        line.size(),
        "ev=omega_probe stage=cond_75 n=%u result=%u name_hash=%08X context=%p "
        "target=%08X matcher_calls=%u matcher_true=%u iterator_measured=%u "
        "iterator_empty=%u iterator_state=%d property_type=%08X",
        observation,
        nativeResult ? 1U : 0U,
        hash,
        static_cast<const void*>(context),
        target,
        matcherCalls,
        matcherMatches,
        iterator.measured ? 1U : 0U,
        iterator.empty ? 1U : 0U,
        iterator.state,
        iterator.propertyType);
    write_line(core::log::Level::info, line.data(), written);
    return nativeResult;
}

__declspec(noinline) std::int32_t* __fastcall component_construct(
    std::int32_t* result,
    std::byte* descriptor) noexcept {
    const void* const caller = _ReturnAddress();
    const std::uint32_t owner = safe_read<std::uint32_t>(
        descriptor != nullptr ? descriptor + 0x30U : nullptr,
        std::numeric_limits<std::uint32_t>::max());
    std::array<std::uint64_t, 11> descriptorCopy{};
    if (descriptor != nullptr) {
        for (std::size_t index = 0; index < descriptorCopy.size(); ++index) {
            descriptorCopy[index] = safe_read<std::uint64_t>(
                descriptor + index * sizeof(std::uint64_t), 0U);
        }
    }

    const ComponentConstruct original =
        g_componentConstructOriginal.load(std::memory_order_acquire);
    std::int32_t* const returned = original != nullptr ? original(result, descriptor) : result;
    const std::uint32_t component = static_cast<std::uint32_t>(safe_read<std::int32_t>(
        returned != nullptr ? returned : result, -1));

    // The native descriptor is temporary; preserve the pre-call bytes in a local wrapper.
    record_component_mutation(ComponentMutationKind::construct,
                              owner,
                              component,
                              caller,
                              reinterpret_cast<const std::byte*>(descriptorCopy.data()));
    return returned;
}

__declspec(noinline) void __fastcall component_remove(std::uint32_t owner,
                                                       std::uint32_t component) noexcept {
    record_component_mutation(ComponentMutationKind::remove,
                              owner,
                              component,
                              _ReturnAddress());
    const ComponentRemove original = g_componentRemoveOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(owner, component);
    }
}

__declspec(noinline) void __fastcall component_attach(std::uint32_t owner,
                                                       std::uint32_t component) noexcept {
    record_component_mutation(ComponentMutationKind::attach,
                              owner,
                              component,
                              _ReturnAddress());
    const ComponentAttach original = g_componentAttachOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(owner, component);
    }
}

__declspec(noinline) void __fastcall runner_start(const std::byte* component) noexcept {
    const ScriptRunnerStart original = g_runnerStartOriginal.load(std::memory_order_acquire);
    observe_runner_batch();
    const std::uint32_t observation =
        g_runnerStarts.fetch_add(1U, std::memory_order_relaxed) + 1U;
    if (observation <= kInitialRunnerSamples) {
        std::array<char, core::log::kLineCapacity> line{};
        const int written = std::snprintf(
            line.data(),
            line.size(),
            "ev=omega_probe stage=runner_start n=%u component=%p datum=%08X sobject=%08X",
            observation,
            static_cast<const void*>(component),
            safe_read<std::uint32_t>(component, 0U),
            safe_read<std::uint32_t>(component != nullptr ? component + 0x2CU : nullptr,
                                     std::numeric_limits<std::uint32_t>::max()));
        write_line(core::log::Level::info, line.data(), written);
    }
    if (original != nullptr) {
        original(component);
    }
}

__declspec(noinline) std::uint64_t __fastcall runner_tick(const std::byte* component) noexcept {
    const ScriptRunnerTick original = g_runnerTickOriginal.load(std::memory_order_acquire);
    const std::uint64_t nativeResult = original != nullptr ? original(component) : 0U;
    const std::uint32_t observation =
        g_runnerTicks.fetch_add(1U, std::memory_order_relaxed) + 1U;
    if (observation <= 8U || observation % kTickSamplePeriod == 0U) {
        std::array<char, core::log::kLineCapacity> line{};
        const int written = std::snprintf(
            line.data(),
            line.size(),
            "ev=omega_probe stage=runner_tick n=%u result=%llu component=%p sobject=%08X "
            "cond_7a=%u cond_75=%u vm_spawn=%u",
            observation,
            static_cast<unsigned long long>(nativeResult),
            static_cast<const void*>(component),
            safe_read<std::uint32_t>(component != nullptr ? component + 0x2CU : nullptr,
                                     std::numeric_limits<std::uint32_t>::max()),
            g_condition7ACalls.load(std::memory_order_relaxed),
            g_condition75Calls.load(std::memory_order_relaxed),
            g_vmSpawns.load(std::memory_order_relaxed));
        write_line(core::log::Level::info, line.data(), written);
    }
    return nativeResult;
}

__declspec(noinline) void __fastcall vm_spawn(const std::byte* node,
                                               const std::byte* context) noexcept {
    const std::uint32_t observation =
        g_vmSpawns.fetch_add(1U, std::memory_order_relaxed) + 1U;
    if (observation <= kInitialRunnerSamples || observation % kVmSpawnSamplePeriod == 0U) {
        std::array<char, core::log::kLineCapacity> line{};
        const int written = std::snprintf(
            line.data(),
            line.size(),
            "ev=omega_probe stage=vm_spawn n=%u node=%p context=%p "
            "node0=%016llX node8=%016llX ctx0=%016llX ctx8=%016llX",
            observation,
            static_cast<const void*>(node),
            static_cast<const void*>(context),
            static_cast<unsigned long long>(safe_read<std::uint64_t>(node, 0U)),
            static_cast<unsigned long long>(safe_read<std::uint64_t>(
                node != nullptr ? node + 8U : nullptr, 0U)),
            static_cast<unsigned long long>(safe_read<std::uint64_t>(context, 0U)),
            static_cast<unsigned long long>(safe_read<std::uint64_t>(
                context != nullptr ? context + 8U : nullptr, 0U)));
        write_line(core::log::Level::info, line.data(), written);
    }
    const VmSpawn original = g_vmSpawnOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(node, context);
    }
}

void clear_runtime() noexcept {
    g_condition7AOriginal.store(nullptr, std::memory_order_release);
    g_condition75Original.store(nullptr, std::memory_order_release);
    g_propertyNameMatchOriginal.store(nullptr, std::memory_order_release);
    g_runnerStartOriginal.store(nullptr, std::memory_order_release);
    g_runnerTickOriginal.store(nullptr, std::memory_order_release);
    g_vmSpawnOriginal.store(nullptr, std::memory_order_release);
    g_componentConstructOriginal.store(nullptr, std::memory_order_release);
    g_componentAttachOriginal.store(nullptr, std::memory_order_release);
    g_componentRemoveOriginal.store(nullptr, std::memory_order_release);
    g_image.store(nullptr, std::memory_order_release);
    g_condition7ACalls.store(0U, std::memory_order_release);
    g_condition75Calls.store(0U, std::memory_order_release);
    g_runnerStarts.store(0U, std::memory_order_release);
    g_runnerTicks.store(0U, std::memory_order_release);
    g_vmSpawns.store(0U, std::memory_order_release);
    g_condition7ALastLog.store(0U, std::memory_order_release);
    g_condition75LastLog.store(0U, std::memory_order_release);
    g_lastRunnerStartTick.store(0U, std::memory_order_release);
    clear_anchor_bank();
    clear_property_candidates();
    clear_component_mutations();
}

} // namespace

bool activity_behavior_condition_probe_has_ownership() noexcept {
    // LEGACY_OWNER_SENTINEL_BEGIN(activity_behavior_condition, 9)
    const std::array hooks{
        legacy_owner_sentinel::HookOwnership{
            g_handles[0].attached,
            g_condition7AOriginal.load(std::memory_order_acquire) != nullptr},
        legacy_owner_sentinel::HookOwnership{
            g_handles[1].attached,
            g_condition75Original.load(std::memory_order_acquire) != nullptr},
        legacy_owner_sentinel::HookOwnership{
            g_handles[2].attached,
            g_propertyNameMatchOriginal.load(std::memory_order_acquire) != nullptr},
        legacy_owner_sentinel::HookOwnership{
            g_handles[3].attached,
            g_runnerStartOriginal.load(std::memory_order_acquire) != nullptr},
        legacy_owner_sentinel::HookOwnership{
            g_handles[4].attached,
            g_runnerTickOriginal.load(std::memory_order_acquire) != nullptr},
        legacy_owner_sentinel::HookOwnership{
            g_handles[5].attached,
            g_vmSpawnOriginal.load(std::memory_order_acquire) != nullptr},
        legacy_owner_sentinel::HookOwnership{
            g_handles[6].attached,
            g_componentConstructOriginal.load(std::memory_order_acquire) != nullptr},
        legacy_owner_sentinel::HookOwnership{
            g_handles[7].attached,
            g_componentAttachOriginal.load(std::memory_order_acquire) != nullptr},
        legacy_owner_sentinel::HookOwnership{
            g_handles[8].attached,
            g_componentRemoveOriginal.load(std::memory_order_acquire) != nullptr},
    };
    // LEGACY_OWNER_SENTINEL_END(activity_behavior_condition)
    static_assert(hooks.size() == kHookCount);
    // LEGACY_OWNER_CLAIMS_BEGIN(activity_behavior_condition, 2)
    const std::array claims{
        g_installed.load(std::memory_order_acquire),
        g_image.load(std::memory_order_acquire) != nullptr,
    };
    // LEGACY_OWNER_CLAIMS_END(activity_behavior_condition)
    return legacy_owner_sentinel::has_ownership(hooks, claims);
}

MissionTriggerSnapshot mission_trigger_snapshot() noexcept {
    MissionTriggerSnapshot result{};
    const player::position::Snapshot player = player::position::snapshot();
    if (!player.present) {
        return result;
    }

    std::array<ObservedAnchor, kMaximumObservedAnchors> anchors{};
    std::array<float, 3> originOffset{};
    std::uint32_t playerHandle = std::numeric_limits<std::uint32_t>::max();
    bool offsetValid = false;
    AcquireSRWLockShared(&g_anchorLock);
    anchors = g_anchors;
    originOffset = g_playerOriginOffset;
    playerHandle = g_observedPlayerHandle;
    offsetValid = g_playerOriginOffsetValid;
    ReleaseSRWLockShared(&g_anchorLock);

    result.playerPosition = player.position;
    if (offsetValid) {
        for (std::size_t lane = 0; lane < result.playerPosition.size(); ++lane) {
            result.playerPosition[lane] += originOffset[lane];
        }
    }
    result.playerHandle = playerHandle;
    float closestSquared = std::numeric_limits<float>::max();
    for (const ObservedAnchor& anchor : anchors) {
        if (!anchor.valid) {
            continue;
        }
        ++result.anchorCount;
        float distanceSquared = 0.0F;
        std::array<float, 3> delta{};
        for (std::size_t lane = 0; lane < delta.size(); ++lane) {
            delta[lane] = anchor.position[lane] - result.playerPosition[lane];
            distanceSquared += delta[lane] * delta[lane];
        }
        if (!std::isfinite(distanceSquared) || distanceSquared >= closestSquared) {
            continue;
        }
        closestSquared = distanceSquared;
        result.anchorHandle = anchor.handle;
        result.anchorPosition = anchor.position;
        result.delta = delta;
        result.distance = std::sqrt(distanceSquared);
        result.requiredMaximum = anchor.requiredMaximum;
        result.present = true;
    }
    return result;
}

bool install_activity_behavior_condition_probe() noexcept {
    if (g_installed.load(std::memory_order_acquire)) {
        return true;
    }
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    if (image == nullptr) {
        return false;
    }

    std::array<void*, kHookCount> targets{
        image + kCondition7ARva,
        image + kCondition75Rva,
        image + kPropertyNameMatchRva,
        image + kScriptRunnerStartRva,
        image + kScriptRunnerTickRva,
        image + kVmSpawnRva,
        image + kComponentConstructRva,
        image + kComponentAttachRva,
        image + kComponentRemoveRva,
    };
    if (!prefix_matches(static_cast<std::byte*>(targets[0]), kCondition7APrefix)
        || !prefix_matches(static_cast<std::byte*>(targets[1]), kCondition75Prefix)
        || !prefix_matches(static_cast<std::byte*>(targets[2]), kPropertyNameMatchPrefix)
        || !prefix_matches(static_cast<std::byte*>(targets[3]), kScriptRunnerStartPrefix)
        || !prefix_matches(static_cast<std::byte*>(targets[4]), kScriptRunnerTickPrefix)
        || !prefix_matches(static_cast<std::byte*>(targets[5]), kVmSpawnPrefix)
        || !prefix_matches(static_cast<std::byte*>(targets[6]), kComponentConstructPrefix)
        || !prefix_matches(static_cast<std::byte*>(targets[7]), kComponentAttachPrefix)
        || !prefix_matches(static_cast<std::byte*>(targets[8]), kComponentRemovePrefix)) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::warn,
                         "ev=omega_probe stage=install result=prefix_mismatch");
        return false;
    }

    const std::array<hooking::detour::Spec, kHookCount> specs{{
        {targets[0], reinterpret_cast<void*>(&condition_7a)},
        {targets[1], reinterpret_cast<void*>(&condition_75)},
        {targets[2], reinterpret_cast<void*>(&property_name_match)},
        {targets[3], reinterpret_cast<void*>(&runner_start)},
        {targets[4], reinterpret_cast<void*>(&runner_tick)},
        {targets[5], reinterpret_cast<void*>(&vm_spawn)},
        {targets[6], reinterpret_cast<void*>(&component_construct)},
        {targets[7], reinterpret_cast<void*>(&component_attach)},
        {targets[8], reinterpret_cast<void*>(&component_remove)},
    }};
    if (!hooking::detour::install(specs, g_handles)) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::warn,
                         "ev=omega_probe stage=install result=attach_fail");
        return false;
    }

    g_image.store(image, std::memory_order_release);
    g_condition7AOriginal.store(reinterpret_cast<Condition7A>(g_handles[0].original),
                                std::memory_order_release);
    g_condition75Original.store(reinterpret_cast<Condition75>(g_handles[1].original),
                                std::memory_order_release);
    g_propertyNameMatchOriginal.store(
        reinterpret_cast<PropertyNameMatch>(g_handles[2].original), std::memory_order_release);
    g_runnerStartOriginal.store(reinterpret_cast<ScriptRunnerStart>(g_handles[3].original),
                                std::memory_order_release);
    g_runnerTickOriginal.store(reinterpret_cast<ScriptRunnerTick>(g_handles[4].original),
                               std::memory_order_release);
    g_vmSpawnOriginal.store(reinterpret_cast<VmSpawn>(g_handles[5].original),
                            std::memory_order_release);
    g_componentConstructOriginal.store(
        reinterpret_cast<ComponentConstruct>(g_handles[6].original), std::memory_order_release);
    g_componentAttachOriginal.store(reinterpret_cast<ComponentAttach>(g_handles[7].original),
                                    std::memory_order_release);
    g_componentRemoveOriginal.store(reinterpret_cast<ComponentRemove>(g_handles[8].original),
                                    std::memory_order_release);
    g_installed.store(true, std::memory_order_release);
    core::log::write(
        core::log::Channel::client,
        core::log::Level::info,
        "ev=omega_probe stage=install result=ok mode=read_only "
        "sites=cond_7a,cond_75,cond_75_match,runner_start,runner_tick,vm_spawn,"
        "component_construct,component_attach,component_remove");
    return true;
}

void uninstall_activity_behavior_condition_probe() noexcept {
    if (!g_installed.exchange(false, std::memory_order_acq_rel)) {
        return;
    }
    (void)hooking::detour::uninstall(std::span(g_handles));
    clear_runtime();
    g_handles = {};
}

} // namespace sunrise::client::hooks::bootflow
