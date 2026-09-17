#include "public_event_placement_observer.h"
#include <Windows.h>
#include <atomic>
#include "omega_enemy_native_reference.h"

namespace dawn::client::hooks::bootflow::public_event_placement_observer {
namespace {
std::atomic_uint64_t sequence{};
bool copy(std::uintptr_t from,std::span<std::byte> to) noexcept {
    SIZE_T copied{};
    return from>=0x10000 && to.size()<=8192 && from<=UINTPTR_MAX-to.size()
        && ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(from),to.data(),to.size(),&copied)
        && copied==to.size();
}
template<class T> bool read(std::uintptr_t from,T& to) noexcept {
    return copy(from,std::as_writable_bytes(std::span{&to,std::size_t{1}}));
}
std::uintptr_t image() noexcept {return reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));}
bool helpers() noexcept {
    // Fixed pinned-client helper entry prefixes; all three are read-only.
    static const bool checked=[]() noexcept {
        struct Row {std::uintptr_t rva;std::array<std::byte,16> prefix;};
        const std::array<Row,3> expected{{
            {0x352310,{std::byte{0x48},std::byte{0x83},std::byte{0xEC},std::byte{0x08},std::byte{0x44},std::byte{0x8B},std::byte{0x51},std::byte{0x04},std::byte{0x4C},std::byte{0x8B},std::byte{0xCA},std::byte{0xC7},std::byte{0x02},std::byte{0xFF},std::byte{0xFF},std::byte{0xFF}}},
            {0x4E5C60,{std::byte{0x0F},std::byte{0xB7},std::byte{0x41},std::byte{0x20},std::byte{0x25},std::byte{0xFF},std::byte{0x1F},std::byte{0x00},std::byte{0x00},std::byte{0x0F},std::byte{0xAF},std::byte{0x05},std::byte{0xA0},std::byte{0xC4},std::byte{0xAA},std::byte{0x01}}},
            {0x9FEC30,{std::byte{0x40},std::byte{0x53},std::byte{0x48},std::byte{0x83},std::byte{0xEC},std::byte{0x20},std::byte{0x48},std::byte{0x8B},std::byte{0xD9},std::byte{0x0F},std::byte{0xB7},std::byte{0x49},std::byte{0x48},std::byte{0xE8},std::byte{0xFE},std::byte{0x50}}},
        }};
        for(const auto& row:expected) {std::array<std::byte,16> actual{};
            if(!copy(image()+row.rva,actual) || actual!=row.prefix)return false;}
        return true;
    }();
    return checked;
}
bool add(std::uintptr_t from,std::uint64_t amount,std::uintptr_t& to) noexcept {
    if(from<0x10000 || amount>UINTPTR_MAX-from)return false;to=from+static_cast<std::uintptr_t>(amount);return true;
}
// Same native directory/relative correction as the proven population adapter.
// Full source identity is checked separately; a matching13-bit index is not enough.
bool resolve(std::uint32_t handle,std::int64_t offset,std::uintptr_t& out) noexcept {
    if(handle==UINT32_MAX || offset<0 || offset>0x2000000)return false;
    std::uintptr_t directory{},tables{},address{};
    if(!read(image()+0x2439C70,directory) || !read(directory,tables))return false;
    const auto shifted=static_cast<std::uint32_t>(static_cast<std::int32_t>(handle)>>13);
    const auto bucket=((std::uint64_t{shifted}|0xFFC0000ULL)>>18)&(shifted&0xFFFFU);
    std::array<std::byte,0x38> table{};
    if(!add(tables,bucket*0x40,address) || !copy(address,table))return false;
    const auto stride=feedback::field<std::int32_t>(table,0x30);
    if(stride<=0 || stride>0x100000 || !add(feedback::field<std::uintptr_t>(table,8),
        std::uint64_t{handle&0x1FFFU}*static_cast<unsigned>(stride),address))return false;
    std::uint64_t correction{};if(address>UINTPTR_MAX-8 || !read(address+8,correction))return false;
    return add(static_cast<std::uintptr_t>(omega_enemy_native_reference::corrected_base(address,correction,
        feedback::field<std::int32_t>(table,0x34))),static_cast<std::uint64_t>(offset),out);
}
bool current_source(std::uintptr_t component,feedback::SourceReference ref) noexcept {
    std::uintptr_t resolved{};std::uint32_t current=UINT32_MAX;
    if(!resolve(ref.member,ref.offset,resolved) || resolved!=component)return false;
    __try {reinterpret_cast<std::uint32_t*(__fastcall*)(const void*,std::uint32_t*) noexcept>(image()+0x4E5C60)(
        reinterpret_cast<const void*>(component),&current);}
    __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
    return current==ref.member;
}
std::uintptr_t authority_body(std::uintptr_t object) noexcept {
    __try {return reinterpret_cast<std::uintptr_t(__fastcall*)(std::uintptr_t) noexcept>(image()+0x9FEC30)(object);}
    __except(EXCEPTION_EXECUTE_HANDLER) {return 0;}
}
bool entity(std::span<const std::byte> component,std::uint32_t& handle,std::uint32_t& flags) noexcept {
    const auto weak=feedback::field<std::uint64_t>(component,0x440);handle=UINT32_MAX;
    __try {reinterpret_cast<std::uint32_t*(__fastcall*)(const void*,std::uint32_t*) noexcept>(image()+0x352310)(&weak,&handle);}
    __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
    if(handle==UINT32_MAX || handle!=feedback::field<std::uint32_t>(component,0x444))return false;
    std::uintptr_t table{},address{};std::int32_t stride{};
    return read(image()+0x1F93428,table) && read(image()+0x1F93430,stride) && stride>=8 && stride<0x100000
        && add(table,std::uint64_t{handle&0x1FFFU}*static_cast<unsigned>(stride)+4,address) && read(address,flags);
}
}
Context begin(void* pointer) noexcept {
    Context result{};const auto component=reinterpret_cast<std::uintptr_t>(pointer);
    std::array<std::byte,16> header{};if(!copy(component,header) || feedback::field<std::uint32_t>(header,4)!=0x80809928)return {};
    const auto binding=bridge::lookup_definition(feedback::field<std::uint32_t>(header,0));
    if(!binding.epoch || !helpers() || feedback::field<std::int64_t>(header,8)!=binding.ticket.definition.nativeDefinitionOffset)return {};
    // Native common definition+38 contains the authored bubble scope. Resolve
    // the actual definition rather than treating authority+68 as an endpoint.
    std::uintptr_t definition{};std::array<std::byte,0x98> definitionBytes{};
    if(!resolve(binding.ticket.asset.definition,binding.ticket.definition.nativeDefinitionOffset,definition)
        || !copy(definition,definitionBytes)
        || feedback::field<std::uint32_t>(definitionBytes,0x30)!=binding.ticket.asset.registry
        || feedback::field<std::uint8_t>(definitionBytes,0x34)!=4
        || feedback::field<std::uint16_t>(definitionBytes,0x36)!=binding.ticket.asset.slot
        || feedback::field<std::uint32_t>(definitionBytes,0x38)!=binding.ticket.bubble
        || feedback::field<std::uint8_t>(definitionBytes,0x94)!=0)return {};
    if(!copy(component,result.priorComponent))return {};
    const auto& bytes=result.priorComponent;
    result.source={feedback::field<std::uint32_t>(bytes,0x160),feedback::field<std::int64_t>(bytes,0x168)};
    if(feedback::field<std::uint32_t>(bytes,0x164)!=0x80809927 || !current_source(component,result.source))return {};
    std::uintptr_t object{};result.authorityHandle=feedback::field<std::uint32_t>(bytes,0x170);
    if(!resolve(result.authorityHandle,0,object) || !copy(object,result.authorityObject))return {};
    const auto& a=result.authorityObject;
    if(feedback::field<std::uint32_t>(a,0)!=binding.ticket.asset.registry || feedback::field<std::uint8_t>(a,4)!=4
        || feedback::field<std::uint16_t>(a,6)!=binding.ticket.asset.slot || feedback::field<std::uint32_t>(a,0xC)!=0x8080992F
        || feedback::field<std::uint32_t>(a,0x68)!=binding.ticket.authorityOwner || feedback::field<std::uint8_t>(a,0x18)!=0
        || !copy(authority_body(object),result.authorityBody))return {};
    if(feedback::field<std::uint32_t>(bytes,0x444)!=UINT32_MAX
        && !entity(bytes,result.priorEntity,result.priorEntityFlags))return {};
    result.binding=binding;result.component=component;return result;
}
bool finish(void* pointer,const Context& before) noexcept {
    const auto component=reinterpret_cast<std::uintptr_t>(pointer);
    if(!before.binding.epoch || component!=before.component || !current_source(component,before.source))return false;
    std::array<std::byte,feedback::kComponentBytes> c{},fresh{};
    std::array<std::byte,0x70> objectBytes{};std::array<std::byte,feedback::kAuthorityBytes> body{};
    std::uintptr_t object{};
    if(!copy(component,c) || feedback::field<std::uint32_t>(c,0x170)!=before.authorityHandle
        || !resolve(before.authorityHandle,0,object) || !copy(object,objectBytes) || objectBytes!=before.authorityObject
        || !copy(authority_body(object),body) || body!=before.authorityBody)return false;
    std::uint32_t handle{},flags{};
    if(!entity(c,handle,flags) || !current_source(component,before.source) || !copy(component,fresh) || c!=fresh)return false;
    const auto serial=sequence.fetch_add(1,std::memory_order_relaxed)+1;if(!serial)return false;
    feedback::Capture capture{before.binding.ticket,c,objectBytes,body,std::span<const std::byte>(c).subspan(0x2F0,feedback::kSenseBytes),
        before.source,handle,flags,feedback::kProducerRva,serial,before.priorComponent,before.priorEntity,before.priorEntityFlags};
    feedback::Observation observation{};
    return feedback::qualify(before.binding.ticket,capture,observation) && bridge::submit({before.binding,observation});
}
} // namespace dawn::client::hooks::bootflow::public_event_placement_observer
