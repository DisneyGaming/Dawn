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
    CHECK(!wait.arm(0,{8,1,1,0}));CHECK(wait.arm(0,{7,1,1,0}));
    CHECK(!wait.arm(0,{7,1,1,0}));
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
    CHECK((receipt.token==c::Token{7,1,1,0}));CHECK(receipt.milestone==c::Milestone::observed);
    CHECK(!wait.observe(owner,123,13,report(8),submit));CHECK(!wait.arm(0,{7,2,1,0}));

    const auto doc=load("Sunrise/scripts/infinite_abyss.json",hf::kProfile);
    CHECK(a::PersistentActivity::valid(hf::kActivity,*doc));
    auto invalidDefinition=hf::kActivity;auto invalidGenerator=hf::kGenerators;
    invalidDefinition.generators=invalidGenerator;invalidGenerator[0].slot=97;
    CHECK(!a::PersistentActivity::valid(invalidDefinition,*doc));
    invalidGenerator=hf::kGenerators;invalidGenerator[0].seedParameter="missing.seed";
    CHECK(!a::PersistentActivity::valid(invalidDefinition,*doc));
    invalidGenerator=hf::kGenerators;invalidGenerator[0].anchorParameters[3].height="missing.height";
    CHECK(!a::PersistentActivity::valid(invalidDefinition,*doc));
    invalidDefinition=hf::kActivity;auto invalidDevice=hf::kDevices;
    invalidDefinition.devices=invalidDevice;invalidDevice[0].slot=32;
    CHECK(!a::PersistentActivity::valid(invalidDefinition,*doc));
    a::PersistentActivity activity;CHECK(activity.begin(owner,hf::kActivity,doc,123));
    CHECK(activity.update(13,false).placements.count==0);
    CHECK(activity.update(12,true).placements.count==0);
    CHECK(!activity.observe_occupancy(owner,123,13,report(10)));
    a::NativeActivityFrame frame;
    a::activity_clock::Service clock;
    CHECK(clock.begin(owner,123,1,{0x81550015,13,{false,1000.0F/30.0F}},1000));
    a::activity_clock::Publication publication;
    CHECK(clock.project(owner,123,13,1000,publication));
    auto wrongClock=publication;wrongClock.domain.owner.incarnation.value++;
    CHECK(activity.update(13,true,{},true,wrongClock).placements.count==0);
    wrongClock=publication;wrongClock.domain.boot++;
    CHECK(activity.update(13,true,{},true,wrongClock).placements.count==0);
    wrongClock=publication;wrongClock.domain.scenario++;
    CHECK(activity.update(13,true,{},true,wrongClock).placements.count==0);
    for(int i=0;i<4;++i)frame=activity.update(13,true,{},true,publication);
    CHECK(frame.placements.count==3 && frame.populations.count==0);
    CHECK(frame.generators.count==0);
    CHECK(frame.devices.count==1 && frame.devices.entries[0].registry==0x34D23982
        && frame.devices.entries[0].slot==33 && frame.devices.entries[0].bubble==13);
    CHECK(frame.devices.entries[0].state.position.value==0.2F
        && frame.devices.entries[0].state.position.revision==1 && !frame.devices.entries[0].state.position.snap);
    CHECK(frame.devices.entries[0].state.power.revision==0 && frame.devices.entries[0].state.lock.revision==0);
    CHECK(frame.cues.count==1 && frame.cues.entries[0].event==0x000D87C7
        && frame.cues.entries[0].scope==UINT32_MAX && !frame.cues.entries[0].hasTimer);
    for(std::size_t i=0;i<frame.placements.count;++i)CHECK(frame.placements.entries[i].generation==1);
    CHECK(frame.placements.entries[2].capture && frame.placements.entries[2].capture->active);
    CHECK(!frame.placements.entries[2].capture->clock.running);
    CHECK(activity.diagnostics().phase==c::Phase::running);
    CHECK(!activity.observe_occupancy(owner,123,13,report(10)));
    CHECK(!activity.observe_occupancy(owner,123,13,report(9)));
    CHECK(!activity.observe_occupancy({42,{8}},123,13,report(11)));
    CHECK(activity.observe_occupancy(owner,123,13,report(11)));
    CHECK(clock.project(owner,123,13,4000,publication));
    for(int i=0;i<4;++i)frame=activity.update(13,true,{},true,publication);
    CHECK(frame.placements.count==3 && frame.populations.count==0);
    CHECK(activity.diagnostics().phase==c::Phase::running);
    CHECK(frame.placements.entries[2].capture->clock.running);
    CHECK(frame.devices.count==1 && frame.devices.entries[0].state.position.value==0.2F
        && frame.devices.entries[0].state.position.revision==1 && !frame.devices.entries[0].state.position.snap);
    CHECK(frame.devices.entries[0].state.power.revision==0 && frame.devices.entries[0].state.lock.revision==0);
    CHECK(frame.placements.entries[2].capture->clock.anchor==2019600);
    CHECK(activity.capture().state(0).requested && !activity.capture().state(0).ready);
    // Host time passing cannot manufacture native readiness or capture success.
    CHECK(clock.project(owner,123,13,60000,publication));
    for(int i=0;i<40;++i)CHECK(activity.update(13,true,{},true,publication).placements.count==3);
    CHECK(activity.diagnostics().phase==c::Phase::running && !activity.capture().state(0).completed);
    CHECK(activity.generator().project(13).count==0 && activity.generator().revision()==1);
    const auto waitingFrame=activity.update(13,true,{},true,publication);
    CHECK(waitingFrame.devices.entries[0].state.position.value==0.2F
        && waitingFrame.devices.entries[0].state.position.revision==1);
    CHECK(activity.presentation().current_action()==1 && !activity.presentation().presentation()->hasTimer);
    CHECK(!activity.observe_occupancy(owner,123,13,report(12)));
    CHECK(activity.update(12,true).placements.count==0);
    CHECK(activity.update(13,true,{},true,publication).placements.count==3);
    CHECK(unit_capture_completed(activity.capture().state(0).ticket));
    for(int i=0;i<4;++i)frame=activity.update(13,true,{},true,publication);
    CHECK(activity.capture().state(0).completed && activity.diagnostics().phase==c::Phase::complete);
    CHECK(frame.generators.count==1 && frame.placements.count==3 && frame.populations.count==0);
    CHECK(frame.devices.entries[0].state.position.value==0.1F
        && frame.devices.entries[0].state.position.revision==2);
    CHECK(frame.cues.count==1 && frame.cues.entries[0].event==0x6A3CC92F
        && frame.cues.entries[0].variant==1 && frame.cues.entries[0].ring==1);
    CHECK(frame.cues.entries[0].hasTimer && frame.cues.entries[0].timer.advancing);
    CHECK(frame.cues.entries[0].timer.remaining==900ULL*673200
        && frame.cues.entries[0].timer.anchor==publication.elapsedTicks);
    const auto retainedCue=frame.cues.entries[0];
    const auto generation=frame.generators.entries[0];
    CHECK(generation.registry==0x34D23982 && generation.slot==98 && generation.bubble==13);
    auto expected=hf::kGeneratorIgnition;expected.primary.seed=generation.state.primary.seed;
    expected.primary.overrides|=a::forest_generator::wire::Seed;
    CHECK(generation.state==expected && generation.state.primary.seed!=0);
    const auto& anchors=generation.state.primary.anchors;
    CHECK((anchors[0]==a::forest_generator::wire::Anchor{3,2,0,true}));
    CHECK((anchors[1]==a::forest_generator::wire::Anchor{1,0,0,true}));
    CHECK((anchors[2]==a::forest_generator::wire::Anchor{1,1,0,true}));
    CHECK((anchors[3]==a::forest_generator::wire::Anchor{2,2,0,true}));
    for(int i=0;i<10;++i) {
        frame=activity.update(13,true,{},true,publication);
        CHECK(frame.generators.count==1 && frame.generators.entries[0].state==generation.state);
        CHECK(frame.cues.count==1 && frame.cues.entries[0]==retainedCue);
        CHECK(frame.devices.count==1 && frame.devices.entries[0].state.position.revision==2
            && frame.devices.entries[0].state.position.value==0.1F);
    }
    CHECK(activity.generator().revision()==2 && activity.generator().last_request()==1);

    // No bindings leaves established Mercury behavior unchanged.
    const auto mercury=load("Sunrise/scripts/mercury_freeroam.json",a::mercury::kProfile);
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
