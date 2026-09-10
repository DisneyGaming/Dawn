#include "adventure_dialogue_observer.h"
#include "adventure_cue_native_identity.h"
#include "omega_enemy_native_reference.h"
#include "../../../core/logging/log.h"
#include <Windows.h>
#include <atomic>
#include <cstdio>
namespace sunrise::client::hooks::bootflow::adventure_dialogue_observer {
namespace {
std::atomic_uint64_t sequence{},lastRejected{},lastReported{};
template<class T> T field(std::span<const std::byte> b,std::size_t at) noexcept {
    T v{};if(at<=b.size() && sizeof(v)<=b.size()-at)std::memcpy(&v,b.data()+at,sizeof(v));return v;
}
bool copy(std::uintptr_t address,std::span<std::byte> out) noexcept {
    SIZE_T n{};return address>=0x10000 && out.size()<=8192 && address<=UINTPTR_MAX-out.size()
        && ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),out.data(),out.size(),&n) && n==out.size();
}
template<class T> bool read(std::uintptr_t at,T& out) noexcept {return copy(at,std::as_writable_bytes(std::span{&out,std::size_t{1}}));}
std::uintptr_t image() noexcept {return reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));}
bool add(std::uintptr_t base,std::uint64_t n,std::uintptr_t& out) noexcept {
    if(base<0x10000 || n>UINTPTR_MAX-base)return false;out=base+static_cast<std::uintptr_t>(n);return true;
}
bool resolve(std::uint32_t handle,std::int64_t offset,std::uintptr_t& out) noexcept {
    if(handle==UINT32_MAX || offset<0 || offset>=0x2000000)return false;
    std::uintptr_t dir{},tables{},at{};std::array<std::byte,0x38> row{};
    if(!read(image()+0x2439C70,dir) || !read(dir,tables))return false;
    const auto shifted=static_cast<std::uint32_t>(static_cast<std::int32_t>(handle)>>13);
    const auto bucket=((std::uint64_t{shifted}|0xFFC0000ULL)>>18)&(shifted&0xFFFFU);
    if(!add(tables,bucket*0x40,at) || !copy(at,row))return false;
    const auto stride=field<std::int32_t>(row,0x30);
    if(stride<=0 || stride>0x100000 || !add(field<std::uintptr_t>(row,8),std::uint64_t{handle&0x1FFFU}*static_cast<unsigned>(stride),at))return false;
    std::uint64_t correction{};if(at>UINTPTR_MAX-8 || !read(at+8,correction))return false;
    return add(static_cast<std::uintptr_t>(omega_enemy_native_reference::corrected_base(at,correction,field<std::int32_t>(row,0x34))),static_cast<std::uint64_t>(offset),out);
}
bool self(std::uintptr_t component,adventure_cue_native_identity::Identity& out) noexcept {
    constexpr std::array<std::byte,16> expected{std::byte{0x0F},std::byte{0xB7},std::byte{0x41},std::byte{0x20},std::byte{0x4C},std::byte{0x8B},std::byte{0xD2},std::byte{0x25},std::byte{0xFF},std::byte{0x1F},std::byte{0},std::byte{0},std::byte{0x4C},std::byte{0x8B},std::byte{0xC9},std::byte{0x0F}};
    static const bool helper=[&] {std::array<std::byte,16> actual{};return copy(image()+0x4E5640,actual) && actual==expected;}();
    std::uintptr_t pool{},classPointer{};std::uint32_t stride{},sourceClass{};
    return helper && read(image()+0x1F92108,pool) && read(image()+0x1F92110,stride)
        && read(image()+0x1FA0D38,classPointer) && read(classPointer,sourceClass) && sourceClass==0x80809A3B
        && adventure_cue_native_identity::capture(component,pool,stride,copy,resolve,out);
}
bool stable(const Context& c) noexcept {
    std::array<std::byte,16> header{};std::array<std::byte,0x60> definition{};std::array<std::byte,0x70> authority{};
    std::uintptr_t at{};std::uint32_t handle{};adventure_cue_native_identity::Identity source{};
    return copy(c.component,header) && header==c.header && self(c.component,source)
        && source.source.member==c.source.member && source.source.offset==c.source.offset && source.componentLink==c.componentLink
        && read(c.component+0x170,handle) && handle==c.authority && resolve(handle,0,at) && copy(at,authority) && authority==c.authorityObject
        && resolve(c.binding.ticket.authored.definition,0x1408,at) && copy(at,definition) && definition==c.definition;
}
void log(const Context& c,const char* stage,std::uint32_t after=0) noexcept {
    const auto& t=c.binding.ticket;std::array<char,640> line{};
    const auto n=std::snprintf(line.data(),line.size(),
        "ev=adventure_dialogue stage=%s owner=%016llX incarnation=%llu boot=%016llX target=%d selection_revision=%llu epoch=%llu definition=%08X bank=%08X row=%u selector=%08X generation=%u processed_before=%u processed_after=%u source=%08X source_offset=%lld native=100A180 completion=submission_only mutation=observe_only",
        stage,t.owner.sessionId,t.owner.incarnation.value,t.boot,t.activity,t.selectionRevision,c.binding.epoch,t.authored.definition,t.authored.bank,
        t.authored.row,t.authored.selector,t.request.generations[t.authored.row],c.processed,after,c.source.member,c.source.offset);
    if(n>0 && static_cast<std::size_t>(n)<line.size())core::log::write(core::log::Channel::client,core::log::Level::info,{line.data(),static_cast<std::size_t>(n)});
}
}
Context begin(void* pointer) noexcept {
    Context c{};c.component=reinterpret_cast<std::uintptr_t>(pointer);
    if(!copy(c.component,c.header))return {};
    c.binding=bridge::lookup(field<std::uint32_t>(c.header,0));if(!c.binding.epoch)return {};
    const auto fail=[&](const char* stage) {if(lastRejected.exchange(c.binding.epoch)!=c.binding.epoch)log(c,stage);return Context{};};
    const auto& t=c.binding.ticket;std::uintptr_t at{};
    if(field<std::uint32_t>(c.header,4)!=0x80804F4C || field<std::int64_t>(c.header,8)!=0x1408
        || !resolve(t.authored.definition,0x1408,at) || !copy(at,c.definition))return fail("definition_read");
    const auto& d=c.definition;
    if(field<std::uint32_t>(d,0x30)!=t.request.registry || field<std::uint8_t>(d,0x34)!=53
        || field<std::uint16_t>(d,0x36)!=t.request.slot || field<std::uint32_t>(d,0x38)!=UINT32_MAX
        || field<std::uint32_t>(d,0x48)!=0x80804F77 || field<std::uint32_t>(d,0x58)!=t.authored.bank)return fail("authored_definition");
    adventure_cue_native_identity::Identity source{};
    if(!self(c.component,source))return fail("common_component_self");
    c.source={source.source.member,source.source.offset};c.componentLink=source.componentLink;
    if(!read(c.component+0x170,c.authority) || !resolve(c.authority,0,at) || !copy(at,c.authorityObject))return fail("authority_read");
    const auto& a=c.authorityObject;
    if(field<std::uint32_t>(a,0)!=t.request.registry || field<std::uint8_t>(a,4)!=53 || field<std::uint16_t>(a,6)!=t.request.slot
        || field<std::uint32_t>(a,0xC)!=0x80804F77 || field<std::uint32_t>(a,0x68)!=UINT32_MAX || field<std::uint8_t>(a,0x18)!=0)return fail("authority_identity");
    std::array<std::byte,0x20> bank{};std::uint32_t selector{};std::uintptr_t bankAddress{},row{};
    if(!resolve(t.authored.bank,0,bankAddress) || !copy(bankAddress,bank) || field<std::uint64_t>(bank,8)!=t.authored.bankRows
        || field<std::int64_t>(bank,0x10)<=0 || !add(bankAddress,0x20+static_cast<std::uint64_t>(field<std::int64_t>(bank,0x10))+t.authored.row*8ULL,row)
        || !read(row,selector) || selector!=t.authored.selector)return fail("bank_row");
    if(!copy(c.component+0x180,c.applied) || !feedback::wire::matches_fields(c.applied,t.request)
        || !read(c.component+0x1188+t.authored.row*4ULL,c.processed))return fail("applied_body");
    if(c.processed==t.request.generations[t.authored.row])return {};
    if(!stable(c))return fail("source_generation");
    c.sequence=sequence.fetch_add(1)+1;return c;
}
bool finish(void* pointer,const Context& c,bool forwarded) noexcept {
    if(!c.binding.epoch || reinterpret_cast<std::uintptr_t>(pointer)!=c.component)return false;
    feedback::wire::Decoded after{};std::uint32_t processed{};
    const bool identity=stable(c);
    if(!identity || !copy(c.component+0x180,after) || !read(c.component+0x1188+c.binding.ticket.authored.row*4ULL,processed))return false;
    feedback::Observation observation{};
    const feedback::Capture capture{c.binding.ticket,c.source,c.sequence,c.applied,after,c.processed,processed,forwarded,identity};
    if(feedback::qualify(c.binding.ticket,capture,observation)!=feedback::Result::accepted || !bridge::submit({c.binding,observation}))return false;
    if(lastReported.exchange(c.binding.epoch)!=c.binding.epoch)log(c,"native_submitted",processed);return true;
}
}
