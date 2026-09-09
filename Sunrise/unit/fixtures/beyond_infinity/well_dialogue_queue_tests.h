#pragma once
#include "../../../src/state/activity/beyond_infinity/controller.h"
#include <algorithm>

namespace beyond_well_dialogue_queue_fixture {
template<class Check> void run(Check check,const sunrise::state::activity::coo::script::Views& shipped) {
    namespace bi=sunrise::state::activity::beyond_infinity;
    namespace coo=sunrise::state::activity::coo;
    const auto asset=[&](std::string_view id) {
        for(const auto& c:bi::kCapabilities) { if(c.id==id) { return c.spec.asset; } }
        check(false,"Well queue capability exists");return coo::Asset{};
    };
    auto views=shipped;views.phases=shipped.phases.subspan(1,3);
    bi::Controller c;constexpr std::uint64_t run=903;
    check(c.select(views,run),"Well queue selects shipped phases");
    const auto enter=[&](coo::Asset a) {
        for(const auto& v:bi::kVolumes) { if(v.asset!=a) { continue; }
            for(unsigned x=1;x<20;++x) for(unsigned y=1;y<20;++y) {
                const bi::Point p{v.min.x+(v.max.x-v.min.x)*float(x)/20,
                    v.min.y+(v.max.y-v.min.y)*float(y)/20,(v.min.z+v.max.z)/2};
                if(bi::contains(v,p)) { c.position(run,p);return; }
            }
        }
        check(false,"Well queue crossing has an interior point");
    };
    enter(views.observationStart->asset);
    std::uint64_t now=1000;bool destroyed{};
    // Cross each physical trigger while the first scene's audio submission is
    // deliberately withheld. Animation must not depend on that acknowledgement.
    for(unsigned tick=0;tick<300;++tick,now+=10) {
        const auto f=c.update(run,now,true);
        for(std::size_t i=0;i<std::size(bi::kAssets);++i) {
            const auto& s=f.native[i];const auto a=bi::kAssets[i].asset;
            if(a.type==4 && s.managed && !s.prepared) { static_cast<void>(c.prepared({run,s.generation},a)); }
        }
        for(const auto& scene:bi::kScenes) {
            const auto& s=f.native[bi::asset_index(scene.asset)];if(!s.active) { continue; }
            static_cast<void>(c.scene({{run,s.generation},scene.asset,10,20,30,40},false));
        }
        // Search is ordinary pre-scene speech; allow its acknowledgement only.
        if(f.activeRow==4) { static_cast<void>(c.submitted(run,bi::kBank,4,f.generations[4],now)); }
        const auto* graph=c.graph();
        for(const auto& b:graph->commands) {
            if(c.step_state(b.step).phase!=coo::StepPhase::active) { continue; }
            const auto& spec=graph->definition.steps[b.step].commands[b.command];
            if(spec.asset.type==60) { enter(spec.asset); }
            else if(const auto* condition=views.condition(spec)) {
                for(const auto& node:condition->nodes) { if(node.native.asset.type==60) { enter(node.native.asset); } }
            } else if(spec.asset==bi::kLens && !destroyed) {
                const auto& f2=c.frame();const auto lens=f2.native[bi::asset_index(bi::kLens)];
                const auto plate=f2.native[bi::asset_index(bi::kPlate)];
                if(!lens.active || !plate.active) { continue; }
                enter(asset("well.plate_occupied"));
                const bi::PlateReceipt p{{run,plate.generation},0x200000,4,5,6,7};
                const bi::LensReceipt l{{run,lens.generation},0x100000,1,2,3};
                static_cast<void>(c.bind_plate(p));static_cast<void>(c.lens(l,false));
                const auto revision=c.frame().plateRevision;
                static_cast<void>(c.plate(p,revision,.5F,false));
                static_cast<void>(c.plate(p,revision,1.F,true));destroyed=c.lens(l,true);
            }
        }
    }
    check(destroyed && c.frame().section==1,"Well phase advances without its first voice finishing");
    for(const auto id:{"reflections.scene_echo_first","reflections.scene_split_2char",
        "reflections.scene_echo_intro_two","reflections.scene_echo_intro_four_3char",
        "reflections.scene_echo_intro_six","reflections.scene_echo_intro_six_right"}) {
        const auto a=asset(id);
        check(c.frame().native[bi::asset_index(a)].active,"every crossed Well scene starts with first voice unacknowledged");
        check(c.frame().sceneRequests[bi::scene_index(a)].silent,"Well native speech detached from its animation");
    }
    const auto reveal=asset("reveal.scene_if_reveal");
    check(!c.frame().native[bi::asset_index(reveal)].active && c.frame().forestPass==0,
        "final native conversation cannot overlap queued Well speech or release Forest");
    check(c.frame().native[bi::asset_index(asset("reveal.scene_runup_1"))].active,
        "Precipice run-up animation follows its volume before the voice backlog ends");
    // Acknowledge each offered clip and advance through its real bank duration.
    // Expected order is independent of native callback order, repeated crossings,
    // and the player's ability to outrun a preceding spoken line.
    constexpr std::uint8_t expected[]{6,12,13,14,9,10,15,17,19};
    for(const auto row:expected) {
        auto f=c.update(run,now,true);
        check(f.activeRow==row,"Well voice queue retains story order without duplicates");
        check(c.submitted(run,bi::kBank,row,f.generations[row],now),"queued Well clip has an authentic submission");
        enter(asset("reflections.tv_echo_six"));enter(asset("reflections.tv_echo_two"));
        now+=bi::kDialogue[row].durationMs+249;
        f=c.update(run,now,true);
        check(f.activeRow==coo::kNoDialogue,"next voice cannot overlap the submitted clip or its spacing");
        now+=1;
    }
    for(unsigned tick=0;tick<5;++tick) { static_cast<void>(c.update(run,now++,true)); }
    check(c.frame().native[bi::asset_index(reveal)].active,"Where is the real Osiris scene follows the drained queue");
    check(c.frame().activeRow==coo::kNoDialogue && c.frame().forestPass==0,"no duplicate Well speech and Behold remains required");
    c.reset();check(c.select(views,run),"Well dialogue reset selects a fresh owner");
    check(c.frame().activeRow==coo::kNoDialogue && c.seen().none(),"reset discards old voice requests and crossings");
}
}
