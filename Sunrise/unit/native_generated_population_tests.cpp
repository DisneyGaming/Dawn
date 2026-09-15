#include "state/activity/native_population_events.h"
#include "client/hooks/bootflow/native_generated_population_identity.h"
#include "client/hooks/bootflow/native_generated_roster.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace e=sunrise::state::activity::native_population;
namespace c=sunrise::state::activity::coo;
unsigned checks{};
#define CHECK(value) do { ++checks;if(!(value)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#value);std::exit(1); } } while(false)

constexpr std::uint32_t kRegistry=0x10000001U,kDefinition=0x10000002U,kGenerator=0x10000003U;
constexpr std::uint32_t kPaletteSet=0x10000004U,kPalette=0x10000005U,kWorkerOffset=0x120U;
constexpr std::uint32_t kPaletteOffset=0x40U,kRowsOffset=0x100U,kSeed=17U,kSourceGeneration=77U;
constexpr std::uint32_t kSourceHandle=9U;

e::Lease lease(std::uint64_t session,std::uint64_t incarnation,std::uint64_t run,
    std::uint32_t generation,std::uint16_t type,std::uint32_t definition,std::uint32_t registry) {
    const sunrise::state::activity::ActivityInstanceKey activity{session,{incarnation}};
    return {activity,{session,run,incarnation,{registry,definition,type,0},generation},15};
}

struct GeneratedRef final {
    std::uint32_t handle{UINT32_MAX},kind{};std::int64_t offset{};
};
struct GeneratedReader final {
    static constexpr std::uintptr_t image=0x10000000U;
    static constexpr std::uintptr_t sourceAddress=0x110000U,workerAddress=0x111000U;
    static constexpr std::uintptr_t definitionAddress=0x112000U,encounterAddress=0x113000U;
    static constexpr std::uintptr_t workerDefinitionAddress=0x11A000U;
    static constexpr std::uintptr_t poolAddress=0x114000U,poolLink=0x115000U;
    static constexpr std::uintptr_t bitmapLink=0x116000U,poolRows=0x117000U,entries=0x118000U,bitmap=0x119000U;
    static constexpr std::int64_t paletteDefinitionOffset=0x8050LL;
    static constexpr std::int64_t definitionRuntimeOffset=0x70LL;
    static constexpr std::int64_t actorRowsRelative=0x200LL;
    static constexpr std::int64_t sourceActorRowsRelative=0xE8LL;
    static constexpr std::int64_t actorRowReferenceOffset=definitionRuntimeOffset
        +sourceActorRowsRelative+0xD8LL;
    static constexpr std::uintptr_t arrayHeaderAddress=definitionAddress+actorRowsRelative+0x114U;
    static constexpr std::uintptr_t firstRowsAddress=arrayHeaderAddress+0x14U;
    std::array<std::byte,0x970> worker{};std::array<std::byte,0x150> source{};
    std::array<std::byte,0x120> definition{};std::array<std::byte,0x38> entry{};
    std::array<std::byte,0x28> workerDefinition{};
    std::array<std::byte,0x18> pool{};std::array<std::byte,0x20> poolLinkBytes{};
    std::array<std::byte,0x30> bitmapLinkBytes{};std::array<std::byte,0x400> bitmapBytes{};std::array<std::byte,0x60> rows{};
    std::array<std::byte,0x38> entriesBytes{};std::array<std::byte,20> arrayHeader{};
    std::array<std::byte,0xC0> actorRow{};std::array<std::byte,8> selectorTable{};
    struct Block final {std::uintptr_t address{};const std::byte* bytes{};std::size_t size{};};
    std::array<Block,13> blocks{};std::size_t blockCount{};
    GeneratedRef sourceRef{1,0x8080501DU,0},definitionRef{2,0x8080501DU,paletteDefinitionOffset};
    GeneratedRef workerRef{3,0x80804FECU,0x80},memberRef{2,0x808099D8U,
        paletteDefinitionOffset+actorRowsRelative+0x128LL+0x10LL};
    void init() noexcept {
        blockCount=0;
        addBlock(sourceAddress,source);addBlock(workerAddress,worker);addBlock(workerDefinitionAddress,workerDefinition);
        addBlock(definitionAddress,definition);
        addBlock(encounterAddress,entry);addBlock(poolAddress,pool);addBlock(poolLink,poolLinkBytes);
        addBlock(bitmapLink,bitmapLinkBytes);addBlock(bitmap,bitmapBytes);addBlock(poolRows,rows);addBlock(entries,entriesBytes);
        addBlock(arrayHeaderAddress,arrayHeader);addBlock(firstRowsAddress,actorRow);
        const std::uint32_t sourceDefinitionKind=0x8080501CU;
        put(source,0,definitionRef);put(source,4,std::uint32_t{0x8080501DU});
        put(source,0xC0,std::int64_t{1});put(source,0xC8,sourceActorRowsRelative);
        put(source,0x148,std::uint32_t{3});
        put(source,0x14C,std::int32_t{0});put(worker,4,std::uint32_t{0x80804FECU});
        put(worker,0,workerRef);put(worker,0x24,std::uint32_t{3});put(worker,0x940,std::uint32_t{7});
        // +94C is the OR operand, not the applied seed used by the lease.
        put(worker,0x94C,std::uint32_t{0});
        put(worker,0x96C,std::uint32_t{5});put(worker,0x850,std::int32_t{1});
        put(worker,0x858,static_cast<std::int64_t>(entries-workerAddress-0x868));
        put(entriesBytes,0x18,std::uint8_t{0});put(entriesBytes,0x34,std::uint32_t{6});
        put(entry,8,std::int8_t{0});put(pool,0,poolLink);put(pool,8,poolRows);
        put(pool,0x10,std::int32_t{0x30});put(pool,0x14,std::int32_t{2});
        put(poolLinkBytes,8,bitmapLink);put(bitmapLinkBytes,0x10,bitmap);bitmapBytes[0]=std::byte{1};
        put(rows,0x18,std::uint32_t{6});put(rows,0x20,sourceRef);
        put(rows,0x48,std::uint32_t{6});put(rows,0x50,sourceRef);
        put(definition,0,std::uint32_t{2});put(definition,4,sourceDefinitionKind);put(definition,8,definitionRuntimeOffset);put(definition,0x110,std::int64_t{1});
        put(definition,0x118,actorRowsRelative);put(arrayHeader,0,std::uint32_t{0x80809FBDU});
        put(arrayHeader,4,std::uint64_t{1});put(arrayHeader,12,std::uint32_t{0x8080502EU});
        put(actorRow,0,GeneratedRef{2,0x8080502DU,actorRowReferenceOffset});put(actorRow,0x10,std::uint32_t{0x12345678U});
        put(actorRow,0xA8,std::uint32_t{9});
        put(workerDefinition,0,std::uint32_t{3});put(workerDefinition,4,std::uint32_t{0x80805017U});
        put(selectorTable,0,poolAddress);
    }
    template<class T,std::size_t N> static void put(std::array<std::byte,N>& bytes,std::size_t offset,T value) noexcept {
        std::memcpy(bytes.data()+offset,&value,sizeof value);
    }
    template<std::size_t N> void addBlock(std::uintptr_t address,const std::array<std::byte,N>& bytes) noexcept {
        blocks[blockCount++]={address,bytes.data(),N};
    }
    bool copy(std::uintptr_t address,std::span<std::byte> out) noexcept {
        if(address==image+0x1F921B0U && out.size()==sizeof(std::uintptr_t)) {
            std::memcpy(out.data(),selectorTable.data(),out.size());return true;
        }
        for(std::size_t i=0;i<blockCount;++i) {
            const auto& block=blocks[i];
            if(address>=block.address && address-block.address<=block.size
                && out.size()<=block.size-(address-block.address)) {
                std::memcpy(out.data(),block.bytes+(address-block.address),out.size());return true;
            }
        }
        return false;
    }
    template<class T> bool value(std::uintptr_t address,T& out) noexcept {
        return copy(address,std::as_writable_bytes(std::span{&out,std::size_t{1}}));
    }
    bool resolve(const GeneratedRef& ref,std::uintptr_t& out) noexcept {
        if(ref.handle==sourceRef.handle && ref.kind==sourceRef.kind && ref.offset==sourceRef.offset) {out=sourceAddress;return true;}
        if(ref.handle==definitionRef.handle && ref.kind==definitionRef.kind && ref.offset==definitionRef.offset) {out=definitionAddress;return true;}
        if(ref.handle==3 && ref.kind==0 && ref.offset==0) {out=workerAddress;return true;}
        if(ref.handle==workerRef.handle && ref.kind==workerRef.kind && ref.offset==workerRef.offset) {out=workerDefinitionAddress;return true;}
        if(ref.handle==6 && ref.kind==0 && ref.offset==0) {out=encounterAddress;return true;}
        return false;
    }
};

void generated_identity_rejections() {
    namespace identity=sunrise::client::hooks::bootflow::native_generated_population_identity;
    GeneratedReader reader;reader.init();identity::Identity output{};
    CHECK(identity::qualify(reader,GeneratedReader::image,reader.sourceRef,reader.memberRef,output));
    CHECK(output.memberPrefabTag==0x12345678U && output.completionGroup==9U);
    auto wrongKind=reader.sourceRef;wrongKind.kind=identity::kMemberReferenceClass;
    CHECK(!identity::qualify(reader,GeneratedReader::image,wrongKind,reader.memberRef,output));
    auto wrongMember=reader.memberRef;wrongMember.offset=0x239;
    CHECK(!identity::qualify(reader,GeneratedReader::image,reader.sourceRef,wrongMember,output));
    reader.bitmapBytes[0]=std::byte{3};
    CHECK(!identity::qualify(reader,GeneratedReader::image,reader.sourceRef,reader.memberRef,output));
}

int main() {
    generated_identity_rejections();
    const auto generated=lease(1,1,9,kSourceGeneration,37,kGenerator,kRegistry);
    const auto fixed=lease(1,1,9,1,1,kDefinition,kRegistry);
    constexpr std::array<e::PaletteDefinition,1> palettes{{{kPalette,kPaletteOffset,kRowsOffset,2}}};
    const e::GeneratorBinding binding{generated,kSeed,kPaletteSet,kGenerator,kWorkerOffset,palettes};

    namespace roster=sunrise::client::hooks::bootflow::native_generated_roster;
    roster::Provenance provenance{};provenance.lease=generated;provenance.sourceHandle=0x901U;
    provenance.sourceKind=0x8080501CU;provenance.sourceOffset=0;
    provenance.actorHandle=0x902U;provenance.entityHandle=0x903U;provenance.parentHandle=0x904U;
    provenance.workerSelf=0x905U;provenance.entryIndex=53U;provenance.rosterRowIndex=7U;
    provenance.authoredActorRow=325U;provenance.nativeSpawnId=0x906U;provenance.memberPrefabTag=0x907U;
    provenance.completionGroup=0x908U;
    roster::Cache<> provenanceCache;
    CHECK(provenanceCache.observe(provenance)==roster::CacheIntake::accepted);
    CHECK(provenanceCache.observe(provenance)==roster::CacheIntake::duplicate);
    auto conflicting=provenance;conflicting.memberPrefabTag=0x909U;
    CHECK(provenanceCache.observe(conflicting)==roster::CacheIntake::conflict);
    provenanceCache.erase(provenance.actorHandle,provenance.entityHandle,provenance.parentHandle,generated);
    CHECK(provenanceCache.size()==0U);

    e::Mailbox mailbox;
    const e::Event generatedAdmission{generated,{generated.source,1,2},kSourceHandle,e::Kind::admitted};
    CHECK(generatedAdmission.memberPrefabTag==0 && generatedAdmission.completionGroup==UINT32_MAX);
    CHECK(!mailbox.bind(generated));
    CHECK(!mailbox.submit(generatedAdmission,mailbox.generator_epoch()));
    CHECK(!mailbox.bind_generator({fixed,kSeed,kPaletteSet,kGenerator,kWorkerOffset,palettes}));
    auto sameGeneration=binding;sameGeneration.lease.source.generation=kSeed;CHECK(!mailbox.bind_generator(sameGeneration));
    CHECK(!mailbox.bind_generator({generated,kSeed,kPaletteSet,kGenerator,kWorkerOffset,{}}));
    CHECK(mailbox.bind(fixed));
    CHECK(mailbox.lookup(kDefinition,kRegistry,0,1)==fixed);
    CHECK(!mailbox.lookup(kDefinition,kRegistry,0,kSourceGeneration).activity);
    CHECK(mailbox.bind_generator(binding));
    const auto epoch=mailbox.generator_epoch();CHECK(mailbox.bind_generator(binding));CHECK(mailbox.generator_epoch()==epoch);
    CHECK(mailbox.has_lease(fixed) && mailbox.has_lease(generated));
    CHECK(mailbox.lookup_generated(kPaletteSet,kSeed,kGenerator,kWorkerOffset,kPalette,kPaletteOffset)==generated);
    CHECK(!mailbox.lookup_generated(kPaletteSet+1,kSeed,kGenerator,kWorkerOffset,kPalette,kPaletteOffset).activity);
    CHECK(!mailbox.lookup_generated(kPaletteSet,kSeed+1,kGenerator,kWorkerOffset,kPalette,kPaletteOffset).activity);
    CHECK(!mailbox.lookup_generated(kPaletteSet,kSeed,kGenerator+1,kWorkerOffset,kPalette,kPaletteOffset).activity);
    CHECK(!mailbox.lookup_generated(kPaletteSet,kSeed,kGenerator,kWorkerOffset+1,kPalette,kPaletteOffset).activity);
    CHECK(!mailbox.lookup_generated(kPaletteSet,kSeed,kGenerator,kWorkerOffset,kPalette+1,kPaletteOffset).activity);
    CHECK(!mailbox.lookup_generated(kPaletteSet,kSeed,kGenerator,kWorkerOffset,kPalette,kPaletteOffset+1).activity);

    auto invalid=binding;std::array<e::PaletteDefinition,1> bad=palettes;
    bad[0].actorRowCount=0;invalid.palettes=bad;CHECK(!mailbox.bind_generator(invalid));
    bad[0]=palettes[0];bad[0].actorRowCount=513;CHECK(!mailbox.bind_generator({lease(2,1,9,88,37,kGenerator,kRegistry),kSeed,kPaletteSet,kGenerator,kWorkerOffset,bad}));
    bad[0]=palettes[0];bad[0].actorRowsOffset=UINT32_MAX-64U;CHECK(!mailbox.bind_generator({lease(3,1,9,89,37,kGenerator,kRegistry),kSeed,kPaletteSet,kGenerator,kWorkerOffset,bad}));
    bad[0]=palettes[0];bad[0].resourceTag=0;CHECK(!mailbox.bind_generator({lease(4,1,9,90,37,kGenerator,kRegistry),kSeed,kPaletteSet,kGenerator,kWorkerOffset,bad}));
    bad[0]=palettes[0];bad[0].definitionOffset=UINT32_MAX;CHECK(!mailbox.bind_generator({lease(5,1,9,91,37,kGenerator,kRegistry),kSeed,kPaletteSet,kGenerator,kWorkerOffset,bad}));

    e::Mailbox copied;
    const auto copiedLease=lease(6,1,9,92,37,kGenerator,kRegistry);
    {
        std::array<e::PaletteDefinition,1> temporary=palettes;
        CHECK(copied.bind_generator({copiedLease,kSeed,kPaletteSet,kGenerator,kWorkerOffset,temporary}));
    }
    CHECK(copied.lookup_generated(kPaletteSet,kSeed,kGenerator,kWorkerOffset,kPalette,kPaletteOffset)==copiedLease);

    const auto fixedAdmission=e::Event{fixed,{fixed.source,3,4},kSourceHandle,e::Kind::admitted};
    const auto drained=[&]() {return mailbox.generator_drained(generated.activity,generated.source.run,
        generated.source.source.registry,generated.source.source.slot,kSeed);};
    e::GeneratorObservation observation{kPaletteSet,kSeed,kGenerator,kWorkerOffset,0x3001,0,0,0,0,false};
    CHECK(!drained()); // Absence of a native observation is not retirement.
    CHECK(!mailbox.observe_generator(observation,epoch-1));CHECK(!drained());
    observation.enabled=true;
    CHECK(mailbox.observe_generator(observation,epoch));CHECK(!drained());
    observation.enabled=false;observation.state=6;
    CHECK(mailbox.observe_generator(observation,epoch));CHECK(!drained());
    observation.state=0;observation.entries=1;
    CHECK(mailbox.observe_generator(observation,epoch));CHECK(!drained());
    observation.entries=0;observation.gateways=1;
    CHECK(mailbox.observe_generator(observation,epoch));CHECK(!drained());
    observation.gateways=0;observation.areas=1;
    CHECK(mailbox.observe_generator(observation,epoch));CHECK(!drained());
    observation.areas=0;
    CHECK(mailbox.observe_generator(observation,epoch));CHECK(drained());
    auto wrong=observation;wrong.seed++;
    CHECK(!mailbox.observe_generator(wrong,epoch));
    CHECK(!mailbox.generator_drained(generated.activity,generated.source.run+1,
        generated.source.source.registry,generated.source.source.slot,kSeed));
    CHECK(mailbox.submit(generatedAdmission,epoch));CHECK(mailbox.submit(fixedAdmission,mailbox.capture(fixedAdmission.lease)));
    auto rebound=generated;rebound.source.generation=78;
    const e::GeneratorBinding next{rebound,kSeed+1,kPaletteSet,kGenerator,kWorkerOffset,palettes};
    auto stale=next;stale.lease.source.generation=generated.source.generation;
    CHECK(!mailbox.rebind_generator(generated,stale));
    auto sameSeed=next;sameSeed.lease.source.generation=79;sameSeed.seed=kSeed;
    CHECK(!mailbox.rebind_generator(generated,sameSeed));
    CHECK(mailbox.rebind_generator(generated,next));
    CHECK(!mailbox.generator_drained(rebound.activity,rebound.source.run,
        rebound.source.source.registry,rebound.source.source.slot,kSeed+1));
    const auto reboundEpoch=mailbox.generator_epoch();CHECK(reboundEpoch!=epoch);
    CHECK(!mailbox.submit(generatedAdmission,epoch));
    CHECK(!mailbox.rebind_generator(generated,next));
    CHECK(mailbox.bind_generator(next));CHECK(mailbox.generator_epoch()==reboundEpoch);
    std::array<e::Event,4> output{};CHECK(mailbox.drain(fixed.activity,output)==1);
    CHECK(output[0].lease==fixed);

    e::Mailbox ambiguous;
    const auto other=lease(7,1,9,93,37,kGenerator,kRegistry);
    CHECK(ambiguous.bind_generator(binding));
    CHECK(ambiguous.bind_generator({other,kSeed,kPaletteSet,kGenerator,kWorkerOffset,palettes}));
    CHECK(ambiguous.bind_generator({lease(8,1,9,94,37,kGenerator,kRegistry),kSeed,kPaletteSet,kGenerator,kWorkerOffset,palettes}));
    CHECK(ambiguous.bind_generator({lease(9,1,9,95,37,kGenerator,kRegistry),kSeed,kPaletteSet,kGenerator,kWorkerOffset,palettes}));
    CHECK(!ambiguous.bind_generator({lease(10,1,9,96,37,kGenerator,kRegistry),kSeed,kPaletteSet,kGenerator,kWorkerOffset,palettes}));
    CHECK(!ambiguous.lookup_generated(kPaletteSet,kSeed,kGenerator,kWorkerOffset,kPalette,kPaletteOffset).activity);

    e::Mailbox lifecycle;CHECK(lifecycle.bind_generator(next));
    const auto actor=e::Event{rebound,{rebound.source,11,12},kSourceHandle,e::Kind::admitted};
    auto death=actor;death.kind=e::Kind::died;
    auto retired=actor;retired.kind=e::Kind::retired;
    CHECK(lifecycle.submit(actor,lifecycle.generator_epoch()));CHECK(lifecycle.submit(death,lifecycle.generator_epoch()));
    CHECK(lifecycle.submit(retired,lifecycle.generator_epoch()));CHECK(lifecycle.drain(rebound.activity,output)==3);
    c::NativePopulationLedger<4> ledger;CHECK(ledger.begin(rebound.source));
    CHECK(ledger.admitted(output[0].actor)==c::PopulationIntake::accepted);
    CHECK(ledger.died(output[1].actor)==c::PopulationIntake::accepted);
    CHECK(ledger.counts().dead==1 && ledger.counts().resident==1);
    CHECK(ledger.actor_retired(output[2].actor)==c::PopulationIntake::accepted);
    CHECK(ledger.counts().dead==1 && ledger.counts().resident==0);

    CHECK(mailbox.submit(fixedAdmission,mailbox.capture(fixedAdmission.lease)));CHECK(mailbox.pending(fixed.activity));mailbox.release(fixed.activity);
    CHECK(mailbox.generator_epoch()==0 && !mailbox.has_lease(fixed) && !mailbox.has_lease(rebound));
    CHECK(mailbox.drain(fixed.activity,output)==0);
    std::printf("native_generated_population_tests: %u checks passed\n",checks);
}
