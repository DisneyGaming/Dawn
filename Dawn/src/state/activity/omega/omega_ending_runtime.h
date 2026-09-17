#pragma once
#include "omega_ending_transit.h"
#include "omega_mission_runtime.h"
#include "omega_mission_presentation.h"

namespace dawn::state::activity::omega::ending::runtime {
struct View {
    mission::Token token{};
    std::uint32_t revision{}, owner{UINT32_MAX}, component{UINT32_MAX};
    bool requested{}, transition{}, arrived{}, play{}, started{}, finished{}, failed{};
};
inline std::mutex mutex;
inline View view;
inline Transit transit;
inline std::uint64_t requestedAt{},startedAt{},nextRefresh{};
// No native calls while this lock is held. Mission receipts use their own lock
// only after the ending lock is released.
inline View update(std::uint64_t run,std::uint64_t now) noexcept {
    const auto s=mission::runtime::snapshot(run);
    const auto speech=mission_presentation::snapshot(run);
    const bool qualifies=s.phase==mission::Phase::ending && s.command.action==mission::Action::ending;
    bool claimed=s.command.claimed;
    if(qualifies && !claimed) claimed=mission::runtime::receipt([&](auto& state){return state.claim(s.command.token,mission::Action::ending);});
    const std::lock_guard lock(mutex);
    if(view.token.boss.run!=run) {view={};transit={};requestedAt=startedAt=nextRefresh=0;}
    if(qualifies && claimed && !view.requested) {
        view.token=s.command.token;view.requested=true;view.revision=s.generation;requestedAt=now;
    }
    if(!view.requested || view.token!=s.command.token || view.finished || view.failed) return view;
    if(now>=requestedAt && !view.started && now-requestedAt>120000) view.failed=true;
    if(view.started && now>=startedAt && now-startedAt>300000) view.failed=true;
    if(!view.failed && speech.endingSubmitted && now>=speech.submittedAt && now-speech.submittedAt>=9102)
        view.transition=true;
    return view;
}
inline View snapshot(std::uint64_t run) noexcept {
    const std::lock_guard lock(mutex);return view.token.boss.run==run?view:View{};
}
inline bool selected(std::uint64_t run,ActivityInstanceKey activity) noexcept {
    const std::lock_guard lock(mutex);
    return view.token.boss.run==run && view.transition && transit.bound() && transit.scope().activity==activity;
}
inline Projection project(std::uint64_t run,ActivityInstanceKey activity,std::uint64_t member,
                          bool exactOmega,bool hasReceipt,membership::TeleportState local,std::int32_t region) noexcept {
    const std::lock_guard lock(mutex);
    if(view.token.boss.run!=run || !view.transition || (view.failed && !transit.bound())) return {};
    auto result=transit.project({view.token,activity,member},exactOmega,hasReceipt,local,region);
    if(result.arrived) view.arrived=true;
    return result;
}
inline bool refresh(std::uint64_t run,ActivityInstanceKey activity,std::uint64_t now) noexcept {
    update(run,now);
    const std::lock_guard lock(mutex);
    if(view.token.boss.run!=run || !view.transition || view.failed || now<nextRefresh
        || (transit.bound() && (transit.scope().activity!=activity || !transit.pending()))) return false;
    nextRefresh=now+500;return true;
}
// Exact native lookup validates the full registered resource owner before this call.
inline bool resource(const mission::Token& token,std::uint32_t owner) noexcept {
    const std::lock_guard lock(mutex);
    if(token!=view.token || !view.arrived || view.failed || view.finished || !owner || owner==UINT32_MAX) return false;
    if(view.owner!=UINT32_MAX && view.owner!=owner) return false;
    view.owner=owner;view.play=true;return true;
}
inline bool native(const mission::Token& token,std::uint32_t owner,std::uint32_t component,
                   std::uint32_t revision,bool registered,bool active,std::uint64_t now) noexcept {
    bool finish{};
    {
        const std::lock_guard lock(mutex);
        if(token!=view.token || !view.play || view.failed || view.finished || !registered
            || owner!=view.owner || revision!=view.revision || !component || component==UINT32_MAX) return false;
        if(view.component!=UINT32_MAX && view.component!=component) return false;
        if(!view.started) {
            if(!active) return false;
            view.component=component;view.started=true;startedAt=now;
        } else {if(active)return false;view.finished=true;finish=true;}
    }
    return mission::runtime::receipt([&](auto& state){return state.ending(token,finish);});
}
} // namespace dawn::state::activity::omega::ending::runtime
