#pragma once
#include "placement_ownership.h"
#include "../coo/lifecycle_service.h"
namespace sunrise::state::activity::hijacked {
struct RetirementWorld {
    std::uintptr_t entities{};std::uint32_t stride{},space{};std::uintptr_t manager{};
    friend bool operator==(RetirementWorld,RetirementWorld)=default;
};
struct RetirementEnemy {
    coo::Generation run{};RetirementWorld world{};std::uint16_t source{};
    std::uint32_t owner{UINT32_MAX},actor{UINT32_MAX},parent{UINT32_MAX},entity{UINT32_MAX},bundle{UINT32_MAX};
};
enum class RetirementRead : std::uint8_t { unreadable, otherWorld, replaced, removed, live };
inline RetirementRead retirement_entity_state(const RetirementEnemy& target,RetirementWorld world,
    bool worldReadable,bool entityReadable,std::span<const std::byte> entity) noexcept {
    if(!worldReadable) {return RetirementRead::unreadable;}
    if(world!=target.world) {return RetirementRead::otherWorld;}
    if(!entityReadable || entity.size()<0x50) {return RetirementRead::unreadable;}
    if(placement_field<std::uint32_t>(entity,0x0C)!=target.entity
        || placement_field<std::uint32_t>(entity,0x4C)!=target.bundle) {return RetirementRead::replaced;}
    return (placement_field<std::uint32_t>(entity,4)&4U)?RetirementRead::removed:RetirementRead::live;
}
inline bool retirement_expired(RetirementRead state) noexcept {
    return state==RetirementRead::replaced || state==RetirementRead::removed;
}
struct RetirementOrigin {
    coo::Generation run{};std::uintptr_t actors{};std::uint32_t stride{};std::uint16_t source{};
    std::uint32_t owner{UINT32_MAX},actor{UINT32_MAX},parent{UINT32_MAX};
};
inline bool retirement_origin_matches(const RetirementOrigin& origin,std::uintptr_t actors,std::uint32_t stride,
    std::span<const std::byte> actor) noexcept {
    if(!origin.run.valid() || origin.source<1 || origin.source>7 || !actors || stride<0x68
        || origin.actors!=actors || origin.stride!=stride || actor.size()<0x68
        || origin.owner==UINT32_MAX || origin.actor==UINT32_MAX || origin.parent==UINT32_MAX) {return false;}
    const auto owner=placement_field<std::uint32_t>(actor,0x38);
    return placement_field<std::uint32_t>(actor,0x48)==origin.actor
        && placement_field<std::uint32_t>(actor,0x50)==origin.parent
        && (owner==UINT32_MAX || (owner==origin.owner
            && placement_field<std::uint32_t>(actor,0x3C)==0x80809A3BU
            && placement_field<std::int64_t>(actor,0x40)==0));
}
inline bool capture_retirement_origin(coo::Generation run,std::uint16_t source,std::uint32_t owner,
    std::uintptr_t actors,std::uint32_t stride,std::span<const std::byte> actor,RetirementOrigin& out) noexcept {
    if(actor.size()<0x68 || placement_field<std::uint32_t>(actor,0x38)!=owner) {return false;}
    RetirementOrigin candidate{run,actors,stride,source,owner,
        placement_field<std::uint32_t>(actor,0x48),placement_field<std::uint32_t>(actor,0x50)};
    if(!retirement_origin_matches(candidate,actors,stride,actor)) {return false;}
    out=candidate;return true;
}
inline bool retirement_entity_matches(const RetirementEnemy& target,RetirementWorld world,
    std::span<const std::byte> entity) noexcept {
    return target.run.valid() && target.source>=1 && target.source<=7 && target.world==world
        && world.entities && world.stride>=0x50 && entity.size()>=0x50
        && target.actor!=UINT32_MAX && target.parent!=UINT32_MAX && target.owner!=UINT32_MAX
        && target.entity!=UINT32_MAX && target.bundle!=UINT32_MAX
        && placement_field<std::uint32_t>(entity,0x0C)==target.entity
        && placement_field<std::uint32_t>(entity,0x4C)==target.bundle
        && (placement_field<std::uint32_t>(entity,4)&4U)==0;
}
// Streaming can free/reuse the actor while the captured entity stays alive.
// Only an actor still referring to this exact entity can change its ownership.
inline bool retirement_actor_allows(const RetirementEnemy& target,std::span<const std::byte> actor) noexcept {
    if(actor.size()<0x68) {return false;}
    if(placement_field<std::uint32_t>(actor,0x4C)!=target.entity) {return true;}
    const auto owner=placement_field<std::uint32_t>(actor,0x38);
    return owner==target.owner || owner==UINT32_MAX;
}
inline bool retirement_entity_matches(const RetirementEnemy& target,RetirementWorld world,
    std::span<const std::byte> actor,std::span<const std::byte> entity) noexcept {
    return retirement_entity_matches(target,world,entity) && actor.size()>=0x68
        && placement_field<std::uint32_t>(actor,0x48)==target.actor
        && placement_field<std::uint32_t>(actor,0x50)==target.parent
        && placement_field<std::uint32_t>(actor,0x4C)==target.entity
        && retirement_actor_allows(target,actor);
}
inline bool capture_retirement_enemy(coo::Generation run,std::uint16_t source,std::uint32_t owner,
    RetirementWorld world,std::span<const std::byte> actor,std::span<const std::byte> entity,RetirementEnemy& out) noexcept {
    if(actor.size()<0x68 || entity.size()<0x50
        || placement_field<std::uint32_t>(actor,0x38)!=owner
        || placement_field<std::uint32_t>(actor,0x3C)!=0x80809A3BU
        || placement_field<std::int64_t>(actor,0x40)!=0) {return false;}
    RetirementEnemy candidate{run,world,source,owner,placement_field<std::uint32_t>(actor,0x48),
        placement_field<std::uint32_t>(actor,0x50),placement_field<std::uint32_t>(actor,0x4C),
        placement_field<std::uint32_t>(entity,0x4C)};
    if(!retirement_entity_matches(candidate,world,actor,entity)) {return false;}
    out=candidate;return true;
}
inline bool bind_retirement_entity(const RetirementOrigin& origin,std::uintptr_t actors,std::uint32_t stride,
    std::span<const std::byte> actor,RetirementWorld world,std::span<const std::byte> entity,RetirementEnemy& out) noexcept {
    if(!retirement_origin_matches(origin,actors,stride,actor) || entity.size()<0x50) {return false;}
    RetirementEnemy candidate{origin.run,world,origin.source,origin.owner,origin.actor,origin.parent,
        placement_field<std::uint32_t>(actor,0x4C),placement_field<std::uint32_t>(entity,0x4C)};
    if(!retirement_entity_matches(candidate,world,actor,entity)) {return false;}
    out=candidate;return true;
}

}
