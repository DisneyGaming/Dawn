#pragma once
#include "../../../middleware/bap/activity_message/native/generic_device_authority.h"
#include <algorithm>
#include <limits>

namespace sunrise::server::runtime::activity::mission_device_pose {
namespace wire=middleware::bap::activity_message::native::generic_device;
struct Sample final {
    float actual{},target{};
    std::int32_t revision{-1},snapRevision{-1};
    friend constexpr bool operator==(const Sample&,const Sample&)=default;
};
struct Publication final {
    wire::State state{};
    std::int32_t nativeRevision{-1},nativeSnapRevision{-1};
    bool published{},fault{};
};
[[nodiscard]] inline bool advance(Publication& p,float desired) noexcept {
    const auto revision=(std::max)(p.state.position.revision,p.nativeRevision);
    const auto snap=(std::max)(p.state.position.snapRevision,p.nativeSnapRevision);
    if(revision==INT32_MAX || snap==INT32_MAX) {p.fault=true;return false;}
    p.state.position={revision+1,snap+1,desired};p.published=true;return true;
}
[[nodiscard]] inline bool desire(Publication& p,float desired) noexcept {
    if(p.fault || !std::isfinite(desired))return false;
    if(p.published && p.state.position.value==desired)return true;
    return advance(p,desired);
}
// Caller authenticates the complete source/entity/device receipt. The native
// consumer accepts only a strictly greater signed32 position revision. Store
// high-water marks even on matching poses; repair drift once its native
// revision has caught up with the pending publication. Replay cannot churn it.
[[nodiscard]] inline bool observe(Publication& p,Sample sample) noexcept {
    if(p.fault || !std::isfinite(sample.actual) || !std::isfinite(sample.target)
        || sample.revision<-1 || sample.snapRevision<-1)return false;
    p.nativeRevision=(std::max)(p.nativeRevision,sample.revision);
    p.nativeSnapRevision=(std::max)(p.nativeSnapRevision,sample.snapRevision);
    if(!p.published)return true;
    const auto desired=p.state.position.value;
    if(sample.actual==desired && sample.target==desired)return true;
    if(sample.revision<p.state.position.revision)return true;
    return advance(p,desired);
}
// One latest sample per authenticated owner, independent of the event queue.
// Repeated world ticks cannot overflow event intake. Owner checks happen before
// submit; reset occurs on mission lifecycle changes. Native revisions are
// monotonic for that owner, so an older revision cannot displace newer data.
template<class Receipt> struct Inbox final {
    Receipt receipt{};Sample sample{};bool dirty{},present{};
    void submit(const Receipt& r,Sample s) noexcept {
        if(present && receipt==r) {
            if(s.revision<sample.revision || s.snapRevision<sample.snapRevision)return;
            if(s==sample)return;
        }
        receipt=r;sample=s;dirty=present=true;
    }
    template<class Apply> void drain(Apply apply) noexcept {
        if(dirty) {dirty=false;apply(receipt,sample);}
    }
};
}
