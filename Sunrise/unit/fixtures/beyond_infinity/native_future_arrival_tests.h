#pragma once
#include "../../../src/state/activity/beyond_infinity/controller.h"

namespace beyond_native_future_arrival_fixture {
template<class Check> void run(Check check,const sunrise::state::activity::coo::script::Views& shipped) {
    namespace bi=sunrise::state::activity::beyond_infinity;
    namespace coo=sunrise::state::activity::coo;
    const auto asset=[&](std::string_view name) {
        for(const auto& cap:bi::kCapabilities) { if(cap.id==name) { return cap.spec.asset; } }
        check(false,"Future arrival capability exists");return coo::Asset{};
    };
    const auto point=[&](coo::Asset a) {
        for(const auto& v:bi::kVolumes) {
            if(v.asset!=a) { continue; }
            for(unsigned x=1;x<20;++x) { for(unsigned y=1;y<20;++y) {
                const bi::Point p{v.min.x+(v.max.x-v.min.x)*float(x)/20,
                    v.min.y+(v.max.y-v.min.y)*float(y)/20,(v.min.z+v.max.z)/2};
                if(bi::contains(v,p)) { return p; }
            } }
        }
        check(false,"Future native volume has an interior point");return bi::Point{};
    };
    const auto arrival=point(asset("future.tv_player_enters_space"));
    const auto scene=asset("future.scene_future_echo");
    for(const bool runAhead:{false,true}) {
        auto views=shipped;views.phases=shipped.phases.subspan(5,2);
        bi::Controller controller;const std::uint64_t owner=runAhead?807:806;
        check(controller.select(views,owner),"select shipped second Forest and Future phases");
        if(views.observationStart) { controller.position(owner,point(views.observationStart->asset)); }
        std::uint64_t now=1000;bool allowWarning{};unsigned warningSubmissions{};
        const auto tick=[&]() {
            const auto frame=controller.update(owner,now,true);now+=1000;
            check(frame.transitRoute==0,"Future load zone never publishes a host teleport request");
            if(frame.activeRow!=coo::kNoDialogue && (frame.activeRow!=38 || allowWarning)) {
                check(controller.submitted(owner,bi::kBank,frame.activeRow,frame.generations[frame.activeRow],now),
                    "native global speech submission accepted during Future approach");
                if(frame.activeRow==38) { ++warningSubmissions; }
            }
            for(std::size_t i=0;i<std::size(bi::kAssets);++i) {
                const auto a=bi::kAssets[i].asset;const auto& state=frame.native[i];
                if(a.type==4 && state.managed && !state.prepared) {
                    static_cast<void>(controller.prepared({owner,state.generation},a));
                }
            }
            return frame;
        };
        static_cast<void>(tick());
        controller.position(owner,point(asset("forest.tv_end_2")));
        bool warningOffered{};
        for(unsigned i=0;i<90 && !warningOffered;++i) { warningOffered=tick().activeRow==38; }
        check(warningOffered,"second exit requests its approach warning");
        // This exact point used to satisfy transit3.contact and preempt the
        // client's already-running normal_z_leg transition in the live log.
        controller.position(owner,{-226.F,1072.F,-65.F});
        for(unsigned i=0;i<5;++i) {
            const auto frame=tick();
            check(frame.section==0 && !frame.native[bi::asset_index(scene)].desired,
                "entering the loading corridor cannot skip the native portal barrier");
        }
        if(runAhead) {
            controller.position(owner,arrival);controller.position(owner,{0,0,0});
            check(tick().section==0,"real early arrival still waits for approach speech submission");
        }
        allowWarning=true;
        for(unsigned i=0;i<65;++i) { static_cast<void>(tick()); }
        if(!runAhead) {
            check(controller.frame().section==0,"finished warning plus loading-zone presence cannot report arrival");
            controller.position(owner+1,arrival);
            check(tick().section==0,"foreign-run arrival does not advance Future");
            controller.position(owner,arrival);controller.position(owner,{0,0,0});
            for(unsigned i=0;i<15;++i) { static_cast<void>(tick()); }
        }
        check(controller.frame().section==1 && controller.frame().native[bi::asset_index(scene)].active,
            "actual native arrival starts Future even after the player leaves the arrival volume");
        check(warningSubmissions==1,"Future approach warning submits once");
        controller.reset();check(controller.select(views,owner),"Future arrival supports reset");
        check(controller.seen().none() && controller.frame().transitRoute==0,
            "reset clears prior destination crossings and cannot resume a forced teleport");
    }
}
}
