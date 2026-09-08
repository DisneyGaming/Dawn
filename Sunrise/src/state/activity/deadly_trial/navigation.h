#pragma once
#include "profile.h"
namespace sunrise::state::activity::deadly_trial::navigation {
struct Goal { coo::MarkerTarget target{};std::uint32_t bubble{};Point position{}; };
inline Goal goal(std::uint32_t event) noexcept {
    Goal result{};
    for(const auto& marker:kNativeMarkers) { if(marker.event==event) { result.target=marker.target;break; } }
    switch(result.target.asset.definition) {
    case 0x80B2E62BU: case 0x80B2E6E9U: result.position={742.496094F,198.489990F,87.452415F};break;
    case 0x80B2E66BU: result.position={945.683655F,435.263672F,152.227814F};break;
    case 0x80B2E68DU: result.position={1002.685974F,409.087189F,153.471344F};break;
    // EC47's authored altitude is on the roof. Use the entrance directive's
    // center height, confirmed by the user at the walkway during live testing.
    case 0x80B2EC47U: result.bubble=1;result.position={1374.614624F,544.469482F,198.662613F};break;
    case 0x80B2EC53U: result.bubble=1;result.position={1412.822388F,595.623779F,172.705978F};break;
    case 0x80B2EC62U: result.bubble=1;result.position={1411.089966F,598.714478F,173.280655F};break;
    default:return {};
    }return result;
}
}
