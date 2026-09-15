#include <Windows.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <span>
#include <string_view>
#include "client/hooking/call_gate.h"
#include "client/hooks/bootflow/gateway_native_read.h"
#include "client/hooks/bootflow/native_population_pending.h"
#include "client/hooks/bootflow/native_population_streaming.h"
#include "client/hooks/bootflow/omega_enemy_lair_receipts.h"
#include "core/logging/log.h"
#include "state/activity/native_population_events.h"

unsigned checks{};
#define CHECK(x) do {++checks;if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}} while(false)
namespace sunrise::core::log {void write(Channel,Level,std::string_view) noexcept {}}
namespace sunrise::client::hooks::bootflow {
namespace nativeEvents=state::activity::native_population;
namespace pending=native_population_pending;
hooking::CallGate g_gate;
std::mutex g_pendingMutex;
std::uintptr_t g_image{};
pending::Queue<16> g_admittedActors;
using Ref=gateway_native::Ref;
template<class T>T at(const std::byte* bytes) noexcept {return gateway_native::at<T>(bytes);}
struct Read:gateway_native::Read {
    Read() noexcept {image=g_image;}
    bool resolve(Ref ref,std::uintptr_t& result) noexcept {
        if(!gateway_native::Read::resolve(ref.handle,result))return false;
        result+=ref.offset;return true;
    }
};
struct Actor {Ref source{};};
nativeEvents::Lease fixtureLease{{42,{1}},{42,123,1,{0x74337EDD,0x80F5B68E,1,1},1},15,true};
bool registered_source(Read&,const Actor&,nativeEvents::Receipt& receipt) noexcept {
    receipt=nativeEvents::capture(fixtureLease);return static_cast<bool>(receipt);
}
void finish_native_admissions() noexcept {}
#include "client/hooks/bootflow/native_population_streaming.inl"

struct Fixture {
    std::uintptr_t pool{},entityPool{},netPool{};
    unsigned binds{},deletes{},dispatches{},destroys{},consumes{};
    bool deferred{};
    LONG dispatchedConsumed{-1},dispatchedSecondConsumed{-1};
    static Fixture* active;
    template<class T>void put(std::uintptr_t address,T value) {std::memcpy(reinterpret_cast<void*>(address),&value,sizeof value);}
    template<class T>T get(std::uintptr_t address) {T value;std::memcpy(&value,reinterpret_cast<void*>(address),sizeof value);return value;}
    std::uintptr_t ref(unsigned handle)const {return pool+handle*0x1000;}
    std::uintptr_t source(unsigned handle)const {return ref(handle);}
    std::uintptr_t facet()const {return g_image+0x30B0440;}
    static void unlocked() {CHECK(g_pendingMutex.try_lock());g_pendingMutex.unlock();}
    static void __fastcall bind_original(std::uint32_t net,std::uint32_t object) noexcept {
        unlocked();++active->binds;
        if(!active->deferred)active->put(active->netPool+(net&8191)*12+4,object);
    }
    static void __fastcall delete_original(std::uint32_t net,std::uint8_t freeNet) noexcept {
        unlocked();CHECK(freeNet==0);++active->deletes;
        active->put(active->netPool+(net&8191)*12,UINT32_MAX);
        active->put(g_image+0x30B0340,std::uint32_t{0});
    }
    static void __fastcall dispatch_original(std::uint32_t* source,std::uint32_t mode,const std::byte* authority) noexcept {
        unlocked();CHECK(mode==17);CHECK(authority!=nullptr);++active->dispatches;
        active->dispatchedConsumed=active->get<LONG>(reinterpret_cast<std::uintptr_t>(source)+0x268);
        active->dispatchedSecondConsumed=active->get<LONG>(reinterpret_cast<std::uintptr_t>(source)+0x26C);
    }
    static void __fastcall destroy_original(void*) noexcept {unlocked();++active->destroys;}
    static void __fastcall consume_original(void* instance,std::uint32_t) noexcept {
        unlocked();++active->consumes;const auto source=reinterpret_cast<std::uintptr_t>(instance);
        active->put(source+0x268,active->get<LONG>(source+0x650));
        active->put(source+0x26C,active->get<LONG>(source+0x654));
    }
    explicit Fixture(std::int32_t categories=1) {
        active=this;nativeEvents::release(fixtureLease.activity);CHECK(nativeEvents::bind(fixtureLease));
        g_image=reinterpret_cast<std::uintptr_t>(VirtualAlloc(nullptr,0x3200000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
        CHECK(g_image!=0);pool=g_image+0x3100000;entityPool=pool+0x20000;netPool=pool+0x21000;
        const auto directory=pool+0x30000,registry=directory+0x1000,metadata=registry+0x1000,head=metadata+0x1000;
        put(g_image+0x2439C70,directory);put(directory,registry);put(directory+0x10,std::int32_t{64});
        put(registry+8,pool);put(registry+0x10,metadata);put(registry+0x30,std::int32_t{0x1000});
        put(metadata,head);put(metadata+8,pool);put(metadata+0x1C,std::uint32_t{0xFFC});
        put(metadata+0x20,std::uint32_t{0x1000});put(head+0x1C,std::uint16_t{32});
        for(unsigned i=0;i<16;++i)put(ref(i)+0xFFC,std::uint32_t{10});
        for(unsigned i:{1U,3U}) {
            put(source(i),Ref{2,0x8080948F,16});put(source(i)+0x48,i);
            put(source(i)+0x650,LONG{4});put(source(i)+0x268,LONG{0});
            put(source(i)+0x654,LONG{categories==2?3:0});put(source(i)+0x26C,LONG{0});
        }
        put(ref(2)+16+0xA8,categories);
        put(g_image+0x1F93428,entityPool);put(g_image+0x1F93430,std::uint32_t{0x80});
        put(entityPool+4*0x80+0x4C,std::uint32_t{5});put(ref(5)+0x10,std::uint32_t{4});
        put(ref(6)+0xC0,std::uint32_t{6});put(ref(6)+0xC8,std::uint32_t{5});
        put(g_image+0x2037D48,netPool);put(g_image+0x2037D50,std::uint32_t{12});
        put(netPool+7*12,std::uint32_t{0x100002});put(netPool+7*12+4,std::uint32_t{6});
        put(g_image+0x30B0340,std::uint32_t{1});put(facet()+1,std::int8_t{-1});
        put(facet()+4,std::uint32_t{7});put(facet()+8,std::uint32_t{0x100002});
        streaming::reset();g_admittedActors={};g_gate.accept();
        streaming::bind.store(&bind_original);streaming::deleteFacet=&delete_original;
        streaming::destroy.store(&destroy_original);
        streaming::consume.store(&consume_original);
    }
    ~Fixture() {g_gate.quiesce();nativeEvents::release(fixtureLease.activity);VirtualFree(reinterpret_cast<void*>(g_image),0,MEM_RELEASE);g_image=0;}
    void remember() {
        const auto receipt=nativeEvents::capture(fixtureLease);
        const nativeEvents::Event birth{fixtureLease,{fixtureLease.source,100,4},1,nativeEvents::Kind::admitted};
        streaming::remember({birth,101,receipt});CHECK(streaming::roots[0].receipt==receipt);
        streaming::capture_network_roots();CHECK(streaming::roots[0].network.net.handle==7);
    }
    void dispatch(unsigned handle) {
        std::array<std::byte,0xC0> authority{};
        streaming::dispatch_source(reinterpret_cast<std::uint32_t*>(source(handle)),17,authority.data(),&dispatch_original);
    }
    void destroy() {
        streaming::destroy_hook(reinterpret_cast<void*>(source(1)));
        put(ref(1)+0xFFC,std::uint32_t{11}); // allocator releases after the destructor
    }
    std::size_t drain(std::array<nativeEvents::Event,8>& events) {
        bool overflow{};const auto count=nativeEvents::drain(fixtureLease.activity,events,overflow);CHECK(!overflow);return count;
    }
};
Fixture* Fixture::active{};
void cases() {
    std::array<nativeEvents::Event,8> events{};
    {
        Fixture f;f.remember();f.put(f.source(1)+0x268,LONG{1});f.dispatch(1);
        CHECK(f.dispatches==1 && f.dispatchedConsumed==1);
        streaming::consume_hook(reinterpret_cast<void*>(f.source(1)),0);
        CHECK(f.get<LONG>(f.source(1)+0x268)==4);
        CHECK(streaming::sources[0].counters.consumed==1);
        streaming::bind_hook(7,UINT32_MAX);CHECK(f.binds==1 && f.deletes==1);
        CHECK(streaming::roots[0].closed);f.destroy();f.dispatch(3);
        CHECK(f.dispatches==2 && f.dispatchedConsumed==1 && f.get<LONG>(f.source(3)+0x650)==4);
        CHECK(f.drain(events)==1);CHECK(events[0].kind==nativeEvents::Kind::sourceRecreated);
        CHECK(events[0].previousSourceHandle==1 && events[0].sourceHandle==3);
        f.dispatch(3);CHECK(f.drain(events)==0 && f.dispatchedConsumed==1);
    }
    {
        Fixture f{2};f.put(f.source(1)+0x268,LONG{1});f.put(f.source(1)+0x26C,LONG{2});
        f.dispatch(1);CHECK(f.dispatchedConsumed==1 && f.dispatchedSecondConsumed==2);
        streaming::consume_hook(reinterpret_cast<void*>(f.source(1)),0);
        CHECK(f.get<LONG>(f.source(1)+0x268)==4 && f.get<LONG>(f.source(1)+0x26C)==3);
        f.destroy();f.dispatch(3);
        CHECK(f.dispatchedConsumed==1 && f.dispatchedSecondConsumed==2);
        CHECK(f.drain(events)==1 && events[0].kind==nativeEvents::Kind::sourceRecreated);
    }
    {
        Fixture f;f.remember();f.deferred=true;streaming::bind_hook(7,UINT32_MAX);
        CHECK(f.deletes==0 && f.get<std::uint32_t>(f.netPool+7*12+4)==6);
        f.deferred=false;streaming::bind_hook(7,UINT32_MAX);CHECK(f.deletes==1);
    }
    for(int kind=0;kind<8;++kind) {
        Fixture f;f.remember();
        if(kind==0)f.put(f.facet()+1,std::int8_t{0}); // transferred to a remote peer
        if(kind==1)f.put(f.facet()+0x50,std::uint16_t{4});
        if(kind==2)f.put(f.ref(7)+0xFFC,std::uint32_t{11}); // recycled network allocation
        if(kind==3)nativeEvents::release(fixtureLease.activity);
        if(kind==4)g_gate.quiesce();
        if(kind==5)f.put(f.facet()+8,std::uint32_t{0x200002}); // recycled facet slot
        if(kind==6)f.put(f.netPool+7*12+4,std::uint32_t{8}); // rebound to another object
        if(kind==7) {
            f.put(f.facet()+0x60,f.ref(9));f.put(f.ref(9)+4,std::uint32_t{1});
        }
        streaming::bind_hook(7,UINT32_MAX);CHECK(f.binds==1 && f.deletes==0);
    }
    {
        Fixture f;f.remember();
        // Native teardown can release the actor root and clear object identity
        // before network detachment. Retained network ownership still proves
        // which saved facet belongs to that original source-backed actor.
        f.put(f.ref(5)+0xFFC,std::uint32_t{11});
        f.put(f.ref(6)+0xC8,UINT32_MAX);f.put(f.ref(6)+0xC0,UINT32_MAX);
        f.put(f.facet()+1,std::int8_t{-2}); // observed native unload ownership transition
        streaming::roots[0].retired=true;
        streaming::bind_hook(7,UINT32_MAX);CHECK(f.binds==1 && f.deletes==1);
        CHECK(!streaming::roots[0].receipt);
    }
    {
        Fixture f;
        // An unowned replica without a witnessed local registration must stay
        // outside this policy, even if its current object resembles an actor.
        f.put(f.facet()+1,std::int8_t{-2});
        const auto receipt=nativeEvents::capture(fixtureLease);
        const nativeEvents::Event birth{fixtureLease,{fixtureLease.source,100,4},1,nativeEvents::Kind::admitted};
        streaming::remember({birth,101,receipt});streaming::capture_network_roots();
        CHECK(streaming::roots[0].network.net.handle==UINT32_MAX);
        streaming::bind_hook(7,UINT32_MAX);CHECK(f.binds==1 && f.deletes==0);
    }
    for(int kind=0;kind<6;++kind) {
        Fixture f;f.put(f.source(1)+0x268,LONG{1});f.dispatch(1);f.destroy();
        if(kind==0)f.put(f.ref(1)+0xFFC,std::uint32_t{10}); // old allocation still live
        if(kind==1)streaming::sources[0].destroyed=false;
        if(kind==2)f.put(f.source(3)+0x670,LONG{1}); // in-flight birth
        if(kind==3)f.put(f.source(3)+0x268,LONG{2}); // conflicting restored counter
        if(kind==4)nativeEvents::release(fixtureLease.activity);
        if(kind==5)g_gate.quiesce();
        f.dispatch(3);CHECK(f.dispatches==2 && f.dispatchedConsumed==(kind==3?2:0));
        CHECK(f.drain(events)==0);
    }
    {
        const auto opted=fixtureLease;fixtureLease.discardStreamedReplicas=false;
        Fixture f;f.dispatch(1);f.destroy();f.dispatch(3);
        CHECK(f.dispatches==2 && f.dispatchedConsumed==0 && f.drain(events)==0);
        fixtureLease=opted;
    }
}
}
int main() {sunrise::client::hooks::bootflow::cases();std::printf("Mercury streaming hooks: %u checks passed\n",checks);}
