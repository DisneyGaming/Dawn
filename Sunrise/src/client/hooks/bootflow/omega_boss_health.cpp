#include <Windows.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <span>

#include "omega_boss_health.h"
#include "omega_boss_health_identity.h"
#include "../../../core/logging/log.h"
#include "../../../state/activity/omega_first_lair_runtime.h"
#include "../../../state/activity/omega_presentation.h"

namespace sunrise::client::hooks::bootflow::omega_boss_health {
namespace {
using Fraction = float(__fastcall*)(void*,std::int32_t) noexcept;
namespace lair=state::activity::omega_first_lair;
struct DeathLease final {
    std::uintptr_t image{};
    Resolve resolve{};
    lair::CrownToken token{};
};
SRWLOCK g_lock=SRWLOCK_INIT;
DeathLease g_death{};
// Bounded rejection diagnostics: one line per (reason, key) per run, at most
// kRejectLines lines per run. Observe-only; never changes control flow.
constexpr unsigned kRejectLines=64;
SRWLOCK g_rejectLock=SRWLOCK_INIT;
std::uint64_t g_rejectRun{};
unsigned g_rejectLines{},g_rejectKeyCount{};
std::array<std::uint64_t,kRejectLines> g_rejectKeys{};
void report_reject(const char* stage,std::uint64_t run,Reject reason,std::uint32_t handle,
    std::uint32_t detail=0,std::uint32_t extra=0) noexcept {
    const auto key=(static_cast<std::uint64_t>(static_cast<std::uint8_t>(reason))<<56)
        |(static_cast<std::uint64_t>(handle)<<24)|(static_cast<std::uint64_t>(stage[0])<<16)
        |static_cast<std::uint64_t>(detail&0xFFFFU);
    AcquireSRWLockExclusive(&g_rejectLock);
    if(g_rejectRun!=run) { g_rejectRun=run;g_rejectLines=0;g_rejectKeyCount=0;g_rejectKeys={}; }
    bool emit=g_rejectLines<kRejectLines;
    for(unsigned i=0;emit && i<g_rejectKeyCount;++i) { if(g_rejectKeys[i]==key) { emit=false; } }
    if(emit) { ++g_rejectLines;if(g_rejectKeyCount<g_rejectKeys.size()) { g_rejectKeys[g_rejectKeyCount++]=key; } }
    ReleaseSRWLockExclusive(&g_rejectLock);
    if(!emit) { return; }
    std::array<char,256> line{};
    const int size=std::snprintf(line.data(),line.size(),
        "ev=omega_boss_health stage=reject site=%s run=%llu reason=%s handle=%08X detail=%08X extra=%08X",
        stage,static_cast<unsigned long long>(run),reject_name(reason),handle,detail,extra);
    if(size>0 && static_cast<std::size_t>(size)<line.size()) {
        core::log::write(core::log::Channel::client,core::log::Level::info,
            {line.data(),static_cast<std::size_t>(size)});
    }
}
bool copy(const void* source,std::span<std::byte> target) noexcept {
    SIZE_T read{};
    return source!=nullptr && ReadProcessMemory(GetCurrentProcess(),source,
        target.data(),target.size(),&read) && read==target.size();
}
template<class T> bool value(std::uintptr_t address,T& out) noexcept {
    return address>=0x10000 && address<=UINTPTR_MAX-sizeof(T)
        && copy(reinterpret_cast<const void*>(address),std::as_writable_bytes(std::span{&out,std::size_t{1}}));
}
bool final_death_action(const lair::Status& status,const lair::CrownToken& token) noexcept {
    return token.valid() && token.cycle==3 && token.boss.actionEpoch!=0
        && status.enabled && !status.failed && status.token==token
        && status.crownStage==lair::CrownStage::death
        && (status.phase==lair::Phase::mechanicRequested || status.phase==lair::Phase::mechanicPlaying);
}
/** Actor row at image+1F9D7F8 (stride image+1F9D800): +48 self handle, +4C
 * world entity, +60 member reference {handle,8080834E,0}. This layout and the
 * member reference kind were captured live from the enemy roster (re-2026-09-05
 * omega_enemy_lair_live_38748_184127/capture.json); the same table serves the
 * boss actor. The member itself is the 80F4756D roster component. */
Reject member_current(const DeathLease& lease,std::uint32_t* observed=nullptr) noexcept {
    const auto& boss=lease.token.boss;
    std::uintptr_t base{};std::int32_t stride{};
    if(!value(lease.image+0x1F9D7F8,base) || !value(lease.image+0x1F9D800,stride)
        || stride<0x70 || stride>0x100000) { return Reject::actorRow; }
    const auto offset=std::uintptr_t{boss.actor&0x1FFFU}*static_cast<std::uintptr_t>(stride);
    if(base<0x10000 || base>UINTPTR_MAX-offset) { return Reject::actorRow; }
    std::array<std::byte,0x70> actor{};
    if(!copy(reinterpret_cast<const void*>(base+offset),actor)
        || detail::read<std::uint32_t>(actor,0x48)!=boss.actor
        || detail::read<std::uint32_t>(actor,0x4C)!=boss.entity) {
        if(observed!=nullptr) { *observed=detail::read<std::uint32_t>(actor,0x48); }
        return Reject::actorRow;
    }
    const auto ref=detail::read<Reference>(actor,0x60);
    if(ref.handle==UINT32_MAX || ref.kind!=0x8080834EU || ref.offset!=0) {
        if(observed!=nullptr) { *observed=ref.kind; }
        return Reject::memberReference;
    }
    std::array<std::byte,kMemberBytes> member{};
    const auto* instance=lease.resolve(ref.handle);
    if(!copy(instance,member) || !member_identity(member,boss,ref)) {
        if(observed!=nullptr) { *observed=detail::read<std::uint32_t>(member,0x21C); }
        return Reject::memberIdentity;
    }
    // Recheck the actor's full typed backlink after resolving/copying the
    // inline member; a retired/rebound row cannot authorize the old snapshot.
    std::array<std::byte,0x70> after{};
    if(!copy(reinterpret_cast<const void*>(base+offset),after)
        || detail::read<std::uint32_t>(after,0x48)!=boss.actor
        || detail::read<std::uint32_t>(after,0x4C)!=boss.entity
        || std::memcmp(after.data()+0x60,actor.data()+0x60,sizeof(Reference))!=0
        || lease.resolve(ref.handle)!=instance) { return Reject::memberIdentity; }
    return Reject::none;
}
bool is_native_death_event(const DeathLease& lease,std::uint32_t event,std::uint32_t* observed=nullptr) noexcept {
    if(event==UINT32_MAX) { return false; }
    // The original 9ECC70 resolves this exact event arena header before C72390.
    std::uintptr_t allocator{},buffer{};
    if(!value(lease.image+0x274F778,allocator) || allocator>UINTPTR_MAX-0x10
        || !value(allocator+0x10,buffer)) { return false; }
    const auto offset=std::uintptr_t{event&0xFFFFFU};
    if(buffer<0x10000 || buffer>UINTPTR_MAX-offset-0x3C) { return false; }
    std::array<std::byte,0x3C> header{};
    if(!copy(reinterpret_cast<const void*>(buffer+offset),header)) { return false; }
    const auto definition=detail::read<std::uint32_t>(header,0x34);
    if(observed!=nullptr) { *observed=definition; }
    return definition==0x80804C54U;
}
void set(Reject* reason,Reject value) noexcept { if(reason!=nullptr) { *reason=value; } }
} // namespace

const char* reject_name(Reject reason) noexcept {
    switch(reason) {
    case Reject::none:return "none";
    case Reject::arguments:return "arguments";
    case Reject::getterPrefix:return "getter_prefix";
    case Reject::characterRead:return "character_read";
    case Reject::characterIdentity:return "character_identity";
    case Reject::healthRead:return "health_read";
    case Reject::healthIdentity:return "health_identity";
    case Reject::fractionInvalid:return "fraction_invalid";
    case Reject::healthRecheck:return "health_recheck";
    case Reject::characterRecheck:return "character_recheck";
    case Reject::stateNotFinalDeath:return "state_not_final_death";
    case Reject::actorRow:return "actor_row";
    case Reject::memberReference:return "member_reference";
    case Reject::memberIdentity:return "member_identity";
    case Reject::leaseConflict:return "lease_conflict";
    case Reject::noLease:return "no_lease";
    case Reject::eventHeader:return "event_header";
    case Reject::deathBit:return "death_bit";
    case Reject::gateClosed:return "gate_closed";
    }
    return "unknown";
}

bool sample(std::uintptr_t image,Resolve resolve,
    const state::activity::omega_first_lair::Boss& boss,
    const void* character,Sample& result,Reject* reason) noexcept {
    result={};set(reason,Reject::none);
    if(image==0 || resolve==nullptr || character==nullptr || !boss.valid()) { set(reason,Reject::arguments);return false; }
    // CD6C20(component,region): rcx is the runtime health component. Its own
    // 16-byte prefix {815B5A40,80804B8A,F98} is read as a definition reference
    // (handle at +0, offset at +8) to reach the authored region table at
    // definition+1B8 (file 16C0, stride 138); the region's +D0 index builds the
    // health context (B7BE80) and B8B0C0 decrypts the stored regional fraction,
    // returning it as a float in XMM0 (its epilogue is movss xmm0,[rsp+30]).
    // The failure path returns xorps xmm0 (0.0), so an out-of-range region or
    // bad prefix cannot be told from empty health without the identity guards.
    std::array<std::byte,16> prefix{};
    const auto* entry=reinterpret_cast<const void*>(image+0xCD6C20);
    if(!copy(entry,prefix) || !fraction_getter_prefix(prefix)) {
        set(reason,Reject::getterPrefix);return false;
    }
    std::array<std::byte,kCharacterBytes> owner{};
    Reference reference{};
    if(!copy(character,owner)) { set(reason,Reject::characterRead);return false; }
    if(!character_identity(owner,boss,reference)) { set(reason,Reject::characterIdentity);return false; }
    auto* health=resolve(reference.handle);
    std::array<std::byte,kHealthBytes> bytes{};
    if(!copy(health,bytes)) { set(reason,Reject::healthRead);return false; }
    if(!health_identity(bytes,boss,reference)) { set(reason,Reject::healthIdentity);return false; }
    const auto fraction=reinterpret_cast<Fraction>(image+0xCD6C20);
    const auto body=fraction(health,0);
    const auto eye=fraction(health,1);
    // Recheck after the native getters. A recycled or rebound component must
    // not transfer the previous allocation's health into a new action token.
    if(!fraction_valid(body) || !fraction_valid(eye)) { set(reason,Reject::fractionInvalid);return false; }
    if(!copy(health,bytes) || !health_identity(bytes,boss,reference)) { set(reason,Reject::healthRecheck);return false; }
    Reference after{};
    if(!copy(character,owner) || !character_identity(owner,boss,after) || after.handle!=reference.handle) {
        set(reason,Reject::characterRecheck);return false;
    }
    result={reference.handle,body,eye,(detail::read<std::uint8_t>(bytes,0x338)&1U)!=0};
    return true;
}

bool arm_native_death(std::uintptr_t image,Resolve resolve,const lair::CrownToken& token,Reject* reason) noexcept {
    set(reason,Reject::none);
    if(image==0 || resolve==nullptr) { set(reason,Reject::arguments);return false; }
    if(!final_death_action(lair::status(token.boss.run),token)) {
        set(reason,Reject::stateNotFinalDeath);
        report_reject("arm",token.boss.run,Reject::stateNotFinalDeath,token.boss.actor,token.boss.actionEpoch,token.cycle);
        return false;
    }
    const DeathLease lease{image,resolve,token};
    std::uint32_t observed{};
    if(const auto member=member_current(lease,&observed);member!=Reject::none) {
        set(reason,member);report_reject("arm",token.boss.run,member,token.boss.actor,observed,token.boss.entity);
        return false;
    }
    AcquireSRWLockExclusive(&g_lock);
    // A fresh state-owned run may replace a previous run whose native actor
    // was retired before producing death. Keep same-run action epochs strict.
    const bool accepted=!g_death.token.valid() || g_death.token==token
        || (g_death.token.boss.run!=token.boss.run
            && !lair::status(g_death.token.boss.run).enabled);
    const auto previous=g_death.token;
    if(accepted) { g_death=lease; }
    ReleaseSRWLockExclusive(&g_lock);
    if(!accepted) {
        set(reason,Reject::leaseConflict);
        report_reject("arm",token.boss.run,Reject::leaseConflict,token.boss.actor,
            previous.boss.actionEpoch,static_cast<std::uint32_t>(previous.boss.run));
    }
    return accepted;
}

__declspec(noinline) void observe_native_death(void* character,std::uint32_t event,
    const hooking::CallGate::Scope& scope) noexcept {
    if(!scope.accepts_side_effects()) { return; }
    AcquireSRWLockShared(&g_lock);const auto lease=g_death;ReleaseSRWLockShared(&g_lock);
    // Every native health death (including adds) passes here; without an armed
    // final-death lease there is nothing to explain, so stay silent.
    if(!lease.token.valid()) { return; }
    const auto& boss=lease.token.boss;
    const auto run=boss.run;
    if(!final_death_action(lair::status(run),lease.token)) {
        report_reject("death",run,Reject::stateNotFinalDeath,boss.actor,boss.actionEpoch,event);return;
    }
    // A different character dying during the final phase (an add) is not a
    // rejection of the boss receipt; log it once so the run shows the traffic.
    if(lease.resolve(boss.character)!=character) {
        report_reject("death",run,Reject::characterIdentity,boss.character,0,event);return;
    }
    std::uint32_t observed{};
    if(!is_native_death_event(lease,event,&observed)) {
        report_reject("death",run,Reject::eventHeader,boss.actor,event,observed);return;
    }
    if(const auto member=member_current(lease,&observed);member!=Reject::none) {
        report_reject("death",run,member,boss.actor,observed,event);return;
    }
    std::array<std::byte,kCharacterBytes> owner{};Reference reference{};
    if(!copy(character,owner)) { report_reject("death",run,Reject::characterRead,boss.character,event,0);return; }
    if(!character_identity(owner,boss,reference)) {
        report_reject("death",run,Reject::characterIdentity,boss.character,event,detail::read<std::uint32_t>(owner,0x2E8));return;
    }
    const auto* health=lease.resolve(reference.handle);
    std::array<std::byte,kHealthBytes> bytes{};
    if(!copy(health,bytes)) { report_reject("death",run,Reject::healthRead,reference.handle,event,0);return; }
    if(!health_identity(bytes,boss,reference)) {
        report_reject("death",run,Reject::healthIdentity,reference.handle,event,detail::read<std::uint32_t>(bytes,0));return;
    }
    const auto flags=detail::read<std::uint8_t>(bytes,0x338);
    if((flags&1U)==0) { report_reject("death",run,Reject::deathBit,reference.handle,event,flags);return; }
    if(const auto member=member_current(lease,&observed);member!=Reject::none) {
        report_reject("death",run,member,boss.actor,observed,event);return;
    }
    if(!scope.accepts_side_effects()) { report_reject("death",run,Reject::gateClosed,boss.actor,event,0);return; }
    // Delivery precedes native C72390 retirement. State validates the retained
    // cycle/action epoch again and deduplicates; no current token is substituted.
    const bool accepted=lair::observe_health(lease.token,lair::HealthMilestone::bossDead);
    if(accepted) {
        state::activity::omega_presentation::note_encounter(lease.token.boss.run,
            state::activity::omega_presentation::Encounter::defeated,3);
        AcquireSRWLockExclusive(&g_lock);
        if(g_death.token==lease.token) { g_death={}; }
        ReleaseSRWLockExclusive(&g_lock);
    }
    std::array<char,256> line{};
    const int size=std::snprintf(line.data(),line.size(),
        "ev=omega_boss_health stage=%s run=%llu actor=%08X character=%08X health=%08X event=%08X cycle=%u epoch=%u flags=%02X",
        accepted?"native_death_accepted":"native_death_duplicate",static_cast<unsigned long long>(run),
        boss.actor,boss.character,reference.handle,event,lease.token.cycle,boss.actionEpoch,flags);
    if(size>0 && static_cast<std::size_t>(size)<line.size()) {
        core::log::write(core::log::Channel::client,core::log::Level::info,
            {line.data(),static_cast<std::size_t>(size)});
    }
}

void reset() noexcept {
    AcquireSRWLockExclusive(&g_lock);g_death={};ReleaseSRWLockExclusive(&g_lock);
    AcquireSRWLockExclusive(&g_rejectLock);
    g_rejectRun=0;g_rejectLines=0;g_rejectKeyCount=0;g_rejectKeys={};
    ReleaseSRWLockExclusive(&g_rejectLock);
}

} // namespace sunrise::client::hooks::bootflow::omega_boss_health
