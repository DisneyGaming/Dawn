// Native identities and receipt capabilities; mission decisions are authored in Lua.
#pragma once
#include "catalog.h"
#include "../coo/mission_script.h"
namespace dawn::state::activity::deadly_trial {
inline constexpr coo::Asset kModule{0xC9BC773AU,0x80B2E043U,0,0},kRevive{0xEB7AF01BU,0x80B2EBA7U,4,59},kScene{0x27660927U,0x80B2E6D7U,65,0},kBarrier{0xEB7AF018U,0x80B2E3ADU,23,46};
inline constexpr coo::script::Capability kCapabilities[]{
    {"pike.mounted","*",{coo::Operation::observation,kModule,10U,coo::Wait::observed}},
    {"walker.enable","*",{coo::Operation::population,kModule,4U,coo::Wait::requested}},
    {"tower.enable","*",{coo::Operation::population,kModule,8U,coo::Wait::requested}},
    {"lair.enable","*",{coo::Operation::population,kModule,9U,coo::Wait::requested}},
    {"walker.cleared","*",{coo::Operation::observation,kModule,4U,coo::Wait::observed}},
    {"tower.cleared","*",{coo::Operation::observation,kModule,8U,coo::Wait::observed}},
    {"lair.cleared","*",{coo::Operation::observation,kModule,9U,coo::Wait::observed}},
    {"overpass.fallback","*",{coo::Operation::observation,{0xEB7AF018U,0x80B2E2B5U,60,130},0U,coo::Wait::observed}},
    {"overpass.dropship","*",{coo::Operation::observation,{0xEB7AF018U,0x80B2E2B5U,60,132},0U,coo::Wait::observed}},
    {"overpass.directive","*",{coo::Operation::observation,{0x38C174BBU,0x80B2E491U,60,4},0U,coo::Wait::observed}},
    {"overpass.waypoint","*",{coo::Operation::observation,{0xE4BA2E54U,0x80B2E499U,60,1},0U,coo::Wait::observed}},
    {"trial.directive","*",{coo::Operation::observation,{0x4DDB8242U,0x80B2E47DU,60,1},0U,coo::Wait::observed}},
    {"tower.directive","*",{coo::Operation::observation,{0xC91BDFF0U,0x80B2E6C7U,60,1},0U,coo::Wait::observed}},
    {"tower.waypoint","*",{coo::Operation::observation,{0xBC1C972BU,0x80B2E6CEU,60,1},0U,coo::Wait::observed}},

    {"opening.module","composition",{coo::Operation::mechanic,{0xC9BC773AU,0x80B2E043U,0,0},1U,coo::Wait::requested}},
    {"opening.checked","composition",{coo::Operation::observation,{0x00000000U,0x00000000U,0,0},0U,coo::Wait::observed}},
    {"landing.entered","*",{coo::Operation::observation,{0x3A62993FU,0x80B2E6F3U,60,2},0U,coo::Wait::observed}},
    {"objective.coordinates","*",{coo::Operation::objective,{0xC9BC773AU,0x80B2E706U,68,0},3961616249U,coo::Wait::requested}},
    {"dialogue.0","*",{coo::Operation::dialogue,{0xC9BC773AU,0x80B2E709U,53,2},0U,coo::Wait::nativeReady}},
    {"square.entered","*",{coo::Operation::observation,{0x40ADE010U,0x80B2E485U,60,3},0U,coo::Wait::observed}},
    {"square.fallen","*",{coo::Operation::population,{0xC9BC773AU,0x80B2E043U,0,0},1U,coo::Wait::completed}},
    {"objective.pike","*",{coo::Operation::objective,{0xC9BC773AU,0x80B2E706U,68,0},3667242569U,coo::Wait::requested}},
    {"square.pikes","*",{coo::Operation::device,{0xC9BC773AU,0x80B2E043U,0,0},1U,coo::Wait::requested}},
    {"followers.entered","*",{coo::Operation::observation,{0x38C174BBU,0x80B2E491U,60,2},0U,coo::Wait::observed}},
    {"dialogue.1","*",{coo::Operation::dialogue,{0xC9BC773AU,0x80B2E709U,53,2},1U,coo::Wait::nativeReady}},
    {"objective.path","*",{coo::Operation::objective,{0xC9BC773AU,0x80B2E706U,68,0},406820629U,coo::Wait::requested}},
    {"streets.entered","*",{coo::Operation::observation,{0xEB7AF018U,0x80B2E2B5U,60,214},0U,coo::Wait::observed}},
    {"streets.fallen","*",{coo::Operation::population,{0xC9BC773AU,0x80B2E043U,0,0},2U,coo::Wait::requested}},
    {"choke.entered","*",{coo::Operation::observation,{0xEB7AF018U,0x80B2E2B5U,60,229},0U,coo::Wait::observed}},
    {"choke.fallen","*",{coo::Operation::population,{0xC9BC773AU,0x80B2E043U,0,0},3U,coo::Wait::requested}},
    {"overpass.entered","*",{coo::Operation::observation,{0xEB7AF018U,0x80B2E2B5U,60,131},0U,coo::Wait::observed}},
    {"objective.walker","*",{coo::Operation::objective,{0xC9BC773AU,0x80B2E706U,68,0},1874372698U,coo::Wait::requested}},
    {"overpass.support","*",{coo::Operation::population,{0xC9BC773AU,0x80B2E043U,0,0},5U,coo::Wait::requested}},
    {"walker.dead","*",{coo::Operation::population,{0xC9BC773AU,0x80B2E043U,0,0},4U,coo::Wait::completed}},
    {"barrier.open","*",{coo::Operation::device,{0xEB7AF018U,0x80B2E3ADU,23,46},0U,coo::Wait::requested}},
    {"objective.temple","*",{coo::Operation::objective,{0xC9BC773AU,0x80B2E706U,68,0},3851137782U,coo::Wait::requested}},
    {"overpass.pikes","*",{coo::Operation::device,{0xC9BC773AU,0x80B2E043U,0,0},2U,coo::Wait::requested}},
    {"trial.entered","*",{coo::Operation::observation,{0x4DDB8242U,0x80B2E47DU,60,2},0U,coo::Wait::observed}},
    {"dialogue.2","*",{coo::Operation::dialogue,{0xC9BC773AU,0x80B2E709U,53,2},2U,coo::Wait::nativeReady}},
    {"cliff.entered","*",{coo::Operation::observation,{0xEB7AF01BU,0x80B2E575U,60,157},0U,coo::Wait::observed}},
    {"cliff.fallen","*",{coo::Operation::population,{0xC9BC773AU,0x80B2E043U,0,0},6U,coo::Wait::requested}},
    {"tunnel.entered","*",{coo::Operation::observation,{0xEB7AF01BU,0x80B2E575U,60,223},0U,coo::Wait::observed}},
    {"tunnel.fallen","*",{coo::Operation::population,{0xC9BC773AU,0x80B2E043U,0,0},7U,coo::Wait::requested}},
    {"tower.entered","*",{coo::Operation::observation,{0xBC1C972BU,0x80B2E6CEU,60,2},0U,coo::Wait::observed}},
    {"dialogue.4","*",{coo::Operation::dialogue,{0xC9BC773AU,0x80B2E709U,53,2},4U,coo::Wait::nativeReady}},
    {"tower.fallen","*",{coo::Operation::population,{0xC9BC773AU,0x80B2E043U,0,0},8U,coo::Wait::completed}},
    {"objective.search","*",{coo::Operation::objective,{0xC9BC773AU,0x80B2E706U,68,0},2284704542U,coo::Wait::requested}},
    {"lair.entered","*",{coo::Operation::observation,{0xEB7AF01BU,0x80B2E575U,60,195},0U,coo::Wait::observed}},
    {"lair.marauders","*",{coo::Operation::population,{0xC9BC773AU,0x80B2E043U,0,0},9U,coo::Wait::completed}},
    {"bodies.entered","*",{coo::Operation::observation,{0x9F8CA248U,0x80B2E6C0U,60,2},0U,coo::Wait::observed}},
    {"dialogue.7","*",{coo::Operation::dialogue,{0xC9BC773AU,0x80B2E709U,53,2},7U,coo::Wait::nativeReady}},
    {"objective.revive","*",{coo::Operation::objective,{0xC9BC773AU,0x80B2E706U,68,0},1888195409U,coo::Wait::requested}},
    {"revive.enable","*",{coo::Operation::device,{0xEB7AF01BU,0x80B2EBA7U,4,59},1U,coo::Wait::requested}},
    {"revive.accepted","*",{coo::Operation::observation,{0xEB7AF01BU,0x80B2EBA7U,4,59},1U,coo::Wait::observed}},
    {"prelude.finished","*",{coo::Operation::eventAfter,{0xC9BC773AU,0x80B2E709U,53,2},7U,coo::Wait::observed}},
    {"revive.scene","*",{coo::Operation::scene,{0x27660927U,0x80B2E6D7U,65,0},1U,coo::Wait::nativeReady}},
    {"revive.scene_finished","*",{coo::Operation::observation,{0x27660927U,0x80B2E6D7U,65,0},2U,coo::Wait::observed}},
    {"revive.audio_finished","*",{coo::Operation::eventAfter,{0x27660927U,0x80B2E6D7U,65,0},3U,coo::Wait::observed}},
    {"mission.finish","*",{coo::Operation::complete,{0xC9BC773AU,0x80B2E043U,0,0},6U,coo::Wait::requested}},
};
inline constexpr coo::script::ModuleCapability kModules[]{{"opening",{kModule,1}}};
inline constexpr coo::script::FactCapability kFacts[]{{"opening.checked",0}};

inline constexpr coo::ObjectiveMarker kNativeMarkers[]{
    {0xEC217779U,{{0x40ADE010U,0x80B2E62BU,47,1},{0x29930BA4U,0xC6905B77U,0x40ADE010U,0xBA2B60FDU}}},
    {0xDA95AE49U,{{0xECC3696FU,0x80B2E6E9U,47,1},{0x29930BA4U,0xC6905B77U,0xECC3696FU,0x24A5314AU}}},
    {0x183F9715U,{{0x38C174BBU,0x80B2E66BU,47,7},{0x29930BA4U,0xC6905B77U,0x38C174BBU,0xC704236EU}}},
    {0x6FB8A85AU,{{0xE4BA2E54U,0x80B2E68DU,47,0},{0x29930BA4U,0xC6905B77U,0xE4BA2E54U,0xE58495E9U}}},
    {0xE58BB2F6U,{{0xC91BDFF0U,0x80B2EC47U,47,4},{0x29930BA4U,0xC6905B74U,0xC91BDFF0U,0x69AEC903U}}},
    {0x882DD31EU,{{0xBC1C972BU,0x80B2EC53U,47,10},{0x29930BA4U,0xC6905B74U,0xBC1C972BU,0x69AEC903U}}},
    {0x708B9351U,{{0x27660927U,0x80B2EC62U,47,4},{0x29930BA4U,0xC6905B74U,0x27660927U,0x9BEEEAA2U}}},
};

}
