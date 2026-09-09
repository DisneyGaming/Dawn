#include <Windows.h>
#include "runtime.h"
#include "controller.h"
#include "../runtime.h"
#include "../../../core/logging/log.h"
#include <cstdio>
#include <mutex>
#include "../../../client/hooks/bootflow/gateway_native_read.h"
#include "../../../client/hooks/bootflow/coo_enemy_readiness.h"

namespace sunrise::state::activity::deep_storage {
namespace {
std::mutex mutex;
Controller controller;
std::unique_ptr<coo::script::MissionDocument> document;
std::uint64_t selectedRun{},nextPublication{};
coo::StallDiagnostics stalled;
std::uint32_t lastActive{UINT32_MAX},lastComplete{UINT32_MAX};
std::uint8_t lastSection{UINT8_MAX};
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
            else { document=coo::script::MissionDocument::read(std::filesystem::path(path.data()).parent_path()/L"Sunrise"/L"scripts"/L"deep_storage.lua",kProfile,error); }
            if(document && !valid_document(document->views())) { document.reset();error="Deep Storage native profile mismatch"; }
        } catch(const std::exception& e) { error=e.what(); }
        std::array<char,768> line{};
        if(document) { std::snprintf(line.data(),line.size(),"ev=coo_script mission=deep_storage result=loaded format=lua fnv1a64=%016llX path=Sunrise/scripts/deep_storage.lua reload=next_process",static_cast<unsigned long long>(document->fingerprint())); }
        else { std::snprintf(line.data(),line.size(),"ev=coo_script mission=deep_storage result=failed reason=\"%.*s\"",static_cast<int>((std::min)(error.size(),std::size_t{500})),error.data()); }
        log(line.data());
    });
    return document!=nullptr;
}
}
bool prepare(std::uint64_t run,bool selected) noexcept {
    const std::lock_guard lock(mutex);if(run!=mission_run_generation()) {return false;}
    if(!selected) {controller.reset();selectedRun=nextPublication=0;stalled.reset();return false;}
    if(!load() || !controller.select(document->views(),run)) {return false;}
    if(selectedRun!=run) {lastActive=lastComplete=UINT32_MAX;lastSection=UINT8_MAX;stalled.reset();}
    selectedRun=run;return true;
}
namespace {
void log_receipt(const char* stage,std::uint64_t run,std::uint32_t value) noexcept {
    std::array<char,192> line{};std::snprintf(line.data(),line.size(),"ev=deep_storage stage=%s run=%llu value=%u",stage,static_cast<unsigned long long>(run),value);log(line.data());
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
Frame snapshot(std::uint64_t run,std::uint64_t now,bool ready) noexcept {
    poll_readiness(run,now);const std::lock_guard lock(mutex);if(!current() || run!=selectedRun) {return {};}
    const auto f=controller.update(run,now,ready);nextPublication=now+100;const auto d=controller.diagnostics();
    if(d.active!=lastActive || d.complete!=lastComplete || f.section!=lastSection) {
        lastActive=d.active;lastComplete=d.complete;lastSection=f.section;std::array<char,320> line{};
        std::snprintf(line.data(),line.size(),"ev=coo_executor mission=deep_storage run=%llu generation=%u section=%u phase=%u active=%08X complete=%08X failure=%u mission_finished=%u",
            static_cast<unsigned long long>(run),f.spawnGeneration,f.section,static_cast<unsigned>(d.phase),d.active,d.complete,static_cast<unsigned>(d.failure),f.finished?1U:0U);log(line.data());
    }
    if(const auto* g=controller.graph();g && d.phase==coo::Phase::running) for(const auto& b:g->commands) {
        if(controller.step_state(b.step).phase!=coo::StepPhase::active) {continue;}coo::StallReport report{};
        if(!stalled.observe({run,d.incarnation,b.step,b.command},controller.missing(g->definition.steps[b.step].commands[b.command]),now,report)) {continue;}
        std::array<char,384> line{};std::snprintf(line.data(),line.size(),"ev=coo_stall mission=deep_storage command=%.*s missing=%s registry=%08X slot=%u expected=%u actual=%u waiting_ms=%llu",
            static_cast<int>(b.id.size()),b.id.data(),coo::missing_name(report.detail.missing),report.detail.asset.registry,report.detail.asset.slot,report.detail.expected,report.detail.actual,static_cast<unsigned long long>(report.waitingMs));log(line.data());
    }return f;
}
Request request() noexcept {const std::lock_guard lock(mutex);return current()?Request{controller.owner(),controller.frame()}:Request{};}
std::uint64_t native_run() noexcept {const std::lock_guard lock(mutex);return current()?selectedRun:0;}
bool publication_due(std::uint64_t now) noexcept {const std::lock_guard lock(mutex);return current() && controller.frame().enabled && !controller.frame().finished && now>=nextPublication;}
void observe_position(float x,float y,float z) noexcept {const std::lock_guard lock(mutex);if(current()) {controller.position(selectedRun,{x,y,z});}}
void observe_submission(std::uint64_t run,std::uint32_t definition,std::int64_t offset,std::uint32_t bank,std::uint8_t row,std::uint32_t generation) noexcept {
    if(!dialogue_identity(definition,offset,bank,row)) {return;}const std::lock_guard lock(mutex);
    if(current() && controller.submitted(run,bank,row,generation,GetTickCount64())) {log_receipt("dialogue_submitted",run,row);}
}
void observe_prepared(coo::Generation owner,coo::Asset a) noexcept {const std::lock_guard lock(mutex);if(current()) {static_cast<void>(controller.prepared(owner,a));}}
void observe_object(const coo::ObjectReceipt& r) noexcept {const std::lock_guard lock(mutex);if(current()) {static_cast<void>(controller.object(r));}}
bool observe_admission(const EnemyReceipt& r) noexcept {const std::lock_guard lock(mutex);return current() && controller.admitted(r);}
bool observe_death(const EnemyReceipt& r) noexcept {const std::lock_guard lock(mutex);const bool ok=current() && controller.died(r);if(ok) {log_receipt("enemy_died",r.run,r.source);}return ok;}
void observe_readiness(const EnemyReceipt& r,coo::EnemyReadiness v) noexcept {const std::lock_guard lock(mutex);if(current()) {static_cast<void>(controller.readiness(r,v));}}
LivingEnemies living_enemies() noexcept {
    const std::lock_guard lock(mutex);LivingEnemies out{};if(!current() || !controller.frame().enabled) {return out;}
    out.owner=controller.owner();controller.living_enemies([&](const EnemyReceipt& r) {if(out.count<out.actors.size()) {out.actors[out.count++]=r;}});return out;
}
LensRequest lens_request() noexcept {const std::lock_guard lock(mutex);return current()?controller.lens_request():LensRequest{};}
void observe_lens(const LensReceipt& r,bool dead) noexcept {const std::lock_guard lock(mutex);if(current() && controller.lens(r,dead)) {log_receipt(dead?"lens_destroyed":"lens_bound",r.owner.run,r.health);}}
PlateRequest plate_request(std::size_t i) noexcept {const std::lock_guard lock(mutex);return current()?controller.plate_request(i):PlateRequest{};}
void observe_plate_binding(const PlateReceipt& r) noexcept {const std::lock_guard lock(mutex);if(current() && controller.bind_plate(r)) {log_receipt("plate_bound",r.owner.run,r.index);}}
void observe_plate(const PlateReceipt& r,std::uint32_t revision,float value,bool complete) noexcept {const std::lock_guard lock(mutex);if(current() && controller.plate(r,revision,value,complete)) {log_receipt("plate_charged",r.owner.run,r.index);}}
void observe_contested(const PlateReceipt& r,bool value) noexcept {const std::lock_guard lock(mutex);if(current()) {static_cast<void>(controller.contested(r,value));}}
ScanRequest scan_request(std::size_t i) noexcept {const std::lock_guard lock(mutex);return current()?controller.scan_request(i):ScanRequest{};}
void observe_scan_binding(const ScanReceipt& r) noexcept {const std::lock_guard lock(mutex);if(current() && controller.bind_scan(r)) {log_receipt("scan_bound",r.owner.run,r.index);}}
void observe_scan(const ScanReceipt& r,bool started,bool complete) noexcept {const std::lock_guard lock(mutex);if(current() && controller.scan(r,started,complete)) {log_receipt(complete?"scan_finished":"scan_started",r.owner.run,r.index);}}
}
