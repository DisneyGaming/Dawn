#include <Windows.h>

#include "runtime.h"
#include "../runtime.h"
#include "../../../core/logging/log.h"

#include <cstdio>
#include <atomic>
#include <filesystem>
#include <mutex>

namespace sunrise::state::activity::eater_of_worlds {
namespace {
std::mutex mutex;
std::mutex prepareMutex;
Controller controller;
std::unique_ptr<coo::script::MissionDocument> document;
std::uint64_t selectedRun{}, nextPublication{};
std::atomic<std::uint64_t> publishedRosterRun{};
std::uint32_t lastActive{UINT32_MAX}, lastComplete{UINT32_MAX};
std::uint32_t lastWaitingMechanic{};
std::uint8_t lastSection{UINT8_MAX};
bool lastRouteComplete{};
std::mutex resetReceiptsMutex;
ResetReceipts resetReceipts{};
void clear_native_receipts() noexcept {
    const std::lock_guard lock(resetReceiptsMutex);
    if (resetReceipts) { resetReceipts(); }
}

const char* mechanic_name(std::uint32_t value) noexcept {
    switch (value) {
    case 101: return "reactor.path1.finished";
    case 102: return "reactor.path2.finished";
    case 103: return "reactor.path3.finished";
    case 104: return "reactor.path4.finished";
    case 110: return "barrier.cycle.finished";
    case 111: return "argos.cycle.finished";
    default: return "unknown";
    }
}
const char* missing_binding(std::uint32_t value) noexcept {
    if (value >= 101 && value <= 104) return "native_platform_occupancy_and_completion";
    if (value == 110) return "native_barrier_cycle_receipts";
    if (value == 111) return "native_argos_cycle_receipts";
    return "unsupported_native_mechanic";
}

bool current() noexcept {
    return selectedRun && selectedRun == mission_run_generation() && mission_seed_armed()
           && world_phase() == WorldPhase::arrived;
}
void log(std::string_view text) noexcept {
    core::log::write(core::log::Channel::server, core::log::Level::info, text);
}
bool load() noexcept {
    static std::once_flag once;
    std::call_once(once, [] {
        std::string error;
        try {
            HMODULE module{};
            std::array<wchar_t, 32768> path{};
            const bool found = GetModuleHandleExW(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(&load), &module) != FALSE;
            const auto size = found
                ? GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size())) : 0;
            if (!size || size >= path.size()) {
                error = "cannot resolve DLL-relative script path";
            } else {
                document = coo::script::MissionDocument::read(
                    std::filesystem::path(path.data()).parent_path() / L"Sunrise" / L"scripts"
                        / L"eater_of_worlds.lua",
                    kProfile, error);
            }
            if (document && !valid_document(document->views())) {
                document.reset();
                error = "Eater of Worlds native profile mismatch";
            }
        } catch (const std::exception& e) {
            error = e.what();
        }
        std::array<char, 768> line{};
        if (document) {
            std::snprintf(line.data(), line.size(),
                "ev=coo_script mission=eater_of_worlds result=loaded format=lua fnv1a64=%016llX path=Sunrise/scripts/eater_of_worlds.lua reload=next_process",
                static_cast<unsigned long long>(document->fingerprint()));
        } else {
            std::snprintf(line.data(), line.size(),
                "ev=coo_script mission=eater_of_worlds result=failed reason=\"%.*s\"",
                static_cast<int>((std::min)(error.size(), std::size_t{500})), error.data());
        }
        log(line.data());
    });
    return document != nullptr;
}
void log_receipt(const char* stage, std::uint64_t run, std::uint32_t value) noexcept {
    std::array<char, 192> line{};
    std::snprintf(line.data(), line.size(), "ev=eater_of_worlds stage=%s run=%llu value=%u",
        stage, static_cast<unsigned long long>(run), value);
    log(line.data());
}
} // namespace

void set_reset_receipts(ResetReceipts callback) noexcept {
    const std::lock_guard lock(resetReceiptsMutex);
    resetReceipts = callback;
}

bool prepare(std::uint64_t run, bool selected) noexcept {
    const std::lock_guard prepareLock(prepareMutex);
    if (!selected) {
        bool retired{};
        {
            const std::lock_guard lock(mutex);
            if (run == mission_run_generation()) {
                publishedRosterRun.store(0, std::memory_order_release);
                controller.reset();
                selectedRun = nextPublication = 0;
                lastActive = lastComplete = UINT32_MAX;
                lastWaitingMechanic = 0;
                lastSection = UINT8_MAX;
                lastRouteComplete = false;
                retired = true;
            }
        }
        if (retired) { clear_native_receipts(); }
        return false;
    }
    bool reset{};
    {
        const std::lock_guard lock(mutex);
        if (run != mission_run_generation()) { return false; }
        if (selectedRun && selectedRun != run) {
            publishedRosterRun.store(0, std::memory_order_release);
            controller.reset();
            selectedRun = nextPublication = 0;
            lastActive = lastComplete = UINT32_MAX;
            lastWaitingMechanic = 0;
            lastSection = UINT8_MAX;
            lastRouteComplete = false;
            reset = true;
        }
    }
    if (reset) { clear_native_receipts(); }
    const std::lock_guard lock(mutex);
    if (run != mission_run_generation() || !load() || !controller.select(document->views(), run)) { return false; }
    if (selectedRun != run) {
        lastActive = lastComplete = UINT32_MAX;
        lastWaitingMechanic = 0;
        lastSection = UINT8_MAX;
        lastRouteComplete = false;
    }
    selectedRun = run;
    publishedRosterRun.store(run, std::memory_order_release);
    return true;
}

coo::ReadinessRequest<EnemyReceipt> readiness_request(std::uint64_t now) noexcept {
    const std::lock_guard lock(mutex);
    return current() ? controller.readiness_request(now) : coo::ReadinessRequest<EnemyReceipt>{};
}
Frame snapshot(std::uint64_t run, std::uint64_t now, bool ready, int region) noexcept {
    const std::lock_guard lock(mutex);
    if (!current() || run != selectedRun) { return {}; }
    const auto frame = controller.update(run, now, ready, region);
    nextPublication = now + 100;
    const auto diagnostics = controller.diagnostics();
    if(frame.routeComplete && !lastRouteComplete) {
        lastRouteComplete=true;
        log_receipt("barrier_arena_arrived",run,frame.section);
    }
    if (frame.waitingMechanic != lastWaitingMechanic) {
        lastWaitingMechanic = frame.waitingMechanic;
        if (frame.waitingMechanic != 0) {
            std::array<char, 320> line{};
            std::snprintf(line.data(), line.size(),
                "ev=eater_of_worlds stage=mechanic_wait run=%llu section=%u operation=%s mechanic=%u result=pending reason=%s binding=%s",
                static_cast<unsigned long long>(run), frame.section,
                mechanic_name(frame.waitingMechanic), frame.waitingMechanic,
                frame.waitingMechanic>=101 && frame.waitingMechanic<=104?"awaiting_ordered_platform_contacts":"unsupported_native_binding",
                missing_binding(frame.waitingMechanic));
            log(line.data());
        }
    }
    if (diagnostics.active != lastActive || diagnostics.complete != lastComplete
        || frame.section != lastSection) {
        lastActive = diagnostics.active;
        lastComplete = diagnostics.complete;
        lastSection = frame.section;
        std::array<char, 320> line{};
        std::snprintf(line.data(), line.size(),
            "ev=coo_executor mission=eater_of_worlds run=%llu generation=%u section=%u phase=%u active=%08X complete=%08X failure=%u mission_finished=%u",
            static_cast<unsigned long long>(run), frame.spawnGeneration, frame.section,
            static_cast<unsigned>(diagnostics.phase), diagnostics.active, diagnostics.complete,
            static_cast<unsigned>(diagnostics.failure), frame.finished ? 1U : 0U);
        log(line.data());
    }
    return frame;
}
Request request() noexcept {
    const std::lock_guard lock(mutex);
    return current() ? Request{controller.owner(), controller.frame()} : Request{};
}
ObjectRequest object_request(std::size_t assetIndex) noexcept {
    const std::lock_guard lock(mutex);
    return current()?eater_of_worlds::object_request(controller.frame(),controller.owner(),assetIndex)
        : ObjectRequest{};
}
GateRequest gate_request(coo::Asset gate) noexcept {
    const std::lock_guard lock(mutex);
    return current()?eater_of_worlds::gate_request(controller.frame(),controller.owner(),gate)
        : GateRequest{};
}
GrateRequest grate_request() noexcept {
    const std::lock_guard lock(mutex);
    return current()?eater_of_worlds::grate_request(controller.frame(),controller.owner())
        : GrateRequest{};
}
SourceRequest source_request(std::uint32_t registry,std::uint16_t slot) noexcept {
    const std::lock_guard lock(mutex);
    return current()?eater_of_worlds::source_request(controller.frame(),controller.owner(),registry,slot)
        : SourceRequest{};
}
ContactRequest contact_request() noexcept {
    const std::lock_guard lock(mutex);
    return current()?eater_of_worlds::contact_request(controller.frame(),controller.owner())
        : ContactRequest{};
}
ArrivalRequest arrival_request() noexcept {
    const std::lock_guard lock(mutex);
    return current()?eater_of_worlds::arrival_request(controller.frame(),controller.owner()):ArrivalRequest{};
}
bool observe_arrival_motion(const ArrivalMotion& motion) noexcept {
    const std::lock_guard lock(mutex);
    const bool accepted=current() && controller.arrival_motion(motion);
    if(accepted) log_receipt(controller.frame().arrival.landed?"arena_arrival_trigger":"hoop_crossed",motion.run,motion.attempt);
    return accepted;
}
bool owner_current(coo::Generation owner) noexcept {
    const std::lock_guard lock(mutex);
    return current() && controller.frame().enabled && controller.owner()==owner;
}
std::uint64_t native_run() noexcept {
    const std::lock_guard lock(mutex);
    return current() ? selectedRun : 0;
}
std::uint64_t native_roster_run() noexcept {
    const auto run = publishedRosterRun.load(std::memory_order_acquire);
    return run && run == mission_run_generation() && mission_seed_armed()
        && world_phase() == WorldPhase::arrived ? run : 0;
}
bool publication_due(std::uint64_t now) noexcept {
    const std::lock_guard lock(mutex);
    return current() && controller.frame().enabled && now >= nextPublication && controller.due(now);
}
bool due(std::uint64_t now) noexcept {
    const std::lock_guard lock(mutex);
    return current() && controller.due(now);
}
void observe_position(float x, float y, float z) noexcept {
    const std::lock_guard lock(mutex);
    if (current()) { controller.position(selectedRun, {x, y, z}); }
}
void observe_prepared(coo::Generation owner, coo::Asset asset) noexcept {
    const std::lock_guard lock(mutex);
    if (current()) { static_cast<void>(controller.prepared(owner, asset)); }
}
void observe_object(const coo::ObjectReceipt& receipt) noexcept {
    const std::lock_guard lock(mutex);
    if (current()) { static_cast<void>(controller.object(receipt)); }
}
bool observe_grate_pose(const GratePoseReceipt& receipt) noexcept {
    const std::lock_guard lock(mutex);
    const bool accepted=current() && controller.grate_pose(receipt);
    if(accepted) log_receipt("reactor_exit_grate_pose",receipt.object.owner.run,
        static_cast<std::uint32_t>(receipt.revision));
    return accepted;
}
bool observe_platform_pose(const PlatformPoseReceipt& receipt) noexcept {
    const std::lock_guard lock(mutex);
    const bool accepted=current() && controller.platform_pose(receipt);
    if(accepted) log_receipt("platform_pose_applied",receipt.object.owner.run,receipt.object.source.slot);
    return accepted;
}
bool observe_platform_contact(const PlatformContact& receipt) noexcept {
    const std::lock_guard lock(mutex);
    const bool accepted=current() && controller.platform_contact(receipt);
    if(accepted) {
        const auto& reactor=controller.frame().reactor;std::array<char,256> line{};
        std::snprintf(line.data(),line.size(),
            "ev=eater_of_worlds stage=platform_activated run=%llu path=%u activated=%u total=%u attempt=%u source=%08X slot=%u player=%08X",
            static_cast<unsigned long long>(receipt.object.owner.run),reactor.path,reactor.next,
            kPathLengths[reactor.path-1],reactor.attempt,receipt.object.source.definition,receipt.object.source.slot,receipt.player);
        log(line.data());
    }
    return accepted;
}
void observe_player(std::uint32_t player) noexcept {
    const std::lock_guard lock(mutex);
    if(current()) controller.player(selectedRun,player);
}
void observe_player_health(std::uint64_t run,std::uint32_t player,bool dead) noexcept {
    const std::lock_guard lock(mutex);
    if(current() && controller.player_health(run,player,dead))
        log_receipt(dead?"reactor_player_died":"reactor_player_alive",run,player);
}
bool observe_health(const HealthReceipt& receipt, bool dead) noexcept {
    const std::lock_guard lock(mutex);
    const bool accepted = current() && controller.health(receipt, dead);
    if (accepted) { log_receipt(dead ? "object_health_dead" : "object_health_alive", receipt.generation.run, receipt.source.definition); }
    return accepted;
}
bool observe_cranium(const CraniumReceipt& receipt, bool held, std::uint32_t player) noexcept {
    const std::lock_guard lock(mutex);
    const bool accepted = current() && controller.cranium(receipt, held, player);
    if (accepted) { log_receipt(held ? "cranium_carried" : "cranium_dropped", receipt.generation.run, receipt.source.definition); }
    return accepted;
}
bool observe_station(const StationReceipt& receipt, std::uint32_t player) noexcept {
    const std::lock_guard lock(mutex);
    const bool accepted = current() && controller.station(receipt, player);
    if (accepted) {
        std::array<char, 320> line{};
        std::snprintf(line.data(), line.size(),
            "ev=eater_of_worlds stage=station_used run=%llu registry=%08X slot=%u source=%08X player=%08X cranium_registry=%08X cranium_slot=%u cranium_source=%08X cranium_component=%08X",
            static_cast<unsigned long long>(receipt.generation.run), receipt.source.registry,
            static_cast<unsigned>(receipt.source.slot), receipt.source.definition, player,
            receipt.cranium.source.registry, static_cast<unsigned>(receipt.cranium.source.slot),
            receipt.cranium.source.definition, receipt.cranium.component);
        log(line.data());
    }
    return accepted;
}
void observe_device(coo::Generation owner, coo::Asset asset, std::int16_t revision,
                    float position) noexcept {
    const std::lock_guard lock(mutex);
    if (current()) { static_cast<void>(controller.device(owner, asset, revision, position)); }
}
bool observe_admission(const EnemyReceipt& receipt) noexcept {
    const std::lock_guard lock(mutex);
    return current() && controller.admitted(receipt);
}
bool observe_death(const EnemyReceipt& receipt) noexcept {
    const std::lock_guard lock(mutex);
    const bool accepted = current() && controller.died(receipt);
    if (accepted) { log_receipt("enemy_died", receipt.run, receipt.source); }
    return accepted;
}
bool observe_source_retired(std::uint64_t run,std::uint32_t key,std::uint16_t slot,std::uint32_t generation) noexcept {
    const std::lock_guard lock(mutex);
    const bool accepted=current() && controller.source_retired(run,key,slot,generation);
    if(accepted) log_receipt("source_retired",run,slot);
    return accepted;
}
void observe_readiness(const EnemyReceipt& receipt, coo::EnemyReadiness value) noexcept {
    const std::lock_guard lock(mutex);
    if (current()) { static_cast<void>(controller.readiness(receipt, value)); }
}
void observe_costs(std::uint32_t key, std::uint16_t slot, const coo::TaskCosts& costs) noexcept {
    const std::lock_guard lock(mutex);
    if (current()) { controller.costed(key, slot, costs); }
}
void observe_player_trigger(std::uint64_t run, std::uint32_t key, std::uint16_t slot) noexcept {
    const std::lock_guard lock(mutex);
    if (current()) { static_cast<void>(controller.trigger(run, key, slot)); }
}
void observe_monitor(std::uint32_t key, std::uint16_t slot, bool any,
                     std::int32_t count, std::int32_t value) noexcept {
    const std::lock_guard lock(mutex);
    if (current()) { controller.monitor(key, slot, any, count, value); }
}
void observe_combatant(std::uint64_t run, std::uint32_t key, std::uint16_t slot,
                       const middleware::bap::activity_message::combatant_sense::Output& output) noexcept {
    const std::lock_guard lock(mutex);
    if (current()) { controller.combatant(run, key, slot, output); }
}
void observe_scene(std::uint64_t run, std::uint32_t key, std::uint16_t slot,
                   const middleware::bap::activity_message::scene_sense::Output& output) noexcept {
    const std::lock_guard lock(mutex);
    if (current()) { controller.scene(run, key, slot, output); }
}
void observe_squad(std::uint64_t run, std::uint32_t key, std::uint16_t slot,
                   const middleware::bap::activity_message::squad_sense::Output& output) noexcept {
    const std::lock_guard lock(mutex);
    if (current()) { controller.squad(run, key, slot, output); }
}
void observe_submission(std::uint64_t run, std::uint32_t tag, std::int64_t offset,
                        std::uint32_t bank, std::uint8_t row, std::uint32_t generation) noexcept {
    const std::lock_guard lock(mutex);
    if (current() && controller.submitted(run, tag, offset, bank, row, generation)) {
        log_receipt("dialogue_submitted", run, row);
    }
}
void observe_capacity(std::uint64_t run, coo::PopulationCapacity capacity) noexcept {
    const std::lock_guard lock(mutex);
    if (current()) { controller.capacity(run, capacity); }
}

} // namespace sunrise::state::activity::eater_of_worlds
