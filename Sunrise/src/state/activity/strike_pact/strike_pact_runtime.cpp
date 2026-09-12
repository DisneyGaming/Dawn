#include <Windows.h>
#include "runtime.h"
#include "controller.h"
#include "../runtime.h"
#include "../../../core/logging/log.h"
#include <cstdio>
#include <mutex>
namespace sunrise::state::activity::strike_pact {
namespace {
std::mutex mutex;
Controller controller;
coo::ReadinessSchedule readinessSchedule;
coo::StallDiagnostics stalled,background;
std::uint64_t nextPublication{};
std::uint32_t publishedRevision{UINT32_MAX};
std::uint64_t selectedRun{};
std::unique_ptr<coo::script::MissionDocument> document;
std::bitset<kAllVolumes.size()> loggedVolumes;
std::uint32_t loggedActive{UINT32_MAX},loggedTimeouts{};
std::uint8_t loggedSection{UINT8_MAX};
coo::Phase loggedPhase{coo::Phase::idle};
bool loggedBank{};
void log(std::string_view message) noexcept {
    core::log::write(core::log::Channel::server,core::log::Level::info,message);
}
bool load() noexcept {
    static std::once_flag once;
    std::call_once(once,[] {
        std::string error;
        try {
            HMODULE module{};std::array<wchar_t,32768> path{};
            const bool found=GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(&load),&module)!=FALSE;
            const auto size=found?GetModuleFileNameW(module,path.data(),static_cast<DWORD>(path.size())):0;
            if(size==0 || size>=path.size()) { error="cannot resolve DLL-relative script path"; }
            else { document=coo::script::MissionDocument::read(std::filesystem::path(path.data()).parent_path()/L"Sunrise"/L"scripts"/L"strike_pact.lua",kProfile,error); }
            if(document && !valid_document(document->views())) { document.reset();error="strike_pact native binding validation failed"; }
        } catch(const std::exception& exception) { error=exception.what(); }
        std::array<char,768> line{};
        if(document) {
            std::snprintf(line.data(),line.size(),"ev=coo_script mission=strike_pact result=loaded format=lua fnv1a64=%016llX path=Sunrise/scripts/strike_pact.lua scope=mission reload=next_process",
                static_cast<unsigned long long>(document->fingerprint()));
        } else { std::snprintf(line.data(),line.size(),"ev=coo_script mission=strike_pact result=failed reason=\"%.*s\"",static_cast<int>((std::min)(error.size(),std::size_t{500})),error.data()); }
        log(line.data());
    });
    return document!=nullptr;
}
void reset_diagnostics() noexcept {
    stalled.reset();background.reset();readinessSchedule.reset();nextPublication=0;publishedRevision=UINT32_MAX;
    loggedVolumes.reset();loggedActive=UINT32_MAX;loggedSection=UINT8_MAX;loggedTimeouts=0;loggedPhase=coo::Phase::idle;loggedBank=false;
}
}
bool prepare(std::uint64_t run,bool selected) noexcept {
    const std::lock_guard lock(mutex);
    if(run!=mission_run_generation()) { return false; }
    if(!selected) {
        if(selectedRun!=0) {
            std::array<char,192> line{};
            std::snprintf(line.data(),line.size(),"ev=strike_pact stage=reset run=%llu reason=destination_changed",
                static_cast<unsigned long long>(selectedRun));log(line.data());
        }
        controller.reset();readinessSchedule.reset();reset_diagnostics();selectedRun=0;return false;
    }
    if(!load()) { return false; }
    if(run!=selectedRun) { reset_diagnostics(); }
    if(!controller.select(document->views(),run)) { return false; }
    selectedRun=run;return true;
}
coo::ReadinessRequest<EnemyReceipt> readiness_request(std::uint64_t now) noexcept {
    const std::lock_guard lock(mutex);
    if(!selectedRun || selectedRun!=mission_run_generation() || !mission_seed_armed()
        || world_phase()!=WorldPhase::arrived) { return {}; }
    return readinessSchedule.request<EnemyReceipt, 64>(selectedRun,now,
        [&](auto visit) noexcept { controller.pending_enemies(visit); });
}

void observe_readiness(const EnemyReceipt& receipt,coo::EnemyReadiness value) noexcept {
    const std::lock_guard lock(mutex);if(receipt.run!=selectedRun || receipt.run!=mission_run_generation()) { return; }
    static_cast<void>(controller.readiness(receipt,value));
}
void observe_capacity(std::uint64_t run,coo::PopulationCapacity value) noexcept {
    const std::lock_guard lock(mutex);
    if(run && run==selectedRun && run==mission_run_generation() && mission_seed_armed()
        && world_phase()==WorldPhase::arrived) controller.capacity(value);
}
Frame snapshot(std::uint64_t run,std::uint64_t now,bool ready,int region) noexcept {
    const std::lock_guard lock(mutex);
    if(run!=selectedRun || run!=mission_run_generation()) { return {}; }
    const auto frame=controller.update(run,now,ready && mission_seed_armed() && world_phase()==WorldPhase::arrived,region);
    const bool sectionChanged=frame.section!=loggedSection;
    if(sectionChanged) { loggedVolumes.reset();loggedSection=frame.section; }
    for(std::size_t i=0;i<kAllVolumes.size();++i) {
        if(controller.seen()[i] && !loggedVolumes[i]) {
            loggedVolumes.set(i);std::array<char,320> line{};const auto& v=kAllVolumes[i];
            std::snprintf(line.data(),line.size(),"ev=strike_pact stage=volume_entered run=%llu registry=%08X type=60 slot=%u name=%.*s evidence=native_player_trigger",
                static_cast<unsigned long long>(run),v.registry,v.slot,static_cast<int>(v.name.size()),v.name.data());log(line.data());
        }
    }
    const auto d=controller.diagnostics();
    if(sectionChanged || d.active!=loggedActive || d.phase!=loggedPhase || controller.timeouts()!=loggedTimeouts) {
        loggedActive=d.active;loggedPhase=d.phase;loggedTimeouts=controller.timeouts();std::array<char,320> line{};
        std::snprintf(line.data(),line.size(),"ev=coo_executor mission=strike_pact scope=mission run=%llu phase=%u active=%08X complete=%08X failure=%u dialogue_timeouts=%u section=%u cohorts=%016llX finished=%u generator_seed=%u",
            static_cast<unsigned long long>(run),static_cast<unsigned>(d.phase),d.active,d.complete,static_cast<unsigned>(d.failure),loggedTimeouts,frame.section,
            static_cast<unsigned long long>(frame.cohorts),frame.finished?1U:0U,frame.generatorSeed);log(line.data());
    }
    if(const auto* graph=controller.graph();graph && d.phase==coo::Phase::running) {
        unsigned lines{};
        for(const auto& binding:graph->commands) {
            if(controller.step_state(binding.step).phase!=coo::StepPhase::active || lines>=8) { continue; }
            const auto& spec=graph->definition.steps[binding.step].commands[binding.command];coo::StallReport report{};
            if(!stalled.observe({run,d.incarnation,binding.step,binding.command},controller.missing(spec),now,report)) { continue; }
            ++lines;std::array<char,512> line{};
            std::snprintf(line.data(),line.size(),"ev=coo_stall mission=strike_pact step=%.*s command=%.*s missing=%s registry=%08X type=%u slot=%u expected=%u actual=%u detail=%u waiting_ms=%llu",
                static_cast<int>(graph->definition.steps[binding.step].name.size()),graph->definition.steps[binding.step].name.data(),
                static_cast<int>(binding.id.size()),binding.id.data(),coo::missing_name(report.detail.missing),report.detail.asset.registry,
                report.detail.asset.type,report.detail.asset.slot,report.detail.expected,report.detail.actual,report.detail.detail,
                static_cast<unsigned long long>(report.waitingMs));log(line.data());
        }
    }
    if(frame.enabled && !frame.finished) {
        unsigned lines{};
        controller.background_diagnostics([&](std::size_t i,coo::StallDetail detail) noexcept {
            coo::StallReport report{};
            if(lines>=8 || !background.observe({run,d.incarnation,static_cast<std::uint8_t>(i/8),static_cast<std::uint8_t>(i%8)},detail,now,report)) { return; }
            ++lines;std::array<char,400> line{};const auto capacity=controller.capacity();
            std::snprintf(line.data(),line.size(),"ev=coo_readiness_stall mission=strike_pact missing=%s registry=%08X type=%u slot=%u expected=%u actual=%u actor=%08X capacity_known=%u allocated=%u maximum=%u waiting_ms=%llu",
                coo::missing_name(detail.missing),detail.asset.registry,detail.asset.type,detail.asset.slot,detail.expected,detail.actual,detail.detail,
                capacity.known?1U:0U,capacity.used,capacity.maximum,static_cast<unsigned long long>(report.waitingMs));log(line.data());
        });
    }
    publishedRevision=frame.revision;
    return frame;
}
bool publication_due(std::uint64_t now) noexcept {
    if(!mission_seed_armed() || world_phase()!=WorldPhase::arrived) { return false; }
    const std::lock_guard lock(mutex);
    if(selectedRun!=mission_run_generation() || now<nextPublication) { return false; }
    // Responsive presentation: republish at a bounded 100 ms cadence while the frame changes.
    if(controller.frame().revision==publishedRevision && !controller.frame().enabled) { return false; }
    nextPublication=now+100;
    // The graph owns host timers as well as native receipts. It must tick even when no receipt
    // changed the last frame, otherwise laser holds and the terminal countdown never advance.
    return controller.frame().enabled || controller.frame().revision!=publishedRevision;
}
std::uint64_t native_run() noexcept {
    if(!mission_seed_armed() || world_phase()!=WorldPhase::arrived) { return 0; }
    const std::lock_guard lock(mutex);return selectedRun==mission_run_generation()?selectedRun:0;
}
bool observe_enemy(const EnemyReceipt& receipt,bool death) noexcept {
    if(!mission_seed_armed() || world_phase()!=WorldPhase::arrived) { return false; }
    const std::lock_guard lock(mutex);
    if(receipt.run!=selectedRun || receipt.run!=mission_run_generation()) { return false; }
    const bool accepted=death?controller.died(receipt):controller.admitted(receipt);
    if(accepted) {
        std::array<char,256> line{};
        std::snprintf(line.data(),line.size(),"ev=strike_pact stage=%s run=%llu registry=%08X source=%u actor=%08X owner=%08X generation=%u evidence=native_%s",
            death?"death":"admission",static_cast<unsigned long long>(receipt.run),receipt.registry,receipt.source,receipt.actor,receipt.owner,receipt.generation,death?"health_death":"actor_creation");log(line.data());
    }
    return accepted;
}
bool observe_admission(const EnemyReceipt& receipt) noexcept { return observe_enemy(receipt,false); }
bool observe_death(const EnemyReceipt& receipt) noexcept { return observe_enemy(receipt,true); }
void observe_costs(std::uint32_t registry,std::uint16_t slot,const TaskCosts& report) noexcept {
    if(!mission_seed_armed() || world_phase()!=WorldPhase::arrived) { return; }
    const auto run=mission_run_generation();
    const std::lock_guard lock(mutex);
    if(run!=selectedRun) { return; }
    std::int8_t selected{-1};
    std::uint32_t known{};
    if(!controller.costed(registry,slot,report,selected,known)) { return; }
    std::array<char,320> line{};
    std::snprintf(line.data(),line.size(),"ev=strike_pact stage=task_assigned run=%llu registry=%08X type=1 slot=%u group=%d delta=%06X known=%06X evidence=native_task_costs",
        static_cast<unsigned long long>(run),registry,slot,static_cast<int>(selected),
        report.mask&0xFFFFFFU,known&0xFFFFFFU);
    log(line.data());
}
void observe_player_trigger(std::uint64_t run,std::uint32_t registry,std::uint16_t slot) noexcept {
    const std::lock_guard lock(mutex);
    if(run!=selectedRun || run!=mission_run_generation()) { return; }
    if(controller.player_trigger(run,registry,slot)) {
        std::array<char,224> line{};
        std::snprintf(line.data(),line.size(),"ev=strike_pact stage=player_trigger run=%llu registry=%08X type=31 slot=%u evidence=incident_6685",
            static_cast<unsigned long long>(run),registry,slot);log(line.data());
    }
}
void observe_scene(std::uint64_t run,std::uint32_t registry,std::uint16_t slot,
    const middleware::bap::activity_message::scene_sense::Output& report) noexcept {
    const std::lock_guard lock(mutex);
    if(run==selectedRun && run==mission_run_generation()) { controller.scene(run,registry,slot,report); }
}
void observe_squad(std::uint64_t run,std::uint32_t registry,std::uint16_t slot,
    const middleware::bap::activity_message::squad_sense::Output& report) noexcept {
    const std::lock_guard lock(mutex);
    if(run==selectedRun && run==mission_run_generation()) { controller.squad(run,registry,slot,report); }
}
void observe_generator(std::uint64_t run,std::uint32_t registry,std::uint16_t slot,
                       std::uint32_t seed,std::uint32_t completed) noexcept {
    if(run!=mission_run_generation()) { return; }
    const std::lock_guard lock(mutex);
    if(run==selectedRun) { controller.generator(run,registry,slot,seed,completed); }
}
void observe_monitor(std::uint32_t registry,std::uint16_t slot,bool any,
                     std::int32_t count,std::int32_t value) noexcept {
    if(!mission_seed_armed() || world_phase()!=WorldPhase::arrived) { return; }
    const auto run=mission_run_generation();const std::lock_guard lock(mutex);
    if(run!=selectedRun || !controller.monitor(run,registry,slot,any,count,value)) { return; }
    std::array<char,256> line{};
    std::snprintf(line.data(),line.size(),"ev=strike_pact stage=monitor_entered run=%llu registry=%08X type=30 slot=%u count=%d evidence=native_sense",
        static_cast<unsigned long long>(run),registry,slot,count);log(line.data());
}
void observe_combatant(std::uint64_t run,std::uint32_t registry,std::uint16_t slot,
    const middleware::bap::activity_message::combatant_sense::Output& report) noexcept {
    if(!mission_seed_armed() || world_phase()!=WorldPhase::arrived
        || run!=mission_run_generation()) { return; }
    const std::lock_guard lock(mutex);
    if(run!=selectedRun || !controller.combatant(run,registry,slot,report)) { return; }
    std::array<char,384> line{};
    std::snprintf(line.data(),line.size(),
        "ev=strike_pact stage=actor_feedback run=%llu registry=%08X type=2 slot=%u spawn=%u/%u program=%u/%u progress=%u/%u delivery=%u/%u delivery_state=%d detached=%u valid=%u",
        static_cast<unsigned long long>(run),registry,slot,report.spawnRevision,report.hasSpawnRevision,
        report.programRevision,report.hasProgramRevision,report.programState,report.hasProgramState,
        report.deliveryRevision,report.hasDeliveryRevision,report.deliveryState,report.detached,report.snapshotValid);
    log(line.data());
}
void observe_submission(std::uint64_t run,std::uint32_t definition,std::int64_t offset,
    std::uint32_t bank,std::uint8_t row,std::uint32_t generation) noexcept {
    if(offset!=0x1408 || row>=32 || run!=mission_run_generation() || !mission_seed_armed() || world_phase()!=WorldPhase::arrived) { return; }
    const std::lock_guard lock(mutex);
    if(run!=selectedRun) { return; }
    if(!loggedBank) {
        loggedBank=true;std::array<char,240> line{};
        std::snprintf(line.data(),line.size(),"ev=strike_pact stage=dialogue_component run=%llu definition=%08X bank=%08X evidence=native_dispatch",
            static_cast<unsigned long long>(run),definition,bank);log(line.data());
    }
    if(!controller.submitted(run,bank,row,generation,GetTickCount64())) { return; }
    std::array<char,240> line{};
    std::snprintf(line.data(),line.size(),"ev=strike_pact stage=dialogue_submitted run=%llu bank=%08X row=%u generation=%u evidence=native_dispatch",
        static_cast<unsigned long long>(run),bank,row,generation);log(line.data());
}
} // namespace sunrise::state::activity::strike_pact
