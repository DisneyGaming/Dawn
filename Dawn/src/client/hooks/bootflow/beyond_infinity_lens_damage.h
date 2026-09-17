#pragma once
#include "native_box_identity.h"
#include "../../../state/activity/beyond_infinity/runtime.h"
namespace dawn::client::hooks::bootflow::beyond_infinity_lens_damage {
namespace bi=state::activity::beyond_infinity;
// One source-authenticated candidate closes the window between native source
// observation and controller receipt binding. Its lifecycle is distinct from
// the lens object's generation and must match the current mission request.
struct Candidate { state::activity::coo::Generation lifecycle{};bi::LensReceipt lens{}; };
inline bi::LensReceipt owner(const bi::LensRequest& request,const Candidate& candidate) noexcept {
    if(!request.enabled || request.destroyed || !request.owner.valid()) { return {}; }
    const auto lens=request.lens.valid()?request.lens:candidate.lifecycle==request.owner?candidate.lens:bi::LensReceipt{};
    if(!lens.valid() || lens.owner.run!=request.owner.run || lens.owner.value!=request.objectGeneration) { return {}; }
    return lens;
}
template<class Read> bool current(Read& read,const bi::LensRequest& request,const Candidate& candidate,
    const native_box_identity::Sample& sample) noexcept {
    const auto lens=owner(request,candidate);
    return lens.valid() && native_box_identity::current(read,
        {lens.source,lens.owner.value,lens.serial,lens.entity,lens.health},sample,0x80F462B3U)
        && read.weak({lens.serial,lens.entity});
}
inline bool blocked(const bi::LensRequest& request,bool current) noexcept {
    return request.enabled && request.owner.valid() && !request.destroyed && current
        && (!request.vulnerable || !request.lens.valid());
}
inline bool allowed(const bi::LensRequest& request,bool current,bool nativeResult) noexcept {
    if(!current || !request.enabled || !request.owner.valid() || request.destroyed) { return nativeResult; }
    return request.vulnerable && request.lens.valid();
}
}
