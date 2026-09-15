#include "middleware/bap/activity_message/native/status_effect_authority.h"
#include "server/runtime/activity/status_effect_service.h"
#include "server/runtime/activity/transit_effect_service.h"
#include "middleware/encoding/bit_reader.h"
#include "middleware/encoding/bit_writer.h"
#include "middleware/bap/activity_message/sensor_auth_update.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <span>

namespace status=sunrise::server::runtime::activity::status_effect;
namespace wire=sunrise::middleware::bap::activity_message::native::status_effect;
namespace bits=sunrise::middleware::encoding::bits;
namespace sensor=sunrise::middleware::bap::activity_message::sensor_auth_update;
namespace placement=sunrise::middleware::bap::activity_message::native::placement;
namespace registry=sunrise::state::activity::coo::registry;
using Owner=sunrise::state::activity::ActivityInstanceKey;

constexpr Owner kOwner{41,{7}};
constexpr std::uint64_t kBoot=19;
constexpr std::uint32_t kRegistry=0x12345678U;
constexpr std::uint16_t kSlot=23;
constexpr std::uint8_t kBubble=2;
unsigned checks{};

void expect(bool value) {
    ++checks;
    if(!value) { std::fprintf(stderr,"failed check %u\n",checks); std::exit(1); }
}

struct Authored final {
    std::array<registry::Slot,1> slots{{
        {kSlot,wire::kType,wire::kComponentClass,0x12340001U,wire::kSchema,0x12340002U}}};
    registry::Definition definition{
        "synthetic_status_effect",1,kRegistry,0x12345679U,0x1234567AU,kBubble,slots};
};

status::Service make_service() {
    static Authored authored;
    const std::array<status::Capability,1> capabilities{{{&authored.definition,kSlot}}};
    status::Service result;
    expect(result.begin(kOwner,kBoot,capabilities));
    return result;
}

std::uint64_t read(bits::Reader& reader,std::uint8_t width) {
    std::uint64_t value{};expect(reader.read(width,value));return value;
}

void codec_decodes_all_players_and_disabled_record_length() {
    std::array<std::byte,32> activeBytes{};bits::Writer activeWriter(activeBytes);
    expect(wire::write_authority(activeWriter,true));
    expect(activeWriter.bit_count()==wire::kActiveBits);
    bits::Reader activeReader(activeBytes);
    expect(read(activeReader,1)==0 && read(activeReader,1)==0);
    for(std::size_t channel=0;channel<wire::kBiasedZeroChannelCount;++channel)
        expect(read(activeReader,32)==0x80000000U);
    expect(read(activeReader,32)==0x811C9DC5U && read(activeReader,7)==0
        && read(activeReader,16)==0x7FFFU && read(activeReader,1)==1);
    expect(read(activeReader,32)==wire::kPlayerSelector && read(activeReader,1)==1
        && read(activeReader,32)==wire::kAllPlayersSelector);
    expect(activeReader.remaining_bits()==5);

    std::array<std::byte,24> disabledBytes{};bits::Writer disabledWriter(disabledBytes);
    expect(wire::write_authority(disabledWriter,false));
    expect(disabledWriter.bit_count()==wire::kInactiveBits);
    bits::Reader disabledReader(disabledBytes);
    expect(read(disabledReader,1)==0 && read(disabledReader,1)==1);
    for(std::size_t channel=0;channel<wire::kBiasedZeroChannelCount;++channel)
        expect(read(disabledReader,32)==0x80000000U);
    expect(read(disabledReader,32)==0x811C9DC5U && read(disabledReader,7)==0
        && read(disabledReader,16)==0x7FFFU && read(disabledReader,1)==0);
    expect(disabledReader.remaining_bits()==6);
}

void invalid_registry_and_type_proof_are_rejected() {
    Authored authored;
    expect(status::valid({&authored.definition,kSlot}));
    const registry::Slot wrongType{kSlot,25,wire::kComponentClass,0x12340001U,wire::kSchema,1};
    authored.slots[0]=wrongType;
    expect(!status::valid({&authored.definition,kSlot}));
    authored.slots[0]={kSlot,wire::kType,0x12340003U,0x12340001U,wire::kSchema,1};
    expect(!status::valid({&authored.definition,kSlot}));

    wire::Batch batch{};batch.entries[0]={kRegistry,kSlot,kBubble,true};batch.count=1;
    expect(wire::find(batch,kRegistry,wire::kType,kSlot)!=nullptr);
    expect(wire::find(batch,kRegistry,25,kSlot)==nullptr);
    expect(wire::find(batch,0x87654321U,wire::kType,kSlot)==nullptr);
}

void native_batch_validates_admitted_type26_scope() {
    std::array<std::uint32_t,1> keys{{kRegistry}};
    std::array<std::uint8_t,1> types{{wire::kType}},flags{{3}};
    std::array<std::uint16_t,1> indices{{kSlot}};
    sensor::Group group{kRegistry,types,flags,indices};
    sensor::BubbleSubBlock block{kBubble,keys,{},{}};
    sensor::Roster roster{};roster.groups[0]=group;roster.groupCount=1;
    roster.topLevelGroupCount=1;roster.bubbleSubBlocks=std::span(&block,1);
    wire::Batch batch{};batch.entries[0]={kRegistry,kSlot,kBubble,true};batch.count=1;
    expect(wire::valid(batch,roster,kBubble*8U));
    batch.entries[0].registry=0x87654321U;
    expect(!wire::valid(batch,roster,kBubble*8U));
}

void requests_are_monotonic_and_disable_preserves_native_defaults() {
    auto service=make_service();
    const auto initial=service.snapshot();
    expect(initial.revision==1 && service.project(kBubble).count==0);
    expect(service.request({kOwner,kBoot,initial.revision,1,0,true})==status::Result::accepted);
    const auto enabled=service.snapshot();expect(enabled.revision==2);
    auto projected=service.project(kBubble);
    expect(projected.count==1 && projected.entries[0].enabled);

    const auto beforeDuplicate=service.snapshot();
    expect(service.request({kOwner,kBoot,beforeDuplicate.revision,1,0,true})==status::Result::duplicate);
    expect(service.snapshot().revision==beforeDuplicate.revision
        && service.snapshot().lastRequest==beforeDuplicate.lastRequest);
    const auto beforeStale=service.snapshot();
    expect(service.request({kOwner,kBoot,initial.revision,2,0,false})==status::Result::stale);
    expect(service.snapshot().revision==beforeStale.revision
        && service.snapshot().lastRequest==beforeStale.lastRequest);

    expect(service.request({kOwner,kBoot,enabled.revision,2,0,true})==status::Result::unchanged);
    expect(service.revision()==enabled.revision && service.last_request()==2);
    expect(service.request({kOwner,kBoot,enabled.revision,3,0,false})==status::Result::accepted);
    projected=service.project(kBubble);
    expect(projected.count==1 && !projected.entries[0].enabled);
    std::array<std::byte,24> disabled{};bits::Writer writer(disabled);
    expect(wire::write_authority(writer,projected.entries[0].enabled));
    expect(writer.bit_count()==wire::kInactiveBits);
    service.release_owner();expect(!service.owner() && service.project(kBubble).count==0);
}

void one_shot_playback_waits_for_matching_native_application() {
    namespace transit=sunrise::server::runtime::activity::transit_effect;
    namespace sense=sunrise::middleware::bap::activity_message::sense_update;
    namespace native=sunrise::middleware::bap::activity_message::native_sense;
    Authored authored;authored.slots[0].senseSchema=0x8080954A;
    const std::array<status::Capability,1> effects{{{&authored.definition,kSlot}}};
    const std::array<transit::Route,2> routes{{{1,0,100},{2,0,100}}};
    const transit::Definition definition{effects,routes};
    transit::Service service;expect(service.begin(kOwner,kBoot,definition));
    expect(service.request(1,1));
    wire::Batch publication;expect(service.append(publication,kBubble));
    expect(publication.count==1 && publication.entries[0].once
        && publication.entries[0].selectionRevision==1);
    std::array<std::byte,32> encoded{};bits::Writer writer(encoded);
    expect(wire::write_authority(writer,publication.entries[0]));
    bits::Reader reader(encoded);expect(read(reader,1)==1 && read(reader,1)==0);
    for(unsigned i=0;i<3;++i)expect(read(reader,32)==0x80000000U);
    expect(read(reader,32)==0x80000001U);
    expect(!service.ready(1,10000)); // Host elapsed time alone cannot qualify playback.

    std::array<std::byte,17> payload{};bits::Writer senseWriter(payload);
    expect(senseWriter.write(1,1));
    expect(senseWriter.write(0x80000000,32));
    expect(senseWriter.write(0x80000000,32));
    expect(senseWriter.write(0x80000001,32));
    expect(senseWriter.write(0,1));expect(senseWriter.write(4,32));
    bits::Reader senseReader(payload);native::Output parsed{};std::size_t width{};
    expect(native::read(senseReader,26,parsed,width) && width==130
        && parsed.schema==0x8080954A && parsed.root && parsed.revision==4);
    bits::Reader capture(payload);sense::SenseObject object{};
    object.bodyFirst=read(capture,64);object.bodySecond=read(capture,64);object.bodyThird=read(capture,2);
    object.slotType=26;object.slotIndex=kSlot;object.registryKey=kRegistry;
    object.nativeSchema=parsed.schema;object.hasNativeSchema=true;
    object.hasRootDelta=true;object.bodyBits=130;object.nativeRevision=4;
    expect(!service.observe(kOwner,kBoot+1,kBubble,object,1000));
    expect(!service.observe(kOwner,kBoot,kBubble+1,object,1000));
    expect(service.observe(kOwner,kBoot,kBubble,object,1000));
    expect(!service.ready(1,1099) && service.ready(1,1100));
    expect(service.request(1,1) && service.ready(1,1100)); // Repeated snapshot never restarts.
    expect(!service.observe(kOwner,kBoot,kBubble,object,1050));
    expect(service.request(2,2));
    expect(!service.observe(kOwner,kBoot,kBubble,object,2000)); // Old cycle acknowledgement.
    publication={};expect(service.append(publication,kBubble));
    expect(publication.entries[0].selectionRevision==2);
    object.bodySecond+=std::uint64_t{1}<<31;
    expect(service.observe(kOwner,kBoot,kBubble,object,2000));
    expect(!service.ready(1,2100) && service.ready(2,2100));
    service.release_owner();publication={};expect(service.append(publication,kBubble) && !publication.count);
}

void local_targets_require_fresh_preparation_and_arrival_receipts() {
    namespace transit=sunrise::server::runtime::activity::transit_effect;
    namespace sense=sunrise::middleware::bap::activity_message::sense_update;
    std::array<registry::Slot,4> slots{{{kSlot,26,0x8080953F,0x8080954A,0x8080954B,0x12340002},
        {24,4,0x80809927,0x8080992E,0x8080992F,0x12340003},
        {25,30,0x8080952F,0x80809531,0x80809532,0x12340004},
        {26,4,0x80809927,0x8080992E,0x8080992F,0x12340005}}};
    registry::Definition authored{"synthetic_local_transit",1,kRegistry,0x12345679U,0x1234567AU,kBubble,slots};
    const std::array<status::Capability,1> effects{{{&authored,kSlot}}};
    const std::array<transit::TargetPlacement,1> firstTargets{{{&authored,24,0x12340003U,0x4C8}}};
    const std::array<transit::TargetPlacement,1> secondTargets{{{&authored,26,0x12340005U,0x4C8}}};
    const std::array<transit::Route,2> routes{{
        {1,0,1,firstTargets,{transit::ArrivalKind::monitor,&authored,25,0}},
        {2,0,1,secondTargets,{}}}};
    transit::Service service;const transit::Definition definition{effects,routes};
    expect(service.begin(kOwner,kBoot,definition));
    expect(!service.native_targets()); // preparation, not publication, opens the target path
    expect(service.prepare(1,1) && service.native_targets() && !service.target_ready(1));
    std::array<transit::TargetRequest,18> pending{};
    expect(service.pending_targets(pending)==1 && pending[0].registry==kRegistry
        && pending[0].slot==24 && pending[0].definitionTag==0x12340003U
        && pending[0].definitionOffset==0x4C8 && pending[0].generation==1 && pending[0].active);
    auto targets=placement::Batch{};
    expect(service.append_targets(targets,kBubble) && targets.count==1
        && targets.entries[0].slot==24 && targets.entries[0].generation==1);
    sense::SenseObject monitor{};monitor.registryKey=kRegistry;monitor.slotIndex=25;monitor.slotType=30;
    monitor.nativeSchema=0x80809531;monitor.hasNativeSchema=true;monitor.hasRootDelta=true;
    monitor.hasMonitorOutput=true;monitor.monitorOutput={true,false,1,0};monitor.nativeRevision=1;
    expect(!service.observe(kOwner,kBoot,kBubble,monitor,10)); // stale all-player root
    monitor.monitorOutput.all=true;
    expect(service.observe(kOwner,kBoot,kBubble,monitor,10)); // early receipt is retained
    expect(service.observe_target(kRegistry,24,1,true) && service.target_ready(1));
    expect(service.pulse(1) && service.requested() && !service.arrival_qualified());
    std::array<std::byte,17> effectPayload{};bits::Writer effectWriter(effectPayload);
    expect(effectWriter.write(1,1) && effectWriter.write(0x80000000U,32)
        && effectWriter.write(0x80000000U,32) && effectWriter.write(0x80000001U,32)
        && effectWriter.write(0,1) && effectWriter.write(4,32));
    bits::Reader effectReader(effectPayload);sense::SenseObject effect{};
    expect(effectReader.read(64,effect.bodyFirst) && effectReader.read(64,effect.bodySecond)
        && effectReader.read(2,effect.bodyThird));
    effect.registryKey=kRegistry;effect.slotIndex=kSlot;effect.slotType=26;
    effect.nativeSchema=0x8080954A;effect.hasNativeSchema=true;effect.hasRootDelta=true;
    effect.bodyBits=130;effect.nativeRevision=4;
    expect(service.observe(kOwner,kBoot,kBubble,effect,11) && service.arrival_qualified());
    expect(!service.pulse(1)); // exactly one effect pulse per cohort
    expect(service.prepare(2,2));
    targets={};expect(service.append_targets(targets,kBubble) && targets.count==2);
    const auto* retired=placement::find(targets,kRegistry,4,24);
    expect(retired && !retired->active && retired->generation==2); // prior child is retired
    const auto* fresh=placement::find(targets,kRegistry,4,26);
    expect(fresh && fresh->active && fresh->generation==1);
    expect(!service.target_ready(2));
    pending={};expect(service.pending_targets(pending)==2);
    expect(pending[0].slot==24 && !pending[0].active && pending[0].generation==2
        && pending[1].slot==26 && pending[1].active && pending[1].generation==1);
    auto stale=pending[1];stale.generation=2;
    expect(!service.observe_target(stale)); // a later native generation cannot satisfy this route
    expect(service.observe_target(kRegistry,26,1,true) && !service.target_ready(2));
    expect(service.observe_target(kRegistry,24,2,false) && service.target_ready(2));
    expect(!service.observe_player_trigger(kRegistry,25,1)); // mixed source/slot cannot qualify
}

int main() {
    codec_decodes_all_players_and_disabled_record_length();
    invalid_registry_and_type_proof_are_rejected();
    native_batch_validates_admitted_type26_scope();
    requests_are_monotonic_and_disable_preserves_native_defaults();
    one_shot_playback_waits_for_matching_native_application();
    local_targets_require_fresh_preparation_and_arrival_receipts();
    std::printf("status effect service: %u checks, zero failures\n",checks);
}
