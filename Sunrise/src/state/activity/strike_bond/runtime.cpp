#include <Windows.h>
#include "runtime.h"
#include "controller.h"
#include "../runtime.h"
#include "../../../core/logging/log.h"
#include <cstdio>
#include <cmath>
#include <mutex>
#include "../../../client/hooks/bootflow/gateway_native_read.h"
#include "../../../client/hooks/bootflow/coo_enemy_readiness.h"

namespace sunrise::state::activity::strike_bond {
namespace {
std::mutex mutex;
Controller controller;
std::unique_ptr<coo::script::MissionDocument> document;
std::uint64_t selectedRun{},nextPublication{};
std::bitset<std::size(kVolumes)> loggedVolumes{};bool loggedFault{};coo::StallDiagnostics stalled;
std::bitset<std::size(kLenses)> refusedLenses{};
std::uint32_t lastObjective{UINT32_MAX};coo::MarkerTarget lastMarker{};
std::uint32_t lastActive{UINT32_MAX},lastComplete{UINT32_MAX};
std::uint8_t lastSection{UINT8_MAX},lastBossStage{UINT8_MAX};bool lastBossFighting{};
bool current() noexcept { return selectedRun && selectedRun==mission_run_generation() && mission_seed_armed() && world_phase()==WorldPhase::arrived; }
void log(std::string_view text) noexcept { core::log::write(core::log::Channel::server,core::log::Level::info,text); }
bool load() noexcept {
    static std::once_flag once;
    std::call_once(once,[] {
        std::string error;
        try {
            HMODULE module{};std::array<wchar_t,32768> path{};
            const bool found=GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(&load),&module)!=FALSE;
            const auto size=found?GetModuleFileNameW(module,path.data(),static_cast<DWORD>(path.size())):0;
            if(!size || size>=path.size()) { error="cannot resolve DLL-relative script path"; }
            else { document=coo::script::MissionDocument::read(std::filesystem::path(path.data()).parent_path()/L"Sunrise"/L"scripts"/L"strike_bond.lua",kProfile,error); }
            if(document && !valid_document(document->views())) { document.reset();error="Garden World native profile mismatch"; }
        } catch(const std::exception& e) { error=e.what(); }
        std::array<char,768> line{};
        if(document) { std::snprintf(line.data(),line.size(),"ev=coo_script mission=strike_bond result=loaded format=lua fnv1a64=%016llX path=Sunrise/scripts/strike_bond.lua reload=next_process",static_cast<unsigned long long>(document->fingerprint())); }
        else { std::snprintf(line.data(),line.size(),"ev=coo_script mission=strike_bond result=failed reason=\"%.*s\"",static_cast<int>((std::min)(error.size(),std::size_t{500})),error.data()); }
        log(line.data());
    });
    return document!=nullptr;
}
}
bool prepare(std::uint64_t run,bool selected) noexcept {
    const std::lock_guard lock(mutex);if(run!=mission_run_generation()) {return false;}
    if(!selected) {controller.reset();selectedRun=nextPublication=0;stalled.reset();return false;}
    if(!load() || !controller.select(document->views(),run)) {return false;}
    if(selectedRun!=run) {loggedVolumes.reset();loggedFault=false;lastActive=lastComplete=UINT32_MAX;lastSection=lastBossStage=UINT8_MAX;lastBossFighting=false;stalled.reset();refusedLenses.reset();lastObjective=UINT32_MAX;lastMarker={};}
    selectedRun=run;return true;
}
namespace {
void log_receipt(const char* stage,std::uint64_t run,std::uint32_t value) noexcept {
    std::array<char,192> line{};std::snprintf(line.data(),line.size(),"ev=strike_bond stage=%s run=%llu value=%u",stage,static_cast<unsigned long long>(run),value);log(line.data());
}
void poll_readiness(std::uint64_t run,std::uint64_t now) noexcept {
    static std::uint64_t next{},lastRun{};static std::size_t cursor{};
    std::array<EnemyReceipt,256> pending{};std::size_t count{},begin{};
    {const std::lock_guard lock(mutex);if(!current() || run!=selectedRun) {return;}
     if(run!=lastRun) {next=0;cursor=0;lastRun=run;}if(now<next) {return;}next=now+500;
     controller.pending_enemies([&](const auto& r) {if(count<pending.size()) {pending[count++]=r;}});
     if(count) {begin=cursor%count;cursor=(begin+12)%count;}}
    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    for(std::size_t i=0;i<(std::min)(count,std::size_t{12});++i) {
        client::hooks::bootflow::gateway_native::Read read{image};const auto& r=pending[(begin+i)%count];
        observe_readiness(r,client::hooks::bootflow::coo_native::enemy(read,image,r));
    }

}
}
Frame snapshot(std::uint64_t run,std::uint64_t now,bool ready,int region) noexcept {
    poll_readiness(run,now);const std::lock_guard lock(mutex);if(!current() || run!=selectedRun) {return {};}
    const auto f=controller.update(run,now,ready,region);nextPublication=now+100;const auto d=controller.diagnostics();
    if(d.active!=lastActive || d.complete!=lastComplete || f.section!=lastSection) {
        lastActive=d.active;lastComplete=d.complete;lastSection=f.section;std::array<char,320> line{};
        std::snprintf(line.data(),line.size(),"ev=coo_executor mission=strike_bond run=%llu generation=%u section=%u phase=%u active=%08X complete=%08X failure=%u mission_finished=%u",
            static_cast<unsigned long long>(run),f.spawnGeneration,f.section,static_cast<unsigned>(d.phase),d.active,d.complete,static_cast<unsigned>(d.failure),f.finished?1U:0U);log(line.data());
    }
    if(f.bossFighting!=lastBossFighting || (f.bossFighting && f.bossStage!=lastBossStage)) {
        lastBossFighting=f.bossFighting;lastBossStage=f.bossStage;
        std::array<char,192> line{};
        std::snprintf(line.data(),line.size(),"ev=strike_bond stage=boss_phase run=%llu fighting=%u damage_bar=%u floor=%.6f",
            static_cast<unsigned long long>(run),f.bossFighting?1U:0U,f.bossStage+1U,boss_floor(f));log(line.data());
    }
    // "The flag for that objective is in the wrong place" is unanswerable without knowing which
    // waypoint was published alongside which directive. Report the pair whenever either changes.
    if(f.presentation.event!=lastObjective || !(f.presentation.marker==lastMarker)) {
        lastObjective=f.presentation.event;lastMarker=f.presentation.marker;
        std::array<char,288> line{};
        std::snprintf(line.data(),line.size(),
            "ev=strike_bond stage=presentation run=%llu section=%u objective=%u retired=%u marker_registry=%08X marker_type=%u marker_slot=%u marker_valid=%u",
            static_cast<unsigned long long>(run),f.section,f.presentation.event,f.presentation.retiredEvent,
            f.presentation.marker.asset.registry,static_cast<unsigned>(f.presentation.marker.asset.type),
            f.presentation.marker.asset.slot,f.presentation.marker.valid()?1U:0U);
        log(line.data());
    }
    if(f.populationFault && !loggedFault) {
        loggedFault=true;std::array<char,256> line{};
        std::snprintf(line.data(),line.size(),"ev=strike_bond stage=population_fault run=%llu result=host_disabled reason=admission_overflow",
            static_cast<unsigned long long>(run));log(line.data());
    }
    for(std::size_t i=0;i<std::size(kVolumes);++i) {
        if(!controller.seen()[i] || loggedVolumes[i]) {continue;}
        loggedVolumes.set(i);const auto& v=kVolumes[i];std::array<char,320> line{};
        std::snprintf(line.data(),line.size(),"ev=strike_bond stage=volume_entered run=%llu registry=%08X type=60 slot=%u bubble=%u evidence=local_player_position",
            static_cast<unsigned long long>(run),v.asset.registry,v.asset.slot,static_cast<unsigned>(v.bubble));log(line.data());
    }
    if(const auto* g=controller.graph();g && d.phase==coo::Phase::running) for(const auto& b:g->commands) {
        if(controller.step_state(b.step).phase!=coo::StepPhase::active) {continue;}coo::StallReport report{};
        if(!stalled.observe({run,d.incarnation,b.step,b.command},controller.missing(g->definition.steps[b.step].commands[b.command]),now,report)) {continue;}
        std::array<char,384> line{};std::snprintf(line.data(),line.size(),"ev=coo_stall mission=strike_bond command=%.*s missing=%s registry=%08X slot=%u expected=%u actual=%u waiting_ms=%llu",
            static_cast<int>(b.id.size()),b.id.data(),coo::missing_name(report.detail.missing),report.detail.asset.registry,report.detail.asset.slot,report.detail.expected,report.detail.actual,static_cast<unsigned long long>(report.waitingMs));log(line.data());
        // A condition stalls under a synthetic asset (registry FFFFFFFF, slot 65535), so the
        // per-volume dump below never fired for the one case that needed it - past.gates sat
        // unsatisfied for seven minutes and the log could not say whether the player was ten
        // metres away or in a different part of the map. Answer it: where the player is, and the
        // nearest boxes to them in this bubble.
        if(report.detail.asset.type!=60 && controller.has_point()) {
            const Point q=controller.point();const int bubble=f.region>=0?f.region/8:-1;
            std::array<std::size_t,3> best{};std::array<float,3> bestD{1e30F,1e30F,1e30F};
            for(std::size_t i=0;i<std::size(kVolumes);++i) {
                const auto& v=kVolumes[i];if(bubble>=0 && static_cast<int>(v.bubble)!=bubble) {continue;}
                const float dx=q.x<v.min.x?v.min.x-q.x:q.x>v.max.x?q.x-v.max.x:0.F;
                const float dy=q.y<v.min.y?v.min.y-q.y:q.y>v.max.y?q.y-v.max.y:0.F;
                const float dz=q.z<v.min.z?v.min.z-q.z:q.z>v.max.z?q.z-v.max.z:0.F;
                const float sq=dx*dx+dy*dy+dz*dz;
                for(std::size_t k=0;k<3;++k) {
                    if(sq<bestD[k]) {
                        for(std::size_t j=2;j>k;--j) {bestD[j]=bestD[j-1];best[j]=best[j-1];}
                        bestD[k]=sq;best[k]=i;break;
                    }
                }
            }
            std::array<char,384> point{};
            std::snprintf(point.data(),point.size(),
                "ev=strike_bond stage=stall_point command=%.*s region=%d bubble=%d player=%.1f,%.1f,%.1f "
                "near0=%u@%.0f near1=%u@%.0f near2=%u@%.0f",
                static_cast<int>(b.id.size()),b.id.data(),f.region,bubble,q.x,q.y,q.z,
                kVolumes[best[0]].asset.slot,bestD[0]<1e29F?std::sqrt(bestD[0]):-1.F,
                kVolumes[best[1]].asset.slot,bestD[1]<1e29F?std::sqrt(bestD[1]):-1.F,
                kVolumes[best[2]].asset.slot,bestD[2]<1e29F?std::sqrt(bestD[2]):-1.F);
            log(point.data());
        }
        // A stalled trigger-volume observation is nearly always "is the player in the box".
        // Answer it in the log instead of guessing: the box, the player, and the region filter.
        if(report.detail.asset.type==60 && controller.has_point()) {
            const Point q=controller.point();
            for(std::size_t i=0;i<std::size(kVolumes);++i) {
                const auto& v=kVolumes[i];if(!(v.asset==report.detail.asset)) {continue;}
                std::array<char,448> box{};
                std::snprintf(box.data(),box.size(),"ev=strike_bond stage=volume_wait slot=%u bubble=%u region=%d landed=%u min=%.1f,%.1f,%.1f max=%.1f,%.1f,%.1f player=%.1f,%.1f,%.1f inside=%u",
                    v.asset.slot,static_cast<unsigned>(v.bubble),f.region,controller.landed()?1U:0U,
                    v.min.x,v.min.y,v.min.z,v.max.x,v.max.y,v.max.z,q.x,q.y,q.z,contains(v,q)?1U:0U);
                log(box.data());break;
            }
        }
    }return f;
}
Request request() noexcept {const std::lock_guard lock(mutex);return current()?Request{controller.owner(),controller.frame()}:Request{};}
std::uint64_t native_run() noexcept {const std::lock_guard lock(mutex);return current()?selectedRun:0;}
bool publication_due(std::uint64_t now) noexcept {const std::lock_guard lock(mutex);return current() && controller.frame().enabled && now>=nextPublication;}
void observe_position(float x,float y,float z) noexcept {
    const std::lock_guard lock(mutex);if(!current()) return;
    controller.position(selectedRun,{x,y,z});
    static std::uint64_t heightRun{};static bool insideUpper{},reportedAbove{};
    if(heightRun!=selectedRun) {heightRun=selectedRun;insideUpper=reportedAbove=false;}
    if(controller.frame().region!=136) return;
    for(const auto& volume:kVolumes) if(volume.asset.registry==0x2CB86C0FU && volume.asset.slot==406) {
        const bool inside=contains(volume,{x,y,z});
        const bool above=z>volume.max.z && x>=volume.min.x && x<=volume.max.x && y>=volume.min.y && y<=volume.max.y;
        if(inside!=insideUpper || (above && !reportedAbove)) {
            std::array<char,256> line{};
            std::snprintf(line.data(),line.size(),"ev=strike_bond stage=upper_height_check run=%llu volume=406 inside=%u above=%u player=%.3f,%.3f,%.3f upper_toggle_requested=off",
                static_cast<unsigned long long>(selectedRun),inside?1U:0U,above?1U:0U,x,y,z);log(line.data());
        }
        insideUpper=inside;reportedAbove=reportedAbove || above;break;
    }
}
void observe_submission(std::uint64_t run,std::uint32_t definition,std::int64_t offset,std::uint32_t bank,std::uint8_t row,std::uint32_t generation) noexcept {
    if(definition!=kDialogueAsset.definition || offset!=0x1408 || bank!=kBank || row>=std::size(kDialogueRows)) {return;}const std::lock_guard lock(mutex);
    if(current() && controller.submitted(run,bank,row,generation,GetTickCount64())) {log_receipt("dialogue_submitted",run,row);}
}
void observe_prepared(coo::Generation owner,coo::Asset a) noexcept {const std::lock_guard lock(mutex);if(current()) {static_cast<void>(controller.prepared(owner,a));}}
void observe_object(const coo::ObjectReceipt& r) noexcept {const std::lock_guard lock(mutex);if(current()) {static_cast<void>(controller.object(r));}}
bool observe_admission(const EnemyReceipt& r) noexcept {
    // Admissions were the one population signal with no telemetry, so an area that spawned nothing
    // and an area whose actors were never admitted looked identical in the log.
    const std::lock_guard lock(mutex);const bool ok=current() && controller.admitted(r);
    if(ok) {log_receipt("enemy_admitted",r.run,r.source);}return ok;
}
bool observe_death(const EnemyReceipt& r) noexcept {const std::lock_guard lock(mutex);const bool ok=current() && controller.died(r);if(ok) {log_receipt("enemy_died",r.run,r.source);}return ok;}
EnemyReceipt boss_enemy() noexcept {const std::lock_guard lock(mutex);return current()?controller.boss_enemy():EnemyReceipt{};}
BossRequest boss_request() noexcept {const std::lock_guard lock(mutex);return current()?BossRequest{controller.owner(),controller.boss_enemy(),controller.frame()}:BossRequest{};}
EnemyReceipt guardian_enemy(std::uint32_t registry,std::uint16_t source) noexcept {
    const std::lock_guard lock(mutex);EnemyReceipt result{};
    if(current()) controller.living_enemies([&](const EnemyReceipt& r) {
        if(r.registry==registry && r.source==source) result=r;
    });return result;
}
void observe_health(const EnemyReceipt& r,float value) noexcept {
    const std::lock_guard lock(mutex);if(current()) static_cast<void>(controller.health(r,value));
}
void observe_readiness(const EnemyReceipt& r,coo::EnemyReadiness v) noexcept {const std::lock_guard lock(mutex);if(current()) {static_cast<void>(controller.readiness(r,v));}}

LensRequest lens_request(std::size_t i) noexcept {const std::lock_guard lock(mutex);return current()?controller.lens_request(i):LensRequest{};}
void observe_lens(const LensReceipt& r,bool dead) noexcept {
    const std::lock_guard lock(mutex);if(!current()) {return;}
    if(controller.lens(r,dead)) {log_receipt(dead?"lens_destroyed":"lens_bound",r.owner.run,r.asset.slot);return;}
    // A refused DEATH receipt was silent, so "the player never shot it" and "the host threw the
    // kill away" looked identical from the log - and the run stalled on two lenses that had both
    // bound. Report the refusal, with the state that decided it, once per lens per run.
    if(!dead) {return;}
    const auto i=lens_index(r.asset);if(i==std::size(kLenses) || refusedLenses.test(i)) {return;}
    refusedLenses.set(i);
    const auto q=controller.lens_request(i);
    std::array<char,288> line{};
    std::snprintf(line.data(),line.size(),
        "ev=strike_bond stage=lens_refused run=%llu slot=%u receipt_gen=%u want_gen=%u enabled=%u vulnerable=%u already_dead=%u entity=%u serial=%u",
        static_cast<unsigned long long>(r.owner.run),r.asset.slot,r.owner.value,q.generation,
        q.enabled?1U:0U,q.vulnerable?1U:0U,q.destroyed?1U:0U,r.entity,r.serial);
    log(line.data());
}
void observe_costs(std::uint32_t key,std::uint16_t slot,const coo::TaskCosts& costs) noexcept {const std::lock_guard lock(mutex);std::int8_t task{};std::uint32_t known{};if(current()) static_cast<void>(controller.costed(key,slot,costs,task,known));}
void observe_player_trigger(std::uint64_t run,std::uint32_t key,std::uint16_t slot) noexcept {const std::lock_guard lock(mutex);if(current()) static_cast<void>(controller.player_trigger(run,key,slot));}
void observe_generator(std::uint64_t run,std::uint32_t key,std::uint16_t slot,std::uint32_t seed,std::uint32_t completed) noexcept {const std::lock_guard lock(mutex);if(current()) controller.generator(run,key,slot,seed,completed);}
void observe_scene(std::uint64_t run,std::uint32_t key,std::uint16_t slot,const middleware::bap::activity_message::scene_sense::Output& r) noexcept {const std::lock_guard lock(mutex);if(current()) controller.scene(run,key,slot,r);}
void observe_squad(std::uint64_t run,std::uint32_t key,std::uint16_t slot,const middleware::bap::activity_message::squad_sense::Output& r) noexcept {const std::lock_guard lock(mutex);if(current()) controller.squad(run,key,slot,r);}
void observe_combatant(std::uint64_t run,std::uint32_t key,std::uint16_t slot,const middleware::bap::activity_message::combatant_sense::Output& r) noexcept {const std::lock_guard lock(mutex);if(current()) controller.combatant(run,key,slot,r);}
}
