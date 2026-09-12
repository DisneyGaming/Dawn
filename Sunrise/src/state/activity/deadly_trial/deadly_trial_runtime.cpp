#include <Windows.h>
#include "runtime.h"
#include "controller.h"
#include "../coo/objective_delivery.h"
#include "../runtime.h"
#include "../../../core/logging/log.h"
#include <mutex>
#include <cstdio>
namespace sunrise::state::activity::deadly_trial {
namespace {
std::mutex mutex;Controller controller;
coo::ReadinessSchedule readinessSchedule;
coo::ObjectiveDelivery objectiveDelivery;
std::unique_ptr<coo::script::MissionDocument> document;
std::uint64_t selectedRun{},lastPublish{},nextTraversal{};
coo::StallDiagnostics stalls;std::uint32_t lastActive{UINT32_MAX};std::uint8_t lastSection{UINT8_MAX};
std::bitset<std::size(kVolumes)> logged;
void log(std::string_view s) noexcept { core::log::write(core::log::Channel::server,core::log::Level::info,s); }
bool current() noexcept { return selectedRun && selectedRun==mission_run_generation(); }
bool load() noexcept {
    static std::once_flag once;std::call_once(once,[] {
        std::string error;
        try {
            HMODULE module{};std::array<wchar_t,32768> path{};
            const bool found=GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&load),&module)!=FALSE;
            const auto size=found?GetModuleFileNameW(module,path.data(),static_cast<DWORD>(path.size())):0;
            if(!size || size>=path.size()) { error="cannot resolve DLL-relative script path"; }
            else { document=coo::script::MissionDocument::read(std::filesystem::path(path.data()).parent_path()/L"Sunrise"/L"scripts"/L"deadly_trial.lua",kProfile,error); }
            if(document && !valid_document(document->views())) { document.reset();error="A Deadly Trial native binding validation failed"; }
        } catch(const std::exception& e) { error=e.what(); }
        std::array<char,768> line{};
        if(document) { std::snprintf(line.data(),line.size(),"ev=coo_script mission=deadly_trial result=loaded format=lua script=Sunrise/scripts/deadly_trial.lua fnv1a64=%016llX reload=next_process",static_cast<unsigned long long>(document->fingerprint())); }
        else { std::snprintf(line.data(),line.size(),"ev=coo_script mission=deadly_trial result=failed reason=%.*s",static_cast<int>((std::min)(error.size(),std::size_t{500})),error.data()); }
        log(line.data());
    });return document!=nullptr;
}
void receipt_log(const char* stage,std::uint64_t run,std::uint32_t value) noexcept {
    std::array<char,192> line{};std::snprintf(line.data(),line.size(),"ev=deadly_trial stage=%s run=%llu value=%u",stage,static_cast<unsigned long long>(run),value);log(line.data());
}
}
bool prepare(std::uint64_t run,bool selected) noexcept {
    const std::lock_guard lock(mutex);if(run!=mission_run_generation()) { return false; }
    if(!selected) { controller.reset();objectiveDelivery={};selectedRun=0;stalls.reset();logged.reset();lastActive=UINT32_MAX;lastSection=UINT8_MAX;nextTraversal=lastPublish=0;readinessSchedule.reset();return false; }
    if(!load()) { return false; }if(run!=selectedRun) { objectiveDelivery={};stalls.reset();logged.reset();lastActive=UINT32_MAX;lastSection=UINT8_MAX;nextTraversal=lastPublish=0;readinessSchedule.reset(); }
    if(!controller.select(document->views(),run)) { return false; }selectedRun=run;return true;
}
coo::ReadinessRequest<EnemyReceipt> readiness_request(std::uint64_t now) noexcept {
    const std::lock_guard lock(mutex);
    if(!selectedRun || selectedRun!=mission_run_generation() || !mission_seed_armed()
        || world_phase()!=WorldPhase::arrived) { return {}; }
    return readinessSchedule.request<EnemyReceipt, 94>(selectedRun,now,
        [&](auto visit) noexcept { controller.pending_enemies(visit); }, 8);
}
coo::Generation traversal_request(std::uint64_t now) noexcept {
    const std::lock_guard lock(mutex);
    if(!current() || !mission_seed_armed() || world_phase()!=WorldPhase::arrived
        || !controller.frame().enabled || controller.frame().finished || now<nextTraversal) return {};
    nextTraversal=now>UINT64_MAX-100?UINT64_MAX:now+100;
    return {selectedRun,controller.frame().spawnGeneration};
}
void observe_mount(const PikeMount& receipt,Point position) noexcept {
    const std::lock_guard lock(mutex);
    if(!current() || !mission_seed_armed() || world_phase()!=WorldPhase::arrived
        || receipt.owner.run!=selectedRun || receipt.owner.value!=controller.frame().spawnGeneration
        || !receipt.valid()) return;
    controller.position(selectedRun,position);
    if(controller.mounted(receipt)) receipt_log("pike_mounted",selectedRun,receipt.vehicle);
}
Frame snapshot(std::uint64_t run,std::uint64_t now,bool ready) noexcept {
    const std::lock_guard lock(mutex);if(!current() || run!=selectedRun) { return {}; }
    auto f=controller.update(run,now,ready && mission_seed_armed() && world_phase()==WorldPhase::arrived);lastPublish=now;
    f.presentation=objectiveDelivery.project({run,f.spawnGeneration},f.presentation);
    const auto d=controller.diagnostics();
    if(d.active!=lastActive || f.section!=lastSection) { lastActive=d.active;lastSection=f.section;
        std::array<char,256> line{};std::snprintf(line.data(),line.size(),"ev=coo_executor mission=deadly_trial run=%llu section=%u phase=%u active=%08X complete=%08X failure=%u finished=%u",static_cast<unsigned long long>(run),f.section,static_cast<unsigned>(d.phase),d.active,d.complete,static_cast<unsigned>(d.failure),f.finished?1U:0U);log(line.data());
    }
    for(std::size_t i=0;i<std::size(kVolumes);++i) { if(controller.seen()[i] && !logged[i]) { logged.set(i);
        std::array<char,256> line{};std::snprintf(line.data(),line.size(),"ev=deadly_trial stage=volume_entered registry=%08X slot=%u name=%.*s",kVolumes[i].registry,kVolumes[i].slot,static_cast<int>(kVolumes[i].name.size()),kVolumes[i].name.data());log(line.data());
    } }
    if(const auto* g=controller.graph();g && d.phase==coo::Phase::running) for(const auto& b:g->commands) {
        if(controller.step_state(b.step).phase!=coo::StepPhase::active) { continue; }
        coo::StallReport report{};if(!stalls.observe({run,d.incarnation,b.step,b.command},controller.missing(g->definition.steps[b.step].commands[b.command]),now,report)) { continue; }
        std::array<char,384> line{};std::snprintf(line.data(),line.size(),"ev=coo_stall mission=deadly_trial command=%.*s missing=%s registry=%08X slot=%u expected=%u actual=%u waiting_ms=%llu",static_cast<int>(b.id.size()),b.id.data(),coo::missing_name(report.detail.missing),report.detail.asset.registry,report.detail.asset.slot,report.detail.expected,report.detail.actual,static_cast<unsigned long long>(report.waitingMs));log(line.data());
    }
    return f;
}
Presentation presentation() noexcept {
    if(!mission_seed_armed() || world_phase()!=WorldPhase::arrived) { return {}; }
    const std::lock_guard lock(mutex);return current()?Presentation{selectedRun,controller.frame()}:Presentation{};
}
std::uint64_t native_run() noexcept { if(!mission_seed_armed() || world_phase()!=WorldPhase::arrived) { return 0; }const std::lock_guard lock(mutex);return current()?selectedRun:0; }
void observe_objective_readiness(coo::Generation owner,std::uint32_t handle,std::uintptr_t component,std::uintptr_t content,bool ready) noexcept {
    const std::lock_guard lock(mutex);
    if(!current() || owner!=coo::Generation{selectedRun,controller.frame().spawnGeneration} || !controller.frame().enabled)return;
    objectiveDelivery.select(owner);
    if(objectiveDelivery.observe(owner,handle,component,content,ready)) {lastPublish=0;}
}
bool publication_due(std::uint64_t now) noexcept {
    if(!mission_seed_armed() || world_phase()!=WorldPhase::arrived) { return false; }const std::lock_guard lock(mutex);
    return current() && controller.frame().enabled && now>=lastPublish+100;
}
Request request() noexcept { if(!mission_seed_armed() || world_phase()!=WorldPhase::arrived) { return {}; }const std::lock_guard lock(mutex);return current()?controller.request():Request{}; }
bool observe_admission(const EnemyReceipt& r) noexcept { const std::lock_guard lock(mutex);const bool ok=current() && controller.admitted(r);if(ok) { receipt_log("enemy_admitted",r.run,r.source); }return ok; }
bool observe_death(const EnemyReceipt& r) noexcept { const std::lock_guard lock(mutex);const bool ok=current() && controller.died(r);if(ok) { receipt_log("enemy_died",r.run,r.source); }return ok; }
void observe_readiness(const EnemyReceipt& r,coo::EnemyReadiness v) noexcept { const std::lock_guard lock(mutex);if(current()) { static_cast<void>(controller.readiness(r,v)); } }
void observe_position(float x,float y,float z) noexcept { if(!mission_seed_armed() || world_phase()!=WorldPhase::arrived) { return; }const std::lock_guard lock(mutex);if(current()) { controller.position(selectedRun,{x,y,z}); } }
void observe_submission(std::uint64_t run,std::uint32_t definition,std::int64_t offset,std::uint32_t bank,std::uint8_t row,std::uint32_t generation) noexcept {
    if(definition!=0x80B2E709U || offset!=0x1408) { return; }const std::lock_guard lock(mutex);
    if(current() && controller.submitted(run,bank,row,generation,GetTickCount64())) { receipt_log("dialogue_submitted",run,row); }
}
void observe_binding(const InteractionBinding& b) noexcept { const std::lock_guard lock(mutex);if(current() && !controller.request().interaction.valid() && controller.bind(b)) { receipt_log("revive_bound",b.owner.run,b.entity); } }
void observe_interaction(const InteractionBinding& b,std::int32_t requested,std::int32_t before,std::int32_t after,bool active) noexcept {
    const std::lock_guard lock(mutex);if(current() && controller.interact(b,requested,before,after,active)) { receipt_log("revive_accepted",b.owner.run,b.entity); }
}
bool observe_scene_binding(const SceneReceipt& r) noexcept {
    const std::lock_guard lock(mutex);const bool changed=!controller.frame().sceneBound;
    const bool ok=current() && controller.bind_scene(r);
    if(ok && changed) { receipt_log("scene_bound",r.run,r.selector); }return ok;
}
bool observe_scene_audio(const SceneReceipt& r,float elapsed) noexcept {
    const std::lock_guard lock(mutex);const bool ok=current() && controller.scene_audio(r,elapsed,GetTickCount64());
    if(ok) { receipt_log("scene_audio_cue",r.run,r.selector); }return ok;
}
void observe_scene(const SceneReceipt& r,bool completed) noexcept {
    const std::lock_guard lock(mutex);const bool changed=completed?!controller.frame().sceneComplete:!controller.frame().sceneStarted;
    if(current() && controller.scene(r,completed) && changed) { receipt_log(completed?"scene_finished":"scene_started",r.run,r.selector); }
}
}
