#include <Windows.h>
#include "omega_arc_charge_receipts.h"
#include <bcrypt.h>

#include <array>
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <intrin.h>
#include <limits>
#include <string_view>

#include "../../../core/filesystem/path.h"
#include "../../../core/logging/log.h"
#include "../../../core/settings/settings.h"
#include "../../../state/activity/forced/activity_forced_destination.h"
#include "../../../state/activity/omega_first_lair_runtime.h"
#include "../../../state/activity/omega_first_mancannon_authority.h"
#include "../../../state/activity/omega_crown_transit_authority.h"
#include "../../../state/activity/omega_presentation.h"
#include "../../../state/activity/runtime.h"
#include "../../diagnostics/module_range.h"
#include "../../hooking/call_gate.h"
#include "../../hooking/detour.h"
#include "internal.h"
#include "legacy_owner_sentinel.h"
#include "type31_objective_capture.h"
#include "type31_objective_capture_lifecycle.h"

#pragma comment(lib, "bcrypt.lib")

namespace sunrise::client::hooks::bootflow {
namespace {

/** Central unsigned bit reader used by the pinned Season of Arrivals authority decoder. */
constexpr std::uintptr_t kBitReaderRva = 0x3513B0U;
constexpr std::array<std::byte, 12> kBitReaderPrefix{
    std::byte{0x40}, std::byte{0x53}, std::byte{0x8B}, std::byte{0x59},
    std::byte{0x30}, std::byte{0xB8}, std::byte{0x40}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x2B}, std::byte{0xC3}};

/** Single-bit reader used beside the unsigned reader in every phase-two object loop. */
constexpr std::uintptr_t kBoolReaderRva = 0x350EF0U;
constexpr std::array<std::byte, 12> kBoolReaderPrefix{
    std::byte{0x48}, std::byte{0x83}, std::byte{0xEC}, std::byte{0x08},
    std::byte{0x44}, std::byte{0x8B}, std::byte{0x59}, std::byte{0x30},
    std::byte{0x4C}, std::byte{0x8B}, std::byte{0xC9}, std::byte{0x41}};

/** Wider reader and status accessor reported beside the two already-validated primitives. */
constexpr std::uintptr_t kWideReaderRva = 0x351070U;
constexpr std::uintptr_t kStreamStatusRva = 0x34E9B0U;

/** Generic BAP response decoder used to reveal wire-to-internal service mappings at runtime. */
constexpr std::uintptr_t kResponseHandleInternalRva = 0xE02280U;
constexpr std::array<std::byte, 12> kResponseHandleInternalPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x18}, std::byte{0x55}, std::byte{0x56}, std::byte{0x57},
    std::byte{0x41}, std::byte{0x54}, std::byte{0x41}, std::byte{0x55}};

/** Outer authority-schema apply that decides whether phase-two object blocks may decode. */
constexpr std::uintptr_t kAuthoritySchemaApplyRva = 0x4D92A0U;
constexpr std::array<std::byte, 12> kAuthoritySchemaApplyPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x20}, std::byte{0x55}, std::byte{0x56}, std::byte{0x57},
    std::byte{0xB8}, std::byte{0x60}, std::byte{0x00}, std::byte{0x02}};

/** One phase-two object decoder. This is the only function that can set object+0x6E dirty. */
constexpr std::uintptr_t kAuthorityGroupDecodeRva = 0x4D7380U;
constexpr std::array<std::byte, 12> kAuthorityGroupDecodePrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x08}, std::byte{0x48}, std::byte{0x89}, std::byte{0x74},
    std::byte{0x24}, std::byte{0x10}, std::byte{0x57}, std::byte{0x48}};
constexpr std::uintptr_t kAuthorityObjectDecodeRva = 0x4D7470U;
// The unwind record proves the entry starts with the REX-prefixed `40 57`; the stable sequence
// below therefore begins one byte into the function while the detour must attach at the entry.
constexpr std::uintptr_t kAuthorityObjectDecodeValidatedPrefixOffset = 1U;
constexpr std::array<std::byte, 12> kAuthorityObjectDecodePrefix{
    std::byte{0x57}, std::byte{0x41}, std::byte{0x55}, std::byte{0x41},
    std::byte{0x56}, std::byte{0x41}, std::byte{0x57}, std::byte{0xB8},
    std::byte{0x98}, std::byte{0x78}, std::byte{0x00}, std::byte{0x00}};
constexpr std::uint32_t kOmegaSpawnerRegistry = 0xD00142CFU;
constexpr std::size_t kObjectDecodeReadCapacity = 48U;

/** Validates every resolved phase-two authority object before the publish pass. */
constexpr std::uintptr_t kAuthorityValidationRva = 0x4D6530U;
constexpr std::array<std::byte, 12> kAuthorityValidationPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x18}, std::byte{0x48}, std::byte{0x89}, std::byte{0x7C},
    std::byte{0x24}, std::byte{0x20}, std::byte{0x55}, std::byte{0x48}};

/** The three post-validation passes that build, consume, and finalize the changed-object list. */
constexpr std::uintptr_t kAuthorityPublishCollectRva = 0x4D8AB0U;
constexpr std::array<std::byte, 12> kAuthorityPublishCollectPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x18}, std::byte{0x48}, std::byte{0x89}, std::byte{0x74},
    std::byte{0x24}, std::byte{0x20}, std::byte{0x57}, std::byte{0x48}};
constexpr std::uintptr_t kAuthorityPublishApplyRva = 0x4D6430U;
constexpr std::array<std::byte, 12> kAuthorityPublishApplyPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x6C}, std::byte{0x24},
    std::byte{0x20}, std::byte{0x57}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x40}, std::byte{0x33}, std::byte{0xFF}};
constexpr std::uintptr_t kAuthorityPublishFinalizeRva = 0x4DA190U;
constexpr std::array<std::byte, 12> kAuthorityPublishFinalizePrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x08}, std::byte{0x57}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x20}, std::byte{0x33}, std::byte{0xDB}};

/** Global native datum registry used by the publish pass to resolve changed-object handles. */
constexpr std::uintptr_t kObjectDatumRegistryRva = 0x2439C70U;
/** Read-only native helpers used by the same publish pass to locate a component runtime. */
constexpr std::uintptr_t kObjectComponentLookupRva = 0x9EB6D0U;
constexpr std::uintptr_t kObjectRuntimeResolveRva = 0x9FEC30U;
constexpr std::array<std::byte, 12> kObjectRuntimeResolvePrefix{
    std::byte{0x40}, std::byte{0x53}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x20}, std::byte{0x48}, std::byte{0x8B},
    std::byte{0xD9}, std::byte{0x0F}, std::byte{0xB7}, std::byte{0x49}};
constexpr std::uint32_t kActivityScriptSchema = 0x80809919U;
constexpr std::size_t kActivityScriptMaximumResolverCallers = 32U;

/**
 * Exact 0x8080992F visual-component consumers recovered from its native descriptor.
 * These are relative to Ghidra's actual 0x7FF618070000 program image base; do not derive them
 * from the older synthetic 0x7FF618000000 base, which shifts every target forward by 0x70000.
 */
constexpr std::uintptr_t kOmegaVisualApplyRva = 0x9F19F0U;
constexpr std::array<std::byte, 14> kOmegaVisualApplyPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x74}, std::byte{0x24},
    std::byte{0x20}, std::byte{0x41}, std::byte{0x56}, std::byte{0x48},
    std::byte{0x83}, std::byte{0xEC}, std::byte{0x30}, std::byte{0x44}, std::byte{0x8B}, std::byte{0x02}};
constexpr std::uintptr_t kOmegaVisualCreateRva = 0x9EFBC0U;
constexpr std::array<std::byte, 13> kOmegaVisualCreatePrefix{
    std::byte{0x40}, std::byte{0x55}, std::byte{0x53}, std::byte{0x57},
    std::byte{0x41}, std::byte{0x54}, std::byte{0x41}, std::byte{0x56},
    std::byte{0x48}, std::byte{0x8D}, std::byte{0xAC}, std::byte{0x24},
    std::byte{0x20}};
constexpr std::uintptr_t kOmegaVisualUpdateRva = 0x9EF680U;
constexpr std::array<std::byte, 13> kOmegaVisualUpdatePrefix{
    std::byte{0x40}, std::byte{0x55}, std::byte{0x56}, std::byte{0x57},
    std::byte{0x48}, std::byte{0x81}, std::byte{0xEC}, std::byte{0x80},
    std::byte{0x0C}, std::byte{0x00}, std::byte{0x00}, std::byte{0x48},
    std::byte{0x8B}};
// FUN_7ff618a62200. This is the actual authored-visual selector used by +9EFFC0 before
// +9EFBC0 receives a visual index. It returns -1 when no authored entry survives the native
// count, explicit-index, availability, mask, reference, and score gates.
constexpr std::uintptr_t kOmegaVisualSelectRva = 0x9F2200U;
constexpr std::array<std::byte, 14> kOmegaVisualSelectPrefix{
    std::byte{0x40}, std::byte{0x57}, std::byte{0x41}, std::byte{0x56},
    std::byte{0x41}, std::byte{0x57}, std::byte{0x48}, std::byte{0x81},
    std::byte{0xEC}, std::byte{0xC0}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x48}};
constexpr std::uint32_t kOmegaVisualTraceLimit = 256U;
constexpr std::size_t kOmegaVisualDecodedStateBytes = 0x170U;

/** Exact post-scene apply methods recovered from each class's native registration table. */
constexpr std::uintptr_t kOmegaGateApplyRva = 0x10699C0U;
constexpr std::uintptr_t kOmegaEngagementApplyRva = 0x9F1820U;
constexpr std::uintptr_t kOmegaMonitorApplyRva = 0xB20540U;
constexpr std::array<std::byte, 16> kOmegaPostSceneApplyPrefix{
    std::byte{0x40}, std::byte{0x53}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x30}, std::byte{0x44}, std::byte{0x8B},
    std::byte{0x02}, std::byte{0x48}, std::byte{0x8B}, std::byte{0xD9},
    std::byte{0x4C}, std::byte{0x8B}, std::byte{0x4A}, std::byte{0x08}};
// Portal stage is intentionally held while the player approaches the entrance. Leave enough
// observations for several minutes of one-second authority keepalives during manual testing.
constexpr std::uint32_t kOmegaPostSceneTraceLimit = 512U;
constexpr std::size_t kOmegaPostSceneMaximumStateBytes = 0xD0U;

/** Native accessors used by both object decode and changed-object collection to select authority. */
constexpr std::uintptr_t kCurrentAuthorityRootRva = 0x4294C0U;
constexpr std::array<std::byte, 7> kCurrentAuthorityRootPrefix{
    std::byte{0x48}, std::byte{0x8D}, std::byte{0x05}, std::byte{0x39},
    std::byte{0x48}, std::byte{0xB6}, std::byte{0x01}};
constexpr std::uintptr_t kCurrentAuthorityIdentityRva = 0x429BA0U;
constexpr std::array<std::byte, 12> kCurrentAuthorityIdentityPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x10}, std::byte{0x57}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x20}, std::byte{0x48}, std::byte{0x8B}};

/** Bit-reader stream offset incremented by every successful field read. */
constexpr std::size_t kStreamConsumedBitsOffset = 0x24;
/** Enough observations to cover three complete roster updates without flooding logs. */
constexpr std::uint32_t kMaximumObservations = 8192;
constexpr std::uint32_t kMaximumMessages = 3;
/** Return RVA immediately after the bubble prefix's first read, used to arm one live stream. */
constexpr std::uintptr_t kAuthorityPrefixReadCallerRva = 0x3CA015U;
/** The stable one-group roster's terminal loop bit; its parent stack reveals the outer gate. */
constexpr std::uint32_t kRosterTerminalBit = 521;
constexpr std::size_t kMaximumStackFrames = 24;
/** The pinned client image is smaller than this conservative upper bound. */
constexpr std::uintptr_t kMaximumGameImageSize = 0x9000000U;
/** Service 7 should be decoded immediately after the embedded server builds its response. */
constexpr ULONGLONG kServiceSevenCaptureMilliseconds = 10000U;
/** Bounded above several complete 1,024-bit payload decodes, including unrelated nearby reads. */
constexpr std::uint32_t kServiceSevenMaximumObservations = 4096U;
/** Every distinct immediate reader caller gets one live code dump and one native stack. */
constexpr std::size_t kServiceSevenMaximumCallers = 64U;
/** Capture a bounded set of generic handler calls so the wire-to-internal enum mapping is visible. */
constexpr std::uint32_t kServiceSevenMaximumResponses = 8U;
using BitReader = std::uint32_t(__fastcall*)(void* stream, std::uint32_t width) noexcept;
using BoolReader = bool(__fastcall*)(void* stream) noexcept;
using ResponseHandleInternal = std::uint64_t(__fastcall*)(std::uint32_t requestId,
                                                           std::uint32_t responseService,
                                                           void* descriptor,
                                                           void* primaryData,
                                                           std::uint32_t primarySize,
                                                           void* secondaryData,
                                                           std::uint32_t secondarySize,
                                                           void* responseMap,
                                                           std::uint32_t mapKey,
                                                           void* completion,
                                                           void* context) noexcept;
using AuthoritySchemaApply = bool(__fastcall*)(void* roster,
                                                void* stream,
                                                void* event) noexcept;
using AuthorityGroupDecode = std::uint64_t(__fastcall*)(void* roster,
                                                         void* stream) noexcept;
using AuthorityObjectDecode = std::uint64_t(__fastcall*)(void* roster,
                                                          void* stream) noexcept;
using AuthorityValidation = bool(__fastcall*)(void* roster) noexcept;
using AuthorityPublishPass = void(__fastcall*)(void* roster, void* changedObjects) noexcept;
using ObjectComponentLookup = std::uint64_t*(__fastcall*)(std::uint64_t* result,
                                                          std::int32_t component) noexcept;
using ObjectRuntimeResolve = void*(__fastcall*)(void* object, std::uint32_t component) noexcept;
using OmegaVisualApply = void(__fastcall*)(void* component, void* stateKey) noexcept;
using OmegaVisualCreate = std::uint16_t*(__fastcall*)(void* component,
                                                       std::uint16_t* result,
                                                       void* decodedState,
                                                       std::int32_t visualIndex) noexcept;
using OmegaVisualUpdate = void(__fastcall*)(void* component,
                                             std::int32_t* decodedState,
                                             void* effectState) noexcept;
using OmegaVisualSelect = std::int32_t(__fastcall*)(void* component) noexcept;
using OmegaPostSceneApply = void(__fastcall*)(void* component, void* stateKey) noexcept;
using CurrentAuthorityRoot = void*(__fastcall*)() noexcept;
using CurrentAuthorityIdentity = std::uint32_t*(__fastcall*)(void* root,
                                                              std::uint32_t* identity) noexcept;

[[nodiscard]] std::byte* resolve_changed_object(std::uint32_t datum) noexcept;
void report_response_dispatch_target(std::uint32_t generation,
                                     std::uint32_t observation,
                                     std::uint32_t requestId,
                                     std::uint32_t responseService,
                                     void* responseMap) noexcept;

hooking::detour::Handle g_bitHandle{};
hooking::detour::Handle g_boolHandle{};
hooking::detour::Handle g_responseHandleInternalHandle{};
hooking::detour::Handle g_authoritySchemaApplyHandle{};
hooking::detour::Handle g_authorityGroupDecodeHandle{};
hooking::detour::Handle g_authorityObjectDecodeHandle{};
hooking::detour::Handle g_authorityValidationHandle{};
hooking::detour::Handle g_authorityPublishCollectHandle{};
hooking::detour::Handle g_authorityPublishApplyHandle{};
hooking::detour::Handle g_authorityPublishFinalizeHandle{};
hooking::detour::Handle g_objectRuntimeResolveHandle{};
// This one target belongs to the narrow cannon receipt lifecycle, not the retired bundle.
hooking::detour::Handle g_omegaVisualApplyHandle{};
hooking::CallGate g_omegaCannonReceiptGate{};
std::array<std::atomic_uint64_t, 4> g_omegaCannonReceiptLoggedRuns{};
std::array<std::atomic_uint64_t, 7> g_omegaTransitReceiptLoggedRuns{};
/** Once per catalog core definition per run: why a 9F19F0 apply was not a preparation receipt. */
std::array<std::atomic_uint64_t, 11> g_omegaPreparationRejectLoggedRuns{};

[[nodiscard]] std::size_t omega_preparation_catalog_index(std::uint32_t definition) noexcept {
    for (const auto& cannon : state::activity::omega_first_mancannon::kCannons) {
        if (cannon.coreDefinition == definition) { return cannon.index; }
    }
    for (const auto& item : state::activity::omega_crown_transit::kSources) {
        if (item.definition == definition
            && item.preparationIndex != state::activity::omega_crown_transit::kNoPreparation) {
            return 4U + item.preparationIndex;
        }
    }
    return SIZE_MAX;
}

void report_omega_preparation_reject(void* component, std::uint64_t enteredRun,
                                     const char* reason) noexcept {
    std::array<std::byte, 16> prefix{};
    std::array<std::byte, 0x44> state{};
    SIZE_T got{};
    const auto* const bytes = static_cast<const std::byte*>(component);
    if (component == nullptr
        || !ReadProcessMemory(GetCurrentProcess(), bytes, prefix.data(), prefix.size(), &got) || got != prefix.size()
        || !ReadProcessMemory(GetCurrentProcess(), bytes + 0x180, state.data(), state.size(), &got) || got != state.size()) { return; }
    std::uint32_t definition{}, generation{}, kind{}, aux{};
    std::memcpy(&definition, prefix.data(), 4); std::memcpy(&kind, prefix.data() + 4, 4);
    std::memcpy(&generation, state.data(), 4); std::memcpy(&aux, state.data() + 0xC, 4);
    const auto index = omega_preparation_catalog_index(definition);
    if (index == SIZE_MAX || index >= g_omegaPreparationRejectLoggedRuns.size() || kind != 0x80809928U) { return; }
    if (g_omegaPreparationRejectLoggedRuns[index].exchange(enteredRun, std::memory_order_acq_rel)
        == enteredRun) { return; }
    std::array<char, 300> line{};
    const int written = std::snprintf(line.data(), line.size(),
        "ev=omega_crown_transit stage=reject reason=%s run=%llu mission_run=%llu definition=%08X core_index=%zu generation=%u candidate=%u active=%u override=%u aux=%08X flag30=%u mutation=observe_only",
        reason, static_cast<unsigned long long>(enteredRun),
        static_cast<unsigned long long>(state::activity::mission_run_generation()), definition, index,
        generation, static_cast<unsigned>(std::to_integer<std::uint8_t>(state[4])),
        static_cast<unsigned>(std::to_integer<std::uint8_t>(state[8])),
        static_cast<unsigned>(std::to_integer<std::uint8_t>(state[9])), aux,
        static_cast<unsigned>(std::to_integer<std::uint8_t>(state[0x30])));
    if (written > 0 && static_cast<std::size_t>(written) < line.size()) {
        core::log::write(core::log::Channel::client, core::log::Level::info,
                        {line.data(), static_cast<std::size_t>(written)});
    }
}
hooking::detour::Handle g_omegaVisualCreateHandle{};
hooking::detour::Handle g_omegaVisualUpdateHandle{};
hooking::detour::Handle g_omegaVisualSelectHandle{};
hooking::detour::Handle g_omegaGateApplyHandle{};
hooking::detour::Handle g_omegaEngagementApplyHandle{};
hooking::detour::Handle g_omegaPointApplyHandle{};
hooking::detour::Handle g_omegaMonitorApplyHandle{};
std::atomic<BitReader> g_original{nullptr};
std::atomic<BoolReader> g_boolOriginal{nullptr};
std::atomic<ResponseHandleInternal> g_responseHandleInternalOriginal{nullptr};
std::atomic<AuthoritySchemaApply> g_authoritySchemaApplyOriginal{nullptr};
std::atomic<AuthorityGroupDecode> g_authorityGroupDecodeOriginal{nullptr};
std::atomic<AuthorityObjectDecode> g_authorityObjectDecodeOriginal{nullptr};
std::atomic<AuthorityValidation> g_authorityValidationOriginal{nullptr};
std::atomic<AuthorityPublishPass> g_authorityPublishCollectOriginal{nullptr};
std::atomic<AuthorityPublishPass> g_authorityPublishApplyOriginal{nullptr};
std::atomic<AuthorityPublishPass> g_authorityPublishFinalizeOriginal{nullptr};
std::atomic<ObjectRuntimeResolve> g_objectRuntimeResolveOriginal{nullptr};
std::atomic<OmegaVisualApply> g_omegaVisualApplyOriginal{nullptr};
std::atomic<OmegaVisualCreate> g_omegaVisualCreateOriginal{nullptr};
std::atomic<OmegaVisualUpdate> g_omegaVisualUpdateOriginal{nullptr};
std::atomic<OmegaVisualSelect> g_omegaVisualSelectOriginal{nullptr};
std::atomic<OmegaPostSceneApply> g_omegaGateApplyOriginal{nullptr};
std::atomic<OmegaPostSceneApply> g_omegaEngagementApplyOriginal{nullptr};
std::atomic<type31_capture::NativeApply> g_omegaPointApplyOriginal{nullptr};
std::atomic<OmegaPostSceneApply> g_omegaMonitorApplyOriginal{nullptr};
hooking::CallGate g_type31PointCallGate{};
hooking::CallGate g_type31DrainCallGate{};
type31_capture::CaptureQueue g_type31CaptureQueue{};
type31_capture::OwnerLifecycle g_type31OwnerLifecycle{};
std::atomic_uint64_t g_type31CaptureEpochCounter{};
std::atomic_uint64_t g_type31CaptureEpoch{};
std::atomic_size_t g_type31MappedImageBytes{};
std::atomic_uint64_t g_type31ApplyPrefixHash{};
std::atomic_bool g_type31PackedIdentityValidated{};
std::atomic_flag g_type31DrainOwner = ATOMIC_FLAG_INIT;
type31_capture::QueueCounters g_type31ReportedQueueCounters{};
std::atomic_uint32_t g_observed{};
std::atomic_uint32_t g_schemaApplyObserved{};
std::atomic_uint32_t g_validationObserved{};
std::atomic_uint32_t g_publishObserved{};
std::atomic_bool g_changedObjectsReported{false};
std::atomic_bool g_activityScriptPostApplyReported{false};
std::array<std::atomic_uint64_t, 2> g_missionRuntimeHashes{};
std::array<std::atomic_bool, 2> g_missionRuntimeReported{};
std::atomic_uint64_t g_omegaSpawnerRuntimeHash{};
std::atomic_bool g_omegaSpawnerRuntimeReported{false};
std::atomic<std::uintptr_t> g_omegaSpawnerRuntime{};
std::atomic<std::uintptr_t> g_omegaSpawnerObject{};
std::atomic_uint32_t g_omegaTriggeredSchemaApplyObserved{};
std::atomic_uint32_t g_omegaTriggeredPublishObserved{};
std::atomic_uint32_t g_omegaSceneRuntimeObserved{};
std::atomic_uint32_t g_omegaRuntimeFollowupObserved{};
constexpr std::size_t kOmegaRuntimeTrackCapacity = 32U;
constexpr std::size_t kOmegaRuntimeMaximumBytes = kOmegaVisualDecodedStateBytes;

struct OmegaRuntimeTrack final {
    std::uintptr_t object{};
    std::uintptr_t runtime{};
    std::uint32_t datum{};
    std::uint32_t schema{};
    std::size_t bytes{};
    std::uint64_t hash{};
};

SRWLOCK g_omegaRuntimeTrackLock = SRWLOCK_INIT;
std::array<OmegaRuntimeTrack, kOmegaRuntimeTrackCapacity> g_omegaRuntimeTracks{};
std::size_t g_omegaRuntimeTrackCount{};
std::atomic_uint32_t g_omegaGroupDecodeObserved{};
std::atomic_uint32_t g_omegaObjectDecodeObserved{};
std::atomic_uint32_t g_omegaObjectStateObserved{};
std::atomic_uint32_t g_omegaApplyStateObserved{};
std::atomic_uint32_t g_omegaVisualApplyObserved{};
std::atomic_uint32_t g_omegaVisualCreateObserved{};
std::atomic_uint32_t g_omegaVisualUpdateObserved{};
std::atomic_uint32_t g_omegaVisualSelectObserved{};
std::atomic_uint32_t g_omegaPostSceneApplyObserved{};
std::atomic_bool g_serviceSevenCaptureActive{false};
std::atomic_bool g_serviceSevenReadCaptureActive{false};
std::atomic_uint32_t g_serviceSevenGeneration{};
std::atomic_uint32_t g_serviceSevenObserved{};
std::atomic_uint32_t g_serviceSevenResponsesObserved{};
std::atomic_bool g_serviceSevenReadLimitReported{false};
std::atomic<ULONGLONG> g_serviceSevenArmedAt{};
std::array<std::atomic<std::uintptr_t>, kServiceSevenMaximumCallers>
    g_serviceSevenCallerRvas{};
std::array<std::atomic<std::uintptr_t>, kActivityScriptMaximumResolverCallers>
    g_activityScriptResolverCallerRvas{};
thread_local void* g_authorityStream{};
thread_local std::uint32_t g_authorityMessage{};
thread_local std::uint32_t g_omegaTriggeredSchemaApply{};

struct ObjectDecodeRead final {
    std::uint32_t width{};
    std::uint32_t value{};
    std::uint32_t before{};
    std::uint32_t after{};
    bool boolean{};
};

thread_local bool g_objectDecodeActive{};
thread_local bool g_objectDecodeFirst32Set{};
thread_local bool g_objectDecodeSawOmega{};
thread_local std::uint32_t g_objectDecodeFirst32{};
thread_local std::uint32_t g_objectDecodeReadCount{};
thread_local std::array<ObjectDecodeRead, kObjectDecodeReadCapacity> g_objectDecodeReads{};
thread_local bool g_groupDecodeActive{};
thread_local bool g_groupDecodeSawOmega{};
thread_local std::uint32_t g_groupDecodeReadCount{};
thread_local std::uint32_t g_groupDecodeChildObjects{};
thread_local std::array<ObjectDecodeRead, kObjectDecodeReadCapacity> g_groupDecodeReads{};
thread_local std::uint32_t g_omegaVisualTraceDepth{};

/** Resolves and validates the exact reader without depending on encrypted on-disk code. */
[[nodiscard]] std::byte* bit_reader_target() noexcept {
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    if (image == nullptr) {
        return nullptr;
    }
    std::byte* const target = image + kBitReaderRva;
    for (std::size_t index = 0; index < kBitReaderPrefix.size(); ++index) {
        if (target[index] != kBitReaderPrefix[index]) {
            return nullptr;
        }
    }
    return target;
}

/** Resolves and validates the matching single-bit reader. */
[[nodiscard]] std::byte* bool_reader_target() noexcept {
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    if (image == nullptr) {
        return nullptr;
    }
    std::byte* const target = image + kBoolReaderRva;
    for (std::size_t index = 0; index < kBoolReaderPrefix.size(); ++index) {
        if (target[index] != kBoolReaderPrefix[index]) {
            return nullptr;
        }
    }
    return target;
}

/** Resolves and validates one exact activity-object function. */
template <std::size_t Size>
[[nodiscard]] std::byte* object_target(
    std::uintptr_t rva,
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

struct AuthorityFilterSnapshot {
    std::uint32_t owner{0xFFFFFFFFU};
    std::uint32_t current{0xFFFFFFFFU};
    std::uint8_t initialized{};
    std::uint8_t dirty{};
    bool objectReadable{};
    bool accessorReadable{};
};

constexpr std::size_t kOmegaSpawnerRuntimeBytes = 0xC4U;

/** Immutable copy of the spawner state at one native decode/apply boundary. */
struct OmegaSpawnerStateSnapshot final {
    std::uintptr_t address{};
    std::array<std::byte, kOmegaSpawnerRuntimeBytes> bytes{};
    bool readable{};
};

/** Copies the complete spawner state without retaining a native pointer across the hooked call. */
[[nodiscard]] OmegaSpawnerStateSnapshot capture_omega_spawner_state() noexcept {
    OmegaSpawnerStateSnapshot snapshot{};
    snapshot.address = g_omegaSpawnerRuntime.load(std::memory_order_acquire);
    __try {
        if (snapshot.address != 0U) {
            std::memcpy(snapshot.bytes.data(),
                        reinterpret_cast<const void*>(snapshot.address),
                        snapshot.bytes.size());
            snapshot.readable = true;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        snapshot.readable = false;
    }
    return snapshot;
}

[[nodiscard]] std::uint32_t omega_state_u32(const OmegaSpawnerStateSnapshot& snapshot,
                                            std::size_t offset) noexcept {
    std::uint32_t value = 0U;
    if (snapshot.readable && offset + sizeof value <= snapshot.bytes.size()) {
        std::memcpy(&value, snapshot.bytes.data() + offset, sizeof value);
    }
    return value;
}

[[nodiscard]] std::uint16_t omega_state_u16(const OmegaSpawnerStateSnapshot& snapshot,
                                            std::size_t offset) noexcept {
    std::uint16_t value = 0U;
    if (snapshot.readable && offset + sizeof value <= snapshot.bytes.size()) {
        std::memcpy(&value, snapshot.bytes.data() + offset, sizeof value);
    }
    return value;
}

[[nodiscard]] std::uint8_t omega_state_u8(const OmegaSpawnerStateSnapshot& snapshot,
                                          std::size_t offset) noexcept {
    return snapshot.readable && offset < snapshot.bytes.size()
               ? std::to_integer<std::uint8_t>(snapshot.bytes[offset])
               : 0U;
}

/** Logs every state byte plus the spawn-driving fields at one exact native boundary. */
void report_omega_spawner_state_snapshot(const char* phase,
                                         const char* moment,
                                         std::uint32_t observation,
                                         const OmegaSpawnerStateSnapshot& snapshot) noexcept {
    if (phase == nullptr || moment == nullptr || observation == 0U || observation > 16U) {
        return;
    }
    std::array<char, core::log::kLineCapacity> line{};
    int written = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=omega_spawner_state_snapshot phase=%s moment=%s n=%u runtime=%p readable=%s requested=%u counts=%u,%u,%u,%u,%u,%u generation=%u target=%08X/%u/%u squad=%08X/%u/%u target_gen=%u active=%u mode=%u name=%08X hex=",
        phase,
        moment,
        observation,
        reinterpret_cast<void*>(snapshot.address),
        snapshot.readable ? "yes" : "no",
        omega_state_u32(snapshot, 0x2C),
        omega_state_u32(snapshot, 0x30),
        omega_state_u32(snapshot, 0x34),
        omega_state_u32(snapshot, 0x38),
        omega_state_u32(snapshot, 0x3C),
        omega_state_u32(snapshot, 0x40),
        omega_state_u32(snapshot, 0x44),
        omega_state_u32(snapshot, 0x7C),
        omega_state_u32(snapshot, 0x90),
        static_cast<unsigned>(omega_state_u8(snapshot, 0x94)),
        static_cast<unsigned>(omega_state_u16(snapshot, 0x96)),
        omega_state_u32(snapshot, 0x98),
        static_cast<unsigned>(omega_state_u8(snapshot, 0x9C)),
        static_cast<unsigned>(omega_state_u16(snapshot, 0x9E)),
        omega_state_u32(snapshot, 0xB8),
        static_cast<unsigned>(omega_state_u8(snapshot, 0xBC)),
        static_cast<unsigned>(omega_state_u8(snapshot, 0xBD)),
        omega_state_u32(snapshot, 0xC0));
    if (written <= 0 || static_cast<std::size_t>(written) >= line.size()) {
        return;
    }
    constexpr char kHex[] = "0123456789ABCDEF";
    for (std::size_t index = 0;
         index < snapshot.bytes.size() && static_cast<std::size_t>(written) + 2U < line.size();
         ++index) {
        const unsigned value = std::to_integer<unsigned char>(snapshot.bytes[index]);
        line[static_cast<std::size_t>(written++)] = kHex[value >> 4U];
        line[static_cast<std::size_t>(written++)] = kHex[value & 0xFU];
    }
    core::log::write(core::log::Channel::client,
                     core::log::Level::info,
                     {line.data(), static_cast<std::size_t>(written)});
}

/** Reads the exact two values the native object decoder compares before consuming an auth body. */
[[nodiscard]] AuthorityFilterSnapshot snapshot_authority_filter(
    const std::byte* object) noexcept {
    AuthorityFilterSnapshot snapshot{};
    __try {
        if (object != nullptr) {
            std::memcpy(&snapshot.owner, object + 0x68U, sizeof snapshot.owner);
            std::memcpy(&snapshot.initialized,
                        object + 0x18U,
                        sizeof snapshot.initialized);
            std::memcpy(&snapshot.dirty, object + 0x6EU, sizeof snapshot.dirty);
            snapshot.objectReadable = true;
        }

        auto* const rootTarget = object_target(kCurrentAuthorityRootRva,
                                               kCurrentAuthorityRootPrefix);
        auto* const identityTarget = object_target(kCurrentAuthorityIdentityRva,
                                                   kCurrentAuthorityIdentityPrefix);
        if (rootTarget != nullptr && identityTarget != nullptr) {
            const auto rootAccessor = reinterpret_cast<CurrentAuthorityRoot>(rootTarget);
            const auto identityAccessor =
                reinterpret_cast<CurrentAuthorityIdentity>(identityTarget);
            void* const root = rootAccessor();
            std::uint32_t current = 0xFFFFFFFFU;
            if (root != nullptr && identityAccessor(root, &current) != nullptr) {
                snapshot.current = current;
                snapshot.accessorReadable = true;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        snapshot.accessorReadable = false;
    }
    return snapshot;
}

/** Saves one validated, runtime-decrypted publish function for offline disassembly. */
void dump_publish_function(const wchar_t* phase, const std::byte* target) noexcept {
    if (phase == nullptr || target == nullptr) {
        return;
    }
    constexpr std::uintptr_t kCaptureSize = 0x400U;
    const auto begin = reinterpret_cast<std::uintptr_t>(target);
    const auto end = begin + kCaptureSize;
    const auto image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    if (image == 0 || begin < image || end <= begin) {
        return;
    }

    const HMODULE sunrise = GetModuleHandleW(L"steam_api64.dll");
    core::path::Buffer path{};
    if (sunrise == nullptr || !core::path::artifact_directory(sunrise, path)
        || !core::path::append(path, L"\\analysis")) {
        return;
    }
    if (CreateDirectoryW(path.chars.data(), nullptr) == FALSE
        && GetLastError() != ERROR_ALREADY_EXISTS) {
        return;
    }
    std::array<wchar_t, 144> filename{};
    const int length = std::swprintf(
        filename.data(),
        filename.size(),
        L"\\activity_code.%ls.rva_%08llX.begin_%08llX.end_%08llX.bin",
        phase,
        static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(target) - image),
        static_cast<unsigned long long>(begin - image),
        static_cast<unsigned long long>(end - image));
    if (length <= 0 || !core::path::append(path, filename.data())) {
        return;
    }
    const HANDLE file = CreateFileW(path.chars.data(),
                                    GENERIC_WRITE,
                                    FILE_SHARE_READ,
                                    nullptr,
                                    CREATE_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL,
                                    nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    const DWORD size = static_cast<DWORD>(end - begin);
    DWORD written = 0;
    const bool complete = WriteFile(file,
                                    reinterpret_cast<const void*>(begin),
                                    size,
                                    &written,
                                    nullptr)
                              != FALSE
                          && written == size && FlushFileBuffers(file) != FALSE;
    (void)CloseHandle(file);
    std::array<char, 224> line{};
    const int lineLength = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_authority_code phase=%ls rva=0x%llX begin=0x%llX end=0x%llX bytes=%lu result=%s",
        phase,
        static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(target) - image),
        static_cast<unsigned long long>(begin - image),
        static_cast<unsigned long long>(end - image),
        static_cast<unsigned long>(written),
        complete ? "ok" : "write");
    if (lineLength > 0) {
        core::log::write(core::log::Channel::client,
                         complete ? core::log::Level::info : core::log::Level::warn,
                         {line.data(), static_cast<std::size_t>(lineLength)});
    }
}

/**
 * Reports one pinned Omega visual consumer before any detour is attempted.
 *
 * The previous installer resolved all three consumers through object_target() and collapsed a
 * prefix mismatch into one generic `reason=target` line. That made a single stale RVA or prologue
 * disable every observer while hiding which consumer disagreed with the live image. Copy and dump
 * the live bytes first, report the exact mismatch independently, and return a target only when its
 * complete pinned prefix still matches. This remains fail-closed: diagnostic bytes are captured
 * from mismatched targets, but a detour is never installed on one.
 */
template <std::size_t Size>
[[nodiscard]] std::byte* omega_visual_target(
    const char* name,
    const wchar_t* dumpPhase,
    std::uintptr_t rva,
    const std::array<std::byte, Size>& expected) noexcept {
    constexpr std::size_t kReportedBytes = 24U;
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    if (name == nullptr || dumpPhase == nullptr || image == nullptr) {
        return nullptr;
    }

    std::byte* const target = image + rva;
    std::array<std::byte, kReportedBytes> actual{};
    bool readable = false;
    __try {
        std::memcpy(actual.data(), target, actual.size());
        readable = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        readable = false;
    }

    std::size_t mismatch = Size;
    if (readable) {
        for (std::size_t index = 0; index < Size; ++index) {
            if (actual[index] != expected[index]) {
                mismatch = index;
                break;
            }
        }
        // Keep a runtime-decrypted artifact even when validation fails. The target is inside the
        // loaded image and the bounded read above proved the entry page is readable.
        dump_publish_function(dumpPhase, target);
    }

    std::array<char, core::log::kLineCapacity> line{};
    int prefix = std::snprintf(
        line.data(),
        line.size(),
        "ev=omega_visual_trace stage=target name=%s rva=0x%llX address=%p readable=%s "
        "prefix=%s mismatch=%d expected=",
        name,
        static_cast<unsigned long long>(rva),
        target,
        readable ? "yes" : "no",
        readable && mismatch == Size ? "match" : "mismatch",
        readable && mismatch != Size ? static_cast<int>(mismatch) : -1);
    if (prefix <= 0) {
        return readable && mismatch == Size ? target : nullptr;
    }

    constexpr char kHex[] = "0123456789ABCDEF";
    std::size_t used = (std::min)(static_cast<std::size_t>(prefix), line.size() - 1U);
    for (std::byte value : expected) {
        if (used + 2U >= line.size()) {
            break;
        }
        const std::uint8_t byte = std::to_integer<std::uint8_t>(value);
        line[used++] = kHex[byte >> 4U];
        line[used++] = kHex[byte & 0x0FU];
    }
    constexpr std::string_view kActualLabel{" actual="};
    if (used + kActualLabel.size() < line.size()) {
        std::memcpy(line.data() + used, kActualLabel.data(), kActualLabel.size());
        used += kActualLabel.size();
    }
    if (readable) {
        for (std::byte value : actual) {
            if (used + 2U >= line.size()) {
                break;
            }
            const std::uint8_t byte = std::to_integer<std::uint8_t>(value);
            line[used++] = kHex[byte >> 4U];
            line[used++] = kHex[byte & 0x0FU];
        }
    }
    constexpr std::string_view kMutation{" mutation=observe_only"};
    if (used + kMutation.size() < line.size()) {
        std::memcpy(line.data() + used, kMutation.data(), kMutation.size());
        used += kMutation.size();
    }
    line[(std::min)(used, line.size() - 1U)] = '\0';
    core::log::write(core::log::Channel::client,
                     readable && mismatch == Size ? core::log::Level::info
                                                  : core::log::Level::warn,
                     {line.data(), (std::min)(used, line.size() - 1U)});
    return readable && mismatch == Size ? target : nullptr;
}

/** Saves one small live object buffer without retaining its pointer after the native call. */
void dump_object_buffer(const wchar_t* phase,
                        const void* source,
                        std::size_t size) noexcept {
    constexpr std::size_t kMaximumCaptureBytes = 0x400U;
    if (phase == nullptr || source == nullptr || size == 0U
        || size > kMaximumCaptureBytes) {
        return;
    }

    std::array<std::byte, kMaximumCaptureBytes> snapshot{};
    __try {
        std::memcpy(snapshot.data(), source, size);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return;
    }

    const HMODULE sunrise = GetModuleHandleW(L"steam_api64.dll");
    core::path::Buffer path{};
    if (sunrise == nullptr || !core::path::artifact_directory(sunrise, path)
        || !core::path::append(path, L"\\analysis")) {
        return;
    }
    if (CreateDirectoryW(path.chars.data(), nullptr) == FALSE
        && GetLastError() != ERROR_ALREADY_EXISTS) {
        return;
    }

    std::array<wchar_t, 144> filename{};
    const int length = std::swprintf(filename.data(),
                                    filename.size(),
                                    L"\\activity_runtime.%ls.bin",
                                    phase);
    if (length <= 0 || !core::path::append(path, filename.data())) {
        return;
    }

    const HANDLE file = CreateFileW(path.chars.data(),
                                    GENERIC_WRITE,
                                    FILE_SHARE_READ,
                                    nullptr,
                                    CREATE_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL,
                                    nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD written = 0;
    const bool complete = WriteFile(file,
                                    snapshot.data(),
                                    static_cast<DWORD>(size),
                                    &written,
                                    nullptr)
                              != FALSE
                          && written == size && FlushFileBuffers(file) != FALSE;
    (void)CloseHandle(file);

    std::array<char, 256> line{};
    const int lineLength = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_runtime_buffer phase=%ls bytes=%lu result=%s mutation=observe_only",
        phase,
        static_cast<unsigned long>(written),
        complete ? "ok" : "write");
    if (lineLength > 0) {
        core::log::write(core::log::Channel::client,
                         complete ? core::log::Level::info : core::log::Level::warn,
                         {line.data(), static_cast<std::size_t>(lineLength)});
    }
}

/** @return True while the operator has selected a Red War or Omega mission under test. */
[[nodiscard]] bool opening_is_forced(state::activity::forced::ForcedDestination& forced,
                                      std::string_view& package) noexcept {
    state::activity::forced::snapshot(forced);
    package = std::string_view(forced.packageName.data(), forced.packageNameLength);
    return state::activity::forced::override_active()
           && (package == "cine_110_twr" || package == "mission_towerfall"
               || package == "mission_scot");
}

/** Reads the decoder's monotonic bit position while the stream is known to be live. */
[[nodiscard]] std::uint32_t consumed_bits(const void* stream) noexcept {
    std::uint32_t value = 0;
    if (stream != nullptr) {
        const auto* const bytes = static_cast<const std::byte*>(stream);
        std::memcpy(&value, bytes + kStreamConsumedBitsOffset, sizeof value);
    }
    return value;
}

/** Retains the bounded field sequence for the one object decoder active on this thread. */
void note_object_decode_read(bool boolean,
                             std::uint32_t width,
                             std::uint32_t value,
                             std::uint32_t before,
                             std::uint32_t after) noexcept {
    if (!g_objectDecodeActive) {
        return;
    }
    if (!boolean && width == 32U && !g_objectDecodeFirst32Set) {
        g_objectDecodeFirst32 = value;
        g_objectDecodeFirst32Set = true;
    }
    if (!boolean && width == 32U && value == kOmegaSpawnerRegistry) {
        g_objectDecodeSawOmega = true;
    }
    if (g_objectDecodeReadCount < g_objectDecodeReads.size()) {
        g_objectDecodeReads[g_objectDecodeReadCount++] = {width, value, before, after, boolean};
    }
}

/** Retains the bounded field sequence for the parent phase-two group decoder. */
void note_group_decode_read(bool boolean,
                            std::uint32_t width,
                            std::uint32_t value,
                            std::uint32_t before,
                            std::uint32_t after) noexcept {
    if (!g_groupDecodeActive) {
        return;
    }
    if (!boolean && width == 32U && value == kOmegaSpawnerRegistry) {
        g_groupDecodeSawOmega = true;
    }
    if (g_groupDecodeReadCount < g_groupDecodeReads.size()) {
        g_groupDecodeReads[g_groupDecodeReadCount++] = {width, value, before, after, boolean};
    }
}

/** Emits one bounded field record from inside the native authority decoder. */
void report(std::uint32_t observation,
            std::uint32_t message,
            const char* kind,
            std::uintptr_t callerRva,
            void* stream,
            std::uint32_t width,
            std::uint32_t value,
            std::uint32_t before,
            std::uint32_t after,
            std::string_view package) noexcept {
    std::array<char, core::log::kLineCapacity> line{};
    const int written = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_authority_stream_probe n=%u message=%u kind=%s caller_rva=0x%llX stream=%p width=%u value=0x%08X bit_before=%u bit_after=%u forced=%.*s",
        observation,
        message,
        kind,
        static_cast<unsigned long long>(callerRva),
        stream,
        width,
        value,
        before,
        after,
        static_cast<int>(package.size()),
        package.data());
    if (written > 0) {
        const std::size_t length =
            static_cast<std::size_t>(written) < line.size()
                ? static_cast<std::size_t>(written)
                : line.size() - 1;
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), length});
    }
}

/** Emits only game-image frames for the one terminal roster read. */
void report_terminal_stack(std::uint32_t message, std::string_view package) noexcept {
    std::array<void*, kMaximumStackFrames> frames{};
    const USHORT captured = RtlCaptureStackBackTrace(
        0, static_cast<ULONG>(frames.size()), frames.data(), nullptr);
    const auto image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    if (image == 0) {
        return;
    }

    std::array<char, core::log::kLineCapacity> line{};
    int used = std::snprintf(line.data(),
                             line.size(),
                             "ev=bootflow stage=activity_roster_terminal_stack message=%u bit=%u forced=%.*s frames=",
                             message,
                             kRosterTerminalBit,
                             static_cast<int>(package.size()),
                             package.data());
    if (used <= 0) {
        return;
    }
    for (USHORT index = 0; index < captured && static_cast<std::size_t>(used) < line.size();
         ++index) {
        const auto address = reinterpret_cast<std::uintptr_t>(frames[index]);
        if (address < image || address - image >= kMaximumGameImageSize) {
            continue;
        }
        const int appended = std::snprintf(
            line.data() + used,
            line.size() - static_cast<std::size_t>(used),
            "%s0x%llX",
            line[static_cast<std::size_t>(used) - 1] == '=' ? "" : ",",
            static_cast<unsigned long long>(address - image));
        if (appended <= 0) {
            break;
        }
        used += appended;
    }
    const std::size_t length =
        static_cast<std::size_t>(used) < line.size() ? static_cast<std::size_t>(used)
                                                     : line.size() - 1;
    core::log::write(core::log::Channel::client,
                     core::log::Level::info,
                     {line.data(), length});
}

/** Records the native chain that first resolves Homecoming's activity-script runtime. */
void report_activity_script_resolver_stack(std::uint32_t observation,
                                           std::uintptr_t callerRva) noexcept {
    std::array<void*, kMaximumStackFrames> frames{};
    const USHORT captured = RtlCaptureStackBackTrace(
        0, static_cast<ULONG>(frames.size()), frames.data(), nullptr);
    const auto image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    if (image == 0U) {
        return;
    }

    std::array<char, core::log::kLineCapacity> line{};
    int used = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_runtime_resolve_stack n=%u caller_rva=0x%llX frames=",
        observation,
        static_cast<unsigned long long>(callerRva));
    if (used <= 0) {
        return;
    }
    for (USHORT index = 0; index < captured && static_cast<std::size_t>(used) < line.size();
         ++index) {
        const auto address = reinterpret_cast<std::uintptr_t>(frames[index]);
        if (address < image || address - image >= kMaximumGameImageSize) {
            continue;
        }
        const int appended = std::snprintf(
            line.data() + used,
            line.size() - static_cast<std::size_t>(used),
            "%s0x%llX",
            line[static_cast<std::size_t>(used) - 1U] == '=' ? "" : ",",
            static_cast<unsigned long long>(address - image));
        if (appended <= 0) {
            break;
        }
        used += appended;
    }
    core::log::write(core::log::Channel::client,
                     core::log::Level::info,
                     {line.data(),
                      static_cast<std::size_t>(used) < line.size()
                          ? static_cast<std::size_t>(used)
                          : line.size() - 1U});
}

/** Emits one record for each distinct game caller that resolves schema 0x80809919. */
void capture_activity_script_resolver(void* object,
                                      std::uint32_t component,
                                      void* runtime,
                                      std::uintptr_t callerRva) noexcept {
    if (callerRva == 0U || callerRva >= kMaximumGameImageSize || runtime == nullptr) {
        return;
    }

    for (std::size_t index = 0; index < g_activityScriptResolverCallerRvas.size(); ++index) {
        auto& known = g_activityScriptResolverCallerRvas[index];
        const std::uintptr_t present = known.load(std::memory_order_acquire);
        if (present == callerRva) {
            return;
        }
        if (present != 0U) {
            continue;
        }
        std::uintptr_t empty = 0U;
        if (!known.compare_exchange_strong(empty,
                                           callerRva,
                                           std::memory_order_acq_rel,
                                           std::memory_order_acquire)) {
            continue;
        }

        std::array<std::uint64_t, 8> words{};
        std::uint8_t activityScriptFlag = 0U;
        std::int32_t activityScriptState = 0;
        bool readable = false;
        __try {
            std::memcpy(words.data(), runtime, sizeof words);
            std::memcpy(&activityScriptFlag,
                        static_cast<const std::byte*>(runtime) + 0x38U,
                        sizeof activityScriptFlag);
            std::memcpy(&activityScriptState,
                        static_cast<const std::byte*>(runtime) + 0x3CU,
                        sizeof activityScriptState);
            readable = true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            readable = false;
        }

        std::array<char, core::log::kLineCapacity> line{};
        const int written = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_script_runtime_resolve n=%u caller_rva=0x%llX object=%p component=0x%08X runtime=%p readable=%s field38=%u field3c=%d q0=0x%016llX q1=0x%016llX q2=0x%016llX q3=0x%016llX q4=0x%016llX q5=0x%016llX q6=0x%016llX q7=0x%016llX mutation=observe_only",
            static_cast<unsigned>(index + 1U),
            static_cast<unsigned long long>(callerRva),
            object,
            component,
            runtime,
            readable ? "yes" : "no",
            static_cast<unsigned>(activityScriptFlag),
            activityScriptState,
            static_cast<unsigned long long>(words[0]),
            static_cast<unsigned long long>(words[1]),
            static_cast<unsigned long long>(words[2]),
            static_cast<unsigned long long>(words[3]),
            static_cast<unsigned long long>(words[4]),
            static_cast<unsigned long long>(words[5]),
            static_cast<unsigned long long>(words[6]),
            static_cast<unsigned long long>(words[7]));
        if (written > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(),
                              static_cast<std::size_t>(written) < line.size()
                                  ? static_cast<std::size_t>(written)
                                  : line.size() - 1U});
        }

        auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
        if (image != nullptr) {
            const std::uintptr_t captureRva = callerRva >= 0x100U ? callerRva - 0x100U
                                                                  : callerRva;
            std::array<wchar_t, 80> phase{};
            const int phaseLength = std::swprintf(
                phase.data(),
                phase.size(),
                L"activity_script_runtime_resolver_%08llX",
                static_cast<unsigned long long>(callerRva));
            if (phaseLength > 0) {
                dump_publish_function(phase.data(), image + captureRva);
            }
        }
        report_activity_script_resolver_stack(static_cast<std::uint32_t>(index + 1U),
                                              callerRva);
        return;
    }
}

/** Calls the native runtime resolver unchanged and records only real slot-18 consumers. */
__declspec(noinline) void* __fastcall object_runtime_resolve(void* object,
                                                             std::uint32_t component) noexcept {
    const ObjectRuntimeResolve original =
        g_objectRuntimeResolveOriginal.load(std::memory_order_acquire);
    if (original == nullptr) {
        return nullptr;
    }
    void* const runtime = original(object, component);

    std::uint32_t schema = 0U;
    bool isActivityScript = false;
    __try {
        if (object != nullptr) {
            std::memcpy(&schema,
                        static_cast<const std::byte*>(object) + 0x0CU,
                        sizeof schema);
            isActivityScript = schema == kActivityScriptSchema;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        isActivityScript = false;
    }
    if (!isActivityScript) {
        return runtime;
    }

    const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    const auto image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const std::uintptr_t callerRva = image != 0U && caller >= image ? caller - image : 0U;
    capture_activity_script_resolver(object, component, runtime, callerRva);
    return runtime;
}

/** @return True only inside the bounded window armed by the embedded service-7 response. */
[[nodiscard]] bool service_seven_capture_active() noexcept {
    if (!g_serviceSevenCaptureActive.load(std::memory_order_acquire)) {
        return false;
    }
    const ULONGLONG armedAt = g_serviceSevenArmedAt.load(std::memory_order_acquire);
    const ULONGLONG elapsed = GetTickCount64() - armedAt;
    if (armedAt != 0U && elapsed <= kServiceSevenCaptureMilliseconds) {
        return true;
    }
    if (g_serviceSevenCaptureActive.exchange(false, std::memory_order_acq_rel)) {
        g_serviceSevenReadCaptureActive.store(false, std::memory_order_release);
        std::array<char, core::log::kLineCapacity> line{};
        const int written = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_host_manager_svc7_decode_complete generation=%u "
            "result=timeout reads=%u responses=%u elapsed_ms=%llu mutation=observe_only",
            g_serviceSevenGeneration.load(std::memory_order_acquire),
            g_serviceSevenObserved.load(std::memory_order_acquire),
            g_serviceSevenResponsesObserved.load(std::memory_order_acquire),
            static_cast<unsigned long long>(elapsed));
        if (written > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(written)});
        }
    }
    return false;
}

/** Emits the native game-image chain for the first read issued by one distinct caller. */
void report_service_seven_stack(std::uint32_t generation,
                                std::uintptr_t callerRva) noexcept {
    std::array<void*, kMaximumStackFrames> frames{};
    const USHORT captured = RtlCaptureStackBackTrace(
        0, static_cast<ULONG>(frames.size()), frames.data(), nullptr);
    const auto image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    if (image == 0U) {
        return;
    }

    std::array<char, core::log::kLineCapacity> line{};
    int used = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_host_manager_svc7_decode_stack generation=%u "
        "caller_rva=0x%llX frames=",
        generation,
        static_cast<unsigned long long>(callerRva));
    if (used <= 0) {
        return;
    }
    for (USHORT index = 0; index < captured && static_cast<std::size_t>(used) < line.size();
         ++index) {
        const auto address = reinterpret_cast<std::uintptr_t>(frames[index]);
        if (address < image || address - image >= kMaximumGameImageSize) {
            continue;
        }
        const int appended = std::snprintf(
            line.data() + used,
            line.size() - static_cast<std::size_t>(used),
            "%s0x%llX",
            line[static_cast<std::size_t>(used) - 1U] == '=' ? "" : ",",
            static_cast<unsigned long long>(address - image));
        if (appended <= 0) {
            break;
        }
        used += appended;
    }
    core::log::write(core::log::Channel::client,
                     core::log::Level::info,
                     {line.data(),
                      static_cast<std::size_t>(used) < line.size()
                          ? static_cast<std::size_t>(used)
                          : line.size() - 1U});
}

/** Saves one live caller region and stack the first time it consumes service-7-window bits. */
void capture_service_seven_caller(std::uint32_t generation,
                                  std::uintptr_t callerRva) noexcept {
    if (callerRva == 0U || callerRva >= kMaximumGameImageSize) {
        return;
    }
    for (auto& known : g_serviceSevenCallerRvas) {
        const std::uintptr_t present = known.load(std::memory_order_acquire);
        if (present == callerRva) {
            return;
        }
        if (present != 0U) {
            continue;
        }
        std::uintptr_t empty = 0U;
        if (!known.compare_exchange_strong(empty,
                                           callerRva,
                                           std::memory_order_acq_rel,
                                           std::memory_order_acquire)) {
            continue;
        }
        auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
        if (image != nullptr) {
            const std::uintptr_t captureRva = callerRva >= 0x100U ? callerRva - 0x100U
                                                                  : callerRva;
            std::array<wchar_t, 80> phase{};
            const int phaseLength = std::swprintf(
                phase.data(),
                phase.size(),
                L"activity_host_manager_svc7_caller_%08llX",
                static_cast<unsigned long long>(callerRva));
            if (phaseLength > 0) {
                dump_publish_function(phase.data(), image + captureRva);
            }
        }
        report_service_seven_stack(generation, callerRva);
        return;
    }
}

/** Emits one bounded native read made after the exact service-7 body was encoded. */
void report_service_seven_read(std::uint32_t generation,
                               std::uint32_t observation,
                               const char* kind,
                               std::uintptr_t callerRva,
                               void* stream,
                               std::uint32_t width,
                               std::uint32_t value,
                               std::uint32_t before,
                               std::uint32_t after) noexcept {
    std::array<char, core::log::kLineCapacity> line{};
    const int written = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_host_manager_svc7_decode_read generation=%u n=%u "
        "kind=%s caller_rva=0x%llX stream=%p width=%u value=0x%08X "
        "bit_before=%u bit_after=%u elapsed_ms=%llu mutation=observe_only",
        generation,
        observation,
        kind,
        static_cast<unsigned long long>(callerRva),
        stream,
        width,
        value,
        before,
        after,
        static_cast<unsigned long long>(
            GetTickCount64() - g_serviceSevenArmedAt.load(std::memory_order_acquire)));
    if (written > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(written)});
    }
}

/** Observes native authority reads without changing any argument, state, or return value. */
__declspec(noinline) std::uint32_t __fastcall bit_reader(void* stream,
                                                         std::uint32_t width) noexcept {
    const BitReader original = g_original.load(std::memory_order_acquire);
    if (original == nullptr) {
        return 0;
    }

    const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    const auto image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const std::uintptr_t callerRva = image != 0 && caller >= image ? caller - image : 0;
    if (callerRva == kAuthorityPrefixReadCallerRva) {
        g_authorityStream = stream;
        ++g_authorityMessage;
    }

    state::activity::forced::ForcedDestination forced{};
    std::string_view package{};
    const bool inspect = stream == g_authorityStream && g_authorityMessage != 0
                         && g_authorityMessage <= kMaximumMessages
                         && opening_is_forced(forced, package);
    const std::uint32_t observation = inspect
                                          ? g_observed.fetch_add(1, std::memory_order_relaxed) + 1U
                                          : 0U;
    const bool capture = inspect && observation <= kMaximumObservations;
    const std::uint32_t before = capture ? consumed_bits(stream) : 0U;

    const bool serviceSevenInspect =
        g_serviceSevenReadCaptureActive.load(std::memory_order_acquire)
        && service_seven_capture_active();
    const std::uint32_t serviceSevenObservation =
        serviceSevenInspect
            ? g_serviceSevenObserved.fetch_add(1, std::memory_order_relaxed) + 1U
            : 0U;
    const bool serviceSevenCapture =
        serviceSevenInspect
        && serviceSevenObservation <= kServiceSevenMaximumObservations;
    const std::uint32_t serviceSevenBefore =
        serviceSevenCapture ? consumed_bits(stream) : 0U;

    const std::uint32_t objectDecodeBefore =
        g_objectDecodeActive ? consumed_bits(stream) : 0U;
    const std::uint32_t groupDecodeBefore =
        g_groupDecodeActive ? consumed_bits(stream) : 0U;

    const std::uint32_t value = original(stream, width);

    note_object_decode_read(false,
                            width,
                            value,
                            objectDecodeBefore,
                            g_objectDecodeActive ? consumed_bits(stream) : 0U);
    note_group_decode_read(false,
                           width,
                           value,
                           groupDecodeBefore,
                           g_groupDecodeActive ? consumed_bits(stream) : 0U);

    if (capture) {
        report(observation,
               g_authorityMessage,
               "uint",
               callerRva,
               stream,
               width,
               value,
               before,
               consumed_bits(stream),
               package);
    }
    if (serviceSevenCapture) {
        const std::uint32_t generation =
            g_serviceSevenGeneration.load(std::memory_order_acquire);
        report_service_seven_read(generation,
                                  serviceSevenObservation,
                                  "uint",
                                  callerRva,
                                  stream,
                                  width,
                                  value,
                                  serviceSevenBefore,
                                  consumed_bits(stream));
        capture_service_seven_caller(generation, callerRva);
    } else if (serviceSevenObservation == kServiceSevenMaximumObservations + 1U
               && !g_serviceSevenReadLimitReported.exchange(true,
                                                             std::memory_order_acq_rel)) {
        g_serviceSevenReadCaptureActive.store(false, std::memory_order_release);
        core::log::write(
            core::log::Channel::client,
            core::log::Level::info,
            "ev=bootflow stage=activity_host_manager_svc7_decode_read_limit result=limit "
            "reads=4096 capture_window=continues mutation=observe_only");
    }
    return value;
}

/** Observes matching one-bit reads on the stream armed by the authority-prefix reader. */
__declspec(noinline) bool __fastcall bool_reader(void* stream) noexcept {
    const BoolReader original = g_boolOriginal.load(std::memory_order_acquire);
    if (original == nullptr) {
        return false;
    }

    state::activity::forced::ForcedDestination forced{};
    std::string_view package{};
    const bool inspect = stream == g_authorityStream && g_authorityMessage != 0
                         && g_authorityMessage <= kMaximumMessages
                         && opening_is_forced(forced, package);
    const std::uint32_t observation = inspect
                                          ? g_observed.fetch_add(1, std::memory_order_relaxed) + 1U
                                          : 0U;
    const bool capture = inspect && observation <= kMaximumObservations;
    const std::uint32_t before = capture ? consumed_bits(stream) : 0U;

    const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    const auto image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const std::uintptr_t callerRva = image != 0 && caller >= image ? caller - image : 0;

    const bool serviceSevenInspect =
        g_serviceSevenReadCaptureActive.load(std::memory_order_acquire)
        && service_seven_capture_active();
    const std::uint32_t serviceSevenObservation =
        serviceSevenInspect
            ? g_serviceSevenObserved.fetch_add(1, std::memory_order_relaxed) + 1U
            : 0U;
    const bool serviceSevenCapture =
        serviceSevenInspect
        && serviceSevenObservation <= kServiceSevenMaximumObservations;
    const std::uint32_t serviceSevenBefore =
        serviceSevenCapture ? consumed_bits(stream) : 0U;

    const std::uint32_t objectDecodeBefore =
        g_objectDecodeActive ? consumed_bits(stream) : 0U;
    const std::uint32_t groupDecodeBefore =
        g_groupDecodeActive ? consumed_bits(stream) : 0U;

    const bool value = original(stream);
    note_object_decode_read(true,
                            1U,
                            value ? 1U : 0U,
                            objectDecodeBefore,
                            g_objectDecodeActive ? consumed_bits(stream) : 0U);
    note_group_decode_read(true,
                           1U,
                           value ? 1U : 0U,
                           groupDecodeBefore,
                           g_groupDecodeActive ? consumed_bits(stream) : 0U);
    if (capture) {
        report(observation,
               g_authorityMessage,
               "bool",
               callerRva,
               stream,
               1U,
               value ? 1U : 0U,
               before,
               consumed_bits(stream),
               package);
        if (before == kRosterTerminalBit && g_authorityMessage >= 2U) {
            report_terminal_stack(g_authorityMessage, package);
        }
    }
    if (serviceSevenCapture) {
        const std::uint32_t generation =
            g_serviceSevenGeneration.load(std::memory_order_acquire);
        report_service_seven_read(generation,
                                  serviceSevenObservation,
                                  "bool",
                                  callerRva,
                                  stream,
                                  1U,
                                  value ? 1U : 0U,
                                  serviceSevenBefore,
                                  consumed_bits(stream));
        capture_service_seven_caller(generation, callerRva);
    } else if (serviceSevenObservation == kServiceSevenMaximumObservations + 1U
               && !g_serviceSevenReadLimitReported.exchange(true,
                                                             std::memory_order_acq_rel)) {
        g_serviceSevenReadCaptureActive.store(false, std::memory_order_release);
        core::log::write(
            core::log::Channel::client,
            core::log::Level::info,
            "ev=bootflow stage=activity_host_manager_svc7_decode_read_limit result=limit "
            "reads=4096 capture_window=continues mutation=observe_only");
    }
    return value;
}

struct ResponsePointerSnapshot final {
    std::uint64_t first{};
    std::uint64_t second{};
    bool readable{};
};

[[nodiscard]] ResponsePointerSnapshot snapshot_response_pointer(const void* pointer) noexcept {
    ResponsePointerSnapshot snapshot{};
    if (pointer == nullptr) {
        return snapshot;
    }
    __try {
        std::memcpy(&snapshot.first, pointer, sizeof snapshot.first);
        std::memcpy(&snapshot.second,
                    static_cast<const std::byte*>(pointer) + sizeof snapshot.first,
                    sizeof snapshot.second);
        snapshot.readable = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        snapshot = {};
    }
    return snapshot;
}

void report_response_pointer_snapshot(std::uint32_t generation,
                                      std::uint32_t observation,
                                      const char* phase,
                                      std::uint32_t requestId,
                                      std::uint32_t responseService,
                                      void* descriptor,
                                      void* primaryData,
                                      std::uint32_t primarySize,
                                      void* secondaryData,
                                      std::uint32_t secondarySize,
                                      void* responseMap,
                                      std::uint32_t mapKey,
                                      void* completion,
                                      void* context) noexcept {
    const ResponsePointerSnapshot descriptorWords = snapshot_response_pointer(descriptor);
    const ResponsePointerSnapshot primaryWords = snapshot_response_pointer(primaryData);
    const ResponsePointerSnapshot secondaryWords = snapshot_response_pointer(secondaryData);
    const ResponsePointerSnapshot mapWords = snapshot_response_pointer(responseMap);
    const ResponsePointerSnapshot completionWords = snapshot_response_pointer(completion);
    const ResponsePointerSnapshot contextWords = snapshot_response_pointer(context);
    const unsigned int readableMask = (descriptorWords.readable ? 1U : 0U)
                                      | (primaryWords.readable ? 2U : 0U)
                                      | (secondaryWords.readable ? 4U : 0U)
                                      | (mapWords.readable ? 8U : 0U)
                                      | (completionWords.readable ? 16U : 0U)
                                      | (contextWords.readable ? 32U : 0U);

    std::array<char, core::log::kLineCapacity> line{};
    const int written = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_host_manager_svc7_handler phase=%s generation=%u n=%u request=%u internal_service=%u primary=%p primary_size=%u secondary=%p secondary_size=%u descriptor=%p response_map=%p map_key=%u completion=%p context=%p readable=0x%02X descriptor_words=%016llX,%016llX primary_words=%016llX,%016llX secondary_words=%016llX,%016llX map_words=%016llX,%016llX completion_words=%016llX,%016llX context_words=%016llX,%016llX mutation=observe_only",
        phase,
        generation,
        observation,
        requestId,
        responseService,
        primaryData,
        primarySize,
        secondaryData,
        secondarySize,
        descriptor,
        responseMap,
        mapKey,
        completion,
        context,
        readableMask,
        static_cast<unsigned long long>(descriptorWords.first),
        static_cast<unsigned long long>(descriptorWords.second),
        static_cast<unsigned long long>(primaryWords.first),
        static_cast<unsigned long long>(primaryWords.second),
        static_cast<unsigned long long>(secondaryWords.first),
        static_cast<unsigned long long>(secondaryWords.second),
        static_cast<unsigned long long>(mapWords.first),
        static_cast<unsigned long long>(mapWords.second),
        static_cast<unsigned long long>(completionWords.first),
        static_cast<unsigned long long>(completionWords.second),
        static_cast<unsigned long long>(contextWords.first),
        static_cast<unsigned long long>(contextWords.second));
    if (written > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(),
                          static_cast<std::size_t>(written) < line.size()
                              ? static_cast<std::size_t>(written)
                              : line.size() - 1U});
    }
}

void dump_response_handler_buffer(std::uint32_t generation,
                                  std::uint32_t observation,
                                  const wchar_t* kind,
                                  const void* data,
                                  std::size_t size) noexcept {
    if (kind == nullptr || data == nullptr || size == 0U) {
        return;
    }
    std::array<wchar_t, 112> phase{};
    const int written = std::swprintf(phase.data(),
                                      phase.size(),
                                      L"activity_host_manager_svc7_g%u_call%u_%ls",
                                      generation,
                                      observation,
                                      kind);
    if (written > 0) {
        dump_object_buffer(phase.data(), data, size);
    }
}

/** Observes every generic response handled inside the exact wire service-7 capture window. */
__declspec(noinline) std::uint64_t __fastcall response_handle_internal(
    std::uint32_t requestId,
    std::uint32_t responseService,
    void* descriptor,
    void* primaryData,
    std::uint32_t primarySize,
    void* secondaryData,
    std::uint32_t secondarySize,
    void* responseMap,
    std::uint32_t mapKey,
    void* completion,
    void* context) noexcept {
    const ResponseHandleInternal original =
        g_responseHandleInternalOriginal.load(std::memory_order_acquire);
    if (original == nullptr) {
        return 0U;
    }
    if (!service_seven_capture_active()) {
        return original(requestId,
                        responseService,
                        descriptor,
                        primaryData,
                        primarySize,
                        secondaryData,
                        secondarySize,
                        responseMap,
                        mapKey,
                        completion,
                        context);
    }

    const std::uint32_t generation =
        g_serviceSevenGeneration.load(std::memory_order_acquire);
    const std::uint32_t observation =
        g_serviceSevenResponsesObserved.fetch_add(1, std::memory_order_relaxed) + 1U;
    if (observation > kServiceSevenMaximumResponses) {
        return original(requestId,
                        responseService,
                        descriptor,
                        primaryData,
                        primarySize,
                        secondaryData,
                        secondarySize,
                        responseMap,
                        mapKey,
                        completion,
                        context);
    }
    const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    const auto image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const std::uintptr_t callerRva = image != 0U && caller >= image ? caller - image : 0U;

    report_response_pointer_snapshot(generation,
                                     observation,
                                     "entry",
                                     requestId,
                                     responseService,
                                     descriptor,
                                     primaryData,
                                     primarySize,
                                     secondaryData,
                                     secondarySize,
                                     responseMap,
                                     mapKey,
                                     completion,
                                     context);
    report_service_seven_stack(generation, callerRva);
    dump_response_handler_buffer(generation, observation, L"descriptor", descriptor, 0x100U);
    dump_response_handler_buffer(
        generation,
        observation,
        L"primary",
        primaryData,
        (std::min)(static_cast<std::size_t>(primarySize),
                   static_cast<std::size_t>(0x400U)));
    dump_response_handler_buffer(
        generation,
        observation,
        L"secondary_before",
        secondaryData,
        (std::min)(static_cast<std::size_t>(secondarySize),
                   static_cast<std::size_t>(0x400U)));
    dump_response_handler_buffer(
        generation, observation, L"response_map_before", responseMap, 0x100U);
    report_response_dispatch_target(
        generation, observation, requestId, responseService, responseMap);

    const std::uint64_t result = original(requestId,
                                          responseService,
                                          descriptor,
                                          primaryData,
                                          primarySize,
                                          secondaryData,
                                          secondarySize,
                                          responseMap,
                                          mapKey,
                                          completion,
                                          context);

    dump_response_handler_buffer(
        generation,
        observation,
        L"secondary_after",
        secondaryData,
        (std::min)(static_cast<std::size_t>(secondarySize),
                   static_cast<std::size_t>(0x400U)));
    dump_response_handler_buffer(
        generation, observation, L"response_map_after", responseMap, 0x100U);
    report_response_pointer_snapshot(generation,
                                     observation,
                                     result != 0U ? "return_ok" : "return_fail",
                                     requestId,
                                     responseService,
                                     descriptor,
                                     primaryData,
                                     primarySize,
                                     secondaryData,
                                     secondarySize,
                                     responseMap,
                                     mapKey,
                                     completion,
                                     context);
    return result;
}

/** Safely reads the bounded changed-object list's leading count. */
[[nodiscard]] std::uint32_t changed_object_count(const void* changedObjects) noexcept {
    std::uint32_t count = 0;
    __try {
        if (changedObjects != nullptr) {
            std::memcpy(&count, changedObjects, sizeof count);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        count = 0;
    }
    return count;
}

void report_changed_objects(void* changedObjects) noexcept {
    if (changedObjects == nullptr || changed_object_count(changedObjects) == 0
        || g_changedObjectsReported.exchange(true, std::memory_order_acq_rel)) {
        return;
    }
    const std::uint32_t count = (std::min)(changed_object_count(changedObjects), 64U);
    // The native collect/apply/finalize passes reserve a 32-byte header. Their indexed loops use
    // `(index + 1) << 5`, so the first entry is at +0x20 rather than immediately after the count.
    // Reading at +8 only reported header scratch and made healthy entries look pointer-shaped.
    const auto* const base = static_cast<const std::byte*>(changedObjects) + 0x20;
    for (std::uint32_t index = 0; index < count; ++index) {
        std::array<std::uint32_t, 8> words{};
        bool readable = true;
        __try {
            std::memcpy(words.data(), base + static_cast<std::size_t>(index) * 32, 32);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            readable = false;
        }
        std::array<char, core::log::kLineCapacity> line{};
        const int written = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_authority_changed_object index=%u readable=%s w0=0x%08X w1=0x%08X w2=0x%08X w3=0x%08X w4=0x%08X w5=0x%08X w6=0x%08X w7=0x%08X",
            index,
            readable ? "yes" : "no",
            words[0],
            words[1],
            words[2],
            words[3],
            words[4],
            words[5],
            words[6],
            words[7]);
        if (written > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(written)});
        }
    }
}

/**
 * Resolves one native changed-object datum using the exact read-only calculation in
 * authority_publish_apply. The resolver is guarded because the registry belongs to Destiny.
 */
[[nodiscard]] std::byte* resolve_changed_object(std::uint32_t datum) noexcept {
    std::byte* object = nullptr;
    __try {
        auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
        if (image == nullptr) {
            return nullptr;
        }
        auto* const registry = *reinterpret_cast<std::byte**>(image + kObjectDatumRegistryRva);
        if (registry == nullptr) {
            return nullptr;
        }
        auto* const buckets = *reinterpret_cast<std::byte**>(registry);
        if (buckets == nullptr) {
            return nullptr;
        }

        const std::int32_t shifted = static_cast<std::int32_t>(datum) >> 13;
        const std::uint64_t bucketIndex =
            ((static_cast<std::uint32_t>(shifted) | 0x0FFC0000ULL) >> 18)
            & static_cast<std::uint16_t>(shifted);
        auto* const bucket = buckets + bucketIndex * 64U;
        const std::uint32_t slot = datum & 0x1FFFU;
        const std::uint32_t stride = *reinterpret_cast<const std::uint32_t*>(bucket + 0x30);
        auto* const records = *reinterpret_cast<std::byte**>(bucket + 0x08);
        if (records == nullptr || stride == 0U) {
            return nullptr;
        }
        auto* const encoded = records + static_cast<std::size_t>(slot) * stride;
        const std::uint64_t mask =
            static_cast<std::uint64_t>(static_cast<std::int64_t>(
                *reinterpret_cast<const std::int32_t*>(bucket + 0x34)))
            & *reinterpret_cast<const std::uint64_t*>(encoded + 0x08);
        object = encoded - mask;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        object = nullptr;
    }
    return object;
}

/**
 * Resolves the request-specific callback exactly as the generic response handler does after
 * codec dispatch. Service 2 uses a no-op descriptor callback; this datum owns the real consumer.
 */
void report_response_dispatch_target(std::uint32_t generation,
                                     std::uint32_t observation,
                                     std::uint32_t requestId,
                                     std::uint32_t responseService,
                                     void* responseMap) noexcept {
    if (responseService != 2U || responseMap == nullptr) {
        return;
    }

    std::uint32_t mapHeader = 0U;
    std::uint32_t datum = 0xFFFFFFFFU;
    std::byte* record = nullptr;
    void* callback = nullptr;
    void* vtable = nullptr;
    void* target = nullptr;
    void* owner = nullptr;
    void* inner = nullptr;
    void* innerVtable = nullptr;
    void* innerTarget = nullptr;
    std::uint32_t ownerSlot = 0xFFFFFFFFU;
    bool readable = false;
    __try {
        auto* const map = static_cast<std::byte*>(responseMap);
        std::memcpy(&mapHeader, map + 0x80, sizeof mapHeader);
        std::memcpy(&datum, map + 0x84, sizeof datum);
        record = resolve_changed_object(datum);
        if (record != nullptr) {
            callback = *reinterpret_cast<void**>(record);
            if (callback != nullptr) {
                vtable = *reinterpret_cast<void**>(callback);
                if (vtable != nullptr) {
                    target = *reinterpret_cast<void**>(vtable);
                }
                owner = *reinterpret_cast<void**>(static_cast<std::byte*>(callback) + 0x08);
                if (owner != nullptr) {
                    inner = *reinterpret_cast<void**>(static_cast<std::byte*>(owner) + 0x08);
                    std::memcpy(&ownerSlot,
                                static_cast<std::byte*>(owner) + 0x78,
                                sizeof ownerSlot);
                    if (inner != nullptr) {
                        innerVtable = *reinterpret_cast<void**>(inner);
                        if (innerVtable != nullptr) {
                            innerTarget = *reinterpret_cast<void**>(
                                static_cast<std::byte*>(innerVtable) + 0x08);
                        }
                    }
                }
                readable = true;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        readable = false;
    }

    const auto image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const auto targetAddress = reinterpret_cast<std::uintptr_t>(target);
    const auto innerTargetAddress = reinterpret_cast<std::uintptr_t>(innerTarget);
    const std::uintptr_t targetRva =
        image != 0U && targetAddress >= image
                && targetAddress - image < kMaximumGameImageSize
            ? targetAddress - image
            : 0U;
    const std::uintptr_t innerTargetRva =
        image != 0U && innerTargetAddress >= image
                && innerTargetAddress - image < kMaximumGameImageSize
            ? innerTargetAddress - image
            : 0U;
    const ResponsePointerSnapshot recordWords = snapshot_response_pointer(record);
    const ResponsePointerSnapshot callbackWords = snapshot_response_pointer(callback);
    const ResponsePointerSnapshot vtableWords = snapshot_response_pointer(vtable);
    const ResponsePointerSnapshot ownerWords = snapshot_response_pointer(owner);
    const ResponsePointerSnapshot innerWords = snapshot_response_pointer(inner);
    const ResponsePointerSnapshot innerVtableWords = snapshot_response_pointer(innerVtable);

    std::array<char, core::log::kLineCapacity> line{};
    const int written = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_host_manager_svc7_request_dispatch generation=%u n=%u request=%u internal_service=%u map_header=0x%08X datum=0x%08X record=%p callback=%p vtable=%p target=%p target_rva=0x%llX owner=%p owner_slot=%u inner=%p inner_vtable=%p inner_target=%p inner_target_rva=0x%llX readable=%s record_words=%016llX,%016llX callback_words=%016llX,%016llX vtable_words=%016llX,%016llX owner_words=%016llX,%016llX inner_words=%016llX,%016llX inner_vtable_words=%016llX,%016llX mutation=observe_only",
        generation,
        observation,
        requestId,
        responseService,
        mapHeader,
        datum,
        record,
        callback,
        vtable,
        target,
        static_cast<unsigned long long>(targetRva),
        owner,
        ownerSlot,
        inner,
        innerVtable,
        innerTarget,
        static_cast<unsigned long long>(innerTargetRva),
        readable ? "yes" : "no",
        static_cast<unsigned long long>(recordWords.first),
        static_cast<unsigned long long>(recordWords.second),
        static_cast<unsigned long long>(callbackWords.first),
        static_cast<unsigned long long>(callbackWords.second),
        static_cast<unsigned long long>(vtableWords.first),
        static_cast<unsigned long long>(vtableWords.second),
        static_cast<unsigned long long>(ownerWords.first),
        static_cast<unsigned long long>(ownerWords.second),
        static_cast<unsigned long long>(innerWords.first),
        static_cast<unsigned long long>(innerWords.second),
        static_cast<unsigned long long>(innerVtableWords.first),
        static_cast<unsigned long long>(innerVtableWords.second));
    if (written > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(),
                          static_cast<std::size_t>(written) < line.size()
                              ? static_cast<std::size_t>(written)
                              : line.size() - 1U});
    }

    dump_response_handler_buffer(generation, observation, L"dispatch_record", record, 0x100U);
    dump_response_handler_buffer(generation, observation, L"dispatch_callback", callback, 0x200U);
    dump_response_handler_buffer(generation, observation, L"dispatch_owner", owner, 0x200U);
    dump_response_handler_buffer(generation, observation, L"dispatch_inner", inner, 0x200U);
    if (targetRva != 0U) {
        std::array<wchar_t, 112> phase{};
        const int phaseWritten = std::swprintf(
            phase.data(),
            phase.size(),
            L"activity_host_manager_svc7_dispatch_target_%08llX",
            static_cast<unsigned long long>(targetRva));
        if (phaseWritten > 0) {
            dump_publish_function(phase.data(), static_cast<std::byte*>(target));
        }
    }
    if (innerTargetRva != 0U) {
        std::array<wchar_t, 112> phase{};
        const int phaseWritten = std::swprintf(
            phase.data(),
            phase.size(),
            L"activity_host_manager_svc7_inner_target_%08llX",
            static_cast<unsigned long long>(innerTargetRva));
        if (phaseWritten > 0) {
            dump_publish_function(phase.data(), static_cast<std::byte*>(innerTarget));
        }
    }
}

void report_activity_script_post_apply(void* changedObjects) noexcept {
    // The server's stable Homecoming roster is 16,35,18,17,41,..., so entry two is the
    // activity-script authority object. Never inspect it until all three entries exist.
    if (changedObjects == nullptr || changed_object_count(changedObjects) <= 2U
        || g_activityScriptPostApplyReported.exchange(true, std::memory_order_acq_rel)) {
        return;
    }

    std::uint32_t datum = 0xFFFFFFFFU;
    std::byte* object = nullptr;
    std::int32_t componentIndex = -1;
    std::uint32_t component = 0xFFFFFFFFU;
    void* runtime = nullptr;
    std::array<std::uint64_t, 16> objectWords{};
    std::array<std::uint64_t, 16> runtimeWords{};
    bool objectReadable = false;
    bool runtimeReadable = false;
    __try {
        const auto* const entry = static_cast<const std::byte*>(changedObjects) + 0x20 + 2U * 32U;
        std::memcpy(&datum, entry, sizeof datum);
        object = resolve_changed_object(datum);
        if (object != nullptr) {
            std::memcpy(objectWords.data(), object, sizeof objectWords);
            componentIndex = *reinterpret_cast<const std::int32_t*>(object + 0x0C);
            objectReadable = true;
            if (componentIndex != -1) {
                auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
                const auto componentLookup = reinterpret_cast<ObjectComponentLookup>(
                    image + kObjectComponentLookupRva);
                const auto runtimeResolve = reinterpret_cast<ObjectRuntimeResolve>(
                    image + kObjectRuntimeResolveRva);
                std::uint64_t componentValue = 0;
                (void)componentLookup(&componentValue, componentIndex);
                component = static_cast<std::uint32_t>(componentValue);
                runtime = runtimeResolve(object, component);
                if (runtime != nullptr) {
                    std::memcpy(runtimeWords.data(), runtime, sizeof runtimeWords);
                    runtimeReadable = true;
                }
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        objectReadable = false;
        runtimeReadable = false;
    }

    std::array<char, core::log::kLineCapacity> line{};
    int written = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_post_apply datum=0x%08X object=%p object_readable=%s component_index=%d component=0x%08X runtime=%p runtime_readable=%s",
        datum,
        object,
        objectReadable ? "yes" : "no",
        componentIndex,
        component,
        runtime,
        runtimeReadable ? "yes" : "no");
    if (written > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(written)});
    }

    const auto reportWords = [&line](const char* source,
                                     std::uint32_t first,
                                     const std::array<std::uint64_t, 16>& words) noexcept {
        const int count = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_script_post_apply_words source=%s first=%u q0=0x%016llX q1=0x%016llX q2=0x%016llX q3=0x%016llX q4=0x%016llX q5=0x%016llX q6=0x%016llX q7=0x%016llX",
            source,
            first,
            words[first + 0U],
            words[first + 1U],
            words[first + 2U],
            words[first + 3U],
            words[first + 4U],
            words[first + 5U],
            words[first + 6U],
            words[first + 7U]);
        if (count > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(count)});
        }
    };
    if (objectReadable) {
        reportWords("object", 0U, objectWords);
        reportWords("object", 8U, objectWords);
    }
    if (runtimeReadable) {
        reportWords("runtime", 0U, runtimeWords);
        reportWords("runtime", 8U, runtimeWords);
    }

}

/** Reports mission script/director runtime changes wherever the current delta placed them. */
void report_mission_runtime_changes(void* changedObjects) noexcept {
    constexpr std::array<std::uint32_t, 2> kSlots = {35U, 18U};
    constexpr std::array<std::uint32_t, 2> kSchemas = {0x808099BFU, 0x80809919U};
    if (changedObjects == nullptr) {
        return;
    }
    const std::uint32_t count = (std::min)(changed_object_count(changedObjects), 128U);

    for (std::size_t mission = 0; mission < kSchemas.size(); ++mission) {
        std::uint32_t entryIndex = 0xFFFFFFFFU;
        std::uint32_t datum = 0xFFFFFFFFU;
        std::uint32_t schema = 0U;
        std::uint32_t component = 0xFFFFFFFFU;
        std::byte* object = nullptr;
        void* runtime = nullptr;
        std::array<std::uint64_t, 8> words{};
        bool readable = false;
        for (std::uint32_t index = 0; index < count && entryIndex == 0xFFFFFFFFU; ++index) {
            __try {
                const auto* const entry = static_cast<const std::byte*>(changedObjects) + 0x20
                                          + static_cast<std::size_t>(index) * 32U;
                std::memcpy(&datum, entry, sizeof datum);
                object = resolve_changed_object(datum);
                if (object == nullptr) {
                    continue;
                }
                std::memcpy(&schema, object + 0x0C, sizeof schema);
                if (schema != kSchemas[mission]) {
                    continue;
                }
                entryIndex = index;
                auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
                const auto componentLookup = reinterpret_cast<ObjectComponentLookup>(
                    image + kObjectComponentLookupRva);
                const auto runtimeResolve = reinterpret_cast<ObjectRuntimeResolve>(
                    image + kObjectRuntimeResolveRva);
                std::uint64_t componentValue = 0;
                (void)componentLookup(&componentValue, static_cast<std::int32_t>(schema));
                component = static_cast<std::uint32_t>(componentValue);
                runtime = runtimeResolve(object, component);
                if (runtime != nullptr) {
                    std::memcpy(words.data(), runtime, sizeof words);
                    readable = true;
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                readable = false;
            }
        }
        if (entryIndex == 0xFFFFFFFFU) {
            continue;
        }

        // A readable type-18 runtime in the changed-object list proves only that mission-state
        // storage survived authority gating and completed its post-apply path. It does not prove
        // that an activity-host mission executor exists. Until this exact observation, the server
        // continues including both type 18 and type 35.
        if (mission == 1U && readable) {
            state::activity::forced::ForcedDestination forced{};
            state::activity::forced::snapshot(forced);
            const std::size_t packageLength =
                forced.packageNameLength <= forced.packageName.size()
                    ? forced.packageNameLength
                    : forced.packageName.size();
            const std::string_view package(forced.packageName.data(), packageLength);
            if (state::activity::forced::override_active() && package == "mission_scot"
                && state::activity::acknowledge_mission_authority_runtime_initialized()) {
                core::log::write(
                    core::log::Channel::client,
                    core::log::Level::info,
                    "ev=bootflow stage=mission_authority_runtime_initialization result=acknowledged slot=18 observer=post_apply storage=created next=authority_preserve");
            }
            if (static_cast<std::uint32_t>(words[7] >> 32U) == 4U) {
                arm_omega_forest_route_trace();
            }
        }

        std::uint64_t hash = readable ? 1469598103934665603ULL : 0U;
        if (readable) {
            const auto* const bytes = reinterpret_cast<const std::uint8_t*>(words.data());
            for (std::size_t index = 0; index < sizeof words; ++index) {
                hash ^= bytes[index];
                hash *= 1099511628211ULL;
            }
        }
        const bool first =
            !g_missionRuntimeReported[mission].exchange(true, std::memory_order_acq_rel);
        const std::uint64_t previous =
            g_missionRuntimeHashes[mission].exchange(hash, std::memory_order_acq_rel);
        if (!first && previous == hash) {
            continue;
        }

        std::array<char, core::log::kLineCapacity> line{};
        const int written = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=mission_runtime_change slot=%u entry=%u datum=0x%08X object=%p schema=0x%08X expected_schema=0x%08X component=0x%08X runtime=%p readable=%s first=%u previous_hash=0x%016llX hash=0x%016llX q0=0x%016llX q1=0x%016llX q2=0x%016llX q3=0x%016llX q4=0x%016llX q5=0x%016llX q6=0x%016llX q7=0x%016llX mutation=observe_only",
            kSlots[mission],
            entryIndex,
            datum,
            object,
            schema,
            kSchemas[mission],
            component,
            runtime,
            readable ? "yes" : "no",
            first ? 1U : 0U,
            static_cast<unsigned long long>(previous),
            static_cast<unsigned long long>(hash),
            static_cast<unsigned long long>(words[0]),
            static_cast<unsigned long long>(words[1]),
            static_cast<unsigned long long>(words[2]),
            static_cast<unsigned long long>(words[3]),
            static_cast<unsigned long long>(words[4]),
            static_cast<unsigned long long>(words[5]),
            static_cast<unsigned long long>(words[6]),
            static_cast<unsigned long long>(words[7]));
        if (written > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(),
                              static_cast<std::size_t>(written) < line.size()
                                  ? static_cast<std::size_t>(written)
                                  : line.size() - 1U});
        }
    }
}

[[nodiscard]] std::uint64_t hash_omega_runtime(const std::byte* state,
                                               std::size_t bytes) noexcept {
    if (state == nullptr || bytes == 0U || bytes > kOmegaRuntimeMaximumBytes) {
        return 0U;
    }
    std::uint64_t hash = 1469598103934665603ULL;
    for (std::size_t index = 0; index < bytes; ++index) {
        hash ^= std::to_integer<std::uint8_t>(state[index]);
        hash *= 1099511628211ULL;
    }
    return hash;
}

[[nodiscard]] const char* omega_runtime_label(std::uint32_t schema) noexcept {
    switch (schema) {
    case 0x8080626BU:
        return "scene";
    case 0x8080992FU:
        return "visual";
    case 0x8080954BU:
        return "hold";
    case 0x80804F48U:
        return "gate";
    case 0x808094F1U:
        return "engagement";
    case 0x80809524U:
        return "point";
    case 0x80809532U:
        return "monitor";
    default:
        return nullptr;
    }
}

[[nodiscard]] std::int32_t omega_portal_visual_slot(std::uint32_t datum) noexcept {
    switch (datum & 0x00FFFFFFU) {
    // BA5F26EF type-4 index 0: the authored `lighthouse_teleport` transport device.
    case 0x00F47B52U:
    case 0x00F26019U:
        return 0;
    // Live client component datums for D00142CF's three type-4 entries (indices 2..4).
    case 0x00F47B76U:
    case 0x00F26020U:
        return 2;
    case 0x00F47B79U:
    case 0x00F26021U:
        return 3;
    case 0x00F47B7CU:
    case 0x00F26022U:
        return 4;
    default:
        return -1;
    }
}

struct OmegaVisualSelectorSnapshot final {
    std::uint32_t datum{0xFFFFFFFFU};
    std::uint32_t appliedGeneration{};
    std::int32_t explicitIndex{-1};
    std::uint32_t selectedGeneration{};
    std::int32_t cachedIndex{-1};
    std::array<std::uint32_t, 4> candidateMask{};
    std::array<std::uint32_t, 8> referenceWords{};
    std::uint64_t liveEffect{0xFFFFFFFFFFFFFFFFULL};
    std::int64_t authoredCount{-1};
    std::int64_t tableRelative{};
    std::int64_t optionalRelative{};
    std::int32_t targetDatum{-1};
    std::array<std::uint64_t, 4> candidateReferences{};
    std::uint8_t stickySelection{};
    std::uint8_t requiresReference{};
    std::uint8_t usesCandidateMask{};
    std::uint8_t optionalRouteActive{};
    bool componentReadable{};
    bool authoredReadable{};
};

/**
 * Mirrors only the read-only datum resolution performed at the head of +9F2200. Keeping this
 * snapshot beside the selector result exposes the authored entry count and all top-level gates
 * without calling any additional game function or changing selector state.
 */
[[nodiscard]] OmegaVisualSelectorSnapshot capture_omega_visual_selector(
    const void* component) noexcept {
    OmegaVisualSelectorSnapshot snapshot{};
    if (component == nullptr) {
        return snapshot;
    }

    __try {
        const auto* const bytes = static_cast<const std::byte*>(component);
        std::memcpy(&snapshot.datum, bytes, sizeof snapshot.datum);
        std::memcpy(&snapshot.appliedGeneration,
                    bytes + 0x180U,
                    sizeof snapshot.appliedGeneration);
        std::memcpy(&snapshot.explicitIndex, bytes + 0x184U, sizeof snapshot.explicitIndex);
        std::memcpy(snapshot.referenceWords.data(),
                    bytes + 0x190U,
                    snapshot.referenceWords.size() * sizeof snapshot.referenceWords[0]);
        std::memcpy(&snapshot.selectedGeneration,
                    bytes + 0x2F0U,
                    sizeof snapshot.selectedGeneration);
        std::memcpy(&snapshot.stickySelection,
                    bytes + 0x2F5U,
                    sizeof snapshot.stickySelection);
        std::memcpy(&snapshot.cachedIndex, bytes + 0x2F8U, sizeof snapshot.cachedIndex);
        std::memcpy(snapshot.candidateMask.data(),
                    bytes + 0x2FCU,
                    snapshot.candidateMask.size() * sizeof snapshot.candidateMask[0]);
        std::memcpy(&snapshot.liveEffect, bytes + 0x440U, sizeof snapshot.liveEffect);
        snapshot.componentReadable = true;

        auto* const image = reinterpret_cast<const std::byte*>(GetModuleHandleW(nullptr));
        if (image == nullptr) {
            return snapshot;
        }
        std::uintptr_t registry = 0U;
        std::uintptr_t pool = 0U;
        std::uintptr_t componentRelative = 0U;
        std::memcpy(&registry, image + kObjectDatumRegistryRva, sizeof registry);
        std::memcpy(&componentRelative, bytes + 0x08U, sizeof componentRelative);
        if (registry == 0U) {
            return snapshot;
        }
        // +9F2200 loads the registry global and then dereferences its bucket-table pointer
        // (MOV RAX,[global]; MOV R10,[RAX]). Match that exact two-level path.
        std::memcpy(&pool, reinterpret_cast<const void*>(registry), sizeof pool);
        if (pool == 0U) {
            return snapshot;
        }

        const std::uint32_t page = static_cast<std::uint32_t>(
            static_cast<std::int32_t>(snapshot.datum) >> 13);
        const std::uint64_t groupIndex =
            ((static_cast<std::uint64_t>(page) | 0x0FFC0000ULL) >> 18U)
            & static_cast<std::uint64_t>(page & 0xFFFFU);
        const std::uintptr_t group = pool + groupIndex * 0x40U;
        std::int32_t stride = 0;
        std::int32_t maskOffset = 0;
        std::uintptr_t slots = 0U;
        std::memcpy(&slots,
                    reinterpret_cast<const void*>(group + 0x08U),
                    sizeof slots);
        std::memcpy(&stride,
                    reinterpret_cast<const void*>(group + 0x30U),
                    sizeof stride);
        std::memcpy(&maskOffset,
                    reinterpret_cast<const void*>(group + 0x34U),
                    sizeof maskOffset);
        if (slots == 0U || stride <= 0) {
            return snapshot;
        }
        const std::uintptr_t slot = slots
                                    + static_cast<std::uintptr_t>(snapshot.datum & 0x1FFFU)
                                          * static_cast<std::uintptr_t>(stride);
        std::uint64_t slotMask = 0U;
        std::memcpy(&slotMask,
                    reinterpret_cast<const void*>(slot + 0x08U),
                    sizeof slotMask);
        const std::uintptr_t authored = slot
                                        + componentRelative
                                        - (static_cast<std::int64_t>(maskOffset) & slotMask);

        std::memcpy(&snapshot.authoredCount,
                    reinterpret_cast<const void*>(authored + 0x58U),
                    sizeof snapshot.authoredCount);
        std::memcpy(&snapshot.tableRelative,
                    reinterpret_cast<const void*>(authored + 0x60U),
                    sizeof snapshot.tableRelative);
        std::memcpy(&snapshot.optionalRelative,
                    reinterpret_cast<const void*>(authored + 0x78U),
                    sizeof snapshot.optionalRelative);
        std::memcpy(&snapshot.targetDatum,
                    reinterpret_cast<const void*>(authored + 0x80U),
                    sizeof snapshot.targetDatum);
        std::memcpy(&snapshot.requiresReference,
                    reinterpret_cast<const void*>(authored + 0x88U),
                    sizeof snapshot.requiresReference);
        std::memcpy(&snapshot.usesCandidateMask,
                    reinterpret_cast<const void*>(authored + 0x89U),
                    sizeof snapshot.usesCandidateMask);
        if (snapshot.optionalRelative != 0) {
            const std::uintptr_t optional = authored + 0x78U
                                            + static_cast<std::uintptr_t>(snapshot.optionalRelative);
            std::memcpy(&snapshot.optionalRouteActive,
                        reinterpret_cast<const void*>(optional),
                        sizeof snapshot.optionalRouteActive);
        }
        if (snapshot.authoredCount > 0 && snapshot.tableRelative != 0) {
            const std::size_t count = (std::min)(
                static_cast<std::size_t>(snapshot.authoredCount),
                snapshot.candidateReferences.size());
            for (std::size_t index = 0U; index < count; ++index) {
                const std::uintptr_t candidate = authored
                                                 + static_cast<std::uintptr_t>(
                                                       snapshot.tableRelative)
                                                 + 0xE0U + index * 0x90U;
                std::memcpy(&snapshot.candidateReferences[index],
                            reinterpret_cast<const void*>(candidate),
                            sizeof snapshot.candidateReferences[index]);
            }
        }
        snapshot.authoredReadable = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // A partial snapshot is still useful and never changes native state.
    }
    return snapshot;
}

[[nodiscard]] bool omega_visual_trace_active() noexcept {
    state::activity::forced::ForcedDestination forced{};
    std::string_view package{};
    return opening_is_forced(forced, package) && package == "mission_scot";
}

[[nodiscard]] std::uint64_t safe_omega_visual_state_hash(const void* state) noexcept {
    if (state == nullptr) {
        return 0U;
    }
    std::array<std::byte, kOmegaVisualDecodedStateBytes> snapshot{};
    bool readable = false;
    __try {
        std::memcpy(snapshot.data(), state, snapshot.size());
        readable = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        readable = false;
    }
    return readable ? hash_omega_runtime(snapshot.data(), snapshot.size()) : 0U;
}

/**
 * Records the complete decoded visual state consumed by the native 0x8080992F component. Static
 * RE proved this state is 0x170 bytes; the old 64-byte recorder stopped before the transform that
 * +9EF680 propagates to the live effect. All field names below remain offset-based until the live
 * values identify the parent/anchor without inference.
 */
void report_omega_visual_trace(const char* stage,
                               std::uint32_t observation,
                               void* component,
                               const void* decodedState,
                               const void* effectState,
                               std::int32_t visualIndex,
                               std::uint32_t resultHandle,
                               std::uint64_t previousHash) noexcept {
    if (stage == nullptr || component == nullptr || decodedState == nullptr
        || observation > kOmegaVisualTraceLimit) {
        return;
    }

    std::array<std::byte, kOmegaVisualDecodedStateBytes> state{};
    std::array<std::byte, 0x60U> effect{};
    std::array<std::byte, 0x20U> componentPlacement{};
    std::uint32_t componentDatum = 0xFFFFFFFFU;
    std::uint32_t appliedGeneration = 0U;
    std::uint32_t selectedGeneration = 0xFFFFFFFFU;
    std::uint32_t selectedVisual = 0xFFFFFFFFU;
    std::uint64_t liveEffect = 0U;
    bool stateReadable = false;
    bool componentReadable = false;
    bool effectReadable = false;
    __try {
        std::memcpy(state.data(), decodedState, state.size());
        stateReadable = true;
        std::memcpy(&componentDatum, component, sizeof componentDatum);
        std::memcpy(&appliedGeneration,
                    static_cast<const std::byte*>(component) + 0x180U,
                    sizeof appliedGeneration);
        std::memcpy(&selectedGeneration,
                    static_cast<const std::byte*>(component) + 0x2F0U,
                    sizeof selectedGeneration);
        std::memcpy(&selectedVisual,
                    static_cast<const std::byte*>(component) + 0x2F8U,
                    sizeof selectedVisual);
        std::memcpy(&liveEffect,
                    static_cast<const std::byte*>(component) + 0x440U,
                    sizeof liveEffect);
        // +9EFBC0 falls back to these four authored values when state+0x09 does not authorize
        // resolving the reference record beginning at state+0x10.
        std::memcpy(componentPlacement.data(),
                    static_cast<const std::byte*>(component) + 0x1A0U,
                    componentPlacement.size());
        componentReadable = true;
        if (effectState != nullptr) {
            std::memcpy(effect.data(), effectState, effect.size());
            effectReadable = true;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // Preserve whichever independent snapshots completed before the fault.
    }

    const auto read32 = [](const auto& bytes, std::size_t offset) noexcept {
        std::uint32_t value = 0U;
        if (offset + sizeof value <= bytes.size()) {
            std::memcpy(&value, bytes.data() + offset, sizeof value);
        }
        return value;
    };
    const auto read8 = [](const auto& bytes, std::size_t offset) noexcept {
        return offset < bytes.size() ? std::to_integer<std::uint8_t>(bytes[offset]) : 0U;
    };
    const auto readFloat = [](const auto& bytes, std::size_t offset) noexcept {
        float value = 0.0F;
        if (offset + sizeof value <= bytes.size()) {
            std::memcpy(&value, bytes.data() + offset, sizeof value);
        }
        return value;
    };
    const std::uint64_t hash =
        stateReadable ? hash_omega_runtime(state.data(), state.size()) : 0U;

    std::array<void*, 12> frames{};
    const USHORT frameCount = RtlCaptureStackBackTrace(
        1, static_cast<ULONG>(frames.size()), frames.data(), nullptr);
    const std::uintptr_t image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));

    std::array<char, core::log::kLineCapacity> line{};
    int written = std::snprintf(
        line.data(),
        line.size(),
        "ev=omega_visual_trace stage=%s n=%u component=%p component_readable=%s "
        "component_datum=0x%08X decoded_state=%p state_readable=%s effect_state=%p "
        "effect_readable=%s visual_index=%d result_handle=0x%08X applied_generation=%u "
        "selected_generation=%u selected_visual=%u live_effect=0x%016llX "
        "previous_hash=0x%016llX hash=0x%016llX triggered=%u scene_completed=%u "
        "attach_gate09=%u active08=%u "
        "d00=%08X d04=%08X d08=%08X d0C=%08X d10=%08X d14=%08X d18=%08X d1C=%08X "
        "d20=%08X d24=%08X d28=%08X d2C=%08X d30=%08X d34=%08X d38=%08X d3C=%08X "
        "xyz34=%.6f,%.6f,%.6f d40=%08X d44=%08X d48=%08X d4C=%08X "
        "d50=%08X d54=%08X d58=%08X d5C=%08X effect20=%08X effect24=%08X "
        "effect28=%08X effect2C=%08X "
        "fallback1A0=%08X,%08X,%08X,%08X fallback_xyz=%.6f,%.6f,%.6f,%.6f "
        "state_xyz40=%.6f,%.6f,%.6f,%.6f "
        "effect10=%08X,%08X,%08X,%08X effect_xyz20=%.6f,%.6f,%.6f,%.6f hex=",
        stage,
        observation,
        component,
        componentReadable ? "yes" : "no",
        componentDatum,
        decodedState,
        stateReadable ? "yes" : "no",
        effectState,
        effectReadable ? "yes" : "no",
        visualIndex,
        resultHandle,
        appliedGeneration,
        selectedGeneration,
        selectedVisual,
        static_cast<unsigned long long>(liveEffect),
        static_cast<unsigned long long>(previousHash),
        static_cast<unsigned long long>(hash),
        0U,
        0U,
        read8(state, 0x09U),
        read8(state, 0x08U),
        read32(state, 0x00U),
        read32(state, 0x04U),
        read32(state, 0x08U),
        read32(state, 0x0CU),
        read32(state, 0x10U),
        read32(state, 0x14U),
        read32(state, 0x18U),
        read32(state, 0x1CU),
        read32(state, 0x20U),
        read32(state, 0x24U),
        read32(state, 0x28U),
        read32(state, 0x2CU),
        read32(state, 0x30U),
        read32(state, 0x34U),
        read32(state, 0x38U),
        read32(state, 0x3CU),
        static_cast<double>(readFloat(state, 0x34U)),
        static_cast<double>(readFloat(state, 0x38U)),
        static_cast<double>(readFloat(state, 0x3CU)),
        read32(state, 0x40U),
        read32(state, 0x44U),
        read32(state, 0x48U),
        read32(state, 0x4CU),
        read32(state, 0x50U),
        read32(state, 0x54U),
        read32(state, 0x58U),
        read32(state, 0x5CU),
        read32(effect, 0x20U),
        read32(effect, 0x24U),
        read32(effect, 0x28U),
        read32(effect, 0x2CU),
        read32(componentPlacement, 0x00U),
        read32(componentPlacement, 0x04U),
        read32(componentPlacement, 0x08U),
        read32(componentPlacement, 0x0CU),
        static_cast<double>(readFloat(componentPlacement, 0x00U)),
        static_cast<double>(readFloat(componentPlacement, 0x04U)),
        static_cast<double>(readFloat(componentPlacement, 0x08U)),
        static_cast<double>(readFloat(componentPlacement, 0x0CU)),
        static_cast<double>(readFloat(state, 0x40U)),
        static_cast<double>(readFloat(state, 0x44U)),
        static_cast<double>(readFloat(state, 0x48U)),
        static_cast<double>(readFloat(state, 0x4CU)),
        read32(effect, 0x10U),
        read32(effect, 0x14U),
        read32(effect, 0x18U),
        read32(effect, 0x1CU),
        static_cast<double>(readFloat(effect, 0x20U)),
        static_cast<double>(readFloat(effect, 0x24U)),
        static_cast<double>(readFloat(effect, 0x28U)),
        static_cast<double>(readFloat(effect, 0x2CU)));
    if (written <= 0) {
        return;
    }

    std::size_t used = (std::min)(static_cast<std::size_t>(written), line.size() - 1U);
    constexpr char kHex[] = "0123456789ABCDEF";
    if (stateReadable) {
        for (std::byte value : state) {
            if (used + 2U >= line.size()) {
                break;
            }
            const std::uint8_t byte = std::to_integer<std::uint8_t>(value);
            line[used++] = kHex[byte >> 4U];
            line[used++] = kHex[byte & 0x0FU];
        }
    }
    if (used + 9U < line.size()) {
        const int appended = std::snprintf(line.data() + used, line.size() - used, " frames=");
        if (appended > 0) {
            used += static_cast<std::size_t>(appended);
        }
    }
    for (USHORT index = 0; index < frameCount && used + 20U < line.size(); ++index) {
        const std::uintptr_t frame = reinterpret_cast<std::uintptr_t>(frames[index]);
        const std::uintptr_t rva = image != 0U && frame >= image ? frame - image : frame;
        const int appended = std::snprintf(line.data() + used,
                                           line.size() - used,
                                           "%s+%llX",
                                           index == 0U ? "" : ",",
                                           static_cast<unsigned long long>(rva));
        if (appended <= 0) {
            break;
        }
        used += static_cast<std::size_t>(appended);
    }
    line[(std::min)(used, line.size() - 1U)] = '\0';
    core::log::write(core::log::Channel::client,
                     core::log::Level::info,
                     {line.data(), (std::min)(used, line.size() - 1U)});
}

/** Reads only an exact authored core cannon and its inactive authority fields. */
[[nodiscard]] bool first_cannon_preparation(void* component,
    state::activity::omega_first_mancannon::Preparation& receipt,
    state::activity::omega_crown_transit::Preparation& transitReceipt) noexcept {
    receipt = {};
    transitReceipt = {};
    if (component == nullptr) { return false; }
    std::array<std::byte, 16> prefix{};
    std::array<std::byte, 0x44> state{};
    __try {
        const auto* const bytes = static_cast<const std::byte*>(component);
        std::memcpy(prefix.data(), bytes, prefix.size());
        std::memcpy(state.data(), bytes + 0x180, state.size());
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    return state::activity::omega_first_mancannon::preparation(prefix, state, receipt)
        || state::activity::omega_crown_transit::preparation(prefix, state, transitReceipt);
}

/** Post-original receipt only; native state and entity ownership remain untouched. */
__declspec(noinline) void observe_first_cannon_preparation(void* component,
                                                          std::uint64_t enteredRun) noexcept {
    if (enteredRun == 0U) { return; }
    const auto nav = state::activity::omega_presentation::navigation();
    state::activity::omega_first_mancannon::Preparation receipt{};
    state::activity::omega_crown_transit::Preparation transitReceipt{};
    if (!nav.enabled || nav.run != enteredRun || nav.run != state::activity::mission_run_generation()) {
        report_omega_preparation_reject(component, enteredRun, "navigation_or_run");
        return;
    }
    if (!first_cannon_preparation(component, receipt, transitReceipt)) {
        // Silent for every foreign type-4 object; one line when one of our eleven core
        // definitions was applied with a non-preparation body (active, wrong generation...).
        report_omega_preparation_reject(component, enteredRun, "state_predicate");
        return;
    }
    if (transitReceipt.generation != 0U) {
        state::activity::omega_first_lair::observe_transit_prepared(
            nav.run, transitReceipt.generation, transitReceipt.index);
        if (g_omegaTransitReceiptLoggedRuns[transitReceipt.index].exchange(nav.run,
                std::memory_order_acq_rel) != nav.run) {
            std::array<char, 256> line{};
            const int written = std::snprintf(line.data(), line.size(),
                "ev=omega_crown_transit stage=inactive_applied run=%llu generation=%u core_index=%u candidate=0 placement_override=0 mutation=observe_only",
                static_cast<unsigned long long>(nav.run), transitReceipt.generation,
                static_cast<unsigned>(transitReceipt.index));
            if (written > 0 && static_cast<std::size_t>(written) < line.size()) {
                core::log::write(core::log::Channel::client, core::log::Level::info,
                                {line.data(), static_cast<std::size_t>(written)});
            }
        }
        return;
    }
    // The encounter owner independently qualifies each core's applied generation
    // against the pending publication and keeps the per-run preparation mask.
    state::activity::omega_first_lair::observe_cannon_prepared(
        nav.run, receipt.generation, receipt.index);
    if (g_omegaCannonReceiptLoggedRuns[receipt.index].exchange(nav.run,
            std::memory_order_acq_rel) != nav.run) {
        const auto& cannon = state::activity::omega_first_mancannon::kCannons[receipt.index];
        std::array<char, 280> line{};
        const int written = std::snprintf(line.data(), line.size(),
            "ev=omega_first_cannon stage=inactive_applied run=%llu generation=%u cannon_index=%u source=95FB2E01/4/%u definition=%08X candidate=0 placement_override=0 mutation=observe_only",
            static_cast<unsigned long long>(nav.run), receipt.generation,
            static_cast<unsigned>(receipt.index), static_cast<unsigned>(cannon.coreSlot),
            cannon.coreDefinition);
        if (written > 0 && static_cast<std::size_t>(written) < line.size()) {
            core::log::write(core::log::Channel::client, core::log::Level::info,
                            {line.data(), static_cast<std::size_t>(written)});
        }
    }
}

[[nodiscard]] bool first_cannon_receipt_idle() noexcept {
    return g_omegaCannonReceiptGate.idle();
}

__declspec(noinline) void __fastcall omega_visual_apply(void* component,
                                                         void* stateKey) noexcept {
    const hooking::CallGate::Scope call(g_omegaCannonReceiptGate);
    const auto nav = call.accepts_side_effects()
        ? state::activity::omega_presentation::navigation()
        : state::activity::omega_presentation::Navigation{};
    // Only the retired diagnostic bundle owns the create trace. The narrow
    // receipt does not enable broad visual hashing or nested capture work.
    const bool trace = call.accepts_side_effects()
        && g_omegaVisualCreateOriginal.load(std::memory_order_acquire) != nullptr
        && omega_visual_trace_active();
    const void* const state = component != nullptr
                                  ? static_cast<const std::byte*>(component) + 0x180U
                                  : nullptr;
    const std::uint64_t previousHash = trace ? safe_omega_visual_state_hash(state) : 0U;
    if (trace) { ++g_omegaVisualTraceDepth; }
    hooking::await_original(g_omegaVisualApplyOriginal)(component, stateKey);
    if (trace) { --g_omegaVisualTraceDepth; }
    if (!call.accepts_side_effects()) { return; }
    if (nav.enabled) { observe_first_cannon_preparation(component, nav.run); }
    if (nav.enabled) { observe_omega_arc_charge_carrier(component,nav.run); }
    if (!trace) { return; }
    const std::uint32_t observation =
        g_omegaVisualApplyObserved.fetch_add(1, std::memory_order_relaxed) + 1U;
    report_omega_visual_trace("apply",
                              observation,
                              component,
                              state,
                              nullptr,
                              -1,
                              0xFFFFFFFFU,
                               previousHash);
}

/** Records the exact client state materialized by one verified post-scene apply method. */
void report_omega_post_scene_apply(const char* kind,
                                   void* component,
                                   void* stateKey,
                                   std::size_t stateOffset,
                                   std::size_t stateBytes) noexcept {
    if (kind == nullptr || component == nullptr || stateBytes == 0U
        || stateBytes > kOmegaPostSceneMaximumStateBytes || !omega_visual_trace_active()) {
        return;
    }
    const std::uint32_t observation =
        g_omegaPostSceneApplyObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
    if (observation > kOmegaPostSceneTraceLimit) {
        return;
    }

    std::array<std::byte, kOmegaPostSceneMaximumStateBytes> state{};
    std::array<std::byte, 0x10U> key{};
    std::uint32_t datum = 0xFFFFFFFFU;
    bool stateReadable = false;
    bool keyReadable = false;
    __try {
        std::memcpy(&datum, component, sizeof datum);
        std::memcpy(state.data(),
                    static_cast<const std::byte*>(component) + stateOffset,
                    stateBytes);
        stateReadable = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        stateReadable = false;
    }
    if (stateKey != nullptr) {
        __try {
            std::memcpy(key.data(), stateKey, key.size());
            keyReadable = true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            keyReadable = false;
        }
    }

    const auto read32 = [](const auto& bytes, std::size_t offset) noexcept {
        std::uint32_t value = 0U;
        if (offset + sizeof value <= bytes.size()) {
            std::memcpy(&value, bytes.data() + offset, sizeof value);
        }
        return value;
    };
    const auto read8 = [](const auto& bytes, std::size_t offset) noexcept {
        return offset < bytes.size() ? std::to_integer<std::uint8_t>(bytes[offset]) : 0U;
    };
    const std::uint64_t hash =
        stateReadable ? hash_omega_runtime(state.data(), stateBytes) : 0U;
    const std::uintptr_t image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const std::uintptr_t caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    const std::uintptr_t callerRva = image != 0U && caller >= image ? caller - image : caller;

    std::array<char, core::log::kLineCapacity> line{};
    int written = std::snprintf(
        line.data(),
        line.size(),
        "ev=omega_post_scene_apply n=%u kind=%s component=%p datum=0x%08X state_key=%p "
        "key_readable=%s key=%08X,%08X,%08X,%08X offset=0x%zX bytes=%zu "
        "state_readable=%s hash=0x%016llX byte14=%u "
        "triggered=%u scene_completed=%u "
        "dwords=%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X,"
        "%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X caller=+%llX hex=",
        observation,
        kind,
        component,
        datum,
        stateKey,
        keyReadable ? "yes" : "no",
        read32(key, 0x00U),
        read32(key, 0x04U),
        read32(key, 0x08U),
        read32(key, 0x0CU),
        stateOffset,
        stateBytes,
        stateReadable ? "yes" : "no",
        static_cast<unsigned long long>(hash),
        read8(state, 0x14U),
        0U,
        0U,
        read32(state, 0x00U), read32(state, 0x04U),
        read32(state, 0x08U), read32(state, 0x0CU),
        read32(state, 0x10U), read32(state, 0x14U),
        read32(state, 0x18U), read32(state, 0x1CU),
        read32(state, 0x20U), read32(state, 0x24U),
        read32(state, 0x28U), read32(state, 0x2CU),
        read32(state, 0x30U), read32(state, 0x34U),
        read32(state, 0x38U), read32(state, 0x3CU),
        static_cast<unsigned long long>(callerRva));
    if (written <= 0) {
        return;
    }
    std::size_t used = (std::min)(static_cast<std::size_t>(written), line.size() - 1U);
    constexpr char kHex[] = "0123456789ABCDEF";
    if (stateReadable) {
        for (std::size_t index = 0U; index < stateBytes && used + 2U < line.size(); ++index) {
            const std::uint8_t value = std::to_integer<std::uint8_t>(state[index]);
            line[used++] = kHex[value >> 4U];
            line[used++] = kHex[value & 0x0FU];
        }
    }
    line[used] = '\0';
    core::log::write(core::log::Channel::client,
                     core::log::Level::info,
                     {line.data(), used});
}

__declspec(noinline) void __fastcall omega_gate_apply(void* component,
                                                       void* stateKey) noexcept {
    const OmegaPostSceneApply original =
        g_omegaGateApplyOriginal.load(std::memory_order_acquire);
    if (original == nullptr) {
        return;
    }
    // 80804F45 is exactly 0x200 bytes. Keep this observer inside the proven link-state window;
    // the +0x260 storage belongs to the larger generic 80804F06 family, not this gate class.
    report_omega_post_scene_apply("gate_before", component, stateKey, 0x1C0U, 0x18U);
    original(component, stateKey);
    report_omega_post_scene_apply("gate_after", component, stateKey, 0x1C0U, 0x18U);
}

__declspec(noinline) void __fastcall omega_engagement_apply(void* component,
                                                             void* stateKey) noexcept {
    const OmegaPostSceneApply original =
        g_omegaEngagementApplyOriginal.load(std::memory_order_acquire);
    if (original == nullptr) {
        return;
    }
    original(component, stateKey);
    report_omega_post_scene_apply("engagement", component, stateKey, 0x180U, 0xD0U);
}

__declspec(noinline) void __fastcall omega_point_apply(
    void* component,
    type31_capture::StateKey16* stateKey) noexcept {
    // This is the sole +B20640 replacement. Its full-call scope begins before any owner state or
    // trampoline is read and spans pre-copy, native forwarding, post-copy, and queue publication.
    hooking::CallGate::Scope call(g_type31PointCallGate);
    const type31_capture::NativeApply original =
        hooking::await_original(g_omegaPointApplyOriginal);
    const std::uint64_t captureEpoch =
        g_type31CaptureEpoch.load(std::memory_order_acquire);
    if (!call.accepts_side_effects() || captureEpoch == 0U) {
        original(component, stateKey);
        return;
    }

    type31_capture::PendingCapture pending{};
    type31_capture::CaptureTarget target{};
    type31_capture::CaptureBuildResult prepared =
        type31_capture::inspect_capture_target(component, stateKey, target);
    if (prepared == type31_capture::CaptureBuildResult::ready) {
        const std::uintptr_t image =
            reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        const std::uintptr_t caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
        const std::size_t imageBytes =
            g_type31MappedImageBytes.load(std::memory_order_acquire);
        const std::uintptr_t callerRva =
            image != 0U && caller >= image && caller - image < imageBytes
                ? caller - image
                : 0U;
        // No exact apply-ABI bridge to an activation, host session, run, correlation, or authority
        // generation exists. The all-absent snapshot is valid raw diagnostic evidence.
        constexpr type31_capture::CaptureContext kAbsentContext{};
        const type31_capture::CaptureMetadata metadata{
            captureEpoch, GetTickCount64(), GetCurrentThreadId(), callerRva};
        prepared = type31_capture::prepare_capture(
            pending, kAbsentContext, metadata, target, component, stateKey);
    } else if (prepared != type31_capture::CaptureBuildResult::unsupported_definition) {
        g_type31CaptureQueue.account_rejected();
    }

    // Native forwarding is unconditional and exactly once for every admission/gate outcome.
    original(component, stateKey);

    if (prepared != type31_capture::CaptureBuildResult::ready) {
        if (prepared != type31_capture::CaptureBuildResult::unsupported_definition) {
            // inspect failures were counted above; prepare failures reach this branch only after a
            // successful target gate and therefore need one separate rejection count.
            const bool inspectFailure = target.definition == 0U;
            if (!inspectFailure) {
                g_type31CaptureQueue.account_rejected();
            }
        }
        return;
    }

    type31_capture::CaptureRecord record{};
    const type31_capture::CaptureBuildResult finished =
        type31_capture::finish_capture(pending, component, false, record);
    if (finished == type31_capture::CaptureBuildResult::complete
        || finished == type31_capture::CaptureBuildResult::partial) {
        (void)g_type31CaptureQueue.try_push(record);
    } else {
        g_type31CaptureQueue.account_rejected();
    }
}

__declspec(noinline) void __fastcall omega_monitor_apply(void* component,
                                                          void* stateKey) noexcept {
    const OmegaPostSceneApply original =
        g_omegaMonitorApplyOriginal.load(std::memory_order_acquire);
    if (original == nullptr) {
        return;
    }
    original(component, stateKey);
    report_omega_post_scene_apply("monitor", component, stateKey, 0x180U, 0x0CU);
}

__declspec(noinline) std::uint16_t* __fastcall omega_visual_create(
    void* component,
    std::uint16_t* result,
    void* decodedState,
    std::int32_t visualIndex) noexcept {
    const OmegaVisualCreate original =
        g_omegaVisualCreateOriginal.load(std::memory_order_acquire);
    if (original == nullptr) {
        return result;
    }
    ++g_omegaVisualTraceDepth;
    std::uint16_t* const returned = original(component, result, decodedState, visualIndex);
    --g_omegaVisualTraceDepth;
    if (!omega_visual_trace_active()) {
        return returned;
    }
    std::uint32_t handle = 0xFFFFFFFFU;
    __try {
        if (returned != nullptr) {
            std::memcpy(&handle, returned, sizeof handle);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        handle = 0xFFFFFFFFU;
    }
    const std::uint32_t observation =
        g_omegaVisualCreateObserved.fetch_add(1, std::memory_order_relaxed) + 1U;
    report_omega_visual_trace("create",
                              observation,
                              component,
                              decodedState,
                              nullptr,
                              visualIndex,
                              handle,
                              0U);
    return returned;
}

__declspec(noinline) void __fastcall omega_visual_update(void* component,
                                                          std::int32_t* decodedState,
                                                          void* effectState) noexcept {
    const OmegaVisualUpdate original =
        g_omegaVisualUpdateOriginal.load(std::memory_order_acquire);
    if (original == nullptr) {
        return;
    }
    ++g_omegaVisualTraceDepth;
    original(component, decodedState, effectState);
    --g_omegaVisualTraceDepth;
    if (!omega_visual_trace_active()) {
        return;
    }
    const std::uint32_t observation =
        g_omegaVisualUpdateObserved.fetch_add(1, std::memory_order_relaxed) + 1U;
    report_omega_visual_trace("update",
                              observation,
                              component,
                              decodedState,
                              effectState,
                              -1,
                              0xFFFFFFFFU,
                              0U);
}

__declspec(noinline) std::int32_t __fastcall omega_visual_select(void* component) noexcept {
    const OmegaVisualSelect original =
        g_omegaVisualSelectOriginal.load(std::memory_order_acquire);
    if (original == nullptr) {
        return -1;
    }

    const OmegaVisualSelectorSnapshot before = capture_omega_visual_selector(component);
    const std::int32_t selected = original(component);
    if (!omega_visual_trace_active()) {
        return selected;
    }
    const std::int32_t slot = omega_portal_visual_slot(before.datum);
    if (slot < 0) {
        return selected;
    }
    const std::uint32_t observation =
        g_omegaVisualSelectObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
    if (observation > kOmegaVisualTraceLimit) {
        return selected;
    }
    const OmegaVisualSelectorSnapshot after = capture_omega_visual_selector(component);
    const char* route = before.authoredCount < 1
                            ? "no_authored_entries"
                        : before.stickySelection != 0U
                            ? "sticky_cached"
                        : before.explicitIndex >= 0
                                  && before.explicitIndex < before.authoredCount
                            ? "explicit_index"
                        : before.targetDatum == -1 && before.optionalRouteActive == 0U
                            ? "round_robin_available"
                            : "weighted_filtered";
    const std::uintptr_t caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    const std::uintptr_t image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const std::uintptr_t callerRva = image != 0U && caller >= image ? caller - image : caller;
    std::array<char, 1400> line{};
    const int written = std::snprintf(
        line.data(),
        line.size(),
        "ev=omega_visual_selector n=%u component=%p datum=0x%08X portal_slot=%d "
        "selected=%d route=%s caller=+%llX component_readable=%u authored_readable=%u "
        "authored_count=%lld table_rel=%lld optional_rel=%lld optional_active=%u "
        "target_datum=%d require_reference=%u use_mask=%u sticky=%u "
        "explicit_before=%d explicit_after=%d cached_before=%d cached_after=%d "
        "generation_before=%u generation_after=%u selected_generation_before=%u "
        "selected_generation_after=%u live_effect=0x%016llX "
        "mask=%08X,%08X,%08X,%08X ref=%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X "
        "candidate_refs=%016llX,%016llX,%016llX,%016llX triggered=%u completed=%u "
        "mutation=observe_only",
        observation,
        component,
        before.datum,
        slot,
        selected,
        route,
        static_cast<unsigned long long>(callerRva),
        before.componentReadable ? 1U : 0U,
        before.authoredReadable ? 1U : 0U,
        static_cast<long long>(before.authoredCount),
        static_cast<long long>(before.tableRelative),
        static_cast<long long>(before.optionalRelative),
        static_cast<unsigned int>(before.optionalRouteActive),
        before.targetDatum,
        static_cast<unsigned int>(before.requiresReference),
        static_cast<unsigned int>(before.usesCandidateMask),
        static_cast<unsigned int>(before.stickySelection),
        before.explicitIndex,
        after.explicitIndex,
        before.cachedIndex,
        after.cachedIndex,
        before.appliedGeneration,
        after.appliedGeneration,
        before.selectedGeneration,
        after.selectedGeneration,
        static_cast<unsigned long long>(after.liveEffect),
        before.candidateMask[0],
        before.candidateMask[1],
        before.candidateMask[2],
        before.candidateMask[3],
        before.referenceWords[0],
        before.referenceWords[1],
        before.referenceWords[2],
        before.referenceWords[3],
        before.referenceWords[4],
        before.referenceWords[5],
        before.referenceWords[6],
        before.referenceWords[7],
        static_cast<unsigned long long>(before.candidateReferences[0]),
        static_cast<unsigned long long>(before.candidateReferences[1]),
        static_cast<unsigned long long>(before.candidateReferences[2]),
        static_cast<unsigned long long>(before.candidateReferences[3]),
        0U,
        0U);
    if (written > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(),
                          (std::min)(static_cast<std::size_t>(written),
                                     line.size() - 1U)});
    }
    return selected;
}

void remember_omega_runtime(void* object,
                            std::uint32_t datum,
                            std::uint32_t schema,
                            void* runtime,
                            std::size_t bytes,
                            std::uint64_t hash) noexcept {
    if (runtime == nullptr || omega_runtime_label(schema) == nullptr || bytes == 0U
        || bytes > kOmegaRuntimeMaximumBytes) {
        return;
    }
    const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(runtime);
    AcquireSRWLockExclusive(&g_omegaRuntimeTrackLock);
    for (std::size_t index = 0; index < g_omegaRuntimeTrackCount; ++index) {
        OmegaRuntimeTrack& track = g_omegaRuntimeTracks[index];
        if (track.runtime == address) {
            track.object = reinterpret_cast<std::uintptr_t>(object);
            track.datum = datum;
            track.schema = schema;
            track.bytes = bytes;
            track.hash = hash;
            ReleaseSRWLockExclusive(&g_omegaRuntimeTrackLock);
            return;
        }
    }
    if (g_omegaRuntimeTrackCount < g_omegaRuntimeTracks.size()) {
        g_omegaRuntimeTracks[g_omegaRuntimeTrackCount++] = {
            reinterpret_cast<std::uintptr_t>(object), address, datum, schema, bytes, hash};
    }
    ReleaseSRWLockExclusive(&g_omegaRuntimeTrackLock);
}

void reset_omega_runtime_tracks() noexcept {
    AcquireSRWLockExclusive(&g_omegaRuntimeTrackLock);
    g_omegaRuntimeTracks = {};
    g_omegaRuntimeTrackCount = 0U;
    ReleaseSRWLockExclusive(&g_omegaRuntimeTrackLock);
}

/**
 * Confirms that Omega's opening scene records survived native publish apply. The changed-object
 * list proves framing; resolving and hashing each component runtime proves the corresponding body
 * reached the client's authored-state storage. This remains observe-only.
 */
void report_omega_scene_runtime_changes(void* changedObjects) noexcept {
    constexpr std::uint32_t kOmegaSceneSchema = 0x8080626BU;
    constexpr std::size_t kOmegaSceneStateBytes = 0xD4U;
    struct SceneSchema final {
        std::uint32_t schema;
        const char* label;
    };
    constexpr std::array<SceneSchema, 7> kSceneSchemas{{
        {0x8080626BU, "scene"},
        {0x8080992FU, "visual"},
        {0x8080954BU, "hold"},
        {0x80804F48U, "gate"},
        {0x808094F1U, "engagement"},
        {0x80809524U, "point"},
        {0x80809532U, "monitor"},
    }};
    if (changedObjects == nullptr) {
        return;
    }
    state::activity::forced::ForcedDestination forced{};
    state::activity::forced::snapshot(forced);
    const std::string_view package(forced.packageName.data(), forced.packageNameLength);
    if (!state::activity::forced::override_active() || package != "mission_scot") {
        return;
    }

    const std::uint32_t count = (std::min)(changed_object_count(changedObjects), 128U);
    for (std::uint32_t index = 0; index < count; ++index) {
        std::uint32_t datum = 0xFFFFFFFFU;
        std::uint32_t schema = 0U;
        std::uint32_t component = 0xFFFFFFFFU;
        std::byte* object = nullptr;
        void* runtime = nullptr;
        std::array<std::byte, kOmegaRuntimeMaximumBytes> state{};
        std::array<std::uint64_t, 8> words{};
        std::size_t runtimeBytes = sizeof words;
        const char* label = nullptr;
        bool readable = false;
        __try {
            const auto* const entry = static_cast<const std::byte*>(changedObjects) + 0x20
                                      + static_cast<std::size_t>(index) * 32U;
            std::memcpy(&datum, entry, sizeof datum);
            object = resolve_changed_object(datum);
            if (object == nullptr) {
                continue;
            }
            std::memcpy(&schema, object + 0x0C, sizeof schema);
            for (const SceneSchema& candidate : kSceneSchemas) {
                if (candidate.schema == schema) {
                    label = candidate.label;
                    break;
                }
            }
            if (label == nullptr) {
                continue;
            }
            auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
            if (image != nullptr) {
                const auto componentLookup = reinterpret_cast<ObjectComponentLookup>(
                    image + kObjectComponentLookupRva);
                const auto runtimeResolve = reinterpret_cast<ObjectRuntimeResolve>(
                    image + kObjectRuntimeResolveRva);
                std::uint64_t componentValue = 0;
                (void)componentLookup(&componentValue, static_cast<std::int32_t>(schema));
                component = static_cast<std::uint32_t>(componentValue);
                runtime = runtimeResolve(object, component);
                if (runtime != nullptr) {
                    runtimeBytes = schema == kOmegaSceneSchema
                                       ? kOmegaSceneStateBytes
                                       : (schema == 0x8080992FU
                                              ? kOmegaVisualDecodedStateBytes
                                              : sizeof words);
                    std::memcpy(state.data(), runtime, runtimeBytes);
                    std::memcpy(words.data(), state.data(), sizeof words);
                    readable = true;
                }
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            readable = false;
        }

        const std::uint32_t observation =
            g_omegaSceneRuntimeObserved.fetch_add(1, std::memory_order_relaxed) + 1U;
        if (observation > 64U) {
            return;
        }
        const std::uint64_t hash =
            readable ? hash_omega_runtime(state.data(), runtimeBytes) : 0U;
        if (readable) {
            remember_omega_runtime(object, datum, schema, runtime, runtimeBytes, hash);
        }
        const auto read32 = [&state](std::size_t offset) noexcept {
            std::uint32_t value = 0U;
            if (offset + sizeof value <= state.size()) {
                std::memcpy(&value, state.data() + offset, sizeof value);
            }
            return value;
        };
        // +B41DD0 treats decoded +0x08 as N (an actor/count field), then separately publishes
        // component +0x260 = (N != 0).  This recorder has only the decoded body, so retain N as
        // raw observation and do not classify it as the component's live active latch.
        const std::uint32_t decodedN = read32(0x08);

        std::array<char, core::log::kLineCapacity> line{};
        const int written = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=omega_scene_runtime n=%u entry=%u datum=0x%08X object=%p schema=0x%08X label=%s visual_slot=%d component=0x%08X runtime=%p readable=%s bytes=%zu hash=0x%016llX decoded_n_at_08=%u active_latch=not_captured scene_value=0x%08X d0=0x%08X d1=0x%08X d2=0x%08X d3=0x%08X tail90=0x%08X tail94=0x%08X tail98=0x%08X tail9C=0x%08X visual_dwords=%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X q0=0x%016llX q1=0x%016llX q2=0x%016llX q3=0x%016llX q4=0x%016llX q5=0x%016llX q6=0x%016llX q7=0x%016llX mutation=observe_only",
            observation,
            index,
            datum,
            object,
            schema,
            label,
            schema == 0x8080992FU ? omega_portal_visual_slot(datum) : -1,
            component,
            runtime,
            readable ? "yes" : "no",
            runtimeBytes,
            static_cast<unsigned long long>(hash),
            decodedN,
            read32(0x4C),
            read32(0x00),
            read32(0x04),
            read32(0x08),
            read32(0x0C),
            read32(0x90),
            read32(0x94),
            read32(0x98),
            read32(0x9C),
            read32(0x00),
            read32(0x04),
            read32(0x08),
            read32(0x0C),
            read32(0x10),
            read32(0x14),
            read32(0x18),
            read32(0x1C),
            read32(0x20),
            read32(0x24),
            read32(0x28),
            read32(0x2C),
            read32(0x30),
            read32(0x34),
            read32(0x38),
            read32(0x3C),
            static_cast<unsigned long long>(words[0]),
            static_cast<unsigned long long>(words[1]),
            static_cast<unsigned long long>(words[2]),
            static_cast<unsigned long long>(words[3]),
            static_cast<unsigned long long>(words[4]),
            static_cast<unsigned long long>(words[5]),
            static_cast<unsigned long long>(words[6]),
            static_cast<unsigned long long>(words[7]));
        if (written > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(),
                              static_cast<std::size_t>(written) < line.size()
                                  ? static_cast<std::size_t>(written)
                                  : line.size() - 1U});
        }
    }
}

/**
 * Finds schema 0x80807EC9 in the native changed-object list and reports the fields that make
 * squad_ikora actionable. Scanning by schema avoids assuming that five roster groups always
 * flatten to the same entry index.
 */
void report_omega_spawner_runtime_change(void* changedObjects) noexcept {
    constexpr std::uint32_t kSpawnerSchema = 0x80807EC9U;
    constexpr std::size_t kSpawnerStateBytes = 0xC4U;
    constexpr std::uint32_t kOmegaSquadOwner = 0xD00142CFU;
    constexpr std::uint8_t kOmegaSquadType = 66U;
    constexpr std::uint16_t kOmegaSquadIndex = 0U;
    if (changedObjects == nullptr) {
        return;
    }
    const std::uint32_t count = (std::min)(changed_object_count(changedObjects), 128U);
    for (std::uint32_t index = 0; index < count; ++index) {
        std::uint32_t datum = 0xFFFFFFFFU;
        std::uint32_t schema = 0U;
        std::uint32_t component = 0xFFFFFFFFU;
        std::byte* object = nullptr;
        void* runtime = nullptr;
        std::array<std::byte, kSpawnerStateBytes> state{};
        bool readable = false;
        __try {
            const auto* const entry = static_cast<const std::byte*>(changedObjects) + 0x20
                                      + static_cast<std::size_t>(index) * 32U;
            std::memcpy(&datum, entry, sizeof datum);
            object = resolve_changed_object(datum);
            if (object == nullptr) {
                continue;
            }
            std::memcpy(&schema, object + 0x0C, sizeof schema);
            if (schema != kSpawnerSchema) {
                continue;
            }
            g_omegaSpawnerObject.store(reinterpret_cast<std::uintptr_t>(object),
                                       std::memory_order_release);
            auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
            const auto componentLookup = reinterpret_cast<ObjectComponentLookup>(
                image + kObjectComponentLookupRva);
            const auto runtimeResolve = reinterpret_cast<ObjectRuntimeResolve>(
                image + kObjectRuntimeResolveRva);
            std::uint64_t componentValue = 0;
            (void)componentLookup(&componentValue, static_cast<std::int32_t>(schema));
            component = static_cast<std::uint32_t>(componentValue);
            runtime = runtimeResolve(object, component);
            if (runtime != nullptr) {
                std::memcpy(state.data(), runtime, state.size());
                g_omegaSpawnerRuntime.store(reinterpret_cast<std::uintptr_t>(runtime),
                                            std::memory_order_release);
                readable = true;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            readable = false;
        }

        std::uint64_t hash = 1469598103934665603ULL;
        if (readable) {
            for (const std::byte value : state) {
                hash ^= std::to_integer<std::uint8_t>(value);
                hash *= 1099511628211ULL;
            }
        } else {
            hash = 0U;
        }
        const bool first =
            !g_omegaSpawnerRuntimeReported.exchange(true, std::memory_order_acq_rel);
        const std::uint64_t previous =
            g_omegaSpawnerRuntimeHash.exchange(hash, std::memory_order_acq_rel);
        if (!first && previous == hash) {
            return;
        }

        const auto read32 = [&state](std::size_t offset) noexcept {
            std::uint32_t value = 0U;
            if (offset + sizeof value <= state.size()) {
                std::memcpy(&value, state.data() + offset, sizeof value);
            }
            return value;
        };
        const auto read16 = [&state](std::size_t offset) noexcept {
            std::uint16_t value = 0U;
            if (offset + sizeof value <= state.size()) {
                std::memcpy(&value, state.data() + offset, sizeof value);
            }
            return value;
        };
        const auto read8 = [&state](std::size_t offset) noexcept {
            return offset < state.size() ? std::to_integer<std::uint8_t>(state[offset]) : 0U;
        };
        const bool squadValid = read32(0x98) == kOmegaSquadOwner
                                && read8(0x9C) == kOmegaSquadType
                                && read16(0x9E) == kOmegaSquadIndex;
        std::array<char, core::log::kLineCapacity> line{};
        const int written = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=omega_spawner_runtime entry=%u datum=0x%08X object=%p schema=0x%08X component=0x%08X runtime=%p readable=%s first=%u previous_hash=0x%016llX hash=0x%016llX requested=%u counts=%u,%u,%u,%u,%u,%u generation=%u target=%08X/%u/%u squad=%08X/%u/%u squad_valid=%s expected=%08X/%u/%u target_gen=%u active=%u mode=%u name=0x%08X mutation=observe_only",
            index,
            datum,
            object,
            schema,
            component,
            runtime,
            readable ? "yes" : "no",
            first ? 1U : 0U,
            static_cast<unsigned long long>(previous),
            static_cast<unsigned long long>(hash),
            read32(0x2C),
            read32(0x30),
            read32(0x34),
            read32(0x38),
            read32(0x3C),
            read32(0x40),
            read32(0x44),
            read32(0x7C),
            read32(0x90),
            read8(0x94),
            read16(0x96),
            read32(0x98),
            read8(0x9C),
            read16(0x9E),
            squadValid ? "yes" : "no",
            kOmegaSquadOwner,
            static_cast<unsigned>(kOmegaSquadType),
            static_cast<unsigned>(kOmegaSquadIndex),
            read32(0xB8),
            read8(0xBC),
            read8(0xBD),
            read32(0xC0));
        if (written > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(),
                              static_cast<std::size_t>(written) < line.size()
                                  ? static_cast<std::size_t>(written)
                                  : line.size() - 1U});
        }
        return;
    }
}

/**
 * Reports the exact changed-object list while an Omega opening authority message is being applied.
 * The generic publish recorder is intentionally short-lived and is exhausted during initial load;
 * this recorder is package-gated so it also captures the new pre-trigger preload packets.
 */
void report_omega_trigger_publish_pass(const char* phase,
                                       const char* moment,
                                       void* changedObjects) noexcept {
    constexpr std::uint32_t kSpawnerSchema = 0x80807EC9U;
    constexpr std::uint32_t kScriptSchema = 0x80809919U;
    constexpr std::uint32_t kDirectorSchema = 0x808099BFU;
    constexpr std::uint32_t kSceneSchema = 0x8080626BU;
    constexpr std::uint32_t kVisualSchema = 0x8080992FU;
    constexpr std::uint32_t kHoldSchema = 0x8080954BU;
    constexpr std::uint32_t kGateSchema = 0x80804F48U;
    constexpr std::uint32_t kEngagementSchema = 0x808094F1U;
    constexpr std::uint32_t kPointSchema = 0x80809524U;
    constexpr std::uint32_t kMonitorSchema = 0x80809532U;
    constexpr std::uint32_t kMissingIndex = 0xFFFFFFFFU;
    constexpr std::size_t kSpawnerStateBytes = 0xC4U;
    state::activity::forced::ForcedDestination forced{};
    state::activity::forced::snapshot(forced);
    const std::string_view package(forced.packageName.data(), forced.packageNameLength);
    if (phase == nullptr || moment == nullptr || !state::activity::forced::override_active()
        || package != "mission_scot" || !state::activity::mission_seed_armed()) {
        return;
    }
    const std::uint32_t publishObservation =
        g_omegaTriggeredPublishObserved.fetch_add(1, std::memory_order_relaxed) + 1U;
    if (publishObservation > 96U) {
        return;
    }

    const std::uint32_t count = (std::min)(changed_object_count(changedObjects), 128U);
    std::uint32_t resolved = 0U;
    std::uint32_t spawnerIndex = kMissingIndex;
    std::uint32_t spawnerDatum = 0xFFFFFFFFU;
    std::uint32_t scriptIndex = kMissingIndex;
    std::uint32_t directorIndex = kMissingIndex;
    std::uint32_t sceneIndex = kMissingIndex;
    std::uint32_t gateIndex = kMissingIndex;
    std::uint32_t engagementIndex = kMissingIndex;
    std::uint32_t visualCount = 0U;
    std::uint32_t holdCount = 0U;
    std::uint32_t pointCount = 0U;
    std::uint32_t monitorCount = 0U;
    std::array<std::uint32_t, 4> firstData{0xFFFFFFFFU,
                                           0xFFFFFFFFU,
                                           0xFFFFFFFFU,
                                           0xFFFFFFFFU};
    std::array<std::uint32_t, 4> firstSchemas{};
    for (std::uint32_t index = 0; index < count; ++index) {
        std::uint32_t datum = 0xFFFFFFFFU;
        std::uint32_t schema = 0U;
        __try {
            const auto* const entry = static_cast<const std::byte*>(changedObjects) + 0x20
                                      + static_cast<std::size_t>(index) * 32U;
            std::memcpy(&datum, entry, sizeof datum);
            std::byte* const object = resolve_changed_object(datum);
            if (object != nullptr) {
                std::memcpy(&schema, object + 0x0C, sizeof schema);
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            datum = 0xFFFFFFFFU;
            schema = 0U;
        }
        if (index < firstData.size()) {
            firstData[index] = datum;
            firstSchemas[index] = schema;
        }
        if (schema == 0U) {
            continue;
        }
        ++resolved;
        if (schema == kSpawnerSchema) {
            spawnerIndex = index;
            spawnerDatum = datum;
        } else if (schema == kScriptSchema) {
            scriptIndex = index;
        } else if (schema == kDirectorSchema) {
            directorIndex = index;
        } else if (schema == kSceneSchema) {
            sceneIndex = index;
        } else if (schema == kVisualSchema) {
            ++visualCount;
        } else if (schema == kHoldSchema) {
            ++holdCount;
        } else if (schema == kGateSchema) {
            gateIndex = index;
        } else if (schema == kEngagementSchema) {
            engagementIndex = index;
        } else if (schema == kPointSchema) {
            ++pointCount;
        } else if (schema == kMonitorSchema) {
            ++monitorCount;
        }
    }

    std::array<std::byte, kSpawnerStateBytes> state{};
    bool runtimeReadable = false;
    const auto runtimeAddress = g_omegaSpawnerRuntime.load(std::memory_order_acquire);
    const auto objectAddress = g_omegaSpawnerObject.load(std::memory_order_acquire);
    const AuthorityFilterSnapshot authority = snapshot_authority_filter(
        reinterpret_cast<const std::byte*>(objectAddress));
    __try {
        if (runtimeAddress != 0U) {
            std::memcpy(state.data(), reinterpret_cast<const void*>(runtimeAddress), state.size());
            runtimeReadable = true;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        runtimeReadable = false;
    }
    std::uint64_t runtimeHash = 0U;
    if (runtimeReadable) {
        runtimeHash = 1469598103934665603ULL;
        for (const std::byte value : state) {
            runtimeHash ^= std::to_integer<std::uint8_t>(value);
            runtimeHash *= 1099511628211ULL;
        }
    }
    const auto read32 = [&state](std::size_t offset) noexcept {
        std::uint32_t value = 0U;
        if (offset + sizeof value <= state.size()) {
            std::memcpy(&value, state.data() + offset, sizeof value);
        }
        return value;
    };
    const auto read16 = [&state](std::size_t offset) noexcept {
        std::uint16_t value = 0U;
        if (offset + sizeof value <= state.size()) {
            std::memcpy(&value, state.data() + offset, sizeof value);
        }
        return value;
    };
    const auto read8 = [&state](std::size_t offset) noexcept {
        return offset < state.size() ? std::to_integer<std::uint8_t>(state[offset]) : 0U;
    };

    std::array<char, core::log::kLineCapacity> line{};
    const int written = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=omega_trigger_authority_publish n=%u phase=%s moment=%s count=%u resolved=%u spawner_index=%u spawner_datum=0x%08X script_index=%u director_index=%u scene_index=%u gate_index=%u engagement_index=%u visuals=%u holds=%u points=%u monitors=%u first=%08X/%08X,%08X/%08X,%08X/%08X,%08X/%08X object=%p object_readable=%s owner=%u current_authority=%u owner_match=%u owner_global=%u initialized=%u dirty=%u authority_readable=%s runtime=%p runtime_readable=%s runtime_hash=0x%016llX requested=%u squad=%08X/%u/%u target_gen=%u active=%u mode=%u mutation=observe_only",
        publishObservation,
        phase,
        moment,
        count,
        resolved,
        spawnerIndex,
        spawnerDatum,
        scriptIndex,
        directorIndex,
        sceneIndex,
        gateIndex,
        engagementIndex,
        visualCount,
        holdCount,
        pointCount,
        monitorCount,
        firstData[0],
        firstSchemas[0],
        firstData[1],
        firstSchemas[1],
        firstData[2],
        firstSchemas[2],
        firstData[3],
        firstSchemas[3],
        reinterpret_cast<void*>(objectAddress),
        authority.objectReadable ? "yes" : "no",
        authority.owner,
        authority.current,
        authority.owner == authority.current ? 1U : 0U,
        authority.owner > 0x3FU ? 1U : 0U,
        static_cast<unsigned>(authority.initialized),
        static_cast<unsigned>(authority.dirty),
        authority.accessorReadable ? "yes" : "no",
        reinterpret_cast<void*>(runtimeAddress),
        runtimeReadable ? "yes" : "no",
        static_cast<unsigned long long>(runtimeHash),
        // +516340 returns this exact runtime pointer. Native +4E8FB0 consumes the requested-count
        // record at +0x2C/+0x30; there is no second expanded layout.
        read32(0x2C),
        read32(0x98),
        static_cast<unsigned>(read8(0x9C)),
        static_cast<unsigned>(read16(0x9E)),
        read32(0xB8),
        static_cast<unsigned>(read8(0xBC)),
        static_cast<unsigned>(read8(0xBD)));
    if (written > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(),
                          static_cast<std::size_t>(written) < line.size()
                              ? static_cast<std::size_t>(written)
                              : line.size() - 1U});
    }
}

void report_publish_pass(const char* phase,
                         std::uint32_t before,
                         std::uint32_t after,
                         void* roster,
                         void* changedObjects) noexcept {
    state::activity::forced::ForcedDestination forced{};
    std::string_view package{};
    if (!opening_is_forced(forced, package)) {
        return;
    }
    const std::uint32_t observation =
        g_publishObserved.fetch_add(1, std::memory_order_relaxed) + 1U;
    if (observation > 64U) {
        return;
    }
    std::array<char, core::log::kLineCapacity> line{};
    const int written = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_authority_publish n=%u phase=%s count_before=%u count_after=%u roster=%p list=%p forced=%.*s",
        observation,
        phase,
        before,
        after,
        roster,
        changedObjects,
        static_cast<int>(package.size()),
        package.data());
    if (written > 0) {
        const std::size_t length = static_cast<std::size_t>(written) < line.size()
                                       ? static_cast<std::size_t>(written)
                                       : line.size() - 1;
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), length});
    }
}

__declspec(noinline) void __fastcall authority_publish_collect(void* roster,
                                                                void* changedObjects) noexcept {
    const AuthorityPublishPass original =
        g_authorityPublishCollectOriginal.load(std::memory_order_acquire);
    if (original == nullptr) {
        return;
    }
    const std::uint32_t before = changed_object_count(changedObjects);
    report_omega_trigger_publish_pass("collect", "before", changedObjects);
    original(roster, changedObjects);
    report_changed_objects(changedObjects);
    report_omega_trigger_publish_pass("collect", "after", changedObjects);
    report_publish_pass("collect",
                        before,
                        changed_object_count(changedObjects),
                        roster,
                        changedObjects);
}

__declspec(noinline) void __fastcall authority_publish_apply(void* roster,
                                                              void* changedObjects) noexcept {
    const AuthorityPublishPass original =
        g_authorityPublishApplyOriginal.load(std::memory_order_acquire);
    if (original == nullptr) {
        return;
    }
    const bool inspectOmegaState = g_omegaTriggeredSchemaApply != 0U;
    const std::uint32_t stateObservation =
        inspectOmegaState
            ? g_omegaApplyStateObserved.fetch_add(1, std::memory_order_relaxed) + 1U
            : 0U;
    const OmegaSpawnerStateSnapshot stateBefore =
        inspectOmegaState ? capture_omega_spawner_state() : OmegaSpawnerStateSnapshot{};
    const std::uint32_t before = changed_object_count(changedObjects);
    report_omega_trigger_publish_pass("apply", "before", changedObjects);
    original(roster, changedObjects);
    const OmegaSpawnerStateSnapshot stateAfter =
        inspectOmegaState ? capture_omega_spawner_state() : OmegaSpawnerStateSnapshot{};
    if (inspectOmegaState) {
        report_omega_spawner_state_snapshot("publish_apply",
                                            "before",
                                            stateObservation,
                                            stateBefore);
        report_omega_spawner_state_snapshot("publish_apply",
                                            "after",
                                            stateObservation,
                                            stateAfter);
    }
    report_activity_script_post_apply(changedObjects);
    report_mission_runtime_changes(changedObjects);
    report_omega_scene_runtime_changes(changedObjects);
    report_omega_spawner_runtime_change(changedObjects);
    report_omega_trigger_publish_pass("apply", "after", changedObjects);
    report_publish_pass("apply",
                        before,
                        changed_object_count(changedObjects),
                        roster,
                        changedObjects);
}

__declspec(noinline) void __fastcall authority_publish_finalize(void* roster,
                                                                 void* changedObjects) noexcept {
    const AuthorityPublishPass original =
        g_authorityPublishFinalizeOriginal.load(std::memory_order_acquire);
    if (original == nullptr) {
        return;
    }
    const std::uint32_t before = changed_object_count(changedObjects);
    report_omega_trigger_publish_pass("finalize", "before", changedObjects);
    original(roster, changedObjects);
    const std::uint32_t after = changed_object_count(changedObjects);
    report_omega_trigger_publish_pass("finalize", "after", changedObjects);
    report_publish_pass("finalize", before, after, roster, changedObjects);
}

/** Reports whether the native all-object validator allows the publish pass to run. */
/**
 * Records the parent group boundary that decides whether its per-object loop executes. This is
 * intentionally observe-only: the key/header/result tuple distinguishes an unresolved roster
 * group from a malformed child block without changing either path.
 */
__declspec(noinline) std::uint64_t __fastcall authority_group_decode(void* roster,
                                                                     void* stream) noexcept {
    const AuthorityGroupDecode original =
        g_authorityGroupDecodeOriginal.load(std::memory_order_acquire);
    if (original == nullptr) {
        return 0U;
    }

    state::activity::forced::ForcedDestination forced{};
    std::string_view package{};
    const bool inspect = g_omegaTriggeredSchemaApply != 0U
                         && opening_is_forced(forced, package)
                         && package == "mission_scot";
    const std::uint32_t beforeBits = inspect ? consumed_bits(stream) : 0U;
    if (inspect) {
        g_groupDecodeActive = true;
        g_groupDecodeSawOmega = false;
        g_groupDecodeReadCount = 0U;
        g_groupDecodeChildObjects = 0U;
    }
    const std::uint64_t result = original(roster, stream);
    if (!inspect) {
        return result;
    }
    g_groupDecodeActive = false;

    const std::uint32_t observation =
        g_omegaGroupDecodeObserved.fetch_add(1, std::memory_order_relaxed) + 1U;
    if (observation > 32U) {
        return result;
    }
    const std::uint32_t afterBits = consumed_bits(stream);
    std::array<char, core::log::kLineCapacity> line{};
    int used = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=omega_phase_two_group_decode n=%u result=%s raw=0x%016llX "
        "bit_before=%u bit_after=%u consumed=%u reads=%u saw_omega=%u child_objects=%u fields=",
        observation,
        (result & 0xFFU) != 0U ? "accepted" : "rejected",
        static_cast<unsigned long long>(result),
        beforeBits,
        afterBits,
        afterBits >= beforeBits ? afterBits - beforeBits : 0U,
        g_groupDecodeReadCount,
        static_cast<unsigned>(g_groupDecodeSawOmega),
        g_groupDecodeChildObjects);
    const std::uint32_t shown = (std::min)(g_groupDecodeReadCount, 16U);
    for (std::uint32_t index = 0;
         used > 0 && index < shown && static_cast<std::size_t>(used) < line.size();
         ++index) {
        const ObjectDecodeRead& read = g_groupDecodeReads[index];
        const int appended = std::snprintf(
            line.data() + used,
            line.size() - static_cast<std::size_t>(used),
            "%s%c%u:0x%X@%u-%u",
            index == 0U ? "" : ",",
            read.boolean ? 'b' : 'u',
            read.width,
            read.value,
            read.before,
            read.after);
        if (appended <= 0) {
            break;
        }
        used += appended;
    }
    if (used > 0) {
        const std::size_t length = static_cast<std::size_t>(used) < line.size()
                                       ? static_cast<std::size_t>(used)
                                       : line.size() - 1U;
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), length});
    }
    return result;
}

/**
 * Records the exact field sequence for D00142CF's post-trigger object block. The outer decoder can
 * accept a packet even when its trailing object is skipped, so observing only the outer return is
 * insufficient to distinguish a wire-shape fault from a lookup/filter fault.
 */
__declspec(noinline) std::uint64_t __fastcall authority_object_decode(void* roster,
                                                                      void* stream) noexcept {
    const AuthorityObjectDecode original =
        g_authorityObjectDecodeOriginal.load(std::memory_order_acquire);
    if (original == nullptr) {
        return 0U;
    }
    if (g_groupDecodeActive) {
        ++g_groupDecodeChildObjects;
    }

    state::activity::forced::ForcedDestination forced{};
    std::string_view package{};
    const bool inspect = g_omegaTriggeredSchemaApply != 0U
                         && opening_is_forced(forced, package)
                         && package == "mission_scot";
    const std::uint32_t beforeBits = inspect ? consumed_bits(stream) : 0U;
    const auto objectAddress = g_omegaSpawnerObject.load(std::memory_order_acquire);
    const AuthorityFilterSnapshot beforeFilter =
        inspect ? snapshot_authority_filter(reinterpret_cast<const std::byte*>(objectAddress))
                : AuthorityFilterSnapshot{};
    const OmegaSpawnerStateSnapshot stateBefore =
        inspect ? capture_omega_spawner_state() : OmegaSpawnerStateSnapshot{};

    if (inspect) {
        g_objectDecodeActive = true;
        g_objectDecodeFirst32Set = false;
        g_objectDecodeSawOmega = false;
        g_objectDecodeFirst32 = 0U;
        g_objectDecodeReadCount = 0U;
    }
    const std::uint64_t result = original(roster, stream);
    if (!inspect) {
        return result;
    }
    g_objectDecodeActive = false;

    const std::uint32_t afterBits = consumed_bits(stream);
    const AuthorityFilterSnapshot afterFilter = snapshot_authority_filter(
        reinterpret_cast<const std::byte*>(objectAddress));
    const OmegaSpawnerStateSnapshot stateAfter = capture_omega_spawner_state();
    const bool omega = g_objectDecodeSawOmega;
    if (!omega) {
        return result;
    }

    const std::uint32_t observation =
        g_omegaObjectDecodeObserved.fetch_add(1, std::memory_order_relaxed) + 1U;
    if (observation > 8U) {
        return result;
    }
    const std::uint32_t stateObservation =
        g_omegaObjectStateObserved.fetch_add(1, std::memory_order_relaxed) + 1U;
    report_omega_spawner_state_snapshot("object_decode",
                                        "before",
                                        stateObservation,
                                        stateBefore);
    report_omega_spawner_state_snapshot("object_decode",
                                        "after",
                                        stateObservation,
                                        stateAfter);
    std::array<char, core::log::kLineCapacity> line{};
    int used = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=omega_spawner_object_decode n=%u result=%s raw=0x%016llX "
        "bit_before=%u bit_after=%u consumed=%u reads=%u first32=0x%08X object=%p "
        "owner=%u->%u current=%u->%u initialized=%u->%u dirty=%u->%u fields=",
        observation,
        (result & 0xFFU) != 0U ? "accepted" : "rejected",
        static_cast<unsigned long long>(result),
        beforeBits,
        afterBits,
        afterBits >= beforeBits ? afterBits - beforeBits : 0U,
        g_objectDecodeReadCount,
        g_objectDecodeFirst32,
        reinterpret_cast<void*>(objectAddress),
        beforeFilter.owner,
        afterFilter.owner,
        beforeFilter.current,
        afterFilter.current,
        static_cast<unsigned>(beforeFilter.initialized),
        static_cast<unsigned>(afterFilter.initialized),
        static_cast<unsigned>(beforeFilter.dirty),
        static_cast<unsigned>(afterFilter.dirty));
    for (std::uint32_t index = 0;
         used > 0 && index < g_objectDecodeReadCount
         && static_cast<std::size_t>(used) < line.size();
         ++index) {
        const ObjectDecodeRead& read = g_objectDecodeReads[index];
        const int appended = std::snprintf(
            line.data() + used,
            line.size() - static_cast<std::size_t>(used),
            "%s%c%u:0x%X@%u-%u",
            index == 0U ? "" : ",",
            read.boolean ? 'b' : 'u',
            read.width,
            read.value,
            read.before,
            read.after);
        if (appended <= 0) {
            break;
        }
        used += appended;
    }
    if (used > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(),
                          static_cast<std::size_t>(used) < line.size()
                              ? static_cast<std::size_t>(used)
                              : line.size() - 1U});
    }
    return result;
}

__declspec(noinline) bool __fastcall authority_validation(void* roster) noexcept {
    const AuthorityValidation original =
        g_authorityValidationOriginal.load(std::memory_order_acquire);
    if (original == nullptr) {
        return false;
    }

    const bool accepted = original(roster);
    state::activity::forced::ForcedDestination forced{};
    std::string_view package{};
    const bool inspect = opening_is_forced(forced, package);
    const std::uint32_t observation =
        inspect ? g_validationObserved.fetch_add(1, std::memory_order_relaxed) + 1U : 0U;
    if (observation != 0U && observation <= 16U) {
        std::uintptr_t container = 0;
        std::uintptr_t manager = 0;
        std::uint8_t ready = 0;
        std::uint8_t enabled = 0;
        __try {
            if (roster != nullptr) {
                const auto* const bytes = static_cast<const std::byte*>(roster);
                std::memcpy(&container, bytes + 0x808, sizeof container);
                std::memcpy(&manager, bytes + 0x818, sizeof manager);
                std::memcpy(&ready, bytes + 0x10E7F, sizeof ready);
                std::memcpy(&enabled, bytes + 0x10E80, sizeof enabled);
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            container = 0;
            manager = 0;
            ready = 0;
            enabled = 0;
        }
        std::array<char, core::log::kLineCapacity> line{};
        const int written = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_authority_validation n=%u result=%s roster=%p container=%p manager=%p ready=%u enabled=%u forced=%.*s",
            observation,
            accepted ? "accepted" : "rejected",
            roster,
            reinterpret_cast<void*>(container),
            reinterpret_cast<void*>(manager),
            static_cast<unsigned>(ready),
            static_cast<unsigned>(enabled),
            static_cast<int>(package.size()),
            package.data());
        if (written > 0) {
            const std::size_t length = static_cast<std::size_t>(written) < line.size()
                                           ? static_cast<std::size_t>(written)
                                           : line.size() - 1;
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(), length});
        }
    }
    return accepted;
}

/** Reports the gate object and the two local latches surrounding one authority-schema apply. */
__declspec(noinline) bool __fastcall authority_schema_apply(void* roster,
                                                             void* stream,
                                                             void* event) noexcept {
    const AuthoritySchemaApply original =
        g_authoritySchemaApplyOriginal.load(std::memory_order_acquire);
    if (original == nullptr) {
        return false;
    }

    state::activity::forced::ForcedDestination forced{};
    std::string_view package{};
    const bool inspect = opening_is_forced(forced, package);
    const std::uint32_t observation =
        inspect ? g_schemaApplyObserved.fetch_add(1, std::memory_order_relaxed) + 1U : 0U;
    std::uint8_t readyBefore = 0;
    std::uint8_t enabledBefore = 0;
    std::uintptr_t manager = 0;
    std::uintptr_t vtable = 0;
    std::uintptr_t gateTarget = 0;
    if (observation != 0U && observation <= 12U && roster != nullptr) {
        __try {
            const auto* const bytes = static_cast<const std::byte*>(roster);
            std::memcpy(&readyBefore, bytes + 0x10E7F, sizeof readyBefore);
            std::memcpy(&enabledBefore, bytes + 0x10E80, sizeof enabledBefore);
            std::memcpy(&manager, bytes + 0x818, sizeof manager);
            if (manager != 0) {
                std::memcpy(&vtable, reinterpret_cast<const void*>(manager), sizeof vtable);
                if (vtable != 0) {
                    std::memcpy(&gateTarget,
                                reinterpret_cast<const void*>(vtable + 0x20),
                                sizeof gateTarget);
                }
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            manager = 0;
            vtable = 0;
            gateTarget = 0;
        }
    }

    const bool omegaTriggered = inspect && package == "mission_scot"
                                && state::activity::mission_seed_armed();
    const std::uint32_t omegaObservation =
        omegaTriggered
            ? g_omegaTriggeredSchemaApplyObserved.fetch_add(1, std::memory_order_relaxed) + 1U
            : 0U;
    const std::uint32_t previousOmegaApply = g_omegaTriggeredSchemaApply;
    g_omegaTriggeredSchemaApply = omegaObservation <= 8U ? omegaObservation : 0U;
    const bool accepted = original(roster, stream, event);
    g_omegaTriggeredSchemaApply = previousOmegaApply;
    if (omegaObservation != 0U && omegaObservation <= 8U) {
        std::array<char, core::log::kLineCapacity> omegaLine{};
        const int omegaWritten = std::snprintf(
            omegaLine.data(),
            omegaLine.size(),
            "ev=bootflow stage=omega_trigger_authority_schema_apply n=%u result=%s bits=%u roster=%p stream=%p event=%p forced=%.*s mutation=observe_only",
            omegaObservation,
            accepted ? "accepted" : "rejected",
            consumed_bits(stream),
            roster,
            stream,
            event,
            static_cast<int>(package.size()),
            package.data());
        if (omegaWritten > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {omegaLine.data(),
                              static_cast<std::size_t>(omegaWritten) < omegaLine.size()
                                  ? static_cast<std::size_t>(omegaWritten)
                                  : omegaLine.size() - 1U});
        }
    }
    if (observation != 0U && observation <= 12U) {
        std::uint8_t readyAfter = 0;
        std::uint8_t enabledAfter = 0;
        std::uint32_t bits = 0;
        __try {
            if (roster != nullptr) {
                const auto* const bytes = static_cast<const std::byte*>(roster);
                std::memcpy(&readyAfter, bytes + 0x10E7F, sizeof readyAfter);
                std::memcpy(&enabledAfter, bytes + 0x10E80, sizeof enabledAfter);
            }
            bits = consumed_bits(stream);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            bits = 0;
        }
        const auto image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        const std::uintptr_t gateRva = image != 0 && gateTarget >= image
                                           && gateTarget - image < kMaximumGameImageSize
                                       ? gateTarget - image
                                       : 0;
        std::array<char, core::log::kLineCapacity> line{};
        const int written = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_authority_schema_gate n=%u result=%u bits=%u ready_before=%u ready_after=%u enabled_before=%u enabled_after=%u manager=%p vtable=%p gate_rva=0x%llX forced=%.*s",
            observation,
            accepted ? 1U : 0U,
            bits,
            static_cast<unsigned>(readyBefore),
            static_cast<unsigned>(readyAfter),
            static_cast<unsigned>(enabledBefore),
            static_cast<unsigned>(enabledAfter),
            reinterpret_cast<void*>(manager),
            reinterpret_cast<void*>(vtable),
            static_cast<unsigned long long>(gateRva),
            static_cast<int>(package.size()),
            package.data());
        if (written > 0) {
            const std::size_t length = static_cast<std::size_t>(written) < line.size()
                                           ? static_cast<std::size_t>(written)
                                           : line.size() - 1;
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(), length});
        }
    }
    return accepted;
}

[[nodiscard]] bool
report_type31_capture_record(const type31_capture::CaptureRecord& record) noexcept {
    // Keep enough room for the logging layer's channel/level/timestamp prefix and CRLF. The two
    // sequence-keyed events are fixed scalar/hash-only projections: no raw StateKey, body,
    // pointer-bearing window, or ASLR-bearing native address enters the normal client log.
    constexpr std::size_t kEventCapacity = 896U;
    constexpr std::size_t kLogEnvelopeReserve = 96U;
    static_assert(kEventCapacity + kLogEnvelopeReserve <= core::log::kLineCapacity);

    type31_capture::DefaultLogFields fields{};
    if (!type31_capture::default_log_fields(record, fields)) {
        return false;
    }
    const type31_capture::CaptureContext& context = record.context;
    std::array<char, kEventCapacity> line{};
    const int written = std::snprintf(
        line.data(),
        line.size(),
        "ev=omega_type31_capture v=2 sequence=%llu capture_epoch=%llu tick=%llu thread=%u "
        "caller_rva=0x%llX component_token=0x%016llX definition=0x%08X schema=0x%08X "
        "logical=%08X/%u/%u occupancy=%08X/%u/%u presence=0x%08X "
        "context_hash=0x%016llX context_shape_valid=%u fully_correlated=%u "
        "activation_state=%u activation_entry_exact=%u activation_current_at_exit=%u "
        "state_key_hash=0x%016llX decoded_body_pre_hash=0x%016llX "
        "component_before_hash=0x%016llX component_after_hash=0x%016llX "
        "component_before_valid=%u component_after_valid=%u decoded_valid=%u auth_bool=%u "
        "raw_u64_0=0x%016llX raw_u64_1=0x%016llX mutation=observe_only",
        static_cast<unsigned long long>(record.sequence),
        static_cast<unsigned long long>(record.capture_epoch),
        static_cast<unsigned long long>(record.monotonic_tick),
        record.producer_thread_id,
        static_cast<unsigned long long>(record.caller_rva),
        static_cast<unsigned long long>(fields.component_token),
        record.definition,
        record.schema,
        record.logical.registry,
        record.logical.type,
        record.logical.index,
        record.logical.occupancy_registry,
        record.logical.occupancy_type,
        record.logical.occupancy_index,
        context.presence_mask,
        static_cast<unsigned long long>(fields.context_hash),
        record.context_shape_valid ? 1U : 0U,
        record.context_shape_valid && type31_capture::fully_correlated(context) ? 1U : 0U,
        static_cast<unsigned>(context.activation_state),
        record.activation_entry_exact ? 1U : 0U,
        record.activation_current_at_exit ? 1U : 0U,
        static_cast<unsigned long long>(fields.state_key_hash),
        static_cast<unsigned long long>(fields.decoded_body_pre_hash),
        static_cast<unsigned long long>(fields.component_before_hash),
        static_cast<unsigned long long>(fields.component_after_hash),
        record.component_before_valid ? 1U : 0U,
        record.component_after_valid ? 1U : 0U,
        fields.decoded_valid ? 1U : 0U,
        fields.decoded_valid && fields.decoded.auth_bool ? 1U : 0U,
        static_cast<unsigned long long>(fields.decoded.u64_0),
        static_cast<unsigned long long>(fields.decoded.u64_1));
    if (written <= 0 || static_cast<std::size_t>(written) >= line.size()) {
        return false;
    }
    core::log::write(core::log::Channel::client,
                     core::log::Level::info,
                     {line.data(), static_cast<std::size_t>(written)});

    std::array<char, kEventCapacity> incidentLine{};
    const int incidentWritten = std::snprintf(
        incidentLine.data(),
        incidentLine.size(),
        "ev=omega_type31_incident_state v=2 sequence=%llu capture_epoch=%llu "
        "point=%08X/%u/%u authored_volume=%08X/%u/%u "
        "semantic=host_armed_local_one_shot_incident decoded_active=%u "
        "decoded_pending_generation=0x%016llX decoded_companion=0x%016llX "
        "consumed_before=0x%016llX authority_active_qword_before=0x%016llX "
        "pending_before=0x%016llX companion_before=0x%016llX "
        "consumed_after=0x%016llX authority_active_qword_after=0x%016llX "
        "pending_after=0x%016llX companion_after=0x%016llX before_valid=%u after_valid=%u "
        "terminal_observed=0 listener_identity=unknown_not_captured strict_gate=none "
        "mutation=observe_only",
        static_cast<unsigned long long>(record.sequence),
        static_cast<unsigned long long>(record.capture_epoch),
        record.logical.registry,
        record.logical.type,
        record.logical.index,
        record.logical.occupancy_registry,
        record.logical.occupancy_type,
        record.logical.occupancy_index,
        fields.decoded_valid && fields.decoded.auth_bool ? 1U : 0U,
        static_cast<unsigned long long>(fields.decoded.u64_0),
        static_cast<unsigned long long>(fields.decoded.u64_1),
        static_cast<unsigned long long>(record.before.offset_180),
        static_cast<unsigned long long>(record.before.offset_188),
        static_cast<unsigned long long>(record.before.offset_190),
        static_cast<unsigned long long>(record.before.offset_198),
        static_cast<unsigned long long>(record.after.offset_180),
        static_cast<unsigned long long>(record.after.offset_188),
        static_cast<unsigned long long>(record.after.offset_190),
        static_cast<unsigned long long>(record.after.offset_198),
        record.component_before_valid ? 1U : 0U,
        record.component_after_valid ? 1U : 0U);
    if (incidentWritten <= 0
        || static_cast<std::size_t>(incidentWritten) >= incidentLine.size()) {
        return false;
    }
    core::log::write(core::log::Channel::client,
                     core::log::Level::info,
                     {incidentLine.data(), static_cast<std::size_t>(incidentWritten)});
    return true;
}

[[nodiscard]] constexpr std::uint64_t type31_counter_delta(std::uint64_t current,
                                                            std::uint64_t previous) noexcept {
    return current >= previous ? current - previous : current;
}

[[nodiscard]] std::uint64_t allocate_type31_capture_epoch() noexcept {
    std::uint64_t current = g_type31CaptureEpochCounter.load(std::memory_order_acquire);
    while (current != (std::numeric_limits<std::uint64_t>::max)()) {
        const std::uint64_t next = current + 1U;
        if (g_type31CaptureEpochCounter.compare_exchange_weak(current,
                                                              next,
                                                              std::memory_order_acq_rel,
                                                              std::memory_order_acquire)) {
            return next;
        }
    }
    return 0U;
}

class Type31DrainClaim final {
public:
    Type31DrainClaim() noexcept
        : claimed_(!g_type31DrainOwner.test_and_set(std::memory_order_acquire)) {}
    ~Type31DrainClaim() {
        if (claimed_) {
            g_type31DrainOwner.clear(std::memory_order_release);
        }
    }
    Type31DrainClaim(const Type31DrainClaim&) = delete;
    Type31DrainClaim& operator=(const Type31DrainClaim&) = delete;
    [[nodiscard]] explicit operator bool() const noexcept { return claimed_; }

private:
    bool claimed_{};
};

void drain_type31_capture_queue(std::size_t budget) noexcept {
    for (std::size_t index = 0U; index < budget; ++index) {
        type31_capture::CaptureRecord record{};
        const type31_capture::ReadResult result =
            g_type31CaptureQueue.try_pop_raw(record);
        if (result != type31_capture::ReadResult::success) {
            break;
        }
        if (!report_type31_capture_record(record)) {
            g_type31CaptureQueue.account_projection_failure();
        }
    }

    const type31_capture::QueueCounters current = g_type31CaptureQueue.counters();
    const type31_capture::QueueCounters delta{
        type31_counter_delta(current.accepted, g_type31ReportedQueueCounters.accepted),
        type31_counter_delta(current.duplicates, g_type31ReportedQueueCounters.duplicates),
        type31_counter_delta(current.rejected, g_type31ReportedQueueCounters.rejected),
        type31_counter_delta(current.dropped_full, g_type31ReportedQueueCounters.dropped_full),
        type31_counter_delta(current.dropped_busy, g_type31ReportedQueueCounters.dropped_busy),
        type31_counter_delta(current.dropped_sequence_exhausted,
                             g_type31ReportedQueueCounters.dropped_sequence_exhausted),
        type31_counter_delta(current.context_resolver_failures,
                             g_type31ReportedQueueCounters.context_resolver_failures),
        type31_counter_delta(current.projection_failures,
                             g_type31ReportedQueueCounters.projection_failures),
    };
    g_type31ReportedQueueCounters = current;
    if (delta.accepted == 0U && delta.duplicates == 0U && delta.rejected == 0U
        && delta.dropped() == 0U) {
        return;
    }

    constexpr std::size_t kQueueEventCapacity = 768U;
    static_assert(kQueueEventCapacity + 128U <= core::log::kLineCapacity);
    std::array<char, kQueueEventCapacity> line{};
    const int written = std::snprintf(
        line.data(),
        line.size(),
        "ev=omega_type31_capture_queue v=1 delta_accepted=%llu delta_duplicate=%llu "
        "delta_rejected=%llu delta_full=%llu delta_busy=%llu delta_exhausted=%llu "
        "delta_resolver=%llu delta_projection=%llu total_accepted=%llu total_duplicate=%llu "
        "total_rejected=%llu total_full=%llu total_busy=%llu total_exhausted=%llu "
        "total_resolver=%llu total_projection=%llu mutation=observe_only",
        static_cast<unsigned long long>(delta.accepted),
        static_cast<unsigned long long>(delta.duplicates),
        static_cast<unsigned long long>(delta.rejected),
        static_cast<unsigned long long>(delta.dropped_full),
        static_cast<unsigned long long>(delta.dropped_busy),
        static_cast<unsigned long long>(delta.dropped_sequence_exhausted),
        static_cast<unsigned long long>(delta.context_resolver_failures),
        static_cast<unsigned long long>(delta.projection_failures),
        static_cast<unsigned long long>(current.accepted),
        static_cast<unsigned long long>(current.duplicates),
        static_cast<unsigned long long>(current.rejected),
        static_cast<unsigned long long>(current.dropped_full),
        static_cast<unsigned long long>(current.dropped_busy),
        static_cast<unsigned long long>(current.dropped_sequence_exhausted),
        static_cast<unsigned long long>(current.context_resolver_failures),
        static_cast<unsigned long long>(current.projection_failures));
    if (written > 0 && static_cast<std::size_t>(written) < line.size()) {
        core::log::write(
            core::log::Channel::client,
            core::log::Level::info,
            {line.data(), static_cast<std::size_t>(written)});
    } else {
        g_type31CaptureQueue.account_projection_failure();
    }
}

[[nodiscard]] bool type31_owner_calls_idle() noexcept {
    return g_type31PointCallGate.idle() && g_type31DrainCallGate.idle()
           && !g_type31DrainOwner.test(std::memory_order_acquire);
}

[[nodiscard]] bool cng_succeeded(NTSTATUS status) noexcept { return status >= 0; }

[[nodiscard]] bool current_executable_sha256(HMODULE module,
                                              type31_capture::ImageSha256& output) noexcept {
    constexpr std::size_t kModulePathCapacity = 32768U;
    constexpr std::size_t kReadBufferBytes = 16U * 1024U;
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
        complete = cng_succeeded(
            BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0));
    }
    std::array<std::byte, kReadBufferBytes> buffer{};
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

[[nodiscard]] std::array<legacy_owner_sentinel::HookOwnership, 17>
activity_schema_decode_legacy_bundle_hook_ownership() noexcept {
    // LEGACY_OWNER_SENTINEL_BEGIN(activity_schema_decode_legacy_bundle, 17)
    return {{{g_bitHandle.attached,
              g_original.load(std::memory_order_acquire) != nullptr},
             {g_boolHandle.attached,
              g_boolOriginal.load(std::memory_order_acquire) != nullptr},
             {g_responseHandleInternalHandle.attached,
              g_responseHandleInternalOriginal.load(std::memory_order_acquire) != nullptr},
             {g_authoritySchemaApplyHandle.attached,
              g_authoritySchemaApplyOriginal.load(std::memory_order_acquire) != nullptr},
             {g_authorityGroupDecodeHandle.attached,
              g_authorityGroupDecodeOriginal.load(std::memory_order_acquire) != nullptr},
             {g_authorityObjectDecodeHandle.attached,
              g_authorityObjectDecodeOriginal.load(std::memory_order_acquire) != nullptr},
             {g_authorityValidationHandle.attached,
              g_authorityValidationOriginal.load(std::memory_order_acquire) != nullptr},
             {g_authorityPublishCollectHandle.attached,
              g_authorityPublishCollectOriginal.load(std::memory_order_acquire) != nullptr},
             {g_authorityPublishApplyHandle.attached,
              g_authorityPublishApplyOriginal.load(std::memory_order_acquire) != nullptr},
             {g_authorityPublishFinalizeHandle.attached,
              g_authorityPublishFinalizeOriginal.load(std::memory_order_acquire) != nullptr},
             {g_objectRuntimeResolveHandle.attached,
              g_objectRuntimeResolveOriginal.load(std::memory_order_acquire) != nullptr},
             {g_omegaVisualCreateHandle.attached,
              g_omegaVisualCreateOriginal.load(std::memory_order_acquire) != nullptr},
             {g_omegaVisualUpdateHandle.attached,
              g_omegaVisualUpdateOriginal.load(std::memory_order_acquire) != nullptr},
             {g_omegaVisualSelectHandle.attached,
              g_omegaVisualSelectOriginal.load(std::memory_order_acquire) != nullptr},
             {g_omegaGateApplyHandle.attached,
              g_omegaGateApplyOriginal.load(std::memory_order_acquire) != nullptr},
             {g_omegaEngagementApplyHandle.attached,
              g_omegaEngagementApplyOriginal.load(std::memory_order_acquire) != nullptr},
             {g_omegaMonitorApplyHandle.attached,
              g_omegaMonitorApplyOriginal.load(std::memory_order_acquire) != nullptr}}};
    // LEGACY_OWNER_SENTINEL_END(activity_schema_decode_legacy_bundle)
}

class Type31OwnerOperations final {
public:
    [[nodiscard]] bool enabled() const noexcept {
        return core::settings::get().omegaExperiments.unsafeDiagnostics;
    }

    [[nodiscard]] bool admit_image() noexcept {
        module_ = GetModuleHandleW(nullptr);
        diagnostics::ModuleRange range{};
        type31_capture::ImageSha256 packedSha{};
        if (module_ == nullptr || !diagnostics::module_range(module_, range)
            || range.end <= range.base || !current_executable_sha256(module_, packedSha)
            || packedSha != type31_capture::kPinnedPackedImageSha256) {
            return false;
        }
        imageBytes_ = range.end - range.base;
        const auto* const image = reinterpret_cast<const std::byte*>(range.base);
        constexpr std::array surfaces{type31_capture::NativeSurface::apply,
                                      type31_capture::NativeSurface::predicate,
                                      type31_capture::NativeSurface::terminal,
                                      type31_capture::NativeSurface::subscriber,
                                      type31_capture::NativeSurface::listener_enumerator};
        std::uint64_t contractHash = 14695981039346656037ULL;
        for (const type31_capture::NativeSurface surface : surfaces) {
            const std::byte* const validated =
                type31_capture::validated_native_target(surface, image, imageBytes_);
            if (validated == nullptr) {
                return false;
            }
            if (surface == type31_capture::NativeSurface::apply) {
                target_ = validated;
            }
            const type31_capture::NativeTarget contract = type31_capture::native_target(surface);
            contractHash ^= type31_capture::bounded_hash(
                std::span<const std::byte>{validated, contract.prefix.size()});
            contractHash *= 1099511628211ULL;
        }
        prefixHash_ = contractHash;
        return true;
    }

    [[nodiscard]] std::uint64_t allocate_epoch() const noexcept {
        return allocate_type31_capture_epoch();
    }

    [[nodiscard]] bool reset_for_new_epoch(std::uint64_t) noexcept {
        if (g_type31CaptureQueue.try_reset() != type31_capture::ResetResult::reset) {
            return false;
        }
        g_type31ReportedQueueCounters = {};
        return true;
    }

    [[nodiscard]] bool attach() noexcept {
        if (target_ == nullptr || g_omegaPointApplyHandle.attached) {
            return false;
        }
        dump_publish_function(L"omega_post_point_apply", const_cast<std::byte*>(target_));
        const hooking::detour::Spec spec{const_cast<std::byte*>(target_),
                                          reinterpret_cast<void*>(&omega_point_apply)};
        return hooking::detour::install(spec, g_omegaPointApplyHandle);
    }

    void publish(std::uint64_t epoch) noexcept {
        hooking::publish_original(
            g_omegaPointApplyOriginal,
            reinterpret_cast<type31_capture::NativeApply>(g_omegaPointApplyHandle.original));
        g_type31MappedImageBytes.store(imageBytes_, std::memory_order_release);
        g_type31ApplyPrefixHash.store(prefixHash_, std::memory_order_release);
        g_type31PackedIdentityValidated.store(true, std::memory_order_release);
        g_type31CaptureEpoch.store(epoch, std::memory_order_release);
        g_type31PointCallGate.accept();
        g_type31DrainCallGate.accept();
    }

    void close_admission() const noexcept {
        g_type31PointCallGate.quiesce();
        g_type31DrainCallGate.quiesce();
    }

    [[nodiscard]] type31_capture::OwnerRemovalResult remove() noexcept {
        if (!g_omegaPointApplyHandle.attached) {
            removalResult_ = type31_capture::OwnerRemovalResult::failed;
            return removalResult_;
        }
        const std::array protectedEntries{
            hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&omega_point_apply)},
            hooking::detour::ProtectedCodeEntry{
                reinterpret_cast<void*>(&hooking::call_gate_detail::enter)},
            hooking::detour::ProtectedCodeEntry{
                reinterpret_cast<void*>(&hooking::call_gate_detail::leave)},
            hooking::detour::ProtectedCodeEntry{
                reinterpret_cast<void*>(&sample_type31_objective_capture)},
        };
        const hooking::detour::UninstallResult result = hooking::detour::uninstall(
            g_omegaPointApplyHandle, protectedEntries, &type31_owner_calls_idle);
        if (result == hooking::detour::UninstallResult::removed) {
            removalResult_ = type31_capture::OwnerRemovalResult::removed;
            return removalResult_;
        }
        removalResult_ = result == hooking::detour::UninstallResult::protectedCodeActive
                             ? type31_capture::OwnerRemovalResult::protected_code_active
                             : type31_capture::OwnerRemovalResult::failed;
        return removalResult_;
    }

    [[nodiscard]] bool finalize_removed() const noexcept {
        if (g_omegaPointApplyHandle.attached || !type31_owner_calls_idle()) {
            return false;
        }
        Type31DrainClaim claim;
        if (!claim) {
            return false;
        }
        drain_type31_capture_queue(type31_capture::kQueueCapacity);
        if (g_type31CaptureQueue.try_reset() != type31_capture::ResetResult::reset) {
            return false;
        }
        g_type31ReportedQueueCounters = {};
        g_omegaPointApplyOriginal.store(nullptr, std::memory_order_release);
        g_type31CaptureEpoch.store(0U, std::memory_order_release);
        g_type31MappedImageBytes.store(0U, std::memory_order_release);
        g_type31ApplyPrefixHash.store(0U, std::memory_order_release);
        g_type31PackedIdentityValidated.store(false, std::memory_order_release);
        return true;
    }

    [[nodiscard]] type31_capture::OwnerRemovalResult removal_result() const noexcept {
        return removalResult_;
    }

private:
    HMODULE module_{};
    const std::byte* target_{};
    std::size_t imageBytes_{};
    std::uint64_t prefixHash_{};
    type31_capture::OwnerRemovalResult removalResult_{
        type31_capture::OwnerRemovalResult::failed};
};

} // namespace

bool install_omega_first_cannon_receipt() noexcept {
    if (g_omegaVisualApplyHandle.attached) { return g_omegaCannonReceiptGate.accepting(); }
    std::byte* target{};
    __try {
        target = object_target(kOmegaVisualApplyRva, kOmegaVisualApplyPrefix);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        target = nullptr;
    }
    if (target == nullptr
        || !hooking::detour::install({target, reinterpret_cast<void*>(&omega_visual_apply)},
                                     g_omegaVisualApplyHandle)) {
        core::log::write(core::log::Channel::client, core::log::Level::warn,
                        "ev=omega_first_cannon stage=install result=fail");
        return false;
    }
    hooking::publish_original(g_omegaVisualApplyOriginal,
        reinterpret_cast<OmegaVisualApply>(g_omegaVisualApplyHandle.original));
    for (auto& logged : g_omegaCannonReceiptLoggedRuns) {
        logged.store(0U, std::memory_order_release);
    }
    for (auto& logged : g_omegaTransitReceiptLoggedRuns) {
        logged.store(0U, std::memory_order_release);
    }
    g_omegaCannonReceiptGate.accept();
    core::log::write(core::log::Channel::client, core::log::Level::info,
        "ev=omega_first_cannon stage=install result=ok target=9F19F0 sources=95FB2E01/4/10,11,12,13 mutation=observe_only");
    return true;
}

void quiesce_omega_first_cannon_receipt() noexcept {
    g_omegaCannonReceiptGate.quiesce();
}

bool uninstall_omega_first_cannon_receipt() noexcept {
    quiesce_omega_first_cannon_receipt();
    if (!g_omegaVisualApplyHandle.attached) { return true; }
    const std::array<hooking::detour::ProtectedCodeEntry, 5> protectedCode{{
        {reinterpret_cast<void*>(&omega_visual_apply)},
        {reinterpret_cast<void*>(&observe_first_cannon_preparation)},
        {reinterpret_cast<void*>(&first_cannon_preparation)},
        {reinterpret_cast<void*>(&hooking::call_gate_detail::enter)},
        {reinterpret_cast<void*>(&hooking::call_gate_detail::leave)},
    }};
    if (hooking::detour::uninstall(g_omegaVisualApplyHandle, protectedCode,
                                   first_cannon_receipt_idle)
        != hooking::detour::UninstallResult::removed) { return false; }
    g_omegaVisualApplyOriginal.store(nullptr, std::memory_order_release);
    for (auto& logged : g_omegaCannonReceiptLoggedRuns) {
        logged.store(0U, std::memory_order_release);
    }
    for (auto& logged : g_omegaTransitReceiptLoggedRuns) {
        logged.store(0U, std::memory_order_release);
    }
    return true;
}

bool activity_schema_decode_legacy_bundle_has_ownership() noexcept {
    // LEGACY_OWNER_CLAIMS_BEGIN(activity_schema_decode_legacy_bundle, 2)
    const std::array claims{
        g_serviceSevenCaptureActive.load(std::memory_order_acquire),
        g_serviceSevenReadCaptureActive.load(std::memory_order_acquire),
    };
    // LEGACY_OWNER_CLAIMS_END(activity_schema_decode_legacy_bundle)
    return legacy_owner_sentinel::has_ownership(
        activity_schema_decode_legacy_bundle_hook_ownership(), claims);
}

bool install_type31_objective_capture() noexcept {
    Type31OwnerOperations operations;
    const bool installed = g_type31OwnerLifecycle.install(operations);
    const std::uint64_t epoch = g_type31CaptureEpoch.load(std::memory_order_acquire);
    const std::uint64_t prefixHash =
        g_type31ApplyPrefixHash.load(std::memory_order_acquire);
    const std::size_t imageBytes =
        g_type31MappedImageBytes.load(std::memory_order_acquire);
    constexpr std::size_t kInstallEventCapacity = 768U;
    static_assert(kInstallEventCapacity + 128U <= core::log::kLineCapacity);
    std::array<char, kInstallEventCapacity> line{};
    const int written = std::snprintf(
        line.data(),
        line.size(),
        "ev=omega_type31_owner v=2 stage=install result=%s owner=sole_plus_b20640 "
        "gate=unsafe_diagnostics packed_sha256=%s mapped_image_bytes=%zu "
        "apply_rva=0x%llX contract_prefix_hash=0x%016llX capture_epoch=%llu "
        "semantic=host_armed_local_one_shot_incident terminal_rva=0x%llX "
        "incident_dispatch_rva=0x%llX listener_enum_rva=0x%llX "
        "listener_identity=unknown_dynamic strict_gate=none mode=observe_only mutation=observe_only",
        installed ? "ok" : "fail_closed",
        g_type31PackedIdentityValidated.load(std::memory_order_acquire)
            ? "81964380664E7FCEE3C620085A157FDEAF91FEFACF7214907820F188BBEB4CED"
            : "unverified",
        imageBytes,
        static_cast<unsigned long long>(type31_capture::kApplyRva),
        static_cast<unsigned long long>(prefixHash),
        static_cast<unsigned long long>(epoch),
        static_cast<unsigned long long>(type31_capture::kTerminalRva),
        static_cast<unsigned long long>(type31_capture::kSubscriberRva),
        static_cast<unsigned long long>(type31_capture::kListenerEnumeratorRva));
    if (written > 0 && static_cast<std::size_t>(written) < line.size()) {
        core::log::write(core::log::Channel::client,
                         installed ? core::log::Level::info : core::log::Level::warn,
                         {line.data(), static_cast<std::size_t>(written)});
    } else {
        core::log::write(core::log::Channel::client,
                         core::log::Level::error,
                         "ev=omega_type31_owner v=2 stage=install result=serialization_failure");
    }
    return installed;
}

void quiesce_type31_objective_capture() noexcept {
    Type31OwnerOperations operations;
    g_type31OwnerLifecycle.quiesce(operations);
}

bool uninstall_type31_objective_capture() noexcept {
    Type31OwnerOperations operations;
    const bool removed = g_type31OwnerLifecycle.uninstall(operations);
    if (removed) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         "ev=omega_type31_owner v=2 stage=uninstall result=removed retained=0");
    } else if (g_type31OwnerLifecycle.phase()
               == type31_capture::OwnerPhase::removed_pending_reset) {
        core::log::write(
            core::log::Channel::client,
            core::log::Level::warn,
            "ev=omega_type31_owner v=2 stage=uninstall result=retained_reset retained=1");
    } else if (operations.removal_result()
               == type31_capture::OwnerRemovalResult::protected_code_active) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::warn,
                         "ev=omega_type31_owner v=2 stage=uninstall result=deferred retained=1");
    } else {
        core::log::write(core::log::Channel::client,
                         core::log::Level::error,
                         "ev=omega_type31_owner v=2 stage=uninstall result=failed retained=1");
    }
    return removed;
}

__declspec(noinline) void sample_type31_objective_capture() noexcept {
    hooking::CallGate::Scope drain(g_type31DrainCallGate);
    if (!drain.accepts_side_effects()) {
        return;
    }
    Type31DrainClaim claim;
    if (claim) {
        constexpr std::size_t kDrainBudget = 8U;
        drain_type31_capture_queue(kDrainBudget);
    }
}

/** Records local scene/gate state changes that do not return through a wire publish pass. */
void sample_omega_opening_runtime_state() noexcept {
    state::activity::forced::ForcedDestination forced{};
    state::activity::forced::snapshot(forced);
    const std::string_view package(forced.packageName.data(), forced.packageNameLength);
    if (!state::activity::forced::override_active() || package != "mission_scot") {
        return;
    }

    struct Sample final {
        OmegaRuntimeTrack track{};
        std::uint64_t previousHash{};
        std::array<std::byte, kOmegaRuntimeMaximumBytes> state{};
    };
    std::array<Sample, kOmegaRuntimeTrackCapacity> changed{};
    std::size_t changedCount = 0U;

    AcquireSRWLockExclusive(&g_omegaRuntimeTrackLock);
    for (std::size_t index = 0; index < g_omegaRuntimeTrackCount; ++index) {
        OmegaRuntimeTrack& track = g_omegaRuntimeTracks[index];
        if (track.runtime == 0U || track.bytes == 0U
            || track.bytes > kOmegaRuntimeMaximumBytes) {
            continue;
        }
        std::array<std::byte, kOmegaRuntimeMaximumBytes> state{};
        bool readable = false;
        __try {
            std::memcpy(state.data(),
                        reinterpret_cast<const void*>(track.runtime),
                        track.bytes);
            readable = true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            readable = false;
        }
        if (!readable) {
            continue;
        }
        const std::uint64_t hash = hash_omega_runtime(state.data(), track.bytes);
        if (hash == track.hash) {
            continue;
        }
        if (changedCount < changed.size()) {
            changed[changedCount].track = track;
            changed[changedCount].previousHash = track.hash;
            changed[changedCount].track.hash = hash;
            changed[changedCount].state = state;
            ++changedCount;
        }
        track.hash = hash;
    }
    ReleaseSRWLockExclusive(&g_omegaRuntimeTrackLock);

    constexpr char kHex[] = "0123456789ABCDEF";
    for (std::size_t index = 0; index < changedCount; ++index) {
        const std::uint32_t observation =
            g_omegaRuntimeFollowupObserved.fetch_add(1, std::memory_order_relaxed) + 1U;
        if (observation > 512U) {
            return;
        }
        const Sample& sample = changed[index];
        const auto read32 = [&sample](std::size_t offset) noexcept {
            std::uint32_t value = 0U;
            if (offset + sizeof value <= sample.track.bytes) {
                std::memcpy(&value, sample.state.data() + offset, sizeof value);
            }
            return value;
        };
        std::array<char, core::log::kLineCapacity> line{};
        const int prefix = std::snprintf(
            line.data(),
            line.size(),
            "ev=omega_runtime_followup n=%u label=%s schema=0x%08X datum=0x%08X "
            "visual_slot=%d object=%p runtime=%p bytes=%zu previous_hash=0x%016llX "
            "hash=0x%016llX visual_dwords=%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X,"
            "%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X hex=",
            observation,
            omega_runtime_label(sample.track.schema),
            sample.track.schema,
            sample.track.datum,
            sample.track.schema == 0x8080992FU
                ? omega_portal_visual_slot(sample.track.datum)
                : -1,
            reinterpret_cast<void*>(sample.track.object),
            reinterpret_cast<void*>(sample.track.runtime),
            sample.track.bytes,
            static_cast<unsigned long long>(sample.previousHash),
            static_cast<unsigned long long>(sample.track.hash),
            read32(0x00),
            read32(0x04),
            read32(0x08),
            read32(0x0C),
            read32(0x10),
            read32(0x14),
            read32(0x18),
            read32(0x1C),
            read32(0x20),
            read32(0x24),
            read32(0x28),
            read32(0x2C),
            read32(0x30),
            read32(0x34),
            read32(0x38),
            read32(0x3C));
        if (prefix <= 0) {
            continue;
        }
        std::size_t used = (std::min)(static_cast<std::size_t>(prefix), line.size() - 1U);
        for (std::size_t byte = 0;
             byte < sample.track.bytes && used + 2U < line.size();
             ++byte) {
            const std::uint8_t value = std::to_integer<std::uint8_t>(sample.state[byte]);
            line[used++] = kHex[value >> 4U];
            line[used++] = kHex[value & 0x0FU];
        }
        line[used] = '\0';
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), used});
    }
}

void uninstall_omega_post_scene_trace() noexcept {
    if (g_omegaMonitorApplyHandle.attached) {
        (void)hooking::detour::uninstall(g_omegaMonitorApplyHandle);
    }
    if (g_omegaEngagementApplyHandle.attached) {
        (void)hooking::detour::uninstall(g_omegaEngagementApplyHandle);
    }
    if (g_omegaGateApplyHandle.attached) {
        (void)hooking::detour::uninstall(g_omegaGateApplyHandle);
    }
    g_omegaGateApplyOriginal.store(nullptr, std::memory_order_release);
    g_omegaEngagementApplyOriginal.store(nullptr, std::memory_order_release);
    g_omegaMonitorApplyOriginal.store(nullptr, std::memory_order_release);
    g_omegaPostSceneApplyObserved.store(0U, std::memory_order_release);
}

/** Installs the three legacy post-scene observers; Type-31 owns +B20640 independently. */
[[nodiscard]] bool install_omega_post_scene_trace() noexcept {
    const auto attach = [](const char* name,
                           const wchar_t* dumpPhase,
                           std::uintptr_t rva,
                           void* replacement,
                           hooking::detour::Handle& handle,
                           std::atomic<OmegaPostSceneApply>& original) noexcept {
        if (handle.attached) {
            return true;
        }
        std::byte* const target = omega_visual_target(
            name, dumpPhase, rva, kOmegaPostSceneApplyPrefix);
        if (target == nullptr) {
            return false;
        }
        const hooking::detour::Spec spec{target, replacement};
        if (!hooking::detour::install(spec, handle)) {
            return false;
        }
        original.store(reinterpret_cast<OmegaPostSceneApply>(handle.original),
                       std::memory_order_release);
        return true;
    };

    const bool gate = attach("post_gate_apply",
                             L"omega_post_gate_apply",
                             kOmegaGateApplyRva,
                             reinterpret_cast<void*>(&omega_gate_apply),
                             g_omegaGateApplyHandle,
                             g_omegaGateApplyOriginal);
    const bool engagement = attach("post_engagement_apply",
                                   L"omega_post_engagement_apply",
                                   kOmegaEngagementApplyRva,
                                   reinterpret_cast<void*>(&omega_engagement_apply),
                                   g_omegaEngagementApplyHandle,
                                   g_omegaEngagementApplyOriginal);
    const bool monitor = attach("post_monitor_apply",
                                L"omega_post_monitor_apply",
                                kOmegaMonitorApplyRva,
                                reinterpret_cast<void*>(&omega_monitor_apply),
                                g_omegaMonitorApplyHandle,
                                g_omegaMonitorApplyOriginal);
    const bool pointOwned = g_omegaPointApplyHandle.attached;
    const bool any = gate || engagement || monitor;
    const bool all = gate && engagement && monitor;
    g_omegaPostSceneApplyObserved.store(0U, std::memory_order_release);

    const std::uint64_t captureEpoch =
        g_type31CaptureEpoch.load(std::memory_order_acquire);
    std::array<char, 384> line{};
    const int written = std::snprintf(
        line.data(),
        line.size(),
        "ev=omega_post_scene_apply stage=install result=%s gate=%u engagement=%u "
        "point_external_owner=%u monitor=%u type31_fanout=%s capture_epoch=%llu context=unknown "
        "point_owner=narrow_lifecycle hold_client_runtime=absent mode=observe mutation=observe_only",
        all ? "ok" : any ? "partial" : "fail",
        gate ? 1U : 0U,
        engagement ? 1U : 0U,
        pointOwned ? 1U : 0U,
        monitor ? 1U : 0U,
        captureEpoch != 0U ? "on" : "off",
        static_cast<unsigned long long>(captureEpoch));
    if (written > 0) {
        core::log::write(core::log::Channel::client,
                         all ? core::log::Level::info : core::log::Level::warn,
                         {line.data(),
                          (std::min)(static_cast<std::size_t>(written), line.size() - 1U)});
    }
    return any;
}

void uninstall_omega_visual_trace() noexcept {
    if (g_omegaVisualSelectHandle.attached) {
        (void)hooking::detour::uninstall(g_omegaVisualSelectHandle);
    }
    if (g_omegaVisualUpdateHandle.attached) {
        (void)hooking::detour::uninstall(g_omegaVisualUpdateHandle);
    }
    if (g_omegaVisualCreateHandle.attached) {
        (void)hooking::detour::uninstall(g_omegaVisualCreateHandle);
    }
    g_omegaVisualCreateOriginal.store(nullptr, std::memory_order_release);
    g_omegaVisualUpdateOriginal.store(nullptr, std::memory_order_release);
    g_omegaVisualSelectOriginal.store(nullptr, std::memory_order_release);
}

/** Installs only signatures proven by the exact visual-component decompile; failure is nonfatal. */
[[nodiscard]] bool install_omega_visual_trace() noexcept {
    if (g_omegaVisualCreateHandle.attached
        && g_omegaVisualUpdateHandle.attached && g_omegaVisualSelectHandle.attached) {
        return true;
    }
    std::byte* const createTarget = omega_visual_target("create",
                                                        L"omega_visual_create",
                                                        kOmegaVisualCreateRva,
                                                        kOmegaVisualCreatePrefix);
    std::byte* const updateTarget = omega_visual_target("update",
                                                        L"omega_visual_update",
                                                        kOmegaVisualUpdateRva,
                                                        kOmegaVisualUpdatePrefix);
    std::byte* const selectTarget = omega_visual_target("select",
                                                        L"omega_visual_select",
                                                        kOmegaVisualSelectRva,
                                                        kOmegaVisualSelectPrefix);

    if (createTarget != nullptr && !g_omegaVisualCreateHandle.attached) {
        const hooking::detour::Spec createSpec{createTarget,
                                               reinterpret_cast<void*>(&omega_visual_create)};
        if (hooking::detour::install(createSpec, g_omegaVisualCreateHandle)) {
            g_omegaVisualCreateOriginal.store(
                reinterpret_cast<OmegaVisualCreate>(g_omegaVisualCreateHandle.original),
                std::memory_order_release);
        } else {
            core::log::write(core::log::Channel::client,
                             core::log::Level::warn,
                             "ev=omega_visual_trace stage=install result=partial reason=create_attach mutation=observe_only");
        }
    }

    if (updateTarget != nullptr && !g_omegaVisualUpdateHandle.attached) {
        const hooking::detour::Spec updateSpec{updateTarget,
                                               reinterpret_cast<void*>(&omega_visual_update)};
        if (hooking::detour::install(updateSpec, g_omegaVisualUpdateHandle)) {
            g_omegaVisualUpdateOriginal.store(
                reinterpret_cast<OmegaVisualUpdate>(g_omegaVisualUpdateHandle.original),
                std::memory_order_release);
        } else {
            core::log::write(core::log::Channel::client,
                             core::log::Level::warn,
                             "ev=omega_visual_trace stage=install result=partial reason=update_attach mutation=observe_only");
        }
    }

    if (selectTarget != nullptr && !g_omegaVisualSelectHandle.attached) {
        const hooking::detour::Spec selectSpec{selectTarget,
                                               reinterpret_cast<void*>(&omega_visual_select)};
        if (hooking::detour::install(selectSpec, g_omegaVisualSelectHandle)) {
            g_omegaVisualSelectOriginal.store(
                reinterpret_cast<OmegaVisualSelect>(g_omegaVisualSelectHandle.original),
                std::memory_order_release);
        } else {
            core::log::write(core::log::Channel::client,
                             core::log::Level::warn,
                             "ev=omega_visual_trace stage=install result=partial reason=select_attach mutation=observe_only");
        }
    }

    const bool applyAttached = g_omegaVisualApplyHandle.attached;
    const bool createAttached = g_omegaVisualCreateHandle.attached;
    const bool updateAttached = g_omegaVisualUpdateHandle.attached;
    const bool selectAttached = g_omegaVisualSelectHandle.attached;
    const bool anyAttached = createAttached || updateAttached || selectAttached;
    const bool allAttached = createAttached && updateAttached && selectAttached;
    g_omegaVisualApplyObserved.store(0U, std::memory_order_release);
    g_omegaVisualCreateObserved.store(0U, std::memory_order_release);
    g_omegaVisualUpdateObserved.store(0U, std::memory_order_release);
    g_omegaVisualSelectObserved.store(0U, std::memory_order_release);
    std::array<char, 256> line{};
    const int written = std::snprintf(
        line.data(),
        line.size(),
        "ev=omega_visual_trace stage=install result=%s apply_external_owner=%u create=%u update=%u select=%u "
        "state_bytes=368 mode=observe mutation=observe_only",
        allAttached ? "ok" : anyAttached ? "partial" : "fail",
        applyAttached ? 1U : 0U,
        createAttached ? 1U : 0U,
        updateAttached ? 1U : 0U,
        selectAttached ? 1U : 0U);
    if (written > 0) {
        core::log::write(core::log::Channel::client,
                         allAttached ? core::log::Level::info : core::log::Level::warn,
                         {line.data(),
                          (std::min)(static_cast<std::size_t>(written),
                                     line.size() - 1U)});
    }
    return anyAttached;
}

/** Attaches read-only observers that follow the authority stream across decoder boundaries. */
bool install_activity_schema_decode_probe() noexcept {
    if (g_bitHandle.attached && g_boolHandle.attached
        && g_responseHandleInternalHandle.attached && g_authoritySchemaApplyHandle.attached
        && g_authorityGroupDecodeHandle.attached && g_authorityObjectDecodeHandle.attached
        && g_authorityValidationHandle.attached && g_authorityPublishCollectHandle.attached
        && g_authorityPublishApplyHandle.attached && g_authorityPublishFinalizeHandle.attached
        && g_objectRuntimeResolveHandle.attached) {
        return true;
    }
    std::byte* const bitTarget = bit_reader_target();
    std::byte* const boolTarget = bool_reader_target();
    if (bitTarget == nullptr || boolTarget == nullptr) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::warn,
                         "ev=bootflow stage=activity_authority_stream_probe result=fail reason=target");
        return false;
    }
    auto* const readerImage = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    dump_publish_function(L"activity_host_manager_read_bits", bitTarget);
    dump_publish_function(L"activity_host_manager_read_bool", boolTarget);
    if (readerImage != nullptr) {
        dump_publish_function(L"activity_host_manager_read_wide",
                              readerImage + kWideReaderRva);
        dump_publish_function(L"activity_host_manager_stream_status",
                              readerImage + kStreamStatusRva);
    }
    const hooking::detour::Spec bitSpec{bitTarget, reinterpret_cast<void*>(&bit_reader)};
    if (!hooking::detour::install(bitSpec, g_bitHandle)) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::warn,
                         "ev=bootflow stage=activity_authority_stream_probe result=fail reason=bit_attach");
        return false;
    }
    g_original.store(reinterpret_cast<BitReader>(g_bitHandle.original),
                     std::memory_order_release);

    const hooking::detour::Spec boolSpec{boolTarget, reinterpret_cast<void*>(&bool_reader)};
    if (!hooking::detour::install(boolSpec, g_boolHandle)) {
        g_original.store(nullptr, std::memory_order_release);
        (void)hooking::detour::uninstall(g_bitHandle);
        core::log::write(core::log::Channel::client,
                         core::log::Level::warn,
                         "ev=bootflow stage=activity_authority_stream_probe result=fail reason=bool_attach");
        return false;
    }
    g_boolOriginal.store(reinterpret_cast<BoolReader>(g_boolHandle.original),
                         std::memory_order_release);

    std::byte* const responseHandleTarget =
        object_target(kResponseHandleInternalRva, kResponseHandleInternalPrefix);
    if (responseHandleTarget == nullptr) {
        uninstall_activity_schema_decode_probe();
        core::log::write(
            core::log::Channel::client,
            core::log::Level::warn,
            "ev=bootflow stage=activity_host_manager_svc7_handler result=fail reason=target");
        return false;
    }
    const hooking::detour::Spec responseHandleSpec{
        responseHandleTarget, reinterpret_cast<void*>(&response_handle_internal)};
    if (!hooking::detour::install(responseHandleSpec, g_responseHandleInternalHandle)) {
        uninstall_activity_schema_decode_probe();
        core::log::write(
            core::log::Channel::client,
            core::log::Level::warn,
            "ev=bootflow stage=activity_host_manager_svc7_handler result=fail reason=attach");
        return false;
    }
    g_responseHandleInternalOriginal.store(
        reinterpret_cast<ResponseHandleInternal>(g_responseHandleInternalHandle.original),
        std::memory_order_release);

    std::byte* const objectRuntimeResolveTarget =
        object_target(kObjectRuntimeResolveRva, kObjectRuntimeResolvePrefix);
    if (objectRuntimeResolveTarget == nullptr) {
        uninstall_activity_schema_decode_probe();
        core::log::write(core::log::Channel::client,
                         core::log::Level::warn,
                         "ev=bootflow stage=activity_script_runtime_resolver result=fail reason=target");
        return false;
    }
    const hooking::detour::Spec objectRuntimeResolveSpec{
        objectRuntimeResolveTarget, reinterpret_cast<void*>(&object_runtime_resolve)};
    if (!hooking::detour::install(objectRuntimeResolveSpec, g_objectRuntimeResolveHandle)) {
        uninstall_activity_schema_decode_probe();
        core::log::write(core::log::Channel::client,
                         core::log::Level::warn,
                         "ev=bootflow stage=activity_script_runtime_resolver result=fail reason=attach");
        return false;
    }
    g_objectRuntimeResolveOriginal.store(
        reinterpret_cast<ObjectRuntimeResolve>(g_objectRuntimeResolveHandle.original),
        std::memory_order_release);

    std::byte* const schemaApplyTarget =
        object_target(kAuthoritySchemaApplyRva, kAuthoritySchemaApplyPrefix);
    if (schemaApplyTarget == nullptr) {
        uninstall_activity_schema_decode_probe();
        core::log::write(core::log::Channel::client,
                         core::log::Level::warn,
                         "ev=bootflow stage=activity_authority_schema_gate result=fail reason=target");
        return false;
    }
    const hooking::detour::Spec schemaApplySpec{
        schemaApplyTarget, reinterpret_cast<void*>(&authority_schema_apply)};
    if (!hooking::detour::install(schemaApplySpec, g_authoritySchemaApplyHandle)) {
        uninstall_activity_schema_decode_probe();
        core::log::write(core::log::Channel::client,
                         core::log::Level::warn,
                         "ev=bootflow stage=activity_authority_schema_gate result=fail reason=attach");
        return false;
    }
    g_authoritySchemaApplyOriginal.store(
        reinterpret_cast<AuthoritySchemaApply>(g_authoritySchemaApplyHandle.original),
        std::memory_order_release);

    std::byte* const groupDecodeTarget =
        object_target(kAuthorityGroupDecodeRva, kAuthorityGroupDecodePrefix);
    if (groupDecodeTarget == nullptr) {
        uninstall_activity_schema_decode_probe();
        core::log::write(core::log::Channel::client,
                         core::log::Level::warn,
                         "ev=bootflow stage=omega_phase_two_group_decode result=fail reason=target");
        return false;
    }
    const hooking::detour::Spec groupDecodeSpec{
        groupDecodeTarget, reinterpret_cast<void*>(&authority_group_decode)};
    if (!hooking::detour::install(groupDecodeSpec, g_authorityGroupDecodeHandle)) {
        uninstall_activity_schema_decode_probe();
        core::log::write(core::log::Channel::client,
                         core::log::Level::warn,
                         "ev=bootflow stage=omega_phase_two_group_decode result=fail reason=attach");
        return false;
    }
    g_authorityGroupDecodeOriginal.store(
        reinterpret_cast<AuthorityGroupDecode>(g_authorityGroupDecodeHandle.original),
        std::memory_order_release);

    std::byte* const objectDecodeValidatedPrefix = object_target(
        kAuthorityObjectDecodeRva + kAuthorityObjectDecodeValidatedPrefixOffset,
        kAuthorityObjectDecodePrefix);
    std::byte* const objectDecodeTarget =
        objectDecodeValidatedPrefix != nullptr
            ? objectDecodeValidatedPrefix - kAuthorityObjectDecodeValidatedPrefixOffset
            : nullptr;
    if (objectDecodeTarget == nullptr) {
        uninstall_activity_schema_decode_probe();
        core::log::write(core::log::Channel::client,
                         core::log::Level::warn,
                         "ev=bootflow stage=omega_spawner_object_decode result=fail reason=target");
        return false;
    }
    const hooking::detour::Spec objectDecodeSpec{
        objectDecodeTarget, reinterpret_cast<void*>(&authority_object_decode)};
    if (!hooking::detour::install(objectDecodeSpec, g_authorityObjectDecodeHandle)) {
        uninstall_activity_schema_decode_probe();
        core::log::write(core::log::Channel::client,
                         core::log::Level::warn,
                         "ev=bootflow stage=omega_spawner_object_decode result=fail reason=attach");
        return false;
    }
    g_authorityObjectDecodeOriginal.store(
        reinterpret_cast<AuthorityObjectDecode>(g_authorityObjectDecodeHandle.original),
        std::memory_order_release);

    std::byte* const validationTarget =
        object_target(kAuthorityValidationRva, kAuthorityValidationPrefix);
    if (validationTarget == nullptr) {
        uninstall_activity_schema_decode_probe();
        core::log::write(core::log::Channel::client,
                         core::log::Level::warn,
                         "ev=bootflow stage=activity_authority_validation result=fail reason=target");
        return false;
    }
    const hooking::detour::Spec validationSpec{
        validationTarget, reinterpret_cast<void*>(&authority_validation)};
    if (!hooking::detour::install(validationSpec, g_authorityValidationHandle)) {
        uninstall_activity_schema_decode_probe();
        core::log::write(core::log::Channel::client,
                         core::log::Level::warn,
                         "ev=bootflow stage=activity_authority_validation result=fail reason=attach");
        return false;
    }
    g_authorityValidationOriginal.store(
        reinterpret_cast<AuthorityValidation>(g_authorityValidationHandle.original),
        std::memory_order_release);

    std::byte* const publishCollectTarget =
        object_target(kAuthorityPublishCollectRva, kAuthorityPublishCollectPrefix);
    std::byte* const publishApplyTarget =
        object_target(kAuthorityPublishApplyRva, kAuthorityPublishApplyPrefix);
    std::byte* const publishFinalizeTarget =
        object_target(kAuthorityPublishFinalizeRva, kAuthorityPublishFinalizePrefix);
    if (publishCollectTarget == nullptr || publishApplyTarget == nullptr
        || publishFinalizeTarget == nullptr) {
        uninstall_activity_schema_decode_probe();
        core::log::write(core::log::Channel::client,
                         core::log::Level::warn,
                         "ev=bootflow stage=activity_authority_publish result=fail reason=target");
        return false;
    }
    dump_publish_function(L"authority_publish_collect", publishCollectTarget);
    dump_publish_function(L"authority_publish_apply", publishApplyTarget);
    dump_publish_function(L"authority_publish_finalize", publishFinalizeTarget);
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    dump_publish_function(L"activity_slot_lookup", image + 0x4EA280U);
    dump_publish_function(L"object_descriptor_copy", image + 0xA03560U);
    dump_publish_function(L"object_component_lookup", image + 0x9EB6D0U);
    dump_publish_function(L"object_runtime_resolve", image + 0x9FEC30U);
    dump_publish_function(L"object_runtime_materialize", image + 0x9FF880U);
    dump_publish_function(L"object_runtime_finalize", image + 0x9FF6D0U);
    dump_publish_function(L"activity_slot_lookup_inner", image + 0x4E5840U);
    dump_publish_function(L"activity_slot_register", image + 0x4EA330U);
    dump_publish_function(L"activity_slot_register_inner", image + 0x4EA800U);
    dump_publish_function(L"object_descriptor_resolve", image + 0x4EA140U);
    dump_publish_function(L"object_descriptor_datum", image + 0x4E5D80U);
    dump_publish_function(L"object_runtime_base", image + 0x4E55E0U);
    const hooking::detour::Spec publishCollectSpec{
        publishCollectTarget, reinterpret_cast<void*>(&authority_publish_collect)};
    if (!hooking::detour::install(publishCollectSpec, g_authorityPublishCollectHandle)) {
        uninstall_activity_schema_decode_probe();
        core::log::write(core::log::Channel::client,
                         core::log::Level::warn,
                         "ev=bootflow stage=activity_authority_publish result=fail reason=collect_attach");
        return false;
    }
    g_authorityPublishCollectOriginal.store(
        reinterpret_cast<AuthorityPublishPass>(g_authorityPublishCollectHandle.original),
        std::memory_order_release);
    const hooking::detour::Spec publishApplySpec{
        publishApplyTarget, reinterpret_cast<void*>(&authority_publish_apply)};
    if (!hooking::detour::install(publishApplySpec, g_authorityPublishApplyHandle)) {
        uninstall_activity_schema_decode_probe();
        core::log::write(core::log::Channel::client,
                         core::log::Level::warn,
                         "ev=bootflow stage=activity_authority_publish result=fail reason=apply_attach");
        return false;
    }
    g_authorityPublishApplyOriginal.store(
        reinterpret_cast<AuthorityPublishPass>(g_authorityPublishApplyHandle.original),
        std::memory_order_release);
    const hooking::detour::Spec publishFinalizeSpec{
        publishFinalizeTarget, reinterpret_cast<void*>(&authority_publish_finalize)};
    if (!hooking::detour::install(publishFinalizeSpec, g_authorityPublishFinalizeHandle)) {
        uninstall_activity_schema_decode_probe();
        core::log::write(core::log::Channel::client,
                         core::log::Level::warn,
                         "ev=bootflow stage=activity_authority_publish result=fail reason=finalize_attach");
        return false;
    }
    g_authorityPublishFinalizeOriginal.store(
        reinterpret_cast<AuthorityPublishPass>(g_authorityPublishFinalizeHandle.original),
        std::memory_order_release);
    g_observed.store(0, std::memory_order_release);
    g_schemaApplyObserved.store(0, std::memory_order_release);
    g_validationObserved.store(0, std::memory_order_release);
    g_publishObserved.store(0, std::memory_order_release);
    g_changedObjectsReported.store(false, std::memory_order_release);
    g_activityScriptPostApplyReported.store(false, std::memory_order_release);
    for (std::size_t index = 0; index < g_missionRuntimeHashes.size(); ++index) {
        g_missionRuntimeHashes[index].store(0, std::memory_order_release);
        g_missionRuntimeReported[index].store(false, std::memory_order_release);
    }
    g_omegaSpawnerRuntimeHash.store(0, std::memory_order_release);
    g_omegaSpawnerRuntimeReported.store(false, std::memory_order_release);
    g_omegaSpawnerRuntime.store(0, std::memory_order_release);
    g_omegaSpawnerObject.store(0, std::memory_order_release);
    g_omegaRuntimeFollowupObserved.store(0, std::memory_order_release);
    reset_omega_runtime_tracks();
    g_omegaTriggeredSchemaApplyObserved.store(0, std::memory_order_release);
    g_omegaTriggeredPublishObserved.store(0, std::memory_order_release);
    g_omegaGroupDecodeObserved.store(0, std::memory_order_release);
    g_omegaObjectDecodeObserved.store(0, std::memory_order_release);
    g_omegaObjectStateObserved.store(0, std::memory_order_release);
    g_omegaApplyStateObserved.store(0, std::memory_order_release);
    g_serviceSevenCaptureActive.store(false, std::memory_order_release);
    g_serviceSevenReadCaptureActive.store(false, std::memory_order_release);
    g_serviceSevenObserved.store(0, std::memory_order_release);
    g_serviceSevenResponsesObserved.store(0, std::memory_order_release);
    g_serviceSevenReadLimitReported.store(false, std::memory_order_release);
    g_serviceSevenArmedAt.store(0, std::memory_order_release);
    for (auto& caller : g_serviceSevenCallerRvas) {
        caller.store(0, std::memory_order_release);
    }
    for (auto& caller : g_activityScriptResolverCallerRvas) {
        caller.store(0, std::memory_order_release);
    }
    const bool omegaVisualTrace = install_omega_visual_trace();
    const bool omegaPostSceneTrace = install_omega_post_scene_trace();
    core::log::write(core::log::Channel::client,
                     core::log::Level::info,
                     omegaVisualTrace && omegaPostSceneTrace
                         ? "ev=bootflow stage=activity_authority_stream_probe result=ok mode=observe messages=3 svc7_handler=on script_runtime_resolver=on omega_visual_trace=on omega_post_scene_trace=on"
                         : "ev=bootflow stage=activity_authority_stream_probe result=ok mode=observe messages=3 svc7_handler=on script_runtime_resolver=on omega_visual_trace=partial omega_post_scene_trace=partial");
    return true;
}

/** Arms one exact service-7 response without modifying its bytes or any decoded native state. */
void arm_activity_host_manager_response_decode_probe(std::uint64_t sessionId,
                                                     const std::byte* response,
                                                     std::size_t responseSize) noexcept {
    LateInstallGuard lateInstall;
    if (!lateInstall.accepted() || !g_bitHandle.attached
        || g_original.load(std::memory_order_acquire) == nullptr) {
        return;
    }
    if (response == nullptr || responseSize == 0U) {
        return;
    }

    g_serviceSevenCaptureActive.store(false, std::memory_order_release);
    g_serviceSevenReadCaptureActive.store(false, std::memory_order_release);
    g_serviceSevenObserved.store(0, std::memory_order_release);
    g_serviceSevenResponsesObserved.store(0, std::memory_order_release);
    g_serviceSevenReadLimitReported.store(false, std::memory_order_release);
    for (auto& caller : g_serviceSevenCallerRvas) {
        caller.store(0, std::memory_order_release);
    }
    const std::uint32_t generation =
        g_serviceSevenGeneration.fetch_add(1, std::memory_order_acq_rel) + 1U;

    std::size_t zeroTailBytes = 0U;
    constexpr std::size_t kHeaderBytes = 9U;
    if (responseSize > kHeaderBytes) {
        for (std::size_t index = kHeaderBytes; index < responseSize; ++index) {
            zeroTailBytes += response[index] == std::byte{0} ? 1U : 0U;
        }
    }
    dump_object_buffer(L"activity_host_manager_svc7_response", response, responseSize);

    g_serviceSevenArmedAt.store(GetTickCount64(), std::memory_order_release);
    g_serviceSevenReadCaptureActive.store(true, std::memory_order_release);
    g_serviceSevenCaptureActive.store(true, std::memory_order_release);

    std::array<char, core::log::kLineCapacity> line{};
    const int written = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_host_manager_svc7_decode_arm generation=%u "
        "session_id=0x%016llX bytes=%llu discriminator=%u tail_zero_bytes=%llu "
        "window_ms=%llu mutation=observe_only",
        generation,
        static_cast<unsigned long long>(sessionId),
        static_cast<unsigned long long>(responseSize),
        static_cast<unsigned int>(response[0]),
        static_cast<unsigned long long>(zeroTailBytes),
        static_cast<unsigned long long>(kServiceSevenCaptureMilliseconds));
    if (written > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(written)});
    }
}

/** Detaches both observers and clears their trampolines. */
void uninstall_activity_schema_decode_probe() noexcept {
    uninstall_omega_post_scene_trace();
    uninstall_omega_visual_trace();
    if (g_objectRuntimeResolveHandle.attached) {
        (void)hooking::detour::uninstall(g_objectRuntimeResolveHandle);
    }
    if (g_authorityPublishFinalizeHandle.attached) {
        (void)hooking::detour::uninstall(g_authorityPublishFinalizeHandle);
    }
    if (g_authorityPublishApplyHandle.attached) {
        (void)hooking::detour::uninstall(g_authorityPublishApplyHandle);
    }
    if (g_authorityPublishCollectHandle.attached) {
        (void)hooking::detour::uninstall(g_authorityPublishCollectHandle);
    }
    if (g_authorityValidationHandle.attached) {
        (void)hooking::detour::uninstall(g_authorityValidationHandle);
    }
    if (g_authorityObjectDecodeHandle.attached) {
        (void)hooking::detour::uninstall(g_authorityObjectDecodeHandle);
    }
    if (g_authorityGroupDecodeHandle.attached) {
        (void)hooking::detour::uninstall(g_authorityGroupDecodeHandle);
    }
    if (g_authoritySchemaApplyHandle.attached) {
        (void)hooking::detour::uninstall(g_authoritySchemaApplyHandle);
    }
    if (g_responseHandleInternalHandle.attached) {
        (void)hooking::detour::uninstall(g_responseHandleInternalHandle);
    }
    if (g_boolHandle.attached) {
        (void)hooking::detour::uninstall(g_boolHandle);
    }
    if (g_bitHandle.attached) {
        (void)hooking::detour::uninstall(g_bitHandle);
    }
    g_boolOriginal.store(nullptr, std::memory_order_release);
    g_original.store(nullptr, std::memory_order_release);
    g_responseHandleInternalOriginal.store(nullptr, std::memory_order_release);
    g_authoritySchemaApplyOriginal.store(nullptr, std::memory_order_release);
    g_authorityGroupDecodeOriginal.store(nullptr, std::memory_order_release);
    g_authorityObjectDecodeOriginal.store(nullptr, std::memory_order_release);
    g_authorityValidationOriginal.store(nullptr, std::memory_order_release);
    g_authorityPublishCollectOriginal.store(nullptr, std::memory_order_release);
    g_authorityPublishApplyOriginal.store(nullptr, std::memory_order_release);
    g_authorityPublishFinalizeOriginal.store(nullptr, std::memory_order_release);
    g_objectRuntimeResolveOriginal.store(nullptr, std::memory_order_release);
    g_omegaVisualApplyObserved.store(0U, std::memory_order_release);
    g_omegaVisualCreateObserved.store(0U, std::memory_order_release);
    g_omegaVisualUpdateObserved.store(0U, std::memory_order_release);
    g_omegaVisualSelectObserved.store(0U, std::memory_order_release);
    g_observed.store(0, std::memory_order_release);
    g_schemaApplyObserved.store(0, std::memory_order_release);
    g_validationObserved.store(0, std::memory_order_release);
    g_publishObserved.store(0, std::memory_order_release);
    g_changedObjectsReported.store(false, std::memory_order_release);
    g_activityScriptPostApplyReported.store(false, std::memory_order_release);
    for (std::size_t index = 0; index < g_missionRuntimeHashes.size(); ++index) {
        g_missionRuntimeHashes[index].store(0, std::memory_order_release);
        g_missionRuntimeReported[index].store(false, std::memory_order_release);
    }
    g_omegaSpawnerRuntimeHash.store(0, std::memory_order_release);
    g_omegaSpawnerRuntimeReported.store(false, std::memory_order_release);
    g_omegaSpawnerRuntime.store(0, std::memory_order_release);
    g_omegaSpawnerObject.store(0, std::memory_order_release);
    g_omegaRuntimeFollowupObserved.store(0, std::memory_order_release);
    reset_omega_runtime_tracks();
    g_omegaTriggeredSchemaApplyObserved.store(0, std::memory_order_release);
    g_omegaTriggeredPublishObserved.store(0, std::memory_order_release);
    g_omegaGroupDecodeObserved.store(0, std::memory_order_release);
    g_omegaObjectDecodeObserved.store(0, std::memory_order_release);
    g_omegaObjectStateObserved.store(0, std::memory_order_release);
    g_omegaApplyStateObserved.store(0, std::memory_order_release);
    g_serviceSevenCaptureActive.store(false, std::memory_order_release);
    g_serviceSevenReadCaptureActive.store(false, std::memory_order_release);
    g_serviceSevenObserved.store(0, std::memory_order_release);
    g_serviceSevenResponsesObserved.store(0, std::memory_order_release);
    g_serviceSevenReadLimitReported.store(false, std::memory_order_release);
    g_serviceSevenArmedAt.store(0, std::memory_order_release);
    for (auto& caller : g_serviceSevenCallerRvas) {
        caller.store(0, std::memory_order_release);
    }
    for (auto& caller : g_activityScriptResolverCallerRvas) {
        caller.store(0, std::memory_order_release);
    }
    g_authorityStream = nullptr;
    g_authorityMessage = 0;
    g_omegaTriggeredSchemaApply = 0;
    g_objectDecodeActive = false;
    g_objectDecodeFirst32Set = false;
    g_objectDecodeSawOmega = false;
    g_objectDecodeFirst32 = 0U;
    g_objectDecodeReadCount = 0U;
    g_groupDecodeActive = false;
    g_groupDecodeSawOmega = false;
    g_groupDecodeReadCount = 0U;
    g_groupDecodeChildObjects = 0U;
}

} // namespace sunrise::client::hooks::bootflow
