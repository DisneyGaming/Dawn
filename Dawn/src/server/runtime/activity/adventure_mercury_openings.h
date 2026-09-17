#pragma once
#include "adventure_opening_runtime.h"
#include "adventure_mercury_overlays.h"
#include "adventure_mercury_forest.h"
#include "../../../state/activity/coo/mercury_registries.h"
namespace dawn::server::runtime::activity::adventure::mercury {
inline constexpr std::array<gateway::Binding,3> kForestGateways{{
    {{{{&coo::mercury::kRegistries[2],6,1},{&coo::mercury::kRegistries[2],7,1}}},0xD4C4F182,&kForestOverlays[0]},
    {{{{&coo::mercury::kRegistries[2],6,1},{&coo::mercury::kRegistries[2],7,1}}},0xD4C4F182,&kForestOverlays[1]},
    {{{{&coo::mercury::kRegistries[2],6,1},{&coo::mercury::kRegistries[2],7,1}}},0xD4C4F182,&kForestOverlays[2]},
}};
inline constexpr std::array<coo::script::Capability,3> kGatewayCapabilities{{
    {"adventure.forest.source","adventureOpening",{coo::Operation::device,{0xF25B938B,0x80F5E5BE,4,6},1,coo::Wait::requested}},
    {"adventure.forest.destination","adventureOpening",{coo::Operation::device,{0xF25B938B,0x80F5E5C1,4,7},1,coo::Wait::requested}},
    {"adventure.forest.permission","adventureOpening",{coo::Operation::traversal,{0xF25B938B,0x80F5E5BE,4,6},0xD4C4F182,coo::Wait::requested}},
}};
[[nodiscard]] constexpr OpeningBinding opening(std::size_t index,std::int16_t activity,std::string_view role,
    std::uint32_t definition,std::uint32_t table,std::uint32_t bank,dialogue_feedback::Authored dialogue) noexcept {
    cue_feedback::Ticket cue{};cue.activity=activity;cue.definition=definition;cue.definitionOffset=0xB88;
    cue.table=table;cue.stringBank=bank;cue.title=0x099C20CB;cue.detail=0x9661E8EB;
    cue.request={kOpeningOverlays[index].root.key,0xC9E4C596,0,0};
    // Reconstructed opening policy: the native waypoint targets the authored
    // Forest F source. Its placement reference is independent of readiness.
    cue.request.navigation[0]={0xF25B938B,6,15};
    return {activity,role,&kOpeningOverlays[index],cue,dialogue,&kForestGateways[index]};
}
// Event declarations in each selected Lighthouse local group and the matched
// global type68 table supply the opening objective. The user-provided retail
// sequence places this after native flag consumption and before Forest entry.
inline constexpr std::array<OpeningBinding,3> kOpenings{{
    opening(0,1076,"adventure_ascend",0x80F46D67,0x80F46D25,0x80F56028,{0x80F46D63,0x80F1FBD0,0x99EAC274,16286,21,0}),
    opening(1,1077,"adventure_horde",0x80F46D7B,0x80F46D26,0x80C71C76,{0x80F46D77,0x80F1FC17,0xF3B3CEFA,8713,14,0}),
    opening(2,1078,"adventure_intercept",0x80F46D8B,0x80F46D27,0x80F56024,{0x80F46D8F,0x80F1FC54,0xF0C647AE,20396,14,0}),
}};
// Local Lighthouse type54 declarations match these exact bank row0 selectors.
// Retail starts this authored conversation alongside the first objective. The
// host chooses the graph; native dialogue retains its variants and speakers.
inline constexpr std::array<coo::script::Capability,3> kDialogueCapabilities{{
    {"adventure.ascend.dialogue","adventureOpening",{coo::Operation::dialogue,{0x08551BCF,0x80F46D63,53,2},0x99EAC274,coo::Wait::nativeReady}},
    {"adventure.horde.dialogue","adventureOpening",{coo::Operation::dialogue,{0x9104CE05,0x80F46D77,53,2},0xF3B3CEFA,coo::Wait::nativeReady}},
    {"adventure.intercept.dialogue","adventureOpening",{coo::Operation::dialogue,{0x3DB08419,0x80F46D8F,53,2},0xF0C647AE,coo::Wait::nativeReady}},
}};
inline constexpr std::array<coo::script::Capability,3> kOpeningCapabilities{{
    {"adventure.ascend.opening","adventureOpening",{coo::Operation::objective,{0x08551BCF,0x80F46D67,68,0},0xC9E4C596,coo::Wait::nativeReady}},
    {"adventure.horde.opening","adventureOpening",{coo::Operation::objective,{0x9104CE05,0x80F46D7B,68,0},0xC9E4C596,coo::Wait::nativeReady}},
    {"adventure.intercept.opening","adventureOpening",{coo::Operation::objective,{0x3DB08419,0x80F46D8B,68,0},0xC9E4C596,coo::Wait::nativeReady}},
}};
}
