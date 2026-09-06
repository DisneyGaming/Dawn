#include "state/activity/omega/omega_rescue_authority.h"
#include "state/activity/omega/omega_transit_authority.h"
#include "client/hooks/bootflow/omega_reveal_source.h"
#include "client/hooks/bootflow/omega_cannon_delivery.h"
#include <mutex>
#include <algorithm>
#include <cstring>
#include <span>
#include <string_view>
#include <vector>
#include <map>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <format>
namespace omega=sunrise::state::activity::omega;
namespace omega_reveal_source=sunrise::client::hooks::bootflow::omega_reveal_source;
namespace omega_cannon_delivery=sunrise::client::hooks::bootflow::omega_cannon_delivery;
namespace state=sunrise::state;
unsigned checks{},forwarded{},receipts{},starts{};
void check(bool v,const char* what) {++checks;if(!v){std::fprintf(stderr,"FAIL %s\n",what);std::exit(1);}}
omega::mission::Snapshot fixtureSnapshot;
namespace sunrise::state::activity {
inline std::uint64_t mission_run_generation() {return 7;}
namespace omega::mission::runtime {
inline Snapshot snapshot(std::uint64_t) {return ::fixtureSnapshot;}
struct Sink {
 bool pickup(const ChargeReceipt& r) {check(r.token==::fixtureSnapshot.command.token,"pickup token");++receipts;::fixtureSnapshot.phase=Phase::carrying;return true;}
 bool drop(const ChargeReceipt& r) {check(r.token==::fixtureSnapshot.command.token,"drop token");++receipts;::fixtureSnapshot.phase=Phase::route;return true;}
 bool dunk(const ChargeReceipt& r) {check(r.token==::fixtureSnapshot.command.token,"dunk token");++receipts;::fixtureSnapshot.phase=Phase::shield;return true;}
 bool rescue_started(const Token& token,std::uint16_t slot) {
    const auto* c=rescue::command(::fixtureSnapshot,slot);check(c && c->token==token,"qualified Scene start");++starts;return true;
 }
 bool rescue_ready(const Token& token,std::uint16_t slot) {
    const auto* c=rescue::command(::fixtureSnapshot,slot);check(c && c->token==token,"qualified exact request");++receipts;return true;
}};
template<class F> bool receipt(F&& f) {Sink s;return f(s);}
}}
struct Gate {bool accepting=true;} callGate;
namespace hooking {
struct CallGate {struct Scope {Gate& gate;explicit Scope(Gate& g):gate(g){} bool accepts_side_effects() const {return gate.accepting;}};};
template<class T> T await_original(std::atomic<T>& p) {return p.load();}
}
bool enabled=true;
bool active() {return enabled;}
template<class... A> void log(const char*,A...) {}
std::map<std::uint32_t,std::vector<std::byte>> memory;
std::map<std::uint32_t,std::uint32_t> serials;
std::uint32_t fixtureGroup=0x12342001;
std::byte* resolve_handle(std::uint32_t handle) {auto it=memory.find(handle);return it==memory.end()?nullptr:it->second.data();}
bool copy_native(const void* from,void* to,std::size_t count) {
    const auto a=reinterpret_cast<std::uintptr_t>(from);
    for(const auto& [h,b]:memory) {
        (void)h;const auto start=reinterpret_cast<std::uintptr_t>(b.data());
        if(a>=start && a-start<=b.size() && count<=b.size()-(a-start)) {std::memcpy(to,from,count);return true;}
    }
    return false;
}
template<class T> T read(const std::byte* p,std::size_t at) {T v{};std::memcpy(&v,p+at,sizeof v);return v;}
template<class T> bool copy_value(const std::byte* p,T& v) {return copy_native(p,&v,sizeof v);}
bool mission_source_identity(std::byte* component,std::span<const std::byte> bytes,std::size_t reference,
    std::uint32_t metadata,omega_cannon_delivery::Reference& out) noexcept {
    if(bytes.size()<reference+16 || read<std::uint32_t>(bytes.data(),reference+4)!=metadata) return false;
    out={read<std::uint32_t>(bytes.data(),reference),read<std::int64_t>(bytes.data(),reference+8)};
    auto* base=resolve_handle(out.member);
    return base && out.offset>=0 && out.offset<=0x2000000 && base+out.offset==component;
}
template<class T> void put(std::byte* p,std::size_t at,T v) {std::memcpy(p+at,&v,sizeof v);}
std::uint32_t* __fastcall weak(const void* raw,std::uint32_t* out) noexcept {
    const auto* p=static_cast<const std::byte*>(raw);const auto handle=read<std::uint32_t>(p,4);
    const auto it=serials.find(handle);*out=it!=serials.end() && it->second==read<std::uint32_t>(p,0)?handle:UINT32_MAX;return out;
}
std::uint32_t* __fastcall group_of(const std::byte*,std::uint32_t* out) noexcept {*out=fixtureGroup;return out;}
std::uint32_t* __fastcall holder_of(std::byte*,std::uint32_t*) noexcept;
bool __fastcall component_of(const std::byte*,std::uint32_t,void*,std::uint32_t*) noexcept;
template<class F> F native(std::uintptr_t rva) {
    if(rva==0x597B10) return reinterpret_cast<F>(&holder_of);
    if(rva==0x557470) return reinterpret_cast<F>(&component_of);
    check(rva==0x352310 || rva==0x4E5C60,"only native identity readers called");
    return rva==0x352310?reinterpret_cast<F>(&weak):reinterpret_cast<F>(&group_of);
}
bool replaceDuringOriginal{};
void __fastcall original(std::byte*) noexcept {++forwarded;if(replaceDuringOriginal)fixtureGroup^=0x2000;}
std::atomic<void(__fastcall*)(std::byte*) noexcept> sceneSenseOriginal{&original};
#include "client/hooks/bootflow/omega_mission_rescue.inl"
constexpr std::uint32_t selectorHandle=0x23452002,childHandle=0x34562003;
struct Writer {
    std::vector<unsigned> values;
    bool write(std::uint64_t value,unsigned count) {for(unsigned i=0;i<count;++i)values.push_back(static_cast<unsigned>((value>>(count-1-i))&1));return true;}
};
std::vector<std::byte> load(const std::filesystem::path& path) {
    std::ifstream in(path,std::ios::binary|std::ios::ate);check(in.good(),"package asset opens");
    std::vector<std::byte> b(static_cast<std::size_t>(in.tellg()));in.seekg(0);in.read(reinterpret_cast<char*>(b.data()),static_cast<std::streamsize>(b.size()));check(in.good(),"package asset reads");return b;
}
std::byte* setup(const omega::rescue::Scene& row,const std::filesystem::path& assets) {
    memory.clear();serials.clear();fixtureSnapshot={};fixtureGroup=0x12342001;replaceDuringOriginal=false;callGate.accepting=true;enabled=true;
    fixtureSnapshot.generation=2;fixtureSnapshot.scenes[0]={{{7,2,3,4,5,6,7,0},15},2,row.slot,false,0,{}};
    memory[row.definition]=load(assets/(std::format("{:08X}.bin",row.definition)));
    auto* owner=(memory[fixtureGroup]=std::vector<std::byte>(0x500)).data();auto* component=owner+0x100;
    put(component,0,row.definition);put(component,4,0x80806266U);put(component,8,std::int64_t{0x368});
    put(component,0x180,2U);put(component,0x254,2U);put(component,0x2E8,7U);put(component,0x2EC,selectorHandle);
    put(component,0x264,1U);put(component,0x268,0x63A4F800U);serials[selectorHandle]=7;
    auto* selector=(memory[selectorHandle]=std::vector<std::byte>(0x600)).data();
    const auto folder=std::filesystem::path(__FILE__).parent_path()/"fixtures/omega_rescue";
    memory[row.selector-1]=load(folder/std::format("{:08X}.bin",row.selector-1));
    std::memcpy(selector,resolve_handle(row.selector-1)+0x90,16);
    put(selector,0x24,selectorHandle);put(selector,0x7C,std::uint8_t{2});put(selector,0x38,std::int64_t{1});
    put(selector,0x40,std::int64_t{0x90});put(selector,0x100,std::int64_t{0x100});
    auto* node=selector+0x200;put(node,0,row.selector-1);put(node,4,0x808062FEU);
    put(node,8,std::int64_t{0x400});put(node,0x10,std::int64_t{-0x210});put(node,0x20,0x808062FDU);
    put(node,0x98,std::uint8_t{1});put(node,0x1B0,9U);put(node,0x1B4,childHandle);serials[childHandle]=9;
    put(resolve_handle(row.selector-1),0x458,0x80EC0E00U);
    auto* child=(memory[childHandle]=std::vector<std::byte>(0x80)).data();
    memory[0x80EC0DFF]=load(folder/"80EC0DFF.bin");
    std::memcpy(child,resolve_handle(0x80EC0DFF)+0x90,16);
    put(child,0x24,childHandle);return component;
}
void captured_graph_case(const std::filesystem::path& assets) {
    const auto folder=std::filesystem::path(__FILE__).parent_path()/"fixtures/omega_rescue";
    auto* component=setup(*omega::rescue::scene(9),assets);
    memory[0x80EC0DCF]=load(folder/"80EC0DCF.bin");
    memory[selectorHandle]=load(folder/"live-52852-selector.bin");
    auto* selector=resolve_handle(selectorHandle);put(selector,0x24,selectorHandle);
    constexpr std::uint32_t capturedChild=0x72F9E20C;
    memory[capturedChild]=load(folder/"live-52852-hold.bin");serials[capturedChild]=0x6DBC2BE5;
    const auto initial=receipts,arrival=starts;
    mission_scene_sense(component);
    check(receipts==initial+1 && starts==arrival+1,"replay actual 41-node live graph recognizes timeline hold and entrance event");
    // Stale weak handle must invalidate hold even though the Scene remains active.
    ++serials[capturedChild];mission_scene_sense(component);
    check(receipts==initial+1,"captured child salt replacement cannot release route");
}
std::byte* image{};
void __fastcall carry_original(std::byte* p,std::uint8_t value,const void*) noexcept {++forwarded;put(p,0x470,value);}
void __fastcall use_original(std::byte* p) noexcept;
std::atomic<void(__fastcall*)(std::byte*,std::uint8_t,const void*) noexcept> arcCarryOriginal{&carry_original};
std::atomic<void(__fastcall*)(std::byte*) noexcept> arcUseOriginal{&use_original};
#include "client/hooks/bootflow/omega_mission_arc.inl"
constexpr std::uint32_t itemEntity=0x01002001,sinkEntity=0x02002002,carrier=0x10002003,sinkController=0x20002004;
constexpr std::uint32_t itemSource=0x30002005,sinkSource=0x40002006,playerEntity=0x50002007;
constexpr std::uint32_t playerRecord=0x3DFEC000;
constexpr std::uint32_t inventoryHandle=0x60002008;
bool consumeRequest=true,dropDuringUse=false;
std::uint32_t* __fastcall holder_of(std::byte*,std::uint32_t* out) noexcept {
    *out=read<std::uint32_t>(resolve_handle(0xEE000002),0xE0+0x3C);return out;
}
bool __fastcall component_of(const std::byte* world,std::uint32_t type,void* reference,std::uint32_t*) noexcept {
    const bool item=type==0x80803F6A;
    check((item || type==0x80809658) && read<std::uint32_t>(world,12)==(item?itemEntity:sinkEntity),"registered native interface requested for exact entity");
    put(static_cast<std::byte*>(reference),0x18,item?carrier:sinkController);put(static_cast<std::byte*>(reference),0x20,std::int64_t{});return true;
}
void __fastcall use_original(std::byte* p) noexcept {
    ++forwarded;
    if(dropDuringUse) mission_arc_carry(resolve_handle(carrier),0,nullptr);
    if(consumeRequest) {put(p,0x2D0,std::uint8_t{1});put(p,0x2D8,read<std::uint32_t>(p,0x2DC));}
}
void setup_arc(unsigned cycle) {
    memory.clear();serials.clear();missionArcSources={};missionArcHolding=false;missionArcUse=false;missionArcDeferredDrop=false;
    fixtureSnapshot={};fixtureSnapshot.generation=2;fixtureSnapshot.chargeEnabled=true;fixtureSnapshot.phase=omega::mission::Phase::route;
    fixtureSnapshot.command.token={{7,2,3,4,5,6,7,0},15};fixtureSnapshot.command.cycle=static_cast<std::uint8_t>(cycle);
    callGate.accepting=true;enabled=true;consumeRequest=true;dropDuringUse=false;
    image=(memory[0xEE000001]=std::vector<std::byte>(0x1F93440)).data();
    auto* world=(memory[0xEE000002]=std::vector<std::byte>(8*0xE0)).data();
    put(image,0x1F93428,world);put(image,0x1F93430,0xE0U);put(world,2*0xE0+12,sinkEntity);
    put(world,0xE0+12,itemEntity);put(world,0xE0+0x3C,playerEntity);put(world,7*0xE0+12,playerEntity);
    auto* player=(memory[playerRecord]=std::vector<std::byte>(0x1E0)).data();
    // The live failed dunk had zero at +44. It is not the record handle.
    put(player,0x44,0U);put(player,0x54,playerEntity);
    put(image,0x1F90E18,player);put(image,0x1F90E20,0x1E0U);
    for(const auto& row:omega::transit::sources) if(row.cycle==cycle && (row.role==omega::transit::Role::charge || row.role==omega::transit::Role::sink)) {
        const bool item=row.role==omega::transit::Role::charge;const auto id=item?itemSource:sinkSource;
        auto* b=(memory[id]=std::vector<std::byte>(0x500)).data();put(b,0,row.definition);put(b,4,0x80809928U);put(b,8,std::int64_t{0x4C8});
        put(b,0x24,0xDEADBEEFU);put(b,0x160,id);put(b,0x164,0x80809927U);
        put(b,0x180,3U);put(b,0x188,std::uint8_t{1});put(b,0x440,7U);put(b,0x444,item?itemEntity:sinkEntity);
        serials[item?itemEntity:sinkEntity]=7;observe_mission_arc_source(b);
    }
    auto* c=(memory[carrier]=std::vector<std::byte>(0x500)).data();put(c,0,0x80F66667U);put(c,4,0x80804221U);put(c,8,std::int64_t{0x598});put(c,0x24,carrier);put(c,0x2C,itemEntity);
    auto* sink=(memory[sinkController]=std::vector<std::byte>(0x400)).data();
    constexpr std::array<std::uint32_t,3> definitions{0x80F6666E,0x80F66671,0x80F66673};
    put(sink,0,definitions[cycle-1]);put(sink,4,0x80804FB2U);put(sink,8,std::int64_t{0x388});put(sink,0x24,sinkController);put(sink,0x2C,sinkEntity);
    put(sink,0x2DC,1U);put(sink,0x2E0,5U);put(sink,0x2E4,playerRecord);serials[playerRecord]=5;
}
void arc_cases() {
    for(unsigned cycle=1;cycle<=3;++cycle) for(unsigned mutation=0;mutation<14;++mutation) {
        setup_arc(cycle);const auto initial=receipts;
        if(mutation==1) serials[itemEntity]++;
        if(mutation==2) put(resolve_handle(itemSource),0x180,4U);
        mission_arc_carry(resolve_handle(carrier),3,nullptr);
        const bool picked=mutation!=1 && mutation!=2;
        check(receipts==initial+(picked?1:0),"pickup requires native source and serial");
        if(!picked) continue;
        mission_arc_carry(resolve_handle(carrier),3,nullptr);check(receipts==initial+1,"duplicate carried state inert");
        auto* sink=resolve_handle(sinkController);
        if(mutation==3) serials[playerRecord]++;
        if(mutation==4) put(sink,0x2D8,1U);
        if(mutation==5) consumeRequest=false;
        if(mutation==6) dropDuringUse=true;
        if(mutation==7) {consumeRequest=false;dropDuringUse=true;}
        if(mutation==8) serials[sinkEntity]++;
        if(mutation==9) put(resolve_handle(playerRecord),0x54,playerEntity^0x10000000U);
        if(mutation==10) put(image,0x1F90E18,resolve_handle(playerRecord)+8);
        if(mutation==11) put(resolve_handle(0xEE000002),7*0xE0+12,playerEntity^0x10000000U);
        if(mutation==12) {put(sink,0x2E4,playerEntity);serials[playerEntity]=5;}
        if(mutation==13) put(resolve_handle(playerRecord),0x44,0x12345678U);
        mission_arc_use(sink);
        const bool dunk=mutation==0 || mutation==6 || mutation==13;
        check(fixtureSnapshot.phase==(dunk?omega::mission::Phase::shield:mutation==7?omega::mission::Phase::route:omega::mission::Phase::carrying),"dunk requires matching consumed request; nested consumption retained");
        check(receipts==initial+1+(dunk || mutation==7?1:0),"native use produces exactly one qualified receipt");
    }
}
void captured_inventory_cases() {
    const auto folder=std::filesystem::path(__FILE__).parent_path()/"fixtures/omega_arc";
    const auto captured=load(folder/"held-item-48516.bin"),inventory=load(folder/"held-inventory-48516.bin");
    check(read<std::uint8_t>(captured.data(),0x470)==1,"user-confirmed held capture uses native state 1");
    for(unsigned cycle=1;cycle<=3;++cycle) for(unsigned mutation=0;mutation<16;++mutation) {
        setup_arc(cycle);const auto initial=receipts,forwards=forwarded;
        memory[carrier]=captured;memory[inventoryHandle]=inventory;
        auto* c=resolve_handle(carrier);auto* inv=resolve_handle(inventoryHandle);auto* world=resolve_handle(0xEE000002);
        put(c,0x24,carrier);put(c,0x2C,itemEntity);put(c,0x490,inventoryHandle);
        put(inv,0x24,inventoryHandle);put(inv,0x2C,playerEntity);
        switch(mutation) {
        case 1:put(c,0x470,std::uint8_t{0});break;
        case 2:put(c,0x470,std::uint8_t{2});break;
        case 3:put(c,0x47C,0U);break;
        case 4:put(c,0x480,std::int64_t{0});break;
        case 5:put(c,0x494,0U);break;
        case 6:put(c,0x490,inventoryHandle^0x800000U);break;
        case 7:put(c,0x498,std::int64_t{-1});break;
        case 8:put(inv,0x24,inventoryHandle^0x800000U);break;
        case 9:put(inv,0x2C,playerEntity^0x800000U);break;
        case 10:put(inv,4,0x80804057U);break;
        case 11:put(world,0xE0+0x3C,UINT32_MAX);break;
        case 12:put(world,7*0xE0+12,playerEntity^0x800000U);break;
        case 13:serials[itemEntity]++;break;
        case 14:put(resolve_handle(sinkSource),0x188,std::uint8_t{0});break;
        case 15:put(inv,0,0x80F56155U);break;
        }
        poll_mission_arc_carry();
        check(receipts==initial+(mutation==0?1:0),"captured equipped charge requires exact source, typed owner, full player and attachment");
        check(forwarded==forwards,"bounded reconciliation never calls native carry transition");
        if(mutation) continue;
        poll_mission_arc_carry();check(receipts==initial+1,"reconciled inventory pickup deduplicates");
        dropDuringUse=true;mission_arc_use(resolve_handle(sinkController));
        check(fixtureSnapshot.phase==omega::mission::Phase::shield && receipts==initial+2,"captured inventory pickup plus consumed native dunk reaches shield break");
    }
    // A callback can precede its source binding; the committed held state must
    // be recognized once the same native source association becomes available.
    setup_arc(1);memory[carrier]=captured;memory[inventoryHandle]=inventory;
    auto* c=resolve_handle(carrier);auto* inv=resolve_handle(inventoryHandle);
    put(c,0x24,carrier);put(c,0x2C,itemEntity);put(c,0x490,inventoryHandle);
    put(inv,0x24,inventoryHandle);put(inv,0x2C,playerEntity);
    const auto initial=receipts;const auto binding=missionArcSources[0][0];missionArcSources[0][0]={};
    mission_arc_carry(c,1,nullptr);check(receipts==initial,"early inventory callback waits for native source");
    missionArcSources[0][0]=binding;poll_mission_arc_carry();check(receipts==initial+1,"late source adoption recovers committed pickup");
    put(c,0x470,std::uint8_t{0});poll_mission_arc_carry();
    check(receipts==initial+2 && fixtureSnapshot.phase==omega::mission::Phase::route,"observed actual drop releases inventory receipt");
}
void captured_delivery_cases() {
    namespace delivery=sunrise::client::hooks::bootflow::omega_rescue_delivery;
    const auto folder=std::filesystem::path(__FILE__).parent_path()/"fixtures/omega_rescue";
    auto npc=load(folder/"80F4799D-360.bin"),object=load(folder/"80F4799D-360-object.bin"),body=load(folder/"80F4799D-360-authority.bin");
    fixtureSnapshot={};fixtureSnapshot.generation=2;
    fixtureSnapshot.scenes[0]={{},2,9,false,0,{}};
    const auto* row=delivery::source(npc);check(row && row->slot==0,"captured Osiris exact source");
    check(delivery::pending(*row,fixtureSnapshot,object,body),"captured unadopted Osiris request accepted");
    check(!sunrise::client::hooks::bootflow::omega_lair_delivery::adopted(npc,body),"capture reproduces missing native adoption");
    for(std::size_t i=0;i<body.size();++i) {body[i]^=std::byte{1};check(!delivery::pending(*row,fixtureSnapshot,object,body),"altered native NPC body rejected");body[i]^=std::byte{1};}
    fixtureSnapshot.scenes={};check(!delivery::pending(*row,fixtureSnapshot,object,body),"dormant cast never activated");
    fixtureSnapshot.scenes[0]={{},2,9,false,0,{}};
    for(auto offset:{0U,4U,6U,12U,24U,104U,110U}) {object[offset]^=std::byte{1};check(!delivery::pending(*row,fixtureSnapshot,object,body),"wrong object scope rejected");object[offset]^=std::byte{1};}
    auto scene=load(folder/"80F479BF-369.bin"),sceneBody=load(folder/"80F479BF-369-authority.bin");
    check(read<std::uint32_t>(scene.data(),4)==0x80806266 && read<std::uint32_t>(scene.data(),0x164)==0x80806382,"runtime class differs from source-reference metadata");
    check(delivery::scene_pending(*omega::rescue::scene(9),fixtureSnapshot,sceneBody),"captured Scene authority accepted");
    for(std::size_t i=0;i<sceneBody.size();++i) {sceneBody[i]^=std::byte{1};check(!delivery::scene_pending(*omega::rescue::scene(9),fixtureSnapshot,sceneBody),"altered Scene generation/cast/event rejected");sceneBody[i]^=std::byte{1};}
    auto core=load(folder/"80F47690-185.bin"),coreObject=load(folder/"80F47690-185-object.bin"),coreBody=load(folder/"80F47690-185-authority.bin");
    const auto& d=omega_cannon_delivery::kDevices[12];
    check(omega_cannon_delivery::source(core,d) && omega_cannon_delivery::authority(d,fixtureSnapshot,coreObject,coreBody),"first captured ring preparation uses same native delivery");
    check(!omega_cannon_delivery::adopted(core,d,coreBody),"captured ring is pending adoption");
    std::size_t count{};
    for(const auto& device:omega_cannon_delivery::kDevices) {check(device.asset!=0,"every discovery entry has a packaged definition");if(device.kind==3)++count;}
    check(count==omega::rescue::scenes.size(),"all and only cataloged Scenes discovered");
    // Decode the independent wire writer through every route lifecycle. This
    // catches disagreement between storage publication and native delivery.
    for(unsigned cycle=1;cycle<=3;++cycle) for(unsigned phase=0;phase<4;++phase) {
        fixtureSnapshot.command.cycle=static_cast<std::uint8_t>(cycle);
        fixtureSnapshot.phase=phase==3?omega::mission::Phase::shield:omega::mission::Phase::route;
        fixtureSnapshot.chargeEnabled=phase==1 || phase==2;
        fixtureSnapshot.chargePickedUp=phase>=2;fixtureSnapshot.chargeDunked=phase==3;
        for(const auto& device:omega_cannon_delivery::kDevices) if(device.transitIndex>=0) {
            Writer wire;check(omega::transit::write(wire,fixtureSnapshot,device.registry,device.kind==2?23:4,device.slot),"route wire encoded");
            std::size_t cursor{};
            auto take=[&](unsigned width) {std::uint64_t v{};for(unsigned i=0;i<width;++i)v=(v<<1)|wire.values[cursor++];return v;};
            std::vector<std::byte> nativeBody(device.bodyBytes),sync(0x70);
            put(sync.data(),0,device.registry);put(sync.data(),4,static_cast<std::uint8_t>(device.kind==2?23:4));
            put(sync.data(),6,device.slot);put(sync.data(),12,device.schema);put(sync.data(),0x68,14U);put(sync.data(),0x6E,std::uint8_t{1});
            if(device.kind==2) for(unsigned channel=0;channel<3;++channel) {
                put(nativeBody.data(),channel*8,static_cast<std::uint32_t>(take(32)));
                put(nativeBody.data(),channel*8+4,static_cast<std::uint16_t>(take(16)-0x8000));
                put(nativeBody.data(),channel*8+6,static_cast<std::uint8_t>(take(1)));
            } else {
                put(nativeBody.data(),0,static_cast<std::uint32_t>(take(32)-0x80000000ULL));
                put(nativeBody.data(),4,static_cast<std::uint32_t>(take(32)-0x80000000ULL));
                put(nativeBody.data(),8,static_cast<std::uint8_t>(take(1)));put(nativeBody.data(),9,static_cast<std::uint8_t>(take(1)));
                put(nativeBody.data(),12,static_cast<std::uint32_t>(take(32)-0x80000000ULL));
                put(nativeBody.data(),16,static_cast<std::uint32_t>(take(32)));
                put(nativeBody.data(),20,static_cast<std::uint8_t>(take(7)-1));put(nativeBody.data(),22,static_cast<std::uint16_t>(take(16)-0x8000));
                check(take(32)==0 && take(32)==0 && take(32)==0 && take(1)==0,"source has no placement override");
                const auto overrideCount=take(2);put(nativeBody.data(),0x40,static_cast<std::uint32_t>(overrideCount));
                check(overrideCount<=1,"bounded interaction override count");
                if(overrideCount) {
                    check(take(1)==1,"dynamic interaction schema present");
                    put(nativeBody.data(),0x50,static_cast<std::uint32_t>(take(32)));
                    put(nativeBody.data(),0x60,static_cast<std::uint8_t>(take(2)-1));
                    put(nativeBody.data(),0x64,static_cast<std::uint32_t>(take(32)));
                    put(nativeBody.data(),0x68,static_cast<std::uint8_t>(take(7)-1));
                    put(nativeBody.data(),0x6A,static_cast<std::uint16_t>(take(16)-0x8000));
                    put(nativeBody.data(),0x6C,static_cast<std::uint32_t>(take(32)-0x80000000ULL));
                    put(nativeBody.data(),0x70,static_cast<std::uint8_t>(take(1)));
                }
            }
            check(cursor==wire.values.size(),"independent decode consumes complete route body");
            check(omega_cannon_delivery::authority(device,fixtureSnapshot,sync,nativeBody),"native route delivery exactly matches decoded publication");
            if(device.kind!=2 && omega::transit::sources[device.transitIndex].role==omega::transit::Role::sink) {
                for(const auto offset:{0x40U,0x50U,0x60U,0x64U,0x68U,0x6AU,0x6CU,0x70U}) {
                    nativeBody[offset]^=std::byte{1};
                    check(!omega_cannon_delivery::authority(device,fixtureSnapshot,sync,nativeBody),"malformed interaction override cannot reach native apply");
                    nativeBody[offset]^=std::byte{1};
                }
            }
            sync[6]^=std::byte{1};check(!omega_cannon_delivery::authority(device,fixtureSnapshot,sync,nativeBody),"route cannot apply to adjacent slot");
        }
    }
}
void platform_sequence_cases() {
    namespace t=omega::transit;namespace m=omega::mission;
    for(unsigned cycle=1;cycle<=3;++cycle) {
        m::Snapshot s{};s.generation=2;s.command.cycle=static_cast<std::uint8_t>(cycle);
        for(const auto& row:t::sources) if(row.cycle==cycle && (row.role==t::Role::bridge || row.role==t::Role::portal)) {
            s.chargeEnabled=false;s.chargePickedUp=false;s.chargeDunked=false;s.eyePlatform=false;
            check(!t::status(s,row).active,"route geometry dormant before rescue");
            s.chargeEnabled=true;s.phase=m::Phase::route;
            check(t::status(s,row).active==(row.role==t::Role::bridge),"platform exists before pickup; contact portal stays dormant");
            check(t::generation(s,row)==(row.role==t::Role::bridge?3U:2U),"pre-pickup geometry receives activation generation");
            s.chargePickedUp=true;s.phase=m::Phase::carrying;
            check(t::status(s,row).active==(row.role==t::Role::bridge),"carrying preserves platform without enabling contact teleport");
            check(t::generation(s,row)==(row.role==t::Role::bridge?3U:2U),"pickup does not respawn platform or create a portal");
            s.phase=m::Phase::route;
            check(t::status(s,row).active==(row.role==t::Role::bridge),"drop preserves platform and keeps contact portal dormant");
            s.chargeEnabled=false;s.chargeDunked=true;s.phase=m::Phase::shield;
            check(t::status(s,row).active,"completed dunk preserves bridge and creates authored transport");
            s.eyePlatform=true;
            check(t::status(s,row).active==(row.role==t::Role::bridge),"receiving platform retires transport but preserves bridge");
            s.command.cycle=static_cast<std::uint8_t>(cycle+1);
            check(!t::status(s,row).active && t::status(s,row).retired,"previous cycle geometry retires");
            s.command.cycle=static_cast<std::uint8_t>(cycle);
        }
    }
}
int main(int argc,char** argv) {
    check(argc==2,"asset directory required");const std::filesystem::path assets=argv[1];
    fixtureSnapshot={};fixtureSnapshot.generation=2;
    for(const auto& row:omega::rescue::scenes) {
        Writer dormant;check(omega::rescue::write(dormant,fixtureSnapshot,omega::rescue::kRegistry,43,row.slot),"dormant scene encoded");
        check(dormant.values.size()==74+55*row.count,"dormant native width");
        check(dormant.values[0]==0 && std::all_of(dormant.values.begin()+1,dormant.values.begin()+32,[](auto v){return v==1;}),"dormant signed minus one");
        fixtureSnapshot.scenes[0]={{},2,row.slot,false,4,{1,2,3,4}};Writer started;
        check(omega::rescue::write(started,fixtureSnapshot,omega::rescue::kRegistry,43,row.slot),"retained events encoded");
        check(started.values.size()==74+55*row.count+128,"active native width");
        fixtureSnapshot.scenes[0].stop=true;
        for(unsigned i=0;i<row.count;++i) check(omega::rescue::requested(fixtureSnapshot,row.sources[i]),"stopped scene keeps cast authorized");
        fixtureSnapshot.scenes={};
    }
    for(const auto& row:omega::rescue::sources) {
        Writer source;check(omega::rescue::write(source,fixtureSnapshot,omega::rescue::kRegistry,1,row.slot),"dormant source encoded");check(source.values.size()==641,"native source width");
    }
    for(auto slot:{9,27,46}) for(unsigned mutation=0;mutation<18;++mutation) {
        auto* component=setup(*omega::rescue::scene(static_cast<std::uint16_t>(slot)),assets);
        const auto before=receipts,forwards=forwarded;
        switch(mutation) {
        case 1:serials[selectorHandle]++;break;case 2:serials[childHandle]++;break;
        case 3:put(component,0x254,3U);break;case 4:fixtureSnapshot.scenes[0].stop=true;break;
        case 5:put(resolve_handle(selectorHandle),0x298,std::uint8_t{0});break;
        case 6:put(resolve_handle(selectorHandle),0x3A0,1);break;
        case 7:put(resolve_handle(selectorHandle),0x210,std::int64_t{-0x200});break;
        case 8:put(component,0x264,0U);break;case 9:replaceDuringOriginal=true;break;
        case 10:put(component,0x258,std::uint8_t{1});break;
        case 11:callGate.accepting=false;break;case 12:enabled=false;break;
        case 13:put(resolve_handle(selectorHandle),0x7C,std::uint8_t{4});break;
        case 14:put(resolve_handle(selectorHandle),4,0x808063A7U);break;
        case 15:put(resolve_handle(childHandle),4,0x808063A7U);break;
        case 16:put(resolve_handle(selectorHandle),8,std::int64_t{0x90});break;
        case 17:put(resolve_handle(childHandle),8,std::int64_t{0x90});break;
        }
        mission_scene_sense(component);
        check(forwarded==forwards+1,"original forwarded exactly once");
        check(receipts==before+((mutation==0 || (mutation==8 && slot==27))?1:0),"only native qualified rescue advances");
    }
    captured_graph_case(assets);arc_cases();captured_inventory_cases();captured_delivery_cases();platform_sequence_cases();
    std::printf("%u production rescue/Arc checks passed\n",checks);
}
