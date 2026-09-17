#pragma once
#include "frame.h"
#include "../membership/definition.h"
#include "../lifecycle_generation.h"
#include "../omega_ending_transit_rules.h"

namespace dawn::state::activity::beyond_infinity::transit {
// The wire contract is the same verified native C72CE0 contract used by Omega.
namespace native=omega_ending_transit;
using Target=native::Target;
using Observation=native::Observation;
using Authority=native::Authority;
using native::host_synchronization_ready;
// Native map point tables 80F51375,80F4D101,80EAD674,80F50039.
// These route pairings reconstruct the authored geometry; semantic spawn-set
// names and the original mission teleport command tables remain unresolved.
constexpr Target destination(std::uint8_t route) noexcept {
    switch(route) {
    case 1:return {144,0x89729CCEU}; // Past opening, (576,685,9).
    case 2:return {64,0xE323B000U};  // Forest A west return, (-843,1073,-68).
    case 3:return {32,0x2EA8FB98U};  // Future platform, (187,750,312).
    case 4:return {32,0xB8B85CE7U};  // Future return corridor, (-40,1412,-2).
    case 5:return {120,0x5726AC0AU}; // Present Mercury gate, (-546,1972,184).
    default:return {};
    }
}
inline bool active_source(const Frame& frame,std::uint32_t registry,std::uint8_t type,std::uint16_t slot) noexcept {
    const auto* binding=find(registry,type,slot);if(!binding) { return false; }
    const auto& state=frame.native[asset_index(binding->asset)];
    return state.managed && state.desired && state.prepared && state.active && state.generation;
}
// Groups retain their native bubble ownership. Requiring an unstreamed remote
// receiving source would deadlock travel before the client can load that bubble.
inline bool presentation_ready(const Frame& frame,std::uint8_t route,std::int32_t currentRegion) noexcept {
    if(!frame.enabled) { return false; }
    switch(route) {
    case 1:return currentRegion==64 || (currentRegion==144 && active_source(frame,0xC7FB7155U,4,0));
    case 2:return currentRegion==144 && active_source(frame,0xC7FB7155U,4,1)
        && active_source(frame,0xC7FB7155U,4,2) && active_source(frame,0xC7FB7155U,23,3);
    case 3:return currentRegion==64 || currentRegion==32;
    case 4:return currentRegion==32 && active_source(frame,0x0FF26BCCU,4,4)
        && active_source(frame,0x0FF26BCCU,4,5) && active_source(frame,0x0FF26BCCU,23,1)
        && active_source(frame,0x0FF26BCCU,23,2);
    case 5:return currentRegion==32 || currentRegion==120;
    default:return false;
    }
}
struct Scope {
    coo::Generation owner{};
    ActivityInstanceKey activity{};
    std::uint64_t member{};
    std::uint8_t route{};
    bool valid() const noexcept { return owner.valid() && bool(activity)
        && member && member!=UINT64_MAX && destination(route).valid(); }
    bool operator==(const Scope&) const = default;
};
class Transaction final {
public:
    bool begin(Scope scope,Observation observation) noexcept {
        if(!scope.valid()) { return false; }
        if(bound_) { return scope==scope_; }
        if(observation.local.state!=0 || (!observation.hasTeleport && observation.local!=native::Teleport{})) { return false; }
        scope_=scope;bound_=true;
        auto token=static_cast<std::uint8_t>(observation.local.token+1U);if(!token) { token=1; }
        const auto target=destination(scope.route);host_={1,token,target.region,target.spawnSet};return true;
    }
    bool observe(Scope scope,Observation observation) noexcept {
        if(!bound_ || scope!=scope_ || !scope.valid() || !observation.hasTeleport
            || observation.local.token!=host_.token || observation.local.sliceSetIndex!=host_.sliceSetIndex
            || observation.local.sliceSetHash!=host_.sliceSetHash) { return false; }
        if(host_.state==1 && observation.local.state==3 && observation.hasRegion && observation.currentRegion==host_.sliceSetIndex) {
            host_.state=3;arrived_=true;return true;
        }
        if(host_.state==3 && observation.local.state==0) { host_.state=0; }
        return false;
    }
    Authority authority(Scope scope) const noexcept {
        return bound_ && scope==scope_ && scope.valid()?Authority{host_,true,arrived_,host_.state==0}:Authority{};
    }
    bool bound() const noexcept { return bound_; }
    bool pending() const noexcept { return bound_ && host_.state!=0; }
    const Scope& scope() const noexcept { return scope_; }
private:
    Scope scope_{};native::Teleport host_{};bool bound_{},arrived_{};
};
}
