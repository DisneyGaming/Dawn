#include "ambient_population_named_observer.h"
#include "native_hook_ownership.h"
#include "ambient_population_named_identity.h"
#include "native_population_retirement.h"
#include "omega_enemy_native_reference.h"
#include "../../hooking/call_gate.h"
#include "../../hooking/detour.h"
#include "../../../core/logging/log.h"
#include <Windows.h>
#include <atomic>
#include <cstdio>

namespace sunrise::client::hooks::bootflow {
namespace {
namespace points=server::runtime::activity::ambient_population::named_points;
namespace identity=ambient_named_identity;
using Remove=void(__fastcall*)(std::uint32_t);
using Release=void(__fastcall*)(void*,std::uint32_t);
using Interface=std::uint8_t(__fastcall*)(const void*,void*);
hooking::CallGate gate;
std::array<hooking::detour::Handle,3> handles{};
std::atomic<Remove> removeList{};std::atomic<Interface> pointInterface{};
std::atomic<Release> releaseSlot{};
std::uintptr_t image{};
bool copy(std::uintptr_t from,std::span<std::byte> to) noexcept {
    SIZE_T copied{};
    return from>=0x10000 && to.size()<=4096 && from<=UINTPTR_MAX-to.size()
        && ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(from),to.data(),to.size(),&copied)
        && copied==to.size();
}
template<class T> bool read(std::uintptr_t from,T& value) noexcept {
    return copy(from,std::as_writable_bytes(std::span{&value,std::size_t{1}}));
}
bool add(std::uintptr_t base,std::uint64_t offset,std::uintptr_t& out) noexcept {
    if(base<0x10000 || offset>UINTPTR_MAX-base)return false;out=base+offset;return true;
}
bool asset(std::uint32_t tag,std::uintptr_t& out) noexcept {
    std::uintptr_t directory{},tables{},at{};std::array<std::byte,0x38> descriptor{};
    const auto shifted=static_cast<std::uint32_t>(static_cast<std::int32_t>(tag)>>13);
    const auto bank=((std::uint64_t{shifted}|0xFFC0000ULL)>>18)&(shifted&0xFFFFU);
    if(!read(image+0x2439C70,directory) || !read(directory,tables) || !add(tables,bank*64,at)
        || !copy(at,descriptor))return false;
    const auto stride=identity::field<std::int32_t>(descriptor,0x30);
    if(stride<=0 || stride>0x100000 || !add(identity::field<std::uintptr_t>(descriptor,8),
        std::uint64_t{tag&0x1FFFU}*static_cast<unsigned>(stride),out))return false;
    std::uint64_t correction{};
    if(!read(out+8,correction))return false;
    out=omega_enemy_native_reference::corrected_base(out,correction,identity::field<std::int32_t>(descriptor,0x34));
    return out>=0x10000 && out<=UINTPTR_MAX-0x1000;
}
bool authored(const points::Binding& binding,std::uint32_t index,std::uintptr_t supplied,
    std::array<std::byte,144>& bytes) noexcept {
    std::uintptr_t list{},header{},expected{};std::int64_t relative{};std::uint64_t count{};
    std::array<std::byte,16> arrayHeader{};
    if(!asset(binding.ticket.list,list) || !read(list+8,count) || count!=binding.ticket.placementCount
        || !read(list+16,relative) || relative<0 || !add(list+16,static_cast<std::uint64_t>(relative),header)
        || !copy(header,arrayHeader) || identity::field<std::uint64_t>(arrayHeader,0)!=count
        || identity::field<std::uint32_t>(arrayHeader,8)!=0x808099D8
        || index>=count || !add(header+16,std::uint64_t{index}*144,expected) || supplied!=expected)return false;
    return copy(supplied,bytes);
}
bool object(std::uint32_t handle,std::array<std::byte,0x98>& bytes,std::uintptr_t* location=nullptr) noexcept {
    std::uintptr_t base{},at{};std::uint32_t stride{};
    if(handle==UINT32_MAX || !read(image+0x1F93428,base) || !read(image+0x1F93430,stride)
        || stride<bytes.size() || stride>0x100000 || !add(base,std::uint64_t{handle&0x1FFFU}*stride,at)
        || !copy(at,bytes))return false;
    if(location)*location=at;return true;
}
bool generation(std::uint32_t handle,native_population_retirement::Slot& out) noexcept {
    std::array<std::byte,0x28> descriptor{};std::uintptr_t at{};
    if(!copy(image+0x1F93420,descriptor))return false;
    out.base=identity::field<std::uintptr_t>(descriptor,8);
    out.stride=identity::field<std::uint32_t>(descriptor,0x20);
    out.generationOffset=identity::field<std::uint32_t>(descriptor,0x1C);
    out.mask=identity::field<std::uint32_t>(descriptor,0x24);
    return native_population_retirement::valid(out) && add(out.base,
        std::uint64_t{handle&0x1FFFU}*out.stride+out.generationOffset,at) && read(at,out.generation);
}
void report(const char* stage,const points::Binding& binding,std::uint32_t index,std::uint64_t guid,
    std::uint32_t handle,unsigned result,unsigned detail=0) noexcept {
    std::array<char,448> line{};const auto& t=binding.ticket;
    const auto n=std::snprintf(line.data(),line.size(),
        "ev=ambient_named_point stage=%s owner=%016llX incarnation=%llu boot=%016llX registry=%08X list=%08X epoch=%llu index=%u guid=%016llX entity=%08X result=%u detail=%u mutation=observe_only",
        stage,t.owner.sessionId,t.owner.incarnation.value,t.boot,t.registry,t.list,binding.epoch,index,guid,handle,result,detail);
    if(n>0 && static_cast<std::size_t>(n)<line.size())core::log::write(core::log::Channel::client,core::log::Level::info,
        {line.data(),static_cast<std::size_t>(n)});
}
__declspec(noinline) void __fastcall remove_hook(std::uint32_t list) noexcept {
    const hooking::CallGate::Scope scope{gate};
    const auto prior=scope.accepts_side_effects()?points::retirement(list):points::Snapshot{};
    std::array<native_population_retirement::Slot,8> before{};std::uint8_t qualified{};
    if(prior.binding.epoch) {
        for(std::size_t i=0;i<prior.binding.ticket.count;++i) {
            std::array<std::byte,0x98> resident{};
            if((prior.constructed&(1U<<i)) && object(prior.handles[i],resident)
                && identity::resident(prior.binding,prior.binding.ticket.points[i],prior.handles[i],resident)
                && generation(prior.handles[i],before[i]))qualified|=static_cast<std::uint8_t>(1U<<i);
        }
        static_cast<void>(points::removing(prior.binding));
    }
    hooking::await_original(removeList)(list);
    if(!prior.binding.epoch || !scope.accepts_side_effects())return;
    std::uint8_t released{};
    for(std::size_t i=0;i<prior.binding.ticket.count;++i) {
        native_population_retirement::Slot after{};
        if((qualified&(1U<<i)) && generation(prior.handles[i],after)
            && native_population_retirement::released(before[i],after))released|=static_cast<std::uint8_t>(1U<<i);
    }
    // A return alone is not retirement. Publish separately the qualified
    // pre-call members and exact native generation advances actually observed.
    report("remove_return",prior.binding,0,0,UINT32_MAX,released,qualified);
}
__declspec(noinline) void __fastcall release_hook(void* heap,std::uint32_t handle) noexcept {
    const hooking::CallGate::Scope scope{gate};points::Snapshot prior{};points::Point point{};
    native_population_retirement::Slot before{};bool qualified{};
    if(scope.accepts_side_effects() && reinterpret_cast<std::uintptr_t>(heap)==image+0x1F93420) {
        std::array<std::byte,0x98> resident{};
        if(object(handle,resident)) {
            prior=points::retirement(identity::field<std::uint32_t>(resident,0x88),handle);
            for(std::size_t i=0;i<prior.binding.ticket.count;++i)
                if((prior.observed&(1U<<i)) && !(prior.retired&(1U<<i)) && prior.handles[i]==handle
                    && identity::resident(prior.binding,prior.binding.ticket.points[i],handle,resident)) {
                    point=prior.binding.ticket.points[i];qualified=generation(handle,before);break;
                }
        }
    }
    hooking::await_original(releaseSlot)(heap,handle);
    if(!qualified || !scope.accepts_side_effects())return;
    native_population_retirement::Slot after{};
    const bool released=generation(handle,after) && native_population_retirement::released(before,after);
    const bool accepted=released && points::actor_retired(prior.binding,point,handle);
    report("pool_release",prior.binding,point.index,point.guid,handle,released?1U:0U,accepted?1U:0U);
}
__declspec(noinline) std::uint8_t __fastcall interface_hook(const void* entity,void* output) noexcept {
    const hooking::CallGate::Scope scope{gate};std::array<std::byte,0x98> before{},after{};
    points::Snapshot prior{};
    if(scope.accepts_side_effects() && copy(reinterpret_cast<std::uintptr_t>(entity),before))
        prior=points::lookup(identity::field<std::uint32_t>(before,0x88));
    const auto result=hooking::await_original(pointInterface)(entity,output);
    if(result && prior.binding.epoch && scope.accepts_side_effects()
        && copy(reinterpret_cast<std::uintptr_t>(entity),after)) {
        const auto guid=identity::field<std::uint64_t>(before,0x90);
        for(std::size_t i=0;i<prior.binding.ticket.count;++i) {
            std::array<std::byte,0x98> resident{};std::uintptr_t address{};
            if(prior.binding.ticket.points[i].guid==guid && (prior.constructed&(1U<<i))
                && identity::resident(prior.binding,prior.binding.ticket.points[i],prior.handles[i],before)
                && identity::resident(prior.binding,prior.binding.ticket.points[i],prior.handles[i],after)
                && object(prior.handles[i],resident,&address) && address==reinterpret_cast<std::uintptr_t>(entity)
                && identity::resident(prior.binding,prior.binding.ticket.points[i],prior.handles[i],resident)
                && points::interface_resolved(prior.binding,guid))
                report("interface_80807F35",prior.binding,prior.binding.ticket.points[i].index,guid,prior.handles[i],1);
        }
    }
    return result;
}
bool idle() noexcept {return gate.idle();}
void* target(std::uintptr_t rva,const std::array<std::uint8_t,16>& expected) noexcept {
    std::array<std::byte,16> actual{};
    return copy(image+rva,actual) && std::memcmp(actual.data(),expected.data(),16)==0?reinterpret_cast<void*>(image+rva):nullptr;
}
}
AmbientNamedConstruction capture_ambient_named_construction(const void* placement,
    std::uint32_t list,std::uint32_t index) noexcept {
    const hooking::CallGate::Scope scope{gate};AmbientNamedConstruction captured{};
    if(!scope.accepts_side_effects())return {};
    captured.binding=points::lookup(list).binding;
    if(!captured.binding.epoch)return {};
    bool required{};
    for(std::size_t i=0;i<captured.binding.ticket.count;++i)
        if(captured.binding.ticket.points[i].index==index)required=true;
    if(!required)return {};
    if(!authored(captured.binding,index,reinterpret_cast<std::uintptr_t>(placement),captured.placement)
        || !identity::placement(captured.binding,index,captured.placement,captured.point)) {
        report("construction_identity",captured.binding,index,0,UINT32_MAX,0);return {};
    }
    return captured;
}
void complete_ambient_named_construction(const AmbientNamedConstruction& captured,
    const void* placement,const void* output,const void* returned) noexcept {
    const hooking::CallGate::Scope scope{gate};
    if(!captured.binding.epoch || !scope.accepts_side_effects())return;
    const auto& binding=captured.binding;const auto& point=captured.point;
    std::uint32_t handle=UINT32_MAX;std::array<std::byte,0x98> resident{};std::array<std::byte,144> after{};
    const bool created=returned==output && read(reinterpret_cast<std::uintptr_t>(output),handle)
        && authored(binding,point.index,reinterpret_cast<std::uintptr_t>(placement),after) && captured.placement==after
        && object(handle,resident) && identity::resident(binding,point,handle,resident);
    const bool accepted=created && points::constructed(binding,point,handle);
    report("created",binding,point.index,point.guid,handle,accepted?1U:0U,created?1U:0U);
}
namespace {
bool invoke_point_interface(Interface function,const void* entity,void* output) noexcept {
    __try {return function(entity,output)!=0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
}
__declspec(noinline) bool read_native_point_interface(const void* entity,std::span<std::byte,48> output) noexcept {
    const hooking::CallGate::Scope scope{gate};
    std::fill(output.begin(),output.end(),std::byte{});
    if(!entity || !scope.accepts_side_effects())return false;
    const auto original=pointInterface.load(std::memory_order_acquire);
    return original && invoke_point_interface(original,entity,output.data());
}
bool install_ambient_population_named_observer() noexcept {
    if(handles[0].attached)return gate.accepting();image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    // Hijacked's placement owner supplies the shared 575690 constructor callback.
    const std::array<hooking::detour::Spec,3> specs{{
        {target(native_hook_ownership::kAmbientNamedPoints[0],{0x48,0x89,0x5C,0x24,0x18,0x55,0x57,0x41,0x56,0x48,0x8B,0xEC,0x48,0x83,0xEC,0x40}),reinterpret_cast<void*>(&remove_hook)},
        {target(native_hook_ownership::kAmbientNamedPoints[1],{0x4C,0x8B,0xDC,0x49,0x89,0x5B,0x08,0x57,0x48,0x83,0xEC,0x50,0x33,0xC0,0x4D,0x8D}),reinterpret_cast<void*>(&interface_hook)},
        {target(native_hook_ownership::kAmbientNamedPoints[2],{0x48,0x89,0x6C,0x24,0x18,0x56,0x41,0x56,0x41,0x57,0x48,0x83,0xEC,0x20,0x0F,0xB7}),reinterpret_cast<void*>(&release_hook)},
    }};
    if(!image || !specs[0].target || !specs[1].target || !specs[2].target
        || !hooking::detour::install(specs,handles))return false;
    hooking::publish_original(removeList,reinterpret_cast<Remove>(handles[0].original));
    hooking::publish_original(pointInterface,reinterpret_cast<Interface>(handles[1].original));
    hooking::publish_original(releaseSlot,reinterpret_cast<Release>(handles[2].original));gate.accept();
    core::log::write(core::log::Channel::client,core::log::Level::info,
        "ev=ambient_named_point stage=install result=ok construct=shared_575690 remove=569D10 interface=4E25D0 release=34F790 mutation=observe_only");return true;
}
void quiesce_ambient_population_named_observer() noexcept {gate.quiesce();}
bool uninstall_ambient_population_named_observer() noexcept {
    gate.quiesce();if(!handles[0].attached)return true;
    const std::array<hooking::detour::ProtectedCodeEntry,9> protectedCode{{
        {reinterpret_cast<void*>(&read_native_point_interface)},
        {reinterpret_cast<void*>(&invoke_point_interface)},
        {reinterpret_cast<void*>(&release_hook)},
        {reinterpret_cast<void*>(&capture_ambient_named_construction)},
        {reinterpret_cast<void*>(&complete_ambient_named_construction)},{reinterpret_cast<void*>(&remove_hook)},
        {reinterpret_cast<void*>(&interface_hook)},{reinterpret_cast<void*>(&hooking::call_gate_detail::enter)},
        {reinterpret_cast<void*>(&hooking::call_gate_detail::leave)},
    }};
    if(hooking::detour::uninstall(handles,protectedCode,idle)!=hooking::detour::UninstallResult::removed)return false;
    removeList.store(nullptr,std::memory_order_release);
    pointInterface.store(nullptr,std::memory_order_release);releaseSlot.store(nullptr,std::memory_order_release);
    image=0;return true;
}
}
