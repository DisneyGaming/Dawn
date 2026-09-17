#pragma once
#include "coo_enemy_readiness.h"
#include "omega_boss_health_identity.h"
#include "../../../state/activity/strike_bond/boss_damage.h"
namespace dawn::client::hooks::bootflow::strike_bond_boss_damage {
namespace mission=state::activity::strike_bond;
struct Sample {std::uintptr_t health{};std::uint32_t handle{UINT32_MAX};std::int32_t bodyRegion{};};
// Resolve the admitted source, salted actor, character health reference, and
// exact Dendron body definition afresh at the existing damage boundary.
template<class Read> bool sample(Read& read,std::uintptr_t image,std::uintptr_t context,
                                 const mission::BossRequest& request,Sample& out) noexcept {
    if(!request.frame.enabled || request.frame.finished || !request.owner.valid() || !request.enemy.valid() || request.enemy.run!=request.owner.run
        || request.enemy.generation!=request.owner.value || request.enemy.registry!=mission::kBossActor.registry
        || request.enemy.source!=3 || request.frame.bossStage>2) {return false;}
    std::uintptr_t health{},definition{},base{},resolved{},region{};std::array<std::byte,0x30> header{};
    if(!read.value(context,definition) || !read.value(context+8,health) || !read.copy(health,header)
        || mission::boss_damage::get<std::uint32_t>(header,0)!=0x815B5A47U
        || mission::boss_damage::get<std::uint32_t>(header,4)!=0x80804B8AU
        || mission::boss_damage::get<std::int64_t>(header,8)!=0xF28
        || !read.resolve(0x815B5A47U,base) || definition!=base+0xF28) {return false;}
    const auto ready=coo_native::enemy(read,image,request.enemy);
    if(!ready.created || !ready.health || ready.healthHandle!=mission::boss_damage::get<std::uint32_t>(header,0x24)
        || !read.resolve(ready.healthHandle,resolved) || resolved!=health) {return false;}
    std::int32_t count{};std::int64_t relative{};std::int16_t bias{};
    if(!read.value(definition+0x1B0,count) || count<1 || count>32
        || !read.value(definition+0x1B8,relative) || relative<=0 || relative>0x100000
        || !read.value(health+0x6F8,bias) || bias<0) {return false;}
    region=definition+0x1C8+static_cast<std::uintptr_t>(relative);
    std::array<std::byte,0xD4> body{};std::array<std::byte,16> prefix{};std::array<std::byte,0x30> again{};
    if(region!=base+0x1540 || !read.copy(region,body)
        || mission::boss_damage::get<std::uint32_t>(body,0)!=0x815B5A47U
        || mission::boss_damage::get<std::uint32_t>(body,4)!=0x80804BABU
        || mission::boss_damage::get<std::int64_t>(body,8)!=0xE60
        || mission::boss_damage::get<std::uint32_t>(body,0x10)!=0x6DFE676DU
        || mission::boss_damage::get<std::uint32_t>(body,0xD0)!=0
        || !read.copy(image+0xCD6C20,prefix) || !omega_boss_health::fraction_getter_prefix(prefix)
        || !read.copy(health,again) || again!=header) {return false;}
    out={health,ready.healthHandle,bias};return true;
}
}
