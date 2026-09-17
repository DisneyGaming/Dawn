#pragma once
#include "gateway_module_identity.h"
#include "../../../state/activity/gateway/ending_receipts.h"
#include <array>
namespace dawn::client::hooks::bootflow::gateway_module_damage {
namespace gateway=state::activity::gateway;
using native_box_identity::Sample;
using native_box_identity::sample;
template<class Read> bool current(Read& read,const gateway::EndingRequest& request,const Sample& sample) noexcept {
    const auto& owner=request.owner;
    return request.enabled && !request.moduleDestroyed && owner.valid() && owner.run==request.run
        && owner.generation==request.generation
        && native_box_identity::current(read,{owner.source,owner.generation,owner.serial,owner.entity,owner.health},sample,0x80F46F23U);
}
inline bool blocked(const gateway::EndingRequest& request) noexcept {
    return request.enabled && request.run!=0 && !request.moduleVulnerable && !request.moduleDestroyed;
}
// CDCB60 controls the lethal-health branch (B815FA), not every damage side effect.
// B804E0 is suppressed while protected. Once exposed, the native pipeline owns
// damage amounts, health loss, destruction and effects, with death permitted.
inline bool allowed(const gateway::EndingRequest& request,bool current,bool nativeResult) noexcept {
    if(!request.enabled || request.run==0 || request.moduleDestroyed) { return nativeResult; }
    if(!request.moduleVulnerable) { return false; } // Also protect the first frame before binding.
    return current?true:nativeResult;
}
}
