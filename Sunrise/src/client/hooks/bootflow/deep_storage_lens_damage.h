#pragma once
#include "native_box_identity.h"
#include "../../../state/activity/deep_storage/runtime.h"
namespace sunrise::client::hooks::bootflow::deep_storage_lens_damage {
namespace ds=state::activity::deep_storage;
// One source-authenticated candidate closes the window between native source
// observation and controller receipt binding. Its lifecycle is distinct from
// the lens object's generation and must match the current mission request.
struct Candidate { state::activity::coo::Generation lifecycle{};ds::LensReceipt lens{}; };
inline ds::LensReceipt owner(const ds::LensRequest& request,const Candidate& candidate) noexcept {
    if(!request.enabled || request.destroyed || !request.owner.valid()) { return {}; }
    const auto lens=request.lens.valid()?request.lens:candidate.lifecycle==request.owner?candidate.lens:ds::LensReceipt{};
    if(!lens.valid() || lens.owner.run!=request.owner.run || lens.owner.value!=request.objectGeneration) { return {}; }
    return lens;
}
template<class Read> bool current(Read& read,const ds::LensRequest& request,const Candidate& candidate,
    const native_box_identity::Sample& sample) noexcept {
    const auto lens=owner(request,candidate);
    return lens.valid() && native_box_identity::current(read,
        {lens.source,lens.owner.value,lens.serial,lens.entity,lens.health},sample,0x80B568A6U)
        && read.weak({lens.serial,lens.entity});
}
inline bool blocked(const ds::LensRequest& request,bool current) noexcept {
    return request.enabled && request.owner.valid() && !request.destroyed && current
        && (!request.vulnerable || !request.lens.valid());
}
inline bool allowed(const ds::LensRequest& request,bool current,bool nativeResult) noexcept {
    if(!current || !request.enabled || !request.owner.valid() || request.destroyed) { return nativeResult; }
    return request.vulnerable && request.lens.valid();
}
}
