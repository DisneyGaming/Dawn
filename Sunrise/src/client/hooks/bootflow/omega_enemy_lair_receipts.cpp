#include <Windows.h>
#include <intrin.h>
#include <bit>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <mutex>

#include "omega_enemy_lair_receipts.h"
#include "mission_population_observer.h"
#include "hijacked_placements.h"
#include "../../../state/activity/gateway/runtime.h"
#include "../../../state/activity/deadly_trial/runtime.h"
#include "../../../state/activity/deep_storage/runtime.h"
#include "../../../state/activity/strike_pact/runtime.h"
#include "../../../state/activity/hijacked/runtime.h"
#include "native_population_pending.h"
#include "native_population_retirement.h"
#include "vance_contact_observer.h"
#include "../../../state/activity/strike_bond/runtime.h"
#include "omega_enemy_native_reference.h"
#include "omega_enemy_native_admission.h"
#include "omega_enemy_native_health.h"
#include "gateway_native_read.h"
#include "coo_enemy_readiness.h"
#include "strike_bond_fire_trace.h"
#include "strike_bond_carriage.h"
#include "coo_native_components.h"
#include "strike_bond_intro_release.h"
#include "strike_bond_boss_cycle.h"
#include "strike_bond_boss_shield.h"
#include "strike_bond_boss_retirement.h"
#include "omega_vex_lattice_probe.h"
#include "strike_bond_target_binding.h"
#include "omega_boss_health.h"
#include "../../hooking/call_gate.h"
#include "../../hooking/detour.h"
#include "../../../core/logging/log.h"
#include "../../../state/activity/omega_enemy_lair_catalog.h"
#include "../../../state/activity/omega_enemy_crown_catalog.h"
#include "../../../state/activity/omega_first_lair_runtime.h"
#include "../../../state/activity/omega_presentation.h"
#include "../../../state/activity/native_population_events.h"

namespace sunrise::client::hooks::bootflow {
namespace {
namespace catalog=state::activity::omega_enemy_lair;
namespace crownCatalog=state::activity::omega_enemy_crown;
namespace gateway=state::activity::gateway;
namespace trial=state::activity::deadly_trial;
namespace hijacked=state::activity::hijacked;
namespace deep=state::activity::deep_storage;
namespace strike=state::activity::strike_pact;
namespace garden=state::activity::strike_bond;
struct Context final { bool enabled;std::uint64_t run;bool gateway;bool trial{};bool deep{};bool strike{};bool hijacked{};bool garden{}; };
Context selected_context() noexcept {
    const auto gardenRun=garden::native_run();if(gardenRun) return {true,gardenRun,false,false,false,false,false,true};
    const auto hijackedRun=hijacked::native_run();if(hijackedRun) {return {true,hijackedRun,false,false,false,false,true};}
    const auto deepRun=deep::native_run();if(deepRun) {return {true,deepRun,false,false,true};}
    const auto strikeRun=strike::native_run();if(strikeRun) {return {true,strikeRun,false,false,false,true};}
    const auto trialRun=trial::native_run();if(trialRun) { return {true,trialRun,false,true}; }
    const auto run=gateway::native_run();if(run!=0) { return {true,run,true}; }
    const auto nav=state::activity::omega_presentation::navigation();return {nav.enabled,nav.run,false};
}

using Admission=std::uint64_t(__fastcall*)(void*,const void*) noexcept;
using CandidateEvent=std::uint64_t(__fastcall*)(void*,std::uint32_t) noexcept;
using Retirement=void(__fastcall*)(std::uint32_t,std::uint8_t) noexcept;
hooking::CallGate g_gate;
std::array<hooking::detour::Handle,9> g_handles{};
std::atomic<Admission> g_admission{};
std::atomic<CandidateEvent> g_candidate{};
std::atomic<Retirement> g_retirement{};
std::uintptr_t g_image{};
SRWLOCK g_lock=SRWLOCK_INIT;
std::uint64_t g_run{UINT64_MAX};
unsigned g_lines{},g_seenCount{};
struct Seen final {std::uint32_t actor{},registry{};std::uint16_t source{};};
// A full run now has143 planned admissions including the nonblocking Cabal
// escape. This remains a bounded diagnostic set; state delivery occurs first.
std::array<Seen,256> g_seen{};
std::uint32_t g_candidateRejected{};
std::uint32_t g_parentRejected{};
constexpr unsigned kLineLimit=768;
constexpr std::size_t kCopyLimit=8192;
// Bounded post-identity rejection evidence: one line per boundary/reason/
// registry/source per run. Lines never change delivery or native returns.
struct Reject final {std::uint8_t boundary{},reason{};std::uint32_t registry{};std::uint16_t source{};};
std::array<Reject,64> g_rejects{};unsigned g_rejectCount{};bool g_rejectOverflow{};
constexpr std::uint8_t kBoundaryAdmission=1,kBoundaryDeath=2;
enum : std::uint8_t {
    kRejectRunChanged=1,kRejectStateAdmission,kRejectEventInvalid,kRejectEventClass,
    kRejectHealthInvalid,kRejectDeathBitUnset,kRejectGeneration,kRejectGateClosed,kRejectStateDeath
};
constexpr const char* reject_name(std::uint8_t reason) noexcept {
    switch(reason) {
    case kRejectRunChanged: return "run_changed";
    case kRejectStateAdmission: return "state_admission";
    case kRejectEventInvalid: return "event_invalid";
    case kRejectEventClass: return "event_class";
    case kRejectHealthInvalid: return "health_invalid";
    case kRejectDeathBitUnset: return "death_bit_unset";
    case kRejectGeneration: return "generation";
    case kRejectGateClosed: return "gate_closed";
    case kRejectStateDeath: return "state_death";
    default: return "unknown";
    }
}
[[nodiscard]] bool first_reject(std::uint8_t boundary,std::uint8_t reason,std::uint32_t registry,std::uint16_t source) noexcept {
    for(unsigned i=0;i<g_rejectCount;++i) {
        const auto& seen=g_rejects[i];
        if(seen.boundary==boundary && seen.reason==reason && seen.registry==registry && seen.source==source) {return false;}
    }
    if(g_rejectCount>=g_rejects.size()) {g_rejectOverflow=true;return false;}
    g_rejects[g_rejectCount++]={boundary,reason,registry,source};return true;
}

template<class T> T at(const std::byte* bytes) noexcept {
    T value{};std::memcpy(&value,bytes,sizeof value);return value;
}
struct Ref final {std::uint32_t handle{UINT32_MAX},kind{};std::int64_t offset{};};
static_assert(sizeof(Ref)==16);
bool add(std::uintptr_t address,std::int64_t offset,std::uintptr_t& result) noexcept {
    if(offset>=0) {
        if(static_cast<std::uint64_t>(offset)>UINTPTR_MAX-address) {return false;}
        result=address+static_cast<std::uintptr_t>(offset);
    } else {
        const auto magnitude=static_cast<std::uint64_t>(-(offset+1))+1U;
        if(magnitude>address) {return false;}result=address-static_cast<std::uintptr_t>(magnitude);
    }
    return result>=0x10000;
}
struct Read final {
    std::size_t copied{};
    bool copy(std::uintptr_t source,std::span<std::byte> destination) noexcept {
        if(source<0x10000 || source>UINTPTR_MAX-destination.size()
            || copied>kCopyLimit || destination.size()>kCopyLimit-copied) {return false;}
        copied+=destination.size();SIZE_T size{};
        return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(source),
            destination.data(),destination.size(),&size) && size==destination.size();
    }
    template<class T> bool value(std::uintptr_t source,T& value) noexcept {
        return copy(source,std::as_writable_bytes(std::span{&value,std::size_t{1}}));
    }
    bool resolve(const Ref& reference,std::uintptr_t& output) noexcept {
        if(reference.handle==UINT32_MAX) {return false;}
        std::uintptr_t directory{},registry{};
        if(!value(g_image+0x2439C70,directory) || !value(directory,registry)) {return false;}
        const auto shifted=static_cast<std::uint32_t>(static_cast<std::int32_t>(reference.handle)>>13);
        const auto index=((static_cast<std::uint64_t>(shifted)|0xFFC0000ULL)>>18)&(shifted&0xFFFFU);
        std::uintptr_t table{};
        if(!add(registry,static_cast<std::int64_t>(index*0x40),table)) {return false;}
        std::array<std::byte,0x38> bytes{};if(!copy(table,bytes)) {return false;}
        const auto stride=at<std::int32_t>(bytes.data()+0x30);
        const auto mask=at<std::int32_t>(bytes.data()+0x34);
        std::uintptr_t element{};
        if(stride<=0 || stride>0x100000 || !add(at<std::uintptr_t>(bytes.data()+8),
            static_cast<std::int64_t>(reference.handle&0x1FFFU)*stride,element)) {return false;}
        std::uint64_t relocation{};
        if(element>UINTPTR_MAX-8 || !value(element+8,relocation)) {return false;}
        const auto base=omega_enemy_native_reference::corrected_base(element,relocation,mask);
        return add(static_cast<std::uintptr_t>(base),reference.offset,output);
    }
};
struct Actor final {
    std::uint32_t handle{},entity{UINT32_MAX},parent{UINT32_MAX};
    Ref source{},member{};
    std::uint16_t flags{};
    // Sub-reason when the table walk fails (A0D510 stores the created actor in
    // the 0x1F9D7F8/0x1F9D800 table; +0x48 is its own full handle).
    std::uint8_t reason{};std::uint32_t self{UINT32_MAX};
};
bool actor(Read& read,std::uint32_t handle,Actor& actor) noexcept {
    actor.reason=0;actor.self=UINT32_MAX;
    if(handle==UINT32_MAX) {actor.reason=1;return false;}
    std::uintptr_t base{};std::int32_t stride{};
    if(!read.value(g_image+0x1F9D7F8,base) || !read.value(g_image+0x1F9D800,stride)
        || stride<0x70 || stride>0x100000) {actor.reason=2;return false;}
    std::uintptr_t address{};
    if(!add(base,static_cast<std::int64_t>(handle&0x1FFFU)*stride,address)) {actor.reason=3;return false;}
    std::array<std::byte,0x70> bytes{};
    if(!read.copy(address,bytes)) {actor.reason=4;return false;}
    actor.self=at<std::uint32_t>(bytes.data()+0x48);
    if(actor.self!=handle) {actor.reason=5;return false;}
    actor.handle=handle;actor.flags=at<std::uint16_t>(bytes.data());
    actor.entity=at<std::uint32_t>(bytes.data()+0x4C);
    actor.parent=at<std::uint32_t>(bytes.data()+0x50);
    actor.source=at<Ref>(bytes.data()+0x38);actor.member=at<Ref>(bytes.data()+0x60);
    return true;
}
struct Source final {
    std::uint16_t slot{};
    std::uint32_t registry{},resource{},kind{},generation{},senseGeneration{};
    std::uintptr_t address{};
    // Sub-reason and the native scoped identity actually read at definition+0x30
    // (registry, type, slot). Cycle-2/3 registries share class 8080948F and
    // definition offset 728 with the accepted BF06 sources; a mismatch here names
    // the exact field instead of a bare "reason=7".
    std::uint8_t reason{};
    std::uint32_t nativeRegistry{};std::uint8_t nativeType{};std::int16_t nativeSlot{-1};
    std::uint32_t expectedRegistry{};std::uint16_t expectedSlot{};
};
enum : std::uint8_t {
    kSourceDefinitionRef=1,kSourceUnknownResource,kSourceResolve,kSourceRegistry,
    kSourceType,kSourceSlot,kSourceGenerationRead
};
bool source(Read& read,std::uintptr_t instance,Source& source,bool gatewayContext,bool trialContext,bool deepContext,bool strikeContext,bool hijackedContext,bool gardenContext) noexcept {
    source.reason=0;source.nativeRegistry=0;source.nativeType=0;source.nativeSlot=-1;
    source.expectedRegistry=0;source.expectedSlot=0;
    Ref definition{};
    if(!read.value(instance,definition) || definition.kind!=0x8080948FU || (!gatewayContext && !trialContext && !deepContext && !strikeContext && !hijackedContext && !gardenContext && definition.offset!=0x728)) {
        source.resource=definition.handle;source.kind=definition.kind;source.reason=kSourceDefinitionRef;return false;
    }
    source.resource=definition.handle;source.kind=definition.kind;
    std::uint32_t expectedRegistry{};
    std::uint16_t expectedSlot{};
    if(deepContext) {
        for(const auto& row:deep::kSpawns) {
            if(row.definition==definition.handle && row.offset==definition.offset) {expectedRegistry=row.registry;expectedSlot=row.source;break;}
        }
        if(!expectedRegistry) {source.reason=kSourceUnknownResource;return false;}
    } else if(gardenContext) {
        for(const auto& row:garden::kAssets) {
            if(row.asset.type==1 && row.asset.definition==definition.handle && row.offset==definition.offset) {
                expectedRegistry=row.asset.registry;expectedSlot=row.asset.slot;break;
            }
        }
        if(!expectedRegistry) {source.reason=kSourceUnknownResource;return false;}
    } else if(strikeContext) {
        // The strike catalog pins sources by their native scoped identity (registry, type 1,
        // slot) rather than by descriptor tag/offset, which the SDK export does not carry.
        std::uintptr_t scopedDefinition{};std::array<std::byte,8> scoped{};
        if(!read.resolve(definition,scopedDefinition) || scopedDefinition>UINTPTR_MAX-0x30
            || !read.copy(scopedDefinition+0x30,scoped)) {source.reason=kSourceResolve;return false;}
        const auto registry=at<std::uint32_t>(scoped.data());const auto type=at<std::uint8_t>(scoped.data()+4);
        const auto slot=at<std::int16_t>(scoped.data()+6);
        // all_spawn, not spawn: the latter searches the opening's own thirteen rows, so an actor
        // from any later section resolved to nothing and was rejected as an unknown resource.
        const auto* row=type==1 && slot>=0?strike::all_spawn(registry,static_cast<std::uint16_t>(slot)):nullptr;
        if(!row) {source.nativeRegistry=registry;source.nativeType=type;source.nativeSlot=slot;source.reason=kSourceUnknownResource;return false;}
        expectedRegistry=row->registry;expectedSlot=row->source;
    }
    if(hijackedContext) {
        for(const auto& row:hijacked::kSpawns) {
            if(row.definition==definition.handle && row.offset==definition.offset) {expectedRegistry=row.registry;expectedSlot=row.source;break;}
        }
        if(!expectedRegistry) {source.reason=kSourceUnknownResource;return false;}
    }
    if(trialContext) {
        for(const auto& row:trial::kSpawns) {
            if(row.definition==definition.handle && row.offset==definition.offset) { expectedRegistry=row.registry;expectedSlot=row.source;break; }
        }
        if(!expectedRegistry) { source.reason=kSourceUnknownResource;return false; }
    }
    if(gatewayContext) {
        for(const auto& row:gateway::kSpawns) {
            if(row.definition==definition.handle && row.offset==definition.offset) { expectedRegistry=row.registry;expectedSlot=row.source;break; }
        }
        if(expectedRegistry==0) { source.reason=kSourceUnknownResource;return false; }
    }
    for(const auto& row:catalog::kSpawners) {
        if(!gatewayContext && !trialContext && !deepContext && !strikeContext && !hijackedContext && !gardenContext && catalog::supported_by_encounter(row) && row.resource==definition.handle) {
            expectedRegistry=catalog::kRegistry;expectedSlot=row.slot;break;
        }
    }
    if(expectedRegistry==0) {
        for(const auto& registry:crownCatalog::kRegistries) {
            for(const auto& row:registry.spawners) {
                if(row.resource==definition.handle) {expectedRegistry=registry.key;expectedSlot=row.slot;break;}
            }
            if(expectedRegistry!=0) {break;}
        }
    }
    if(expectedRegistry==0) {source.reason=kSourceUnknownResource;return false;}
    source.expectedRegistry=expectedRegistry;source.expectedSlot=expectedSlot;
    std::uintptr_t nativeDefinition{};
    if(!read.resolve(definition,nativeDefinition) || nativeDefinition>UINTPTR_MAX-0x30) {source.reason=kSourceResolve;return false;}
    std::array<std::byte,8> scoped{};
    if(!read.copy(nativeDefinition+0x30,scoped)) {source.reason=kSourceResolve;return false;}
    source.nativeRegistry=at<std::uint32_t>(scoped.data());
    source.nativeType=at<std::uint8_t>(scoped.data()+4);
    source.nativeSlot=at<std::int16_t>(scoped.data()+6);
    if(source.nativeRegistry!=expectedRegistry) {source.reason=kSourceRegistry;return false;}
    if(source.nativeType!=1) {source.reason=kSourceType;return false;}
    if(source.nativeSlot!=expectedSlot) {source.reason=kSourceSlot;return false;}
    if(instance>UINTPTR_MAX-0x244 || !read.value(instance+0x1FC,source.generation)
        || !read.value(instance+0x244,source.senseGeneration)) {source.reason=kSourceGenerationRead;return false;}
    source.registry=expectedRegistry;source.slot=expectedSlot;source.address=instance;
    return true;
}
#include "hijacked_population_logging.inl"

template<class... Args> void report(const char* format,Args... args) noexcept {
    if(g_lines>=kLineLimit) {return;}
    std::array<char,768> message{};const int count=std::snprintf(message.data(),message.size(),format,args...);
    if(count>0 && static_cast<std::size_t>(count)<message.size()) {
        ++g_lines;core::log::write(core::log::Channel::client,core::log::Level::info,
            {message.data(),static_cast<std::size_t>(count)});
    }
}
void run(std::uint64_t run) noexcept {
    if(g_run!=run) {
        g_run=run;g_lines=0;g_seenCount=0;g_seen={};g_candidateRejected=0;g_parentRejected=0;
        g_rejects={};g_rejectCount=0;g_rejectOverflow=false;
    }
}
/** Caller holds g_lock. Emits one bounded line per new boundary/reason/source key
 * and one overflow notice per run when the table fills. Observe-only. */
void reject(std::uint64_t run,std::uint8_t boundary,std::uint8_t reason,std::uint32_t registry,
            std::uint16_t source,std::uint32_t actor,std::uint32_t detail0,std::uint32_t detail1) noexcept {
    const bool wasOverflow=g_rejectOverflow;
    if(first_reject(boundary,reason,registry,source)) {
        report("ev=omega_enemy_lair stage=reject boundary=%s reason=%s run=%llu registry=%08X source=%u actor=%08X detail0=%08X detail1=%08X",
            boundary==kBoundaryAdmission?"A0D510":"C72390",reject_name(reason),
            static_cast<unsigned long long>(run),registry,source,actor,detail0,detail1);
    } else if(g_rejectOverflow && !wasOverflow) {
        report("ev=omega_enemy_lair stage=reject boundary=%s reason=overflow run=%llu capacity=%zu",
            boundary==kBoundaryAdmission?"A0D510":"C72390",static_cast<unsigned long long>(run),g_rejects.size());
    }
}

/** A0D510 copies source/member backlinks directly from its spawn context. The
 * callback pointer may have moved, so resolve the pre-call full selfhandle anew. */
__declspec(noinline) void observe_admission(std::uint32_t parent,std::uint64_t callRun) noexcept {
    const auto nav=selected_context();
    if(!nav.enabled || nav.run==0) {return;}
    if(nav.run!=callRun) {
        // The run advanced between the pre-call identity copy and this callback.
        // Nothing is admitted; record it once per new run so a silent gap is visible.
        if(!TryAcquireSRWLockExclusive(&g_lock)) {return;}
        run(nav.run);
        reject(nav.run,kBoundaryAdmission,kRejectRunChanged,0,0,UINT32_MAX,parent,
            static_cast<std::uint32_t>(callRun));
        ReleaseSRWLockExclusive(&g_lock);return;
    }
    Read read;read.copied=0x28; // Include the pre-original identity copy in this callback's budget.
    Actor actorState;Source sourceState;Ref definition{};
    std::uintptr_t currentParent{},parentAgain{},linked{};
    std::array<std::byte,0x28> parentHeader{};std::uint32_t handle{UINT32_MAX};
    unsigned rejection{};
    if(!read.resolve({parent,0,0},currentParent)) {rejection=1;}
    else if(!read.copy(currentParent,parentHeader)
        || at<std::uint32_t>(parentHeader.data()+4)!=0x808082ECU
        || at<std::uint32_t>(parentHeader.data()+0x24)!=parent) {rejection=2;}
    else if(currentParent>UINTPTR_MAX-0x1470 || !read.value(currentParent+0x1470,handle)) {rejection=3;}
    else if(!actor(read,handle,actorState)) {rejection=4;}
    else if(!omega_enemy_native_admission::identity(parent,
        at<std::uint32_t>(parentHeader.data()+4),at<std::uint32_t>(parentHeader.data()+0x24),
        handle,actorState.handle,actorState.parent)
        || !read.resolve({actorState.parent,0,0},parentAgain) || parentAgain!=currentParent) {rejection=5;}
    else if(!read.resolve(actorState.source,linked)) {rejection=6;}
    else if(!read.value(linked,definition) || !source(read,linked,sourceState,nav.gateway,nav.trial,nav.deep,nav.strike,nav.hijacked,nav.garden)) {rejection=7;}
    else if(sourceState.generation==0 || sourceState.generation!=sourceState.senseGeneration) {rejection=8;}

    // Retain the authentic actor/AI-parent origin before progression can detach
    // its source. Entity readiness may arrive later on the placement poll.
    // A generation mismatch still has a verified source; detached unknown actors do not.
    if(nav.hijacked && (rejection==0 || rejection==8) && sourceState.registry==0x3E9B74F3U) {
        hijacked_placements::retain_enemy(nav.run,sourceState.slot,actorState.source.handle,handle);
    }
    // Progression delivery must never depend on the best-effort diagnostic lock.
    // The state observer guarantees serialization, validates the current run,
    // deduplicates full actor IDs, and fails closed on unexpected population.
    bool accepted=false;
    if(rejection==0) {
        accepted=nav.garden?garden::observe_admission(
            {nav.run,handle,actorState.source.handle,sourceState.generation,sourceState.slot,sourceState.registry})
            :nav.hijacked?hijacked::observe_admission(
            {nav.run,handle,actorState.source.handle,sourceState.generation,sourceState.slot,sourceState.registry})
            :nav.deep?deep::observe_admission(
            {nav.run,handle,actorState.source.handle,sourceState.generation,sourceState.slot,sourceState.registry})
            :nav.strike?strike::observe_admission(
            {nav.run,handle,actorState.source.handle,sourceState.generation,sourceState.slot,sourceState.registry})
            :nav.trial?trial::observe_admission(
            {nav.run,handle,actorState.source.handle,sourceState.generation,sourceState.slot,sourceState.registry})
            :nav.gateway?gateway::observe_admission(
            {nav.run,handle,actorState.source.handle,sourceState.generation,sourceState.slot,sourceState.registry})
            :state::activity::omega_first_lair::observe_admission(
            {nav.run,handle,actorState.source.handle,sourceState.generation,sourceState.slot,sourceState.registry});
    }
    if(nav.deep && rejection==0) {
        gateway_native::Read probe{g_image};
        const deep::EnemyReceipt receipt{nav.run,handle,actorState.source.handle,sourceState.generation,sourceState.slot,sourceState.registry};
        deep::observe_readiness(receipt,coo_native::enemy(probe,g_image,receipt));
    }
    if(nav.hijacked && rejection==0) {
        gateway_native::Read probe{g_image};
        const hijacked::EnemyReceipt receipt{nav.run,handle,actorState.source.handle,sourceState.generation,sourceState.slot,sourceState.registry};
        hijacked::observe_readiness(receipt,coo_native::enemy(probe,g_image,receipt));
    }
    if(nav.trial && rejection==0) {
        gateway_native::Read probe{g_image};
        const trial::EnemyReceipt receipt{nav.run,handle,actorState.source.handle,sourceState.generation,sourceState.slot,sourceState.registry};
        trial::observe_readiness(receipt,coo_native::enemy(probe,g_image,receipt));
    }
    if(nav.gateway && rejection==0) {
        gateway_native::Read probe{g_image};
        const gateway::EnemyReceipt receipt{nav.run,handle,actorState.source.handle,sourceState.generation,sourceState.slot,sourceState.registry};
        gateway::observe_readiness(receipt,coo_native::enemy(probe,g_image,receipt));
    }
    if(nav.strike && rejection==0) {
        gateway_native::Read probe{g_image};
        const strike::EnemyReceipt receipt{nav.run,handle,actorState.source.handle,sourceState.generation,sourceState.slot,sourceState.registry};
        strike::observe_readiness(receipt,coo_native::enemy(probe,g_image,receipt));
    }
    if(!TryAcquireSRWLockExclusive(&g_lock)) {return;}
    run(nav.run);
    if(nav.hijacked && (rejection==0 || rejection>=6))
        trace_hijacked_birth(nav.run,actorState,sourceState,at<std::uint32_t>(parentHeader.data()),rejection,accepted);
    if(rejection==0) {
        bool seen=false;
        for(unsigned i=0;i<g_seenCount;++i) {
            seen|=g_seen[i].actor==handle && g_seen[i].source==sourceState.slot && g_seen[i].registry==sourceState.registry;
        }
        if(!seen && g_seenCount<g_seen.size()) {
            g_seen[g_seenCount++]={handle,sourceState.registry,sourceState.slot};
            report("ev=omega_enemy_lair stage=admitted run=%llu registry=%08X source=%u resource=%08X kind=%08X actor=%08X entity=%08X parent=%08X source_ref=%08X member_ref=%08X generation=%u native_flags=%04X copied=%zu boundary=A0D510 accepted=%u",
                static_cast<unsigned long long>(nav.run),sourceState.registry,sourceState.slot,sourceState.resource,sourceState.kind,
                handle,actorState.entity,actorState.parent,actorState.source.handle,actorState.member.handle,
                sourceState.generation,actorState.flags,read.copied,accepted?1U:0U);
        }
        if(!accepted) {
            // A fully identified native creation the encounter state refused
            // (wrong run/generation, source not enabled for the current wave,
            // duplicate actor, or over-population). Once per registry/source.
            reject(nav.run,kBoundaryAdmission,kRejectStateAdmission,sourceState.registry,sourceState.slot,
                handle,sourceState.generation,actorState.source.handle);
        }
    } else {
        // Unscoped creation is common outside this encounter. These records are
        // only bounded rejection evidence, not admissions or missing enemies.
        // sub= names the failing field inside actor()/source(); native_* echo the
        // scoped identity actually read at definition+30 for cross-registry checks.
        const auto flag=std::uint32_t{1}<<rejection;
        if((g_parentRejected&flag)==0) {
            g_parentRejected|=flag;
            report("ev=omega_enemy_lair stage=admission_rejected run=%llu reason=%u sub=%u parent=%08X current_parent=%p parent_kind=%08X parent_self=%08X actor=%08X actor_self=%08X actor_parent=%08X source_ref=%08X resource=%08X linked=%p native_registry=%08X native_type=%u native_slot=%d expected_registry=%08X expected_slot=%u generation=%u sense_generation=%u copied=%zu boundary=A0D510",
                static_cast<unsigned long long>(nav.run),rejection,
                rejection==4?actorState.reason:rejection==7?sourceState.reason:0U,
                parent,reinterpret_cast<void*>(currentParent),
                at<std::uint32_t>(parentHeader.data()+4),at<std::uint32_t>(parentHeader.data()+0x24),handle,
                actorState.self,actorState.parent,actorState.source.handle,definition.handle,reinterpret_cast<void*>(linked),
                sourceState.nativeRegistry,sourceState.nativeType,sourceState.nativeSlot,
                sourceState.expectedRegistry,sourceState.expectedSlot,
                sourceState.generation,sourceState.senseGeneration,read.copied);
        }
    }
    if(rejection==8) {
        // A catalogued Crown/Lair source whose authority generation has not been
        // reflected yet (or is retiring). This is the cycle-2/3 hazard: a later
        // registry whose sense generation lags its auth generation. Once per source.
        reject(nav.run,kBoundaryAdmission,kRejectGeneration,sourceState.registry,sourceState.slot,
            handle,sourceState.generation,sourceState.senseGeneration);
    }
    ReleaseSRWLockExclusive(&g_lock);
}

/** The original C72390 resolves its event this way through 9ECC70. Perform only
 * bounded copies here, without invoking its validation/dispatch helper. */
bool event_payload(Read& read,std::uint32_t event,std::array<std::byte,0x3C>& header,
                    std::array<std::byte,0x38>& payload,std::uint32_t& definition) noexcept {
    std::uintptr_t allocator{},buffer{},address{};
    if(!read.value(g_image+0x274F778,allocator) || allocator>UINTPTR_MAX-0x10
        || !read.value(allocator+0x10,buffer) || !add(buffer,event&0xFFFFFU,address)
        || !read.copy(address,header)) {return false;}
    definition=at<std::uint32_t>(header.data()+0x34);
    std::uintptr_t descriptor{};std::uint8_t alignment{};
    if(!read.resolve({definition,0,0},descriptor) || descriptor>UINTPTR_MAX-0x18
        || !read.value(descriptor+0x18,alignment) || alignment==0 || alignment>64
        || (alignment&(alignment-1U))!=0 || address>UINTPTR_MAX-0x3B-alignment) {return false;}
    const auto body=(address+0x3B+alignment)&~static_cast<std::uintptr_t>(alignment-1U);
    return read.copy(body,payload);
}
__declspec(noinline) void observe_candidate(void* instance,std::uint32_t event,
    const hooking::CallGate::Scope& call) noexcept {
    const auto nav=selected_context();
    if(!nav.enabled || nav.run==0) {return;}
    Read read;std::array<std::byte,0xC4> character{};Actor actorState;Source sourceState;
    std::uintptr_t linked{},characterAddress{};
    const auto address=reinterpret_cast<std::uintptr_t>(instance);
    unsigned rejection{};
    if(!read.copy(address,character)) {rejection=1;}
    else if(at<std::uint32_t>(character.data()+4)!=0x80806832U) {rejection=2;}
    else if(!actor(read,at<std::uint32_t>(character.data()+0xC0),actorState)) {rejection=3;}
    else if(!read.resolve({at<std::uint32_t>(character.data()+0x24),0,0},characterAddress)) {rejection=4;}
    else if(characterAddress!=address) {rejection=5;}
    else if(!read.resolve(actorState.source,linked)) {rejection=6;}
    else if(!source(read,linked,sourceState,nav.gateway,nav.trial,nav.deep,nav.strike,nav.hijacked,nav.garden)) {rejection=7;}
    std::array<std::byte,0x3C> eventHeader{};std::array<std::byte,0x38> payload{};
    std::uint32_t eventDefinition{};bool eventValid=false,healthValid=false,deathAccepted=false;
    Ref healthRef{};std::uintptr_t healthAddress{},memberAddress{};
    std::array<std::byte,0x340> health{};std::array<std::byte,0x194> member{};bool memberValid=false;
    std::uint8_t deathReject{};
    if(rejection==0) {
        eventValid=event_payload(read,event,eventHeader,payload,eventDefinition);
        healthValid=omega_enemy_native_health::owner(actorState.entity,
                at<std::uint32_t>(character.data()+0x2C))
            && address<=UINTPTR_MAX-0x2E8
            && read.value(address+0x2E8,healthRef)
            && healthRef.handle!=UINT32_MAX && healthRef.kind==0x80804BEEU && healthRef.offset==0
            && read.resolve(healthRef,healthAddress) && read.copy(healthAddress,health)
            && omega_enemy_native_health::identity(healthRef.handle,healthRef.kind,healthRef.offset,
                at<std::uint32_t>(health.data()+4),at<std::uint32_t>(health.data()+0x24),
                actorState.entity,at<std::uint32_t>(health.data()+0x2C));
        memberValid=actorState.member.handle!=UINT32_MAX && read.resolve(actorState.member,memberAddress)
            && read.copy(memberAddress,member);
        // Deliver once through the encounter state lock, before optional logs.
        // No native reads or calls execute while that lock is held. The state
        // validates the complete original admission, run and generation again.
        const auto healthFlags=at<std::uint8_t>(health.data()+0x338);
        const bool qualified=omega_enemy_native_health::death(eventValid,eventDefinition,healthValid,
            healthFlags,sourceState.generation,sourceState.senseGeneration);
        if(qualified && call.accepts_side_effects()) {
            deathAccepted=nav.garden?garden::observe_death(
                {nav.run,actorState.handle,actorState.source.handle,sourceState.generation,sourceState.slot,sourceState.registry})
            :nav.hijacked?hijacked::observe_death(
                {nav.run,actorState.handle,actorState.source.handle,sourceState.generation,sourceState.slot,sourceState.registry})
            :nav.deep?deep::observe_death(
                {nav.run,actorState.handle,actorState.source.handle,sourceState.generation,sourceState.slot,sourceState.registry})
            :nav.strike?strike::observe_death(
                {nav.run,actorState.handle,actorState.source.handle,sourceState.generation,sourceState.slot,sourceState.registry})
                :nav.trial?trial::observe_death(
                {nav.run,actorState.handle,actorState.source.handle,sourceState.generation,sourceState.slot,sourceState.registry})
                :nav.gateway?gateway::observe_death(
                {nav.run,actorState.handle,actorState.source.handle,sourceState.generation,sourceState.slot,sourceState.registry})
                :state::activity::omega_first_lair::observe_death(
                {nav.run,actorState.handle,actorState.source.handle,sourceState.generation,sourceState.slot,sourceState.registry});
            if(!deathAccepted) {deathReject=kRejectStateDeath;}
        } else if(qualified) {deathReject=kRejectGateClosed;}
        else {
            // Name the first failing qualification exactly as death() orders them.
            using omega_enemy_native_health::DeathRejection;
            switch(omega_enemy_native_health::death_rejection(eventValid,eventDefinition,healthValid,
                healthFlags,sourceState.generation,sourceState.senseGeneration)) {
            case DeathRejection::eventInvalid: deathReject=kRejectEventInvalid;break;
            case DeathRejection::eventClass: deathReject=kRejectEventClass;break;
            case DeathRejection::healthInvalid: deathReject=kRejectHealthInvalid;break;
            case DeathRejection::deathBitUnset: deathReject=kRejectDeathBitUnset;break;
            case DeathRejection::generation: deathReject=kRejectGeneration;break;
            case DeathRejection::none: break;
            }
        }
    }
    if(!TryAcquireSRWLockExclusive(&g_lock)) {return;}
    run(nav.run);
    if(rejection==0) {
        if(deathReject!=0) {
            reject(nav.run,kBoundaryDeath,deathReject,sourceState.registry,sourceState.slot,actorState.handle,
                deathReject==kRejectEventClass?eventDefinition:
                deathReject==kRejectGeneration?sourceState.generation:
                deathReject==kRejectDeathBitUnset?at<std::uint8_t>(health.data()+0x338):event,
                deathReject==kRejectGeneration?sourceState.senseGeneration:actorState.source.handle);
        }
        if(deathAccepted) {
            report("ev=omega_enemy_lair stage=death_accepted run=%llu registry=%08X source=%u actor=%08X source_ref=%08X generation=%u event=%08X health=%08X",
                static_cast<unsigned long long>(nav.run),sourceState.registry,sourceState.slot,actorState.handle,
                actorState.source.handle,sourceState.generation,event,healthRef.handle);
        }
        report("ev=omega_enemy_lair stage=candidate_event run=%llu source=%u resource=%08X actor=%08X entity=%08X character=%08X character_definition=%08X source_ref=%08X member_ref=%08X member_valid=%u member_definition=%08X member_generation=%u member_revision=%u generation=%u native_flags=%04X event=%08X event_valid=%u event_definition=%08X context=%016llX payload00=%08X payload04=%08X payload08=%08X header00=%08X header04=%08X header08=%08X copied=%zu",
            static_cast<unsigned long long>(nav.run),sourceState.slot,sourceState.resource,actorState.handle,
            actorState.entity,at<std::uint32_t>(character.data()+0x24),at<std::uint32_t>(character.data()),
            actorState.source.handle,actorState.member.handle,memberValid?1U:0U,at<std::uint32_t>(member.data()),
            at<std::uint32_t>(member.data()+0x180),at<std::uint32_t>(member.data()+0x190),sourceState.generation,
            actorState.flags,event,eventValid?1U:0U,eventDefinition,
            static_cast<unsigned long long>(at<std::uint64_t>(payload.data()+0x2C)),
            at<std::uint32_t>(payload.data()),at<std::uint32_t>(payload.data()+4),at<std::uint32_t>(payload.data()+8),
            at<std::uint32_t>(eventHeader.data()),at<std::uint32_t>(eventHeader.data()+4),
            at<std::uint32_t>(eventHeader.data()+8),read.copied);
        report("ev=omega_enemy_lair stage=candidate_health run=%llu source=%u actor=%08X event=%08X health_valid=%u health_ref=%08X health_ref_kind=%08X health_definition=%08X health_kind=%08X health_self=%08X health_entity=%08X native_320=%016llX native_328=%016llX native_330=%016llX native_338=%016llX payload_qwords=%016llX,%016llX,%016llX,%016llX,%016llX,%016llX,%016llX copied=%zu",
            static_cast<unsigned long long>(nav.run),sourceState.slot,actorState.handle,event,healthValid?1U:0U,
            healthRef.handle,healthRef.kind,at<std::uint32_t>(health.data()),
            at<std::uint32_t>(health.data()+4),at<std::uint32_t>(health.data()+0x24),
            at<std::uint32_t>(health.data()+0x2C),
            static_cast<unsigned long long>(at<std::uint64_t>(health.data()+0x320)),
            static_cast<unsigned long long>(at<std::uint64_t>(health.data()+0x328)),
            static_cast<unsigned long long>(at<std::uint64_t>(health.data()+0x330)),
            static_cast<unsigned long long>(at<std::uint64_t>(health.data()+0x338)),
            static_cast<unsigned long long>(at<std::uint64_t>(payload.data())),
            static_cast<unsigned long long>(at<std::uint64_t>(payload.data()+8)),
            static_cast<unsigned long long>(at<std::uint64_t>(payload.data()+0x10)),
            static_cast<unsigned long long>(at<std::uint64_t>(payload.data()+0x18)),
            static_cast<unsigned long long>(at<std::uint64_t>(payload.data()+0x20)),
            static_cast<unsigned long long>(at<std::uint64_t>(payload.data()+0x28)),
            static_cast<unsigned long long>(at<std::uint64_t>(payload.data()+0x30)),read.copied);
    } else {
        // One line per rejection reason per Omega run. An unscoped event is
        // explicitly diagnostic: it is not classified as a Lair enemy or death.
        const auto flag=std::uint32_t{1}<<rejection;
        if((g_candidateRejected&flag)==0) {
            g_candidateRejected|=flag;
            report("ev=omega_enemy_lair stage=candidate_unscoped run=%llu reason=%u sub=%u instance=%p event=%08X definition=%08X kind=%08X actor_field=%08X actor=%08X actor_self=%08X character_field=%08X resolved=%p source_ref=%08X resource=%08X native_registry=%08X native_type=%u native_slot=%d expected_registry=%08X expected_slot=%u copied=%zu",
                static_cast<unsigned long long>(nav.run),rejection,
                rejection==3?actorState.reason:rejection==7?sourceState.reason:0U,instance,event,
                at<std::uint32_t>(character.data()),at<std::uint32_t>(character.data()+4),
                at<std::uint32_t>(character.data()+0xC0),actorState.handle,actorState.self,
                at<std::uint32_t>(character.data()+0x24),reinterpret_cast<void*>(characterAddress),
                actorState.source.handle,sourceState.resource,sourceState.nativeRegistry,sourceState.nativeType,
                sourceState.nativeSlot,sourceState.expectedRegistry,sourceState.expectedSlot,read.copied);
        }
    }
    ReleaseSRWLockExclusive(&g_lock);
}
namespace nativeEvents=state::activity::native_population;
namespace pending=native_population_pending;
std::mutex g_pendingMutex;
pending::Queue<128> g_pendingBirths;
pending::Queue<128> g_admittedActors;
std::atomic_uint g_nativeLines{};
template<class... Args> void native_report(const char* format,Args... args) noexcept {
    if(g_nativeLines.fetch_add(1,std::memory_order_relaxed)>=128) return;
    std::array<char,512> line{};const auto count=std::snprintf(line.data(),line.size(),format,args...);
    if(count>0 && static_cast<std::size_t>(count)<line.size())
        core::log::write(core::log::Channel::client,core::log::Level::info,{line.data(),static_cast<std::size_t>(count)});
}
// Shared observer for explicitly registered activity sources. Package definitions,
// salted actor backlinks, typed health interfaces and source generations are
// qualified before copying an event into the state mailbox. No spawn or AI call.
bool registered_source(Read& read,const Actor& actorState,nativeEvents::Lease& lease) noexcept {
    std::uintptr_t address{},definitionAddress{};Ref definition{};std::uint32_t marker{};
    if(actorState.source.kind!=0x80809A3BU || actorState.source.offset!=0
        || !read.resolve(actorState.source,address) || !read.value(address,definition)
        || definition.kind!=0x8080948FU || definition.offset<4 || definition.offset>0x100000
        || !read.resolve(definition,definitionAddress) || definitionAddress>UINTPTR_MAX-0x30
        || !read.value(definitionAddress-4,marker) || !pending::definition(definition.kind,definition.offset,marker)
        || address>UINTPTR_MAX-0x244) return false;
    std::array<std::byte,8> identity{};std::uint32_t generation{},sense{};
    if(!read.copy(definitionAddress+0x30,identity) || at<std::uint8_t>(identity.data()+4)!=1
        || !read.value(address+0x1FC,generation) || !read.value(address+0x244,sense)
        || !generation || generation!=sense) return false;
    const auto slot=at<std::int16_t>(identity.data()+6);if(slot<0) return false;
    lease=nativeEvents::lookup(definition.handle,at<std::uint32_t>(identity.data()),static_cast<std::uint16_t>(slot),generation);
    return lease.activity && lease.source.valid();
}
void observe_native_admission(std::uint32_t parent,std::uint64_t epochValue) noexcept {
    if(!epochValue || parent==UINT32_MAX) return;
    Read read;std::uintptr_t address{},again{};std::array<std::byte,0x28> header{};
    std::uint32_t handle{UINT32_MAX};Actor actorState;nativeEvents::Lease lease;
    if(!read.resolve({parent,0,0},address) || address>UINTPTR_MAX-0x1470
        || !read.copy(address,header) || at<std::uint32_t>(header.data()+4)!=0x808082ECU
        || at<std::uint32_t>(header.data()+0x24)!=parent || !read.value(address+0x1470,handle)
        || !actor(read,handle,actorState) || actorState.parent!=parent
        || !read.resolve({actorState.parent,0,0},again) || again!=address) {
        native_report("ev=native_population_capture stage=created result=identity_rejected parent=%08X actor=%08X",parent,handle);
        return;
    }
    if(!registered_source(read,actorState,lease)) {
        native_report("ev=native_population_capture stage=created result=source_rejected parent=%08X actor=%08X source=%08X entity=%08X",
            parent,handle,actorState.source.handle,actorState.entity);
        // A streamed copy can finish construction without any source link. Keep
        // a bounded native call path so restoration is distinguishable from a
        // new population request; observation never changes construction.
        static std::atomic_uint sourceLessTraces{};
        if(actorState.source.handle==UINT32_MAX
            && sourceLessTraces.fetch_add(1,std::memory_order_relaxed)<12) {
            std::array<void*,24> frames{};
            const auto depth=RtlCaptureStackBackTrace(0,static_cast<ULONG>(frames.size()),frames.data(),nullptr);
            std::array<char,256> path{};std::size_t used{};
            for(USHORT i=0;i<depth;++i) {
                const auto frame=reinterpret_cast<std::uintptr_t>(frames[i]);
                if(frame<g_image || frame-g_image>=0x1C00000U)continue;
                const auto written=std::snprintf(path.data()+used,path.size()-used,"%s%llX",
                    used?",":"",static_cast<unsigned long long>(frame-g_image));
                if(written<=0 || static_cast<std::size_t>(written)>=path.size()-used)break;
                used+=static_cast<std::size_t>(written);
            }
            native_report("ev=native_population_capture stage=source_less_path parent=%08X definition=%08X actor=%08X flags=%04X native_rvas=%s",
                parent,at<std::uint32_t>(header.data()),handle,actorState.flags,path.data());
        }
        return;
    }
    std::lock_guard lock(g_pendingMutex);
    if(epochValue!=nativeEvents::epoch()) {nativeEvents::observation_lost();return;}
    const auto result=g_pendingBirths.add({{lease,{lease.source,actorState.handle,actorState.entity},
        actorState.source.handle,nativeEvents::Kind::admitted},parent});
    if(result!=pending::Intake::accepted && result!=pending::Intake::duplicate) nativeEvents::observation_lost();
    native_report("ev=native_population_capture stage=created registry=%08X slot=%u actor=%08X entity=%08X parent=%08X result=%u",
        lease.source.source.registry,lease.source.source.slot,actorState.handle,actorState.entity,parent,static_cast<unsigned>(result));
}
// Caller owns g_pendingMutex. This only completes previously witnessed births;
// it never discovers actors by scanning the world or invents a creation event.
void finish_native_admissions(std::uint32_t onlyActor=UINT32_MAX) noexcept {
    for(std::size_t i=0;i<g_pendingBirths.size();) {
        const auto birth=g_pendingBirths[i];const auto& expected=birth.event;
        if(onlyActor!=UINT32_MAX && expected.actor.actor!=onlyActor) {++i;continue;}
        const auto& source=expected.lease.source;
        const auto epochValue=nativeEvents::epoch();
        if(nativeEvents::lookup(source.source.definition,source.source.registry,source.source.slot,source.generation)!=expected.lease) {
            g_pendingBirths.erase(i);continue; // Authoritative owner/lease was released.
        }
        Read read;Actor current;nativeEvents::Lease lease;std::uintptr_t address{};
        std::array<std::byte,0x30> header{};std::uint32_t parentActor{UINT32_MAX};
        if(!actor(read,expected.actor.actor,current) || current.parent!=birth.parent
            || current.source.handle!=expected.sourceHandle || !read.resolve({birth.parent,0,0},address)
            || address>UINTPTR_MAX-0x1470 || !read.copy(address,header)
            || !read.value(address+0x1470,parentActor)
            || !omega_enemy_native_admission::identity(birth.parent,at<std::uint32_t>(header.data()+4),
                at<std::uint32_t>(header.data()+0x24),parentActor,current.handle,current.parent)
            || !registered_source(read,current,lease) || lease!=expected.lease) {
            nativeEvents::observation_lost();g_pendingBirths.erase(i);
            native_report("ev=native_population_capture stage=attachment result=identity_lost actor=%08X",expected.actor.actor);
            continue;
        }
        if(current.entity==UINT32_MAX || at<std::uint32_t>(header.data()+0x2C)==UINT32_MAX) {++i;continue;}
        if(current.entity!=at<std::uint32_t>(header.data()+0x2C)) {
            nativeEvents::observation_lost();g_pendingBirths.erase(i);
            native_report("ev=native_population_capture stage=attachment result=entity_mismatch actor=%08X",current.handle);
            continue;
        }
        const nativeEvents::Event complete{lease,{lease.source,current.handle,current.entity},
            current.source.handle,nativeEvents::Kind::admitted};
        const bool accepted=nativeEvents::submit(complete,epochValue);
        if(!accepted) nativeEvents::observation_lost();
        else {
            const auto retained=g_admittedActors.add({complete,birth.parent});
            if(retained!=pending::Intake::accepted && retained!=pending::Intake::duplicate) nativeEvents::observation_lost();
            observe_vance_contact_admission(complete,birth.parent);
        }
        native_report("ev=native_population_capture stage=attachment actor=%08X entity=%08X accepted=%u",
            current.handle,current.entity,accepted?1U:0U);
        g_pendingBirths.erase(i);
    }
}
void observe_native_candidate(void* instance,std::uint32_t event,std::uint64_t epochValue) noexcept {
    if(!epochValue) return;
    Read read;std::array<std::byte,0x3C> eventHeader{};std::array<std::byte,0x38> payload{};
    std::uint32_t eventDefinition{};
    if(!event_payload(read,event,eventHeader,payload,eventDefinition) || eventDefinition!=0x80804C54U) return;
    const auto address=reinterpret_cast<std::uintptr_t>(instance);std::uintptr_t resolved{},healthAddress{};
    std::array<std::byte,0xC4> character{};Actor actorState;nativeEvents::Lease lease;
    if(address>UINTPTR_MAX-0x2E8 || !read.copy(address,character)
        || at<std::uint32_t>(character.data()+4)!=0x80806832U
        || !actor(read,at<std::uint32_t>(character.data()+0xC0),actorState)
        || !read.resolve({at<std::uint32_t>(character.data()+0x24),0,0},resolved) || resolved!=address
        || !registered_source(read,actorState,lease)) return;
    Ref healthRef{};std::array<std::byte,0x340> health{};
    if(!omega_enemy_native_health::owner(actorState.entity,at<std::uint32_t>(character.data()+0x2C))
        || !read.value(address+0x2E8,healthRef) || healthRef.kind!=0x80804BEEU || healthRef.offset!=0
        || !read.resolve(healthRef,healthAddress) || !read.copy(healthAddress,health)
        || !omega_enemy_native_health::identity(healthRef.handle,healthRef.kind,healthRef.offset,
            at<std::uint32_t>(health.data()+4),at<std::uint32_t>(health.data()+0x24),
            actorState.entity,at<std::uint32_t>(health.data()+0x2C))
        || !omega_enemy_native_health::death(true,eventDefinition,true,at<std::uint8_t>(health.data()+0x338),
            lease.source.generation,lease.source.generation)) return;
    // A death can beat the next frame poll. Deliver that actor's captured birth
    // first under the same lock, then its independently qualified native death.
    std::lock_guard lock(g_pendingMutex);
    finish_native_admissions(actorState.handle);
    if(!nativeEvents::submit({lease,{lease.source,actorState.handle,actorState.entity},
        actorState.source.handle,nativeEvents::Kind::died},epochValue)) nativeEvents::observation_lost();
}
__declspec(noinline) std::uint64_t __fastcall admission_hook(void* instance,const void* context) noexcept {
    const hooking::CallGate::Scope scope{g_gate};
    const auto nav=selected_context();
    const auto nativeEpoch=scope.accepts_side_effects()?nativeEvents::epoch():0;
    std::uint32_t parent=UINT32_MAX;
    if(scope.accepts_side_effects() && ((nav.enabled && nav.run!=0) || nativeEpoch)) {
        Read read;std::array<std::byte,0x28> header{};
        if(read.copy(reinterpret_cast<std::uintptr_t>(instance),header)
            && at<std::uint32_t>(header.data()+4)==0x808082ECU) {
            parent=at<std::uint32_t>(header.data()+0x24);
        }
    }
    return omega_enemy_native_admission::forward(hooking::await_original(g_admission),
        [&scope]() noexcept {return scope.accepts_side_effects();},
        [callRun=nav.run,nativeEpoch](std::uint32_t identity) noexcept {
            if(identity!=UINT32_MAX) {observe_admission(identity,callRun);observe_native_admission(identity,nativeEpoch);}
            else if(nativeEpoch) native_report("ev=native_population_capture stage=created result=pre_identity_rejected");
        },instance,context,parent);
}
__declspec(noinline) std::uint64_t __fastcall candidate_hook(void* instance,std::uint32_t event) noexcept {
    const hooking::CallGate::Scope scope{g_gate};
    const auto original=hooking::await_original(g_candidate);
    if(scope.accepts_side_effects()) {omega_boss_health::observe_native_death(instance,event,scope);}
    if(scope.accepts_side_effects()) {observe_candidate(instance,event,scope);
        observe_native_candidate(instance,event,nativeEvents::epoch());}
    return original(instance,event);
}
bool idle() noexcept {return g_gate.idle();}
bool retirement_slot(Read& read,std::uint32_t handle,native_population_retirement::Slot& slot) noexcept {
    std::array<std::byte,0x28> descriptor{};
    if(!read.copy(g_image+0x1F9D7F0,descriptor)) return false;
    slot.base=at<std::uintptr_t>(descriptor.data()+8);
    slot.stride=at<std::uint32_t>(descriptor.data()+0x20);
    slot.generationOffset=at<std::uint32_t>(descriptor.data()+0x1C);
    slot.mask=at<std::uint32_t>(descriptor.data()+0x24);
    std::uintptr_t cell{};
    return native_population_retirement::valid(slot)
        && add(slot.base,static_cast<std::int64_t>(handle&0x1FFFU)*slot.stride+slot.generationOffset,cell)
        && read.value(cell,slot.generation);
}
__declspec(noinline) void __fastcall retirement_hook(std::uint32_t handle,std::uint8_t mode) noexcept {
    const hooking::CallGate::Scope scope{g_gate};
    const auto original=hooking::await_original(g_retirement);
    const auto hijackedRun=scope.accepts_side_effects()?hijacked::native_run():0;
    if(hijackedRun && TryAcquireSRWLockExclusive(&g_lock)) {
        trace_hijacked_retirement(hijackedRun,handle);
        ReleaseSRWLockExclusive(&g_lock);
    }
    const auto epochValue=scope.accepts_side_effects()?nativeEvents::epoch():0;
    pending::Birth captured;native_population_retirement::Slot before;
    bool qualified{};
    if(epochValue) {
        std::lock_guard lock(g_pendingMutex);
        finish_native_admissions(handle);
        for(std::size_t i=0;i<g_admittedActors.size();++i) {
            const auto& birth=g_admittedActors[i];if(birth.event.actor.actor!=handle) continue;
            const auto& source=birth.event.lease.source;Read read;Actor current;
            qualified=nativeEvents::lookup(source.source.definition,source.source.registry,source.source.slot,source.generation)==birth.event.lease
                && actor(read,handle,current)
                && native_population_retirement::identity(birth.event.actor.actor,birth.event.actor.entity,birth.parent,
                    birth.event.sourceHandle,current.handle,current.entity,current.parent,current.source.handle)
                && retirement_slot(read,handle,before);
            if(qualified) captured=birth;
            else {
                nativeEvents::observation_lost();
                native_report("ev=native_population_capture stage=retirement_begin result=identity_rejected actor=%08X entity=%08X parent=%08X source=%08X",
                    handle,current.entity,current.parent,current.source.handle);
            }
            break;
        }
    }
    // Never hold the pending/mailbox lock across native teardown. Native
    // callbacks may run inside it, and their original order must be retained.
    native_population_retirement::forward(original,[&]() noexcept {
        if(!qualified || !scope.accepts_side_effects()) return;
        Read read;native_population_retirement::Slot after;
        const bool released=retirement_slot(read,handle,after) && native_population_retirement::released(before,after);
        auto event=captured.event;event.kind=nativeEvents::Kind::retired;
        std::lock_guard lock(g_pendingMutex);
        const bool accepted=released && nativeEvents::submit(event,epochValue);
        if(!accepted) nativeEvents::observation_lost();
        if(released) observe_vance_contact_retirement(event);
        for(std::size_t i=0;i<g_admittedActors.size();++i) {
            if(g_admittedActors[i].event.actor==event.actor) {g_admittedActors.erase(i);break;}
        }
        native_report("ev=native_population_capture stage=retired actor=%08X entity=%08X generation_before=%u generation_after=%u released=%u accepted=%u",
            handle,captured.event.actor.entity,before.generation,after.generation,released?1U:0U,accepted?1U:0U);
    },handle,mode);
}
#include "strike_bond_intro_release.inl"
#include "strike_bond_target_binding.inl"
#include "strike_bond_carriage.inl"
#include "strike_bond_boss_cycle.inl"
#include "strike_bond_boss_shield.inl"
#include "strike_bond_boss_retirement.inl"
#include "strike_bond_fire_trace.inl"

void* target(std::uintptr_t rva,const std::array<std::uint8_t,16>& expected) noexcept {
    Read read;std::array<std::byte,16> actual{};
    if(!read.copy(g_image+rva,actual) || std::memcmp(actual.data(),expected.data(),expected.size())!=0) {return nullptr;}
    return reinterpret_cast<void*>(g_image+rva);
}
} // namespace

__declspec(noinline) void retire_strike_bond_boss(std::uintptr_t source,bool allocatorReady) noexcept {
    const hooking::CallGate::Scope scope{g_gate};
    if(scope.accepts_side_effects()) garden_retirement::dispatch(source,allocatorReady);
}

__declspec(noinline) void poll_native_population_admissions() noexcept {
    const hooking::CallGate::Scope scope{g_gate};
    if(!scope.accepts_side_effects()) return;
    poll_mission_population_readiness(g_image,GetTickCount64());
    const auto hijackedRun=hijacked::native_run();
    if(hijackedRun && TryAcquireSRWLockExclusive(&g_lock)) {
        trace_hijacked_attachments(hijackedRun);
        ReleaseSRWLockExclusive(&g_lock);
    }
    std::lock_guard lock(g_pendingMutex);
    finish_native_admissions();
    for(std::size_t i=0;i<g_admittedActors.size();) {
        const auto& birth=g_admittedActors[i];const auto& source=birth.event.lease.source;
        if(nativeEvents::lookup(source.source.definition,source.source.registry,source.source.slot,source.generation)!=birth.event.lease)
            g_admittedActors.erase(i);
        else ++i;
    }
}

bool install_omega_enemy_lair_receipts() noexcept {
    if(g_handles[0].attached) {return g_gate.accepting();}
    g_image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    if(g_image==0) {return false;}
    const std::array<hooking::detour::Spec,9> specs{{
        {target(0xA0D510,{0x48,0x89,0x5C,0x24,0x20,0x56,0x48,0x83,0xEC,0x30,0x48,0x8B,0xD9,0x48,0x8B,0xF2}),reinterpret_cast<void*>(&admission_hook)},
        {target(0xC72390,{0x48,0x89,0x5C,0x24,0x10,0x55,0x56,0x57,0x48,0x83,0xEC,0x20,0x48,0x8B,0xE9,0x8B}),reinterpret_cast<void*>(&candidate_hook)},
        {target(0xA85540,{0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xEC,0x70,0x8B}),reinterpret_cast<void*>(&retirement_hook)},
        {target(0xBC8F20,{0x40,0x53,0x48,0x83,0xEC,0x20,0x48,0x8B,0xD9,0x48,0x8D,0x4C,0x24,0x30,0xE8,0x5D}),reinterpret_cast<void*>(&garden_fire::one_tick_hook)},
        {target(0xBC8F80,{0x0F,0x2F,0x89,0xC0,0x0A,0x00,0x00,0x0F,0xB6,0x91,0xC4,0x0A,0x00,0x00,0x76,0x1B}),reinterpret_cast<void*>(&garden_fire::duration_hook)},
        {target(0xBCD330,{0x40,0x56,0x57,0x48,0x83,0xEC,0x48,0x48,0x89,0x5C,0x24,0x68,0x32,0xC0,0x8B,0x59}),reinterpret_cast<void*>(&garden_fire::update_hook)},
        {target(0xC31200,{0x48,0x89,0x4C,0x24,0x08,0x53,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41}),reinterpret_cast<void*>(&garden_fire::eligibility_hook)},
        {target(0xC613E0,{0x48,0x89,0x5C,0x24,0x18,0x48,0x89,0x74,0x24,0x20,0x57,0x48,0x83,0xEC,0x20,0x44}),reinterpret_cast<void*>(&garden_target::dispatch_hook)},
        {target(0xC5FEF0,{0x40,0x57,0x48,0x83,0xEC,0x20,0x44,0x0F,0xBE,0x49,0x7A,0x49,0x8B,0xF8,0x41,0x83}),reinterpret_cast<void*>(&garden_target::decode_hook)}
    }};
    for(const auto& spec:specs) if(!spec.target) return false;
    if(!hooking::detour::install(specs,g_handles)) return false;
    hooking::publish_original(g_admission,reinterpret_cast<Admission>(g_handles[0].original));
    hooking::publish_original(g_candidate,reinterpret_cast<CandidateEvent>(g_handles[1].original));
    hooking::publish_original(g_retirement,reinterpret_cast<Retirement>(g_handles[2].original));
    hooking::publish_original(garden_fire::oneTick,reinterpret_cast<garden_fire::OneTick>(g_handles[3].original));
    hooking::publish_original(garden_fire::duration,reinterpret_cast<garden_fire::Duration>(g_handles[4].original));
    hooking::publish_original(garden_fire::update,reinterpret_cast<garden_fire::Update>(g_handles[5].original));
    hooking::publish_original(garden_fire::eligibility,reinterpret_cast<garden_fire::Eligibility>(g_handles[6].original));
    hooking::publish_original(garden_target::dispatch,reinterpret_cast<garden_target::Dispatch>(g_handles[7].original));
    hooking::publish_original(garden_target::decode,reinterpret_cast<garden_target::Decode>(g_handles[8].original));
    g_gate.accept();
    core::log::write(core::log::Channel::client,core::log::Level::info,
        "ev=omega_enemy_lair stage=install result=ok admission=A0D510 health_death=C72390 event=80804C54 retirement=A85540 garden_fire_trace=BC8F20,BC8F80,BCD330,C31200 garden_intro_release=AFB11A12:31A03F93 garden_target_binding=C613E0,C5FEF0");
    return true;
}
void quiesce_omega_enemy_lair_receipts() noexcept {g_gate.quiesce();}
bool uninstall_omega_enemy_lair_receipts() noexcept {
    quiesce_omega_enemy_lair_receipts();if(!g_handles[0].attached) {return true;}
    const std::array<hooking::detour::ProtectedCodeEntry,17> protectedCode{{
        {reinterpret_cast<void*>(&admission_hook)},{reinterpret_cast<void*>(&candidate_hook)},
        {reinterpret_cast<void*>(&observe_admission)},{reinterpret_cast<void*>(&observe_candidate)},
        {reinterpret_cast<void*>(&omega_boss_health::observe_native_death)},
        {reinterpret_cast<void*>(&poll_native_population_admissions)},
        {reinterpret_cast<void*>(&retirement_hook)},
        {reinterpret_cast<void*>(&retire_strike_bond_boss)},
        {reinterpret_cast<void*>(&garden_retirement::dispatch)},
        {reinterpret_cast<void*>(&garden_fire::one_tick_hook)},
        {reinterpret_cast<void*>(&garden_fire::duration_hook)},
        {reinterpret_cast<void*>(&garden_fire::update_hook)},
        {reinterpret_cast<void*>(&garden_fire::eligibility_hook)},
        {reinterpret_cast<void*>(&garden_target::dispatch_hook)},
        {reinterpret_cast<void*>(&garden_target::decode_hook)},
        {reinterpret_cast<void*>(&hooking::call_gate_detail::enter)},
        {reinterpret_cast<void*>(&hooking::call_gate_detail::leave)}
    }};
    if(hooking::detour::uninstall(g_handles,protectedCode,idle)!=hooking::detour::UninstallResult::removed) {return false;}
    g_admission.store(nullptr,std::memory_order_release);g_candidate.store(nullptr,std::memory_order_release);
    g_retirement.store(nullptr,std::memory_order_release);
    garden_fire::oneTick.store(nullptr,std::memory_order_release);garden_fire::duration.store(nullptr,std::memory_order_release);
    garden_fire::update.store(nullptr,std::memory_order_release);garden_fire::eligibility.store(nullptr,std::memory_order_release);
    garden_target::dispatch.store(nullptr,std::memory_order_release);garden_target::decode.store(nullptr,std::memory_order_release);
    garden_fire::reset();garden_intro::reset();garden_target::reset();garden_target::reset_replay();garden_carriage::reset();garden_cycle::reset();garden_shield::reset();garden_retirement::reset();
    g_image=0;g_run=UINT64_MAX;g_lines=0;g_seenCount=0;g_seen={};
    hijacked_trace_run(0);
    g_pendingBirths={};g_admittedActors={};g_nativeLines.store(0,std::memory_order_relaxed);
    g_candidateRejected=0;g_parentRejected=0;g_rejects={};g_rejectCount=0;g_rejectOverflow=false;return true;
}
} // namespace sunrise::client::hooks::bootflow
