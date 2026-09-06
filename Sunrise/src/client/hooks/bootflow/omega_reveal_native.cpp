#include <Windows.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string_view>

#include "omega_reveal_native.h"
#include "omega_native_readable.h"
#include "omega_presentation.h"
#include "omega_reveal_bindings.h"
#include "omega_directive_native.h"
#include "omega_boss_spawn.h"
#include "omega_boss_graph.h"
#include "omega_boss_graph_observation.h"
#include "omega_boss_combat_start.h"
#include "omega_boss_eye_diagnostics.h"
#include "omega_boss_vfx_start.h"
#include "omega_lair_delivery.h"
#include "omega_mission_characters.h"
#include "omega_mission_delivery.h"
#include "omega_rescue_delivery.h"
#include "omega_cannon_delivery.h"
#include "omega_mission_motion.h"
#include "omega_mission_crown.h"
#include "../../../state/activity/omega/omega_rescue_authority.h"
#include "../../../state/activity/omega/omega_transit_authority.h"
#include "../../../state/activity/omega/omega_mission_devices.h"
#include "../../../state/activity/omega/omega_mission_runtime.h"
#include "../../../state/activity/omega/omega_ending_runtime.h"
#include "../graphics/omega_frame_timing.h"
#include "../../../core/logging/log.h"
#include "../../../state/activity/forced/activity_forced_destination.h"
#include "../../../state/activity/runtime.h"
#include "../../../state/activity/omega/omega_lair_start.h"
#include "../../hooking/detour.h"
#include "../../hooking/call_gate.h"

namespace sunrise::client::hooks::bootflow {
namespace omega_reveal_native {
namespace {
namespace omega = state::activity::omega;
namespace presentation = omega_presentation;
namespace graph = omega_boss_graph;
namespace observation = omega_boss_graph_observation;
namespace combat = omega_boss_combat_start;
namespace motion = omega_mission_motion;
namespace crown = omega_mission_crown;
namespace frame_timing = graphics::omega_frame_timing;
using Tick = void(__fastcall*)(std::byte*) noexcept;
using CharacterInitialize = void(__fastcall*)(std::byte*, const std::uint32_t*) noexcept;
using GraphUpdate = bool(__fastcall*)(float, const void*, std::byte*, bool*) noexcept;
using FullBodyUpdate = bool(__fastcall*)(std::byte*, const void*, float, std::uint8_t, const void*, void*) noexcept;
std::atomic<CharacterInitialize> characterOriginal{};
using CharacterDeath = bool(__fastcall*)(std::byte*,std::uint32_t) noexcept;
std::atomic<CharacterDeath> deathOriginal{};
using MotionUpdate=bool(__fastcall*)(void*,const void*,std::byte*) noexcept;
using MotionCleanup=void(__fastcall*)(void*,std::byte*,void*) noexcept;
std::atomic<MotionUpdate> motionUpdateOriginal{};
std::atomic<MotionCleanup> motionCleanupOriginal{};
using ArcCarry=void(__fastcall*)(std::byte*,std::uint8_t,const void*) noexcept;
std::atomic<ArcCarry> arcCarryOriginal{};
std::atomic<Tick> arcUseOriginal{};
std::atomic<GraphUpdate> graphOriginal{};
std::atomic<FullBodyUpdate> fullBodyOriginal{};
// One-shot initialization must not lose a binding when a diagnostics lock is busy.
std::atomic<std::uint64_t> observedCharacter{UINT64_MAX};
std::atomic<Tick> spawnerOriginal{}, cinematicOriginal{}, memberOriginal{}, sceneSenseOriginal{};
std::array<hooking::detour::Handle, 21> handles{};
hooking::CallGate callGate;
std::atomic_bool ready{};
std::byte* image{};
std::mutex mutex;
struct RunState {
    std::uint64_t run{UINT64_MAX};
    bool bossSeen{}, cinematicStarted{}, cinematicFinished{}, cinematicObserved{};
    unsigned cinematicAttempts{};
    std::uint64_t nextCinematic{};
    std::uint64_t cinematicCompletedAt{};
    graph::Owner graphOwner{};
    graph::EventLease summonLease{};
    bool queueClaimed{}, queueAccepted{}, graphRetired{}, flightSeen{}, summonSeen{}, introIdleSeen{};
    observation::Phase graphPhase{};
    unsigned memberWaits{}, graphRejectSamples{};
    std::uint64_t ownerWaits{};
    combat::Track left{};
    omega::mission::Token armToken{};
    motion::Track departure{};
    crown::Track crown{};
    crown::HealthTrack health{};
    const void* directive{};
    omega::Route route{};
    std::uint32_t area{UINT32_MAX};
    std::uint32_t viewer{UINT32_MAX};
    bool markerInstalled{};
    bool cinematicMissingLogged{};
    const void* bossComponent{};
    omega::Route bossRoute{};
    bool bossPendingLogged{}, bossQueueWaitLogged{};
    std::uint32_t bossArea{UINT32_MAX};
    const void* cinematicComponent{};
    omega::Route cinematicRoute{};
    std::uint32_t cinematicArea{UINT32_MAX};
    std::array<std::uint64_t, 2> ticks{}, matched{};
    struct Source { std::uint32_t tag{}, kind{}; std::int64_t offset{}; };
    std::array<std::array<Source, 32>, 2> sources{};
    std::array<unsigned, 2> sourceCount{};
    std::uint64_t nextBindingSample{};
    unsigned bindingSamples{};
    std::array<bool, 6> lairDeliveryClaimed{};
    std::array<bool, omega::mission::kSources.size()> missionDeliveryClaimed{};
    std::array<bool, omega::rescue::sources.size()> rescueDeliveryClaimed{};
    std::array<omega_cannon_delivery::Reference, omega::rescue::sources.size()> rescueReferences{};
} runState;

template<class Fn> Fn native(std::uintptr_t rva) noexcept { return reinterpret_cast<Fn>(image + rva); }
template<class T> T read(const std::byte* component, std::size_t offset) noexcept {
    T value{}; std::memcpy(&value, component + offset, sizeof value); return value;
}
bool readable(const void* pointer, std::size_t size) noexcept {
    return omega_native_memory::readable(pointer, size);
}
template<class... Args> void log(const char* format, Args... args) noexcept {
    std::array<char, 512> line{};
    const int n = std::snprintf(line.data(), line.size(), format, args...);
    if (n > 0 && static_cast<std::size_t>(n) < line.size())
        core::log::write(core::log::Channel::client, core::log::Level::info,
                        {line.data(), static_cast<std::size_t>(n)});
}
bool active() noexcept {
    const frame_timing::PostSpan workTiming(frame_timing::Kind::scope_check);
    if (!ready.load(std::memory_order_acquire)
        || state::activity::world_phase() != state::activity::WorldPhase::arrived
        || !state::activity::forced::override_active()) return false;
    state::activity::forced::ForcedDestination destination{};
    state::activity::forced::snapshot(destination);
    return destination.packageNameLength <= destination.packageName.size()
        && std::string_view(destination.packageName.data(), destination.packageNameLength) == "mission_scot";
}
void reset(std::uint64_t run) noexcept {
    if (runState.run != run) { frame_timing::clear_scope(); runState = {}; runState.run = run; }
}
// Observe before the exact identity filter. Separate finite budgets prevent
// Forest spawners from consuming the cinematic or exact-component receipts.
void observe_tick(bool boss, std::byte* component, bool fullReadable, bool matches) noexcept {
    const auto run = state::activity::mission_run_generation();
    const auto progress = omega::presentation_progress(run);
    const std::lock_guard lock(mutex);
    reset(run);
    const unsigned slot = boss ? 0U : 1U;
    if (callGate.accepting() && !boss && matches && progress.loadedBubble == 14 && progress.bossDoorReached)
        frame_timing::set_scope(run, static_cast<std::uint32_t>(runState.graphPhase));
    ++runState.ticks[slot];
    if (matches) ++runState.matched[slot];
    if (!readable(component, 16)) return;
    const RunState::Source source{read<std::uint32_t>(component, 0),
        read<std::uint32_t>(component, 4), read<std::int64_t>(component, 8)};
    auto& count = runState.sourceCount[slot];
    for (unsigned i = 0; i < count; ++i) {
        const auto& seen = runState.sources[slot][i];
        if (seen.tag == source.tag && seen.kind == source.kind && seen.offset == source.offset) return;
    }
    if (count == runState.sources[slot].size()) return;
    runState.sources[slot][count++] = source;
    log("ev=omega_reveal stage=callback_source run=%llu owner=%s source=%08X/%08X/%llX "
        "component=%p readable=%u matches=%u route=%u area=%u sample=%u/32 mutation=observe_only",
        run, boss ? "boss" : "intro", source.tag, source.kind,
        static_cast<unsigned long long>(source.offset), static_cast<void*>(component),
        fullReadable ? 1U : 0U, matches ? 1U : 0U, static_cast<unsigned>(progress.route),
        progress.loadedBubble, count);
}

void deliver_lair_source(std::byte*, const hooking::CallGate::Scope&) noexcept;
void observe_mission_source(std::byte*) noexcept;
void deliver_mission_source(std::byte*,const hooking::CallGate::Scope&) noexcept;
void deliver_rescue_source(std::byte*,const hooking::CallGate::Scope&) noexcept;
void observe_mission_character(std::byte*,const std::uint32_t*) noexcept;
void __fastcall spawner_tick(std::byte* component) noexcept {
    const hooking::CallGate::Scope call(callGate);
    hooking::await_original(spawnerOriginal)(component);
    if (!call.accepts_side_effects() || !active()) return;
    if(omega::mission::runtime::snapshot(state::activity::mission_run_generation()).generation) deliver_mission_source(component,call);
    else deliver_lair_source(component, call);
    observe_mission_source(component);
    deliver_rescue_source(component,call);
    const bool fullReadable = readable(component, omega_boss_spawn::kComponentBytes);
    const bool matches = fullReadable && omega_boss_spawn::matches({component, omega_boss_spawn::kComponentBytes});
    observe_tick(true, component, fullReadable, matches);
    // The authority body activates the authored type-2 member. The parent
    // remains observable, but never receives another loose-actor spawn request.
}

void observe_eye_inputs(const graph::Owner&, const observation::Snapshot&) noexcept;
bool prepare_intro_vfx(const graph::Owner&, const hooking::CallGate::Scope&) noexcept;
void observe_mission_crown(std::span<const std::byte>,bool,const hooking::CallGate::Scope&) noexcept;
void observe_mission_health(const hooking::CallGate::Scope&) noexcept;
void observe_mission_boss_death(std::byte*,std::uint32_t) noexcept;
bool mission_source_identity(std::byte*,std::span<const std::byte>,std::size_t,std::uint32_t,omega_cannon_delivery::Reference&) noexcept;
void observe_eye_clip_cursor(const void*,std::span<const std::byte>,const hooking::CallGate::Scope&) noexcept;
#include "omega_boss_graph_runtime.inl"
#include "omega_eye_clip_trace.inl"
#include "omega_mission_receipts.inl"
bool mission_crown_queue(const graph::Owner&,const MemberView&,unsigned) noexcept;
#include "omega_mission_motion.inl"
#include "omega_mission_crown.inl"
#include "omega_mission_health.inl"
#include "omega_eye_resource_trace.inl"
#include "omega_eye_execution_trace.inl"
#include "omega_mission_rescue.inl"
#include "omega_mission_arc.inl"
#include "omega_boss_combat_runtime.inl"
#include "omega_boss_eye_diagnostics.inl"
#include "omega_boss_vfx_start.inl"
#include "omega_lair_delivery.inl"
#include "omega_mission_delivery.inl"
#include "omega_rescue_delivery.inl"
#include "omega_cannon_delivery.inl"

#include "omega_mission_ending.inl"
#include "omega_cinematic_runtime.inl"


} // namespace

void observe_mission_position(std::uint32_t player,const float* xyz) noexcept {
    const hooking::CallGate::Scope call(callGate);
    if(!call.accepts_side_effects() || !active() || player==UINT32_MAX || !xyz) return;
    const auto snapshot=omega::mission::runtime::snapshot(state::activity::mission_run_generation());
    const bool eyeTransit=snapshot.phase==omega::mission::Phase::shield && snapshot.chargeDunked && !snapshot.eyePlatform;
    if(!snapshot.generation || (!snapshot.command.claimed && !eyeTransit)) return;
    struct Volume {std::uint32_t asset,registry;std::size_t size,root;std::uint16_t slot;std::uint8_t vertices,triangles,cycle,milestone;};
    // milestone 0..4 = chase/final arrival, 5 = charge platform, 6 = eye return.
    constexpr std::array<Volume,10> volumes{{
        {0x80F47913,0xF4D0E0B2,0x20E2,0xD90,57,10,8,0,0},
        {0x80F47913,0xF4D0E0B2,0x20E2,0xFD0,59,5,3,0,1},
        {0x80F47913,0xF4D0E0B2,0x20E2,0x1210,61,4,2,0,2},
        {0x80F47913,0xF4D0E0B2,0x20E2,0xB50,55,6,4,0,3},
        {0x80F47845,0x0040BF03,5650,0xAD0,60,12,10,2,4},
        {0x80F47639,0x0040BF06,6035,0xAF0,69,4,2,1,5},
        {0x80F47712,0x0040BF05,6035,0xC10,80,4,2,2,5},
        {0x80F47639,0x0040BF06,6035,0xD30,81,4,2,1,6},
        {0x80F47712,0x0040BF05,6035,0xAF0,76,4,2,2,6},
        {0x80F47845,0x0040BF03,5650,0xBF0,73,4,2,3,6}}};
    for(const auto& volume:volumes) {
        if(volume.milestone<5) {
            if(snapshot.command.island!=volume.milestone
                || (snapshot.phase!=omega::mission::Phase::departure && snapshot.phase!=omega::mission::Phase::arrival)) continue;
        } else if(snapshot.command.cycle!=volume.cycle
            || (volume.milestone==6?!eyeTransit:
                (snapshot.phase!=omega::mission::Phase::route && snapshot.phase!=omega::mission::Phase::carrying))
            || (volume.milestone==5?snapshot.chargePlatform:snapshot.eyePlatform)) continue;
        auto* asset=resolve_handle(volume.asset);std::array<std::byte,0x2200> storage{};
        if(volume.size>storage.size() || !copy_native(asset,storage.data(),volume.size)) continue;
        const auto bytes=std::span(storage).first(volume.size);
        const auto root=volume.root,shape=root+0xB0;
        if(read<std::uint32_t>(bytes.data(),root+0xC)!=volume.registry
            || read<std::uint8_t>(bytes.data(),root+0x10)!=60 || read<std::uint16_t>(bytes.data(),root+0x12)!=volume.slot
            || read<std::uint64_t>(bytes.data(),shape+0x20)!=volume.vertices
            || read<std::uint64_t>(bytes.data(),shape+0x30)!=volume.triangles) continue;
        const auto vr=read<std::uint64_t>(bytes.data(),shape+0x28),tr=read<std::uint64_t>(bytes.data(),shape+0x38);
        if(vr>bytes.size() || tr>bytes.size()) continue;
        const auto v=shape+0x38+static_cast<std::size_t>(vr),t=shape+0x48+static_cast<std::size_t>(tr);
        if(v>bytes.size() || volume.vertices*16U>bytes.size()-v || t>bytes.size() || volume.triangles*3U>bytes.size()-t) continue;
        bool indices=true;
        for(std::size_t i=0;i<volume.triangles*3U;++i) indices&=read<std::uint8_t>(bytes.data(),t+i)<volume.vertices;
        if(!indices) continue;
        alignas(16) motion::Point position{xyz[0],xyz[1],xyz[2],1.F};
        if(!motion::finite(position)) return;
        alignas(16) std::array<std::uint32_t,4> result{};
        native<void*(__fastcall*)(void*,const void*,const void*) noexcept>(0x4A55A0)(result.data(),asset+shape,position.data());
        if(!std::all_of(result.begin(),result.end(),[](auto lane){return lane==UINT32_MAX;})) continue;
        const bool accepted=omega::mission::runtime::receipt([&](auto& s){
            return volume.milestone<5?s.arrival(snapshot.command.token,static_cast<std::uint8_t>(volume.milestone+1),player)
                :s.route_arrival(snapshot.command.token,volume.milestone==6,player);
        });
        if(accepted) log("ev=omega_mission stage=arrival run=%llu epoch=%u milestone=%u player=%08X volume=%08X/60/%u receipt=native_polygon",
            snapshot.command.token.boss.run,snapshot.command.token.epoch,volume.milestone,player,volume.registry,volume.slot);
    }
}

#include "omega_mission_devices.inl"

bool cinematic_completion(std::uint64_t run, std::uint64_t& completedAt) noexcept {
    const std::lock_guard lock(mutex);
    if (runState.run != run || !runState.cinematicFinished) return false;
    completedAt = runState.cinematicCompletedAt;
    return true;
}

bool begin_binding_sample() noexcept {
    if (!active()) return false;
    const auto run = state::activity::mission_run_generation();
    const auto progress = omega::presentation_progress(run);
    if (progress.loadedBubble != 14) return false;
    const auto now = GetTickCount64();
    const std::lock_guard lock(mutex);
    reset(run);
    if (now < runState.nextBindingSample || runState.bindingSamples >= 60) return false;
    // Reserve most samples for the reveal even if the player pauses in the
    // loaded Lair approach. Exhausting approach diagnostics must not hide it.
    if (progress.route != omega::Route::reveal && runState.bindingSamples >= 5) return false;
    runState.nextBindingSample = now + 2000;
    ++runState.bindingSamples;
    log("ev=omega_reveal stage=boundary run=%llu route=%u area=%u eligible=%u "
        "boss_ticks=%llu boss_matches=%llu intro_ticks=%llu intro_matches=%llu "
        "boss_seen=%u intro_started=%u sample=%u/60 mutation=observe_only", run,
        static_cast<unsigned>(progress.route), progress.loadedBubble,
        omega_boss_spawn::eligible(true, progress) ? 1U : 0U,
        runState.ticks[0], runState.matched[0], runState.ticks[1], runState.matched[1],
        runState.bossSeen ? 1U : 0U, runState.cinematicStarted ? 1U : 0U, runState.bindingSamples);
    return true;
}

void observe_binding(bool boss, bool found, std::uint32_t handle,
                     std::uint32_t schema, std::uint32_t componentKind,
                     std::uint32_t bubble, std::uint8_t pending, std::uint8_t applied,
                     const std::byte* runtime, std::span<const std::byte> copiedHeader) noexcept {
    if (!active()) return;
    const bool headerReadable = copiedHeader.size() >= 16;
    const auto* header = copiedHeader.data();
    log("ev=omega_reveal stage=roster_binding run=%llu slot=%s found=%u object=%08X "
        "schema=%08X body_kind=%08X bubble=%u pending=%u applied=%u body=%p readable=%u "
        "decoded_prefix=%08X/%08X/%llX mutation=observe_only",
        state::activity::mission_run_generation(), boss ? "95FB2E01/1/0" : "F4D0E0B2/6/22",
        found ? 1U : 0U, handle, schema, componentKind, bubble,
        static_cast<unsigned>(pending), static_cast<unsigned>(applied), static_cast<const void*>(runtime),
        headerReadable ? 1U : 0U, headerReadable ? read<std::uint32_t>(header, 0) : 0U,
        headerReadable ? read<std::uint32_t>(header, 4) : 0U,
        headerReadable ? static_cast<unsigned long long>(read<std::int64_t>(header, 8)) : 0ULL);
}

void update_directive(std::byte* component) noexcept {
    const frame_timing::PostSpan workTiming(frame_timing::Kind::directive);
    const hooking::CallGate::Scope call(callGate);
    if (!call.accepts_side_effects()) return;
    if (!active() || !readable(component, 0xB08)
        || read<std::uint32_t>(component, 0) != 0x80F47BD4U
        || read<std::int64_t>(component, 8) != 0xB88) return;
    pump_cannon_delivery(call);
    const auto run = state::activity::mission_run_generation();
    using Source = void*(__fastcall*)();
    using Context = const std::uint32_t*(__fastcall*)(void*, std::uint32_t*);
    std::uint32_t area = UINT32_MAX, destination = 0x811C9DC5U, viewer = UINT32_MAX;
    const auto* areaResult = native<Context>(0x429BA0)(native<Source>(0x4294C0)(), &area);
    if (areaResult) area = *areaResult;
    native<void*(__fastcall*)(std::uint32_t*)>(0xAE0040)(&destination);
    native<void*(__fastcall*)(std::uint32_t*)>(0x4FFBB0)(&viewer);
    omega::note_native_area(run, area, destination);
    const auto progress = omega::presentation_progress(run);
    if (progress.lairObjectiveEvent != 0) return;
    const auto target = omega::waypoint(progress.route);
    if (!target.registry || read<std::uint32_t>(component, 0x190) != 0x1EBF4621U
        || read<std::uint32_t>(component, 0x194) != 0
        || read<std::int8_t>(component, 0x198) != 0) return;
    const std::lock_guard lock(mutex);
    reset(run);
    const bool replace = !runState.markerInstalled || runState.directive != component
        || runState.route != progress.route || runState.area != area || runState.viewer != viewer;
    using Refresh = void(__fastcall*)(std::byte*, std::uint32_t);
    const auto refresh = native<Refresh>(0x100A6E0);
    // +100ACFF synthesizes a portal beacon when the supplied context differs from
    // the objective viewer. Forced teleportation can leave those contexts different.
    // Bind this mission's checkpoint to its actual viewer; its full locator still
    // names the target area. Never change the game's shared region/viewer state.
    const std::uint32_t presentationContext = viewer != UINT32_MAX ? viewer : area;
    if (replace && !native<bool(__fastcall*)()>(0x137E1D0)()) return;
    if (replace && !omega_directive_native::available()) return;
    void* content = native<void*(__fastcall*)(std::byte*)>(0x1009430)(component);
    if (!content) return;
    const bool changed = presentation::sync_directive({component, presentation::kDirectiveBytes}, progress);
    const bool staleCache = read<std::uint32_t>(component, 0x4B0) != target.registry
        || read<std::uint8_t>(component, 0x4B4) != 47
        || read<std::uint16_t>(component, 0x4B6) != target.index;
    if (replace) {
        // Retire the old HUD identity and all its native beacons before reinstalling the
        // authored entry. Variant zero is an authored text index, not a revision counter.
        std::uint32_t identity = read<std::uint32_t>(component, 0x190);
        for (unsigned i = 0; i < 4; ++i)
            identity = identity * 0x01000193U ^ read<std::uint8_t>(component, 0x194 + i);
        component[0x198] = std::byte{0xFF};
        refresh(component, presentationContext);
        native<void(__fastcall*)(std::byte*, std::uint32_t, std::uint32_t, std::int32_t)>(0x1009E20)
            (component, identity, 4, -1);
        component[0x198] = std::byte{0};
        if (!omega_directive_native::install_checkpoint(component, component + 0x190, content)) return;
        runState.markerInstalled = true;
        runState.directive = component;
        runState.route = progress.route;
        runState.area = area;
        runState.viewer = viewer;
    }
    if (changed || replace || staleCache) {
        refresh(component, presentationContext);
        log("ev=omega_waypoint stage=%s run=%llu route=%u area=%u viewer=%u "
            "target=0x%08X/47/%u locator=%08X,%08X,%08X,%08X cached=0x%08X/%u/%u",
            replace ? "retire_and_install" : "repair", run, static_cast<unsigned>(progress.route), area, viewer,
            target.registry, static_cast<unsigned>(target.index), destination, target.bubbleName,
            target.registry, target.pointName, read<std::uint32_t>(component, 0x4B0),
            static_cast<unsigned>(read<std::uint8_t>(component, 0x4B4)),
            static_cast<unsigned>(read<std::uint16_t>(component, 0x4B6)));
    }
}

bool install() noexcept {
    if (ready.load(std::memory_order_acquire)) return true;
    if (std::any_of(handles.begin(), handles.end(), [](const auto& h) { return h.attached; })) {
        log("ev=omega_reveal stage=install result=retained_quiescing");
        return false;
    }
    image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    if (!image) return false;
    if (!omega_directive_native::available()) {
        log("ev=omega_reveal stage=install result=directive_trampoline_unavailable");
        return false;
    }
    if (const auto mismatch=first_mismatched_binding(image)) {
        log("ev=omega_reveal stage=install result=prefix_mismatch rva=0x%llX",
            static_cast<unsigned long long>(mismatch));
        return false;
    }
    const std::array specs{
        hooking::detour::Spec{image + 0x4EDC20, reinterpret_cast<void*>(&spawner_tick)},
        hooking::detour::Spec{image + 0x106AB20, reinterpret_cast<void*>(&cinematic_tick)},
        hooking::detour::Spec{image + 0xAB6600, reinterpret_cast<void*>(&member_tick)},
        hooking::detour::Spec{image + 0xC70180, reinterpret_cast<void*>(&character_initialize)},
        hooking::detour::Spec{image + 0xF4E660, reinterpret_cast<void*>(&graph_update)},
        hooking::detour::Spec{image + 0x104B7A0, reinterpret_cast<void*>(&fullbody_update)},
        hooking::detour::Spec{image + 0xC72390, reinterpret_cast<void*>(&mission_character_death)},
        hooking::detour::Spec{image + 0x10B0030, reinterpret_cast<void*>(&mission_motion_update)},
        hooking::detour::Spec{image + 0x10ADF70, reinterpret_cast<void*>(&mission_motion_cleanup)},
        hooking::detour::Spec{image + 0xB438B0, reinterpret_cast<void*>(&mission_scene_sense)},
        hooking::detour::Spec{image + 0xD99620, reinterpret_cast<void*>(&mission_arc_carry)},
        hooking::detour::Spec{image + 0xF36640, reinterpret_cast<void*>(&mission_arc_use)},
        hooking::detour::Spec{image + 0xC71C30, reinterpret_cast<void*>(&trace_eye_resource)},
        hooking::detour::Spec{image + 0x104DA00, reinterpret_cast<void*>(&trace_eye_clip)},
        hooking::detour::Spec{image + 0x4B25F0, reinterpret_cast<void*>(&trace_eye_clip_builder)},
        hooking::detour::Spec{image + 0x58E260, reinterpret_cast<void*>(&trace_eye_script_tick)},
        hooking::detour::Spec{image + 0x5873F0, reinterpret_cast<void*>(&trace_eye_script_action)},
        hooking::detour::Spec{image + 0x1212AF0, reinterpret_cast<void*>(&trace_eye_behavior)},
        hooking::detour::Spec{image + 0xB804E0, reinterpret_cast<void*>(&trace_eye_damage)},
        hooking::detour::Spec{image + 0xCDCB60, reinterpret_cast<void*>(&trace_eye_damage_gate)},
        hooking::detour::Spec{image + 0xB7E3C0, reinterpret_cast<void*>(&trace_eye_damage_summary)}};
    bool installed = false;
    for (unsigned attempt = 1; attempt <= 3; ++attempt) {
        hooking::detour::InstallFailure failure{};
        if (hooking::detour::install(specs, handles, failure)) { installed = true; break; }
        log("ev=omega_reveal stage=install result=fail transaction=atomic attempt=%u "
            "stage_id=%u slot=%zu native_error_known=%u native_error=%ld", attempt,
            static_cast<unsigned>(failure.stage), failure.index,
            failure.hasNativeError ? 1U : 0U, failure.nativeError);
        if (attempt < 3) Sleep(50);
    }
    if (!installed) return false;
    hooking::publish_original(spawnerOriginal, reinterpret_cast<Tick>(handles[0].original));
    hooking::publish_original(cinematicOriginal, reinterpret_cast<Tick>(handles[1].original));
    hooking::publish_original(memberOriginal, reinterpret_cast<Tick>(handles[2].original));
    hooking::publish_original(characterOriginal, reinterpret_cast<CharacterInitialize>(handles[3].original));
    hooking::publish_original(graphOriginal, reinterpret_cast<GraphUpdate>(handles[4].original));
    hooking::publish_original(fullBodyOriginal, reinterpret_cast<FullBodyUpdate>(handles[5].original));
    hooking::publish_original(deathOriginal, reinterpret_cast<CharacterDeath>(handles[6].original));
    hooking::publish_original(motionUpdateOriginal,reinterpret_cast<MotionUpdate>(handles[7].original));
    hooking::publish_original(motionCleanupOriginal,reinterpret_cast<MotionCleanup>(handles[8].original));
    hooking::publish_original(sceneSenseOriginal,reinterpret_cast<Tick>(handles[9].original));
    hooking::publish_original(arcCarryOriginal,reinterpret_cast<ArcCarry>(handles[10].original));
    hooking::publish_original(arcUseOriginal,reinterpret_cast<Tick>(handles[11].original));
    hooking::publish_original(eyeResourceOriginal,reinterpret_cast<EyeResourceDispatch>(handles[12].original));
    hooking::publish_original(eyeClipOriginal,reinterpret_cast<EyeClipDispatch>(handles[13].original));
    hooking::publish_original(eyeClipBuilderOriginal,reinterpret_cast<EyeClipBuilder>(handles[14].original));
    hooking::publish_original(eyeScriptTickOriginal,reinterpret_cast<EyeScriptTick>(handles[15].original));
    hooking::publish_original(eyeScriptActionOriginal,reinterpret_cast<EyeScriptAction>(handles[16].original));
    hooking::publish_original(eyeBehaviorOriginal,reinterpret_cast<EyeBehavior>(handles[17].original));
    hooking::publish_original(eyeDamageOriginal,reinterpret_cast<EyeDamage>(handles[18].original));
    hooking::publish_original(eyeDamageGateOriginal,reinterpret_cast<EyeDamageGate>(handles[19].original));
    hooking::publish_original(eyeDamageSummaryOriginal,reinterpret_cast<EyeDamageSummary>(handles[20].original));
    callGate.accept();
    ready.store(true, std::memory_order_release);
    log("ev=omega_reveal stage=install result=ok revision=26 transaction=atomic hooks=21 health_monitor=native_crown_callback eye_diagnostics=discovery_v1 animation=native_member_graph frame_timing=bounded");
    return true;
}
void uninstall() noexcept {
    ready.store(false, std::memory_order_release);
    callGate.quiesce();
    {
        const std::lock_guard lock(mutex);
        frame_timing::clear_scope();
    }
    if (std::none_of(handles.begin(), handles.end(), [](const auto& h) { return h.attached; })) return;
    const std::array protectedEntries{
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&spawner_tick)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&cinematic_tick)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&member_tick)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&character_initialize)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&graph_update)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&fullbody_update)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&mission_character_death)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&mission_motion_update)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&mission_motion_cleanup)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&mission_scene_sense)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&mission_arc_carry)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&mission_arc_use)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&trace_eye_resource)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&trace_eye_clip)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&trace_eye_clip_builder)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&trace_eye_script_tick)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&trace_eye_script_action)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&trace_eye_behavior)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&trace_eye_damage)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&trace_eye_damage_gate)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&trace_eye_damage_summary)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&observe_mission_device)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&observe_mission_position)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&update_directive)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&hooking::call_gate_detail::enter)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&hooking::call_gate_detail::leave)}};
    const auto result = hooking::detour::uninstall(handles, protectedEntries,
        +[]() noexcept { return callGate.idle(); });
    if (result == hooking::detour::UninstallResult::removed) {
        spawnerOriginal.store(nullptr, std::memory_order_release);
        cinematicOriginal.store(nullptr, std::memory_order_release);
        memberOriginal.store(nullptr, std::memory_order_release);
        characterOriginal.store(nullptr, std::memory_order_release);
        graphOriginal.store(nullptr, std::memory_order_release);
        fullBodyOriginal.store(nullptr, std::memory_order_release);
        deathOriginal.store(nullptr, std::memory_order_release);
        motionUpdateOriginal.store(nullptr,std::memory_order_release);
        motionCleanupOriginal.store(nullptr,std::memory_order_release);
        sceneSenseOriginal.store(nullptr,std::memory_order_release);
        arcCarryOriginal.store(nullptr,std::memory_order_release);
        arcUseOriginal.store(nullptr,std::memory_order_release);
        eyeResourceOriginal.store(nullptr,std::memory_order_release);
        eyeClipOriginal.store(nullptr,std::memory_order_release);
        eyeClipBuilderOriginal.store(nullptr,std::memory_order_release);
        eyeScriptTickOriginal.store(nullptr,std::memory_order_release);
        eyeScriptActionOriginal.store(nullptr,std::memory_order_release);
        eyeBehaviorOriginal.store(nullptr,std::memory_order_release);
        eyeDamageOriginal.store(nullptr,std::memory_order_release);
        eyeDamageGateOriginal.store(nullptr,std::memory_order_release);
        eyeDamageSummaryOriginal.store(nullptr,std::memory_order_release);
    } else {
        log("ev=omega_reveal stage=uninstall result=retained_quiescing reason=%u", static_cast<unsigned>(result));
    }
}
} // namespace omega_reveal_native
} // namespace sunrise::client::hooks::bootflow
