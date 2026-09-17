#pragma once
#include "coo_enemy_readiness.h"
#include "omega_boss_health_identity.h"
#include "../../../state/activity/strike_pact/boss_damage.h"
#include "../../../state/activity/strike_pact/catalog_boss.h"
namespace dawn::client::hooks::bootflow::strike_pact_boss_damage {
namespace mission=state::activity::strike_pact;
struct Sample {std::uintptr_t health{};std::uint32_t handle{UINT32_MAX};std::int32_t bodyRegion{};};
// Resolve the admitted source, salted actor, character health reference, and
// exact Valus Thuun body definition afresh at the existing damage boundary.
template<class Read> bool sample(Read& read,std::uintptr_t image,std::uintptr_t context,
                                 const mission::BossRequest& request,Sample& out) noexcept {
    if(!request.owner.valid() || !request.enemy.valid() || request.enemy.run!=request.owner.run
        || request.enemy.generation!=request.owner.value || request.enemy.registry!=mission::kBoss
        || request.enemy.source!=mission::kBossSquad || request.stage>2) {return false;}
    std::uintptr_t health{},definition{},base{},resolved{},region{};std::array<std::byte,0x30> header{};
    if(!read.value(context,definition) || !read.value(context+8,health) || !read.copy(health,header)
        || mission::boss_damage::get<std::uint32_t>(header,0)!=0x815B5A4DU
        || mission::boss_damage::get<std::uint32_t>(header,4)!=0x80804B8AU
        || mission::boss_damage::get<std::int64_t>(header,8)!=0x1158
        || !read.resolve(0x815B5A4DU,base) || definition!=base+0x1158) {return false;}
    const auto ready=coo_native::enemy(read,image,request.enemy);
    if(!ready.created || !ready.health || ready.healthHandle!=mission::boss_damage::get<std::uint32_t>(header,0x24)
        || !read.resolve(ready.healthHandle,resolved) || resolved!=health) {return false;}
    std::int32_t count{};std::int64_t relative{};std::int16_t bias{};
    if(!read.value(definition+0x1B0,count) || count<1 || count>32
        || !read.value(definition+0x1B8,relative) || relative<=0 || relative>0x100000
        || !read.value(health+0x6F8,bias) || bias<0) {return false;}
    region=definition+0x1C8+static_cast<std::uintptr_t>(relative);
    std::array<std::byte,0xD4> body{};std::array<std::byte,16> prefix{};std::array<std::byte,0x30> again{};
    if(region!=base+0x1900 || !read.copy(region,body)
        || mission::boss_damage::get<std::uint32_t>(body,0)!=0x815B5A4DU
        || mission::boss_damage::get<std::uint32_t>(body,4)!=0x80804BABU
        || mission::boss_damage::get<std::int64_t>(body,8)!=0x1010
        || mission::boss_damage::get<std::uint32_t>(body,0x10)!=0x6DFE676DU
        || mission::boss_damage::get<std::uint32_t>(body,0xD0)!=0
        || !read.copy(image+0xCD6C20,prefix) || !omega_boss_health::fraction_getter_prefix(prefix)
        || !read.copy(health,again) || again!=header) {return false;}
    out={health,ready.healthHandle,bias};return true;
}
}
