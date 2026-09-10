#include <Windows.h>
#include <atomic>
#include <array>
#include <cstdio>
#include <cstring>
#include <span>
#include "vance_contact_observer.h"
#include "vance_contact_contract.h"
#include "omega_enemy_native_reference.h"
#include "../../hooking/call_gate.h"
#include "../../hooking/detour.h"
#include "../../../core/logging/log.h"

namespace sunrise::client::hooks::bootflow {
namespace {
namespace contract=vance_contact;
namespace events=state::activity::native_population;
using Job=void(__fastcall*)(const void*);
using Consume=void(__fastcall*)(void*,void*);
hooking::CallGate g_gate;
std::atomic<Job> g_job{};
std::atomic<Consume> g_consume{};
std::array<hooking::detour::Handle,2> g_handles{};
std::uintptr_t g_image{};
SRWLOCK g_lock=SRWLOCK_INIT;
contract::Identity g_identity{};
contract::Budget g_budget{};
std::uint32_t g_locomotionHandle{UINT32_MAX};

template<class T> T at(const std::byte* data) noexcept {
    T result{};std::memcpy(&result,data,sizeof result);return result;
}
struct Ref final {std::uint32_t handle{UINT32_MAX},kind{};std::int64_t offset{};};
static_assert(sizeof(Ref)==16);
struct Read final {
    unsigned copied{};
    bool copy(std::uintptr_t address,std::span<std::byte> out) noexcept {
        if(address<0x10000 || address>UINTPTR_MAX-out.size() || out.size()>8192-copied) return false;
        copied+=static_cast<unsigned>(out.size());SIZE_T got{};
        return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),out.data(),out.size(),&got)
            && got==out.size();
    }
    template<class T> bool value(std::uintptr_t address,T& out) noexcept {
        return copy(address,std::as_writable_bytes(std::span{&out,std::size_t{1}}));
    }
    bool resolve(Ref ref,std::uintptr_t& out) noexcept {
        if(ref.handle==UINT32_MAX || ref.offset<0 || ref.offset>0x100000) return false;
        std::uintptr_t directory{},registry{};
        if(!value(g_image+0x2439C70,directory) || !value(directory,registry)) return false;
        const auto shifted=static_cast<std::uint32_t>(static_cast<std::int32_t>(ref.handle)>>13);
        const auto index=((static_cast<std::uint64_t>(shifted)|0xFFC0000ULL)>>18)&(shifted&0xFFFFU);
        std::array<std::byte,0x38> table{};
        if(registry>UINTPTR_MAX-index*0x40 || !copy(registry+index*0x40,table)) return false;
        const auto stride=at<std::int32_t>(table.data()+0x30);
        const auto pool=at<std::uintptr_t>(table.data()+8);
        if(stride<=0 || stride>0x100000) return false;
        const auto delta=static_cast<std::uintptr_t>(ref.handle&0x1FFF)*static_cast<unsigned>(stride);
        if(pool>UINTPTR_MAX-delta-8) return false;
        const auto element=pool+delta;std::uint64_t relocation{};
        if(!value(element+8,relocation)) return false;
        const auto base=omega_enemy_native_reference::corrected_base(element,relocation,
            at<std::int32_t>(table.data()+0x34));
        if(base>UINTPTR_MAX-static_cast<std::uint64_t>(ref.offset)) return false;
        out=static_cast<std::uintptr_t>(base+ref.offset);return out>=0x10000;
    }
};
contract::Identity identity() noexcept {
    AcquireSRWLockShared(&g_lock);const auto value=g_identity;ReleaseSRWLockShared(&g_lock);return value;
}
bool begin_sample(unsigned boundary) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    const bool admitted=g_identity.valid() && g_budget.sample(boundary,GetTickCount64());
    ReleaseSRWLockExclusive(&g_lock);return admitted;
}
void invalidate(const contract::Identity& expected) noexcept {
    AcquireSRWLockExclusive(&g_lock);
    if(g_identity==expected) {g_identity={};g_budget={};g_locomotionHandle=UINT32_MAX;}
    ReleaseSRWLockExclusive(&g_lock);
}
// Fresh salted actor/source/parent/entity joins. No actor-pool scanning and no
// reliance on a previously saved process address. Released leases invalidate it.
bool owner(Read& read,const contract::Identity& id,std::uint32_t entity) noexcept {
    if(!id.valid() || id.event.actor.entity!=entity) return false;
    const auto& source=id.event.lease.source;
    if(events::lookup(source.source.definition,source.source.registry,source.source.slot,source.generation)
        !=id.event.lease) {invalidate(id);return false;}
    std::uintptr_t pool{};std::int32_t stride{};
    if(!read.value(g_image+0x1F9D7F8,pool) || !read.value(g_image+0x1F9D800,stride)
        || stride<0x80 || stride>0x100000) return false;
    const auto delta=static_cast<std::uintptr_t>(id.event.actor.actor&0x1FFF)*static_cast<unsigned>(stride);
    if(pool>UINTPTR_MAX-delta-0x80) return false;
    std::array<std::byte,0x80> actor{};
    if(!read.copy(pool+delta,actor) || at<std::uint32_t>(actor.data()+0x48)!=id.event.actor.actor
        || at<std::uint32_t>(actor.data()+0x4C)!=entity || at<std::uint32_t>(actor.data()+0x50)!=id.parent
        || at<std::uint32_t>(actor.data()+0x2C)!=0x80EC120B) return false;
    const auto ref=at<Ref>(actor.data()+0x38);
    if(ref.handle!=id.event.sourceHandle || ref.kind!=0x80809A3B || ref.offset!=0) return false;
    std::uintptr_t instance{},definition{},parent{};Ref header{};
    if(!read.resolve(ref,instance) || !read.value(instance,header)
        || header.handle!=0x80F5B9CC || header.kind!=0x8080948F || header.offset!=0x878
        || !read.resolve(header,definition) || !read.resolve({id.parent,0,0},parent)) return false;
    std::array<std::byte,8> authored{};std::array<std::byte,0x30> parentHeader{};
    std::uint32_t generation{},sense{},parentActor{};
    if(!read.copy(definition+0x30,authored) || at<std::uint32_t>(authored.data())!=0x564C6ECE
        || at<std::uint8_t>(authored.data()+4)!=1 || at<std::int16_t>(authored.data()+6)!=0
        || !read.value(instance+0x1FC,generation) || !read.value(instance+0x244,sense)
        || generation!=source.generation || sense!=generation
        || !read.copy(parent,parentHeader) || at<std::uint32_t>(parentHeader.data()+4)!=0x808082EC
        || at<std::uint32_t>(parentHeader.data()+0x24)!=id.parent
        || at<std::uint32_t>(parentHeader.data()+0x2C)!=entity
        || !read.value(parent+0x1470,parentActor) || parentActor!=id.event.actor.actor) return false;
    return true;
}
struct Capture final {
    contract::Identity id{};contract::Signature signature{};
    std::uintptr_t row{},loco{};std::uint32_t handle{UINT32_MAX};
    std::array<float,4> origin{};std::array<float,3> dimensions{};
    std::array<float,4> firstHit{};float removedHeight{};
    bool valid{};
};
// Reject other actors before spending the bounded sample budget. Remember only
// the salted handle, never a live pointer; each accepted sample requalifies it.
bool matching_row(std::uintptr_t row) noexcept {
    const auto id=identity();if(!id.valid()) return false;
    Read read;std::uint32_t handle{};
    if(!read.value(row,handle)) return false;
    AcquireSRWLockShared(&g_lock);const auto known=g_locomotionHandle;ReleaseSRWLockShared(&g_lock);
    if(known!=UINT32_MAX) return known==handle;
    std::uintptr_t loco{};std::array<std::byte,0x30> header{};
    if(!read.resolve({handle,0,0},loco) || !read.copy(loco,header)
        || at<std::uint32_t>(header.data())!=0x80FCDF9A
        || at<std::uint32_t>(header.data()+4)!=0x80806908
        || at<std::uint32_t>(header.data()+0x24)!=handle
        || at<std::uint32_t>(header.data()+0x2C)!=id.event.actor.entity) return false;
    AcquireSRWLockExclusive(&g_lock);
    const bool same=g_identity==id;
    if(same) g_locomotionHandle=handle;
    ReleaseSRWLockExclusive(&g_lock);return same;
}
Capture capture(std::uintptr_t row,std::uintptr_t expectedLoco=0) noexcept {
    Capture value;value.id=identity();if(!value.id.valid() || row<0x10000 || row>UINTPTR_MAX-0x610) return value;
    Read read;std::array<std::byte,0x30> locoHeader{};std::uintptr_t loco{};
    if(!read.value(row,value.handle) || !read.resolve({value.handle,0,0},loco)
        || (expectedLoco && loco!=expectedLoco) || !read.copy(loco,locoHeader)
        || at<std::uint32_t>(locoHeader.data())!=0x80FCDF9A
        || at<std::uint32_t>(locoHeader.data()+4)!=0x80806908
        || at<std::int64_t>(locoHeader.data()+8)!=0x6D8
        || at<std::uint32_t>(locoHeader.data()+0x24)!=value.handle
        || !owner(read,value.id,at<std::uint32_t>(locoHeader.data()+0x2C))) return value;
    std::array<std::byte,0x610> data{};
    if(!read.copy(row,data) || at<std::uint32_t>(data.data())!=value.handle) return value;
    const auto hits=at<std::int32_t>(data.data()+0x1EC);
    if(!contract::result_layout(row,at<std::uintptr_t>(data.data()+0x1E0),
        at<std::int32_t>(data.data()+0x1E8),hits)) return value;
    value.signature.rawHits=hits;value.signature.prepared=at<std::uint8_t>(data.data()+4);
    value.signature.request=at<std::uint8_t>(data.data()+5);
    value.signature.filter=at<std::uint32_t>(data.data()+0x2C);
    value.origin=at<std::array<float,4>>(data.data()+0x10);
    value.dimensions=at<std::array<float,3>>(data.data()+0x20);
    value.removedHeight=at<float>(data.data()+0x608);
    if(hits) value.firstHit=at<std::array<float,4>>(data.data()+0x1F0);
    std::uint32_t providerHandle{},motionHandle{};std::uintptr_t provider{},motion{};
    std::array<std::byte,0x30> providerHeader{},motionHeader{};
    if(!read.value(loco+0x1DC,value.signature.contact) || !read.value(loco+0x1E0,value.signature.state)
        || !read.value(loco+0x2B8,providerHandle) || !read.resolve({providerHandle,0,0},provider)
        || !read.copy(provider,providerHeader) || at<std::uint32_t>(providerHeader.data())!=0x80BFDE20
        || at<std::uint32_t>(providerHeader.data()+4)!=0x80803D14
        || at<std::uint32_t>(providerHeader.data()+0x24)!=providerHandle
        || at<std::uint32_t>(providerHeader.data()+0x2C)!=value.id.event.actor.entity
        || !read.value(provider+0x50,value.signature.provider) || !read.value(provider+0x30,motionHandle)
        || !read.resolve({motionHandle,0,0},motion) || !read.copy(motion,motionHeader)
        || at<std::uint32_t>(motionHeader.data()+4)!=0x808069EE
        || at<std::uint32_t>(motionHeader.data()+0x24)!=motionHandle
        || at<std::uint32_t>(motionHeader.data()+0x2C)!=value.id.event.actor.entity
        || !read.value(motion+0x2A0,value.signature.mode)) return value;
    value.row=row;value.loco=loco;value.valid=true;return value;
}
void report(unsigned boundary,const Capture& before,const Capture& after) noexcept {
    if(!before.valid || !after.valid || !(before.id==after.id) || before.handle!=after.handle
        || before.row!=after.row || before.loco!=after.loco) return;
    AcquireSRWLockExclusive(&g_lock);
    const bool emit=g_identity==after.id && g_budget.admit(boundary,after.signature);
    ReleaseSRWLockExclusive(&g_lock);if(!emit) return;
    const auto& e=after.id.event;const auto& s=after.signature;
    std::array<char,1024> line{};
    const int count=std::snprintf(line.data(),line.size(),
        "ev=vance_contact stage=%s owner=%016llX incarnation=%llu registry=564C6ECE slot=0 generation=%u actor=%08X entity=%08X source=%08X loco=%08X prepared=%u request=%u hits_before=%d hits_after=%d filter=%08X origin=%.6f,%.6f,%.6f dimensions=%.8f,%.8f,%.8f first_hit=%.6f,%.6f,%.6f removed_height=%.6g contact=%08X state=%08X provider=%u policy=%u mutation=none",
        boundary==0?"async_complete":"contact_applied",
        static_cast<unsigned long long>(e.lease.activity.sessionId),
        static_cast<unsigned long long>(e.lease.activity.incarnation.value),e.lease.source.generation,
        e.actor.actor,e.actor.entity,e.sourceHandle,after.handle,s.prepared,s.request,
        before.signature.rawHits,s.rawHits,s.filter,after.origin[0],after.origin[1],after.origin[2],
        after.dimensions[0],after.dimensions[1],after.dimensions[2],
        after.firstHit[0],after.firstHit[1],after.firstHit[2],after.removedHeight,
        s.contact,s.state,s.provider,s.mode);
    if(count>0 && static_cast<std::size_t>(count)<line.size())
        core::log::write(core::log::Channel::client,core::log::Level::info,{line.data(),static_cast<std::size_t>(count)});
}
__declspec(noinline) void __fastcall job_hook(const void* context) noexcept {
    const hooking::CallGate::Scope scope{g_gate};Capture before;
    if(scope.accepts_side_effects() && identity().valid()) {
        Read read;std::uintptr_t query{};
        if(read.value(reinterpret_cast<std::uintptr_t>(context),query) && query>=0x10070
            && matching_row(query-0x70) && begin_sample(0))
            before=capture(query-0x70);
    }
    hooking::await_original(g_job)(context);
    if(scope.accepts_side_effects() && before.valid) report(0,before,capture(before.row,before.loco));
}
__declspec(noinline) void __fastcall consume_hook(void* loco,void* row) noexcept {
    const hooking::CallGate::Scope scope{g_gate};Capture before;
    if(scope.accepts_side_effects() && matching_row(reinterpret_cast<std::uintptr_t>(row)) && begin_sample(1))
        before=capture(reinterpret_cast<std::uintptr_t>(row),reinterpret_cast<std::uintptr_t>(loco));
    hooking::await_original(g_consume)(loco,row);
    if(scope.accepts_side_effects() && before.valid) report(1,before,capture(before.row,before.loco));
}
bool idle() noexcept {return g_gate.idle();}
std::byte* target(std::uintptr_t rva,const std::array<std::uint8_t,16>& expected) noexcept {
    Read read;std::array<std::uint8_t,16> actual{};
    return read.value(g_image+rva,actual) && actual==expected?reinterpret_cast<std::byte*>(g_image+rva):nullptr;
}
}
void observe_vance_contact_admission(const events::Event& event,std::uint32_t parent) noexcept {
    const hooking::CallGate::Scope scope{g_gate};const contract::Identity candidate{event,parent};
    if(!scope.accepts_side_effects() || !candidate.valid()) return;
    AcquireSRWLockExclusive(&g_lock);
    const bool changed=!(g_identity==candidate);
    if(changed) {g_identity=candidate;g_budget={};g_locomotionHandle=UINT32_MAX;}
    ReleaseSRWLockExclusive(&g_lock);
    if(changed) {
        std::array<char,320> line{};
        const int count=std::snprintf(line.data(),line.size(),
            "ev=vance_contact stage=armed owner=%016llX incarnation=%llu generation=%u actor=%08X entity=%08X source=%08X samples_per_boundary=40 interval_ms=250 mutation=none",
            static_cast<unsigned long long>(event.lease.activity.sessionId),
            static_cast<unsigned long long>(event.lease.activity.incarnation.value),event.lease.source.generation,
            event.actor.actor,event.actor.entity,event.sourceHandle);
        if(count>0 && static_cast<std::size_t>(count)<line.size())
            core::log::write(core::log::Channel::client,core::log::Level::info,{line.data(),static_cast<std::size_t>(count)});
    }
}
bool install_vance_contact_observer() noexcept {
    if(g_handles[0].attached) return g_gate.accepting();
    g_image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));if(!g_image) return false;
    const std::array<hooking::detour::Spec,2> specs{{
        {target(0xA2B010,{0x48,0x89,0x5C,0x24,0x10,0x57,0x48,0x83,0xEC,0x50,0x48,0x8B,0xD9,0xBA,0x01,0x00}),reinterpret_cast<void*>(&job_hook)},
        {target(0xD69E90,{0x48,0x89,0x5C,0x24,0x18,0x48,0x89,0x7C,0x24,0x20,0x55,0x48,0x8D,0x6C,0x24,0xB0}),reinterpret_cast<void*>(&consume_hook)}
    }};
    if(!specs[0].target || !specs[1].target || !hooking::detour::install(specs,g_handles)) return false;
    hooking::publish_original(g_job,reinterpret_cast<Job>(g_handles[0].original));
    hooking::publish_original(g_consume,reinterpret_cast<Consume>(g_handles[1].original));
    g_gate.accept();
    core::log::write(core::log::Channel::client,core::log::Level::info,
        "ev=vance_contact stage=install result=ok async=A2B010 consumer=D69E90 limit=24 mutation=none");
    return true;
}
void observe_vance_contact_retirement(const events::Event& event) noexcept {
    const hooking::CallGate::Scope scope{g_gate};
    if(!scope.accepts_side_effects() || event.kind!=events::Kind::retired) return;
    const auto current=identity();
    if(current.event.lease==event.lease && current.event.actor==event.actor
        && current.event.sourceHandle==event.sourceHandle) invalidate(current);
}
void quiesce_vance_contact_observer() noexcept {g_gate.quiesce();}
bool uninstall_vance_contact_observer() noexcept {
    quiesce_vance_contact_observer();if(!g_handles[0].attached) return true;
    const std::array<hooking::detour::ProtectedCodeEntry,6> protectedCode{{
        {reinterpret_cast<void*>(&job_hook)},{reinterpret_cast<void*>(&consume_hook)},
        {reinterpret_cast<void*>(&observe_vance_contact_admission)},
        {reinterpret_cast<void*>(&observe_vance_contact_retirement)},
        {reinterpret_cast<void*>(&hooking::call_gate_detail::enter)},
        {reinterpret_cast<void*>(&hooking::call_gate_detail::leave)}
    }};
    if(hooking::detour::uninstall(g_handles,protectedCode,idle)!=hooking::detour::UninstallResult::removed) return false;
    g_job.store(nullptr,std::memory_order_release);g_consume.store(nullptr,std::memory_order_release);
    g_identity={};g_budget={};g_locomotionHandle=UINT32_MAX;g_image=0;return true;
}
}
