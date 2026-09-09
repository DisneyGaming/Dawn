#pragma once
#include "../../../state/activity/beyond_infinity/frame.h"
#include "../../../state/activity/coo/native_device_authority.h"
#include <cstring>

namespace sunrise::client::hooks::bootflow::beyond_infinity_native {
namespace bi=state::activity::beyond_infinity;
namespace coo=state::activity::coo;
struct Definition { std::uint32_t tag{},kind{};std::int64_t offset{}; };
inline bool preparation(std::span<const std::byte,16> prefix,std::span<const std::byte,0x44> state,
    const bi::Frame& frame,coo::Generation owner,coo::Asset& asset) noexcept {
    Definition definition{};std::memcpy(&definition,prefix.data(),sizeof definition);
    if(!owner.valid() || !frame.enabled || frame.spawnGeneration!=owner.value || definition.kind!=0x80809928U
        || !coo::native_device::inactive_state(state)) { return false; }
    std::uint32_t generation{};std::memcpy(&generation,state.data(),4);
    for(std::size_t i=0;i<std::size(bi::kAssets);++i) {
        const auto& binding=bi::kAssets[i];const auto& desired=frame.native[i];
        if(binding.asset.type!=4 || binding.asset.definition!=definition.tag || binding.offset!=definition.offset) { continue; }
        if(!desired.managed || desired.prepared || generation!=desired.generation) { return false; }
        asset=binding.asset;return true;
    }
    return false;
}
// Captured before/after the native B438B0 call. Validation is pure and shared by
// the hook and offline tests; a scene name alone never authenticates a receipt.
struct SceneSample {
    Definition definition{};
    coo::Asset scope{};
    std::uint32_t group{UINT32_MAX},sensor{UINT32_MAX},generation{};
    Definition selector{};
    std::uint32_t weak{UINT32_MAX},serial{UINT32_MAX},self{UINT32_MAX},parent{UINT32_MAX};
    std::uint8_t complete{UINT8_MAX};
};
inline bool scene_sample(const SceneSample& sample,const bi::Frame& frame,coo::Generation owner,
    bi::SceneReceipt& receipt) noexcept {
    if(!owner.valid() || !frame.enabled || frame.spawnGeneration!=owner.value || sample.definition.kind!=0x80806266U || sample.definition.offset!=0x368
        || sample.complete>1 || sample.selector.kind!=0x80806384U || sample.weak!=sample.self
        || sample.weak==UINT32_MAX || sample.serial==UINT32_MAX || sample.parent==UINT32_MAX
        || sample.group==UINT32_MAX || sample.sensor==UINT32_MAX) { return false; }
    for(const auto& scene:bi::kScenes) {
        if(scene.asset!=sample.scope || scene.asset.definition!=sample.definition.tag || scene.selectorGraph!=sample.selector.tag || scene.selectorOffset!=sample.selector.offset) { continue; }
        const auto& desired=frame.native[bi::asset_index(scene.asset)];
        if(!desired.active || desired.generation!=sample.generation) { return false; }
        receipt={{owner.run,sample.generation},scene.asset,sample.group,sample.sensor,sample.weak,sample.serial};return true;
    }
    return false;
}
inline bool same_scene(const SceneSample& before,const SceneSample& after) noexcept {
    return before.definition.tag==after.definition.tag && before.definition.kind==after.definition.kind
        && before.definition.offset==after.definition.offset && before.scope==after.scope
        && before.group==after.group && before.sensor==after.sensor && before.generation==after.generation
        && before.selector.tag==after.selector.tag && before.selector.kind==after.selector.kind && before.selector.offset==after.selector.offset
        && before.weak==after.weak && before.serial==after.serial && before.self==after.self
        && before.parent==after.parent && before.complete<=1 && after.complete<=1;
}
inline bool speech_node(const bi::SceneBinding& scene,const bi::SceneSpeech& speech,
    Definition node,std::uint32_t state) noexcept {
    return speech.row<49 && node.tag==scene.selectorGraph && node.kind==0x808062F6U
        && node.offset==speech.definition && (state==1 || state==2);
}
inline bool cue_node(const bi::SceneBinding& scene,const bi::SceneCue& cue,
    Definition node,Definition signal,std::uint32_t self,std::uint32_t state,std::uint32_t starts) noexcept {
    return cue.id<64 && node.tag==scene.selectorGraph && node.kind==cue.kind && node.offset==cue.definition
        && signal.tag==scene.selectorGraph && signal.kind==cue.signalKind && signal.offset==cue.signalDefinition
        && self!=UINT32_MAX && starts>0 && (state==1 || state==2);
}
// These Well graphs have no animation downstream of a speech completion.
// Their native speech callbacks can be detached while Lua queues the same bank
// rows. verify_beyond_well_speech.py checks that property against package edges.
inline bool silent_voice(const SceneSample& sample,const bi::Frame& frame) noexcept {
    const auto index=bi::scene_index(sample.scope);
    if(index==std::size(bi::kScenes) || !frame.sceneRequests[index].silent
        || sample.scope.registry!=0x1194F70FU || sample.scope.type!=43) { return false; }
    const auto& scene=bi::kScenes[index];
    if(sample.selector.tag!=scene.selectorGraph || sample.selector.kind!=0x80806384U
        || sample.selector.offset!=scene.selectorOffset) { return false; }
    switch(sample.scope.slot) { case 14:case 19:case 22:case 24:case 25:case 28:return true;default:return false; }
}
// Exact callback consumed by native DB4B80. Only the instance's speech-start
// callback is detached; its animation inputs and all package definitions remain.
inline bool silent_callback(std::span<const std::byte,24> callback,std::uint32_t self,std::uint32_t node=0xA30) noexcept {
    std::array<std::uint32_t,4> words{};std::int64_t offset{};
    std::memcpy(words.data(),callback.data(),16);std::memcpy(&offset,callback.data()+16,8);
    return self!=UINT32_MAX && words==std::array<std::uint32_t,4>{0x80806342U,0,self,0x808062F5U} && offset==node;
}

}
