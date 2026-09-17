#pragma once
#include <cstddef>
#include <limits>

namespace dawn::server::bap::encrypted::lost_sector_rewards::detail {
inline constexpr std::size_t kNoClaimSlot=std::numeric_limits<std::size_t>::max();
inline constexpr std::size_t kDuplicateClaimSlot=kNoClaimSlot-1;

// The caller may recycle a delivered entry only after proving that entry's
// native clear ticket is no longer current. A still-valid delivered record is
// retained so an accepted-use replay cannot queue a duplicate grant.
template<class Range,class Occupied,class Recyclable,class Duplicate>
[[nodiscard]] constexpr std::size_t select_claim_slot(const Range& claims,
    Occupied&& occupied,Recyclable&& recyclable,Duplicate&& duplicate) noexcept {
    std::size_t empty=kNoClaimSlot,deliveredSlot=kNoClaimSlot;
    for(std::size_t i=0;i<claims.size();++i) {
        const auto& claim=claims[i];
        if(occupied(claim) && duplicate(claim))return kDuplicateClaimSlot;
        if(!occupied(claim) && empty==kNoClaimSlot)empty=i;
        else if(recyclable(claim) && deliveredSlot==kNoClaimSlot)deliveredSlot=i;
    }
    return empty!=kNoClaimSlot?empty:deliveredSlot;
}
}
