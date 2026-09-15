#include <Windows.h>
#include "hijacked_placements.h"
#include "omega_enemy_lair_receipts.h"
#include "native_hook_ownership.h"
#include "ambient_population_named_observer.h"
#include "gateway_native_read.h"
#include "eater_source_retirement.h"
#include "../graphics/hijacked_frame_timing.h"
#include "../../hooking/call_gate.h"
#include "../../hooking/detour.h"
#include "../../../state/activity/hijacked/runtime.h"
#include "../../../state/activity/hijacked/placement_ownership.h"
#include "../../../state/activity/hijacked/retirement_identity.h"
#include "../../../state/activity/hijacked/backtracking_identity.h"
#include "../../../state/activity/hijacked/retirement_unload.h"
#include "../../../state/activity/eater_of_worlds/runtime.h"
#include "../../../core/logging/log.h"
#include <atomic>
#include <cstdio>
#include <intrin.h>
namespace sunrise::client::hooks::bootflow::hijacked_placements {
namespace {
hooking::CallGate gate;
std::array<hooking::detour::Handle,4> hooks{};
using Create=std::int32_t*(__fastcall*)(std::int32_t*,const std::byte*,std::uint32_t,std::uint32_t) noexcept;
namespace mission=state::activity::hijacked;
namespace timing=graphics::hijacked_frame_timing;
using gateway_native::Read;using gateway_native::at;
using Context=std::uintptr_t(__fastcall*)() noexcept;
using Authority=bool(__fastcall*)() noexcept;
using Find=std::uint32_t*(__fastcall*)(const std::uint64_t*,std::uint32_t*) noexcept;
using RetireSource=void(__fastcall*)(std::uintptr_t) noexcept;
using UnloadRegion=void(__fastcall*)(std::uintptr_t,std::uint32_t,std::uintptr_t) noexcept;
using FindComponent=bool(__fastcall*)(std::uintptr_t,std::uint32_t,std::uint32_t,void*) noexcept;
std::atomic<Create> create{};std::atomic<RetireSource> retireSource{};std::atomic<DWORD> gameThread{};
std::atomic<UnloadRegion> unloadRegion{};std::atomic<FindComponent> findComponent{};
std::atomic_flag retirementDispatching=ATOMIC_FLAG_INIT;
SRWLOCK lock=SRWLOCK_INIT;
std::uintptr_t image{};Context context{};Authority authority{};Find find{};
state::activity::coo::Generation owner{};
std::uint64_t nextPoll{};
std::array<std::uint8_t,mission::kSpawns.size()> lastStatus{};
struct Entry {std::uintptr_t rva;std::array<unsigned char,16> prefix;};
constexpr Entry entries[]{
 {0x424D10,{0x40,0x53,0x56,0x57,0x41,0x54,0x41,0x55,0xB8,0x80,0x90,0x00,0x00,0xE8,0x0E,0x7E}},
 {0x557690,{0x40,0x53,0x48,0x83,0xEC,0x50,0x83,0xC9,0xFF,0x41,0x8B,0xC0,0x66,0x89,0x4C,0x24}},
 {0xA07BE0,{0x48,0x83,0xEC,0x78,0x33,0xD2,0x48,0x8D,0x4C,0x24,0x20,0xE8,0x40,0xA9,0xB5,0xFF}},
 {0x56A8F0,{0x48,0x89,0x5C,0x24,0x18,0x48,0x89,0x74,0x24,0x20,0x57,0x48,0x83,0xEC,0x40,0x48}},
 {0x4EC1A0,{0x40,0x53,0x48,0x83,0xEC,0x30,0x48,0x8B,0xD9,0xC7,0x44,0x24,0x40,0x01,0x00,0x00}},
 {0x16C0F00,{0x40,0x53,0x48,0x83,0xEC,0x20,0x48,0x8B,0x1D,0xF3,0x0D,0x99,0x01,0x48,0x85,0xDB}},
 {0xB3FFA0,{0x48,0x83,0xEC,0x28,0xE8,0xE7,0xC6,0xBB,0x00,0x84,0xC0,0x74,0x19,0xE8,0xBE,0xC6}},
 {0xA1F8D0,{0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xEC,0x50,0x83}}
};
void report(const char* stage,const mission::Placement& p,std::uint32_t handle) noexcept {
    std::array<char,320> line{};const auto n=std::snprintf(line.data(),line.size(),
        "ev=hijacked stage=placement_%s run=%llu generation=%u registry=%08X source=%u bubble=%u table=%08X record=%u guid=%016llX entity=%08X handle=%08X",
        stage,static_cast<unsigned long long>(owner.run),owner.value,p.registry,p.source,p.bubble,p.table,p.record,
        static_cast<unsigned long long>(p.guid),p.entity,handle);
    if(n>0 && static_cast<std::size_t>(n)<line.size()) {core::log::write(core::log::Channel::client,core::log::Level::info,{line.data(),static_cast<std::size_t>(n)});}
}
bool current(const mission::Request& request,state::activity::coo::Asset source) noexcept {
    const auto live=mission::request();const auto index=mission::asset_index(source);
    return live.owner==request.owner && live.frame.enabled && !live.frame.finished
        && index<live.frame.native.size() && live.frame.native[index].active && !live.frame.native[index].prepared;
}
bool in_context(std::uint16_t expected) noexcept {
    Read read{image};std::uint32_t bubble=UINT32_MAX;
    const auto c=context();return c && read.value(c+4,bubble) && bubble==expected && authority();
}
struct SourceIdentity {
    std::uintptr_t root{},row{},address{};std::uint32_t handle{UINT32_MAX};
    friend bool operator==(const SourceIdentity&,const SourceIdentity&)=default;
};
// Read the currently selected native registry, not a cached source pointer.
// This walk was observed in the live Hijacked source inspection.
bool native_source(std::size_t index,std::uint32_t generation,SourceIdentity& identity,bool diagnostic=false) noexcept {
    const auto& spawn=mission::kSpawns[index];Read read{image};
    std::uint16_t key{};std::uintptr_t relative{},stride{},root{};
    constexpr std::uintptr_t tableRva=0x2109B80;
    if(!read.value(image+0x1F91FE8,key) || key==UINT16_MAX
        || !read.value(image+tableRva,relative) || !read.value(image+0x2109B90,stride)
        || stride<8 || stride>0x100000 || relative>UINTPTR_MAX-image-tableRva) {return false;}
    const auto table=image+tableRva+relative;
    const auto displacement=static_cast<std::uintptr_t>(key)*stride;
    if(table>UINTPTR_MAX-displacement || !read.value(table+displacement,root)) {return false;}
    std::uint32_t count{};if(!read.value(root+8,count) || count>128) {return false;}
    for(std::uint32_t i=0;i<count;++i) {
        std::array<std::uint32_t,6> group{};
        if(!read.value(root+0xC+static_cast<std::uintptr_t>(i)*24,group)) {return false;}
        if(group[2]!=spawn.registry) {continue;}
        if(group[0]>4096 || spawn.source>=group[0] || group[1]>65535) {return false;}
        const auto row=root+0xC20+(static_cast<std::uintptr_t>(group[1])+spawn.source)*40;
        gateway_native::Ref ref{};std::uintptr_t address{},definition{};
        if(!read.value(row,ref) || ref.handle==UINT32_MAX || ref.kind!=0x80809A3BU || ref.offset!=0
            || !read.resolve(ref.handle,address)) {return false;}
        std::array<std::byte,0x200> bytes{};
        if(!read.copy(address,bytes)) {return false;}
        // Readback may observe an unapplied retirement generation. Creation keeps
        // its exact generation check. Readback/capture also authenticates the source
        // definition and catalog identity; only retired, retained entities can be removed.
        const auto expected=diagnostic?at<std::uint32_t>(bytes.data()+0x1FC):generation;
        if(!mission::placement_source_identity(bytes,ref.handle,spawn.definition,spawn.offset,expected)
            || !read.resolve(spawn.definition,definition)) {return false;}
        std::array<std::byte,8> scoped{};
        if(!read.copy(definition+static_cast<std::uintptr_t>(spawn.offset)+0x30,scoped)
            || at<std::uint32_t>(scoped.data())!=spawn.registry || at<std::uint8_t>(scoped.data()+4)!=1
            || at<std::int16_t>(scoped.data()+6)!=static_cast<std::int16_t>(spawn.source)) {return false;}
        identity={root,row,address,ref.handle};return true;
    }
    return false;
}
bool same_source(std::size_t index,std::uint32_t generation,const SourceIdentity& identity) noexcept {
    SourceIdentity live{};return native_source(index,generation,live) && live==identity;
}
// The generation boundary can run after the selected registry has changed.
// Authenticate its actual argument against the current mission catalog instead
// of trusting a cached row or requiring the old registry to remain selected.
bool retirement_source_matches(const mission::Request& request,std::uint16_t slot,
    std::uintptr_t address,std::uint32_t handle) noexcept {
    if(slot<1 || slot>7 || !request.owner.valid() || !request.frame.enabled || request.frame.finished) {return false;}
    const auto asset=mission::find(0x3E9B74F3U,1,slot)->asset;
    const auto& state=request.frame.native[mission::asset_index(asset)];
    if(!state.managed || (!state.active && !state.retired)) {return false;}
    const auto& spawn=mission::kSpawns[mission::spawn_index(asset)];
    Read read{image};std::uintptr_t actual{},definition{};std::array<std::byte,0x200> bytes{};
    if(!read.resolve(handle,actual) || actual!=address || !read.copy(address,bytes)) {return false;}
    const auto generation=at<std::uint32_t>(bytes.data()+0x1FC);
    if(generation!=request.frame.spawnGeneration && generation!=state.generation) {return false;}
    if(!mission::placement_source_identity(bytes,handle,spawn.definition,spawn.offset,generation)
        || !read.resolve(spawn.definition,definition)) {return false;}
    std::array<std::byte,8> scoped{};
    return read.copy(definition+static_cast<std::uintptr_t>(spawn.offset)+0x30,scoped)
        && at<std::uint32_t>(scoped.data())==spawn.registry && at<std::uint8_t>(scoped.data()+4)==1
        && at<std::int16_t>(scoped.data()+6)==static_cast<std::int16_t>(slot);
}
bool retirement_allocator_ready() noexcept {
    // 98F40 obtains this exact TLS service; the camera-thread crash dereferenced
    // a null service at 98F66. A world context or thread id cannot substitute.
    Read read{image};DWORD index{};std::uintptr_t service{},vtable{};
    if(!read.value(image+0x20BBB30,index) || index==TLS_OUT_OF_INDEXES) {return false;}
    const auto error=GetLastError();const auto tls=reinterpret_cast<std::uintptr_t>(TlsGetValue(index));SetLastError(error);
    if(!tls || !read.value(tls+0x58,service) || !service || !read.value(service,vtable) || !vtable) {return false;}
    // Native allocation/free wrappers use these methods; the observed failing
    // free at 98F40 dispatches through +0x20, not the first virtual method.
    for(const auto offset:{0x08U,0x10U,0x20U}) {
        std::uintptr_t method{};MEMORY_BASIC_INFORMATION memory{};
        if(!read.value(vtable+offset,method) || !method
            || VirtualQuery(reinterpret_cast<const void*>(method),&memory,sizeof memory)!=sizeof memory
            || memory.State!=MEM_COMMIT || (memory.Protect&(PAGE_GUARD|PAGE_NOACCESS))
            || !(memory.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))) {return false;}
    }
    return true;
}
#include "hijacked_retirement_logging.inl"
#include "hijacked_retirement_cleanup.inl"
#include "hijacked_backtracking.inl"
#include "hijacked_retirement_unload.inl"
#include "eater_source_retirement.inl"
bool descriptor(const mission::Placement& p,std::uintptr_t& address) noexcept {
    Read read{image};std::uintptr_t table{};std::uint32_t count{};std::int64_t relative{};
    if(!read.resolve(p.table,table) || !read.value(table+8,count) || count>4096 || p.record>=count
        || !read.value(table+0x10,relative) || relative<0 || relative>0x100000
        || 0x20+static_cast<std::uint64_t>(relative)+static_cast<std::uint64_t>(p.record)*0x90!=p.offset) {return false;}
    address=table+p.offset;std::array<std::byte,0x90> row{};
    return read.copy(address,row) && mission::placement_hash(row)==p.descriptorHash;
}
// GUID lookup is native and uncached. Verify the complete salted object identity
// and its original table/row before accepting the actual native pose provider.
bool placed(const mission::Placement& p,std::uint32_t handle) noexcept {
    if(handle==UINT32_MAX) {return false;}
    Read read{image};std::uintptr_t table{};std::uint32_t stride{};
    if(!read.value(image+0x1F93428,table) || !read.value(image+0x1F93430,stride) || stride<0xE0 || stride>0x100000) {return false;}
    const auto object=table+static_cast<std::uintptr_t>(handle&0x1FFFU)*stride;
    std::array<std::byte,0xA0> b{};
    if(!read.copy(object,b) || !mission::placed_identity(b,p,handle)) {return false;}
    alignas(16) std::array<std::byte,0x30> pose{};
    if(!read_native_point_interface(reinterpret_cast<const void*>(object),pose)) {return false;}
    const auto weak=at<gateway_native::Weak>(pose.data()+0x18);
    const auto offset=at<std::int64_t>(pose.data()+0x20);std::uintptr_t base{};
    std::array<std::byte,16> target{};
    // The provider is an arbitrary native call: resolve its weak target and
    // recheck the original salted object after it returns.
    return read.weak(weak) && offset>=0 && offset<0x100000 && read.resolve(weak.handle,base)
        && base<=UINTPTR_MAX-static_cast<std::uintptr_t>(offset)
        && read.copy(base+static_cast<std::uintptr_t>(offset),target)
        && read.weak(weak) && read.copy(object,b) && mission::placed_identity(b,p,handle);
}
__declspec(noinline) void update() noexcept {
    const auto fn=create.load(std::memory_order_acquire);
    if(!fn || !context || !authority || !find || !TryAcquireSRWLockExclusive(&lock)) {return;}
    struct Unlock {~Unlock() {ReleaseSRWLockExclusive(&lock);}} unlock;
    const auto request=[] {
        const timing::PostSpan requestTiming(timing::Kind::placement_request);
        return mission::request();
    }();
    if(!request.owner.valid() || !request.frame.enabled || request.frame.finished) {timing::clear_scope();return;}
    timing::set_scope(request.owner.run);
    const timing::PostSpan updateTiming(timing::Kind::update);
    if(owner!=request.owner) {owner=request.owner;nextPoll=0;lastStatus={};}
    backtracking_resume(request);
    const auto now=GetTickCount64();
    {const timing::PostSpan captureTiming(timing::Kind::capture);retirement_capture(request);}
    {const timing::PostSpan readbackTiming(timing::Kind::readback);retirement_logging(request,now);}
    if(now<nextPoll) {return;}bool attempted=false;
    for(std::size_t i=0;i<mission::kSpawns.size();++i) {
        const auto& spawn=mission::kSpawns[i];const auto source=mission::find(spawn.registry,1,spawn.source)->asset;
        const auto& state=request.frame.native[mission::asset_index(source)];
        if(!state.active || state.prepared) {continue;}
        std::uint8_t status=1;const mission::Placement* first{};SourceIdentity native{};
        for(const auto& p:mission::kPlacements) {
            if(p.registry!=spawn.registry || p.source!=spawn.source) {continue;}
            if(!first) {first=&p;}
            if(!current(request,source)) {return;}
            if(!in_context(p.bubble)) {status=2;break;}
            if(!native.address) {
                if(!native_source(i,state.generation,native)) {status=6;break;}
            } else if(!same_source(i,state.generation,native)) {status=6;break;}
            attempted=true;
            std::uintptr_t row{};if(!descriptor(p,row)) {status=3;break;}
            std::uint32_t handle=UINT32_MAX;find(&p.guid,&handle);
            if(handle==UINT32_MAX) {
                if(!current(request,source) || !in_context(p.bubble) || !descriptor(p,row)) {status=3;break;}
                if(!same_source(i,state.generation,native)) {status=6;break;}
                std::int32_t created=-1;
                // Same arguments as native569F00. Keep original relative references,
                // authored transform, GUID and native table-owned retirement.
                fn(&created,reinterpret_cast<const std::byte*>(row),p.table,p.record);
                if(!current(request,source)) {return;}
                find(&p.guid,&handle);
                if(created==-1 || handle!=static_cast<std::uint32_t>(created)) {status=4;break;}
                report("created",p,handle);
            }
            if(!placed(p,handle)) {status=5;break;}
            if(!same_source(i,state.generation,native)) {status=6;break;}
        }
        if(!first) {continue;}
        if(status==1) {
            if(!current(request,source)) {return;}
            if(!in_context(first->bubble)) {status=2;}
            else if(!same_source(i,state.generation,native)) {status=6;}
            else {mission::observe_prepared(request.owner,source);}
        }
        if(lastStatus[i]!=status) {
            lastStatus[i]=status;
            constexpr const char* stages[]{"unused","ready","pending_context","descriptor_unavailable","create_pending","provider_pending","source_pending"};
            report(stages[status],*first,UINT32_MAX);
        }
    }
    // A camera poll outside the native bubble cannot delay a subsequent
    // constructor callback inside the proper creation context.
    if(attempted) {nextPoll=now+250;}
}
__declspec(noinline) std::int32_t* __fastcall create_object(std::int32_t* output,const std::byte* descriptor,
    std::uint32_t table,std::uint32_t record) noexcept {
    const hooking::CallGate::Scope scope(gate);
    const auto fn=hooking::await_original(create);
    // One constructor detour supplies both native ambient and mission receipts.
    const auto ambient=scope.accepts_side_effects()
        ?capture_ambient_named_construction(descriptor,table,record):AmbientNamedConstruction{};
    auto* result=fn(output,descriptor,table,record);
    if(scope.accepts_side_effects())complete_ambient_named_construction(ambient,descriptor,output,result);
    if(scope.accepts_side_effects() && GetCurrentThreadId()==gameThread.load(std::memory_order_acquire)) {update();}
    return result;
}
__declspec(noinline) void __fastcall retire_source(std::uintptr_t address) noexcept {
    const hooking::CallGate::Scope scope(gate);
    const auto fn=hooking::await_original(retireSource);
    if(scope.accepts_side_effects() && !retirementDispatching.test_and_set(std::memory_order_acquire)) {
        struct Release {~Release() {retirementDispatching.clear(std::memory_order_release);}} release;
        const auto request=mission::request();Read read{image};std::uint32_t handle{UINT32_MAX};
        if(request.owner.valid() && request.frame.enabled && !request.frame.finished && read.value(address+0x30,handle)) {
            for(std::uint16_t slot=1;slot<=7;++slot) {
                if(!retirement_source_matches(request,slot,address,handle)) {continue;}
                const auto asset=mission::find(0x3E9B74F3U,1,slot)->asset;
                if(!request.frame.native[mission::asset_index(asset)].retired) {break;}
                std::array<gateway_native::Weak,64> owned{};std::uint32_t count{};unsigned captured{};
                const bool snapshot=read.value(address+0x314,count) && count<=owned.size()
                    && read.copy(address+0x318,std::as_writable_bytes(std::span(owned).first(count)));
                // Snapshot every weak reference before callbacks can compact the list.
                if(snapshot) {
                    for(std::uint32_t i=0;i<count;++i) {
                        Read weakRead{image};
                        if(weakRead.weak(owned[i]) && capture_retirement_target(request,slot,{0,0,address,handle},owned[i].handle)) {++captured;}
                    }
                }
                resolve_retirement_origins(request);
                const bool ready=retirement_allocator_ready();
                std::array<char,320> line{};
                std::snprintf(line.data(),line.size(),
                    "ev=hijacked_retirement stage=source_boundary run=%llu source=%u handle=%08X owned_count=%u snapshot=%u captured=%u allocator_ready=%u",
                    static_cast<unsigned long long>(request.owner.run),slot,handle,count,snapshot?1U:0U,captured,ready?1U:0U);
                core::log::write(core::log::Channel::client,core::log::Level::info,line.data());
                if(ready) {retirement_dispatch(request);}
                break;
            }
        }
        retire_strike_bond_boss(address,retirement_allocator_ready());
        capture_eater_source_retirement(address);
        // Every path forwards native retirement, including missing TLS and
        // unrelated sources. Keep reentrant cleanup disabled across this call.
        fn(address);return;
    }
    fn(address);
}
bool idle() noexcept {return gate.idle();}
void install_report(const char* result,std::uintptr_t rva=0x575690) noexcept {
    std::array<char,192> line{};const auto n=std::snprintf(line.data(),line.size(),
        "ev=hijacked stage=placement_install result=%s target=+%llX owner=production",
        result,static_cast<unsigned long long>(rva));
    if(n>0 && static_cast<std::size_t>(n)<line.size()) {
        core::log::write(core::log::Channel::client,core::log::Level::info,{line.data(),static_cast<std::size_t>(n)});
    }
}
}
bool install() noexcept {
    if(create.load(std::memory_order_acquire)) {return gate.accepting();}
    gate.quiesce();image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));Read read{image};
    for(const auto& entry:entries) {
        std::array<unsigned char,16> actual{};
        if(!read.value(image+entry.rva,actual) || actual!=entry.prefix) {install_report("prefix_mismatch",entry.rva);return false;}
    }
    // Exact native engine lifetime APIs; no additional physical detour.
    const std::array<Entry,2> lifetimeEntries{{
        {0x170FC90,{0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x6C,0x24,0x18,0x48,0x89,0x74,0x24,0x20,0x57}},
        {0x16FC600,{0x40,0x53,0x48,0x83,0xEC,0x20,0x33,0xDB,0x38,0x1D,0xE2,0xB4,0x93,0x00,0x74,0x11}}
    }};
    backtrackingNativeReady=true;
    for(const auto& entry:lifetimeEntries) {
        std::array<unsigned char,16> actual{};
        if(!read.value(image+entry.rva,actual) || actual!=entry.prefix) {backtrackingNativeReady=false;install_report("lifetime_prefix_mismatch",entry.rva);return false;}
    }
    constexpr std::array<unsigned char,20> expected{0x40,0x55,0x53,0x56,0x41,0x56,0x41,0x57,
        0x48,0x8D,0x6C,0x24,0xA0,0x48,0x81,0xEC,0x60,0x01,0x00,0x00};
    std::array<unsigned char,20> actual{};
    if(!read.value(image+0x575690,actual) || actual!=expected) {install_report("prefix_mismatch");return false;}
    // The boolean-only branch must still call native mark/remove immediately.
    // Do not turn a component lookup into a fabricated reference for any caller.
    constexpr std::array<unsigned char,16> discardCall{0xE8,0xD2,0xF9,0xB4,0xFF,0x84,0xC0,0x74,
        0x07,0x8B,0xCB,0xE8,0x27,0x2C,0xB6,0xFF};
    std::array<unsigned char,16> actualDiscard{};std::uintptr_t discardType{};std::uint32_t component{};
    if(!read.value(image+0xA07CB9,actualDiscard) || actualDiscard!=discardCall
        || !read.value(image+0x20503C0,discardType) || !read.value(discardType,component)
        || component!=mission::kUnloadDiscardComponent) {install_report("discard_call_mismatch",0xA07CB9);return false;}
    std::uintptr_t tlsAccessor{};
    if(!read.value(image+0x3CC7890,tlsAccessor) || tlsAccessor!=reinterpret_cast<std::uintptr_t>(&TlsGetValue)) {
        install_report("tls_accessor_mismatch",0x3CC7890);return false;
    }
    context=reinterpret_cast<Context>(image+0x16C0F00);authority=reinterpret_cast<Authority>(image+0xB3FFA0);
    find=reinterpret_cast<Find>(image+0xA1F8D0);
    const std::array specs{
        hooking::detour::Spec{reinterpret_cast<void*>(image+native_hook_ownership::kHijackedPlacements[0]),reinterpret_cast<void*>(&create_object)},
        hooking::detour::Spec{reinterpret_cast<void*>(image+native_hook_ownership::kHijackedPlacements[1]),reinterpret_cast<void*>(&retire_source)},
        hooking::detour::Spec{reinterpret_cast<void*>(image+native_hook_ownership::kHijackedPlacements[2]),reinterpret_cast<void*>(&unload_region)},
        hooking::detour::Spec{reinterpret_cast<void*>(image+native_hook_ownership::kHijackedPlacements[3]),reinterpret_cast<void*>(&find_component)}};
    if(!hooking::detour::install(specs,hooks)) {
        install_report("attach_failed");return false;
    }
    hooking::publish_original(create,reinterpret_cast<Create>(hooks[0].original));
    hooking::publish_original(retireSource,reinterpret_cast<RetireSource>(hooks[1].original));
    hooking::publish_original(unloadRegion,reinterpret_cast<UnloadRegion>(hooks[2].original));
    hooking::publish_original(findComponent,reinterpret_cast<FindComponent>(hooks[3].original));gate.accept();
    install_report("ok");return true;
}
void quiesce() noexcept {gate.quiesce();}
bool uninstall() noexcept {
    gate.quiesce();bool attached=false;for(const auto& hook:hooks) {attached=attached || hook.attached;}if(!attached) {return true;}
    const std::array protectedEntries{
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&create_object)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&poll)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&update)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&retirement_logging)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&retirement_capture)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&retirement_dispatch)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&retire_source)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&capture_eater_source_retirement)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&poll_eater_source_retirements)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&unload_region)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&find_component)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&retain_enemy)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&hooking::call_gate_detail::enter)},
        hooking::detour::ProtectedCodeEntry{reinterpret_cast<void*>(&hooking::call_gate_detail::leave)}};
    if(hooking::detour::uninstall(hooks,protectedEntries,&idle)!=hooking::detour::UninstallResult::removed) {
        install_report("retained_on_uninstall");return false;
    }
    create.store(nullptr,std::memory_order_release);retireSource.store(nullptr,std::memory_order_release);
    unloadRegion.store(nullptr,std::memory_order_release);findComponent.store(nullptr,std::memory_order_release);
    hooks={};context=nullptr;authority=nullptr;find=nullptr;backtrackingNativeReady=false;backtrackingReturns={};backtrackingHasReturns=false;
    clear_eater_retirements();image=0;gameThread=0;owner={};nextPoll=0;lastStatus={};return true;
}
void retain_enemy(std::uint64_t run,std::uint16_t slot,std::uint32_t sourceHandle,std::uint32_t actor) noexcept {
    const hooking::CallGate::Scope scope(gate);
    if(!scope.accepts_side_effects() || slot<1 || slot>7) {return;}
    const auto request=mission::request();
    if(!request.owner.valid() || request.owner.run!=run || !request.frame.enabled || request.frame.finished) {return;}
    const auto asset=mission::find(0x3E9B74F3U,1,slot)->asset;
    const auto& state=request.frame.native[mission::asset_index(asset)];
    SourceIdentity source{};
    if(state.managed && (state.active || state.retired)
        && native_source(mission::spawn_index(asset),state.generation,source,true) && source.handle==sourceHandle) {
        static_cast<void>(capture_retirement_target(request,slot,source,actor));
    }
}
void poll() noexcept {
    const hooking::CallGate::Scope scope(gate);if(!scope.accepts_side_effects()) {return;}
    gameThread.store(GetCurrentThreadId(),std::memory_order_release);update();poll_eater_source_retirements();
}
}
