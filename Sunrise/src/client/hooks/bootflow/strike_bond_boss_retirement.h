#pragma once
#include "strike_bond_fire_trace.h"

namespace sunrise::client::hooks::bootflow::strike_bond_boss_retirement {
namespace mission=state::activity::strike_bond;
namespace trace=strike_bond_fire_trace;
inline constexpr std::uint32_t kDefinition=0x80F54740U;
inline constexpr std::int64_t kDefinitionOffset=0x728;
struct World {
    std::uintptr_t entities{};std::uint32_t stride{},space{};std::uintptr_t manager{};
    friend bool operator==(World,World)=default;
};
struct Lease {
    state::activity::coo::Generation owner{};mission::EnemyReceipt enemy{};World world{};
    std::uintptr_t actors{};std::uint32_t actorStride{};
    std::uint32_t entity{UINT32_MAX},bundle{UINT32_MAX},parent{UINT32_MAX},characterSelf{UINT32_MAX},reference{UINT32_MAX};
    friend bool operator==(const Lease&,const Lease&)=default;
};
enum class EntityState : std::uint8_t {unreadable,otherWorld,replaced,marked,live};
inline bool same_request(const mission::BossRequest& r,const Lease& lease) noexcept {
    return r.owner.valid() && r.enemy.valid() && r.owner==lease.owner && r.enemy==lease.enemy
        && r.enemy.run==r.owner.run && r.enemy.generation==r.owner.value
        && r.enemy.registry==mission::kBossActor.registry && r.enemy.source==3
        && r.frame.enabled && !r.frame.finished;
}
inline bool requested(const mission::BossRequest& r,const Lease& lease) noexcept {
    const auto* source=mission::find(mission::kBossActor.registry,1,3);
    const auto& state=r.frame.native[mission::asset_index(source->asset)];
    return same_request(r,lease) && r.frame.bossDead && state.managed && !state.active && !state.desired
        && state.generation>lease.enemy.generation;
}
inline bool row(std::uintptr_t table,std::uint32_t stride,std::uint32_t handle,std::uintptr_t& out) noexcept {
    const auto offset=static_cast<std::uintptr_t>(handle&0x1FFFU)*stride;
    if(table<0x10000 || handle==UINT32_MAX || stride<0x50 || stride>0x100000
        || table>UINTPTR_MAX-offset-0xA0) return false;
    out=table+offset;return true;
}
template<class Read> bool world(Read& read,std::uintptr_t image,World& out) noexcept {
    return read.value(image+0x1F93428,out.entities) && read.value(image+0x1F93430,out.stride)
        && read.value(image+0x1F9344C,out.space) && read.value(image+0x2744A18,out.manager)
        && out.entities>=0x10000 && out.stride>=0x9C && out.stride<=0x100000 && out.manager;
}
template<class Read> EntityState entity_state(Read& read,std::uintptr_t image,const Lease& lease) noexcept {
    World current{};std::uintptr_t entity{};std::uint32_t self{},bundle{},flags{};
    if(!world(read,image,current)) return EntityState::unreadable;
    if(current!=lease.world) return EntityState::otherWorld;
    if(!row(current.entities,current.stride,lease.entity,entity)
        || !read.value(entity+0x0C,self) || !read.value(entity+0x4C,bundle) || !read.value(entity+4,flags)) return EntityState::unreadable;
    if(self!=lease.entity || bundle!=lease.bundle) return EntityState::replaced;
    return (flags&4U)?EntityState::marked:EntityState::live;
}
template<class Read> bool source_matches(Read& read,std::uintptr_t address,const mission::EnemyReceipt& enemy) noexcept {
    std::uintptr_t resolved{},definition{};std::uint32_t value{};std::int64_t offset{};
    if(!read.resolve(enemy.owner,resolved) || resolved!=address
        || !read.value(address,value) || value!=kDefinition || !read.value(address+4,value) || value!=0x8080948FU
        || !read.value(address+8,offset) || offset!=kDefinitionOffset) return false;
    for(const auto self:{0x30U,0x48U}) {
        if(!read.value(address+self,value) || value!=enemy.owner
            || !read.value(address+self+4,value) || value!=0x80809A3BU
            || !read.value(address+self+8,offset) || offset) return false;
    }
    std::uint8_t type{};std::uint16_t slot{};
    return read.resolve(kDefinition,definition) && definition<=UINTPTR_MAX-0x760
        && read.value(definition+0x758,value) && value==enemy.registry
        && read.value(definition+0x75C,type) && type==1 && read.value(definition+0x75E,slot) && slot==3;
}
// Source detachment is native death bookkeeping, not a replacement of the
// authenticated actor. The salted actor, parent and character graph still bind
// this one entity; no raw component pointer is retained across callbacks.
template<class Read> bool identity(Read& read,std::uintptr_t image,const Lease& lease,bool dead) noexcept {
    std::uintptr_t actors{},actor{},entity{},parent{},reference{},character{};
    std::uint32_t stride{},value{},owner{};std::uint16_t flags{};std::int64_t offset{};
    if((lease.enemy.actor&0x1FFFU)>=256 || lease.parent==UINT32_MAX || lease.bundle==UINT32_MAX
        || lease.characterSelf==UINT32_MAX || lease.reference==UINT32_MAX
        || !read.value(image+0x1F9D7F8,actors) || actors!=lease.actors
        || !read.value(image+0x1F9D800,stride) || stride!=lease.actorStride || stride<0x70
        || !row(actors,stride,lease.enemy.actor,actor)
        || !read.value(actor+0x48,value) || value!=lease.enemy.actor
        || !read.value(actor+0x4C,value) || value!=lease.entity
        || !read.value(actor+0x50,value) || value!=lease.parent
        || !read.value(actor,flags) || ((flags&0x20U)!=0)!=dead
        || !read.value(actor+0x38,owner)) return false;
    if(owner!=lease.enemy.owner && !(dead && owner==UINT32_MAX)) return false;
    if(owner!=UINT32_MAX && (!read.value(actor+0x3C,value) || value!=0x80809A3BU
        || !read.value(actor+0x40,offset) || offset)) return false;
    if(entity_state(read,image,lease)!=EntityState::live
        || !read.resolve(lease.parent,parent) || !read.value(parent+4,value) || value!=0x808082ECU
        || !read.value(parent+0x24,value) || value!=lease.parent
        || !read.value(parent+0x2C,value) || value!=lease.entity
        || !read.value(parent+0x1470,value) || value!=lease.enemy.actor
        || !row(lease.world.entities,lease.world.stride,lease.entity,entity)
        || !read.value(entity+0x98,value) || value!=lease.reference
        || !read.resolve(lease.reference,reference) || !read.value(reference+4,value) || value!=0x80803A38U
        || !read.value(reference+0x24,value) || value!=lease.reference
        || !read.value(reference+0x2C,value) || value!=lease.entity
        || !read.value(reference+0x60,value) || value!=lease.characterSelf
        || !read.resolve(lease.characterSelf,character) || !read.value(character,value) || value!=0x80F459A3U
        || !read.value(character+4,value) || value!=0x80803A00U
        || !read.value(character+0x24,value) || value!=lease.characterSelf
        || !read.value(character+0x2C,value) || value!=lease.entity) return false;
    return true;
}
// Called only after trace::sample has authenticated the live callback.
template<class Read> bool capture(Read& read,std::uintptr_t image,const mission::BossRequest& r,
    const trace::Identity& character,Lease& out) noexcept {
    if(!trace::admitted(r)) return false;
    Lease lease{};lease.owner=r.owner;lease.enemy=r.enemy;lease.entity=character.entity;lease.characterSelf=character.self;
    std::uintptr_t actor{},entity{},resolved{};
    if(!world(read,image,lease.world) || !read.value(image+0x1F9D7F8,lease.actors)
        || !read.value(image+0x1F9D800,lease.actorStride) || lease.actorStride<0x70
        || !row(lease.actors,lease.actorStride,r.enemy.actor,actor) || !read.value(actor+0x50,lease.parent)
        || !row(lease.world.entities,lease.world.stride,lease.entity,entity)
        || !read.value(entity+0x4C,lease.bundle) || !read.value(entity+0x98,lease.reference)
        || !read.resolve(character.self,resolved) || resolved!=character.character
        || !identity(read,image,lease,false)) return false;
    out=lease;return true;
}
template<class Read> bool validate(Read& read,std::uintptr_t image,const mission::BossRequest& r,
    std::uintptr_t sourceAddress,const Lease& lease) noexcept {
    std::uint32_t generation{};
    if(!requested(r,lease) || !source_matches(read,sourceAddress,lease.enemy)
        || !read.value(sourceAddress+0x1FC,generation)
        || (generation!=lease.enemy.generation && generation!=r.frame.native[mission::asset_index(mission::find(mission::kBossActor.registry,1,3)->asset)].generation)) return false;
    // 4E95DD calls this retirement boundary before 4E964D updates +244.
    return read.value(sourceAddress+0x244,generation) && generation==lease.enemy.generation
        && identity(read,image,lease,true);
}
}
