#include "public_event_deferred_placement_observer.h"
#include "public_event_point_interface_capture.h"
#include "omega_enemy_native_reference.h"
#include "ambient_population_named_observer.h"
#include "../../../server/runtime/activity/public_event_key_bridge.h"
#include "../../../core/logging/log.h"
#include <Windows.h>
#include <atomic>
#include <cstdio>
namespace sunrise::client::hooks::bootflow::public_event_deferred_placement_observer {
namespace {
namespace capture=public_event_deferred_placement_capture;
namespace bridge=server::runtime::activity::public_event::deferred_bridge;
namespace feedback=capture::feedback;
std::atomic_uint64_t sequence{};
std::array<std::atomic_uint64_t,16> rejectedEpoch{},failedEpoch{};
std::uintptr_t image() noexcept {return reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));}
bool copy(std::uintptr_t from,std::span<std::byte> to) noexcept {
    SIZE_T copied{};
    return from>=0x10000 && to.size()<=8192 && from<=UINTPTR_MAX-to.size()
        && ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(from),to.data(),to.size(),&copied)
        && copied==to.size();
}
template<class T> bool read(std::uintptr_t from,T& to) noexcept {
    return copy(from,std::as_writable_bytes(std::span{&to,std::size_t{1}}));
}
bool add(std::uintptr_t from,std::uint64_t amount,std::uintptr_t& to) noexcept {
    if(from<0x10000 || amount>UINTPTR_MAX-from)return false;
    to=from+static_cast<std::uintptr_t>(amount);return true;
}
bool resolve(std::uint32_t handle,std::int64_t offset,std::uintptr_t& out) noexcept {
    if(handle==UINT32_MAX || offset<0 || offset>=0x2000000)return false;
    std::uintptr_t directory{},tables{},address{};
    if(!read(image()+0x2439C70,directory) || !read(directory,tables))return false;
    const auto shifted=static_cast<std::uint32_t>(static_cast<std::int32_t>(handle)>>13);
    const auto bucket=((std::uint64_t{shifted}|0xFFC0000ULL)>>18)&(shifted&0xFFFFU);
    std::array<std::byte,0x38> table{};
    if(!add(tables,bucket*0x40,address) || !copy(address,table))return false;
    const auto stride=feedback::field<std::int32_t>(table,0x30);
    if(stride<=0 || stride>0x100000 || !add(feedback::field<std::uintptr_t>(table,8),
        std::uint64_t{handle&0x1FFFU}*static_cast<unsigned>(stride),address))return false;
    std::uint64_t correction{};
    if(address>UINTPTR_MAX-8 || !read(address+8,correction))return false;
    return add(static_cast<std::uintptr_t>(omega_enemy_native_reference::corrected_base(address,correction,
        feedback::field<std::int32_t>(table,0x34))),static_cast<std::uint64_t>(offset),out);
}
bool source(std::uintptr_t component,capture::Identity& identity) noexcept {
    static const bool helperMatches=[]() noexcept {
        constexpr std::array<std::byte,16> expected{
            std::byte{0x0F},std::byte{0xB7},std::byte{0x41},std::byte{0x20},std::byte{0x4C},std::byte{0x8B},
            std::byte{0xD2},std::byte{0x25},std::byte{0xFF},std::byte{0x1F},std::byte{0},std::byte{0},
            std::byte{0x4C},std::byte{0x8B},std::byte{0xC9},std::byte{0x0F}};
        std::array<std::byte,16> actual{};return copy(image()+0x4E5640,actual) && actual==expected;
    }();
    std::uintptr_t pool{},classPointer{};std::uint32_t stride{},sourceClass{};
    return helperMatches && read(image()+0x1F92108,pool) && read(image()+0x1F92110,stride)
        && read(image()+0x1FA0D38,classPointer) && read(classPointer,sourceClass) && sourceClass==0x80809A3B
        && adventure_cue_native_identity::capture(component,pool,stride,copy,resolve,identity);
}

bool helpers() noexcept {
    static const bool ok=[]() noexcept {
        struct Row {std::uintptr_t rva;std::array<std::byte,16> bytes;};
        constexpr std::array<Row,2> rows{{
            {0x352310,{std::byte{0x48},std::byte{0x83},std::byte{0xec},std::byte{0x08},std::byte{0x44},std::byte{0x8b},std::byte{0x51},std::byte{0x04},std::byte{0x4c},std::byte{0x8b},std::byte{0xca},std::byte{0xc7},std::byte{0x02},std::byte{0xff},std::byte{0xff},std::byte{0xff}}},
            {0x323D40,{std::byte{0x0f},std::byte{0xb7},std::byte{0xc1},std::byte{0x48},std::byte{0x8d},std::byte{0x15},std::byte{0x36},std::byte{0x5e},std::byte{0xde},std::byte{0x01},std::byte{0x48},std::byte{0x0f},std::byte{0xaf},std::byte{0x05},std::byte{0x3e},std::byte{0x5e}}},
        }};
        for(const auto& row:rows){std::array<std::byte,16> actual{};if(!copy(image()+row.rva,actual) || actual!=row.bytes)return false;}
        return true;
    }();return ok;
}
bool body(std::uintptr_t object,std::span<std::byte> output) noexcept {
    // Exact9FEC30 ->323D40 backing-pool lookup, performed as reads so a
    // separately installed diagnostic resolver detour does not change this path.
    std::array<std::byte,0x50> header{};std::int64_t relative{};std::uint64_t stride{};
    std::uintptr_t row{},base{},pointer{};
    if(!copy(object,header) || !read(image()+0x2109B80,relative) || !read(image()+0x2109B90,stride)
        || stride<8 || stride>0x100000)return false;
    base=image()+0x2109B80+static_cast<std::uintptr_t>(relative);
    if(!add(base,std::uint64_t{feedback::field<std::uint16_t>(header,0x48)}*stride,row) || !read(row,base)
        || !add(base,feedback::field<std::uint64_t>(header,0x40),pointer))return false;
    return copy(pointer,output);
}
bool weak(std::uint64_t pair,std::uint32_t& output) noexcept {
    output=UINT32_MAX;
    __try {reinterpret_cast<std::uint32_t*(__fastcall*)(const void*,std::uint32_t*) noexcept>(image()+0x352310)(&pair,&output);}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
    return output!=UINT32_MAX && output==static_cast<std::uint32_t>(pair>>32);
}
bool entity(std::span<const std::byte> component,std::uint32_t& handle,std::array<std::byte,0x98>& output) noexcept {
    if(!weak(feedback::field<std::uint64_t>(component,0x440),handle))return false;
    std::uintptr_t pool{},address{};std::uint32_t stride{};
    return read(image()+0x1F93428,pool) && read(image()+0x1F93430,stride) && stride>=output.size() && stride<=0x100000
        && add(pool,std::uint64_t{handle&0x1FFFU}*stride,address) && copy(address,output)
        && feedback::field<std::uint32_t>(output,0xC)==handle && !(feedback::field<std::uint32_t>(output,4)&4U);
}
void report(const char* stage,const bridge::State& state,unsigned result) noexcept {
    std::array<char,640> line{};const auto& t=state.binding.ticket;
    const auto n=std::snprintf(line.data(),line.size(),
        "ev=public_event_gate stage=%s owner=%016llX incarnation=%llu boot=%016llX event=%llu epoch=%llu registry=%08X slot=%u definition=%08X generation=%u child=%08X created=%u point_ready=%u result=%u mutation=observe_only",
        stage,t.owner.sessionId,t.owner.incarnation.value,t.boot,t.event,state.binding.epoch,t.registry,t.slot,t.definition,t.generation,
        state.creation.child,state.created?1U:0U,state.ready?1U:0U,result);
    if(n>0 && static_cast<std::size_t>(n)<line.size())core::log::write(core::log::Channel::client,core::log::Level::info,{line.data(),static_cast<std::size_t>(n)});
}
bool point(const Context& context,const bridge::State& retained) noexcept {
    std::array<std::byte,0x98> before{},after{};
    if(!capture::retained(context,retained.creation,copy,resolve,source,body,entity,before))return false;
    std::uintptr_t pool{},address{},schema{};std::uint32_t stride{},schemaTag{};
    if(!read(image()+0x204FE88,schema) || !read(schema,schemaTag) || schemaTag!=0x80807F35
        || !read(image()+0x1F93428,pool) || !read(image()+0x1F93430,stride) || stride<before.size() || stride>0x100000
        || !add(pool,std::uint64_t{retained.creation.child&0x1FFFU}*stride,address))return false;
    std::array<std::byte,48> output{};
    const bool found=read_native_point_interface(reinterpret_cast<const void*>(address),output);
    std::uint64_t group{};
    if(!found || !public_event_point_interface_capture::qualify(retained.creation,before,output,copy,resolve,weak,group)
        || !capture::retained(context,retained.creation,copy,resolve,source,body,entity,after) || before!=after)return false;
    std::uintptr_t freshPool{},freshAddress{};std::uint32_t freshStride{};std::uint64_t freshGroup{};
    if(!read(image()+0x1F93428,freshPool) || !read(image()+0x1F93430,freshStride)
        || freshPool!=pool || freshStride!=stride
        || !add(freshPool,std::uint64_t{retained.creation.child&0x1FFFU}*freshStride,freshAddress) || freshAddress!=address
        || !public_event_point_interface_capture::qualify(retained.creation,after,output,copy,resolve,weak,freshGroup)
        || freshGroup!=group)return false;
    return bridge::point_ready(retained.binding,retained.creation,sequence.fetch_add(1)+1,group);
}
}
Context begin(void* pointer) noexcept {
    const auto component=reinterpret_cast<std::uintptr_t>(pointer);std::array<std::byte,16> header{};
    if(!copy(component,header))return {};
    const auto state=bridge::lookup(feedback::field<std::uint32_t>(header,0));
    if(!state.binding.epoch || state.created || !helpers())return {};
    Context context{};const auto result=capture::begin(state.binding,component,copy,resolve,source,body,context);
    if(result!=capture::Result::accepted && rejectedEpoch[state.binding.ticket.slot%rejectedEpoch.size()].exchange(state.binding.epoch)!=state.binding.epoch)
        report("capture_rejected",state,static_cast<unsigned>(result));
    return context;
}
bool finish(void* pointer,const Context& before,bool created) noexcept {
    if(!before.binding.epoch)return false;
    feedback::Observation observation{};
    const bool qualified=capture::finish(before,reinterpret_cast<std::uintptr_t>(pointer),created,sequence.fetch_add(1)+1,
        copy,resolve,source,body,entity,observation);
    const bool accepted=qualified && bridge::created(before.binding,observation);
    if(accepted) {
        static_cast<void>(server::runtime::activity::public_event::keys::bridge::created(observation));
        refresh(pointer);
    }
    if(accepted || failedEpoch[before.binding.ticket.slot%failedEpoch.size()].exchange(before.binding.epoch)!=before.binding.epoch)
        report("native_create",bridge::lookup(before.binding.ticket.definition),accepted?1U:0U);return accepted;
}
void refresh(void* pointer) noexcept {
    const auto component=reinterpret_cast<std::uintptr_t>(pointer);std::array<std::byte,16> header{};
    if(!copy(component,header))return;
    const auto state=bridge::lookup(feedback::field<std::uint32_t>(header,0));
    if(!state.binding.epoch || !state.created || state.ready || !state.binding.ticket.pointComponent || !helpers())return;
    Context context{};
    if(capture::begin(state.binding,component,copy,resolve,source,body,context)!=capture::Result::accepted)return;
    const bool ready=point(context,state);
    if(ready)report("native_point_interface",bridge::lookup(state.binding.ticket.definition),1);
}
bool live_entity(std::uint32_t handle) noexcept {
    std::uintptr_t pool{},address{};std::uint32_t stride{};std::array<std::byte,0x98> row{};
    return handle!=UINT32_MAX && read(image()+0x1F93428,pool) && read(image()+0x1F93430,stride)
        && stride>=row.size() && stride<=0x100000 && add(pool,std::uint64_t{handle&0x1FFFU}*stride,address)
        && copy(address,row) && feedback::field<std::uint32_t>(row,0xC)==handle && !(feedback::field<std::uint32_t>(row,4)&4U);
}
std::uint32_t holder_context(const void* pointer) noexcept {
    // D96040 ->9EC890/9EC700 ordinary runtime-component route. Bound the
    // ancestor walk and qualify the full root handle and resulting live entity.
    const auto context=reinterpret_cast<std::uintptr_t>(pointer);
    std::array<std::byte,16> reference{},fresh{};
    if(context<0x10000 || context>UINTPTR_MAX-0x28 || !copy(context+0x18,reference))return UINT32_MAX;
    const auto member=feedback::field<std::uint32_t>(reference,0),type=feedback::field<std::uint32_t>(reference,4);
    const auto offset=feedback::field<std::int64_t>(reference,8);
    std::uint32_t alternative{};
    if(!read(image()+0x27449B8,alternative) || type==alternative || member==UINT32_MAX
        || (member&0xC0000000U)==0x80000000U)return UINT32_MAX;
    std::uintptr_t first{},at{};
    if(!resolve(member,offset,first))return UINT32_MAX;
    std::array<std::byte,0x30> header{};
    if(!copy(first,header) || feedback::field<std::uint32_t>(header,4)!=type)return UINT32_MAX;
    capture::Identity identity{};
    if(!source(first,identity) || identity.source.member!=member || identity.source.offset!=offset)return UINT32_MAX;
    at=first;
    for(unsigned depth=0;depth<32;++depth) {
        if(!copy(at,header))return UINT32_MAX;
        const auto relative=feedback::field<std::int64_t>(header,0x10);
        if(relative) {
            if(relative<=-0x2000000 || relative>=0x2000000 || at>UINTPTR_MAX-0x10)return UINT32_MAX;
            const auto base=at+0x10;
            if(relative<0 && static_cast<std::uint64_t>(-relative)>base)return UINT32_MAX;
            const auto next=relative<0?base-static_cast<std::uint64_t>(-relative):base+static_cast<std::uint64_t>(relative);
            if(next==at || next<0x10000)return UINT32_MAX;at=next;continue;
        }
        const auto root=feedback::field<std::uint32_t>(header,0x24);
        std::uintptr_t object{},again{};std::array<std::byte,0x30> rootBytes{},verified{};
        if(root==UINT32_MAX || !resolve(root,0,object) || !copy(object,rootBytes)
            || feedback::field<std::uint32_t>(rootBytes,0x24)!=root)return UINT32_MAX;
        const auto entity=feedback::field<std::uint32_t>(rootBytes,0x2C);
        capture::Identity current{};
        if(!live_entity(entity) || !source(first,current) || current!=identity
            || !copy(context+0x18,fresh) || fresh!=reference || !resolve(root,0,again) || again!=object
            || !copy(again,verified) || verified!=rootBytes)return UINT32_MAX;
        return entity;
    }
    return UINT32_MAX;
}
}
