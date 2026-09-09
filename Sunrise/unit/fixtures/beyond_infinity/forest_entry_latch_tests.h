#pragma once
#include "../../../src/state/activity/beyond_infinity/controller.h"
#include <algorithm>
namespace beyond_forest_entry_latch_fixture {
template<class Check> void run(Check check,const sunrise::state::activity::coo::script::Views& shipped) {
    namespace bi=sunrise::state::activity::beyond_infinity;
    namespace coo=sunrise::state::activity::coo;
    const auto asset=[&](std::string_view id) {
        for(const auto& c:bi::kCapabilities) { if(c.id==id) { return c.spec.asset; } }
        check(false,"entry-latch replay capability exists");return coo::Asset{};
    };
    const auto entry=asset("reveal.tv_if_entered"),futureEnd=asset("forest.tv_end_2");
    const auto reveal=asset("reveal.scene_if_reveal"),returnEntry=asset("forest.tv_begin_past");
    const auto volumeIndex=[&](coo::Asset a) {
        for(std::size_t i=0;i<std::size(bi::kVolumes);++i) { if(bi::kVolumes[i].asset==a) { return i; } }
        check(false,"entry-latch replay volume exists");return std::size_t{};
    };
    // Replay the shipped reveal, first Forest, Past and second Forest phases.
    auto views=shipped;views.phases=shipped.phases.subspan(2,4);
    bi::Controller controller;constexpr std::uint64_t run=802;
    check(controller.select(views,run),"entry-latch replay selects an owned run");
    const auto enter=[&](coo::Asset a) {
        const auto& v=bi::kVolumes[volumeIndex(a)];
        for(unsigned x=1;x<20;++x) { for(unsigned y=1;y<20;++y) {
            const bi::Point p{v.min.x+(v.max.x-v.min.x)*float(x)/20,
                v.min.y+(v.max.y-v.min.y)*float(y)/20,(v.min.z+v.max.z)/2};
            if(bi::contains(v,p)) { controller.position(run,p);return; }
        } }
        check(false,"entry-latch volume has an interior point");
    };
    enter(views.observationStart->asset);
    std::uint64_t now=1000,beholdAt{};
    bool jumped{},speechHeld{},beholdDone{},tutorial{},secondSeeded{},secondReset{};
    for(unsigned iteration=0;iteration<700 && !secondReset;++iteration) {
        const auto frame=controller.update(run,now,true);now+=1000;
        if(frame.forestPass==1 && frame.section==1) {
            check(beholdDone,"first Forest still waits for native Behold completion");
            check(!frame.native[bi::asset_index(reveal)].active,"completed reveal is retired before leaving the first Forest");
            check(controller.seen()[volumeIndex(entry)],"early physical jump survives the first Forest visit command");
        }
        if(frame.forestPass==2) {
            check(secondSeeded,"second-pass replay recorded an earlier exit observation");
            check(!controller.seen()[volumeIndex(futureEnd)],"starting the second visit clears prior exit latches");
            check(!frame.native[bi::asset_index(reveal)].active,"returning to the Forest cannot replay the retained reveal inputs");
            secondReset=true;break;
        }
        if(frame.activeRow!=coo::kNoDialogue) {
            if(frame.activeRow==30) { check(beholdDone && jumped,"Daemon line follows actual jump and completed Behold");tutorial=true; }
            static_cast<void>(controller.submitted(run,bi::kBank,frame.activeRow,frame.generations[frame.activeRow],now));
        }
        for(std::size_t i=0;i<std::size(bi::kAssets);++i) {
            const auto& state=frame.native[i];const auto a=bi::kAssets[i].asset;
            if(a.type==4 && state.managed && !state.prepared) { static_cast<void>(controller.prepared({run,state.generation},a)); }
        }
        for(const auto& scene:bi::kScenes) {
            const auto& state=frame.native[bi::asset_index(scene.asset)];if(!state.active) { continue; }
            const bi::SceneReceipt receipt{{run,state.generation},scene.asset,10,20,30,40};
            static_cast<void>(controller.scene(receipt,false));static_cast<void>(controller.scene(receipt,true));
            const auto sceneIndex=bi::scene_index(scene.asset);
            if(frame.sceneRequests[sceneIndex].silent) { continue; }
            for(const auto& speech:scene.speech) {
                if(scene.asset==reveal && speech.row==23) {
                    const auto events=frame.sceneRequests[sceneIndex].inputs();
                    if(std::find(events.begin(),events.end(),0xC7ECAA77U)==events.end()) { continue; }
                    static_cast<void>(controller.scene_speech(receipt,23,1));
                    if(!beholdAt) {
                        beholdAt=now;enter(entry);controller.position(run,{0,0,0});jumped=true;
                        check(controller.seen()[volumeIndex(entry)],"native jump observed while Behold is running");
                    }
                    if(now<beholdAt+15000) {
                        check(frame.forestPass==0 && frame.generations[30]==0,"early jump cannot release Forest or play Daemon dialogue during Behold");
                        speechHeld=true;continue;
                    }
                    static_cast<void>(controller.scene_speech(receipt,23,2));beholdDone=true;
                } else {
                    static_cast<void>(controller.scene_speech(receipt,speech.row,1));
                    static_cast<void>(controller.scene_speech(receipt,speech.row,2));
                }
            }
            for(const auto& cue:scene.cues) { static_cast<void>(controller.scene_cue(receipt,cue.id,2,1)); }
        }
        if(frame.transitRoute==2) { static_cast<void>(controller.transit(controller.owner(),2)); }
        const auto* graph=controller.graph();
        for(const auto& command:graph->commands) {
            if(controller.step_state(command.step).phase!=coo::StepPhase::active) { continue; }
            const auto& spec=graph->definition.steps[command.step].commands[command.command];
            if(spec.asset==entry) { continue; } // Never cross entry again after the early jump.
            if(spec.asset==returnEntry && frame.section==2) {
                enter(futureEnd);enter(returnEntry);secondSeeded=true;
                check(controller.seen()[volumeIndex(futureEnd)],"prior exit was latched immediately before the second Forest visit");
            } else if(spec.asset.type==60) { enter(spec.asset); }
            else if(spec.asset==bi::kModule && spec.argument==32) { controller.position(run,{748.93F,709.90F,2.93F}); }
            else if(const auto* condition=views.condition(spec)) {
                for(const auto& node:condition->nodes) { if(node.native.asset.type==60) { enter(node.native.asset); } }
            }
        }
    }
    check(jumped && speechHeld && beholdDone && tutorial && secondReset,"early entry, native speech gate, Daemon tutorial and second-visit reset exercised");
    controller.reset();check(controller.select(views,run),"entry replay resets within the same numeric run");
    check(controller.seen().none(),"new owner cannot inherit any old entry latch");
}
}
