#include "../src/server/runtime/activity/forest_generator_service.h"
#include "../src/server/runtime/activity/haunted_forest_registries.h"
#include "../src/state/activity/coo/native_generator_authority.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
namespace service=sunrise::server::runtime::activity::forest_generator;
namespace fg=service::wire;
namespace hf=sunrise::server::runtime::activity::haunted_forest::mode;
namespace roster=sunrise::middleware::bap::activity_message::sensor_auth_update;
unsigned checks{};
void expect(bool value,const char* reason="") {
    ++checks;if(!value){std::fprintf(stderr,"failed check %u: %s\n",checks,reason);std::exit(1);}
}
struct PacketWriter final {
    std::size_t bits{};
    std::vector<std::pair<std::uint64_t,unsigned>> fields{};
    std::size_t bit_count() const noexcept {return bits;}
    bool write(std::uint64_t value,std::size_t width) {
        bits+=width;fields.push_back({value,static_cast<unsigned>(width)});return true;
    }
};
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
    auto cycleCapability=seeded;cycleCapability.allowCycles=true;
    service::Service cycles;
    expect(cycles.begin({1,{2}},3,{&cycleCapability,1},fixed));
    service::CycleCommand cycle{{1,{2}},3,1,10,0x34D23982,98,1,1};
    expect(cycles.begin_cycle(cycle,13)==service::Result::accepted);
    const auto firstCycle=cycles.project(13);
    expect(firstCycle.count==1 && firstCycle.entries[0].state.primary.enabled
        && firstCycle.entries[0].state.primary.seed!=9001 && firstCycle.entries[0].state.primary.seed!=0);
    const auto firstCycleState=firstCycle.entries[0].state;const auto firstCycleRevision=cycles.revision();
    cycle.expectedRevision=firstCycleRevision;
    expect(cycles.begin_cycle(cycle,13)==service::Result::duplicate
        && cycles.revision()==firstCycleRevision && cycles.last_request()==10
        && cycles.project(13).entries[0].state==firstCycleState);
    auto invalidCycle=cycle;invalidCycle.request=11;
    expect(cycles.begin_cycle(invalidCycle,13)==service::Result::stale
        && cycles.revision()==firstCycleRevision && cycles.project(13).entries[0].state==firstCycleState);
    cycle={ {1,{2}},3,firstCycleRevision,11,0x34D23982,98,1,2};
    expect(cycles.begin_cycle(cycle,13)==service::Result::accepted);
    const auto secondCycle=cycles.project(13).entries[0].state.primary.seed;
    expect(secondCycle!=firstCycleState.primary.seed && cycles.revision()==firstCycleRevision+1);
    cycle={ {1,{2}},3,cycles.revision(),12,0x34D23982,98,1,3};
    expect(cycles.begin_cycle(cycle,13)==service::Result::accepted);
    const auto thirdCycle=cycles.project(13).entries[0].state.primary.seed;
    expect(thirdCycle!=secondCycle && thirdCycle!=firstCycleState.primary.seed);
    const std::array<std::uint32_t,3> cycleSeeds{firstCycleState.primary.seed,secondCycle,thirdCycle};
    const auto beforeStale=cycles.project(13).entries[0].state;const auto staleRevision=cycles.revision();
    auto staleCycle=cycle;staleCycle.expectedRevision=staleRevision;staleCycle.request=13;staleCycle.cycle=1;
    expect(cycles.begin_cycle(staleCycle,13)==service::Result::stale
        && cycles.revision()==staleRevision && cycles.project(13).entries[0].state==beforeStale);
    service::Command disable{{1,{2}},3,cycles.revision(),14,0x34D23982,2,98};
    expect(cycles.request(disable,13)==service::Result::accepted);
    expect(!cycles.project(13).entries[0].state.primary.enabled
        && cycles.project(13).entries[0].state.primary.seed==thirdCycle);
    service::Command enable{{1,{2}},3,cycles.revision(),15,0x34D23982,1,98};
    expect(cycles.request(enable,13)==service::Result::accepted);
    expect(cycles.project(13).entries[0].state.primary.enabled
        && cycles.project(13).entries[0].state.primary.seed==thirdCycle);
    auto badCycle=cycle;badCycle.expectedRevision=cycles.revision();badCycle.request=16;badCycle.cycle=4;
    badCycle.action=2;
    const auto beforeInvalid=cycles.project(13).entries[0].state;const auto invalidRevision=cycles.revision();
    expect(cycles.begin_cycle(badCycle,13)==service::Result::unsupported
        && cycles.revision()==invalidRevision && cycles.project(13).entries[0].state==beforeInvalid);
    auto wrongBubble=cycle;wrongBubble.expectedRevision=cycles.revision();wrongBubble.request=17;wrongBubble.cycle=4;
    expect(cycles.begin_cycle(wrongBubble,12)==service::Result::stale
        && cycles.revision()==invalidRevision && cycles.project(13).entries[0].state==beforeInvalid);
    service::Service nonOptIn;
    expect(nonOptIn.begin({1,{2}},3,{&seeded,1},fixed));
    service::CycleCommand rejected{{1,{2}},3,1,1,0x34D23982,98,1,1};
    expect(nonOptIn.begin_cycle(rejected,13)==service::Result::unsupported
        && nonOptIn.revision()==1 && nonOptIn.project(13).count==0);
    service::Service cycleReplay;
    expect(cycleReplay.begin({1,{2}},3,{&cycleCapability,1},fixed));
    for(std::uint64_t round=1;round<=3;++round) {
        service::CycleCommand replayCommand{{1,{2}},3,cycleReplay.revision(),20+round,
            0x34D23982,98,1,round};
        expect(cycleReplay.begin_cycle(replayCommand,13)==service::Result::accepted);
        expect(cycleReplay.project(13).entries[0].state.primary.seed
            ==cycleSeeds[round-1]);
    }
    expect(cycleReplay.project(13).entries[0].state.primary.seed==thirdCycle);
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
    // The route contract is independent of mission IDs and maps both pairs of
    // native side groups through the same resolver used by admission.
    fg::State routeState=active;
    routeState.primary.overrides=fg::Enabled;
    routeState.primary.seed=41;routeState.primary.densityA=.25F;routeState.primary.densityB=.5F;
    routeState.primary.scalarOverrides={1,2,3,4,5};routeState.primary.blockedCount=1;
    routeState.primary.blockedCells[0]={7,8,9};
    routeState.secondary.seed=73;routeState.secondary.densityA=.75F;routeState.reportedSeed=99;
    routeState.areas[3]=4;routeState.groups[5]=6;
    const fg::Route eastWest{{3,2},{fg::Side::positiveX,2,1,true},{fg::Side::negativeX,0,0,false},{1,0,0,1}};
    auto eastWestResolved=routeState;expect(fg::resolve_route(eastWest,eastWestResolved));
    auto expectedEastWest=routeState;
    expectedEastWest.primary.anchors={{{2,1,0.F,true},{0,0,1.F,false},{-1,0,0.F,false},{-1,1,0.F,false}}};
    expectedEastWest.primary.overrides=static_cast<std::uint8_t>(expectedEastWest.primary.overrides|fg::Anchors);
    expect(eastWestResolved==expectedEastWest,
        "route resolution changes only primary anchors and the Anchors override");
    const fg::Route northSouth{{3,3},{fg::Side::positiveY,1,0,true},{fg::Side::negativeY,2,2,true},{0,1,0,1}};
    auto northSouthResolved=routeState;expect(fg::resolve_route(northSouth,northSouthResolved));
    expect(northSouthResolved.primary.anchors[2]==fg::Anchor{1,0,0.F,true}
        && northSouthResolved.primary.anchors[3]==fg::Anchor{2,2,1.F,true}
        && northSouthResolved.primary.anchors[0].a==-1
        && northSouthResolved.primary.anchors[1].a==-1,
        "north/south route selects the two requested sides and one exit goal");
    auto opaqueHeights=eastWest;opaqueHeights.unusedHeights[2]=-1;opaqueHeights.unusedHeights[3]=100;
    auto opaqueResolved=routeState;
    expect(fg::valid(opaqueHeights) && fg::resolve_route(opaqueHeights,opaqueResolved)
        && opaqueResolved.primary.anchors[0]==fg::Anchor{2,1,0.F,true}
        && opaqueResolved.primary.anchors[1]==fg::Anchor{0,0,1.F,false}
        && opaqueResolved.primary.anchors[2]==fg::Anchor{-1,-1,0.F,false}
        && opaqueResolved.primary.anchors[3]==fg::Anchor{-1,100,0.F,false},
        "unused heights accept opaque signed metadata while selected coordinates remain valid");
    for(unsigned invalidKind=0;invalidKind<7;++invalidKind) {
        auto badRouteConfig=eastWest;
        if(invalidKind==0)badRouteConfig.entrance.side=static_cast<fg::Side>(4);
        if(invalidKind==1)badRouteConfig.exit.side=badRouteConfig.entrance.side;
        if(invalidKind==2)badRouteConfig.entrance.column=3;
        if(invalidKind==3)badRouteConfig.entrance.height=2;
        if(invalidKind==4)badRouteConfig.grid.columns=0;
        if(invalidKind==5)badRouteConfig.grid.heights=129;
        if(invalidKind==6)badRouteConfig.exit.height=-1;
        auto unchanged=eastWestResolved;
        expect(!fg::resolve_route(badRouteConfig,unchanged) && unchanged==eastWestResolved);
    }

    namespace native=sunrise::state::activity::coo::native_generator;
    native::Request baseRequest{};baseRequest.seed=12345;baseRequest.values[0]=6;
    baseRequest.topology=native::kAuthoredTopologies;baseRequest.regions[0]=9;baseRequest.groups[0]=7;
    native::Request routedRequest{};
    expect(native::build_route_request(baseRequest,eastWest,routedRequest)
        && routedRequest.selectAnchors && routedRequest.seed==12345
        && routedRequest.values[0]==6 && routedRequest.topology==native::kAuthoredTopologies
        && routedRequest.regions[0]==9 && routedRequest.groups[0]==7
        && routedRequest.anchors[1].column==0 && routedRequest.anchors[1].height==0
        && routedRequest.anchors[1].progress==1.F && !routedRequest.anchors[1].enabled,
        "legacy request builder uses the shared east/west route resolver");
    PacketWriter packet;expect(native::write_activation(packet,routedRequest)
        && packet.bits==native::kMinimumBits && packet.fields.size()>89
        && packet.fields[28]==std::pair<std::uint64_t,unsigned>{0,32}
        && packet.fields[55]==std::pair<std::uint64_t,unsigned>{0,7}
        && packet.fields[56]==std::pair<std::uint64_t,unsigned>{12345,32}
        && packet.fields[57]==std::pair<std::uint64_t,unsigned>{9,8}
        && packet.fields[89]==std::pair<std::uint64_t,unsigned>{7,8},
        "full route packet preserves the second record and activation tail");
    auto invalidOutput=baseRequest;
    const bool rejectedRequest=!native::build_route_request(baseRequest,
        fg::Route{{0,2},eastWest.entrance,eastWest.exit,{0,0,0,0}},invalidOutput);
    bool anchorsUnchanged=true;
    for(std::size_t i=0;i<native::kAnchorCount;++i)
        anchorsUnchanged&=invalidOutput.anchors[i].column==baseRequest.anchors[i].column
            && invalidOutput.anchors[i].height==baseRequest.anchors[i].height
            && invalidOutput.anchors[i].progress==baseRequest.anchors[i].progress
            && invalidOutput.anchors[i].enabled==baseRequest.anchors[i].enabled;
    expect(rejectedRequest && invalidOutput.seed==baseRequest.seed && anchorsUnchanged
        && invalidOutput.topology==baseRequest.topology,
        "invalid route builder leaves its output request unchanged");

    const std::array<service::Action,1> routedActions{{{1,routeState,eastWest}}};
    auto routedCapability=cap;routedCapability.actions=routedActions;
    service::Service routedService;
    expect(service::valid(routedCapability)
        && routedService.begin({1,{2}},3,{&routedCapability,1}));
    request={{1,{2}},3,1,1,0x34D23982,1,98};
    expect(routedService.request(request,13)==service::Result::accepted);
    const auto routedState=routedService.project(13).entries[0].state;
    expect(routedState.primary.anchors==eastWestResolved.primary.anchors
        && routedState.primary.anchors[1].a==0 && !routedState.primary.anchors[1].active,
        "service admission applies the same route policy, including a disabled exit");

    auto routedWithParameters=routedCapability;
    routedWithParameters.anchorParameters[0]={"east.column","east.height"};
    expect(service::valid(routedWithParameters));
    auto absentParameter=routedWithParameters;absentParameter.anchorParameters[2]={"north.column","north.height"};
    expect(!service::valid(absentParameter));
    std::array<service::AnchorConfiguration,1> routeCoordinates{};routeCoordinates[0][0]={2,1};
    service::Service positionedRoute;
    expect(positionedRoute.begin({1,{2}},3,{&routedWithParameters,1},{},routeCoordinates));
    auto badCoordinates=routeCoordinates;badCoordinates[0][0]={3,1};
    service::Service rejectedRoute;
    expect(!rejectedRoute.begin({1,{2}},3,{&routedWithParameters,1},{},badCoordinates)
        && rejectedRoute.revision()==0 && !rejectedRoute.owner(),
        "routed coordinate bounds fail admission atomically");
    badCoordinates=routeCoordinates;badCoordinates[0][0]={2,2};
    expect(!rejectedRoute.begin({1,{2}},3,{&routedWithParameters,1},{},badCoordinates)
        && rejectedRoute.revision()==0 && !rejectedRoute.owner(),
        "routed height bounds fail admission atomically");
    auto badRouteActions=routedActions;auto badRoute=eastWest;badRoute.grid.columns=0;
    badRouteActions[0].route=badRoute;auto badRouteCapability=routedCapability;badRouteCapability.actions=badRouteActions;
    service::Service badRouteService;
    expect(!badRouteService.begin({1,{2}},3,{&badRouteCapability,1})
        && badRouteService.revision()==0 && !badRouteService.owner(),
        "invalid routed capability cannot commit ownership or bindings");
    std::printf("forest generator service: %u checks, zero failures\n",checks);
}
