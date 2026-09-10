#include "server/runtime/activity/mercury_public_event_definition.h"
#include <cstdio>
#include <vector>
#include <filesystem>
#include <fstream>
#include "middleware/content/packages/tables/slot_descriptor_reader.h"
#include "middleware/bap/activity_message/native/public_event_sense.h"

namespace pe=sunrise::server::runtime::activity::public_event;
namespace mercury=sunrise::server::runtime::activity::mercury::public_events;
namespace placement=sunrise::server::runtime::activity::placement;
namespace coo=sunrise::state::activity::coo;
namespace reg=sunrise::state::activity::coo::registry;
namespace {
unsigned checks{}, failures{};
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; std::printf("FAIL line %u: %s\n",unsigned(__LINE__),#x); } } while(false)
constexpr reg::Slot slots[]{
    {0,4,0x80809927,0x8080992E,0x8080992F,0x80110001},
    {1,30,0x8080952F,0x80809531,0x80809532,0x80110002},
    {2,4,0x80809927,0x8080992E,0x8080992F,0x80110003},
};
constexpr reg::Definition registry[]{ {"fixture_other_destination",0x80100001,0x1234,0x80100002,0xBEEF,7,slots} };
constexpr coo::Asset rally{0x1234,0x80110001,4,0}, monitor{0x1234,0x80110002,30,1}, reward{0x1234,0x80110003,4,2};
constexpr coo::CommandSpec rallyCmd[]{ {coo::Operation::device,rally,1,coo::Wait::nativeReady} };
constexpr coo::CommandSpec encounterCmd[]{ {coo::Operation::observation,monitor,10,coo::Wait::observed} };
constexpr coo::CommandSpec rewardCmd[]{ {coo::Operation::device,reward,1,coo::Wait::completed} };
constexpr coo::CommandSpec heroicCmd[]{ {coo::Operation::device,reward,2,coo::Wait::completed} };
constexpr coo::Step rallySteps[]{ {"rally",0,rallyCmd} }, encounterSteps[]{ {"condition",0,encounterCmd} },
    rewardSteps[]{ {"native_completion",0,rewardCmd} }, heroicSteps[]{ {"native_heroic_completion",0,heroicCmd} };
constexpr coo::ReceiptBinding bindings[]{ {"done",0,0} };
constexpr coo::Definition rallyGraph{"fixture_rally",coo::Schema::otherMissions,rallySteps,bindings},
    encounterGraph{"fixture_encounter",coo::Schema::otherMissions,encounterSteps,bindings},
    rewardGraph{"fixture_reward",coo::Schema::otherMissions,rewardSteps,bindings},
    heroicGraph{"fixture_heroic",coo::Schema::otherMissions,heroicSteps,bindings};
constexpr pe::Definition definition{"fixture.event",registry,&rallyGraph,&encounterGraph,&rewardGraph,&heroicGraph,{monitor,42}};
constexpr pe::Lease lease{{0x123456,{9}},0xABC,3,17};
constexpr pe::Eligibility eligible{7,true,true};
struct Ports final : pe::Ports {
    struct Row { pe::Lease lease; pe::Stage stage; coo::Command command; };
    std::vector<Row> publications, cancellations;
    bool reject{};
    bool publish(const pe::Lease& l,pe::Stage s,const coo::Command& c) noexcept override {
        if(reject) return false;
        publications.push_back({l,s,c});return true;
    }
    void cancel(const pe::Lease& l,pe::Stage s,const coo::Command& c) noexcept override {cancellations.push_back({l,s,c});}
};
pe::Receipt receipt(pe::Service& service,pe::Stage stage,coo::Asset asset,coo::Milestone milestone) {
    return {lease,stage,asset,{service.token(stage,"done"),milestone}};
}
void ready(pe::Service& service,Ports& ports) {
    service.update(ports);
    CHECK(service.observe(receipt(service,pe::Stage::rally,rally,coo::Milestone::nativeReady))==pe::Result::accepted);
    service.update(ports);CHECK(service.diagnostics().phase==pe::Phase::rallyReady);
}
void lifecycle(bool heroic) {
    pe::Service service;Ports ports;
    CHECK(service.begin(lease,definition));CHECK(!service.begin(lease,definition));
    CHECK(service.start({lease,1},eligible)==pe::Result::wrongPhase);
    CHECK(service.observe(receipt(service,pe::Stage::rally,rally,coo::Milestone::nativeReady))==pe::Result::stale);
    service.update(ports);CHECK(ports.publications.size()==1);CHECK(service.diagnostics().phase==pe::Phase::rallyPending);
    for(unsigned i=0;i<100;++i) service.update(ports);
    CHECK(ports.publications.size()==1);CHECK(service.diagnostics().phase==pe::Phase::rallyPending);
    for(unsigned i=0;i<5;++i) {
        auto stale=receipt(service,pe::Stage::rally,rally,coo::Milestone::nativeReady);
        if(i==0) ++stale.lease.boot; if(i==1) ++stale.lease.owner.sessionId;
        if(i==2) ++stale.lease.owner.incarnation.value; if(i==3) ++stale.lease.revision; if(i==4) ++stale.lease.event;
        CHECK(service.observe(stale)==pe::Result::stale);
    }
    auto wrong=receipt(service,pe::Stage::rally,rally,coo::Milestone::nativeReady);++wrong.asset.definition;
    CHECK(service.observe(wrong)==pe::Result::invalid);
    wrong=receipt(service,pe::Stage::rally,rally,coo::Milestone::nativeReady);++wrong.event.token.incarnation;
    CHECK(service.observe(wrong)==pe::Result::stale);
    ready(service,ports);
    CHECK(service.start({lease,0},eligible)==pe::Result::invalid);
    auto e=eligible;e.registriesAdmitted=false;CHECK(service.start({lease,1},e)==pe::Result::invalid);
    e=eligible;e.resourcesReserved=false;CHECK(service.start({lease,1},e)==pe::Result::invalid);
    e=eligible;e.bubble=6;CHECK(service.start({lease,1},e)==pe::Result::invalid);
    CHECK(service.start({lease,1},eligible)==pe::Result::accepted);
    CHECK(service.start({lease,1},eligible)==pe::Result::duplicate);
    CHECK(service.start({lease,2},eligible)==pe::Result::wrongPhase);
    if(heroic) {
        CHECK(service.heroic({lease,rally,42})==pe::Result::invalid);
        CHECK(service.heroic({lease,monitor,43})==pe::Result::invalid);
        CHECK(service.heroic({lease,monitor,42})==pe::Result::accepted);
        CHECK(service.heroic({lease,monitor,42})==pe::Result::duplicate);
    }
    service.update(ports);CHECK(ports.publications.size()==2);
    CHECK(service.observe(receipt(service,pe::Stage::encounter,monitor,coo::Milestone::observed))==pe::Result::accepted);
    service.update(ports);CHECK(service.diagnostics().phase==pe::Phase::completing);
    CHECK(service.heroic({lease,monitor,42})==pe::Result::wrongPhase);
    service.update(ports);CHECK(ports.publications.size()==3);
    CHECK(ports.publications.back().command.spec.argument==(heroic?2U:1U));
    CHECK(service.observe(receipt(service,pe::Stage::completion,reward,coo::Milestone::completed))==pe::Result::accepted);
    service.update(ports);CHECK(service.diagnostics().phase==pe::Phase::completing);
    CHECK(service.observe(receipt(service,pe::Stage::completion,reward,coo::Milestone::nativeReady))==pe::Result::accepted);
    CHECK(service.observe(receipt(service,pe::Stage::completion,reward,coo::Milestone::completed))==pe::Result::accepted);
    service.update(ports);CHECK(service.diagnostics().phase==pe::Phase::complete);
    service.update(ports);CHECK(ports.publications.size()==3);
    CHECK(service.retired({lease,true,true,true,0})==pe::Result::wrongPhase);
    CHECK(service.retire(lease,ports)==pe::Result::accepted);CHECK(ports.cancellations.size()==3);
    CHECK(service.retire(lease,ports)==pe::Result::duplicate);CHECK(ports.cancellations.size()==3);
    CHECK(ports.cancellations[0].stage==pe::Stage::completion);CHECK(ports.cancellations[2].stage==pe::Stage::rally);
    for(unsigned i=0;i<4;++i) {
        pe::Retirement r{lease,true,true,true,0};
        if(i==0)r.sourcesQuiesced=false;if(i==1)r.placementsRetired=false;if(i==2)r.observationsDrained=false;if(i==3)r.residentActors=1;
        CHECK(service.retired(r)==pe::Result::invalid);
    }
    CHECK(service.diagnostics().phase==pe::Phase::retiring);
    CHECK(service.observe(receipt(service,pe::Stage::encounter,monitor,coo::Milestone::observed))==pe::Result::wrongPhase);
    CHECK(service.retired({lease,true,true,true,0})==pe::Result::accepted);
    CHECK(service.diagnostics().phase==pe::Phase::retired);CHECK(!service.begin(lease,definition));
}
void validation() {
    CHECK(pe::Service::valid(definition));CHECK(pe::Service::valid(mercury::kRallyProbeDefinition));
    auto invalid=definition;invalid.id={};CHECK(!pe::Service::valid(invalid));
    invalid=definition;invalid.heroic.condition=0;CHECK(!pe::Service::valid(invalid));
    auto graph=encounterGraph;graph.schema=coo::Schema::omegaArchive;
    invalid=definition;invalid.encounter=&graph;CHECK(!pe::Service::valid(invalid));
    coo::CommandSpec requested[]{ {coo::Operation::device,rally,1,coo::Wait::requested} };
    coo::Step requestedSteps[]{ {"request",0,requested} };coo::Definition requestedGraph{"request_only",coo::Schema::otherMissions,requestedSteps,{}};
    invalid=definition;invalid.encounter=&requestedGraph;CHECK(!pe::Service::valid(invalid));
    invalid=definition;invalid.rally=&requestedGraph;CHECK(!pe::Service::valid(invalid));
    pe::Service service;Ports ports;CHECK(service.begin(lease,definition));ports.reject=true;service.update(ports);
    CHECK(service.diagnostics().phase==pe::Phase::failed);
    CHECK(service.start({lease,1},eligible)==pe::Result::wrongPhase);
    CHECK(service.retired({lease,true,true,true,0})==pe::Result::wrongPhase);
    CHECK(service.retire(lease,ports)==pe::Result::accepted);CHECK(service.diagnostics().phase==pe::Phase::retiring);
}
struct BitWriter {
    std::vector<bool> bits;
    bool write(std::uint64_t value,std::size_t width) {for(std::size_t i=0;i<width;++i)bits.push_back(((value>>i)&1)!=0);return true;}
    std::uint64_t read(std::size_t& cursor,std::size_t width) const {std::uint64_t value{};for(std::size_t i=0;i<width;++i)value|=std::uint64_t(bits[cursor++])<<i;return value;}
};
void raw_sense() {
    namespace sense=sunrise::middleware::bap::activity_message::native::public_event_sense;
    struct Reader {
        const std::vector<bool>& bits;std::size_t end{},cursor{};
        std::size_t remaining_bits() const noexcept {return end-cursor;}
        bool read(std::size_t count,std::uint64_t& value) noexcept {
            if(count>remaining_bits())return false;
            value=0;for(std::size_t i=0;i<count;++i)value|=std::uint64_t(bits[cursor++])<<i;
            return true;
        }
    };
    for(unsigned root=0;root<2;++root)for(unsigned flag=0;flag<2;++flag) {
        BitWriter wire;wire.write(root,1);if(root)wire.write(flag,1);wire.write(0xA1234567,32);
        const auto expected=wire.bits.size();wire.write(1,1); // next enclosing field remains untouched
        Reader reader{wire.bits,wire.bits.size()};sense::Output output{};std::size_t width{};
        CHECK(sense::read(reader,output,width));CHECK(width==expected);CHECK(reader.remaining_bits()==1);
        CHECK(output.revision==0xA1234567 && output.root==bool(root) && output.value==bool(root&&flag));
        for(std::size_t length=0;length<expected;++length) {
            Reader truncated{wire.bits,length};sense::Output unchanged{0x55,true,true};width=123;
            CHECK(!sense::read(truncated,unchanged,width));CHECK(unchanged.revision==0x55 && unchanged.root && unchanged.value && width==123);
        }
    }
}
void mercury_probe() {
    pe::Service service;pe::PlacementAdapter adapter;
    CHECK(service.begin(lease,mercury::kRallyProbeDefinition));CHECK(adapter.begin(lease,mercury::kRallyPlacement));
    placement::wire::Batch frame{};CHECK(adapter.append(15,frame));CHECK(frame.count==0);
    service.update(adapter);CHECK(adapter.published());CHECK(service.diagnostics().phase==pe::Phase::rallyPending);
    CHECK(service.start({lease,1},{15,true,true})==pe::Result::unavailable);
    CHECK(service.heroic({lease,mercury::kRallyFlag,1})==pe::Result::unavailable);
    CHECK(adapter.append(14,frame));CHECK(frame.count==0);
    CHECK(adapter.append(15,frame));CHECK(frame.count==1);
    CHECK(frame.entries[0].registry==0x85C38F77 && frame.entries[0].slot==0 && frame.entries[0].bubble==15);
    CHECK(!adapter.append(15,frame));CHECK(frame.count==1);
    for(unsigned i=0;i<100;++i){service.update(adapter);frame={};CHECK(adapter.append(15,frame));CHECK(frame.count==1);}
    CHECK(service.diagnostics().phase==pe::Phase::rallyPending);
    CHECK(service.retire(lease,adapter)==pe::Result::accepted);CHECK(adapter.cancellation_pending());
    frame={};CHECK(adapter.append(15,frame));CHECK(frame.count==1); // retained native lease
    CHECK(service.diagnostics().phase==pe::Phase::retiring);
    BitWriter bits;CHECK(placement::wire::write_active(bits));CHECK(bits.bits.size()==253);
    std::size_t cursor{};
    CHECK(bits.read(cursor,32)==0);CHECK(bits.read(cursor,32)==1);CHECK(bits.read(cursor,1)==1);CHECK(bits.read(cursor,1)==1);
    CHECK(bits.read(cursor,32)==0);CHECK(bits.read(cursor,32)==0x811C9DC5);CHECK(bits.read(cursor,7)==0);CHECK(bits.read(cursor,16)==32767);
    CHECK(bits.read(cursor,32)==0);CHECK(bits.read(cursor,32)==0);CHECK(bits.read(cursor,32)==0);CHECK(bits.read(cursor,1)==0);
    CHECK(bits.read(cursor,2)==1);CHECK(bits.read(cursor,1)==0);CHECK(cursor==253);
    CHECK(mercury::kRegistries[0].slots.size()==13);CHECK(mercury::kRegistries[1].slots.size()==145);CHECK(mercury::kRegistries[2].slots.size()==37);
    for(const auto& source:mercury::kSources) {
        unsigned sources{},rules{};
        for(const auto& slot:mercury::kRegistries[1].slots) {
            if(slot.index==source.slot && slot.type==1 && slot.descriptorTag==source.definition)++sources;
            if(slot.index==source.primaryRule && slot.type==66)++rules;
        }
        CHECK(sources==1);CHECK(rules==1);CHECK(source.primaryRule==source.fallbackRule);
    }
}
void placement_feedback() {
    namespace feedback=pe::placement_feedback;
    pe::Service service;pe::PlacementAdapter adapter;
    CHECK(service.begin(lease,mercury::kRallyProbeDefinition));
    CHECK(adapter.begin(lease,mercury::kRallyPlacement,mercury::kRallyFeedback));
    feedback::Ticket ticket{};
    CHECK(!adapter.ticket(lease,15,15,ticket));
    service.update(adapter);CHECK(adapter.ticket(lease,15,15,ticket));
    const auto authorityBits=[&] {
        placement::wire::Batch frame{};CHECK(adapter.append(15,frame));CHECK(frame.count==1);
        BitWriter wire;CHECK(placement::wire::write(wire,frame.entries[0]));
        CHECK(wire.bits.size()==placement::wire::body_bits(frame.entries[0]));return wire.bits.size();
    };
    CHECK(!adapter.interaction_published() && authorityBits()==253);
    std::array<std::byte,feedback::kComponentBytes> component{};
    std::array<std::byte,0x70> authority{};
    std::array<std::byte,feedback::kAuthorityBytes> body{};
    std::array<std::byte,feedback::kSenseBytes> sense{};
    const auto put=[]<class Bytes,class T>(Bytes& bytes,std::size_t at,T value) {
        std::memcpy(bytes.data()+at,&value,sizeof value);
    };
    put(component,0,0x80F5BF33U);put(component,4,0x80809928U);put(component,8,std::int64_t{0x4C8});
    put(component,0x160,0x0A123456U);put(component,0x164,0x80809927U);put(component,0x168,std::int64_t{0x280});
    put(component,0x444,0x04234567U);
    put(authority,0,0x85C38F77U);put(authority,4,std::uint8_t{4});put(authority,6,std::uint16_t{0});
    put(authority,0xC,0x8080992FU);put(authority,0x68,15U);
    // Captured native ABI values, independently documented by reflection and
    // 9F19F0/9EFFC0; these are not a synthetic wire "success" callback.
    put(body,0,0x80000000U);put(body,4,0x80000001U);put(body,8,std::uint8_t{1});put(body,9,std::uint8_t{1});
    put(body,0xC,0x80000000U);put(body,0x10,0x811C9DC5U);put(body,0x14,std::uint8_t{0xFF});
    put(body,0x16,std::uint16_t{0xFFFF});put(body,0x40,1U);put(body,0x50,0xFFFFFFFFU);
    put(sense,0,0x80000000U);put(sense,5,std::uint8_t{0});put(sense,8,0U);put(sense,12,1U);
    const auto copyState=[&] {
        std::memcpy(component.data()+0x180,body.data(),body.size());
        std::memcpy(component.data()+0x2F0,sense.data(),sense.size());
    };
    copyState();
    auto prior=component;
    feedback::Capture capture{ticket,component,authority,body,sense,{0x0A123456,0x280},0x04234567,0,feedback::kProducerRva,1,prior,0x04234567,0};
    feedback::Observation observation{};
    CHECK(feedback::qualify(ticket,capture,observation));
    CHECK(observation.receipt.asset==mercury::kRallyFlag && observation.entity==0x04234567);
    CHECK(observation.receipt.event.milestone==coo::Milestone::nativeReady);
    const auto reject=[&](feedback::Capture bad) {feedback::Observation out{};out.sequence=99;
        CHECK(!feedback::qualify(ticket,bad,out));CHECK(out.sequence==99);};
    for(unsigned i=0;i<21;++i) {
        auto bad=capture;
        switch(i) {
        case 0:++bad.ticket.lease.boot;break;case 1:++bad.ticket.lease.owner.sessionId;break;
        case 2:++bad.ticket.lease.owner.incarnation.value;break;case 3:++bad.ticket.lease.event;break;
        case 4:++bad.ticket.lease.revision;break;case 5:++bad.ticket.token.incarnation;break;
        case 6:++bad.ticket.asset.definition;break;case 7:++bad.ticket.asset.registry;break;
        case 8:++bad.ticket.asset.slot;break;case 9:++bad.ticket.bubble;break;
        case 10:++bad.ticket.authorityOwner;break;case 11:++bad.producerRva;break;
        case 12:bad.sequence=0;break;case 13:++bad.resolvedSource.member;break;
        case 14:++bad.resolvedSource.offset;break;case 15:bad.resolvedEntity=UINT32_MAX;break;
        case 16:++bad.resolvedEntity;break;case 17:bad.entityFlags=4;break;
        case 18:++bad.ticket.definition.nativeDefinitionOffset;break;
        case 19:++bad.ticket.definition.authoredVisualCount;break;
        case 20:bad.ticket.definition.enableInteractionAfterPlacement=false;break;
        }
        reject(bad);
    }
    // Every truncated native snapshot must fail without producing a receipt.
    for(std::size_t i=0;i<component.size();++i){auto bad=capture;bad.component=std::span(component).first(i);reject(bad);}
    for(std::size_t i=0;i<authority.size();++i){auto bad=capture;bad.authorityObject=std::span(authority).first(i);reject(bad);}
    for(std::size_t i=0;i<body.size();++i){auto bad=capture;bad.authorityBody=std::span(body).first(i);reject(bad);}
    for(std::size_t i=0;i<sense.size();++i){auto bad=capture;bad.senseBody=std::span(sense).first(i);reject(bad);}
    for(std::size_t i=0;i<prior.size();++i){auto bad=capture;bad.priorComponent=std::span(prior).first(i);reject(bad);}
    // The live local flag retained committed generation0 while adopted authority
    // was INT_MIN. Existing-object readiness requires exact retained identity and
    // committed state, not invented equality with the incoming authority.
    put(sense,0,0U);copyState();put(prior,0x2F0,0U);
    CHECK(feedback::qualify(ticket,capture,observation));
    put(prior,0x2F0,1U);reject(capture);put(prior,0x2F0,0U);
    auto badPrior=capture;++badPrior.priorResolvedEntity;reject(badPrior);
    put(prior,0x444,0x04234568U);reject(capture);put(prior,0x444,0x04234567U);
    // New local creation commits the previous prepared generation. Exercise
    // several values so the observed live0 never becomes a magic allowance.
    capture.priorResolvedEntity=UINT32_MAX;put(prior,0x444,UINT32_MAX);
    for(const auto generation:{0U,7U,0x80000000U,0x7FFFFFFFU}) {
        put(prior,0x180,generation);put(sense,0,generation);copyState();
        CHECK(feedback::qualify(ticket,capture,observation));
        put(sense,0,generation^1U);copyState();reject(capture);
    }
    put(sense,0,0x80000000U);copyState();prior=component;capture.priorResolvedEntity=0x04234567;
    // Valid but wrong-generation state, a stale selector and absent native
    // entity all retain rallyPending; updating the adopted copy cannot bypass it.
    put(sense,0,1U);copyState();reject(capture);put(sense,0,0x80000000U);
    put(sense,8,1U);copyState();reject(capture);put(sense,8,0U);put(sense,12,1U);
    // The authored flag is local (+94=0), so its replicated sense flag stays0.
    copyState();CHECK(feedback::qualify(ticket,capture,observation));
    component[0x180]^=std::byte{1};reject(capture);copyState();
    component[0x2F0]^=std::byte{1};reject(capture);copyState();
    CHECK(adapter.observe(capture,14,service)==pe::Result::invalid);
    CHECK(service.diagnostics().phase==pe::Phase::rallyPending);
    CHECK(!adapter.interaction_published() && authorityBits()==253);
    auto stale=capture;++stale.ticket.lease.owner.incarnation.value;
    CHECK(adapter.observe(stale,15,service)==pe::Result::stale);
    CHECK(!adapter.interaction_published() && authorityBits()==253);
    CHECK(adapter.observe(capture,15,service)==pe::Result::accepted);
    CHECK(adapter.interaction_published() && authorityBits()==375);
    CHECK(adapter.observe(capture,15,service)==pe::Result::duplicate);
    CHECK(service.diagnostics().phase==pe::Phase::rallyPending);service.update(adapter);
    CHECK(service.diagnostics().phase==pe::Phase::rallyReady);
    CHECK(service.start({lease,1},{15,true,true})==pe::Result::unavailable);
    CHECK(service.retire(lease,adapter)==pe::Result::accepted);
    CHECK(!adapter.ticket(lease,15,15,ticket));++capture.sequence;
    CHECK(adapter.observe(capture,15,service)==pe::Result::stale);
    CHECK(adapter.interaction_published() && authorityBits()==375); // cancellation cannot invent retirement
    pe::Service nextService;pe::PlacementAdapter nextAdapter;auto nextLease=lease;++nextLease.owner.incarnation.value;
    CHECK(nextService.begin(nextLease,mercury::kRallyProbeDefinition));
    CHECK(nextAdapter.begin(nextLease,mercury::kRallyPlacement,mercury::kRallyFeedback));nextService.update(nextAdapter);
    CHECK(nextAdapter.observe(capture,15,nextService)==pe::Result::stale);CHECK(!nextAdapter.interaction_published());
    placement::wire::Batch nextFrame{};CHECK(nextAdapter.append(15,nextFrame));
    CHECK(nextFrame.count==1 && placement::wire::body_bits(nextFrame.entries[0])==253);
}
void packages(const char* directory) {
    namespace tables=sunrise::middleware::content::packages::tables;
    struct Expected { const reg::Slot* slot; unsigned matches{}; };
    const auto visitor=[](void* context,const tables::SlotDescriptor& descriptor) noexcept {
        auto& expected=*static_cast<Expected*>(context);
        const auto& slot=*expected.slot;
        if(descriptor.slotIndex!=slot.index || descriptor.slotType!=slot.type || descriptor.sourceTag!=slot.descriptorTag
            || descriptor.componentClass!=slot.componentClass || descriptor.senseSchema!=slot.senseSchema
            || descriptor.authSchema!=slot.authSchema) return false;
        ++expected.matches;return true;
    };
    for(const auto& group:mercury::kRegistries) {
        CHECK(sunrise::server::runtime::activity::registry::valid(group));
        for(const auto& slot:group.slots) {
            char name[16]{};std::snprintf(name,sizeof(name),"%08X.bin",slot.descriptorTag);
            std::ifstream input(std::filesystem::path(directory)/name,std::ios::binary|std::ios::ate);
            CHECK(input && input.tellg()>4);
            if(!input || input.tellg()<=4)continue;
            const auto size=static_cast<std::size_t>(input.tellg());
            input.seekg(0);std::uint32_t cls{};input.read(reinterpret_cast<char*>(&cls),4);
            std::vector<std::byte> blob(size-4);input.read(reinterpret_cast<char*>(blob.data()),static_cast<std::streamsize>(blob.size()));
            CHECK(input && cls==tables::kPlacedObjectClass);
            Expected expected{&slot};
            CHECK(tables::visit_slot_descriptors(blob,slot.descriptorTag,group.key,visitor,&expected));CHECK(expected.matches==1);
            expected.matches=0;CHECK(tables::visit_slot_descriptors(blob,slot.descriptorTag,group.key^1U,visitor,&expected));CHECK(expected.matches==0);
            expected.matches=0;CHECK(tables::visit_slot_descriptors(blob,slot.descriptorTag^1U,group.key,visitor,&expected));CHECK(expected.matches==0);
        }
    }
}
void original_native_apply(const char* directory) {
    namespace feedback=pe::placement_feedback;
    const auto read=[&](const std::string& name) {
        std::ifstream input(std::filesystem::path(directory)/name,std::ios::binary|std::ios::ate);
        CHECK(input && input.tellg()>0);if(!input || input.tellg()<=0)return std::vector<std::byte>{};
        std::vector<std::byte> bytes(static_cast<std::size_t>(input.tellg()));input.seekg(0);
        input.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));CHECK(input.good());return bytes;
    };
    const auto authority=read("authority-header.bin");
    const feedback::Ticket ticket{lease,mercury::kRallyFlag,{lease.event,1,0,0},15,15,mercury::kRallyFeedback};
    for(unsigned i=0;i<4;++i) {
        const auto before=read("case"+std::to_string(i)+"-before.bin");
        const auto after=read("case"+std::to_string(i)+"-after.bin");
        CHECK(before.size()==feedback::kComponentBytes && after.size()==feedback::kComponentBytes);
        if(after.size()!=feedback::kComponentBytes)continue;
        feedback::Capture capture{ticket,after,authority,std::span(after).subspan(0x180,feedback::kAuthorityBytes),
            std::span(after).subspan(0x2F0,feedback::kSenseBytes),
            {feedback::field<std::uint32_t>(after,0x160),feedback::field<std::int64_t>(after,0x168)},
            feedback::field<std::uint32_t>(after,0x444),0,feedback::kProducerRva,1,before,i==2?1U:UINT32_MAX,0};
        feedback::Observation observation{};observation.sequence=99;
        // Bytes were produced by original9F19F0/9EFFC0/9F2200 in a private
        // emulator. Only the entity/weak/input/override API boundaries are fake.
        CHECK(feedback::qualify(ticket,capture,observation)==(i!=3));
        if(i==3)CHECK(observation.sequence==99);
        else {
            CHECK(observation.receipt.event.milestone==coo::Milestone::nativeReady);
            ++capture.ticket.lease.owner.incarnation.value;CHECK(!feedback::qualify(ticket,capture,observation));
        }
    }
}
}
int main(int argc,char** argv) {
    lifecycle(false);lifecycle(true);validation();mercury_probe();raw_sense();placement_feedback();
    if(argc>=2)packages(argv[1]);
    if(argc==3)original_native_apply(argv[2]);
    std::printf("public_event_tests: %u checks, %u failures\n",checks,failures);return failures?1:0;
}
