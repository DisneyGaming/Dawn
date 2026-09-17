#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "omega_rescue_npc_catalog.h"

namespace dawn::state::activity::omega_rescue_npc {

/** A generation belongs to one authored controller in the current activity run.
 * Generation zero is a dormant sentinel; it serializes signed -1 so admission
 * cannot create the selector before the encounter requests that Scene. */
struct SceneCommand final {
    std::uint32_t generation{};
    bool stop{};
    std::uint8_t eventCount{};
    std::array<std::uint32_t,4> events{};
    friend constexpr bool operator==(SceneCommand,SceneCommand)=default;
};
using Commands=std::array<SceneCommand,kScenes.size()>;
inline constexpr std::uint32_t kIntroBlockingReady=0x63A4F800U;
inline constexpr std::uint32_t kReleaseBlocking=0x1E9C04B7U;
inline constexpr std::uint32_t kReleaseFirstEye=0xECF4AD0CU;
inline constexpr std::uint32_t kReleaseRingEchoes=0x97C48FC6U;
inline constexpr std::uint32_t kReleaseChargeEcho=0x0E57C0FAU;
inline constexpr std::uint32_t kFinalBlockingRelease=0x14A9A975U;
inline constexpr std::uint32_t kFinalAttackRelease=0x505750FEU;
inline constexpr std::uint32_t kFinalCannonApproach=0x29210D29U;
inline constexpr std::uint32_t kFinalCannonArrived=0x02A77788U;
inline constexpr std::size_t kSceneSourceBits=641;

[[nodiscard]] constexpr const SceneCommand* command(const Commands& commands,
                                                   std::uint16_t slot) noexcept {
    for(std::size_t i=0;i<kScenes.size();++i) {
        if(kScenes[i].slot==slot) { return &commands[i]; }
    }
    return nullptr;
}
[[nodiscard]] constexpr bool valid(const SceneCommand& value) noexcept {
    if(value.generation>0x7FFFFFFFU || value.eventCount>value.events.size()
        || (value.generation==0 && (value.stop || value.eventCount!=0))) { return false; }
    for(std::size_t i=0;i<value.events.size();++i) {
        if(i>=value.eventCount) { if(value.events[i]!=0) { return false; } continue; }
        if(value.events[i]==0 || value.events[i]==UINT32_MAX) { return false; }
        for(std::size_t j=0;j<i;++j) { if(value.events[j]==value.events[i]) { return false; } }
    }
    return true;
}
[[nodiscard]] constexpr bool source_requested(const Commands& commands,
                                             std::uint16_t slot) noexcept {
    if(find_source(slot)==nullptr) { return false; }
    for(std::size_t i=0;i<kScenes.size();++i) {
        if(commands[i].generation==0 || !valid(commands[i])) { continue; }
        for(const auto& binding:kScenes[i].bindings) {
            if(binding.type==1 && binding.slot==slot) { return true; }
        }
    }
    return false;
}
[[nodiscard]] constexpr std::size_t scene_bits(const Scene& scene,
                                              const SceneCommand& value) noexcept {
    if(!valid(value)) { return 0; }
    return 74U+(value.generation==0?0U:55U*source_count(scene))+32U*value.eventCount;
}

/** Complete 8080626B, using the native controller's authored selector and cast.
 * The bounded source list is authorization for the selector's 808062AA nodes.
 * An event is retained in authority so native B41330 delivers it exactly once. */
template<class Writer>
[[nodiscard]] bool write_scene(Writer& writer,std::uint16_t slot,
                               const SceneCommand& value) noexcept {
    const auto* scene=find_scene(slot);
    if(scene==nullptr || !valid(value) || source_count(*scene)>8) { return false; }
    const auto begin=writer.bit_count();
    const auto count=value.generation==0?0U:source_count(*scene);
    const auto signedGeneration=value.generation==0?0x7FFFFFFFU:0x80000000U+value.generation;
    if(!writer.write(signedGeneration,32) || !writer.write(value.stop?1U:0U,1)
        || !writer.write(count,4)) { return false; }
    if(value.generation!=0) {
        for(const auto& binding:scene->bindings) {
            if(binding.type!=1) { continue; }
            if(!writer.write(kRegistry,32) || !writer.write(2,7)
                || !writer.write(32768U+binding.slot,16)) { return false; }
        }
    }
    if(!writer.write(value.generation==0?0U:1U,31)
        || !writer.write(value.eventCount,6)) { return false; }
    for(std::size_t i=0;i<value.eventCount;++i) {
        if(!writer.write(value.events[i],32)) { return false; }
    }
    return writer.bit_count()-begin==scene_bits(*scene,value);
}

/** 80807EC9 with one source-owned request and mode1: only the authored Scene
 * requests an actor. Both placement refs remain absent to select the registered
 * inline transform, and the packaged FNV sentinel remains intact. Sources keep
 * one generation throughout all Scenes in a run; a later Scene can reuse cast. */
template<class Writer>
[[nodiscard]] bool write_source(Writer& writer,std::uint32_t generation,
                                bool requested) noexcept {
    if(generation==0 || generation>0x7FFFFFFFU) { return false; }
    const auto begin=writer.bit_count();
    const auto absent=[&writer]() noexcept {
        return writer.write(1,1) && writer.write(0x811C9DC5U,32)
            && writer.write(0,7) && writer.write(32767U,16);
    };
    const bool ok=absent() && absent()
        && writer.write(1,1) && writer.write(0,3)
        && writer.write(1,1) && writer.write(1,4)
        && writer.write(0x80000000U+(requested?1U:0U),32)
        && writer.write(1,1) && writer.write(0,4)
        && writer.write(1,1) && writer.write(1,3) && writer.write(1,2)
        && writer.write(1,3) && writer.write(1,2) && writer.write(1,3)
        && writer.write(1,1) && writer.write(generation,31)
        && writer.write(1,1) && writer.write(0,32)
        && writer.write(1,1) && writer.write(0x811C9DC5U,32)
        && absent() && absent() && absent() && absent()
        && writer.write(1,1) && writer.write(0,31)
        && writer.write(1,1) && writer.write(0,31)
        && writer.write(1,1) && writer.write(1,6)
        && writer.write(1,1) && writer.write(1,5)
        && writer.write(1,1) && writer.write(0,31)
        && writer.write(1,2) && writer.write(2,3)
        && writer.write(1,1) && writer.write(0x811C9DC5U,32);
    return ok && writer.bit_count()-begin==kSceneSourceBits;
}

} // namespace dawn::state::activity::omega_rescue_npc
