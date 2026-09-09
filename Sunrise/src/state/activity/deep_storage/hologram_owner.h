#pragma once
#include "frame.h"
namespace sunrise::state::activity::deep_storage::hologram_owner {
inline constexpr coo::Asset kSource{0x59700FA7U,0x80B568AFU,4,82};
struct Generic {
    std::uint32_t definition{},kind{},self{UINT32_MAX},entity{UINT32_MAX};
    std::uint64_t offset{};
};
// The live dome retains its prepared entity: applied source generation2,
// creation generation1. Admit only that same-lease transition for this source,
// with the package-linked generic component owned by the salted source entity.
// The caller resolves/revalidates both ownership handles and source fields.
inline bool retained(coo::Generation owner,coo::Asset source,const NativeState& desired,
                     std::uint32_t applied,std::uint32_t committed,std::uint32_t entity,Generic generic) noexcept {
    return owner.valid() && owner.value<32766 && source==kSource
        && desired.managed && desired.desired && desired.prepared && desired.active
        && applied==desired.generation && applied==owner.value+1U && committed==owner.value
        && entity!=UINT32_MAX && generic.entity==entity && generic.self!=UINT32_MAX
        && generic.definition==0x80FD20CDU && generic.kind==0x80803910U && generic.offset==0xA78U;
}
}
