#pragma once
#include "catalog.h"
#include "../coo/native_combatant_authority.h"
#include "../coo/native_device_authority.h"
#include "../../../middleware/bap/activity_message/native/native_npc_animation_authority.h"
namespace sunrise::state::activity::vendors::presentation {
// Authored ordinary Tower vendors; keep descriptor order and dormant district leases stable.
inline constexpr AssetBinding kAssets[]{
    bind({0x50CC9C7DU,0x80B4A4A4U,1,0},0x728U,"sq_gunsmith"),
    bind({0x50CC9C7DU,0x80B4A4A9U,1,1},0x728U,"sq_vendor_cryptarch"),
    bind({0x50CC9C7DU,0x80B4A4ACU,1,2},0x728U,"sq_vendor_postmaster"),
    bind({0x50CC9C7DU,0x80B4A4AFU,1,3},0x728U,"sq_vendor_pvp"),
    bind({0x50CC9C7DU,0x80B4A4B2U,1,4},0x728U,"sq_vendor_tes_everis"),
    bind({0x50CC9C7DU,0x80B4A4B5U,1,5},0x728U,"vendor_zavala"),
    bind({0x50CC9C7DU,0x80B4A4B9U,4,6},0x4C8U,"o_statue_vendor"),
    bind({0x50CC9C7DU,0x80B4A4BDU,1,7},0x878U,"sq_eva_levante_solstice"),
    bind({0x50CC9C7DU,0x80B4A4C0U,1,8},0x728U,"sq_test_vendor"),
    bind({0x50CC9C7DU,0x80B4A484U,6,9},0x2E8U,"cin_tower_landing_bay_3"),
    bind({0x50CC9C7DU,0x80B4A487U,6,10},0x2E8U,"cin_tower_landing_bay_1"),
    bind({0x50CC9C7DU,0x80B4A48AU,6,11},0x2E8U,"cin_tower_landing_bay_2"),
    bind({0x50CC9C7DU,0x80B4A6A1U,4,12},0x4C8U,"o_fem01_dpad_type_stand"),
    bind({0x50CC9C7DU,0x80B4A4C7U,42,13},0x228U,"sq_vendor_postmaster_idle"),
    bind({0x50CC9C7DU,0x80B4A4CAU,42,14},0x228U,"sq_vendor_cryptarch_idle"),
    bind({0x50CC9C7DU,0x80B4A4CEU,42,15},0x228U,"sq_vendor_tes_everis_idle"),
    bind({0x50CC9C7DU,0x80B4A4D1U,42,16},0x228U,"sq_vendor_pvp_idle"),
    bind({0x50CC9C7DU,0x80B4A4D5U,42,17},0x228U,"sq_gunsmith_idle"),
    bind({0x50CC9C7DU,0x80B4A4D8U,42,18},0x228U,"vendor_zavala_idle"),
    bind({0x50CC9C7DU,0x80B4A4DBU,42,19},0x228U,"sq_test_vendor_idle"),
    bind({0x50CC9C7DU,0x80B4A6A4U,42,20},0x228U,"sq_eva_levante_solstice_idle"),
    bind({0xF8790DA5U,0x80B4A28CU,1,0},0x728U,"sq_vendor_hawthorne"),
    bind({0xF8790DA5U,0x80B4A291U,1,1},0x728U,"sq_vendor_new_monarchy"),
    bind({0xF8790DA5U,0x80B4A294U,4,2},0x4C8U,"o_ikora_vendor_s8"),
    bind({0xF8790DA5U,0x80B4A297U,4,3},0x4C8U,"o_ikora_vendor"),
    bind({0xF8790DA5U,0x80B4A29AU,1,4},0x878U,"sq_gambit"),
    bind({0xF8790DA5U,0x80B4A29EU,1,5},0x878U,"sq_eva_levante"),
    bind({0xF8790DA5U,0x80B4A2A1U,42,6},0x228U,"sq_vendor_new_monarchy_idle"),
    bind({0xF8790DA5U,0x80B4A2A4U,42,7},0x228U,"sq_vendor_hawthorne_idle"),
    bind({0xF8790DA5U,0x80B4A2A7U,42,8},0x228U,"sq_gambit_idle"),
    bind({0xF8790DA5U,0x80B4A2AAU,42,9},0x228U,"sq_eva_levante_idle"),
    bind({0x728E75D1U,0x80B4A979U,4,0},0x4C8U,"o_catwalk_controller_1._object"),
    bind({0x728E75D1U,0x80B4A97CU,4,1},0x4C8U,"o_catwalk_controller_2._object"),
    bind({0x728E75D1U,0x80B4A97FU,23,2},0x278U,"d_catwalk_controller"),
    bind({0x728E75D1U,0x80B4A982U,1,3},0x728U,"sq_vendor_amanda_holliday"),
    bind({0x728E75D1U,0x80B4A985U,1,4},0x728U,"sq_vendor_dead_orbit"),
    bind({0x728E75D1U,0x80B4A988U,1,5},0x728U,"sq_vendor_future_war_cult"),
    bind({0x728E75D1U,0x80B4A98BU,1,6},0x728U,"xur.xur_squad"),
    bind({0x728E75D1U,0x80B4A98EU,1,7},0x728U,"sq_patrol_mid_inventory"),
    bind({0x728E75D1U,0x80B4A991U,1,8},0x728U,"sq_patrol_back_inventory"),
    bind({0x728E75D1U,0x80B4A994U,43,9},0x368U,"sc_back_inventory"),
    bind({0x728E75D1U,0x80B4A997U,43,10},0x368U,"sc_mid_inventory"),
    bind({0x728E75D1U,0x80B4A99AU,4,11},0x4C8U,"o_deflector_1._object"),
    bind({0x728E75D1U,0x80B4A99DU,23,12},0x278U,"d_deflector_1_a"),
    bind({0x728E75D1U,0x80B4A9A0U,23,13},0x278U,"d_deflector_1_b"),
    bind({0x728E75D1U,0x80B4A9A3U,4,14},0x4C8U,"o_deflector_2._object"),
    bind({0x728E75D1U,0x80B4A9A8U,23,15},0x278U,"d_deflector_2_a"),
    bind({0x728E75D1U,0x80B4A9ACU,23,16},0x278U,"d_deflector_2_b"),
    bind({0x728E75D1U,0x80B4A9B0U,4,17},0x4C8U,"o_deflector_3._object"),
    bind({0x728E75D1U,0x80B4A9B3U,23,18},0x278U,"d_deflector_3_a"),
    bind({0x728E75D1U,0x80B4A9B6U,23,19},0x278U,"d_deflector_3_b"),
    bind({0x728E75D1U,0x80B4A9B9U,4,20},0x4C8U,"o_deflector_4._object"),
    bind({0x728E75D1U,0x80B4A9BCU,23,21},0x278U,"d_deflector_4_a"),
    bind({0x728E75D1U,0x80B4A9BFU,23,22},0x278U,"d_deflector_4_b"),
    bind({0x728E75D1U,0x80B4AC50U,34,23},0x388U,"o_deflector_1.interacting_player_filter"),
    bind({0x728E75D1U,0x80B4AC53U,34,24},0x388U,"o_deflector_2.interacting_player_filter"),
    bind({0x728E75D1U,0x80B4AC56U,34,25},0x388U,"o_deflector_3.interacting_player_filter"),
    bind({0x728E75D1U,0x80B4AC59U,34,26},0x388U,"o_deflector_4.interacting_player_filter"),
    bind({0x728E75D1U,0x80B4AC5CU,34,27},0x388U,"o_catwalk_controller_1.interacting_player_filter"),
    bind({0x728E75D1U,0x80B4AC62U,34,28},0x388U,"o_catwalk_controller_2.interacting_player_filter"),
    bind({0x728E75D1U,0x80B4A9C2U,42,29},0x228U,"xur.xur_squad_idle"),
    bind({0x728E75D1U,0x80B4A9C6U,42,30},0x228U,"sq_vendor_amanda_holliday_idle"),
    bind({0x728E75D1U,0x80B4A9C9U,42,31},0x228U,"sq_vendor_dead_orbit_idle"),
    bind({0x728E75D1U,0x80B4A9CFU,42,32},0x228U,"sq_vendor_future_war_cult_idle"),
    bind({0x58B3D759U,0x80B4A1B2U,1,0},0x878U,"sq_armory_vendor"),
    bind({0x58B3D759U,0x80B4A1B6U,70,1},0x358U,"m_engagement_sensor"),
    bind({0x58B3D759U,0x80B4A1BAU,42,2},0x228U,"sq_armory_vendor_idle"),
    bind({0xD227B29FU,0x80B4A1BEU,4,0},0x4C8U,"o_drifter_vendor"),
    bind({0xD227B29FU,0x80B4A1C1U,70,1},0x358U,"m_engagement_sensor"),
    bind({0x4F4ED92FU,0x80B4A1C5U,4,0},0x4C8U,"o_penumbra_vendor"),
    bind({0x4F4ED92FU,0x80B4A1C8U,70,1},0x358U,"m_engagement_sensor"),
    bind({0xE7F0A95BU,0x80B4A233U,4,0},0x4C8U,"o_ritual_kiosk_vendor"),
    bind({0xE7F0A95BU,0x80B4A236U,4,1},0x4C8U,"o_monolith_vendor"),
    bind({0xE7F0A95BU,0x80B4A239U,70,2},0x358U,"m_engagement_sensor"),
    bind({0x9052672CU,0x80B4A22BU,4,0},0x4C8U,"o_saint_14"),
    bind({0x9052672CU,0x80B4A22FU,70,1},0x358U,"m_engagement_sensor"),
};
inline constexpr Group kGroups[]{
    {0x50CC9C7DU,0x80B4A6B0U,UINT32_MAX,6,false,std::span(kAssets).subspan(0,21)},
    {0xF8790DA5U,0x80B4A135U,UINT32_MAX,1,false,std::span(kAssets).subspan(21,10)},
    {0x728E75D1U,0x80B4AD29U,UINT32_MAX,7,false,std::span(kAssets).subspan(31,33)},
    {0x58B3D759U,0x80B4A1BDU,UINT32_MAX,0,false,std::span(kAssets).subspan(64,3)},
    {0xD227B29FU,0x80B4A1C4U,UINT32_MAX,0,false,std::span(kAssets).subspan(67,2)},
    {0x4F4ED92FU,0x80B4A1CBU,UINT32_MAX,0,false,std::span(kAssets).subspan(69,2)},
    {0xE7F0A95BU,0x80B4A23EU,UINT32_MAX,0,false,std::span(kAssets).subspan(71,3)},
    {0x9052672CU,0x80B4A232U,UINT32_MAX,7,false,std::span(kAssets).subspan(74,2)},
};
inline constexpr std::uint32_t kTower=0x80B4A0F4U;
inline constexpr std::span<const Group> groups(std::uint32_t scenario=kTower) noexcept {
    return scenario==kTower?std::span(kGroups):activity::vendors::groups(scenario);
}
struct Population {std::uint32_t generation{1};bool occupied{},suspended{};};
struct Frame {
    bool enabled{};std::uint8_t bubble{};std::uint64_t nearby{};std::uint32_t scenario{kTower};
    std::array<Population,12+std::size(activity::vendors::kSources)> population{};
};
inline constexpr bool owns(std::uint32_t key,std::uint32_t scenario=kTower) noexcept {
    for(const auto& g:groups(scenario)) if(g.key==key) {return true;}return false;
}
inline const AssetBinding* find(const Frame& f,std::uint32_t key,std::uint8_t type,std::uint16_t slot) noexcept {
    if(!f.enabled) {return nullptr;}
    // A neighbouring district may remain loaded. Never reset its actors or controllers
    // to their defaults simply because the held district changed.
    for(const auto& g:groups(f.scenario)) if(g.key==key)
        for(const auto& a:g.slots) if(a.asset.type==type && a.asset.slot==slot) {return &a;}
    return nullptr;
}
struct VendorSource {std::uint32_t key;std::uint16_t source,rule,idle;std::uint32_t sequence;};
inline constexpr VendorSource kSources[]{
    {0x50CC9C7D,0,21,17,0x36466083},{0x50CC9C7D,1,26,14,0xF3A6BCD7},
    {0x50CC9C7D,2,24,13,0x870A7D66},{0x50CC9C7D,3,27,16,0xD9474D64},
    {0x50CC9C7D,4,23,15,0xB2E54A10},{0x50CC9C7D,5,25,18,0x08BA6CD2},
    {0xF8790DA5,0,10,7,0xFD41C0E5},{0xF8790DA5,1,11,6,0xE9527AF7},
    {0x728E75D1,3,39,30,0x5E795D1C},{0x728E75D1,4,34,31,0x3E9851D6},
    {0x728E75D1,5,40,32,0x94DBC38B},
    {0x58B3D759,0,UINT16_MAX,2,0xFA55297B},
};
static_assert(std::size(kSources)==12);
inline constexpr std::size_t source_index(std::uint32_t scenario,std::uint32_t key,
    std::uint8_t type,std::uint16_t slot) noexcept {
    if(scenario==kTower && type==1) for(std::size_t i=0;i<std::size(kSources);++i)
        if(kSources[i].key==key && kSources[i].source==slot) return i;
    for(std::size_t i=0;i<std::size(activity::vendors::kSources);++i) {
        const auto& v=activity::vendors::kSources[i];
        if(v.scenario==scenario && v.key==key
            && ((type==1 && v.source==slot) || (type==2 && v.member==slot))) return std::size(kSources)+i;
    }
    return SIZE_MAX;
}
// Native spawn-rule points; Ada's point is embedded in source80B4A1B2.
inline constexpr std::array<std::array<float,3>,std::size(kSources)> kPositions{{
    {-5.754721F,17.616762F,17.018948F},{5.360869F,41.242577F,17.032867F},
    {46.062386F,37.054321F,15.023687F},{-3.982012F,-2.727180F,16.679058F},
    {43.000839F,27.968651F,14.950609F},{35.236374F,-14.996662F,16.999365F},
    {-119.455284F,50.039520F,5.798670F},{-147.832916F,54.687862F,0.463862F},
    {150.960007F,51.423859F,5.959360F},{141.454849F,102.388374F,4.950690F},
    {152.316147F,81.796204F,11.330965F},{-198.826294F,87.476120F,-25.352823F},
}};
inline std::uint64_t presence(std::uint64_t previous,[[maybe_unused]] std::uint8_t bubble,
    std::array<float,3> player,bool present,std::uint32_t scenario=kTower) noexcept {
    if(!present) {return 0;}
    std::uint64_t result{};
    const auto evaluate=[&](std::size_t i,const std::array<float,3>& point) {
        const double dx=double(player[0])-point[0],dy=double(player[1])-point[1],dz=double(player[2])-point[2];
        // Reconstructed presence policy: 6m approach, 8m departure. Hysteresis
        // prevents boundary jitter; native graphs own the greeting/idle timing.
        const auto radius=(previous&(UINT64_C(1)<<i))?8.:6.;
        if(dx*dx+dy*dy<=radius*radius && dz*dz<=9.) {result|=UINT64_C(1)<<i;}
    };
    if(scenario==kTower) for(std::size_t i=0;i<std::size(kSources);++i) {evaluate(i,kPositions[i]);}
    for(std::size_t i=0;i<std::size(activity::vendors::kSources);++i) {
        const auto& v=activity::vendors::kSources[i];
        if(v.scenario==scenario && v.approach) {evaluate(std::size(kSources)+i,v.position);}
    }
    return result;
}
static_assert(std::size(kSources)+std::size(activity::vendors::kSources)<=64);
inline constexpr bool nearby(const Frame& f,std::uint32_t key,std::uint16_t slot) noexcept {
    for(std::size_t i=0;i<std::size(kSources);++i)
        if(kSources[i].key==key && kSources[i].idle==slot) {return (f.nearby&(UINT64_C(1)<<i))!=0;}
    for(std::size_t i=0;i<std::size(activity::vendors::kSources);++i) {
        const auto& v=activity::vendors::kSources[i];
        if(v.key==key && v.idle==slot) {return (f.nearby&(UINT64_C(1)<<(std::size(kSources)+i)))!=0;}
    }
    return false;
}
inline constexpr std::uint32_t idle(std::uint32_t key,std::uint16_t slot) noexcept {
    for(const auto& v:kSources) if(v.key==key && v.idle==slot) {return v.sequence;}
    for(const auto& v:activity::vendors::kSources) if(v.key==key && v.idle==slot) {return v.sequence;}return 0;
}
inline std::size_t body_bits(const Frame& f,std::uint32_t key,std::uint8_t type,std::uint16_t slot) noexcept {
    if(!find(f,key,type,slot)) {return 0;}
    if(type==1) {return coo::native_combatant::kSourceBits;}
    if(type==2) {return coo::native_combatant::kBindBits;}
    if(type==4) {return 252;}
    if(type==42) {return middleware::bap::activity_message::native::npc_animation::body_bits(nearby(f,key,slot)?1:0);}
    return 0;
}
template<class W> bool write(W& w,const Frame& f,std::uint32_t key,std::uint8_t type,std::uint16_t slot) noexcept {
    if(!body_bits(f,key,type,slot)) {return false;}
    const auto index=source_index(f.scenario,key,type,slot);
    const auto population=index<f.population.size()?f.population[index]:Population{};
    const bool blocked=population.occupied || population.suspended;
    if(type==1) {
        for(const auto& v:activity::vendors::kSources) if(v.key==key && v.source==slot) {
            coo::native_combatant::Source source{key,population.generation,static_cast<std::uint16_t>(v.rule==UINT16_MAX?0:v.rule),
                static_cast<std::uint8_t>(v.member==UINT16_MAX && !blocked?1:0),{}};
            source.hasRule=v.rule!=UINT16_MAX;source.memberOwned=v.member!=UINT16_MAX;
            return coo::native_combatant::write_source(w,source);
        }
        const VendorSource* v{};for(const auto& entry:kSources) if(entry.key==key && entry.source==slot) {v=&entry;break;}
        coo::native_combatant::Source source{key,population.generation,static_cast<std::uint16_t>(v && v->rule!=UINT16_MAX?v->rule:0),
            static_cast<std::uint8_t>(v && !blocked?1:0),{}};
        source.hasRule=v && v->rule!=UINT16_MAX;
        // No new request until the old network actor is gone and the reloaded
        // source has accepted its new generation.
        return coo::native_combatant::write_source(w,source,v!=nullptr);
    }
    if(type==2) {return blocked?coo::native_combatant::write_bind(w,population.generation)
        :coo::native_combatant::write_spawn(w,population.generation);}
    if(type==4) {
        const bool active=f.scenario!=kTower || (key==0xF8790DA5 && slot==3) || ((key==0xD227B29F || key==0x4F4ED92F) && slot==0)
            || (key==0xE7F0A95BU && slot<2) || (key==0x9052672CU && slot==0);
        return coo::native_device::object(w,1,active);
    }
    namespace animation=middleware::bap::activity_message::native::npc_animation;
    const auto sequence=idle(key,slot);
    auto counter=sequence?1U:0U;
    // Vance's retained performance counter survives the area's actor. Native
    // A0F465 compares it with the actor resource's last consumed counter before
    // issuing the idle request. A fresh source lifetime needs a fresh request;
    // approach/withdrawal and repeated publication must keep the same counter.
    if(f.scenario==0x80F4696AU && key==0x564C6ECEU && type==42 && slot==2) {
        const auto source=source_index(f.scenario,key,1,0);
        if(source<f.population.size()) {counter=f.population[source].generation;}
    }
    // Each authored graph's idle -> greeting edge consumes F3999068. Type42
    // carries it as a retained native 5E event; do not restart the idle graph.
    const std::array events{animation::Event{sequence,0xF3999068U}};
    return animation::write(w,{sequence?sequence:animation::kEmptyHash,animation::kEmptyHash,counter},
        std::span(events).first(nearby(f,key,slot)?1:0));
}
}
