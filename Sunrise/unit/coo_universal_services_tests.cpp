#include "state/activity/coo/lifecycle_service.h"
#include "state/activity/coo/object_service.h"
#include "state/activity/coo/objective_service.h"
#include "state/activity/coo/scene_orchestration.h"
#include "state/activity/coo/population_service.h"
#include "state/activity/coo/native_presentation_authority.h"
#include "state/activity/coo/mission_script.h"
#include "client/hooks/bootflow/coo_enemy_readiness.h"
#include "middleware/encoding/bit_writer.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
namespace c=sunrise::state::activity::coo;
namespace n=sunrise::client::hooks::bootflow::coo_native;
unsigned checks{};
#define CHECK(x) do { ++checks;if(!(x)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);std::exit(1); } } while(false)
struct Receipt {
    std::uint64_t run{};std::uint32_t actor{},owner{},generation{};std::uint16_t source{};std::uint32_t registry{};
    bool valid() const noexcept { return run && generation && actor!=UINT32_MAX && owner!=UINT32_MAX; }
    friend bool operator==(const Receipt&,const Receipt&)=default;
};
struct Group { std::uint16_t source;std::uint32_t registry;std::uint8_t count;bool required; };
constexpr std::array<Group,2> groups{{{4,0x111,2,true},{7,0x222,1,true}}};
void population() {
    c::PopulationService<Receipt,2,2> service;service.enable(0);service.enable(1);
    service.policy(0,{true,c::EnemyIntent::combat,0x111,10,2});service.policy(1,{true,c::EnemyIntent::idleReveal,0x222,20,3});
    Receipt normal{1,31,41,1,4,0x111},second=normal,boss{1,33,43,1,7,0x222};second.actor++;
    CHECK(service.admit(groups,normal,1,1)==c::Admission::accepted);CHECK(!service.ready(0,2));
    CHECK(service.admit(groups,second,1,1)==c::Admission::accepted);CHECK(service.admit(groups,boss,1,1)==c::Admission::accepted);
    CHECK(!service.ready(0,2));CHECK(service.missing(0,2,false).missing==c::Missing::admission);
    c::EnemyReadiness r{true,false,false,false,100,0x111,10,2};CHECK(service.observe(normal,r));CHECK(service.missing(0,2,false).missing==c::Missing::health);
    r.health=true;CHECK(service.observe(normal,r));CHECK(service.missing(0,2,false).missing==c::Missing::ai);
    r.healthHandle=UINT32_MAX;CHECK(service.observe(normal,r));CHECK(service.missing(0,2,false).missing==c::Missing::health);r.healthHandle=100;
    r.ai=true;r.tactical=true;r.tacticalRow=1;CHECK(service.observe(normal,r));CHECK(service.missing(0,2,false).missing==c::Missing::tactical);
    r.tacticalRow=2;CHECK(service.observe(normal,r));CHECK(service.observe(second,r));CHECK(service.ready(0,2));CHECK(!service.cleared(0,2));
    CHECK(service.missing(0,2,true).missing==c::Missing::death);
    CHECK(service.observe(normal,{}));service.policy(0,{true,c::EnemyIntent::combat,0x111,10,2});CHECK(service.ready(0,2));
    // Idle reveal still requires a created actor, health and AI, but no combat assignment or motion.
    r.tactical=false;CHECK(service.observe(boss,r));CHECK(service.ready(1,1));
    service.policy(1,{true,c::EnemyIntent::combat,0x222,20,3});CHECK(!service.ready(1,1));
    r.tactical=true;r.tacticalRegistry=0x222;r.tacticalSlot=20;r.tacticalRow=3;CHECK(service.observe(boss,r));CHECK(service.ready(1,1));
    auto stale=normal;stale.generation++;CHECK(!service.died(stale,1,1));CHECK(!service.observe(stale,r));
    CHECK(service.died(normal,1,1));CHECK(!service.died(normal,1,1));CHECK(!service.cleared(0,2));CHECK(service.died(second,1,1));CHECK(service.cleared(0,2));
    // A verified death can precede the readiness poll, including an early boss kill.
    c::PopulationService<Receipt,2,2> early;early.enable(1);early.policy(1,{true,c::EnemyIntent::combat,0x222,20,3});
    CHECK(early.admit(groups,boss,1,1)==c::Admission::accepted);CHECK(early.died(boss,1,1));CHECK(early.ready(1,1));CHECK(early.cleared(1,1));
    early.enable(0);early.capacity({true,80,80});CHECK(early.missing(0,2,false).missing==c::Missing::capacity);
    early.capacity({true,81,80});CHECK(!early.capacity().known);CHECK(early.missing(0,2,false).missing==c::Missing::admission);
}
void objects() {
    c::ObjectService<3> objects;c::Generation owner{1,1};
    constexpr c::ObjectBinding bindings[]{ {{1,2,4,0},{1,3,23,1},1,true},{{1,4,4,2},{1,5,23,3},1,true},{{1,6,4,4},{1,7,23,5},1,true} };
    CHECK(objects.begin(owner,bindings));
    for(std::size_t i=0;i<3;++i) {
        c::ObjectReceipt r{{1,2},bindings[i].source,100+static_cast<unsigned>(i),50,UINT32_MAX};
        CHECK(!objects.observe(i,r,true,1,1));CHECK(!objects.prepared({1,2},i));CHECK(objects.prepared(owner,i));CHECK(!objects.prepared(owner,i));
        CHECK(objects.state(i).phase==c::ObjectPhase::create && objects.state(i).generation==2);
        CHECK(objects.observe(i,r,true,1,1));CHECK(objects.state(i).phase==c::ObjectPhase::bind);CHECK(!objects.state(i).apply);
        r.controller=200+static_cast<unsigned>(i);CHECK(objects.observe(i,r,false,1,1));CHECK(objects.state(i).apply);
        CHECK(!objects.observe(i,r,true,0,1));CHECK(!objects.observe(i,r,true,1,2));CHECK(objects.observe(i,r,true,1,1));CHECK(objects.state(i).phase==c::ObjectPhase::ready);
        auto stale=r;stale.serial++;CHECK(!objects.observe(i,stale,true,1,1));stale=r;stale.controller++;CHECK(!objects.observe(i,stale,true,1,1));
    }
    c::DestructibleService<Receipt> box;const Receipt health{1,99,100,2,0,1};CHECK(box.bind(health));CHECK(!box.destroyed(health));
    constexpr c::LinkedDevice links[]{ {0,1,1,0,true},{1,1,.75F,0,true},{2,1,1,0,true} };
    box.project(objects,links);CHECK(objects.state(1).position==1);box.expose();box.project(objects,links);
    CHECK(objects.state(1).position==.75F && objects.state(1).phase==c::ObjectPhase::apply);CHECK(objects.state(0).position==1);
    auto stale=health;stale.generation++;CHECK(!box.destroyed(stale));CHECK(box.destroyed(health));CHECK(!box.destroyed(health));box.project(objects,links);
    for(unsigned i=0;i<3;++i) { CHECK(objects.state(i).position==0);CHECK(!objects.state(i).create);CHECK(objects.state(i).phase==c::ObjectPhase::retired); }
    CHECK(objects.begin({1,3},bindings));CHECK(!objects.prepared(owner,0));CHECK(objects.state(0).generation==3);
}
void scenes() {
    using S=c::SceneSignal;
    constexpr c::AuthoredSceneEvent events[]{ {90,c::signal_bit(S::greetingSubmitted)}, {91,c::signal_bit(S::approached)|c::signal_bit(S::animationReady)|c::signal_bit(S::prerollFinished)} };
    for(unsigned order=0;order<6;++order) {
        c::SceneOrchestration<Receipt> scene;Receipt owner{1,2,3,4,0,5};CHECK(scene.preload(1,4,events));CHECK(scene.bind(owner));CHECK(!scene.bind(owner));
        CHECK(!scene.signal(owner,S::conversationFinished,0));CHECK(!scene.after(S::conversationStarted,100000,22640));CHECK(scene.event_count()==0);
        CHECK(scene.signal(owner,S::greetingSubmitted,0));CHECK(scene.event_count()==1);
        constexpr S orders[][3]{{S::approached,S::animationReady,S::prerollFinished},{S::approached,S::prerollFinished,S::animationReady},{S::animationReady,S::approached,S::prerollFinished},{S::animationReady,S::prerollFinished,S::approached},{S::prerollFinished,S::animationReady,S::approached},{S::prerollFinished,S::approached,S::animationReady}};
        for(unsigned i=0;i<3;++i) { CHECK(scene.signal(owner,orders[order][i],i));CHECK(scene.event_count()==(i==2?2U:1U)); }
        CHECK(scene.signal(owner,S::conversationStarted,100000));CHECK(!scene.signal(owner,S::conversationStarted,200000));
        CHECK(!scene.after(S::conversationStarted,122639,22640));CHECK(scene.after(S::conversationStarted,122640,22640));
        CHECK(!scene.after(S::conversationStarted,99999,0));scene.update(130999,31000);CHECK(!scene.seen(S::conversationFinished));
        scene.update(131000,31000);CHECK(scene.seen(S::conversationFinished));CHECK(!scene.seen(S::sceneFinished));
        auto stale=owner;stale.generation++;CHECK(!scene.signal(stale,S::sceneFinished,131000));CHECK(scene.signal(owner,S::sceneFinished,131000));
    }
    c::EventTimeline<Receipt> zero;const Receipt owner{1,2,3,4,0,0};CHECK(zero.bind(owner));CHECK(zero.mark(owner,0,0));CHECK(zero.elapsed(0,22,22));CHECK(!zero.mark(owner,32,0));
}
void lifecycle() {
    c::LifecycleService life;CHECK(!life.begin(0));CHECK(life.begin(1));const auto old=life.owner();CHECK(life.complete(old));CHECK(life.activity_state()==6);CHECK(!life.complete(old));life.reset();CHECK(!life.finished());CHECK(!life.complete(old));CHECK(life.begin(1));CHECK(life.owner().value>old.value+1);CHECK(!life.complete(old));
    c::LifecycleService limit;CHECK(limit.begin(1,2,4));CHECK(limit.begin(2,2,4));CHECK(!limit.begin(3,2,4));CHECK(!limit.owner().valid());
    c::StallDiagnostics stalls;c::StallReport report{};c::Token token{1,1,0,0};c::StallDetail detail{c::Missing::health,{1,2,1,3},1,0,10};
    CHECK(!stalls.observe(token,detail,0,report));CHECK(!stalls.observe(token,detail,9999,report));CHECK(stalls.observe(token,detail,10000,report));CHECK(report.waitingMs==10000);
    CHECK(!stalls.observe(token,detail,10001,report));CHECK(stalls.observe(token,detail,25000,report));detail.missing=c::Missing::ai;CHECK(!stalls.observe(token,detail,25000,report));CHECK(stalls.observe(token,detail,35000,report));
    ++token.incarnation;CHECK(!stalls.observe(token,detail,35001,report));stalls.reset();CHECK(!stalls.observe(token,detail,50000,report));
}
void objectives() {
    c::ObjectiveService objective;const c::MarkerTarget first{{1,2,4,3},{}},second{{4,5,1,6},{}};
    objective.set(10,first);CHECK(objective.state().active);CHECK(objective.state().marker==first);auto rev=objective.state().revision;
    objective.set(10,first);CHECK(objective.state().revision==rev);objective.set(11,second);CHECK(objective.state().retiredEvent==10);CHECK(objective.state().marker==second);
    objective.clear_marker();CHECK(!objective.state().marker.valid());CHECK(objective.state().active);objective.marker(first);objective.clear();CHECK(!objective.state().active);CHECK(!objective.state().marker.valid());CHECK(objective.state().published);
    auto writer=sunrise::middleware::encoding::bits::Writer::measuring();CHECK(c::native_presentation::objective(writer,objective.state()));CHECK(writer.bit_count()==4802);
    for(bool active:{true,false}) {
        objective.set(12,second);if(!active) { objective.clear(); }
        std::array<std::byte,1024> bytes{};sunrise::middleware::encoding::bits::Writer encoded(bytes);
        CHECK(c::native_presentation::objective(encoded,objective.state()));
        const auto get=[&](unsigned bit,unsigned width) { unsigned value{};for(unsigned i=0;i<width;++i) { value=(value<<1)|((std::to_integer<unsigned>(bytes[(bit+i)/8])>>(7-(bit+i)%8))&1U); }return value; };
        CHECK(get(717,32)==(active?second.asset.registry:0x811C9DC5U));
        CHECK(get(749,7)==(active?second.asset.type+1U:0U));CHECK(get(756,16)==(active?second.asset.slot+32768U:32767U));
    }
}
struct Memory {
    std::map<std::uintptr_t,std::byte> bytes;std::map<std::uint32_t,std::uintptr_t> handles;
    template<class T> void put(std::uintptr_t address,T value) { const auto* b=reinterpret_cast<const std::byte*>(&value);for(std::size_t i=0;i<sizeof(T);++i) { bytes[address+i]=b[i]; } }
    bool copy(std::uintptr_t address,std::span<std::byte> out) { for(std::size_t i=0;i<out.size();++i) { const auto it=bytes.find(address+i);if(it==bytes.end()) { return false; }out[i]=it->second; }return true; }
    template<class T> bool value(std::uintptr_t address,T& out) { return copy(address,std::as_writable_bytes(std::span{&out,1})); }
    bool resolve(std::uint32_t handle,std::uintptr_t& out,std::uintptr_t* allocation=nullptr) { const auto it=handles.find(handle);if(it==handles.end()) { return false; }out=it->second;if(allocation) { *allocation=out; }return true; }
};
void native_capacity() {
    Memory m;m.put<std::uintptr_t>(0x10000,0x20000);m.put<std::uint32_t>(0x10014,80);m.put<std::uint16_t>(0x2001C,80);
    m.put<std::uintptr_t>(0x20008,0x30000);m.put<std::uintptr_t>(0x30010,0x40000);
    m.put<std::uint32_t>(0x40000,UINT32_MAX);m.put<std::uint32_t>(0x40004,UINT32_MAX);m.put<std::uint32_t>(0x40008,UINT32_MAX);
    auto cap=n::capacity(m,0x10000);CHECK(cap.known && cap.maximum==80 && cap.used==80);
    m.put<std::uint32_t>(0x40008,0);cap=n::capacity(m,0x10000);CHECK(cap.used==64);m.put<std::uint32_t>(0x10014,120);CHECK(!n::capacity(m,0x10000).known);
}

void native_enemy() {
    Memory m;constexpr std::uintptr_t image=0x100000,table=0x200000,source=0x300000,parent=0x400000,entityTable=0x500000,bundle=0x600000,metadata=0x700000,health=0x800000;
    const Receipt receipt{1,3,10,7,4,0x111};const auto actor=table+3U*0x100U;const auto row=entityTable+5U*0x100U;const auto character=bundle+0x100;
    m.put<std::uintptr_t>(image+0x1F9D7F8,table);m.put<std::uint32_t>(image+0x1F9D800,0x100);
    m.put<std::uint32_t>(actor+0x48,3);m.put<std::uint32_t>(actor+0x4C,5);m.put<std::uint32_t>(actor+0x38,10);m.put<std::int64_t>(actor+0x40,0);m.put<std::uint32_t>(actor+0x50,11);
    m.handles[10]=source;m.handles[11]=parent;m.handles[12]=bundle;m.handles[13]=metadata;m.handles[14]=character;m.handles[15]=health;
    m.handles[17]=0x900000;m.put<std::uint32_t>(source,17);m.put<std::int64_t>(source+8,0x728);
    m.put<std::uint32_t>(0x900758,0x111);m.put<std::uint8_t>(0x90075C,1);m.put<std::uint16_t>(0x90075E,4);
    m.put<std::uint32_t>(source+4,0x8080948F);m.put<std::uint32_t>(source+0x1FC,7);m.put<std::uint32_t>(source+0x244,7);
    m.put<std::uint32_t>(parent+4,0x808082EC);m.put<std::uint32_t>(parent+0x24,11);m.put<std::uint32_t>(parent+0x2C,5);m.put<std::uint32_t>(parent+0x1470,3);
    m.put<std::uint32_t>(source+0x180,0x111);m.put<std::uint8_t>(source+0x184,3);m.put<std::uint16_t>(source+0x186,10);
    m.put<std::int32_t>(source+0x234,2);m.put<std::int32_t>(source+0x600,2);m.put<std::uint32_t>(source+0x5E0,16);
    m.put<std::uintptr_t>(image+0x1F93428,entityTable);m.put<std::uint32_t>(image+0x1F93430,0x100);m.put<std::uint32_t>(row+4,0);m.put<std::uint32_t>(row+0x4C,12);
    m.put<std::uint32_t>(bundle,0);m.put<std::uint32_t>(bundle+4,13);m.put<std::uint32_t>(bundle+0x18,UINT32_MAX);
    m.put<std::uint64_t>(metadata+0x68,1);m.put<std::int64_t>(metadata+0x70,0x80);m.put<std::int32_t>(metadata+0x100+0x14,0x100);
    m.put<std::uint32_t>(character+4,0x80806832);m.put<std::uint32_t>(character+0x24,14);m.put<std::uint32_t>(character+0x2C,5);m.put<std::uint32_t>(character+0xC0,3);
    m.put<std::uint32_t>(character+0x2E8,15);m.put<std::uint32_t>(character+0x2EC,0x80804BEE);m.put<std::int64_t>(character+0x2F0,0);
    m.put<std::uint32_t>(health+4,0x80804B8A);m.put<std::uint32_t>(health+0x24,15);m.put<std::uint32_t>(health+0x2C,5);
    auto ready=n::enemy(m,image,receipt);CHECK(ready.created && ready.health && ready.ai && ready.tactical);CHECK(ready.tacticalRow==2);
    m.put<std::uint16_t>(0x90075E,5);CHECK(!n::enemy(m,image,receipt).created);m.put<std::uint16_t>(0x90075E,4);
    m.put<std::int32_t>(source+0x600,-1);ready=n::enemy(m,image,receipt);CHECK(ready.health && ready.ai && !ready.tactical);m.put<std::int32_t>(source+0x600,2);
    m.put<std::uint32_t>(health+0x2C,6);CHECK(!n::enemy(m,image,receipt).health);m.put<std::uint32_t>(health+0x2C,5);
    m.put<std::uint32_t>(parent+0x1470,4);CHECK(!n::enemy(m,image,receipt).ai);m.put<std::uint32_t>(parent+0x1470,3);
    m.put<std::uint32_t>(source+0x244,8);CHECK(!n::enemy(m,image,receipt).created);m.put<std::uint32_t>(source+0x244,7);
    m.put<std::uint32_t>(actor+0x48,4);CHECK(!n::enemy(m,image,receipt).created);
    std::uintptr_t found{};m.put<std::uint64_t>(metadata+0x68,0);m.put<std::int64_t>(metadata+0x70,INT64_MIN);
    CHECK(!n::component(m,12,5,0x80806832U,found)); // Empty list has no relative pointer to evaluate.
    m.put<std::uint64_t>(metadata+0x68,2);m.put<std::int64_t>(metadata+0x70,0x80);m.put<std::int32_t>(metadata+0x100+24+0x14,0x100);
    CHECK(!n::component(m,12,5,0x80806832U,found)); // Duplicate native component ownership is ambiguous.

}
void alternate_script() {
    namespace script=c::script;
    constexpr c::Asset module{0x100,0x200,0,0},sceneAsset{0x101,0x201,43,8},objectiveAsset{0x100,0x202,68,0};
    const script::Capability caps[]{
        {"launch","composition",{c::Operation::mechanic,module,1,c::Wait::requested}},
        {"done","composition",{c::Operation::observation,module,0,c::Wait::observed}},
        {"talk","room",{c::Operation::scene,sceneAsset,1,c::Wait::nativeReady}},
        {"timer","room",{c::Operation::eventAfter,sceneAsset,100,c::Wait::observed},1000},
        {"objective","room",{c::Operation::objective,objectiveAsset,256,c::Wait::requested}},
        {"finish","room",{c::Operation::complete,module,6,c::Wait::requested}}
    };
    const script::ModuleCapability modules[]{{"room",{module,1}}};const script::FactCapability facts[]{{"done",0}};
    const std::uint32_t objectives[]{256};const script::MarkerCapability markers[]{{"npc",{{0x101,0x204,1,4},{}}}};
    const script::Profile profile{"chamber.native.v1","otherMissions",c::Schema::otherMissions,caps,modules,facts,{0x300,{},{}},objectives,{},{},markers};
    std::ifstream input("Sunrise/unit/fixtures/mission_script_universal.json");CHECK(input.good());std::string text((std::istreambuf_iterator<char>(input)),{}),error;
    auto doc=script::MissionDocument::parse(text,profile,error);if(!doc) { std::fprintf(stderr,"%s\n",error.c_str()); }CHECK(doc);CHECK(doc->views().markers.size()==1);
    const auto reject=[&](const char* from,const char* to) { auto bad=text;const auto at=bad.find(from);CHECK(at!=std::string::npos);bad.replace(at,std::strlen(from),to);CHECK(!script::MissionDocument::parse(bad,profile,error)); };
    reject("\"argument\": 500","\"argument\": 1001");reject("\"argument\": 500","\"argument\": 0");reject("\"target\": \"npc\"","\"target\": \"foreign\"");
    struct Host:c::Services {
        c::LifecycleService life;c::ObjectiveService hud;
        bool publish(const c::Command& cmd) noexcept override {
            if(cmd.spec.operation==c::Operation::complete) { hud.clear();return cmd.spec.argument==6 && life.complete(life.owner()); }
            if(cmd.spec.operation==c::Operation::objective) { hud.set(cmd.spec.argument,{{0x101,0x204,1,4},{}}); }return true;
        }
        void cancel(const c::Command&) noexcept override {}
    } host;
    CHECK(host.life.begin(37));c::Executor executor;const auto& graph=*doc->views().role("ending");CHECK(executor.start(graph.definition,37));executor.update(host);
    c::EventTimeline<Receipt> timeline;const Receipt owner{37,2,3,1,0,0};CHECK(timeline.bind(owner));
    CHECK(!timeline.elapsed(0,100000,500));CHECK(executor.step_state(1).phase==c::StepPhase::pending);
    CHECK(timeline.mark(owner,0,1000));CHECK(executor.enqueue({executor.token(0,0),c::Milestone::nativeReady}));executor.update(host);
    const auto delay=graph.definition.steps[1].commands[0].argument;CHECK(delay==500);CHECK(!timeline.elapsed(0,1499,delay));CHECK(timeline.elapsed(0,1500,delay));
    CHECK(executor.enqueue({executor.token(1,0),c::Milestone::observed}));executor.update(host);CHECK(host.life.activity_state()==6);CHECK(!host.hud.state().active);CHECK(!host.hud.state().marker.valid());
    executor.update(host);CHECK(executor.diagnostics().phase==c::Phase::complete);const auto stale=executor.token(1,0);executor.cancel(host);host.life.reset();CHECK(host.life.begin(37));CHECK(executor.start(graph.definition,37));executor.update(host);CHECK(!executor.enqueue({stale,c::Milestone::observed}));
}
int main() { population();objects();scenes();lifecycle();objectives();native_capacity();native_enemy();alternate_script();std::printf("Universal services: %u checks passed\n",checks); }
