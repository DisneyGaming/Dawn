#include "middleware/bap/activity_message/native/world_sequence_authority.h"
#include "server/runtime/activity/mercury_public_event_sequences.h"
#include "middleware/encoding/bit_writer.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <vector>
#ifdef SEQUENCE_ROUTING_TEST
#include "core/logging/log.h"
namespace dawn::core::log {void write(Channel,Level,std::string_view) noexcept {}}
#endif
namespace wire=dawn::middleware::bap::activity_message::native::world_sequence;
namespace bits=dawn::middleware::encoding::bits;
namespace sequence=dawn::server::runtime::activity::public_event::sequence;
namespace data=dawn::server::runtime::activity::mercury::public_events;
unsigned checks{};void check(bool value,const char* message){++checks;if(!value){std::fprintf(stderr,"FAIL %u: %s\n",checks,message);std::exit(1);}}
struct Group {std::uint32_t key;std::span<const std::uint8_t> slotTypes,slotFlags;std::span<const std::uint16_t> slotIndices;};
struct Block {std::uint32_t bubble;std::span<const std::uint32_t> keys;std::span<const std::uint8_t> presence,states;};
struct Roster {std::array<Group,2> groups{};std::size_t groupCount{},topLevelGroupCount{};std::vector<Block> bubbleSubBlocks;};
int main(int argc,char** argv) {
    if(argc!=2)return 2;const std::filesystem::path root(argv[1]);std::filesystem::create_directories(root);
    wire::Request initial{0xC8229B2B,88,15,1,6732000,UINT64_MAX};
    for(const auto generation:{0U,1U,254U,255U})for(const auto time:{0ULL,6732000ULL,100000000000ULL}) {
        auto r=initial;r.generation=static_cast<std::uint8_t>(generation);r.startTicks=time;
        std::array<std::byte,(wire::kPayloadBits+7)/8> bytes{};bits::Writer writer(bytes);
        check(wire::write(writer,r) && writer.bit_count()==wire::kPayloadBits,"actual sequence payload width");
        const auto name="g"+std::to_string(generation)+"-t"+std::to_string(time);
        std::ofstream body(root/(name+".body"),std::ios::binary);body.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
        const auto expected=wire::decoded(r);std::ofstream decoded(root/(name+".expected"),std::ios::binary);decoded.write(reinterpret_cast<const char*>(expected.data()),expected.size());
        check(body.good() && decoded.good(),"native decoder fixtures exported");
#ifdef SEQUENCE_ROUTING_TEST
        namespace sensor=dawn::middleware::bap::activity_message::sensor_auth_update;
        sensor::Snapshot snapshot;snapshot.sequences.entries[0]=r;snapshot.sequences.count=1;
        for(const bool legacy:{false,true}) {
            std::array<std::byte,(wire::kPayloadBits+7)/8> routed{};bits::Writer route(routed);
            const auto width=legacy?sensor::legacy_auth_body_bits(snapshot,r.registry,5,r.slot,false):sensor::auth_body_bits(snapshot,r.registry,5,r.slot,false);
            const bool result=legacy?sensor::legacy_write_auth_body(route,snapshot,r.registry,5,r.slot,false):sensor::write_auth_body(route,snapshot,r.registry,5,r.slot,false);
            check(result && width==wire::kPayloadBits && route.bit_count()==width && routed==bytes,"both actual routers retain exact original-decoded sequence payload");
        }
#endif
    }
    auto bad=initial;bad.registry=0;check(!wire::valid(bad),"invalid registry");bad=initial;bad.bubble=64;check(!wire::valid(bad),"invalid bubble");
    bad=initial;bad.startTicks=UINT64_MAX;check(!wire::valid(bad),"invalid native time");bad=initial;bad.endTicks=bad.startTicks-1;check(!wire::valid(bad),"invalid native interval");
    std::array<std::uint8_t,1> types{5},flags{2},presence{1},states{0x80};std::array<std::uint16_t,1> slots{88};std::array<std::uint32_t,1> keys{initial.registry};
    Roster roster;roster.groupCount=1;roster.groups[0]={initial.registry,types,flags,slots};roster.bubbleSubBlocks.push_back({15,keys,presence,states});
    wire::Batch batch;batch.count=1;batch.entries[0]=initial;
    check(wire::valid(batch,roster,120),"native sequence exact admission");
    check(wire::valid(batch,roster,128),"retained exact admission survives region movement");
    presence[0]=0;check(!wire::valid(batch,roster,120),"withdrawn native admission cannot publish");presence[0]=1;
    types[0]=4;check(!wire::valid(batch,roster,120),"placement cannot impersonate sequence");types[0]=5;
    flags[0]=1;check(!wire::valid(batch,roster,120),"sense-only cannot accept sequence authority");flags[0]=2;
    batch.entries[1]=initial;batch.count=2;check(!wire::valid(batch,roster,120),"duplicate authority cannot publish");
    check(sequence::Runtime::valid(data::kIncomingSequence),"actual intro sequence has bounded no-parameter native definition");
    auto definition=data::kIncomingSequence;definition.parameterRows=1;
    check(!sequence::Runtime::valid(definition),"unsupported parameters cannot use empty authority");
    definition=data::kIncomingSequence;definition.slot=89;
    check(!sequence::Runtime::valid(definition),"different sequence slot requires its own exact graph");
    sequence::Context context{{123,{7}},11,12,13,14,6732000,29,15,true,true};sequence::Runtime runtime;
    auto foreign=context;foreign.bubble=16;
    check(!runtime.begin(data::kIncomingSequence,foreign),"wrong bubble cannot start sequence");
    foreign=context;foreign.clockTicks=UINT64_MAX;
    check(!runtime.begin(data::kIncomingSequence,foreign),"missing native clock cannot start sequence");
    check(runtime.begin(data::kIncomingSequence,context),"event-qualified sequence begin");
    check(!runtime.begin(data::kIncomingSequence,context),"duplicate begin cannot restart generation");
    batch={};check(runtime.append(batch) && !batch.count,"begin alone does not publish sequence");
    foreign=context;foreign.owner.incarnation.value++;
    check(!runtime.update(foreign) && !runtime.requested(),"stale owner cannot publish sequence");
    foreign=context;foreign.bubble=16;
    check(runtime.update(foreign) && !runtime.requested(),"region exit defers first request");
    check(runtime.update(context) && runtime.requested(),"UE emits native sequence authority");
    check(runtime.append(batch) && batch.count==1 && batch.entries[0]==initial,"exact native source clock and generation emitted");
    auto duplicate=batch;check(!runtime.append(duplicate) && duplicate.count==1,"duplicate projection leaves existing authority intact");
    check(runtime.update(context),"completed requested graph remains stable");
    wire::Batch repeated;check(runtime.append(repeated) && repeated.entries[0]==initial,"repeat update does not restart native entity");
    foreign=context;foreign.clockTicks--;
    check(!runtime.withdraw(foreign) && !runtime.stopped(),"withdrawal rejects regressing native clock");
    foreign=context;foreign.event++;
    check(!runtime.withdraw(foreign),"another event cannot stop source");
    context.clockTicks+=673200;
    check(runtime.withdraw(context) && runtime.stopped(),"exact owner can withdraw source");
    batch={};check(runtime.append(batch) && batch.entries[0].generation==255 && batch.entries[0].endTicks==context.clockTicks,"withdrawal uses native FF generation");
    const auto stop=batch.entries[0];context.clockTicks+=673200;
    check(runtime.withdraw(context),"duplicate withdrawal succeeds idempotently");
    repeated={};check(runtime.append(repeated) && repeated.entries[0]==stop,"first withdrawal record retained");
    std::printf("PASS %u native sequence checks; original decoder fixtures exported\n",checks);
}
