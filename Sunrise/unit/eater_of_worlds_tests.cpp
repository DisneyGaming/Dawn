#include "../src/state/activity/eater_of_worlds/controller.h"
#include "../src/state/activity/eater_of_worlds/authority.h"
#include "../src/state/activity/eater_of_worlds/reactor_combat.h"
#include "../src/state/activity/eater_of_worlds/doors.h"
#include "../src/client/hooks/bootflow/eater_cranium_deferred.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <set>
#include <tuple>
namespace m=sunrise::state::activity::eater_of_worlds;
namespace coo=sunrise::state::activity::coo;
int eater_platform_contact_native_tests();
unsigned checks{};
static void check(bool ok,const char* message) {++checks;if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
static void deferred_carry_ordering() {
    namespace deferred=sunrise::client::hooks::bootflow::eater_cranium_deferred;
    deferred::Queue<unsigned,4> first,second;std::array<unsigned,8> seen{};std::size_t count{};
    const auto visit=[&](unsigned value){check(count<seen.size(),"deferred delivery stays bounded");seen[count++]=value;};
    check(!first.defer(1) && !first.begin(false),"carry events cannot defer outside a native station scope");
    const auto outer=first.begin(true),inner=first.begin(true);
    check(first.defer(11) && first.pending()==1,"actual nested native drop is retained");
    check(first.cancel([](unsigned value){return value==11;}),"qualified re-pickup cancels its deferred drop");
    check(first.finish(inner,visit) && count==0 && first.active(),"nested station completion does not drain outer queue");
    check(first.finish(outer,visit) && count==0 && !first.active(),"drop then re-pickup emits no deferred drop");
    const auto a=first.begin(true),b=second.begin(true);
    check(first.defer(21) && second.defer(22),"independent thread queues retain separate drops");
    check(!first.finish(b,visit) && !second.finish(a,visit) && count==0,"a scope cannot drain another thread queue");
    check(first.finish(a,visit) && count==1 && seen[0]==21 && second.pending()==1,"one thread completion preserves other thread pending event");
    check(second.finish(b,visit) && count==2 && seen[1]==22,"other thread delivers its own event");
    const auto stale=first.begin(true);check(first.defer(31),"queue pre-reset native drop");first.reset();
    const auto fresh=first.begin(true);check(first.defer(32),"queue new native drop after reset");
    check(!first.finish(stale,visit) && first.pending()==1,"stale scope cannot drain a new epoch");
    check(first.finish(fresh,visit) && count==3 && seen[2]==32,"reset retires prior deferred event");
    const auto bounded=first.begin(true);
    for(unsigned value=40;value<44;++value) check(first.defer(value),"deferred queue admits its bounded capacity");
    check(!first.defer(44) && first.pending()==4,"overflow cannot overwrite a retained native event");
    check(first.finish(bounded,visit) && count==7 && seen[3]==40 && seen[6]==43,"bounded queue retains event order");
    check(!first.finish(bounded,visit) && count==7,"completed scope cannot replay drops");
}
struct Wire {
    std::size_t bits{};bool valid{true};
    std::size_t bit_count() const noexcept {return bits;}
    bool write(std::uint64_t value,unsigned width) noexcept {
        if(width==0 || width>64 || (width<64 && value>=(1ULL<<width))) valid=false;
        bits+=width;return valid;
    }
};
#include "eater_arrival_tests.inl"
#include "eater_combat_tests.inl"
#include "eater_door_native_tests.inl"
#include "eater_traversal_tests.inl"
#include "eater_source_retirement_tests.inl"
static std::string capability(coo::Asset asset,coo::Operation operation,unsigned argument) {
    for(const auto& c:m::kCapabilities) if(c.spec.asset==asset && c.spec.operation==operation
        && c.spec.argument==argument) return std::string(c.id);
    check(false,"native observation test capability exists");return {};
}
static void carry_observations(const std::string& shipped) {
    const auto first=m::find(0x91264981U,4,116)->asset;
    const auto second=m::find(0x91264981U,4,117)->asset;
    check(m::cranium_index(first)<std::size(m::kCraniumBindings)
        && m::cranium_index(second)<std::size(m::kCraniumBindings),"craniums have package-proved carry components");
    auto script=shipped;
    const auto begin=script.find("local arrival=graph("),end=script.find("local reactorSteps=");
    check(begin!=std::string::npos && end>begin,"isolate native carry observation graph");
    script.replace(begin,end-begin,
        "local arrival=graph(\"arrival\",\"Native carry fixture\",sequence("
        "step(\"request\",parallel(\""+capability(first,coo::Operation::device,1)+"\",\""
        +capability(second,coo::Operation::device,1)+"\")),"
        "step(\"held\",\""+capability(first,coo::Operation::observation,3)+"\"),"
        "step(\"dropped\",\""+capability(first,coo::Operation::observation,4)+"\")))\n");
    std::string error;auto document=coo::script::MissionDocument::parse_lua(script,m::kProfile,error);
    check(document && m::valid_document(document->views()),"fixture uses registered pickup and drop observations");
    auto controller=std::make_unique<m::Controller>();check(controller->select(document->views(),92),"select carry fixture");
    auto frame=controller->update(92,1000,true,48);const auto owner=controller->owner();
    const auto create=[&](coo::Asset asset,std::uint32_t entity,std::uint32_t serial) {
        check(controller->prepared(owner,asset),"prepare requested native cranium");
        const auto generation=controller->frame().native[m::asset_index(asset)].generation;
        check(controller->object({{92,generation},asset,entity,serial}),"bind actual cranium creation");
        return m::CraniumReceipt{{92,generation},asset,0x20000ULL+entity*0x100ULL,entity,serial,entity+200};
    };
    const auto a=create(first,41,51),b=create(second,42,52);
    check(!controller->cranium(a,false,71),"initial ground state is not a player drop");
    check(!controller->cranium(a,true,UINT32_MAX),"pickup requires a native authenticated holder");
    auto invalid=a;invalid.component=UINT32_MAX;
    check(!controller->cranium(invalid,true,71),"pickup requires a live component handle");
    invalid=a;invalid.sourcePointer=0;check(!controller->cranium(invalid,true,71),"pickup requires source ownership");
    invalid=a;++invalid.serial;check(!controller->cranium(invalid,true,71),"pickup rejects a replaced entity salt");
    invalid=a;++invalid.generation.value;check(!controller->cranium(invalid,true,71),"pickup rejects a stale generation");
    check(controller->cranium(b,true,71) && controller->cranium(b,false,71),"other cranium has independent pickup and drop");
    frame=controller->update(92,1100,true,48);
    check(frame.section==0 && !frame.native[m::asset_index(first)].carried,"another cranium cannot satisfy required pickup");
    check(controller->cranium(a,true,71),"exact native pickup is accepted");
    check(!controller->cranium(a,true,71) && !controller->cranium(a,true,72),"duplicate pickup and direct holder replacement rejected");
    frame=controller->update(92,1200,true,48);
    check(frame.section==0 && frame.native[m::asset_index(first)].carried
        && !frame.native[m::asset_index(first)].dropped,"pickup leaves drop gate pending");
    invalid=a;++invalid.component;check(!controller->cranium(invalid,false,71),"another carry component cannot drop current cranium");
    invalid=a;++invalid.sourcePointer;check(!controller->cranium(invalid,false,71),"another source pointer cannot drop current cranium");
    check(!controller->cranium(a,false,72),"drop must match previously authenticated holder");
    static_cast<void>(controller->update(92,1300,true,16));
    check(!controller->cranium(a,false,71),"unloaded area cannot supply carry state");
    static_cast<void>(controller->update(92,1400,true,48));
    check(controller->cranium(b,true,71),"dropped cranium can be picked up again");
    const m::HealthReceipt health{b.generation,b.source,b.sourcePointer,b.entity,b.serial,400};
    check(controller->health(health,false) && controller->health(health,true),"carried cranium can be destroyed by real health transition");
    check(!controller->frame().native[m::asset_index(second)].carried
        && !controller->frame().native[m::asset_index(second)].dropped,"destruction clears held state without fabricating a drop");
    check(!controller->cranium(b,false,71) && !controller->cranium(b,true,71),"destroyed cranium rejects late carry updates");
    check(controller->cranium(a,false,71),"exact current holder drop is accepted");
    check(!controller->cranium(a,false,71),"drop is deduplicated");
    frame=controller->update(92,1500,true,48);
    check(frame.section==1 && !frame.finished,"ordered pickup and drop release only the fixture gate");
    controller->reset();check(controller->select(document->views(),92),"restart carry fixture with fresh lifecycle lease");
    static_cast<void>(controller->update(92,1600,true,48));
    check(!controller->cranium(a,true,71) && !controller->cranium(a,false,71),"previous attempt carry state cannot cross reset");
}
static void health_observations(const std::string& shipped) {
    const auto mine=m::find(0x91264981U,4,12)->asset;
    const auto missile=m::find(0xE8D290A0U,4,8)->asset;
    check(m::health_index(mine)<std::size(m::kHealthBindings)
        && m::health_index(missile)<std::size(m::kHealthBindings),"mine and missile have package-proved health components");
    auto script=shipped;
    const auto begin=script.find("local arrival=graph("),end=script.find("local reactorSteps=");
    check(begin!=std::string::npos && end>begin,"isolate native health observation graph");
    script.replace(begin,end-begin,
        "local arrival=graph(\"arrival\",\"Native health fixture\",sequence("
        "step(\"request\",parallel(\""+capability(mine,coo::Operation::device,1)+"\",\""
        +capability(missile,coo::Operation::device,1)+"\")),"
        "step(\"required_mine\",\""+capability(mine,coo::Operation::observation,2)+"\")))\n");
    std::string error;auto document=coo::script::MissionDocument::parse_lua(script,m::kProfile,error);
    check(document && m::valid_document(document->views()),"fixture uses real registered health observation");
    auto controller=std::make_unique<m::Controller>();check(controller->select(document->views(),91),"select health fixture");
    auto frame=controller->update(91,1000,true,48);const auto owner=controller->owner();
    const auto create=[&](coo::Asset asset,std::uint32_t entity,std::uint32_t serial) {
        check(controller->prepared(owner,asset),"prepare requested native destructible");
        const auto generation=controller->frame().native[m::asset_index(asset)].generation;
        check(controller->object({{91,generation},asset,entity,serial}),"bind actual destructible creation");
        return m::HealthReceipt{{91,generation},asset,0x10000ULL+entity*0x100ULL,entity,serial,entity+100};
    };
    const auto mineReceipt=create(mine,21,31),missileReceipt=create(missile,22,32);
    check(!controller->health(mineReceipt,true),"initial dead state without a live sample cannot clear a target");
    check(controller->health(mineReceipt,false),"bind living native mine");
    check(!controller->health(mineReceipt,false),"repeat alive sample is idempotent");
    check(controller->health(missileReceipt,false) && controller->health(missileReceipt,true),"observe genuine missile destruction independently");
    frame=controller->update(91,1100,true,48);
    check(frame.section==0 && !frame.native[m::asset_index(mine)].destroyed,"missile destruction cannot satisfy required mine");
    auto stale=mineReceipt;++stale.serial;check(!controller->health(stale,true),"reused entity index with another salt is rejected");
    stale=mineReceipt;++stale.health;check(!controller->health(stale,true),"replaced health handle is rejected");
    stale=mineReceipt;++stale.generation.value;check(!controller->health(stale,true),"prior or unrelated source generation is rejected");
    stale=mineReceipt;++stale.sourcePointer;check(!controller->health(stale,true),"another source pointer cannot replace current health binding");
    static_cast<void>(controller->update(91,1200,true,16));
    check(!controller->health(mineReceipt,true),"another current area cannot supply target destruction");
    static_cast<void>(controller->update(91,1300,true,48));
    check(controller->health(mineReceipt,true),"live-to-dead transition of exact required mine is accepted");
    check(!controller->health(mineReceipt,true) && !controller->health(mineReceipt,false),"destruction is terminal and deduplicated within attempt");
    frame=controller->update(91,1400,true,48);
    check(frame.section==1 && !frame.finished,"only exact target death releases the fixture gate, not raid completion");
    controller->reset();check(controller->select(document->views(),91),"restart health fixture with a new generation lease");
    static_cast<void>(controller->update(91,1500,true,48));
    check(!controller->health(mineReceipt,true),"previous attempt health receipt cannot clear a new target");
}
static void station_observations(const std::string& shipped) {
    const auto station=m::find(0x91264981U,4,177)->asset,other=m::find(0x91264981U,4,179)->asset;
    const auto first=m::find(0x91264981U,4,116)->asset,second=m::find(0x91264981U,4,117)->asset;
    const auto foreign=m::find(0xE8D290A0U,4,158)->asset;
    check(m::station_index(station)<std::size(m::kStationBindings),"station has exact native interaction binding");
    auto script=shipped;
    const auto begin=script.find("local arrival=graph("),end=script.find("local reactorSteps=");
    check(begin!=std::string::npos && end>begin,"isolate station observation graph");
    script.replace(begin,end-begin,
        "local arrival=graph(\"arrival\",\"Native station fixture\",sequence("
        "step(\"request\",parallel(\""+capability(station,coo::Operation::device,1)+"\",\""
        +capability(other,coo::Operation::device,1)+"\",\""+capability(first,coo::Operation::device,1)+"\",\""
        +capability(second,coo::Operation::device,1)+"\",\""+capability(foreign,coo::Operation::device,1)+"\")),"
        "step(\"use_station\",\""+capability(station,coo::Operation::observation,5)+"\")))\n");
    std::string error;auto document=coo::script::MissionDocument::parse_lua(script,m::kProfile,error);
    check(document && m::valid_document(document->views()),"fixture uses compiled native station observation");
    auto controller=std::make_unique<m::Controller>();check(controller->select(document->views(),93),"select station fixture");
    static_cast<void>(controller->update(93,1000,true,48));const auto owner=controller->owner();
    const auto create=[&](coo::Asset asset,std::uint32_t entity,std::uint32_t serial) {
        check(controller->prepared(owner,asset),"prepare exact station fixture source");
        const auto generation=controller->frame().native[m::asset_index(asset)].generation;
        check(controller->object({{93,generation},asset,entity,serial}),"bind actual station fixture entity");
        return m::CraniumReceipt{{93,generation},asset,0x30000ULL+entity*0x100ULL,entity,serial,entity+300};
    };
    const auto a=create(first,61,81),b=create(second,62,82),f=create(foreign,63,83);
    const auto s=create(station,64,84),t=create(other,65,85);
    const auto interaction=[&a](const m::CraniumReceipt& object) {
        return m::StationReceipt{object.generation,object.source,object.sourcePointer,
            object.entity,object.serial,object.component,1,0,1,a};
    };
    const auto required=interaction(s),otherUse=interaction(t);
    check(!controller->station(required,91),"station use requires a held current cranium");
    check(controller->cranium(f,true,91) && !controller->station(required,91),"Argos cranium cannot supply Barrier station use");
    check(controller->cranium(a,true,91) && controller->cranium(b,true,91),"bind two distinct fixture craniums");
    check(!controller->station(required,91),"ambiguous multiple carried craniums cannot select a station input");
    check(controller->cranium(b,false,91),"remove ambiguity using actual drop");
    check(!controller->station(required,92) && !controller->station(required,UINT32_MAX),"requester must match the authenticated carrier");
    auto invalid=required;invalid.consumedAfter=0;check(!controller->station(invalid,91),"prompt or unconsumed request is not completed use");
    invalid=required;invalid.consumedBefore=1;check(!controller->station(invalid,91),"previously consumed request is not a new edge");
    invalid=required;invalid.consumedBefore=-1;check(!controller->station(invalid,91),"uninitialized consumed counter rejected");
    invalid=required;++invalid.serial;check(!controller->station(invalid,91),"replaced station entity rejected");
    invalid=required;++invalid.generation.value;check(!controller->station(invalid,91),"stale station generation rejected");
    invalid=required;invalid.component=UINT32_MAX;check(!controller->station(invalid,91),"station requires current native component");
    invalid=required;invalid.cranium=b;check(!controller->station(invalid,91),"station must use the same cranium captured before native consumption");
    invalid=required;++invalid.cranium.component;check(!controller->station(invalid,91),"station cannot substitute another carry component");
    invalid=required;++invalid.cranium.generation.value;check(!controller->station(invalid,91),"station rejects previous carry generation");
    check(controller->station(otherUse,91),"independent other station use accepted");
    auto frame=controller->update(93,1100,true,48);
    check(frame.section==0 && !frame.native[m::asset_index(station)].used,"other station cannot complete required station gate");
    check(!controller->station(otherUse,91),"same consumed station edge is deduplicated");
    invalid=otherUse;invalid.requested=invalid.consumedAfter=2;invalid.consumedBefore=1;++invalid.sourcePointer;
    check(!controller->station(invalid,91),"new request on replacement station source rejected");
    invalid=otherUse;invalid.requested=invalid.consumedAfter=2;invalid.consumedBefore=1;++invalid.component;
    check(!controller->station(invalid,91),"new request on replacement station component rejected");
    static_cast<void>(controller->update(93,1200,true,16));
    check(!controller->station(required,91),"another current area cannot supply station use");
    static_cast<void>(controller->update(93,1300,true,48));
    check(controller->station(required,91),"exact consumed request while holding cranium accepted");
    frame=controller->frame();
    check(frame.native[m::asset_index(station)].usedCranium==m::asset_index(first)
        && frame.native[m::asset_index(first)].carried && !frame.native[m::asset_index(first)].dropped,
        "station records exact held cranium and preserves native carry state");
    check(controller->cranium(a,false,91),"native drop following consumed use is accepted independently");
    invalid=required;invalid.requested=invalid.consumedAfter=2;invalid.consumedBefore=1;
    check(!controller->station(invalid,91),"station cannot reuse a cranium after its drop");
    frame=controller->update(93,1400,true,48);
    check(frame.section==1 && !frame.finished,"consumed use releases fixture gate without claiming charging or victory");
    controller->reset();check(controller->select(document->views(),93),"station fixture restarts with fresh lease");
    static_cast<void>(controller->update(93,1500,true,48));
    check(!controller->station(required,91),"previous attempt station request rejected");
}
static void shipped_route(const std::string& source) {
    std::string error;
    auto document=coo::script::MissionDocument::parse_lua(source,m::kProfile,error);
    if(!document) std::fprintf(stderr,"Lua: %s\n",error.c_str());
    check(document && m::valid_document(document->views()),"shipped reactor-through-arena graph is authorized");
    check(document->views().parameter("platform_hold_ms") && document->views().parameter("platform_hold_ms")->value==500,"Lua exposes bounded native solo policy");
    auto invalidPolicy=source;const auto policyAt=invalidPolicy.find("platform_hold_ms=500");
    check(policyAt!=std::string::npos,"find authored solo policy");
    invalidPolicy.replace(policyAt,std::string("platform_hold_ms=500").size(),"platform_hold_ms=0");
    check(!coo::script::MissionDocument::parse_lua(invalidPolicy,m::kProfile,error),"Lua cannot escape native solo policy bounds");
    auto unknownPolicy=source;unknownPolicy.replace(policyAt,std::string("platform_hold_ms=500").size(),"invented_control=500");
    check(!coo::script::MissionDocument::parse_lua(unknownPolicy,m::kProfile,error),"Lua cannot invent a native control");
    check(std::size(m::kGroups)==19 && std::size(m::kSpawns)==75 && std::size(m::kVolumes)==73,"full recovered ownership and spatial catalog");
    std::set<std::tuple<std::uint32_t,unsigned,unsigned>> identities;unsigned descriptors{};
    for(const auto& g:m::kGroups) for(const auto& s:g.slots) {
        check(identities.emplace(g.key,s.type,s.index).second,"unique declared slot identity");
        check(s.flags==((s.sense!=UINT32_MAX?1:0)|(s.auth!=UINT32_MAX?2:0)),"flags reflect supported native schemas");
        if(s.tag!=UINT32_MAX) ++descriptors;
    }
    check(descriptors==891,"all client descriptors retained alongside host-only slots");
    auto controller=std::make_unique<m::Controller>();
    check(controller->select(document->views(),77),"select exact solo document");
    const auto first=controller->owner();
    check(!controller->update(77,1000,false,16).enabled,"wait for real world readiness");
    auto f=controller->update(77,1000,true,16);
    check(f.enabled && f.section==0 && !f.finished,"arrive at entrance without claiming progress");
    controller->monitor(0xA9E6185FU,3,true,1,static_cast<std::int32_t>(f.spawnGeneration+1));
    f=controller->update(77,1500,true,16);
    const auto door=m::find(0xA9E6185FU,23,1)->asset;
    check(!f.native[m::asset_index(door)].managed,"stale monitor cannot open door");
    controller->monitor(0xA9E6185FU,3,true,2,static_cast<std::int32_t>(f.spawnGeneration));
    f=controller->update(77,2000,true,16);
    check(!f.native[m::asset_index(door)].managed,"solo monitor rejects multiple participants");
    controller->monitor(0xA9E6185FU,3,true,1,static_cast<std::int32_t>(f.spawnGeneration));
    f=controller->update(77,2500,true,16);f=controller->update(77,2600,true,16);
    check(f.native[m::asset_index(door)].managed,"current single-player presence reaches door request");
    f=controller->update(77,3000,true,56);f=controller->update(77,3100,true,56);
    check(f.section==1 && f.waitingMechanic==0,"native route reaches the two entry doors before platforms");
    const auto floor=m::find(0x5654D7FDU,23,0)->asset;
    const auto crossing=m::find(0x5654D7FDU,23,1)->asset;
    check(!f.native[m::asset_index(floor)].managed && !f.native[m::asset_index(crossing)].managed,"entry doors wait for a real approach");
    controller->position(77,{5.F,1000.F,-145.F});
    for(unsigned tick=0;tick<3;++tick) f=controller->update(77,3200+tick*100,true,56);
    check(f.native[m::asset_index(floor)].active && !f.native[m::asset_index(crossing)].managed,"exact dropdown volume opens only the dropdown");
    controller->monitor(0x5654D7FDU,4,true,1,static_cast<std::int32_t>(f.spawnGeneration));
    for(unsigned tick=0;tick<4;++tick) f=controller->update(77,3500+tick*100,true,56);
    check(f.native[m::asset_index(crossing)].active && f.waitingMechanic==101,"second exact door approach opens iris and starts first path");
    check(!controller->died({77,1,2,f.spawnGeneration,3,0xE8D290A0U}),"unadmitted Argos death cannot complete raid");
    check(!controller->submitted(77,0x8155C312U,0x1408,m::kBank,0,f.spawnGeneration),"unrequested ending dialogue cannot complete raid");
    auto pending=f;
    f=controller->update(77,3600000,true,56);
    check(f.section==1 && !f.finished,"elapsed time cannot fabricate platform completion");
    const auto object=m::find(0x686321C8U,4,38)->asset;
    check(controller->prepared(first,object),"prepare only a requested native platform");
    f=controller->update(77,3600100,true,56);
    auto receipt=coo::ObjectReceipt{{77,f.native[m::asset_index(object)].generation},object,10,11,UINT32_MAX};
    check(controller->object(receipt),"exact creation receipt reaches native ready");
    auto activePlatform=controller->update(77,3600150,true,56);
    check(activePlatform.reactor.raised[0] && activePlatform.reactor.raised.count()==1,"path begins with exactly its first platform");
    check(m::body_bits(activePlatform,object.registry,4,object.slot)==573,"verified platform creation enables separate generic activation record");
    Wire platformWire;
    check(m::write_body(platformWire,activePlatform,object.registry,4,object.slot) && platformWire.bits==573,"platform activation uses the native dynamic component schema");
    auto replacement=receipt;replacement.serial=12;
    check(!controller->object(replacement),"salted object replacement cannot forge readiness");
    // Every offered body is executed through the native serializer contract.
    for(auto& s:pending.native) {s.managed=true;s.active=s.desired=s.prepared=true;s.generation=pending.spawnGeneration;s.position=1.F;}
    for(const auto& a:m::kAssets) {
        const auto bits=m::body_bits(pending,a.asset.registry,static_cast<std::uint8_t>(a.asset.type),a.asset.slot);
        if(!bits) continue;
        Wire wire;check(m::write_body(wire,pending,a.asset.registry,static_cast<std::uint8_t>(a.asset.type),a.asset.slot) && wire.valid && wire.bits==bits,"published native authority width and ranges");
    }
    controller->reset();check(controller->select(document->views(),77),"same run can acquire a fresh attempt");
    check(controller->owner().value>first.value,"reset preserves generation high-water mark");
    check(!controller->object(receipt),"prior-attempt object receipt rejected");
    check(!m::contains(m::kVolumes[0],{NAN,0,0}),"invalid positions rejected");
}
static void reactor_policy() {
    m::ReactorState state{};
    check(!state.begin(2,100),"cannot skip first path checkpoint");
    check(state.begin(1,100) && state.raised.count()==1,"path starts with one physical platform");
    check(!state.goal(1),"goal before all required activations cannot complete path");
    check(!state.contact(0,1000,500),"contact without applied native pose cannot activate");
    state.poseAcknowledged.set(0);
    check(!state.contact(0,1100,500) && !state.contact(0,1400,500),"sample gap resets dwell");
    check(!state.contact(0,1500,500),"short dwell cannot activate");
    state.interrupt();
    check(!state.contact(0,1800,500),"leaving before hold restarts dwell");
    for(std::uint64_t at=1900;at<2300;at+=100) check(!state.contact(0,at,500),"continuous dwell remains pending until hold duration");
    check(state.contact(0,2300,500) && state.next==1 && state.raised.count()==2,"valid dwell latches platform and raises next");
    check(!state.contact(0,2400,500),"repeat old platform cannot advance");
    const auto nextRevision=state.poseRevision[1];
    check(state.reset_attempt() && state.next==0 && state.raised.count()==1
        && state.poseRevision[1]>nextRevision && !state.poseAcknowledged[1],"retry lowers uncommitted path using fresh native revision");
    std::uint64_t at=3000;
    for(std::uint8_t path=1;path<=4;++path) {
        check(state.begin(path,100),"only next checkpoint path can start");
        for(std::size_t index=0;index<56;++index) {
            if(m::kReactorPlatforms[index].path+1!=path) continue;
            check(state.raised[index],"authored next platform is raised including nonmonotonic source slots");
            state.poseAcknowledged.set(index);
            check(!state.contact(index,at,500),"each platform needs its own fresh dwell");
            check(!state.contact(index,at,500),"duplicate timestamp does not add time");
            for(unsigned sample=1;sample<5;++sample) check(!state.contact(index,at+sample*100,500),"short samples cannot skip dwell");
            check(state.contact(index,at+500,500),"ordered exact platform activates");
            at+=700;
        }
        check(state.next==m::kPathLengths[path-1] && !state.completed[path-1],"all activations still require fresh goal presence");
        check(state.goal(path) && !state.goal(path),"goal commits path once");
        const auto kept=state.raised;
        check(!state.reset_attempt() && state.raised==kept,"completed path survives attempt reset");
    }
    check(state.completed.all() && state.activated.count()==56 && state.raised.count()==56,"all four authored paths retain 56 valid activations");
}
static void narrow_native_requests() {
    m::Frame frame{};const coo::Generation owner{71,9};
    const auto firstPlatform=m::platform_index(m::kReactorPlatforms[0].source);
    const auto firstAsset=m::asset_index(m::kReactorPlatforms[firstPlatform].source);
    check(firstPlatform<56 && firstAsset<std::size(m::kAssets),"narrow request fixture resolves first platform");
    auto object=m::object_request(frame,owner,firstAsset);
    check(object.owner==owner && !object.enabled && object.desired==m::NativeState{},
        "disabled frame projects identity without enabling native work");
    check(!m::object_request(frame,owner,std::size(m::kAssets)).enabled,
        "invalid asset index cannot enable object request");
    frame.enabled=true;
    check(!m::object_request(frame,{},firstAsset).enabled,"invalid owner cannot enable object request");
    frame.native[firstAsset].managed=frame.native[firstAsset].desired=true;
    frame.native[firstAsset].prepared=frame.native[firstAsset].active=true;
    frame.native[firstAsset].acknowledged=true;frame.native[firstAsset].generation=9;
    frame.reactor.poseRevision[firstPlatform]=17;frame.reactor.raised.set(firstPlatform);
    frame.reactor.poseAcknowledged.set(firstPlatform);
    object=m::object_request(frame,owner,firstAsset);
    check(object.enabled && object.desired==frame.native[firstAsset] && object.poseRevision==17
        && object.raised && object.poseAcknowledged,"platform object projection includes exact desired pose state");
    const auto ordinary=m::object_request(frame,owner,0);
    check(ordinary.enabled && !ordinary.poseRevision && !ordinary.raised && !ordinary.poseAcknowledged,
        "non-platform projection has no reactor pose state");
    frame.finished=true;check(!m::object_request(frame,owner,firstAsset).enabled,
        "finished frame disables object request");frame.finished=false;

    frame.reactor.path=1;frame.reactor.next=0;frame.reactor.attempt=3;frame.reactor.player=901;
    auto contact=m::contact_request(frame,owner);
    check(contact.ready && contact.object==object && contact.assetIndex==firstAsset
        && contact.platformIndex==firstPlatform && contact.path==1 && contact.next==0
        && contact.attempt==3 && contact.player==901,"contact projection selects exact ready platform");
    frame.reactor.poseAcknowledged.reset(firstPlatform);
    check(!m::contact_request(frame,owner).ready,"pending pose prevents contact work");
    frame.reactor.poseRevision[firstPlatform]=18;frame.reactor.poseAcknowledged.set(firstPlatform);
    frame.reactor.attempt=4;frame.reactor.player=902;
    contact=m::contact_request(frame,owner);
    check(contact.ready && contact.object.poseRevision==18 && contact.attempt==4 && contact.player==902,
        "new pose command attempt and player are projected without a Frame copy");
    frame.reactor.next=1;
    const auto secondPlatform=[] {
        for(std::size_t i=0;i<std::size(m::kReactorPlatforms);++i)
            if(m::kReactorPlatforms[i].path==0 && m::kReactorPlatforms[i].index==1) return i;
        return std::size(m::kReactorPlatforms);
    }();
    const auto secondAsset=m::asset_index(m::kReactorPlatforms[secondPlatform].source);
    frame.native[secondAsset]=frame.native[firstAsset];
    frame.reactor.poseRevision[secondPlatform]=19;frame.reactor.raised.set(secondPlatform);
    frame.reactor.poseAcknowledged.set(secondPlatform);
    contact=m::contact_request(frame,owner);
    check(contact.ready && contact.platformIndex==secondPlatform && contact.assetIndex==secondAsset
        && contact.object.poseRevision==19,"next transition selects the authored next platform");
    frame.reactor.player=UINT32_MAX;check(!m::contact_request(frame,owner).ready,
        "missing authenticated player prevents contact work");frame.reactor.player=902;
    frame.reactor.next=m::kPathLengths[0];contact=m::contact_request(frame,owner);
    check(!contact.ready && contact.assetIndex==std::size(m::kAssets) && contact.platformIndex==56,
        "completed path has no current contact target");
    frame.reactor.path=0;check(!m::contact_request(frame,owner).ready,
        "inactive path has no current contact target");
    frame.reactor.path=5;check(!m::contact_request(frame,owner).ready,
        "invalid path has no current contact target");
    frame.reactor.path=1;frame.reactor.next=1;frame.finished=true;
    check(!m::contact_request(frame,owner).ready,"finished frame disables contact request");
}
static void reactor_native_receipts(const std::string& source,bool observeHealth) {
    std::string error;auto document=coo::script::MissionDocument::parse_lua(source,m::kProfile,error);
    check(document!=nullptr,"parse shipped graph for full reactor receipt test");
    auto controller=std::make_unique<m::Controller>();constexpr std::uint64_t run=94;
    check(controller->select(document->views(),run),"select complete reactor receipt fixture");
    std::uint64_t now=1000;auto frame=controller->update(run,now,true,16);
    controller->monitor(0xA9E6185FU,3,true,1,static_cast<std::int32_t>(frame.spawnGeneration));
    for(unsigned i=0;i<3;++i) frame=controller->update(run,now+=100,true,16);
    for(unsigned i=0;i<3;++i) frame=controller->update(run,now+=100,true,56);
    controller->monitor(0x5654D7FDU,3,true,1,static_cast<std::int32_t>(frame.spawnGeneration));
    controller->monitor(0x5654D7FDU,4,true,1,static_cast<std::int32_t>(frame.spawnGeneration));
    for(unsigned i=0;i<5;++i) frame=controller->update(run,now+=100,true,56);
    check(frame.waitingMechanic==101,"door sequence reaches first reactor mechanic");
    std::uint32_t player=901;controller->player(run,player);
    if(observeHealth)
        check(controller->player_health(run,player,false),"native living player qualifies reactor occupancy");
    for(std::uint8_t path=1;path<=4;++path) {
        check(frame.waitingMechanic==100U+path,"shipped Lua starts each path in order");
        controller->monitor(0x686321C8U,static_cast<std::uint16_t>(106+path),true,1,static_cast<std::int32_t>(frame.spawnGeneration));
        check(!controller->frame().reactor.completed[path-1],"early genuine goal cannot clear required platforms");
        m::Point goalPoint{};bool goalFound{};
        for(const auto& volume:m::kVolumes) if(volume.asset.registry==0x686321C8U && volume.asset.slot==234+path) {
            for(const auto& vertex:volume.vertices) {goalPoint.x+=vertex.x;goalPoint.y+=vertex.y;}
            goalPoint.x/=static_cast<float>(volume.vertices.size());goalPoint.y/=static_cast<float>(volume.vertices.size());
            goalPoint.z=(volume.min.z+volume.max.z)*0.5F;goalFound=m::contains(volume,goalPoint);break;
        }
        check(goalFound,"recover interior point from exact authored goal polygon");
        controller->position(run,goalPoint);
        check(!controller->frame().reactor.completed[path-1],"early real goal position cannot skip required platforms");
        for(std::size_t i=0;i<56;++i) {
            if(m::kReactorPlatforms[i].path+1!=path) continue;
            const auto asset=m::kReactorPlatforms[i].source;
            check(controller->prepared(controller->owner(),asset),"requested platform accepts its native preparation");
            const coo::ObjectReceipt receipt{{run,controller->frame().native[m::asset_index(asset)].generation},asset,
                static_cast<std::uint32_t>(500+i),static_cast<std::uint32_t>(1500+i)};
            check(controller->object(receipt),"current salted platform creation is acknowledged");
            const auto revision=controller->frame().reactor.poseRevision[i];
            check(!controller->platform_pose({receipt,revision+1,1.F}),"unrequested movement revision cannot claim native application");
            auto stale=receipt;++stale.serial;
            check(!controller->platform_pose({stale,revision,1.F}),"replaced platform object cannot acknowledge movement");
            check(!controller->platform_pose({receipt,revision,0.F}),"closed pose cannot acknowledge raised platform");
            check(controller->platform_pose({receipt,revision,1.F}),"exact applied native raise revision accepted");
            check(!controller->platform_contact({receipt,now,player+1,true}),"another player cannot supply platform occupancy");
            for(unsigned sample=0;sample<=5;++sample) {
                frame=controller->update(run,now+=100,true,56);
                const bool activated=controller->platform_contact({receipt,now,player,true});
                check(activated==(sample==5),"only continuous half-second exact contact activates platform");
            }
            check(!controller->platform_contact({receipt,now,player,true}),"duplicate native callback cannot advance twice");
            if(observeHealth && i==13) {
                const auto oldTime=now;
                check(controller->player_health(run,player,true),"native live-to-dead transition resets current path even with same player handle");
                check(!controller->player_health(run,player,true),"repeated corpse samples do not repeat reset");
                frame=controller->frame();
                check(frame.reactor.next==0 && frame.reactor.completed[0]
                    && frame.reactor.activated.count()==13 && frame.checkpointSpawnSet==m::kReactorGoalSpawns[0]
                    && !frame.restricted,"death keeps prior checkpoint and temporarily permits native respawn");
                check(!controller->platform_contact({receipt,now+1,player,true}),"confirmed same-player death blocks platform occupancy");
                check(controller->player_health(run,player,false),"native respawn re-arms current path");
                check(!controller->platform_contact({receipt,oldTime,player,true}),"pre-death contact cannot replay into retry");
                for(unsigned sample=0;sample<=5;++sample) {
                    frame=controller->update(run,now+=100,true,56);
                    check(controller->platform_contact({receipt,now,player,true})==(sample==5),"respawned player must repeat unfinished path hold");
                }
            }
            if(observeHealth && i==24) {
                check(controller->player_health(run,player,true),"confirmed death precedes replacement-player policy check");
                check(!controller->platform_contact({receipt,now+1,player,true}),"confirmed same-player death remains a contact veto");
                controller->position(run,goalPoint);
                check(!controller->frame().reactor.completed[path-1],"confirmed same-player death cannot satisfy the path goal");
                ++player;controller->player(run,player);
                for(unsigned sample=0;sample<=5;++sample) {
                    frame=controller->update(run,now+=100,true,56);
                    check(controller->platform_contact({receipt,now,player,true})==(sample==5),
                        "authenticated replacement player is not blocked by stale previous-player death");
                }
            }
        }
        frame=controller->frame();
        check(!frame.reactor.completed[path-1],"early goal latch is not reused after final platform");
        controller->monitor(0x686321C8U,static_cast<std::uint16_t>(106+path),true,1,static_cast<std::int32_t>(frame.spawnGeneration));
        check(!controller->frame().reactor.completed[path-1],"delayed same-generation monitor cannot replace current goal presence");
        controller->position(run,goalPoint);
        if(observeHealth && path==3)
            check(controller->frame().reactor.completed[path-1],
                "replacement player's authenticated goal position is not blocked by stale previous-player death");
        if(observeHealth && (path==1 || path==4)) {
            if(controller->frame().reactor.healthPlayer!=player)
                check(controller->player_health(run,player,false),"current replacement player establishes health before death test");
            check(controller->player_health(run,player,true),"death immediately after committed goal is accepted");
            frame=controller->update(run,now+=100,true,56);
            check(frame.section==1 && !frame.restricted && frame.checkpointSpawnSet==m::kReactorGoalSpawns[path-1],
                "goal-to-next-phase death retains checkpoint and cannot lock native respawn");
            if(path==4) {
                ++player;controller->player(run,player);
                check(controller->frame().reactor.dead && controller->frame().reactor.healthPlayer!=player,
                    "replacement retains only the previous player's confirmed death");
            } else
                check(controller->player_health(run,player,false),"goal-boundary respawn permits subsequent progression");
        }
        for(unsigned i=0;i<4;++i) frame=controller->update(run,now+=100,true,56);
        check(frame.reactor.completed[path-1],"fresh real body in native goal volume commits exact completed path");
        check(frame.checkpointSliceSet==56 && frame.checkpointSpawnSet==m::kReactorGoalSpawns[path-1],"completed path publishes exact native checkpoint spawn set");
    }
    check(frame.section==2 && frame.reactor.completed.all() && !frame.finished,"four genuine path gates start holdout without claiming raid victory");
    holdout_and_traversal(*controller,run,now,player,observeHealth);
}
int main() {
    eater_door_native_binding_tests();
    eater_door_authority_tests();
    hoop_crossing_math();
    eater_traversal_binding_tests();
    reactor_source_authority();
    eater_source_retirement_policy_tests();
    check(eater_platform_contact_native_tests()==0,"native platform contact reader fixtures");
    std::ifstream file(std::filesystem::path(__FILE__).parent_path().parent_path()/"scripts/eater_of_worlds.lua");
    std::ostringstream source;source<<file.rdbuf();
    // Keep each large snapshot fixture in a separate stack lifetime. Debug
    // retains return-value temporaries that Release can reuse or eliminate.
    shipped_route(source.str());
    health_observations(source.str());
    carry_observations(source.str());
    station_observations(source.str());
    deferred_carry_ordering();
    reactor_policy();
    narrow_native_requests();
    reactor_native_receipts(source.str(),false);
    reactor_native_receipts(source.str(),true);
    std::printf("PASS: %u Eater checks; gameplay acceptance is separate\n",checks);
}
