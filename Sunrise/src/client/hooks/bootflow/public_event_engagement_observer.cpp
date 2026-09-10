#include "public_event_engagement_observer.h"
#include "omega_enemy_native_reference.h"
#include "../../../core/logging/log.h"
#include <Windows.h>
#include <atomic>
#include <cstdio>

namespace sunrise::client::hooks::bootflow::public_event_engagement_observer {
namespace {
namespace capture=public_event_engagement_capture;
namespace bridge=capture::bridge;
namespace feedback=capture::feedback;
std::atomic_uint64_t sequence{},lastRejected{},lastReported{};
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
void rejected(const bridge::Binding& binding,capture::Result result) noexcept {
    if(!binding.epoch || lastRejected.exchange(binding.epoch,std::memory_order_acq_rel)==binding.epoch)return;
    const auto& ticket=binding.ticket;std::array<char,384> line{};
    const auto n=std::snprintf(line.data(),line.size(),
        "ev=public_event_engagement stage=qualification result=%u owner=%016llX incarnation=%llu event=%llu definition=%08X epoch=%llu mutation=observe_only",
        static_cast<unsigned>(result),ticket.owner.sessionId,ticket.owner.incarnation.value,ticket.event,ticket.definition,binding.epoch);
    if(n>0 && static_cast<std::size_t>(n)<line.size())
        core::log::write(core::log::Channel::client,core::log::Level::info,{line.data(),static_cast<std::size_t>(n)});
}
}
Context begin(void* pointer,const void* packet) noexcept {
    const auto component=reinterpret_cast<std::uintptr_t>(pointer);
    std::array<std::byte,16> header{};
    if(!copy(component,header))return {};
    const auto binding=bridge::lookup(feedback::field<std::uint32_t>(header,0));
    if(!binding.epoch)return {};
    Context context{};
    const auto result=capture::begin(binding,component,reinterpret_cast<std::uintptr_t>(packet),copy,resolve,source,context);
    if(result!=capture::Result::accepted)rejected(binding,result);
    return context;
}
bool finish(void* pointer,const Context& before) noexcept {
    if(!before.binding.epoch)return false;
    const auto serial=sequence.fetch_add(1,std::memory_order_relaxed)+1;
    feedback::Observation observation{};
    const bool qualified=capture::finish(before,reinterpret_cast<std::uintptr_t>(pointer),serial,copy,resolve,source,observation);
    const bool accepted=qualified && bridge::submit({before.binding,observation});
    if(lastReported.exchange(before.binding.epoch,std::memory_order_acq_rel)!=before.binding.epoch || accepted) {
        const auto& ticket=before.binding.ticket;std::array<char,640> line{};
        const auto n=std::snprintf(line.data(),line.size(),
            "ev=public_event_engagement stage=native_apply owner=%016llX incarnation=%llu boot=%016llX selection_revision=%llu event=%llu registry=%08X slot=%u definition=%08X scope=%u generation=%d source=%08X source_offset=%lld sequence=%llu qualified=%u accepted=%u full_body_bytes=208 mutation=observe_only",
            ticket.owner.sessionId,ticket.owner.incarnation.value,ticket.boot,ticket.selectionRevision,ticket.event,
            ticket.request.registry,ticket.request.slot,ticket.definition,ticket.request.scope,ticket.request.generation,
            before.self.source.member,before.self.source.offset,serial,qualified?1U:0U,accepted?1U:0U);
        if(n>0 && static_cast<std::size_t>(n)<line.size())
            core::log::write(core::log::Channel::client,core::log::Level::info,{line.data(),static_cast<std::size_t>(n)});
    }
    return accepted;
}
}
