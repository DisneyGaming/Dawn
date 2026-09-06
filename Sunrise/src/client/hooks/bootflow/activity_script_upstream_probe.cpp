#include <Windows.h>
#include <intrin.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string_view>

#include "../../../core/filesystem/path.h"
#include "../../../core/logging/log.h"
#include "../../../state/activity/forced/activity_forced_destination.h"
#include "../../hooking/detour.h"
#include "internal.h"
#include "legacy_owner_quarantine_lifecycle.h"
#include "legacy_owner_sentinel.h"

namespace sunrise::client::hooks::bootflow {
namespace {

constexpr std::uintptr_t kTransitionUpdateRva = 0x177AC40U;
constexpr std::uintptr_t kSlotUpdateRva = 0x17893D0U;
constexpr std::uintptr_t kModeUpdateRva = 0x1790770U;
constexpr std::uintptr_t kAuthoritySlotMapRva = 0x16FA5B0U;
constexpr std::uintptr_t kAuthorityApplyRva = 0x16F8170U;
constexpr std::uintptr_t kAuthorityRefreshRva = 0x1760800U;
constexpr std::uintptr_t kAuthorityAnyAssignedGateRva = 0x16FD530U;
constexpr std::uintptr_t kAuthorityAnyUnassignedGateRva = 0x16FD560U;
constexpr std::uintptr_t kAuthoritySnapshotGateRva = 0x16FD590U;
constexpr std::uintptr_t kAssignmentProviderUpdateRva = 0xF26D50U;
constexpr std::uintptr_t kAssignmentProviderStateConsumerRva = 0xF17EE0U;
constexpr std::uintptr_t kMatchmakingLanePolicyMaterializerRva = 0xF13370U;
constexpr std::uintptr_t kMatchmakingLaneRebuildRva = 0xF249A0U;
constexpr std::uintptr_t kAssignmentWatcherUpdateRva = 0x1747470U;
constexpr std::uintptr_t kAssignmentRevisionRva = 0x17E43C0U;
constexpr std::uintptr_t kAssignmentProviderResolveRva = 0xF25CF0U;
constexpr std::uintptr_t kAssignmentProviderPublishRva = 0xF25F70U;
constexpr std::uintptr_t kAssignmentProviderRuntimeInitializerRva = 0x116D100U;
constexpr std::uintptr_t kManagerLookupRva = 0x177A0B0U;
constexpr std::uintptr_t kManagerTableRva = 0x16C8470U;
constexpr std::uintptr_t kLifecycleEventRva = 0x176F870U;
constexpr std::uintptr_t kManagerEventBeginRva = 0x178B060U;
constexpr std::uintptr_t kManagerEnsureRva = 0x1794550U;
constexpr std::uintptr_t kIdentityRequestRva = 0x17948F0U;
constexpr std::uintptr_t kIdentityEnableRva = 0x179F340U;
constexpr std::uintptr_t kDefinitionPendingConsumerRva = 0x179FFF0U;
constexpr std::uintptr_t kManagerActivateRva = 0x177E940U;
constexpr std::uintptr_t kHostHandoffApplyRva = 0x177DF30U;
constexpr std::uintptr_t kMemberRecordIndexRva = 0x1779AD0U;
constexpr std::uintptr_t kManagerActivateMemberLookupReturnRva = 0x177E988U;
constexpr std::uintptr_t kManagerActivateTailLookupReturnRva = 0x17FA7F5U;
constexpr std::uintptr_t kIdentityDefinitionRva = 0x179AF00U;
constexpr std::uintptr_t kIdentityDescriptorCopyRva = 0x179B460U;
constexpr std::uintptr_t kSessionDescriptionStageRva = 0x17948F0U;
constexpr std::uintptr_t kManagedSessionMigrationRva = 0x1796000U;
constexpr std::uintptr_t kTransitionDispatchRva = 0x16DF310U;
constexpr std::uintptr_t kWireDispatchRva = 0x16DF360U;
constexpr std::uintptr_t kActivityClientUpdateRva = 0x16D56C0U;
constexpr std::uintptr_t kActivityReceiverLookupRva = 0x17CE430U;
constexpr std::uintptr_t kActivityReceiverIteratorRva = 0x16C3E20U;
constexpr std::uintptr_t kActivityRosterApplyRva = 0x17D3010U;
constexpr std::uintptr_t kActivityPeerDispatchRva = 0x16E0940U;
constexpr std::uintptr_t kActivityReceiverActivateRva = 0x17CC1A0U;
constexpr std::uintptr_t kActivityReceiverRootRva = 0x17CF0E0U;
constexpr std::uintptr_t kActivityReceiverBindRva = 0x17C3480U;

/** Kind-22's 144-bit online-session identity begins after ids, NetAddr, and security identity. */
constexpr std::size_t kHostReestablishOnlineIdentityOffset = 0x76U;
/** The mode-4 manager cache stores payload byte 8 at +0x1AF04. */
constexpr std::size_t kManagerOnlineIdentityOffset = 0x1AF72U;
/** Steam's online-session identity occupies 144 bits. */
constexpr std::size_t kOnlineIdentityBytes = 18U;
constexpr std::size_t kSessionDescriptionBytes = 0x80U;
/** The packed 144-bit online identity occupies the final 18 bytes of a session description. */
constexpr std::size_t kSessionDescriptionOnlineIdentityOffset = 0x6EU;
constexpr std::uintptr_t kActivityReceiverCreatorRva = 0x17B8C50U;
constexpr std::uintptr_t kManagerUpdateLoopRva = 0x1764EC0U;
constexpr std::uintptr_t kManagerUpdateGateRva = 0x17510A0U;
constexpr std::uintptr_t kLaunchProducerRva = 0x1763B20U;
constexpr std::uintptr_t kLaunchProducerGuardSetterRva = 0x1764C50U;
constexpr std::uintptr_t kManagerModeSetRva = 0x17B3600U;
constexpr std::uintptr_t kRouteDescriptorLookupRva = 0x1751370U;
constexpr std::uintptr_t kRouteCommitRva = 0x1751F50U;
constexpr std::uintptr_t kRouteUpdateRva = 0x17AC390U;
constexpr std::uintptr_t kDefaultRouteZeroInitializerRva = 0x175B440U;
// The parent at 0xEE8390 evaluates these four gates immediately before it chooses
// the authored alternate branch or falls through to the route-zero initializer.
constexpr std::uintptr_t kDefaultRouteReferencePredicateRva = 0x175A070U;
constexpr std::uintptr_t kDefaultRoutePairPredicateRva = 0xC260E0U;
constexpr std::uintptr_t kDefaultRouteRequiredPredicateRva = 0xE2D130U;
constexpr std::uintptr_t kDefaultRouteCrossManagerPredicateRva = 0xC28320U;
constexpr std::uintptr_t kDefaultRouteReferencePredicateReturnRva = 0xEE83D6U;
constexpr std::uintptr_t kDefaultRoutePairPredicateReturnRva = 0xEE83E4U;
constexpr std::uintptr_t kDefaultRouteRequiredPredicateReturnRva = 0xEE83EFU;
constexpr std::uintptr_t kDefaultRouteCrossManagerPredicateReturnRva = 0xEE843DU;
constexpr std::uintptr_t kLaneOneReadyScanRva = 0x178D3F0U;
constexpr std::uintptr_t kLaneOneActiveCountRva = 0x175C290U;
constexpr std::uintptr_t kLaneOneSelectionReadyRva = 0x17D1220U;
constexpr std::uintptr_t kLaneOneAuthoredInitializerRva = 0x175C2A0U;
constexpr std::uintptr_t kRouteFlagUpdateRva = 0xC1CFD0U;
constexpr std::uintptr_t kRouteElapsedTimeRva = 0x3CC890U;
constexpr std::uintptr_t kRouteTimeoutConfigRva = 0x5A2B20U;
constexpr std::uintptr_t kRouteAlternateUpdateRva = 0xC1AD00U;
constexpr std::uintptr_t kRouteStateEvaluatorRva = 0xC1C330U;
constexpr std::uintptr_t kRouteParentDispatchRva = 0xC1BBF0U;
constexpr std::uintptr_t kRouteSelectorContextAccessorRva = 0xF17C70U;
constexpr std::uintptr_t kRouteStateArmRva = 0xC12180U;
constexpr std::uintptr_t kEmbeddedRouteStatePhaseSetRva = 0xC25790U;
constexpr std::uintptr_t kEmbeddedRouteStateInitializeRva = 0xC25800U;
constexpr std::uintptr_t kEmbeddedRouteStateResetRva = 0xBF8B30U;
constexpr std::uintptr_t kEmbeddedRouteObjectTransitionRva = 0xE1AE70U;
constexpr std::uintptr_t kEmbeddedRouteLaneZeroDriverRva = 0xEF1D90U;
constexpr std::uintptr_t kEmbeddedRouteTransitionHandlerRva = 0xEE5A80U;
constexpr std::uintptr_t kEmbeddedRouteStateStatusRva = 0xC256E0U;
constexpr std::uintptr_t kEmbeddedRouteContextSkipPredicateRva = 0xEF0690U;
constexpr std::uintptr_t kEmbeddedRouteLaneAvailablePredicateRva = 0xC26A20U;
constexpr std::uintptr_t kEmbeddedRouteAuthoredIdentityPredicateRva = 0xDE0340U;
constexpr std::uintptr_t kEmbeddedRouteLocalIdentityPredicateRva = 0xC25200U;
constexpr std::uintptr_t kEmbeddedRouteIdentityProviderRecordLookupRva = 0xDDF130U;
constexpr std::uintptr_t kEmbeddedRouteIdentityProviderServicePointerRva = 0x2742FB0U;
constexpr std::uintptr_t kEmbeddedRouteIdentityProviderRecordLookupReturnRva = 0xC25219U;
constexpr std::uintptr_t kEmbeddedRouteIdentityProviderLookupReturnRva = 0xDDF15DU;
constexpr std::size_t kEmbeddedRouteIdentityProviderLookupVtableOffset = 0x460U;
constexpr std::size_t kEmbeddedRouteIdentityProviderRelativeRecordOffset = 0x20U;
/** Reunion is the source activity retained by the native launch object. */
constexpr std::uint16_t kReunionSourceIdentifier = 282U;
/** Towerfall/Homecoming is the destination activity whose provider record the lane needs. */
constexpr std::uint16_t kHomecomingDestinationIdentifier = 266U;
constexpr std::size_t kEmbeddedRouteStateStride = 0x12A8U;
constexpr std::uintptr_t kRouteAlternateSourceRva = 0x16D99F0U;
constexpr std::uintptr_t kRouteAlternatePredicateRva = 0x16C3220U;
constexpr std::uintptr_t kRouteAlternateModeRva = 0x1A84DD0U;
constexpr std::uintptr_t kRouteSelectionIngestRva = 0x175B8F0U;
constexpr std::uintptr_t kAuthoredLaunchDispatchRva = 0x134FDF0U;
constexpr std::uintptr_t kLaunchCommandInitializeRva = 0xFA8070U;
constexpr std::uintptr_t kLaunchCommandDispatchRva = 0xFA9860U;
constexpr std::uintptr_t kLaunchProducerGuardRva = 0x31DC431U;
constexpr std::uintptr_t kLaunchProducerDispatchReturnRva = 0x1763D04U;
constexpr std::uintptr_t kLaunchProducerWorldActivityRva = 0xE2D510U;
constexpr std::uintptr_t kLaunchProducerSessionActivityRva = 0xE2D440U;
constexpr std::uintptr_t kLaunchProducerReadyRva = 0x4CDEB0U;
constexpr std::uintptr_t kLaunchProducerReadyCallbackRva = 0x4CDED0U;
constexpr std::uintptr_t kActivityEventDispatchLocalRva = 0x4E1600U;
constexpr std::uintptr_t kActivityEventDispatchPayloadRva = 0x4E16A0U;
constexpr std::uintptr_t kActivityEventRegistryQueueRva = 0x4E1500U;
constexpr std::uintptr_t kActivityEventQueueDrainCallerRva = 0x16D1D00U;
constexpr std::uintptr_t kActivityEvent46RegisteredProducerRva = 0x43DDD0U;
constexpr std::uintptr_t kActivityEvent46ProducerResetRva = 0x43DD80U;
constexpr std::uintptr_t kActivityEvent46ProducerInitializeRva = 0x43DEA0U;
constexpr std::uintptr_t kActivityEvent46ProducerAvailableRva = 0x43DD50U;
constexpr std::uintptr_t kActivityEvent46ProducerRequestRva = 0x43DFA0U;
constexpr std::uintptr_t kActivityEvent46ProducerTickRva = 0x43E060U;
constexpr std::uintptr_t kActivityEvent46RequestFlowRva = 0x1753B50U;
constexpr std::uintptr_t kActivityEvent46ActionDispatchRva = 0x16A6BD0U;
constexpr std::uintptr_t kActivityEvent46RequestPredicateRva = 0x16B22D0U;
constexpr std::uintptr_t kActivityWorldContextRva = 0xBE1E20U;
constexpr std::uintptr_t kActivityEvent46PendingPayloadRva = 0x26C4A60U;
constexpr std::uintptr_t kActivityEvent46ProducerRequestActiveRva = 0x26C4A70U;
constexpr std::uintptr_t kActivityEvent46ProviderStorageRva = 0x1F8E020U;
constexpr std::uintptr_t kActivityEvent46ProviderAccessorSlotRva = 0x20BB500U;
constexpr std::uintptr_t kLaunchProducerReadyByteRva = 0x2735DA1U;
constexpr std::uintptr_t kLaunchProducerPhaseRva = 0x35DC50U;
constexpr std::uint32_t kLaunchProducerWindowBudget = 4096U;
constexpr std::uintptr_t kRouteModeZeroQueryRva = 0x1759910U;
constexpr std::uintptr_t kRouteModeOneQueryRva = 0x1755AB0U;
constexpr std::uintptr_t kRouteModeZeroSetRva = 0x1756BE0U;
constexpr std::uintptr_t kRouteModeOneSetRva = 0x1756F40U;
constexpr std::uintptr_t kManagerSelectionPumpRva = 0x175E520U;
constexpr std::uintptr_t kLaneManagerAccessorRva = 0xC26490U;
constexpr std::uintptr_t kOptionalHasValueRva = 0x178DAB0U;
constexpr std::size_t kManagerUpstreamSelectionOffset = 0x18DD0U;
constexpr std::size_t kManagerCurrentSelectionOffset = 0x19068U;
constexpr std::size_t kOptionalValueVtableOffset = 0x98U;
constexpr std::size_t kOptionalPublishVtableOffset = 0xB0U;
constexpr std::size_t kOptionalUpdateVtableOffset = 0xB8U;
constexpr std::uintptr_t kRouteStateQueryRva = 0xC06EA0U;
constexpr std::uintptr_t kRouteStatePublishRva = 0x1758800U;
// Omega route 38 (identifier 32000) calls these three runtime-decrypted helpers from its
// method-30 tick.  The final helper receives the embedded state at object +0x40 and is the
// narrowest native decision point between route-mode activation and lane publication.
constexpr std::uintptr_t kOmegaRoute38PrepareRva = 0xD4D200U;
constexpr std::uintptr_t kOmegaRoute38ModeApplyRva = 0xD4D360U;
constexpr std::uintptr_t kOmegaRoute38EmbeddedDriverRva = 0xD4B080U;
constexpr std::uintptr_t kOmegaRouteLifecycleQueryRva = 0x4FFD10U;
constexpr std::uintptr_t kOmegaRouteLifecycleAccessorRva = 0x4FF9F0U;
constexpr std::uintptr_t kOmegaRouteMethod30LifecycleReturnRva = 0xD4B958U;
constexpr std::uintptr_t kOmegaRouteModeApplyLifecycleReturnRva = 0xD4D372U;
constexpr std::uintptr_t kManagerLocalDispatchRva = 0x17723C0U;
constexpr std::uintptr_t kManagerLocalStartRva = 0x1772440U;
constexpr std::uintptr_t kManagerAuthoredStartRva = 0x1773200U;
constexpr std::uintptr_t kManagerModeSixThunkRva = 0x17B2B80U;
constexpr std::uintptr_t kManagerSetupStageARva = 0x1763FC0U;
constexpr std::uintptr_t kManagerSetupStageBRva = 0x1765490U;
constexpr std::uintptr_t kManagerSetupStageCRva = 0x17663E0U;
constexpr std::uintptr_t kManagerSetupStageDRva = 0x1765EF0U;
constexpr std::uintptr_t kManagerSetupStageACallerRva = 0x1764F77U;
constexpr std::uintptr_t kManagerSetupStageBCallerRva = 0x1764F7FU;
constexpr std::uintptr_t kManagerSetupStageCCallerRva = 0x1764F87U;
constexpr std::uintptr_t kManagerSetupStageDCallerRva = 0x1764F8FU;
constexpr std::uintptr_t kComponentDispatchRva = 0x1766A30U;
constexpr std::uintptr_t kComponentRegisterRva = 0x17664C0U;
constexpr std::uintptr_t kComponentLookupRva = 0x1775810U;
constexpr std::uintptr_t kComponentReuseRva = 0x17A4CE0U;
constexpr std::uintptr_t kComponentMappingRva = 0x1779F60U;
constexpr std::uintptr_t kComponentBuildRva = 0x17A6040U;
constexpr std::uintptr_t kComponentStepRva = 0x17A5FB0U;
constexpr std::uintptr_t kComponentTickRva = 0x17A6440U;
constexpr std::uintptr_t kComponentTransitionRequestSendRva = 0x17C48F0U;
constexpr std::uintptr_t kPostComponentSyncRva = 0x1761D80U;
constexpr std::uintptr_t kActivityStateTableRva = 0x174F4D0U;
constexpr std::uintptr_t kManagerSnapshotRva = 0x177A240U;
constexpr std::uintptr_t kManagerGenerationRva = 0x177A270U;
constexpr std::uintptr_t kSnapshotFirstEntryRva = 0x1779270U;

constexpr std::array<std::byte, 22> kTransitionUpdatePrefix{
    std::byte{0x40}, std::byte{0x55}, std::byte{0x53}, std::byte{0x56}, std::byte{0x57},
    std::byte{0x41}, std::byte{0x56}, std::byte{0x48}, std::byte{0x8D}, std::byte{0xAC},
    std::byte{0x24}, std::byte{0xA0}, std::byte{0xFD}, std::byte{0xFF}, std::byte{0xFF},
    std::byte{0x48}, std::byte{0x81}, std::byte{0xEC}, std::byte{0x60}, std::byte{0x03},
    std::byte{0x00}, std::byte{0x00}};
constexpr std::array<std::byte, 28> kSlotUpdatePrefix{
    std::byte{0x40}, std::byte{0x55}, std::byte{0x53}, std::byte{0x56}, std::byte{0x57},
    std::byte{0x41}, std::byte{0x54}, std::byte{0x41}, std::byte{0x55}, std::byte{0x41},
    std::byte{0x56}, std::byte{0x41}, std::byte{0x57}, std::byte{0x48}, std::byte{0x8D},
    std::byte{0xAC}, std::byte{0x24}, std::byte{0xA8}, std::byte{0xFD}, std::byte{0xFF},
    std::byte{0xFF}, std::byte{0x48}, std::byte{0x81}, std::byte{0xEC}, std::byte{0x58},
    std::byte{0x03}, std::byte{0x00}, std::byte{0x00}};
constexpr std::array<std::byte, 24> kModeUpdatePrefix{
    std::byte{0x40}, std::byte{0x55}, std::byte{0x53}, std::byte{0x56}, std::byte{0x57},
    std::byte{0x41}, std::byte{0x56}, std::byte{0x41}, std::byte{0x57}, std::byte{0x48},
    std::byte{0x8D}, std::byte{0xAC}, std::byte{0x24}, std::byte{0x18}, std::byte{0xFD},
    std::byte{0xFF}, std::byte{0xFF}, std::byte{0x48}, std::byte{0x81}, std::byte{0xEC},
    std::byte{0xE8}, std::byte{0x03}, std::byte{0x00}, std::byte{0x00}};
constexpr std::array<std::byte, 24> kAuthoritySlotMapPrefix{
    std::byte{0x40}, std::byte{0x55}, std::byte{0x53}, std::byte{0x56}, std::byte{0x57},
    std::byte{0x41}, std::byte{0x56}, std::byte{0x41}, std::byte{0x57}, std::byte{0x48},
    std::byte{0x8D}, std::byte{0xAC}, std::byte{0x24}, std::byte{0x68}, std::byte{0xF9},
    std::byte{0xFF}, std::byte{0xFF}, std::byte{0x48}, std::byte{0x81}, std::byte{0xEC},
    std::byte{0x98}, std::byte{0x07}, std::byte{0x00}, std::byte{0x00}};
constexpr std::array<std::byte, 11> kAuthorityApplyPrefix{
    std::byte{0x40}, std::byte{0x56}, std::byte{0x41}, std::byte{0x57},
    std::byte{0x48}, std::byte{0x81}, std::byte{0xEC}, std::byte{0xC8},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}};
constexpr std::array<std::byte, 15> kAuthorityRefreshPrefix{
    std::byte{0x40}, std::byte{0x55}, std::byte{0x53}, std::byte{0x41},
    std::byte{0x54}, std::byte{0x41}, std::byte{0x57}, std::byte{0x48},
    std::byte{0x8D}, std::byte{0xAC}, std::byte{0x24}, std::byte{0x88},
    std::byte{0xFD}, std::byte{0xFF}, std::byte{0xFF}};
constexpr std::array<std::byte, 16> kAuthorityAnyAssignedGatePrefix{
    std::byte{0x44}, std::byte{0x8B}, std::byte{0x09}, std::byte{0x33},
    std::byte{0xD2}, std::byte{0x45}, std::byte{0x85}, std::byte{0xC9},
    std::byte{0x7E}, std::byte{0x1B}, std::byte{0x66}, std::byte{0x0F},
    std::byte{0x1F}, std::byte{0x44}, std::byte{0x00}, std::byte{0x00}};
constexpr std::array<std::byte, 16> kAuthorityAnyUnassignedGatePrefix{
    std::byte{0x44}, std::byte{0x8B}, std::byte{0x09}, std::byte{0x33},
    std::byte{0xD2}, std::byte{0x45}, std::byte{0x85}, std::byte{0xC9},
    std::byte{0x7E}, std::byte{0x1B}, std::byte{0x66}, std::byte{0x0F},
    std::byte{0x1F}, std::byte{0x44}, std::byte{0x00}, std::byte{0x00}};
constexpr std::array<std::byte, 16> kAuthoritySnapshotGatePrefix{
    std::byte{0x40}, std::byte{0x53}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x20}, std::byte{0x80}, std::byte{0xB9},
    std::byte{0x2B}, std::byte{0x02}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x48}, std::byte{0x8B}, std::byte{0xDA}};
constexpr std::array<std::byte, 5> kAssignmentProviderUpdatePrefix{
    std::byte{0xE9}, std::byte{0x23}, std::byte{0x4D}, std::byte{0x17}, std::byte{0x03}};
constexpr std::array<std::byte, 20> kAssignmentProviderStateConsumerPrefix{
    std::byte{0x4C}, std::byte{0x8B}, std::byte{0xDC}, std::byte{0x55},
    std::byte{0x41}, std::byte{0x57}, std::byte{0x49}, std::byte{0x8D},
    std::byte{0xAB}, std::byte{0x38}, std::byte{0xFD}, std::byte{0xFF},
    std::byte{0xFF}, std::byte{0x48}, std::byte{0x81}, std::byte{0xEC},
    std::byte{0xB8}, std::byte{0x03}, std::byte{0x00}, std::byte{0x00}};
constexpr std::array<std::byte, 12> kMatchmakingLanePolicyMaterializerPrefix{
    std::byte{0x40}, std::byte{0x53}, std::byte{0x56}, std::byte{0x57},
    std::byte{0x48}, std::byte{0x81}, std::byte{0xEC}, std::byte{0xF0},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x48}};
constexpr std::array<std::byte, 24> kMatchmakingLaneRebuildPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x10}, std::byte{0x48}, std::byte{0x89}, std::byte{0x74},
    std::byte{0x24}, std::byte{0x18}, std::byte{0x48}, std::byte{0x89},
    std::byte{0x7C}, std::byte{0x24}, std::byte{0x20}, std::byte{0x55},
    std::byte{0x41}, std::byte{0x54}, std::byte{0x41}, std::byte{0x55},
    std::byte{0x41}, std::byte{0x56}, std::byte{0x41}, std::byte{0x57}};
constexpr std::array<std::byte, 22> kAssignmentWatcherUpdatePrefix{
    std::byte{0x4C}, std::byte{0x8B}, std::byte{0xDC}, std::byte{0x55},
    std::byte{0x56}, std::byte{0x41}, std::byte{0x56}, std::byte{0x49},
    std::byte{0x8D}, std::byte{0xAB}, std::byte{0x68}, std::byte{0xFD},
    std::byte{0xFF}, std::byte{0xFF}, std::byte{0x48}, std::byte{0x81},
    std::byte{0xEC}, std::byte{0x80}, std::byte{0x03}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x48}};
constexpr std::array<std::byte, 20> kManagerLookupPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x08}, std::byte{0x48}, std::byte{0x89}, std::byte{0x74},
    std::byte{0x24}, std::byte{0x10}, std::byte{0x48}, std::byte{0x89},
    std::byte{0x7C}, std::byte{0x24}, std::byte{0x18}, std::byte{0x41},
    std::byte{0x56}, std::byte{0x48}, std::byte{0x83}, std::byte{0xEC}};
constexpr std::array<std::byte, 24> kManagerEnsurePrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x74}, std::byte{0x24},
    std::byte{0x20}, std::byte{0x55}, std::byte{0x57}, std::byte{0x41},
    std::byte{0x57}, std::byte{0x48}, std::byte{0x8D}, std::byte{0xAC},
    std::byte{0x24}, std::byte{0xB0}, std::byte{0xFD}, std::byte{0xFF},
    std::byte{0xFF}, std::byte{0x48}, std::byte{0x81}, std::byte{0xEC},
    std::byte{0x50}, std::byte{0x03}, std::byte{0x00}, std::byte{0x00}};
constexpr std::array<std::byte, 20> kIdentityRequestPrefix{
    std::byte{0x40}, std::byte{0x55}, std::byte{0x53}, std::byte{0x56},
    std::byte{0x57}, std::byte{0x48}, std::byte{0x8D}, std::byte{0xAC},
    std::byte{0x24}, std::byte{0x38}, std::byte{0xFD}, std::byte{0xFF},
    std::byte{0xFF}, std::byte{0x48}, std::byte{0x81}, std::byte{0xEC},
    std::byte{0xC8}, std::byte{0x03}, std::byte{0x00}, std::byte{0x00}};
constexpr std::array<std::byte, 24> kDefinitionPendingConsumerPrefix{
    std::byte{0x4C}, std::byte{0x8B}, std::byte{0xDC}, std::byte{0x49},
    std::byte{0x89}, std::byte{0x73}, std::byte{0x18}, std::byte{0x49},
    std::byte{0x89}, std::byte{0x7B}, std::byte{0x20}, std::byte{0x55},
    std::byte{0x49}, std::byte{0x8D}, std::byte{0xAB}, std::byte{0xB8},
    std::byte{0xFD}, std::byte{0xFF}, std::byte{0xFF}, std::byte{0x48},
    std::byte{0x81}, std::byte{0xEC}, std::byte{0x40}, std::byte{0x03}};
constexpr std::array<std::byte, 8> kLifecycleEventPrefix{
    std::byte{0x40}, std::byte{0x53}, std::byte{0x55}, std::byte{0x56},
    std::byte{0x48}, std::byte{0x83}, std::byte{0xEC}, std::byte{0x50}};
constexpr std::array<std::byte, 18> kManagerEventBeginPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x18}, std::byte{0x48}, std::byte{0x89}, std::byte{0x74},
    std::byte{0x24}, std::byte{0x20}, std::byte{0x55}, std::byte{0x57},
    std::byte{0x41}, std::byte{0x54}, std::byte{0x41}, std::byte{0x56},
    std::byte{0x41}, std::byte{0x57}};
constexpr std::array<std::byte, 18> kIdentityEnablePrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x10}, std::byte{0x48}, std::byte{0x89}, std::byte{0x74},
    std::byte{0x24}, std::byte{0x18}, std::byte{0x57}, std::byte{0x48},
    std::byte{0x81}, std::byte{0xEC}, std::byte{0x00}, std::byte{0x03},
    std::byte{0x00}, std::byte{0x00}};
constexpr std::array<std::byte, 24> kManagerActivatePrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x18}, std::byte{0x55}, std::byte{0x56}, std::byte{0x57},
    std::byte{0x41}, std::byte{0x54}, std::byte{0x41}, std::byte{0x55},
    std::byte{0x41}, std::byte{0x56}, std::byte{0x41}, std::byte{0x57},
    std::byte{0x48}, std::byte{0x8D}, std::byte{0xAC}, std::byte{0x24},
    std::byte{0x40}, std::byte{0xFD}, std::byte{0xFF}, std::byte{0xFF}};
constexpr std::array<std::byte, 25> kHostHandoffApplyPrefix{
    std::byte{0x40}, std::byte{0x55}, std::byte{0x53}, std::byte{0x57},
    std::byte{0x41}, std::byte{0x54}, std::byte{0x41}, std::byte{0x56},
    std::byte{0x41}, std::byte{0x57}, std::byte{0x48}, std::byte{0x8D},
    std::byte{0xAC}, std::byte{0x24}, std::byte{0x48}, std::byte{0xFD},
    std::byte{0xFF}, std::byte{0xFF}, std::byte{0x48}, std::byte{0x81},
    std::byte{0xEC}, std::byte{0xB8}, std::byte{0x03}, std::byte{0x00},
    std::byte{0x00}};
constexpr std::array<std::byte, 16> kMemberRecordIndexPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x18}, std::byte{0x57}, std::byte{0x48}, std::byte{0x81},
    std::byte{0xEC}, std::byte{0x90}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x48}, std::byte{0x8B}, std::byte{0x05}};
constexpr std::array<std::byte, 10> kIdentityDefinitionPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x08}, std::byte{0x57}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x20}};
constexpr std::array<std::byte, 15> kIdentityDescriptorCopyPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x08}, std::byte{0x48}, std::byte{0x89}, std::byte{0x74},
    std::byte{0x24}, std::byte{0x10}, std::byte{0x57}, std::byte{0x48},
    std::byte{0x83}, std::byte{0xEC}, std::byte{0x20}};
constexpr std::array<std::byte, 20> kSessionDescriptionStagePrefix{
    std::byte{0x40}, std::byte{0x55}, std::byte{0x53}, std::byte{0x56},
    std::byte{0x57}, std::byte{0x48}, std::byte{0x8D}, std::byte{0xAC},
    std::byte{0x24}, std::byte{0x38}, std::byte{0xFD}, std::byte{0xFF},
    std::byte{0xFF}, std::byte{0x48}, std::byte{0x81}, std::byte{0xEC},
    std::byte{0xC8}, std::byte{0x03}, std::byte{0x00}, std::byte{0x00}};
constexpr std::array<std::byte, 23> kManagedSessionMigrationPrefix{
    std::byte{0x40}, std::byte{0x55}, std::byte{0x53}, std::byte{0x56},
    std::byte{0x41}, std::byte{0x56}, std::byte{0x41}, std::byte{0x57},
    std::byte{0x48}, std::byte{0x8D}, std::byte{0xAC}, std::byte{0x24},
    std::byte{0x80}, std::byte{0xFC}, std::byte{0xFF}, std::byte{0xFF},
    std::byte{0x48}, std::byte{0x81}, std::byte{0xEC}, std::byte{0x80},
    std::byte{0x04}, std::byte{0x00}, std::byte{0x00}};
constexpr std::array<std::byte, 14> kTransitionDispatchPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x08}, std::byte{0x57}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x20}, std::byte{0x48}, std::byte{0x8B},
    std::byte{0x49}, std::byte{0x28}};
constexpr std::array<std::byte, 19> kWireDispatchPrefix{
    std::byte{0x40}, std::byte{0x55}, std::byte{0x53}, std::byte{0x57},
    std::byte{0x48}, std::byte{0x8D}, std::byte{0xAC}, std::byte{0x24},
    std::byte{0x70}, std::byte{0xFE}, std::byte{0xFF}, std::byte{0xFF},
    std::byte{0x48}, std::byte{0x81}, std::byte{0xEC}, std::byte{0x90},
    std::byte{0x02}, std::byte{0x00}, std::byte{0x00}};
constexpr std::array<std::byte, 28> kActivityClientUpdatePrefix{
    std::byte{0x40}, std::byte{0x55}, std::byte{0x53}, std::byte{0x56},
    std::byte{0x57}, std::byte{0x41}, std::byte{0x54}, std::byte{0x41},
    std::byte{0x55}, std::byte{0x41}, std::byte{0x56}, std::byte{0x41},
    std::byte{0x57}, std::byte{0x48}, std::byte{0x8D}, std::byte{0xAC},
    std::byte{0x24}, std::byte{0x38}, std::byte{0xF8}, std::byte{0xFF},
    std::byte{0xFF}, std::byte{0x48}, std::byte{0x81}, std::byte{0xEC},
    std::byte{0xC8}, std::byte{0x08}, std::byte{0x00}, std::byte{0x00}};
constexpr std::array<std::byte, 10> kActivityReceiverLookupPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x74}, std::byte{0x24}, std::byte{0x18},
    std::byte{0x57}, std::byte{0x48}, std::byte{0x83}, std::byte{0xEC}, std::byte{0x40}};
constexpr std::array<std::byte, 10> kActivityReceiverIteratorPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24}, std::byte{0x08},
    std::byte{0x57}, std::byte{0x48}, std::byte{0x83}, std::byte{0xEC}, std::byte{0x20}};
constexpr std::array<std::byte, 16> kActivityRosterApplyPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x20}, std::byte{0x55}, std::byte{0x56}, std::byte{0x57},
    std::byte{0x41}, std::byte{0x54}, std::byte{0x41}, std::byte{0x55},
    std::byte{0x41}, std::byte{0x56}, std::byte{0x41}, std::byte{0x57}};
constexpr std::array<std::byte, 16> kActivityPeerDispatchPrefix{
    std::byte{0x4C}, std::byte{0x8B}, std::byte{0xD2}, std::byte{0x41},
    std::byte{0x83}, std::byte{0xF9}, std::byte{0x2A}, std::byte{0x0F},
    std::byte{0x87}, std::byte{0xF3}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x48}, std::byte{0x8D}, std::byte{0x15}};
constexpr std::array<std::byte, 16> kActivityReceiverActivatePrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x20}, std::byte{0x55}, std::byte{0x56}, std::byte{0x57},
    std::byte{0x41}, std::byte{0x54}, std::byte{0x41}, std::byte{0x55},
    std::byte{0x41}, std::byte{0x56}, std::byte{0x41}, std::byte{0x57}};
constexpr std::array<std::byte, 9> kActivityReceiverRootPrefix{
    std::byte{0x40}, std::byte{0x53}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x20}, std::byte{0x48}, std::byte{0x8B},
    std::byte{0x1D}};
constexpr std::array<std::byte, 20> kActivityReceiverBindPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x08}, std::byte{0x48}, std::byte{0x89}, std::byte{0x6C},
    std::byte{0x24}, std::byte{0x10}, std::byte{0x48}, std::byte{0x89},
    std::byte{0x74}, std::byte{0x24}, std::byte{0x18}, std::byte{0x57},
    std::byte{0x48}, std::byte{0x83}, std::byte{0xEC}, std::byte{0x30}};
constexpr std::array<std::byte, 24> kActivityReceiverCreatorPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x10}, std::byte{0x48}, std::byte{0x89}, std::byte{0x74},
    std::byte{0x24}, std::byte{0x18}, std::byte{0x48}, std::byte{0x89},
    std::byte{0x7C}, std::byte{0x24}, std::byte{0x20}, std::byte{0x55},
    std::byte{0x41}, std::byte{0x54}, std::byte{0x41}, std::byte{0x55},
    std::byte{0x41}, std::byte{0x56}, std::byte{0x41}, std::byte{0x57}};
constexpr std::array<std::byte, 16> kManagerUpdateLoopPrefix{
    std::byte{0x40}, std::byte{0x55}, std::byte{0x56}, std::byte{0x48},
    std::byte{0x83}, std::byte{0xEC}, std::byte{0x78}, std::byte{0x8B},
    std::byte{0x81}, std::byte{0x50}, std::byte{0x08}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x48}, std::byte{0x8B}, std::byte{0xF1}};
constexpr std::array<std::byte, 16> kLaunchProducerPrefix{
    std::byte{0x4C}, std::byte{0x8B}, std::byte{0xDC}, std::byte{0x55},
    std::byte{0x49}, std::byte{0x8D}, std::byte{0xAB}, std::byte{0x98},
    std::byte{0xFC}, std::byte{0xFF}, std::byte{0xFF}, std::byte{0x48},
    std::byte{0x81}, std::byte{0xEC}, std::byte{0x60}, std::byte{0x04}};
constexpr std::array<std::byte, 7> kLaunchProducerGuardSetterPrefix{
    std::byte{0x88}, std::byte{0x0D}, std::byte{0xDB}, std::byte{0x77},
    std::byte{0xA7}, std::byte{0x01}, std::byte{0xC3}};
constexpr std::array<std::byte, 21> kManagerModeSetPrefix{
    std::byte{0x4C}, std::byte{0x8B}, std::byte{0xDC}, std::byte{0x55},
    std::byte{0x56}, std::byte{0x41}, std::byte{0x56}, std::byte{0x49},
    std::byte{0x8D}, std::byte{0xAB}, std::byte{0x98}, std::byte{0xFD},
    std::byte{0xFF}, std::byte{0xFF}, std::byte{0x48}, std::byte{0x81},
    std::byte{0xEC}, std::byte{0x50}, std::byte{0x03}, std::byte{0x00},
    std::byte{0x00}};
constexpr std::array<std::byte, 21> kRouteDescriptorLookupPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x08}, std::byte{0x48}, std::byte{0x89}, std::byte{0x6C},
    std::byte{0x24}, std::byte{0x18}, std::byte{0x48}, std::byte{0x89},
    std::byte{0x74}, std::byte{0x24}, std::byte{0x20}, std::byte{0x57},
    std::byte{0x41}, std::byte{0x56}, std::byte{0x41}, std::byte{0x57},
    std::byte{0x48}};
constexpr std::array<std::byte, 20> kRouteCommitPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x08}, std::byte{0x48}, std::byte{0x89}, std::byte{0x6C},
    std::byte{0x24}, std::byte{0x10}, std::byte{0x48}, std::byte{0x89},
    std::byte{0x74}, std::byte{0x24}, std::byte{0x18}, std::byte{0x48},
    std::byte{0x89}, std::byte{0x7C}, std::byte{0x24}, std::byte{0x20}};
constexpr std::array<std::byte, 20> kDefaultRouteZeroInitializerPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x10}, std::byte{0x48}, std::byte{0x89}, std::byte{0x6C},
    std::byte{0x24}, std::byte{0x18}, std::byte{0x48}, std::byte{0x89},
    std::byte{0x74}, std::byte{0x24}, std::byte{0x20}, std::byte{0x57},
    std::byte{0x48}, std::byte{0x81}, std::byte{0xEC}, std::byte{0xF0}};
constexpr std::array<std::byte, 20> kLaneOneReadyScanPrefix{
    std::byte{0x48}, std::byte{0x83}, std::byte{0xEC}, std::byte{0x08},
    std::byte{0x8B}, std::byte{0x81}, std::byte{0xF8}, std::byte{0xAE},
    std::byte{0x01}, std::byte{0x00}, std::byte{0x83}, std::byte{0xC0},
    std::byte{0xFC}, std::byte{0x83}, std::byte{0xF8}, std::byte{0x05},
    std::byte{0x0F}, std::byte{0x87}, std::byte{0xA8}, std::byte{0x00}};
constexpr std::array<std::byte, 7> kLaneOneActiveCountPrefix{
    std::byte{0x8B}, std::byte{0x05}, std::byte{0x82}, std::byte{0x4E},
    std::byte{0xA8}, std::byte{0x01}, std::byte{0xC3}};
constexpr std::array<std::byte, 20> kLaneOneSelectionReadyPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x18}, std::byte{0x57}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x40}, std::byte{0x48}, std::byte{0x8B},
    std::byte{0x05}, std::byte{0x57}, std::byte{0x88}, std::byte{0x8D},
    std::byte{0x00}, std::byte{0x48}, std::byte{0x33}, std::byte{0xC4}};
constexpr std::array<std::byte, 20> kLaneOneAuthoredInitializerPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x08}, std::byte{0x48}, std::byte{0x89}, std::byte{0x74},
    std::byte{0x24}, std::byte{0x18}, std::byte{0x55}, std::byte{0x57},
    std::byte{0x41}, std::byte{0x56}, std::byte{0x48}, std::byte{0x8D},
    std::byte{0x6C}, std::byte{0x24}, std::byte{0xB9}, std::byte{0x48}};
constexpr std::array<std::byte, 20> kRouteFlagUpdatePrefix{
    std::byte{0x40}, std::byte{0x55}, std::byte{0x53}, std::byte{0x57},
    std::byte{0x48}, std::byte{0x8D}, std::byte{0xAC}, std::byte{0x24},
    std::byte{0x90}, std::byte{0xFD}, std::byte{0xFF}, std::byte{0xFF},
    std::byte{0x48}, std::byte{0x81}, std::byte{0xEC}, std::byte{0x70},
    std::byte{0x03}, std::byte{0x00}, std::byte{0x00}, std::byte{0x48}};
constexpr std::array<std::byte, 16> kRouteElapsedTimePrefix{
    std::byte{0x40}, std::byte{0x53}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x20}, std::byte{0x80}, std::byte{0x3D},
    std::byte{0x83}, std::byte{0x70}, std::byte{0x2E}, std::byte{0x02},
    std::byte{0x00}, std::byte{0x48}, std::byte{0x8B}, std::byte{0xD9}};
constexpr std::array<std::byte, 20> kRouteTimeoutConfigPrefix{
    std::byte{0x40}, std::byte{0x53}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x20}, std::byte{0x48}, std::byte{0x8B},
    std::byte{0x1D}, std::byte{0x0B}, std::byte{0x3C}, std::byte{0x1A},
    std::byte{0x02}, std::byte{0x48}, std::byte{0x85}, std::byte{0xDB},
    std::byte{0x0F}, std::byte{0x84}, std::byte{0x3B}, std::byte{0x03}};
constexpr std::array<std::byte, 20> kRouteAlternateUpdatePrefix{
    std::byte{0x40}, std::byte{0x53}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x20}, std::byte{0x48}, std::byte{0x8B},
    std::byte{0xD9}, std::byte{0xE8}, std::byte{0x72}, std::byte{0x91},
    std::byte{0xFE}, std::byte{0xFF}, std::byte{0x0F}, std::byte{0xB6},
    std::byte{0x4B}, std::byte{0x20}, std::byte{0xF6}, std::byte{0xC1}};
constexpr std::array<std::byte, 16> kRouteStateEvaluatorPrefix{
    std::byte{0x40}, std::byte{0x53}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x20}, std::byte{0x8B}, std::byte{0x01},
    std::byte{0x48}, std::byte{0x8B}, std::byte{0xD9}, std::byte{0x83},
    std::byte{0xF8}, std::byte{0x02}, std::byte{0x75}, std::byte{0x21}};
constexpr std::array<std::byte, 20> kRouteParentDispatchPrefix{
    std::byte{0x4C}, std::byte{0x8B}, std::byte{0xDC}, std::byte{0x49},
    std::byte{0x89}, std::byte{0x5B}, std::byte{0x18}, std::byte{0x49},
    std::byte{0x89}, std::byte{0x73}, std::byte{0x20}, std::byte{0x55},
    std::byte{0x57}, std::byte{0x41}, std::byte{0x54}, std::byte{0x41},
    std::byte{0x56}, std::byte{0x41}, std::byte{0x57}, std::byte{0x49}};
constexpr std::array<std::byte, 7> kRouteSelectorContextAccessorPrefix{
    std::byte{0x48}, std::byte{0x8D}, std::byte{0x0D}, std::byte{0x51},
    std::byte{0x90}, std::byte{0x8F}, std::byte{0x01}};
constexpr std::array<std::byte, 16> kRouteStateArmPrefix{
    std::byte{0x40}, std::byte{0x53}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x20}, std::byte{0x80}, std::byte{0x79},
    std::byte{0x22}, std::byte{0x00}, std::byte{0x48}, std::byte{0x8B},
    std::byte{0xD9}, std::byte{0xC6}, std::byte{0x41}, std::byte{0x18}};
constexpr std::array<std::byte, 20> kEmbeddedRouteStatePhaseSetPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x08}, std::byte{0x57}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x20}, std::byte{0x0F}, std::byte{0xB6},
    std::byte{0xFA}, std::byte{0x48}, std::byte{0x63}, std::byte{0xD9},
    std::byte{0xE8}, std::byte{0xEB}, std::byte{0x21}, std::byte{0x21}};
constexpr std::array<std::byte, 20> kEmbeddedRouteStateInitializePrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x08}, std::byte{0x48}, std::byte{0x89}, std::byte{0x74},
    std::byte{0x24}, std::byte{0x10}, std::byte{0x57}, std::byte{0x48},
    std::byte{0x83}, std::byte{0xEC}, std::byte{0x20}, std::byte{0x41},
    std::byte{0x0F}, std::byte{0xB6}, std::byte{0xD8}, std::byte{0x48}};
constexpr std::array<std::byte, 20> kEmbeddedRouteStateResetPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x10}, std::byte{0x48}, std::byte{0x89}, std::byte{0x7C},
    std::byte{0x24}, std::byte{0x18}, std::byte{0x55}, std::byte{0x48},
    std::byte{0x8D}, std::byte{0xAC}, std::byte{0x24}, std::byte{0x00},
    std::byte{0xFE}, std::byte{0xFF}, std::byte{0xFF}, std::byte{0x48}};
constexpr std::array<std::byte, 20> kEmbeddedRouteLaneZeroDriverPrefix{
    std::byte{0x4C}, std::byte{0x8B}, std::byte{0xDC}, std::byte{0x55},
    std::byte{0x57}, std::byte{0x49}, std::byte{0x8D}, std::byte{0xAB},
    std::byte{0xB8}, std::byte{0xFD}, std::byte{0xFF}, std::byte{0xFF},
    std::byte{0x48}, std::byte{0x81}, std::byte{0xEC}, std::byte{0x38},
    std::byte{0x03}, std::byte{0x00}, std::byte{0x00}, std::byte{0x48}};
constexpr std::array<std::byte, 20> kEmbeddedRouteTransitionHandlerPrefix{
    std::byte{0x48}, std::byte{0x8B}, std::byte{0xC4}, std::byte{0x57},
    std::byte{0x48}, std::byte{0x81}, std::byte{0xEC}, std::byte{0x80},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x48},
    std::byte{0x89}, std::byte{0x58}, std::byte{0x18}, std::byte{0x48},
    std::byte{0x8B}, std::byte{0xF9}, std::byte{0x48}, std::byte{0x89}};
constexpr std::array<std::byte, 20> kEmbeddedRouteSessionActivityPrefix{
    std::byte{0x48}, std::byte{0x83}, std::byte{0xEC}, std::byte{0x28},
    std::byte{0xE8}, std::byte{0xE7}, std::byte{0x8F}, std::byte{0xDF},
    std::byte{0xFF}, std::byte{0x48}, std::byte{0x85}, std::byte{0xC0},
    std::byte{0x74}, std::byte{0x0C}, std::byte{0x48}, std::byte{0x8B},
    std::byte{0xC8}, std::byte{0x48}, std::byte{0x83}, std::byte{0xC4}};
constexpr std::array<std::byte, 20> kEmbeddedRouteStateStatusPrefix{
    std::byte{0x40}, std::byte{0x53}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x20}, std::byte{0x48}, std::byte{0x63},
    std::byte{0xD9}, std::byte{0xE8}, std::byte{0xA2}, std::byte{0x22},
    std::byte{0x21}, std::byte{0x00}, std::byte{0x48}, std::byte{0x69},
    std::byte{0xCB}, std::byte{0xA8}, std::byte{0x12}, std::byte{0x00}};
constexpr std::array<std::byte, 20> kEmbeddedRouteContextSkipPredicatePrefix{
    std::byte{0x40}, std::byte{0x55}, std::byte{0x48}, std::byte{0x8D},
    std::byte{0xAC}, std::byte{0x24}, std::byte{0x00}, std::byte{0xFE},
    std::byte{0xFF}, std::byte{0xFF}, std::byte{0x48}, std::byte{0x81},
    std::byte{0xEC}, std::byte{0x00}, std::byte{0x03}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x48}, std::byte{0x8B}, std::byte{0x05}};
constexpr std::array<std::byte, 20> kEmbeddedRouteLaneAvailablePredicatePrefix{
    std::byte{0x40}, std::byte{0x53}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x20}, std::byte{0x48}, std::byte{0x63},
    std::byte{0xD9}, std::byte{0xE8}, std::byte{0x02}, std::byte{0xCA},
    std::byte{0xFD}, std::byte{0xFF}, std::byte{0x48}, std::byte{0x85},
    std::byte{0xC0}, std::byte{0x74}, std::byte{0x39}, std::byte{0x83}};
constexpr std::array<std::byte, 20> kEmbeddedRouteAuthoredIdentityPredicatePrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x08}, std::byte{0x57}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x20}, std::byte{0x40}, std::byte{0x32},
    std::byte{0xFF}, std::byte{0x0F}, std::byte{0xB7}, std::byte{0xD9},
    std::byte{0x66}, std::byte{0x83}, std::byte{0xF9}, std::byte{0xFF}};
constexpr std::array<std::byte, 20> kEmbeddedRouteLocalIdentityPredicatePrefix{
    std::byte{0x40}, std::byte{0x53}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x20}, std::byte{0x0F}, std::byte{0xB7},
    std::byte{0xD9}, std::byte{0x66}, std::byte{0x83}, std::byte{0xF9},
    std::byte{0xFF}, std::byte{0x74}, std::byte{0x4D}, std::byte{0x48},
    std::byte{0x89}, std::byte{0x7C}, std::byte{0x24}, std::byte{0x30}};
constexpr std::array<std::byte, 20> kEmbeddedRouteObjectTransitionPrefix{
    std::byte{0x40}, std::byte{0x55}, std::byte{0x53}, std::byte{0x48},
    std::byte{0x8D}, std::byte{0xAC}, std::byte{0x24}, std::byte{0xE8},
    std::byte{0xFD}, std::byte{0xFF}, std::byte{0xFF}, std::byte{0x48},
    std::byte{0x81}, std::byte{0xEC}, std::byte{0x18}, std::byte{0x03},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x48}, std::byte{0x8B}};
constexpr std::array<std::byte, 20> kEmbeddedRouteIdentityProviderRecordLookupPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x08}, std::byte{0x57}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x20}, std::byte{0x0F}, std::byte{0xB7},
    std::byte{0xD9}, std::byte{0x33}, std::byte{0xFF}, std::byte{0xE8},
    std::byte{0xBC}, std::byte{0xCD}, std::byte{0x72}, std::byte{0xFF}};
constexpr std::array<std::byte, 16> kRouteAlternateSourcePrefix{
    std::byte{0x40}, std::byte{0x53}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x30}, std::byte{0x48}, std::byte{0x8B},
    std::byte{0x1D}, std::byte{0xFB}, std::byte{0x86}, std::byte{0x97},
    std::byte{0x01}, std::byte{0x48}, std::byte{0x85}, std::byte{0xDB}};
constexpr std::array<std::byte, 16> kRouteAlternatePredicatePrefix{
    std::byte{0x44}, std::byte{0x8B}, std::byte{0x41}, std::byte{0x40},
    std::byte{0x83}, std::byte{0xCA}, std::byte{0xFF}, std::byte{0x33},
    std::byte{0xC0}, std::byte{0x45}, std::byte{0x85}, std::byte{0xC0},
    std::byte{0x74}, std::byte{0x0B}, std::byte{0x41}, std::byte{0x83}};
constexpr std::array<std::byte, 4> kRouteAlternateModePrefix{
    std::byte{0x8B}, std::byte{0x41}, std::byte{0x40}, std::byte{0xC3}};
constexpr std::array<std::byte, 20> kRouteSelectionIngestPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x08}, std::byte{0x48}, std::byte{0x89}, std::byte{0x74},
    std::byte{0x24}, std::byte{0x10}, std::byte{0x48}, std::byte{0x89},
    std::byte{0x7C}, std::byte{0x24}, std::byte{0x18}, std::byte{0x55},
    std::byte{0x41}, std::byte{0x54}, std::byte{0x41}, std::byte{0x55}};
constexpr std::array<std::byte, 18> kAuthoredLaunchDispatchPrefix{
    std::byte{0x40}, std::byte{0x53}, std::byte{0x57}, std::byte{0x41},
    std::byte{0x54}, std::byte{0x41}, std::byte{0x55}, std::byte{0x41},
    std::byte{0x56}, std::byte{0x41}, std::byte{0x57}, std::byte{0x48},
    std::byte{0x81}, std::byte{0xEC}, std::byte{0x68}, std::byte{0x09},
    std::byte{0x00}, std::byte{0x00}};
constexpr std::array<std::byte, 20> kLaunchCommandInitializePrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x08}, std::byte{0x48}, std::byte{0x89}, std::byte{0x6C},
    std::byte{0x24}, std::byte{0x10}, std::byte{0x48}, std::byte{0x89},
    std::byte{0x74}, std::byte{0x24}, std::byte{0x18}, std::byte{0x57},
    std::byte{0x48}, std::byte{0x83}, std::byte{0xEC}, std::byte{0x20}};
constexpr std::array<std::byte, 16> kLaunchCommandDispatchPrefix{
    std::byte{0x8B}, std::byte{0x11}, std::byte{0x85}, std::byte{0xD2},
    std::byte{0x74}, std::byte{0x28}, std::byte{0x83}, std::byte{0xEA},
    std::byte{0x01}, std::byte{0x74}, std::byte{0x1E}, std::byte{0x83},
    std::byte{0xEA}, std::byte{0x01}, std::byte{0x74}, std::byte{0x14}};
constexpr std::array<std::byte, 10> kRouteModeSetPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x08}, std::byte{0x57}, std::byte{0x48}, std::byte{0x83},
    std::byte{0xEC}, std::byte{0x20}};
constexpr std::array<std::byte, 24> kRouteStateQueryPrefix{
    std::byte{0x40}, std::byte{0x55}, std::byte{0x53}, std::byte{0x56},
    std::byte{0x57}, std::byte{0x41}, std::byte{0x55}, std::byte{0x48},
    std::byte{0x8D}, std::byte{0xAC}, std::byte{0x24}, std::byte{0x90},
    std::byte{0xFD}, std::byte{0xFF}, std::byte{0xFF}, std::byte{0x48},
    std::byte{0x81}, std::byte{0xEC}, std::byte{0x70}, std::byte{0x03},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x48}, std::byte{0x8B}};
constexpr std::array<std::byte, 24> kRouteStatePublishPrefix{
    std::byte{0x40}, std::byte{0x55}, std::byte{0x53}, std::byte{0x57},
    std::byte{0x41}, std::byte{0x54}, std::byte{0x41}, std::byte{0x55},
    std::byte{0x41}, std::byte{0x56}, std::byte{0x41}, std::byte{0x57},
    std::byte{0x48}, std::byte{0x8D}, std::byte{0xAC}, std::byte{0x24},
    std::byte{0x20}, std::byte{0xFD}, std::byte{0xFF}, std::byte{0xFF},
    std::byte{0x48}, std::byte{0x81}, std::byte{0xEC}, std::byte{0xE0}};
constexpr std::array<std::byte, 21> kManagerLocalStartPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x10}, std::byte{0x4C}, std::byte{0x89}, std::byte{0x4C},
    std::byte{0x24}, std::byte{0x20}, std::byte{0x55}, std::byte{0x56},
    std::byte{0x57}, std::byte{0x41}, std::byte{0x54}, std::byte{0x41},
    std::byte{0x55}, std::byte{0x41}, std::byte{0x56}, std::byte{0x41},
    std::byte{0x57}};
constexpr std::array<std::byte, 24> kManagerAuthoredStartPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x10}, std::byte{0x55}, std::byte{0x56}, std::byte{0x57},
    std::byte{0x41}, std::byte{0x54}, std::byte{0x41}, std::byte{0x55},
    std::byte{0x41}, std::byte{0x56}, std::byte{0x41}, std::byte{0x57},
    std::byte{0x48}, std::byte{0x8D}, std::byte{0xAC}, std::byte{0x24},
    std::byte{0x40}, std::byte{0xFD}, std::byte{0xFF}, std::byte{0xFF}};
constexpr std::array<std::byte, 24> kManagerSetupStageAPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x10}, std::byte{0x48}, std::byte{0x89}, std::byte{0x74},
    std::byte{0x24}, std::byte{0x18}, std::byte{0x48}, std::byte{0x89},
    std::byte{0x7C}, std::byte{0x24}, std::byte{0x20}, std::byte{0x55},
    std::byte{0x41}, std::byte{0x54}, std::byte{0x41}, std::byte{0x55},
    std::byte{0x41}, std::byte{0x56}, std::byte{0x41}, std::byte{0x57}};
constexpr std::array<std::byte, 19> kManagerSetupStageBPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x18}, std::byte{0x48}, std::byte{0x89}, std::byte{0x74},
    std::byte{0x24}, std::byte{0x20}, std::byte{0x55}, std::byte{0x41},
    std::byte{0x54}, std::byte{0x41}, std::byte{0x55}, std::byte{0x41},
    std::byte{0x56}, std::byte{0x41}, std::byte{0x57}};
constexpr std::array<std::byte, 11> kManagerSetupStageCPrefix{
    std::byte{0x48}, std::byte{0x89}, std::byte{0x5C}, std::byte{0x24},
    std::byte{0x18}, std::byte{0x55}, std::byte{0x56}, std::byte{0x57},
    std::byte{0x48}, std::byte{0x83}, std::byte{0xEC}};
constexpr std::array<std::byte, 11> kManagerSetupStageDPrefix{
    std::byte{0x41}, std::byte{0x54}, std::byte{0x41}, std::byte{0x56},
    std::byte{0x48}, std::byte{0x81}, std::byte{0xEC}, std::byte{0x78},
    std::byte{0x04}, std::byte{0x00}, std::byte{0x00}};
constexpr std::array<std::byte, 16> kComponentDispatchPrefix{
    std::byte{0x40}, std::byte{0x55}, std::byte{0x56}, std::byte{0x57},
    std::byte{0x41}, std::byte{0x56}, std::byte{0x41}, std::byte{0x57},
    std::byte{0x48}, std::byte{0x8D}, std::byte{0xAC}, std::byte{0x24},
    std::byte{0x10}, std::byte{0xFE}, std::byte{0xFF}, std::byte{0xFF}};
constexpr std::array<std::byte, 16> kComponentRegisterPrefix{
    std::byte{0x40}, std::byte{0x55}, std::byte{0x56}, std::byte{0x57},
    std::byte{0x41}, std::byte{0x54}, std::byte{0x41}, std::byte{0x55},
    std::byte{0x41}, std::byte{0x57}, std::byte{0x48}, std::byte{0x8D},
    std::byte{0xAC}, std::byte{0x24}, std::byte{0xC8}, std::byte{0xFC}};
constexpr std::array<std::byte, 13> kComponentLookupPrefix{
    std::byte{0x8B}, std::byte{0x41}, std::byte{0x30}, std::byte{0x4C},
    std::byte{0x8B}, std::byte{0xDA}, std::byte{0x44}, std::byte{0x0F},
    std::byte{0xA3}, std::byte{0xC0}, std::byte{0x4C}, std::byte{0x8B},
    std::byte{0xD1}};
constexpr std::array<std::byte, 16> kComponentBuildPrefix{
    std::byte{0x40}, std::byte{0x55}, std::byte{0x53}, std::byte{0x56},
    std::byte{0x57}, std::byte{0x41}, std::byte{0x54}, std::byte{0x41},
    std::byte{0x56}, std::byte{0x41}, std::byte{0x57}, std::byte{0x48},
    std::byte{0x8D}, std::byte{0xAC}, std::byte{0x24}, std::byte{0xA0}};
constexpr std::array<std::byte, 21> kComponentTickPrefix{
    std::byte{0x4C}, std::byte{0x8B}, std::byte{0xDC}, std::byte{0x55},
    std::byte{0x57}, std::byte{0x41}, std::byte{0x56}, std::byte{0x49},
    std::byte{0x8D}, std::byte{0xAB}, std::byte{0x78}, std::byte{0xFD},
    std::byte{0xFF}, std::byte{0xFF}, std::byte{0x48}, std::byte{0x81},
    std::byte{0xEC}, std::byte{0x70}, std::byte{0x03}, std::byte{0x00},
    std::byte{0x00}};
constexpr std::array<std::byte, 12> kPostComponentSyncPrefix{
    std::byte{0x40}, std::byte{0x53}, std::byte{0x57}, std::byte{0x41},
    std::byte{0x54}, std::byte{0x48}, std::byte{0x81}, std::byte{0xEC},
    std::byte{0x60}, std::byte{0x01}, std::byte{0x00}, std::byte{0x00}};

using TransitionUpdate = bool(__fastcall*)(std::byte* manager,
                                            const void* transition,
                                            const void* update) noexcept;
using SlotUpdate = bool(__fastcall*)(std::byte* manager,
                                      std::int32_t slot,
                                      std::int32_t mode,
                                      std::int32_t value) noexcept;
using ModeUpdate = bool(__fastcall*)(std::byte* manager,
                                      std::int32_t slot,
                                      std::int32_t mode) noexcept;
using AuthoritySlotMap = bool(__fastcall*)(std::byte* context,
                                             std::byte* runtime,
                                             std::byte* entries,
                                             std::int32_t* mapping) noexcept;
using AuthorityApply = bool(__fastcall*)(std::byte* runtime,
                                          std::byte* entries,
                                          std::byte* context) noexcept;
using AuthorityRefresh = void(__fastcall*)(std::byte* manager, bool refresh) noexcept;
using AuthorityEntriesGate = bool(__fastcall*)(std::byte* entries) noexcept;
using AuthoritySnapshotGate = bool(__fastcall*)(std::byte* runtime,
                                                 std::byte* snapshot) noexcept;
using ManagerLookup = std::byte*(__fastcall*)(std::byte* context,
                                               const void* identifier) noexcept;
using ManagerTable = std::byte**(__fastcall*)() noexcept;
using LifecycleEvent = std::int32_t(__fastcall*)(std::byte* manager,
                                                   std::int32_t eventCode,
                                                   const void* payload) noexcept;
using ManagerEventBegin = void(__fastcall*)(std::byte* manager,
                                              std::int32_t eventCode) noexcept;
using ManagerEnsure = void(__fastcall*)(std::int32_t identity) noexcept;
using IdentityRequest = void(__fastcall*)(std::int32_t identity,
                                           const void* descriptor) noexcept;
using IdentityEnable = void(__fastcall*)(std::int32_t identity, bool local) noexcept;
using DefinitionPendingConsumer = void(__fastcall*)(std::int32_t identity) noexcept;
using ManagerActivate = bool(__fastcall*)(std::byte* manager,
                                            const void* record,
                                            const std::byte* payload) noexcept;
using HostHandoffApply = bool(__fastcall*)(std::byte* manager,
                                             const std::byte* payload) noexcept;
using MemberRecordIndex = std::int32_t(__fastcall*)(std::byte** table,
                                                      const void* record) noexcept;
using IdentityDefinition = std::byte*(__fastcall*)(std::int32_t identity) noexcept;
using IdentityDescriptorCopy = bool(__fastcall*)(std::int32_t identity,
                                                   bool alternate,
                                                   void* output) noexcept;
using SessionDescriptionStage = void(__fastcall*)(std::int32_t identity,
                                                    const std::byte* descriptor) noexcept;
using ManagedSessionMigration = void(__fastcall*)(std::int32_t identity,
                                                    std::byte* definition) noexcept;
using TransitionDispatch = void(__fastcall*)(std::byte* context,
                                               const void* transition,
                                               const void* update) noexcept;
using WireDispatch = void(__fastcall*)(std::byte* context,
                                        std::byte* activityClient,
                                        const void* eventData,
                                        const void* identifier,
                                        bool flag,
                                        std::uint32_t eventCode,
                                        std::uint32_t eventValue,
                                         const void* payload) noexcept;
using ActivityClientUpdate = bool(__fastcall*)(std::byte* activityClient,
                                                 void* eventData,
                                                 std::uint32_t eventValue,
                                                 void* identifier,
                                                 void* payload,
                                                 bool flag6,
                                                 bool flag7) noexcept;
using ActivityReceiverLookup = std::byte*(__fastcall*)(const void* member) noexcept;
using ActivityReceiverIterator = std::byte*(__fastcall*)(std::byte* current) noexcept;
using ActivityRosterApply = void(__fastcall*)(std::byte* context,
                                                const std::byte* message,
                                                const std::byte* payload) noexcept;
using ActivityPeerDispatch = void(__fastcall*)(std::byte* context,
                                                 const std::byte* message,
                                                 std::byte* source,
                                                 std::uint32_t messageType,
                                                 std::uint32_t payloadSize,
                                                 const std::byte* payload) noexcept;
using ActivityReceiverActivate = void(__fastcall*)(std::byte* root,
                                                     std::int32_t slot,
                                                     bool activate) noexcept;
using ActivityReceiverRoot = std::byte*(__fastcall*)() noexcept;
using ActivityReceiverBind = std::int32_t(__fastcall*)(void* context,
                                                        std::int32_t activity,
                                                        const void* descriptorA,
                                                        const void* descriptorB,
                                                        bool flag,
                                                        std::int32_t mode,
                                                        const void* extra) noexcept;
using ActivityReceiverCreator = void(__fastcall*)(std::byte* manager) noexcept;
using ManagerUpdateLoop = void(__fastcall*)(std::byte* manager) noexcept;
using ManagerUpdateGate = bool(__fastcall*)(std::byte* manager) noexcept;
using LaunchProducer = void(__fastcall*)() noexcept;
using LaunchProducerGuardSetter = void(__fastcall*)(bool enabled) noexcept;
using LaunchProducerWorldActivity = std::int32_t(__fastcall*)() noexcept;
using LaunchProducerSessionActivity = std::int32_t(__fastcall*)(std::int32_t lane) noexcept;
using LaunchProducerReady = bool(__fastcall*)(std::uint8_t* stateOut) noexcept;
using LaunchProducerReadyCallback = void(__fastcall*)(void* context,
                                                        std::uint32_t event,
                                                        void* payload) noexcept;
using ActivityEventDispatchLocal = void(__fastcall*)(std::uint32_t event) noexcept;
using ActivityEventDispatchPayload = void(__fastcall*)(std::uint32_t event,
                                                         void* payload) noexcept;
using ActivityEvent46RegisteredProducer = void(__fastcall*)(void* context,
                                                              const void* payloadRef) noexcept;
using ActivityEvent46ProducerReset = void(__fastcall*)(void* context) noexcept;
using ActivityEvent46ProducerInitialize = void(__fastcall*)(void* context) noexcept;
using ActivityEvent46ProducerAvailable = bool(__fastcall*)(void* context) noexcept;
using ActivityEvent46ProducerRequest = void(__fastcall*)(void* context,
                                                           void* request,
                                                           void* output) noexcept;
using ActivityEvent46ProducerTick = void(__fastcall*)(void* context) noexcept;
using ActivityEvent46RequestFlow = bool(__fastcall*)(void* context,
                                                      const void* descriptor) noexcept;
using ActivityWorldContext = void*(__fastcall*)() noexcept;
using ActivityEvent46ActionDispatch = bool(__fastcall*)(void* context,
                                                         void* auxiliary) noexcept;
using ActivityEvent46RequestPredicate = bool(__fastcall*)(void* context,
                                                           void* statusOut) noexcept;
using LaunchProducerPhase = bool(__fastcall*)(std::int32_t phase,
                                               std::int32_t state) noexcept;
using ManagerModeSet = void(__fastcall*)(std::byte* manager,
                                          std::int32_t newMode,
                                          std::uint32_t payloadSize,
                                          const void* payload) noexcept;
using RouteDescriptorLookup = std::byte*(__fastcall*)(std::int32_t lane,
                                                         std::byte** managerOut) noexcept;
using RouteCommit = void(__fastcall*)(const std::byte* descriptor) noexcept;
using RouteUpdate = void(__fastcall*)(std::byte* manager,
                                       std::uint64_t sourceNonce,
                                       std::int32_t generation) noexcept;
using DefaultRouteZeroInitializer = std::uint64_t*(__fastcall*)(
    std::uint64_t* result) noexcept;
using DefaultRouteReferencePredicate = bool(__fastcall*)(std::byte* service) noexcept;
using DefaultRoutePairPredicate = bool(__fastcall*)(std::int32_t left,
                                                      std::int32_t right) noexcept;
using DefaultRouteRequiredPredicate = bool(__fastcall*)(std::int32_t lane) noexcept;
using DefaultRouteCrossManagerPredicate = bool(__fastcall*)() noexcept;
using LaneOneReadyScan = bool(__fastcall*)(std::byte* manager) noexcept;
using LaneOneActiveCount = std::int32_t(__fastcall*)() noexcept;
using LaneOneSelectionReady = bool(__fastcall*)(std::byte* owner,
                                                 const std::byte* selection) noexcept;
using LaneOneAuthoredInitializer = void(__fastcall*)(std::int32_t selector,
                                                       const std::byte* source,
                                                       const std::byte* descriptor) noexcept;
using RouteFlagUpdate = void(__fastcall*)(std::byte* state) noexcept;
using RouteElapsedTime = std::int64_t(__fastcall*)(std::uint64_t timestamp) noexcept;
using RouteTimeoutConfig = std::byte*(__fastcall*)() noexcept;
using RouteAlternateUpdate = void(__fastcall*)(std::byte* state) noexcept;
using RouteStateEvaluator = void(__fastcall*)(std::byte* state) noexcept;
using RouteParentDispatch = void(__fastcall*)(std::byte* state) noexcept;
using RouteSelectorContextAccessor = std::byte*(__fastcall*)() noexcept;
using RouteSelectorVariantLookup = std::byte*(__fastcall*)(std::byte* context,
                                                            std::int32_t selector) noexcept;
using AssignmentProviderUpdate = bool(__fastcall*)(std::byte* context,
                                                    std::int32_t selector,
                                                    std::int32_t identity,
                                                    std::byte* variant,
                                                    std::byte* assignment) noexcept;
using AssignmentProviderStateConsumer = std::byte*(__fastcall*)(
    std::byte* state,
    std::byte* value,
    std::int32_t* resultOut) noexcept;
using MatchmakingLanePolicyMaterializer = void(__fastcall*)(std::byte* configuration,
                                                             std::byte* descriptor,
                                                             std::byte* laneRecord) noexcept;
using MatchmakingLaneRebuild = void(__fastcall*)(std::byte* context) noexcept;
using AssignmentWatcherUpdate = void(__fastcall*)(std::byte* watcher,
                                                    std::byte* manager) noexcept;
using AssignmentRevision = std::int32_t(__fastcall*)(std::int32_t selector) noexcept;
using RouteStateArm = void(__fastcall*)(std::byte* state) noexcept;
using EmbeddedRouteStatePhaseSet = void(__fastcall*)(std::int32_t lane,
                                                       bool enabled) noexcept;
using EmbeddedRouteStateInitialize = void(__fastcall*)(std::int32_t lane,
                                                         std::uint8_t flags,
                                                         std::uint8_t phase) noexcept;
using EmbeddedRouteStateReset = void(__fastcall*)(std::byte* state) noexcept;
using EmbeddedRouteLaneZeroDriver = void(__fastcall*)(std::byte* context) noexcept;
using EmbeddedRouteTransitionHandler = void(__fastcall*)(std::byte* context,
                                                           std::int32_t eventCode,
                                                           std::int32_t detail) noexcept;
using EmbeddedRouteStateStatus = std::int32_t(__fastcall*)(std::int32_t lane) noexcept;
using EmbeddedRouteContextSkipPredicate = bool(__fastcall*)(std::byte* context) noexcept;
using EmbeddedRouteLaneAvailablePredicate = bool(__fastcall*)(std::int32_t lane) noexcept;
using EmbeddedRouteIdentityPredicate = bool(__fastcall*)(std::uint16_t identifier) noexcept;
using EmbeddedRouteIdentityProviderRecordLookup = std::byte*(__fastcall*)(
    std::uint16_t identifier) noexcept;
using EmbeddedRouteIdentityProviderLookup = std::byte*(__fastcall*)(
    std::byte* service,
    std::uint16_t identifier) noexcept;
using EmbeddedRouteObjectTransition = void(__fastcall*)(std::byte* owner) noexcept;
using RouteAlternateSource = std::byte*(__fastcall*)() noexcept;
using RouteAlternatePredicate = bool(__fastcall*)(std::byte* source) noexcept;
using RouteAlternateMode = std::int32_t(__fastcall*)(std::byte* source) noexcept;
using LaneManagerAccessor = std::byte*(__fastcall*)(std::int32_t lane) noexcept;
using OptionalHasValue = bool(__fastcall*)(std::byte* owner) noexcept;
using OptionalValue = const std::byte*(__fastcall*)(std::byte* owner) noexcept;
using CurrentSelectionPublish = void(__fastcall*)(std::byte* owner,
                                                    const std::byte* descriptor) noexcept;
using RouteSelectionIngest = bool(__fastcall*)(std::uintptr_t arg1,
                                                 bool alternate,
                                                 std::uint32_t arg3,
                                                 std::uint32_t arg4,
                                                 const std::byte* selection,
                                                 std::uintptr_t handle) noexcept;
using AuthoredLaunchDispatch = void(__fastcall*)(std::int32_t launchMode,
                                                  std::int32_t selectionType,
                                                  const std::byte* selection,
                                                  const void* handle,
                                                  std::uint32_t option) noexcept;
using LaunchCommandInitialize = bool(__fastcall*)(std::byte* command,
                                                   const std::byte* selection,
                                                   std::int32_t launchMode,
                                                   std::int32_t deferred,
                                                   const void* handle) noexcept;
using LaunchCommandDispatch = void(__fastcall*)(std::byte* command) noexcept;
using RouteModeQuery = bool(__fastcall*)() noexcept;
using RouteModeSet = void(__fastcall*)(bool enabled) noexcept;
using OmegaRouteLifecycleQuery = std::int32_t(__fastcall*)() noexcept;
using OmegaRouteLifecycleAccessor = std::byte*(__fastcall*)() noexcept;
using RouteStateQuery = bool(__fastcall*)(std::int32_t lane,
                                           std::int32_t* stateOut) noexcept;
using RouteStatePublish = void(__fastcall*)(std::int32_t lane,
                                             std::int32_t routeCode) noexcept;
using ManagerLocalStart = bool(__fastcall*)(std::byte* manager,
                                             std::uint8_t flags,
                                             std::int32_t selector,
                                             std::uint64_t sourceNonce,
                                             std::uintptr_t arg5,
                                             std::uintptr_t arg6,
                                             std::uintptr_t arg7,
                                             std::uintptr_t arg8) noexcept;
using ManagerAuthoredStart = void(__fastcall*)(std::byte* manager,
                                                std::uint8_t flags,
                                                std::int32_t selector,
                                                const void* source,
                                                std::uintptr_t arg5,
                                                std::uintptr_t arg6,
                                                std::uintptr_t arg7,
                                                std::uintptr_t arg8,
                                                std::uintptr_t arg9) noexcept;
using ManagerSetupStage = void(__fastcall*)(std::byte* manager) noexcept;
using ComponentDispatch = void(__fastcall*)(std::byte* manager,
                                               std::int32_t componentIndex) noexcept;
using ComponentRegister = void(__fastcall*)(std::byte* manager,
                                              std::int32_t componentIndex) noexcept;
using ComponentLookup = bool(__fastcall*)(std::byte* context,
                                             const void* identifier,
                                             std::int32_t slot,
                                             std::int32_t componentIndex) noexcept;
using ComponentReuse = void(__fastcall*)(std::byte* manager,
                                          std::int32_t componentIndex,
                                          const void* identifier) noexcept;
using ComponentMapping = std::int32_t(__fastcall*)(std::byte* context,
                                                     const void* identifier) noexcept;
using ComponentBuild = bool(__fastcall*)(std::byte* manager,
                                           std::int32_t componentIndex,
                                           std::uint32_t generation,
                                           std::uint32_t componentValue,
                                           std::uint32_t mask,
                                           const void* descriptor,
                                           bool slotReady,
                                           std::uint32_t descriptorHash,
                                           const void* fingerprint) noexcept;
using ComponentStep = std::int32_t(__fastcall*)(std::byte* manager,
                                                  std::int32_t componentIndex,
                                                  const void* identifier) noexcept;
using ComponentTick = bool(__fastcall*)(std::byte* manager,
                                          std::int32_t componentIndex) noexcept;
using PostComponentSync = void(__fastcall*)(std::byte* manager, bool force) noexcept;
using ActivityStateTable = std::byte*(__fastcall*)() noexcept;
using ManagerSnapshot = std::byte*(__fastcall*)(std::byte* manager) noexcept;
using ManagerGeneration = std::int32_t(__fastcall*)(std::byte* manager) noexcept;
using SnapshotFirstEntry = std::int32_t(__fastcall*)(std::byte* snapshot) noexcept;

hooking::detour::Handle g_transitionHandle{};
hooking::detour::Handle g_slotHandle{};
hooking::detour::Handle g_modeHandle{};
hooking::detour::Handle g_authoritySlotMapHandle{};
hooking::detour::Handle g_authorityApplyHandle{};
hooking::detour::Handle g_authorityRefreshHandle{};
hooking::detour::Handle g_authorityAnyAssignedGateHandle{};
hooking::detour::Handle g_authorityAnyUnassignedGateHandle{};
hooking::detour::Handle g_authoritySnapshotGateHandle{};
hooking::detour::Handle g_managerLookupHandle{};
hooking::detour::Handle g_lifecycleEventHandle{};
hooking::detour::Handle g_managerEnsureHandle{};
hooking::detour::Handle g_identityRequestHandle{};
hooking::detour::Handle g_identityEnableHandle{};
hooking::detour::Handle g_managerActivateHandle{};
hooking::detour::Handle g_hostHandoffApplyHandle{};
hooking::detour::Handle g_memberRecordIndexHandle{};
hooking::detour::Handle g_identityDescriptorCopyHandle{};
hooking::detour::Handle g_sessionDescriptionStageHandle{};
hooking::detour::Handle g_managedSessionMigrationHandle{};
hooking::detour::Handle g_transitionDispatchHandle{};
hooking::detour::Handle g_wireDispatchHandle{};
hooking::detour::Handle g_activityClientUpdateHandle{};
hooking::detour::Handle g_activityReceiverLookupHandle{};
hooking::detour::Handle g_activityRosterApplyHandle{};
hooking::detour::Handle g_activityPeerDispatchHandle{};
hooking::detour::Handle g_activityReceiverActivateHandle{};
hooking::detour::Handle g_activityReceiverBindHandle{};
hooking::detour::Handle g_activityReceiverCreatorHandle{};
hooking::detour::Handle g_managerUpdateLoopHandle{};
hooking::detour::Handle g_managerUpdateGateHandle{};
hooking::detour::Handle g_launchProducerHandle{};
hooking::detour::Handle g_launchProducerGuardSetterHandle{};
hooking::detour::Handle g_launchProducerReadyCallbackHandle{};
hooking::detour::Handle g_activityEventDispatchLocalHandle{};
hooking::detour::Handle g_activityEventDispatchPayloadHandle{};
hooking::detour::Handle g_activityEvent46RegisteredProducerHandle{};
hooking::detour::Handle g_activityEvent46ProducerResetHandle{};
hooking::detour::Handle g_activityEvent46ProducerInitializeHandle{};
hooking::detour::Handle g_activityEvent46ProducerAvailableHandle{};
hooking::detour::Handle g_activityEvent46ProducerRequestHandle{};
hooking::detour::Handle g_activityEvent46ProducerTickHandle{};
hooking::detour::Handle g_activityEvent46RequestFlowHandle{};
hooking::detour::Handle g_activityEvent46ActionDispatchHandle{};
hooking::detour::Handle g_activityEvent46RequestPredicateHandle{};
hooking::detour::Handle g_managerModeSetHandle{};
hooking::detour::Handle g_routeDescriptorLookupHandle{};
hooking::detour::Handle g_routeCommitHandle{};
hooking::detour::Handle g_routeUpdateHandle{};
hooking::detour::Handle g_defaultRouteZeroInitializerHandle{};
hooking::detour::Handle g_defaultRouteReferencePredicateHandle{};
hooking::detour::Handle g_defaultRoutePairPredicateHandle{};
hooking::detour::Handle g_defaultRouteRequiredPredicateHandle{};
hooking::detour::Handle g_defaultRouteCrossManagerPredicateHandle{};
hooking::detour::Handle g_laneOneReadyScanHandle{};
hooking::detour::Handle g_laneOneActiveCountHandle{};
hooking::detour::Handle g_laneOneSelectionReadyHandle{};
hooking::detour::Handle g_laneOneAuthoredInitializerHandle{};
hooking::detour::Handle g_routeFlagUpdateHandle{};
hooking::detour::Handle g_routeElapsedTimeHandle{};
hooking::detour::Handle g_routeTimeoutConfigHandle{};
hooking::detour::Handle g_routeAlternateUpdateHandle{};
hooking::detour::Handle g_routeStateEvaluatorHandle{};
hooking::detour::Handle g_routeParentDispatchHandle{};
hooking::detour::Handle g_routeSelectorVariantLookupHandle{};
hooking::detour::Handle g_assignmentProviderUpdateHandle{};
hooking::detour::Handle g_assignmentProviderStateConsumerHandle{};
hooking::detour::Handle g_matchmakingLanePolicyMaterializerHandle{};
hooking::detour::Handle g_matchmakingLaneRebuildHandle{};
hooking::detour::Handle g_assignmentWatcherUpdateHandle{};
hooking::detour::Handle g_routeStateArmHandle{};
hooking::detour::Handle g_embeddedRouteStatePhaseSetHandle{};
hooking::detour::Handle g_embeddedRouteStateInitializeHandle{};
hooking::detour::Handle g_embeddedRouteStateResetHandle{};
hooking::detour::Handle g_embeddedRouteLaneZeroDriverHandle{};
hooking::detour::Handle g_embeddedRouteTransitionHandlerHandle{};
hooking::detour::Handle g_embeddedRouteSessionActivityHandle{};
hooking::detour::Handle g_embeddedRouteStateStatusHandle{};
hooking::detour::Handle g_embeddedRouteContextSkipPredicateHandle{};
hooking::detour::Handle g_embeddedRouteLaneAvailablePredicateHandle{};
hooking::detour::Handle g_embeddedRouteAuthoredIdentityPredicateHandle{};
hooking::detour::Handle g_embeddedRouteLocalIdentityPredicateHandle{};
hooking::detour::Handle g_embeddedRouteIdentityProviderRecordLookupHandle{};
hooking::detour::Handle g_embeddedRouteIdentityProviderLookupHandle{};
hooking::detour::Handle g_embeddedRouteObjectTransitionHandle{};
hooking::detour::Handle g_routeAlternateSourceHandle{};
hooking::detour::Handle g_routeAlternatePredicateHandle{};
hooking::detour::Handle g_routeAlternateModeHandle{};
hooking::detour::Handle g_currentSelectionPublishHandle{};
hooking::detour::Handle g_upstreamSelectionPublishHandle{};
hooking::detour::Handle g_upstreamSelectionUpdateHandle{};
hooking::detour::Handle g_routeSelectionIngestHandle{};
hooking::detour::Handle g_authoredLaunchDispatchHandle{};
hooking::detour::Handle g_launchCommandInitializeHandle{};
hooking::detour::Handle g_launchCommandDispatchHandle{};
hooking::detour::Handle g_routeModeZeroSetHandle{};
hooking::detour::Handle g_routeModeOneSetHandle{};
hooking::detour::Handle g_omegaRouteLifecycleQueryHandle{};
hooking::detour::Handle g_omegaRouteLifecycleAccessorHandle{};
hooking::detour::Handle g_routeStateQueryHandle{};
hooking::detour::Handle g_routeStatePublishHandle{};
hooking::detour::Handle g_managerLocalStartHandle{};
hooking::detour::Handle g_managerAuthoredStartHandle{};
hooking::detour::Handle g_managerSetupStageAHandle{};
hooking::detour::Handle g_managerSetupStageBHandle{};
hooking::detour::Handle g_managerSetupStageCHandle{};
hooking::detour::Handle g_managerSetupStageDHandle{};
hooking::detour::Handle g_componentDispatchHandle{};
hooking::detour::Handle g_componentLookupHandle{};
hooking::detour::Handle g_componentRegisterHandle{};
hooking::detour::Handle g_componentReuseHandle{};
hooking::detour::Handle g_componentMappingHandle{};
hooking::detour::Handle g_componentBuildHandle{};
hooking::detour::Handle g_componentStepHandle{};
hooking::detour::Handle g_componentTickHandle{};
hooking::detour::Handle g_postComponentSyncHandle{};
std::atomic<TransitionUpdate> g_transitionOriginal{nullptr};
std::atomic<SlotUpdate> g_slotOriginal{nullptr};
std::atomic<ModeUpdate> g_modeOriginal{nullptr};
std::atomic<AuthoritySlotMap> g_authoritySlotMapOriginal{nullptr};
std::atomic<AuthorityApply> g_authorityApplyOriginal{nullptr};
std::atomic<AuthorityRefresh> g_authorityRefreshOriginal{nullptr};
std::atomic<AuthorityEntriesGate> g_authorityAnyAssignedGateOriginal{nullptr};
std::atomic<AuthorityEntriesGate> g_authorityAnyUnassignedGateOriginal{nullptr};
std::atomic<AuthoritySnapshotGate> g_authoritySnapshotGateOriginal{nullptr};
std::atomic<ManagerLookup> g_managerLookupOriginal{nullptr};
std::atomic<LifecycleEvent> g_lifecycleEventOriginal{nullptr};
std::atomic<ManagerEnsure> g_managerEnsureOriginal{nullptr};
std::atomic<IdentityRequest> g_identityRequestOriginal{nullptr};
std::atomic<IdentityEnable> g_identityEnableOriginal{nullptr};
std::atomic<ManagerActivate> g_managerActivateOriginal{nullptr};
std::atomic<HostHandoffApply> g_hostHandoffApplyOriginal{nullptr};
std::atomic<MemberRecordIndex> g_memberRecordIndexOriginal{nullptr};
struct ManagerActivationTrace {
    bool armed{};
    std::int32_t entryRecordIndex{-2};
    std::int32_t tailRecordIndex{-2};
};
thread_local ManagerActivationTrace g_managerActivationTrace{};
struct WireDispatchManagerTrace {
    bool armed{};
    std::uint32_t eventCode{};
    std::byte* manager{};
};
thread_local WireDispatchManagerTrace g_wireDispatchManagerTrace{};
std::atomic<IdentityDescriptorCopy> g_identityDescriptorCopyOriginal{nullptr};
std::atomic<SessionDescriptionStage> g_sessionDescriptionStageOriginal{nullptr};
std::atomic<ManagedSessionMigration> g_managedSessionMigrationOriginal{nullptr};
std::atomic<TransitionDispatch> g_transitionDispatchOriginal{nullptr};
std::atomic<WireDispatch> g_wireDispatchOriginal{nullptr};
std::atomic<ActivityClientUpdate> g_activityClientUpdateOriginal{nullptr};
std::atomic<ActivityReceiverLookup> g_activityReceiverLookupOriginal{nullptr};
std::atomic<ActivityRosterApply> g_activityRosterApplyOriginal{nullptr};
std::atomic<ActivityPeerDispatch> g_activityPeerDispatchOriginal{nullptr};
std::atomic<ActivityReceiverActivate> g_activityReceiverActivateOriginal{nullptr};
std::atomic<ActivityReceiverBind> g_activityReceiverBindOriginal{nullptr};
std::atomic<ActivityReceiverCreator> g_activityReceiverCreatorOriginal{nullptr};
std::atomic<ManagerUpdateLoop> g_managerUpdateLoopOriginal{nullptr};
std::atomic<ManagerUpdateGate> g_managerUpdateGateOriginal{nullptr};
std::atomic<LaunchProducer> g_launchProducerOriginal{nullptr};
std::atomic<LaunchProducerGuardSetter> g_launchProducerGuardSetterOriginal{nullptr};
std::atomic<LaunchProducerReadyCallback> g_launchProducerReadyCallbackOriginal{nullptr};
std::atomic<ActivityEventDispatchLocal> g_activityEventDispatchLocalOriginal{nullptr};
std::atomic<ActivityEventDispatchPayload> g_activityEventDispatchPayloadOriginal{nullptr};
std::atomic<ActivityEvent46RegisteredProducer> g_activityEvent46RegisteredProducerOriginal{nullptr};
std::atomic<ActivityEvent46ProducerReset> g_activityEvent46ProducerResetOriginal{nullptr};
std::atomic<ActivityEvent46ProducerInitialize> g_activityEvent46ProducerInitializeOriginal{nullptr};
std::atomic<ActivityEvent46ProducerAvailable> g_activityEvent46ProducerAvailableOriginal{nullptr};
std::atomic<ActivityEvent46ProducerRequest> g_activityEvent46ProducerRequestOriginal{nullptr};
std::atomic<ActivityEvent46ProducerTick> g_activityEvent46ProducerTickOriginal{nullptr};
std::atomic<ActivityEvent46RequestFlow> g_activityEvent46RequestFlowOriginal{nullptr};
std::atomic<ActivityEvent46ActionDispatch> g_activityEvent46ActionDispatchOriginal{nullptr};
std::atomic<ActivityEvent46RequestPredicate> g_activityEvent46RequestPredicateOriginal{nullptr};
std::atomic<ManagerModeSet> g_managerModeSetOriginal{nullptr};
std::atomic<RouteDescriptorLookup> g_routeDescriptorLookupOriginal{nullptr};
std::atomic<RouteCommit> g_routeCommitOriginal{nullptr};
std::atomic<RouteUpdate> g_routeUpdateOriginal{nullptr};
std::atomic<DefaultRouteZeroInitializer> g_defaultRouteZeroInitializerOriginal{nullptr};
std::atomic<DefaultRouteReferencePredicate> g_defaultRouteReferencePredicateOriginal{nullptr};
std::atomic<DefaultRoutePairPredicate> g_defaultRoutePairPredicateOriginal{nullptr};
std::atomic<DefaultRouteRequiredPredicate> g_defaultRouteRequiredPredicateOriginal{nullptr};
std::atomic<DefaultRouteCrossManagerPredicate>
    g_defaultRouteCrossManagerPredicateOriginal{nullptr};
std::atomic<LaneOneReadyScan> g_laneOneReadyScanOriginal{nullptr};
std::atomic<LaneOneActiveCount> g_laneOneActiveCountOriginal{nullptr};
std::atomic<LaneOneSelectionReady> g_laneOneSelectionReadyOriginal{nullptr};
std::atomic<LaneOneAuthoredInitializer> g_laneOneAuthoredInitializerOriginal{nullptr};
std::atomic<RouteFlagUpdate> g_routeFlagUpdateOriginal{nullptr};
std::atomic<RouteElapsedTime> g_routeElapsedTimeOriginal{nullptr};
std::atomic<RouteTimeoutConfig> g_routeTimeoutConfigOriginal{nullptr};
std::atomic<RouteAlternateUpdate> g_routeAlternateUpdateOriginal{nullptr};
std::atomic<RouteStateEvaluator> g_routeStateEvaluatorOriginal{nullptr};
std::atomic<RouteParentDispatch> g_routeParentDispatchOriginal{nullptr};
std::atomic<RouteSelectorVariantLookup> g_routeSelectorVariantLookupOriginal{nullptr};
std::atomic<AssignmentProviderUpdate> g_assignmentProviderUpdateOriginal{nullptr};
std::atomic<AssignmentProviderStateConsumer> g_assignmentProviderStateConsumerOriginal{nullptr};
std::atomic<MatchmakingLanePolicyMaterializer>
    g_matchmakingLanePolicyMaterializerOriginal{nullptr};
std::atomic<MatchmakingLaneRebuild> g_matchmakingLaneRebuildOriginal{nullptr};
std::atomic<AssignmentWatcherUpdate> g_assignmentWatcherUpdateOriginal{nullptr};
std::atomic<RouteStateArm> g_routeStateArmOriginal{nullptr};
std::atomic<EmbeddedRouteStatePhaseSet> g_embeddedRouteStatePhaseSetOriginal{nullptr};
std::atomic<EmbeddedRouteStateInitialize> g_embeddedRouteStateInitializeOriginal{nullptr};
std::atomic<EmbeddedRouteStateReset> g_embeddedRouteStateResetOriginal{nullptr};
std::atomic<EmbeddedRouteLaneZeroDriver> g_embeddedRouteLaneZeroDriverOriginal{nullptr};
std::atomic<EmbeddedRouteTransitionHandler> g_embeddedRouteTransitionHandlerOriginal{nullptr};
std::atomic<LaunchProducerSessionActivity> g_embeddedRouteSessionActivityOriginal{nullptr};
std::atomic<EmbeddedRouteStateStatus> g_embeddedRouteStateStatusOriginal{nullptr};
std::atomic<EmbeddedRouteContextSkipPredicate>
    g_embeddedRouteContextSkipPredicateOriginal{nullptr};
std::atomic<EmbeddedRouteLaneAvailablePredicate>
    g_embeddedRouteLaneAvailablePredicateOriginal{nullptr};
std::atomic<EmbeddedRouteIdentityPredicate>
    g_embeddedRouteAuthoredIdentityPredicateOriginal{nullptr};
std::atomic<EmbeddedRouteIdentityPredicate>
    g_embeddedRouteLocalIdentityPredicateOriginal{nullptr};
std::atomic<EmbeddedRouteIdentityProviderRecordLookup>
    g_embeddedRouteIdentityProviderRecordLookupOriginal{nullptr};
std::atomic<EmbeddedRouteIdentityProviderLookup>
    g_embeddedRouteIdentityProviderLookupOriginal{nullptr};
std::atomic_bool g_embeddedRouteIdentityProviderLookupInstallAttempted{};
std::atomic_bool g_embeddedRouteIdentityProviderLookupCodeDumped{};
std::atomic<EmbeddedRouteObjectTransition> g_embeddedRouteObjectTransitionOriginal{nullptr};
std::atomic<RouteAlternateSource> g_routeAlternateSourceOriginal{nullptr};
std::atomic<RouteAlternatePredicate> g_routeAlternatePredicateOriginal{nullptr};
std::atomic<RouteAlternateMode> g_routeAlternateModeOriginal{nullptr};
std::atomic<CurrentSelectionPublish> g_currentSelectionPublishOriginal{nullptr};
std::atomic<CurrentSelectionPublish> g_upstreamSelectionPublishOriginal{nullptr};
std::atomic<CurrentSelectionPublish> g_upstreamSelectionUpdateOriginal{nullptr};
std::atomic<std::byte*> g_currentSelectionManager{nullptr};
std::atomic<std::byte*> g_currentSelectionOwner{nullptr};
std::atomic_bool g_currentSelectionPublishInstallAttempted{};
std::atomic_uint32_t g_currentSelectionPublishObserved{};
std::atomic<RouteSelectionIngest> g_routeSelectionIngestOriginal{nullptr};
std::atomic<AuthoredLaunchDispatch> g_authoredLaunchDispatchOriginal{nullptr};
std::atomic<LaunchCommandInitialize> g_launchCommandInitializeOriginal{nullptr};
std::atomic<LaunchCommandDispatch> g_launchCommandDispatchOriginal{nullptr};
std::atomic<RouteModeSet> g_routeModeZeroSetOriginal{nullptr};
std::atomic<RouteModeSet> g_routeModeOneSetOriginal{nullptr};
std::atomic<OmegaRouteLifecycleQuery> g_omegaRouteLifecycleQueryOriginal{nullptr};
std::atomic<OmegaRouteLifecycleAccessor> g_omegaRouteLifecycleAccessorOriginal{nullptr};
std::atomic<RouteStateQuery> g_routeStateQueryOriginal{nullptr};
std::atomic<RouteStatePublish> g_routeStatePublishOriginal{nullptr};
std::atomic<ManagerLocalStart> g_managerLocalStartOriginal{nullptr};
std::atomic<ManagerAuthoredStart> g_managerAuthoredStartOriginal{nullptr};
std::atomic<ManagerSetupStage> g_managerSetupStageAOriginal{nullptr};
std::atomic<ManagerSetupStage> g_managerSetupStageBOriginal{nullptr};
std::atomic<ManagerSetupStage> g_managerSetupStageCOriginal{nullptr};
std::atomic<ManagerSetupStage> g_managerSetupStageDOriginal{nullptr};
std::atomic<ComponentDispatch> g_componentDispatchOriginal{nullptr};
std::atomic<ComponentLookup> g_componentLookupOriginal{nullptr};
std::atomic<ComponentRegister> g_componentRegisterOriginal{nullptr};
std::atomic<ComponentReuse> g_componentReuseOriginal{nullptr};
std::atomic<ComponentMapping> g_componentMappingOriginal{nullptr};
std::atomic<ComponentBuild> g_componentBuildOriginal{nullptr};
std::atomic<ComponentStep> g_componentStepOriginal{nullptr};
std::atomic<ComponentTick> g_componentTickOriginal{nullptr};
std::atomic<PostComponentSync> g_postComponentSyncOriginal{nullptr};
std::atomic_uint32_t g_transitionObserved{};
std::atomic_uint32_t g_slotObserved{};
std::atomic_uint32_t g_slotIdentityOneObserved{};
std::atomic_uint32_t g_modeObserved{};
std::atomic_uint32_t g_authoritySlotMapObserved{};
std::atomic_uint32_t g_authorityApplyObserved{};
std::atomic_uint32_t g_authorityRefreshObserved{};
std::atomic_uint32_t g_authorityRefreshGateObserved{};
std::atomic_uint32_t g_authorityBootstrapObserved{};
std::atomic_uint32_t g_authoritySnapshotBootstrapObserved{};
thread_local std::byte* g_authorityRefreshManager{};
thread_local bool g_authorityRefreshRequested{};
std::atomic_uint32_t g_managerLookupObserved{};
std::atomic_uint32_t g_lifecycleEventObserved{};
std::atomic_uint32_t g_managerEnsureObserved{};
std::atomic_uint32_t g_identityRequestObserved{};
std::atomic_uint32_t g_identityEnableObserved{};
std::atomic_uint32_t g_identityDescriptorCopyObserved{};
std::atomic_uint32_t g_sessionDescriptionStageObserved{};
std::atomic_uint32_t g_managedSessionMigrationObserved{};
std::atomic_uint32_t g_managerTableSnapshotObserved{};
std::atomic_uint32_t g_transitionDispatchObserved{};
std::atomic_uint32_t g_wireDispatchObserved{};
std::atomic_uint32_t g_activityClientUpdateObserved{};
std::atomic_uint32_t g_activityClientPumpObserved{};
std::atomic_uint32_t g_activityReceiverLookupObserved{};
std::atomic_uint32_t g_activityRosterApplyObserved{};
std::atomic_uint32_t g_activityPeerDispatchObserved{};
std::atomic_uint32_t g_activityReceiverActivateObserved{};
std::atomic_uint32_t g_activityReceiverBindObserved{};
std::atomic_uint32_t g_activityReceiverCreatorObserved{};
std::atomic_uint32_t g_managerUpdateLoopObserved{};
std::atomic_uint32_t g_managerUpdateLoopIdentityOneObserved{};
std::atomic_uint32_t g_managerUpdateLoopIdentityTwoObserved{};
std::atomic_uint32_t g_managerUpdateGateIdentityOneObserved{};
std::atomic_uint32_t g_managerUpdateGateIdentityTwoObserved{};
std::atomic_uint32_t g_managerModeSetObserved{};
std::atomic_uint32_t g_routeDescriptorObserved{};
std::array<std::atomic_uint64_t, 3> g_routeDescriptorLastSignature{};
std::atomic_uint32_t g_routeCommitObserved{};
std::atomic_uint32_t g_routeCommitOpeningObserved{};
std::atomic_uint32_t g_routeUpdateObserved{};
std::atomic_uint32_t g_routeUpdateOpeningObserved{};
std::atomic_uint32_t g_defaultRouteZeroInitializerObserved{};
std::atomic_uint32_t g_defaultRouteZeroInitializerDeferred{};
std::atomic_uint32_t g_defaultRouteReferencePredicateObserved{};
std::atomic_uint32_t g_defaultRoutePairPredicateObserved{};
std::atomic_uint32_t g_defaultRouteRequiredPredicateObserved{};
std::atomic_uint32_t g_defaultRouteCrossManagerPredicateObserved{};
std::atomic_uint32_t g_laneOneReadyScanObserved{};
std::atomic_uint32_t g_laneOneActiveCountObserved{};
std::atomic_uint32_t g_laneOneSelectionReadyObserved{};
std::atomic_uint32_t g_laneOneAuthoredInitializerObserved{};
std::atomic_uint32_t g_routeFlagUpdateObserved{};
std::atomic_uint32_t g_routeElapsedTimeObserved{};
std::atomic_uint32_t g_routeTimeoutConfigObserved{};
std::atomic_uint32_t g_routeAlternateUpdateObserved{};
std::atomic_uint32_t g_routeStateEvaluatorObserved{};
std::atomic_uint32_t g_routeParentDispatchObserved{};
std::atomic<std::byte*> g_routeParentState{nullptr};
std::atomic<std::byte*> g_embeddedRouteLaneZeroState{nullptr};
std::atomic_uint32_t g_routeSelectorVariantLookupObserved{};
std::atomic_uint32_t g_assignmentProviderUpdateObserved{};
std::atomic_uint32_t g_assignmentProviderStateConsumerObserved{};
std::atomic_uint32_t g_matchmakingLanePolicyMaterializerObserved{};
std::atomic_uint32_t g_matchmakingLaneRebuildObserved{};
std::atomic_uint32_t g_assignmentWatcherUpdateObserved{};
std::atomic_uint32_t g_assignmentWatcherOpeningObserved{};
std::atomic_uint32_t g_routeStateArmObserved{};
std::atomic_uint32_t g_embeddedRouteStatePhaseSetObserved{};
std::atomic_uint32_t g_embeddedRouteStateInitializeObserved{};
std::atomic_uint32_t g_embeddedRouteStateResetObserved{};
std::atomic_uint32_t g_embeddedRouteLaneZeroDriverObserved{};
std::atomic_uint32_t g_embeddedRouteLaneZeroDriverOpeningObserved{};
std::atomic_uint32_t g_embeddedRouteTransitionHandlerObserved{};
std::atomic_uint32_t g_embeddedRouteSessionActivityObserved{};
std::atomic_uint32_t g_embeddedRouteStateStatusObserved{};
std::atomic_uint32_t g_embeddedRouteContextSkipPredicateObserved{};
std::atomic_uint32_t g_embeddedRouteLaneAvailablePredicateObserved{};
std::atomic_uint32_t g_embeddedRouteAuthoredIdentityPredicateObserved{};
std::atomic_uint32_t g_embeddedRouteLocalIdentityPredicateObserved{};
std::atomic_uint32_t g_embeddedRouteIdentityProviderRecordLookupObserved{};
std::atomic_uint32_t g_embeddedRouteIdentityProviderLookupObserved{};
std::atomic_uint32_t g_embeddedRouteIdentityProviderControlObserved{};
/** 0 untested, 1 missing, 2 valid-but-refused, 3 valid-and-accepted. */
std::atomic_uint32_t g_embeddedRouteProvider266Outcome{};
std::atomic_uint32_t g_embeddedRouteObjectTransitionObserved{};
std::atomic_bool g_omegaForestRouteTraceArmed{};
std::atomic<std::byte*> g_omegaRouteOwner{nullptr};
std::atomic_bool g_omegaRouteEntrySnapshotDumped{};
std::atomic_bool g_omegaRouteState4SnapshotDumped{};
std::atomic<std::byte*> g_omegaRoutePreviousOwner{nullptr};
std::atomic_int32_t g_omegaRoutePreviousCurrent{INT32_MIN};
std::atomic_int32_t g_omegaRoutePreviousCurrentDetail{INT32_MIN};
std::atomic_int32_t g_omegaRoutePreviousPending{INT32_MIN};
std::atomic_int32_t g_omegaRoutePreviousPendingDetail{INT32_MIN};
std::atomic_bool g_embeddedRouteObject282PredicateDumped{};
std::atomic_bool g_embeddedRouteObject282ActionDumped{};
std::atomic_uint32_t g_routeAlternateDecisionObserved{};
std::atomic_int32_t g_routePhaseTimeoutMs{-1};
std::atomic_int32_t g_routeSelectionTimeoutMs{-1};
std::atomic_bool g_routePhaseTimeoutPassedObserved{};
std::atomic_bool g_routeSelectionTimeoutPassedObserved{};
std::atomic_uint32_t g_routeSelectionIngestObserved{};
std::atomic_uint32_t g_authoredLaunchDispatchObserved{};
std::atomic_uint32_t g_launchCommandInitializeObserved{};
std::atomic_uint32_t g_launchCommandDispatchObserved{};
std::atomic_uint8_t g_launchProducerGuardLast{0xFFU};
std::atomic_uint32_t g_launchProducerWindowState{};
std::atomic_uint32_t g_launchProducerWindowTicks{};
std::atomic_uint32_t g_launchProducerWindowReassertions{};
std::atomic_bool g_launchProducerSelectionReady{};
alignas(16) std::array<std::byte, 48> g_launchProducerSelectionDescriptor{};
std::atomic_bool g_launchProducerSelectionDescriptorReady{};
std::atomic_bool g_launchProducerReadinessRequestAttempted{};
std::atomic_uint32_t g_launchProducerEntryObserved{};
std::atomic_uint64_t g_launchProducerEntryLastSignature{~std::uint64_t{0}};
std::atomic_uint32_t g_launchProducerReadyCallbackObserved{};
std::atomic_uint32_t g_activityEvent46Observed{};
std::atomic_uint32_t g_activityEvent46RegisteredProducerObserved{};
std::atomic_uint32_t g_activityEvent46ProducerResetObserved{};
std::atomic_uint32_t g_activityEvent46ProducerInitializeObserved{};
std::atomic_uint32_t g_activityEvent46ProducerAvailableObserved{};
std::atomic_uint32_t g_activityEvent46ProducerRequestObserved{};
std::atomic_uint32_t g_activityEvent46ProducerTickObserved{};
std::atomic_uint32_t g_activityEvent46RequestFlowObserved{};
std::atomic_uint32_t g_activityEvent46ActionDispatchObserved{};
std::atomic_uint32_t g_activityEvent46RequestPredicateObserved{};
std::atomic_uint32_t g_routeModeZeroSetObserved{};
std::atomic_uint32_t g_routeModeOneSetObserved{};
std::atomic_uint32_t g_omegaRouteLifecycleQueryObserved{};
std::atomic_int32_t g_omegaRouteLifecycleQueryLast{INT32_MIN};
std::atomic_uint32_t g_omegaRouteLifecycleAccessorObserved{};
std::array<std::atomic_uintptr_t, 64> g_omegaRouteLifecycleAccessorCallers{};
std::atomic_uint32_t g_routeStateQueryObserved{};
std::atomic_uint64_t g_routeStateQueryLastSignature{};
std::atomic_uint32_t g_routeStatePublishObserved{};
std::atomic_uint32_t g_managerStartObserved{};
std::atomic_uint32_t g_managerIdentityOneModeTransitionObserved{};
std::atomic_int32_t g_managerIdentityOneLastMode{INT32_MIN};
std::atomic<std::byte*> g_managerIdentityOneLastPointer{nullptr};
std::atomic_uint32_t g_managerIdentityTwoComponentTransitionObserved{};
std::atomic_int32_t g_managerIdentityTwoLastComponent{INT32_MIN};
std::atomic<std::byte*> g_managerIdentityTwoLastPointer{nullptr};
std::atomic_uint32_t g_managerSetupStageObserved{};
std::atomic_bool g_componentDispatchAttempted{};
std::atomic_bool g_activitySetupComplete{};
std::atomic_bool g_activityWorldStarted{};
std::atomic<std::byte*> g_activityClientCandidate{nullptr};
std::atomic<std::byte*> g_joinedActivityClient{nullptr};
std::atomic_bool g_activityReceiverSnapshotAttempted{};
std::atomic_uint32_t g_componentDispatchIdentityOneObserved{};
std::atomic_uint32_t g_componentDispatchIdentityTwoObserved{};
std::atomic_uint32_t g_componentLookupObserved{};
std::atomic_uint32_t g_componentLookupIdentityOneObserved{};
std::atomic_uint32_t g_componentLookupIdentityTwoObserved{};
std::atomic_uint32_t g_componentRegisterObserved{};
std::atomic_uint32_t g_componentRegisterIdentityTwoObserved{};
std::atomic_uint32_t g_componentReuseObserved{};
std::atomic_uint32_t g_componentReuseIdentityTwoObserved{};
std::atomic_uint32_t g_componentMappingObserved{};
std::atomic_uint32_t g_componentMappingIdentityTwoObserved{};
std::atomic_uint32_t g_componentBuildObserved{};
std::atomic_uint32_t g_componentBuildIdentityTwoObserved{};
std::atomic_uint32_t g_componentSlotStateObserved{};
std::atomic_uint64_t g_componentSlotStateLastSignature{~std::uint64_t{0}};
std::atomic_uint64_t g_componentSlotStateIdentityTwoLastSignature{~std::uint64_t{0}};
std::atomic_uint32_t g_componentStepObserved{};
std::atomic_uint32_t g_componentStepIdentityTwoObserved{};
std::atomic_uint32_t g_componentTickObserved{};
std::atomic_uint32_t g_postComponentSyncObserved{};
std::atomic_bool g_bootstrapRequestAttempted{};
std::atomic_bool g_towerfallLifecycleStarted{};
std::atomic<std::byte*> g_omegaLifecycleManager{nullptr};
std::atomic_bool g_omegaLifecycleStarted{};
std::atomic_bool g_omegaLifecycleComplete{};
std::atomic_bool g_omegaPendingConsumerAttempted{};
std::atomic_uint32_t g_omegaLifecycleCalls{};
std::atomic_uint32_t g_omegaStageTwoStalls{};
std::atomic_uint64_t g_omegaStageTwoFirstTick{};

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

/** Saves runtime-decrypted script code so the post-lookup activation branch can be mapped. */
void dump_runtime_code(const wchar_t* phase,
                       const std::byte* target,
                       std::uintptr_t captureSize) noexcept {
    if (phase == nullptr || target == nullptr || captureSize == 0U) {
        return;
    }
    const auto begin = reinterpret_cast<std::uintptr_t>(target);
    const auto end = begin + captureSize;
    const auto image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    if (image == 0U || begin < image || end <= begin) {
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
        static_cast<unsigned long long>(begin - image),
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
    const DWORD size = static_cast<DWORD>(captureSize);
    DWORD written = 0;
    const bool complete = WriteFile(file, target, size, &written, nullptr) != FALSE
                          && written == size && FlushFileBuffers(file) != FALSE;
    (void)CloseHandle(file);

    std::array<char, core::log::kLineCapacity> line{};
    const int lineLength = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_runtime_code phase=%ls rva=0x%llX bytes=%lu result=%s",
        phase,
        static_cast<unsigned long long>(begin - image),
        static_cast<unsigned long>(written),
        complete ? "ok" : "write");
    if (lineLength > 0) {
        core::log::write(core::log::Channel::client,
                         complete ? core::log::Level::info : core::log::Level::warn,
                         {line.data(), static_cast<std::size_t>(lineLength)});
    }
}

void snapshot_manager_table(std::string_view package) noexcept;

/** @return True only for the retained Homecoming authority-slot experiment. */
[[nodiscard]] bool local_solo_authority_is_forced(std::string_view package) noexcept {
    return state::activity::forced::override_active()
           && package == "mission_towerfall";
}

void log_observation(const char* path,
                     std::uint32_t observation,
                     bool result,
                     std::byte* manager,
                     std::uint64_t arg1,
                     std::uint64_t arg2,
                     std::uint64_t arg3,
                     std::int32_t modeBefore,
                     std::int32_t modeAfter) noexcept {
    state::activity::forced::ForcedDestination forced{};
    state::activity::forced::snapshot(forced);
    const std::string_view package(forced.packageName.data(), forced.packageNameLength);
    const bool opening = state::activity::forced::override_active()
                         && (package == "cine_110_twr"
                             || local_solo_authority_is_forced(package));
    if ((!opening && observation > 64U) || (opening && observation > 512U)) {
        return;
    }
    const std::int32_t activity =
        manager != nullptr ? *reinterpret_cast<const std::int32_t*>(manager + 0x850) : -1;
    const std::int32_t variant =
        manager != nullptr ? *reinterpret_cast<const std::int32_t*>(manager + 0x854) : -1;
    const std::int32_t selected =
        manager != nullptr ? *reinterpret_cast<const std::int32_t*>(manager + 0x87C) : -1;
    const std::int32_t registered =
        manager != nullptr ? *reinterpret_cast<const std::int32_t*>(manager + 0xE93C) : -1;
    const std::int32_t component =
        manager != nullptr ? *reinterpret_cast<const std::int32_t*>(manager + 0x1AF00) : -1;
    std::array<char, 384> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_upstream_probe path=%s n=%u result=%s manager=%p arg1=0x%llX arg2=0x%llX arg3=0x%llX mode_before=%d mode_after=%d activity=%d variant=%d selected=%d registered=%d component=%d forced=%.*s",
        path,
        observation,
        result ? "accepted" : "rejected",
        static_cast<void*>(manager),
        static_cast<unsigned long long>(arg1),
        static_cast<unsigned long long>(arg2),
        static_cast<unsigned long long>(arg3),
        modeBefore,
        modeAfter,
        activity,
        variant,
        selected,
        registered,
        component,
        static_cast<int>(package.size()),
        package.data());
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
}

__declspec(noinline) bool __fastcall transition_update(std::byte* manager,
                                                        const void* transition,
                                                        const void* update) noexcept {
    const std::int32_t before =
        manager != nullptr ? *reinterpret_cast<const std::int32_t*>(manager + 0x1AEF8) : -1;
    const TransitionUpdate original = g_transitionOriginal.load(std::memory_order_acquire);
    const bool result = original != nullptr && original(manager, transition, update);
    const std::int32_t after =
        manager != nullptr ? *reinterpret_cast<const std::int32_t*>(manager + 0x1AEF8) : -1;
    const std::uint32_t observation =
        g_transitionObserved.fetch_add(1, std::memory_order_relaxed) + 1U;
    log_observation("transition", observation, result, manager,
                    reinterpret_cast<std::uint64_t>(transition),
                    reinterpret_cast<std::uint64_t>(update), 0, before, after);
    return result;
}

__declspec(noinline) bool __fastcall slot_update(std::byte* manager,
                                                  std::int32_t slot,
                                                  std::int32_t mode,
                                                  std::int32_t value) noexcept {
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;
    const std::int32_t before =
        manager != nullptr ? *reinterpret_cast<const std::int32_t*>(manager + 0x1AEF8) : -1;
    const SlotUpdate original = g_slotOriginal.load(std::memory_order_acquire);
    const bool result = original != nullptr && original(manager, slot, mode, value);
    const std::int32_t after =
        manager != nullptr ? *reinterpret_cast<const std::int32_t*>(manager + 0x1AEF8) : -1;
    const std::uint32_t observation =
        g_slotObserved.fetch_add(1, std::memory_order_relaxed) + 1U;
    log_observation("slot", observation, result, manager,
                    static_cast<std::uint32_t>(slot), static_cast<std::uint32_t>(mode),
                    static_cast<std::uint32_t>(value), before, after);

    state::activity::forced::ForcedDestination forced{};
    state::activity::forced::snapshot(forced);
    const std::string_view package(forced.packageName.data(), forced.packageNameLength);
    const std::int32_t identity =
        manager != nullptr ? *reinterpret_cast<const std::int32_t*>(manager + 0x1C7C0) : -1;
    if (local_solo_authority_is_forced(package) && identity == 1) {
        const std::uint32_t identityObservation =
            g_slotIdentityOneObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
        if (identityObservation <= 128U) {
            const std::int32_t registrationIndex =
                *reinterpret_cast<const std::int32_t*>(manager + 0xE93C);
            const std::int32_t componentIndex =
                *reinterpret_cast<const std::int32_t*>(manager + 0x1AF00);
            std::array<char, 512> line{};
            const int length = std::snprintf(
                line.data(),
                line.size(),
                "ev=bootflow stage=activity_script_identity1_slot_update n=%u result=%s caller_rva=0x%llX manager=%p slot=%d requested_mode=%d value=%d manager_mode_before=%d manager_mode_after=%d registration_index=%d component_index=%d forced=%.*s",
                identityObservation,
                result ? "accepted" : "rejected",
                static_cast<unsigned long long>(callerRva),
                static_cast<void*>(manager),
                slot,
                mode,
                value,
                before,
                after,
                registrationIndex,
                componentIndex,
                static_cast<int>(package.size()),
                package.data());
            if (length > 0) {
                core::log::write(core::log::Channel::client,
                                 core::log::Level::info,
                                 {line.data(), static_cast<std::size_t>(length)});
            }
        }
    }
    return result;
}

__declspec(noinline) bool __fastcall mode_update(std::byte* manager,
                                                  std::int32_t slot,
                                                  std::int32_t mode) noexcept {
    const std::int32_t before =
        manager != nullptr ? *reinterpret_cast<const std::int32_t*>(manager + 0x1AEF8) : -1;
    const ModeUpdate original = g_modeOriginal.load(std::memory_order_acquire);
    const bool result = original != nullptr && original(manager, slot, mode);
    const std::int32_t after =
        manager != nullptr ? *reinterpret_cast<const std::int32_t*>(manager + 0x1AEF8) : -1;
    const std::uint32_t observation =
        g_modeObserved.fetch_add(1, std::memory_order_relaxed) + 1U;
    log_observation("mode", observation, result, manager,
                    static_cast<std::uint32_t>(slot), static_cast<std::uint32_t>(mode), 0,
                    before, after);
    return result;
}

__declspec(noinline) bool __fastcall authority_slot_map(std::byte* context,
                                                         std::byte* runtime,
                                                         std::byte* entries,
                                                         std::int32_t* mapping) noexcept {
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0;
    const std::uint32_t runtimeLimit =
        runtime != nullptr ? *reinterpret_cast<const std::uint32_t*>(runtime + 0x21C) : 0;
    const std::uint8_t runtimeMode =
        runtime != nullptr ? *reinterpret_cast<const std::uint8_t*>(runtime + 0x2AC) : 0;
    const std::int32_t entryCount =
        entries != nullptr ? *reinterpret_cast<const std::int32_t*>(entries) : -1;
    const AuthoritySlotMap original = g_authoritySlotMapOriginal.load(std::memory_order_acquire);
    const bool result = original != nullptr && original(context, runtime, entries, mapping);

    std::array<std::int32_t, 8> values{};
    values.fill(INT32_MIN);
    std::uint32_t sentinelCount = 0U;
    if (mapping != nullptr && entryCount > 0) {
        const std::int32_t inspected = entryCount < 32 ? entryCount : 32;
        for (std::int32_t index = 0; index < inspected; ++index) {
            if (mapping[index] == -2) {
                ++sentinelCount;
            }
            if (index < static_cast<std::int32_t>(values.size())) {
                values[static_cast<std::size_t>(index)] = mapping[index];
            }
        }
    }

    state::activity::forced::ForcedDestination forced{};
    state::activity::forced::snapshot(forced);
    const std::string_view package(forced.packageName.data(), forced.packageNameLength);
    const bool opening = state::activity::forced::override_active()
                         && (package == "cine_110_twr"
                             || local_solo_authority_is_forced(package));
    const std::uint32_t observation =
        g_authoritySlotMapObserved.fetch_add(1, std::memory_order_relaxed) + 1U;
    if ((opening && observation <= 512U) || callerRva == 0x16F81C1U) {
        std::array<char, 512> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_authority_slot_map_probe n=%u result=%s caller_rva=0x%llX context=%p runtime=%p entries=%p mapping=%p runtime_limit=%u runtime_mode=%u entry_count=%d sentinel_minus2=%u mapping_first8=%d,%d,%d,%d,%d,%d,%d,%d forced=%.*s",
            observation,
            result ? "accepted" : "rejected",
            static_cast<unsigned long long>(callerRva),
            static_cast<void*>(context),
            static_cast<void*>(runtime),
            static_cast<void*>(entries),
            static_cast<void*>(mapping),
            runtimeLimit,
            static_cast<unsigned int>(runtimeMode),
            entryCount,
            sentinelCount,
            values[0],
            values[1],
            values[2],
            values[3],
            values[4],
            values[5],
            values[6],
            values[7],
            static_cast<int>(package.size()),
            package.data());
        if (length > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(length)});
        }
    }
    if (opening) {
        snapshot_manager_table(package);
    }
    return result;
}

/**
 * Observes the native roster-to-script-slot apply boundary. The function itself builds the
 * mapping array and only calls slot_update when authority_slot_map emits sentinel -2.
 */
__declspec(noinline) bool __fastcall authority_apply(std::byte* runtime,
                                                       std::byte* entries,
                                                       std::byte* context) noexcept {
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;
    const std::int32_t entryCount =
        entries != nullptr ? *reinterpret_cast<const std::int32_t*>(entries) : -1;
    const std::uint32_t runtimeLimit =
        runtime != nullptr ? *reinterpret_cast<const std::uint32_t*>(runtime + 0x21C) : 0U;
    const std::uint8_t runtimeMode =
        runtime != nullptr ? *reinterpret_cast<const std::uint8_t*>(runtime + 0x2AC) : 0U;
    std::array<std::int32_t, 8> slots{};
    slots.fill(INT32_MIN);
    if (entries != nullptr && entryCount > 0) {
        const std::int32_t inspected =
            entryCount < static_cast<std::int32_t>(slots.size())
                ? entryCount
                : static_cast<std::int32_t>(slots.size());
        for (std::int32_t index = 0; index < inspected; ++index) {
            slots[static_cast<std::size_t>(index)] =
                *reinterpret_cast<const std::int32_t*>(entries + 0xCU + (index * 0x10U));
        }
    }

    const AuthorityApply original = g_authorityApplyOriginal.load(std::memory_order_acquire);
    const bool result = original != nullptr && original(runtime, entries, context);

    state::activity::forced::ForcedDestination forced{};
    state::activity::forced::snapshot(forced);
    const std::string_view package(forced.packageName.data(), forced.packageNameLength);
    if (local_solo_authority_is_forced(package)) {
        const std::uint32_t observation =
            g_authorityApplyObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
        if (observation <= 128U) {
            std::array<char, 608> line{};
            const int length = std::snprintf(
                line.data(),
                line.size(),
                "ev=bootflow stage=activity_authority_apply_probe n=%u result=%s caller_rva=0x%llX runtime=%p entries=%p context=%p runtime_limit=%u runtime_mode=%u entry_count=%d entry_slots_first8=%d,%d,%d,%d,%d,%d,%d,%d forced=%.*s",
                observation,
                result ? "accepted" : "rejected",
                static_cast<unsigned long long>(callerRva),
                static_cast<void*>(runtime),
                static_cast<void*>(entries),
                static_cast<void*>(context),
                runtimeLimit,
                static_cast<unsigned int>(runtimeMode),
                entryCount,
                slots[0],
                slots[1],
                slots[2],
                slots[3],
                slots[4],
                slots[5],
                slots[6],
                slots[7],
                static_cast<int>(package.size()),
                package.data());
            if (length > 0) {
                core::log::write(core::log::Channel::client,
                                 core::log::Level::info,
                                 {line.data(), static_cast<std::size_t>(length)});
            }
        }
    }
    return result;
}

void report_authority_entries_gate(const char* gate,
                                   bool result,
                                   std::byte* entries) noexcept {
    std::byte* const manager = g_authorityRefreshManager;
    if (manager == nullptr
        || *reinterpret_cast<const std::int32_t*>(manager + 0x1C7C0) != 1) {
        return;
    }
    state::activity::forced::ForcedDestination forced{};
    state::activity::forced::snapshot(forced);
    const std::string_view package(forced.packageName.data(), forced.packageNameLength);
    if (!local_solo_authority_is_forced(package)) {
        return;
    }

    const std::uint32_t observation =
        g_authorityRefreshGateObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
    if (observation > 256U) {
        return;
    }
    const std::int32_t entryCount =
        entries != nullptr ? *reinterpret_cast<const std::int32_t*>(entries) : -1;
    std::array<std::int32_t, 8> statuses{};
    statuses.fill(INT32_MIN);
    if (entries != nullptr && entryCount > 0) {
        const std::int32_t inspected =
            entryCount < static_cast<std::int32_t>(statuses.size())
                ? entryCount
                : static_cast<std::int32_t>(statuses.size());
        for (std::int32_t index = 0; index < inspected; ++index) {
            statuses[static_cast<std::size_t>(index)] =
                *reinterpret_cast<const std::int32_t*>(entries + 4U + (index * 0x10U));
        }
    }
    std::array<char, 512> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_authority_refresh_gate n=%u gate=%s result=%s manager=%p refresh_requested=%u entries=%p entry_count=%d statuses_first8=%d,%d,%d,%d,%d,%d,%d,%d forced=%.*s",
        observation,
        gate,
        result ? "pass" : "stop",
        static_cast<void*>(manager),
        g_authorityRefreshRequested ? 1U : 0U,
        static_cast<void*>(entries),
        entryCount,
        statuses[0],
        statuses[1],
        statuses[2],
        statuses[3],
        statuses[4],
        statuses[5],
        statuses[6],
        statuses[7],
        static_cast<int>(package.size()),
        package.data());
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
}

__declspec(noinline) bool __fastcall authority_any_assigned_gate(std::byte* entries) noexcept {
    std::byte* const manager = g_authorityRefreshManager;
    state::activity::forced::ForcedDestination forced{};
    state::activity::forced::snapshot(forced);
    const std::string_view package(forced.packageName.data(), forced.packageNameLength);
    const bool localSoloAuthority = local_solo_authority_is_forced(package);
    const std::int32_t identity =
        manager != nullptr
            ? *reinterpret_cast<const std::int32_t*>(manager + 0x1C7C0)
            : -1;
    const std::int32_t entryCount =
        entries != nullptr ? *reinterpret_cast<const std::int32_t*>(entries) : -1;
    auto* const firstStatus =
        entries != nullptr ? reinterpret_cast<std::int32_t*>(entries + 4U) : nullptr;
    const std::int32_t statusBefore = firstStatus != nullptr ? *firstStatus : INT32_MIN;

    // The local solo manager intentionally initializes its only assignment byte to -1. The
    // scheduler then requires any_assigned before it calls authority_apply, which is the sole
    // native path that can commit a resolved assignment back into the manager. Bootstrap the
    // one-record local mission to slot zero at that exact boundary; all other identities,
    // activities, table shapes, statuses, and non-refresh passes remain native/pass-through.
    bool bootstrapped = false;
    if (localSoloAuthority && g_authorityRefreshRequested && identity == 1 && entryCount == 1
        && statusBefore == -1) {
        *firstStatus = 0;
        bootstrapped = true;
        const std::uint32_t observation =
            g_authorityBootstrapObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
        if (observation <= 32U) {
            const std::int32_t recordWord1 =
                *reinterpret_cast<const std::int32_t*>(entries + 8U);
            const std::int32_t recordWord2 =
                *reinterpret_cast<const std::int32_t*>(entries + 0xCU);
            const std::int32_t recordWord3 =
                *reinterpret_cast<const std::int32_t*>(entries + 0x10U);
            std::array<char, 448> line{};
            const int length = std::snprintf(
                line.data(),
                line.size(),
                "ev=bootflow stage=activity_authority_assignment_bootstrap n=%u result=slot0 manager=%p identity=%d refresh_requested=1 entries=%p entry_count=1 status_before=-1 status_after=0 record_words=%d,%d,%d mutation=local_solo_slot0 forced=%.*s",
                observation,
                static_cast<void*>(manager),
                identity,
                static_cast<void*>(entries),
                recordWord1,
                recordWord2,
                recordWord3,
                static_cast<int>(package.size()),
                package.data());
            if (length > 0) {
                core::log::write(core::log::Channel::client,
                                 core::log::Level::info,
                                 {line.data(), static_cast<std::size_t>(length)});
            }
        }
    }

    const AuthorityEntriesGate original =
        g_authorityAnyAssignedGateOriginal.load(std::memory_order_acquire);
    const bool result = original != nullptr && original(entries);
    report_authority_entries_gate("any_assigned", result, entries);
    if (bootstrapped && !result) {
        core::log::write(
            core::log::Channel::client,
            core::log::Level::warn,
            "ev=bootflow stage=activity_authority_assignment_bootstrap result=rejected reason=native_any_assigned_gate");
    }
    return result;
}

__declspec(noinline) bool __fastcall authority_any_unassigned_gate(std::byte* entries) noexcept {
    const AuthorityEntriesGate original =
        g_authorityAnyUnassignedGateOriginal.load(std::memory_order_acquire);
    const bool result = original != nullptr && original(entries);
    report_authority_entries_gate("any_unassigned", result, entries);
    return result;
}

__declspec(noinline) bool __fastcall authority_snapshot_gate(std::byte* runtime,
                                                               std::byte* snapshot) noexcept {
    std::byte* const manager = g_authorityRefreshManager;
    state::activity::forced::ForcedDestination forced{};
    state::activity::forced::snapshot(forced);
    const std::string_view package(forced.packageName.data(), forced.packageNameLength);
    const bool localSoloAuthority = local_solo_authority_is_forced(package);
    const std::int32_t identity =
        manager != nullptr
            ? *reinterpret_cast<const std::int32_t*>(manager + 0x1C7C0)
            : -1;

    // Once the local solo assignment table contains slot zero, the scheduler takes the
    // snapshot-resolved branch before authority_apply. A freshly constructed native snapshot
    // still carries its initializer sentinels, so that gate cannot pass and the resolver that
    // would commit the actual slot is never reached. Seed only the exact one-entry, identity-1,
    // solo mission snapshot observed at that deadlock. Leave actualSlot at -1 so authority_apply
    // remains the native owner of the commit.
    bool bootstrapped = false;
    if (localSoloAuthority && g_authorityRefreshRequested && identity == 1 && runtime != nullptr
        && snapshot != nullptr && manager != nullptr && snapshot == manager + 0x860
        && *reinterpret_cast<const std::uint8_t*>(runtime) != 0U
        && *reinterpret_cast<const std::uint8_t*>(runtime + 0x228) == 1U
        && *reinterpret_cast<const std::uint8_t*>(runtime + 0x22B) == 0U
        && *reinterpret_cast<const std::uint32_t*>(snapshot + 0x3B5C) == 1U
        && *reinterpret_cast<const std::int8_t*>(snapshot + 0x3C32) == -1
        && *reinterpret_cast<const std::int8_t*>(snapshot + 0x3C68) == -1
        && *reinterpret_cast<const std::int8_t*>(snapshot + 0x3C69) == -1
        && *reinterpret_cast<const std::int8_t*>(snapshot + 0x3C6B) == -1) {
        *reinterpret_cast<std::uint8_t*>(runtime + 0x22B) = 1U;
        *reinterpret_cast<std::int8_t*>(snapshot + 0x3C32) = 0;
        *reinterpret_cast<std::int8_t*>(snapshot + 0x3C69) = 0;
        *reinterpret_cast<std::int8_t*>(snapshot + 0x3C6B) = 0;
        bootstrapped = true;

        // Orbit and pre-resolution traffic can exhaust the focused trace budgets before the
        // native authority resolver commits slot zero. Re-open only observation budgets at the
        // exact snapshot transition so the next identity-1 manager/client branch is visible.
        g_lifecycleEventObserved.store(0U, std::memory_order_release);
        g_transitionDispatchObserved.store(0U, std::memory_order_release);
        g_wireDispatchObserved.store(0U, std::memory_order_release);
        g_activityClientUpdateObserved.store(0U, std::memory_order_release);
        g_activityClientPumpObserved.store(0U, std::memory_order_release);
        g_activityReceiverLookupObserved.store(0U, std::memory_order_release);
        g_activityRosterApplyObserved.store(0U, std::memory_order_release);
        g_activityPeerDispatchObserved.store(0U, std::memory_order_release);
        g_activityReceiverActivateObserved.store(0U, std::memory_order_release);
        g_activityReceiverBindObserved.store(0U, std::memory_order_release);
        g_activityReceiverCreatorObserved.store(0U, std::memory_order_release);
        g_managerUpdateLoopObserved.store(0U, std::memory_order_release);
        g_managerUpdateLoopIdentityOneObserved.store(0U, std::memory_order_release);
        g_managerUpdateGateIdentityOneObserved.store(0U, std::memory_order_release);
        g_componentDispatchIdentityOneObserved.store(0U, std::memory_order_release);
        g_componentLookupObserved.store(0U, std::memory_order_release);
        g_componentLookupIdentityOneObserved.store(0U, std::memory_order_release);
        g_componentRegisterObserved.store(0U, std::memory_order_release);
        g_componentReuseObserved.store(0U, std::memory_order_release);
        g_componentMappingObserved.store(0U, std::memory_order_release);
        g_componentBuildObserved.store(0U, std::memory_order_release);
        g_componentSlotStateObserved.store(0U, std::memory_order_release);
        g_componentSlotStateLastSignature.store(~std::uint64_t{0},
                                                std::memory_order_release);
        g_componentSlotStateIdentityTwoLastSignature.store(~std::uint64_t{0},
                                                           std::memory_order_release);
        g_componentStepObserved.store(0U, std::memory_order_release);
        g_componentTickObserved.store(0U, std::memory_order_release);
        g_postComponentSyncObserved.store(0U, std::memory_order_release);

        const std::uint32_t observation =
            g_authoritySnapshotBootstrapObserved.fetch_add(1U,
                                                            std::memory_order_relaxed)
            + 1U;
        if (observation <= 32U) {
            const std::int8_t timer =
                *reinterpret_cast<const std::int8_t*>(snapshot + 0x3C6A);
            std::array<char, 512> line{};
            const int length = std::snprintf(
                line.data(),
                line.size(),
                "ev=bootflow stage=activity_authority_snapshot_bootstrap n=%u result=seeded manager=%p identity=1 refresh_requested=1 runtime=%p snapshot=%p active_mask=0x1 runtime_enabled=0->1 desired_slot=-1->0 actual_slot=-1 state=-1->0 timer=%d ack=-1->0 mutation=local_solo_snapshot_slot0 post_authority_trace=armed forced=%.*s",
                observation,
                static_cast<void*>(manager),
                static_cast<void*>(runtime),
                static_cast<void*>(snapshot),
                static_cast<int>(timer),
                static_cast<int>(package.size()),
                package.data());
            if (length > 0) {
                core::log::write(core::log::Channel::client,
                                 core::log::Level::info,
                                 {line.data(), static_cast<std::size_t>(length)});
            }
        }
    }

    const AuthoritySnapshotGate original =
        g_authoritySnapshotGateOriginal.load(std::memory_order_acquire);
    const bool result = original != nullptr && original(runtime, snapshot);
    if (manager == nullptr || identity != 1) {
        return result;
    }
    if (!localSoloAuthority) {
        return result;
    }
    const std::uint32_t observation =
        g_authorityRefreshGateObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
    if (observation <= 256U) {
        const std::uint8_t enabled =
            runtime != nullptr ? *reinterpret_cast<const std::uint8_t*>(runtime + 0x22B) : 0U;
        const std::uint8_t runtimeMode =
            runtime != nullptr ? *reinterpret_cast<const std::uint8_t*>(runtime + 0x2AC) : 0U;
        std::array<char, 416> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_authority_refresh_gate n=%u gate=snapshot_resolved result=%s manager=%p refresh_requested=%u runtime=%p snapshot=%p runtime_enabled=%u runtime_mode=%u forced=%.*s",
            observation,
            result ? "pass" : "stop",
            static_cast<void*>(manager),
            g_authorityRefreshRequested ? 1U : 0U,
            static_cast<void*>(runtime),
            static_cast<void*>(snapshot),
            static_cast<unsigned int>(enabled),
            static_cast<unsigned int>(runtimeMode),
            static_cast<int>(package.size()),
            package.data());
        if (length > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(length)});
        }
    }
    if (bootstrapped && !result) {
        core::log::write(
            core::log::Channel::client,
            core::log::Level::warn,
            "ev=bootflow stage=activity_authority_snapshot_bootstrap result=rejected reason=native_snapshot_gate");
    }
    return result;
}

/** Observes the scheduler that decides whether synchronized components reach authority_apply. */
__declspec(noinline) void __fastcall authority_refresh(std::byte* manager,
                                                         bool refresh) noexcept {
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;
    const std::uint32_t gatesBefore =
        g_authorityRefreshGateObserved.load(std::memory_order_acquire);
    const std::uint32_t appliesBefore =
        g_authorityApplyObserved.load(std::memory_order_acquire);
    std::byte* const previousManager = g_authorityRefreshManager;
    const bool previousRequested = g_authorityRefreshRequested;
    g_authorityRefreshManager = manager;
    g_authorityRefreshRequested = refresh;
    const AuthorityRefresh original = g_authorityRefreshOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(manager, refresh);
    }
    g_authorityRefreshManager = previousManager;
    g_authorityRefreshRequested = previousRequested;

    state::activity::forced::ForcedDestination forced{};
    state::activity::forced::snapshot(forced);
    const std::string_view package(forced.packageName.data(), forced.packageNameLength);
    const std::int32_t identity =
        manager != nullptr ? *reinterpret_cast<const std::int32_t*>(manager + 0x1C7C0) : -1;
    if (!local_solo_authority_is_forced(package) || identity != 1) {
        return;
    }
    const std::uint32_t observation =
        g_authorityRefreshObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
    if (observation <= 256U) {
        const std::uint32_t gatesAfter =
            g_authorityRefreshGateObserved.load(std::memory_order_acquire);
        const std::uint32_t appliesAfter =
            g_authorityApplyObserved.load(std::memory_order_acquire);
        const std::int32_t mode =
            *reinterpret_cast<const std::int32_t*>(manager + 0x1AEF8);
        const std::int32_t activity =
            *reinterpret_cast<const std::int32_t*>(manager + 0x850);
        const std::int32_t variant =
            *reinterpret_cast<const std::int32_t*>(manager + 0x854);
        std::array<char, 448> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_authority_refresh_probe n=%u caller_rva=0x%llX manager=%p identity=%d mode=%d activity=%d variant=%d refresh_requested=%u gate_calls=%u apply_calls=%u forced=%.*s",
            observation,
            static_cast<unsigned long long>(callerRva),
            static_cast<void*>(manager),
            identity,
            mode,
            activity,
            variant,
            refresh ? 1U : 0U,
            gatesAfter - gatesBefore,
            appliesAfter - appliesBefore,
            static_cast<int>(package.size()),
            package.data());
        if (length > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(length)});
        }
    }
}

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

struct SessionDescriptionSnapshot {
    std::array<std::uint64_t, kSessionDescriptionBytes / sizeof(std::uint64_t)> words{};
    std::uint32_t nonzeroBytes{};
    std::uint64_t hash{1469598103934665603ULL};
};

struct RetainedSessionDescription {
    std::array<std::uint64_t, kSessionDescriptionBytes / sizeof(std::uint64_t)> words{};
    std::int32_t identity{-1};
    std::uint32_t nonzeroBytes{};
    std::uint64_t hash{};
    bool valid{};
};

SRWLOCK g_retainedSessionDescriptionLock = SRWLOCK_INIT;
RetainedSessionDescription g_retainedSessionDescription{};

void retain_session_description(std::int32_t identity,
                                const SessionDescriptionSnapshot& snapshot) noexcept {
    AcquireSRWLockExclusive(&g_retainedSessionDescriptionLock);
    g_retainedSessionDescription.words = snapshot.words;
    g_retainedSessionDescription.identity = identity;
    g_retainedSessionDescription.nonzeroBytes = snapshot.nonzeroBytes;
    g_retainedSessionDescription.hash = snapshot.hash;
    g_retainedSessionDescription.valid = true;
    ReleaseSRWLockExclusive(&g_retainedSessionDescriptionLock);
}

[[nodiscard]] bool take_retained_session_description(
    std::int32_t identity,
    RetainedSessionDescription& retained) noexcept {
    bool matched = false;
    AcquireSRWLockExclusive(&g_retainedSessionDescriptionLock);
    if (g_retainedSessionDescription.valid
        && g_retainedSessionDescription.identity == identity) {
        retained = g_retainedSessionDescription;
        g_retainedSessionDescription.valid = false;
        matched = true;
    }
    ReleaseSRWLockExclusive(&g_retainedSessionDescriptionLock);
    return matched;
}

[[nodiscard]] SessionDescriptionSnapshot snapshot_session_description(
    const std::byte* descriptor) noexcept {
    SessionDescriptionSnapshot snapshot{};
    for (std::size_t index = 0; index < kSessionDescriptionBytes; ++index) {
        const std::uint8_t value = safe_read<std::uint8_t>(
            descriptor != nullptr ? descriptor + index : nullptr, 0U);
        snapshot.nonzeroBytes += value != 0U ? 1U : 0U;
        snapshot.hash ^= value;
        snapshot.hash *= 1099511628211ULL;
    }
    for (std::size_t index = 0; index < snapshot.words.size(); ++index) {
        snapshot.words[index] = safe_read<std::uint64_t>(
            descriptor != nullptr ? descriptor + index * sizeof(std::uint64_t) : nullptr, 0U);
    }
    return snapshot;
}

void log_session_description_snapshot(std::string_view stage,
                                      std::uint32_t observation,
                                      std::int32_t identity,
                                      const std::byte* definition,
                                      std::string_view slot,
                                      const SessionDescriptionSnapshot& snapshot,
                                      std::string_view package) noexcept {
    std::array<char, core::log::kLineCapacity> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=%.*s n=%u identity=%d definition=%p slot=%.*s nonzero=%u hash=0x%016llX words=0x%016llX,0x%016llX,0x%016llX,0x%016llX,0x%016llX,0x%016llX,0x%016llX,0x%016llX,0x%016llX,0x%016llX,0x%016llX,0x%016llX,0x%016llX,0x%016llX,0x%016llX,0x%016llX forced=%.*s",
        static_cast<int>(stage.size()),
        stage.data(),
        observation,
        identity,
        static_cast<const void*>(definition),
        static_cast<int>(slot.size()),
        slot.data(),
        snapshot.nonzeroBytes,
        static_cast<unsigned long long>(snapshot.hash),
        static_cast<unsigned long long>(snapshot.words[0]),
        static_cast<unsigned long long>(snapshot.words[1]),
        static_cast<unsigned long long>(snapshot.words[2]),
        static_cast<unsigned long long>(snapshot.words[3]),
        static_cast<unsigned long long>(snapshot.words[4]),
        static_cast<unsigned long long>(snapshot.words[5]),
        static_cast<unsigned long long>(snapshot.words[6]),
        static_cast<unsigned long long>(snapshot.words[7]),
        static_cast<unsigned long long>(snapshot.words[8]),
        static_cast<unsigned long long>(snapshot.words[9]),
        static_cast<unsigned long long>(snapshot.words[10]),
        static_cast<unsigned long long>(snapshot.words[11]),
        static_cast<unsigned long long>(snapshot.words[12]),
        static_cast<unsigned long long>(snapshot.words[13]),
        static_cast<unsigned long long>(snapshot.words[14]),
        static_cast<unsigned long long>(snapshot.words[15]),
        static_cast<int>(package.size()),
        package.data());
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
}

[[nodiscard]] bool opening_is_forced(std::string_view& package) noexcept {
    static thread_local state::activity::forced::ForcedDestination forced{};
    state::activity::forced::snapshot(forced);
    package = std::string_view(forced.packageName.data(), forced.packageNameLength);
    return state::activity::forced::override_active()
           && (package == "cine_110_twr" || package == "mission_towerfall"
               || package == "mission_scot");
}

/** @return True for destinations whose native embedded-route graph is being traced. */
[[nodiscard]] bool embedded_route_trace_is_forced(std::string_view& package) noexcept {
    return opening_is_forced(package)
           && (package == "mission_towerfall" || package == "mission_scot");
}

struct AssignmentTableSnapshot {
    std::int32_t count{-1};
    std::array<std::int32_t, 4> first{INT32_MIN, INT32_MIN, INT32_MIN, INT32_MIN};
};

[[nodiscard]] AssignmentTableSnapshot snapshot_assignment_table(
    const std::byte* assignment) noexcept {
    AssignmentTableSnapshot snapshot{};
    const std::byte* const entries = assignment != nullptr ? assignment + 0x28U : nullptr;
    snapshot.count = safe_read<std::int32_t>(entries, -1);
    if (snapshot.count > 0) {
        for (std::size_t index = 0; index < snapshot.first.size(); ++index) {
            snapshot.first[index] = safe_read<std::int32_t>(
                entries + 4U + index * sizeof(std::int32_t), INT32_MIN);
        }
    }
    return snapshot;
}

/** Observes the native watcher whose armed/revision gate owns the provider update call. */
__declspec(noinline) void __fastcall assignment_watcher_update(std::byte* watcher,
                                                                std::byte* manager) noexcept {
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;
    const std::int32_t selector = safe_read<std::int32_t>(watcher, -1);
    const std::int32_t identity = safe_read<std::int32_t>(
        manager != nullptr ? manager + 0x1C7C0U : nullptr, -1);
    const std::int32_t managerPhase = safe_read<std::int32_t>(
        manager != nullptr ? manager + 0x854U : nullptr, -1);
    const std::int32_t cachedBefore = safe_read<std::int32_t>(
        watcher != nullptr ? watcher + 4U : nullptr, INT32_MIN);
    const std::uint8_t flagsBefore = safe_read<std::uint8_t>(
        watcher != nullptr ? watcher + 8U : nullptr, 0U);
    std::int32_t revisionBefore = INT32_MIN;
    const auto revision = image != nullptr
                              ? reinterpret_cast<AssignmentRevision>(image + kAssignmentRevisionRva)
                              : nullptr;
    __try {
        if (revision != nullptr && selector >= 0) {
            revisionBefore = revision(selector);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        revisionBefore = INT32_MIN;
    }

    const AssignmentWatcherUpdate original =
        g_assignmentWatcherUpdateOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(watcher, manager);
    }

    const std::int32_t cachedAfter = safe_read<std::int32_t>(
        watcher != nullptr ? watcher + 4U : nullptr, INT32_MIN);
    const std::uint8_t flagsAfter = safe_read<std::uint8_t>(
        watcher != nullptr ? watcher + 8U : nullptr, 0U);
    std::int32_t revisionAfter = INT32_MIN;
    __try {
        if (revision != nullptr && selector >= 0) {
            revisionAfter = revision(selector);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        revisionAfter = INT32_MIN;
    }

    std::string_view package{};
    const bool opening = opening_is_forced(package);
    const std::uint32_t observation =
        g_assignmentWatcherUpdateObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
    const std::uint32_t openingObservation = opening && package == "mission_towerfall"
                                                 ? g_assignmentWatcherOpeningObserved.fetch_add(
                                                       1U, std::memory_order_relaxed)
                                                       + 1U
                                                 : 0U;
    if (observation <= 128U || (openingObservation > 0U && openingObservation <= 128U)) {
            std::array<char, 640> line{};
            const int length = std::snprintf(
                line.data(),
                line.size(),
                "ev=bootflow stage=activity_assignment_watcher_update n=%u opening_n=%u caller_rva=0x%llX watcher=%p manager=%p selector=%d identity=%d manager_phase=%d flags_before=0x%02X armed_before=%u cached_before=%d revision_before=%d differs_before=%u flags_after=0x%02X armed_after=%u cached_after=%d revision_after=%d differs_after=%u mutation=observe_only forced=%.*s",
                observation,
                openingObservation,
                static_cast<unsigned long long>(callerRva),
                static_cast<void*>(watcher),
                static_cast<void*>(manager),
                selector,
                identity,
                managerPhase,
                static_cast<unsigned int>(flagsBefore),
                static_cast<unsigned int>((flagsBefore & 1U) != 0U),
                cachedBefore,
                revisionBefore,
                static_cast<unsigned int>(cachedBefore != revisionBefore),
                static_cast<unsigned int>(flagsAfter),
                static_cast<unsigned int>((flagsAfter & 1U) != 0U),
                cachedAfter,
                revisionAfter,
                static_cast<unsigned int>(cachedAfter != revisionAfter),
                static_cast<int>(package.size()),
                package.data());
            if (length > 0) {
                core::log::write(core::log::Channel::client,
                                 core::log::Level::info,
                                 {line.data(), static_cast<std::size_t>(length)});
            }
    }
}

/** Observes the route-selector provider method that creates/resolves the authority assignment. */
__declspec(noinline) bool __fastcall assignment_provider_update(std::byte* context,
                                                                 std::int32_t selector,
                                                                 std::int32_t identity,
                                                                 std::byte* variant,
                                                                 std::byte* assignment) noexcept {
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;
    const AssignmentTableSnapshot before = snapshot_assignment_table(assignment);
    const std::uint32_t variantWord = safe_read<std::uint32_t>(variant, 0U);
    const std::uint8_t assignmentReady = safe_read<std::uint8_t>(
        assignment != nullptr ? assignment + 0x2A2U : nullptr, 0U);
    const std::int32_t assignmentIndex = safe_read<std::int32_t>(
        assignment != nullptr ? assignment + 0x2A8U : nullptr, -1);

    const AssignmentProviderUpdate original =
        g_assignmentProviderUpdateOriginal.load(std::memory_order_acquire);
    const bool result = original != nullptr
                        && original(context, selector, identity, variant, assignment);
    const AssignmentTableSnapshot after = snapshot_assignment_table(assignment);

    std::string_view package{};
    const bool opening = opening_is_forced(package);
    if (opening && package == "mission_towerfall") {
        const std::uint32_t observation =
            g_assignmentProviderUpdateObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
        if (observation <= 128U) {
            std::array<char, 704> line{};
            const int length = std::snprintf(
                line.data(),
                line.size(),
                "ev=bootflow stage=activity_assignment_provider_update n=%u result=%s caller_rva=0x%llX context=%p selector=%d identity=%d variant=%p variant_word=0x%08X assignment=%p assignment_ready=%u assignment_index=%d count_before=%d count_after=%d first_before=%d,%d,%d,%d first_after=%d,%d,%d,%d mutation=observe_only forced=%.*s",
                observation,
                result ? "accepted" : "rejected",
                static_cast<unsigned long long>(callerRva),
                static_cast<void*>(context),
                selector,
                identity,
                static_cast<void*>(variant),
                variantWord,
                static_cast<void*>(assignment),
                static_cast<unsigned int>(assignmentReady),
                assignmentIndex,
                before.count,
                after.count,
                before.first[0],
                before.first[1],
                before.first[2],
                before.first[3],
                after.first[0],
                after.first[1],
                after.first[2],
                after.first[3],
                static_cast<int>(package.size()),
                package.data());
            if (length > 0) {
                core::log::write(core::log::Channel::client,
                                 core::log::Level::info,
                                 {line.data(), static_cast<std::size_t>(length)});
            }
        }
    }
    return result;
}

/** Observes the native state-2 session-search result decoder. */
__declspec(noinline) std::byte* __fastcall assignment_provider_state_consumer(
    std::byte* state,
    std::byte* value,
    std::int32_t* resultOut) noexcept {
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;
    const std::int32_t stateBefore = safe_read<std::int32_t>(state, -1);
    const std::uint8_t presentBefore = safe_read<std::uint8_t>(
        state != nullptr ? state + 0x20U : nullptr, 0U);
    const std::int32_t countBefore = safe_read<std::int32_t>(
        state != nullptr ? state + 0x28U : nullptr, -1);
    const std::int32_t resultBefore = safe_read<std::int32_t>(resultOut, -1);
    std::array<std::uint32_t, 8> firstEntry{};
    for (std::size_t index = 0; index < firstEntry.size(); ++index) {
        firstEntry[index] = safe_read<std::uint32_t>(
            state != nullptr && countBefore > 0
                ? state + 0x120U + index * sizeof(std::uint32_t)
                : nullptr,
            UINT32_MAX);
    }

    const AssignmentProviderStateConsumer original =
        g_assignmentProviderStateConsumerOriginal.load(std::memory_order_acquire);
    std::byte* const returned =
        original != nullptr ? original(state, value, resultOut) : value;

    const std::int32_t stateAfter = safe_read<std::int32_t>(state, -1);
    const std::uint8_t presentAfter = safe_read<std::uint8_t>(
        state != nullptr ? state + 0x20U : nullptr, 0U);
    const std::int32_t countAfter = safe_read<std::int32_t>(
        state != nullptr ? state + 0x28U : nullptr, -1);
    const std::int32_t resultAfter = safe_read<std::int32_t>(resultOut, -1);
    const std::uint32_t observation =
        g_assignmentProviderStateConsumerObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
    if (observation <= 64U) {
        std::array<char, core::log::kLineCapacity> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_session_search_result_consumer n=%u caller_rva=0x%llX state_ptr=%p value=%p returned=%p state_before=%d state_after=%d present_before=%u present_after=%u count_before=%d count_after=%d result_before=%d result_after=%d first_entry=%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X mutation=observe_only",
            observation,
            static_cast<unsigned long long>(callerRva),
            static_cast<void*>(state),
            static_cast<void*>(value),
            static_cast<void*>(returned),
            stateBefore,
            stateAfter,
            static_cast<unsigned int>(presentBefore),
            static_cast<unsigned int>(presentAfter),
            countBefore,
            countAfter,
            resultBefore,
            resultAfter,
            firstEntry[0],
            firstEntry[1],
            firstEntry[2],
            firstEntry[3],
            firstEntry[4],
            firstEntry[5],
            firstEntry[6],
            firstEntry[7]);
        if (length > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(length)});
        }
    }
    return returned;
}

/**
 * Observes the verified configuration-to-lane-record materializer. The optional nested lane
 * policy is present at configuration +0x9A; its provider entries begin at +0xBC and are copied
 * into the native lane record at +0x08.
 */
__declspec(noinline) void __fastcall matchmaking_lane_policy_materializer(
    std::byte* configuration,
    std::byte* descriptor,
    std::byte* laneRecord) noexcept {
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;
    const std::uint8_t descriptorPresent = safe_read<std::uint8_t>(
        descriptor != nullptr ? descriptor + 0x1CU : nullptr, 0U);
    const std::uint16_t descriptorId = safe_read<std::uint16_t>(descriptor, UINT16_MAX);
    const std::uint8_t nestedPresent = safe_read<std::uint8_t>(
        configuration != nullptr ? configuration + 0x9AU : nullptr, 0U);
    const std::uint32_t nestedScalarOne = safe_read<std::uint32_t>(
        configuration != nullptr ? configuration + 0xA4U : nullptr, UINT32_MAX);
    const std::uint32_t nestedScalarTwo = safe_read<std::uint32_t>(
        configuration != nullptr ? configuration + 0xACU : nullptr, UINT32_MAX);
    const std::int32_t nestedCount = safe_read<std::int32_t>(
        configuration != nullptr ? configuration + 0xB0U : nullptr, -1);
    const std::uint8_t nestedFlag = safe_read<std::uint8_t>(
        configuration != nullptr ? configuration + 0x479U : nullptr, 0U);
    std::array<std::uint32_t, 4> firstConfigurationEntry{};
    for (std::size_t index = 0; index < firstConfigurationEntry.size(); ++index) {
        firstConfigurationEntry[index] = safe_read<std::uint32_t>(
            configuration != nullptr ? configuration + 0xBCU + index * 8U : nullptr,
            UINT32_MAX);
    }

    const MatchmakingLanePolicyMaterializer original =
        g_matchmakingLanePolicyMaterializerOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(configuration, descriptor, laneRecord);
    }

    const std::uint32_t observation =
        g_matchmakingLanePolicyMaterializerObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
    if (observation <= 64U) {
        std::array<char, core::log::kLineCapacity> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_matchmaking_lane_policy_materializer n=%u caller_rva=0x%llX configuration=%p descriptor=%p lane_record=%p descriptor_id=%u descriptor_present=%u nested_present=%u nested_scalar1=%u nested_scalar2=%u nested_count=%d nested_flag=%u config_entry0=%08X,%08X,%08X,%08X lane_valid=%u lane_count=%d lane_scalar1=%u lane_scalar2=%u lane_flag=%u lane_entry0=%08X,%08X,%08X,%08X mutation=observe_only",
            observation,
            static_cast<unsigned long long>(callerRva),
            static_cast<void*>(configuration),
            static_cast<void*>(descriptor),
            static_cast<void*>(laneRecord),
            static_cast<unsigned int>(descriptorId),
            static_cast<unsigned int>(descriptorPresent),
            static_cast<unsigned int>(nestedPresent),
            nestedScalarOne,
            nestedScalarTwo,
            nestedCount,
            static_cast<unsigned int>(nestedFlag),
            firstConfigurationEntry[0],
            firstConfigurationEntry[1],
            firstConfigurationEntry[2],
            firstConfigurationEntry[3],
            static_cast<unsigned int>(safe_read<std::uint8_t>(laneRecord, 0U)),
            safe_read<std::int32_t>(laneRecord != nullptr ? laneRecord + 0x04U : nullptr, -1),
            safe_read<std::uint32_t>(laneRecord != nullptr ? laneRecord + 0x1E8U : nullptr, 0U),
            safe_read<std::uint32_t>(laneRecord != nullptr ? laneRecord + 0x1ECU : nullptr, 0U),
            static_cast<unsigned int>(safe_read<std::uint8_t>(
                laneRecord != nullptr ? laneRecord + 0x1F0U : nullptr, 0U)),
            safe_read<std::uint32_t>(laneRecord != nullptr ? laneRecord + 0x08U : nullptr, 0U),
            safe_read<std::uint32_t>(laneRecord != nullptr ? laneRecord + 0x0CU : nullptr, 0U),
            safe_read<std::uint32_t>(laneRecord != nullptr ? laneRecord + 0x10U : nullptr, 0U),
            safe_read<std::uint32_t>(laneRecord != nullptr ? laneRecord + 0x14U : nullptr, 0U));
        if (length > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(length)});
        }
    }
}

/** Observes native rebuilds of the three cached matchmaking lane records. */
__declspec(noinline) void __fastcall matchmaking_lane_rebuild(std::byte* context) noexcept {
    constexpr std::size_t kLaneCount = 3U;
    constexpr std::size_t kLaneStride = 0x388U;
    std::array<std::uint16_t, kLaneCount> tagsBefore{};
    std::array<std::int32_t, kLaneCount> generationsBefore{};
    for (std::size_t lane = 0; lane < kLaneCount; ++lane) {
        tagsBefore[lane] = safe_read<std::uint16_t>(
            context != nullptr ? context + 0x868U + lane * kLaneStride : nullptr,
            UINT16_MAX);
        generationsBefore[lane] = safe_read<std::int32_t>(
            context != nullptr ? context + 0xBE8U + lane * kLaneStride : nullptr,
            -1);
    }

    const MatchmakingLaneRebuild original =
        g_matchmakingLaneRebuildOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(context);
    }

    std::array<std::uint16_t, kLaneCount> tagsAfter{};
    std::array<std::int32_t, kLaneCount> generationsAfter{};
    std::array<std::uint8_t, kLaneCount> validAfter{};
    std::array<std::int32_t, kLaneCount> countsAfter{};
    bool changed = false;
    for (std::size_t lane = 0; lane < kLaneCount; ++lane) {
        std::byte* const laneRecord =
            context != nullptr ? context + 0x888U + lane * kLaneStride : nullptr;
        tagsAfter[lane] = safe_read<std::uint16_t>(
            context != nullptr ? context + 0x868U + lane * kLaneStride : nullptr,
            UINT16_MAX);
        generationsAfter[lane] = safe_read<std::int32_t>(
            context != nullptr ? context + 0xBE8U + lane * kLaneStride : nullptr,
            -1);
        validAfter[lane] = safe_read<std::uint8_t>(laneRecord, 0U);
        countsAfter[lane] = safe_read<std::int32_t>(
            laneRecord != nullptr ? laneRecord + 0x04U : nullptr, -1);
        changed = changed || tagsBefore[lane] != tagsAfter[lane]
                  || generationsBefore[lane] != generationsAfter[lane];
    }
    const std::uint32_t observation =
        g_matchmakingLaneRebuildObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
    if (observation <= 16U || changed) {
        std::array<char, core::log::kLineCapacity> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_matchmaking_lane_rebuild n=%u context=%p changed=%u tag_before=%u,%u,%u tag_after=%u,%u,%u generation_before=%d,%d,%d generation_after=%d,%d,%d valid_after=%u,%u,%u count_after=%d,%d,%d mutation=observe_only",
            observation,
            static_cast<void*>(context),
            changed ? 1U : 0U,
            static_cast<unsigned int>(tagsBefore[0]),
            static_cast<unsigned int>(tagsBefore[1]),
            static_cast<unsigned int>(tagsBefore[2]),
            static_cast<unsigned int>(tagsAfter[0]),
            static_cast<unsigned int>(tagsAfter[1]),
            static_cast<unsigned int>(tagsAfter[2]),
            generationsBefore[0],
            generationsBefore[1],
            generationsBefore[2],
            generationsAfter[0],
            generationsAfter[1],
            generationsAfter[2],
            static_cast<unsigned int>(validAfter[0]),
            static_cast<unsigned int>(validAfter[1]),
            static_cast<unsigned int>(validAfter[2]),
            countsAfter[0],
            countsAfter[1],
            countsAfter[2]);
        if (length > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(length)});
        }
    }
}

struct RouteModeSnapshot {
    bool modeZero{};
    bool modeOne{};
};

/** Reads the two native predicates which gate creation of an authored route descriptor. */
[[nodiscard]] RouteModeSnapshot query_route_modes() noexcept {
    RouteModeSnapshot snapshot{};
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    if (image == nullptr) {
        return snapshot;
    }
    const auto queryZero = reinterpret_cast<RouteModeQuery>(image + kRouteModeZeroQueryRva);
    const auto queryOne = reinterpret_cast<RouteModeQuery>(image + kRouteModeOneQueryRva);
    __try {
        snapshot.modeZero = queryZero();
        snapshot.modeOne = queryOne();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        snapshot = {};
    }
    return snapshot;
}

/** Captures the six global activity-script manager slots as the opening initializes. */
void snapshot_manager_table(std::string_view package) noexcept {
    const std::uint32_t observation =
        g_managerTableSnapshotObserved.fetch_add(1, std::memory_order_relaxed) + 1U;
    if (observation > 32U) {
        return;
    }
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    std::byte** table = nullptr;
    IdentityDefinition identityDefinition = nullptr;
    if (image != nullptr) {
        const auto tableAccessor = reinterpret_cast<ManagerTable>(image + kManagerTableRva);
        __try {
            table = tableAccessor();
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            table = nullptr;
        }
        identityDefinition = reinterpret_cast<IdentityDefinition>(
            validated_target(kIdentityDefinitionRva, kIdentityDefinitionPrefix));
    }
    for (std::uint32_t index = 0; index < 6U; ++index) {
        std::byte* const manager =
            safe_read<std::byte*>(table != nullptr ? table + index : nullptr, nullptr);
        const std::int32_t mode =
            safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AEF8 : nullptr, -1);
        const std::int32_t component =
            safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AF00 : nullptr, -1);
        const std::int32_t identity =
            safe_read<std::int32_t>(manager != nullptr ? manager + 0x1C7C0 : nullptr, -1);
        const std::int32_t activity =
            safe_read<std::int32_t>(manager != nullptr ? manager + 0x850 : nullptr, -1);
        const std::int32_t selected =
            safe_read<std::int32_t>(manager != nullptr ? manager + 0x87C : nullptr, -1);
        const std::int32_t registered =
            safe_read<std::int32_t>(manager != nullptr ? manager + 0xE93C : nullptr, -1);
        const std::uint32_t managerFlags =
            safe_read<std::uint32_t>(manager != nullptr ? manager + 0x1AEE8 : nullptr, 0U);
        const std::int32_t lifecycleState =
            safe_read<std::int32_t>(manager != nullptr ? manager + 0x1C820 : nullptr, -1);
        const std::uint64_t lifecycleBegin =
            safe_read<std::uint64_t>(manager != nullptr ? manager + 0x1C888 : nullptr, 0U);
        const std::uint64_t lifecycleUpdate =
            safe_read<std::uint64_t>(manager != nullptr ? manager + 0x1C890 : nullptr, 0U);
        std::byte* const nested =
            safe_read<std::byte*>(manager != nullptr ? manager + 0x1AE10 : nullptr, nullptr);

        std::byte* definition = nullptr;
        if (identityDefinition != nullptr && identity >= 0) {
            __try {
                definition = identityDefinition(identity);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                definition = nullptr;
            }
        }
        const std::uint32_t definitionFlags =
            safe_read<std::uint32_t>(definition != nullptr ? definition + 0x4 : nullptr, 0U);
        const std::uint16_t definitionState =
            safe_read<std::uint16_t>(definition != nullptr ? definition + 0xA : nullptr, 0U);
        const void* const definitionContext =
            safe_read<const void*>(definition != nullptr ? definition + 0x18 : nullptr, nullptr);
        const std::int32_t definitionActivity =
            safe_read<std::int32_t>(definition != nullptr ? definition + 0x24 : nullptr, -1);
        const std::uint16_t definitionRequest =
            safe_read<std::uint16_t>(definition != nullptr ? definition + 0x2C : nullptr, 0U);
        const std::uint8_t definitionEnabled =
            safe_read<std::uint8_t>(definition != nullptr ? definition + 0x94C : nullptr, 0U);
        const std::uint8_t definitionPending =
            safe_read<std::uint8_t>(definition != nullptr ? definition + 0x94D : nullptr, 0U);

        std::array<char, 768> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_script_manager_table n=%u slot=%u table=%p manager=%p mode=%d component=%d identity=%d activity=%d selected=%d registered=%d manager_flags=0x%08X lifecycle_state=%d lifecycle_begin=0x%llX lifecycle_update=0x%llX nested=%p definition=%p definition_flags=0x%08X definition_state=0x%04X definition_context=%p definition_activity=%d definition_request=0x%04X definition_enabled=%u definition_pending=%u forced=%.*s",
            observation,
            index,
            static_cast<void*>(table),
            static_cast<void*>(manager),
            mode,
            component,
            identity,
            activity,
            selected,
            registered,
            managerFlags,
            lifecycleState,
            static_cast<unsigned long long>(lifecycleBegin),
            static_cast<unsigned long long>(lifecycleUpdate),
            static_cast<void*>(nested),
            static_cast<void*>(definition),
            definitionFlags,
            static_cast<unsigned int>(definitionState),
            definitionContext,
            definitionActivity,
            static_cast<unsigned int>(definitionRequest),
            static_cast<unsigned int>(definitionEnabled),
            static_cast<unsigned int>(definitionPending),
            static_cast<int>(package.size()),
            package.data());
        if (length > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(length)});
        }

    }
}

/** Observes the manager lifecycle gateway that owns the identity-request transition. */
__declspec(noinline) std::int32_t __fastcall lifecycle_event(std::byte* manager,
                                                              std::int32_t eventCode,
                                                              const void* payload) noexcept {
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0;
    const std::int32_t identity =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1C7C0 : nullptr, -1);
    const std::int32_t modeBefore =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AEF8 : nullptr, -1);
    const LifecycleEvent original =
        g_lifecycleEventOriginal.load(std::memory_order_acquire);
    const std::int32_t result =
        original != nullptr ? original(manager, eventCode, payload) : -1;
    const std::int32_t modeAfter =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AEF8 : nullptr, -1);
    std::string_view package{};
    if (!opening_is_forced(package)) {
        return result;
    }
    const std::uint32_t observation =
        g_lifecycleEventObserved.fetch_add(1, std::memory_order_relaxed) + 1U;
    if (observation <= 512U) {
        std::array<char, 320> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_script_lifecycle_event n=%u caller_rva=0x%llX manager=%p event=%d payload=%p result=%d identity=%d mode_before=%d mode_after=%d forced=%.*s",
            observation,
            static_cast<unsigned long long>(callerRva),
            static_cast<void*>(manager),
            eventCode,
            payload,
            result,
            identity,
            modeBefore,
            modeAfter,
            static_cast<int>(package.size()),
            package.data());
        if (length > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(length)});
        }
    }
    snapshot_manager_table(package);
    return result;
}

/** Observes the first entry point that requests an activity-script manager for an identity. */
__declspec(noinline) void __fastcall manager_ensure(std::int32_t identity) noexcept {
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0;
    const ManagerEnsure original = g_managerEnsureOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(identity);
    }
    std::string_view package{};
    const bool opening = opening_is_forced(package);
    const std::uint32_t observation =
        g_managerEnsureObserved.fetch_add(1, std::memory_order_relaxed) + 1U;
    if (!opening && observation > 128U) {
        return;
    }
    std::array<char, 384> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_manager_ensure n=%u caller_rva=0x%llX identity=%d forced=%.*s",
        observation,
        static_cast<unsigned long long>(callerRva),
        identity,
        static_cast<int>(package.size()),
        package.data());
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
}

/** Observes the selector that translates one identity descriptor into a manager request. */
__declspec(noinline) void __fastcall identity_request(std::int32_t identity,
                                                       const void* descriptor) noexcept {
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0;
    const std::uint64_t descriptor0 = safe_read<std::uint64_t>(descriptor, 0);
    const auto* const descriptorBytes = static_cast<const std::byte*>(descriptor);
    const std::uint64_t descriptor8 =
        safe_read<std::uint64_t>(descriptorBytes != nullptr ? descriptorBytes + 8 : nullptr, 0);
    const IdentityRequest original = g_identityRequestOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(identity, descriptor);
    }
    std::string_view package{};
    const bool opening = opening_is_forced(package);
    const std::uint32_t observation =
        g_identityRequestObserved.fetch_add(1, std::memory_order_relaxed) + 1U;
    if (!opening && observation > 128U) {
        return;
    }
    std::array<char, 320> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_identity_request n=%u caller_rva=0x%llX identity=%d descriptor=%p descriptor0=0x%llX descriptor8=0x%llX forced=%.*s",
        observation,
        static_cast<unsigned long long>(callerRva),
        identity,
        descriptor,
        static_cast<unsigned long long>(descriptor0),
        static_cast<unsigned long long>(descriptor8),
        static_cast<int>(package.size()),
        package.data());
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
}

/** Observes the identity-definition flag change used when no manager exists yet. */
__declspec(noinline) void __fastcall identity_enable(std::int32_t identity, bool local) noexcept {
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0;
    const IdentityEnable original = g_identityEnableOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(identity, local);
    }
    std::string_view package{};
    const bool opening = opening_is_forced(package);
    const std::uint32_t observation =
        g_identityEnableObserved.fetch_add(1, std::memory_order_relaxed) + 1U;
    if (!opening && observation > 128U) {
        return;
    }
    std::array<char, core::log::kLineCapacity> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_identity_enable n=%u caller_rva=0x%llX identity=%d local=%u forced=%.*s",
        observation,
        static_cast<unsigned long long>(callerRva),
        identity,
        local ? 1U : 0U,
        static_cast<int>(package.size()),
        package.data());
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
}

/** Observes the native writer that stages a 128-byte managed-session description. */
__declspec(noinline) void __fastcall session_description_stage(
    std::int32_t identity,
    const std::byte* descriptor) noexcept {
    std::string_view package{};
    const bool opening = opening_is_forced(package);
    const std::uint32_t observation =
        g_sessionDescriptionStageObserved.fetch_add(1, std::memory_order_relaxed) + 1U;

    std::byte* definition = nullptr;
    const auto identityDefinition = reinterpret_cast<IdentityDefinition>(
        validated_target(kIdentityDefinitionRva, kIdentityDefinitionPrefix));
    __try {
        definition = identityDefinition != nullptr && identity >= 0
                         ? identityDefinition(identity)
                         : nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        definition = nullptr;
    }

    const SessionDescriptionSnapshot input = snapshot_session_description(descriptor);
    const SessionDescriptionSnapshot currentBefore = snapshot_session_description(
        definition != nullptr ? definition + 0x57CU : nullptr);
    const SessionDescriptionSnapshot pendingBefore = snapshot_session_description(
        definition != nullptr ? definition + 0x94EU : nullptr);
    const std::uint8_t enabledBefore = safe_read<std::uint8_t>(
        definition != nullptr ? definition + 0x94CU : nullptr, 0U);
    const std::uint8_t pendingValidBefore = safe_read<std::uint8_t>(
        definition != nullptr ? definition + 0x94DU : nullptr, 0U);
    const std::uint16_t sourceFlagsBefore = safe_read<std::uint16_t>(
        definition != nullptr ? definition + 0x2CU : nullptr, 0U);
    const std::uint64_t inputOnlineA = safe_read<std::uint64_t>(
        descriptor != nullptr ? descriptor + kSessionDescriptionOnlineIdentityOffset : nullptr,
        0U);
    const std::uint64_t inputOnlineB = safe_read<std::uint64_t>(
        descriptor != nullptr
            ? descriptor + kSessionDescriptionOnlineIdentityOffset + sizeof(std::uint64_t)
            : nullptr,
        0U);
    const std::uint16_t inputOnlineTail = safe_read<std::uint16_t>(
        descriptor != nullptr
            ? descriptor + kSessionDescriptionOnlineIdentityOffset
                  + 2U * sizeof(std::uint64_t)
            : nullptr,
        0U);

    const SessionDescriptionStage original =
        g_sessionDescriptionStageOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(identity, descriptor);
    }

    if (!opening || observation > 64U) {
        return;
    }
    const SessionDescriptionSnapshot currentAfter = snapshot_session_description(
        definition != nullptr ? definition + 0x57CU : nullptr);
    const SessionDescriptionSnapshot pendingAfter = snapshot_session_description(
        definition != nullptr ? definition + 0x94EU : nullptr);
    const std::uint8_t enabledAfter = safe_read<std::uint8_t>(
        definition != nullptr ? definition + 0x94CU : nullptr, 0U);
    const std::uint8_t nativePendingValidAfter = safe_read<std::uint8_t>(
        definition != nullptr ? definition + 0x94DU : nullptr, 0U);
    const std::uint16_t sourceFlagsAfter = safe_read<std::uint16_t>(
        definition != nullptr ? definition + 0x2CU : nullptr, 0U);

    // The reset inside native staging clears definition+0x2C bit 0 before native snapshots it to
    // +0x94D. Preserve the valid entry-state decision only when the retained Homecoming descriptor
    // carries a nonzero online identity and native copied it but marked it invalid. This runs before
    // the caller can schedule the asynchronous consumer; native still owns the descriptor copy and
    // all subsequent commit/swap behavior.
    const bool validityBridgeEligible =
        opening && package == "mission_towerfall" && identity >= 0 && definition != nullptr
        && descriptor != nullptr && enabledBefore != 0U && pendingValidBefore == 0U
        && (sourceFlagsBefore & 1U) != 0U && (sourceFlagsAfter & 1U) == 0U
        && nativePendingValidAfter == 0U
        && (inputOnlineA != 0U || inputOnlineB != 0U || inputOnlineTail != 0U)
        && pendingAfter.hash == input.hash && pendingAfter.nonzeroBytes == input.nonzeroBytes;
    bool validityBridgeApplied = false;
    if (validityBridgeEligible) {
        __try {
            *(definition + 0x94DU) = std::byte{1U};
            validityBridgeApplied = safe_read<std::uint8_t>(definition + 0x94DU, 0U) == 1U;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            validityBridgeApplied = false;
        }
    }
    if (validityBridgeApplied) {
        retain_session_description(identity, input);
    }
    const std::uint8_t pendingValidAfter = safe_read<std::uint8_t>(
        definition != nullptr ? definition + 0x94DU : nullptr, 0U);
    std::array<char, 512> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_session_description_stage n=%u identity=%d definition=%p enabled_before=%u enabled_after=%u pending_valid_before=%u pending_valid_native_after=%u pending_valid_after=%u source_flags_before=0x%04X source_flags_after=0x%04X validity_bridge=%s current_flags=0x%04X pending_flags=0x%04X forced=%.*s",
        observation,
        identity,
        static_cast<void*>(definition),
        static_cast<unsigned int>(enabledBefore),
        static_cast<unsigned int>(enabledAfter),
        static_cast<unsigned int>(pendingValidBefore),
        static_cast<unsigned int>(nativePendingValidAfter),
        static_cast<unsigned int>(pendingValidAfter),
        static_cast<unsigned int>(sourceFlagsBefore),
        static_cast<unsigned int>(sourceFlagsAfter),
        validityBridgeApplied ? "applied" : "not_applied",
        static_cast<unsigned int>(safe_read<std::uint16_t>(
            definition != nullptr ? definition + 0x56CU : nullptr, 0U)),
        static_cast<unsigned int>(safe_read<std::uint16_t>(
            definition != nullptr ? definition + 0x94CU : nullptr, 0U)),
        static_cast<int>(package.size()),
        package.data());
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
    log_session_description_snapshot("activity_script_session_description_stage",
                                     observation,
                                     identity,
                                     definition,
                                     "input",
                                     input,
                                     package);
    log_session_description_snapshot("activity_script_session_description_stage",
                                     observation,
                                     identity,
                                     definition,
                                     "current_before",
                                     currentBefore,
                                     package);
    log_session_description_snapshot("activity_script_session_description_stage",
                                     observation,
                                     identity,
                                     definition,
                                     "pending_before",
                                     pendingBefore,
                                     package);
    log_session_description_snapshot("activity_script_session_description_stage",
                                     observation,
                                     identity,
                                     definition,
                                     "current_after",
                                     currentAfter,
                                     package);
    log_session_description_snapshot("activity_script_session_description_stage",
                                     observation,
                                     identity,
                                     definition,
                                     "pending_after",
                                     pendingAfter,
                                     package);
}

/** Observes the migration consumer whose invalid branch reaches the zero-identity Steam join. */
__declspec(noinline) void __fastcall managed_session_migration(
    std::int32_t identity,
    std::byte* definition) noexcept {
    std::string_view package{};
    const bool opening = opening_is_forced(package);
    const std::uint32_t observation =
        g_managedSessionMigrationObserved.fetch_add(1, std::memory_order_relaxed) + 1U;
    const std::uint8_t enabledBefore = safe_read<std::uint8_t>(
        definition != nullptr ? definition + 0x94CU : nullptr, 0U);
    const std::uint8_t pendingValidBefore = safe_read<std::uint8_t>(
        definition != nullptr ? definition + 0x94DU : nullptr, 0U);
    const std::uint16_t currentFlagsBefore = safe_read<std::uint16_t>(
        definition != nullptr ? definition + 0x56CU : nullptr, 0U);
    const SessionDescriptionSnapshot currentBefore = snapshot_session_description(
        definition != nullptr ? definition + 0x57CU : nullptr);
    const SessionDescriptionSnapshot pendingBefore = snapshot_session_description(
        definition != nullptr ? definition + 0x94EU : nullptr);

    RetainedSessionDescription retained{};
    const bool descriptorBridgeEligible =
        opening && package == "mission_towerfall" && identity >= 0 && definition != nullptr
        && enabledBefore == 0U && pendingValidBefore == 1U && pendingBefore.nonzeroBytes == 0U;
    bool descriptorBridgeApplied = false;
    if (descriptorBridgeEligible && take_retained_session_description(identity, retained)
        && retained.nonzeroBytes != 0U) {
        __try {
            std::memcpy(definition + 0x94EU,
                        retained.words.data(),
                        kSessionDescriptionBytes);
            const SessionDescriptionSnapshot restored = snapshot_session_description(
                definition + 0x94EU);
            descriptorBridgeApplied = restored.hash == retained.hash
                                      && restored.nonzeroBytes == retained.nonzeroBytes;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            descriptorBridgeApplied = false;
        }
    }
    const SessionDescriptionSnapshot pendingPrecall = snapshot_session_description(
        definition != nullptr ? definition + 0x94EU : nullptr);

    const ManagedSessionMigration original =
        g_managedSessionMigrationOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(identity, definition);
    }

    if (!opening || observation > 64U) {
        return;
    }
    const std::uint8_t enabledAfter = safe_read<std::uint8_t>(
        definition != nullptr ? definition + 0x94CU : nullptr, 0U);
    const std::uint8_t pendingValidAfter = safe_read<std::uint8_t>(
        definition != nullptr ? definition + 0x94DU : nullptr, 0U);
    const std::uint16_t currentFlagsAfter = safe_read<std::uint16_t>(
        definition != nullptr ? definition + 0x56CU : nullptr, 0U);
    const SessionDescriptionSnapshot currentAfter = snapshot_session_description(
        definition != nullptr ? definition + 0x57CU : nullptr);
    const SessionDescriptionSnapshot pendingAfter = snapshot_session_description(
        definition != nullptr ? definition + 0x94EU : nullptr);
    std::array<char, 512> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_managed_session_migration n=%u identity=%d definition=%p role=%d enabled_before=%u enabled_after=%u pending_valid_before=%u pending_valid_after=%u descriptor_bridge=%s current_flags_before=0x%04X current_flags_after=0x%04X forced=%.*s",
        observation,
        identity,
        static_cast<void*>(definition),
        safe_read<std::int32_t>(definition, -1),
        static_cast<unsigned int>(enabledBefore),
        static_cast<unsigned int>(enabledAfter),
        static_cast<unsigned int>(pendingValidBefore),
        static_cast<unsigned int>(pendingValidAfter),
        descriptorBridgeApplied ? "applied" : "not_applied",
        static_cast<unsigned int>(currentFlagsBefore),
        static_cast<unsigned int>(currentFlagsAfter),
        static_cast<int>(package.size()),
        package.data());
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
    log_session_description_snapshot("activity_script_managed_session_migration",
                                     observation,
                                     identity,
                                     definition,
                                     "current_before",
                                     currentBefore,
                                     package);
    log_session_description_snapshot("activity_script_managed_session_migration",
                                     observation,
                                     identity,
                                     definition,
                                     "pending_before",
                                     pendingBefore,
                                     package);
    log_session_description_snapshot("activity_script_managed_session_migration",
                                     observation,
                                     identity,
                                     definition,
                                     "pending_precall",
                                     pendingPrecall,
                                     package);
    log_session_description_snapshot("activity_script_managed_session_migration",
                                     observation,
                                     identity,
                                     definition,
                                     "current_after",
                                     currentAfter,
                                     package);
    log_session_description_snapshot("activity_script_managed_session_migration",
                                     observation,
                                     identity,
                                     definition,
                                     "pending_after",
                                     pendingAfter,
                                     package);
}

/** Captures the member index resolved inside manager activation without performing a second lookup. */
__declspec(noinline) std::int32_t __fastcall member_record_index(std::byte** table,
                                                                  const void* record) noexcept {
    const MemberRecordIndex original =
        g_memberRecordIndexOriginal.load(std::memory_order_acquire);
    const std::int32_t result = original != nullptr ? original(table, record) : -1;
    if (!g_managerActivationTrace.armed) {
        return result;
    }

    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;
    if (callerRva == kManagerActivateMemberLookupReturnRva) {
        g_managerActivationTrace.entryRecordIndex = result;
    }
    else if (callerRva == kManagerActivateTailLookupReturnRva) {
        g_managerActivationTrace.tailRecordIndex = result;
    }
    return result;
}

/** Observes kind 19, the only native session handler that can write manager+0xE938. */
__declspec(noinline) bool __fastcall manager_host_handoff(std::byte* manager,
                                                           const std::byte* payload) noexcept {
    constexpr std::size_t kNetAddrBytes = 0x56U;
    const std::int32_t targetIndex =
        safe_read<std::int32_t>(payload != nullptr ? payload + 0x60 : nullptr, -1);
    const std::int32_t modeBefore =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AEF8 : nullptr, -1);
    const std::int32_t currentBefore =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x874 : nullptr, -1);
    const std::int32_t selectedBefore =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x87C : nullptr, -1);
    const std::int32_t pendingBefore =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0xE938 : nullptr, -1);
    const std::int32_t registeredBefore =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0xE93C : nullptr, -1);
    const std::uint8_t handoffReadyBefore =
        safe_read<std::uint8_t>(manager != nullptr ? manager + 0xE92D : nullptr, 0U);
    const std::uint32_t occupiedMask =
        safe_read<std::uint32_t>(manager != nullptr ? manager + 0x890 : nullptr, 0U);
    const bool targetInRange = targetIndex >= 0 && targetIndex < 32;
    const std::int32_t targetState = safe_read<std::int32_t>(
        manager != nullptr && targetInRange
            ? manager + 0x1FB8 + static_cast<std::ptrdiff_t>(targetIndex) * 0x120
            : nullptr,
        -1);
    const std::byte* const storedAddress =
        manager != nullptr && targetInRange
            ? manager + 0x8B0 + static_cast<std::ptrdiff_t>(targetIndex) * 0xB8
            : nullptr;
    bool addressMatch = payload != nullptr && storedAddress != nullptr;
    for (std::size_t index = 0; addressMatch && index < kNetAddrBytes; ++index) {
        addressMatch = safe_read<std::uint8_t>(payload + 8 + index, 0U)
                       == safe_read<std::uint8_t>(storedAddress + index, 0xFFU);
    }

    const HostHandoffApply original =
        g_hostHandoffApplyOriginal.load(std::memory_order_acquire);
    const bool result = original != nullptr && original(manager, payload);

    std::string_view package{};
    const bool opening = opening_is_forced(package);
    std::array<char, core::log::kLineCapacity> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_host_handoff manager=%p payload=%p session=0x%llX "
        "target=%d target_state=%d target_occupied=%u address_match=%u result=%u mode_before=%d "
        "mode_after=%d current_before=%d current_after=%d selected_before=%d selected_after=%d "
        "pending_before=%d pending_after=%d registered_before=%d registered_after=%d "
        "handoff_ready_before=%u handoff_ready_after=%u opening=%u package=%.*s",
        static_cast<void*>(manager),
        static_cast<const void*>(payload),
        static_cast<unsigned long long>(safe_read<std::uint64_t>(payload, 0U)),
        targetIndex,
        targetState,
        targetInRange && (occupiedMask & (std::uint32_t{1} << targetIndex)) != 0U ? 1U : 0U,
        addressMatch ? 1U : 0U,
        result ? 1U : 0U,
        modeBefore,
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AEF8 : nullptr, -1),
        currentBefore,
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x874 : nullptr, -1),
        selectedBefore,
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x87C : nullptr, -1),
        pendingBefore,
        safe_read<std::int32_t>(manager != nullptr ? manager + 0xE938 : nullptr, -1),
        registeredBefore,
        safe_read<std::int32_t>(manager != nullptr ? manager + 0xE93C : nullptr, -1),
        static_cast<unsigned int>(handoffReadyBefore),
        static_cast<unsigned int>(
            safe_read<std::uint8_t>(manager != nullptr ? manager + 0xE92D : nullptr, 0U)),
        opening ? 1U : 0U,
        static_cast<int>(package.size()),
        package.data());
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
    return result;
}

/** Observes the only native path that commits manager+0x1AF00 (active identity) to one. */
__declspec(noinline) bool __fastcall manager_activate(std::byte* manager,
                                                       const void* record,
                                                       const std::byte* payload) noexcept {
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0;
    const std::int32_t identityBefore = manager != nullptr
                                            ? safe_read<std::int32_t>(manager + 0x1C7C0, -1)
                                            : -1;
    const std::int32_t modeBefore = manager != nullptr
                                        ? safe_read<std::int32_t>(manager + 0x1AEF8, -1)
                                        : -1;
    const std::int32_t activeBefore = manager != nullptr
                                          ? safe_read<std::int32_t>(manager + 0x1AF00, -1)
                                          : -1;
    const std::int32_t currentIndexBefore = manager != nullptr
                                                ? safe_read<std::int32_t>(manager + 0x874, -1)
                                                : -1;
    const std::int32_t selectedBefore = manager != nullptr
                                            ? safe_read<std::int32_t>(manager + 0x87C, -1)
                                            : -1;
    const std::int32_t pendingIndexBefore = manager != nullptr
                                                ? safe_read<std::int32_t>(manager + 0xE938, -1)
                                                : -1;
    const std::int32_t registeredIndexBefore = manager != nullptr
                                                   ? safe_read<std::int32_t>(manager + 0xE93C, -1)
                                                   : -1;
    const auto* const recordBytes = static_cast<const std::byte*>(record);
    const std::uint64_t record0 = safe_read<std::uint64_t>(recordBytes, 0U);
    const std::uint64_t record8 = safe_read<std::uint64_t>(
        recordBytes != nullptr ? recordBytes + 8 : nullptr, 0U);
    const std::uint64_t record16 = safe_read<std::uint64_t>(
        recordBytes != nullptr ? recordBytes + 16 : nullptr, 0U);
    const std::uint64_t payload0 = safe_read<std::uint64_t>(payload, 0U);
    const std::uint64_t payload8 = payload != nullptr
                                       ? safe_read<std::uint64_t>(payload + 8, 0U)
                                       : 0U;
    const std::uint64_t payloadIdentity128A = safe_read<std::uint64_t>(
        payload != nullptr ? payload + 0x66U : nullptr, 0U);
    const std::uint64_t payloadIdentity128B = safe_read<std::uint64_t>(
        payload != nullptr ? payload + 0x6EU : nullptr, 0U);
    const std::uint64_t payloadOnlineA = safe_read<std::uint64_t>(
        payload != nullptr ? payload + kHostReestablishOnlineIdentityOffset : nullptr, 0U);
    const std::uint64_t payloadOnlineB = safe_read<std::uint64_t>(
        payload != nullptr ? payload + kHostReestablishOnlineIdentityOffset + 8U : nullptr, 0U);
    const std::uint16_t payloadOnlineTail = safe_read<std::uint16_t>(
        payload != nullptr ? payload + kHostReestablishOnlineIdentityOffset + 16U : nullptr, 0U);
    const std::uint64_t cachedOnlineBeforeA = safe_read<std::uint64_t>(
        manager != nullptr ? manager + kManagerOnlineIdentityOffset : nullptr, 0U);
    const std::uint64_t cachedOnlineBeforeB = safe_read<std::uint64_t>(
        manager != nullptr ? manager + kManagerOnlineIdentityOffset + 8U : nullptr, 0U);
    const std::uint16_t cachedOnlineBeforeTail = safe_read<std::uint16_t>(
        manager != nullptr ? manager + kManagerOnlineIdentityOffset + 16U : nullptr, 0U);

    std::uint32_t payloadNonzero = 0U;
    std::uint32_t payloadTailNonzero = 0U;
    if (payload != nullptr) {
        for (std::size_t index = 0; index < 136U; ++index) {
            const bool nonzero = safe_read<std::uint8_t>(payload + index, 0U) != 0U;
            payloadNonzero += nonzero ? 1U : 0U;
            payloadTailNonzero += nonzero && index >= 102U ? 1U : 0U;
        }
    }

    std::string_view package{};
    const bool opening = opening_is_forced(package);
    const bool onlineIdentityPresent = payloadOnlineA != 0U || payloadOnlineB != 0U
                                       || payloadOnlineTail != 0U;
    const bool onlineIdentityPreseedEligible =
        opening && package == "mission_towerfall" && callerRva == 0x16DF850U
        && manager != nullptr && payload != nullptr && identityBefore >= 0 && modeBefore == 9
        && activeBefore != 1 && currentIndexBefore >= 0 && pendingIndexBefore >= 0
        && currentIndexBefore != pendingIndexBefore && registeredIndexBefore >= 0
        && onlineIdentityPresent;
    bool onlineIdentityPreseeded = false;
    if (onlineIdentityPreseedEligible) {
        __try {
            // Native mgr_activate starts the mode-9 -> mode-4 migration before its final copy of
            // payload[8..135] into manager+0x1AF04. The managed-session request snapshots this
            // 18-byte tail during that helper and otherwise sees the previous all-zero cache.
            // Seed only the online identity: it lies beyond mode 9's 0x30-byte union payload, so
            // none of the mode-9 transition fields are modified. Native code overwrites the same
            // bytes from the same source after the helper returns.
            std::memcpy(manager + kManagerOnlineIdentityOffset,
                        payload + kHostReestablishOnlineIdentityOffset,
                        kOnlineIdentityBytes);
            onlineIdentityPreseeded = true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            onlineIdentityPreseeded = false;
        }
    }
    const std::uint64_t cachedOnlinePrecallA = safe_read<std::uint64_t>(
        manager != nullptr ? manager + kManagerOnlineIdentityOffset : nullptr, 0U);
    const std::uint64_t cachedOnlinePrecallB = safe_read<std::uint64_t>(
        manager != nullptr ? manager + kManagerOnlineIdentityOffset + 8U : nullptr, 0U);
    const std::uint16_t cachedOnlinePrecallTail = safe_read<std::uint16_t>(
        manager != nullptr ? manager + kManagerOnlineIdentityOffset + 16U : nullptr, 0U);

    g_managerActivationTrace = ManagerActivationTrace{true, -2, -2};
    bool result = false;
    const ManagerActivate original = g_managerActivateOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        result = original(manager, record, payload);
    }
    const ManagerActivationTrace trace = g_managerActivationTrace;
    g_managerActivationTrace = ManagerActivationTrace{};

    const std::int32_t identityAfter = manager != nullptr
                                           ? safe_read<std::int32_t>(manager + 0x1C7C0, -1)
                                           : -1;
    const std::int32_t modeAfter = manager != nullptr
                                       ? safe_read<std::int32_t>(manager + 0x1AEF8, -1)
                                       : -1;
    const std::int32_t activeAfter = manager != nullptr
                                         ? safe_read<std::int32_t>(manager + 0x1AF00, -1)
                                         : -1;
    const std::int32_t currentIndexAfter = manager != nullptr
                                               ? safe_read<std::int32_t>(manager + 0x874, -1)
                                               : -1;
    const std::int32_t selectedAfter = manager != nullptr
                                           ? safe_read<std::int32_t>(manager + 0x87C, -1)
                                           : -1;
    const std::int32_t pendingIndexAfter = manager != nullptr
                                               ? safe_read<std::int32_t>(manager + 0xE938, -1)
                                               : -1;
    const std::int32_t registeredIndexAfter = manager != nullptr
                                                  ? safe_read<std::int32_t>(manager + 0xE93C, -1)
                                                  : -1;
    std::byte* definition = nullptr;
    const auto identityDefinition = reinterpret_cast<IdentityDefinition>(
        validated_target(kIdentityDefinitionRva, kIdentityDefinitionPrefix));
    __try {
        definition = identityDefinition != nullptr && identityAfter >= 0
                         ? identityDefinition(identityAfter)
                         : nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        definition = nullptr;
    }
    const std::uint8_t definitionEnabledBefore =
        safe_read<std::uint8_t>(definition != nullptr ? definition + 0x94C : nullptr, 0U);
    const bool writerBranchTaken = result && trace.entryRecordIndex >= 0
                                   && trace.entryRecordIndex != registeredIndexBefore
                                   && modeBefore >= 4 && modeBefore <= 9
                                   && trace.entryRecordIndex != currentIndexBefore
                                   && trace.entryRecordIndex == pendingIndexBefore
                                   && activeAfter == 1;
    const bool identityEnableEligible = opening && package == "mission_towerfall"
                                        && writerBranchTaken && activeBefore != 1
                                        && identityAfter >= 0 && definition != nullptr;
    bool identityEnableReturned = false;
    if (identityEnableEligible) {
        __try {
            // This calls the real native function through its observer. It is deliberately gated
            // behind the real manager writer instead of writing definition+0x94C ourselves. The
            // migration-complete path may already have enabled the definition, so that byte is an
            // observed postcondition rather than an eligibility guard.
            identity_enable(identityAfter, false);
            identityEnableReturned = true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            identityEnableReturned = false;
        }
    }
    const std::uint8_t definitionEnabledAfter =
        safe_read<std::uint8_t>(definition != nullptr ? definition + 0x94C : nullptr, 0U);
    const std::uint8_t definitionPending =
        safe_read<std::uint8_t>(definition != nullptr ? definition + 0x94D : nullptr, 0U);
    const std::uint64_t cachedOnlineAfterA = safe_read<std::uint64_t>(
        manager != nullptr ? manager + kManagerOnlineIdentityOffset : nullptr, 0U);
    const std::uint64_t cachedOnlineAfterB = safe_read<std::uint64_t>(
        manager != nullptr ? manager + kManagerOnlineIdentityOffset + 8U : nullptr, 0U);
    const std::uint16_t cachedOnlineAfterTail = safe_read<std::uint16_t>(
        manager != nullptr ? manager + kManagerOnlineIdentityOffset + 16U : nullptr, 0U);
    const char* path = "writer_candidate";
    if (trace.entryRecordIndex == -2) {
        path = "lookup_not_observed";
    }
    else if (trace.entryRecordIndex == -1) {
        path = "record_unresolved";
    }
    else if (trace.entryRecordIndex == registeredIndexBefore) {
        path = "already_registered";
    }
    else if (modeBefore == 10) {
        path = "mode10_reconcile";
    }
    else if (modeBefore < 4 || modeBefore > 9) {
        path = "mode_outside_activation";
    }
    else if (trace.entryRecordIndex == currentIndexBefore) {
        path = "already_current";
    }
    else if (trace.entryRecordIndex != pendingIndexBefore) {
        path = "not_pending";
    }
    std::array<char, core::log::kLineCapacity> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_manager_activate caller_rva=0x%llX manager=%p record=%p record_words=0x%llX,0x%llX,0x%llX record_index=%d tail_index=%d path=%s payload=%p payload0=0x%llX payload8=0x%llX identity128=0x%llX,0x%llX online_payload=0x%llX,0x%llX,0x%04X online_cache_before=0x%llX,0x%llX,0x%04X online_cache_precall=0x%llX,0x%llX,0x%04X online_cache_after=0x%llX,0x%llX,0x%04X online_preseed=%s payload_nonzero=%u payload_tail_nonzero=%u result=%u identity_before=%d identity_after=%d mode_before=%d mode_after=%d current_before=%d current_after=%d selected_before=%d selected_after=%d pending_before=%d pending_after=%d registered_before=%d registered_after=%d active_before=%d active_after=%d definition=%p definition_enabled_before=%u definition_enabled_after=%u definition_pending=%u identity_enable_bridge=%s opening=%u package=%.*s",
        static_cast<unsigned long long>(callerRva),
        static_cast<void*>(manager),
        record,
        static_cast<unsigned long long>(record0),
        static_cast<unsigned long long>(record8),
        static_cast<unsigned long long>(record16),
        trace.entryRecordIndex,
        trace.tailRecordIndex,
        path,
        static_cast<const void*>(payload),
        static_cast<unsigned long long>(payload0),
        static_cast<unsigned long long>(payload8),
        static_cast<unsigned long long>(payloadIdentity128A),
        static_cast<unsigned long long>(payloadIdentity128B),
        static_cast<unsigned long long>(payloadOnlineA),
        static_cast<unsigned long long>(payloadOnlineB),
        static_cast<unsigned int>(payloadOnlineTail),
        static_cast<unsigned long long>(cachedOnlineBeforeA),
        static_cast<unsigned long long>(cachedOnlineBeforeB),
        static_cast<unsigned int>(cachedOnlineBeforeTail),
        static_cast<unsigned long long>(cachedOnlinePrecallA),
        static_cast<unsigned long long>(cachedOnlinePrecallB),
        static_cast<unsigned int>(cachedOnlinePrecallTail),
        static_cast<unsigned long long>(cachedOnlineAfterA),
        static_cast<unsigned long long>(cachedOnlineAfterB),
        static_cast<unsigned int>(cachedOnlineAfterTail),
        onlineIdentityPreseedEligible ? (onlineIdentityPreseeded ? "applied" : "exception")
                                      : "skip",
        payloadNonzero,
        payloadTailNonzero,
        result ? 1U : 0U,
        identityBefore,
        identityAfter,
        modeBefore,
        modeAfter,
        currentIndexBefore,
        currentIndexAfter,
        selectedBefore,
        selectedAfter,
        pendingIndexBefore,
        pendingIndexAfter,
        registeredIndexBefore,
        registeredIndexAfter,
        activeBefore,
        activeAfter,
        static_cast<void*>(definition),
        static_cast<unsigned int>(definitionEnabledBefore),
        static_cast<unsigned int>(definitionEnabledAfter),
        static_cast<unsigned int>(definitionPending),
        identityEnableEligible ? (identityEnableReturned ? "returned" : "exception") : "skip",
        opening ? 1U : 0U,
        static_cast<int>(package.size()),
        package.data());
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
    return result;
}

/**
 * Observes the authored-descriptor gate used by the receiver creator. Returning false here keeps
 * the manager alive but prevents the native fixed-slot allocator from ever being called.
 */
__declspec(noinline) bool __fastcall identity_descriptor_copy(std::int32_t identity,
                                                               bool alternate,
                                                               void* output) noexcept {
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;
    const IdentityDescriptorCopy original =
        g_identityDescriptorCopyOriginal.load(std::memory_order_acquire);
    const bool result = original != nullptr && original(identity, alternate, output);

    std::string_view package{};
    if (!opening_is_forced(package) || package != "mission_towerfall" || identity != 1) {
        return result;
    }
    const std::uint32_t observation =
        g_identityDescriptorCopyObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
    if (observation > 128U) {
        return result;
    }

    std::byte* definition = nullptr;
    const auto identityDefinition = reinterpret_cast<IdentityDefinition>(
        validated_target(kIdentityDefinitionRva, kIdentityDefinitionPrefix));
    __try {
        definition = identityDefinition != nullptr ? identityDefinition(identity) : nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        definition = nullptr;
    }
    const auto* const outputBytes = static_cast<const std::byte*>(output);
    std::array<char, core::log::kLineCapacity> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_identity_descriptor_copy n=%u caller_rva=0x%llX identity=%d alternate=%u result=%s output=%p output0=0x%llX output8=0x%llX definition=%p flags=0x%08X state=%u enabled=%u pending=%u forced=%.*s",
        observation,
        static_cast<unsigned long long>(callerRva),
        identity,
        alternate ? 1U : 0U,
        result ? "copied" : "rejected",
        output,
        static_cast<unsigned long long>(safe_read<std::uint64_t>(outputBytes, 0U)),
        static_cast<unsigned long long>(safe_read<std::uint64_t>(
            outputBytes != nullptr ? outputBytes + 8 : nullptr, 0U)),
        static_cast<void*>(definition),
        safe_read<std::uint32_t>(definition != nullptr ? definition + 0x4 : nullptr, 0U),
        static_cast<unsigned int>(safe_read<std::uint8_t>(
            definition != nullptr ? definition + 0x94C : nullptr, 0U)),
        static_cast<unsigned int>(safe_read<std::uint8_t>(
            definition != nullptr ? definition + 0x94D : nullptr, 0U) & 1U),
        static_cast<unsigned int>((safe_read<std::uint8_t>(
            definition != nullptr ? definition + 0x94D : nullptr, 0U) >> 1U) & 1U),
        static_cast<int>(package.size()),
        package.data());
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         result ? core::log::Level::info : core::log::Level::warn,
                         {line.data(), static_cast<std::size_t>(length)});
    }
    return result;
}

/**
 * Observes the manager gateway immediately above descriptor selection and receiver allocation.
 * Filtering on identity 1 avoids the continuously ticking identity-0 UI/script manager.
 */
__declspec(noinline) void __fastcall activity_receiver_creator(std::byte* manager) noexcept {
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;
    const std::int32_t identityBefore = safe_read<std::int32_t>(
        manager != nullptr ? manager + 0x1C7C0 : nullptr, -1);
    const std::int32_t modeBefore = safe_read<std::int32_t>(
        manager != nullptr ? manager + 0x1AEF8 : nullptr, -1);
    const std::int32_t componentBefore = safe_read<std::int32_t>(
        manager != nullptr ? manager + 0x1AF00 : nullptr, -1);

    const ActivityReceiverCreator original =
        g_activityReceiverCreatorOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(manager);
    }

    std::string_view package{};
    const bool opening = opening_is_forced(package) && package == "mission_towerfall";
    if (identityBefore != 1) {
        return;
    }
    const std::uint32_t observation =
        g_activityReceiverCreatorObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
    if (observation > 128U) {
        return;
    }

    std::byte* definition = nullptr;
    const auto identityDefinition = reinterpret_cast<IdentityDefinition>(
        validated_target(kIdentityDefinitionRva, kIdentityDefinitionPrefix));
    __try {
        definition = identityDefinition != nullptr ? identityDefinition(identityBefore) : nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        definition = nullptr;
    }
    std::array<char, core::log::kLineCapacity> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_receiver_creator n=%u caller_rva=0x%llX manager=%p identity_before=%d identity_after=%d activity=%d mode_before=%d mode_after=%d component_before=%d component_after=%d definition=%p flags=0x%08X state=%u enabled=%u pending=%u opening=%u package=%.*s",
        observation,
        static_cast<unsigned long long>(callerRva),
        static_cast<void*>(manager),
        identityBefore,
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1C7C0 : nullptr, -1),
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x850 : nullptr, -1),
        modeBefore,
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AEF8 : nullptr, -1),
        componentBefore,
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AF00 : nullptr, -1),
        static_cast<void*>(definition),
        safe_read<std::uint32_t>(definition != nullptr ? definition + 0x4 : nullptr, 0U),
        static_cast<unsigned int>(safe_read<std::uint8_t>(
            definition != nullptr ? definition + 0x94C : nullptr, 0U)),
        static_cast<unsigned int>(safe_read<std::uint8_t>(
            definition != nullptr ? definition + 0x94D : nullptr, 0U) & 1U),
        static_cast<unsigned int>((safe_read<std::uint8_t>(
            definition != nullptr ? definition + 0x94D : nullptr, 0U) >> 1U) & 1U),
        opening ? 1U : 0U,
        static_cast<int>(package.size()),
        package.data());
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
}

__declspec(noinline) std::byte* __fastcall manager_lookup(std::byte* context,
                                                           const void* identifier) noexcept {
    const ManagerLookup original = g_managerLookupOriginal.load(std::memory_order_acquire);
    std::byte* const manager = original != nullptr ? original(context, identifier) : nullptr;
    if (g_wireDispatchManagerTrace.armed) {
        g_wireDispatchManagerTrace.manager = manager;
    }
    std::string_view package{};
    if (!opening_is_forced(package)) {
        return manager;
    }
    const std::uint32_t observation =
        g_managerLookupObserved.fetch_add(1, std::memory_order_relaxed) + 1U;
    if (observation > 512U) {
        return manager;
    }
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0;
    const auto* const idBytes = static_cast<const std::byte*>(identifier);
    const std::uint64_t id0 = safe_read<std::uint64_t>(idBytes, 0);
    const std::uint64_t id8 = safe_read<std::uint64_t>(idBytes != nullptr ? idBytes + 8 : nullptr, 0);
    const std::int32_t activity =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x850 : nullptr, -1);
    const std::int32_t variant =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x854 : nullptr, -1);
    const std::int32_t registered =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0xE93C : nullptr, -1);
    const std::int32_t mode =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AEF8 : nullptr, -1);
    const std::int32_t component =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AF00 : nullptr, -1);
    const std::int32_t identity =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1C7C0 : nullptr, -1);
    std::array<char, core::log::kLineCapacity> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_manager_lookup n=%u caller_rva=0x%llX context=%p identifier=%p id0=0x%llX id8=0x%llX result=%p activity=%d variant=%d registered=%d mode=%d component=%d identity=%d forced=%.*s",
        observation,
        static_cast<unsigned long long>(callerRva),
        static_cast<void*>(context),
        identifier,
        static_cast<unsigned long long>(id0),
        static_cast<unsigned long long>(id8),
        static_cast<void*>(manager),
        activity,
        variant,
        registered,
        mode,
        component,
        identity,
        static_cast<int>(package.size()),
        package.data());
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
    return manager;
}

__declspec(noinline) void __fastcall transition_dispatch(std::byte* context,
                                                           const void* transition,
                                                           const void* update) noexcept {
    std::string_view package{};
    const bool opening = opening_is_forced(package);
    const std::uint32_t observation = opening
                                          ? g_transitionDispatchObserved.fetch_add(
                                                1, std::memory_order_relaxed)
                                                + 1U
                                          : 0U;
    const TransitionDispatch original =
        g_transitionDispatchOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(context, transition, update);
    }
    if (observation == 0U || observation > 256U) {
        return;
    }
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0;
    std::array<char, core::log::kLineCapacity> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_transition_dispatch n=%u caller_rva=0x%llX context=%p transition=%p update=%p forced=%.*s",
        observation,
        static_cast<unsigned long long>(callerRva),
        static_cast<void*>(context),
        transition,
        update,
        static_cast<int>(package.size()),
        package.data());
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
}

__declspec(noinline) void __fastcall wire_dispatch(std::byte* context,
                                                    std::byte* activityClient,
                                                    const void* eventData,
                                                    const void* identifier,
                                                    bool flag,
                                                    std::uint32_t eventCode,
                                                    std::uint32_t eventValue,
                                                    const void* payload) noexcept {
    std::string_view package{};
    const bool opening = opening_is_forced(package);
    const std::uint32_t observation = opening
                                          ? g_wireDispatchObserved.fetch_add(
                                                1, std::memory_order_relaxed)
                                                + 1U
                                          : 0U;
    const std::int32_t clientState = safe_read<std::int32_t>(
        activityClient != nullptr ? activityClient + 0x1D18 : nullptr, -1);
    if (observation != 0U && observation <= 512U) {
        const std::uint64_t id0 = safe_read<std::uint64_t>(identifier, 0);
        const auto* const idBytes = static_cast<const std::byte*>(identifier);
        const std::uint64_t id8 =
            safe_read<std::uint64_t>(idBytes != nullptr ? idBytes + 8 : nullptr, 0);
        std::array<char, core::log::kLineCapacity> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_script_wire_dispatch n=%u client_state=%d event=%u value=%u flag=%u context=%p activity_client=%p event_data=%p identifier=%p id0=0x%llX id8=0x%llX payload=%p forced=%.*s",
            observation,
            clientState,
            eventCode,
            eventValue,
            flag ? 1U : 0U,
            static_cast<void*>(context),
            static_cast<void*>(activityClient),
            eventData,
            identifier,
            static_cast<unsigned long long>(id0),
            static_cast<unsigned long long>(id8),
            payload,
            static_cast<int>(package.size()),
            package.data());
        if (length > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(length)});
        }
    }
    const WireDispatch original = g_wireDispatchOriginal.load(std::memory_order_acquire);
    const WireDispatchManagerTrace previousTrace = g_wireDispatchManagerTrace;
    g_wireDispatchManagerTrace = WireDispatchManagerTrace{true, eventCode, nullptr};
    if (original != nullptr) {
        original(context,
                 activityClient,
                 eventData,
                 identifier,
                 flag,
                 eventCode,
                 eventValue,
                 payload);
    }
    const WireDispatchManagerTrace dispatchTrace = g_wireDispatchManagerTrace;
    g_wireDispatchManagerTrace = previousTrace;

    const auto* const payloadBytes = static_cast<const std::byte*>(payload);
    const std::int32_t targetIndex = safe_read<std::int32_t>(
        payloadBytes != nullptr ? payloadBytes + 0x60 : nullptr, -1);
    std::byte* const manager = dispatchTrace.manager;
    const std::int32_t modeBefore = safe_read<std::int32_t>(
        manager != nullptr ? manager + 0x1AEF8 : nullptr, -1);
    const std::int32_t currentBefore = safe_read<std::int32_t>(
        manager != nullptr ? manager + 0x874 : nullptr, -1);
    const std::int32_t pendingBefore = safe_read<std::int32_t>(
        manager != nullptr ? manager + 0xE938 : nullptr, -1);
    const std::int32_t activeBefore = safe_read<std::int32_t>(
        manager != nullptr ? manager + 0x1AF00 : nullptr, -1);
    const bool bridgeEligible = opening && package == "mission_towerfall"
                                && dispatchTrace.eventCode == 19U
                                && eventValue == 100U && payloadBytes != nullptr
                                && targetIndex == 0 && manager != nullptr && modeBefore == 9
                                && currentBefore == 1 && pendingBefore == -1
                                && activeBefore == 2;
    bool bridgeResult = false;
    if (bridgeEligible) {
        __try {
            // Kind 21 makes this process the local host, so the session-protocol wrapper refuses
            // a subsequent wire handoff from the old host. Forward only that exact refused return
            // packet to the same validated native writer the wrapper normally calls.
            bridgeResult = manager_host_handoff(manager, payloadBytes);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            bridgeResult = false;
        }
    }
    if (opening && eventCode == 19U && targetIndex == 0) {
        std::array<char, core::log::kLineCapacity> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_script_host_handoff_bridge manager=%p target=%d mode_before=%d current_before=%d pending_before=%d pending_after=%d active_before=%d active_after=%d eligible=%u result=%u package=%.*s",
            static_cast<void*>(manager),
            targetIndex,
            modeBefore,
            currentBefore,
            pendingBefore,
            safe_read<std::int32_t>(manager != nullptr ? manager + 0xE938 : nullptr, -1),
            activeBefore,
            safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AF00 : nullptr, -1),
            bridgeEligible ? 1U : 0U,
            bridgeResult ? 1U : 0U,
            static_cast<int>(package.size()),
            package.data());
        if (length > 0) {
            core::log::write(core::log::Channel::client,
                             bridgeResult ? core::log::Level::info : core::log::Level::warn,
                             {line.data(), static_cast<std::size_t>(length)});
        }
    }
}

/** Retains the client object while its native event pump still owns a verified live reference. */
__declspec(noinline) bool __fastcall activity_client_update(std::byte* activityClient,
                                                             void* eventData,
                                                             std::uint32_t eventValue,
                                                             void* identifier,
                                                             void* payload,
                                                             bool flag6,
                                                             bool flag7) noexcept {
    const std::int32_t stateBefore = safe_read<std::int32_t>(
        activityClient != nullptr ? activityClient + 0x1D18 : nullptr, -1);
    const std::uint32_t observation =
        g_activityClientPumpObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
    const ActivityClientUpdate original =
        g_activityClientUpdateOriginal.load(std::memory_order_acquire);
    const bool result = original != nullptr
                        && original(activityClient,
                                    eventData,
                                    eventValue,
                                    identifier,
                                    payload,
                                    flag6,
                                    flag7);

    const std::int32_t stateAfter = safe_read<std::int32_t>(
        activityClient != nullptr ? activityClient + 0x1D18 : nullptr, -1);
    std::byte* const context = safe_read<std::byte*>(
        activityClient != nullptr ? activityClient + 0x18 : nullptr, nullptr);
    const std::uint32_t membershipMask = safe_read<std::uint32_t>(
        activityClient != nullptr ? activityClient + 0x20A8 : nullptr, 0U);
    if (activityClient != nullptr && stateAfter == 5 && context != nullptr) {
        g_activityClientCandidate.store(activityClient, std::memory_order_release);
    }

    std::string_view package{};
    const bool opening = opening_is_forced(package);
    if (observation <= 64U) {
        std::array<char, core::log::kLineCapacity> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_client_pump n=%u result=%u activity_client=%p state_before=%d state_after=%d context=%p membership_mask=0x%08X event_data=%p event_value=%u identifier=%p payload=%p flag6=%u flag7=%u opening=%u forced=%.*s",
            observation,
            result ? 1U : 0U,
            static_cast<void*>(activityClient),
            stateBefore,
            stateAfter,
            static_cast<void*>(context),
            membershipMask,
            eventData,
            eventValue,
            identifier,
            payload,
            flag6 ? 1U : 0U,
            flag7 ? 1U : 0U,
            opening ? 1U : 0U,
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

/**
 * Read-only qualification of the saved client at the still-live identity-1 manager boundary.
 * No event is dispatched until every pointer and native state agrees with the retail event-22 arm.
 */
void qualify_event22_candidate(std::string_view package) noexcept {
    if (package != "mission_towerfall"
        || !g_bootstrapRequestAttempted.load(std::memory_order_acquire)
        || !g_activitySetupComplete.load(std::memory_order_acquire)
        || !g_activityWorldStarted.load(std::memory_order_acquire)) {
        return;
    }
    const std::uint32_t observation =
        g_activityClientUpdateObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
    if (observation > 16U) {
        return;
    }

    std::byte* const activityClient =
        g_activityClientCandidate.load(std::memory_order_acquire);
    const std::int32_t clientState = safe_read<std::int32_t>(
        activityClient != nullptr ? activityClient + 0x1D18 : nullptr, -1);
    std::byte* const context = safe_read<std::byte*>(
        activityClient != nullptr ? activityClient + 0x18 : nullptr, nullptr);
    std::byte* const managerTable =
        safe_read<std::byte*>(context != nullptr ? context + 0x28 : nullptr, nullptr);
    const std::uint32_t membershipMask = safe_read<std::uint32_t>(
        activityClient != nullptr ? activityClient + 0x20A8 : nullptr, 0U);
    std::uint32_t memberIndex = 18U;
    for (std::uint32_t index = 0; index < 18U; ++index) {
        if ((membershipMask & (1U << index)) != 0U) {
            memberIndex = index;
            break;
        }
    }
    const std::byte* const member = memberIndex < 18U && activityClient != nullptr
                                        ? activityClient + 0x20B0 + memberIndex * 24U
                                        : nullptr;

    std::byte* definition = nullptr;
    std::byte* manager = nullptr;
    const auto identityDefinition = reinterpret_cast<IdentityDefinition>(
        validated_target(kIdentityDefinitionRva, kIdentityDefinitionPrefix));
    const ManagerLookup managerLookup = g_managerLookupOriginal.load(std::memory_order_acquire);
    __try {
        definition = identityDefinition != nullptr ? identityDefinition(1) : nullptr;
        const void* const managerIdentifier = definition != nullptr ? definition + 0x57C : nullptr;
        manager = managerLookup != nullptr && managerTable != nullptr
                      && managerIdentifier != nullptr
                      ? managerLookup(managerTable, managerIdentifier)
                      : nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        definition = nullptr;
        manager = nullptr;
    }

    const bool qualified = clientState == 5 && context != nullptr && managerTable != nullptr
                           && member != nullptr && definition != nullptr && manager != nullptr
                           && safe_read<std::int32_t>(manager + 0x1C7C0, -1) == 1
                           && safe_read<std::int32_t>(manager + 0x1AEF8, -1) == 6
                           && safe_read<std::int32_t>(manager + 0x1AF00, -1) == 0;
    if (observation == 1U) {
        // The previous successful public-player run reached in-world with no identity-1 manager,
        // whereas the immediately preceding run retained a local mode-6 manager. Snapshot the
        // native global slots and probe counters here so the next run identifies the exact missing
        // creation/update boundary without manufacturing a manager or changing route state.
        g_managerTableSnapshotObserved.store(0U, std::memory_order_release);
        snapshot_manager_table(package);
        const RouteModeSnapshot routeModes = query_route_modes();
        std::array<char, core::log::kLineCapacity> snapshotLine{};
        const int snapshotLength = std::snprintf(
            snapshotLine.data(),
            snapshotLine.size(),
            "ev=bootflow stage=activity_script_event22_manager_boundary_snapshot manager_update_calls=%u identity1_update_calls=%u identity2_update_calls=%u route_mode0_set_calls=%u route_mode1_set_calls=%u route_mode0=%u route_mode1=%u launch_window=%u launch_ticks=%u identity1_last_manager=%p identity1_last_mode=%d activity_client_pump_calls=%u qualified=%u forced=%.*s",
            g_managerUpdateLoopObserved.load(std::memory_order_acquire),
            g_managerUpdateLoopIdentityOneObserved.load(std::memory_order_acquire),
            g_managerUpdateLoopIdentityTwoObserved.load(std::memory_order_acquire),
            g_routeModeZeroSetObserved.load(std::memory_order_acquire),
            g_routeModeOneSetObserved.load(std::memory_order_acquire),
            routeModes.modeZero ? 1U : 0U,
            routeModes.modeOne ? 1U : 0U,
            g_launchProducerWindowState.load(std::memory_order_acquire),
            g_launchProducerWindowTicks.load(std::memory_order_acquire),
            static_cast<void*>(g_managerIdentityOneLastPointer.load(std::memory_order_acquire)),
            g_managerIdentityOneLastMode.load(std::memory_order_acquire),
            g_activityClientPumpObserved.load(std::memory_order_acquire),
            qualified ? 1U : 0U,
            static_cast<int>(package.size()),
            package.data());
        if (snapshotLength > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {snapshotLine.data(), static_cast<std::size_t>(snapshotLength)});
        }
    }
    std::array<char, core::log::kLineCapacity> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_event22_qualification n=%u result=%s activity_client=%p client_state=%d context=%p manager_table=%p membership_mask=0x%08X member_index=%u member=%p member0=0x%llX member8=0x%llX definition=%p identifier=%p manager=%p manager_mode=%d manager_component=%d manager_identity=%d forced=%.*s",
        observation,
        qualified ? "ready" : "wait",
        static_cast<void*>(activityClient),
        clientState,
        static_cast<void*>(context),
        static_cast<void*>(managerTable),
        membershipMask,
        memberIndex,
        static_cast<const void*>(member),
        static_cast<unsigned long long>(safe_read<std::uint64_t>(member, 0U)),
        static_cast<unsigned long long>(safe_read<std::uint64_t>(
            member != nullptr ? member + 8 : nullptr, 0U)),
        static_cast<void*>(definition),
        static_cast<void*>(definition != nullptr ? definition + 0x57C : nullptr),
        static_cast<void*>(manager),
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AEF8 : nullptr, -1),
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AF00 : nullptr, -1),
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1C7C0 : nullptr, -1),
        static_cast<int>(package.size()),
        package.data());
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         qualified ? core::log::Level::info : core::log::Level::warn,
                         {line.data(), static_cast<std::size_t>(length)});
    }
}

/**
 * Captures the actual script-event receiver selected by Destiny's native member lookup. The
 * apply-join callback exposes a different network activity client whose layout does not contain
 * the event-22 state at +0x1D18. This hook is observational and returns the native result
 * unchanged.
 */
__declspec(noinline) std::byte* __fastcall activity_receiver_lookup(const void* member) noexcept {
    const ActivityReceiverLookup original =
        g_activityReceiverLookupOriginal.load(std::memory_order_acquire);
    std::byte* const receiver = original != nullptr ? original(member) : nullptr;

    std::string_view package{};
    if (!opening_is_forced(package) || package != "mission_towerfall") {
        return receiver;
    }
    const std::uint32_t observation =
        g_activityReceiverLookupObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
    const std::int32_t state = safe_read<std::int32_t>(
        receiver != nullptr ? receiver + 0x1D18 : nullptr, -1);
    std::byte* const context = safe_read<std::byte*>(
        receiver != nullptr ? receiver + 0x18 : nullptr, nullptr);
    const std::uint32_t membershipMask = safe_read<std::uint32_t>(
        receiver != nullptr ? receiver + 0x20A8 : nullptr, 0U);
    if (receiver != nullptr) {
        g_activityClientCandidate.store(receiver, std::memory_order_release);
    }

    if (observation <= 64U) {
        std::array<char, core::log::kLineCapacity> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_script_receiver_lookup n=%u result=%s member=%p member0=0x%llX member8=0x%llX receiver=%p state=%d context=%p manager_table=%p membership_mask=0x%08X joined_client=%p forced=%.*s",
            observation,
            receiver != nullptr ? "found" : "missing",
            member,
            static_cast<unsigned long long>(safe_read<std::uint64_t>(member, 0U)),
            static_cast<unsigned long long>(safe_read<std::uint64_t>(
                member != nullptr ? static_cast<const std::byte*>(member) + 8 : nullptr, 0U)),
            static_cast<void*>(receiver),
            state,
            static_cast<void*>(context),
            safe_read<void*>(context != nullptr ? context + 0x28 : nullptr, nullptr),
            membershipMask,
            static_cast<void*>(g_joinedActivityClient.load(std::memory_order_acquire)),
            static_cast<int>(package.size()),
            package.data());
        if (length > 0) {
            core::log::write(core::log::Channel::client,
                             receiver != nullptr ? core::log::Level::info
                                                 : core::log::Level::warn,
                             {line.data(), static_cast<std::size_t>(length)});
        }
    }
    if (receiver != nullptr && state == 5 && context != nullptr
        && g_activityWorldStarted.load(std::memory_order_acquire)) {
        qualify_event22_candidate(package);
    }
    return receiver;
}

[[nodiscard]] std::byte* first_activity_receiver(ActivityReceiverIterator iterator) noexcept {
    if (iterator == nullptr) {
        return nullptr;
    }
    __try {
        return iterator(nullptr);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

/**
 * Observes Destiny's native type-5 activity-roster handler. This is the path that normally
 * resolves a roster message to one of the 62 fixed receiver slots and activates that slot.
 * The hook forwards every argument unchanged and only snapshots the handler's inputs and the
 * first active receiver after the native function returns.
 */
__declspec(noinline) void __fastcall activity_roster_apply(std::byte* context,
                                                            const std::byte* message,
                                                            const std::byte* payload) noexcept {
    const ActivityRosterApply original =
        g_activityRosterApplyOriginal.load(std::memory_order_acquire);
    std::string_view package{};
    const bool observe = opening_is_forced(package) && package == "mission_towerfall";
    const std::uint32_t observation = observe
                                          ? g_activityRosterApplyObserved.fetch_add(
                                                1U, std::memory_order_relaxed)
                                                + 1U
                                          : 0U;
    const std::uint16_t slotRaw = safe_read<std::uint16_t>(
        message != nullptr ? message + 0x12 : nullptr, 0xFFFFU);

    if (observe && observation <= 64U) {
        std::array<char, core::log::kLineCapacity> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_script_roster_apply phase=enter n=%u context=%p message=%p payload=%p slot_raw=%u message0=0x%llX message8=0x%llX message16=0x%llX message24=0x%llX payload0=0x%llX payload8=0x%llX payload16=0x%llX payload24=0x%llX forced=%.*s",
            observation,
            static_cast<void*>(context),
            static_cast<const void*>(message),
            static_cast<const void*>(payload),
            static_cast<unsigned int>(slotRaw),
            static_cast<unsigned long long>(safe_read<std::uint64_t>(message, 0U)),
            static_cast<unsigned long long>(safe_read<std::uint64_t>(
                message != nullptr ? message + 8 : nullptr, 0U)),
            static_cast<unsigned long long>(safe_read<std::uint64_t>(
                message != nullptr ? message + 16 : nullptr, 0U)),
            static_cast<unsigned long long>(safe_read<std::uint64_t>(
                message != nullptr ? message + 24 : nullptr, 0U)),
            static_cast<unsigned long long>(safe_read<std::uint64_t>(payload, 0U)),
            static_cast<unsigned long long>(safe_read<std::uint64_t>(
                payload != nullptr ? payload + 8 : nullptr, 0U)),
            static_cast<unsigned long long>(safe_read<std::uint64_t>(
                payload != nullptr ? payload + 16 : nullptr, 0U)),
            static_cast<unsigned long long>(safe_read<std::uint64_t>(
                payload != nullptr ? payload + 24 : nullptr, 0U)),
            static_cast<int>(package.size()),
            package.data());
        if (length > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(length)});
        }
    }

    if (original != nullptr) {
        original(context, message, payload);
    }

    if (!observe || observation > 64U) {
        return;
    }
    const auto iterator = reinterpret_cast<ActivityReceiverIterator>(
        validated_target(kActivityReceiverIteratorRva, kActivityReceiverIteratorPrefix));
    std::byte* const receiver = first_activity_receiver(iterator);
    if (receiver != nullptr) {
        g_activityClientCandidate.store(receiver, std::memory_order_release);
    }

    std::array<char, core::log::kLineCapacity> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_roster_apply phase=leave n=%u result=%s slot_raw=%u receiver=%p active=%u state=%d context=%p membership_mask=0x%08X forced=%.*s",
        observation,
        receiver != nullptr ? "active" : "empty",
        static_cast<unsigned int>(slotRaw),
        static_cast<void*>(receiver),
        safe_read<std::uint32_t>(receiver != nullptr ? receiver + 0x3040 : nullptr, 0U),
        safe_read<std::int32_t>(receiver != nullptr ? receiver + 0x1D18 : nullptr, -1),
        safe_read<void*>(receiver != nullptr ? receiver + 0x18 : nullptr, nullptr),
        safe_read<std::uint32_t>(receiver != nullptr ? receiver + 0x20A8 : nullptr, 0U),
        static_cast<int>(package.size()),
        package.data());
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         receiver != nullptr ? core::log::Level::info
                                             : core::log::Level::warn,
                         {line.data(), static_cast<std::size_t>(length)});
    }
}

/** Records the message types that actually reach Destiny's native peer-network dispatcher. */
__declspec(noinline) void __fastcall activity_peer_dispatch(std::byte* context,
                                                             const std::byte* message,
                                                             std::byte* source,
                                                             std::uint32_t messageType,
                                                             std::uint32_t payloadSize,
                                                             const std::byte* payload) noexcept {
    std::string_view package{};
    const bool observe = opening_is_forced(package) && package == "mission_towerfall";
    const std::uint32_t observation = observe
                                          ? g_activityPeerDispatchObserved.fetch_add(
                                                1U, std::memory_order_relaxed)
                                                + 1U
                                          : 0U;
    if (observe && observation <= 128U) {
        std::array<char, core::log::kLineCapacity> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_script_peer_dispatch n=%u type=%u size=%u context=%p message=%p source=%p payload=%p message0=0x%llX message8=0x%llX payload0=0x%llX payload8=0x%llX forced=%.*s",
            observation,
            messageType,
            payloadSize,
            static_cast<void*>(context),
            static_cast<const void*>(message),
            static_cast<void*>(source),
            static_cast<const void*>(payload),
            static_cast<unsigned long long>(safe_read<std::uint64_t>(message, 0U)),
            static_cast<unsigned long long>(safe_read<std::uint64_t>(
                message != nullptr ? message + 8 : nullptr, 0U)),
            static_cast<unsigned long long>(safe_read<std::uint64_t>(payload, 0U)),
            static_cast<unsigned long long>(safe_read<std::uint64_t>(
                payload != nullptr ? payload + 8 : nullptr, 0U)),
            static_cast<int>(package.size()),
            package.data());
        if (length > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(length)});
        }
    }

    const ActivityPeerDispatch original =
        g_activityPeerDispatchOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(context, message, source, messageType, payloadSize, payload);
    }
}

/** Observes the local routine that transitions one fixed receiver slot into an active state. */
__declspec(noinline) void __fastcall activity_receiver_activate(std::byte* root,
                                                                 std::int32_t slot,
                                                                 bool activate) noexcept {
    std::string_view package{};
    const bool observe = opening_is_forced(package) && package == "mission_towerfall";
    const std::uint32_t observation = observe
                                          ? g_activityReceiverActivateObserved.fetch_add(
                                                1U, std::memory_order_relaxed)
                                                + 1U
                                          : 0U;
    std::byte* const receiver = root != nullptr && slot >= 0 && slot < 62
                                    ? root + 0xA8 + static_cast<std::size_t>(slot) * 0x41F0U
                                    : nullptr;
    const std::uint32_t activeBefore = safe_read<std::uint32_t>(
        receiver != nullptr ? receiver + 0x3040 : nullptr, 0xFFFFFFFFU);
    const std::int32_t stateBefore = safe_read<std::int32_t>(
        receiver != nullptr ? receiver + 0x1D18 : nullptr, -1);

    const ActivityReceiverActivate original =
        g_activityReceiverActivateOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(root, slot, activate);
    }

    if (!observe || observation > 128U) {
        return;
    }
    const std::uint32_t activeAfter = safe_read<std::uint32_t>(
        receiver != nullptr ? receiver + 0x3040 : nullptr, 0xFFFFFFFFU);
    const std::int32_t stateAfter = safe_read<std::int32_t>(
        receiver != nullptr ? receiver + 0x1D18 : nullptr, -1);
    if (activeAfter != 0U && activeAfter != 0xFFFFFFFFU && receiver != nullptr) {
        g_activityClientCandidate.store(receiver, std::memory_order_release);
    }

    std::array<char, core::log::kLineCapacity> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_receiver_activate n=%u root=%p slot=%d requested=%u receiver=%p active_before=%u active_after=%u state_before=%d state_after=%d forced=%.*s",
        observation,
        static_cast<void*>(root),
        slot,
        activate ? 1U : 0U,
        static_cast<void*>(receiver),
        activeBefore,
        activeAfter,
        stateBefore,
        stateAfter,
        static_cast<int>(package.size()),
        package.data());
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         activeAfter != 0U && activeAfter != 0xFFFFFFFFU
                             ? core::log::Level::info
                             : core::log::Level::warn,
                         {line.data(), static_cast<std::size_t>(length)});
    }
}

/**
 * Observes the only validated wrapper that allocates and initializes one of the 62 native
 * script-event receiver slots. It forwards all seven arguments and returns the original slot.
 */
__declspec(noinline) std::int32_t __fastcall activity_receiver_bind(
    void* context,
    std::int32_t activity,
    const void* descriptorA,
    const void* descriptorB,
    bool flag,
    std::int32_t mode,
    const void* extra) noexcept {
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;
    const std::uint64_t descriptorA0 = safe_read<std::uint64_t>(descriptorA, 0U);
    const std::uint64_t descriptorA8 = safe_read<std::uint64_t>(
        descriptorA != nullptr ? static_cast<const std::byte*>(descriptorA) + 8 : nullptr, 0U);
    const std::uint64_t descriptorB0 = safe_read<std::uint64_t>(descriptorB, 0U);
    const std::uint64_t descriptorB8 = safe_read<std::uint64_t>(
        descriptorB != nullptr ? static_cast<const std::byte*>(descriptorB) + 8 : nullptr, 0U);

    const ActivityReceiverBind original =
        g_activityReceiverBindOriginal.load(std::memory_order_acquire);
    const std::int32_t slot = original != nullptr
                                  ? original(context,
                                             activity,
                                             descriptorA,
                                             descriptorB,
                                             flag,
                                             mode,
                                             extra)
                                  : -1;

    std::string_view package{};
    if (!opening_is_forced(package) || package != "mission_towerfall") {
        return slot;
    }
    const std::uint32_t observation =
        g_activityReceiverBindObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
    if (observation > 128U) {
        return slot;
    }

    std::byte* root = nullptr;
    const auto rootAccessor = reinterpret_cast<ActivityReceiverRoot>(
        validated_target(kActivityReceiverRootRva, kActivityReceiverRootPrefix));
    __try {
        root = rootAccessor != nullptr ? rootAccessor() : nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        root = nullptr;
    }
    std::byte* const receiver = root != nullptr && slot >= 0 && slot < 62
                                    ? root + 0xA8U
                                          + static_cast<std::size_t>(slot) * 0x41F0U
                                    : nullptr;
    if (receiver != nullptr
        && safe_read<std::uint32_t>(receiver + 0x3040, 0U) != 0U) {
        g_activityClientCandidate.store(receiver, std::memory_order_release);
    }

    std::array<char, core::log::kLineCapacity> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_receiver_bind n=%u caller_rva=0x%llX result=%s slot=%d context=%p activity=%d descriptor_a=%p descriptor_a0=0x%llX descriptor_a8=0x%llX descriptor_b=%p descriptor_b0=0x%llX descriptor_b8=0x%llX flag=%u mode=%d extra=%p root=%p receiver=%p active=%u state=%d interface=%p forced=%.*s",
        observation,
        static_cast<unsigned long long>(callerRva),
        slot >= 0 ? "allocated" : "rejected",
        slot,
        context,
        activity,
        descriptorA,
        static_cast<unsigned long long>(descriptorA0),
        static_cast<unsigned long long>(descriptorA8),
        descriptorB,
        static_cast<unsigned long long>(descriptorB0),
        static_cast<unsigned long long>(descriptorB8),
        flag ? 1U : 0U,
        mode,
        extra,
        static_cast<void*>(root),
        static_cast<void*>(receiver),
        safe_read<std::uint32_t>(receiver != nullptr ? receiver + 0x3040 : nullptr, 0U),
        safe_read<std::int32_t>(receiver != nullptr ? receiver + 0x1D18 : nullptr, -1),
        safe_read<void*>(receiver != nullptr ? receiver + 0x3150 : nullptr, nullptr),
        static_cast<int>(package.size()),
        package.data());
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         slot >= 0 ? core::log::Level::info : core::log::Level::warn,
                         {line.data(), static_cast<std::size_t>(length)});
    }
    return slot;
}

/**
 * Enumerates Destiny's native script-event receiver registry once after Homecoming enters the
 * world. This is deliberately observational: it never changes receiver memory, membership, or
 * dispatch state. The snapshot distinguishes a receiver that exists without queued fragments
 * from a receiver that was never constructed.
 */
void snapshot_activity_receiver_registry(std::string_view package) noexcept {
    if (package != "mission_towerfall"
        || g_activityReceiverSnapshotAttempted.exchange(true, std::memory_order_acq_rel)) {
        return;
    }

    const auto iterator = reinterpret_cast<ActivityReceiverIterator>(
        validated_target(kActivityReceiverIteratorRva, kActivityReceiverIteratorPrefix));
    if (iterator == nullptr) {
        core::log::write(
            core::log::Channel::client,
            core::log::Level::warn,
            "ev=bootflow stage=activity_script_receiver_registry result=fail reason=target");
        return;
    }

    constexpr std::uint32_t kMaximumReceivers = 64U;
    std::array<std::byte*, kMaximumReceivers> visited{};
    std::uint32_t count = 0U;
    const char* stopReason = "empty";

    __try {
        std::byte* receiver = iterator(nullptr);
        while (receiver != nullptr && count < kMaximumReceivers) {
            bool repeated = false;
            for (std::uint32_t index = 0U; index < count; ++index) {
                if (visited[index] == receiver) {
                    repeated = true;
                    break;
                }
            }
            if (repeated) {
                stopReason = "repeat";
                break;
            }
            visited[count] = receiver;

            const std::int32_t state = safe_read<std::int32_t>(receiver + 0x1D18, -1);
            std::byte* const context = safe_read<std::byte*>(receiver + 0x18, nullptr);
            std::byte* const managerTable =
                safe_read<std::byte*>(context != nullptr ? context + 0x28 : nullptr, nullptr);
            const std::uint32_t membershipMask =
                safe_read<std::uint32_t>(receiver + 0x20A8, 0U);
            std::uint32_t memberIndex = 18U;
            for (std::uint32_t index = 0U; index < 18U; ++index) {
                if ((membershipMask & (1U << index)) != 0U) {
                    memberIndex = index;
                    break;
                }
            }
            const std::byte* const member =
                memberIndex < 18U ? receiver + 0x20B0 + memberIndex * 24U : nullptr;
            if (state == 5 && context != nullptr) {
                g_activityClientCandidate.store(receiver, std::memory_order_release);
            }

            std::array<char, core::log::kLineCapacity> line{};
            const int length = std::snprintf(
                line.data(),
                line.size(),
                "ev=bootflow stage=activity_script_receiver_registry_entry n=%u receiver=%p vtable=%p state=%d context=%p manager_table=%p event_counter=%u membership_mask=0x%08X member_index=%u member=%p member0=0x%llX member8=0x%llX joined_client=%p forced=%.*s",
                count + 1U,
                static_cast<void*>(receiver),
                safe_read<void*>(receiver, nullptr),
                state,
                static_cast<void*>(context),
                static_cast<void*>(managerTable),
                safe_read<std::uint32_t>(receiver + 0x2024, 0U),
                membershipMask,
                memberIndex,
                static_cast<const void*>(member),
                static_cast<unsigned long long>(safe_read<std::uint64_t>(member, 0U)),
                static_cast<unsigned long long>(safe_read<std::uint64_t>(
                    member != nullptr ? member + 8 : nullptr, 0U)),
                static_cast<void*>(g_joinedActivityClient.load(std::memory_order_acquire)),
                static_cast<int>(package.size()),
                package.data());
            if (length > 0) {
                core::log::write(core::log::Channel::client,
                                 core::log::Level::info,
                                 {line.data(), static_cast<std::size_t>(length)});
            }

            ++count;
            std::byte* const next = iterator(receiver);
            if (next == receiver) {
                stopReason = "self";
                break;
            }
            receiver = next;
            stopReason = receiver == nullptr ? "end" : "limit";
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        stopReason = "exception";
    }

    std::array<char, 256> summary{};
    const int summaryLength = std::snprintf(
        summary.data(),
        summary.size(),
        "ev=bootflow stage=activity_script_receiver_registry result=%s count=%u stop=%s forced=%.*s",
        count > 0U ? "found" : "empty",
        count,
        stopReason,
        static_cast<int>(package.size()),
        package.data());
    if (summaryLength > 0) {
        core::log::write(core::log::Channel::client,
                         count > 0U ? core::log::Level::info : core::log::Level::warn,
                         {summary.data(), static_cast<std::size_t>(summaryLength)});
    }

    // The public iterator intentionally hides inactive entries. Dump the fixed backing slots as
    // well so a banner-only launch reveals which native prerequisite is keeping activation at
    // zero. This is observation only: no slot field is modified and the singleton getter is the
    // same native accessor used by Destiny's receiver code.
    const auto rootGetter = reinterpret_cast<ActivityReceiverRoot>(
        validated_target(kActivityReceiverRootRva, kActivityReceiverRootPrefix));
    if (rootGetter == nullptr) {
        core::log::write(
            core::log::Channel::client,
            core::log::Level::warn,
            "ev=bootflow stage=activity_script_receiver_raw result=fail reason=target");
        return;
    }

    std::byte* root = nullptr;
    __try {
        root = rootGetter();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        root = nullptr;
    }
    if (root == nullptr) {
        core::log::write(
            core::log::Channel::client,
            core::log::Level::warn,
            "ev=bootflow stage=activity_script_receiver_raw result=fail reason=root");
        return;
    }

    constexpr std::uint32_t kReceiverSlotCount = 62U;
    constexpr std::size_t kReceiverSlotBase = 0xA8U;
    constexpr std::size_t kReceiverSlotStride = 0x41F0U;
    std::uint32_t nonzeroCount = 0U;
    for (std::uint32_t slot = 0U; slot < kReceiverSlotCount; ++slot) {
        std::byte* const receiver =
            root + kReceiverSlotBase + static_cast<std::size_t>(slot) * kReceiverSlotStride;
        const std::uint32_t active = safe_read<std::uint32_t>(receiver + 0x3040, 0U);
        const std::int32_t state = safe_read<std::int32_t>(receiver + 0x1D18, -1);
        const std::uint32_t membershipMask = safe_read<std::uint32_t>(receiver + 0x20A8, 0U);
        const std::uint32_t flags3068 = safe_read<std::uint32_t>(receiver + 0x3068, 0U);
        const std::uint16_t flags306A = safe_read<std::uint16_t>(receiver + 0x306A, 0U);
        std::byte* const context = safe_read<std::byte*>(receiver + 0x18, nullptr);
        std::byte* const interfacePointer = safe_read<std::byte*>(receiver + 0x3150, nullptr);
        const bool nonzero = active != 0U || state != 0 || context != nullptr
                             || membershipMask != 0U || flags3068 != 0U
                             || flags306A != 0U || interfacePointer != nullptr
                             || safe_read<std::uint32_t>(receiver + 0x2024, 0U) != 0U
                             || safe_read<std::uint32_t>(receiver + 0x3080, 0U) != 0U
                             || safe_read<std::uint32_t>(receiver + 0x3084, 0U) != 0U
                             || safe_read<std::uint32_t>(receiver + 0x3088, 0U) != 0U
                             || safe_read<std::uint32_t>(receiver + 0x308C, 0U) != 0U
                             || safe_read<std::uint32_t>(receiver + 0x3090, 0U) != 0U
                             || safe_read<std::uint16_t>(receiver + 0x30F2, 0U) != 0U
                             || safe_read<std::uint8_t>(receiver + 0x3147, 0U) != 0U
                             || safe_read<std::uint8_t>(receiver + 0x3148, 0U) != 0U;
        if (!nonzero) {
            continue;
        }
        ++nonzeroCount;
        if (active != 0U && state == 5 && context != nullptr) {
            g_activityClientCandidate.store(receiver, std::memory_order_release);
        }

        std::array<char, core::log::kLineCapacity> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_script_receiver_raw_entry slot=%u receiver=%p vtable=%p active=%u state=%d state_aux=%d context=%p event_counter=%u membership_mask=0x%08X flags3068=0x%08X flags306A=0x%04X s3080=%u s3084=%u s3088=%u s308C=%u s3090=%u s30F2=%u s3147=%u s3148=%u interface=%p forced=%.*s",
            slot,
            static_cast<void*>(receiver),
            safe_read<void*>(receiver, nullptr),
            active,
            state,
            safe_read<std::int32_t>(receiver + 0x1D20, -1),
            static_cast<void*>(context),
            safe_read<std::uint32_t>(receiver + 0x2024, 0U),
            membershipMask,
            flags3068,
            static_cast<unsigned int>(flags306A),
            safe_read<std::uint32_t>(receiver + 0x3080, 0U),
            safe_read<std::uint32_t>(receiver + 0x3084, 0U),
            safe_read<std::uint32_t>(receiver + 0x3088, 0U),
            safe_read<std::uint32_t>(receiver + 0x308C, 0U),
            safe_read<std::uint32_t>(receiver + 0x3090, 0U),
            static_cast<unsigned int>(safe_read<std::uint16_t>(receiver + 0x30F2, 0U)),
            static_cast<unsigned int>(safe_read<std::uint8_t>(receiver + 0x3147, 0U)),
            static_cast<unsigned int>(safe_read<std::uint8_t>(receiver + 0x3148, 0U)),
            static_cast<void*>(interfacePointer),
            static_cast<int>(package.size()),
            package.data());
        if (length > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(length)});
        }
    }

    std::array<char, 256> rawSummary{};
    const int rawSummaryLength = std::snprintf(
        rawSummary.data(),
        rawSummary.size(),
        "ev=bootflow stage=activity_script_receiver_raw result=ok root=%p slots=%u nonzero=%u forced=%.*s",
        static_cast<void*>(root),
        kReceiverSlotCount,
        nonzeroCount,
        static_cast<int>(package.size()),
        package.data());
    if (rawSummaryLength > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {rawSummary.data(), static_cast<std::size_t>(rawSummaryLength)});
    }
}

[[nodiscard]] std::uintptr_t image_rva(const void* address) noexcept {
    const auto* const image = reinterpret_cast<const std::byte*>(GetModuleHandleW(nullptr));
    const auto* const pointer = static_cast<const std::byte*>(address);
    return image != nullptr && pointer >= image
               ? static_cast<std::uintptr_t>(pointer - image)
               : 0U;
}

[[nodiscard]] bool should_observe_manager_setup(std::byte* manager,
                                                std::uintptr_t callerRva,
                                                std::uintptr_t expectedCallerRva) noexcept {
    std::string_view package{};
    return callerRva == expectedCallerRva && opening_is_forced(package)
           && package == "mission_towerfall"
           && safe_read<std::int32_t>(manager != nullptr ? manager + 0x1C7C0 : nullptr, -1)
                  == 1;
}

void report_manager_setup_descriptor(const char* helper,
                                     const char* phase,
                                     std::byte* manager,
                                     std::uintptr_t callerRva) noexcept {
    const std::uint32_t observation =
        g_managerSetupStageObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
    if (observation > 32U) {
        return;
    }

    std::byte* definition = nullptr;
    const auto identityDefinition = reinterpret_cast<IdentityDefinition>(
        validated_target(kIdentityDefinitionRva, kIdentityDefinitionPrefix));
    __try {
        definition = identityDefinition != nullptr ? identityDefinition(1) : nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        definition = nullptr;
    }
    constexpr std::ptrdiff_t kAuthoredDescriptorOffset = 0x57C;
    std::byte* const descriptor =
        definition != nullptr ? definition + kAuthoredDescriptorOffset : nullptr;
    const std::uint64_t descriptor0 = safe_read<std::uint64_t>(descriptor, 0U);
    const std::uint64_t descriptor8 =
        safe_read<std::uint64_t>(descriptor != nullptr ? descriptor + 8 : nullptr, 0U);
    std::array<char, core::log::kLineCapacity> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_manager_setup n=%u helper=%s phase=%s caller_rva=0x%llX manager=%p definition=%p descriptor0=0x%llX descriptor8=0x%llX flags=0x%08X state=%u context=0x%llX activity=%d enabled=%u pending=%u",
        observation,
        helper,
        phase,
        static_cast<unsigned long long>(callerRva),
        static_cast<void*>(manager),
        static_cast<void*>(definition),
        static_cast<unsigned long long>(descriptor0),
        static_cast<unsigned long long>(descriptor8),
        safe_read<std::uint32_t>(definition != nullptr ? definition + 0x4 : nullptr, 0U),
        static_cast<unsigned int>(
            safe_read<std::uint16_t>(definition != nullptr ? definition + 0xA : nullptr, 0U)),
        static_cast<unsigned long long>(safe_read<std::uintptr_t>(
            definition != nullptr ? definition + 0x18 : nullptr, 0U)),
        safe_read<std::int32_t>(definition != nullptr ? definition + 0x24 : nullptr, -1),
        static_cast<unsigned int>(
            safe_read<std::uint8_t>(definition != nullptr ? definition + 0x94C : nullptr, 0U)),
        static_cast<unsigned int>(
            safe_read<std::uint8_t>(definition != nullptr ? definition + 0x94D : nullptr, 0U)));
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
}

__declspec(noinline) void __fastcall manager_setup_stage_a(std::byte* manager) noexcept {
    const std::uintptr_t callerRva = image_rva(_ReturnAddress());
    const bool focused =
        should_observe_manager_setup(manager, callerRva, kManagerSetupStageACallerRva);
    if (focused) {
        report_manager_setup_descriptor("0x1763FC0", "before", manager, callerRva);
    }
    const ManagerSetupStage original =
        g_managerSetupStageAOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(manager);
    }
    if (focused) {
        report_manager_setup_descriptor("0x1763FC0", "after", manager, callerRva);
    }
}

__declspec(noinline) void __fastcall manager_setup_stage_b(std::byte* manager) noexcept {
    const std::uintptr_t callerRva = image_rva(_ReturnAddress());
    const bool focused =
        should_observe_manager_setup(manager, callerRva, kManagerSetupStageBCallerRva);
    if (focused) {
        report_manager_setup_descriptor("0x1765490", "before", manager, callerRva);
    }
    const ManagerSetupStage original =
        g_managerSetupStageBOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(manager);
    }
    if (focused) {
        report_manager_setup_descriptor("0x1765490", "after", manager, callerRva);
    }
}

__declspec(noinline) void __fastcall manager_setup_stage_c(std::byte* manager) noexcept {
    const std::uintptr_t callerRva = image_rva(_ReturnAddress());
    const bool focused =
        should_observe_manager_setup(manager, callerRva, kManagerSetupStageCCallerRva);
    if (focused) {
        report_manager_setup_descriptor("0x17663E0", "before", manager, callerRva);
    }
    const ManagerSetupStage original =
        g_managerSetupStageCOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(manager);
    }
    if (focused) {
        report_manager_setup_descriptor("0x17663E0", "after", manager, callerRva);
    }
}

__declspec(noinline) void __fastcall manager_setup_stage_d(std::byte* manager) noexcept {
    const std::uintptr_t callerRva = image_rva(_ReturnAddress());
    const bool focused =
        should_observe_manager_setup(manager, callerRva, kManagerSetupStageDCallerRva);
    if (focused) {
        report_manager_setup_descriptor("0x1765EF0", "before", manager, callerRva);
    }
    const ManagerSetupStage original =
        g_managerSetupStageDOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(manager);
    }
    if (focused) {
        report_manager_setup_descriptor("0x1765EF0", "after", manager, callerRva);
    }
}

void attempt_homecoming_component_dispatch(std::byte* manager,
                                           std::int32_t identity,
                                           std::string_view package) noexcept {
    if (manager == nullptr || identity != 1 || package != "mission_towerfall"
         || !g_bootstrapRequestAttempted.load(std::memory_order_acquire)
         || !g_activitySetupComplete.load(std::memory_order_acquire)
         || g_componentDispatchAttempted.load(std::memory_order_acquire)) {
        return;
    }

    std::byte* definition = nullptr;
    const auto identityDefinition = reinterpret_cast<IdentityDefinition>(
        validated_target(kIdentityDefinitionRva, kIdentityDefinitionPrefix));
    __try {
        definition = identityDefinition != nullptr ? identityDefinition(identity) : nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        definition = nullptr;
    }
    const std::uint8_t enabled =
        safe_read<std::uint8_t>(definition != nullptr ? definition + 0x94C : nullptr, 0U);
    const std::uint8_t pending =
        safe_read<std::uint8_t>(definition != nullptr ? definition + 0x94D : nullptr, 0U);
    const std::int32_t mode = safe_read<std::int32_t>(manager + 0x1AEF8, -1);
    const std::int32_t registered = safe_read<std::int32_t>(manager + 0xE93C, -1);
    if (enabled == 0U || pending == 0U || mode < 4 || mode > 9 || registered < 0) {
        return;
    }

    const auto dispatch = reinterpret_cast<ComponentDispatch>(
        validated_target(kComponentDispatchRva, kComponentDispatchPrefix));
    if (dispatch == nullptr) {
        return;
    }
    bool expected = false;
    if (!g_componentDispatchAttempted.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel, std::memory_order_acquire)) {
        return;
    }

    std::array<char, core::log::kLineCapacity> line{};
    int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_component_dispatch result=attempt identity=1 component_index=0 manager=%p mode=%d registered=%d enabled=%u pending=%u forced=%.*s",
        static_cast<void*>(manager),
        mode,
        registered,
        static_cast<unsigned int>(enabled),
        static_cast<unsigned int>(pending),
        static_cast<int>(package.size()),
        package.data());
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }

    bool returned = true;
    __try {
        dispatch(manager, 0);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        returned = false;
    }
    length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_component_dispatch result=%s identity=1 component_index=0 manager=%p mode=%d selected=%d registered=%d active=%d forced=%.*s",
        returned ? "returned" : "exception",
        static_cast<void*>(manager),
        safe_read<std::int32_t>(manager + 0x1AEF8, -1),
        safe_read<std::int32_t>(manager + 0x87C, -1),
        safe_read<std::int32_t>(manager + 0xE93C, -1),
        safe_read<std::int32_t>(manager + 0x1AF00, -1),
        static_cast<int>(package.size()),
        package.data());
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         returned ? core::log::Level::info : core::log::Level::error,
                         {line.data(), static_cast<std::size_t>(length)});
    }
}

/** Observes either manager-owned optional value involved in selecting a launch route. */
void observe_current_selection_publish(
    std::byte* owner,
    const std::byte* descriptor,
    CurrentSelectionPublish original,
    const char* method) noexcept {
    std::byte* const manager =
        g_currentSelectionManager.load(std::memory_order_acquire);
    std::byte* const downstreamOwner =
        g_currentSelectionOwner.load(std::memory_order_acquire);
    std::byte* const upstreamOwner =
        manager != nullptr ? manager + kManagerUpstreamSelectionOffset : nullptr;
    const bool upstream = owner != nullptr && owner == upstreamOwner;
    const bool downstream = owner != nullptr && owner == downstreamOwner;
    const bool relevant = upstream || downstream;
    if (!relevant) {
        if (original != nullptr) {
            original(owner, descriptor);
        }
        return;
    }

    std::array<void*, 16> callstack{};
    const USHORT callstackCount = RtlCaptureStackBackTrace(
        1,
        static_cast<ULONG>(callstack.size()),
        callstack.data(),
        nullptr);
    const std::uint32_t observation =
        g_currentSelectionPublishObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
    const std::uint32_t descriptor0 = safe_read<std::uint32_t>(descriptor, 0U);
    const std::uint32_t descriptor4 =
        safe_read<std::uint32_t>(descriptor != nullptr ? descriptor + 0x04 : nullptr, 0U);
    const std::uint32_t descriptor8 =
        safe_read<std::uint32_t>(descriptor != nullptr ? descriptor + 0x08 : nullptr, 0U);
    const std::uint32_t descriptorC =
        safe_read<std::uint32_t>(descriptor != nullptr ? descriptor + 0x0C : nullptr, 0U);
    const std::uint32_t descriptor10 =
        safe_read<std::uint32_t>(descriptor != nullptr ? descriptor + 0x10 : nullptr, 0U);
    const std::uint64_t descriptor18 =
        safe_read<std::uint64_t>(descriptor != nullptr ? descriptor + 0x18 : nullptr, 0U);
    const std::int16_t selectionSource = safe_read<std::int16_t>(
        descriptor != nullptr ? descriptor + 0x22 : nullptr,
        static_cast<std::int16_t>(-1));
    const std::int16_t selectionDestination = safe_read<std::int16_t>(
        descriptor != nullptr ? descriptor + 0x24 : nullptr,
        static_cast<std::int16_t>(-1));
    const std::uint8_t selector =
        safe_read<std::uint8_t>(descriptor != nullptr ? descriptor + 0xA0 : nullptr, 0U);
    std::array<char, 41> selectionPackage{};
    std::size_t selectionPackageLength = 0;
    for (; descriptor != nullptr && selectionPackageLength + 1U < selectionPackage.size();
         ++selectionPackageLength) {
        const char value = safe_read<char>(descriptor + 0x70 + selectionPackageLength, '\0');
        selectionPackage[selectionPackageLength] = value;
        if (value == '\0') {
            break;
        }
    }
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    std::size_t imageSize = 0;
    if (image != nullptr) {
        const auto* const dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
        if (dos->e_magic == IMAGE_DOS_SIGNATURE && dos->e_lfanew > 0) {
            const auto* const nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(
                image + dos->e_lfanew);
            if (nt->Signature == IMAGE_NT_SIGNATURE) {
                imageSize = nt->OptionalHeader.SizeOfImage;
            }
        }
    }
    const auto* const caller = static_cast<const std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && caller >= image
                                             && static_cast<std::size_t>(caller - image) < imageSize
                                         ? static_cast<std::uintptr_t>(caller - image)
                                         : 0U;

    if (observation <= 64U) {
        std::array<char, core::log::kLineCapacity> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_script_current_selection_publish n=%u path=%s method=%s caller_rva=0x%llX manager=%p owner=%p descriptor=%p d0=0x%08X d4=0x%08X d8=0x%08X dc=0x%08X bytes10=0x%08X route=%u pointer18=0x%llX selection_source=%d selection_destination=%d selection_package=%.*s selector=%u",
            observation,
            upstream ? "upstream" : "downstream",
            method,
            static_cast<unsigned long long>(callerRva),
            static_cast<void*>(manager),
            static_cast<void*>(owner),
            static_cast<const void*>(descriptor),
            descriptor0,
            descriptor4,
            descriptor8,
            descriptorC,
            descriptor10,
            static_cast<unsigned int>((descriptor10 >> 16U) & 0xFFU),
            static_cast<unsigned long long>(descriptor18),
            static_cast<int>(selectionSource),
            static_cast<int>(selectionDestination),
            static_cast<int>(selectionPackageLength),
            selectionPackage.data(),
            static_cast<unsigned int>(selector));
        if (length > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(length)});
        }

        std::array<char, core::log::kLineCapacity> stackLine{};
        int stackLength = std::snprintf(
            stackLine.data(),
            stackLine.size(),
            "ev=bootflow stage=activity_script_current_selection_publish_stack n=%u path=%s method=%s count=%u rvas=",
            observation,
            upstream ? "upstream" : "downstream",
            method,
            static_cast<unsigned int>(callstackCount));
        for (USHORT index = 0;
             index < callstackCount && stackLength > 0
             && static_cast<std::size_t>(stackLength) < stackLine.size();
             ++index) {
            const auto address = reinterpret_cast<std::uintptr_t>(callstack[index]);
            const auto imageAddress = reinterpret_cast<std::uintptr_t>(image);
            const std::uintptr_t rva = image != nullptr && address >= imageAddress
                                               && address - imageAddress < imageSize
                                           ? address - imageAddress
                                           : 0U;
            const int appended = std::snprintf(
                stackLine.data() + stackLength,
                stackLine.size() - static_cast<std::size_t>(stackLength),
                "%s0x%llX",
                index == 0 ? "" : ",",
                static_cast<unsigned long long>(rva));
            if (appended <= 0) {
                break;
            }
            stackLength += appended;
        }
        if (stackLength > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {stackLine.data(),
                              static_cast<std::size_t>(stackLength) < stackLine.size()
                                  ? static_cast<std::size_t>(stackLength)
                                  : stackLine.size() - 1U});
        }
    }

    if (original != nullptr) {
        original(owner, descriptor);
    }
}

__declspec(noinline) void __fastcall current_selection_publish(
    std::byte* owner,
    const std::byte* descriptor) noexcept {
    observe_current_selection_publish(
        owner,
        descriptor,
        g_currentSelectionPublishOriginal.load(std::memory_order_acquire),
        "publish_b0");
}

__declspec(noinline) void __fastcall upstream_selection_publish(
    std::byte* owner,
    const std::byte* descriptor) noexcept {
    observe_current_selection_publish(
        owner,
        descriptor,
        g_upstreamSelectionPublishOriginal.load(std::memory_order_acquire),
        "publish_b0");
}

__declspec(noinline) void __fastcall upstream_selection_update(
    std::byte* owner,
    const std::byte* descriptor) noexcept {
    observe_current_selection_publish(
        owner,
        descriptor,
        g_upstreamSelectionUpdateOriginal.load(std::memory_order_acquire),
        "update_b8");
}

/** Installs the publisher probe after lane zero exposes its concrete optional-value vtable. */
void current_selection_publish_probe_admitted(std::int32_t lane) noexcept {
    if (lane != 0 || g_currentSelectionPublishHandle.attached) {
        return;
    }
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    if (image == nullptr) {
        return;
    }
    const auto managerAccessor =
        reinterpret_cast<LaneManagerAccessor>(image + kLaneManagerAccessorRva);
    std::byte* const manager = managerAccessor(lane);
    std::byte* const owner =
        manager != nullptr ? manager + kManagerCurrentSelectionOffset : nullptr;
    std::byte* const upstreamOwner =
        manager != nullptr ? manager + kManagerUpstreamSelectionOffset : nullptr;
    std::byte* const vtable = safe_read<std::byte*>(owner, nullptr);
    std::byte* const target = safe_read<std::byte*>(
        vtable != nullptr ? vtable + kOptionalPublishVtableOffset : nullptr,
        nullptr);
    std::byte* const upstreamVtable = safe_read<std::byte*>(upstreamOwner, nullptr);
    std::byte* const upstreamTarget = safe_read<std::byte*>(
        upstreamVtable != nullptr ? upstreamVtable + kOptionalPublishVtableOffset : nullptr,
        nullptr);
    std::byte* const upstreamUpdateTarget = safe_read<std::byte*>(
        upstreamVtable != nullptr ? upstreamVtable + kOptionalUpdateVtableOffset : nullptr,
        nullptr);
    MEMORY_BASIC_INFORMATION memory{};
    const DWORD executableMask = PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE
                                 | PAGE_EXECUTE_WRITECOPY;
    if (target == nullptr || upstreamTarget == nullptr || upstreamUpdateTarget == nullptr
        || VirtualQuery(target, &memory, sizeof memory) != sizeof memory
        || memory.State != MEM_COMMIT || (memory.Protect & executableMask) == 0
        || (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
        return;
    }

    bool expected = false;
    if (!g_currentSelectionPublishInstallAttempted.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel, std::memory_order_acquire)) {
        return;
    }
    g_currentSelectionManager.store(manager, std::memory_order_release);
    g_currentSelectionOwner.store(owner, std::memory_order_release);
    if (!hooking::detour::install(
            {target, reinterpret_cast<void*>(&current_selection_publish)},
            g_currentSelectionPublishHandle)) {
        g_currentSelectionManager.store(nullptr, std::memory_order_release);
        g_currentSelectionOwner.store(nullptr, std::memory_order_release);
        g_currentSelectionPublishInstallAttempted.store(false, std::memory_order_release);
        core::log::write(
            core::log::Channel::client,
            core::log::Level::warn,
            "ev=bootflow stage=activity_script_current_selection_publish_probe result=fail reason=attach");
        return;
    }
    g_currentSelectionPublishOriginal.store(
        reinterpret_cast<CurrentSelectionPublish>(g_currentSelectionPublishHandle.original),
        std::memory_order_release);
    if (upstreamTarget != target) {
        MEMORY_BASIC_INFORMATION upstreamMemory{};
        const bool upstreamExecutable =
            VirtualQuery(upstreamTarget, &upstreamMemory, sizeof upstreamMemory)
                == sizeof upstreamMemory
            && upstreamMemory.State == MEM_COMMIT
            && (upstreamMemory.Protect & executableMask) != 0
            && (upstreamMemory.Protect & (PAGE_GUARD | PAGE_NOACCESS)) == 0;
        if (!upstreamExecutable
            || !hooking::detour::install(
                {upstreamTarget, reinterpret_cast<void*>(&upstream_selection_publish)},
                g_upstreamSelectionPublishHandle)) {
            core::log::write(
                core::log::Channel::client,
                core::log::Level::warn,
                "ev=bootflow stage=activity_script_upstream_selection_publish_probe result=fail reason=attach");
        } else {
            g_upstreamSelectionPublishOriginal.store(
                reinterpret_cast<CurrentSelectionPublish>(
                    g_upstreamSelectionPublishHandle.original),
                std::memory_order_release);
        }
    }
    if (upstreamUpdateTarget != target && upstreamUpdateTarget != upstreamTarget) {
        MEMORY_BASIC_INFORMATION updateMemory{};
        const bool updateExecutable =
            VirtualQuery(upstreamUpdateTarget, &updateMemory, sizeof updateMemory)
                == sizeof updateMemory
            && updateMemory.State == MEM_COMMIT
            && (updateMemory.Protect & executableMask) != 0
            && (updateMemory.Protect & (PAGE_GUARD | PAGE_NOACCESS)) == 0;
        if (!updateExecutable
            || !hooking::detour::install(
                {upstreamUpdateTarget, reinterpret_cast<void*>(&upstream_selection_update)},
                g_upstreamSelectionUpdateHandle)) {
            core::log::write(
                core::log::Channel::client,
                core::log::Level::warn,
                "ev=bootflow stage=activity_script_upstream_selection_update_probe result=fail reason=attach");
        } else {
            g_upstreamSelectionUpdateOriginal.store(
                reinterpret_cast<CurrentSelectionPublish>(
                    g_upstreamSelectionUpdateHandle.original),
                std::memory_order_release);
        }
    }
    // The launch publisher at 0x17ADA60 starts near the old dump's end. Keep its complete
    // 0x1B0-byte copy path and the following state helpers for offline reconstruction.
    dump_runtime_code(L"activity_script_current_selection_publish", target, 0x1200U);
    dump_runtime_code(L"activity_script_upstream_selection_update",
                      upstreamUpdateTarget,
                      0x400U);
    std::array<char, core::log::kLineCapacity> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_current_selection_publish_probe result=ok target_rva=0x%llX upstream_target_rva=0x%llX upstream_update_target_rva=0x%llX shared=%d update_shared=%d manager=%p upstream_owner=%p downstream_owner=%p manager_mode=%d manager_identity=%d",
        static_cast<unsigned long long>(target - image),
        static_cast<unsigned long long>(upstreamTarget - image),
        static_cast<unsigned long long>(upstreamUpdateTarget - image),
        upstreamTarget == target ? 1 : 0,
        upstreamUpdateTarget == target || upstreamUpdateTarget == upstreamTarget ? 1 : 0,
        static_cast<void*>(manager),
        static_cast<void*>(upstreamOwner),
        static_cast<void*>(owner),
        safe_read<std::int32_t>(manager + 0x1AEF8, -1),
        safe_read<std::int32_t>(manager + 0x1C7C0, -1));
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
}

void ensure_current_selection_publish_probe(std::int32_t lane) noexcept {
    (void)legacy_owner_quarantine::execute_admitted_dynamic_writer(
        LateInstallGuard{},
        [lane]() noexcept { current_selection_publish_probe_admitted(lane); });
}

/** Records the descriptor source whose +0x12 byte selects local or authored initialization. */
__declspec(noinline) std::byte* __fastcall route_descriptor_lookup(
    std::int32_t lane,
    std::byte** managerOut) noexcept {
    const RouteDescriptorLookup original =
        g_routeDescriptorLookupOriginal.load(std::memory_order_acquire);
    std::byte* const descriptor = original != nullptr ? original(lane, managerOut) : nullptr;
    ensure_current_selection_publish_probe(lane);
    std::byte* const sourceManager = safe_read<std::byte*>(managerOut, nullptr);
    const std::uint32_t descriptor0 = safe_read<std::uint32_t>(descriptor, 0U);
    const std::uint32_t descriptor4 =
        safe_read<std::uint32_t>(descriptor != nullptr ? descriptor + 0x04 : nullptr, 0U);
    const std::uint32_t descriptor8 =
        safe_read<std::uint32_t>(descriptor != nullptr ? descriptor + 0x08 : nullptr, 0U);
    const std::uint32_t descriptorC =
        safe_read<std::uint32_t>(descriptor != nullptr ? descriptor + 0x0C : nullptr, 0U);
    const std::uint32_t descriptor10 =
        safe_read<std::uint32_t>(descriptor != nullptr ? descriptor + 0x10 : nullptr, 0U);
    const std::uint64_t descriptor18 =
        safe_read<std::uint64_t>(descriptor != nullptr ? descriptor + 0x18 : nullptr, 0U);
    const std::uint8_t selector =
        safe_read<std::uint8_t>(descriptor != nullptr ? descriptor + 0xA0 : nullptr, 0U);
    const std::int16_t selectionSource = safe_read<std::int16_t>(
        descriptor != nullptr ? descriptor + 0x22 : nullptr,
        static_cast<std::int16_t>(-1));
    const std::int16_t selectionDestination = safe_read<std::int16_t>(
        descriptor != nullptr ? descriptor + 0x24 : nullptr,
        static_cast<std::int16_t>(-1));
    std::array<char, 41> selectionPackage{};
    std::size_t selectionPackageLength = 0;
    for (; descriptor != nullptr && selectionPackageLength + 1U < selectionPackage.size();
         ++selectionPackageLength) {
        const char value = safe_read<char>(descriptor + 0x70 + selectionPackageLength, '\0');
        selectionPackage[selectionPackageLength] = value;
        if (value == '\0') {
            break;
        }
    }
    std::string_view package{};
    const bool opening = opening_is_forced(package) && package == "mission_towerfall";
    std::uint64_t signature = reinterpret_cast<std::uintptr_t>(descriptor)
                              ^ (reinterpret_cast<std::uintptr_t>(sourceManager) << 1U)
                              ^ (static_cast<std::uint64_t>(descriptor10) << 17U)
                              ^ (static_cast<std::uint64_t>(descriptor8) << 33U)
                              ^ (static_cast<std::uint64_t>(selector) << 9U)
                              ^ (static_cast<std::uint64_t>(
                                     static_cast<std::uint16_t>(selectionSource))
                                 << 41U)
                              ^ (static_cast<std::uint64_t>(
                                     static_cast<std::uint16_t>(selectionDestination))
                                 << 49U)
                              ^ (opening ? 0x9E3779B97F4A7C15ULL : 0U);
    const bool validLane = lane >= 0 && lane < 3;
    if (validLane) {
        const std::uint64_t prior = g_routeDescriptorLastSignature[static_cast<std::size_t>(lane)]
                                        .exchange(signature, std::memory_order_acq_rel);
        if (prior == signature) {
            return descriptor;
        }
    }
    const std::uint32_t observation =
        g_routeDescriptorObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
    if (observation > 512U) {
        return descriptor;
    }
    std::array<char, core::log::kLineCapacity> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_route_descriptor n=%u lane=%d source=%s descriptor=%p d0=0x%08X d4=0x%08X d8=0x%08X dc=0x%08X bytes10=0x%08X route=%u pointer18=0x%llX selection_source=%d selection_destination=%d selection_package=%.*s selector=%u manager_out=%p manager_mode=%d manager_identity=%d opening=%u package=%.*s",
        observation,
        lane,
        sourceManager != nullptr ? "manager" : "lane",
        static_cast<void*>(descriptor),
        descriptor0,
        descriptor4,
        descriptor8,
        descriptorC,
        descriptor10,
        static_cast<unsigned int>((descriptor10 >> 16U) & 0xFFU),
        static_cast<unsigned long long>(descriptor18),
        static_cast<int>(selectionSource),
        static_cast<int>(selectionDestination),
        static_cast<int>(selectionPackageLength),
        selectionPackage.data(),
        static_cast<unsigned int>(selector),
        static_cast<void*>(sourceManager),
        safe_read<std::int32_t>(sourceManager != nullptr ? sourceManager + 0x1AEF8 : nullptr, -1),
        safe_read<std::int32_t>(sourceManager != nullptr ? sourceManager + 0x1C7C0 : nullptr, -1),
        opening ? 1U : 0U,
        static_cast<int>(package.size()),
        package.data());
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
    return descriptor;
}

void log_route_callstack(const char* stage,
                         std::uint32_t observation,
                         const std::array<void*, 16>& callstack,
                         USHORT callstackCount) noexcept {
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    std::array<char, core::log::kLineCapacity> line{};
    int length = std::snprintf(line.data(),
                               line.size(),
                               "ev=bootflow stage=%s_stack n=%u count=%u rvas=",
                               stage,
                               observation,
                               static_cast<unsigned int>(callstackCount));
    for (USHORT index = 0;
         index < callstackCount && length > 0
         && static_cast<std::size_t>(length) < line.size();
         ++index) {
        const auto address = reinterpret_cast<std::uintptr_t>(callstack[index]);
        const auto imageAddress = reinterpret_cast<std::uintptr_t>(image);
        const std::uintptr_t rva = image != nullptr && address >= imageAddress
                                       ? address - imageAddress
                                       : 0U;
        const int appended = std::snprintf(
            line.data() + length,
            line.size() - static_cast<std::size_t>(length),
            "%s0x%llX",
            index == 0 ? "" : ",",
            static_cast<unsigned long long>(rva));
        if (appended <= 0) {
            break;
        }
        length += appended;
    }
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(),
                          static_cast<std::size_t>(length) < line.size()
                              ? static_cast<std::size_t>(length)
                              : line.size() - 1U});
    }
}

/** Observes a complete route descriptor at the native commit boundary without rewriting it. */
__declspec(noinline) void __fastcall route_commit(const std::byte* descriptor) noexcept {
    std::string_view package{};
    const bool opening = opening_is_forced(package) && package == "mission_towerfall";
    const std::uint32_t observation =
        g_routeCommitObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
    const std::uint32_t openingObservation =
        opening ? g_routeCommitOpeningObserved.fetch_add(1U, std::memory_order_relaxed) + 1U
                : 0U;
    const bool report = observation <= 16U || (opening && openingObservation <= 256U);
    std::array<void*, 16> callstack{};
    USHORT callstackCount = 0;
    if (report) {
        callstackCount = RtlCaptureStackBackTrace(
            1,
            static_cast<ULONG>(callstack.size()),
            callstack.data(),
            nullptr);
        std::array<char, core::log::kLineCapacity> line{};
        const std::uint32_t descriptor10 = safe_read<std::uint32_t>(
            descriptor != nullptr ? descriptor + 0x10 : nullptr,
            0U);
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_script_route_commit n=%u opening_n=%u descriptor=%p d0=0x%08X d4=0x%08X d8=0x%08X dc=0x%08X bytes10=0x%08X route=%u pointer18=0x%llX selection_source=%d selection_destination=%d selector=%u opening=%u package=%.*s mutation=observe_only",
            observation,
            openingObservation,
            static_cast<const void*>(descriptor),
            safe_read<std::uint32_t>(descriptor, 0U),
            safe_read<std::uint32_t>(descriptor != nullptr ? descriptor + 0x04 : nullptr, 0U),
            safe_read<std::uint32_t>(descriptor != nullptr ? descriptor + 0x08 : nullptr, 0U),
            safe_read<std::uint32_t>(descriptor != nullptr ? descriptor + 0x0C : nullptr, 0U),
            descriptor10,
            static_cast<unsigned int>((descriptor10 >> 16U) & 0xFFU),
            static_cast<unsigned long long>(safe_read<std::uint64_t>(
                descriptor != nullptr ? descriptor + 0x18 : nullptr,
                0U)),
            static_cast<int>(safe_read<std::int16_t>(
                descriptor != nullptr ? descriptor + 0x22 : nullptr,
                static_cast<std::int16_t>(-1))),
            static_cast<int>(safe_read<std::int16_t>(
                descriptor != nullptr ? descriptor + 0x24 : nullptr,
                static_cast<std::int16_t>(-1))),
            static_cast<unsigned int>(safe_read<std::uint8_t>(
                descriptor != nullptr ? descriptor + 0xA0 : nullptr,
                0U)),
            opening ? 1U : 0U,
            static_cast<int>(package.size()),
            package.data());
        if (length > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(length)});
        }
        log_route_callstack(
            "activity_script_route_commit", observation, callstack, callstackCount);
    }

    const RouteCommit original = g_routeCommitOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(descriptor);
    }
}

/** Observes which manager receives each committed route nonce and generation. */
__declspec(noinline) void __fastcall route_update(std::byte* manager,
                                                   std::uint64_t sourceNonce,
                                                   std::int32_t generation) noexcept {
    std::string_view package{};
    const bool opening = opening_is_forced(package) && package == "mission_towerfall";
    const std::uint32_t observation =
        g_routeUpdateObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
    const std::uint32_t openingObservation =
        opening ? g_routeUpdateOpeningObserved.fetch_add(1U, std::memory_order_relaxed) + 1U
                : 0U;
    const bool report = observation <= 16U || (opening && openingObservation <= 512U);
    const std::int32_t modeBefore =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AEF8 : nullptr, -1);
    const std::int32_t identityBefore =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1C7C0 : nullptr, -1);
    std::array<void*, 16> callstack{};
    USHORT callstackCount = 0;
    if (report) {
        callstackCount = RtlCaptureStackBackTrace(
            1,
            static_cast<ULONG>(callstack.size()),
            callstack.data(),
            nullptr);
    }

    const RouteUpdate original = g_routeUpdateOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(manager, sourceNonce, generation);
    }

    if (report) {
        std::array<char, core::log::kLineCapacity> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_script_route_update n=%u opening_n=%u manager=%p identity_before=%d identity_after=%d mode_before=%d mode_after=%d source_nonce=0x%llX generation=%d opening=%u package=%.*s mutation=observe_only",
            observation,
            openingObservation,
            static_cast<void*>(manager),
            identityBefore,
            safe_read<std::int32_t>(manager != nullptr ? manager + 0x1C7C0 : nullptr, -1),
            modeBefore,
            safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AEF8 : nullptr, -1),
            static_cast<unsigned long long>(sourceNonce),
            generation,
            opening ? 1U : 0U,
            static_cast<int>(package.size()),
            package.data());
        if (length > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(length)});
        }
        log_route_callstack(
            "activity_script_route_update", observation, callstack, callstackCount);
    }
}

/** Observes the first service-backed gate that can divert 0xEE8390 to its alternate path. */
__declspec(noinline) bool __fastcall default_route_reference_predicate(
    std::byte* service) noexcept {
    const DefaultRouteReferencePredicate original =
        g_defaultRouteReferencePredicateOriginal.load(std::memory_order_acquire);
    const bool result = original != nullptr && original(service);
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;
    std::string_view package{};
    if (callerRva == kDefaultRouteReferencePredicateReturnRva
        && opening_is_forced(package) && package == "mission_towerfall") {
        const std::uint32_t observation =
            g_defaultRouteReferencePredicateObserved.fetch_add(
                1U,
                std::memory_order_relaxed)
            + 1U;
        std::array<std::uint64_t, 8> words{};
        for (std::size_t index = 0; index < words.size(); ++index) {
            words[index] = safe_read<std::uint64_t>(
                service != nullptr ? service + index * sizeof(std::uint64_t) : nullptr,
                0U);
        }
        std::array<char, core::log::kLineCapacity> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_script_default_route_parent_gate gate=reference n=%u caller_rva=0x%llX service=%p vtable=%p words=%016llX,%016llX,%016llX,%016llX,%016llX,%016llX,%016llX,%016llX result=%u alternate_when_true=1 package=%.*s mutation=observe_only",
            observation,
            static_cast<unsigned long long>(callerRva),
            static_cast<void*>(service),
            static_cast<void*>(safe_read<std::byte*>(service, nullptr)),
            static_cast<unsigned long long>(words[0]),
            static_cast<unsigned long long>(words[1]),
            static_cast<unsigned long long>(words[2]),
            static_cast<unsigned long long>(words[3]),
            static_cast<unsigned long long>(words[4]),
            static_cast<unsigned long long>(words[5]),
            static_cast<unsigned long long>(words[6]),
            static_cast<unsigned long long>(words[7]),
            result ? 1U : 0U,
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

/** Observes the lane-pair gate immediately before the route-zero requirement test. */
__declspec(noinline) bool __fastcall default_route_pair_predicate(
    std::int32_t left,
    std::int32_t right) noexcept {
    const DefaultRoutePairPredicate original =
        g_defaultRoutePairPredicateOriginal.load(std::memory_order_acquire);
    const bool result = original != nullptr && original(left, right);
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;
    std::string_view package{};
    if (callerRva == kDefaultRoutePairPredicateReturnRva
        && opening_is_forced(package) && package == "mission_towerfall") {
        const std::uint32_t observation =
            g_defaultRoutePairPredicateObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
        std::array<char, core::log::kLineCapacity> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_script_default_route_parent_gate gate=pair n=%u caller_rva=0x%llX left=%d right=%d result=%u alternate_when_true=1 package=%.*s mutation=observe_only",
            observation,
            static_cast<unsigned long long>(callerRva),
            left,
            right,
            result ? 1U : 0U,
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

/** Observes the final predicate whose true result calls the route-zero constructor. */
__declspec(noinline) bool __fastcall default_route_required_predicate(
    std::int32_t lane) noexcept {
    const DefaultRouteRequiredPredicate original =
        g_defaultRouteRequiredPredicateOriginal.load(std::memory_order_acquire);
    const bool result = original != nullptr && original(lane);
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;
    std::string_view package{};
    if (callerRva == kDefaultRouteRequiredPredicateReturnRva
        && opening_is_forced(package) && package == "mission_towerfall") {
        const std::uint32_t observation =
            g_defaultRouteRequiredPredicateObserved.fetch_add(
                1U,
                std::memory_order_relaxed)
            + 1U;
        std::array<char, core::log::kLineCapacity> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_script_default_route_parent_gate gate=route_zero_required n=%u caller_rva=0x%llX lane=%d result=%u default_constructor_when_true=1 package=%.*s mutation=observe_only",
            observation,
            static_cast<unsigned long long>(callerRva),
            lane,
            result ? 1U : 0U,
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

/** Observes the cross-manager gate reached only after 0xEE8390 enters its alternate branch. */
__declspec(noinline) bool __fastcall default_route_cross_manager_predicate() noexcept {
    const DefaultRouteCrossManagerPredicate original =
        g_defaultRouteCrossManagerPredicateOriginal.load(std::memory_order_acquire);
    const bool result = original != nullptr && original();
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;
    std::string_view package{};
    if (callerRva == kDefaultRouteCrossManagerPredicateReturnRva
        && opening_is_forced(package) && package == "mission_towerfall") {
        const std::uint32_t observation =
            g_defaultRouteCrossManagerPredicateObserved.fetch_add(
                1U,
                std::memory_order_relaxed)
            + 1U;
        std::array<char, core::log::kLineCapacity> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_script_default_route_parent_gate gate=cross_manager n=%u caller_rva=0x%llX result=%u package=%.*s mutation=observe_only",
            observation,
            static_cast<unsigned long long>(callerRva),
            result ? 1U : 0U,
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

/** Observes the native default lane-zero initializer without changing its result or side effects. */
__declspec(noinline) std::uint64_t* __fastcall default_route_zero_initializer(
    std::uint64_t* result) noexcept {
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;
    std::string_view package{};
    const bool opening = opening_is_forced(package) && package == "mission_towerfall";
    const std::uint32_t observation =
        g_defaultRouteZeroInitializerObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;

    // The parent at 0xEE8390 calls this constructor only after its route-reference
    // predicate returned false. Snapshot both manager-owned optional selections at
    // that exact boundary; this is read-only and runs before the constructor publishes
    // the downstream/local descriptor.
    if (opening && callerRva == 0xC2806EU && image != nullptr) {
        const auto managerAccessor =
            reinterpret_cast<LaneManagerAccessor>(image + kLaneManagerAccessorRva);
        const auto optionalHasValue =
            reinterpret_cast<OptionalHasValue>(image + kOptionalHasValueRva);
        std::byte* const manager = managerAccessor(0);
        std::byte* const upstreamOwner =
            manager != nullptr ? manager + kManagerUpstreamSelectionOffset : nullptr;
        std::byte* const currentOwner =
            manager != nullptr ? manager + kManagerCurrentSelectionOffset : nullptr;
        const bool upstreamHas = upstreamOwner != nullptr && optionalHasValue(upstreamOwner);
        const bool currentHas = currentOwner != nullptr && optionalHasValue(currentOwner);
        const auto optionalValue = [](std::byte* owner) noexcept -> const std::byte* {
            std::byte* const vtable = safe_read<std::byte*>(owner, nullptr);
            const auto getter = reinterpret_cast<OptionalValue>(safe_read<std::byte*>(
                vtable != nullptr ? vtable + kOptionalValueVtableOffset : nullptr,
                nullptr));
            return getter != nullptr ? getter(owner) : nullptr;
        };
        const std::byte* const upstreamValue = upstreamHas ? optionalValue(upstreamOwner) : nullptr;
        const std::byte* const currentValue = currentHas ? optionalValue(currentOwner) : nullptr;
        std::array<char, core::log::kLineCapacity> decisionLine{};
        const int decisionLength = std::snprintf(
            decisionLine.data(),
            decisionLine.size(),
            "ev=bootflow stage=activity_script_default_route_parent_preconstructor n=%u parent_rva=0xEE8390 manager=%p identity=%d mode=%d upstream_owner=%p upstream_has=%u upstream_value=%p upstream_d0=0x%08X upstream_d4=0x%08X upstream_d8=0x%08X upstream_pointer18=0x%llX current_owner=%p current_has=%u current_value=%p current_d0=0x%08X current_d4=0x%08X current_d8=0x%08X current_pointer18=0x%llX mutation=observe_only",
            observation,
            static_cast<void*>(manager),
            safe_read<std::int32_t>(manager != nullptr ? manager + 0x1C7C0 : nullptr, -1),
            safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AEF8 : nullptr, -1),
            static_cast<void*>(upstreamOwner),
            upstreamHas ? 1U : 0U,
            static_cast<const void*>(upstreamValue),
            safe_read<std::uint32_t>(upstreamValue, 0U),
            safe_read<std::uint32_t>(
                upstreamValue != nullptr ? upstreamValue + 0x04 : nullptr,
                0U),
            safe_read<std::uint32_t>(
                upstreamValue != nullptr ? upstreamValue + 0x08 : nullptr,
                0U),
            static_cast<unsigned long long>(safe_read<std::uint64_t>(
                upstreamValue != nullptr ? upstreamValue + 0x18 : nullptr,
                0U)),
            static_cast<void*>(currentOwner),
            currentHas ? 1U : 0U,
            static_cast<const void*>(currentValue),
            safe_read<std::uint32_t>(currentValue, 0U),
            safe_read<std::uint32_t>(
                currentValue != nullptr ? currentValue + 0x04 : nullptr,
                0U),
            safe_read<std::uint32_t>(
                currentValue != nullptr ? currentValue + 0x08 : nullptr,
                0U),
            static_cast<unsigned long long>(safe_read<std::uint64_t>(
                currentValue != nullptr ? currentValue + 0x18 : nullptr,
                0U)));
        if (decisionLength > 0) {
            core::log::write(
                core::log::Channel::client,
                core::log::Level::info,
                {decisionLine.data(), static_cast<std::size_t>(decisionLength)});
        }
    }

    if (observation <= 64U) {
        std::array<void*, 16> callstack{};
        const USHORT callstackCount = RtlCaptureStackBackTrace(
            1,
            static_cast<ULONG>(callstack.size()),
            callstack.data(),
            nullptr);
        std::array<char, core::log::kLineCapacity> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_script_default_route_zero_initializer n=%u caller_rva=0x%llX result=%p opening=%u package=%.*s mutation=observe_only",
            observation,
            static_cast<unsigned long long>(callerRva),
            static_cast<void*>(result),
            opening ? 1U : 0U,
            static_cast<int>(package.size()),
            package.data());
        if (length > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(length)});
        }
        log_route_callstack(
            "activity_script_default_route_zero_initializer",
            observation,
            callstack,
            callstackCount);
    }

    const DefaultRouteZeroInitializer original =
        g_defaultRouteZeroInitializerOriginal.load(std::memory_order_acquire);
    return original != nullptr ? original(result) : result;
}

/** Observes the first lane-one authored-constructor gate. */
__declspec(noinline) bool __fastcall lane_one_ready_scan(std::byte* manager) noexcept {
    const LaneOneReadyScan original =
        g_laneOneReadyScanOriginal.load(std::memory_order_acquire);
    const bool result = original != nullptr && original(manager);
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;
    std::string_view package{};
    const bool opening = opening_is_forced(package) && package == "mission_towerfall";
    if (opening && callerRva == 0x17478F7U) {
        const std::uint32_t observation =
            g_laneOneReadyScanObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
        if (observation <= 64U) {
            std::array<char, core::log::kLineCapacity> line{};
            const int length = std::snprintf(
                line.data(),
                line.size(),
                "ev=bootflow stage=activity_script_lane1_gate gate=ready_scan n=%u caller_rva=0x%llX manager=%p identity=%d mode=%d result=%u package=%.*s mutation=observe_only",
                observation,
                static_cast<unsigned long long>(callerRva),
                static_cast<void*>(manager),
                safe_read<std::int32_t>(manager != nullptr ? manager + 0x1C7C0 : nullptr, -1),
                safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AEF8 : nullptr, -1),
                result ? 1U : 0U,
                static_cast<int>(package.size()),
                package.data());
            if (length > 0) {
                core::log::write(core::log::Channel::client,
                                 core::log::Level::info,
                                 {line.data(), static_cast<std::size_t>(length)});
            }
        }
    }
    return result;
}

/** Observes whether a pre-existing lane-one record prevents construction. */
__declspec(noinline) std::int32_t __fastcall lane_one_active_count() noexcept {
    const LaneOneActiveCount original =
        g_laneOneActiveCountOriginal.load(std::memory_order_acquire);
    const std::int32_t result = original != nullptr ? original() : -1;
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;
    std::string_view package{};
    const bool opening = opening_is_forced(package) && package == "mission_towerfall";
    if (opening && callerRva == 0x174795DU) {
        const std::uint32_t observation =
            g_laneOneActiveCountObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
        if (observation <= 64U) {
            std::array<char, core::log::kLineCapacity> line{};
            const int length = std::snprintf(
                line.data(),
                line.size(),
                "ev=bootflow stage=activity_script_lane1_gate gate=active_count n=%u caller_rva=0x%llX result=%d package=%.*s mutation=observe_only",
                observation,
                static_cast<unsigned long long>(callerRva),
                result,
                static_cast<int>(package.size()),
                package.data());
            if (length > 0) {
                core::log::write(core::log::Channel::client,
                                 core::log::Level::info,
                                 {line.data(), static_cast<std::size_t>(length)});
            }
        }
    }
    return result;
}

/** Observes the final external-selection readiness gate before lane-one construction. */
__declspec(noinline) bool __fastcall lane_one_selection_ready(
    std::byte* owner,
    const std::byte* selection) noexcept {
    const LaneOneSelectionReady original =
        g_laneOneSelectionReadyOriginal.load(std::memory_order_acquire);
    const bool result = original != nullptr && original(owner, selection);
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;
    std::string_view package{};
    const bool opening = opening_is_forced(package) && package == "mission_towerfall";
    if (opening && callerRva == 0x1747B0BU) {
        const std::uint32_t observation =
            g_laneOneSelectionReadyObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
        if (observation <= 64U) {
            std::array<char, core::log::kLineCapacity> line{};
            const int length = std::snprintf(
                line.data(),
                line.size(),
                "ev=bootflow stage=activity_script_lane1_gate gate=selection_ready n=%u caller_rva=0x%llX owner=%p owner_flags=0x%02X selection=%p selection0=0x%llX result=%u package=%.*s mutation=observe_only",
                observation,
                static_cast<unsigned long long>(callerRva),
                static_cast<void*>(owner),
                static_cast<unsigned int>(safe_read<std::uint8_t>(
                    owner != nullptr ? owner + 0x04 : nullptr,
                    0U)),
                static_cast<const void*>(selection),
                static_cast<unsigned long long>(safe_read<std::uint64_t>(selection, 0U)),
                result ? 1U : 0U,
                static_cast<int>(package.size()),
                package.data());
            if (length > 0) {
                core::log::write(core::log::Channel::client,
                                 core::log::Level::info,
                                 {line.data(), static_cast<std::size_t>(length)});
            }
        }
    }
    return result;
}

/** Observes the genuine native lane-one authored initializer. */
__declspec(noinline) void __fastcall lane_one_authored_initializer(
    std::int32_t selector,
    const std::byte* source,
    const std::byte* descriptor) noexcept {
    const std::uint32_t observation =
        g_laneOneAuthoredInitializerObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
    std::string_view package{};
    const bool opening = opening_is_forced(package) && package == "mission_towerfall";
    if (observation <= 64U || opening) {
        auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
        auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
        const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                             ? static_cast<std::uintptr_t>(returnAddress - image)
                                             : 0U;
        std::array<char, core::log::kLineCapacity> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_script_lane1_authored_initializer n=%u caller_rva=0x%llX selector=%d source=%p descriptor=%p d0=0x%08X d4=0x%08X d8=0x%08X dc=0x%08X bytes10=0x%08X route=%u package=%.*s mutation=observe_only",
            observation,
            static_cast<unsigned long long>(callerRva),
            selector,
            static_cast<const void*>(source),
            static_cast<const void*>(descriptor),
            safe_read<std::uint32_t>(descriptor, 0U),
            safe_read<std::uint32_t>(descriptor != nullptr ? descriptor + 0x04 : nullptr, 0U),
            safe_read<std::uint32_t>(descriptor != nullptr ? descriptor + 0x08 : nullptr, 0U),
            safe_read<std::uint32_t>(descriptor != nullptr ? descriptor + 0x0C : nullptr, 0U),
            safe_read<std::uint32_t>(descriptor != nullptr ? descriptor + 0x10 : nullptr, 0U),
            static_cast<unsigned int>(safe_read<std::uint8_t>(
                descriptor != nullptr ? descriptor + 0x12 : nullptr,
                0U)),
            static_cast<int>(package.size()),
            package.data());
        if (length > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(length)});
        }
    }
    const LaneOneAuthoredInitializer original =
        g_laneOneAuthoredInitializerOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(selector, source, descriptor);
    }
}

/** Observes a look-alike state-object initializer encountered near route setup. */
__declspec(noinline) void __fastcall route_state_arm(std::byte* state) noexcept {
    const std::uint8_t phaseBefore =
        safe_read<std::uint8_t>(state != nullptr ? state + 0x20 : nullptr, 0U);
    const std::uint8_t flagsBefore =
        safe_read<std::uint8_t>(state != nullptr ? state + 0x21 : nullptr, 0U);
    const std::uint8_t timestampPresentBefore =
        safe_read<std::uint8_t>(state != nullptr ? state + 0x22 : nullptr, 0U);
    const std::uint64_t timestampBefore =
        safe_read<std::uint64_t>(state != nullptr ? state + 0x28 : nullptr, 0U);
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;

    const RouteStateArm original = g_routeStateArmOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(state);
    }

    const std::uint8_t phaseAfter =
        safe_read<std::uint8_t>(state != nullptr ? state + 0x20 : nullptr, 0U);
    const std::uint8_t flagsAfter =
        safe_read<std::uint8_t>(state != nullptr ? state + 0x21 : nullptr, 0U);
    const std::uint8_t timestampPresentAfter =
        safe_read<std::uint8_t>(state != nullptr ? state + 0x22 : nullptr, 0U);
    const std::uint64_t timestampAfter =
        safe_read<std::uint64_t>(state != nullptr ? state + 0x28 : nullptr, 0U);
    std::string_view package{};
    const bool opening = opening_is_forced(package) && package == "mission_towerfall";
    const bool tracked = state != nullptr
                         && state == g_routeParentState.load(std::memory_order_acquire);
    if (opening || tracked) {
        const std::uint32_t observation =
            g_routeStateArmObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
        const bool changed = phaseBefore != phaseAfter || flagsBefore != flagsAfter
                             || timestampPresentBefore != timestampPresentAfter
                             || timestampBefore != timestampAfter;
        if (observation <= 64U || changed) {
            std::array<char, core::log::kLineCapacity> line{};
            const int length = std::snprintf(
                line.data(),
                line.size(),
                "ev=bootflow stage=activity_script_candidate_state_object_initializer n=%u caller_rva=0x%llX state=%p tracked=%u phase_before=0x%02X phase_after=0x%02X flags_before=0x%02X flags_after=0x%02X timestamp_present_before=%u timestamp_present_after=%u timestamp_before=0x%llX timestamp_after=0x%llX package=%.*s mutation=observe_only",
                observation,
                static_cast<unsigned long long>(callerRva),
                static_cast<void*>(state),
                tracked ? 1U : 0U,
                static_cast<unsigned int>(phaseBefore),
                static_cast<unsigned int>(phaseAfter),
                static_cast<unsigned int>(flagsBefore),
                static_cast<unsigned int>(flagsAfter),
                static_cast<unsigned int>(timestampPresentBefore),
                static_cast<unsigned int>(timestampPresentAfter),
                static_cast<unsigned long long>(timestampBefore),
                static_cast<unsigned long long>(timestampAfter),
                static_cast<int>(package.size()),
                package.data());
            if (length > 0) {
                core::log::write(core::log::Channel::client,
                                 core::log::Level::info,
                                 {line.data(), static_cast<std::size_t>(length)});
            }
        }
    }
}

[[nodiscard]] std::byte* embedded_route_state_for_lane(std::int32_t lane) noexcept {
    if (lane < 0 || lane > 2) {
        return nullptr;
    }
    std::byte* const laneZero =
        g_embeddedRouteLaneZeroState.load(std::memory_order_acquire);
    return laneZero != nullptr
               ? laneZero + static_cast<std::size_t>(lane) * kEmbeddedRouteStateStride
               : nullptr;
}

/** Observes the exact phase-bit setter for one of the three embedded route states. */
__declspec(noinline) void __fastcall embedded_route_state_phase_set(
    std::int32_t lane,
    bool enabled) noexcept {
    std::byte* const state = embedded_route_state_for_lane(lane);
    const std::uint8_t phaseBefore =
        safe_read<std::uint8_t>(state != nullptr ? state + 0x20 : nullptr, 0U);
    const std::uint8_t flagsBefore =
        safe_read<std::uint8_t>(state != nullptr ? state + 0x21 : nullptr, 0U);
    const std::uint64_t timestampBefore =
        safe_read<std::uint64_t>(state != nullptr ? state + 0x18 : nullptr, UINT64_MAX);
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;

    const EmbeddedRouteStatePhaseSet original =
        g_embeddedRouteStatePhaseSetOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(lane, enabled);
    }

    const std::uint8_t phaseAfter =
        safe_read<std::uint8_t>(state != nullptr ? state + 0x20 : nullptr, 0U);
    const std::uint8_t flagsAfter =
        safe_read<std::uint8_t>(state != nullptr ? state + 0x21 : nullptr, 0U);
    const std::uint64_t timestampAfter =
        safe_read<std::uint64_t>(state != nullptr ? state + 0x18 : nullptr, UINT64_MAX);
    const std::uint32_t observation =
        g_embeddedRouteStatePhaseSetObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
    std::string_view package{};
    const bool opening = embedded_route_trace_is_forced(package);
    if (observation <= 32U || opening || phaseBefore != phaseAfter || flagsBefore != flagsAfter) {
        std::array<char, core::log::kLineCapacity> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_script_embedded_route_phase_set n=%u caller_rva=0x%llX lane=%d enabled=%u state=%p phase_before=0x%02X phase_after=0x%02X flags_before=0x%02X flags_after=0x%02X timestamp_before=0x%llX timestamp_after=0x%llX package=%.*s mutation=observe_only",
            observation,
            static_cast<unsigned long long>(callerRva),
            lane,
            enabled ? 1U : 0U,
            static_cast<void*>(state),
            static_cast<unsigned int>(phaseBefore),
            static_cast<unsigned int>(phaseAfter),
            static_cast<unsigned int>(flagsBefore),
            static_cast<unsigned int>(flagsAfter),
            static_cast<unsigned long long>(timestampBefore),
            static_cast<unsigned long long>(timestampAfter),
            static_cast<int>(package.size()),
            package.data());
        if (length > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(length)});
        }
    }
}

/** Observes the exact native initializer for one of the three embedded route states. */
__declspec(noinline) void __fastcall embedded_route_state_initialize(
    std::int32_t lane,
    std::uint8_t flags,
    std::uint8_t phase) noexcept {
    std::byte* const state = embedded_route_state_for_lane(lane);
    const std::int32_t routeStateBefore =
        safe_read<std::int32_t>(state != nullptr ? state + 0x14 : nullptr, -1);
    const std::uint64_t timestampBefore =
        safe_read<std::uint64_t>(state != nullptr ? state + 0x18 : nullptr, UINT64_MAX);
    const std::uint8_t phaseBefore =
        safe_read<std::uint8_t>(state != nullptr ? state + 0x20 : nullptr, 0U);
    const std::uint8_t flagsBefore =
        safe_read<std::uint8_t>(state != nullptr ? state + 0x21 : nullptr, 0U);
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;

    const EmbeddedRouteStateInitialize original =
        g_embeddedRouteStateInitializeOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(lane, flags, phase);
    }

    const std::int32_t routeStateAfter =
        safe_read<std::int32_t>(state != nullptr ? state + 0x14 : nullptr, -1);
    const std::uint64_t timestampAfter =
        safe_read<std::uint64_t>(state != nullptr ? state + 0x18 : nullptr, UINT64_MAX);
    const std::uint8_t phaseAfter =
        safe_read<std::uint8_t>(state != nullptr ? state + 0x20 : nullptr, 0U);
    const std::uint8_t flagsAfter =
        safe_read<std::uint8_t>(state != nullptr ? state + 0x21 : nullptr, 0U);
    const std::uint32_t observation =
        g_embeddedRouteStateInitializeObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
    std::string_view package{};
    const bool opening = embedded_route_trace_is_forced(package);
    if (observation <= 32U || opening) {
        std::array<char, core::log::kLineCapacity> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_script_embedded_route_initialize n=%u caller_rva=0x%llX lane=%d requested_phase=0x%02X requested_flags=0x%02X state=%p route_state_before=%d route_state_after=%d phase_before=0x%02X phase_after=0x%02X flags_before=0x%02X flags_after=0x%02X timestamp_before=0x%llX timestamp_after=0x%llX package=%.*s mutation=observe_only",
            observation,
            static_cast<unsigned long long>(callerRva),
            lane,
            static_cast<unsigned int>(phase),
            static_cast<unsigned int>(flags),
            static_cast<void*>(state),
            routeStateBefore,
            routeStateAfter,
            static_cast<unsigned int>(phaseBefore),
            static_cast<unsigned int>(phaseAfter),
            static_cast<unsigned int>(flagsBefore),
            static_cast<unsigned int>(flagsAfter),
            static_cast<unsigned long long>(timestampBefore),
            static_cast<unsigned long long>(timestampAfter),
            static_cast<int>(package.size()),
            package.data());
        if (length > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(length)});
        }
    }
}

/** Observes resets that clear an embedded route state's phase and timestamp. */
__declspec(noinline) void __fastcall embedded_route_state_reset(std::byte* state) noexcept {
    const std::int32_t selectorBefore = safe_read<std::int32_t>(state, -1);
    const std::int32_t routeStateBefore =
        safe_read<std::int32_t>(state != nullptr ? state + 0x14 : nullptr, -1);
    const std::uint64_t timestampBefore =
        safe_read<std::uint64_t>(state != nullptr ? state + 0x18 : nullptr, UINT64_MAX);
    const std::uint8_t phaseBefore =
        safe_read<std::uint8_t>(state != nullptr ? state + 0x20 : nullptr, 0U);
    const std::uint8_t flagsBefore =
        safe_read<std::uint8_t>(state != nullptr ? state + 0x21 : nullptr, 0U);
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;

    const EmbeddedRouteStateReset original =
        g_embeddedRouteStateResetOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(state);
    }

    const std::int32_t selectorAfter = safe_read<std::int32_t>(state, -1);
    const std::int32_t routeStateAfter =
        safe_read<std::int32_t>(state != nullptr ? state + 0x14 : nullptr, -1);
    const std::uint64_t timestampAfter =
        safe_read<std::uint64_t>(state != nullptr ? state + 0x18 : nullptr, UINT64_MAX);
    const std::uint8_t phaseAfter =
        safe_read<std::uint8_t>(state != nullptr ? state + 0x20 : nullptr, 0U);
    const std::uint8_t flagsAfter =
        safe_read<std::uint8_t>(state != nullptr ? state + 0x21 : nullptr, 0U);
    if (state != nullptr && (selectorBefore == 0 || selectorAfter == 0)) {
        g_embeddedRouteLaneZeroState.store(state, std::memory_order_release);
    }
    const std::uint32_t observation =
        g_embeddedRouteStateResetObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
    std::string_view package{};
    const bool opening = embedded_route_trace_is_forced(package);
    if (observation <= 48U || opening) {
        std::array<char, core::log::kLineCapacity> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_script_embedded_route_reset n=%u caller_rva=0x%llX state=%p selector_before=%d selector_after=%d route_state_before=%d route_state_after=%d phase_before=0x%02X phase_after=0x%02X flags_before=0x%02X flags_after=0x%02X timestamp_before=0x%llX timestamp_after=0x%llX package=%.*s mutation=observe_only",
            observation,
            static_cast<unsigned long long>(callerRva),
            static_cast<void*>(state),
            selectorBefore,
            selectorAfter,
            routeStateBefore,
            routeStateAfter,
            static_cast<unsigned int>(phaseBefore),
            static_cast<unsigned int>(phaseAfter),
            static_cast<unsigned int>(flagsBefore),
            static_cast<unsigned int>(flagsAfter),
            static_cast<unsigned long long>(timestampBefore),
            static_cast<unsigned long long>(timestampAfter),
            static_cast<int>(package.size()),
            package.data());
        if (length > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(length)});
        }
    }
}

/** Captures the driver's own session-state query at its exact return boundary. */
__declspec(noinline) std::int32_t __fastcall embedded_route_session_activity_query(
    std::int32_t lane) noexcept {
    const LaunchProducerSessionActivity original =
        g_embeddedRouteSessionActivityOriginal.load(std::memory_order_acquire);
    const std::int32_t result = original != nullptr ? original(lane) : -1;
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;
    std::string_view package{};
    const bool opening = opening_is_forced(package) && package == "mission_towerfall";
    if (opening && callerRva == 0xEF1DCDU) {
        const std::uint32_t observation =
            g_embeddedRouteSessionActivityObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
        std::array<char, core::log::kLineCapacity> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_script_embedded_route_driver_session_query n=%u caller_rva=0x%llX lane=%d result=%d expected=31 package=%.*s mutation=observe_only",
            observation,
            static_cast<unsigned long long>(callerRva),
            lane,
            result,
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

/** Captures the driver's own lane-status query at its exact return boundary. */
__declspec(noinline) std::int32_t __fastcall embedded_route_state_status_query(
    std::int32_t lane) noexcept {
    const EmbeddedRouteStateStatus original =
        g_embeddedRouteStateStatusOriginal.load(std::memory_order_acquire);
    const std::int32_t result = original != nullptr ? original(lane) : -1;
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;
    std::string_view package{};
    const bool opening = embedded_route_trace_is_forced(package);
    if (opening && callerRva == 0xEF1DD7U) {
        const std::uint32_t observation =
            g_embeddedRouteStateStatusObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
        std::array<char, core::log::kLineCapacity> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_script_embedded_route_driver_status_query n=%u caller_rva=0x%llX lane=%d result=%d expected=0 package=%.*s mutation=observe_only",
            observation,
            static_cast<unsigned long long>(callerRva),
            lane,
            result,
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

/** Captures the driver's context-skip predicate at its exact return boundary. */
__declspec(noinline) bool __fastcall embedded_route_context_skip_predicate(
    std::byte* context) noexcept {
    const EmbeddedRouteContextSkipPredicate original =
        g_embeddedRouteContextSkipPredicateOriginal.load(std::memory_order_acquire);
    const bool result = original != nullptr && original(context);
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;
    std::string_view package{};
    const bool opening = embedded_route_trace_is_forced(package);
    if (opening && callerRva == 0xEF1E6CU) {
        const std::uint32_t observation =
            g_embeddedRouteContextSkipPredicateObserved.fetch_add(
                1U,
                std::memory_order_relaxed)
            + 1U;
        std::array<char, core::log::kLineCapacity> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_script_embedded_route_driver_context_skip n=%u caller_rva=0x%llX context=%p result=%u expected=0 flag4a=%u package=%.*s mutation=observe_only",
            observation,
            static_cast<unsigned long long>(callerRva),
            static_cast<void*>(context),
            result ? 1U : 0U,
            static_cast<unsigned int>(safe_read<std::uint8_t>(
                context != nullptr ? context + 0x4A : nullptr,
                0U)),
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

/** Captures the driver's lane-availability predicate at its exact return boundary. */
__declspec(noinline) bool __fastcall embedded_route_lane_available_predicate(
    std::int32_t lane) noexcept {
    const EmbeddedRouteLaneAvailablePredicate original =
        g_embeddedRouteLaneAvailablePredicateOriginal.load(std::memory_order_acquire);
    const bool result = original != nullptr && original(lane);
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;
    std::string_view package{};
    const bool opening = embedded_route_trace_is_forced(package);
    const bool driverCall = callerRva == 0xEF1E7EU;
    const bool defaultParentCall = callerRva == 0xEE83CDU;
    if (opening && (driverCall || defaultParentCall)) {
        const std::uint32_t observation =
            g_embeddedRouteLaneAvailablePredicateObserved.fetch_add(
                1U,
                std::memory_order_relaxed)
            + 1U;
        std::array<char, core::log::kLineCapacity> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=%s n=%u caller_rva=0x%llX lane=%d result=%u expected=1 package=%.*s mutation=observe_only",
            defaultParentCall
                ? "activity_script_default_route_parent_gate gate=lane0_available"
                : "activity_script_embedded_route_driver_lane_available",
            observation,
            static_cast<unsigned long long>(callerRva),
            lane,
            result ? 1U : 0U,
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

/** Captures authored-identity classification used directly and by the local predicate. */
__declspec(noinline) bool __fastcall embedded_route_authored_identity_predicate(
    std::uint16_t identifier) noexcept {
    const EmbeddedRouteIdentityPredicate original =
        g_embeddedRouteAuthoredIdentityPredicateOriginal.load(std::memory_order_acquire);
    const bool result = original != nullptr && original(identifier);
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;
    std::string_view package{};
    const bool opening = embedded_route_trace_is_forced(package);
    const bool driverCall = callerRva == 0xEF1E8FU;
    const bool localNestedCall = callerRva == 0xC25243U;
    if (opening && (driverCall || localNestedCall)) {
        const std::uint32_t observation =
            g_embeddedRouteAuthoredIdentityPredicateObserved.fetch_add(
                1U,
                std::memory_order_relaxed)
            + 1U;
        if (observation <= 64U || driverCall) {
            std::array<char, core::log::kLineCapacity> line{};
            const int length = std::snprintf(
                line.data(),
                line.size(),
                "ev=bootflow stage=activity_script_embedded_route_driver_authored_identity n=%u caller_rva=0x%llX site=%s identifier=%u result=%u expected=0 package=%.*s mutation=observe_only",
                observation,
                static_cast<unsigned long long>(callerRva),
                driverCall ? "driver" : "local_nested",
                static_cast<unsigned int>(identifier),
                result ? 1U : 0U,
                static_cast<int>(package.size()),
                package.data());
            if (length > 0) {
                core::log::write(core::log::Channel::client,
                                 core::log::Level::info,
                                 {line.data(), static_cast<std::size_t>(length)});
            }
        }
    }
    return result;
}

/** Captures the provider object returned by its virtual identifier lookup. */
__declspec(noinline) std::byte* __fastcall embedded_route_identity_provider_lookup(
    std::byte* service,
    std::uint16_t identifier) noexcept {
    const EmbeddedRouteIdentityProviderLookup original =
        g_embeddedRouteIdentityProviderLookupOriginal.load(std::memory_order_acquire);
    std::byte* const result = original != nullptr ? original(service, identifier) : nullptr;
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;
    std::string_view package{};
    if (callerRva == kEmbeddedRouteIdentityProviderLookupReturnRva
        && embedded_route_trace_is_forced(package)) {
        const std::uint32_t observation =
            g_embeddedRouteIdentityProviderLookupObserved.fetch_add(
                1U,
                std::memory_order_relaxed)
            + 1U;
        if (observation > 64U) {
            return result;
        }
        std::byte* const relativeBase =
            result != nullptr
                ? result + kEmbeddedRouteIdentityProviderRelativeRecordOffset
                : nullptr;
        const std::uintptr_t relative = safe_read<std::uintptr_t>(relativeBase, 0U);
        std::byte* resolvedRecord = nullptr;
        if (relativeBase != nullptr && relative != 0U) {
            const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(relativeBase);
            if (relative <= UINTPTR_MAX - base) {
                resolvedRecord = reinterpret_cast<std::byte*>(base + relative);
            }
        }
        const std::int32_t recordFirst = safe_read<std::int32_t>(resolvedRecord, INT32_MIN);
        std::array<char, core::log::kLineCapacity> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_script_embedded_route_identity_provider_virtual_lookup n=%u caller_rva=0x%llX identifier=%u service=%p object=%p relative=0x%llX record=%p record_first=%d package=%.*s mutation=observe_only",
            observation,
            static_cast<unsigned long long>(callerRva),
            static_cast<unsigned int>(identifier),
            static_cast<void*>(service),
            static_cast<void*>(result),
            static_cast<unsigned long long>(relative),
            static_cast<void*>(resolvedRecord),
            recordFirst,
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

/** Lazily resolves the provider's virtual identifier lookup once its service exists. */
void embedded_route_identity_provider_lookup_probe_admitted() noexcept {
    if (g_embeddedRouteIdentityProviderLookupHandle.attached) {
        return;
    }
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    std::byte* const service = safe_read<std::byte*>(
        image != nullptr ? image + kEmbeddedRouteIdentityProviderServicePointerRva : nullptr,
        nullptr);
    std::byte* const vtable = safe_read<std::byte*>(service, nullptr);
    std::byte* const target = safe_read<std::byte*>(
        vtable != nullptr ? vtable + kEmbeddedRouteIdentityProviderLookupVtableOffset : nullptr,
        nullptr);
    MEMORY_BASIC_INFORMATION memory{};
    const DWORD executableMask = PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE
                                 | PAGE_EXECUTE_WRITECOPY;
    if (target == nullptr || VirtualQuery(target, &memory, sizeof memory) != sizeof memory
        || memory.State != MEM_COMMIT || (memory.Protect & executableMask) == 0
        || (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
        return;
    }
    bool expected = false;
    if (!g_embeddedRouteIdentityProviderLookupInstallAttempted.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel, std::memory_order_acquire)) {
        return;
    }
    if (!g_embeddedRouteIdentityProviderLookupCodeDumped.exchange(
            true,
            std::memory_order_acq_rel)) {
        dump_runtime_code(L"activity_script_embedded_route_identity_provider_virtual_lookup",
                          target,
                          0x1000U);
    }
    if (!hooking::detour::install(
            {target, reinterpret_cast<void*>(&embedded_route_identity_provider_lookup)},
            g_embeddedRouteIdentityProviderLookupHandle)) {
        g_embeddedRouteIdentityProviderLookupInstallAttempted.store(false,
                                                                     std::memory_order_release);
        core::log::write(
            core::log::Channel::client,
            core::log::Level::warn,
            "ev=bootflow stage=activity_script_embedded_route_identity_provider_virtual_lookup_probe result=fail reason=attach");
        return;
    }
    g_embeddedRouteIdentityProviderLookupOriginal.store(
        reinterpret_cast<EmbeddedRouteIdentityProviderLookup>(
            g_embeddedRouteIdentityProviderLookupHandle.original),
        std::memory_order_release);
    const std::uintptr_t targetRva =
        image != nullptr && target >= image ? static_cast<std::uintptr_t>(target - image) : 0U;
    std::array<char, core::log::kLineCapacity> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_embedded_route_identity_provider_virtual_lookup_probe result=armed service=%p target_rva=0x%llX mode=observe",
        static_cast<void*>(service),
        static_cast<unsigned long long>(targetRva));
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
}

void ensure_embedded_route_identity_provider_lookup_probe() noexcept {
    (void)legacy_owner_quarantine::execute_admitted_dynamic_writer(
        LateInstallGuard{},
        []() noexcept { embedded_route_identity_provider_lookup_probe_admitted(); });
}

/** Captures the relative identity record returned to the local predicate. */
__declspec(noinline) std::byte* __fastcall embedded_route_identity_provider_record_lookup(
    std::uint16_t identifier) noexcept {
    const EmbeddedRouteIdentityProviderRecordLookup original =
        g_embeddedRouteIdentityProviderRecordLookupOriginal.load(std::memory_order_acquire);
    std::byte* const result = original != nullptr ? original(identifier) : nullptr;
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;
    std::string_view package{};
    if (callerRva == kEmbeddedRouteIdentityProviderRecordLookupReturnRva
        && embedded_route_trace_is_forced(package)) {
        const std::uint32_t observation =
            g_embeddedRouteIdentityProviderRecordLookupObserved.fetch_add(
                1U,
                std::memory_order_relaxed)
            + 1U;
        std::byte* const service = safe_read<std::byte*>(
            image != nullptr ? image + kEmbeddedRouteIdentityProviderServicePointerRva : nullptr,
            nullptr);
        std::byte* const vtable = safe_read<std::byte*>(service, nullptr);
        std::byte* const lookupTarget = safe_read<std::byte*>(
            vtable != nullptr ? vtable + kEmbeddedRouteIdentityProviderLookupVtableOffset
                              : nullptr,
            nullptr);
        const std::uintptr_t lookupTargetRva = image != nullptr && lookupTarget >= image
                                                   ? static_cast<std::uintptr_t>(
                                                         lookupTarget - image)
                                                   : 0U;
        const std::int32_t recordFirst = safe_read<std::int32_t>(result, INT32_MIN);
        const std::uint32_t recordSecond = safe_read<std::uint32_t>(
            result != nullptr ? result + sizeof(std::uint32_t) : nullptr,
            UINT32_MAX);
        std::array<std::uint32_t, 12> recordWords{};
        for (std::size_t index = 0; index < recordWords.size(); ++index) {
            recordWords[index] = safe_read<std::uint32_t>(
                result != nullptr ? result + index * sizeof(std::uint32_t) : nullptr,
                UINT32_MAX);
        }
        std::array<char, core::log::kLineCapacity> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_script_embedded_route_identity_provider_record_lookup n=%u caller_rva=0x%llX identifier=%u service=%p lookup_target_rva=0x%llX record=%p record_first=%d record_second=%u record_words=%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X expected_record_first_not_minus_one=1 package=%.*s mutation=observe_only",
            observation,
            static_cast<unsigned long long>(callerRva),
            static_cast<unsigned int>(identifier),
            static_cast<void*>(service),
            static_cast<unsigned long long>(lookupTargetRva),
            static_cast<void*>(result),
            recordFirst,
            recordSecond,
            recordWords[0],
            recordWords[1],
            recordWords[2],
            recordWords[3],
            recordWords[4],
            recordWords[5],
            recordWords[6],
            recordWords[7],
            recordWords[8],
            recordWords[9],
            recordWords[10],
            recordWords[11],
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

/** Captures the driver's local-identity availability predicate. */
__declspec(noinline) bool __fastcall embedded_route_local_identity_predicate(
    std::uint16_t identifier) noexcept {
    std::string_view package{};
    const bool opening = opening_is_forced(package) && package == "mission_towerfall";
    if (opening) {
        ensure_embedded_route_identity_provider_lookup_probe();
    }
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;
    const bool driverCall = opening && callerRva == 0xEF1EAEU;
    const EmbeddedRouteIdentityProviderRecordLookup providerLookup =
        g_embeddedRouteIdentityProviderRecordLookupOriginal.load(std::memory_order_acquire);
    std::byte* const destinationRecord =
        driverCall && identifier == kReunionSourceIdentifier && providerLookup != nullptr
            ? providerLookup(kHomecomingDestinationIdentifier)
            : nullptr;
    const std::int32_t destinationRecordFirst = safe_read<std::int32_t>(
        destinationRecord,
        INT32_MIN);
    // Change only this lane-zero predicate, and only when the destination's own native record is
    // populated. A -1 record is the provider-insertion failure we are testing for; passing 266 in
    // that case merely exchanges one invalid lookup for another.
    const bool destinationRecordValid = destinationRecord != nullptr
                                        && destinationRecordFirst != -1
                                        && destinationRecordFirst != INT32_MIN;
    const std::uint16_t effectiveIdentifier =
        driverCall && identifier == kReunionSourceIdentifier && destinationRecordValid
            ? kHomecomingDestinationIdentifier
            : identifier;
    const EmbeddedRouteIdentityPredicate original =
        g_embeddedRouteLocalIdentityPredicateOriginal.load(std::memory_order_acquire);
    const bool result = original != nullptr && original(effectiveIdentifier);
    if (driverCall) {
        const std::uint32_t controlObservation =
            g_embeddedRouteIdentityProviderControlObserved.fetch_add(
                1U,
                std::memory_order_relaxed)
            + 1U;
        const bool remapped = effectiveIdentifier != identifier;
        const std::uint32_t providerOutcome =
            !destinationRecordValid ? 1U : result ? 3U : 2U;
        g_embeddedRouteProvider266Outcome.store(providerOutcome, std::memory_order_release);
        if (controlObservation == 1U) {
            std::array<std::uint32_t, 12> controlWords{};
            for (std::size_t index = 0; index < controlWords.size(); ++index) {
                controlWords[index] = safe_read<std::uint32_t>(
                    destinationRecord != nullptr
                        ? destinationRecord + index * sizeof(std::uint32_t)
                        : nullptr,
                    UINT32_MAX);
            }
            std::array<char, core::log::kLineCapacity> controlLine{};
            const int controlLength = std::snprintf(
                controlLine.data(),
                controlLine.size(),
                "ev=bootflow stage=activity_script_embedded_route_identity_provider_control n=%u caller_rva=0x%llX driver_identifier=%u control_identifier=%u control_record=%p control_first=%d control_second=%u control_words=%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X package=%.*s mutation=conditional_valid_record_remap",
                controlObservation,
                static_cast<unsigned long long>(callerRva),
                static_cast<unsigned int>(identifier),
                static_cast<unsigned int>(kHomecomingDestinationIdentifier),
                static_cast<void*>(destinationRecord),
                static_cast<std::int32_t>(controlWords[0]),
                controlWords[1],
                controlWords[0],
                controlWords[1],
                controlWords[2],
                controlWords[3],
                controlWords[4],
                controlWords[5],
                controlWords[6],
                controlWords[7],
                controlWords[8],
                controlWords[9],
                controlWords[10],
                controlWords[11],
                static_cast<int>(package.size()),
                package.data());
            if (controlLength > 0) {
                core::log::write(
                    core::log::Channel::client,
                    core::log::Level::info,
                    {controlLine.data(), static_cast<std::size_t>(controlLength)});
            }
        }
        const std::uint32_t observation =
            g_embeddedRouteLocalIdentityPredicateObserved.fetch_add(
                1U,
                std::memory_order_relaxed)
            + 1U;
        std::array<char, core::log::kLineCapacity> line{};
        const char* const outcome = !destinationRecordValid
                                        ? "provider_record_missing"
                                        : result ? "destination_accepted"
                                                 : "destination_predicate_refused";
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_script_embedded_route_provider_266_experiment n=%u caller_rva=0x%llX requested_identifier=%u effective_identifier=%u destination_record=%p record_first=%d record_valid=%u remapped=%u predicate_result=%u outcome=%s package=%.*s mutation=%s",
            observation,
            static_cast<unsigned long long>(callerRva),
            static_cast<unsigned int>(identifier),
            static_cast<unsigned int>(effectiveIdentifier),
            static_cast<void*>(destinationRecord),
            destinationRecordFirst,
            destinationRecordValid ? 1U : 0U,
            remapped ? 1U : 0U,
            result ? 1U : 0U,
            outcome,
            static_cast<int>(package.size()),
            package.data(),
            remapped ? "late_282_to_266" : "none");
        if (length > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(length)});
        }
    }
    return result;
}

[[nodiscard]] std::int32_t embedded_route_session_activity() noexcept {
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    if (image == nullptr) {
        return -1;
    }
    const auto getter = reinterpret_cast<LaunchProducerSessionActivity>(
        image + kLaunchProducerSessionActivityRva);
    return getter(1);
}

[[nodiscard]] std::int32_t embedded_route_state_status(std::int32_t lane) noexcept {
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    if (image == nullptr) {
        return -1;
    }
    const auto getter = reinterpret_cast<EmbeddedRouteStateStatus>(
        image + kEmbeddedRouteStateStatusRva);
    return getter(lane);
}

[[nodiscard]] std::byte* embedded_route_transition_object(std::byte* owner,
                                                           std::int32_t index) noexcept {
    if (owner == nullptr || index < 0 || index >= 50) {
        return nullptr;
    }
    return safe_read<std::byte*>(owner + 0x200U + static_cast<std::size_t>(index) * 8U,
                                 nullptr);
}

[[nodiscard]] std::uintptr_t embedded_route_code_rva(std::byte* image,
                                                      std::byte* address) noexcept {
    return image != nullptr && address >= image
               ? static_cast<std::uintptr_t>(address - image)
               : 0U;
}

void dump_omega_route_focus(const char* phase,
                            std::int32_t index,
                            std::int32_t current,
                            std::byte* object,
                            std::byte* image,
                            std::uintptr_t method18Rva,
                            std::uintptr_t method20Rva,
                            std::uintptr_t method30Rva) noexcept {
    const bool stateFour = phase != nullptr && std::string_view(phase) == "script_state_4";
    const bool currentRoute = index == current;
    const bool successorRoute = current >= 0 && index == current + 1;
    if (!stateFour || (!currentRoute && !successorRoute) || object == nullptr) {
        return;
    }
    std::array<char, core::log::kLineCapacity> focus{};
    const int focusLength = std::snprintf(
        focus.data(),
        focus.size(),
        "ev=bootflow stage=omega_route_focus phase=script_state_4 role=%s slot=%d identifier=%u object=%p q00=0x%016llX q08=0x%016llX q10=0x%016llX q18=0x%016llX q20=0x%016llX q28=0x%016llX q30=0x%016llX q38=0x%016llX q40=0x%016llX q48=0x%016llX q50=0x%016llX q58=0x%016llX q60=0x%016llX q68=0x%016llX package=mission_scot mutation=observe_only",
        currentRoute ? "current" : "successor",
        index,
        static_cast<unsigned int>(safe_read<std::uint16_t>(object + 0x60U, UINT16_MAX)),
        static_cast<void*>(object),
        static_cast<unsigned long long>(safe_read<std::uint64_t>(object + 0x00U, 0U)),
        static_cast<unsigned long long>(safe_read<std::uint64_t>(object + 0x08U, 0U)),
        static_cast<unsigned long long>(safe_read<std::uint64_t>(object + 0x10U, 0U)),
        static_cast<unsigned long long>(safe_read<std::uint64_t>(object + 0x18U, 0U)),
        static_cast<unsigned long long>(safe_read<std::uint64_t>(object + 0x20U, 0U)),
        static_cast<unsigned long long>(safe_read<std::uint64_t>(object + 0x28U, 0U)),
        static_cast<unsigned long long>(safe_read<std::uint64_t>(object + 0x30U, 0U)),
        static_cast<unsigned long long>(safe_read<std::uint64_t>(object + 0x38U, 0U)),
        static_cast<unsigned long long>(safe_read<std::uint64_t>(object + 0x40U, 0U)),
        static_cast<unsigned long long>(safe_read<std::uint64_t>(object + 0x48U, 0U)),
        static_cast<unsigned long long>(safe_read<std::uint64_t>(object + 0x50U, 0U)),
        static_cast<unsigned long long>(safe_read<std::uint64_t>(object + 0x58U, 0U)),
        static_cast<unsigned long long>(safe_read<std::uint64_t>(object + 0x60U, 0U)),
        static_cast<unsigned long long>(safe_read<std::uint64_t>(object + 0x68U, 0U)));
    if (focusLength > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {focus.data(), static_cast<std::size_t>(focusLength)});
    }
    const wchar_t* const method18Name = currentRoute ? L"omega_route_38_method18"
                                                      : L"omega_route_39_method18";
    const wchar_t* const method20Name = currentRoute ? L"omega_route_38_method20"
                                                      : L"omega_route_39_method20";
    const wchar_t* const method30Name = currentRoute ? L"omega_route_38_method30"
                                                      : L"omega_route_39_method30";
    dump_runtime_code(method18Name,
                      method18Rva != 0U ? image + method18Rva : nullptr,
                      0x400U);
    dump_runtime_code(method20Name,
                      method20Rva != 0U ? image + method20Rva : nullptr,
                      0x400U);
    dump_runtime_code(method30Name,
                      method30Rva != 0U ? image + method30Rva : nullptr,
                      0x800U);
}

/** Writes one bounded inventory of the native route objects without changing the table. */
void dump_omega_route_table(std::byte* owner, const char* phase) noexcept {
    if (owner == nullptr || phase == nullptr) {
        return;
    }
    const std::int32_t current = safe_read<std::int32_t>(owner + 0x390U, -1);
    const std::int32_t currentDetail = safe_read<std::int32_t>(owner + 0x394U, -1);
    const std::int32_t pending = safe_read<std::int32_t>(owner + 0x3A0U, -1);
    const std::int32_t pendingDetail = safe_read<std::int32_t>(owner + 0x3A4U, -1);
    std::uint32_t populated = 0U;
    for (std::int32_t index = 0; index < 50; ++index) {
        if (embedded_route_transition_object(owner, index) != nullptr) {
            ++populated;
        }
    }

    std::array<char, core::log::kLineCapacity> header{};
    const int headerLength = std::snprintf(
        header.data(),
        header.size(),
        "ev=bootflow stage=omega_route_table phase=%s moment=begin owner=%p slots=50 populated=%u current=%d current_detail=%d pending=%d pending_detail=%d package=mission_scot mutation=observe_only",
        phase,
        static_cast<void*>(owner),
        populated,
        current,
        currentDetail,
        pending,
        pendingDetail);
    if (headerLength > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {header.data(), static_cast<std::size_t>(headerLength)});
    }

    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    for (std::int32_t index = 0; index < 50; ++index) {
        std::byte* const object = embedded_route_transition_object(owner, index);
        if (object == nullptr) {
            continue;
        }
        std::byte* const vtable = safe_read<std::byte*>(object, nullptr);
        const std::uintptr_t method18Rva = embedded_route_code_rva(
            image,
            safe_read<std::byte*>(vtable != nullptr ? vtable + 0x18U : nullptr, nullptr));
        const std::uintptr_t method20Rva = embedded_route_code_rva(
            image,
            safe_read<std::byte*>(vtable != nullptr ? vtable + 0x20U : nullptr, nullptr));
        const std::uintptr_t method28Rva = embedded_route_code_rva(
            image,
            safe_read<std::byte*>(vtable != nullptr ? vtable + 0x28U : nullptr, nullptr));
        const std::uintptr_t method30Rva = embedded_route_code_rva(
            image,
            safe_read<std::byte*>(vtable != nullptr ? vtable + 0x30U : nullptr, nullptr));
        const char* const selected = index == current && index == pending
                                         ? "current_pending"
                                     : index == current ? "current"
                                     : index == pending ? "pending"
                                                        : "none";
        std::array<char, core::log::kLineCapacity> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=omega_route_table phase=%s moment=slot owner=%p slot=%d selected=%s object=%p vtable=%p identifier=%u flag49=%u flag4b=%u flag4c=%u qword50=0x%016llX qword58=0x%016llX method18_rva=0x%llX method20_rva=0x%llX method28_rva=0x%llX method30_rva=0x%llX package=mission_scot mutation=observe_only",
            phase,
            static_cast<void*>(owner),
            index,
            selected,
            static_cast<void*>(object),
            static_cast<void*>(vtable),
            static_cast<unsigned int>(safe_read<std::uint16_t>(object + 0x60U, UINT16_MAX)),
            static_cast<unsigned int>(safe_read<std::uint8_t>(object + 0x49U, 0U)),
            static_cast<unsigned int>(safe_read<std::uint8_t>(object + 0x4BU, 0U)),
            static_cast<unsigned int>(safe_read<std::uint8_t>(object + 0x4CU, 0U)),
            static_cast<unsigned long long>(safe_read<std::uint64_t>(object + 0x50U, 0U)),
            static_cast<unsigned long long>(safe_read<std::uint64_t>(object + 0x58U, 0U)),
            static_cast<unsigned long long>(method18Rva),
            static_cast<unsigned long long>(method20Rva),
            static_cast<unsigned long long>(method28Rva),
            static_cast<unsigned long long>(method30Rva));
        if (length > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(length)});
        }
        dump_omega_route_focus(phase,
                               index,
                               current,
                               object,
                               image,
                               method18Rva,
                               method20Rva,
                               method30Rva);
    }
}

/** Observes which route object is selected before the lane-zero virtual driver runs. */
__declspec(noinline) void __fastcall embedded_route_object_transition(
    std::byte* owner) noexcept {
    const std::int32_t currentBefore =
        safe_read<std::int32_t>(owner != nullptr ? owner + 0x390U : nullptr, -1);
    const std::int32_t currentDetailBefore =
        safe_read<std::int32_t>(owner != nullptr ? owner + 0x394U : nullptr, -1);
    const std::int32_t pendingBefore =
        safe_read<std::int32_t>(owner != nullptr ? owner + 0x3A0U : nullptr, -1);
    const std::int32_t pendingDetailBefore =
        safe_read<std::int32_t>(owner != nullptr ? owner + 0x3A4U : nullptr, -1);
    std::byte* const currentObjectBefore =
        embedded_route_transition_object(owner, currentBefore);
    std::byte* const pendingObjectBefore =
        embedded_route_transition_object(owner, pendingBefore);
    const std::uint16_t currentIdentifierBefore = safe_read<std::uint16_t>(
        currentObjectBefore != nullptr ? currentObjectBefore + 0x60U : nullptr,
        UINT16_MAX);
    const std::uint16_t pendingIdentifierBefore = safe_read<std::uint16_t>(
        pendingObjectBefore != nullptr ? pendingObjectBefore + 0x60U : nullptr,
        UINT16_MAX);

    const EmbeddedRouteObjectTransition original =
        g_embeddedRouteObjectTransitionOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(owner);
    }

    const std::int32_t currentAfter =
        safe_read<std::int32_t>(owner != nullptr ? owner + 0x390U : nullptr, -1);
    const std::int32_t currentDetailAfter =
        safe_read<std::int32_t>(owner != nullptr ? owner + 0x394U : nullptr, -1);
    const std::int32_t pendingAfter =
        safe_read<std::int32_t>(owner != nullptr ? owner + 0x3A0U : nullptr, -1);
    const std::int32_t pendingDetailAfter =
        safe_read<std::int32_t>(owner != nullptr ? owner + 0x3A4U : nullptr, -1);
    std::byte* const currentObjectAfter =
        embedded_route_transition_object(owner, currentAfter);
    std::byte* const pendingObjectAfter =
        embedded_route_transition_object(owner, pendingAfter);
    const std::uint16_t currentIdentifierAfter = safe_read<std::uint16_t>(
        currentObjectAfter != nullptr ? currentObjectAfter + 0x60U : nullptr,
        UINT16_MAX);
    const std::uint16_t pendingIdentifierAfter = safe_read<std::uint16_t>(
        pendingObjectAfter != nullptr ? pendingObjectAfter + 0x60U : nullptr,
        UINT16_MAX);
    std::string_view package{};
    const bool opening = embedded_route_trace_is_forced(package);
    if (opening && package == "mission_scot" && owner != nullptr) {
        g_omegaRouteOwner.store(owner, std::memory_order_release);
        if (!g_omegaRouteEntrySnapshotDumped.exchange(true, std::memory_order_acq_rel)) {
            dump_omega_route_table(owner, "activity_entry");
        }
        if (g_omegaForestRouteTraceArmed.load(std::memory_order_acquire)
            && !g_omegaRouteState4SnapshotDumped.exchange(true,
                                                           std::memory_order_acq_rel)) {
            dump_omega_route_table(owner, "script_state_4");
        }

        auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
        auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
        const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                             ? static_cast<std::uintptr_t>(returnAddress - image)
                                             : 0U;
        std::byte* const previousOwner =
            g_omegaRoutePreviousOwner.exchange(owner, std::memory_order_acq_rel);
        const std::int32_t previousCurrent =
            g_omegaRoutePreviousCurrent.exchange(currentAfter, std::memory_order_acq_rel);
        const std::int32_t previousCurrentDetail =
            g_omegaRoutePreviousCurrentDetail.exchange(currentDetailAfter,
                                                        std::memory_order_acq_rel);
        const std::int32_t previousPending =
            g_omegaRoutePreviousPending.exchange(pendingAfter, std::memory_order_acq_rel);
        const std::int32_t previousPendingDetail =
            g_omegaRoutePreviousPendingDetail.exchange(pendingDetailAfter,
                                                        std::memory_order_acq_rel);
        const bool ownerStable = previousOwner == owner;
        const bool changedBetweenCalls =
            ownerStable && (previousCurrent != currentBefore
                            || previousCurrentDetail != currentDetailBefore
                            || previousPending != pendingBefore
                            || previousPendingDetail != pendingDetailBefore);
        const bool changedInsideCall = currentBefore != currentAfter
                                       || currentDetailBefore != currentDetailAfter
                                       || pendingBefore != pendingAfter
                                       || pendingDetailBefore != pendingDetailAfter;
        if (changedBetweenCalls || changedInsideCall) {
            std::array<char, core::log::kLineCapacity> changeLine{};
            const int changeLength = std::snprintf(
                changeLine.data(),
                changeLine.size(),
                "ev=bootflow stage=omega_route_selection_change boundary=%s caller_rva=0x%llX owner=%p previous_current=%d previous_current_detail=%d previous_pending=%d previous_pending_detail=%d before_current=%d before_current_detail=%d before_pending=%d before_pending_detail=%d after_current=%d after_current_detail=%d after_pending=%d after_pending_detail=%d package=mission_scot mutation=observe_only",
                changedInsideCall ? "transition_call" : "between_calls",
                static_cast<unsigned long long>(callerRva),
                static_cast<void*>(owner),
                previousCurrent,
                previousCurrentDetail,
                previousPending,
                previousPendingDetail,
                currentBefore,
                currentDetailBefore,
                pendingBefore,
                pendingDetailBefore,
                currentAfter,
                currentDetailAfter,
                pendingAfter,
                pendingDetailAfter);
            if (changeLength > 0) {
                core::log::write(core::log::Channel::client,
                                 core::log::Level::info,
                                 {changeLine.data(), static_cast<std::size_t>(changeLength)});
            }
        }
    }
    const bool transition = currentBefore != pendingBefore
                            || currentDetailBefore != pendingDetailBefore
                            || currentBefore != currentAfter
                            || currentDetailBefore != currentDetailAfter;
    // After state 4 lands, unchanged calls matter too: they prove the native dispatcher ran but
    // selected no successor object. The arm is bounded by the observation cap below.
    const bool omegaForestTrace = package == "mission_scot"
                                  && g_omegaForestRouteTraceArmed.load(
                                      std::memory_order_acquire);
    if (!opening || (!transition && !omegaForestTrace)) {
        return;
    }
    const std::uint32_t observation =
        g_embeddedRouteObjectTransitionObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
    if (observation > (omegaForestTrace ? 256U : 64U)) {
        return;
    }
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    std::byte* const inspectedObject =
        pendingObjectBefore != nullptr ? pendingObjectBefore : currentObjectAfter;
    std::byte* const vtable = safe_read<std::byte*>(inspectedObject, nullptr);
    const std::uintptr_t method18Rva = embedded_route_code_rva(
        image,
        safe_read<std::byte*>(vtable != nullptr ? vtable + 0x18U : nullptr, nullptr));
    const std::uintptr_t method20Rva = embedded_route_code_rva(
        image,
        safe_read<std::byte*>(vtable != nullptr ? vtable + 0x20U : nullptr, nullptr));
    std::byte* const method28 =
        safe_read<std::byte*>(vtable != nullptr ? vtable + 0x28U : nullptr, nullptr);
    std::byte* const method30 =
        safe_read<std::byte*>(vtable != nullptr ? vtable + 0x30U : nullptr, nullptr);
    const std::uintptr_t method28Rva = embedded_route_code_rva(image, method28);
    const std::uintptr_t method30Rva = embedded_route_code_rva(image, method30);
    const std::uint16_t inspectedIdentifier = inspectedObject == currentObjectAfter
                                                  ? currentIdentifierAfter
                                                  : pendingIdentifierBefore;
    if (inspectedIdentifier == 282U && method28 != nullptr
        && !g_embeddedRouteObject282PredicateDumped.exchange(true,
                                                              std::memory_order_acq_rel)) {
        dump_runtime_code(L"activity_script_route_object_282_predicate", method28, 0x400U);
    }
    if (inspectedIdentifier == 282U && method30 != nullptr
        && !g_embeddedRouteObject282ActionDumped.exchange(true,
                                                           std::memory_order_acq_rel)) {
        dump_runtime_code(L"activity_script_route_object_282_action", method30, 0x800U);
    }
    std::array<char, core::log::kLineCapacity> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_embedded_route_object_transition n=%u owner=%p current_before=%d current_detail_before=%d pending_before=%d pending_detail_before=%d current_object_before=%p current_identifier_before=%u pending_object_before=%p pending_identifier_before=%u current_after=%d current_detail_after=%d pending_after=%d pending_detail_after=%d current_object_after=%p current_identifier_after=%u pending_object_after=%p pending_identifier_after=%u inspected_object=%p vtable=%p method18_rva=0x%llX method20_rva=0x%llX method28_rva=0x%llX method30_rva=0x%llX package=%.*s mutation=observe_only",
        observation,
        static_cast<void*>(owner),
        currentBefore,
        currentDetailBefore,
        pendingBefore,
        pendingDetailBefore,
        static_cast<void*>(currentObjectBefore),
        static_cast<unsigned int>(currentIdentifierBefore),
        static_cast<void*>(pendingObjectBefore),
        static_cast<unsigned int>(pendingIdentifierBefore),
        currentAfter,
        currentDetailAfter,
        pendingAfter,
        pendingDetailAfter,
        static_cast<void*>(currentObjectAfter),
        static_cast<unsigned int>(currentIdentifierAfter),
        static_cast<void*>(pendingObjectAfter),
        static_cast<unsigned int>(pendingIdentifierAfter),
        static_cast<void*>(inspectedObject),
        static_cast<void*>(vtable),
        static_cast<unsigned long long>(method18Rva),
        static_cast<unsigned long long>(method20Rva),
        static_cast<unsigned long long>(method28Rva),
        static_cast<unsigned long long>(method30Rva),
        static_cast<int>(package.size()),
        package.data());
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
}

/** Observes the only native driver that can initialize lane zero as phase three. */
__declspec(noinline) void __fastcall embedded_route_lane_zero_driver(
    std::byte* context) noexcept {
    std::byte* const state = embedded_route_state_for_lane(0);
    const std::int32_t sessionBefore = embedded_route_session_activity();
    const std::int32_t statusBefore = embedded_route_state_status(0);
    const std::uint64_t timestampBefore =
        safe_read<std::uint64_t>(state != nullptr ? state + 0x18 : nullptr, UINT64_MAX);
    const std::uint8_t phaseBefore =
        safe_read<std::uint8_t>(state != nullptr ? state + 0x20 : nullptr, 0U);
    const std::uint8_t flagsBefore =
        safe_read<std::uint8_t>(state != nullptr ? state + 0x21 : nullptr, 0U);
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;
    std::byte* const contextVtable = safe_read<std::byte*>(context, nullptr);
    const std::uintptr_t method18Rva = embedded_route_code_rva(
        image,
        safe_read<std::byte*>(contextVtable != nullptr ? contextVtable + 0x18U : nullptr,
                              nullptr));
    const std::uintptr_t method20Rva = embedded_route_code_rva(
        image,
        safe_read<std::byte*>(contextVtable != nullptr ? contextVtable + 0x20U : nullptr,
                              nullptr));
    const std::uintptr_t method28Rva = embedded_route_code_rva(
        image,
        safe_read<std::byte*>(contextVtable != nullptr ? contextVtable + 0x28U : nullptr,
                              nullptr));
    const std::uintptr_t method30Rva = embedded_route_code_rva(
        image,
        safe_read<std::byte*>(contextVtable != nullptr ? contextVtable + 0x30U : nullptr,
                              nullptr));

    const EmbeddedRouteLaneZeroDriver original =
        g_embeddedRouteLaneZeroDriverOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(context);
    }

    const std::int32_t sessionAfter = embedded_route_session_activity();
    const std::int32_t statusAfter = embedded_route_state_status(0);
    const std::uint64_t timestampAfter =
        safe_read<std::uint64_t>(state != nullptr ? state + 0x18 : nullptr, UINT64_MAX);
    const std::uint8_t phaseAfter =
        safe_read<std::uint8_t>(state != nullptr ? state + 0x20 : nullptr, 0U);
    const std::uint8_t flagsAfter =
        safe_read<std::uint8_t>(state != nullptr ? state + 0x21 : nullptr, 0U);
    const std::uint32_t observation =
        g_embeddedRouteLaneZeroDriverObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
    std::string_view package{};
    const bool opening = embedded_route_trace_is_forced(package);
    const std::uint32_t openingObservation = opening
                                                 ? g_embeddedRouteLaneZeroDriverOpeningObserved
                                                       .fetch_add(1U, std::memory_order_relaxed)
                                                   + 1U
                                                 : 0U;
    const bool changed = sessionBefore != sessionAfter || statusBefore != statusAfter
                         || timestampBefore != timestampAfter || phaseBefore != phaseAfter
                         || flagsBefore != flagsAfter;
    const std::uint32_t providerOutcome =
        g_embeddedRouteProvider266Outcome.load(std::memory_order_acquire);
    const char* const providerOutcomeName =
        providerOutcome == 1U ? "provider_record_missing"
        : providerOutcome == 2U ? "destination_predicate_refused"
        : providerOutcome == 3U ? "destination_accepted"
                                : "untested";
    if (observation <= 16U || (opening && openingObservation <= 64U) || changed) {
        std::array<char, core::log::kLineCapacity> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_script_embedded_route_lane0_driver n=%u opening_n=%u caller_rva=0x%llX context=%p vtable=%p method18_rva=0x%llX method20_rva=0x%llX method28_rva=0x%llX method30_rva=0x%llX identifier=%u flag49=%u flag4b=%u flag4c=%u state=%p session_before=%d session_after=%d status_before=%d status_after=%d phase_before=0x%02X phase_after=0x%02X flags_before=0x%02X flags_after=0x%02X timestamp_before=0x%llX timestamp_after=0x%llX provider266_outcome=%s lane0_phase3=%u package=%.*s mutation=observe_only",
            observation,
            openingObservation,
            static_cast<unsigned long long>(callerRva),
            static_cast<void*>(context),
            static_cast<void*>(contextVtable),
            static_cast<unsigned long long>(method18Rva),
            static_cast<unsigned long long>(method20Rva),
            static_cast<unsigned long long>(method28Rva),
            static_cast<unsigned long long>(method30Rva),
            static_cast<unsigned int>(safe_read<std::uint16_t>(
                context != nullptr ? context + 0x60 : nullptr,
                0U)),
            static_cast<unsigned int>(safe_read<std::uint8_t>(
                context != nullptr ? context + 0x49 : nullptr,
                0U)),
            static_cast<unsigned int>(safe_read<std::uint8_t>(
                context != nullptr ? context + 0x4B : nullptr,
                0U)),
            static_cast<unsigned int>(safe_read<std::uint8_t>(
                context != nullptr ? context + 0x4C : nullptr,
                0U)),
            static_cast<void*>(state),
            sessionBefore,
            sessionAfter,
            statusBefore,
            statusAfter,
            static_cast<unsigned int>(phaseBefore),
            static_cast<unsigned int>(phaseAfter),
            static_cast<unsigned int>(flagsBefore),
            static_cast<unsigned int>(flagsAfter),
            static_cast<unsigned long long>(timestampBefore),
            static_cast<unsigned long long>(timestampAfter),
            providerOutcomeName,
            phaseAfter == 3U ? 1U : 0U,
            static_cast<int>(package.size()),
            package.data());
        if (length > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(length)});
        }
    }
}

/** Observes the high-level transition handler that unconditionally resets lane zero. */
__declspec(noinline) void __fastcall embedded_route_transition_handler(
    std::byte* context,
    std::int32_t eventCode,
    std::int32_t detail) noexcept {
    std::byte* const state = embedded_route_state_for_lane(0);
    const std::int32_t sessionBefore = embedded_route_session_activity();
    const std::int32_t statusBefore = embedded_route_state_status(0);
    const std::uint64_t timestampBefore =
        safe_read<std::uint64_t>(state != nullptr ? state + 0x18 : nullptr, UINT64_MAX);
    const std::uint8_t phaseBefore =
        safe_read<std::uint8_t>(state != nullptr ? state + 0x20 : nullptr, 0U);
    const std::uint8_t flagsBefore =
        safe_read<std::uint8_t>(state != nullptr ? state + 0x21 : nullptr, 0U);
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;

    const EmbeddedRouteTransitionHandler original =
        g_embeddedRouteTransitionHandlerOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(context, eventCode, detail);
    }

    const std::int32_t sessionAfter = embedded_route_session_activity();
    const std::int32_t statusAfter = embedded_route_state_status(0);
    const std::uint64_t timestampAfter =
        safe_read<std::uint64_t>(state != nullptr ? state + 0x18 : nullptr, UINT64_MAX);
    const std::uint8_t phaseAfter =
        safe_read<std::uint8_t>(state != nullptr ? state + 0x20 : nullptr, 0U);
    const std::uint8_t flagsAfter =
        safe_read<std::uint8_t>(state != nullptr ? state + 0x21 : nullptr, 0U);
    const std::uint32_t observation =
        g_embeddedRouteTransitionHandlerObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
    std::string_view package{};
    const bool opening = embedded_route_trace_is_forced(package);
    if (observation <= 32U || opening) {
        std::array<char, core::log::kLineCapacity> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_script_embedded_route_transition_handler n=%u caller_rva=0x%llX context=%p event=%d detail=%d identifier=%u state=%p session_before=%d session_after=%d status_before=%d status_after=%d phase_before=0x%02X phase_after=0x%02X flags_before=0x%02X flags_after=0x%02X timestamp_before=0x%llX timestamp_after=0x%llX package=%.*s mutation=observe_only",
            observation,
            static_cast<unsigned long long>(callerRva),
            static_cast<void*>(context),
            eventCode,
            detail,
            static_cast<unsigned int>(safe_read<std::uint16_t>(
                context != nullptr ? context + 0x60 : nullptr,
                0U)),
            static_cast<void*>(state),
            sessionBefore,
            sessionAfter,
            statusBefore,
            statusAfter,
            static_cast<unsigned int>(phaseBefore),
            static_cast<unsigned int>(phaseAfter),
            static_cast<unsigned int>(flagsBefore),
            static_cast<unsigned int>(flagsAfter),
            static_cast<unsigned long long>(timestampBefore),
            static_cast<unsigned long long>(timestampAfter),
            static_cast<int>(package.size()),
            package.data());
        if (length > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(length)});
        }
    }
}

/** Observes the native lane-readiness flags that decide whether lane one is pumped. */
__declspec(noinline) void __fastcall route_flag_update(std::byte* state) noexcept {
    const std::uint8_t phaseBefore =
        safe_read<std::uint8_t>(state != nullptr ? state + 0x20 : nullptr, 0U);
    const std::uint8_t flagsBefore =
        safe_read<std::uint8_t>(state != nullptr ? state + 0x21 : nullptr, 0U);
    const std::int32_t stateBefore =
        safe_read<std::int32_t>(state != nullptr ? state + 0x14 : nullptr, -1);
    const std::int32_t sequenceBefore =
        safe_read<std::int32_t>(state != nullptr ? state + 0x1198 : nullptr, -1);
    const std::uint64_t phaseTimestamp =
        safe_read<std::uint64_t>(state != nullptr ? state + 0x18 : nullptr, 0U);
    const std::uint64_t selectionTimestamp =
        safe_read<std::uint64_t>(state != nullptr ? state + 0x11A0 : nullptr, 0U);

    const RouteFlagUpdate original =
        g_routeFlagUpdateOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(state);
    }

    const std::uint8_t phaseAfter =
        safe_read<std::uint8_t>(state != nullptr ? state + 0x20 : nullptr, 0U);
    const std::uint8_t flagsAfter =
        safe_read<std::uint8_t>(state != nullptr ? state + 0x21 : nullptr, 0U);
    const std::int32_t stateAfter =
        safe_read<std::int32_t>(state != nullptr ? state + 0x14 : nullptr, -1);
    const std::int32_t sequenceAfter =
        safe_read<std::int32_t>(state != nullptr ? state + 0x1198 : nullptr, -1);
    std::string_view package{};
    const bool opening = embedded_route_trace_is_forced(package);
    const bool relevant = ((flagsBefore | flagsAfter) & 0x03U) != 0U
                          || flagsBefore != flagsAfter || stateBefore != stateAfter;
    const bool tracked = state != nullptr
                         && state == g_routeParentState.load(std::memory_order_acquire);
    if ((opening && relevant) || tracked) {
        const std::uint32_t observation =
            g_routeFlagUpdateObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
        const bool changed = phaseBefore != phaseAfter || flagsBefore != flagsAfter
                             || stateBefore != stateAfter || sequenceBefore != sequenceAfter;
        if (observation <= 64U || changed) {
            std::array<char, core::log::kLineCapacity> line{};
            const int length = std::snprintf(
                line.data(),
                line.size(),
                "ev=bootflow stage=activity_script_route_flag_update n=%u state=%p phase_before=0x%02X phase_after=0x%02X flags_before=0x%02X flags_after=0x%02X lane1_before=%u lane1_after=%u lane2_before=%u lane2_after=%u route_state_before=%d route_state_after=%d sequence_before=%d sequence_after=%d phase_timestamp=0x%llX selection_timestamp=0x%llX package=%.*s mutation=observe_only",
                observation,
                static_cast<void*>(state),
                static_cast<unsigned int>(phaseBefore),
                static_cast<unsigned int>(phaseAfter),
                static_cast<unsigned int>(flagsBefore),
                static_cast<unsigned int>(flagsAfter),
                (flagsBefore & 0x01U) != 0U ? 1U : 0U,
                (flagsAfter & 0x01U) != 0U ? 1U : 0U,
                (flagsBefore & 0x02U) != 0U ? 1U : 0U,
                (flagsAfter & 0x02U) != 0U ? 1U : 0U,
                stateBefore,
                stateAfter,
                sequenceBefore,
                sequenceAfter,
                static_cast<unsigned long long>(phaseTimestamp),
                static_cast<unsigned long long>(selectionTimestamp),
                static_cast<int>(package.size()),
                package.data());
            if (length > 0) {
                core::log::write(core::log::Channel::client,
                                 core::log::Level::info,
                                 {line.data(), static_cast<std::size_t>(length)});
            }
        }
    }
}

/** Observes the two elapsed-time comparisons that gate the lane-one flag. */
__declspec(noinline) std::int64_t __fastcall route_elapsed_time(
    std::uint64_t timestamp) noexcept {
    const RouteElapsedTime original =
        g_routeElapsedTimeOriginal.load(std::memory_order_acquire);
    const std::int64_t result = original != nullptr ? original(timestamp) : -1;
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;
    const bool phaseTimer = callerRva == 0xC1D71FU;
    const bool selectionTimer = callerRva == 0xC1D73BU;
    std::string_view package{};
    const bool opening = embedded_route_trace_is_forced(package);
    if (opening && (phaseTimer || selectionTimer)) {
        const std::int32_t threshold = phaseTimer
                                           ? g_routePhaseTimeoutMs.load(std::memory_order_acquire)
                                           : g_routeSelectionTimeoutMs.load(std::memory_order_acquire);
        const bool passed = threshold >= 0 && result > static_cast<std::int64_t>(threshold);
        std::atomic_bool* const passedObserved = phaseTimer
                                                    ? &g_routePhaseTimeoutPassedObserved
                                                    : &g_routeSelectionTimeoutPassedObserved;
        const bool firstPassed = passed
                                 && !passedObserved->exchange(true, std::memory_order_acq_rel);
        const std::uint32_t observation =
            g_routeElapsedTimeObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
        if (observation <= 24U || observation % 60U == 0U || firstPassed) {
            std::array<char, core::log::kLineCapacity> line{};
            const int length = std::snprintf(
                line.data(),
                line.size(),
                "ev=bootflow stage=activity_script_lane1_timer n=%u timer=%s caller_rva=0x%llX timestamp=0x%llX elapsed=%lld threshold=%d passed=%u first_pass=%u package=%.*s mutation=observe_only",
                observation,
                phaseTimer ? "phase" : "selection",
                static_cast<unsigned long long>(callerRva),
                static_cast<unsigned long long>(timestamp),
                static_cast<long long>(result),
                threshold,
                passed ? 1U : 0U,
                firstPassed ? 1U : 0U,
                static_cast<int>(package.size()),
                package.data());
            if (length > 0) {
                core::log::write(core::log::Channel::client,
                                 core::log::Level::info,
                                 {line.data(), static_cast<std::size_t>(length)});
            }
        }
    }
    return result;
}

/** Captures the native timeout values used by the lane-one elapsed-time gates. */
__declspec(noinline) std::byte* __fastcall route_timeout_config() noexcept {
    const RouteTimeoutConfig original =
        g_routeTimeoutConfigOriginal.load(std::memory_order_acquire);
    std::byte* const result = original != nullptr ? original() : nullptr;
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;
    const bool phaseTimer = callerRva == 0xC1D712U;
    const bool selectionTimer = callerRva == 0xC1D72BU;
    std::string_view package{};
    const bool opening = embedded_route_trace_is_forced(package);
    if (opening && (phaseTimer || selectionTimer)) {
        const std::int32_t phaseTimeout =
            safe_read<std::int32_t>(result != nullptr ? result + 0x34 : nullptr, -1);
        const std::int32_t selectionTimeout =
            safe_read<std::int32_t>(result != nullptr ? result + 0x3C : nullptr, -1);
        g_routePhaseTimeoutMs.store(phaseTimeout, std::memory_order_release);
        g_routeSelectionTimeoutMs.store(selectionTimeout, std::memory_order_release);
        const std::uint32_t observation =
            g_routeTimeoutConfigObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
        if (observation <= 8U) {
            std::array<char, core::log::kLineCapacity> line{};
            const int length = std::snprintf(
                line.data(),
                line.size(),
                "ev=bootflow stage=activity_script_lane1_timer_config n=%u caller_rva=0x%llX config=%p phase_threshold=%d selection_threshold=%d package=%.*s mutation=observe_only",
                observation,
                static_cast<unsigned long long>(callerRva),
                static_cast<void*>(result),
                phaseTimeout,
                selectionTimeout,
                static_cast<int>(package.size()),
                package.data());
            if (length > 0) {
                core::log::write(core::log::Channel::client,
                                 core::log::Level::info,
                                 {line.data(), static_cast<std::size_t>(length)});
            }
        }
    }
    return result;
}

/** Observes the alternate virtual updater that chooses between lane one and lane two. */
__declspec(noinline) void __fastcall route_alternate_update(std::byte* state) noexcept {
    const std::uint8_t phaseBefore =
        safe_read<std::uint8_t>(state != nullptr ? state + 0x20 : nullptr, 0U);
    const std::uint8_t flagsBefore =
        safe_read<std::uint8_t>(state != nullptr ? state + 0x21 : nullptr, 0U);
    const std::int32_t stateBefore =
        safe_read<std::int32_t>(state != nullptr ? state + 0x14 : nullptr, -1);
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;

    const RouteAlternateUpdate original =
        g_routeAlternateUpdateOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(state);
    }

    const std::uint8_t phaseAfter =
        safe_read<std::uint8_t>(state != nullptr ? state + 0x20 : nullptr, 0U);
    const std::uint8_t flagsAfter =
        safe_read<std::uint8_t>(state != nullptr ? state + 0x21 : nullptr, 0U);
    const std::int32_t stateAfter =
        safe_read<std::int32_t>(state != nullptr ? state + 0x14 : nullptr, -1);
    std::string_view package{};
    const bool opening = embedded_route_trace_is_forced(package);
    const bool tracked = state != nullptr
                         && state == g_routeParentState.load(std::memory_order_acquire);
    if (opening || tracked) {
        const std::uint32_t observation =
            g_routeAlternateUpdateObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
        const bool changed = phaseBefore != phaseAfter || flagsBefore != flagsAfter
                             || stateBefore != stateAfter;
        if (observation <= 64U || changed) {
            std::array<char, core::log::kLineCapacity> line{};
            const int length = std::snprintf(
                line.data(),
                line.size(),
                "ev=bootflow stage=activity_script_route_alternate_update n=%u caller_rva=0x%llX state=%p phase_before=0x%02X phase_after=0x%02X flags_before=0x%02X flags_after=0x%02X lane1_before=%u lane1_after=%u lane2_before=%u lane2_after=%u route_state_before=%d route_state_after=%d package=%.*s mutation=observe_only",
                observation,
                static_cast<unsigned long long>(callerRva),
                static_cast<void*>(state),
                static_cast<unsigned int>(phaseBefore),
                static_cast<unsigned int>(phaseAfter),
                static_cast<unsigned int>(flagsBefore),
                static_cast<unsigned int>(flagsAfter),
                (flagsBefore & 0x01U) != 0U ? 1U : 0U,
                (flagsAfter & 0x01U) != 0U ? 1U : 0U,
                (flagsBefore & 0x02U) != 0U ? 1U : 0U,
                (flagsAfter & 0x02U) != 0U ? 1U : 0U,
                stateBefore,
                stateAfter,
                static_cast<int>(package.size()),
                package.data());
            if (length > 0) {
                core::log::write(core::log::Channel::client,
                                 core::log::Level::info,
                                 {line.data(), static_cast<std::size_t>(length)});
            }
        }
    }
}

/** Observes the state-machine decision immediately before route dispatch. */
__declspec(noinline) void __fastcall route_state_evaluator(std::byte* state) noexcept {
    const std::int32_t selectorBefore =
        safe_read<std::int32_t>(state, -1);
    if (state != nullptr && selectorBefore == 0) {
        g_embeddedRouteLaneZeroState.store(state, std::memory_order_release);
    }
    const std::int32_t routeStateBefore =
        safe_read<std::int32_t>(state != nullptr ? state + 0x14 : nullptr, -1);
    const std::uint8_t phaseBefore =
        safe_read<std::uint8_t>(state != nullptr ? state + 0x20 : nullptr, 0U);
    const std::uint8_t flagsBefore =
        safe_read<std::uint8_t>(state != nullptr ? state + 0x21 : nullptr, 0U);

    const RouteStateEvaluator original =
        g_routeStateEvaluatorOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(state);
    }

    const std::int32_t selectorAfter =
        safe_read<std::int32_t>(state, -1);
    const std::int32_t routeStateAfter =
        safe_read<std::int32_t>(state != nullptr ? state + 0x14 : nullptr, -1);
    const std::uint8_t phaseAfter =
        safe_read<std::uint8_t>(state != nullptr ? state + 0x20 : nullptr, 0U);
    const std::uint8_t flagsAfter =
        safe_read<std::uint8_t>(state != nullptr ? state + 0x21 : nullptr, 0U);
    std::string_view package{};
    const bool opening = embedded_route_trace_is_forced(package);
    const bool tracked = state != nullptr
                         && state == g_routeParentState.load(std::memory_order_acquire);
    if (opening || tracked) {
        const std::uint32_t observation =
            g_routeStateEvaluatorObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
        const bool changed = selectorBefore != selectorAfter
                             || routeStateBefore != routeStateAfter
                             || phaseBefore != phaseAfter || flagsBefore != flagsAfter;
        if (observation <= 128U || changed) {
            const char* branch = selectorBefore == 0
                                     ? "selector0_virtual_choice"
                                     : selectorBefore == 1
                                           ? "selector1_requires_phase_bit1"
                                           : selectorBefore == 2
                                                 ? "selector2_requires_phase_bit0"
                                                 : "selector_other_no_update";
            std::array<char, core::log::kLineCapacity> line{};
            const int length = std::snprintf(
                line.data(),
                line.size(),
                "ev=bootflow stage=activity_script_route_state_evaluator n=%u state=%p tracked=%u selector_before=%d selector_after=%d route_state_before=%d route_state_after=%d phase_before=0x%02X phase_after=0x%02X flags_before=0x%02X flags_after=0x%02X branch=%s package=%.*s mutation=observe_only",
                observation,
                static_cast<void*>(state),
                tracked ? 1U : 0U,
                selectorBefore,
                selectorAfter,
                routeStateBefore,
                routeStateAfter,
                static_cast<unsigned int>(phaseBefore),
                static_cast<unsigned int>(phaseAfter),
                static_cast<unsigned int>(flagsBefore),
                static_cast<unsigned int>(flagsAfter),
                branch,
                static_cast<int>(package.size()),
                package.data());
            if (length > 0) {
                core::log::write(core::log::Channel::client,
                                 core::log::Level::info,
                                 {line.data(), static_cast<std::size_t>(length)});
            }
        }
    }
}

/** Observes the selector-0 virtual lookup result used to choose the updater family. */
__declspec(noinline) std::byte* __fastcall route_selector_variant_lookup(
    std::byte* context,
    std::int32_t selector) noexcept {
    const RouteSelectorVariantLookup original =
        g_routeSelectorVariantLookupOriginal.load(std::memory_order_acquire);
    std::byte* const result = original != nullptr ? original(context, selector) : nullptr;
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;
    std::string_view package{};
    const bool opening = embedded_route_trace_is_forced(package);
    const bool tracked = g_routeParentState.load(std::memory_order_acquire) != nullptr;
    if (callerRva == 0xC1C39EU && (opening || tracked)) {
        const std::uint32_t observation =
            g_routeSelectorVariantLookupObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
        if (observation <= 128U) {
            const std::uint8_t discriminator =
                safe_read<std::uint8_t>(result != nullptr ? result + 2 : nullptr, 0xFFU);
            std::array<char, core::log::kLineCapacity> line{};
            const int length = std::snprintf(
                line.data(),
                line.size(),
                "ev=bootflow stage=activity_script_route_selector_variant n=%u caller_rva=0x%llX context=%p selector=%d result=%p word0=0x%04X discriminator=%u updater=%s qword8=0x%llX package=%.*s mutation=observe_only",
                observation,
                static_cast<unsigned long long>(callerRva),
                static_cast<void*>(context),
                selector,
                static_cast<void*>(result),
                static_cast<unsigned int>(safe_read<std::uint16_t>(result, 0U)),
                static_cast<unsigned int>(discriminator),
                discriminator == 0U ? "timer_flag_update" : "alternate_update",
                static_cast<unsigned long long>(safe_read<std::uint64_t>(
                    result != nullptr ? result + 8 : nullptr,
                    0U)),
                static_cast<int>(package.size()),
                package.data());
            if (length > 0) {
                core::log::write(core::log::Channel::client,
                                 core::log::Level::info,
                                 {line.data(), static_cast<std::size_t>(length)});
            }
        }
    }
    return result;
}

/** Observes the parent dispatcher that conditionally calls the lane-one and lane-two pumps. */
__declspec(noinline) void __fastcall route_parent_dispatch(std::byte* state) noexcept {
    const std::int32_t selectorBefore = safe_read<std::int32_t>(state, -1);
    const std::int32_t routeStateBefore =
        safe_read<std::int32_t>(state != nullptr ? state + 0x14 : nullptr, -1);
    const std::uint8_t phaseBefore =
        safe_read<std::uint8_t>(state != nullptr ? state + 0x20 : nullptr, 0U);
    const std::uint8_t flagsBefore =
        safe_read<std::uint8_t>(state != nullptr ? state + 0x21 : nullptr, 0U);
    const RouteParentDispatch original =
        g_routeParentDispatchOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(state);
    }
    const std::uint8_t phaseAfter =
        safe_read<std::uint8_t>(state != nullptr ? state + 0x20 : nullptr, 0U);
    const std::uint8_t flagsAfter =
        safe_read<std::uint8_t>(state != nullptr ? state + 0x21 : nullptr, 0U);
    const std::int32_t selectorAfter = safe_read<std::int32_t>(state, -1);
    const std::int32_t routeStateAfter =
        safe_read<std::int32_t>(state != nullptr ? state + 0x14 : nullptr, -1);
    std::string_view package{};
    const bool opening = embedded_route_trace_is_forced(package);
    const bool tracked = state != nullptr
                         && state == g_routeParentState.load(std::memory_order_acquire);
    if (opening || tracked) {
        g_routeParentState.store(state, std::memory_order_release);
        const std::uint32_t observation =
            g_routeParentDispatchObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
        if (observation <= 96U || phaseBefore != phaseAfter || flagsBefore != flagsAfter) {
            std::array<char, core::log::kLineCapacity> line{};
            const int length = std::snprintf(
                line.data(),
                line.size(),
                "ev=bootflow stage=activity_script_route_parent_dispatch n=%u state=%p selector_before=%d selector_after=%d route_state_before=%d route_state_after=%d phase_before=0x%02X phase_after=0x%02X flags_before=0x%02X flags_after=0x%02X lane1_before=%u lane1_after=%u lane2_before=%u lane2_after=%u package=%.*s mutation=observe_only",
                observation,
                static_cast<void*>(state),
                selectorBefore,
                selectorAfter,
                routeStateBefore,
                routeStateAfter,
                static_cast<unsigned int>(phaseBefore),
                static_cast<unsigned int>(phaseAfter),
                static_cast<unsigned int>(flagsBefore),
                static_cast<unsigned int>(flagsAfter),
                (flagsBefore & 0x01U) != 0U ? 1U : 0U,
                (flagsAfter & 0x01U) != 0U ? 1U : 0U,
                (flagsBefore & 0x02U) != 0U ? 1U : 0U,
                (flagsAfter & 0x02U) != 0U ? 1U : 0U,
                static_cast<int>(package.size()),
                package.data());
            if (length > 0) {
                core::log::write(core::log::Channel::client,
                                 core::log::Level::info,
                                 {line.data(), static_cast<std::size_t>(length)});
            }
        }
    }
}

/** Observes the native source object used by the alternate lane decision. */
__declspec(noinline) std::byte* __fastcall route_alternate_source() noexcept {
    const RouteAlternateSource original =
        g_routeAlternateSourceOriginal.load(std::memory_order_acquire);
    std::byte* const result = original != nullptr ? original() : nullptr;
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;
    std::string_view package{};
    const bool opening = embedded_route_trace_is_forced(package);
    if (opening && callerRva == 0xC1AD49U) {
        const std::uint32_t observation =
            g_routeAlternateDecisionObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
        if (observation <= 64U) {
            std::array<char, core::log::kLineCapacity> line{};
            const int length = std::snprintf(
                line.data(),
                line.size(),
                "ev=bootflow stage=activity_script_route_alternate_decision n=%u step=source caller_rva=0x%llX source=%p mode=%d slot0=0x%llX slot1=0x%llX package=%.*s mutation=observe_only",
                observation,
                static_cast<unsigned long long>(callerRva),
                static_cast<void*>(result),
                safe_read<std::int32_t>(result != nullptr ? result + 0x40 : nullptr, -1),
                static_cast<unsigned long long>(safe_read<std::uint64_t>(
                    result != nullptr ? result + 0x18 : nullptr,
                    0U)),
                static_cast<unsigned long long>(safe_read<std::uint64_t>(
                    result != nullptr ? result + 0x30 : nullptr,
                    0U)),
                static_cast<int>(package.size()),
                package.data());
            if (length > 0) {
                core::log::write(core::log::Channel::client,
                                 core::log::Level::info,
                                 {line.data(), static_cast<std::size_t>(length)});
            }
        }
    }
    return result;
}

/** Observes whether the alternate source has an active entry for its selected mode. */
__declspec(noinline) bool __fastcall route_alternate_predicate(std::byte* source) noexcept {
    const RouteAlternatePredicate original =
        g_routeAlternatePredicateOriginal.load(std::memory_order_acquire);
    const bool result = original != nullptr && original(source);
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;
    std::string_view package{};
    const bool opening = embedded_route_trace_is_forced(package);
    if (opening && callerRva == 0xC1AD66U) {
        const std::uint32_t observation =
            g_routeAlternateDecisionObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
        if (observation <= 64U) {
            std::array<char, core::log::kLineCapacity> line{};
            const int length = std::snprintf(
                line.data(),
                line.size(),
                "ev=bootflow stage=activity_script_route_alternate_decision n=%u step=predicate caller_rva=0x%llX source=%p mode=%d result=%u package=%.*s mutation=observe_only",
                observation,
                static_cast<unsigned long long>(callerRva),
                static_cast<void*>(source),
                safe_read<std::int32_t>(source != nullptr ? source + 0x40 : nullptr, -1),
                result ? 1U : 0U,
                static_cast<int>(package.size()),
                package.data());
            if (length > 0) {
                core::log::write(core::log::Channel::client,
                                 core::log::Level::info,
                                 {line.data(), static_cast<std::size_t>(length)});
            }
        }
    }
    return result;
}

/** Observes the mode value that selects the alternate updater's final lane. */
__declspec(noinline) std::int32_t __fastcall route_alternate_mode(std::byte* source) noexcept {
    const RouteAlternateMode original =
        g_routeAlternateModeOriginal.load(std::memory_order_acquire);
    const std::int32_t result = original != nullptr ? original(source) : -1;
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;
    std::string_view package{};
    const bool opening = embedded_route_trace_is_forced(package);
    if (opening && callerRva == 0xC1AD72U) {
        const std::uint32_t observation =
            g_routeAlternateDecisionObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
        if (observation <= 64U) {
            std::array<char, core::log::kLineCapacity> line{};
            const int length = std::snprintf(
                line.data(),
                line.size(),
                "ev=bootflow stage=activity_script_route_alternate_decision n=%u step=mode caller_rva=0x%llX source=%p result=%d package=%.*s mutation=observe_only",
                observation,
                static_cast<unsigned long long>(callerRva),
                static_cast<void*>(source),
                result,
                static_cast<int>(package.size()),
                package.data());
            if (length > 0) {
                core::log::write(core::log::Channel::client,
                                 core::log::Level::info,
                                 {line.data(), static_cast<std::size_t>(length)});
            }
        }
    }
    return result;
}

/** Observes the lane-zero selection-ingest boundary without changing its arguments or result. */
__declspec(noinline) bool __fastcall route_selection_ingest(
    std::uintptr_t arg1,
    bool alternate,
    std::uint32_t arg3,
    std::uint32_t arg4,
    const std::byte* selection,
    std::uintptr_t handle) noexcept {
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;
    const RouteDescriptorLookup lookup =
        g_routeDescriptorLookupOriginal.load(std::memory_order_acquire);
    std::byte* beforeManager = nullptr;
    std::byte* const beforeDescriptor =
        lookup != nullptr ? lookup(0, &beforeManager) : nullptr;
    const std::uint8_t beforeRoute = safe_read<std::uint8_t>(
        beforeDescriptor != nullptr ? beforeDescriptor + 0x12 : nullptr,
        0U);
    const std::uint64_t beforePointer18 = safe_read<std::uint64_t>(
        beforeDescriptor != nullptr ? beforeDescriptor + 0x18 : nullptr,
        0U);

    const RouteSelectionIngest original =
        g_routeSelectionIngestOriginal.load(std::memory_order_acquire);
    const bool result = original != nullptr
                            && original(arg1, alternate, arg3, arg4, selection, handle);

    std::byte* afterManager = nullptr;
    std::byte* const afterDescriptor =
        lookup != nullptr ? lookup(0, &afterManager) : nullptr;
    const std::uint8_t afterRoute = safe_read<std::uint8_t>(
        afterDescriptor != nullptr ? afterDescriptor + 0x12 : nullptr,
        0U);
    const std::uint64_t afterPointer18 = safe_read<std::uint64_t>(
        afterDescriptor != nullptr ? afterDescriptor + 0x18 : nullptr,
        0U);

    const std::uint32_t observation =
        g_routeSelectionIngestObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
    std::string_view package{};
    const bool opening = embedded_route_trace_is_forced(package);
    if (observation <= 128U || opening) {
        std::array<char, core::log::kLineCapacity> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_script_route_selection_ingest n=%u caller_rva=0x%llX result=%s arg1=0x%llX alternate=%u arg3=%u arg4=%u selection=%p handle=0x%llX s0=0x%08X s4=0x%08X s8=0x%08X sc=0x%08X before_descriptor=%p before_source=%s before_route=%u before_pointer18=0x%llX after_descriptor=%p after_source=%s after_route=%u after_pointer18=0x%llX opening=%u package=%.*s",
            observation,
            static_cast<unsigned long long>(callerRva),
            result ? "accepted" : "rejected",
            static_cast<unsigned long long>(arg1),
            alternate ? 1U : 0U,
            arg3,
            arg4,
            static_cast<const void*>(selection),
            static_cast<unsigned long long>(handle),
            safe_read<std::uint32_t>(selection, 0U),
            safe_read<std::uint32_t>(selection != nullptr ? selection + 0x04 : nullptr, 0U),
            safe_read<std::uint32_t>(selection != nullptr ? selection + 0x08 : nullptr, 0U),
            safe_read<std::uint32_t>(selection != nullptr ? selection + 0x0C : nullptr, 0U),
            static_cast<void*>(beforeDescriptor),
            beforeManager != nullptr ? "manager" : "lane",
            static_cast<unsigned int>(beforeRoute),
            static_cast<unsigned long long>(beforePointer18),
            static_cast<void*>(afterDescriptor),
            afterManager != nullptr ? "manager" : "lane",
            static_cast<unsigned int>(afterRoute),
            static_cast<unsigned long long>(afterPointer18),
            opening ? 1U : 0U,
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

/** Observes the genuine entry that accepts a launch payload and creates command state 1. */
__declspec(noinline) bool __fastcall launch_command_initialize(
    std::byte* command,
    const std::byte* selection,
    std::int32_t launchMode,
    std::int32_t deferred,
    const void* handle) noexcept {
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;
    const std::int32_t stateBefore = safe_read<std::int32_t>(command, -1);
    const std::array<std::uint64_t, 6> payload{
        safe_read<std::uint64_t>(selection, 0U),
        safe_read<std::uint64_t>(selection != nullptr ? selection + 0x08 : nullptr, 0U),
        safe_read<std::uint64_t>(selection != nullptr ? selection + 0x10 : nullptr, 0U),
        safe_read<std::uint64_t>(selection != nullptr ? selection + 0x18 : nullptr, 0U),
        safe_read<std::uint64_t>(selection != nullptr ? selection + 0x20 : nullptr, 0U),
        safe_read<std::uint64_t>(selection != nullptr ? selection + 0x28 : nullptr, 0U)};

    const LaunchCommandInitialize original =
        g_launchCommandInitializeOriginal.load(std::memory_order_acquire);
    const bool result = original != nullptr
                            && original(command, selection, launchMode, deferred, handle);
    const std::int32_t stateAfter = safe_read<std::int32_t>(command, -1);

    const std::uint32_t observation =
        g_launchCommandInitializeObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
    if (observation <= 64U) {
        std::string_view package{};
        const bool opening = opening_is_forced(package) && package == "mission_towerfall";
        std::array<char, core::log::kLineCapacity> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_script_launch_initialize n=%u caller_rva=0x%llX result=%s command=%p state_before=%d state_after=%d launch_mode=%d deferred=%d selection=%p payload00=0x%016llX payload08=0x%016llX payload10=0x%016llX payload18=0x%016llX payload20=0x%016llX payload28=0x%016llX handle=%p opening=%u package=%.*s",
            observation,
            static_cast<unsigned long long>(callerRva),
            result ? "accepted" : "rejected",
            static_cast<void*>(command),
            stateBefore,
            stateAfter,
            launchMode,
            deferred,
            static_cast<const void*>(selection),
            static_cast<unsigned long long>(payload[0]),
            static_cast<unsigned long long>(payload[1]),
            static_cast<unsigned long long>(payload[2]),
            static_cast<unsigned long long>(payload[3]),
            static_cast<unsigned long long>(payload[4]),
            static_cast<unsigned long long>(payload[5]),
            handle,
            opening ? 1U : 0U,
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

/** Observes the native launch-command state switch whose state 4 enters authored dispatch. */
__declspec(noinline) void __fastcall launch_command_dispatch(std::byte* command) noexcept {
    const std::uint32_t observation =
        g_launchCommandDispatchObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
    if (observation <= 256U) {
        auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
        auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
        const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                             ? static_cast<std::uintptr_t>(returnAddress - image)
                                             : 0U;
        const std::int32_t state = safe_read<std::int32_t>(command, -1);
        const std::int32_t launchMode = safe_read<std::int32_t>(
            command != nullptr ? command + 0xD0 : nullptr,
            -1);
        const std::int16_t source = safe_read<std::int16_t>(
            command != nullptr ? command + 0x42 : nullptr,
            static_cast<std::int16_t>(-1));
        const std::int16_t destination = safe_read<std::int16_t>(
            command != nullptr ? command + 0x44 : nullptr,
            static_cast<std::int16_t>(-1));
        const void* const handle = safe_read<const void*>(
            command != nullptr ? command + 0x2B8 : nullptr,
            nullptr);
        std::array<char, 41> commandPackage{};
        std::size_t commandPackageLength = 0;
        for (; command != nullptr && commandPackageLength + 1U < commandPackage.size();
             ++commandPackageLength) {
            const char value = safe_read<char>(command + 0x90 + commandPackageLength, '\0');
            commandPackage[commandPackageLength] = value;
            if (value == '\0') {
                break;
            }
        }
        std::string_view package{};
        const bool opening = opening_is_forced(package) && package == "mission_towerfall";
        std::array<char, core::log::kLineCapacity> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_script_launch_command n=%u caller_rva=0x%llX command=%p state=%d launch_mode=%d selection_source=%d selection_destination=%d selection_package=%.*s handle=%p opening=%u package=%.*s",
            observation,
            static_cast<unsigned long long>(callerRva),
            static_cast<void*>(command),
            state,
            launchMode,
            static_cast<int>(source),
            static_cast<int>(destination),
            static_cast<int>(commandPackageLength),
            commandPackage.data(),
            handle,
            opening ? 1U : 0U,
            static_cast<int>(package.size()),
            package.data());
        if (length > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(length)});
        }
    }

    const LaunchCommandDispatch original =
        g_launchCommandDispatchOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(command);
    }
}

void log_launch_producer_window(const char* phase,
                                const char* result,
                                const char* reason,
                                std::int32_t worldActivity,
                                std::int32_t sessionActivity,
                                std::uint32_t ticks,
                                std::uint8_t guardBefore,
                                std::uint8_t guardAfter) noexcept {
    std::string_view package{};
    const bool opening = opening_is_forced(package) && package == "mission_towerfall";
    std::array<char, 384> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_launch_producer_window phase=%s result=%s reason=%s world=%d session=%d ticks=%u guard_before=%u guard_after=%u opening=%u package=%.*s",
        phase,
        result,
        reason,
        worldActivity,
        sessionActivity,
        ticks,
        static_cast<unsigned int>(guardBefore),
        static_cast<unsigned int>(guardAfter),
        opening ? 1U : 0U,
        static_cast<int>(package.size()),
        package.data());
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         std::string_view(result) == "ok" ? core::log::Level::info
                                                           : core::log::Level::warn,
                         {line.data(), static_cast<std::size_t>(length)});
    }
}

void close_launch_producer_window(const char* reason,
                                  std::int32_t worldActivity = -1,
                                  std::int32_t sessionActivity = -1) noexcept {
    std::uint32_t expected = 1U;
    if (!g_launchProducerWindowState.compare_exchange_strong(
            expected, 2U, std::memory_order_acq_rel, std::memory_order_acquire)) {
        return;
    }
    const std::uint32_t ticks =
        g_launchProducerWindowTicks.load(std::memory_order_acquire);
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    const std::uint8_t guardBefore = safe_read<std::uint8_t>(
        image != nullptr ? image + kLaunchProducerGuardRva : nullptr,
        0xFFU);
    const std::uint8_t guardAfter = guardBefore;
    log_launch_producer_window("close",
                               "ok",
                               reason,
                               worldActivity,
                               sessionActivity,
                               ticks,
                               guardBefore,
                               guardAfter);
}

/** Preserves the native producer guard setter while observing its callers. */
__declspec(noinline) void __fastcall launch_producer_guard_set(bool enabled) noexcept {
    const LaunchProducerGuardSetter original =
        g_launchProducerGuardSetterOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(enabled);
    }
}

void log_activity_event46(const char* stage,
                          const char* phase,
                          std::uint32_t observation,
                          void* context,
                          void* payload,
                          std::uint8_t readyBefore,
                          std::uint8_t readyAfter) noexcept {
    std::string_view package{};
    const bool opening = opening_is_forced(package) && package == "mission_towerfall";
    std::array<char, 384> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=%s n=%u phase=%s event=46 context=%p payload=%p ready_before=%u ready_after=%u opening=%u package=%.*s",
        stage,
        observation,
        phase,
        context,
        payload,
        static_cast<unsigned int>(readyBefore),
        static_cast<unsigned int>(readyAfter),
        opening ? 1U : 0U,
        static_cast<int>(package.size()),
        package.data());
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
}

/** Observes the sole authentic event-46 callback that can set launch readiness. */
__declspec(noinline) void __fastcall launch_producer_ready_callback(void* context,
                                                                     std::uint32_t event,
                                                                     void* payload) noexcept {
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    const std::uint8_t before = safe_read<std::uint8_t>(
        image != nullptr ? image + kLaunchProducerReadyByteRva : nullptr,
        0xFFU);
    const std::uint32_t observation =
        g_launchProducerReadyCallbackObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
    const LaunchProducerReadyCallback original =
        g_launchProducerReadyCallbackOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(context, event, payload);
    }
    const std::uint8_t after = safe_read<std::uint8_t>(
        image != nullptr ? image + kLaunchProducerReadyByteRva : nullptr,
        0xFFU);
    if (observation <= 64U) {
        log_activity_event46("activity_script_launch_ready_callback",
                             "return",
                             observation,
                             context,
                             payload,
                             before,
                             after);
    }
}

/** Observes event 46 when it is delivered without an explicit payload. */
__declspec(noinline) void __fastcall activity_event_dispatch_local(std::uint32_t event) noexcept {
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    const std::uint8_t before = safe_read<std::uint8_t>(
        image != nullptr ? image + kLaunchProducerReadyByteRva : nullptr,
        0xFFU);
    const std::uint32_t observation = event == 46U
                                          ? g_activityEvent46Observed.fetch_add(
                                                1U, std::memory_order_relaxed)
                                                + 1U
                                          : 0U;
    const ActivityEventDispatchLocal original =
        g_activityEventDispatchLocalOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(event);
    }
    if (event == 46U && observation <= 64U) {
        const std::uint8_t after = safe_read<std::uint8_t>(
            image != nullptr ? image + kLaunchProducerReadyByteRva : nullptr,
            0xFFU);
        log_activity_event46("activity_global_event46_dispatch",
                             "local_return",
                             observation,
                             nullptr,
                             nullptr,
                             before,
                             after);
    }
}

/** Observes event 46 and its native payload before the registered callback consumes it. */
__declspec(noinline) void __fastcall activity_event_dispatch_payload(std::uint32_t event,
                                                                      void* payload) noexcept {
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    const std::uint8_t before = safe_read<std::uint8_t>(
        image != nullptr ? image + kLaunchProducerReadyByteRva : nullptr,
        0xFFU);
    const std::uint32_t observation = event == 46U
                                          ? g_activityEvent46Observed.fetch_add(
                                                1U, std::memory_order_relaxed)
                                                + 1U
                                          : 0U;
    const ActivityEventDispatchPayload original =
        g_activityEventDispatchPayloadOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(event, payload);
    }
    if (event == 46U && observation <= 64U) {
        const std::uint8_t after = safe_read<std::uint8_t>(
            image != nullptr ? image + kLaunchProducerReadyByteRva : nullptr,
            0xFFU);
        log_activity_event46("activity_global_event46_dispatch",
                             "payload_return",
                             observation,
                             nullptr,
                             payload,
                             before,
                             after);
    }
}

/** Observes the registered native producer that republishes a non-null callback payload as event 46. */
__declspec(noinline) void __fastcall activity_event46_registered_producer(
    void* context,
    const void* payloadRef) noexcept {
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    const std::uintptr_t payloadValue = safe_read<std::uintptr_t>(payloadRef, 0U);
    const std::uint8_t readyBefore = safe_read<std::uint8_t>(
        image != nullptr ? image + kLaunchProducerReadyByteRva : nullptr,
        0xFFU);
    const std::uint32_t observation =
        g_activityEvent46RegisteredProducerObserved.fetch_add(1U, std::memory_order_relaxed)
        + 1U;
    const ActivityEvent46RegisteredProducer original =
        g_activityEvent46RegisteredProducerOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(context, payloadRef);
    }
    if (observation <= 64U) {
        std::string_view package{};
        const bool opening = opening_is_forced(package) && package == "mission_towerfall";
        const std::uint8_t readyAfter = safe_read<std::uint8_t>(
            image != nullptr ? image + kLaunchProducerReadyByteRva : nullptr,
            0xFFU);
        std::array<char, 448> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_event46_registered_producer n=%u context=%p payload_ref=%p payload_value=%p ready_before=%u ready_after=%u opening=%u package=%.*s",
            observation,
            context,
            payloadRef,
            reinterpret_cast<void*>(payloadValue),
            static_cast<unsigned int>(readyBefore),
            static_cast<unsigned int>(readyAfter),
            opening ? 1U : 0U,
            static_cast<int>(package.size()),
            package.data());
        if (length > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(length)});
        }
    }
}

/** Observes lifecycle resets of the subsystem that supplies launch-readiness event 46. */
__declspec(noinline) void __fastcall activity_event46_producer_reset(void* context) noexcept {
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    const std::uintptr_t callerRva = image_rva(_ReturnAddress());
    const std::uintptr_t pendingBefore = safe_read<std::uintptr_t>(
        image != nullptr ? image + kActivityEvent46PendingPayloadRva : nullptr,
        0U);
    const std::uint8_t activeBefore = safe_read<std::uint8_t>(
        image != nullptr ? image + kActivityEvent46ProducerRequestActiveRva : nullptr,
        0xFFU);
    const std::uint32_t observation =
        g_activityEvent46ProducerResetObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;

    const ActivityEvent46ProducerReset original =
        g_activityEvent46ProducerResetOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(context);
    }

    if (observation <= 32U) {
        const std::uintptr_t pendingAfter = safe_read<std::uintptr_t>(
            image != nullptr ? image + kActivityEvent46PendingPayloadRva : nullptr,
            0U);
        const std::uint8_t activeAfter = safe_read<std::uint8_t>(
            image != nullptr ? image + kActivityEvent46ProducerRequestActiveRva : nullptr,
            0xFFU);
        std::string_view package{};
        const bool opening = opening_is_forced(package) && package == "mission_towerfall";
        std::array<char, 448> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_event46_producer_reset n=%u caller_rva=0x%llX context=%p pending_before=%p pending_after=%p active_before=%u active_after=%u opening=%u package=%.*s",
            observation,
            static_cast<unsigned long long>(callerRva),
            context,
            reinterpret_cast<void*>(pendingBefore),
            reinterpret_cast<void*>(pendingAfter),
            static_cast<unsigned int>(activeBefore),
            static_cast<unsigned int>(activeAfter),
            opening ? 1U : 0U,
            static_cast<int>(package.size()),
            package.empty() ? "" : package.data());
        if (length > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(length)});
        }
    }
}

/** Observes construction/registration of the subsystem that supplies launch-readiness event 46. */
__declspec(noinline) void __fastcall activity_event46_producer_initialize(void* context) noexcept {
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    const std::uintptr_t callerRva = image_rva(_ReturnAddress());
    const std::uintptr_t pendingBefore = safe_read<std::uintptr_t>(
        image != nullptr ? image + kActivityEvent46PendingPayloadRva : nullptr,
        0U);
    const std::uint8_t activeBefore = safe_read<std::uint8_t>(
        image != nullptr ? image + kActivityEvent46ProducerRequestActiveRva : nullptr,
        0xFFU);
    const std::uint32_t observation =
        g_activityEvent46ProducerInitializeObserved.fetch_add(1U, std::memory_order_relaxed)
        + 1U;

    const ActivityEvent46ProducerInitialize original =
        g_activityEvent46ProducerInitializeOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(context);
    }

    if (observation <= 16U) {
        const std::uintptr_t pendingAfter = safe_read<std::uintptr_t>(
            image != nullptr ? image + kActivityEvent46PendingPayloadRva : nullptr,
            0U);
        const std::uint8_t activeAfter = safe_read<std::uint8_t>(
            image != nullptr ? image + kActivityEvent46ProducerRequestActiveRva : nullptr,
            0xFFU);
        std::string_view package{};
        const bool opening = opening_is_forced(package) && package == "mission_towerfall";
        std::array<char, 448> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_event46_producer_initialize n=%u caller_rva=0x%llX context=%p pending_before=%p pending_after=%p active_before=%u active_after=%u opening=%u package=%.*s",
            observation,
            static_cast<unsigned long long>(callerRva),
            context,
            reinterpret_cast<void*>(pendingBefore),
            reinterpret_cast<void*>(pendingAfter),
            static_cast<unsigned int>(activeBefore),
            static_cast<unsigned int>(activeAfter),
            opening ? 1U : 0U,
            static_cast<int>(package.size()),
            package.empty() ? "" : package.data());
        if (length > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(length)});
        }
    }
}

/** Observes the producer's native pre-request availability predicate. */
__declspec(noinline) bool __fastcall activity_event46_producer_available(void* context) noexcept {
    const std::uintptr_t callerRva = image_rva(_ReturnAddress());
    const std::uint32_t observation =
        g_activityEvent46ProducerAvailableObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
    const ActivityEvent46ProducerAvailable original =
        g_activityEvent46ProducerAvailableOriginal.load(std::memory_order_acquire);
    const bool result = original != nullptr && original(context);

    if (observation <= 64U) {
        std::string_view package{};
        const bool opening = opening_is_forced(package) && package == "mission_towerfall";
        std::array<char, 384> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_event46_producer_available n=%u caller_rva=0x%llX context=%p result=%u opening=%u package=%.*s",
            observation,
            static_cast<unsigned long long>(callerRva),
            context,
            result ? 1U : 0U,
            opening ? 1U : 0U,
            static_cast<int>(package.size()),
            package.empty() ? "" : package.data());
        if (length > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(length)});
        }
    }
    return result;
}

/** Observes upstream service requests whose accepted responses feed the event-46 producer. */
__declspec(noinline) void __fastcall activity_event46_producer_request(
    void* context,
    void* request,
    void* output) noexcept {
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    const std::uintptr_t callerRva = image_rva(_ReturnAddress());
    const std::uintptr_t pendingBefore = safe_read<std::uintptr_t>(
        image != nullptr ? image + kActivityEvent46PendingPayloadRva : nullptr,
        0U);
    const std::uint8_t activeBefore = safe_read<std::uint8_t>(
        image != nullptr ? image + kActivityEvent46ProducerRequestActiveRva : nullptr,
        0xFFU);
    const std::uint32_t observation =
        g_activityEvent46ProducerRequestObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;

    const ActivityEvent46ProducerRequest original =
        g_activityEvent46ProducerRequestOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(context, request, output);
    }

    if (observation <= 64U) {
        const std::uintptr_t pendingAfter = safe_read<std::uintptr_t>(
            image != nullptr ? image + kActivityEvent46PendingPayloadRva : nullptr,
            0U);
        const std::uint8_t activeAfter = safe_read<std::uint8_t>(
            image != nullptr ? image + kActivityEvent46ProducerRequestActiveRva : nullptr,
            0xFFU);
        std::string_view package{};
        const bool opening = opening_is_forced(package) && package == "mission_towerfall";
        std::array<char, 512> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_event46_producer_request n=%u caller_rva=0x%llX context=%p request=%p output=%p pending_before=%p pending_after=%p active_before=%u active_after=%u opening=%u package=%.*s",
            observation,
            static_cast<unsigned long long>(callerRva),
            context,
            request,
            output,
            reinterpret_cast<void*>(pendingBefore),
            reinterpret_cast<void*>(pendingAfter),
            static_cast<unsigned int>(activeBefore),
            static_cast<unsigned int>(activeAfter),
            opening ? 1U : 0U,
            static_cast<int>(package.size()),
            package.empty() ? "" : package.data());
        if (length > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(length)});
        }

        // The native request delegates to a lazily resolved provider vfunc at +0x188. Resolve
        // the same provider after the native call, when it is guaranteed to be initialized, and
        // record its concrete target and task inputs. The accessor was already invoked by the
        // native request and none of the provider state is modified here.
        if (opening && image != nullptr) {
            const std::uintptr_t accessorAddress = safe_read<std::uintptr_t>(
                image + kActivityEvent46ProviderAccessorSlotRva,
                0U);
            void* holder = nullptr;
            if (accessorAddress != 0U) {
                using ProviderAccessor = void*(__fastcall*)(void* storage);
                holder = reinterpret_cast<ProviderAccessor>(accessorAddress)(
                    image + kActivityEvent46ProviderStorageRva);
            }
            const std::uintptr_t providerAddress = safe_read<std::uintptr_t>(holder, 0U);
            const std::uintptr_t vtableAddress = safe_read<std::uintptr_t>(
                reinterpret_cast<const void*>(providerAddress),
                0U);
            const std::uintptr_t submitAddress = safe_read<std::uintptr_t>(
                reinterpret_cast<const void*>(vtableAddress + 0x188U),
                0U);

            const auto* const requestBytes = static_cast<const std::byte*>(request);
            const auto* const providerBytes = reinterpret_cast<const std::byte*>(providerAddress);
            std::array<char, 1408> providerLine{};
            const int providerLength = std::snprintf(
                providerLine.data(),
                providerLine.size(),
                "ev=bootflow stage=activity_event46_provider_task n=%u accessor=%p storage=%p holder=%p provider=%p vtable=%p submit=%p submit_rva=0x%llX request=%p r00=0x%016llX r08=0x%016llX r10=0x%016llX r18=0x%016llX r20=0x%016llX r28=0x%016llX r30=0x%016llX r38=0x%016llX p08=0x%016llX p10=0x%016llX p18=0x%016llX p20=0x%016llX p28=0x%016llX p30=0x%016llX p38=0x%016llX active=%u opening=1 package=%.*s",
                observation,
                reinterpret_cast<void*>(accessorAddress),
                image + kActivityEvent46ProviderStorageRva,
                holder,
                reinterpret_cast<void*>(providerAddress),
                reinterpret_cast<void*>(vtableAddress),
                reinterpret_cast<void*>(submitAddress),
                static_cast<unsigned long long>(image_rva(
                    reinterpret_cast<const void*>(submitAddress))),
                request,
                static_cast<unsigned long long>(safe_read<std::uint64_t>(requestBytes, 0U)),
                static_cast<unsigned long long>(safe_read<std::uint64_t>(requestBytes + 0x08U, 0U)),
                static_cast<unsigned long long>(safe_read<std::uint64_t>(requestBytes + 0x10U, 0U)),
                static_cast<unsigned long long>(safe_read<std::uint64_t>(requestBytes + 0x18U, 0U)),
                static_cast<unsigned long long>(safe_read<std::uint64_t>(requestBytes + 0x20U, 0U)),
                static_cast<unsigned long long>(safe_read<std::uint64_t>(requestBytes + 0x28U, 0U)),
                static_cast<unsigned long long>(safe_read<std::uint64_t>(requestBytes + 0x30U, 0U)),
                static_cast<unsigned long long>(safe_read<std::uint64_t>(requestBytes + 0x38U, 0U)),
                static_cast<unsigned long long>(safe_read<std::uint64_t>(
                    providerBytes != nullptr ? providerBytes + 0x08U : nullptr,
                    0U)),
                static_cast<unsigned long long>(safe_read<std::uint64_t>(
                    providerBytes != nullptr ? providerBytes + 0x10U : nullptr,
                    0U)),
                static_cast<unsigned long long>(safe_read<std::uint64_t>(
                    providerBytes != nullptr ? providerBytes + 0x18U : nullptr,
                    0U)),
                static_cast<unsigned long long>(safe_read<std::uint64_t>(
                    providerBytes != nullptr ? providerBytes + 0x20U : nullptr,
                    0U)),
                static_cast<unsigned long long>(safe_read<std::uint64_t>(
                    providerBytes != nullptr ? providerBytes + 0x28U : nullptr,
                    0U)),
                static_cast<unsigned long long>(safe_read<std::uint64_t>(
                    providerBytes != nullptr ? providerBytes + 0x30U : nullptr,
                    0U)),
                static_cast<unsigned long long>(safe_read<std::uint64_t>(
                    providerBytes != nullptr ? providerBytes + 0x38U : nullptr,
                    0U)),
                static_cast<unsigned int>(activeAfter),
                static_cast<int>(package.size()),
                package.data());
            if (providerLength > 0) {
                core::log::write(core::log::Channel::client,
                                 core::log::Level::info,
                                 {providerLine.data(),
                                  static_cast<std::size_t>(providerLength)});
            }
        }
    }
}

/** Observes the authentic higher-level request flow that should start the event-46 service request. */
__declspec(noinline) bool __fastcall activity_event46_request_flow(
    void* context,
    const void* descriptor) noexcept {
    const std::uintptr_t callerRva = image_rva(_ReturnAddress());
    std::string_view package{};
    const bool opening = opening_is_forced(package) && package == "mission_towerfall";
    const std::uint32_t observation =
        opening
            ? g_activityEvent46RequestFlowObserved.fetch_add(1U, std::memory_order_relaxed) + 1U
            : 0U;
    const auto* const bytes = static_cast<const std::byte*>(descriptor);
    const std::uint64_t descriptor00 = safe_read<std::uint64_t>(bytes, 0U);
    const std::uint64_t descriptor08 = safe_read<std::uint64_t>(
        bytes != nullptr ? bytes + 0x08U : nullptr,
        0U);
    const std::uint64_t descriptor10 = safe_read<std::uint64_t>(
        bytes != nullptr ? bytes + 0x10U : nullptr,
        0U);
    const std::uint64_t descriptor18 = safe_read<std::uint64_t>(
        bytes != nullptr ? bytes + 0x18U : nullptr,
        0U);
    const std::uint64_t descriptor20 = safe_read<std::uint64_t>(
        bytes != nullptr ? bytes + 0x20U : nullptr,
        0U);
    const std::uint64_t descriptor28 = safe_read<std::uint64_t>(
        bytes != nullptr ? bytes + 0x28U : nullptr,
        0U);

    const ActivityEvent46RequestFlow original =
        g_activityEvent46RequestFlowOriginal.load(std::memory_order_acquire);
    const bool result = original != nullptr && original(context, descriptor);

    if (opening && observation <= 32U) {
        std::array<char, 768> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_event46_request_flow n=%u caller_rva=0x%llX context=%p descriptor=%p d00=0x%llX d08=0x%llX d10=0x%llX d18=0x%llX d20=0x%llX d28=0x%llX result=%u opening=1 package=%.*s",
            observation,
            static_cast<unsigned long long>(callerRva),
            context,
            descriptor,
            static_cast<unsigned long long>(descriptor00),
            static_cast<unsigned long long>(descriptor08),
            static_cast<unsigned long long>(descriptor10),
            static_cast<unsigned long long>(descriptor18),
            static_cast<unsigned long long>(descriptor20),
            static_cast<unsigned long long>(descriptor28),
            result ? 1U : 0U,
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

/** Observes which native director-action branch CHOSEN selects before request preflight. */
__declspec(noinline) bool __fastcall activity_event46_action_dispatch(
    void* context,
    void* auxiliary) noexcept {
    const std::uintptr_t callerRva = image_rva(_ReturnAddress());
    std::string_view package{};
    const bool opening = opening_is_forced(package) && package == "mission_towerfall";
    const std::uint32_t observation =
        opening
            ? g_activityEvent46ActionDispatchObserved.fetch_add(1U,
                                                                 std::memory_order_relaxed)
                  + 1U
            : 0U;
    const auto* const bytes = static_cast<const std::byte*>(context);
    const std::uint8_t pending = safe_read<std::uint8_t>(
        bytes != nullptr ? bytes + 0x128U : nullptr,
        0xFFU);
    const std::int8_t selector = safe_read<std::int8_t>(
        bytes != nullptr ? bytes + 0x138U : nullptr,
        static_cast<std::int8_t>(-1));
    const std::uint32_t flags190 = safe_read<std::uint32_t>(
        bytes != nullptr ? bytes + 0x190U : nullptr,
        0xFFFFFFFFU);
    const std::uint32_t actionHash = safe_read<std::uint32_t>(
        bytes != nullptr ? bytes + 0x1B8U : nullptr,
        0xFFFFFFFFU);

    const ActivityEvent46ActionDispatch original =
        g_activityEvent46ActionDispatchOriginal.load(std::memory_order_acquire);
    const bool result = original != nullptr && original(context, auxiliary);

    if (opening && observation <= 64U) {
        std::array<char, 512> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_event46_action_dispatch n=%u caller_rva=0x%llX context=%p auxiliary=%p pending=%u selector=%d flags190=0x%08X action_hash=0x%08X result=%u opening=1 package=%.*s",
            observation,
            static_cast<unsigned long long>(callerRva),
            context,
            auxiliary,
            static_cast<unsigned int>(pending),
            static_cast<int>(selector),
            flags190,
            actionHash,
            result ? 1U : 0U,
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

/** Observes the native preflight predicate guarding entry into the event-46 request flow. */
__declspec(noinline) bool __fastcall activity_event46_request_predicate(
    void* context,
    void* statusOut) noexcept {
    const std::uintptr_t callerRva = image_rva(_ReturnAddress());
    std::string_view package{};
    const bool opening = opening_is_forced(package) && package == "mission_towerfall";
    const std::uint32_t observation =
        opening
            ? g_activityEvent46RequestPredicateObserved.fetch_add(1U,
                                                                   std::memory_order_relaxed)
                  + 1U
            : 0U;
    const auto* const bytes = static_cast<const std::byte*>(context);
    const std::uint32_t flags50 = safe_read<std::uint32_t>(
        bytes != nullptr ? bytes + 0x50U : nullptr,
        0xFFFFFFFFU);
    const std::uint32_t flags54 = safe_read<std::uint32_t>(
        bytes != nullptr ? bytes + 0x54U : nullptr,
        0xFFFFFFFFU);
    const std::uint64_t descriptor18 = safe_read<std::uint64_t>(
        bytes != nullptr ? bytes + 0x18U : nullptr,
        0U);
    const std::uint64_t descriptor20 = safe_read<std::uint64_t>(
        bytes != nullptr ? bytes + 0x20U : nullptr,
        0U);
    const std::uint64_t descriptor28 = safe_read<std::uint64_t>(
        bytes != nullptr ? bytes + 0x28U : nullptr,
        0U);
    const std::uint64_t descriptor30 = safe_read<std::uint64_t>(
        bytes != nullptr ? bytes + 0x30U : nullptr,
        0U);
    const std::uint64_t descriptor38 = safe_read<std::uint64_t>(
        bytes != nullptr ? bytes + 0x38U : nullptr,
        0U);
    const std::uint64_t descriptor40 = safe_read<std::uint64_t>(
        bytes != nullptr ? bytes + 0x40U : nullptr,
        0U);
    const std::int32_t statusBefore = safe_read<std::int32_t>(statusOut, INT32_MIN);

    const ActivityEvent46RequestPredicate original =
        g_activityEvent46RequestPredicateOriginal.load(std::memory_order_acquire);
    const bool result = original != nullptr && original(context, statusOut);

    if (opening && observation <= 64U) {
        const std::int32_t statusAfter = safe_read<std::int32_t>(statusOut, INT32_MIN);
        std::array<char, 832> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_event46_request_predicate n=%u caller_rva=0x%llX context=%p status_out=%p flags50=0x%08X flags54=0x%08X d18=0x%llX d20=0x%llX d28=0x%llX d30=0x%llX d38=0x%llX d40=0x%llX status_before=%d status_after=%d result=%u opening=1 package=%.*s",
            observation,
            static_cast<unsigned long long>(callerRva),
            context,
            statusOut,
            flags50,
            flags54,
            static_cast<unsigned long long>(descriptor18),
            static_cast<unsigned long long>(descriptor20),
            static_cast<unsigned long long>(descriptor28),
            static_cast<unsigned long long>(descriptor30),
            static_cast<unsigned long long>(descriptor38),
            static_cast<unsigned long long>(descriptor40),
            statusBefore,
            statusAfter,
            result ? 1U : 0U,
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

/** Observes the subsystem tick that consumes its pending payload and emits event 46. */
__declspec(noinline) void __fastcall activity_event46_producer_tick(void* context) noexcept {
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    const std::uintptr_t callerRva = image_rva(_ReturnAddress());
    const std::uintptr_t pendingBefore = safe_read<std::uintptr_t>(
        image != nullptr ? image + kActivityEvent46PendingPayloadRva : nullptr,
        0U);
    const std::uint8_t activeBefore = safe_read<std::uint8_t>(
        image != nullptr ? image + kActivityEvent46ProducerRequestActiveRva : nullptr,
        0xFFU);
    const bool windowActive =
        g_launchProducerWindowState.load(std::memory_order_acquire) == 1U;
    std::string_view package{};
    const bool opening = opening_is_forced(package) && package == "mission_towerfall";
    const bool observe = windowActive || opening;
    const std::uint32_t observation =
        observe
            ? g_activityEvent46ProducerTickObserved.fetch_add(1U, std::memory_order_relaxed) + 1U
            : 0U;

    const ActivityEvent46ProducerTick original =
        g_activityEvent46ProducerTickOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(context);
    }

    const std::uintptr_t pendingAfter = safe_read<std::uintptr_t>(
        image != nullptr ? image + kActivityEvent46PendingPayloadRva : nullptr,
        0U);
    const std::uint8_t activeAfter = safe_read<std::uint8_t>(
        image != nullptr ? image + kActivityEvent46ProducerRequestActiveRva : nullptr,
        0xFFU);
    if (observe && (observation <= 16U || pendingBefore != 0U || pendingAfter != 0U)) {
        const std::uint8_t ready = safe_read<std::uint8_t>(
            image != nullptr ? image + kLaunchProducerReadyByteRva : nullptr,
            0xFFU);
        std::array<char, 448> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_event46_producer_tick n=%u caller_rva=0x%llX context=%p pending_before=%p pending_after=%p active_before=%u active_after=%u ready=%u window=%u opening=%u package=%.*s",
            observation,
            static_cast<unsigned long long>(callerRva),
            context,
            reinterpret_cast<void*>(pendingBefore),
            reinterpret_cast<void*>(pendingAfter),
            static_cast<unsigned int>(activeBefore),
            static_cast<unsigned int>(activeAfter),
            static_cast<unsigned int>(ready),
            windowActive ? 1U : 0U,
            opening ? 1U : 0U,
            static_cast<int>(package.size()),
            package.data());
        if (length > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(length)});
        }
    }
}

/** Reports the exact native gate that prevents the authored launch producer from dispatching. */
__declspec(noinline) void __fastcall launch_producer() noexcept {
    std::string_view package{};
    const bool opening = opening_is_forced(package) && package == "mission_towerfall";
    const bool windowActive =
        g_launchProducerWindowState.load(std::memory_order_acquire) == 1U;
    if (windowActive && !opening) {
        close_launch_producer_window("package_changed_producer");
    }

    if (windowActive && opening) {
        auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
        const std::uint8_t guard = safe_read<std::uint8_t>(
            image != nullptr ? image + kLaunchProducerGuardRva : nullptr,
            0xFFU);
        std::int32_t worldActivity = -2;
        std::int32_t sessionActivity = -2;
        std::uint8_t ready = 0xFFU;
        std::uint8_t phase = 0xFFU;
        bool snapshotOk = image != nullptr;
        if (snapshotOk) {
            __try {
                const auto worldGetter = reinterpret_cast<LaunchProducerWorldActivity>(
                    image + kLaunchProducerWorldActivityRva);
                const auto sessionGetter = reinterpret_cast<LaunchProducerSessionActivity>(
                    image + kLaunchProducerSessionActivityRva);
                const auto readyGetter = reinterpret_cast<LaunchProducerReady>(
                    image + kLaunchProducerReadyRva);
                const auto phaseGetter = reinterpret_cast<LaunchProducerPhase>(
                    image + kLaunchProducerPhaseRva);
                worldActivity = worldGetter();
                sessionActivity = sessionGetter(0);
                ready = readyGetter(nullptr) ? 1U : 0U;
                if (ready != 0U && worldActivity != -1
                    && worldActivity == sessionActivity) {
                    phase = phaseGetter(10, 3) ? 1U : 0U;
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                snapshotOk = false;
            }
        }

        const char* gate = "dispatch_expected";
        if (!snapshotOk) {
            gate = "snapshot_fault";
        } else if (guard != 0U) {
            gate = "guard";
        } else if (ready == 0U) {
            gate = "ready";
        } else if (worldActivity == -1) {
            gate = "world_none";
        } else if (worldActivity != sessionActivity) {
            gate = "activity_mismatch";
        } else if (phase == 0U) {
            gate = "phase";
        }
        const std::uint64_t signature =
            static_cast<std::uint16_t>(worldActivity)
            | (static_cast<std::uint64_t>(static_cast<std::uint16_t>(sessionActivity)) << 16U)
            | (static_cast<std::uint64_t>(guard) << 32U)
            | (static_cast<std::uint64_t>(ready) << 40U)
            | (static_cast<std::uint64_t>(phase) << 48U)
            | (static_cast<std::uint64_t>(snapshotOk ? 1U : 0U) << 56U);
        if (g_launchProducerEntryLastSignature.exchange(signature, std::memory_order_acq_rel)
            != signature) {
            const std::uint32_t observation =
                g_launchProducerEntryObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
            std::array<char, 384> line{};
            const int length = std::snprintf(
                line.data(),
                line.size(),
                "ev=bootflow stage=activity_script_launch_producer_entry n=%u gate=%s guard=%u ready=%u world=%d session=%d phase=%u ticks=%u opening=1 package=%.*s",
                observation,
                gate,
                static_cast<unsigned int>(guard),
                static_cast<unsigned int>(ready),
                worldActivity,
                sessionActivity,
                static_cast<unsigned int>(phase),
                g_launchProducerWindowTicks.load(std::memory_order_acquire),
                static_cast<int>(package.size()),
                package.data());
            if (length > 0) {
                core::log::write(core::log::Channel::client,
                                 snapshotOk ? core::log::Level::info
                                            : core::log::Level::warn,
                                 {line.data(), static_cast<std::size_t>(length)});
            }
        }
    }

    const LaunchProducer original =
        g_launchProducerOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original();
    }

    if (windowActive
        && g_launchProducerWindowState.load(std::memory_order_acquire) == 1U) {
        const std::uint32_t ticks =
            g_launchProducerWindowTicks.fetch_add(1U, std::memory_order_acq_rel) + 1U;
        if (ticks >= kLaunchProducerWindowBudget) {
            close_launch_producer_window("producer_timeout");
        }
    }
}

void service_homecoming_launch_producer_window(bool opening,
                                                std::string_view package,
                                                std::int32_t identity) noexcept {
    (void)identity;
    if (!opening || package != "mission_towerfall") {
        return;
    }
    std::uint32_t expected = 0U;
    if (!g_launchProducerWindowState.compare_exchange_strong(
            expected, 1U, std::memory_order_acq_rel, std::memory_order_acquire)) {
        return;
    }
    g_launchProducerWindowTicks.store(0U, std::memory_order_release);
    g_launchProducerEntryObserved.store(0U, std::memory_order_release);
    g_launchProducerEntryLastSignature.store(UINT64_MAX,
                                             std::memory_order_release);
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    const std::uint8_t guard = safe_read<std::uint8_t>(
        image != nullptr ? image + kLaunchProducerGuardRva : nullptr,
        0xFFU);
    log_launch_producer_window("open",
                               "ok",
                               "observe_only",
                               -1,
                               -1,
                               0U,
                               guard,
                               guard);
}

void log_authored_launch_dispatch(std::uint32_t observation,
                                  const char* phase,
                                  std::uintptr_t callerRva,
                                  std::int32_t launchMode,
                                  std::int32_t selectionType,
                                  const std::byte* selection,
                                  const void* handle,
                                  std::uint32_t option,
                                  RouteModeSnapshot routeModes,
                                  bool opening,
                                  std::string_view package) noexcept {
    std::array<char, core::log::kLineCapacity> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_authored_launch_dispatch n=%u phase=%s caller_rva=0x%llX launch_mode=%d selection_type=%d selection=%p s0=0x%08X s4=0x%08X s8=0x%08X sc=0x%08X handle=%p option=%u route_mode0=%u route_mode1=%u opening=%u package=%.*s",
        observation,
        phase,
        static_cast<unsigned long long>(callerRva),
        launchMode,
        selectionType,
        static_cast<const void*>(selection),
        safe_read<std::uint32_t>(selection, 0U),
        safe_read<std::uint32_t>(selection != nullptr ? selection + 0x04 : nullptr, 0U),
        safe_read<std::uint32_t>(selection != nullptr ? selection + 0x08 : nullptr, 0U),
        safe_read<std::uint32_t>(selection != nullptr ? selection + 0x0C : nullptr, 0U),
        handle,
        option,
        routeModes.modeZero ? 1U : 0U,
        routeModes.modeOne ? 1U : 0U,
        opening ? 1U : 0U,
        static_cast<int>(package.size()),
        package.data());
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
}

/** Observes the real launch dispatcher before it decides whether authored ingest is eligible. */
__declspec(noinline) void __fastcall authored_launch_dispatch(
    std::int32_t launchMode,
    std::int32_t selectionType,
    const std::byte* selection,
    const void* handle,
    std::uint32_t option) noexcept {
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;
    if (callerRva == kLaunchProducerDispatchReturnRva) {
        close_launch_producer_window("authored_dispatch");
    }
    std::string_view package{};
    const bool opening = embedded_route_trace_is_forced(package);
    const std::uint32_t observation =
        g_authoredLaunchDispatchObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
    if (observation <= 128U || opening) {
        log_authored_launch_dispatch(observation,
                                     "enter",
                                     callerRva,
                                     launchMode,
                                     selectionType,
                                     selection,
                                     handle,
                                     option,
                                     query_route_modes(),
                                     opening,
                                     package);
    }

    const AuthoredLaunchDispatch original =
        g_authoredLaunchDispatchOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(launchMode, selectionType, selection, handle, option);
    }

    if (observation <= 128U || opening) {
        log_authored_launch_dispatch(observation,
                                     "exit",
                                     callerRva,
                                     launchMode,
                                     selectionType,
                                     selection,
                                     handle,
                                     option,
                                     query_route_modes(),
                                     opening,
                                     package);
    }
}

void log_route_mode_set(std::uint32_t observation,
                        std::uint32_t routeMode,
                        const char* phase,
                        std::uintptr_t callerRva,
                        bool requested,
                        RouteModeSnapshot snapshot,
                        bool opening,
                        std::string_view package) noexcept {
    std::array<char, core::log::kLineCapacity> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_route_mode_set n=%u route_mode=%u phase=%s caller_rva=0x%llX requested=%u observed_mode0=%u observed_mode1=%u opening=%u package=%.*s",
        observation,
        routeMode,
        phase,
        static_cast<unsigned long long>(callerRva),
        requested ? 1U : 0U,
        snapshot.modeZero ? 1U : 0U,
        snapshot.modeOne ? 1U : 0U,
        opening ? 1U : 0U,
        static_cast<int>(package.size()),
        package.data());
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
}

void observe_route_mode_set(std::uint32_t routeMode,
                            bool enabled,
                            RouteModeSet original,
                            std::atomic_uint32_t& counter,
                            std::uintptr_t callerRva) noexcept {
    std::string_view package{};
    const bool opening = embedded_route_trace_is_forced(package);
    const std::uint32_t observation =
        counter.fetch_add(1U, std::memory_order_relaxed) + 1U;
    const RouteModeSnapshot before = query_route_modes();
    if (original != nullptr) {
        original(enabled);
    }
    const RouteModeSnapshot after = query_route_modes();
    const bool beforeValue = routeMode == 0U ? before.modeZero : before.modeOne;
    const bool afterValue = routeMode == 0U ? after.modeZero : after.modeOne;
    if (observation == 1U || beforeValue != afterValue || afterValue != enabled) {
        log_route_mode_set(observation,
                           routeMode,
                           "enter",
                           callerRva,
                           enabled,
                           before,
                           opening,
                           package);
        log_route_mode_set(observation,
                           routeMode,
                           "exit",
                           callerRva,
                           enabled,
                           after,
                           opening,
                           package);
    }
}

__declspec(noinline) void __fastcall route_mode_zero_set(bool enabled) noexcept {
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;
    observe_route_mode_set(0U,
                           enabled,
                           g_routeModeZeroSetOriginal.load(std::memory_order_acquire),
                           g_routeModeZeroSetObserved,
                           callerRva);
}

__declspec(noinline) void __fastcall route_mode_one_set(bool enabled) noexcept {
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;
    observe_route_mode_set(1U,
                           enabled,
                           g_routeModeOneSetOriginal.load(std::memory_order_acquire),
                           g_routeModeOneSetObserved,
                           callerRva);
}

/** Inventories native users of the controller whose first byte gates Omega route 38. */
__declspec(noinline) std::byte* __fastcall omega_route_lifecycle_accessor() noexcept {
    const OmegaRouteLifecycleAccessor original =
        g_omegaRouteLifecycleAccessorOriginal.load(std::memory_order_acquire);
    std::byte* const result = original != nullptr ? original() : nullptr;
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;
    std::string_view package{};
    if (!embedded_route_trace_is_forced(package) || callerRva == 0U) {
        return result;
    }
    bool unique = false;
    for (auto& recorded : g_omegaRouteLifecycleAccessorCallers) {
        std::uintptr_t current = recorded.load(std::memory_order_acquire);
        if (current == callerRva) {
            return result;
        }
        if (current == 0U
            && recorded.compare_exchange_strong(current,
                                                callerRva,
                                                std::memory_order_acq_rel,
                                                std::memory_order_acquire)) {
            unique = true;
            break;
        }
    }
    if (!unique) {
        return result;
    }
    const std::uint32_t observation =
        g_omegaRouteLifecycleAccessorObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
    std::array<void*, 8> stack{};
    const USHORT stackCount = RtlCaptureStackBackTrace(
        0,
        static_cast<ULONG>(stack.size()),
        stack.data(),
        nullptr);
    std::array<std::uintptr_t, 4> stackRvas{};
    for (std::size_t index = 0; index < stackRvas.size() && index < stackCount; ++index) {
        auto* const frame = reinterpret_cast<std::byte*>(stack[index]);
        stackRvas[index] = image != nullptr && frame >= image
                               ? static_cast<std::uintptr_t>(frame - image)
                               : 0U;
    }
    if (observation <= 32U && returnAddress >= image + 0x80) {
        dump_runtime_code(L"activity_script_omega_route_lifecycle_accessor_caller",
                          returnAddress - 0x80,
                          0x200U);
    }
    std::array<char, core::log::kLineCapacity> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_omega_route_lifecycle_accessor n=%u caller_rva=0x%llX controller=%p value=%d byte1=%u byte2=%u qword8=0x%llX stack=%llX,%llX,%llX,%llX package=%.*s mutation=observe_only",
        observation,
        static_cast<unsigned long long>(callerRva),
        static_cast<void*>(result),
        static_cast<int>(safe_read<std::int8_t>(result, -1)),
        static_cast<unsigned int>(safe_read<std::uint8_t>(
            result != nullptr ? result + 1 : nullptr,
            0xFFU)),
        static_cast<unsigned int>(safe_read<std::uint8_t>(
            result != nullptr ? result + 2 : nullptr,
            0xFFU)),
        static_cast<unsigned long long>(safe_read<std::uint64_t>(
            result != nullptr ? result + 8 : nullptr,
            0U)),
        static_cast<unsigned long long>(stackRvas[0]),
        static_cast<unsigned long long>(stackRvas[1]),
        static_cast<unsigned long long>(stackRvas[2]),
        static_cast<unsigned long long>(stackRvas[3]),
        static_cast<int>(package.size()),
        package.data());
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
    return result;
}

/** Observes the native lifecycle value that decides whether Omega route 38 arms lane zero. */
__declspec(noinline) std::int32_t __fastcall omega_route_lifecycle_query() noexcept {
    const OmegaRouteLifecycleQuery original =
        g_omegaRouteLifecycleQueryOriginal.load(std::memory_order_acquire);
    const std::int32_t result = original != nullptr ? original() : -1;
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;
    const bool method30Gate = callerRva == kOmegaRouteMethod30LifecycleReturnRva;
    const bool modeApplyGate = callerRva == kOmegaRouteModeApplyLifecycleReturnRva;
    const std::int32_t previous =
        g_omegaRouteLifecycleQueryLast.exchange(result, std::memory_order_acq_rel);
    const bool changed = previous != result;
    std::string_view package{};
    if (!embedded_route_trace_is_forced(package)
        || (!method30Gate && !modeApplyGate && !changed)) {
        return result;
    }
    const std::uint32_t observation =
        g_omegaRouteLifecycleQueryObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
    if (observation > 256U && !changed) {
        return result;
    }
    const char* gate = method30Gate
                           ? "method30_requires_8"
                           : modeApplyGate ? "mode_apply_requires_7_or_10"
                                           : "value_change";
    std::array<char, core::log::kLineCapacity> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_omega_route_lifecycle n=%u caller_rva=0x%llX gate=%s value=%d previous=%d changed=%u method30_pass=%u lane_arm_pass=%u package=%.*s mutation=observe_only",
        observation,
        static_cast<unsigned long long>(callerRva),
        gate,
        result,
        previous,
        changed ? 1U : 0U,
        result == 8 ? 1U : 0U,
        result == 7 || result == 10 ? 1U : 0U,
        static_cast<int>(package.size()),
        package.data());
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
    return result;
}

/** Observes the native route scheduler without changing its state. */
__declspec(noinline) bool __fastcall route_state_query(std::int32_t lane,
                                                        std::int32_t* stateOut) noexcept {
    const RouteStateQuery original = g_routeStateQueryOriginal.load(std::memory_order_acquire);
    const bool result = original != nullptr && original(lane, stateOut);
    const std::int32_t nativeState = safe_read<std::int32_t>(stateOut, -1);
    const auto image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    const std::uintptr_t callerRva = image != 0U && caller >= image ? caller - image : 0U;
    std::string_view package{};
    const bool opening = opening_is_forced(package) && package == "mission_towerfall";
    if (!opening || callerRva != 0xC1C79CU) {
        return result;
    }
    // Route state 1 is not an authored-script readiness flag.  It asks the world controller to
    // publish route 0x12, which tears down the current posse/fireteam and begins a destination
    // transition.  Synthesizing it caused a deterministic `prune` disconnect when the archived
    // destination/slice mapping was absent.  Keep this hook observational only.
    std::int32_t effectiveState = nativeState;
    bool effectiveResult = result;
    bool synthesized = false;
    const std::uint64_t signature = 0xA17E000000000000ULL
                                    ^ static_cast<std::uint32_t>(lane)
                                    ^ (static_cast<std::uint64_t>(
                                           static_cast<std::uint32_t>(nativeState))
                                       << 16U)
                                    ^ (static_cast<std::uint64_t>(
                                           static_cast<std::uint32_t>(effectiveState))
                                       << 32U)
                                    ^ (result ? 0x800000000000ULL : 0U)
                                    ^ (synthesized ? 0x400000000000ULL : 0U);
    const std::uint64_t prior =
        g_routeStateQueryLastSignature.exchange(signature, std::memory_order_acq_rel);
    if (prior == signature) {
        return effectiveResult;
    }
    const std::uint32_t observation =
        g_routeStateQueryObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
    if (observation <= 128U) {
        std::array<char, core::log::kLineCapacity> line{};
        const int length = std::snprintf(
            line.data(),
            line.size(),
            "ev=bootflow stage=activity_script_route_state_query n=%u caller_rva=0x%llX lane=%d result=%u native_state=%d effective_state=%d synthesized=%u publish=%s package=%.*s",
            observation,
            static_cast<unsigned long long>(callerRva),
            lane,
            result ? 1U : 0U,
            nativeState,
            effectiveState,
            synthesized ? 1U : 0U,
            effectiveResult && (effectiveState == 1 || effectiveState == 2) ? "yes" : "no",
            static_cast<int>(package.size()),
            package.data());
        if (length > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::info,
                             {line.data(), static_cast<std::size_t>(length)});
        }
    }
    return effectiveResult;
}

/** Records the native route publication without changing the requested route or descriptor. */
__declspec(noinline) void __fastcall route_state_publish(std::int32_t lane,
                                                          std::int32_t routeCode) noexcept {
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;
    const RouteDescriptorLookup lookup =
        g_routeDescriptorLookupOriginal.load(std::memory_order_acquire);
    std::byte* beforeManager = nullptr;
    std::byte* const beforeDescriptor =
        lookup != nullptr ? lookup(lane, &beforeManager) : nullptr;
    const std::uint8_t beforeRoute = safe_read<std::uint8_t>(
        beforeDescriptor != nullptr ? beforeDescriptor + 0x12 : nullptr,
        0U);
    const RouteStatePublish original =
        g_routeStatePublishOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(lane, routeCode);
    }
    std::byte* afterManager = nullptr;
    std::byte* const afterDescriptor =
        lookup != nullptr ? lookup(lane, &afterManager) : nullptr;
    const std::uint8_t afterRoute = safe_read<std::uint8_t>(
        afterDescriptor != nullptr ? afterDescriptor + 0x12 : nullptr,
        0U);
    std::string_view package{};
    if (!embedded_route_trace_is_forced(package)) {
        return;
    }
    const std::uint32_t observation =
        g_routeStatePublishObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
    if (observation > 128U) {
        return;
    }
    std::array<char, core::log::kLineCapacity> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_route_state_publish n=%u caller_rva=0x%llX lane=%d requested=%d before_descriptor=%p before_source=%s before_route=%u after_descriptor=%p after_source=%s after_route=%u package=%.*s",
        observation,
        static_cast<unsigned long long>(callerRva),
        lane,
        routeCode,
        static_cast<void*>(beforeDescriptor),
        beforeManager != nullptr ? "manager" : "lane",
        static_cast<unsigned int>(beforeRoute),
        static_cast<void*>(afterDescriptor),
        afterManager != nullptr ? "manager" : "lane",
        static_cast<unsigned int>(afterRoute),
        static_cast<int>(package.size()),
        package.data());
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
}

/** Records the local initializer that resets an activity-script manager to mode 6. */
__declspec(noinline) bool __fastcall manager_local_start(std::byte* manager,
                                                          std::uint8_t flags,
                                                          std::int32_t selector,
                                                          std::uint64_t sourceNonce,
                                                          std::uintptr_t arg5,
                                                          std::uintptr_t arg6,
                                                          std::uintptr_t arg7,
                                                          std::uintptr_t arg8) noexcept {
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    const auto* const caller = static_cast<const std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && caller >= image
                                         ? static_cast<std::uintptr_t>(caller - image)
                                         : 0U;
    const std::int32_t identityBefore =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1C7C0 : nullptr, -1);
    const std::int32_t modeBefore =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AEF8 : nullptr, -1);
    std::array<void*, 12> callstack{};
    const USHORT callstackCount = RtlCaptureStackBackTrace(
        0,
        static_cast<ULONG>(callstack.size()),
        callstack.data(),
        nullptr);
    const ManagerLocalStart original =
        g_managerLocalStartOriginal.load(std::memory_order_acquire);
    const bool result = original != nullptr
                            ? original(manager,
                                       flags,
                                       selector,
                                       sourceNonce,
                                       arg5,
                                       arg6,
                                       arg7,
                                       arg8)
                            : false;
    const std::uint32_t observation =
        g_managerStartObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
    if (observation > 128U) {
        return result;
    }
    std::string_view package{};
    const bool opening = opening_is_forced(package) && package == "mission_towerfall";
    std::array<char, core::log::kLineCapacity> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_manager_start n=%u route=local caller_rva=0x%llX manager=%p identity_before=%d identity_after=%d mode_before=%d mode_after=%d flags=0x%02X selector=%d source_nonce=0x%llX arg5=0x%llX arg6=0x%llX arg7=0x%llX arg8=0x%llX result=%u opening=%u package=%.*s",
        observation,
        static_cast<unsigned long long>(callerRva),
        static_cast<void*>(manager),
        identityBefore,
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1C7C0 : nullptr, -1),
        modeBefore,
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AEF8 : nullptr, -1),
        static_cast<unsigned int>(flags),
        selector,
        static_cast<unsigned long long>(sourceNonce),
        static_cast<unsigned long long>(arg5),
        static_cast<unsigned long long>(arg6),
        static_cast<unsigned long long>(arg7),
        static_cast<unsigned long long>(arg8),
        result ? 1U : 0U,
        opening ? 1U : 0U,
        static_cast<int>(package.size()),
        package.data());
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
    std::array<char, core::log::kLineCapacity> stackLine{};
    int stackLength = std::snprintf(
        stackLine.data(),
        stackLine.size(),
        "ev=bootflow stage=activity_script_manager_start_stack n=%u route=local count=%u rvas=",
        observation,
        static_cast<unsigned int>(callstackCount));
    for (USHORT index = 0;
         index < callstackCount && stackLength > 0
         && static_cast<std::size_t>(stackLength) < stackLine.size();
         ++index) {
        const auto address = reinterpret_cast<std::uintptr_t>(callstack[index]);
        const auto imageAddress = reinterpret_cast<std::uintptr_t>(image);
        const std::uintptr_t rva = image != nullptr && address >= imageAddress
                                       ? address - imageAddress
                                       : 0U;
        const int appended = std::snprintf(
            stackLine.data() + stackLength,
            stackLine.size() - static_cast<std::size_t>(stackLength),
            "%s0x%llX",
            index == 0 ? "" : ",",
            static_cast<unsigned long long>(rva));
        if (appended <= 0) {
            break;
        }
        stackLength += appended;
    }
    if (stackLength > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {stackLine.data(),
                          static_cast<std::size_t>(stackLength) < stackLine.size()
                              ? static_cast<std::size_t>(stackLength)
                              : stackLine.size() - 1U});
    }
    return result;
}

/** Records the authored initializer that constructs a manager's mode-1 payload. */
__declspec(noinline) void __fastcall manager_authored_start(std::byte* manager,
                                                             std::uint8_t flags,
                                                             std::int32_t selector,
                                                             const void* source,
                                                             std::uintptr_t arg5,
                                                             std::uintptr_t arg6,
                                                             std::uintptr_t arg7,
                                                             std::uintptr_t arg8,
                                                             std::uintptr_t arg9) noexcept {
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    const auto* const caller = static_cast<const std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && caller >= image
                                         ? static_cast<std::uintptr_t>(caller - image)
                                         : 0U;
    const std::int32_t identityBefore =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1C7C0 : nullptr, -1);
    const std::int32_t modeBefore =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AEF8 : nullptr, -1);
    const std::uint64_t source0 = safe_read<std::uint64_t>(source, 0U);
    const ManagerAuthoredStart original =
        g_managerAuthoredStartOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(manager, flags, selector, source, arg5, arg6, arg7, arg8, arg9);
    }
    std::string_view package{};
    const bool opening = opening_is_forced(package) && package == "mission_towerfall";
    const std::uint32_t observation =
        g_managerStartObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
    if (observation > 128U) {
        return;
    }
    std::array<char, core::log::kLineCapacity> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_manager_start n=%u route=authored caller_rva=0x%llX manager=%p identity_before=%d identity_after=%d mode_before=%d mode_after=%d flags=0x%02X selector=%d source=%p source0=0x%llX arg5=0x%llX arg6=0x%llX arg7=0x%llX arg8=0x%llX arg9=0x%llX opening=%u package=%.*s",
        observation,
        static_cast<unsigned long long>(callerRva),
        static_cast<void*>(manager),
        identityBefore,
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1C7C0 : nullptr, -1),
        modeBefore,
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AEF8 : nullptr, -1),
        static_cast<unsigned int>(flags),
        selector,
        source,
        static_cast<unsigned long long>(source0),
        static_cast<unsigned long long>(arg5),
        static_cast<unsigned long long>(arg6),
        static_cast<unsigned long long>(arg7),
        static_cast<unsigned long long>(arg8),
        static_cast<unsigned long long>(arg9),
        opening ? 1U : 0U,
        static_cast<int>(package.size()),
        package.data());
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
}

/** Records Destiny's native activity-script state-machine transitions without changing them. */
__declspec(noinline) void __fastcall manager_mode_set(std::byte* manager,
                                                       std::int32_t newMode,
                                                       std::uint32_t payloadSize,
                                                       const void* payload) noexcept {
    const std::int32_t identityBefore =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1C7C0 : nullptr, -1);
    const std::int32_t modeBefore =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AEF8 : nullptr, -1);
    const std::uint64_t payload0 = safe_read<std::uint64_t>(payload, 0U);
    const std::uint64_t payload8 = safe_read<std::uint64_t>(
        payload != nullptr ? static_cast<const std::byte*>(payload) + 8 : nullptr, 0U);
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    const auto* const caller = static_cast<const std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && caller >= image
                                         ? static_cast<std::uintptr_t>(caller - image)
                                         : 0U;

    const ManagerModeSet original = g_managerModeSetOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(manager, newMode, payloadSize, payload);
    }

    std::string_view package{};
    const bool opening = opening_is_forced(package) && package == "mission_towerfall";
    const std::int32_t identityAfter =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1C7C0 : nullptr, -1);
    const std::int32_t modeAfter =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AEF8 : nullptr, -1);
    const std::uint32_t observation =
        g_managerModeSetObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
    if (observation > 256U) {
        return;
    }
    std::array<char, core::log::kLineCapacity> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_manager_mode_set n=%u manager=%p identity_before=%d identity_after=%d mode_before=%d requested_mode=%d mode_after=%d payload_size=%u payload=%p payload0=0x%llX payload8=0x%llX caller_rva=0x%llX opening=%u package=%.*s",
        observation,
        static_cast<void*>(manager),
        identityBefore,
        identityAfter,
        modeBefore,
        newMode,
        modeAfter,
        payloadSize,
        payload,
        static_cast<unsigned long long>(payload0),
        static_cast<unsigned long long>(payload8),
        static_cast<unsigned long long>(callerRva),
        opening ? 1U : 0U,
        static_cast<int>(package.size()),
        package.data());
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
}

void observe_launch_producer_guard(const char* observer) noexcept {
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    const std::uint8_t current = safe_read<std::uint8_t>(
        image != nullptr ? image + kLaunchProducerGuardRva : nullptr,
        0xFFU);
    const std::uint8_t previous =
        g_launchProducerGuardLast.exchange(current, std::memory_order_acq_rel);
    if (current == previous) {
        return;
    }
    std::string_view package{};
    const bool opening = opening_is_forced(package) && package == "mission_towerfall";
    std::array<char, 256> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_launch_producer_guard observer=%s previous=%u current=%u opening=%u package=%.*s",
        observer,
        static_cast<unsigned int>(previous),
        static_cast<unsigned int>(current),
        opening ? 1U : 0U,
        static_cast<int>(package.size()),
        package.data());
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
}

/** Records the native eligibility decision that gates mode-4 manager setup and component work. */
__declspec(noinline) bool __fastcall manager_update_gate(std::byte* manager) noexcept {
    const ManagerUpdateGate original =
        g_managerUpdateGateOriginal.load(std::memory_order_acquire);
    const bool result = original != nullptr && original(manager);

    const std::int32_t identity =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1C7C0 : nullptr, -1);
    std::string_view package{};
    if ((identity != 1 && identity != 2) || !opening_is_forced(package)
        || package != "mission_towerfall") {
        return result;
    }

    const std::uint32_t observation = identity == 1
                                              ? g_managerUpdateGateIdentityOneObserved.fetch_add(
                                                    1U, std::memory_order_relaxed)
                                                    + 1U
                                              : g_managerUpdateGateIdentityTwoObserved.fetch_add(
                                                    1U, std::memory_order_relaxed)
                                                    + 1U;
    if (observation > 128U) {
        return result;
    }

    const std::int32_t tableIndex =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x874 : nullptr, -1);
    const bool tableIndexValid = tableIndex >= 0 && tableIndex < 64;
    const std::uint32_t tableValue = safe_read<std::uint32_t>(
        manager != nullptr && tableIndexValid
            ? manager + 0xE94C + (static_cast<std::ptrdiff_t>(tableIndex) * 0x38)
            : nullptr,
        0U);
    std::array<char, core::log::kLineCapacity> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_manager_update_gate identity_n=%u result=%s manager=%p identity=%d mode=%d activity=%d variant=%d table_index=%d table_value=0x%08X selected=%d registered=%d active=%d forced=%.*s",
        observation,
        result ? "accepted" : "rejected",
        static_cast<void*>(manager),
        identity,
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AEF8 : nullptr, -1),
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x850 : nullptr, -1),
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x854 : nullptr, -1),
        tableIndex,
        tableValue,
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x87C : nullptr, -1),
        safe_read<std::int32_t>(manager != nullptr ? manager + 0xE93C : nullptr, -1),
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AF00 : nullptr, -1),
        static_cast<int>(package.size()),
        package.data());
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
    return result;
}

constexpr std::size_t kComponentDescriptorBytes = 0xE8U;

struct ComponentDescriptorSnapshot final {
    std::uint32_t nonzeroBytes{};
    std::uint64_t hash{1469598103934665603ULL};
};

[[nodiscard]] ComponentDescriptorSnapshot snapshot_component_descriptor(
    const std::byte* descriptor) noexcept {
    ComponentDescriptorSnapshot snapshot{};
    for (std::size_t index = 0; index < kComponentDescriptorBytes; ++index) {
        const std::uint8_t value = safe_read<std::uint8_t>(
            descriptor != nullptr ? descriptor + index : nullptr, 0U);
        snapshot.nonzeroBytes += value != 0U ? 1U : 0U;
        snapshot.hash ^= value;
        snapshot.hash *= 1099511628211ULL;
    }
    return snapshot;
}

/** Observes whether the activity-script manager reaches its native per-frame update loop. */
struct ComponentSlotState final {
    std::byte* manager{};
    std::byte* component{};
    std::byte* sourceRecord{};
    std::byte* activitySlot{};
    std::int32_t identity{-1};
    std::int32_t activity{-1};
    std::uint8_t sourceReady{0xFFU};
    std::uint8_t activitySlotReady{0xFFU};
    std::uint32_t sourceGeneration{0xFFFFFFFFU};
    std::uint32_t activitySlotGeneration{0xFFFFFFFFU};
    std::uint64_t sourceDescriptorWord0{};
    std::uint64_t activitySlotDescriptorWord0{};
    ComponentDescriptorSnapshot sourceDescriptor{};
    ComponentDescriptorSnapshot activitySlotDescriptor{};
    bool valid{};
};

/** Reads the source descriptor and per-activity publication slot used by component-register. */
[[nodiscard]] ComponentSlotState read_component_slot_state(std::byte* manager,
                                                            std::int32_t componentIndex) noexcept {
    ComponentSlotState state{};
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    if (image == nullptr || manager == nullptr || componentIndex < 0 || componentIndex >= 64) {
        return state;
    }
    __try {
        auto* const table = reinterpret_cast<ActivityStateTable>(image + kActivityStateTableRva)();
        state.manager = manager;
        state.identity = *reinterpret_cast<const std::int32_t*>(manager + 0x1C7C0);
        state.activity = *reinterpret_cast<const std::int32_t*>(manager + 0x850);
        if (table == nullptr || state.activity < -1 || state.activity >= 64) {
            return state;
        }
        state.component = table + (static_cast<std::ptrdiff_t>(componentIndex) * 0x7A8);
        state.sourceRecord = state.component + 0x148;
        state.activitySlot = state.sourceRecord
                             + (static_cast<std::ptrdiff_t>(state.activity) + 1) * 0x118;
        state.sourceReady = *reinterpret_cast<const std::uint8_t*>(state.sourceRecord);
        state.activitySlotReady = *reinterpret_cast<const std::uint8_t*>(state.activitySlot);
        state.sourceGeneration =
            *reinterpret_cast<const std::uint32_t*>(state.component + 0x240);
        state.activitySlotGeneration =
            *reinterpret_cast<const std::uint32_t*>(state.activitySlot + 0x4);
        state.sourceDescriptorWord0 =
            *reinterpret_cast<const std::uint64_t*>(state.sourceRecord + 0x10);
        state.activitySlotDescriptorWord0 =
            *reinterpret_cast<const std::uint64_t*>(state.activitySlot + 0x10);
        state.sourceDescriptor = snapshot_component_descriptor(state.sourceRecord + 0x10);
        state.activitySlotDescriptor =
            snapshot_component_descriptor(state.activitySlot + 0x10);
        state.valid = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return ComponentSlotState{};
    }
    return state;
}

[[nodiscard]] std::uint64_t component_slot_state_signature(
    const ComponentSlotState& state) noexcept {
    std::uint64_t signature = state.valid ? 0xCBF29CE484222325ULL : 0U;
    const auto mix = [&signature](std::uint64_t value) noexcept {
        signature ^= value;
        signature *= 1099511628211ULL;
    };
    mix(reinterpret_cast<std::uintptr_t>(state.manager));
    mix(static_cast<std::uint32_t>(state.identity));
    mix(static_cast<std::uint32_t>(state.activity));
    mix(state.sourceReady);
    mix(state.activitySlotReady);
    mix(state.sourceGeneration);
    mix(state.activitySlotGeneration);
    mix(state.sourceDescriptorWord0);
    mix(state.activitySlotDescriptorWord0);
    mix(state.sourceDescriptor.nonzeroBytes);
    mix(state.sourceDescriptor.hash);
    mix(state.activitySlotDescriptor.nonzeroBytes);
    mix(state.activitySlotDescriptor.hash);
    return signature;
}

/** Emits only real slot-state changes, keeping the per-frame observer diagnostic-only. */
void report_component_slot_state(std::byte* manager,
                                 std::int32_t componentIndex,
                                 const char* phase) noexcept {
    const ComponentSlotState state = read_component_slot_state(manager, componentIndex);
    const std::uint64_t signature = component_slot_state_signature(state);
    std::atomic_uint64_t& lastSignature =
        state.identity == 2 ? g_componentSlotStateIdentityTwoLastSignature
                            : g_componentSlotStateLastSignature;
    const std::uint64_t previous = lastSignature.exchange(signature, std::memory_order_acq_rel);
    if (signature == previous) {
        return;
    }
    const std::uint32_t observation =
        g_componentSlotStateObserved.fetch_add(1U, std::memory_order_relaxed) + 1U;
    if (observation > 64U) {
        return;
    }
    std::array<char, core::log::kLineCapacity> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_component_slot_state n=%u phase=%s valid=%u manager=%p identity=%d mode=%d manager_component=%d registration_table=%d component_index=%d activity=%d component=%p source_record=%p activity_slot=%p source_ready=%u activity_slot_ready=%u source_generation=%u activity_slot_generation=%u generation_match=%u source_descriptor_word0=0x%llX activity_slot_descriptor_word0=0x%llX source_descriptor_nonzero=%u source_descriptor_hash=0x%llX activity_slot_descriptor_nonzero=%u activity_slot_descriptor_hash=0x%llX",
        observation,
        phase,
        state.valid ? 1U : 0U,
        static_cast<void*>(manager),
        state.identity,
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AEF8 : nullptr, -1),
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AF00 : nullptr, -1),
        safe_read<std::int32_t>(manager != nullptr ? manager + 0xE93C : nullptr, -1),
        componentIndex,
        state.activity,
        static_cast<void*>(state.component),
        static_cast<void*>(state.sourceRecord),
        static_cast<void*>(state.activitySlot),
        static_cast<unsigned int>(state.sourceReady),
        static_cast<unsigned int>(state.activitySlotReady),
        state.sourceGeneration,
        state.activitySlotGeneration,
        state.valid && state.sourceGeneration == state.activitySlotGeneration ? 1U : 0U,
        static_cast<unsigned long long>(state.sourceDescriptorWord0),
        static_cast<unsigned long long>(state.activitySlotDescriptorWord0),
        state.sourceDescriptor.nonzeroBytes,
        static_cast<unsigned long long>(state.sourceDescriptor.hash),
        state.activitySlotDescriptor.nonzeroBytes,
        static_cast<unsigned long long>(state.activitySlotDescriptor.hash));
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
}

/** Hashes the three lifecycle descriptor banks without retaining a transient pointer. */
[[nodiscard]] std::uint64_t hash_lifecycle_descriptor(const std::byte* descriptor) noexcept {
    if (descriptor == nullptr) {
        return 0U;
    }
    std::uint64_t hash = 1469598103934665603ULL;
    __try {
        for (std::size_t index = 0; index < 0x80U; ++index) {
            hash ^= std::to_integer<std::uint8_t>(descriptor[index]);
            hash *= 1099511628211ULL;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0U;
    }
    return hash;
}

/**
 * Runs one retained Omega lifecycle stage per identity-1 manager update. This is the exact retail
 * caller contract: read manager+0x1AF00, call the native gateway with manager+0x1AF04, and store
 * the returned stage for the next tick.
 */
void drive_omega_lifecycle(std::byte* manager, std::string_view package) noexcept {
    if (manager == nullptr || package != "mission_scot"
        || !g_omegaLifecycleStarted.load(std::memory_order_acquire)
        || g_omegaLifecycleComplete.load(std::memory_order_acquire)
        || g_omegaLifecycleManager.load(std::memory_order_acquire) != manager
        || safe_read<std::int32_t>(manager + 0x1C7C0, -1) != 1) {
        return;
    }

    const std::int32_t stage = safe_read<std::int32_t>(manager + 0x1AF00, -1);
    if (stage == 0) {
        g_omegaLifecycleComplete.store(true, std::memory_order_release);
        core::log::write(
            core::log::Channel::client,
            core::log::Level::info,
            "ev=bootflow stage=activity_script_lifecycle_runner result=complete identity=1 retained_stage=0");
        return;
    }
    if (stage < 1 || stage > 5) {
        std::array<char, 256> invalidLine{};
        const int invalidLength = std::snprintf(
            invalidLine.data(),
            invalidLine.size(),
            "ev=bootflow stage=activity_script_lifecycle_runner result=stop reason=invalid_stage identity=1 manager=%p retained_stage=%d",
            static_cast<void*>(manager),
            stage);
        if (invalidLength > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::error,
                             {invalidLine.data(), static_cast<std::size_t>(invalidLength)});
        }
        return;
    }

    std::byte* definition = nullptr;
    const auto identityDefinition = reinterpret_cast<IdentityDefinition>(
        validated_target(kIdentityDefinitionRva, kIdentityDefinitionPrefix));
    __try {
        definition = identityDefinition != nullptr ? identityDefinition(1) : nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        definition = nullptr;
    }
    if (definition == nullptr) {
        return;
    }

    const std::uint32_t flagsBefore = safe_read<std::uint32_t>(definition + 0x4, 0U);
    const std::uint8_t enabledBefore = safe_read<std::uint8_t>(definition + 0x94C, 0U);
    const std::uint8_t pendingBefore = safe_read<std::uint8_t>(definition + 0x94D, 0U);
    const std::uint64_t activeHashBefore = hash_lifecycle_descriptor(definition + 0x57C);
    const std::uint64_t pendingHashBefore = hash_lifecycle_descriptor(definition + 0x94E);
    const std::uint64_t payloadHash = hash_lifecycle_descriptor(manager + 0x1AF04);

    std::int32_t returnedStage = stage;
    bool lifecycleReturned = true;
    __try {
        returnedStage = lifecycle_event(manager, stage, manager + 0x1AF04);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        lifecycleReturned = false;
    }
    if (!lifecycleReturned || returnedStage < 0 || returnedStage > 5) {
        std::array<char, 320> failureLine{};
        const int failureLength = std::snprintf(
            failureLine.data(),
            failureLine.size(),
            "ev=bootflow stage=activity_script_lifecycle_runner result=%s identity=1 manager=%p retained_stage=%d returned_stage=%d",
            lifecycleReturned ? "invalid_return" : "exception",
            static_cast<void*>(manager),
            stage,
            returnedStage);
        if (failureLength > 0) {
            core::log::write(core::log::Channel::client,
                             core::log::Level::error,
                             {failureLine.data(), static_cast<std::size_t>(failureLength)});
        }
        return;
    }
    __try {
        *reinterpret_cast<std::int32_t*>(manager + 0x1AF00) = returnedStage;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return;
    }

    const std::uint32_t call =
        g_omegaLifecycleCalls.fetch_add(1U, std::memory_order_relaxed) + 1U;
    std::uint32_t flagsAfter = safe_read<std::uint32_t>(definition + 0x4, 0U);
    const std::uint8_t enabledAfter = safe_read<std::uint8_t>(definition + 0x94C, 0U);
    const std::uint8_t pendingAfter = safe_read<std::uint8_t>(definition + 0x94D, 0U);
    bool attemptedAcknowledgement = false;
    bool acknowledgementReturned = false;

    if (stage == 2 && returnedStage == 2 && (flagsAfter & 0x80U) == 0U) {
        const std::uint32_t stalls =
            g_omegaStageTwoStalls.fetch_add(1U, std::memory_order_relaxed) + 1U;
        const std::uint64_t now = GetTickCount64();
        std::uint64_t firstTick = g_omegaStageTwoFirstTick.load(std::memory_order_acquire);
        if (firstTick == 0U) {
            g_omegaStageTwoFirstTick.compare_exchange_strong(
                firstTick, now, std::memory_order_acq_rel, std::memory_order_acquire);
            firstTick = g_omegaStageTwoFirstTick.load(std::memory_order_acquire);
        }
        bool expected = false;
        if (now - firstTick >= 1'000U
            && g_omegaPendingConsumerAttempted.compare_exchange_strong(
                expected, true, std::memory_order_acq_rel, std::memory_order_acquire)) {
            attemptedAcknowledgement = true;
            const auto pendingConsumer = reinterpret_cast<DefinitionPendingConsumer>(
                validated_target(kDefinitionPendingConsumerRva,
                                 kDefinitionPendingConsumerPrefix));
            if (pendingConsumer != nullptr) {
                __try {
                    pendingConsumer(1);
                    acknowledgementReturned = true;
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                    acknowledgementReturned = false;
                }
                flagsAfter = safe_read<std::uint32_t>(definition + 0x4, 0U);
            }
        }
        if (stalls != 1U && stalls % 60U != 0U && !attemptedAcknowledgement) {
            return;
        }
    } else {
        g_omegaStageTwoStalls.store(0U, std::memory_order_release);
        g_omegaStageTwoFirstTick.store(0U, std::memory_order_release);
    }

    std::array<char, core::log::kLineCapacity> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_lifecycle_runner result=%s call=%u identity=1 manager=%p retained_stage=%d returned_stage=%d flags_before=0x%08X flags_after=0x%08X flag40=%u flag80=%u flag200=%u enabled_before=%u enabled_after=%u pending_before=%u pending_after=%u payload_hash=0x%llX active_hash_before=0x%llX active_hash_after=0x%llX pending_hash_before=0x%llX pending_hash_after=0x%llX ack_attempted=%u ack_returned=%u",
        returnedStage == 0 ? "complete" : (returnedStage == stage ? "retry" : "advanced"),
        call,
        static_cast<void*>(manager),
        stage,
        returnedStage,
        flagsBefore,
        flagsAfter,
        (flagsAfter & 0x40U) != 0U ? 1U : 0U,
        (flagsAfter & 0x80U) != 0U ? 1U : 0U,
        (flagsAfter & 0x200U) != 0U ? 1U : 0U,
        static_cast<unsigned int>(enabledBefore),
        static_cast<unsigned int>(safe_read<std::uint8_t>(definition + 0x94C, enabledAfter)),
        static_cast<unsigned int>(pendingBefore),
        static_cast<unsigned int>(safe_read<std::uint8_t>(definition + 0x94D, pendingAfter)),
        static_cast<unsigned long long>(payloadHash),
        static_cast<unsigned long long>(activeHashBefore),
        static_cast<unsigned long long>(hash_lifecycle_descriptor(definition + 0x57C)),
        static_cast<unsigned long long>(pendingHashBefore),
        static_cast<unsigned long long>(hash_lifecycle_descriptor(definition + 0x94E)),
        attemptedAcknowledgement ? 1U : 0U,
        acknowledgementReturned ? 1U : 0U);
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         returnedStage == stage ? core::log::Level::warn
                                                : core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
    if (returnedStage == 0) {
        g_omegaLifecycleComplete.store(true, std::memory_order_release);
    }
}

__declspec(noinline) void __fastcall manager_update_loop(std::byte* manager) noexcept {
    observe_launch_producer_guard("manager_update");
    std::string_view package{};
    const bool opening = opening_is_forced(package);
    const std::int32_t identity =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1C7C0 : nullptr, -1);
    const std::uint32_t observation = opening
                                          ? g_managerUpdateLoopObserved.fetch_add(
                                                1, std::memory_order_relaxed)
                                                + 1U
                                          : 0U;
    const std::uint32_t identityObservation = opening && identity == 1
                                                  ? g_managerUpdateLoopIdentityOneObserved.fetch_add(
                                                        1, std::memory_order_relaxed)
                                                        + 1U
                                                  : 0U;
    const std::uint32_t identityTwoObservation = opening && identity == 2
                                                     ? g_managerUpdateLoopIdentityTwoObserved.fetch_add(
                                                           1U, std::memory_order_relaxed)
                                                           + 1U
                                                     : 0U;
    const std::int32_t modeBefore =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AEF8 : nullptr, -1);
    const std::int32_t selectedBefore =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x87C : nullptr, -1);
    const std::int32_t registeredBefore =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0xE93C : nullptr, -1);
    const std::int32_t activeBefore =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AF00 : nullptr, -1);

    const ManagerUpdateLoop original =
        g_managerUpdateLoopOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(manager);
    }
    // Do not drive the manager's retained stage here. RVA 0x176F870 is Destiny's managed-session
    // migration machine, not an authored mission-script lifecycle. Retail reaches its wrapper only
    // from manager mode 4; this observer runs for identity 1 in mode 6. Calling it here advanced the
    // migration to an unsatisfiable stage 3 and retried it every frame. It is unrelated to mission
    // execution and mutates session ownership out of contract.
    if (opening && package == "mission_towerfall" && (identity == 1 || identity == 2)) {
        report_component_slot_state(manager, 0, "manager_update_after");
    }
    service_homecoming_launch_producer_window(opening, package, identity);

    const std::int32_t modeAfter =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AEF8 : nullptr, -1);
    if (identity == 1) {
        std::byte* const previousManager =
            g_managerIdentityOneLastPointer.exchange(manager, std::memory_order_acq_rel);
        const std::int32_t previousMode =
            g_managerIdentityOneLastMode.exchange(modeAfter, std::memory_order_acq_rel);
        if (previousManager != manager || previousMode != modeAfter || modeBefore != modeAfter) {
            const std::uint32_t transition =
                g_managerIdentityOneModeTransitionObserved.fetch_add(
                    1U, std::memory_order_relaxed) + 1U;
            if (transition <= 64U) {
                std::array<char, core::log::kLineCapacity> transitionLine{};
                const int transitionLength = std::snprintf(
                    transitionLine.data(),
                    transitionLine.size(),
                    "ev=bootflow stage=activity_script_manager_mode_transition n=%u manager=%p previous_manager=%p previous_mode=%d mode_before=%d mode_after=%d selected=%d registered=%d active=%d opening=%u package=%.*s",
                    transition,
                    static_cast<void*>(manager),
                    static_cast<void*>(previousManager),
                    previousMode,
                    modeBefore,
                    modeAfter,
                    safe_read<std::int32_t>(manager != nullptr ? manager + 0x87C : nullptr, -1),
                    safe_read<std::int32_t>(manager != nullptr ? manager + 0xE93C : nullptr, -1),
                    safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AF00 : nullptr, -1),
                    opening && package == "mission_towerfall" ? 1U : 0U,
                    static_cast<int>(package.size()),
                    package.data());
                if (transitionLength > 0) {
                    core::log::write(
                        core::log::Channel::client,
                        core::log::Level::info,
                        {transitionLine.data(), static_cast<std::size_t>(transitionLength)});
                }
            }
        }
    }

    // The full-table recorder intentionally stops after 32 snapshots and used to miss the
    // identity-2 state reached after the six-second migration. Keep a change-only recorder alive
    // for the entire Homecoming session so the native 0->1->2->3->4 ladder is measurable without
    // per-frame log noise.
    if (opening && package == "mission_towerfall" && identity == 2) {
        const std::int32_t componentAfter =
            safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AF00 : nullptr, -1);
        std::byte* const previousManager =
            g_managerIdentityTwoLastPointer.exchange(manager, std::memory_order_acq_rel);
        const std::int32_t previousComponent =
            g_managerIdentityTwoLastComponent.exchange(componentAfter, std::memory_order_acq_rel);
        if (previousManager != manager || previousComponent != componentAfter
            || activeBefore != componentAfter) {
            const std::uint32_t transition =
                g_managerIdentityTwoComponentTransitionObserved.fetch_add(
                    1U, std::memory_order_relaxed) + 1U;
            if (transition <= 64U) {
                std::byte* definition = nullptr;
                const auto identityDefinition = reinterpret_cast<IdentityDefinition>(
                    validated_target(kIdentityDefinitionRva, kIdentityDefinitionPrefix));
                __try {
                    definition = identityDefinition != nullptr
                                     ? identityDefinition(identity)
                                     : nullptr;
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                    definition = nullptr;
                }
                std::array<char, core::log::kLineCapacity> transitionLine{};
                const int transitionLength = std::snprintf(
                    transitionLine.data(),
                    transitionLine.size(),
                    "ev=bootflow stage=activity_script_identity2_component_transition n=%u manager=%p previous_manager=%p previous_component=%d component_before=%d component_after=%d mode=%d selected=%d registered=%d definition=%p definition_flags=0x%08X definition_state=0x%04X definition_context=%p definition_activity=%d definition_request=0x%04X definition_enabled=%u definition_pending=%u forced=%.*s",
                    transition,
                    static_cast<void*>(manager),
                    static_cast<void*>(previousManager),
                    previousComponent,
                    activeBefore,
                    componentAfter,
                    modeAfter,
                    safe_read<std::int32_t>(manager != nullptr ? manager + 0x87C : nullptr, -1),
                    safe_read<std::int32_t>(manager != nullptr ? manager + 0xE93C : nullptr, -1),
                    static_cast<void*>(definition),
                    safe_read<std::uint32_t>(definition != nullptr ? definition + 0x4 : nullptr,
                                             0U),
                    static_cast<unsigned int>(safe_read<std::uint16_t>(
                        definition != nullptr ? definition + 0xA : nullptr, 0U)),
                    safe_read<void*>(definition != nullptr ? definition + 0x18 : nullptr,
                                     nullptr),
                    safe_read<std::int32_t>(definition != nullptr ? definition + 0x24 : nullptr,
                                            -1),
                    static_cast<unsigned int>(safe_read<std::uint16_t>(
                        definition != nullptr ? definition + 0x2C : nullptr, 0U)),
                    static_cast<unsigned int>(safe_read<std::uint8_t>(
                        definition != nullptr ? definition + 0x94C : nullptr, 0U)),
                    static_cast<unsigned int>(safe_read<std::uint8_t>(
                        definition != nullptr ? definition + 0x94D : nullptr, 0U)),
                    static_cast<int>(package.size()),
                    package.data());
                if (transitionLength > 0) {
                    core::log::write(
                        core::log::Channel::client,
                        core::log::Level::info,
                        {transitionLine.data(), static_cast<std::size_t>(transitionLength)});
                }
            }
        }
    }

    if (opening && package == "mission_towerfall" && identity == 1) {
        qualify_event22_candidate(package);
    }

    // Do not invoke the native component dispatcher from this manager-update detour. Tests at
    // physics-join, roster-finalize, and in-world timing all either invalidated membership or
    // permanently stalled network_update. The remaining work is to restore the native manager
    // preconditions so Destiny reaches this dispatcher through its ordinary call path.

    const bool identityTwoDiagnostic = identity == 2 && identityTwoObservation <= 128U;
    if (!identityTwoDiagnostic
        && (observation == 0U
            || (observation > 512U && (identity != 1 || identityObservation > 256U)))) {
        return;
    }
    const std::int32_t selectedAfter =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x87C : nullptr, -1);
    const std::int32_t registeredAfter =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0xE93C : nullptr, -1);
    const std::int32_t activeAfter =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AF00 : nullptr, -1);
    std::byte* definition = nullptr;
    if (identity == 1) {
        const auto identityDefinition = reinterpret_cast<IdentityDefinition>(
            validated_target(kIdentityDefinitionRva, kIdentityDefinitionPrefix));
        __try {
            definition = identityDefinition != nullptr ? identityDefinition(identity) : nullptr;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            definition = nullptr;
        }
    }
    const std::uint32_t definitionFlags =
        safe_read<std::uint32_t>(definition != nullptr ? definition + 0x4 : nullptr, 0U);
    const std::uint16_t definitionState =
        safe_read<std::uint16_t>(definition != nullptr ? definition + 0xA : nullptr, 0U);
    const std::uintptr_t definitionContext =
        safe_read<std::uintptr_t>(definition != nullptr ? definition + 0x18 : nullptr, 0U);
    const std::int32_t definitionActivity =
        safe_read<std::int32_t>(definition != nullptr ? definition + 0x24 : nullptr, -1);
    const std::uint8_t definitionEnabled =
        safe_read<std::uint8_t>(definition != nullptr ? definition + 0x94C : nullptr, 0U);
    const std::uint8_t definitionPending =
        safe_read<std::uint8_t>(definition != nullptr ? definition + 0x94D : nullptr, 0U);
    std::array<char, core::log::kLineCapacity> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_manager_update_loop n=%u identity_n=%u identity2_n=%u manager=%p identity=%d mode_before=%d mode_after=%d selected_before=%d selected_after=%d registered_before=%d registered_after=%d active_before=%d active_after=%d definition=%p definition_flags=0x%08X definition_state=0x%04X definition_context=%p definition_activity=%d definition_enabled=%u definition_pending=%u forced=%.*s",
        observation,
        identityObservation,
        identityTwoObservation,
        static_cast<void*>(manager),
        identity,
        modeBefore,
        modeAfter,
        selectedBefore,
        selectedAfter,
        registeredBefore,
        registeredAfter,
        activeBefore,
        activeAfter,
        static_cast<void*>(definition),
        definitionFlags,
        definitionState,
        reinterpret_cast<void*>(definitionContext),
        definitionActivity,
        static_cast<unsigned int>(definitionEnabled),
        static_cast<unsigned int>(definitionPending),
        static_cast<int>(package.size()),
        package.data());
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
}

struct PostComponentSyncState final {
    std::int32_t cachedGeneration{-1};
    std::int32_t nativeGeneration{-1};
    std::int32_t firstEntry{-1};
    std::uint64_t sourceHash{};
    std::uint64_t destinationHash{};
};

/** Hashes one descriptor payload without letting a transient runtime pointer escape the probe. */
[[nodiscard]] std::uint64_t hash_descriptor_payload(const std::byte* payload) noexcept {
    if (payload == nullptr) {
        return 0U;
    }
    std::uint64_t hash = 1469598103934665603ULL;
    __try {
        for (std::size_t index = 0; index < 0x80U; ++index) {
            hash ^= std::to_integer<std::uint8_t>(payload[index]);
            hash *= 1099511628211ULL;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0U;
    }
    return hash;
}

/** Reads the same generation and first descriptor pair consumed by the native sync routine. */
[[nodiscard]] PostComponentSyncState read_post_component_sync_state(std::byte* manager) noexcept {
    PostComponentSyncState state{};
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    if (image == nullptr || manager == nullptr) {
        return state;
    }
    __try {
        const std::int32_t activity = *reinterpret_cast<const std::int32_t*>(manager + 0x850);
        auto* const activityState =
            reinterpret_cast<ActivityStateTable>(image + kActivityStateTableRva)();
        if (activityState != nullptr && activity >= 0 && activity < 1024) {
            state.cachedGeneration = *reinterpret_cast<const std::int32_t*>(
                activityState + (static_cast<std::ptrdiff_t>(activity) * 0x1F0) + 0xC14);
        }
        state.nativeGeneration =
            reinterpret_cast<ManagerGeneration>(image + kManagerGenerationRva)(manager);
        std::byte* const snapshot =
            reinterpret_cast<ManagerSnapshot>(image + kManagerSnapshotRva)(manager);
        if (snapshot != nullptr) {
            state.firstEntry =
                reinterpret_cast<SnapshotFirstEntry>(image + kSnapshotFirstEntryRva)(snapshot);
            if (state.firstEntry >= 0 && state.firstEntry < 1024) {
                const std::ptrdiff_t entryOffset =
                    static_cast<std::ptrdiff_t>(state.firstEntry) * 0x1A8;
                state.sourceHash =
                    hash_descriptor_payload(snapshot + 0x3B80 + entryOffset);
                state.destinationHash =
                    hash_descriptor_payload(snapshot + 0x3C6C + entryOffset);
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return PostComponentSyncState{};
    }
    return state;
}

/** Observes the post-build descriptor synchronization that follows the component loop. */
__declspec(noinline) void __fastcall post_component_sync(std::byte* manager, bool force) noexcept {
    const std::int32_t identity =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1C7C0 : nullptr, -1);
    std::string_view package{};
    const bool opening = identity == 1 && opening_is_forced(package)
                         && package == "mission_towerfall";
    const PostComponentSyncState before =
        opening ? read_post_component_sync_state(manager) : PostComponentSyncState{};
    const std::int32_t modeBefore =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AEF8 : nullptr, -1);
    const std::int32_t registeredBefore =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0xE93C : nullptr, -1);
    const std::int32_t activeBefore =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AF00 : nullptr, -1);

    const PostComponentSync original =
        g_postComponentSyncOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(manager, force);
    }
    if (!opening) {
        return;
    }

    const PostComponentSyncState after = read_post_component_sync_state(manager);
    const std::int32_t modeAfter =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AEF8 : nullptr, -1);
    const std::int32_t registeredAfter =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0xE93C : nullptr, -1);
    const std::int32_t activeAfter =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AF00 : nullptr, -1);
    const bool changed = before.cachedGeneration != after.cachedGeneration
                         || before.nativeGeneration != after.nativeGeneration
                         || before.firstEntry != after.firstEntry
                         || before.sourceHash != after.sourceHash
                         || before.destinationHash != after.destinationHash
                         || modeBefore != modeAfter || registeredBefore != registeredAfter
                         || activeBefore != activeAfter;
    const std::uint32_t observation =
        g_postComponentSyncObserved.fetch_add(1, std::memory_order_relaxed) + 1U;
    if (observation > 128U && !changed) {
        return;
    }
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;
    std::array<char, core::log::kLineCapacity> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_post_component_sync n=%u changed=%u caller_rva=0x%llX manager=%p force=%u activity=%d variant=%d mode_before=%d mode_after=%d registered_before=%d registered_after=%d active_before=%d active_after=%d cached_generation_before=%d cached_generation_after=%d native_generation_before=%d native_generation_after=%d first_entry_before=%d first_entry_after=%d source_hash_before=0x%llX source_hash_after=0x%llX destination_hash_before=0x%llX destination_hash_after=0x%llX forced=%.*s",
        observation,
        changed ? 1U : 0U,
        static_cast<unsigned long long>(callerRva),
        static_cast<void*>(manager),
        force ? 1U : 0U,
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x850 : nullptr, -1),
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x854 : nullptr, -1),
        modeBefore,
        modeAfter,
        registeredBefore,
        registeredAfter,
        activeBefore,
        activeAfter,
        before.cachedGeneration,
        after.cachedGeneration,
        before.nativeGeneration,
        after.nativeGeneration,
        before.firstEntry,
        after.firstEntry,
        static_cast<unsigned long long>(before.sourceHash),
        static_cast<unsigned long long>(after.sourceHash),
        static_cast<unsigned long long>(before.destinationHash),
        static_cast<unsigned long long>(after.destinationHash),
        static_cast<int>(package.size()),
        package.data());
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
}

/** Records the native component-dispatch branch selected by the manager update loop. */
__declspec(noinline) void __fastcall component_dispatch(std::byte* manager,
                                                         std::int32_t componentIndex) noexcept {
    const std::int32_t identity =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1C7C0 : nullptr, -1);
    const std::int32_t registeredBefore =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0xE93C : nullptr, -1);
    const std::int32_t activeBefore =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AF00 : nullptr, -1);
    const ComponentDispatch original =
        g_componentDispatchOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(manager, componentIndex);
    }

    std::string_view package{};
    if ((identity != 1 && identity != 2) || !opening_is_forced(package)
        || package != "mission_towerfall") {
        return;
    }
    const std::uint32_t observation = identity == 1
                                              ? g_componentDispatchIdentityOneObserved.fetch_add(
                                                    1U, std::memory_order_relaxed)
                                                    + 1U
                                              : g_componentDispatchIdentityTwoObserved.fetch_add(
                                                    1U, std::memory_order_relaxed)
                                                    + 1U;
    if (observation > 128U) {
        return;
    }

    std::array<char, core::log::kLineCapacity> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_component_dispatch identity_n=%u manager=%p identity=%d component_index=%d mode=%d selected=%d registered_before=%d registered_after=%d active_before=%d active_after=%d forced=%.*s",
        observation,
        static_cast<void*>(manager),
        identity,
        componentIndex,
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AEF8 : nullptr, -1),
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x87C : nullptr, -1),
        registeredBefore,
        safe_read<std::int32_t>(manager != nullptr ? manager + 0xE93C : nullptr, -1),
        activeBefore,
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AF00 : nullptr, -1),
        static_cast<int>(package.size()),
        package.data());
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
}

/** Records the exact native table gate used immediately before component activation. */
__declspec(noinline) bool __fastcall component_lookup(std::byte* context,
                                                       const void* identifier,
                                                       std::int32_t slot,
                                                       std::int32_t componentIndex) noexcept {
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0;
    const ComponentLookup original =
        g_componentLookupOriginal.load(std::memory_order_acquire);
    bool result =
        original != nullptr && original(context, identifier, slot, componentIndex);

    std::string_view package{};
    if (!opening_is_forced(package)) {
        return result;
    }
    // The context embedded at manager + 0x860 is shared by the orbit (identity 0)
    // and mission (identity 1) managers. Keep a separate identity-1 budget because
    // orbit can consume the general observation budget before Homecoming initializes.
    constexpr std::ptrdiff_t kContextToManagerIdentity = 0x1C7C0 - 0x860;
    const std::int32_t managerIdentity = safe_read<std::int32_t>(
        context != nullptr ? context + kContextToManagerIdentity : nullptr, -1);
    const std::uint32_t observation =
        g_componentLookupObserved.fetch_add(1, std::memory_order_relaxed) + 1U;
    const std::uint32_t identityObservation = managerIdentity == 1
                                                  ? g_componentLookupIdentityOneObserved.fetch_add(
                                                        1, std::memory_order_relaxed)
                                                        + 1U
                                                  : 0U;
    const std::uint32_t identityTwoObservation = managerIdentity == 2
                                                     ? g_componentLookupIdentityTwoObserved.fetch_add(
                                                           1U, std::memory_order_relaxed)
                                                           + 1U
                                                     : 0U;
    const bool focusedIdentity = (managerIdentity == 1 && identityObservation <= 256U)
                                 || (managerIdentity == 2 && identityTwoObservation <= 128U);
    if (observation > 512U && !focusedIdentity) {
        return result;
    }

    const std::uint32_t mask =
        safe_read<std::uint32_t>(context != nullptr ? context + 0x30 : nullptr, 0U);
    const std::int32_t slotMode = safe_read<std::int32_t>(
        context != nullptr && slot >= 0 ? context + 0x1758 + (slot * 0x120) : nullptr, -1);
    const std::int32_t entryCount = safe_read<std::int32_t>(
        context != nullptr && slot >= 0 ? context + 0xE8 + (slot * 0xB8) : nullptr, -1);
    const std::int32_t mapping = safe_read<std::int32_t>(
        context != nullptr && slot >= 0 && componentIndex >= 0
            ? context + 0xF0 + (slot * 0xB8) + (componentIndex * 4)
            : nullptr,
        -1);
    const std::uint64_t actualIdentifier = safe_read<std::uint64_t>(identifier, 0U);
    const std::uint64_t expectedIdentifier = safe_read<std::uint64_t>(
        context != nullptr && mapping >= 0 ? context + 0x3B64 + (mapping * 0x1A8) : nullptr,
        0U);
    std::array<char, core::log::kLineCapacity> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_component_lookup n=%u identity_n=%u identity2_n=%u result=%s caller_rva=0x%llX context=%p manager_identity=%d identifier=%p slot=%d component_index=%d mask=0x%08X slot_mode=%d entry_count=%d mapping=%d actual_id=0x%llX expected_id=0x%llX forced=%.*s",
        observation,
        identityObservation,
        identityTwoObservation,
        result ? "accepted" : "rejected",
        static_cast<unsigned long long>(callerRva),
        static_cast<void*>(context),
        managerIdentity,
        identifier,
        slot,
        componentIndex,
        mask,
        slotMode,
        entryCount,
        mapping,
        static_cast<unsigned long long>(actualIdentifier),
        static_cast<unsigned long long>(expectedIdentifier),
        static_cast<int>(package.size()),
        package.data());
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
    return result;
}

/** Observes the final per-activity registration call after component reuse and mapping. */
__declspec(noinline) void __fastcall component_register(std::byte* manager,
                                                          std::int32_t componentIndex) noexcept {
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;
    const std::int32_t identity =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1C7C0 : nullptr, -1);
    const std::int32_t registeredBefore =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0xE93C : nullptr, -1);
    const std::int32_t activeBefore =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AF00 : nullptr, -1);
    std::string_view package{};
    const bool target = (identity == 1 || identity == 2) && opening_is_forced(package)
                        && package == "mission_towerfall";
    const ComponentSlotState slotBefore =
        target ? read_component_slot_state(manager, componentIndex) : ComponentSlotState{};
    const ComponentRegister original =
        g_componentRegisterOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(manager, componentIndex);
    }

    if (!target) {
        return;
    }
    const ComponentSlotState slotAfter = read_component_slot_state(manager, componentIndex);
    const std::uint32_t observation = identity == 2
                                          ? g_componentRegisterIdentityTwoObserved.fetch_add(
                                                1U, std::memory_order_relaxed)
                                                + 1U
                                          : g_componentRegisterObserved.fetch_add(
                                                1U, std::memory_order_relaxed)
                                                + 1U;
    if (observation > 128U) {
        return;
    }
    const std::int32_t registeredAfter =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0xE93C : nullptr, -1);
    const std::int32_t activeAfter =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AF00 : nullptr, -1);
    std::array<char, core::log::kLineCapacity> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_component_register identity_n=%u caller_rva=0x%llX manager=%p identity=%d component_index=%d registered_before=%d registered_after=%d active_before=%d active_after=%d slot_valid_before=%u slot_valid_after=%u activity_before=%d activity_after=%d slot_ready_before=%u slot_ready_after=%u source_generation_before=%u source_generation_after=%u slot_generation_before=%u slot_generation_after=%u source_nonzero_before=%u source_nonzero_after=%u source_hash_before=0x%llX source_hash_after=0x%llX slot_nonzero_before=%u slot_nonzero_after=%u slot_hash_before=0x%llX slot_hash_after=0x%llX forced=%.*s",
        observation,
        static_cast<unsigned long long>(callerRva),
        static_cast<void*>(manager),
        identity,
        componentIndex,
        registeredBefore,
        registeredAfter,
        activeBefore,
        activeAfter,
        slotBefore.valid ? 1U : 0U,
        slotAfter.valid ? 1U : 0U,
        slotBefore.activity,
        slotAfter.activity,
        static_cast<unsigned int>(slotBefore.activitySlotReady),
        static_cast<unsigned int>(slotAfter.activitySlotReady),
        slotBefore.sourceGeneration,
        slotAfter.sourceGeneration,
        slotBefore.activitySlotGeneration,
        slotAfter.activitySlotGeneration,
        slotBefore.sourceDescriptor.nonzeroBytes,
        slotAfter.sourceDescriptor.nonzeroBytes,
        static_cast<unsigned long long>(slotBefore.sourceDescriptor.hash),
        static_cast<unsigned long long>(slotAfter.sourceDescriptor.hash),
        slotBefore.activitySlotDescriptor.nonzeroBytes,
        slotAfter.activitySlotDescriptor.nonzeroBytes,
        static_cast<unsigned long long>(slotBefore.activitySlotDescriptor.hash),
        static_cast<unsigned long long>(slotAfter.activitySlotDescriptor.hash),
        static_cast<int>(package.size()),
        package.data());
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
}

/**
 * Observes the native registration builder called by component_register. slotReady=false means
 * the caller is publishing a full descriptor into an empty per-activity slot; a successful return
 * causes component_register to mark that slot ready and copy the descriptor after this call.
 */
__declspec(noinline) bool __fastcall component_build(std::byte* manager,
                                                       std::int32_t componentIndex,
                                                       std::uint32_t generation,
                                                       std::uint32_t componentValue,
                                                       std::uint32_t mask,
                                                       const void* descriptor,
                                                       bool slotReady,
                                                       std::uint32_t descriptorHash,
                                                       const void* fingerprint) noexcept {
    auto* const returnAddress = reinterpret_cast<std::byte*>(_ReturnAddress());
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    const std::uintptr_t callerRva = image != nullptr && returnAddress >= image
                                         ? static_cast<std::uintptr_t>(returnAddress - image)
                                         : 0U;
    const std::int32_t identity =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1C7C0 : nullptr, -1);
    const std::int32_t mode =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AEF8 : nullptr, -1);
    const std::int32_t registration =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0xE93C : nullptr, -1);
    const bool tableValid = manager != nullptr && registration >= -11 && registration <= 64;
    const std::ptrdiff_t tableOffset = tableValid
                                           ? static_cast<std::ptrdiff_t>(registration + 12) * 0xB8
                                           : 0;
    const std::byte* const table = tableValid ? manager + tableOffset : nullptr;
    const std::int32_t componentCount =
        safe_read<std::int32_t>(table != nullptr ? table + 0xA8 : nullptr, -1);
    const std::int32_t mappedSlot = safe_read<std::int32_t>(
        table != nullptr && componentIndex >= 0 && componentIndex < 64
            ? table + 0xB0 + (static_cast<std::ptrdiff_t>(componentIndex) * 4)
            : nullptr,
        -1);
    const ComponentDescriptorSnapshot descriptorSnapshot =
        snapshot_component_descriptor(static_cast<const std::byte*>(descriptor));

    const ComponentBuild original = g_componentBuildOriginal.load(std::memory_order_acquire);
    const bool result = original != nullptr
                        && original(manager,
                                    componentIndex,
                                    generation,
                                    componentValue,
                                    mask,
                                    descriptor,
                                    slotReady,
                                    descriptorHash,
                                    fingerprint);

    std::string_view package{};
    if ((identity != 1 && identity != 2) || !opening_is_forced(package)
        || package != "mission_towerfall") {
        return result;
    }
    const std::uint32_t observation = identity == 2
                                          ? g_componentBuildIdentityTwoObserved.fetch_add(
                                                1U, std::memory_order_relaxed)
                                                + 1U
                                          : g_componentBuildObserved.fetch_add(
                                                1U, std::memory_order_relaxed)
                                                + 1U;
    if (observation > 128U) {
        return result;
    }
    std::array<char, core::log::kLineCapacity> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_component_build identity_n=%u result=%s caller_rva=0x%llX manager=%p identity=%d mode=%d component_index=%d registration=%d table_count=%d mapped_slot=%d generation=%u component_value=%u mask=0x%08X slot_ready=%u descriptor_hash=0x%08X descriptor_nonzero=%u descriptor_snapshot_hash=0x%llX descriptor=%p fingerprint=%p forced=%.*s",
        observation,
        result ? "accepted" : "rejected",
        static_cast<unsigned long long>(callerRva),
        static_cast<void*>(manager),
        identity,
        mode,
        componentIndex,
        registration,
        componentCount,
        mappedSlot,
        generation,
        componentValue,
        mask,
        slotReady ? 1U : 0U,
        descriptorHash,
        descriptorSnapshot.nonzeroBytes,
        static_cast<unsigned long long>(descriptorSnapshot.hash),
        descriptor,
        fingerprint,
        static_cast<int>(package.size()),
        package.data());
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
    return result;
}

/** Observes the branch used when the global component identifier is already active. */
__declspec(noinline) void __fastcall component_reuse(std::byte* manager,
                                                       std::int32_t componentIndex,
                                                       const void* identifier) noexcept {
    const std::int32_t identity =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1C7C0 : nullptr, -1);
    const std::int32_t registeredBefore =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0xE93C : nullptr, -1);
    const std::int32_t activeBefore =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AF00 : nullptr, -1);
    const ComponentReuse original = g_componentReuseOriginal.load(std::memory_order_acquire);
    if (original != nullptr) {
        original(manager, componentIndex, identifier);
    }

    std::string_view package{};
    if ((identity != 1 && identity != 2) || !opening_is_forced(package)
        || package != "mission_towerfall") {
        return;
    }
    const std::uint32_t observation = identity == 2
                                          ? g_componentReuseIdentityTwoObserved.fetch_add(
                                                1U, std::memory_order_relaxed)
                                                + 1U
                                          : g_componentReuseObserved.fetch_add(
                                                1U, std::memory_order_relaxed)
                                                + 1U;
    if (observation > (identity == 2 ? 128U : 256U)) {
        return;
    }
    const std::int32_t registeredAfter =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0xE93C : nullptr, -1);
    const std::int32_t activeAfter =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AF00 : nullptr, -1);
    const std::uint64_t componentIdentifier = safe_read<std::uint64_t>(identifier, 0U);
    std::array<char, core::log::kLineCapacity> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_component_reuse identity_n=%u manager=%p identity=%d component_index=%d identifier=0x%llX registered_before=%d registered_after=%d active_before=%d active_after=%d forced=%.*s",
        observation,
        static_cast<void*>(manager),
        identity,
        componentIndex,
        static_cast<unsigned long long>(componentIdentifier),
        registeredBefore,
        registeredAfter,
        activeBefore,
        activeAfter,
        static_cast<int>(package.size()),
        package.data());
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
}

/** Records the mapping gate immediately following the reuse-existing-component call. */
__declspec(noinline) std::int32_t __fastcall component_mapping(std::byte* context,
                                                                const void* identifier) noexcept {
    const ComponentMapping original = g_componentMappingOriginal.load(std::memory_order_acquire);
    const std::int32_t result = original != nullptr ? original(context, identifier) : -1;
    constexpr std::ptrdiff_t kContextToManagerIdentity = 0x1C7C0 - 0x860;
    const std::int32_t identity = safe_read<std::int32_t>(
        context != nullptr ? context + kContextToManagerIdentity : nullptr, -1);

    std::string_view package{};
    if ((identity != 1 && identity != 2) || !opening_is_forced(package)
        || package != "mission_towerfall") {
        return result;
    }
    const std::uint32_t observation = identity == 2
                                          ? g_componentMappingIdentityTwoObserved.fetch_add(
                                                1U, std::memory_order_relaxed)
                                                + 1U
                                          : g_componentMappingObserved.fetch_add(
                                                1U, std::memory_order_relaxed)
                                                + 1U;
    if (observation > (identity == 2 ? 128U : 256U)) {
        return result;
    }
    const std::int32_t registration = safe_read<std::int32_t>(
        context != nullptr && result >= 0 ? context + 0x3B7C + (result * 0x1A8) : nullptr,
        -1);
    const std::uint64_t mappedIdentifier = safe_read<std::uint64_t>(
        context != nullptr && result >= 0 ? context + 0x3B64 + (result * 0x1A8) : nullptr,
        0U);
    const std::uint64_t requestedIdentifier = safe_read<std::uint64_t>(identifier, 0U);
    std::array<char, core::log::kLineCapacity> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_component_mapping identity_n=%u result=%d context=%p identity=%d requested_id=0x%llX mapped_id=0x%llX registration=%d forced=%.*s",
        observation,
        result,
        static_cast<void*>(context),
        identity,
        static_cast<unsigned long long>(requestedIdentifier),
        static_cast<unsigned long long>(mappedIdentifier),
        registration,
        static_cast<int>(package.size()),
        package.data());
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
    return result;
}

/** Records the ordinary already-active component step selected instead of component dispatch. */
__declspec(noinline) std::int32_t __fastcall component_step(std::byte* manager,
                                                             std::int32_t componentIndex,
                                                             const void* identifier) noexcept {
    const std::int32_t identity =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1C7C0 : nullptr, -1);
    const std::int32_t registeredBefore =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0xE93C : nullptr, -1);
    const std::int32_t activeBefore =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AF00 : nullptr, -1);
    const ComponentStep original = g_componentStepOriginal.load(std::memory_order_acquire);
    const std::int32_t result =
        original != nullptr ? original(manager, componentIndex, identifier) : -1;

    std::string_view package{};
    if ((identity != 1 && identity != 2) || !opening_is_forced(package)
        || package != "mission_towerfall") {
        return result;
    }
    const std::uint32_t observation = identity == 2
                                          ? g_componentStepIdentityTwoObserved.fetch_add(
                                                1U, std::memory_order_relaxed)
                                                + 1U
                                          : g_componentStepObserved.fetch_add(
                                                1U, std::memory_order_relaxed)
                                                + 1U;
    if (observation > (identity == 2 ? 128U : 256U)) {
        return result;
    }

    const std::int32_t mode =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AEF8 : nullptr, -1);
    const std::int32_t selected =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x87C : nullptr, -1);
    const std::int32_t registeredAfter =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0xE93C : nullptr, -1);
    const std::int32_t activeAfter =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AF00 : nullptr, -1);
    const std::uint64_t componentIdentifier = safe_read<std::uint64_t>(identifier, 0U);
    std::array<char, core::log::kLineCapacity> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_component_step identity_n=%u result=%d manager=%p identity=%d component_index=%d identifier=0x%llX mode=%d selected=%d registered_before=%d registered_after=%d active_before=%d active_after=%d forced=%.*s",
        observation,
        result,
        static_cast<void*>(manager),
        identity,
        componentIndex,
        static_cast<unsigned long long>(componentIdentifier),
        mode,
        selected,
        registeredBefore,
        registeredAfter,
        activeBefore,
        activeAfter,
        static_cast<int>(package.size()),
        package.data());
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
    return result;
}

/** Observes the native per-component activation gate driven by the manager update loop. */
__declspec(noinline) bool __fastcall component_tick(std::byte* manager,
                                                      std::int32_t componentIndex) noexcept {
    const std::int32_t modeBefore =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AEF8 : nullptr, -1);
    const std::int32_t selectedBefore =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x87C : nullptr, -1);
    const std::int32_t registeredBefore =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0xE93C : nullptr, -1);
    const std::int32_t activeBefore =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AF00 : nullptr, -1);
    const ComponentTick original =
        g_componentTickOriginal.load(std::memory_order_acquire);
    const bool result = original != nullptr && original(manager, componentIndex);

    std::string_view package{};
    if (!opening_is_forced(package)) {
        return result;
    }
    const std::uint32_t observation =
        g_componentTickObserved.fetch_add(1, std::memory_order_relaxed) + 1U;
    if (observation > 512U) {
        return result;
    }
    const std::int32_t identity =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1C7C0 : nullptr, -1);
    const std::int32_t modeAfter =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AEF8 : nullptr, -1);
    const std::int32_t selectedAfter =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x87C : nullptr, -1);
    const std::int32_t registeredAfter =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0xE93C : nullptr, -1);
    const std::int32_t activeAfter =
        safe_read<std::int32_t>(manager != nullptr ? manager + 0x1AF00 : nullptr, -1);
    std::array<char, core::log::kLineCapacity> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_component_tick n=%u result=%s manager=%p identity=%d component_index=%d mode_before=%d mode_after=%d selected_before=%d selected_after=%d registered_before=%d registered_after=%d active_before=%d active_after=%d forced=%.*s",
        observation,
        result ? "accepted" : "rejected",
        static_cast<void*>(manager),
        identity,
        componentIndex,
        modeBefore,
        modeAfter,
        selectedBefore,
        selectedAfter,
        registeredBefore,
        registeredAfter,
        activeBefore,
        activeAfter,
        static_cast<int>(package.size()),
        package.data());
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
    return result;
}

void clear_originals() noexcept {
    g_transitionOriginal.store(nullptr, std::memory_order_release);
    g_slotOriginal.store(nullptr, std::memory_order_release);
    g_modeOriginal.store(nullptr, std::memory_order_release);
    g_authoritySlotMapOriginal.store(nullptr, std::memory_order_release);
    g_authorityApplyOriginal.store(nullptr, std::memory_order_release);
    g_authorityRefreshOriginal.store(nullptr, std::memory_order_release);
    g_authorityAnyAssignedGateOriginal.store(nullptr, std::memory_order_release);
    g_authorityAnyUnassignedGateOriginal.store(nullptr, std::memory_order_release);
    g_authoritySnapshotGateOriginal.store(nullptr, std::memory_order_release);
    g_assignmentProviderUpdateOriginal.store(nullptr, std::memory_order_release);
    g_assignmentProviderStateConsumerOriginal.store(nullptr, std::memory_order_release);
    g_matchmakingLanePolicyMaterializerOriginal.store(nullptr, std::memory_order_release);
    g_matchmakingLaneRebuildOriginal.store(nullptr, std::memory_order_release);
    g_assignmentWatcherUpdateOriginal.store(nullptr, std::memory_order_release);
    g_managerLookupOriginal.store(nullptr, std::memory_order_release);
    g_lifecycleEventOriginal.store(nullptr, std::memory_order_release);
    g_managerEnsureOriginal.store(nullptr, std::memory_order_release);
    g_identityRequestOriginal.store(nullptr, std::memory_order_release);
    g_identityEnableOriginal.store(nullptr, std::memory_order_release);
    g_managerActivateOriginal.store(nullptr, std::memory_order_release);
    g_hostHandoffApplyOriginal.store(nullptr, std::memory_order_release);
    g_memberRecordIndexOriginal.store(nullptr, std::memory_order_release);
    g_identityDescriptorCopyOriginal.store(nullptr, std::memory_order_release);
    g_sessionDescriptionStageOriginal.store(nullptr, std::memory_order_release);
    g_managedSessionMigrationOriginal.store(nullptr, std::memory_order_release);
    g_transitionDispatchOriginal.store(nullptr, std::memory_order_release);
    g_wireDispatchOriginal.store(nullptr, std::memory_order_release);
    g_activityClientUpdateOriginal.store(nullptr, std::memory_order_release);
    g_activityReceiverLookupOriginal.store(nullptr, std::memory_order_release);
    g_activityRosterApplyOriginal.store(nullptr, std::memory_order_release);
    g_activityPeerDispatchOriginal.store(nullptr, std::memory_order_release);
    g_activityReceiverActivateOriginal.store(nullptr, std::memory_order_release);
    g_activityReceiverBindOriginal.store(nullptr, std::memory_order_release);
    g_activityReceiverCreatorOriginal.store(nullptr, std::memory_order_release);
    g_managerUpdateLoopOriginal.store(nullptr, std::memory_order_release);
    g_managerUpdateGateOriginal.store(nullptr, std::memory_order_release);
    g_launchProducerOriginal.store(nullptr, std::memory_order_release);
    g_launchProducerGuardSetterOriginal.store(nullptr, std::memory_order_release);
    g_launchProducerReadyCallbackOriginal.store(nullptr, std::memory_order_release);
    g_activityEventDispatchLocalOriginal.store(nullptr, std::memory_order_release);
    g_activityEventDispatchPayloadOriginal.store(nullptr, std::memory_order_release);
    g_activityEvent46RegisteredProducerOriginal.store(nullptr, std::memory_order_release);
    g_activityEvent46ProducerResetOriginal.store(nullptr, std::memory_order_release);
    g_activityEvent46ProducerInitializeOriginal.store(nullptr, std::memory_order_release);
    g_activityEvent46ProducerAvailableOriginal.store(nullptr, std::memory_order_release);
    g_activityEvent46ProducerRequestOriginal.store(nullptr, std::memory_order_release);
    g_activityEvent46ProducerTickOriginal.store(nullptr, std::memory_order_release);
    g_activityEvent46RequestFlowOriginal.store(nullptr, std::memory_order_release);
    g_activityEvent46ActionDispatchOriginal.store(nullptr, std::memory_order_release);
    g_activityEvent46RequestPredicateOriginal.store(nullptr, std::memory_order_release);
    g_managerModeSetOriginal.store(nullptr, std::memory_order_release);
    g_routeDescriptorLookupOriginal.store(nullptr, std::memory_order_release);
    g_routeCommitOriginal.store(nullptr, std::memory_order_release);
    g_routeUpdateOriginal.store(nullptr, std::memory_order_release);
    g_defaultRouteZeroInitializerOriginal.store(nullptr, std::memory_order_release);
    g_defaultRouteReferencePredicateOriginal.store(nullptr, std::memory_order_release);
    g_defaultRoutePairPredicateOriginal.store(nullptr, std::memory_order_release);
    g_defaultRouteRequiredPredicateOriginal.store(nullptr, std::memory_order_release);
    g_defaultRouteCrossManagerPredicateOriginal.store(nullptr, std::memory_order_release);
    g_laneOneReadyScanOriginal.store(nullptr, std::memory_order_release);
    g_laneOneActiveCountOriginal.store(nullptr, std::memory_order_release);
    g_laneOneSelectionReadyOriginal.store(nullptr, std::memory_order_release);
    g_laneOneAuthoredInitializerOriginal.store(nullptr, std::memory_order_release);
    g_routeFlagUpdateOriginal.store(nullptr, std::memory_order_release);
    g_routeElapsedTimeOriginal.store(nullptr, std::memory_order_release);
    g_routeTimeoutConfigOriginal.store(nullptr, std::memory_order_release);
    g_routeAlternateUpdateOriginal.store(nullptr, std::memory_order_release);
    g_routeStateEvaluatorOriginal.store(nullptr, std::memory_order_release);
    g_routeParentDispatchOriginal.store(nullptr, std::memory_order_release);
    g_routeSelectorVariantLookupOriginal.store(nullptr, std::memory_order_release);
    g_routeStateArmOriginal.store(nullptr, std::memory_order_release);
    g_embeddedRouteStatePhaseSetOriginal.store(nullptr, std::memory_order_release);
    g_embeddedRouteStateInitializeOriginal.store(nullptr, std::memory_order_release);
    g_embeddedRouteStateResetOriginal.store(nullptr, std::memory_order_release);
    g_embeddedRouteLaneZeroDriverOriginal.store(nullptr, std::memory_order_release);
    g_embeddedRouteTransitionHandlerOriginal.store(nullptr, std::memory_order_release);
    g_embeddedRouteSessionActivityOriginal.store(nullptr, std::memory_order_release);
    g_embeddedRouteStateStatusOriginal.store(nullptr, std::memory_order_release);
    g_embeddedRouteContextSkipPredicateOriginal.store(nullptr, std::memory_order_release);
    g_embeddedRouteLaneAvailablePredicateOriginal.store(nullptr, std::memory_order_release);
    g_embeddedRouteAuthoredIdentityPredicateOriginal.store(nullptr, std::memory_order_release);
    g_embeddedRouteLocalIdentityPredicateOriginal.store(nullptr, std::memory_order_release);
    g_embeddedRouteIdentityProviderRecordLookupOriginal.store(nullptr,
                                                               std::memory_order_release);
    g_embeddedRouteIdentityProviderLookupOriginal.store(nullptr, std::memory_order_release);
    g_embeddedRouteObjectTransitionOriginal.store(nullptr, std::memory_order_release);
    g_routeAlternateSourceOriginal.store(nullptr, std::memory_order_release);
    g_routeAlternatePredicateOriginal.store(nullptr, std::memory_order_release);
    g_routeAlternateModeOriginal.store(nullptr, std::memory_order_release);
    g_currentSelectionPublishOriginal.store(nullptr, std::memory_order_release);
    g_upstreamSelectionPublishOriginal.store(nullptr, std::memory_order_release);
    g_upstreamSelectionUpdateOriginal.store(nullptr, std::memory_order_release);
    g_currentSelectionManager.store(nullptr, std::memory_order_release);
    g_currentSelectionOwner.store(nullptr, std::memory_order_release);
    g_routeSelectionIngestOriginal.store(nullptr, std::memory_order_release);
    g_authoredLaunchDispatchOriginal.store(nullptr, std::memory_order_release);
    g_launchCommandInitializeOriginal.store(nullptr, std::memory_order_release);
    g_launchCommandDispatchOriginal.store(nullptr, std::memory_order_release);
    g_routeModeZeroSetOriginal.store(nullptr, std::memory_order_release);
    g_routeModeOneSetOriginal.store(nullptr, std::memory_order_release);
    g_omegaRouteLifecycleQueryOriginal.store(nullptr, std::memory_order_release);
    g_omegaRouteLifecycleAccessorOriginal.store(nullptr, std::memory_order_release);
    g_routeStateQueryOriginal.store(nullptr, std::memory_order_release);
    g_routeStatePublishOriginal.store(nullptr, std::memory_order_release);
    g_managerLocalStartOriginal.store(nullptr, std::memory_order_release);
    g_managerAuthoredStartOriginal.store(nullptr, std::memory_order_release);
    g_managerSetupStageAOriginal.store(nullptr, std::memory_order_release);
    g_managerSetupStageBOriginal.store(nullptr, std::memory_order_release);
    g_managerSetupStageCOriginal.store(nullptr, std::memory_order_release);
    g_managerSetupStageDOriginal.store(nullptr, std::memory_order_release);
    g_componentDispatchOriginal.store(nullptr, std::memory_order_release);
    g_componentLookupOriginal.store(nullptr, std::memory_order_release);
    g_componentRegisterOriginal.store(nullptr, std::memory_order_release);
    g_componentReuseOriginal.store(nullptr, std::memory_order_release);
    g_componentMappingOriginal.store(nullptr, std::memory_order_release);
    g_componentBuildOriginal.store(nullptr, std::memory_order_release);
    g_componentStepOriginal.store(nullptr, std::memory_order_release);
    g_componentTickOriginal.store(nullptr, std::memory_order_release);
    g_postComponentSyncOriginal.store(nullptr, std::memory_order_release);
}

[[nodiscard]] std::array<legacy_owner_sentinel::HookOwnership, 116>
activity_script_upstream_hook_ownership() noexcept {
#define LEGACY_OWNER_HOOK(handle, original)                                            \
    legacy_owner_sentinel::HookOwnership {                                            \
        handle.attached, original.load(std::memory_order_acquire) != nullptr           \
    }
    // LEGACY_OWNER_SENTINEL_BEGIN(activity_script_upstream, 116)
    const std::array hooks{
        LEGACY_OWNER_HOOK(g_transitionHandle, g_transitionOriginal),
        LEGACY_OWNER_HOOK(g_slotHandle, g_slotOriginal),
        LEGACY_OWNER_HOOK(g_modeHandle, g_modeOriginal),
        LEGACY_OWNER_HOOK(g_authoritySlotMapHandle, g_authoritySlotMapOriginal),
        LEGACY_OWNER_HOOK(g_authorityApplyHandle, g_authorityApplyOriginal),
        LEGACY_OWNER_HOOK(g_authorityRefreshHandle, g_authorityRefreshOriginal),
        LEGACY_OWNER_HOOK(g_authorityAnyAssignedGateHandle,
                          g_authorityAnyAssignedGateOriginal),
        LEGACY_OWNER_HOOK(g_authorityAnyUnassignedGateHandle,
                          g_authorityAnyUnassignedGateOriginal),
        LEGACY_OWNER_HOOK(g_authoritySnapshotGateHandle, g_authoritySnapshotGateOriginal),
        LEGACY_OWNER_HOOK(g_managerLookupHandle, g_managerLookupOriginal),
        LEGACY_OWNER_HOOK(g_lifecycleEventHandle, g_lifecycleEventOriginal),
        LEGACY_OWNER_HOOK(g_managerEnsureHandle, g_managerEnsureOriginal),
        LEGACY_OWNER_HOOK(g_identityRequestHandle, g_identityRequestOriginal),
        LEGACY_OWNER_HOOK(g_identityEnableHandle, g_identityEnableOriginal),
        LEGACY_OWNER_HOOK(g_managerActivateHandle, g_managerActivateOriginal),
        LEGACY_OWNER_HOOK(g_hostHandoffApplyHandle, g_hostHandoffApplyOriginal),
        LEGACY_OWNER_HOOK(g_memberRecordIndexHandle, g_memberRecordIndexOriginal),
        LEGACY_OWNER_HOOK(g_identityDescriptorCopyHandle, g_identityDescriptorCopyOriginal),
        LEGACY_OWNER_HOOK(g_sessionDescriptionStageHandle, g_sessionDescriptionStageOriginal),
        LEGACY_OWNER_HOOK(g_managedSessionMigrationHandle, g_managedSessionMigrationOriginal),
        LEGACY_OWNER_HOOK(g_transitionDispatchHandle, g_transitionDispatchOriginal),
        LEGACY_OWNER_HOOK(g_wireDispatchHandle, g_wireDispatchOriginal),
        LEGACY_OWNER_HOOK(g_activityClientUpdateHandle, g_activityClientUpdateOriginal),
        LEGACY_OWNER_HOOK(g_activityReceiverLookupHandle, g_activityReceiverLookupOriginal),
        LEGACY_OWNER_HOOK(g_activityRosterApplyHandle, g_activityRosterApplyOriginal),
        LEGACY_OWNER_HOOK(g_activityPeerDispatchHandle, g_activityPeerDispatchOriginal),
        LEGACY_OWNER_HOOK(g_activityReceiverActivateHandle, g_activityReceiverActivateOriginal),
        LEGACY_OWNER_HOOK(g_activityReceiverBindHandle, g_activityReceiverBindOriginal),
        LEGACY_OWNER_HOOK(g_activityReceiverCreatorHandle, g_activityReceiverCreatorOriginal),
        LEGACY_OWNER_HOOK(g_managerUpdateLoopHandle, g_managerUpdateLoopOriginal),
        LEGACY_OWNER_HOOK(g_managerUpdateGateHandle, g_managerUpdateGateOriginal),
        LEGACY_OWNER_HOOK(g_launchProducerHandle, g_launchProducerOriginal),
        LEGACY_OWNER_HOOK(g_launchProducerGuardSetterHandle,
                          g_launchProducerGuardSetterOriginal),
        LEGACY_OWNER_HOOK(g_launchProducerReadyCallbackHandle,
                          g_launchProducerReadyCallbackOriginal),
        LEGACY_OWNER_HOOK(g_activityEventDispatchLocalHandle,
                          g_activityEventDispatchLocalOriginal),
        LEGACY_OWNER_HOOK(g_activityEventDispatchPayloadHandle,
                          g_activityEventDispatchPayloadOriginal),
        LEGACY_OWNER_HOOK(g_activityEvent46RegisteredProducerHandle,
                          g_activityEvent46RegisteredProducerOriginal),
        LEGACY_OWNER_HOOK(g_activityEvent46ProducerResetHandle,
                          g_activityEvent46ProducerResetOriginal),
        LEGACY_OWNER_HOOK(g_activityEvent46ProducerInitializeHandle,
                          g_activityEvent46ProducerInitializeOriginal),
        LEGACY_OWNER_HOOK(g_activityEvent46ProducerAvailableHandle,
                          g_activityEvent46ProducerAvailableOriginal),
        LEGACY_OWNER_HOOK(g_activityEvent46ProducerRequestHandle,
                          g_activityEvent46ProducerRequestOriginal),
        LEGACY_OWNER_HOOK(g_activityEvent46ProducerTickHandle,
                          g_activityEvent46ProducerTickOriginal),
        LEGACY_OWNER_HOOK(g_activityEvent46RequestFlowHandle,
                          g_activityEvent46RequestFlowOriginal),
        LEGACY_OWNER_HOOK(g_activityEvent46ActionDispatchHandle,
                          g_activityEvent46ActionDispatchOriginal),
        LEGACY_OWNER_HOOK(g_activityEvent46RequestPredicateHandle,
                          g_activityEvent46RequestPredicateOriginal),
        LEGACY_OWNER_HOOK(g_managerModeSetHandle, g_managerModeSetOriginal),
        LEGACY_OWNER_HOOK(g_routeDescriptorLookupHandle, g_routeDescriptorLookupOriginal),
        LEGACY_OWNER_HOOK(g_routeCommitHandle, g_routeCommitOriginal),
        LEGACY_OWNER_HOOK(g_routeUpdateHandle, g_routeUpdateOriginal),
        LEGACY_OWNER_HOOK(g_defaultRouteZeroInitializerHandle,
                          g_defaultRouteZeroInitializerOriginal),
        LEGACY_OWNER_HOOK(g_defaultRouteReferencePredicateHandle,
                          g_defaultRouteReferencePredicateOriginal),
        LEGACY_OWNER_HOOK(g_defaultRoutePairPredicateHandle,
                          g_defaultRoutePairPredicateOriginal),
        LEGACY_OWNER_HOOK(g_defaultRouteRequiredPredicateHandle,
                          g_defaultRouteRequiredPredicateOriginal),
        LEGACY_OWNER_HOOK(g_defaultRouteCrossManagerPredicateHandle,
                          g_defaultRouteCrossManagerPredicateOriginal),
        LEGACY_OWNER_HOOK(g_laneOneReadyScanHandle, g_laneOneReadyScanOriginal),
        LEGACY_OWNER_HOOK(g_laneOneActiveCountHandle, g_laneOneActiveCountOriginal),
        LEGACY_OWNER_HOOK(g_laneOneSelectionReadyHandle, g_laneOneSelectionReadyOriginal),
        LEGACY_OWNER_HOOK(g_laneOneAuthoredInitializerHandle,
                          g_laneOneAuthoredInitializerOriginal),
        LEGACY_OWNER_HOOK(g_routeFlagUpdateHandle, g_routeFlagUpdateOriginal),
        LEGACY_OWNER_HOOK(g_routeElapsedTimeHandle, g_routeElapsedTimeOriginal),
        LEGACY_OWNER_HOOK(g_routeTimeoutConfigHandle, g_routeTimeoutConfigOriginal),
        LEGACY_OWNER_HOOK(g_routeAlternateUpdateHandle, g_routeAlternateUpdateOriginal),
        LEGACY_OWNER_HOOK(g_routeStateEvaluatorHandle, g_routeStateEvaluatorOriginal),
        LEGACY_OWNER_HOOK(g_routeParentDispatchHandle, g_routeParentDispatchOriginal),
        LEGACY_OWNER_HOOK(g_routeSelectorVariantLookupHandle,
                          g_routeSelectorVariantLookupOriginal),
        LEGACY_OWNER_HOOK(g_assignmentProviderUpdateHandle,
                          g_assignmentProviderUpdateOriginal),
        LEGACY_OWNER_HOOK(g_assignmentProviderStateConsumerHandle,
                          g_assignmentProviderStateConsumerOriginal),
        LEGACY_OWNER_HOOK(g_matchmakingLanePolicyMaterializerHandle,
                          g_matchmakingLanePolicyMaterializerOriginal),
        LEGACY_OWNER_HOOK(g_matchmakingLaneRebuildHandle, g_matchmakingLaneRebuildOriginal),
        LEGACY_OWNER_HOOK(g_assignmentWatcherUpdateHandle, g_assignmentWatcherUpdateOriginal),
        LEGACY_OWNER_HOOK(g_routeStateArmHandle, g_routeStateArmOriginal),
        LEGACY_OWNER_HOOK(g_embeddedRouteStatePhaseSetHandle,
                          g_embeddedRouteStatePhaseSetOriginal),
        LEGACY_OWNER_HOOK(g_embeddedRouteStateInitializeHandle,
                          g_embeddedRouteStateInitializeOriginal),
        LEGACY_OWNER_HOOK(g_embeddedRouteStateResetHandle,
                          g_embeddedRouteStateResetOriginal),
        LEGACY_OWNER_HOOK(g_embeddedRouteLaneZeroDriverHandle,
                          g_embeddedRouteLaneZeroDriverOriginal),
        LEGACY_OWNER_HOOK(g_embeddedRouteTransitionHandlerHandle,
                          g_embeddedRouteTransitionHandlerOriginal),
        LEGACY_OWNER_HOOK(g_embeddedRouteSessionActivityHandle,
                          g_embeddedRouteSessionActivityOriginal),
        LEGACY_OWNER_HOOK(g_embeddedRouteStateStatusHandle,
                          g_embeddedRouteStateStatusOriginal),
        LEGACY_OWNER_HOOK(g_embeddedRouteContextSkipPredicateHandle,
                          g_embeddedRouteContextSkipPredicateOriginal),
        LEGACY_OWNER_HOOK(g_embeddedRouteLaneAvailablePredicateHandle,
                          g_embeddedRouteLaneAvailablePredicateOriginal),
        LEGACY_OWNER_HOOK(g_embeddedRouteAuthoredIdentityPredicateHandle,
                          g_embeddedRouteAuthoredIdentityPredicateOriginal),
        LEGACY_OWNER_HOOK(g_embeddedRouteLocalIdentityPredicateHandle,
                          g_embeddedRouteLocalIdentityPredicateOriginal),
        LEGACY_OWNER_HOOK(g_embeddedRouteIdentityProviderRecordLookupHandle,
                          g_embeddedRouteIdentityProviderRecordLookupOriginal),
        LEGACY_OWNER_HOOK(g_embeddedRouteIdentityProviderLookupHandle,
                          g_embeddedRouteIdentityProviderLookupOriginal),
        LEGACY_OWNER_HOOK(g_embeddedRouteObjectTransitionHandle,
                          g_embeddedRouteObjectTransitionOriginal),
        LEGACY_OWNER_HOOK(g_routeAlternateSourceHandle, g_routeAlternateSourceOriginal),
        LEGACY_OWNER_HOOK(g_routeAlternatePredicateHandle, g_routeAlternatePredicateOriginal),
        LEGACY_OWNER_HOOK(g_routeAlternateModeHandle, g_routeAlternateModeOriginal),
        LEGACY_OWNER_HOOK(g_currentSelectionPublishHandle, g_currentSelectionPublishOriginal),
        LEGACY_OWNER_HOOK(g_upstreamSelectionPublishHandle,
                          g_upstreamSelectionPublishOriginal),
        LEGACY_OWNER_HOOK(g_upstreamSelectionUpdateHandle, g_upstreamSelectionUpdateOriginal),
        LEGACY_OWNER_HOOK(g_routeSelectionIngestHandle, g_routeSelectionIngestOriginal),
        LEGACY_OWNER_HOOK(g_authoredLaunchDispatchHandle, g_authoredLaunchDispatchOriginal),
        LEGACY_OWNER_HOOK(g_launchCommandInitializeHandle,
                          g_launchCommandInitializeOriginal),
        LEGACY_OWNER_HOOK(g_launchCommandDispatchHandle, g_launchCommandDispatchOriginal),
        LEGACY_OWNER_HOOK(g_routeModeZeroSetHandle, g_routeModeZeroSetOriginal),
        LEGACY_OWNER_HOOK(g_routeModeOneSetHandle, g_routeModeOneSetOriginal),
        LEGACY_OWNER_HOOK(g_omegaRouteLifecycleQueryHandle,
                          g_omegaRouteLifecycleQueryOriginal),
        LEGACY_OWNER_HOOK(g_omegaRouteLifecycleAccessorHandle,
                          g_omegaRouteLifecycleAccessorOriginal),
        LEGACY_OWNER_HOOK(g_routeStateQueryHandle, g_routeStateQueryOriginal),
        LEGACY_OWNER_HOOK(g_routeStatePublishHandle, g_routeStatePublishOriginal),
        LEGACY_OWNER_HOOK(g_managerLocalStartHandle, g_managerLocalStartOriginal),
        LEGACY_OWNER_HOOK(g_managerAuthoredStartHandle, g_managerAuthoredStartOriginal),
        LEGACY_OWNER_HOOK(g_managerSetupStageAHandle, g_managerSetupStageAOriginal),
        LEGACY_OWNER_HOOK(g_managerSetupStageBHandle, g_managerSetupStageBOriginal),
        LEGACY_OWNER_HOOK(g_managerSetupStageCHandle, g_managerSetupStageCOriginal),
        LEGACY_OWNER_HOOK(g_managerSetupStageDHandle, g_managerSetupStageDOriginal),
        LEGACY_OWNER_HOOK(g_componentDispatchHandle, g_componentDispatchOriginal),
        LEGACY_OWNER_HOOK(g_componentLookupHandle, g_componentLookupOriginal),
        LEGACY_OWNER_HOOK(g_componentRegisterHandle, g_componentRegisterOriginal),
        LEGACY_OWNER_HOOK(g_componentReuseHandle, g_componentReuseOriginal),
        LEGACY_OWNER_HOOK(g_componentMappingHandle, g_componentMappingOriginal),
        LEGACY_OWNER_HOOK(g_componentBuildHandle, g_componentBuildOriginal),
        LEGACY_OWNER_HOOK(g_componentStepHandle, g_componentStepOriginal),
        LEGACY_OWNER_HOOK(g_componentTickHandle, g_componentTickOriginal),
        LEGACY_OWNER_HOOK(g_postComponentSyncHandle, g_postComponentSyncOriginal),
    };
    // LEGACY_OWNER_SENTINEL_END(activity_script_upstream)
#undef LEGACY_OWNER_HOOK
    static_assert(hooks.size() == 116U);
    return hooks;
}

} // namespace

void arm_omega_forest_route_trace() noexcept {
    LateInstallGuard lateInstall;
    if (!lateInstall.accepted()) {
        return;
    }
    std::string_view package{};
    if (!embedded_route_trace_is_forced(package) || package != "mission_scot"
        || g_omegaForestRouteTraceArmed.exchange(true, std::memory_order_acq_rel)) {
        return;
    }
    g_embeddedRouteObjectTransitionObserved.store(0U, std::memory_order_release);
    g_routeFlagUpdateObserved.store(0U, std::memory_order_release);
    g_routeElapsedTimeObserved.store(0U, std::memory_order_release);
    g_routeTimeoutConfigObserved.store(0U, std::memory_order_release);
    g_routeAlternateUpdateObserved.store(0U, std::memory_order_release);
    g_routeStateEvaluatorObserved.store(0U, std::memory_order_release);
    g_routeParentDispatchObserved.store(0U, std::memory_order_release);
    g_routeSelectorVariantLookupObserved.store(0U, std::memory_order_release);
    g_omegaRouteLifecycleQueryObserved.store(0U, std::memory_order_release);
    g_omegaRouteLifecycleQueryLast.store(INT32_MIN, std::memory_order_release);
    g_omegaRouteLifecycleAccessorObserved.store(0U, std::memory_order_release);
    for (auto& caller : g_omegaRouteLifecycleAccessorCallers) {
        caller.store(0U, std::memory_order_release);
    }
    g_routeAlternateDecisionObserved.store(0U, std::memory_order_release);
    g_routePhaseTimeoutPassedObserved.store(false, std::memory_order_release);
    g_routeSelectionTimeoutPassedObserved.store(false, std::memory_order_release);
    g_routeStateArmObserved.store(0U, std::memory_order_release);
    g_embeddedRouteStatePhaseSetObserved.store(0U, std::memory_order_release);
    g_embeddedRouteStateInitializeObserved.store(0U, std::memory_order_release);
    g_embeddedRouteStateResetObserved.store(0U, std::memory_order_release);
    g_embeddedRouteLaneZeroDriverObserved.store(0U, std::memory_order_release);
    g_embeddedRouteLaneZeroDriverOpeningObserved.store(0U, std::memory_order_release);
    g_embeddedRouteTransitionHandlerObserved.store(0U, std::memory_order_release);
    g_embeddedRouteSessionActivityObserved.store(0U, std::memory_order_release);
    g_embeddedRouteStateStatusObserved.store(0U, std::memory_order_release);
    g_embeddedRouteContextSkipPredicateObserved.store(0U, std::memory_order_release);
    g_embeddedRouteLaneAvailablePredicateObserved.store(0U, std::memory_order_release);
    g_embeddedRouteAuthoredIdentityPredicateObserved.store(0U, std::memory_order_release);
    g_embeddedRouteLocalIdentityPredicateObserved.store(0U, std::memory_order_release);
    g_embeddedRouteIdentityProviderRecordLookupObserved.store(0U, std::memory_order_release);
    g_embeddedRouteIdentityProviderLookupObserved.store(0U, std::memory_order_release);
    g_routeSelectionIngestObserved.store(0U, std::memory_order_release);
    g_authoredLaunchDispatchObserved.store(0U, std::memory_order_release);
    g_routeModeZeroSetObserved.store(0U, std::memory_order_release);
    g_routeModeOneSetObserved.store(0U, std::memory_order_release);
    g_routeStateQueryObserved.store(0U, std::memory_order_release);
    g_routeStateQueryLastSignature.store(~std::uint64_t{0}, std::memory_order_release);
    g_routeStatePublishObserved.store(0U, std::memory_order_release);
    g_omegaRouteState4SnapshotDumped.store(false, std::memory_order_release);
    std::byte* const owner = g_omegaRouteOwner.load(std::memory_order_acquire);
    if (owner != nullptr
        && !g_omegaRouteState4SnapshotDumped.exchange(true, std::memory_order_acq_rel)) {
        dump_omega_route_table(owner, "script_state_4");
    }
    core::log::write(
        core::log::Channel::client,
        core::log::Level::info,
        "ev=bootflow stage=omega_forest_route_trace result=armed trigger=script_state_4 mutation=observe_only");
}

void observe_activity_script_manager_table() noexcept {
    std::string_view package{};
    if (opening_is_forced(package)) {
        snapshot_manager_table(package);
    }
}

void notify_homecoming_authored_selection(const std::byte* descriptor) noexcept {
    LateInstallGuard lateInstall;
    if (!lateInstall.accepted()) {
        return;
    }
    if (descriptor != nullptr) {
        std::memcpy(g_launchProducerSelectionDescriptor.data(),
                    descriptor,
                    g_launchProducerSelectionDescriptor.size());
        g_launchProducerSelectionDescriptorReady.store(true, std::memory_order_release);
    }
    if (g_launchProducerSelectionReady.exchange(true, std::memory_order_acq_rel)) {
        return;
    }
    core::log::write(
        core::log::Channel::client,
        core::log::Level::info,
        "ev=bootflow stage=activity_script_authored_selection_snapshot result=captured source=282 destination=266 package=mission_towerfall");
}

/**
 * Starts an authored opening activity-script identity exactly once through the native lifecycle
 * gateway. The authored nodes are already linked by the descriptor at definition +0x57C; event 1
 * initializes that descriptor on the ready identity-1 manager. Its returned stage is retained in
 * the manager and advanced by the update-loop runner using the same contract as retail.
 *
 * The earliest caller runs on identity 0 before the mission's identity-1 manager is constructed,
 * so this remains a deferred, readiness-gated attempt. The authority post-apply callback is only
 * a fallback. Passing no descriptor merely ensures a blank manager and never starts the authored
 * mission simulation.
 */
void attempt_activity_script_bootstrap() noexcept {
    std::string_view package{};
    if (!opening_is_forced(package)
        || (package != "mission_scot" && package != "mission_towerfall")) {
        return;
    }
    const bool towerfall = package == "mission_towerfall";
    if ((towerfall && g_towerfallLifecycleStarted.load(std::memory_order_acquire))
        || (!towerfall && g_omegaLifecycleStarted.load(std::memory_order_acquire))) {
        return;
    }

    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    std::byte* definition = nullptr;
    if (image != nullptr) {
        const auto identityDefinition = reinterpret_cast<IdentityDefinition>(
            validated_target(kIdentityDefinitionRva, kIdentityDefinitionPrefix));
        __try {
            definition = identityDefinition != nullptr ? identityDefinition(1) : nullptr;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            definition = nullptr;
        }
    }

    constexpr std::ptrdiff_t kAuthoredDescriptorOffset = 0x57C;
    constexpr std::size_t kAuthoredDescriptorBytes = 0x80;
    std::byte* const descriptor =
        definition != nullptr ? definition + kAuthoredDescriptorOffset : nullptr;
    bool descriptorPresent = false;
    __try {
        for (std::size_t index = 0; descriptor != nullptr && index < kAuthoredDescriptorBytes;
             ++index) {
            descriptorPresent = descriptorPresent || descriptor[index] != std::byte{0};
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        descriptorPresent = false;
    }
    if (!descriptorPresent) {
        core::log::write(
            core::log::Channel::client,
            core::log::Level::warn,
            "ev=bootflow stage=activity_script_bootstrap_experiment result=skip identity=1 path=identity_request reason=descriptor");
        return;
    }

    std::byte* manager = nullptr;
    if (image != nullptr) {
        __try {
            std::byte** const table =
                reinterpret_cast<ManagerTable>(image + kManagerTableRva)();
            for (std::size_t index = 0; table != nullptr && index < 6U; ++index) {
                std::byte* const candidate = table[index];
                if (candidate != nullptr
                    && *reinterpret_cast<const std::int32_t*>(candidate + 0x1C7C0) == 1) {
                    manager = candidate;
                    break;
                }
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            manager = nullptr;
        }
    }
    if (manager == nullptr) {
        core::log::write(
            core::log::Channel::client,
            core::log::Level::info,
            "ev=bootflow stage=activity_script_bootstrap_experiment result=skip identity=1 path=lifecycle_event_1 reason=manager");
        return;
    }

    // Event 1 is rejected with result 2 while the identity manager is still in lifecycle state 0.
    // The manager reaches state 2 shortly after its initial component/setup pass, so defer the
    // one-shot request until that native readiness transition has completed.
    const std::int32_t lifecycleState =
        safe_read<std::int32_t>(manager + 0x1C820, -1);
    if (lifecycleState != 2) {
        return;
    }

    const auto managerEventBegin = reinterpret_cast<ManagerEventBegin>(
        validated_target(kManagerEventBeginRva, kManagerEventBeginPrefix));
    if (managerEventBegin == nullptr) {
        core::log::write(
            core::log::Channel::client,
            core::log::Level::warn,
            "ev=bootflow stage=activity_script_bootstrap_experiment result=skip identity=1 path=manager_event_begin_0 reason=target");
        return;
    }
    // A reset between orbit and the initial slice may re-arm the local once flag while the native
    // request itself remains pending. Preserve that request instead of publishing it twice.
    if (safe_read<std::uint8_t>(definition + 0x94D, 0U) != 0U) {
        __try {
            std::memcpy(manager + 0x1AF04, descriptor, kAuthoredDescriptorBytes);
            *reinterpret_cast<std::int32_t*>(manager + 0x1AF00) = 2;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return;
        }
        if (towerfall) {
            g_towerfallLifecycleStarted.store(true, std::memory_order_release);
        } else {
            g_omegaLifecycleManager.store(manager, std::memory_order_release);
            g_omegaLifecycleStarted.store(true, std::memory_order_release);
        }
        g_bootstrapRequestAttempted.store(true, std::memory_order_release);
        core::log::write(
            core::log::Channel::client,
            core::log::Level::info,
            "ev=bootflow stage=activity_script_bootstrap_experiment result=resume identity=1 path=lifecycle_event_2 reason=already_pending");
        return;
    }
    g_bootstrapRequestAttempted.store(true, std::memory_order_release);

    const std::uint64_t descriptor0 = safe_read<std::uint64_t>(descriptor, 0U);
    const std::uint64_t descriptor8 = safe_read<std::uint64_t>(descriptor + 8, 0U);
    std::array<char, 384> line{};
    int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_bootstrap_experiment result=attempt identity=1 path=lifecycle_event_1 manager=%p lifecycle_state=%d definition=%p descriptor=%p descriptor0=0x%llX descriptor8=0x%llX",
        static_cast<void*>(manager),
        lifecycleState,
        static_cast<void*>(definition),
        static_cast<void*>(descriptor),
        static_cast<unsigned long long>(descriptor0),
        static_cast<unsigned long long>(descriptor8));
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }

    // The manager update loop is already hot by the time the type-18 runtime publishes. Reset the
    // focused budgets here so the trace captures only the transition caused by this request.
    g_managerTableSnapshotObserved.store(0U, std::memory_order_release);
    g_managerUpdateLoopIdentityOneObserved.store(0U, std::memory_order_release);
    g_componentLookupIdentityOneObserved.store(0U, std::memory_order_release);
    g_componentTickObserved.store(0U, std::memory_order_release);
    g_lifecycleEventObserved.store(0U, std::memory_order_release);
    g_managerEnsureObserved.store(0U, std::memory_order_release);
    g_identityEnableObserved.store(0U, std::memory_order_release);
    g_sessionDescriptionStageObserved.store(0U, std::memory_order_release);
    g_managedSessionMigrationObserved.store(0U, std::memory_order_release);
    g_transitionDispatchObserved.store(0U, std::memory_order_release);
    g_wireDispatchObserved.store(0U, std::memory_order_release);

    // Manager event publication is intentionally a no-op until the native begin path initializes
    // +0x1C888/+0x1C890. Retail's general manager-start caller supplies event code 0 here.
    const std::uint64_t lifecycleBeginBefore =
        safe_read<std::uint64_t>(manager + 0x1C888, UINT64_MAX);
    managerEventBegin(manager, 0);
    const std::uint64_t lifecycleBeginAfter =
        safe_read<std::uint64_t>(manager + 0x1C888, UINT64_MAX);
    const std::uint64_t lifecycleUpdateAfter =
        safe_read<std::uint64_t>(manager + 0x1C890, 0U);
    length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_manager_event_begin_experiment result=returned identity=1 event=0 lifecycle_begin_before=0x%llX lifecycle_begin_after=0x%llX lifecycle_update_after=0x%llX",
        static_cast<unsigned long long>(lifecycleBeginBefore),
        static_cast<unsigned long long>(lifecycleBeginAfter),
        static_cast<unsigned long long>(lifecycleUpdateAfter));
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }
    if (lifecycleBeginAfter == UINT64_MAX) {
        return;
    }

    // Retail reaches identity_request only through lifecycle event 1. The lifecycle handler then
    // publishes manager event 10, which is the missing binding step; calling identity_request and
    // identity_enable directly left the definition permanently pending with no activity/context.
    std::int32_t lifecycleResult = -1;
    __try {
        // Preserve the authored payload in the exact bank consumed by the retail stage callers.
        std::memcpy(manager + 0x1AF04, descriptor, kAuthoredDescriptorBytes);
        *reinterpret_cast<std::int32_t*>(manager + 0x1AF00) = 1;
        lifecycleResult = lifecycle_event(manager, 1, manager + 0x1AF04);
        if (lifecycleResult >= 0 && lifecycleResult <= 5) {
            *reinterpret_cast<std::int32_t*>(manager + 0x1AF00) = lifecycleResult;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        lifecycleResult = -1;
    }
    if (lifecycleResult < 0 || lifecycleResult > 5) {
        core::log::write(
            core::log::Channel::client,
            core::log::Level::error,
            "ev=bootflow stage=activity_script_bootstrap_experiment result=fail identity=1 path=lifecycle_event_1 reason=return");
        return;
    }
    if (towerfall) {
        g_towerfallLifecycleStarted.store(true, std::memory_order_release);
    } else {
        g_omegaLifecycleManager.store(manager, std::memory_order_release);
        g_omegaLifecycleStarted.store(true, std::memory_order_release);
    }
    snapshot_manager_table(package);
    length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_bootstrap_experiment result=returned identity=1 path=lifecycle_event_1 lifecycle_result=%d definition_flags=0x%08X definition_enabled=%u definition_pending=%u definition_context=%p definition_activity=%d",
        lifecycleResult,
        safe_read<std::uint32_t>(definition + 0x4, 0U),
        static_cast<unsigned int>(safe_read<std::uint8_t>(definition + 0x94C, 0U)),
        static_cast<unsigned int>(safe_read<std::uint8_t>(definition + 0x94D, 0U)),
        safe_read<void*>(definition + 0x18, nullptr),
        safe_read<std::int32_t>(definition + 0x24, -1));
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }

    snapshot_manager_table(package);
}

void notify_activity_script_setup_complete() noexcept {
    LateInstallGuard lateInstall;
    if (!lateInstall.accepted()) {
        return;
    }
    std::string_view package{};
    if (!opening_is_forced(package) || package != "mission_towerfall") {
        return;
    }

    if (g_activitySetupComplete.exchange(true, std::memory_order_acq_rel)) {
        return;
    }
    core::log::write(
        core::log::Channel::client,
        core::log::Level::info,
        "ev=bootflow stage=activity_script_event22_gate result=armed phase=prologue_intro_loading mode=qualification_only");
}

void notify_activity_script_client_joined(std::byte* activityClient) noexcept {
    LateInstallGuard lateInstall;
    if (!lateInstall.accepted()) {
        return;
    }
    std::string_view package{};
    if (activityClient == nullptr || !opening_is_forced(package)
        || package != "mission_towerfall") {
        return;
    }

    g_joinedActivityClient.store(activityClient, std::memory_order_release);
    std::array<char, 384> line{};
    const int length = std::snprintf(
        line.data(),
        line.size(),
        "ev=bootflow stage=activity_script_client_capture result=verified source=apply_join_result activity_client=%p client_state=%d context=%p membership_mask=0x%08X setup=%d world=%d",
        static_cast<void*>(activityClient),
        safe_read<std::int32_t>(activityClient + 0x1D18, -1),
        safe_read<void*>(activityClient + 0x18, nullptr),
        safe_read<std::uint32_t>(activityClient + 0x20A8, 0U),
        g_activitySetupComplete.load(std::memory_order_acquire) ? 1 : 0,
        g_activityWorldStarted.load(std::memory_order_acquire) ? 1 : 0);
    if (length > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {line.data(), static_cast<std::size_t>(length)});
    }

    if (g_activityWorldStarted.load(std::memory_order_acquire)) {
        qualify_event22_candidate(package);
    }
}

void notify_activity_script_world_started() noexcept {
    LateInstallGuard lateInstall;
    if (!lateInstall.accepted()) {
        return;
    }
    std::string_view package{};
    if (!opening_is_forced(package) || package != "mission_towerfall"
        || !g_bootstrapRequestAttempted.load(std::memory_order_acquire)) {
        return;
    }

    if (g_activityWorldStarted.exchange(true, std::memory_order_acq_rel)) {
        return;
    }
    std::array<char, 320> precheckLine{};
    const int precheckLength = std::snprintf(
        precheckLine.data(),
        precheckLine.size(),
        "ev=bootflow stage=activity_script_event22_precheck bootstrap=%d setup=%d world=%d activity_client=%p pump_calls=%u forced=%.*s",
        g_bootstrapRequestAttempted.load(std::memory_order_acquire) ? 1 : 0,
        g_activitySetupComplete.load(std::memory_order_acquire) ? 1 : 0,
        g_activityWorldStarted.load(std::memory_order_acquire) ? 1 : 0,
        static_cast<void*>(g_activityClientCandidate.load(std::memory_order_acquire)),
        g_activityClientPumpObserved.load(std::memory_order_acquire),
        static_cast<int>(package.size()),
        package.data());
    if (precheckLength > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {precheckLine.data(), static_cast<std::size_t>(precheckLength)});
    }
    snapshot_activity_receiver_registry(package);
    qualify_event22_candidate(package);
    core::log::write(
        core::log::Channel::client,
        core::log::Level::info,
        "ev=bootflow stage=activity_script_event22_gate result=armed phase=in_world mode=qualification_only");
}

void reset_activity_script_bootstrap() noexcept {
    g_bootstrapRequestAttempted.store(false, std::memory_order_release);
    g_towerfallLifecycleStarted.store(false, std::memory_order_release);
    g_componentDispatchAttempted.store(false, std::memory_order_release);
    // The initial-slice transition occurs after prologue setup and still belongs to the same
    // activity client. Preserve both pieces of evidence so the in-world event-22 gate can
    // qualify the native client after physics join. DLL install/uninstall remains responsible
    // for clearing them between processes.
    g_activityWorldStarted.store(false, std::memory_order_release);
    g_activityClientUpdateObserved.store(0U, std::memory_order_release);
    g_activityReceiverSnapshotAttempted.store(false, std::memory_order_release);
}

bool activity_script_upstream_probe_attached() noexcept {
    return legacy_owner_sentinel::any_handle_attached(
        activity_script_upstream_hook_ownership());
}

bool activity_script_upstream_probe_has_ownership() noexcept {
    // LEGACY_OWNER_CLAIMS_BEGIN(activity_script_upstream, 4)
    const std::array claims{
        g_currentSelectionPublishInstallAttempted.load(std::memory_order_acquire),
        g_embeddedRouteIdentityProviderLookupInstallAttempted.load(
            std::memory_order_acquire),
        g_currentSelectionManager.load(std::memory_order_acquire) != nullptr,
        g_currentSelectionOwner.load(std::memory_order_acquire) != nullptr,
    };
    // LEGACY_OWNER_CLAIMS_END(activity_script_upstream)
    return legacy_owner_sentinel::has_ownership(
        activity_script_upstream_hook_ownership(), claims);
}

bool install_activity_script_upstream_probe() noexcept {
    LateInstallGuard lateInstall;
    if (!lateInstall.accepted()) {
        return false;
    }
    if (g_transitionHandle.attached && g_slotHandle.attached && g_modeHandle.attached
        && g_authoritySlotMapHandle.attached && g_authorityApplyHandle.attached
        && g_authorityRefreshHandle.attached
        && g_authorityAnyAssignedGateHandle.attached
        && g_authorityAnyUnassignedGateHandle.attached
        && g_authoritySnapshotGateHandle.attached
        && g_managerLookupHandle.attached
        && g_lifecycleEventHandle.attached
        && g_managerEnsureHandle.attached && g_identityRequestHandle.attached
        && g_identityEnableHandle.attached
        && g_managerActivateHandle.attached
        && g_hostHandoffApplyHandle.attached
        && g_memberRecordIndexHandle.attached
        && g_identityDescriptorCopyHandle.attached
        && g_sessionDescriptionStageHandle.attached
        && g_managedSessionMigrationHandle.attached
        && g_transitionDispatchHandle.attached && g_wireDispatchHandle.attached
        && g_activityClientUpdateHandle.attached
        && g_activityReceiverLookupHandle.attached
        && g_activityRosterApplyHandle.attached
        && g_activityPeerDispatchHandle.attached
        && g_activityReceiverActivateHandle.attached
        && g_activityReceiverBindHandle.attached
         && g_activityReceiverCreatorHandle.attached
         && g_managerUpdateLoopHandle.attached
         && g_managerUpdateGateHandle.attached
         && g_launchProducerHandle.attached
         && g_launchProducerGuardSetterHandle.attached
         && g_launchProducerReadyCallbackHandle.attached
         && g_activityEventDispatchLocalHandle.attached
         && g_activityEventDispatchPayloadHandle.attached
         && g_activityEvent46RegisteredProducerHandle.attached
         && g_activityEvent46ProducerResetHandle.attached
         && g_activityEvent46ProducerInitializeHandle.attached
         && g_activityEvent46ProducerAvailableHandle.attached
         && g_activityEvent46ProducerRequestHandle.attached
         && g_activityEvent46ProducerTickHandle.attached
         && g_activityEvent46RequestFlowHandle.attached
         && g_activityEvent46ActionDispatchHandle.attached
         && g_activityEvent46RequestPredicateHandle.attached
         && g_managerModeSetHandle.attached
         && g_routeDescriptorLookupHandle.attached
         && g_defaultRouteZeroInitializerHandle.attached
         && g_defaultRouteReferencePredicateHandle.attached
         && g_defaultRoutePairPredicateHandle.attached
         && g_defaultRouteRequiredPredicateHandle.attached
         && g_defaultRouteCrossManagerPredicateHandle.attached
         && g_laneOneReadyScanHandle.attached
         && g_laneOneActiveCountHandle.attached
         && g_laneOneSelectionReadyHandle.attached
         && g_laneOneAuthoredInitializerHandle.attached
         && g_routeFlagUpdateHandle.attached
         && g_routeElapsedTimeHandle.attached
         && g_routeTimeoutConfigHandle.attached
         && g_routeAlternateUpdateHandle.attached
         && g_routeStateEvaluatorHandle.attached
         && g_routeParentDispatchHandle.attached
         && g_routeSelectorVariantLookupHandle.attached
         && g_assignmentProviderUpdateHandle.attached
         && g_assignmentProviderStateConsumerHandle.attached
         && g_assignmentWatcherUpdateHandle.attached
         && g_routeStateArmHandle.attached
         && g_embeddedRouteStatePhaseSetHandle.attached
         && g_embeddedRouteStateInitializeHandle.attached
         && g_embeddedRouteStateResetHandle.attached
         && g_embeddedRouteObjectTransitionHandle.attached
         && g_embeddedRouteLaneZeroDriverHandle.attached
         && g_embeddedRouteTransitionHandlerHandle.attached
         && g_embeddedRouteSessionActivityHandle.attached
         && g_embeddedRouteStateStatusHandle.attached
         && g_embeddedRouteContextSkipPredicateHandle.attached
         && g_embeddedRouteLaneAvailablePredicateHandle.attached
         && g_embeddedRouteAuthoredIdentityPredicateHandle.attached
         && g_embeddedRouteLocalIdentityPredicateHandle.attached
         && g_embeddedRouteIdentityProviderRecordLookupHandle.attached
         && g_routeAlternateSourceHandle.attached
         && g_routeAlternatePredicateHandle.attached
         && g_routeAlternateModeHandle.attached
         && g_routeSelectionIngestHandle.attached
         && g_authoredLaunchDispatchHandle.attached
         && g_launchCommandInitializeHandle.attached
         && g_launchCommandDispatchHandle.attached
         && g_routeModeZeroSetHandle.attached && g_routeModeOneSetHandle.attached
         && g_omegaRouteLifecycleQueryHandle.attached
         && g_omegaRouteLifecycleAccessorHandle.attached
         && g_routeStateQueryHandle.attached && g_routeStatePublishHandle.attached
         && g_managerLocalStartHandle.attached && g_managerAuthoredStartHandle.attached
         && g_managerSetupStageAHandle.attached && g_managerSetupStageBHandle.attached
         && g_managerSetupStageCHandle.attached && g_managerSetupStageDHandle.attached
         && g_componentDispatchHandle.attached
         && g_componentLookupHandle.attached
         && g_componentRegisterHandle.attached
         && g_componentReuseHandle.attached
         && g_componentMappingHandle.attached
         && g_componentBuildHandle.attached
         && g_componentStepHandle.attached
         && g_componentTickHandle.attached
         && g_postComponentSyncHandle.attached) {
        return true;
    }
    auto* const image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    std::byte* const transitionTarget =
        validated_target(kTransitionUpdateRva, kTransitionUpdatePrefix);
    std::byte* const slotTarget = validated_target(kSlotUpdateRva, kSlotUpdatePrefix);
    std::byte* const modeTarget = validated_target(kModeUpdateRva, kModeUpdatePrefix);
    std::byte* const authoritySlotMapTarget =
        validated_target(kAuthoritySlotMapRva, kAuthoritySlotMapPrefix);
    std::byte* const authorityApplyTarget =
        validated_target(kAuthorityApplyRva, kAuthorityApplyPrefix);
    std::byte* const authorityRefreshTarget =
        validated_target(kAuthorityRefreshRva, kAuthorityRefreshPrefix);
    std::byte* const authorityAnyAssignedGateTarget =
        validated_target(kAuthorityAnyAssignedGateRva, kAuthorityAnyAssignedGatePrefix);
    std::byte* const authorityAnyUnassignedGateTarget =
        validated_target(kAuthorityAnyUnassignedGateRva, kAuthorityAnyUnassignedGatePrefix);
    std::byte* const authoritySnapshotGateTarget =
        validated_target(kAuthoritySnapshotGateRva, kAuthoritySnapshotGatePrefix);
    std::byte* const assignmentProviderUpdateTarget =
        validated_target(kAssignmentProviderUpdateRva, kAssignmentProviderUpdatePrefix);
    std::byte* const assignmentProviderStateConsumerTarget = validated_target(
        kAssignmentProviderStateConsumerRva,
        kAssignmentProviderStateConsumerPrefix);
    std::byte* const matchmakingLanePolicyMaterializerTarget = validated_target(
        kMatchmakingLanePolicyMaterializerRva,
        kMatchmakingLanePolicyMaterializerPrefix);
    std::byte* const matchmakingLaneRebuildTarget =
        validated_target(kMatchmakingLaneRebuildRva, kMatchmakingLaneRebuildPrefix);
    std::byte* const assignmentWatcherUpdateTarget =
        validated_target(kAssignmentWatcherUpdateRva, kAssignmentWatcherUpdatePrefix);
    std::byte* const managerLookupTarget =
        validated_target(kManagerLookupRva, kManagerLookupPrefix);
    std::byte* const lifecycleEventTarget =
        validated_target(kLifecycleEventRva, kLifecycleEventPrefix);
    std::byte* const managerEnsureTarget =
        validated_target(kManagerEnsureRva, kManagerEnsurePrefix);
    std::byte* const identityRequestTarget =
        validated_target(kIdentityRequestRva, kIdentityRequestPrefix);
    std::byte* const identityEnableTarget =
        validated_target(kIdentityEnableRva, kIdentityEnablePrefix);
    std::byte* const managerActivateTarget =
        validated_target(kManagerActivateRva, kManagerActivatePrefix);
    std::byte* const hostHandoffApplyTarget =
        validated_target(kHostHandoffApplyRva, kHostHandoffApplyPrefix);
    std::byte* const memberRecordIndexTarget =
        validated_target(kMemberRecordIndexRva, kMemberRecordIndexPrefix);
    std::byte* const identityDescriptorCopyTarget =
        validated_target(kIdentityDescriptorCopyRva, kIdentityDescriptorCopyPrefix);
    std::byte* const sessionDescriptionStageTarget =
        validated_target(kSessionDescriptionStageRva, kSessionDescriptionStagePrefix);
    std::byte* const managedSessionMigrationTarget =
        validated_target(kManagedSessionMigrationRva, kManagedSessionMigrationPrefix);
    std::byte* const transitionDispatchTarget =
        validated_target(kTransitionDispatchRva, kTransitionDispatchPrefix);
    std::byte* const wireDispatchTarget = validated_target(kWireDispatchRva, kWireDispatchPrefix);
    std::byte* const activityClientUpdateTarget =
        validated_target(kActivityClientUpdateRva, kActivityClientUpdatePrefix);
    std::byte* const activityReceiverLookupTarget =
        validated_target(kActivityReceiverLookupRva, kActivityReceiverLookupPrefix);
    std::byte* const activityRosterApplyTarget =
        validated_target(kActivityRosterApplyRva, kActivityRosterApplyPrefix);
    std::byte* const activityPeerDispatchTarget =
        validated_target(kActivityPeerDispatchRva, kActivityPeerDispatchPrefix);
    std::byte* const activityReceiverActivateTarget =
        validated_target(kActivityReceiverActivateRva, kActivityReceiverActivatePrefix);
    std::byte* const activityReceiverBindTarget =
        validated_target(kActivityReceiverBindRva, kActivityReceiverBindPrefix);
    std::byte* const activityReceiverCreatorTarget =
        validated_target(kActivityReceiverCreatorRva, kActivityReceiverCreatorPrefix);
    std::byte* const managerUpdateLoopTarget =
        validated_target(kManagerUpdateLoopRva, kManagerUpdateLoopPrefix);
    std::byte* const managerUpdateGateTarget =
        image != nullptr && managerUpdateLoopTarget != nullptr ? image + kManagerUpdateGateRva
                                                               : nullptr;
    std::byte* const launchProducerTarget =
        validated_target(kLaunchProducerRva, kLaunchProducerPrefix);
    std::byte* const launchProducerGuardSetterTarget =
        validated_target(kLaunchProducerGuardSetterRva, kLaunchProducerGuardSetterPrefix);
    std::byte* const launchProducerReadyCallbackTarget =
        image != nullptr ? image + kLaunchProducerReadyCallbackRva : nullptr;
    std::byte* const activityEventDispatchLocalTarget =
        image != nullptr ? image + kActivityEventDispatchLocalRva : nullptr;
    std::byte* const activityEventDispatchPayloadTarget =
        image != nullptr ? image + kActivityEventDispatchPayloadRva : nullptr;
    std::byte* const activityEvent46RegisteredProducerTarget =
        image != nullptr ? image + kActivityEvent46RegisteredProducerRva : nullptr;
    std::byte* const activityEvent46ProducerResetTarget =
        image != nullptr ? image + kActivityEvent46ProducerResetRva : nullptr;
    std::byte* const activityEvent46ProducerInitializeTarget =
        image != nullptr ? image + kActivityEvent46ProducerInitializeRva : nullptr;
    std::byte* const activityEvent46ProducerAvailableTarget =
        image != nullptr ? image + kActivityEvent46ProducerAvailableRva : nullptr;
    std::byte* const activityEvent46ProducerRequestTarget =
        image != nullptr ? image + kActivityEvent46ProducerRequestRva : nullptr;
    std::byte* const activityEvent46ProducerTickTarget =
        image != nullptr ? image + kActivityEvent46ProducerTickRva : nullptr;
    std::byte* const activityEvent46RequestFlowTarget =
        image != nullptr ? image + kActivityEvent46RequestFlowRva : nullptr;
    std::byte* const activityEvent46ActionDispatchTarget =
        image != nullptr ? image + kActivityEvent46ActionDispatchRva : nullptr;
    std::byte* const activityEvent46RequestPredicateTarget =
        image != nullptr ? image + kActivityEvent46RequestPredicateRva : nullptr;
    std::byte* const managerModeSetTarget =
        validated_target(kManagerModeSetRva, kManagerModeSetPrefix);
    std::byte* const routeDescriptorLookupTarget =
        validated_target(kRouteDescriptorLookupRva, kRouteDescriptorLookupPrefix);
    std::byte* const routeCommitTarget =
        validated_target(kRouteCommitRva, kRouteCommitPrefix);
    // The pinned executable's route-commit function directly calls this target at
    // 0x175200F and 0x1752023. Its runtime bytes are dumped below for verification.
    std::byte* const routeUpdateTarget =
        image != nullptr && routeCommitTarget != nullptr ? image + kRouteUpdateRva : nullptr;
    std::byte* const defaultRouteZeroInitializerTarget = validated_target(
        kDefaultRouteZeroInitializerRva,
        kDefaultRouteZeroInitializerPrefix);
    // These targets are recovered from the already runtime-validated 0xEE8390 caller.
    // Their on-disk bytes are encrypted, so installation follows the same direct-RVA
    // pattern as other call-target probes in this file.
    std::byte* const defaultRouteReferencePredicateTarget =
        image != nullptr && defaultRouteZeroInitializerTarget != nullptr
            ? image + kDefaultRouteReferencePredicateRva
            : nullptr;
    std::byte* const defaultRoutePairPredicateTarget =
        image != nullptr && defaultRouteZeroInitializerTarget != nullptr
            ? image + kDefaultRoutePairPredicateRva
            : nullptr;
    std::byte* const defaultRouteRequiredPredicateTarget =
        image != nullptr && defaultRouteZeroInitializerTarget != nullptr
            ? image + kDefaultRouteRequiredPredicateRva
            : nullptr;
    std::byte* const defaultRouteCrossManagerPredicateTarget =
        image != nullptr && defaultRouteZeroInitializerTarget != nullptr
            ? image + kDefaultRouteCrossManagerPredicateRva
            : nullptr;
    std::byte* const laneOneReadyScanTarget =
        validated_target(kLaneOneReadyScanRva, kLaneOneReadyScanPrefix);
    std::byte* const laneOneActiveCountTarget =
        validated_target(kLaneOneActiveCountRva, kLaneOneActiveCountPrefix);
    std::byte* const laneOneSelectionReadyTarget =
        validated_target(kLaneOneSelectionReadyRva, kLaneOneSelectionReadyPrefix);
    std::byte* const laneOneAuthoredInitializerTarget = validated_target(
        kLaneOneAuthoredInitializerRva,
        kLaneOneAuthoredInitializerPrefix);
    std::byte* const routeFlagUpdateTarget =
        validated_target(kRouteFlagUpdateRva, kRouteFlagUpdatePrefix);
    std::byte* const routeElapsedTimeTarget =
        validated_target(kRouteElapsedTimeRva, kRouteElapsedTimePrefix);
    std::byte* const routeTimeoutConfigTarget =
        validated_target(kRouteTimeoutConfigRva, kRouteTimeoutConfigPrefix);
    std::byte* const routeAlternateUpdateTarget =
        validated_target(kRouteAlternateUpdateRva, kRouteAlternateUpdatePrefix);
    std::byte* const routeStateEvaluatorTarget =
        validated_target(kRouteStateEvaluatorRva, kRouteStateEvaluatorPrefix);
    std::byte* const routeParentDispatchTarget =
        validated_target(kRouteParentDispatchRva, kRouteParentDispatchPrefix);
    std::byte* const routeSelectorContextAccessorTarget = validated_target(
        kRouteSelectorContextAccessorRva,
        kRouteSelectorContextAccessorPrefix);
    std::byte* const routeStateArmTarget =
        validated_target(kRouteStateArmRva, kRouteStateArmPrefix);
    std::byte* const embeddedRouteStatePhaseSetTarget = validated_target(
        kEmbeddedRouteStatePhaseSetRva,
        kEmbeddedRouteStatePhaseSetPrefix);
    std::byte* const embeddedRouteStateInitializeTarget = validated_target(
        kEmbeddedRouteStateInitializeRva,
        kEmbeddedRouteStateInitializePrefix);
    std::byte* const embeddedRouteStateResetTarget = validated_target(
        kEmbeddedRouteStateResetRva,
        kEmbeddedRouteStateResetPrefix);
    std::byte* const embeddedRouteObjectTransitionTarget = validated_target(
        kEmbeddedRouteObjectTransitionRva,
        kEmbeddedRouteObjectTransitionPrefix);
    std::byte* const embeddedRouteLaneZeroDriverTarget = validated_target(
        kEmbeddedRouteLaneZeroDriverRva,
        kEmbeddedRouteLaneZeroDriverPrefix);
    std::byte* const embeddedRouteTransitionHandlerTarget = validated_target(
        kEmbeddedRouteTransitionHandlerRva,
        kEmbeddedRouteTransitionHandlerPrefix);
    std::byte* const embeddedRouteSessionActivityTarget = validated_target(
        kLaunchProducerSessionActivityRva,
        kEmbeddedRouteSessionActivityPrefix);
    std::byte* const embeddedRouteStateStatusTarget = validated_target(
        kEmbeddedRouteStateStatusRva,
        kEmbeddedRouteStateStatusPrefix);
    std::byte* const embeddedRouteContextSkipPredicateTarget = validated_target(
        kEmbeddedRouteContextSkipPredicateRva,
        kEmbeddedRouteContextSkipPredicatePrefix);
    std::byte* const embeddedRouteLaneAvailablePredicateTarget = validated_target(
        kEmbeddedRouteLaneAvailablePredicateRva,
        kEmbeddedRouteLaneAvailablePredicatePrefix);
    std::byte* const embeddedRouteAuthoredIdentityPredicateTarget = validated_target(
        kEmbeddedRouteAuthoredIdentityPredicateRva,
        kEmbeddedRouteAuthoredIdentityPredicatePrefix);
    std::byte* const embeddedRouteLocalIdentityPredicateTarget = validated_target(
        kEmbeddedRouteLocalIdentityPredicateRva,
        kEmbeddedRouteLocalIdentityPredicatePrefix);
    std::byte* const embeddedRouteIdentityProviderRecordLookupTarget = validated_target(
        kEmbeddedRouteIdentityProviderRecordLookupRva,
        kEmbeddedRouteIdentityProviderRecordLookupPrefix);
    std::byte* routeSelectorContext = nullptr;
    std::byte* routeSelectorVtable = nullptr;
    std::byte* routeSelectorVariantLookupTarget = nullptr;
    std::byte* routeSelectorVariantModeTarget = nullptr;
    if (routeSelectorContextAccessorTarget != nullptr) {
        const auto accessor = reinterpret_cast<RouteSelectorContextAccessor>(
            routeSelectorContextAccessorTarget);
        routeSelectorContext = accessor();
        routeSelectorVtable = safe_read<std::byte*>(routeSelectorContext, nullptr);
        routeSelectorVariantLookupTarget =
            safe_read<std::byte*>(routeSelectorVtable != nullptr ? routeSelectorVtable + 0x88
                                                                 : nullptr,
                                  nullptr);
        routeSelectorVariantModeTarget =
            safe_read<std::byte*>(routeSelectorVtable != nullptr ? routeSelectorVtable + 0x90
                                                                 : nullptr,
                                  nullptr);
    }
    std::byte* const routeAlternateSourceTarget =
        validated_target(kRouteAlternateSourceRva, kRouteAlternateSourcePrefix);
    std::byte* const routeAlternatePredicateTarget =
        validated_target(kRouteAlternatePredicateRva, kRouteAlternatePredicatePrefix);
    std::byte* const routeAlternateModeTarget =
        validated_target(kRouteAlternateModeRva, kRouteAlternateModePrefix);
    std::byte* const routeSelectionIngestTarget =
        validated_target(kRouteSelectionIngestRva, kRouteSelectionIngestPrefix);
    std::byte* const authoredLaunchDispatchTarget =
        validated_target(kAuthoredLaunchDispatchRva, kAuthoredLaunchDispatchPrefix);
    std::byte* const launchCommandInitializeTarget =
        validated_target(kLaunchCommandInitializeRva, kLaunchCommandInitializePrefix);
    std::byte* const launchCommandDispatchTarget =
        validated_target(kLaunchCommandDispatchRva, kLaunchCommandDispatchPrefix);
    std::byte* const routeModeZeroSetTarget =
        validated_target(kRouteModeZeroSetRva, kRouteModeSetPrefix);
    std::byte* const routeModeOneSetTarget =
        validated_target(kRouteModeOneSetRva, kRouteModeSetPrefix);
    std::byte* const routeStateQueryTarget =
        validated_target(kRouteStateQueryRva, kRouteStateQueryPrefix);
    std::byte* const routeStatePublishTarget =
        validated_target(kRouteStatePublishRva, kRouteStatePublishPrefix);
    std::byte* const omegaRoute38PrepareTarget =
        image != nullptr ? image + kOmegaRoute38PrepareRva : nullptr;
    std::byte* const omegaRoute38ModeApplyTarget =
        image != nullptr ? image + kOmegaRoute38ModeApplyRva : nullptr;
    std::byte* const omegaRoute38EmbeddedDriverTarget =
        image != nullptr ? image + kOmegaRoute38EmbeddedDriverRva : nullptr;
    std::byte* const omegaRouteLifecycleQueryTarget =
        image != nullptr ? image + kOmegaRouteLifecycleQueryRva : nullptr;
    std::byte* const omegaRouteLifecycleAccessorTarget =
        image != nullptr ? image + kOmegaRouteLifecycleAccessorRva : nullptr;
    std::byte* const managerLocalStartTarget =
        validated_target(kManagerLocalStartRva, kManagerLocalStartPrefix);
    std::byte* const managerAuthoredStartTarget =
        validated_target(kManagerAuthoredStartRva, kManagerAuthoredStartPrefix);
    std::byte* const managerSetupStageATarget =
        validated_target(kManagerSetupStageARva, kManagerSetupStageAPrefix);
    std::byte* const managerSetupStageBTarget =
        validated_target(kManagerSetupStageBRva, kManagerSetupStageBPrefix);
    std::byte* const managerSetupStageCTarget =
        validated_target(kManagerSetupStageCRva, kManagerSetupStageCPrefix);
    std::byte* const managerSetupStageDTarget =
        validated_target(kManagerSetupStageDRva, kManagerSetupStageDPrefix);
    std::byte* const componentDispatchTarget =
        validated_target(kComponentDispatchRva, kComponentDispatchPrefix);
    std::byte* const componentLookupTarget =
        validated_target(kComponentLookupRva, kComponentLookupPrefix);
    std::byte* const componentRegisterTarget =
        validated_target(kComponentRegisterRva, kComponentRegisterPrefix);
    std::byte* const componentReuseTarget =
        image != nullptr && managerUpdateLoopTarget != nullptr ? image + kComponentReuseRva : nullptr;
    std::byte* const componentMappingTarget = image != nullptr && managerUpdateLoopTarget != nullptr
                                                   ? image + kComponentMappingRva
                                                   : nullptr;
    std::byte* const componentBuildTarget =
        validated_target(kComponentBuildRva, kComponentBuildPrefix);
    // The exact call target is recovered from the validated manager-update loop at 0x176509F.
    // Its on-disk bytes are encrypted, so the runtime address cannot use a static prefix check.
    std::byte* const componentStepTarget =
        image != nullptr && managerUpdateLoopTarget != nullptr ? image + kComponentStepRva : nullptr;
    std::byte* const componentTickTarget =
        validated_target(kComponentTickRva, kComponentTickPrefix);
    std::byte* const postComponentSyncTarget =
        validated_target(kPostComponentSyncRva, kPostComponentSyncPrefix);
    if (transitionTarget == nullptr || slotTarget == nullptr || modeTarget == nullptr
        || authoritySlotMapTarget == nullptr || authorityApplyTarget == nullptr
        || authorityRefreshTarget == nullptr || authorityAnyAssignedGateTarget == nullptr
        || authorityAnyUnassignedGateTarget == nullptr || authoritySnapshotGateTarget == nullptr
        || assignmentProviderUpdateTarget == nullptr
        || assignmentProviderStateConsumerTarget == nullptr
        || matchmakingLanePolicyMaterializerTarget == nullptr
        || matchmakingLaneRebuildTarget == nullptr
        || assignmentWatcherUpdateTarget == nullptr
        || managerLookupTarget == nullptr
        || lifecycleEventTarget == nullptr
        || managerEnsureTarget == nullptr || identityRequestTarget == nullptr
        || identityEnableTarget == nullptr
        || managerActivateTarget == nullptr
        || hostHandoffApplyTarget == nullptr
        || memberRecordIndexTarget == nullptr
        || identityDescriptorCopyTarget == nullptr
         || transitionDispatchTarget == nullptr || wireDispatchTarget == nullptr
         || activityClientUpdateTarget == nullptr
         || activityReceiverLookupTarget == nullptr
         || activityRosterApplyTarget == nullptr
         || activityPeerDispatchTarget == nullptr
         || activityReceiverActivateTarget == nullptr
         || activityReceiverBindTarget == nullptr
         || activityReceiverCreatorTarget == nullptr
        || managerUpdateLoopTarget == nullptr || managerUpdateGateTarget == nullptr
        || launchProducerTarget == nullptr
        || launchProducerGuardSetterTarget == nullptr
         || launchProducerReadyCallbackTarget == nullptr
         || activityEventDispatchLocalTarget == nullptr
         || activityEventDispatchPayloadTarget == nullptr
         || activityEvent46RegisteredProducerTarget == nullptr
         || activityEvent46ProducerResetTarget == nullptr
         || activityEvent46ProducerInitializeTarget == nullptr
         || activityEvent46ProducerAvailableTarget == nullptr
         || activityEvent46ProducerRequestTarget == nullptr
         || activityEvent46ProducerTickTarget == nullptr
         || activityEvent46RequestFlowTarget == nullptr
         || activityEvent46ActionDispatchTarget == nullptr
         || activityEvent46RequestPredicateTarget == nullptr
         || managerModeSetTarget == nullptr
        || routeDescriptorLookupTarget == nullptr
        || routeCommitTarget == nullptr || routeUpdateTarget == nullptr
        || defaultRouteZeroInitializerTarget == nullptr
        || defaultRouteReferencePredicateTarget == nullptr
        || defaultRoutePairPredicateTarget == nullptr
        || defaultRouteRequiredPredicateTarget == nullptr
        || defaultRouteCrossManagerPredicateTarget == nullptr
        || laneOneReadyScanTarget == nullptr || laneOneActiveCountTarget == nullptr
        || laneOneSelectionReadyTarget == nullptr
        || laneOneAuthoredInitializerTarget == nullptr
        || routeFlagUpdateTarget == nullptr || routeElapsedTimeTarget == nullptr
        || routeTimeoutConfigTarget == nullptr
        || routeAlternateUpdateTarget == nullptr || routeStateEvaluatorTarget == nullptr
        || routeParentDispatchTarget == nullptr
        || routeSelectorContextAccessorTarget == nullptr || routeSelectorContext == nullptr
        || routeSelectorVtable == nullptr || routeSelectorVariantLookupTarget == nullptr
        || routeSelectorVariantModeTarget == nullptr || routeStateArmTarget == nullptr
        || embeddedRouteStatePhaseSetTarget == nullptr
        || embeddedRouteStateInitializeTarget == nullptr
        || embeddedRouteStateResetTarget == nullptr
        || embeddedRouteObjectTransitionTarget == nullptr
        || embeddedRouteLaneZeroDriverTarget == nullptr
        || embeddedRouteTransitionHandlerTarget == nullptr
        || embeddedRouteSessionActivityTarget == nullptr
        || embeddedRouteStateStatusTarget == nullptr
        || embeddedRouteContextSkipPredicateTarget == nullptr
        || embeddedRouteLaneAvailablePredicateTarget == nullptr
        || embeddedRouteAuthoredIdentityPredicateTarget == nullptr
        || embeddedRouteLocalIdentityPredicateTarget == nullptr
        || embeddedRouteIdentityProviderRecordLookupTarget == nullptr
        || routeAlternateSourceTarget == nullptr || routeAlternatePredicateTarget == nullptr
        || routeAlternateModeTarget == nullptr
        || routeSelectionIngestTarget == nullptr
        || authoredLaunchDispatchTarget == nullptr
        || launchCommandInitializeTarget == nullptr
        || launchCommandDispatchTarget == nullptr
        || routeModeZeroSetTarget == nullptr || routeModeOneSetTarget == nullptr
        || omegaRouteLifecycleQueryTarget == nullptr
        || omegaRouteLifecycleAccessorTarget == nullptr
        || routeStateQueryTarget == nullptr || routeStatePublishTarget == nullptr
        || managerLocalStartTarget == nullptr || managerAuthoredStartTarget == nullptr
        || managerSetupStageATarget == nullptr
         || managerSetupStageBTarget == nullptr || managerSetupStageCTarget == nullptr
         || managerSetupStageDTarget == nullptr || componentDispatchTarget == nullptr
         || componentLookupTarget == nullptr
         || componentRegisterTarget == nullptr || componentReuseTarget == nullptr
          || componentMappingTarget == nullptr || componentBuildTarget == nullptr
          || componentStepTarget == nullptr || componentTickTarget == nullptr
          || postComponentSyncTarget == nullptr) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::warn,
                         "ev=bootflow stage=activity_script_upstream_probe result=fail reason=target");
        return false;
    }
    dump_runtime_code(L"activity_script_manager_update_loop", managerUpdateLoopTarget, 0x500U);
    dump_runtime_code(L"activity_script_authority_apply", authorityApplyTarget, 0x400U);
    dump_runtime_code(L"activity_script_authority_refresh", authorityRefreshTarget, 0x700U);
    dump_runtime_code(L"activity_script_authority_refresh_gates",
                      authorityAnyAssignedGateTarget,
                      0x100U);
    // The guarded authority bootstrap can only resolve the native snapshot; it cannot initialize
    // the route-selector provider's allocation strategy at variant +0x2AC. Preserve the provider
    // methods that own that runtime before changing the field or requesting another mission run.
    dump_runtime_code(L"activity_script_assignment_provider_vtable",
                      routeSelectorVtable,
                      0x200U);
    dump_runtime_code(L"activity_script_assignment_provider_variant_lookup",
                      routeSelectorVariantLookupTarget,
                      0x100U);
    dump_runtime_code(L"activity_script_assignment_provider_variant_mode",
                      routeSelectorVariantModeTarget,
                      0x200U);
    dump_runtime_code(L"activity_session_search_result_consumer",
                      assignmentProviderStateConsumerTarget,
                      0x1000U);
    dump_runtime_code(L"activity_matchmaking_lane_policy_materializer",
                      matchmakingLanePolicyMaterializerTarget,
                      0x600U);
    dump_runtime_code(L"activity_matchmaking_lane_rebuild",
                      matchmakingLaneRebuildTarget,
                      0x1000U);
    dump_runtime_code(L"activity_script_assignment_provider_resolve",
                      image + kAssignmentProviderResolveRva,
                      0x1000U);
    dump_runtime_code(L"activity_script_assignment_provider_publish",
                      image + kAssignmentProviderPublishRva,
                      0x1000U);
    dump_runtime_code(L"activity_script_assignment_provider_runtime_initializer",
                      image + kAssignmentProviderRuntimeInitializerRva,
                      0x1800U);
    std::byte* const routeSelectorVariantZero = routeSelectorContext + 0x25C40U;
    std::array<char, core::log::kLineCapacity> providerLine{};
    const int providerLineLength = std::snprintf(
        providerLine.data(),
        providerLine.size(),
        "ev=bootflow stage=activity_assignment_provider_runtime result=observed context=%p vtable=%p variant0=%p valid=%u limit=%u enabled=%u strategy=%u configuration=%d entry_count=%d context_token=0x%llX context_pending=%u context_ready=%u mutation=observe_only",
        static_cast<void*>(routeSelectorContext),
        static_cast<void*>(routeSelectorVtable),
        static_cast<void*>(routeSelectorVariantZero),
        static_cast<unsigned int>(safe_read<std::uint8_t>(routeSelectorVariantZero, 0U)),
        safe_read<std::uint32_t>(routeSelectorVariantZero + 0x21CU, 0U),
        static_cast<unsigned int>(
            safe_read<std::uint8_t>(routeSelectorVariantZero + 0x22BU, 0U)),
        static_cast<unsigned int>(
            safe_read<std::uint8_t>(routeSelectorVariantZero + 0x2ACU, 0U)),
        safe_read<std::int32_t>(routeSelectorVariantZero + 0x2B0U, -1),
        safe_read<std::int32_t>(routeSelectorVariantZero + 0x2B4U, -1),
        static_cast<unsigned long long>(
            safe_read<std::uint64_t>(routeSelectorContext + 0x253C0U, 0U)),
        static_cast<unsigned int>(
            safe_read<std::uint8_t>(routeSelectorContext + 0x253F0U, 0U)),
        safe_read<std::uint32_t>(routeSelectorContext + 0x253ECU, 0U));
    if (providerLineLength > 0) {
        core::log::write(core::log::Channel::client,
                         core::log::Level::info,
                         {providerLine.data(), static_cast<std::size_t>(providerLineLength)});
    }
    // Preserve the runtime-decrypted assignment-table producer chain. The scheduler at
    // 0x1760800 only reads this table; these upstream calls are where its lone status=-1
    // record is created or resolved. This is dump-only and does not alter native state.
    dump_runtime_code(L"activity_script_assignment_update_caller_region",
                      image + 0x1747000U,
                      0x1000U);
    dump_runtime_code(L"activity_script_assignment_candidate_update",
                      image + 0x17DA900U,
                      0x1000U);
    dump_runtime_code(L"activity_script_assignment_publish",
                      image + 0x17E22F0U,
                      0x800U);
    dump_runtime_code(L"activity_script_assignment_global_accessor",
                      image + 0x17E2EC0U,
                      0x400U);
    dump_runtime_code(L"activity_script_manager_update_gate", managerUpdateGateTarget, 0x300U);
    dump_runtime_code(L"activity_script_route_descriptor_lookup", image + 0x1751370U, 0x1000U);
    dump_runtime_code(L"activity_script_route_commit", routeCommitTarget, 0x300U);
    dump_runtime_code(L"activity_script_route_update", routeUpdateTarget, 0x800U);
    // Preserve the runtime-decrypted native caller which selects the default/local
    // route constructor.  The call at 0xC28069 returns to 0xC2806E; observing the
    // surrounding branch is the next upstream diagnostic and does not mutate state.
    dump_runtime_code(L"activity_script_default_route_caller_region",
                      image + 0xC27000U,
                      0x3000U);
    dump_runtime_code(L"activity_script_default_route_zero_initializer",
                      defaultRouteZeroInitializerTarget,
                      0x600U);
    dump_runtime_code(L"activity_script_default_route_reference_predicate",
                      defaultRouteReferencePredicateTarget,
                      0x1000U);
    dump_runtime_code(L"activity_script_default_route_pair_predicate",
                      defaultRoutePairPredicateTarget,
                      0x300U);
    dump_runtime_code(L"activity_script_default_route_required_predicate",
                      defaultRouteRequiredPredicateTarget,
                      0x300U);
    dump_runtime_code(L"activity_script_default_route_cross_manager_predicate",
                      defaultRouteCrossManagerPredicateTarget,
                      0x300U);
    dump_runtime_code(L"activity_script_lane1_ready_scan", laneOneReadyScanTarget, 0x200U);
    dump_runtime_code(L"activity_script_lane1_active_count", laneOneActiveCountTarget, 0x20U);
    dump_runtime_code(L"activity_script_lane1_selection_ready",
                      laneOneSelectionReadyTarget,
                      0x200U);
    dump_runtime_code(L"activity_script_lane1_authored_initializer",
                      laneOneAuthoredInitializerTarget,
                      0x700U);
    dump_runtime_code(L"activity_script_route_flag_update", routeFlagUpdateTarget, 0xB00U);
    dump_runtime_code(L"activity_script_route_elapsed_time", routeElapsedTimeTarget, 0x80U);
    dump_runtime_code(L"activity_script_route_timeout_config", routeTimeoutConfigTarget, 0x400U);
    dump_runtime_code(L"activity_script_route_alternate_update",
                      routeAlternateUpdateTarget,
                      0x100U);
    dump_runtime_code(L"activity_script_route_state_evaluator",
                      routeStateEvaluatorTarget,
                      0x100U);
    dump_runtime_code(L"activity_script_route_parent_dispatch",
                      routeParentDispatchTarget,
                      0x700U);
    dump_runtime_code(L"activity_script_omega_route_38_prepare",
                      omegaRoute38PrepareTarget,
                      0x600U);
    dump_runtime_code(L"activity_script_omega_route_38_mode_apply",
                      omegaRoute38ModeApplyTarget,
                      0x300U);
    dump_runtime_code(L"activity_script_omega_route_38_embedded_driver",
                      omegaRoute38EmbeddedDriverTarget,
                      0x1000U);
    dump_runtime_code(L"activity_script_omega_route_lifecycle_query",
                      omegaRouteLifecycleQueryTarget,
                      0x400U);
    dump_runtime_code(L"activity_script_omega_route_lifecycle_accessor",
                      omegaRouteLifecycleAccessorTarget,
                      0x800U);
    dump_runtime_code(L"activity_script_route_selector_variant",
                      routeSelectorVariantLookupTarget,
                      0x100U);
    dump_runtime_code(L"activity_script_candidate_state_object_initializer",
                      routeStateArmTarget,
                      0x80U);
    dump_runtime_code(L"activity_script_embedded_route_phase_set",
                      embeddedRouteStatePhaseSetTarget,
                      0x80U);
    dump_runtime_code(L"activity_script_embedded_route_initialize",
                      embeddedRouteStateInitializeTarget,
                      0x100U);
    dump_runtime_code(L"activity_script_embedded_route_reset",
                      embeddedRouteStateResetTarget,
                      0x200U);
    dump_runtime_code(L"activity_script_embedded_route_object_transition",
                      embeddedRouteObjectTransitionTarget,
                      0x500U);
    dump_runtime_code(L"activity_script_embedded_route_lane0_driver",
                      embeddedRouteLaneZeroDriverTarget,
                      0x600U);
    dump_runtime_code(L"activity_script_embedded_route_transition_handler",
                      embeddedRouteTransitionHandlerTarget,
                      0x400U);
    dump_runtime_code(L"activity_script_embedded_route_driver_session_query",
                      embeddedRouteSessionActivityTarget,
                      0x80U);
    dump_runtime_code(L"activity_script_embedded_route_driver_status_query",
                      embeddedRouteStateStatusTarget,
                      0x80U);
    dump_runtime_code(L"activity_script_embedded_route_driver_context_skip",
                      embeddedRouteContextSkipPredicateTarget,
                      0x200U);
    dump_runtime_code(L"activity_script_embedded_route_driver_lane_available",
                      embeddedRouteLaneAvailablePredicateTarget,
                      0x100U);
    dump_runtime_code(L"activity_script_embedded_route_driver_authored_identity",
                      embeddedRouteAuthoredIdentityPredicateTarget,
                      0x100U);
    dump_runtime_code(L"activity_script_embedded_route_driver_local_identity",
                      embeddedRouteLocalIdentityPredicateTarget,
                      0x100U);
    dump_runtime_code(L"activity_script_embedded_route_identity_provider_record_lookup",
                      embeddedRouteIdentityProviderRecordLookupTarget,
                      0x80U);
    dump_runtime_code(L"activity_script_identity_provider_container_region",
                      image + 0x505000U,
                      0x3000U);
    dump_runtime_code(L"activity_script_embedded_route_action_region",
                      image + 0xEEA000U,
                      0x9000U);
    // The default-route constructor thunk returns to 0xEE83F8. Preserve its
    // runtime-decrypted caller so the native local/authored selection can be
    // mapped without changing the route object or constructor result.
    dump_runtime_code(L"activity_script_default_route_parent_caller_region",
                      image + 0xEE7000U,
                      0x4000U);
    dump_runtime_code(L"activity_selection_state5_blocking_helper_region",
                      image + 0xC21000U,
                      0x3000U);
    dump_runtime_code(L"activity_script_route_alternate_source",
                      routeAlternateSourceTarget,
                      0x100U);
    dump_runtime_code(L"activity_script_route_alternate_predicate",
                      routeAlternatePredicateTarget,
                      0x80U);
    dump_runtime_code(L"activity_script_route_alternate_mode",
                      routeAlternateModeTarget,
                      0x20U);
    dump_runtime_code(L"activity_script_route_selection_ingest",
                      routeSelectionIngestTarget,
                      0x800U);
    dump_runtime_code(L"activity_script_route_state_query", routeStateQueryTarget, 0xB20U);
    dump_runtime_code(L"activity_script_route_state_publish", routeStatePublishTarget, 0x700U);
    dump_runtime_code(L"activity_script_manager_selection_pump",
                      image + kManagerSelectionPumpRva,
                      0x1D00U);
    dump_runtime_code(L"activity_script_manager_mode_set", managerModeSetTarget, 0xC00U);
    dump_runtime_code(L"activity_script_manager_local_dispatch",
                      image + kManagerLocalDispatchRva,
                      0x80U);
    dump_runtime_code(L"activity_script_manager_local_start", managerLocalStartTarget, 0xE00U);
    dump_runtime_code(L"activity_script_manager_mode_six_thunk",
                      image + kManagerModeSixThunkRva,
                      0x80U);
    dump_runtime_code(L"activity_script_manager_authored_start",
                      managerAuthoredStartTarget,
                      0xB00U);
    dump_runtime_code(L"activity_script_launch_producer", launchProducerTarget, 0x500U);
    dump_runtime_code(L"activity_script_launch_producer_ready",
                      image + kLaunchProducerReadyRva,
                      0x400U);
    dump_runtime_code(L"activity_script_event_registry_queue",
                      image + kActivityEventRegistryQueueRva,
                      0x900U);
    dump_runtime_code(L"activity_script_event_queue_drain_caller",
                      image + kActivityEventQueueDrainCallerRva,
                      0x1200U);
    dump_runtime_code(L"activity_script_launch_producer_phase",
                      image + kLaunchProducerPhaseRva,
                      0x400U);
    dump_runtime_code(L"activity_script_component_dispatch", componentDispatchTarget, 0x240U);
    dump_runtime_code(L"activity_script_component_lookup", componentLookupTarget, 0x300U);
    dump_runtime_code(L"activity_script_component_register", componentRegisterTarget, 0x1000U);
    dump_runtime_code(L"activity_script_component_reuse", componentReuseTarget, 0x800U);
    dump_runtime_code(L"activity_script_component_mapping", componentMappingTarget, 0x800U);
    dump_runtime_code(L"activity_script_component_build", componentBuildTarget, 0x800U);
    dump_runtime_code(L"activity_script_component_step", componentStepTarget, 0x800U);
    dump_runtime_code(L"activity_script_component_tick", componentTickTarget, 0x800U);
    dump_runtime_code(L"activity_script_component_transition_request_send",
                      image + kComponentTransitionRequestSendRva,
                      0x800U);
    dump_runtime_code(L"activity_script_post_component_sync", postComponentSyncTarget, 0x500U);
    dump_runtime_code(L"activity_script_lifecycle_event", lifecycleEventTarget, 0x800U);
    dump_runtime_code(L"activity_script_manager_event_publish", image + 0x178A830U, 0x1000U);
    dump_runtime_code(L"activity_script_manager_ensure", managerEnsureTarget, 0x800U);
    dump_runtime_code(L"activity_script_identity_request", identityRequestTarget, 0x600U);
    dump_runtime_code(L"activity_script_identity_enable", identityEnableTarget, 0x800U);
    dump_runtime_code(L"activity_script_manager_activate", managerActivateTarget, 0x1000U);
    dump_runtime_code(L"activity_script_host_handoff", hostHandoffApplyTarget, 0x1000U);
    dump_runtime_code(L"activity_script_member_record_index", memberRecordIndexTarget, 0x200U);
    dump_runtime_code(L"activity_script_identity_definition",
                      image + kIdentityDefinitionRva,
                      0x300U);
    dump_runtime_code(L"activity_script_identity_descriptor_copy",
                      identityDescriptorCopyTarget,
                      0x200U);
    dump_runtime_code(L"activity_script_session_description_stage",
                      sessionDescriptionStageTarget,
                      0x600U);
    dump_runtime_code(L"activity_script_managed_session_migration",
                      managedSessionMigrationTarget,
                      0x700U);
    dump_runtime_code(L"activity_script_receiver_bind", activityReceiverBindTarget, 0x200U);
    dump_runtime_code(L"activity_script_receiver_creator",
                      activityReceiverCreatorTarget,
                      0x800U);
    dump_runtime_code(L"activity_script_identity_enable_caller", image + 0x1775000U, 0x600U);
    if (!hooking::detour::install(
            {transitionTarget, reinterpret_cast<void*>(&transition_update)}, g_transitionHandle)) {
        return false;
    }
    g_transitionOriginal.store(reinterpret_cast<TransitionUpdate>(g_transitionHandle.original),
                               std::memory_order_release);
    if (!hooking::detour::install({slotTarget, reinterpret_cast<void*>(&slot_update)},
                                  g_slotHandle)) {
        (void)hooking::detour::uninstall(g_transitionHandle);
        clear_originals();
        return false;
    }
    g_slotOriginal.store(reinterpret_cast<SlotUpdate>(g_slotHandle.original),
                         std::memory_order_release);
    if (!hooking::detour::install({modeTarget, reinterpret_cast<void*>(&mode_update)},
                                  g_modeHandle)) {
        (void)hooking::detour::uninstall(g_slotHandle);
        (void)hooking::detour::uninstall(g_transitionHandle);
        clear_originals();
        return false;
    }
    g_modeOriginal.store(reinterpret_cast<ModeUpdate>(g_modeHandle.original),
                         std::memory_order_release);
    if (!hooking::detour::install(
            {authoritySlotMapTarget, reinterpret_cast<void*>(&authority_slot_map)},
            g_authoritySlotMapHandle)) {
        (void)hooking::detour::uninstall(g_modeHandle);
        (void)hooking::detour::uninstall(g_slotHandle);
        (void)hooking::detour::uninstall(g_transitionHandle);
        clear_originals();
        return false;
    }
    g_authoritySlotMapOriginal.store(
        reinterpret_cast<AuthoritySlotMap>(g_authoritySlotMapHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {authorityApplyTarget, reinterpret_cast<void*>(&authority_apply)},
            g_authorityApplyHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_authorityApplyOriginal.store(
        reinterpret_cast<AuthorityApply>(g_authorityApplyHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {authorityAnyAssignedGateTarget,
             reinterpret_cast<void*>(&authority_any_assigned_gate)},
            g_authorityAnyAssignedGateHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_authorityAnyAssignedGateOriginal.store(
        reinterpret_cast<AuthorityEntriesGate>(g_authorityAnyAssignedGateHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {authorityAnyUnassignedGateTarget,
             reinterpret_cast<void*>(&authority_any_unassigned_gate)},
            g_authorityAnyUnassignedGateHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_authorityAnyUnassignedGateOriginal.store(
        reinterpret_cast<AuthorityEntriesGate>(g_authorityAnyUnassignedGateHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {authoritySnapshotGateTarget, reinterpret_cast<void*>(&authority_snapshot_gate)},
            g_authoritySnapshotGateHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_authoritySnapshotGateOriginal.store(
        reinterpret_cast<AuthoritySnapshotGate>(g_authoritySnapshotGateHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {assignmentProviderUpdateTarget,
             reinterpret_cast<void*>(&assignment_provider_update)},
            g_assignmentProviderUpdateHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_assignmentProviderUpdateOriginal.store(
        reinterpret_cast<AssignmentProviderUpdate>(g_assignmentProviderUpdateHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {assignmentProviderStateConsumerTarget,
             reinterpret_cast<void*>(&assignment_provider_state_consumer)},
            g_assignmentProviderStateConsumerHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_assignmentProviderStateConsumerOriginal.store(
        reinterpret_cast<AssignmentProviderStateConsumer>(
            g_assignmentProviderStateConsumerHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {matchmakingLanePolicyMaterializerTarget,
             reinterpret_cast<void*>(&matchmaking_lane_policy_materializer)},
            g_matchmakingLanePolicyMaterializerHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_matchmakingLanePolicyMaterializerOriginal.store(
        reinterpret_cast<MatchmakingLanePolicyMaterializer>(
            g_matchmakingLanePolicyMaterializerHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {matchmakingLaneRebuildTarget, reinterpret_cast<void*>(&matchmaking_lane_rebuild)},
            g_matchmakingLaneRebuildHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_matchmakingLaneRebuildOriginal.store(
        reinterpret_cast<MatchmakingLaneRebuild>(g_matchmakingLaneRebuildHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {assignmentWatcherUpdateTarget,
             reinterpret_cast<void*>(&assignment_watcher_update)},
            g_assignmentWatcherUpdateHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_assignmentWatcherUpdateOriginal.store(
        reinterpret_cast<AssignmentWatcherUpdate>(g_assignmentWatcherUpdateHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {authorityRefreshTarget, reinterpret_cast<void*>(&authority_refresh)},
            g_authorityRefreshHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_authorityRefreshOriginal.store(
        reinterpret_cast<AuthorityRefresh>(g_authorityRefreshHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {lifecycleEventTarget, reinterpret_cast<void*>(&lifecycle_event)},
            g_lifecycleEventHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_lifecycleEventOriginal.store(
        reinterpret_cast<LifecycleEvent>(g_lifecycleEventHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {managerLookupTarget, reinterpret_cast<void*>(&manager_lookup)},
            g_managerLookupHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_managerLookupOriginal.store(
        reinterpret_cast<ManagerLookup>(g_managerLookupHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {managerEnsureTarget, reinterpret_cast<void*>(&manager_ensure)},
            g_managerEnsureHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_managerEnsureOriginal.store(
        reinterpret_cast<ManagerEnsure>(g_managerEnsureHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {identityRequestTarget, reinterpret_cast<void*>(&identity_request)},
            g_identityRequestHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_identityRequestOriginal.store(
        reinterpret_cast<IdentityRequest>(g_identityRequestHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {identityEnableTarget, reinterpret_cast<void*>(&identity_enable)},
            g_identityEnableHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_identityEnableOriginal.store(
        reinterpret_cast<IdentityEnable>(g_identityEnableHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {managerActivateTarget, reinterpret_cast<void*>(&manager_activate)},
            g_managerActivateHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_managerActivateOriginal.store(
        reinterpret_cast<ManagerActivate>(g_managerActivateHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {hostHandoffApplyTarget, reinterpret_cast<void*>(&manager_host_handoff)},
            g_hostHandoffApplyHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_hostHandoffApplyOriginal.store(
        reinterpret_cast<HostHandoffApply>(g_hostHandoffApplyHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {memberRecordIndexTarget, reinterpret_cast<void*>(&member_record_index)},
            g_memberRecordIndexHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_memberRecordIndexOriginal.store(
        reinterpret_cast<MemberRecordIndex>(g_memberRecordIndexHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {identityDescriptorCopyTarget,
             reinterpret_cast<void*>(&identity_descriptor_copy)},
            g_identityDescriptorCopyHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_identityDescriptorCopyOriginal.store(
        reinterpret_cast<IdentityDescriptorCopy>(g_identityDescriptorCopyHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {sessionDescriptionStageTarget,
             reinterpret_cast<void*>(&session_description_stage)},
            g_sessionDescriptionStageHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_sessionDescriptionStageOriginal.store(
        reinterpret_cast<SessionDescriptionStage>(g_sessionDescriptionStageHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {managedSessionMigrationTarget,
             reinterpret_cast<void*>(&managed_session_migration)},
            g_managedSessionMigrationHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_managedSessionMigrationOriginal.store(
        reinterpret_cast<ManagedSessionMigration>(g_managedSessionMigrationHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {transitionDispatchTarget, reinterpret_cast<void*>(&transition_dispatch)},
            g_transitionDispatchHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_transitionDispatchOriginal.store(
        reinterpret_cast<TransitionDispatch>(g_transitionDispatchHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {wireDispatchTarget, reinterpret_cast<void*>(&wire_dispatch)},
            g_wireDispatchHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_wireDispatchOriginal.store(reinterpret_cast<WireDispatch>(g_wireDispatchHandle.original),
                                  std::memory_order_release);
    if (!hooking::detour::install(
            {activityClientUpdateTarget, reinterpret_cast<void*>(&activity_client_update)},
            g_activityClientUpdateHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_activityClientUpdateOriginal.store(
        reinterpret_cast<ActivityClientUpdate>(g_activityClientUpdateHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {activityReceiverLookupTarget, reinterpret_cast<void*>(&activity_receiver_lookup)},
            g_activityReceiverLookupHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_activityReceiverLookupOriginal.store(
        reinterpret_cast<ActivityReceiverLookup>(g_activityReceiverLookupHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {activityRosterApplyTarget, reinterpret_cast<void*>(&activity_roster_apply)},
            g_activityRosterApplyHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_activityRosterApplyOriginal.store(
        reinterpret_cast<ActivityRosterApply>(g_activityRosterApplyHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {activityPeerDispatchTarget, reinterpret_cast<void*>(&activity_peer_dispatch)},
            g_activityPeerDispatchHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_activityPeerDispatchOriginal.store(
        reinterpret_cast<ActivityPeerDispatch>(g_activityPeerDispatchHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {activityReceiverActivateTarget,
             reinterpret_cast<void*>(&activity_receiver_activate)},
            g_activityReceiverActivateHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_activityReceiverActivateOriginal.store(
        reinterpret_cast<ActivityReceiverActivate>(g_activityReceiverActivateHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {activityReceiverBindTarget, reinterpret_cast<void*>(&activity_receiver_bind)},
            g_activityReceiverBindHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_activityReceiverBindOriginal.store(
        reinterpret_cast<ActivityReceiverBind>(g_activityReceiverBindHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {activityReceiverCreatorTarget,
             reinterpret_cast<void*>(&activity_receiver_creator)},
            g_activityReceiverCreatorHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_activityReceiverCreatorOriginal.store(
        reinterpret_cast<ActivityReceiverCreator>(g_activityReceiverCreatorHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {managerUpdateLoopTarget, reinterpret_cast<void*>(&manager_update_loop)},
            g_managerUpdateLoopHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_managerUpdateLoopOriginal.store(
        reinterpret_cast<ManagerUpdateLoop>(g_managerUpdateLoopHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {managerUpdateGateTarget, reinterpret_cast<void*>(&manager_update_gate)},
            g_managerUpdateGateHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_managerUpdateGateOriginal.store(
        reinterpret_cast<ManagerUpdateGate>(g_managerUpdateGateHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {launchProducerTarget, reinterpret_cast<void*>(&launch_producer)},
            g_launchProducerHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_launchProducerOriginal.store(
        reinterpret_cast<LaunchProducer>(g_launchProducerHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {launchProducerGuardSetterTarget,
             reinterpret_cast<void*>(&launch_producer_guard_set)},
            g_launchProducerGuardSetterHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_launchProducerGuardSetterOriginal.store(
        reinterpret_cast<LaunchProducerGuardSetter>(g_launchProducerGuardSetterHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {launchProducerReadyCallbackTarget,
             reinterpret_cast<void*>(&launch_producer_ready_callback)},
            g_launchProducerReadyCallbackHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_launchProducerReadyCallbackOriginal.store(
        reinterpret_cast<LaunchProducerReadyCallback>(
            g_launchProducerReadyCallbackHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {activityEventDispatchLocalTarget,
             reinterpret_cast<void*>(&activity_event_dispatch_local)},
            g_activityEventDispatchLocalHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_activityEventDispatchLocalOriginal.store(
        reinterpret_cast<ActivityEventDispatchLocal>(g_activityEventDispatchLocalHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {activityEventDispatchPayloadTarget,
             reinterpret_cast<void*>(&activity_event_dispatch_payload)},
            g_activityEventDispatchPayloadHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_activityEventDispatchPayloadOriginal.store(
        reinterpret_cast<ActivityEventDispatchPayload>(
            g_activityEventDispatchPayloadHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {activityEvent46RegisteredProducerTarget,
             reinterpret_cast<void*>(&activity_event46_registered_producer)},
            g_activityEvent46RegisteredProducerHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_activityEvent46RegisteredProducerOriginal.store(
        reinterpret_cast<ActivityEvent46RegisteredProducer>(
            g_activityEvent46RegisteredProducerHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {activityEvent46ProducerResetTarget,
             reinterpret_cast<void*>(&activity_event46_producer_reset)},
            g_activityEvent46ProducerResetHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_activityEvent46ProducerResetOriginal.store(
        reinterpret_cast<ActivityEvent46ProducerReset>(
            g_activityEvent46ProducerResetHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {activityEvent46ProducerInitializeTarget,
             reinterpret_cast<void*>(&activity_event46_producer_initialize)},
            g_activityEvent46ProducerInitializeHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_activityEvent46ProducerInitializeOriginal.store(
        reinterpret_cast<ActivityEvent46ProducerInitialize>(
            g_activityEvent46ProducerInitializeHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {activityEvent46ProducerAvailableTarget,
             reinterpret_cast<void*>(&activity_event46_producer_available)},
            g_activityEvent46ProducerAvailableHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_activityEvent46ProducerAvailableOriginal.store(
        reinterpret_cast<ActivityEvent46ProducerAvailable>(
            g_activityEvent46ProducerAvailableHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {activityEvent46ProducerRequestTarget,
             reinterpret_cast<void*>(&activity_event46_producer_request)},
            g_activityEvent46ProducerRequestHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_activityEvent46ProducerRequestOriginal.store(
        reinterpret_cast<ActivityEvent46ProducerRequest>(
            g_activityEvent46ProducerRequestHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {activityEvent46ProducerTickTarget,
             reinterpret_cast<void*>(&activity_event46_producer_tick)},
            g_activityEvent46ProducerTickHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_activityEvent46ProducerTickOriginal.store(
        reinterpret_cast<ActivityEvent46ProducerTick>(
            g_activityEvent46ProducerTickHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {activityEvent46RequestFlowTarget,
             reinterpret_cast<void*>(&activity_event46_request_flow)},
            g_activityEvent46RequestFlowHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_activityEvent46RequestFlowOriginal.store(
        reinterpret_cast<ActivityEvent46RequestFlow>(
            g_activityEvent46RequestFlowHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {activityEvent46ActionDispatchTarget,
             reinterpret_cast<void*>(&activity_event46_action_dispatch)},
            g_activityEvent46ActionDispatchHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_activityEvent46ActionDispatchOriginal.store(
        reinterpret_cast<ActivityEvent46ActionDispatch>(
            g_activityEvent46ActionDispatchHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {activityEvent46RequestPredicateTarget,
             reinterpret_cast<void*>(&activity_event46_request_predicate)},
            g_activityEvent46RequestPredicateHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_activityEvent46RequestPredicateOriginal.store(
        reinterpret_cast<ActivityEvent46RequestPredicate>(
            g_activityEvent46RequestPredicateHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {managerModeSetTarget, reinterpret_cast<void*>(&manager_mode_set)},
            g_managerModeSetHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_managerModeSetOriginal.store(
        reinterpret_cast<ManagerModeSet>(g_managerModeSetHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {routeDescriptorLookupTarget, reinterpret_cast<void*>(&route_descriptor_lookup)},
            g_routeDescriptorLookupHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_routeDescriptorLookupOriginal.store(
        reinterpret_cast<RouteDescriptorLookup>(g_routeDescriptorLookupHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {routeUpdateTarget, reinterpret_cast<void*>(&route_update)},
            g_routeUpdateHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_routeUpdateOriginal.store(
        reinterpret_cast<RouteUpdate>(g_routeUpdateHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {routeCommitTarget, reinterpret_cast<void*>(&route_commit)},
            g_routeCommitHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_routeCommitOriginal.store(
        reinterpret_cast<RouteCommit>(g_routeCommitHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {defaultRouteZeroInitializerTarget,
             reinterpret_cast<void*>(&default_route_zero_initializer)},
            g_defaultRouteZeroInitializerHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_defaultRouteZeroInitializerOriginal.store(
        reinterpret_cast<DefaultRouteZeroInitializer>(
            g_defaultRouteZeroInitializerHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {defaultRouteReferencePredicateTarget,
             reinterpret_cast<void*>(&default_route_reference_predicate)},
            g_defaultRouteReferencePredicateHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_defaultRouteReferencePredicateOriginal.store(
        reinterpret_cast<DefaultRouteReferencePredicate>(
            g_defaultRouteReferencePredicateHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {defaultRoutePairPredicateTarget,
             reinterpret_cast<void*>(&default_route_pair_predicate)},
            g_defaultRoutePairPredicateHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_defaultRoutePairPredicateOriginal.store(
        reinterpret_cast<DefaultRoutePairPredicate>(
            g_defaultRoutePairPredicateHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {defaultRouteRequiredPredicateTarget,
             reinterpret_cast<void*>(&default_route_required_predicate)},
            g_defaultRouteRequiredPredicateHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_defaultRouteRequiredPredicateOriginal.store(
        reinterpret_cast<DefaultRouteRequiredPredicate>(
            g_defaultRouteRequiredPredicateHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {defaultRouteCrossManagerPredicateTarget,
             reinterpret_cast<void*>(&default_route_cross_manager_predicate)},
            g_defaultRouteCrossManagerPredicateHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_defaultRouteCrossManagerPredicateOriginal.store(
        reinterpret_cast<DefaultRouteCrossManagerPredicate>(
            g_defaultRouteCrossManagerPredicateHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {laneOneReadyScanTarget, reinterpret_cast<void*>(&lane_one_ready_scan)},
            g_laneOneReadyScanHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_laneOneReadyScanOriginal.store(
        reinterpret_cast<LaneOneReadyScan>(g_laneOneReadyScanHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {laneOneActiveCountTarget, reinterpret_cast<void*>(&lane_one_active_count)},
            g_laneOneActiveCountHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_laneOneActiveCountOriginal.store(
        reinterpret_cast<LaneOneActiveCount>(g_laneOneActiveCountHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {laneOneSelectionReadyTarget, reinterpret_cast<void*>(&lane_one_selection_ready)},
            g_laneOneSelectionReadyHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_laneOneSelectionReadyOriginal.store(
        reinterpret_cast<LaneOneSelectionReady>(g_laneOneSelectionReadyHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {laneOneAuthoredInitializerTarget,
             reinterpret_cast<void*>(&lane_one_authored_initializer)},
            g_laneOneAuthoredInitializerHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_laneOneAuthoredInitializerOriginal.store(
        reinterpret_cast<LaneOneAuthoredInitializer>(
            g_laneOneAuthoredInitializerHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {routeFlagUpdateTarget, reinterpret_cast<void*>(&route_flag_update)},
            g_routeFlagUpdateHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_routeFlagUpdateOriginal.store(
        reinterpret_cast<RouteFlagUpdate>(g_routeFlagUpdateHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {routeElapsedTimeTarget, reinterpret_cast<void*>(&route_elapsed_time)},
            g_routeElapsedTimeHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_routeElapsedTimeOriginal.store(
        reinterpret_cast<RouteElapsedTime>(g_routeElapsedTimeHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {routeTimeoutConfigTarget, reinterpret_cast<void*>(&route_timeout_config)},
            g_routeTimeoutConfigHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_routeTimeoutConfigOriginal.store(
        reinterpret_cast<RouteTimeoutConfig>(g_routeTimeoutConfigHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {routeAlternateUpdateTarget, reinterpret_cast<void*>(&route_alternate_update)},
            g_routeAlternateUpdateHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_routeAlternateUpdateOriginal.store(
        reinterpret_cast<RouteAlternateUpdate>(g_routeAlternateUpdateHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {routeStateEvaluatorTarget, reinterpret_cast<void*>(&route_state_evaluator)},
            g_routeStateEvaluatorHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_routeStateEvaluatorOriginal.store(
        reinterpret_cast<RouteStateEvaluator>(g_routeStateEvaluatorHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {routeParentDispatchTarget, reinterpret_cast<void*>(&route_parent_dispatch)},
            g_routeParentDispatchHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_routeParentDispatchOriginal.store(
        reinterpret_cast<RouteParentDispatch>(g_routeParentDispatchHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {routeSelectorVariantLookupTarget,
             reinterpret_cast<void*>(&route_selector_variant_lookup)},
            g_routeSelectorVariantLookupHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_routeSelectorVariantLookupOriginal.store(
        reinterpret_cast<RouteSelectorVariantLookup>(
            g_routeSelectorVariantLookupHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {routeStateArmTarget, reinterpret_cast<void*>(&route_state_arm)},
            g_routeStateArmHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_routeStateArmOriginal.store(
        reinterpret_cast<RouteStateArm>(g_routeStateArmHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {embeddedRouteStatePhaseSetTarget,
             reinterpret_cast<void*>(&embedded_route_state_phase_set)},
            g_embeddedRouteStatePhaseSetHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_embeddedRouteStatePhaseSetOriginal.store(
        reinterpret_cast<EmbeddedRouteStatePhaseSet>(
            g_embeddedRouteStatePhaseSetHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {embeddedRouteStateInitializeTarget,
             reinterpret_cast<void*>(&embedded_route_state_initialize)},
            g_embeddedRouteStateInitializeHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_embeddedRouteStateInitializeOriginal.store(
        reinterpret_cast<EmbeddedRouteStateInitialize>(
            g_embeddedRouteStateInitializeHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {embeddedRouteStateResetTarget,
             reinterpret_cast<void*>(&embedded_route_state_reset)},
            g_embeddedRouteStateResetHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_embeddedRouteStateResetOriginal.store(
        reinterpret_cast<EmbeddedRouteStateReset>(
            g_embeddedRouteStateResetHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {embeddedRouteSessionActivityTarget,
             reinterpret_cast<void*>(&embedded_route_session_activity_query)},
            g_embeddedRouteSessionActivityHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_embeddedRouteSessionActivityOriginal.store(
        reinterpret_cast<LaunchProducerSessionActivity>(
            g_embeddedRouteSessionActivityHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {embeddedRouteStateStatusTarget,
             reinterpret_cast<void*>(&embedded_route_state_status_query)},
            g_embeddedRouteStateStatusHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_embeddedRouteStateStatusOriginal.store(
        reinterpret_cast<EmbeddedRouteStateStatus>(
            g_embeddedRouteStateStatusHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {embeddedRouteContextSkipPredicateTarget,
             reinterpret_cast<void*>(&embedded_route_context_skip_predicate)},
            g_embeddedRouteContextSkipPredicateHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_embeddedRouteContextSkipPredicateOriginal.store(
        reinterpret_cast<EmbeddedRouteContextSkipPredicate>(
            g_embeddedRouteContextSkipPredicateHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {embeddedRouteLaneAvailablePredicateTarget,
             reinterpret_cast<void*>(&embedded_route_lane_available_predicate)},
            g_embeddedRouteLaneAvailablePredicateHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_embeddedRouteLaneAvailablePredicateOriginal.store(
        reinterpret_cast<EmbeddedRouteLaneAvailablePredicate>(
            g_embeddedRouteLaneAvailablePredicateHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {embeddedRouteAuthoredIdentityPredicateTarget,
             reinterpret_cast<void*>(&embedded_route_authored_identity_predicate)},
            g_embeddedRouteAuthoredIdentityPredicateHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_embeddedRouteAuthoredIdentityPredicateOriginal.store(
        reinterpret_cast<EmbeddedRouteIdentityPredicate>(
            g_embeddedRouteAuthoredIdentityPredicateHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {embeddedRouteLocalIdentityPredicateTarget,
             reinterpret_cast<void*>(&embedded_route_local_identity_predicate)},
            g_embeddedRouteLocalIdentityPredicateHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_embeddedRouteLocalIdentityPredicateOriginal.store(
        reinterpret_cast<EmbeddedRouteIdentityPredicate>(
            g_embeddedRouteLocalIdentityPredicateHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {embeddedRouteIdentityProviderRecordLookupTarget,
             reinterpret_cast<void*>(&embedded_route_identity_provider_record_lookup)},
            g_embeddedRouteIdentityProviderRecordLookupHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_embeddedRouteIdentityProviderRecordLookupOriginal.store(
        reinterpret_cast<EmbeddedRouteIdentityProviderRecordLookup>(
            g_embeddedRouteIdentityProviderRecordLookupHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {embeddedRouteObjectTransitionTarget,
             reinterpret_cast<void*>(&embedded_route_object_transition)},
            g_embeddedRouteObjectTransitionHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_embeddedRouteObjectTransitionOriginal.store(
        reinterpret_cast<EmbeddedRouteObjectTransition>(
            g_embeddedRouteObjectTransitionHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {embeddedRouteLaneZeroDriverTarget,
             reinterpret_cast<void*>(&embedded_route_lane_zero_driver)},
            g_embeddedRouteLaneZeroDriverHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_embeddedRouteLaneZeroDriverOriginal.store(
        reinterpret_cast<EmbeddedRouteLaneZeroDriver>(
            g_embeddedRouteLaneZeroDriverHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {embeddedRouteTransitionHandlerTarget,
             reinterpret_cast<void*>(&embedded_route_transition_handler)},
            g_embeddedRouteTransitionHandlerHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_embeddedRouteTransitionHandlerOriginal.store(
        reinterpret_cast<EmbeddedRouteTransitionHandler>(
            g_embeddedRouteTransitionHandlerHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {routeAlternateSourceTarget, reinterpret_cast<void*>(&route_alternate_source)},
            g_routeAlternateSourceHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_routeAlternateSourceOriginal.store(
        reinterpret_cast<RouteAlternateSource>(g_routeAlternateSourceHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {routeAlternatePredicateTarget, reinterpret_cast<void*>(&route_alternate_predicate)},
            g_routeAlternatePredicateHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_routeAlternatePredicateOriginal.store(
        reinterpret_cast<RouteAlternatePredicate>(g_routeAlternatePredicateHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {routeAlternateModeTarget, reinterpret_cast<void*>(&route_alternate_mode)},
            g_routeAlternateModeHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_routeAlternateModeOriginal.store(
        reinterpret_cast<RouteAlternateMode>(g_routeAlternateModeHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {routeSelectionIngestTarget, reinterpret_cast<void*>(&route_selection_ingest)},
            g_routeSelectionIngestHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_routeSelectionIngestOriginal.store(
        reinterpret_cast<RouteSelectionIngest>(g_routeSelectionIngestHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {authoredLaunchDispatchTarget, reinterpret_cast<void*>(&authored_launch_dispatch)},
            g_authoredLaunchDispatchHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_authoredLaunchDispatchOriginal.store(
        reinterpret_cast<AuthoredLaunchDispatch>(g_authoredLaunchDispatchHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {launchCommandInitializeTarget,
             reinterpret_cast<void*>(&launch_command_initialize)},
            g_launchCommandInitializeHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_launchCommandInitializeOriginal.store(
        reinterpret_cast<LaunchCommandInitialize>(g_launchCommandInitializeHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {launchCommandDispatchTarget, reinterpret_cast<void*>(&launch_command_dispatch)},
            g_launchCommandDispatchHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_launchCommandDispatchOriginal.store(
        reinterpret_cast<LaunchCommandDispatch>(g_launchCommandDispatchHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {routeModeZeroSetTarget, reinterpret_cast<void*>(&route_mode_zero_set)},
            g_routeModeZeroSetHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_routeModeZeroSetOriginal.store(
        reinterpret_cast<RouteModeSet>(g_routeModeZeroSetHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {routeModeOneSetTarget, reinterpret_cast<void*>(&route_mode_one_set)},
            g_routeModeOneSetHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_routeModeOneSetOriginal.store(
        reinterpret_cast<RouteModeSet>(g_routeModeOneSetHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {omegaRouteLifecycleQueryTarget,
             reinterpret_cast<void*>(&omega_route_lifecycle_query)},
            g_omegaRouteLifecycleQueryHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_omegaRouteLifecycleQueryOriginal.store(
        reinterpret_cast<OmegaRouteLifecycleQuery>(g_omegaRouteLifecycleQueryHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {omegaRouteLifecycleAccessorTarget,
             reinterpret_cast<void*>(&omega_route_lifecycle_accessor)},
            g_omegaRouteLifecycleAccessorHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_omegaRouteLifecycleAccessorOriginal.store(
        reinterpret_cast<OmegaRouteLifecycleAccessor>(
            g_omegaRouteLifecycleAccessorHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {routeStateQueryTarget, reinterpret_cast<void*>(&route_state_query)},
            g_routeStateQueryHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_routeStateQueryOriginal.store(
        reinterpret_cast<RouteStateQuery>(g_routeStateQueryHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {routeStatePublishTarget, reinterpret_cast<void*>(&route_state_publish)},
            g_routeStatePublishHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_routeStatePublishOriginal.store(
        reinterpret_cast<RouteStatePublish>(g_routeStatePublishHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {managerLocalStartTarget, reinterpret_cast<void*>(&manager_local_start)},
            g_managerLocalStartHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_managerLocalStartOriginal.store(
        reinterpret_cast<ManagerLocalStart>(g_managerLocalStartHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {managerAuthoredStartTarget, reinterpret_cast<void*>(&manager_authored_start)},
            g_managerAuthoredStartHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_managerAuthoredStartOriginal.store(
        reinterpret_cast<ManagerAuthoredStart>(g_managerAuthoredStartHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {managerSetupStageATarget, reinterpret_cast<void*>(&manager_setup_stage_a)},
            g_managerSetupStageAHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_managerSetupStageAOriginal.store(
        reinterpret_cast<ManagerSetupStage>(g_managerSetupStageAHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {managerSetupStageBTarget, reinterpret_cast<void*>(&manager_setup_stage_b)},
            g_managerSetupStageBHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_managerSetupStageBOriginal.store(
        reinterpret_cast<ManagerSetupStage>(g_managerSetupStageBHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {managerSetupStageCTarget, reinterpret_cast<void*>(&manager_setup_stage_c)},
            g_managerSetupStageCHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_managerSetupStageCOriginal.store(
        reinterpret_cast<ManagerSetupStage>(g_managerSetupStageCHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {managerSetupStageDTarget, reinterpret_cast<void*>(&manager_setup_stage_d)},
            g_managerSetupStageDHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_managerSetupStageDOriginal.store(
        reinterpret_cast<ManagerSetupStage>(g_managerSetupStageDHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {componentDispatchTarget, reinterpret_cast<void*>(&component_dispatch)},
            g_componentDispatchHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_componentDispatchOriginal.store(
        reinterpret_cast<ComponentDispatch>(g_componentDispatchHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {componentLookupTarget, reinterpret_cast<void*>(&component_lookup)},
            g_componentLookupHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_componentLookupOriginal.store(
        reinterpret_cast<ComponentLookup>(g_componentLookupHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {componentRegisterTarget, reinterpret_cast<void*>(&component_register)},
            g_componentRegisterHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_componentRegisterOriginal.store(
        reinterpret_cast<ComponentRegister>(g_componentRegisterHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {componentReuseTarget, reinterpret_cast<void*>(&component_reuse)},
            g_componentReuseHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_componentReuseOriginal.store(
        reinterpret_cast<ComponentReuse>(g_componentReuseHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {componentMappingTarget, reinterpret_cast<void*>(&component_mapping)},
            g_componentMappingHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_componentMappingOriginal.store(
        reinterpret_cast<ComponentMapping>(g_componentMappingHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {componentBuildTarget, reinterpret_cast<void*>(&component_build)},
            g_componentBuildHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_componentBuildOriginal.store(
        reinterpret_cast<ComponentBuild>(g_componentBuildHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {componentStepTarget, reinterpret_cast<void*>(&component_step)},
            g_componentStepHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_componentStepOriginal.store(
        reinterpret_cast<ComponentStep>(g_componentStepHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {componentTickTarget, reinterpret_cast<void*>(&component_tick)},
            g_componentTickHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_componentTickOriginal.store(
        reinterpret_cast<ComponentTick>(g_componentTickHandle.original),
        std::memory_order_release);
    if (!hooking::detour::install(
            {postComponentSyncTarget, reinterpret_cast<void*>(&post_component_sync)},
            g_postComponentSyncHandle)) {
        uninstall_activity_script_upstream_probe();
        return false;
    }
    g_postComponentSyncOriginal.store(
        reinterpret_cast<PostComponentSync>(g_postComponentSyncHandle.original),
        std::memory_order_release);
    g_transitionObserved.store(0, std::memory_order_release);
    g_slotObserved.store(0, std::memory_order_release);
    g_slotIdentityOneObserved.store(0, std::memory_order_release);
    g_modeObserved.store(0, std::memory_order_release);
    g_authoritySlotMapObserved.store(0, std::memory_order_release);
    g_authorityApplyObserved.store(0, std::memory_order_release);
    g_authorityRefreshObserved.store(0, std::memory_order_release);
    g_authorityRefreshGateObserved.store(0, std::memory_order_release);
    g_authorityBootstrapObserved.store(0, std::memory_order_release);
    g_authoritySnapshotBootstrapObserved.store(0, std::memory_order_release);
    g_assignmentProviderUpdateObserved.store(0, std::memory_order_release);
    g_assignmentProviderStateConsumerObserved.store(0, std::memory_order_release);
    g_matchmakingLanePolicyMaterializerObserved.store(0, std::memory_order_release);
    g_matchmakingLaneRebuildObserved.store(0, std::memory_order_release);
    g_assignmentWatcherUpdateObserved.store(0, std::memory_order_release);
    g_assignmentWatcherOpeningObserved.store(0, std::memory_order_release);
    g_managerLookupObserved.store(0, std::memory_order_release);
    g_lifecycleEventObserved.store(0, std::memory_order_release);
    g_managerEnsureObserved.store(0, std::memory_order_release);
    g_identityRequestObserved.store(0, std::memory_order_release);
    g_identityEnableObserved.store(0, std::memory_order_release);
    g_identityDescriptorCopyObserved.store(0, std::memory_order_release);
    g_sessionDescriptionStageObserved.store(0, std::memory_order_release);
    g_managedSessionMigrationObserved.store(0, std::memory_order_release);
    g_managerTableSnapshotObserved.store(0, std::memory_order_release);
    g_transitionDispatchObserved.store(0, std::memory_order_release);
    g_wireDispatchObserved.store(0, std::memory_order_release);
    g_activityClientUpdateObserved.store(0, std::memory_order_release);
    g_activityClientPumpObserved.store(0, std::memory_order_release);
    g_activityReceiverLookupObserved.store(0, std::memory_order_release);
    g_activityRosterApplyObserved.store(0, std::memory_order_release);
    g_activityPeerDispatchObserved.store(0, std::memory_order_release);
    g_activityReceiverActivateObserved.store(0, std::memory_order_release);
    g_activityReceiverBindObserved.store(0, std::memory_order_release);
    g_activityReceiverCreatorObserved.store(0, std::memory_order_release);
    g_managerUpdateLoopObserved.store(0, std::memory_order_release);
    g_managerUpdateLoopIdentityOneObserved.store(0, std::memory_order_release);
    g_managerUpdateLoopIdentityTwoObserved.store(0, std::memory_order_release);
    g_managerUpdateGateIdentityOneObserved.store(0, std::memory_order_release);
    g_managerUpdateGateIdentityTwoObserved.store(0, std::memory_order_release);
    g_managerModeSetObserved.store(0, std::memory_order_release);
    g_routeDescriptorObserved.store(0, std::memory_order_release);
    g_currentSelectionPublishInstallAttempted.store(false, std::memory_order_release);
    g_currentSelectionPublishObserved.store(0, std::memory_order_release);
    for (auto& signature : g_routeDescriptorLastSignature) {
        signature.store(0, std::memory_order_release);
    }
    g_routeCommitObserved.store(0, std::memory_order_release);
    g_routeCommitOpeningObserved.store(0, std::memory_order_release);
    g_routeUpdateObserved.store(0, std::memory_order_release);
    g_routeUpdateOpeningObserved.store(0, std::memory_order_release);
    g_defaultRouteZeroInitializerObserved.store(0, std::memory_order_release);
    g_defaultRouteZeroInitializerDeferred.store(0, std::memory_order_release);
    g_defaultRouteReferencePredicateObserved.store(0, std::memory_order_release);
    g_defaultRoutePairPredicateObserved.store(0, std::memory_order_release);
    g_defaultRouteRequiredPredicateObserved.store(0, std::memory_order_release);
    g_defaultRouteCrossManagerPredicateObserved.store(0, std::memory_order_release);
    g_laneOneReadyScanObserved.store(0, std::memory_order_release);
    g_laneOneActiveCountObserved.store(0, std::memory_order_release);
    g_laneOneSelectionReadyObserved.store(0, std::memory_order_release);
    g_laneOneAuthoredInitializerObserved.store(0, std::memory_order_release);
    g_routeFlagUpdateObserved.store(0, std::memory_order_release);
    g_routeElapsedTimeObserved.store(0, std::memory_order_release);
    g_routeTimeoutConfigObserved.store(0, std::memory_order_release);
    g_routeAlternateUpdateObserved.store(0, std::memory_order_release);
    g_routeStateEvaluatorObserved.store(0, std::memory_order_release);
    g_routeParentDispatchObserved.store(0, std::memory_order_release);
    g_routeParentState.store(nullptr, std::memory_order_release);
    g_embeddedRouteLaneZeroState.store(nullptr, std::memory_order_release);
    g_routeSelectorVariantLookupObserved.store(0, std::memory_order_release);
    g_routeStateArmObserved.store(0, std::memory_order_release);
    g_embeddedRouteStatePhaseSetObserved.store(0, std::memory_order_release);
    g_embeddedRouteStateInitializeObserved.store(0, std::memory_order_release);
    g_embeddedRouteStateResetObserved.store(0, std::memory_order_release);
    g_embeddedRouteLaneZeroDriverObserved.store(0, std::memory_order_release);
    g_embeddedRouteLaneZeroDriverOpeningObserved.store(0, std::memory_order_release);
    g_embeddedRouteTransitionHandlerObserved.store(0, std::memory_order_release);
    g_embeddedRouteSessionActivityObserved.store(0, std::memory_order_release);
    g_embeddedRouteStateStatusObserved.store(0, std::memory_order_release);
    g_embeddedRouteContextSkipPredicateObserved.store(0, std::memory_order_release);
    g_embeddedRouteLaneAvailablePredicateObserved.store(0, std::memory_order_release);
    g_embeddedRouteAuthoredIdentityPredicateObserved.store(0, std::memory_order_release);
    g_embeddedRouteLocalIdentityPredicateObserved.store(0, std::memory_order_release);
    g_embeddedRouteIdentityProviderRecordLookupObserved.store(0,
                                                               std::memory_order_release);
    g_embeddedRouteIdentityProviderLookupObserved.store(0, std::memory_order_release);
    g_embeddedRouteIdentityProviderControlObserved.store(0, std::memory_order_release);
    g_embeddedRouteProvider266Outcome.store(0, std::memory_order_release);
    g_embeddedRouteObjectTransitionObserved.store(0, std::memory_order_release);
    g_omegaForestRouteTraceArmed.store(false, std::memory_order_release);
    g_omegaRouteOwner.store(nullptr, std::memory_order_release);
    g_omegaRouteEntrySnapshotDumped.store(false, std::memory_order_release);
    g_omegaRouteState4SnapshotDumped.store(false, std::memory_order_release);
    g_omegaRoutePreviousOwner.store(nullptr, std::memory_order_release);
    g_omegaRoutePreviousCurrent.store(INT32_MIN, std::memory_order_release);
    g_omegaRoutePreviousCurrentDetail.store(INT32_MIN, std::memory_order_release);
    g_omegaRoutePreviousPending.store(INT32_MIN, std::memory_order_release);
    g_omegaRoutePreviousPendingDetail.store(INT32_MIN, std::memory_order_release);
    g_embeddedRouteObject282PredicateDumped.store(false, std::memory_order_release);
    g_embeddedRouteObject282ActionDumped.store(false, std::memory_order_release);
    g_embeddedRouteIdentityProviderLookupInstallAttempted.store(false,
                                                                 std::memory_order_release);
    g_embeddedRouteIdentityProviderLookupCodeDumped.store(false,
                                                           std::memory_order_release);
    g_routeAlternateDecisionObserved.store(0, std::memory_order_release);
    g_routePhaseTimeoutMs.store(-1, std::memory_order_release);
    g_routeSelectionTimeoutMs.store(-1, std::memory_order_release);
    g_routePhaseTimeoutPassedObserved.store(false, std::memory_order_release);
    g_routeSelectionTimeoutPassedObserved.store(false, std::memory_order_release);
    g_routeSelectionIngestObserved.store(0, std::memory_order_release);
    g_authoredLaunchDispatchObserved.store(0, std::memory_order_release);
    g_launchCommandInitializeObserved.store(0, std::memory_order_release);
    g_launchCommandDispatchObserved.store(0, std::memory_order_release);
    g_launchProducerGuardLast.store(0xFFU, std::memory_order_release);
    g_routeModeZeroSetObserved.store(0, std::memory_order_release);
    g_routeModeOneSetObserved.store(0, std::memory_order_release);
    g_omegaRouteLifecycleQueryObserved.store(0, std::memory_order_release);
    g_omegaRouteLifecycleQueryLast.store(INT32_MIN, std::memory_order_release);
    g_omegaRouteLifecycleAccessorObserved.store(0, std::memory_order_release);
    for (auto& caller : g_omegaRouteLifecycleAccessorCallers) {
        caller.store(0U, std::memory_order_release);
    }
    g_routeStateQueryObserved.store(0, std::memory_order_release);
    g_routeStateQueryLastSignature.store(0, std::memory_order_release);
    g_routeStatePublishObserved.store(0, std::memory_order_release);
    g_managerStartObserved.store(0, std::memory_order_release);
    g_managerIdentityOneModeTransitionObserved.store(0, std::memory_order_release);
    g_managerIdentityOneLastMode.store(INT32_MIN, std::memory_order_release);
    g_managerIdentityOneLastPointer.store(nullptr, std::memory_order_release);
    g_bootstrapRequestAttempted.store(false, std::memory_order_release);
    g_towerfallLifecycleStarted.store(false, std::memory_order_release);
    g_omegaLifecycleManager.store(nullptr, std::memory_order_release);
    g_omegaLifecycleStarted.store(false, std::memory_order_release);
    g_omegaLifecycleComplete.store(false, std::memory_order_release);
    g_omegaPendingConsumerAttempted.store(false, std::memory_order_release);
    g_omegaLifecycleCalls.store(0U, std::memory_order_release);
    g_omegaStageTwoStalls.store(0U, std::memory_order_release);
    g_omegaStageTwoFirstTick.store(0U, std::memory_order_release);
    g_managerSetupStageObserved.store(0, std::memory_order_release);
    g_componentDispatchAttempted.store(false, std::memory_order_release);
    g_activitySetupComplete.store(false, std::memory_order_release);
    g_activityWorldStarted.store(false, std::memory_order_release);
    g_activityClientCandidate.store(nullptr, std::memory_order_release);
    g_joinedActivityClient.store(nullptr, std::memory_order_release);
    g_activityReceiverSnapshotAttempted.store(false, std::memory_order_release);
    g_componentDispatchIdentityOneObserved.store(0, std::memory_order_release);
    g_componentDispatchIdentityTwoObserved.store(0, std::memory_order_release);
    g_componentLookupObserved.store(0, std::memory_order_release);
    g_componentLookupIdentityOneObserved.store(0, std::memory_order_release);
    g_componentLookupIdentityTwoObserved.store(0, std::memory_order_release);
    g_componentRegisterObserved.store(0, std::memory_order_release);
    g_componentRegisterIdentityTwoObserved.store(0, std::memory_order_release);
    g_componentReuseObserved.store(0, std::memory_order_release);
    g_componentReuseIdentityTwoObserved.store(0, std::memory_order_release);
    g_componentMappingObserved.store(0, std::memory_order_release);
    g_componentMappingIdentityTwoObserved.store(0, std::memory_order_release);
    g_componentBuildObserved.store(0, std::memory_order_release);
    g_componentBuildIdentityTwoObserved.store(0, std::memory_order_release);
    g_componentSlotStateObserved.store(0, std::memory_order_release);
    g_componentSlotStateLastSignature.store(~std::uint64_t{0}, std::memory_order_release);
    g_componentSlotStateIdentityTwoLastSignature.store(~std::uint64_t{0},
                                                       std::memory_order_release);
    g_componentStepObserved.store(0, std::memory_order_release);
    g_componentStepIdentityTwoObserved.store(0, std::memory_order_release);
    g_componentTickObserved.store(0, std::memory_order_release);
    g_postComponentSyncObserved.store(0, std::memory_order_release);
    g_launchProducerWindowState.store(0U, std::memory_order_release);
    g_launchProducerWindowTicks.store(0U, std::memory_order_release);
    g_launchProducerWindowReassertions.store(0U, std::memory_order_release);
    g_launchProducerSelectionReady.store(false, std::memory_order_release);
    g_launchProducerSelectionDescriptor.fill(std::byte{});
    g_launchProducerSelectionDescriptorReady.store(false, std::memory_order_release);
    g_launchProducerReadinessRequestAttempted.store(false, std::memory_order_release);
    g_launchProducerEntryObserved.store(0U, std::memory_order_release);
    g_launchProducerEntryLastSignature.store(~std::uint64_t{0},
                                             std::memory_order_release);
    g_launchProducerReadyCallbackObserved.store(0U, std::memory_order_release);
    g_activityEvent46Observed.store(0U, std::memory_order_release);
    g_activityEvent46RegisteredProducerObserved.store(0U, std::memory_order_release);
    g_activityEvent46ProducerResetObserved.store(0U, std::memory_order_release);
    g_activityEvent46ProducerInitializeObserved.store(0U, std::memory_order_release);
    g_activityEvent46ProducerAvailableObserved.store(0U, std::memory_order_release);
    g_activityEvent46ProducerRequestObserved.store(0U, std::memory_order_release);
    g_activityEvent46ProducerTickObserved.store(0U, std::memory_order_release);
    g_activityEvent46RequestFlowObserved.store(0U, std::memory_order_release);
    g_activityEvent46ActionDispatchObserved.store(0U, std::memory_order_release);
    g_activityEvent46RequestPredicateObserved.store(0U, std::memory_order_release);
    g_launchProducerGuardLast.store(0xFFU, std::memory_order_release);
    observe_launch_producer_guard("install");
    core::log::write(core::log::Channel::client,
                     core::log::Level::info,
                     "ev=bootflow stage=activity_script_upstream_probe result=ok mode=observe");
    return true;
}

void uninstall_activity_script_upstream_probe() noexcept {
    close_launch_producer_window("uninstall");
    if (g_upstreamSelectionUpdateHandle.attached) {
        (void)hooking::detour::uninstall(g_upstreamSelectionUpdateHandle);
    }
    if (g_upstreamSelectionPublishHandle.attached) {
        (void)hooking::detour::uninstall(g_upstreamSelectionPublishHandle);
    }
    if (g_currentSelectionPublishHandle.attached) {
        (void)hooking::detour::uninstall(g_currentSelectionPublishHandle);
    }
    if (g_postComponentSyncHandle.attached) {
        (void)hooking::detour::uninstall(g_postComponentSyncHandle);
    }
    if (g_componentTickHandle.attached) {
        (void)hooking::detour::uninstall(g_componentTickHandle);
    }
    if (g_componentStepHandle.attached) {
        (void)hooking::detour::uninstall(g_componentStepHandle);
    }
    if (g_componentBuildHandle.attached) {
        (void)hooking::detour::uninstall(g_componentBuildHandle);
    }
    if (g_componentMappingHandle.attached) {
        (void)hooking::detour::uninstall(g_componentMappingHandle);
    }
    if (g_componentReuseHandle.attached) {
        (void)hooking::detour::uninstall(g_componentReuseHandle);
    }
    if (g_componentRegisterHandle.attached) {
        (void)hooking::detour::uninstall(g_componentRegisterHandle);
    }
    if (g_componentLookupHandle.attached) {
        (void)hooking::detour::uninstall(g_componentLookupHandle);
    }
    if (g_componentDispatchHandle.attached) {
        (void)hooking::detour::uninstall(g_componentDispatchHandle);
    }
    if (g_managerSetupStageDHandle.attached) {
        (void)hooking::detour::uninstall(g_managerSetupStageDHandle);
    }
    if (g_managerSetupStageCHandle.attached) {
        (void)hooking::detour::uninstall(g_managerSetupStageCHandle);
    }
    if (g_managerSetupStageBHandle.attached) {
        (void)hooking::detour::uninstall(g_managerSetupStageBHandle);
    }
    if (g_managerSetupStageAHandle.attached) {
        (void)hooking::detour::uninstall(g_managerSetupStageAHandle);
    }
    if (g_managerAuthoredStartHandle.attached) {
        (void)hooking::detour::uninstall(g_managerAuthoredStartHandle);
    }
    if (g_managerLocalStartHandle.attached) {
        (void)hooking::detour::uninstall(g_managerLocalStartHandle);
    }
    if (g_routeStatePublishHandle.attached) {
        (void)hooking::detour::uninstall(g_routeStatePublishHandle);
    }
    if (g_routeStateQueryHandle.attached) {
        (void)hooking::detour::uninstall(g_routeStateQueryHandle);
    }
    if (g_omegaRouteLifecycleAccessorHandle.attached) {
        (void)hooking::detour::uninstall(g_omegaRouteLifecycleAccessorHandle);
    }
    if (g_omegaRouteLifecycleQueryHandle.attached) {
        (void)hooking::detour::uninstall(g_omegaRouteLifecycleQueryHandle);
    }
    if (g_routeModeOneSetHandle.attached) {
        (void)hooking::detour::uninstall(g_routeModeOneSetHandle);
    }
    if (g_routeModeZeroSetHandle.attached) {
        (void)hooking::detour::uninstall(g_routeModeZeroSetHandle);
    }
    if (g_launchCommandDispatchHandle.attached) {
        (void)hooking::detour::uninstall(g_launchCommandDispatchHandle);
    }
    if (g_launchCommandInitializeHandle.attached) {
        (void)hooking::detour::uninstall(g_launchCommandInitializeHandle);
    }
    if (g_authoredLaunchDispatchHandle.attached) {
        (void)hooking::detour::uninstall(g_authoredLaunchDispatchHandle);
    }
    if (g_routeSelectionIngestHandle.attached) {
        (void)hooking::detour::uninstall(g_routeSelectionIngestHandle);
    }
    if (g_routeAlternateModeHandle.attached) {
        (void)hooking::detour::uninstall(g_routeAlternateModeHandle);
    }
    if (g_routeAlternatePredicateHandle.attached) {
        (void)hooking::detour::uninstall(g_routeAlternatePredicateHandle);
    }
    if (g_routeAlternateSourceHandle.attached) {
        (void)hooking::detour::uninstall(g_routeAlternateSourceHandle);
    }
    if (g_embeddedRouteTransitionHandlerHandle.attached) {
        (void)hooking::detour::uninstall(g_embeddedRouteTransitionHandlerHandle);
    }
    if (g_embeddedRouteLaneZeroDriverHandle.attached) {
        (void)hooking::detour::uninstall(g_embeddedRouteLaneZeroDriverHandle);
    }
    if (g_embeddedRouteObjectTransitionHandle.attached) {
        (void)hooking::detour::uninstall(g_embeddedRouteObjectTransitionHandle);
    }
    if (g_embeddedRouteIdentityProviderLookupHandle.attached) {
        (void)hooking::detour::uninstall(g_embeddedRouteIdentityProviderLookupHandle);
    }
    if (g_embeddedRouteIdentityProviderRecordLookupHandle.attached) {
        (void)hooking::detour::uninstall(g_embeddedRouteIdentityProviderRecordLookupHandle);
    }
    if (g_embeddedRouteLocalIdentityPredicateHandle.attached) {
        (void)hooking::detour::uninstall(g_embeddedRouteLocalIdentityPredicateHandle);
    }
    if (g_embeddedRouteAuthoredIdentityPredicateHandle.attached) {
        (void)hooking::detour::uninstall(g_embeddedRouteAuthoredIdentityPredicateHandle);
    }
    if (g_embeddedRouteLaneAvailablePredicateHandle.attached) {
        (void)hooking::detour::uninstall(g_embeddedRouteLaneAvailablePredicateHandle);
    }
    if (g_embeddedRouteContextSkipPredicateHandle.attached) {
        (void)hooking::detour::uninstall(g_embeddedRouteContextSkipPredicateHandle);
    }
    if (g_embeddedRouteStateStatusHandle.attached) {
        (void)hooking::detour::uninstall(g_embeddedRouteStateStatusHandle);
    }
    if (g_embeddedRouteSessionActivityHandle.attached) {
        (void)hooking::detour::uninstall(g_embeddedRouteSessionActivityHandle);
    }
    if (g_embeddedRouteStateResetHandle.attached) {
        (void)hooking::detour::uninstall(g_embeddedRouteStateResetHandle);
    }
    if (g_embeddedRouteStateInitializeHandle.attached) {
        (void)hooking::detour::uninstall(g_embeddedRouteStateInitializeHandle);
    }
    if (g_embeddedRouteStatePhaseSetHandle.attached) {
        (void)hooking::detour::uninstall(g_embeddedRouteStatePhaseSetHandle);
    }
    if (g_routeStateArmHandle.attached) {
        (void)hooking::detour::uninstall(g_routeStateArmHandle);
    }
    if (g_routeSelectorVariantLookupHandle.attached) {
        (void)hooking::detour::uninstall(g_routeSelectorVariantLookupHandle);
    }
    if (g_routeParentDispatchHandle.attached) {
        (void)hooking::detour::uninstall(g_routeParentDispatchHandle);
    }
    if (g_routeStateEvaluatorHandle.attached) {
        (void)hooking::detour::uninstall(g_routeStateEvaluatorHandle);
    }
    if (g_routeAlternateUpdateHandle.attached) {
        (void)hooking::detour::uninstall(g_routeAlternateUpdateHandle);
    }
    if (g_routeTimeoutConfigHandle.attached) {
        (void)hooking::detour::uninstall(g_routeTimeoutConfigHandle);
    }
    if (g_routeElapsedTimeHandle.attached) {
        (void)hooking::detour::uninstall(g_routeElapsedTimeHandle);
    }
    if (g_routeFlagUpdateHandle.attached) {
        (void)hooking::detour::uninstall(g_routeFlagUpdateHandle);
    }
    if (g_laneOneAuthoredInitializerHandle.attached) {
        (void)hooking::detour::uninstall(g_laneOneAuthoredInitializerHandle);
    }
    if (g_laneOneSelectionReadyHandle.attached) {
        (void)hooking::detour::uninstall(g_laneOneSelectionReadyHandle);
    }
    if (g_laneOneActiveCountHandle.attached) {
        (void)hooking::detour::uninstall(g_laneOneActiveCountHandle);
    }
    if (g_laneOneReadyScanHandle.attached) {
        (void)hooking::detour::uninstall(g_laneOneReadyScanHandle);
    }
    if (g_defaultRouteCrossManagerPredicateHandle.attached) {
        (void)hooking::detour::uninstall(g_defaultRouteCrossManagerPredicateHandle);
    }
    if (g_defaultRouteRequiredPredicateHandle.attached) {
        (void)hooking::detour::uninstall(g_defaultRouteRequiredPredicateHandle);
    }
    if (g_defaultRoutePairPredicateHandle.attached) {
        (void)hooking::detour::uninstall(g_defaultRoutePairPredicateHandle);
    }
    if (g_defaultRouteReferencePredicateHandle.attached) {
        (void)hooking::detour::uninstall(g_defaultRouteReferencePredicateHandle);
    }
    if (g_defaultRouteZeroInitializerHandle.attached) {
        (void)hooking::detour::uninstall(g_defaultRouteZeroInitializerHandle);
    }
    if (g_routeCommitHandle.attached) {
        (void)hooking::detour::uninstall(g_routeCommitHandle);
    }
    if (g_routeUpdateHandle.attached) {
        (void)hooking::detour::uninstall(g_routeUpdateHandle);
    }
    if (g_routeDescriptorLookupHandle.attached) {
        (void)hooking::detour::uninstall(g_routeDescriptorLookupHandle);
    }
    if (g_managerModeSetHandle.attached) {
        (void)hooking::detour::uninstall(g_managerModeSetHandle);
    }
    if (g_activityEvent46RequestPredicateHandle.attached) {
        (void)hooking::detour::uninstall(g_activityEvent46RequestPredicateHandle);
    }
    if (g_activityEvent46ActionDispatchHandle.attached) {
        (void)hooking::detour::uninstall(g_activityEvent46ActionDispatchHandle);
    }
    if (g_activityEvent46RequestFlowHandle.attached) {
        (void)hooking::detour::uninstall(g_activityEvent46RequestFlowHandle);
    }
    if (g_activityEvent46ProducerTickHandle.attached) {
        (void)hooking::detour::uninstall(g_activityEvent46ProducerTickHandle);
    }
    if (g_activityEvent46ProducerRequestHandle.attached) {
        (void)hooking::detour::uninstall(g_activityEvent46ProducerRequestHandle);
    }
    if (g_activityEvent46ProducerAvailableHandle.attached) {
        (void)hooking::detour::uninstall(g_activityEvent46ProducerAvailableHandle);
    }
    if (g_activityEvent46ProducerInitializeHandle.attached) {
        (void)hooking::detour::uninstall(g_activityEvent46ProducerInitializeHandle);
    }
    if (g_activityEvent46ProducerResetHandle.attached) {
        (void)hooking::detour::uninstall(g_activityEvent46ProducerResetHandle);
    }
    if (g_activityEvent46RegisteredProducerHandle.attached) {
        (void)hooking::detour::uninstall(g_activityEvent46RegisteredProducerHandle);
    }
    if (g_activityEventDispatchPayloadHandle.attached) {
        (void)hooking::detour::uninstall(g_activityEventDispatchPayloadHandle);
    }
    if (g_activityEventDispatchLocalHandle.attached) {
        (void)hooking::detour::uninstall(g_activityEventDispatchLocalHandle);
    }
    if (g_launchProducerReadyCallbackHandle.attached) {
        (void)hooking::detour::uninstall(g_launchProducerReadyCallbackHandle);
    }
    if (g_launchProducerGuardSetterHandle.attached) {
        (void)hooking::detour::uninstall(g_launchProducerGuardSetterHandle);
    }
    if (g_launchProducerHandle.attached) {
        (void)hooking::detour::uninstall(g_launchProducerHandle);
    }
    if (g_managerUpdateGateHandle.attached) {
        (void)hooking::detour::uninstall(g_managerUpdateGateHandle);
    }
    if (g_managerUpdateLoopHandle.attached) {
        (void)hooking::detour::uninstall(g_managerUpdateLoopHandle);
    }
    if (g_activityReceiverCreatorHandle.attached) {
        (void)hooking::detour::uninstall(g_activityReceiverCreatorHandle);
    }
    if (g_activityReceiverBindHandle.attached) {
        (void)hooking::detour::uninstall(g_activityReceiverBindHandle);
    }
    if (g_activityReceiverActivateHandle.attached) {
        (void)hooking::detour::uninstall(g_activityReceiverActivateHandle);
    }
    if (g_activityPeerDispatchHandle.attached) {
        (void)hooking::detour::uninstall(g_activityPeerDispatchHandle);
    }
    if (g_activityRosterApplyHandle.attached) {
        (void)hooking::detour::uninstall(g_activityRosterApplyHandle);
    }
    if (g_activityReceiverLookupHandle.attached) {
        (void)hooking::detour::uninstall(g_activityReceiverLookupHandle);
    }
    if (g_activityClientUpdateHandle.attached) {
        (void)hooking::detour::uninstall(g_activityClientUpdateHandle);
    }
    if (g_wireDispatchHandle.attached) {
        (void)hooking::detour::uninstall(g_wireDispatchHandle);
    }
    if (g_transitionDispatchHandle.attached) {
        (void)hooking::detour::uninstall(g_transitionDispatchHandle);
    }
    if (g_managedSessionMigrationHandle.attached) {
        (void)hooking::detour::uninstall(g_managedSessionMigrationHandle);
    }
    if (g_sessionDescriptionStageHandle.attached) {
        (void)hooking::detour::uninstall(g_sessionDescriptionStageHandle);
    }
    if (g_identityDescriptorCopyHandle.attached) {
        (void)hooking::detour::uninstall(g_identityDescriptorCopyHandle);
    }
    if (g_managerActivateHandle.attached) {
        (void)hooking::detour::uninstall(g_managerActivateHandle);
    }
    if (g_hostHandoffApplyHandle.attached) {
        (void)hooking::detour::uninstall(g_hostHandoffApplyHandle);
    }
    if (g_memberRecordIndexHandle.attached) {
        (void)hooking::detour::uninstall(g_memberRecordIndexHandle);
    }
    if (g_managerLookupHandle.attached) {
        (void)hooking::detour::uninstall(g_managerLookupHandle);
    }
    if (g_lifecycleEventHandle.attached) {
        (void)hooking::detour::uninstall(g_lifecycleEventHandle);
    }
    if (g_identityEnableHandle.attached) {
        (void)hooking::detour::uninstall(g_identityEnableHandle);
    }
    if (g_identityRequestHandle.attached) {
        (void)hooking::detour::uninstall(g_identityRequestHandle);
    }
    if (g_managerEnsureHandle.attached) {
        (void)hooking::detour::uninstall(g_managerEnsureHandle);
    }
    if (g_authorityRefreshHandle.attached) {
        (void)hooking::detour::uninstall(g_authorityRefreshHandle);
    }
    if (g_assignmentWatcherUpdateHandle.attached) {
        (void)hooking::detour::uninstall(g_assignmentWatcherUpdateHandle);
    }
    if (g_assignmentProviderUpdateHandle.attached) {
        (void)hooking::detour::uninstall(g_assignmentProviderUpdateHandle);
    }
    if (g_assignmentProviderStateConsumerHandle.attached) {
        (void)hooking::detour::uninstall(g_assignmentProviderStateConsumerHandle);
    }
    if (g_matchmakingLaneRebuildHandle.attached) {
        (void)hooking::detour::uninstall(g_matchmakingLaneRebuildHandle);
    }
    if (g_matchmakingLanePolicyMaterializerHandle.attached) {
        (void)hooking::detour::uninstall(g_matchmakingLanePolicyMaterializerHandle);
    }
    if (g_authoritySnapshotGateHandle.attached) {
        (void)hooking::detour::uninstall(g_authoritySnapshotGateHandle);
    }
    if (g_authorityAnyUnassignedGateHandle.attached) {
        (void)hooking::detour::uninstall(g_authorityAnyUnassignedGateHandle);
    }
    if (g_authorityAnyAssignedGateHandle.attached) {
        (void)hooking::detour::uninstall(g_authorityAnyAssignedGateHandle);
    }
    if (g_authorityApplyHandle.attached) {
        (void)hooking::detour::uninstall(g_authorityApplyHandle);
    }
    if (g_authoritySlotMapHandle.attached) {
        (void)hooking::detour::uninstall(g_authoritySlotMapHandle);
    }
    if (g_modeHandle.attached) {
        (void)hooking::detour::uninstall(g_modeHandle);
    }
    if (g_slotHandle.attached) {
        (void)hooking::detour::uninstall(g_slotHandle);
    }
    if (g_transitionHandle.attached) {
        (void)hooking::detour::uninstall(g_transitionHandle);
    }
    clear_originals();
    g_transitionObserved.store(0, std::memory_order_release);
    g_slotObserved.store(0, std::memory_order_release);
    g_slotIdentityOneObserved.store(0, std::memory_order_release);
    g_modeObserved.store(0, std::memory_order_release);
    g_authoritySlotMapObserved.store(0, std::memory_order_release);
    g_authorityApplyObserved.store(0, std::memory_order_release);
    g_authorityRefreshObserved.store(0, std::memory_order_release);
    g_authorityRefreshGateObserved.store(0, std::memory_order_release);
    g_authorityBootstrapObserved.store(0, std::memory_order_release);
    g_authoritySnapshotBootstrapObserved.store(0, std::memory_order_release);
    g_assignmentProviderUpdateObserved.store(0, std::memory_order_release);
    g_assignmentProviderStateConsumerObserved.store(0, std::memory_order_release);
    g_matchmakingLanePolicyMaterializerObserved.store(0, std::memory_order_release);
    g_matchmakingLaneRebuildObserved.store(0, std::memory_order_release);
    g_assignmentWatcherUpdateObserved.store(0, std::memory_order_release);
    g_assignmentWatcherOpeningObserved.store(0, std::memory_order_release);
    g_managerLookupObserved.store(0, std::memory_order_release);
    g_lifecycleEventObserved.store(0, std::memory_order_release);
    g_managerEnsureObserved.store(0, std::memory_order_release);
    g_identityRequestObserved.store(0, std::memory_order_release);
    g_identityEnableObserved.store(0, std::memory_order_release);
    g_identityDescriptorCopyObserved.store(0, std::memory_order_release);
    g_sessionDescriptionStageObserved.store(0, std::memory_order_release);
    g_managedSessionMigrationObserved.store(0, std::memory_order_release);
    g_managerTableSnapshotObserved.store(0, std::memory_order_release);
    g_transitionDispatchObserved.store(0, std::memory_order_release);
    g_wireDispatchObserved.store(0, std::memory_order_release);
    g_activityClientUpdateObserved.store(0, std::memory_order_release);
    g_activityClientPumpObserved.store(0, std::memory_order_release);
    g_activityReceiverLookupObserved.store(0, std::memory_order_release);
    g_activityRosterApplyObserved.store(0, std::memory_order_release);
    g_activityPeerDispatchObserved.store(0, std::memory_order_release);
    g_activityReceiverActivateObserved.store(0, std::memory_order_release);
    g_activityReceiverBindObserved.store(0, std::memory_order_release);
    g_activityReceiverCreatorObserved.store(0, std::memory_order_release);
    g_managerUpdateLoopObserved.store(0, std::memory_order_release);
    g_managerUpdateLoopIdentityOneObserved.store(0, std::memory_order_release);
    g_managerUpdateLoopIdentityTwoObserved.store(0, std::memory_order_release);
    g_managerUpdateGateIdentityOneObserved.store(0, std::memory_order_release);
    g_managerUpdateGateIdentityTwoObserved.store(0, std::memory_order_release);
    g_managerModeSetObserved.store(0, std::memory_order_release);
    g_routeDescriptorObserved.store(0, std::memory_order_release);
    g_currentSelectionPublishInstallAttempted.store(false, std::memory_order_release);
    g_currentSelectionPublishObserved.store(0, std::memory_order_release);
    for (auto& signature : g_routeDescriptorLastSignature) {
        signature.store(0, std::memory_order_release);
    }
    g_routeCommitObserved.store(0, std::memory_order_release);
    g_routeCommitOpeningObserved.store(0, std::memory_order_release);
    g_routeUpdateObserved.store(0, std::memory_order_release);
    g_routeUpdateOpeningObserved.store(0, std::memory_order_release);
    g_defaultRouteZeroInitializerObserved.store(0, std::memory_order_release);
    g_defaultRouteZeroInitializerDeferred.store(0, std::memory_order_release);
    g_defaultRouteReferencePredicateObserved.store(0, std::memory_order_release);
    g_defaultRoutePairPredicateObserved.store(0, std::memory_order_release);
    g_defaultRouteRequiredPredicateObserved.store(0, std::memory_order_release);
    g_defaultRouteCrossManagerPredicateObserved.store(0, std::memory_order_release);
    g_laneOneReadyScanObserved.store(0, std::memory_order_release);
    g_laneOneActiveCountObserved.store(0, std::memory_order_release);
    g_laneOneSelectionReadyObserved.store(0, std::memory_order_release);
    g_laneOneAuthoredInitializerObserved.store(0, std::memory_order_release);
    g_routeFlagUpdateObserved.store(0, std::memory_order_release);
    g_routeElapsedTimeObserved.store(0, std::memory_order_release);
    g_routeTimeoutConfigObserved.store(0, std::memory_order_release);
    g_routeAlternateUpdateObserved.store(0, std::memory_order_release);
    g_routeStateEvaluatorObserved.store(0, std::memory_order_release);
    g_routeParentDispatchObserved.store(0, std::memory_order_release);
    g_routeParentState.store(nullptr, std::memory_order_release);
    g_embeddedRouteLaneZeroState.store(nullptr, std::memory_order_release);
    g_routeSelectorVariantLookupObserved.store(0, std::memory_order_release);
    g_routeStateArmObserved.store(0, std::memory_order_release);
    g_embeddedRouteStatePhaseSetObserved.store(0, std::memory_order_release);
    g_embeddedRouteStateInitializeObserved.store(0, std::memory_order_release);
    g_embeddedRouteStateResetObserved.store(0, std::memory_order_release);
    g_embeddedRouteLaneZeroDriverObserved.store(0, std::memory_order_release);
    g_embeddedRouteLaneZeroDriverOpeningObserved.store(0, std::memory_order_release);
    g_embeddedRouteTransitionHandlerObserved.store(0, std::memory_order_release);
    g_embeddedRouteSessionActivityObserved.store(0, std::memory_order_release);
    g_embeddedRouteStateStatusObserved.store(0, std::memory_order_release);
    g_embeddedRouteContextSkipPredicateObserved.store(0, std::memory_order_release);
    g_embeddedRouteLaneAvailablePredicateObserved.store(0, std::memory_order_release);
    g_embeddedRouteAuthoredIdentityPredicateObserved.store(0, std::memory_order_release);
    g_embeddedRouteLocalIdentityPredicateObserved.store(0, std::memory_order_release);
    g_embeddedRouteIdentityProviderRecordLookupObserved.store(0,
                                                               std::memory_order_release);
    g_embeddedRouteIdentityProviderLookupObserved.store(0, std::memory_order_release);
    g_embeddedRouteIdentityProviderControlObserved.store(0, std::memory_order_release);
    g_embeddedRouteProvider266Outcome.store(0, std::memory_order_release);
    g_embeddedRouteObjectTransitionObserved.store(0, std::memory_order_release);
    g_omegaForestRouteTraceArmed.store(false, std::memory_order_release);
    g_omegaRouteOwner.store(nullptr, std::memory_order_release);
    g_omegaRouteEntrySnapshotDumped.store(false, std::memory_order_release);
    g_omegaRouteState4SnapshotDumped.store(false, std::memory_order_release);
    g_omegaRoutePreviousOwner.store(nullptr, std::memory_order_release);
    g_omegaRoutePreviousCurrent.store(INT32_MIN, std::memory_order_release);
    g_omegaRoutePreviousCurrentDetail.store(INT32_MIN, std::memory_order_release);
    g_omegaRoutePreviousPending.store(INT32_MIN, std::memory_order_release);
    g_omegaRoutePreviousPendingDetail.store(INT32_MIN, std::memory_order_release);
    g_embeddedRouteObject282PredicateDumped.store(false, std::memory_order_release);
    g_embeddedRouteObject282ActionDumped.store(false, std::memory_order_release);
    g_embeddedRouteIdentityProviderLookupInstallAttempted.store(false,
                                                                 std::memory_order_release);
    g_embeddedRouteIdentityProviderLookupCodeDumped.store(false,
                                                           std::memory_order_release);
    g_routeAlternateDecisionObserved.store(0, std::memory_order_release);
    g_routePhaseTimeoutMs.store(-1, std::memory_order_release);
    g_routeSelectionTimeoutMs.store(-1, std::memory_order_release);
    g_routePhaseTimeoutPassedObserved.store(false, std::memory_order_release);
    g_routeSelectionTimeoutPassedObserved.store(false, std::memory_order_release);
    g_routeSelectionIngestObserved.store(0, std::memory_order_release);
    g_authoredLaunchDispatchObserved.store(0, std::memory_order_release);
    g_launchCommandInitializeObserved.store(0, std::memory_order_release);
    g_launchCommandDispatchObserved.store(0, std::memory_order_release);
    g_launchProducerGuardLast.store(0xFFU, std::memory_order_release);
    g_routeModeZeroSetObserved.store(0, std::memory_order_release);
    g_routeModeOneSetObserved.store(0, std::memory_order_release);
    g_omegaRouteLifecycleQueryObserved.store(0, std::memory_order_release);
    g_omegaRouteLifecycleQueryLast.store(INT32_MIN, std::memory_order_release);
    g_omegaRouteLifecycleAccessorObserved.store(0, std::memory_order_release);
    for (auto& caller : g_omegaRouteLifecycleAccessorCallers) {
        caller.store(0U, std::memory_order_release);
    }
    g_routeStateQueryObserved.store(0, std::memory_order_release);
    g_routeStateQueryLastSignature.store(0, std::memory_order_release);
    g_routeStatePublishObserved.store(0, std::memory_order_release);
    g_managerStartObserved.store(0, std::memory_order_release);
    g_managerIdentityOneModeTransitionObserved.store(0, std::memory_order_release);
    g_managerIdentityOneLastMode.store(INT32_MIN, std::memory_order_release);
    g_managerIdentityOneLastPointer.store(nullptr, std::memory_order_release);
    g_bootstrapRequestAttempted.store(false, std::memory_order_release);
    g_towerfallLifecycleStarted.store(false, std::memory_order_release);
    g_omegaLifecycleManager.store(nullptr, std::memory_order_release);
    g_omegaLifecycleStarted.store(false, std::memory_order_release);
    g_omegaLifecycleComplete.store(false, std::memory_order_release);
    g_omegaPendingConsumerAttempted.store(false, std::memory_order_release);
    g_omegaLifecycleCalls.store(0U, std::memory_order_release);
    g_omegaStageTwoStalls.store(0U, std::memory_order_release);
    g_omegaStageTwoFirstTick.store(0U, std::memory_order_release);
    g_managerSetupStageObserved.store(0, std::memory_order_release);
    g_componentDispatchAttempted.store(false, std::memory_order_release);
    g_activitySetupComplete.store(false, std::memory_order_release);
    g_activityWorldStarted.store(false, std::memory_order_release);
    g_activityClientCandidate.store(nullptr, std::memory_order_release);
    g_joinedActivityClient.store(nullptr, std::memory_order_release);
    g_activityReceiverSnapshotAttempted.store(false, std::memory_order_release);
    g_componentDispatchIdentityOneObserved.store(0, std::memory_order_release);
    g_componentDispatchIdentityTwoObserved.store(0, std::memory_order_release);
    g_componentLookupObserved.store(0, std::memory_order_release);
    g_componentLookupIdentityOneObserved.store(0, std::memory_order_release);
    g_componentLookupIdentityTwoObserved.store(0, std::memory_order_release);
    g_componentRegisterObserved.store(0, std::memory_order_release);
    g_componentRegisterIdentityTwoObserved.store(0, std::memory_order_release);
    g_componentReuseObserved.store(0, std::memory_order_release);
    g_componentReuseIdentityTwoObserved.store(0, std::memory_order_release);
    g_componentMappingObserved.store(0, std::memory_order_release);
    g_componentMappingIdentityTwoObserved.store(0, std::memory_order_release);
    g_componentBuildObserved.store(0, std::memory_order_release);
    g_componentBuildIdentityTwoObserved.store(0, std::memory_order_release);
    g_componentSlotStateObserved.store(0, std::memory_order_release);
    g_componentSlotStateLastSignature.store(~std::uint64_t{0}, std::memory_order_release);
    g_componentSlotStateIdentityTwoLastSignature.store(~std::uint64_t{0},
                                                       std::memory_order_release);
    g_componentStepObserved.store(0, std::memory_order_release);
    g_componentStepIdentityTwoObserved.store(0, std::memory_order_release);
    g_componentTickObserved.store(0, std::memory_order_release);
    g_postComponentSyncObserved.store(0, std::memory_order_release);
    g_launchProducerWindowState.store(0U, std::memory_order_release);
    g_launchProducerWindowTicks.store(0U, std::memory_order_release);
    g_launchProducerWindowReassertions.store(0U, std::memory_order_release);
    g_launchProducerSelectionReady.store(false, std::memory_order_release);
    g_launchProducerSelectionDescriptor.fill(std::byte{});
    g_launchProducerSelectionDescriptorReady.store(false, std::memory_order_release);
    g_launchProducerReadinessRequestAttempted.store(false, std::memory_order_release);
    g_launchProducerEntryObserved.store(0U, std::memory_order_release);
    g_launchProducerEntryLastSignature.store(~std::uint64_t{0},
                                             std::memory_order_release);
    g_launchProducerReadyCallbackObserved.store(0U, std::memory_order_release);
    g_activityEvent46Observed.store(0U, std::memory_order_release);
    g_activityEvent46RegisteredProducerObserved.store(0U, std::memory_order_release);
    g_activityEvent46ProducerResetObserved.store(0U, std::memory_order_release);
    g_activityEvent46ProducerInitializeObserved.store(0U, std::memory_order_release);
    g_activityEvent46ProducerAvailableObserved.store(0U, std::memory_order_release);
    g_activityEvent46ProducerRequestObserved.store(0U, std::memory_order_release);
    g_activityEvent46ProducerTickObserved.store(0U, std::memory_order_release);
    g_activityEvent46RequestFlowObserved.store(0U, std::memory_order_release);
    g_activityEvent46ActionDispatchObserved.store(0U, std::memory_order_release);
    g_activityEvent46RequestPredicateObserved.store(0U, std::memory_order_release);
}

} // namespace sunrise::client::hooks::bootflow
