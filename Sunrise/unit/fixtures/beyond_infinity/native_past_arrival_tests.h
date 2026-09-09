#pragma once
#include "../../../src/state/activity/beyond_infinity/controller.h"
#include "../../../src/state/activity/beyond_infinity/transit_contacts.h"

namespace beyond_native_past_arrival_fixture {
template<class Check> void run(Check check,const sunrise::state::activity::coo::script::Views& shipped) {
    namespace bi=sunrise::state::activity::beyond_infinity;
    namespace coo=sunrise::state::activity::coo;
    const auto capability=[&](std::string_view name)->const coo::script::Capability& {
        for(const auto& c:bi::kCapabilities) { if(c.id==name) { return c; } }
        check(false,"native Past replay capability exists");return bi::kCapabilities[0];
    };
    const auto occupied=capability("past.past_quarantine_volume.occupied").spec;
    check(occupied.argument==1 && occupied.wait==coo::Wait::observed,"Past arrival requires current occupancy");
    auto views=shipped;views.phases=shipped.phases.subspan(3,2);
    bi::Controller controller;constexpr std::uint64_t run=801;
    check(controller.select(views,run),"replay shipped Forest and Past phases");
    const auto enter=[&](coo::Asset asset) {
        for(const auto& volume:bi::kVolumes) {
            if(volume.asset!=asset) { continue; }
            for(unsigned x=1;x<20;++x) { for(unsigned y=1;y<20;++y) {
                const bi::Point p{volume.min.x+(volume.max.x-volume.min.x)*float(x)/20,
                    volume.min.y+(volume.max.y-volume.min.y)*float(y)/20,(volume.min.z+volume.max.z)/2};
                if(bi::contains(volume,p)) { controller.position(run,p);return; }
            } }
        }
        check(false,"native Past replay volume has an interior point");
    };
    enter(views.observationStart->asset);std::uint64_t now=1000;
    controller.update(run,now,true);
    // A previous visit cannot satisfy the newly active current-occupancy wait.
    controller.position(run,{740.F,711.F,7.7F});controller.position(run,{0,0,0});
    bool held{};
    const auto tick=[&](bool publishSpeech) {
        auto frame=controller.update(run,now,true);now+=1000;
        if(publishSpeech && frame.activeRow!=coo::kNoDialogue) {
            static_cast<void>(controller.submitted(run,bi::kBank,frame.activeRow,frame.generations[frame.activeRow],now));
        }
        for(std::size_t i=0;i<std::size(bi::kAssets);++i) {
            const auto& state=frame.native[i];const auto asset=bi::kAssets[i].asset;
            if(asset.type==4 && state.managed && !state.prepared) { static_cast<void>(controller.prepared({run,state.generation},asset)); }
        }
        check(frame.transitRoute==0,"first native portal does not request a reconstructed host teleport");
        check((frame.transitContact&2U)==0,"native Past replay never touches the reconstructed first portal sphere");
        return frame;
    };
    for(unsigned i=0;i<100 && !held;++i) {
        const auto frame=tick(true);check(frame.section==0,"Past cannot begin from an old occupancy latch");
        const auto* graph=controller.graph();
        for(const auto& command:graph->commands) {
            if(controller.step_state(command.step).phase!=coo::StepPhase::active) { continue; }
            const auto& spec=graph->definition.steps[command.step].commands[command.command];
            check(!(spec.asset==bi::kModule && (spec.argument==21 || spec.argument==31)),"first Forest has no mandatory synthetic contact or transport command");
            if(spec.asset==occupied.asset && spec.argument==1) { held=true;continue; }
            if(spec.asset.type==60) { enter(spec.asset); }
        }
    }
    check(held,"first Forest waits at actual native Past arrival");
    controller.position(run,{0,0,0});
    const auto scene=capability("past.scene_past_echo").spec.asset;
    for(unsigned i=0;i<30;++i) {
        const auto frame=tick(true);
        check(frame.section==0 && frame.generations[32]==0
            && !frame.native[bi::asset_index(scene)].desired,"stale destination visit cannot request Past speech or Scene");
    }
    controller.position(run+1,{740.F,711.F,7.7F});
    check(tick(true).section==0,"foreign-run native position cannot report arrival");
    // This is the actual native arrival position from the failed playthrough.
    // No call to Controller::transit and no native membership tuple is forged.
    controller.position(run,{740.F,711.F,7.7F});
    bool speech{},sceneRequested{};
    for(unsigned i=0;i<40 && !(speech && sceneRequested);++i) {
        const auto frame=tick(false);
        speech|=frame.activeRow==32;
        sceneRequested|=frame.native[bi::asset_index(scene)].desired && frame.native[bi::asset_index(scene)].active;
    }
    check(controller.frame().section==1 && speech && sceneRequested,"actual native Past arrival starts row32 and the authored Scene without first portal contact");
}
}
