#pragma once

#include <Windows.h>
#include <algorithm>
#include <array>
#include <cstdio>

#include "omega_enemy_forest_receipts.h"
#include "omega_enemy_native_reference.h"
#include "../../../core/logging/log.h"

namespace dawn::client::hooks::bootflow::omega_enemy_forest {
namespace receipts_detail {
constexpr std::uint64_t kIntervalMs=500;
constexpr std::size_t kReadBudget=128*1024;
constexpr unsigned kPollLineBudget=12,kRunLineBudget=1024,kEncounterBudget=4;
struct Worker final {
    std::uintptr_t address{};
    std::uint32_t self{UINT32_MAX};
    std::uint64_t lastPoll{},summaryHash{},invalidHash{};
    std::array<std::uint64_t,kMaximumEntries> entries{},encounters{};
    std::array<std::uint64_t,128> gates{};
    std::array<std::uint64_t,128> areas{};
    std::array<std::array<std::uint64_t,17>,128> gateParts{};
    std::uint32_t nextEntry{};
};
inline SRWLOCK lock=SRWLOCK_INIT;
inline std::uint64_t run=UINT64_MAX;
inline unsigned lines{};
inline std::array<Worker,4> workers{};

/** Runtime objects move during arena compaction. Preserve receipts by full native
 * self handle; an address is only the location supplied by this tick. */
inline Worker& select_worker(std::uint32_t self) noexcept {
    for(auto& worker:workers) { if(worker.self==self) { return worker; } }
    Worker* selected=&workers.front();
    for(auto& worker:workers) {
        if(worker.self==UINT32_MAX) { selected=&worker;break; }
        if(worker.lastPoll<selected->lastPoll) { selected=&worker; }
    }
    *selected={};selected->self=self;
    return *selected;
}

struct Poll final {
    std::size_t bytes{};
    unsigned emitted{},encounters{};
    bool copy(std::uintptr_t address,std::span<std::byte> target) noexcept {
        if(target.empty()) { return true; }
        if(address<0x10000 || address>UINTPTR_MAX-target.size()
            || target.size()>kReadBudget-bytes) { return false; }
        bytes+=target.size();
        SIZE_T copied{};
        return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),
                                 target.data(),target.size(),&copied) && copied==target.size();
    }
    template<typename T> bool value(std::uintptr_t address,T& item) noexcept {
        return copy(address,std::as_writable_bytes(std::span{&item,std::size_t{1}}));
    }
    template<typename... Args> bool report(const char* format,Args... args) noexcept {
        if(emitted>=kPollLineBudget || lines>=kRunLineBudget) { return false; }
        std::array<char,640> line{};
        const int size=std::snprintf(line.data(),line.size(),format,args...);
        if(size<=0 || static_cast<std::size_t>(size)>=line.size()) { return false; }
        ++emitted;++lines;
        core::log::write(core::log::Channel::client,core::log::Level::info,
                         {line.data(),static_cast<std::size_t>(size)});
        return true;
    }
};

struct Reference final { std::uint32_t handle{UINT32_MAX},type{};std::int64_t offset{}; };
static_assert(sizeof(Reference)==16);
inline constexpr std::array<std::byte,16> kGetterPrefix{
    std::byte{0x48},std::byte{0x89},std::byte{0x5C},std::byte{0x24},std::byte{0x08},
    std::byte{0x48},std::byte{0x89},std::byte{0x6C},std::byte{0x24},std::byte{0x10},
    std::byte{0x48},std::byte{0x89},std::byte{0x74},std::byte{0x24},std::byte{0x18},std::byte{0x57}};

/** Existing-reference getter only; no request, activation, clear or gate setter. */
inline bool encounter_reference(std::uintptr_t image,std::uint32_t handle,Reference& out) noexcept {
    using Getter=std::uint8_t(__fastcall*)(std::uint32_t,Reference*) noexcept;
    __try {
        return reinterpret_cast<Getter>(image+0x4F0290)(handle,&out)!=0;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}

inline bool resolve(Poll& poll,std::uintptr_t image,const Reference& ref,
                    std::uintptr_t& address) noexcept {
    if(ref.handle==UINT32_MAX) { return false; }
    std::uintptr_t directory{},registry{};
    if(!poll.value(image+0x2439C70,directory) || !poll.value(directory,registry)) { return false; }
    const auto shifted=static_cast<std::uint32_t>(static_cast<std::int32_t>(ref.handle)>>13);
    const auto index=((static_cast<std::uint64_t>(shifted)|0xFFC0000ULL)>>18)&(shifted&0xFFFFU);
    std::uintptr_t table{};
    if(!relative(registry,0,static_cast<std::size_t>(index)*0x40,table)) { return false; }
    std::array<std::byte,0x38> descriptor{};
    if(!poll.copy(table,descriptor)) { return false; }
    const auto stride=read<std::int32_t>(descriptor,0x30);
    const auto mask=read<std::int32_t>(descriptor,0x34);
    std::uintptr_t element{};
    if(stride<=0 || stride>0x100000
        || !relative(read<std::uintptr_t>(descriptor,8),0,
                      (ref.handle&0x1FFFU)*static_cast<std::size_t>(stride),element)) { return false; }
    std::uint64_t relocation{};
    std::uintptr_t relocationAddress{};
    if(!relative(element,0,8,relocationAddress)
        || !poll.value(relocationAddress,relocation)) { return false; }
    return relative(omega_enemy_native_reference::corrected_base(element,relocation,mask),
                    ref.offset,0,address);
}

struct Completion final {
    bool valid{};
    std::uint32_t groups{},firstTag{};
    std::int32_t activeQueued{},remaining{};
    std::int64_t required{},taggedRemaining{},taggedQueued{};
};
/** Original FF2CA0 and FF2E20 are read-only accounting getters. Their actor
 * predicate is native FEE750, not a substitute health/death classification. */
inline bool native_counts(std::uintptr_t image,std::uintptr_t encounter,
    std::span<const std::uint32_t> tags,Completion& output) noexcept {
    using Count=std::int32_t(__fastcall*)(std::uintptr_t) noexcept;
    using GroupCount=std::int32_t(__fastcall*)(std::uintptr_t,const std::uint32_t*) noexcept;
    __try {
        output.activeQueued=reinterpret_cast<Count>(image+0xFF2E20)(encounter);
        output.remaining=reinterpret_cast<Count>(image+0xFF2F30)(encounter);
        for(const auto& tag:tags) {
            output.required+=reinterpret_cast<GroupCount>(image+0xFF2CA0)(encounter,&tag);
        }
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
inline Completion completion(Poll& poll,std::uintptr_t image,std::uintptr_t encounter,
    std::span<const std::byte> head,std::span<const std::byte> rows,std::int64_t count) noexcept {
    Completion output{};
    const auto slots=read<std::uint32_t>(head,0x100);
    const auto groups=read<std::int64_t>(head,0xB0);
    if(count<0 || count>kMaximumActorRows || slots>255 || groups<0 || groups>8) { return output; }
    std::uintptr_t prototype{},actorDefinitions{},groupDefinitions{},groupRows{},trackedAddress{};
    std::array<std::byte,0x128> authored{};
    std::array<std::byte,8*0x30> runtimeGroups{};
    std::array<std::byte,8*0x18> authoredGroups{};
    std::array<std::byte,255*12> tracked{};
    if(!resolve(poll,image,read<Reference>(head,0),prototype) || !poll.copy(prototype,authored)
        || read<std::uint32_t>(authored,4)!=0x8080501CU
        || read<std::int64_t>(authored,0x110)!=count || read<std::int64_t>(authored,0x100)!=groups
        || !relative(prototype,read<std::int64_t>(authored,0x118),0x128,actorDefinitions)
        || !relative(prototype,read<std::int64_t>(authored,0x108),0x118,groupDefinitions)
        || !relative(encounter,read<std::int64_t>(head,0xB8),0xC8,groupRows)
        || !relative(encounter,read<std::int64_t>(head,0x108),0x118,trackedAddress)
        || !poll.copy(groupRows,std::span{runtimeGroups}.first(static_cast<std::size_t>(groups)*0x30))
        || !poll.copy(groupDefinitions,std::span{authoredGroups}.first(static_cast<std::size_t>(groups)*0x18))
        || !poll.copy(trackedAddress,std::span{tracked}.first(slots*12))) { return output; }
    // FF2CA0 indexes row definitions from tracked slot row IDs. Reject malformed
    // IDs before invoking native code, even for currently invalid actor slots.
    for(std::size_t i=0;i<slots;++i) {
        const auto row=read<std::int32_t>(tracked,i*12+8);
        if(row!=-1 && (row<0 || row>=count)) { return output; }
    }
    std::array<std::uint32_t,8> tags{};
    for(std::size_t i=0;i<static_cast<std::size_t>(groups);++i) {
        if(read<std::uint8_t>(runtimeGroups,i*0x30+0x20)!=0) {
            tags[output.groups++]=read<std::uint32_t>(authoredGroups,i*0x18+0x10);
        }
    }
    if(output.groups) { output.firstTag=tags[0]; }
    for(std::size_t i=0;i<static_cast<std::size_t>(count);++i) {
        const auto row=rows.subspan(i*kActorRowBytes,kActorRowBytes);
        if(read<std::uint8_t>(row,0x2C)==0) { continue; }
        std::uintptr_t at{};std::uint32_t tag{};
        if(!relative(actorDefinitions,0,i*0xC0+0xA8,at) || !poll.value(at,tag)) { return output; }
        for(std::size_t j=0;j<output.groups;++j) {
            if(tag==tags[j]) {
                output.taggedRemaining+=read<std::int32_t>(row,0x20);
                output.taggedQueued+=read<std::int32_t>(row,0x24);
            }
        }
    }
    constexpr std::array<std::uintptr_t,3> rvas{0xFF2CA0,0xFF2E20,0xFF2F30};
    constexpr std::array<std::array<std::uint8_t,16>,3> prefixes{{
        {0x48,0x89,0x5C,0x24,0x10,0x48,0x89,0x6C,0x24,0x18,0x48,0x89,0x7C,0x24,0x20,0x41},
        {0x40,0x53,0x56,0x48,0x83,0xEC,0x38,0x48,0x8B,0x81,0xC0,0,0,0,0x33,0xF6},
        {0x48,0x8B,0x91,0xC0,0,0,0,0x45,0x33,0xC9,0x48,0x85,0xD2,0x7E,0x2E,0x48}}};
    for(std::size_t i=0;i<rvas.size();++i) {
        std::array<std::byte,16> actual{};
        if(!poll.copy(image+rvas[i],actual) || std::memcmp(actual.data(),prefixes[i].data(),16)!=0) { return output; }
    }
    output.valid=native_counts(image,encounter,std::span{tags}.first(output.groups),output);
    return output;
}

inline void population(Poll& poll,Worker& previous,std::uintptr_t image,
                       std::uint32_t index,const Entry& item,std::uint32_t workerRegistry) noexcept {
    if(item.kind!=0 || item.encounter==UINT32_MAX || item.palette>=26
        || poll.encounters>=kEncounterBudget) { return; }
    ++poll.encounters;
    Reference ref{};
    std::uintptr_t encounter{};
    std::array<std::byte,0x150> head{};
    unsigned status=1;
    Totals totals{};
    Completion counts{};
    std::uint32_t tracked{};
    std::array<std::byte,kGetterPrefix.size()> prefix{};
    const bool getterValid=image!=0 && poll.copy(image+0x4F0290,prefix) && prefix==kGetterPrefix;
    if(!getterValid) { status=4; }
    if(getterValid && encounter_reference(image,item.encounter,ref) && resolve(poll,image,ref,encounter)
        && poll.copy(encounter,head)) {
        status=2;
        const auto count=read<std::int64_t>(head,0xC0);
        tracked=read<std::uint32_t>(head,0x100);
        std::uintptr_t rows{};
        // FF6830 writes worker+24 and the entry index into these backlinks.
        if(read<std::uint32_t>(head,0x148)==workerRegistry
            && read<std::int32_t>(head,0x14C)==static_cast<std::int32_t>(index)
            && count>=0 && count<=kMaximumActorRows
            && relative(encounter,read<std::int64_t>(head,0xC8),0xD8,rows)) {
            std::array<std::byte,kMaximumActorRows*kActorRowBytes> data{};
            const auto bytes=static_cast<std::size_t>(count)*kActorRowBytes;
            if(poll.copy(rows,std::span{data}.first(bytes))
                && actor_totals(std::span{data}.first(bytes),count,totals)) {
                status=3;counts=completion(poll,image,encounter,head,std::span{data}.first(bytes),count);
            }
        }
    }
    auto digest=std::uint64_t{1469598103934665603ULL};
    for(const auto value : {static_cast<std::uint64_t>(item.encounter),
         static_cast<std::uint64_t>(status),static_cast<std::uint64_t>(totals.rows),
         static_cast<std::uint64_t>(totals.enabled),static_cast<std::uint64_t>(totals.remaining),
         static_cast<std::uint64_t>(totals.queued),static_cast<std::uint64_t>(tracked),
         static_cast<std::uint64_t>(item.pending)}) { fold(digest,value); }
    fold(digest,counts.valid);fold(digest,counts.groups);fold(digest,counts.firstTag);
    fold(digest,counts.activeQueued);fold(digest,counts.remaining);fold(digest,counts.required);
    fold(digest,counts.taggedRemaining);fold(digest,counts.taggedQueued);
    if(previous.encounters[index]!=digest && poll.report(
        "ev=forest_enemy stage=population run=%llu worker=%p entry=%u palette=%u area=%u "
        "encounter=%08X address=%p status=%u rows=%u enabled_rows=%u remaining=%lld "
        "queued=%lld tracked_actor_slots=%u pending=%u counts_valid=%u native_active_queued=%d "
        "native_remaining=%d groups=%u group0=%08X native_required=%lld tagged_remaining=%lld tagged_queued=%lld",
        static_cast<unsigned long long>(run),reinterpret_cast<void*>(previous.address),index,
        static_cast<unsigned>(item.palette),static_cast<unsigned>(item.area),item.encounter,
        reinterpret_cast<void*>(encounter),status,totals.rows,totals.enabled,
        static_cast<long long>(totals.remaining),static_cast<long long>(totals.queued),tracked,
        static_cast<unsigned>(item.pending),unsigned(counts.valid),counts.activeQueued,counts.remaining,
        counts.groups,counts.firstTag,static_cast<long long>(counts.required),
        static_cast<long long>(counts.taggedRemaining),static_cast<long long>(counts.taggedQueued))) {
        previous.encounters[index]=digest;
    }
}

inline void sample(Poll& poll,Worker& previous,std::uintptr_t image) noexcept {
    std::array<std::byte,kWorkerBytes> worker{};
    if(!poll.copy(previous.address,worker)
        || read<std::uint32_t>(worker,4)!=0x80804FECU
        || read<std::uint32_t>(worker,0x96C)!=0x80F4E6E7U) { return; }
    const auto count=read<std::int32_t>(worker,0x924);
    const auto pairs=read<std::uint32_t>(worker,0x9C0);
    const auto state=read<std::uint8_t>(worker,0x9BC);
    if(count<0 || count>4096 || pairs>kMaximumPairs || state>6) {
        auto invalid=std::uint64_t{1469598103934665603ULL};
        fold(invalid,count);fold(invalid,pairs);fold(invalid,state);
        if(previous.invalidHash!=invalid && poll.report(
            "ev=forest_enemy stage=invalid_worker run=%llu worker=%p entries=%d pairs=%u state=%u",
            static_cast<unsigned long long>(run),reinterpret_cast<void*>(previous.address),count,pairs,
            static_cast<unsigned>(state))) { previous.invalidHash=invalid; }
        return;
    }
    auto digest=hash(std::span{worker}.subspan(0x9C0,4+pairs*8));
    fold(digest,count);fold(digest,state);
    std::array<std::uint32_t,4> races{};
    std::array<bool,4> present{};
    constexpr std::array<std::uint32_t,4> keys{0x67AF9045,0x0D979BCD,0x89567586,0xAD3780EE};
    for(std::size_t i=0;i<pairs;++i) {
        for(std::size_t j=0;j<keys.size();++j) {
            if(read<std::uint32_t>(worker,0x9C4+i*8)==keys[j]) {
                present[j]=true;races[j]=read<std::uint32_t>(worker,0x9C8+i*8);
            }
        }
    }
    if(previous.summaryHash!=digest && poll.report(
        "ev=forest_enemy stage=selection run=%llu worker=%p config=80F4E6E7 state=%u entries=%d "
        "captured_entries=%u pairs=%u selection_hash=%016llX vex=%u/%08X cabal=%u/%08X fallen=%u/%08X hive=%u/%08X active_token=050C5D2E",
        static_cast<unsigned long long>(run),reinterpret_cast<void*>(previous.address),
        static_cast<unsigned>(state),count,static_cast<unsigned>((std::min)(static_cast<std::size_t>(count),kMaximumEntries)),
        pairs,static_cast<unsigned long long>(digest),
        unsigned(present[0]),races[0],unsigned(present[1]),races[1],unsigned(present[2]),races[2],
        unsigned(present[3]),races[3])) { previous.summaryHash=digest; }
    const auto areaCount=read<std::int32_t>(worker,0x928);
    std::uintptr_t areaBase{};
    if(areaCount>=0 && areaCount<=128
        && relative(previous.address,read<std::int64_t>(worker,0x880),0x890,areaBase)) {
        for(std::int32_t i=0;i<areaCount && poll.emitted<kPollLineBudget;++i) {
            std::array<std::byte,0x4C> area{};std::uintptr_t at{};
            if(!relative(areaBase,0,static_cast<std::size_t>(i)*0x4C,at) || !poll.copy(at,area)) { break; }
            const auto areaHash=hash(area);
            if(previous.areas[static_cast<std::size_t>(i)]!=areaHash && poll.report(
                "ev=forest_enemy stage=area run=%llu worker=%p area=%d entry=%u attached=%u native_clear=%u",
                static_cast<unsigned long long>(run),reinterpret_cast<void*>(previous.address),i,
                unsigned(read<std::uint16_t>(area,0)),read<std::uint32_t>(area,4),
                unsigned(read<std::uint8_t>(area,0x48)))) {
                previous.areas[static_cast<std::size_t>(i)]=areaHash;
            }
        }
    }
    const auto gates=read<std::int32_t>(worker,0x92C);
    std::uintptr_t gateBase{};
    if(gates>=0 && gates<=128 && relative(previous.address,read<std::int64_t>(worker,0x890),0x8A0,gateBase)) {
        for(std::int32_t i=0;i<gates && poll.emitted<kPollLineBudget;++i) {
            std::array<std::byte,2> flags{};
            std::uintptr_t at{};
            if(!relative(gateBase,0,static_cast<std::size_t>(i)*0x360+0x354,at)
                || !poll.copy(at,flags)) { break; }
            const auto gateHash=hash(flags);
            if(previous.gates[static_cast<std::size_t>(i)]!=gateHash && poll.report(
                "ev=forest_enemy stage=gateway run=%llu worker=%p gateway=%d native_354=%u open=%u",
                static_cast<unsigned long long>(run),reinterpret_cast<void*>(previous.address),i,
                unsigned(read<std::uint8_t>(flags,0)),unsigned(read<std::uint8_t>(flags,1)))) {
                previous.gates[static_cast<std::size_t>(i)]=gateHash;
            }
            std::array<std::byte,0x340> parts{};std::uintptr_t partBase{};
            if(!relative(gateBase,0,static_cast<std::size_t>(i)*0x360,partBase)
                || !poll.copy(partBase,parts)) { break; }
            const auto partCount=read<std::int32_t>(parts,0);
            if(partCount<0 || partCount>17) { continue; }
            for(std::int32_t j=0;j<partCount && poll.emitted<kPollLineBudget;++j) {
                const auto part=std::span{parts}.subspan(0x10+static_cast<std::size_t>(j)*0x30,0x30);
                const auto partHash=hash(part);
                auto& previousPart=previous.gateParts[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)];
                if(previousPart!=partHash && poll.report(
                        "ev=forest_enemy stage=gate_part run=%llu worker=%p gateway=%d part=%d area=%u "
                        "weak_serial=%08X weak_actor=%08X controller=%08X",
                        static_cast<unsigned long long>(run),reinterpret_cast<void*>(previous.address),i,j,
                        unsigned(read<std::uint8_t>(part,0x20)),read<std::uint32_t>(part,0),
                        read<std::uint32_t>(part,4),read<std::uint32_t>(part,8))) {
                    previousPart=partHash;
                }
            }
        }
    }
    const auto entries=(std::min)(static_cast<std::size_t>(count),kMaximumEntries);
    std::uintptr_t first{};
    std::array<std::byte,kMaximumEntries*kEntryBytes> raw{};
    if(!relative(previous.address,read<std::int64_t>(worker,0x858),0x868,first)
        || !poll.copy(first,std::span{raw}.first(entries*kEntryBytes))) { return; }
    for(std::size_t visited=0;visited<entries && poll.emitted<kPollLineBudget;++visited) {
        const auto index=previous.nextEntry%static_cast<std::uint32_t>(entries);
        previous.nextEntry=(index+1)%static_cast<std::uint32_t>(entries);
        const auto item=entry(std::span{raw}.subspan(index*kEntryBytes,kEntryBytes));
        auto entryHash=std::uint64_t{1469598103934665603ULL};
        fold(entryHash,item.encounter);fold(entryHash,item.kind);fold(entryHash,item.state);
        fold(entryHash,item.palette);fold(entryHash,item.area);fold(entryHash,item.gateway);fold(entryHash,item.pending);
        if(previous.entries[index]!=entryHash && poll.report(
            "ev=forest_enemy stage=entry run=%llu worker=%p entry=%u kind=%u state=%u palette=%u area=%u gateway=%u pending=%u encounter=%08X",
            static_cast<unsigned long long>(run),reinterpret_cast<void*>(previous.address),index,
            unsigned(item.kind),unsigned(item.state),unsigned(item.palette),unsigned(item.area),
            unsigned(item.gateway),unsigned(item.pending),item.encounter)) { previous.entries[index]=entryHash; }
        population(poll,previous,image,index,item,read<std::uint32_t>(worker,0x24));
        if(poll.encounters>=kEncounterBudget) { break; }
    }
}
} // namespace receipts_detail

/** After original exact-Omega worker tick only. No hook installation or gameplay mutation. */
inline void observe(void* instance,std::uint64_t generation) noexcept {
    using namespace receipts_detail;
    if(instance==nullptr || !TryAcquireSRWLockExclusive(&lock)) { return; }
    if(run!=generation) { run=generation;lines=0;workers={}; }
    Poll poll{};
    const auto address=reinterpret_cast<std::uintptr_t>(instance);
    std::array<std::byte,0x28> identity{};std::uint32_t config{};
    if(!poll.copy(address,identity) || read<std::uint32_t>(identity,4)!=0x80804FECU
        || address>UINTPTR_MAX-0x96C || !poll.value(address+0x96C,config) || config!=0x80F4E6E7U
        || read<std::uint32_t>(identity,0x24)==UINT32_MAX) {
        ReleaseSRWLockExclusive(&lock);return;
    }
    auto& selected=select_worker(read<std::uint32_t>(identity,0x24));
    const auto now=GetTickCount64();
    if((selected.address==0 || now-selected.lastPoll>=kIntervalMs)
        && lines<kRunLineBudget) {
        if(selected.address!=0 && selected.address!=address) {
            poll.report("ev=forest_enemy stage=worker_moved run=%llu self=%08X previous=%p worker=%p",
                static_cast<unsigned long long>(run),selected.self,
                reinterpret_cast<void*>(selected.address),instance);
        }
        selected.address=address;selected.lastPoll=now;
        sample(poll,selected,reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)));
        if(lines==kRunLineBudget) {
            core::log::write(core::log::Channel::client,core::log::Level::info,
                "ev=forest_enemy stage=budget_exhausted mutation=none");
        }
    }
    ReleaseSRWLockExclusive(&lock);
}
} // namespace dawn::client::hooks::bootflow::omega_enemy_forest
