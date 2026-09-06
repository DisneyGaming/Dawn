#include <Windows.h>

#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <span>

#include "internal.h"
#include "omega_boss_health.h"
#include "omega_boss_health_identity.h"
#include "omega_boss_vfx_start.h"
#include "../../hooking/call_gate.h"
#include "../../hooking/detour.h"
#include "../../../core/logging/log.h"
#include "../../../state/activity/omega_presentation.h"
#include "../../../state/activity/omega_ending.h"
#include "../../../state/activity/omega_boss_intro_action.h"
#include "../../../state/activity/omega_boss_crown_action.h"
#include "../../../state/activity/omega_boss_combat_action.h"
#include "../../../state/activity/omega_boss_lift_action.h"
#include "../../../state/activity/omega_boss_teleport_action.h"
#include "../../../state/activity/omega_first_lair_runtime.h"
#include "../../../state/activity/omega_lair_chase_geometry.h"

namespace sunrise::client::hooks::bootflow {
namespace {
namespace p = state::activity::omega_presentation;
namespace ending = state::activity::omega_ending;
namespace lift = state::activity::omega_boss_lift;
namespace lair = state::activity::omega_first_lair;
namespace motion = state::activity::omega_boss_teleport;
namespace chase = state::activity::omega_lair_chase_geometry;
namespace crown = state::activity::omega_boss_crown;
namespace combat = state::activity::omega_boss_combat;
using Tick = void(__fastcall*)(void*) noexcept;
using Apply = void(__fastcall*)(void*, const void*) noexcept;
using Lookup = const void*(__fastcall*)(const std::uint32_t*) noexcept;
using IssueAction = void(__fastcall*)(void*,const void*,std::uint32_t) noexcept;
using ActorContext = void(__fastcall*)(void*,std::uint32_t) noexcept;
using Resolve = void*(__fastcall*)(std::uint32_t) noexcept;
using CharacterInit = void(__fastcall*)(void*,const std::uint32_t*) noexcept;
using AnimationRequest = void(__fastcall*)(void*,const void*,void*) noexcept;
using UpdateGraph = char(__fastcall*)(float,void*,void*,char*) noexcept;
using UpdateFullBody = std::uint64_t(__fastcall*)(void*,void*,float,std::uint8_t,void*,void*) noexcept;
using SetVariable = std::uint64_t(__fastcall*)(std::uint32_t,const std::uint32_t*,const float*) noexcept;
using UpdateMotion = bool(__fastcall*)(void*,void*,void*) noexcept;
using CleanupMotion = void(__fastcall*)(void*,void*,void*) noexcept;
using WorldPosition = float*(__fastcall*)(void*,float*) noexcept;
using NamedSequenceLookup = const void*(__fastcall*)(const void*,const std::uint32_t*) noexcept;
using NamedSequenceStart = std::uint64_t(__fastcall*)(void*,const void*,std::uint8_t) noexcept;
using NamedSequenceStop = std::uint64_t(__fastcall*)(void*,void*,const std::uint32_t*) noexcept;
std::atomic<NamedSequenceLookup> g_namedSequenceLookup{};
std::atomic<NamedSequenceStart> g_namedSequenceStart{};
std::atomic<NamedSequenceStop> g_namedSequenceStop{};
hooking::CallGate g_gate;
std::array<hooking::detour::Handle,9> g_handles{};
std::atomic<UpdateMotion> g_updateMotion{};
std::atomic<CleanupMotion> g_cleanupMotion{};
std::atomic<WorldPosition> g_worldPosition{};
std::uintptr_t g_image{};
SRWLOCK g_motionLock=SRWLOCK_INIT;
motion::CycleTracker g_motion{};
std::atomic_bool g_motionPending{};
std::uint64_t g_motionRequest{},g_motionSample{};
lair::Boss g_introStopOwner{};
constexpr std::array<float,3> departure_destination(std::uint8_t island) noexcept {
    if(island==4) { return combat::kFinalDestination; }
    const auto& point=chase::kBossDepartures[island];
    return {point.x,point.y,point.z};
}
std::atomic<UpdateGraph> g_updateGraph{};
std::atomic<UpdateFullBody> g_updateFullBody{};
std::atomic<SetVariable> g_setVariable{};
std::atomic_bool g_waitingIntroIdle{};
std::atomic_bool g_waitingIntroFlight{};
std::atomic_bool g_waitingIntroSummon{};
SRWLOCK g_crownLock=SRWLOCK_INIT;
crown::CycleTracker g_crown{};
std::uint64_t g_crownRequest{},g_crownSample{};
std::atomic_bool g_waitingCrown{};
SRWLOCK g_combatLock=SRWLOCK_INIT;
combat::Tracker g_combat{};
std::uint64_t g_combatSample{};
bool g_combatStopping{};
bool g_combatPublishing{};
struct StunnedLease final { combat::Token token{};std::uint32_t animation{UINT32_MAX};combat::NamedEventIdentity event{}; };
StunnedLease g_stunned{};
StunnedLease g_eyeRefill{};
combat::EyeHoldLedger g_eyeHold{};
void start_eye_hold(const combat::Token& token) noexcept;
void stop_eye_hold(const combat::Token& phase,bool retiring=false) noexcept;
omega_boss_health::EyeThresholdTracker g_eyeThreshold{};
lair::CrownToken g_eyeToken{},g_checkpointToken{};
bool g_eyeLogged{},g_healthUnavailableLogged{},g_checkpointReported{},g_eyeReported{};
std::atomic_bool g_waitingCombat{};
SRWLOCK g_liftLock=SRWLOCK_INIT;
lift::CycleTracker g_lift{};
std::uint64_t g_liftRequest{},g_liftSample{};
std::atomic<Tick> g_tick{}, g_resource{};
std::atomic<Tick> g_memberTick{};
std::atomic<IssueAction> g_issueAction{};
std::atomic<ActorContext> g_actorContext{};
std::atomic<Resolve> g_resolve{};
std::atomic<CharacterInit> g_characterInit{};
// Native identities only. Never retain a component pointer across callbacks.
std::atomic_uint64_t g_characterBinding{UINT64_MAX};
std::atomic<AnimationRequest> g_addEvent{},g_removeEvent{};
std::atomic<Apply> g_apply{};
std::atomic<Lookup> g_lookup{};
std::atomic_uint32_t g_resourceOwner{UINT32_MAX};
std::atomic_uint32_t g_endingResourceOwner{UINT32_MAX};
std::atomic_uint64_t g_nextSample{};
SRWLOCK g_logLock = SRWLOCK_INIT;
std::uint64_t g_loggedRun{UINT64_MAX};
std::uint32_t g_loggedRevision{UINT32_MAX};
bool g_loggedActive{},g_loggedReady{};
std::uint64_t g_loggedMemberRun{UINT64_MAX};
std::uint32_t g_loggedMemberActor{UINT32_MAX};
std::int32_t g_loggedMemberHead{-1};
std::uint32_t g_loggedOwnerActor{UINT32_MAX};
const char* g_loggedOwnerCheck{};
unsigned g_ownerGuardLogs{};
std::uint64_t g_graphLoggedRun{},g_bodyLoggedRun{};
std::array<std::uint32_t,5> g_graphLoggedState{};
std::uint32_t g_bodyLoggedStage{UINT32_MAX};
unsigned g_graphLogs{},g_bodyLogs{};
SRWLOCK g_summonLock = SRWLOCK_INIT;
struct SummonLease {
    bool owned{};
    std::uint64_t run{};
    std::uint32_t actor{},owner{},generation{},revision{};
} g_summon;
struct IssuedIntro {
    bool valid{},eventAttempted{};
    std::uint64_t run{};
    std::uint32_t actor{},generation{},revision{};
} g_issuedIntro;

bool copy(const void* source, std::span<std::byte> destination) noexcept {
    SIZE_T size{};
    return ReadProcessMemory(GetCurrentProcess(),source,destination.data(),destination.size(),&size)
        && size==destination.size();
}
template<class T> T at(const std::byte* source) noexcept {
    T value{}; std::memcpy(&value,source,sizeof value); return value;
}
template<std::size_t N> void log(const std::array<char,N>& message,int size) noexcept {
    if(size>0 && static_cast<std::size_t>(size)<N) {
        core::log::write(core::log::Channel::client,core::log::Level::info,
                        {message.data(),static_cast<std::size_t>(size)});
    }
}
bool identity(const std::byte* bytes,std::uint32_t tag,std::uint32_t kind,std::uint64_t offset) noexcept {
    return at<std::uint32_t>(bytes)==tag && at<std::uint32_t>(bytes+4)==kind
        && at<std::uint64_t>(bytes+8)==offset;
}

// These pointers live only on the native member's stack, never across ticks.
struct BossOwner {
    void* object{};
    std::uint32_t handle{UINT32_MAX};
    p::BossSummonEventState event{};
    std::uint32_t entity{UINT32_MAX},animation{UINT32_MAX};
};
void report_owner_guard(std::uint32_t actor,const char* check,std::uint32_t owner=UINT32_MAX,
                         std::uint32_t observed=UINT32_MAX,std::uint32_t component=UINT32_MAX,
                         std::uint32_t detail=UINT32_MAX,std::uint32_t ownerSelf=UINT32_MAX,
                         std::uint32_t componentOwner=UINT32_MAX,
                         std::uint32_t contextOwner=UINT32_MAX,
                         std::uint32_t contextActor=UINT32_MAX,unsigned reads=0) noexcept {
    AcquireSRWLockExclusive(&g_logLock);
    const bool changed=actor!=g_loggedOwnerActor || check!=g_loggedOwnerCheck;
    g_loggedOwnerActor=actor; g_loggedOwnerCheck=check;
    const bool emit=changed && g_ownerGuardLogs<32;
    if(emit) { ++g_ownerGuardLogs; }
    ReleaseSRWLockExclusive(&g_logLock);
    if(emit) {
        std::array<char,448> message{};
        log(message,std::snprintf(message.data(),message.size(),
            "ev=omega_boss stage=owner_guard actor=%08X check=%s owner=%08X observed=%08X component=%08X detail=%08X owner_self=%08X component_owner=%08X context_owner=%08X context_actor=%08X reads=%X",
            actor,check,owner,observed,component,detail,ownerSelf,componentOwner,
            contextOwner,contextActor,reads));
    }
}
/** Bounded rejection diagnostics for the combat-action/health observers: one
 * line per (site, reason) per run, at most kRejectLines lines per run. These
 * explain a missing receipt in the next live log. Observe-only; no control
 * flow depends on whether a line was emitted. */
constexpr unsigned kRejectLines=64;
SRWLOCK g_rejectLock=SRWLOCK_INIT;
std::uint64_t g_rejectRun{};
unsigned g_rejectLines{},g_rejectKeyCount{};
std::array<std::uint64_t,kRejectLines> g_rejectKeys{};
std::uint32_t g_rejectEpoch{};
void report_reject(std::uint64_t run,const char* site,const char* reason,std::uint32_t a=0,
    std::uint32_t b=0,std::uint32_t c=0,std::uint32_t epoch=0) noexcept {
    // Sites and reasons are string literals; their addresses identify the pair.
    // A new action epoch (next mechanic/cycle) may repeat the same pair once.
    const auto key=(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(site))<<32)
        ^static_cast<std::uint64_t>(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(reason)));
    AcquireSRWLockExclusive(&g_rejectLock);
    if(g_rejectRun!=run) { g_rejectRun=run;g_rejectLines=0;g_rejectKeyCount=0;g_rejectKeys={};g_rejectEpoch=0; }
    if(epoch!=0 && epoch!=g_rejectEpoch) { g_rejectEpoch=epoch;g_rejectKeyCount=0;g_rejectKeys={}; }
    bool emit=g_rejectLines<kRejectLines;
    for(unsigned i=0;emit && i<g_rejectKeyCount;++i) { if(g_rejectKeys[i]==key) { emit=false; } }
    if(emit) { ++g_rejectLines;if(g_rejectKeyCount<g_rejectKeys.size()) { g_rejectKeys[g_rejectKeyCount++]=key; } }
    ReleaseSRWLockExclusive(&g_rejectLock);
    if(!emit) { return; }
    std::array<char,256> line{};
    log(line,std::snprintf(line.data(),line.size(),
        "ev=omega_boss stage=reject site=%s run=%llu reason=%s a=%08X b=%08X c=%08X",
        site,static_cast<unsigned long long>(run),reason,a,b,c));
}
BossOwner boss_owner(std::uint32_t actor,bool requireEventTable=true) noexcept {
    const auto context=g_actorContext.load(std::memory_order_acquire);
    const auto resolve=g_resolve.load(std::memory_order_acquire);
    if(context==nullptr || resolve==nullptr || actor==UINT32_MAX) {
        report_owner_guard(actor,"api_or_binding"); return {};
    }
    std::array<std::byte,24> actorContext{};
    // The original member just ticked and still owns this non-invalid binding.
    // A8CB20 obtains record+50's generic parent and cached action-context
    // reference. Neither is the 808069B9 instance accepted by C620F0.
    context(actorContext.data(),actor);
    const auto handle=at<std::uint32_t>(actorContext.data());
    if(handle==UINT32_MAX || at<std::uint32_t>(actorContext.data()+4)!=actor) {
        report_owner_guard(actor,"context",handle,at<std::uint32_t>(actorContext.data()+4)); return {};
    }
    std::array<std::byte,4> value{};
    const auto* parent=static_cast<const std::byte*>(resolve(handle));
    if(parent==nullptr || !copy(parent+0x24,value) || at<std::uint32_t>(value.data())!=handle) {
        report_owner_guard(actor,"parent_identity",handle,at<std::uint32_t>(value.data())); return {};
    }
    const auto binding=g_characterBinding.load(std::memory_order_acquire);
    const auto boundActor=static_cast<std::uint32_t>(binding>>32);
    const auto characterHandle=static_cast<std::uint32_t>(binding);
    if(boundActor!=actor || characterHandle==UINT32_MAX) {
        report_owner_guard(actor,"character_not_observed",handle,boundActor,characterHandle); return {};
    }
    // C70180 observed this exact character after its native initialization.
    // Resolve its selfhandle afresh, then recheck its definition and binding.
    auto* object=static_cast<std::byte*>(resolve(characterHandle));
    std::array<std::byte,0xC4> characterBytes{};
    if(object==nullptr || !copy(object,characterBytes)) {
        report_owner_guard(actor,"character_read",handle,characterHandle); return {};
    }
    // Exact boss control definition, typed body 808069B9. The instance prefix
    // is its enclosing 80806832 definition, retained by native C70180.
    if(!identity(characterBytes.data(),0x80F6690BU,0x80806832U,0x738)) {
        report_owner_guard(actor,"character_type",handle,at<std::uint32_t>(characterBytes.data()),
                           at<std::uint32_t>(characterBytes.data()+4),
                           static_cast<std::uint32_t>(at<std::uint64_t>(characterBytes.data()+8)),
                           characterHandle); return {};
    }
    const auto ownerSelf=at<std::uint32_t>(characterBytes.data()+0x24);
    const auto ownerActor=at<std::uint32_t>(characterBytes.data()+0xC0);
    if(ownerActor!=actor || ownerSelf!=characterHandle) {
        report_owner_guard(actor,"actor_identity",handle,ownerActor,UINT32_MAX,UINT32_MAX,
                           ownerSelf,UINT32_MAX,UINT32_MAX,UINT32_MAX,3); return {};
    }
    if(!copy(object+0x5C0,value)) {
        report_owner_guard(actor,"animation_handle_read",handle,ownerActor); return {};
    }
    const auto componentHandle=at<std::uint32_t>(value.data());
    if(componentHandle==UINT32_MAX) {
        report_owner_guard(actor,"animation_not_ready",handle,ownerActor); return {};
    }
    const auto* component=resolve(componentHandle);
    std::array<std::byte,0xD8> bytes{};
    if(component==nullptr || !copy(component,bytes)) {
        report_owner_guard(actor,"component_read",handle,ownerActor,componentHandle); return {};
    }
    const auto componentOwner=at<std::uint32_t>(bytes.data()+0x30);
    const auto contextOwner=at<std::uint32_t>(bytes.data()+0xC0);
    const auto contextActor=at<std::uint32_t>(bytes.data()+0xC4);
    if(componentOwner!=ownerSelf || contextOwner!=handle || contextActor!=actor) {
        report_owner_guard(actor,"component_owner",handle,ownerActor,componentHandle,
                           at<std::uint32_t>(bytes.data()+0x34),ownerSelf,componentOwner,
                           contextOwner,contextActor,15); return {};
    }
    const auto event=p::boss_summon_event_state(bytes);
    report_owner_guard(actor,event.valid?"ready":"event_table",handle,ownerActor,componentHandle,
                       at<std::uint32_t>(bytes.data()+0x34),ownerSelf,componentOwner,
                       contextOwner,contextActor,15);
    return (event.valid || !requireEventTable) ? BossOwner{object,ownerSelf,event,
        at<std::uint32_t>(characterBytes.data()+0x2C),componentHandle} : BossOwner{};
}

// These are native typed references, not pointers retained between callbacks.
std::byte* resolve_part(std::uint32_t handle,std::uint64_t offset) noexcept {
    const auto resolve=g_resolve.load(std::memory_order_acquire);
    if(resolve==nullptr || handle==UINT32_MAX || offset>0x100000 || (offset&7)!=0) { return nullptr; }
    auto* object=static_cast<std::byte*>(resolve(handle));
    return object==nullptr ? nullptr : object+offset;
}
struct FullBody final {
    std::byte* object{};
    std::uint32_t handle{UINT32_MAX},biped{UINT32_MAX};
    std::array<std::byte,lift::kFullBodyHeaderBytes> header{};
    std::uint32_t check{},observedSelf{UINT32_MAX},observedEntity{UINT32_MAX};
    std::array<std::byte,16> observedPrefix{};
};
FullBody full_body(const BossOwner& owner) noexcept {
    FullBody result{};
    result.check=1;
    if(owner.object==nullptr || owner.entity==UINT32_MAX) { return result; }
    auto* animation=resolve_part(owner.animation,0);
    std::array<std::byte,4> handleBytes{};
    if(animation==nullptr || !copy(animation+0x1260,handleBytes)) { return result; }
    const auto bipedHandle=at<std::uint32_t>(handleBytes.data());
    result.biped=bipedHandle;result.check=2;
    auto* biped=resolve_part(bipedHandle,0);
    std::array<std::byte,0x7A8> bytes{};
    if(biped==nullptr || !copy(biped,bytes)) { return result; }
    result.check=3;std::memcpy(result.observedPrefix.data(),bytes.data(),16);
    result.observedSelf=at<std::uint32_t>(bytes.data()+0x24);
    result.observedEntity=at<std::uint32_t>(bytes.data()+0x2C);
    if(!identity(bytes.data(),0x80F66907U,0x808036CFU,0x1B48)
        || at<std::uint32_t>(bytes.data()+0x24)!=bipedHandle
        || at<std::uint32_t>(bytes.data()+0x2C)!=owner.entity) { return result; }
    const auto fullHandle=at<std::uint32_t>(bytes.data()+0x798);
    result.handle=fullHandle;result.check=4;
    auto* full=resolve_part(fullHandle,at<std::uint64_t>(bytes.data()+0x7A0));
    if(full==nullptr || !copy(full,result.header)) { return result; }
    result.check=5;std::memcpy(result.observedPrefix.data(),result.header.data(),16);
    result.observedSelf=at<std::uint32_t>(result.header.data()+0x24);
    result.observedEntity=at<std::uint32_t>(result.header.data()+0x2C);
    if(!identity(result.header.data(),0x815B5A41U,0x80803640U,0x15B8)
        || at<std::uint32_t>(result.header.data()+0x24)!=fullHandle
        || at<std::uint32_t>(result.header.data()+0x2C)!=owner.entity) { return result; }
    result.object=full;result.check=6;return result;
}

struct TeleportBinding final {
    BossOwner character{};
    motion::Owner owner{};
    std::uint32_t motionComponent{UINT32_MAX};
    std::uint32_t introSelector{UINT32_MAX};
    std::array<float,3> requested{},placed{};
    bool valid{},inactive{},destinationValid{};
};
/** Native CC4CA0(entity) reads world row+98; teleport10A8580 then resolves
 * that owner's +60 movement body. This is separate from the animation biped.
 * Resolve and validate every full handle again on each callback. */
std::uint32_t movement_body(std::uint32_t entity) noexcept {
    if(entity==UINT32_MAX) { return UINT32_MAX; }
    std::array<std::byte,8> baseBytes{};
    std::array<std::byte,4> strideBytes{};
    if(!copy(reinterpret_cast<void*>(g_image+0x1F93428),baseBytes)
        || !copy(reinterpret_cast<void*>(g_image+0x1F93430),strideBytes)) { return UINT32_MAX; }
    const auto base=at<std::uintptr_t>(baseBytes.data());
    const auto stride=at<std::uint32_t>(strideBytes.data());
    const auto delta=static_cast<std::uint64_t>(entity&0x1FFFU)*stride;
    if(base<0x10000 || stride<0xBC || stride>0x100000 || delta>UINTPTR_MAX-base) { return UINT32_MAX; }
    auto* record=reinterpret_cast<std::byte*>(base+delta);
    std::array<std::byte,0xA0> row{};
    if(!copy(record,row) || at<std::uint32_t>(row.data()+0xC)!=entity) { return UINT32_MAX; }
    const auto ownerHandle=at<std::uint32_t>(row.data()+0x98);
    auto* owner=resolve_part(ownerHandle,0);
    std::array<std::byte,0x64> ownerBytes{};
    if(owner==nullptr || !copy(owner,ownerBytes)
        || !identity(ownerBytes.data(),0x80C224E5U,0x80803A38U,0xF8)
        || at<std::uint32_t>(ownerBytes.data()+0x24)!=ownerHandle
        || at<std::uint32_t>(ownerBytes.data()+0x2C)!=entity) { return UINT32_MAX; }
    const auto bodyHandle=at<std::uint32_t>(ownerBytes.data()+0x60);
    auto* body=resolve_part(bodyHandle,0);
    std::array<std::byte,0x30> bodyBytes{};
    if(body==nullptr || !copy(body,bodyBytes)
        || !identity(bodyBytes.data(),0x80F4516EU,0x80803A00U,0xE78)
        || at<std::uint32_t>(bodyBytes.data()+0x24)!=bodyHandle
        || at<std::uint32_t>(bodyBytes.data()+0x2C)!=entity
        || !copy(record,row) || at<std::uint32_t>(row.data()+0xC)!=entity
        || at<std::uint32_t>(row.data()+0x98)!=ownerHandle) { return UINT32_MAX; }
    return bodyHandle;
}
/** Fresh native handles only; neither component nor compacting-storage pointers
 * escape this callback. Group 1 and the named-animation table are independent
 * of the cinematic graph that continues to hold its terminal idle pose. */
TeleportBinding teleport_binding(const lair::Boss& boss) noexcept {
    TeleportBinding result{};
    const auto character=boss_owner(boss.actor);
    if(character.object==nullptr || character.handle!=boss.character || character.entity!=boss.entity) { return result; }
    const auto body=full_body(character);
    if(body.object==nullptr) { return result; }
    const auto movementBody=movement_body(boss.entity);
    if(movementBody==UINT32_MAX) { return result; }
    auto* biped=resolve_part(body.biped,0);
    std::array<std::byte,16> lookupRef{};
    if(biped==nullptr || !copy(biped+0x838,lookupRef)
        || at<std::uint32_t>(lookupRef.data()+4)!=0x80803466U
        || at<std::uint64_t>(lookupRef.data()+8)!=0) { return result; }
    const auto lookupHandle=at<std::uint32_t>(lookupRef.data());
    auto* lookup=resolve_part(lookupHandle,0);
    std::array<std::byte,0x30> lookupBytes{};
    if(lookup==nullptr || !copy(lookup,lookupBytes)
        || !identity(lookupBytes.data(),0x80F45195U,0x8080344BU,0x788)
        || at<std::uint32_t>(lookupBytes.data()+0x24)!=lookupHandle
        || at<std::uint32_t>(lookupBytes.data()+0x2C)!=boss.entity) { return result; }
    auto* definition=resolve_part(0x80F45195U,0x788);
    std::array<std::byte,16> names{};
    if(definition==nullptr || !copy(definition+0x90,names)
        || at<std::uint32_t>(names.data())!=0x80F45190U
        || at<std::uint32_t>(names.data()+0xC)!=0x80F45191U) { return result; }

    auto* object=static_cast<std::byte*>(character.object);
    std::array<std::byte,16> groups{};
    if(!copy(object+0x580,groups)) { return result; }
    const auto count=at<std::uint64_t>(groups.data());
    const auto relative=at<std::int64_t>(groups.data()+8);
    if(count<2 || count>32 || relative<=0 || relative>0x100000) { return result; }
    std::array<std::byte,0x18> introGroup{};
    if(!copy(object+0x588+relative+0x40,introGroup)
        || at<std::uint32_t>(introGroup.data())!=0x80BFDE65U
        || at<std::uint32_t>(introGroup.data()+0xC)!=0x8080686BU
        || at<std::uint64_t>(introGroup.data()+0x10)!=0) { return result; }
    result.introSelector=at<std::uint32_t>(introGroup.data()+8);
    // Original C61660: descriptor base is relative to character+588; each
    // group is 50 bytes, with its interface at +40 and selector reference +48.
    std::array<std::byte,0x18> group{};
    if(!copy(object+0x588+relative+0x40+0x50,group)
        || at<std::uint32_t>(group.data())!=0x80FEE862U
        || at<std::uint32_t>(group.data()+0xC)!=0x80806750U
        || at<std::uint64_t>(group.data()+0x10)!=0) { return result; }
    const auto selectorHandle=at<std::uint32_t>(group.data()+8);
    auto* selector=resolve_part(selectorHandle,0);
    std::array<std::byte,0xD0> selectorBytes{};
    if(selector==nullptr || !copy(selector,selectorBytes)
        || !identity(selectorBytes.data(),0x80F45176U,0x80806751U,0x188)
        || at<std::uint32_t>(selectorBytes.data()+0x24)!=selectorHandle
        || at<std::uint32_t>(selectorBytes.data()+0x2C)!=boss.entity
        || at<std::uint32_t>(selectorBytes.data()+0x30)!=boss.character
        || at<std::uint32_t>(selectorBytes.data()+0x7C)!=0x808069EFU
        || at<std::uint64_t>(selectorBytes.data()+0x80)!=0) { return result; }
    const auto motionHandle=at<std::uint32_t>(selectorBytes.data()+0x78);
    auto* component=resolve_part(motionHandle,0);
    std::array<std::byte,0x30> componentHeader{};
    if(component==nullptr || !copy(component,componentHeader)
        || !identity(componentHeader.data(),0x815B5A43U,0x808069EEU,0x1010)
        || at<std::uint32_t>(componentHeader.data()+0x24)!=motionHandle
        || at<std::uint32_t>(componentHeader.data()+0x2C)!=boss.entity) { return result; }
    auto* interface=resolve_part(0x80FEE862U,0);
    std::array<std::byte,16> method{};
    if(interface==nullptr || !copy(interface+0x40,method)
        || at<std::uint32_t>(method.data())!=0x80806750U
        || at<std::uint32_t>(method.data()+4)!=4
        || at<std::uintptr_t>(method.data()+8)!=g_image+0x10C6AF0) { return result; }
    result.character=character;
    result.owner={boss.run,boss.actor,boss.character,boss.entity,boss.generation,boss.revision,
                  body.biped,selectorHandle,0x80FEE862U,movementBody,boss.island,boss.actionEpoch};
    result.motionComponent=motionHandle;
    result.inactive=selectorBytes[0x90]==std::byte{};
    for(std::size_t i=0;i<3;++i) {
        result.placed[i]=at<float>(selectorBytes.data()+0xB0+i*4);
        result.requested[i]=at<float>(selectorBytes.data()+0xC0+i*4);
    }
    // These are populated by the native producer, not necessarily before the
    // request. An owned stage-1 motion must independently match both below.
    result.destinationValid=at<float>(selectorBytes.data()+0xBC)==1.F
        && at<float>(selectorBytes.data()+0xCC)==1.F;
    result.valid=true;return result;
}

lair::Boss encounter_boss(const motion::Owner& owner) noexcept {
    return {owner.run,owner.actor,owner.character,owner.entity,owner.generation,owner.revision,owner.island,owner.actionEpoch};
}
void report_motion(const char* stage,const motion::Owner& owner,
                   motion::CycleEvent event=motion::CycleEvent::none) noexcept {
    std::array<char,288> line{};
    log(line,std::snprintf(line.data(),line.size(),
        "ev=omega_boss stage=%s run=%llu actor=%08X character=%08X entity=%08X selector=%08X event=%u island=%u epoch=%u",
        stage,static_cast<unsigned long long>(owner.run),owner.actor,owner.character,owner.entity,
        owner.selector,static_cast<unsigned>(event),static_cast<unsigned>(owner.island),owner.actionEpoch));
}
/** This is an independent native world-transform read, not the requested XYZ
 * in the motion allocation. The measured boss is unparented; reject parented
 * storage until its separate parent traversal is admitted. */
bool actual_boss_position(std::uint32_t handle,std::array<float,3>& position) noexcept {
    const auto getter=g_worldPosition.load(std::memory_order_acquire);
    if(getter==nullptr || handle==UINT32_MAX) { return false; }
    std::array<std::byte,8> baseBytes{};
    std::array<std::byte,4> strideBytes{};
    if(!copy(reinterpret_cast<void*>(g_image+0x1F93428),baseBytes)
        || !copy(reinterpret_cast<void*>(g_image+0x1F93430),strideBytes)) { return false; }
    const auto base=at<std::uintptr_t>(baseBytes.data());
    const auto stride=at<std::uint32_t>(strideBytes.data());
    const auto delta=static_cast<std::uint64_t>(handle&0x1FFFU)*stride;
    if(base<0x10000 || stride<0xBC || stride>0x100000 || delta>UINTPTR_MAX-base) { return false; }
    auto* record=reinterpret_cast<std::byte*>(base+delta);
    std::array<std::byte,0x40> identityBytes{};
    if(!copy(record,identityBytes) || at<std::uint32_t>(identityBytes.data()+0xC)!=handle
        || at<std::uint32_t>(identityBytes.data()+0x3C)!=UINT32_MAX) { return false; }
    alignas(16) std::array<float,4> sampled{};
    getter(record,sampled.data());
    if(!copy(record,identityBytes) || at<std::uint32_t>(identityBytes.data()+0xC)!=handle
        || at<std::uint32_t>(identityBytes.data()+0x3C)!=UINT32_MAX || sampled[3]!=1.F) { return false; }
    position={sampled[0],sampled[1],sampled[2]};return true;
}
bool release_intro_for_departure(const lair::Boss& boss,const TeleportBinding& binding,
    bool nativeIntroActive) noexcept {
    auto* component=resolve_part(binding.motionComponent,0);
    std::array<std::byte,motion::kMotionComponentSnapshotBytes> bytes{};
    if(component==nullptr || !copy(component,bytes)) { return false; }
    if(motion::empty_motion_arena(bytes,binding.motionComponent,boss.entity)) { return true; }
    AcquireSRWLockShared(&g_motionLock);
    auto introOwner=boss;introOwner.island=0;
    const bool introStopIssued=g_introStopOwner==introOwner;
    ReleaseSRWLockShared(&g_motionLock);
    if(introStopIssued && motion::default_locomotion_motion_arena(bytes,
        reinterpret_cast<std::uintptr_t>(component),binding.motionComponent,boss.entity)) {
        // The intro was retired and native locomotion took its place. The
        // authored arbiter, not this observer, cancels it to admit teleport.
        report_motion("teleport_fallback_ready",binding.owner);
        return true;
    }
    bool ownedIntro=false;
    for(std::uint32_t slot=0;slot<3;++slot) {
        const auto id=motion::resolve_motion_slot(bytes,reinterpret_cast<std::uintptr_t>(component),
            binding.motionComponent,boss.entity,slot,0x39,0x150);
        if(!id) { continue; }
        const auto* raw=bytes.data()+id->stateOffset;
        ownedIntro|=identity(raw,0x80F45174U,0x80806872U,0x350)
            && at<std::uint32_t>(raw+0x10)==0x80BFDE65U
            && at<std::uint32_t>(raw+0x18)==binding.introSelector
            && at<std::uint32_t>(raw+0x20)==0 && at<std::uint32_t>(raw+0x24)==0
            && at<std::uint32_t>(raw+0x28)==boss.entity
            && at<std::uint32_t>(raw+0x2C)==boss.character
            && at<std::uint32_t>(raw+0x30)==0 && at<std::uint32_t>(raw+0x34)==1
            && at<std::uint32_t>(raw+0x38)==0x80F45178U
            && at<std::uint32_t>(raw+0x3C)==binding.owner.biped;
    }
    if(!ownedIntro || !nativeIntroActive) { return false; }
    AcquireSRWLockShared(&g_summonLock);
    const bool issued=g_issuedIntro.valid && g_issuedIntro.run==boss.run
        && g_issuedIntro.actor==boss.actor && g_issuedIntro.generation==boss.generation
        && g_issuedIntro.revision==boss.revision;
    ReleaseSRWLockShared(&g_summonLock);
    const auto remove=g_removeEvent.load(std::memory_order_acquire);
    if(!issued || remove==nullptr) { return false; }
    AcquireSRWLockExclusive(&g_motionLock);
    const bool attempt=g_introStopOwner!=boss;
    if(attempt) { g_introStopOwner=boss; }
    ReleaseSRWLockExclusive(&g_motionLock);
    if(attempt) {
        // Cancel only our original named request. Its native selector and
        // scheduler own retirement; never alter the arena or force priority.
        const auto stop=p::boss_intro_stop_request();
        remove(binding.character.object,stop.data(),nullptr);
        report_motion("intro_stop_requested",binding.owner);
    }
    // Even synchronous selector inactivity must wait for native retirement.
    return false;
}
bool release_combat_for_departure(const lair::Boss& boss,const TeleportBinding& binding,bool queueActive) noexcept;
void try_departure(const lair::Boss& boss,bool nativeIntroActive,bool nativeCombatActive=false) noexcept {
    const auto state=lair::status(boss.run);
    const bool final=boss.island==4 && state.action==lair::Action::relocateFinal && state.token.valid() && state.cycle==2;
    if(!state.enabled || state.failed || state.boss!=boss || (!final && state.action!=lair::Action::depart)
        || boss.island>4) { return; }
    const auto binding=teleport_binding(boss);
    const auto destination=departure_destination(boss.island);
    const auto request=motion::encode_request(destination);
    const auto issue=g_addEvent.load(std::memory_order_acquire);
    if(!binding.valid || !binding.inactive || !request || issue==nullptr) {
        report_owner_guard(boss.actor,"teleport_binding",boss.character,
                           binding.valid?1U:0U,binding.inactive?1U:0U);return;
    }
    if(final?!release_combat_for_departure(boss,binding,nativeCombatActive)
        :!release_intro_for_departure(boss,binding,nativeIntroActive)) { return; }
    AcquireSRWLockExclusive(&g_motionLock);
    g_motion.begin_run(boss.run);
    const bool claimed=lair::claim_action(boss,final?lair::Action::relocateFinal:lair::Action::depart);
    const bool tracked=claimed && g_motion.claim(binding.owner,++g_motionRequest,true,destination,0.25F);
    if(tracked) { g_motionPending.store(true,std::memory_order_release); }
    ReleaseSRWLockExclusive(&g_motionLock);
    if(!tracked) { if(claimed) { lair::invalidate(boss.run); }return; }
    // Claim before dispatch, but hold no observer lock while native code may
    // publish/consume its motion event. Never retry an uncertain native request.
    issue(binding.character.object,request->data(),nullptr);
    report_motion("teleport_requested",binding.owner);
}

bool motion_handler(void* handler) noexcept {
    std::array<std::byte,8> bytes{};
    return copy(handler,bytes) && at<std::uintptr_t>(bytes.data())==g_image+0x1C40050;
}
TeleportBinding current_motion_binding(std::span<const std::byte> raw) noexcept {
    const auto navigation=p::navigation();
    if(!navigation.enabled || raw.size()<0xF0) { return {}; }
    const auto state=lair::status(navigation.run);
    const bool departing=state.phase==lair::Phase::departing || (state.boss.island==4
        && state.phase==lair::Phase::mechanicRequested && state.crownStage==lair::CrownStage::relocation);
    if(!state.enabled || state.failed || !departing
        || state.boss.entity!=at<std::uint32_t>(raw.data()+0x30)) { return {}; }
    return teleport_binding(state.boss);
}
struct MotionSample final {
    motion::Owner owner{};
    motion::MotionIdentity identity{};
    std::array<std::byte,0xF0> raw{};
    std::array<float,3> requested{},placed{},rawPlaced{};
    bool valid{},destinationValid{};
};
MotionSample sample_motion(void* instance,void* updateContext) noexcept {
    MotionSample result{};
    if(!copy(instance,result.raw)) { return result; }
    const auto binding=current_motion_binding(result.raw);
    if(!binding.valid) { return result; }
    auto* component=resolve_part(binding.motionComponent,0);
    std::array<std::byte,motion::kMotionComponentSnapshotBytes> bytes{};
    if(component==nullptr || !copy(component,bytes)) { return result; }
    const auto address=reinterpret_cast<std::uintptr_t>(component);
    const auto id=motion::resolve_motion_identity(bytes,address,
        reinterpret_cast<std::uintptr_t>(instance),binding.motionComponent,binding.owner.entity);
    if(!id) { return result; }
    if(updateContext!=nullptr) {
        std::array<std::byte,motion::kMotionUpdateContextBytes> context{};
        if(!copy(updateContext,context)
            || !motion::validate_update_context(context,address,binding.motionComponent)) { return result; }
    }
    result.owner=binding.owner;result.identity=*id;
    std::memcpy(result.raw.data(),bytes.data()+id->stateOffset,result.raw.size());
    result.valid=motion::parse_motion(result.raw,result.owner,true).valid;
    result.requested=binding.requested;result.placed=binding.placed;
    for(std::size_t i=0;i<3;++i) { result.rawPlaced[i]=at<float>(result.raw.data()+0x10+i*4); }
    result.destinationValid=binding.destinationValid && at<float>(result.raw.data()+0x1C)==1.F;
    return result;
}
MotionSample resample_motion(const MotionSample& before) noexcept {
    MotionSample result{};
    const auto binding=current_motion_binding(before.raw);
    if(!binding.valid || binding.owner!=before.owner
        || binding.motionComponent!=before.identity.component) { return result; }
    auto* component=resolve_part(binding.motionComponent,0);
    std::array<std::byte,motion::kMotionComponentSnapshotBytes> bytes{};
    if(component==nullptr || !copy(component,bytes)) { return result; }
    const auto id=motion::resolve_motion_slot(bytes,reinterpret_cast<std::uintptr_t>(component),
        binding.motionComponent,binding.owner.entity,before.identity.logicalSlot);
    if(!id || id->stable_id()!=before.identity.stable_id()) { return result; }
    result.owner=binding.owner;result.identity=*id;
    std::memcpy(result.raw.data(),bytes.data()+id->stateOffset,result.raw.size());
    result.valid=motion::parse_motion(result.raw,result.owner,true).valid;
    return result;
}
__declspec(noinline) void observe_motion_update(const MotionSample& before,
    const MotionSample& after,bool continues) noexcept {
    if(!before.valid || !after.valid || before.owner!=after.owner
        || before.identity.stable_id()!=after.identity.stable_id()) { return; }
    const auto first=motion::parse_motion(before.raw,after.owner,true);
    const auto next=motion::parse_motion(after.raw,after.owner,continues);
    if(!first.valid || !next.valid) { return; }
    AcquireSRWLockExclusive(&g_motionLock);
    if(before.owner!=g_motion.owner()) {
        ReleaseSRWLockExclusive(&g_motionLock);return;
    }
    const auto request=g_motion.request_id();
    const auto id=after.identity.stable_id();
    if(g_motion.phase()==motion::CyclePhase::claimed
        && (!before.destinationValid || !g_motion.bind_destination(before.owner,request,id,
            before.requested,before.placed,before.rawPlaced))) {
        g_motionPending.store(false,std::memory_order_release);
        ReleaseSRWLockExclusive(&g_motionLock);
        report_motion("teleport_destination_rejected",before.owner);
        lair::invalidate(before.owner.run);return;
    }
    // Sample before and after: a long native frame may finish departure on its
    // first update. The pre-state records that it actually entered stage 1.
    const auto entered=g_motion.observe(after.owner,request,++g_motionSample,id,first);
    const auto advanced=g_motion.observe(after.owner,request,++g_motionSample,id,next);
    ReleaseSRWLockExclusive(&g_motionLock);
    if(entered!=motion::CycleEvent::none) { report_motion("teleport_motion",after.owner,entered); }
    if(advanced!=motion::CycleEvent::none) { report_motion("teleport_motion",after.owner,advanced); }
    if(entered==motion::CycleEvent::interrupted || advanced==motion::CycleEvent::interrupted) {
        g_motionPending.store(false,std::memory_order_release);lair::invalidate(after.owner.run);
    }
}
__declspec(noinline) bool __fastcall motion_update_hook(void* handler,void* context,void* instance) noexcept {
    const hooking::CallGate::Scope call(g_gate);
    MotionSample before{};
    if(call.accepts_side_effects() && g_motionPending.load(std::memory_order_acquire)
        && motion_handler(handler) && context!=nullptr) { before=sample_motion(instance,context); }
    const bool continues=hooking::await_original(g_updateMotion)(handler,context,instance);
    if(before.valid && call.accepts_side_effects()) {
        const auto after=resample_motion(before);
        observe_motion_update(before,after,continues);
    }
    return continues;
}
__declspec(noinline) void observe_motion_cleanup(const MotionSample& before) noexcept {
    const auto binding=current_motion_binding(before.raw);
    if(!before.valid || !binding.valid || binding.owner!=before.owner
        || binding.motionComponent!=before.identity.component
        || !motion::parse_motion(before.raw,binding.owner,false).valid) { return; }
    std::array<float,3> position{};
    const bool positioned=actual_boss_position(binding.owner.entity,position);
    AcquireSRWLockExclusive(&g_motionLock);
    const auto result=g_motion.cleanup(binding.owner,g_motion.request_id(),++g_motionSample,
        before.identity.stable_id(),binding.inactive && positioned,position);
    if(result==motion::CycleEvent::completed || result==motion::CycleEvent::interrupted) {
        g_motionPending.store(false,std::memory_order_release);
    }
    ReleaseSRWLockExclusive(&g_motionLock);
    if(result!=motion::CycleEvent::none) { report_motion("teleport_cleanup",binding.owner,result); }
    if(result==motion::CycleEvent::completed) { lair::observe_departure(encounter_boss(binding.owner),true,true); }
    else if(result==motion::CycleEvent::interrupted) { lair::invalidate(binding.owner.run); }
}
__declspec(noinline) void __fastcall motion_cleanup_hook(void* handler,void* instance,void* context) noexcept {
    const hooking::CallGate::Scope call(g_gate);
    MotionSample before{};
    if(call.accepts_side_effects() && g_motionPending.load(std::memory_order_acquire)
        && motion_handler(handler)) { before=sample_motion(instance,nullptr); }
    hooking::await_original(g_cleanupMotion)(handler,instance,context);
    // Cleanup may retire the allocation; only the copied pre-state is read.
    if(before.valid && call.accepts_side_effects()) { observe_motion_cleanup(before); }
}
void report_body_guard(std::uint64_t run,const BossOwner& owner,const FullBody& body) noexcept {
    AcquireSRWLockExclusive(&g_logLock);
    if(run!=g_bodyLoggedRun) { g_bodyLoggedRun=run;g_bodyLoggedStage=UINT32_MAX;g_bodyLogs=0; }
    const bool changed=body.check!=g_bodyLoggedStage && g_bodyLogs<16;
    g_bodyLoggedStage=body.check;if(changed) { ++g_bodyLogs; }
    ReleaseSRWLockExclusive(&g_logLock);
    if(!changed) { return; }
    std::array<char,336> line{};
    log(line,std::snprintf(line.data(),line.size(),
        "ev=omega_boss stage=animation_binding run=%llu check=%u character=%08X entity=%08X animation=%08X biped=%08X fullbody=%08X observed=%08X/%08X/%llX self=%08X owner=%08X",
        static_cast<unsigned long long>(run),body.check,owner.handle,owner.entity,owner.animation,
        body.biped,body.handle,at<std::uint32_t>(body.observedPrefix.data()),
        at<std::uint32_t>(body.observedPrefix.data()+4),
        static_cast<unsigned long long>(at<std::uint64_t>(body.observedPrefix.data()+8)),
        body.observedSelf,body.observedEntity));
}
bool bound_value(const FullBody& body,lift::Arm arm,std::array<float,4>& values) noexcept {
    if(body.object==nullptr || at<std::uint32_t>(body.header.data()+0x30)!=8) { return false; }
    const auto relative=at<std::uint64_t>(body.header.data()+0x38);
    if(relative==0 || relative>0x100000) { return false; }
    const auto index=arm==lift::Arm::left?6U:7U;
    std::array<std::byte,0x60> input{};
    if(!copy(body.object+0x48+relative+index*0x60,input)
        || !identity(input.data(),0x815B5A41U,0x80809789U,arm==lift::Arm::left?0x19E0:0x1A08)) { return false; }
    auto* container=resolve_part(at<std::uint32_t>(input.data()+0x38),at<std::uint64_t>(input.data()+0x40));
    std::array<std::byte,0x10> array{};
    if(container==nullptr || !copy(container+0x50,array)) { return false; }
    const auto count=at<std::uint32_t>(array.data());
    const auto vectorIndex=at<std::uint32_t>(input.data()+0x50);
    const auto vectorRelative=at<std::uint64_t>(array.data()+8);
    if(count==0 || count>4096 || vectorIndex>=count || vectorRelative==0 || vectorRelative>0x100000) { return false; }
    return copy(container+0x68+vectorRelative+vectorIndex*0x10,std::as_writable_bytes(std::span(values)));
}
bool all_one(const std::array<float,4>& value) noexcept {
    for(const auto lane:value) { if(lane!=1.F) { return false; } }return true;
}
void report_lift(const char* stage,const lift::Owner& owner,lift::Arm arm,std::uint64_t result=0) noexcept {
    std::array<char,288> line{};
    log(line,std::snprintf(line.data(),line.size(),
        "ev=omega_boss stage=%s run=%llu actor=%08X character=%08X entity=%08X fullbody=%08X arm=%s result=%llu island=%u epoch=%u",
        stage,static_cast<unsigned long long>(owner.run),owner.actor,owner.character,owner.entity,
        owner.fullBody,arm==lift::Arm::left?"left":"right",static_cast<unsigned long long>(result),
        static_cast<unsigned>(owner.island),owner.actionEpoch));
}
lair::Boss encounter_boss(const lift::Owner& owner) noexcept {
    return {owner.run,owner.actor,owner.character,owner.entity,owner.generation,owner.revision,owner.island,owner.actionEpoch};
}
lair::Action summon_action(lift::Arm arm) noexcept {
    return arm==lift::Arm::left?lair::Action::summonLeft:lair::Action::summonRight;
}

lair::Boss encounter_boss(const crown::Owner& owner) noexcept {
    return {owner.run,owner.actor,owner.character,owner.entity,owner.generation,owner.revision,owner.island};
}
void report_crown(const char* stage,const crown::Owner& owner,std::uint64_t request) noexcept {
    std::array<char,288> line{};
    log(line,std::snprintf(line.data(),line.size(),
        "ev=omega_boss stage=%s run=%llu actor=%08X character=%08X entity=%08X generation=%u revision=%u island=%u request=%llu sequence=65D2379C",
        stage,static_cast<unsigned long long>(owner.run),owner.actor,owner.character,owner.entity,
        owner.generation,owner.revision,static_cast<unsigned>(owner.island),static_cast<unsigned long long>(request)));
}
void finish_crown_lease(const p::Navigation& navigation,std::uint32_t actor,std::uint32_t generation,
    std::uint32_t revision,bool disabled,bool queueActive) noexcept {
    AcquireSRWLockShared(&g_crownLock);
    const auto lease=g_crown.owner();const auto request=g_crown.request_id();
    const auto phase=g_crown.phase();
    ReleaseSRWLockShared(&g_crownLock);
    if(!lease.valid()) { return; }
    const auto state=lair::status(lease.run);
    const bool memberMatches=actor==lease.actor && generation==lease.generation && revision==lease.revision;
    // Another instance of the same authored member is not evidence that this
    // request was replaced. A run/state change can retire it independently.
    if(actor!=lease.actor && navigation.enabled && navigation.run==lease.run
        && state.enabled && !state.failed && state.boss==encounter_boss(lease)) { return; }
    if(navigation.enabled && navigation.run==lease.run && memberMatches && !disabled && queueActive
        && state.enabled && !state.failed && state.boss==encounter_boss(lease)) { return; }
    AcquireSRWLockExclusive(&g_crownLock);
    const bool retired=g_crown.owner()==lease && g_crown.request_id()==request;
    if(retired) { g_crown={};g_waitingCrown.store(false,std::memory_order_release); }
    ReleaseSRWLockExclusive(&g_crownLock);
    // A replaced member/run owns its own cleanup. Only remove the exact named
    // request when the still-current physical owner and queue both match.
    if(retired && memberMatches && navigation.run==lease.run && queueActive) {
        const auto owner=boss_owner(actor,false);
        const auto remove=g_removeEvent.load(std::memory_order_acquire);
        if(owner.object!=nullptr && owner.handle==lease.character && owner.entity==lease.entity && remove!=nullptr) {
            const auto stop=crown::stop_request();remove(owner.object,stop.data(),nullptr);
        }
    }
    if(retired && (phase==crown::Phase::claimed || phase==crown::Phase::playing)
        && navigation.enabled && navigation.run==lease.run && state.enabled && !state.failed
        && state.boss==encounter_boss(lease)) { lair::invalidate(lease.run); }
    if(retired) { report_crown("crown_summon_retired",lease,request); }
}
void try_crown_summon(void* member,const lair::Boss& boss,const BossOwner& character,
    const FullBody& body) noexcept {
    const auto issue=g_issueAction.load(std::memory_order_acquire);
    if(issue==nullptr || boss.island!=4 || character.object==nullptr || body.object==nullptr
        || character.handle!=boss.character || character.entity!=boss.entity) { return; }
    const crown::Owner owner{boss.run,boss.actor,boss.character,boss.entity,boss.generation,
                             boss.revision,body.biped,boss.island};
    AcquireSRWLockExclusive(&g_crownLock);
    const bool claimed=g_crown.phase()==crown::Phase::idle && lair::claim_action(boss,lair::Action::summonBoth);
    const bool tracked=claimed && g_crown.claim(owner,++g_crownRequest);
    const auto request=g_crown.request_id();
    if(tracked) { g_waitingCrown.store(true,std::memory_order_release); }
    ReleaseSRWLockExclusive(&g_crownLock);
    if(!tracked) { if(claimed) { lair::invalidate(boss.run); }return; }
    // Claim before native dispatch; no observer lock is held while native code
    // can load a graph and synchronously deliver its first receipt.
    const auto queue=crown::action();issue(member,queue.data(),0);
    if(!g_gate.accepting()) { return; }
    report_crown("crown_summon_requested",owner,request);
    std::array<std::byte,0xA48> bytes{};
    if(!copy(member,bytes) || at<std::uint32_t>(bytes.data()+0x21C)!=boss.actor
        || at<std::uint32_t>(bytes.data()+0x180)!=boss.generation
        || at<std::uint32_t>(bytes.data()+0x190)!=boss.revision
        || at<std::uint8_t>(bytes.data()+0x1D4)!=0
        || !crown::action_active(std::span(bytes).subspan(0x230,0x808),at<std::int32_t>(bytes.data()+0x228))) {
        lair::invalidate(boss.run);
    }
}
__declspec(noinline) void observe_crown_graph(std::span<const std::byte> bytes) noexcept {
    if(!g_waitingCrown.load(std::memory_order_acquire)) { return; }
    AcquireSRWLockShared(&g_crownLock);
    const auto lease=g_crown.owner();const auto request=g_crown.request_id();
    ReleaseSRWLockShared(&g_crownLock);
    const auto navigation=p::navigation();const auto state=lair::status(lease.run);
    if(!lease.valid() || !navigation.enabled || navigation.run!=lease.run || !state.enabled
        || state.failed || state.boss!=encounter_boss(lease)) { return; }
    const auto character=boss_owner(lease.actor,false);const auto body=full_body(character);
    if(character.object==nullptr || character.handle!=lease.character || character.entity!=lease.entity
        || body.object==nullptr || body.biped!=lease.biped) { return; }
    const auto receipt=crown::parse(bytes,lease);
    AcquireSRWLockExclusive(&g_crownLock);
    const auto event=g_crown.observe(lease,request,++g_crownSample,receipt);
    if(event==crown::Event::completed) { g_waitingCrown.store(false,std::memory_order_release); }
    if(event==crown::Event::started || event==crown::Event::completed) {
        lair::observe_summon(encounter_boss(lease),lair::Action::summonBoth,event==crown::Event::completed);
    }
    ReleaseSRWLockExclusive(&g_crownLock);
    if(event==crown::Event::started || event==crown::Event::completed) {
        report_crown(event==crown::Event::started?"crown_summon_started":"crown_summon_completed",lease,request);
    }
}

lair::CrownToken encounter_token(const combat::Token& token) noexcept {
    const auto& owner=token.owner;
    return {{owner.run,owner.actor,owner.character,owner.entity,owner.generation,owner.revision,4,token.epoch},token.cycle};
}
combat::Token combat_token(const lair::CrownToken& token,std::uint32_t biped) noexcept {
    const auto& boss=token.boss;
    return {{boss.run,boss.actor,boss.character,boss.entity,boss.generation,boss.revision,biped,4},boss.actionEpoch,token.cycle};
}
bool physical_match(const combat::Owner& owner,const lair::Boss& boss) noexcept {
    return owner.run==boss.run && owner.actor==boss.actor && owner.character==boss.character
        && owner.entity==boss.entity && owner.generation==boss.generation && owner.revision==boss.revision && boss.island==4;
}
void report_combat(const char* stage,const combat::Token& token,unsigned detail=0) noexcept {
    const auto& owner=token.owner;std::array<char,320> line{};
    log(line,std::snprintf(line.data(),line.size(),
        "ev=omega_boss stage=%s run=%llu actor=%08X character=%08X entity=%08X generation=%u revision=%u cycle=%u epoch=%u detail=%u",
        stage,static_cast<unsigned long long>(owner.run),owner.actor,owner.character,owner.entity,
        owner.generation,owner.revision,static_cast<unsigned>(token.cycle),token.epoch,detail));
}
// Exact named .sequence.tft entries in80F4519A, looked up and started by the
// original C66590 path used by opcode57. No fabricated script context or health
// writes. C6E920 initializes the channel's character/animation backlinks.
bool named_sequence(const combat::Token& token,std::uint32_t name,std::uint32_t resource) noexcept {
    const auto lookup=g_namedSequenceLookup.load(std::memory_order_acquire);
    const auto start=g_namedSequenceStart.load(std::memory_order_acquire);
    const auto resolve=g_resolve.load(std::memory_order_acquire);
    const auto state=lair::status(token.owner.run);
    const auto run=token.owner.run;
    if(lookup==nullptr || start==nullptr || resolve==nullptr) { report_reject(run,"named_sequence","api",name,token.epoch);return false; }
    if(state.token!=encounter_token(token) || state.failed) {
        report_reject(run,"named_sequence","state_token",name,state.boss.actionEpoch,token.epoch);return false;
    }
    if(!g_gate.accepting()) { return false; }
    const auto owner=boss_owner(token.owner.actor,false);
    auto* animation=static_cast<std::byte*>(resolve(owner.animation));
    std::array<std::byte,8> configRef{},channelOwner{};
    if(owner.handle!=token.owner.character || owner.entity!=token.owner.entity || animation==nullptr) {
        report_reject(run,"named_sequence","owner",name,owner.handle,owner.animation,token.epoch);return false;
    }
    // Live capture (boss-owner-33128.json): animation component prefix is
    // {80F56184,80F4519A,...}; C6E920 writes channel+0 = character handle and
    // channel+4 = animation handle at animation+1980.
    if(!copy(animation,configRef) || at<std::uint32_t>(configRef.data()+4)!=0x80F4519AU) {
        report_reject(run,"named_sequence","animation_config",name,at<std::uint32_t>(configRef.data()),at<std::uint32_t>(configRef.data()+4),token.epoch);return false;
    }
    if(!copy(animation+0x1980,channelOwner) || at<std::uint32_t>(channelOwner.data())!=owner.handle
        || at<std::uint32_t>(channelOwner.data()+4)!=owner.animation) {
        report_reject(run,"named_sequence","channel_owner",name,at<std::uint32_t>(channelOwner.data()),at<std::uint32_t>(channelOwner.data()+4),token.epoch);return false;
    }
    const auto* config=static_cast<const std::byte*>(resolve(0x80F4519AU));
    std::array<std::byte,0x108> definition{};
    if(config==nullptr || !copy(config,definition) || at<std::uint64_t>(definition.data()+0x10)!=7
        || at<std::int64_t>(definition.data()+0x18)!=0x38
        || at<std::uint32_t>(definition.data()+0x58)!=0x808081AEU) {
        report_reject(run,"named_sequence","config_table",name,static_cast<std::uint32_t>(at<std::uint64_t>(definition.data()+0x10)),at<std::uint32_t>(definition.data()+0x58),token.epoch);return false;
    }
    const auto* entry=static_cast<const std::byte*>(lookup(config,&name));
    bool authored{};
    for(unsigned i=0;i<7;++i) {
        const auto offset=0x60+i*0x18;
        if(entry==config+offset && at<std::uint32_t>(definition.data()+offset)==name
            && definition[offset+4]==std::byte{} && at<std::uint32_t>(definition.data()+offset+0x10)==resource) { authored=true; }
    }
    if(!authored) {
        report_reject(run,"named_sequence","lookup",name,resource,static_cast<std::uint32_t>(entry==nullptr?0U:static_cast<std::uint32_t>(entry-config)),token.epoch);return false;
    }
    std::array<std::byte,0x860> before{};
    if(!copy(animation+0x1980,before)) { report_reject(run,"named_sequence","channel_read",name,token.epoch);return false; }
    bool free{};
    for(unsigned i=0;i<8;++i) {
        const auto existing=at<std::uint32_t>(before.data()+0x20+i*0x108);
        if(existing==name) { report_reject(run,"named_sequence","already_active",name,i,token.epoch);return false; }
        free=free || existing==0x811C9DC5U;
    }
    if(!free) { report_reject(run,"named_sequence","no_free_slot",name,at<std::uint32_t>(before.data()+0x20),token.epoch);return false; }
    static_cast<void>(start(animation+0x1980,entry,1));
    if(!g_gate.accepting()) { return false; }
    const auto afterState=lair::status(token.owner.run);
    // The original final-health event can synchronously retire the character.
    // Only the state transition requiring BOTH native death and graph finish
    // may replace the normal live event-slot receipt in that case.
    if(name==0x9527E28AU && token.cycle==3 && token.epoch!=UINT32_MAX
        && afterState.enabled && !afterState.failed && afterState.crownStage==lair::CrownStage::ending
        && afterState.cycle==3 && physical_match(token.owner,afterState.boss)
        && afterState.boss.actionEpoch==token.epoch+1) {
        report_combat("named_sequence_native_death",token,name);return true;
    }
    const auto fresh=boss_owner(token.owner.actor,false);
    if(fresh.handle!=owner.handle || fresh.entity!=owner.entity || fresh.animation!=owner.animation) {
        report_reject(run,"named_sequence","owner_changed_after_start",name,fresh.handle,fresh.animation,token.epoch);return false;
    }
    if(afterState.token!=encounter_token(token)) {
        report_reject(run,"named_sequence","state_changed_after_start",name,afterState.boss.actionEpoch,static_cast<std::uint32_t>(afterState.crownStage),token.epoch);return false;
    }
    auto* current=static_cast<std::byte*>(resolve(fresh.animation));
    std::array<std::byte,0x860> after{};
    if(current==nullptr || !copy(current+0x1980,after)
        || at<std::uint32_t>(after.data())!=owner.handle || at<std::uint32_t>(after.data()+4)!=owner.animation) {
        report_reject(run,"named_sequence","channel_after_start",name,at<std::uint32_t>(after.data()),at<std::uint32_t>(after.data()+4),token.epoch);return false;
    }
    unsigned matches{};
    for(unsigned i=0;i<8;++i) { matches+=at<std::uint32_t>(after.data()+0x20+i*0x108)==name?1U:0U; }
    report_combat(matches==1?"named_sequence_started":"named_sequence_uncertain",token,name);
    if(matches!=1) { report_reject(run,"named_sequence","slot_count",name,matches,at<std::uint32_t>(after.data()+0x20),token.epoch); }
    return matches==1;
}
// Health sample trace: the baseline, every 5-percent change of either region,
// each stage change and the accepted crossing/checkpoint, capped per run.
constexpr unsigned kHealthTraceLines=48;
float g_tracedBody{-1.F},g_tracedEye{-1.F};
lair::CrownStage g_tracedStage{};
unsigned g_healthTraceLines{};
std::uint64_t g_healthTraceRun{};
combat::NamedEventIdentity stunned_identity(const BossOwner& owner,std::uint32_t name=combat::kStunnedName) noexcept {
    auto* animation=resolve_part(owner.animation,0);std::array<std::byte,0x860> bytes{};
    return animation!=nullptr && copy(animation+0x1980,bytes)
        ?combat::named_event_identity(bytes,owner.handle,owner.animation,name):combat::NamedEventIdentity{};
}
void start_stunned(const combat::Token& token) noexcept {
    AcquireSRWLockShared(&g_combatLock);const bool occupied=g_stunned.token.valid();ReleaseSRWLockShared(&g_combatLock);
    if(occupied) { report_combat("stunned_loop_lease_occupied",token);return; }
    if(!named_sequence(token,combat::kStunnedName,combat::kStunnedResource)) {
        report_combat("stunned_loop_start_unconfirmed",token);return;
    }
    const auto owner=boss_owner(token.owner.actor,false);const auto event=stunned_identity(owner);
    if(!g_gate.accepting() || owner.handle!=token.owner.character || owner.entity!=token.owner.entity
        || lair::status(token.owner.run).token!=encounter_token(token) || !event.valid()) {
        report_combat("stunned_loop_identity_unconfirmed",token);return;
    }
    AcquireSRWLockExclusive(&g_combatLock);const bool stored=!g_stunned.token.valid();
    if(stored) { g_stunned={token,owner.animation,event}; }
    ReleaseSRWLockExclusive(&g_combatLock);
    report_combat(stored?"stunned_loop_started_candidate":"stunned_loop_lease_raced",token,event.entity);
}
void stop_combat_effect(const combat::Token& phase,bool refill,bool retiring) noexcept {
    const auto name=refill?combat::kEyeRefillName:combat::kStunnedName;
    AcquireSRWLockExclusive(&g_combatLock);auto& retained=refill?g_eyeRefill:g_stunned;const auto lease=retained;
    const bool owned=lease.token.valid() && lease.token.owner==phase.owner && lease.token.cycle==phase.cycle
        && (retiring || phase.epoch>=lease.token.epoch);
    if(owned) { retained={}; }ReleaseSRWLockExclusive(&g_combatLock);
    if(!owned) { return; }
    const auto stop=g_namedSequenceStop.load(std::memory_order_acquire);
    const auto resolve=g_resolve.load(std::memory_order_acquire);const auto navigation=p::navigation();
    if(!g_gate.accepting() || stop==nullptr || resolve==nullptr || navigation.run!=lease.token.owner.run) { return; }
    const auto owner=boss_owner(lease.token.owner.actor,false);
    if(owner.handle!=lease.token.owner.character || owner.entity!=lease.token.owner.entity
        || owner.animation!=lease.animation || !lease.event.valid() || stunned_identity(owner,name)!=lease.event) {
        report_combat(refill?"eye_refill_already_retired":"stunned_loop_already_retired",lease.token);return;
    }
    std::array<std::byte,4> ancillary{};
    if(!copy(static_cast<const std::byte*>(owner.object)+0x5C4,ancillary)
        || at<std::uint32_t>(ancillary.data())==UINT32_MAX || resolve(at<std::uint32_t>(ancillary.data()))==nullptr) {
        report_combat(refill?"eye_refill_stop_owner_unconfirmed":"stunned_loop_stop_owner_unconfirmed",lease.token);return;
    }
    auto* animation=resolve_part(owner.animation,0);if(animation==nullptr) { return; }
    static_cast<void>(stop(animation+0x1980,owner.object,&name));
    if(!g_gate.accepting()) { return; }
    const auto fresh=boss_owner(lease.token.owner.actor,false);
    const bool cleared=fresh.handle==owner.handle && fresh.entity==owner.entity && fresh.animation==owner.animation
        && stunned_identity(fresh,name)!=lease.event;
    report_combat(refill?(cleared?"eye_refill_stopped":"eye_refill_stop_unconfirmed")
        :(cleared?"stunned_loop_stopped":"stunned_loop_stop_unconfirmed"),lease.token);
}
void stop_stunned(const combat::Token& phase,bool retiring=false) noexcept { stop_combat_effect(phase,false,retiring); }
void stop_eye_refill(const combat::Token& phase,bool retiring=false) noexcept { stop_combat_effect(phase,true,retiring); }
void capture_eye_refill(const combat::Token& token,const BossOwner& owner) noexcept {
    if(!g_gate.accepting() || owner.handle!=token.owner.character || owner.entity!=token.owner.entity
        || lair::status(token.owner.run).token!=encounter_token(token)) { return; }
    const auto event=stunned_identity(owner,combat::kEyeRefillName);
    if(!event.valid()) { return; }
    AcquireSRWLockExclusive(&g_combatLock);
    const bool stored=g_eyeRefill.token==token && g_eyeRefill.animation==owner.animation && !g_eyeRefill.event.valid();
    if(stored) { g_eyeRefill.event=event; }
    ReleaseSRWLockExclusive(&g_combatLock);
    if(stored) { report_combat("eye_refill_owned",token,event.entity); }
}
const char* stage_name(lair::CrownStage stage) noexcept {
    switch(stage) {
    case lair::CrownStage::eyeOpening:return "eyeOpening";
    case lair::CrownStage::eyeDps:return "eyeDps";
    case lair::CrownStage::recovery:return "recovery";
    case lair::CrownStage::death:return "death";
    default:return "other";
    }
}
/** The producer: called from the native boss-member tick (AB6600) while a
 * shield/eye token or a recovery checkpoint token is leased. It reads the
 * original CD6C20 regional getter through omega_boss_health::sample, arms the
 * eye tracker during eyeOpening/eyeDps and publishes the qualified downward
 * crossing only in eyeDps; the body checkpoint publishes only in recovery. */
void observe_combat_health(const lair::Status& state,const BossOwner& owner) noexcept {
    if(!state.enabled || state.failed || !state.token.valid()) { return; }
    const auto run=state.boss.run;
    AcquireSRWLockShared(&g_combatLock);
    const auto eye=g_eyeToken,checkpoint=g_checkpointToken;
    const bool relevant=state.token==eye || state.token==checkpoint;
    ReleaseSRWLockShared(&g_combatLock);
    if(!relevant) { return; }
    if(state.token==eye) { capture_eye_refill(combat_token(eye,full_body(owner).biped),owner); }
    if(owner.object==nullptr || owner.handle!=state.boss.character || owner.entity!=state.boss.entity) {
        report_reject(run,"health","owner",owner.handle,owner.entity,state.boss.character,state.boss.actionEpoch);return;
    }
    omega_boss_health::Sample sample{};
    omega_boss_health::Reject reason{};
    const bool available=omega_boss_health::sample(g_image,g_resolve.load(std::memory_order_acquire),state.boss,owner.object,sample,&reason);
    bool baseline{},crossing{},checkpointReady{},trace{},armed{};
    float previous{};
    AcquireSRWLockExclusive(&g_combatLock);
    if(!available) {
        ReleaseSRWLockExclusive(&g_combatLock);
        report_reject(run,"health",omega_boss_health::reject_name(reason),state.boss.character,state.boss.actionEpoch,static_cast<std::uint32_t>(state.crownStage),state.boss.actionEpoch);
        return;
    }
    if(g_healthTraceRun!=run) { g_healthTraceRun=run;g_healthTraceLines=0;g_tracedBody=g_tracedEye=-1.F;g_tracedStage={}; }
    if(state.token==g_eyeToken && (state.crownStage==lair::CrownStage::eyeOpening || state.crownStage==lair::CrownStage::eyeDps)) {
        baseline=!g_eyeLogged;g_eyeLogged=true;
        static_cast<void>(g_eyeThreshold.observe(g_eyeToken,sample.handle,sample.eye));
        crossing=state.crownStage==lair::CrownStage::eyeDps && g_eyeThreshold.crossed() && !g_eyeReported;
        if(crossing) { g_eyeReported=true; }
    } else if(state.token==g_checkpointToken && state.crownStage==lair::CrownStage::recovery
        && !g_checkpointReported && omega_boss_health::checkpoint_reached(state.cycle,sample.body)) {
        g_checkpointReported=true;checkpointReady=true;
    }
    armed=g_eyeThreshold.armed();previous=g_eyeThreshold.previous();
    const bool changed=std::fabs(sample.body-g_tracedBody)>=0.05F || std::fabs(sample.eye-g_tracedEye)>=0.05F
        || state.crownStage!=g_tracedStage;
    if((baseline || crossing || checkpointReady || changed) && g_healthTraceLines<kHealthTraceLines) {
        trace=true;++g_healthTraceLines;g_tracedBody=sample.body;g_tracedEye=sample.eye;g_tracedStage=state.crownStage;
    }
    ReleaseSRWLockExclusive(&g_combatLock);
    if(trace) {
        std::array<char,448> message{};
        log(message,std::snprintf(message.data(),message.size(),
            "ev=omega_boss stage=eye_health run=%llu actor=%08X character=%08X health=%08X cycle=%u epoch=%u crown_stage=%s body=%.9g eye=%.9g dead=%u armed=%u previous=%.9g crossing=%u checkpoint=%u",
            static_cast<unsigned long long>(run),state.boss.actor,state.boss.character,sample.handle,state.cycle,
            state.boss.actionEpoch,stage_name(state.crownStage),sample.body,sample.eye,sample.dead?1U:0U,armed?1U:0U,previous,
            crossing?1U:0U,checkpointReady?1U:0U));
    }
    // A body value close to, but not at, the authored checkpoint would mean
    // the native 1F setter does not round-trip the fraction; report it once.
    if(state.token==g_checkpointToken && state.crownStage==lair::CrownStage::recovery && !checkpointReady) {
        float expected{};
        if(omega_boss_health::checkpoint_fraction(state.cycle,expected) && std::fabs(sample.body-expected)<0.02F) {
            std::uint32_t bits{};std::memcpy(&bits,&sample.body,sizeof bits);
            report_reject(run,"health","checkpoint_near_miss",bits,state.cycle,sample.handle,state.boss.actionEpoch);
        }
    }
    if(!g_gate.accepting()) { return; }
    if(lair::status(run).token!=state.token) {
        if(crossing || checkpointReady) { report_reject(run,"health","token_changed",state.boss.actionEpoch,crossing?1U:0U,checkpointReady?1U:0U,state.boss.actionEpoch); }
        return;
    }
    if(crossing && !lair::observe_health(state.token,lair::HealthMilestone::eyeThresholdReached)) {
        report_reject(run,"health","state_rejected_eye",state.boss.actionEpoch,static_cast<std::uint32_t>(state.crownStage),static_cast<std::uint32_t>(state.phase),state.boss.actionEpoch);
    }
    if(checkpointReady && !lair::observe_health(state.token,lair::HealthMilestone::checkpointReached)) {
        report_reject(run,"health","state_rejected_checkpoint",state.boss.actionEpoch,static_cast<std::uint32_t>(state.crownStage),static_cast<std::uint32_t>(state.phase),state.boss.actionEpoch);
    }
}
p::BossSummonEventState combat_event_state(const BossOwner& owner,std::uint8_t cycle,std::uint32_t event) noexcept {
    auto* object=resolve_part(owner.animation,0);std::array<std::byte,0xD8> bytes{};
    if(object==nullptr || !copy(object,bytes) || at<std::uint32_t>(bytes.data()+0x30)!=owner.handle) { return {}; }
    return combat::event_state(bytes,cycle,event);
}
bool remove_combat_event(const combat::Token& token,std::uint32_t event) noexcept {
    const auto run=token.owner.run;
    const auto owner=boss_owner(token.owner.actor,false);const auto remove=g_removeEvent.load(std::memory_order_acquire);
    if(owner.object==nullptr || owner.handle!=token.owner.character || owner.entity!=token.owner.entity || remove==nullptr) {
        report_reject(run,"remove_event","owner",event,owner.handle,owner.entity);return false;
    }
    const auto before=combat_event_state(owner,token.cycle,event);
    if(!before.valid || before.refs==0) { report_reject(run,"remove_event","not_registered",event,before.valid?1U:0U,before.refs);return false; }
    const auto request=combat::request(token.cycle,event);remove(owner.object,request.data(),nullptr);
    if(!g_gate.accepting()) { return false; }
    const auto fresh=boss_owner(token.owner.actor,false);
    const auto after=combat_event_state(fresh,token.cycle,event);
    const bool released=fresh.object!=nullptr && fresh.handle==owner.handle && fresh.entity==owner.entity
        && after.valid && after.refs+1==before.refs;
    if(!released) { report_reject(run,"remove_event","refs_after_remove",event,before.refs,after.valid?after.refs:UINT32_MAX); }
    return released;
}
bool combat_motion_ready(const lair::Boss& boss) noexcept {
    // This reader's semantic island bound is for the four chase requests. Use
    // its physical handle validation only; no teleport request is dispatched.
    auto physical=boss;physical.island=0;physical.actionEpoch=0;
    const auto binding=teleport_binding(physical);
    auto* object=resolve_part(binding.motionComponent,0);
    std::array<std::byte,motion::kMotionComponentSnapshotBytes> bytes{};
    return binding.valid && binding.inactive && object!=nullptr && copy(object,bytes)
        && (motion::empty_motion_arena(bytes,binding.motionComponent,boss.entity)
            || motion::default_locomotion_motion_arena(bytes,reinterpret_cast<std::uintptr_t>(object),binding.motionComponent,boss.entity));
}
bool release_combat_for_departure(const lair::Boss& boss,const TeleportBinding& binding,bool queueActive) noexcept {
    AcquireSRWLockShared(&g_combatLock);const auto graph=g_combat.graph_token();
    const bool stopping=g_combatStopping;const auto stage=g_combat.stage();
    ReleaseSRWLockShared(&g_combatLock);
    if(!graph.valid() || graph.cycle!=2 || !physical_match(graph.owner,boss)
        || stage!=combat::Stage::recovered) { return false; }
    if(stopping) { return combat_motion_ready(boss); }
    if(!queueActive || binding.owner.biped!=graph.owner.biped) { return false; }
    auto* component=resolve_part(binding.motionComponent,0);
    std::array<std::byte,motion::kMotionComponentSnapshotBytes> bytes{};
    if(component==nullptr || !copy(component,bytes)) { return false; }
    bool owned=false;
    for(std::uint32_t slot=0;slot<3;++slot) {
        const auto id=motion::resolve_motion_slot(bytes,reinterpret_cast<std::uintptr_t>(component),binding.motionComponent,boss.entity,slot,0x39,0x150);
        if(!id) { continue; }
        const auto* raw=bytes.data()+id->stateOffset;
        owned|=identity(raw,0x80F45174U,0x80806872U,0x350)
            && at<std::uint32_t>(raw+0x10)==0x80BFDE65U && at<std::uint32_t>(raw+0x18)==binding.introSelector
            && at<std::uint32_t>(raw+0x1C)==0x8080686BU
            && at<std::uint32_t>(raw+0x20)==0 && at<std::uint32_t>(raw+0x24)==0
            && at<std::uint32_t>(raw+0x28)==boss.entity && at<std::uint32_t>(raw+0x2C)==boss.character
            && at<std::uint32_t>(raw+0x30)==0 && at<std::uint32_t>(raw+0x34)==combat::graph(2)->ordinal
            && at<std::uint32_t>(raw+0x38)==0x80F45178U && at<std::uint32_t>(raw+0x3C)==graph.owner.biped
            && at<std::uint32_t>(raw+0xD8)==4 && at<std::uint32_t>(raw+0xDC)==combat::graph(2)->record;
    }
    const auto remove=g_removeEvent.load(std::memory_order_acquire);
    if(!owned || remove==nullptr) { return false; }
    AcquireSRWLockExclusive(&g_combatLock);
    const bool stop=g_combat.graph_token()==graph && !g_combatStopping;
    if(stop) { g_combatStopping=true; }
    ReleaseSRWLockExclusive(&g_combatLock);
    if(stop) { const auto request=combat::request(2);remove(binding.character.object,request.data(),nullptr);report_combat("combat_final_stop_requested",graph); }
    return false;
}
void retire_combat(const p::Navigation& navigation,std::uint32_t actor,std::uint32_t generation,
    std::uint32_t revision,bool disabled,std::span<const std::byte> queue,std::int32_t head) noexcept {
    AcquireSRWLockShared(&g_combatLock);const auto graph=g_combat.graph_token();const auto event=g_combat.leased_event();
    const bool active=g_combat.active();ReleaseSRWLockShared(&g_combatLock);
    if(!active) { return; }
    const auto state=lair::status(graph.owner.run);
    const bool current=navigation.enabled && navigation.run==graph.owner.run && state.enabled && !state.failed
        && physical_match(graph.owner,state.boss);
    if(current && (actor!=graph.owner.actor || (generation==graph.owner.generation
        && revision==graph.owner.revision && !disabled))) { return; }
    AcquireSRWLockExclusive(&g_combatLock);
    const bool retired=g_combat.graph_token()==graph;
    if(retired) { g_combat={};g_combatStopping=false;g_combatPublishing=false;g_eyeThreshold={};g_eyeToken={};g_checkpointToken={};g_waitingCombat.store(false,std::memory_order_release); }
    ReleaseSRWLockExclusive(&g_combatLock);
    if(!retired) { return; }
    stop_eye_hold(graph,true);
    stop_eye_refill(graph,true);
    stop_stunned(graph,true);
    if(navigation.run==graph.owner.run && actor==graph.owner.actor && generation==graph.owner.generation
        && revision==graph.owner.revision && combat::action_active(queue,head,graph.cycle)) {
        if(event!=0) { static_cast<void>(remove_combat_event(graph,event)); }
        const auto owner=boss_owner(actor,false);const auto remove=g_removeEvent.load(std::memory_order_acquire);
        if(remove!=nullptr && owner.handle==graph.owner.character && owner.entity==graph.owner.entity) {
            const auto stop=combat::request(graph.cycle);remove(owner.object,stop.data(),nullptr);
        }
    }
    report_combat("combat_retired",graph);
}
void try_combat(void* member,const lair::Status& state,const BossOwner& owner,const FullBody& body,
    std::span<const std::byte> queue,std::int32_t head,std::int32_t count) noexcept {
    if(!state.token.valid() || state.failed || owner.object==nullptr || body.object==nullptr
        || owner.handle!=state.boss.character || owner.entity!=state.boss.entity) { return; }
    const auto token=combat_token(state.token,body.biped);
    AcquireSRWLockShared(&g_combatLock);const auto graph=g_combat.graph_token();
    const bool active=g_combat.active(),stopping=g_combatStopping;const auto stage=g_combat.stage();
    ReleaseSRWLockShared(&g_combatLock);
    if(state.action==lair::Action::summonBoth) {
        if(active) {
            if(graph.owner!=token.owner || token.cycle!=graph.cycle+1 || stage!=combat::Stage::recovered) { return; }
            const bool queueActive=combat::action_active(queue,head,graph.cycle);
            if(!stopping) {
                const auto remove=g_removeEvent.load(std::memory_order_acquire);
                if(!queueActive || remove==nullptr) { return; }
                AcquireSRWLockExclusive(&g_combatLock);
                const bool stop=g_combat.graph_token()==graph && !g_combatStopping;
                if(stop) { g_combatStopping=true; }
                ReleaseSRWLockExclusive(&g_combatLock);
                if(stop) { const auto request=combat::request(graph.cycle);remove(owner.object,request.data(),nullptr);report_combat("combat_stop_requested",graph); }
                return;
            }
            if(queueActive || head<count || !combat_motion_ready(state.boss)) { return; }
            AcquireSRWLockExclusive(&g_combatLock);
            if(g_combat.graph_token()==graph && g_combatStopping) { g_combat={};g_combatStopping=false; }
            ReleaseSRWLockExclusive(&g_combatLock);
        }
        if(head<count || !combat_motion_ready(state.boss)) { return; }
        const auto issue=g_issueAction.load(std::memory_order_acquire);if(issue==nullptr) { return; }
        AcquireSRWLockExclusive(&g_combatLock);
        const bool claimed=!g_combat.active() && lair::claim_action(state.boss,state.action);
        const bool tracked=claimed && g_combat.begin(token);
        if(tracked) { g_waitingCombat.store(true,std::memory_order_release); }
        ReleaseSRWLockExclusive(&g_combatLock);
        if(!tracked) { if(claimed) { lair::invalidate(state.boss.run); }return; }
        const auto action=combat::action(token.cycle);issue(member,action.data(),0);
        if(!g_gate.accepting()) { return; }
        std::array<std::byte,0xA48> after{};
        const bool accepted=copy(member,after) && at<std::uint32_t>(after.data()+0x21C)==state.boss.actor
            && at<std::uint32_t>(after.data()+0x180)==state.boss.generation
            && at<std::uint32_t>(after.data()+0x190)==state.boss.revision && after[0x1D4]==std::byte{}
            && combat::action_active(std::span(after).subspan(0x230,0x808),at<std::int32_t>(after.data()+0x228),token.cycle);
        report_combat(accepted?"combat_summon_requested":"combat_summon_uncertain",token);
        if(!accepted) { lair::invalidate(state.boss.run); }
        return;
    }
    combat::Command command{};
    if(state.action==lair::Action::beginDeletion) { command=combat::Command::deletion; }
    else if(state.action==lair::Action::breakShield) { command=combat::Command::shield; }
    else if(state.action==lair::Action::endEyePhase) { command=combat::Command::eye; }
    else { return; }
    // Only a mechanic waiting to be requested is a rejection worth explaining;
    // mechanicPlaying/waiting ticks arrive here every frame by design.
    const bool pending=state.phase==lair::Phase::mechanicReady;
    const auto run=state.boss.run;
    const auto commandCode=static_cast<std::uint32_t>(command);
    if(!active || stopping) {
        if(pending) { report_reject(run,"try_combat","graph_inactive",commandCode,active?1U:0U,stopping?1U:0U,token.epoch); }
        return;
    }
    if(graph.owner!=token.owner || graph.cycle!=token.cycle) {
        if(pending) { report_reject(run,"try_combat","graph_owner",commandCode,graph.cycle,token.cycle,token.epoch); }
        return;
    }
    if(!combat::action_active(queue,head,token.cycle)) {
        if(pending) { report_reject(run,"try_combat","queue_inactive",commandCode,static_cast<std::uint32_t>(head),static_cast<std::uint32_t>(count),token.epoch); }
        return;
    }
    const auto& value=*combat::graph(token.cycle);
    const auto event=command==combat::Command::deletion?value.deletion:command==combat::Command::shield?value.shield:value.eye;
    const auto before=combat_event_state(owner,token.cycle,event);const auto add=g_addEvent.load(std::memory_order_acquire);
    if(!before.can_add() || add==nullptr) {
        if(pending) { report_reject(run,"try_combat","event_table",event,before.valid?1U:0U,before.refs,token.epoch); }
        return;
    }
    AcquireSRWLockExclusive(&g_combatLock);
    const bool publishing=g_combatPublishing;
    const bool requestable=!publishing && g_combat.can_request(token,command);
    const auto trackerStage=static_cast<std::uint32_t>(g_combat.stage());
    const auto commandEpoch=g_combat.command_token().epoch;
    const bool claimed=requestable && lair::claim_action(state.boss,state.action);
    const bool tracked=claimed && g_combat.claim(token,command);
    if(tracked) { g_combatPublishing=true; }
    ReleaseSRWLockExclusive(&g_combatLock);
    if(!tracked) {
        if(claimed) { report_reject(run,"try_combat","tracker_claim",commandCode,trackerStage,token.epoch);lair::invalidate(run); }
        else if(pending) {
            report_reject(run,requestable?"try_combat_state":"try_combat",requestable?"claim_action":"can_request",
                commandCode,trackerStage,requestable?token.epoch:commandEpoch);
        }
        return;
    }
    if(command==combat::Command::shield) {
        // The eye refill (95A36FFE, upward 100 percent) runs before the graph
        // event so the tracker's first sample is the old side of the crossing.
        if(!named_sequence(token,combat::kEyeRefillName,combat::kEyeRefillResource)) {
            AcquireSRWLockExclusive(&g_combatLock);g_combatPublishing=false;ReleaseSRWLockExclusive(&g_combatLock);
            if(g_gate.accepting()) { report_reject(run,"try_combat","refill_sequence",event,token.epoch);lair::invalidate(run); }return;
        }
        AcquireSRWLockExclusive(&g_combatLock);
        g_eyeRefill={token,owner.animation,{}};
        g_eyeThreshold={};g_eyeToken=state.token;g_eyeLogged=false;g_eyeReported=false;g_healthUnavailableLogged=false;
        ReleaseSRWLockExclusive(&g_combatLock);
        capture_eye_refill(token,owner);
        observe_combat_health(lair::status(run),owner);
    }
    if(command==combat::Command::eye && token.cycle==3) {
        omega_boss_health::Reject reason{};
        if(!omega_boss_health::arm_native_death(g_image,g_resolve.load(std::memory_order_acquire),state.token,&reason)) {
            AcquireSRWLockExclusive(&g_combatLock);g_combatPublishing=false;ReleaseSRWLockExclusive(&g_combatLock);
            report_reject(run,"try_combat","arm_native_death",event,static_cast<std::uint32_t>(reason),token.epoch);
            lair::invalidate(run);return;
        }
    }
    const auto request=combat::request(token.cycle,event);add(owner.object,request.data(),nullptr);
    const auto fresh=boss_owner(token.owner.actor,false);const auto after=combat_event_state(fresh,token.cycle,event);
    const bool registered=fresh.handle==owner.handle && fresh.entity==owner.entity && after.valid && after.refs==before.refs+1;
    AcquireSRWLockExclusive(&g_combatLock);g_combatPublishing=false;ReleaseSRWLockExclusive(&g_combatLock);
    if(!g_gate.accepting()) { return; }
    report_combat(registered?"combat_event_registered":"combat_event_uncertain",token,event);
    if(!registered) {
        report_reject(run,"try_combat","event_refs_after_add",event,before.refs,after.valid?after.refs:UINT32_MAX,token.epoch);
        lair::invalidate(run);
    }
}
// Graph node trace for the owned combat controller: one line per node/record
// change, capped per run, independent of whether parse accepted the receipt.
constexpr unsigned kGraphTraceLines=48;
std::array<std::uint32_t,3> g_tracedGraph{UINT32_MAX,UINT32_MAX,UINT32_MAX};
unsigned g_graphTraceLines{};
std::uint64_t g_graphTraceRun{};
__declspec(noinline) void observe_combat_graph(std::span<const std::byte> bytes,char nativeResult) noexcept {
    if(!g_waitingCombat.load(std::memory_order_acquire)) { return; }
    AcquireSRWLockShared(&g_combatLock);const auto graph=g_combat.graph_token();
    const bool blocked=g_combatPublishing || g_combatStopping;ReleaseSRWLockShared(&g_combatLock);
    if(blocked || !graph.valid()) { return; }
    const auto run=graph.owner.run;
    const auto navigation=p::navigation();const auto state=lair::status(run);
    if(!navigation.enabled || navigation.run!=run || !state.enabled || state.failed || !physical_match(graph.owner,state.boss)) {
        report_reject(run,"combat_graph","state",state.enabled?1U:0U,state.failed?1U:0U,state.boss.actor,state.boss.actionEpoch);return;
    }
    // Only this owner's controller is interesting: same entity/character/biped.
    const bool ours=bytes.size()>=0xB8 && at<std::uint32_t>(bytes.data())==graph.owner.entity
        && at<std::uint32_t>(bytes.data()+4)==graph.owner.character && at<std::uint32_t>(bytes.data()+0x14)==graph.owner.biped;
    if(!ours) { return; }
    const auto owner=boss_owner(graph.owner.actor,false);const auto body=full_body(owner);
    if(owner.handle!=graph.owner.character || owner.entity!=graph.owner.entity || body.object==nullptr || body.biped!=graph.owner.biped) {
        report_reject(run,"combat_graph","owner",owner.handle,body.check,body.biped,state.boss.actionEpoch);return;
    }
    const auto receipt=combat::parse(bytes,graph,nativeResult);
    const std::array<std::uint32_t,3> observedGraph{at<std::uint32_t>(bytes.data()+0xB0),at<std::uint32_t>(bytes.data()+0xB4),
        static_cast<std::uint32_t>(receipt.stage)};
    AcquireSRWLockExclusive(&g_combatLock);
    const auto outcome=g_combatPublishing?combat::Outcome{}:g_combat.observe(graph,++g_combatSample,receipt);
    if(g_graphTraceRun!=run) { g_graphTraceRun=run;g_graphTraceLines=0;g_tracedGraph={UINT32_MAX,UINT32_MAX,UINT32_MAX}; }
    const bool traceNode=observedGraph!=g_tracedGraph && g_graphTraceLines<kGraphTraceLines;
    if(traceNode) { ++g_graphTraceLines;g_tracedGraph=observedGraph; }
    const auto trackerStage=static_cast<unsigned>(g_combat.stage());
    ReleaseSRWLockExclusive(&g_combatLock);
    if(traceNode) {
        std::array<char,384> line{};
        log(line,std::snprintf(line.data(),line.size(),
            "ev=omega_boss stage=combat_graph_node run=%llu cycle=%u epoch=%u node=%u record=%u requested=%u/%u duration=%.9g elapsed=%.9g clip_active=%u flags=%02X native=%d stage=%u valid=%u tracker_stage=%u",
            static_cast<unsigned long long>(run),static_cast<unsigned>(graph.cycle),state.boss.actionEpoch,observedGraph[0],observedGraph[1],
            at<std::uint32_t>(bytes.data()+0xA4),at<std::uint32_t>(bytes.data()+0xA8),at<float>(bytes.data()+0x38),at<float>(bytes.data()+0x3C),
            static_cast<unsigned>(at<std::uint8_t>(bytes.data()+0x21)),static_cast<unsigned>(at<std::uint8_t>(bytes.data()+0x20)),
            static_cast<int>(nativeResult),static_cast<unsigned>(receipt.stage),receipt.valid?1U:0U,trackerStage));
    }
    if(!receipt.valid && at<std::uint8_t>(bytes.data()+0x21)!=0) {
        // Node known but duration/record outside the authored graph proof.
        std::uint32_t durationBits{};std::memcpy(&durationBits,bytes.data()+0x38,sizeof durationBits);
        report_reject(run,"combat_graph","receipt_invalid",observedGraph[0],durationBits,observedGraph[1],state.boss.actionEpoch);
    }
    if(outcome.milestone==combat::Milestone::none) { return; }
    if(outcome.releaseEvent!=0 && !remove_combat_event(outcome.token,outcome.releaseEvent)) {
        report_combat("combat_event_release_uncertain",outcome.token,outcome.releaseEvent);lair::invalidate(run);return;
    }
    if(!g_gate.accepting()) { return; }
    const auto token=encounter_token(outcome.token);
    // The named stunned resource owns its authored DPS effects. Its visual
    // identity is a live-test candidate; failed VFX never advances or aborts
    // combat. Exact child handles qualify native cleanup at phase retirement.
    if(outcome.milestone==combat::Milestone::eyeExposing) { start_stunned(outcome.token); }
    else if(outcome.milestone==combat::Milestone::eyeVulnerable) { start_eye_hold(outcome.token); }
    else if(outcome.milestone==combat::Milestone::recoveryStarted || outcome.milestone==combat::Milestone::deathStarted) {
        stop_eye_hold(outcome.token);
        stop_eye_refill(outcome.token);
        stop_stunned(outcome.token);
    }
    if(!g_gate.accepting()) { return; }
    if(outcome.milestone==combat::Milestone::summonStarted || outcome.milestone==combat::Milestone::summonFinished) {
        lair::observe_summon(token.boss,lair::Action::summonBoth,outcome.milestone==combat::Milestone::summonFinished);
    } else {
        // combat::Milestone deletionStarted..deathFinished (3..10) map onto
        // lair::AnimationMilestone deletionStarted..deathFinished (0..7).
        const auto milestone=static_cast<lair::AnimationMilestone>(static_cast<unsigned>(outcome.milestone)
            -static_cast<unsigned>(combat::Milestone::deletionStarted));
        if(!lair::observe_animation(token,milestone)) {
            report_reject(run,"combat_graph","state_rejected_animation",static_cast<std::uint32_t>(outcome.milestone),
                static_cast<std::uint32_t>(state.crownStage),static_cast<std::uint32_t>(state.phase),token.boss.actionEpoch);
        }
    }
    if(outcome.milestone==combat::Milestone::recoveryStarted || outcome.milestone==combat::Milestone::deathFinished) {
        const auto name=token.cycle==1?0xB52CCA3BU:token.cycle==2?0xB52CCA38U:0x9527E28AU;
        const auto resource=token.cycle==1?0x80F4545CU:token.cycle==2?0x80F4545EU:0x80F45460U;
        if(!named_sequence(outcome.token,name,resource)) {
            if(g_gate.accepting()) { report_reject(run,"combat_graph","checkpoint_sequence",name,token.cycle,token.boss.actionEpoch);lair::invalidate(run); }return;
        }
        if(token.cycle<3) {
            AcquireSRWLockExclusive(&g_combatLock);g_checkpointToken=token;g_checkpointReported=false;
            ReleaseSRWLockExclusive(&g_combatLock);
        }
    }
    report_combat("combat_graph_receipt",outcome.token,static_cast<unsigned>(outcome.milestone));
}

__declspec(noinline) char __fastcall graph_update_hook(float dt,void* context,void* controller,
                                                       char* transitioned) noexcept {
    const hooking::CallGate::Scope call(g_gate);
    const auto result=hooking::await_original(g_updateGraph)(dt,context,controller,transitioned);
    if(!call.accepts_side_effects() || (!g_waitingIntroIdle.load(std::memory_order_acquire)
        && !g_waitingIntroFlight.load(std::memory_order_acquire)
        && !g_waitingIntroSummon.load(std::memory_order_acquire)
        && !g_waitingCrown.load(std::memory_order_acquire)
        && !g_waitingCombat.load(std::memory_order_acquire))) { return result; }
    std::array<std::byte,0xB8> bytes{};
    if(!copy(controller,bytes) || at<std::uint32_t>(bytes.data()+0x10)!=0x80F45178U) { return result; }
    observe_crown_graph(bytes);
    observe_combat_graph(bytes,result);
    if(!g_waitingIntroIdle.load(std::memory_order_acquire)
        && !g_waitingIntroFlight.load(std::memory_order_acquire)
        && !g_waitingIntroSummon.load(std::memory_order_acquire)) { return result; }
    const auto navigation=p::navigation();
    if(!navigation.enabled) { return result; }
    AcquireSRWLockShared(&g_summonLock);
    const auto intro=g_issuedIntro;
    ReleaseSRWLockShared(&g_summonLock);
    if(!intro.valid || intro.run!=navigation.run) { return result; }
    const std::array<std::uint32_t,5> observed{
        at<std::uint32_t>(bytes.data()+4),at<std::uint32_t>(bytes.data()+0xB0),
        at<std::uint32_t>(bytes.data()+0xB4),at<std::uint32_t>(bytes.data()+0xA4),
        at<std::uint32_t>(bytes.data()+0xA8)};
    AcquireSRWLockExclusive(&g_logLock);
    if(g_graphLoggedRun!=navigation.run) { g_graphLoggedRun=navigation.run;g_graphLogs=0;g_graphLoggedState={}; }
    const bool changed=observed!=g_graphLoggedState && g_graphLogs<32;
    g_graphLoggedState=observed;if(changed) { ++g_graphLogs; }
    ReleaseSRWLockExclusive(&g_logLock);
    if(changed) {
        std::array<char,320> line{};
        log(line,std::snprintf(line.data(),line.size(),
            "ev=omega_boss stage=graph_loaded run=%llu entity=%08X character=%08X group_index=%u sequence_index=%u biped=%08X loaded=%u/%u requested=%u/%u clip_active=%u",
            static_cast<unsigned long long>(navigation.run),at<std::uint32_t>(bytes.data()),observed[0],
            at<std::uint32_t>(bytes.data()+8),at<std::uint32_t>(bytes.data()+0xC),
            at<std::uint32_t>(bytes.data()+0x14),observed[2],observed[1],observed[4],observed[3],
            static_cast<unsigned>(at<std::uint8_t>(bytes.data()+0x21))));
    }
    if(g_waitingIntroFlight.load(std::memory_order_acquire) && observed[1]==1 && observed[2]==0) {
        const auto owner=boss_owner(intro.actor,false);
        const auto body=full_body(owner);
        if(body.object!=nullptr && lift::intro_flight(bytes,owner.entity,owner.handle,body.biped)
            && p::observe_boss_flight(intro.run,intro.generation)) {
            g_waitingIntroFlight.store(false,std::memory_order_release);
        }
    }
    // Node 4 is the authored two-arm summon clip; the graph reaches it only via
    // the leased C0F9C866 event. Its loaded receipt starts the first cohort,
    // not the terminal idle 7.2 s later. No clip time is used to gate spawns.
    if(g_waitingIntroSummon.load(std::memory_order_acquire) && intro.eventAttempted
        && observed[1]==4 && observed[2]==0) {
        const auto owner=boss_owner(intro.actor);
        const auto body=full_body(owner);
        if(body.object!=nullptr && lift::intro_summon(bytes,owner.entity,owner.handle,body.biped)) {
            const lair::Boss boss{intro.run,intro.actor,owner.handle,owner.entity,intro.generation,intro.revision};
            lair::observe_initial_summon(boss);
            const auto state=lair::status(intro.run);
            if(state.enabled && state.boss==boss && state.phase!=lair::Phase::initial) {
                g_waitingIntroSummon.store(false,std::memory_order_release);
                std::array<char,320> line{};
                log(line,std::snprintf(line.data(),line.size(),
                    "ev=omega_boss stage=intro_summon_started run=%llu actor=%08X character=%08X entity=%08X generation=%u revision=%u node=4 clip=80F45188 elapsed=%.3f duration=%.3f phase=%u",
                    static_cast<unsigned long long>(intro.run),intro.actor,owner.handle,owner.entity,
                    intro.generation,intro.revision,static_cast<double>(at<float>(bytes.data()+0x3C)),
                    static_cast<double>(at<float>(bytes.data()+0x38)),static_cast<unsigned>(state.phase)));
            }
        }
    }
    if(!intro.eventAttempted || observed[1]!=2 || observed[2]!=0) { return result; }
    const auto owner=boss_owner(intro.actor);
    const auto body=full_body(owner);
    report_body_guard(intro.run,owner,body);
    if(body.object==nullptr || !lift::intro_terminal_idle(bytes,owner.entity,owner.handle,body.biped)) { return result; }
    const lair::Boss boss{intro.run,intro.actor,owner.handle,owner.entity,intro.generation,intro.revision};
    lair::observe_initial_idle(boss);
    const auto state=lair::status(intro.run);
    if(state.enabled && state.boss==boss && state.phase!=lair::Phase::initial
        && state.phase!=lair::Phase::bothPlaying) {
        g_waitingIntroIdle.store(false,std::memory_order_release);
        g_waitingIntroSummon.store(false,std::memory_order_release);
    }
    return result;
}

/** Observe after native overlay compaction, then request/release a single named
 * property through its original setter. The existing terminal-idle queue stays
 * in place. Native animation time, not a wall clock, releases the owned cycle. */
void release_cancelled_lift(void* instance) noexcept {
    if(!TryAcquireSRWLockExclusive(&g_liftLock)) { return; }
    if(g_lift.phase()==lift::CyclePhase::claimed || g_lift.phase()==lift::CyclePhase::playing) {
        const auto lease=g_lift.owner();
        const auto owner=boss_owner(lease.actor);
        const auto body=full_body(owner);
        const auto setter=g_setVariable.load(std::memory_order_acquire);
        std::array<float,4> before{};
        if(setter!=nullptr && body.object==instance && body.handle==lease.fullBody
            && owner.handle==lease.character && owner.entity==lease.entity
            && bound_value(body,g_lift.arm(),before)) {
            if(all_one(before)) {
                const auto name=lift::property(g_lift.arm());
                const std::array<float,4> zero{};
                const auto result=setter(owner.entity,&name,zero.data());
                report_lift("lift_cancel_release",lease,g_lift.arm(),result);
            }
            // Dispatch at most once; a changed native value belongs to its new owner.
            g_lift={};
        }
    }
    ReleaseSRWLockExclusive(&g_liftLock);
}
__declspec(noinline) void observe_lift(void* instance) noexcept {
    std::array<std::byte,0x30> prefix{};
    if(!copy(instance,prefix) || !identity(prefix.data(),0x815B5A41U,0x80803640U,0x15B8)) { return; }
    const auto navigation=p::navigation();
    if(!navigation.enabled) { release_cancelled_lift(instance);return; }
    const auto state=lair::status(navigation.run);
    if(!state.enabled || state.failed) { release_cancelled_lift(instance);return; }
    if(!state.boss.valid()) { return; }
    if(at<std::uint32_t>(prefix.data()+0x2C)!=state.boss.entity) { return; }
    const auto owner=boss_owner(state.boss.actor);
    const auto body=full_body(owner);
    if(body.object!=instance || owner.handle!=state.boss.character || owner.entity!=state.boss.entity) { return; }
    const lift::Owner current{navigation.run,state.boss.actor,owner.handle,owner.entity,
                             state.boss.generation,state.boss.revision,body.handle,state.boss.island,state.boss.actionEpoch};
    const auto setter=g_setVariable.load(std::memory_order_acquire);
    if(setter==nullptr || !TryAcquireSRWLockExclusive(&g_liftLock)) { return; }
    g_lift.begin_run(navigation.run);
    const bool running=g_lift.phase()==lift::CyclePhase::claimed || g_lift.phase()==lift::CyclePhase::playing;
    if(running) {
        if(current.island!=g_lift.owner().island) {
            ReleaseSRWLockExclusive(&g_liftLock);return;
        }
        const auto arm=g_lift.arm();
        std::array<float,4> before{};
        if(current!=g_lift.owner() || !bound_value(body,arm,before) || !all_one(before)) {
            report_lift("lift_ownership_lost",current,arm);lair::invalidate(navigation.run);
            ReleaseSRWLockExclusive(&g_liftLock);return;
        }
        const auto relative=at<std::uint64_t>(body.header.data()+0x508);
        std::array<std::byte,lift::kGroupBytes*lift::kGroupCount> groups{};
        if(relative==0 || relative>0x100000 || !copy(body.object+0x518+relative,groups)) {
            ReleaseSRWLockExclusive(&g_liftLock);return;
        }
        const auto receipt=lift::parse_overlay(body.header,groups,owner.entity,body.handle,arm);
        const auto event=g_lift.observe(current,g_lift.request_id(),++g_liftSample,receipt);
        if(event==lift::CycleEvent::started) {
            report_lift("lift_started",current,arm);
            lair::observe_summon(encounter_boss(current),summon_action(arm),false);
        } else if(event==lift::CycleEvent::completed || event==lift::CycleEvent::interrupted) {
            const auto name=lift::property(arm);
            const std::array<float,4> zero{};
            const auto setResult=setter(owner.entity,&name,zero.data());
            std::array<float,4> after{};
            const auto fresh=full_body(boss_owner(current.actor));
            const bool released=fresh.object==body.object && bound_value(fresh,arm,after) && lift::zero_baseline(after);
            report_lift(released?"lift_released":"lift_release_uncertain",current,arm,setResult);
            if(event==lift::CycleEvent::completed && released) {
                lair::observe_summon(encounter_boss(current),summon_action(arm),true);
            } else { lair::invalidate(navigation.run); }
        }
    } else if(state.action==lair::Action::summonLeft || state.action==lair::Action::summonRight) {
        const auto arm=state.action==lair::Action::summonLeft?lift::Arm::left:lift::Arm::right;
        std::array<float,4> left{},right{};
        if(bound_value(body,lift::Arm::left,left) && bound_value(body,lift::Arm::right,right)
            && lift::zero_baseline(left) && lift::zero_baseline(right)
            && lair::claim_action(state.boss,state.action)) {
            if(!g_lift.claim(current,++g_liftRequest,arm,arm==lift::Arm::left?left:right)) {
                lair::invalidate(navigation.run);
            } else {
                const auto name=lift::property(arm);
                const std::array<float,4> one{1,1,1,1};
                const auto setResult=setter(owner.entity,&name,one.data());
                std::array<float,4> after{};
                const auto fresh=full_body(boss_owner(current.actor));
                const bool applied=fresh.object==body.object && bound_value(fresh,arm,after) && all_one(after);
                report_lift(applied?"lift_requested":"lift_request_uncertain",current,arm,setResult);
                if(!applied) { lair::invalidate(navigation.run); }
            }
        }
    }
    ReleaseSRWLockExclusive(&g_liftLock);
}
__declspec(noinline) std::uint64_t __fastcall full_body_update_hook(void* instance,void* requests,
    float dt,std::uint8_t lod,void* timing,void* selectors) noexcept {
    const hooking::CallGate::Scope call(g_gate);
    const auto result=hooking::await_original(g_updateFullBody)(instance,requests,dt,lod,timing,selectors);
    if(call.accepts_side_effects()) { observe_lift(instance); }
    return result;
}

__declspec(noinline) void __fastcall character_init_hook(void* instance,
                                                        const std::uint32_t* actorId) noexcept {
    const hooking::CallGate::Scope call(g_gate);
    hooking::await_original(g_characterInit)(instance,actorId);
    if(!call.accepts_side_effects()) { return; }
    std::array<std::byte,0xC4> bytes{};
    if(!copy(instance,bytes) || !identity(bytes.data(),0x80F6690BU,0x80806832U,0x738)) { return; }
    const auto actor=at<std::uint32_t>(bytes.data()+0xC0);
    const auto handle=at<std::uint32_t>(bytes.data()+0x24);
    const auto resolve=g_resolve.load(std::memory_order_acquire);
    std::array<std::byte,4> input{};
    if(actor==UINT32_MAX || handle==UINT32_MAX || resolve==nullptr || resolve(handle)!=instance
        || !copy(actorId,input) || at<std::uint32_t>(input.data())!=actor) { return; }
    const auto binding=(static_cast<std::uint64_t>(actor)<<32)|handle;
    if(g_characterBinding.exchange(binding,std::memory_order_acq_rel)==binding) { return; }
    std::array<char,192> message{};
    log(message,std::snprintf(message.data(),message.size(),
        "ev=omega_boss stage=character_bound actor=%08X character=%08X definition=80F6690B instance=%p",
        actor,handle,instance));
}

// Paired removal runs only while this exact member still owns the same actor.
// On actor destruction/rebinding the old component owns its own teardown.
void finish_summon_lease(std::uint64_t run,std::uint32_t actor,std::uint32_t generation,
                         std::uint32_t revision,bool finished) noexcept {
    AcquireSRWLockExclusive(&g_summonLock);
    if(g_issuedIntro.valid && (finished || g_issuedIntro.run!=run
        || g_issuedIntro.actor!=actor || g_issuedIntro.generation!=generation
        || g_issuedIntro.revision!=revision)) {
        g_issuedIntro={};
        g_waitingIntroIdle.store(false,std::memory_order_release);
        g_waitingIntroFlight.store(false,std::memory_order_release);
        g_waitingIntroSummon.store(false,std::memory_order_release);
    }
    if(!g_summon.owned) { ReleaseSRWLockExclusive(&g_summonLock); return; }
    if(g_summon.actor!=actor) {
        g_summon={};
        ReleaseSRWLockExclusive(&g_summonLock); return;
    }
    if(!finished && g_summon.run==run && g_summon.generation==generation && g_summon.revision==revision) {
        ReleaseSRWLockExclusive(&g_summonLock); return;
    }
    const auto owner=boss_owner(actor);
    const auto remove=g_removeEvent.load(std::memory_order_acquire);
    if(owner.object==nullptr || remove==nullptr) { ReleaseSRWLockExclusive(&g_summonLock); return; }
    if(owner.handle!=g_summon.owner || owner.event.refs==0) {
        g_summon={};
        ReleaseSRWLockExclusive(&g_summonLock); return;
    }
    const auto request=p::boss_summon_request();
    remove(owner.object,request.data(),nullptr);
    const auto after=boss_owner(actor);
    const bool paired=after.object!=nullptr && after.handle==owner.handle
        && after.event.refs+1==owner.event.refs;
    std::array<char,256> message{};
    log(message,std::snprintf(message.data(),message.size(),
        "ev=omega_boss stage=summon_event_remove run=%llu actor=%08X owner=%08X before=%u after=%u paired=%u",
        static_cast<unsigned long long>(g_summon.run),actor,owner.handle,owner.event.refs,
        after.event.refs,paired?1U:0U));
    // The void API was dispatched once. Never retry an uncertain removal and
    // risk decrementing a reference owned by another native controller.
    g_summon={};
    ReleaseSRWLockExclusive(&g_summonLock);
}

/** Observe within native ownership of this exact 0x270-byte component. No pointer is
 * retained and no native field, camera/HUD flag, or component authority bit is patched. */
__declspec(noinline) void observe(void* instance,bool immediate) noexcept {
    if(!g_gate.accepting()) { return; }
    const auto navigation=p::navigation();
    if(!navigation.enabled) { return; }
    std::array<std::byte,0x10> prefix{};
    if(!copy(instance,prefix)) { return; }
    if(identity(prefix.data(),ending::kDefinition,0x80804F07U,0x2E8)) {
        const auto command=ending::authority(navigation.run,GetTickCount64());
        if(!command.token.valid() || !command.bookendState) { return; }
        std::array<std::byte,0x270> bytes{};
        if(!copy(instance,bytes)) { return; }
        const auto lookup=g_lookup.load(std::memory_order_acquire);
        const auto owner=g_endingResourceOwner.load(std::memory_order_acquire);
        bool ready=false;
        if(lookup!=nullptr && owner!=UINT32_MAX) {
            const std::uint32_t selector=ending::kSelector;
            const auto* entry=lookup(&selector);
            std::array<std::byte,8> header{};
            ready=entry!=nullptr && copy(entry,header)
                && at<std::uint32_t>(header.data()+4)==owner;
        }
        ending::observe(command.token,at<std::uint32_t>(bytes.data()+0x190),
            at<std::uint8_t>(bytes.data()+0x260)!=0,ready);
        return;
    }
    if(!identity(prefix.data(),p::kIntroDefinition,0x80804F07U,0x2E8)) { return; }
    const auto now=GetTickCount64();
    if(!immediate && now<g_nextSample.load(std::memory_order_relaxed)) { return; }
    g_nextSample.store(now+250,std::memory_order_relaxed);
    std::array<std::byte,0x270> bytes{};
    if(!copy(instance,bytes)) { return; }
    const auto lookup=g_lookup.load(std::memory_order_acquire);
    bool ready=false;
    if(lookup!=nullptr) {
        const std::uint32_t selector=p::kIntroSelector;
        const auto* entry=lookup(&selector);
        std::array<std::byte,8> header{};
        // DD1680 registered the exact 80F44F66 cinematic owner. A74B2200 is also
        // used by unrelated templates, so the selector alone is insufficient.
        ready=entry!=nullptr && copy(entry,header)
            && g_resourceOwner.load(std::memory_order_acquire)!=UINT32_MAX
            && at<std::uint32_t>(header.data()+4)==g_resourceOwner.load(std::memory_order_acquire);
    }
    const auto revision=at<std::uint32_t>(bytes.data()+0x190);
    const bool active=at<std::uint8_t>(bytes.data()+0x260)!=0;
    AcquireSRWLockExclusive(&g_logLock);
    const bool changed=navigation.run!=g_loggedRun || revision!=g_loggedRevision
        || active!=g_loggedActive || ready!=g_loggedReady;
    g_loggedRun=navigation.run; g_loggedRevision=revision;
    g_loggedActive=active; g_loggedReady=ready;
    ReleaseSRWLockExclusive(&g_logLock);
    if(changed) {
        std::array<char,256> message{};
        const int size=std::snprintf(message.data(),message.size(),
            "ev=omega_intro stage=native run=%llu instance=%p revision=%u desired=%u active=%u ready=%u owner=%08X",
            static_cast<unsigned long long>(navigation.run),instance,revision,
            static_cast<unsigned>(at<std::uint8_t>(bytes.data()+0x194)),active?1U:0U,ready?1U:0U,
            g_resourceOwner.load(std::memory_order_acquire));
        log(message,size);
    }
    p::observe_intro(revision,active,ready);
}
__declspec(noinline) void __fastcall tick_hook(void* instance) noexcept {
    const hooking::CallGate::Scope call(g_gate);
    hooking::await_original(g_tick)(instance);
    if(call.accepts_side_effects()) { observe(instance,false); }
}
__declspec(noinline) void __fastcall apply_hook(void* instance,const void* authority) noexcept {
    const hooking::CallGate::Scope call(g_gate);
    hooking::await_original(g_apply)(instance,authority);
    if(call.accepts_side_effects()) { observe(instance,true); }
}
__declspec(noinline) void __fastcall resource_hook(void* instance) noexcept {
    const hooking::CallGate::Scope call(g_gate);
    hooking::await_original(g_resource)(instance);
    if(!call.accepts_side_effects()) { return; }
    std::array<std::byte,0x30> bytes{};
    if(!copy(instance,bytes)) { return; }
    if(identity(bytes.data(),ending::kResource,0x80806647U,ending::kResourceOffset)) {
        const auto owner=at<std::uint32_t>(bytes.data()+0x2C);
        g_endingResourceOwner.store(owner,std::memory_order_release);
        std::array<char,176> message{};
        const int size=std::snprintf(message.data(),message.size(),
            "ev=omega_ending stage=resource_registered definition=80C177DD instance=%p owner=%08X selector=2FECC6FD",
            instance,owner);
        log(message,size);
        return;
    }
    if(!identity(bytes.data(),p::kIntroResource,0x80806647U,0x378)) { return; }
    const auto owner=at<std::uint32_t>(bytes.data()+0x2C);
    g_resourceOwner.store(owner,std::memory_order_release);
    std::array<char,160> message{};
    const int size=std::snprintf(message.data(),message.size(),
        "ev=omega_intro stage=resource_registered definition=80F44F66 instance=%p owner=%08X selector=A74B2200",
        instance,owner);
    log(message,size);
}

// The flight command does not depend on event-table availability. Once that
// exact command is active, the native graph can wait for its summon event.
// Retry read-only readiness checks, but never retry an uncertain native add.
void try_summon_event(std::uint64_t run,std::uint32_t actor,std::uint32_t generation,
                      std::uint32_t revision) noexcept {
    AcquireSRWLockExclusive(&g_summonLock);
    if(g_summon.owned || !g_issuedIntro.valid || g_issuedIntro.eventAttempted
        || g_issuedIntro.run!=run || g_issuedIntro.actor!=actor
        || g_issuedIntro.generation!=generation || g_issuedIntro.revision!=revision) {
        ReleaseSRWLockExclusive(&g_summonLock); return;
    }
    const auto owner=boss_owner(actor);
    const auto add=g_addEvent.load(std::memory_order_acquire);
    if(owner.object==nullptr || !owner.event.can_add() || add==nullptr) {
        ReleaseSRWLockExclusive(&g_summonLock); return;
    }
    g_issuedIntro.eventAttempted=true;
    const auto request=p::boss_summon_request();
    add(owner.object,request.data(),nullptr);
    const auto after=boss_owner(actor);
    const bool registered=after.object!=nullptr && after.handle==owner.handle
        && after.event.refs==owner.event.refs+1;
    std::array<char,256> message{};
    log(message,std::snprintf(message.data(),message.size(),
        "ev=omega_boss stage=summon_event_add run=%llu actor=%08X owner=%08X before=%u after=%u registered=%u",
        static_cast<unsigned long long>(run),actor,owner.handle,owner.event.refs,
        after.event.refs,registered?1U:0U));
    if(registered) { g_summon={true,run,actor,owner.handle,generation,revision}; }
    ReleaseSRWLockExclusive(&g_summonLock);
}

// Lane J: red-eye scalar initialization; shares this file's helpers and gate.
#include "omega_boss_vfx_start.inl"

/** Called while the native member owns its lifetime. Keep its authority, actor
 * binding and generation unchanged; the native command setter owns the queue
 * copy and animation completion. Do not replace a currently active command. */
__declspec(noinline) void observe_boss_member(void* instance) noexcept {
    const auto navigation=p::navigation();
    std::array<std::byte,0x10> prefix{};
    if(!copy(instance,prefix)
        || !identity(prefix.data(),p::kBossMemberDefinition,0x80807D9DU,0xB58)) { return; }
    std::array<std::byte,0xA48> bytes{};
    if(!copy(instance,bytes)) { return; }
    const auto actor=at<std::uint32_t>(bytes.data()+0x21C);
    const auto head=at<std::int32_t>(bytes.data()+0x228);
    const auto count=at<std::int32_t>(bytes.data()+0x230);
    const auto generation=at<std::uint32_t>(bytes.data()+0x180);
    const auto revision=at<std::uint32_t>(bytes.data()+0x190);
    const bool disabled=at<std::uint8_t>(bytes.data()+0x1D4)!=0;
    retire_combat(navigation,actor,generation,revision,disabled,std::span(bytes).subspan(0x230,0x808),head);
    finish_crown_lease(navigation,actor,generation,revision,disabled,
        crown::action_active(std::span(bytes).subspan(0x230,0x808),head));
    finish_summon_lease(navigation.run,actor,generation,revision,
        !navigation.enabled || disabled
        || !p::boss_intro_action_active(std::span(bytes).subspan(0x230,0x808),head));
    if(!navigation.enabled) { return; }
    if(!disabled && actor!=UINT32_MAX) {
        const auto binding=g_characterBinding.load(std::memory_order_acquire);
        if(static_cast<std::uint32_t>(binding>>32)==actor) {
            const auto status=lair::status(navigation.run);
            if(status.enabled && status.boss.actor==actor && status.boss.generation==generation
                && status.boss.revision==revision
                && status.boss.character==static_cast<std::uint32_t>(binding)) {
                try_departure(status.boss,p::boss_intro_action_active(
                    std::span(bytes).subspan(0x230,0x808),head),
                    combat::action_active(std::span(bytes).subspan(0x230,0x808),head,status.cycle));
            }
        }
    }
    AcquireSRWLockExclusive(&g_logLock);
    const bool changed=navigation.run!=g_loggedMemberRun || actor!=g_loggedMemberActor
        || head!=g_loggedMemberHead;
    g_loggedMemberRun=navigation.run;g_loggedMemberActor=actor;g_loggedMemberHead=head;
    ReleaseSRWLockExclusive(&g_logLock);
    if(changed) {
        std::array<char,256> message{};
        log(message,std::snprintf(message.data(),message.size(),
            "ev=omega_boss stage=member run=%llu instance=%p actor=%08X generation=%u revision=%u disabled=%u head=%d count=%d",
            static_cast<unsigned long long>(navigation.run),instance,actor,generation,revision,
            disabled?1U:0U,head,count));
    }
    if(!disabled && actor!=UINT32_MAX) { trace_intro_vfx(navigation.run,actor); }
    const auto issue=g_issueAction.load(std::memory_order_acquire);
    if(issue==nullptr || actor==UINT32_MAX || disabled || head<0 || count<0 || count>32
        || head>32) { return; }
    const auto crownState=lair::status(navigation.run);
    if(crownState.enabled && crownState.token.valid() && crownState.boss.actor==actor
        && crownState.boss.generation==generation && crownState.boss.revision==revision) {
        const auto character=boss_owner(actor,false);const auto full=full_body(character);
        observe_combat_health(crownState,character);
        try_combat(instance,crownState,character,full,std::span(bytes).subspan(0x230,0x808),head,count);
        return;
    }
    if(head<count) {
        if(p::boss_intro_action_active(std::span(bytes).subspan(0x230,0x808),head)) {
            try_summon_event(navigation.run,actor,generation,revision);
        }
        return;
    }
    // The native member is authoritative readiness evidence even when an
    // optional forced-activity factory diagnostic did not observe creation.
    // Validate the full character/actor/animation linkage on this tick, and
    // require this exact member generation to match the requested boss.
    // Flight remains independent of later summon-event-table availability.
    const auto owner=boss_owner(actor,false);
    const auto body=full_body(owner);
    if(owner.object==nullptr || body.object==nullptr) { return; }
    const auto state=lair::status(navigation.run);
    if(state.enabled && state.boss.island==4) {
        if(!state.failed && state.boss.actor==actor && state.boss.generation==generation && state.boss.revision==revision
            && state.boss.character==owner.handle && state.boss.entity==owner.entity
            && state.action==lair::Action::summonBoth) { try_crown_summon(instance,state.boss,owner,body); }
        return;
    }
    if(!p::observe_boss(navigation.run,p::kBossEntity,owner.entity,generation)) { return; }
    if(!p::claim_boss_intro_action(navigation.run)) { return; }
    AcquireSRWLockExclusive(&g_summonLock);
    g_issuedIntro={true,false,navigation.run,actor,generation,revision};
    g_waitingIntroIdle.store(true,std::memory_order_release);
    g_waitingIntroFlight.store(true,std::memory_order_release);
    g_waitingIntroSummon.store(true,std::memory_order_release);
    ReleaseSRWLockExclusive(&g_summonLock);
    const auto queue=p::boss_intro_action();
    std::array<char,256> message{};
    log(message,std::snprintf(message.data(),message.size(),
        "ev=omega_boss stage=intro_action_begin run=%llu actor=%08X group=%08X sequence=%08X generation=%u revision=%u",
        static_cast<unsigned long long>(navigation.run),actor,p::kBossIntroGroup,
        p::kBossIntroSequence,generation,revision));
    // Enable the authored red-eye scalar through its original setter once per
    // owner and run, immediately before the intro graph is queued. Its outcome
    // never gates the intro.
    prepare_intro_vfx(navigation.run,actor,generation,revision,owner,disabled,head,count);
    issue(instance,queue.data(),0);
    if(copy(instance,bytes)) {
        log(message,std::snprintf(message.data(),message.size(),
            "ev=omega_boss stage=intro_action_return run=%llu actor=%08X generation=%u revision=%u head=%d count=%d dispatch=%p",
            static_cast<unsigned long long>(navigation.run),at<std::uint32_t>(bytes.data()+0x21C),
            at<std::uint32_t>(bytes.data()+0x180),at<std::uint32_t>(bytes.data()+0x190),
            at<std::int32_t>(bytes.data()+0x228),at<std::int32_t>(bytes.data()+0x230),
            reinterpret_cast<void*>(at<std::uintptr_t>(bytes.data()+0xA40))));
        finish_summon_lease(navigation.run,at<std::uint32_t>(bytes.data()+0x21C),
            at<std::uint32_t>(bytes.data()+0x180),at<std::uint32_t>(bytes.data()+0x190),
            !p::boss_intro_action_active(std::span(bytes).subspan(0x230,0x808),
                                         at<std::int32_t>(bytes.data()+0x228)));
        if(at<std::uint8_t>(bytes.data()+0x1D4)==0
            && p::boss_intro_action_active(std::span(bytes).subspan(0x230,0x808),
                                           at<std::int32_t>(bytes.data()+0x228))) {
            try_summon_event(navigation.run,at<std::uint32_t>(bytes.data()+0x21C),
                             at<std::uint32_t>(bytes.data()+0x180),
                             at<std::uint32_t>(bytes.data()+0x190));
        }
    }
}
__declspec(noinline) void __fastcall member_tick_hook(void* instance) noexcept {
    const hooking::CallGate::Scope call(g_gate);
    hooking::await_original(g_memberTick)(instance);
    if(call.accepts_side_effects()) { observe_boss_member(instance); }
}
void* target(std::uintptr_t rva,const std::array<std::uint8_t,16>& prefix) noexcept {
    auto* image=reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    std::array<std::byte,16> actual{};
    return image!=nullptr && copy(image+rva,actual)
        && std::memcmp(actual.data(),prefix.data(),prefix.size())==0 ? image+rva : nullptr;
}
bool idle() noexcept { return g_gate.idle(); }
} // namespace

bool install_omega_lair_cinematic() noexcept {
    if(g_handles[0].attached) { return g_gate.accepting(); }
    g_image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const auto lookup=reinterpret_cast<Lookup>(target(0xC4C1A0,
        {0x40,0x53,0x55,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x83,0xEC,0x50}));
    const auto issue=reinterpret_cast<IssueAction>(target(0xAB6C60,
        {0x48,0x89,0x5C,0x24,0x08,0x57,0x48,0x83,0xEC,0x20,0x48,0x8B,0xF9,0x48,0x81,0xC1}));
    const auto actorContext=reinterpret_cast<ActorContext>(target(0xA8CB20,
        {0x40,0x53,0x48,0x83,0xEC,0x20,0x89,0x51,0x04,0x48,0x8B,0xD9,0x48,0x8B,0x05,0xC5}));
    const auto resolve=reinterpret_cast<Resolve>(target(0x30CAB0,
        {0x83,0xF9,0xFF,0x74,0x47,0x8B,0xC1,0x81,0xE1,0xFF,0x1F,0x00,0x00,0xC1,0xF8,0x0D}));
    const std::array<std::uint8_t,16> eventPrefix{
        0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x7C,0x24,0x10,0x55,0x48,0x8B,0xEC,0x48,0x83};
    const auto add=reinterpret_cast<AnimationRequest>(target(0xC620F0,eventPrefix));
    const auto remove=reinterpret_cast<AnimationRequest>(target(0xC693F0,eventPrefix));
    const auto setVariable=reinterpret_cast<SetVariable>(target(0x576420,
        {0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x55,0x57,0x41,0x56,0x48,0x8D}));
    const auto worldPosition=reinterpret_cast<WorldPosition>(target(0x3F7C30,
        {0x48,0x89,0x5C,0x24,0x08,0x57,0x48,0x83,0xEC,0x40,0x0F,0x29,0x74,0x24,0x30,0x48}));
    const auto namedLookup=reinterpret_cast<NamedSequenceLookup>(target(0xA89430,
        {0x48,0x83,0xEC,0x28,0x8B,0x02,0x48,0x83,0xC1,0x10,0x48,0x8D,0x54,0x24,0x30,0x89}));
    const auto namedStart=reinterpret_cast<NamedSequenceStart>(target(0xC66590,
        {0x48,0x83,0xEC,0x38,0x45,0x0F,0xB6,0xD8,0x4C,0x8B,0xC2,0x48,0x85,0xD2,0x0F,0x84}));
    const auto namedStop=reinterpret_cast<NamedSequenceStop>(target(0xC6FE70,
        {0x48,0x89,0x74,0x24,0x18,0x48,0x89,0x7C,0x24,0x20,0x41,0x56,0x48,0x83,0xEC,0x20}));
    if(lookup==nullptr || issue==nullptr || actorContext==nullptr || resolve==nullptr
        || add==nullptr || remove==nullptr || setVariable==nullptr || worldPosition==nullptr
        || namedLookup==nullptr || namedStart==nullptr || namedStop==nullptr) { return false; }
    const std::array<hooking::detour::Spec,9> specs{{
        {target(0x106AB20,{0x40,0x53,0x48,0x83,0xEC,0x50,0x48,0x8B,0x05,0x5B,0xEF,0x03,0x01,0x48,0x33,0xC4}),reinterpret_cast<void*>(&tick_hook)},
        {target(0x10697B0,{0x40,0x53,0x48,0x83,0xEC,0x20,0x0F,0x10,0x02,0x44,0x8B,0x81,0x90,0x01,0x00,0x00}),reinterpret_cast<void*>(&apply_hook)},
        {target(0xDD1680,{0x4C,0x8B,0xDC,0x49,0x89,0x5B,0x10,0x49,0x89,0x73,0x18,0x55,0x57,0x41,0x57,0x49}),reinterpret_cast<void*>(&resource_hook)},
        {target(0xAB6600,{0x40,0x55,0x53,0x48,0x8D,0x6C,0x24,0xB8,0x48,0x81,0xEC,0x48,0x01,0x00,0x00,0x48}),reinterpret_cast<void*>(&member_tick_hook)},
        {target(0xC70180,{0x48,0x89,0x5C,0x24,0x10,0x48,0x89,0x6C,0x24,0x18,0x48,0x89,0x74,0x24,0x20,0x57}),reinterpret_cast<void*>(&character_init_hook)},
        {target(0xF4E660,{0x4C,0x8B,0xDC,0x55,0x41,0x54,0x41,0x55,0x49,0x8D,0x6B,0xA8,0x48,0x81,0xEC,0x40}),reinterpret_cast<void*>(&graph_update_hook)},
        {target(0x104B7A0,{0x48,0x89,0x5C,0x24,0x20,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57}),reinterpret_cast<void*>(&full_body_update_hook)},
        {target(0x10B0030,{0x40,0x55,0x53,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x57,0x48,0x8D,0x6C,0x24,0xD9}),reinterpret_cast<void*>(&motion_update_hook)},
        {target(0x10ADF70,{0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x74,0x24,0x20,0x57,0x48,0x81,0xEC,0x80,0x00}),reinterpret_cast<void*>(&motion_cleanup_hook)}
    }};
    if(!hooking::detour::install(specs,g_handles)) {
        core::log::write(core::log::Channel::client,core::log::Level::warn,"ev=omega_intro stage=install result=fail");
        return false;
    }
    g_tick.store(reinterpret_cast<Tick>(g_handles[0].original),std::memory_order_release); g_tick.notify_all();
    g_apply.store(reinterpret_cast<Apply>(g_handles[1].original),std::memory_order_release); g_apply.notify_all();
    g_resource.store(reinterpret_cast<Tick>(g_handles[2].original),std::memory_order_release); g_resource.notify_all();
    g_memberTick.store(reinterpret_cast<Tick>(g_handles[3].original),std::memory_order_release); g_memberTick.notify_all();
    g_characterInit.store(reinterpret_cast<CharacterInit>(g_handles[4].original),std::memory_order_release); g_characterInit.notify_all();
    g_updateGraph.store(reinterpret_cast<UpdateGraph>(g_handles[5].original),std::memory_order_release);g_updateGraph.notify_all();
    g_updateFullBody.store(reinterpret_cast<UpdateFullBody>(g_handles[6].original),std::memory_order_release);g_updateFullBody.notify_all();
    hooking::publish_original(g_updateMotion,reinterpret_cast<UpdateMotion>(g_handles[7].original));
    hooking::publish_original(g_cleanupMotion,reinterpret_cast<CleanupMotion>(g_handles[8].original));
    g_worldPosition.store(worldPosition,std::memory_order_release);
    g_setVariable.store(setVariable,std::memory_order_release);
    g_issueAction.store(issue,std::memory_order_release);
    g_actorContext.store(actorContext,std::memory_order_release);
    g_resolve.store(resolve,std::memory_order_release);
    g_addEvent.store(add,std::memory_order_release); g_removeEvent.store(remove,std::memory_order_release);
    g_lookup.store(lookup,std::memory_order_release);
    g_namedSequenceLookup.store(namedLookup,std::memory_order_release);g_namedSequenceStart.store(namedStart,std::memory_order_release);
    g_namedSequenceStop.store(namedStop,std::memory_order_release);
    g_gate.accept();
    core::log::write(core::log::Channel::client,core::log::Level::info,
        "ev=omega_intro stage=install result=ok trigger=A3928C71/60/4 controller=F4D0E0B2/6/22 boss_action=95FB2E01/2/1");
    return true;
}
void quiesce_omega_lair_cinematic() noexcept { g_gate.quiesce(); }
bool uninstall_omega_lair_cinematic() noexcept {
    quiesce_omega_lair_cinematic();
    if(!g_handles[0].attached) { return true; }
    const std::array<hooking::detour::ProtectedCodeEntry,18> protectedCode{{
        {reinterpret_cast<void*>(&tick_hook)},{reinterpret_cast<void*>(&apply_hook)},
        {reinterpret_cast<void*>(&resource_hook)},{reinterpret_cast<void*>(&observe)},
        {reinterpret_cast<void*>(&member_tick_hook)},{reinterpret_cast<void*>(&observe_boss_member)},
        {reinterpret_cast<void*>(&character_init_hook)},
        {reinterpret_cast<void*>(&graph_update_hook)},{reinterpret_cast<void*>(&full_body_update_hook)},
        {reinterpret_cast<void*>(&observe_lift)},
        {reinterpret_cast<void*>(&observe_crown_graph)},
        {reinterpret_cast<void*>(&observe_combat_graph)},
        {reinterpret_cast<void*>(&motion_update_hook)},{reinterpret_cast<void*>(&observe_motion_update)},
        {reinterpret_cast<void*>(&motion_cleanup_hook)},{reinterpret_cast<void*>(&observe_motion_cleanup)},
        {reinterpret_cast<void*>(&hooking::call_gate_detail::enter)},
        {reinterpret_cast<void*>(&hooking::call_gate_detail::leave)}
    }};
    if(hooking::detour::uninstall(g_handles,protectedCode,idle)!=hooking::detour::UninstallResult::removed) { return false; }
    g_tick.store(nullptr,std::memory_order_release); g_apply.store(nullptr,std::memory_order_release);
    g_resource.store(nullptr,std::memory_order_release); g_lookup.store(nullptr,std::memory_order_release);
    g_memberTick.store(nullptr,std::memory_order_release); g_issueAction.store(nullptr,std::memory_order_release);
    g_actorContext.store(nullptr,std::memory_order_release); g_resolve.store(nullptr,std::memory_order_release);
    g_characterInit.store(nullptr,std::memory_order_release);
    g_updateGraph.store(nullptr,std::memory_order_release);g_updateFullBody.store(nullptr,std::memory_order_release);
    g_updateMotion.store(nullptr,std::memory_order_release);g_cleanupMotion.store(nullptr,std::memory_order_release);
    g_worldPosition.store(nullptr,std::memory_order_release);
    g_motion={};g_introStopOwner={};g_motionRequest=0;g_motionSample=0;g_motionPending.store(false,std::memory_order_release);g_image=0;
    g_setVariable.store(nullptr,std::memory_order_release);g_waitingIntroIdle.store(false,std::memory_order_release);
    g_waitingIntroFlight.store(false,std::memory_order_release);g_waitingIntroSummon.store(false,std::memory_order_release);
    g_lift={};g_liftRequest=0;g_liftSample=0;
    g_crown={};g_crownRequest=0;g_crownSample=0;g_waitingCrown.store(false,std::memory_order_release);
    g_combat={};g_stunned={};g_eyeRefill={};g_eyeHold={};g_combatSample=0;g_combatStopping=false;g_combatPublishing=false;g_waitingCombat.store(false,std::memory_order_release);
    g_eyeThreshold={};g_eyeToken={};g_checkpointToken={};g_eyeLogged=false;g_healthUnavailableLogged=false;g_checkpointReported=false;g_eyeReported=false;
    g_namedSequenceLookup.store(nullptr,std::memory_order_release);g_namedSequenceStart.store(nullptr,std::memory_order_release);
    g_namedSequenceStop.store(nullptr,std::memory_order_release);
    omega_boss_health::reset();reset_intro_vfx();
    g_characterBinding.store(UINT64_MAX,std::memory_order_release);
    g_addEvent.store(nullptr,std::memory_order_release); g_removeEvent.store(nullptr,std::memory_order_release);
    g_summon={}; g_issuedIntro={};
    g_loggedOwnerActor=UINT32_MAX; g_loggedOwnerCheck=nullptr; g_ownerGuardLogs=0;
    g_resourceOwner.store(UINT32_MAX,std::memory_order_release); g_nextSample.store(0,std::memory_order_relaxed);
    g_endingResourceOwner.store(UINT32_MAX,std::memory_order_release);
    return true;
}
} // namespace sunrise::client::hooks::bootflow
