#pragma once
#include "frame.h"
#include "../coo/native_damage_floor.h"
#include <cmath>
#include <cstring>
#include <span>
namespace sunrise::state::activity::hijacked::boss_damage {
inline constexpr float floor(std::uint8_t stage) noexcept { return stage==0?2.F/3.F:stage==1?1.F/3.F:0.F; }
inline bool blocked(const BossRequest& request,float fraction) noexcept {
    return request.owner.valid() && request.enemy.valid() && request.stage<3 && std::isfinite(fraction)
        && ((request.stage>0 && request.requested) || (request.stage<2 && fraction<=floor(request.stage)));
}
using coo::native_damage_floor::Packet;
using coo::native_damage_floor::get;
using coo::native_damage_floor::clamp;
}
