#include "../src/server/runtime/activity/native_activity_profiles.h"
#include "../src/server/runtime/activity/persistent_activity.h"
#include "../src/middleware/content/packages/tables/slot_descriptor_reader.h"
#include "../src/middleware/encoding/bit_reader.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <filesystem>
#include <vector>
#include "mission_parameter_fixture.h"

namespace a=sunrise::server::runtime::activity;
namespace hf=a::haunted_forest::mode;
namespace c=sunrise::state::activity::coo;
using Object=a::ambient_population::sense::SenseObject;
static unsigned checks{};
#define CHECK(x) do {++checks;if(!(x)){std::printf("FAILED %d: %s\n",__LINE__,#x);std::exit(1);}}while(false)
Object report(std::uint32_t revision,int count=1,std::int32_t token=0) {
    const auto countCode=static_cast<std::uint32_t>(static_cast<std::int64_t>(count)+2147483648LL);
    const auto tokenCode=static_cast<std::uint32_t>(static_cast<std::int64_t>(token)+2147483648LL);
    Object value{};value.registryKey=0x34D23982;value.slotIndex=109;value.slotType=30;
    value.hasNativeSchema=true;value.nativeSchema=0x80809531;value.nativeRevision=revision;
    value.hasRootDelta=true;value.bodyBits=99;
    value.bodyFirst=(std::uint64_t{1}<<63)|(std::uint64_t(count!=0)<<62)
        |(std::uint64_t(countCode)<<29)|(tokenCode>>3);
    value.bodySecond=(std::uint64_t(tokenCode&7U)<<32)|revision;return value;
}
std::shared_ptr<const c::script::MissionDocument> load(const char* file,const c::script::Profile& profile) {
    std::ifstream input(file,std::ios::binary);CHECK(input.good());
    const std::string text((std::istreambuf_iterator<char>(input)),{});std::string error;
    auto document=c::script::MissionDocument::parse(text,profile,error);
    if(!document)std::printf("parse: %s\n",error.c_str());CHECK(document);return document;
}
// Fabricated UNIT INPUT, passed through the same qualification and mailbox as
// the native observer. These bytes are never emitted to the running game.
bool unit_capture_completed(const a::capture_feedback::Ticket& t) {
    namespace f=a::capture_feedback;
    std::array<std::byte,0x448> source{};std::array<std::byte,0x70> definition{};
    std::array<std::byte,0xC0> entity{};std::array<std::byte,0x10> scene{};
    std::array<std::byte,0x1D0> before{},after{};std::array<std::byte,0x80> context{};
    auto put=[](auto& bytes,std::size_t at,auto value){std::memcpy(bytes.data()+at,&value,sizeof value);};
    put(source,0,t.source.definition);put(source,4,0x80809928U);put(source,8,t.sourceDefinitionOffset);
    put(source,0x20,1U);put(source,0x160,2U);put(source,0x164,0x80809927U);
    put(source,0x180,t.generation);put(source,0x188,std::uint8_t{1});put(source,0x2F0,t.generation);
    put(source,0x440,8U);put(source,0x444,3U);
    put(definition,0,t.source.definition);put(definition,4,0x80809927U);put(definition,0x30,t.source.registry);
    put(definition,0x34,std::uint16_t{4});put(definition,0x36,t.source.slot);put(definition,0x38,std::uint32_t{t.domain.bubble});
    put(definition,0x48,0x8080992FU);put(entity,0,8U);put(entity,0xC,3U);put(entity,0x4C,4U);
    put(scene,4,t.entityDefinition);put(before,0,t.controllerDefinition);put(before,4,0x80804FCBU);
    put(before,8,t.controllerDefinitionOffset);put(before,0x24,5U);put(before,0x2C,3U);
    const auto& clock=t.requested.clock;
    put(before,0x30,std::uint8_t{t.requested.active});put(before,0x38,std::uint8_t{clock.running});
    put(before,0x40,clock.minimum);put(before,0x48,clock.maximum);put(before,0x50,clock.elapsed);
    put(before,0x58,clock.remaining);put(before,0x60,clock.anchor);put(before,0x68,clock.rate);
    put(before,0x70,std::uint8_t{t.requested.field2});
    after=before;put(after,0x1B8,1.0F);put(after,0x79,std::uint8_t{1});
    put(context,0x10,std::uint8_t{1});put(context,0x14,std::uint8_t{t.clockConfiguration.field0});
    put(context,0x18,t.clockConfiguration.timing);put(context,0x48,t.domain.scenario);
    return a::capture_bridge::submit({t,source,definition,entity,scene,before,after,context,1,2,3,4,5,6,6,f::kTickRva,1});
}
void placement_generation() {
    namespace wire=a::placement::wire;
    namespace bits=sunrise::middleware::encoding::bits;
    namespace interaction=sunrise::middleware::bap::activity_message::native::interaction;
    for(const auto mode:{interaction::Mode::unchanged,interaction::Mode::disabled,interaction::Mode::enabled}) {
        // Compatibility bytes remain exact, including the existing rally tuple.
        std::array<std::byte,64> legacy{},current{};bits::Writer old(legacy),now(current);
        wire::Request request{0x34D23982,32,13,mode};
        CHECK(wire::write_active(old,mode));CHECK(wire::write(now,request));
        CHECK(old.bit_count()==now.bit_count() && legacy==current);
        CHECK(now.bit_count()==wire::body_bits(request));
    }
    for(const auto generation:{1U,2U,0x7FFFFFFFU}) {
        wire::Request request{0x34D23982,32,13,interaction::Mode::unchanged,generation};
        std::array<std::byte,64> body{};bits::Writer writer(body);CHECK(wire::write(writer,request));
        CHECK(writer.bit_count()==252 && wire::body_bits(request)==252);
        bits::Reader reader(body);std::uint64_t value{};
        CHECK(reader.read(32,value));
        // Decode the reflected signed field with its recovered INT_MIN bias.
        const auto applied=static_cast<std::int64_t>(value)-2147483648LL;
        CHECK(applied==generation && applied>0);
        CHECK(reader.read(32,value) && value==0x80000000U);
        CHECK(reader.read(1,value) && value==1); // Native source active.
        CHECK(reader.read(1,value) && value==0); // Authored candidate transform.
        CHECK(reader.read(32,value) && value==0x80000000U);
        CHECK(reader.read(32,value) && value==0x811C9DC5U);
        CHECK(reader.read(7,value) && value==0);
        CHECK(reader.read(16,value) && value==32767);
        CHECK(reader.read(32,value) && value==0);
        CHECK(reader.read(32,value) && value==0);
        CHECK(reader.read(32,value) && value==0);
        CHECK(reader.read(1,value) && value==0);
        CHECK(reader.read(2,value) && value==0); // Empty native override list.
        // After native creation commits G, repeated G no longer requests creation.
        CHECK(!(generation<applied));
        request.interactionMode=interaction::Mode::enabled;
        auto measure=bits::Writer::measuring();CHECK(wire::write(measure,request));
        CHECK(measure.bit_count()==wire::body_bits(request) && measure.bit_count()==375);
    }
    for(const auto invalid:{0x80000000U,UINT32_MAX}) {
        wire::Request request{0x34D23982,32,13,interaction::Mode::unchanged,invalid};
        auto writer=bits::Writer::measuring();CHECK(!wire::write(writer,request));
        CHECK(wire::body_bits(request)==0 && writer.bit_count()==0);
        a::placement::Capability cap{&hf::kRegistries[0],32,invalid};wire::Batch batch;
        CHECK(!a::placement::project({&cap,1},13,batch));CHECK(batch.count==0);
    }
}
int main(int argc,char** argv) {
    placement_generation();
    if(argc==2) {
        namespace tables=sunrise::middleware::content::packages::tables;
        for(const auto& slot:hf::kRegistries[0].slots) {
            char name[16]{};std::snprintf(name,sizeof(name),"%08X.bin",slot.descriptorTag);
            std::ifstream input(std::filesystem::path(argv[1])/name,std::ios::binary);CHECK(input.good());
            const std::vector<char> raw((std::istreambuf_iterator<char>(input)),{});
            const auto blob=std::as_bytes(std::span(raw));
            struct Expected {const a::registry::Slot* slot{};unsigned matches{};} expected{&slot};
            const auto visitor=[](void* context,const tables::SlotDescriptor& d) noexcept {
                auto& e=*static_cast<Expected*>(context);const auto& s=*e.slot;
                if(d.slotIndex!=s.index || d.slotType!=s.type || d.sourceTag!=s.descriptorTag
                    || d.componentClass!=s.componentClass || d.senseSchema!=s.senseSchema || d.authSchema!=s.authSchema)return false;
                ++e.matches;return true;
            };
            CHECK(tables::visit_slot_descriptors(blob,slot.descriptorTag,0x34D23982,visitor,&expected));
            CHECK(expected.matches==1);
        }
    }
    CHECK(a::native_activity_profile("infinite_abyss",78)==&hf::kActivity);
    CHECK(!a::native_activity_profile("infinite_abyss",79));
    CHECK(!a::native_activity_profile("mission_abyss_intro",80));
    CHECK(!a::native_activity_profile("infinite_abyss"));
    CHECK(a::native_activity_profile("mercury_freeroam")==&a::mercury::kActivity);
    CHECK(a::registry::valid(hf::kRegistries[0]));CHECK(hf::kRegistries[0].slots.size()==126);

    const a::population::Owner owner{42,{7}};
    a::population::Service empty;
    CHECK(!empty.begin({},{}));CHECK(!empty.begin(owner,{},0));
    CHECK(empty.begin(owner,{},123));CHECK(empty.owner()==owner && empty.revision()==1);
    CHECK(empty.project(13).count==0);CHECK(!empty.begin(owner,{},123));
    CHECK(empty.request({owner,1,1,0x34D23982,59,1,123},13)==a::population::Result::unsupported);
    CHECK(empty.last_request()==0 && empty.revision()==1);
    empty={};CHECK(!empty.owner());CHECK(empty.begin({42,{8}},{},124));

    a::occupancy_wait::Service wait;
    CHECK(wait.begin(owner,123,hf::kOccupancy));CHECK(!wait.begin(owner,123,hf::kOccupancy));
    unsigned delivered{};c::Event receipt{};
    auto submit=[&](c::Event event){++delivered;receipt=event;return true;};
    CHECK(!wait.observe(owner,123,13,report(5),submit)); // Pre-arm occupancy is only a mirror.
    CHECK(!wait.arm(0,{8,1,1,0}));CHECK(wait.arm(0,{42,1,1,0}));
    CHECK(!wait.arm(0,{42,1,1,0}));
    CHECK(!wait.observe(owner,123,13,report(5),submit)); // Replay after arming.
    CHECK(!wait.observe(owner,123,13,report(4),submit));
    CHECK(!wait.observe({42,{8}},123,13,report(6),submit));
    CHECK(!wait.observe({43,{7}},123,13,report(6),submit));
    CHECK(!wait.observe(owner,124,13,report(6),submit));
    CHECK(!wait.observe(owner,123,12,report(6),submit));
    auto bad=report(6);bad.registryKey++;CHECK(!wait.observe(owner,123,13,bad,submit));
    bad=report(6);bad.slotIndex++;CHECK(!wait.observe(owner,123,13,bad,submit));
    CHECK(!wait.observe(owner,123,13,report(6,1,1),submit));
    bad=report(6);bad.inferredBodyWidth=true;CHECK(!wait.observe(owner,123,13,bad,submit));
    CHECK(!wait.observe(owner,123,13,report(6,0),submit));
    CHECK(wait.observe(owner,123,13,report(7),submit));CHECK(delivered==1);
    CHECK((receipt.token==c::Token{42,1,1,0}));CHECK(receipt.milestone==c::Milestone::observed);
    CHECK(!wait.observe(owner,123,13,report(8),submit));CHECK(!wait.arm(0,{42,2,1,0}));

    const auto doc=load("Sunrise/scripts/infinite_abyss.json",hf::kProfile);
    CHECK(a::PersistentActivity::valid(hf::kActivity,*doc));
    auto invalidDefinition=hf::kActivity;auto invalidGenerator=hf::kGenerators;
    invalidDefinition.generators=invalidGenerator;invalidGenerator[0].slot=97;
    CHECK(!a::PersistentActivity::valid(invalidDefinition,*doc));
    invalidDefinition=hf::kActivity;auto invalidDevice=hf::kDevices;
    invalidDefinition.devices=invalidDevice;invalidDevice[0].slot=32;
    CHECK(!a::PersistentActivity::valid(invalidDefinition,*doc));
    // The branch now uses the shared repeated-round graph rather than the old
    // one-shot nativeActivity graph. Exercise entry and the real capture boundary.
    a::PersistentActivity activity;CHECK(activity.begin(owner,hf::kActivity,doc,123));
    CHECK(activity.update(13,false).placements.count==0);
    a::NativeActivityFrame frame;
    a::activity_clock::Service clock;
    CHECK(clock.begin(owner,123,1,{0x81550015,13,{false,1000.0F/30.0F}},1000));
    a::activity_clock::Publication publication;
    CHECK(clock.project(owner,123,13,1000,publication));
    auto wrongClock=publication;wrongClock.domain.owner.incarnation.value++;
    CHECK(activity.update(13,true,{},true,wrongClock).placements.count==0);
    wrongClock=publication;wrongClock.domain.boot++;
    CHECK(activity.update(13,true,{},true,wrongClock).placements.count==0);
    for(int i=0;i<8;++i)frame=activity.update(13,true,{},true,publication);
    CHECK(!activity.round().failed());
    CHECK(activity.round().round_snapshot().phase==a::timed_round::Phase::entry);
    CHECK(frame.placements.count==3 && frame.generators.count==0);
    CHECK(activity.observe_occupancy(owner,123,13,report(11)));
    CHECK(clock.project(owner,123,13,4000,publication));
    for(int i=0;i<8;++i)frame=activity.update(13,true,{},true,publication);
    CHECK(activity.capture().state(0).requested && !activity.capture().state(0).completed);
    CHECK(clock.project(owner,123,13,60000,publication));
    for(int i=0;i<8;++i)frame=activity.update(13,true,{},true,publication);
    CHECK(activity.round().round_snapshot().phase==a::timed_round::Phase::entry);
    CHECK(!activity.capture().state(0).completed && frame.generators.count==0);
    CHECK(unit_capture_completed(activity.capture().state(0).ticket));
    for(int i=0;i<8;++i)frame=activity.update(13,true,{},true,publication);
    CHECK(!activity.round().failed());
    CHECK(activity.round().round_snapshot().phase==a::timed_round::Phase::traversal);
    CHECK(frame.generators.count==1 && frame.cues.count==1);
    CHECK(frame.cues.entries[0].hasTimer && frame.cues.entries[0].timer.advancing);
    CHECK(frame.generators.entries[0].registry==0x34D23982 && frame.generators.entries[0].slot==98);
    const auto generation=frame.generators.entries[0];
    CHECK(generation.state.primary.seed!=0);
    for(int i=0;i<10;++i) {
        frame=activity.update(13,true,{},true,publication);
        CHECK(frame.generators.count==1 && frame.generators.entries[0].state==generation.state);
        CHECK(activity.round().round_snapshot().phase==a::timed_round::Phase::traversal);
    }
    // Repeated rounds require fresh tokens and reject receipts from prior trips.
    a::timed_round::Service rounds;
    using Result=a::timed_round::Result;using Phase=a::timed_round::Phase;
    CHECK(rounds.begin(owner,123,{100000,10,a::timed_round::ActiveCombatPhases}));
    std::uint64_t now{};
    for(std::uint64_t round=1;round<=12;++round) {
        const auto entry=rounds.snapshot().token;
        CHECK(rounds.capture_complete(entry,++now)==Result::accepted);
        CHECK(rounds.capture_complete(entry,now)==Result::stale);
        const auto traversal=rounds.snapshot().token;
        CHECK(rounds.add_progress(traversal,++now,round*2,5)==Result::accepted);
        CHECK(rounds.add_progress(traversal,now,round*2,5)==Result::duplicate);
        CHECK(rounds.add_progress(traversal,++now,round*2+1,5)==Result::accepted);
        CHECK(rounds.snapshot().phase==Phase::toEncounter);
        CHECK(rounds.encounter_arrived(rounds.snapshot().token,++now)==Result::accepted);
        CHECK(rounds.encounter_defeated(rounds.snapshot().token,++now,round)==Result::accepted);
        CHECK(rounds.returned(rounds.snapshot().token,++now)==Result::accepted);
        CHECK(rounds.snapshot().completedRounds==round && rounds.snapshot().progress==0);
    }
    CHECK(rounds.capture_complete(rounds.snapshot().token,++now)==Result::accepted);
    CHECK(rounds.all_players_defeated(rounds.snapshot().token,now+100000,1)==Result::accepted);
    CHECK(rounds.snapshot().phase==Phase::rewards);
    CHECK(rounds.rewards_finished(rounds.snapshot().token,now+100000)==Result::accepted);
    CHECK(rounds.snapshot().phase==Phase::complete);

    // No bindings leaves established Mercury behavior unchanged.
    std::ifstream mercuryInput("Sunrise/scripts/mercury_freeroam.json",std::ios::binary);CHECK(mercuryInput.good());
    std::string mercuryText((std::istreambuf_iterator<char>(mercuryInput)),{});
    CHECK(mission_parameter_fixture::numeric(mercuryText,"public_event_rally_probe",0));
    CHECK(mission_parameter_fixture::numeric(mercuryText,"ambient_vex_probe_count",0));
    std::string mercuryError;auto parsedMercury=c::script::MissionDocument::parse(mercuryText,a::mercury::kProfile,mercuryError);CHECK(parsedMercury);
    std::shared_ptr<const c::script::MissionDocument> mercury=std::move(parsedMercury);
    CHECK(a::PersistentActivity::valid(a::mercury::kActivity,*mercury));
    a::PersistentActivity prior;CHECK(prior.begin({99,{1}},a::mercury::kActivity,mercury,999));
    for(int i=0;i<4;++i)frame=prior.update(15,true);
    CHECK(frame.placements.count==9 && frame.populations.count==2);
    for(std::size_t i=0;i<frame.placements.count;++i)CHECK(frame.placements.entries[i].generation==0);
    CHECK(!frame.clock && prior.capture().size()==0);
    CHECK(frame.generators.count==0 && prior.generator().project(15).count==0);
    CHECK(frame.cues.count==0 && frame.devices.count==0);
    for(std::size_t i=0;i<frame.placements.count;++i)CHECK(!frame.placements.entries[i].capture);
    CHECK(prior.diagnostics().phase==c::Phase::complete);
    CHECK(!prior.observe_occupancy({99,{1}},999,15,report(1)));
    a::capture_bridge::release(owner);
    std::printf("Haunted Forest native entry/capture: %u checks passed.\n",checks);
}
