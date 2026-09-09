#include <Windows.h>
#include "runtime.h"
#include "controller.h"
#include "../runtime.h"
#include "../../../core/logging/log.h"
#include <cstdio>
#include <mutex>

namespace sunrise::state::activity::beyond_infinity {
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
            else { document=coo::script::MissionDocument::read(std::filesystem::path(path.data()).parent_path()/L"Sunrise"/L"scripts"/L"beyond_infinity.lua",kProfile,error); }
            if(document && !valid_document(document->views())) { document.reset();error="Beyond Infinity native profile mismatch"; }
        } catch(const std::exception& e) { error=e.what(); }
        std::array<char,768> line{};
        if(document) { std::snprintf(line.data(),line.size(),"ev=coo_script mission=beyond_infinity result=loaded format=lua fnv1a64=%016llX path=Sunrise/scripts/beyond_infinity.lua reload=next_process",static_cast<unsigned long long>(document->fingerprint())); }
        else { std::snprintf(line.data(),line.size(),"ev=coo_script mission=beyond_infinity result=failed reason=\"%.*s\"",static_cast<int>((std::min)(error.size(),std::size_t{500})),error.data()); }
        log(line.data());
    });
    return document!=nullptr;
}
}
bool prepare(std::uint64_t run,bool selected) noexcept {
    const std::lock_guard lock(mutex);
    if(run!=mission_run_generation()) { return false; }
    if(!selected) { controller.reset();selectedRun=nextPublication=0;stalled.reset();lastActive=lastComplete=UINT32_MAX;lastSection=UINT8_MAX;return false; }
    if(!load() || !controller.select(document->views(),run)) { return false; }
    if(selectedRun!=run) { stalled.reset();lastActive=lastComplete=UINT32_MAX;lastSection=UINT8_MAX; }
    selectedRun=run;return true;
}
Frame snapshot(std::uint64_t run,std::uint64_t now,bool ready) noexcept {
    const std::lock_guard lock(mutex);
    if(run!=selectedRun || run!=mission_run_generation()) { return {}; }
    const auto frame=controller.update(run,now,ready && current());
    const auto d=controller.diagnostics();
    if(frame.section!=lastSection || d.active!=lastActive || d.complete!=lastComplete) {
        lastSection=frame.section;lastActive=d.active;lastComplete=d.complete;
        std::array<char,320> line{};
        std::snprintf(line.data(),line.size(),"ev=coo_executor mission=beyond_infinity run=%llu generation=%u section=%u phase=%u active=%08X complete=%08X failure=%u forest_pass=%u mission_finished=%u",
            static_cast<unsigned long long>(run),frame.spawnGeneration,frame.section,static_cast<unsigned>(d.phase),d.active,d.complete,static_cast<unsigned>(d.failure),frame.forestPass,frame.finished?1U:0U);log(line.data());
    }
    if(const auto* graph=controller.graph();graph && d.phase==coo::Phase::running) {
        for(const auto& binding:graph->commands) {
            if(controller.step_state(binding.step).phase!=coo::StepPhase::active) { continue; }
            const auto& spec=graph->definition.steps[binding.step].commands[binding.command];coo::StallReport report{};
            if(!stalled.observe({run,d.incarnation,binding.step,binding.command},controller.missing(spec),now,report)) { continue; }
            std::array<char,512> line{};
            std::snprintf(line.data(),line.size(),"ev=coo_stall mission=beyond_infinity command=%.*s missing=%s registry=%08X type=%u slot=%u waiting_ms=%llu",
                static_cast<int>(binding.id.size()),binding.id.data(),coo::missing_name(report.detail.missing),spec.asset.registry,spec.asset.type,spec.asset.slot,static_cast<unsigned long long>(report.waitingMs));log(line.data());
        }
    }
    nextPublication=now+100;return frame;
}
Request request() noexcept {
    const std::lock_guard lock(mutex);return current()?Request{controller.owner(),controller.frame()}:Request{};
}
LensRequest lens_request() noexcept {
    const std::lock_guard lock(mutex);
    if(!current()) { return {}; }
    const auto& frame=controller.frame();const auto& lens=controller.lens_owner();
    const auto& native=frame.native[asset_index(kLens)];
    return {controller.owner(),lens,frame.enabled && native.active
        && (!lens.valid() || (lens.owner.run==selectedRun && lens.owner.value==native.generation)),
        frame.lensExposed,frame.lensDestroyed,native.generation};
}
PlateRequest plate_request() noexcept {
    const std::lock_guard lock(mutex);if(!current()) { return {}; }
    const auto& frame=controller.frame();const auto plate=controller.plate_owner();
    const auto& native=frame.native[asset_index(kPlate)];
    return {controller.owner(),plate,frame.plateRevision,frame.enabled && native.active && plate.valid()
        && plate.owner.run==selectedRun && plate.owner.value==native.generation,frame.plateOccupied,frame.lensDestroyed,frame.lensExposed};
}
void observe_plate_binding(const PlateReceipt& receipt) noexcept {
    const std::lock_guard lock(mutex);if(!current() || !controller.bind_plate(receipt)) { return; }
    std::array<char,240> line{};std::snprintf(line.data(),line.size(),"ev=beyond_infinity stage=plate_bound run=%llu generation=%u entity=%08X device=%08X timer=%08X",static_cast<unsigned long long>(receipt.owner.run),receipt.owner.value,receipt.entity,receipt.device,receipt.timer);log(line.data());
}
void observe_plate(const PlateReceipt& receipt,std::uint32_t revision,float value,bool complete) noexcept {
    const std::lock_guard lock(mutex);if(!current() || !controller.plate(receipt,revision,value,complete)) { return; }
    std::array<char,240> line{};std::snprintf(line.data(),line.size(),"ev=beyond_infinity stage=plate_charged run=%llu generation=%u entity=%08X revision=%u evidence=native_timer_1006F20",static_cast<unsigned long long>(receipt.owner.run),receipt.owner.value,receipt.entity,revision);log(line.data());
}
bool publication_due(std::uint64_t now) noexcept {
    const std::lock_guard lock(mutex);return current() && now>=nextPublication;
}
void observe_position(float x,float y,float z) noexcept {
    const std::lock_guard lock(mutex);if(current()) { controller.position(selectedRun,{x,y,z}); }
}
void observe_submission(std::uint64_t run,std::uint32_t definition,std::int64_t offset,
    std::uint32_t bank,std::uint8_t row,std::uint32_t generation) noexcept {
    if(!dialogue_identity(definition,offset,bank,row)) { return; }
    const std::lock_guard lock(mutex);
    if(!current() || run!=selectedRun || !controller.submitted(run,bank,row,generation,GetTickCount64())) { return; }
    std::array<char,192> line{};std::snprintf(line.data(),line.size(),"ev=beyond_infinity stage=dialogue_submitted run=%llu row=%u generation=%u",static_cast<unsigned long long>(run),row,generation);log(line.data());
}
void observe_prepared(coo::Generation owner,coo::Asset asset) noexcept {
    const std::lock_guard lock(mutex);if(current()) { static_cast<void>(controller.prepared(owner,asset)); }
}
void observe_lens(const LensReceipt& receipt,bool dead) noexcept {
    const std::lock_guard lock(mutex);
    if(!current() || !controller.lens(receipt,dead)) { return; }
    std::array<char,192> line{};std::snprintf(line.data(),line.size(),"ev=beyond_infinity stage=lens_%s run=%llu generation=%u entity=%08X evidence=native_health",dead?"destroyed":"bound",static_cast<unsigned long long>(receipt.owner.run),receipt.owner.value,receipt.entity);log(line.data());
}
void observe_scene(const SceneReceipt& receipt,bool complete) noexcept {
    const std::lock_guard lock(mutex);
    if(!current() || !controller.scene(receipt,complete)) { return; }
    std::array<char,240> line{};std::snprintf(line.data(),line.size(),"ev=beyond_infinity stage=scene_%s run=%llu registry=%08X slot=%u generation=%u evidence=native_B438B0",complete?"finished":"bound",static_cast<unsigned long long>(receipt.owner.run),receipt.asset.registry,receipt.asset.slot,receipt.owner.value);log(line.data());
}
void observe_transit(coo::Generation owner,std::uint8_t route) noexcept {
    const std::lock_guard lock(mutex);if(!current() || !controller.transit(owner,route)) { return; }
    std::array<char,192> line{};std::snprintf(line.data(),line.size(),
        "ev=beyond_infinity stage=transit_arrived run=%llu generation=%u route=%u evidence=native_membership_tuple",
        static_cast<unsigned long long>(owner.run),owner.value,route);log(line.data());
}
void observe_scene_cue(const SceneReceipt& receipt,std::uint8_t id,std::uint32_t state,std::uint32_t starts) noexcept {
    const std::lock_guard lock(mutex);
    if(!current() || !controller.scene_cue(receipt,id,state,starts)) { return; }
    std::array<char,256> line{};std::snprintf(line.data(),line.size(),
        "ev=beyond_infinity stage=scene_cue run=%llu registry=%08X slot=%u generation=%u cue=%u state=%u starts=%u evidence=native_node_input",
        static_cast<unsigned long long>(receipt.owner.run),receipt.asset.registry,receipt.asset.slot,receipt.owner.value,id,state,starts);log(line.data());
}
void observe_scene_speech(const SceneReceipt& receipt,std::uint8_t row,std::uint32_t state) noexcept {
    const std::lock_guard lock(mutex);
    if(!current() || !controller.scene_speech(receipt,row,state)) { return; }
    std::array<char,256> line{};std::snprintf(line.data(),line.size(),
        "ev=beyond_infinity stage=scene_speech run=%llu registry=%08X slot=%u generation=%u row=%u state=%u evidence=native_node_98",
        static_cast<unsigned long long>(receipt.owner.run),receipt.asset.registry,receipt.asset.slot,receipt.owner.value,row,state);log(line.data());
}

}
