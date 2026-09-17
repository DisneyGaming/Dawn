#pragma once
#include "retirement_identity.h"
namespace dawn::state::activity::hijacked {
// Recovered native unload input and the one predicate call whose output is
// boolean-only. Other bundle lookups must receive the original result/output.
inline constexpr std::uint32_t kMistsUnloadRegion=264;
inline constexpr std::uintptr_t kUnloadDiscardCaller=0xA07CBE;
inline constexpr std::uint32_t kUnloadDiscardComponent=0x80807E3EU;
inline bool retirement_unload_matches(const RetirementEnemy& target,coo::Generation run,
    std::uint32_t destination,std::uintptr_t caller,std::uint32_t component,
    std::uintptr_t address,std::uint32_t bundle,RetirementWorld world,
    std::span<const std::byte> entity) noexcept {
    if(destination!=kMistsUnloadRegion || caller!=kUnloadDiscardCaller
        || component!=kUnloadDiscardComponent || target.run!=run
        || bundle!=target.bundle || !retirement_entity_matches(target,world,entity)) {return false;}
    const auto offset=static_cast<std::uintptr_t>(target.entity&0x1FFFU)*world.stride;
    return world.entities<=UINTPTR_MAX-offset && address==world.entities+offset;
}
}
