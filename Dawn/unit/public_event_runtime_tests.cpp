#include "server/runtime/activity/mercury_definition.h"
#include "server/runtime/activity/persistent_activity.h"
#include "mission_parameter_fixture.h"
#include <cstdio>
#include <fstream>
#include <thread>
#include <vector>

namespace runtime=dawn::server::runtime::activity;
namespace pe=runtime::public_event;
namespace bridge=pe::native_bridge;
namespace feedback=pe::placement_feedback;
namespace mercury=runtime::mercury;
namespace coo=dawn::state::activity::coo;
namespace {
unsigned checks{},failures{};
#define CHECK(x) do {++checks;if(!(x)){++failures;std::printf("FAIL line %u: %s\n",unsigned(__LINE__),#x);}}while(false)
constexpr pe::Lease lease{{0x845231,{9}},0xABC,3,17};
feedback::Ticket ticket() {
    return {lease,mercury::public_events::kRallyFlag,{17,1,0,0},15,15,mercury::public_events::kRallyFeedback};
}
// Synthetic already-qualified records exercise the bridge/server boundary.
// Actual native qualification is independently tested by public_event_tests.
bridge::Event event(const bridge::Binding& binding) {
    const auto& t=binding.ticket;
    return {binding,{{t.lease,pe::Stage::rally,t.asset,{t.token,coo::Milestone::nativeReady}},
        {0x01234012,0x880},0x04234567,0,t.authorityOwner,1}};
}
void mailbox() {
    bridge::Mailbox box;auto t=ticket();
    CHECK(box.bind(t));auto binding=box.lookup_definition(t.asset.definition);CHECK(binding.epoch!=0);
    CHECK(box.bind(t));CHECK(box.lookup(t.asset.definition,t.asset.registry,t.asset.slot).epoch==binding.epoch);
    CHECK(!box.lookup(t.asset.definition,t.asset.registry,t.asset.slot+1).epoch);
    auto concurrent=t;++concurrent.lease.owner.incarnation.value;CHECK(!box.bind(concurrent));
    auto proof=event(binding);
    for(unsigned i=0;i<13;++i) {
        auto bad=proof;
        switch(i) {
        case 0:++bad.binding.epoch;break;
        case 1:++bad.binding.ticket.lease.boot;break;
        case 2:++bad.observation.receipt.lease.owner.sessionId;break;
        case 3:bad.observation.receipt.stage=pe::Stage::encounter;break;
        case 4:++bad.observation.receipt.asset.slot;break;
        case 5:++bad.observation.receipt.event.token.incarnation;break;
        case 6:bad.observation.receipt.event.milestone=coo::Milestone::completed;break;
        case 7:++bad.observation.authorityOwner;break;
        case 8:bad.observation.sequence=0;break;
        case 9:bad.observation.entity=UINT32_MAX;break;
        case 10:bad.observation.source.member=UINT32_MAX;break;
        case 11:bad.observation.source.offset=-1;break;
        case 12:bad.observation.selector=1;break;
        }
        CHECK(!box.submit(bad));
    }
    CHECK(box.submit(proof));CHECK(!box.submit(proof));CHECK(!box.lookup_definition(t.asset.definition).epoch);
    std::array<bridge::Event,16> output{};
    CHECK(box.drain(concurrent.lease.owner,output)==0);CHECK(box.drain(t.lease.owner,output)==1);
    CHECK(output[0].binding.epoch==binding.epoch);CHECK(box.drain(t.lease.owner,output)==0);
    box.release(t.lease.owner);CHECK(!box.submit(proof));CHECK(box.bind(t));
    const auto renewed=box.lookup_definition(t.asset.definition);CHECK(renewed.epoch>binding.epoch);
    CHECK(!box.submit(proof));CHECK(box.submit(event(renewed)));
    box.release(t.lease.owner);CHECK(box.drain(t.lease.owner,output)==0);
    // Full salted owners and immutable epochs survive multiple slot reuse cycles.
    for(unsigned i=0;i<64;++i) {
        CHECK(box.bind(t));const auto current=box.lookup_definition(t.asset.definition);CHECK(current.epoch>renewed.epoch);
        CHECK(!box.submit(proof));CHECK(box.submit(event(current)));box.release(t.lease.owner);
    }
    for(unsigned i=0;i<16;++i) {
        auto row=t;row.asset.definition+=i;row.asset.registry+=i;row.lease.owner.sessionId+=i;
        CHECK(box.bind(row));CHECK(box.submit(event(box.lookup_definition(row.asset.definition))));
    }
    auto excess=t;excess.asset.definition+=16;excess.asset.registry+=16;CHECK(!box.bind(excess));CHECK(!box.overflow());
    for(unsigned i=0;i<16;++i) {auto owner=t.lease.owner;owner.sessionId+=i;CHECK(box.drain(owner,output)==1);box.release(owner);}
    // Ambiguous payload reuse cannot select a ticket using only the callback header.
    CHECK(box.bind(t));auto ambiguous=t;++ambiguous.asset.registry;++ambiguous.asset.slot;
    CHECK(box.bind(ambiguous));CHECK(!box.lookup_definition(t.asset.definition).epoch);
    CHECK(box.lookup(t.asset.definition,t.asset.registry,t.asset.slot).epoch!=0);
}
void concurrent_bridge() {
    const auto t=ticket();CHECK(bridge::bind(t));const auto proof=event(bridge::lookup_definition(t.asset.definition));
    std::array<unsigned,8> accepted{};std::vector<std::thread> writers;
    for(unsigned i=0;i<accepted.size();++i)writers.emplace_back([&,i]{for(unsigned j=0;j<64;++j)accepted[i]+=bridge::submit(proof)?1U:0U;});
    for(auto& writer:writers)writer.join();unsigned total{};for(const auto count:accepted)total+=count;
    CHECK(total==1);std::array<bridge::Event,2> output{};bool overflow{};
    CHECK(bridge::drain(t.lease.owner,output,overflow)==1 && !overflow);
    bridge::release(t.lease.owner);CHECK(!bridge::submit(proof));
}
std::shared_ptr<const coo::script::MissionDocument> parse(const std::string& text) {
    std::string error;auto document=coo::script::MissionDocument::parse(text,mercury::kProfile,error);
    if(!document)std::printf("Parse: %s\n",error.c_str());CHECK(document!=nullptr);return document;
}
void real_profile(const char* combinedOutput) {
    std::ifstream file("Dawn/scripts/mercury_freeroam.json",std::ios::binary);CHECK(file.good());
    std::string text((std::istreambuf_iterator<char>(file)),{});
    CHECK(mission_parameter_fixture::numeric(text,"public_event_rally_probe",0));
    CHECK(mission_parameter_fixture::numeric(text,"ambient_vex_probe_count",0));
    const auto disabled=parse(text);if(!disabled)return;
    CHECK(disabled->views().parameter("public_event_rally_probe")->value==0);
    CHECK(runtime::PersistentActivity::valid(mercury::kActivity,*disabled));
    CHECK(mercury::kActivity.animations.size()==1 && mercury::kActivity.startRoutes.size()==3 && mercury::kActivity.ambientInitial.size()==2);
    runtime::PersistentActivity normal;CHECK(normal.begin({500,{1}},mercury::kActivity,disabled,17));
    runtime::NativeActivityFrame frame{};for(unsigned i=0;i<4;++i)frame=normal.update(15,true);
    CHECK(frame.placements.count==9 && frame.populations.count==2);
    CHECK(!normal.rally().enabled() && normal.rally().diagnostics().phase==pe::Phase::idle);
    CHECK(!bridge::lookup_definition(mercury::public_events::kRallyFlag.definition).epoch);
    CHECK(mission_parameter_fixture::numeric(text,"public_event_rally_probe",1));
    const auto enabled=parse(text);if(!enabled)return;
    CHECK(runtime::PersistentActivity::valid(mercury::kActivity,*enabled));
    const runtime::population::Owner owner{501,{2}};
    runtime::PersistentActivity activity;CHECK(activity.begin(owner,mercury::kActivity,enabled,18));
    CHECK(activity.rally().enabled());CHECK(activity.placements().count==0);
    CHECK(activity.update(15,false).placements.count==0);CHECK(activity.update(14,true).placements.count==0);
    CHECK(!bridge::lookup_definition(mercury::public_events::kRallyFlag.definition).epoch);
    for(unsigned i=0;i<4;++i)frame=activity.update(15,true);
    CHECK(!activity.rally().failed() && activity.rally().diagnostics().phase==pe::Phase::rallyPending);
    CHECK(frame.placements.count==10 && frame.populations.count==2 && activity.placements().count==10);
    const auto interactionFrame=[&](const auto& placements,bool enabledInteraction) {
        unsigned rallyCount{};
        for(std::size_t i=0;i<placements.count;++i) {
            const auto& row=placements.entries[i];const bool rally=row.registry==0x85C38F77 && row.slot==0;
            rallyCount+=rally;CHECK(runtime::placement::wire::body_bits(row)==(rally && enabledInteraction?375U:253U));
        }
        CHECK(rallyCount==1);
    };
    interactionFrame(frame.placements,false);
    const auto binding=bridge::lookup_definition(mercury::public_events::kRallyFlag.definition);
    CHECK(binding.epoch && binding.ticket.lease.owner==owner && binding.ticket.lease.boot==18);
    unsigned rally{},foreign{};for(unsigned i=0;i<frame.placements.count;++i) {
        const auto& p=frame.placements.entries[i];rally+=p.registry==0x85C38F77 && p.slot==0;
        foreign+=p.registry==0xC8229B2B || p.registry==0xB1EAC5B6;
    }
    CHECK(rally==1 && foreign==0);
    const auto proof=event(binding);CHECK(bridge::submit(proof));
    CHECK(activity.rally().diagnostics().phase==pe::Phase::rallyPending);
    frame=activity.update(15,true);
    CHECK(activity.rally().diagnostics().phase==pe::Phase::rallyReady && !activity.rally().failed());
    CHECK(frame.placements.count==10 && frame.populations.count==2);
    interactionFrame(frame.placements,true);
    CHECK(!activity.rally().diagnostics().encounterAvailable && !activity.rally().diagnostics().heroicAvailable);
    CHECK(!bridge::submit(proof));
    for(unsigned i=0;i<32;++i) {frame=activity.update(15,true);CHECK(frame.placements.count==10 && activity.rally().diagnostics().phase==pe::Phase::rallyReady);}
    bridge::release(owner);CHECK(!bridge::submit(proof));
    runtime::PersistentActivity replacement;const runtime::population::Owner next{501,{3}};
    CHECK(replacement.begin(next,mercury::kActivity,enabled,18));for(unsigned i=0;i<4;++i)frame=replacement.update(15,true);
    const auto current=bridge::lookup_definition(mercury::public_events::kRallyFlag.definition);
    CHECK(current.epoch>binding.epoch && current.ticket.lease.owner==next);CHECK(!bridge::submit(proof));
    CHECK(replacement.rally().diagnostics().phase==pe::Phase::rallyPending);
    interactionFrame(frame.placements,false);
    bridge::release(next);
    // Reviewable next-build definition enables both bounded probes. A native
    // monitor observation gates the new source independently of flag readiness.
    auto combined=text;CHECK(mission_parameter_fixture::numeric(combined,"ambient_vex_probe_count",1));
    const auto combinedDocument=parse(combined);if(!combinedDocument)return;
    CHECK(runtime::PersistentActivity::valid(mercury::kActivity,*combinedDocument));
    runtime::PersistentActivity together;const runtime::population::Owner both{502,{4}};
    CHECK(together.begin(both,mercury::kActivity,combinedDocument,19));
    for(unsigned i=0;i<4;++i)frame=together.update(15,true);
    CHECK(frame.populations.count==2 && frame.placements.count==10);
    interactionFrame(frame.placements,false);
    CHECK(together.ambient().state(0)==runtime::ambient_population::InitialState::awaitingMonitor);
    runtime::ambient_population::sense::SenseObject monitor{};
    monitor.registryKey=0xEB1E8934;monitor.slotIndex=3;monitor.slotType=30;
    monitor.hasNativeSchema=true;monitor.nativeSchema=0x80809531;monitor.nativeRevision=1;
    monitor.hasRootDelta=true;monitor.bodyBits=99;
    monitor.bodyFirst=0xD000000030000000ULL;monitor.bodySecond=1;
    CHECK(together.ambient().observe(both,19,15,monitor)==1);
    frame=together.update(15,true);
    CHECK(frame.populations.count==3 && frame.placements.count==10);
    CHECK(frame.populations.entries[2].source.registry==0xEB1E8934 && frame.populations.entries[2].slot==0
        && frame.populations.entries[2].source.looseRequested==1 && frame.populations.entries[2].source.ruleSlot==6
        && frame.populations.entries[2].source.tactical.slot==1 && frame.populations.entries[2].source.tactical.row==1);
    const auto togetherProof=event(bridge::lookup_definition(mercury::public_events::kRallyFlag.definition));
    CHECK(bridge::submit(togetherProof));frame=together.update(15,true);
    CHECK(frame.populations.count==3 && frame.placements.count==10 && together.rally().diagnostics().phase==pe::Phase::rallyReady);
    interactionFrame(frame.placements,true);
    CHECK(together.ambient().state(0)==runtime::ambient_population::InitialState::published);
    bridge::release(both);
    if(combinedOutput && !failures) {std::ofstream output(combinedOutput,std::ios::binary);output<<combined;CHECK(output.good());}
    auto bad=mercury::kActivity;auto bound=mercury::kPublicEventRallies[0];bad.publicEventRallies={&bound,1};
    ++bound.nativeAuthorityOwner;CHECK(!runtime::PersistentActivity::valid(bad,*enabled)); // native scope must equal the authored bubble
    bound.nativeAuthorityOwner=64;CHECK(!runtime::PersistentActivity::valid(bad,*enabled));
    bound=mercury::kPublicEventRallies[0];bound.placement.slot=1;CHECK(!runtime::PersistentActivity::valid(bad,*enabled));
    bound=mercury::kPublicEventRallies[0];bound.enabledParameter="missing";CHECK(!runtime::PersistentActivity::valid(bad,*enabled));
    bound=mercury::kPublicEventRallies[0];bound.feedback.authoredVisualCount=0;CHECK(!runtime::PersistentActivity::valid(bad,*enabled));
    bound=mercury::kPublicEventRallies[0];auto copy=*bound.placement.registry;bound.placement.registry=&copy;
    CHECK(!runtime::PersistentActivity::valid(bad,*enabled));
    bound=mercury::kPublicEventRallies[0];bad.placements={&bound.placement,1};CHECK(!runtime::PersistentActivity::valid(bad,*enabled));
    auto invalid=text;CHECK(mission_parameter_fixture::numeric(invalid,"public_event_rally_probe",2));
    std::string error;CHECK(!coo::script::MissionDocument::parse(invalid,mercury::kProfile,error));
    // The separate state-layer admission roster must exactly match every recovered descriptor.
    const auto& state=mercury::kRegistries[5];const auto& recovered=mercury::public_events::kRegistries[0];
    CHECK(state.slots.size()==recovered.slots.size());
    for(unsigned i=0;i<state.slots.size();++i) {
        const auto& a=state.slots[i];const auto& b=recovered.slots[i];
        CHECK(a.index==b.index && a.type==b.type && a.componentClass==b.componentClass && a.senseSchema==b.senseSchema
            && a.authSchema==b.authSchema && a.descriptorTag==b.descriptorTag);
    }
}
}
int main(int argc,char** argv) {
    mailbox();concurrent_bridge();real_profile(argc==2?argv[1]:nullptr);
    std::printf("public_event_runtime_tests: %u checks, %u failures\n",checks,failures);return failures?1:0;
}
