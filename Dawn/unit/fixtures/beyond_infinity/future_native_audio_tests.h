#pragma once
#include "../../../src/state/activity/beyond_infinity/controller.h"
#include "../../../src/state/activity/beyond_infinity/shield_authority.h"
#include <algorithm>

namespace beyond_future_native_audio_fixture {
template<class Check> void run(Check check,const dawn::state::activity::coo::script::Views& shipped) {
    namespace bi=dawn::state::activity::beyond_infinity;
    namespace coo=dawn::state::activity::coo;
    const auto spec=[&](std::string_view id) {
        for(const auto& c:bi::kCapabilities) { if(c.id==id) { return c.spec; } }
        check(false,"native Future audio capability exists");return coo::CommandSpec{};
    };
    const auto scene=spec("future.scene_future_echo").asset;
    const auto portal=spec("ambush.lighthouse_teleport_object.on").asset;
    const auto panoptes=static_cast<std::uint8_t>(spec("future.scene_future_echo.event.3A26E9BE.emitted").argument-0x300);
    const auto escape=static_cast<std::uint8_t>(spec("future.scene_future_echo.event.4A0A18EB.emitted").argument-0x300);
    // Exercise the first native vignette while the opening speech is still
    // running. Waiting for speech completion used to hide its arrival motion.
    {
        auto opening=shipped;opening.phases=shipped.phases.subspan(6,1);
        bi::Controller probe;constexpr std::uint64_t owner=809;
        check(probe.select(opening,owner),"select Future opening choreography");
        const auto volume=[&](std::string_view name)->const bi::Volume& {
            const auto a=spec(name).asset;
            for(const auto& v:bi::kVolumes) { if(v.asset==a) { return v; } }
            check(false,"Future choreography volume exists");return bi::kVolumes[0];
        };
        const auto inside=[&](const bi::Volume& v,const bi::Volume* exclude) {
            for(unsigned x=1;x<40;++x) { for(unsigned y=1;y<40;++y) {
                bi::Point p{v.min.x+(v.max.x-v.min.x)*float(x)/40,
                    v.min.y+(v.max.y-v.min.y)*float(y)/40,(v.min.z+v.max.z)/2};
                if(bi::contains(v,p) && (!exclude || !bi::contains(*exclude,p))) { return p; }
            } }
            check(false,"distinct native approach and close-up trigger points exist");return bi::Point{};
        };
        const auto& approach=volume("future.tv_player_approaching_first_echo");
        const auto& near=volume("future.tv_player_near_first_echo");
        probe.position(owner,inside(volume("lighthouse.mercury_m_vod_lighthouse_010_filter"),nullptr));
        std::uint64_t time=1000;bi::SceneReceipt receipt{};
        const auto tickOpening=[&]() {
            auto frame=probe.update(owner,time,true);time+=100;
            for(std::size_t i=0;i<std::size(bi::kAssets);++i) {
                const auto a=bi::kAssets[i].asset;const auto& state=frame.native[i];
                if(a.type==4 && state.managed && !state.prepared) { static_cast<void>(probe.prepared({owner,state.generation},a)); }
            }
            const auto& state=frame.native[bi::asset_index(scene)];
            if(state.active) {
                if(!receipt.owner.run) { receipt={{owner,state.generation},scene,10,20,30,40}; }
                check(state.generation==receipt.owner.value,"arrival and teleport share one native Scene generation");
                static_cast<void>(probe.scene(receipt,false));
            }
            return frame;
        };
        const auto has=[&](std::uint32_t event) {
            const auto inputs=probe.frame().sceneRequests[bi::scene_index(scene)].inputs();
            return std::find(inputs.begin(),inputs.end(),event)!=inputs.end();
        };
        static_cast<void>(tickOpening());
        probe.position(owner,inside(volume("future.tv_player_enters_space"),&approach));
        for(unsigned i=0;i<15;++i) { static_cast<void>(tickOpening()); }
        check(has(0xFA39DB9EU) && !has(0x05BBC301U),"arrival begins Sagira while native approach controls the first teleport");
        static_cast<void>(probe.scene_speech(receipt,39,1));
        const auto approachPoint=inside(approach,&near);
        probe.position(owner+1,approachPoint);
        for(unsigned i=0;i<5;++i) { static_cast<void>(tickOpening()); }
        check(!has(0x05BBC301U),"foreign approach cannot start the reflection vignette");
        probe.position(owner,approachPoint);
        for(unsigned i=0;i<15;++i) { static_cast<void>(tickOpening()); }
        check(has(0x05BBC301U),"first reflection appears and teleports before Sagira39 finishes or the closer trigger is reached");
        probe.position(owner,{0,0,0});
        for(unsigned i=0;i<15;++i) { static_cast<void>(tickOpening()); }
        const auto inputs=probe.frame().sceneRequests[bi::scene_index(scene)].inputs();
        check(std::count(inputs.begin(),inputs.end(),0x05BBC301U)==1,"leaving the approach neither cancels nor replays the first vignette");
        for(const auto& actor:bi::kScenes[bi::scene_index(scene)].cast) {
            if(actor.type==1) { check(probe.frame().native[bi::asset_index(actor)].active,"native Scene retains its reflection sources after approach exit"); }
        }
        probe.position(owner,inside(volume("future.tv_player_near_echo"),nullptr));
        static_cast<void>(probe.scene_speech(receipt,40,1));static_cast<void>(probe.scene_speech(receipt,40,2));
        for(unsigned i=0;i<15;++i) { static_cast<void>(tickOpening()); }
        check(!has(0x43E472AFU),"higher reflection still waits for opening speech completion");
        static_cast<void>(probe.scene_speech(receipt,39,2));
        for(unsigned i=0;i<15;++i) { static_cast<void>(tickOpening()); }
        check(has(0x43E472AFU),"higher native scene remains reachable after both earlier speeches finish");
    }
    auto views=shipped;views.phases=shipped.phases.subspan(6,2);
    bi::Controller controller;constexpr std::uint64_t run=804;
    check(controller.select(views,run),"replay shipped Future phase with native audio ownership");
    bi::Point lastPosition{};
    const auto enter=[&](coo::Asset a) {
        for(const auto& v:bi::kVolumes) {
            if(v.asset!=a) { continue; }
            for(unsigned x=1;x<20;++x) { for(unsigned y=1;y<20;++y) {
                const bi::Point p{v.min.x+(v.max.x-v.min.x)*float(x)/20,
                    v.min.y+(v.max.y-v.min.y)*float(y)/20,(v.min.z+v.max.z)/2};
                if(bi::contains(v,p)) { lastPosition=p;controller.position(run,p);return; }
            } }
        }
        check(false,"native Future replay volume has an interior point");
    };
    if(views.observationStart) { enter(views.observationStart->asset); }std::uint64_t now=1000;
    controller.update(run,now,true);
    const auto corridor=spec("escape.mercury_m_vod_future_060_filter.occupied").asset;
    enter(corridor);const auto corridorPoint=lastPosition;
    enter(spec("future.tv_player_enters_space").asset);
    bool childRequested{},allowPanoptes{},allowEscape{},allowFinalCue{},firstSpeech{},secondSpeech{};
    std::uint32_t sceneGeneration{};
    bool shieldedAmbush{};
    bool allow47{},allow48{};unsigned offers47{},offers48{};std::uint64_t finalVoiceEnd{};
    const auto tick=[&]() {
        const auto frame=controller.update(run,now,true);now+=1000;
        check(frame.transitRoute==0,"native final portals do not request host routes4 or5");
        if(frame.activeRow==47 && allow47) {
            check(controller.submitted(run,bi::kBank,47,frame.generations[47],now),"actual escape speech submission accepted");++offers47;
        }
        if(frame.activeRow==48 && allow48) {
            check(controller.submitted(run,bi::kBank,48,frame.generations[48],now),"actual final speech submission accepted");++offers48;
            finalVoiceEnd=now+views.dialogue.rows[48].durationMs+views.dialogue.rows[48].delayMs+views.dialogue.spacingMs;
        }
        if(!finalVoiceEnd || now-1000<finalVoiceEnd) { check(!frame.finished,"mission cannot finish before actual row48 playback ends"); }
        for(const auto& binding:bi::kAssets) {
            const auto a=binding.asset;
            if(a.registry!=0x0FF26BCCU || a.type!=1 || a.slot<6 || a.slot>14
                || !frame.native[bi::asset_index(a)].active) { continue; }
            for(const auto shield:{bi::shields::kFrontFilter,bi::shields::kBackFilter,bi::shields::kFrontEffect,bi::shields::kBackEffect}) {
                check(frame.native[bi::asset_index(shield)].active,"native shield collections and effects are active before any ambush actor source");
            }
            shieldedAmbush=true;
        }
        for(auto row:{42,43,44,45}) {
            check(frame.generations[row]==0 && frame.activeRow!=row,"native child audio is never republished through the global bank");
        }
        for(std::size_t i=0;i<std::size(bi::kAssets);++i) {
            const auto& state=frame.native[i];const auto a=bi::kAssets[i].asset;
            if(a.type==4 && state.managed && !state.prepared) { static_cast<void>(controller.prepared({run,state.generation},a)); }
        }
        const auto& state=frame.native[bi::asset_index(scene)];
        if(state.active) {
            if(!sceneGeneration) { sceneGeneration=state.generation; }
            check(state.generation==sceneGeneration,"nested native speech does not restart its owning Scene");
            const bi::SceneReceipt receipt{{run,state.generation},scene,10,20,30,40};
            static_cast<void>(controller.scene(receipt,false));static_cast<void>(controller.scene(receipt,true));
            const auto& request=frame.sceneRequests[bi::scene_index(scene)];
            check(!request.silent,"native Future dialogue is retained rather than muted");
            const auto events=request.inputs();
            const auto has=[&](std::uint32_t event) { return std::find(events.begin(),events.end(),event)!=events.end(); };
            if(has(0xFA39DB9EU)) {
                static_cast<void>(controller.scene_speech(receipt,39,1));
                static_cast<void>(controller.scene_speech(receipt,39,2));firstSpeech=true;
            }
            if(has(0x05BBC301U)) {
                static_cast<void>(controller.scene_speech(receipt,40,1));
                static_cast<void>(controller.scene_speech(receipt,40,2));secondSpeech=true;
            }
            if(has(0x43E472AFU)) {
                check(firstSpeech && secondSpeech,"native child starts after the earlier native Future speeches");
                check(std::count(events.begin(),events.end(),0x43E472AFU)==1,"native child start is requested exactly once");
                childRequested=true;
                if(allowPanoptes) { static_cast<void>(controller.scene_cue(receipt,panoptes,2,1)); }
                if(allowEscape) {
                    auto foreign=receipt;++foreign.owner.run;
                    check(!controller.scene_cue(foreign,escape,2,1),"foreign Scene cannot release the escape portal");
                    static_cast<void>(controller.scene_cue(receipt,escape,2,1));
                    if(allowFinalCue) { static_cast<void>(controller.scene_cue(receipt,23,2,1)); }
                }
            }
        }
        const auto* graph=controller.graph();
        for(const auto& command:graph->commands) {
            if(controller.step_state(command.step).phase!=coo::StepPhase::active) { continue; }
            const auto& active=graph->definition.steps[command.step].commands[command.command];
            if(active.asset.type==60 && active.asset.registry==0x15FFBE16U) { enter(active.asset); }
        }
        return frame;
    };
    for(unsigned i=0;i<100 && !childRequested;++i) { static_cast<void>(tick()); }
    check(childRequested,"native Future child graph remains reachable without any duplicate global submission");
    for(unsigned i=0;i<45;++i) {
        const auto frame=tick();
        check(!frame.native[bi::asset_index(portal)].desired,"elapsed native speech duration and Scene root completion cannot replace Panoptes cue");
    }
    allowPanoptes=true;
    for(unsigned i=0;i<15;++i) {
        const auto frame=tick();
        check(!frame.native[bi::asset_index(portal)].desired,"Panoptes cue alone cannot replace the native escape cue");
    }
    allowEscape=true;
    for(unsigned i=0;i<15;++i) {
        check(!tick().native[bi::asset_index(portal)].desired,"early escape cue cannot reveal a portal before Go");
    }
    check(shieldedAmbush,"ambush appears on its native cue before delayed portals");
    allowFinalCue=true;static_cast<void>(tick());
    for(unsigned i=0;i<4;++i) {
        check(!tick().native[bi::asset_index(portal)].desired,"portal waits through the final native speech tail");
    }
    bool released{};
    for(unsigned i=0;i<30 && !released;++i) { released=tick().native[bi::asset_index(portal)].active; }
    check(released,"final native speech tail releases the portal without duplicate global rows");
    for(unsigned i=0;i<20;++i) {
        const auto frame=tick();
        check(frame.section==0 && frame.generations[47]==0,"Future platform and old corridor latch cannot report escape arrival");
    }
    controller.position(run+1,corridorPoint);
    check(tick().generations[47]==0,"foreign-run receiving position cannot release escape dialogue");
    controller.position(run,corridorPoint);bool offered47{};
    for(unsigned i=0;i<15 && !offered47;++i) { offered47=tick().activeRow==47; }
    check(offered47,"fresh native receiving corridor starts row47");
    // The escape corridor and its load zone cannot release the final exchange.
    for(unsigned i=0;i<5;++i) { check(tick().generations[48]==0,"corridor alone cannot start Ikora exchange"); }
    // Run ahead while row47 is still offered but unsubmitted. Only the actual
    // present-Mercury exit is crossed; the outbound intro corridor is never hit.
    enter(spec("lighthouse.mercury_m_vod_lighthouse_030_filter").asset);
    static_cast<void>(tick());controller.position(run,{0,0,0});
    allow47=true;bool offered48{};
    for(unsigned i=0;i<60 && !offered48;++i) { offered48=tick().activeRow==48; }
    check(offered48 && controller.frame().section==1,"actual Lighthouse exit survives row47 without the unrelated outbound corridor");
    for(unsigned i=0;i<5;++i) { check(!tick().finished,"unsubmitted final dialogue cannot complete the mission"); }
    allow48=true;bool complete{};
    for(unsigned i=0;i<60 && !complete;++i) { complete=tick().finished; }
    check(shieldedAmbush,"final route exercised native shielded ambush sources");
    check(complete && offers47==1 && offers48==1,"native final arrival plays rows47 and48 once and completes after48 ends");
    controller.reset();check(controller.select(views,run),"final-arrival replay can reset within the same run id");
    check(controller.seen().none() && controller.frame().generations[47]==0 && controller.frame().generations[48]==0,
        "reset clears prior corridor, Lighthouse and final dialogue observations");
}
}
