#include "state/activity/native_population_events.h"
#include "client/hooks/bootflow/native_generated_population_identity.h"
#include "client/hooks/bootflow/native_generated_roster.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace identity=sunrise::client::hooks::bootflow::native_generated_population_identity;
namespace events=sunrise::state::activity::native_population;
namespace coo=sunrise::state::activity::coo;

namespace {
unsigned checks{};
#define CHECK(value) do { ++checks; if(!(value)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#value); std::exit(1); } } while(false)

struct Ref final {
    std::uint32_t handle{UINT32_MAX},kind{};
    std::int64_t offset{};
};

struct Reader final {
    static constexpr std::uintptr_t image=0x10000000U;
    static constexpr std::uintptr_t sourceAddress=0x200000U;
    static constexpr std::uintptr_t paletteDefinitionAddress=0x210000U;
    static constexpr std::uintptr_t workerAddress=0x220000U;
    static constexpr std::uintptr_t workerDefinitionAddress=0x230000U;
    static constexpr std::uintptr_t encounterAddress=0x240000U;
    static constexpr std::uintptr_t poolAddress=0x250000U;
    static constexpr std::uintptr_t poolLinkAddress=0x260000U;
    static constexpr std::uintptr_t bitmapLinkAddress=0x270000U;
    static constexpr std::uintptr_t bitmapAddress=0x280000U;
    static constexpr std::uintptr_t poolRowsAddress=0x290000U;
    static constexpr std::uintptr_t entriesAddress=0x2A0000U;
    static constexpr std::uintptr_t arrayHeaderAddress=paletteDefinitionAddress+0x200U+0x114U;
    static constexpr std::uintptr_t firstRowsAddress=arrayHeaderAddress+0x14U;
    static constexpr std::uintptr_t selectedRowAddress=firstRowsAddress+0xC0U;

    static constexpr std::uint32_t sourceHandle=0x101U;
    static constexpr std::uint32_t sourceNativeHandle=0x808U;
    static constexpr std::uint32_t paletteTag=0x202U;
    static constexpr std::uint32_t workerHandle=0x303U;
    static constexpr std::uint32_t workerDefinitionTag=0x404U;
    static constexpr std::uint32_t encounterHandle=0x505U;
    static constexpr std::uint32_t resourceTag=0x606U;
    static constexpr std::uint32_t seed=0x707U;
    static constexpr std::int64_t paletteDefinitionOffset=0x8050LL;
    static constexpr std::int64_t definitionRuntimeOffset=0x70LL;
    static constexpr std::int64_t workerDefinitionOffset=0x1060LL;
    static constexpr std::int64_t actorRowsRelative=0x200LL;
    static constexpr std::int64_t sourceActorRowsRelative=0xE8LL;
    static constexpr std::uint32_t actorRowsOffset=static_cast<std::uint32_t>(
        paletteDefinitionOffset+actorRowsRelative+0x128LL);
    static constexpr std::int64_t memberOffset=static_cast<std::int64_t>(actorRowsOffset)+0xC0LL+0x10LL;
    static constexpr std::int64_t actorRowReferenceOffset=definitionRuntimeOffset
        +sourceActorRowsRelative+identity::kActorRowReferenceBaseOffset
        +identity::kActorRowReferenceStride;

    Ref actorSource{sourceHandle,identity::kSourceRuntimeClass,0};
    Ref paletteRuntime{paletteTag,identity::kSourceRuntimeClass,paletteDefinitionOffset};
    Ref workerDefinition{workerDefinitionTag,identity::kWorkerRuntimeClass,workerDefinitionOffset};
    Ref member{paletteTag,identity::kMemberReferenceClass,memberOffset};

    std::array<std::byte,0x150> source{};
    std::array<std::byte,0x970> worker{};
    std::array<std::byte,0x28> workerDefinitionBytes{};
    std::array<std::byte,0x120> paletteDefinition{};
    std::array<std::byte,0x38> entries{};
    std::array<std::byte,0x18> pool{};
    std::array<std::byte,0x18> poolLink{};
    std::array<std::byte,0x20> bitmapLink{};
    std::array<std::byte,0x400> bitmap{};
    std::array<std::byte,0x60> poolRows{};
    std::array<std::byte,20> arrayHeader{};
    std::array<std::byte,0xC0> actorRow{};
    std::array<std::byte,8> selectorTable{};

    struct Block final { std::uintptr_t address{}; const std::byte* bytes{}; std::size_t size{}; };
    std::array<Block,16> blocks{};
    std::size_t blockCount{};

    template<class T,std::size_t N>
    static void put(std::array<std::byte,N>& bytes,std::size_t offset,T value) noexcept {
        std::memcpy(bytes.data()+offset,&value,sizeof(value));
    }
    template<std::size_t N>
    void addBlock(std::uintptr_t address,const std::array<std::byte,N>& bytes) noexcept {
        blocks[blockCount++]={address,bytes.data(),N};
    }

    void init() noexcept {
        blockCount=0;
        addBlock(sourceAddress,source);
        addBlock(workerAddress,worker);
        addBlock(workerDefinitionAddress,workerDefinitionBytes);
        addBlock(paletteDefinitionAddress,paletteDefinition);
        addBlock(encounterAddress,encounterBytes);
        addBlock(entriesAddress,entries);
        addBlock(poolAddress,pool);
        addBlock(poolLinkAddress,poolLink);
        addBlock(bitmapLinkAddress,bitmapLink);
        addBlock(bitmapAddress,bitmap);
        addBlock(poolRowsAddress,poolRows);
        addBlock(arrayHeaderAddress,arrayHeader);
        addBlock(selectedRowAddress,actorRow);

        put(source,0,paletteRuntime);
        put(source,0x24,sourceNativeHandle);
        put(source,0xC0,std::int64_t{2});
        put(source,0xC8,sourceActorRowsRelative);
        put(source,0x148,workerHandle);
        put(source,0x14C,std::int32_t{0});

        put(worker,0,workerDefinition);
        put(worker,4,identity::kWorkerRuntimeClass);
        put(worker,0x24,workerHandle);
        put(worker,0x850,std::int32_t{1});
        put(worker,0x858,static_cast<std::int64_t>(entriesAddress-workerAddress-0x868U));
        put(worker,0x940,seed);
        put(worker,0x94C,std::uint32_t{0});
        put(worker,0x96C,resourceTag);

        put(workerDefinitionBytes,4,identity::kWorkerReferenceClass);
        put(workerDefinitionBytes,0,workerDefinitionTag);
        put(entries,0x18,std::uint8_t{0});
        put(entries,0x34,encounterHandle);
        put(encounterBytes,8,std::int8_t{0});

        put(pool,0,poolLinkAddress);
        put(pool,8,poolRowsAddress);
        put(pool,0x10,std::int32_t{0x30});
        put(pool,0x14,std::int32_t{2});
        put(poolLink,8,bitmapLinkAddress);
        put(bitmapLink,0x10,bitmapAddress);
        bitmap[0]=std::byte{1};

        put(poolRows,0x18,encounterHandle);
        put(poolRows,0x20,actorSource);
        put(poolRows,0x48,encounterHandle+1U);
        put(poolRows,0x50,actorSource);

        put(paletteDefinition,4,identity::kSourceDefinitionClass);
        put(paletteDefinition,0,paletteTag);
        put(paletteDefinition,8,definitionRuntimeOffset);
        put(paletteDefinition,0x110,std::int64_t{2});
        put(paletteDefinition,0x118,actorRowsRelative);
        put(arrayHeader,0,identity::kArrayHeaderMarker);
        put(arrayHeader,4,std::uint64_t{2});
        put(arrayHeader,12,identity::kPaletteArrayElementClass);
        put(actorRow,0,Ref{paletteTag,identity::kPaletteRowReferenceClass,actorRowReferenceOffset});
        put(actorRow,0x10,std::uint32_t{0x12345678U});
        put(actorRow,0xA8,std::uint32_t{0x87654321U});
        put(selectorTable,0,poolAddress);
    }

    // Kept as a separate local block so the fixture's handle table remains
    // explicit rather than relying on a packaged or process-backed object.
    std::array<std::byte,0x10> encounterBytes{};

    bool copy(std::uintptr_t address,std::span<std::byte> out) noexcept {
        if(address==image+0x1F921B0U && out.size()==sizeof(std::uintptr_t)) {
            std::memcpy(out.data(),selectorTable.data(),out.size());
            return true;
        }
        for(std::size_t i=0;i<blockCount;++i) {
            const auto& block=blocks[i];
            if(address>=block.address && address-block.address<=block.size
                && out.size()<=block.size-(address-block.address)) {
                std::memcpy(out.data(),block.bytes+(address-block.address),out.size());
                return true;
            }
        }
        return false;
    }
    template<class T>
    bool value(std::uintptr_t address,T& out) noexcept {
        return copy(address,std::as_writable_bytes(std::span{&out,std::size_t{1}}));
    }
    bool resolve(const Ref& ref,std::uintptr_t& out) noexcept {
        if(ref.handle==sourceHandle && ref.kind==identity::kSourceRuntimeClass && ref.offset==0) {
            out=sourceAddress;return true;
        }
        if(ref.handle==paletteTag && ref.kind==identity::kSourceRuntimeClass
            && ref.offset==paletteDefinitionOffset) {
            out=paletteDefinitionAddress;return true;
        }
        if(ref.handle==workerDefinitionTag && ref.kind==identity::kWorkerRuntimeClass
            && ref.offset==workerDefinitionOffset) { out=workerDefinitionAddress;return true; }
        if(ref.handle==workerHandle && ref.kind==0 && ref.offset==0) { out=workerAddress;return true; }
        if(ref.handle==encounterHandle && ref.kind==0 && ref.offset==0) { out=encounterAddress;return true; }
        return false;
    }
};

events::Lease fixedLease() {
    const sunrise::state::activity::ActivityInstanceKey activity{1,{2}};
    return {activity,{1,3,2,{0x1111U,0x2222U,1,0},4},15};
}

void positive_identity() {
    Reader reader;reader.init();identity::Identity output{};
    CHECK(identity::qualify(reader,Reader::image,reader.actorSource,reader.member,output));
    CHECK(output.resourceTag==Reader::resourceTag);
    CHECK(output.seed==Reader::seed);
    CHECK(output.workerDefinitionTag==Reader::workerDefinitionTag);
    CHECK(output.workerDefinitionOffset==static_cast<std::uint32_t>(Reader::workerDefinitionOffset));
    CHECK(output.paletteDefinitionTag==Reader::paletteTag);
    CHECK(output.paletteDefinitionOffset==static_cast<std::uint32_t>(Reader::paletteDefinitionOffset));
    CHECK(output.actorRowsOffset==Reader::actorRowsOffset);
    CHECK(output.actorRowCount==2U);
    CHECK(output.memberPrefabTag==0x12345678U);
    CHECK(output.completionGroup==0x87654321U);
    // An override operand is not the seed applied by the native worker.
    Reader::put(reader.worker,0x94C,std::uint32_t{0x999U});
    CHECK(identity::qualify(reader,Reader::image,reader.actorSource,reader.member,output));
    CHECK(output.seed==Reader::seed);
}

template<class Mutator>
void rejection(const char* name,Mutator mutate) {
    Reader reader;reader.init();mutate(reader);identity::Identity output{};
    if(identity::qualify(reader,Reader::image,reader.actorSource,reader.member,output)) {
        std::fprintf(stderr,"FAIL accepted mutation: %s\n",name);std::exit(1);
    }
    ++checks;
}

void qualification_rejections() {
    rejection("source runtime kind",[](Reader& r) { r.actorSource.kind=identity::kSourceDefinitionClass; });
    rejection("source runtime offset negative",[](Reader& r) { r.actorSource.offset=-1; });
    rejection("source reference kind",[](Reader& r) { Reader::put(r.source,4,identity::kSourceDefinitionClass); });
    rejection("palette definition header",[](Reader& r) { Reader::put(r.paletteDefinition,4,identity::kSourceRuntimeClass); });
    rejection("palette definition handle",[](Reader& r) { Reader::put(r.paletteDefinition,0,Reader::paletteTag+1U); });
    rejection("palette pointer",[](Reader& r) { r.paletteRuntime.handle=UINT32_MAX;Reader::put(r.source,0,r.paletteRuntime); });
    rejection("worker runtime header",[](Reader& r) { Reader::put(r.worker,4,identity::kSourceRuntimeClass); });
    rejection("worker self relation",[](Reader& r) { Reader::put(r.worker,0x24,Reader::workerHandle+1U); });
    rejection("worker reference kind",[](Reader& r) { r.workerDefinition.kind=identity::kWorkerReferenceClass;Reader::put(r.worker,0,r.workerDefinition); });
    rejection("worker definition header",[](Reader& r) { Reader::put(r.workerDefinitionBytes,4,identity::kSourceDefinitionClass); });
    rejection("worker definition handle",[](Reader& r) { Reader::put(r.workerDefinitionBytes,0,Reader::workerDefinitionTag+1U); });
    rejection("worker definition offset",[](Reader& r) { r.workerDefinition.offset=0x1064;Reader::put(r.worker,0,r.workerDefinition); });
    rejection("seed salt",[](Reader& r) { Reader::put(r.worker,0x940,std::uint32_t{0}); });
    rejection("resource salt",[](Reader& r) { Reader::put(r.worker,0x96C,std::uint32_t{0}); });
    rejection("entry index",[](Reader& r) { Reader::put(r.source,0x14C,std::int32_t{1}); });
    rejection("entry active flag",[](Reader& r) { Reader::put(r.entries,0x18,std::uint8_t{1}); });
    rejection("encounter identity",[](Reader& r) { Reader::put(r.entries,0x34,UINT32_MAX); });
    rejection("selector bounds",[](Reader& r) { Reader::put(r.encounterBytes,8,std::int8_t{64}); });
    rejection("pool stride",[](Reader& r) { Reader::put(r.pool,0x10,std::int32_t{0x1001}); });
    rejection("bitmap pointer",[](Reader& r) { Reader::put(r.bitmapLink,0x10,UINTPTR_MAX); });
    rejection("pool row read bound",[](Reader& r) { Reader::put(r.pool,8,UINTPTR_MAX-0x20U); });
    rejection("duplicate pool identity",[](Reader& r) { r.bitmap[0]=std::byte{3};Reader::put(r.poolRows,0x48,Reader::encounterHandle); });
    rejection("array marker",[](Reader& r) { Reader::put(r.arrayHeader,0,identity::kSourceDefinitionClass); });
    rejection("array count",[](Reader& r) { Reader::put(r.arrayHeader,4,std::uint64_t{1}); });
    rejection("array element class",[](Reader& r) { Reader::put(r.arrayHeader,12,identity::kSourceDefinitionClass); });
    rejection("row relative bounds",[](Reader& r) { Reader::put(r.paletteDefinition,0x118,std::int64_t{-1}); });
    rejection("absolute row offset overflow",[](Reader& r) { Reader::put(r.paletteDefinition,0x118,std::int64_t{0xFFFF0}); });
    rejection("member handle",[](Reader& r) { r.member.handle=Reader::paletteTag+1U; });
    rejection("member kind",[](Reader& r) { r.member.kind=identity::kPaletteRowReferenceClass; });
    rejection("member relative offset",[](Reader& r) { r.member.offset=0x238; });
    rejection("member row alignment",[](Reader& r) { ++r.member.offset; });
    rejection("row reference kind",[](Reader& r) { Reader::put(r.actorRow,0,Ref{Reader::paletteTag,identity::kMemberReferenceClass,0}); });
    rejection("row reference handle",[](Reader& r) { Reader::put(r.actorRow,0,Ref{Reader::workerDefinitionTag,identity::kPaletteRowReferenceClass,0}); });
    rejection("row reference offset below",[](Reader& r) {
        Reader::put(r.actorRow,0,Ref{Reader::paletteTag,identity::kPaletteRowReferenceClass,
            Reader::actorRowReferenceOffset-1});
    });
    rejection("row reference offset above",[](Reader& r) {
        Reader::put(r.actorRow,0,Ref{Reader::paletteTag,identity::kPaletteRowReferenceClass,
            Reader::actorRowReferenceOffset+1});
    });
}

void mailbox_metadata() {
    const auto lease=fixedLease();events::Mailbox mailbox;CHECK(mailbox.bind(lease));
    const events::Event admitted{lease,{lease.source,0x900U,0x901U},0x902U,events::Kind::admitted,0xA03U,0xB04U};
    auto died=admitted;died.kind=events::Kind::died;
    auto retired=admitted;retired.kind=events::Kind::retired;
    const auto epoch=mailbox.epoch();
    CHECK(mailbox.submit(admitted,epoch));CHECK(mailbox.submit(died,epoch));CHECK(mailbox.submit(retired,epoch));
    std::array<events::Event,3> output{};CHECK(mailbox.drain(lease.activity,output)==3);
    for(const auto& event:output) CHECK(event.memberPrefabTag==0xA03U && event.completionGroup==0xB04U);
}

void roster_context_contract() {
    namespace roster=sunrise::client::hooks::bootflow::native_generated_roster;
    Ref getter{0x123U,0x8080501CU,0};
    CHECK(roster::valid_getter_reference(getter));
    getter.kind=0x8080501DU;CHECK(!roster::valid_getter_reference(getter));
    getter.kind=0x8080501CU;getter.offset=4;CHECK(!roster::valid_getter_reference(getter));
    CHECK(roster::valid_runtime_source_header(0x8080501DU));
    CHECK(!roster::valid_runtime_source_header(0x8080501CU));

    const roster::RosterRow row{0xA01U,0xB02U,325};
    CHECK(roster::valid_row(row,roster::kMaximumAuthoredRows));
    CHECK(!roster::valid_row(row,325));
    CHECK(!roster::valid_row({0xA01U,0xB02U,512},roster::kMaximumAuthoredRows));
    CHECK(!roster::valid_row({UINT32_MAX,UINT32_MAX,0},roster::kMaximumAuthoredRows));

    std::uint32_t cursor{};
    CHECK(roster::next_entry(cursor,3)==0U);
    CHECK(roster::next_entry(cursor,3)==1U);
    CHECK(roster::next_entry(cursor,3)==2U);
    CHECK(roster::next_entry(cursor,3)==0U);
    CHECK(roster::next_entry(cursor,0)==roster::kInvalidHandle);
    CHECK(roster::current_early_death_epoch(7,7,7));
    CHECK(!roster::current_early_death_epoch(6,7,7));
    CHECK(!roster::current_early_death_epoch(7,6,7));
    CHECK(!roster::current_early_death_epoch(7,7,8));
}

void fair_generated_roster_schedule() {
    namespace roster=sunrise::client::hooks::bootflow::native_generated_roster;
    roster::ScheduleCursor cursor;
    const roster::WorkerTuple worker{1U,2U,3U,4U};
    cursor.synchronize(5U,worker);
    std::array<std::array<bool,roster::kMaximumRosterSlots>,2> live{};
    std::array<bool,2*roster::kMaximumRosterSlots> admitted{};
    for(std::uint32_t i=0;i<12U;++i) {
        live[0][8U+i]=true;
        live[1][243U+i]=true;
    }

    unsigned polls{};
    std::size_t discovered{};
    while(discovered<24U && polls<8U) {
        ++polls;
        std::size_t attempts{};
        for(unsigned selected=0;selected<2U;++selected) {
            const auto entry=cursor.select_entry(2U);
            for(std::uint32_t visited=0;visited<roster::kMaximumRosterSlots;++visited) {
                const auto row=cursor.row_cursor(entry,roster::kMaximumRosterSlots);
                CHECK(row!=roster::kInvalidHandle);
                const auto id=static_cast<std::size_t>(entry)*roster::kMaximumRosterSlots+row;
                if(!live[entry][row]) {
                    cursor.advance_row(entry,roster::kMaximumRosterSlots);continue;
                }
                if(admitted[id]) {
                    // This is the production cache-hit path: it advances the
                    // entry cursor without spending a candidate attempt.
                    cursor.advance_row(entry,roster::kMaximumRosterSlots);continue;
                }
                if(attempts==8U) break;
                ++attempts;admitted[id]=true;++discovered;
                cursor.advance_row(entry,roster::kMaximumRosterSlots);
            }
        }
    }
    CHECK(discovered==24U);
    CHECK(polls==3U);

    // A worker salt/definition change resets every per-entry row cursor.
    cursor.advance_row(1U,roster::kMaximumRosterSlots);
    cursor.synchronize(5U,{1U,9U,3U,4U});
    CHECK(cursor.row_cursor(1U,roster::kMaximumRosterSlots)==0U);
}
}

int main() {
    positive_identity();
    qualification_rejections();
    mailbox_metadata();
    roster_context_contract();
    fair_generated_roster_schedule();
    std::printf("native_generated_identity_tests: %u checks passed\n",checks);
}
