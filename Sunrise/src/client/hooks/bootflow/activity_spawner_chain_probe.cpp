#include <Windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <intrin.h>
#include <limits>
#include <span>
#include <string_view>

#include "../../../core/logging/log.h"
#include "../../../state/activity/forced/activity_forced_destination.h"
#include "../../hooking/detour.h"
#include "internal.h"

namespace sunrise::client::hooks::bootflow {
namespace {

// Exact pinned-client sites from the 2026-08-20 headless decompile. This probe observes the native
// AI-spawner and scene-renderer paths. All three authored scenes now activate natively. Scene 2's
// model builders remain suppressed, while its terminal retirement branch is retained so its
// authored state can coexist with scene 3 without blocking the separate host-progression latch.
// The disproven candidate-8 provider mutation is disabled. The exact four low-level purple effect
// instances keep the requested +10 Z calibration treatment after the native composer writes them.
constexpr std::uintptr_t kSpawnerApplyRva = 0x4E8FB0U;
constexpr std::uintptr_t kSpawnerDeficitRva = 0x4E4580U;
constexpr std::uintptr_t kSpawnerRequestRva = 0x4E2E80U;
constexpr std::uintptr_t kSpawnerSquadResolveRva = 0x4E6480U;
constexpr std::uintptr_t kSpawnerMemberBuildRva = 0x4EC4C0U;
constexpr std::uintptr_t kSpawnerQueueFindRva = 0x4E4720U;
constexpr std::uintptr_t kSpawnerDrainRva = 0x4E8AE0U;
constexpr std::uintptr_t kEntityFactoryRva = 0x56D9B0U;
// FUN_7ff6186002c0. This scene actor scheduler walks the authored actor-slot records at
// scene-base+0x170 and invokes the common entity factory once for each unresolved slot. Observing
// this boundary distinguishes intentional cast members from duplicate spawner requests.
constexpr std::uintptr_t kSceneActorSchedulerRva = 0x5902C0U;
// +B3F620 resolves each active type-43 entry through +501AD0 before entering the common runtime
// iterator. The Towerfall crash stack proves the resolver-failure branch supplies null to that
// iterator; observing this pair identifies the unresolved reference without touching native state.
constexpr std::uintptr_t kSceneEntryUpdateRva = 0xB3F620U;
constexpr std::uintptr_t kSceneEntryResolverRva = 0x501AD0U;
constexpr std::uintptr_t kSceneEntryResolverReturnRva = 0xB3F678U;
// FUN_7ff6185fb9a0. A low-byte transition value of 0xFF enters the scene-owned actor's terminal
// retirement branch. Scene event completion reaches this boundary after scene 3 has already been
// activated, which makes it the narrow point where scene 2 can be retained without freezing the
// authored event scheduler or the mission timeline.
constexpr std::uintptr_t kSceneTransitionRetireRva = 0x58B9A0U;
constexpr std::uintptr_t kObjectFinalizeRva = 0x56C6B0U;
constexpr std::uintptr_t kEntityForObjectRva = 0x4D7110U;
constexpr std::uintptr_t kComponentStartRva = 0xB31910U;
constexpr std::uintptr_t kBehaviorStepRva = 0xC2FF50U;
constexpr std::uintptr_t kBehaviorNodeRva = 0xA21520U;
constexpr std::uintptr_t kIndexHeapReleaseRva = 0x34F790U;
// FUN_7ff6185e5690 in the pinned Ghidra program. This is the common placed-object/effect
// creation boundary used by the native 0x8080992F visual component and four other callers.
// It receives the complete authored descriptor (including the optional relative attachment/
// transform block) and returns the created object handle.
constexpr std::uintptr_t kSceneVisualObjectCreateRva = 0x575690U;
// FUN_7ff618a5fbc0 is the 0x8080992F visual-component entry creator. It materializes
// the entry's transform into the common +575690 descriptor, creates the object, binds
// that object to the owning component graph, and commits the result. Recording this
// complete call is the first point where the scene-bank transform can be correlated
// with the final object and its native parent/proxy relationship.
constexpr std::uintptr_t kVisualEntryCreateRva = 0x9EFBC0U;
constexpr std::uintptr_t kVisualTransformApplyRva = 0x5012E0U;
constexpr std::uintptr_t kVisualAttachmentBindRva = 0x3F8DE0U;
constexpr std::uintptr_t kVisualEntryTransformApplyReturnRva = 0x9EFCFFU;
constexpr std::uintptr_t kVisualEntryObjectCreateReturnRva = 0x9EFD4BU;
constexpr std::uintptr_t kVisualEntryAttachmentBindReturnRva = 0x9EFE5AU;
// FUN_7ff6185f73f0 in the pinned image. This is the leaf authored-scene event dispatcher: it
// resolves one authored event, selects its 0x60-byte handler-table row by the event type at +0x30,
// then invokes the start callback at row +0x08 with the live scene/cast context.
constexpr std::uintptr_t kSceneAuthoredEventDispatchRva = 0x5873F0U;
// Exact event-specific callback and its selector-to-transform resolver. The callback's temporary
// visual object is recycled by the outer dispatcher, so its owner must be observed here.
constexpr std::uintptr_t kSceneType23CallbackRva = 0x11EE970U;
constexpr std::uintptr_t kSceneType23TransformResolveRva = 0x58A150U;
// FUN_7ff6185feb40. +58FA20 calls this once for each authored transform-source row.
// The row names its destination bank range and cast slot, so this is the first boundary that can
// attribute bank entry 1 to its real writer instead of inferring provenance from later readers.
constexpr std::uintptr_t kSceneTransformSourceUpdateRva = 0x58EB40U;
// FUN_7ff618a8f360. Source kinds 2/4 pass their 0x18-byte runtime row here. The resolver
// follows the row's datum/relative pair to a separate provider before producing 0x50-byte
// transform candidates. Recording this boundary distinguishes the row itself from the provider
// it selects; neither the static decompile nor this probe assumes that component +0x43C is read.
constexpr std::uintptr_t kSceneTransformKind2ResolveRva = 0xA1F360U;
// FUN_7ff618a8f640. +A1F360 passes its resolved provider as argument 1 here. The treatment hook
// substitutes the structurally equivalent factory-2 provider only under the exact writer TLS gate.
constexpr std::uintptr_t kSceneTransformKind2ProviderResolveRva = 0xA1F640U;
// FUN_7ff618a8e8a0. +A1F640 calls this once for each selected provider row. The row's
// second dword is passed to the provider's virtual pose resolver, making this the narrowest
// stable boundary that exposes the actual authored bone/socket key and returned transform.
constexpr std::uintptr_t kSceneTransformPoseCandidateResolveRva = 0xA1E8A0U;
// FUN_7ff618ba14a0. This 16-byte tail-dispatch thunk unwraps the two-word pose interface passed
// by +A1E8A0, then forwards the authored binding key, flags, and 0x20-byte transform output to
// the concrete pose object. Hooking the thunk exposes other native users of the same socket key,
// which lets the bad VFX pose be compared with the visible scene-1 renderer pose without changing
// either interface or guessing a vtable pointer.
constexpr std::uintptr_t kPoseSocketDispatchRva = 0xB314A0U;
// FUN_7ff619260510 is the corrected, live-image-mapped per-effect transform composer. The effect
// creation path calls it at +1206E14 and the per-frame effect loop calls it at +121074F. It writes
// the final 0x20-byte transform directly at the start of the effect object.
constexpr std::uintptr_t kEffectTransformComposeRva = 0x11F0510U;
constexpr std::uintptr_t kEffectTransformCreateCallsiteRva = 0x1206E14U;
constexpr std::uintptr_t kEffectTransformUpdateCallsiteRva = 0x121074FU;
// The 0x80EC13A2 presentation component owns five small renderer-object wrappers. They are
// intentionally hooked instead of the broad renderer service: four wrappers are exclusive to
// the component graph and the fifth is filtered by the active component-start context.
constexpr std::uintptr_t kRendererBulkBooleanRva = 0x444A00U;
constexpr std::uintptr_t kRendererBulkSiblingRva = 0x444A80U;
constexpr std::uintptr_t kRendererObjectARva = 0x4453F0U;
constexpr std::uintptr_t kRendererObjectBRva = 0x445430U;
constexpr std::uintptr_t kRendererObjectCRva = 0x445470U;
// FUN_7ff6184d4300. This is the presentation component's native body-state gate: it stores
// the requested state at +0x240, and the component later passes that byte to the renderer's
// bulk enable call. Scene 0x80EC0FA8 owns the wanted portal VFX and the unwanted visible body,
// so this is a narrower boundary than suppressing the scene or guessing renderer-array offsets.
constexpr std::uintptr_t kPresentationBodyStateRva = 0x464300U;

constexpr std::uintptr_t kObjectIndexHeapRva = 0x1F93420U;
constexpr std::uintptr_t kObjectTablePointerRva = 0x1F93428U;
constexpr std::uintptr_t kObjectTableStrideRva = 0x1F93430U;
// +1087F70 reads the live object root from these protected datum fields. Keep this independent
// from factory descriptors: the cinematic actors move after construction, while their descriptors
// retain only the common authored placement.
constexpr std::uintptr_t kPositionMaskOneRva = 0x1B9E420U;
constexpr std::uintptr_t kPositionMaskTwoRva = 0x1B9E430U;
constexpr std::uintptr_t kPositionKeyXyRva = 0x6260781U;
constexpr std::uintptr_t kPositionKeyZwRva = 0x584DFF2U;
constexpr std::size_t kObjectPositionOffset = 0xD0U;
constexpr std::uintptr_t kScenePoolTablePointerRva = 0x2439C70U;
constexpr std::uintptr_t kSceneEventHandlerTableRva = 0x2744AF0U;
constexpr std::size_t kSceneEventHandlerStride = 0x60U;

constexpr std::uint32_t kOmegaSpawnerRegistry = 0xD00142CFU;
constexpr std::uint32_t kAbsentHash = 0x811C9DC5U;
constexpr std::uint32_t kIkoraEntityDefinition = 0x80EC0F27U;
constexpr std::uint32_t kOmegaPortalSceneHandle = 0x80C51CBEU;
constexpr std::uint32_t kOmegaIkoraOpeningSceneHandle = 0x80FCCE87U;
constexpr std::uint32_t kOmegaPurpleEffectOne = 0x80B9FDBEU;
constexpr std::uint32_t kOmegaPurpleEffectTwo = 0x80C220EBU;
// Confirmed by the low-level effect provenance capture. These are the complete set of effect
// handles constructed at the two purple type-23 event timestamps.
constexpr std::array<std::uint32_t, 4U> kOmegaPurpleLowLevelEffectHandles{
    0x80C71D8EU,
    0x80C71D8AU,
    0x80F1FCCAU,
    0x80C71D70U};
constexpr float kOmegaPurpleDirectEffectWorldZOffset = 10.0F;
constexpr bool kEnableKind2ProviderAb = false;
constexpr bool kEnableOmegaSceneTwoPresentationSuppression = false;
constexpr bool kEnableOmegaSceneTwoBodyStateSuppression = false;
// Scene 2's body resources enter distinct construction callbacks.  The primary 808072BD
// component builds through +1225BB0; the 80807286 mesh/cloth component builds through +11761F0,
// which invokes the same base builder internally.  A third 808072BD resource is the head model
// on a nested child object. Suppress only those three dispatches for scene 2 so the actor/animation
// graph and all authored portal events remain native.
constexpr bool kEnableOmegaSceneTwoModelSuppression = true;
constexpr bool kEnableOmegaIkoraSceneTwoOnlyAb = false;
// Keep the old scene-1/2-only A/B disabled: scene 3 must activate for normal mission progression.
constexpr bool kEnableOmegaSceneOneWithVfxAb = false;
// Retain only scene 2's terminal actor-retirement transition. Event completion and scene 3 remain
// native, and the existing deferred host latch substitutes for the intentionally skipped release.
constexpr bool kEnableOmegaSceneTwoInfinitePersistence = true;
// The first pose-interface treatments retained scene-creation objects/providers whose roles had
// changed by the purple bank update. Keep that late treatment disabled. The +A1F640 treatment now
// keeps the exact active provider and live pose interface, changing only candidate row 7 to row 8.
constexpr bool kEnableOmegaPurplePoseRebind = false;
static_assert(!(kEnableOmegaIkoraSceneTwoOnlyAb && kEnableOmegaSceneOneWithVfxAb),
              "Omega scene-cast A/B modes are mutually exclusive");
constexpr std::uint32_t kIkoraTransformProviderComponentDefinition = 0x8161FB60U;
// Confirmed in repeated native +A1E8A0 row captures for both purple type-23 events.
constexpr std::uint32_t kOmegaPurplePoseBindingKey = 0x15U;
constexpr std::uint32_t kOmegaSceneOnePoseCandidateIndex = 8U;
constexpr std::uint32_t kOmegaSceneOnePoseSelection = 0xA02A6431U;
constexpr std::uint32_t kOmegaPurplePoseCandidateIndex = 7U;
constexpr std::uint32_t kOmegaPurplePoseSelection = 0x98DA9A6BU;
constexpr std::uint64_t kOmegaScenePoseObjectIdentity = 0x8080854680EC139FULL;
constexpr std::uint64_t kOmegaScenePoseObjectSize = 0x550ULL;
constexpr std::size_t kIkoraTransformProviderProbeBytes = 0x80U;
constexpr std::size_t kIkoraTransformProviderScanAlignment = sizeof(std::uint64_t);
// The three native one-member scene casts observed during Omega's opening. Keep these neutral
// labels until the authority correlation proves their exact authored roles.
constexpr std::uint32_t kOmegaIkoraSceneOneHandle = 0x80EC0F0EU;
constexpr std::uint32_t kOmegaIkoraSceneTwoHandle = 0x80EC0FA8U;
constexpr std::uint32_t kOmegaIkoraSceneThreeHandle = 0x80EC0FA6U;
constexpr std::uint32_t kIkoraPresentationDefinition = 0x80EC13A2U;
constexpr std::uint32_t kIkoraPrimaryModelDefinition = 0x80EC0F17U;
constexpr std::uint32_t kIkoraMeshClothModelDefinition = 0x80EC0F1DU;
constexpr std::uint32_t kIkoraHeadModelDefinition = 0x80F2EC2FU;
constexpr std::uintptr_t kIkoraPrimaryModelBuildRva = 0x1225BB0U;
constexpr std::uintptr_t kIkoraMeshClothModelBuildRva = 0x11761F0U;
constexpr std::uintptr_t kIkoraHeadModelBuildRva = 0x1225BB0U;
constexpr std::uint32_t kSquadIkoraDefinition = 0x80F47B70U;
constexpr std::uint32_t kSceneIkoraDefinition = 0x80F47B73U;
constexpr std::uint32_t kGateControllerDefinition = 0x80F47BA0U;
constexpr std::uint32_t kEngagementSensorDefinition = 0x80F47BA3U;
constexpr std::uint32_t kVignettePointDefinition = 0x80F47BA6U;
constexpr std::uint32_t kDialoguePointDefinition = 0x80F47BA9U;
constexpr std::uint32_t kTriggerVolumeDefinition = 0x80F47B6CU;
constexpr std::uint32_t kInvalidHandle = 0xFFFFFFFFU;
constexpr std::uint32_t kMaximumApplyLogs = 256U;
constexpr std::uint32_t kMaximumChainLogs = 128U;
constexpr std::uint32_t kMaximumFactoryLogs = 64U;
constexpr std::uint32_t kMaximumBehaviorCandidateLogs = 64U;
constexpr std::uint32_t kMaximumBehaviorNodeLogs = 256U;
constexpr std::uint32_t kMaximumSceneVisualCreateLogs = 256U;
constexpr std::uint32_t kMaximumSceneEntryResolverLogs = 512U;
constexpr std::uint32_t kMaximumVisualEntryRecorderLogs = 512U;
constexpr std::uint32_t kMaximumSceneAuthoredEventLogs = 4096U;
constexpr std::uint32_t kMaximumSceneEventResolveFailureLogs = 256U;
constexpr std::uint32_t kMaximumSceneType23Samples = 512U;
constexpr std::uint32_t kMaximumSceneTransformWriterSamples = 512U;
constexpr std::uint32_t kMaximumSceneTransformKind2Samples = 64U;
constexpr std::size_t kMaximumScenePoseCandidatesPerResolve = 8U;
constexpr std::uint32_t kMaximumScenePoseTargetSamples = 64U;
constexpr std::uint32_t kMaximumPoseSocketDispatchSamples = 256U;
constexpr std::size_t kMaximumPoseSocketTraceKeys = 128U;
constexpr std::uint32_t kMaximumEffectTransformSamples = 512U;
constexpr std::size_t kMaximumEffectTransformTraceKeys = 512U;
constexpr std::uint64_t kEffectTransformArmWindowMs = 500U;
constexpr std::uint64_t kEffectTransformHeartbeatMs = 250U;
constexpr float kEffectTransformNearVfxDistance = 192.0F;
constexpr std::uint32_t kMaximumPoseProviderProvenanceSamples = 32U;
constexpr std::size_t kMaximumPoseProviderProvenanceTraceKeys = 32U;
constexpr std::uint64_t kPoseSocketHeartbeatMs = 100U;
constexpr std::uint32_t kMaximumRendererHandleSamples = 1024U;
constexpr std::int32_t kMaximumRendererObjectArrayCount = 256;
constexpr std::size_t kMaximumSceneTwoPresentationRendererObjects = 16U;
constexpr std::size_t kRendererCorrelationBytes = 0x200U;
constexpr std::uint64_t kSceneEventHeartbeatMs = 1000U;
constexpr std::size_t kMaximumSceneEventTraceStates = 256U;
constexpr std::size_t kMaximumSceneType23TraceStates = 32U;
constexpr std::uint64_t kFollowupSampleMs = 1000U;
constexpr std::uint64_t kHandoffRollingSampleMs = 16U;
constexpr std::size_t kBehaviorHandleScanBytes = 0x800U;
// One Ikora factory expands into roughly 55 root and nested entity components. The opening keeps
// the preload stand-in, scene performer, and persistent replacement alive across one handoff, so
// 64 entries silently discarded the entire third actor in the first recorder build.
constexpr std::size_t kMaximumIkoraComponents = 256U;
constexpr std::size_t kMaximumIkoraActors = 8U;
constexpr std::size_t kHandoffSnapshotBytes = 0x100U;
// The opening can keep three complete Ikora graphs alive: preload stand-in, scene
// performer, and persistent replacement. Keep enough rolling slots for all of them.
constexpr std::size_t kMaximumHandoffComponents = kMaximumIkoraComponents;
constexpr std::size_t kMaximumBindingOffsets = 24U;
constexpr std::size_t kNativeStackDepth = 16U;
constexpr std::size_t kMaximumCapturedSceneTransforms = 6U;
constexpr std::size_t kMaximumPresentationFingerprintPointers = 768U;
constexpr std::size_t kMaximumAttachmentGraphScanBytes = 0x1000U;
constexpr std::size_t kMaximumPresentationNestedScanBytes = 0x200U;

constexpr std::array<std::byte, 16> kSpawnerApplyPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x10}, std::byte{0x57}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x30}, std::byte{0x48}, std::byte{0x8B},
    std::byte{0xFA}, std::byte{0x48}, std::byte{0x8B}, std::byte{0xD9}};
constexpr std::array<std::byte, 16> kSpawnerDeficitPrefix{
    std::byte{0x4C}, std::byte{0x8B}, std::byte{0xDC}, std::byte{0x49},
    std::byte{0x89}, std::byte{0x5B}, std::byte{0x10}, std::byte{0x49},
    std::byte{0x89}, std::byte{0x6B}, std::byte{0x18}, std::byte{0x56},
    std::byte{0x57}, std::byte{0x41}, std::byte{0x56}, std::byte{0x48}};
constexpr std::array<std::byte, 16> kSpawnerRequestPrefix{
    std::byte{0x48}, std::byte{0x8B}, std::byte{0xC4}, std::byte{0x48},
    std::byte{0x89}, std::byte{0x58}, std::byte{0x10}, std::byte{0x48},
    std::byte{0x89}, std::byte{0x70}, std::byte{0x18}, std::byte{0x55},
    std::byte{0x57}, std::byte{0x41}, std::byte{0x54}, std::byte{0x41}};
constexpr std::array<std::byte, 16> kSpawnerSquadResolvePrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x08}, std::byte{0x48}, std::byte{0x89}, std::byte{0x74},
    std::byte{0x24}, std::byte{0x10}, std::byte{0x57}, std::byte{0x48},
    std::byte{0x83}, std::byte{0xEC}, std::byte{0x20}, std::byte{0x44}};
constexpr std::array<std::byte, 16> kSpawnerMemberBuildPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x54}, std::byte{0x24},
    std::byte{0x10}, std::byte{0x57}, std::byte{0x41}, std::byte{0x55},
    std::byte{0x41}, std::byte{0x56}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x40}, std::byte{0x49}, std::byte{0x8B}};
constexpr std::array<std::byte, 16> kSpawnerQueueFindPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x74}, std::byte{0x24},
    std::byte{0x20}, std::byte{0x41}, std::byte{0x56}, std::byte{0x48},
    std::byte{0x83}, std::byte{0xEC}, std::byte{0x20}, std::byte{0x83},
    std::byte{0x39}, std::byte{0x50}, std::byte{0x4C}, std::byte{0x8B}};
constexpr std::array<std::byte, 16> kSpawnerDrainPrefix{
    std::byte{0x40}, std::byte{0x55}, std::byte{0x41}, std::byte{0x54},
    std::byte{0x41}, std::byte{0x55}, std::byte{0x41}, std::byte{0x57},
    std::byte{0x48}, std::byte{0x8D}, std::byte{0xAC}, std::byte{0x24},
    std::byte{0x08}, std::byte{0xD0}, std::byte{0xFF}, std::byte{0xFF}};
constexpr std::array<std::byte, 16> kEntityFactoryPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x18}, std::byte{0x48}, std::byte{0x89}, std::byte{0x6C},
    std::byte{0x24}, std::byte{0x20}, std::byte{0x56}, std::byte{0x57},
    std::byte{0x41}, std::byte{0x56}, std::byte{0x48}, std::byte{0x81}};
constexpr std::array<std::byte, 24> kSceneActorSchedulerPrefix{
    std::byte{0x4C}, std::byte{0x8B}, std::byte{0xDC}, std::byte{0x55},
    std::byte{0x56}, std::byte{0x41}, std::byte{0x55}, std::byte{0x41},
    std::byte{0x57}, std::byte{0x49}, std::byte{0x8D}, std::byte{0xAB},
    std::byte{0xE8}, std::byte{0xF7}, std::byte{0xFF}, std::byte{0xFF},
    std::byte{0x48}, std::byte{0x81}, std::byte{0xEC}, std::byte{0xF8},
    std::byte{0x08}, std::byte{0x00}, std::byte{0x00}, std::byte{0x48}};
constexpr std::array<std::byte, 16> kSceneEntryUpdatePrefix{
    std::byte{0x40}, std::byte{0x56}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x30}, std::byte{0x80}, std::byte{0xB9},
    std::byte{0x60}, std::byte{0x02}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x48}, std::byte{0x8B}, std::byte{0xF1}};
constexpr std::array<std::byte, 18> kSceneEntryResolverPrefix{
    std::byte{0x40}, std::byte{0x53}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x20}, std::byte{0x48}, std::byte{0x0F},
    std::byte{0xBE}, std::byte{0x41}, std::byte{0x04}, std::byte{0x48},
    std::byte{0x8B}, std::byte{0xDA}, std::byte{0x83}, std::byte{0xF8},
    std::byte{0x3C}, std::byte{0x77}};
constexpr std::array<std::byte, 24> kSceneTransitionRetirePrefix{
    std::byte{0x40}, std::byte{0x57}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x40}, std::byte{0x48}, std::byte{0x8B},
    std::byte{0xF9}, std::byte{0x80}, std::byte{0xFA}, std::byte{0xFF},
    std::byte{0x0F}, std::byte{0x84}, std::byte{0x2F}, std::byte{0x01},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x48}, std::byte{0x89},
    std::byte{0x5C}, std::byte{0x24}, std::byte{0x50}, std::byte{0x33}};
constexpr std::array<std::byte, 16> kObjectFinalizePrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x08}, std::byte{0x57}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x20}, std::byte{0x8B}, std::byte{0xF9},
    std::byte{0x8B}, std::byte{0xD9}, std::byte{0x81}, std::byte{0xE7}};
constexpr std::array<std::byte, 16> kEntityForObjectPrefix{
    std::byte{0x40}, std::byte{0x56}, std::byte{0x57}, std::byte{0x48},
    std::byte{0x83}, std::byte{0xEC}, std::byte{0x28}, std::byte{0xB8},
    std::byte{0x80}, std::byte{0x02}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x0F}, std::byte{0xB6}, std::byte{0xF2}, std::byte{0x48}};
constexpr std::array<std::byte, 16> kComponentStartPrefix{
    std::byte{0x48}, std::byte{0x8B}, std::byte{0x11}, std::byte{0x48},
    std::byte{0x8B}, std::byte{0x49}, std::byte{0x08}, std::byte{0x48},
    std::byte{0x8B}, std::byte{0x42}, std::byte{0x18}, std::byte{0x48},
    std::byte{0xFF}, std::byte{0x64}, std::byte{0x02}, std::byte{0x30}};
constexpr std::array<std::byte, 16> kBehaviorStepPrefix{
    std::byte{0x40}, std::byte{0x55}, std::byte{0x53}, std::byte{0x56},
    std::byte{0x57}, std::byte{0x41}, std::byte{0x55}, std::byte{0x41},
    std::byte{0x56}, std::byte{0x41}, std::byte{0x57}, std::byte{0x48},
    std::byte{0x8D}, std::byte{0xAC}, std::byte{0x24}, std::byte{0xB0}};
constexpr std::array<std::byte, 16> kBehaviorNodePrefix{
    std::byte{0x41}, std::byte{0x55}, std::byte{0x41}, std::byte{0x56},
    std::byte{0x48}, std::byte{0x81}, std::byte{0xEC}, std::byte{0x38},
    std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x48},
    std::byte{0x8B}, std::byte{0x05}, std::byte{0x56}, std::byte{0x85}};
constexpr std::array<std::byte, 16> kIndexHeapReleasePrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x6C}, std::byte{0x24},
    std::byte{0x18}, std::byte{0x56}, std::byte{0x41}, std::byte{0x56},
    std::byte{0x41}, std::byte{0x57}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x20}, std::byte{0x0F}, std::byte{0xB7}};
constexpr std::array<std::byte, 20> kSceneVisualObjectCreatePrefix{
    // The live image retains a redundant REX prefix on PUSH RBP (`40 55`) that Ghidra's
    // instruction rendering hides. Keep it in the byte signature so validation is exact.
    std::byte{0x40}, std::byte{0x55}, std::byte{0x53}, std::byte{0x56},
    std::byte{0x41}, std::byte{0x56}, std::byte{0x41}, std::byte{0x57},
    std::byte{0x48}, std::byte{0x8D}, std::byte{0x6C}, std::byte{0x24},
    std::byte{0xA0}, std::byte{0x48}, std::byte{0x81}, std::byte{0xEC},
    std::byte{0x60}, std::byte{0x01}, std::byte{0x00}, std::byte{0x00}};
constexpr std::array<std::byte, 20> kVisualEntryCreatePrefix{
    std::byte{0x40}, std::byte{0x55}, std::byte{0x53}, std::byte{0x57},
    std::byte{0x41}, std::byte{0x54}, std::byte{0x41}, std::byte{0x56},
    std::byte{0x48}, std::byte{0x8D}, std::byte{0xAC}, std::byte{0x24},
    std::byte{0x20}, std::byte{0xF0}, std::byte{0xFF}, std::byte{0xFF},
    std::byte{0xB8}, std::byte{0xE0}, std::byte{0x10}, std::byte{0x00}};
constexpr std::array<std::byte, 16> kVisualTransformApplyPrefix{
    std::byte{0x4C}, std::byte{0x8B}, std::byte{0xDC}, std::byte{0x55},
    std::byte{0x56}, std::byte{0x57}, std::byte{0x41}, std::byte{0x56},
    std::byte{0x41}, std::byte{0x57}, std::byte{0x49}, std::byte{0x8D},
    std::byte{0x6B}, std::byte{0xA1}, std::byte{0x48}, std::byte{0x81}};
constexpr std::array<std::byte, 16> kVisualAttachmentBindPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x08}, std::byte{0x48}, std::byte{0x89}, std::byte{0x6C},
    std::byte{0x24}, std::byte{0x10}, std::byte{0x48}, std::byte{0x89},
    std::byte{0x74}, std::byte{0x24}, std::byte{0x18}, std::byte{0x48}};
constexpr std::array<std::byte, 15> kSceneAuthoredEventDispatchPrefix{
    std::byte{0x48}, std::byte{0x8B}, std::byte{0xC4}, std::byte{0x55},
    std::byte{0x57}, std::byte{0x48}, std::byte{0x81}, std::byte{0xEC},
    std::byte{0xD8}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x44}, std::byte{0x8B}, std::byte{0x11}};
constexpr std::array<std::byte, 19> kSceneType23CallbackPrefix{
    std::byte{0x40}, std::byte{0x53}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x30}, std::byte{0x48}, std::byte{0x89},
    std::byte{0x74}, std::byte{0x24}, std::byte{0x50}, std::byte{0x83},
    std::byte{0xCE}, std::byte{0xFF}, std::byte{0x48}, std::byte{0x89},
    std::byte{0x7C}, std::byte{0x24}, std::byte{0x58}};
constexpr std::array<std::byte, 16> kSceneType23TransformResolvePrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x10}, std::byte{0x48}, std::byte{0x89}, std::byte{0x74},
    std::byte{0x24}, std::byte{0x18}, std::byte{0x57}, std::byte{0x48},
    std::byte{0x83}, std::byte{0xEC}, std::byte{0x40}, std::byte{0x44}};
constexpr std::array<std::byte, 20> kSceneTransformSourceUpdatePrefix{
    // The live image retains the redundant REX prefix on PUSH RBP (`40 55`).
    std::byte{0x40}, std::byte{0x55}, std::byte{0x53}, std::byte{0x56},
    std::byte{0x57}, std::byte{0x41}, std::byte{0x56}, std::byte{0x41},
    std::byte{0x57}, std::byte{0x48}, std::byte{0x8D}, std::byte{0xAC},
    std::byte{0x24}, std::byte{0xE8}, std::byte{0xD7}, std::byte{0xFF},
    std::byte{0xFF}, std::byte{0xB8}, std::byte{0x18}, std::byte{0x29}};
constexpr std::array<std::byte, 17> kSceneTransformKind2ResolvePrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x10}, std::byte{0x48}, std::byte{0x89}, std::byte{0x6C},
    std::byte{0x24}, std::byte{0x18}, std::byte{0x48}, std::byte{0x89},
    std::byte{0x7C}, std::byte{0x24}, std::byte{0x20}, std::byte{0x41},
    std::byte{0x56}};
constexpr std::array<std::byte, 20> kSceneTransformKind2ProviderResolvePrefix{
    std::byte{0x89}, std::byte{0x54}, std::byte{0x24}, std::byte{0x10},
    std::byte{0x53}, std::byte{0x55}, std::byte{0x56}, std::byte{0x41},
    std::byte{0x54}, std::byte{0x41}, std::byte{0x55}, std::byte{0x41},
    std::byte{0x57}, std::byte{0x48}, std::byte{0x81}, std::byte{0xEC},
    std::byte{0x88}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
constexpr std::array<std::byte, 20> kSceneTransformPoseCandidateResolvePrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x08}, std::byte{0x48}, std::byte{0x89}, std::byte{0x6C},
    std::byte{0x24}, std::byte{0x10}, std::byte{0x48}, std::byte{0x89},
    std::byte{0x74}, std::byte{0x24}, std::byte{0x18}, std::byte{0x48},
    std::byte{0x89}, std::byte{0x7C}, std::byte{0x24}, std::byte{0x20}};
constexpr std::array<std::byte, 16> kPoseSocketDispatchPrefix{
    std::byte{0x4C}, std::byte{0x8B}, std::byte{0x11}, std::byte{0x48},
    std::byte{0x8B}, std::byte{0x49}, std::byte{0x08}, std::byte{0x49},
    std::byte{0x8B}, std::byte{0x42}, std::byte{0x18}, std::byte{0x4A},
    std::byte{0xFF}, std::byte{0x64}, std::byte{0x10}, std::byte{0x48}};
constexpr std::array<std::byte, 16> kEffectTransformComposePrefix{
    std::byte{0x40}, std::byte{0x53}, std::byte{0x55}, std::byte{0x56},
    std::byte{0x57}, std::byte{0x41}, std::byte{0x56}, std::byte{0x48},
    std::byte{0x83}, std::byte{0xEC}, std::byte{0x50}, std::byte{0x0F},
    std::byte{0x29}, std::byte{0x74}, std::byte{0x24}, std::byte{0x40}};
constexpr std::array<std::byte, 16> kRendererBulkBooleanPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x08}, std::byte{0x48}, std::byte{0x89}, std::byte{0x74},
    std::byte{0x24}, std::byte{0x18}, std::byte{0x57}, std::byte{0x48},
    std::byte{0x83}, std::byte{0xEC}, std::byte{0x20}, std::byte{0x41}};
constexpr std::array<std::byte, 16> kRendererBulkSiblingPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x08}, std::byte{0x57}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x20}, std::byte{0x8B}, std::byte{0xDA},
    std::byte{0x48}, std::byte{0x8B}, std::byte{0xF9}, std::byte{0x83}};
constexpr std::array<std::byte, 16> kRendererObjectAPrefix{
    std::byte{0x40}, std::byte{0x53}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x20}, std::byte{0x48}, std::byte{0x8B},
    std::byte{0xD9}, std::byte{0x48}, std::byte{0x8D}, std::byte{0x4C},
    std::byte{0x24}, std::byte{0x30}, std::byte{0xE8}, std::byte{0x7D}};
constexpr std::array<std::byte, 16> kRendererObjectBPrefix{
    std::byte{0x40}, std::byte{0x53}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x20}, std::byte{0x48}, std::byte{0x8B},
    std::byte{0xD9}, std::byte{0x48}, std::byte{0x8D}, std::byte{0x4C},
    std::byte{0x24}, std::byte{0x38}, std::byte{0xE8}, std::byte{0x3D}};
constexpr std::array<std::byte, 16> kRendererObjectCPrefix{
    std::byte{0x40}, std::byte{0x53}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x20}, std::byte{0x48}, std::byte{0x8B},
    std::byte{0xD9}, std::byte{0x48}, std::byte{0x8D}, std::byte{0x4C},
    std::byte{0x24}, std::byte{0x38}, std::byte{0xE8}, std::byte{0xFD}};
constexpr std::array<std::byte, 10> kPresentationBodyStatePrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x6C}, std::byte{0x24},
    std::byte{0x20}, std::byte{0x56}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x60}};
using SpawnerApply = void(__fastcall*)(std::uint32_t* instance,
                                        std::uint32_t* message) noexcept;
using SpawnerDeficit = void(__fastcall*)(std::uint32_t* instance,
                                          std::uint32_t reason,
                                          const std::byte* payload) noexcept;
using SpawnerRequest = void(__fastcall*)(std::byte* instance,
                                          std::uint64_t reason,
                                          const std::int32_t* request,
                                          std::int32_t* result) noexcept;
using SpawnerSquadResolve = void(__fastcall*)(std::uint32_t* instance,
                                               char preferPrimary,
                                               std::int32_t* squad,
                                               std::uint64_t* auxiliary) noexcept;
using SpawnerMemberBuild = void(__fastcall*)(std::uint32_t* instance,
                                              const std::int32_t* request,
                                              std::int32_t* members) noexcept;
using SpawnerQueueFind = std::int32_t(__fastcall*)(std::int32_t* queue,
                                                    const std::int32_t* squad) noexcept;
using SpawnerDrain = void(__fastcall*)(std::uint32_t* queue) noexcept;
using EntityFactory = std::int32_t*(__fastcall*)(std::int32_t* result,
                                                  const std::byte* descriptor,
                                                  std::uint32_t table,
                                                  std::int32_t record) noexcept;
using SceneActorScheduler = void(__fastcall*)(std::uint32_t* scene) noexcept;
using SceneEntryUpdate = void(__fastcall*)(std::byte* component) noexcept;
using SceneEntryResolver =
    bool(__fastcall*)(const std::byte* reference, std::byte* output) noexcept;
using SceneTransitionRetire = void(__fastcall*)(std::uint32_t* scene,
                                                 std::uint32_t transition,
                                                 char transitionFlag,
                                                 char alreadyProcessed,
                                                 char forceRetire) noexcept;
using ObjectFinalize = void(__fastcall*)(std::uint32_t object) noexcept;
using EntityForObject = void(__fastcall*)(const std::byte* record, char flag) noexcept;
// +B31910 is a tail-dispatch stub, not a conventional one-argument wrapper. It rewrites RCX/RDX
// to the component instance/descriptor and deliberately preserves XMM1, R8, and R9 for the
// selected lifecycle handler. Model and forward those passthrough registers explicitly; calling
// it with only `entry` leaves R8/R9 as compiler scratch and corrupts handlers such as 80BC9228.
using ComponentStart = void(__fastcall*)(const std::uintptr_t* entry,
                                          float elapsed,
                                          std::uintptr_t context,
                                          std::uintptr_t auxiliary) noexcept;
using BehaviorStep = void(__fastcall*)(const std::byte* actor,
                                        std::uintptr_t context) noexcept;
using BehaviorNode = void(__fastcall*)(const std::byte* node,
                                        const std::uint32_t* context) noexcept;
using IndexHeapRelease = void(__fastcall*)(void* heap, std::uint32_t handle) noexcept;
using SceneVisualObjectCreate = std::int32_t*(__fastcall*)(std::int32_t* result,
                                                           const std::byte* descriptor,
                                                           std::uint32_t table,
                                                           std::uint32_t record) noexcept;
using VisualEntryCreate = std::uint16_t*(__fastcall*)(std::uint32_t* component,
                                                       std::uint16_t* result,
                                                       std::uint32_t* entryState,
                                                       std::int32_t entryIndex) noexcept;
using VisualTransformApply = std::uint64_t(__fastcall*)(const std::byte* source,
                                                         std::byte* destination) noexcept;
using VisualAttachmentBind = void(__fastcall*)(std::uintptr_t ownerRecord,
                                                std::uint32_t object,
                                                std::uint64_t binding,
                                                const std::byte* attachment) noexcept;
using SceneAuthoredEventDispatch = std::uint64_t(__fastcall*)(std::uint32_t* scene,
                                                              std::int32_t eventIndex,
                                                              float elapsed,
                                                              float* consumed) noexcept;
using SceneType23Callback = void(__fastcall*)(std::uint32_t* context) noexcept;
using SceneType23TransformResolve = void(__fastcall*)(std::uint32_t* context,
                                                       std::uint32_t* seed,
                                                       std::uint32_t selector,
                                                       std::uint32_t mode,
                                                       void* transformOutput,
                                                       void* auxiliaryOutput) noexcept;
using SceneTransformSourceUpdate = void(__fastcall*)(std::byte* bank,
                                                       std::uint32_t* scene,
                                                       const std::byte* descriptor,
                                                       const std::byte* runtime) noexcept;
using SceneTransformKind2Resolve = std::int32_t(__fastcall*)(
    const std::byte* runtime,
    std::uint32_t outputCapacity,
    void* transformOutput,
    std::uintptr_t resolverContext,
    std::uint8_t mode) noexcept;
using SceneTransformKind2ProviderResolve = std::int32_t(__fastcall*)(
    const std::byte* provider,
    std::uint32_t outputCapacity,
    void* transformOutput,
    std::uintptr_t resolverContext,
    const std::uint32_t* selection,
    std::uint8_t mode) noexcept;
using SceneTransformPoseCandidateResolve = std::uint64_t(__fastcall*)(
    const std::byte* provider,
    std::uint32_t candidateIndex,
    std::uintptr_t resolverContext,
    std::uint8_t flags,
    const void* filterContext,
    const std::byte* definitionBase,
    const std::uintptr_t* poseInterface,
    char allowFallback,
    std::byte* output) noexcept;
using PoseSocketDispatch = void(__fastcall*)(const std::uintptr_t* poseInterface,
                                              std::uint32_t bindingKey,
                                              std::uint8_t flags,
                                              std::byte* output) noexcept;
using EffectTransformCompose = void(__fastcall*)(
    std::byte* effect,
    const std::uintptr_t* transformArrayBase,
    const std::uintptr_t* selectorArrayBase,
    std::uintptr_t extraTranslationBase) noexcept;
using RendererBulkBoolean = void(__fastcall*)(std::uintptr_t* objects,
                                               std::int32_t count,
                                               char enabled) noexcept;
using RendererBulkSibling = void(__fastcall*)(std::uintptr_t* objects,
                                               std::int32_t count) noexcept;
using RendererObject = void(__fastcall*)(std::byte* object) noexcept;
using PresentationBodyState = void(__fastcall*)(std::byte* component,
                                                 char enabled) noexcept;
using PositionKey = std::uint32_t(__fastcall*)() noexcept;

enum class HookSlot : std::size_t {
    apply,
    deficit,
    request,
    squadResolve,
    memberBuild,
    queueFind,
    drain,
    factory,
    objectFinalize,
    entityForObject,
    componentStart,
    behaviorStep,
    behaviorNode,
    indexHeapRelease,
    count,
};

constexpr std::size_t kHookCount = static_cast<std::size_t>(HookSlot::count);

enum class RendererHookSlot : std::size_t {
    bulkBoolean,
    bulkSibling,
    objectA,
    objectB,
    objectC,
    count,
};

constexpr std::size_t kRendererHookCount =
    static_cast<std::size_t>(RendererHookSlot::count);

enum class VisualRecorderHookSlot : std::size_t {
    entryCreate,
    transformApply,
    attachmentBind,
    count,
};

constexpr std::size_t kVisualRecorderHookCount =
    static_cast<std::size_t>(VisualRecorderHookSlot::count);

std::array<hooking::detour::Handle, kHookCount> g_handles{};
std::atomic<SpawnerApply> g_applyOriginal{nullptr};
std::atomic<SpawnerDeficit> g_deficitOriginal{nullptr};
std::atomic<SpawnerRequest> g_requestOriginal{nullptr};
std::atomic<SpawnerSquadResolve> g_squadResolveOriginal{nullptr};
std::atomic<SpawnerMemberBuild> g_memberBuildOriginal{nullptr};
std::atomic<SpawnerQueueFind> g_queueFindOriginal{nullptr};
std::atomic<SpawnerDrain> g_drainOriginal{nullptr};
std::atomic<EntityFactory> g_factoryOriginal{nullptr};
hooking::detour::Handle g_sceneActorSchedulerHandle{};
std::atomic<SceneActorScheduler> g_sceneActorSchedulerOriginal{nullptr};
hooking::detour::Handle g_sceneEntryUpdateHandle{};
std::atomic<SceneEntryUpdate> g_sceneEntryUpdateOriginal{nullptr};
hooking::detour::Handle g_sceneEntryResolverHandle{};
std::atomic<SceneEntryResolver> g_sceneEntryResolverOriginal{nullptr};
hooking::detour::Handle g_sceneTransitionRetireHandle{};
std::atomic<SceneTransitionRetire> g_sceneTransitionRetireOriginal{nullptr};
std::atomic<ObjectFinalize> g_objectFinalizeOriginal{nullptr};
std::atomic<EntityForObject> g_entityForObjectOriginal{nullptr};
std::atomic<ComponentStart> g_componentStartOriginal{nullptr};
std::atomic<BehaviorStep> g_behaviorStepOriginal{nullptr};
std::atomic<BehaviorNode> g_behaviorNodeOriginal{nullptr};
std::atomic<IndexHeapRelease> g_indexHeapReleaseOriginal{nullptr};
hooking::detour::Handle g_sceneVisualObjectCreateHandle{};
std::atomic<SceneVisualObjectCreate> g_sceneVisualObjectCreateOriginal{nullptr};
std::array<hooking::detour::Handle, kVisualRecorderHookCount>
    g_visualRecorderHandles{};
std::atomic<VisualEntryCreate> g_visualEntryCreateOriginal{nullptr};
std::atomic<VisualTransformApply> g_visualTransformApplyOriginal{nullptr};
std::atomic<VisualAttachmentBind> g_visualAttachmentBindOriginal{nullptr};
hooking::detour::Handle g_sceneAuthoredEventDispatchHandle{};
std::atomic<SceneAuthoredEventDispatch> g_sceneAuthoredEventDispatchOriginal{nullptr};
hooking::detour::Handle g_sceneType23CallbackHandle{};
std::atomic<SceneType23Callback> g_sceneType23CallbackOriginal{nullptr};
hooking::detour::Handle g_sceneType23TransformResolveHandle{};
std::atomic<SceneType23TransformResolve> g_sceneType23TransformResolveOriginal{nullptr};
hooking::detour::Handle g_sceneTransformSourceUpdateHandle{};
std::atomic<SceneTransformSourceUpdate> g_sceneTransformSourceUpdateOriginal{nullptr};
hooking::detour::Handle g_sceneTransformKind2ResolveHandle{};
std::atomic<SceneTransformKind2Resolve> g_sceneTransformKind2ResolveOriginal{nullptr};
hooking::detour::Handle g_sceneTransformKind2ProviderResolveHandle{};
std::atomic<SceneTransformKind2ProviderResolve>
    g_sceneTransformKind2ProviderResolveOriginal{nullptr};
hooking::detour::Handle g_sceneTransformPoseCandidateResolveHandle{};
std::atomic<SceneTransformPoseCandidateResolve>
    g_sceneTransformPoseCandidateResolveOriginal{nullptr};
hooking::detour::Handle g_poseSocketDispatchHandle{};
std::atomic<PoseSocketDispatch> g_poseSocketDispatchOriginal{nullptr};
hooking::detour::Handle g_effectTransformComposeHandle{};
std::atomic<EffectTransformCompose> g_effectTransformComposeOriginal{nullptr};
std::atomic_uint32_t g_sceneTransformSourceUpdateActiveCalls{};
std::atomic_uint32_t g_sceneTransformKind2ResolveActiveCalls{};
std::atomic_uint32_t g_sceneTransformKind2ProviderResolveActiveCalls{};
std::atomic_uint32_t g_sceneTransformPoseCandidateResolveActiveCalls{};
std::atomic_uint32_t g_poseSocketDispatchActiveCalls{};
std::atomic_uint32_t g_effectTransformComposeActiveCalls{};
std::array<hooking::detour::Handle, kRendererHookCount> g_rendererHandles{};
std::atomic<RendererBulkBoolean> g_rendererBulkBooleanOriginal{nullptr};
std::atomic<RendererBulkSibling> g_rendererBulkSiblingOriginal{nullptr};
std::atomic<RendererObject> g_rendererObjectAOriginal{nullptr};
std::atomic<RendererObject> g_rendererObjectBOriginal{nullptr};
std::atomic<RendererObject> g_rendererObjectCOriginal{nullptr};
hooking::detour::Handle g_presentationBodyStateHandle{};
std::atomic<PresentationBodyState> g_presentationBodyStateOriginal{nullptr};
std::atomic<std::byte*> g_image{nullptr};
std::atomic_uint32_t g_applyCalls{};
std::atomic_uint32_t g_chainCalls{};
std::atomic_uint32_t g_factoryCalls{};
std::atomic_uint32_t g_drainCalls{};
std::atomic_uint32_t g_behaviorCalls{};
std::atomic_uint32_t g_behaviorMatches{};
std::atomic_uint32_t g_behaviorNodeCalls{};
std::atomic_uint32_t g_sceneVisualCreateCalls{};
std::atomic_uint32_t g_sceneEntryResolverCalls{};
std::atomic_uint32_t g_visualEntryRecorderCalls{};
std::atomic_uint32_t g_sceneAuthoredEventCalls{};
std::atomic_uint32_t g_sceneAuthoredEventSamples{};
std::atomic_uint32_t g_sceneAuthoredEventAttempts{};
std::atomic_uint32_t g_sceneType23Calls{};
std::atomic_uint32_t g_sceneType23Samples{};
std::atomic_uint32_t g_sceneTransformWriterCalls{};
std::atomic_uint32_t g_sceneTransformWriterSamples{};
std::atomic_uint32_t g_sceneTransformKind2ResolveCalls{};
std::atomic_uint32_t g_sceneTransformKind2Samples{};
std::atomic_uint32_t g_sceneTransformKind2ProviderTreatmentCalls{};
std::atomic_uint32_t g_sceneTransformKind2ProviderTreatmentApplied{};
std::atomic_uint32_t g_sceneTransformKind2ProviderTreatmentFallbacks{};
std::atomic_uint32_t g_rendererHandleSamples{};
std::atomic_uint64_t g_sceneTransformWriterLastTick{};
std::atomic_uint64_t g_sceneTransformKind2LastTick{};
std::atomic_uintptr_t g_sceneTransformKind2LastProvider{};
std::atomic_uint32_t g_sceneTransformKind2LastCandidate{kInvalidHandle};
std::atomic_int32_t g_sceneTransformKind2LastResult{INT32_MIN};
std::atomic_uint32_t g_scenePoseBindingKey{kInvalidHandle};
std::atomic_uint32_t g_scenePoseProviderTargetHandle{kInvalidHandle};
std::atomic_uintptr_t g_scenePoseProviderTargetRecord{};
std::atomic_int64_t g_scenePoseProviderTargetRelative{INT64_MIN};
std::atomic_uintptr_t g_scenePoseProviderTarget{};
std::atomic_uint32_t g_scenePoseProviderTargetObservedMask{};
std::atomic_uint32_t g_scenePoseProviderTargetResolvedMask{};
std::array<std::atomic_uint32_t, 2U> g_scenePoseNaturalCalls{};
std::array<std::atomic_uint32_t, 2U> g_scenePoseNaturalSamples{};
std::array<std::atomic_uint64_t, 2U> g_scenePoseNaturalLastTick{};
std::atomic_uint32_t g_poseSocketDispatchCalls{};
std::atomic_uint32_t g_poseSocketDispatchSamples{};
std::atomic_uint64_t g_poseSocketDispatchLastTick{};
std::atomic_uintptr_t g_poseSocketVfxDispatchBase{};
std::atomic_uintptr_t g_poseSocketVfxDispatchTarget{};
std::atomic_uintptr_t g_poseSocketVfxObject{};
std::atomic_uint64_t g_poseSocketVfxLastCallTick{};
std::atomic_uint32_t g_effectTransformComposeCalls{};
std::atomic_uint32_t g_effectTransformComposeSamples{};
std::atomic_uint32_t g_omegaPurpleDirectEffectOffsetCalls{};
std::atomic_uint32_t g_omegaPurpleDirectEffectOffsetApplied{};
std::atomic_uint32_t g_omegaPurpleDirectEffectOffsetSeenMask{};
std::atomic_uint64_t g_omegaPurpleDirectEffectOffsetLastTick{};
std::atomic_uint32_t g_poseProviderProvenanceCalls{};
std::atomic_uint32_t g_poseProviderProvenanceSamples{};
std::atomic_uint32_t g_omegaPurplePoseRebindCalls{};
std::atomic_uint32_t g_omegaPurplePoseRebindApplied{};
std::atomic_uint32_t g_omegaPurplePoseRebindFallbacks{};
std::atomic_uint64_t g_omegaPurplePoseRebindLastTick{};
std::atomic_uint32_t g_sceneActivationObservedMask{};
std::atomic_uint32_t g_sceneCastSuppressionObservedMask{};
std::atomic_uint32_t g_sceneTwoPersistenceRetireCalls{};
std::array<std::atomic_uintptr_t, 3U> g_omegaSceneSchedulerPointers{};
std::array<std::atomic_uintptr_t, 3U> g_omegaSceneEventPointers{};
std::atomic_uintptr_t g_sceneTwoPresentationComponent{};
std::atomic_uint32_t g_sceneTwoPresentationObject{kInvalidHandle};
std::atomic_uint32_t g_sceneTwoPresentationCaptures{};
std::atomic_uint32_t g_sceneTwoPresentationDisableCalls{};
std::atomic_uint32_t g_sceneTwoPresentationDisabledObjects{};
std::atomic_uint32_t g_sceneTwoBodyStateCalls{};
std::atomic_uint32_t g_sceneTwoBodyStateOverrides{};
std::atomic_uint32_t g_sceneTwoBodyStateForces{};
std::atomic_uint32_t g_sceneTwoModelSuppressions{};
std::array<std::atomic_uintptr_t, kMaximumSceneTwoPresentationRendererObjects>
    g_sceneTwoPresentationRenderers{};
std::atomic_uint64_t g_followupLastTick{};
std::atomic_uint64_t g_handoffRollingBeforeLastTick{};
std::atomic_uint64_t g_handoffRollingAfterLastTick{};
std::atomic_uint32_t g_handoffRollingPasses{};
std::atomic_uintptr_t g_omegaQueue{};
std::atomic_uint32_t g_ikoraObject{kInvalidHandle};
std::atomic_uint32_t g_ikoraEntity{kInvalidHandle};
std::atomic_uintptr_t g_ikoraNetworkRecord{};
std::atomic_uintptr_t g_ikoraBehaviorActor{};
std::atomic_uintptr_t g_ikoraHandoffBehaviorActor{};
std::atomic_uint32_t g_sceneAnimatedObject{kInvalidHandle};
// The native object-index heap callback is allocator teardown code. It may identify the one
// cinematic release edge, but it must not publish host state or remain detoured for the later
// persistent-actor teardown. A normal behavior tick consumes these flags outside the allocator.
std::atomic_bool g_sceneCompletionPending{};
std::atomic_bool g_indexHeapReleaseDetachPending{};
std::atomic_flag g_indexHeapReleaseDetachBusy = ATOMIC_FLAG_INIT;
std::atomic_bool g_installed{};

struct VisualEntryRecorderContext final {
    std::uint32_t sequence{};
    std::uint32_t* component{};
    std::uint32_t* entryState{};
    std::int32_t entryIndex{-1};
    std::uintptr_t callerRva{};
    std::uint32_t createdObject{kInvalidHandle};
    std::uint32_t createdDefinition{kInvalidHandle};
    std::uint32_t objectCreateCalls{};
    std::uint32_t transformApplyCalls{};
    std::uint32_t attachmentBindCalls{};
    std::array<std::byte, 0x20U> createdTransform{};
    bool createdTransformReadable{};
    bool report{};
};

// The nested sites are shared. Only report them while +4E4580 is processing Omega's payload.
thread_local std::uint32_t g_omegaChainDepth{};
thread_local std::uint32_t g_omegaChainSequence{};
thread_local std::uint32_t g_ikoraFactoryDepth{};
thread_local std::uint32_t g_ikoraFactorySequence{};
thread_local std::uint32_t g_ikoraFactoryObject{kInvalidHandle};
thread_local std::uintptr_t g_ikoraFactoryCallerRva{};
thread_local std::uint32_t* g_sceneActorSchedulerScene{};
thread_local std::uint32_t g_sceneActorSchedulerDepth{};
thread_local std::uint32_t g_sceneActorSchedulerSpawnMask{};
thread_local std::byte* g_sceneEntryUpdateComponent{};
thread_local std::uint32_t g_sceneEntryUpdateIndex{};
thread_local std::uint32_t g_ikoraBehaviorDepth{};
thread_local std::uint32_t g_sceneVisualCreateDepth{};
thread_local std::uint32_t g_visualEntryRecorderDepth{};
thread_local VisualEntryRecorderContext g_visualEntryRecorderContext{};
thread_local std::uint32_t g_sceneAuthoredEventDepth{};
thread_local std::uint32_t* g_sceneAuthoredEventScene{};
thread_local std::int32_t g_sceneAuthoredEventIndex{-1};
thread_local std::uint32_t g_sceneAuthoredEventHandle{kInvalidHandle};
thread_local const std::byte* g_sceneAuthoredEventRuntime{};
thread_local const std::byte* g_sceneAuthoredEventAuthored{};
thread_local std::uint32_t g_sceneType23Depth{};
thread_local std::uint32_t g_poseSocketExactVfxDepth{};
thread_local std::uint32_t g_rendererHandleDepth{};
thread_local std::uintptr_t g_componentStartInstance{};
thread_local std::uint32_t g_componentStartDefinition{kInvalidHandle};
thread_local std::uintptr_t g_componentStartHandlerRva{};
thread_local std::uint32_t g_componentStartFactorySequence{};
thread_local std::uint32_t g_componentStartObject{kInvalidHandle};

struct WatchedComponent final {
    std::uint32_t definition{};
    const char* name{};
};

constexpr std::array<WatchedComponent, 7> kWatchedComponents{{
    {kSquadIkoraDefinition, "squad_ikora"},
    {kSceneIkoraDefinition, "scene_ikora_opens_portal"},
    {kGateControllerDefinition, "d_gate_controller"},
    {kEngagementSensorDefinition, "m_engagement_sensor"},
    {kVignettePointDefinition, "pt_start_ikora_vignette"},
    {kDialoguePointDefinition, "pt_start_ikora_outer_dialogue"},
    {kTriggerVolumeDefinition, "ikora_trigger_volumes"},
}};

std::array<std::atomic_uintptr_t, kWatchedComponents.size()> g_watchedComponentPointers{};
std::array<std::atomic_uint64_t, kWatchedComponents.size()> g_watchedComponentHashes{};

struct IkoraComponent final {
    std::uintptr_t instance{};
    std::uintptr_t handlerRva{};
    std::uint32_t definition{};
    std::uint32_t factorySequence{};
    std::uint32_t object{kInvalidHandle};
    std::size_t size{};
    std::uint64_t hash{};
};

struct IkoraProviderRecordMatch final {
    std::uintptr_t provider{};
    std::size_t offset{};
    std::uint32_t targetHandle{kInvalidHandle};
    std::uintptr_t targetRecord{};
    std::int64_t targetRelative{INT64_MIN};
    std::uintptr_t target{};
};

struct IkoraActor final {
    std::uint32_t factorySequence{};
    std::uint32_t object{kInvalidHandle};
    std::uint32_t entity{kInvalidHandle};
    std::uintptr_t factoryCallerRva{};
    std::uintptr_t descriptor{};
    std::uintptr_t networkRecord{};
    std::uintptr_t behaviorActor{};
    std::uint64_t createdAtMs{};
    std::uint64_t lastBehaviorAtMs{};
    std::uint64_t releasedAtMs{};
    std::uint64_t behaviorUpdates{};
    bool released{};
};

struct IkoraComponentAddressOwner final {
    std::uint32_t factorySequence{};
    std::uint32_t object{kInvalidHandle};
    std::uint32_t definition{kInvalidHandle};
    std::uintptr_t instance{};
    std::size_t size{};
    std::size_t offset{};
    bool exactInstance{};
    bool found{};
};

/** One exact +A1E8A0 provider-row lookup made beneath the armed kind-2 source resolver. */
struct ScenePoseCandidateCapture final {
    bool called{};
    bool rowReadable{};
    bool outputBeforeReadable{};
    bool outputAfterReadable{};
    std::uint32_t candidateIndex{};
    std::uint32_t resolverStride{};
    std::uint32_t rowWord0{kInvalidHandle};
    std::uint32_t bindingKey{kInvalidHandle};
    std::uint32_t selectionKey{kInvalidHandle};
    std::uint8_t flags{};
    char allowFallback{};
    std::uint64_t result{};
    std::uintptr_t provider{};
    std::uintptr_t resolverContext{};
    std::uintptr_t filterContext{};
    std::uintptr_t definitionBase{};
    std::uintptr_t table{};
    std::uintptr_t row{};
    std::uintptr_t poseInterface{};
    std::uintptr_t poseDispatchBase{};
    std::uintptr_t poseObject{};
    std::uintptr_t output{};
    IkoraComponentAddressOwner providerOwner{};
    IkoraComponentAddressOwner filterOwner{};
    IkoraComponentAddressOwner poseDispatchOwner{};
    IkoraComponentAddressOwner poseObjectOwner{};
    std::array<std::byte, 0x40U> rowBytes{};
    std::array<std::byte, 0x50U> outputBefore{};
    std::array<std::byte, 0x50U> outputAfter{};
};

/** One thread-local observation of +A1F360 while the exact Omega entry-1 writer is active. */
struct SceneTransformKind2ResolveCapture final {
    bool armed{};
    bool called{};
    bool outputBeforeReadable{};
    bool outputAfterReadable{};
    bool providerReadable{};
    bool providerTargetReadable{};
    bool providerTreatmentCalled{};
    bool providerTreatmentEligible{};
    bool providerTreatmentApplied{};
    bool providerTreatmentFallback{};
    bool providerTreatmentInputReadable{};
    bool providerTreatmentOutputReadable{};
    bool providerTreatmentOutputCommitted{};
    bool sceneOneProviderReadable{};
    bool sceneOneDefinitionReadable{};
    std::uint32_t writerCall{};
    std::uint32_t resolverCall{};
    std::uint32_t outputCapacity{};
    std::uint32_t providerHandle{kInvalidHandle};
    std::uint32_t providerTargetHandle{kInvalidHandle};
    std::uint32_t runtimeSelector{};
    std::uint32_t resolverStride{};
    std::uint32_t providerSelection{kInvalidHandle};
    std::uint32_t providerTreatmentNativeSelection{kInvalidHandle};
    std::uint32_t sceneOneDispatchHandle{kInvalidHandle};
    std::uint32_t sceneOnePoseHandle{kInvalidHandle};
    std::uint32_t sceneOneRowBinding{kInvalidHandle};
    std::uint32_t sceneOneRowSelection{kInvalidHandle};
    std::uint32_t poseCandidateCount{};
    std::int32_t result{INT32_MIN};
    std::int32_t providerTreatmentResult{INT32_MIN};
    std::int32_t providerTreatmentFallbackResult{INT32_MIN};
    std::uint8_t mode{};
    const std::byte* expectedRuntime{};
    const std::byte* runtime{};
    void* transformOutput{};
    std::uintptr_t resolverContext{};
    std::uintptr_t providerRecord{};
    std::int64_t providerRelative{INT64_MIN};
    std::uintptr_t provider{};
    std::uintptr_t providerTargetRecord{};
    std::int64_t providerTargetRelative{INT64_MIN};
    std::uintptr_t providerTarget{};
    std::uintptr_t sceneOneProviderComponent{};
    std::size_t sceneOneProviderComponentSize{};
    std::size_t sceneOneProviderOffset{};
    std::uintptr_t sceneOneProvider{};
    std::uintptr_t sceneOneDefinitionBase{};
    std::uintptr_t sceneOneDefinitionTable{};
    std::uintptr_t sceneOneDefinitionRow{};
    std::uintptr_t sceneOneDispatchRecord{};
    std::uintptr_t sceneOnePoseRecord{};
    std::int64_t sceneOnePoseRelative{INT64_MIN};
    std::uintptr_t sceneOnePose{};
    std::uint64_t sceneOnePoseIdentity{};
    std::uint64_t sceneOnePoseSize{};
    std::uintptr_t sceneOneFilterContext{};
    std::uint64_t sceneOneCandidateResult{};
    std::uintptr_t providerTableAnchor{};
    std::int64_t providerTableRelative{INT64_MIN};
    std::uintptr_t providerTable{};
    std::uintptr_t providerSelectionAddress{};
    IkoraComponentAddressOwner providerOwner{};
    IkoraComponentAddressOwner providerTargetOwner{};
    IkoraComponentAddressOwner providerTableOwner{};
    IkoraComponentAddressOwner providerSelectionOwner{};
    const char* providerTreatmentReason{"not_called"};
    std::array<std::byte, 0x50U> outputBefore{};
    std::array<std::byte, 0x50U> outputAfter{};
    std::array<std::byte, 0x50U> providerTreatmentOutputBefore{};
    std::array<std::byte, 0x50U> providerTreatmentOutputAfter{};
    std::array<std::byte, 0x80U> providerBytes{};
    std::array<std::byte, 0x80U> providerTargetBytes{};
    std::array<std::byte, kIkoraTransformProviderProbeBytes> sceneOneProviderBytes{};
    std::array<ScenePoseCandidateCapture, kMaximumScenePoseCandidatesPerResolve>
        poseCandidates{};
};

struct RendererActorCorrelation final {
    std::array<std::uint32_t, 3U> objects{
        kInvalidHandle, kInvalidHandle, kInvalidHandle};
    std::array<std::uintptr_t, 3U> presentationComponents{};
    std::array<std::uintptr_t, 3U> behaviorActors{};
    std::array<std::int32_t, 3U> objectOffsets{-1, -1, -1};
    std::array<std::int32_t, 3U> presentationOffsets{-1, -1, -1};
    std::uint32_t releasedMask{};
    std::uint32_t objectReferenceMask{};
    std::uint32_t presentationReferenceMask{};
    std::uint32_t behaviorReferenceMask{};
    std::uint32_t directActorMask{};
    std::uint32_t directPresentationMask{};
    bool readable{};
};

enum class PresentationFingerprintSource : std::uint8_t {
    component,
    componentField,
    renderEntry,
    renderObjectField,
    array1B0,
    array1C0,
    array1D0,
    array1E0,
};

struct PresentationFingerprintPointer final {
    std::uintptr_t value{};
    std::uintptr_t component{};
    std::uint32_t actor{};
    std::uint32_t sourceOffset{};
    PresentationFingerprintSource source{};
};

struct PresentationFingerprintSet final {
    std::array<PresentationFingerprintPointer,
               kMaximumPresentationFingerprintPointers> pointers{};
    std::array<std::uint32_t, 3U> actorPointerCounts{};
    std::array<std::uintptr_t, 3U> components{};
    std::size_t count{};
    std::uint32_t readableActorMask{};
    bool saturated{};
};

struct AttachmentPointerCorrelation final {
    std::array<std::uint32_t, 3U> counts{};
    std::array<std::uint32_t, 3U> rendererCounts{};
    std::array<std::uintptr_t, 3U> firstValues{};
    std::array<std::uintptr_t, 3U> firstComponents{};
    std::array<std::uint32_t, 3U> firstRegionOffsets{};
    std::array<std::uint32_t, 3U> firstFingerprintOffsets{};
    std::array<PresentationFingerprintSource, 3U> firstFingerprintSources{};
    std::array<const char*, 3U> firstRegions{};
    std::uint32_t actorMask{};
    std::uint32_t rendererActorMask{};
};

struct AttachmentHandleCorrelation final {
    std::array<std::uint32_t, 3U> counts{};
    std::array<std::uintptr_t, 3U> firstComponents{};
    std::array<std::uint32_t, 3U> firstDefinitions{};
    std::array<std::uint32_t, 3U> firstOffsets{};
    std::uint32_t actorMask{};
};

/** Exact references to one pose object across captured component and presentation graphs. */
struct PosePointerCorrelation final {
    std::array<std::uint32_t, 3U> componentCounts{};
    std::array<std::uint32_t, 3U> presentationCounts{};
    std::array<std::uint32_t, 3U> rendererCounts{};
    std::array<std::uintptr_t, 3U> firstComponents{};
    std::array<std::uint32_t, 3U> firstDefinitions{};
    std::array<std::uint32_t, 3U> firstComponentOffsets{};
    std::array<std::uintptr_t, 3U> firstPresentationComponents{};
    std::array<std::uint32_t, 3U> firstFingerprintOffsets{};
    std::array<PresentationFingerprintSource, 3U> firstFingerprintSources{};
    std::uint32_t componentActorMask{};
    std::uint32_t presentationActorMask{};
    std::uint32_t rendererActorMask{};
};

/** De-duplicates high-frequency socket calls while preserving each distinct native pose path. */
struct PoseSocketTraceKey final {
    std::uintptr_t callerRva{};
    std::uintptr_t dispatchBase{};
    std::uintptr_t poseObject{};
    std::uint32_t bindingKey{kInvalidHandle};
    bool exactVfx{};
};

/** One concrete effect/caller/definition tuple observed after the exact purple socket resolves. */
struct EffectTransformTraceKey final {
    std::uintptr_t effect{};
    std::uintptr_t callerRva{};
    std::uint32_t definition{kInvalidHandle};
    std::uint32_t resource{kInvalidHandle};
    std::uint64_t lastSampleAtMs{};
};

/** Latest exact purple socket transform, copied under a lock for the render-thread comparison. */
struct VfxSocketTransformSnapshot final {
    std::array<float, 4U> rotation{};
    std::array<float, 3U> position{};
    float scale{};
    std::uint64_t capturedAtMs{};
    bool valid{};
};

/** De-duplicates the provider/interface provenance for each concrete 0x15 pose path. */
struct PoseProviderProvenanceTraceKey final {
    std::uintptr_t provider{};
    std::uintptr_t poseObject{};
    std::uintptr_t definitionBase{};
    std::uint32_t candidateIndex{};
    std::uint32_t bindingKey{kInvalidHandle};
    std::uint8_t flags{};
    bool exactVfx{};
};

/** Stable scene-1 provider route learned from its native 0x8161FB60 candidate-8 lookup. */
struct SceneOnePoseBindingCache final {
    bool valid{};
    std::uint32_t actor{kInvalidHandle};
    std::uintptr_t provider{};
    std::uintptr_t providerComponent{};
    std::size_t providerOffset{};
    std::uintptr_t definitionBase{};
    std::uintptr_t dispatchBase{};
    std::uintptr_t poseObject{};
    std::uintptr_t sceneScheduler{};
    std::int64_t sceneSchedulerDelta{INT64_MIN};
    std::uint64_t poseIdentity{};
    std::uint64_t poseSize{};
    std::uint64_t capturedAtMs{};
};

struct RendererTraceKey final {
    std::uintptr_t wrapperRva{};
    std::uintptr_t callerRva{};
    std::uintptr_t object{};
    std::int32_t enabled{};
    std::uint32_t releasedMask{};
    bool after{};
};

struct IkoraOwnershipEvidence final {
    std::size_t componentCount{};
    std::size_t liveComponentCount{};
    std::size_t scannedBytes{};
    std::uint32_t effect80B9FDBERefs{};
    std::uint32_t effect80C220EBRefs{};
    std::uint32_t selfObjectRefs{};
    std::array<std::uint32_t, 3U> actorObjectRefs{};
    std::uint32_t behaviorActorRefs{};
    std::uintptr_t firstEffect80B9FDBEComponent{};
    std::uintptr_t firstEffect80C220EBComponent{};
    std::size_t firstEffect80B9FDBEOffset{static_cast<std::size_t>(-1)};
    std::size_t firstEffect80C220EBOffset{static_cast<std::size_t>(-1)};
};

struct HandoffComponentSnapshot final {
    std::uintptr_t instance{};
    std::uintptr_t handlerRva{};
    std::uint32_t definition{};
    std::uint32_t factorySequence{};
    std::uint32_t object{kInvalidHandle};
    std::uint64_t lastHash{};
    std::uint64_t previousHash{};
    std::uint64_t lastObservedAtMs{};
    std::uint64_t lastChangedAtMs{};
    std::uint64_t previousChangedAtMs{};
    std::uint32_t captures{};
    std::uint32_t changes{};
    std::uint32_t lastPhase{};
    std::uint32_t previousPhase{};
    bool valid{};
    bool hasPrevious{};
    std::array<std::byte, kHandoffSnapshotBytes> last{};
    std::array<std::byte, kHandoffSnapshotBytes> previous{};
};

// The authored scene dispatcher can run the same row thousands of times per second. Retain one
// small state record per row so the recorder spans the complete scene and emits only first sight,
// actor/visual ownership changes, and a one-second heartbeat. Full object handles contain a
// generation field which changes when short-lived visual objects are recycled, so visual identity
// is compared by its stable 13-bit slot as well as the native owner written at object+0x20.
struct SceneEventTraceState final {
    std::uintptr_t scene{};
    std::uintptr_t authored{};
    std::int32_t eventIndex{-1};
    std::uint8_t type{0xFFU};
    std::uint32_t actor1{kInvalidHandle};
    std::uint32_t actor2{kInvalidHandle};
    std::uint32_t actor3{kInvalidHandle};
    std::uint32_t createdSlot{kInvalidHandle};
    std::uint32_t createdOwner{kInvalidHandle};
    std::uint32_t createdDefinition{};
    std::uint64_t lastLoggedAtMs{};
    std::uint32_t calls{};
    bool valid{};
};

struct SceneType23TraceState final {
    std::uintptr_t contextData{};
    std::uintptr_t authored{};
    std::int32_t eventIndex{-1};
    std::uint32_t actor1{kInvalidHandle};
    std::uint32_t actor2{kInvalidHandle};
    std::uint32_t actor3{kInvalidHandle};
    std::uint32_t contextOwner{kInvalidHandle};
    std::uint32_t contextObject{kInvalidHandle};
    std::uint32_t createdSlot{kInvalidHandle};
    std::uint32_t createdOwner{kInvalidHandle};
    std::uint64_t lastLoggedAtMs{};
    std::uint32_t calls{};
    bool valid{};
};

struct SceneType23TransformCapture final {
    bool called{};
    bool transformReadable{};
    bool auxiliaryReadable{};
    bool bankReadable{};
    bool programReadable{};
    bool selectorRowReadable{};
    bool descriptorReadable{};
    bool bankTransformsReadable{};
    bool bankMetadataReadable{};
    bool bankSecondaryReadable{};
    std::uint32_t selector{};
    std::uint32_t mode{};
    std::uint32_t seedBefore{};
    std::uint32_t seedAfter{};
    std::uint32_t sceneHandle{kInvalidHandle};
    std::uint32_t contextObject{kInvalidHandle};
    std::uint64_t contextOffset{};
    std::int64_t selectorRelative{INT64_MIN};
    std::int64_t descriptorRelative{INT64_MIN};
    std::uint64_t bankTransformCount{};
    std::int64_t bankTransformRelative{INT64_MIN};
    std::uint64_t bankMetadataCount{};
    std::int64_t bankMetadataRelative{INT64_MIN};
    std::uint64_t bankSecondaryCount{};
    std::int64_t bankSecondaryRelative{INT64_MIN};
    std::uint8_t selectorSourceStart{};
    std::uint8_t selectorSourceCount{};
    std::uint8_t selectorDescriptorStart{};
    std::uint8_t selectorDescriptorCount{};
    std::uint8_t selectorFlags{};
    bool selectorUsesDirectBank{};
    const void* contextAddress{};
    const void* sceneRecordAddress{};
    const void* objectArgumentAddress{};
    const void* bankAddress{};
    const void* programAddress{};
    const void* selectorRowAddress{};
    const void* descriptorAddress{};
    const void* bankTransformsAddress{};
    const void* bankMetadataAddress{};
    const void* bankSecondaryAddress{};
    const void* transformAddress{};
    const void* auxiliaryAddress{};
    std::array<std::byte, 0x80U> bank{};
    std::array<std::byte, 0x40U> program{};
    std::array<std::byte, 0x20U> selectorRow{};
    std::array<std::byte, 0x100U> descriptors{};
    // +589670's direct-bank branch copies one 0x20-byte transform from the +0x38 vector and
    // one parallel uint16 from the +0x48 vector. The transform vector starts with the native
    // 16-byte {count, 0x80809F75} header. The portal scene has three entries while Ikora's
    // opening scene has six, so retain the complete larger vector.
    std::array<std::byte,
               0x10U + kMaximumCapturedSceneTransforms * 0x20U> bankTransforms{};
    std::array<std::byte, 0x20U> bankMetadata{};
    std::array<std::byte, 0x40U> bankSecondary{};
    std::array<std::byte, 0x40U> transform{};
    std::array<std::byte, 0x20U> auxiliary{};
};

SRWLOCK g_ikoraComponentLock = SRWLOCK_INIT;
std::array<IkoraComponent, kMaximumIkoraComponents> g_ikoraComponents{};
std::size_t g_ikoraComponentCount{};
SRWLOCK g_ikoraActorLock = SRWLOCK_INIT;
std::array<IkoraActor, kMaximumIkoraActors> g_ikoraActors{};
std::size_t g_ikoraActorCount{};
SRWLOCK g_rendererTraceLock = SRWLOCK_INIT;
std::array<RendererTraceKey, kMaximumRendererHandleSamples> g_rendererTraceKeys{};
std::size_t g_rendererTraceKeyCount{};
SRWLOCK g_poseSocketTraceLock = SRWLOCK_INIT;
std::array<PoseSocketTraceKey, kMaximumPoseSocketTraceKeys> g_poseSocketTraceKeys{};
std::size_t g_poseSocketTraceKeyCount{};
SRWLOCK g_effectTransformTraceLock = SRWLOCK_INIT;
std::array<EffectTransformTraceKey, kMaximumEffectTransformTraceKeys>
    g_effectTransformTraceKeys{};
std::size_t g_effectTransformTraceKeyCount{};
SRWLOCK g_vfxSocketTransformLock = SRWLOCK_INIT;
VfxSocketTransformSnapshot g_vfxSocketTransform{};
SRWLOCK g_poseProviderProvenanceTraceLock = SRWLOCK_INIT;
std::array<PoseProviderProvenanceTraceKey, kMaximumPoseProviderProvenanceTraceKeys>
    g_poseProviderProvenanceTraceKeys{};
std::size_t g_poseProviderProvenanceTraceKeyCount{};
SRWLOCK g_sceneOnePoseBindingLock = SRWLOCK_INIT;
SceneOnePoseBindingCache g_sceneOnePoseBinding{};
SRWLOCK g_handoffSnapshotLock = SRWLOCK_INIT;
std::array<HandoffComponentSnapshot, kMaximumHandoffComponents>
    g_handoffSnapshots{};
std::size_t g_handoffSnapshotCount{};
SRWLOCK g_sceneEventTraceStateLock = SRWLOCK_INIT;
std::array<SceneEventTraceState, kMaximumSceneEventTraceStates>
    g_sceneEventTraceStates{};
std::size_t g_sceneEventTraceStateCount{};
SRWLOCK g_sceneType23TraceStateLock = SRWLOCK_INIT;
std::array<SceneType23TraceState, kMaximumSceneType23TraceStates>
    g_sceneType23TraceStates{};
std::size_t g_sceneType23TraceStateCount{};
thread_local SceneType23TransformCapture g_sceneType23TransformCapture{};
thread_local PresentationFingerprintSet g_presentationFingerprintScratch{};
thread_local SceneTransformKind2ResolveCapture g_sceneTransformKind2ResolveCapture{};

/** Holds a whole replacement call live until every helper and trampoline call has returned. */
class SceneTransformRecorderCallGuard final {
public:
    explicit SceneTransformRecorderCallGuard(std::atomic_uint32_t& activeCalls) noexcept
        : activeCalls_(activeCalls) {
        activeCalls_.fetch_add(1U, std::memory_order_acq_rel);
    }

    ~SceneTransformRecorderCallGuard() noexcept {
        activeCalls_.fetch_sub(1U, std::memory_order_acq_rel);
    }

    SceneTransformRecorderCallGuard(const SceneTransformRecorderCallGuard&) = delete;
    SceneTransformRecorderCallGuard& operator=(const SceneTransformRecorderCallGuard&) = delete;

private:
    std::atomic_uint32_t& activeCalls_;
};

/** @return True only when no source-writer replacement call owns its trampoline. */
[[nodiscard]] bool scene_transform_source_update_idle() noexcept {
    return g_sceneTransformSourceUpdateActiveCalls.load(std::memory_order_acquire) == 0U;
}

/** @return True only when no kind-2 resolver replacement call owns its trampoline. */
[[nodiscard]] bool scene_transform_kind2_resolve_idle() noexcept {
    return g_sceneTransformKind2ResolveActiveCalls.load(std::memory_order_acquire) == 0U;
}

/** @return True only when no provider-treatment replacement call owns its trampoline. */
[[nodiscard]] bool scene_transform_kind2_provider_resolve_idle() noexcept {
    return g_sceneTransformKind2ProviderResolveActiveCalls.load(std::memory_order_acquire) == 0U;
}

/** @return True only when no pose-candidate recorder call owns its trampoline. */
[[nodiscard]] bool scene_transform_pose_candidate_resolve_idle() noexcept {
    return g_sceneTransformPoseCandidateResolveActiveCalls.load(std::memory_order_acquire) == 0U;
}

/** @return True only when no concrete pose/socket dispatch replacement owns its trampoline. */
[[nodiscard]] bool pose_socket_dispatch_idle() noexcept {
    return g_poseSocketDispatchActiveCalls.load(std::memory_order_acquire) == 0U;
}

/** @return True only when no per-effect transform recorder owns its trampoline. */
[[nodiscard]] bool effect_transform_compose_idle() noexcept {
    return g_effectTransformComposeActiveCalls.load(std::memory_order_acquire) == 0U;
}

template <typename Value>
[[nodiscard]] Value safe_read(const void* address, Value fallback = {}) noexcept {
    Value value = fallback;
    __try {
        if (address != nullptr) {
            std::memcpy(&value, address, sizeof value);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        value = fallback;
    }
    return value;
}

[[nodiscard]] bool safe_copy(void* destination,
                             const void* source,
                             std::size_t size) noexcept {
    if (destination == nullptr || source == nullptr || size == 0U) {
        return false;
    }
    __try {
        std::memcpy(destination, source, size);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

/**
 * Verifies that an entire bounded address range is currently committed and readable.
 *
 * Component start handlers repurpose their first runtime word after construction, so that word
 * cannot serve as a permanent definition/liveness tag. Range validation is content-independent;
 * callers separately prove the component's captured definition, owning actor, size, and any
 * provider-specific schema before using it.
 */
[[nodiscard]] bool readable_memory_range(const void* address,
                                         std::size_t size) noexcept {
    const std::uintptr_t begin = reinterpret_cast<std::uintptr_t>(address);
    if (begin == 0U || size == 0U || begin > UINTPTR_MAX - size) {
        return false;
    }

    const std::uintptr_t end = begin + size;
    std::uintptr_t cursor = begin;
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION region{};
        if (VirtualQuery(reinterpret_cast<const void*>(cursor),
                         &region,
                         sizeof region) == 0U
            || region.State != MEM_COMMIT
            || (region.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0U) {
            return false;
        }

        const DWORD access = region.Protect & 0xFFU;
        if (access != PAGE_READONLY && access != PAGE_READWRITE
            && access != PAGE_WRITECOPY && access != PAGE_EXECUTE_READ
            && access != PAGE_EXECUTE_READWRITE
            && access != PAGE_EXECUTE_WRITECOPY) {
            return false;
        }

        const std::uintptr_t regionBegin =
            reinterpret_cast<std::uintptr_t>(region.BaseAddress);
        if (regionBegin > UINTPTR_MAX - region.RegionSize) {
            return false;
        }
        const std::uintptr_t regionEnd = regionBegin + region.RegionSize;
        if (regionEnd <= cursor) {
            return false;
        }
        cursor = std::min(end, regionEnd);
    }
    return true;
}

struct DecodedObjectPosition final {
    std::array<std::uint32_t, 3> raw{};
    std::array<float, 3> value{};
    std::uint32_t currentHandle{kInvalidHandle};
    bool present{};
    bool current{};
    bool valid{};
};

[[nodiscard]] float float_from_bits(std::uint32_t bits) noexcept {
    float value = 0.0F;
    static_assert(sizeof value == sizeof bits);
    std::memcpy(&value, &bits, sizeof value);
    return value;
}

/** Reads the live actor root exactly as the pinned client's +1087F70 object-position path does. */
[[nodiscard]] DecodedObjectPosition decode_object_position(std::uint32_t handle) noexcept {
    DecodedObjectPosition result{};
    std::byte* const image = g_image.load(std::memory_order_acquire);
    if (image == nullptr || handle == kInvalidHandle) {
        return result;
    }
    std::byte* const table = safe_read<std::byte*>(image + kObjectTablePointerRva, nullptr);
    const std::uint32_t stride = safe_read<std::uint32_t>(image + kObjectTableStrideRva, 0U);
    if (table == nullptr || stride == 0U || stride > 0x10000U) {
        return result;
    }
    result.present = true;
    const std::byte* const record =
        table + static_cast<std::size_t>(handle & 0x1FFFU) * stride;
    result.currentHandle = safe_read<std::uint32_t>(record + 0x0CU, kInvalidHandle);
    result.current = result.currentHandle == handle;
    const std::byte* const encoded =
        record + kObjectPositionOffset;
    for (std::size_t lane = 0U; lane < result.raw.size(); ++lane) {
        result.raw[lane] = safe_read<std::uint32_t>(
            encoded + lane * sizeof(std::uint32_t), 0U);
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

    for (std::size_t lane = 0U; lane < result.value.size(); ++lane) {
        const std::uint32_t first = safe_read<std::uint32_t>(
            image + kPositionMaskOneRva + lane * sizeof(std::uint32_t), 0U);
        const std::uint32_t second = safe_read<std::uint32_t>(
            image + kPositionMaskTwoRva + lane * sizeof(std::uint32_t), 0U);
        const std::uint32_t key = lane < 2U ? xy : zw;
        const std::uint32_t bits = ((result.raw[lane] ^ key) & first & second)
                                   | ((~second) & 0x3F800000U);
        result.value[lane] = float_from_bits(bits);
    }
    result.valid = result.current
                   && std::isfinite(result.value[0])
                   && std::isfinite(result.value[1])
                   && std::isfinite(result.value[2])
                   && std::fabs(result.value[0]) < 10'000'000.0F
                   && std::fabs(result.value[1]) < 10'000'000.0F
                   && std::fabs(result.value[2]) < 10'000'000.0F;
    return result;
}

struct DecodedSceneTransform final {
    std::array<float, 4> rotation{};
    std::array<float, 3> position{};
    float scale{};
    bool readable{};
    bool finite{};
};

[[nodiscard]] DecodedSceneTransform decode_scene_bank_transform(
    const SceneType23TransformCapture& capture,
    std::size_t index) noexcept {
    DecodedSceneTransform result{};
    constexpr std::size_t kTransformSize = 0x20U;
    if (!capture.bankTransformsReadable
        || index >= kMaximumCapturedSceneTransforms
        || index >= capture.bankTransformCount) {
        return result;
    }
    constexpr std::size_t kVectorHeaderSize = 0x10U;
    const std::byte* const source = capture.bankTransforms.data()
                                    + kVectorHeaderSize + index * kTransformSize;
    std::memcpy(result.rotation.data(), source, sizeof result.rotation);
    std::memcpy(result.position.data(), source + 0x10U, sizeof result.position);
    std::memcpy(&result.scale, source + 0x1CU, sizeof result.scale);
    result.readable = true;
    result.finite = std::all_of(result.rotation.begin(), result.rotation.end(), [](float value) {
                        return std::isfinite(value);
                    })
                    && std::all_of(result.position.begin(), result.position.end(), [](float value) {
                           return std::isfinite(value);
                       })
                    && std::isfinite(result.scale);
    return result;
}

[[nodiscard]] DecodedSceneTransform decode_scene_transform_address(
    const void* address) noexcept {
    DecodedSceneTransform result{};
    std::array<std::byte, 0x20U> bytes{};
    if (!safe_copy(bytes.data(), address, bytes.size())) {
        return result;
    }
    std::memcpy(result.rotation.data(), bytes.data(), sizeof result.rotation);
    std::memcpy(result.position.data(), bytes.data() + 0x10U, sizeof result.position);
    std::memcpy(&result.scale, bytes.data() + 0x1CU, sizeof result.scale);
    result.readable = true;
    result.finite = std::all_of(result.rotation.begin(), result.rotation.end(), [](float value) {
                        return std::isfinite(value);
                    })
                    && std::all_of(result.position.begin(), result.position.end(), [](float value) {
                           return std::isfinite(value);
                       })
                    && std::isfinite(result.scale);
    return result;
}

struct SceneTransformComponentMatch final {
    std::uint32_t factorySequence{};
    std::uint32_t object{kInvalidHandle};
    std::uint32_t definition{};
    std::uintptr_t instance{};
    std::size_t componentSize{};
    std::size_t fullOffset{static_cast<std::size_t>(-1)};
    std::size_t positionOffset{static_cast<std::size_t>(-1)};
};

/**
 * Searches the three Ikora component graphs for the exact transform selected by the native scene.
 * A full 0x20-byte match proves matrix provenance; a position-only match remains useful when a
 * component stores the same animated socket with a different rotation representation.
 */
[[nodiscard]] std::size_t find_scene_transform_component_matches(
    const SceneType23TransformCapture& capture,
    std::array<SceneTransformComponentMatch, 32U>& matches) noexcept {
    if (!capture.transformReadable) {
        return 0U;
    }

    std::array<IkoraComponent, kMaximumIkoraComponents> components{};
    std::size_t componentCount = 0U;
    AcquireSRWLockShared(&g_ikoraComponentLock);
    componentCount = std::min(g_ikoraComponentCount, components.size());
    std::copy_n(g_ikoraComponents.begin(), componentCount, components.begin());
    ReleaseSRWLockShared(&g_ikoraComponentLock);

    constexpr std::size_t kTransformSize = 0x20U;
    constexpr std::size_t kPositionOffset = 0x10U;
    constexpr std::size_t kPositionSize = 0x0CU;
    constexpr std::size_t kNoOffset = static_cast<std::size_t>(-1);
    std::size_t matchCount = 0U;
    for (std::size_t index = 0U;
         index < componentCount && matchCount < matches.size();
         ++index) {
        const IkoraComponent& component = components[index];
        if (component.instance == 0U || component.factorySequence < 1U
            || component.factorySequence > 3U) {
            continue;
        }
        // +B31910 replaces instance+0x08 while starting the component. The previous recorder read
        // that mutated field later and treated values such as 0x10000 as a component bound,
        // allowing the scan to alias the unrelated scene bank. Use the size captured before the
        // native start callback instead.
        const std::size_t componentSize = component.size;
        if (componentSize < kTransformSize) {
            continue;
        }

        std::size_t fullOffset = kNoOffset;
        std::size_t positionOffset = kNoOffset;
        __try {
            const auto* const bytes = reinterpret_cast<const std::byte*>(component.instance);
            for (std::size_t offset = 0U; offset + kTransformSize <= componentSize;
                 offset += sizeof(std::uint32_t)) {
                if (fullOffset == kNoOffset
                    && std::memcmp(bytes + offset,
                                   capture.transform.data(),
                                   kTransformSize)
                           == 0) {
                    fullOffset = offset;
                }
                if (positionOffset == kNoOffset
                    && std::memcmp(bytes + offset,
                                   capture.transform.data() + kPositionOffset,
                                   kPositionSize)
                           == 0) {
                    positionOffset = offset;
                }
                if (fullOffset != kNoOffset && positionOffset != kNoOffset) {
                    break;
                }
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            fullOffset = kNoOffset;
            positionOffset = kNoOffset;
        }
        if (fullOffset == kNoOffset && positionOffset == kNoOffset) {
            continue;
        }
        matches[matchCount++] = {
            component.factorySequence,
            component.object,
            component.definition,
            component.instance,
            componentSize,
            fullOffset,
            positionOffset,
        };
    }
    return matchCount;
}

[[nodiscard]] float position_distance(const DecodedSceneTransform& transform,
                                      const DecodedObjectPosition& actor) noexcept {
    if (!transform.finite || !actor.valid) {
        return -1.0F;
    }
    const float x = transform.position[0] - actor.value[0];
    const float y = transform.position[1] - actor.value[1];
    const float z = transform.position[2] - actor.value[2];
    return std::sqrt(x * x + y * y + z * z);
}

template <std::size_t Size>
[[nodiscard]] bool prefix_matches(const std::byte* target,
                                  const std::array<std::byte, Size>& expected) noexcept {
    if (target == nullptr) {
        return false;
    }
    __try {
        return std::memcmp(target, expected.data(), expected.size()) == 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void report(const char* format, ...) noexcept {
    std::array<char, core::log::kLineCapacity> line{};
    va_list arguments;
    va_start(arguments, format);
    const int written = std::vsnprintf(line.data(), line.size(), format, arguments);
    va_end(arguments);
    if (written <= 0) {
        return;
    }
    const std::size_t length = std::min(static_cast<std::size_t>(written), line.size() - 1U);
    core::log::write(core::log::Channel::client,
                     core::log::Level::info,
                     {line.data(), length});
}

/** Emits a bounded raw snapshot as dwords so unknown attachment fields remain recoverable. */
template <std::size_t Size>
void report_scene_visual_words(std::uint32_t sequence,
                               const char* source,
                               const void* address,
                               const std::array<std::byte, Size>& snapshot,
                               bool readable) noexcept {
    static_assert(Size % sizeof(std::uint32_t) == 0U);
    if (source == nullptr) {
        return;
    }
    constexpr std::size_t kWordsPerLine = 16U;
    constexpr std::size_t kWordCount = Size / sizeof(std::uint32_t);
    constexpr char kHex[] = "0123456789ABCDEF";
    for (std::size_t first = 0U; first < kWordCount; first += kWordsPerLine) {
        std::array<char, core::log::kLineCapacity> line{};
        const int prefix = std::snprintf(
            line.data(),
            line.size(),
            "ev=omega_scene_vfx_trace stage=raw n=%u source=%s address=%p readable=%s "
            "offset=0x%03llX words=",
            sequence,
            source,
            address,
            readable ? "yes" : "no",
            static_cast<unsigned long long>(first * sizeof(std::uint32_t)));
        if (prefix <= 0) {
            continue;
        }
        std::size_t used = std::min(static_cast<std::size_t>(prefix), line.size() - 1U);
        const std::size_t end = std::min(first + kWordsPerLine, kWordCount);
        for (std::size_t word = first; word < end && used + 9U < line.size(); ++word) {
            std::uint32_t value = 0U;
            std::memcpy(&value,
                        snapshot.data() + word * sizeof(std::uint32_t),
                        sizeof value);
            for (int shift = 28; shift >= 0; shift -= 4) {
                line[used++] = kHex[(value >> static_cast<unsigned int>(shift)) & 0x0FU];
            }
            line[used++] = word + 1U == end ? ' ' : ',';
        }
        constexpr std::string_view kMutation{"mutation=observe_only"};
        if (used + kMutation.size() < line.size()) {
            std::memcpy(line.data() + used, kMutation.data(), kMutation.size());
            used += kMutation.size();
        }
        line[std::min(used, line.size() - 1U)] = '\0';
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), std::min(used, line.size() - 1U)});
    }
}

/** Reports the pinned target bytes independently so an RVA error cannot disable the main probe. */
[[nodiscard]] bool report_scene_visual_target(const std::byte* target) noexcept {
    std::array<std::byte, 24U> actual{};
    const bool readable = safe_copy(actual.data(), target, actual.size());
    const bool match = readable
                       && std::memcmp(actual.data(),
                                      kSceneVisualObjectCreatePrefix.data(),
                                      kSceneVisualObjectCreatePrefix.size()) == 0;
    std::array<char, 256U> expectedHex{};
    std::array<char, 256U> actualHex{};
    constexpr char kHex[] = "0123456789ABCDEF";
    const auto encode = [&](auto values, auto& destination) noexcept {
        std::size_t used = 0U;
        for (std::byte value : values) {
            if (used + 2U >= destination.size()) {
                break;
            }
            const std::uint8_t byte = std::to_integer<std::uint8_t>(value);
            destination[used++] = kHex[byte >> 4U];
            destination[used++] = kHex[byte & 0x0FU];
        }
        destination[used] = '\0';
    };
    encode(kSceneVisualObjectCreatePrefix, expectedHex);
    encode(actual, actualHex);
    report("ev=omega_scene_vfx_trace stage=target rva=0x%llX address=%p readable=%s "
           "prefix=%s expected=%s actual=%s mutation=observe_only",
           static_cast<unsigned long long>(kSceneVisualObjectCreateRva),
           target,
           readable ? "yes" : "no",
           match ? "match" : "mismatch",
           expectedHex.data(),
           actualHex.data());
    return match;
}

/** Validates the leaf scene-event dispatcher independently from the older visual-object probe. */
[[nodiscard]] bool report_scene_authored_event_target(const std::byte* target) noexcept {
    std::array<std::byte, 24U> actual{};
    const bool readable = safe_copy(actual.data(), target, actual.size());
    const bool match = readable
                       && std::memcmp(actual.data(),
                                      kSceneAuthoredEventDispatchPrefix.data(),
                                      kSceneAuthoredEventDispatchPrefix.size()) == 0;
    report("ev=omega_scene_event_trace stage=target rva=0x%llX address=%p "
           "readable=%s prefix=%s mutation=observe_only",
           static_cast<unsigned long long>(kSceneAuthoredEventDispatchRva),
           target,
           readable ? "yes" : "no",
           match ? "match" : "mismatch");
    return match;
}

template <std::size_t Size>
[[nodiscard]] bool report_scene_type23_target(
    const char* site,
    std::uintptr_t rva,
    const std::byte* target,
    const std::array<std::byte, Size>& expected) noexcept {
    std::array<std::byte, 24U> actual{};
    const bool readable = safe_copy(actual.data(), target, actual.size());
    const bool match = readable
                       && std::memcmp(actual.data(), expected.data(), expected.size()) == 0;
    report("ev=omega_scene_type23_trace stage=target site=%s rva=0x%llX address=%p "
           "readable=%s prefix=%s mutation=observe_only",
           site == nullptr ? "unknown" : site,
           static_cast<unsigned long long>(rva),
           target,
           readable ? "yes" : "no",
           match ? "match" : "mismatch");
    return match;
}

/**
 * Reports every beam-recorder target independently. The unpacked image can differ from the
 * VM-mutated live image, so include enough runtime bytes and the first differing byte offset to
 * correct one site without weakening validation for the other two.
 */
template <std::size_t Size>
[[nodiscard]] bool report_visual_recorder_target(
    const char* site,
    std::uintptr_t rva,
    const std::byte* target,
    const std::array<std::byte, Size>& expected) noexcept {
    static_assert(Size <= 32U);
    std::array<std::byte, 32U> actual{};
    const bool readable = safe_copy(actual.data(), target, actual.size());
    const bool match = readable
                       && std::memcmp(actual.data(), expected.data(), expected.size()) == 0;
    std::size_t firstDifference = expected.size();
    if (readable) {
        for (std::size_t index = 0U; index < expected.size(); ++index) {
            if (actual[index] != expected[index]) {
                firstDifference = index;
                break;
            }
        }
    }

    constexpr char kHex[] = "0123456789ABCDEF";
    const auto encode = [&](const auto& values, auto& destination) noexcept {
        std::size_t used = 0U;
        for (std::byte value : values) {
            if (used + 2U >= destination.size()) {
                break;
            }
            const std::uint8_t byte = std::to_integer<std::uint8_t>(value);
            destination[used++] = kHex[byte >> 4U];
            destination[used++] = kHex[byte & 0x0FU];
        }
        destination[used] = '\0';
    };
    std::array<char, Size * 2U + 1U> expectedHex{};
    std::array<char, 32U * 2U + 1U> actualHex{};
    encode(expected, expectedHex);
    encode(actual, actualHex);

    report("ev=omega_beam_final_transform stage=target site=%s rva=+%llX "
           "address=%p readable=%u prefix=%s first_difference=0x%llX "
           "expected=%s actual=%s mutation=observe_only",
           site == nullptr ? "unknown" : site,
           static_cast<unsigned long long>(rva),
           target,
           readable ? 1U : 0U,
           match ? "match" : "mismatch",
           static_cast<unsigned long long>(firstDifference),
           expectedHex.data(),
           actualHex.data());
    return match;
}

[[nodiscard]] bool omega_forced() noexcept {
    state::activity::forced::ForcedDestination forced{};
    state::activity::forced::snapshot(forced);
    const std::string_view package(forced.packageName.data(), forced.packageNameLength);
    return state::activity::forced::override_active() && package == "mission_scot";
}

[[nodiscard]] std::uintptr_t image_rva(std::uintptr_t address) noexcept;

[[nodiscard]] bool scene_entry_trace_forced(std::string_view& package) noexcept {
    static thread_local state::activity::forced::ForcedDestination forced{};
    state::activity::forced::snapshot(forced);
    package = std::string_view(forced.packageName.data(), forced.packageNameLength);
    return state::activity::forced::override_active()
           && (package == "mission_scot" || package == "mission_towerfall"
               || package == "cine_110_twr");
}

/** Owns thread-local correlation for resolver calls made by one native Scene component update. */
__declspec(noinline) void __fastcall scene_entry_update(std::byte* component) noexcept {
    std::byte* const previousComponent = g_sceneEntryUpdateComponent;
    const std::uint32_t previousIndex = g_sceneEntryUpdateIndex;
    g_sceneEntryUpdateComponent = component;
    g_sceneEntryUpdateIndex = 0U;
    const SceneEntryUpdate original =
        g_sceneEntryUpdateOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(component);
    }
    g_sceneEntryUpdateComponent = previousComponent;
    g_sceneEntryUpdateIndex = previousIndex;
}

/** Captures the exact native reference-resolution result without altering it or its output. */
__declspec(noinline) bool __fastcall scene_entry_resolver(const std::byte* reference,
                                                           std::byte* output) noexcept {
    const std::uintptr_t callerRva = image_rva(
        reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
    const bool sceneCaller = callerRva == kSceneEntryResolverReturnRva;
    std::uint64_t referenceRaw = 0U;
    std::array<std::uint64_t, 2U> outputBefore{};
    const bool referenceReadable = sceneCaller
                                   && safe_copy(&referenceRaw,
                                                reference,
                                                sizeof referenceRaw);
    const bool outputBeforeReadable = sceneCaller
                                      && safe_copy(outputBefore.data(),
                                                   output,
                                                   sizeof outputBefore);

    const SceneEntryResolver original =
        g_sceneEntryResolverOriginal.load(std::memory_order_acquire);
    const bool result = original != nullptr && original(reference, output);
    if (!sceneCaller) {
        return result;
    }

    std::array<std::uint64_t, 2U> outputAfter{};
    const bool outputAfterReadable = safe_copy(outputAfter.data(),
                                               output,
                                               sizeof outputAfter);
    const std::uint32_t entryIndex = g_sceneEntryUpdateIndex++;
    const std::uint32_t call =
        g_sceneEntryResolverCalls.fetch_add(1U, std::memory_order_relaxed) + 1U;
    std::string_view package{};
    const bool opening = scene_entry_trace_forced(package);
    if ((!opening && call > 64U) || call > kMaximumSceneEntryResolverLogs) {
        return result;
    }

    const std::uint32_t componentValue = safe_read<std::uint32_t>(
        g_sceneEntryUpdateComponent != nullptr ? g_sceneEntryUpdateComponent + 0x25CU : nullptr,
        0U);
    const std::uint8_t componentActive = safe_read<std::uint8_t>(
        g_sceneEntryUpdateComponent != nullptr ? g_sceneEntryUpdateComponent + 0x260U : nullptr,
        0U);
    const std::uint32_t referenceRegistry = static_cast<std::uint32_t>(referenceRaw);
    const std::uint8_t referenceType = static_cast<std::uint8_t>(referenceRaw >> 32U);
    const std::uint16_t referenceIndex = static_cast<std::uint16_t>(referenceRaw >> 40U);
    report("ev=scene_entry_runtime_resolve stage=return n=%u result=%s "
           "caller=+%llX component=%p component_value=0x%08X component_active=%u "
           "entry=%u reference=%p reference_readable=%u reference_raw=0x%016llX "
           "reference_registry=0x%08X reference_type=%u reference_index=%u "
           "output=%p output_before_readable=%u output_before=0x%016llX,0x%016llX "
           "output_after_readable=%u output_after=0x%016llX,0x%016llX "
           "forced=%.*s mutation=observe_only",
           call,
           result ? "resolved" : "unresolved",
           static_cast<unsigned long long>(callerRva),
           g_sceneEntryUpdateComponent,
           componentValue,
           static_cast<unsigned>(componentActive),
           entryIndex,
           reference,
           referenceReadable ? 1U : 0U,
           static_cast<unsigned long long>(referenceRaw),
           referenceRegistry,
           static_cast<unsigned>(referenceType),
           static_cast<unsigned>(referenceIndex),
           output,
           outputBeforeReadable ? 1U : 0U,
           static_cast<unsigned long long>(outputBefore[0]),
           static_cast<unsigned long long>(outputBefore[1]),
           outputAfterReadable ? 1U : 0U,
           static_cast<unsigned long long>(outputAfter[0]),
           static_cast<unsigned long long>(outputAfter[1]),
           static_cast<int>(package.size()),
           package.data());
    return result;
}

/**
 * Completes the scene handoff and retires the temporary allocator probe from a normal gameplay
 * callback. Detours' protected removal refuses to detach while another release callback is live,
 * so a failed attempt remains pending and is retried on a later behavior tick.
 */
void service_deferred_scene_completion() noexcept {
    const bool completionPending =
        g_sceneCompletionPending.exchange(false, std::memory_order_acq_rel);
    if (completionPending) {
        report("ev=omega_scene_lifecycle stage=deferred_completion result=retired "
               "mutation=observe_only");
    }

    if (!g_indexHeapReleaseDetachPending.load(std::memory_order_acquire)
        || g_indexHeapReleaseDetachBusy.test_and_set(std::memory_order_acquire)) {
        return;
    }

    auto& handle = g_handles[static_cast<std::size_t>(HookSlot::indexHeapRelease)];
    bool replacementActive = false;
    const bool detached = !handle.attached
                          || hooking::detour::uninstall(handle, replacementActive);
    if (detached) {
        g_indexHeapReleaseOriginal.store(nullptr, std::memory_order_release);
        g_indexHeapReleaseDetachPending.store(false, std::memory_order_release);
    }
    report("ev=omega_scene_lifecycle stage=release_probe_retire result=%s active=%u "
           "mutation=detach_allocator_probe",
           detached ? "detached" : "retry",
           replacementActive ? 1U : 0U);
    g_indexHeapReleaseDetachBusy.clear(std::memory_order_release);
}

[[nodiscard]] bool omega_payload(const std::byte* payload) noexcept {
    if (payload == nullptr) {
        return false;
    }
    return safe_read<std::uint32_t>(payload + 0x90U, kAbsentHash) == kOmegaSpawnerRegistry
           || safe_read<std::uint32_t>(payload + 0x98U, kAbsentHash) == kOmegaSpawnerRegistry;
}

[[nodiscard]] std::uintptr_t image_rva(std::uintptr_t address) noexcept {
    const auto* const image = g_image.load(std::memory_order_acquire);
    const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(image);
    return address >= base && address < base + 0x8A5EA00U ? address - base : 0U;
}

void report_native_stack(const char* site,
                         std::uint32_t factorySequence,
                         std::uint32_t object) noexcept {
    if (site == nullptr) {
        return;
    }
    std::array<void*, kNativeStackDepth> frames{};
    const USHORT depth = RtlCaptureStackBackTrace(
        1U,
        static_cast<ULONG>(frames.size()),
        frames.data(),
        nullptr);
    std::array<std::uintptr_t, kNativeStackDepth> rvas{};
    for (USHORT index = 0U; index < depth; ++index) {
        rvas[index] = image_rva(reinterpret_cast<std::uintptr_t>(frames[index]));
    }
    report("ev=omega_ikora_handoff stage=native_stack site=%s factory_n=%u "
           "object=%08X depth=%u rvas=+%llX,+%llX,+%llX,+%llX,+%llX,+%llX,+%llX,+%llX,"
           "+%llX,+%llX,+%llX,+%llX,+%llX,+%llX,+%llX,+%llX mutation=observe_only",
           site,
           factorySequence,
           object,
           static_cast<unsigned int>(depth),
           static_cast<unsigned long long>(rvas[0]),
           static_cast<unsigned long long>(rvas[1]),
           static_cast<unsigned long long>(rvas[2]),
           static_cast<unsigned long long>(rvas[3]),
           static_cast<unsigned long long>(rvas[4]),
           static_cast<unsigned long long>(rvas[5]),
           static_cast<unsigned long long>(rvas[6]),
           static_cast<unsigned long long>(rvas[7]),
           static_cast<unsigned long long>(rvas[8]),
           static_cast<unsigned long long>(rvas[9]),
           static_cast<unsigned long long>(rvas[10]),
           static_cast<unsigned long long>(rvas[11]),
           static_cast<unsigned long long>(rvas[12]),
           static_cast<unsigned long long>(rvas[13]),
           static_cast<unsigned long long>(rvas[14]),
           static_cast<unsigned long long>(rvas[15]));
}

void report_scene_activation_stack(std::uint32_t sceneHandle,
                                   std::uint32_t authoritySequence) noexcept {
    std::array<void*, kNativeStackDepth> frames{};
    const USHORT depth = RtlCaptureStackBackTrace(
        1U,
        static_cast<ULONG>(frames.size()),
        frames.data(),
        nullptr);
    std::array<std::uintptr_t, kNativeStackDepth> rvas{};
    for (USHORT index = 0U; index < depth; ++index) {
        rvas[index] = image_rva(reinterpret_cast<std::uintptr_t>(frames[index]));
    }
    report("ev=omega_scene_activation_owner stage=native_stack scene_handle=%08X "
           "authority_n=%u depth=%u "
           "rvas=+%llX,+%llX,+%llX,+%llX,+%llX,+%llX,+%llX,+%llX,"
           "+%llX,+%llX,+%llX,+%llX,+%llX,+%llX,+%llX,+%llX "
           "mutation=observe_only",
           sceneHandle,
           authoritySequence,
           static_cast<unsigned int>(depth),
           static_cast<unsigned long long>(rvas[0]),
           static_cast<unsigned long long>(rvas[1]),
           static_cast<unsigned long long>(rvas[2]),
           static_cast<unsigned long long>(rvas[3]),
           static_cast<unsigned long long>(rvas[4]),
           static_cast<unsigned long long>(rvas[5]),
           static_cast<unsigned long long>(rvas[6]),
           static_cast<unsigned long long>(rvas[7]),
           static_cast<unsigned long long>(rvas[8]),
           static_cast<unsigned long long>(rvas[9]),
           static_cast<unsigned long long>(rvas[10]),
           static_cast<unsigned long long>(rvas[11]),
           static_cast<unsigned long long>(rvas[12]),
           static_cast<unsigned long long>(rvas[13]),
           static_cast<unsigned long long>(rvas[14]),
           static_cast<unsigned long long>(rvas[15]));
}

void report_scene_transform_writer_stack(std::uint32_t call,
                                         std::uint32_t sceneHandle,
                                         const void* descriptor,
                                         const void* runtime) noexcept {
    std::array<void*, kNativeStackDepth> frames{};
    const USHORT depth = RtlCaptureStackBackTrace(
        1U,
        static_cast<ULONG>(frames.size()),
        frames.data(),
        nullptr);
    std::array<std::uintptr_t, kNativeStackDepth> rvas{};
    for (USHORT index = 0U; index < depth; ++index) {
        rvas[index] = image_rva(reinterpret_cast<std::uintptr_t>(frames[index]));
    }
    report("ev=omega_scene_transform_writer stage=native_stack call=%u scene=%08X "
           "descriptor=%p source_runtime=%p depth=%u "
           "rvas=+%llX,+%llX,+%llX,+%llX,+%llX,+%llX,+%llX,+%llX,"
           "+%llX,+%llX,+%llX,+%llX,+%llX,+%llX,+%llX,+%llX "
           "mutation=observe_only",
           call,
           sceneHandle,
           descriptor,
           runtime,
           static_cast<unsigned int>(depth),
           static_cast<unsigned long long>(rvas[0]),
           static_cast<unsigned long long>(rvas[1]),
           static_cast<unsigned long long>(rvas[2]),
           static_cast<unsigned long long>(rvas[3]),
           static_cast<unsigned long long>(rvas[4]),
           static_cast<unsigned long long>(rvas[5]),
           static_cast<unsigned long long>(rvas[6]),
           static_cast<unsigned long long>(rvas[7]),
           static_cast<unsigned long long>(rvas[8]),
           static_cast<unsigned long long>(rvas[9]),
           static_cast<unsigned long long>(rvas[10]),
           static_cast<unsigned long long>(rvas[11]),
           static_cast<unsigned long long>(rvas[12]),
           static_cast<unsigned long long>(rvas[13]),
           static_cast<unsigned long long>(rvas[14]),
           static_cast<unsigned long long>(rvas[15]));
}

[[nodiscard]] std::uint64_t hash_region(const void* address, std::size_t size) noexcept {
    if (address == nullptr || size == 0U) {
        return 0U;
    }
    std::uint64_t hash = 1469598103934665603ULL;
    __try {
        const auto* const bytes = static_cast<const std::byte*>(address);
        for (std::size_t index = 0; index < size; ++index) {
            hash ^= std::to_integer<std::uint8_t>(bytes[index]);
            hash *= 1099511628211ULL;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0U;
    }
    return hash;
}

struct SceneAuthoredEventView final {
    const std::byte* authored{};
    const std::byte* runtime{};
    std::uint32_t sceneHandle{kInvalidHandle};
    std::uint8_t type{0xFFU};
    std::uintptr_t callback{};
    const char* stage{"input"};
    std::uint64_t page{};
    std::uintptr_t poolHolder{};
    std::uintptr_t pool{};
    std::uintptr_t descriptor{};
    std::int32_t stride{};
    std::int32_t maskValue{};
    std::uintptr_t records{};
    std::uintptr_t record{};
    std::uint64_t recordMask{};
    std::uintptr_t sceneData{};
    std::uintptr_t adjustment{};
    std::uintptr_t base{};
    std::int64_t eventVectorRelative{INT64_MIN};
    std::uintptr_t eventVector{};
    std::uintptr_t eventSlot{};
    std::int64_t eventRelative{INT64_MIN};
    std::uintptr_t runtimeAnchor{};
    std::int64_t runtimeVectorRelative{INT64_MIN};
    std::uintptr_t runtimeVector{};
    std::uintptr_t runtimeSlot{};
    std::int64_t runtimeRelative{INT64_MIN};
};

/** Adds one bounded signed relative offset without wrapping the process address space. */
[[nodiscard]] std::uintptr_t add_relative(std::uintptr_t base,
                                          std::int64_t relative) noexcept {
    constexpr std::int64_t kMaximumRelative = 0x40000000LL;
    if (base == 0U || relative <= -kMaximumRelative || relative >= kMaximumRelative) {
        return 0U;
    }
    if (relative >= 0) {
        const auto distance = static_cast<std::uintptr_t>(relative);
        return base <= UINTPTR_MAX - distance ? base + distance : 0U;
    }
    const auto distance = static_cast<std::uintptr_t>(-relative);
    return base >= distance ? base - distance : 0U;
}

/** Returns a bounded signed address delta for recorder output. */
[[nodiscard]] std::int64_t pointer_delta(std::uintptr_t address,
                                         std::uintptr_t base) noexcept {
    if (address == 0U || base == 0U) {
        return INT64_MIN;
    }
    if (address >= base) {
        const std::uintptr_t distance = address - base;
        return distance <= static_cast<std::uintptr_t>(INT64_MAX)
                   ? static_cast<std::int64_t>(distance)
                   : INT64_MIN;
    }
    const std::uintptr_t distance = base - address;
    return distance <= static_cast<std::uintptr_t>(INT64_MAX)
               ? -static_cast<std::int64_t>(distance)
               : INT64_MIN;
}

/**
 * Repeats only the pointer arithmetic performed by +5873F0 before its indirect start callback.
 * Every dereference is guarded; failure returns an empty view and never changes scene state.
 */
[[nodiscard]] SceneAuthoredEventView resolve_scene_authored_event(
    const std::uint32_t* scene,
    std::int32_t eventIndex) noexcept {
    SceneAuthoredEventView view{};
    const auto* const image = g_image.load(std::memory_order_acquire);
    if (image == nullptr || scene == nullptr || eventIndex < 0 || eventIndex > 4096) {
        return view;
    }

    view.stage = "scene_handle";
    view.sceneHandle = safe_read<std::uint32_t>(scene, kInvalidHandle);
    if (view.sceneHandle == kInvalidHandle) {
        return view;
    }
    const std::uint32_t partition = static_cast<std::uint32_t>(
        static_cast<std::int32_t>(view.sceneHandle) >> 13);
    view.page =
        (static_cast<std::uint64_t>(partition | 0x0FFC0000U) >> 18U)
        & static_cast<std::uint64_t>(partition & 0xFFFFU);
    view.stage = "pool_holder";
    view.poolHolder = safe_read<std::uintptr_t>(
        image + kScenePoolTablePointerRva, 0U);
    view.stage = "pool";
    view.pool = safe_read<std::uintptr_t>(
        reinterpret_cast<const void*>(view.poolHolder), 0U);
    if (view.poolHolder == 0U || view.pool == 0U || view.page > 0x10000U) {
        return view;
    }
    view.stage = "descriptor";
    view.descriptor = view.pool + view.page * 0x40U;
    view.stride = safe_read<std::int32_t>(
        reinterpret_cast<const void*>(view.descriptor + 0x30U), 0);
    view.maskValue = safe_read<std::int32_t>(
        reinterpret_cast<const void*>(view.descriptor + 0x34U), 0);
    view.records = safe_read<std::uintptr_t>(
        reinterpret_cast<const void*>(view.descriptor + 0x08U), 0U);
    if (view.stride <= 0 || view.stride > 0x100000 || view.records == 0U) {
        return view;
    }
    view.stage = "record";
    view.record = view.records
                  + static_cast<std::uintptr_t>(view.sceneHandle & 0x1FFFU)
                        * static_cast<std::uintptr_t>(view.stride);
    view.recordMask = safe_read<std::uint64_t>(
        reinterpret_cast<const void*>(view.record + 0x08U), 0U);
    view.sceneData = safe_read<std::uintptr_t>(
        reinterpret_cast<const std::byte*>(scene) + 0x08U, 0U);
    view.adjustment = static_cast<std::uintptr_t>(
        static_cast<std::uint64_t>(static_cast<std::int64_t>(view.maskValue))
        & view.recordMask);
    if (view.sceneData == 0U) {
        return view;
    }
    view.stage = "event_vector";
    // Match the native x64 add/sub sequence exactly. The descriptor mask is
    // commonly sign-extended (for example FFFFFFFF8FAFF500), so the subtract
    // intentionally wraps modulo 2^64 to produce the valid scene-data base.
    view.base = static_cast<std::uintptr_t>(
        static_cast<std::uint64_t>(view.record)
        + static_cast<std::uint64_t>(view.sceneData)
        - static_cast<std::uint64_t>(view.adjustment));
    view.eventVectorRelative = safe_read<std::int64_t>(
        reinterpret_cast<const void*>(view.base + 0x170U), INT64_MIN);
    view.eventVector = view.eventVectorRelative == INT64_MIN
                           ? 0U
                           : add_relative(view.base + 0x190U,
                                          view.eventVectorRelative);
    if (view.eventVector == 0U) {
        return view;
    }
    view.stage = "event_record";
    view.eventSlot = view.eventVector
                     + static_cast<std::uintptr_t>(eventIndex) * 0x18U;
    view.eventRelative = safe_read<std::int64_t>(
        reinterpret_cast<const void*>(view.eventSlot), INT64_MIN);
    const std::uintptr_t authored =
        view.eventRelative == INT64_MIN || view.eventRelative == 0
            ? 0U
            : add_relative(view.eventSlot, view.eventRelative);
    view.authored = reinterpret_cast<const std::byte*>(authored);
    if (view.authored != nullptr) {
        view.stage = "event_type";
        view.type = safe_read<std::uint8_t>(view.authored + 0x30U, 0xFFU);
        if (view.type < 72U) {
            view.callback = safe_read<std::uintptr_t>(
                image + kSceneEventHandlerTableRva
                    + static_cast<std::size_t>(view.type) * kSceneEventHandlerStride + 0x08U,
                0U);
        }
    }

    view.runtimeAnchor = reinterpret_cast<std::uintptr_t>(scene) + 0xB8U;
    view.runtimeVectorRelative = safe_read<std::int64_t>(
        reinterpret_cast<const void*>(view.runtimeAnchor), INT64_MIN);
    view.runtimeVector = view.runtimeVectorRelative == INT64_MIN
                             ? 0U
                             : add_relative(view.runtimeAnchor,
                                            view.runtimeVectorRelative);
    if (view.runtimeVector != 0U) {
        view.runtimeSlot = view.runtimeVector
                           + static_cast<std::uintptr_t>(eventIndex + 1) * 0x30U;
        view.runtimeRelative = safe_read<std::int64_t>(
            reinterpret_cast<const void*>(view.runtimeSlot), INT64_MIN);
        if (view.runtimeRelative != INT64_MIN && view.runtimeRelative != 0) {
            view.runtime = reinterpret_cast<const std::byte*>(
                add_relative(view.runtimeSlot, view.runtimeRelative));
        }
    }
    if (view.authored != nullptr && view.type < 72U) {
        view.stage = "ready";
    }
    return view;
}

struct SceneActorSlotCapture final {
    bool valid{};
    std::uint32_t* scene{};
    std::uint32_t sceneHandle{kInvalidHandle};
    std::uint32_t slotIndex{kInvalidHandle};
    std::uint8_t actorCount{};
    std::int64_t authoredVectorRelative{INT64_MIN};
    std::int64_t runtimeVectorRelative{INT64_MIN};
    const std::byte* authoredSlot{};
    const std::byte* runtimeSlot{};
    std::array<std::uint32_t, 6U> authoredWords{};
    std::int32_t runtimeC8{-1};
    std::int32_t runtimeCC{-1};
    std::uint8_t runtimeD4{};
};

/**
 * Resolves the exact actor-slot inputs consumed by +5902C0 around its call to +56D990.
 *
 * The factory result pointer is the scheduler's stack local at rsp+0x20. The loop stores
 * index*3 at rsp+0x38, so result+0x18 recovers the slot without reading registers or patching a
 * mid-function instruction. All native relative pointers are independently bounded.
 */
[[nodiscard]] SceneActorSlotCapture capture_scene_actor_slot(
    std::uint32_t* scene,
    std::uint32_t slotIndex) noexcept {
    SceneActorSlotCapture capture{};
    capture.scene = scene;
    capture.slotIndex = slotIndex;
    if (capture.scene == nullptr || capture.slotIndex > 31U) {
        return capture;
    }

    const SceneAuthoredEventView sceneView = resolve_scene_authored_event(capture.scene, 0);
    if (sceneView.base == 0U) {
        return capture;
    }
    capture.sceneHandle = safe_read<std::uint32_t>(capture.scene, kInvalidHandle);
    capture.actorCount = safe_read<std::uint8_t>(
        reinterpret_cast<const void*>(sceneView.base + 0x2E3U), 0U);
    if (capture.slotIndex >= capture.actorCount || capture.actorCount > 32U) {
        return capture;
    }

    capture.authoredVectorRelative = safe_read<std::int64_t>(
        reinterpret_cast<const void*>(sceneView.base + 0x150U), INT64_MIN);
    capture.runtimeVectorRelative = safe_read<std::int64_t>(
        reinterpret_cast<const std::byte*>(capture.scene) + 0x98U, INT64_MIN);
    const std::uintptr_t authoredVector = capture.authoredVectorRelative == INT64_MIN
                                              ? 0U
                                              : add_relative(sceneView.base,
                                                             capture.authoredVectorRelative);
    const std::uintptr_t runtimeVector = capture.runtimeVectorRelative == INT64_MIN
                                             ? 0U
                                             : add_relative(
                                                   reinterpret_cast<std::uintptr_t>(capture.scene),
                                                   capture.runtimeVectorRelative);
    if (authoredVector == 0U || runtimeVector == 0U) {
        return capture;
    }

    capture.authoredSlot = reinterpret_cast<const std::byte*>(
        authoredVector + 0x170U + static_cast<std::size_t>(capture.slotIndex) * 0x18U);
    capture.runtimeSlot = reinterpret_cast<const std::byte*>(
        runtimeVector + 0xC8U + static_cast<std::size_t>(capture.slotIndex) * 0x30U);
    if (!safe_copy(capture.authoredWords.data(),
                   capture.authoredSlot,
                   sizeof capture.authoredWords)) {
        return capture;
    }
    capture.runtimeC8 = safe_read<std::int32_t>(capture.runtimeSlot, -1);
    capture.runtimeCC = safe_read<std::int32_t>(capture.runtimeSlot + 0x04U, -1);
    capture.runtimeD4 = safe_read<std::uint8_t>(capture.runtimeSlot + 0x0CU, 0U);
    capture.valid = true;
    return capture;
}

[[nodiscard]] SceneActorSlotCapture capture_scene_actor_slot(
    const std::int32_t* factoryResult) noexcept {
    if (g_sceneActorSchedulerDepth == 0U || g_sceneActorSchedulerScene == nullptr
        || factoryResult == nullptr) {
        return {};
    }
    const std::uint64_t tripledIndex = safe_read<std::uint64_t>(
        reinterpret_cast<const std::byte*>(factoryResult) + 0x18U, UINT64_MAX);
    if (tripledIndex == UINT64_MAX || tripledIndex % 3U != 0U
        || tripledIndex / 3U > 31U) {
        return {};
    }
    return capture_scene_actor_slot(
        g_sceneActorSchedulerScene,
        static_cast<std::uint32_t>(tripledIndex / 3U));
}

void report_scene_actor_slot(const SceneActorSlotCapture& capture,
                             const char* moment,
                             std::uint32_t factorySequence) noexcept {
    if (!capture.valid) {
        report("ev=omega_scene_cast_slot moment=%s factory_n=%u result=unresolved "
               "scene=%p scene_handle=%08X slot=%u actor_count=%u mutation=observe_only",
               moment,
               factorySequence,
               capture.scene,
               capture.sceneHandle,
               capture.slotIndex,
               static_cast<unsigned int>(capture.actorCount));
        return;
    }
    report("ev=omega_scene_cast_slot moment=%s factory_n=%u result=resolved scene=%p "
           "scene_handle=%08X slot=%u actor_count=%u authored_rel=%lld runtime_rel=%lld "
           "authored_slot=%p authored=%08X,%08X,%08X,%08X,%08X,%08X runtime_slot=%p "
           "runtime_c8=%d runtime_cc=%d runtime_d4=%u mutation=observe_only",
           moment,
           factorySequence,
           capture.scene,
           capture.sceneHandle,
           capture.slotIndex,
           static_cast<unsigned int>(capture.actorCount),
           static_cast<long long>(capture.authoredVectorRelative),
           static_cast<long long>(capture.runtimeVectorRelative),
           capture.authoredSlot,
           capture.authoredWords[0],
           capture.authoredWords[1],
           capture.authoredWords[2],
           capture.authoredWords[3],
           capture.authoredWords[4],
           capture.authoredWords[5],
           capture.runtimeSlot,
           capture.runtimeC8,
           capture.runtimeCC,
           static_cast<unsigned int>(capture.runtimeD4));
}

[[nodiscard]] std::uint32_t omega_scene_activation_bit(
    std::uint32_t sceneHandle) noexcept {
    switch (sceneHandle) {
        case kOmegaIkoraSceneOneHandle:
            return 1U << 0U;
        case kOmegaIkoraSceneTwoHandle:
            return 1U << 1U;
        case kOmegaIkoraSceneThreeHandle:
            return 1U << 2U;
        default:
            return 0U;
    }
}

[[nodiscard]] std::size_t omega_scene_activation_index(
    std::uint32_t sceneHandle) noexcept {
    switch (sceneHandle) {
        case kOmegaIkoraSceneOneHandle:
            return 0U;
        case kOmegaIkoraSceneTwoHandle:
            return 1U;
        case kOmegaIkoraSceneThreeHandle:
            return 2U;
        default:
            return 3U;
    }
}

/**
 * Correlates one native scene initialization with the most recent decoded type-43 state.
 *
 * +5902C0 runs before +5FC0A0 and the initial authored-event advancement in +5FE260. Capturing
 * here therefore observes the scene's input state before its actor factory, timeline, or visual
 * callbacks can mutate it. The authority observation is a value copy made by +B41DD0; no stale
 * network or component pointer is dereferenced here.
 */
void report_scene_activation_owner(std::uint32_t* scene,
                                   const char* moment,
                                   std::uintptr_t callerRva,
                                   std::uint32_t spawnedMask) noexcept {
    const std::uint32_t sceneHandle = safe_read<std::uint32_t>(scene, kInvalidHandle);
    const SceneAuthoredEventView view = resolve_scene_authored_event(scene, 0);
    OmegaSceneAuthorityObservation authority{};
    const bool authorityValid = snapshot_omega_scene_authority_observation(authority);
    const std::uint64_t now = GetTickCount64();
    const std::uint64_t authorityAge = authorityValid && now >= authority.tickMs
                                           ? now - authority.tickMs
                                           : UINT64_MAX;
    const auto* const bytes = reinterpret_cast<const std::byte*>(scene);
    const std::uint8_t actorCount = view.base == 0U
                                        ? 0U
                                        : safe_read<std::uint8_t>(
                                              reinterpret_cast<const void*>(view.base + 0x2E3U),
                                              0U);

    report("ev=omega_scene_activation_owner moment=%s scene=%p scene_handle=%08X "
           "caller=+%llX actor_count=%u spawned_mask=%08X scene_data=%p authored_base=%p "
           "scene_024=%08X scene_028=%08X scene_02C=%08X runtime_e0=%d runtime_f8=%d "
           "flag_243=%u flag_246=%u flag_247=%u authority_present=%u authority_n=%u "
           "authority_tick_ms=%llu authority_age_ms=%llu authority_component=%p datum_key=%p "
           "datum_identity=%08X mutation=observe_only",
           moment,
           scene,
           sceneHandle,
           static_cast<unsigned long long>(callerRva),
           static_cast<unsigned int>(actorCount),
           spawnedMask,
           reinterpret_cast<void*>(view.sceneData),
           reinterpret_cast<void*>(view.base),
           safe_read<std::uint32_t>(bytes + 0x24U, 0U),
           safe_read<std::uint32_t>(bytes + 0x28U, 0U),
           safe_read<std::uint32_t>(bytes + 0x2CU, 0U),
           safe_read<std::int32_t>(bytes + 0xE0U, -1),
           safe_read<std::int32_t>(bytes + 0xF8U, -1),
           static_cast<unsigned int>(safe_read<std::uint8_t>(bytes + 0x243U, 0U)),
           static_cast<unsigned int>(safe_read<std::uint8_t>(bytes + 0x246U, 0U)),
           static_cast<unsigned int>(safe_read<std::uint8_t>(bytes + 0x247U, 0U)),
           authorityValid ? 1U : 0U,
           authority.sequence,
           static_cast<unsigned long long>(authority.tickMs),
           static_cast<unsigned long long>(authorityAge),
           reinterpret_cast<void*>(authority.component),
           reinterpret_cast<void*>(authority.datumKey),
           authority.datumIdentity);
    report("ev=omega_scene_activation_authority moment=%s scene_handle=%08X authority_n=%u "
           "source=%p source_valid=%u source_hash=%016llX source_00=%08X source_04=%08X "
           "source_08=%08X source_4C=%08X source_98=%08X component_value_before=%08X "
           "component_value_after=%08X component_active_before=%u component_active_after=%u "
           "mutation=observe_only",
           moment,
           sceneHandle,
           authority.sequence,
           reinterpret_cast<void*>(authority.source),
           authority.sourceValid ? 1U : 0U,
           static_cast<unsigned long long>(authority.sourceHash),
           authority.source00,
           authority.source04,
           authority.source08,
           authority.source4C,
           authority.source98,
           authority.componentValueBefore,
           authority.componentValueAfter,
           static_cast<unsigned int>(authority.componentActiveBefore),
           static_cast<unsigned int>(authority.componentActiveAfter));
}

/**
 * Repeats the two native callback-context accessors at +589020 and +589050.
 *
 * A scene-event callback does not receive the outer scene pointer accepted by +5873F0. Its RCX is
 * a short-lived callback frame whose +0x10 field anchors the authored-event vector and whose +0x08
 * field points at the per-scene runtime data. Keeping this resolver separate prevents the stack
 * frame's event index from being mistaken for a scene object handle.
 */
[[nodiscard]] SceneAuthoredEventView resolve_scene_type23_context(
    const std::uint32_t* context,
    std::int32_t eventIndex) noexcept {
    SceneAuthoredEventView view{};
    if (context == nullptr || eventIndex < 0 || eventIndex > 4096) {
        return view;
    }

    view.stage = "callback_context";
    view.sceneData = safe_read<std::uintptr_t>(
        reinterpret_cast<const std::byte*>(context) + 0x08U, 0U);
    view.base = safe_read<std::uintptr_t>(
        reinterpret_cast<const std::byte*>(context) + 0x10U, 0U);
    if (view.sceneData == 0U || view.base == 0U) {
        return view;
    }
    view.sceneHandle = safe_read<std::uint32_t>(
        reinterpret_cast<const void*>(view.sceneData), kInvalidHandle);

    // +589020: anchor = *(context+0x10)+0x170; vector = anchor+*anchor;
    // return vector + eventIndex*0x18 + 0x20.
    view.stage = "callback_authored_vector";
    const std::uintptr_t authoredAnchor = view.base + 0x170U;
    view.eventVectorRelative = safe_read<std::int64_t>(
        reinterpret_cast<const void*>(authoredAnchor), INT64_MIN);
    view.eventVector = view.eventVectorRelative == INT64_MIN
                           ? 0U
                           : add_relative(authoredAnchor, view.eventVectorRelative);
    if (view.eventVector != 0U) {
        view.eventSlot = view.eventVector
                         + static_cast<std::uintptr_t>(eventIndex) * 0x18U + 0x20U;
        view.eventRelative = safe_read<std::int64_t>(
            reinterpret_cast<const void*>(view.eventSlot), INT64_MIN);
        if (view.eventRelative != INT64_MIN && view.eventRelative != 0) {
            view.authored = reinterpret_cast<const std::byte*>(
                add_relative(view.eventSlot, view.eventRelative));
        }
    }

    // +589050: anchor = *(context+0x08)+0xB8; vector = anchor+*anchor;
    // return vector + (eventIndex*3+3)*0x10.
    view.stage = "callback_runtime_vector";
    view.runtimeAnchor = view.sceneData + 0xB8U;
    view.runtimeVectorRelative = safe_read<std::int64_t>(
        reinterpret_cast<const void*>(view.runtimeAnchor), INT64_MIN);
    view.runtimeVector = view.runtimeVectorRelative == INT64_MIN
                             ? 0U
                             : add_relative(view.runtimeAnchor,
                                            view.runtimeVectorRelative);
    if (view.runtimeVector != 0U) {
        view.runtimeSlot = view.runtimeVector
                           + static_cast<std::uintptr_t>(eventIndex + 1) * 0x30U;
        view.runtimeRelative = safe_read<std::int64_t>(
            reinterpret_cast<const void*>(view.runtimeSlot), INT64_MIN);
        if (view.runtimeRelative != INT64_MIN && view.runtimeRelative != 0) {
            view.runtime = reinterpret_cast<const std::byte*>(
                add_relative(view.runtimeSlot, view.runtimeRelative));
        }
    }

    if (view.authored != nullptr) {
        view.type = safe_read<std::uint8_t>(view.authored + 0x30U, 0xFFU);
    }
    if (view.authored != nullptr && view.runtime != nullptr) {
        view.stage = "ready";
    }
    return view;
}

/** Emits bounded qword snapshots of the authored and live event records. */
template <std::size_t Size>
void report_scene_event_words(std::uint32_t sequence,
                              const char* source,
                              const void* address,
                              const std::array<std::byte, Size>& snapshot,
                              bool readable) noexcept {
    static_assert(Size % sizeof(std::uint64_t) == 0U);
    constexpr std::size_t kWordsPerLine = 8U;
    constexpr std::size_t kWordCount = Size / sizeof(std::uint64_t);
    for (std::size_t first = 0U; first < kWordCount; first += kWordsPerLine) {
        std::array<char, core::log::kLineCapacity> line{};
        const int prefix = std::snprintf(
            line.data(),
            line.size(),
            "ev=omega_scene_event_trace stage=raw n=%u source=%s address=%p "
            "readable=%s offset=0x%03llX qwords=",
            sequence,
            source,
            address,
            readable ? "yes" : "no",
            static_cast<unsigned long long>(first * sizeof(std::uint64_t)));
        if (prefix <= 0) {
            continue;
        }
        std::size_t used = std::min(static_cast<std::size_t>(prefix), line.size() - 1U);
        const std::size_t end = std::min(first + kWordsPerLine, kWordCount);
        for (std::size_t word = first; word < end && used + 18U < line.size(); ++word) {
            std::uint64_t value = 0U;
            std::memcpy(&value,
                        snapshot.data() + word * sizeof(std::uint64_t),
                        sizeof value);
            const int written = std::snprintf(line.data() + used,
                                              line.size() - used,
                                              "%016llX%s",
                                              static_cast<unsigned long long>(value),
                                              word + 1U == end ? " " : ",");
            if (written <= 0) {
                break;
            }
            used = std::min(used + static_cast<std::size_t>(written), line.size() - 1U);
        }
        constexpr std::string_view kMutation{"mutation=observe_only"};
        if (used + kMutation.size() < line.size()) {
            std::memcpy(line.data() + used, kMutation.data(), kMutation.size());
            used += kMutation.size();
        }
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), std::min(used, line.size() - 1U)});
    }
}

/** Emits exact-callback snapshots separately from the outer dispatcher samples. */
template <std::size_t Size>
void report_scene_type23_words(std::uint32_t sequence,
                               const char* source,
                               const void* address,
                               const std::array<std::byte, Size>& snapshot,
                               bool readable) noexcept {
    static_assert(Size % sizeof(std::uint64_t) == 0U);
    constexpr std::size_t kWordsPerLine = 8U;
    constexpr std::size_t kWordCount = Size / sizeof(std::uint64_t);
    for (std::size_t first = 0U; first < kWordCount; first += kWordsPerLine) {
        std::array<char, core::log::kLineCapacity> line{};
        const int prefix = std::snprintf(
            line.data(),
            line.size(),
            "ev=omega_scene_type23_trace stage=raw n=%u source=%s address=%p "
            "readable=%s offset=0x%03llX qwords=",
            sequence,
            source == nullptr ? "unknown" : source,
            address,
            readable ? "yes" : "no",
            static_cast<unsigned long long>(first * sizeof(std::uint64_t)));
        if (prefix <= 0) {
            continue;
        }
        std::size_t used = std::min(static_cast<std::size_t>(prefix), line.size() - 1U);
        const std::size_t end = std::min(first + kWordsPerLine, kWordCount);
        for (std::size_t word = first; word < end && used + 18U < line.size(); ++word) {
            std::uint64_t value = 0U;
            std::memcpy(&value,
                        snapshot.data() + word * sizeof(std::uint64_t),
                        sizeof value);
            const int written = std::snprintf(line.data() + used,
                                              line.size() - used,
                                              "%016llX%s",
                                              static_cast<unsigned long long>(value),
                                              word + 1U == end ? " " : ",");
            if (written <= 0) {
                break;
            }
            used = std::min(used + static_cast<std::size_t>(written), line.size() - 1U);
        }
        constexpr std::string_view kMutation{"mutation=observe_only"};
        if (used + kMutation.size() < line.size()) {
            std::memcpy(line.data() + used, kMutation.data(), kMutation.size());
            used += kMutation.size();
        }
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), std::min(used, line.size() - 1U)});
    }
}

[[nodiscard]] const std::byte* object_record(std::uint32_t handle) noexcept {
    const auto* const image = g_image.load(std::memory_order_acquire);
    if (image == nullptr || handle == kInvalidHandle) {
        return nullptr;
    }
    const auto* const table = safe_read<const std::byte*>(image + kObjectTablePointerRva, nullptr);
    const std::uint32_t stride = safe_read<std::uint32_t>(image + kObjectTableStrideRva, 0U);
    if (table == nullptr || stride == 0U || stride > 0x10000U) {
        return nullptr;
    }
    return table + static_cast<std::size_t>(handle & 0x1FFFU) * stride;
}

/** Resolves the paged scene-object record exactly as the native type-23 callback does. */
[[nodiscard]] const std::byte* scene_object_record(std::uint32_t handle) noexcept {
    const auto* const image = g_image.load(std::memory_order_acquire);
    if (image == nullptr || handle == kInvalidHandle) {
        return nullptr;
    }

    const std::uint32_t partition = static_cast<std::uint32_t>(
        static_cast<std::int32_t>(handle) >> 13);
    const std::uint64_t page =
        (static_cast<std::uint64_t>(partition | 0x0FFC0000U) >> 18U)
        & static_cast<std::uint64_t>(partition & 0xFFFFU);
    const std::uintptr_t poolHolder = safe_read<std::uintptr_t>(
        image + kScenePoolTablePointerRva, 0U);
    const std::uintptr_t pool = safe_read<std::uintptr_t>(
        reinterpret_cast<const void*>(poolHolder), 0U);
    if (poolHolder == 0U || pool == 0U || page > 0x10000U) {
        return nullptr;
    }

    const std::uintptr_t descriptor = pool + page * 0x40U;
    const std::int32_t stride = safe_read<std::int32_t>(
        reinterpret_cast<const void*>(descriptor + 0x30U), 0);
    const std::int32_t maskValue = safe_read<std::int32_t>(
        reinterpret_cast<const void*>(descriptor + 0x34U), 0);
    const std::uintptr_t records = safe_read<std::uintptr_t>(
        reinterpret_cast<const void*>(descriptor + 0x08U), 0U);
    if (stride <= 0 || stride > 0x100000 || records == 0U) {
        return nullptr;
    }

    const std::uintptr_t unadjusted =
        records + static_cast<std::uintptr_t>(handle & 0x1FFFU)
                      * static_cast<std::uintptr_t>(stride);
    const std::uint64_t recordMask = safe_read<std::uint64_t>(
        reinterpret_cast<const void*>(unadjusted + 0x08U), 0U);
    const std::uintptr_t adjustment = static_cast<std::uintptr_t>(
        static_cast<std::uint64_t>(static_cast<std::int64_t>(maskValue)) & recordMask);
    return reinterpret_cast<const std::byte*>(unadjusted - adjustment);
}

/** Resolves the datum/relative pair consumed at +A1F360 and again by its provider. */
[[nodiscard]] std::uintptr_t resolve_scene_relative_record(const std::byte* row,
                                                           std::uint32_t& handle,
                                                           std::int64_t& relative,
                                                           std::uintptr_t& record) noexcept {
    handle = safe_read<std::uint32_t>(row, kInvalidHandle);
    relative = safe_read<std::int64_t>(row == nullptr ? nullptr : row + 0x08U, INT64_MIN);
    record = reinterpret_cast<std::uintptr_t>(scene_object_record(handle));
    if (record == 0U || relative == INT64_MIN) {
        return 0U;
    }
    return add_relative(record, relative);
}

[[nodiscard]] bool object_handle_is_current(std::uint32_t handle) noexcept {
    const std::byte* const record = object_record(handle);
    return record != nullptr
           && safe_read<std::uint32_t>(record + 0x0CU, kInvalidHandle) == handle;
}

[[nodiscard]] std::uintptr_t component_handler_rva(const std::uintptr_t* entry) noexcept {
    const std::uintptr_t descriptor = safe_read<std::uintptr_t>(entry, 0U);
    const std::uintptr_t relative = safe_read<std::uintptr_t>(
        reinterpret_cast<const void*>(descriptor + 0x18U), UINTPTR_MAX);
    if (descriptor < 0x10000U || relative == UINTPTR_MAX || relative > 0x10000U) {
        return 0U;
    }
    const std::uintptr_t handler = safe_read<std::uintptr_t>(
        reinterpret_cast<const void*>(descriptor + 0x30U + relative), 0U);
    return image_rva(handler);
}

[[nodiscard]] std::size_t watched_component_index(std::uint32_t definition) noexcept {
    for (std::size_t index = 0; index < kWatchedComponents.size(); ++index) {
        if (kWatchedComponents[index].definition == definition) {
            return index;
        }
    }
    return kWatchedComponents.size();
}

struct HandleMatch final {
    std::int32_t objectOffset{-1};
    std::int32_t entityOffset{-1};

    [[nodiscard]] bool any() const noexcept {
        return objectOffset >= 0 || entityOffset >= 0;
    }
};

[[nodiscard]] HandleMatch find_handles(const std::byte* address,
                                       std::size_t size,
                                       std::uint32_t object,
                                       std::uint32_t entity) noexcept {
    HandleMatch match{};
    if (address == nullptr) {
        return match;
    }
    __try {
        for (std::size_t offset = 0U; offset + sizeof(std::uint32_t) <= size;
             offset += sizeof(std::uint32_t)) {
            const std::uint32_t value = *reinterpret_cast<const std::uint32_t*>(address + offset);
            if (match.objectOffset < 0 && object != kInvalidHandle && value == object) {
                match.objectOffset = static_cast<std::int32_t>(offset);
            }
            if (match.entityOffset < 0 && entity != kInvalidHandle && value == entity) {
                match.entityOffset = static_cast<std::int32_t>(offset);
            }
            if (match.objectOffset >= 0 && match.entityOffset >= 0) {
                break;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return {};
    }
    return match;
}

void remember_ikora_component(std::uintptr_t instance,
                              std::uint32_t definition,
                              std::uintptr_t handlerRva,
                              std::uint32_t factorySequence,
                              std::uint32_t object,
                              std::size_t componentSize) noexcept {
    if (instance == 0U) {
        return;
    }
    AcquireSRWLockExclusive(&g_ikoraComponentLock);
    for (std::size_t index = 0; index < g_ikoraComponentCount; ++index) {
        if (g_ikoraComponents[index].instance == instance) {
            ReleaseSRWLockExclusive(&g_ikoraComponentLock);
            return;
        }
    }
    if (g_ikoraComponentCount < g_ikoraComponents.size()) {
        g_ikoraComponents[g_ikoraComponentCount++] = {
            instance, handlerRva, definition, factorySequence, object,
            componentSize,
            hash_region(reinterpret_cast<const void*>(instance), 0x100U)};
    }
    ReleaseSRWLockExclusive(&g_ikoraComponentLock);
}

void remember_ikora_actor(std::uint32_t factorySequence,
                          std::uint32_t object,
                          std::uintptr_t factoryCallerRva,
                          const std::byte* descriptor) noexcept {
    if (factorySequence == 0U) {
        return;
    }
    AcquireSRWLockExclusive(&g_ikoraActorLock);
    for (std::size_t index = 0; index < g_ikoraActorCount; ++index) {
        IkoraActor& actor = g_ikoraActors[index];
        if (actor.factorySequence == factorySequence) {
            actor.object = object;
            actor.factoryCallerRva = factoryCallerRva;
            actor.descriptor = reinterpret_cast<std::uintptr_t>(descriptor);
            ReleaseSRWLockExclusive(&g_ikoraActorLock);
            return;
        }
    }
    if (g_ikoraActorCount < g_ikoraActors.size()) {
        IkoraActor& actor = g_ikoraActors[g_ikoraActorCount++];
        actor = {};
        actor.factorySequence = factorySequence;
        actor.object = object;
        actor.entity = kInvalidHandle;
        actor.factoryCallerRva = factoryCallerRva;
        actor.descriptor = reinterpret_cast<std::uintptr_t>(descriptor);
        actor.createdAtMs = GetTickCount64();
    }
    ReleaseSRWLockExclusive(&g_ikoraActorLock);
}

void remember_ikora_network(std::uint32_t factorySequence,
                            std::uint32_t object,
                            std::uint32_t entity,
                            const std::byte* networkRecord) noexcept {
    AcquireSRWLockExclusive(&g_ikoraActorLock);
    for (std::size_t index = 0; index < g_ikoraActorCount; ++index) {
        IkoraActor& actor = g_ikoraActors[index];
        if ((factorySequence != 0U && actor.factorySequence == factorySequence)
            || (factorySequence == 0U && actor.object == object)) {
            actor.object = object;
            actor.entity = entity;
            actor.networkRecord = reinterpret_cast<std::uintptr_t>(networkRecord);
            break;
        }
    }
    ReleaseSRWLockExclusive(&g_ikoraActorLock);
}

void remember_ikora_behavior(std::uint32_t object,
                             const std::byte* behaviorActor) noexcept {
    const std::uint64_t now = GetTickCount64();
    AcquireSRWLockExclusive(&g_ikoraActorLock);
    for (std::size_t index = 0; index < g_ikoraActorCount; ++index) {
        IkoraActor& actor = g_ikoraActors[index];
        if (actor.object == object && !actor.released) {
            actor.behaviorActor = reinterpret_cast<std::uintptr_t>(behaviorActor);
            actor.lastBehaviorAtMs = now;
            ++actor.behaviorUpdates;
        }
    }
    ReleaseSRWLockExclusive(&g_ikoraActorLock);
}

void remember_ikora_behavior_tick(const std::byte* behaviorActor) noexcept {
    if (behaviorActor == nullptr) {
        return;
    }
    const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(behaviorActor);
    const std::uint64_t now = GetTickCount64();
    AcquireSRWLockExclusive(&g_ikoraActorLock);
    for (std::size_t index = 0; index < g_ikoraActorCount; ++index) {
        IkoraActor& actor = g_ikoraActors[index];
        if (actor.behaviorActor == address && !actor.released) {
            actor.lastBehaviorAtMs = now;
            ++actor.behaviorUpdates;
        }
    }
    ReleaseSRWLockExclusive(&g_ikoraActorLock);
}

void remember_ikora_release(std::uint32_t object) noexcept {
    const std::uint64_t now = GetTickCount64();
    AcquireSRWLockExclusive(&g_ikoraActorLock);
    for (std::size_t index = 0; index < g_ikoraActorCount; ++index) {
        IkoraActor& actor = g_ikoraActors[index];
        if (actor.object == object) {
            actor.released = true;
            actor.releasedAtMs = now;
        }
    }
    ReleaseSRWLockExclusive(&g_ikoraActorLock);
}

[[nodiscard]] std::uint32_t ikora_factory_sequence(std::uint32_t object) noexcept {
    std::uint32_t sequence = 0U;
    AcquireSRWLockShared(&g_ikoraActorLock);
    for (std::size_t index = 0; index < g_ikoraActorCount; ++index) {
        const IkoraActor& actor = g_ikoraActors[index];
        if (actor.object == object) {
            sequence = actor.factorySequence;
        }
    }
    ReleaseSRWLockShared(&g_ikoraActorLock);
    return sequence;
}

[[nodiscard]] std::uint32_t ikora_factory_object(std::uint32_t factorySequence) noexcept {
    std::uint32_t object = kInvalidHandle;
    AcquireSRWLockShared(&g_ikoraActorLock);
    for (std::size_t index = 0; index < g_ikoraActorCount; ++index) {
        const IkoraActor& actor = g_ikoraActors[index];
        if (actor.factorySequence == factorySequence) {
            object = actor.object;
            break;
        }
    }
    ReleaseSRWLockShared(&g_ikoraActorLock);
    return object;
}

/** Returns one stable actor snapshot so transform-map logging never races a handoff update. */
[[nodiscard]] IkoraActor ikora_factory_actor(std::uint32_t factorySequence) noexcept {
    IkoraActor result{};
    result.factorySequence = factorySequence;
    AcquireSRWLockShared(&g_ikoraActorLock);
    for (std::size_t index = 0U; index < g_ikoraActorCount; ++index) {
        if (g_ikoraActors[index].factorySequence == factorySequence) {
            result = g_ikoraActors[index];
            break;
        }
    }
    ReleaseSRWLockShared(&g_ikoraActorLock);
    return result;
}

/**
 * Maps a native pointer back to the exact captured Ikora component range that owns it.
 * This is stronger than scanning for matching handle values: it proves whether the visual
 * entry or its attachment record physically belongs to Actor 1, 2, or 3.
 */
[[nodiscard]] IkoraComponentAddressOwner locate_ikora_component_address(
    const void* address) noexcept {
    IkoraComponentAddressOwner result{};
    const std::uintptr_t target = reinterpret_cast<std::uintptr_t>(address);
    if (target == 0U) {
        return result;
    }

    AcquireSRWLockShared(&g_ikoraComponentLock);
    for (std::size_t index = 0U; index < g_ikoraComponentCount; ++index) {
        const IkoraComponent& component = g_ikoraComponents[index];
        if (component.instance == 0U || component.size == 0U
            || component.size > 0x10000U
            || component.instance > UINTPTR_MAX - component.size) {
            continue;
        }
        const std::uintptr_t end = component.instance + component.size;
        if (target < component.instance || target >= end) {
            continue;
        }
        result.factorySequence = component.factorySequence;
        result.object = component.object;
        result.definition = component.definition;
        result.instance = component.instance;
        result.size = component.size;
        result.offset = static_cast<std::size_t>(target - component.instance);
        result.exactInstance = target == component.instance;
        result.found = true;
        break;
    }
    ReleaseSRWLockShared(&g_ikoraComponentLock);
    return result;
}

/**
 * Finds one exact readable component captured for a current factory actor.
 * Duplicate or stale records fail closed so an A/B never guesses between component instances.
 * The captured definition is immutable recorder metadata; the first live component word is not a
 * definition tag after the native start handler initializes the component.
 */
[[nodiscard]] bool find_unique_live_ikora_component(
    std::uint32_t factorySequence,
    std::uint32_t object,
    std::uint32_t definition,
    std::size_t minimumSize,
    IkoraComponent& result) noexcept {
    result = {};
    std::size_t matches = 0U;
    AcquireSRWLockShared(&g_ikoraComponentLock);
    for (std::size_t index = 0U; index < g_ikoraComponentCount; ++index) {
        const IkoraComponent& component = g_ikoraComponents[index];
        if (component.factorySequence != factorySequence || component.object != object
            || component.definition != definition) {
            continue;
        }
        ++matches;
        result = component;
    }
    ReleaseSRWLockShared(&g_ikoraComponentLock);
    if (matches != 1U || result.instance == 0U || result.size < minimumSize
        || result.size > 0x10000U || result.instance > UINTPTR_MAX - result.size
        || !readable_memory_range(reinterpret_cast<const void*>(result.instance),
                                  result.size)) {
        result = {};
        return false;
    }
    return true;
}

/**
 * Finds the one provider record in a live component that resolves to the active provider target.
 *
 * The provider is datum-backed and its component-relative placement changed from +0x9F0 to
 * +0x210 across two clean runs even though the component definition and size stayed identical.
 * Treat the resolved target pair as the invariant and scan only the bounded live component.
 * A return value of 2 means multiple matches; the first and second offsets are preserved for a
 * conclusive log, but callers must fail closed for every value but 1.
 */
[[nodiscard]] std::size_t find_matching_ikora_provider_record(
    const IkoraComponent& component,
    std::uint32_t targetHandle,
    std::uintptr_t targetRecord,
    std::int64_t targetRelative,
    std::uintptr_t target,
    IkoraProviderRecordMatch& result,
    std::size_t& alternateOffset) noexcept {
    result = {};
    alternateOffset = 0U;
    if (component.instance == 0U
        || component.size < kIkoraTransformProviderProbeBytes
        || component.size > 0x10000U
        || component.instance > UINTPTR_MAX - component.size
        || targetHandle == kInvalidHandle || targetRecord == 0U
        || targetRelative == INT64_MIN || targetRelative == 0 || target == 0U) {
        return 0U;
    }

    const std::size_t maximumOffset =
        component.size - kIkoraTransformProviderProbeBytes;
    std::size_t matches = 0U;
    __try {
        const auto* const bytes =
            reinterpret_cast<const std::byte*>(component.instance);
        for (std::size_t offset = 0U; offset <= maximumOffset;
             offset += kIkoraTransformProviderScanAlignment) {
            std::uint32_t candidateHandle = kInvalidHandle;
            std::int64_t candidateRelative = INT64_MIN;
            std::memcpy(&candidateHandle,
                        bytes + offset,
                        sizeof candidateHandle);
            std::memcpy(&candidateRelative,
                        bytes + offset + 0x08U,
                        sizeof candidateRelative);
            if (candidateHandle != targetHandle
                || candidateRelative != targetRelative) {
                continue;
            }

            const std::uintptr_t candidateRecord =
                reinterpret_cast<std::uintptr_t>(
                    scene_object_record(candidateHandle));
            const std::uintptr_t candidateTarget =
                add_relative(candidateRecord, candidateRelative);
            if (candidateRecord != targetRecord || candidateTarget != target) {
                continue;
            }

            ++matches;
            if (matches != 1U) {
                alternateOffset = offset;
                return 2U;
            }
            result.provider = component.instance + offset;
            result.offset = offset;
            result.targetHandle = candidateHandle;
            result.targetRecord = candidateRecord;
            result.targetRelative = candidateRelative;
            result.target = candidateTarget;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        result = {};
        return 0U;
    }
    if (matches != 1U) {
        result = {};
    }
    return matches;
}

/**
 * Once the exact VFX row has exposed its provider schema, locate the equivalent provider record
 * in the two real animation actors. Scene 2 is deliberately excluded: it is the VFX carrier and
 * broken visual control, never a valid pose target.
 */
void report_scene_pose_provider_targets() noexcept {
    const std::uint32_t targetHandle =
        g_scenePoseProviderTargetHandle.load(std::memory_order_acquire);
    const std::uintptr_t targetRecord =
        g_scenePoseProviderTargetRecord.load(std::memory_order_acquire);
    const std::int64_t targetRelative =
        g_scenePoseProviderTargetRelative.load(std::memory_order_acquire);
    const std::uintptr_t target =
        g_scenePoseProviderTarget.load(std::memory_order_acquire);
    if (targetHandle == kInvalidHandle || targetRecord == 0U
        || targetRelative == INT64_MIN || targetRelative == 0 || target == 0U) {
        return;
    }

    constexpr std::array<std::uint32_t, 2U> kRealAnimationFactories{1U, 3U};
    constexpr std::array<const char*, 2U> kRoles{
        "scene1_real_animation", "scene3_real_animation"};
    for (std::size_t roleIndex = 0U; roleIndex < kRealAnimationFactories.size();
         ++roleIndex) {
        const std::uint32_t factorySequence = kRealAnimationFactories[roleIndex];
        const std::uint32_t bit = 1U << (factorySequence - 1U);
        if ((g_scenePoseProviderTargetResolvedMask.load(std::memory_order_acquire) & bit)
            != 0U) {
            continue;
        }

        const IkoraActor actor = ikora_factory_actor(factorySequence);
        const bool actorCurrent = !actor.released
                                  && object_handle_is_current(actor.object);
        if (!actorCurrent) {
            continue;
        }

        IkoraComponent component{};
        const bool componentFound = find_unique_live_ikora_component(
            factorySequence,
            actor.object,
            kIkoraTransformProviderComponentDefinition,
            kIkoraTransformProviderProbeBytes,
            component);
        IkoraProviderRecordMatch match{};
        std::size_t alternateOffset = 0U;
        const std::size_t matches = componentFound
                                        ? find_matching_ikora_provider_record(
                                              component,
                                              targetHandle,
                                              targetRecord,
                                              targetRelative,
                                              target,
                                              match,
                                              alternateOffset)
                                        : 0U;
        const bool firstObservation =
            (g_scenePoseProviderTargetObservedMask.fetch_or(
                 bit, std::memory_order_acq_rel)
             & bit)
            == 0U;
        if (matches == 1U) {
            g_scenePoseProviderTargetResolvedMask.fetch_or(
                bit, std::memory_order_acq_rel);
        }
        if (!firstObservation && matches != 1U) {
            continue;
        }

        const IkoraComponentAddressOwner providerOwner =
            locate_ikora_component_address(
                reinterpret_cast<const void*>(match.provider));
        report("ev=omega_scene_pose_binding stage=provider_target role=%s "
               "factory_n=%u actor=%08X actor_current=%s component_found=%s "
               "component=%p component_size=0x%llX matches=%zu provider=%p "
               "provider_offset=0x%llX alternate_offset=0x%llX "
               "provider_owner_found=%s provider_owner_factory=%u "
               "provider_owner_definition=%08X target_handle=%08X "
               "target_record=%p target_relative=%lld target=%p "
               "binding_key=%08X mutation=observe_only",
               kRoles[roleIndex],
               factorySequence,
               actor.object,
               actorCurrent ? "yes" : "no",
               componentFound ? "yes" : "no",
               reinterpret_cast<void*>(component.instance),
               static_cast<unsigned long long>(component.size),
               matches,
               reinterpret_cast<void*>(match.provider),
               static_cast<unsigned long long>(match.offset),
               static_cast<unsigned long long>(alternateOffset),
               providerOwner.found ? "yes" : "no",
               providerOwner.factorySequence,
               providerOwner.definition,
               targetHandle,
               reinterpret_cast<void*>(targetRecord),
               static_cast<long long>(targetRelative),
               reinterpret_cast<void*>(target),
               g_scenePoseBindingKey.load(std::memory_order_acquire));
    }
}

/**
 * Collects bounded, read-only ownership evidence from one factory actor's captured component
 * graph. Exact effect references establish which graph owns the two purple authored assets;
 * exact object/controller references expose cross-binding without assigning meaning to unknown
 * object-record flags. Component sizes come from the validated pre-start header, preventing the
 * out-of-bounds aliases that invalidated the earlier provenance scan.
 */
[[nodiscard]] IkoraOwnershipEvidence collect_ikora_ownership_evidence(
    const IkoraActor& actor,
    const std::array<IkoraActor, kMaximumIkoraActors>& actors,
    std::size_t actorCount) noexcept {
    IkoraOwnershipEvidence evidence{};
    if (actor.factorySequence == 0U) {
        return evidence;
    }

    std::array<IkoraComponent, kMaximumIkoraComponents> components{};
    std::size_t componentCount = 0U;
    AcquireSRWLockShared(&g_ikoraComponentLock);
    for (std::size_t index = 0U; index < g_ikoraComponentCount; ++index) {
        if (g_ikoraComponents[index].factorySequence == actor.factorySequence
            && componentCount < components.size()) {
            components[componentCount++] = g_ikoraComponents[index];
        }
    }
    ReleaseSRWLockShared(&g_ikoraComponentLock);
    evidence.componentCount = componentCount;

    constexpr std::uint32_t kEffectOne = 0x80B9FDBEU;
    constexpr std::uint32_t kEffectTwo = 0x80C220EBU;
    for (std::size_t index = 0U; index < componentCount; ++index) {
        const IkoraComponent& component = components[index];
        if (component.instance == 0U || component.size < sizeof(std::uint32_t)
            || component.size > 0x10000U) {
            continue;
        }
        if (!readable_memory_range(reinterpret_cast<const void*>(component.instance),
                                   component.size)) {
            continue;
        }
        ++evidence.liveComponentCount;
        evidence.scannedBytes += component.size;

        for (std::size_t offset = 0U;
             offset + sizeof(std::uint32_t) <= component.size;
             offset += alignof(std::uint32_t)) {
            const std::uint32_t value = safe_read<std::uint32_t>(
                reinterpret_cast<const void*>(component.instance + offset), 0U);
            if (value == kEffectOne) {
                ++evidence.effect80B9FDBERefs;
                if (evidence.firstEffect80B9FDBEComponent == 0U) {
                    evidence.firstEffect80B9FDBEComponent = component.instance;
                    evidence.firstEffect80B9FDBEOffset = offset;
                }
            }
            if (value == kEffectTwo) {
                ++evidence.effect80C220EBRefs;
                if (evidence.firstEffect80C220EBComponent == 0U) {
                    evidence.firstEffect80C220EBComponent = component.instance;
                    evidence.firstEffect80C220EBOffset = offset;
                }
            }
            if (actor.object != kInvalidHandle && value == actor.object) {
                ++evidence.selfObjectRefs;
            }
            for (std::size_t actorIndex = 0U; actorIndex < actorCount; ++actorIndex) {
                const std::uint32_t sequence = actors[actorIndex].factorySequence;
                if (sequence >= 1U && sequence <= 3U
                    && actors[actorIndex].object != kInvalidHandle
                    && value == actors[actorIndex].object) {
                    ++evidence.actorObjectRefs[sequence - 1U];
                }
            }
        }

        if (actor.behaviorActor != 0U) {
            for (std::size_t offset = 0U;
                 offset + sizeof(std::uintptr_t) <= component.size;
                 offset += alignof(std::uintptr_t)) {
                const std::uintptr_t value = safe_read<std::uintptr_t>(
                    reinterpret_cast<const void*>(component.instance + offset), 0U);
                if (value == actor.behaviorActor) {
                    ++evidence.behaviorActorRefs;
                }
            }
        }
    }
    return evidence;
}

struct BindingOffsetList final {
    std::array<std::uint16_t, kMaximumBindingOffsets> offsets{};
    std::size_t count{};
    std::size_t total{};
};

template <typename Value>
[[nodiscard]] BindingOffsetList find_binding_offsets(
    const std::array<std::byte, kHandoffSnapshotBytes>& bytes,
    Value value) noexcept {
    BindingOffsetList result{};
    const std::uint64_t comparable = static_cast<std::uint64_t>(value);
    if (comparable == 0U
        || (sizeof(Value) == sizeof(std::uint32_t)
            && comparable == static_cast<std::uint64_t>(kInvalidHandle))) {
        return result;
    }
    for (std::size_t offset = 0U; offset + sizeof(Value) <= bytes.size(); ++offset) {
        Value candidate{};
        std::memcpy(&candidate, bytes.data() + offset, sizeof candidate);
        if (candidate != value) {
            continue;
        }
        if (result.count < result.offsets.size()) {
            result.offsets[result.count++] = static_cast<std::uint16_t>(offset);
        }
        ++result.total;
    }
    return result;
}

[[nodiscard]] BindingOffsetList find_changed_offsets(
    const std::array<std::byte, kHandoffSnapshotBytes>& previous,
    const std::array<std::byte, kHandoffSnapshotBytes>& last) noexcept {
    BindingOffsetList result{};
    for (std::size_t offset = 0U; offset < previous.size(); ++offset) {
        if (previous[offset] == last[offset]) {
            continue;
        }
        if (result.count < result.offsets.size()) {
            result.offsets[result.count++] = static_cast<std::uint16_t>(offset);
        }
        ++result.total;
    }
    return result;
}

void format_binding_offsets(const BindingOffsetList& offsets,
                            char* destination,
                            std::size_t capacity) noexcept {
    if (destination == nullptr || capacity == 0U) {
        return;
    }
    if (offsets.total == 0U) {
        (void)std::snprintf(destination, capacity, "none");
        return;
    }
    std::size_t used = 0U;
    for (std::size_t index = 0U; index < offsets.count && used < capacity; ++index) {
        const int written = std::snprintf(destination + used,
                                          capacity - used,
                                          "%s%03X",
                                          index == 0U ? "" : ",",
                                          static_cast<unsigned>(offsets.offsets[index]));
        if (written <= 0 || static_cast<std::size_t>(written) >= capacity - used) {
            destination[capacity - 1U] = '\0';
            return;
        }
        used += static_cast<std::size_t>(written);
    }
    if (offsets.total > offsets.count && used < capacity) {
        (void)std::snprintf(destination + used,
                            capacity - used,
                            ",+%zu",
                            offsets.total - offsets.count);
    }
}

void report_handoff_binding_evidence(
    const char* moment,
    const HandoffComponentSnapshot& snapshot) noexcept {
    if (moment == nullptr || !snapshot.hasPrevious) {
        return;
    }

    const BindingOffsetList changed = find_changed_offsets(snapshot.previous, snapshot.last);
    std::array<char, 128> changedText{};
    format_binding_offsets(changed, changedText.data(), changedText.size());
    report("ev=omega_scene_binding stage=component_delta moment=%s factory_n=%u "
           "object=%08X component=%p definition=%08X handler=+%llX changes=%u "
           "changed_bytes=%zu changed_offsets=%s previous_phase=%s last_phase=%s "
           "mutation=observe_only",
           moment,
           snapshot.factorySequence,
           snapshot.object,
           reinterpret_cast<void*>(snapshot.instance),
           snapshot.definition,
           static_cast<unsigned long long>(snapshot.handlerRva),
           snapshot.changes,
           changed.total,
           changedText.data(),
           snapshot.previousPhase == 0U ? "before_behavior" : "after_behavior",
           snapshot.lastPhase == 0U ? "before_behavior" : "after_behavior");

    std::array<IkoraActor, kMaximumIkoraActors> actors{};
    std::size_t actorCount = 0U;
    AcquireSRWLockShared(&g_ikoraActorLock);
    actorCount = g_ikoraActorCount;
    std::copy_n(g_ikoraActors.begin(), actorCount, actors.begin());
    ReleaseSRWLockShared(&g_ikoraActorLock);

    for (std::size_t index = 0U; index < actorCount; ++index) {
        const IkoraActor& actor = actors[index];
        if (actor.factorySequence < 1U || actor.factorySequence > 3U) {
            continue;
        }
        const BindingOffsetList previousObject =
            find_binding_offsets(snapshot.previous, actor.object);
        const BindingOffsetList lastObject = find_binding_offsets(snapshot.last, actor.object);
        const BindingOffsetList previousEntity =
            find_binding_offsets(snapshot.previous, actor.entity);
        const BindingOffsetList lastEntity = find_binding_offsets(snapshot.last, actor.entity);
        const BindingOffsetList previousBehavior =
            find_binding_offsets(snapshot.previous, actor.behaviorActor);
        const BindingOffsetList lastBehavior =
            find_binding_offsets(snapshot.last, actor.behaviorActor);
        if (previousObject.total == 0U && lastObject.total == 0U
            && previousEntity.total == 0U && lastEntity.total == 0U
            && previousBehavior.total == 0U && lastBehavior.total == 0U) {
            continue;
        }

        std::array<char, 128> previousObjectText{};
        std::array<char, 128> lastObjectText{};
        std::array<char, 128> previousEntityText{};
        std::array<char, 128> lastEntityText{};
        std::array<char, 128> previousBehaviorText{};
        std::array<char, 128> lastBehaviorText{};
        format_binding_offsets(previousObject,
                               previousObjectText.data(),
                               previousObjectText.size());
        format_binding_offsets(lastObject, lastObjectText.data(), lastObjectText.size());
        format_binding_offsets(previousEntity,
                               previousEntityText.data(),
                               previousEntityText.size());
        format_binding_offsets(lastEntity, lastEntityText.data(), lastEntityText.size());
        format_binding_offsets(previousBehavior,
                               previousBehaviorText.data(),
                               previousBehaviorText.size());
        format_binding_offsets(lastBehavior,
                               lastBehaviorText.data(),
                               lastBehaviorText.size());
        report("ev=omega_scene_binding stage=actor_reference moment=%s component_factory=%u "
               "component=%p definition=%08X target_factory=%u target_object=%08X "
               "target_entity=%08X target_behavior=%p previous_object_at=%s last_object_at=%s "
               "previous_entity_at=%s last_entity_at=%s previous_behavior_at=%s "
               "last_behavior_at=%s mutation=observe_only",
               moment,
               snapshot.factorySequence,
               reinterpret_cast<void*>(snapshot.instance),
               snapshot.definition,
               actor.factorySequence,
               actor.object,
               actor.entity,
               reinterpret_cast<void*>(actor.behaviorActor),
               previousObjectText.data(),
               lastObjectText.data(),
               previousEntityText.data(),
               lastEntityText.data(),
               previousBehaviorText.data(),
               lastBehaviorText.data());
    }
}

void capture_scene_handoff_components(std::uint32_t triggerFactorySequence,
                                      std::uint32_t phase) noexcept {
    if (triggerFactorySequence == 0U) {
        return;
    }
    const std::uint64_t now = GetTickCount64();
    std::atomic_uint64_t& lastTick = phase == 0U
                                        ? g_handoffRollingBeforeLastTick
                                        : g_handoffRollingAfterLastTick;
    std::uint64_t previousTick = lastTick.load(std::memory_order_relaxed);
    if (previousTick != 0U && now - previousTick < kHandoffRollingSampleMs) {
        return;
    }
    if (!lastTick.compare_exchange_strong(
            previousTick, now, std::memory_order_acq_rel)) {
        return;
    }

    std::array<IkoraComponent, kMaximumHandoffComponents> components{};
    std::size_t componentCount = 0U;
    AcquireSRWLockShared(&g_ikoraComponentLock);
    for (std::size_t index = 0U; index < g_ikoraComponentCount; ++index) {
        const std::uint32_t componentFactory =
            g_ikoraComponents[index].factorySequence;
        if (componentFactory >= 1U && componentFactory <= 3U
            && componentCount < components.size()) {
            components[componentCount++] = g_ikoraComponents[index];
        }
    }
    ReleaseSRWLockShared(&g_ikoraComponentLock);

    std::uint32_t validComponents = 0U;
    for (std::size_t index = 0U; index < componentCount; ++index) {
        const IkoraComponent& component = components[index];
        std::array<std::byte, kHandoffSnapshotBytes> bytes{};
        if (!safe_copy(bytes.data(),
                       reinterpret_cast<const void*>(component.instance),
                       bytes.size())) {
            continue;
        }
        // ComponentStart hands us the component's data instance, not a header whose first dword
        // repeats the authored definition. The old equality check discarded every live component
        // and made the actor-reference delta recorder silently empty. The definition was already
        // captured from the validated ComponentStart entry and remains stable in IkoraComponent.
        ++validComponents;
        const std::uint64_t hash = hash_region(bytes.data(), bytes.size());

        AcquireSRWLockExclusive(&g_handoffSnapshotLock);
        HandoffComponentSnapshot* target = nullptr;
        for (std::size_t snapshotIndex = 0U;
             snapshotIndex < g_handoffSnapshotCount;
             ++snapshotIndex) {
            if (g_handoffSnapshots[snapshotIndex].instance == component.instance) {
                target = &g_handoffSnapshots[snapshotIndex];
                break;
            }
        }
        if (target == nullptr && g_handoffSnapshotCount < g_handoffSnapshots.size()) {
            target = &g_handoffSnapshots[g_handoffSnapshotCount++];
            target->instance = component.instance;
            target->handlerRva = component.handlerRva;
            target->definition = component.definition;
            target->factorySequence = component.factorySequence;
            target->object = component.object;
        }
        if (target != nullptr) {
            ++target->captures;
            target->lastObservedAtMs = now;
            if (!target->valid || target->lastHash != hash) {
                if (target->valid) {
                    target->previous = target->last;
                    target->previousHash = target->lastHash;
                    target->previousChangedAtMs = target->lastChangedAtMs;
                    target->previousPhase = target->lastPhase;
                    target->hasPrevious = true;
                }
                target->last = bytes;
                target->lastHash = hash;
                target->lastChangedAtMs = now;
                target->lastPhase = phase;
                target->valid = true;
                ++target->changes;
            }
        }
        ReleaseSRWLockExclusive(&g_handoffSnapshotLock);
    }
    const std::uint32_t pass =
        g_handoffRollingPasses.fetch_add(1U, std::memory_order_relaxed) + 1U;
    if (pass == 1U || pass % 300U == 0U) {
        report("ev=omega_ikora_handoff stage=rolling_capture pass=%u trigger_factory_n=%u "
               "tracked_factories=1,2,3 "
               "phase=%s tracked=%zu valid=%u sample_ms=%llu mutation=observe_only",
               pass,
               triggerFactorySequence,
               phase == 0U ? "before_behavior" : "after_behavior",
               componentCount,
               validComponents,
               static_cast<unsigned long long>(now));
    }
}

[[nodiscard]] std::uint64_t snapshot_qword(
    const std::array<std::byte, kHandoffSnapshotBytes>& bytes,
    std::size_t index) noexcept {
    std::uint64_t value = 0U;
    if (index < bytes.size() / sizeof value) {
        std::memcpy(&value, bytes.data() + index * sizeof value, sizeof value);
    }
    return value;
}

void report_handoff_component_bytes(
    const char* moment,
    const char* version,
    const HandoffComponentSnapshot& snapshot,
    const std::array<std::byte, kHandoffSnapshotBytes>& bytes,
    std::uint64_t hash,
    std::uint64_t changedAtMs,
    std::uint32_t phase) noexcept {
    report("ev=omega_ikora_handoff stage=rolling_bytes moment=%s version=%s "
           "factory_n=%u object=%08X component=%p definition=%08X handler=+%llX "
           "hash=%016llX changed_at_ms=%llu phase=%s half=0 "
           "raw=%016llX,%016llX,%016llX,%016llX,%016llX,%016llX,%016llX,%016llX,"
           "%016llX,%016llX,%016llX,%016llX,%016llX,%016llX,%016llX,%016llX "
           "mutation=observe_only",
           moment,
           version,
           snapshot.factorySequence,
           snapshot.object,
           reinterpret_cast<void*>(snapshot.instance),
           snapshot.definition,
           static_cast<unsigned long long>(snapshot.handlerRva),
           static_cast<unsigned long long>(hash),
           static_cast<unsigned long long>(changedAtMs),
           phase == 0U ? "before_behavior" : "after_behavior",
           static_cast<unsigned long long>(snapshot_qword(bytes, 0U)),
           static_cast<unsigned long long>(snapshot_qword(bytes, 1U)),
           static_cast<unsigned long long>(snapshot_qword(bytes, 2U)),
           static_cast<unsigned long long>(snapshot_qword(bytes, 3U)),
           static_cast<unsigned long long>(snapshot_qword(bytes, 4U)),
           static_cast<unsigned long long>(snapshot_qword(bytes, 5U)),
           static_cast<unsigned long long>(snapshot_qword(bytes, 6U)),
           static_cast<unsigned long long>(snapshot_qword(bytes, 7U)),
           static_cast<unsigned long long>(snapshot_qword(bytes, 8U)),
           static_cast<unsigned long long>(snapshot_qword(bytes, 9U)),
           static_cast<unsigned long long>(snapshot_qword(bytes, 10U)),
           static_cast<unsigned long long>(snapshot_qword(bytes, 11U)),
           static_cast<unsigned long long>(snapshot_qword(bytes, 12U)),
           static_cast<unsigned long long>(snapshot_qword(bytes, 13U)),
           static_cast<unsigned long long>(snapshot_qword(bytes, 14U)),
           static_cast<unsigned long long>(snapshot_qword(bytes, 15U)));
    report("ev=omega_ikora_handoff stage=rolling_bytes moment=%s version=%s "
           "factory_n=%u object=%08X component=%p definition=%08X handler=+%llX "
           "hash=%016llX changed_at_ms=%llu phase=%s half=1 "
           "raw=%016llX,%016llX,%016llX,%016llX,%016llX,%016llX,%016llX,%016llX,"
           "%016llX,%016llX,%016llX,%016llX,%016llX,%016llX,%016llX,%016llX "
           "mutation=observe_only",
           moment,
           version,
           snapshot.factorySequence,
           snapshot.object,
           reinterpret_cast<void*>(snapshot.instance),
           snapshot.definition,
           static_cast<unsigned long long>(snapshot.handlerRva),
           static_cast<unsigned long long>(hash),
           static_cast<unsigned long long>(changedAtMs),
           phase == 0U ? "before_behavior" : "after_behavior",
           static_cast<unsigned long long>(snapshot_qword(bytes, 16U)),
           static_cast<unsigned long long>(snapshot_qword(bytes, 17U)),
           static_cast<unsigned long long>(snapshot_qword(bytes, 18U)),
           static_cast<unsigned long long>(snapshot_qword(bytes, 19U)),
           static_cast<unsigned long long>(snapshot_qword(bytes, 20U)),
           static_cast<unsigned long long>(snapshot_qword(bytes, 21U)),
           static_cast<unsigned long long>(snapshot_qword(bytes, 22U)),
           static_cast<unsigned long long>(snapshot_qword(bytes, 23U)),
           static_cast<unsigned long long>(snapshot_qword(bytes, 24U)),
           static_cast<unsigned long long>(snapshot_qword(bytes, 25U)),
           static_cast<unsigned long long>(snapshot_qword(bytes, 26U)),
           static_cast<unsigned long long>(snapshot_qword(bytes, 27U)),
           static_cast<unsigned long long>(snapshot_qword(bytes, 28U)),
           static_cast<unsigned long long>(snapshot_qword(bytes, 29U)),
           static_cast<unsigned long long>(snapshot_qword(bytes, 30U)),
           static_cast<unsigned long long>(snapshot_qword(bytes, 31U)));
}

void report_scene_handoff_rolling_snapshot(const char* moment,
                                           std::uint32_t factorySequence) noexcept {
    if (moment == nullptr || factorySequence == 0U) {
        return;
    }
    std::array<HandoffComponentSnapshot, kMaximumHandoffComponents> snapshots{};
    std::size_t snapshotCount = 0U;
    AcquireSRWLockShared(&g_handoffSnapshotLock);
    for (std::size_t index = 0U; index < g_handoffSnapshotCount; ++index) {
        if (g_handoffSnapshots[index].factorySequence == factorySequence
            && snapshotCount < snapshots.size()) {
            snapshots[snapshotCount++] = g_handoffSnapshots[index];
        }
    }
    ReleaseSRWLockShared(&g_handoffSnapshotLock);

    const std::uint64_t now = GetTickCount64();
    report("ev=omega_ikora_handoff stage=rolling_summary moment=%s factory_n=%u "
           "passes=%u components=%zu now_ms=%llu mutation=observe_only",
           moment,
           factorySequence,
           g_handoffRollingPasses.load(std::memory_order_acquire),
           snapshotCount,
           static_cast<unsigned long long>(now));
    for (std::size_t index = 0U; index < snapshotCount; ++index) {
        const HandoffComponentSnapshot& snapshot = snapshots[index];
        if (!snapshot.valid) {
            continue;
        }
        report("ev=omega_ikora_handoff stage=rolling_component moment=%s "
               "component_n=%zu factory_n=%u object=%08X component=%p definition=%08X "
               "handler=+%llX captures=%u changes=%u last_observed_at_ms=%llu "
               "last_changed_at_ms=%llu last_phase=%s age_ms=%llu has_previous=%u "
               "mutation=observe_only",
               moment,
               index + 1U,
               snapshot.factorySequence,
               snapshot.object,
               reinterpret_cast<void*>(snapshot.instance),
               snapshot.definition,
               static_cast<unsigned long long>(snapshot.handlerRva),
               snapshot.captures,
               snapshot.changes,
               static_cast<unsigned long long>(snapshot.lastObservedAtMs),
               static_cast<unsigned long long>(snapshot.lastChangedAtMs),
               snapshot.lastPhase == 0U ? "before_behavior" : "after_behavior",
               static_cast<unsigned long long>(now - snapshot.lastObservedAtMs),
               snapshot.hasPrevious ? 1U : 0U);
        if (snapshot.hasPrevious) {
            report_handoff_binding_evidence(moment, snapshot);
            report_handoff_component_bytes(moment,
                                           "previous",
                                           snapshot,
                                           snapshot.previous,
                                           snapshot.previousHash,
                                           snapshot.previousChangedAtMs,
                                           snapshot.previousPhase);
        }
        report_handoff_component_bytes(moment,
                                       "last",
                                       snapshot,
                                       snapshot.last,
                                       snapshot.lastHash,
                                       snapshot.lastChangedAtMs,
                                       snapshot.lastPhase);
    }
}

void report_ikora_handoff_snapshot(const char* moment,
                                   std::uint32_t factorySequence) noexcept {
    if (moment == nullptr || factorySequence == 0U) {
        return;
    }

    std::array<IkoraActor, kMaximumIkoraActors> actors{};
    std::size_t actorCount = 0U;
    AcquireSRWLockShared(&g_ikoraActorLock);
    actorCount = g_ikoraActorCount;
    std::copy_n(g_ikoraActors.begin(), actorCount, actors.begin());
    ReleaseSRWLockShared(&g_ikoraActorLock);
    for (std::size_t index = 0; index < actorCount; ++index) {
        const IkoraActor& actor = actors[index];
        if (actor.factorySequence != factorySequence) {
            continue;
        }
        const std::byte* const record = object_record(actor.object);
        report("ev=omega_ikora_handoff stage=actor_snapshot moment=%s factory_n=%u "
               "object=%08X entity=%08X released=%u record=%p behavior_actor=%p "
               "record_raw=%016llX,%016llX,%016llX,%016llX,%016llX,%016llX,"
               "%016llX,%016llX mutation=observe_only",
               moment,
               factorySequence,
               actor.object,
               actor.entity,
               actor.released ? 1U : 0U,
               record,
               reinterpret_cast<void*>(actor.behaviorActor),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(record + 0x00U, 0U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(record + 0x08U, 0U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(record + 0x10U, 0U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(record + 0x18U, 0U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(record + 0x20U, 0U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(record + 0x28U, 0U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(record + 0x30U, 0U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(record + 0x38U, 0U)));
    }

    std::array<IkoraComponent, kMaximumIkoraComponents> components{};
    std::size_t componentCount = 0U;
    AcquireSRWLockShared(&g_ikoraComponentLock);
    for (std::size_t index = 0; index < g_ikoraComponentCount; ++index) {
        if (g_ikoraComponents[index].factorySequence == factorySequence
            && componentCount < components.size()) {
            components[componentCount++] = g_ikoraComponents[index];
        }
    }
    ReleaseSRWLockShared(&g_ikoraComponentLock);

    for (std::size_t index = 0; index < componentCount; ++index) {
        const IkoraComponent& component = components[index];
        const std::uint32_t capturedDefinition = safe_read<std::uint32_t>(
            reinterpret_cast<const void*>(component.instance), kInvalidHandle);
        report("ev=omega_ikora_handoff stage=component_snapshot moment=%s factory_n=%u "
               "component_n=%zu object=%08X component=%p definition=%08X handler=+%llX "
               "header_valid=%u captured_definition=%08X hash=%016llX "
               "raw=%016llX,%016llX,%016llX,%016llX,%016llX,%016llX,"
               "%016llX,%016llX,%016llX,%016llX,%016llX,%016llX,%016llX,%016llX,"
               "%016llX,%016llX mutation=observe_only",
               moment,
               factorySequence,
               index + 1U,
               component.object,
               reinterpret_cast<void*>(component.instance),
               component.definition,
               static_cast<unsigned long long>(component.handlerRva),
               capturedDefinition == component.definition ? 1U : 0U,
               capturedDefinition,
               static_cast<unsigned long long>(hash_region(
                   reinterpret_cast<const void*>(component.instance), 0x100U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(
                   reinterpret_cast<const void*>(component.instance + 0x00U), 0U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(
                   reinterpret_cast<const void*>(component.instance + 0x08U), 0U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(
                   reinterpret_cast<const void*>(component.instance + 0x10U), 0U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(
                   reinterpret_cast<const void*>(component.instance + 0x18U), 0U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(
                   reinterpret_cast<const void*>(component.instance + 0x20U), 0U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(
                   reinterpret_cast<const void*>(component.instance + 0x28U), 0U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(
                   reinterpret_cast<const void*>(component.instance + 0x30U), 0U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(
                   reinterpret_cast<const void*>(component.instance + 0x38U), 0U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(
                   reinterpret_cast<const void*>(component.instance + 0x40U), 0U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(
                   reinterpret_cast<const void*>(component.instance + 0x48U), 0U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(
                   reinterpret_cast<const void*>(component.instance + 0x50U), 0U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(
                   reinterpret_cast<const void*>(component.instance + 0x58U), 0U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(
                   reinterpret_cast<const void*>(component.instance + 0x60U), 0U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(
                   reinterpret_cast<const void*>(component.instance + 0x68U), 0U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(
                   reinterpret_cast<const void*>(component.instance + 0x70U), 0U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(
                   reinterpret_cast<const void*>(component.instance + 0x78U), 0U)));
    }
}

void report_followup_state() noexcept {
    if (!omega_forced() || g_ikoraObject.load(std::memory_order_acquire) == kInvalidHandle) {
        return;
    }
    const std::uint64_t now = GetTickCount64();
    std::uint64_t previous = g_followupLastTick.load(std::memory_order_relaxed);
    if (previous != 0U && now - previous < kFollowupSampleMs) {
        return;
    }
    if (!g_followupLastTick.compare_exchange_strong(
            previous, now, std::memory_order_acq_rel)) {
        return;
    }

    const std::uint32_t object = g_ikoraObject.load(std::memory_order_acquire);
    const std::uint32_t entity = g_ikoraEntity.load(std::memory_order_acquire);
    const std::byte* const record = object_record(object);
    const auto network = reinterpret_cast<const std::byte*>(
        g_ikoraNetworkRecord.load(std::memory_order_acquire));
    report("ev=omega_ikora_followup stage=entity_state object=%08X entity=%08X "
           "object_record=%p flags4=%08X definition4c=%08X raw80=%08X raw84=%08X "
           "network_record=%p network_entity=%08X network_object=%08X network_flags=%08X "
           "behavior_actor=%p behavior_calls=%u behavior_matches=%u mutation=observe_only",
           object,
           entity,
           record,
           record == nullptr ? 0U : safe_read<std::uint32_t>(record + 0x04U, 0U),
           record == nullptr ? kInvalidHandle
                             : safe_read<std::uint32_t>(record + 0x4CU, kInvalidHandle),
           record == nullptr ? 0U : safe_read<std::uint32_t>(record + 0x80U, 0U),
           record == nullptr ? 0U : safe_read<std::uint32_t>(record + 0x84U, 0U),
           network,
           network == nullptr ? kInvalidHandle
                              : safe_read<std::uint32_t>(network + 0x110U, kInvalidHandle),
           network == nullptr ? kInvalidHandle
                              : safe_read<std::uint32_t>(network + 0x120U, kInvalidHandle),
           network == nullptr ? 0U
                              : safe_read<std::uint32_t>(network + 0x124U, 0U),
           reinterpret_cast<void*>(g_ikoraBehaviorActor.load(std::memory_order_acquire)),
           g_behaviorCalls.load(std::memory_order_acquire),
           g_behaviorMatches.load(std::memory_order_acquire));

    std::array<IkoraActor, kMaximumIkoraActors> actors{};
    std::size_t actorCount = 0U;
    AcquireSRWLockShared(&g_ikoraActorLock);
    actorCount = g_ikoraActorCount;
    std::copy_n(g_ikoraActors.begin(), actorCount, actors.begin());
    ReleaseSRWLockShared(&g_ikoraActorLock);
    for (std::size_t index = 0; index < actorCount; ++index) {
        const IkoraActor& actor = actors[index];
        const std::byte* const actorRecord = object_record(actor.object);
        const DecodedObjectPosition position = decode_object_position(actor.object);
        const bool current = position.current;
        const std::uint32_t parent20 = current
                                           ? safe_read<std::uint32_t>(
                                                 actorRecord == nullptr
                                                     ? nullptr
                                                     : actorRecord + 0x20U,
                                                 kInvalidHandle)
                                           : kInvalidHandle;
        const std::uint32_t parent68 = current
                                           ? safe_read<std::uint32_t>(
                                                 actorRecord == nullptr
                                                     ? nullptr
                                                     : actorRecord + 0x68U,
                                                 kInvalidHandle)
                                           : kInvalidHandle;
        const IkoraOwnershipEvidence ownership = collect_ikora_ownership_evidence(
            actor, actors, actorCount);
        const bool controllerActive = actor.behaviorActor != 0U
                                      && actor.lastBehaviorAtMs != 0U
                                      && now - actor.lastBehaviorAtMs <= 1500U
                                      && !actor.released;
        report("ev=omega_ikora_ownership stage=actor_state factory_n=%u role=%s "
               "caller=+%llX object=%08X current_handle=%08X current=%s entity=%08X "
               "released=%u created_at_ms=%llu released_at_ms=%llu descriptor=%p "
               "object_record=%p flags4=%08X definition4c=%08X parent20=%08X "
               "parent68=%08X raw80=%08X raw84=%08X pos_valid=%s "
               "pos=%.3f,%.3f,%.3f network_record=%p behavior_controller=%p "
               "controller_active=%s behavior_updates=%llu last_behavior_ms=%llu "
               "components=%zu live_components=%zu scanned_bytes=%zu "
               "effect_80B9FDBE_refs=%u effect_80B9FDBE_component=%p "
               "effect_80B9FDBE_offset=%lld effect_80C220EB_refs=%u "
               "effect_80C220EB_component=%p effect_80C220EB_offset=%lld "
               "self_refs=%u actor1_refs=%u actor2_refs=%u actor3_refs=%u "
               "behavior_controller_refs=%u mutation=observe_only",
               actor.factorySequence,
               actor.behaviorActor != 0U ? "scene_performer" : "standin_or_replacement",
               static_cast<unsigned long long>(actor.factoryCallerRva),
               actor.object,
               position.currentHandle,
               current ? "yes" : "no",
               actor.entity,
               actor.released ? 1U : 0U,
               static_cast<unsigned long long>(actor.createdAtMs),
               static_cast<unsigned long long>(actor.releasedAtMs),
               reinterpret_cast<void*>(actor.descriptor),
               actorRecord,
               !current || actorRecord == nullptr ? 0U
                                      : safe_read<std::uint32_t>(actorRecord + 0x04U, 0U),
               !current || actorRecord == nullptr
                   ? kInvalidHandle
                   : safe_read<std::uint32_t>(actorRecord + 0x4CU, kInvalidHandle),
               parent20,
               parent68,
               !current || actorRecord == nullptr ? 0U
                                      : safe_read<std::uint32_t>(actorRecord + 0x80U, 0U),
               !current || actorRecord == nullptr ? 0U
                                      : safe_read<std::uint32_t>(actorRecord + 0x84U, 0U),
               position.valid ? "yes" : "no",
               position.value[0],
               position.value[1],
               position.value[2],
               reinterpret_cast<void*>(actor.networkRecord),
               reinterpret_cast<void*>(actor.behaviorActor),
               controllerActive ? "yes" : "no",
               static_cast<unsigned long long>(actor.behaviorUpdates),
               static_cast<unsigned long long>(actor.lastBehaviorAtMs),
               ownership.componentCount,
               ownership.liveComponentCount,
               ownership.scannedBytes,
               ownership.effect80B9FDBERefs,
               reinterpret_cast<void*>(ownership.firstEffect80B9FDBEComponent),
               ownership.firstEffect80B9FDBEOffset == static_cast<std::size_t>(-1)
                   ? -1LL
                   : static_cast<long long>(ownership.firstEffect80B9FDBEOffset),
               ownership.effect80C220EBRefs,
               reinterpret_cast<void*>(ownership.firstEffect80C220EBComponent),
               ownership.firstEffect80C220EBOffset == static_cast<std::size_t>(-1)
                   ? -1LL
                   : static_cast<long long>(ownership.firstEffect80C220EBOffset),
               ownership.selfObjectRefs,
               ownership.actorObjectRefs[0],
               ownership.actorObjectRefs[1],
               ownership.actorObjectRefs[2],
               ownership.behaviorActorRefs);
    }

    for (std::size_t index = 0; index < kWatchedComponents.size(); ++index) {
        const std::uintptr_t component =
            g_watchedComponentPointers[index].load(std::memory_order_acquire);
        if (component == 0U) {
            continue;
        }
        const std::uint64_t hash = hash_region(reinterpret_cast<const void*>(component), 0x100U);
        const std::uint64_t old =
            g_watchedComponentHashes[index].exchange(hash, std::memory_order_acq_rel);
        if (old == hash) {
            continue;
        }
        report("ev=omega_ikora_followup stage=authored_component_state name=%s "
               "definition=%08X component=%p previous_hash=%016llX hash=%016llX "
               "raw=%016llX,%016llX,%016llX,%016llX mutation=observe_only",
               kWatchedComponents[index].name,
               kWatchedComponents[index].definition,
               reinterpret_cast<void*>(component),
               static_cast<unsigned long long>(old),
               static_cast<unsigned long long>(hash),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(
                   reinterpret_cast<const void*>(component + 0x00U), 0U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(
                   reinterpret_cast<const void*>(component + 0x08U), 0U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(
                   reinterpret_cast<const void*>(component + 0x10U), 0U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(
                   reinterpret_cast<const void*>(component + 0x18U), 0U)));
    }

    std::array<IkoraComponent, kMaximumIkoraComponents> changed{};
    std::size_t changedCount = 0U;
    AcquireSRWLockExclusive(&g_ikoraComponentLock);
    for (std::size_t index = 0; index < g_ikoraComponentCount; ++index) {
        IkoraComponent& component = g_ikoraComponents[index];
        const std::uint64_t hash = hash_region(
            reinterpret_cast<const void*>(component.instance), 0x100U);
        if (hash == component.hash) {
            continue;
        }
        component.hash = hash;
        changed[changedCount++] = component;
    }
    ReleaseSRWLockExclusive(&g_ikoraComponentLock);
    for (std::size_t index = 0; index < changedCount; ++index) {
        const IkoraComponent& component = changed[index];
        report("ev=omega_ikora_followup stage=entity_component_change factory_n=%u "
               "object=%08X component=%p definition=%08X start_handler=+%llX "
               "hash=%016llX raw=%016llX,%016llX,%016llX,%016llX,%016llX,%016llX,"
               "%016llX,%016llX mutation=observe_only",
               component.factorySequence,
               component.object,
               reinterpret_cast<void*>(component.instance),
               component.definition,
               static_cast<unsigned long long>(component.handlerRva),
               static_cast<unsigned long long>(component.hash),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(
                   reinterpret_cast<const void*>(component.instance + 0x00U), 0U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(
                   reinterpret_cast<const void*>(component.instance + 0x08U), 0U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(
                   reinterpret_cast<const void*>(component.instance + 0x10U), 0U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(
                   reinterpret_cast<const void*>(component.instance + 0x18U), 0U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(
                   reinterpret_cast<const void*>(component.instance + 0x20U), 0U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(
                   reinterpret_cast<const void*>(component.instance + 0x28U), 0U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(
                   reinterpret_cast<const void*>(component.instance + 0x30U), 0U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(
                   reinterpret_cast<const void*>(component.instance + 0x38U), 0U)));
    }
    report_scene_pose_provider_targets();
    sample_omega_opening_runtime_state();
}

void report_payload(std::uint32_t sequence,
                    const char* moment,
                    const std::uint32_t* instance,
                    const std::byte* payload) noexcept {
    report(
        "ev=omega_spawner_chain stage=deficit moment=%s n=%u instance=%p datum=0x%08X "
        "payload=%p native_count=%d native_counts=%d,%d,%d,%d,%d,%d generation=%u "
        "target=%08X/%u/%u squad=%08X/%u/%u target_generation=%u active=%u mode=%u "
        "initialized=%u desired=%d,%d,%d,%d,%d,%d pending=%d,%d,%d,%d,%d,%d "
        "secondary=%d,%d,%d,%d,%d,%d ref30=0x%016llX ref38=0x%016llX mutation=observe_only",
        moment,
        sequence,
        instance,
        safe_read<std::uint32_t>(instance, 0xFFFFFFFFU),
        payload,
        safe_read<std::int32_t>(payload + 0x2CU, -1),
        safe_read<std::int32_t>(payload + 0x30U, -1),
        safe_read<std::int32_t>(payload + 0x34U, -1),
        safe_read<std::int32_t>(payload + 0x38U, -1),
        safe_read<std::int32_t>(payload + 0x3CU, -1),
        safe_read<std::int32_t>(payload + 0x40U, -1),
        safe_read<std::int32_t>(payload + 0x44U, -1),
        safe_read<std::uint32_t>(payload + 0x7CU),
        safe_read<std::uint32_t>(payload + 0x90U, kAbsentHash),
        safe_read<std::uint8_t>(payload + 0x94U, 0xFFU),
        safe_read<std::uint16_t>(payload + 0x96U, 0xFFFFU),
        safe_read<std::uint32_t>(payload + 0x98U, kAbsentHash),
        safe_read<std::uint8_t>(payload + 0x9CU, 0xFFU),
        safe_read<std::uint16_t>(payload + 0x9EU, 0xFFFFU),
        safe_read<std::uint32_t>(payload + 0xB8U),
        safe_read<std::uint8_t>(payload + 0xBCU),
        safe_read<std::uint8_t>(payload + 0xBDU),
        safe_read<std::uint8_t>(reinterpret_cast<const std::byte*>(instance) + 0x260U),
        safe_read<std::int32_t>(reinterpret_cast<const std::byte*>(instance) + 0x650U),
        safe_read<std::int32_t>(reinterpret_cast<const std::byte*>(instance) + 0x654U),
        safe_read<std::int32_t>(reinterpret_cast<const std::byte*>(instance) + 0x658U),
        safe_read<std::int32_t>(reinterpret_cast<const std::byte*>(instance) + 0x65CU),
        safe_read<std::int32_t>(reinterpret_cast<const std::byte*>(instance) + 0x660U),
        safe_read<std::int32_t>(reinterpret_cast<const std::byte*>(instance) + 0x664U),
        safe_read<std::int32_t>(reinterpret_cast<const std::byte*>(instance) + 0x268U),
        safe_read<std::int32_t>(reinterpret_cast<const std::byte*>(instance) + 0x26CU),
        safe_read<std::int32_t>(reinterpret_cast<const std::byte*>(instance) + 0x270U),
        safe_read<std::int32_t>(reinterpret_cast<const std::byte*>(instance) + 0x274U),
        safe_read<std::int32_t>(reinterpret_cast<const std::byte*>(instance) + 0x278U),
        safe_read<std::int32_t>(reinterpret_cast<const std::byte*>(instance) + 0x27CU),
        safe_read<std::int32_t>(reinterpret_cast<const std::byte*>(instance) + 0x670U),
        safe_read<std::int32_t>(reinterpret_cast<const std::byte*>(instance) + 0x674U),
        safe_read<std::int32_t>(reinterpret_cast<const std::byte*>(instance) + 0x678U),
        safe_read<std::int32_t>(reinterpret_cast<const std::byte*>(instance) + 0x67CU),
        safe_read<std::int32_t>(reinterpret_cast<const std::byte*>(instance) + 0x680U),
        safe_read<std::int32_t>(reinterpret_cast<const std::byte*>(instance) + 0x684U),
        static_cast<unsigned long long>(safe_read<std::uint64_t>(
            reinterpret_cast<const std::byte*>(instance) + 0x218U)),
        static_cast<unsigned long long>(safe_read<std::uint64_t>(
            reinterpret_cast<const std::byte*>(instance) + 0x220U)));
}

__declspec(noinline) void __fastcall spawner_apply(std::uint32_t* instance,
                                                    std::uint32_t* message) noexcept {
    const SpawnerApply original = g_applyOriginal.load(std::memory_order_acquire);
    if (original == nullptr) {
        return;
    }
    const bool inspect = omega_forced();
    const std::uint32_t sequence = inspect
                                       ? g_applyCalls.fetch_add(1, std::memory_order_relaxed) + 1U
                                       : 0U;
    if (inspect && sequence <= kMaximumApplyLogs) {
        report("ev=omega_spawner_chain stage=apply moment=before n=%u instance=%p datum=0x%08X "
               "message=%p key=0x%08X key_tail=0x%016llX initialized=%u mutation=observe_only",
               sequence,
               instance,
               safe_read<std::uint32_t>(instance, 0xFFFFFFFFU),
               message,
               safe_read<std::uint32_t>(message, 0xFFFFFFFFU),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(message + 2)),
               safe_read<std::uint8_t>(reinterpret_cast<const std::byte*>(instance) + 0x260U));
    }
    original(instance, message);
    if (inspect && sequence <= kMaximumApplyLogs) {
        report("ev=omega_spawner_chain stage=apply moment=after n=%u instance=%p datum=0x%08X "
               "initialized=%u desired=%d,%d,%d,%d,%d,%d pending=%d,%d,%d,%d,%d,%d "
               "mutation=observe_only",
               sequence,
               instance,
               safe_read<std::uint32_t>(instance, 0xFFFFFFFFU),
               safe_read<std::uint8_t>(reinterpret_cast<const std::byte*>(instance) + 0x260U),
               safe_read<std::int32_t>(reinterpret_cast<const std::byte*>(instance) + 0x650U),
               safe_read<std::int32_t>(reinterpret_cast<const std::byte*>(instance) + 0x654U),
               safe_read<std::int32_t>(reinterpret_cast<const std::byte*>(instance) + 0x658U),
               safe_read<std::int32_t>(reinterpret_cast<const std::byte*>(instance) + 0x65CU),
               safe_read<std::int32_t>(reinterpret_cast<const std::byte*>(instance) + 0x660U),
               safe_read<std::int32_t>(reinterpret_cast<const std::byte*>(instance) + 0x664U),
               safe_read<std::int32_t>(reinterpret_cast<const std::byte*>(instance) + 0x268U),
               safe_read<std::int32_t>(reinterpret_cast<const std::byte*>(instance) + 0x26CU),
               safe_read<std::int32_t>(reinterpret_cast<const std::byte*>(instance) + 0x270U),
               safe_read<std::int32_t>(reinterpret_cast<const std::byte*>(instance) + 0x274U),
               safe_read<std::int32_t>(reinterpret_cast<const std::byte*>(instance) + 0x278U),
               safe_read<std::int32_t>(reinterpret_cast<const std::byte*>(instance) + 0x27CU));
    }
}

__declspec(noinline) void __fastcall spawner_deficit(std::uint32_t* instance,
                                                      std::uint32_t reason,
                                                      const std::byte* payload) noexcept {
    const SpawnerDeficit original = g_deficitOriginal.load(std::memory_order_acquire);
    if (original == nullptr) {
        return;
    }
    const bool inspect = omega_forced() && omega_payload(payload);
    std::uint32_t sequence = 0;
    if (inspect) {
        sequence = g_chainCalls.fetch_add(1, std::memory_order_relaxed) + 1U;
        g_omegaChainSequence = sequence;
        ++g_omegaChainDepth;
        if (sequence <= kMaximumChainLogs) {
            report_payload(sequence, "before", instance, payload);
        }
    }
    original(instance, reason, payload);
    if (inspect) {
        if (sequence <= kMaximumChainLogs) {
            report_payload(sequence, "after", instance, payload);
        }
        --g_omegaChainDepth;
        if (g_omegaChainDepth == 0U) {
            g_omegaChainSequence = 0U;
        }
    }
}

__declspec(noinline) void __fastcall spawner_request(std::byte* instance,
                                                      std::uint64_t reason,
                                                      const std::int32_t* request,
                                                      std::int32_t* result) noexcept {
    const SpawnerRequest original = g_requestOriginal.load(std::memory_order_acquire);
    if (original == nullptr) {
        return;
    }
    const bool inspect = g_omegaChainDepth != 0U && g_omegaChainSequence <= kMaximumChainLogs;
    if (inspect) {
        report("ev=omega_spawner_chain stage=request moment=before n=%u instance=%p reason=%llu "
               "request=%p request_count=%d values=%d,%d,%d,%d,%d,%d flags=0x%08X "
               "result_before=%d mutation=observe_only",
               g_omegaChainSequence,
               instance,
               static_cast<unsigned long long>(reason),
               request,
               safe_read<std::int32_t>(request, -1),
               safe_read<std::int32_t>(request + 1, -1),
               safe_read<std::int32_t>(request + 2, -1),
               safe_read<std::int32_t>(request + 3, -1),
               safe_read<std::int32_t>(request + 4, -1),
               safe_read<std::int32_t>(request + 5, -1),
               safe_read<std::int32_t>(request + 6, -1),
               safe_read<std::uint32_t>(reinterpret_cast<const std::byte*>(request) + 0x24U),
               safe_read<std::int32_t>(result, -1));
    }
    original(instance, reason, request, result);
    if (inspect) {
        report("ev=omega_spawner_chain stage=request moment=after n=%u instance=%p "
               "result_count=%d pending=%d,%d,%d,%d,%d,%d mutation=observe_only",
               g_omegaChainSequence,
               instance,
               safe_read<std::int32_t>(result, -1),
               safe_read<std::int32_t>(instance + 0x268U),
               safe_read<std::int32_t>(instance + 0x26CU),
               safe_read<std::int32_t>(instance + 0x270U),
               safe_read<std::int32_t>(instance + 0x274U),
               safe_read<std::int32_t>(instance + 0x278U),
               safe_read<std::int32_t>(instance + 0x27CU));
    }
}

__declspec(noinline) void __fastcall spawner_squad_resolve(std::uint32_t* instance,
                                                            char preferPrimary,
                                                            std::int32_t* squad,
                                                            std::uint64_t* auxiliary) noexcept {
    const SpawnerSquadResolve original = g_squadResolveOriginal.load(std::memory_order_acquire);
    if (original == nullptr) {
        return;
    }
    original(instance, preferPrimary, squad, auxiliary);
    if (g_omegaChainDepth != 0U && g_omegaChainSequence <= kMaximumChainLogs) {
        report("ev=omega_spawner_chain stage=squad_resolve n=%u instance=%p prefer_primary=%d "
               "squad=%08X/%08X/%08X/%08X auxiliary=%016llX,%016llX "
               "ref30=0x%016llX ref38=0x%016llX mutation=observe_only",
               g_omegaChainSequence,
               instance,
               static_cast<int>(preferPrimary),
               static_cast<std::uint32_t>(safe_read<std::int32_t>(squad, -1)),
               static_cast<std::uint32_t>(safe_read<std::int32_t>(squad + 1, -1)),
               static_cast<std::uint32_t>(safe_read<std::int32_t>(squad + 2)),
               static_cast<std::uint32_t>(safe_read<std::int32_t>(squad + 3)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(auxiliary,
                                                                        UINT64_MAX)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(auxiliary + 1)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(
                   reinterpret_cast<const std::byte*>(instance) + 0x218U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(
                   reinterpret_cast<const std::byte*>(instance) + 0x220U)));
    }
}

__declspec(noinline) void __fastcall spawner_member_build(std::uint32_t* instance,
                                                           const std::int32_t* request,
                                                           std::int32_t* members) noexcept {
    const SpawnerMemberBuild original = g_memberBuildOriginal.load(std::memory_order_acquire);
    if (original == nullptr) {
        return;
    }
    original(instance, request, members);
    if (g_omegaChainDepth != 0U && g_omegaChainSequence <= kMaximumChainLogs) {
        report("ev=omega_spawner_chain stage=member_build n=%u instance=%p request_count=%d "
               "members=%d first=%08X,%08X,%08X,%08X mutation=observe_only",
               g_omegaChainSequence,
               instance,
               safe_read<std::int32_t>(request, -1),
               safe_read<std::int32_t>(members, -1),
               static_cast<std::uint32_t>(safe_read<std::int32_t>(members + 2, -1)),
               static_cast<std::uint32_t>(safe_read<std::int32_t>(members + 3, -1)),
               static_cast<std::uint32_t>(safe_read<std::int32_t>(members + 4, -1)),
               static_cast<std::uint32_t>(safe_read<std::int32_t>(members + 5, -1)));
    }
}

__declspec(noinline) std::int32_t __fastcall spawner_queue_find(std::int32_t* queue,
                                                                 const std::int32_t* squad) noexcept {
    const SpawnerQueueFind original = g_queueFindOriginal.load(std::memory_order_acquire);
    if (original == nullptr) {
        return -1;
    }
    const std::int32_t result = original(queue, squad);
    if (g_omegaChainDepth != 0U && g_omegaChainSequence <= kMaximumChainLogs) {
        g_omegaQueue.store(reinterpret_cast<std::uintptr_t>(queue), std::memory_order_release);
        report("ev=omega_spawner_chain stage=queue_find n=%u queue=%p queued=%d squads=%d "
               "squad=%08X/%08X/%08X/%08X result=%d mutation=observe_only",
               g_omegaChainSequence,
               queue,
               safe_read<std::int32_t>(queue, -1),
               safe_read<std::int32_t>(queue + 0x6E2, -1),
               static_cast<std::uint32_t>(safe_read<std::int32_t>(squad, -1)),
               static_cast<std::uint32_t>(safe_read<std::int32_t>(squad + 1, -1)),
               static_cast<std::uint32_t>(safe_read<std::int32_t>(squad + 2)),
               static_cast<std::uint32_t>(safe_read<std::int32_t>(squad + 3)),
               result);
    }
    return result;
}

__declspec(noinline) void __fastcall spawner_drain(std::uint32_t* queue) noexcept {
    const SpawnerDrain original = g_drainOriginal.load(std::memory_order_acquire);
    if (original == nullptr) {
        return;
    }
    const bool inspect = omega_forced()
                         && reinterpret_cast<std::uintptr_t>(queue)
                                == g_omegaQueue.load(std::memory_order_acquire);
    const std::uint32_t sequence = inspect
                                       ? g_drainCalls.fetch_add(1, std::memory_order_relaxed) + 1U
                                       : 0U;
    const bool reportDrain = inspect && (sequence <= 16U || sequence % 300U == 0U);
    if (reportDrain) {
        report("ev=omega_ikora_followup stage=queue_drain moment=before queue=%p "
               "n=%u queued=%d squads=%d mutation=observe_only",
               queue,
               sequence,
               safe_read<std::int32_t>(queue, -1),
               safe_read<std::int32_t>(queue + 0x6E2, -1));
    }
    original(queue);
    if (reportDrain) {
        report("ev=omega_ikora_followup stage=queue_drain moment=after queue=%p "
               "n=%u queued=%d squads=%d mutation=observe_only",
               queue,
               sequence,
               safe_read<std::int32_t>(queue, -1),
               safe_read<std::int32_t>(queue + 0x6E2, -1));
    }
    report_followup_state();
}

__declspec(noinline) void __fastcall scene_transition_retire(std::uint32_t* scene,
                                                             std::uint32_t transition,
                                                             char transitionFlag,
                                                             char alreadyProcessed,
                                                             char forceRetire) noexcept {
    const SceneTransitionRetire original =
        g_sceneTransitionRetireOriginal.load(std::memory_order_acquire);
    if (original == nullptr) {
        return;
    }

    const std::uint32_t sceneHandle = safe_read<std::uint32_t>(scene, kInvalidHandle);
    const bool terminalRetirement = (transition & 0xFFU) == 0xFFU;
    const bool retainSceneTwo = kEnableOmegaSceneTwoInfinitePersistence
                                && omega_forced()
                                && sceneHandle == kOmegaIkoraSceneTwoHandle
                                && terminalRetirement;
    if (!retainSceneTwo) {
        original(scene, transition, transitionFlag, alreadyProcessed, forceRetire);
        return;
    }

    // +58B9A0's terminal branch does nothing except mark and retire the scene-owned actor.
    // Leave that branch unexecuted so the complete scene-2 graph stays alive. The outer event
    // completion caller has already advanced the authored timeline, so scene 3 and mission flow
    // remain native. Publish the host completion edge separately because the retained actor will
    // intentionally never reach the allocator-release probe that used to provide it.
    const std::uint32_t call = g_sceneTwoPersistenceRetireCalls.fetch_add(
                                   1U,
                                   std::memory_order_relaxed)
                               + 1U;
    const std::uint32_t actor = safe_read<std::uint32_t>(scene + 0x0BU, kInvalidHandle);
    const bool sceneThreeActivated =
        (g_sceneActivationObservedMask.load(std::memory_order_acquire) & 0x4U) != 0U;
    const bool handoffArmed = false;
    g_sceneCompletionPending.store(true, std::memory_order_release);
    g_indexHeapReleaseDetachPending.store(true, std::memory_order_release);
    report("ev=omega_scene_two_persistence stage=terminal_retirement action=retain "
           "n=%u scene=%08X scene_ptr=%p actor=%08X transition=%08X "
           "scene3_activated=%s handoff_armed=%s completion_scheduled=yes "
           "event_completion=native mutation=skip_scene_two_terminal_retirement",
           call,
           sceneHandle,
           scene,
           actor,
           transition,
           sceneThreeActivated ? "yes" : "no",
           handoffArmed ? "yes" : "no");
}

__declspec(noinline) void __fastcall scene_actor_scheduler(std::uint32_t* scene) noexcept {
    const SceneActorScheduler original =
        g_sceneActorSchedulerOriginal.load(std::memory_order_acquire);
    if (original == nullptr) {
        return;
    }

    const bool inspect = omega_forced();
    const std::uint32_t sceneHandle = safe_read<std::uint32_t>(scene, kInvalidHandle);
    const std::uint32_t activationBit = omega_scene_activation_bit(sceneHandle);
    const std::size_t activationIndex = omega_scene_activation_index(sceneHandle);
    if (inspect && activationIndex < g_omegaSceneSchedulerPointers.size()) {
        g_omegaSceneSchedulerPointers[activationIndex].store(
            reinterpret_cast<std::uintptr_t>(scene),
            std::memory_order_release);
    }
    const std::uintptr_t callerRva = image_rva(
        reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
    const bool sceneTwoOnlySuppression = kEnableOmegaIkoraSceneTwoOnlyAb && inspect
                                         && activationBit != 0U
                                         && sceneHandle != kOmegaIkoraSceneTwoHandle;
    const bool sceneOneWithVfxSuppression = kEnableOmegaSceneOneWithVfxAb && inspect
                                            && activationBit != 0U
                                            && sceneHandle == kOmegaIkoraSceneThreeHandle;
    const bool suppressSceneCast = sceneTwoOnlySuppression || sceneOneWithVfxSuppression;
    if (suppressSceneCast) {
        const bool firstSuppression =
            (g_sceneCastSuppressionObservedMask.fetch_or(
                 activationBit,
                 std::memory_order_acq_rel)
             & activationBit) == 0U;
        if (firstSuppression) {
            report("ev=omega_scene_cast_suppression stage=scheduler action=skip_native "
                    "scene=%08X scene_ptr=%p caller=+%llX actor_definition=%08X "
                    "ab=%s allow=%s "
                    "renderer_array_suppression=disabled "
                    "scene_two_body_state_suppression=disabled "
                    "scene_two_model_suppression=enabled "
                    "scene_two_events_native=yes "
                   "squad_ikora=%08X squad_ikora_untouched=yes "
                   "mutation=%s",
                   sceneHandle,
                   scene,
                   static_cast<unsigned long long>(callerRva),
                   kIkoraEntityDefinition,
                   sceneOneWithVfxSuppression ? "scene_one_with_native_vfx" : "scene_two_only",
                   sceneOneWithVfxSuppression
                       ? "scene1,scene2_hidden_model"
                       : "scene2",
                   kSquadIkoraDefinition,
                   sceneOneWithVfxSuppression
                       ? "suppress_scene_three_cast"
                       : "suppress_scene_one_and_three_casts");
        }
        return;
    }
    const bool firstActivation = inspect && activationBit != 0U
                                 && (g_sceneActivationObservedMask.fetch_or(
                                         activationBit,
                                         std::memory_order_acq_rel)
                                     & activationBit) == 0U;
    std::uint32_t* const previousScene = g_sceneActorSchedulerScene;
    const std::uint32_t previousDepth = g_sceneActorSchedulerDepth;
    const std::uint32_t previousMask = g_sceneActorSchedulerSpawnMask;
    if (inspect) {
        g_sceneActorSchedulerScene = scene;
        g_sceneActorSchedulerDepth = previousDepth + 1U;
        g_sceneActorSchedulerSpawnMask = 0U;
    }
    if (firstActivation) {
        report_scene_activation_owner(scene, "before_scheduler", callerRva, 0U);
        OmegaSceneAuthorityObservation authority{};
        (void)snapshot_omega_scene_authority_observation(authority);
        report_scene_activation_stack(sceneHandle, authority.sequence);
    }

    original(scene);

    if (inspect) {
        const std::uint32_t spawnedMask = g_sceneActorSchedulerSpawnMask;
        if (firstActivation) {
            report_scene_activation_owner(
                scene, "after_scheduler", callerRva, spawnedMask);
        }
        for (std::uint32_t slot = 0U; slot < 32U; ++slot) {
            if ((spawnedMask & (1U << slot)) != 0U) {
                report_scene_actor_slot(
                    capture_scene_actor_slot(scene, slot), "after_scheduler", 0U);
            }
        }
        g_sceneActorSchedulerScene = previousScene;
        g_sceneActorSchedulerDepth = previousDepth;
        g_sceneActorSchedulerSpawnMask = previousMask;
    }
}

__declspec(noinline) std::int32_t* __fastcall entity_factory(
    std::int32_t* result,
    const std::byte* descriptor,
    std::uint32_t table,
    std::int32_t record) noexcept {
    const EntityFactory original = g_factoryOriginal.load(std::memory_order_acquire);
    if (original == nullptr) {
        return result;
    }
    const std::uint32_t definition = safe_read<std::uint32_t>(descriptor, kInvalidHandle);
    const bool inspect = omega_forced() && definition == kIkoraEntityDefinition;
    const std::uint32_t sequence = inspect
                                       ? g_factoryCalls.fetch_add(1, std::memory_order_relaxed) + 1U
                                       : 0U;
    if (inspect) {
        ++g_ikoraFactoryDepth;
        g_ikoraFactorySequence = sequence;
        g_ikoraFactoryObject = kInvalidHandle;
        g_ikoraFactoryCallerRva = image_rva(
            reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
        const SceneActorSlotCapture castSlot = capture_scene_actor_slot(result);
        if (castSlot.valid) {
            g_sceneActorSchedulerSpawnMask |= 1U << castSlot.slotIndex;
        }
        report_scene_actor_slot(castSlot, "before_factory", sequence);
        remember_ikora_actor(sequence,
                             kInvalidHandle,
                             g_ikoraFactoryCallerRva,
                             descriptor);
        report_native_stack("factory", sequence, kInvalidHandle);
        if (sequence <= kMaximumFactoryLogs) {
            report("ev=omega_ikora_followup stage=factory moment=before n=%u definition=%08X "
                   "caller=+%llX descriptor=%p table=%08X record=%d pos=%.3f,%.3f,%.3f "
                   "quat=%.3f,%.3f,%.3f,%.3f mutation=observe_only",
                   sequence,
                   definition,
                   static_cast<unsigned long long>(g_ikoraFactoryCallerRva),
                   descriptor,
                   table,
                   record,
                   safe_read<float>(descriptor + 0x20U),
                   safe_read<float>(descriptor + 0x24U),
                   safe_read<float>(descriptor + 0x28U),
                   safe_read<float>(descriptor + 0x10U),
                   safe_read<float>(descriptor + 0x14U),
                   safe_read<float>(descriptor + 0x18U),
                   safe_read<float>(descriptor + 0x1CU));
        }
    }
    std::int32_t* const returned = original(result, descriptor, table, record);
    if (inspect) {
        const std::uint32_t object = static_cast<std::uint32_t>(
            safe_read<std::int32_t>(returned, -1));
        if (object != kInvalidHandle) {
            g_ikoraObject.store(object, std::memory_order_release);
        }
        remember_ikora_actor(sequence,
                             object,
                             g_ikoraFactoryCallerRva,
                             descriptor);
        if (sequence <= kMaximumFactoryLogs || object != kInvalidHandle) {
            report("ev=omega_ikora_followup stage=factory moment=after n=%u definition=%08X "
                   "caller=+%llX object=%08X success=%u object_record=%p components=%zu "
                   "mutation=observe_only",
                   sequence,
                   definition,
                   static_cast<unsigned long long>(g_ikoraFactoryCallerRva),
                   object,
                    object != kInvalidHandle ? 1U : 0U,
                    object_record(object),
                    g_ikoraComponentCount);
        }
        if (sequence == 3U) {
            report_ikora_handoff_snapshot("persistent_created", sequence);
        }
        --g_ikoraFactoryDepth;
        g_ikoraFactorySequence = 0U;
        g_ikoraFactoryObject = kInvalidHandle;
        g_ikoraFactoryCallerRva = 0U;
        report_followup_state();
    }
    return returned;
}

__declspec(noinline) void __fastcall object_finalize(std::uint32_t object) noexcept {
    const ObjectFinalize original = g_objectFinalizeOriginal.load(std::memory_order_acquire);
    if (original == nullptr) {
        return;
    }
    const bool inspect = g_ikoraFactoryDepth != 0U;
    if (inspect) {
        g_ikoraFactoryObject = object;
        g_ikoraObject.store(object, std::memory_order_release);
        const std::byte* const native = object_record(object);
        report("ev=omega_ikora_followup stage=object_finalize moment=before factory_n=%u "
               "caller=+%llX object=%08X record=%p flags4=%08X definition4c=%08X "
               "mutation=observe_only",
               g_ikoraFactorySequence,
               static_cast<unsigned long long>(g_ikoraFactoryCallerRva),
               object,
               native,
               native == nullptr ? 0U : safe_read<std::uint32_t>(native + 0x04U, 0U),
               native == nullptr ? kInvalidHandle
                                 : safe_read<std::uint32_t>(native + 0x4CU, kInvalidHandle));
    }
    original(object);
    if (inspect) {
        const std::byte* const native = object_record(object);
        report("ev=omega_ikora_followup stage=object_finalize moment=after factory_n=%u "
               "caller=+%llX object=%08X record=%p flags4=%08X definition4c=%08X "
               "mutation=observe_only",
               g_ikoraFactorySequence,
               static_cast<unsigned long long>(g_ikoraFactoryCallerRva),
               object,
               native,
               native == nullptr ? 0U : safe_read<std::uint32_t>(native + 0x04U, 0U),
               native == nullptr ? kInvalidHandle
                                 : safe_read<std::uint32_t>(native + 0x4CU, kInvalidHandle));
    }
}

__declspec(noinline) void __fastcall entity_for_object(const std::byte* record,
                                                        char flag) noexcept {
    const EntityForObject original = g_entityForObjectOriginal.load(std::memory_order_acquire);
    if (original == nullptr) {
        return;
    }
    const std::uint32_t ownerBefore = record == nullptr
                                          ? kInvalidHandle
                                          : safe_read<std::uint32_t>(record + 0x120U, kInvalidHandle);
    const bool inspect = g_ikoraFactoryDepth != 0U
                         || ownerBefore == g_ikoraObject.load(std::memory_order_acquire);
    original(record, flag);
    if (!inspect) {
        return;
    }
    const std::uint32_t owner = record == nullptr
                                    ? kInvalidHandle
                                    : safe_read<std::uint32_t>(record + 0x120U, kInvalidHandle);
    const std::uint32_t entity = record == nullptr
                                     ? kInvalidHandle
                                     : safe_read<std::uint32_t>(record + 0x110U, kInvalidHandle);
    g_ikoraNetworkRecord.store(reinterpret_cast<std::uintptr_t>(record),
                               std::memory_order_release);
    if (owner != kInvalidHandle) {
        g_ikoraObject.store(owner, std::memory_order_release);
    }
    if (entity != kInvalidHandle) {
        g_ikoraEntity.store(entity, std::memory_order_release);
    }
    remember_ikora_network(g_ikoraFactorySequence, owner, entity, record);
    report("ev=omega_ikora_followup stage=entity_for_object factory_n=%u caller=+%llX "
           "record=%p flag=%d object=%08X entity=%08X flags=%08X state=%08X "
           "mutation=observe_only",
           g_ikoraFactorySequence,
           static_cast<unsigned long long>(g_ikoraFactoryCallerRva),
           record,
           static_cast<int>(flag),
           owner,
           entity,
           record == nullptr ? 0U : safe_read<std::uint32_t>(record + 0x124U, 0U),
           record == nullptr ? 0U : safe_read<std::uint32_t>(record + 0x12CU, 0U));
}

[[nodiscard]] const char* presentation_fingerprint_source_name(
    PresentationFingerprintSource source) noexcept {
    switch (source) {
    case PresentationFingerprintSource::component:
        return "component";
    case PresentationFingerprintSource::componentField:
        return "component_field";
    case PresentationFingerprintSource::renderEntry:
        return "render_entry";
    case PresentationFingerprintSource::renderObjectField:
        return "render_object_field";
    case PresentationFingerprintSource::array1B0:
        return "array_1b0";
    case PresentationFingerprintSource::array1C0:
        return "array_1c0";
    case PresentationFingerprintSource::array1D0:
        return "array_1d0";
    case PresentationFingerprintSource::array1E0:
        return "array_1e0";
    }
    return "unknown";
}

[[nodiscard]] bool renderer_fingerprint_source(
    PresentationFingerprintSource source) noexcept {
    return source != PresentationFingerprintSource::component
           && source != PresentationFingerprintSource::componentField;
}

[[nodiscard]] bool readable_user_pointer(std::uintptr_t value) noexcept {
    if (value < 0x10000U || value > 0x00007FFFFFFFFFFFULL) {
        return false;
    }
    std::byte probe{};
    return safe_copy(&probe, reinterpret_cast<const void*>(value), sizeof probe);
}

void add_presentation_fingerprint(PresentationFingerprintSet& set,
                                  std::uint32_t actor,
                                  std::uintptr_t component,
                                  std::uintptr_t value,
                                  PresentationFingerprintSource source,
                                  std::uint32_t sourceOffset) noexcept {
    if (actor < 1U || actor > 3U || !readable_user_pointer(value)) {
        return;
    }
    for (std::size_t index = 0U; index < set.count; ++index) {
        const PresentationFingerprintPointer& known = set.pointers[index];
        if (known.actor == actor && known.value == value) {
            return;
        }
    }
    if (set.count >= set.pointers.size()) {
        set.saturated = true;
        return;
    }
    set.pointers[set.count++] = {value, component, actor, sourceOffset, source};
    ++set.actorPointerCounts[actor - 1U];
}

void add_presentation_pointer_fields(PresentationFingerprintSet& set,
                                     std::uint32_t actor,
                                     std::uintptr_t component,
                                     const void* address,
                                     std::size_t size,
                                     PresentationFingerprintSource source,
                                     std::uint32_t sourceBase) noexcept {
    if (address == nullptr || size < sizeof(std::uintptr_t)) {
        return;
    }
    const auto base = reinterpret_cast<std::uintptr_t>(address);
    for (std::size_t offset = 0U; offset + sizeof(std::uintptr_t) <= size;
         offset += sizeof(std::uintptr_t)) {
        const std::uintptr_t value = safe_read<std::uintptr_t>(
            reinterpret_cast<const void*>(base + offset), 0U);
        add_presentation_fingerprint(set,
                                     actor,
                                     component,
                                     value,
                                     source,
                                     sourceBase + static_cast<std::uint32_t>(offset));
    }
}

[[nodiscard]] std::int32_t bounded_presentation_count(std::uintptr_t component,
                                                       std::size_t offset) noexcept {
    const std::int32_t count = safe_read<std::int32_t>(
        reinterpret_cast<const void*>(component + offset), 0);
    return count > 0 && count <= kMaximumRendererObjectArrayCount ? count : 0;
}

[[nodiscard]] std::int32_t collect_scene_two_presentation_renderers(
    std::array<std::uintptr_t, kMaximumRendererObjectArrayCount>& objects) noexcept {
    objects = {};
    if (!kEnableOmegaSceneTwoPresentationSuppression) {
        return 0;
    }
    const std::uintptr_t component =
        g_sceneTwoPresentationComponent.load(std::memory_order_acquire);
    if (component == 0U
        || safe_read<std::uint32_t>(reinterpret_cast<const void*>(component), kInvalidHandle)
               != kIkoraPresentationDefinition) {
        return 0;
    }
    const std::uint32_t owner = safe_read<std::uint32_t>(
        reinterpret_cast<const void*>(component + 0x2CU), kInvalidHandle);
    if (owner == kInvalidHandle
        || owner != g_sceneTwoPresentationObject.load(std::memory_order_acquire)
        || object_record(owner) == nullptr) {
        return 0;
    }
    std::int32_t count = 0;
    const auto append = [&objects, &count](std::uintptr_t object) noexcept {
        if (count >= static_cast<std::int32_t>(objects.size())
            || !readable_user_pointer(object)
            || std::find(objects.begin(), objects.begin() + count, object)
                   != objects.begin() + count) {
            return;
        }
        objects[static_cast<std::size_t>(count++)] = object;
    };
    for (const auto& cached : g_sceneTwoPresentationRenderers) {
        append(cached.load(std::memory_order_acquire));
    }
    const std::uintptr_t entries = safe_read<std::uintptr_t>(
        reinterpret_cast<const void*>(component + 0x190U), 0U);
    const std::int32_t entryCount = bounded_presentation_count(component, 0x198U);
    for (std::int32_t index = 0; entries != 0U && index < entryCount; ++index) {
        const std::uintptr_t entry = entries + static_cast<std::uintptr_t>(index) * 0x50U;
        const std::uintptr_t object = safe_read<std::uintptr_t>(
            reinterpret_cast<const void*>(entry + 0x20U), 0U);
        append(object);
        if (!readable_user_pointer(object)) {
            continue;
        }
        for (auto& cached : g_sceneTwoPresentationRenderers) {
            std::uintptr_t expected = 0U;
            const std::uintptr_t known = cached.load(std::memory_order_acquire);
            if (known == object
                || (known == 0U && cached.compare_exchange_strong(
                                      expected,
                                      object,
                                      std::memory_order_acq_rel,
                                      std::memory_order_acquire))) {
                break;
            }
        }
    }
    return count;
}

std::int32_t disable_scene_two_presentation_renderers(const char* source,
                                                       std::uintptr_t callerRva) noexcept {
    if (!kEnableOmegaSceneTwoPresentationSuppression) {
        return 0;
    }
    const std::uint32_t call =
        g_sceneTwoPresentationDisableCalls.fetch_add(1U, std::memory_order_relaxed) + 1U;
    std::array<std::uintptr_t, kMaximumRendererObjectArrayCount> objects{};
    const std::int32_t count = collect_scene_two_presentation_renderers(objects);
    const RendererBulkBoolean original =
        g_rendererBulkBooleanOriginal.load(std::memory_order_acquire);
    if (count > 0 && original != nullptr) {
        original(objects.data(), count, 0);
        g_sceneTwoPresentationDisabledObjects.fetch_add(
            static_cast<std::uint32_t>(count), std::memory_order_relaxed);
    }
    if (call <= 8U || (count > 0 && (call % 64U) == 0U)) {
        report("ev=omega_scene_two_presentation_suppression stage=renderer_disable "
               "n=%u source=%s caller=+%llX component=%p actor=%08X "
               "render_objects=%d action=%s scene_casts=native vfx_paths=native "
               "mutation=force_scene_two_presentation_disabled",
               call,
               source == nullptr ? "unknown" : source,
               static_cast<unsigned long long>(callerRva),
               reinterpret_cast<void*>(
                   g_sceneTwoPresentationComponent.load(std::memory_order_acquire)),
               g_sceneTwoPresentationObject.load(std::memory_order_acquire),
               count,
               count > 0 && original != nullptr ? "force_disabled" : "not_ready");
    }
    return count;
}

void gather_presentation_fingerprints(PresentationFingerprintSet& set) noexcept {
    set = {};
    std::array<IkoraComponent, 3U> presentations{};
    AcquireSRWLockShared(&g_ikoraComponentLock);
    for (std::size_t index = 0U; index < g_ikoraComponentCount; ++index) {
        const IkoraComponent& component = g_ikoraComponents[index];
        if (component.definition == kIkoraPresentationDefinition
            && component.factorySequence >= 1U && component.factorySequence <= 3U) {
            presentations[component.factorySequence - 1U] = component;
        }
    }
    ReleaseSRWLockShared(&g_ikoraComponentLock);

    for (std::size_t slot = 0U; slot < presentations.size(); ++slot) {
        const IkoraComponent& presentation = presentations[slot];
        if (presentation.instance == 0U) {
            continue;
        }
        const std::uint32_t actor = static_cast<std::uint32_t>(slot + 1U);
        const std::uintptr_t component = presentation.instance;
        set.components[slot] = component;
        set.readableActorMask |= 1U << slot;
        add_presentation_fingerprint(set,
                                     actor,
                                     component,
                                     component,
                                     PresentationFingerprintSource::component,
                                     0U);
        add_presentation_pointer_fields(
            set,
            actor,
            component,
            reinterpret_cast<const void*>(component),
            std::min<std::size_t>(presentation.size, 0x398U),
            PresentationFingerprintSource::componentField,
            0U);

        // +0x190 is the 0x50-byte rendered-object entry vector. The pointer at entry+0x20 is the
        // renderer-owned object consumed by the presentation subsystem.
        const std::uintptr_t renderEntries = safe_read<std::uintptr_t>(
            reinterpret_cast<const void*>(component + 0x190U), 0U);
        const std::int32_t renderEntryCount = bounded_presentation_count(component, 0x198U);
        for (std::int32_t index = 0; renderEntries != 0U && index < renderEntryCount; ++index) {
            const std::uintptr_t entry = renderEntries
                                         + static_cast<std::uintptr_t>(index) * 0x50U;
            const std::uintptr_t renderObject = safe_read<std::uintptr_t>(
                reinterpret_cast<const void*>(entry + 0x20U), 0U);
            const std::uint32_t encoded = (static_cast<std::uint32_t>(index) << 16U) | 0x20U;
            add_presentation_fingerprint(set,
                                         actor,
                                         component,
                                         renderObject,
                                         PresentationFingerprintSource::renderEntry,
                                         encoded);
            if (readable_user_pointer(renderObject)) {
                add_presentation_pointer_fields(
                    set,
                    actor,
                    component,
                    reinterpret_cast<const void*>(renderObject),
                    kMaximumPresentationNestedScanBytes,
                    PresentationFingerprintSource::renderObjectField,
                    static_cast<std::uint32_t>(index) << 16U);
            }
        }

        struct ArrayDescription final {
            std::size_t pointerOffset;
            std::size_t countOffset;
            std::size_t stride;
            PresentationFingerprintSource source;
        };
        constexpr std::array<ArrayDescription, 4U> arrays{{
            {0x1B0U, 0x1B8U, 0x08U, PresentationFingerprintSource::array1B0},
            {0x1C0U, 0x1C8U, 0x18U, PresentationFingerprintSource::array1C0},
            {0x1D0U, 0x1D8U, 0x20U, PresentationFingerprintSource::array1D0},
            {0x1E0U, 0x1E8U, 0x08U, PresentationFingerprintSource::array1E0},
        }};
        for (const ArrayDescription& description : arrays) {
            const std::uintptr_t base = safe_read<std::uintptr_t>(
                reinterpret_cast<const void*>(component + description.pointerOffset), 0U);
            const std::int32_t count = bounded_presentation_count(
                component, description.countOffset);
            for (std::int32_t index = 0; base != 0U && index < count; ++index) {
                const std::uintptr_t entry = base
                                             + static_cast<std::uintptr_t>(index)
                                                   * description.stride;
                add_presentation_pointer_fields(
                    set,
                    actor,
                    component,
                    reinterpret_cast<const void*>(entry),
                    description.stride,
                    description.source,
                    static_cast<std::uint32_t>(index) << 16U);
            }
        }
    }
}

void correlate_attachment_pointer_region(
    AttachmentPointerCorrelation& correlation,
    const PresentationFingerprintSet& fingerprints,
    const char* region,
    const void* address,
    std::size_t size) noexcept {
    if (address == nullptr || size < sizeof(std::uintptr_t)) {
        return;
    }
    const auto base = reinterpret_cast<std::uintptr_t>(address);
    for (std::size_t offset = 0U; offset + sizeof(std::uintptr_t) <= size;
         offset += sizeof(std::uint32_t)) {
        const std::uintptr_t value = safe_read<std::uintptr_t>(
            reinterpret_cast<const void*>(base + offset), 0U);
        if (value == 0U) {
            continue;
        }
        for (std::size_t index = 0U; index < fingerprints.count; ++index) {
            const PresentationFingerprintPointer& fingerprint = fingerprints.pointers[index];
            if (fingerprint.value != value || fingerprint.actor < 1U
                || fingerprint.actor > 3U) {
                continue;
            }
            const std::size_t slot = fingerprint.actor - 1U;
            ++correlation.counts[slot];
            correlation.actorMask |= 1U << slot;
            if (renderer_fingerprint_source(fingerprint.source)) {
                ++correlation.rendererCounts[slot];
                correlation.rendererActorMask |= 1U << slot;
            }
            if (correlation.firstValues[slot] == 0U) {
                correlation.firstValues[slot] = value;
                correlation.firstComponents[slot] = fingerprint.component;
                correlation.firstRegionOffsets[slot] = static_cast<std::uint32_t>(offset);
                correlation.firstFingerprintOffsets[slot] = fingerprint.sourceOffset;
                correlation.firstFingerprintSources[slot] = fingerprint.source;
                correlation.firstRegions[slot] = region;
            }
        }
    }
}

[[nodiscard]] AttachmentHandleCorrelation correlate_attachment_handle(
    std::uint32_t handle) noexcept {
    AttachmentHandleCorrelation result{};
    if (handle == kInvalidHandle) {
        return result;
    }
    std::array<IkoraComponent, kMaximumIkoraComponents> components{};
    std::size_t componentCount = 0U;
    AcquireSRWLockShared(&g_ikoraComponentLock);
    componentCount = std::min(g_ikoraComponentCount, components.size());
    std::copy_n(g_ikoraComponents.begin(), componentCount, components.begin());
    ReleaseSRWLockShared(&g_ikoraComponentLock);

    for (std::size_t index = 0U; index < componentCount; ++index) {
        const IkoraComponent& component = components[index];
        if (component.instance == 0U || component.factorySequence < 1U
            || component.factorySequence > 3U) {
            continue;
        }
        const std::size_t scanSize = std::min(component.size,
                                               kMaximumAttachmentGraphScanBytes);
        for (std::size_t offset = 0U; offset + sizeof(handle) <= scanSize;
             offset += sizeof(handle)) {
            const std::uint32_t value = safe_read<std::uint32_t>(
                reinterpret_cast<const void*>(component.instance + offset),
                kInvalidHandle);
            if (value != handle) {
                continue;
            }
            const std::size_t slot = component.factorySequence - 1U;
            ++result.counts[slot];
            result.actorMask |= 1U << slot;
            if (result.firstComponents[slot] == 0U) {
                result.firstComponents[slot] = component.instance;
                result.firstDefinitions[slot] = component.definition;
                result.firstOffsets[slot] = static_cast<std::uint32_t>(offset);
            }
        }
    }
    return result;
}

/**
 * Correlates one concrete pose object by exact pointer equality only.
 *
 * The first half scans bounded captured component storage. The second half checks the richer
 * presentation fingerprint, including pointers found inside renderer-owned objects. A renderer
 * mask with exactly one bit is the strongest evidence that a pose belongs to the visible actor;
 * this recorder deliberately does not turn that evidence into a mutation in the same run.
 */
[[nodiscard]] PosePointerCorrelation correlate_pose_pointer(
    std::uintptr_t poseObject) noexcept {
    PosePointerCorrelation result{};
    if (poseObject == 0U) {
        return result;
    }

    std::array<IkoraComponent, kMaximumIkoraComponents> components{};
    std::size_t componentCount = 0U;
    AcquireSRWLockShared(&g_ikoraComponentLock);
    componentCount = std::min(g_ikoraComponentCount, components.size());
    std::copy_n(g_ikoraComponents.begin(), componentCount, components.begin());
    ReleaseSRWLockShared(&g_ikoraComponentLock);

    for (std::size_t index = 0U; index < componentCount; ++index) {
        const IkoraComponent& component = components[index];
        if (component.instance == 0U || component.factorySequence < 1U
            || component.factorySequence > 3U) {
            continue;
        }
        const std::size_t scanSize = std::min(
            component.size, kMaximumAttachmentGraphScanBytes);
        for (std::size_t offset = 0U;
             offset + sizeof(std::uintptr_t) <= scanSize;
             offset += sizeof(std::uintptr_t)) {
            const std::uintptr_t value = safe_read<std::uintptr_t>(
                reinterpret_cast<const void*>(component.instance + offset), 0U);
            if (value != poseObject) {
                continue;
            }
            const std::size_t slot = component.factorySequence - 1U;
            ++result.componentCounts[slot];
            result.componentActorMask |= 1U << slot;
            if (result.firstComponents[slot] == 0U) {
                result.firstComponents[slot] = component.instance;
                result.firstDefinitions[slot] = component.definition;
                result.firstComponentOffsets[slot] =
                    static_cast<std::uint32_t>(offset);
            }
        }
    }

    PresentationFingerprintSet& fingerprints = g_presentationFingerprintScratch;
    gather_presentation_fingerprints(fingerprints);
    for (std::size_t index = 0U; index < fingerprints.count; ++index) {
        const PresentationFingerprintPointer& fingerprint =
            fingerprints.pointers[index];
        if (fingerprint.value != poseObject || fingerprint.actor < 1U
            || fingerprint.actor > 3U) {
            continue;
        }
        const std::size_t slot = fingerprint.actor - 1U;
        ++result.presentationCounts[slot];
        result.presentationActorMask |= 1U << slot;
        if (renderer_fingerprint_source(fingerprint.source)) {
            ++result.rendererCounts[slot];
            result.rendererActorMask |= 1U << slot;
        }
        if (result.firstPresentationComponents[slot] == 0U) {
            result.firstPresentationComponents[slot] = fingerprint.component;
            result.firstFingerprintOffsets[slot] = fingerprint.sourceOffset;
            result.firstFingerprintSources[slot] = fingerprint.source;
        }
    }
    return result;
}

/** @return True when this exact caller/interface/key path has not been sampled before. */
[[nodiscard]] bool remember_pose_socket_trace_key(
    const PoseSocketTraceKey& candidate) noexcept {
    bool first = false;
    AcquireSRWLockExclusive(&g_poseSocketTraceLock);
    const auto begin = g_poseSocketTraceKeys.begin();
    const auto end = begin + g_poseSocketTraceKeyCount;
    const auto known = std::find_if(
        begin,
        end,
        [&candidate](const PoseSocketTraceKey& key) noexcept {
            return key.callerRva == candidate.callerRva
                   && key.dispatchBase == candidate.dispatchBase
                   && key.poseObject == candidate.poseObject
                   && key.bindingKey == candidate.bindingKey
                   && key.exactVfx == candidate.exactVfx;
        });
    if (known == end && g_poseSocketTraceKeyCount < g_poseSocketTraceKeys.size()) {
        g_poseSocketTraceKeys[g_poseSocketTraceKeyCount++] = candidate;
        first = true;
    }
    ReleaseSRWLockExclusive(&g_poseSocketTraceLock);
    return first;
}

/** Selects the first and bounded heartbeat sample for one concrete effect transform path. */
[[nodiscard]] bool select_effect_transform_sample(
    const EffectTransformTraceKey& candidate,
    std::uint64_t now,
    std::size_t& ordinal,
    bool& first,
    bool& heartbeat) noexcept {
    bool selected = false;
    first = false;
    heartbeat = false;
    ordinal = 0U;
    AcquireSRWLockExclusive(&g_effectTransformTraceLock);
    const auto begin = g_effectTransformTraceKeys.begin();
    const auto end = begin + g_effectTransformTraceKeyCount;
    auto known = std::find_if(
        begin,
        end,
        [&candidate](const EffectTransformTraceKey& key) noexcept {
            return key.effect == candidate.effect
                   && key.callerRva == candidate.callerRva
                   && key.definition == candidate.definition
                   && key.resource == candidate.resource;
        });
    if (known == end && g_effectTransformTraceKeyCount < g_effectTransformTraceKeys.size()) {
        EffectTransformTraceKey stored = candidate;
        stored.lastSampleAtMs = now;
        g_effectTransformTraceKeys[g_effectTransformTraceKeyCount++] = stored;
        ordinal = g_effectTransformTraceKeyCount;
        first = true;
        selected = true;
    } else if (known != end) {
        ordinal = static_cast<std::size_t>(known - begin) + 1U;
        if (now >= known->lastSampleAtMs
            && now - known->lastSampleAtMs >= kEffectTransformHeartbeatMs) {
            known->lastSampleAtMs = now;
            heartbeat = true;
            selected = true;
        }
    }
    ReleaseSRWLockExclusive(&g_effectTransformTraceLock);
    return selected;
}

void publish_vfx_socket_transform(const DecodedSceneTransform& transform,
                                  std::uint64_t capturedAtMs) noexcept {
    if (!transform.readable || !transform.finite) {
        return;
    }
    AcquireSRWLockExclusive(&g_vfxSocketTransformLock);
    g_vfxSocketTransform.rotation = transform.rotation;
    g_vfxSocketTransform.position = transform.position;
    g_vfxSocketTransform.scale = transform.scale;
    g_vfxSocketTransform.capturedAtMs = capturedAtMs;
    g_vfxSocketTransform.valid = true;
    ReleaseSRWLockExclusive(&g_vfxSocketTransformLock);
}

[[nodiscard]] VfxSocketTransformSnapshot latest_vfx_socket_transform() noexcept {
    VfxSocketTransformSnapshot result{};
    AcquireSRWLockShared(&g_vfxSocketTransformLock);
    result = g_vfxSocketTransform;
    ReleaseSRWLockShared(&g_vfxSocketTransformLock);
    return result;
}

/** @return True when this provider/interface/pose provenance has not been sampled before. */
[[nodiscard]] bool remember_pose_provider_provenance_trace_key(
    const PoseProviderProvenanceTraceKey& candidate) noexcept {
    bool first = false;
    AcquireSRWLockExclusive(&g_poseProviderProvenanceTraceLock);
    const auto begin = g_poseProviderProvenanceTraceKeys.begin();
    const auto end = begin + g_poseProviderProvenanceTraceKeyCount;
    const auto known = std::find_if(
        begin,
        end,
        [&candidate](const PoseProviderProvenanceTraceKey& key) noexcept {
            return key.provider == candidate.provider
                   && key.poseObject == candidate.poseObject
                   && key.definitionBase == candidate.definitionBase
                   && key.candidateIndex == candidate.candidateIndex
                   && key.bindingKey == candidate.bindingKey
                   && key.flags == candidate.flags
                   && key.exactVfx == candidate.exactVfx;
        });
    if (known == end
        && g_poseProviderProvenanceTraceKeyCount
               < g_poseProviderProvenanceTraceKeys.size()) {
        g_poseProviderProvenanceTraceKeys[
            g_poseProviderProvenanceTraceKeyCount++] = candidate;
        first = true;
    }
    ReleaseSRWLockExclusive(&g_poseProviderProvenanceTraceLock);
    return first;
}

[[nodiscard]] bool snapshot_scene_one_pose_binding(
    SceneOnePoseBindingCache& destination) noexcept {
    AcquireSRWLockShared(&g_sceneOnePoseBindingLock);
    destination = g_sceneOnePoseBinding;
    ReleaseSRWLockShared(&g_sceneOnePoseBindingLock);
    return destination.valid;
}

/** Publishes a validated native scene-1 pose and reports whether its identity changed. */
[[nodiscard]] bool publish_scene_one_pose_binding(
    const SceneOnePoseBindingCache& source) noexcept {
    bool changed = false;
    AcquireSRWLockExclusive(&g_sceneOnePoseBindingLock);
    changed = !g_sceneOnePoseBinding.valid
              || g_sceneOnePoseBinding.actor != source.actor
              || g_sceneOnePoseBinding.provider != source.provider
              || g_sceneOnePoseBinding.definitionBase != source.definitionBase
              || g_sceneOnePoseBinding.dispatchBase != source.dispatchBase
              || g_sceneOnePoseBinding.poseObject != source.poseObject;
    g_sceneOnePoseBinding = source;
    ReleaseSRWLockExclusive(&g_sceneOnePoseBindingLock);
    return changed;
}

void clear_scene_one_pose_binding() noexcept {
    AcquireSRWLockExclusive(&g_sceneOnePoseBindingLock);
    g_sceneOnePoseBinding = {};
    ReleaseSRWLockExclusive(&g_sceneOnePoseBindingLock);
}

void report_attachment_handle_correlation(std::uint32_t sample,
                                          std::uint32_t call,
                                          const char* target,
                                          std::uint32_t handle,
                                          const AttachmentHandleCorrelation& match) noexcept {
    report("ev=omega_vfx_renderer_correlation stage=attachment_handle n=%u call=%u "
           "target=%s handle=%08X actor_mask=%u counts=%u,%u,%u "
           "first_components=%p,%p,%p first_definitions=%08X,%08X,%08X "
           "first_offsets=0x%X,0x%X,0x%X evidence=exact_u32_component_graph_match "
           "mutation=observe_only",
           sample,
           call,
           target,
           handle,
           match.actorMask,
           match.counts[0],
           match.counts[1],
           match.counts[2],
           reinterpret_cast<void*>(match.firstComponents[0]),
           reinterpret_cast<void*>(match.firstComponents[1]),
           reinterpret_cast<void*>(match.firstComponents[2]),
           match.firstDefinitions[0],
           match.firstDefinitions[1],
           match.firstDefinitions[2],
           match.firstOffsets[0],
           match.firstOffsets[1],
           match.firstOffsets[2]);
}

void report_vfx_renderer_correlation(
    std::uint32_t sample,
    std::uint32_t call,
    const SceneAuthoredEventView& view,
    const void* context,
    std::uintptr_t contextData,
    std::uint32_t contextOwner,
    std::uint32_t contextObject,
    std::uint32_t createdObject,
    std::uint32_t createdOwner,
    const std::byte* createdRecord,
    const std::byte* ownerRecord,
    const std::byte* ownerSceneRecord,
    const std::byte* contextObjectRecord,
    const SceneType23TransformCapture& transform) noexcept {
    if (view.sceneHandle != kOmegaPortalSceneHandle
        && view.sceneHandle != kOmegaIkoraOpeningSceneHandle) {
        return;
    }

    PresentationFingerprintSet& fingerprints = g_presentationFingerprintScratch;
    gather_presentation_fingerprints(fingerprints);
    AttachmentPointerCorrelation pointers{};
    correlate_attachment_pointer_region(pointers, fingerprints, "context", context, 0x80U);
    correlate_attachment_pointer_region(
        pointers, fingerprints, "context_data", reinterpret_cast<const void*>(contextData), 0x200U);
    correlate_attachment_pointer_region(
        pointers, fingerprints, "authored", view.authored, 0x100U);
    correlate_attachment_pointer_region(
        pointers, fingerprints, "runtime", view.runtime, 0x100U);
    correlate_attachment_pointer_region(
        pointers, fingerprints, "created_record", createdRecord, 0xA0U);
    correlate_attachment_pointer_region(
        pointers, fingerprints, "owner_record", ownerRecord, 0x100U);
    correlate_attachment_pointer_region(
        pointers, fingerprints, "owner_scene_record", ownerSceneRecord, 0x100U);
    correlate_attachment_pointer_region(
        pointers, fingerprints, "context_object_record", contextObjectRecord, 0x100U);
    if (transform.transformReadable) {
        correlate_attachment_pointer_region(pointers,
                                             fingerprints,
                                             "transform_output",
                                             transform.transform.data(),
                                             transform.transform.size());
    }
    if (transform.auxiliaryReadable) {
        correlate_attachment_pointer_region(pointers,
                                             fingerprints,
                                             "transform_auxiliary",
                                             transform.auxiliary.data(),
                                             transform.auxiliary.size());
    }

    report("ev=omega_vfx_renderer_correlation stage=presentation_fingerprint n=%u call=%u "
           "scene=%08X selector=%u created=%08X components=%p,%p,%p "
           "pointer_counts=%u,%u,%u total=%zu readable_actor_mask=%u saturated=%u "
           "source=80EC13A2_runtime_graph mutation=observe_only",
           sample,
           call,
           view.sceneHandle,
           transform.selector,
           createdObject,
           reinterpret_cast<void*>(fingerprints.components[0]),
           reinterpret_cast<void*>(fingerprints.components[1]),
           reinterpret_cast<void*>(fingerprints.components[2]),
           fingerprints.actorPointerCounts[0],
           fingerprints.actorPointerCounts[1],
           fingerprints.actorPointerCounts[2],
           fingerprints.count,
           fingerprints.readableActorMask,
           fingerprints.saturated ? 1U : 0U);
    report("ev=omega_vfx_renderer_correlation stage=pointer_match n=%u call=%u "
           "scene=%08X actor_mask=%u renderer_actor_mask=%u counts=%u,%u,%u "
           "renderer_counts=%u,%u,%u first_values=%p,%p,%p "
           "first_regions=%s,%s,%s first_region_offsets=0x%X,0x%X,0x%X "
           "first_components=%p,%p,%p first_sources=%s,%s,%s "
           "first_fingerprint_offsets=0x%X,0x%X,0x%X "
           "evidence=exact_u64_pointer_equality mutation=observe_only",
           sample,
           call,
           view.sceneHandle,
           pointers.actorMask,
           pointers.rendererActorMask,
           pointers.counts[0],
           pointers.counts[1],
           pointers.counts[2],
           pointers.rendererCounts[0],
           pointers.rendererCounts[1],
           pointers.rendererCounts[2],
           reinterpret_cast<void*>(pointers.firstValues[0]),
           reinterpret_cast<void*>(pointers.firstValues[1]),
           reinterpret_cast<void*>(pointers.firstValues[2]),
           pointers.firstRegions[0] == nullptr ? "none" : pointers.firstRegions[0],
           pointers.firstRegions[1] == nullptr ? "none" : pointers.firstRegions[1],
           pointers.firstRegions[2] == nullptr ? "none" : pointers.firstRegions[2],
           pointers.firstRegionOffsets[0],
           pointers.firstRegionOffsets[1],
           pointers.firstRegionOffsets[2],
           reinterpret_cast<void*>(pointers.firstComponents[0]),
           reinterpret_cast<void*>(pointers.firstComponents[1]),
           reinterpret_cast<void*>(pointers.firstComponents[2]),
           pointers.firstValues[0] == 0U
               ? "none"
               : presentation_fingerprint_source_name(pointers.firstFingerprintSources[0]),
           pointers.firstValues[1] == 0U
               ? "none"
               : presentation_fingerprint_source_name(pointers.firstFingerprintSources[1]),
           pointers.firstValues[2] == 0U
               ? "none"
               : presentation_fingerprint_source_name(pointers.firstFingerprintSources[2]),
           pointers.firstFingerprintOffsets[0],
           pointers.firstFingerprintOffsets[1],
           pointers.firstFingerprintOffsets[2]);

    report_attachment_handle_correlation(
        sample, call, "context_owner_cd7050", contextOwner,
        correlate_attachment_handle(contextOwner));
    report_attachment_handle_correlation(
        sample, call, "context_object", contextObject,
        correlate_attachment_handle(contextObject));
    report_attachment_handle_correlation(
        sample, call, "created_owner", createdOwner,
        correlate_attachment_handle(createdOwner));
}

[[nodiscard]] bool renderer_handle_capture_active() noexcept {
    if (!omega_forced()) {
        return false;
    }
    const bool presentationStart =
        g_componentStartInstance != 0U
        && g_componentStartDefinition == kIkoraPresentationDefinition
        && g_componentStartFactorySequence != 0U;
    // Component construction did not call these wrappers in the first run. Keep recording after
    // the factory returns so deferred renderer creation, scene playback, and both handoff releases
    // remain inside the capture window.
    return presentationStart || g_ikoraFactoryDepth != 0U
           || g_ikoraObject.load(std::memory_order_acquire) != kInvalidHandle;
}

[[nodiscard]] RendererActorCorrelation correlate_renderer_object(
    std::uintptr_t object) noexcept {
    RendererActorCorrelation result{};

    AcquireSRWLockShared(&g_ikoraActorLock);
    for (std::size_t index = 0U; index < g_ikoraActorCount; ++index) {
        const IkoraActor& actor = g_ikoraActors[index];
        if (actor.factorySequence < 1U || actor.factorySequence > 3U) {
            continue;
        }
        const std::size_t slot = actor.factorySequence - 1U;
        result.objects[slot] = actor.object;
        result.behaviorActors[slot] = actor.behaviorActor;
        if (actor.released) {
            result.releasedMask |= 1U << slot;
        }
    }
    ReleaseSRWLockShared(&g_ikoraActorLock);

    AcquireSRWLockShared(&g_ikoraComponentLock);
    for (std::size_t index = 0U; index < g_ikoraComponentCount; ++index) {
        const IkoraComponent& component = g_ikoraComponents[index];
        if (component.definition != kIkoraPresentationDefinition
            || component.factorySequence < 1U || component.factorySequence > 3U) {
            continue;
        }
        result.presentationComponents[component.factorySequence - 1U] = component.instance;
    }
    ReleaseSRWLockShared(&g_ikoraComponentLock);

    for (std::size_t slot = 0U; slot < 3U; ++slot) {
        const std::uint32_t actor = result.objects[slot];
        const std::uintptr_t presentation = result.presentationComponents[slot];
        if (actor != kInvalidHandle && object == static_cast<std::uintptr_t>(actor)) {
            result.directActorMask |= 1U << slot;
        }
        if (presentation != 0U && object == presentation) {
            result.directPresentationMask |= 1U << slot;
        }
    }

    std::array<std::byte, kRendererCorrelationBytes> bytes{};
    result.readable = object >= 0x10000U
                      && safe_copy(bytes.data(), reinterpret_cast<const void*>(object), bytes.size());
    if (!result.readable) {
        return result;
    }

    for (std::size_t offset = 0U; offset + sizeof(std::uint32_t) <= bytes.size();
         offset += sizeof(std::uint32_t)) {
        std::uint32_t value = 0U;
        std::memcpy(&value, bytes.data() + offset, sizeof value);
        for (std::size_t slot = 0U; slot < 3U; ++slot) {
            if (result.objects[slot] != kInvalidHandle && value == result.objects[slot]) {
                result.objectReferenceMask |= 1U << slot;
                if (result.objectOffsets[slot] < 0) {
                    result.objectOffsets[slot] = static_cast<std::int32_t>(offset);
                }
            }
        }
    }
    for (std::size_t offset = 0U; offset + sizeof(std::uintptr_t) <= bytes.size();
         offset += sizeof(std::uintptr_t)) {
        std::uintptr_t value = 0U;
        std::memcpy(&value, bytes.data() + offset, sizeof value);
        for (std::size_t slot = 0U; slot < 3U; ++slot) {
            if (result.presentationComponents[slot] != 0U
                && value == result.presentationComponents[slot]) {
                result.presentationReferenceMask |= 1U << slot;
                if (result.presentationOffsets[slot] < 0) {
                    result.presentationOffsets[slot] = static_cast<std::int32_t>(offset);
                }
            }
            if (result.behaviorActors[slot] != 0U && value == result.behaviorActors[slot]) {
                result.behaviorReferenceMask |= 1U << slot;
            }
        }
    }
    return result;
}

[[nodiscard]] bool remember_renderer_trace_key(std::uintptr_t wrapperRva,
                                               std::uintptr_t callerRva,
                                               std::uintptr_t object,
                                               int enabled,
                                               std::uint32_t releasedMask,
                                               const char* moment) noexcept {
    const RendererTraceKey candidate{
        wrapperRva,
        callerRva,
        object,
        enabled,
        releasedMask,
        moment != nullptr && std::strcmp(moment, "after") == 0};
    bool inserted = false;
    AcquireSRWLockExclusive(&g_rendererTraceLock);
    const auto same = [&candidate](const RendererTraceKey& key) noexcept {
        return key.wrapperRva == candidate.wrapperRva
               && key.callerRva == candidate.callerRva
               && key.object == candidate.object
               && key.enabled == candidate.enabled
               && key.releasedMask == candidate.releasedMask
               && key.after == candidate.after;
    };
    const bool known = std::any_of(g_rendererTraceKeys.begin(),
                                   g_rendererTraceKeys.begin() + g_rendererTraceKeyCount,
                                   same);
    if (!known && g_rendererTraceKeyCount < g_rendererTraceKeys.size()) {
        g_rendererTraceKeys[g_rendererTraceKeyCount++] = candidate;
        inserted = true;
    }
    ReleaseSRWLockExclusive(&g_rendererTraceLock);
    return inserted;
}

void report_renderer_handle(const char* operation,
                            const char* moment,
                            std::uintptr_t wrapperRva,
                            std::uintptr_t callerRva,
                            std::uintptr_t object,
                            std::int32_t index,
                            std::int32_t count,
                            int enabled) noexcept {
    if (!renderer_handle_capture_active()) {
        return;
    }
    const RendererActorCorrelation correlation = correlate_renderer_object(object);
    if (!remember_renderer_trace_key(wrapperRva,
                                     callerRva,
                                     object,
                                     enabled,
                                     correlation.releasedMask,
                                     moment)) {
        return;
    }
    const std::uint32_t sample =
        g_rendererHandleSamples.fetch_add(1U, std::memory_order_relaxed) + 1U;
    if (sample > kMaximumRendererHandleSamples) {
        return;
    }
    const auto address = [object](std::uintptr_t offset) noexcept -> const void* {
        if (object < 0x10000U || object > UINTPTR_MAX - offset) {
            return nullptr;
        }
        return reinterpret_cast<const void*>(object + offset);
    };
    report("ev=omega_actor_renderer_handle stage=wrapper moment=%s sample=%u "
           "operation=%s wrapper=+%llX caller=+%llX factory_n=%u actor=%08X "
           "component=%p definition=%08X handler=+%llX object=%p index=%d count=%d "
           "enabled=%d q00=%016llX q08=%016llX q10=%016llX q18=%016llX "
           "q20=%016llX q28=%016llX q30=%016llX q38=%016llX "
           "u130=%08X u134=%08X b160=%02X b161=%02X b162=%02X b164=%02X "
           "p170=%p correlation_readable=%s released_mask=%u "
           "actors=%08X,%08X,%08X presentations=%p,%p,%p "
           "object_ref_mask=%u object_offsets=%d,%d,%d "
           "presentation_ref_mask=%u presentation_offsets=%d,%d,%d "
           "behavior_ref_mask=%u direct_actor_mask=%u direct_presentation_mask=%u "
           "scope=%s mutation=observe_only",
           moment,
           sample,
           operation,
           static_cast<unsigned long long>(wrapperRva),
           static_cast<unsigned long long>(callerRva),
           g_componentStartFactorySequence,
           g_componentStartObject,
           reinterpret_cast<void*>(g_componentStartInstance),
           g_componentStartDefinition,
           static_cast<unsigned long long>(g_componentStartHandlerRva),
           reinterpret_cast<void*>(object),
           index,
           count,
           enabled,
           static_cast<unsigned long long>(safe_read<std::uint64_t>(address(0x00U), 0U)),
           static_cast<unsigned long long>(safe_read<std::uint64_t>(address(0x08U), 0U)),
           static_cast<unsigned long long>(safe_read<std::uint64_t>(address(0x10U), 0U)),
           static_cast<unsigned long long>(safe_read<std::uint64_t>(address(0x18U), 0U)),
           static_cast<unsigned long long>(safe_read<std::uint64_t>(address(0x20U), 0U)),
           static_cast<unsigned long long>(safe_read<std::uint64_t>(address(0x28U), 0U)),
           static_cast<unsigned long long>(safe_read<std::uint64_t>(address(0x30U), 0U)),
           static_cast<unsigned long long>(safe_read<std::uint64_t>(address(0x38U), 0U)),
           safe_read<std::uint32_t>(address(0x130U), 0U),
           safe_read<std::uint32_t>(address(0x134U), 0U),
           static_cast<unsigned int>(safe_read<std::uint8_t>(address(0x160U), 0U)),
           static_cast<unsigned int>(safe_read<std::uint8_t>(address(0x161U), 0U)),
           static_cast<unsigned int>(safe_read<std::uint8_t>(address(0x162U), 0U)),
           static_cast<unsigned int>(safe_read<std::uint8_t>(address(0x164U), 0U)),
           reinterpret_cast<void*>(safe_read<std::uintptr_t>(address(0x170U), 0U)),
           correlation.readable ? "yes" : "no",
           correlation.releasedMask,
           correlation.objects[0],
           correlation.objects[1],
           correlation.objects[2],
           reinterpret_cast<void*>(correlation.presentationComponents[0]),
           reinterpret_cast<void*>(correlation.presentationComponents[1]),
           reinterpret_cast<void*>(correlation.presentationComponents[2]),
           correlation.objectReferenceMask,
           correlation.objectOffsets[0],
           correlation.objectOffsets[1],
           correlation.objectOffsets[2],
           correlation.presentationReferenceMask,
           correlation.presentationOffsets[0],
           correlation.presentationOffsets[1],
           correlation.presentationOffsets[2],
           correlation.behaviorReferenceMask,
           correlation.directActorMask,
           correlation.directPresentationMask,
           g_componentStartDefinition == kIkoraPresentationDefinition
               ? "presentation_start" : "omega_scene");
}

void report_renderer_array(const char* operation,
                           const char* moment,
                           std::uintptr_t wrapperRva,
                           std::uintptr_t callerRva,
                           std::uintptr_t* objects,
                           std::int32_t count,
                           int enabled) noexcept {
    if (!renderer_handle_capture_active()) {
        return;
    }
    if (objects == nullptr || count < 1 || count > kMaximumRendererObjectArrayCount) {
        const std::uint32_t sample =
            g_rendererHandleSamples.fetch_add(1U, std::memory_order_relaxed) + 1U;
        if (sample > kMaximumRendererHandleSamples) {
            return;
        }
        report("ev=omega_actor_renderer_handle stage=wrapper moment=%s operation=%s "
               "sample=%u wrapper=+%llX caller=+%llX factory_n=%u actor=%08X component=%p "
               "definition=%08X objects=%p count=%d enabled=%d result=invalid_array "
               "scope=omega_scene mutation=observe_only",
               moment,
               operation,
               sample,
               static_cast<unsigned long long>(wrapperRva),
               static_cast<unsigned long long>(callerRva),
               g_componentStartFactorySequence,
               g_componentStartObject,
               reinterpret_cast<void*>(g_componentStartInstance),
               g_componentStartDefinition,
               objects,
               count,
               enabled);
        return;
    }
    for (std::int32_t index = 0; index < count; ++index) {
        const std::uintptr_t object = safe_read<std::uintptr_t>(objects + index, 0U);
        report_renderer_handle(operation,
                               moment,
                               wrapperRva,
                               callerRva,
                               object,
                               index,
                               count,
                               enabled);
    }
}

[[nodiscard]] bool is_scene_two_presentation_body_target(
    const std::byte* component) noexcept {
    if (!kEnableOmegaSceneTwoBodyStateSuppression || component == nullptr) {
        return false;
    }
    const std::uintptr_t instance = reinterpret_cast<std::uintptr_t>(component);
    if (instance != g_sceneTwoPresentationComponent.load(std::memory_order_acquire)
        || safe_read<std::uint32_t>(component, kInvalidHandle)
               != kIkoraPresentationDefinition) {
        return false;
    }
    const std::uint32_t owner = safe_read<std::uint32_t>(component + 0x2CU,
                                                         kInvalidHandle);
    return owner != kInvalidHandle
           && owner == g_sceneTwoPresentationObject.load(std::memory_order_acquire)
           && object_record(owner) != nullptr;
}

bool force_scene_two_presentation_body_off(const char* source,
                                           std::uintptr_t callerRva) noexcept {
    const PresentationBodyState original =
        g_presentationBodyStateOriginal.load(std::memory_order_acquire);
    auto* const component = reinterpret_cast<std::byte*>(
        g_sceneTwoPresentationComponent.load(std::memory_order_acquire));
    if (original == nullptr || !is_scene_two_presentation_body_target(component)) {
        return false;
    }
    const std::uint8_t before = safe_read<std::uint8_t>(component + 0x240U, 0xFFU);
    original(component, 0);
    const std::uint8_t after = safe_read<std::uint8_t>(component + 0x240U, 0xFFU);
    const std::uint32_t force =
        g_sceneTwoBodyStateForces.fetch_add(1U, std::memory_order_relaxed) + 1U;
    if (force <= 16U || before != after) {
        report("ev=omega_scene_two_body_state stage=force_off n=%u source=%s "
               "caller=+%llX component=%p actor=%08X before=%u after=%u "
               "scene=80EC0FA8 scene2_only=yes scene_events=native vfx_paths=native "
               "mutation=force_scene_two_body_state_disabled",
               force,
               source == nullptr ? "unknown" : source,
               static_cast<unsigned long long>(callerRva),
               component,
               g_sceneTwoPresentationObject.load(std::memory_order_acquire),
               static_cast<unsigned int>(before),
               static_cast<unsigned int>(after));
    }
    return after == 0U;
}

__declspec(noinline) void __fastcall presentation_body_state(std::byte* component,
                                                               char enabled) noexcept {
    const PresentationBodyState original =
        g_presentationBodyStateOriginal.load(std::memory_order_acquire);
    if (original == nullptr) {
        return;
    }
    const bool target = is_scene_two_presentation_body_target(component);
    const char applied = target ? 0 : enabled;
    const std::uint8_t before = safe_read<std::uint8_t>(
        component == nullptr ? nullptr : component + 0x240U, 0xFFU);
    original(component, applied);
    if (!target) {
        return;
    }
    const std::uint8_t after = safe_read<std::uint8_t>(component + 0x240U, 0xFFU);
    const std::uint32_t call =
        g_sceneTwoBodyStateCalls.fetch_add(1U, std::memory_order_relaxed) + 1U;
    const bool overridden = enabled != 0;
    if (overridden) {
        g_sceneTwoBodyStateOverrides.fetch_add(1U, std::memory_order_relaxed);
    }
    if (call <= 32U || overridden || before != after) {
        report("ev=omega_scene_two_body_state stage=native_request n=%u "
               "caller=+%llX component=%p actor=%08X requested=%d applied=%d "
               "before=%u after=%u action=%s scene=80EC0FA8 scene2_only=yes "
               "scene_events=native vfx_paths=native "
               "mutation=override_scene_two_body_enable",
               call,
               static_cast<unsigned long long>(image_rva(
                   reinterpret_cast<std::uintptr_t>(_ReturnAddress()))),
               component,
               g_sceneTwoPresentationObject.load(std::memory_order_acquire),
               static_cast<int>(enabled),
               static_cast<int>(applied),
               static_cast<unsigned int>(before),
               static_cast<unsigned int>(after),
               overridden ? "force_disabled" : "native_disabled");
    }
}

__declspec(noinline) void __fastcall renderer_bulk_boolean(std::uintptr_t* objects,
                                                             std::int32_t count,
                                                            char enabled) noexcept {
    const RendererBulkBoolean original =
        g_rendererBulkBooleanOriginal.load(std::memory_order_acquire);
    if (original == nullptr) {
        return;
    }
    if (g_rendererHandleDepth != 0U) {
        original(objects, count, enabled);
        return;
    }
    ++g_rendererHandleDepth;
    const std::uintptr_t callerRva = image_rva(
        reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
    report_renderer_array("bulk_boolean", "before", kRendererBulkBooleanRva,
                          callerRva, objects, count, static_cast<int>(enabled));
    std::array<std::uintptr_t, kMaximumRendererObjectArrayCount> sceneTwoRenderers{};
    const std::int32_t sceneTwoRendererCount =
        collect_scene_two_presentation_renderers(sceneTwoRenderers);
    std::array<std::uintptr_t, kMaximumRendererObjectArrayCount> nativeObjects{};
    std::array<std::uintptr_t, kMaximumRendererObjectArrayCount> suppressedObjects{};
    std::int32_t nativeCount = 0;
    std::int32_t suppressedCount = 0;
    if (enabled != 0 && objects != nullptr && count > 0
        && count <= kMaximumRendererObjectArrayCount && sceneTwoRendererCount > 0) {
        for (std::int32_t index = 0; index < count; ++index) {
            const std::uintptr_t object = safe_read<std::uintptr_t>(objects + index, 0U);
            const bool suppress = std::find(sceneTwoRenderers.begin(),
                                            sceneTwoRenderers.begin() + sceneTwoRendererCount,
                                            object)
                                  != sceneTwoRenderers.begin() + sceneTwoRendererCount;
            if (suppress) {
                suppressedObjects[static_cast<std::size_t>(suppressedCount++)] = object;
            } else {
                nativeObjects[static_cast<std::size_t>(nativeCount++)] = object;
            }
        }
    }
    if (suppressedCount > 0) {
        if (nativeCount > 0) {
            original(nativeObjects.data(), nativeCount, enabled);
        }
        original(suppressedObjects.data(), suppressedCount, 0);
        report("ev=omega_scene_two_presentation_suppression stage=enable_override "
               "caller=+%llX requested_enabled=%d native_objects=%d "
               "suppressed_objects=%d action=force_disabled "
               "mutation=override_scene_two_renderer_enable",
               static_cast<unsigned long long>(callerRva),
               static_cast<int>(enabled),
               nativeCount,
               suppressedCount);
    } else {
        original(objects, count, enabled);
    }
    (void)disable_scene_two_presentation_renderers("bulk_boolean_after", callerRva);
    report_renderer_array("bulk_boolean", "after", kRendererBulkBooleanRva,
                          callerRva, objects, count, static_cast<int>(enabled));
    --g_rendererHandleDepth;
}

__declspec(noinline) void __fastcall renderer_bulk_sibling(std::uintptr_t* objects,
                                                            std::int32_t count) noexcept {
    const RendererBulkSibling original =
        g_rendererBulkSiblingOriginal.load(std::memory_order_acquire);
    if (original == nullptr) {
        return;
    }
    if (g_rendererHandleDepth != 0U) {
        original(objects, count);
        return;
    }
    ++g_rendererHandleDepth;
    const std::uintptr_t callerRva = image_rva(
        reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
    report_renderer_array("bulk_sibling", "before", kRendererBulkSiblingRva,
                          callerRva, objects, count, -1);
    original(objects, count);
    report_renderer_array("bulk_sibling", "after", kRendererBulkSiblingRva,
                          callerRva, objects, count, -1);
    --g_rendererHandleDepth;
}

void renderer_object_call(RendererObject original,
                          const char* operation,
                          std::uintptr_t wrapperRva,
                          std::uintptr_t callerRva,
                          std::byte* object) noexcept {
    if (original == nullptr) {
        return;
    }
    if (g_rendererHandleDepth != 0U) {
        original(object);
        return;
    }
    ++g_rendererHandleDepth;
    report_renderer_handle(operation, "before", wrapperRva, callerRva,
                           reinterpret_cast<std::uintptr_t>(object), 0, 1, -1);
    original(object);
    report_renderer_handle(operation, "after", wrapperRva, callerRva,
                           reinterpret_cast<std::uintptr_t>(object), 0, 1, -1);
    --g_rendererHandleDepth;
}

__declspec(noinline) void __fastcall renderer_object_a(std::byte* object) noexcept {
    renderer_object_call(g_rendererObjectAOriginal.load(std::memory_order_acquire),
                         "object_a",
                         kRendererObjectARva,
                         image_rva(reinterpret_cast<std::uintptr_t>(_ReturnAddress())),
                         object);
}

__declspec(noinline) void __fastcall renderer_object_b(std::byte* object) noexcept {
    renderer_object_call(g_rendererObjectBOriginal.load(std::memory_order_acquire),
                         "object_b",
                         kRendererObjectBRva,
                         image_rva(reinterpret_cast<std::uintptr_t>(_ReturnAddress())),
                         object);
}

__declspec(noinline) void __fastcall renderer_object_c(std::byte* object) noexcept {
    renderer_object_call(g_rendererObjectCOriginal.load(std::memory_order_acquire),
                         "object_c",
                         kRendererObjectCRva,
                         image_rva(reinterpret_cast<std::uintptr_t>(_ReturnAddress())),
                         object);
}

__declspec(noinline) void __fastcall component_start(const std::uintptr_t* entry,
                                                       float elapsed,
                                                       std::uintptr_t context,
                                                       std::uintptr_t auxiliary) noexcept {
    const ComponentStart original = g_componentStartOriginal.load(std::memory_order_acquire);
    if (original == nullptr) {
        return;
    }
    const std::uintptr_t instance = safe_read<std::uintptr_t>(entry + 1, 0U);
    const std::uint32_t definition = safe_read<std::uint32_t>(
        reinterpret_cast<const void*>(instance), kInvalidHandle);
    const std::uintptr_t handlerRva = component_handler_rva(entry);
    const std::size_t watched = watched_component_index(definition);
    const bool ikoraEntityComponent = g_ikoraFactoryDepth != 0U;
    const std::uint32_t factorySequence =
        ikoraEntityComponent ? g_ikoraFactorySequence : 0U;
    const std::uint64_t encodedComponentSize = safe_read<std::uint64_t>(
        reinterpret_cast<const void*>(instance + 0x08U), 0U);
    const std::size_t componentSize = encodedComponentSize >= 0x20U
                                              && encodedComponentSize <= 0x10000U
                                          ? static_cast<std::size_t>(encodedComponentSize)
                                          : 0U;
    // The factory's thread-local object is updated by nested object finalizers. The component's
    // own +0x2C owner remains the stable root actor across all three lifecycle handlers.
    const std::uint32_t componentOwnedObject = safe_read<std::uint32_t>(
        reinterpret_cast<const void*>(instance + 0x2CU), kInvalidHandle);
    const std::uint32_t ownedObject = componentOwnedObject != kInvalidHandle
                                          ? componentOwnedObject
                                          : g_ikoraFactoryObject;
    const std::uint32_t sceneHandle = g_sceneActorSchedulerDepth != 0U
                                              && g_sceneActorSchedulerScene != nullptr
                                          ? safe_read<std::uint32_t>(
                                                g_sceneActorSchedulerScene, kInvalidHandle)
                                          : kInvalidHandle;
    const bool sceneTwoPrimaryModelBuild =
        kEnableOmegaSceneTwoModelSuppression && ikoraEntityComponent
        && sceneHandle == kOmegaIkoraSceneTwoHandle
        && definition == kIkoraPrimaryModelDefinition
        && handlerRva == kIkoraPrimaryModelBuildRva;
    const bool sceneTwoMeshClothModelBuild =
        kEnableOmegaSceneTwoModelSuppression && ikoraEntityComponent
        && sceneHandle == kOmegaIkoraSceneTwoHandle
        && definition == kIkoraMeshClothModelDefinition
        && handlerRva == kIkoraMeshClothModelBuildRva;
    // The head is authored as a nested child object, so its +0x2C owner differs from the main
    // Ikora object even though it is constructed under the same scene-2 factory TLS scope.
    const bool sceneTwoHeadModelBuild =
        kEnableOmegaSceneTwoModelSuppression && ikoraEntityComponent
        && sceneHandle == kOmegaIkoraSceneTwoHandle
        && definition == kIkoraHeadModelDefinition
        && handlerRva == kIkoraHeadModelBuildRva;
    const bool suppressSceneTwoModelBuild =
        sceneTwoPrimaryModelBuild || sceneTwoMeshClothModelBuild
        || sceneTwoHeadModelBuild;
    const bool sceneTwoPresentation =
        (kEnableOmegaSceneTwoPresentationSuppression
         || kEnableOmegaSceneTwoBodyStateSuppression)
        && ikoraEntityComponent && sceneHandle == kOmegaIkoraSceneTwoHandle
        && definition == kIkoraPresentationDefinition;
    if (sceneTwoPresentation) {
        const std::uintptr_t previous =
            g_sceneTwoPresentationComponent.exchange(instance, std::memory_order_acq_rel);
        g_sceneTwoPresentationObject.store(ownedObject, std::memory_order_release);
        const std::uint32_t capture =
            g_sceneTwoPresentationCaptures.fetch_add(1U, std::memory_order_relaxed) + 1U;
        if (previous != instance) {
            for (auto& renderer : g_sceneTwoPresentationRenderers) {
                renderer.store(0U, std::memory_order_release);
            }
            report("ev=omega_scene_two_presentation_suppression stage=capture n=%u "
                   "scene=%08X factory_n=%u component=%p definition=%08X "
                   "handler=+%llX actor=%08X body_state=%u "
                   "action=track_scene_two_presentation scene2_only=yes "
                   "scene_events=native vfx_paths=native "
                   "mutation=track_for_body_state_suppression",
                   capture,
                   sceneHandle,
                   factorySequence,
                   reinterpret_cast<void*>(instance),
                   definition,
                   static_cast<unsigned long long>(handlerRva),
                   ownedObject,
                   static_cast<unsigned int>(safe_read<std::uint8_t>(
                       reinterpret_cast<const void*>(instance + 0x240U), 0xFFU)));
        }
    }
    if (watched < kWatchedComponents.size()) {
        const std::uint64_t before = hash_region(
            reinterpret_cast<const void*>(instance), 0x100U);
        g_watchedComponentPointers[watched].store(instance, std::memory_order_release);
        g_watchedComponentHashes[watched].store(before, std::memory_order_release);
        report("ev=omega_ikora_followup stage=authored_component_start moment=before "
               "name=%s definition=%08X component=%p handler=+%llX object=%08X "
               "hash=%016llX mutation=observe_only",
               kWatchedComponents[watched].name,
               definition,
               reinterpret_cast<void*>(instance),
               static_cast<unsigned long long>(handlerRva),
               safe_read<std::uint32_t>(reinterpret_cast<const void*>(instance + 0x2CU),
                                        kInvalidHandle),
               static_cast<unsigned long long>(before));
    }
    if (ikoraEntityComponent) {
        remember_ikora_component(instance,
                                 definition,
                                 handlerRva,
                                 factorySequence,
                                 ownedObject,
                                 componentSize);
        report("ev=omega_ikora_followup stage=entity_component_start moment=before "
               "factory_n=%u caller=+%llX component=%p definition=%08X handler=+%llX "
               "object=%08X elapsed=%.6f context=%p auxiliary=%p "
               "raw=%016llX,%016llX,%016llX,%016llX,%016llX,%016llX,"
               "%016llX,%016llX mutation=observe_only",
               factorySequence,
               static_cast<unsigned long long>(g_ikoraFactoryCallerRva),
               reinterpret_cast<void*>(instance),
               definition,
               static_cast<unsigned long long>(handlerRva),
               ownedObject,
               static_cast<double>(elapsed),
               reinterpret_cast<void*>(context),
               reinterpret_cast<void*>(auxiliary),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(
                   reinterpret_cast<const void*>(instance + 0x00U), 0U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(
                   reinterpret_cast<const void*>(instance + 0x08U), 0U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(
                   reinterpret_cast<const void*>(instance + 0x10U), 0U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(
                   reinterpret_cast<const void*>(instance + 0x18U), 0U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(
                   reinterpret_cast<const void*>(instance + 0x20U), 0U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(
                   reinterpret_cast<const void*>(instance + 0x28U), 0U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(
                   reinterpret_cast<const void*>(instance + 0x30U), 0U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(
                   reinterpret_cast<const void*>(instance + 0x38U), 0U)));
    }
    const std::uintptr_t previousInstance = g_componentStartInstance;
    const std::uint32_t previousDefinition = g_componentStartDefinition;
    const std::uintptr_t previousHandlerRva = g_componentStartHandlerRva;
    const std::uint32_t previousFactorySequence = g_componentStartFactorySequence;
    const std::uint32_t previousObject = g_componentStartObject;
    if (ikoraEntityComponent) {
        g_componentStartInstance = instance;
        g_componentStartDefinition = definition;
        g_componentStartHandlerRva = handlerRva;
        g_componentStartFactorySequence = factorySequence;
        g_componentStartObject = ownedObject;
    }
    if (suppressSceneTwoModelBuild) {
        const std::uint32_t suppression =
            g_sceneTwoModelSuppressions.fetch_add(1U, std::memory_order_relaxed) + 1U;
        report("ev=omega_scene_two_model_suppression stage=component_dispatch n=%u "
               "scene=%08X factory_n=%u component=%p definition=%08X "
               "handler=+%llX actor=%08X model=%s "
               "action=skip_native_render_model_construction "
               "scene_events=native animation_graph=native vfx_paths=native "
               "mutation=suppress_scene_two_model_builder",
               suppression,
               sceneHandle,
               factorySequence,
               reinterpret_cast<void*>(instance),
               definition,
               static_cast<unsigned long long>(handlerRva),
               ownedObject,
               sceneTwoPrimaryModelBuild
                   ? "primary"
                   : sceneTwoMeshClothModelBuild ? "mesh_cloth" : "head_child");
    } else {
        original(entry, elapsed, context, auxiliary);
    }
    if (sceneTwoPresentation && kEnableOmegaSceneTwoBodyStateSuppression) {
        (void)force_scene_two_presentation_body_off(
            "component_lifecycle_after", handlerRva);
    }
    if (sceneTwoPresentation && kEnableOmegaSceneTwoPresentationSuppression) {
        (void)disable_scene_two_presentation_renderers(
            "component_lifecycle_after", handlerRva);
    }
    g_componentStartInstance = previousInstance;
    g_componentStartDefinition = previousDefinition;
    g_componentStartHandlerRva = previousHandlerRva;
    g_componentStartFactorySequence = previousFactorySequence;
    g_componentStartObject = previousObject;
    if (watched < kWatchedComponents.size()) {
        const std::uint64_t after = hash_region(
            reinterpret_cast<const void*>(instance), 0x100U);
        g_watchedComponentHashes[watched].store(after, std::memory_order_release);
        report("ev=omega_ikora_followup stage=authored_component_start moment=after "
               "name=%s definition=%08X component=%p handler=+%llX hash=%016llX "
               "mutation=observe_only",
               kWatchedComponents[watched].name,
               definition,
               reinterpret_cast<void*>(instance),
               static_cast<unsigned long long>(handlerRva),
               static_cast<unsigned long long>(after));
    }
}

__declspec(noinline) void __fastcall behavior_step(const std::byte* actor,
                                                    std::uintptr_t context) noexcept {
    // This hook is a normal gameplay update boundary and remains active after the cinematic
    // performer is released, making it a safe place to leave allocator teardown before Actor 3.
    service_deferred_scene_completion();
    const BehaviorStep original = g_behaviorStepOriginal.load(std::memory_order_acquire);
    if (original == nullptr) {
        return;
    }
    const std::uint32_t object = g_ikoraObject.load(std::memory_order_acquire);
    if (object != kInvalidHandle) {
        remember_ikora_behavior_tick(actor);
    }
    const std::uint32_t entity = g_ikoraEntity.load(std::memory_order_acquire);
    const bool armed = omega_forced() && object != kInvalidHandle;
    const std::uint32_t sequence = armed
                                       ? g_behaviorCalls.fetch_add(1, std::memory_order_relaxed) + 1U
                                       : 0U;
    const std::uintptr_t known = g_ikoraBehaviorActor.load(std::memory_order_acquire);
    const std::uintptr_t handoffKnown =
        g_ikoraHandoffBehaviorActor.load(std::memory_order_acquire);
    const std::uint32_t sceneObject =
        g_sceneAnimatedObject.load(std::memory_order_acquire);
    const bool handoffCandidate = known != 0U && object != sceneObject;
    const bool scan = armed && sequence <= 30'000U
                      && (known == 0U || (handoffCandidate && handoffKnown == 0U));
    const HandleMatch match = scan
                                  ? find_handles(actor, kBehaviorHandleScanBytes, object, entity)
                                  : HandleMatch{};
    const bool direct = static_cast<std::uint32_t>(context) == object
                        || (entity != kInvalidHandle
                            && static_cast<std::uint32_t>(context) == entity);
    const bool newPrimary = armed && known == 0U && (match.any() || direct);
    const bool newHandoff = armed && handoffCandidate && handoffKnown == 0U
                            && (match.any() || direct);
    const bool primaryMatch = actor == reinterpret_cast<const std::byte*>(known);
    const bool handoffMatch = actor == reinterpret_cast<const std::byte*>(handoffKnown);
    const bool inspect = armed
                         && (primaryMatch || handoffMatch || newPrimary || newHandoff);
    if (newPrimary) {
        g_ikoraBehaviorActor.store(reinterpret_cast<std::uintptr_t>(actor),
                                   std::memory_order_release);
        remember_ikora_behavior(object, actor);
        report_native_stack("behavior_primary", ikora_factory_sequence(object), object);
        std::uint32_t expected = kInvalidHandle;
        if (g_sceneAnimatedObject.compare_exchange_strong(expected,
                                                          object,
                                                          std::memory_order_acq_rel,
                                                          std::memory_order_acquire)) {
            report("ev=omega_scene_lifecycle stage=actor_identified factory_n=%u "
                   "object=%08X entity=%08X actor=%p result=armed mutation=observe_only",
                   ikora_factory_sequence(object),
                   object,
                   entity,
                   actor);
        }
    }
    if (newHandoff) {
        g_ikoraHandoffBehaviorActor.store(reinterpret_cast<std::uintptr_t>(actor),
                                          std::memory_order_release);
        remember_ikora_behavior(object, actor);
        report_native_stack("behavior_persistent", ikora_factory_sequence(object), object);
        report("ev=omega_ikora_handoff stage=behavior_identified factory_n=%u "
               "object=%08X entity=%08X actor=%p scene_object=%08X "
               "result=secondary mutation=observe_only",
               ikora_factory_sequence(object),
               object,
               entity,
               actor,
               sceneObject);
    }
    if (inspect) {
        const std::uint32_t matchSequence =
            g_behaviorMatches.fetch_add(1, std::memory_order_relaxed) + 1U;
        const bool reportMatch = matchSequence <= 32U || matchSequence % 300U == 0U;
        ++g_ikoraBehaviorDepth;
        if (reportMatch) {
            report("ev=omega_ikora_followup stage=behavior_step moment=before n=%u match_n=%u "
                   "role=%s actor=%p context=%p object=%08X entity=%08X object_offset=%d "
                   "entity_offset=%d direct=%u mutation=observe_only",
                   sequence,
                   matchSequence,
                   handoffMatch || newHandoff ? "persistent" : "scene",
                   actor,
                   reinterpret_cast<void*>(context),
                   object,
                   entity,
                   match.objectOffset,
                   match.entityOffset,
                   direct ? 1U : 0U);
        }
    } else if (armed && sequence <= kMaximumBehaviorCandidateLogs) {
        report("ev=omega_ikora_followup stage=behavior_candidate n=%u actor=%p context=%p "
               "object=%08X entity=%08X object_offset=%d entity_offset=%d "
               "raw=%016llX,%016llX,%016llX,%016llX mutation=observe_only",
               sequence,
               actor,
               reinterpret_cast<void*>(context),
               object,
               entity,
               match.objectOffset,
               match.entityOffset,
               static_cast<unsigned long long>(safe_read<std::uint64_t>(actor + 0x00U, 0U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(actor + 0x08U, 0U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(actor + 0x10U, 0U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(actor + 0x18U, 0U)));
    }
    const std::uint32_t sceneTrackedObject = sceneObject != kInvalidHandle
                                                 ? sceneObject
                                                 : object;
    const std::uint32_t sceneFactorySequence =
        newPrimary || primaryMatch ? ikora_factory_sequence(sceneTrackedObject) : 0U;
    if (sceneFactorySequence != 0U) {
        capture_scene_handoff_components(sceneFactorySequence, 0U);
    }
    original(actor, context);
    if (sceneFactorySequence != 0U) {
        capture_scene_handoff_components(sceneFactorySequence, 1U);
    }
    if (inspect) {
        --g_ikoraBehaviorDepth;
    }
}

__declspec(noinline) void __fastcall behavior_node(const std::byte* node,
                                                    const std::uint32_t* context) noexcept {
    const BehaviorNode original = g_behaviorNodeOriginal.load(std::memory_order_acquire);
    if (original == nullptr) {
        return;
    }
    const bool inspect = g_ikoraBehaviorDepth != 0U;
    const std::uint32_t sequence = inspect
                                       ? g_behaviorNodeCalls.fetch_add(1, std::memory_order_relaxed) + 1U
                                       : 0U;
    if (inspect && sequence <= kMaximumBehaviorNodeLogs) {
        const std::uint64_t relative = safe_read<std::uint64_t>(node + 0x08U, 0U);
        const std::uint32_t nodeClass = relative == 0U || relative > 0x10000000U
                                            ? kInvalidHandle
                                            : safe_read<std::uint32_t>(
                                                  node + relative + 0x04U, kInvalidHandle);
        report("ev=omega_ikora_followup stage=behavior_node n=%u node=%p context=%p "
               "class=%08X relative=%016llX context_head=%08X,%08X,%08X,%08X "
               "mutation=observe_only",
               sequence,
               node,
               context,
               nodeClass,
               static_cast<unsigned long long>(relative),
               safe_read<std::uint32_t>(context + 0, kInvalidHandle),
               safe_read<std::uint32_t>(context + 1, kInvalidHandle),
               safe_read<std::uint32_t>(context + 2, kInvalidHandle),
               safe_read<std::uint32_t>(context + 3, kInvalidHandle));
    }
    original(node, context);
}

/**
 * Records the concrete pose object that resolves the purple socket key.
 *
 * +B314A0 is a pure tail-dispatch thunk: interface[0] is its dispatch base and interface[1] is
 * the concrete pose instance forwarded as RCX. Once +A1E8A0 teaches us the authored key, this
 * hook samples every other native resolution of that same key and correlates the pose instance
 * against the captured scene-1/2/3 component and presentation graphs. This build never replaces
 * the interface; it first requires a unique, structurally compatible scene-1 observation.
 */
__declspec(noinline) void __fastcall pose_socket_dispatch(
    const std::uintptr_t* poseInterface,
    std::uint32_t bindingKey,
    std::uint8_t flags,
    std::byte* output) noexcept {
    const SceneTransformRecorderCallGuard activeCall{
        g_poseSocketDispatchActiveCalls};
    const PoseSocketDispatch original =
        g_poseSocketDispatchOriginal.load(std::memory_order_acquire);
    if (original == nullptr) {
        return;
    }

    const bool exactVfxCall = g_poseSocketExactVfxDepth != 0U;
    const std::uint64_t now = GetTickCount64();
    const std::uint32_t trackedBindingKey =
        g_scenePoseBindingKey.load(std::memory_order_acquire);
    const std::uint32_t comparisonBindingKey =
        trackedBindingKey == kInvalidHandle
            ? kOmegaPurplePoseBindingKey
            : trackedBindingKey;
    const bool sceneOneActivated =
        (g_sceneActivationObservedMask.load(std::memory_order_acquire) & 0x1U)
        != 0U;
    if ((!exactVfxCall && !sceneOneActivated)
        || (!exactVfxCall && bindingKey != comparisonBindingKey)
        || !omega_forced()) {
        original(poseInterface, bindingKey, flags, output);
        return;
    }

    const std::uintptr_t callerRva = image_rva(
        reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
    const std::uintptr_t dispatchBase = safe_read<std::uintptr_t>(
        poseInterface, 0U);
    const std::uintptr_t poseObject = safe_read<std::uintptr_t>(
        poseInterface == nullptr ? nullptr : poseInterface + 1U, 0U);
    const std::int64_t dispatchRelative = safe_read<std::int64_t>(
        reinterpret_cast<const void*>(
            dispatchBase == 0U ? 0U : dispatchBase + 0x18U),
        INT64_MIN);
    std::uintptr_t dispatchTarget = 0U;
    if (dispatchBase != 0U && dispatchRelative != INT64_MIN
        && dispatchRelative > -0x1000000LL && dispatchRelative < 0x1000000LL) {
        const std::uintptr_t dispatchTable = add_relative(
            dispatchBase, dispatchRelative);
        dispatchTarget = safe_read<std::uintptr_t>(
            reinterpret_cast<const void*>(
                dispatchTable == 0U ? 0U : dispatchTable + 0x48U),
            0U);
    }

    if (exactVfxCall) {
        g_poseSocketVfxDispatchBase.store(dispatchBase, std::memory_order_release);
        g_poseSocketVfxDispatchTarget.store(dispatchTarget, std::memory_order_release);
        g_poseSocketVfxObject.store(poseObject, std::memory_order_release);
        g_poseSocketVfxLastCallTick.store(now, std::memory_order_release);
    }
    const PoseSocketTraceKey traceKey{
        callerRva, dispatchBase, poseObject, bindingKey, exactVfxCall};
    const bool firstPath = remember_pose_socket_trace_key(traceKey);
    const std::uint32_t call = g_poseSocketDispatchCalls.fetch_add(
                                   1U, std::memory_order_relaxed)
                               + 1U;
    std::uint64_t previous = g_poseSocketDispatchLastTick.load(
        std::memory_order_acquire);
    bool heartbeat = false;
    while (!firstPath && now - previous >= kPoseSocketHeartbeatMs) {
        if (g_poseSocketDispatchLastTick.compare_exchange_weak(
                previous,
                now,
                std::memory_order_acq_rel,
                std::memory_order_acquire)) {
            heartbeat = true;
            break;
        }
    }
    const bool sampleCall = firstPath || heartbeat;
    const DecodedSceneTransform before = sampleCall
                                             ? decode_scene_transform_address(output)
                                             : DecodedSceneTransform{};
    original(poseInterface, bindingKey, flags, output);
    DecodedSceneTransform exactVfxAfter{};
    if (exactVfxCall) {
        exactVfxAfter = decode_scene_transform_address(output);
        publish_vfx_socket_transform(exactVfxAfter, now);
    }
    if (!sampleCall) {
        return;
    }

    const std::uint32_t sample = g_poseSocketDispatchSamples.fetch_add(
                                     1U, std::memory_order_relaxed)
                                 + 1U;
    if (sample > kMaximumPoseSocketDispatchSamples) {
        return;
    }
    if (firstPath) {
        g_poseSocketDispatchLastTick.store(now, std::memory_order_release);
    }

    const DecodedSceneTransform after = exactVfxCall
                                            ? exactVfxAfter
                                            : decode_scene_transform_address(output);
    const PosePointerCorrelation correlation = correlate_pose_pointer(poseObject);
    const IkoraComponentAddressOwner interfaceOwner =
        locate_ikora_component_address(poseInterface);
    const IkoraComponentAddressOwner dispatchOwner =
        locate_ikora_component_address(
            reinterpret_cast<const void*>(dispatchBase));
    const IkoraComponentAddressOwner poseOwner =
        locate_ikora_component_address(
            reinterpret_cast<const void*>(poseObject));
    const std::uint32_t evidenceMask = correlation.rendererActorMask != 0U
                                           ? correlation.rendererActorMask
                                           : correlation.presentationActorMask != 0U
                                                 ? correlation.presentationActorMask
                                                 : correlation.componentActorMask;
    const bool uniqueEvidence = evidenceMask != 0U
                                && (evidenceMask & (evidenceMask - 1U)) == 0U;
    const char* role = exactVfxCall
                           ? "exact_vfx_pose"
                           : uniqueEvidence && evidenceMask == 0x1U
                                 ? "scene1_pose_candidate"
                                 : uniqueEvidence && evidenceMask == 0x2U
                                       ? "scene2_pose_candidate"
                                       : uniqueEvidence && evidenceMask == 0x4U
                                             ? "scene3_pose_candidate"
                                             : "unclassified_pose_candidate";
    const std::uintptr_t vfxDispatchBase =
        g_poseSocketVfxDispatchBase.load(std::memory_order_acquire);
    const std::uintptr_t vfxDispatchTarget =
        g_poseSocketVfxDispatchTarget.load(std::memory_order_acquire);
    const std::uintptr_t vfxPoseObject =
        g_poseSocketVfxObject.load(std::memory_order_acquire);

    std::array<void*, 8U> frames{};
    const USHORT depth = RtlCaptureStackBackTrace(
        1U,
        static_cast<ULONG>(frames.size()),
        frames.data(),
        nullptr);
    std::array<std::uintptr_t, 8U> frameRvas{};
    for (USHORT index = 0U; index < depth; ++index) {
        frameRvas[index] = image_rva(
            reinterpret_cast<std::uintptr_t>(frames[index]));
    }

    report("ev=omega_pose_socket_dispatch stage=identity n=%u call=%u role=%s "
           "exact_vfx=%u first_path=%u heartbeat=%u caller=+%llX "
           "binding_key=%08X flags=%02X pose_interface=%p "
           "dispatch_base=%p dispatch_relative=%lld dispatch_target=%p pose_object=%p "
           "vfx_dispatch_base=%p vfx_dispatch_target=%p vfx_pose_object=%p "
           "same_dispatch=%u same_target=%u same_pose=%u "
           "interface_owner=%u:%08X:+0x%llX dispatch_owner=%u:%08X:+0x%llX "
           "pose_owner=%u:%08X:+0x%llX "
           "pose_words=%016llX,%016llX,%016llX,%016llX "
           "stack_depth=%u rvas=+%llX,+%llX,+%llX,+%llX,+%llX,+%llX,+%llX,+%llX "
           "mutation=observe_only",
           sample,
           call,
           role,
           exactVfxCall ? 1U : 0U,
           firstPath ? 1U : 0U,
           heartbeat ? 1U : 0U,
           static_cast<unsigned long long>(callerRva),
           bindingKey,
           static_cast<unsigned int>(flags),
           poseInterface,
           reinterpret_cast<void*>(dispatchBase),
           static_cast<long long>(dispatchRelative),
           reinterpret_cast<void*>(dispatchTarget),
           reinterpret_cast<void*>(poseObject),
           reinterpret_cast<void*>(vfxDispatchBase),
           reinterpret_cast<void*>(vfxDispatchTarget),
           reinterpret_cast<void*>(vfxPoseObject),
           dispatchBase != 0U && dispatchBase == vfxDispatchBase ? 1U : 0U,
           dispatchTarget != 0U && dispatchTarget == vfxDispatchTarget ? 1U : 0U,
           poseObject != 0U && poseObject == vfxPoseObject ? 1U : 0U,
           interfaceOwner.factorySequence,
           interfaceOwner.definition,
           static_cast<unsigned long long>(interfaceOwner.offset),
           dispatchOwner.factorySequence,
           dispatchOwner.definition,
           static_cast<unsigned long long>(dispatchOwner.offset),
           poseOwner.factorySequence,
           poseOwner.definition,
           static_cast<unsigned long long>(poseOwner.offset),
           static_cast<unsigned long long>(safe_read<std::uint64_t>(
               reinterpret_cast<const void*>(poseObject + 0x00U), 0U)),
           static_cast<unsigned long long>(safe_read<std::uint64_t>(
               reinterpret_cast<const void*>(poseObject + 0x08U), 0U)),
           static_cast<unsigned long long>(safe_read<std::uint64_t>(
               reinterpret_cast<const void*>(poseObject + 0x10U), 0U)),
           static_cast<unsigned long long>(safe_read<std::uint64_t>(
               reinterpret_cast<const void*>(poseObject + 0x18U), 0U)),
           static_cast<unsigned int>(depth),
           static_cast<unsigned long long>(frameRvas[0]),
           static_cast<unsigned long long>(frameRvas[1]),
           static_cast<unsigned long long>(frameRvas[2]),
           static_cast<unsigned long long>(frameRvas[3]),
           static_cast<unsigned long long>(frameRvas[4]),
           static_cast<unsigned long long>(frameRvas[5]),
           static_cast<unsigned long long>(frameRvas[6]),
           static_cast<unsigned long long>(frameRvas[7]));

    report("ev=omega_pose_socket_dispatch stage=correlation n=%u call=%u role=%s "
           "pose_object=%p component_mask=%u component_counts=%u,%u,%u "
           "presentation_mask=%u presentation_counts=%u,%u,%u "
           "renderer_mask=%u renderer_counts=%u,%u,%u evidence_mask=%u unique=%u "
           "first_components=%p,%p,%p first_definitions=%08X,%08X,%08X "
           "first_component_offsets=0x%X,0x%X,0x%X "
           "first_presentation_components=%p,%p,%p "
           "first_fingerprint_sources=%s,%s,%s "
           "first_fingerprint_offsets=0x%X,0x%X,0x%X "
           "evidence=exact_u64_pointer_equality mutation=observe_only",
           sample,
           call,
           role,
           reinterpret_cast<void*>(poseObject),
           correlation.componentActorMask,
           correlation.componentCounts[0],
           correlation.componentCounts[1],
           correlation.componentCounts[2],
           correlation.presentationActorMask,
           correlation.presentationCounts[0],
           correlation.presentationCounts[1],
           correlation.presentationCounts[2],
           correlation.rendererActorMask,
           correlation.rendererCounts[0],
           correlation.rendererCounts[1],
           correlation.rendererCounts[2],
           evidenceMask,
           uniqueEvidence ? 1U : 0U,
           reinterpret_cast<void*>(correlation.firstComponents[0]),
           reinterpret_cast<void*>(correlation.firstComponents[1]),
           reinterpret_cast<void*>(correlation.firstComponents[2]),
           correlation.firstDefinitions[0],
           correlation.firstDefinitions[1],
           correlation.firstDefinitions[2],
           correlation.firstComponentOffsets[0],
           correlation.firstComponentOffsets[1],
           correlation.firstComponentOffsets[2],
           reinterpret_cast<void*>(correlation.firstPresentationComponents[0]),
           reinterpret_cast<void*>(correlation.firstPresentationComponents[1]),
           reinterpret_cast<void*>(correlation.firstPresentationComponents[2]),
           correlation.presentationCounts[0] == 0U
               ? "none"
               : presentation_fingerprint_source_name(
                     correlation.firstFingerprintSources[0]),
           correlation.presentationCounts[1] == 0U
               ? "none"
               : presentation_fingerprint_source_name(
                     correlation.firstFingerprintSources[1]),
           correlation.presentationCounts[2] == 0U
               ? "none"
               : presentation_fingerprint_source_name(
                     correlation.firstFingerprintSources[2]),
           correlation.firstFingerprintOffsets[0],
           correlation.firstFingerprintOffsets[1],
           correlation.firstFingerprintOffsets[2]);

    const IkoraActor actor1 = ikora_factory_actor(1U);
    const IkoraActor actor2 = ikora_factory_actor(2U);
    const IkoraActor actor3 = ikora_factory_actor(3U);
    const DecodedObjectPosition root1 = decode_object_position(actor1.object);
    const DecodedObjectPosition root2 = decode_object_position(actor2.object);
    const DecodedObjectPosition root3 = decode_object_position(actor3.object);
    report("ev=omega_pose_socket_dispatch stage=transform n=%u call=%u role=%s "
           "binding_key=%08X output=%p before_readable=%u before_finite=%u "
           "before_rotation=%.6f,%.6f,%.6f,%.6f "
           "before_position=%.3f,%.3f,%.3f before_scale=%.6f "
           "after_readable=%u after_finite=%u "
           "after_rotation=%.6f,%.6f,%.6f,%.6f "
           "after_position=%.3f,%.3f,%.3f after_scale=%.6f "
           "actors=%08X,%08X,%08X actor_current=%u,%u,%u "
           "roots_valid=%u,%u,%u roots=%.3f,%.3f,%.3f|%.3f,%.3f,%.3f|%.3f,%.3f,%.3f "
           "mutation=observe_only",
           sample,
           call,
           role,
           bindingKey,
           output,
           before.readable ? 1U : 0U,
           before.finite ? 1U : 0U,
           before.rotation[0],
           before.rotation[1],
           before.rotation[2],
           before.rotation[3],
           before.position[0],
           before.position[1],
           before.position[2],
           before.scale,
           after.readable ? 1U : 0U,
           after.finite ? 1U : 0U,
           after.rotation[0],
           after.rotation[1],
           after.rotation[2],
           after.rotation[3],
           after.position[0],
           after.position[1],
           after.position[2],
           after.scale,
           actor1.object,
           actor2.object,
           actor3.object,
           !actor1.released && object_handle_is_current(actor1.object) ? 1U : 0U,
           !actor2.released && object_handle_is_current(actor2.object) ? 1U : 0U,
           !actor3.released && object_handle_is_current(actor3.object) ? 1U : 0U,
           root1.valid ? 1U : 0U,
           root2.valid ? 1U : 0U,
           root3.valid ? 1U : 0U,
           root1.value[0],
           root1.value[1],
           root1.value[2],
           root2.value[0],
           root2.value[1],
           root2.value[2],
           root3.value[0],
           root3.value[1],
           root3.value[2]);
}

#if 0 // Invalidated: these RVAs came from the old +0x70000-skewed Ghidra mapping.
/**
 * Invokes one native part submission and records the composed root it actually consumes.
 *
 * The parent traversal entry is modified before this probe installs in the live image. Its two
 * child submissions remain clean and both explicitly read traversal->state+0x640, so observing
 * them provides the same downstream-composition evidence without following or stacking detours.
 * This recorder never changes the traversal, root, part arguments, or original result.
 */
__declspec(noinline) void invoke_final_model_part_submit(
    FinalModelPartSubmit original,
    FinalModelPartRoute route,
    std::uintptr_t* traversal,
    std::uintptr_t partOutput,
    std::uintptr_t partDescriptor,
    std::uintptr_t renderEntry,
    std::uintptr_t partContext) noexcept {
    const SceneTransformRecorderCallGuard activeCall{
        g_finalModelTransformSubmitActiveCalls};
    if (original == nullptr) {
        return;
    }

    const std::uint64_t now = GetTickCount64();
    const std::uint64_t vfxTick =
        g_poseSocketVfxLastCallTick.load(std::memory_order_acquire);
    const bool armed = omega_forced() && vfxTick != 0U && now >= vfxTick
                       && now - vfxTick <= kFinalModelTransformArmWindowMs;
    if (!armed) {
        original(traversal,
                 partOutput,
                 partDescriptor,
                 renderEntry,
                 partContext);
        return;
    }

    const IkoraActor actor1 = ikora_factory_actor(1U);
    const bool actor1Current = !actor1.released
                               && object_handle_is_current(actor1.object);
    if (!actor1Current) {
        original(traversal,
                 partOutput,
                 partDescriptor,
                 renderEntry,
                 partContext);
        return;
    }

    const std::uintptr_t modelState = safe_read<std::uintptr_t>(
        traversal == nullptr ? nullptr : traversal + 5U, 0U);
    const std::uintptr_t rootAddress =
        modelState != 0U && modelState <= UINTPTR_MAX - kFinalModelTransformRootOffset
            ? modelState + kFinalModelTransformRootOffset
            : 0U;
    const DecodedSceneTransform rootBefore = decode_scene_transform_address(
        reinterpret_cast<const void*>(rootAddress));
    const DecodedObjectPosition actorRoot1 = decode_object_position(actor1.object);
    const std::uint32_t call = g_finalModelTransformCalls.fetch_add(
                                   1U, std::memory_order_relaxed)
                               + 1U;

    original(traversal,
             partOutput,
             partDescriptor,
             renderEntry,
             partContext);

    const FinalModelTransformTraceKey traceKey{
        reinterpret_cast<std::uintptr_t>(traversal), modelState, route};
    std::size_t ordinal = 0U;
    if (!remember_final_model_transform_trace_key(traceKey, ordinal)) {
        return;
    }

    const IkoraComponentAddressOwner traversalOwner =
        locate_ikora_component_address(traversal);
    const IkoraComponentAddressOwner stateOwner =
        locate_ikora_component_address(reinterpret_cast<const void*>(modelState));
    const IkoraComponentAddressOwner outputOwner =
        locate_ikora_component_address(reinterpret_cast<const void*>(partOutput));
    const IkoraComponentAddressOwner descriptorOwner =
        locate_ikora_component_address(reinterpret_cast<const void*>(partDescriptor));
    const IkoraComponentAddressOwner entryOwner =
        locate_ikora_component_address(reinterpret_cast<const void*>(renderEntry));
    const IkoraComponentAddressOwner contextOwner =
        locate_ikora_component_address(reinterpret_cast<const void*>(partContext));
    const bool directScene1 = traversalOwner.factorySequence == 1U
                              || stateOwner.factorySequence == 1U
                              || outputOwner.factorySequence == 1U
                              || descriptorOwner.factorySequence == 1U
                              || entryOwner.factorySequence == 1U
                              || contextOwner.factorySequence == 1U;
    const float actorDistance = position_distance(rootBefore, actorRoot1);
    const bool nearScene1 = actorDistance >= 0.0F
                            && actorDistance <= kFinalModelTransformNearActorDistance;
    // Preserve a small unfiltered discovery slice in case +0x640 is not encoded as the expected
    // native 0x20-byte transform. All later samples must be spatially or structurally Scene 1.
    const bool discovery = ordinal <= 64U;
    if (!discovery && !nearScene1 && !directScene1) {
        return;
    }

    const std::uint32_t sample = g_finalModelTransformSamples.fetch_add(
                                     1U, std::memory_order_relaxed)
                                 + 1U;
    if (sample > kMaximumFinalModelTransformSamples) {
        return;
    }

    const DecodedSceneTransform rootAfter = decode_scene_transform_address(
        reinterpret_cast<const void*>(rootAddress));
    const IkoraActor actor2 = ikora_factory_actor(2U);
    const IkoraActor actor3 = ikora_factory_actor(3U);
    const DecodedObjectPosition actorRoot2 = decode_object_position(actor2.object);
    const DecodedObjectPosition actorRoot3 = decode_object_position(actor3.object);
    const VfxSocketTransformSnapshot vfx = latest_vfx_socket_transform();
    float rootToVfx = -1.0F;
    if (rootBefore.finite && vfx.valid) {
        const float x = rootBefore.position[0] - vfx.position[0];
        const float y = rootBefore.position[1] - vfx.position[1];
        const float z = rootBefore.position[2] - vfx.position[2];
        rootToVfx = std::sqrt(x * x + y * y + z * z);
    }

    const RendererActorCorrelation traversalCorrelation = correlate_renderer_object(
        reinterpret_cast<std::uintptr_t>(traversal));
    const RendererActorCorrelation stateCorrelation =
        correlate_renderer_object(modelState);
    const RendererActorCorrelation entryCorrelation =
        correlate_renderer_object(renderEntry);
    const auto evidenceMask = [](const RendererActorCorrelation& correlation) noexcept {
        return correlation.directActorMask
               | correlation.directPresentationMask
               | correlation.objectReferenceMask
               | correlation.presentationReferenceMask
               | correlation.behaviorReferenceMask;
    };
    const std::uint32_t traversalMask = evidenceMask(traversalCorrelation);
    const std::uint32_t stateMask = evidenceMask(stateCorrelation);
    const std::uint32_t entryMask = evidenceMask(entryCorrelation);

    std::array<std::uint64_t, 4U> rawRoot{};
    (void)safe_copy(rawRoot.data(),
                    reinterpret_cast<const void*>(rootAddress),
                    sizeof rawRoot);
    std::array<std::uint64_t, 4U> rawContext{};
    (void)safe_copy(rawContext.data(),
                    reinterpret_cast<const void*>(partContext),
                    sizeof rawContext);

    const char* const routeName = route == FinalModelPartRoute::type5
                                      ? "type5"
                                      : "ordinary";

    report("ev=omega_final_model_transform stage=sample n=%u call=%u unique=%zu "
           "boundary=%s reason=%s%s%s vfx_age_ms=%llu traversal=%p model_state=%p "
           "part_output=%p descriptor=%p entry=%p part_context=%p "
           "owners=t:%u:%08X:+0x%llX|s:%u:%08X:+0x%llX|o:%u:%08X:+0x%llX|"
           "d:%u:%08X:+0x%llX|e:%u:%08X:+0x%llX|c:%u:%08X:+0x%llX "
           "root_before=%u:%u:%.6f,%.6f,%.6f,%.6f|%.3f,%.3f,%.3f|%.6f "
           "root_after=%u:%u:%.3f,%.3f,%.3f "
           "actors=%08X,%08X,%08X roots_valid=%u,%u,%u "
           "roots=%.3f,%.3f,%.3f|%.3f,%.3f,%.3f|%.3f,%.3f,%.3f "
           "distances=%.3f,%.3f,%.3f vfx_valid=%u vfx_position=%.3f,%.3f,%.3f "
           "root_to_vfx=%.3f correlation_masks=%u,%u,%u "
           "raw_root=%016llX,%016llX,%016llX,%016llX "
           "raw_context=%016llX,%016llX,%016llX,%016llX mutation=observe_only",
           sample,
           call,
           ordinal,
           routeName,
           directScene1 ? "direct_scene1" : "",
           nearScene1 ? "+near_scene1" : "",
           discovery ? "+discovery" : "",
           static_cast<unsigned long long>(now - vfxTick),
           traversal,
           reinterpret_cast<void*>(modelState),
           reinterpret_cast<void*>(partOutput),
           reinterpret_cast<void*>(partDescriptor),
           reinterpret_cast<void*>(renderEntry),
           reinterpret_cast<void*>(partContext),
           traversalOwner.factorySequence,
           traversalOwner.definition,
           static_cast<unsigned long long>(traversalOwner.offset),
           stateOwner.factorySequence,
           stateOwner.definition,
           static_cast<unsigned long long>(stateOwner.offset),
           outputOwner.factorySequence,
           outputOwner.definition,
           static_cast<unsigned long long>(outputOwner.offset),
           descriptorOwner.factorySequence,
           descriptorOwner.definition,
           static_cast<unsigned long long>(descriptorOwner.offset),
           entryOwner.factorySequence,
           entryOwner.definition,
           static_cast<unsigned long long>(entryOwner.offset),
           contextOwner.factorySequence,
           contextOwner.definition,
           static_cast<unsigned long long>(contextOwner.offset),
           rootBefore.readable ? 1U : 0U,
           rootBefore.finite ? 1U : 0U,
           rootBefore.rotation[0],
           rootBefore.rotation[1],
           rootBefore.rotation[2],
           rootBefore.rotation[3],
           rootBefore.position[0],
           rootBefore.position[1],
           rootBefore.position[2],
           rootBefore.scale,
           rootAfter.readable ? 1U : 0U,
           rootAfter.finite ? 1U : 0U,
           rootAfter.position[0],
           rootAfter.position[1],
           rootAfter.position[2],
           actor1.object,
           actor2.object,
           actor3.object,
           actorRoot1.valid ? 1U : 0U,
           actorRoot2.valid ? 1U : 0U,
           actorRoot3.valid ? 1U : 0U,
           actorRoot1.value[0],
           actorRoot1.value[1],
           actorRoot1.value[2],
           actorRoot2.value[0],
           actorRoot2.value[1],
           actorRoot2.value[2],
           actorRoot3.value[0],
           actorRoot3.value[1],
           actorRoot3.value[2],
           position_distance(rootBefore, actorRoot1),
           position_distance(rootBefore, actorRoot2),
           position_distance(rootBefore, actorRoot3),
           vfx.valid ? 1U : 0U,
           vfx.position[0],
           vfx.position[1],
           vfx.position[2],
           rootToVfx,
           traversalMask,
           stateMask,
           entryMask,
           static_cast<unsigned long long>(rawRoot[0]),
           static_cast<unsigned long long>(rawRoot[1]),
           static_cast<unsigned long long>(rawRoot[2]),
           static_cast<unsigned long long>(rawRoot[3]),
           static_cast<unsigned long long>(rawContext[0]),
           static_cast<unsigned long long>(rawContext[1]),
           static_cast<unsigned long long>(rawContext[2]),
           static_cast<unsigned long long>(rawContext[3]));
}

__declspec(noinline) void __fastcall final_model_type5_part_submit(
    std::uintptr_t* traversal,
    std::uintptr_t partOutput,
    std::uintptr_t partDescriptor,
    std::uintptr_t renderEntry,
    std::uintptr_t partContext) noexcept {
    invoke_final_model_part_submit(
        g_finalModelType5PartSubmitOriginal.load(std::memory_order_acquire),
        FinalModelPartRoute::type5,
        traversal,
        partOutput,
        partDescriptor,
        renderEntry,
        partContext);
}

__declspec(noinline) void __fastcall final_model_ordinary_part_submit(
    std::uintptr_t* traversal,
    std::uintptr_t partOutput,
    std::uintptr_t partDescriptor,
    std::uintptr_t renderEntry,
    std::uintptr_t partContext) noexcept {
    invoke_final_model_part_submit(
        g_finalModelOrdinaryPartSubmitOriginal.load(std::memory_order_acquire),
        FinalModelPartRoute::ordinary,
        traversal,
        partOutput,
        partDescriptor,
        renderEntry,
        partContext);
}
#endif

[[nodiscard]] float position_distance_to_vfx(
    const DecodedSceneTransform& transform,
    const VfxSocketTransformSnapshot& vfx) noexcept {
    if (!transform.finite || !vfx.valid) {
        return -1.0F;
    }
    const float x = transform.position[0] - vfx.position[0];
    const float y = transform.position[1] - vfx.position[1];
    const float z = transform.position[2] - vfx.position[2];
    return std::sqrt(x * x + y * y + z * z);
}

/**
 * Applies the exact low-level purple-effect calibration and records the native composer.
 *
 * Corrected unwind/decompile analysis places this function after effect creation and in the
 * per-frame effect loop. The native composer runs exactly once. Only the four proven purple
 * handles are changed, after native composition, by adding +10 Z to the two live position vectors
 * at effect+0x08 and effect+0x18. All other effects remain observe-only.
 */
__declspec(noinline) void __fastcall effect_transform_compose(
    std::byte* effect,
    const std::uintptr_t* transformArrayBase,
    const std::uintptr_t* selectorArrayBase,
    std::uintptr_t extraTranslationBase) noexcept {
    const SceneTransformRecorderCallGuard activeCall{
        g_effectTransformComposeActiveCalls};
    const EffectTransformCompose original =
        g_effectTransformComposeOriginal.load(std::memory_order_acquire);
    if (original == nullptr) {
        return;
    }

    const std::uint64_t now = GetTickCount64();
    const std::uint64_t vfxTick =
        g_poseSocketVfxLastCallTick.load(std::memory_order_acquire);
    const bool armed = effect != nullptr && omega_forced() && vfxTick != 0U
                       && now >= vfxTick
                       && now - vfxTick <= kEffectTransformArmWindowMs;
    const std::uintptr_t callerRva = armed
                                         ? image_rva(reinterpret_cast<std::uintptr_t>(
                                               _ReturnAddress()))
                                         : 0U;
    const DecodedSceneTransform before = armed
                                             ? decode_scene_transform_address(effect)
                                             : DecodedSceneTransform{};

    original(effect,
             transformArrayBase,
             selectorArrayBase,
             extraTranslationBase);

    const std::uint32_t directDefinition = effect == nullptr
                                               ? kInvalidHandle
                                               : safe_read<std::uint32_t>(
                                                     effect + 0x48U,
                                                     kInvalidHandle);
    const std::uint32_t directHandle = effect == nullptr
                                           ? kInvalidHandle
                                           : safe_read<std::uint32_t>(
                                                 effect + 0x3CU,
                                                 kInvalidHandle);
    std::size_t directHandleIndex = kOmegaPurpleLowLevelEffectHandles.size();
    for (std::size_t index = 0U;
         index < kOmegaPurpleLowLevelEffectHandles.size();
         ++index) {
        if (directHandle == kOmegaPurpleLowLevelEffectHandles[index]) {
            directHandleIndex = index;
            break;
        }
    }

    bool directOffsetApplied = false;
    if (effect != nullptr && omega_forced()
        && directHandleIndex < kOmegaPurpleLowLevelEffectHandles.size()) {
        const std::uint32_t directCall =
            g_omegaPurpleDirectEffectOffsetCalls.fetch_add(
                1U, std::memory_order_relaxed)
            + 1U;
        const float invalidPosition =
            std::numeric_limits<float>::quiet_NaN();
        const float nativeCurrentZ = safe_read<float>(
            effect + 0x08U, invalidPosition);
        const float nativePreviousZ = safe_read<float>(
            effect + 0x18U, invalidPosition);
        const bool nativeFinite = std::isfinite(nativeCurrentZ)
                                  && std::isfinite(nativePreviousZ);
        const float shiftedCurrentZ = nativeCurrentZ
                                      + kOmegaPurpleDirectEffectWorldZOffset;
        const float shiftedPreviousZ = nativePreviousZ
                                       + kOmegaPurpleDirectEffectWorldZOffset;
        bool currentWritten = false;
        bool previousWritten = false;
        if (nativeFinite) {
            currentWritten = safe_copy(
                effect + 0x08U, &shiftedCurrentZ, sizeof shiftedCurrentZ);
            previousWritten = safe_copy(
                effect + 0x18U, &shiftedPreviousZ, sizeof shiftedPreviousZ);
        }

        float readbackCurrentZ = safe_read<float>(
            effect + 0x08U, invalidPosition);
        float readbackPreviousZ = safe_read<float>(
            effect + 0x18U, invalidPosition);
        directOffsetApplied = currentWritten && previousWritten
                              && std::isfinite(readbackCurrentZ)
                              && std::isfinite(readbackPreviousZ)
                              && std::fabs(readbackCurrentZ - shiftedCurrentZ)
                                     <= 0.001F
                              && std::fabs(readbackPreviousZ - shiftedPreviousZ)
                                     <= 0.001F;
        if (directOffsetApplied) {
            g_omegaPurpleDirectEffectOffsetApplied.fetch_add(
                1U, std::memory_order_relaxed);
        } else if (currentWritten || previousWritten) {
            if (currentWritten) {
                (void)safe_copy(
                    effect + 0x08U, &nativeCurrentZ, sizeof nativeCurrentZ);
            }
            if (previousWritten) {
                (void)safe_copy(
                    effect + 0x18U, &nativePreviousZ, sizeof nativePreviousZ);
            }
            readbackCurrentZ = safe_read<float>(
                effect + 0x08U, invalidPosition);
            readbackPreviousZ = safe_read<float>(
                effect + 0x18U, invalidPosition);
        }

        const std::uint32_t directHandleBit =
            1U << static_cast<std::uint32_t>(directHandleIndex);
        const std::uint32_t previousSeenMask =
            g_omegaPurpleDirectEffectOffsetSeenMask.fetch_or(
                directHandleBit, std::memory_order_acq_rel);
        const bool firstForHandle =
            (previousSeenMask & directHandleBit) == 0U;
        std::uint64_t previousTick =
            g_omegaPurpleDirectEffectOffsetLastTick.load(
                std::memory_order_acquire);
        bool heartbeat = false;
        while (now - previousTick >= kSceneEventHeartbeatMs) {
            if (g_omegaPurpleDirectEffectOffsetLastTick.compare_exchange_weak(
                    previousTick,
                    now,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                heartbeat = true;
                break;
            }
        }
        if (firstForHandle || heartbeat) {
            if (firstForHandle) {
                g_omegaPurpleDirectEffectOffsetLastTick.store(
                    now, std::memory_order_release);
            }
            const char* const reason = directOffsetApplied
                                           ? "applied"
                                           : !nativeFinite
                                                 ? "nonfinite_native_position"
                                                 : "write_or_verify_failed";
            report("ev=omega_purple_effect_direct_offset stage=apply "
                   "call=%u effect=%p definition=%08X handle=%08X "
                   "handle_index=%zu first=%u heartbeat=%u "
                   "native_current_z=%.3f native_previous_z=%.3f "
                   "offset_z=%.3f shifted_current_z=%.3f "
                   "shifted_previous_z=%.3f readback_current_z=%.3f "
                   "readback_previous_z=%.3f applied=%s reason=%s "
                   "mutation=exact_effect_output_world_z_plus_10",
                   directCall,
                   effect,
                   directDefinition,
                   directHandle,
                   directHandleIndex,
                   firstForHandle ? 1U : 0U,
                   heartbeat ? 1U : 0U,
                   static_cast<double>(nativeCurrentZ),
                   static_cast<double>(nativePreviousZ),
                   static_cast<double>(kOmegaPurpleDirectEffectWorldZOffset),
                   static_cast<double>(shiftedCurrentZ),
                   static_cast<double>(shiftedPreviousZ),
                   static_cast<double>(readbackCurrentZ),
                   static_cast<double>(readbackPreviousZ),
                   directOffsetApplied ? "yes" : "no",
                   reason);
        }
    }

    if (!armed) {
        return;
    }

    const std::uint32_t call = g_effectTransformComposeCalls.fetch_add(
                                   1U, std::memory_order_relaxed)
                               + 1U;
    const DecodedSceneTransform after = decode_scene_transform_address(effect);
    const VfxSocketTransformSnapshot vfx = latest_vfx_socket_transform();
    if (!after.finite || !vfx.valid) {
        return;
    }

    const std::uintptr_t transformBase = safe_read<std::uintptr_t>(
        transformArrayBase, 0U);
    const std::uintptr_t selectorBase = safe_read<std::uintptr_t>(
        selectorArrayBase, 0U);
    const std::int64_t transformRelative = safe_read<std::int64_t>(
        effect + 0xD8U, INT64_MIN);
    const std::int64_t selectorRelative = safe_read<std::int64_t>(
        effect + 0xE0U, INT64_MIN);
    const std::uintptr_t transformArray = transformRelative == INT64_MIN
                                              ? 0U
                                              : add_relative(transformBase,
                                                             transformRelative);
    const std::uintptr_t selectorArray = selectorRelative == INT64_MIN
                                             ? 0U
                                             : add_relative(selectorBase,
                                                            selectorRelative);
    const std::uint8_t selector = safe_read<std::uint8_t>(effect + 0xD0U, 0U);
    const std::uintptr_t selectorRow = add_relative(
        selectorArray,
        static_cast<std::int64_t>(selector) * 0x10LL);
    const std::uint8_t sourceCount = safe_read<std::uint8_t>(
        reinterpret_cast<const void*>(selectorRow), 0U);
    const std::int16_t sourceIndex = safe_read<std::int16_t>(
        reinterpret_cast<const void*>(add_relative(selectorRow, 2)), -1);
    std::uintptr_t selectedInputAddress = 0U;
    if (sourceCount != 0U && sourceIndex >= 0) {
        selectedInputAddress = add_relative(
            add_relative(transformArray, 0x10),
            static_cast<std::int64_t>(sourceIndex) * 0x20LL);
    }
    const DecodedSceneTransform selectedInput = decode_scene_transform_address(
        reinterpret_cast<const void*>(selectedInputAddress));

    std::array<std::byte, 0x100U> rawEffect{};
    const bool rawReadable = safe_copy(rawEffect.data(), effect, rawEffect.size());
    std::array<std::uint64_t, 8U> rawHead{};
    std::int32_t purpleOneOffset = -1;
    std::int32_t purpleTwoOffset = -1;
    if (rawReadable) {
        std::memcpy(rawHead.data(), rawEffect.data(), sizeof rawHead);
        for (std::size_t offset = 0U;
             offset + sizeof(std::uint32_t) <= rawEffect.size();
             offset += sizeof(std::uint32_t)) {
            std::uint32_t value = 0U;
            std::memcpy(&value, rawEffect.data() + offset, sizeof value);
            if (value == kOmegaPurpleEffectOne && purpleOneOffset < 0) {
                purpleOneOffset = static_cast<std::int32_t>(offset);
            }
            if (value == kOmegaPurpleEffectTwo && purpleTwoOffset < 0) {
                purpleTwoOffset = static_cast<std::int32_t>(offset);
            }
        }
    }

    const float beforeToVfx = position_distance_to_vfx(before, vfx);
    const float inputToVfx = position_distance_to_vfx(selectedInput, vfx);
    const float afterToVfx = position_distance_to_vfx(after, vfx);
    const auto nearVfx = [](float distance) noexcept {
        return distance >= 0.0F && distance <= kEffectTransformNearVfxDistance;
    };
    const bool exactResource = purpleOneOffset >= 0 || purpleTwoOffset >= 0;
    if (!exactResource && !nearVfx(beforeToVfx) && !nearVfx(inputToVfx)
        && !nearVfx(afterToVfx)) {
        return;
    }

    const std::uint32_t definition = safe_read<std::uint32_t>(
        effect + 0x48U, kInvalidHandle);
    const std::uint32_t resource = purpleOneOffset >= 0
                                       ? kOmegaPurpleEffectOne
                                       : purpleTwoOffset >= 0
                                             ? kOmegaPurpleEffectTwo
                                             : safe_read<std::uint32_t>(
                                                   effect + 0x30U,
                                                   kInvalidHandle);
    std::size_t ordinal = 0U;
    bool first = false;
    bool heartbeat = false;
    const EffectTransformTraceKey traceKey{
        reinterpret_cast<std::uintptr_t>(effect),
        callerRva,
        definition,
        resource,
        now};
    if (!select_effect_transform_sample(
            traceKey, now, ordinal, first, heartbeat)) {
        return;
    }

    const std::uint32_t sample = g_effectTransformComposeSamples.fetch_add(
                                     1U, std::memory_order_relaxed)
                                 + 1U;
    if (sample > kMaximumEffectTransformSamples) {
        return;
    }

    const IkoraActor actor1 = ikora_factory_actor(1U);
    const IkoraActor actor2 = ikora_factory_actor(2U);
    const IkoraActor actor3 = ikora_factory_actor(3U);
    const DecodedObjectPosition actorRoot1 = decode_object_position(actor1.object);
    const DecodedObjectPosition actorRoot2 = decode_object_position(actor2.object);
    const DecodedObjectPosition actorRoot3 = decode_object_position(actor3.object);
    const std::uint32_t flags = safe_read<std::uint32_t>(effect + 0xACU, 0U);
    const std::uint32_t handle34 = safe_read<std::uint32_t>(
        effect + 0x34U, kInvalidHandle);
    const std::uint32_t handle38 = safe_read<std::uint32_t>(
        effect + 0x38U, kInvalidHandle);
    const std::uint32_t handle3C = safe_read<std::uint32_t>(
        effect + 0x3CU, kInvalidHandle);
    const std::uint16_t definitionTransformIndex = safe_read<std::uint16_t>(
        effect + 0x44U, 0U);
    const std::uint16_t definitionSelectorIndex = safe_read<std::uint16_t>(
        effect + 0x46U, 0U);
    const std::uint8_t secondarySelector = safe_read<std::uint8_t>(
        effect + 0xD1U, 0U);
    std::array<float, 3U> extraTranslation{};
    const bool extraReadable = safe_copy(
        extraTranslation.data(),
        reinterpret_cast<const void*>(extraTranslationBase),
        sizeof extraTranslation);

    const char* const route = callerRva == kEffectTransformCreateCallsiteRva
                                  ? "create"
                                  : callerRva == kEffectTransformUpdateCallsiteRva
                                        ? "update"
                                        : "other";
    report("ev=omega_effect_transform_compose stage=sample n=%u call=%u unique=%zu "
           "first=%u heartbeat=%u route=%s caller=+%llX vfx_age_ms=%llu "
           "effect=%p definition=%08X resource=%08X handles=%08X,%08X,%08X "
           "flags=%08X exact_resource=%u purple_offsets=%d,%d "
           "before=%u:%u:%.3f,%.3f,%.3f input=%u:%u:%.3f,%.3f,%.3f "
           "after=%u:%u:%.3f,%.3f,%.3f vfx=%.3f,%.3f,%.3f "
           "to_vfx=%.3f,%.3f,%.3f actors=%08X,%08X,%08X "
           "actor_roots=%.3f,%.3f,%.3f|%.3f,%.3f,%.3f|%.3f,%.3f,%.3f "
           "after_to_actors=%.3f,%.3f,%.3f mutation=%s",
           sample,
           call,
           ordinal,
           first ? 1U : 0U,
           heartbeat ? 1U : 0U,
           route,
           static_cast<unsigned long long>(callerRva),
           static_cast<unsigned long long>(now - vfxTick),
           effect,
           definition,
           resource,
           handle34,
           handle38,
           handle3C,
           flags,
           exactResource ? 1U : 0U,
           purpleOneOffset,
           purpleTwoOffset,
           before.readable ? 1U : 0U,
           before.finite ? 1U : 0U,
           before.position[0],
           before.position[1],
           before.position[2],
           selectedInput.readable ? 1U : 0U,
           selectedInput.finite ? 1U : 0U,
           selectedInput.position[0],
           selectedInput.position[1],
           selectedInput.position[2],
           after.readable ? 1U : 0U,
           after.finite ? 1U : 0U,
           after.position[0],
           after.position[1],
           after.position[2],
           vfx.position[0],
           vfx.position[1],
           vfx.position[2],
           beforeToVfx,
           inputToVfx,
           afterToVfx,
           actor1.object,
           actor2.object,
           actor3.object,
           actorRoot1.value[0],
           actorRoot1.value[1],
           actorRoot1.value[2],
           actorRoot2.value[0],
           actorRoot2.value[1],
           actorRoot2.value[2],
           actorRoot3.value[0],
           actorRoot3.value[1],
           actorRoot3.value[2],
           position_distance(after, actorRoot1),
           position_distance(after, actorRoot2),
           position_distance(after, actorRoot3),
           directOffsetApplied ? "exact_effect_output_world_z_plus_10"
                               : "observe_only");
    report("ev=omega_effect_transform_compose stage=inputs n=%u effect=%p "
           "transform_arg=%p transform_base=%p transform_relative=%lld "
           "transform_array=%p selector_arg=%p selector_base=%p "
           "selector_relative=%lld selector_array=%p selector_row=%p "
           "selector=%u secondary_selector=%u source_count=%u source_index=%d "
           "selected_input=%p definition_indices=%u,%u extra=%p:%u:%.3f,%.3f,%.3f "
           "raw_readable=%u raw_head=%016llX,%016llX,%016llX,%016llX,"
           "%016llX,%016llX,%016llX,%016llX mutation=%s",
           sample,
           effect,
           transformArrayBase,
           reinterpret_cast<void*>(transformBase),
           static_cast<long long>(transformRelative),
           reinterpret_cast<void*>(transformArray),
           selectorArrayBase,
           reinterpret_cast<void*>(selectorBase),
           static_cast<long long>(selectorRelative),
           reinterpret_cast<void*>(selectorArray),
           reinterpret_cast<void*>(selectorRow),
           static_cast<unsigned int>(selector),
           static_cast<unsigned int>(secondarySelector),
           static_cast<unsigned int>(sourceCount),
           static_cast<int>(sourceIndex),
           reinterpret_cast<void*>(selectedInputAddress),
           static_cast<unsigned int>(definitionTransformIndex),
           static_cast<unsigned int>(definitionSelectorIndex),
           reinterpret_cast<void*>(extraTranslationBase),
           extraReadable ? 1U : 0U,
           extraTranslation[0],
           extraTranslation[1],
           extraTranslation[2],
           rawReadable ? 1U : 0U,
           static_cast<unsigned long long>(rawHead[0]),
           static_cast<unsigned long long>(rawHead[1]),
           static_cast<unsigned long long>(rawHead[2]),
           static_cast<unsigned long long>(rawHead[3]),
           static_cast<unsigned long long>(rawHead[4]),
           static_cast<unsigned long long>(rawHead[5]),
           static_cast<unsigned long long>(rawHead[6]),
           static_cast<unsigned long long>(rawHead[7]),
           directOffsetApplied ? "exact_effect_output_world_z_plus_10"
                               : "observe_only");
}

/**
 * Records the provider row immediately around its native virtual pose/socket lookup.
 *
 * The exact VFX call is retained in the enclosing TLS capture. Every distinct 0x15 path observed
 * after scene 1 activates also records the terminal provider's +0x50 dispatch datum and
 * +0x58/+0x60 pose datum/relative pair, plus the active scheduler/event entry. This proves which
 * scene-1 runtime entry produces each structurally compatible pose without depending on the
 * purple provider's component definition. The treatment changes only the two-word pose interface
 * argument for the exact purple row; every failed validation executes the native interface.
 */
__declspec(noinline) std::uint64_t __fastcall scene_transform_pose_candidate_resolve(
    const std::byte* provider,
    std::uint32_t candidateIndex,
    std::uintptr_t resolverContext,
    std::uint8_t flags,
    const void* filterContext,
    const std::byte* definitionBase,
    const std::uintptr_t* poseInterface,
    char allowFallback,
    std::byte* output) noexcept {
    const SceneTransformRecorderCallGuard activeCall{
        g_sceneTransformPoseCandidateResolveActiveCalls};
    const SceneTransformPoseCandidateResolve original =
        g_sceneTransformPoseCandidateResolveOriginal.load(std::memory_order_acquire);
    if (original == nullptr) {
        return 0U;
    }

    SceneTransformKind2ResolveCapture& parent =
        g_sceneTransformKind2ResolveCapture;
    const bool exactVfxCall = parent.armed && parent.called
                              && provider != nullptr
                              && reinterpret_cast<std::uintptr_t>(provider)
                                     == parent.provider;
    const std::uint32_t trackedBindingKey =
        g_scenePoseBindingKey.load(std::memory_order_acquire);
    const std::uint32_t comparisonBindingKey =
        trackedBindingKey == kInvalidHandle
            ? kOmegaPurplePoseBindingKey
            : trackedBindingKey;
    const bool sceneOneActivated =
        (g_sceneActivationObservedMask.load(std::memory_order_acquire) & 0x1U)
        != 0U;
    if (!exactVfxCall && (!omega_forced() || !sceneOneActivated)) {
        return original(provider,
                        candidateIndex,
                        resolverContext,
                        flags,
                        filterContext,
                        definitionBase,
                        poseInterface,
                        allowFallback,
                        output);
    }

    const std::uint32_t stride = safe_read<std::uint32_t>(
        reinterpret_cast<const void*>(
            resolverContext == 0U ? 0U : resolverContext + 0x14U),
        0U);
    const std::uintptr_t tableAnchor =
        reinterpret_cast<std::uintptr_t>(definitionBase) + 0x58U;
    const std::int64_t tableRelative = safe_read<std::int64_t>(
        definitionBase == nullptr
            ? nullptr
            : reinterpret_cast<const void*>(tableAnchor),
        INT64_MIN);
    const std::uintptr_t table =
        tableRelative == INT64_MIN || tableRelative == 0
            ? 0U
            : add_relative(tableAnchor, tableRelative);
    const std::uint64_t rowOffset =
        0x10ULL + static_cast<std::uint64_t>(candidateIndex)
                       * static_cast<std::uint64_t>(stride);
    const std::uintptr_t row = table == 0U || stride == 0U
                                       || rowOffset >= 0x40000000ULL
                                   ? 0U
                                   : add_relative(
                                         table,
                                         static_cast<std::int64_t>(rowOffset));
    std::array<std::byte, 0x40U> rowBytes{};
    const bool rowReadable = safe_copy(
        rowBytes.data(), reinterpret_cast<const void*>(row), rowBytes.size());
    std::uint32_t rowWord0 = kInvalidHandle;
    std::uint32_t bindingKey = kInvalidHandle;
    std::uint32_t selectionKey = kInvalidHandle;
    if (rowReadable) {
        std::memcpy(&rowWord0, rowBytes.data() + 0x00U, sizeof rowWord0);
        std::memcpy(&bindingKey, rowBytes.data() + 0x04U, sizeof bindingKey);
        std::memcpy(&selectionKey, rowBytes.data() + 0x30U, sizeof selectionKey);
    }

    if (!exactVfxCall && bindingKey != comparisonBindingKey) {
        return original(provider,
                        candidateIndex,
                        resolverContext,
                        flags,
                        filterContext,
                        definitionBase,
                        poseInterface,
                        allowFallback,
                        output);
    }

    const IkoraComponentAddressOwner providerOwner =
        locate_ikora_component_address(provider);
    const bool realAnimationProvider = providerOwner.found
                                       && providerOwner.definition
                                              == kIkoraTransformProviderComponentDefinition
                                       && (providerOwner.factorySequence == 1U
                                           || providerOwner.factorySequence == 3U);

    const std::uintptr_t poseDispatchBase = safe_read<std::uintptr_t>(
        poseInterface, 0U);
    const std::uintptr_t poseObject = safe_read<std::uintptr_t>(
        poseInterface == nullptr ? nullptr : poseInterface + 1U, 0U);
    const IkoraComponentAddressOwner filterOwner =
        locate_ikora_component_address(filterContext);
    const IkoraComponentAddressOwner poseDispatchOwner =
        locate_ikora_component_address(
            reinterpret_cast<const void*>(poseDispatchBase));
    const IkoraComponentAddressOwner poseObjectOwner =
        locate_ikora_component_address(
            reinterpret_cast<const void*>(poseObject));

    const PoseProviderProvenanceTraceKey provenanceKey{
        reinterpret_cast<std::uintptr_t>(provider),
        poseObject,
        reinterpret_cast<std::uintptr_t>(definitionBase),
        candidateIndex,
        bindingKey,
        flags,
        exactVfxCall};
    const bool firstProvenancePath =
        remember_pose_provider_provenance_trace_key(provenanceKey);
    const std::uint32_t provenanceCall =
        g_poseProviderProvenanceCalls.fetch_add(1U, std::memory_order_relaxed) + 1U;
    std::uint32_t provenanceSample = 0U;
    if (firstProvenancePath) {
        const std::uint32_t candidateSample =
            g_poseProviderProvenanceSamples.fetch_add(
                1U, std::memory_order_relaxed)
            + 1U;
        if (candidateSample <= kMaximumPoseProviderProvenanceSamples) {
            provenanceSample = candidateSample;
        }
    }

    const std::byte* const dispatchSource =
        provider == nullptr ? nullptr : provider + 0x50U;
    const std::byte* const poseSource =
        provider == nullptr ? nullptr : provider + 0x58U;
    const std::uint32_t providerDispatchHandle = safe_read<std::uint32_t>(
        dispatchSource, kInvalidHandle);
    const std::uintptr_t providerDispatchRecord = reinterpret_cast<std::uintptr_t>(
        scene_object_record(providerDispatchHandle));
    std::uint32_t providerPoseHandle = kInvalidHandle;
    std::int64_t providerPoseRelative = INT64_MIN;
    std::uintptr_t providerPoseRecord = 0U;
    const std::uintptr_t providerResolvedPose = resolve_scene_relative_record(
        poseSource,
        providerPoseHandle,
        providerPoseRelative,
        providerPoseRecord);

    const std::uintptr_t activeSchedulerScene =
        g_sceneActorSchedulerDepth != 0U && g_sceneActorSchedulerScene != nullptr
            ? reinterpret_cast<std::uintptr_t>(g_sceneActorSchedulerScene)
            : 0U;
    const std::uint32_t activeSchedulerHandle = safe_read<std::uint32_t>(
        reinterpret_cast<const void*>(activeSchedulerScene), kInvalidHandle);
    const std::uintptr_t activeEventScene =
        reinterpret_cast<std::uintptr_t>(g_sceneAuthoredEventScene);
    const std::int32_t activeEventIndex = g_sceneAuthoredEventIndex;
    const std::uint32_t activeEventHandle = g_sceneAuthoredEventHandle;
    const std::uintptr_t activeEventRuntime =
        reinterpret_cast<std::uintptr_t>(g_sceneAuthoredEventRuntime);
    const std::uintptr_t activeEventAuthored =
        reinterpret_cast<std::uintptr_t>(g_sceneAuthoredEventAuthored);
    const std::uintptr_t sceneOneScheduler =
        g_omegaSceneSchedulerPointers[0].load(std::memory_order_acquire);
    const std::uintptr_t sceneOneEvent =
        g_omegaSceneEventPointers[0].load(std::memory_order_acquire);
    const std::int64_t sceneOneSchedulerDelta =
        pointer_delta(poseObject, sceneOneScheduler);
    const std::int64_t sceneOneEventDelta = pointer_delta(poseObject, sceneOneEvent);
    const SceneActorSlotCapture sceneOneSlot =
        provenanceSample != 0U && sceneOneScheduler != 0U
            ? capture_scene_actor_slot(
                  reinterpret_cast<std::uint32_t*>(sceneOneScheduler), 0U)
            : SceneActorSlotCapture{};
    const IkoraActor sceneOneActor = ikora_factory_actor(1U);
    const std::uintptr_t provenanceCallerRva = image_rva(
        reinterpret_cast<std::uintptr_t>(_ReturnAddress()));

    ScenePoseCandidateCapture* exactCapture = nullptr;
    if (exactVfxCall
        && parent.poseCandidateCount < parent.poseCandidates.size()) {
        exactCapture = &parent.poseCandidates[parent.poseCandidateCount++];
        exactCapture->called = true;
        exactCapture->rowReadable = rowReadable;
        exactCapture->candidateIndex = candidateIndex;
        exactCapture->resolverStride = stride;
        exactCapture->rowWord0 = rowWord0;
        exactCapture->bindingKey = bindingKey;
        exactCapture->selectionKey = selectionKey;
        exactCapture->flags = flags;
        exactCapture->allowFallback = allowFallback;
        exactCapture->provider = reinterpret_cast<std::uintptr_t>(provider);
        exactCapture->resolverContext = resolverContext;
        exactCapture->filterContext = reinterpret_cast<std::uintptr_t>(filterContext);
        exactCapture->definitionBase =
            reinterpret_cast<std::uintptr_t>(definitionBase);
        exactCapture->table = table;
        exactCapture->row = row;
        exactCapture->poseInterface =
            reinterpret_cast<std::uintptr_t>(poseInterface);
        exactCapture->poseDispatchBase = poseDispatchBase;
        exactCapture->poseObject = poseObject;
        exactCapture->output = reinterpret_cast<std::uintptr_t>(output);
        exactCapture->providerOwner = providerOwner;
        exactCapture->filterOwner = filterOwner;
        exactCapture->poseDispatchOwner = poseDispatchOwner;
        exactCapture->poseObjectOwner = poseObjectOwner;
        exactCapture->rowBytes = rowBytes;
        exactCapture->outputBeforeReadable = safe_copy(
            exactCapture->outputBefore.data(),
            output,
            exactCapture->outputBefore.size());
        if (bindingKey != kInvalidHandle) {
            g_scenePoseBindingKey.store(bindingKey, std::memory_order_release);
        }
    }

    const std::uint64_t poseIdentity = safe_read<std::uint64_t>(
        reinterpret_cast<const void*>(poseObject), 0U);
    const std::uint64_t poseSize = safe_read<std::uint64_t>(
        reinterpret_cast<const void*>(poseObject + 0x08U), 0U);
    const bool sceneOnePoseCacheCandidate =
        !exactVfxCall && rowReadable && realAnimationProvider
        && providerOwner.factorySequence == 1U
        && providerOwner.object == sceneOneActor.object
        && !sceneOneActor.released
        && object_handle_is_current(sceneOneActor.object)
        && candidateIndex == kOmegaSceneOnePoseCandidateIndex
        && selectionKey == kOmegaSceneOnePoseSelection
        && bindingKey == kOmegaPurplePoseBindingKey && flags == 0U
        && allowFallback == 0 && poseDispatchBase != 0U && poseObject != 0U
        && providerDispatchRecord == poseDispatchBase
        && providerResolvedPose == poseObject
        && poseIdentity == kOmegaScenePoseObjectIdentity
        && poseSize == kOmegaScenePoseObjectSize
        && sceneOneScheduler != 0U && sceneOneSchedulerDelta > 0
        && sceneOneSchedulerDelta < 0x10000LL;

    SceneOnePoseBindingCache rebindCache{};
    IkoraComponentAddressOwner rebindProviderOwner{};
    std::array<std::uintptr_t, 2U> reboundPoseInterface{};
    const std::uintptr_t* effectivePoseInterface = poseInterface;
    bool rebindSelected = false;
    bool rebindApplied = false;
    bool rebindFallback = false;
    const char* rebindReason = exactVfxCall ? "disabled" : "not_exact_vfx";
    std::uint32_t rebindCall = 0U;
    if (exactVfxCall) {
        rebindCall = g_omegaPurplePoseRebindCalls.fetch_add(
                         1U, std::memory_order_relaxed)
                     + 1U;
        const bool cacheAvailable = snapshot_scene_one_pose_binding(rebindCache);
        if (!kEnableOmegaPurplePoseRebind) {
            rebindReason = "disabled";
        } else if (!rowReadable || bindingKey != kOmegaPurplePoseBindingKey
                   || candidateIndex != kOmegaPurplePoseCandidateIndex
                   || selectionKey != kOmegaPurplePoseSelection || flags != 0U
                   || allowFallback != 0) {
            rebindReason = "exact_row_identity";
        } else if (!cacheAvailable) {
            rebindReason = "scene1_cache_missing";
        } else if (sceneOneActor.released
                   || sceneOneActor.object != rebindCache.actor
                   || !object_handle_is_current(sceneOneActor.object)) {
            rebindReason = "scene1_actor_not_current";
        } else if (!providerOwner.found || providerOwner.factorySequence != 1U
                   || providerOwner.object != sceneOneActor.object) {
            rebindReason = "vfx_provider_owner";
        } else {
            rebindProviderOwner = locate_ikora_component_address(
                reinterpret_cast<const void*>(rebindCache.provider));
            const std::uint64_t cachedPoseIdentity = safe_read<std::uint64_t>(
                reinterpret_cast<const void*>(rebindCache.poseObject), 0U);
            const std::uint64_t cachedPoseSize = safe_read<std::uint64_t>(
                reinterpret_cast<const void*>(rebindCache.poseObject + 0x08U), 0U);
            const std::int64_t currentSceneDelta = pointer_delta(
                rebindCache.poseObject, sceneOneScheduler);
            if (!rebindProviderOwner.found
                || rebindProviderOwner.factorySequence != 1U
                || rebindProviderOwner.object != sceneOneActor.object
                || rebindProviderOwner.definition
                       != kIkoraTransformProviderComponentDefinition
                || rebindProviderOwner.instance != rebindCache.providerComponent
                || rebindProviderOwner.offset != rebindCache.providerOffset) {
                rebindReason = "scene1_provider_owner";
            } else if (rebindCache.dispatchBase == 0U
                       || rebindCache.dispatchBase != poseDispatchBase) {
                rebindReason = "dispatch_mismatch";
            } else if (rebindCache.poseObject == 0U
                       || rebindCache.poseObject == poseObject) {
                rebindReason = "pose_identity_not_distinct";
            } else if (rebindCache.sceneScheduler == 0U
                       || rebindCache.sceneScheduler != sceneOneScheduler
                       || currentSceneDelta != rebindCache.sceneSchedulerDelta
                       || currentSceneDelta <= 0 || currentSceneDelta >= 0x10000LL) {
                rebindReason = "scene1_runtime_identity";
            } else if (cachedPoseIdentity != kOmegaScenePoseObjectIdentity
                       || cachedPoseSize != kOmegaScenePoseObjectSize
                       || cachedPoseIdentity != rebindCache.poseIdentity
                       || cachedPoseSize != rebindCache.poseSize) {
                rebindReason = "scene1_pose_header";
            } else {
                reboundPoseInterface[0] = rebindCache.dispatchBase;
                reboundPoseInterface[1] = rebindCache.poseObject;
                effectivePoseInterface = reboundPoseInterface.data();
                rebindSelected = true;
                rebindReason = "selected";
            }
        }
    }

    if (exactVfxCall) {
        ++g_poseSocketExactVfxDepth;
    }
    std::uint64_t result = original(provider,
                                    candidateIndex,
                                    resolverContext,
                                    flags,
                                    filterContext,
                                    definitionBase,
                                    effectivePoseInterface,
                                    allowFallback,
                                    output);
    if (rebindSelected) {
        const DecodedSceneTransform treatmentTransform =
            decode_scene_transform_address(
                output == nullptr ? nullptr : output + 0x10U);
        if (result == 0U || !treatmentTransform.readable
            || !treatmentTransform.finite) {
            rebindFallback = true;
            rebindReason = result == 0U
                               ? "treatment_result"
                               : "treatment_transform";
            result = original(provider,
                              candidateIndex,
                              resolverContext,
                              flags,
                              filterContext,
                              definitionBase,
                              poseInterface,
                              allowFallback,
                              output);
            rebindSelected = false;
        } else {
            rebindApplied = true;
            rebindReason = "applied";
        }
    } else if (exactVfxCall) {
        rebindFallback = true;
    }
    if (exactVfxCall) {
        --g_poseSocketExactVfxDepth;
    }

    if (sceneOnePoseCacheCandidate && result != 0U) {
        const DecodedSceneTransform learnedTransform =
            decode_scene_transform_address(
                output == nullptr ? nullptr : output + 0x10U);
        if (learnedTransform.readable && learnedTransform.finite) {
            SceneOnePoseBindingCache learned{};
            learned.valid = true;
            learned.actor = sceneOneActor.object;
            learned.provider = reinterpret_cast<std::uintptr_t>(provider);
            learned.providerComponent = providerOwner.instance;
            learned.providerOffset = providerOwner.offset;
            learned.definitionBase = reinterpret_cast<std::uintptr_t>(definitionBase);
            learned.dispatchBase = poseDispatchBase;
            learned.poseObject = poseObject;
            learned.sceneScheduler = sceneOneScheduler;
            learned.sceneSchedulerDelta = sceneOneSchedulerDelta;
            learned.poseIdentity = poseIdentity;
            learned.poseSize = poseSize;
            learned.capturedAtMs = GetTickCount64();
            if (publish_scene_one_pose_binding(learned)) {
                report("ev=omega_purple_pose_rebind stage=cache result=ok "
                       "actor=%08X provider=%p component=%p offset=0x%llX definition=%p "
                       "candidate_index=%u selection=%08X binding_key=%08X "
                       "dispatch=%p pose=%p scene_scheduler=%p scene_delta=%lld "
                       "pose_words=%016llX,%016llX "
                       "position=%.3f,%.3f,%.3f mutation=observe_only",
                       learned.actor,
                       reinterpret_cast<void*>(learned.provider),
                       reinterpret_cast<void*>(learned.providerComponent),
                       static_cast<unsigned long long>(learned.providerOffset),
                       reinterpret_cast<void*>(learned.definitionBase),
                       candidateIndex,
                       selectionKey,
                       bindingKey,
                       reinterpret_cast<void*>(learned.dispatchBase),
                       reinterpret_cast<void*>(learned.poseObject),
                       reinterpret_cast<void*>(learned.sceneScheduler),
                       static_cast<long long>(learned.sceneSchedulerDelta),
                       static_cast<unsigned long long>(learned.poseIdentity),
                       static_cast<unsigned long long>(learned.poseSize),
                       learnedTransform.position[0],
                       learnedTransform.position[1],
                       learnedTransform.position[2]);
            }
        }
    }

    if (exactVfxCall) {
        if (rebindApplied) {
            g_omegaPurplePoseRebindApplied.fetch_add(
                1U, std::memory_order_relaxed);
        } else {
            g_omegaPurplePoseRebindFallbacks.fetch_add(
                1U, std::memory_order_relaxed);
        }
        const std::uint64_t now = GetTickCount64();
        std::uint64_t previous = g_omegaPurplePoseRebindLastTick.load(
            std::memory_order_acquire);
        bool heartbeat = false;
        while (now - previous >= kSceneEventHeartbeatMs) {
            if (g_omegaPurplePoseRebindLastTick.compare_exchange_weak(
                    previous,
                    now,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                heartbeat = true;
                break;
            }
        }
        if (rebindCall <= 4U || heartbeat) {
            if (rebindCall <= 4U) {
                g_omegaPurplePoseRebindLastTick.store(
                    now, std::memory_order_release);
            }
            const DecodedSceneTransform finalTransform =
                decode_scene_transform_address(
                    output == nullptr ? nullptr : output + 0x10U);
            report("ev=omega_purple_pose_rebind stage=treatment call=%u "
                   "selected=%u applied=%u fallback=%u reason=%s "
                   "actor=%08X actor_current=%u vfx_provider=%p "
                   "vfx_provider_definition=%08X candidate_index=%u "
                   "selection=%08X binding_key=%08X native_dispatch=%p native_pose=%p "
                   "cached_provider=%p cached_dispatch=%p cached_pose=%p "
                   "effective_pose=%p result=%016llX "
                   "transform_readable=%u transform_finite=%u "
                   "position=%.3f,%.3f,%.3f mutation=%s",
                   rebindCall,
                   rebindSelected || rebindApplied ? 1U : 0U,
                   rebindApplied ? 1U : 0U,
                   rebindFallback ? 1U : 0U,
                   rebindReason,
                   sceneOneActor.object,
                   !sceneOneActor.released
                           && object_handle_is_current(sceneOneActor.object)
                       ? 1U
                       : 0U,
                   provider,
                   providerOwner.definition,
                   candidateIndex,
                   selectionKey,
                   bindingKey,
                   reinterpret_cast<void*>(poseDispatchBase),
                   reinterpret_cast<void*>(poseObject),
                   reinterpret_cast<void*>(rebindCache.provider),
                   reinterpret_cast<void*>(rebindCache.dispatchBase),
                   reinterpret_cast<void*>(rebindCache.poseObject),
                   reinterpret_cast<void*>(
                       rebindApplied ? rebindCache.poseObject : poseObject),
                   static_cast<unsigned long long>(result),
                   finalTransform.readable ? 1U : 0U,
                   finalTransform.finite ? 1U : 0U,
                   finalTransform.position[0],
                   finalTransform.position[1],
                   finalTransform.position[2],
                   rebindApplied ? "pose_interface_substitute" : "native_fallback");
        }
    }

    if (exactCapture != nullptr) {
        exactCapture->result = result;
        exactCapture->outputAfterReadable = safe_copy(
            exactCapture->outputAfter.data(),
            output,
            exactCapture->outputAfter.size());
    }

    if (provenanceSample != 0U) {
        const DecodedSceneTransform provenanceTransform =
            decode_scene_transform_address(
                output == nullptr ? nullptr : output + 0x10U);
        report("ev=omega_pose_provider_provenance stage=provider n=%u call=%u "
               "first_path=%u exact_vfx=%u caller=+%llX provider=%p "
               "provider_owner=%u:%08X:%08X component=%p offset=0x%llX "
               "provider_words_00_10=%016llX,%016llX,%016llX "
               "provider_words_50_68=%016llX,%016llX,%016llX,%016llX "
               "mutation=observe_only",
               provenanceSample,
               provenanceCall,
               firstProvenancePath ? 1U : 0U,
               exactVfxCall ? 1U : 0U,
               static_cast<unsigned long long>(provenanceCallerRva),
               provider,
               providerOwner.factorySequence,
               providerOwner.object,
               providerOwner.definition,
               reinterpret_cast<void*>(providerOwner.instance),
               static_cast<unsigned long long>(providerOwner.offset),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(
                   provider, 0U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(
                   provider == nullptr ? nullptr : provider + 0x08U, 0U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(
                   provider == nullptr ? nullptr : provider + 0x10U, 0U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(
                   dispatchSource, 0U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(
                   poseSource, 0U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(
                   provider == nullptr ? nullptr : provider + 0x60U, 0U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(
                   provider == nullptr ? nullptr : provider + 0x68U, 0U)));
        report("ev=omega_pose_provider_provenance stage=interface n=%u call=%u "
               "candidate_index=%u flags=%02X allow_fallback=%u "
               "definition_base=%p table=%p row=%p row_word0=%08X "
               "binding_key=%08X selection_key=%08X "
               "dispatch_handle=%08X dispatch_record=%p interface_dispatch=%p "
               "dispatch_matches=%u pose_handle=%08X pose_record=%p "
               "pose_relative=%lld resolved_pose=%p interface_pose=%p pose_matches=%u "
               "pose_words=%016llX,%016llX mutation=observe_only",
               provenanceSample,
               provenanceCall,
               candidateIndex,
               static_cast<unsigned int>(flags),
               allowFallback != 0 ? 1U : 0U,
               definitionBase,
               reinterpret_cast<void*>(table),
               reinterpret_cast<void*>(row),
               rowWord0,
               bindingKey,
               selectionKey,
               providerDispatchHandle,
               reinterpret_cast<void*>(providerDispatchRecord),
               reinterpret_cast<void*>(poseDispatchBase),
               providerDispatchRecord != 0U
                       && providerDispatchRecord == poseDispatchBase
                   ? 1U
                   : 0U,
               providerPoseHandle,
               reinterpret_cast<void*>(providerPoseRecord),
               static_cast<long long>(providerPoseRelative),
               reinterpret_cast<void*>(providerResolvedPose),
               reinterpret_cast<void*>(poseObject),
               providerResolvedPose != 0U && providerResolvedPose == poseObject
                   ? 1U
                   : 0U,
               static_cast<unsigned long long>(safe_read<std::uint64_t>(
                   reinterpret_cast<const void*>(poseObject), 0U)),
               static_cast<unsigned long long>(safe_read<std::uint64_t>(
                   reinterpret_cast<const void*>(poseObject + 0x08U), 0U)));
        report("ev=omega_pose_provider_provenance stage=context n=%u call=%u "
               "active_scheduler=%p scheduler_handle=%08X scheduler_depth=%u "
               "active_event_scene=%p event_handle=%08X event_index=%d "
               "event_authored=%p event_runtime=%p "
               "scene1_scheduler=%p scene1_event=%p "
               "pose_scene1_scheduler_delta=%lld pose_scene1_event_delta=%lld "
               "scene1_actor=%08X actor_current=%u "
               "slot_valid=%u slot_runtime=%p slot_c8=%d slot_cc=%d slot_d4=%u "
               "mutation=observe_only",
               provenanceSample,
               provenanceCall,
               reinterpret_cast<void*>(activeSchedulerScene),
               activeSchedulerHandle,
               g_sceneActorSchedulerDepth,
               reinterpret_cast<void*>(activeEventScene),
               activeEventHandle,
               activeEventIndex,
               reinterpret_cast<void*>(activeEventAuthored),
               reinterpret_cast<void*>(activeEventRuntime),
               reinterpret_cast<void*>(sceneOneScheduler),
               reinterpret_cast<void*>(sceneOneEvent),
               static_cast<long long>(sceneOneSchedulerDelta),
               static_cast<long long>(sceneOneEventDelta),
               sceneOneActor.object,
               !sceneOneActor.released
                       && object_handle_is_current(sceneOneActor.object)
                   ? 1U
                   : 0U,
               sceneOneSlot.valid ? 1U : 0U,
               sceneOneSlot.runtimeSlot,
               sceneOneSlot.runtimeC8,
               sceneOneSlot.runtimeCC,
               static_cast<unsigned int>(sceneOneSlot.runtimeD4));
        report("ev=omega_pose_provider_provenance stage=result n=%u call=%u "
               "result=%016llX output=%p transform_readable=%u transform_finite=%u "
               "rotation=%.6f,%.6f,%.6f,%.6f "
               "position=%.3f,%.3f,%.3f scale=%.6f mutation=observe_only",
               provenanceSample,
               provenanceCall,
               static_cast<unsigned long long>(result),
               output,
               provenanceTransform.readable ? 1U : 0U,
               provenanceTransform.finite ? 1U : 0U,
               provenanceTransform.rotation[0],
               provenanceTransform.rotation[1],
               provenanceTransform.rotation[2],
               provenanceTransform.rotation[3],
               provenanceTransform.position[0],
               provenanceTransform.position[1],
               provenanceTransform.position[2],
               provenanceTransform.scale);
    }

    if (!exactVfxCall && realAnimationProvider) {
        const std::size_t targetIndex =
            providerOwner.factorySequence == 1U ? 0U : 1U;
        const std::uint32_t call =
            g_scenePoseNaturalCalls[targetIndex].fetch_add(
                1U, std::memory_order_relaxed)
            + 1U;
        const std::uint64_t now = GetTickCount64();
        std::uint64_t previous =
            g_scenePoseNaturalLastTick[targetIndex].load(
                std::memory_order_acquire);
        bool heartbeat = false;
        while (now - previous >= kSceneEventHeartbeatMs) {
            if (g_scenePoseNaturalLastTick[targetIndex].compare_exchange_weak(
                    previous,
                    now,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                heartbeat = true;
                break;
            }
        }
        if (call <= 4U || heartbeat) {
            const std::uint32_t sample =
                g_scenePoseNaturalSamples[targetIndex].fetch_add(
                    1U, std::memory_order_relaxed)
                + 1U;
            if (sample <= kMaximumScenePoseTargetSamples) {
                const DecodedSceneTransform transform =
                    decode_scene_transform_address(
                        output == nullptr ? nullptr : output + 0x10U);
                const std::uint64_t binding0 = safe_read<std::uint64_t>(
                    output == nullptr ? nullptr : output + 0x30U, 0U);
                const std::uint64_t binding1 = safe_read<std::uint64_t>(
                    output == nullptr ? nullptr : output + 0x38U, 0U);
                const std::uint64_t binding2 = safe_read<std::uint64_t>(
                    output == nullptr ? nullptr : output + 0x40U, 0U);
                const IkoraActor actor =
                    ikora_factory_actor(providerOwner.factorySequence);
                report("ev=omega_scene_pose_binding stage=target_transform "
                       "n=%u call=%u role=%s factory_n=%u actor=%08X "
                       "actor_current=%s provider=%p provider_component=%p "
                       "provider_offset=0x%llX candidate_index=%u flags=%02X "
                       "allow_fallback=%u row=%p row_word0=%08X "
                       "binding_key=%08X selection_key=%08X pose_interface=%p "
                       "pose_dispatch_base=%p pose_object=%p result=%016llX "
                       "transform_readable=%s transform_finite=%s "
                       "rotation=%.6f,%.6f,%.6f,%.6f "
                       "position=%.3f,%.3f,%.3f scale=%.6f "
                       "output_binding=%016llX,%016llX,%016llX "
                       "mutation=observe_only",
                       sample,
                       call,
                       providerOwner.factorySequence == 1U
                           ? "scene1_real_animation"
                           : "scene3_real_animation",
                       providerOwner.factorySequence,
                       actor.object,
                       !actor.released && object_handle_is_current(actor.object)
                           ? "yes"
                           : "no",
                       provider,
                       reinterpret_cast<void*>(providerOwner.instance),
                       static_cast<unsigned long long>(providerOwner.offset),
                       candidateIndex,
                       static_cast<unsigned int>(flags),
                       allowFallback != 0 ? 1U : 0U,
                       reinterpret_cast<void*>(row),
                       rowWord0,
                       bindingKey,
                       selectionKey,
                       poseInterface,
                       reinterpret_cast<void*>(poseDispatchBase),
                       reinterpret_cast<void*>(poseObject),
                       static_cast<unsigned long long>(result),
                       transform.readable ? "yes" : "no",
                       transform.finite ? "yes" : "no",
                       transform.rotation[0],
                       transform.rotation[1],
                       transform.rotation[2],
                       transform.rotation[3],
                       transform.position[0],
                       transform.position[1],
                       transform.position[2],
                       transform.scale,
                       static_cast<unsigned long long>(binding0),
                       static_cast<unsigned long long>(binding1),
                       static_cast<unsigned long long>(binding2));
            }
        }
    }
    return result;
}

/**
 * Rebuilds the exact purple transform by changing only the active provider's candidate row.
 *
 * The earlier treatments retained a scene-creation pose object or provider whose role changed
 * before the purple bank update. This treatment stays entirely inside the exact +A1F640 call:
 * the active provider, definition, dispatch, pose, and filter remain native, while +A1E8A0 is
 * invoked with authored candidate 8 instead of native candidate 7. Any identity, liveness,
 * schema, or output failure executes the untouched +A1F640 path.
 */
__declspec(noinline) std::int32_t __fastcall scene_transform_kind2_provider_resolve(
    const std::byte* provider,
    std::uint32_t outputCapacity,
    void* transformOutput,
    std::uintptr_t resolverContext,
    const std::uint32_t* selection,
    std::uint8_t mode) noexcept {
    const SceneTransformRecorderCallGuard activeCall{
        g_sceneTransformKind2ProviderResolveActiveCalls};
    const SceneTransformKind2ProviderResolve original =
        g_sceneTransformKind2ProviderResolveOriginal.load(std::memory_order_acquire);
    if (original == nullptr) {
        return 0;
    }

    SceneTransformKind2ResolveCapture& capture = g_sceneTransformKind2ResolveCapture;
    const bool inspect = capture.armed && capture.called
                         && !capture.providerTreatmentCalled && provider != nullptr
                         && reinterpret_cast<std::uintptr_t>(provider) == capture.provider;
    if (!inspect) {
        return original(provider,
                        outputCapacity,
                        transformOutput,
                        resolverContext,
                        selection,
                        mode);
    }

    capture.providerTreatmentCalled = true;
    g_sceneTransformKind2ProviderTreatmentCalls.fetch_add(1U, std::memory_order_relaxed);
    const auto fallback = [&](const char* reason) noexcept {
        capture.providerTreatmentReason = reason;
        capture.providerTreatmentFallback = true;
        g_sceneTransformKind2ProviderTreatmentFallbacks.fetch_add(
            1U, std::memory_order_relaxed);
        capture.providerTreatmentFallbackResult = original(provider,
                                                           outputCapacity,
                                                           transformOutput,
                                                           resolverContext,
                                                           selection,
                                                           mode);
        return capture.providerTreatmentFallbackResult;
    };

    if (outputCapacity != 1U || outputCapacity != capture.outputCapacity
        || transformOutput == nullptr || transformOutput != capture.transformOutput
        || resolverContext != capture.resolverContext || selection == nullptr
        || mode != capture.mode || mode != 0U) {
        return fallback("argument_mismatch");
    }

    const IkoraActor actor1 = ikora_factory_actor(1U);
    const IkoraActor actor2 = ikora_factory_actor(2U);
    if (actor1.released || actor2.released
        || !object_handle_is_current(actor1.object)
        || !object_handle_is_current(actor2.object)) {
        return fallback("actor_not_current");
    }
    if (!capture.providerReadable || !capture.providerTargetReadable) {
        return fallback("active_provider_unreadable");
    }
    if (capture.providerTargetHandle == kInvalidHandle
        || capture.providerTargetRecord == 0U
        || capture.providerTargetRelative == INT64_MIN
        || capture.providerTargetRelative == 0
        || capture.providerTarget == 0U) {
        return fallback("active_definition_identity");
    }

    // +A1F640 may receive a short-lived copy outside the component allocation catalog. Native
    // +A1E8A0 demonstrably consumes that copy, so catalog provenance is diagnostic only. Keep it
    // in the log when available, but gate the mutation on the provider's engine-facing schema.
    capture.sceneOneProviderComponent = capture.providerOwner.instance;
    capture.sceneOneProviderComponentSize = capture.providerOwner.size;
    capture.sceneOneProviderOffset = capture.providerOwner.offset;
    capture.sceneOneProvider = capture.provider;
    capture.sceneOneDefinitionBase = capture.providerTarget;
    capture.sceneOneProviderBytes = capture.providerBytes;
    capture.sceneOneProviderReadable = capture.providerReadable;
    if (!capture.sceneOneProviderReadable) {
        return fallback("active_provider_copy");
    }

    const std::uint32_t stride = safe_read<std::uint32_t>(
        reinterpret_cast<const void*>(resolverContext + 0x14U), 0U);
    if (stride == 0U || stride != capture.resolverStride
        || stride > 0x10000U) {
        return fallback("active_definition_stride");
    }
    const std::uintptr_t activeTableAnchor = capture.providerTarget + 0x58U;
    const std::int64_t activeTableRelative = safe_read<std::int64_t>(
        reinterpret_cast<const void*>(activeTableAnchor), INT64_MIN);
    if (activeTableRelative == INT64_MIN || activeTableRelative == 0) {
        return fallback("active_definition_table");
    }
    capture.sceneOneDefinitionTable = add_relative(
        activeTableAnchor, activeTableRelative);
    const std::uint64_t rowOffset =
        0x10ULL
        + static_cast<std::uint64_t>(kOmegaSceneOnePoseCandidateIndex)
              * static_cast<std::uint64_t>(stride);
    if (capture.sceneOneDefinitionTable == 0U || rowOffset >= 0x40000000ULL) {
        return fallback("candidate8_definition_row");
    }
    capture.sceneOneDefinitionRow = add_relative(
        capture.sceneOneDefinitionTable, static_cast<std::int64_t>(rowOffset));
    std::array<std::byte, 0x40U> sceneOneRow{};
    capture.sceneOneDefinitionReadable = safe_copy(
        sceneOneRow.data(),
        reinterpret_cast<const void*>(capture.sceneOneDefinitionRow),
        sceneOneRow.size());
    if (capture.sceneOneDefinitionReadable) {
        std::memcpy(&capture.sceneOneRowBinding,
                    sceneOneRow.data() + 0x04U,
                    sizeof capture.sceneOneRowBinding);
        std::memcpy(&capture.sceneOneRowSelection,
                    sceneOneRow.data() + 0x30U,
                    sizeof capture.sceneOneRowSelection);
    }
    if (!capture.sceneOneDefinitionReadable
        || capture.sceneOneRowBinding != kOmegaPurplePoseBindingKey
        || capture.sceneOneRowSelection != kOmegaSceneOnePoseSelection) {
        return fallback("candidate8_definition_identity");
    }

    const auto* const activeProvider =
        reinterpret_cast<const std::byte*>(capture.provider);
    capture.sceneOneDispatchHandle = safe_read<std::uint32_t>(
        activeProvider + 0x50U, kInvalidHandle);
    capture.sceneOneDispatchRecord = reinterpret_cast<std::uintptr_t>(
        scene_object_record(capture.sceneOneDispatchHandle));
    capture.sceneOnePose = resolve_scene_relative_record(
        activeProvider + 0x58U,
        capture.sceneOnePoseHandle,
        capture.sceneOnePoseRelative,
        capture.sceneOnePoseRecord);
    capture.sceneOnePoseIdentity = safe_read<std::uint64_t>(
        reinterpret_cast<const void*>(capture.sceneOnePose), 0U);
    capture.sceneOnePoseSize = safe_read<std::uint64_t>(
        reinterpret_cast<const void*>(capture.sceneOnePose + 0x08U), 0U);
    if (capture.sceneOneDispatchHandle == kInvalidHandle
        || capture.sceneOneDispatchRecord == 0U
        || capture.sceneOnePoseHandle == kInvalidHandle
        || capture.sceneOnePoseRecord == 0U
        || capture.sceneOnePoseRelative == INT64_MIN
        || capture.sceneOnePose == 0U
        || capture.sceneOnePoseIdentity != kOmegaScenePoseObjectIdentity
        || capture.sceneOnePoseSize != kOmegaScenePoseObjectSize) {
        return fallback("active_live_pose_interface");
    }

    const std::int64_t filterRelative = safe_read<std::int64_t>(
        activeProvider + 0x70U, INT64_MIN);
    if (filterRelative == INT64_MIN) {
        return fallback("active_filter_context");
    }
    capture.sceneOneFilterContext =
        filterRelative == 0
            ? 0U
            : add_relative(
                  reinterpret_cast<std::uintptr_t>(activeProvider + 0x70U),
                  filterRelative);
    if (capture.sceneOneFilterContext != 0U
        && !readable_memory_range(
            reinterpret_cast<const void*>(capture.sceneOneFilterContext), 1U)) {
        return fallback("active_filter_unreadable");
    }

    const std::uint32_t nativeSelection = safe_read<std::uint32_t>(
        selection, kInvalidHandle);
    capture.providerTreatmentNativeSelection = nativeSelection;
    if (nativeSelection != kOmegaPurplePoseSelection
        || nativeSelection != capture.providerSelection) {
        return fallback("selection_mismatch");
    }

    const SceneTransformPoseCandidateResolve poseCandidate =
        g_sceneTransformPoseCandidateResolveOriginal.load(
            std::memory_order_acquire);
    if (poseCandidate == nullptr) {
        return fallback("pose_candidate_unavailable");
    }

    capture.providerTreatmentInputReadable = safe_copy(
        capture.providerTreatmentOutputBefore.data(),
        transformOutput,
        capture.providerTreatmentOutputBefore.size());
    if (!capture.providerTreatmentInputReadable) {
        return fallback("output_unreadable");
    }

    capture.providerTreatmentEligible = true;
    const std::array<std::uintptr_t, 2U> livePoseInterface{
        capture.sceneOneDispatchRecord,
        capture.sceneOnePose};
    capture.sceneOneCandidateResult = poseCandidate(
        activeProvider,
        kOmegaSceneOnePoseCandidateIndex,
        resolverContext,
        mode,
        reinterpret_cast<const void*>(capture.sceneOneFilterContext),
        reinterpret_cast<const std::byte*>(capture.providerTarget),
        livePoseInterface.data(),
        0,
        static_cast<std::byte*>(transformOutput));
    capture.providerTreatmentResult =
        capture.sceneOneCandidateResult != 0U ? 1 : 0;
    capture.providerTreatmentOutputReadable = safe_copy(
        capture.providerTreatmentOutputAfter.data(),
        transformOutput,
        capture.providerTreatmentOutputAfter.size());
    const DecodedSceneTransform treatmentTransform =
        decode_scene_transform_address(
            static_cast<const std::byte*>(transformOutput) + 0x10U);
    if (capture.providerTreatmentResult != 1
        || !capture.providerTreatmentOutputReadable
        || !treatmentTransform.readable || !treatmentTransform.finite) {
        return fallback(capture.providerTreatmentResult != 1
                            ? "treatment_result"
                            : !capture.providerTreatmentOutputReadable
                                  ? "treatment_output_unreadable"
                                  : "treatment_transform");
    }

    capture.providerTreatmentApplied = true;
    capture.providerTreatmentOutputCommitted = true;
    capture.providerTreatmentReason = "applied";
    g_sceneTransformKind2ProviderTreatmentApplied.fetch_add(
        1U, std::memory_order_relaxed);
    return capture.providerTreatmentResult;
}

/**
 * Records the exact resolver invoked by source kind 2 and arms its pose-row recorder.
 *
 * Static analysis proves that +A1F360 reads only runtime +0/+8/+0x10, resolves the +0/+8
 * datum/relative pair to a separate provider, and passes that provider downstream. +A1E8A0 then
 * exposes the provider row's binding key and virtual pose interface. The exact +A1F640 treatment
 * may replace native candidate 7 with authored candidate 8 while keeping every live provider
 * input unchanged; the enclosing recorder captures its committed output or native fallback.
 */
__declspec(noinline) std::int32_t __fastcall scene_transform_kind2_resolve(
    const std::byte* runtime,
    std::uint32_t outputCapacity,
    void* transformOutput,
    std::uintptr_t resolverContext,
    std::uint8_t mode) noexcept {
    const SceneTransformRecorderCallGuard activeCall{
        g_sceneTransformKind2ResolveActiveCalls};
    const SceneTransformKind2Resolve original =
        g_sceneTransformKind2ResolveOriginal.load(std::memory_order_acquire);
    if (original == nullptr) {
        return 0;
    }

    SceneTransformKind2ResolveCapture& capture = g_sceneTransformKind2ResolveCapture;
    const bool inspect = capture.armed && runtime != nullptr
                         && runtime == capture.expectedRuntime && !capture.called;
    if (!inspect) {
        return original(runtime, outputCapacity, transformOutput, resolverContext, mode);
    }

    capture.called = true;
    capture.resolverCall =
        g_sceneTransformKind2ResolveCalls.fetch_add(1U, std::memory_order_relaxed) + 1U;
    capture.runtime = runtime;
    capture.outputCapacity = outputCapacity;
    capture.transformOutput = transformOutput;
    capture.resolverContext = resolverContext;
    capture.mode = mode;
    capture.outputBeforeReadable = safe_copy(
        capture.outputBefore.data(), transformOutput, capture.outputBefore.size());
    capture.provider = resolve_scene_relative_record(runtime,
                                                     capture.providerHandle,
                                                     capture.providerRelative,
                                                     capture.providerRecord);
    capture.providerOwner = locate_ikora_component_address(
        reinterpret_cast<const void*>(capture.provider));
    capture.providerReadable = safe_copy(
        capture.providerBytes.data(),
        reinterpret_cast<const void*>(capture.provider),
        capture.providerBytes.size());
    capture.providerTarget = resolve_scene_relative_record(
        reinterpret_cast<const std::byte*>(capture.provider),
        capture.providerTargetHandle,
        capture.providerTargetRelative,
        capture.providerTargetRecord);
    capture.providerTargetOwner = locate_ikora_component_address(
        reinterpret_cast<const void*>(capture.providerTarget));
    capture.providerTargetReadable = safe_copy(
        capture.providerTargetBytes.data(),
        reinterpret_cast<const void*>(capture.providerTarget),
        capture.providerTargetBytes.size());
    if (capture.providerTargetHandle != kInvalidHandle
        && capture.providerTargetRecord != 0U
        && capture.providerTargetRelative != INT64_MIN
        && capture.providerTargetRelative != 0
        && capture.providerTarget != 0U) {
        g_scenePoseProviderTargetHandle.store(
            capture.providerTargetHandle, std::memory_order_release);
        g_scenePoseProviderTargetRecord.store(
            capture.providerTargetRecord, std::memory_order_release);
        g_scenePoseProviderTargetRelative.store(
            capture.providerTargetRelative, std::memory_order_release);
        g_scenePoseProviderTarget.store(
            capture.providerTarget, std::memory_order_release);
    }
    capture.runtimeSelector = safe_read<std::uint32_t>(runtime + 0x10U, UINT32_MAX);
    capture.resolverStride = safe_read<std::uint32_t>(
        reinterpret_cast<const void*>(resolverContext == 0U ? 0U : resolverContext + 0x14U),
        0U);
    if (capture.providerTarget != 0U) {
        capture.providerTableAnchor = capture.providerTarget + 0x58U;
        capture.providerTableRelative = safe_read<std::int64_t>(
            reinterpret_cast<const void*>(capture.providerTableAnchor), INT64_MIN);
        if (capture.providerTableRelative != INT64_MIN
            && capture.providerTableRelative != 0) {
            capture.providerTable = add_relative(
                capture.providerTableAnchor, capture.providerTableRelative);
        }
    }
    capture.providerTableOwner = locate_ikora_component_address(
        reinterpret_cast<const void*>(capture.providerTable));
    const std::uint64_t selectionOffset =
        0x40ULL
        + static_cast<std::uint64_t>(capture.runtimeSelector)
              * static_cast<std::uint64_t>(capture.resolverStride);
    if (capture.providerTable != 0U && capture.runtimeSelector != UINT32_MAX
        && capture.resolverStride != 0U && selectionOffset < 0x40000000ULL) {
        capture.providerSelectionAddress = add_relative(
            capture.providerTable, static_cast<std::int64_t>(selectionOffset));
        capture.providerSelection = safe_read<std::uint32_t>(
            reinterpret_cast<const void*>(capture.providerSelectionAddress), kInvalidHandle);
    }
    capture.providerSelectionOwner = locate_ikora_component_address(
        reinterpret_cast<const void*>(capture.providerSelectionAddress));

    const std::int32_t result =
        original(runtime, outputCapacity, transformOutput, resolverContext, mode);
    capture.result = result;
    capture.outputAfterReadable = safe_copy(
        capture.outputAfter.data(), transformOutput, capture.outputAfter.size());
    return result;
}

/**
 * Records the native authored source that populates opening-scene transform-bank entry 1.
 *
 * +58FA20 walks the authored source descriptors and calls this function once per row. Bytes 1/2
 * name the destination start/count and +0x0C names the cast slot, so filtering here directly
 * distinguishes an animated cast-member socket from a static marker or stand-in transform.
 */
__declspec(noinline) void __fastcall scene_transform_source_update(
    std::byte* bank,
    std::uint32_t* scene,
    const std::byte* descriptor,
    const std::byte* runtime) noexcept {
    const SceneTransformRecorderCallGuard activeCall{
        g_sceneTransformSourceUpdateActiveCalls};
    const SceneTransformSourceUpdate original =
        g_sceneTransformSourceUpdateOriginal.load(std::memory_order_acquire);
    if (original == nullptr) {
        return;
    }

    const std::uint32_t sceneHandle = safe_read<std::uint32_t>(scene, kInvalidHandle);
    const std::uint8_t outputStart = safe_read<std::uint8_t>(
        descriptor == nullptr ? nullptr : descriptor + 0x01U, 0U);
    const std::uint8_t outputCount = safe_read<std::uint8_t>(
        descriptor == nullptr ? nullptr : descriptor + 0x02U, 0U);
    const std::uint16_t outputEnd = static_cast<std::uint16_t>(outputStart)
                                    + static_cast<std::uint16_t>(outputCount);
    constexpr std::uint8_t kSelectedEntry = 1U;
    const bool inspect = sceneHandle == kOmegaIkoraOpeningSceneHandle
                         && descriptor != nullptr
                         && outputCount != 0U
                         && outputStart <= kSelectedEntry
                         && kSelectedEntry < outputEnd
                         && omega_forced();
    if (!inspect) {
        original(bank, scene, descriptor, runtime);
        return;
    }

    const std::uint32_t call =
        g_sceneTransformWriterCalls.fetch_add(1U, std::memory_order_relaxed) + 1U;
    const std::uint8_t sourceKind = safe_read<std::uint8_t>(descriptor + 0x00U, 0xFFU);
    const std::uint8_t flags = safe_read<std::uint8_t>(descriptor + 0x04U, 0U);
    const std::int32_t castIndex = static_cast<std::int32_t>(
        safe_read<std::int8_t>(descriptor + 0x0CU, static_cast<std::int8_t>(-1)));
    std::array<std::uint32_t, 4U> descriptorWords{};
    std::array<std::uint64_t, 3U> runtimeWords{};
    const bool descriptorReadable = safe_copy(
        descriptorWords.data(), descriptor, sizeof descriptorWords);
    const bool runtimeReadable = safe_copy(
        runtimeWords.data(), runtime, sizeof runtimeWords);

    const auto bankAddress = reinterpret_cast<std::uintptr_t>(bank);
    const std::uint64_t transformCount = safe_read<std::uint64_t>(
        reinterpret_cast<const void*>(bankAddress + 0x30U), 0U);
    const std::int64_t transformRelative = safe_read<std::int64_t>(
        reinterpret_cast<const void*>(bankAddress + 0x38U), INT64_MIN);
    const std::uintptr_t transformVector =
        transformRelative == INT64_MIN
            ? 0U
            : add_relative(bankAddress + 0x38U, transformRelative);
    const std::uintptr_t selectedAddress =
        transformVector == 0U || transformCount <= kSelectedEntry
            ? 0U
            : transformVector + 0x10U
                  + static_cast<std::uintptr_t>(kSelectedEntry) * 0x20U;
    std::array<std::byte, 0x20U> beforeBytes{};
    const bool beforeReadable = safe_copy(
        beforeBytes.data(), reinterpret_cast<const void*>(selectedAddress), beforeBytes.size());
    const DecodedSceneTransform before = decode_scene_transform_address(
        reinterpret_cast<const void*>(selectedAddress));

    const std::int64_t runtimeRelative = safe_read<std::int64_t>(
        reinterpret_cast<const void*>(bankAddress + 0x58U), INT64_MIN);
    const std::uintptr_t runtimeVector =
        runtimeRelative == INT64_MIN
            ? 0U
            : add_relative(bankAddress + 0x58U, runtimeRelative);
    const std::uintptr_t runtimeRows = runtimeVector == 0U ? 0U : runtimeVector + 0x10U;
    std::int64_t sourceIndex = -1;
    const std::uintptr_t runtimeAddress = reinterpret_cast<std::uintptr_t>(runtime);
    if (runtimeRows != 0U && runtimeAddress >= runtimeRows
        && (runtimeAddress - runtimeRows) % 0x18U == 0U) {
        sourceIndex = static_cast<std::int64_t>((runtimeAddress - runtimeRows) / 0x18U);
    }

    const SceneActorSlotCapture cast = castIndex >= 0 && castIndex <= 31
                                           ? capture_scene_actor_slot(
                                                 scene,
                                                 static_cast<std::uint32_t>(castIndex))
                                           : SceneActorSlotCapture{};
    const IkoraActor actor1 = ikora_factory_actor(1U);
    const IkoraActor actor2 = ikora_factory_actor(2U);
    const IkoraActor actor3 = ikora_factory_actor(3U);

    const IkoraComponentAddressOwner runtimeOwner =
        locate_ikora_component_address(runtime);
    const bool actor1Current = !actor1.released
                               && object_handle_is_current(actor1.object);
    const bool actor2Current = !actor2.released
                               && object_handle_is_current(actor2.object);
    const bool exactKind2Source = sourceKind == 2U && outputStart == 1U
                                  && outputCount == 1U && flags == 0x3CU
                                  && sourceIndex == 1;
    const std::uintptr_t expectedRuntimeAddress =
        runtimeRows != 0U && runtimeRows <= UINTPTR_MAX - 0x18U
            ? runtimeRows + 0x18U
            : 0U;
    const bool runtimeMatchesBankRow = runtimeAddress != 0U
                                       && runtimeAddress == expectedRuntimeAddress;
    const bool bankSlotResolved = selectedAddress != 0U && beforeReadable;
    const bool exactNativeRow = exactKind2Source && runtimeMatchesBankRow
                                && bankSlotResolved;
    const bool armKind2Resolver = exactNativeRow && actor1Current && actor2Current;
    if (!armKind2Resolver && call <= 8U) {
        const char* const rejectionReason =
            !exactKind2Source
                ? "source_signature_mismatch"
                : !runtimeMatchesBankRow
                      ? "runtime_not_native_bank_row_1"
                      : selectedAddress == 0U
                            ? "bank_slot_unresolved"
                            : !beforeReadable
                                  ? "bank_slot_unreadable"
                                  : !actor1Current
                                        ? "scene1_actor_not_current"
                                        : !actor2Current
                                              ? "scene2_actor_not_current"
                                              : "unknown";
        report("ev=omega_scene_kind2_resolver stage=arm_reject writer_call=%u "
               "reason=%s scene=%08X exact_source=%s kind=%u output_start=%u "
               "output_count=%u flags=%02X source_index=%lld bank=%p "
               "transform_vector=%p selected_slot=%p slot_readable=%s "
               "runtime_rows=%p expected_runtime=%p runtime=%p runtime_match=%s "
               "runtime_owner_found=%s runtime_factory=%u runtime_definition=%08X "
               "runtime_component=%p runtime_offset=0x%llX runtime_size=0x%llX "
               "actor1=%08X actor1_current=%s actor2=%08X actor2_current=%s "
               "mutation=observe_only",
               call,
               rejectionReason,
               sceneHandle,
               exactKind2Source ? "yes" : "no",
               static_cast<unsigned int>(sourceKind),
               static_cast<unsigned int>(outputStart),
               static_cast<unsigned int>(outputCount),
               static_cast<unsigned int>(flags),
               static_cast<long long>(sourceIndex),
               bank,
               reinterpret_cast<void*>(transformVector),
               reinterpret_cast<void*>(selectedAddress),
               beforeReadable ? "yes" : "no",
               reinterpret_cast<void*>(runtimeRows),
               reinterpret_cast<void*>(expectedRuntimeAddress),
               runtime,
               runtimeMatchesBankRow ? "yes" : "no",
               runtimeOwner.found ? "yes" : "no",
               runtimeOwner.factorySequence,
               runtimeOwner.definition,
               reinterpret_cast<void*>(runtimeOwner.instance),
               static_cast<unsigned long long>(runtimeOwner.offset),
               static_cast<unsigned long long>(runtimeOwner.size),
               actor1.object,
               actor1Current ? "yes" : "no",
               actor2.object,
               actor2Current ? "yes" : "no");
    }
    SceneTransformKind2ResolveCapture previousKind2Capture{};
    if (armKind2Resolver) {
        previousKind2Capture = g_sceneTransformKind2ResolveCapture;
        g_sceneTransformKind2ResolveCapture = {};
        g_sceneTransformKind2ResolveCapture.armed = true;
        g_sceneTransformKind2ResolveCapture.writerCall = call;
        g_sceneTransformKind2ResolveCapture.expectedRuntime = runtime;
    }

    original(bank, scene, descriptor, runtime);

    SceneTransformKind2ResolveCapture kind2Capture{};
    if (armKind2Resolver) {
        kind2Capture = g_sceneTransformKind2ResolveCapture;
        g_sceneTransformKind2ResolveCapture = previousKind2Capture;
    }
    const DecodedSceneTransform nativeAfter = decode_scene_transform_address(
        reinterpret_cast<const void*>(selectedAddress));
    const char* const providerMutation =
        kind2Capture.providerTreatmentApplied
            ? "same_call_candidate8_row_swap"
            : kind2Capture.providerTreatmentFallback ? "native_fallback"
                                                      : "observe_only";

    std::array<std::byte, 0x20U> afterBytes{};
    const bool afterReadable = safe_copy(
        afterBytes.data(), reinterpret_cast<const void*>(selectedAddress), afterBytes.size());
    const DecodedSceneTransform after = decode_scene_transform_address(
        reinterpret_cast<const void*>(selectedAddress));
    const bool changed = beforeReadable && afterReadable
                         && std::memcmp(beforeBytes.data(), afterBytes.data(), beforeBytes.size())
                                != 0;
    const std::uint64_t now = GetTickCount64();
    std::uint64_t previousTick =
        g_sceneTransformWriterLastTick.load(std::memory_order_acquire);
    bool heartbeat = false;
    while (now - previousTick >= kSceneEventHeartbeatMs) {
        if (g_sceneTransformWriterLastTick.compare_exchange_weak(
                previousTick, now, std::memory_order_acq_rel, std::memory_order_acquire)) {
            heartbeat = true;
            break;
        }
    }
    if (armKind2Resolver) {
        const std::uintptr_t previousProvider =
            g_sceneTransformKind2LastProvider.exchange(
                kind2Capture.provider, std::memory_order_acq_rel);
        const std::uint32_t previousCandidate =
            g_sceneTransformKind2LastCandidate.exchange(
                kind2Capture.providerSelection, std::memory_order_acq_rel);
        const std::int32_t previousResult =
            g_sceneTransformKind2LastResult.exchange(
                kind2Capture.result, std::memory_order_acq_rel);
        const bool structuralChange = previousProvider != kind2Capture.provider
                                      || previousCandidate
                                             != kind2Capture.providerSelection
                                      || previousResult != kind2Capture.result;
        std::uint64_t previousKind2Tick =
            g_sceneTransformKind2LastTick.load(std::memory_order_acquire);
        bool kind2Heartbeat = false;
        while (now - previousKind2Tick >= kSceneEventHeartbeatMs) {
            if (g_sceneTransformKind2LastTick.compare_exchange_weak(
                    previousKind2Tick,
                    now,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                kind2Heartbeat = true;
                break;
            }
        }
        const bool emitKind2 = call <= 8U || structuralChange || kind2Heartbeat;
        if (emitKind2) {
            const std::uint32_t kind2Sample =
                g_sceneTransformKind2Samples.fetch_add(1U, std::memory_order_relaxed) + 1U;
            if (kind2Sample <= kMaximumSceneTransformKind2Samples) {
                std::array<std::uint64_t, 6U> outputWords{};
                if (kind2Capture.outputAfterReadable) {
                    std::memcpy(outputWords.data(),
                                kind2Capture.outputAfter.data(),
                                sizeof outputWords);
                }
                report("ev=omega_scene_kind2_resolver stage=call n=%u writer_call=%u "
                       "resolver_call=%u resolver_called=%s scene=%08X kind=%u "
                       "source_index=%lld runtime=%p runtime_owner_found=%s "
                       "runtime_factory=%u runtime_definition=%08X runtime_component=%p "
                       "runtime_offset=0x%llX runtime_size=0x%llX "
                       "runtime_words=%016llX,%016llX,%016llX output_capacity=%u "
                       "output=%p resolver_context=%p mode=%u result=%d "
                       "mutation=%s",
                       kind2Sample,
                       call,
                       kind2Capture.resolverCall,
                       kind2Capture.called ? "yes" : "no",
                       sceneHandle,
                       static_cast<unsigned int>(sourceKind),
                       static_cast<long long>(sourceIndex),
                       runtime,
                       runtimeOwner.found ? "yes" : "no",
                       runtimeOwner.factorySequence,
                       runtimeOwner.definition,
                       reinterpret_cast<void*>(runtimeOwner.instance),
                       static_cast<unsigned long long>(runtimeOwner.offset),
                       static_cast<unsigned long long>(runtimeOwner.size),
                       static_cast<unsigned long long>(runtimeWords[0]),
                       static_cast<unsigned long long>(runtimeWords[1]),
                       static_cast<unsigned long long>(runtimeWords[2]),
                       kind2Capture.outputCapacity,
                       kind2Capture.transformOutput,
                       reinterpret_cast<void*>(kind2Capture.resolverContext),
                       static_cast<unsigned int>(kind2Capture.mode),
                       kind2Capture.result,
                       providerMutation);
                report("ev=omega_scene_kind2_resolver stage=provider n=%u "
                       "provider_handle=%08X provider_record=%p provider_relative=%lld "
                       "provider=%p provider_readable=%s provider_hash=%016llX "
                       "provider_owner_found=%s provider_owner_factory=%u "
                       "provider_owner_definition=%08X provider_owner_component=%p "
                       "provider_owner_offset=0x%llX target_handle=%08X "
                       "target_record=%p target_relative=%lld target=%p "
                       "target_readable=%s target_hash=%016llX target_owner_found=%s "
                       "target_owner_factory=%u target_owner_definition=%08X "
                       "target_owner_component=%p target_owner_offset=0x%llX "
                       "mutation=%s",
                       kind2Sample,
                       kind2Capture.providerHandle,
                       reinterpret_cast<void*>(kind2Capture.providerRecord),
                       static_cast<long long>(kind2Capture.providerRelative),
                       reinterpret_cast<void*>(kind2Capture.provider),
                       kind2Capture.providerReadable ? "yes" : "no",
                       static_cast<unsigned long long>(
                           kind2Capture.providerReadable
                               ? hash_region(kind2Capture.providerBytes.data(),
                                             kind2Capture.providerBytes.size())
                               : 0U),
                       kind2Capture.providerOwner.found ? "yes" : "no",
                       kind2Capture.providerOwner.factorySequence,
                       kind2Capture.providerOwner.definition,
                       reinterpret_cast<void*>(kind2Capture.providerOwner.instance),
                       static_cast<unsigned long long>(kind2Capture.providerOwner.offset),
                       kind2Capture.providerTargetHandle,
                       reinterpret_cast<void*>(kind2Capture.providerTargetRecord),
                       static_cast<long long>(kind2Capture.providerTargetRelative),
                       reinterpret_cast<void*>(kind2Capture.providerTarget),
                       kind2Capture.providerTargetReadable ? "yes" : "no",
                       static_cast<unsigned long long>(
                           kind2Capture.providerTargetReadable
                               ? hash_region(kind2Capture.providerTargetBytes.data(),
                                             kind2Capture.providerTargetBytes.size())
                               : 0U),
                       kind2Capture.providerTargetOwner.found ? "yes" : "no",
                       kind2Capture.providerTargetOwner.factorySequence,
                       kind2Capture.providerTargetOwner.definition,
                       reinterpret_cast<void*>(kind2Capture.providerTargetOwner.instance),
                       static_cast<unsigned long long>(
                           kind2Capture.providerTargetOwner.offset),
                       providerMutation);
                report("ev=omega_scene_kind2_resolver stage=selection n=%u "
                       "definition_base=%p definition_owner_found=%s "
                       "definition_owner_factory=%u definition_owner_definition=%08X "
                       "definition_owner_component=%p definition_owner_offset=0x%llX "
                       "table_anchor=%p table_relative=%lld table=%p "
                       "table_owner_found=%s table_owner_factory=%u "
                       "table_owner_definition=%08X table_owner_component=%p "
                       "table_owner_offset=0x%llX runtime_selector=%u resolver_stride=%u "
                       "selection_address=%p selection=%08X selection_owner_found=%s "
                       "selection_owner_factory=%u selection_owner_definition=%08X "
                       "selection_owner_component=%p selection_owner_offset=0x%llX "
                       "mutation=%s",
                       kind2Sample,
                       reinterpret_cast<void*>(kind2Capture.providerTarget),
                       kind2Capture.providerTargetOwner.found ? "yes" : "no",
                       kind2Capture.providerTargetOwner.factorySequence,
                       kind2Capture.providerTargetOwner.definition,
                       reinterpret_cast<void*>(kind2Capture.providerTargetOwner.instance),
                       static_cast<unsigned long long>(
                           kind2Capture.providerTargetOwner.offset),
                       reinterpret_cast<void*>(kind2Capture.providerTableAnchor),
                       static_cast<long long>(kind2Capture.providerTableRelative),
                       reinterpret_cast<void*>(kind2Capture.providerTable),
                       kind2Capture.providerTableOwner.found ? "yes" : "no",
                       kind2Capture.providerTableOwner.factorySequence,
                       kind2Capture.providerTableOwner.definition,
                       reinterpret_cast<void*>(kind2Capture.providerTableOwner.instance),
                       static_cast<unsigned long long>(
                           kind2Capture.providerTableOwner.offset),
                       kind2Capture.runtimeSelector,
                       kind2Capture.resolverStride,
                       reinterpret_cast<void*>(kind2Capture.providerSelectionAddress),
                       kind2Capture.providerSelection,
                       kind2Capture.providerSelectionOwner.found ? "yes" : "no",
                       kind2Capture.providerSelectionOwner.factorySequence,
                       kind2Capture.providerSelectionOwner.definition,
                       reinterpret_cast<void*>(
                           kind2Capture.providerSelectionOwner.instance),
                       static_cast<unsigned long long>(
                           kind2Capture.providerSelectionOwner.offset),
                       providerMutation);
                for (std::size_t candidateIndex = 0U;
                     candidateIndex < kind2Capture.poseCandidateCount
                     && candidateIndex < kind2Capture.poseCandidates.size();
                     ++candidateIndex) {
                    const ScenePoseCandidateCapture& candidate =
                        kind2Capture.poseCandidates[candidateIndex];
                    const DecodedSceneTransform rowLocal =
                        decode_scene_transform_address(
                            candidate.rowReadable
                                ? candidate.rowBytes.data() + 0x10U
                                : nullptr);
                    const DecodedSceneTransform outputTransform =
                        decode_scene_transform_address(
                            candidate.outputAfterReadable
                                ? candidate.outputAfter.data() + 0x10U
                                : nullptr);
                    std::array<std::uint64_t, 3U> outputBindings{};
                    if (candidate.outputAfterReadable) {
                        std::memcpy(outputBindings.data(),
                                    candidate.outputAfter.data() + 0x30U,
                                    sizeof outputBindings);
                    }
                    const std::uint32_t outputBindingHandle =
                        static_cast<std::uint32_t>(outputBindings[0]);
                    const bool actor3Current = !actor3.released
                                               && object_handle_is_current(
                                                   actor3.object);
                    report("ev=omega_scene_pose_binding stage=candidate "
                           "n=%u candidate_n=%u candidate_count=%u "
                           "valid_target=%s actor1=%08X actor1_current=%s "
                           "actor3=%08X actor3_current=%s provider=%p "
                           "provider_owner_found=%s provider_owner_factory=%u "
                           "provider_owner_object=%08X provider_owner_definition=%08X "
                           "provider_component=%p provider_offset=0x%llX "
                           "candidate_index=%u resolver_stride=%u flags=%02X "
                           "allow_fallback=%u definition_base=%p table=%p row=%p "
                           "row_readable=%s row_word0=%08X binding_key=%08X "
                           "selection_key=%08X expected_selection=%08X "
                           "selection_matches=%s row_local_finite=%s "
                           "row_local_rotation=%.6f,%.6f,%.6f,%.6f "
                           "row_local_position=%.3f,%.3f,%.3f "
                           "pose_interface=%p pose_dispatch_base=%p pose_object=%p "
                           "dispatch_owner_factory=%u dispatch_owner_definition=%08X "
                           "pose_object_owner_factory=%u pose_object_owner_definition=%08X "
                           "filter_context=%p filter_owner_factory=%u "
                           "filter_owner_definition=%08X result=%016llX "
                           "output_readable=%s output_finite=%s "
                           "output_rotation=%.6f,%.6f,%.6f,%.6f "
                           "output_position=%.3f,%.3f,%.3f output_scale=%.6f "
                           "output_binding=%016llX,%016llX,%016llX "
                           "binding_handle=%08X binding_matches_scene1=%s "
                           "binding_matches_scene3=%s mutation=observe_only",
                           kind2Sample,
                           static_cast<unsigned int>(candidateIndex + 1U),
                           kind2Capture.poseCandidateCount,
                           candidate.providerOwner.factorySequence == 1U
                               ? "scene1_real_animation"
                               : candidate.providerOwner.factorySequence == 3U
                                     ? "scene3_real_animation"
                                     : "not_scene1_or_scene3",
                           actor1.object,
                           actor1Current ? "yes" : "no",
                           actor3.object,
                           actor3Current ? "yes" : "no",
                           reinterpret_cast<void*>(candidate.provider),
                           candidate.providerOwner.found ? "yes" : "no",
                           candidate.providerOwner.factorySequence,
                           candidate.providerOwner.object,
                           candidate.providerOwner.definition,
                           reinterpret_cast<void*>(candidate.providerOwner.instance),
                           static_cast<unsigned long long>(
                               candidate.providerOwner.offset),
                           candidate.candidateIndex,
                           candidate.resolverStride,
                           static_cast<unsigned int>(candidate.flags),
                           candidate.allowFallback != 0 ? 1U : 0U,
                           reinterpret_cast<void*>(candidate.definitionBase),
                           reinterpret_cast<void*>(candidate.table),
                           reinterpret_cast<void*>(candidate.row),
                           candidate.rowReadable ? "yes" : "no",
                           candidate.rowWord0,
                           candidate.bindingKey,
                           candidate.selectionKey,
                           kind2Capture.providerSelection,
                           candidate.selectionKey
                                   == kind2Capture.providerSelection
                               ? "yes"
                               : "no",
                           rowLocal.finite ? "yes" : "no",
                           rowLocal.rotation[0],
                           rowLocal.rotation[1],
                           rowLocal.rotation[2],
                           rowLocal.rotation[3],
                           rowLocal.position[0],
                           rowLocal.position[1],
                           rowLocal.position[2],
                           reinterpret_cast<void*>(candidate.poseInterface),
                           reinterpret_cast<void*>(candidate.poseDispatchBase),
                           reinterpret_cast<void*>(candidate.poseObject),
                           candidate.poseDispatchOwner.factorySequence,
                           candidate.poseDispatchOwner.definition,
                           candidate.poseObjectOwner.factorySequence,
                           candidate.poseObjectOwner.definition,
                           reinterpret_cast<void*>(candidate.filterContext),
                           candidate.filterOwner.factorySequence,
                           candidate.filterOwner.definition,
                           static_cast<unsigned long long>(candidate.result),
                           candidate.outputAfterReadable ? "yes" : "no",
                           outputTransform.finite ? "yes" : "no",
                           outputTransform.rotation[0],
                           outputTransform.rotation[1],
                           outputTransform.rotation[2],
                           outputTransform.rotation[3],
                           outputTransform.position[0],
                           outputTransform.position[1],
                           outputTransform.position[2],
                           outputTransform.scale,
                           static_cast<unsigned long long>(outputBindings[0]),
                           static_cast<unsigned long long>(outputBindings[1]),
                           static_cast<unsigned long long>(outputBindings[2]),
                           outputBindingHandle,
                           outputBindingHandle == actor1.object ? "yes" : "no",
                           outputBindingHandle == actor3.object ? "yes" : "no");
                }
                report_scene_pose_provider_targets();
        report("ev=omega_purple_provider_reroute stage=treatment n=%u "
               "called=%s eligible=%s applied=%s fallback=%s reason=%s "
                       "active_factory=%u active_object=%08X active_definition=%08X "
                       "active_component=%p active_component_size=0x%llX "
                       "active_provider=%p active_offset=0x%llX "
                       "candidate_component=%p candidate_component_size=0x%llX "
                       "candidate_provider=%p candidate_provider_offset=0x%llX "
                       "candidate_provider_readable=%s candidate_provider_hash=%016llX "
                       "active_definition=%p candidate_table=%p candidate_row=%p "
                       "candidate_definition_readable=%s row_binding=%08X "
                       "row_selection=%08X live_dispatch_handle=%08X "
                       "live_dispatch=%p live_pose_handle=%08X live_pose_record=%p "
                       "live_pose_relative=%lld live_pose=%p pose_words=%016llX,%016llX "
                       "filter_context=%p native_selection=%08X "
                       "candidate_result=%016llX treatment_result=%d fallback_result=%d "
                       "input_readable=%s treatment_output_readable=%s "
                       "output_committed=%s treatment_before_hash=%016llX "
                       "treatment_after_hash=%016llX treatment_calls=%u "
                       "applied_total=%u fallback_total=%u mutation=%s",
                       kind2Sample,
                       kind2Capture.providerTreatmentCalled ? "yes" : "no",
                       kind2Capture.providerTreatmentEligible ? "yes" : "no",
                       kind2Capture.providerTreatmentApplied ? "yes" : "no",
                       kind2Capture.providerTreatmentFallback ? "yes" : "no",
                       kind2Capture.providerTreatmentReason == nullptr
                           ? "unknown"
                           : kind2Capture.providerTreatmentReason,
                       kind2Capture.providerOwner.factorySequence,
                       kind2Capture.providerOwner.object,
                       kind2Capture.providerOwner.definition,
                       reinterpret_cast<void*>(kind2Capture.providerOwner.instance),
                       static_cast<unsigned long long>(kind2Capture.providerOwner.size),
                       reinterpret_cast<void*>(kind2Capture.provider),
                       static_cast<unsigned long long>(kind2Capture.providerOwner.offset),
                       reinterpret_cast<void*>(kind2Capture.sceneOneProviderComponent),
                       static_cast<unsigned long long>(
                           kind2Capture.sceneOneProviderComponentSize),
                       reinterpret_cast<void*>(kind2Capture.sceneOneProvider),
                       static_cast<unsigned long long>(
                           kind2Capture.sceneOneProviderOffset),
                       kind2Capture.sceneOneProviderReadable ? "yes" : "no",
                       static_cast<unsigned long long>(
                           kind2Capture.sceneOneProviderReadable
                               ? hash_region(kind2Capture.sceneOneProviderBytes.data(),
                                             kind2Capture.sceneOneProviderBytes.size())
                               : 0U),
                       reinterpret_cast<void*>(kind2Capture.sceneOneDefinitionBase),
                       reinterpret_cast<void*>(kind2Capture.sceneOneDefinitionTable),
                       reinterpret_cast<void*>(kind2Capture.sceneOneDefinitionRow),
                       kind2Capture.sceneOneDefinitionReadable ? "yes" : "no",
                       kind2Capture.sceneOneRowBinding,
                       kind2Capture.sceneOneRowSelection,
                       kind2Capture.sceneOneDispatchHandle,
                       reinterpret_cast<void*>(kind2Capture.sceneOneDispatchRecord),
                       kind2Capture.sceneOnePoseHandle,
                       reinterpret_cast<void*>(kind2Capture.sceneOnePoseRecord),
                       static_cast<long long>(kind2Capture.sceneOnePoseRelative),
                       reinterpret_cast<void*>(kind2Capture.sceneOnePose),
                       static_cast<unsigned long long>(kind2Capture.sceneOnePoseIdentity),
                       static_cast<unsigned long long>(kind2Capture.sceneOnePoseSize),
                       reinterpret_cast<void*>(kind2Capture.sceneOneFilterContext),
                       kind2Capture.providerTreatmentNativeSelection,
                       static_cast<unsigned long long>(
                           kind2Capture.sceneOneCandidateResult),
                       kind2Capture.providerTreatmentResult,
                       kind2Capture.providerTreatmentFallbackResult,
                       kind2Capture.providerTreatmentInputReadable ? "yes" : "no",
                       kind2Capture.providerTreatmentOutputReadable ? "yes" : "no",
                       kind2Capture.providerTreatmentOutputCommitted ? "yes" : "no",
                       static_cast<unsigned long long>(
                           kind2Capture.providerTreatmentInputReadable
                               ? hash_region(
                                     kind2Capture.providerTreatmentOutputBefore.data(),
                                     kind2Capture.providerTreatmentOutputBefore.size())
                               : 0U),
                       static_cast<unsigned long long>(
                           kind2Capture.providerTreatmentOutputReadable
                               ? hash_region(
                                     kind2Capture.providerTreatmentOutputAfter.data(),
                                     kind2Capture.providerTreatmentOutputAfter.size())
                               : 0U),
                       g_sceneTransformKind2ProviderTreatmentCalls.load(
                           std::memory_order_acquire),
                       g_sceneTransformKind2ProviderTreatmentApplied.load(
                           std::memory_order_acquire),
                       g_sceneTransformKind2ProviderTreatmentFallbacks.load(
                           std::memory_order_acquire),
                       providerMutation);
                report("ev=omega_scene_kind2_resolver stage=output n=%u "
                       "native_bank=%p native_slot=%p runtime_rows=%p "
                       "expected_runtime=%p runtime_matches_bank_row=%s "
                       "pose_candidate_count=%u binding_key=%08X "
                       "actor1=%08X actor1_current=%s actor2=%08X actor2_current=%s "
                       "output_before_readable=%s output_after_readable=%s "
                       "output_before_hash=%016llX output_after_hash=%016llX "
                       "output_words=%016llX,%016llX,%016llX,%016llX,%016llX,%016llX "
                       "bank_slot=%p bank_before_hash=%016llX bank_after_hash=%016llX "
                       "bank_changed=%s bank_after_pos=%.3f,%.3f,%.3f "
                       "mutation=%s",
                       kind2Sample,
                       bank,
                       reinterpret_cast<void*>(selectedAddress),
                       reinterpret_cast<void*>(runtimeRows),
                       reinterpret_cast<void*>(expectedRuntimeAddress),
                       runtimeMatchesBankRow ? "yes" : "no",
                       kind2Capture.poseCandidateCount,
                       g_scenePoseBindingKey.load(std::memory_order_acquire),
                       actor1.object,
                       actor1Current ? "yes" : "no",
                       actor2.object,
                       actor2Current ? "yes" : "no",
                       kind2Capture.outputBeforeReadable ? "yes" : "no",
                       kind2Capture.outputAfterReadable ? "yes" : "no",
                       static_cast<unsigned long long>(
                           kind2Capture.outputBeforeReadable
                               ? hash_region(kind2Capture.outputBefore.data(),
                                             kind2Capture.outputBefore.size())
                               : 0U),
                       static_cast<unsigned long long>(
                           kind2Capture.outputAfterReadable
                               ? hash_region(kind2Capture.outputAfter.data(),
                                             kind2Capture.outputAfter.size())
                               : 0U),
                       static_cast<unsigned long long>(outputWords[0]),
                       static_cast<unsigned long long>(outputWords[1]),
                       static_cast<unsigned long long>(outputWords[2]),
                       static_cast<unsigned long long>(outputWords[3]),
                       static_cast<unsigned long long>(outputWords[4]),
                       static_cast<unsigned long long>(outputWords[5]),
                       reinterpret_cast<void*>(selectedAddress),
                       static_cast<unsigned long long>(
                           beforeReadable
                               ? hash_region(beforeBytes.data(), beforeBytes.size())
                               : 0U),
                       static_cast<unsigned long long>(
                           afterReadable ? hash_region(afterBytes.data(), afterBytes.size())
                                         : 0U),
                       changed ? "yes" : "no",
                       after.position[0],
                       after.position[1],
                       after.position[2],
                       providerMutation);
            } else if (kind2Sample == kMaximumSceneTransformKind2Samples + 1U) {
                report("ev=omega_scene_kind2_resolver stage=sample_limit n=%u limit=%u "
                       "writer_calls=%u resolver_calls=%u mutation=%s",
                       kind2Sample,
                       kMaximumSceneTransformKind2Samples,
                       call,
                       g_sceneTransformKind2ResolveCalls.load(std::memory_order_acquire),
                       providerMutation);
            }
        }
    }
    const bool emit = call == 1U || (changed && call <= 128U) || heartbeat;
    if (!emit) {
        return;
    }
    const std::uint32_t sample =
        g_sceneTransformWriterSamples.fetch_add(1U, std::memory_order_relaxed) + 1U;
    if (sample > kMaximumSceneTransformWriterSamples) {
        if (sample == kMaximumSceneTransformWriterSamples + 1U) {
            report("ev=omega_scene_transform_writer stage=sample_limit n=%u limit=%u "
                   "calls=%u mutation=%s",
                   sample,
                   kMaximumSceneTransformWriterSamples,
                   call,
                   providerMutation);
        }
        return;
    }

    if (call == 1U) {
        report_scene_transform_writer_stack(
            call, sceneHandle, descriptor, runtime);
    }
    report("ev=omega_scene_transform_writer stage=slot_update n=%u call=%u "
           "scene=%08X bank=%p transform_count=%llu transform_relative=%lld "
           "transform_vector=%p slot=1 slot_address=%p kind=%u output_start=%u "
           "output_count=%u flags=%02X cast_index=%d source_index=%lld changed=%s "
           "before_readable=%s before_finite=%s before_rot=%.6f,%.6f,%.6f,%.6f "
           "before_pos=%.3f,%.3f,%.3f before_scale=%.6f after_readable=%s "
           "after_finite=%s after_rot=%.6f,%.6f,%.6f,%.6f "
           "after_pos=%.3f,%.3f,%.3f after_scale=%.6f mutation=%s",
           sample,
           call,
           sceneHandle,
           bank,
           static_cast<unsigned long long>(transformCount),
           static_cast<long long>(transformRelative),
           reinterpret_cast<void*>(transformVector),
           reinterpret_cast<void*>(selectedAddress),
           static_cast<unsigned int>(sourceKind),
           static_cast<unsigned int>(outputStart),
           static_cast<unsigned int>(outputCount),
           static_cast<unsigned int>(flags),
           castIndex,
           static_cast<long long>(sourceIndex),
           changed ? "yes" : "no",
           before.readable ? "yes" : "no",
           before.finite ? "yes" : "no",
           before.rotation[0],
           before.rotation[1],
           before.rotation[2],
           before.rotation[3],
           before.position[0],
           before.position[1],
           before.position[2],
           before.scale,
           after.readable ? "yes" : "no",
           after.finite ? "yes" : "no",
           after.rotation[0],
           after.rotation[1],
           after.rotation[2],
           after.rotation[3],
           after.position[0],
           after.position[1],
           after.position[2],
           after.scale,
           providerMutation);
    report("ev=omega_scene_transform_writer stage=source n=%u call=%u "
           "descriptor=%p descriptor_readable=%s descriptor_words=%08X,%08X,%08X,%08X "
           "source_runtime=%p runtime_readable=%s runtime_words=%016llX,%016llX,%016llX "
           "cast_valid=%s cast_slot=%u actor_count=%u authored_slot=%p "
           "authored=%08X,%08X,%08X,%08X,%08X,%08X runtime_slot=%p "
           "runtime_c8=%d runtime_cc=%d runtime_d4=%u actor1=%08X behavior1=%p "
           "released1=%u actor2=%08X behavior2=%p released2=%u "
           "actor3=%08X behavior3=%p released3=%u mutation=%s",
           sample,
           call,
           descriptor,
           descriptorReadable ? "yes" : "no",
           descriptorWords[0],
           descriptorWords[1],
           descriptorWords[2],
           descriptorWords[3],
           runtime,
           runtimeReadable ? "yes" : "no",
           static_cast<unsigned long long>(runtimeWords[0]),
           static_cast<unsigned long long>(runtimeWords[1]),
           static_cast<unsigned long long>(runtimeWords[2]),
           cast.valid ? "yes" : "no",
           cast.valid ? cast.slotIndex : kInvalidHandle,
           static_cast<unsigned int>(cast.actorCount),
           cast.authoredSlot,
           cast.authoredWords[0],
           cast.authoredWords[1],
           cast.authoredWords[2],
           cast.authoredWords[3],
           cast.authoredWords[4],
           cast.authoredWords[5],
           cast.runtimeSlot,
           cast.runtimeC8,
           cast.runtimeCC,
           static_cast<unsigned int>(cast.runtimeD4),
           actor1.object,
           reinterpret_cast<void*>(actor1.behaviorActor),
           actor1.released ? 1U : 0U,
           actor2.object,
           reinterpret_cast<void*>(actor2.behaviorActor),
           actor2.released ? 1U : 0U,
           actor3.object,
           reinterpret_cast<void*>(actor3.behaviorActor),
           actor3.released ? 1U : 0U,
           providerMutation);
}

/** Captures the transform resolved by the native type-23 callback without invoking it twice. */
__declspec(noinline) void __fastcall scene_type23_transform_resolve(
    std::uint32_t* context,
    std::uint32_t* seed,
    std::uint32_t selector,
    std::uint32_t mode,
    void* transformOutput,
    void* auxiliaryOutput) noexcept {
    const SceneType23TransformResolve original =
        g_sceneType23TransformResolveOriginal.load(std::memory_order_acquire);
    if (original == nullptr) {
        return;
    }
    if (g_sceneType23Depth == 0U) {
        original(context, seed, selector, mode, transformOutput, auxiliaryOutput);
        return;
    }

    SceneType23TransformCapture& capture = g_sceneType23TransformCapture;
    capture.called = true;
    capture.selector = selector;
    capture.mode = mode;
    capture.seedBefore = safe_read<std::uint32_t>(seed, 0U);
    capture.contextAddress = context;
    capture.sceneHandle = safe_read<std::uint32_t>(context, kInvalidHandle);
    capture.contextObject = safe_read<std::uint32_t>(
        reinterpret_cast<const std::byte*>(context) + 0x2CU, kInvalidHandle);
    capture.contextOffset = safe_read<std::uint64_t>(
        reinterpret_cast<const std::byte*>(context) + 0x08U, 0U);
    capture.bankAddress = reinterpret_cast<const std::byte*>(context) + 0x150U;
    capture.sceneRecordAddress = scene_object_record(capture.sceneHandle);
    capture.objectArgumentAddress = object_record(capture.contextObject);
    if (capture.sceneRecordAddress != nullptr && capture.contextOffset < 0x1000000ULL) {
        capture.programAddress = static_cast<const std::byte*>(capture.sceneRecordAddress)
                                 + 0x1A0U
                                 + static_cast<std::size_t>(capture.contextOffset);
    }
    capture.bankReadable = safe_copy(
        capture.bank.data(), capture.bankAddress, capture.bank.size());
    capture.programReadable = safe_copy(
        capture.program.data(), capture.programAddress, capture.program.size());
    if (capture.bankReadable) {
        const auto bank = reinterpret_cast<std::uintptr_t>(capture.bankAddress);
        capture.bankTransformCount = safe_read<std::uint64_t>(
            reinterpret_cast<const void*>(bank + 0x30U), 0U);
        capture.bankTransformRelative = safe_read<std::int64_t>(
            reinterpret_cast<const void*>(bank + 0x38U), INT64_MIN);
        capture.bankMetadataCount = safe_read<std::uint64_t>(
            reinterpret_cast<const void*>(bank + 0x40U), 0U);
        capture.bankMetadataRelative = safe_read<std::int64_t>(
            reinterpret_cast<const void*>(bank + 0x48U), INT64_MIN);
        capture.bankSecondaryCount = safe_read<std::uint64_t>(
            reinterpret_cast<const void*>(bank + 0x50U), 0U);
        capture.bankSecondaryRelative = safe_read<std::int64_t>(
            reinterpret_cast<const void*>(bank + 0x58U), INT64_MIN);

        const std::uintptr_t transformBase =
            capture.bankTransformRelative == INT64_MIN
                ? 0U
                : add_relative(bank + 0x38U, capture.bankTransformRelative);
        const std::uintptr_t metadataBase =
            capture.bankMetadataRelative == INT64_MIN
                ? 0U
                : add_relative(bank + 0x58U, capture.bankMetadataRelative);
        const std::uintptr_t secondaryBase =
            capture.bankSecondaryRelative == INT64_MIN
                ? 0U
                : add_relative(bank + 0x58U, capture.bankSecondaryRelative);
        capture.bankTransformsAddress = reinterpret_cast<const void*>(transformBase);
        capture.bankMetadataAddress = reinterpret_cast<const void*>(metadataBase);
        capture.bankSecondaryAddress = reinterpret_cast<const void*>(secondaryBase);
        // Omega's portal scene has three entries and Ikora's opening scene has six. Refuse
        // malformed vectors instead of following an unbounded relative pointer from a corrupt
        // scene context; the fixed capture buffer intentionally retains only the first six.
        if (capture.bankTransformCount >= 3U && capture.bankTransformCount <= 0x1000U) {
            capture.bankTransformsReadable = safe_copy(capture.bankTransforms.data(),
                                                        capture.bankTransformsAddress,
                                                        capture.bankTransforms.size());
        }
        if (capture.bankMetadataCount >= 3U && capture.bankMetadataCount <= 0x1000U) {
            capture.bankMetadataReadable = safe_copy(capture.bankMetadata.data(),
                                                      capture.bankMetadataAddress,
                                                      capture.bankMetadata.size());
        }
        if (capture.bankSecondaryCount >= 3U && capture.bankSecondaryCount <= 0x1000U) {
            capture.bankSecondaryReadable = safe_copy(capture.bankSecondary.data(),
                                                       capture.bankSecondaryAddress,
                                                       capture.bankSecondary.size());
        }
    }
    if (capture.programReadable) {
        const auto program = reinterpret_cast<std::uintptr_t>(capture.programAddress);
        capture.selectorRelative = safe_read<std::int64_t>(
            reinterpret_cast<const void*>(program + 0x18U), INT64_MIN);
        capture.descriptorRelative = safe_read<std::int64_t>(
            reinterpret_cast<const void*>(program + 0x28U), INT64_MIN);
        const std::uintptr_t selectorBase =
            capture.selectorRelative == INT64_MIN
                ? 0U
                : add_relative(program, capture.selectorRelative);
        if (selectorBase != 0U && selector <= 0x10000U) {
            capture.selectorRowAddress = reinterpret_cast<const void*>(
                selectorBase + static_cast<std::uintptr_t>(selector) * 6U + 0x28U);
            capture.selectorRowReadable = safe_copy(capture.selectorRow.data(),
                                                    capture.selectorRowAddress,
                                                    capture.selectorRow.size());
        }
        if (capture.selectorRowReadable) {
            const auto* const row = static_cast<const std::byte*>(
                capture.selectorRowAddress);
            capture.selectorSourceStart = safe_read<std::uint8_t>(row + 0x00U, 0U);
            capture.selectorSourceCount = safe_read<std::uint8_t>(row + 0x01U, 0U);
            capture.selectorDescriptorStart = safe_read<std::uint8_t>(row + 0x02U, 0U);
            capture.selectorDescriptorCount = safe_read<std::uint8_t>(row + 0x03U, 0U);
            capture.selectorFlags = safe_read<std::uint8_t>(row + 0x04U, 0U);
            const std::uint8_t directBit =
                (capture.seedBefore & 1U) != 0U ? 3U : 4U;
            capture.selectorUsesDirectBank =
                ((capture.selectorFlags >> directBit) & 1U) != 0U;
        }
        const std::uintptr_t descriptorBase =
            capture.descriptorRelative == INT64_MIN
                ? 0U
                : add_relative(program + 0x28U, capture.descriptorRelative);
        if (descriptorBase != 0U && capture.selectorRowReadable) {
            capture.descriptorAddress = reinterpret_cast<const void*>(
                descriptorBase
                + (static_cast<std::uintptr_t>(capture.selectorDescriptorStart) + 1U)
                      * 0x10U);
            capture.descriptorReadable = safe_copy(capture.descriptors.data(),
                                                   capture.descriptorAddress,
                                                   capture.descriptors.size());
        }
    }
    capture.transformAddress = transformOutput;
    capture.auxiliaryAddress = auxiliaryOutput;
    original(context, seed, selector, mode, transformOutput, auxiliaryOutput);
    capture.seedAfter = safe_read<std::uint32_t>(seed, 0U);
    capture.transformReadable = safe_copy(capture.transform.data(),
                                         transformOutput,
                                         capture.transform.size());
    capture.auxiliaryReadable = safe_copy(capture.auxiliary.data(),
                                         auxiliaryOutput,
                                         capture.auxiliary.size());
}

/**
 * Captures a type-23 visual while its native object record is still live, except for the exact
 * two-reference Omega suppression A/B.
 *
 * The outer dispatcher releases/recycles this short-lived record before returning to its caller.
 * At this boundary the handler has already written owner +0x20, event +0x24, and selector +0x16,
 * while the nested +58A150 output still belongs to the same invocation. During the scene-cast A/B,
 * this callback remains observational and always invokes the native implementation. The only
 * This callback remains observational. The bounded manual-offset treatment is applied earlier to
 * the exact live bank slot after its native source writer refreshes it.
 */
__declspec(noinline) void __fastcall scene_type23_callback(std::uint32_t* context) noexcept {
    const SceneType23Callback original =
        g_sceneType23CallbackOriginal.load(std::memory_order_acquire);
    if (original == nullptr) {
        return;
    }
    const bool inspect = g_sceneType23Depth == 0U && omega_forced()
                         && g_sceneAnimatedObject.load(std::memory_order_acquire)
                                != kInvalidHandle;
    if (!inspect) {
        original(context);
        return;
    }

    ++g_sceneType23Depth;
    g_sceneType23TransformCapture = {};
    const std::uint32_t call =
        g_sceneType23Calls.fetch_add(1U, std::memory_order_relaxed) + 1U;
    const std::int32_t eventIndex =
        static_cast<std::int32_t>(safe_read<std::uint32_t>(context, kInvalidHandle));
    const std::uintptr_t contextData = safe_read<std::uintptr_t>(
        reinterpret_cast<const std::byte*>(context) + 0x08U, 0U);
    const SceneAuthoredEventView view = resolve_scene_type23_context(context, eventIndex);
    const std::uint32_t contextOwner = safe_read<std::uint32_t>(
        reinterpret_cast<const void*>(contextData == 0U ? 0U : contextData + 0x24U),
        kInvalidHandle);
    const std::uint32_t contextObject = safe_read<std::uint32_t>(
        reinterpret_cast<const void*>(contextData == 0U ? 0U : contextData + 0x2CU),
        kInvalidHandle);
    const std::uint32_t actor1Before = ikora_factory_object(1U);
    const std::uint32_t actor2Before = ikora_factory_object(2U);
    const std::uint32_t actor3Before = ikora_factory_object(3U);
    const std::uint8_t authoredSelector = safe_read<std::uint8_t>(
        view.authored == nullptr ? nullptr : view.authored + 0x40U, 0xFFU);
    const std::uint32_t authoredReference = safe_read<std::uint32_t>(
        view.authored == nullptr ? nullptr : view.authored + 0x44U, 0U);

    original(context);

    const std::uint32_t actor1 = ikora_factory_object(1U);
    const std::uint32_t actor2 = ikora_factory_object(2U);
    const std::uint32_t actor3 = ikora_factory_object(3U);
    const std::uint32_t createdObject =
        view.runtime == nullptr
            ? kInvalidHandle
            : safe_read<std::uint32_t>(view.runtime + 0x40U, kInvalidHandle);
    const std::uint32_t createdSlot =
        createdObject == kInvalidHandle ? kInvalidHandle : createdObject & 0x1FFFU;
    const std::byte* const createdRecord = scene_object_record(createdObject);
    const std::uint32_t createdOwner = safe_read<std::uint32_t>(
        createdRecord == nullptr ? nullptr : createdRecord + 0x20U, kInvalidHandle);
    const std::uint32_t createdEvent = safe_read<std::uint32_t>(
        createdRecord == nullptr ? nullptr : createdRecord + 0x24U, kInvalidHandle);
    const std::uint8_t createdSelector = safe_read<std::uint8_t>(
        createdRecord == nullptr ? nullptr : createdRecord + 0x16U, 0xFFU);
    const std::uint32_t createdDefinition = safe_read<std::uint32_t>(
        createdRecord == nullptr ? nullptr : createdRecord + 0x4CU, 0U);
    const std::uint32_t createdFlags = safe_read<std::uint32_t>(
        createdRecord == nullptr ? nullptr : createdRecord + 0x04U, 0U);
    const std::uint32_t createdCurrentHandle = safe_read<std::uint32_t>(
        createdRecord == nullptr ? nullptr : createdRecord + 0x0CU, kInvalidHandle);
    const std::byte* const ownerRecord = object_record(createdOwner);
    const std::uint32_t ownerCurrentHandle = safe_read<std::uint32_t>(
        ownerRecord == nullptr ? nullptr : ownerRecord + 0x0CU, kInvalidHandle);
    const bool ownerRecordValid = createdOwner != kInvalidHandle
                                  && ownerCurrentHandle == createdOwner;
    const std::uint32_t ownerDefinition = safe_read<std::uint32_t>(
        ownerRecord == nullptr ? nullptr : ownerRecord + 0x4CU, 0U);
    const std::uint32_t ownerParent20 = safe_read<std::uint32_t>(
        ownerRecord == nullptr ? nullptr : ownerRecord + 0x20U, kInvalidHandle);
    const std::uint32_t ownerParent68 = safe_read<std::uint32_t>(
        ownerRecord == nullptr ? nullptr : ownerRecord + 0x68U, kInvalidHandle);
    const std::byte* const ownerSceneRecord = scene_object_record(createdOwner);
    const std::uint32_t ownerSceneCurrentHandle = safe_read<std::uint32_t>(
        ownerSceneRecord == nullptr ? nullptr : ownerSceneRecord + 0x0CU,
        kInvalidHandle);
    const bool ownerSceneRecordValid = createdOwner != kInvalidHandle
                                       && ownerSceneCurrentHandle == createdOwner;
    const std::uint32_t ownerSceneDefinition = safe_read<std::uint32_t>(
        ownerSceneRecord == nullptr ? nullptr : ownerSceneRecord + 0x4CU, 0U);
    const std::uint32_t ownerSceneParent20 = safe_read<std::uint32_t>(
        ownerSceneRecord == nullptr ? nullptr : ownerSceneRecord + 0x20U,
        kInvalidHandle);
    const std::uint32_t ownerSceneParent68 = safe_read<std::uint32_t>(
        ownerSceneRecord == nullptr ? nullptr : ownerSceneRecord + 0x68U,
        kInvalidHandle);
    const std::byte* const contextObjectRecord = object_record(contextObject);
    const std::uint32_t contextObjectCurrent = safe_read<std::uint32_t>(
        contextObjectRecord == nullptr ? nullptr : contextObjectRecord + 0x0CU,
        kInvalidHandle);
    const std::uint32_t contextObjectDefinition = safe_read<std::uint32_t>(
        contextObjectRecord == nullptr ? nullptr : contextObjectRecord + 0x4CU, 0U);
    const std::uint32_t contextObjectOwner20 = safe_read<std::uint32_t>(
        contextObjectRecord == nullptr ? nullptr : contextObjectRecord + 0x20U,
        kInvalidHandle);
    const std::uint32_t contextObjectOwner68 = safe_read<std::uint32_t>(
        contextObjectRecord == nullptr ? nullptr : contextObjectRecord + 0x68U,
        kInvalidHandle);

    const std::uint64_t now = GetTickCount64();
    bool first = false;
    bool actorChange = false;
    bool ownershipChange = false;
    bool heartbeat = false;
    bool reportCall = false;
    AcquireSRWLockExclusive(&g_sceneType23TraceStateLock);
    SceneType23TraceState* state = nullptr;
    for (std::size_t index = 0U; index < g_sceneType23TraceStateCount; ++index) {
        SceneType23TraceState& candidate = g_sceneType23TraceStates[index];
        if (candidate.valid
            && candidate.contextData == contextData
            && candidate.authored == reinterpret_cast<std::uintptr_t>(view.authored)
            && candidate.eventIndex == eventIndex) {
            state = &candidate;
            break;
        }
    }
    if (state == nullptr && g_sceneType23TraceStateCount < g_sceneType23TraceStates.size()) {
        state = &g_sceneType23TraceStates[g_sceneType23TraceStateCount++];
        *state = {};
        state->contextData = contextData;
        state->authored = reinterpret_cast<std::uintptr_t>(view.authored);
        state->eventIndex = eventIndex;
        state->contextOwner = kInvalidHandle;
        state->contextObject = kInvalidHandle;
        state->createdSlot = kInvalidHandle;
        state->createdOwner = kInvalidHandle;
        first = true;
    }
    if (state != nullptr) {
        actorChange = state->valid
                      && (state->actor1 != actor1 || state->actor2 != actor2
                          || state->actor3 != actor3);
        ownershipChange = state->valid
                          && (state->contextOwner != contextOwner
                              || state->contextObject != contextObject
                              || state->createdSlot != createdSlot
                              || state->createdOwner != createdOwner);
        heartbeat = state->valid
                    && now - state->lastLoggedAtMs >= kSceneEventHeartbeatMs;
        reportCall = first || actorChange || ownershipChange || heartbeat;
        state->valid = true;
        state->actor1 = actor1;
        state->actor2 = actor2;
        state->actor3 = actor3;
        state->contextOwner = contextOwner;
        state->contextObject = contextObject;
        state->createdSlot = createdSlot;
        state->createdOwner = createdOwner;
        ++state->calls;
        if (reportCall) {
            state->lastLoggedAtMs = now;
        }
    }
    ReleaseSRWLockExclusive(&g_sceneType23TraceStateLock);

    if (reportCall) {
        const std::uint32_t sample =
            g_sceneType23Samples.fetch_add(1U, std::memory_order_relaxed) + 1U;
        if (sample <= kMaximumSceneType23Samples) {
            const auto matchesActor = [](std::uint32_t candidate,
                                         std::uint32_t actor) noexcept {
                return actor != kInvalidHandle && candidate == actor;
            };
            const char* contextOwnerMatch = "other";
            if (contextOwner == kInvalidHandle) {
                contextOwnerMatch = "none";
            } else if (matchesActor(contextOwner, actor1)) {
                contextOwnerMatch = "actor1";
            } else if (matchesActor(contextOwner, actor2)) {
                contextOwnerMatch = "actor2";
            } else if (matchesActor(contextOwner, actor3)) {
                contextOwnerMatch = "actor3";
            }
            const char* createdOwnerMatch = "other";
            if (createdOwner == kInvalidHandle) {
                createdOwnerMatch = "none";
            } else if (matchesActor(createdOwner, actor1)) {
                createdOwnerMatch = "actor1";
            } else if (matchesActor(createdOwner, actor2)) {
                createdOwnerMatch = "actor2";
            } else if (matchesActor(createdOwner, actor3)) {
                createdOwnerMatch = "actor3";
            } else if ((ownerRecordValid
                        && (matchesActor(ownerParent20, actor1)
                            || matchesActor(ownerParent68, actor1)))
                       || (ownerSceneRecordValid
                           && (matchesActor(ownerSceneParent20, actor1)
                               || matchesActor(ownerSceneParent68, actor1)))) {
                createdOwnerMatch = "via_actor1";
            } else if ((ownerRecordValid
                        && (matchesActor(ownerParent20, actor2)
                            || matchesActor(ownerParent68, actor2)))
                       || (ownerSceneRecordValid
                           && (matchesActor(ownerSceneParent20, actor2)
                               || matchesActor(ownerSceneParent68, actor2)))) {
                createdOwnerMatch = "via_actor2";
            } else if ((ownerRecordValid
                        && (matchesActor(ownerParent20, actor3)
                            || matchesActor(ownerParent68, actor3)))
                       || (ownerSceneRecordValid
                           && (matchesActor(ownerSceneParent20, actor3)
                               || matchesActor(ownerSceneParent68, actor3)))) {
                createdOwnerMatch = "via_actor3";
            }
            const char* contextObjectMatch = "other";
            if (contextObject == kInvalidHandle) {
                contextObjectMatch = "none";
            } else if (matchesActor(contextObject, actor1)) {
                contextObjectMatch = "actor1";
            } else if (matchesActor(contextObject, actor2)) {
                contextObjectMatch = "actor2";
            } else if (matchesActor(contextObject, actor3)) {
                contextObjectMatch = "actor3";
            } else if (matchesActor(contextObjectOwner20, actor1)
                       || matchesActor(contextObjectOwner68, actor1)) {
                contextObjectMatch = "via_actor1";
            } else if (matchesActor(contextObjectOwner20, actor2)
                       || matchesActor(contextObjectOwner68, actor2)) {
                contextObjectMatch = "via_actor2";
            } else if (matchesActor(contextObjectOwner20, actor3)
                       || matchesActor(contextObjectOwner68, actor3)) {
                contextObjectMatch = "via_actor3";
            }

            const SceneType23TransformCapture& transform = g_sceneType23TransformCapture;
            report_vfx_renderer_correlation(sample,
                                            call,
                                            view,
                                            context,
                                            contextData,
                                            contextOwner,
                                            contextObject,
                                            createdObject,
                                            createdOwner,
                                            createdRecord,
                                            ownerRecord,
                                            ownerSceneRecord,
                                            contextObjectRecord,
                                            transform);
            report("ev=omega_scene_type23_trace stage=callback n=%u call=%u "
                   "first=%s actor_change=%s ownership_change=%s heartbeat=%s "
                   "context=%p context_data=%p context_owner=%08X context_owner_match=%s "
                   "event=%d scene_handle=%08X authored=%p authored_selector=%u "
                   "authored_reference=%08X runtime=%p actor1_before=%08X actor2_before=%08X "
                   "actor3_before=%08X actor1=%08X actor2=%08X actor3=%08X "
                   "created=%08X created_slot=%u created_current=%08X created_record=%p "
                   "created_flags=%08X created_definition=%08X created_owner=%08X "
                   "created_owner_match=%s created_event=%08X created_selector=%u "
                   "owner_record=%p owner_definition=%08X owner_parent20=%08X "
                   "owner_parent68=%08X transform_called=%s transform_selector=%u "
                   "transform_mode=%u transform_seed_before=%08X transform_seed_after=%08X "
                   "transform_address=%p transform_readable=%s transform_hash=%016llX "
                   "aux_address=%p aux_readable=%s aux_hash=%016llX mutation=observe_only",
                   sample,
                   call,
                   first ? "yes" : "no",
                   actorChange ? "yes" : "no",
                   ownershipChange ? "yes" : "no",
                   heartbeat ? "yes" : "no",
                   context,
                   reinterpret_cast<void*>(contextData),
                   contextOwner,
                   contextOwnerMatch,
                   eventIndex,
                   view.sceneHandle,
                   view.authored,
                   static_cast<unsigned int>(authoredSelector),
                   authoredReference,
                   view.runtime,
                   actor1Before,
                   actor2Before,
                   actor3Before,
                   actor1,
                   actor2,
                   actor3,
                   createdObject,
                   createdSlot,
                   createdCurrentHandle,
                   createdRecord,
                   createdFlags,
                   createdDefinition,
                   createdOwner,
                   createdOwnerMatch,
                   createdEvent,
                   static_cast<unsigned int>(createdSelector),
                   ownerRecord,
                   ownerDefinition,
                   ownerParent20,
                   ownerParent68,
                   transform.called ? "yes" : "no",
                   transform.selector,
                   transform.mode,
                   transform.seedBefore,
                   transform.seedAfter,
                   transform.transformAddress,
                   transform.transformReadable ? "yes" : "no",
                   static_cast<unsigned long long>(
                       transform.transformReadable
                           ? hash_region(transform.transform.data(), transform.transform.size())
                           : 0U),
                   transform.auxiliaryAddress,
                   transform.auxiliaryReadable ? "yes" : "no",
                   static_cast<unsigned long long>(
                       transform.auxiliaryReadable
                           ? hash_region(transform.auxiliary.data(), transform.auxiliary.size())
                           : 0U));
            report("ev=omega_scene_type23_trace stage=attachment n=%u call=%u "
                   "resolver_stage=%s authored_slot=%p runtime_slot=%p "
                   "context_object=%08X context_object_match=%s context_object_record=%p "
                   "context_object_current=%08X context_object_definition=%08X "
                   "context_object_owner20=%08X context_object_owner68=%08X "
                   "mutation=observe_only",
                   sample,
                   call,
                   view.stage,
                   reinterpret_cast<void*>(view.eventSlot),
                   reinterpret_cast<void*>(view.runtimeSlot),
                   contextObject,
                   contextObjectMatch,
                   contextObjectRecord,
                   contextObjectCurrent,
                   contextObjectDefinition,
                   contextObjectOwner20,
                   contextObjectOwner68);
            report("ev=omega_scene_type23_trace stage=owner_candidates n=%u call=%u "
                   "created=%08X scene_record=%p created_owner=%08X "
                   "normal_owner_record=%p normal_current=%08X normal_valid=%s "
                   "normal_definition=%08X normal_parent20=%08X normal_parent68=%08X "
                   "scene_owner_record=%p scene_current=%08X scene_valid=%s "
                   "scene_definition=%08X scene_parent20=%08X scene_parent68=%08X "
                   "match=%s mutation=observe_only",
                   sample,
                   call,
                   createdObject,
                   createdRecord,
                   createdOwner,
                   ownerRecord,
                   ownerCurrentHandle,
                   ownerRecordValid ? "yes" : "no",
                   ownerDefinition,
                   ownerParent20,
                   ownerParent68,
                   ownerSceneRecord,
                   ownerSceneCurrentHandle,
                   ownerSceneRecordValid ? "yes" : "no",
                   ownerSceneDefinition,
                   ownerSceneParent20,
                   ownerSceneParent68,
                   createdOwnerMatch);
            report("ev=omega_scene_transform_source stage=selection n=%u call=%u "
                   "scene=%08X context=%p context_object=%08X scene_record=%p "
                   "object_argument=%p context_offset=0x%llX bank=%p bank_readable=%s "
                   "program=%p program_readable=%s selector=%u output_capacity=%u "
                   "seed=%08X selector_relative=%lld selector_row=%p row_readable=%s "
                   "source_start=%u source_count=%u descriptor_start=%u "
                   "descriptor_count=%u flags=%02X route=%s descriptor_relative=%lld "
                   "descriptor=%p descriptor_readable=%s object_argument_consumed=no "
                   "mutation=observe_only",
                   sample,
                   call,
                   transform.sceneHandle,
                   transform.contextAddress,
                   transform.contextObject,
                   transform.sceneRecordAddress,
                   transform.objectArgumentAddress,
                   static_cast<unsigned long long>(transform.contextOffset),
                   transform.bankAddress,
                   transform.bankReadable ? "yes" : "no",
                   transform.programAddress,
                   transform.programReadable ? "yes" : "no",
                   transform.selector,
                   transform.mode,
                   transform.seedBefore,
                   static_cast<long long>(transform.selectorRelative),
                   transform.selectorRowAddress,
                   transform.selectorRowReadable ? "yes" : "no",
                   static_cast<unsigned int>(transform.selectorSourceStart),
                   static_cast<unsigned int>(transform.selectorSourceCount),
                   static_cast<unsigned int>(transform.selectorDescriptorStart),
                   static_cast<unsigned int>(transform.selectorDescriptorCount),
                   static_cast<unsigned int>(transform.selectorFlags),
                   transform.selectorUsesDirectBank ? "direct_bank" : "descriptor_program",
                   static_cast<long long>(transform.descriptorRelative),
                   transform.descriptorAddress,
                   transform.descriptorReadable ? "yes" : "no");

            const bool omegaPortalTransform =
                transform.sceneHandle == kOmegaPortalSceneHandle
                && transform.selector == 2U;
            const bool omegaIkoraVisualTransform =
                transform.sceneHandle == kOmegaIkoraOpeningSceneHandle
                && transform.selector == 1U;
            if ((omegaPortalTransform || omegaIkoraVisualTransform)
                && transform.selectorUsesDirectBank) {
                const IkoraActor actorState1 = ikora_factory_actor(1U);
                const IkoraActor actorState2 = ikora_factory_actor(2U);
                const IkoraActor actorState3 = ikora_factory_actor(3U);
                const DecodedObjectPosition actorPosition1 =
                    decode_object_position(actorState1.object);
                const DecodedObjectPosition actorPosition2 =
                    decode_object_position(actorState2.object);
                const DecodedObjectPosition actorPosition3 =
                    decode_object_position(actorState3.object);
                const bool selectedInRange =
                    transform.selectorSourceStart < kMaximumCapturedSceneTransforms
                    && transform.selectorSourceStart < transform.bankTransformCount;
                const bool selectedMatchesOutput =
                    selectedInRange
                    && transform.bankTransformsReadable
                    && transform.transformReadable
                    && std::memcmp(
                           transform.bankTransforms.data() + 0x10U
                               + static_cast<std::size_t>(transform.selectorSourceStart) * 0x20U,
                           transform.transform.data(),
                           0x20U)
                           == 0;
                report("ev=omega_scene_transform_bank_map stage=layout n=%u call=%u "
                       "scene=%08X context=%p bank=%p transform_count=%llu "
                       "transform_relative=%lld transform_base=%p transform_readable=%s "
                       "metadata_count=%llu metadata_relative=%lld metadata_base=%p "
                       "metadata_readable=%s secondary_count=%llu secondary_relative=%lld "
                       "secondary_base=%p secondary_readable=%s selected=%u "
                       "selected_matches_output=%s mutation=observe_only",
                       sample,
                       call,
                       transform.sceneHandle,
                       transform.contextAddress,
                       transform.bankAddress,
                       static_cast<unsigned long long>(transform.bankTransformCount),
                       static_cast<long long>(transform.bankTransformRelative),
                       transform.bankTransformsAddress,
                       transform.bankTransformsReadable ? "yes" : "no",
                       static_cast<unsigned long long>(transform.bankMetadataCount),
                       static_cast<long long>(transform.bankMetadataRelative),
                       transform.bankMetadataAddress,
                       transform.bankMetadataReadable ? "yes" : "no",
                       static_cast<unsigned long long>(transform.bankSecondaryCount),
                       static_cast<long long>(transform.bankSecondaryRelative),
                       transform.bankSecondaryAddress,
                       transform.bankSecondaryReadable ? "yes" : "no",
                       static_cast<unsigned int>(transform.selectorSourceStart),
                       selectedMatchesOutput ? "yes" : "no");
                report("ev=omega_scene_transform_bank_map stage=actors n=%u call=%u "
                       "actor1=%08X current1=%08X released1=%u behavior1=%p valid1=%s "
                       "pos1=%.3f,%.3f,%.3f actor2=%08X current2=%08X released2=%u "
                       "behavior2=%p valid2=%s pos2=%.3f,%.3f,%.3f actor3=%08X "
                       "current3=%08X released3=%u behavior3=%p valid3=%s "
                       "pos3=%.3f,%.3f,%.3f mutation=observe_only",
                       sample,
                       call,
                       actorState1.object,
                       actorPosition1.currentHandle,
                       actorState1.released ? 1U : 0U,
                       reinterpret_cast<void*>(actorState1.behaviorActor),
                       actorPosition1.valid ? "yes" : "no",
                       actorPosition1.value[0],
                       actorPosition1.value[1],
                       actorPosition1.value[2],
                       actorState2.object,
                       actorPosition2.currentHandle,
                       actorState2.released ? 1U : 0U,
                       reinterpret_cast<void*>(actorState2.behaviorActor),
                       actorPosition2.valid ? "yes" : "no",
                       actorPosition2.value[0],
                       actorPosition2.value[1],
                       actorPosition2.value[2],
                       actorState3.object,
                       actorPosition3.currentHandle,
                       actorState3.released ? 1U : 0U,
                       reinterpret_cast<void*>(actorState3.behaviorActor),
                       actorPosition3.valid ? "yes" : "no",
                       actorPosition3.value[0],
                       actorPosition3.value[1],
                       actorPosition3.value[2]);
                const std::size_t capturedTransformCount = static_cast<std::size_t>(
                    std::min<std::uint64_t>(transform.bankTransformCount,
                                            kMaximumCapturedSceneTransforms));
                for (std::size_t bankIndex = 0U;
                     bankIndex < capturedTransformCount;
                     ++bankIndex) {
                    const DecodedSceneTransform entry =
                        decode_scene_bank_transform(transform, bankIndex);
                    std::uint16_t metadata = 0U;
                    if (transform.bankMetadataReadable) {
                        std::memcpy(&metadata,
                                    transform.bankMetadata.data()
                                        + bankIndex * sizeof metadata,
                                    sizeof metadata);
                    }
                    const bool entryMatchesOutput =
                        entry.readable
                        && transform.transformReadable
                        && std::memcmp(transform.bankTransforms.data() + 0x10U
                                           + bankIndex * 0x20U,
                                       transform.transform.data(),
                                       0x20U)
                               == 0;
                    report("ev=omega_scene_transform_bank_map stage=entry n=%u call=%u "
                           "index=%u selected=%s readable=%s finite=%s metadata=%04X "
                           "rotation=%.6f,%.6f,%.6f,%.6f position=%.3f,%.3f,%.3f "
                           "scale=%.6f output_match=%s distance_actor1=%.3f "
                           "distance_actor2=%.3f distance_actor3=%.3f "
                           "mutation=observe_only",
                           sample,
                           call,
                           static_cast<unsigned int>(bankIndex),
                           bankIndex == transform.selectorSourceStart ? "yes" : "no",
                           entry.readable ? "yes" : "no",
                           entry.finite ? "yes" : "no",
                           static_cast<unsigned int>(metadata),
                           entry.rotation[0],
                           entry.rotation[1],
                           entry.rotation[2],
                           entry.rotation[3],
                           entry.position[0],
                           entry.position[1],
                           entry.position[2],
                           entry.scale,
                           entryMatchesOutput ? "yes" : "no",
                           position_distance(entry, actorPosition1),
                           position_distance(entry, actorPosition2),
                           position_distance(entry, actorPosition3));
                }

                if (omegaIkoraVisualTransform) {
                    std::array<SceneTransformComponentMatch, 32U> matches{};
                    const std::size_t matchCount =
                        find_scene_transform_component_matches(transform, matches);
                    report("ev=omega_scene_transform_provenance stage=summary n=%u call=%u "
                           "scene=%08X selector=%u reference=%08X matches=%zu "
                           "mutation=observe_only",
                           sample,
                           call,
                           transform.sceneHandle,
                           transform.selector,
                           authoredReference,
                           matchCount);
                    for (std::size_t matchIndex = 0U;
                         matchIndex < matchCount;
                         ++matchIndex) {
                        const SceneTransformComponentMatch& match = matches[matchIndex];
                        report("ev=omega_scene_transform_provenance stage=component "
                               "n=%u call=%u match=%u factory_n=%u object=%08X "
                               "definition=%08X component=%p size=0x%zX "
                               "full_offset=%lld position_offset=%lld "
                               "mutation=observe_only",
                               sample,
                               call,
                               static_cast<unsigned int>(matchIndex),
                               match.factorySequence,
                               match.object,
                               match.definition,
                               reinterpret_cast<void*>(match.instance),
                               match.componentSize,
                               match.fullOffset == static_cast<std::size_t>(-1)
                                   ? -1LL
                                   : static_cast<long long>(match.fullOffset),
                               match.positionOffset == static_cast<std::size_t>(-1)
                                   ? -1LL
                                   : static_cast<long long>(match.positionOffset));
                    }
                }
            }

            std::array<std::byte, 0x80U> contextSnapshot{};
            std::array<std::byte, 0x100U> contextDataSnapshot{};
            std::array<std::byte, 0x80U> authoredSnapshot{};
            std::array<std::byte, 0x60U> runtimeSnapshot{};
            std::array<std::byte, 0x80U> createdSnapshot{};
            std::array<std::byte, 0x80U> ownerSnapshot{};
            std::array<std::byte, 0x80U> ownerSceneSnapshot{};
            std::array<std::byte, 0x80U> contextObjectSnapshot{};
            const bool contextReadable =
                safe_copy(contextSnapshot.data(), context, contextSnapshot.size());
            const bool contextDataReadable = contextData != 0U
                                             && safe_copy(contextDataSnapshot.data(),
                                                          reinterpret_cast<const void*>(contextData),
                                                          contextDataSnapshot.size());
            const bool authoredReadable = view.authored != nullptr
                                          && safe_copy(authoredSnapshot.data(),
                                                       view.authored,
                                                       authoredSnapshot.size());
            const bool runtimeReadable = view.runtime != nullptr
                                         && safe_copy(runtimeSnapshot.data(),
                                                      view.runtime,
                                                      runtimeSnapshot.size());
            const bool createdReadable = createdRecord != nullptr
                                         && safe_copy(createdSnapshot.data(),
                                                      createdRecord,
                                                      createdSnapshot.size());
            const bool ownerReadable = ownerRecord != nullptr
                                       && safe_copy(ownerSnapshot.data(),
                                                    ownerRecord,
                                                    ownerSnapshot.size());
            const bool ownerSceneReadable = ownerSceneRecord != nullptr
                                            && safe_copy(ownerSceneSnapshot.data(),
                                                         ownerSceneRecord,
                                                         ownerSceneSnapshot.size());
            const bool contextObjectReadable = contextObjectRecord != nullptr
                                               && safe_copy(contextObjectSnapshot.data(),
                                                            contextObjectRecord,
                                                            contextObjectSnapshot.size());
            report_scene_type23_words(
                sample, "context", context, contextSnapshot, contextReadable);
            report_scene_type23_words(sample,
                                      "context_data",
                                      reinterpret_cast<const void*>(contextData),
                                      contextDataSnapshot,
                                      contextDataReadable);
            report_scene_type23_words(
                sample, "authored", view.authored, authoredSnapshot, authoredReadable);
            report_scene_type23_words(
                sample, "runtime", view.runtime, runtimeSnapshot, runtimeReadable);
            report_scene_type23_words(sample,
                                      "created_object",
                                      createdRecord,
                                      createdSnapshot,
                                      createdReadable);
            report_scene_type23_words(sample,
                                      "created_owner",
                                      ownerRecord,
                                      ownerSnapshot,
                                      ownerReadable);
            report_scene_type23_words(sample,
                                      "created_owner_scene",
                                      ownerSceneRecord,
                                      ownerSceneSnapshot,
                                      ownerSceneReadable);
            report_scene_type23_words(sample,
                                      "context_object",
                                      contextObjectRecord,
                                      contextObjectSnapshot,
                                      contextObjectReadable);
            report_scene_type23_words(sample,
                                      "transform_output",
                                      transform.transformAddress,
                                      transform.transform,
                                      transform.transformReadable);
            report_scene_type23_words(sample,
                                      "transform_auxiliary",
                                      transform.auxiliaryAddress,
                                      transform.auxiliary,
                                      transform.auxiliaryReadable);
            report_scene_type23_words(sample,
                                      "transform_bank",
                                      transform.bankAddress,
                                      transform.bank,
                                      transform.bankReadable);
            report_scene_type23_words(sample,
                                      "transform_program",
                                      transform.programAddress,
                                      transform.program,
                                      transform.programReadable);
            report_scene_type23_words(sample,
                                      "transform_selector_row",
                                      transform.selectorRowAddress,
                                      transform.selectorRow,
                                      transform.selectorRowReadable);
            report_scene_type23_words(sample,
                                      "transform_descriptors",
                                      transform.descriptorAddress,
                                      transform.descriptors,
                                      transform.descriptorReadable);
            report_scene_type23_words(sample,
                                      "transform_bank_entries",
                                      transform.bankTransformsAddress,
                                      transform.bankTransforms,
                                      transform.bankTransformsReadable);
            report_scene_type23_words(sample,
                                      "transform_bank_metadata",
                                      transform.bankMetadataAddress,
                                      transform.bankMetadata,
                                      transform.bankMetadataReadable);
            report_scene_type23_words(sample,
                                      "transform_bank_secondary",
                                      transform.bankSecondaryAddress,
                                      transform.bankSecondary,
                                      transform.bankSecondaryReadable);
        } else if (sample == kMaximumSceneType23Samples + 1U) {
            report("ev=omega_scene_type23_trace stage=sample_limit n=%u limit=%u "
                   "calls=%u mutation=observe_only",
                   sample,
                   kMaximumSceneType23Samples,
                   call);
        }
    }
    --g_sceneType23Depth;
}

/**
 * Records the exact authored-event row selected by the native scene timeline.
 *
 * Type 23's native handler creates the short-lived visual object, writes the callback context's
 * owner handle at created-record +0x20, writes the event index at +0x24, and copies the authored
 * selector to +0x16. Following those fields after dispatch is the first direct measurement of
 * which Ikora/root owns the purple beam. Change-driven sampling keeps the recorder alive through
 * the complete scene instead of exhausting its budget during the first few seconds.
 */
__declspec(noinline) std::uint64_t __fastcall scene_authored_event_dispatch(
    std::uint32_t* scene,
    std::int32_t eventIndex,
    float elapsed,
    float* consumed) noexcept {
    const SceneAuthoredEventDispatch original =
        g_sceneAuthoredEventDispatchOriginal.load(std::memory_order_acquire);
    if (original == nullptr) {
        return 0U;
    }
    const std::uint32_t animatedObject =
        g_sceneAnimatedObject.load(std::memory_order_acquire);
    // Actor 2 is armed only after the native cinematic performer has been identified. Starting
    // here avoids consuming the recorder on the thousands of unrelated load-time scene events.
    const bool inspect = g_sceneAuthoredEventDepth == 0U && omega_forced()
                         && animatedObject != kInvalidHandle;
    if (!inspect) {
        return original(scene, eventIndex, elapsed, consumed);
    }

    ++g_sceneAuthoredEventDepth;
    const std::uint32_t attempt =
        g_sceneAuthoredEventAttempts.fetch_add(1U, std::memory_order_relaxed) + 1U;
    const SceneAuthoredEventView view = resolve_scene_authored_event(scene, eventIndex);
    if (view.authored == nullptr || view.type >= 72U) {
        if (attempt <= kMaximumSceneEventResolveFailureLogs) {
            report("ev=omega_scene_event_trace stage=resolve_failure attempt=%u "
                   "failure_at=%s scene=%p animated=%08X scene_handle=%08X event=%d "
                   "elapsed=%.6f page=%llu pool_holder=%p pool=%p descriptor=%p "
                   "stride=%d mask=%d records=%p record=%p record_mask=%016llX "
                   "scene_data=%p adjustment=%p base=%p event_vector_relative=%lld "
                   "event_vector=%p event_slot=%p event_relative=%lld authored=%p "
                   "type=%u runtime_anchor=%p runtime_vector_relative=%lld "
                   "runtime_vector=%p runtime_slot=%p runtime_relative=%lld runtime=%p "
                   "mutation=observe_only",
                   attempt,
                   view.stage,
                   scene,
                   animatedObject,
                   view.sceneHandle,
                   eventIndex,
                   static_cast<double>(elapsed),
                   static_cast<unsigned long long>(view.page),
                   reinterpret_cast<void*>(view.poolHolder),
                   reinterpret_cast<void*>(view.pool),
                   reinterpret_cast<void*>(view.descriptor),
                   view.stride,
                   view.maskValue,
                   reinterpret_cast<void*>(view.records),
                   reinterpret_cast<void*>(view.record),
                   static_cast<unsigned long long>(view.recordMask),
                   reinterpret_cast<void*>(view.sceneData),
                   reinterpret_cast<void*>(view.adjustment),
                   reinterpret_cast<void*>(view.base),
                   static_cast<long long>(view.eventVectorRelative),
                   reinterpret_cast<void*>(view.eventVector),
                   reinterpret_cast<void*>(view.eventSlot),
                   static_cast<long long>(view.eventRelative),
                   view.authored,
                   static_cast<unsigned int>(view.type),
                   reinterpret_cast<void*>(view.runtimeAnchor),
                   static_cast<long long>(view.runtimeVectorRelative),
                   reinterpret_cast<void*>(view.runtimeVector),
                   reinterpret_cast<void*>(view.runtimeSlot),
                   static_cast<long long>(view.runtimeRelative),
                   view.runtime);
        } else if (attempt == kMaximumSceneEventResolveFailureLogs + 1U) {
            report("ev=omega_scene_event_trace stage=resolve_failure_limit attempt=%u "
                   "limit=%u mutation=observe_only",
                   attempt,
                   kMaximumSceneEventResolveFailureLogs);
        }
        const std::uint64_t result = original(scene, eventIndex, elapsed, consumed);
        --g_sceneAuthoredEventDepth;
        return result;
    }

    const std::uint32_t call =
        g_sceneAuthoredEventCalls.fetch_add(1U, std::memory_order_relaxed) + 1U;
    std::array<std::byte, 0x80U> authoredSnapshot{};
    std::array<std::byte, 0x60U> runtimeBefore{};
    const bool authoredReadable = view.authored != nullptr
                                  && safe_copy(authoredSnapshot.data(),
                                               view.authored,
                                               authoredSnapshot.size());
    const bool runtimeBeforeReadable = view.runtime != nullptr
                                       && safe_copy(runtimeBefore.data(),
                                                    view.runtime,
                                                    runtimeBefore.size());
    const std::size_t activationIndex = omega_scene_activation_index(view.sceneHandle);
    if (activationIndex < g_omegaSceneEventPointers.size()) {
        g_omegaSceneEventPointers[activationIndex].store(
            reinterpret_cast<std::uintptr_t>(scene),
            std::memory_order_release);
    }
    std::uint32_t* const previousEventScene = g_sceneAuthoredEventScene;
    const std::int32_t previousEventIndex = g_sceneAuthoredEventIndex;
    const std::uint32_t previousEventHandle = g_sceneAuthoredEventHandle;
    const std::byte* const previousEventRuntime = g_sceneAuthoredEventRuntime;
    const std::byte* const previousEventAuthored = g_sceneAuthoredEventAuthored;
    g_sceneAuthoredEventScene = scene;
    g_sceneAuthoredEventIndex = eventIndex;
    g_sceneAuthoredEventHandle = view.sceneHandle;
    g_sceneAuthoredEventRuntime = view.runtime;
    g_sceneAuthoredEventAuthored = view.authored;
    const float consumedBefore = safe_read<float>(consumed, -1.0F);
    const std::uint64_t result = original(scene, eventIndex, elapsed, consumed);
    g_sceneAuthoredEventScene = previousEventScene;
    g_sceneAuthoredEventIndex = previousEventIndex;
    g_sceneAuthoredEventHandle = previousEventHandle;
    g_sceneAuthoredEventRuntime = previousEventRuntime;
    g_sceneAuthoredEventAuthored = previousEventAuthored;

    std::array<std::byte, 0x60U> runtimeAfter{};
    const bool runtimeAfterReadable = view.runtime != nullptr
                                      && safe_copy(runtimeAfter.data(),
                                                   view.runtime,
                                                   runtimeAfter.size());
    const std::uint32_t actor1 = ikora_factory_object(1U);
    const std::uint32_t actor2 = ikora_factory_object(2U);
    const std::uint32_t actor3 = ikora_factory_object(3U);

    // Only type 23 owns this runtime field. Treating +0x40 as a handle for other event classes
    // would turn unrelated timing/state bytes into bogus object-table lookups.
    const std::uint32_t createdObject =
        view.type == 23U && view.runtime != nullptr
            ? safe_read<std::uint32_t>(view.runtime + 0x40U, kInvalidHandle)
            : kInvalidHandle;
    const std::uint32_t createdSlot =
        createdObject == kInvalidHandle ? kInvalidHandle : createdObject & 0x1FFFU;
    const std::byte* const createdRecord = object_record(createdObject);
    const std::uint32_t createdOwner =
        safe_read<std::uint32_t>(createdRecord == nullptr ? nullptr : createdRecord + 0x20U,
                                 kInvalidHandle);
    const std::uint32_t createdEvent =
        safe_read<std::uint32_t>(createdRecord == nullptr ? nullptr : createdRecord + 0x24U,
                                 kInvalidHandle);
    const std::uint8_t createdSelector =
        safe_read<std::uint8_t>(createdRecord == nullptr ? nullptr : createdRecord + 0x16U,
                                0xFFU);
    const std::uint32_t createdDefinition =
        safe_read<std::uint32_t>(createdRecord == nullptr ? nullptr : createdRecord + 0x4CU, 0U);
    const std::uint32_t createdFlags =
        safe_read<std::uint32_t>(createdRecord == nullptr ? nullptr : createdRecord + 0x04U, 0U);

    const std::byte* const ownerRecord = object_record(createdOwner);
    const std::uint32_t ownerDefinition =
        safe_read<std::uint32_t>(ownerRecord == nullptr ? nullptr : ownerRecord + 0x4CU, 0U);
    const std::uint32_t ownerParent20 =
        safe_read<std::uint32_t>(ownerRecord == nullptr ? nullptr : ownerRecord + 0x20U,
                                 kInvalidHandle);
    const std::uint32_t ownerParent68 =
        safe_read<std::uint32_t>(ownerRecord == nullptr ? nullptr : ownerRecord + 0x68U,
                                 kInvalidHandle);

    const std::uint64_t now = GetTickCount64();
    bool first = false;
    bool actorChange = false;
    bool visualChange = false;
    bool heartbeat = false;
    bool reportCall = false;
    AcquireSRWLockExclusive(&g_sceneEventTraceStateLock);
    SceneEventTraceState* state = nullptr;
    for (std::size_t index = 0U; index < g_sceneEventTraceStateCount; ++index) {
        SceneEventTraceState& candidate = g_sceneEventTraceStates[index];
        if (candidate.valid && candidate.scene == reinterpret_cast<std::uintptr_t>(scene)
            && candidate.authored == reinterpret_cast<std::uintptr_t>(view.authored)
            && candidate.eventIndex == eventIndex) {
            state = &candidate;
            break;
        }
    }
    if (state == nullptr && g_sceneEventTraceStateCount < g_sceneEventTraceStates.size()) {
        state = &g_sceneEventTraceStates[g_sceneEventTraceStateCount++];
        *state = {};
        state->scene = reinterpret_cast<std::uintptr_t>(scene);
        state->authored = reinterpret_cast<std::uintptr_t>(view.authored);
        state->eventIndex = eventIndex;
        state->type = view.type;
        state->createdSlot = kInvalidHandle;
        state->createdOwner = kInvalidHandle;
        first = true;
    }
    if (state != nullptr) {
        actorChange = state->valid
                      && (state->actor1 != actor1 || state->actor2 != actor2
                          || state->actor3 != actor3);
        visualChange = state->valid && view.type == 23U
                       && (state->createdSlot != createdSlot
                           || state->createdOwner != createdOwner
                           || state->createdDefinition != createdDefinition);
        heartbeat = state->valid && now - state->lastLoggedAtMs >= kSceneEventHeartbeatMs;
        reportCall = first || actorChange || visualChange || heartbeat;
        state->valid = true;
        state->type = view.type;
        state->actor1 = actor1;
        state->actor2 = actor2;
        state->actor3 = actor3;
        state->createdSlot = createdSlot;
        state->createdOwner = createdOwner;
        state->createdDefinition = createdDefinition;
        ++state->calls;
        if (reportCall) {
            state->lastLoggedAtMs = now;
        }
    }
    ReleaseSRWLockExclusive(&g_sceneEventTraceStateLock);

    if (reportCall) {
        const std::uint32_t sample =
            g_sceneAuthoredEventSamples.fetch_add(1U, std::memory_order_relaxed) + 1U;
        if (sample <= kMaximumSceneAuthoredEventLogs) {
            const char* ownerMatch = "other";
            if (createdOwner == kInvalidHandle) {
                ownerMatch = "none";
            } else if (createdOwner == actor1) {
                ownerMatch = "actor1";
            } else if (createdOwner == actor2) {
                ownerMatch = "actor2";
            } else if (createdOwner == actor3) {
                ownerMatch = "actor3";
            } else if (ownerParent20 == actor1 || ownerParent68 == actor1) {
                ownerMatch = "via_actor1";
            } else if (ownerParent20 == actor2 || ownerParent68 == actor2) {
                ownerMatch = "via_actor2";
            } else if (ownerParent20 == actor3 || ownerParent68 == actor3) {
                ownerMatch = "via_actor3";
            }

            const std::uint8_t authoredSelector =
                safe_read<std::uint8_t>(view.authored + 0x40U, 0xFFU);
            const std::uint32_t authoredReference =
                safe_read<std::uint32_t>(view.authored + 0x44U, 0U);
            const std::uint64_t authoredHash = authoredReadable
                                                   ? hash_region(authoredSnapshot.data(),
                                                                 authoredSnapshot.size())
                                                   : 0U;
            const std::uint64_t runtimeBeforeHash =
                runtimeBeforeReadable
                    ? hash_region(runtimeBefore.data(), runtimeBefore.size())
                    : 0U;
            const std::uint64_t runtimeAfterHash =
                runtimeAfterReadable
                    ? hash_region(runtimeAfter.data(), runtimeAfter.size())
                    : 0U;
            report("ev=omega_scene_event_trace stage=sample n=%u call=%u "
                   "first=%s actor_change=%s visual_change=%s heartbeat=%s scene=%p "
                   "scene_handle=%08X event=%d type=%u callback_rva=+%llX "
                   "elapsed=%.6f consumed_before=%.6f consumed_after=%.6f result=%llu "
                   "actor1=%08X actor2=%08X actor3=%08X animated=%08X latest=%08X "
                   "authored=%p authored_selector=%u authored_reference=%08X "
                   "authored_hash=%016llX runtime=%p runtime_before_hash=%016llX "
                   "runtime_after_hash=%016llX created=%08X created_slot=%u "
                   "created_record=%p created_flags=%08X created_definition=%08X "
                   "created_owner=%08X created_event=%08X created_selector=%u "
                   "owner_match=%s owner_record=%p owner_definition=%08X "
                   "owner_parent20=%08X owner_parent68=%08X mutation=observe_only",
                   sample,
                   call,
                   first ? "yes" : "no",
                   actorChange ? "yes" : "no",
                   visualChange ? "yes" : "no",
                   heartbeat ? "yes" : "no",
                   scene,
                   view.sceneHandle,
                   eventIndex,
                   static_cast<unsigned int>(view.type),
                   static_cast<unsigned long long>(image_rva(view.callback)),
                   static_cast<double>(elapsed),
                   static_cast<double>(consumedBefore),
                   static_cast<double>(safe_read<float>(consumed, -1.0F)),
                   static_cast<unsigned long long>(result),
                   actor1,
                   actor2,
                   actor3,
                   g_sceneAnimatedObject.load(std::memory_order_acquire),
                   g_ikoraObject.load(std::memory_order_acquire),
                   view.authored,
                   static_cast<unsigned int>(authoredSelector),
                   authoredReference,
                   static_cast<unsigned long long>(authoredHash),
                   view.runtime,
                   static_cast<unsigned long long>(runtimeBeforeHash),
                   static_cast<unsigned long long>(runtimeAfterHash),
                   createdObject,
                   createdSlot,
                   createdRecord,
                   createdFlags,
                   createdDefinition,
                   createdOwner,
                   createdEvent,
                   static_cast<unsigned int>(createdSelector),
                   ownerMatch,
                   ownerRecord,
                   ownerDefinition,
                   ownerParent20,
                   ownerParent68);
            report_scene_event_words(sample,
                                     "authored",
                                     view.authored,
                                     authoredSnapshot,
                                     authoredReadable);
            report_scene_event_words(sample,
                                     "runtime_before",
                                     view.runtime,
                                     runtimeBefore,
                                     runtimeBeforeReadable);
            report_scene_event_words(sample,
                                     "runtime_after",
                                     view.runtime,
                                     runtimeAfter,
                                     runtimeAfterReadable);
            if (view.type == 23U) {
                std::array<std::byte, 0x80U> createdSnapshot{};
                std::array<std::byte, 0x80U> ownerSnapshot{};
                const bool createdReadable = createdRecord != nullptr
                                             && safe_copy(createdSnapshot.data(),
                                                          createdRecord,
                                                          createdSnapshot.size());
                const bool ownerReadable = ownerRecord != nullptr
                                           && safe_copy(ownerSnapshot.data(),
                                                        ownerRecord,
                                                        ownerSnapshot.size());
                report_scene_event_words(sample,
                                         "created_object",
                                         createdRecord,
                                         createdSnapshot,
                                         createdReadable);
                report_scene_event_words(sample,
                                         "created_owner",
                                         ownerRecord,
                                         ownerSnapshot,
                                         ownerReadable);
            }
        } else if (sample == kMaximumSceneAuthoredEventLogs + 1U) {
            report("ev=omega_scene_event_trace stage=sample_limit n=%u limit=%u "
                   "calls=%u mutation=observe_only",
                   sample,
                   kMaximumSceneAuthoredEventLogs,
                   call);
        }
    }
    --g_sceneAuthoredEventDepth;
    return result;
}

/**
 * Records one complete 0x8080992F visual-entry creation. The nested hooks below use this
 * thread-local context to join the tagged transform source, finalized +575690 descriptor,
 * created object, attachment record, and committed world position without changing any state.
 */
__declspec(noinline) std::uint16_t* __fastcall visual_entry_create(
    std::uint32_t* component,
    std::uint16_t* result,
    std::uint32_t* entryState,
    std::int32_t entryIndex) noexcept {
    const VisualEntryCreate original =
        g_visualEntryCreateOriginal.load(std::memory_order_acquire);
    if (original == nullptr) {
        return result;
    }

    const bool inspect = g_visualEntryRecorderDepth == 0U && omega_forced();
    if (!inspect) {
        return original(component, result, entryState, entryIndex);
    }

    ++g_visualEntryRecorderDepth;
    VisualEntryRecorderContext& context = g_visualEntryRecorderContext;
    context = {};
    context.sequence =
        g_visualEntryRecorderCalls.fetch_add(1U, std::memory_order_relaxed) + 1U;
    context.component = component;
    context.entryState = entryState;
    context.entryIndex = entryIndex;
    context.callerRva = image_rva(
        reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
    context.report = context.sequence <= kMaximumVisualEntryRecorderLogs;

    const IkoraComponentAddressOwner componentOwner =
        locate_ikora_component_address(component);
    const std::uint32_t componentWord0 = safe_read<std::uint32_t>(
        component, kInvalidHandle);
    const std::uint32_t stateWord0 = safe_read<std::uint32_t>(
        entryState, kInvalidHandle);
    const std::uint8_t statePresent = safe_read<std::uint8_t>(
        entryState == nullptr
            ? nullptr
            : reinterpret_cast<const std::byte*>(entryState) + 0x09U,
        0U);
    const std::uint8_t stateTransformKind = safe_read<std::uint8_t>(
        entryState == nullptr
            ? nullptr
            : reinterpret_cast<const std::byte*>(entryState) + 0x14U,
        0xFFU);
    const std::int16_t stateTransformIndex = safe_read<std::int16_t>(
        entryState == nullptr
            ? nullptr
            : reinterpret_cast<const std::byte*>(entryState) + 0x16U,
        static_cast<std::int16_t>(-1));
    const float fallbackX = safe_read<float>(
        component == nullptr
            ? nullptr
            : reinterpret_cast<const std::byte*>(component) + 0x1A0U,
        0.0F);
    const float fallbackY = safe_read<float>(
        component == nullptr
            ? nullptr
            : reinterpret_cast<const std::byte*>(component) + 0x1A4U,
        0.0F);
    const float fallbackZ = safe_read<float>(
        component == nullptr
            ? nullptr
            : reinterpret_cast<const std::byte*>(component) + 0x1A8U,
        0.0F);
    const float fallbackScale = safe_read<float>(
        component == nullptr
            ? nullptr
            : reinterpret_cast<const std::byte*>(component) + 0x1ACU,
        0.0F);
    const std::uint64_t targetBefore = safe_read<std::uint64_t>(
        component == nullptr
            ? nullptr
            : reinterpret_cast<const std::byte*>(component) + 0x440U,
        0U);

    if (context.report) {
        report("ev=omega_beam_final_transform stage=entry_begin n=%u caller=+%llX "
               "component=%p component_word0=%08X component_owner_actor=%u "
               "component_owner_object=%08X component_definition=%08X "
               "component_owner_offset=0x%llX component_exact=%u "
               "state=%p state_word0=%08X state_present=%u "
               "state_transform_kind=%u state_transform_index=%d entry_index=%d "
               "fallback_xyzs=%.6f,%.6f,%.6f,%.6f target_before=%016llX "
               "mutation=observe_only",
               context.sequence,
               static_cast<unsigned long long>(context.callerRva),
               component,
               componentWord0,
               componentOwner.factorySequence,
               componentOwner.object,
               componentOwner.definition,
               static_cast<unsigned long long>(componentOwner.offset),
               componentOwner.exactInstance ? 1U : 0U,
               entryState,
               stateWord0,
               static_cast<unsigned int>(statePresent),
               static_cast<unsigned int>(stateTransformKind),
               static_cast<int>(stateTransformIndex),
               entryIndex,
               static_cast<double>(fallbackX),
               static_cast<double>(fallbackY),
               static_cast<double>(fallbackZ),
               static_cast<double>(fallbackScale),
               static_cast<unsigned long long>(targetBefore));
    }

    std::uint16_t* const returned =
        original(component, result, entryState, entryIndex);
    const std::uint32_t createdObject = static_cast<std::uint32_t>(
        safe_read<std::int32_t>(returned, -1));
    if (context.createdObject == kInvalidHandle) {
        context.createdObject = createdObject;
    }
    const DecodedObjectPosition createdPosition =
        decode_object_position(context.createdObject);
    const DecodedSceneTransform createdTransform =
        context.createdTransformReadable
            ? decode_scene_transform_address(context.createdTransform.data())
            : DecodedSceneTransform{};
    const IkoraActor actor1 = ikora_factory_actor(1U);
    const IkoraActor actor2 = ikora_factory_actor(2U);
    const IkoraActor actor3 = ikora_factory_actor(3U);
    const DecodedObjectPosition actorPosition1 = decode_object_position(actor1.object);
    const DecodedObjectPosition actorPosition2 = decode_object_position(actor2.object);
    const DecodedObjectPosition actorPosition3 = decode_object_position(actor3.object);
    const std::uint64_t targetAfter = safe_read<std::uint64_t>(
        component == nullptr
            ? nullptr
            : reinterpret_cast<const std::byte*>(component) + 0x440U,
        0U);

    if (context.report) {
        report("ev=omega_beam_final_transform stage=entry_complete n=%u "
               "component_owner_actor=%u component_definition=%08X entry_index=%d "
               "created=%08X created_definition=%08X object_create_calls=%u "
               "transform_apply_calls=%u attachment_bind_calls=%u "
               "descriptor_transform_readable=%u descriptor_xyzs=%.6f,%.6f,%.6f,%.6f "
               "descriptor_distance_actor1=%.3f descriptor_distance_actor2=%.3f "
               "descriptor_distance_actor3=%.3f world_valid=%u "
               "world_xyz=%.6f,%.6f,%.6f target_after=%016llX mutation=observe_only",
               context.sequence,
               componentOwner.factorySequence,
               componentOwner.definition,
               entryIndex,
               context.createdObject,
               context.createdDefinition,
               context.objectCreateCalls,
               context.transformApplyCalls,
               context.attachmentBindCalls,
               createdTransform.readable ? 1U : 0U,
               static_cast<double>(createdTransform.position[0]),
               static_cast<double>(createdTransform.position[1]),
               static_cast<double>(createdTransform.position[2]),
               static_cast<double>(createdTransform.scale),
               static_cast<double>(position_distance(createdTransform, actorPosition1)),
               static_cast<double>(position_distance(createdTransform, actorPosition2)),
               static_cast<double>(position_distance(createdTransform, actorPosition3)),
               createdPosition.valid ? 1U : 0U,
               static_cast<double>(createdPosition.value[0]),
               static_cast<double>(createdPosition.value[1]),
               static_cast<double>(createdPosition.value[2]),
               static_cast<unsigned long long>(targetAfter));
    } else if (context.sequence == kMaximumVisualEntryRecorderLogs + 1U) {
        report("ev=omega_beam_final_transform stage=sample_limit n=%u limit=%u "
               "mutation=observe_only",
               context.sequence,
               kMaximumVisualEntryRecorderLogs);
    }

    context = {};
    --g_visualEntryRecorderDepth;
    return returned;
}

/** Captures the exact 0x20-byte transform produced for the visual-entry descriptor. */
__declspec(noinline) std::uint64_t __fastcall visual_transform_apply(
    const std::byte* source,
    std::byte* destination) noexcept {
    const VisualTransformApply original =
        g_visualTransformApplyOriginal.load(std::memory_order_acquire);
    if (original == nullptr) {
        return 0U;
    }

    VisualEntryRecorderContext& context = g_visualEntryRecorderContext;
    const std::uintptr_t caller = image_rva(
        reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
    const bool inspect = g_visualEntryRecorderDepth != 0U && context.report
                         && caller == kVisualEntryTransformApplyReturnRva;
    if (!inspect) {
        return original(source, destination);
    }

    ++context.transformApplyCalls;
    std::array<std::byte, 0x20U> sourceSnapshot{};
    const bool sourceReadable = safe_copy(
        sourceSnapshot.data(), source, sourceSnapshot.size());
    const DecodedSceneTransform before =
        decode_scene_transform_address(destination);
    const std::uint64_t returned = original(source, destination);
    const DecodedSceneTransform after =
        decode_scene_transform_address(destination);
    const IkoraActor actor1 = ikora_factory_actor(1U);
    const IkoraActor actor2 = ikora_factory_actor(2U);
    const IkoraActor actor3 = ikora_factory_actor(3U);

    const auto qword = [&sourceSnapshot](std::size_t offset) noexcept {
        std::uint64_t value = 0U;
        if (offset + sizeof value <= sourceSnapshot.size()) {
            std::memcpy(&value, sourceSnapshot.data() + offset, sizeof value);
        }
        return value;
    };
    report("ev=omega_beam_final_transform stage=transform_apply n=%u caller=+%llX "
           "source=%p source_readable=%u source_qwords=%016llX,%016llX,%016llX,%016llX "
           "destination=%p before_xyzs=%.6f,%.6f,%.6f,%.6f "
           "after_readable=%u after_xyzs=%.6f,%.6f,%.6f,%.6f "
           "distance_actor1=%.3f distance_actor2=%.3f distance_actor3=%.3f "
           "mutation=observe_only",
           context.sequence,
           static_cast<unsigned long long>(caller),
           source,
           sourceReadable ? 1U : 0U,
           static_cast<unsigned long long>(qword(0x00U)),
           static_cast<unsigned long long>(qword(0x08U)),
           static_cast<unsigned long long>(qword(0x10U)),
           static_cast<unsigned long long>(qword(0x18U)),
           destination,
           static_cast<double>(before.position[0]),
           static_cast<double>(before.position[1]),
           static_cast<double>(before.position[2]),
           static_cast<double>(before.scale),
           after.readable ? 1U : 0U,
           static_cast<double>(after.position[0]),
           static_cast<double>(after.position[1]),
           static_cast<double>(after.position[2]),
           static_cast<double>(after.scale),
           static_cast<double>(position_distance(after, decode_object_position(actor1.object))),
           static_cast<double>(position_distance(after, decode_object_position(actor2.object))),
           static_cast<double>(position_distance(after, decode_object_position(actor3.object))));
    return returned;
}

/** Captures the parent/proxy association applied after +575690 creates the visual object. */
__declspec(noinline) void __fastcall visual_attachment_bind(
    std::uintptr_t ownerRecord,
    std::uint32_t object,
    std::uint64_t binding,
    const std::byte* attachment) noexcept {
    const VisualAttachmentBind original =
        g_visualAttachmentBindOriginal.load(std::memory_order_acquire);
    if (original == nullptr) {
        return;
    }

    VisualEntryRecorderContext& context = g_visualEntryRecorderContext;
    const std::uintptr_t caller = image_rva(
        reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
    const bool inspect = g_visualEntryRecorderDepth != 0U && context.report
                         && caller == kVisualEntryAttachmentBindReturnRva;
    if (!inspect) {
        original(ownerRecord, object, binding, attachment);
        return;
    }

    ++context.attachmentBindCalls;
    const IkoraComponentAddressOwner owner = locate_ikora_component_address(
        reinterpret_cast<const void*>(ownerRecord));
    const DecodedObjectPosition before = decode_object_position(object);
    const std::uint16_t attachmentIndex = safe_read<std::uint16_t>(attachment, 0xFFFFU);
    const std::uint8_t attachmentFlags = safe_read<std::uint8_t>(
        attachment == nullptr ? nullptr : attachment + 0x02U, 0xFFU);
    const std::uint64_t owner0 = safe_read<std::uint64_t>(
        reinterpret_cast<const void*>(ownerRecord + 0x00U), 0U);
    const std::uint64_t owner8 = safe_read<std::uint64_t>(
        reinterpret_cast<const void*>(ownerRecord + 0x08U), 0U);

    original(ownerRecord, object, binding, attachment);

    const DecodedObjectPosition after = decode_object_position(object);
    const std::byte* const record = object_record(object);
    const std::uint32_t nativeOwner = safe_read<std::uint32_t>(
        record == nullptr ? nullptr : record + 0x68U, kInvalidHandle);
    const AttachmentHandleCorrelation graphMatch =
        correlate_attachment_handle(object);
    report("ev=omega_beam_final_transform stage=attachment_bind n=%u caller=+%llX "
           "object=%08X owner_record=%p owner_actor=%u owner_object=%08X "
           "owner_definition=%08X owner_offset=0x%llX owner_words=%016llX,%016llX "
           "binding=%016llX binding_low=%08X binding_high=%04X "
           "attachment_index=%u attachment_flags=%u native_owner=%08X "
           "component_graph_actor_mask=%u component_graph_counts=%u,%u,%u "
           "before_valid=%u before_xyz=%.6f,%.6f,%.6f "
           "after_valid=%u after_xyz=%.6f,%.6f,%.6f mutation=observe_only",
           context.sequence,
           static_cast<unsigned long long>(caller),
           object,
           reinterpret_cast<void*>(ownerRecord),
           owner.factorySequence,
           owner.object,
           owner.definition,
           static_cast<unsigned long long>(owner.offset),
           static_cast<unsigned long long>(owner0),
           static_cast<unsigned long long>(owner8),
           static_cast<unsigned long long>(binding),
           static_cast<std::uint32_t>(binding & 0xFFFFFFFFULL),
           static_cast<unsigned int>((binding >> 32U) & 0xFFFFULL),
           static_cast<unsigned int>(attachmentIndex),
           static_cast<unsigned int>(attachmentFlags),
           nativeOwner,
           graphMatch.actorMask,
           graphMatch.counts[0],
           graphMatch.counts[1],
           graphMatch.counts[2],
           before.valid ? 1U : 0U,
           static_cast<double>(before.value[0]),
           static_cast<double>(before.value[1]),
           static_cast<double>(before.value[2]),
           after.valid ? 1U : 0U,
           static_cast<double>(after.value[0]),
           static_cast<double>(after.value[1]),
           static_cast<double>(after.value[2]));
}

/**
 * Observes every common object/effect creation while the native scene performer is alive.
 *
 * The three type-4 portal components never called this function in the previous run, despite the
 * visible beam. The scene runtime must therefore reach it through another caller or use a lower
 * layer. Capturing the complete descriptor, optional relative reference block, return handle,
 * native owner field, actor handles, and call stack answers that branch without changing state.
 */
__declspec(noinline) std::int32_t* __fastcall scene_visual_object_create(
    std::int32_t* result,
    const std::byte* descriptor,
    std::uint32_t table,
    std::uint32_t record) noexcept {
    const SceneVisualObjectCreate original =
        g_sceneVisualObjectCreateOriginal.load(std::memory_order_acquire);
    if (original == nullptr) {
        return result;
    }

    const std::uint32_t sceneObject =
        g_sceneAnimatedObject.load(std::memory_order_acquire);
    VisualEntryRecorderContext& visualContext = g_visualEntryRecorderContext;
    const bool visualEntryActive = g_visualEntryRecorderDepth != 0U
                                   && visualContext.report;
    const bool inspect = g_sceneVisualCreateDepth == 0U && omega_forced()
                         && (sceneObject != kInvalidHandle || visualEntryActive);
    if (!inspect) {
        return original(result, descriptor, table, record);
    }

    ++g_sceneVisualCreateDepth;
    const std::uint32_t sequence =
        g_sceneVisualCreateCalls.fetch_add(1U, std::memory_order_relaxed) + 1U;
    const bool reportCall = sequence <= kMaximumSceneVisualCreateLogs;

    std::array<std::byte, 0xC0U> descriptorSnapshot{};
    const bool descriptorReadable = reportCall
                                    && safe_copy(descriptorSnapshot.data(),
                                                 descriptor,
                                                 descriptorSnapshot.size());
    const std::int64_t relative80 =
        safe_read<std::int64_t>(descriptor == nullptr ? nullptr : descriptor + 0x80U, 0);
    const std::uintptr_t relativeField =
        reinterpret_cast<std::uintptr_t>(descriptor == nullptr ? nullptr : descriptor + 0x80U);
    const std::uintptr_t referenceAddress =
        relativeField != 0U && relative80 != 0
            ? relativeField + static_cast<std::uintptr_t>(relative80)
            : 0U;
    const auto* const reference = reinterpret_cast<const std::byte*>(referenceAddress);
    std::array<std::byte, 0x60U> referenceSnapshot{};
    const bool referenceReadable = reportCall && reference != nullptr
                                   && safe_copy(referenceSnapshot.data(),
                                                reference,
                                                referenceSnapshot.size());

    std::array<void*, 16U> frames{};
    std::array<std::uintptr_t, 16U> frameRvas{};
    USHORT frameCount = 0U;
    if (reportCall) {
        frameCount = RtlCaptureStackBackTrace(
            1U, static_cast<ULONG>(frames.size()), frames.data(), nullptr);
        for (USHORT index = 0U; index < frameCount; ++index) {
            frameRvas[index] = image_rva(reinterpret_cast<std::uintptr_t>(frames[index]));
        }
    }
    const std::uintptr_t caller = image_rva(
        reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
    const bool visualEntryCreate = visualEntryActive
                                   && caller == kVisualEntryObjectCreateReturnRva;
    if (visualEntryCreate) {
        ++visualContext.objectCreateCalls;
        visualContext.createdDefinition = safe_read<std::uint32_t>(
            descriptor, kInvalidHandle);
        visualContext.createdTransformReadable = safe_copy(
            visualContext.createdTransform.data(),
            descriptor == nullptr ? nullptr : descriptor + 0x10U,
            visualContext.createdTransform.size());
    }
    const std::uint32_t latestObject = g_ikoraObject.load(std::memory_order_acquire);
    const std::uintptr_t sceneActor =
        g_ikoraBehaviorActor.load(std::memory_order_acquire);
    const std::uintptr_t persistentActor =
        g_ikoraHandoffBehaviorActor.load(std::memory_order_acquire);

    std::int32_t* const returned = original(result, descriptor, table, record);
    const std::uint32_t createdObject = static_cast<std::uint32_t>(
        safe_read<std::int32_t>(returned, -1));
    if (visualEntryCreate) {
        visualContext.createdObject = createdObject;
    }
    const std::byte* const createdRecord = object_record(createdObject);
    std::array<std::byte, 0xA0U> createdRecordSnapshot{};
    const bool createdRecordReadable = reportCall && createdRecord != nullptr
                                       && safe_copy(createdRecordSnapshot.data(),
                                                    createdRecord,
                                                    createdRecordSnapshot.size());

    if (visualEntryCreate) {
        const DecodedSceneTransform finalDescriptor =
            decode_scene_transform_address(
                descriptor == nullptr ? nullptr : descriptor + 0x10U);
        const DecodedObjectPosition createdPosition =
            decode_object_position(createdObject);
        const IkoraActor actor1 = ikora_factory_actor(1U);
        const IkoraActor actor2 = ikora_factory_actor(2U);
        const IkoraActor actor3 = ikora_factory_actor(3U);
        report("ev=omega_beam_final_transform stage=object_create n=%u caller=+%llX "
               "component=%p entry_index=%d descriptor=%p definition=%08X "
               "transform_readable=%u rotation=%.6f,%.6f,%.6f,%.6f "
               "xyzs=%.6f,%.6f,%.6f,%.6f "
               "distance_actor1=%.3f distance_actor2=%.3f distance_actor3=%.3f "
               "created=%08X pre_bind_world_valid=%u pre_bind_world_xyz=%.6f,%.6f,%.6f "
               "mutation=observe_only",
               visualContext.sequence,
               static_cast<unsigned long long>(caller),
               visualContext.component,
               visualContext.entryIndex,
               descriptor,
               visualContext.createdDefinition,
               finalDescriptor.readable ? 1U : 0U,
               static_cast<double>(finalDescriptor.rotation[0]),
               static_cast<double>(finalDescriptor.rotation[1]),
               static_cast<double>(finalDescriptor.rotation[2]),
               static_cast<double>(finalDescriptor.rotation[3]),
               static_cast<double>(finalDescriptor.position[0]),
               static_cast<double>(finalDescriptor.position[1]),
               static_cast<double>(finalDescriptor.position[2]),
               static_cast<double>(finalDescriptor.scale),
               static_cast<double>(position_distance(
                   finalDescriptor, decode_object_position(actor1.object))),
               static_cast<double>(position_distance(
                   finalDescriptor, decode_object_position(actor2.object))),
               static_cast<double>(position_distance(
                   finalDescriptor, decode_object_position(actor3.object))),
               createdObject,
               createdPosition.valid ? 1U : 0U,
               static_cast<double>(createdPosition.value[0]),
               static_cast<double>(createdPosition.value[1]),
               static_cast<double>(createdPosition.value[2]));
    }

    if (reportCall) {
        const auto dword = [](const auto& snapshot, std::size_t offset) noexcept {
            std::uint32_t value = 0U;
            if (offset + sizeof value <= snapshot.size()) {
                std::memcpy(&value, snapshot.data() + offset, sizeof value);
            }
            return value;
        };
        const auto real = [](const auto& snapshot, std::size_t offset) noexcept {
            float value = 0.0F;
            if (offset + sizeof value <= snapshot.size()) {
                std::memcpy(&value, snapshot.data() + offset, sizeof value);
            }
            return value;
        };
        report("ev=omega_scene_vfx_trace stage=create n=%u caller=+%llX depth=%u "
               "descriptor=%p descriptor_readable=%s definition=%08X table=%08X record=%u "
               "descriptor68=%08X phase69=%u relative80=%lld reference=%p "
               "reference_readable=%s reference10=%08X reference14=%08X reference18=%08X "
               "reference1C=%08X reference_xyz20=%.6f,%.6f,%.6f,%.6f "
               "created=%08X created_record=%p created_readable=%s created_definition4C=%08X "
               "created_owner68=%08X scene_object=%08X latest_object=%08X scene_actor=%p "
               "persistent_actor=%p mutation=observe_only",
               sequence,
               static_cast<unsigned long long>(caller),
               static_cast<unsigned int>(frameCount),
               descriptor,
               descriptorReadable ? "yes" : "no",
               dword(descriptorSnapshot, 0x00U),
               table,
               record,
               dword(descriptorSnapshot, 0x68U),
               descriptorReadable
                   ? std::to_integer<unsigned int>(descriptorSnapshot[0x69U])
                   : 0U,
               static_cast<long long>(relative80),
               reference,
               referenceReadable ? "yes" : "no",
               dword(referenceSnapshot, 0x10U),
               dword(referenceSnapshot, 0x14U),
               dword(referenceSnapshot, 0x18U),
               dword(referenceSnapshot, 0x1CU),
               static_cast<double>(real(referenceSnapshot, 0x20U)),
               static_cast<double>(real(referenceSnapshot, 0x24U)),
               static_cast<double>(real(referenceSnapshot, 0x28U)),
               static_cast<double>(real(referenceSnapshot, 0x2CU)),
               createdObject,
               createdRecord,
               createdRecordReadable ? "yes" : "no",
               dword(createdRecordSnapshot, 0x4CU),
               dword(createdRecordSnapshot, 0x68U),
               sceneObject,
               latestObject,
               reinterpret_cast<void*>(sceneActor),
               reinterpret_cast<void*>(persistentActor));
        report("ev=omega_scene_vfx_trace stage=stack n=%u rvas=+%llX,+%llX,+%llX,+%llX,"
               "+%llX,+%llX,+%llX,+%llX,+%llX,+%llX,+%llX,+%llX,+%llX,+%llX,+%llX,"
               "+%llX mutation=observe_only",
               sequence,
               static_cast<unsigned long long>(frameRvas[0]),
               static_cast<unsigned long long>(frameRvas[1]),
               static_cast<unsigned long long>(frameRvas[2]),
               static_cast<unsigned long long>(frameRvas[3]),
               static_cast<unsigned long long>(frameRvas[4]),
               static_cast<unsigned long long>(frameRvas[5]),
               static_cast<unsigned long long>(frameRvas[6]),
               static_cast<unsigned long long>(frameRvas[7]),
               static_cast<unsigned long long>(frameRvas[8]),
               static_cast<unsigned long long>(frameRvas[9]),
               static_cast<unsigned long long>(frameRvas[10]),
               static_cast<unsigned long long>(frameRvas[11]),
               static_cast<unsigned long long>(frameRvas[12]),
               static_cast<unsigned long long>(frameRvas[13]),
               static_cast<unsigned long long>(frameRvas[14]),
               static_cast<unsigned long long>(frameRvas[15]));
        report_scene_visual_words(sequence,
                                  "descriptor",
                                  descriptor,
                                  descriptorSnapshot,
                                  descriptorReadable);
        if (reference != nullptr) {
            report_scene_visual_words(sequence,
                                      "reference",
                                      reference,
                                      referenceSnapshot,
                                      referenceReadable);
        }
        if (createdRecord != nullptr) {
            report_scene_visual_words(sequence,
                                      "created_record",
                                      createdRecord,
                                      createdRecordSnapshot,
                                      createdRecordReadable);
        }
    } else if (sequence == kMaximumSceneVisualCreateLogs + 1U) {
        report("ev=omega_scene_vfx_trace stage=limit n=%u limit=%u mutation=observe_only",
               sequence,
               kMaximumSceneVisualCreateLogs);
    }

    --g_sceneVisualCreateDepth;
    return returned;
}

__declspec(noinline) void __fastcall index_heap_release(void* heap,
                                                         std::uint32_t handle) noexcept {
    const IndexHeapRelease original =
        g_indexHeapReleaseOriginal.load(std::memory_order_acquire);
    if (original == nullptr) {
        return;
    }
    std::byte* const image = g_image.load(std::memory_order_acquire);
    const std::uint32_t cinematicObject =
        g_sceneAnimatedObject.load(std::memory_order_acquire);
    const bool cinematicRelease =
        image != nullptr && heap == image + kObjectIndexHeapRva
        && cinematicObject != kInvalidHandle && handle == cinematicObject;
    const bool sceneTwoPresentationRelease =
        image != nullptr && heap == image + kObjectIndexHeapRva
        && handle == g_sceneTwoPresentationObject.load(std::memory_order_acquire);
    const std::uint32_t trackedSequence = image != nullptr && heap == image + kObjectIndexHeapRva
                                              ? ikora_factory_sequence(handle)
                                              : 0U;
    if (trackedSequence != 0U) {
        const std::byte* const native = object_record(handle);
        const DecodedObjectPosition position = decode_object_position(handle);
        const IkoraActor trackedActor = ikora_factory_actor(trackedSequence);
        report("ev=omega_ikora_ownership stage=object_release moment=before object=%08X "
               "factory_n=%u cinematic=%s current_handle=%08X current=%s record=%p "
               "flags4=%08X definition4c=%08X behavior_controller=%p "
               "behavior_updates=%llu pos_valid=%s pos=%.3f,%.3f,%.3f "
               "handoff_armed=%u mutation=observe_only",
               handle,
               trackedSequence,
               cinematicRelease ? "yes" : "no",
               position.currentHandle,
               position.current ? "yes" : "no",
               native,
               native == nullptr ? 0U : safe_read<std::uint32_t>(native + 0x04U, 0U),
               native == nullptr ? kInvalidHandle
                                 : safe_read<std::uint32_t>(native + 0x4CU, kInvalidHandle),
               reinterpret_cast<void*>(trackedActor.behaviorActor),
               static_cast<unsigned long long>(trackedActor.behaviorUpdates),
               position.valid ? "yes" : "no",
               position.value[0],
               position.value[1],
               position.value[2],
               0U);
    }
    original(heap, handle);
    if (trackedSequence != 0U) {
        remember_ikora_release(handle);
        report("ev=omega_ikora_ownership stage=object_release moment=after object=%08X "
               "factory_n=%u cinematic=%s result=released mutation=observe_only",
               handle,
               trackedSequence,
                cinematicRelease ? "yes" : "no");
    }
    if (sceneTwoPresentationRelease) {
        const std::uintptr_t component =
            g_sceneTwoPresentationComponent.exchange(0U, std::memory_order_acq_rel);
        g_sceneTwoPresentationObject.store(kInvalidHandle, std::memory_order_release);
        for (auto& renderer : g_sceneTwoPresentationRenderers) {
            renderer.store(0U, std::memory_order_release);
        }
        report("ev=omega_scene_two_presentation_suppression stage=release "
               "actor=%08X component=%p action=stop_tracking result=released "
               "mutation=tracking_only",
               handle,
               reinterpret_cast<void*>(component));
    }
    if (cinematicRelease) {
        g_sceneAnimatedObject.store(kInvalidHandle, std::memory_order_release);
        g_sceneCompletionPending.store(true, std::memory_order_release);
        g_indexHeapReleaseDetachPending.store(true, std::memory_order_release);
        report("ev=omega_scene_lifecycle stage=object_release moment=after object=%08X "
               "result=deferred mutation=schedule_handoff_and_probe_retire",
               handle);
    }
}

} // namespace

bool install_activity_spawner_chain_probe() noexcept {
    if (g_installed.load(std::memory_order_acquire)) {
        return true;
    }
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    if (image == nullptr) {
        return false;
    }

    std::array<void*, kHookCount> targets{
        image + kSpawnerApplyRva,
        image + kSpawnerDeficitRva,
        image + kSpawnerRequestRva,
        image + kSpawnerSquadResolveRva,
        image + kSpawnerMemberBuildRva,
        image + kSpawnerQueueFindRva,
        image + kSpawnerDrainRva,
        image + kEntityFactoryRva,
        image + kObjectFinalizeRva,
        image + kEntityForObjectRva,
        image + kComponentStartRva,
        image + kBehaviorStepRva,
        image + kBehaviorNodeRva,
        image + kIndexHeapReleaseRva,
    };
    if (!prefix_matches(static_cast<std::byte*>(targets[0]), kSpawnerApplyPrefix)
        || !prefix_matches(static_cast<std::byte*>(targets[1]), kSpawnerDeficitPrefix)
        || !prefix_matches(static_cast<std::byte*>(targets[2]), kSpawnerRequestPrefix)
        || !prefix_matches(static_cast<std::byte*>(targets[3]), kSpawnerSquadResolvePrefix)
        || !prefix_matches(static_cast<std::byte*>(targets[4]), kSpawnerMemberBuildPrefix)
        || !prefix_matches(static_cast<std::byte*>(targets[5]), kSpawnerQueueFindPrefix)
        || !prefix_matches(static_cast<std::byte*>(targets[6]), kSpawnerDrainPrefix)
        || !prefix_matches(static_cast<std::byte*>(targets[7]), kEntityFactoryPrefix)
        || !prefix_matches(static_cast<std::byte*>(targets[8]), kObjectFinalizePrefix)
        || !prefix_matches(static_cast<std::byte*>(targets[9]), kEntityForObjectPrefix)
        || !prefix_matches(static_cast<std::byte*>(targets[10]), kComponentStartPrefix)
        || !prefix_matches(static_cast<std::byte*>(targets[11]), kBehaviorStepPrefix)
        || !prefix_matches(static_cast<std::byte*>(targets[12]), kBehaviorNodePrefix)
        || !prefix_matches(static_cast<std::byte*>(targets[13]), kIndexHeapReleasePrefix)) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::warn,
                         "ev=omega_spawner_chain stage=install result=prefix_mismatch");
        return false;
    }

    const std::array<hooking::detour::Spec, kHookCount> specs{{
        {targets[0], reinterpret_cast<void*>(&spawner_apply)},
        {targets[1], reinterpret_cast<void*>(&spawner_deficit)},
        {targets[2], reinterpret_cast<void*>(&spawner_request)},
        {targets[3], reinterpret_cast<void*>(&spawner_squad_resolve)},
        {targets[4], reinterpret_cast<void*>(&spawner_member_build)},
        {targets[5], reinterpret_cast<void*>(&spawner_queue_find)},
        {targets[6], reinterpret_cast<void*>(&spawner_drain)},
        {targets[7], reinterpret_cast<void*>(&entity_factory)},
        {targets[8], reinterpret_cast<void*>(&object_finalize)},
        {targets[9], reinterpret_cast<void*>(&entity_for_object)},
        {targets[10], reinterpret_cast<void*>(&component_start)},
        {targets[11], reinterpret_cast<void*>(&behavior_step)},
        {targets[12], reinterpret_cast<void*>(&behavior_node)},
        {targets[13], reinterpret_cast<void*>(&index_heap_release)},
    }};
    g_image.store(image, std::memory_order_release);
    if (!hooking::detour::install(specs, g_handles)) {
        g_image.store(nullptr, std::memory_order_release);
        core::log::write(core::log::Channel::client,
                         core::log::Level::warn,
                         "ev=omega_spawner_chain stage=install result=attach_fail");
        return false;
    }

    g_applyOriginal.store(reinterpret_cast<SpawnerApply>(g_handles[0].original),
                          std::memory_order_release);
    g_deficitOriginal.store(reinterpret_cast<SpawnerDeficit>(g_handles[1].original),
                            std::memory_order_release);
    g_requestOriginal.store(reinterpret_cast<SpawnerRequest>(g_handles[2].original),
                            std::memory_order_release);
    g_squadResolveOriginal.store(reinterpret_cast<SpawnerSquadResolve>(g_handles[3].original),
                                 std::memory_order_release);
    g_memberBuildOriginal.store(reinterpret_cast<SpawnerMemberBuild>(g_handles[4].original),
                                std::memory_order_release);
    g_queueFindOriginal.store(reinterpret_cast<SpawnerQueueFind>(g_handles[5].original),
                              std::memory_order_release);
    g_drainOriginal.store(reinterpret_cast<SpawnerDrain>(g_handles[6].original),
                          std::memory_order_release);
    g_factoryOriginal.store(reinterpret_cast<EntityFactory>(g_handles[7].original),
                            std::memory_order_release);
    g_objectFinalizeOriginal.store(reinterpret_cast<ObjectFinalize>(g_handles[8].original),
                                   std::memory_order_release);
    g_entityForObjectOriginal.store(reinterpret_cast<EntityForObject>(g_handles[9].original),
                                    std::memory_order_release);
    g_componentStartOriginal.store(reinterpret_cast<ComponentStart>(g_handles[10].original),
                                   std::memory_order_release);
    g_behaviorStepOriginal.store(reinterpret_cast<BehaviorStep>(g_handles[11].original),
                                 std::memory_order_release);
    g_behaviorNodeOriginal.store(reinterpret_cast<BehaviorNode>(g_handles[12].original),
                                 std::memory_order_release);
    g_indexHeapReleaseOriginal.store(
        reinterpret_cast<IndexHeapRelease>(g_handles[13].original),
        std::memory_order_release);
    const std::array<void*, kRendererHookCount> rendererTargets{
        image + kRendererBulkBooleanRva,
        image + kRendererBulkSiblingRva,
        image + kRendererObjectARva,
        image + kRendererObjectBRva,
        image + kRendererObjectCRva,
    };
    const bool rendererPrefixes =
        prefix_matches(static_cast<std::byte*>(rendererTargets[0]),
                       kRendererBulkBooleanPrefix)
        && prefix_matches(static_cast<std::byte*>(rendererTargets[1]),
                          kRendererBulkSiblingPrefix)
        && prefix_matches(static_cast<std::byte*>(rendererTargets[2]),
                          kRendererObjectAPrefix)
        && prefix_matches(static_cast<std::byte*>(rendererTargets[3]),
                          kRendererObjectBPrefix)
        && prefix_matches(static_cast<std::byte*>(rendererTargets[4]),
                          kRendererObjectCPrefix);
    bool rendererAttached = false;
    if (rendererPrefixes) {
        const std::array<hooking::detour::Spec, kRendererHookCount> rendererSpecs{{
            {rendererTargets[0], reinterpret_cast<void*>(&renderer_bulk_boolean)},
            {rendererTargets[1], reinterpret_cast<void*>(&renderer_bulk_sibling)},
            {rendererTargets[2], reinterpret_cast<void*>(&renderer_object_a)},
            {rendererTargets[3], reinterpret_cast<void*>(&renderer_object_b)},
            {rendererTargets[4], reinterpret_cast<void*>(&renderer_object_c)},
        }};
        rendererAttached = hooking::detour::install(rendererSpecs, g_rendererHandles);
        if (rendererAttached) {
            g_rendererBulkBooleanOriginal.store(
                reinterpret_cast<RendererBulkBoolean>(g_rendererHandles[0].original),
                std::memory_order_release);
            g_rendererBulkSiblingOriginal.store(
                reinterpret_cast<RendererBulkSibling>(g_rendererHandles[1].original),
                std::memory_order_release);
            g_rendererObjectAOriginal.store(
                reinterpret_cast<RendererObject>(g_rendererHandles[2].original),
                std::memory_order_release);
            g_rendererObjectBOriginal.store(
                reinterpret_cast<RendererObject>(g_rendererHandles[3].original),
                std::memory_order_release);
            g_rendererObjectCOriginal.store(
                reinterpret_cast<RendererObject>(g_rendererHandles[4].original),
                std::memory_order_release);
        }
    }
    report("ev=omega_actor_renderer_handle stage=install result=%s "
           "targets=+%llX,+%llX,+%llX,+%llX,+%llX "
           "filter=omega_scene_after_first_actor "
           "capture=unique_wrapper_object,actors,presentation_refs,before_after "
           "sample_limit=%u treatment=observe_all_scenes_model_suppression_ab "
           "mutation=observe_only",
           rendererAttached ? "ok" : rendererPrefixes ? "attach_fail" : "prefix_mismatch",
           static_cast<unsigned long long>(kRendererBulkBooleanRva),
           static_cast<unsigned long long>(kRendererBulkSiblingRva),
           static_cast<unsigned long long>(kRendererObjectARva),
           static_cast<unsigned long long>(kRendererObjectBRva),
           static_cast<unsigned long long>(kRendererObjectCRva),
           kMaximumRendererHandleSamples);
    std::byte* const presentationBodyStateTarget = image + kPresentationBodyStateRva;
    const bool presentationBodyStatePrefix = prefix_matches(
        presentationBodyStateTarget, kPresentationBodyStatePrefix);
    bool presentationBodyStateAttached = false;
    if (presentationBodyStatePrefix) {
        const hooking::detour::Spec presentationBodyStateSpec{
            presentationBodyStateTarget,
            reinterpret_cast<void*>(&presentation_body_state)};
        presentationBodyStateAttached = hooking::detour::install(
            presentationBodyStateSpec, g_presentationBodyStateHandle);
        if (presentationBodyStateAttached) {
            g_presentationBodyStateOriginal.store(
                reinterpret_cast<PresentationBodyState>(
                    g_presentationBodyStateHandle.original),
                std::memory_order_release);
        }
    }
    report("ev=omega_scene_two_body_state stage=install result=%s target=+%llX "
           "enabled=%s scene=80EC0FA8 component=80EC13A2 state_offset=0x240 "
           "ab=all_scenes_native scene_cast_suppression=disabled "
           "scene_events=native vfx_paths=native squad_ikora_untouched=yes "
           "mutation=override_scene_two_body_enable",
           presentationBodyStateAttached
               ? "ok"
               : presentationBodyStatePrefix ? "attach_fail" : "prefix_mismatch",
           static_cast<unsigned long long>(kPresentationBodyStateRva),
           kEnableOmegaSceneTwoBodyStateSuppression ? "yes" : "no");
    g_sceneCompletionPending.store(false, std::memory_order_release);
    g_indexHeapReleaseDetachPending.store(false, std::memory_order_release);
    g_indexHeapReleaseDetachBusy.clear(std::memory_order_release);
    g_installed.store(true, std::memory_order_release);
    g_sceneVisualCreateCalls.store(0U, std::memory_order_release);
    g_visualEntryRecorderCalls.store(0U, std::memory_order_release);
    g_sceneAuthoredEventCalls.store(0U, std::memory_order_release);
    g_sceneAuthoredEventSamples.store(0U, std::memory_order_release);
    g_sceneAuthoredEventAttempts.store(0U, std::memory_order_release);
    g_sceneType23Calls.store(0U, std::memory_order_release);
    g_sceneType23Samples.store(0U, std::memory_order_release);
    g_sceneTransformWriterCalls.store(0U, std::memory_order_release);
    g_sceneTransformWriterSamples.store(0U, std::memory_order_release);
    g_sceneTransformKind2ResolveCalls.store(0U, std::memory_order_release);
    g_sceneTransformKind2Samples.store(0U, std::memory_order_release);
    g_sceneTransformKind2ProviderTreatmentCalls.store(0U, std::memory_order_release);
    g_sceneTransformKind2ProviderTreatmentApplied.store(0U, std::memory_order_release);
    g_sceneTransformKind2ProviderTreatmentFallbacks.store(0U, std::memory_order_release);
    g_rendererHandleSamples.store(0U, std::memory_order_release);
    g_sceneTwoPresentationComponent.store(0U, std::memory_order_release);
    g_sceneTwoPresentationObject.store(kInvalidHandle, std::memory_order_release);
    g_sceneTwoPresentationCaptures.store(0U, std::memory_order_release);
    g_sceneTwoPresentationDisableCalls.store(0U, std::memory_order_release);
    g_sceneTwoPresentationDisabledObjects.store(0U, std::memory_order_release);
    g_sceneTwoBodyStateCalls.store(0U, std::memory_order_release);
    g_sceneTwoBodyStateOverrides.store(0U, std::memory_order_release);
    g_sceneTwoBodyStateForces.store(0U, std::memory_order_release);
    g_sceneTwoModelSuppressions.store(0U, std::memory_order_release);
    for (auto& renderer : g_sceneTwoPresentationRenderers) {
        renderer.store(0U, std::memory_order_release);
    }
    AcquireSRWLockExclusive(&g_rendererTraceLock);
    g_rendererTraceKeys = {};
    g_rendererTraceKeyCount = 0U;
    ReleaseSRWLockExclusive(&g_rendererTraceLock);
    g_sceneTransformWriterLastTick.store(0U, std::memory_order_release);
    g_sceneTransformKind2LastTick.store(0U, std::memory_order_release);
    g_sceneTransformKind2LastProvider.store(0U, std::memory_order_release);
    g_sceneTransformKind2LastCandidate.store(kInvalidHandle, std::memory_order_release);
    g_sceneTransformKind2LastResult.store(INT32_MIN, std::memory_order_release);
    g_scenePoseBindingKey.store(kInvalidHandle, std::memory_order_release);
    g_scenePoseProviderTargetHandle.store(kInvalidHandle, std::memory_order_release);
    g_scenePoseProviderTargetRecord.store(0U, std::memory_order_release);
    g_scenePoseProviderTargetRelative.store(INT64_MIN, std::memory_order_release);
    g_scenePoseProviderTarget.store(0U, std::memory_order_release);
    g_scenePoseProviderTargetObservedMask.store(0U, std::memory_order_release);
    g_scenePoseProviderTargetResolvedMask.store(0U, std::memory_order_release);
    for (std::size_t index = 0U; index < g_scenePoseNaturalCalls.size(); ++index) {
        g_scenePoseNaturalCalls[index].store(0U, std::memory_order_release);
        g_scenePoseNaturalSamples[index].store(0U, std::memory_order_release);
        g_scenePoseNaturalLastTick[index].store(0U, std::memory_order_release);
    }
    g_poseSocketDispatchCalls.store(0U, std::memory_order_release);
    g_poseSocketDispatchSamples.store(0U, std::memory_order_release);
    g_poseSocketDispatchLastTick.store(0U, std::memory_order_release);
    g_poseSocketVfxDispatchBase.store(0U, std::memory_order_release);
    g_poseSocketVfxDispatchTarget.store(0U, std::memory_order_release);
    g_poseSocketVfxObject.store(0U, std::memory_order_release);
    g_poseSocketVfxLastCallTick.store(0U, std::memory_order_release);
    g_effectTransformComposeCalls.store(0U, std::memory_order_release);
    g_effectTransformComposeSamples.store(0U, std::memory_order_release);
    g_omegaPurpleDirectEffectOffsetCalls.store(0U, std::memory_order_release);
    g_omegaPurpleDirectEffectOffsetApplied.store(0U, std::memory_order_release);
    g_omegaPurpleDirectEffectOffsetSeenMask.store(0U, std::memory_order_release);
    g_omegaPurpleDirectEffectOffsetLastTick.store(0U, std::memory_order_release);
    g_poseProviderProvenanceCalls.store(0U, std::memory_order_release);
    g_poseProviderProvenanceSamples.store(0U, std::memory_order_release);
    g_omegaPurplePoseRebindCalls.store(0U, std::memory_order_release);
    g_omegaPurplePoseRebindApplied.store(0U, std::memory_order_release);
    g_omegaPurplePoseRebindFallbacks.store(0U, std::memory_order_release);
    g_omegaPurplePoseRebindLastTick.store(0U, std::memory_order_release);
    clear_scene_one_pose_binding();
    AcquireSRWLockExclusive(&g_poseSocketTraceLock);
    g_poseSocketTraceKeys = {};
    g_poseSocketTraceKeyCount = 0U;
    ReleaseSRWLockExclusive(&g_poseSocketTraceLock);
    AcquireSRWLockExclusive(&g_effectTransformTraceLock);
    g_effectTransformTraceKeys = {};
    g_effectTransformTraceKeyCount = 0U;
    ReleaseSRWLockExclusive(&g_effectTransformTraceLock);
    AcquireSRWLockExclusive(&g_vfxSocketTransformLock);
    g_vfxSocketTransform = {};
    ReleaseSRWLockExclusive(&g_vfxSocketTransformLock);
    AcquireSRWLockExclusive(&g_poseProviderProvenanceTraceLock);
    g_poseProviderProvenanceTraceKeys = {};
    g_poseProviderProvenanceTraceKeyCount = 0U;
    ReleaseSRWLockExclusive(&g_poseProviderProvenanceTraceLock);
    for (auto& pointer : g_omegaSceneSchedulerPointers) {
        pointer.store(0U, std::memory_order_release);
    }
    for (auto& pointer : g_omegaSceneEventPointers) {
        pointer.store(0U, std::memory_order_release);
    }
    g_sceneAuthoredEventScene = nullptr;
    g_sceneAuthoredEventIndex = -1;
    g_sceneAuthoredEventHandle = kInvalidHandle;
    g_sceneAuthoredEventRuntime = nullptr;
    g_sceneAuthoredEventAuthored = nullptr;
    g_sceneActivationObservedMask.store(0U, std::memory_order_release);
    g_sceneCastSuppressionObservedMask.store(0U, std::memory_order_release);
    g_sceneTwoPersistenceRetireCalls.store(0U, std::memory_order_release);
    report("ev=omega_ikora_ownership stage=install result=ok "
           "capture=current_handle,position,parent,behavior_controller_ticks," 
           "component_graph,purple_references,all_actor_releases "
           "mode=change_driven_and_one_second_snapshot mutation=observe_only");
    AcquireSRWLockExclusive(&g_sceneEventTraceStateLock);
    g_sceneEventTraceStates = {};
    g_sceneEventTraceStateCount = 0U;
    ReleaseSRWLockExclusive(&g_sceneEventTraceStateLock);
    AcquireSRWLockExclusive(&g_sceneType23TraceStateLock);
    g_sceneType23TraceStates = {};
    g_sceneType23TraceStateCount = 0U;
    ReleaseSRWLockExclusive(&g_sceneType23TraceStateLock);
    g_sceneEntryResolverCalls.store(0U, std::memory_order_release);
    std::byte* const sceneEntryUpdateTarget = image + kSceneEntryUpdateRva;
    std::byte* const sceneEntryResolverTarget = image + kSceneEntryResolverRva;
    const bool sceneEntryUpdatePrefix = prefix_matches(
        sceneEntryUpdateTarget, kSceneEntryUpdatePrefix);
    const bool sceneEntryResolverPrefix = prefix_matches(
        sceneEntryResolverTarget, kSceneEntryResolverPrefix);
    bool sceneEntryRuntimeAttached = false;
    if (sceneEntryUpdatePrefix && sceneEntryResolverPrefix) {
        const hooking::detour::Spec updateSpec{
            sceneEntryUpdateTarget, reinterpret_cast<void*>(&scene_entry_update)};
        if (hooking::detour::install(updateSpec, g_sceneEntryUpdateHandle)) {
            g_sceneEntryUpdateOriginal.store(
                reinterpret_cast<SceneEntryUpdate>(g_sceneEntryUpdateHandle.original),
                std::memory_order_release);
            const hooking::detour::Spec resolverSpec{
                sceneEntryResolverTarget, reinterpret_cast<void*>(&scene_entry_resolver)};
            if (hooking::detour::install(resolverSpec, g_sceneEntryResolverHandle)) {
                g_sceneEntryResolverOriginal.store(
                    reinterpret_cast<SceneEntryResolver>(g_sceneEntryResolverHandle.original),
                    std::memory_order_release);
                sceneEntryRuntimeAttached = true;
            } else {
                (void)hooking::detour::uninstall(g_sceneEntryUpdateHandle);
                g_sceneEntryUpdateHandle = {};
                g_sceneEntryUpdateOriginal.store(nullptr, std::memory_order_release);
            }
        }
    }
    report("ev=scene_entry_runtime_probe stage=install result=%s "
           "update=+%llX resolver=+%llX caller=+%llX "
           "capture=component,entry,reference,resolver_output mode=observe_only",
           sceneEntryRuntimeAttached
               ? "ok"
               : sceneEntryUpdatePrefix && sceneEntryResolverPrefix
                     ? "attach_fail"
                     : "prefix_mismatch",
           static_cast<unsigned long long>(kSceneEntryUpdateRva),
           static_cast<unsigned long long>(kSceneEntryResolverRva),
           static_cast<unsigned long long>(kSceneEntryResolverReturnRva));
    std::byte* const sceneActorSchedulerTarget = image + kSceneActorSchedulerRva;
    const bool sceneActorSchedulerPrefix =
        prefix_matches(sceneActorSchedulerTarget, kSceneActorSchedulerPrefix);
    bool sceneActorSchedulerAttached = false;
    if (sceneActorSchedulerPrefix) {
        const hooking::detour::Spec sceneActorSchedulerSpec{
            sceneActorSchedulerTarget, reinterpret_cast<void*>(&scene_actor_scheduler)};
        sceneActorSchedulerAttached = hooking::detour::install(
            sceneActorSchedulerSpec, g_sceneActorSchedulerHandle);
        if (sceneActorSchedulerAttached) {
            g_sceneActorSchedulerOriginal.store(
                reinterpret_cast<SceneActorScheduler>(
                    g_sceneActorSchedulerHandle.original),
                std::memory_order_release);
        }
    }
    std::byte* const sceneTransitionRetireTarget = image + kSceneTransitionRetireRva;
    const bool sceneTransitionRetirePrefix = prefix_matches(
        sceneTransitionRetireTarget,
        kSceneTransitionRetirePrefix);
    bool sceneTransitionRetireAttached = false;
    if (sceneTransitionRetirePrefix) {
        const hooking::detour::Spec sceneTransitionRetireSpec{
            sceneTransitionRetireTarget,
            reinterpret_cast<void*>(&scene_transition_retire)};
        sceneTransitionRetireAttached = hooking::detour::install(
            sceneTransitionRetireSpec,
            g_sceneTransitionRetireHandle);
        if (sceneTransitionRetireAttached) {
            g_sceneTransitionRetireOriginal.store(
                reinterpret_cast<SceneTransitionRetire>(
                    g_sceneTransitionRetireHandle.original),
                std::memory_order_release);
        }
    }
    report("ev=omega_scene_two_persistence stage=install result=%s target=+%llX "
           "enabled=%s filter=scene_80EC0FA8_terminal_transition_low_byte_ff "
           "scene1=native scene2=retain_terminal scene3=native "
           "mission_completion=deferred_host_latch "
           "mutation=skip_scene_two_terminal_retirement",
           sceneTransitionRetireAttached
               ? "ok"
               : sceneTransitionRetirePrefix ? "attach_fail" : "prefix_mismatch",
           static_cast<unsigned long long>(kSceneTransitionRetireRva),
           kEnableOmegaSceneTwoInfinitePersistence ? "yes" : "no");
    report("ev=omega_scene_cast_slot stage=install result=%s target=+%llX "
           "capture=slot_index,authored_record,runtime_binding,"
           "type43_authority_owner,initial_scene_flags "
           "ab=%s mutation=observe_only",
           sceneActorSchedulerAttached
               ? "ok"
               : sceneActorSchedulerPrefix ? "attach_fail" : "prefix_mismatch",
           static_cast<unsigned long long>(kSceneActorSchedulerRva),
           kEnableOmegaSceneOneWithVfxAb
               ? "scene_one_with_native_vfx"
               : kEnableOmegaIkoraSceneTwoOnlyAb ? "scene_two_only" : "all_scenes_native");
    report("ev=omega_scene_cast_suppression stage=install result=%s "
           "target=+%llX enabled=%s ab=%s allow=%s "
           "suppress=%s scene_events=native "
           "presentation_renderer_array_suppression=disabled "
           "scene_two_body_state_suppression=disabled "
           "scene_two_model_suppression=enabled vfx_paths=native "
           "squad_ikora=%08X squad_ikora_untouched=yes "
           "mutation=observe_only",
           sceneActorSchedulerAttached
               ? "ok"
               : sceneActorSchedulerPrefix ? "attach_fail" : "prefix_mismatch",
           static_cast<unsigned long long>(kSceneActorSchedulerRva),
           (kEnableOmegaIkoraSceneTwoOnlyAb || kEnableOmegaSceneOneWithVfxAb)
               ? "yes"
               : "no",
           kEnableOmegaSceneOneWithVfxAb
               ? "scene_one_with_native_vfx"
               : kEnableOmegaIkoraSceneTwoOnlyAb ? "scene_two_only" : "all_scenes_native",
           kEnableOmegaSceneOneWithVfxAb
               ? "scene1,scene2_hidden_model"
               : kEnableOmegaIkoraSceneTwoOnlyAb ? "scene2" : "all",
           kEnableOmegaSceneOneWithVfxAb
               ? "scene3"
               : kEnableOmegaIkoraSceneTwoOnlyAb ? "scene1,scene3" : "none",
           kSquadIkoraDefinition);
    report("ev=omega_scene_two_model_suppression stage=install result=%s "
           "enabled=%s scene=80EC0FA8 "
           "primary_definition=%08X primary_builder=+%llX "
           "mesh_cloth_definition=%08X mesh_cloth_builder=+%llX "
           "head_definition=%08X head_builder=+%llX "
           "scene_events=native animation_graph=native vfx_paths=native "
           "mutation=suppress_scene_two_model_builders",
           sceneActorSchedulerAttached ? "ok" : "scene_scheduler_unavailable",
           kEnableOmegaSceneTwoModelSuppression ? "yes" : "no",
           kIkoraPrimaryModelDefinition,
           static_cast<unsigned long long>(kIkoraPrimaryModelBuildRva),
           kIkoraMeshClothModelDefinition,
           static_cast<unsigned long long>(kIkoraMeshClothModelBuildRva),
           kIkoraHeadModelDefinition,
           static_cast<unsigned long long>(kIkoraHeadModelBuildRva));
    report("ev=omega_scene_two_presentation_suppression stage=install "
           "result=disabled_for_model_suppression_ab enabled=%s scene=80EC0FA8 "
           "component=80EC13A2 scene_two_events_native=yes vfx_paths=native "
           "squad_ikora=%08X squad_ikora_untouched=yes kind2_provider_ab=disabled "
           "replacement=80EC0F17,80EC0F1D,80F2EC2F_model_build_suppression "
           "mutation=observe_only",
           kEnableOmegaSceneTwoPresentationSuppression ? "yes" : "no",
           kSquadIkoraDefinition);
    report("ev=omega_scene_activation_owner stage=install result=%s target=+%llX "
           "filter=80EC0F0E,80EC0FA8,80EC0FA6 "
           "capture=type43_sequence,age,source_hash,component_fields,scene_flags,stack "
           "mutation=observe_only",
           sceneActorSchedulerAttached
               ? "ok"
               : sceneActorSchedulerPrefix ? "attach_fail" : "prefix_mismatch",
           static_cast<unsigned long long>(kSceneActorSchedulerRva));
    std::byte* const sceneVisualTarget = image + kSceneVisualObjectCreateRva;
    const bool sceneVisualPrefix = report_scene_visual_target(sceneVisualTarget);
    bool sceneVisualAttached = false;
    if (sceneVisualPrefix) {
        const hooking::detour::Spec sceneVisualSpec{
            sceneVisualTarget, reinterpret_cast<void*>(&scene_visual_object_create)};
        sceneVisualAttached = hooking::detour::install(
            sceneVisualSpec, g_sceneVisualObjectCreateHandle);
        if (sceneVisualAttached) {
            g_sceneVisualObjectCreateOriginal.store(
                reinterpret_cast<SceneVisualObjectCreate>(
                    g_sceneVisualObjectCreateHandle.original),
                std::memory_order_release);
        }
    }
    report("ev=omega_scene_vfx_trace stage=install result=%s target=+%llX "
           "filter=scene_actor_alive capture=descriptor,reference,created_record,stack "
           "limit=%u mutation=observe_only",
           sceneVisualAttached ? "ok" : sceneVisualPrefix ? "attach_fail" : "prefix_mismatch",
           static_cast<unsigned long long>(kSceneVisualObjectCreateRva),
           kMaximumSceneVisualCreateLogs);
    const std::array<std::byte*, kVisualRecorderHookCount> visualRecorderTargets{
        image + kVisualEntryCreateRva,
        image + kVisualTransformApplyRva,
        image + kVisualAttachmentBindRva,
    };
    const bool visualEntryPrefix = report_visual_recorder_target(
        "entry_create",
        kVisualEntryCreateRva,
        visualRecorderTargets[0],
        kVisualEntryCreatePrefix);
    const bool visualTransformPrefix = report_visual_recorder_target(
        "transform_apply",
        kVisualTransformApplyRva,
        visualRecorderTargets[1],
        kVisualTransformApplyPrefix);
    const bool visualAttachmentPrefix = report_visual_recorder_target(
        "attachment_bind",
        kVisualAttachmentBindRva,
        visualRecorderTargets[2],
        kVisualAttachmentBindPrefix);
    const bool visualRecorderPrefixes = visualEntryPrefix
                                        && visualTransformPrefix
                                        && visualAttachmentPrefix;
    bool visualRecorderAttached = false;
    if (visualRecorderPrefixes) {
        const std::array<hooking::detour::Spec, kVisualRecorderHookCount>
            visualRecorderSpecs{{
                {visualRecorderTargets[0], reinterpret_cast<void*>(&visual_entry_create)},
                {visualRecorderTargets[1], reinterpret_cast<void*>(&visual_transform_apply)},
                {visualRecorderTargets[2], reinterpret_cast<void*>(&visual_attachment_bind)},
            }};
        visualRecorderAttached = hooking::detour::install(
            visualRecorderSpecs, g_visualRecorderHandles);
        if (visualRecorderAttached) {
            g_visualEntryCreateOriginal.store(
                reinterpret_cast<VisualEntryCreate>(
                    g_visualRecorderHandles[0].original),
                std::memory_order_release);
            g_visualTransformApplyOriginal.store(
                reinterpret_cast<VisualTransformApply>(
                    g_visualRecorderHandles[1].original),
                std::memory_order_release);
            g_visualAttachmentBindOriginal.store(
                reinterpret_cast<VisualAttachmentBind>(
                    g_visualRecorderHandles[2].original),
                std::memory_order_release);
        }
    }
    report("ev=omega_beam_final_transform stage=install result=%s "
           "targets=+%llX,+%llX,+%llX object_create=+%llX "
           "capture=visual_component_owner,tagged_transform,final_descriptor,"
           "created_object,parent_proxy_bind,committed_world_position "
           "filter=omega_exact_native_callers limit=%u mutation=observe_only",
           visualRecorderAttached
               ? "ok"
               : visualRecorderPrefixes ? "attach_fail" : "prefix_mismatch",
           static_cast<unsigned long long>(kVisualEntryCreateRva),
           static_cast<unsigned long long>(kVisualTransformApplyRva),
           static_cast<unsigned long long>(kVisualAttachmentBindRva),
           static_cast<unsigned long long>(kSceneVisualObjectCreateRva),
           kMaximumVisualEntryRecorderLogs);
    std::byte* const sceneEventTarget = image + kSceneAuthoredEventDispatchRva;
    const bool sceneEventPrefix = report_scene_authored_event_target(sceneEventTarget);
    bool sceneEventAttached = false;
    if (sceneEventPrefix) {
        const hooking::detour::Spec sceneEventSpec{
            sceneEventTarget, reinterpret_cast<void*>(&scene_authored_event_dispatch)};
        sceneEventAttached = hooking::detour::install(
            sceneEventSpec, g_sceneAuthoredEventDispatchHandle);
        if (sceneEventAttached) {
            g_sceneAuthoredEventDispatchOriginal.store(
                reinterpret_cast<SceneAuthoredEventDispatch>(
                    g_sceneAuthoredEventDispatchHandle.original),
                std::memory_order_release);
        }
    }
    report("ev=omega_scene_event_trace stage=install result=%s target=+%llX "
           "filter=mission_scot_actor2_live "
           "mode=change_driven heartbeat_ms=%llu "
           "capture=resolver,event_type,callback,authored,runtime,actors,"
           "type23_created_visual,owner_chain "
           "sample_limit=%u state_limit=%llu resolve_failure_limit=%u "
           "mutation=observe_only",
           sceneEventAttached ? "ok" : sceneEventPrefix ? "attach_fail" : "prefix_mismatch",
           static_cast<unsigned long long>(kSceneAuthoredEventDispatchRva),
           static_cast<unsigned long long>(kSceneEventHeartbeatMs),
           kMaximumSceneAuthoredEventLogs,
           static_cast<unsigned long long>(kMaximumSceneEventTraceStates),
           kMaximumSceneEventResolveFailureLogs);
    std::byte* const type23Target = image + kSceneType23CallbackRva;
    const bool type23Prefix = report_scene_type23_target(
        "callback", kSceneType23CallbackRva, type23Target, kSceneType23CallbackPrefix);
    bool type23Attached = false;
    if (type23Prefix) {
        const hooking::detour::Spec type23Spec{
            type23Target, reinterpret_cast<void*>(&scene_type23_callback)};
        type23Attached = hooking::detour::install(type23Spec, g_sceneType23CallbackHandle);
        if (type23Attached) {
            g_sceneType23CallbackOriginal.store(
                reinterpret_cast<SceneType23Callback>(g_sceneType23CallbackHandle.original),
                std::memory_order_release);
        }
    }
    std::byte* const type23TransformTarget = image + kSceneType23TransformResolveRva;
    const bool type23TransformPrefix = report_scene_type23_target(
        "transform_resolver",
        kSceneType23TransformResolveRva,
        type23TransformTarget,
        kSceneType23TransformResolvePrefix);
    bool type23TransformAttached = false;
    if (type23TransformPrefix) {
        const hooking::detour::Spec type23TransformSpec{
            type23TransformTarget,
            reinterpret_cast<void*>(&scene_type23_transform_resolve)};
        type23TransformAttached = hooking::detour::install(
            type23TransformSpec, g_sceneType23TransformResolveHandle);
        if (type23TransformAttached) {
            g_sceneType23TransformResolveOriginal.store(
                reinterpret_cast<SceneType23TransformResolve>(
                    g_sceneType23TransformResolveHandle.original),
                std::memory_order_release);
        }
    }
    report("ev=omega_scene_type23_trace stage=install callback=%s callback_rva=+%llX "
           "transform_resolver=%s transform_rva=+%llX mode=change_driven "
           "heartbeat_ms=%llu sample_limit=%u state_limit=%llu "
           "capture=context_owner,live_created_owner,selector,transform,"
           "renderer_fingerprint,cd7050_owner_component_graph "
           "suppression=disabled_for_model_suppression_ab native_callback=yes "
           "actor_components_untouched=yes mutation=observe_only",
           type23Attached ? "ok" : type23Prefix ? "attach_fail" : "prefix_mismatch",
           static_cast<unsigned long long>(kSceneType23CallbackRva),
           type23TransformAttached
               ? "ok"
               : type23TransformPrefix ? "attach_fail" : "prefix_mismatch",
           static_cast<unsigned long long>(kSceneType23TransformResolveRva),
           static_cast<unsigned long long>(kSceneEventHeartbeatMs),
           kMaximumSceneType23Samples,
           static_cast<unsigned long long>(kMaximumSceneType23TraceStates));
    report("ev=omega_vfx_renderer_correlation stage=install result=%s "
           "callback=+%llX transform_resolver=+%llX "
           "targets=context_owner_cd7050,context_object,created_owner,"
           "vfx_context_records,transform_output "
           "actors=1,2,3 evidence=exact_pointer_or_handle_equality "
           "mutation=observe_only",
           type23Attached && type23TransformAttached ? "ok" : "incomplete",
           static_cast<unsigned long long>(kSceneType23CallbackRva),
           static_cast<unsigned long long>(kSceneType23TransformResolveRva));
    std::byte* const sceneTransformWriterTarget =
        image + kSceneTransformSourceUpdateRva;
    const bool sceneTransformWriterPrefix = report_scene_type23_target(
        "transform_bank_writer",
        kSceneTransformSourceUpdateRva,
        sceneTransformWriterTarget,
        kSceneTransformSourceUpdatePrefix);
    bool sceneTransformWriterAttached = false;
    if (sceneTransformWriterPrefix) {
        const hooking::detour::Spec sceneTransformWriterSpec{
            sceneTransformWriterTarget,
            reinterpret_cast<void*>(&scene_transform_source_update)};
        sceneTransformWriterAttached = hooking::detour::install(
            sceneTransformWriterSpec, g_sceneTransformSourceUpdateHandle);
        if (sceneTransformWriterAttached) {
            g_sceneTransformSourceUpdateOriginal.store(
                reinterpret_cast<SceneTransformSourceUpdate>(
                    g_sceneTransformSourceUpdateHandle.original),
                std::memory_order_release);
        }
    }
    report("ev=omega_scene_transform_writer stage=install result=%s target=+%llX "
           "filter=scene_80FCCE87_bank_entry_1 "
           "capture=source_kind,output_range,cast_slot,source_runtime,before_after,stack "
           "sample_limit=%u mutation=observe_only",
           sceneTransformWriterAttached
               ? "ok"
               : sceneTransformWriterPrefix ? "attach_fail" : "prefix_mismatch",
           static_cast<unsigned long long>(kSceneTransformSourceUpdateRva),
           kMaximumSceneTransformWriterSamples);
    std::byte* const sceneTransformPoseCandidateTarget =
        image + kSceneTransformPoseCandidateResolveRva;
    const bool sceneTransformPoseCandidatePrefix = report_scene_type23_target(
        "transform_pose_candidate_resolver",
        kSceneTransformPoseCandidateResolveRva,
        sceneTransformPoseCandidateTarget,
        kSceneTransformPoseCandidateResolvePrefix);
    bool sceneTransformPoseCandidateAttached = false;
    if (sceneTransformPoseCandidatePrefix) {
        const hooking::detour::Spec sceneTransformPoseCandidateSpec{
            sceneTransformPoseCandidateTarget,
            reinterpret_cast<void*>(&scene_transform_pose_candidate_resolve)};
        sceneTransformPoseCandidateAttached = hooking::detour::install(
            sceneTransformPoseCandidateSpec,
            g_sceneTransformPoseCandidateResolveHandle);
        if (sceneTransformPoseCandidateAttached) {
            g_sceneTransformPoseCandidateResolveOriginal.store(
                reinterpret_cast<SceneTransformPoseCandidateResolve>(
                    g_sceneTransformPoseCandidateResolveHandle.original),
                std::memory_order_release);
        }
    }
    report("ev=omega_scene_pose_binding stage=install result=%s target=+%llX "
           "upstream=+%llX filter=exact_vfx_or_scene1_active_binding_key_%08X "
           "capture=provider_owner,row,binding_key,virtual_pose_interface,"
           "returned_transform,output_binding sample_limit=%u "
           "scene2_valid_target=no mutation=%s",
           sceneTransformPoseCandidateAttached
               ? "ok"
               : sceneTransformPoseCandidatePrefix ? "attach_fail" : "prefix_mismatch",
           static_cast<unsigned long long>(kSceneTransformPoseCandidateResolveRva),
           static_cast<unsigned long long>(kSceneTransformKind2ResolveRva),
           kOmegaPurplePoseBindingKey,
           kMaximumScenePoseTargetSamples,
           kEnableOmegaPurplePoseRebind
               ? "exact_purple_pose_interface_substitute"
               : "observe_only");
    report("ev=omega_pose_provider_provenance stage=install result=%s target=+%llX "
           "downstream=+%llX filter=scene1_active_binding_key_%08X_unique_paths "
           "capture=terminal_provider,provider_50_dispatch_datum,"
           "provider_58_60_pose_datum_relative,interface_match,"
           "active_scheduler,active_event,scene1_actor_slot,returned_transform "
           "sample_limit=%u treatment=observe_only mutation=observe_only",
           sceneTransformPoseCandidateAttached
               ? "ok"
               : sceneTransformPoseCandidatePrefix ? "attach_fail" : "prefix_mismatch",
           static_cast<unsigned long long>(kSceneTransformPoseCandidateResolveRva),
           static_cast<unsigned long long>(kPoseSocketDispatchRva),
           kOmegaPurplePoseBindingKey,
           kMaximumPoseProviderProvenanceSamples);
    report("ev=omega_purple_pose_rebind stage=install result=%s enabled=%s "
           "target=+%llX cache=factory1_component_%08X_candidate_%u_selection_%08X "
           "treatment=exact_vfx_candidate_%u_selection_%08X_binding_%08X "
           "validation=current_actor,provider_owner,same_dispatch,distinct_pose,"
           "scene_runtime_delta,pose_header,finite_output fail_mode=native_fallback "
           "mutation=%s",
           sceneTransformPoseCandidateAttached
               ? "ok"
               : sceneTransformPoseCandidatePrefix ? "attach_fail" : "prefix_mismatch",
           kEnableOmegaPurplePoseRebind ? "yes" : "no",
           static_cast<unsigned long long>(kSceneTransformPoseCandidateResolveRva),
           kIkoraTransformProviderComponentDefinition,
           kOmegaSceneOnePoseCandidateIndex,
           kOmegaSceneOnePoseSelection,
           kOmegaPurplePoseCandidateIndex,
           kOmegaPurplePoseSelection,
           kOmegaPurplePoseBindingKey,
           kEnableOmegaPurplePoseRebind
               ? "pose_interface_substitute"
               : "observe_only");
    std::byte* const poseSocketDispatchTarget = image + kPoseSocketDispatchRva;
    const bool poseSocketDispatchPrefix = report_scene_type23_target(
        "pose_socket_dispatch",
        kPoseSocketDispatchRva,
        poseSocketDispatchTarget,
        kPoseSocketDispatchPrefix);
    bool poseSocketDispatchAttached = false;
    if (poseSocketDispatchPrefix) {
        const hooking::detour::Spec poseSocketDispatchSpec{
            poseSocketDispatchTarget,
            reinterpret_cast<void*>(&pose_socket_dispatch)};
        poseSocketDispatchAttached = hooking::detour::install(
            poseSocketDispatchSpec,
            g_poseSocketDispatchHandle);
        if (poseSocketDispatchAttached) {
            g_poseSocketDispatchOriginal.store(
                reinterpret_cast<PoseSocketDispatch>(
                    g_poseSocketDispatchHandle.original),
                std::memory_order_release);
        }
    }
    report("ev=omega_pose_socket_dispatch stage=install result=%s target=+%llX "
           "upstream=+%llX filter=mission_scot_scene1_active_binding_key_%08X_then_native_validation "
           "capture=caller,dispatch_target,pose_object,returned_transform,"
           "scene1_scene2_scene3_component_and_renderer_pointer_correlation,stack "
           "sample_limit=%u heartbeat_ms=%llu treatment=compare_only "
           "mutation=observe_only",
           poseSocketDispatchAttached
               ? "ok"
               : poseSocketDispatchPrefix ? "attach_fail" : "prefix_mismatch",
           static_cast<unsigned long long>(kPoseSocketDispatchRva),
           static_cast<unsigned long long>(
               kSceneTransformPoseCandidateResolveRva),
           kOmegaPurplePoseBindingKey,
           kMaximumPoseSocketDispatchSamples,
           static_cast<unsigned long long>(kPoseSocketHeartbeatMs));
#if 0 // Invalidated by the corrected runtime-RVA mapping; never install these sites again.
    std::byte* const finalModelType5Target =
        image + kFinalModelType5PartSubmitRva;
    std::byte* const finalModelOrdinaryTarget =
        image + kFinalModelOrdinaryPartSubmitRva;
    const bool finalModelType5Prefix = report_visual_recorder_target(
        "final_model_type5_part_submit",
        kFinalModelType5PartSubmitRva,
        finalModelType5Target,
        kFinalModelType5PartSubmitPrefix);
    const bool finalModelOrdinaryPrefix = report_visual_recorder_target(
        "final_model_ordinary_part_submit",
        kFinalModelOrdinaryPartSubmitRva,
        finalModelOrdinaryTarget,
        kFinalModelOrdinaryPartSubmitPrefix);
    bool finalModelType5Attached = false;
    if (finalModelType5Prefix) {
        const hooking::detour::Spec spec{
            finalModelType5Target,
            reinterpret_cast<void*>(&final_model_type5_part_submit)};
        finalModelType5Attached = hooking::detour::install(
            spec,
            g_finalModelType5PartSubmitHandle);
        if (finalModelType5Attached) {
            g_finalModelType5PartSubmitOriginal.store(
                reinterpret_cast<FinalModelPartSubmit>(
                    g_finalModelType5PartSubmitHandle.original),
                std::memory_order_release);
        }
    }
    bool finalModelOrdinaryAttached = false;
    if (finalModelOrdinaryPrefix) {
        const hooking::detour::Spec spec{
            finalModelOrdinaryTarget,
            reinterpret_cast<void*>(&final_model_ordinary_part_submit)};
        finalModelOrdinaryAttached = hooking::detour::install(
            spec,
            g_finalModelOrdinaryPartSubmitHandle);
        if (finalModelOrdinaryAttached) {
            g_finalModelOrdinaryPartSubmitOriginal.store(
                reinterpret_cast<FinalModelPartSubmit>(
                    g_finalModelOrdinaryPartSubmitHandle.original),
                std::memory_order_release);
        }
    }
    const bool finalModelRecorderReady = finalModelType5Attached
                                         && finalModelOrdinaryAttached;
    const char* const finalModelInstallResult = finalModelRecorderReady
                                                   ? "ok"
                                                   : (finalModelType5Attached
                                                      || finalModelOrdinaryAttached)
                                                         ? "partial"
                                                         : "unavailable";
    report("ev=omega_final_model_transform stage=install result=%s recorder_ready=%s "
           "targets=type5:+%llX:%s,ordinary:+%llX:%s "
           "parent=+12257A0:live_prefix_modified:not_hooked "
           "arm=exact_vfx_socket_call window_ms=%llu "
           "root=arg1_plus_0x28_then_plus_0x%llX "
           "capture=five_call_arguments,component_owners,composed_root,actor_roots,"
           "vfx_transform,renderer_correlations,raw_words sample_limit=%u "
           "candidate8_treatment=disabled mutation=observe_only",
           finalModelInstallResult,
           finalModelRecorderReady ? "yes" : "no",
           static_cast<unsigned long long>(kFinalModelType5PartSubmitRva),
           finalModelType5Attached
               ? "ok"
               : finalModelType5Prefix ? "attach_fail" : "prefix_mismatch",
           static_cast<unsigned long long>(kFinalModelOrdinaryPartSubmitRva),
           finalModelOrdinaryAttached
               ? "ok"
               : finalModelOrdinaryPrefix ? "attach_fail" : "prefix_mismatch",
           static_cast<unsigned long long>(kFinalModelTransformArmWindowMs),
           static_cast<unsigned long long>(kFinalModelTransformRootOffset),
           kMaximumFinalModelTransformSamples);
#endif
    std::byte* const effectTransformComposeTarget =
        image + kEffectTransformComposeRva;
    const bool effectTransformComposePrefix = report_visual_recorder_target(
        "effect_transform_compose",
        kEffectTransformComposeRva,
        effectTransformComposeTarget,
        kEffectTransformComposePrefix);
    bool effectTransformComposeAttached = false;
    if (effectTransformComposePrefix) {
        const hooking::detour::Spec spec{
            effectTransformComposeTarget,
            reinterpret_cast<void*>(&effect_transform_compose)};
        effectTransformComposeAttached = hooking::detour::install(
            spec,
            g_effectTransformComposeHandle);
        if (effectTransformComposeAttached) {
            g_effectTransformComposeOriginal.store(
                reinterpret_cast<EffectTransformCompose>(
                    g_effectTransformComposeHandle.original),
                std::memory_order_release);
        }
    }
    report("ev=omega_effect_transform_compose stage=install result=%s "
           "recorder_ready=%s target=+%llX callers=create:+%llX,update:+%llX "
           "arm=exact_vfx_socket_call window_ms=%llu "
           "treatment_filter=handles_80C71D8E_80C71D8A_80F1FCCA_80C71D70 "
           "treatment_offset_z=%.3f "
           "capture=effect_before,selected_input,effect_after,purple_resource_scan,"
           "actor_roots,transform_arrays,selectors,raw_head sample_limit=%u "
           "heartbeat_ms=%llu mutation=exact_effect_output_world_z_plus_10",
           effectTransformComposeAttached
               ? "ok"
               : effectTransformComposePrefix ? "attach_fail" : "prefix_mismatch",
           effectTransformComposeAttached ? "yes" : "no",
           static_cast<unsigned long long>(kEffectTransformComposeRva),
           static_cast<unsigned long long>(kEffectTransformCreateCallsiteRva),
           static_cast<unsigned long long>(kEffectTransformUpdateCallsiteRva),
           static_cast<unsigned long long>(kEffectTransformArmWindowMs),
           static_cast<double>(kOmegaPurpleDirectEffectWorldZOffset),
           kMaximumEffectTransformSamples,
           static_cast<unsigned long long>(kEffectTransformHeartbeatMs));
    std::byte* const sceneTransformKind2ProviderTarget =
        image + kSceneTransformKind2ProviderResolveRva;
    const bool sceneTransformKind2ProviderPrefix = report_scene_type23_target(
        "transform_kind2_provider_resolver",
        kSceneTransformKind2ProviderResolveRva,
        sceneTransformKind2ProviderTarget,
        kSceneTransformKind2ProviderResolvePrefix);
    bool sceneTransformKind2ProviderAttached = false;
    if (kEnableKind2ProviderAb && sceneTransformKind2ProviderPrefix) {
        const hooking::detour::Spec sceneTransformKind2ProviderSpec{
            sceneTransformKind2ProviderTarget,
            reinterpret_cast<void*>(&scene_transform_kind2_provider_resolve)};
        sceneTransformKind2ProviderAttached = hooking::detour::install(
            sceneTransformKind2ProviderSpec,
            g_sceneTransformKind2ProviderResolveHandle);
        if (sceneTransformKind2ProviderAttached) {
            g_sceneTransformKind2ProviderResolveOriginal.store(
                reinterpret_cast<SceneTransformKind2ProviderResolve>(
                    g_sceneTransformKind2ProviderResolveHandle.original),
                std::memory_order_release);
        }
    }
    report("ev=omega_purple_provider_reroute stage=install result=%s target=+%llX "
           "upstream=+%llX filter=scene_80FCCE87_kind_2_entry_1_exact_provider "
           "treatment=same_call_active_provider_candidate_7_to_8_A02A6431 "
           "validation=current_actors,active_provider_bytes,authored_definition_row,"
           "live_dispatch,live_pose_header,native_selection,finite_output "
           "fail_mode=native_fallback component_writes=none mutation=%s",
           sceneTransformKind2ProviderAttached
               ? "ok"
               : !kEnableKind2ProviderAb
                     ? "disabled"
                     : sceneTransformKind2ProviderPrefix ? "attach_fail" : "prefix_mismatch",
           static_cast<unsigned long long>(kSceneTransformKind2ProviderResolveRva),
           static_cast<unsigned long long>(kSceneTransformKind2ResolveRva),
           sceneTransformKind2ProviderAttached
               ? "same_call_candidate8_row_swap"
               : "observe_only");
    std::byte* const sceneTransformKind2Target =
        image + kSceneTransformKind2ResolveRva;
    const bool sceneTransformKind2Prefix = report_scene_type23_target(
        "transform_kind2_resolver",
        kSceneTransformKind2ResolveRva,
        sceneTransformKind2Target,
        kSceneTransformKind2ResolvePrefix);
    bool sceneTransformKind2Attached = false;
    if (sceneTransformKind2Prefix) {
        const hooking::detour::Spec sceneTransformKind2Spec{
            sceneTransformKind2Target,
            reinterpret_cast<void*>(&scene_transform_kind2_resolve)};
        sceneTransformKind2Attached = hooking::detour::install(
            sceneTransformKind2Spec, g_sceneTransformKind2ResolveHandle);
        if (sceneTransformKind2Attached) {
            g_sceneTransformKind2ResolveOriginal.store(
                reinterpret_cast<SceneTransformKind2Resolve>(
                    g_sceneTransformKind2ResolveHandle.original),
                std::memory_order_release);
        }
    }
    report("ev=omega_scene_kind2_resolver stage=install result=%s target=+%llX "
           "writer=+%llX provider_resolver=+%llX pose_candidate=+%llX "
           "filter=scene_80FCCE87_kind_2_entry_1_source_1_flags_3C_"
           "native_bank_row_1_actors_1_2_live "
           "capture=runtime_row,resolved_provider,definition_table_selection,"
           "provider_row,binding_key,pose_interface,output_50,bank_20,"
           "native_bank_row_relation,arm_rejection_reason "
           "sample_limit=%u ab_test=%s mutation=%s",
           sceneTransformKind2Attached
               ? "ok"
               : sceneTransformKind2Prefix ? "attach_fail" : "prefix_mismatch",
           static_cast<unsigned long long>(kSceneTransformKind2ResolveRva),
           static_cast<unsigned long long>(kSceneTransformSourceUpdateRva),
           static_cast<unsigned long long>(kSceneTransformKind2ProviderResolveRva),
           static_cast<unsigned long long>(kSceneTransformPoseCandidateResolveRva),
           kMaximumSceneTransformKind2Samples,
           sceneTransformKind2ProviderAttached ? "enabled" : "disabled",
           sceneTransformKind2ProviderAttached
               ? "same_call_candidate8_row_swap"
               : "observe_only");
    core::log::write(
        core::log::Channel::client,
        core::log::Level::info,
        "ev=omega_spawner_chain stage=install result=ok "
        "mode=all_scenes_native_retain_scene_two_terminal_and_latch_handoff "
        "sites=apply,deficit,request,squad_resolve,member_build,queue_find,drain,factory,"
        "object_finalize,entity_for_object,component_start,behavior_step,behavior_node,"
        "index_heap_release,scene_transition_retire");
    return true;
}

void uninstall_activity_spawner_chain_probe() noexcept {
    if (!g_installed.load(std::memory_order_acquire)) {
        return;
    }

#if 0 // Invalidated by the corrected runtime-RVA mapping.
    if (g_finalModelOrdinaryPartSubmitHandle.attached) {
        const std::array<hooking::detour::ProtectedCodeEntry, 1> protectedEntries{
            hooking::detour::ProtectedCodeEntry{
                reinterpret_cast<void*>(&final_model_ordinary_part_submit)}};
        const hooking::detour::UninstallResult result = hooking::detour::uninstall(
            g_finalModelOrdinaryPartSubmitHandle,
            protectedEntries,
            &final_model_transform_submit_idle);
        if (result != hooking::detour::UninstallResult::removed) {
            report("ev=omega_final_model_transform stage=uninstall result=deferred "
                   "site=ordinary_part_submit reason=%s mutation=observe_only",
                   result == hooking::detour::UninstallResult::protectedCodeActive
                       ? "replacement_active"
                       : "detour_failure");
            return;
        }
    }
    g_finalModelOrdinaryPartSubmitOriginal.store(nullptr, std::memory_order_release);

    if (g_finalModelType5PartSubmitHandle.attached) {
        const std::array<hooking::detour::ProtectedCodeEntry, 1> protectedEntries{
            hooking::detour::ProtectedCodeEntry{
                reinterpret_cast<void*>(&final_model_type5_part_submit)}};
        const hooking::detour::UninstallResult result = hooking::detour::uninstall(
            g_finalModelType5PartSubmitHandle,
            protectedEntries,
            &final_model_transform_submit_idle);
        if (result != hooking::detour::UninstallResult::removed) {
            report("ev=omega_final_model_transform stage=uninstall result=deferred "
                   "site=type5_part_submit reason=%s mutation=observe_only",
                   result == hooking::detour::UninstallResult::protectedCodeActive
                       ? "replacement_active"
                       : "detour_failure");
            return;
        }
    }
    g_finalModelType5PartSubmitOriginal.store(nullptr, std::memory_order_release);
#endif

    if (g_effectTransformComposeHandle.attached) {
        const std::array<hooking::detour::ProtectedCodeEntry, 1> protectedEntries{
            hooking::detour::ProtectedCodeEntry{
                reinterpret_cast<void*>(&effect_transform_compose)}};
        const hooking::detour::UninstallResult result = hooking::detour::uninstall(
            g_effectTransformComposeHandle,
            protectedEntries,
            &effect_transform_compose_idle);
        if (result != hooking::detour::UninstallResult::removed) {
            report("ev=omega_effect_transform_compose stage=uninstall result=deferred "
                   "reason=%s mutation=observe_only",
                   result == hooking::detour::UninstallResult::protectedCodeActive
                       ? "replacement_active"
                       : "detour_failure");
            return;
        }
    }
    g_effectTransformComposeOriginal.store(nullptr, std::memory_order_release);
    g_effectTransformComposeHandle = {};

    // The writer can call the kind-2 resolver synchronously. Retire it first, and keep each
    // trampoline published unless Detours confirms that no thread is executing the replacement.
    // A deferred removal leaves g_installed set so lifecycle teardown can safely retry.
    if (g_sceneTransformSourceUpdateHandle.attached) {
        const std::array<hooking::detour::ProtectedCodeEntry, 1> protectedEntries{
            hooking::detour::ProtectedCodeEntry{
                reinterpret_cast<void*>(&scene_transform_source_update)}};
        const hooking::detour::UninstallResult result = hooking::detour::uninstall(
            g_sceneTransformSourceUpdateHandle,
            protectedEntries,
            &scene_transform_source_update_idle);
        if (result != hooking::detour::UninstallResult::removed) {
            report("ev=omega_scene_kind2_resolver stage=uninstall result=deferred "
                   "site=source_update reason=%s mutation=observe_only",
                   result == hooking::detour::UninstallResult::protectedCodeActive
                       ? "replacement_active"
                       : "detour_failure");
            return;
        }
    }
    g_sceneTransformSourceUpdateOriginal.store(nullptr, std::memory_order_release);

    if (g_sceneTransformKind2ResolveHandle.attached) {
        const std::array<hooking::detour::ProtectedCodeEntry, 1> protectedEntries{
            hooking::detour::ProtectedCodeEntry{
                reinterpret_cast<void*>(&scene_transform_kind2_resolve)}};
        const hooking::detour::UninstallResult result = hooking::detour::uninstall(
            g_sceneTransformKind2ResolveHandle,
            protectedEntries,
            &scene_transform_kind2_resolve_idle);
        if (result != hooking::detour::UninstallResult::removed) {
            report("ev=omega_scene_kind2_resolver stage=uninstall result=deferred "
                   "site=kind2_resolve reason=%s mutation=observe_only",
                   result == hooking::detour::UninstallResult::protectedCodeActive
                       ? "replacement_active"
                       : "detour_failure");
            return;
        }
    }
    g_sceneTransformKind2ResolveOriginal.store(nullptr, std::memory_order_release);

    if (g_sceneTransformPoseCandidateResolveHandle.attached) {
        const std::array<hooking::detour::ProtectedCodeEntry, 1> protectedEntries{
            hooking::detour::ProtectedCodeEntry{
                reinterpret_cast<void*>(&scene_transform_pose_candidate_resolve)}};
        const hooking::detour::UninstallResult result = hooking::detour::uninstall(
            g_sceneTransformPoseCandidateResolveHandle,
            protectedEntries,
            &scene_transform_pose_candidate_resolve_idle);
        if (result != hooking::detour::UninstallResult::removed) {
            report("ev=omega_scene_pose_binding stage=uninstall result=deferred "
                   "site=pose_candidate reason=%s mutation=observe_only",
                   result == hooking::detour::UninstallResult::protectedCodeActive
                       ? "replacement_active"
                       : "detour_failure");
            return;
        }
    }
    g_sceneTransformPoseCandidateResolveOriginal.store(
        nullptr, std::memory_order_release);

    if (g_poseSocketDispatchHandle.attached) {
        const std::array<hooking::detour::ProtectedCodeEntry, 1> protectedEntries{
            hooking::detour::ProtectedCodeEntry{
                reinterpret_cast<void*>(&pose_socket_dispatch)}};
        const hooking::detour::UninstallResult result = hooking::detour::uninstall(
            g_poseSocketDispatchHandle,
            protectedEntries,
            &pose_socket_dispatch_idle);
        if (result != hooking::detour::UninstallResult::removed) {
            report("ev=omega_pose_socket_dispatch stage=uninstall result=deferred "
                   "site=pose_socket_dispatch reason=%s mutation=observe_only",
                   result == hooking::detour::UninstallResult::protectedCodeActive
                       ? "replacement_active"
                       : "detour_failure");
            return;
        }
    }
    g_poseSocketDispatchOriginal.store(nullptr, std::memory_order_release);

    if (g_sceneTransformKind2ProviderResolveHandle.attached) {
        const std::array<hooking::detour::ProtectedCodeEntry, 1> protectedEntries{
            hooking::detour::ProtectedCodeEntry{
                reinterpret_cast<void*>(&scene_transform_kind2_provider_resolve)}};
        const hooking::detour::UninstallResult result = hooking::detour::uninstall(
            g_sceneTransformKind2ProviderResolveHandle,
            protectedEntries,
            &scene_transform_kind2_provider_resolve_idle);
        if (result != hooking::detour::UninstallResult::removed) {
            report("ev=omega_purple_provider_reroute stage=uninstall result=deferred "
                   "site=provider_resolve reason=%s mutation=same_call_candidate8_row_swap",
                   result == hooking::detour::UninstallResult::protectedCodeActive
                       ? "replacement_active"
                       : "detour_failure");
            return;
        }
    }
    g_sceneTransformKind2ProviderResolveOriginal.store(nullptr, std::memory_order_release);

    if (!g_installed.exchange(false, std::memory_order_acq_rel)) {
        return;
    }
    if (g_presentationBodyStateHandle.attached) {
        (void)hooking::detour::uninstall(g_presentationBodyStateHandle);
    }
    g_presentationBodyStateHandle = {};
    g_presentationBodyStateOriginal.store(nullptr, std::memory_order_release);
    for (auto& handle : g_rendererHandles) {
        if (handle.attached) {
            (void)hooking::detour::uninstall(handle);
        }
    }
    g_rendererHandles = {};
    g_rendererBulkBooleanOriginal.store(nullptr, std::memory_order_release);
    g_rendererBulkSiblingOriginal.store(nullptr, std::memory_order_release);
    g_rendererObjectAOriginal.store(nullptr, std::memory_order_release);
    g_rendererObjectBOriginal.store(nullptr, std::memory_order_release);
    g_rendererObjectCOriginal.store(nullptr, std::memory_order_release);
    if (g_sceneTransitionRetireHandle.attached) {
        (void)hooking::detour::uninstall(g_sceneTransitionRetireHandle);
    }
    g_sceneTransitionRetireHandle = {};
    g_sceneTransitionRetireOriginal.store(nullptr, std::memory_order_release);
    if (g_sceneEntryResolverHandle.attached) {
        (void)hooking::detour::uninstall(g_sceneEntryResolverHandle);
    }
    g_sceneEntryResolverHandle = {};
    g_sceneEntryResolverOriginal.store(nullptr, std::memory_order_release);
    if (g_sceneEntryUpdateHandle.attached) {
        (void)hooking::detour::uninstall(g_sceneEntryUpdateHandle);
    }
    g_sceneEntryUpdateHandle = {};
    g_sceneEntryUpdateOriginal.store(nullptr, std::memory_order_release);
    g_sceneEntryResolverCalls.store(0U, std::memory_order_release);
    g_sceneEntryUpdateComponent = nullptr;
    g_sceneEntryUpdateIndex = 0U;
    if (g_sceneActorSchedulerHandle.attached) {
        (void)hooking::detour::uninstall(g_sceneActorSchedulerHandle);
    }
    g_sceneActorSchedulerHandle = {};
    g_sceneActorSchedulerOriginal.store(nullptr, std::memory_order_release);
    if (g_sceneType23TransformResolveHandle.attached) {
        (void)hooking::detour::uninstall(g_sceneType23TransformResolveHandle);
    }
    g_sceneType23TransformResolveHandle = {};
    g_sceneType23TransformResolveOriginal.store(nullptr, std::memory_order_release);
    if (g_sceneType23CallbackHandle.attached) {
        (void)hooking::detour::uninstall(g_sceneType23CallbackHandle);
    }
    g_sceneType23CallbackHandle = {};
    g_sceneType23CallbackOriginal.store(nullptr, std::memory_order_release);
    for (auto& handle : g_visualRecorderHandles) {
        if (handle.attached) {
            (void)hooking::detour::uninstall(handle);
        }
    }
    g_visualRecorderHandles = {};
    g_visualEntryCreateOriginal.store(nullptr, std::memory_order_release);
    g_visualTransformApplyOriginal.store(nullptr, std::memory_order_release);
    g_visualAttachmentBindOriginal.store(nullptr, std::memory_order_release);
    if (g_sceneVisualObjectCreateHandle.attached) {
        (void)hooking::detour::uninstall(g_sceneVisualObjectCreateHandle);
    }
    g_sceneVisualObjectCreateHandle = {};
    g_sceneVisualObjectCreateOriginal.store(nullptr, std::memory_order_release);
    if (g_sceneAuthoredEventDispatchHandle.attached) {
        (void)hooking::detour::uninstall(g_sceneAuthoredEventDispatchHandle);
    }
    g_sceneAuthoredEventDispatchHandle = {};
    g_sceneAuthoredEventDispatchOriginal.store(nullptr, std::memory_order_release);
    // The release slot is normally retired immediately after cinematic completion. Remove the
    // remaining hooks individually so shutdown also works after that intentional partial detach.
    for (auto& handle : g_handles) {
        if (handle.attached) {
            (void)hooking::detour::uninstall(handle);
        }
    }
    g_handles = {};
    g_applyOriginal.store(nullptr, std::memory_order_release);
    g_deficitOriginal.store(nullptr, std::memory_order_release);
    g_requestOriginal.store(nullptr, std::memory_order_release);
    g_squadResolveOriginal.store(nullptr, std::memory_order_release);
    g_memberBuildOriginal.store(nullptr, std::memory_order_release);
    g_queueFindOriginal.store(nullptr, std::memory_order_release);
    g_drainOriginal.store(nullptr, std::memory_order_release);
    g_factoryOriginal.store(nullptr, std::memory_order_release);
    g_objectFinalizeOriginal.store(nullptr, std::memory_order_release);
    g_entityForObjectOriginal.store(nullptr, std::memory_order_release);
    g_componentStartOriginal.store(nullptr, std::memory_order_release);
    g_behaviorStepOriginal.store(nullptr, std::memory_order_release);
    g_behaviorNodeOriginal.store(nullptr, std::memory_order_release);
    g_indexHeapReleaseOriginal.store(nullptr, std::memory_order_release);
    g_image.store(nullptr, std::memory_order_release);
    g_applyCalls.store(0U, std::memory_order_release);
    g_chainCalls.store(0U, std::memory_order_release);
    g_factoryCalls.store(0U, std::memory_order_release);
    g_drainCalls.store(0U, std::memory_order_release);
    g_behaviorCalls.store(0U, std::memory_order_release);
    g_behaviorMatches.store(0U, std::memory_order_release);
    g_behaviorNodeCalls.store(0U, std::memory_order_release);
    g_sceneVisualCreateCalls.store(0U, std::memory_order_release);
    g_visualEntryRecorderCalls.store(0U, std::memory_order_release);
    g_sceneAuthoredEventCalls.store(0U, std::memory_order_release);
    g_sceneAuthoredEventSamples.store(0U, std::memory_order_release);
    g_sceneAuthoredEventAttempts.store(0U, std::memory_order_release);
    g_sceneType23Calls.store(0U, std::memory_order_release);
    g_sceneType23Samples.store(0U, std::memory_order_release);
    g_sceneTransformWriterCalls.store(0U, std::memory_order_release);
    g_sceneTransformWriterSamples.store(0U, std::memory_order_release);
    g_sceneTransformKind2ResolveCalls.store(0U, std::memory_order_release);
    g_sceneTransformKind2Samples.store(0U, std::memory_order_release);
    g_sceneTransformKind2ProviderTreatmentCalls.store(0U, std::memory_order_release);
    g_sceneTransformKind2ProviderTreatmentApplied.store(0U, std::memory_order_release);
    g_sceneTransformKind2ProviderTreatmentFallbacks.store(0U, std::memory_order_release);
    g_rendererHandleSamples.store(0U, std::memory_order_release);
    g_sceneTwoPresentationComponent.store(0U, std::memory_order_release);
    g_sceneTwoPresentationObject.store(kInvalidHandle, std::memory_order_release);
    g_sceneTwoPresentationCaptures.store(0U, std::memory_order_release);
    g_sceneTwoPresentationDisableCalls.store(0U, std::memory_order_release);
    g_sceneTwoPresentationDisabledObjects.store(0U, std::memory_order_release);
    g_sceneTwoBodyStateCalls.store(0U, std::memory_order_release);
    g_sceneTwoBodyStateOverrides.store(0U, std::memory_order_release);
    g_sceneTwoBodyStateForces.store(0U, std::memory_order_release);
    g_sceneTwoModelSuppressions.store(0U, std::memory_order_release);
    for (auto& renderer : g_sceneTwoPresentationRenderers) {
        renderer.store(0U, std::memory_order_release);
    }
    AcquireSRWLockExclusive(&g_rendererTraceLock);
    g_rendererTraceKeys = {};
    g_rendererTraceKeyCount = 0U;
    ReleaseSRWLockExclusive(&g_rendererTraceLock);
    g_rendererHandleDepth = 0U;
    g_visualEntryRecorderDepth = 0U;
    g_visualEntryRecorderContext = {};
    g_sceneTransformKind2ResolveCapture = {};
    g_sceneTransformWriterLastTick.store(0U, std::memory_order_release);
    g_sceneTransformKind2LastTick.store(0U, std::memory_order_release);
    g_sceneTransformKind2LastProvider.store(0U, std::memory_order_release);
    g_sceneTransformKind2LastCandidate.store(kInvalidHandle, std::memory_order_release);
    g_sceneTransformKind2LastResult.store(INT32_MIN, std::memory_order_release);
    g_scenePoseBindingKey.store(kInvalidHandle, std::memory_order_release);
    g_scenePoseProviderTargetHandle.store(kInvalidHandle, std::memory_order_release);
    g_scenePoseProviderTargetRecord.store(0U, std::memory_order_release);
    g_scenePoseProviderTargetRelative.store(INT64_MIN, std::memory_order_release);
    g_scenePoseProviderTarget.store(0U, std::memory_order_release);
    g_scenePoseProviderTargetObservedMask.store(0U, std::memory_order_release);
    g_scenePoseProviderTargetResolvedMask.store(0U, std::memory_order_release);
    for (std::size_t index = 0U; index < g_scenePoseNaturalCalls.size(); ++index) {
        g_scenePoseNaturalCalls[index].store(0U, std::memory_order_release);
        g_scenePoseNaturalSamples[index].store(0U, std::memory_order_release);
        g_scenePoseNaturalLastTick[index].store(0U, std::memory_order_release);
    }
    g_poseSocketDispatchCalls.store(0U, std::memory_order_release);
    g_poseSocketDispatchSamples.store(0U, std::memory_order_release);
    g_poseSocketDispatchLastTick.store(0U, std::memory_order_release);
    g_poseSocketVfxDispatchBase.store(0U, std::memory_order_release);
    g_poseSocketVfxDispatchTarget.store(0U, std::memory_order_release);
    g_poseSocketVfxObject.store(0U, std::memory_order_release);
    g_poseSocketVfxLastCallTick.store(0U, std::memory_order_release);
    g_effectTransformComposeCalls.store(0U, std::memory_order_release);
    g_effectTransformComposeSamples.store(0U, std::memory_order_release);
    g_omegaPurpleDirectEffectOffsetCalls.store(0U, std::memory_order_release);
    g_omegaPurpleDirectEffectOffsetApplied.store(0U, std::memory_order_release);
    g_omegaPurpleDirectEffectOffsetSeenMask.store(0U, std::memory_order_release);
    g_omegaPurpleDirectEffectOffsetLastTick.store(0U, std::memory_order_release);
    g_poseProviderProvenanceCalls.store(0U, std::memory_order_release);
    g_poseProviderProvenanceSamples.store(0U, std::memory_order_release);
    g_omegaPurplePoseRebindCalls.store(0U, std::memory_order_release);
    g_omegaPurplePoseRebindApplied.store(0U, std::memory_order_release);
    g_omegaPurplePoseRebindFallbacks.store(0U, std::memory_order_release);
    g_omegaPurplePoseRebindLastTick.store(0U, std::memory_order_release);
    clear_scene_one_pose_binding();
    AcquireSRWLockExclusive(&g_poseSocketTraceLock);
    g_poseSocketTraceKeys = {};
    g_poseSocketTraceKeyCount = 0U;
    ReleaseSRWLockExclusive(&g_poseSocketTraceLock);
    AcquireSRWLockExclusive(&g_effectTransformTraceLock);
    g_effectTransformTraceKeys = {};
    g_effectTransformTraceKeyCount = 0U;
    ReleaseSRWLockExclusive(&g_effectTransformTraceLock);
    AcquireSRWLockExclusive(&g_vfxSocketTransformLock);
    g_vfxSocketTransform = {};
    ReleaseSRWLockExclusive(&g_vfxSocketTransformLock);
    AcquireSRWLockExclusive(&g_poseProviderProvenanceTraceLock);
    g_poseProviderProvenanceTraceKeys = {};
    g_poseProviderProvenanceTraceKeyCount = 0U;
    ReleaseSRWLockExclusive(&g_poseProviderProvenanceTraceLock);
    for (auto& pointer : g_omegaSceneSchedulerPointers) {
        pointer.store(0U, std::memory_order_release);
    }
    for (auto& pointer : g_omegaSceneEventPointers) {
        pointer.store(0U, std::memory_order_release);
    }
    g_poseSocketExactVfxDepth = 0U;
    g_sceneAuthoredEventScene = nullptr;
    g_sceneAuthoredEventIndex = -1;
    g_sceneAuthoredEventHandle = kInvalidHandle;
    g_sceneAuthoredEventRuntime = nullptr;
    g_sceneAuthoredEventAuthored = nullptr;
    g_sceneActivationObservedMask.store(0U, std::memory_order_release);
    g_sceneCastSuppressionObservedMask.store(0U, std::memory_order_release);
    g_sceneTwoPersistenceRetireCalls.store(0U, std::memory_order_release);
    g_followupLastTick.store(0U, std::memory_order_release);
    g_handoffRollingBeforeLastTick.store(0U, std::memory_order_release);
    g_handoffRollingAfterLastTick.store(0U, std::memory_order_release);
    g_handoffRollingPasses.store(0U, std::memory_order_release);
    g_omegaQueue.store(0U, std::memory_order_release);
    g_ikoraObject.store(kInvalidHandle, std::memory_order_release);
    g_ikoraEntity.store(kInvalidHandle, std::memory_order_release);
    g_ikoraNetworkRecord.store(0U, std::memory_order_release);
    g_ikoraBehaviorActor.store(0U, std::memory_order_release);
    g_ikoraHandoffBehaviorActor.store(0U, std::memory_order_release);
    g_sceneAnimatedObject.store(kInvalidHandle, std::memory_order_release);
    g_sceneCompletionPending.store(false, std::memory_order_release);
    g_indexHeapReleaseDetachPending.store(false, std::memory_order_release);
    g_indexHeapReleaseDetachBusy.clear(std::memory_order_release);
    g_componentStartInstance = 0U;
    g_componentStartDefinition = kInvalidHandle;
    g_componentStartHandlerRva = 0U;
    g_componentStartFactorySequence = 0U;
    g_componentStartObject = kInvalidHandle;
    for (auto& pointer : g_watchedComponentPointers) {
        pointer.store(0U, std::memory_order_release);
    }
    for (auto& hash : g_watchedComponentHashes) {
        hash.store(0U, std::memory_order_release);
    }
    AcquireSRWLockExclusive(&g_ikoraComponentLock);
    g_ikoraComponents = {};
    g_ikoraComponentCount = 0U;
    ReleaseSRWLockExclusive(&g_ikoraComponentLock);
    AcquireSRWLockExclusive(&g_ikoraActorLock);
    g_ikoraActors = {};
    g_ikoraActorCount = 0U;
    ReleaseSRWLockExclusive(&g_ikoraActorLock);
    AcquireSRWLockExclusive(&g_handoffSnapshotLock);
    g_handoffSnapshots = {};
    g_handoffSnapshotCount = 0U;
    ReleaseSRWLockExclusive(&g_handoffSnapshotLock);
    AcquireSRWLockExclusive(&g_sceneEventTraceStateLock);
    g_sceneEventTraceStates = {};
    g_sceneEventTraceStateCount = 0U;
    ReleaseSRWLockExclusive(&g_sceneEventTraceStateLock);
    AcquireSRWLockExclusive(&g_sceneType23TraceStateLock);
    g_sceneType23TraceStates = {};
    g_sceneType23TraceStateCount = 0U;
    ReleaseSRWLockExclusive(&g_sceneType23TraceStateLock);
}

} // namespace sunrise::client::hooks::bootflow
