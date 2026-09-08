#include <Windows.h>
#include "runtime.h"
#include "controller.h"
#include "../runtime.h"
#include "../../../client/hooks/bootflow/gateway_native_read.h"
#include "../../../client/hooks/bootflow/coo_enemy_readiness.h"
#include "../../../client/hooks/bootflow/coo_native_player_mount.h"
#include "../../../core/logging/log.h"
#include <mutex>
#include <cstdio>
namespace sunrise::state::activity::deadly_trial {
namespace {
std::mutex mutex;Controller controller;std::unique_ptr<coo::script::MissionDocument> document;
std::uint64_t selectedRun{},lastPublish{},nextProbe{},nextTraversal{};std::size_t probeCursor{};
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
    if(!selected) { controller.reset();selectedRun=0;stalls.reset();logged.reset();lastActive=UINT32_MAX;lastSection=UINT8_MAX;nextTraversal=nextProbe=lastPublish=0;probeCursor=0;return false; }
    if(!load()) { return false; }if(run!=selectedRun) { stalls.reset();logged.reset();lastActive=UINT32_MAX;lastSection=UINT8_MAX;nextTraversal=nextProbe=lastPublish=0;probeCursor=0; }
    if(!controller.select(document->views(),run)) { return false; }selectedRun=run;return true;
}
namespace {
struct NativeMount {
    std::uintptr_t image{};
    void controlled(std::uint32_t& out) const noexcept { reinterpret_cast<void(*)(std::uint32_t*)>(image+0x4B2260)(&out); }
    void parent(std::uintptr_t row,std::uint32_t& out) const noexcept { reinterpret_cast<void(*)(std::uintptr_t,std::uint32_t*)>(image+0x5582E0)(row,&out); }
    void position(std::uintptr_t row,std::array<float,4>& out) const noexcept { reinterpret_cast<void(*)(std::uintptr_t,std::array<float,4>*)>(image+0x558330)(row,&out); }
    bool valid(client::hooks::bootflow::gateway_native::Read& read) const noexcept {
        constexpr std::array<std::uintptr_t,3> entries{0x4B2260,0x5582E0,0x558330};
        constexpr std::array<std::array<std::uint8_t,16>,3> prefixes{{
            {0x40,0x53,0x48,0x83,0xEC,0x20,0x48,0x8B,0xD9,0xC7,0x01,0xFF,0xFF,0xFF,0xFF,0x48},
            {0x48,0x83,0xC1,0xA0,0xC7,0x02,0xFF,0xFF,0xFF,0xFF,0xB8,0x00,0x00,0x00,0x00,0xF6},
            {0x40,0x57,0x48,0x83,0xEC,0x40,0x48,0x83,0xC1,0xA0,0xB8,0x00,0x00,0x00,0x00,0x48}}};
        for(std::size_t i=0;i<entries.size();++i) {
            std::array<std::uint8_t,16> bytes{};
            if(!read.value(image+entries[i],bytes) || bytes!=prefixes[i]) { return false; }
        }return true;
    }
};
void poll_traversal(std::uint64_t run) noexcept {
    { const std::lock_guard lock(mutex);
      if(!current() || run!=selectedRun || !controller.frame().enabled || controller.frame().finished || GetTickCount64()<nextTraversal) { return; }
      nextTraversal=GetTickCount64()+100;
    }
    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    client::hooks::bootflow::gateway_native::Read read{image};NativeMount native{image};
    client::hooks::bootflow::coo_native::MountedPlayer sample{};
    if(!native.valid(read) || !client::hooks::bootflow::coo_native::mounted_pike(read,native,sample)) { return; }
    const std::lock_guard lock(mutex);if(!current() || run!=selectedRun) { return; }
    controller.position(run,{sample.position[0],sample.position[1],sample.position[2]});
    if(controller.mounted({{run,controller.frame().spawnGeneration},sample.player,sample.vehicle,sample.seat})) {
        receipt_log("pike_mounted",run,sample.vehicle);
    }
}
}
void poll(std::uint64_t run) noexcept {
    if(!mission_seed_armed() || world_phase()!=WorldPhase::arrived) { return; }
    poll_traversal(run);
    std::array<EnemyReceipt,94> pending{};std::size_t count{},begin{};
    { const std::lock_guard lock(mutex);if(!current() || run!=selectedRun || GetTickCount64()<nextProbe) { return; }
      nextProbe=GetTickCount64()+500;controller.pending_enemies([&](const EnemyReceipt& r) noexcept { if(count<pending.size()) { pending[count++]=r; } });
      if(count) { begin=probeCursor%count;probeCursor=(begin+8)%count; }
    }
    const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    for(std::size_t i=0;i<(std::min)(count,std::size_t{8});++i) {
        client::hooks::bootflow::gateway_native::Read read{image};const auto& r=pending[(begin+i)%count];
        observe_readiness(r,client::hooks::bootflow::coo_native::enemy(read,image,r));
    }
}
Frame snapshot(std::uint64_t run,std::uint64_t now,bool ready) noexcept {
    poll(run);const std::lock_guard lock(mutex);if(!current() || run!=selectedRun) { return {}; }
    const auto f=controller.update(run,now,ready && mission_seed_armed() && world_phase()==WorldPhase::arrived);lastPublish=now;
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
bool publication_due(std::uint64_t now) noexcept {
    if(!mission_seed_armed() || world_phase()!=WorldPhase::arrived) { return false; }const std::lock_guard lock(mutex);
    return current() && controller.frame().enabled && !controller.frame().finished && now>=lastPublish+100;
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
