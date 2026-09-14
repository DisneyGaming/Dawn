#pragma once
#include "frame.h"
#include "reactor_combat.h"
#include "scenes.h"
#include "station_bindings.h"
#include "../coo/native_player_trigger.h"
#include "../coo/native_presentation_authority.h"
#include "../coo/native_combatant_authority.h"
#include "../coo/native_device_authority.h"
#include "../coo/native_scene_authority.h"
#include "../coo/native_scene_cast_authority.h"
#include "../coo/native_clock_authority.h"
namespace sunrise::state::activity::eater_of_worlds {
inline coo::native_device::interaction::Mode station_mode(const coo::Asset& source,const NativeState& state) noexcept {
    // All nine station entity/config pairs expose the 80809AE3 authority
    // interface for 80804FB0 slots 4/5 at +648, resolving to +388.
    // Enabling the prompt does not complete a use or charge a cranium.
    return state.active && state.acknowledged && station_index(source)<std::size(kStationBindings)
        ?coo::native_device::interaction::Mode::enabled:coo::native_device::interaction::Mode::unchanged;
}
inline std::size_t body_bits(const Frame& f,std::uint32_t key,std::uint8_t type,std::uint16_t slot) noexcept {
    if(!f.enabled || !f.spawnGeneration) return 0;
    const auto* a=find(key,type,slot);if(!a || a->authority==UINT32_MAX) return 0;
    if(type==31) return coo::native_player_trigger::kAuthBits;
    if(type==30) return 87;
    if(type==18) return 386;
    if(type==35 && key==0x24C67333U && slot==1) return 359;
    if(type==70) return 23;
    if(key==kRoot) {
        if(type==53) return coo::native_presentation::dialogue_bits(f.generations,f.activeRow);
        if(type==68 && f.presentation.published) return coo::native_presentation::kDirectiveBits;
        return 0;
    }
    const auto& s=f.native[asset_index(a->asset)];
    if(type==2 && a->asset==kBossActor) {
        const auto& source=f.native[asset_index(kBossSource)];
        return source.managed && source.active?coo::native_combatant::kBindBits:0;
    }
    if(!s.managed || !s.generation) return 0;
    if(type==1) {
        const auto i=spawn_index(key,slot);if(i>=std::size(kSpawns)) return 0;
        if(s.retiring) return kSpawns[i].categories==2?coo::native_combatant::kTwoCategorySourceBits
            :coo::native_combatant::kSourceBits;
        if(!s.active) return 0;
        return reactor_spawn_rule(a->asset)!=UINT16_MAX
            ?(kSpawns[i].categories==2?coo::native_combatant::kTwoCategorySourceBits:coo::native_combatant::kSourceBits)
            :coo::native_combatant::authored_source_bits(kSpawns[i].categories,true);
    }
    if(type==4) return coo::native_device::object_bits(s.active,false,
        station_mode(a->asset,s),(reactor_platform(a->asset) || a->asset==kReactorExitGrate)
            && s.acknowledged);
    if(type==23) return 147;
    if(type==43) {
        const auto* scene=scene_binding(a->asset);
        return scene?coo::native_scene::cast_bits(s.active?scene->cast.size():0):0;
    }
    return 0;
}
template<class Writer> bool write_body(Writer& w,const Frame& f,std::uint32_t key,std::uint8_t type,std::uint16_t slot) noexcept {
    if(!body_bits(f,key,type,slot)) return false;
    if(type==31) return coo::native_player_trigger::arm(w,f.spawnGeneration);
    if(type==30) return w.write(0x811C9DC5U,32) && w.write(0,7) && w.write(32767U,16)
        && w.write(0x80000000U+f.spawnGeneration,32);
    if(type==18) return coo::native_clock::countdown(w,f.completion.valid() && f.completion.state==6,f.endEpoch,30000);
    if(type==35) return w.write(f.restricted?1U:0U,1) && w.write(0,1)
        && w.write(1,2) && w.write(0,2) && w.write(0,1)
        && w.write(0,64) && w.write(0x134F00C00000ULL,64)
        && w.write(0,64) && w.write(0,64) && w.write(UINT64_MAX,64) && w.write(0x3F800000U,32);
    if(type==70) return w.write(0,5) && w.write(0,1) && w.write(32769U,16) && w.write(0,1);
    if(key==kRoot) {
        if(type==53) return coo::native_presentation::dialogue(w,f.generations,f.activeRow);
        coo::Asset audience{};
        for(const auto& g:kGroups) if(f.region>=0 && f.region<64 && (g.bubblesMask&(1U<<(f.region/8)))) {
            for(const auto& a:kAssets) if(a.asset.registry==g.key && a.asset.type==70) {audience=a.asset;break;}
        }
        return coo::native_presentation::objective(w,f.presentation,audience,true);
    }
    const auto* a=find(key,type,slot);if(!a) return false;
    const auto& s=f.native[asset_index(a->asset)];
    if(type==4) {
        coo::native_device::generic::State pose{};
        // 80F42FCD build row6 owns 80C3EE24/8080390E, the native
        // 80805063 generic-device consumer. Its three logical roots test
        // device_position > 0. Creation and activation remain separate.
        const bool platform=reactor_platform(a->asset) && s.acknowledged;
        const bool grate=a->asset==kReactorExitGrate && s.acknowledged;
        const auto index=platform_index(a->asset);
        if(platform && index<56) pose.position={static_cast<std::int32_t>(f.reactor.poseRevision[index]),-1,f.reactor.raised[index]?1.F:0.F};
        if(grate) pose.position={static_cast<std::int32_t>(f.grate.revision),-1,f.grate.open?1.F:0.F};
        return coo::native_device::object(w,s.generation,s.active,nullptr,
            station_mode(a->asset,s),platform||grate?&pose:nullptr);
    }
    if(type==23) return coo::native_device::position_only(w,s.position,static_cast<std::int16_t>(s.generation),false);
    if(type==2) return coo::native_combatant::write_bind(w,f.spawnGeneration);
    if(type==1) {
        const auto i=spawn_index(key,slot);const auto& row=kSpawns[i];
        if(s.retiring) {
            coo::native_combatant::Source source{key,s.generation,0,0,{},0,row.categories==2,false};
            source.retireOwned=true;
            return coo::native_combatant::write_source(w,source);
        }
        const auto rule=reactor_spawn_rule(a->asset);
        const auto requested=reactor_category_request(a->asset);
        coo::native_combatant::Source source{key,s.generation,
            rule==UINT16_MAX?std::uint16_t{}:rule,requested,
            {key,row.objective,static_cast<std::int8_t>(int(f.taskPlusOne[i])-1),s.generation},
            static_cast<std::uint8_t>(row.categories==2?requested:0),row.categories==2,rule!=UINT16_MAX};
        // Arrival uses the authored boss source directly. The scene-only
        // reservation path creates no actor without an authored cast selector.
        source.sceneRequested=row.sceneOwned && !(a->asset==kBossSource && f.arrival.intro);
        return rule!=UINT16_MAX?coo::native_combatant::write_source(w,source)
            :coo::native_combatant::write_authored_source(w,source,{0,0,0,0});
    }
    if(type==43) {
        const auto* scene=scene_binding(a->asset);
        return scene && coo::native_scene::cast_scene(w,s.active?s.generation:0,s.active?scene->cast:std::span<const coo::Asset>{});
    }
    return false;
}
}
