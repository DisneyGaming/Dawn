#pragma once
#include "../../../src/state/activity/beyond_infinity/controller.h"
#include "../../../src/state/activity/beyond_infinity/authority.h"
#include "../../../src/middleware/encoding/bit_writer.h"
#include <bit>

namespace beyond_native_past_return_fixture {
template<class Check> void run(Check check,const sunrise::state::activity::coo::script::Views& shipped) {
    namespace bi=sunrise::state::activity::beyond_infinity;
    namespace coo=sunrise::state::activity::coo;
    const auto asset=[&](std::string_view id) {
        for(const auto& c:bi::kCapabilities) { if(c.id==id) { return c.spec.asset; } }
        check(false,"native Past return capability exists");return coo::Asset{};
    };
    const auto portal=asset("past.vex_teleporter_device.on");
    // Test actual packed authority, including the off transition and a neighboring
    // device, rather than checking only a helper's chosen floating point value.
    bi::Frame encoded{};encoded.enabled=true;encoded.spawnGeneration=66;
    const auto encodedPosition=[&](coo::Asset a,bool active) {
        encoded.native[bi::asset_index(a)]={66,true,active,true,active};
        std::array<std::byte,19> bytes{};
        sunrise::middleware::encoding::bits::Writer writer(bytes);
        check(bi::write_body(writer,encoded,a.registry,static_cast<std::uint8_t>(a.type),a.slot)
            && writer.bit_count()==147,"Past portal writes the unchanged native device body");
        std::uint32_t bits{};
        for(unsigned i=0;i<4;++i) { bits=(bits<<8)|std::to_integer<std::uint8_t>(bytes[i]); }
        return std::bit_cast<float>(bits);
    };
    check(encodedPosition(portal,true)==.1F,"C7 wire selects native activate range instead of unmatched position1");
    check(encodedPosition(portal,false)==0.F,"C7 off retains native zero mode");
    for(const auto name:{"ambush.vex_teleporter_fx_device.on","ambush.vex_teleporter_fx2_device.on"}) {
        const auto frame=asset(name);
        check(encodedPosition(frame,true)==.1F,"Future static frame uses its native activate range");
        check(encodedPosition(frame,false)==0.F,"Future frame retires with native zero mode");
    }
    check(encodedPosition(asset("past.vex_machine2_device.on"),true)==1.F,"portal mode does not alter neighboring machine devices");

    auto views=shipped;views.phases=shipped.phases.subspan(4,2);
    bi::Controller controller;constexpr std::uint64_t run=803;
    check(controller.select(views,run),"replay shipped Past and second Forest phases");
    const auto enter=[&](coo::Asset a) {
        for(const auto& v:bi::kVolumes) {
            if(v.asset!=a) { continue; }
            for(unsigned x=1;x<20;++x) { for(unsigned y=1;y<20;++y) {
                const bi::Point p{v.min.x+(v.max.x-v.min.x)*float(x)/20,
                    v.min.y+(v.max.y-v.min.y)*float(y)/20,(v.min.z+v.max.z)/2};
                if(bi::contains(v,p)) { controller.position(run,p);return; }
            } }
        }
        check(false,"native Past return volume has an interior point");
    };
    enter(views.observationStart->asset);
    std::uint64_t now=1000;controller.update(run,now,true);
    // This corridor was already visited on the first Forest traversal.
    controller.position(run,{-901.F,1073.F,-63.9F});
    controller.position(run,{740.F,711.F,7.7F});
    const auto receiving=asset("forest.tv_end_1.occupied"),entry=asset("forest.tv_begin_past");
    const auto pastScene=asset("past.scene_past_echo");
    bool allowGuardian{},guardianHeld{},portalReady{},arrivalHeld{};
    unsigned row37Offers{};std::uint64_t guardianEnd{};
    const auto tick=[&](bool drivePast) {
        const auto frame=controller.update(run,now,true);now+=1000;
        check(frame.transitRoute==0,"native Past return never requests a reconstructed host teleport");
        check(frame.generations[36]==0,"second Forest entry does not request row36");
        if(guardianEnd && now-1000<guardianEnd) {
            check(!frame.native[bi::asset_index(portal)].desired,"submitted row34 must finish playing before portal release");
        }
        if(frame.activeRow==37) { ++row37Offers; }
        if(frame.activeRow!=coo::kNoDialogue && (frame.activeRow!=34 || allowGuardian)) {
            if(frame.activeRow==34) { guardianEnd=now+views.dialogue.rows[34].durationMs+views.dialogue.rows[34].delayMs+views.dialogue.spacingMs; }
            static_cast<void>(controller.submitted(run,bi::kBank,frame.activeRow,frame.generations[frame.activeRow],now));
        }
        for(std::size_t i=0;i<std::size(bi::kAssets);++i) {
            const auto& state=frame.native[i];const auto a=bi::kAssets[i].asset;
            if(a.type==4 && state.managed && !state.prepared) { static_cast<void>(controller.prepared({run,state.generation},a)); }
        }
        const auto& state=frame.native[bi::asset_index(pastScene)];
        if(state.active) {
            const bi::SceneReceipt receipt{{run,state.generation},pastScene,10,20,30,40};
            static_cast<void>(controller.scene(receipt,false));static_cast<void>(controller.scene(receipt,true));
            for(const auto& cue:bi::kScenes[bi::scene_index(pastScene)].cues) {
                if(cue.id!=17) { static_cast<void>(controller.scene_cue(receipt,cue.id,2,1)); }
            }
        }
        if(drivePast && frame.section==0) {
            const auto* graph=controller.graph();
            for(const auto& command:graph->commands) {
                if(controller.step_state(command.step).phase!=coo::StepPhase::active) { continue; }
                const auto& spec=graph->definition.steps[command.step].commands[command.command];
                if(spec.asset==receiving) {
                    check(spec.argument==1,"native return uses current receiving corridor occupancy");arrivalHeld=true;
                } else if(spec.asset!=entry && spec.asset.type==60) { enter(spec.asset); }
            }
        }
        return frame;
    };
    for(unsigned i=0;i<150 && !guardianHeld;++i) {
        const auto frame=tick(true);
        if(frame.activeRow==34) { guardianHeld=true; }
        check(!frame.native[bi::asset_index(portal)].desired,"portal remains absent before row34 is submitted");
    }
    check(guardianHeld,"native Past replay reaches row34");
    for(unsigned i=0;i<5;++i) {
        const auto frame=tick(true);
        check(!frame.native[bi::asset_index(portal)].desired,"elapsed time without real row34 submission cannot release portal");
    }
    allowGuardian=true;
    for(unsigned i=0;i<150 && !(portalReady && arrivalHeld);++i) {
        const auto frame=tick(true);portalReady=frame.native[bi::asset_index(portal)].active;
        check(frame.section==0 && frame.generations[37]==0,"old first-pass corridor latch cannot start second Forest");
    }
    check(portalReady && arrivalHeld,"completed row34 releases portal without the absent51EE cue");
    controller.position(run,{740.F,711.F,7.7F});
    for(unsigned i=0;i<20;++i) { check(tick(false).section==0,"Past occupancy cannot impersonate the receiving corridor"); }
    controller.position(run+1,{-901.F,1073.F,-63.9F});
    check(tick(false).section==0,"foreign-run return position is rejected");
    // Actual native return sampled in the accepted run; no transit receipt is injected.
    controller.position(run,{-901.F,1073.F,-63.9F});
    for(unsigned i=0;i<10;++i) {
        const auto frame=tick(false);
        check(frame.section==0 && frame.generations[37]==0,"receiving corridor waits for the second Forest entrance");
    }
    controller.position(run,{-810.F,1073.F,-63.9F});
    for(unsigned i=0;i<100;++i) { static_cast<void>(tick(false)); }
    check(controller.frame().section==1 && controller.frame().forestPass==2 && row37Offers==1,
        "native return followed by Forest entrance starts Fallen pass and row37 exactly once");
}
}
