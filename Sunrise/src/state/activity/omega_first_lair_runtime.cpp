#include <Windows.h>

#include <array>
#include <cstdio>

#include "omega_first_lair_runtime.h"
#include "omega_crown_transit_authority.h"
#include "omega_rescue_marker_authority.h"
#include "runtime.h"
#include "../../core/logging/log.h"

namespace sunrise::state::activity::omega_first_lair {
namespace {
SRWLOCK g_lock=SRWLOCK_INIT;
Encounter g_run{};
omega_archive_arm::Ledger g_arm{};
omega_archive_intro::Ledger g_intro{};
Boss arm_boss(const omega_archive_arm::Owner& owner) noexcept {
    return {owner.run,owner.actor,owner.character,owner.entity,owner.generation,owner.revision,owner.island,owner.actionEpoch};
}
std::uint32_t g_generation{};
std::uint64_t g_revision{},g_published{},g_lastPublish{};
std::uint64_t g_transitRejectLogged{};
std::uint64_t g_transitCreated{};
std::uint32_t g_dpsBackEntity{UINT32_MAX};
std::uint8_t g_dpsBackCycle{};
std::uint16_t g_rescueMarkerReadyMask{};

bool admitted(std::uint64_t run) noexcept {
    return run!=0 && run==g_run.run() && run==mission_run_generation()
        && g_generation!=0 && mission_seed_armed() && !omega_authority_quiesced()
        && world_phase()!=WorldPhase::idle;
}
void changed(const char* event) noexcept {
    ++g_revision;
    std::array<char,400> line{};
    const int size=std::snprintf(line.data(),line.size(),
        "ev=omega_first_lair stage=%s run=%llu phase=%u action=%u failed=%u cannon=%u revision=%llu island=%u final_cannon=%u prepared=%u crown=%u cycle=%u wave=%u crown_stage=%u epoch=%u",
        event,static_cast<unsigned long long>(g_run.run()),static_cast<unsigned>(g_run.phase()),
        static_cast<unsigned>(g_run.pending()),g_run.failed()?1U:0U,g_run.cannon_active()?1U:0U,
        static_cast<unsigned long long>(g_revision),static_cast<unsigned>(g_run.island()),
        g_run.final_cannon_active()?1U:0U,static_cast<unsigned>(g_run.cannon_prepared_mask()),
        g_run.crown_restricted()?1U:0U,static_cast<unsigned>(g_run.cycle()),static_cast<unsigned>(g_run.wave()),
        static_cast<unsigned>(g_run.crown_stage()),g_run.boss().actionEpoch);
    if(size>0 && static_cast<std::size_t>(size)<line.size()) {
        core::log::write(core::log::Channel::server,core::log::Level::info,
            {line.data(),static_cast<std::size_t>(size)});
    }
}
void advance_locked() noexcept {
    if(!g_run.executor_owned() || g_run.failed()) { return; }
    const auto before=g_run.executor_diagnostics();
    g_run.update_executor();
    const auto after=g_run.executor_diagnostics();
    if(before.incarnation==after.incarnation && before.active==after.active
        && before.complete==after.complete && before.phase==after.phase && !g_run.failed()) { return; }
    changed("executor");
    const auto& definition=*coo::combat::kSections[g_run.executor_section()];
    std::string_view waiting="complete";
    for(std::size_t i=0;i<definition.steps.size();++i) {
        if((after.active&(std::uint32_t{1}<<i))!=0) { waiting=definition.steps[i].name;break; }
    }
    std::array<char,384> line{};
    const auto size=std::snprintf(line.data(),line.size(),
        "ev=coo_combat run=%llu section=%u incarnation=%llu phase=%u active=%08X complete=%08X failure=%u failed=%u waiting=\"%.*s\"",
        static_cast<unsigned long long>(after.run),static_cast<unsigned>(g_run.executor_section()),
        static_cast<unsigned long long>(after.incarnation),static_cast<unsigned>(after.phase),after.active,after.complete,
        static_cast<unsigned>(after.failure),g_run.failed()?1U:0U,static_cast<int>(waiting.size()),waiting.data());
    if(size>0 && static_cast<std::size_t>(size)<line.size()) {
        core::log::write(core::log::Channel::server,core::log::Level::info,{line.data(),static_cast<std::size_t>(size)});
    }
}
}

Authority authority(std::uint64_t run,std::uint32_t generation,bool executorOwned) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    Authority output{};
    // The FX source needs the succeeding generation, so retain signed headroom.
    if(run!=0 && run==mission_run_generation() && generation!=0 && generation<0x7FFFFFFFU
        && mission_seed_armed() && !omega_authority_quiesced() && world_phase()!=WorldPhase::idle) {
        if(run!=g_run.run()) {
            g_run.begin(run,executorOwned);g_arm={};g_intro={};g_generation=generation;g_revision=1;g_published=0;g_transitRejectLogged=0;g_transitCreated=0;
            g_dpsBackEntity=UINT32_MAX;g_dpsBackCycle=0;
            g_rescueMarkerReadyMask=0;
            changed("prepare");
        }
        if(generation==g_generation) {
            advance_locked();
            output.generation=generation;
            output.arm=g_arm.status().control;
            output.intro=g_intro.status().program;
            if(g_run.island()==4) { output.crownGeneration=generation; }
            for(std::size_t index=0;index<kAllGroups.size();++index) {
                if(!g_run.group_enabled(index)) { continue; }
                const auto& group=kAllGroups[index];
                if(group.registry==0x0040BF06U || group.registry==0x0040BF05U || group.registry==0x0040BF03U || group.registry==0x0040BF04U) {
                    if(group.registry==0x0040BF06U && group.member) { output.crownAnchor=true; }
                    else if(group.registry==0x0040BF05U && group.member) { output.hiveAnchor=true; }
                    else if(group.registry==0x0040BF03U && group.member) { output.vexAnchor=true; }
                    else if(group.registry==0x0040BF04U && group.member) { output.cabalAnchor=true; }
                    else {
                        auto* requested=group.registry==0x0040BF06U?output.crownLoose[group.source].data():
                            group.registry==0x0040BF05U?output.hiveLoose[group.source].data():
                            group.registry==0x0040BF04U?output.cabalLoose[group.source].data():output.vexLoose[group.source].data();
                        for(std::size_t category=0;category<2;++category) {
                            requested[category]=static_cast<std::uint8_t>(requested[category]+group.requested[category]);
                        }
                    }
                    continue;
                }
                if(group.member) { output.anchor=true; }
                else { output.loose[group.source]=static_cast<std::uint8_t>(output.loose[group.source]+group.count); }
            }
            output.rescueScenes=g_run.rescue_scenes();
            output.rescueMarkerReadyMask=g_rescueMarkerReadyMask;
            output.cycle=g_run.cycle();output.wave=g_run.wave();
            output.routeEnabled=g_run.route_enabled();output.chargeEnabled=g_run.charge_enabled();
            output.chargeDunked=g_run.charge_dunked();
            output.eyeStatusActive=g_run.eye_status_active();
            output.exitCannon=g_run.final_traversal();output.endingRequested=g_run.ending_requested();
            output.transitLaunches=output.routeEnabled;
            output.transitBridge=g_run.transit_bridge();
            output.transitTarget=g_run.transit_target()
                && omega_crown_transit::dps_platform_created(output.cycle,g_transitCreated)
                && (output.cycle==3 || g_dpsBackCycle==output.cycle);
            output.transitCreated=g_transitCreated;
            output.returnLaunch=g_run.return_launch();
            output.finalTraversal=output.exitCannon;
            if(g_run.island()==4) {
                output.restriction=g_run.crown_restricted()?omega_crown_respawn::Restriction::enable:omega_crown_respawn::Restriction::disable;
            }
            output.cannon=g_run.cannon_active();
            output.finalCannon=g_run.final_cannon_active();
            output.crownRestricted=g_run.crown_restricted();
            g_published=g_revision;g_lastPublish=GetTickCount64();
        }
    }
    ReleaseSRWLockExclusive(&g_lock);
    return output;
}
Status status(std::uint64_t run) noexcept {
    AcquireSRWLockShared(&g_lock);
    const bool active=admitted(run);
    const Status output{g_run.boss(),active?g_run.pending():Action::none,g_run.phase(),active,g_run.failed(),g_run.island(),
        g_run.cycle(),g_run.wave(),g_run.crown_stage(),g_run.token(),g_arm.status(),g_intro.status()};
    ReleaseSRWLockShared(&g_lock);return output;
}
bool observe_rescue_marker_created(std::uint64_t run,std::uint32_t generation,
                                   std::uint16_t slot) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    const auto* marker=omega_rescue_markers::find(slot);
    const auto bit=omega_rescue_markers::bit(slot);
    const bool accepted=admitted(run) && generation==g_generation && !g_run.failed()
        && marker!=nullptr && omega_rescue_markers::requested(g_run.rescue_scenes(),*marker)
        && (g_rescueMarkerReadyMask&bit)==0;
    if(accepted) {
        g_rescueMarkerReadyMask=static_cast<std::uint16_t>(g_rescueMarkerReadyMask|bit);
        changed("rescue_marker_created");
    }
    ReleaseSRWLockExclusive(&g_lock);
    return accepted;
}
bool request_intro(const omega_archive_intro::Owner& owner) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    const bool accepted=admitted(owner.run) && owner.generation==g_generation && !g_run.failed()
        && !g_run.boss().valid() && g_run.phase()==Phase::initial && g_intro.request(owner);
    if(accepted) { changed("intro_program_requested"); }
    ReleaseSRWLockExclusive(&g_lock);return accepted;
}
bool request_boss_program(const omega_archive_intro::Owner& owner,std::uint32_t sequence) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    constexpr std::array<std::uint32_t,4> sequences{0x65D2379CU,0x65D2379EU,0x65D2379DU,0x65D2379BU};
    const auto cycle=g_run.cycle();
    const bool accepted=admitted(owner.run) && owner.generation==g_generation && !g_run.failed()
        && arm_boss(owner)==g_run.boss() && g_run.pending()==Action::summonBoth
        && cycle<sequences.size() && sequence==sequences[cycle] && !g_arm.status().pending && !g_arm.status().control.high
        && g_intro.next(owner,sequence);
    if(accepted) { static_cast<void>(g_run.claim(arm_boss(owner),Action::summonBoth));changed("boss_program_requested"); }
    ReleaseSRWLockExclusive(&g_lock);return accepted;
}
bool request_boss_movement(const omega_archive_intro::Owner& owner) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    const auto action=g_run.pending();
    const bool moving=(owner.island<4 && action==Action::depart)
        || (owner.island==4 && action==Action::relocateFinal && g_run.cycle()==2);
    const bool accepted=admitted(owner.run) && owner.generation==g_generation && !g_run.failed()
        && arm_boss(owner)==g_run.boss() && moving && !g_arm.status().pending && !g_arm.status().control.high
        && g_intro.depart(owner);
    if(accepted) { static_cast<void>(g_run.claim(arm_boss(owner),action));changed("boss_movement_requested"); }
    ReleaseSRWLockExclusive(&g_lock);return accepted;
}
bool observe_intro_control(const omega_archive_intro::Owner& owner) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    const auto& program=g_intro.status();
    const bool current=!g_run.boss().valid() || arm_boss(program.owner)==g_run.boss();
    const bool accepted=admitted(owner.run) && owner.generation==g_generation && current
        && (!g_run.failed() || !program.program.play) && g_intro.acknowledge(owner);
    if(accepted) { changed("intro_program_applied"); }
    ReleaseSRWLockExclusive(&g_lock);return accepted;
}
void observe_initial_summon(const Boss& boss) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    if(admitted(boss.run) && boss.generation==g_generation && g_intro.status().applied
        && arm_boss(g_intro.status().owner)==boss && g_run.initial_summon(boss)) { changed("initial_summon"); }
    ReleaseSRWLockExclusive(&g_lock);
}
void observe_initial_idle(const Boss& boss) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    if(admitted(boss.run) && boss.generation==g_generation && g_intro.status().applied
        && arm_boss(g_intro.status().owner)==boss) {
        // Idle without the node-4 receipt still releases the cohort; log it apart.
        const bool late=g_run.phase()==Phase::initial;
        if(g_run.initial_idle(boss)) { changed(late?"initial_idle_late_summon":"initial_idle"); }
    }
    ReleaseSRWLockExclusive(&g_lock);
}
bool claim_action(const Boss& boss,Action action) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    const bool claimed=admitted(boss.run) && action!=Action::summonLeft && action!=Action::summonRight && action!=Action::summonBoth && action!=Action::depart && action!=Action::relocateFinal
        && g_run.claim(boss,action);
    if(claimed) { changed("action_claimed"); }
    ReleaseSRWLockExclusive(&g_lock);return claimed;
}
void observe_summon(const Boss& boss,Action action,bool finished) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    const bool single=action==Action::summonLeft || action==Action::summonRight;
    const auto& arm=g_arm.status();
    const bool acceptedArm=(!single && g_intro.status().applied) || (single && arm_boss(arm.owner)==boss && !arm.pending
        && arm.control.right==(action==Action::summonRight) && arm.control.high && !finished);
    if(admitted(boss.run) && acceptedArm && (finished?g_run.summon_finished(boss,action):g_run.summon_started(boss,action))) {
        changed(finished?"summon_finished":"summon_started");
    }
    ReleaseSRWLockExclusive(&g_lock);
}
bool prepare_arm(const omega_archive_arm::Owner& owner) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    const auto action=g_run.pending();
    const bool accepted=admitted(owner.run) && !g_run.failed() && owner.generation==g_generation
        && g_intro.status().applied && arm_boss(owner)==g_run.boss() && (action==Action::summonLeft || action==Action::summonRight)
        && g_arm.prepare(owner,action==Action::summonRight);
    if(accepted) { static_cast<void>(g_run.claim(arm_boss(owner),action));changed("arm_requested"); }
    ReleaseSRWLockExclusive(&g_lock);return accepted;
}
bool observe_arm_control(const omega_archive_arm::Owner& owner,const omega_archive_arm::NativeControl& receipt,
                         const std::array<float,4>& left,const std::array<float,4>& right) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    const bool accepted=admitted(owner.run) && owner.generation==g_generation
        && arm_boss(owner)==g_run.boss() && g_arm.acknowledge(owner,receipt,left,right);
    if(accepted) {
        const auto& arm=g_arm.status();
        if(!arm.control.high && arm.completed && !g_run.failed()) {
            if(!g_run.summon_finished(arm_boss(owner),arm.control.right?Action::summonRight:Action::summonLeft)) {
                g_run.invalidate(owner.run);
            }
        }
        changed(arm.control.high?"arm_applied":"arm_released");
    }
    ReleaseSRWLockExclusive(&g_lock);return accepted;
}
bool release_arm(const omega_archive_arm::Owner& owner,bool completed) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    const auto expected=g_arm.status().control.right?Phase::rightPlaying:Phase::leftPlaying;
    const bool accepted=admitted(owner.run) && arm_boss(owner)==g_run.boss()
        && (!completed || (!g_run.failed() && g_run.phase()==expected)) && g_arm.release(owner,completed);
    if(accepted) {
        if(!completed) { g_run.invalidate(owner.run); }
        changed(completed?"arm_release_requested":"arm_cancelled");
    }
    ReleaseSRWLockExclusive(&g_lock);return accepted;
}
bool observe_admission(const ActorReceipt& receipt) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    bool accepted=false;
    if(admitted(receipt.run) && receipt.generation==g_generation) {
        const bool wasFailed=g_run.failed();
        accepted=g_run.admitted(receipt);
        if(accepted) { changed("actor_admitted"); }
        else if(!wasFailed && g_run.failed()) { changed("unexpected_population"); }
    }
    ReleaseSRWLockExclusive(&g_lock);return accepted;
}
bool observe_death(const ActorReceipt& receipt) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    const bool accepted=admitted(receipt.run) && receipt.generation==g_generation && g_run.died(receipt);
    if(accepted) { changed("actor_died"); }
    ReleaseSRWLockExclusive(&g_lock);return accepted;
}
void observe_departure(const Boss& boss,bool folded,bool atMilestone) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    if(admitted(boss.run) && g_intro.status().applied && g_intro.status().program.departure==boss.island
        && arm_boss(g_intro.status().owner)==boss && (boss.island==4?
        g_run.final_departed({boss,g_run.cycle()},folded,atMilestone):g_run.departed(boss,folded,atMilestone))) { changed("departed"); }
    ReleaseSRWLockExclusive(&g_lock);
}
bool observe_arrival(std::uint64_t run,std::uint8_t islandIndex) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    const bool accepted=admitted(run) && g_run.arrived(run,islandIndex);
    if(accepted) { changed("arrival"); }
    ReleaseSRWLockExclusive(&g_lock);return accepted;
}
bool observe_crown_arrival(std::uint64_t run) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    const bool accepted=admitted(run) && g_run.crown_arrived(run);
    if(accepted) { changed("crown_arrival"); }
    ReleaseSRWLockExclusive(&g_lock);return accepted;
}
void observe_cannon_prepared(std::uint64_t run,std::uint32_t generation,std::uint8_t index) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    if(admitted(run) && generation==g_generation) {
        const auto before=g_run.cannon_prepared_mask();g_run.cannon_prepared(run,generation,index);
        if(before!=g_run.cannon_prepared_mask()) { changed("cannon_prepared"); }
    }
    ReleaseSRWLockExclusive(&g_lock);
}
bool observe_animation(const CrownToken& token,AnimationMilestone event) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    const bool accepted=admitted(token.boss.run) && g_intro.status().applied && g_run.animation(token,event);
    if(accepted) { changed("animation_milestone"); }
    ReleaseSRWLockExclusive(&g_lock);return accepted;
}
bool observe_scene(const CrownToken& token,std::uint16_t slot,SceneMilestone event) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    const bool accepted=admitted(token.boss.run) && g_run.scene(token,slot,event);
    if(accepted) { changed("scene_milestone"); }
    ReleaseSRWLockExclusive(&g_lock);return accepted;
}
SceneRequest scene_request(std::uint64_t run,std::uint16_t slot) noexcept {
    AcquireSRWLockShared(&g_lock);
    const SceneRequest result=admitted(run)?g_run.scene_request(slot):SceneRequest{};
    ReleaseSRWLockShared(&g_lock);return result;
}
bool observe_charge(const ChargeReceipt& receipt,ChargeMilestone event) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    const bool accepted=admitted(receipt.token.boss.run) && g_run.charge(receipt,event);
    if(accepted) { changed("charge_milestone"); }
    ReleaseSRWLockExclusive(&g_lock);return accepted;
}
bool observe_gate_arrival(const CrownToken& token,GateMilestone gate,std::uint32_t player) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    const bool accepted=admitted(token.boss.run) && g_run.gate_arrived(token,gate,player);
    if(accepted) { changed("gate_arrival"); }
    ReleaseSRWLockExclusive(&g_lock);return accepted;
}
bool observe_health(const CrownToken& token,HealthMilestone event) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    const bool accepted=admitted(token.boss.run) && g_run.health(token,event);
    if(accepted) { changed("health_milestone"); }
    ReleaseSRWLockExclusive(&g_lock);return accepted;
}
bool observe_ending(const CrownToken& token,bool finished) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    const bool accepted=admitted(token.boss.run) && g_run.ending(token,finished);
    if(accepted) { changed(finished?"ending_finished":"ending_started"); }
    ReleaseSRWLockExclusive(&g_lock);return accepted;
}
void observe_transit_prepared(std::uint64_t run,std::uint32_t generation,std::uint8_t core) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    if(admitted(run) && generation==g_generation) {
        const auto before=g_run.transit_prepared_mask();g_run.transit_prepared(run,generation,core);
        if(before!=g_run.transit_prepared_mask()) { changed("transit_prepared"); }
    } else if(g_transitRejectLogged!=run) {
        g_transitRejectLogged=run;
        std::array<char,240> line{};
        const int size=std::snprintf(line.data(),line.size(),
            "ev=omega_first_lair stage=transit_prepared_rejected run=%llu current_run=%llu admitted=%u generation=%u expected=%u core=%u",
            static_cast<unsigned long long>(run),static_cast<unsigned long long>(g_run.run()),
            admitted(run)?1U:0U,generation,g_generation,static_cast<unsigned>(core));
        if(size>0 && static_cast<std::size_t>(size)<line.size()) {
            core::log::write(core::log::Channel::server,core::log::Level::info,{line.data(),static_cast<std::size_t>(size)});
        }
    }
    ReleaseSRWLockExclusive(&g_lock);
}
bool observe_transit_created(std::uint64_t run,std::uint8_t index,std::uint32_t entity) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    bool accepted{};
    if(admitted(run) && index<omega_crown_transit::kSources.size() && index<64) {
        const auto bit=UINT64_C(1)<<index;
        if(omega_crown_transit::kSources[index].role==omega_crown_transit::Role::returnLauncher) {
            // A prior cycle's creation bit cannot qualify a second launch.
            accepted=g_run.return_created(run,entity);
            if(accepted) { g_transitCreated|=bit;changed("return_created"); }
        } else if(index==32) {
            // This immediate back is retired after each eye visit. The old
            // created bit and old full entity cannot release the next visit.
            const auto cycle=g_run.cycle();
            if(entity!=UINT32_MAX && entity!=g_dpsBackEntity && cycle>=1 && cycle<=2
                && g_run.charge_dunked() && !g_run.return_launch() && !g_run.failed()) {
                g_dpsBackEntity=entity;g_dpsBackCycle=cycle;g_transitCreated|=bit;
                accepted=true;changed("dps_back_created");
            }
        } else if((g_transitCreated&bit)==0) {
            g_transitCreated|=bit;accepted=true;changed("transit_created");
        }
    }
    ReleaseSRWLockExclusive(&g_lock);return accepted;
}
void invalidate(std::uint64_t run) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    if(admitted(run) && !g_run.failed()) { static_cast<void>(g_arm.release(g_arm.status().owner,false));static_cast<void>(g_intro.cancel());g_run.invalidate(run);changed("invalidated"); }
    ReleaseSRWLockExclusive(&g_lock);
}
bool publication_due(std::uint64_t now) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    if(admitted(g_run.run())) { advance_locked(); }
    const bool due=admitted(g_run.run()) && g_revision!=g_published && now>=g_lastPublish+250;
    ReleaseSRWLockExclusive(&g_lock);return due;
}
void reset() noexcept {
    AcquireSRWLockExclusive(&g_lock);
    g_run.begin(0);g_arm={};g_intro={};g_generation=0;g_revision=0;g_published=0;g_lastPublish=0;
    g_dpsBackEntity=UINT32_MAX;g_dpsBackCycle=0;
    g_rescueMarkerReadyMask=0;
    ReleaseSRWLockExclusive(&g_lock);
}
} // namespace sunrise::state::activity::omega_first_lair
