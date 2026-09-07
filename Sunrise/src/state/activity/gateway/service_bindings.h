#pragma once
#include "../coo/object_service.h"
#include "../coo/scene_orchestration.h"
namespace sunrise::state::activity::gateway {
// Native identities and authored values belong to the mission profile.
inline constexpr coo::ObjectBinding kEndingObjects[]{
    {{0x4B946B28U,0x80F46F14U,4,27},{0x4B946B28U,0x80F46EC3U,23,0},1.F,true},
    {{0x4B946B28U,0x80F46F23U,4,32},{0x4B946B28U,0x80F46ECFU,23,4},1.F,true},
    {{0x4B946B28U,0x80F46F26U,4,33},{0x4B946B28U,0x80F46ECCU,23,3},1.F,true}
};
inline constexpr std::uint8_t kObjectPreparation[]{3,5,6};
inline constexpr coo::LinkedDevice kModuleLinks[]{
    {0,1.F,1.F,0.F,true},{1,1.F,.75F,0.F,true},{2,1.F,1.F,0.F,true}
};
inline constexpr coo::AuthoredSceneEvent kSceneEvents[]{
    {0x3A5C256CU,coo::signal_bit(coo::SceneSignal::greetingSubmitted)},
    {0xC2656F80U,coo::signal_bit(coo::SceneSignal::animationReady)|coo::signal_bit(coo::SceneSignal::approached)|coo::signal_bit(coo::SceneSignal::prerollFinished)}
};
}
