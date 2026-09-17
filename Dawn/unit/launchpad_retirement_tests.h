#pragma once
#include <cstring>
#include "../src/client/hooks/bootflow/launchpad_retirement_native.h"

namespace launchpad_retirement_tests {
namespace retirement=dawn::client::hooks::bootflow::launchpad_retirement_native;
struct Fixture {
    static constexpr std::uintptr_t tables=0x100,directory=0x200,registry=0x1000,
        owner=0x20000,context=owner+0x11208,delta=0x38000,nodes=0x40000;
    std::vector<std::byte> memory=std::vector<std::byte>(0x60000);
    template<class T> void put(std::uintptr_t a,T value) {std::memcpy(memory.data()+a,&value,sizeof value);}
    template<class T> T get(std::uintptr_t a) {T value{};std::memcpy(&value,memory.data()+a,sizeof value);return value;}
    retirement::Source source() {return {tables,this,[](void* p,std::uintptr_t a,std::span<std::byte> out) noexcept {
        auto& bytes=static_cast<Fixture*>(p)->memory;
        if(a>bytes.size() || out.size()>bytes.size()-a) {return false;}
        std::memcpy(out.data(),bytes.data()+a,out.size());return true;
    }};}
    void roster(std::uintptr_t a,bool retired) {
        std::uint32_t top{};
        for(const auto& g:lp::kGroups) if(g.topLevel) {put(a+4+top++*4,g.key);}
        put(a,top);put(a+0x424,top);put(a+0x404,retired?0U:7U);put(a+0x528,4U);
        for(unsigned b=0;b<4;++b) {
            const auto block=a+0x52C+b*retirement::native::kRosterBlockBytes;std::uint32_t count{};
            put(block,b);
            for(const auto& g:lp::kGroups) if(!g.topLevel && g.bubble==b) {put(block+8+count++*4,g.key);}
            put(block+4,count);put(block+0x194,count);put(block+0x188,retired?0U:(1U<<count)-1U);
        }
    }
    explicit Fixture(bool all=false) {
        put(tables,directory);put(directory,registry);
        put(registry+8,owner);put(registry+0x30,0x20000U);
        put(registry+0x40+8,nodes);put(registry+0x40+0x30,0x88U);
        put(owner+0x10,std::uintptr_t{0x1234});put(owner+0x18,std::uint64_t{71});
        put(owner+0x20,std::uint8_t{1});put(owner+0x24,lp::kScenario);
        put(owner+0x848,0U);put(context,0U);put(context+4,lp::kScenario);
        roster(context+8,true);roster(delta,true);
        unsigned groups{},node{};
        for(const auto& g:lp::kGroups) if(g.key==retirement::kSharedPlayers || (all && g.topLevel)) {
            const auto row=owner+0x2C+16*groups++;
            put(row,g.key);put(row+4,0x2000U+node);
            const auto count=static_cast<unsigned>(g.slots.size());
            put(row+8,0x2000U+node+count-1);put(row+12,1U);
            for(unsigned i=0;i<count;++i,++node) {
                const auto address=nodes+0x88*node;put(address,g.key);
                put(address+0x74,i?0x2000U+node-1:UINT32_MAX);
                put(address+0x78,i+1<count?0x2000U+node+1:UINT32_MAX);
            }
        }
        put(owner+0x28,groups);
    }
    void unregister(std::uintptr_t list,std::uint32_t node) {
        CHECK(list==owner+0x28);CHECK(retirement::native::can_unregister(source(),list,node));
        CHECK(get<std::uint32_t>(list+8)==node);
        const auto next=get<std::uint32_t>(nodes+(node&0x1FFFU)*0x88+0x78);
        if(next==UINT32_MAX) {
            const auto count=get<std::uint32_t>(list);
            std::memmove(memory.data()+list+4,memory.data()+list+20,(count-1)*16);
            put(list,count-1);
        } else {put(list+8,next);put(nodes+(next&0x1FFFU)*0x88+0x74,UINT32_MAX);}
    }
};
inline void verify() {
    for(bool all:{false,true}) {
        Fixture f(all);const auto source=f.source();unsigned reason{};
        // No mission root remains in the one-group case, just as in the live stall.
        const auto lease=retirement::begin(source,f.context,f.delta,1,&reason);
        CHECK(lease.valid() && reason==0);CHECK(!retirement::finish(source,lease));
        CHECK(retirement::globals_only(source,lease));unsigned calls{};
        CHECK(retirement::retire_globals(source,lease,[&](auto list,auto node) {++calls;f.unregister(list,node);}));
        CHECK(calls==(all?41U:16U));CHECK(retirement::finish(source,lease));
        CHECK(retirement::begin(source,f.context,f.delta,1).valid());
        CHECK(retirement::begin(source,f.context,f.delta,2).valid());
    }
    Fixture active;active.roster(active.context+8,false);
    CHECK(!retirement::begin(active.source(),active.context,active.delta,1).valid());
    Fixture foreign;foreign.put(foreign.context+8+4,0x12345678U);
    CHECK(!retirement::begin(foreign.source(),foreign.context,foreign.delta,1).valid());
    Fixture changed;changed.put(changed.delta+0x428,std::uint8_t{1});
    CHECK(!retirement::begin(changed.source(),changed.context,changed.delta,1).valid());
    Fixture gameplay;auto lease=retirement::begin(gameplay.source(),gameplay.context,gameplay.delta,1);
    gameplay.put(gameplay.owner+0x2C,lp::kBreach);unsigned calls{};
    CHECK(!retirement::retire_globals(gameplay.source(),lease,[&](auto,auto) {++calls;}));CHECK(calls==0);
    Fixture broken;lease=retirement::begin(broken.source(),broken.context,broken.delta,1);
    broken.put(broken.nodes+0x88+0x74,UINT32_MAX);
    CHECK(!retirement::globals_only(broken.source(),lease));
    Fixture stale;lease=retirement::begin(stale.source(),stale.context,stale.delta,1);
    stale.put(stale.owner+0x18,std::uint64_t{72});CHECK(!retirement::globals_only(stale.source(),lease));
}
}
