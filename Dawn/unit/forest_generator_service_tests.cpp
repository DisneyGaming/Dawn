#include "../src/server/runtime/activity/forest_generator_service.h"
#include "../src/server/runtime/activity/haunted_forest_registries.h"
#include <cstdio>
#include <cstdlib>
namespace service=dawn::server::runtime::activity::forest_generator;
namespace fg=service::wire;
namespace hf=dawn::server::runtime::activity::haunted_forest::mode;
namespace roster=dawn::middleware::bap::activity_message::sensor_auth_update;
unsigned checks{};
void expect(bool value) {++checks;if(!value){std::fprintf(stderr,"failed check %u\n",checks);std::exit(1);}}
int main() {
    fg::State active;active.primary.overrides=fg::Enabled;active.primary.enabled=true;
    auto stopped=active;stopped.primary.enabled=false;
    std::array<service::Action,2> actions{{{1,active},{2,stopped}}};
    const service::Capability cap{&hf::kRegistries[0],98,actions};
    expect(service::valid(cap));
    service::Service empty;expect(empty.begin({1,{2}},3,{}));expect(empty.project(13).count==0);
    expect(empty.request({{1,{2}},3,1,1,0x34D23982,1,98},13)==service::Result::unsupported);
    service::Service source;
    expect(!source.begin({},3,{&cap,1}));expect(!source.begin({1,{2}},0,{&cap,1}));
    expect(source.begin({1,{2}},3,{&cap,1}));expect(!source.begin({1,{2}},3,{&cap,1}));
    actions[0].state.primary.enabled=false; // begin copies trusted state.
    expect(source.project(13).count==0);
    service::Command request{{1,{2}},3,1,1,0x34D23982,1,98};
    for(unsigned field=0;field<6;++field) {
        auto bad=request;
        if(field==0)bad.owner.sessionId=7;
        if(field==1)bad.owner.incarnation.value=7;
        if(field==2)bad.boot=7;
        if(field==3)bad.expectedRevision=7;
        if(field==4)bad.registry=0x34D23983;
        if(field==5)bad.slot=97;
        const auto verdict=source.request(bad,13);
        expect(verdict==service::Result::stale || verdict==service::Result::unsupported);
        expect(source.project(13).count==0);expect(source.revision()==1);expect(source.last_request()==0);
    }
    expect(source.request(request,12)==service::Result::stale);
    expect(source.request(request,13)==service::Result::accepted);
    const auto first=source.project(13);expect(first.count==1);expect(first.entries[0].state==active);
    expect(source.project(12).count==0);expect(source.revision()==2);
    request.expectedRevision=2;expect(source.request(request,13)==service::Result::duplicate);
    request.request=2;expect(source.request(request,13)==service::Result::unchanged);expect(source.revision()==3);
    request.expectedRevision=3;request.request=3;request.action=999;
    expect(source.request(request,13)==service::Result::unsupported);expect(source.last_request()==2);
    request.action=2;expect(source.request(request,13)==service::Result::accepted);
    expect(source.project(13).entries[0].state==stopped);
    source={};expect(source.project(13).count==0);expect(source.request(request,13)==service::Result::stale);
    expect(source.begin({1,{4}},3,{&cap,1}));expect(source.request(request,13)==service::Result::stale);

    std::array<std::uint8_t,126> types{},flags{};std::array<std::uint16_t,126> indices{};
    expect(hf::kRegistries[0].slots.size()==types.size());
    for(std::size_t i=0;i<types.size();++i) {
        const auto& slot=hf::kRegistries[0].slots[i];types[i]=slot.type;flags[i]=slot.flags();indices[i]=slot.index;
    }
    const std::array<std::uint32_t,1> keys{0x34D23982};std::array<std::uint8_t,1> present{1};
    roster::BubbleSubBlock local{13,keys,present};roster::Roster table;
    table.groups[0]={0x34D23982,types,flags,indices};table.groupCount=1;table.bubbleSubBlocks={&local,1};
    expect(fg::valid(first,table,104));expect(!fg::valid(first,table,96));
    expect(fg::find(first,0x34D23982,37,98)!=nullptr);expect(fg::find(first,0x34D23982,37,97)==nullptr);
    expect(fg::find(first,0x34D23982,57,98)==nullptr);expect(fg::find(first,0x34D23983,37,98)==nullptr);
    for(unsigned field=0;field<8;++field) {
        auto invalid=first;
        if(field==0)invalid.entries[0].registry=0;
        if(field==1)invalid.entries[0].slot=127;
        if(field==2)invalid.entries[0].bubble=12;
        if(field==3)invalid.entries[0].state.primary.blockedCount=101;
        if(field==4)invalid.count=5;
        if(field==5){invalid.entries[1]=invalid.entries[0];invalid.count=2;}
        if(field==6)present[0]=0;
        if(field==7)local.bubble=12;
        expect(!fg::valid(invalid,table,104));present[0]=1;local.bubble=13;
    }
    auto wrong=cap;wrong.slot=57;expect(!service::valid(wrong));
    auto duplicate=std::array<service::Capability,2>{cap,cap};service::Service invalid;
    expect(!invalid.begin({1,{2}},3,duplicate));expect(invalid.revision()==0);
    // A named host parameter is resolved once. The default zero policy selects
    // one incarnation seed; ordinary refreshes and disable/enable retain it.
    actions[0].state=active;
    auto seeded=cap;seeded.seedParameter="forest.seed";
    const std::array<std::uint32_t,1> automatic{0},fixed{9001};
    service::Service run;
    expect(!run.begin({1,{2}},3,{&seeded,1}));
    expect(run.begin({1,{2}},3,{&seeded,1},automatic));
    request={{1,{2}},3,1,1,0x34D23982,1,98};
    expect(run.request(request,13)==service::Result::accepted);
    const auto seed=run.project(13).entries[0].state.primary.seed;expect(seed!=0);
    expect(run.project(13).entries[0].state.primary.overrides==(fg::Enabled|fg::Seed));
    for(int i=0;i<10;++i)expect(run.project(13).entries[0].state.primary.seed==seed);
    request.expectedRevision=2;request.request=2;request.action=2;
    expect(run.request(request,13)==service::Result::accepted);
    expect(run.project(13).entries[0].state.primary.seed==seed);
    service::Service replay,other,fixedRun;
    expect(replay.begin({1,{2}},3,{&seeded,1},automatic));
    expect(other.begin({1,{3}},3,{&seeded,1},automatic));
    expect(fixedRun.begin({1,{3}},3,{&seeded,1},fixed));
    request={{1,{2}},3,1,1,0x34D23982,1,98};
    expect(replay.request(request,13)==service::Result::accepted);
    request.owner.incarnation.value=3;
    expect(other.request(request,13)==service::Result::accepted);
    expect(fixedRun.request(request,13)==service::Result::accepted);
    expect(replay.project(13).entries[0].state.primary.seed==seed);
    expect(other.project(13).entries[0].state.primary.seed!=seed);
    expect(fixedRun.project(13).entries[0].state.primary.seed==9001);
    // The wire replaces the complete anchor block. Coordinate bindings must
    // preserve unrelated groups, weights, enabled flags and recipe fields.
    auto anchored=active;anchored.primary.overrides|=fg::Anchors;
    anchored.primary.anchors={{{3,2,0,true},{1,0,0,true},{1,1,0,true},{2,0,0,true}}};
    anchored.primary.anchors[1].weight=0.5F;anchored.primary.anchors[2].active=false;
    anchored.primary.densityA=0.25F;anchored.secondary.seed=17;anchored.groups[4]=9;
    auto anchoredStop=anchored;anchoredStop.primary.enabled=false;
    const std::array<service::Action,2> anchoredActions{{{1,anchored},{2,anchoredStop}}};
    auto coordinateCap=seeded;coordinateCap.actions=anchoredActions;
    coordinateCap.anchorParameters[3]={"entry.column","entry.height"};
    expect(service::valid(coordinateCap));
    auto missingBlock=coordinateCap;missingBlock.actions=actions;
    expect(!service::valid(missingBlock));
    std::array<service::AnchorConfiguration,1> coordinates{};
    coordinates[0][3]={2,2};
    service::Service positioned;
    expect(!positioned.begin({1,{2}},3,{&coordinateCap,1},fixed));
    coordinates[0][3].height=128;
    expect(!positioned.begin({1,{2}},3,{&coordinateCap,1},fixed,coordinates));
    coordinates[0][3]={UINT32_MAX,2};
    expect(!positioned.begin({1,{2}},3,{&coordinateCap,1},fixed,coordinates));
    expect(positioned.revision()==0 && !positioned.owner());
    coordinates[0][3]={2,2};
    expect(positioned.begin({1,{2}},3,{&coordinateCap,1},fixed,coordinates));
    coordinates[0][3]={4,1}; // Later caller edits cannot move an active island.
    request={{1,{2}},3,1,1,0x34D23982,1,98};
    expect(positioned.request(request,13)==service::Result::accepted);
    auto expectedAnchored=anchored;expectedAnchored.primary.seed=9001;
    expectedAnchored.primary.overrides|=fg::Seed;expectedAnchored.primary.anchors[3].b=2;
    expect(positioned.project(13).entries[0].state==expectedAnchored);
    request.expectedRevision=2;request.request=2;request.action=2;
    expect(positioned.request(request,13)==service::Result::accepted);
    expectedAnchored.primary.enabled=false;
    expect(positioned.project(13).entries[0].state==expectedAnchored);
    positioned={};
    expect(positioned.begin({1,{3}},4,{&coordinateCap,1},fixed,coordinates));
    expect(positioned.request(request,13)==service::Result::stale);
    request={{1,{3}},4,1,1,0x34D23982,1,98};
    expect(positioned.request(request,13)==service::Result::accepted);
    expectedAnchored.primary.enabled=true;expectedAnchored.primary.anchors[3].a=4;
    expectedAnchored.primary.anchors[3].b=1;
    expect(positioned.project(13).entries[0].state==expectedAnchored);
    std::printf("forest generator service: %u checks, zero failures\n",checks);
}
