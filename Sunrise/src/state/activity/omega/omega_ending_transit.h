#pragma once
#include "omega_mission_state.h"
#include "../membership/definition.h"
#include "../lifecycle_generation.h"

namespace sunrise::state::activity::omega::ending {
inline constexpr std::int32_t kRegion=121;
inline constexpr std::uint32_t kSpawn=0xAB06CC27, kRegistry=0x3A6CE17A;
struct Scope {
    mission::Token token{};
    ActivityInstanceKey activity{};
    std::uint64_t member{};
    bool operator==(const Scope&) const = default;
};
struct Projection { membership::TeleportState host{}; bool present{}, arrived{}; };
// Reconstructed host policy for original C72CE0. The native membership
// transition token belongs to a different protocol and never enters this helper.
class Transit final {
public:
    Projection project(const Scope& scope,bool exactOmega,bool hasReceipt,
                       membership::TeleportState local,std::int32_t region) noexcept {
        if(!exactOmega || !scope.activity || !scope.member || !scope.token.epoch
            || !scope.token.boss.run || !scope.token.boss.generation) return {};
        if(bound_ && scope!=scope_) return {};
        if(!bound_) {
            if(!hasReceipt || local.state!=0) return {};
            scope_=scope;bound_=true;
            auto token=static_cast<std::uint8_t>(local.token+1U); if(!token) token=1;
            host_={1,token,kRegion,kSpawn};
        }
        bool arrived=false;
        const bool matches=hasReceipt && local.token==host_.token
            && local.sliceSetIndex==kRegion && local.sliceSetHash==kSpawn;
        if(matches && host_.state==1 && local.state==3 && region==kRegion) {
            host_.state=3;arrived=true;
        } else if(matches && host_.state==3 && local.state==0) host_.state=0;
        return {host_,true,arrived};
    }
    bool pending() const noexcept {return bound_ && host_.state!=0;}
    bool bound() const noexcept {return bound_;}
    const Scope& scope() const noexcept {return scope_;}
private:
    Scope scope_{}; membership::TeleportState host_{};bool bound_{};
};
} // namespace sunrise::state::activity::omega::ending
