#include "adventure_cue_observer.h"
#include "adventure_cue_native_identity.h"
#include "omega_enemy_native_reference.h"
#include "../../../server/runtime/activity/public_event_cue_pending.h"
#include "../../../core/logging/log.h"
#include <Windows.h>
#include <atomic>
#include <cstdio>
#include <mutex>

namespace sunrise::client::hooks::bootflow::adventure_cue_observer {
namespace {
std::atomic_uint64_t sequence{},lastReported{},lastRejected{},lastBody{};
namespace pending=server::runtime::activity::public_event_cue_pending;
struct PendingRow {std::uint32_t definition{};pending::Pending receipt{};};
std::mutex pendingMutex;std::array<PendingRow,16> pendingRows{};
pending::Identity lease(const Context& c) noexcept {
    return {c.binding,c.source,c.component,c.authority,c.componentLink,c.header,c.definition,c.authorityObject};
}
std::uint64_t pending_index(const Context& c) noexcept {
    std::lock_guard lock(pendingMutex);
    for(auto& row:pendingRows)if(row.definition==c.binding.ticket.definition) {
        const auto index=row.receipt.index(lease(c));
        if(index==UINT64_MAX)row={};
        return index;
    }
    return UINT64_MAX;
}
bool pending_finish(const Context& c,const feedback::Capture& capture,std::uint8_t nativePending,
    unsigned matches,feedback::Observation& observation,bool& remembered) noexcept {
    std::lock_guard lock(pendingMutex);remembered=false;
    PendingRow* row{};
    for(auto& candidate:pendingRows)if(candidate.definition==c.binding.ticket.definition){row=&candidate;break;}
    if(row && row->receipt.observe(lease(c),capture,nativePending,matches,observation)) {*row={};return true;}
    if(row && row->receipt.active())return false;
    if(!row)for(auto& candidate:pendingRows)if(!candidate.definition){row=&candidate;break;}
    if(row) {
        remembered=row->receipt.remember(lease(c),capture,nativePending,c.matchingEntries,matches);
        row->definition=remembered?c.binding.ticket.definition:0;
    }
    return false;
}
void manager_snapshot(const Context& c,std::uint64_t serial,std::uint64_t index,
    std::span<const std::byte> entry) noexcept {
    constexpr char digits[]="0123456789ABCDEF";
    for(std::size_t offset=0;offset<entry.size();offset+=128) {
        std::array<char,640> line{};const auto amount=(std::min)(std::size_t{128},entry.size()-offset);
        const auto n=std::snprintf(line.data(),line.size(),
            "ev=adventure_cue stage=manager_entry owner=%016llX incarnation=%llu boot=%016llX epoch=%llu sequence=%llu index=%llu offset=%zu captured=%zu body=",
            c.binding.ticket.owner.sessionId,c.binding.ticket.owner.incarnation.value,c.binding.ticket.boot,
            c.binding.epoch,serial,index,offset,amount);
        if(n<=0 || static_cast<std::size_t>(n)+amount*2>=line.size())continue;
        auto used=static_cast<std::size_t>(n);
        for(std::size_t i=offset;i<offset+amount;++i) {
            const auto byte=std::to_integer<unsigned>(entry[i]);line[used++]=digits[byte>>4];line[used++]=digits[byte&15];
        }
        core::log::write(core::log::Channel::client,core::log::Level::info,{line.data(),used});
    }
}
void rejected(const bridge::Binding& binding,const char* stage) noexcept {
    if(!binding.epoch || lastRejected.exchange(binding.epoch,std::memory_order_acq_rel)==binding.epoch)return;
    const auto& t=binding.ticket;std::array<char,320> line{};
    const auto n=std::snprintf(line.data(),line.size(),
        "ev=adventure_cue stage=qualification result=%s owner=%016llX incarnation=%llu target=%d selection_revision=%llu definition=%08X epoch=%llu mutation=observe_only",
        stage,t.owner.sessionId,t.owner.incarnation.value,t.activity,t.selectionRevision,t.definition,binding.epoch);
    if(n>0 && static_cast<std::size_t>(n)<line.size())core::log::write(core::log::Channel::client,core::log::Level::info,
        {line.data(),static_cast<std::size_t>(n)});
}
void body(const Context& context) noexcept {
    if(lastBody.exchange(context.binding.epoch,std::memory_order_acq_rel)==context.binding.epoch)return;
    constexpr char digits[]="0123456789ABCDEF";
    std::array<char,640> identityLine{};
    const auto identityLength=std::snprintf(identityLine.data(),identityLine.size(),
        "ev=adventure_cue stage=native_arguments owner=%016llX incarnation=%llu target=%d selection_revision=%llu epoch=%llu producer=1009C00 rcx=%016llX rdx=%016llX payload=%016llX source=%08X source_offset=%lld component_link=%08X authority=%08X packet_header_lo=%016llX packet_header_hi=%016llX mutation=observe_only",
        context.binding.ticket.owner.sessionId,context.binding.ticket.owner.incarnation.value,context.binding.ticket.activity,
        context.binding.ticket.selectionRevision,context.binding.epoch,context.component,context.packet,context.packetBody,
        context.source.member,context.source.offset,context.componentLink,context.authority,
        feedback::field<std::uint64_t>(context.packetHeader,0),feedback::field<std::uint64_t>(context.packetHeader,8));
    if(identityLength>0 && static_cast<std::size_t>(identityLength)<identityLine.size())
        core::log::write(core::log::Channel::client,core::log::Level::info,{identityLine.data(),static_cast<std::size_t>(identityLength)});
    for(std::size_t offset=0;offset<context.incoming.size();offset+=128) {
        std::array<char,544> line{};
        const auto n=std::snprintf(line.data(),line.size(),
            "ev=adventure_cue stage=decoded_authority owner=%016llX incarnation=%llu target=%d selection_revision=%llu epoch=%llu bytes=768 offset=%zu captured=128 body=",
            context.binding.ticket.owner.sessionId,context.binding.ticket.owner.incarnation.value,context.binding.ticket.activity,
            context.binding.ticket.selectionRevision,context.binding.epoch,offset);
        if(n<=0 || static_cast<std::size_t>(n)+256>=line.size())continue;
        auto used=static_cast<std::size_t>(n);
        for(std::size_t i=offset;i<offset+128;++i) {
            const auto byte=std::to_integer<unsigned>(context.incoming[i]);line[used++]=digits[byte>>4];line[used++]=digits[byte&15];
        }
        core::log::write(core::log::Channel::client,core::log::Level::info,{line.data(),used});
    }
}
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
bool helper() noexcept {
    static const bool matches=[]() noexcept {
        constexpr std::array<std::byte,16> expected{
            std::byte{0x0F},std::byte{0xB7},std::byte{0x41},std::byte{0x20},std::byte{0x4C},std::byte{0x8B},
            std::byte{0xD2},std::byte{0x25},std::byte{0xFF},std::byte{0x1F},std::byte{0},std::byte{0},
            std::byte{0x4C},std::byte{0x8B},std::byte{0xC9},std::byte{0x0F}};
        std::array<std::byte,16> actual{};return copy(image()+0x4E5640,actual) && actual==expected;
    }();
    return matches;
}
bool source(std::uintptr_t component,adventure_cue_native_identity::Identity& identity) noexcept {
    std::uintptr_t pool{},classPointer{};std::uint32_t stride{},sourceClass{};
    return helper() && read(image()+0x1F92108,pool) && read(image()+0x1F92110,stride)
        && read(image()+0x1FA0D38,classPointer) && read(classPointer,sourceClass) && sourceClass==0x80809A3B
        && adventure_cue_native_identity::capture(component,pool,stride,copy,resolve,identity);
}
bool manager(std::uint64_t& count,bool& ready) noexcept {
    std::uint8_t initialized{};ready=false;
    if(!read(image()+0x2FB7CC8,initialized) || !read(image()+0x2FB6838+0x1480,count) || count>16)return false;
    ready=initialized!=0;return true;
}
bool matches(const feedback::Ticket& ticket,std::uint64_t count,unsigned& total) noexcept {
    total=0;
    for(std::uint64_t i=0;i<count;++i) {
        std::array<std::byte,8> entry{};
        if(!copy(image()+0x2FB6838+i*0x148,entry))return false;
        // Original137BD50 de-duplicates by manager id alone, even if another
        // registry supplied it. Such a pre-existing entry cannot seed a receipt.
        if(feedback::field<std::uint8_t>(entry,0)==2 && feedback::field<std::uint32_t>(entry,4)
            ==feedback::wire::manager_id(ticket.request.event,ticket.request.variant))++total;
    }
    return true;
}
bool identity(const Context& c) noexcept {
    std::array<std::byte,16> header{};
    std::array<std::byte,0x60> definition{};
    std::array<std::byte,0x70> object{};
    std::uintptr_t address{};std::uint32_t authority{};
    adventure_cue_native_identity::Identity self{};
    return copy(c.component,header) && header==c.header
        && source(c.component,self) && self.source==c.source && self.componentLink==c.componentLink
        && read(c.component+0x170,authority) && authority==c.authority
        && resolve(authority,0,address) && copy(address,object) && object==c.authorityObject
        && resolve(c.binding.ticket.definition,c.binding.ticket.definitionOffset,address)
        && copy(address,definition) && definition==c.definition;
}
}
Context begin(void* pointer,const void* packet) noexcept {
    Context result{};const auto component=reinterpret_cast<std::uintptr_t>(pointer);
    if(!copy(component,result.header))return {};
    const auto binding=bridge::lookup(feedback::field<std::uint32_t>(result.header,0));
    if(!binding.epoch)return {};
    const auto fail=[&](const char* stage) {rejected(binding,stage);return Context{};};
    if(feedback::field<std::uint32_t>(result.header,4)!=0x80804F54
        || feedback::field<std::int64_t>(result.header,8)!=binding.ticket.definitionOffset)return fail("component_header");
    const auto& ticket=binding.ticket;
    std::uintptr_t address{};
    if(!resolve(ticket.definition,ticket.definitionOffset,address) || !copy(address,result.definition))return fail("definition_read");
    const auto& d=result.definition;
    if(feedback::field<std::uint32_t>(d,0x30)!=ticket.request.registry || feedback::field<std::uint8_t>(d,0x34)!=68
        || feedback::field<std::uint16_t>(d,0x36)!=ticket.request.slot
        || feedback::field<std::uint32_t>(d,0x38)!=ticket.nativeScope
        || !feedback::matches_predicate(ticket.presentation,feedback::field<std::uint8_t>(d,0x58))
        || feedback::field<std::uint32_t>(d,0x48)!=0x80804F67
        || feedback::field<std::uint32_t>(d,0x5C)!=ticket.table)return fail("authored_definition");
    adventure_cue_native_identity::Identity self{};
    if(!source(component,self))return fail("common_component_self");
    result.source=self.source;result.componentLink=self.componentLink;
    if(!read(component+0x170,result.authority) || !resolve(result.authority,0,address)
        || !copy(address,result.authorityObject))return fail("authority_read");
    result.packet=reinterpret_cast<std::uintptr_t>(packet);
    if(!copy(result.packet,result.packetHeader))return fail("packet_header_read");
    const auto& a=result.authorityObject;
    if(feedback::field<std::uint32_t>(a,0)!=ticket.request.registry || feedback::field<std::uint8_t>(a,4)!=68
        || feedback::field<std::uint16_t>(a,6)!=ticket.request.slot || feedback::field<std::uint32_t>(a,0xC)!=0x80804F67
        || feedback::field<std::uint32_t>(a,0x68)!=ticket.nativeScope || feedback::field<std::uint8_t>(a,0x18)!=0)return fail("authority_identity");
    // Original 4A6340 returns packet+8. No prefix-only record inference.
    result.packetBody=feedback::field<std::uintptr_t>(result.packetHeader,8);
    if(!copy(result.packetBody,result.incoming) || !feedback::wire::matches_fields(result.incoming,ticket.request)
        || !copy(component+0x180,result.prior) || !manager(result.managerCount,result.managerReady))return fail("full_body_or_manager");
    if(ticket.request.clear) {
        unsigned matches{};
        for(std::uint64_t i=0;i<result.managerCount;++i) {
            std::array<std::byte,0x148> entry{};
            if(!copy(image()+0x2FB6838+i*0x148,entry))return fail("clear_manager_read");
            if(feedback::field<std::uint8_t>(entry,0)==2
                && feedback::field<std::uint32_t>(entry,4)==feedback::wire::manager_id(ticket.request.event,ticket.request.variant)
                && feedback::field<std::uint32_t>(entry,0x10)==ticket.request.registry
                && feedback::field<std::int16_t>(entry,0x124)==ticket.activity) {
                result.entryIndex=i;result.priorEntry=entry;++matches;
            }
        }
        if(matches!=1 || !feedback::presentation_matches(ticket,result.priorEntry))return fail("clear_manager_identity");
    }
    result.binding=binding;result.component=component;
    if(!identity(result))return fail("source_generation");
    if(!ticket.request.clear && ticket.presentation==feedback::Presentation::authoredProgress) {
        if(!matches(ticket,result.managerCount,result.matchingEntries))return fail("pending_manager_read");
        result.entryIndex=pending_index(result);
    }
    body(result);return result;
}
bool finish(void* pointer,const Context& before) noexcept {
    if(!before.binding.epoch || reinterpret_cast<std::uintptr_t>(pointer)!=before.component || !identity(before))return false;
    const auto live=bridge::lookup(before.binding.ticket.definition);
    if(live.epoch!=before.binding.epoch || live.ticket!=before.binding.ticket)return false;
    feedback::wire::Decoded after{},incoming{},fresh{};
    std::array<std::byte,0x148> entry{};
    std::uint64_t count{};bool ready{};
    const auto index=before.entryIndex!=UINT64_MAX?before.entryIndex:before.managerCount;
    if(!copy(before.component+0x180,after) || !copy(before.packetBody,incoming) || incoming!=before.incoming
        || !manager(count,ready) || index>=16
        || !copy(image()+0x2FB6838+index*0x148,entry)
        || !identity(before) || !copy(before.component+0x180,fresh) || after!=fresh)return false;
    const auto serial=sequence.fetch_add(1,std::memory_order_relaxed)+1;
    feedback::Capture capture{before.binding.ticket,before.source,serial,feedback::kProducerRva,
        incoming,before.prior,after,entry,before.managerCount,count,before.managerReady,ready,true,
        before.priorEntry,index};
    feedback::Observation observation{};
    const auto result=feedback::qualify(before.binding.ticket,capture,observation);
    bool deferred{},remembered{};std::uint8_t nativePending{UINT8_MAX};unsigned matchingEntries{};
    if(result!=feedback::Result::accepted && !before.binding.ticket.request.clear
        && before.binding.ticket.presentation==feedback::Presentation::authoredProgress
        && read(before.component+0xB05,nativePending) && matches(before.binding.ticket,count,matchingEntries)) {
        std::array<std::byte,0x148> stableEntry{};std::uint64_t stableCount{};bool stableReady{};
        std::uint8_t stablePending{};
        if(manager(stableCount,stableReady) && stableCount==count && stableReady==ready
            && copy(image()+0x2FB6838+index*0x148,stableEntry) && stableEntry==entry
            && read(before.component+0xB05,stablePending) && stablePending==nativePending && identity(before))
            deferred=pending_finish(before,capture,nativePending,matchingEntries,observation,remembered);
    }
    const bool accepted=(result==feedback::Result::accepted || deferred) && bridge::submit({before.binding,observation});
    if(lastReported.exchange(before.binding.epoch,std::memory_order_acq_rel)!=before.binding.epoch || accepted || remembered) {
        if(before.binding.ticket.presentation==feedback::Presentation::authoredProgress)
            manager_snapshot(before,serial,index,entry);
        const auto& t=before.binding.ticket;std::array<char,640> line{};
        const auto n=std::snprintf(line.data(),line.size(),
            "ev=adventure_cue stage=native_apply owner=%016llX incarnation=%llu boot=%016llX selection_revision=%llu target=%d registry=%08X slot=%u definition=%08X event=%08X source=%08X source_offset=%lld sequence=%llu result=%u accepted=%u full_body_bytes=768 manager_before=%llu manager_after=%llu pending=%u deferred=%u native_pending=%u native_mode=%d manager_index=%llu mutation=observe_only",
            t.owner.sessionId,t.owner.incarnation.value,t.boot,t.selectionRevision,t.activity,t.request.registry,t.request.slot,
            t.definition,t.request.event,before.source.member,before.source.offset,serial,static_cast<unsigned>(result),accepted?1U:0U,
            before.managerCount,count,remembered?1U:0U,deferred?1U:0U,static_cast<unsigned>(nativePending),
            feedback::field<std::int32_t>(entry,0x70),index);
        if(n>0 && static_cast<std::size_t>(n)<line.size())core::log::write(core::log::Channel::client,core::log::Level::info,
            {line.data(),static_cast<std::size_t>(n)});
    }
    return accepted;
}
}
