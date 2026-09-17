#pragma once

#include <string_view>
#include <span>
#include "../../../state/activity/lifecycle_generation.h"
#include "../../../middleware/bap/activity_message/adventure_start_request.h"
#include "../../../middleware/bap/activity_message/native/placement_authority.h"

namespace sunrise::server::runtime::activity::adventure_start {
inline constexpr bool kLaunchesEnabled=false;
namespace wire=middleware::bap::activity_message::adventure_start;
namespace placements=middleware::bap::activity_message::native::placement;

// Trusted activity data, not a message-controlled destination table.
struct Route final {
    std::uint32_t scenario{};
    std::string_view rootPackage{};
    std::uint32_t registry{};
    std::uint16_t slot{};
    std::uint8_t bubble{};
    std::int16_t activity{-1};
};
struct Context final {
    state::activity::ActivityInstanceKey owner{};
    std::uint32_t scenario{};
    std::uint64_t expectedRecordRevision{};
    wire::Request current{};
    placements::Batch published{};
};
struct Plan final {
    state::activity::ActivityInstanceKey owner{};
    std::uint64_t expectedRecordRevision{};
    wire::Request request{};
    Route route{};
};
inline void restrict_banners(std::span<const Route> routes,placements::Batch& batch) noexcept {
    if(kLaunchesEnabled)return;
    for(std::size_t i=0;i<batch.count && i<batch.entries.size();++i) {
        auto& request=batch.entries[i];
        for(const auto& route:routes)if(route.registry==request.registry && route.slot==request.slot
            && route.bubble==request.bubble) {
            request.generation=1;request.active=false;
            request.interactionMode=middleware::bap::activity_message::native::interaction::Mode::disabled;
        }
    }
}

enum class Result : std::uint8_t {
    accepted,invalidOwner,invalidSelection,identityMismatch,staleRevision,
    unsupportedRoute,placementUnavailable
};
[[nodiscard]] inline std::string_view package(const wire::Request& request) noexcept {
    const auto& s=request.selection;
    return s.hasPackageName && s.packageNameLength<s.packageName.size()
        ? std::string_view(reinterpret_cast<const char*>(s.packageName.data()),s.packageNameLength)
        : std::string_view{};
}
[[nodiscard]] constexpr std::uint8_t next_revision(std::uint8_t previous) noexcept {
    return previous==255 ? std::uint8_t{1} : static_cast<std::uint8_t>(previous+1U);
}

// Prepares data only. State must revalidate the exact owner/record revision
// under its write lock before committing and publishing the native descriptor.
// Binding/account authentication occurs before constructing Context. No native
// eligibility flags are granted here; only already-published authored routes
// can request this transition. Same-host root package and nonce are retained.
[[nodiscard]] inline Result prepare(const Context& context,const wire::Request& request,
                                    std::span<const Route> routes,Plan& output) noexcept {
    output={};
    if(!static_cast<bool>(context.owner) || context.expectedRecordRevision==0) return Result::invalidOwner;
    const auto& selected=request.selection;
    if(selected.reason!=1 || selected.sourceActivityIndex!=selected.activityIndex
       || selected.activityIndex<0 || package(request).empty()
       || selected.descriptorBitLength==0) return Result::invalidSelection;
    if(!request.hasAccount || !request.hasNonce || !context.current.hasAccount
       || !context.current.hasNonce || request.account!=context.current.account
       || request.nonce!=context.current.nonce || package(request)!=package(context.current)) return Result::identityMismatch;
    if(request.revision!=next_revision(context.current.revision)) return Result::staleRevision;
    if(!kLaunchesEnabled)return Result::unsupportedRoute;
    const Route* match=nullptr;
    for(const auto& route:routes) {
        if(route.scenario==context.scenario && route.rootPackage==package(request)
           && route.activity==selected.activityIndex) {
            if(match) return Result::unsupportedRoute;
            match=&route;
        }
    }
    if(!match) return Result::unsupportedRoute;
    if(context.published.count>context.published.entries.size()) return Result::placementUnavailable;
    bool published=false;
    for(std::size_t i=0;i<context.published.count;++i) {
        const auto& placement=context.published.entries[i];
        if(placement.registry==match->registry && placement.slot==match->slot
           && placement.bubble==match->bubble) published=true;
    }
    if(!published) return Result::placementUnavailable;
    output={context.owner,context.expectedRecordRevision,request,*match};
    return Result::accepted;
}
} // namespace sunrise::server::runtime::activity::adventure_start
