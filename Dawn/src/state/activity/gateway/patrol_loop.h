#pragma once
#include "traversal_catalog.h"
#include "catalog.h"
#include <array>
#include <cmath>

namespace dawn::state::activity::gateway::patrol {
// Authored spawn-rule positions and firing-area bounds from 80F470D9. The
// firing-area assignment is one-way. Turn before entering its terminal area
// so native tactical AI continues requesting locomotion instead of settling.
struct Lane { Point start,minimum,maximum; };
inline constexpr std::array<Lane,11> kLanes{{
    {{-625.259827F,-57.531593F,18.999914F},{-600.110779F,-104.468445F,17.499914F},{-589.759949F,-95.174431F,20.499914F}},
    {{-566.059204F,82.314919F,23.249937F},{-524.264343F,52.228790F,21.749937F},{-516.265991F,62.968052F,24.749937F}},
    {{-663.259827F,4.968498F,-7.000210F},{-638.110779F,-41.968330F,-8.500210F},{-627.759949F,-32.674316F,-5.500210F}},
    {{-603.332031F,82.689392F,1.499905F},{-562.037842F,51.815777F,-0.000095F},{-553.914795F,62.539131F,2.999905F}},
    {{-483.551880F,159.693680F,23.249937F},{-433.674438F,154.307480F,21.749937F},{-429.354492F,164.431122F,24.749937F}},
    {{-529.526794F,370.131012F,69.000168F},{-479.576508F,363.869537F,67.500168F},{-475.429382F,373.927490F,70.500168F}},
    {{-279.068481F,370.256531F,81.000214F},{-229.279099F,365.747253F,79.500214F},{-224.787613F,375.933502F,82.500214F}},
    {{-279.852295F,126.872429F,25.499956F},{-230.313278F,116.911461F,23.999956F},{-225.769043F,127.116425F,26.999956F}},
    {{-53.999981F,368.500000F,135.000015F},{-27.431839F,324.042084F,133.500015F},{-17.250992F,333.608185F,136.500015F}},
    {{-509.552917F,223.193771F,29.499985F},{-459.675507F,217.807571F,27.999985F},{-455.355560F,227.931213F,30.999985F}},
    {{-92.500015F,207.500000F,112.750000F},{-46.735088F,215.901505F,111.250000F},{-39.863033F,226.646179F,114.250000F}}
}};
inline constexpr std::size_t kActors=44;
[[nodiscard]] constexpr std::size_t index(std::uint32_t registry,std::uint16_t source) noexcept {
    if(registry!=kTraversalRegistry || source<16 || source>202) return kActors;
    const auto relative=source-16,member=relative%18;
    return member<=6 && member%2==0 ? (relative/18)*4+member/2 : kActors;
}
[[nodiscard]] inline bool finite(Point p) noexcept {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}
[[nodiscard]] inline Point center(const Lane& lane) noexcept {
    return {(lane.minimum.x+lane.maximum.x)*.5F,(lane.minimum.y+lane.maximum.y)*.5F,
        (lane.minimum.z+lane.maximum.z)*.5F};
}
struct Target { Point position{};bool valid{},turned{};std::uint64_t turns{}; };
class Loop final {
public:
    // The caller supplies a currently admitted, still-owned living actor. A
    // different run or salted identity starts a fresh route; no address survives.
    [[nodiscard]] Target update(const EnemyReceipt& owner,Point actual,Point nativeGoal) noexcept {
        const auto i=index(owner.registry,owner.source);
        if(i==kActors || !owner.valid() || !finite(actual) || !finite(nativeGoal)) return {};
        const auto& lane=kLanes[i/4];const auto end=center(lane);
        const float dx=end.x-lane.start.x,dy=end.y-lane.start.y;
        const float length=std::sqrt(dx*dx+dy*dy),ux=dx/length,uy=dy/length;
        const float along=(actual.x-lane.start.x)*ux+(actual.y-lane.start.y)*uy;
        const float across=(actual.x-lane.start.x)*uy-(actual.y-lane.start.y)*ux;
        if(std::abs(across)>12.F || std::abs(actual.z-lane.start.z)>5.F
            || along < -10.F || along > length+10.F) return {};
        auto& state=actors_[i];
        if(state.owner!=owner) {
            // Preserve each actor's native formation destination. Do not turn an
            // unrelated avoidance/attack destination into a patrol route.
            if(nativeGoal.x<lane.minimum.x-1.F || nativeGoal.x>lane.maximum.x+1.F
                || nativeGoal.y<lane.minimum.y-1.F || nativeGoal.y>lane.maximum.y+1.F
                || nativeGoal.z<lane.minimum.z-1.F || nativeGoal.z>lane.maximum.z+1.F) return {};
            state={};state.owner=owner;
            state.home={nativeGoal.x-dx+2.F*ux,nativeGoal.y-dy+2.F*uy,nativeGoal.z};
            state.away={nativeGoal.x-6.F*ux,nativeGoal.y-6.F*uy,nativeGoal.z};
            // Also recover a marcher first observed at the far end.
            state.returning=along>=length-8.F;
        }
        const auto target=state.returning?state.home:state.away;
        const float x=actual.x-target.x,y=actual.y-target.y;
        bool turned{};
        if(x*x+y*y<=16.F && std::abs(actual.z-target.z)<=3.F) {
            state.returning=!state.returning;++state.turns;turned=true;
        }
        return {state.returning?state.home:state.away,true,turned,state.turns};
    }
    void reset() noexcept { actors_={}; }
private:
    struct Actor { EnemyReceipt owner{};Point home{},away{};bool returning{};std::uint64_t turns{}; };
    std::array<Actor,kActors> actors_{};
};
}
