#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace dawn::client::hooks::bootflow::eater_platform_contact_native {

inline constexpr std::uint32_t kPhysicsKind=0x80808A0CU;
inline constexpr std::uint32_t kPlayerPhysicsConfig=0x80C0C518U;
inline constexpr std::int64_t kPlayerPhysicsOffset=0x660;
inline constexpr std::uint32_t kPlatformPhysicsConfig=0x80F42FA5U;
inline constexpr std::int64_t kPlatformPhysicsOffset=0x378;
// The platform-specific child is an authored cylinder volume. Live movement
// through its manifold proves membership, not floor support: the normal points
// down at the centre and sideways at the edge while the player remains inside.
inline constexpr std::uint32_t kPlatformVolumeConfig=0x80F42FB5U;
inline constexpr std::uint32_t kPlatformVolumeKind=0x8080929EU;
inline constexpr std::int64_t kPlatformVolumeOffset=0x1B0;
inline constexpr std::size_t kPlatformVolumeBodyOffset=0x88;
inline constexpr std::uintptr_t kRigidBodyVtableRva=0x1BAC0D8;
inline constexpr std::uintptr_t kCylinderShapeVtableRva=0x1BA5EE8;
inline constexpr std::uintptr_t kContactManagerVtableRva=0x1BAC4C0;
inline constexpr std::uintptr_t kWorldVtableRva=0x1BAAC28;
inline constexpr std::size_t kMaximumBodies=64;
inline constexpr std::size_t kMaximumCollisions=64;
inline constexpr std::size_t kMaximumContacts=64;

struct CollisionRow final {
    std::uintptr_t link{};
    std::uintptr_t partnerCollidable{};
    friend bool operator==(const CollisionRow&,const CollisionRow&)=default;
};
static_assert(sizeof(CollisionRow)==16);

struct ContactPoint final {
    std::array<float,4> position{};
    // Havok stores its separating normal from manager body B toward body A.
    // The fourth lane is signed separation distance.
    std::array<float,4> normalDistance{};
    friend bool operator==(const ContactPoint&,const ContactPoint&)=default;
};
static_assert(sizeof(ContactPoint)==32);

enum class Occupancy : std::uint8_t {invalid,absent,present};
inline constexpr bool can_add(std::uintptr_t value,std::size_t bytes) noexcept {
    return value>=0x10000 && value<=UINTPTR_MAX-bytes;
}

template<class Read>
bool platform_volume_body(Read& read,std::uintptr_t image,std::uintptr_t component,
                          std::uint32_t entity,std::uintptr_t& body) noexcept {
    if(!image || !can_add(image,kCylinderShapeVtableRva)
        || !can_add(component,kPlatformVolumeBodyOffset+sizeof(std::uintptr_t))) return false;
    std::array<std::byte,0x90> header{};
    if(!read.copy(component,header)) return false;
    const auto at32=[&](std::size_t offset) noexcept {
        std::uint32_t value{};std::memcpy(&value,header.data()+offset,sizeof value);return value;
    };
    const auto at64=[&](std::size_t offset) noexcept {
        std::uintptr_t value{};std::memcpy(&value,header.data()+offset,sizeof value);return value;
    };
    const auto self=at32(0x24);
    body=at64(kPlatformVolumeBodyOffset);
    std::uintptr_t resolved{},bodyVtable{},shape{},shapeVtable{};
    if(at32(0)!=kPlatformVolumeConfig || at32(4)!=kPlatformVolumeKind
        || at64(8)!=static_cast<std::uintptr_t>(kPlatformVolumeOffset)
        || self==UINT32_MAX || at32(0x2C)!=entity
        || !read.resolve(self,resolved) || resolved!=component
        || !can_add(body,0x28) || !read.value(body,bodyVtable)
        || bodyVtable!=image+kRigidBodyVtableRva
        || !read.value(body+0x20,shape) || !can_add(shape,8)
        || !read.value(shape,shapeVtable) || shapeVtable!=image+kCylinderShapeVtableRva) return false;

    // A recycled bundle/component/body cannot become an occupancy receipt.
    std::array<std::byte,0x90> again{};std::uintptr_t finalResolved{},finalBodyVtable{};
    std::uintptr_t finalShape{},finalShapeVtable{};
    return read.copy(component,again) && again==header
        && read.resolve(self,finalResolved) && finalResolved==component
        && read.value(body,finalBodyVtable) && finalBodyVtable==bodyVtable
        && read.value(body+0x20,finalShape) && finalShape==shape
        && read.value(shape,finalShapeVtable) && finalShapeVtable==shapeVtable;
}

template<class Read>
Occupancy scan_once(Read& read,std::uintptr_t image,std::uintptr_t playerBody,
                    std::uintptr_t platformVolumeBody) noexcept {
    if(!image || !can_add(playerBody,0xA0) || !can_add(platformVolumeBody,0x20))
        return Occupancy::invalid;
    std::uintptr_t vtable{},collisions{};std::int32_t collisionCount{};
    std::uint32_t collisionCapacity{};
    if(!read.value(playerBody,vtable) || vtable!=image+kRigidBodyVtableRva
        || !read.value(playerBody+0x90,collisions)
        || !read.value(playerBody+0x98,collisionCount)
        || !read.value(playerBody+0x9C,collisionCapacity)
        || collisionCount<0 || collisionCount>static_cast<std::int32_t>(kMaximumCollisions)
        || (collisionCapacity&0x3FFFFFFFU)<static_cast<std::uint32_t>(collisionCount)
        || (collisionCount && collisions<0x10000)) return Occupancy::invalid;

    std::array<CollisionRow,kMaximumCollisions> rows{};
    if(collisionCount && !read.copy(collisions,std::as_writable_bytes(
        std::span{rows.data(),static_cast<std::size_t>(collisionCount)}))) return Occupancy::invalid;

    bool occupied{};
    for(std::int32_t i=0;i<collisionCount;++i) {
        const auto& row=rows[static_cast<std::size_t>(i)];
        if(row.partnerCollidable!=platformVolumeBody+0x20) continue;
        const auto platformBody=platformVolumeBody;

        std::uintptr_t platformVtable{},manager{},managerVtable{},bodyA{},bodyB{},atom{};
        std::uintptr_t managerWorld{},playerWorld{},platformWorld{},worldVtable{},collisionInput{},dispatcher{},worldDispatcher{};
        std::uint16_t contactCount{};
        float tolerance{};
        if(!can_add(row.link,8) || !read.value(platformBody,platformVtable)
            || platformVtable!=image+kRigidBodyVtableRva
            // Original 11DE80 returns [collision-row.first + 8].
            || !read.value(row.link+8,manager) || manager<0x10000
            || !can_add(manager,0xB0) || !read.value(manager,managerVtable)
            || managerVtable!=image+kContactManagerVtableRva
            || !read.value(manager+0xA0,bodyA) || !read.value(manager+0xA8,bodyB)
            || !((bodyA==playerBody && bodyB==platformBody)
                || (bodyA==platformBody && bodyB==playerBody))
            || !read.value(manager+0x18,managerWorld) || !can_add(managerWorld,0xD0)
            || !read.value(playerBody+0x10,playerWorld) || playerWorld!=managerWorld
            || !read.value(platformBody+0x10,platformWorld) || platformWorld!=managerWorld
            || !read.value(managerWorld,worldVtable) || worldVtable!=image+kWorldVtableRva
            || !read.value(managerWorld+0xB8,collisionInput) || !can_add(collisionInput,0x14)
            || !read.value(managerWorld+0xC8,worldDispatcher) || worldDispatcher<0x10000
            || !read.value(collisionInput,dispatcher) || dispatcher!=worldDispatcher
            || !read.value(collisionInput+0x10,tolerance) || !std::isfinite(tolerance)
            || tolerance<0.F
            || !read.value(manager+0x68,atom)
            || !can_add(atom,0x30+kMaximumContacts*sizeof(ContactPoint))
            || !read.value(atom+4,contactCount) || contactCount>kMaximumContacts)
            return Occupancy::invalid;

        std::array<ContactPoint,kMaximumContacts> points{};
        for(std::uint16_t contact=0;contact<contactCount;++contact) {
            auto& point=points[contact];
            if(!read.copy(atom+0x30+static_cast<std::uintptr_t>(contact)*sizeof(ContactPoint),
                          std::as_writable_bytes(std::span{&point,std::size_t{1}})))
                return Occupancy::invalid;
            for(const auto value:point.position) if(!std::isfinite(value)) return Occupancy::invalid;
            for(const auto value:point.normalDistance) if(!std::isfinite(value)) return Occupancy::invalid;
            if(point.normalDistance[3]>tolerance) return Occupancy::invalid;
            const auto nx=static_cast<double>(point.normalDistance[0]);
            const auto ny=static_cast<double>(point.normalDistance[1]);
            const auto nz=static_cast<double>(point.normalDistance[2]);
            const auto squared=nx*nx+ny*ny+nz*nz;
            if(!std::isfinite(squared) || squared<.98 || squared>1.02)
                return Occupancy::invalid;
            // The exact 80F42FB5 cylinder is an authored platform-attached volume.
            // Any stable native manifold point establishes membership. Normal
            // direction is geometry-dependent and deliberately has no standing
            // meaning; the controller applies the separate 500 ms solo dwell.
            occupied=true;
        }
        std::uintptr_t finalManager{},finalVtable{},finalA{},finalB{},finalAtom{};
        std::uintptr_t finalManagerWorld{},finalPlayerWorld{},finalPlatformWorld{},finalWorldVtable{},finalInput{},finalDispatcher{},finalWorldDispatcher{};
        std::uint16_t finalCount{};
        float finalTolerance{};
        if(!read.value(row.link+8,finalManager) || finalManager!=manager
            || !read.value(manager,finalVtable) || finalVtable!=managerVtable
            || !read.value(manager+0xA0,finalA) || finalA!=bodyA
            || !read.value(manager+0xA8,finalB) || finalB!=bodyB
            || !read.value(manager+0x18,finalManagerWorld) || finalManagerWorld!=managerWorld
            || !read.value(playerBody+0x10,finalPlayerWorld) || finalPlayerWorld!=playerWorld
            || !read.value(platformBody+0x10,finalPlatformWorld) || finalPlatformWorld!=platformWorld
            || !read.value(managerWorld,finalWorldVtable) || finalWorldVtable!=worldVtable
            || !read.value(managerWorld+0xB8,finalInput) || finalInput!=collisionInput
            || !read.value(managerWorld+0xC8,finalWorldDispatcher) || finalWorldDispatcher!=worldDispatcher
            || !read.value(collisionInput,finalDispatcher) || finalDispatcher!=dispatcher
            || !read.value(collisionInput+0x10,finalTolerance) || finalTolerance!=tolerance
            || !read.value(manager+0x68,finalAtom) || finalAtom!=atom
            || !read.value(atom+4,finalCount) || finalCount!=contactCount) return Occupancy::invalid;
        for(std::uint16_t contact=0;contact<contactCount;++contact) {
            ContactPoint again{};
            if(!read.copy(atom+0x30+static_cast<std::uintptr_t>(contact)*sizeof(ContactPoint),
                          std::as_writable_bytes(std::span{&again,std::size_t{1}}))
                || again!=points[contact]) return Occupancy::invalid;
        }
    }
    std::uintptr_t finalVtable{},finalCollisions{};std::int32_t finalCount{};
    std::uint32_t finalCapacity{};std::array<CollisionRow,kMaximumCollisions> finalRows{};
    if(!read.value(playerBody,finalVtable) || finalVtable!=vtable
        || !read.value(playerBody+0x90,finalCollisions) || finalCollisions!=collisions
        || !read.value(playerBody+0x98,finalCount) || finalCount!=collisionCount
        || !read.value(playerBody+0x9C,finalCapacity) || finalCapacity!=collisionCapacity
        || (finalCount && !read.copy(finalCollisions,std::as_writable_bytes(
            std::span{finalRows.data(),static_cast<std::size_t>(finalCount)})))) return Occupancy::invalid;
    for(std::int32_t i=0;i<finalCount;++i)
        if(finalRows[static_cast<std::size_t>(i)]!=rows[static_cast<std::size_t>(i)]) return Occupancy::invalid;
    return occupied?Occupancy::present:Occupancy::absent;
}

// Two identical classifications bracket the owning component/source rechecks in
// production. A volume membership that changes while copied is not a receipt.
template<class Read>
Occupancy stable_occupancy(Read& read,std::uintptr_t image,std::uintptr_t playerBody,
                           std::uintptr_t platformVolumeBody) noexcept {
    const auto first=scan_once(read,image,playerBody,platformVolumeBody);
    if(first==Occupancy::invalid) return first;
    const auto second=scan_once(read,image,playerBody,platformVolumeBody);
    return second==first?first:Occupancy::invalid;
}

} // namespace dawn::client::hooks::bootflow::eater_platform_contact_native
