#include "server/runtime/activity/mercury_public_event_presentation.h"
#include "middleware/encoding/bit_writer.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <filesystem>
#include <fstream>
namespace event=dawn::server::runtime::activity::public_event::dialogue;
namespace data=dawn::server::runtime::activity::mercury::public_events;
namespace wire=event::feedback::wire;
namespace bits=dawn::middleware::encoding::bits;
unsigned checks{};
void check(bool value,const char* name){++checks;if(!value){std::fprintf(stderr,"FAIL %u: %s\n",checks,name);std::exit(1);}}
struct Group {std::uint32_t key;std::span<const std::uint8_t> slotTypes,slotFlags;std::span<const std::uint16_t> slotIndices;};
struct Block {std::uint32_t bubble;std::span<const std::uint32_t> keys;std::span<const std::uint8_t> presence;};
struct Roster {std::array<Group,2> groups{};std::size_t groupCount{},topLevelGroupCount{};std::vector<Block> bubbleSubBlocks;};
int main(int argc,char** argv) {
    check(event::Runtime::valid(data::kIncomingVoice),"actual authored definition resolves one native dialogue slot");
    auto definition=data::kIncomingVoice;definition.slot=107;
    check(!event::Runtime::valid(definition),"directive cannot impersonate conversation");
    event::Context context{{123,{7}},11,12,13,14,29,15,true,true};event::Runtime runtime;
    auto foreign=context;foreign.bubble=16;
    check(!runtime.begin(data::kIncomingVoice,foreign),"wrong bubble cannot start");
    check(runtime.begin(data::kIncomingVoice,context),"event-qualified begin");
    check(!runtime.begin(data::kIncomingVoice,context),"duplicate start cannot reset generation");
    wire::Batch batch;check(runtime.append(batch) && !batch.count,"begin alone publishes nothing");
    foreign=context;foreign.owner.incarnation.value++;
    check(!runtime.update(foreign) && !runtime.requested(),"stale world cannot publish");
    check(runtime.update(context) && runtime.requested() && !runtime.submitted(),"UE publishes and waits for native submission");
    check(runtime.append(batch) && batch.count==1 && batch.entries[0].activeRow==0 && batch.entries[0].scope==15,"scoped authored incoming row");
    const auto request=batch.entries[0];const auto binding=event::bridge::lookup(0x80F5E33A);
    check(binding.epoch && binding.ticket==runtime.ticket(),"bridge owns exact immutable command");
    if(argc==2) {
        const auto path=std::filesystem::path(argv[1]);std::filesystem::create_directories(path);
        std::vector<std::byte> bytes((wire::body_bits(request)+7)/8);bits::Writer out(bytes);check(wire::write(out,request),"initial production fixture");
        std::ofstream file(path/"initial.body",std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
        const auto expected=wire::decoded(request);std::ofstream exp(path/"initial.expected",std::ios::binary);exp.write(reinterpret_cast<const char*>(expected.data()),expected.size());
    }
    const auto decoded=wire::decoded(request);event::feedback::Observation observation;
    event::feedback::Capture capture{runtime.ticket(),{0x12345678,0},1,decoded,decoded,0,request.generations[0],true,true};
    check(event::feedback::qualify(runtime.ticket(),capture,observation)==event::feedback::Result::accepted,"original-consumer submission qualifies");
    auto wrong=observation;wrong.ticket.owner.incarnation.value++;
    check(!event::bridge::submit({binding,wrong}),"foreign native receipt cannot acknowledge");
    check(event::bridge::submit({binding,observation}),"exact native submission accepted");
    check(!event::bridge::submit({binding,observation}),"duplicate receipt rejected");
    check(runtime.update(context) && runtime.submitted(),"UE consumes native submission");
    batch={};check(runtime.append(batch) && batch.entries[0].activeRow==wire::kNoRow
        && batch.entries[0].generations[0]==request.generations[0],"retired authority preserves generation without replay");
    foreign=context;foreign.bubble=16;check(runtime.update(foreign),"temporary region exit pauses processing");
    wire::Batch retained;check(runtime.append(retained) && retained.entries[0]==batch.entries[0],"region exit retains consumed native history");
    auto duplicate=batch;check(!runtime.append(duplicate) && duplicate.count==batch.count,"duplicate projection fails without editing batch");
    std::array<std::uint8_t,1> types{53},flags{2},presence{1};std::array<std::uint16_t,1> slots{108};
    std::array<std::uint32_t,1> keys{0xC8229B2B};Roster roster;roster.groups[0]={keys[0],types,flags,slots};roster.groupCount=1;
    roster.bubbleSubBlocks.push_back({15,keys,presence});
    check(wire::valid(batch,roster),"one exact bubble admission permitted");
    roster.bubbleSubBlocks[0].bubble=16;check(!wire::valid(batch,roster),"wrong bubble rejected");roster.bubbleSubBlocks[0].bubble=15;
    presence[0]=0;check(!wire::valid(batch,roster),"not-present admission rejected");presence[0]=1;
    roster.bubbleSubBlocks.push_back(roster.bubbleSubBlocks[0]);check(!wire::valid(batch,roster),"duplicate bubble admission rejected");roster.bubbleSubBlocks.pop_back();
    roster.topLevelGroupCount=1;check(!wire::valid(batch,roster),"scoped request cannot impersonate global admission");
    batch.entries[0].scope=UINT32_MAX;check(!wire::valid(batch,roster),"global request cannot use bubble admission");
    roster.bubbleSubBlocks.clear();check(wire::valid(batch,roster),"existing global behavior preserved");
    auto scoped=request;auto global=request;global.scope=UINT32_MAX;
    std::array<std::byte,2479> one{},two{};bits::Writer a(one),b(two);
    check(wire::write(a,scoped) && wire::write(b,global) && a.bit_count()==b.bit_count() && one==two,"scope never changes native wire bytes");
    check(wire::decoded(scoped)==wire::decoded(global),"scope never changes native decoded image");
    scoped.scope=64;check(!wire::valid(scoped),"invalid scope rejected");

    for(const auto* next:{&data::kJoinedVoice,&data::kObservedSuccessVoice}) {
        const auto previous=runtime.ticket();auto wrongContext=context;wrongContext.event++;
        check(!runtime.advance(*next,wrongContext),"another event cannot advance retained bank");
        check(runtime.advance(*next,context) && runtime.update(context),"consumed dialogue advances through a new UE command");
        const auto current=runtime.ticket();const auto row=current.authored.row;
        check(current.request.generations[row]==previous.request.generations[row]+1 && current.request.clearInactiveTimes,"new row has one generation and explicit retired times");
        for(std::size_t i=0;i<wire::kRows;++i)if(i!=row)check(current.request.generations[i]==previous.request.generations[i],"all previous row generations preserved");
        check(!runtime.advance(data::kIncomingVoice,context),"pending native row cannot be replaced");
        auto stale=current;stale.owner.incarnation.value++;check(!event::bridge::Mailbox::can_advance(previous,stale),"foreign dialogue lifetime rejected");
        stale=current;stale.request.generations[previous.authored.row]++;check(!event::bridge::Mailbox::can_advance(previous,stale),"erased or changed prior generation rejected");
        const auto bound=event::bridge::lookup(current.authored.definition);
        check(bound.epoch && !event::bridge::submit({binding,observation}),"new epoch rejects old captured submission");
        if(argc==2) {
            const auto path=std::filesystem::path(argv[1]);std::filesystem::create_directories(path);
            for(bool retired:{false,true}){
                auto projected=current.request;if(retired)projected.activeRow=wire::kNoRow;
                std::vector<std::byte> bytes((wire::body_bits(projected)+7)/8);bits::Writer output(bytes);
                check(wire::write(output,projected) && output.bit_count()==wire::body_bits(projected),"advanced native body width exact");
                const auto stem="advanced-"+std::to_string(row)+(retired?"-retired":"");
                std::ofstream file(path/(stem+".body"),std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
                const auto projectedDecoded=wire::decoded(projected);std::ofstream expected(path/(stem+".expected"),std::ios::binary);expected.write(reinterpret_cast<const char*>(projectedDecoded.data()),projectedDecoded.size());
            }
        }
        const auto state=wire::decoded(current.request);event::feedback::Observation submitted;
        const event::feedback::Capture proof{current,{0x12345678,0},20ULL+row,state,state,previous.request.generations[row],current.request.generations[row],true,true};
        check(event::feedback::qualify(current,proof,submitted)==event::feedback::Result::accepted
            && event::bridge::submit({bound,submitted}) && runtime.update(context) && runtime.submitted(),"advanced row consumes exact native submission");
    }
    event::bridge::release(context.owner);check(!event::bridge::lookup(0x80F5E33A).epoch,"owner retirement clears bridge");
    std::printf("PASS %u public-event dialogue checks\n",checks);
}
